#!/usr/bin/env python3
"""Lists the constructions in the game code that zero-fill an object first.

C++03 and later value-initialize `T()`: a class without a user-provided
constructor is zero-filled before its implicit constructor runs. The GameCube
compiler zero-filled only types it can construct trivially and otherwise just
ran the implicit constructor. The game relies on that: em.cpp sets cEm::subArc,
then the module's init function runs `new (em) cEm10()`, which must not wipe it
(crash in Em12Set, local r100 run of 14.10.2026). whole_prepare.py therefore
writes placement news without the parentheses (PLACEMENT_VALUE_INIT; 142 sites,
all classes with a polymorphic base: cEm, cObj, cModel, cLight ...). This tool
asks clang (clang-query on a configured build, through the #line-free copy of
whole_byte_views.py) for every constructor call that still zero-fills.

    whole_value_init.py BUILD_DIR --shadow SCRATCH [--jobs N]

Expected after the rule (14.10.2026): only trivially constructible types,
which the GameCube build zero-fills as well: two `GXColor()` arguments
(em10.cpp, t_page.cpp) and the members `flg()`, `status()` of cSubChar
(pl_npc.cpp, a cFlag each; the decompiled constructor clears them).
"""
import argparse
import json
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import whole_byte_views as views  # noqa: E402

MATCHER = 'm cxxConstructExpr(requiresZeroInitialization(), unless(isInTemplateInstantiation())).bind("zero")'
LOCATION = re.compile(r'^(?P<file>/[^:]+):(?P<line>\d+):(?P<col>\d+): note: "zero" binds here')


def run_unit(shadow: Path, unit: str, tree: str):
    cmd = [views.CLANG_QUERY, "-p", str(shadow), unit, "-c", "set bind-root false", "-c", "enable output diag",
           "-c", "disable output print", "-c", MATCHER]
    proc = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    found = []
    for line in proc.stdout.splitlines():
        m = LOCATION.match(line)
        if m and m["file"].startswith(tree):
            found.append((m["file"][len(tree) + 1:], int(m["line"]), int(m["col"])))
    return found


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("build", type=Path)
    ap.add_argument("--shadow", type=Path, required=True, help="scratch directory for the copy without #line")
    ap.add_argument("--jobs", type=int, default=8)
    args = ap.parse_args()
    shadow = views.shadow_tree(args.build, args.shadow)
    tree = str(shadow / "tree")
    db = json.loads((shadow / "compile_commands.json").read_text())
    units = sorted({e["file"] for e in db if e["file"].startswith(tree + "/src/")})
    sites = set()
    with ThreadPoolExecutor(args.jobs) as pool:
        for found in pool.map(lambda u: run_unit(shadow, u, tree), units):
            sites.update(found)
    for file, line, col in sorted(sites):
        text = (Path(tree) / file).read_text(errors="replace").splitlines()[line - 1].strip()
        print(f"{file}:{line}:{col}: {text}")
    print(f"value-initialization with zero fill: {len(sites)} sites", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
