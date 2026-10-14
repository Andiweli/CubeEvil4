#!/usr/bin/env python3
"""Compare native directory lookup with the original PowerPC GetDataExt.
Usage: test_archive_data.py yz2_tool archive_tool GAME_ROOT
Requires unicorn==2.1.4. No game files are bundled.
"""
from pathlib import Path
import hashlib
import struct
import subprocess
import sys
import tempfile
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import (UC_PPC_REG_1, UC_PPC_REG_3, UC_PPC_REG_4,
                               UC_PPC_REG_5, UC_PPC_REG_LR, UC_PPC_REG_PC)

if len(sys.argv) != 4:
    raise SystemExit(__doc__)
yz2, archive = map(lambda p: str(Path(p).resolve()), sys.argv[1:3])
game = Path(sys.argv[3]).resolve()
root = Path(__file__).resolve().parent.parent
dol = (game / "disc1/sys/main.dol").read_bytes()
if hashlib.sha1(dol).hexdigest() != "61e0cdacaa48a2de4a935d925feedac968c6bf6f":
    raise SystemExit("Wrong original main.dol")
machine = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
machine.mem_map(0x80000000, 0x2000000)
machine.mem_map(0x1000000, 0x5000000)
for i in range(18):
    start = int.from_bytes(dol[i*4:i*4+4], "big")
    address = int.from_bytes(dol[0x48+i*4:0x4C+i*4], "big")
    size = int.from_bytes(dol[0x90+i*4:0x94+i*4], "big")
    if size:
        machine.mem_write(address, dol[start:start+size])


def original_lookup(tag, occurrence):
    machine.mem_write(0x5100000, tag + b"\x00")
    for register, value in ((UC_PPC_REG_1, 0x81800000),
                            (UC_PPC_REG_3, 0x1000000),
                            (UC_PPC_REG_4, 0x5100000),
                            (UC_PPC_REG_5, occurrence & 0xFFFFFFFF),
                            (UC_PPC_REG_LR, 0x817FFFF0)):
        machine.reg_write(register, value)
    machine.emu_start(0x8017FD18, 0x817FFFF0, count=20000)
    if machine.reg_read(UC_PPC_REG_PC) != 0x817FFFF0:
        raise RuntimeError("Original GetDataExt did not finish")
    value = machine.reg_read(UC_PPC_REG_3)
    return None if value == 0 else value - 0x1000000


rooms = entries = queries = textures = 0
with tempfile.TemporaryDirectory(prefix="re4-archive-") as directory:
    output = Path(directory) / "room.dat"
    for line in (root / "app/src/main/assets/rooms.tsv").read_text().splitlines():
        if line.startswith("#"):
            continue
        disc, path, size, expected, source_hash = line.split("\t")
        source = game / ("disc"+disc) / "files" / path
        if hashlib.sha256(source.read_bytes()).hexdigest() != source_hash:
            raise SystemExit("Wrong source: " + str(source))
        subprocess.run([yz2, str(source), str(output)], check=True, stdout=subprocess.DEVNULL)
        data = output.read_bytes()
        if len(data) != int(size) or hashlib.sha256(data).hexdigest() != expected:
            raise SystemExit("Wrong room output: " + path)
        rows = subprocess.check_output([archive, str(output)], text=True).splitlines()
        machine.mem_write(0x1000000, data)
        count = int.from_bytes(data[:4], "big")
        if len(rows) != count:
            raise SystemExit("Wrong entry count")
        tags = {}
        for index, row in enumerate(rows):
            i, tag_hex, occurrence, offset, extent, images = row.split("\t")
            textures += int(images)
            tag = bytes.fromhex(tag_hex)
            # Separate direct BE table read verifies every native row and inferred extent.
            offset_table = struct.unpack_from(">"+str(count)+"I", data, 16)
            expected_extent = min((v for v in offset_table if v > int(offset)), default=len(data)) - int(offset)
            assert int(i) == index and int(offset) == offset_table[index]
            assert tag == data[16+count*4+index*4:20+count*4+index*4]
            assert int(extent) == expected_extent
            assert original_lookup(tag[:3], int(occurrence)) == int(offset)
            entries += 1
            queries += 1
            tags[tag[:3]] = int(occurrence)+1
        for tag, next_occurrence in tags.items():
            assert original_lookup(tag, next_occurrence) is None
            assert original_lookup(tag, -1) is None
            queries += 2
        assert original_lookup(b"???", 0) is None
        queries += 1
        rooms += 1
        if rooms % 10 == 0:
            print(rooms, "rooms compared", flush=True)
print(rooms, "rooms;", entries, "entries;", queries, "original PowerPC lookup queries;", textures, "TPL images PASS")
