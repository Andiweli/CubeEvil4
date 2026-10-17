#!/usr/bin/env python3
"""Milestone 12, way A: signed 32-bit values turned into pointers.

The game keeps addresses in 32-bit integers now and then (`s32 prim_cnt =
(s32) MEM_ALLOC(...)`, `int` results and parameters) and turns them back into
pointers with a cast. Everything it reaches lies in 0x80000000..0xFFFFFFFF, so
bit 31 is set: on ILP32 the cast keeps the 32 bits, on LP64 a signed value is
sign-extended to 0xFFFFFFFF8xxxxxxx (first 64-bit run on the Retroid,
17.10.2026: memset in primInit on 0xffffffff80d427c0).

With libclang this check parses the units of an LP64 build and lists every
explicit cast to a pointer whose operand has a signed integer type of at most
32 bits, except constants that are not negative (they mean the same either
way). Each such operand is to be read as unsigned (RE4_ADDR32 in
re4_whole_prelude.h), which gives the ILP32 bit pattern; the reviewed list is
port/whole/lp64_int_casts.json, applied by whole_prepare.py (rule
lp64_int_casts). Casts inside macro expansions are reported separately (the
macro has to be changed by hand).

    port/tools/m12/int_cast_check.py [--lp64 BUILD] [--add] [--pattern REGEX]

Without --add it reports what is left (0 once the list covers everything);
--add appends what it finds to the list. Needs the libclang Python bindings
(pip install libclang==18.1.1).
"""

import argparse
import concurrent.futures
import json
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from raw_offset_check import args_of, const_value, entries  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]
LIST = ROOT / "port" / "whole" / "lp64_int_casts.json"
CASTS = ("CSTYLE_CAST_EXPR", "CXX_REINTERPRET_CAST_EXPR", "CXX_FUNCTIONAL_CAST_EXPR")


def signed_small(t):
    import clang.cindex as ci
    t = t.get_canonical()
    if t.kind == ci.TypeKind.ENUM:
        t = t.get_declaration().enum_type.get_canonical()
    return t.kind in (ci.TypeKind.INT, ci.TypeKind.SHORT, ci.TypeKind.SCHAR, ci.TypeKind.CHAR_S) \
        and t.get_size() <= 4


def text_of(data, extent):
    return data[extent.start.offset:extent.end.offset].decode("utf-8", "surrogateescape")


def scan(job):
    """(file, line, col, text, operand, type, macro) of each signed cast in one unit"""
    import clang.cindex as ci
    entry, resource_dir = job
    tree = entry["file"].split("/prepared/tree/")[0] + "/prepared/tree/"
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    files, found = {}, []

    def source(path):
        if path not in files:
            files[path] = Path(path).read_bytes()
        return files[path]

    def walk(c):
        for k in c.get_children():
            if k.kind.name in CASTS and k.type.get_canonical().kind == ci.TypeKind.POINTER:
                kids = [x for x in k.get_children() if x.kind != ci.CursorKind.TYPE_REF]
                op = kids[-1] if kids else None
                if op is not None and signed_small(op.type):
                    value = const_value(op)
                    loc = k.extent.start
                    path = loc.file.name if loc.file else ""
                    if (value is None or value < 0) and path.startswith(tree) and "/src/lib/" not in path:
                        data = source(path)
                        text = text_of(data, k.extent)
                        operand = text_of(data, op.extent)
                        macro = not (text.startswith("(") or text.startswith("reinterpret_cast")) \
                            or not text.endswith(operand) or "\n" in text or op.extent.start.offset < loc.offset
                        found.append((path[len(tree):], loc.line, loc.column, text, operand,
                                      op.type.spelling, macro))
            walk(k)

    walk(tu.cursor)
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--pattern", default=r"/prepared/tree/src/.*\.(c|cpp)$")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--add", action="store_true", help="append the casts found to the list")
    args = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    units = entries(args.lp64, args.pattern)
    sites = {}
    with concurrent.futures.ProcessPoolExecutor(args.jobs) as pool:
        for found in pool.map(scan, [(units[n], resource_dir) for n in sorted(units)], chunksize=2):
            for f in found:
                sites.setdefault(f[:3], f)
    plain = [s for s in sites.values() if not s[6]]
    macros = [s for s in sites.values() if s[6]]
    print(f"int_cast_check: {len(units)} units, {len(sites)} casts of signed 32-bit values to pointers "
          f"({len(macros)} inside macros)")
    for s in sorted(sites.values()):
        print(f"  {s[0]}:{s[1]}:{s[2]}: `{s[3]}` ({s[5]}){' [macro]' if s[6] else ''}")
    if args.add and plain:
        listed = json.loads(LIST.read_text()) if LIST.exists() else []
        known = {(e["file"], e["line"], e["text"]) for e in listed}
        for f, line, col, text, operand, typ, _ in sorted(plain):
            if (f, line, text) not in known:
                listed.append({"file": f, "line": line, "text": text, "operand": operand, "type": typ,
                               "action": "zero_extend"})
        LIST.write_text(json.dumps(listed, indent=1) + "\n")
        print(f"list: {len(listed)} entries in {LIST.relative_to(ROOT)}")
    return 1 if sites else 0


if __name__ == "__main__":
    sys.exit(main())
