#!/usr/bin/env bash
# Milestone 12, inventory: how much slower game-style code runs inside the
# wasm32 sandbox (clang --target=wasm32, wasm2c, then native C) than compiled
# natively. Needs clang 18 with lld (wasm-ld) and wabt (wasm2c, libwasm-rt-impl):
#   sudo apt-get install wabt
# Prints the best of seven runs of each; both must give the same result.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="${1:-$here/../../build/m12-bench}"
mkdir -p "$out"
cd "$out"
clang++ -O2 -fno-exceptions -fno-rtti "$here/wasm_bench.cpp" "$here/wasm_bench_native.cpp" -o bench-native
clang++ --target=wasm32 -O2 -fno-exceptions -fno-rtti -nostdlib -mbulk-memory -Wl,--no-entry \
    -Wl,--export=bench_run -Wl,--export-memory "$here/wasm_bench.cpp" -o bench.wasm
wasm2c bench.wasm -o bench_wasm.c --module-name=bench
clang -O2 -I. "$here/wasm_bench_w2c.c" bench_wasm.c "$(dirname "$(command -v wasm2c)")/../lib/$(gcc -dumpmachine)/libwasm-rt-impl.a" \
    -lm -o bench-w2c
for b in bench-native bench-w2c; do
    best=
    for i in 1 2 3 4 5 6 7; do
        line=$(./$b)
        t=$(echo "$line" | grep -o '[0-9.]* s' | cut -d' ' -f1)
        if [ -z "$best" ] || awk "BEGIN{exit !($t < $best)}"; then best=$t; fi
    done
    echo "$line" | sed "s/, [0-9.]* s/, best of 7: $best s/"
done
