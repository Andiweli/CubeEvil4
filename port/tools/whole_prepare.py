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

ROOT = Path(__file__).resolve().parents[2]
PATCHES = ROOT / "port" / "whole" / "patches"
VERSION = "G4BE08"

# asm volatile -> asm, so the asm(...) macro in re4_whole_prelude.h drops it.
ASM_VOLATILE = re.compile(r"\b(asm|__asm__|__asm)\s+(volatile|__volatile__)\b")
# GameCube-era size parameters of the allocation operators are unsigned int;
# the C++ ABI on LP64 requires size_t.
OPERATOR_SIZE = re.compile(
    r"(operator\s+(?:new|delete)\s*(?:\[\])?\s*\([^)]*?)\bunsigned\s+int\b")
TEXT_SUFFIXES = (".c", ".cpp", ".h", ".hpp", ".inc")

# `decl asm("symbol")` outside a register declaration is a symbol alias, not
# code: the original declares a second name (often with a different signature)
# for an existing function or variable. It is kept as __asm (which the prelude
# does not drop); GCC 2.95 mangled names are translated to Itanium C++ names for
# ILP32 (u32 = unsigned long = 'm', size_t = unsigned int = 'j').
ASM_LABEL = re.compile(r'\b(?:asm|__asm__)(\s*\(\s*")([A-Za-z_][A-Za-z0-9_$.]*)("\s*\))')
GCC295_SYMBOLS = {
    "DrawTile__FiiiiP7GXColor": "_Z8DrawTileiiiiP7GXColor",
    "Filter09Render__Fv": "_Z14Filter09Renderv",
    "FlgOnStatus__5EventUl": "_ZN5Event11FlgOnStatusEm",
    "ScreenReSize__FUsUs": "_Z12ScreenReSizett",
    "SetCallBack__7cObjScrPFP4cObj_v": "_ZN7cObjScr11SetCallBackEPFvP4cObjE",
    "SndCall__FUsUsP3VeciiP5cUnit": "_Z7SndCallttP3VeciiP5cUnit",
    "__builtin_new": "_Znwj",
}
GCC295_MANGLED = re.compile(r"__(F|\d|builtin)")


def keep_asm_label(match: re.Match, line: str) -> str:
    if re.search(r"\bregister\b", line):
        return match.group(0)  # register pinning: dropped by the prelude
    name = match.group(2)
    if name in GCC295_SYMBOLS:
        name = GCC295_SYMBOLS[name]
    elif GCC295_MANGLED.search(name):
        sys.exit(f"unmapped GCC 2.95 symbol alias {name!r}: add it to GCC295_SYMBOLS")
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


def rewrite(text: str) -> str:
    original = text
    text = "\n".join(local_data_alias(line, original) or line if "asm" in line else line
                     for line in text.split("\n"))
    text = "\n".join(ASM_LABEL.sub(lambda m: keep_asm_label(m, line), line) if "asm" in line else line
                     for line in text.split("\n"))
    text = ASM_VOLATILE.sub(r"\1", text)
    text = OPERATOR_SIZE.sub(r"\1__SIZE_TYPE__", text)
    if "pG->stage_" in text:
        text = ROOM_ID_ADDR.sub(lambda m: f"(u16*) &pG->{ROOM_ID_WORD[m.group(1)]}", text)
    return text


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


def prepare(out: Path) -> Path:
    tree = out / "tree"
    staging = out / "staging"
    if staging.exists():
        shutil.rmtree(staging)
    mirror(ROOT / "src", staging / "src")
    mirror(ROOT / "include", staging / "include")
    apply_patches(staging)
    sync(staging, tree)
    shutil.rmtree(staging)
    write_sdk_c_only(out)
    plan = module_plan()
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
