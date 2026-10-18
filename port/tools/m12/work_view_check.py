#!/usr/bin/env python3
"""Milestone 12, way A: work areas kept as void* / byte pointers, read at GameCube offsets.

The game keeps many works behind an untyped pointer (`void* pWork`, `u8* work`, an address in a
u32): one place lays a structure over it (`W* w = (W*) em->pWork;`), another reads it at a fixed
byte offset (`*(s32*) ((u8*) em->pWork + 0x24)`, `((u32*) em->pWork)[9]`). raw_offset_check
sees the offset only when the pointer is a structure pointer; here it is not, so the two places
are tied by the declaration of the pointer (a member or a variable, the same in every unit).

With libclang this check parses the units of an ILP32 build (armhf, the reference) and an LP64
build and collects, per untyped pointer declaration,
  - the structures it is cast to (anywhere in the program),
  - the constant offsets the code adds to it (byte, word and integer views, subscripts),
and reports an offset where a member of one of those structures starts or lies on ILP32 but
which lands on another place of it on LP64.

    port/tools/m12/work_view_check.py [--ref BUILD] [--lp64 BUILD] [--pattern REGEX]

Needs the libclang Python bindings (pip install libclang==18.1.1).
"""

import argparse
import collections
import concurrent.futures
import os
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from raw_offset_check import (WORD_KINDS, args_of, binary_op, const_terms, const_value, entries,  # noqa: E402
                              field_map, strip)

ROOT = Path(__file__).resolve().parents[3]
CASTS = ("CSTYLE_CAST_EXPR", "CXX_REINTERPRET_CAST_EXPR", "CXX_STATIC_CAST_EXPR")


def untyped(t):
    """void*, a byte pointer or a 32/64-bit integer (an address kept as a number)"""
    import clang.cindex as ci
    t = t.get_canonical()
    K = ci.TypeKind
    if t.kind == K.POINTER:
        return t.get_pointee().get_canonical().kind in (K.VOID, K.CHAR_S, K.CHAR_U, K.SCHAR, K.UCHAR)
    return t.kind in (K.UINT, K.ULONG, K.INT, K.LONG) and t.get_size() >= 4


def holder_decl(c):
    """the member or variable an expression names (`em->pWork`, `work`), when it is untyped"""
    import clang.cindex as ci
    c = strip(c)
    while c is not None and c.kind.name in CASTS:
        kids = [x for x in c.get_children() if x.kind != ci.CursorKind.TYPE_REF]
        c = strip(kids[-1]) if kids else None
    if c is None or c.kind not in (ci.CursorKind.MEMBER_REF_EXPR, ci.CursorKind.DECL_REF_EXPR):
        return None
    d = c.referenced
    if d is None or d.kind not in (ci.CursorKind.FIELD_DECL, ci.CursorKind.VAR_DECL, ci.CursorKind.PARM_DECL):
        return None
    if not untyped(d.type):
        return None
    if d.kind == ci.CursorKind.PARM_DECL:
        return None  # a parameter is a new name in each function: no tie between places
    name = d.spelling
    if d.semantic_parent is not None and d.kind == ci.CursorKind.FIELD_DECL:
        name = f"{d.semantic_parent.spelling}::{d.spelling}"
    return d.get_usr(), name


def view_elem(c):
    """element size when c is a cast of the holder to a byte / word pointer or an integer, and the
    holder itself (when c is the untyped holder used as it is)"""
    import clang.cindex as ci
    c = strip(c)
    K = ci.TypeKind
    t = c.type.get_canonical()
    if t.kind == K.POINTER:
        p = t.get_pointee().get_canonical()
        if p.kind in (K.CHAR_S, K.CHAR_U, K.SCHAR, K.UCHAR, K.VOID):
            return 1
        if p.kind in WORD_KINDS() and p.get_size() in (2, 4, 8):
            return p.get_size()
        return None
    if t.kind in (K.UINT, K.ULONG, K.INT, K.LONG):
        return 1
    return None


def scan(job):
    import clang.cindex as ci
    entry, resource_dir = job
    tree = entry["file"].split("/prepared/tree/")[0] + "/prepared/tree/"
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    casts, offsets, layouts = [], [], {}

    def note_layout(rec):
        d = rec.get_declaration()
        usr = d.get_usr()
        if usr and usr not in layouts:
            layouts[usr] = (rec.get_size(), field_map(rec))
        return usr

    def offset(k, base, n):
        h = holder_decl(base)
        e = view_elem(base)
        if h is None or e is None or n is None or n <= 0:
            return
        loc = k.extent.start
        path = loc.file.name if loc.file else ""
        if path.startswith(tree):
            offsets.append((h[0], h[1], n * e, f"{path[len(tree):]}:{loc.line}"))

    def walk(c):
        for k in c.get_children():
            if k.kind.name in CASTS:
                t = k.type.get_canonical()
                if t.kind == ci.TypeKind.POINTER and t.get_pointee().get_canonical().kind == ci.TypeKind.RECORD:
                    rec = t.get_pointee().get_canonical()
                    kids = [x for x in k.get_children() if x.kind != ci.CursorKind.TYPE_REF]
                    h = holder_decl(kids[-1]) if kids else None
                    if h is not None and rec.get_size() > 0:
                        casts.append((h[0], h[1], note_layout(rec), rec.spelling))
            elif k.kind == ci.CursorKind.BINARY_OPERATOR and binary_op(k) in ("+", "-"):
                kids = list(k.get_children())
                if len(kids) == 2:
                    n = const_value(kids[1])
                    if n is None:
                        cst, var = const_terms(kids[1])
                        n = cst if var and cst else None
                    if n is not None and binary_op(k) == "-":
                        n = -n
                    offset(k, kids[0], n)
            elif k.kind == ci.CursorKind.ARRAY_SUBSCRIPT_EXPR:
                kids = list(k.get_children())
                if len(kids) == 2:
                    offset(k, kids[0], const_value(kids[1]))
            walk(k)

    walk(tu.cursor)
    return casts, offsets, layouts


def collect(build, pattern, jobs, resource_dir):
    units = entries(build, pattern)
    casts = collections.defaultdict(dict)    # holder usr -> {record usr: record name}
    names = {}
    offsets = collections.defaultdict(set)   # holder usr -> {(offset, where)}
    layouts = {}
    with concurrent.futures.ProcessPoolExecutor(jobs) as pool:
        for c, o, lay in pool.map(scan, [(units[n], resource_dir) for n in sorted(units)], chunksize=2):
            for husr, hname, rusr, rname in c:
                casts[husr][rusr] = rname
                names[husr] = hname
            for husr, hname, off, where in o:
                offsets[husr].add((off, where))
                names[husr] = hname
            for k, v in lay.items():
                layouts.setdefault(k, v)
    return casts, offsets, layouts, names, len(units)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default=str(ROOT / "port/build/whole-armhf"))
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--pattern", default=r"/prepared/tree/src/.*\.(c|cpp)$")
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    rc, ro, rl, rn, n = collect(args.ref, args.pattern, args.jobs, resource_dir)
    lc, lo, ll, ln, _ = collect(args.lp64, args.pattern, args.jobs, resource_dir)
    both = [h for h in rc if h in ro]
    bad = []
    pairs = 0
    for h in sorted(both, key=lambda u: rn.get(u, "")):
        for rusr, rname in sorted(rc[h].items(), key=lambda x: x[1]):
            if rusr not in rl or rusr not in ll:
                continue
            rsize, rfields = rl[rusr]
            lsize, lfields = ll[rusr]
            if rsize == lsize and rfields == lfields:
                continue
            for off, where in sorted(ro[h]):
                pairs += 1
                cover = [f for f in rfields if f[0] <= off < f[0] + max(f[1], 1)]
                if not cover:
                    continue  # past the structure or in a gap: another view of the work
                o, size, member = cover[-1]
                moved = [f for f in lfields if f[2] == member]
                if moved and moved[0][0] + (off - o) != off:
                    bad.append(f"  {where}: {rn.get(h, '?')} + {off:#x}: {rname}.{member} on ILP32, at "
                               f"{moved[0][0] + (off - o):#x} on LP64")
    print(f"work_view_check: {n} units, {len(both)} untyped pointers both cast to structures and read at "
          f"fixed offsets, {pairs} offset/structure pairs, {len(set(bad))} that land elsewhere on LP64")
    for line in sorted(set(bad)):
        print(line)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
