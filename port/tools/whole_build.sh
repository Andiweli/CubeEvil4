#!/usr/bin/env bash
# Builds the whole-program port for the Linux test targets and runs the checks:
#
#   port/tools/whole_build.sh            # i686 and armhf
#   port/tools/whole_build.sh i686       # one target
#   RE4_JOBS=4 port/tools/whole_build.sh # at most 4 compile jobs (memory)
#
# Per target: build (libre4_main.so, 114 module libraries, re4_host), then
#   re4_host --self-test       cross-module trampolines pass arguments intact
#   re4_host --check-modules   every module loads, every cross-module import resolves
#   re4_host --runtime-test    OSLink, DVD folder and yz2 work without disc data
#   re4_format_compare         the Formatumwandler against the app's checked
#                              big-endian readers (synthetic samples, then the
#                              same as a disc folder: test_whole_format_compare.py)
# armhf runs under qemu-arm when it is installed. Finally the disc folder tool
# is tested (port/tools/test_whole_disc_folder.py, synthetic images). Needs
# clang, lld, cmake, ninja, python3, the lib32 C/C++ libraries,
# g++-arm-linux-gnueabihf and qemu-user: sudo port/tools/whole_setup_linux.sh
# (cloud sessions run it through .claude/hooks/session-start.sh).
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
        ninja -C "$build" -k 0 ${RE4_JOBS:+-j "$RE4_JOBS"} >> "$log" 2>&1
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
    for check in --self-test --check-modules --runtime-test; do
        out=$(cd "$build" && timeout 300 "${run[@]}" ./re4_host "$check" 2>&1)
        code=$?
        echo "  re4_host $check: exit $code | $(echo "$out" | grep -E 'self-test:|check-modules:|runtime-test' | tr '\n' ' ' | sed 's/\[re4\] //g')"
        [ "$code" -ne 0 ] && status=1
    done
    out=$(cd "$build" && timeout 300 "${run[@]}" ./re4_format_compare 2>&1)
    code=$?
    echo "  re4_format_compare: exit $code | $(echo "$out" | grep '^Ergebnis:')"
    if [ "$code" -ne 0 ]; then
        echo "$out" | grep -vE '^  OK |^\[re4\]' | sed 's/^/    /'
        status=1
    fi
    out=$(python3 "$root/port/tools/test_whole_format_compare.py" "$build" "${run[@]}" 2>&1)
    code=$?
    echo "  re4_format_compare --data (synthetic disc folder): exit $code | $(echo "$out" | tail -1)"
    [ "$code" -ne 0 ] && { echo "$out" | sed 's/^/    /'; status=1; }
done
if python3 "$root/port/tools/test_whole_disc_folder.py" > "$root/port/build/disc-folder-test.log" 2>&1; then
    echo "disc folder tool: ok"
else
    echo "disc folder tool: FAILED (log: port/build/disc-folder-test.log)"
    status=1
fi
exit $status
