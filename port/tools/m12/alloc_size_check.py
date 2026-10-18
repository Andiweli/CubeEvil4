#!/usr/bin/env python3
"""Milestone 12, way A: memory requested with a GameCube size for a structure that grew.

The game asks its heaps for fixed byte counts now and then instead of
sizeof: `em->Motion.pAttachCam = (ATTACH_CAMERA*) MEM_ALLOC(0x98, 1, 13)`,
`m_MotTbl = (void**) PL_MEM_ALLOC(0x1B4, ...)` (0x6D pointers of 4 bytes).
On LP64 the structure is larger (ATTACH_CAMERA: 0xA0 bytes, a pointer at
0x08) or the pointers are 8 bytes, and the users write past the block into
the header of the next heap cell (first 64-bit PC check with disc data,
18.10.2026: "OSCheckHeap: Failed cell->next->prev == cell", then a crash in
MotionMoveCore for a Ganado in r100).

With libclang this check parses the units of an LP64 build and lists every
call of an allocator (mem_alloc, mem_calloc, Debug_alloc, OSAllocFromHeap,
OSAllocFromArenaLo, malloc, calloc, realloc) with a constant size, or a size
`count * K` with a constant element size K, whose result is cast to, or
stored into, a pointer to an object larger than that size on LP64, or to a
pointer to pointers (a table of 4-byte entries on the GameCube; sizes
written with sizeof are right), and every memclr_asm / memset / memcpy with
a constant length shorter than the LP64 size of the structure its
destination points to (the tail is left as it was) or with a length
`count * K` on elements larger than K, and every `(T*) (base + i * K)` with a constant stride K smaller
than T on LP64 (a pool walked in GameCube steps). Sizes that are short on
ILP32 too (armhf, the reference: only a header cleared on purpose) are left
out.

    port/tools/m12/alloc_size_check.py [--ref BUILD] [--lp64 BUILD] [--pattern REGEX]

Needs the libclang Python bindings (pip install libclang==18.1.1).
"""

import argparse
import concurrent.futures
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from raw_offset_check import args_of, binary_op, const_value, entries, strip  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]
# allocator: index of its size argument (OSAllocFromHeap(heap, size), realloc(p, size))
ALLOCATORS = {"mem_alloc": 0, "mem_calloc": 0, "Debug_alloc": 0, "OSAllocFromHeap": 1, "OSAllocFromArenaLo": 0,
              "malloc": 0, "calloc": 0, "realloc": 1}
CASTS = ("CSTYLE_CAST_EXPR", "CXX_REINTERPRET_CAST_EXPR", "CXX_STATIC_CAST_EXPR")
FILLERS = {"memclr_asm", "memset", "memcpy", "memmove", "bzero", "memclr", "memcpy_asm"}


def per_element(c):
    """`count * K` / `K * count` with a constant K and no sizeof: K"""
    import clang.cindex as ci
    c = strip(c)
    if c is None or c.kind != ci.CursorKind.BINARY_OPERATOR:
        return None
    kids = list(c.get_children())
    toks = [t.spelling for t in c.get_tokens()]
    # (the operator by libclang, not by token: also inside macros)
    if len(kids) != 2 or "sizeof" in toks or binary_op(c) != "*":
        return None
    for a, b in ((kids[0], kids[1]), (kids[1], kids[0])):
        k = const_value(a)
        if k is not None and const_value(b) is None:
            return k
    return None


def alloc_size(c):
    """(byte count, whether it is a constant total or a per-element size, sizeof used) of an
    allocator call (c stripped), else None"""
    import clang.cindex as ci
    c = strip(c)
    if c is None or c.kind != ci.CursorKind.CALL_EXPR or c.spelling not in ALLOCATORS:
        return None
    args = list(c.get_arguments())
    if len(args) <= ALLOCATORS[c.spelling]:
        return None
    size = args[ALLOCATORS[c.spelling]]
    # sizeof, or a size already given for LP64 (lp64_sizes.json): reviewed
    uses_sizeof = bool({"sizeof", "RE4_LP64_SIZE"} & {t.spelling for t in size.get_tokens()})
    n = const_value(size)
    if c.spelling == "calloc" and len(args) == 2:
        m = const_value(args[1])
        n = n * m if n is not None and m is not None else None
    if n is not None:
        return n, "total", uses_sizeof
    k = per_element(size)
    return (k, "element", False) if k is not None else None


def polymorphic(rec):
    """the record has virtual functions (own or inherited): a pool of it holds derived classes"""
    import clang.cindex as ci
    decl = rec.get_declaration()
    for c in decl.get_children():
        if c.kind == ci.CursorKind.CXX_METHOD and c.is_virtual_method():
            return True
        if c.kind == ci.CursorKind.DESTRUCTOR and c.is_virtual_method():
            return True
        if c.kind == ci.CursorKind.CXX_BASE_SPECIFIER and polymorphic(c.type.get_canonical()):
            return True
    return False


def scan(job):
    import clang.cindex as ci
    entry, resource_dir, lp64 = job
    tree = entry["file"].split("/prepared/tree/")[0] + "/prepared/tree/"
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    found = []

    def check(where, target, alloc):
        size, how, uses_sizeof = alloc
        t = target.get_canonical()
        if t.kind != ci.TypeKind.POINTER:
            return
        obj = t.get_pointee().get_canonical()
        if obj.kind == ci.TypeKind.VOID or obj.get_size() <= 0:
            return
        table = obj.kind == ci.TypeKind.POINTER and size > obj.get_size() and not uses_sizeof
        if obj.get_size() > size or table:
            loc = where.extent.start
            path = loc.file.name if loc.file else ""
            if path.startswith(tree):
                kind = "table of pointers" if table else "object"
                found.append((path[len(tree):], loc.line, loc.column, size, obj.spelling, obj.get_size(),
                              kind if how == "total" else kind + ", per element"))

    def fill(call):
        """memclr_asm / memset / memcpy ... with a constant length on a record that is longer on
        LP64 (the tail stays as it was, or is not copied), or with `count * K` on elements larger
        than K"""
        if call.spelling not in FILLERS:
            return
        args = list(call.get_arguments())
        if len(args) < 2:
            return
        n = const_value(args[-1])
        if {"sizeof", "RE4_LP64_SIZE"} & {t.spelling for t in args[-1].get_tokens()}:
            return
        k = per_element(args[-1]) if n is None else None
        if n is None and k is None:
            return
        dest = args[0]
        while dest is not None and dest.kind in (ci.CursorKind.UNEXPOSED_EXPR, ci.CursorKind.PAREN_EXPR,
                                                 ci.CursorKind.CSTYLE_CAST_EXPR) and \
                dest.type.get_canonical().kind == ci.TypeKind.POINTER and \
                dest.type.get_canonical().get_pointee().get_canonical().kind == ci.TypeKind.VOID:
            kids = [x for x in dest.get_children() if x.kind != ci.CursorKind.TYPE_REF]
            dest = kids[-1] if kids else None
        if dest is None:
            return
        t = dest.type.get_canonical()
        if t.kind != ci.TypeKind.POINTER:
            return
        obj = t.get_pointee().get_canonical()
        loc = call.extent.start
        path = loc.file.name if loc.file else ""
        if not path.startswith(tree):
            return
        if n is None:
            # `count * K` bytes: K per element, short when the element grew (a pointer: 8 bytes)
            if obj.kind in (ci.TypeKind.POINTER, ci.TypeKind.RECORD) and obj.get_size() > k:
                found.append((path[len(tree):], loc.line, loc.column, k, obj.spelling, obj.get_size(),
                              f"{call.spelling}, per element"))
            return
        table = obj.kind == ci.TypeKind.POINTER and n > obj.get_size()
        if not table and (obj.kind != ci.TypeKind.RECORD or obj.get_size() <= n):
            return
        found.append((path[len(tree):], loc.line, loc.column, n, obj.spelling, obj.get_size(),
                      f"{call.spelling} of a table of pointers" if table else f"{call.spelling} shorter than the object"))

    def stride(k):
        """`(T*) (base + i * K)`: element `i` of a pool or table with a constant stride K; on LP64 T
        may be larger than K (the effect pool: 0x150 per slot, cEsp3f 0x1A8)"""
        t = k.type.get_canonical()
        if t.kind != ci.TypeKind.POINTER:
            return
        obj = t.get_pointee().get_canonical()
        if obj.kind != ci.TypeKind.RECORD or obj.get_size() <= 0:
            return
        kids = [x for x in k.get_children() if x.kind != ci.CursorKind.TYPE_REF]
        e = strip(kids[-1]) if kids else None
        if e is None or e.kind != ci.CursorKind.BINARY_OPERATOR:
            return
        parts = list(e.get_children())
        if len(parts) != 2:
            return
        if binary_op(e) != "+":
            return
        n = per_element(parts[1])
        if n is None:
            return
        if {"RE4_LP64_SIZE", "sizeof"} & {x.spelling for x in parts[1].get_tokens()}:
            return
        loc = k.extent.start
        path = loc.file.name if loc.file else ""
        if not path.startswith(tree):
            return
        if obj.get_size() > n:
            found.append((path[len(tree):], loc.line, loc.column, n, obj.spelling, obj.get_size(),
                          "object, stride of a pool"))
        elif lp64 and polymorphic(obj):
            # a pool of a base class: its derived classes may be larger than K on LP64 only
            found.append((path[len(tree):], loc.line, loc.column, n, obj.spelling, obj.get_size(),
                          "stride of a pool of a polymorphic class: check K against its largest derived class"))

    def walk(c):
        for k in c.get_children():
            if k.kind == ci.CursorKind.CALL_EXPR:
                fill(k)
            if k.kind.name in CASTS:
                stride(k)
            if k.kind.name in CASTS:
                kids = [x for x in k.get_children() if x.kind != ci.CursorKind.TYPE_REF]
                alloc = alloc_size(kids[-1]) if kids else None
                if alloc is not None:
                    check(k, k.type, alloc)
            elif k.kind == ci.CursorKind.BINARY_OPERATOR:
                kids = list(k.get_children())
                if len(kids) == 2:
                    alloc = alloc_size(kids[1])
                    if alloc is not None:
                        check(k, kids[0].type, alloc)
            elif k.kind == ci.CursorKind.VAR_DECL:
                kids = [x for x in k.get_children() if x.kind != ci.CursorKind.TYPE_REF]
                alloc = alloc_size(kids[-1]) if kids else None
                if alloc is not None:
                    check(k, k.type, alloc)
            walk(k)

    walk(tu.cursor)
    return found


def collect(build, pattern, jobs, resource_dir, lp64):
    units = entries(build, pattern)
    sites = {}
    with concurrent.futures.ProcessPoolExecutor(jobs) as pool:
        for found in pool.map(scan, [(units[n], resource_dir, lp64) for n in sorted(units)], chunksize=2):
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
    sites, n = collect(args.lp64, args.pattern, args.jobs, resource_dir, True)
    # sizes already short on ILP32 (clearing only a header on purpose): not LP64's
    ref, _ = collect(args.ref, args.pattern, args.jobs, resource_dir, False)
    # objects: not short on ILP32 already; tables of pointers: the same byte count on both
    # (a count already given for LP64 differs)
    sites = {k: v for k, v in sites.items()
             if ((k not in ref) if "table" not in v[6] else (k in ref and ref[k][3] == v[3]))}
    short = len([v for v in ref.values() if "table" not in v[6]])
    print(f"alloc_size_check: {n} units, {len(sites)} allocations, clears or copies too small on LP64 "
          f"({short} short on ILP32 too, by design)")
    for s in sorted(sites.values()):
        print(f"  {s[0]}:{s[1]}:{s[2]}: {s[3]:#x} bytes for {s[4]} ({s[5]:#x} bytes on LP64, {s[6]})")
    return 1 if sites else 0


if __name__ == "__main__":
    sys.exit(main())
