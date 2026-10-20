#!/usr/bin/env python3
"""Milestone 12, way A: RE4_OWN(B, T, gc) where the classes derived from B disagree.

RE4_OWN (re4_whole_prelude.h) is where the own work of a class derived from B begins when its first
member has type T: on the GameCube at sizeof(B), on LP64 where the compiler puts that member, at the
end of B's data (B's tail padding) for a member aligned to 4 or less, further on for one aligned to
8 (a pointer inside). Code that reaches the own work of an object through a B* (`((u8*) esp)[0xF9]`)
is only right for the classes whose work begins there. The church's light puzzle (r117.cpp, patch
0077) read byte 1 of a beam's work as RE4_OWN(cEsp, u8, 0xF8) + 1 = 0x10D; the beam is an esp06
whose work holds a pointer and begins at 0x110: the puzzle waited forever (tester, 19.10.2026).

With libclang this check parses the units of an LP64 build, takes every RE4_OWN use in the tree (its
macro instantiation, B from the syntax of its arguments) and every class derived directly from B
with the offset of its first own member (from the record layout), and lists the uses where B's
derived classes begin their own work at more than one offset: each needs the class it really
reaches named (reviewed.json) or a T of the right alignment.

    port/tools/m12/own_start_check.py [--lp64 BUILD] [--pattern REGEX]

Needs the libclang Python bindings (pip install libclang==18.1.1).
"""

import argparse
import concurrent.futures
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from raw_offset_check import args_of, entries  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]


def scan(job):
    import clang.cindex as ci
    entry, resource_dir = job
    tree = entry["file"].split("/prepared/tree/")[0] + "/prepared/tree/"
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir),
                                 options=ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
    uses, starts = [], {}
    for c in tu.cursor.walk_preorder():
        if c.kind == ci.CursorKind.MACRO_INSTANTIATION and c.spelling == "RE4_OWN":
            loc = c.location
            path = loc.file.name if loc.file else ""
            if not path.startswith(tree):
                continue
            # the arguments: B, T, gc at the top level of the parentheses
            args, depth, cur = [], 0, []
            for t in list(c.get_tokens())[1:]:
                s = t.spelling
                if s == "(":
                    depth += 1
                    if depth == 1:
                        continue
                elif s == ")":
                    depth -= 1
                    if depth == 0:
                        args.append(" ".join(cur))
                        break
                elif s == "," and depth == 1:
                    args.append(" ".join(cur))
                    cur = []
                    continue
                cur.append(s)
            if len(args) == 3:
                uses.append((path[len(tree):], loc.line, args[0], args[1], args[2]))
        elif c.kind in (ci.CursorKind.CLASS_DECL, ci.CursorKind.STRUCT_DECL) and c.is_definition():
            kids = list(c.get_children())
            bases = [k.type.get_declaration().spelling for k in kids if k.kind == ci.CursorKind.CXX_BASE_SPECIFIER]
            fields = [k for k in kids if k.kind == ci.CursorKind.FIELD_DECL]
            if len(bases) != 1 or not fields or c.type.get_size() <= 0:
                continue
            offset = c.type.get_offset(fields[0].spelling)
            if offset >= 0:
                starts.setdefault(bases[0], {})[c.spelling] = offset // 8
    return uses, starts


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--pattern", default=r"/prepared/tree/src/.*\.(c|cpp)$")
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    units = entries(args.lp64, args.pattern)
    uses, starts = set(), {}
    with concurrent.futures.ProcessPoolExecutor(args.jobs) as pool:
        for u, s in pool.map(scan, [(units[n], resource_dir) for n in sorted(units)], chunksize=2):
            uses.update(u)
            for base, classes in s.items():
                starts.setdefault(base, {}).update(classes)
    found = []
    for path, line, base, t, gc in sorted(uses):
        groups = {}
        for cls, off in starts.get(base, {}).items():
            groups.setdefault(off, []).append(cls)
        if len(groups) > 1:
            text = "; ".join(f"{off:#x}: {len(cls)} ({', '.join(sorted(cls)[:4])}{' ...' if len(cls) > 4 else ''})"
                             for off, cls in sorted(groups.items()))
            found.append(f"{path}:{line}: RE4_OWN({base}, {t}, {gc}): the classes derived from {base} begin "
                         f"their own work at {text}")
    print(f"own_start_check: {len(units)} units, {len(uses)} RE4_OWN uses, {len(found)} where the classes of "
          f"the base disagree on LP64")
    for line in found:
        print("  " + line)
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
