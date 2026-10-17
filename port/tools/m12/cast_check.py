#!/usr/bin/env python3
"""Milestone 12, way A: one object seen through two structure types.

C code of the middleware turns a pointer to one structure into a pointer to
another (sfd.h: `#define SFD_TIM(sfd) ((SFTIM)&(sfd)->con)`, the timing work
over the concatenation work SFCON). On ILP32 both views put each member
where the other expects it; on LP64 the view with pointers moves its members.

With libclang this check reads the C units of an ILP32 build (armhf) and an
LP64 build, collects casts from a pointer to a structure A to a pointer to a
different structure B, and compares A and B by their GameCube offsets: a
member start both have on ILP32 must lie at the same LP64 offset, and B must
not end beyond A on LP64 when it fits on ILP32.

    port/tools/m12/cast_check.py [--ref BUILD] [--lp64 BUILD] [--pattern REGEX]

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


def starts(t, base=0, out=None, depth=0, prefix=""):
    """{offset: member path} of every member start (nested records included, as "tot.val": leaf
    names repeat in nested records)"""
    import clang.cindex as ci
    out = {} if out is None else out
    for f in t.get_declaration().get_children():
        if f.kind != ci.CursorKind.FIELD_DECL or f.get_field_offsetof() < 0:
            continue
        off = base + f.get_field_offsetof() // 8
        path = prefix + f.spelling
        out.setdefault(off, path)
        ft = f.type.get_canonical()
        if ft.kind == ci.TypeKind.RECORD and depth < 3:
            starts(ft, off, out, depth + 1, path + ".")
    return out


def scan(job):
    import clang.cindex as ci
    entry, resource_dir = job
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    found = {}

    def rec(t):
        t = t.get_canonical()
        if t.kind == ci.TypeKind.POINTER:
            p = t.get_pointee().get_canonical()
            if p.kind == ci.TypeKind.RECORD and p.get_size() > 0:
                return p
        return None

    def member_view(src):
        """`&x.m` / `&p->m`: (record holding m, m, its offset in bytes) - a view laid over the rest of
        that record from m on (`(GxWork*) &pG->gxStage`)"""
        if src is None or src.kind != ci.CursorKind.UNARY_OPERATOR or \
                [t.spelling for t in src.get_tokens()][:1] != ["&"]:
            return None
        kids = [strip(x) for x in src.get_children()]
        if not kids or kids[0] is None or kids[0].kind != ci.CursorKind.MEMBER_REF_EXPR:
            return None
        field = kids[0].referenced
        if field is None or field.kind != ci.CursorKind.FIELD_DECL or field.get_field_offsetof() < 0:
            return None
        holder = field.semantic_parent.type.get_canonical()
        return holder, field.spelling, field.get_field_offsetof() // 8

    def walk(c):
        for k in c.get_children():
            if k.kind == ci.CursorKind.CSTYLE_CAST_EXPR:
                b = rec(k.type)
                kids = list(k.get_children())
                src = strip(kids[-1]) if kids else None
                a = rec(src.type) if src is not None else None
                view = member_view(src)
                loc = k.location
                where = f"{loc.file.name if loc.file else '?'}:{loc.line}"
                if view is not None and b is not None and view[0].spelling != b.spelling:
                    holder, member, base = view
                    key = (f"{holder.spelling}.{member}", b.spelling)
                    if key not in found:
                        found[key] = (holder.get_size(), starts(holder), b.get_size(), starts(b), where, base)
                elif a is not None and b is not None and a.spelling != b.spelling:
                    key = (a.spelling, b.spelling)
                    if key not in found:
                        found[key] = (a.get_size(), starts(a), b.get_size(), starts(b), where, 0)
            walk(k)

    walk(tu.cursor)
    return found


def collect(build, pattern, jobs_n, resource_dir):
    cc = json.loads((Path(build) / "compile_commands.json").read_text())
    seen, jobs = set(), []
    for e in cc:
        if e["file"] in seen or not re.search(pattern, e["file"]):
            continue
        seen.add(e["file"])
        jobs.append((e, resource_dir))
    out = {}
    with concurrent.futures.ProcessPoolExecutor(jobs_n) as pool:
        for res in pool.map(scan, jobs, chunksize=4):
            for k, v in res.items():
                out.setdefault(k, v)
    return out, len(jobs)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default=str(ROOT / "port/build/whole-armhf"))
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--pattern", default=r"/prepared/tree/src/lib/.*\.c$")
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    ra, n = collect(args.ref, args.pattern, args.jobs, resource_dir)
    rb, _ = collect(args.lp64, args.pattern, args.jobs, resource_dir)
    bad = []
    for key, (asz, astarts, bsz, bstarts, where, base) in sorted(ra.items()):
        if key not in rb:
            continue
        asz2, astarts2, bsz2, bstarts2, _, base2 = rb[key]
        # a view over a member and what follows it: its offsets in the holder's coordinates
        bstarts = {base + o: n for o, n in bstarts.items()}
        bstarts2 = {base2 + o: n for o, n in bstarts2.items()}
        # GameCube offset -> LP64 offset in each view, by member path
        amap = {}
        inv_a = {v: k for k, v in astarts2.items()}
        for off, name in astarts.items():
            if name in inv_a:
                amap[off] = inv_a[name]
        bmap = {}
        inv_b = {v: k for k, v in bstarts2.items()}
        for off, name in bstarts.items():
            if name in inv_b:
                bmap[off] = inv_b[name]
        clash = sorted(o for o in set(amap) & set(bmap) if amap[o] != bmap[o])
        grow = base + bsz <= asz and base2 + bsz2 > asz2
        if clash or grow:
            o = clash[0] if clash else None
            bad.append(f"{key[0]} -> {key[1]} ({where.split('/prepared/tree/')[-1]}): " +
                       (f"GameCube {o:#x} at LP64 {amap[o]:#x} / {bmap[o]:#x} ({astarts[o]} / {bstarts[o]})"
                        if clash else "") +
                       (f"; {key[1]} {bsz2:#x} > {key[0]} {asz2:#x} bytes on LP64" if grow else ""))
    print(f"cast_check: {n} units, {len(rb)} casts between structures, {len(bad)} that disagree on LP64")
    for line in bad:
        print("  " + line)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
