#!/usr/bin/env python3
"""Host-only oracle: execute the verified original PowerPC decoder.
Requires unicorn==2.1.4; provide original game files separately.
Usage: yz2_oracle.py main.dol room.das output.dat
No ported decoder is used to produce the expected output.
"""
from pathlib import Path
import hashlib
import struct
import sys
import time
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import (
    UC_PPC_REG_1, UC_PPC_REG_3, UC_PPC_REG_4, UC_PPC_REG_5,
    UC_PPC_REG_6, UC_PPC_REG_LR, UC_PPC_REG_MSR, UC_PPC_REG_PC,
)


def be(value):
    return struct.pack(">I", value)


def decode(path, dol_path):
    source = Path(path).read_bytes()
    offset = int.from_bytes(source[44:48], "big")
    part_size = int.from_bytes(source[36:40], "big")
    if offset != 1024 or offset + part_size > len(source):
        raise ValueError("Oracle requires the verified room-container layout")
    raw = source[offset:offset + part_size]
    out_size = int(raw[:32].split()[1], 16)
    if len(raw) > 7 * 1024 * 1024 or not 0 < out_size <= 16 * 1024 * 1024:
        raise ValueError("Oracle buffer budget exceeded")
    dol = Path(dol_path).read_bytes()
    if hashlib.sha1(dol).hexdigest() != "61e0cdacaa48a2de4a935d925feedac968c6bf6f":
        raise ValueError("Wrong main.dol revision")
    machine = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
    machine.mem_map(0x80000000, 0x2000000)
    machine.mem_map(0x1000000, 0x2000000)
    for section in range(18):
        position = int.from_bytes(dol[section * 4:section * 4 + 4], "big")
        address = int.from_bytes(dol[0x48 + section * 4:0x4C + section * 4], "big")
        size = int.from_bytes(dol[0x90 + section * 4:0x94 + section * 4], "big")
        if size:
            machine.mem_write(address, dol[position:position + size])
    # Original 32-bit BE structures, NOT native host pointer layouts.
    context, dictionary, stream, output = 0x1000000, 0x1010000, 0x1200000, 0x1900000
    machine.mem_write(context, be(dictionary))
    machine.mem_write(context + 4, be(0x80) + be(raw[32]))
    machine.mem_write(stream, raw[33:])
    machine.mem_write(context + 0x58, be(0x500))
    models = (
        (context + 0xC, 0x500, 0x1111000, 0x1113000, 0x1120000),
        (context + 0x30, 256, 0x1115000, 0x1116000, 0x1140000),
    )
    for model, count, frequencies, cumulative, lookup in models:
        bits = count.bit_length()
        machine.mem_write(model, b"".join(map(be, (
            context + 4, frequencies, count, bits, 1 << bits, cumulative,
            count, 0, lookup,
        ))))
        machine.mem_write(frequencies, b"\x00\x01" * count)
        total, table = 0, bytearray()
        for symbol in range(count):
            weight = 32768 // count + (symbol < 32768 % count)
            table.extend(struct.pack(">HH", weight, total))
            total += weight
        machine.mem_write(cumulative, bytes(table))
    event = 0x1160000
    machine.mem_write(event, be(stream))
    for register, value in (
        (UC_PPC_REG_1, 0x81800000), (UC_PPC_REG_3, context),
        (UC_PPC_REG_4, output), (UC_PPC_REG_5, out_size),
        (UC_PPC_REG_6, event), (UC_PPC_REG_LR, 0x817FFFF0),
        (UC_PPC_REG_MSR, 0x2000),  # Enable the original floating-point instructions.
    ):
        machine.reg_write(register, value)
    started = time.monotonic()
    machine.emu_start(0x801D3E04, 0x817FFFF0, timeout=120_000_000)
    if machine.reg_read(UC_PPC_REG_PC) != 0x817FFFF0:
        raise RuntimeError("PowerPC oracle timed out")
    print("Original PowerPC:", path, round(time.monotonic() - started, 3), "s", flush=True)
    return bytes(machine.mem_read(output, out_size))


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    decoded = decode(sys.argv[2], sys.argv[1])
    Path(sys.argv[3]).write_bytes(decoded)
    print(len(decoded), hashlib.sha256(decoded).hexdigest())
