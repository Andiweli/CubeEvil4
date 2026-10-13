#!/usr/bin/env bash
# Builds the whole-program port for every available Linux toolchain and prints
# a one-line result per target plus the link-readiness report.
#
#   port/tools/whole_build.sh            # i686 and armhf
#   port/tools/whole_build.sh i686       # one target
#
# Needs clang, cmake, ninja, python3; i686 needs g++-multilib, armhf needs
# g++-arm-linux-gnueabihf (apt-get install -y g++-multilib g++-arm-linux-gnueabihf).
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
    errors=$(grep -E ': error:|fatal error:' "$log" | sort -u | wc -l)
    echo "$t: $failed failed units, $errors distinct errors (log: port/build/whole-$t.log)"
    [ "$failed" -ne 0 ] || [ "$errors" -ne 0 ] && status=1
done
if [ -d "$root/port/build/whole-${targets[0]}" ]; then
    python3 "$root/port/tools/whole_symbols.py" "$root/port/build/whole-${targets[0]}" \
        --json "$root/port/build/whole-${targets[0]}/symbols.json"
fi
exit $status
