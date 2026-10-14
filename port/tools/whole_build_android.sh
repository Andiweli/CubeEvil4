#!/usr/bin/env bash
# Builds the game core for the Android app (armeabi-v7a, the core mode of
# RE4-Android-Decomp) with the NDK and copies the libraries into the app:
#
#   port/tools/whole_build_android.sh [--ndk DIR] [--opt N]
#
#   libre4_platform.so, libre4_main.so, libre4_<module>.so (114), libc++_shared.so
#     -> port/android-decomp/app/src/core/jniLibs/armeabi-v7a/   (not in git)
#
# The NDK: --ndk, else $ANDROID_NDK_HOME / $ANDROID_NDK_ROOT, else
# /opt/android-ndk-r28c (sudo port/tools/whole_setup_linux.sh --android
# installs it there). The app builds with NDK 28.2.13676358 (r28c); the core
# uses the same. Runs in Linux / WSL2 like whole_build.sh (python3, cmake,
# ninja). Optimisation -O2 (--opt 0 for a debug build); the same build is
# checked on Linux with RE4_OPT=2 port/tools/whole_build.sh.
#
# Checks before copying: every library built, no module below the expected
# count, the same list of generated stubs as the Linux build (an optimising
# compiler may drop definitions other libraries need; patch 0027), and the
# core's entry points exported.
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
ndk="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-/opt/android-ndk-r28c}}"
opt=2
while [ $# -gt 0 ]; do
    case "$1" in
        --ndk) ndk="$2"; shift 2 ;;
        --opt) opt="$2"; shift 2 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done
toolchain="$ndk/build/cmake/android.toolchain.cmake"
if [ ! -f "$toolchain" ]; then
    echo "FEHLER: kein Android-NDK unter $ndk (einmal: sudo port/tools/whole_setup_linux.sh --android)" >&2
    exit 1
fi
revision=$(sed -n 's/^Pkg.Revision *= *//p' "$ndk/source.properties" 2> /dev/null | tr -d '\r')
echo "NDK: $ndk ($revision)"
case "$revision" in
    28.*) ;;
    *) echo "Hinweis: die App verwendet NDK 28.2.13676358; dieses NDK ist $revision" ;;
esac

build="$root/port/build/whole-android-armv7"
log="$build.log"
out="$root/port/android-decomp/app/src/core/jniLibs/armeabi-v7a"
assets="$root/port/android-decomp/app/src/core/assets"
mkdir -p "$root/port/build"
echo "Build (Log: port/build/whole-android-armv7.log) ..."
cmake -S "$root/port/whole" -B "$build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
    -DANDROID_ABI=armeabi-v7a \
    -DANDROID_PLATFORM=android-21 \
    -DANDROID_STL=c++_shared \
    -DRE4_OPT_LEVEL="$opt" > "$log" 2>&1 &&
    ninja -C "$build" -k 0 ${RE4_JOBS:+-j "$RE4_JOBS"} >> "$log" 2>&1
code=$?
failed=$(grep -c '^FAILED' "$log")
errors=$(grep -E ': error:|fatal error:|ld.lld: error' "$log" | sort -u | wc -l)
echo "android armeabi-v7a -O$opt: $failed failed build steps, $errors distinct errors"
grep -h '^link plan:' "$log" | tail -1 | sed 's/^/  /'
if [ "$code" -ne 0 ] || [ "$failed" -ne 0 ] || [ "$errors" -ne 0 ]; then
    grep -E ': error:|fatal error:|ld.lld: error' "$log" | sort -u | head -40 | sed 's/^/  /'
    exit 1
fi

status=0
modules=("$build"/modules/libre4_*.so)
count=0
for m in "${modules[@]}"; do
    case "$(basename "$m")" in
        libre4_re4test_*) ;;
        *) count=$((count + 1)) ;;
    esac
done
echo "  module libraries: $count"
[ "$count" -lt 114 ] && { echo "FEHLER: weniger als 114 Modul-Bibliotheken"; status=1; }

# the generated stubs must be the same as in the Linux build (whole_build.sh)
python3 - "$build/generated/link_report.json" "$root/port/build" <<'PY' || status=1
import json, sys
from pathlib import Path
def stubs(path):
    s = json.load(open(path))["stubs"]
    return set(s["functions"] if isinstance(s, dict) else s)
mine = stubs(sys.argv[1])
refs = sorted(Path(sys.argv[2]).glob("whole-*/generated/link_report.json"))
refs = [r for r in refs if "android" not in str(r)]
if not refs:
    print(f"  stubs: {len(mine)} (no Linux build to compare with)")
    sys.exit(0)
ref = stubs(refs[0])
extra, missing = sorted(mine - ref), sorted(ref - mine)
print(f"  stubs: {len(mine)}, Linux build {refs[0].parent.parent.name}: {len(ref)}")
for name in extra:
    print(f"  FEHLER: only stubbed on Android: {name}")
for name in missing:
    print(f"  note: stubbed on Linux only: {name}")
sys.exit(1 if extra else 0)
PY

readelf=$(ls "$ndk"/toolchains/llvm/prebuilt/*/bin/llvm-readelf 2> /dev/null | head -1)
strip=$(ls "$ndk"/toolchains/llvm/prebuilt/*/bin/llvm-strip 2> /dev/null | head -1)
cxxshared=$(ls "$ndk"/toolchains/llvm/prebuilt/*/sysroot/usr/lib/arm-linux-androideabi/libc++_shared.so 2> /dev/null | head -1)
for sym in re4_core_reserve re4_core_start re4_core_state re4_core_message re4_core_pad re4_core_acquire \
    re4_core_release re4_core_log_tail; do
    if ! "$readelf" -W --dyn-syms "$build/libre4_main.so" | grep -q " $sym$"; then
        echo "  FEHLER: libre4_main.so exportiert $sym nicht"
        status=1
    fi
done
[ -f "$cxxshared" ] || { echo "  FEHLER: libc++_shared.so nicht im NDK gefunden"; status=1; }
[ "$status" -ne 0 ] && exit 1

rm -rf "$out"
mkdir -p "$out" "$assets"
cp "$build/libre4_platform.so" "$build/libre4_main.so" "$cxxshared" "$out/"
for m in "${modules[@]}"; do
    case "$(basename "$m")" in
        libre4_re4test_*) ;;
        *) cp "$m" "$out/" ;;
    esac
done
# debug sections out (the APK stays small); symbols stay for crash reports
for f in "$out"/libre4_*.so; do
    "$strip" --strip-debug "$f"
done
commit=$(git -C "$root" rev-parse --short HEAD 2> /dev/null || echo unknown)
{
    echo "commit=$commit"
    echo "ndk=$revision"
    echo "opt=$opt"
    echo "libraries=$(ls "$out" | grep -c '\.so$')"
} > "$assets/core-libs.properties"
echo "Kern-Bibliotheken: $(ls "$out" | grep -c '\.so$') Dateien, $(du -sh "$out" | cut -f1) -> $out"
