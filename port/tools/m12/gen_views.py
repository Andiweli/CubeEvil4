#!/usr/bin/env python3
"""Milestone 12: LP64 versions of partial views, laid out after a canonical full view.
usage: gen_views.py COPYDIR CANON_UNIT CANON_REC partial_unit:partial_rec[:defining_file] ...
The canonical unit is compiled from COPYDIR copies (edited headers in COPYDIR/cri).
Each partial view's struct gets an `#if __SIZEOF_POINTER__ == 4 ... #else ... #endif` with
explicit pads that put every member at the LP64 offset the canonical view has for its
GameCube offset.

Part of the LP64 views of milestone 12 (whole-program-status.md, "M12 ARM64");
the armhf and x86_64 builds must be configured.
"""
import json, os, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import fixed_layout_check as F
ROOT = Path(__file__).resolve().parents[3]
ROW = re.compile(r"^\s*(\d+)(?::\d+-\d+)? \|( *)(.*)$")
SIZE = re.compile(r"\[sizeof=(\d+)")
copydir = sys.argv[1]
TREE = str(ROOT / 'port/build/whole-armhf/prepared/tree') + '/'


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


def records(lines):
    """name -> (size, [(offset, typ, member)]) direct members"""
    out = {}
    for i, l in enumerate(lines):
        if not l.startswith('*** Dumping') or i + 1 >= len(lines):
            continue
        m = ROW.match(lines[i + 1])
        if not m:
            continue
        name = F.clean(re.sub(r"^(struct|class|union) ", "", m.group(3).strip()))
        mem, size = [], None
        for l2 in lines[i + 2:]:
            if l2.startswith('*** Dumping'):
                break
            s = SIZE.search(l2)
            if s:
                size = int(s.group(1))
                break
            r = ROW.match(l2)
            if r and len(r.group(2)) == 3:
                text = F.clean(r.group(3).strip())
                typ, nm = text.rsplit(' ', 1) if ' ' in text else (text, '')
                mem.append((int(r.group(1)), typ, nm))
        out.setdefault(name, (size, mem))
    return out


def elem(typ):
    """(element type, count) of an array type text like 'MPVCMC_REF[6]' / 'Uint8 *[6]'"""
    m = re.match(r"^(.*?)\s*\[(\d+)\]((?:\[\d+\])*)$", typ)
    if not m:
        return typ, 1
    inner = m.group(1) + m.group(3)
    return inner, int(m.group(2))


SCALAR = {'Uint8': 1, 'Sint8': 1, 'char': 1, 'Char8': 1, 'Uint16': 2, 'Sint16': 2, 'Uint32': 4, 'Sint32': 4,
          'Float32': 4, 'Float64': 8, 'Sint64': 8, 'Uint64': 8, 'int': 4, 'short': 2, 'Bool': 4}


def tsize(typ, recs, gc):
    fa = re.search(r"\(\*\[(\d+)\]\)", typ)  # array of function pointers: R (*[n])(A)
    if fa:
        return int(fa.group(1)) * (4 if gc else 8)
    t, n = elem(typ)
    if n > 1 or t != typ:
        return tsize(t, recs, gc) * n
    if '*' in typ or '(*' in typ:
        return 4 if gc else 8
    base = re.sub(r"^(struct|union|const|volatile) ", "", typ).strip()
    if base in SCALAR:
        return SCALAR[base]
    if base in recs:
        return recs[base][0]
    if base in ('MPV', 'SJ', 'SFD', 'ADXT', 'MPS', 'SFX') or re.search(r"(FUNC|FN|CB|CBFN)$", base):
        return 4 if gc else 8  # pointer typedefs
    raise KeyError(typ)


def canon(x, rec, ra, rb):
    """LP64 offset of GameCube offset x inside record rec (pads split on LP64 by splitpads.py:
    `pad` -> `pad` / `pad_p<rel>` (pointers) / `pad_<rel>` (bytes))"""
    sa, ma = ra[rec]
    sb, mb = rb[rec]
    lp = {n: (o, t) for o, t, n in mb}
    for oa, ta, na in ma:
        size = tsize(ta, ra, True)
        if not (oa <= x < oa + max(size, 1)):
            continue
        r = x - oa
        parts = sorted(((0 if n == na else int(re.match(re.escape(na) + r"_p?([0-9a-f]+)$", n).group(1), 16)), o)
                       for n, (o, t) in lp.items()
                       if n == na or re.match(re.escape(na) + r"_p?[0-9a-f]+$", n))
        if not parts:
            raise ValueError(f"{rec}.{na} missing on LP64")
        if len(parts) > 1:  # a split pad
            rel, o = max(p for p in parts if p[0] <= r)
            return o + (r - rel)
        ob = parts[0][1]
        t, n = elem(ta)
        if t != ta:  # array
            es_a = tsize(t, ra, True)
            es_b = tsize(t, rb, False)
            k, rr = divmod(r, es_a)
            base = re.sub(r"^(struct|union) ", "", t).strip()
            if base in ra and rr:
                return ob + k * es_b + canon(rr, base, ra, rb)
            return ob + k * es_b + rr
        base = re.sub(r"^(struct|union) ", "", ta).strip()
        if base in ra and r:
            return ob + canon(r, base, ra, rb)
        return ob + r
    if x == sa:
        return sb
    raise ValueError(f"{x:#x} not in {rec}")


def main():
    canon_unit, canon_rec = sys.argv[2], sys.argv[3]
    ra, rb = records(dump('armhf', canon_unit)), records(dump('x86_64', canon_unit))
    for spec in sys.argv[4:]:
        parts = spec.split(':')
        unit, rec = parts[0], parts[1]
        edit = parts[2] if len(parts) > 2 else unit  # the file that defines rec (a header)
        pa = records(dump('armhf', unit))
        pb = records(dump('x86_64', unit))
        for k, v in rb.items():
            pb.setdefault(k, v)
        size_a, mem = pa[rec]
        src = TREE + edit
        dst = os.path.join(copydir, edit.replace('src/lib/', ''))
        if not os.path.exists(dst):
            shutil.copy(src, dst)
        text = open(dst).read()
        end_m = re.search(r"\n\}\s*" + re.escape(rec) + r"\s*;", text)
        start = text.rfind("typedef struct", 0, end_m.start())
        open_brace = text.index("{\n", start) + 2
        body = text[open_brace:end_m.start()]
        # the member declaration lines of the view, in order (pads left out on LP64)
        lines = body.split('\n')
        decls = {}
        for ln in lines:
            mm = re.search(r"([A-Za-z_]\w*)\s*(\[[^\]]*\])*\s*;", ln)
            fm = re.search(r"\(\s*\*\s*(\w+)\s*(?:\[[^\]]*\])*\s*\)", ln)
            name = fm.group(1) if fm else (mm.group(1) if mm else None)
            if name:
                decls[name] = ln
        out, cur = [], 0
        code = text[text.find(body) + len(body):]
        for oa, ta, na in mem:
            if re.match(r"^pad\w*$", na) and not re.search(r"(->|\.)" + na + r"\b", code):
                continue
            if re.match(r"^pad\w*$", na):
                # a pad the code uses as a region: the same bytes on LP64
                t0 = canon(oa, canon_rec, ra, rb)
                t1 = canon(oa + tsize(ta, ra, True), canon_rec, ra, rb)
                if t0 > cur:
                    out.append(f"\tUint8 lp64_pad{oa:x}[{t0 - cur:#x}];")
                out.append(f"\tUint8 {na}[{t1 - t0:#x}];")
                cur = t1
                continue
            target = canon(oa, canon_rec, ra, rb)
            if target < cur:
                raise SystemExit(f"{unit} {rec}.{na}: LP64 {target:#x} before {cur:#x}")
            if target > cur:
                out.append(f"\tUint8 lp64_pad{oa:x}[{target - cur:#x}];")
            if na not in decls:
                raise SystemExit(f"{unit} {rec}: no declaration line for {na}")
            # a pointer (or 8-byte scalar) where the canonical view has a 4-byte member would be
            # moved by its alignment: the view's type is wrong there (sfd_mem.c had void * for an id)
            el, _ = elem(ta)
            if ('*' in el or SCALAR.get(re.sub(r"^(const|volatile) ", "", el).strip(), 0) == 8) and target % 8:
                raise SystemExit(f"{unit} {rec}.{na}: {ta} at LP64 {target:#x} is not 8-aligned; "
                                 f"{canon_rec} has another type there")
            out.append(decls[na].split('/*')[0].rstrip())
            try:
                cur = target + tsize(ta, pb, False)
            except KeyError:
                cur = target + tsize(ta, rb, False)
        end = canon(size_a, canon_rec, ra, rb) if size_a < ra[canon_rec][0] else rb[canon_rec][0]
        if end > cur:
            out.append(f"\tUint8 lp64_pad_end[{end - cur:#x}];")
        new = ("#if __SIZEOF_POINTER__ == 4\n" + body + "\n#else\n"
               f"\t/* port: patch 0070, 64 bit: the members at the offsets of {canon_rec} on LP64 */\n" +
               "\n".join(out) + "\n#endif")
        text = text.replace(body, new, 1)
        open(dst, 'w').write(text)
        print(f"{unit}: {rec}: {len([x for x in mem if not x[2].startswith('pad')])} members placed, LP64 end {end:#x}")


main()
