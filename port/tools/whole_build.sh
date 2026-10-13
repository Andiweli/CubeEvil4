#!/usr/bin/env bash
# Builds the whole-program port for the Linux test targets and runs the checks:
#
#   port/tools/whole_build.sh            # i686 and armhf
#   port/tools/whole_build.sh i686       # one target
#
# Per target: build (libre4_main.so, 114 module libraries, re4_host), then
#   re4_host --self-test       cross-module trampolines pass arguments intact
#   re4_host --check-modules   every module loads, every cross-module import resolves
# armhf runs under qemu-arm when it is installed. Needs clang, lld, cmake, ninja,
# python3, the lib32 C/C++ libraries, g++-arm-linux-gnueabihf and qemu-user;
# .claude/hooks/session-start.sh installs them in cloud sessions.
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
targets=("$@")
[ $# -eq 0 ] && targets=(i686 armhf)
status=0
for t in "${targets[@]}"; do
    build="$root/port/build/whole-$t"
    log="$root/port/build/whole-$t.log"
    mkdir -p "$root/port/build"
    cmake -S "$root/port/whole" -B "$build" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$root/port/whole/cmake/linux-$t.cmake" > "$log" 2>&1 &&
        ninja -C "$build" -k 0 >> "$log" 2>&1
    failed=$(grep -c '^FAILED' "$log")
    errors=$(grep -E ': error:|fatal error:|ld.lld: error' "$log" | sort -u | wc -l)
    echo "$t: $failed failed build steps, $errors distinct errors (log: port/build/whole-$t.log)"
    grep -h '^link plan:' "$log" | tail -1 | sed 's/^/  /'
    if [ "$failed" -ne 0 ] || [ "$errors" -ne 0 ]; then
        status=1
        continue
    fi
    run=()
    if [ "$t" = armhf ]; then
        if ! command -v qemu-arm > /dev/null; then
            echo "  (qemu-arm missing: armhf not executed)"
            continue
        fi
        run=(env QEMU_LD_PREFIX=/usr/arm-linux-gnueabihf qemu-arm)
    fi
    for check in --self-test --check-modules; do
        out=$(cd "$build" && timeout 300 "${run[@]}" ./re4_host "$check" 2>&1)
        code=$?
        echo "  re4_host $check: exit $code | $(echo "$out" | grep -E 'self-test:|check-modules:' | sed 's/^\[re4\] //')"
        [ "$code" -ne 0 ] && status=1
    done
done
exit $status
