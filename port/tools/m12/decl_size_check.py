#!/usr/bin/env python3
"""Milestone 12, way A: global variables whose definition is smaller than their use.

The decompiled code sometimes defines a table in one unit as a raw byte array
of its GameCube size (adx_fini.c: `Uint8 adxf_obj[16 * 0x44]`) and declares
it with its real type in another (adx_fs.c: `extern ADXF_OBJ adxf_obj[16]`).
On ILP32 both have the same size. On LP64 a pointer member makes the real
type larger, and the users write past the definition.

For every unit of a build (compile_commands.json) this check reads, with
libclang, the size of each global variable's type at its definition and at
every `extern` declaration, and reports names where a declaration is larger
than the definition (or definitions of one name differ).

    port/tools/m12/decl_size_check.py [build] [--jobs N]

Needs the libclang Python bindings (pip install libclang==18.1.1); a cloud
development tool, not part of the build.
"""

import argparse
import collections
import concurrent.futures
import json
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def args_of(entry, resource_dir):
    args = shlex.split(entry["command"]) if "command" in entry else list(entry["arguments"])
    out, skip = [], False
    for a in args[1:]:
        if skip:
            skip = False
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a in ("-c", "-MD", "-MMD") or a == entry["file"]:
            continue
        out.append(a)
    return out + ["-resource-dir", resource_dir, "-w"]


def scan(job):
    import clang.cindex as ci
    entry, resource_dir = job
    index = ci.Index.create()
    try:
        tu = index.parse(entry["file"], args=args_of(entry, resource_dir))
    except ci.TranslationUnitLoadError:
        return entry["file"], [], 1
    res = []

    def visit(cur, depth):
        for c in cur.get_children():
            if c.kind == ci.CursorKind.VAR_DECL and c.semantic_parent is not None and \
                    c.semantic_parent.kind in (ci.CursorKind.TRANSLATION_UNIT, ci.CursorKind.NAMESPACE):
                size = c.type.get_size()
                if size > 0 and c.linkage == ci.LinkageKind.EXTERNAL:
                    loc = c.location
                    # C tentative definitions (`Uint8 x[n];` without extern) define too
                    is_def = c.is_definition() or c.storage_class != ci.StorageClass.EXTERN
                    res.append((c.mangled_name or c.spelling, is_def, size,
                                f"{loc.file.name if loc.file else '?'}:{loc.line}"))
            elif c.kind in (ci.CursorKind.NAMESPACE, ci.CursorKind.LINKAGE_SPEC, ci.CursorKind.UNEXPOSED_DECL):
                visit(c, depth + 1)

    visit(tu.cursor, 0)
    errors = sum(1 for d in tu.diagnostics if d.severity >= ci.Diagnostic.Error)
    return entry["file"], res, errors


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("build", nargs="?", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    cc = json.loads((Path(args.build) / "compile_commands.json").read_text())
    seen, jobs = set(), []
    for e in cc:
        f = e["file"]
        if f in seen or "/prepared/" not in f and "/runtime/" not in f:
            continue
        seen.add(f)
        jobs.append((e, resource_dir))
    defs = collections.defaultdict(set)
    decls = collections.defaultdict(set)
    failed = 0
    with concurrent.futures.ProcessPoolExecutor(args.jobs) as pool:
        for f, res, errors in pool.map(scan, jobs, chunksize=4):
            failed += 1 if errors else 0
            for name, is_def, size, where in res:
                (defs if is_def else decls)[name].add((size, where.split("/prepared/tree/")[-1]))
    bad = []
    for name, d in defs.items():
        dsizes = {s for s, _ in d}
        if len(dsizes) > 1:
            bad.append(f"{name}: definitions of {sorted(dsizes)} bytes: " + ", ".join(sorted({w for _, w in d})[:3]))
        big = [(s, w) for s, w in decls.get(name, ()) if s > min(dsizes)]
        if big:
            s, w = max(big)
            bad.append(f"{name}: defined with {min(dsizes)} bytes ({sorted(d)[0][1]}), declared with {s} ({w})")
    print(f"decl_size_check: {len(jobs)} units ({failed} with errors), {len(defs)} defined globals, "
          f"{len(bad)} mismatches")
    for line in sorted(bad):
        print("  " + line)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
