#!/usr/bin/env python3
"""Milestone 12, inventory: which game structures change size on another target.

Runs clang -fsyntax-only with -fdump-record-layouts-simple over every game unit
of a whole-program build (compile_commands.json of port/build/whole-armhf: the
prepared tree with all rewrites and patches) for two targets and compares the
size of every structure both dumps contain.

    port/tools/m12/layout_compare.py [build] [--targets A,B] [--show NAME,...]

Default targets: arm-linux-gnueabihf (the device build today) and
x86_64-linux-gnu (LP64, stands in for arm64-v8a). wasm32-wasi needs the wasi
sysroot (sudo apt-get install wasi-libc).
"""

import argparse
import concurrent.futures
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
REC = re.compile(r"^Type: (?:struct|class|union) (.+)$")
SIZE = re.compile(r"^\s*Size:(\d+)")


def command(entry, target):
    args = shlex.split(entry["command"]) if "command" in entry else list(entry["arguments"])
    out, skip = [], False
    for a in args:
        if skip:
            skip = False
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a in ("-c", "-MD", "-MMD") or a.startswith("--target="):
            continue
        out.append(a)
    return [out[0], "--target=" + target, "-fsyntax-only", "-Xclang", "-fdump-record-layouts-simple",
            "-ferror-limit=0"] + out[1:]


def sizes(entry, target):
    p = subprocess.run(command(entry, target), cwd=entry.get("directory", "."), capture_output=True, text=True)
    res, name = {}, None
    for line in p.stdout.splitlines():
        m = REC.match(line)
        if m:
            name = m.group(1)
            continue
        m = SIZE.match(line)
        if m and name is not None:
            res.setdefault(name, int(m.group(1)) // 8)
            name = None
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("build", nargs="?", default=str(ROOT / "port/build/whole-armhf"))
    ap.add_argument("--targets", default="arm-linux-gnueabihf,x86_64-linux-gnu")
    ap.add_argument("--show", default="GLOBAL_WK,cEm,cPlayer,cObj,cDmg,cModel,cModelInfo,SAVE_DATA_HEAD,"
                    "TEXPalette,cSmd,VibDataTbl,OSThread,DVDFileInfo")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--out", default=None)
    args = ap.parse_args()
    a_t, b_t = args.targets.split(",")
    cc = json.loads((Path(args.build) / "compile_commands.json").read_text())
    seen, units = set(), []
    for e in cc:
        f = e["file"]
        if f in seen:
            continue
        seen.add(f)
        if "/prepared/tree/src/" in f and "/src/lib/" not in f and f.endswith(".cpp"):
            units.append(e)
    result = {}
    for t in (a_t, b_t):
        merged = {}
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
            for r in pool.map(lambda e: sizes(e, t), units):
                for k, v in r.items():
                    merged.setdefault(k, v)
        result[t] = merged
    a, b = result[a_t], result[b_t]
    common = [k for k in a if k in b]
    changed = [k for k in common if a[k] != b[k]]
    print("%d game units; %d structures with a layout on both targets; %d change size (%s -> %s)" % (
        len(units), len(common), len(changed), a_t, b_t))
    for n in args.show.split(","):
        if n in a or n in b:
            print("  %-18s %6s -> %6s" % (n, a.get(n, "-"), b.get(n, "-")))
    if args.out:
        Path(args.out).write_text(json.dumps(result))
    return 0


if __name__ == "__main__":
    sys.exit(main())
