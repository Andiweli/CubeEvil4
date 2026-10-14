#!/usr/bin/env python3
"""Independent primary/common SMD routing and TPL descriptor reference.
Usage: test_room_resources.py yz2_tool room_resource_tool GAME_ROOT
Original game files are not distributed. Checks every draw, not just totals.
"""
from pathlib import Path
import hashlib
import struct
import subprocess
import sys
import tempfile

if len(sys.argv) != 4:
    raise SystemExit(__doc__)
yz2, tool = [str(Path(p).resolve()) for p in sys.argv[1:3]]
game = Path(sys.argv[3]).resolve()
root = Path(__file__).resolve().parent.parent
u32 = lambda b, p: struct.unpack_from('>I', b, p)[0]
u16 = lambda b, p: struct.unpack_from('>H', b, p)[0]

def works(s):
    start = 20 + 4*u32(s, 16) if s[1] & 1 else 16
    return [s[start+i*72:start+(i+1)*72] for i in range(u16(s, 2))]

def resource(s, header, index):
    table = u32(s, header)
    return table + u32(s, table+index*4)

def descriptor(s, palette, index):
    assert u32(s, palette) == 0x20af30
    header = palette + u32(s, palette+u32(s, palette+8)+index*8)
    return (u16(s, header+2), u16(s, header), u32(s, header+4))

def triangle_count(op, n):
    if op == 128:
        assert n % 4 == 0
        return n//4*2
    if op == 144:
        assert n % 3 == 0
        return n//3
    if op in (152, 160):
        return max(n-2, 0)
    if op == 168:
        assert n % 2 == 0
    assert op in (168, 176, 184)
    return 0

rooms = supported = draw_total = textured_total = common_total = alpha_total = 0
smx_records = colored_triangles = 0
with tempfile.TemporaryDirectory(prefix='re4-resources-') as tmp:
    decoded = Path(tmp)/'room.dat'
    for line in (root/'app/src/main/assets/rooms.tsv').read_text().splitlines():
        if line.startswith('#'):
            continue
        disc, path, size, sha, srcsha = line.split('\t')
        source = game/('disc'+disc)/'files'/path
        assert hashlib.sha256(source.read_bytes()).hexdigest() == srcsha
        subprocess.run([yz2, str(source), str(decoded)], check=True, stdout=subprocess.DEVNULL)
        data = decoded.read_bytes()
        assert len(data) == int(size) and hashlib.sha256(data).hexdigest() == sha
        n = u32(data, 0)
        offsets = [u32(data, 16+i*4) for i in range(n)]
        smds = [data[o:min((v for v in offsets if v > o), default=len(data))]
                for i, o in enumerate(offsets)
                if data[16+n*4+i*4:19+n*4+i*4] == b'SMD']
        actual = subprocess.check_output([tool, str(decoded)], text=True).splitlines()
        smx_match = {}
        xs = [data[o:min((v for v in offsets if v > o), default=len(data))]
              for i, o in enumerate(offsets) if data[16+n*4+i*4:19+n*4+i*4] == b'SMX']
        expected_records = []
        if xs:
            x = xs[0]
            assert x[0] == 16 and len(x) >= 16+x[1]*144
            for j in range(x[1]):
                w = x[16+j*144:16+(j+1)*144]
                h = 2166136261
                for byte in w[16:136]: h = ((h ^ byte)*16777619) & 0xffffffff
                expected_records.append([*w[:4], u32(w,4), u32(w,8), u32(w,12),
                                         u32(w,132), u32(w,136), u32(w,140), h])
                smx_match.setdefault(w[0], w)
        observed_records = [list(map(int, row.split()[1:])) for row in actual if row.startswith('X ')]
        assert observed_records == expected_records, (path, 'SMX records')
        smx_records += len(expected_records)
        actual = [row for row in actual if not row.startswith('X ')]
        rooms += 1
        if not smds or smds[0][0] != 64:
            assert actual == []
            continue
        supported += 1
        s = smds[0]
        common = smds[1] if len(smds) > 1 else None
        if common is not None:
            assert common[0] == 64
        expected = []
        included = skipped = tris = base_ok = base_no = common_base = common_alpha = common_bins = 0
        for w in works(s):
            if w[39] == 255:
                continue
            is_common = bool(u32(w, 68) & 16)
            if w[38] != 255 or (is_common and common is None):
                skipped += 1
                continue
            src = common if is_common else s
            b = resource(src, 4, w[36] if w[36] != 255 else 0)
            joint = b + u32(src, b)
            if (src[b+25] != 1 or src[b+24] > 1 or u16(src, b+42) > 255
                    or src[joint] != 0 or src[joint+1] != 255):
                skipped += 1
                continue
            palette_id = 0 if is_common or w[37] == 255 else w[37]
            palette = None if is_common else resource(s, 8, palette_id)
            local_count = 0 if palette is None else u32(s, palette+4)
            cp = resource(common, 8, 0) if common is not None else None
            cp_count = u32(common, cp+4) if cp is not None else 0

            def resolve(image):
                if image >= 248:
                    return (-1, -1, -1), False
                if image < local_count:
                    return descriptor(s, palette, image), False
                image -= local_count
                if image < cp_count:
                    return descriptor(common, cp, image), True
                return (-1, -1, -1), False

            part = b + u32(src, b+28)
            stride = 8 if u32(src, b+32) & 0x80000000 else 6
            placement_tris = 0
            for _ in range(u16(src, b+26)):
                image, alpha_image = src[part+12], src[part+14]
                alpha_test = bool(src[part+11] & 4)
                cur, stop = part+32, part+32+u32(src, part+24)
                while cur < stop:
                    op = src[cur]
                    cur += 1
                    if op == 0:
                        continue
                    assert op & 7 == 0
                    count = u16(src, cur)
                    cur += 2 + count*stride
                    assert cur <= stop
                    nt = triangle_count(op, count)
                    if not nt:
                        continue
                    base, cb = resolve(image)
                    mask, ca = resolve(alpha_image) if alpha_test else ((-1, -1, -1), False)
                    match = smx_match.get(w[39]) if w[39] <= 249 else None
                    rgb = list(match[12:15]) if match is not None and u32(match,12) & 0xffffff00 else [255]*3
                    if rgb != [255]*3: colored_triangles += nt
                    blend = match[15] if match is not None else 0
                    order = match[2] if match is not None else 3
                    reference = 128 if alpha_test and match is not None and u32(match,8)&8 else src[part+20]
                    uv_speed = [u32(match,136),u32(match,140)] if match is not None else [0,0]
                    expected.append([nt*3, palette_id, image, int(is_common), int(alpha_test), alpha_image,
                                     *base, *mask, w[39], *rgb, blend, order, reference, *uv_speed])
                    placement_tris += nt
                    if base[0] >= 0:
                        base_ok += nt
                        common_base += nt if cb else 0
                        common_alpha += nt if alpha_test and mask[0] >= 0 and ca else 0
                    else:
                        base_no += nt
                part = stop
            if placement_tris:
                included += 1
                common_bins += int(is_common)
                tris += placement_tris
            else:
                skipped += 1
        assert actual[0] == 'S ' + ' '.join(map(str, [included, skipped, tris, base_ok, base_no,
                                                     common_base, common_alpha, common_bins])), (path, actual[0])
        observed = [list(map(int, row.split()[1:])) for row in actual[1:]]
        assert observed == expected, (path, 'descriptor/source mismatch')
        draw_total += len(expected)
        textured_total += base_ok
        common_total += common_base
        alpha_total += common_alpha
        if path in ('St1/r100.das', 'St3/r300.das'):
            print(path, actual[0], len(expected), 'draws PASS')
print(rooms, 'rooms;', supported, 'supported primary SMD;', draw_total, 'draws;',
      textured_total, 'textured triangles;', common_total, 'common base triangles;',
      alpha_total, 'common mask triangles PASS')

print(smx_records, "SMX records;", colored_triangles, "material-color triangles PASS")
