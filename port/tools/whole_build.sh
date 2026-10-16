#!/usr/bin/env bash
# Builds the whole-program port for the Linux test targets and runs the checks:
#
#   port/tools/whole_build.sh            # i686 and armhf
#   port/tools/whole_build.sh i686       # one target
#   RE4_JOBS=4 port/tools/whole_build.sh # at most 4 compile jobs (memory)
#   RE4_OPT=2 port/tools/whole_build.sh  # optimised like the Android build
#                                        # (build folders whole-<target>-O2)
#
# Per target: build (libre4_main.so, 114 module libraries, re4_host), then
#   whole_stub_check.py        the generated stubs as port/whole/stubs-expected.txt lists them
#   re4_host --self-test       cross-module trampolines pass arguments intact
#   re4_host --check-modules   every module loads, every cross-module import resolves
#   re4_host --runtime-test    OSLink, DVD folder and yz2 work without disc data
#   re4_host --core-test       the embedding interface of the app (core thread,
#                              controller, snapshots) without disc data, the
#                              memory card library on a card file
#   memcard_list.py            that card file read back (files, the game's CRCs)
#   re4_format_compare         the Formatumwandler against the app's checked
#                              big-endian readers (synthetic samples, then the
#                              same as a disc folder: test_whole_format_compare.py)
#   test_whole_movie.py        a synthetic Sofdec movie (ffmpeg) through the CRI
#                              middleware: frames against ffmpeg, pacing, sound
# armhf runs under qemu-arm when it is installed. Then the port's runtime files
# are compiled against libc++, the NDK's C++ library (whole_libcxx_check.py:
# what only the Android build would notice), and the disc folder tool is
# tested (port/tools/test_whole_disc_folder.py, synthetic images). Needs
# clang, lld, cmake, ninja, python3, the lib32 C/C++ libraries,
# g++-arm-linux-gnueabihf and qemu-user: sudo port/tools/whole_setup_linux.sh
# (cloud sessions run it through .claude/hooks/session-start.sh).
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
targets=("$@")
[ $# -eq 0 ] && targets=(i686 armhf)
status=0
# the runtime test checks the per-thread wait record of the hang search
export RE4_WAIT_TRACE=1
for t in "${targets[@]}"; do
    name="whole-$t${RE4_OPT:+-O$RE4_OPT}"
    build="$root/port/build/$name"
    log="$root/port/build/$name.log"
    mkdir -p "$root/port/build"
    cmake -S "$root/port/whole" -B "$build" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$root/port/whole/cmake/linux-$t.cmake" -DRE4_OPT_LEVEL="${RE4_OPT:-}" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > "$log" 2>&1 &&
        ninja -C "$build" -k 0 ${RE4_JOBS:+-j "$RE4_JOBS"} >> "$log" 2>&1
    code=$?
    failed=$(grep -c '^FAILED' "$log")
    errors=$(grep -E ': error:|fatal error:|ld.lld: error' "$log" | sort -u | wc -l)
    echo "$t${RE4_OPT:+ -O$RE4_OPT}: $failed failed build steps, $errors distinct errors (log: port/build/$name.log)"
    grep -h '^link plan:' "$log" | tail -1 | sed 's/^/  /'
    if [ "$code" -ne 0 ] || [ "$failed" -ne 0 ] || [ "$errors" -ne 0 ]; then
        # a failed CMake configure (whole_prepare.py, patches) leaves the programs of
        # the build before: the checks below would test that old state
        [ "$failed" -eq 0 ] && [ "$errors" -eq 0 ] && { echo "  build stopped (exit code $code):"; tail -n 8 "$log" | sed 's/^/    /'; }
        status=1
        continue
    fi
    # the generated stubs against the list of the repository (port/whole/stubs-expected.txt)
    python3 "$root/port/tools/whole_stub_check.py" "$build/generated/link_report.json" || status=1
    run=()
    if [ "$t" = armhf ]; then
        if ! command -v qemu-arm > /dev/null; then
            echo "  (qemu-arm missing: armhf not executed)"
            continue
        fi
        run=(env QEMU_LD_PREFIX=/usr/arm-linux-gnueabihf qemu-arm)
    fi
    # the core test keeps the card of its memory card checks (RE4_CARD_TEST_FILE),
    # which port/tools/memcard_list.py then reads back
    card="$build/card-test.bin"
    rm -f "$card"
    for check in --self-test --check-modules --runtime-test --core-test; do
        out=$(cd "$build" && RE4_CARD_TEST_FILE="$card" timeout 300 "${run[@]}" ./re4_host "$check" 2>&1)
        code=$?
        echo "  re4_host $check: exit $code | $(echo "$out" | grep -E 'self-test:|check-modules:|runtime-test|core-test' | tr '\n' ' ' | sed 's/\[re4\] //g')"
        [ "$code" -ne 0 ] && status=1
    done
    out=$(python3 "$root/port/tools/memcard_list.py" "$card" --expect-system --expect-save 0 2>&1)
    code=$?
    echo "  memcard_list.py (the card of the core test): exit $code | $(echo "$out" | grep -cE '^  file ') files, $(echo "$out" | grep -oE 'CRC[^;,]*' | tr '\n' ' ')"
    [ "$code" -ne 0 ] && { echo "$out" | sed 's/^/    /'; status=1; }
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
    # a synthetic Sofdec movie through the CRI middleware (needs ffmpeg)
    out=$(python3 "$root/port/tools/test_whole_movie.py" "$build" "${run[@]}" 2>&1)
    code=$?
    echo "  re4_host --movie-test (synthetic movie): exit $code | $(echo "$out" | grep -E '^  (frames|sound|A/V|GX picture[^:]*|fast mode \(RE4_VI_FAST, the PC check\)|60 Hz fields):' | sed 's/^  //' | tr '\n' ';') $(echo "$out" | tail -1)"
    [ "$code" -ne 0 ] && { echo "$out" | sed 's/^/    /'; status=1; }
done
first="$root/port/build/whole-${targets[0]}${RE4_OPT:+-O$RE4_OPT}"
if [ -f "$first/compile_commands.json" ]; then
    python3 "$root/port/tools/whole_libcxx_check.py" "$first" || status=1
fi
if python3 "$root/port/tools/test_whole_disc_folder.py" > "$root/port/build/disc-folder-test.log" 2>&1; then
    echo "disc folder tool: ok"
else
    echo "disc folder tool: FAILED (log: port/build/disc-folder-test.log)"
    status=1
fi
exit $status
