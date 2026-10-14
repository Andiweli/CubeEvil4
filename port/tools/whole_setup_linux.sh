#!/bin/bash
# Installs what port/tools/whole_build.sh needs on Ubuntu 24.04 (cloud sessions
# through .claude/hooks/session-start.sh, locally in WSL2 on the Windows PC):
# clang/lld/cmake/ninja, the 32-bit x86 C/C++ libraries for the i686 test
# build, the ARM32 cross toolchain/sysroot (g++-arm-linux-gnueabihf) and
# qemu-user to run the ARM32 build, clang-tools (clang-query) for the
# byte-view analysis, Mesa's EGL/GLES3 (headless software rendering) for the
# pictures of the app's core-mode renderer in the local r100 check (and the
# app's GL host checks). g++-multilib is not used because Ubuntu
# refuses it next to the ARM cross compiler. Run as root (sudo).
#
#   --android   also the Android NDK r28c (28.2.13676358, the version of the
#               app) under /opt/android-ndk-r28c for whole_build_android.sh;
#               downloads about 700 MB from dl.google.com once
set -euo pipefail

android=0
[ "${1:-}" = "--android" ] && android=1

need=()
# libc++-dev: whole_libcxx_check.py (the NDK's C++ library, checked in the Linux build)
for pkg in libc6-dev-i386 lib32gcc-13-dev lib32stdc++-13-dev g++-arm-linux-gnueabihf qemu-user python3 \
    libgles-dev libegl-dev libegl-mesa0 libgl1-mesa-dri libc++-dev libc++abi-dev; do
  dpkg -s "$pkg" >/dev/null 2>&1 || need+=("$pkg")
done
command -v git >/dev/null 2>&1 || need+=(git)
command -v clang >/dev/null 2>&1 || need+=(clang)
command -v ld.lld >/dev/null 2>&1 || need+=(lld)
command -v cmake >/dev/null 2>&1 || need+=(cmake)
command -v ninja >/dev/null 2>&1 || need+=(ninja-build)
# clang-query for port/tools/whole_byte_views.py (only to re-run that analysis)
command -v clang-query >/dev/null 2>&1 || command -v clang-query-18 >/dev/null 2>&1 || need+=(clang-tools)
if [ "$android" -eq 1 ]; then
  command -v unzip >/dev/null 2>&1 || need+=(unzip)
  command -v curl >/dev/null 2>&1 || need+=(curl)
fi

export DEBIAN_FRONTEND=noninteractive
if [ ${#need[@]} -gt 0 ]; then
  apt-get install -y -q "${need[@]}" >/dev/null 2>&1 || {
    apt-get update -q >/dev/null 2>&1
    apt-get install -y -q "${need[@]}" >/dev/null
  }
fi

if [ "$android" -eq 1 ]; then
  ndk=/opt/android-ndk-r28c
  if ! grep -q '^Pkg.Revision *= *28.2.13676358' "$ndk/source.properties" 2>/dev/null; then
    echo "Android-NDK r28c wird geladen (einmalig, etwa 700 MB) ..."
    tmp=$(mktemp -d)
    curl -fL --retry 3 -o "$tmp/ndk.zip" https://dl.google.com/android/repository/android-ndk-r28c-linux.zip
    rm -rf "$ndk"
    unzip -q "$tmp/ndk.zip" -d /opt
    rm -rf "$tmp"
    grep -q '^Pkg.Revision *= *28.2.13676358' "$ndk/source.properties" || {
      echo "FEHLER: unerwartetes NDK in $ndk" >&2
      exit 1
    }
  fi
  echo "Android-NDK: $ndk"
fi
