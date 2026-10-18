#!/usr/bin/env python3
"""Milestone 12, way A: pointer members that the game relocates in place.

Data from the disc (and the memory card) holds offsets where the game later wants pointers;
the game turns them into pointers in place: `tpl->descriptorArray = (TEXDescriptor*)
((u32) tpl->descriptorArray + (u32) tpl)`, `p->tbl[i] = (T*) (base + (u32) p->tbl[i])`. The
member is the same before and after: an offset of 4 bytes in the file, a pointer afterwards.
On LP64 such a member must keep 4 bytes (port/whole/fixed_form.json, RE4_P32), or the structure
no longer lies on the data as the disc has it: every member behind the first pointer moves.

With libclang this check parses the units of an LP64 build and lists every assignment to a
member (or an element of a member array) of pointer type whose new value is computed from the
same member read as a number (`(u32) p->m`): the relocation of an offset. Members that are 8-byte pointers on LP64 are reported
(fixed-form members are a class, Re4Ptr32, not a pointer, and do not show).

    port/tools/m12/reloc_check.py [--lp64 BUILD] [--pattern REGEX]

Needs the libclang Python bindings (pip install libclang==18.1.1).
"""

import argparse
import concurrent.futures
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from raw_offset_check import args_of, binary_op, descendants, entries, strip  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]


def member_of(c):
    """the FIELD_DECL an lvalue names: `x.m`, `p->m`, `p->m[i]`"""
    import clang.cindex as ci
    c = strip(c)
    if c is not None and c.kind == ci.CursorKind.ARRAY_SUBSCRIPT_EXPR:
        kids = list(c.get_children())
        c = strip(kids[0]) if kids else None
    if c is None or c.kind != ci.CursorKind.MEMBER_REF_EXPR:
        return None
    f = c.referenced
    return f if f is not None and f.kind == ci.CursorKind.FIELD_DECL else None


def scan(job):
    import clang.cindex as ci
    entry, resource_dir = job
    tree = entry["file"].split("/prepared/tree/")[0] + "/prepared/tree/"
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    found = []

    def walk(c):
        for k in c.get_children():
            if k.kind == ci.CursorKind.BINARY_OPERATOR and binary_op(k) == "=":
                kids = list(k.get_children())
                if len(kids) == 2:
                    f = member_of(kids[0])
                    t = kids[0].type.get_canonical()
                    if f is not None and t.kind == ci.TypeKind.POINTER:
                        usr = f.get_usr()
                        # the member read as a number (`(u32) p->m`): an offset turned into a pointer;
                        # `p->m + n` or `(u8*) p->m + n` only moves a pointer it already holds
                        reads = [x for x in descendants(kids[1]) if x.kind in (
                                 ci.CursorKind.CSTYLE_CAST_EXPR, ci.CursorKind.CXX_REINTERPRET_CAST_EXPR,
                                 ci.CursorKind.CXX_FUNCTIONAL_CAST_EXPR) and
                                 x.type.get_canonical().kind in (ci.TypeKind.UINT, ci.TypeKind.INT,
                                                                 ci.TypeKind.ULONG, ci.TypeKind.LONG) and
                                 any(y.kind == ci.CursorKind.MEMBER_REF_EXPR and y.referenced is not None and
                                     y.referenced.get_usr() == usr for y in descendants(x))]
                        adds = [x for x in descendants(kids[1]) if x.kind == ci.CursorKind.BINARY_OPERATOR and
                                binary_op(x) == "+"]
                        # or the member (as a pointer) plus an address as a number: `(u8*) p->m + (u32) base`
                        if not reads:
                            same = any(y.kind == ci.CursorKind.MEMBER_REF_EXPR and y.referenced is not None and
                                       y.referenced.get_usr() == usr for y in descendants(kids[1]))
                            base = [x for x in descendants(kids[1]) if x.kind == ci.CursorKind.CSTYLE_CAST_EXPR and
                                    x.type.get_canonical().kind in (ci.TypeKind.UINT, ci.TypeKind.ULONG) and
                                    any(z.type.get_canonical().kind == ci.TypeKind.POINTER and
                                        z.kind == ci.CursorKind.DECL_REF_EXPR for z in descendants(x))]
                            if same and base:
                                reads = base
                        if reads and adds:
                            loc = k.extent.start
                            path = loc.file.name if loc.file else ""
                            if path.startswith(tree):
                                holder = f.semantic_parent.spelling if f.semantic_parent is not None else "?"
                                found.append((path[len(tree):], loc.line, loc.column, holder, f.spelling,
                                              t.get_size()))
            walk(k)

    walk(tu.cursor)
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--pattern", default=r"/prepared/tree/src/.*\.(c|cpp)$")
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    units = entries(args.lp64, args.pattern)
    sites = {}
    with concurrent.futures.ProcessPoolExecutor(args.jobs) as pool:
        for found in pool.map(scan, [(units[n], resource_dir) for n in sorted(units)], chunksize=2):
            for s in found:
                sites.setdefault(s[:3], s)
    bad = [s for s in sites.values() if s[5] == 8]
    members = sorted({f"{s[3]}::{s[4]}" for s in bad})
    print(f"reloc_check: {len(units)} units, {len(sites)} pointer members relocated in place, "
          f"{len(bad)} places with 8-byte pointers on LP64 ({len(members)} members)")
    for s in sorted(bad):
        print(f"  {s[0]}:{s[1]}:{s[2]}: {s[3]}::{s[4]} relocated in place, 8 bytes on LP64")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
