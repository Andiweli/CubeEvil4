#!/usr/bin/env bash
# Translates the backtrace of a crash on the device (tombstone lines
# "#NN pc ADDR  .../libX.so (...) (BuildId: ...)" from logcat or the system's
# crash report) into function, source file and line, with the libraries of the
# builds on this PC (they still carry their symbols):
#
#   port/tools/android_symbolize.sh CRASH.txt [DIR...]
#
# DIR: folders searched for libX.so; default: the core build of
# whole_build_android.sh (~/re4-kern/RE4/port/build/whole-android-armv7 and the
# checkout's own port/build/whole-android-armv7), the app's native build
# (port/android-decomp/app/build/intermediates/cxx) and the core libraries in
# the app (jniLibs). A library is used when its build ID is the one in the
# backtrace. port/scripts/test-core-app.ps1 calls this after a crash.
set -u
crash="${1:?usage: android_symbolize.sh CRASH.txt [DIR...]}"
shift
root="$(cd "$(dirname "$0")/../.." && pwd)"
dirs=("$@")
if [ ${#dirs[@]} -eq 0 ]; then
    dirs=("$HOME/re4-kern/RE4/port/build/whole-android-armv7"
        "$root/port/build/whole-android-armv7"
        "$root/port/android-decomp/app/build/intermediates/cxx"
        "$root/port/android-decomp/app/src/core/jniLibs/armeabi-v7a")
fi
ndk="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-/opt/android-ndk-r28c}}"
bin=$(ls -d "$ndk"/toolchains/llvm/prebuilt/*/bin 2> /dev/null | head -1)
a2l="$bin/llvm-addr2line"
readelf="$bin/llvm-readelf"
if [ ! -x "$a2l" ] || [ ! -x "$readelf" ]; then
    echo "(kein Android-NDK unter $ndk: Absturzstelle nicht uebersetzt)"
    exit 0
fi

declare -A chosen  # "name buildid" -> library file ("" when none matches)

build_id() {
    "$readelf" -n "$1" 2> /dev/null | sed -n 's/.*Build ID: *\([0-9a-f]*\).*/\1/p' | head -1
}

# The library with this name and build ID; without a build ID in the line the
# first armeabi-v7a copy with debug information.
find_lib() {
    local name="$1" id="$2" key="$1 $2" f fallback=""
    if [ -n "${chosen[$key]+x}" ]; then
        printf '%s' "${chosen[$key]}"
        return
    fi
    while IFS= read -r f; do
        case "$f" in
            *arm64*|*x86*) continue ;;
        esac
        if [ -n "$id" ] && [ "$(build_id "$f")" = "$id" ]; then
            chosen[$key]="$f"
            printf '%s' "$f"
            return
        fi
        [ -z "$fallback" ] && [ -z "$id" ] && fallback="$f"
    done < <(find "${dirs[@]}" -name "$name" -type f 2> /dev/null)
    chosen[$key]="$fallback"
    printf '%s' "$fallback"
}

shorten() {
    sed -E 's#^.*/(port|src|include)/#\1/#'
}

count=0
while IFS= read -r line; do
    line="${line//$'\r'/}"
    [[ "$line" =~ \#([0-9]+)\ pc\ ([0-9a-f]+)\ +([^ ]+) ]] || continue
    frame="${BASH_REMATCH[1]}"
    pc="${BASH_REMATCH[2]}"
    path="${BASH_REMATCH[3]}"
    name="${path##*/}"
    name="${name##*!}"
    id=""
    [[ "$line" =~ BuildId:\ ([0-9a-f]+) ]] && id="${BASH_REMATCH[1]}"
    count=$((count + 1))
    case "$name" in
        *.so) ;;
        *) echo "#$frame $name"; continue ;;
    esac
    lib=$(find_lib "$name" "$id")
    if [ -z "$lib" ]; then
        echo "#$frame $name+0x$pc (keine passende Bibliothek mit Symbolen auf diesem PC)"
        continue
    fi
    # function and file:line, inlined calls first
    first=1
    while IFS= read -r func && IFS= read -r where; do
        where=$(printf '%s' "$where" | shorten)
        if [ "$first" -eq 1 ]; then
            echo "#$frame $name+0x$pc: $func ($where)"
            first=0
        else
            echo "      eingebettet in: $func ($where)"
        fi
    done < <("$a2l" -f -C -i -e "$lib" "0x$pc" 2> /dev/null)
done < "$crash"
[ "$count" -eq 0 ] && echo "(keine Stapelzeilen im Absturzbericht)"
exit 0
