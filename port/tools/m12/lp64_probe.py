#!/usr/bin/env python3
"""What a 64-bit build of the whole program meets (milestone 12, inventory).

Takes the real compile commands of a whole-program build (compile_commands.json
of port/build/whole-armhf or whole-i686: the prepared tree with every rewrite
and patch, the replacement headers, the port's runtime), switches them to an
LP64 target and runs clang -fsyntax-only on every unit. Then sorts the
diagnostics: pointers cast to 32-bit integers (errors in C++, warnings in C),
32-bit integers cast to pointers, static size checks, and the rest by message.
x86_64 stands in for arm64-v8a: both are LP64 (pointers and long 8 bytes), the
pointer questions are the same; the cloud has no arm64 C library headers.

    port/tools/m12/lp64_probe.py [build folder] [--jobs N] [--out report.json] [--target T]

Writes the report (JSON: per unit, per message kind, the lines) and prints the
counts per kind and per area (game core, stage and enemy modules, SDK, CRI,
port runtime).
"""

import argparse
import collections
import concurrent.futures
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

KINDS = [
    ("ptr-to-int", re.compile(r"cast from pointer to smaller type|pointer to integer of different size|"
                              r"cast to smaller integer type .* from|-Wpointer-to-int-cast|"
                              r"-Wvoid-pointer-to-int-cast|-Wvoid-pointer-to-enum-cast|"
                              r"-Wpointer-to-enum-cast")),
    ("int-to-ptr", re.compile(r"cast to .* from smaller integer type|integer of different size to pointer|"
                              r"-Wint-to-pointer-cast|int-to-void-pointer-cast")),
    ("static-size", re.compile(r"static assertion failed|static_assert|array size is negative|"
                               r"'.*' declared as an array with a negative size")),
    ("narrowing", re.compile(r"non-constant-expression cannot be narrowed|cannot be narrowed")),
    ("long-mangling", re.compile(r"conflicting types for|functions that differ only in their return type|"
                                 r"no matching function for call|ambiguous")),
    ("new-delete", re.compile(r"operator new|operator delete|'size_t'")),
]

DIAG = re.compile(r"^(?P<file>[^:\n]+):(?P<line>\d+):(?P<col>\d+): (?P<sev>error|warning): (?P<msg>.*)$")


def area(path):
    p = path.replace("\\", "/")
    if "/prepared/tree/src/lib/" in p or "/prepared/sdk_c/" in p:
        return "SDK/CRI (src/lib)"
    m = re.search(r"/prepared/tree/src/([^/]+)/", p)
    if m:
        d = m.group(1)
        if d == "game":
            return "game"
        if d.startswith("st") and d[2:].isdigit():
            return "stage modules (st*)"
        if d.startswith("em") or d.startswith("Em"):
            return "enemy modules (em*)"
        if d.startswith("wep"):
            return "weapon modules (wep*)"
        return "other modules (%s)" % d if d in ("Sscrn", "Tools", "Event", "Option", "Title") else "other modules"
    if "/port/whole/runtime/" in p or "/port/src/" in p:
        return "port runtime"
    if "/generated/" in p:
        return "generated (link plan)"
    if "/android-decomp/" in p:
        return "app parts in the core"
    return "other"


TARGET = "x86_64-linux-gnu"
EXTRA = []


def lp64_command(entry, out_dir):
    if "arguments" in entry:
        args = list(entry["arguments"])
    else:
        args = shlex.split(entry["command"])
    out = []
    skip = False
    for a in args:
        if skip:
            skip = False
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a in ("-c", "-w", "-MD", "-MMD", "-m32"):
            continue
        if a.startswith("--target="):
            continue
        if a.startswith("-march=") or a.startswith("-mfpu=") or a.startswith("-mfloat-abi=") or a.startswith("-mthumb"):
            continue
        out.append(a)
    compiler = out[0]
    if not os.path.basename(compiler).startswith("clang"):
        compiler = "clang++" if entry["file"].endswith((".cpp", ".cc")) else "clang"
    is_c = entry["file"].endswith(".c")
    cmd = [compiler, "--target=" + TARGET, "-fsyntax-only", "-ferror-limit=0", "-fno-color-diagnostics",
           "-Wno-everything", "-Wpointer-to-int-cast", "-Wint-to-pointer-cast", "-Wvoid-pointer-to-int-cast",
           "-Wpointer-to-enum-cast", "-Wvoid-pointer-to-enum-cast", "-Wint-to-void-pointer-cast"]
    if is_c:
        cmd.append("-Wincompatible-pointer-types")
    cmd += out[1:] + EXTRA
    return cmd


def run_unit(entry, out_dir):
    cmd = lp64_command(entry, out_dir)
    try:
        p = subprocess.run(cmd, cwd=entry.get("directory", "."), capture_output=True, text=True, timeout=600)
    except subprocess.TimeoutExpired:
        return entry["file"], [("error", entry["file"], 0, "timeout")]
    diags = []
    for line in p.stderr.splitlines():
        m = DIAG.match(line)
        if m:
            diags.append((m.group("sev"), m.group("file"), int(m.group("line")), m.group("msg")))
    return entry["file"], diags


def kind_of(msg):
    for name, rx in KINDS:
        if rx.search(msg):
            return name
    return "other"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("build", nargs="?", default=str(ROOT / "port/build/whole-armhf"))
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--out", default=None)
    ap.add_argument("--target", default=TARGET,
                    help="clang target (default x86_64-linux-gnu; wasm32-wasi: the 32-bit sandbox for comparison)")
    ap.add_argument("--extra", default="", help="more compiler switches, e.g. -fms-extensions")
    args = ap.parse_args()
    globals()["TARGET"] = args.target
    globals()["EXTRA"] = shlex.split(args.extra)
    build = Path(args.build)
    cc = json.loads((build / "compile_commands.json").read_text())
    # one entry per source file (a unit can appear for several libraries)
    seen = set()
    entries = []
    for e in cc:
        if e["file"] in seen:
            continue
        seen.add(e["file"])
        if "third_party/boost_context" in e["file"] or e["file"].endswith((".S", ".s")):
            continue
        entries.append(e)
    results = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for f, diags in pool.map(lambda e: run_unit(e, build), entries):
            results[f] = diags

    # each distinct place once (a header line seen from many units counts once)
    places = {}
    units_with_errors = collections.Counter()
    for f, diags in results.items():
        has_error = False
        for sev, file, line, msg in diags:
            key = (file, line, msg)
            if key not in places:
                places[key] = (sev, kind_of(msg), area(file))
            has_error |= sev == "error"
        if has_error:
            units_with_errors[area(f)] += 1
    by_kind = collections.Counter()
    by_kind_sev = collections.Counter()
    by_area_kind = collections.defaultdict(collections.Counter)
    other_msgs = collections.Counter()
    for (file, line, msg), (sev, kind, ar) in places.items():
        by_kind[kind] += 1
        by_kind_sev[(kind, sev)] += 1
        by_area_kind[ar][kind] += 1
        if kind == "other":
            other_msgs[re.sub(r"'[^']*'", "'…'", msg)] += 1
    units_by_area = collections.Counter(area(f) for f in results)

    print("%s syntax check of %d units from %s" % (
        "LP64 (x86_64 for arm64-v8a)" if TARGET.startswith("x86_64") else TARGET, len(results), build))
    print()
    print("distinct places by kind (error / warning):")
    for kind, n in by_kind.most_common():
        print("  %-14s %5d  (%d errors, %d warnings)" % (kind, n, by_kind_sev[(kind, "error")],
                                                       by_kind_sev[(kind, "warning")]))
    print()
    print("by area: units, units with errors, places per kind")
    for ar in sorted(units_by_area):
        kinds = ", ".join("%s %d" % (k, n) for k, n in by_area_kind[ar].most_common())
        print("  %-28s %4d units, %4d with errors; %s" % (ar, units_by_area[ar], units_with_errors[ar],
                                                         kinds or "nothing"))
    print()
    print("other messages (most frequent):")
    for msg, n in other_msgs.most_common(25):
        print("  %5d  %s" % (n, msg[:160]))
    out = Path(args.out) if args.out else build / ("lp64-probe.json" if TARGET.startswith("x86_64") else
                                                    "probe-%s.json" % TARGET)
    report = {
        "build": str(build),
        "units": len(results),
        "by_kind": dict(by_kind),
        "by_area": {a: dict(c) for a, c in by_area_kind.items()},
        "places": [{"file": f, "line": l, "message": m, "severity": s, "kind": k, "area": a}
                   for (f, l, m), (s, k, a) in sorted(places.items())],
    }
    out.write_text(json.dumps(report, indent=1))
    print()
    print("report: %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
