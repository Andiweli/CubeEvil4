#!/usr/bin/env python3
"""Names the functions in a profile of the core probe (re4_probe --profile FILE,
port/whole/host/probe.cpp; port/scripts/ouya-probe.ps1): where the game's thread
spends its time, by library, by rough area and by function.

    probe_profile.py FILE [DIR...]

FILE: lines "LIBRARY OFFSET COUNT" (one per sampled place of the game's thread)
DIR:  folders searched for the libraries with symbols (default: the OUYA build of
      whole_build_android.sh --ouya in WSL, ~/re4-ouya/RE4/port/build/whole-android-ouya,
      then this checkout's port/build/whole-android-ouya). The offsets become function
      names with llvm-symbolizer of an NDK (android_profile.py).
"""
import os
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if not (os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT")):
    for ndk in ("/opt/android-ndk-r28c", "/opt/android-ndk-r23c"):
        if os.path.isdir(ndk):
            os.environ["ANDROID_NDK_HOME"] = ndk
            break
sys.path.insert(0, str(Path(__file__).resolve().parent))
import android_profile  # noqa: E402 (needs the NDK chosen above)

# rough areas by function name (first match wins); the rest is the game's own code by library
AREAS = [
    ("Ton (DSP, AX, Synthesizer, Hall)", re.compile(
        r"dsp|DSP|\bAX|^AX|^ax|__AX|syn[A-Z_]|^syn|SEQ|^seq|mix[A-Z_]|^MIX|reverb|Reverb|chorus|Chorus|delay|Delay|"
        r"re4_ai|re4_audio|AIInit|AXFX|Snd|snd_")),
    ("Filme (CRI: ADX, Sofdec, MPEG)", re.compile(r"^ADX|^adx|^CRI|^cri|^SFD|^sfd|^MPV|^mpv|^SFX|Sofdec")),
    ("Grafik: Umrechnung der GX-Befehle (Port)", re.compile(r"re4gx|gx_decode|gx_fifo|re4_gx|GxDecode|Decoder")),
    ("Grafik: GX-Bibliothek des SDK", re.compile(r"^GX|^__GX")),
    ("Matrix- und Vektorrechnung", re.compile(r"MTX|Mtx|VEC|Vec[A-Z]|QUAT|^C_|^PS[A-Z]|CalcSk|Skin")),
    ("Speicher kopieren/fuellen (C-Bibliothek)", re.compile(r"^mem(cpy|move|set)|__aeabi_mem")),
]


def area(lib, name):
    for label, pattern in AREAS:
        if pattern.search(name):
            return label
    if lib in ("libre4_main.so",) or re.match(r"libre4_(?!platform).*\.so$", lib):
        return "Spiellogik (Spielcode, Module)"
    if lib == "libre4_platform.so":
        return "Plattformschicht sonst"
    return "System (" + lib + ")"


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    dirs = sys.argv[2:] or [os.path.expanduser("~/re4-ouya/RE4/port/build/whole-android-ouya"),
                            str(ROOT / "port/build/whole-android-ouya")]
    places = []
    for line in Path(sys.argv[1]).read_text(errors="replace").splitlines():
        parts = line.split()
        if len(parts) == 3 and not line.startswith("#"):
            try:
                places.append((parts[0], int(parts[1], 16), int(parts[2])))
            except ValueError:
                pass
    total = sum(p[2] for p in places) or 1
    libs = android_profile.find_libs(dirs)
    by_lib = defaultdict(lambda: defaultdict(int))
    for lib, offset, n in places:
        by_lib[lib][offset] += n
    names = {}
    missing = []
    for lib, offsets in by_lib.items():
        if lib.startswith("libre4") or lib == "libc++_shared.so":
            if lib in libs:
                names[lib] = android_profile.symbolize(libs[lib], sorted(offsets))
            else:
                missing.append(lib)
    per_lib = defaultdict(int)
    per_func = defaultdict(int)
    per_area = defaultdict(int)
    for lib, offset, n in places:
        name = names.get(lib, {}).get(offset) or "?"
        if name == "??":
            name = "?"
        per_lib[lib] += n
        per_func[(lib, name)] += n
        per_area[area(lib, name)] += n
    print(f"Profil des Spiel-Threads: {total} Proben (etwa 1000 je Sekunde)")
    print()
    print("Bereiche (grob, nach Funktionsnamen):")
    for label, n in sorted(per_area.items(), key=lambda t: -t[1]):
        print(f"  {100.0 * n / total:5.1f}%  {label}")
    print()
    print("Bibliotheken:")
    for lib, n in sorted(per_lib.items(), key=lambda t: -t[1])[:10]:
        print(f"  {100.0 * n / total:5.1f}%  {lib}")
    print()
    print("Funktionen (die 40 groessten):")
    for (lib, name), n in sorted(per_func.items(), key=lambda t: -t[1])[:40]:
        print(f"  {100.0 * n / total:5.1f}%  {lib:22s} {name[:110]}")
    if missing:
        print()
        print("Ohne Bibliothek mit Symbolen: " + ", ".join(sorted(missing)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
