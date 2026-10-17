#!/usr/bin/env python3
"""Milestone 12, way A: partial views of one structure that fall apart on LP64.

The decompiled code often declares only the members a unit needs, with byte
pads in between that hold the GameCube offsets (adx_bahx.c:
`struct { Uint8 pad[0xB4]; void *ahx; ... } ADXB_OBJ`). On ILP32 every view
puts a member at the same place. On LP64 the full view moves its members
behind each pointer, the partial view does not: the two units read and write
different bytes of the same object.

This check compiles the units of an ILP32 build (the reference, armhf) and an
LP64 build, and for every structure name defined in several units compares
the members by their ILP32 offset: two members that start at the same ILP32
offset must also start at the same LP64 offset. The view with the most
members stands for the structure.

    port/tools/m12/view_check.py [--ref BUILD] [--lp64 BUILD] [--pattern REGEX]

Default pattern: every unit of the prepared tree (game, SDK, CRI). Exit code 1
on a difference.
"""

import argparse
import collections
import concurrent.futures
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fixed_layout_check as F  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]


def units(build, pattern):
    cc = json.loads((Path(build) / "compile_commands.json").read_text())
    out = {}
    for e in cc:
        if re.search(pattern, e["file"]):
            out.setdefault(e["file"].split("/prepared/tree/")[-1], e)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default=str(ROOT / "port/build/whole-armhf"))
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--pattern", default=r"/prepared/tree/src/.*\.(c|cpp)$")
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    ua, ub = units(args.ref, args.pattern), units(args.lp64, args.pattern)
    common = sorted(set(ua) & set(ub))
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        la = dict(zip(common, pool.map(lambda n: F.layouts(ua[n]), common)))
        lb = dict(zip(common, pool.map(lambda n: F.layouts(ub[n]), common)))
    # record -> unit -> [(ilp32 offset, lp64 offset, member)]
    views = collections.defaultdict(dict)
    for u in common:
        for rec, (sa, ma) in la[u].items():
            if rec not in lb[u] or "/usr/" in rec or rec.startswith("__"):
                continue
            sb, mb = lb[u][rec]
            if len(ma) != len(mb):
                continue
            views[rec][u] = (sa, sb, [(x[0], y[0], x[1]) for x, y in zip(ma, mb)])
    bad = []
    for rec, per in sorted(views.items()):
        if len(per) < 2:
            continue
        ref_unit = max(per, key=lambda u: (len(per[u][2]), per[u][1]))
        ref = {}
        for a, b, m in per[ref_unit][2]:
            ref.setdefault(a, (b, m))
        problems = []
        for u, (sa, sb, members) in per.items():
            if u == ref_unit:
                continue
            for a, b, m in members:
                if a in ref and ref[a][0] != b:
                    problems.append(f"{m} at {a:#x}: {u} {b:#x}, {ref_unit} {ref[a][0]:#x} ({ref[a][1]})")
            if sb > per[ref_unit][1] and sa <= per[ref_unit][0]:
                problems.append(f"size: {u} {sb:#x} > {ref_unit} {per[ref_unit][1]:#x}")
        if problems:
            bad.append((rec, problems))
    print(f"view_check: {len(common)} units, {sum(1 for p in views.values() if len(p) > 1)} structures with "
          f"several views, {len(bad)} that fall apart on LP64")
    for rec, problems in bad:
        print(f"  {rec}: " + "; ".join(sorted(set(problems))[:4]) +
              (f" (+{len(set(problems)) - 4})" if len(set(problems)) > 4 else ""))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
