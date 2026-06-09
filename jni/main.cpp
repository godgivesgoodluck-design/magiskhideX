// MagiskHideX - main.cpp
// Zygisk Module: Root hiding compatible with Magisk Alpha + NeoZygisk
//
// Handles:
//   - SU Binary detection        (hide su from /proc/[pid]/maps, fd scans)
//   - RW Paths detection         (unmount Magisk overlay mounts)
//   - Root via native check      (hide root paths from filesystem access)
//   - Magisk specific checks     (hide Magisk files/dirs/props)
//   - Zygisk traces              (FORCE_DENYLIST_UNMOUNT)
//   - DenyList via companion IPC (no need to enable Enforce DenyList)
//
// Compatible with:
//   - Magisk Alpha >= 26402
//   - NeoZygisk (JingMatrix)
//   - ZygiskNext (Dr-TSNG)

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <android/log.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#include "zygisk.hpp"

#define LOG_TAG "MagiskHideX-RF"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#define DATADIR       "/data/adb/magiskhidex"
#define WHITELIST_F   DATADIR "/whitelist"
#define UMOUNT_LIST_F DATADIR "/umount_list"

// ─── Paths to hide from app's view ───────────────────────────────────────────
// These are Magisk-specific paths that root detection apps scan for
static const char *MAGISK_PATHS[] = {
    "/sbin/.magisk",
    "/sbin/.core",
    "/data/adb/magisk",
    "/data/adb/modules",
    "/data/adb/post-fs-data.d",
    "/data/adb/service.d",
    "/data/adb/magisk.db",
    "/data/adb/magisk.img",
    "/cache/magisk.log",
    "/cache/.disable_magisk",
    "/dev/magisk",
    "/dev/.magisk",
    nullptr
};

// SU binary paths that detection apps check
static const char *SU_PATHS[] = {
    "/data/adb/magisk/magisk",
    "/data/adb/magisk/magisk32",
    "/data/adb/magisk/magisk64",
    "/data/adb/magisk/magiskinit",
    "/data/adb/magisk/magiskpolicy",
    "/sbin/magisk",
    "/sbin/su",
    "/system/bin/su",
    "/system/xbin/su",
    "/system/sbin/su",
    "/system/su",
    "/system/bin/.ext/.su",
    "/system/usr/we-need-root/su",
    "/su/bin/su",
    nullptr
};

// Magisk props to hide
static const char *MAGISK_PROPS[] = {
    "ro.magisk.hide",
    "ro.boot.magisk",
    nullptr
};

// ─── Utility ─────────────────────────────────────────────────────────────────
static std::vector<std::string> readLines(const char *path) {
    std::vector<std::string> result;
    std::ifstream f(path);
    if (!f.is_open()) return result;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line[0] != '#')
            result.push_back(line);
    }
    return result;
}

// ─── Hide: unmount Magisk overlay mounts from current process ─────────────────
// Reads /proc/self/mountinfo and unmounts anything Magisk-related
static void unmountMagiskMounts() {
    std::ifstream mi("/proc/self/mountinfo");
    if (!mi.is_open()) return;

    std::vector<std::pair<int, std::string>> to_unmount;
    std::string line;

    while (std::getline(mi, line)) {
        // mountinfo format: mountid parentid major:minor root mountpoint options...
        std::istringstream ss(line);
        int mount_id, parent_id;
        std::string major_minor, root, mountpoint;
        ss >> mount_id >> parent_id >> major_minor >> root >> mountpoint;

        // Check if this mount is Magisk-related
        bool is_magisk = false;

        // Check mountpoint against known Magisk paths
        for (int i = 0; MAGISK_PATHS[i]; i++) {
            if (mountpoint.find(MAGISK_PATHS[i]) == 0 ||
                mountpoint == MAGISK_PATHS[i]) {
                is_magisk = true;
                break;
            }
        }

        // Check root path (bind mounts from Magisk tmpfs)
        if (!is_magisk && (
            root.find("/magisk") != std::string::npos ||
            root.find("/.magisk") != std::string::npos ||
            root.find("/magisktmp") != std::string::npos)) {
            is_magisk = true;
        }

        if (is_magisk) {
            to_unmount.push_back({mount_id, mountpoint});
            LOGD("Will unmount: %s (id=%d)", mountpoint.c_str(), mount_id);
        }
    }
    mi.close();

    // Unmount in reverse order (children before parents)
    for (auto it = to_unmount.rbegin(); it != to_unmount.rend(); ++it) {
        if (umount2(it->second.c_str(), MNT_DETACH) == 0) {
            LOGD("Unmounted: %s", it->second.c_str());
        }
    }
}

// ─── Hide: remove Magisk entries from /proc/self/maps ─────────────────────────
// We can't modify /proc/maps directly, but we can ensure the .so files
// are unmapped. This is handled by FORCE_DENYLIST_UNMOUNT primarily.
// Additional: hide through /proc/self/fd by closing Magisk-related fds
static void closeMagiskFds() {
    DIR *fd_dir = opendir("/proc/self/fd");
    if (!fd_dir) return;

    struct dirent *entry;
    while ((entry = readdir(fd_dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;

        int fd = atoi(entry->d_name);
        if (fd <= 2) continue; // keep stdin/stdout/stderr

        char link_path[64], target[512];
        snprintf(link_path, sizeof(link_path), "/proc/self/fd/%s", entry->d_name);
        ssize_t len = readlink(link_path, target, sizeof(target) - 1);
        if (len <= 0) continue;
        target[len] = '\0';

        // Close fds pointing to Magisk paths
        for (int i = 0; MAGISK_PATHS[i]; i++) {
            if (strncmp(target, MAGISK_PATHS[i], strlen(MAGISK_PATHS[i])) == 0) {
                close(fd);
                LOGD("Closed Magisk fd %d -> %s", fd, target);
                break;
            }
        }

        // Close fds pointing to su binaries
        for (int i = 0; SU_PATHS[i]; i++) {
            if (strcmp(target, SU_PATHS[i]) == 0) {
                close(fd);
                LOGD("Closed su fd %d -> %s", fd, target);
                break;
            }
        }
    }
    closedir(fd_dir);
}

// ─── Hide: spoof system properties ───────────────────────────────────────────
// Use __system_property_find + override to hide Magisk props
// We hook by re-setting to empty if found
static void hideMagiskProps() {
    // Use resetprop-style: we can't easily hook __system_property_get here
    // but we can use the linker to find and null out the prop
    // For now: use __system_property_set to clear known Magisk props
    // This requires root which we have in preAppSpecialize context? No.
    // Props are read-only in app context.
    // Better approach: done at companion level (service.sh uses resetprop)
    // Nothing to do here at app process level for props.
    (void)MAGISK_PROPS; // suppress unused warning
}

// ─── Hide: clean /proc/self/maps of Magisk entries ───────────────────────────
// The real hiding of maps is done via FORCE_DENYLIST_UNMOUNT + unmountMagiskMounts
// Additional: ensure Zygisk .so is unloaded
static void hideFromMaps() {
    // After FORCE_DENYLIST_UNMOUNT, Magisk's own files should be unmapped.
    // We do an additional manual unmount pass for anything left.
    unmountMagiskMounts();
}

// ─── Companion Handler (root context) ─────────────────────────────────────────
static void companionHandler(int client_fd) {
    uint32_t len = 0;
    if (read(client_fd, &len, sizeof(len)) != sizeof(len) || len == 0 || len > 512) {
        uint8_t resp = 0;
        write(client_fd, &resp, 1);
        return;
    }

    std::string pkgName(len, '\0');
    if (read(client_fd, &pkgName[0], len) != (ssize_t)len) {
        uint8_t resp = 0;
        write(client_fd, &resp, 1);
        return;
    }

    LOGD("Companion: checking [%s]", pkgName.c_str());

    bool whitelist_mode = (access(WHITELIST_F, F_OK) == 0);
    uint8_t should_hide = 0;

    if (whitelist_mode) {
        should_hide = 1;
    } else {
        // Check Magisk DenyList
        FILE *pipe = popen("magisk --denylist ls 2>/dev/null", "r");
        if (pipe) {
            char buf[256];
            while (fgets(buf, sizeof(buf), pipe)) {
                std::string entry(buf);
                if (!entry.empty() && entry.back() == '\n') entry.pop_back();
                size_t pipe_pos = entry.find('|');
                std::string entry_pkg = (pipe_pos != std::string::npos)
                                        ? entry.substr(0, pipe_pos) : entry;
                if (entry_pkg == pkgName) {
                    should_hide = 1;
                    break;
                }
            }
            pclose(pipe);
        }

        // Check custom umount_list
        if (!should_hide) {
            for (auto &line : readLines(UMOUNT_LIST_F)) {
                if (line == pkgName) { should_hide = 1; break; }
            }
        }
    }

    LOGI("Companion: [%s] -> hide=%d", pkgName.c_str(), should_hide);
    write(client_fd, &should_hide, 1);
}

// ─── Main Module Class ────────────────────────────────────────────────────────
class MagiskHideX : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        should_hide = false;

        if (!args->nice_name) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        const char *raw = env->GetStringUTFChars(args->nice_name, nullptr);
        if (!raw) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::string pkg(raw);
        env->ReleaseStringUTFChars(args->nice_name, raw);

        // Strip :process suffix
        size_t colon = pkg.find(':');
        if (colon != std::string::npos) pkg = pkg.substr(0, colon);

        // Ask companion
        int fd = api->connectCompanion();
        if (fd < 0) {
            LOGE("Companion connect failed");
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        uint32_t len = (uint32_t)pkg.size();
        write(fd, &len, sizeof(len));
        write(fd, pkg.c_str(), len);

        uint8_t resp = 0;
        read(fd, &resp, 1);
        close(fd);

        should_hide = (resp == 1);

        if (should_hide) {
            LOGI("Hiding root from: [%s]", pkg.c_str());
            // Step 1: Force Magisk to unmount all its bind mounts from this process
            // This handles: RW Paths, Magisk files, module overlays
            api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
        } else {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!should_hide) return;

        // Step 2: Additional cleanup AFTER process specialization
        // At this point the app process is fully set up but hasn't run yet

        // Root-friendly patch: keep Magisk daemon communication intact
        // closeMagiskFds();

        // Unmount any remaining Magisk mounts
        hideFromMaps();

        // Unload ourselves — leave no trace of Zygisk module in this process
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);

        LOGD("postAppSpecialize: hiding complete, unloading");
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

    void postServerSpecialize(const zygisk::ServerSpecializeArgs *args) override {}

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool should_hide = false;
};

REGISTER_ZYGISK_MODULE(MagiskHideX)
REGISTER_ZYGISK_COMPANION(companionHandler)
