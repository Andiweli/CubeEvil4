#!/usr/bin/env python3
"""Prepare the original sources for the whole-program port build.

Mirrors src/ and include/ into a build directory, applies the mechanical
rewrites listed in port/docs/whole-program-status.md, then applies the
reviewed patches in port/whole/patches/. The originals are never touched, so
the GameCube build keeps matching.

Writes <out>/sources.txt with every game translation unit to compile.

Usage: whole_prepare.py --out DIR
"""
import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PATCHES = ROOT / "port" / "whole" / "patches"

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


def rewrite(text: str) -> str:
    original = text
    text = "\n".join(local_data_alias(line, original) or line if "asm" in line else line
                     for line in text.split("\n"))
    text = "\n".join(ASM_LABEL.sub(lambda m: keep_asm_label(m, line), line) if "asm" in line else line
                     for line in text.split("\n"))
    text = ASM_VOLATILE.sub(r"\1", text)
    text = OPERATOR_SIZE.sub(r"\1__SIZE_TYPE__", text)
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


def game_sources(tree: Path) -> list:
    """Every C++ unit outside src/lib. The C units in src/game are the SN/newlib
    C runtime (printf, strcpy, malloc...), which the platform C library replaces."""
    return sorted(p.relative_to(tree).as_posix() for p in (tree / "src").rglob("*.cpp")
                  if p.relative_to(tree / "src").parts[0] != "lib")


def apply_patches(tree: Path) -> None:
    for patch in sorted(PATCHES.glob("*.patch")):
        proc = subprocess.run(["patch", "-p1", "--forward", "--silent", "-d", str(tree),
                               "-i", str(patch)], capture_output=True, text=True)
        if proc.returncode != 0:
            sys.exit(f"patch failed: {patch.name}\n{proc.stdout}{proc.stderr}")


def prepare(out: Path) -> Path:
    tree = out / "tree"
    if tree.exists():
        shutil.rmtree(tree)
    mirror(ROOT / "src", tree / "src")
    mirror(ROOT / "include", tree / "include")
    apply_patches(tree)
    (out / "sources.txt").write_text("\n".join(game_sources(tree)) + "\n")
    return tree


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    prepare(args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
