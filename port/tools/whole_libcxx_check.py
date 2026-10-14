#!/usr/bin/env python3
"""The port's own runtime files against libc++, the C++ library of the Android NDK.

The Linux test builds use libstdc++; the Android build (whole_build_android.sh)
uses libc++. Port files compiled with the game's flags and prelude (the
Formatumwandler, the core bridge, AX ...) can break libc++'s headers where
libstdc++'s still work (14.10.2026: std::vector in formats_event.cpp). This
check compiles every file under port/whole/runtime of a Linux build with
-fsyntax-only -stdlib=libc++, using the build's own command lines.

    whole_libcxx_check.py BUILD_DIR      exit 0 when every file passes
"""
import glob
import json
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


def main() -> int:
    build = Path(sys.argv[1])
    if not glob.glob("/usr/lib/llvm-*/include/c++/v1/vector") and not glob.glob("/usr/include/c++/v1/vector"):
        print("libc++ check: libc++ headers missing (libc++-dev), not checked")
        return 0
    entries = [e for e in json.loads((build / "compile_commands.json").read_text())
               if "/port/whole/runtime/" in e["file"]]

    def check(e):
        args = shlex.split(e["command"]) if "command" in e else list(e["arguments"])
        if "-o" in args:
            i = args.index("-o")
            args[i + 1] = "/dev/null"
        args += ["-fsyntax-only", "-stdlib=libc++"] if not e["file"].endswith(".c") else ["-fsyntax-only"]
        r = subprocess.run(args, cwd=e["directory"], capture_output=True, text=True)
        errors = [l for l in r.stderr.splitlines() if ": error:" in l or "fatal error:" in l]
        return e["file"], r.returncode, errors

    with ThreadPoolExecutor() as pool:
        results = list(pool.map(check, entries))
    bad = [(f, errs) for f, code, errs in results if code != 0]
    print(f"libc++ check (the NDK's C++ library): {len(results) - len(bad)} of {len(results)} runtime files ok")
    for f, errs in bad:
        print(f"  {Path(f).relative_to(Path(f).parents[3]) if len(Path(f).parents) > 3 else f}:")
        for line in sorted(set(errs))[:5]:
            print(f"    {line}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
