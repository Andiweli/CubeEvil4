#!/usr/bin/env python3
"""Create source-free, synthetic DRS fixtures for M20 state/lifecycle tests.

Usage: make_grab_fixtures.py OUTPUT_DIRECTORY

These tiny triangles, flat colours and generated curves are test data, not
original RE4 assets. Source archive indices exercise the production readers;
the curves deliberately do NOT validate original poses or root trajectories.
No downloaded game data is read, embedded or copied by this tool.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct


def half(b, p, value):
    struct.pack_into('>H', b, p, value)


def word(b, p, value):
    struct.pack_into('>I', b, p, value)


def floating(b, p, value):
    struct.pack_into('>f', b, p, value)


def align32(value):
    return (value + 31) & ~31


def model(joints, image=0):
    """One rigid RGB-textured triangle with an owning, ordered skeleton."""
    weights = 64 + 16 * joints
    vertices = weights + 8
    normals = vertices + 24
    uv = normals + 8
    display = align32(uv + 12)
    b = bytearray(display + 64)
    for p, value in [(0, 64), (16, uv), (20, weights), (28, display),
                     (48, vertices), (52, normals), (60, 0x20010801)]:
        word(b, p, value)
    b[24], b[25] = 1, joints
    half(b, 26, 1)
    half(b, 56, 3)
    half(b, 58, 1)
    for i in range(joints):
        p = 64 + i * 16
        b[p], b[p + 1] = i, 255 if i == 0 else 0
        # A generated finite rest pose, shared byte-for-byte by attachments.
        center = (0, 0, 0) if i == 0 else ((i % 4) * 50, 500 + i * 15,
                                          (i % 3) * 30)
        struct.pack_into('>3f', b, p + 4, *center)
    b[weights + 3] = 1  # One joint, implied final weight = 100 percent.
    for i, point in enumerate([(-200, 0, 0), (200, 0, 0), (0, 1000, 0)]):
        struct.pack_into('>3hH', b, vertices + i * 8, *point, 0)
    struct.pack_into('>3hH', b, normals, 0, 0, 16384, 0)
    for i, coord in enumerate([(0, 0), (32768, 0), (16384, 32768)]):
        struct.pack_into('>2H', b, uv + i * 4, *coord)
    b[display + 12] = image
    word(b, display + 24, 32)
    b[display + 32] = 0x90  # GX triangles, no vertex colours.
    half(b, display + 33, 3)
    for i in range(3):
        struct.pack_into('>3H', b, display + 35 + i * 6, i, 0, i)
    return bytes(b)


def palette(images=1):
    """Generated 4x4 RGB565 red/green/blue image tiles, no alpha or mipmaps."""
    headers = 12 + images * 8
    pixels = align32(headers + images * 36)
    b = bytearray(pixels + images * 32)
    word(b, 0, 0x0020af30)
    word(b, 4, images)
    word(b, 8, 12)
    for i in range(images):
        h = headers + i * 36
        word(b, 12 + i * 8, h)
        half(b, h, 4)
        half(b, h + 2, 4)
        word(b, h + 4, 4)  # RGB565
        word(b, h + 8, pixels + i * 32)
        colour = (0xf800, 0x07e0, 0x001f)[i % 3]
        for p in range(pixels + i * 32, pixels + (i + 1) * 32, 2):
            half(b, p, colour)
    return bytes(b)


def motion(frames=50, signature=0):
    """Finite zero-root motion plus a small generated root-joint rotation.

    Format F stores direct float keys. The root-position channel remains zero
    on all axes, so paired state tests never rely on invented authored travel.
    A second channel makes snapshots of distinct motions distinguishable.
    """
    channels = [(0xf001, 0, (0., 0., 0.), (0., 0., 0.)),
                (0xf002, 0, (0., signature * .0001, 0.),
                 (0., signature * .0001 + .015, .025))]
    table = (3 + len(channels) * 3 + 3) & ~3
    begin = table + 4 + len(channels) * 4
    stride = 3 * (2 + 2 * 2 + 2 * 4)
    offsets = []
    size = begin
    for _ in channels:
        size = (size + 3) & ~3  # Type-F blocks start on a four-byte boundary.
        offsets.append(size)
        size += stride
    b = bytearray(size)
    half(b, 0, frames)
    b[2] = len(channels)
    word(b, table, len(b))
    for i, (kind, part, start, end) in enumerate(channels):
        half(b, 3 + i * 2, kind)
        b[3 + len(channels) * 2 + i] = part
        p = offsets[i]
        word(b, table + 4 + i * 4, p)
        for axis in range(3):
            half(b, p, 2)
            half(b, p + 2, 0)
            half(b, p + 4, frames)
            floating(b, p + 6, start[axis])
            floating(b, p + 10, end[axis])
            p += 14
    return bytes(b)


def walk_motion(frames=50, speed=100.):
    """Synthetic positive-Z root travel with unchanged skeletal rest pose.

    This is the source Hermite-channel layout, not an authored game curve.
    Its arbitrary 100 units/tick lets a test enemy reach a distant player.
    The two equal tangents express linear travel over the complete cycle.
    """
    b = bytearray(106)
    half(b, 0, frames)
    b[2] = 1
    half(b, 3, 1)  # Format 0, null/root-position channel.
    word(b, 8, len(b))
    word(b, 12, 16)
    for axis in range(3):
        p = 16 + axis * 30
        distance = frames * speed if axis == 2 else 0.
        half(b, p, 2)
        half(b, p + 2, 0)
        half(b, p + 4, frames)
        for offset, value in [(6, 0.), (10, distance), (14, distance),
                              (18, distance), (22, distance), (26, distance)]:
            floating(b, p + offset, value)
    return bytes(b)


def sequence(frames=50, catch=False):
    b = bytearray(4 + frames * 4)
    half(b, 0, frames)
    for i in range(frames):
        half(b, 4 + i * 4, i * 64)
        if catch and 14 <= i <= 24:
            b[7 + i * 4] = 2  # SEQ Free, not Attr.
    return bytes(b)


def archive(count, entries):
    """A 0x400 DRS envelope and a sparse, aligned original-layout table."""
    body = bytearray(align32(16 + count * 8))
    word(body, 0, count)
    for index in range(count):
        word(body, 16 + index * 4, len(body))
        if index in entries:
            tag, payload = entries[index]
            body[16 + count * 4 + index * 4:
                 20 + count * 4 + index * 4] = tag.encode('ascii') + b'\0'
            body.extend(payload)
            body.extend(bytes(align32(len(body)) - len(body)))
    header = bytearray(0x400)
    signature = b'SYNTHETIC M20 TEST DATA ONLY!!'
    header[:len(signature)] = signature
    word(header, 0x24, len(body))
    word(header, 0x2c, 0x400)
    word(header, 0x40, 0xfffffffe)
    word(header, 0x4c, len(header) + len(body))
    word(header, 0x60, 0xffffffff)
    return bytes(header + body)


def build(directory):
    directory.mkdir(parents=True, exist_ok=True)
    pl = {i: ('BIN', model(119)) for i in [0, 2, 4, 5, 6, 20]}
    pl.update({i: ('TPL', palette()) for i in [1, 3, 13]})
    pl[75], pl[76] = ('FCV', motion(70, 75)), ('SEQ', sequence(70))
    # Required player assets since M26..M29: blast/recovery, door, fixed
    # ladder and fall/landing. These remain synthetic zero-root fixtures;
    # they are never evidence for original trajectory or timing validation.
    for i in [24, 25, 26, 28, 29, 30, 35, 36, 37, 38, 39, 40, 45, 74, 77, 78, 85]:
        pl[i] = ('FCV', motion(50, i))
    for i in [98, 99]:
        pl[i] = ('SEQ', sequence(50))
    for i in [41, 43]:
        pl[i], pl[i + 1] = ('FCV', motion(50, i)), ('SEQ', sequence(50))
    wep = {1: ('TPL', palette()), 2: ('BIN', model(4)),
           6: ('BIN', model(119))}
    for i in [7, 9, 11, 13, 15, 17, 19, 30, 32]:
        wep[i], wep[i + 1] = ('FCV', motion(50, i)), ('SEQ', sequence())
    for i in [34, 35, 36, 37, 38, 39, 41, 46, 47, 48, 50, 51, 53]:
        wep[i] = ('FCV', motion(50, i))
    em = {440: ('BIN', model(34, 1)), 441: ('TPL', palette(3)),
          442: ('BIN', model(34, 0))}
    em.update({i: ('BIN', model(34, 2)) for i in [444, 445, 449, 450]})
    for i, frames in [(1, 50), (4, 50), (34, 50), (40, 50), (99, 50),
                      (105, 70), (130, 50), (651, 50), (652, 65),
                      (654, 80), (659, 50), (660, 55), (661, 60)]:
        em[i] = ('FCV', motion(frames, i))
    em[4] = ('FCV', walk_motion())
    for i in range(5, 10):
        em[i] = ('SEQ', sequence())  # Native emsetNo modulo-five choice.
    for i, frames in [(35, 50), (41, 50), (100, 50), (106, 70),
                      (131, 50), (653, 65), (655, 80)]:
        em[i] = ('SEQ', sequence(frames, catch=i == 131))
    result = {'synthetic': True, 'original_asset_validation': False,
              'purpose': 'Generated parser, state and GPU lifecycle fixtures',
              'root_translation': 'zero except synthetic EM12 walk raw4',
              'synthetic_walk': {'motion_entry': 4, 'units_per_tick': 100,
                                 'direction': 'positive local Z',
                                 'original_speed_validation': False},
              'files': {}}
    for name, count, entries in [('PL00.drs', 132, pl),
                                  ('WEP01.drs', 54, wep),
                                  ('EM12.drs', 672, em)]:
        data = archive(count, entries)
        (directory / name).write_bytes(data)
        result['files'][name] = {
            'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
            'entries': {str(i): tag for i, (tag, _) in sorted(entries.items())}}
        print(f'SYNTHETIC {directory / name}: {len(data)} bytes')
    (directory / 'manifest.json').write_text(
        json.dumps(result, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output_directory', type=Path)
    build(parser.parse_args().output_directory)
