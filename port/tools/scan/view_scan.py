#!/usr/bin/env python3
"""Bug hunt (22.10.2026): two kinds of port bugs the compiler finds, in the game code of a build
(compile_commands.json, the prepared tree, src/ without src/lib).

1. byte order (`order`): a pointer to an object cast to a pointer to a scalar of another width and
   read or written through it at a constant place: `((u8*) inst)[3]` (patch 0090: the low byte
   of the s32 at 0 on the GameCube, the high byte on ARM and x86), `*(u16*) &word`,
   `((u32*) bytes)[1]`. The object is described by its fields (records, arrays, scalars); a view
   that covers exactly one field of its own width is fine, a view of part of a wider field or
   across several smaller fields depends on the byte order. port/tools/whole_byte_views.py
   already covers views of `&x` and of arrays (port/whole/byte_views.json, rewritten in the
   prepared tree); this one also takes pointer values (`(u8*) p->pInst`).

2. frame layout (`frame`): the address of a local variable used beyond the variable: a cast to a
   pointer to a larger type (patch 0089: `(R10fGondolaTbl*) mot` read the locals behind `mot`
   where CodeWarrior put them), the address as a number (patch 0063: `(u32) &pos + 0x28`), or
   pointer arithmetic on the address of a variable that is no array.

    port/tools/scan/view_scan.py BUILD [--jobs N] [--pattern REGEX] [--out FILE]

Prints one line per finding: kind, file:line, the text as written and what it covers. Needs the
libclang Python bindings (pip install libclang==18.1.1).
"""

import argparse
import concurrent.futures
import json
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(HERE.parent / "m12"))
from raw_offset_check import args_of, const_value_plain, entries, field_map, lib, strip, unary_op, binary_op  # noqa: E402

CASTS = None


def kinds():
    import clang.cindex as ci
    global CASTS
    if CASTS is None:
        CASTS = (ci.CursorKind.CSTYLE_CAST_EXPR, ci.CursorKind.CXX_REINTERPRET_CAST_EXPR,
                 ci.CursorKind.CXX_STATIC_CAST_EXPR, ci.CursorKind.CXX_FUNCTIONAL_CAST_EXPR)
    return CASTS


def scalar_kinds():
    import clang.cindex as ci
    K = ci.TypeKind
    return {K.CHAR_S, K.CHAR_U, K.SCHAR, K.UCHAR, K.SHORT, K.USHORT, K.INT, K.UINT, K.LONG, K.ULONG,
            K.LONGLONG, K.ULONGLONG, K.FLOAT, K.DOUBLE, K.BOOL, K.ENUM}


def leaves(t, base=0, out=None, depth=0):
    """[(offset, size, path)] of the scalar pieces of a type: records flattened (with the first base
    class, raw_offset_check.field_map), arrays by element, scalars as one piece"""
    import clang.cindex as ci
    out = [] if out is None else out
    t = t.get_canonical()
    K = ci.TypeKind
    if depth > 6 or len(out) > 4096:
        return out
    if t.kind == K.RECORD:
        decl = t.get_declaration()
        for off, size, name in field_map(t):
            f = next((x for x in decl.get_children() if x.kind == ci.CursorKind.FIELD_DECL and x.spelling == name),
                     None)
            ft = f.type if f is not None else None
            if ft is None:
                out.append((base + off, size, name))
                continue
            n0 = len(out)
            leaves(ft, base + off, out, depth + 1)
            for k in range(n0, len(out)):
                o, s, p = out[k]
                out[k] = (o, s, name + ("." + p if p else ""))
        # base classes beyond the first: field_map takes the first only
        return out
    if t.kind == K.CONSTANTARRAY:
        et = t.get_array_element_type()
        es = et.get_size()
        n = t.get_array_size()
        if es <= 0 or n <= 0:
            return out
        if et.get_canonical().kind == K.RECORD and n > 64:
            n = 64
        for i in range(min(n, 4096)):
            n0 = len(out)
            leaves(et, base + i * es, out, depth + 1)
            for k in range(n0, len(out)):
                o, s, p = out[k]
                out[k] = (o, s, f"[{i}]" + ("." + p if p else ""))
        return out
    if t.kind in scalar_kinds() or t.kind == K.POINTER:
        out.append((base, t.get_size(), ""))
    return out


def describe(obj_type, off, width):
    """None when [off, off + width) is exactly one piece of that width, else the pieces it touches"""
    pieces = leaves(obj_type)
    if not pieces:
        return None
    touched = [(o, s, p) for o, s, p in pieces if o < off + width and off < o + s]
    if len(touched) == 1 and touched[0][0] == off and touched[0][1] == width:
        return None
    if not touched:
        return "padding"
    return ", ".join(f"{p or '(scalar)'} at {o} size {s}" for o, s, p in touched[:6]) + \
        (" ..." if len(touched) > 6 else "")


def local_var(c):
    """the VAR_DECL of an automatic local that c names (a DECL_REF_EXPR), else None"""
    import clang.cindex as ci
    c = strip(c)
    if c is None or c.kind != ci.CursorKind.DECL_REF_EXPR:
        return None
    d = c.referenced
    if d is None or d.kind != ci.CursorKind.VAR_DECL:
        return None
    if d.storage_class in (ci.StorageClass.STATIC, ci.StorageClass.EXTERN):
        return None
    p = d.semantic_parent
    if p is None or p.kind not in (ci.CursorKind.FUNCTION_DECL, ci.CursorKind.CXX_METHOD, ci.CursorKind.CONSTRUCTOR,
                                   ci.CursorKind.DESTRUCTOR, ci.CursorKind.FUNCTION_TEMPLATE):
        return None
    return d


def address_of_local(c):
    """(VAR_DECL, is array) when c is `&local` or a local array (decayed), else None"""
    import clang.cindex as ci
    c = strip(c)
    if c is None:
        return None
    if c.kind == ci.CursorKind.UNARY_OPERATOR and unary_op(c) == "&":
        kids = list(c.get_children())
        d = local_var(kids[0]) if kids else None
        if d is not None:
            return d, d.type.get_canonical().kind == ci.TypeKind.CONSTANTARRAY
        return None
    d = local_var(c)
    if d is not None and d.type.get_canonical().kind == ci.TypeKind.CONSTANTARRAY:
        return d, True
    return None


def cast_source(c):
    """the operand of a cast (through further casts between pointer types)"""
    import clang.cindex as ci
    kids = [x for x in c.get_children() if x.kind != ci.CursorKind.TYPE_REF]
    return strip(kids[-1]) if kids else None


def text_of(c, data):
    e = c.extent
    if not e.start.file or not e.end.file or e.start.file.name != e.end.file.name:
        return "?"
    s = data.get(e.start.file.name)
    if s is None:
        try:
            s = data[e.start.file.name] = Path(e.start.file.name).read_bytes()
        except OSError:
            return "?"
    t = s[e.start.offset:e.end.offset].decode("utf-8", "surrogateescape")
    return " ".join(t.split())[:160]


def scan(job):
    import clang.cindex as ci
    path, entry, resource_dir = job
    index = ci.Index.create()
    try:
        tu = index.parse(entry["file"], args=args_of(entry, resource_dir),
                         options=ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
    except ci.TranslationUnitLoadError as e:
        return [f"error {path}: {e}"]
    main = entry["file"]
    tree = main.split("/prepared/tree/")[0] + "/prepared/tree/"
    out = []
    data = {}
    K = ci.TypeKind
    scal = scalar_kinds()

    def where(c):
        loc = c.location
        name = loc.file.name if loc.file else "?"
        return f"{name.replace(tree, '')}:{loc.line}"

    def access(cast, parent, grand):
        """(byte offset or None, the access expression) when the cast pointer is read or written at
        a constant place: *(T*) x, ((T*) x)[k], *((T*) x + k); None when used otherwise"""
        width = cast.type.get_pointee().get_size()
        if parent is None:
            return None
        if parent.kind == ci.CursorKind.ARRAY_SUBSCRIPT_EXPR:
            kids = list(parent.get_children())
            if len(kids) == 2 and strip(kids[0]) == cast:
                k = const_value_plain(kids[1])
                return (None if k is None else k * width), parent
            return None
        if parent.kind == ci.CursorKind.UNARY_OPERATOR and unary_op(parent) == "*":
            return 0, parent
        if parent.kind == ci.CursorKind.BINARY_OPERATOR and binary_op(parent) in ("+", "-") and grand is not None and \
                grand.kind == ci.CursorKind.UNARY_OPERATOR and unary_op(grand) == "*":
            kids = list(parent.get_children())
            other = kids[1] if strip(kids[0]) == cast else kids[0]
            k = const_value_plain(other)
            if k is None:
                return None, grand
            return (k if binary_op(parent) == "+" else -k) * width, grand
        return None

    def visit(c, parents):
        for k in c.get_children():
            visit_one(k, parents + [c])

    def visit_one(c, parents):
        loc = c.location
        if loc.file is None or loc.file.name != main:
            # only the unit's own code (headers are their own units' business or shared)
            if c.kind not in (ci.CursorKind.NAMESPACE, ci.CursorKind.LINKAGE_SPEC, ci.CursorKind.UNEXPOSED_DECL):
                return
        if c.kind in kinds():
            check_cast(c, parents)
        elif c.kind == ci.CursorKind.BINARY_OPERATOR and binary_op(c) in ("+", "-"):
            kids = list(c.get_children())
            if len(kids) == 2:
                for side, other in ((kids[0], kids[1]), (kids[1], kids[0])):
                    a = address_of_local(side)
                    if a is not None and not a[1] and strip(side).kind == ci.CursorKind.UNARY_OPERATOR:
                        k = const_value_plain(other)
                        if k != 0:
                            out.append(f"frame\t{where(c)}\t{text_of(c, data)}\tarithmetic on the address of "
                                       f"`{a[0].spelling}` ({a[0].type.spelling}, {a[0].type.get_size()} bytes)")
        visit(c, parents)

    def check_cast(c, parents):
        t = c.type.get_canonical()
        src = cast_source(c)
        if src is None:
            return
        st = src.type.get_canonical()
        # frame layout: the address of a local as a number or as a pointer to something larger
        a = address_of_local(src)
        if a is not None:
            d, is_array = a
            size = d.type.get_size()
            if t.kind in (K.INT, K.UINT, K.LONG, K.ULONG, K.LONGLONG, K.ULONGLONG):
                out.append(f"frame\t{where(c)}\t{text_of(c, data)}\tthe address of `{d.spelling}` "
                           f"({d.type.spelling}, {size} bytes) as a number")
            elif t.kind == K.POINTER:
                pt = t.get_pointee().get_canonical()
                ps = pt.get_size()
                if pt.kind == K.RECORD and ps > size > 0:
                    out.append(f"frame\t{where(c)}\t{text_of(c, data)}\t`{d.spelling}` ({d.type.spelling}, {size} "
                               f"bytes) seen as {pt.spelling} ({ps} bytes)")
        # byte order: views of other width read or written at a constant place
        if t.kind != K.POINTER:
            return
        pt = t.get_pointee().get_canonical()
        if pt.kind not in scal or pt.get_size() <= 0:
            return
        if st.kind == K.POINTER:
            obj = st.get_pointee().get_canonical()
        elif st.kind == K.CONSTANTARRAY:
            obj = st
        else:
            return
        if obj.kind in (K.VOID, K.CHAR_S, K.CHAR_U, K.SCHAR, K.UCHAR) or obj.get_size() <= 0:
            return
        if obj.kind == K.CONSTANTARRAY and obj.get_array_element_type().get_canonical().kind in \
                (K.CHAR_S, K.CHAR_U, K.SCHAR, K.UCHAR) and pt.get_size() == 1:
            return
        parent = parents[-1] if parents else None
        grand = parents[-2] if len(parents) > 1 else None
        # through parentheses / implicit conversions to the access
        i = len(parents) - 1
        while i >= 0 and parents[i].kind in (ci.CursorKind.PAREN_EXPR, ci.CursorKind.UNEXPOSED_EXPR):
            i -= 1
        parent = parents[i] if i >= 0 else None
        grand = parents[i - 1] if i >= 1 else None
        if parent is not None and parent.kind == ci.CursorKind.BINARY_OPERATOR:
            j = i - 1
            while j >= 0 and parents[j].kind in (ci.CursorKind.PAREN_EXPR, ci.CursorKind.UNEXPOSED_EXPR):
                j -= 1
            grand = parents[j] if j >= 0 else None
        acc = access(c, parent, grand)
        if acc is None:
            return
        off, expr = acc
        width = pt.get_size()
        if off is None:
            if obj.kind == K.CONSTANTARRAY:
                et = obj.get_array_element_type().get_canonical()
                if et.kind in scal and et.get_size() == width:
                    return  # `((u32*) words)[i]` over an array of 4-byte scalars
            what = describe(obj, 0, width) if obj.kind != K.CONSTANTARRAY else None
            if obj.get_size() == width and what is None:
                return
            out.append(f"order?\t{where(expr)}\t{text_of(expr, data)}\t{width}-byte view of {obj.spelling} at a "
                       f"variable place")
            return
        what = describe(obj, off, width)
        if what is not None:
            out.append(f"order\t{where(expr)}\t{text_of(expr, data)}\t{width}-byte view of {obj.spelling} at "
                       f"{off}: {what}")

    visit(tu.cursor, [])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--pattern", default=r"/prepared/tree/src/(?!lib/).*\.(c|cpp)$")
    ap.add_argument("--out")
    a = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    jobs = [(p, e, resource_dir) for p, e in sorted(entries(a.build, a.pattern).items())]
    seen = set()
    lines = []
    with concurrent.futures.ProcessPoolExecutor(a.jobs) as ex:
        for found in ex.map(scan, jobs, chunksize=1):
            for f in found:
                if f not in seen:
                    seen.add(f)
                    lines.append(f)
    lines.sort()
    text = "\n".join(lines) + ("\n" if lines else "")
    if a.out:
        Path(a.out).write_text(text)
    sys.stdout.write(text)
    kinds_count = {}
    for f in lines:
        kinds_count[f.split("\t")[0]] = kinds_count.get(f.split("\t")[0], 0) + 1
    print("view_scan: " + ", ".join(f"{k} {v}" for k, v in sorted(kinds_count.items())) if lines else
          "view_scan: nothing found", file=sys.stderr)


if __name__ == "__main__":
    main()
