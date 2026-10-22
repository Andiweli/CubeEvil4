#!/usr/bin/env python3
"""Bug hunt (22.10.2026): grab callbacks that check another routine than the one that holds Leon.

An enemy that grabs Leon (or Ashley, Luis) installs a callback for the held character
(EmCatchPLSet, SetSubDamage); the callback lets go as soon as the enemy leaves its routine:
`if (pPL->pEmCatch->r_no_0 != 1 || pPL->pEmCatch->r_no_1 != N)`. Patch 0095 (a bug of the
prototype): the Regenerator's floor bite (1/0x15) checked the standing bite's 0xB, Leon was let go
at once while the bite went on. For every such check in the enemy code this prints the routine
numbers (from the enemy's R1 move table) of the functions that install the callback:

    OK  the number checked is one of them
    ??  it is not, joined with `||`: the held character is let go at once (a bug, like 0095)
    ~~  it is not, joined with `&&`: only lets go once the enemy leaves routine group 1, where
        r_no_1 never takes these numbers (no visible effect; em10 Luis, em39 low kick)

    port/tools/scan/catch_scan.py [SRC]      (default: src/ of this repository)
"""

import re
import sys
from pathlib import Path

FUNC = re.compile(r"^(?:static\s+)?(?:void|int|s32|u32)\s+(\w+)\s*\([^;]*$", re.M)
CHECK = re.compile(r"pEmCatch->r_no_1 != (0x[0-9A-Fa-f]+|\d+)")


def scan(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    checks = list(CHECK.finditer(text))
    if not checks:
        return []
    table = re.search(r"static \w+ (\w+_R1_move_tbl)\[\d+\] = \{(.*?)\};", text, re.S)
    if not table:
        return [f"--  {path}: no R1 move table"]
    names = [e.strip() for e in re.sub(r"//[^\n]*", "", table.group(2)).split(",") if e.strip()]
    paired = re.search(re.escape(table.group(1)) + r"\[em->r_no_1 \* 2\]", text) is not None
    numbers = {}
    for i, name in enumerate(names):
        numbers.setdefault(name, set()).add(i // 2 if paired else i)
    starts = [(m.start(), m.group(1)) for m in FUNC.finditer(text)]

    def enclosing(pos):
        name = None
        for start, n in starts:
            if start > pos:
                break
            name = n
        return name

    out = []
    for c in checks:
        callback = enclosing(c.start())
        # a check inside a macro (em39's PLEM39_K4_WAIT): the callback is where the macro is used
        define = text.rfind("#define ", 0, c.start())
        if define >= 0 and "\\\n" in text[define:c.start()] and "\n\n" not in text[define:c.start()]:
            macro = re.match(r"#define (\w+)", text[define:]).group(1)
            use = re.search(r"\b" + macro + r"\b", text[c.end():])
            callback = enclosing(c.end() + use.start()) if use else callback
        want = int(c.group(1), 0)
        line = text.count("\n", 0, c.start()) + 1
        uses = {enclosing(u.start()) for u in re.finditer(r"\b" + re.escape(callback) + r"\b", text)}
        installers = sorted(uses - {callback, None})
        found = sorted({n for i in installers for n in numbers.get(i, ())})
        joined = "&&" if re.search(r"!= 1 &&\s*\w+->pEmCatch->r_no_1", text[c.start() - 60:c.end()]) else "||"
        mark = "OK" if want in found else "??" if joined == "||" else "~~"
        out.append(f"{mark}  {path}:{line} {callback} checks 1/0x{want:X} ({joined}); installed in "
                   + ", ".join(f"{i} {['1/0x%X' % n for n in sorted(numbers.get(i, ()))]}" for i in installers))
    return out


def main():
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[3] / "src"
    lines = [l for f in sorted(src.glob("em*/*.cpp")) for l in scan(f)]
    print("\n".join(lines))
    print(f"catch_scan: {len(lines)} checks, {sum(l.startswith('??') for l in lines)} not matching with ||, "
          f"{sum(l.startswith('~~') for l in lines)} with &&",
          file=sys.stderr)


if __name__ == "__main__":
    main()
