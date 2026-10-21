#!/usr/bin/env python3
"""Names the functions in a profile of the core probe (re4_probe --profile FILE,
port/whole/host/probe.cpp; port/scripts/ouya-probe.ps1): where the game's thread
spends its time, by library, by rough area and by function.

    probe_profile.py FILE [DIR...]

FILE: lines "LIBRARY OFFSET COUNT" (one per sampled place of the game's thread, or of
      the GP thread: FILE.gp), "# waiting: N" for the samples the thread was asleep
DIR:  folders searched for the libraries with symbols (default: the OUYA build of
      whole_build_android.sh --ouya in WSL, ~/re4-ouya/RE4/port/build/whole-android-ouya,
      then this checkout's port/build/whole-android-ouya). The offsets become function
      names with llvm-symbolizer of an NDK (android_profile.py).
"""
import os
import re
import shutil
import subprocess
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


# rough areas by source file (the build's libraries carry debug information); first match wins
FILE_AREAS = [
    ("Grafik: Umrechnung der GX-Befehle (Port)", re.compile(r"runtime/gx_[a-z_]+\.cpp$")),
    ("Grafik: GX-Bibliothek des SDK", re.compile(r"src/lib/GX[A-Za-z]+\.c$")),
    ("Ton (DSP, AX, Synthesizer, Hall)", re.compile(
        r"runtime/(dsp|ai)\.cpp$|src/lib/(AX|ax|syn|mix|seq|reverb|chorus|delay|DSP)[A-Za-z0-9_]*\.c$")),
    ("Matrix- und Vektorrechnung", re.compile(r"(mtx|mtxvec|mtx44|mtx44vec|vec|quat)\.c$|runtime/mtx_ps\.c$")),
    ("Filme (CRI: ADX, Sofdec, MPEG)", re.compile(r"src/lib/cri/")),
    ("Zustandsbruecke und Protokoll des Ports", re.compile(r"runtime/(core_bridge|core_api|state_trace|log)\.cpp$")),
    ("Spiellogik (Spielcode, Module)", re.compile(r"^src/(?!lib/)|/prepared/tree/src/(?!lib/)")),
]


def symbolize(lib, addresses):
    """Address -> (function, source file) with llvm-symbolizer (no inlined callees)."""
    tool = next(Path(android_profile.NDK).glob("toolchains/llvm/prebuilt/*/bin/llvm-symbolizer"), None) or \
        shutil.which("llvm-symbolizer")
    if not tool:
        return {}
    text = "\n".join(hex(a) for a in addresses) + "\n"
    proc = subprocess.run([str(tool), "--obj=" + lib, "-C", "--inlining=false", "--functions=linkage"],
                          input=text, capture_output=True, text=True)
    out, block = [], []
    for line in proc.stdout.splitlines() + [""]:
        if line.strip() == "":
            if block:
                where = block[1].rsplit(":", 2)[0] if len(block) > 1 else "?"
                out.append((block[0], where))
                block = []
        else:
            block.append(line.strip())
    return dict(zip(addresses, out))


def symbolize_lines(lib, addresses):
    """Address -> (innermost function, source file:line), inlined callees included."""
    tool = next(Path(android_profile.NDK).glob("toolchains/llvm/prebuilt/*/bin/llvm-symbolizer"), None) or \
        shutil.which("llvm-symbolizer")
    if not tool:
        return {}
    text = "\n".join(hex(a) for a in addresses) + "\n"
    proc = subprocess.run([str(tool), "--obj=" + lib, "-C", "--inlining=true", "--functions=linkage"],
                          input=text, capture_output=True, text=True)
    out, block = [], []
    for line in proc.stdout.splitlines() + [""]:
        if line.strip() == "":
            if block:
                where = block[1].rsplit(":", 1)[0] if len(block) > 1 else "?"  # without the column
                where = re.sub(r"^.*?/(port|src|prepared/tree)/", r"\1/", where)
                out.append((block[0], where))
                block = []
        else:
            block.append(line.strip())
    return dict(zip(addresses, out))


def area(lib, name, source="?"):
    for label, pattern in FILE_AREAS:
        if pattern.search(source):
            return label
    for label, pattern in AREAS:
        if pattern.search(name):
            return label
    if lib in ("libc.so", "libm.so"):
        return f"C-Bibliothek ({lib}, ohne Namen: vermutlich Speicherkopien bzw. Mathefunktionen)"
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
    waiting = 0
    which = "Spiel-Threads"
    for line in Path(sys.argv[1]).read_text(errors="replace").splitlines():
        if line.startswith("# waiting:"):
            waiting = int(line.split(":")[1])
        if line.startswith("# re4_probe samples of the GP thread"):
            which = "Grafik-Threads"
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
    lines = {}
    missing = []
    for lib, offsets in by_lib.items():
        if lib.startswith("libre4") or lib == "libc++_shared.so":
            if lib in libs:
                names[lib] = symbolize(libs[lib], sorted(offsets))
                lines[lib] = symbolize_lines(libs[lib], sorted(offsets))
            else:
                missing.append(lib)
    per_lib = defaultdict(int)
    per_func = defaultdict(int)
    per_area = defaultdict(int)
    per_line = defaultdict(int)
    for lib, offset, n in places:
        inner = lines.get(lib, {}).get(offset)
        if inner:
            per_line[(lib, inner[0], inner[1])] += n
        name, source = names.get(lib, {}).get(offset) or ("?", "?")
        if name == "??":
            name = "?"
        per_lib[lib] += n
        per_func[(lib, name)] += n
        per_area[area(lib, name, source)] += n
    print(f"Profil des {which}: {total} Proben bei der Arbeit (etwa 1000 je Sekunde)")
    if waiting:
        print(f"Dazu {waiting} Proben, in denen er wartete (schlief): "
              f"{100.0 * waiting / (total + waiting):.1f} % der Zeit; die Anteile unten nur von der Arbeit")
    print()
    print("Bereiche (grob, nach Quelldatei, sonst Funktionsnamen):")
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
    if per_line:
        print()
        print("Zeilen (innerste Funktion mit Datei:Zeile, die 40 groessten):")
        for (lib, name, where), n in sorted(per_line.items(), key=lambda t: -t[1])[:40]:
            print(f"  {100.0 * n / total:5.1f}%  {where[-60:]:60s} {name[:70]}")
    if missing:
        print()
        print("Ohne Bibliothek mit Symbolen: " + ", ".join(sorted(missing)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
