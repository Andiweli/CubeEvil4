#!/usr/bin/env python3
"""Create a reviewed patch for port/whole/patches/.

Patches apply after the mechanical rewrites of whole_prepare.py, in name
order. The "a" side of a file is therefore the rewritten original plus every
earlier patch that touches the same file. Edit copies, then run:

    whole_make_patch.py NAME ORIGINAL_PATH EDITED_COPY [ORIGINAL_PATH EDITED_COPY ...]
    whole_make_patch.py --base NAME ORIGINAL_PATH OUT   # write the "a" side to OUT

ORIGINAL_PATH is relative to the repository root (e.g. include/mes.h).
"""
import difflib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from whole_prepare import ROOT, PATCHES, rewrite  # noqa: E402


def file_sections(patch_text: str, rel: str) -> str:
    """The sections of a (multi-file) patch that change `rel`."""
    out, keep = [], False
    for line in patch_text.splitlines(True):
        if line.startswith("--- "):
            keep = False
        if line.startswith("+++ "):
            keep = line[4:].split("\t")[0].strip() == f"b/{rel}"
            if keep:
                out.append(pending)
        if line.startswith("--- "):
            pending = line
            continue
        if keep:
            out.append(line)
    return "".join(out)


def base_text(name: str, rel: str) -> str:
    text = rewrite((ROOT / rel).read_text(encoding="utf-8", errors="surrogateescape"))
    earlier = [p for p in sorted(PATCHES.glob("*.patch")) if p.stem < name]
    with tempfile.TemporaryDirectory() as tmp:
        target = Path(tmp) / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8", errors="surrogateescape")
        for patch in earlier:
            part = file_sections(patch.read_text(encoding="utf-8", errors="surrogateescape"), rel)
            if not part:
                continue
            proc = subprocess.run(["patch", "-p1", "--forward", "--silent", "-d", tmp],
                                  input=part, capture_output=True, text=True)
            if proc.returncode != 0:
                sys.exit(f"earlier patch {patch.name} does not apply to {rel}:\n{proc.stdout}{proc.stderr}")
        return target.read_text(encoding="utf-8", errors="surrogateescape")


def main() -> int:
    args = sys.argv[1:]
    if args and args[0] == "--base":
        name, rel, out = args[1], args[2], Path(args[3])
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(base_text(name, rel), encoding="utf-8", errors="surrogateescape")
        return 0
    name, pairs = args[0], args[1:]
    if not pairs or len(pairs) % 2:
        sys.exit(__doc__)
    text = ""
    for rel, edited in zip(pairs[0::2], pairs[1::2]):
        before = base_text(name, rel)
        after = Path(edited).read_text(encoding="utf-8", errors="surrogateescape")
        diff = "".join(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                            f"a/{rel}", f"b/{rel}"))
        if not diff:
            sys.exit(f"no difference in {rel}")
        text += diff
    if not re.match(r"^\d{4}-", name):
        sys.exit("NAME must start with a four-digit number (application order)")
    (PATCHES / f"{name}.patch").write_text(text, encoding="utf-8", errors="surrogateescape")
    print(f"{PATCHES / (name + '.patch')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
