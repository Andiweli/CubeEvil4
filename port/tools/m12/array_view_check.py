#!/usr/bin/env python3
"""Milestone 12, way A: member arrays indexed past their end.

Decompiled classes cover unknown bytes with filler arrays (`u8 pad_EC[8]`),
and the code now and then reaches what follows through them:
`(Esp1bSpWork*) &esp->pad_EC[0xC]` is the GameCube offset 0xF8, the first
byte of the derived class's own work (cEsp1b::m_Free). On ILP32 that holds;
on LP64 the members in front of the filler grow and the derived work starts
elsewhere (tail padding: 0x10C instead of 0x110), so the index lands on
other bytes (first 64-bit run into r120 on the Retroid, 17.10.2026: the
spline sprite drew garbage, the GX stream lost its place and the game hung).

With libclang this check parses the units of an ILP32 build (armhf, the
reference) and an LP64 build and lists every subscript `a.m[k]` and every
`a.m + k` of a member array m with a constant k at or past its length (or
negative), with the byte offset in the holder on both builds.

    port/tools/m12/array_view_check.py [--ref BUILD] [--lp64 BUILD] [--pattern REGEX]

Needs the libclang Python bindings (pip install libclang==18.1.1).
"""

import argparse
import concurrent.futures
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from raw_offset_check import args_of, const_value, entries, strip  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]


def member_array(c):
    """(holder record, field, element size, length) when c names a member array"""
    import clang.cindex as ci
    c = strip(c)
    if c is None or c.kind != ci.CursorKind.MEMBER_REF_EXPR:
        return None
    field = c.referenced
    if field is None or field.kind != ci.CursorKind.FIELD_DECL or field.get_field_offsetof() < 0:
        return None
    t = field.type.get_canonical()
    if t.kind != ci.TypeKind.CONSTANTARRAY:
        return None
    holder = field.semantic_parent.type.get_canonical()
    return holder, field, t.get_array_element_type().get_size(), t.get_array_size()


def scan(job):
    import clang.cindex as ci
    entry, resource_dir = job
    tree = entry["file"].split("/prepared/tree/")[0] + "/prepared/tree/"
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    found = []

    def report(k, arr, index, how):
        holder, field, es, n = arr
        if 0 <= index < n:
            return
        loc = k.extent.start
        path = loc.file.name if loc.file else ""
        if not path.startswith(tree):
            return
        off = field.get_field_offsetof() // 8 + index * es
        found.append((path[len(tree):], loc.line, loc.column, f"{holder.spelling}.{field.spelling}", index, n,
                      how, off, holder.get_size()))

    def walk(c):
        for k in c.get_children():
            if k.kind == ci.CursorKind.ARRAY_SUBSCRIPT_EXPR:
                kids = list(k.get_children())
                if len(kids) == 2:
                    arr = member_array(kids[0])
                    index = const_value(kids[1]) if arr else None
                    if arr and index is not None:
                        report(k, arr, index, "[]")
            elif k.kind == ci.CursorKind.BINARY_OPERATOR:
                kids = list(k.get_children())
                toks = [t.spelling for t in k.get_tokens()]
                if len(kids) == 2 and "+" in toks:
                    arr = member_array(kids[0])
                    index = const_value(kids[1]) if arr else None
                    if arr and index is not None and strip(kids[0]).extent.end.offset < kids[1].extent.start.offset:
                        between = [t.spelling for t in k.get_tokens()
                                   if strip(kids[0]).extent.end.offset <= t.extent.start.offset
                                   < kids[1].extent.start.offset]
                        if between[:1] == ["+"]:
                            report(k, arr, index, "+")
            walk(k)

    walk(tu.cursor)
    return found


def collect(build, pattern, jobs, resource_dir):
    units = entries(build, pattern)
    sites = {}
    with concurrent.futures.ProcessPoolExecutor(jobs) as pool:
        for found in pool.map(scan, [(units[n], resource_dir) for n in sorted(units)], chunksize=2):
            for f in found:
                sites.setdefault(f[:3], f)
    return sites, len(units)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default=str(ROOT / "port/build/whole-armhf"))
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--pattern", default=r"/prepared/tree/src/.*\.(c|cpp)$")
    ap.add_argument("--jobs", type=int, default=8)
    args = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    ref, n = collect(args.ref, args.pattern, args.jobs, resource_dir)
    lp64, _ = collect(args.lp64, args.pattern, args.jobs, resource_dir)
    moved = 0
    lines = []
    for key in sorted(set(ref) | set(lp64)):
        a, b = ref.get(key), lp64.get(key)
        s = a or b
        ilp = f"{a[7]:#x} of {a[8]:#x}" if a else "-"
        lp = f"{b[7]:#x} of {b[8]:#x}" if b else "-"
        same = a and b and a[7] == b[7] and a[8] == b[8]
        moved += not same
        lines.append(f"  {s[0]}:{s[1]}:{s[2]}: {s[3]}{'[' if s[6] == '[]' else ' + '}{s[4]:#x}"
                     f"{']' if s[6] == '[]' else ''} (length {s[5]:#x}): byte {ilp} on ILP32, {lp} on LP64"
                     f"{'' if same else '  <- moves'}")
    print(f"array_view_check: {n} units, {len(lines)} member arrays indexed past their end, {moved} that move on LP64")
    for line in lines:
        print(line)
    return 1 if moved else 0


if __name__ == "__main__":
    sys.exit(main())
