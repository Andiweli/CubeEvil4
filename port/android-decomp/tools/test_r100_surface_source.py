#!/usr/bin/env python3
"""Assert that the independent surface oracle retains the actual source bodies."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]

def function(file, name):
    text = (root / "upstream/re4/src" / file).read_text()
    start = text.rfind("\n", 0, text.index(name)) + 1
    pos = text.index("{", start)
    end, depth = pos + 1, 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]

source = (root / "upstream/re4/src/st1/r100.cpp").read_text()
parts = []
for name in ("r100_eff_info2", "r100_eff_info4", "r100_eff_info5"):
    start = source.index("static const AtEffInfo " + name)
    parts.append(source[start:source.index(";", start) + 1])
for file, name in (("game/at_sub2.cpp", "int AtEffInfo::getWepEff"),
                   ("game/at_sub.cpp", "int EatGetEffectType"),
                   ("game/esp05.cpp", "void cEsp05::move"),
                   ("game/esp05.cpp", "int cEsp05::SetFreeWork")):
    parts.append(function(file, name))
header = "// Frozen verbatim function bodies and tables from upstream/re4 commit\n" \
         "// 9b76ff003081d6ece8b838453fb8310743a972c7. The test supplies narrow SDK stubs.\n"
expected = header + "\n\n".join(parts) + "\n\n" + function("game/esp40.cpp", "int cEsp40::SetFreeWork") + "\n"
assert (root / "tools/r100_surface_source.inc").read_text() == expected
print("PASS: 3 original AtEffInfo tables and 5 exact original function bodies")
