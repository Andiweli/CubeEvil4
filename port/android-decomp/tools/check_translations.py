#!/usr/bin/env python3
"""The app menu's translations (app/src/main/res/values-<language>/strings_ingame.xml) against
the English texts (values/strings_ingame.xml), which Android shows for every language without a
file of its own (decision of the project owner, 21.10.2026).

For each language: every text of the English file is there and no other; each text has the same
placeholders (%1$s, %2$d, ...) and line breaks (\\n) as the English one; no apostrophe without
the backslash Android needs (\\'); each plural has its "other" form. Also: every
R.string.ingame_* / R.plurals.ingame_* the app uses is in the English file.

    tools/check_translations.py [RES_DIR]

Exit code 1 on any error; prints one line per language.
"""

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RES = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "app/src/main/res"
NAME = "strings_ingame.xml"
JAVA = [ROOT / "app/src/main/java/com/ast/re4decomp" / f for f in ("CoreMenu.java", "CoreActivity.java")]
PLACEHOLDER = re.compile(r"%(?:\d+\$)?[-#+ 0,(]*\d*(?:\.\d+)?[a-zA-Z]")


def texts(path):
    """name -> text for <string>, name -> {quantity: text} for <plurals>."""
    root = ET.parse(path).getroot()
    strings, plurals = {}, {}
    for e in root:
        name = e.get("name")
        if e.tag == "string":
            strings[name] = "".join(e.itertext())
        elif e.tag == "plurals":
            plurals[name] = {i.get("quantity"): "".join(i.itertext()) for i in e if i.tag == "item"}
    return strings, plurals


def form(text):
    """What must match the English text: the placeholders (sorted) and the line breaks."""
    return sorted(PLACEHOLDER.findall(text.replace("%%", ""))), text.count("\\n")


def text_problems(where, text):
    out = []
    quoted = len(text) >= 2 and text.startswith('"') and text.endswith('"')
    if not quoted and re.search(r"(?<!\\)'", text):
        out.append(f"{where}: apostrophe without backslash (write \\')")
    if not quoted and re.search(r'(?<!\\)"', text):
        out.append(f'{where}: quotation mark without backslash (write \\")')
    return out


def main():
    english = RES / "values" / NAME
    if not english.exists():
        print(f"no {english}")
        return 1
    en_strings, en_plurals = texts(english)
    errors = []
    for name, text in en_strings.items():
        errors += text_problems(f"values/{NAME} {name}", text)
    # the names the app uses
    used = set()
    for java in JAVA:
        used |= set(re.findall(r"R\.(?:string|plurals)\.(ingame_\w+)", java.read_text(encoding="utf-8")))
    for name in sorted(used - set(en_strings) - set(en_plurals)):
        errors.append(f"values/{NAME}: {name} is used by the app but missing")
    unused = sorted((set(en_strings) | set(en_plurals)) - used)
    languages = sorted(p.parent.name for p in RES.glob(f"values-*/{NAME}"))
    for lang in languages:
        path = RES / lang / NAME
        try:
            strings, plurals = texts(path)
        except ET.ParseError as e:
            errors.append(f"{lang}/{NAME}: not readable as XML ({e}); & and < must be written &amp; and &lt;")
            continue
        bad = []
        for name in sorted(set(en_strings) - set(strings)):
            bad.append(f"{lang}: {name} missing")
        for name in sorted(set(strings) - set(en_strings)):
            bad.append(f"{lang}: {name} is not in the English file")
        for name in sorted(set(en_strings) & set(strings)):
            if form(strings[name]) != form(en_strings[name]):
                bad.append(f"{lang}: {name}: placeholders or line breaks differ from the English text "
                           f"({form(strings[name])} instead of {form(en_strings[name])})")
            bad += text_problems(f"{lang} {name}", strings[name])
        for name in sorted(set(en_plurals) - set(plurals)):
            bad.append(f"{lang}: plurals {name} missing")
        for name in sorted(set(plurals) & set(en_plurals)):
            items = plurals[name]
            if "other" not in items:
                bad.append(f"{lang}: plurals {name} has no \"other\" form")
            for quantity, text in items.items():
                if form(text) != form(en_plurals[name]["other"]):
                    bad.append(f"{lang}: plurals {name} ({quantity}): placeholders differ from the English text")
                bad += text_problems(f"{lang} {name} ({quantity})", text)
        print(f"{lang}: {len(strings)} texts, {len(plurals)} plurals, " + ("ok" if not bad else f"{len(bad)} errors"))
        errors += bad
    print(f"English: {len(en_strings)} texts, {len(en_plurals)} plurals; languages: "
          + (", ".join(languages) if languages else "none")
          + (f"; not used by the app: {', '.join(unused)}" if unused else ""))
    for e in errors:
        print("  " + e)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
