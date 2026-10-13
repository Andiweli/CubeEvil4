#!/usr/bin/env python3
"""Whole-program compile probe.

Compiles every original game source outside src/lib (the Nintendo SDK and CRI
middleware, which the port replaces) for ARM32 and ARM64 with clang in
syntax-only mode, and reports how many files compile and which kinds of errors
remain. Nothing in src/ or include/ is changed: sources are read, lightly
rewritten in memory (asm volatile -> asm) and compiled from a scratch copy.

Usage: probe_full_compile.py [--out DIR] [--jobs N] [--only SUBSTR]
"""
import argparse
import collections
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROBE = ROOT / "port" / "probe"
TARGETS = {"arm32": "armv7a-none-eabi", "arm64": "aarch64-none-elf"}
ASM_VOLATILE = re.compile(r"\b(asm|__asm__|__asm)\s+(volatile|__volatile__)\b")
# GameCube-era size parameters of the allocation operators are u32/unsigned int;
# the C++ ABI on LP64 requires size_t.
OPERATOR_SIZE = re.compile(r"(operator\s+(?:new|delete)\s*(?:\[\])?\s*\([^)]*?)\bunsigned\s+int\b")
ERROR = re.compile(r"^(?P<file>[^:\n]+):(?P<line>\d+):\d+: error: (?P<msg>.*)$", re.M)


def mirror(src: Path, dst: Path) -> None:
    for path in src.rglob("*"):
        if path.is_dir():
            continue
        out = dst / path.relative_to(src)
        out.parent.mkdir(parents=True, exist_ok=True)
        if path.suffix in (".c", ".cpp", ".h", ".hpp", ".inc"):
            text = path.read_text(encoding="utf-8", errors="replace")
            text = ASM_VOLATILE.sub(r"\1", text)
            text = OPERATOR_SIZE.sub(r"\1__SIZE_TYPE__", text)
            out.write_text(text, encoding="utf-8")
        else:
            shutil.copyfile(path, out)


def classify(msg: str) -> str:
    msg = re.sub(r"'[^']*'", "'X'", msg)
    msg = re.sub(r"\d+", "N", msg)
    return msg


def compile_one(work: Path, rel: Path, target: str):
    is_cpp = rel.suffix == ".cpp"
    cmd = [
        "clang++" if is_cpp else "clang",
        f"--target={target}", "-fsyntax-only", "-nostdinc",
        "-ferror-limit=0", "-w", "-Wno-address-of-temporary",
        "-include", str(PROBE / "prelude.h"),
        "-I", str(PROBE / "include"),
        "-I", str(work / "include"),
        "-I", str(work / "include" / "libc"),
        "-I", str(work / rel.parent),
    ]
    cmd += ["-nostdinc++", "-std=gnu++98",
            "-fdelayed-template-parsing"] if is_cpp else ["-std=gnu89"]
    cmd.append(str(work / rel))
    proc = subprocess.run(cmd, capture_output=True, text=True)
    errors = [(m["file"].replace(str(work) + "/", ""), int(m["line"]), m["msg"])
              for m in ERROR.finditer(proc.stderr)]
    return str(rel), errors


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=ROOT / "port" / "build" / "probe")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--only", default="")
    args = ap.parse_args()

    work = args.out / "tree"
    if work.exists():
        shutil.rmtree(work)
    mirror(ROOT / "src", work / "src")
    mirror(ROOT / "include", work / "include")

    sources = sorted(p.relative_to(work) for p in (work / "src").rglob("*")
                     if p.suffix in (".c", ".cpp")
                     and not p.relative_to(work / "src").parts[0] == "lib"
                     and args.only in str(p))
    report = {}
    for abi, target in TARGETS.items():
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
            results = list(pool.map(lambda r: compile_one(work, r, target), sources))
        failed = {f: e for f, e in results if e}
        kinds = collections.Counter()
        where = collections.Counter()
        unique = {}
        for errs in failed.values():
            for file, line, msg in errs:
                unique[(file, line, msg)] = True
        for file, line, msg in unique:
            kinds[classify(msg)] += 1
            where[file] += 1
        report[abi] = {
            "files": len(sources),
            "files_clean": len(sources) - len(failed),
            "unique_errors": len(unique),
            "error_kinds": kinds.most_common(),
            "error_locations": where.most_common(),
            "failed_files": {f: len(e) for f, e in sorted(failed.items())},
        }
        print(f"{abi}: {len(sources) - len(failed)}/{len(sources)} files clean, "
              f"{len(unique)} unique errors")
        for kind, n in kinds.most_common(15):
            print(f"  {n:6d}  {kind}")
    (args.out / "report.json").write_text(json.dumps(report, indent=1))
    print(f"report: {args.out / 'report.json'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
