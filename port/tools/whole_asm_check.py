#!/usr/bin/env python3
"""Inline asm of the game sources for the whole-program port.

re4_whole_prelude.h defines asm(...) and __asm__(...) away. That is only
correct for asm that does nothing a C compiler for ARM or x86 would miss.
Every asm of the units the port compiles (and the headers they include) is
classified:

  label    `decl asm("name")`: register pinning (dropped) or a symbol alias
           (kept as __asm by whole_prepare.py)
  layout   section/alignment/.set/.globl/.type directives, data without a
           label, or a .comm whose symbol no C/C++ code names: they only
           shaped the GameCube object layout
  barrier  codeless asm (scheduling barrier, keep-alive): its outputs are "+"
           (unchanged), memory (not written) or registers that the C code
           overwrites before reading them
  copy     codeless asm whose register output is tied to an input ("0"): it
           moves a value. rewrite_copies() turns it into an assignment
  code     real instructions, labelled data, or a register output read
           without being set: needs a C replacement (a patch) or an entry in
           ASM_REPLACEMENTS, otherwise whole_prepare.py stops the build

Usage: whole_asm_check.py PREPARED_DIR [-v]   (inventory; exit 1 on problems)
"""
import json
import re
import sys
from pathlib import Path

KEYWORD = re.compile(r"\b(asm|__asm__|__asm)\b")
QUALIFIERS = ("volatile", "__volatile__", "goto", "inline")

# Directives that only arrange sections, alignment or assembler-local names.
LAYOUT_DIRECTIVES = {
    ".section", ".text", ".data", ".rodata", ".sdata", ".sdata2", ".bss", ".sbss",
    ".balign", ".align", ".p2align", ".set", ".globl", ".global", ".type", ".size",
    ".previous", ".pushsection", ".popsection", ".file", ".ident",
}

# Real asm that stays dropped because port code replaces it as a whole.
# Key: (unit, first instruction) or (unit, "*") for a unit of whole-function
# asm. Every entry must still match something (stale entries stop the build).
ASM_REPLACEMENTS = {
    ("src/game/memset_2.cpp", "*"):
        "memset/memclr routines: runtime/sn_runtime.cpp (memset_asm, memclr_asm)",
    ("src/game/yz2asm.cpp", "*"):
        "yz2 decoder kernel: runtime/yz2_decode.cpp + port/src/yz2_kernel.cpp",
    ("src/game/main.cpp", "li 3, 4"):
        "GQR2-5 set-up at boot: fixed in the re4_psq_* helpers (re4_ppc.h)",
    ("src/game/scheduler.cpp", "li 3, 4"):
        "GQR2-5 set-up per task thread: fixed in the re4_psq_* helpers (re4_ppc.h)",
}


def mask_comments(text: str) -> str:
    """Comments become spaces (newlines kept), so offsets stay valid."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif c in "\"'":
            j = skip_literal(text, i)
            out.append(text[i:j])
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def skip_literal(text: str, i: int) -> int:
    q, j, n = text[i], i + 1, len(text)
    while j < n and text[j] != q and text[j] != "\n":
        j += 2 if text[j] == "\\" else 1
    return j + 1


def group_end(text: str, start: int) -> int:
    """Index after the parenthesis group opening at text[start]."""
    depth, i, n = 0, start, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            i = skip_literal(text, i)
            continue
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return n


def split_top(text: str, sep: str):
    """Splits at `sep` outside brackets and literals (`::` never splits)."""
    parts, depth, cur, i, n = [], 0, [], 0, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            j = skip_literal(text, i)
            cur.append(text[i:j])
            i = j
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        scope = sep == ":" and (text[i:i + 2] == "::" or (i > 0 and text[i - 1] == ":"))
        if c == sep and depth == 0 and not scope:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(c)
        i += 1
    parts.append("".join(cur))
    return parts


def string_text(expr: str) -> str:
    s = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', expr))
    return s.replace("\\n", "\n").replace("\\t", "\t").replace('\\"', '"')


OPERAND = re.compile(r'^\s*(?:\[\w+\]\s*)?"([^"]*)"\s*\((.*)\)\s*$', re.S)


def operands(section: str):
    out = []
    if section.strip():
        for op in split_top(section, ","):
            m = OPERAND.match(op)
            out.append((m.group(1), m.group(2).strip()) if m else (None, op.strip()))
    return out


class Asm:
    def __init__(self, path, line, start, end, text, kind, detail, first="", copies=()):
        self.path, self.line, self.start, self.end = path, line, start, end
        self.text, self.kind, self.detail, self.first, self.copies = text, kind, detail, first, list(copies)

    def __repr__(self):
        return f"{self.path}:{self.line}: {self.kind} ({self.detail}) {self.text[:110]}"


def template_kind(template: str, computed: bool):
    """('layout' | 'code' | 'data' | 'comm', detail, first instruction)."""
    instructions, data, comm = [], [], []
    for line in template.split("\n"):
        for p in (x.strip() for x in line.split(";")):
            if not p or p.startswith("#"):
                continue
            if re.match(r"^[\w.$]+:$", p):
                data.append(p)
                continue
            word = p.split()[0].rstrip(",")
            if word in (".comm", ".lcomm"):
                comm.append(p.split(None, 1)[1].split(",")[0].strip() if " " in p else "")
            elif word.startswith("."):
                if word not in LAYOUT_DIRECTIVES:
                    data.append(p)
            elif re.match(r"^[0-9]+:?$", p):  # numeric local label
                data.append(p)
            else:
                instructions.append(p)
    if instructions:
        return "code", "instructions: " + "; ".join(instructions[:3]), instructions[0]
    labelled = [d for d in data if d.endswith(":")]
    if labelled:
        return "code", "labelled data: " + "; ".join(data[:3]), labelled[0]
    if comm:
        return "comm", ",".join(comm) + ("*" if computed else ""), ""
    if data:
        return "layout", "unnamed data: " + "; ".join(data[:2]), ""
    return "layout", "", ""


def first_use_overwrites(code: str, end: int, var: str) -> bool:
    """True if the first C mention of `var` after `end` (other asm skipped)
    assigns it, or there is none before the function ends."""
    depth, i, n = 0, end, len(code)
    while i < n:
        c = code[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth < 0:
                break
        i += 1
    rest = code[end:i]
    rest = re.sub(r"\b(?:asm|__asm__)\s*(?:volatile|__volatile__)?\s*\((?:[^()]|\((?:[^()]|\([^()]*\))*\))*\)", " ", rest)
    m = re.search(r"(?<![\w.>])" + re.escape(var) + r"\b", rest)
    if not m:
        return True
    return re.match(r"\s*=(?!=)", rest[m.end():]) is not None


def scan_text(path: str, text: str):
    code = mask_comments(text)
    found = []
    for m in KEYWORD.finditer(code):
        i = m.end()
        while True:
            q = re.match(r"\s*(" + "|".join(QUALIFIERS) + r")\b", code[i:])
            if not q:
                break
            i += q.end()
        p = re.match(r"\s*\(", code[i:])
        if not p:
            continue
        open_paren = i + p.end() - 1
        end = group_end(code, open_paren)
        sections = split_top(code[open_paren + 1:end - 1], ":")
        raw = sections[0]
        template = string_text(raw)
        computed = re.sub(r'"(?:[^"\\]|\\.)*"', "", raw).strip() != ""
        # what precedes the asm in C (preprocessor lines do not count)
        prev = re.sub(r"(?m)^[ \t]*#.*$", "", code[max(0, m.start() - 4000):m.start()]).rstrip()
        prev_word = re.search(r"([\w\])]+)$", prev)
        prev_word = prev_word.group(1) if prev_word else ""
        line = code.count("\n", 0, m.start()) + 1
        snippet = re.sub(r"\s+", " ", code[m.start():end])
        copies = []
        first = ""
        if len(sections) == 1 and not computed and re.fullmatch(r"[\w$.]+", template) and prev_word \
                and not prev.endswith((";", "{", "}")) and prev_word not in ("else", "do", "return", "case", "default"):
            kind, detail = "label", template
        else:
            kind, detail, first = template_kind(template, computed)
            if kind == "layout" and len(sections) > 1 and not template.strip():
                outs = operands(sections[1])
                ins = operands(sections[2]) if len(sections) > 2 else []
                unset = []
                for idx, (con, expr) in enumerate(outs):
                    if con is None or "+" in con or "m" in con.replace("=", "").replace("&", ""):
                        continue
                    tied = [e for c2, e in ins if c2 is not None and c2.strip() == str(idx)]
                    if tied:
                        copies.append((expr, tied[0]))
                    elif not first_use_overwrites(code, end, expr):
                        unset.append(expr)
                if copies:
                    kind, detail = "copy", ", ".join(f"{a} = {b}" for a, b in copies)
                elif unset:
                    kind, detail = "code", "register output read without being set: " + ", ".join(unset)
                else:
                    kind, detail = "barrier", "operands"
            elif kind == "layout" and not template.strip():
                kind, detail = "barrier", "empty"
        found.append(Asm(path, line, m.start(), end, snippet, kind, detail, first, copies))
    return found


def rewrite_copies(text: str) -> str:
    """Codeless asm with tied register outputs -> assignments (whole_prepare.py)."""
    if "asm" not in text:
        return text
    out, last = [], 0
    for a in scan_text("", text):
        if a.kind != "copy":
            continue
        assign = ", ".join(f"(({dst}) = (__typeof__(({dst}))) ({src}))" for dst, src in a.copies)
        out.append(text[last:a.start])
        out.append(f"/* port: tied asm copy */ ({assign})")
        last = a.end
    out.append(text[last:])
    return "".join(out)


INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.M)


def plan_units(plan: dict):
    return list(plan["main"]) + [s for m in plan["modules"].values() for s in m["sources"]]


def compiled_files(tree: Path, units, overlay: Path):
    """The units plus every header they reach (superset: #if is ignored).
    Headers of the port overlay (port/whole/include) shadow the tree's own."""
    seen = {}
    todo = [(tree / u, (tree / u).parent) for u in dict.fromkeys(units)]
    while todo:
        path, unit_dir = todo.pop()
        if path in seen:
            continue
        seen[path] = True
        try:
            text = path.read_text(encoding="utf-8", errors="surrogateescape")
        except OSError:
            continue
        for kind, name in INCLUDE.findall(mask_comments(text)):
            dirs = [overlay, tree / "include", tree / "src", unit_dir]
            if kind == '"':
                dirs.insert(0, path.parent)
            for d in dirs:
                cand = d / name
                if cand.is_file():
                    todo.append((cand.resolve(), unit_dir))
                    break
    files = []
    for path in seen:
        try:
            rel = path.relative_to(tree.resolve()).as_posix()
        except ValueError:
            try:
                rel = path.relative_to(tree).as_posix()
            except ValueError:
                continue  # port overlay header
        files.append((rel, path))
    return sorted(files)


def identifiers(text: str):
    code = re.sub(r'"(?:[^"\\\n]|\\.)*"', " ", mask_comments(text))
    return set(re.findall(r"\b[A-Za-z_]\w*\b", code))


def check(tree: Path, units, overlay: Path):
    """(asm list, problems) for the compiled files of a prepared tree."""
    found, names = [], set()
    for rel, path in compiled_files(tree, units, overlay):
        text = path.read_text(encoding="utf-8", errors="surrogateescape")
        names |= identifiers(text)
        if KEYWORD.search(text):
            found += scan_text(rel, text)
    used, problems = set(), []
    for a in found:
        if a.kind == "comm":
            symbols = [s.rstrip("*") for s in a.detail.split(",")]
            named = [s for s in symbols if s in names or (a.detail.endswith("*") and any(n.startswith(s) for n in names))]
            a.kind = "code" if named else "layout"
            a.detail = (".comm named by C code: " if named else ".comm unnamed in C: ") + a.detail
        if a.kind != "code":
            continue
        key = next((k for k in ((a.path, a.first), (a.path, "*")) if k in ASM_REPLACEMENTS), None)
        if key:
            used.add(key)
            a.detail += " -> replaced: " + ASM_REPLACEMENTS[key]
            continue
        problems.append(f"{a.path}:{a.line}: asm dropped without replacement ({a.detail}): {a.text[:120]}")
    for key in ASM_REPLACEMENTS:
        if key not in used:
            problems.append(f"stale ASM_REPLACEMENTS entry {key}: nothing left to replace")
    return found, problems


def summary(found) -> str:
    counts = {}
    for a in found:
        counts[a.kind] = counts.get(a.kind, 0) + 1
    return ", ".join(f"{k} {v}" for k, v in sorted(counts.items()))


def main() -> int:
    prepared = Path(sys.argv[1])
    overlay = Path(__file__).resolve().parents[1] / "whole" / "include"
    units = plan_units(json.loads((prepared / "modules.json").read_text()))
    found, problems = check(prepared / "tree", units, overlay)
    if "-v" in sys.argv:
        for a in found:
            print(repr(a))
    print("asm inventory:", summary(found))
    for p in problems:
        print(p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
