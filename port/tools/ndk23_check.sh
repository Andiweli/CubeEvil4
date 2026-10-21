#!/usr/bin/env bash
# The OUYA build (Android 4.1, NDK r23c; port/docs/ouya-machbarkeit.md) checked in the
# cloud, where the NDK itself cannot be downloaded (dl.google.com is not reachable):
# every compile step of a Linux armhf build (port/tools/whole_build.sh armhf) again
# against a stand-in for the NDK r23c's headers, with clang 12 like the NDK's:
#
#   port/tools/ndk23_check.sh [--build DIR] [--work DIR] [--clang18]
#
#   libc++     LLVM 12.0.0 (github.com/llvm/llvm-project; the NDK's is a snapshot
#              between LLVM 11 and 12)
#   bionic     Android 12 (github.com/aosp-mirror/platform_bionic, android-12.0.0_r1),
#              API 16, unavailable functions only warn (the NDK's own headers hide them)
#   clang 12   from zig 0.8.1 (PyPI ziglang, "zig clang"); --clang18: the system clang
#
# Downloaded once into --work (default ~/re4-ndk23). Prints each distinct error with its
# count, the units with errors, and every call of a C library function newer than
# Android 4.1 (-Wunguarded-availability, also in game code built with -w). Also the
# Android-only programs (host/probe.cpp, host/coreproc.cpp) and <android/log.h> users
# (a stand-in for liblog's header). Known limits: units that rely on C++17 get it from
# CMake only with the real NDK (clang 12 defaults to C++14, so they get -std=gnu++17
# here); the assembler stubs are left out; the NDK r23c's own sysroot reads <math.h>
# from an earlier system header, this stand-in does not (the fabsf error of the first
# PC build, 21.10.2026, did not show here).
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$root/port/build/whole-armhf"
work="$HOME/re4-ndk23"
clang18=0
while [ $# -gt 0 ]; do
    case "$1" in
        --build) build="$2"; shift 2 ;;
        --work) work="$2"; shift 2 ;;
        --clang18) clang18=1; shift ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done
[ -f "$build/build.ninja" ] || { echo "no armhf build in $build (port/tools/whole_build.sh armhf)" >&2; exit 2; }
mkdir -p "$work"
sparse() {  # sparse() URL TAG DIR PATHS...
    local url=$1 tag=$2 dir=$3
    shift 3
    [ -d "$work/$dir/.git" ] && return 0
    git clone -q --filter=blob:none --no-checkout --depth 1 --branch "$tag" "$url" "$work/$dir" &&
        git -C "$work/$dir" sparse-checkout set --no-cone "$@" > /dev/null 2>&1 &&
        git -C "$work/$dir" checkout -q
}
sparse https://github.com/llvm/llvm-project llvmorg-12.0.0 llvm libcxx/include || exit 1
sparse https://github.com/aosp-mirror/platform_bionic android-12.0.0_r1 bionic \
    libc/include libc/kernel/uapi libc/kernel/android/uapi || exit 1
if [ "$clang18" -eq 1 ]; then
    cxx=clang
    res="$(clang -print-resource-dir)/include"
else
    if [ ! -x "$work/zig/ziglang/zig" ]; then
        (cd "$work" && pip download -q --no-deps ziglang==0.8.1 --platform manylinux_2_12_x86_64 \
            --only-binary=:all: -d wheel && mkdir -p zig && cd zig && python3 -I -m zipfile -e ../wheel/ziglang-0.8.1-*.whl . &&
            chmod +x ziglang/zig) || exit 1
    fi
    cxx="$work/zig/ziglang/zig clang"
    res="$work/zig/ziglang/lib/include"
fi
mkdir -p "$work/extra/android"
cat > "$work/extra/android/log.h" << 'EOF_LOG'
/* stand-in for the NDK's <android/log.h> (liblog is not part of bionic's sources) */
#pragma once
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum android_LogPriority { ANDROID_LOG_UNKNOWN = 0, ANDROID_LOG_DEFAULT, ANDROID_LOG_VERBOSE, ANDROID_LOG_DEBUG,
    ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR, ANDROID_LOG_FATAL, ANDROID_LOG_SILENT } android_LogPriority;
int __android_log_write(int prio, const char* tag, const char* text);
int __android_log_print(int prio, const char* tag, const char* fmt, ...);
int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap);
#ifdef __cplusplus
}
#endif
EOF_LOG
sys="--target=armv7a-none-linux-androideabi16 -nostdinc -nostdinc++ -D__ANDROID__=1 -Wno-macro-redefined -D__ANDROID_API__=16"
sys+=" -D__ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__ -D_LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS"
sys+=" -D_LIBCPP_ABI_NAMESPACE=__ndk1 -isystem $work/extra -isystem $work/llvm/libcxx/include -isystem $res"
sys+=" -isystem $work/bionic/libc/include -isystem $work/bionic/libc/kernel/uapi"
sys+=" -isystem $work/bionic/libc/kernel/uapi/asm-arm -isystem $work/bionic/libc/kernel/android/uapi"
out="$work/run"
rm -rf "$out"
mkdir -p "$out"
(cd "$build" && ninja -t commands) | grep -E "^/usr/bin/clang(\+\+)? .* -c [^ ]+\.(c|cpp)$" |
    grep -vE "re4_app_readers|format_compare|/tests/|/host/(main|loader|det_math)|re4test" |
    sort -u > "$out/commands.txt"
n=0
while IFS= read -r cmd; do
    n=$((n + 1))
    std=""
    case "$cmd" in *" -std="*) ;; *.cpp) std=" -std=gnu++17" ;; esac
    cmd=$(echo "$cmd" | sed "s| --target=arm-linux-gnueabihf||; s| -D_FILE_OFFSET_BITS=64||; s| -MD -MT [^ ]* -MF [^ ]*||; s| -o [^ ]*| -o /dev/null|; s| -w | |")
    echo "$cxx $sys ${cmd#* }$std -Wno-everything -Wunguarded-availability" > "$out/unit$n.sh"
done < "$out/commands.txt"
ls "$out"/unit*.sh | xargs -P "$(nproc)" -I{} sh -c "cd '$build' && sh {} > {}.err 2>&1 || true"
echo "NDK r23c stand-in ($([ "$clang18" -eq 1 ] && echo "clang 18" || echo "clang 12")): $n units"
grep -h "error:" "$out"/*.err | sed "s|$work/||; s|$root/||" | sort | uniq -c | sort -rn | head -30
# calls of functions newer than API 16 outside the C++ library's own headers
newer=$(grep -h "is only available on Android" "$out"/*.err | grep -v "libcxx/include" | sed "s|$work/||; s|$root/||" | sort -u)
echo "calls newer than Android 4.1: $(echo -n "$newer" | grep -c .)"
echo -n "$newer" | head -20 | sed 's/^/  /'

bad=$(grep -l "error:" "$out"/*.err | wc -l)
for f in $(grep -l "error:" "$out"/*.err); do
    grep -o " -c [^ ]*" "${f%.err}" | sed "s| -c $root/|  unit: |"
done | sort -u | head -20
echo "units with errors: $bad"
[ "$bad" -eq 0 ]
