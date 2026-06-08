# Application.mk - Android NDK Build Configuration

# Target all major ABIs
APP_ABI := arm64-v8a armeabi-v7a x86 x86_64

# Minimum Android API (Android 5.0 = API 21)
APP_PLATFORM := android-21

# Use libc++ shared
APP_STL := c++_static

# Enable NDK unified headers
APP_UNIFIED_HEADERS := true

# Enable thin LTO
APP_THIN_ARCHIVE := true
