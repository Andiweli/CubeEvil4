#!/usr/bin/env python3
"""Compare generated pieces with original atari.cpp tables and raw DVD AEV.
Usage: test_k13_scenario_collision_source.py TOOL R100_DAT
Game data stays outside the project and is not distributed.
"""
from pathlib import Path
import re
import struct
import subprocess
import sys
import numpy as np

root = Path(__file__).resolve().parent.parent
tool, room = map(lambda p: Path(p).resolve(), sys.argv[1:])
data = room.read_bytes()
count = struct.unpack_from('>I', data)[0]
offsets = struct.unpack_from(f'>{count}I', data, 16)
tags = [data[16 + count * 4 + i * 4:20 + count * 4 + i * 4] for i in range(count)]
index = next(i for i, tag in enumerate(tags) if tag[:3] == b'AEV')
begin = offsets[index]
end = min([p for p in offsets if p > begin] + [len(data)])
aev = data[begin:end]
assert aev[:6] == b'AEV\x00\x01\x04'
records = [aev[16+i*156:16+(i+1)*156] for i in range(struct.unpack_from('>H', aev, 6)[0])]
records = [r for r in records if r[0x35] == 11]
assert len(records) == 7 and all(not struct.unpack_from('>I', r, 0x4c)[0] for r in records)
source = (root/'upstream/re4/src/game/atari.cpp').read_text()
tables = {}
for name, expected in [('createSat', 8), ('createBoxSat', 12), ('createFloorSat', 2)]:
    start = source.index('cSatFile* ' + name + '(Vec* v, f32 h, u32 attr)\n{')
    body = source[start:source.index('cSatFile* f;', start)]
    triples = re.findall(r'\{ \{ (\d+), (\d+), (\d+) \}', body)
    tables[name] = [tuple(map(int, ids)) for ids in triples]
    assert len(triples) == expected
script = (root/'upstream/re4/src/st1/r100.cpp').read_text()
start = script.index('void r100_Car_pos_move()\n{')
body = script[start:script.index('\n}', start)]
assert set(int(n, 0) for n in re.findall(r'SceAtSetEnable\((0x[0-9A-F]+|\d+), 0\)', body)) == {4, 5, 0x25, 0x26, 0x27}
sce_at = (root/'upstream/re4/src/game/sce_at.cpp').read_text()
assert 'w->scr_at.sat_attr |= 0x40;' in sce_at
checks = 0
for after in (False, True):
    for effect in (False, True):
        expected = []
        for r in records:
            no = r[0x36]
            active = after if no == 0x1c else not after
            attr, eat_attr, control, flags = struct.unpack_from('>4I', r, 0x68)
            if not active or control & (1 if effect else 2):
                continue
            attr = eat_attr if effect else attr | (0 if control & 4 else 0x40)
            floor, height = struct.unpack_from('>2f', r, 8)
            points = np.array(struct.unpack_from('>8f', r, 20), dtype=np.float32).reshape(4, 2)
            vertices = np.zeros((8, 3), dtype=np.float32)
            for i in range(4):
                vertices[i] = [np.float32(points[i, 0]-points[0, 0])+points[0, 0], floor,
                               np.float32(points[i, 1]-points[0, 1])+points[0, 1]]
            vertices[4:] = vertices[:4]
            vertices[4:, 1] += np.float32(height)
            name = 'createFloorSat' if flags & 0x200 else 'createBoxSat' if flags & 0x100 else 'createSat'
            for i, ids in enumerate(tables[name]):
                group = 0 if name == 'createFloorSat' else 1 if name == 'createBoxSat' and i < 4 else 2
                expected.append((group, attr, vertices[list(ids)]))
        expected.sort(key=lambda x: x[0])  # Native SAT group partition; wall order stays source order.
        output = subprocess.check_output([str(tool), str(room), '--dump', 'AFTER' if after else 'BEFORE', 'EAT' if effect else 'SAT'], text=True)
        actual = [line.split() for line in output.splitlines()]
        assert len(actual) == len(expected), (after, effect, len(actual), len(expected))
        for row, (_, attr, vertices) in zip(actual, expected):
            assert int(row[0]) == attr
            np.testing.assert_array_equal(np.array(list(map(float, row[1:])), dtype=np.float32).reshape(3, 3), vertices)
            checks += 10
print(f'Original atari.cpp/AEV geometry parity: {checks} field checks PASS, four pre/post SAT/EAT states')
