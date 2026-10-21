#!/usr/bin/env python3
"""Prepare the original sources for the whole-program port build.

Mirrors src/ and include/ into a build directory, applies the mechanical
rewrites listed in port/docs/whole-program-status.md, then applies the
reviewed patches in port/whole/patches/. The originals are never touched, so
the GameCube build keeps matching.

Writes <out>/modules.json: the units of main.dol and of every REL module.

Usage: whole_prepare.py --out DIR
"""
import argparse
import importlib.util
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import whole_asm_check  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
PATCHES = ROOT / "port" / "whole" / "patches"
OVERLAY = ROOT / "port" / "whole" / "include"
VERSION = "G4BE08"

# asm volatile -> asm, so the asm(...) macro in re4_whole_prelude.h drops it.
ASM_VOLATILE = re.compile(r"\b(asm|__asm__|__asm)\s+(volatile|__volatile__)\b")
# Placement new with empty parentheses, `new (em) cEm10()`: C++03 and later
# value-initialize, which zero-fills a class without a user-provided
# constructor before its implicit constructor runs; GCC 2.95 (C++98) only ran
# the constructor. The game sets fields first (em.cpp: subArc, then
# EmInitFunc -> `new (em) cEm10()`), so the fill would wipe them. Without the
# parentheses the object is default-initialized as on the GameCube; for
# classes with a user constructor both forms are the same.
# port/tools/whole_value_init.py lists what still zero-fills.
PLACEMENT_VALUE_INIT = re.compile(r"\bnew\s*\(([^()]*)\)\s*([A-Za-z_][\w:]*)\s*\(\s*\)")
# GameCube-era size parameters of the allocation operators are unsigned int;
# the C++ ABI on LP64 requires size_t.
OPERATOR_SIZE = re.compile(
    r"(operator\s+(?:new|delete)\s*(?:\[\])?\s*\([^)]*?)\bunsigned(?:\s+int)?\b(?!\s*(?:char|short|long)\b)")
# Game headers declare a few C library functions themselves with `unsigned int`
# for size_t (joy.h memcpy, db_toolbase.h / db_widget.h strlen): the same type
# on ILP32, a conflicting declaration on LP64 (milestone 12).
LIBC_DECL = re.compile(r'extern\s+"C"[^;]*\b(?:memcpy|memset|memmove|memcmp|strlen|strncpy|strncmp|strncat)\s*\(')
LIBC_UNSIGNED = re.compile(r"\bunsigned\s+int\b")
# Padding up to the GameCube size of a structure: `u8 pad[0x320 - sizeof(cModel)]`
# (four effect/cloth units) and sfd.h `padFA4[0x1018 - 0xD28 - sizeof(SFCON)]`.
# On LP64 the structure is larger and the size negative; it becomes 0 there,
# on ILP32 the value stays the same (milestone 12).
PAD_TO_SIZE = re.compile(r"\[([^\[\]]*?)\s*-\s*sizeof\s*\(([^()]*)\)\s*\]")
TEXT_SUFFIXES = (".c", ".cpp", ".h", ".hpp", ".inc")

# `decl asm("symbol")` outside a register declaration is a symbol alias, not
# code: the original declares a second name (often with a different signature)
# for an existing function or variable. It is kept as __asm (which the prelude
# does not drop); GCC 2.95 mangled names are translated to Itanium C++ names
# (u32: '{u32}' = RE4_ASM_U32, 'm' on ILP32, 'j' on LP64; size_t: SIZE_T_SYMBOLS).
ASM_LABEL = re.compile(r'\b(?:asm|__asm__)(\s*\(\s*")([A-Za-z_][A-Za-z0-9_$.]*)("\s*\))')
GCC295_SYMBOLS = {
    "DrawTile__FiiiiP7GXColor": "_Z8DrawTileiiiiP7GXColor",
    "Filter09Render__Fv": "_Z14Filter09Renderv",
    "FlgOnStatus__5EventUl": "_ZN5Event11FlgOnStatusE{u32}",
    "ScreenReSize__FUsUs": "_Z12ScreenReSizett",
    "SetCallBack__7cObjScrPFP4cObj_v": "_ZN7cObjScr11SetCallBackEPFvP4cObjE",
    "SndCall__FUsUsP3VeciiP5cUnit": "_Z7SndCallttP3VeciiP5cUnit",
    "__builtin_new": "_Znwj",
}
# Itanium names that depend on the width of size_t: written as a macro of the
# prelude (re4_whole_prelude.h), "_Znwj" on ILP32, "_Znwm" on LP64.
SIZE_T_SYMBOLS = {"_Znwj": "RE4_ASM_NEW", "_Znaj": "RE4_ASM_NEW_ARRAY"}
GCC295_MANGLED = re.compile(r"__(F|\d|builtin)")


def keep_asm_label(match: re.Match, line: str) -> str:
    if re.search(r"\bregister\b", line):
        return match.group(0)  # register pinning: dropped by the prelude
    name = match.group(2)
    if name in GCC295_SYMBOLS:
        name = GCC295_SYMBOLS[name]
    elif GCC295_MANGLED.search(name):
        sys.exit(f"unmapped GCC 2.95 symbol alias {name!r}: add it to GCC295_SYMBOLS")
    if name in SIZE_T_SYMBOLS:
        return f"__asm({SIZE_T_SYMBOLS[name]})"
    if "{u32}" in name:
        # GCC 2.95 `Ul` (u32): 'm' on ILP32 (unsigned long), 'j' on LP64
        # (unsigned int); RE4_ASM_U32 of the prelude, joined to the literal
        head, tail = name.split("{u32}", 1)
        return f'__asm("{head}" RE4_ASM_U32 "{tail}")'
    return f"__asm{match.group(1)}{name}{match.group(3)}"


# `extern T name asm("target");` naming a variable defined in the same unit:
# GCC 2.95 resolved the label to the unit's own (possibly static) symbol, which
# modern C++ gives an internal mangled name. A reference alias does the same.
DATA_ALIAS = re.compile(
    r'^(\s*)extern\s+(?!"C")(?P<type>[^;()]+?)\s+(?P<name>\w+)\s+(?:asm|__asm__)\s*\(\s*"(?P<target>\w+)"\s*\)\s*;')


def defines_locally(text: str, target: str) -> bool:
    if re.search(r"\\n" + re.escape(target) + r":", text):  # top-level asm data label
        return True
    return re.search(r"^\s*static\b[^;(]*\b" + re.escape(target) + r"\b\s*(=|;|\[)", text, re.M) is not None


def local_data_alias(line: str, text: str):
    m = DATA_ALIAS.match(line)
    if not m or not defines_locally(text, m["target"]):
        return None
    t = m["type"]
    return f"{m.group(1)}static {t}& {m['name']} = *({t}*) &{m['target']};{line[m.end():]}"


# The room number halfword read through the address of its first byte
# (G_ROOM_ID, RsfCheck(*(u16*) &pG->stage_no, ...)): on little-endian hosts
# patch 0016 puts stage_no at the other end of pG->room_id, so the address is
# taken of the halfword itself (the same address on the GameCube).
ROOM_ID_ADDR = re.compile(r"\(\s*u16\s*\*\s*\)\s*&\s*pG->(stage_no|stage_prev)\b")
ROOM_ID_WORD = {"stage_no": "room_id", "stage_prev": "room_id_prev"}

# Stores to the GX write-gather pipe (GXWGFifo, 0xCC008000) feed the
# graphics processor: `GXWGFifo.T = x;` (SDK GXVert.h, __gx.h) and
# `GXWGFifo->T = x;` (the game's gx.h) become re4_gx_put_T(x) (runtime/
# gx_fifo.cpp, port/whole/include/re4_gx_fifo.h). In GXVert.h the member is
# the macro parameter T itself: re4_gx_put_##T. Any other use of GXWGFifo
# (an address taken) stays and is listed by the build when it is compiled.
GX_FIFO_STORE = re.compile(r"\bGXWGFifo[ \t]*(?:\.|->)[ \t]*(\w+)[ \t]*=(?!=)[ \t]*([^;\n]*[^;\s])[ \t]*(;?)")
GX_FIFO_TYPES = {"u8", "u16", "u32", "s8", "s16", "s32", "f32"}


def gx_fifo_store(match: re.Match) -> str:
    member, value, end = match.group(1), match.group(2).strip(), match.group(3)
    name = f"re4_gx_put_{member}" if member in GX_FIFO_TYPES else f"re4_gx_put_##{member}"
    return f"{name}({value}){end}"


def rewrite_gx_fifo(text: str) -> str:
    if "GXWGFifo" not in text or not GX_FIFO_STORE.search(text):
        return text
    text = GX_FIFO_STORE.sub(gx_fifo_store, text)
    return '#include "re4_gx_fifo.h"\n' + text


def rewrite(text: str) -> str:
    text = whole_asm_check.rewrite_copies(text)
    text = whole_asm_check.drop_register_pins(text)
    original = text
    text = "\n".join(local_data_alias(line, original) or line if "asm" in line else line
                     for line in text.split("\n"))
    text = "\n".join(ASM_LABEL.sub(lambda m: keep_asm_label(m, line), line) if "asm" in line else line
                     for line in text.split("\n"))
    text = ASM_VOLATILE.sub(r"\1", text)
    text = OPERATOR_SIZE.sub(r"\1__SIZE_TYPE__", text)
    if "extern" in text:
        text = "\n".join(LIBC_UNSIGNED.sub("__SIZE_TYPE__", line) if LIBC_DECL.search(line) else line
                         for line in text.split("\n"))
    if "sizeof" in text:
        text = PAD_TO_SIZE.sub(lambda m: "[((%s) > sizeof(%s) ? (%s) - sizeof(%s) : 0)]" % (
            m.group(1).strip(), m.group(2), m.group(1).strip(), m.group(2)), text)
    if "new" in text:
        text = PLACEMENT_VALUE_INIT.sub(r"new (\1) \2", text)
    if "pG->stage_" in text:
        text = ROOM_ID_ADDR.sub(lambda m: f"(u16*) &pG->{ROOM_ID_WORD[m.group(1)]}", text)
    text = rewrite_gx_fifo(text)
    return text


# Word accesses to byte fields (port/tools/whole_byte_views.py, reviewed list
# port/whole/byte_views.json): `*(T*) x` -> RE4_BE(T, E, x) and
# `((T*) x)[i]` -> RE4_BE_AT(T, x, i) (re4_whole_prelude.h). Applied after
# the patches; a listed site that is not found exactly once stops the build.
BYTE_VIEWS = ROOT / "port" / "whole" / "byte_views.json"
CAST_PREFIX = re.compile(r"^\(\s*[\w ]+\*\s*\)\s*(?P<operand>.+)$")


def apply_byte_views(tree: Path) -> int:
    entries = json.loads(BYTE_VIEWS.read_text())
    texts = {}
    done = 0
    for e in entries:
        if e["action"] not in ("be", "be_part"):
            continue
        path = tree / e["file"]
        if path not in texts:
            texts[path] = path.read_text(encoding="utf-8", errors="surrogateescape").split("\n")
        lines = texts[path]
        operand = CAST_PREFIX.match(e["text"]).group("operand")
        if e["action"] == "be":
            pattern = re.compile(r"\*\s*" + re.escape(e["text"]))
            replace = lambda m: f"RE4_BE({e['type']}, {e['element']}, {operand})"
        else:
            pattern = re.compile(r"\(\s*" + re.escape(e["text"]) + r"\s*\)\s*\[(?P<i>[^\]]+)\]")
            replace = lambda m: f"RE4_BE_AT({e['type']}, {operand}, {m.group('i')})"
        # the recorded line, or near it (patches move lines)
        order = sorted(range(max(0, e["line"] - 40), min(len(lines), e["line"] + 40)),
                       key=lambda i: abs(i - (e["line"] - 1)))
        hits = [i for i in order if len(pattern.findall(lines[i])) == 1]
        if not hits:
            sys.exit(f"byte view {e['file']}:{e['line']} `{e['text']}` not found: "
                     "rerun port/tools/whole_byte_views.py and review port/whole/byte_views.json")
        lines[hits[0]] = pattern.sub(replace, lines[hits[0]], count=1)
        done += 1
    for path, lines in texts.items():
        path.write_text("\n".join(lines), encoding="utf-8", errors="surrogateescape")
    return done


# Signed 32-bit values the game turns into pointers (milestone 12,
# port/tools/m12/int_cast_check.py, reviewed list port/whole/lp64_int_casts.json):
# the operand of each listed cast `(T*) x` becomes RE4_ADDR32(x)
# (re4_whole_prelude.h), unsigned, so LP64 keeps the ILP32 bit pattern. Applied
# last (the list records the lines of the prepared tree); a listed site whose
# text is not on its line, or near it, as often as listed stops the build.
INT_CASTS = ROOT / "port" / "whole" / "lp64_int_casts.json"


def apply_int_casts(tree: Path) -> int:
    groups = {}
    for e in json.loads(INT_CASTS.read_text()):
        if e["action"] == "zero_extend":
            groups.setdefault((e["file"], e["line"], e["text"], e["operand"]), []).append(e)
    texts = {}
    done = 0
    for (file, line, text, operand), same in sorted(groups.items()):
        if not text.endswith(operand):
            sys.exit(f"lp64_int_casts.json: {file}:{line}: `{text}` does not end with its operand `{operand}`")
        path = tree / file
        if path not in texts:
            texts[path] = path.read_text(encoding="utf-8", errors="surrogateescape").split("\n")
        lines = texts[path]
        new = text[:len(text) - len(operand)] + f"RE4_ADDR32({operand})"
        pattern = re.compile(re.escape(text) + r"(?![\w.\[(]|->)")  # the whole operand, not a prefix of it
        order = sorted(range(max(0, line - 40), min(len(lines), line + 40)), key=lambda i: abs(i - (line - 1)))
        hits = [i for i in order if len(pattern.findall(lines[i])) == len(same)]
        if not hits:
            sys.exit(f"int cast {file}:{line} `{text}` not found: rerun port/tools/m12/int_cast_check.py "
                     "and review port/whole/lp64_int_casts.json")
        lines[hits[0]] = pattern.sub(lambda m: new, lines[hits[0]])
        done += len(same)
    for path, lines in texts.items():
        path.write_text("\n".join(lines), encoding="utf-8", errors="surrogateescape")
    return done


def mirror(src: Path, dst: Path) -> None:
    for path in src.rglob("*"):
        if path.is_dir():
            continue
        out = dst / path.relative_to(src)
        out.parent.mkdir(parents=True, exist_ok=True)
        if path.suffix in TEXT_SUFFIXES:
            text = path.read_text(encoding="utf-8", errors="surrogateescape")
            out.write_text(rewrite(text), encoding="utf-8", errors="surrogateescape")
        else:
            shutil.copyfile(path, out)


def load_config(name: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / "config" / VERSION / f"{name}.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def module_plan() -> dict:
    """The original link layout: the game units of main.dol and, per REL module,
    its units in link order with the module's defines (as configure.py builds
    them). The C units of main.dol are the SN/newlib C runtime (printf,
    strcpy, malloc...), which the platform C library replaces."""
    objects = load_config("objects")
    modules = load_config("modules")
    config_yml = (ROOT / "config" / VERSION / "config.yml").read_text()
    names = re.findall(r"^\s+name:\s*(\S+)\s*$", config_yml, re.M)
    main_units = [f"src/{u}" for u in objects.UNITS if u.startswith("game/") and u.endswith(".cpp")]
    plan = {"main": main_units, "modules": {}}
    for name in names:
        rel = json.loads((ROOT / "config" / VERSION / "modules" / name / "rel.json").read_text())
        units = modules.UNITS.get(name, [(f"{name}/{name}.cpp", None)])
        sources = [f"src/{extra[0] if extra and extra[0] else unit}" for unit, _first, *extra in units]
        # GCC-only -f flags (-fno-implement-inlines) have no clang equivalent and only
        # decide where inline bodies are emitted; the -D flags change the code.
        defines = [f"REL_MODULE={name}"] + [f[2:] for f in modules.CFLAGS.get(name, []) if f.startswith("-D")]
        plan["modules"][name] = {"id": rel["module_id"], "sources": sources, "defines": defines}
    return plan


# Dolphin SDK math units (src/lib). Their PS* functions are paired-single
# assembly; the portable C_* reference versions beside them are kept, and
# runtime/mtx_ps.c routes the PS* names to them.
SDK_C_ONLY_UNITS = ["mtx", "mtxvec", "mtx44", "mtx44vec", "vec", "quat"]
TOP_LEVEL_FUNCTION = re.compile(r"^(?:asm\s+)?[A-Za-z_][\w\s\*]*?\b(\w+)\s*\(")


def sdk_c_only(text: str) -> str:
    """Keeps preprocessor lines, data and the C_* function definitions of an SDK
    unit; drops every other top-level function (the PS* assembly versions)."""
    out, depth, keep, in_function = [], 0, True, False
    for line in text.split("\n"):
        code = line.split("//", 1)[0]
        if depth == 0 and not in_function:
            m = TOP_LEVEL_FUNCTION.match(line)
            if m and not code.rstrip().endswith(";"):
                in_function = True
                keep = m.group(1).startswith("C_")
        if not in_function or keep:
            out.append(line)
        depth += code.count("{") - code.count("}")
        if in_function and depth == 0 and "}" in code:
            in_function = False
            keep = True
    return "\n".join(out)


def write_sdk_c_only(out: Path) -> None:
    for unit in SDK_C_ONLY_UNITS:
        text = (ROOT / "src" / "lib" / f"{unit}.c").read_text(encoding="utf-8", errors="surrogateescape")
        target = out / "sdk_c" / f"{unit}.c"
        result = f"/* Generated by whole_prepare.py: the C_* functions of src/lib/{unit}.c. */\n" + sdk_c_only(text)
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists() or target.read_text(encoding="utf-8", errors="surrogateescape") != result:
            target.write_text(result, encoding="utf-8", errors="surrogateescape")


def apply_patches(tree: Path) -> None:
    for patch in sorted(PATCHES.glob("*.patch")):
        proc = subprocess.run(["patch", "-p1", "--forward", "--silent", "-d", str(tree),
                               "-i", str(patch)], capture_output=True, text=True)
        if proc.returncode != 0:
            sys.exit(f"patch failed: {patch.name}\n{proc.stdout}{proc.stderr}")


def sync(staging: Path, tree: Path) -> None:
    """Copy changed files only, so an unchanged source keeps its timestamp and
    is not recompiled after every CMake configure."""
    wanted = set()
    for path in staging.rglob("*"):
        if path.is_dir():
            continue
        rel = path.relative_to(staging)
        wanted.add(rel)
        dst = tree / rel
        data = path.read_bytes()
        if not dst.exists() or dst.read_bytes() != data:
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_bytes(data)
    for path in tree.rglob("*"):
        if path.is_file() and path.relative_to(tree) not in wanted:
            path.unlink()


# s32 / u32 are `long` in include/types.h and include/dolphin/types.h, and the
# CRI middleware's Sint32 / Uint32 in src/lib/cri/cri_xpt.h: 32 bits on ILP32
# as on the GameCube, 64 on LP64. Game headers include "types.h" with quotes,
# so the copy next to them wins over port/whole/include/types.h; these
# typedefs get the same exact-width form as the port's (milestone 12).
TYPEDEF_LONG32 = re.compile(r"^typedef\s+(signed|unsigned)\s+long\s+(s32|u32|Sint32|Uint32)\s*;", re.M)
EXACT_WIDTH_FILES = ("include/types.h", "include/dolphin/types.h", "src/lib/cri/cri_xpt.h")


# acosf / asinf in the game code (src/ outside src/lib) -> re4_acosf / re4_asinf
# (re4_whole_prelude.h): an argument a rounding step beyond +-1 counts as +-1. The port's
# unit vectors (C_VECNormalize, 1/sqrtf) can be a last bit longer than the GameCube's
# (PSVECNormalize, frsqrte and one Newton step), and acosf of a dot product of two of them
# was NaN (the insects' wall step, tester report 22.10.2026). Applied last, after the
# patches and the reviewed line lists, which match the original text.
ARC_CALL = re.compile(r"\b(acosf|asinf)(\s*\()")


def arc_calls(staging: Path) -> int:
    done = 0
    for path in sorted((staging / "src").rglob("*")):
        if path.suffix not in TEXT_SUFFIXES or path.relative_to(staging / "src").parts[0] == "lib":
            continue
        text = path.read_text(encoding="utf-8", errors="surrogateescape")
        new, n = ARC_CALL.subn(r"re4_\1\2", text)
        if n:
            path.write_text(new, encoding="utf-8", errors="surrogateescape")
            done += n
    if not done:
        sys.exit("arc_calls: no acosf / asinf call found in the game code")
    return done


def exact_width_types(staging: Path) -> None:
    for rel in EXACT_WIDTH_FILES:
        path = staging / rel
        text = path.read_text()
        new = TYPEDEF_LONG32.sub(
            lambda m: "#if __SIZEOF_LONG__ == 4\ntypedef %s long %s;\n#else\ntypedef __%sINT32_TYPE__ %s;\n#endif" % (
                m.group(1), m.group(2), "U" if m.group(1) == "unsigned" else "", m.group(2)), text)
        if new.count("__SIZEOF_LONG__") != 2:
            sys.exit(f"exact_width_types: 32-bit long typedefs not found in {rel}")
        path.write_text(new)


# Structures with a fixed form (port/whole/fixed_form.json): their pointer
# members become RE4_P32(T) (re4_ptr32.h): four bytes as on the GameCube on
# LP64, unchanged T* on ILP32 (milestone 12). `T* p;`, `T** p;`, `T* p[n];`
# and function pointers `R (*f)(A);`; anything else with a `*` in a listed
# structure stops the build (to be written by hand).
FIXED_FORM = ROOT / "port" / "whole" / "fixed_form.json"
MEMBER_PTR = re.compile(
    r"^(?P<ind>\s*)(?P<type>(?:(?:const|volatile|struct|class|union|unsigned|signed)\s+)*[A-Za-z_][\w:]*"
    r"(?:\s+const)?)\s*(?P<stars>\*+)\s*(?P<name>\w+)\s*(?P<dims>(?:\[[^\]]*\])*)\s*;(?P<rest>.*)$")
MEMBER_FUNC_PTR = re.compile(
    r"^(?P<ind>\s*)(?P<ret>(?:const\s+)?[A-Za-z_][\w:]*\s*\**)\s*\(\s*\*\s*(?P<name>\w+)\s*\)\s*"
    r"\((?P<args>[^()]*)\)\s*;(?P<rest>.*)$")


def rewrite_fixed_struct(text: str, name: str, where: str) -> tuple:
    m = re.search(r"\b(?:struct|class)\s+" + re.escape(name) + r"\b[^;{()]*\{", text)
    if not m:
        sys.exit(f"fixed_form.json: no definition of {name} in {where}")
    start = m.end()
    depth, i = 1, start
    while depth and i < len(text):
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        i += 1
    body = text[start:i - 1]
    lines = body.split("\n")
    changed = 0
    for k, line in enumerate(lines):
        code, sep, comment = line.partition("//")
        if "*" not in code:
            continue
        fm = MEMBER_FUNC_PTR.match(code)
        if fm:
            lines[k] = f"{fm['ind']}RE4_P32({fm['ret'].strip()}({fm['args']})) {fm['name']};{fm['rest']}{sep}{comment}"
            changed += 1
            continue
        pm = MEMBER_PTR.match(code)
        if pm and not re.search(r"\b(static|typedef|friend|operator|return)\b", code):
            t = pm["type"].strip()
            for _ in pm["stars"]:
                t = f"RE4_P32({t})"
            lines[k] = f"{pm['ind']}{t} {pm['name']}{pm['dims']};{pm['rest']}{sep}{comment}"
            changed += 1
            continue
        if re.search(r"\(", code) and not re.search(r"\(\s*\*", code):
            continue  # a member function
        sys.exit(f"fixed_form.json: {name} in {where}: cannot rewrite `{line.strip()}`")
    if not changed:
        sys.exit(f"fixed_form.json: {name} in {where} has no pointer member")
    return text[:start] + "\n".join(lines) + text[i - 1:], changed


def fixed_form_structs(staging: Path) -> int:
    spec = json.loads(FIXED_FORM.read_text())
    total = 0
    for rel, names in spec["structs"].items():
        path = staging / rel
        text = path.read_text(encoding="utf-8", errors="surrogateescape")
        for name in names:
            text, n = rewrite_fixed_struct(text, name, rel)
            total += n
        text = '#include "re4_ptr32.h"  /* port: fixed_form.json */\n' + text
        path.write_text(text, encoding="utf-8", errors="surrogateescape")
    return total


# Fixed work areas and pool slots that are too small on LP64 (port/whole/lp64_sizes.json,
# milestone 12): `expr` inside each `text` becomes RE4_LP64_SIZE(expr, lp64)
# (re4_whole_prelude.h), the GameCube value on ILP32. Runs after the patches
# (their context keeps the original lines); a count other than the listed one
# stops the build.
LP64_SIZES = ROOT / "port" / "whole" / "lp64_sizes.json"


def lp64_sizes(staging: Path) -> int:
    spec = json.loads(LP64_SIZES.read_text())
    total = 0
    for entry in spec["sizes"]:
        text, expr = entry["text"], entry["expr"]
        if expr not in text:
            sys.exit(f"lp64_sizes.json: `{expr}` is not part of `{text}`")
        new_text = text.replace(expr, f"RE4_LP64_SIZE({expr}, {entry['lp64']})", 1)
        paths = sorted({p for pattern in entry["files"] for p in staging.glob(pattern)})
        found = 0
        for path in paths:
            if path.relative_to(staging).parts[:2] == ("src", "lib"):
                continue
            body = path.read_text(encoding="utf-8", errors="surrogateescape")
            n = body.count(text)
            if n:
                found += n
                path.write_text(body.replace(text, new_text), encoding="utf-8", errors="surrogateescape")
        if found != entry["count"]:
            sys.exit(f"lp64_sizes.json: `{text}` found {found} times, expected {entry['count']}")
        total += found
    return total


def prepare(out: Path) -> Path:
    tree = out / "tree"
    staging = out / "staging"
    if staging.exists():
        shutil.rmtree(staging)
    mirror(ROOT / "src", staging / "src")
    mirror(ROOT / "include", staging / "include")
    exact_width_types(staging)
    fixed = fixed_form_structs(staging)
    apply_patches(staging)
    sizes = lp64_sizes(staging)
    views = apply_byte_views(staging)
    casts = apply_int_casts(staging)
    arcs = arc_calls(staging)
    plan = module_plan()
    # Every dropped inline asm with real instructions needs a C replacement.
    found, problems = whole_asm_check.check(staging, whole_asm_check.plan_units(plan), OVERLAY)
    if problems:
        sys.exit("inline asm check failed (port/tools/whole_asm_check.py):\n  " + "\n  ".join(problems))
    print(f"whole_prepare: asm {whole_asm_check.summary(found)}; {views} byte views; "
          f"{fixed} fixed-form pointer members; {sizes} LP64 work sizes; {casts} signed casts to pointers; "
          f"{arcs} acosf/asinf calls guarded")
    sync(staging, tree)
    shutil.rmtree(staging)
    write_sdk_c_only(out)
    listed = set(plan["main"]) | {s for m in plan["modules"].values() for s in m["sources"]}
    on_disk = {p.relative_to(tree).as_posix() for p in (tree / "src").rglob("*.cpp")
               if p.relative_to(tree / "src").parts[0] != "lib"}
    if listed - on_disk or on_disk - listed:
        sys.exit(f"module plan and sources disagree: missing {sorted(listed - on_disk)[:5]}, "
                 f"unassigned {sorted(on_disk - listed)[:5]}")
    text = json.dumps(plan, indent=1) + "\n"
    target = out / "modules.json"
    if not target.exists() or target.read_text() != text:
        target.write_text(text)
    return tree


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    prepare(args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
