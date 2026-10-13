#!/bin/bash
# Cloud sessions: install what the whole-program port build needs
# (port/tools/whole_build.sh): the 32-bit x86 C/C++ libraries for clang -m32 and
# the ARM32 cross toolchain/sysroot (g++-arm-linux-gnueabihf) and qemu-user to
# run the ARM32 build. g++-multilib is not used because Ubuntu refuses it next
# to the ARM cross compiler.
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

need=()
for pkg in libc6-dev-i386 lib32gcc-13-dev lib32stdc++-13-dev g++-arm-linux-gnueabihf qemu-user; do
  dpkg -s "$pkg" >/dev/null 2>&1 || need+=("$pkg")
done
command -v clang >/dev/null 2>&1 || need+=(clang)
command -v ld.lld >/dev/null 2>&1 || need+=(lld)
command -v cmake >/dev/null 2>&1 || need+=(cmake)
command -v ninja >/dev/null 2>&1 || need+=(ninja-build)
[ ${#need[@]} -eq 0 ] && exit 0

export DEBIAN_FRONTEND=noninteractive
apt-get install -y -q "${need[@]}" >/dev/null 2>&1 || {
  apt-get update -q >/dev/null 2>&1
  apt-get install -y -q "${need[@]}" >/dev/null
}
