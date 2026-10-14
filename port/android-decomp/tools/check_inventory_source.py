#!/usr/bin/env python3
"""Verify frozen inventory rules against the checked-in upstream source.

No game assets or network are required. This checks source table parity,
independently of the C++ tests exercising the native packing transactions.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
puzzle = (root / "upstream/re4/src/game/puzzle.cpp").read_text()
item = (root / "upstream/re4/src/game/item.cpp").read_text()
port = (root / "app/src/main/cpp/inventory_case.cpp").read_text()


def block(text, first, last):
    return text[text.index(first):text.index(last)]


def numeric(rows):
    return [tuple(int(field, 0) for field in row) for row in rows]


shapes = re.findall(
    r'\{\s*(0x[0-9A-Fa-f]+),\s*0,\s*(\d+),\s*(\d+),.*?"([01]*)"',
    block(puzzle, "ITEM_PIECE_INFO piece_info[] = {", "// Shape record"),
)
assert len(shapes) == 59
assert all(shape == "1" * (int(w) * int(h)) for _, w, h, shape in shapes)
source_shapes = numeric([row[:3] for row in shapes if int(row[0], 0) != 0xffff])
port_shapes = numeric(re.findall(
    r'\{(0x[0-9A-Fa-f]+),\s*(\d+),\s*(\d+)\}',
    block(port, "constexpr Shape shapes[]", "struct Weapon"),
))
assert source_shapes == port_shapes, "Native piece dimensions differ from source"

source_weapons = numeric(re.findall(
    r'\{(0x[0-9A-Fa-f]+),\s*0,\s*0x[0-9A-Fa-f]+,\s*\d+,\s*\d+,\s*(0x[0-9A-Fa-f]+),\s*\{(\d+)',
    block(item, "const ITEM_WEAPON_INFO wep_info[] = {", "const WEAPON_LEVEL wep_level_info[] = {"),
))
port_weapons = numeric(re.findall(
    r'\{(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*(\d+)\}',
    block(port, "constexpr Weapon weapons[]", "struct Recipe"),
))
assert source_weapons == port_weapons, "Native base capacity/ammo IDs differ from source"

source_recipes = numeric(re.findall(
    r'\{(0x[0-9A-F]+),\s*(0x[0-9A-F]+),\s*(0x[0-9A-F]+)\}',
    block(item, "const ITEM_COMBINATION combination_info[] = {", "cItemMgr ItemMgr;"),
))
port_recipes = numeric(re.findall(
    r'\{(0x[0-9A-F]+),\s*(0x[0-9A-F]+),\s*(0x[0-9A-F]+)\}',
    block(port, "constexpr Recipe recipes[]", "std::string json"),
))
assert source_recipes == port_recipes, "Native combination rows differ from source"
print(f"Inventory source parity PASS: {len(source_shapes)} pieces, "
      f"{len(source_weapons)} weapon rows, {len(source_recipes)} recipes")
