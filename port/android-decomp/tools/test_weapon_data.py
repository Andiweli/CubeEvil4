#!/usr/bin/env python3
"""Independent WEP01 BIN/TPL, original FCV and right-hand attachment reference.

Usage: test_weapon_data.py weapon_tool PL00.drs WEP01.drs
Requires numpy, make, g++, and Linux x86-64 for the private upstream helper.
Only that reference helper uses MAP_32BIT; the Android runtime never does.
The pinned upstream tree is copied before building and remains read-only.
No original assets or derived fixture bytes are distributed.
"""
from pathlib import Path
import ctypes
import hashlib
import math
import shutil
import struct
import subprocess
import sys
import tempfile

import numpy as np


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    root = Path(__file__).resolve().parent.parent
    upstream = root / 'upstream/re4'
    sys.path.insert(0, str(upstream / 'tools'))
    import drs
    from motion import evalhost, fcv, meshbin, modelbin

    tool, pl, wep = [Path(value).resolve() for value in sys.argv[1:]]
    for path, digest in [
        (pl, '5ea520d7026c48e33e12256ef82173140e72cf4019758167a7d5aa5b6e356090'),
        (wep, 'fb838948d4161ee04da306ff0910980ff1b9e3b5701b43983a36e1d21bca3689'),
    ]:
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, path
    player_arc = drs.Drs(pl.read_bytes())
    weapon_arc = drs.Drs(wep.read_bytes())
    weapon_model = modelbin.parse(weapon_arc.entries[2][1])
    player_model = modelbin.parse(player_arc.entries[0][1])
    mesh = meshbin.parse(weapon_arc.entries[2][1])
    data = subprocess.check_output([str(tool), str(pl), str(wep)])
    cursor = 0

    def words(count):
        nonlocal cursor
        result = struct.unpack_from('>' + str(count) + 'I', data, cursor)
        cursor += count * 4
        return result

    def floats(count):
        nonlocal cursor
        result = np.frombuffer(data, dtype='>f4', count=count, offset=cursor)
        cursor += count * 4
        assert np.isfinite(result).all()
        return result.astype(np.float64)

    def pixels():
        nonlocal cursor
        size, = words(1)
        result = data[cursor:cursor + size]
        cursor += size
        assert len(result) == size
        return result

    magic, joints, corners, draws, textures = words(5)
    assert magic == 0x57504631
    assert joints == weapon_model.n_parts == 4
    assert corners == mesh.n_triangles() * 3 == 1815
    assert textures == 1
    bind = []
    for part in weapon_model.parts:
        center = np.array(part.pos, dtype=np.float32)
        if part.parent >= 0:
            center = (center + bind[part.parent]).astype(np.float32)
        bind.append(center)
    bind = np.array(bind, dtype=np.float64)
    assert np.allclose(floats(joints * 3).reshape(joints, 3), bind, atol=1e-6)

    references = [corner for part in mesh.parts for triangle in part.triangles()
                  for corner in triangle]
    positions, normals, position_weights, normal_weights = [], [], [], []
    vertex_colors = bool(mesh.flags & 0x40000000)
    for vi, ni, ci, ti in references:
        expected = (*mesh.position(vi), *mesh.normal(ni), *mesh.texcoord(ti))
        assert np.allclose(floats(8), expected, atol=1e-6, rtol=0)
        positions.append(mesh.position(vi))
        normals.append(mesh.normal(ni))
        for matrix_index, output in [
            (mesh.vtx[vi][3], position_weights),
            (mesh.nrm[ni][3], normal_weights),
        ]:
            weight = mesh.weights[matrix_index]
            count, = words(1)
            assert count == weight.num
            pairs = weight.pairs()
            output.append(pairs)
            for index in range(3):
                joint, = words(1)
                rate, = floats(1)
                expected_joint, expected_rate = pairs[index] if index < count else (0, 0)
                assert joint == expected_joint and abs(rate - expected_rate) < 1e-6
        color = mesh.colour(ci) if vertex_colors else (255, 255, 255, 255)
        assert words(4) == color
    positions = np.array(positions)
    normals = np.array(normals)

    expected_draws = []
    first = 0
    for part in mesh.parts:
        for primitive in part.prims:
            if primitive.op == 0x80:
                triangles = len(primitive.verts) // 4 * 2
            elif primitive.op == 0x90:
                triangles = len(primitive.verts) // 3
            elif primitive.op == 0x98:
                triangles = max(len(primitive.verts) - 2, 0)
            else:
                raise AssertionError(f'Unexpected WEP01 primitive {primitive.op:#x}')
            count = triangles * 3
            if count:
                assert part.tex_id == 0 and not part.flags & 4
                expected_draws.append((first, count, 0, part.tex_id, 0, 0xffffffff,
                                       part.alpha_tex, part.alpha_ref, int(vertex_colors)))
                first += count
    assert draws == len(expected_draws) == 93
    for expected in expected_draws:
        assert words(9) == expected

    # Decode the original CMPR bytes independently, including the console's
    # 5/8 and 3/8 opaque interpolation and RGB values in transparent selectors.
    tpl = weapon_arc.entries[1][1]
    u32 = lambda at: struct.unpack_from('>I', tpl, at)[0]
    u16 = lambda at: struct.unpack_from('>H', tpl, at)[0]
    assert u32(0) == 0x0020af30 and u32(4) == 1
    header = u32(u32(8))
    height, width = u16(header), u16(header + 2)
    texture_format = u32(header + 4)
    source = u32(header + 8)
    wrap_s, wrap_t = u32(header + 12), u32(header + 16)
    assert words(8) == (width, height, texture_format, wrap_s, wrap_t,
                        tpl[header + 33], tpl[header + 34], 0)
    assert (width, height, texture_format) == (128, 64, 14)

    def rgb565(value):
        r, g, b = value >> 11, value >> 5 & 63, value & 31
        return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)

    expected_pixels = bytearray()
    for y in range(height):
        for x in range(width):
            tile = source + (y // 8 * ((width + 7) // 8) + x // 8) * 32
            block = tile + (y % 8 // 4 * 2 + x % 8 // 4) * 8
            a, b = u16(block), u16(block + 2)
            ca, cb = rgb565(a), rgb565(b)
            selector = tpl[block + 4 + y % 4] >> (6 - 2 * (x % 4)) & 3
            if selector < 2:
                rgba = (*(ca if selector == 0 else cb), 255)
            elif a > b:
                wa, wb = (5, 3) if selector == 2 else (3, 5)
                rgba = (*( (va * wa + vb * wb) // 8 for va, vb in zip(ca, cb)), 255)
            else:
                rgba = (*( (va + vb) // 2 for va, vb in zip(ca, cb)),
                        255 if selector == 2 else 0)
            expected_pixels.extend(rgba)
    assert pixels() == expected_pixels
    representatives = words(3)
    assert all(index < corners for index in representatives)
    assert [position_weights[index] for index in representatives] == [
        [(0, 1.0)], [(1, 1.0)], [(2, 1.0)],
    ]
    print(f'Independent WEP01 BIN/TPL PASS: {corners} corners, {draws} draws, '
          f'{width * height} RGBA pixels, separate position/normal weights')

    def affine(matrix):
        return np.vstack((np.array(matrix).reshape(3, 4), [0, 0, 0, 1]))

    def frames(maximum):
        return [0, 3, 6, maximum * .125, maximum * .25, maximum * .5,
                maximum * .875, maximum, min(maximum, 14),
                min(maximum, 31), min(maximum, 39)]

    poses = attachment_corners = 0
    maximum_pose_difference = maximum_attachment_difference = 0
    with tempfile.TemporaryDirectory(prefix='re4-weapon-reference-') as tmp:
        host = Path(tmp) / 'host'
        shutil.copytree(upstream / 'tools/motion/host', host,
                        ignore=shutil.ignore_patterns('build', '__pycache__'))
        make = host / 'Makefile'
        make.write_text(make.read_text().replace(
            'ROOT   := $(abspath ../../..)', 'ROOT   := ' + str(upstream)))
        evalhost.HOST_DIR = str(host)
        attachments = []
        for clip, entry in enumerate([7, 13, 17]):
            observed_clip, = words(1)
            observed_frame, = floats(1)
            motion = fcv.parse(weapon_arc.entries[entry][1])
            expected_frame = motion.max_frame * .375
            assert observed_clip == clip and observed_frame == expected_frame
            reference = evalhost.Player(player_model, motion, loop=True, ik=True)
            expected_hand = affine(reference.frame(expected_frame).mat[10])
            observed_hand = affine(floats(12))
            assert np.max(np.abs(observed_hand - expected_hand)) < .08
            placement = affine(floats(12))
            yaw = float(np.float32(.73))
            expected_placement = np.array([
                [math.cos(yaw), 0, math.sin(yaw), 1300],
                [0, 1, 0, -200],
                [-math.sin(yaw), 0, math.cos(yaw), 950],
                [0, 0, 0, 1],
            ])
            assert np.allclose(placement, expected_placement, atol=1e-4, rtol=0)
            reference.place((1300, -200, 950), (0, yaw, 0), (1, 1, 1))
            expected_attachment = affine(reference.frame(expected_frame).mat[10])
            observed_attachment = affine(floats(12))
            difference = float(np.max(np.abs(observed_attachment - expected_attachment)))
            maximum_attachment_difference = max(maximum_attachment_difference, difference)
            assert difference < .08, (clip, 'raw right-hand attachment', difference)
            # Translation in the bind center of Leon #10 must survive; this
            # comparison rejects using Leon's inverse-bind skinning palette.
            assert np.linalg.norm(observed_attachment[:3, 3] - placement[:3, 3]) > 100
            attachments.append(expected_attachment)

        idle = fcv.parse(weapon_arc.entries[48][1])
        for clip, entry in enumerate([48, 46, 50, 53, 51, 47]):
            motion = fcv.parse(weapon_arc.entries[entry][1])
            # Production cObjFn57::init first calls resetMotion with normal
            # idle. Preserve that pose for channels omitted by later FCVs.
            reference = evalhost.Player(weapon_model, idle, loop=True, ik=True)
            reference.frame(0)
            image = fcv.serialise(motion, '<')
            buffer = (ctypes.c_uint8 * len(image)).from_buffer_copy(image)
            reference.lib.mot_host_clear_errors()
            assert reference.lib.mot_model_set(reference.handle, buffer, len(image), 4, 1) == len(motion.joints)
            assert not reference.lib.mot_host_errors()
            for frame in frames(motion.max_frame):
                observed_clip, = words(1)
                observed_frame, observed_maximum = floats(2)
                assert (observed_clip, observed_frame, observed_maximum) == (clip, frame, motion.max_frame)
                observed = floats(joints * 12).reshape(joints, 12)
                expected = np.array(reference.frame(frame).mat, dtype=np.float64)
                difference = float(np.max(np.abs(observed - expected)))
                maximum_pose_difference = max(maximum_pose_difference, difference)
                assert difference < .08, (clip, frame, difference)
                for attachment in attachments:
                    for corner in representatives:
                        observed_position, observed_normal = floats(3), floats(3)
                        expected_position = np.zeros(3)
                        expected_normal = np.zeros(3)
                        for joint, rate in position_weights[corner]:
                            bone = affine(expected[joint])
                            value = np.append(positions[corner] - bind[joint], 1)
                            expected_position += rate * (bone @ value)[:3]
                        for joint, rate in normal_weights[corner]:
                            expected_normal += rate * affine(expected[joint])[:3, :3] @ normals[corner]
                        expected_position = (attachment @ np.append(expected_position, 1))[:3]
                        expected_normal = attachment[:3, :3] @ expected_normal
                        difference = float(np.max(np.abs(observed_position - expected_position)))
                        maximum_attachment_difference = max(maximum_attachment_difference, difference)
                        assert difference < .08, (clip, frame, corner, 'position', difference)
                        assert np.allclose(observed_normal, expected_normal, atol=1e-4, rtol=1e-4), (clip, frame, corner, 'normal')
                        attachment_corners += 1
                poses += 1
    assert cursor == len(data), (cursor, len(data))
    assert poses == 66 and attachment_corners == 594
    print(f'Original WEP01 MotionMove PASS: {poses} poses, {joints} local joints, '
          f'max difference {maximum_pose_difference:.7f} game units')
    print(f'Original right-hand attachment PASS: idle/walk/run Leon poses, '
          f'{attachment_corners} posed corners and normals, nonzero placement/yaw, '
          f'max difference {maximum_attachment_difference:.7f} game units')


if __name__ == '__main__':
    main()
