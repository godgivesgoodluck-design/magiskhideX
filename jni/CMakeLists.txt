cmake_minimum_required(VERSION 3.22.1)
project(MagiskHideX)

# ─── C++ Standard ───────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Compiler Flags ─────────────────────────────────────────────────
# -fvisibility=hidden : don't export unnecessary symbols (reduces detection surface)
# -fno-exceptions     : smaller binary, no C++ exception overhead
# -fno-rtti           : smaller binary, no RTTI overhead
# -nostartfiles       : avoid inclusion of atexit/init_array symbols (detection avoidance)
# -Oz                 : optimize for size (less identifiable)
# -flto               : link-time optimization
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -fvisibility=hidden \
    -fno-exceptions \
    -fno-rtti \
    -Oz \
    -flto \
    -ffunction-sections \
    -fdata-sections \
    -Wno-unused-function \
")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} \
    -Wl,--gc-sections \
    -Wl,--exclude-libs,ALL \
    -flto \
    -nostartfiles \
")

# ─── Zygisk API Header ──────────────────────────────────────────────
# Download from: https://github.com/topjohnwu/zygisk-module-sample/blob/master/module/jni/zygisk.hpp
include_directories(${CMAKE_SOURCE_DIR}/../jni)

# ─── Build Shared Library ───────────────────────────────────────────
add_library(
    magiskhidex        # Library name
    SHARED
    main.cpp
)

# ─── Link System Libraries ──────────────────────────────────────────
target_link_libraries(
    magiskhidex
    log         # Android logging (android/log.h)
    dl          # dlopen/dlsym for dynamic linking hooks
)

# ─── Strip Debug Symbols ────────────────────────────────────────────
add_custom_command(TARGET magiskhidex POST_BUILD
    COMMAND ${CMAKE_STRIP} --strip-all $<TARGET_FILE:magiskhidex>
    COMMENT "Stripping debug symbols from magiskhidex.so"
)
