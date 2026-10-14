#!/usr/bin/env python3
"""Finds word accesses to byte data in the game code with the compiler.

On the GameCube the most significant byte of a word comes first; on ARM and
x86 it comes last. Game code that writes or reads several small fields at once
through a cast pointer (`*(u32*) &col = 0xFF`, `*(u32*) &em->r_no_0 = ...`)
therefore moves the bytes to other fields on the port. This tool asks clang
(clang-query on the prepared tree, compile_commands.json of a configured
build) for every dereferenced cast `*(T*) &x` / `((T*) &x)[i]` / `*(T*) arr`
where T is a scalar, and lists those whose object x consists of elements
smaller than T (byte fields, byte arrays, byte structures) or larger than T
(part of a word).

    whole_byte_views.py scan BUILD_DIR --shadow SCRATCH [--jobs N] [--out FILE.json]
    whole_byte_views.py report FILE.json        # Markdown table

The list is checked in as port/whole/byte_views.json; whole_prepare.py
applies the replacements recorded there (BYTE_VIEW rules) and stops the build
if a recorded site no longer matches.
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

CAST = ('explicitCastExpr(hasDestinationType(pointsTo(qualType(hasCanonicalType(builtinType())))), '
        'hasSourceExpression(ignoringParenImpCasts(anyOf(unaryOperator(hasOperatorName("&")), '
        'expr(hasType(arrayType()))))))')
MATCHERS = [
    f'm unaryOperator(hasOperatorName("*"), hasUnaryOperand(ignoringParenImpCasts({CAST}.bind("deref"))))',
    f'm arraySubscriptExpr(hasBase(ignoringParenImpCasts({CAST}.bind("index"))))',
]

SCALARS = {
    "char": 1, "signed char": 1, "unsigned char": 1, "bool": 1, "_Bool": 1,
    "u8": 1, "s8": 1, "BOOL8": 1,
    "short": 2, "unsigned short": 2, "u16": 2, "s16": 2,
    "int": 4, "unsigned int": 4, "long": 4, "unsigned long": 4, "float": 4,
    "u32": 4, "s32": 4, "f32": 4, "BOOL": 4, "size_t": 4,
    "long long": 8, "unsigned long long": 8, "double": 8, "u64": 8, "s64": 8, "f64": 8,
}
# Element size of the game's structures that appear as cast objects
# (smallest scalar member; filled in by reviewing the headers).
RECORDS = {
    "GXColor": 1,          # u8 r, g, b, a (dolphin/gx/GXStruct.h)
}


def scalar_size(t: str):
    t = re.sub(r"\b(const|volatile|struct|class|union|enum)\b", "", t).strip()
    return SCALARS.get(t)


def element(t: str):
    """(kind, size) of an object type: scalar, array of scalars, record."""
    t = re.sub(r"\b(const|volatile)\b", "", t).strip()
    m = re.match(r"^(.*?)\s*\[\d*\](?:\[\d*\])*$", t)
    if m:
        k, s = element(m.group(1))
        return ("array of " + k if k else "array"), s
    s = scalar_size(t)
    if s:
        return "scalar", s
    name = re.sub(r"\b(struct|class|union)\b", "", t).strip()
    return "record " + name, RECORDS.get(name)


def tree_rel(path: str, tree: str) -> str:
    return path[len(tree):].lstrip("/") if path.startswith(tree) else path


LOCATION = re.compile(r'^(?P<file>/[^:]+):(?P<line>\d+):(?P<col>\d+): note: "(?P<bind>\w+)" binds here')
MACRO = re.compile(r"^(?P<file>/[^:]+):(?P<line>\d+):(?P<col>\d+): note: expanded from macro '(?P<name>\w+)'")
CAST_LINE = re.compile(r"^(?:CStyleCastExpr|CXXReinterpretCastExpr|CXXStaticCastExpr|CXXFunctionalCastExpr)"
                       r" 0x\w+ <(?P<range>[^>]*)> '(?P<type>[^']*)'")
NODE_TYPE = re.compile(r"^[ |`-]*(?P<node>\w+) 0x\w+ <[^>]*> '(?P<type>[^']*)'(?::'(?P<canon>[^']*)')?(?P<rest>.*)$")


def parse(output: str, tree: str):
    sites = []
    for block in output.split("\nMatch #")[1:]:
        lines = block.split("\n")
        loc = next((LOCATION.match(l) for l in lines if LOCATION.match(l)), None)
        if not loc:
            continue
        macros = [MACRO.match(l) for l in lines if MACRO.match(l)]
        i = next(i for i, l in enumerate(lines) if l.startswith("Binding for"))
        cast = CAST_LINE.match(lines[i + 1])
        if not cast:
            continue
        child = NODE_TYPE.match(lines[i + 2]) if i + 2 < len(lines) else None
        dest = cast.group("type").rstrip(" *")
        dsize = scalar_size(dest)
        obj_type, how = None, None
        if child:
            node, ctype = child.group("node"), child.group("type")
            if node == "UnaryOperator" and "prefix '&'" in child.group("rest"):
                how = "&"
                obj_type = ctype[:-1].strip() if ctype.endswith("*") else ctype
                obj_type = re.sub(r"\(\*\)", "", obj_type).strip()
            elif node == "ImplicitCastExpr" and "ArrayToPointerDecay" in child.group("rest"):
                how = "array"
                # the decayed array is the next node
                sub = NODE_TYPE.match(lines[i + 3]) if i + 3 < len(lines) else None
                obj_type = sub.group("type") if sub else ctype[:-1].strip() + " []"
        if obj_type is None:
            continue
        kind, esize = element(obj_type)
        rng = cast.group("range")
        spell = re.match(r"(?P<file>[^:]+):(?P<line>\d+):(?P<col>\d+), (?:line:(?P<eline>\d+):)?col:(?P<ecol>\d+)$", rng)
        sites.append({
            "file": tree_rel(loc.group("file"), tree),
            "line": int(loc.group("line")),
            "col": int(loc.group("col")),
            "use": loc.group("bind"),
            "macro": macros[0].group("name") if macros else None,
            "macro_file": tree_rel(macros[-1].group("file"), tree) if macros else None,
            "macro_line": int(macros[-1].group("line")) if macros else None,
            "cast": dest,
            "cast_size": dsize,
            "object": obj_type,
            "object_kind": kind,
            "element_size": esize,
            "how": how,
            "spelling_file": tree_rel(spell.group("file"), tree) if spell else None,
            "spelling_line": int(spell.group("line")) if spell else None,
            "spelling_col": int(spell.group("col")) if spell else None,
            "spelling_end": (int(spell.group("eline") or spell.group("line")), int(spell.group("ecol"))) if spell else None,
            "text": lines[next(j for j, l in enumerate(lines) if LOCATION.match(l)) + 1].split("|", 1)[-1].strip(),
        })
    return sites


def cast_text(tree: Path, s):
    """Source text of the cast (from its first to the end of its last token)."""
    if not s["spelling_file"]:
        return None
    lines = (tree / s["spelling_file"]).read_text(encoding="utf-8", errors="surrogateescape").split("\n")
    eline, ecol = s["spelling_end"]
    if eline != s["spelling_line"]:
        return None
    line = lines[s["spelling_line"] - 1]
    end = ecol - 1
    if end < len(line) and (line[end].isalnum() or line[end] == "_"):
        while end < len(line) and (line[end].isalnum() or line[end] == "_"):
            end += 1
    else:
        end += 1
    return line[s["spelling_col"] - 1:end]


def verdict(s) -> str:
    if s["cast_size"] is None:
        return "unknown cast type"
    if s["element_size"] is None:
        return "unknown element size"
    if s["element_size"] == s["cast_size"]:
        return "same size"  # type pun (float bits) or plain alias: no byte order effect
    if s["element_size"] < s["cast_size"]:
        return "word over smaller fields"
    return "part of a larger field"


CLANG_QUERY = next((c for c in ("clang-query", "clang-query-18") if shutil.which(c)), "clang-query")


def run_unit(build: Path, unit: str, tree: str):
    cmd = [CLANG_QUERY, "-p", str(build), unit, "-c", "set bind-root false",
           "-c", "enable output diag", "-c", "enable output dump", "-c", "disable output print"]
    for m in MATCHERS:
        cmd += ["-c", m]
    proc = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    return parse(proc.stdout, tree)


LINE_DIRECTIVE = re.compile(r"^[ \t]*#[ \t]*line\b.*$", re.M)


def shadow_tree(build: Path, shadow: Path) -> Path:
    """Copy of the prepared tree without #line directives (312 game files name
    themselves "D:/Bio4/Prog/..." with the original line numbers), plus a
    compile_commands.json pointing at it, so clang reports physical lines."""
    tree = (build / "prepared" / "tree").resolve()
    if shadow.exists():
        shutil.rmtree(shadow)
    for path in tree.rglob("*"):
        if path.is_file():
            out = shadow / "tree" / path.relative_to(tree)
            out.parent.mkdir(parents=True, exist_ok=True)
            text = path.read_text(encoding="utf-8", errors="surrogateescape")
            out.write_text(LINE_DIRECTIVE.sub("", text), encoding="utf-8", errors="surrogateescape")
    db = (build / "compile_commands.json").read_text().replace(str(tree), str(shadow / "tree"))
    (shadow / "compile_commands.json").write_text(db)
    return shadow


def scan(build: Path, jobs: int, shadow: Path):
    shadow = shadow_tree(build, shadow)
    db = json.loads((shadow / "compile_commands.json").read_text())
    tree = str(shadow / "tree")
    units = sorted({e["file"] for e in db if e["file"].startswith(tree + "/src/")})
    sites = {}
    with ThreadPoolExecutor(jobs) as pool:
        for n, result in enumerate(pool.map(lambda u: run_unit(shadow, u, tree), units), 1):
            for s in result:
                key = (s["spelling_file"], s["spelling_line"], s["spelling_col"])
                if key in sites:
                    if (s["file"], s["line"]) not in sites[key]["expansions"]:
                        sites[key]["expansions"].append((s["file"], s["line"]))
                    continue
                s["verdict"] = verdict(s)
                s["text"] = cast_text(Path(tree), s)
                s["expansions"] = [(s["file"], s["line"])]
                sites[key] = s
            if n % 50 == 0:
                print(f"  {n}/{len(units)} units", file=sys.stderr)
    return sorted(sites.values(), key=lambda s: (s["file"], s["line"], s["col"]))


def report(sites) -> str:
    out = ["| Stelle | Makro | Zugriff | Objekt | Urteil |", "|---|---|---|---|---|"]
    for s in sites:
        if s["verdict"] == "same size":
            continue
        out.append(f"| `{s['file']}:{s['line']}` | {s['macro'] or ''} | `{s['cast']}` ({s['use']}) "
                   f"| `{s['object']}` | {s['verdict']} |")
    return "\n".join(out)


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("scan")
    a.add_argument("build", type=Path)
    a.add_argument("--jobs", type=int, default=4)
    a.add_argument("--out", type=Path)
    a.add_argument("--shadow", type=Path, required=True, help="scratch directory for the copy without #line")
    r = sub.add_parser("report")
    r.add_argument("file", type=Path)
    args = ap.parse_args()
    if args.cmd == "scan":
        sites = scan(args.build, args.jobs, args.shadow)
        text = json.dumps(sites, indent=1) + "\n"
        if args.out:
            args.out.write_text(text)
        counts = {}
        for s in sites:
            counts[s["verdict"]] = counts.get(s["verdict"], 0) + 1
        print("byte views:", ", ".join(f"{k} {v}" for k, v in sorted(counts.items())))
        return 0
    print(report(json.loads(args.file.read_text())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
