#!/usr/bin/env python3
"""Independent GC RoomInfo/AEV field and hit checks against both supplied discs.

Usage: test_room_flow_data.py YZ2_TOOL ROOM_FLOW_TOOL GAME_ROOT DISC1.gcm DISC2.gcm
No original game assets are written into the project.
"""
from pathlib import Path
import hashlib
import math
import struct
import subprocess
import sys
import tempfile

if len(sys.argv) != 6:
    raise SystemExit(__doc__)
yz, tool = (str(Path(p).resolve()) for p in sys.argv[1:3])
game = Path(sys.argv[3])
discs = [Path(p) for p in sys.argv[4:6]]
root = Path(__file__).resolve().parent.parent
half = lambda b, p: struct.unpack_from('>H', b, p)[0]
word = lambda b, p: struct.unpack_from('>I', b, p)[0]
floats = lambda b, p, n: struct.unpack_from('>' + str(n) + 'f', b, p)
signed = lambda x: x if x < 128 else x - 256


def roominfo(disc):
    with disc.open('rb') as stream:
        stream.seek(0x424)
        offset, size = struct.unpack('>II', stream.read(8))
        stream.seek(offset)
        fst = stream.read(size)
        count = word(fst, 8)
        names = fst[count * 12:]
        for i in range(1, count):
            if fst[i * 12]:
                continue
            start = word(fst, i * 12) & 0xffffff
            name = names[start:names.index(0, start)].lower()
            if name == b'roominfo.dat':
                stream.seek(word(fst, i * 12 + 4))
                return stream.read(word(fst, i * 12 + 8))
    raise AssertionError('Original roominfo.dat missing')


def info_records(data):
    points = {}
    result = []
    for stage in range(word(data, 0)):
        offset = word(data, 4 + stage * 4)
        if not offset:
            continue
        for i in range(word(data, offset)):
            p = offset + 4 + i * 32
            raw = data[p:p + 32]
            room = half(raw, 2)
            point = points.get(room, 0)
            points[room] = point + 1
            result.append((room, half(raw, 0), point, p,
                           *floats(raw, 4, 4), raw.hex()))
    return result


def eligible(room, r):
    dest = half(r, 0x6c)
    safe_pair = (room, dest, r[0x36], r[0x38]) in [
        (0x300, 0x301, 0, 8), (0x301, 0x300, 0, 8),
        (0x100, 0x101, 0, 2)]
    return (safe_pair and r[0x34] == 3 and r[0x39] == 1
            and r[4] == 1 and r[5] in [1, 2] and floats(r, 12, 1)[0] > 0
            and (r[5] != 2 or floats(r, 16, 1)[0] > 0)
            and not r[0x37] & ~3 and not r[0x6e] and not r[0x52]
            and not r[0x46] and not r[0x47] and not r[0x3a] and not r[0x45]
            and not r[0x74] and r[0x77] <= 1
            and all(not word(r, p) for p in [0x3c, 0x40, 0x4c, 0x70, 0x78]))


def reference_hit(r, feet, heading):
    if not r[0x34] & 1 or not r[0x39] & 1 or word(r, 0x4c) or r[0x37] & ~3:
        return False
    x, y, z = feet
    y += 250
    if r[0x37] & 1:
        x += math.sin(heading) * 550
        z += math.cos(heading) * 550
    floor, height, radius = floats(r, 8, 3)
    if y < floor - 100 or y >= floor + height:
        return False
    values = floats(r, 20, 8)
    if r[5] == 2:
        if (x - values[0]) ** 2 + (z - values[1]) ** 2 >= radius ** 2:
            return False
    elif r[5] == 1:
        polygon = list(zip(values[::2], values[1::2]))
        edges = list(zip(polygon, polygon[1:] + polygon[:1]))
        area = sum(a[0] * b[1] - b[0] * a[1] for a, b in edges)
        if abs(area) < 1e-6:
            return False
        # Independent convex-edge reference, rather than the original four
        # special-case comparisons used by the C++ implementation.
        cross = [(b[0] - a[0]) * (z - a[1]) - (b[1] - a[1]) * (x - a[0])
                 for a, b in edges]
        if any(c < 0 for c in cross):
            return False
    else:
        return False
    if r[0x37] & 2:
        limit = signed(r[0x49]) * 2 * math.pi / 180
        desired = signed(r[0x48]) * 2 * math.pi / 180
        if abs(math.remainder(desired - heading, 2 * math.pi)) > limit:
            return False
    return True


rooms = doors = hits = safe = starts_count = 0
ground = {}
with tempfile.TemporaryDirectory() as temp:
    temp = Path(temp)
    infos = [roominfo(disc) for disc in discs]
    assert infos[0] == infos[1]
    info_path = temp / 'roominfo.dat'
    info_path.write_bytes(infos[0])
    rows = subprocess.check_output([tool, '--info', str(info_path)], text=True).splitlines()
    expected_starts = info_records(infos[0])
    assert rows[0] == f'I {word(infos[0], 0)} {len(expected_starts)}'
    for row, expected in zip(rows[1:], expected_starts):
        fields = row.split()
        actual = (*map(int, fields[1:5]), *map(float, fields[5:9]), fields[9])
        assert actual == expected, (actual, expected)
        starts_count += 1
    assert len(rows) == starts_count + 1
    decoded = temp / 'room.dat'
    for row in (root / 'app/src/main/assets/rooms.tsv').read_text().splitlines():
        if row.startswith('#') or not row.strip():
            continue
        disc, path, size, sha, source_sha = row.split('\t')
        source = game / ('disc' + disc) / 'files' / path
        assert hashlib.sha256(source.read_bytes()).hexdigest() == source_sha
        subprocess.run([yz, str(source), str(decoded)], check=True, stdout=subprocess.DEVNULL)
        data = decoded.read_bytes()
        assert len(data) == int(size) and hashlib.sha256(data).hexdigest() == sha
        room = int(Path(path).stem[1:], 16)
        n = word(data, 0)
        offsets = [word(data, 16 + 4 * i) for i in range(n)]
        aev_offset = next((o for i, o in enumerate(offsets)
                           if data[16 + 4 * n + 4 * i:19 + 4 * n + 4 * i] == b'AEV'), None)
        rows = subprocess.check_output([tool, str(decoded), hex(room), str(info_path)],
                                       text=True).splitlines()
        rooms += 1
        if aev_offset is None:
            assert rows[0] == f'F {room} 0 0'
            continue
        a = data[aev_offset:]
        assert a[:4] == b'AEV\0' and half(a, 4) == 0x104
        records = {a[16 + i * 156 + 0x36]: (i, a[16 + i * 156:16 + (i + 1) * 156])
                   for i in range(half(a, 6)) if a[16 + i * 156 + 0x35] == 1}
        assert rows[0] == f'F {room} {half(a, 6)} {len(records)}'
        for row in rows[1:]:
            fields = row.split()
            if fields[0] == 'D':
                no = int(fields[2])
                index, r = records[no]
                expected = [room, no, index, half(r, 0x6c), r[0x74], r[0x6e],
                            r[0x6f], r[0x34], r[0x37], r[0x38], r[0x39],
                            r[0x44], r[4], r[5], *floats(r, 8, 11),
                            *floats(r, 0x5c, 4), signed(r[0x48]),
                            signed(r[0x49]), int(eligible(room, r))]
                assert list(map(float, fields[1:33])) == expected, (path, fields, expected)
                assert fields[33] == r.hex(), path
                doors += 1
                safe += int(eligible(room, r))
            elif fields[0] == 'H':
                r = records[int(fields[1])][1]
                feet = tuple(map(float, fields[2:5]))
                heading = float(fields[5])
                assert bool(int(fields[6])) == reference_hit(r, feet, heading), (path, fields)
                hits += 1
            elif fields[0] == 'S':
                raw = bytes.fromhex(fields[9])
                assert raw in [bytes.fromhex(s[-1]) for s in expected_starts if s[0] == room]
            elif fields[0] == 'G':
                ground[(int(fields[1]), int(fields[2]))] = fields[3:]
            else:
                raise AssertionError(fields)
    assert rooms == 142 and safe == 3, (rooms, safe)
    assert ground[(0x300, 0)][0] == '1' and ground[(0x300, 0)][2] == '1'
    assert ground[(0x301, 0)][0] == '1' and ground[(0x301, 0)][2] == '1'
    print(f'PASS {rooms} rooms, {doors} original doors, {hits} independent area/facing '
          f'checks, {starts_count} exact RoomInfo records; {safe} audited direct transitions')
    print('RoomInfo SHA256', hashlib.sha256(infos[0]).hexdigest(), 'length', len(infos[0]))
    for key in [(0x300, 0), (0x300, 1), (0x300, 2), (0x301, 0), (0x301, 1)]:
        print('Start', hex(key[0]), key[1], 'ground/snap/clear/polygon', *ground[key])
