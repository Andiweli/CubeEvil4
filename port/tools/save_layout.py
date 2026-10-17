#!/usr/bin/env python3
"""Byte-order table of the game's save files, from the game headers.

The port keeps the game's variables in the byte order of the device
(little-endian). A save file is a copy of some of them (src/game/card.cpp
makeSaveData / makeSystemSaveData), so on a GameCube or in Dolphin the same
bytes come in big-endian order. To move a save between the two
(port/whole/runtime/gci.cpp) every field has to be turned around by its own
size: u16 by two bytes, u32/f32 by four, bytes not at all.

This tool asks the compiler for the layout instead of writing it down by
hand: it compiles a probe with the prepared headers of a configured build
(the same flags as src/game/card.cpp, so patch 0016's little-endian unions
are in), reads the debug information with llvm-dwarfdump and writes, for
each saved structure, the runs of fields wider than a byte:

    save_layout.py BUILD_DIR [--out port/whole/runtime/save_layout.inc] [--check]

--check: exit code 1 when the file on disk differs from what the headers
give now (whole_build.sh runs it when llvm-dwarfdump is there).

Unions take their widest member (patch 0016 turned their byte views around
on little-endian hosts, so the word is the value both platforms agree on).
Members named pad_* / _padding* hold bytes whose meaning is not known; they
are listed (UNKNOWN) so the converter can say when a save has something in
them.
"""
import argparse
import json
import re
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[1].parent
DEFAULT_OUT = REPO / "port/whole/runtime/save_layout.inc"

PROBE = """\
#include "global.h"
#include "item.h"
#include "room_data.h"
#include "sscrn.h"
#include "merchant.h"
#include "game.h"
GLOBAL_WK probe_global;
_ITEM_SAVE_DATA probe_item;
RoomSaveHdr probe_room_hdr;
ROOM_SAVE_DATA probe_room;
SSCRN_SAVE_DATA probe_sscrn;
MERCHANT_DATA probe_merchant;
SYSTEM_SAVE_WORK probe_system;
"""

# (table name, structure, first byte, end) -- GLOBAL_WK: the block cGameSave
# copies (save_data_start_addr 0x4F80 .. 0x8678, game.h GameSaveBlock)
TABLES = [
    ("kSaveGlobal", "GLOBAL_WK", 0x4F80, 0x8678),
    ("kSaveItem", "_ITEM_SAVE_DATA", None, None),
    ("kSaveRoomHeader", "RoomSaveHdr", None, None),
    ("kSaveRoom", "ROOM_SAVE_DATA", None, None),
    ("kSaveSscrn", "SSCRN_SAVE_DATA", None, None),
    ("kSaveMerchant", "MERCHANT_DATA", None, None),
    ("kSaveSystem", "SYSTEM_SAVE_WORK", None, None),
]

# members whose bytes have no known meaning: the decomp's placeholders and the
# start marker of the GLOBAL_WK save block
UNKNOWN_NAME = re.compile(r"^(pad_[0-9A-Fa-f]+|_padding\w*|__ssd_padding\w*|save_data_start_addr)$")
DIE = re.compile(r"^(0x[0-9a-f]+):( +)(DW_TAG_\w+|NULL)")
ATTR = re.compile(r"^\s+(DW_AT_\w+)\s+\((.*)\)\s*$")


def parse(text):
    """llvm-dwarfdump --debug-info text -> {offset: die}; die = dict with tag, attrs, children."""
    dies, stack = {}, []
    current = None
    for line in text.splitlines():
        m = DIE.match(line)
        if m:
            off, indent, tag = int(m.group(1), 16), len(m.group(2)), m.group(3)
            while stack and stack[-1][0] >= indent:
                stack.pop()
            if tag == "NULL":
                current = None
                continue
            current = {"off": off, "tag": tag, "attrs": {}, "children": []}
            dies[off] = current
            if stack:
                stack[-1][1]["children"].append(current)
            stack.append((indent, current))
            continue
        a = ATTR.match(line)
        if a and current is not None:
            current["attrs"][a.group(1)] = a.group(2)
    return dies


def num(value):
    return int(value.split()[0], 0)


def ref(value):
    return int(value.split()[0], 16)


def name(die):
    v = die["attrs"].get("DW_AT_name")
    return v.strip('"') if v else ""


class Layout:
    def __init__(self, dies):
        self.dies = dies

    def resolve(self, die):
        while die["tag"] in ("DW_TAG_typedef", "DW_TAG_const_type", "DW_TAG_volatile_type"):
            die = self.dies[ref(die["attrs"]["DW_AT_type"])]
        return die

    def size(self, die):
        die = self.resolve(die)
        if "DW_AT_byte_size" in die["attrs"]:
            return num(die["attrs"]["DW_AT_byte_size"])
        if die["tag"] == "DW_TAG_array_type":
            n = 1
            for sub in die["children"]:
                if sub["tag"] == "DW_TAG_subrange_type":
                    n *= self.count(sub)
            return n * self.size(self.dies[ref(die["attrs"]["DW_AT_type"])])
        if die["tag"] == "DW_TAG_pointer_type":
            return 4
        raise ValueError(f"no size for {die['tag']} {name(die)}")

    @staticmethod
    def count(sub):
        a = sub["attrs"]
        if "DW_AT_count" in a:
            return num(a["DW_AT_count"])
        return num(a["DW_AT_upper_bound"]) + 1

    def scalars(self, die, base, path, out, unknown):
        """(offset, size, path) of every field wider than a byte under `die` at `base`."""
        die = self.resolve(die)
        tag = die["tag"]
        if tag in ("DW_TAG_base_type", "DW_TAG_enumeration_type", "DW_TAG_pointer_type"):
            s = self.size(die)
            if s > 1:
                out.append((base, s, path))
            return
        if tag == "DW_TAG_array_type":
            elem = self.dies[ref(die["attrs"]["DW_AT_type"])]
            n = 1
            for sub in die["children"]:
                if sub["tag"] == "DW_TAG_subrange_type":
                    n *= self.count(sub)
            es = self.size(elem)
            if self.resolve(elem)["tag"] == "DW_TAG_base_type" and es == 1:
                return
            for i in range(n):
                self.scalars(elem, base + i * es, f"{path}[{i}]", out, unknown)
            return
        if tag in ("DW_TAG_structure_type", "DW_TAG_class_type"):
            for m in die["children"]:
                if m["tag"] == "DW_TAG_inheritance":
                    raise ValueError(f"{path}: base classes are not expected in a save structure")
                if m["tag"] != "DW_TAG_member" or "DW_AT_data_member_location" not in m["attrs"]:
                    continue
                if "DW_AT_bit_size" in m["attrs"]:
                    raise ValueError(f"{path}.{name(m)}: bit fields need their own rule")
                off = base + num(m["attrs"]["DW_AT_data_member_location"])
                t = self.dies[ref(m["attrs"]["DW_AT_type"])]
                n = name(m)
                if UNKNOWN_NAME.match(n):
                    unknown.append((off, self.size(t), f"{path}.{n}" if path else n))
                    continue
                self.scalars(t, off, f"{path}.{n}" if path else (n or "(anonymous)"), out, unknown)
            return
        if tag == "DW_TAG_union_type":
            best, best_unit = None, 0
            for m in die["children"]:
                if m["tag"] != "DW_TAG_member":
                    continue
                runs = []
                self.scalars(self.dies[ref(m["attrs"]["DW_AT_type"])], base, path, runs, [])
                unit = max((r[1] for r in runs), default=1)
                if unit > best_unit:
                    best, best_unit = (m, runs), unit
            if best:
                m, runs = best
                n = name(m) or "(anonymous)"
                out.extend((o, s, f"{path}.{n}" if path else n) for o, s, _ in runs)
            return
        raise ValueError(f"{path}: unexpected {tag}")

    def member_offset(self, die, wanted):
        for m in self.resolve(die)["children"]:
            if m["tag"] == "DW_TAG_member" and name(m) == wanted:
                return num(m["attrs"]["DW_AT_data_member_location"])
        raise ValueError(f"no member {wanted}")

    def struct(self, wanted):
        for die in self.dies.values():
            if die["tag"] in ("DW_TAG_structure_type", "DW_TAG_class_type") and name(die) == wanted \
                    and "DW_AT_byte_size" in die["attrs"]:
                return die
        raise ValueError(f"{wanted} not in the debug information")


def runs_of(fields):
    """Consecutive fields of one size -> (offset, size, count)."""
    out = []
    for off, size, _ in sorted(fields):
        if out and out[-1][1] == size and out[-1][0] + out[-1][1] * out[-1][2] == off:
            out[-1][2] += 1
        else:
            out.append([off, size, 1])
    return out


def compile_probe(build: Path):
    cc = json.loads((build / "compile_commands.json").read_text())
    entry = next(e for e in cc if e["file"].endswith("src/game/card.cpp"))
    args = entry.get("arguments") or shlex.split(entry["command"])
    keep, skip = [], False
    for a in args:
        if skip:
            skip = False
            continue
        if a in ("-o", "-c"):
            skip = True
            continue
        if a.endswith("card.cpp"):
            continue
        keep.append(a)
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / "save_probe.cpp"
        obj = Path(tmp) / "save_probe.o"
        src.write_text(PROBE)
        subprocess.run(keep + ["-g", "-fstandalone-debug", "-c", str(src), "-o", str(obj)], check=True,
                       cwd=entry.get("directory", build))
        return subprocess.run(["llvm-dwarfdump", "--debug-info", str(obj)], check=True, capture_output=True,
                              text=True).stdout


def render(layout):
    lines = [
        "// Byte-order table of the save files (generated by port/tools/save_layout.py",
        "// from the game headers; do not edit). Each run: `count` fields of `size`",
        "// bytes from `offset` (relative to the structure, for kSaveGlobal to",
        "// GLOBAL_WK 0x4F80) that a GameCube keeps big-endian. Bytes not listed",
        "// are single bytes. UNKNOWN: bytes of unknown meaning (pad_*).",
        "",
    ]
    sizes = {}
    for table, struct, first, end in TABLES:
        die = layout.struct(struct)
        size = layout.size(die)
        fields, unknown = [], []
        layout.scalars(die, 0, "", fields, unknown)
        if first is not None:
            # the block starts at the member save_data_start_addr: 0x4F80 on the
            # GameCube, later on LP64 (pointers in front, milestone 12); its
            # contents keep their form
            start = layout.member_offset(die, "save_data_start_addr")
            first, end = start, start + (end - first)
            fields = [(o - first, s, p) for o, s, p in fields if first <= o < end]
            unknown = [(o - first, s, p) for o, s, p in unknown if first <= o < end]
            size = end - first
        for o, s, p in fields:
            if o % s:
                raise ValueError(f"{struct}{p}: {s}-byte field at 0x{o:x} is not aligned")
        sizes[table] = size
        runs = runs_of(fields)
        # the GameCube offsets: the same text for every pointer width
        lines.append(f"// {struct}" + (" 0x4F80..0x8678" if first is not None else "")
                     + f": 0x{size:X} bytes, {len(fields)} fields wider than a byte")
        lines.append(f"static const Re4SaveRun {table}[] = {{")
        for o, s, c in runs:
            lines.append(f"    {{0x{o:04X}, {s}, {c}}},")
        lines.append("};")
        lines.append(f"static const unsigned {table}Size = 0x{size:X};")
        if unknown:
            lines.append(f"static const Re4SaveRun {table}Unknown[] = {{  // UNKNOWN")
            for o, s, p in sorted(unknown):
                lines.append(f"    {{0x{o:04X}, 1, {s}}},  // {p}")
            lines.append("};")
        else:
            lines.append(f"static const Re4SaveRun {table}Unknown[] = {{{{0, 1, 0}}}};  // none")
        lines.append("")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build", type=Path, help="configured build (compile_commands.json, prepared tree)")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    text = render(Layout(parse(compile_probe(args.build))))
    if args.check:
        old = args.out.read_text() if args.out.exists() else ""
        if old != text:
            print(f"{args.out}: differs from the game headers (run port/tools/save_layout.py {args.build})")
            return 1
        print(f"{args.out}: matches the game headers")
        return 0
    args.out.write_text(text)
    print(f"{args.out}: written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
