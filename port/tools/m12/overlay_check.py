#!/usr/bin/env python3
"""Milestone 12, way A: works laid over fixed-size areas that grow too big on LP64.

The game keeps per-class state in fixed areas: an enemy's own work lies in
cEm::free (`#define EM31_WK(em) ((FREE_EM31*) (((cEm31*) (em))->free))`), an
object's in cObj*::free, the characters themselves in pool slots of
EM_WORK_SIZE bytes. On LP64 a work with pointers can outgrow its area.

With libclang this check parses every unit of an LP64 build and an ILP32
build (armhf) and reports
- casts of a byte-array member (`x->free`, `&x->work[0]`) to a pointer to a
  structure that is larger than the array on LP64 (but fits on ILP32),
- classes derived from a pool base (cEm, cObj, ...) larger than the pool
  slot on LP64 (slot sizes from --slot NAME=SIZE, default the LP64 sizes of
  EM_WORK_SIZE and OBJ_WORK_SIZE in port/whole/lp64_sizes.json).

    port/tools/m12/overlay_check.py [--ref BUILD] [--lp64 BUILD] [--pattern REGEX]

Needs the libclang Python bindings (pip install libclang==18.1.1).
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


def entries(build, pattern):
    cc = json.loads((Path(build) / "compile_commands.json").read_text())
    out = {}
    for e in cc:
        if re.search(pattern, e["file"]):
            out.setdefault(e["file"].split("/prepared/tree/")[-1], e)
    return out


def args_of(entry, resource_dir):
    args = shlex.split(entry["command"]) if "command" in entry else list(entry["arguments"])
    out, skip = [], False
    for a in args[1:]:
        if skip:
            skip = False
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a in ("-c", "-MD", "-MMD") or a == entry["file"]:
            continue
        out.append(a)
    return out + ["-resource-dir", resource_dir, "-w"]


def strip(c):
    import clang.cindex as ci
    while c is not None and c.kind in (ci.CursorKind.PAREN_EXPR, ci.CursorKind.UNEXPOSED_EXPR):
        kids = list(c.get_children())
        if len(kids) != 1:
            break
        c = kids[0]
    return c


def scan(job):
    import clang.cindex as ci
    entry, resource_dir = job
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    casts, classes = [], {}

    def bases(decl, seen):
        for b in decl.get_children():
            if b.kind == ci.CursorKind.CXX_BASE_SPECIFIER:
                d = b.type.get_declaration()
                if d.spelling not in seen:
                    seen.add(d.spelling)
                    bases(d, seen)
        return seen

    def walk(c):
        for k in c.get_children():
            if k.kind in (ci.CursorKind.CLASS_DECL, ci.CursorKind.STRUCT_DECL) and k.is_definition():
                size = k.type.get_size()
                if size > 0:
                    classes[k.spelling] = (size, sorted(bases(k, set())),
                                           f"{k.location.file.name if k.location.file else '?'}:{k.location.line}")
            if k.kind in (ci.CursorKind.CSTYLE_CAST_EXPR, ci.CursorKind.CXX_REINTERPRET_CAST_EXPR,
                          ci.CursorKind.CXX_STATIC_CAST_EXPR):
                t = k.type.get_canonical()
                if t.kind == ci.TypeKind.POINTER and t.get_pointee().get_canonical().kind == ci.TypeKind.RECORD:
                    kids = list(k.get_children())
                    src = strip(kids[-1]) if kids else None
                    if src is not None and src.kind == ci.CursorKind.UNARY_OPERATOR:
                        inner = [strip(x) for x in src.get_children()]
                        if inner and inner[0].kind == ci.CursorKind.ARRAY_SUBSCRIPT_EXPR:
                            src = strip(list(inner[0].get_children())[0])
                    if src is not None and src.kind == ci.CursorKind.MEMBER_REF_EXPR and \
                            src.type.get_canonical().kind == ci.TypeKind.CONSTANTARRAY:
                        rec = t.get_pointee().get_canonical()
                        loc = k.location
                        casts.append((rec.spelling, rec.get_size(), src.spelling, src.type.get_size(),
                                      f"{loc.file.name if loc.file else '?'}:{loc.line}"))
            walk(k)

    walk(tu.cursor)
    return casts, classes


def run(build_entries, names, resource_dir, jobs):
    with concurrent.futures.ProcessPoolExecutor(jobs) as pool:
        return dict(zip(names, pool.map(scan, [(build_entries[n], resource_dir) for n in names], chunksize=2)))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default=str(ROOT / "port/build/whole-armhf"))
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--pattern", default=r"/prepared/tree/src/(?!lib/).*\.cpp$")
    spec = json.loads((ROOT / "port/whole/lp64_sizes.json").read_text())
    ap.add_argument("--slot", action="append",
                    default=[f"{e['slot']}={e['lp64']}" for e in spec["sizes"] if "slot" in e])
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    ea, eb = entries(args.ref, args.pattern), entries(args.lp64, args.pattern)
    names = sorted(set(ea) & set(eb))
    ra, rb = run(ea, names, resource_dir, args.jobs), run(eb, names, resource_dir, args.jobs)
    bad = set()
    ref_sizes = {}
    for n in names:
        for rec, rsize, member, msize, where in ra[n][0]:
            ref_sizes[(rec, member, where.split("/prepared/tree/")[-1])] = (rsize, msize)
    for n in names:
        for rec, rsize, member, msize, where in rb[n][0]:
            w = where.split("/prepared/tree/")[-1]
            ref = ref_sizes.get((rec, member, w))
            if rsize > msize and (ref is None or ref[0] <= ref[1]):
                bad.add(f"{w}: {rec} ({rsize:#x} bytes on LP64"
                        f"{', %#x on ILP32' % ref[0] if ref else ''}) over {member} ({msize:#x} bytes)")
    slots = dict(s.split("=") for s in args.slot)
    seen = {}
    for n in names:
        for cls, (size, bases, where) in rb[n][1].items():
            seen.setdefault(cls, (size, bases, where))
    for cls, (size, bases, where) in sorted(seen.items()):
        for base, limit in slots.items():
            if (base in bases) and size > int(limit, 0):
                bad.add(f"{where.split('/prepared/tree/')[-1]}: class {cls} ({size:#x} bytes on LP64) over a "
                        f"{base} slot ({int(limit, 0):#x})")
    print(f"overlay_check: {len(names)} units, {sum(len(rb[n][0]) for n in names)} casts of byte areas, "
          f"{len(seen)} classes, {len(bad)} that do not fit on LP64")
    for line in sorted(bad):
        print("  " + line)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
