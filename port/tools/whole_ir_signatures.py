#!/usr/bin/env python3
"""Calls that rely on the GameCube's registers: declarations against definitions.

The decompiled C sources (SDK, CRI middleware) sometimes declare a function
with fewer parameters than its definition, or without a result where the
definition returns one. On the GameCube that worked by chance: the missing
argument was whatever the caller had left in r4..r10, a result nobody
collected stayed in r3. With another compiler and processor the value is
garbage. Every unit of a build is compiled to LLVM IR (the build's own
compile commands, -O0), every `declare` of a C function (unmangled name) is
compared with its `define` in another unit: number of parameters and
whether there is a result. Mismatches are printed with the units involved:
first those that read garbage (fewer parameters declared than defined, a
result declared that the definition does not return), then the harmless
ones (extra arguments, a result nobody reads). The generated stubs
(generated/stubs.cpp, no parameters) are not definitions here.

    whole_ir_signatures.py BUILD_DIR [--filter REGEX] [--jobs N]

BUILD_DIR: a whole-program build (compile_commands.json, e.g.
port/build/whole-i686). --filter: only declarations in units whose path
matches (e.g. 'src/lib/'). Definitions are taken from all units. Exit code 1
when a call reads garbage. C++ names are mangled with their parameter types
and are left out (a mismatch there does not link).
"""
import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

LINE = re.compile(r'^(define|declare)\s+(.*?)\s*@("[^"]+"|[\w.$]+)\((.*)\)(.*)$')
LINKAGE = {"internal", "private", "linkonce_odr", "weak_odr", "available_externally", "linkonce", "weak"}


def split_params(text):
    depth, cur, out = 0, "", []
    for ch in text:
        if ch in "({[<":
            depth += 1
        elif ch in ")}]>":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def parse(ll):
    """{name: (kind, result type, parameter count or None for '...', linkage)}"""
    funcs = {}
    for line in ll.splitlines():
        m = LINE.match(line)
        if not m:
            continue
        kind, head, name, params, _ = m.groups()
        name = name.strip('"')
        if name.startswith("_Z") or name.startswith("llvm."):
            continue
        words = head.split()
        result = words[-1] if words else "void"
        linkage = next((w for w in words if w in LINKAGE), "")
        plist = split_params(params)
        count = None if plist and plist[-1] == "..." else len(plist)
        if plist and plist[-1] == "..." and len(plist) > 1:
            count = ("varargs", len(plist) - 1)
        funcs[name] = (kind, result, count, linkage)
    return funcs


def to_ir(index, entry, out_dir):
    if "arguments" in entry:
        args = list(entry["arguments"])
    else:
        args = shlex.split(entry["command"])
    src = entry["file"]
    cmd, skip = [], False
    for a in args:
        if skip:
            skip = False
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a in ("-c", "-MD", "-MMD") or a.startswith("-O") or a.startswith("-g"):
            continue
        cmd.append(a)
    out = Path(out_dir) / f"{index}.ll"
    cmd += ["-S", "-emit-llvm", "-O0", "-Xclang", "-disable-llvm-passes", "-o", str(out)]
    proc = subprocess.run(cmd, cwd=entry.get("directory", "."), capture_output=True)
    if proc.returncode != 0 or not out.exists():
        return src, None
    text = out.read_text(errors="replace")
    out.unlink()
    return src, parse(text)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("build")
    ap.add_argument("--filter", default="")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    args = ap.parse_args()
    entries = json.loads((Path(args.build) / "compile_commands.json").read_text())
    with tempfile.TemporaryDirectory(prefix="re4-ir-") as tmp:
        with ThreadPoolExecutor(args.jobs) as pool:
            units = list(pool.map(lambda ie: to_ir(ie[0], ie[1], tmp), enumerate(entries)))
    failed = [src for src, f in units if f is None]
    defs = {}
    for src, funcs in units:
        for name, (kind, result, count, linkage) in (funcs or {}).items():
            if kind == "define" and linkage not in ("internal", "private") and "generated/stubs" not in src:
                defs.setdefault(name, (result, count, src))
    keep = re.compile(args.filter) if args.filter else None
    issues = []
    for src, funcs in units:
        if funcs is None or (keep and not keep.search(src)):
            continue
        for name, (kind, result, count, linkage) in funcs.items():
            if kind != "declare" or name not in defs:
                continue
            dres, dcount, dsrc = defs[name]
            if count is None or dcount is None:
                continue  # unprototyped / '...' on one side: nothing to compare
            problems, garbage = [], False
            n_decl = count[1] if isinstance(count, tuple) else count
            n_def = dcount[1] if isinstance(dcount, tuple) else dcount
            if n_decl != n_def:
                problems.append(f"{n_decl} parameters declared, {n_def} defined")
                garbage |= n_decl < n_def
            if (result == "void") != (dres == "void"):
                problems.append(f"result {result} declared, {dres} defined")
                garbage |= dres == "void"
            if problems:
                issues.append((not garbage, name, src, dsrc, "; ".join(problems)))
    root = str(Path(args.build).resolve()) + "/"
    short = lambda p: p.replace(root, "").replace(str(Path(__file__).resolve().parents[2]) + "/", "").split(
        "/prepared/tree/")[-1]
    bad = [i for i in issues if not i[0]]
    print(f"reads garbage ({len(bad)}):")
    for harmless, name, src, dsrc, what in sorted(set(issues)):
        if not harmless:
            print(f"  {name}: {what} (declared in {short(src)}, defined in {short(dsrc)})")
    print(f"harmless ({len(issues) - len(bad)}):")
    for harmless, name, src, dsrc, what in sorted(set(issues)):
        if harmless:
            print(f"  {name}: {what} (declared in {short(src)}, defined in {short(dsrc)})")
    print(f"ir signatures: {len(entries)} units, {len(set(failed))} not compiled, {len(defs)} C definitions, "
          f"{len(bad)} reading garbage, {len(issues) - len(bad)} harmless")
    for src in sorted(set(failed))[:5]:
        print(f"  not compiled: {short(src)}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
