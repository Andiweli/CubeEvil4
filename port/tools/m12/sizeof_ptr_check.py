#!/usr/bin/env python3
"""Milestone 12, way A: pointer arithmetic with the size of another type.

Decompiled code steps over a structure with its size: `(Work*) ((u8*) esp + sizeof(cEsp))` (the
own work of a derived class behind its base class), `buf + sizeof(Header)` (the data behind a
header), `(T*) (p + n * sizeof(U))`. On LP64 that goes wrong in two ways:
  - the size of a base class is not where the derived class's own members start: the compiler
    puts them into the base's tail padding (cEsp: 0x10C, sizeof 0x110; patch 0074, RE4_OWN);
  - a structure with pointers is larger on LP64: a buffer laid out with the GameCube sizes (disc
    data, a fixed area) is read at other places.

With libclang this check parses the units of an ILP32 build (armhf, the reference) and an LP64
build and lists every `+` / `-` of a pointer (or of an address as a number) and a term with
sizeof(X) of a structure X (for a typed pointer: another type than it points to), when on LP64
  - the pointer is a byte pointer or an address number and X has 4 or more bytes of tail padding
    ("tail": the own members of a class derived from X start there, not at sizeof(X)), or
  - sizeof(X) differs between the two builds ("size": what the buffer holds decides).
Sizes given for LP64 (RE4_LP64_SIZE) and offsets of the layout (offsetof, RE4_OWN) are left out.

    port/tools/m12/sizeof_ptr_check.py [--ref BUILD] [--lp64 BUILD] [--pattern REGEX]

Needs the libclang Python bindings (pip install libclang==18.1.1).
"""

import argparse
import concurrent.futures
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from raw_offset_check import (args_of, binary_op, descendants, entries, layout_offset,  # noqa: E402
                              reviewed_lp64, strip)

ROOT = Path(__file__).resolve().parents[3]


def pointee_record(c):
    """the record a pointer expression (or a byte view / address number of one) stands for"""
    import clang.cindex as ci
    c = strip(c)
    while c is not None:
        t = c.type.get_canonical()
        if t.kind == ci.TypeKind.POINTER:
            p = t.get_pointee().get_canonical()
            if p.kind == ci.TypeKind.RECORD:
                return p
        if c.kind in (ci.CursorKind.CSTYLE_CAST_EXPR, ci.CursorKind.CXX_REINTERPRET_CAST_EXPR,
                      ci.CursorKind.CXX_STATIC_CAST_EXPR, ci.CursorKind.UNEXPOSED_EXPR, ci.CursorKind.PAREN_EXPR):
            kids = [x for x in c.get_children() if x.kind != ci.CursorKind.TYPE_REF]
            c = strip(kids[-1]) if kids else None
            continue
        return None
    return None


def is_pointerish(c):
    import clang.cindex as ci
    t = c.type.get_canonical()
    K = ci.TypeKind
    return t.kind == K.POINTER or (t.kind in (K.UINT, K.ULONG, K.INT, K.LONG) and pointee_record(c) is not None)


def sizeof_records(c):
    """the records X of the sizeof(X) / sizeof(expr of X) terms of an expression"""
    import clang.cindex as ci
    out = []
    for k in descendants(c):
        if k.kind == ci.CursorKind.CXX_UNARY_EXPR:
            kids = list(k.get_children())
            if not kids:
                continue
            t = kids[0].type.get_canonical()
            if t.kind == ci.TypeKind.RECORD and t.get_size() > 0:
                out.append(t)
    return out


def data_end(t, depth=0):
    """where the data of a record ends (its last member, base classes included; single
    inheritance, bases at the start): less than sizeof when the record has tail padding"""
    import clang.cindex as ci
    end = 8 if depth == 0 and dynamic(t) else 0
    for f in t.get_declaration().get_children():
        if f.kind == ci.CursorKind.CXX_BASE_SPECIFIER and depth < 10:
            end = max(end, data_end(f.type.get_canonical(), depth + 1))
        elif f.kind == ci.CursorKind.FIELD_DECL and f.get_field_offsetof() >= 0:
            end = max(end, f.get_field_offsetof() // 8 + max(f.type.get_canonical().get_size(), 0))
    return end


def dynamic(t, depth=0):
    import clang.cindex as ci
    for k in t.get_declaration().get_children():
        if k.kind in (ci.CursorKind.CXX_METHOD, ci.CursorKind.DESTRUCTOR) and k.is_virtual_method():
            return True
        if k.kind == ci.CursorKind.CXX_BASE_SPECIFIER and depth < 10 and dynamic(k.type.get_canonical(), depth + 1):
            return True
    return False


def byte_view(c):
    """the pointer expression is a byte pointer or an address as a number"""
    import clang.cindex as ci
    t = c.type.get_canonical()
    K = ci.TypeKind
    if t.kind == K.POINTER:
        return t.get_pointee().get_canonical().kind in (K.CHAR_S, K.CHAR_U, K.SCHAR, K.UCHAR, K.VOID)
    return t.kind in (K.UINT, K.ULONG, K.INT, K.LONG)


def scan(job):
    import clang.cindex as ci
    entry, resource_dir = job
    tree = entry["file"].split("/prepared/tree/")[0] + "/prepared/tree/"
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    found = []

    def cast_target(parents):
        """the record a cast right above the expression turns it into (`(X*) (pool + i * sizeof(X))`)"""
        for x in reversed(parents):
            if x.kind in (ci.CursorKind.PAREN_EXPR, ci.CursorKind.UNEXPOSED_EXPR):
                continue
            if x.kind in (ci.CursorKind.CSTYLE_CAST_EXPR, ci.CursorKind.CXX_REINTERPRET_CAST_EXPR,
                          ci.CursorKind.CXX_STATIC_CAST_EXPR):
                t = x.type.get_canonical()
                if t.kind == ci.TypeKind.POINTER:
                    return t.get_pointee().get_canonical().spelling
            return None
        return None

    def walk(c, parents):
        for k in c.get_children():
            if k.kind == ci.CursorKind.BINARY_OPERATOR and binary_op(k) in ("+", "-"):
                kids = list(k.get_children())
                if len(kids) == 2:
                    for ptr, n in ((kids[0], kids[1]), (kids[1], kids[0])):
                        if not is_pointerish(ptr) or is_pointerish(n):
                            continue
                        if reviewed_lp64(n) or layout_offset(n):
                            break
                        # `i * sizeof(T) + (u32) w + RE4_OFS(W, arr, gc)`: the array's place comes from
                        # the layout (the sum around this one), the stride is right
                        outer = [x for x in parents if x.kind == ci.CursorKind.BINARY_OPERATOR and
                                 binary_op(x) in ("+", "-")]
                        if outer and (layout_offset(outer[0]) or reviewed_lp64(outer[0])):
                            break
                        target = pointee_record(ptr)
                        view = byte_view(ptr)
                        into = cast_target(parents)
                        for x in sizeof_records(n):
                            if not view and target is not None and x.spelling == target.spelling:
                                continue  # a typed pointer moved by its own size: not this check's form
                            if into == x.spelling:
                                continue  # element i of an array of X in a byte buffer, stepped by sizeof(X)
                            loc = k.extent.start
                            path = loc.file.name if loc.file else ""
                            if not path.startswith(tree):
                                continue
                            found.append((path[len(tree):], loc.line, loc.column, x.spelling, x.get_size(),
                                          target.spelling if target is not None else "-",
                                          data_end(x) if view else None))
                        break
            walk(k, parents + [k])

    walk(tu.cursor, [])
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
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    ref, n = collect(args.ref, args.pattern, args.jobs, resource_dir)
    lp, _ = collect(args.lp64, args.pattern, args.jobs, resource_dir)
    bad = []
    for key in sorted(set(lp)):
        f, line, col, x, size, target, tail = lp[key]
        r = ref.get(key)
        why = []
        if tail is not None and size - tail >= 4:
            why.append(f"tail: the data of {x} ends at {tail:#x}, sizeof {size:#x} (a derived class's own members "
                       f"start in the padding)")
        if r is not None and r[4] != size:
            why.append(f"size: sizeof({x}) {r[4]:#x} on ILP32, {size:#x} on LP64")
        if why:
            bad.append(f"  {f}:{line}:{col}: {target} pointer +/- sizeof({x}): " + "; ".join(why))
    print(f"sizeof_ptr_check: {n} units, {len(lp)} pointers moved by the size of another type, "
          f"{len(bad)} that land elsewhere on LP64")
    for line in bad:
        print(line)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
