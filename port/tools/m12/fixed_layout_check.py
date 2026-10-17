#!/usr/bin/env python3
"""Milestone 12, way A: do the structures of the format converters keep their form?

The format converters (port/whole/runtime/formats) turn disc data into host
byte order at the offsets of the game's own structure definitions
(offsetof/sizeof of cSmd, TEXHeader ...), and the game then lays those
structures over the converted data. On LP64 a pointer member makes a structure
larger and moves the members behind it: the converter and the game would both
read the disc data at the wrong places. Such structures have to keep their
GameCube form (port/whole/fixed_form.json).

This check compiles every converter unit for the armhf build (the reference:
ILP32 with 8-byte alignment of double and long long like the GameCube; i686
aligns them to 4) and an LP64 build, dumps the record layouts and compares,
for each structure the converter sources name, its size and the offsets of
its members.

    port/tools/m12/fixed_layout_check.py [--ref port/build/whole-armhf] [--lp64 port/build/whole-x86_64]

Exit code 1 when a named structure differs.
"""

import argparse
import concurrent.futures
import json
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
FORMATS = ROOT / "port" / "whole" / "runtime" / "formats"
ROW = re.compile(r"^\s*(\d+)(?::\d+-\d+)? \|( *)(.*)$")
SIZE = re.compile(r"\[sizeof=(\d+)")
IDENT = re.compile(r"\b[A-Za-z_]\w*\b")


def units(build):
    cc = json.loads((Path(build) / "compile_commands.json").read_text())
    out = {}
    for e in cc:
        f = e["file"]
        if "/runtime/formats/" in f and f.endswith(".cpp"):
            out.setdefault(Path(f).name, e)
    return out


def command(entry):
    args = shlex.split(entry["command"]) if "command" in entry else list(entry["arguments"])
    out, skip = [], False
    for a in args:
        if skip:
            skip = False
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a in ("-c", "-MD", "-MMD"):
            continue
        out.append(a)
    return [out[0], "-fsyntax-only", "-Xclang", "-fdump-record-layouts", "-w"] + out[1:]


ANON = re.compile(r"\((anonymous|unnamed)([^()]*?) at [^)]*\)")


def clean(text):
    """Unnamed members carry the path of the build tree: drop it."""
    return ANON.sub(r"(\1\2)", text)


def layouts(entry):
    """name -> (size, [(offset, member text)]) for the direct members."""
    p = subprocess.run(command(entry), cwd=entry.get("directory", "."), capture_output=True, text=True)
    res = {}
    lines = p.stdout.splitlines()
    i = 0
    while i < len(lines):
        if not lines[i].startswith("*** Dumping AST Record Layout"):
            i += 1
            continue
        i += 1
        m = ROW.match(lines[i]) if i < len(lines) else None
        if not m:
            continue
        name = clean(re.sub(r"^(struct|class|union) ", "", m.group(3).strip()))
        members, size = [], None
        i += 1
        while i < len(lines) and not lines[i].startswith("*** Dumping"):
            s = SIZE.search(lines[i])
            if s:
                size = int(s.group(1))
                break
            r = ROW.match(lines[i])
            if r and len(r.group(2)) == 3:  # direct member: "|   type name"
                # offset and member name: the type is spelled differently for
                # fixed-form members (Re4Ptr32<T> on LP64, T * on ILP32)
                text = clean(r.group(3).strip())
                ident = re.findall(r"\w+", text)
                members.append((int(r.group(1)), ident[-1] if ident else text))
            i += 1
        res.setdefault(name, (size, members))
    return res


def named_types(source):
    """Identifiers of the converter source, without the structures it defines
    itself (its own bookkeeping, not disc data)."""
    text = Path(source).read_text()
    text = re.sub(r"//[^\n]*|/\*.*?\*/", " ", text, flags=re.S)
    own = set(re.findall(r"\b(?:struct|class|union)\s+(\w+)\s*(?:\{|:)", text))
    return set(IDENT.findall(text)) - own


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default=str(ROOT / "port/build/whole-armhf"))
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    ref, lp = units(args.ref), units(args.lp64)
    common = sorted(set(ref) & set(lp))
    if not common:
        print("fixed_layout_check: no converter units in both builds")
        return 1
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        a = list(pool.map(lambda n: layouts(ref[n]), common))
        b = list(pool.map(lambda n: layouts(lp[n]), common))
    bad, checked = [], 0
    for name, la, lb in zip(common, a, b):
        wanted = named_types(FORMATS / name)
        for rec in sorted(wanted):
            if rec not in la or rec not in lb:
                continue
            checked += 1
            if la[rec] != lb[rec]:
                sa, ma = la[rec]
                sb, mb = lb[rec]
                diff = [f"{x[1]} {x[0]}->{y[0]}" for x, y in zip(ma, mb) if x[0] != y[0]][:4]
                bad.append(f"{name}: {rec} {sa} -> {sb} bytes" + (": " + ", ".join(diff) if diff else ""))
    seen = sorted(set(bad))
    print(f"fixed_layout_check: {len(common)} converter units, {checked} structure uses, "
          f"{len(seen)} with another form on LP64")
    for line in seen:
        print("  " + line)
    return 1 if seen else 0


if __name__ == "__main__":
    sys.exit(main())
