#!/usr/bin/env python3
"""Milestone 12, way A: pointers read or written as 32-bit words.

Decompiled code sometimes reaches a pointer through a word view:
`*(u32*) &wk->next = x`, `((s32*) &p)[0]`, `*(u32*) ptr_array`. On ILP32 the
word is the whole pointer. On LP64 the pointer has 8 bytes: a read gets the
low half (right while everything lies below 4 GB), but a write leaves the high
half as it was, so a pointer over memory that held something else turns into
a wild address.

With libclang this check parses the units of an LP64 build and lists every
dereference (`*`, `[]`) of a cast to a pointer to a 4-byte (or smaller)
integer whose operand is the address of an object of pointer type, or an array
of pointers; reads and writes are told apart. Pointer members of the
fixed-form structures (RE4_P32, 4 bytes on LP64) are no pointer type here and
are not listed.

    port/tools/m12/ptr_word_check.py [--lp64 BUILD] [--pattern REGEX]

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


def pointer_object(op):
    """the operand points at an object of pointer type (`&p`, an array of pointers): its type"""
    import clang.cindex as ci
    op = strip(op)
    if op is None:
        return None
    if op.kind == ci.CursorKind.UNARY_OPERATOR:
        toks = [t.spelling for t in op.get_tokens()]
        kids = list(op.get_children())
        if toks[:1] == ["&"] and kids:
            t = kids[0].type.get_canonical()
            return kids[0].type.spelling if t.kind in (ci.TypeKind.POINTER, ci.TypeKind.MEMBERPOINTER) else None
    t = op.type.get_canonical()
    if t.kind in (ci.TypeKind.CONSTANTARRAY, ci.TypeKind.INCOMPLETEARRAY):
        e = t.get_array_element_type().get_canonical()
        return op.type.spelling if e.kind == ci.TypeKind.POINTER else None
    if t.kind == ci.TypeKind.POINTER and t.get_pointee().get_canonical().kind == ci.TypeKind.POINTER:
        return op.type.spelling  # a T** seen as words
    return None


def scan(job):
    import clang.cindex as ci
    entry, resource_dir = job
    tree = entry["file"].split("/prepared/tree/")[0] + "/prepared/tree/"
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    found = []

    def word_view(c):
        c = strip(c)
        if c is None or c.kind.name not in CASTS:
            return None
        t = c.type.get_canonical()
        if t.kind != ci.TypeKind.POINTER:
            return None
        p = t.get_pointee().get_canonical()
        if p.kind not in (ci.TypeKind.INT, ci.TypeKind.UINT, ci.TypeKind.LONG, ci.TypeKind.ULONG) or p.get_size() != 4:
            return None
        kids = [x for x in c.get_children() if x.kind != ci.CursorKind.TYPE_REF]
        return pointer_object(kids[-1]) if kids else None

    def walk(c, parent):
        for k in c.get_children():
            target = None
            if k.kind == ci.CursorKind.UNARY_OPERATOR:
                toks = [t.spelling for t in k.get_tokens()][:1]
                kids = list(k.get_children())
                if toks == ["*"] and kids:
                    target = word_view(kids[0])
            elif k.kind == ci.CursorKind.ARRAY_SUBSCRIPT_EXPR:
                kids = list(k.get_children())
                if kids:
                    target = word_view(kids[0])
            if target:
                loc = k.extent.start
                path = loc.file.name if loc.file else ""
                if path.startswith(tree):
                    write = parent is not None and parent.kind in (
                        ci.CursorKind.BINARY_OPERATOR, ci.CursorKind.COMPOUND_ASSIGNMENT_OPERATOR) and \
                        list(parent.get_children())[0] == k
                    found.append((path[len(tree):], loc.line, loc.column, target, "write" if write else "read"))
            walk(k, k)

    walk(tu.cursor, None)
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
    writes = [s for s in sites.values() if s[4] == "write"]
    print(f"ptr_word_check: {len(units)} units, {len(sites)} word views of pointers ({len(writes)} writes)")
    for s in sorted(sites.values()):
        print(f"  {s[0]}:{s[1]}:{s[2]}: {s[4]} of `{s[3]}` as a 32-bit word")
    return 1 if writes else 0


if __name__ == "__main__":
    sys.exit(main())
