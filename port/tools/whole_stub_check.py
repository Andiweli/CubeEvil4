#!/usr/bin/env python3
"""The generated stubs of a build against port/whole/stubs-expected.txt.

    whole_stub_check.py LINK_REPORT [--android] [--write]

LINK_REPORT: BUILD/generated/link_report.json. A stub the list does not name
is an error (something the game needs is missing on this target). A listed
stub the build no longer has is an error for the Linux builds (the list is
out of date) and only a note with --android (whole_build_android.sh: the
NDK may provide a function the Linux C library does not). --write brings the
list up to date: vanished entries are dropped, new ones appended at the end
(give them a comment by hand). Exit code 0 when nothing is wrong.
"""
import argparse
import json
import sys
from pathlib import Path

LIST = Path(__file__).resolve().parents[1] / "whole" / "stubs-expected.txt"


def read_list():
    names = set()
    for line in LIST.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            kind, name = line.split()
            names.add((kind, name))
    return names


def read_report(path):
    s = json.loads(Path(path).read_text())["stubs"]
    if not isinstance(s, dict):
        s = {"functions": s, "data": []}
    return {("function", n) for n in s["functions"]} | {("data", n) for n in s.get("data", [])}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("report")
    ap.add_argument("--android", action="store_true")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    have, want = read_report(args.report), read_list()
    extra, gone = sorted(have - want), sorted(want - have)
    if args.write:
        out = []
        for line in LIST.read_text().splitlines():
            parts = line.split()
            if len(parts) == 2 and not line.startswith("#") and tuple(parts) in set(gone):
                continue
            out.append(line)
        if extra:
            out += ["", "# added by whole_stub_check.py --write (say why)"] + [f"{k} {n}" for k, n in extra]
        LIST.write_text("\n".join(out) + "\n")
        print(f"stubs: {LIST} updated ({len(extra)} added, {len(gone)} removed)")
        return 0
    functions = sum(1 for k, _ in have if k == "function")
    print(f"  stubs: {functions} functions, {len(have) - functions} data; "
          f"{'as expected' if not extra and not gone else 'DIFFERENT from'} port/whole/stubs-expected.txt")
    for kind, name in extra:
        print(f"  FEHLER: stub not in stubs-expected.txt ({kind}): {name}")
    for kind, name in gone:
        print(f"  {'note' if args.android else 'FEHLER'}: no longer a stub ({kind}): {name}"
              f"{'' if args.android else ' (update stubs-expected.txt: --write)'}")
    return 1 if extra or (gone and not args.android) else 0


if __name__ == "__main__":
    sys.exit(main())
