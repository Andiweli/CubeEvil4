#!/usr/bin/env python3
"""Self-test of the milestone 12 checks (port/tools/m12): every check must find the example
of its error form in forms.cpp / forms_b.cpp (lines marked `// expect: CHECK[=NAME]`) and
nothing else, in particular none of the reviewed forms (function `reviewed`: RE4_OFS,
RE4_OWN, RE4_LP64_SIZE, RE4_ADDR32, sizeof).

The two units are compiled as a tiny prepared tree for armhf (ILP32, the reference) and
x86_64 (LP64) with the prelude of the port (re4_whole_prelude.h), in a temporary folder;
each check then runs on that pair as on the real builds.

    port/tools/m12/selftest/run_selftest.py [--keep DIR]

Exit code 0 when every check finds exactly what is marked. Needs clang, the armhf headers
(whole_setup_linux.sh) and the libclang Python bindings (pip install libclang==18.1.1).
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
M12 = HERE.parent
ROOT = HERE.parents[3]
UNITS = ["forms.cpp", "forms_b.cpp"]
TARGETS = {"armhf": "arm-linux-gnueabihf", "x86_64": "x86_64-linux-gnu"}
# check -> command line (REF / LP64 / PAT replaced)
CHECKS = {
    "int_cast_check": ["int_cast_check.py", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "ptr_slot_check": ["ptr_slot_check.py", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "ptr_word_check": ["ptr_word_check.py", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "cast_check": ["cast_check.py", "--ref", "REF", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "raw_offset_check": ["raw_offset_check.py", "--ref", "REF", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "raw_offset_words": ["raw_offset_check.py", "--words", "--ref", "REF", "--lp64", "LP64", "--pattern", "PAT",
                         "--jobs", "1"],
    "alloc_size_check": ["alloc_size_check.py", "--ref", "REF", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "span_check": ["span_check.py", "--ref", "REF", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "array_view_check": ["array_view_check.py", "--ref", "REF", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "overlay_check": ["overlay_check.py", "--ref", "REF", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "overlay_areas": ["overlay_check.py", "--areas", "--ref", "REF", "--lp64", "LP64", "--pattern", "PAT", "--jobs",
                      "1"],
    "view_check": ["view_check.py", "--ref", "REF", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "proto_check": ["proto_check.py", "--ref", "REF", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "decl_size_check": ["decl_size_check.py", "LP64", "--jobs", "1"],
    "sizeof_ptr_check": ["sizeof_ptr_check.py", "--ref", "REF", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "work_view_check": ["work_view_check.py", "--ref", "REF", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "copy_view_check": ["copy_view_check.py", "--ref", "REF", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "reloc_check": ["reloc_check.py", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
    "own_start_check": ["own_start_check.py", "--lp64", "LP64", "--pattern", "PAT", "--jobs", "1"],
}


def make_tree(base):
    builds = {}
    for name, triple in TARGETS.items():
        build = base / name
        tree = build / "prepared" / "tree" / "src" / "selftest"
        tree.mkdir(parents=True)
        cc = []
        for u in UNITS:
            shutil.copyfile(HERE / u, tree / u)
            f = tree / u
            cmd = (f"/usr/bin/clang++ --target={triple} -I{ROOT}/port/whole/include "
                   f"-include {ROOT}/port/whole/include/re4_whole_prelude.h -std=gnu++11 -fms-extensions "
                   f"-Wno-everything -o {f}.o -c {f}")
            cc.append({"directory": str(build), "command": cmd, "file": str(f)})
            r = subprocess.run(cmd.split()[:-4] + ["-fsyntax-only", str(f)], capture_output=True, text=True)
            if r.returncode != 0:
                sys.exit(f"selftest: {u} does not compile for {name}:\n{r.stderr}")
        (build / "compile_commands.json").write_text(json.dumps(cc, indent=1))
        builds[name] = build
    return builds


def expected():
    """check -> set of expectations: ("line", unit, n) or ("name", NAME)"""
    out = {}
    for u in UNITS:
        for n, line in enumerate((HERE / u).read_text().splitlines(), 1):
            m = re.search(r"// expect: ([\w=\- ]+?)(?:\s*\(|$)", line)
            if not m:
                continue
            for item in m.group(1).split():
                check, _, name = item.partition("=")
                out.setdefault(check, set()).add(("name", name) if name else ("line", u, n))
    return out


def findings(output):
    """the report lines of a check (indented lines after its summary line)"""
    return [line for line in output.splitlines() if line.startswith("  ")]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--keep", help="build the tree in this folder and keep it")
    args = ap.parse_args()
    base = Path(args.keep) if args.keep else Path(tempfile.mkdtemp(prefix="m12-selftest-"))
    if args.keep and base.exists():
        shutil.rmtree(base)
    base.mkdir(parents=True, exist_ok=True)
    builds = make_tree(base)
    want = expected()
    checks = dict(CHECKS)
    bad = 0
    for check, cmd in checks.items():
        argv = [sys.executable, str(M12 / cmd[0])] + [
            {"REF": str(builds["armhf"]), "LP64": str(builds["x86_64"]),
             "PAT": r"/prepared/tree/src/selftest/.*\.cpp$"}.get(a, a) for a in cmd[1:]]
        r = subprocess.run(argv, capture_output=True, text=True)
        lines = findings(r.stdout)
        got_lines = {(u, int(n)) for line in lines for u, n in re.findall(r"(forms(?:_b)?\.cpp):(\d+)", line)}
        exp = want.get(check, set())
        missing, unexpected = [], []
        for e in sorted(exp):
            if e[0] == "line":
                if (e[1], e[2]) not in got_lines:
                    missing.append(f"{e[1]}:{e[2]}")
            elif not any(re.search(r"\b" + re.escape(e[1]) + r"\b", line) for line in lines):
                missing.append(e[1])
        exp_lines = {(e[1], e[2]) for e in exp if e[0] == "line"}
        names = [e[1] for e in exp if e[0] == "name"]
        for line in lines:
            ok = any(re.search(r"\b" + re.escape(nm) + r"\b", line) for nm in names) or \
                any((u, int(n)) in exp_lines for u, n in re.findall(r"(forms(?:_b)?\.cpp):(\d+)", line))
            if not ok:
                unexpected.append(line.strip())
        status = "ok" if not missing and not unexpected and r.returncode in (0, 1) else "FEHLER"
        bad += status != "ok"
        print(f"{check}: {status} ({len(lines)} findings, {len(exp)} expected)")
        for m in missing:
            print(f"  missing: {m}")
        for u in unexpected:
            print(f"  unexpected: {u}")
        if r.returncode not in (0, 1):
            print("  " + (r.stderr.strip().splitlines() or ["(no output)"])[-1])
    if not args.keep:
        shutil.rmtree(base, ignore_errors=True)
    print(f"selftest: {len(checks) - bad} of {len(checks)} checks find exactly their examples")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
