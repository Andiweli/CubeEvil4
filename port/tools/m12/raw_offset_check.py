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

    port/tools/m12/raw_offset_check.py [--ref BUILD] [--lp64 BUILD] [--pattern REGEX] [--words]

Needs the libclang Python bindings (pip install libclang==18.1.1).
"""

import argparse
import concurrent.futures
import json
import os
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


def view_of(c):
    """(record type, element size) when c is a cast of a record pointer to a byte pointer or to an
    integer (the address as a number: `(u32) em + 0x378`), with --words also to a pointer to a 2-,
    4- or 8-byte integer or float (`((u32*) em)[0x8D]`); types by their kind, not their spelling."""
    import clang.cindex as ci
    c = strip(c)
    if c is None or c.kind not in (ci.CursorKind.CSTYLE_CAST_EXPR, ci.CursorKind.CXX_REINTERPRET_CAST_EXPR,
                                   ci.CursorKind.CXX_STATIC_CAST_EXPR):
        return None
    t = c.type.get_canonical()
    K = ci.TypeKind
    if t.kind in (K.INT, K.UINT, K.LONG, K.ULONG, K.LONGLONG, K.ULONGLONG) and t.get_size() >= 4:
        # the address as a number: `(u32) em + 0x378`, `(T*) ((u32) p + 8)`
        elem = 1
    elif t.kind != K.POINTER:
        return None
    else:
        pointee = t.get_pointee().get_canonical()
        if pointee.kind in (K.CHAR_S, K.CHAR_U, K.SCHAR, K.UCHAR):
            elem = 1
        elif os.environ.get("RE4_RAW_WORDS") and pointee.kind in WORD_KINDS() and pointee.get_size() in (2, 4, 8):
            elem = pointee.get_size()
        else:
            return None
    kids = list(c.get_children())
    if not kids:
        return None
    src = strip(kids[-1])
    st = src.type.get_canonical()
    if st.kind == ci.TypeKind.POINTER:
        rec = st.get_pointee().get_canonical()
        if rec.kind == ci.TypeKind.RECORD:
            return rec, elem
    return None


def WORD_KINDS():
    import clang.cindex as ci
    K = ci.TypeKind
    return (K.SHORT, K.USHORT, K.INT, K.UINT, K.LONG, K.ULONG, K.LONGLONG, K.ULONGLONG, K.FLOAT, K.DOUBLE)


def byte_view_of(c):
    """(record type) when c is a cast of a record pointer to a byte pointer."""
    v = view_of(c)
    return v[0] if v else None


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
        L.clang_getCursorUnaryOperatorKind.argtypes = [ci.Cursor]
        L.clang_getCursorUnaryOperatorKind.restype = ctypes.c_int
        L.clang_getUnaryOperatorKindSpelling.argtypes = [ctypes.c_int]
        L.clang_getUnaryOperatorKindSpelling.restype = ci._CXString
        _LIB["L"] = L
        _LIB["ops"] = {}
        _LIB["uops"] = {}
    return _LIB["L"]


def unary_op(c):
    """the operator of a unary expression ("&", "*", "-" ...), also inside macro expansions"""
    import clang.cindex as ci
    L = lib()
    k = L.clang_getCursorUnaryOperatorKind(c)
    ops = _LIB["uops"]
    if k not in ops:
        ops[k] = ci._CXString.from_result(L.clang_getUnaryOperatorKindSpelling(k))
    return ops[k]


def binary_op(c):
    import clang.cindex as ci
    L = lib()
    k = L.clang_getCursorBinaryOperatorKind(c)
    ops = _LIB["ops"]
    if k not in ops:
        ops[k] = ci._CXString.from_result(L.clang_getBinaryOperatorKindSpelling(k))
    return ops[k]


def const_terms(c, depth=0):
    """(sum of the constant terms, whether there are variable ones) of a sum `a + 8 + b * 6`; a sum
    with an offsetof (RE4_OFS / RE4_OWN of re4_whole_prelude.h, offsetof) follows the layout: (0, False)"""
    import clang.cindex as ci
    c = strip(c)
    if c is None or depth > 12:
        return 0, True
    # (an offsetof in the syntax tree, not the macro's name in the tokens: also inside other macros)
    if depth == 0 and layout_offset(c):
        return 0, False
    v = const_value(c)
    if v is not None:
        return v, False
    if c.kind == ci.CursorKind.BINARY_OPERATOR:
        op = binary_op(c)
        kids = list(c.get_children())
        if op in ("+", "-") and len(kids) == 2:
            a, va = const_terms(kids[0], depth + 1)
            b, vb = const_terms(kids[1], depth + 1)
            return a + (b if op == "+" else -b), va or vb
    return 0, True


def names_member(c, depth=0):
    """the expression names a member (`&((T*) 0)->m`, offsetof): its value follows the layout"""
    import clang.cindex as ci
    if c.kind in (ci.CursorKind.MEMBER_REF_EXPR, ci.CursorKind.MEMBER_REF) or is_offsetof(c):
        return True
    return depth < 12 and any(names_member(k, depth + 1) for k in c.get_children())


def descendants(c, depth=0, limit=16):
    """c and the cursors below it (expressions are shallow; the limit guards against huge trees)"""
    yield c
    if depth < limit:
        for k in c.get_children():
            yield from descendants(k, depth + 1, limit)


def is_offsetof(c):
    """`__builtin_offsetof(T, m)` (also offsetof of stddef.h, RE4_OFS / RE4_OWN): libclang shows it
    as an unexposed expression over a reference to the type and one to the member"""
    import clang.cindex as ci
    return c.kind == ci.CursorKind.UNEXPOSED_EXPR and \
        any(k.kind == ci.CursorKind.MEMBER_REF for k in c.get_children())


def layout_offset(c):
    """the expression takes an offset from the layout: an offsetof, or `&((T*) 0)->m`"""
    import clang.cindex as ci
    for k in descendants(c):
        if is_offsetof(k):
            return True
        if k.kind == ci.CursorKind.UNARY_OPERATOR and unary_op(k) == "&":
            kids = [strip(x) for x in k.get_children()]
            if kids and kids[0] is not None and kids[0].kind == ci.CursorKind.MEMBER_REF_EXPR:
                base = [strip(x) for x in kids[0].get_children()]
                b = base[0] if base else None
                while b is not None and b.kind == ci.CursorKind.CSTYLE_CAST_EXPR:
                    inner = [x for x in b.get_children() if x.kind != ci.CursorKind.TYPE_REF]
                    b = strip(inner[-1]) if inner else None
                if b is not None and const_value_plain(b) == 0:
                    return True
    return False


def has_sizeof(c):
    """the expression uses sizeof / alignof (a size that follows the layout)"""
    import clang.cindex as ci
    return any(k.kind == ci.CursorKind.CXX_UNARY_EXPR for k in descendants(c))


def reviewed_lp64(c):
    """the expression is a size given for LP64: RE4_LP64_SIZE of re4_whole_prelude.h adds the
    enumerator re4_lp64_size there (port/whole/lp64_sizes.json)"""
    import clang.cindex as ci
    for k in descendants(c):
        if k.kind == ci.CursorKind.DECL_REF_EXPR and k.referenced is not None and \
                k.referenced.kind == ci.CursorKind.ENUM_CONSTANT_DECL and k.referenced.spelling == "re4_lp64_size":
            return True
    return False


def const_value_plain(c):
    """the constant value of an expression (also a cast of 0 to a pointer), None otherwise"""
    L = lib()
    r = L.clang_Cursor_Evaluate(c)
    if not r:
        return None
    try:
        return L.clang_EvalResult_getAsLongLong(r) if L.clang_EvalResult_getKind(r) == 1 else None
    finally:
        L.clang_EvalResult_dispose(r)


def macro_instantiations(tu):
    """{file name: [(start offset, end offset, macro name)]} of the macro expansions of a unit parsed
    with the detailed preprocessing record (libclang: MACRO_INSTANTIATION cursors)"""
    import clang.cindex as ci
    out = {}
    for c in tu.cursor.get_children():
        if c.kind == ci.CursorKind.MACRO_INSTANTIATION and c.extent.start.file:
            out.setdefault(c.extent.start.file.name, []).append(
                (c.extent.start.offset, c.extent.end.offset, c.spelling))
    return out


def written_text(c, data, instantiations):
    """(text, line) of an expression as written in its file, or None when it comes from the body of
    a macro (the text is in the #define, not at the place of use); inside a macro argument the text
    of the argument"""
    start, end = spelling(c.extent.start), spelling(c.extent.end)
    loc = c.extent.start
    if not loc.file or start[0] != loc.file.name or end[0] != start[0] or end[3] < start[3]:
        return None
    for a, b, _ in instantiations.get(loc.file.name, ()):
        if a <= loc.offset < b:
            # expanded from a macro: written in one of its arguments, or in its body
            if not (a < start[3] and end[3] <= b):
                return None
            break
    return data[start[3]:end[3]].decode("utf-8", "surrogateescape"), start[1]


def spelling(loc):
    """(file name, line, column, offset) where the text of a location is written: inside a macro
    argument the place in the source, inside a macro body the #define (the location of a cursor is
    where the outermost macro is expanded)"""
    import ctypes
    import clang.cindex as ci
    L = lib()
    if "spell" not in _LIB:
        L.clang_getSpellingLocation.argtypes = [ci.SourceLocation, ctypes.POINTER(ci.c_object_p),
                                                ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint),
                                                ctypes.POINTER(ctypes.c_uint)]
        _LIB["spell"] = True
    f, line, col, off = ci.c_object_p(), ctypes.c_uint(), ctypes.c_uint(), ctypes.c_uint()
    L.clang_getSpellingLocation(loc, ctypes.byref(f), ctypes.byref(line), ctypes.byref(col), ctypes.byref(off))
    name = ci.File(f).name if f else ""
    return name, line.value, col.value, off.value


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
                    r = view_of(init[-1])
                    if r is not None:
                        byte_vars[k.get_usr()] = r
            if k.kind == ci.CursorKind.BINARY_OPERATOR:
                kids = list(k.get_children())
                if len(kids) == 2:
                    lhs, rhs = kids
                    op = binary_op(k)
                    if op == "=":
                        r = view_of(rhs)
                        ls = strip(lhs)
                        if r is not None and ls.kind == ci.CursorKind.DECL_REF_EXPR and ls.referenced is not None:
                            byte_vars[ls.referenced.get_usr()] = r
                    elif op in ("+", "-"):
                        val = const_value(rhs)
                        if val is None:
                            # `(u8*) w + (flip * 12 + 8 + n * 6)` (motion.cpp MOT_HIST): the constant
                            # terms of a sum with variable ones, the GameCube offset of a member
                            c, var = const_terms(rhs)
                            val = c if var and c else None
                        if val is not None and op == "-":
                            val = -val
                        view = view_of(lhs)
                        ls = strip(lhs)
                        if view is None and ls is not None and ls.kind == ci.CursorKind.DECL_REF_EXPR and \
                                ls.referenced is not None:
                            view = byte_vars.get(ls.referenced.get_usr())
                        rec = view[0] if view else None
                        if view is not None and val is not None:
                            val *= view[1]
                        if rec is not None and val is not None and val != 0:
                            loc = k.location
                            found.append((rec.get_declaration().get_usr(), rec.spelling, val,
                                          f"{loc.file.name if loc.file else '?'}:{loc.line}"))
            if k.kind == ci.CursorKind.ARRAY_SUBSCRIPT_EXPR:
                kids = list(k.get_children())
                if len(kids) == 2:
                    view = view_of(kids[0])
                    if view is None:
                        ls = strip(kids[0])
                        if ls is not None and ls.kind == ci.CursorKind.DECL_REF_EXPR and ls.referenced is not None:
                            view = byte_vars.get(ls.referenced.get_usr())
                    rec = view[0] if view else None
                    val = const_value(kids[1])
                    if val is None and view is not None:
                        c, var = const_terms(kids[1])
                        val = c if var and c else None
                    if view is not None and val is not None:
                        val *= view[1]
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
    ap.add_argument("--words", action="store_true",
                    help="also views as 2-, 4- or 8-byte elements (`((u32*) em)[0x8D]`), offsets scaled")
    args = ap.parse_args()
    if args.words:
        os.environ["RE4_RAW_WORDS"] = "1"
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
