#!/usr/bin/env bash
# MagiskHideX - Build Script
# Requirements:
#   - Android NDK r25+ (set NDK env variable or update NDK_PATH below)
#   - CMake 3.22+
#   - zip
#
# Usage:
#   chmod +x build.sh && ./build.sh
#   Output: MagiskHideX-v1.0.0.zip

set -e

# ─── Configuration ──────────────────────────────────────────────────
MODULE_ID="MagiskHideX"
MODULE_VERSION="v1.0.0"
OUTPUT_ZIP="${MODULE_ID}-${MODULE_VERSION}.zip"

# Android NDK path — edit this or set NDK env variable
NDK_PATH="${NDK:-$ANDROID_NDK_HOME}"
if [ -z "$NDK_PATH" ]; then
    # Try common locations
    for try_path in \
        "$HOME/Android/Sdk/ndk/latest" \
        "$HOME/Library/Android/sdk/ndk" \
        "/opt/android-ndk" \
        "/usr/local/lib/android/sdk/ndk"; do
        if [ -d "$try_path" ]; then
            NDK_PATH="$try_path"
            break
        fi
    done
fi

if [ ! -d "$NDK_PATH" ]; then
    echo "ERROR: Android NDK not found."
    echo "Please set NDK=/path/to/ndk or ANDROID_NDK_HOME=/path/to/ndk"
    exit 1
fi

echo "Using NDK: $NDK_PATH"

# ─── Download Zygisk API Header if missing ──────────────────────────
ZYGISK_HPP="jni/zygisk.hpp"
if [ ! -f "$ZYGISK_HPP" ]; then
    echo "Downloading zygisk.hpp..."
    curl -L -o "$ZYGISK_HPP" \
        "https://raw.githubusercontent.com/topjohnwu/zygisk-module-sample/master/module/jni/zygisk.hpp"
fi

# ─── Build per ABI ──────────────────────────────────────────────────
ABIS="arm64-v8a armeabi-v7a x86 x86_64"
BUILD_DIR="build"

mkdir -p "$BUILD_DIR"
mkdir -p "zygisk"

for ABI in $ABIS; do
    echo ""
    echo "══ Building for ABI: $ABI ══"

    ABI_BUILD_DIR="$BUILD_DIR/$ABI"
    mkdir -p "$ABI_BUILD_DIR"

    cmake \
        -S jni \
        -B "$ABI_BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$NDK_PATH/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM="android-21" \
        -DANDROID_STL="c++_static" \
        -DCMAKE_BUILD_TYPE=Release \
        -G "Ninja" \
        2>&1 | grep -E "(error|warning|CMake)" || true

    cmake --build "$ABI_BUILD_DIR" --target magiskhidex

    # Copy .so to zygisk folder with correct name
    SO_SRC=$(find "$ABI_BUILD_DIR" -name "libmagiskhidex.so" | head -1)
    if [ -z "$SO_SRC" ]; then
        echo "ERROR: Build failed for $ABI — .so not found"
        exit 1
    fi

    cp "$SO_SRC" "zygisk/${ABI}.so"
    echo "  -> zygisk/${ABI}.so ($(du -h "zygisk/${ABI}.so" | cut -f1))"
done

echo ""
echo "══ Packaging module ZIP ══"

# ─── Create ZIP ─────────────────────────────────────────────────────
# Remove old zip
rm -f "$OUTPUT_ZIP"

zip -r "$OUTPUT_ZIP" \
    module.prop \
    customize.sh \
    post-fs-data.sh \
    service.sh \
    action.sh \
    zygisk/ \
    META-INF/

echo ""
echo "✓ Done! Module package: $OUTPUT_ZIP"
echo "  Install via Magisk Manager → Modules → Install from storage"
ls -lh "$OUTPUT_ZIP"
