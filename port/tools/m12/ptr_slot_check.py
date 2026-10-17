#!/usr/bin/env python3
"""Milestone 12, way A: 32-bit variables handed out as pointer slots.

Decompiled code passes the address of a 4-byte integer where a function stores
a pointer: `int d; ReadTbl.GetDat((void**) &d, ...)` (EventMgr::GetRead). On
ILP32 the pointer fits; on LP64 the callee writes 8 bytes into the 4-byte
variable and overwrites what lies next to it (first 64-bit run into r120 on the
Retroid, 17.10.2026: "stack corruption detected" in EventMgr::EvtReadSub).

With libclang this check parses the units of an LP64 build and lists every
cast to a pointer to a pointer (`(T**) x`) whose operand points to an object
that is not a pointer and smaller than one (`&int_var`, an int array, a u32*).

    port/tools/m12/ptr_slot_check.py [--lp64 BUILD] [--pattern REGEX]

Needs the libclang Python bindings (pip install libclang==18.1.1).
"""

import argparse
import concurrent.futures
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from raw_offset_check import args_of, entries, strip  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]
CASTS = ("CSTYLE_CAST_EXPR", "CXX_REINTERPRET_CAST_EXPR")


def scan(job):
    import clang.cindex as ci
    entry, resource_dir = job
    tree = entry["file"].split("/prepared/tree/")[0] + "/prepared/tree/"
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    found = []

    def walk(c):
        for k in c.get_children():
            if k.kind.name in CASTS:
                t = k.type.get_canonical()
                if t.kind == ci.TypeKind.POINTER and t.get_pointee().get_canonical().kind == ci.TypeKind.POINTER:
                    kids = [x for x in k.get_children() if x.kind != ci.CursorKind.TYPE_REF]
                    op = strip(kids[-1]) if kids else None
                    src = op.type.get_canonical() if op is not None else None
                    if src is not None and src.kind in (ci.TypeKind.POINTER, ci.TypeKind.CONSTANTARRAY):
                        obj = (src.get_pointee() if src.kind == ci.TypeKind.POINTER
                               else src.get_array_element_type()).get_canonical()
                        if obj.kind not in (ci.TypeKind.POINTER, ci.TypeKind.VOID, ci.TypeKind.RECORD,
                                            ci.TypeKind.FUNCTIONPROTO) and 0 < obj.get_size() < 8:
                            loc = k.extent.start
                            path = loc.file.name if loc.file else ""
                            if path.startswith(tree):
                                found.append((path[len(tree):], loc.line, loc.column, obj.spelling,
                                              k.type.spelling))
            walk(k)

    walk(tu.cursor)
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--pattern", default=r"/prepared/tree/src/.*\.(c|cpp)$")
    ap.add_argument("--jobs", type=int, default=8)
    args = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    units = entries(args.lp64, args.pattern)
    sites = {}
    with concurrent.futures.ProcessPoolExecutor(args.jobs) as pool:
        for found in pool.map(scan, [(units[n], resource_dir) for n in sorted(units)], chunksize=2):
            for f in found:
                sites.setdefault(f[:3], f)
    print(f"ptr_slot_check: {len(units)} units, {len(sites)} 4-byte objects handed out as pointer slots")
    for s in sorted(sites.values()):
        print(f"  {s[0]}:{s[1]}:{s[2]}: {s[3]} seen as {s[4]}")
    return 1 if sites else 0


if __name__ == "__main__":
    sys.exit(main())
