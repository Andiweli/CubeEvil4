#!/usr/bin/env python3
"""Bug hunt (22.10.2026): values the game code uses before it sets them, by the compiler.

The decompiled code keeps places where the original took a value from a processor register or a
stack slot the C code never sets (`register int rb asm("r9")` after whole_prepare.py drops the
pin, a helper without the parameter it is called with, a value set in one case only): on the
GameCube that value was whatever the caller left, in the port it is anything. Two passes over the
game code of a build (compile_commands.json, the prepared tree, src/ without src/lib):

    warnings   clang -Wuninitialized -Wsometimes-uninitialized -Wconditional-uninitialized
               (-fsyntax-only, about 20 s)
    analyzer   clang's static analyzer, core checkers (uninitialized values, undefined operands,
               calls with unset arguments; also null pointers and divisions by zero), path by
               path (about 45 min on 4 cores)

    port/tools/scan/uninit_scan.py BUILD [warnings|analyzer] [--jobs N] [--out FILE]

The locations are the real files (not the #line names of the decompilation). Findings of the
22.10.2026 run and what became of them: port/docs/whole-program-status.md, "Fehlersuche".
"""

import argparse
import concurrent.futures
import json
import shlex
import subprocess
import sys

WARNINGS = ["-fsyntax-only", "-Wno-everything", "-Wuninitialized", "-Wsometimes-uninitialized",
            "-Wconditional-uninitialized"]
ANALYZER = ["--analyze", "-Xclang", "-analyzer-checker=core.uninitialized,core.UndefinedBinaryOperatorResult,"
            "core.CallAndMessage", "-Xclang", "-analyzer-disable-checker=deadcode,unix,cplusplus,nullability,"
            "security,osx,apiModeling.google", "-Xanalyzer", "-analyzer-output=text", "-o", "/dev/null",
            "-Wno-everything"]


def command(entry, extra):
    args = shlex.split(entry["command"]) if "command" in entry else list(entry["arguments"])
    cmd, skip = [args[0]], False
    for a in args[1:]:
        if skip:
            skip = False
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a in ("-c", "-w") or a == entry["file"]:
            continue
        cmd.append(a)
    return cmd + extra + ["-Xclang", "-fno-diagnostics-use-presumed-location", entry["file"]]


def run(job):
    entry, extra = job
    try:
        r = subprocess.run(command(entry, extra), cwd=entry["directory"], capture_output=True, text=True,
                           timeout=1800)
        return r.stderr
    except subprocess.TimeoutExpired:
        return f"{entry['file']}: TIMEOUT\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build")
    ap.add_argument("mode", nargs="?", default="warnings", choices=("warnings", "analyzer"))
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--out")
    a = ap.parse_args()
    entries = [e for e in json.load(open(f"{a.build}/compile_commands.json"))
               if "/prepared/tree/src/" in e["file"] and "/src/lib/" not in e["file"]]
    extra = WARNINGS if a.mode == "warnings" else ANALYZER
    out = open(a.out, "w") if a.out else sys.stdout
    found = 0
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        for text in ex.map(run, [(e, extra) for e in entries]):
            found += text.count(" warning: ")
            out.write(text)
            out.flush()
    print(f"uninit_scan {a.mode}: {found} findings in {len(entries)} units", file=sys.stderr)


if __name__ == "__main__":
    main()
