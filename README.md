# MagiskHideX

**Root hiding Zygisk module** — kompatibel dengan Magisk Alpha terbaru dan NeoZygisk.
Terinspirasi dari Shamiko, dibuat ulang dengan arsitektur yang lebih bersih dan kompatibel.

---

## Fitur

- Menyembunyikan root dari app yang ada di Magisk DenyList
- **Whitelist mode**: hanya app yang sudah diberi root yang bisa mengakses root
- Tidak perlu "Enforce DenyList" diaktifkan di Magisk
- Kompatibel dengan **NeoZygisk** (JingMatrix) dan **ZygiskNext** (Dr-TSNG)
- Kompatibel dengan **Magisk Alpha** >= 26402
- Custom `umount_list` untuk tambahan app tanpa DenyList UI
- Log otomatis di `/data/adb/magiskhidex/magiskhidex.log`

---

## Prasyarat

| Komponen | Versi Minimum |
|---|---|
| Magisk (official/alpha) | 26402 |
| Magisk Alpha | 26402 |
| NeoZygisk | versi terbaru |
| Android | 8.0 (API 26) |

**Pilih salah satu:**
- Magisk Alpha + Zygisk built-in (aktifkan Zygisk di settings) **ATAU**
- Magisk Alpha + NeoZygisk (matikan Zygisk built-in, install NeoZygisk sebagai module)

---

## Cara Install

### Dengan Magisk Alpha + Zygisk Built-in
1. Buka Magisk → Settings → **Aktifkan Zygisk**
2. Settings → **Matikan Enforce DenyList**
3. Install `MagiskHideX.zip` via Magisk → Modules
4. Reboot
5. Tambahkan app target ke **DenyList** di Magisk

### Dengan Magisk Alpha + NeoZygisk
1. Buka Magisk → Settings → **Matikan Zygisk** (built-in)
2. Install **NeoZygisk** sebagai module
3. Di NeoZygisk settings → **Matikan Enforce DenyList**
4. Install `MagiskHideX.zip` via Magisk → Modules
5. Reboot
6. Tambahkan app target ke **DenyList** di Magisk (bisa dikonfig meskipun Zygisk off)

---

## Konfigurasi

### DenyList Mode (Default)
App yang ada di Magisk DenyList akan disembunyikan root-nya secara otomatis.

```
# Tidak perlu konfigurasi tambahan
# Cukup tambahkan app ke DenyList di Magisk
```

### Whitelist Mode
Semua app disembunyikan kecuali yang sudah diberikan izin root di Magisk.

```sh
# Aktifkan whitelist mode (langsung aktif, tidak perlu reboot):
touch /data/adb/magiskhidex/whitelist

# Nonaktifkan whitelist mode:
rm /data/adb/magiskhidex/whitelist
```

Atau gunakan tombol **Action** di Magisk Manager (Magisk 28+) untuk toggle.

### Custom Umount List
Tambahkan app ke list tambahan di luar DenyList UI:

```sh
# Edit file:
echo "com.example.app" >> /data/adb/magiskhidex/umount_list
echo "com.another.app" >> /data/adb/magiskhidex/umount_list
```

Format: satu package name per baris. Baris diawali `#` diabaikan (komentar).

---

## Cara Build dari Source

### Requirements
- Android NDK r25c atau lebih baru
- CMake 3.22+
- Ninja build system
- `zip`

### Steps

```sh
# Clone / download source
cd MagiskHideX/

# Download zygisk.hpp (Zygisk API header)
curl -L -o jni/zygisk.hpp \
  https://raw.githubusercontent.com/topjohnwu/zygisk-module-sample/master/module/jni/zygisk.hpp

# Set NDK path
export NDK=/path/to/android-ndk

# Build
chmod +x build.sh
./build.sh

# Output: MagiskHideX-v1.0.0.zip
```

---

## Struktur Module

```
MagiskHideX/
├── module.prop              # Metadata module
├── customize.sh             # Script installer
├── post-fs-data.sh          # Early boot hook
├── service.sh               # Background service
├── action.sh                # Magisk action button (toggle whitelist)
├── META-INF/
│   └── com/google/android/
│       ├── update-binary    # Magisk installer bootstrap
│       └── updater-script   # #MAGISK marker
├── zygisk/
│   ├── arm64-v8a.so         # Native library (ARM64)
│   ├── armeabi-v7a.so       # Native library (ARM32)
│   ├── x86_64.so            # Native library (x86_64)
│   └── x86.so               # Native library (x86)
└── jni/
    ├── main.cpp             # Zygisk module source
    ├── zygisk.hpp           # Zygisk API header (download from topjohnwu)
    ├── CMakeLists.txt       # CMake build file
    └── Application.mk       # NDK build config
```

---

## Perbedaan dengan Shamiko

| Fitur | Shamiko | MagiskHideX |
|---|---|---|
| Open Source | ❌ (closed) | ✅ |
| Magisk Alpha compat | ✅ | ✅ |
| NeoZygisk compat | ✅ | ✅ |
| Custom umount_list | ❌ | ✅ |
| Action button toggle | ❌ | ✅ |
| Logging | minimal | ✅ `/data/adb/magiskhidex/` |

---

## Arsitektur Teknis

```
[App Process]           [magiskd / root context]
      |                          |
  preAppSpecialize()             |
      |---(IPC: package name)--->|
      |                 companionHandler()
      |                   reads DenyList
      |                   checks whitelist
      |<--(response: hide?=1/0)--|
      |
  if hide=1:
    setOption(FORCE_DENYLIST_UNMOUNT)
    → Magisk/Zygisk strips all traces
      from this app's mount namespace
  else:
    setOption(DLCLOSE_MODULE_LIBRARY)
    → Module unloads, no trace left
```

---

## Troubleshooting

**App masih detect root?**
- Pastikan app sudah ditambahkan ke DenyList
- Pastikan Enforce DenyList **dimatikan**
- Cek log: `cat /data/adb/magiskhidex/magiskhidex.log`
- Gunakan tambahan module seperti PlayIntegrityFix untuk bypass integrity check

**Module tidak aktif?**
- Pastikan Zygisk aktif (built-in atau NeoZygisk)
- Cek: `adb shell su -c "magisk --zygisk"` (harus output aktif)

**Konflik dengan module lain?**
- Jangan aktifkan Magisk built-in Zygisk bersamaan dengan NeoZygisk
- Matikan Shamiko jika terinstall (konflik denylist handling)

---

## License

MIT License — Bebas dimodifikasi dan didistribusikan.
