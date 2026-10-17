#!/usr/bin/env python3
"""Milestone 12, way A: C functions whose units disagree on a parameter's structure.

C has no type-safe linkage. The decompiled middleware often declares a
callee with its own view of a structure (mwsfdcre.c passes a
MWSFD_SFDCREPRM* to SFD_Create, which sfd_ply.c defines with SFD_CREPRM*).
On ILP32 the views agree byte for byte; on LP64 one of them may have a
pointer where the other has an int, and the callee reads the caller's data
at other places.

With libclang this check reads every C unit (and extern "C" declarations of
C++ units) of an LP64 build and lists functions whose declarations in
different units point to differently named structures, or to structures of
one name with another LP64 layout, for the same parameter or return value.

    port/tools/m12/proto_check.py [build] [--pattern REGEX]

Needs the libclang Python bindings (pip install libclang==18.1.1).
"""

import argparse
import collections
import concurrent.futures
import json
import re
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


def layout(t):
    """(size, ((field, offset bits), ...)) of a record type"""
    import clang.cindex as ci
    d = t.get_declaration()
    fields = tuple((f.spelling, f.get_field_offsetof()) for f in d.get_children()
                   if f.kind == ci.CursorKind.FIELD_DECL)
    return t.get_size(), fields


def record_of(t):
    import clang.cindex as ci
    t = t.get_canonical()
    while t.kind == ci.TypeKind.POINTER:
        t = t.get_pointee().get_canonical()
    if t.kind == ci.TypeKind.RECORD:
        return t
    return None


def scan(job):
    import clang.cindex as ci
    entry, resource_dir = job
    tu = ci.Index.create().parse(entry["file"], args=args_of(entry, resource_dir))
    out = []
    unit = entry["file"].split("/prepared/tree/")[-1]

    def visit(c):
        for k in c.get_children():
            if k.kind == ci.CursorKind.FUNCTION_DECL and k.linkage == ci.LinkageKind.EXTERNAL:
                is_c = k.mangled_name == k.spelling or not k.mangled_name
                if is_c:
                    slots = [("ret", k.result_type)] + [(f"arg{i}", a.type) for i, a in enumerate(k.get_arguments())]
                    for slot, t in slots:
                        r = record_of(t)
                        if r is not None and r.get_size() > 0:
                            name = re.sub(r"^(struct|union|const|volatile) ", "", r.spelling)
                            out.append((k.spelling, slot, name, layout(r), unit))
            elif k.kind in (ci.CursorKind.LINKAGE_SPEC, ci.CursorKind.UNEXPOSED_DECL, ci.CursorKind.NAMESPACE):
                visit(k)

    visit(tu.cursor)
    return out


def collect(build, pattern, jobs_n, resource_dir):
    cc = json.loads((Path(build) / "compile_commands.json").read_text())
    seen, jobs = set(), []
    for e in cc:
        if e["file"] in seen or not re.search(pattern, e["file"]):
            continue
        seen.add(e["file"])
        jobs.append((e, resource_dir))
    out = {}
    with concurrent.futures.ProcessPoolExecutor(jobs_n) as pool:
        for res in pool.map(scan, jobs, chunksize=4):
            for fn, slot, name, lay, unit in res:
                out[(fn, slot, unit)] = (name, lay)
    return out, len(jobs)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default=str(ROOT / "port/build/whole-armhf"))
    ap.add_argument("--lp64", default=str(ROOT / "port/build/whole-x86_64"))
    ap.add_argument("--pattern", default=r"/prepared/tree/src/.*\.(c|cpp)$")
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    ra, n = collect(args.ref, args.pattern, args.jobs, resource_dir)
    rb, _ = collect(args.lp64, args.pattern, args.jobs, resource_dir)
    views = collections.defaultdict(list)  # (fn, slot) -> [(unit, name, {ilp32 off: lp64 off})]
    for key, (name, (sa, fa)) in ra.items():
        if key not in rb:
            continue
        nb, (sb, fb) = rb[key]
        lb = dict(fb)
        m = {oa // 8: lb[f] // 8 for f, oa in fa if f in lb and oa >= 0 and lb[f] >= 0}
        views[key[:2]].append((key[2], name, m))
    bad = []
    for (fn, slot), vs in sorted(views.items()):
        if len({v[1] for v in vs}) < 2 and len(vs) < 2:
            continue
        ref = {}
        for unit, name, m in vs:
            for a, b in m.items():
                ref.setdefault(a, set()).add((b, name, unit))
        clash = {a: v for a, v in ref.items() if len({b for b, _, _ in v}) > 1}
        if clash:
            a = min(clash)
            desc = ", ".join(f"{name} {b:#x} ({unit})" for b, name, unit in sorted(clash[a])[:3])
            bad.append(f"{fn} {slot}: ILP32 offset {a:#x} lies at {desc}")
    print(f"proto_check: {n} units, {len(views)} structure parameters, {len(bad)} with views that differ on LP64")
    for line in bad:
        print("  " + line)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
