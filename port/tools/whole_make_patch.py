#!/usr/bin/env python3
"""Create a reviewed patch for port/whole/patches/.

Patches apply after the mechanical rewrites of whole_prepare.py, so the "a"
side is the rewritten original. Edit a copy, then run:

    whole_make_patch.py NAME ORIGINAL_PATH EDITED_COPY

ORIGINAL_PATH is relative to the repository root (e.g. include/mes.h).
"""
import difflib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from whole_prepare import ROOT, PATCHES, rewrite  # noqa: E402


def main() -> int:
    name, rel, edited = sys.argv[1], sys.argv[2], Path(sys.argv[3])
    before = rewrite((ROOT / rel).read_text(encoding="utf-8", errors="surrogateescape"))
    after = edited.read_text(encoding="utf-8", errors="surrogateescape")
    diff = difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                f"a/{rel}", f"b/{rel}")
    text = "".join(diff)
    if not text:
        sys.exit("no difference")
    (PATCHES / f"{name}.patch").write_text(text, encoding="utf-8", errors="surrogateescape")
    print(f"{PATCHES / (name + '.patch')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
