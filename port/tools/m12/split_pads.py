#!/usr/bin/env python3
"""Milestone 12: split pads that hide pointers of other views (one object, several views).
usage: splitpads.py OUTDIR unit:record:base ...   (record views of one object, GameCube base offset)
Writes edited copies of the defining sources into OUTDIR (keeps existing copies there).

Part of the LP64 views of milestone 12 (whole-program-status.md, "M12 ARM64");
the armhf and x86_64 builds must be configured.
"""
import json, os, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import fixed_layout_check as F
ROOT = Path(__file__).resolve().parents[3]

TREE = str(ROOT / 'port/build/whole-armhf/prepared/tree') + '/'
ROW = re.compile(r"^\s*(\d+)(?::\d+-\d+)? \|( *)(.*)$")


def dump(unit):
    cc = json.load(open(str(ROOT / 'port/build/whole-armhf/compile_commands.json')))
    e = next(x for x in cc if x['file'].endswith('/prepared/tree/' + unit))
    p = subprocess.run(F.command(e), cwd=e.get('directory', '.'), capture_output=True, text=True)
    return p.stdout.splitlines()


def rows(lines, rec):
    """[(abs offset, depth, type text, member, owner type, owner base)]"""
    for i, l in enumerate(lines):
        if not l.startswith('*** Dumping') or i + 1 >= len(lines):
            continue
        m = ROW.match(lines[i + 1])
        if not m or F.clean(re.sub(r"^(struct|class|union) ", "", m.group(3).strip())) != rec:
            continue
        out, stack = [], [(0, rec, 0)]
        for l2 in lines[i + 2:]:
            if l2.startswith('*** Dumping') or 'sizeof=' in l2:
                break
            r = ROW.match(l2)
            if not r:
                continue
            off, depth, text = int(r.group(1)), (len(r.group(2)) - 1) // 2, r.group(3).strip()
            while stack and stack[-1][0] >= depth:
                stack.pop()
            owner, obase = (stack[-1][1], stack[-1][2]) if stack else (rec, 0)
            parts = text.rsplit(' ', 1)
            typ, name = (parts[0], parts[1]) if len(parts) == 2 else (text, '')
            out.append((off, depth, typ, name, owner, obase))
            tname = re.sub(r"^(struct|union) ", "", typ)
            if re.match(r"^[A-Za-z_]\w*$", tname) and tname not in ('Uint8', 'Sint8', 'Uint16', 'Sint16', 'Uint32',
                                                                    'Sint32', 'Float32', 'Float64', 'Sint64',
                                                                    'Uint64', 'char', 'int', 'short', 'long'):
                stack.append((depth, tname, off))
        return out
    return []


def pointer_slots(typ):
    """number of 4-byte pointer slots of a member type (0: none)"""
    if '(*' in typ or re.search(r"\*\s*$", typ) or re.search(r"\*\s*\[", typ):
        m = re.search(r"\[(\d+)\]", typ)
        return int(m.group(1)) if m else 1
    return 0


def main():
    outdir = sys.argv[1]
    views = [(u, r, int(b, 0)) for u, r, b in (v.split(':') for v in sys.argv[2:])]
    ptrs, pads = {}, []
    for unit, rec, base in views:
        lines = dump(unit)
        for off, depth, typ, name, owner, obase in rows(lines, rec):
            n = pointer_slots(typ)
            for k in range(n):
                ptrs.setdefault(base + off + 4 * k, set()).add(f"{unit}:{name}")
            m = re.match(r"^(Uint8|Sint8|char|u8|unsigned char)\[(\d+)\]$", typ)
            if m:
                pads.append((unit, owner, base + obase, base + off, int(m.group(2)), name))
    edits = {}
    for unit, owner, obase, start, size, name in pads:
        hidden = sorted(p for p in ptrs if start <= p < start + size)
        if not hidden:
            continue
        rel = [p - start for p in hidden]
        edits.setdefault((unit, owner), {})[name] = (size, rel, start - obase)
    for (unit, owner), members in sorted(edits.items()):
        src = None
        for cand in [unit] + ['src/lib/cri/' + f for f in os.listdir(TREE + 'src/lib/cri')]:
            text = open(TREE + cand).read()
            if re.search(r"\}\s*" + re.escape(owner) + r"\s*;", text):
                src = cand
                break
        if src is None:
            print("no definition of", owner, "for", unit)
            continue
        dst = os.path.join(outdir, src.replace('src/lib/', ''))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if not os.path.exists(dst):
            shutil.copy(TREE + src, dst)
        text = open(dst).read()
        m = re.search(r"typedef struct(?: \w+)? \{\n(.*?)\n\}\s*" + re.escape(owner) + r"\s*;", text, re.S)
        if not m:
            m = re.search(r"struct " + re.escape(owner) + r" \{\n(.*?)\n\}", text, re.S)
        body = m.group(1)
        new_body = body
        for name, (size, rel, ofs) in sorted(members.items()):
            lm = re.search(r"^([ \t]*)(?:Uint8|Sint8|char|u8)\s+" + re.escape(name) + r"\s*\[[^\]]*\]\s*;([^\n]*)$",
                           new_body, re.M)
            if not lm:
                print("pad line not found:", owner, name, "in", src)
                continue
            ind = lm.group(1)
            parts, at = [], 0
            for r in rel:
                if r > at:
                    parts.append(f"Uint8 {name}{'' if at == 0 else '_%x' % at}[{r - at:#x}];")
                elif at == 0 and r == 0:
                    pass
                parts.append(f"void *{name}_p{r:x};")
                at = r + 4
            if at < size:
                parts.append(f"Uint8 {name}_{at:x}[{size - at:#x}];")
            if rel and rel[0] == 0:
                # keep the member name for code that takes the pad's address
                parts[0] = parts[0].replace(f"void *{name}_p0;", f"void *{name};")
            repl = (f"#if __SIZEOF_POINTER__ == 4\n{lm.group(0)}\n#else\n"
                    f"{ind}/* port: patch 0070, 64 bit: the pointers other views have in this pad */\n" +
                    "\n".join(ind + p for p in parts) + "\n#endif")
            new_body = new_body.replace(lm.group(0), repl, 1)
        text = text.replace(body, new_body, 1)
        open(dst, 'w').write(text)
        print(f"{src}: {owner}: " + ", ".join(f"{n} ({len(v[1])} pointers)" for n, v in sorted(members.items())))


main()
