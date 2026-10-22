#!/usr/bin/env bash
# The OUYA's GX renderer (port/ouya, stage O2 of port/docs/ouya-machbarkeit.md) on the PC
# (Linux, WSL, the cloud; Mesa's EGL and GLES, port/tools/whole_setup_linux.sh installs them):
#   1. build (port/ouya/app/src/main/cpp/CMakeLists.txt)
#   2. gx_gl2_check: synthetic scenes drawn by the GLES 2 renderer (compact vertices, Mesa forced
#      to GLES 2.0) and by the main app's GLES 3 renderer; the pictures compared
#   3. with the NDK r23c (/opt/android-ndk-r23c, whole_setup_linux.sh --ouya): the renderer
#      compiled for Android 4.1 (armeabi-v7a), every call checked against API 16
#
#   port/ouya/tools/check.sh [BUILD_DIR]     (default ~/re4-ouya/o2/build)
# RE4_OUYA_DIFF=DIR: the pictures of scenes that differ go there.
set -euo pipefail
here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
port="$(cd -- "$here/../.." && pwd)"
build="${1:-$HOME/re4-ouya/o2/build}"
cmake -S "$port/ouya/app/src/main/cpp" -B "$build" -DCMAKE_BUILD_TYPE=RelWithDebInfo > /dev/null
cmake --build "$build" --parallel "${RE4_TEST_JOBS:-8}"
if [[ -n "${RE4_OUYA_DIFF:-}" ]]; then
    mkdir -p "$RE4_OUYA_DIFF"
    "$build/gx_gl2_check" "$RE4_OUYA_DIFF"
else
    "$build/gx_gl2_check"
fi
ndk="${RE4_OUYA_NDK:-/opt/android-ndk-r23c}"
clang="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++"
if [[ -x "$clang" ]]; then
    objects="$(mktemp -d)"
    for source in "$port/ouya/app/src/main/cpp/gx_gl2.cpp" "$port/ouya/app/src/main/cpp/gx_gl2_shader.cpp" \
        "$port/android-decomp/app/src/main/cpp/gx_texture.cpp"; do
        "$clang" --target=armv7a-linux-androideabi16 -std=c++17 -O2 -Wall -Wextra -Werror -Wunguarded-availability \
            -fPIC -I "$port/ouya/app/src/main/cpp" -I "$port/android-decomp/app/src/main/cpp" -I "$port/whole/runtime" \
            -c "$source" -o "$objects/$(basename "$source").o"
    done
    rm -rf "$objects"
    echo "Android 4.1 (NDK r23c): the renderer compiles"
else
    echo "Android 4.1: not compiled (no NDK r23c at $ndk)"
fi
