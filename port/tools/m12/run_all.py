#!/usr/bin/env python3
"""Milestone 12: every 64-bit check of port/tools/m12 on the real builds, with the reviewed
findings (port/tools/m12/reviewed.json) taken off.

Each check reports the places where the ILP32 build (armhf, the reference) and the LP64 build
(x86_64) part. A finding is either fixed (rule, patch) or reviewed: then reviewed.json holds the
finding (its report line without line and column numbers) and why it is right as it is. This
script runs the checks, compares their findings with that list and prints a table: findings,
reviewed, new. New findings, and reviewed entries no check reports any more (the list must stay
accurate), are listed and make the exit code 1.

    port/tools/m12/run_all.py [--ref BUILD] [--lp64 BUILD] [--only CHECK,...] [--dump FILE]

--dump writes the current findings (normalised) to FILE, as a start for new entries of the list.
Needs the libclang Python bindings (pip install libclang==18.1.1); about 20 minutes on 4 cores.
"""

import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
REVIEWED = HERE / "reviewed.json"
PAT = r"/prepared/tree/src/.*\.(c|cpp)$"
# check -> (script, arguments); REF / LP64 / JOBS replaced
CHECKS = {
    "fixed_layout_check": ["fixed_layout_check.py", "--ref", "REF", "--lp64", "LP64", "--jobs", "JOBS"],
    "decl_size_check": ["decl_size_check.py", "LP64", "--jobs", "JOBS"],
    "view_check": ["view_check.py", "--ref", "REF", "--lp64", "LP64", "--jobs", "JOBS"],
    "proto_check": ["proto_check.py", "--ref", "REF", "--lp64", "LP64", "--jobs", "JOBS"],
    "cast_check": ["cast_check.py", "--ref", "REF", "--lp64", "LP64", "--pattern", PAT, "--jobs", "JOBS"],
    "raw_offset_check": ["raw_offset_check.py", "--ref", "REF", "--lp64", "LP64", "--jobs", "JOBS"],
    "raw_offset_words": ["raw_offset_check.py", "--words", "--ref", "REF", "--lp64", "LP64", "--jobs", "JOBS"],
    "overlay_check": ["overlay_check.py", "--ref", "REF", "--lp64", "LP64", "--jobs", "JOBS"],
    "int_cast_check": ["int_cast_check.py", "--lp64", "LP64", "--jobs", "JOBS"],
    "ptr_slot_check": ["ptr_slot_check.py", "--lp64", "LP64", "--jobs", "JOBS"],
    "ptr_word_check": ["ptr_word_check.py", "--lp64", "LP64", "--jobs", "JOBS"],
    "alloc_size_check": ["alloc_size_check.py", "--ref", "REF", "--lp64", "LP64", "--jobs", "JOBS"],
    "span_check": ["span_check.py", "--ref", "REF", "--lp64", "LP64", "--jobs", "JOBS"],
    "array_view_check": ["array_view_check.py", "--ref", "REF", "--lp64", "LP64", "--jobs", "JOBS"],
    "sizeof_ptr_check": ["sizeof_ptr_check.py", "--ref", "REF", "--lp64", "LP64", "--jobs", "JOBS"],
    "work_view_check": ["work_view_check.py", "--ref", "REF", "--lp64", "LP64", "--jobs", "JOBS"],
    "copy_view_check": ["copy_view_check.py", "--ref", "REF", "--lp64", "LP64", "--jobs", "JOBS"],
    "reloc_check": ["reloc_check.py", "--lp64", "LP64", "--jobs", "JOBS"],
}


def normalise(line):
    """a report line without line and column numbers (they move when a patch changes a file)"""
    return re.sub(r"(\.(?:c|cpp|h)):\d+(?::\d+)?", r"\1", line.strip())


def findings(output, check):
    lines = [line for line in output.splitlines() if line.startswith("  ")]
    if check == "array_view_check":
        # every member array indexed past its end is listed; only those that move are findings
        lines = [line for line in lines if line.rstrip().endswith("<- moves")]
    return [normalise(line) for line in lines]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default=str(ROOT / "port/build/whole-armhf"))
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--jobs", default="4")
    ap.add_argument("--only", help="comma-separated checks")
    ap.add_argument("--dump", help="write the current findings to this file (JSON)")
    args = ap.parse_args()
    checks = dict(CHECKS)
    if args.only:
        wanted = set(args.only.split(","))
        checks = {k: v for k, v in checks.items() if k in wanted}
    reviewed = json.loads(REVIEWED.read_text())["reviewed"] if REVIEWED.exists() else {}
    rows, new, stale, dump = [], {}, {}, {}
    for name, cmd in checks.items():
        argv = [sys.executable, str(HERE / cmd[0])] + [
            {"REF": args.ref, "LP64": args.lp64, "JOBS": args.jobs}.get(a, a) for a in cmd[1:]]
        start = time.time()
        r = subprocess.run(argv, capture_output=True, text=True)
        summary = (r.stdout.splitlines() or ["(no output)"])[0]
        if r.returncode not in (0, 1):
            rows.append((name, "FEHLER", "-", "-", summary))
            new[name] = [(r.stderr.strip().splitlines() or ["?"])[-1]]
            continue
        got = findings(r.stdout, name)
        dump[name] = got
        known = {e["finding"] for e in reviewed.get(name, [])}
        fresh = [f for f in got if f not in known]
        gone = sorted(known - set(got))
        if fresh:
            new[name] = fresh
        if gone:
            stale[name] = gone
        rows.append((name, len(got), len(got) - len(fresh), len(fresh), f"{summary} ({time.time() - start:.0f} s)"))
    print("| Werkzeug | Funde | geprüft (reviewed.json) | neu |")
    print("|---|---:|---:|---:|")
    for name, n, ok, fresh, _ in rows:
        print(f"| `{name}` | {n} | {ok} | {fresh} |")
    print()
    for name, n, ok, fresh, summary in rows:
        print(f"{name}: {summary}")
    for name, lines in new.items():
        print(f"NEU in {name}:")
        for line in lines:
            print(f"  {line}")
    for name, lines in stale.items():
        print(f"NICHT MEHR GEFUNDEN in {name} (Eintrag in reviewed.json entfernen):")
        for line in lines:
            print(f"  {line}")
    if args.dump:
        Path(args.dump).write_text(json.dumps(dump, indent=1, ensure_ascii=False))
    return 1 if new or stale else 0


if __name__ == "__main__":
    sys.exit(main())
