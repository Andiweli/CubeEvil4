#!/usr/bin/env python3
"""Milestone 12, way A: copies between structures of different form.

`memcpy(&cam, &evt->cam, sizeof(CAM))` copies one structure into another. When the two are
different structures that agree on ILP32 (a GameCube view of the other, a save copy, a work
copied from a table), the copy moves the members to the right places there; on LP64 a pointer
in one of them shifts the members behind it, and the copy puts them elsewhere. Copies between a
structure and a byte buffer move the structure's LP64 form into (or out of) the buffer: right
when only the same LP64 code reads the buffer again, wrong when the buffer holds GameCube data
(disc data, the memory card, a fixed area).

With libclang this check parses the units of an ILP32 build (armhf, the reference) and an LP64
build and lists every memcpy / memmove / memcpy_asm / struct copy through casts whose two sides
point to different structures that disagree on LP64 (a member start both have on ILP32 lies at
another place on LP64, as cast_check compares), and copies between a structure whose form
differs between the builds and a byte buffer ("buffer").

    port/tools/m12/copy_view_check.py [--ref BUILD] [--lp64 BUILD] [--pattern REGEX]

Needs the libclang Python bindings (pip install libclang==18.1.1).
"""

import argparse
import concurrent.futures
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from raw_offset_check import args_of, binary_op, entries, strip, unary_op  # noqa: E402
from cast_check import starts  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]
COPIES = {"memcpy": (0, 1), "memmove": (1, 0), "memcpy_asm": (0, 1), "bcopy": (1, 0)}  # (dest, source)


def pointee(c):
    """what a copy argument points to, behind casts to void* / byte pointers: (kind, record type)
    with kind "record", "buffer" (bytes) or None"""
    import clang.cindex as ci
    K = ci.TypeKind
    c = strip(c)
    while c is not None:
        t = c.type.get_canonical()
        if t.kind == K.POINTER:
            p = t.get_pointee().get_canonical()
            if p.kind == K.RECORD and p.get_size() > 0:
                return "record", p
            if p.kind not in (K.VOID, K.CHAR_S, K.CHAR_U, K.SCHAR, K.UCHAR):
                return None, None
        elif t.kind == K.CONSTANTARRAY:
            e = t.get_array_element_type().get_canonical()
            if e.kind == K.RECORD and e.get_size() > 0:
                return "record", e
            if e.kind in (K.CHAR_S, K.CHAR_U, K.SCHAR, K.UCHAR):
                return "buffer", None
            return None, None
        if c.kind in (ci.CursorKind.CSTYLE_CAST_EXPR, ci.CursorKind.CXX_REINTERPRET_CAST_EXPR,
                      ci.CursorKind.CXX_STATIC_CAST_EXPR, ci.CursorKind.UNEXPOSED_EXPR, ci.CursorKind.PAREN_EXPR):
            kids = [x for x in c.get_children() if x.kind != ci.CursorKind.TYPE_REF]
            c = strip(kids[-1]) if kids else None
            continue
        return "buffer", None
    return None, None


def scan(job):
    import clang.cindex as ci
    entry, resource_dir = job
    tree = entry["file"].split("/prepared/tree/")[0] + "/prepared/tree/"
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    found = {}

    def note(k, a, b, kind):
        loc = k.extent.start
        path = loc.file.name if loc.file else ""
        if not path.startswith(tree):
            return
        where = f"{path[len(tree):]}:{loc.line}:{loc.column}"
        if kind == "pair":
            found[where] = (kind, a.spelling, b.spelling, a.get_size(), starts(a), b.get_size(), starts(b))
        else:
            found[where] = (kind, a.spelling, "", a.get_size(), starts(a), 0, {})

    def walk(c):
        for k in c.get_children():
            if k.kind == ci.CursorKind.CALL_EXPR and k.spelling in COPIES:
                args = list(k.get_arguments())
                d, s = COPIES[k.spelling]
                if len(args) > max(d, s):
                    kd, rd = pointee(args[d])
                    ks, rs = pointee(args[s])
                    if kd == "record" and ks == "record" and rd.spelling != rs.spelling:
                        note(k, rd, rs, "pair")
                    elif kd == "record" and ks == "buffer":
                        note(k, rd, None, "buffer")
                    elif kd == "buffer" and ks == "record":
                        note(k, rs, None, "buffer")
            elif k.kind == ci.CursorKind.BINARY_OPERATOR and binary_op(k) == "=":
                # `*(A*) p = *(B*) q` cannot be written in C or C++ for A != B; an assignment of a
                # structure through a cast of a byte pointer is a buffer copy
                kids = [strip(x) for x in k.get_children()]
                if len(kids) == 2 and kids[0] is not None and kids[0].type.get_canonical().kind == ci.TypeKind.RECORD:
                    for side in kids:
                        if side is not None and side.kind == ci.CursorKind.UNARY_OPERATOR and unary_op(side) == "*":
                            inner = [strip(x) for x in side.get_children()]
                            if inner and inner[0] is not None:
                                kind, _ = pointee(inner[0])
                                cast = strip(inner[0])
                                if cast is not None and cast.kind == ci.CursorKind.CSTYLE_CAST_EXPR:
                                    src = [x for x in cast.get_children() if x.kind != ci.CursorKind.TYPE_REF]
                                    sk, _ = pointee(src[-1]) if src else (None, None)
                                    if sk == "buffer":
                                        note(k, kids[0].type.get_canonical(), None, "buffer")
            walk(k)

    walk(tu.cursor)
    return found


def collect(build, pattern, jobs, resource_dir):
    units = entries(build, pattern)
    sites = {}
    with concurrent.futures.ProcessPoolExecutor(jobs) as pool:
        for found in pool.map(scan, [(units[n], resource_dir) for n in sorted(units)], chunksize=2):
            sites.update(found)
    return sites, len(units)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default=str(ROOT / "port/build/whole-armhf"))
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--pattern", default=r"/prepared/tree/src/.*\.(c|cpp)$")
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    ref, n = collect(args.ref, args.pattern, args.jobs, resource_dir)
    lp, _ = collect(args.lp64, args.pattern, args.jobs, resource_dir)
    bad = []
    for where in sorted(set(ref) & set(lp)):
        kind, a, b, asz, ast, bsz, bst = ref[where]
        _, _, _, asz2, ast2, bsz2, bst2 = lp[where]
        if kind == "buffer":
            if asz != asz2 or ast != ast2:
                bad.append(f"  {where}: buffer copy of {a} ({asz:#x} bytes on ILP32, {asz2:#x} on LP64)")
            continue
        inv_a = {v: k for k, v in ast2.items()}
        inv_b = {v: k for k, v in bst2.items()}
        amap = {o: inv_a[m] for o, m in ast.items() if m in inv_a}
        bmap = {o: inv_b[m] for o, m in bst.items() if m in inv_b}
        clash = sorted(o for o in set(amap) & set(bmap) if amap[o] != bmap[o])
        if clash:
            o = clash[0]
            bad.append(f"  {where}: copy between {a} and {b}: GameCube {o:#x} at LP64 {amap[o]:#x} / {bmap[o]:#x} "
                       f"({ast[o]} / {bst[o]})")
    print(f"copy_view_check: {n} units, {len(lp)} copies between different structures or with a byte buffer, "
          f"{len(bad)} that move members on LP64")
    for line in bad:
        print(line)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
