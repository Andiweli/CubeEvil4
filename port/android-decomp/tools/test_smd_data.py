#!/usr/bin/env python3
"""Validate native SMD reading against original PowerPC accessors and an independent vertex reference.
Usage: test_smd_data.py yz2_tool smd_tool GAME_ROOT
Requires unicorn==2.1.4; original game data are not distributed.
"""
from pathlib import Path
import hashlib
import math
import struct
import subprocess
import sys
import tempfile
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import UC_PPC_REG_1, UC_PPC_REG_3, UC_PPC_REG_4, UC_PPC_REG_LR, UC_PPC_REG_PC

if len(sys.argv) != 4:
    raise SystemExit(__doc__)
yz2, tool = [str(Path(s).resolve()) for s in sys.argv[1:3]]
game = Path(sys.argv[3]).resolve()
root = Path(__file__).resolve().parent.parent
dol = (game / 'disc1/sys/main.dol').read_bytes()
assert hashlib.sha1(dol).hexdigest() == '61e0cdacaa48a2de4a935d925feedac968c6bf6f'
u = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
u.mem_map(0x80000000, 0x2000000)
u.mem_map(0x1000000, 0x5000000)
for i in range(18):
    off = int.from_bytes(dol[i*4:i*4+4], 'big')
    addr = int.from_bytes(dol[0x48+i*4:0x4c+i*4], 'big')
    size = int.from_bytes(dol[0x90+i*4:0x94+i*4], 'big')
    if size:
        u.mem_write(addr, dol[off:off+size])


def call(fn, arg, index=0, limit=20000):
    for reg, value in [(UC_PPC_REG_1, 0x81800000), (UC_PPC_REG_3, arg),
                       (UC_PPC_REG_4, index), (UC_PPC_REG_LR, 0x817ffff0)]:
        u.reg_write(reg, value)
    u.emu_start(fn, 0x817ffff0, count=limit)
    assert u.reg_read(UC_PPC_REG_PC) == 0x817ffff0, hex(fn)
    return u.reg_read(UC_PPC_REG_3)


def close(a, b):
    assert math.isclose(a, b, rel_tol=1e-6, abs_tol=1e-5), (a, b)


rooms = supported = skipped = works = models = vertices = lists = bounds = links = 0
with tempfile.TemporaryDirectory(prefix='re4-smd-') as temp:
    out = Path(temp) / 'room.dat'
    for line in (root / 'app/src/main/assets/rooms.tsv').read_text().splitlines():
        if line.startswith('#'):
            continue
        disc, path, size, expected, source_hash = line.split('\t')
        src = game / ('disc'+disc) / 'files' / path
        assert hashlib.sha256(src.read_bytes()).hexdigest() == source_hash
        subprocess.run([yz2, str(src), str(out)], check=True, stdout=subprocess.DEVNULL)
        data = out.read_bytes()
        assert len(data) == int(size) and hashlib.sha256(data).hexdigest() == expected
        count = int.from_bytes(data[:4], 'big')
        offsets = struct.unpack_from('>'+str(count)+'I', data, 16)
        smds = []
        for i, offset in enumerate(offsets):
            if data[16+count*4+i*4:19+count*4+i*4] == b'SMD':
                end = min((o for o in offsets if o > offset), default=len(data))
                smds.append(data[offset:end])
        rows = subprocess.check_output([tool, str(out)], text=True).splitlines()
        for row in rows:
            fields = row.split('\t'); kind = fields[0]; si = int(fields[1]); d = smds[si]
            if kind == 'U':
                assert d[0] != 0x40 and int(fields[2]) == d[0]
                skipped += 1
            elif kind == 'H':
                _, _, version, flags, work_offset, logical, groups, count = map(lambda x: x if x in ('H',) else int(x), fields)
                u.mem_write(0x1000000, d)
                assert version == d[0] and flags == d[1]
                assert count == int.from_bytes(d[2:4], 'big')
                assert call(0x80191600, 0x1000000) - 0x1000000 == work_offset
                assert call(0x80191678, 0x1000000) == logical
                supported += 1
            elif kind == 'G':
                index, value = map(int, fields[2:])
                assert value == int.from_bytes(d[20+4*index:24+4*index], 'big')
            elif kind == 'W':
                i = int(fields[2]); offset = (20+4*int.from_bytes(d[16:20], 'big') if d[1]&1 else 16) + i*72
                expected = struct.unpack_from('>9f', d, offset)
                for a, b in zip(map(float, fields[3:12]), expected):
                    close(a, b)
                assert list(map(int, fields[12:16])) == list(d[offset+36:offset+40])
                assert int(fields[16]) == int.from_bytes(d[offset+68:offset+72], 'big')
                works += 1
            elif kind == 'T':
                table, i, offset = map(int, fields[2:])
                assert call([0x80191630, 0x80191648, 0x80191660][table], 0x1000000, i) - 0x1000000 == offset
                links += 1
            elif kind == 'B':
                i, offset, nv, nl, shift, version, parts, stream_bytes = map(int, fields[2:10])
                assert (nv, nl, shift, version, parts) == (int.from_bytes(d[offset+56:offset+58], 'big'), int.from_bytes(d[offset+26:offset+28], 'big'), d[offset+40], int.from_bytes(d[offset+60:offset+64], 'big'), d[offset+25])
                pos = offset + int.from_bytes(d[offset+48:offset+52], 'big')
                coords = [struct.unpack_from('>3h', d, pos+j*8) for j in range(nv)]
                actual = list(map(float, fields[10:16]))
                if nv:
                    mins = [min(v[k] for v in coords)*2**(-shift) for k in range(3)]
                    maxs = [max(v[k] for v in coords)*2**(-shift) for k in range(3)]
                    for a, b in zip(actual, mins+maxs): close(a, b)
                    bounds += 1
                p = offset+int.from_bytes(d[offset+28:offset+32], 'big'); total = 0
                for _ in range(nl):
                    n = int.from_bytes(d[p+24:p+28], 'big'); total += n; p += 32+n
                assert total == stream_bytes
                models += 1; vertices += nv; lists += nl
        rooms += 1
        if rooms % 20 == 0:
            print(rooms, 'rooms checked', flush=True)
print(rooms, 'rooms;', supported, 'supported SMD;', skipped, 'explicitly unsupported SMD;', works, 'placements;', models, 'BIN models;', vertices, 'vertices;', lists, 'displaylist blocks;', links, 'original pointer queries;', bounds, 'independent vertex bounds PASS')
