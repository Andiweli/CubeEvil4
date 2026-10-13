#!/usr/bin/env python3
"""Link-readiness report for the whole-program build.

Reads the object files of a built port/whole tree with nm and reports
  - unresolved symbols, grouped by the library that has to provide them
    (Nintendo SDK, CRI middleware, C/C++ runtime, game code),
  - symbols defined in more than one unit (original REL overlays each carried
    their own copy; one executable can hold only one).

Usage: whole_symbols.py BUILD_DIR [--json OUT]
"""
import argparse
import collections
import json
import re
import subprocess
import sys
from pathlib import Path

SDK = re.compile(r"^(DC|IC|C_QUAT|C_VEC|SEQ|SYN|TEX|GX|OS|AX|AXFX|AI|AR|ARQ|DVD|PAD|VI|CARD|DSP|EXI|SI|MIX|SP|PPC|PS|C_?MTX|MTX|VEC|"
                 r"__OS|__GX|__DVD|__PAD|__VI|__AX|DB|TRK|LC|GD|SN|THP|HIO|KM|MCC|FIO|__CARD|__DSP)")
CRI = re.compile(r"^(adx|ADX|sfd|SFD|mpv|MPV|cri|CRI|mwPly|mwSfd|mw|MW|sj|SJ|dvci|DVCI|ahx|AHX|svm|SVM|"
                 r"cvFs|CVFS|acs|ACS|adxt|ADXT|adxf|ADXF|xe|XE|sud|SUD|lsc|LSC|ads|ADS|PS2|ps2)", re.I)
RUNTIME = re.compile(r"^(_Z(nw|na|dl|da)|__cxa|__gxx|_Unwind|__dso|_ZTV(N10__cxxabiv)|_ZTI|__stack_chk|"
                     r"__assert|__errno|_GLOBAL_OFFSET|__x86|__aeabi)")


# The SN/GCC 2.95 toolchain runtime: linker-script symbols, old operator new/delete
# names, host-PC file I/O of the SN debugger, and the hand-written asm units
# (src/game/memset_2.cpp, src/game/yz2asm.cpp) whose bodies the prelude drops.
TOOLCHAIN = re.compile(r"^(_ctors|_dtors|__builtin_|__divdi3|__moddi3|__udivdi3|__umoddi3|PC(init|open|creat|close|read|write|lseek)$|"
                       r"memset_asm|memclr_asm|yz2Decode_)")
LIBC_DIRS = [Path("/usr/lib32"), Path("/usr/arm-linux-gnueabihf/lib"), Path("/lib/x86_64-linux-gnu")]


def libc_symbols() -> set:
    names = set()
    for d in LIBC_DIRS:
        for lib in ("libc.so.6", "libm.so.6"):
            if (d / lib).exists():
                out = subprocess.run(["nm", "-D", "--defined-only", str(d / lib)],
                                     capture_output=True, text=True).stdout
                names.update(line.split()[-1].split("@")[0] for line in out.splitlines() if line.strip())
        if names:
            break
    return names


def nm(obj: Path, *flags: str) -> list:
    out = subprocess.run(["nm", *flags, str(obj)], capture_output=True, text=True).stdout
    return [line.split() for line in out.splitlines() if line.strip()]


def demangle(names: list) -> dict:
    out = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True).stdout
    return dict(zip(names, out.splitlines()))


def classify(mangled: str, plain: str, libc: set) -> str:
    if mangled in libc or mangled.startswith("__isoc"):
        return "c library"
    if TOOLCHAIN.match(mangled):
        return "sn toolchain runtime"
    if RUNTIME.match(mangled) or plain.startswith("std::"):
        return "c/c++ runtime"
    if CRI.match(plain):
        return "cri middleware"
    if SDK.match(plain):
        return "nintendo sdk"
    return "game"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("build", type=Path)
    ap.add_argument("--json", type=Path)
    args = ap.parse_args()

    tree_marker = "/prepared/tree/"
    objects = sorted(args.build.glob("CMakeFiles/re4_game.dir/**/*.o"))
    if not objects:
        sys.exit(f"no objects under {args.build}")
    defined = collections.defaultdict(list)
    undefined = collections.defaultdict(set)
    for obj in objects:
        unit = str(obj).split(tree_marker, 1)[-1][:-2]
        for parts in nm(obj, "--defined-only", "-g"):
            if len(parts) == 3 and parts[1] in "TDBRVW":
                if parts[1] not in "VW":  # weak / vague linkage merges at link time
                    defined[parts[2]].append(unit)
        for parts in nm(obj, "-u"):
            undefined[parts[-1]].add(unit)

    all_defined = set(defined)
    missing = sorted(set(undefined) - all_defined)
    plain = demangle(missing)
    libc = libc_symbols()
    groups = collections.defaultdict(list)
    for name in missing:
        groups[classify(name, plain[name], libc)].append(plain[name])
    duplicates = {name: units for name, units in defined.items() if len(units) > 1}
    dup_plain = demangle(sorted(duplicates))

    print(f"objects: {len(objects)}")
    print(f"unresolved symbols: {len(missing)}")
    for group, names in sorted(groups.items(), key=lambda kv: -len(kv[1])):
        print(f"  {len(names):5d}  {group}")
    print(f"symbols defined in more than one unit: {len(duplicates)}")
    per_unit = collections.Counter(u for units in duplicates.values() for u in units)
    for unit, n in per_unit.most_common(10):
        print(f"  {n:5d}  {unit}")

    if args.json:
        args.json.write_text(json.dumps({
            "unresolved": {g: sorted(n) for g, n in groups.items()},
            "duplicates": {dup_plain[k]: v for k, v in sorted(duplicates.items())},
        }, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
