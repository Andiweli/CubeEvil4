#!/usr/bin/env python3
"""Milestone 12: do views of one object (different names, edited copies) agree on LP64?
usage: groupcheck.py COPYDIR unit:record:base ...

Part of the LP64 views of milestone 12 (whole-program-status.md, "M12 ARM64");
the armhf and x86_64 builds must be configured.
"""
import json, os, re, shlex, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import fixed_layout_check as F
ROOT = Path(__file__).resolve().parents[3]
ROW = re.compile(r"^\s*(\d+)(?::\d+-\d+)? \|( *)(.*)$")
copydir = sys.argv[1]


def dump(build, unit):
    cc = json.load(open(str(ROOT / f'port/build/whole-{build}/compile_commands.json')))
    e = next(x for x in cc if x['file'].endswith('/prepared/tree/' + unit))
    cmd = F.command(e)
    copy = os.path.join(copydir, unit.replace('src/lib/', ''))
    if os.path.exists(copy):
        cmd = [copy if a == e['file'] else a for a in cmd]
        cmd.insert(1, '-I' + os.path.dirname(e['file']))
    cmd.insert(1, '-I' + os.path.join(copydir, 'cri'))
    p = subprocess.run(cmd, cwd=e.get('directory', '.'), capture_output=True, text=True)
    if p.returncode:
        print(p.stderr[-1500:])
    return p.stdout.splitlines()


def named(lines, rec):
    for i, l in enumerate(lines):
        if not l.startswith('*** Dumping') or i + 1 >= len(lines):
            continue
        m = ROW.match(lines[i + 1])
        if not m or F.clean(re.sub(r"^(struct|class|union) ", "", m.group(3).strip())) != rec:
            continue
        out, stack = {}, []
        for l2 in lines[i + 2:]:
            if l2.startswith('*** Dumping') or 'sizeof=' in l2:
                break
            r = ROW.match(l2)
            if not r:
                continue
            off, depth, text = int(r.group(1)), (len(r.group(2)) - 1) // 2, r.group(3).strip()
            name = text.rsplit(' ', 1)[-1]
            while stack and stack[-1][0] >= depth:
                stack.pop()
            path = '.'.join([s[1] for s in stack] + [name])
            out[path] = off
            stack.append((depth, name))
        return out
    return {}


views = [(u, r, int(b, 0)) for u, r, b in (v.split(':') for v in sys.argv[2:])]
gc2lp = {}
for unit, rec, base in views:
    a, b = named(dump('armhf', unit), rec), named(dump('x86_64', unit), rec)
    for path, off in a.items():
        if path in b and not re.search(r"(^|\.)pad\w*$|_p[0-9a-f]+$", path):
            gc2lp.setdefault(base + off, {}).setdefault(base + b[path] if base == 0 else None, []).append(
                f"{unit.split('/')[-1]}:{path}")
# views at a non-zero base: LP64 base = the LP64 offset of the same GC offset in a base-0 view
bad = 0
for gc in sorted(gc2lp):
    targets = {k: v for k, v in gc2lp[gc].items() if k is not None}
    if len(targets) > 1:
        bad += 1
        print(f"{gc:#06x}: " + "; ".join(f"{t:#x} {sorted(set(w))[:3]}" for t, w in sorted(targets.items())))
print(f"groupcheck: {len(gc2lp)} GameCube offsets, {bad} with different LP64 offsets")
