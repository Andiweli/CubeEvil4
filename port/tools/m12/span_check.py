#!/usr/bin/env python3
"""Milestone 12, way A: clears and copies that start at a member and span the ones behind it.

Decompiled code clears or copies runs of members with one call and a
GameCube length: `memclr_asm(&em->hp, 0x10)`, `memcpy(&w->Pos, src, 0x24)`.
On ILP32 the run covers the same members as on the GameCube. On LP64 a
pointer in the run is larger, or the members behind the first one move, and
the same length ends elsewhere: in the middle of a member, short of the last
one, or on members the GameCube never touched (a pointer's upper half).

With libclang this check parses the units of an ILP32 build (armhf, the
reference) and an LP64 build and lists every memclr_asm / memset / memcpy /
memmove / bzero with a constant length whose destination or source is a
member (`&x.m`, `x.m` for an array, `&x.m[k]`) and whose length runs past
that member, when the members the run covers (by name) or the place it ends
differ between the two builds.

    port/tools/m12/span_check.py [--ref BUILD] [--lp64 BUILD] [--pattern REGEX]

Needs the libclang Python bindings (pip install libclang==18.1.1).
"""

import argparse
import concurrent.futures
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from raw_offset_check import args_of, const_value, entries, reviewed_lp64, strip, unary_op  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]
CALLS = {"memclr_asm": (0, -1), "memset": (0, -1), "bzero": (0, -1), "memclr": (0, -1),
         "memcpy": (0, -1), "memmove": (0, -1), "memcpy_asm": (0, -1)}
SOURCES = {"memcpy": 1, "memmove": 1, "memcpy_asm": 1}


def fields_of(rec, base=0, out=None, depth=0, prefix=""):
    """[(offset, size, path)] of the leaf members of a record (nested records flattened, arrays as
    one member; members of base classes are not listed: a run stays in the holder's own members)"""
    import clang.cindex as ci
    out = [] if out is None else out
    decl = rec.get_declaration()
    for f in decl.get_children():
        if f.kind != ci.CursorKind.FIELD_DECL or f.get_field_offsetof() < 0:
            continue
        off = base + f.get_field_offsetof() // 8
        t = f.type.get_canonical()
        if t.kind == ci.TypeKind.RECORD and depth < 4 and t.get_size() > 0:
            fields_of(t, off, out, depth + 1, prefix + f.spelling + ".")
        else:
            out.append((off, max(t.get_size(), 0), prefix + f.spelling))
    return out


def member_start(c):
    """(holder record, offset of the start in it, size of the member) when c is `&x.m`, `x.m`
    (array), `&x.m[k]`"""
    import clang.cindex as ci
    c = strip(c)
    while c is not None and c.kind == ci.CursorKind.CSTYLE_CAST_EXPR:
        kids = [x for x in c.get_children() if x.kind != ci.CursorKind.TYPE_REF]
        c = strip(kids[-1]) if kids else None
    if c is None:
        return None
    index = 0
    if c.kind == ci.CursorKind.UNARY_OPERATOR and unary_op(c) == "&":
        kids = [strip(x) for x in c.get_children()]
        c = kids[0] if kids else None
        if c is not None and c.kind == ci.CursorKind.ARRAY_SUBSCRIPT_EXPR:
            ks = list(c.get_children())
            k = const_value(ks[1]) if len(ks) == 2 else None
            if k is None:
                return None
            elem = c.type.get_canonical().get_size()
            index = k * elem
            c = strip(ks[0])
    elif c.kind != ci.CursorKind.MEMBER_REF_EXPR or c.type.get_canonical().kind != ci.TypeKind.CONSTANTARRAY:
        return None
    if c is None or c.kind != ci.CursorKind.MEMBER_REF_EXPR:
        return None
    field = c.referenced
    if field is None or field.kind != ci.CursorKind.FIELD_DECL or field.get_field_offsetof() < 0:
        return None
    holder = field.semantic_parent.type.get_canonical()
    return holder, field.get_field_offsetof() // 8 + index, field.type.get_canonical().get_size() - index, \
        field.spelling


def scan(job):
    import clang.cindex as ci
    entry, resource_dir = job
    tree = entry["file"].split("/prepared/tree/")[0] + "/prepared/tree/"
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    found = []

    def walk(c):
        for k in c.get_children():
            if k.kind == ci.CursorKind.CALL_EXPR and k.spelling in CALLS:
                args = list(k.get_arguments())
                # a length given for LP64 (RE4_LP64_SIZE, lp64_sizes.json) is reviewed
                n = const_value(args[-1]) if len(args) >= 2 and not reviewed_lp64(args[-1]) else None
                if n is not None and n > 0:
                    for which in [0] + ([SOURCES[k.spelling]] if k.spelling in SOURCES else []):
                        m = member_start(args[which])
                        if m is None:
                            continue
                        holder, start, size, name = m
                        if n <= size:
                            continue
                        fields = fields_of(holder)
                        covered = [p for o, s, p in fields if start <= o < start + n]
                        ends = [p for o, s, p in fields if o < start + n < o + s]
                        if start + n > holder.get_size():
                            ends = ends + ["(past the end of " + holder.spelling + ")"]
                        loc = k.extent.start
                        path = loc.file.name if loc.file else ""
                        if path.startswith(tree):
                            found.append((path[len(tree):], loc.line, loc.column, which, holder.spelling, name,
                                          n, tuple(covered), tuple(ends), holder.get_size(), start))
            walk(k)

    walk(tu.cursor)
    return found


def collect(build, pattern, jobs, resource_dir):
    units = entries(build, pattern)
    sites = {}
    with concurrent.futures.ProcessPoolExecutor(jobs) as pool:
        for found in pool.map(scan, [(units[n], resource_dir) for n in sorted(units)], chunksize=2):
            for f in found:
                sites.setdefault(f[:4], f)
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
    bad = []
    for key in sorted(set(ref) | set(lp64)):
        a, b = ref.get(key), lp64.get(key)
        if a and b and a[7] == b[7] and a[8] == b[8]:
            continue
        s = a or b
        side = "destination" if s[3] == 0 else "source"
        def show(x):
            if not x:
                return "-"
            return (f"{len(x[7])} members {x[7][0]}..{x[7][-1]}" if x[7] else "no member") + \
                (f", ends inside {x[8][0]}" if x[8] else "")
        bad.append(f"  {s[0]}:{s[1]}:{s[2]}: {s[6]:#x} bytes from {s[4]}.{s[5]} ({side}): ILP32 {show(a)}; "
                   f"LP64 {show(b)}")
    print(f"span_check: {n} units, {len(set(ref) | set(lp64))} clears/copies over several members, "
          f"{len(bad)} that cover other members on LP64")
    for line in bad:
        print(line)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
