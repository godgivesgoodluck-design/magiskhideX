// MagiskHideX - main.cpp
// Zygisk Module: Root hiding compatible with Magisk Alpha + NeoZygisk
// Build with Android NDK using CMakeLists.txt
//
// Architecture:
//   - Reads Magisk DenyList via companion process (root context)
//   - In preAppSpecialize: checks if target app is on list
//   - If on list: forces unmount (FORCE_DENYLIST_UNMOUNT) to strip all
//     Magisk/Zygisk traces from the app's process
//   - Whitelist mode: only apps granted root can see root
//   - No dependency on DenyList enforcement being ON
//
// Compatible with:
//   - Magisk Alpha >= 26402
//   - NeoZygisk (JingMatrix)
//   - ZygiskNext (Dr-TSNG)

#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <android/log.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#include "zygisk.hpp"

#define LOG_TAG "MagiskHideX"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#define DATADIR       "/data/adb/magiskhidex"
#define WHITELIST_F   DATADIR "/whitelist"
#define UMOUNT_LIST_F DATADIR "/umount_list"

// ──────────────────────────────────────────────────────────────────────────────
// Utility: read a line-delimited list from a file (companion side, has root)
// ──────────────────────────────────────────────────────────────────────────────
static std::vector<std::string> readLines(const char *path) {
    std::vector<std::string> result;
    std::ifstream f(path);
    if (!f.is_open()) return result;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line[0] != '#') {
            result.push_back(line);
        }
    }
    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// Companion: runs in root context (magiskd), handles IPC from Zygisk side
//
// Protocol (simple binary over socket):
//   Client sends: uint32_t (package name length) + package name bytes
//   Server replies: uint8_t (1 = on denylist / 0 = not)
// ──────────────────────────────────────────────────────────────────────────────
static void companionHandler(int client_fd) {
    // Read package name from zygisk side
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

    LOGD("Companion: checking package [%s]", pkgName.c_str());

    bool whitelist_mode = (access(WHITELIST_F, F_OK) == 0);
    uint8_t should_hide = 0;

    if (whitelist_mode) {
        // In whitelist mode: hide everything by default.
        // Only apps that have been explicitly granted root (in Magisk) are NOT hidden.
        // We detect this by checking if Magisk's root database contains the package.
        // Simple heuristic: run 'magisk --list-allow' equivalent.
        // For now, flag all apps as needing hiding in whitelist mode.
        // Real implementation should query /data/adb/magisk.db or use magisk binary.
        should_hide = 1;
        LOGD("Whitelist mode: hiding [%s] by default", pkgName.c_str());
    } else {
        // Denylist mode: read Magisk's denylist via CLI
        // magisk --denylist ls outputs "package|process" per line
        FILE *pipe = popen("magisk --denylist ls 2>/dev/null", "r");
        if (pipe) {
            char buf[256];
            while (fgets(buf, sizeof(buf), pipe)) {
                std::string entry(buf);
                // strip trailing newline
                if (!entry.empty() && entry.back() == '\n') entry.pop_back();
                // format: "com.example.app|com.example.app"
                size_t pipe_pos = entry.find('|');
                std::string entry_pkg = (pipe_pos != std::string::npos)
                                        ? entry.substr(0, pipe_pos)
                                        : entry;
                if (entry_pkg == pkgName) {
                    should_hide = 1;
                    break;
                }
            }
            pclose(pipe);
        }

        // Also check custom umount_list file
        if (!should_hide) {
            auto custom = readLines(UMOUNT_LIST_F);
            for (auto &line : custom) {
                if (line == pkgName) {
                    should_hide = 1;
                    break;
                }
            }
        }
    }

    LOGI("Companion: [%s] -> hide=%d (whitelist=%d)", pkgName.c_str(), should_hide, whitelist_mode);
    write(client_fd, &should_hide, 1);
}

// ──────────────────────────────────────────────────────────────────────────────
// Main Zygisk Module Class
// ──────────────────────────────────────────────────────────────────────────────
class MagiskHideX : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    // Called BEFORE app process specialization
    // This is where we decide whether to hide root from the process
    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        should_hide = false;

        // Get package name from args
        if (args->nice_name == nullptr) {
            // No package name — unload ourselves to be safe
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        const char *raw_pkg = env->GetStringUTFChars(args->nice_name, nullptr);
        if (raw_pkg == nullptr) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::string package_name(raw_pkg);
        env->ReleaseStringUTFChars(args->nice_name, raw_pkg);

        // Strip process suffix (e.g. "com.example.app:service" -> "com.example.app")
        size_t colon = package_name.find(':');
        if (colon != std::string::npos) {
            package_name = package_name.substr(0, colon);
        }

        LOGD("preAppSpecialize: package=[%s]", package_name.c_str());

        // Ask companion (root context) whether this app should be hidden
        int companion_fd = api->connectCompanion();
        if (companion_fd < 0) {
            LOGE("Failed to connect to companion");
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        uint32_t len = (uint32_t)package_name.size();
        write(companion_fd, &len, sizeof(len));
        write(companion_fd, package_name.c_str(), len);

        uint8_t response = 0;
        read(companion_fd, &response, 1);
        close(companion_fd);

        should_hide = (response == 1);

        if (should_hide) {
            LOGI("Hiding root from: [%s]", package_name.c_str());
            // Force full unmount: strips Magisk, Zygisk, and all module files
            // from this app's mount namespace. This is the key hiding mechanism.
            api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
        } else {
            // Not on denylist — unload ourselves to leave no trace
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

    // Called AFTER app process specialization
    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!should_hide) return;
        // At this point unmount has already happened.
        // Nothing else to do — Magisk/Zygisk traces are gone.
        LOGD("postAppSpecialize: hiding complete");
    }

    // Called BEFORE system_server specialization
    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        // Don't touch system_server — unload ourselves
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

    void postServerSpecialize(const zygisk::ServerSpecializeArgs *args) override {}

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool should_hide = false;
};

// ──────────────────────────────────────────────────────────────────────────────
// Register module entry point with Zygisk
// ──────────────────────────────────────────────────────────────────────────────
REGISTER_ZYGISK_MODULE(MagiskHideX)

// Register companion handler (runs in magiskd / root context)
REGISTER_ZYGISK_COMPANION(companionHandler)
