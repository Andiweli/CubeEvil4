#!/usr/bin/env python3
"""Milestone 12, way A: fixed byte offsets into structures that move on LP64.

Decompiled code reaches members through raw offsets now and then:
`*(SJ *)((Uint8 *)adxt + 0x18)`, `((u8 *)wk)[0x2C8]`, or a byte pointer
`p = (Uint8 *)sje;` followed by `p + 0x80`. The offset is the GameCube one. On
LP64, when a pointer member lies in front of it, the member is elsewhere.

With libclang this check parses every unit twice, for an ILP32 build (armhf,
the reference) and an LP64 build, finds additions of an integer constant to a
byte-pointer view of a structure (directly, or through a local byte pointer
set from one in the same function) and compares where the member covering
that offset on ILP32 lies on LP64. Reported: offsets that land on another
place on LP64.

    port/tools/m12/raw_offset_check.py [--ref BUILD] [--lp64 BUILD] [--pattern REGEX]

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
BYTE_TYPES = {"char", "signed char", "unsigned char"}


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


def byte_view_of(c):
    """(record type) when c is a cast of a record pointer to a byte pointer."""
    import clang.cindex as ci
    c = strip(c)
    if c is None or c.kind not in (ci.CursorKind.CSTYLE_CAST_EXPR, ci.CursorKind.CXX_REINTERPRET_CAST_EXPR,
                                   ci.CursorKind.CXX_STATIC_CAST_EXPR):
        return None
    t = c.type.get_canonical()
    if t.kind != ci.TypeKind.POINTER or t.get_pointee().get_canonical().spelling.replace("const ", "") \
            not in BYTE_TYPES:
        return None
    kids = list(c.get_children())
    if not kids:
        return None
    src = strip(kids[-1])
    st = src.type.get_canonical()
    if st.kind == ci.TypeKind.POINTER:
        rec = st.get_pointee().get_canonical()
        if rec.kind == ci.TypeKind.RECORD:
            return rec
    return None


_LIB = {}


def lib():
    """libclang C functions the Python bindings do not wrap: the operator of a binary
    expression and the value of a constant one (both work inside macro expansions, where
    the tokens are the macro's name)."""
    if not _LIB:
        import ctypes
        import clang.cindex as ci
        L = ci.conf.lib
        L.clang_getCursorBinaryOperatorKind.argtypes = [ci.Cursor]
        L.clang_getCursorBinaryOperatorKind.restype = ctypes.c_int
        L.clang_getBinaryOperatorKindSpelling.argtypes = [ctypes.c_int]
        L.clang_getBinaryOperatorKindSpelling.restype = ci._CXString
        L.clang_Cursor_Evaluate.argtypes = [ci.Cursor]
        L.clang_Cursor_Evaluate.restype = ctypes.c_void_p
        L.clang_EvalResult_getKind.argtypes = [ctypes.c_void_p]
        L.clang_EvalResult_getKind.restype = ctypes.c_int
        L.clang_EvalResult_getAsLongLong.argtypes = [ctypes.c_void_p]
        L.clang_EvalResult_getAsLongLong.restype = ctypes.c_longlong
        L.clang_EvalResult_dispose.argtypes = [ctypes.c_void_p]
        _LIB["L"] = L
        _LIB["ops"] = {}
    return _LIB["L"]


def binary_op(c):
    import clang.cindex as ci
    L = lib()
    k = L.clang_getCursorBinaryOperatorKind(c)
    ops = _LIB["ops"]
    if k not in ops:
        ops[k] = ci._CXString.from_result(L.clang_getBinaryOperatorKindSpelling(k))
    return ops[k]


def names_member(c, depth=0):
    """the expression names a member (`&((T*) 0)->m`, offsetof): its value follows the layout"""
    import clang.cindex as ci
    if c.kind in (ci.CursorKind.MEMBER_REF_EXPR, ci.CursorKind.MEMBER_REF):
        return True
    return depth < 12 and any(names_member(k, depth + 1) for k in c.get_children())


def const_value(c):
    import clang.cindex as ci
    c = strip(c)
    if c is None or c.kind not in (ci.CursorKind.INTEGER_LITERAL, ci.CursorKind.BINARY_OPERATOR,
                                   ci.CursorKind.CSTYLE_CAST_EXPR, ci.CursorKind.UNARY_OPERATOR,
                                   ci.CursorKind.PAREN_EXPR, ci.CursorKind.UNEXPOSED_EXPR):
        return None
    if names_member(c):
        return None
    L = lib()
    r = L.clang_Cursor_Evaluate(c)
    if not r:
        return None
    try:
        return L.clang_EvalResult_getAsLongLong(r) if L.clang_EvalResult_getKind(r) == 1 else None
    finally:
        L.clang_EvalResult_dispose(r)


def dynamic(decl, depth=0):
    """the class has a vtable pointer (a virtual method of its own or of a base)"""
    import clang.cindex as ci
    for k in decl.get_children():
        if k.kind in (ci.CursorKind.CXX_METHOD, ci.CursorKind.DESTRUCTOR) and k.is_virtual_method():
            return True
        if k.kind == ci.CursorKind.CXX_BASE_SPECIFIER and depth < 8 and \
                dynamic(k.type.get_declaration(), depth + 1):
            return True
    return False


def field_map(rec_type, depth=0):
    """[(offset bytes, size bytes, name)] of the fields of a record type, with those of its first
    base class (at 0, or behind the vtable pointer when only the derived class has one)."""
    import clang.cindex as ci
    out = []
    decl = rec_type.get_declaration()
    for f in decl.get_children():
        if f.kind == ci.CursorKind.CXX_BASE_SPECIFIER:
            if depth < 8 and not any(x for x in out):
                b = f.type.get_canonical()
                base = 0 if dynamic(b.get_declaration()) or not dynamic(decl) else rec_type.get_align()
                out += [(o + base, sz, n) for o, sz, n in field_map(b, depth + 1)]
            continue
        if f.kind != ci.CursorKind.FIELD_DECL:
            continue
        off = f.get_field_offsetof()
        size = f.type.get_size()
        if off < 0 or size < 0:
            continue
        out.append((off // 8, size, f.spelling))
    return out


def scan_lp64(job):
    """findings in one unit: (record usr, record spelling, offset, file:line)"""
    import clang.cindex as ci
    entry, resource_dir = job
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    found = []

    def walk(c, byte_vars):
        for k in c.get_children():
            if k.kind == ci.CursorKind.FUNCTION_DECL or k.kind == ci.CursorKind.CXX_METHOD:
                walk(k, {})
                continue
            if k.kind == ci.CursorKind.VAR_DECL:
                init = [x for x in k.get_children() if x.kind != ci.CursorKind.TYPE_REF]
                if init:
                    r = byte_view_of(init[-1])
                    if r is not None:
                        byte_vars[k.get_usr()] = r
            if k.kind == ci.CursorKind.BINARY_OPERATOR:
                kids = list(k.get_children())
                if len(kids) == 2:
                    lhs, rhs = kids
                    op = binary_op(k)
                    if op == "=":
                        r = byte_view_of(rhs)
                        ls = strip(lhs)
                        if r is not None and ls.kind == ci.CursorKind.DECL_REF_EXPR and ls.referenced is not None:
                            byte_vars[ls.referenced.get_usr()] = r
                    elif op in ("+", "-"):
                        val = const_value(rhs)
                        if val is not None and op == "-":
                            val = -val
                        rec = byte_view_of(lhs)
                        ls = strip(lhs)
                        if rec is None and ls is not None and ls.kind == ci.CursorKind.DECL_REF_EXPR and \
                                ls.referenced is not None:
                            rec = byte_vars.get(ls.referenced.get_usr())
                        if rec is not None and val is not None and val != 0:
                            loc = k.location
                            found.append((rec.get_declaration().get_usr(), rec.spelling, val,
                                          f"{loc.file.name if loc.file else '?'}:{loc.line}"))
            if k.kind == ci.CursorKind.ARRAY_SUBSCRIPT_EXPR:
                kids = list(k.get_children())
                if len(kids) == 2:
                    rec = byte_view_of(kids[0])
                    val = const_value(kids[1])
                    if rec is not None and val is not None and val > 0:
                        loc = k.location
                        found.append((rec.get_declaration().get_usr(), rec.spelling, val,
                                      f"{loc.file.name if loc.file else '?'}:{loc.line}"))
            walk(k, byte_vars)

    walk(tu.cursor, {})
    layouts = {}
    for usr, spelling, val, where in found:
        if usr not in layouts:
            layouts[usr] = None
    # field maps of the records found (LP64)
    if layouts:
        def collect(c):
            for k in c.get_children():
                if k.kind in (ci.CursorKind.STRUCT_DECL, ci.CursorKind.CLASS_DECL, ci.CursorKind.UNION_DECL) and \
                        k.is_definition() and k.get_usr() in layouts and layouts[k.get_usr()] is None:
                    layouts[k.get_usr()] = (k.type.get_size(), field_map(k.type))
                collect(k)
        collect(tu.cursor)
    return entry["file"], found, layouts


def scan_ref(job):
    import clang.cindex as ci
    entry, resource_dir, usrs = job
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    layouts = {}

    def collect(c):
        for k in c.get_children():
            if k.kind in (ci.CursorKind.STRUCT_DECL, ci.CursorKind.CLASS_DECL, ci.CursorKind.UNION_DECL) and \
                    k.is_definition() and k.get_usr() in usrs and k.get_usr() not in layouts:
                layouts[k.get_usr()] = (k.type.get_size(), field_map(k.type))
            collect(k)
    collect(tu.cursor)
    return layouts


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default=str(ROOT / "port/build/whole-armhf"))
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--pattern", default=r"/prepared/tree/src/.*\.(c|cpp)$")
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    ea, eb = entries(args.ref, args.pattern), entries(args.lp64, args.pattern)
    common = sorted(set(ea) & set(eb))
    results = {}
    with concurrent.futures.ProcessPoolExecutor(args.jobs) as pool:
        for name, (f, found, lay) in zip(common, pool.map(scan_lp64, [(eb[n], resource_dir) for n in common],
                                                          chunksize=2)):
            if found:
                results[name] = (found, lay)
        refs = list(pool.map(scan_ref, [(ea[n], resource_dir, set(results[n][1])) for n in results]))
    bad, total = [], 0
    for (name, (found, lay)), ref in zip(results.items(), refs):
        for usr, spelling, val, where in found:
            total += 1
            if val < 0:
                # the outer object is not known here: every such place is listed
                bad.append(f"{where.split('/prepared/tree/')[-1]}: {spelling} - {-val:#x}: an outer object found "
                           f"from this one, check by hand")
                continue
            if usr not in ref or not lay.get(usr):
                continue
            rsize, rfields = ref[usr]
            lsize, lfields = lay[usr]
            if rsize == lsize and rfields == lfields:
                continue
            cover = [f for f in rfields if f[0] <= val < f[0] + max(f[1], 1)]
            if not cover:
                if val >= rsize:
                    # past the record: a trailing array, the next object, or the own work of a
                    # derived class, which on LP64 starts at the end of the record's data (tail
                    # padding reused: cEm 0x49C of 0x4A0), not at its size
                    bad.append(f"{where.split('/prepared/tree/')[-1]}: {spelling} + {val:#x}: past the record "
                               f"({rsize:#x} bytes on ILP32, {lsize:#x} on LP64), check by hand")
                    continue
                bad.append(f"{where.split('/prepared/tree/')[-1]}: {spelling} + {val:#x}: no member there on ILP32")
                continue
            off, size, member = cover[-1]
            moved = [f for f in lfields if f[2] == member]
            if moved and moved[0][0] + (val - off) != val:
                bad.append(f"{where.split('/prepared/tree/')[-1]}: {spelling} + {val:#x} ({member}) lies at "
                           f"{moved[0][0] + (val - off):#x} on LP64")
    print(f"raw_offset_check: {len(common)} units, {total} constant offsets into structures, "
          f"{len(set(bad))} that move on LP64")
    for line in sorted(set(bad)):
        print("  " + line)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
