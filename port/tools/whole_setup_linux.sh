#!/bin/bash
# Installs what port/tools/whole_build.sh needs on Ubuntu 24.04 (cloud sessions
# through .claude/hooks/session-start.sh, locally in WSL2 on the Windows PC):
# clang/lld/cmake/ninja, the 32-bit x86 C/C++ libraries for the i686 test
# build, the ARM32 cross toolchain/sysroot (g++-arm-linux-gnueabihf) and
# qemu-user to run the ARM32 build, clang-tools (clang-query) for the
# byte-view analysis. g++-multilib is not used because Ubuntu
# refuses it next to the ARM cross compiler. Run as root (sudo).
set -euo pipefail

need=()
for pkg in libc6-dev-i386 lib32gcc-13-dev lib32stdc++-13-dev g++-arm-linux-gnueabihf qemu-user python3; do
  dpkg -s "$pkg" >/dev/null 2>&1 || need+=("$pkg")
done
command -v git >/dev/null 2>&1 || need+=(git)
command -v clang >/dev/null 2>&1 || need+=(clang)
command -v ld.lld >/dev/null 2>&1 || need+=(lld)
command -v cmake >/dev/null 2>&1 || need+=(cmake)
command -v ninja >/dev/null 2>&1 || need+=(ninja-build)
# clang-query for port/tools/whole_byte_views.py (only to re-run that analysis)
command -v clang-query >/dev/null 2>&1 || command -v clang-query-18 >/dev/null 2>&1 || need+=(clang-tools)
[ ${#need[@]} -eq 0 ] && exit 0

export DEBIAN_FRONTEND=noninteractive
apt-get install -y -q "${need[@]}" >/dev/null 2>&1 || {
  apt-get update -q >/dev/null 2>&1
  apt-get install -y -q "${need[@]}" >/dev/null
}
