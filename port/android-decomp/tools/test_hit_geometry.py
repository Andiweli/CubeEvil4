#!/usr/bin/env python3
"""Compare native ray hit math with verbatim frozen game and SDK functions.

Usage: test_hit_geometry.py hit_geometry_tool [--cases 2000]
Requires a C++17 compiler and the checked-in upstream/re4 source tree.
No game assets are needed. Generated source and executables are temporary.
The reference uses the original SDK C operations and game fdlibm, with the
same documented scalar SQRTF replacement as the upstream motion host helper.
It does not execute GameCube paired-single assembly or link production math.
"""
from pathlib import Path
import argparse
import hashlib
import math
import random
import re
import struct
import subprocess
import tempfile


def extract(path, names):
    source = path.read_text()
    output = []
    for name in names:
        signature = re.search(r'^[A-Za-z_][\w\s*&:]*\b' + re.escape(name)
                              + r'\([^;]*?\)\s*\{', source, re.MULTILINE)
        if signature is None:
            raise RuntimeError(f'{path}: function {name} not found')
        depth, end = 1, signature.end()
        while depth:
            if source[end] == '{':
                depth += 1
            elif source[end] == '}':
                depth -= 1
            end += 1
        body = source[signature.start():end]
        line = source.count('\n', 0, signature.start()) + 1
        output.append(f'#line {line} "{path}"\n' + body + '\n')
    return '\n'.join(output)


def build_oracle(root, directory):
    upstream = root / 'upstream/re4'
    fragments = [
        extract(upstream / 'src/lib/vec.c', [
            'C_VECAdd', 'C_VECSubtract', 'C_VECScale', 'C_VECNormalize',
            'C_VECSquareMag', 'C_VECMag', 'C_VECDotProduct', 'C_VECCrossProduct',
        ]),
        extract(upstream / 'src/lib/mtx.c', [
            'C_MTXIdentity', 'C_MTXCopy', 'C_MTXConcat', 'C_MTXInverse',
            'C_MTXRotAxisRad',
        ]),
        extract(upstream / 'src/lib/mtxvec.c', ['C_MTXMultVec', 'C_MTXMultVecSR']),
        extract(upstream / 'src/game/math_sub.cpp', ['TransMatrix']),
        extract(upstream / 'src/game/sub2.cpp', ['LineSphereCrossCk']),
        extract(upstream / 'src/game/em_sub.cpp', [
            'emLinePolyCrossCk', 'emLineCubeCrossCk', 'emLineCapsuleCrossCk',
            'HitParts', 'HandgunCk', 'emLineAtCk',
        ]),
    ]
    generated = directory / 'original-hit-functions.cpp'
    generated.write_text('#include "' + str(root / 'tools/hit_geometry_oracle.cpp')
                         + '"\n#define cEm OracleEnemy\n' + '\n'.join(fragments))
    flags = ['-std=c++17', '-O2', '-fno-fast-math', '-fno-strict-aliasing',
             '-ffp-contract=off']
    libm = ['sf_sin', 'sf_cos', 'kf_sin', 'kf_cos', 'ef_rem_pio2',
            'kf_rem_pio2', 'sf_scalbn', 'sf_floor', 'sf_fabs', 'ef_acos', 'ef_sqrt']
    definitions = [f'-D{name}=re4_{name}' for name in
                   ['sinf', 'cosf', 'acosf', 'fabsf', 'floorf', 'scalbnf']]
    subprocess.run(['g++', *flags, '-I', str(upstream / 'include'),
                    *definitions, '-c',
                    *(str(upstream / 'src/lib' / (name + '.cpp')) for name in libm)],
                   cwd=directory, check=True)
    executable = directory / 'hit_geometry_oracle'
    subprocess.run(['g++', *flags, '-Wall', '-Wextra', '-Werror',
                    '-I', str(upstream / 'tools/motion/host/stub'),
                    str(generated),
                    *(str(directory / (name + '.o')) for name in libm),
                    '-o', str(executable)], check=True)
    source_hash = hashlib.sha256('\n'.join(fragments).encode()).hexdigest()[:16]
    return executable, source_hash


def f32(value):
    return struct.unpack('>f', struct.pack('>f', value))[0]


def add(a, b):
    return [x + y for x, y in zip(a, b)]


def scale(a, factor):
    return [x * factor for x in a]


def identity():
    return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0]


def cases(count):
    result = []

    def capsule(name, start, end, top=(0, 2, 0), bottom=(0, 0, 0), radius=1):
        result.append((name, 'C', [*start, *end, *top, *bottom, radius]))

    def box(name, start, end, matrix=None, offset=(0, 0, 0),
            dimensions=(1, 2, 1)):
        result.append((name, 'B', [*start, *end,
                                   *(identity() if matrix is None else matrix),
                                   *offset, *dimensions]))

    def shape(offset=(0, 0, 0), radius=1, height=2, extent=1, flags=1, part=0):
        return [*offset, radius, height, extent, flags, part]

    def authored(name, start, end, shapes, joints=None, placement=None,
                 maximum=1e12, weapon=1):
        joints = [identity()] if joints is None else joints
        result.append((name, 'A', [*start, *end, maximum, weapon,
                                   *(identity() if placement is None else placement),
                                   len(joints), len(shapes),
                                   *(value for matrix in joints for value in matrix),
                                   *(value for item in shapes for value in item)]))

    capsule('cylinder entry', (-3, 1, 0), (3, 1, 0))
    capsule('cylinder miss', (-3, 1, 2), (3, 1, 2))
    capsule('strict cylinder tangency', (-3, 1, 1), (3, 1, 1))
    capsule('just inside tangency', (-3, 1, f32(1 - 2 ** -23)),
            (3, 1, f32(1 - 2 ** -23)))
    capsule('just outside tangency', (-3, 1, f32(1 + 2 ** -23)),
            (3, 1, f32(1 + 2 ** -23)))
    capsule('start inside cylinder', (0, 1, 0), (3, 1, 0))
    capsule('zero segment inside cylinder', (0, 1, 0), (0, 1, 0))
    capsule('zero segment outside capsule', (3, 1, 0), (3, 1, 0))
    capsule('start inside top sphere', (0, 2.5, 0), (4, 2.5, 0))
    capsule('top cap vertical entry', (0, 5, 0), (0, -5, 0))
    capsule('bottom cap vertical entry', (0, -5, 0), (0, 5, 0))
    capsule('sphere degenerate capsule', (-3, 0, 0), (3, 0, 0), top=(0, 0, 0))
    capsule('sphere strict tangency', (-3, 1, 0), (3, 1, 0), top=(0, 0, 0))
    capsule('short segment outside', (3, 1, 0), (3.1, 1, 0))
    capsule('start on boundary outward', (-1, 1, 0), (-3, 1, 0))
    capsule('entire segment beyond capsule', (3, 1, 0), (4, 1, 0))
    capsule('oblique capsule', (-5, 1, 0), (5, 1, 0), top=(2, 2, 1))
    capsule('horizontal capsule', (-3, 0, 0), (3, 0, 0), top=(0, 0, 2))
    # The game deliberately skips its axis rotation near pi. Retain the exact
    # resulting downward-axis behavior rather than replacing it analytically.
    capsule('downward axis original lower miss', (-3, -5, 0), (3, -5, 0),
            top=(0, -10, 0))
    capsule('downward axis original upper hit', (-3, 5, 0), (3, 5, 0),
            top=(0, -10, 0))
    capsule('downward axis degenerate projection rejects', (0, -5, 0),
            (3, -5, 0), top=(0, -10, 0))
    capsule('zero radius', (-3, 1, 0), (3, 1, 0), radius=0)
    box('front box entry', (0, 1, 4), (0, 1, -4))
    box('rear box entry', (0, 1, -4), (0, 1, 4))
    box('left box entry', (-4, 1, 0), (4, 1, 0))
    box('right box entry', (4, 1, 0), (-4, 1, 0))
    box('top box entry', (0, 4, 0), (0, -4, 0))
    box('bottom box entry', (0, -4, 0), (0, 4, 0))
    box('box start inside original miss', (0, 1, 0), (4, 1, 0))
    box('box start on face original miss', (0, 1, 1), (0, 1, -4))
    box('box end on face original miss', (0, 1, 4), (0, 1, 1))
    box('box zero segment outside', (0, 1, 4), (0, 1, 4))
    box('box zero segment inside', (0, 1, 0), (0, 1, 0))
    box('box ray in face plane', (-4, 1, 1), (4, 1, 1))
    box('box corner crossing', (4, 1, 4), (-4, 1, -4))
    box('box nonzero offset', (3, 5, 10), (3, 5, -10), offset=(3, 4, 2))
    box('box zero height', (0, 4, 0), (0, -4, 0), dimensions=(1, 0, 1))
    box('box singular matrix', (-4, 1, 0), (4, 1, 0), matrix=[0] * 12)
    box('reflected box retains original winding', (0, 1, 4), (0, 1, -4),
        matrix=[-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0])

    authored('equal nearest part later wins', (-3, 0, 0), (3, 0, 0),
             [shape(height=0), shape(height=0, part=1)])
    authored('nearest boundary inclusive', (-3, 0, 0), (3, 0, 0),
             [shape(height=0)], maximum=4)
    authored('nearest boundary outside', (-3, 0, 0), (3, 0, 0),
             [shape(height=0)], maximum=f32(4 - 2 ** -21))
    authored('disabled first linked part', (-3, 0, 0), (3, 0, 0),
             [shape(height=0, flags=0), shape(height=0)])
    authored('farther later part retains nearer', (-3, 0, 0), (3, 0, 0),
             [shape(height=0), shape(offset=(1, 0, 0), height=0)])
    authored('nearer later part replaces farther', (-3, 0, 0), (3, 0, 0),
             [shape(offset=(1, 0, 0), height=0), shape(height=0)])
    authored('nearest uses three dimensional distance', (-4, -4, 0), (4, 4, 0),
             [shape(height=0)], maximum=11)
    for weapon in range(1, 0x2e):
        authored(f'original HandgunCk weapon {weapon}', (-3, 0, 0), (3, 0, 0),
                 [shape(height=0, flags=17)], weapon=weapon)
    for flags in [1, 3, 5, 7, 9, 17, 33, 65, 129, 16385]:
        y = 0 if flags & 6 else 1
        z = 1 if flags == 5 else 0
        authored(f'authored axis/flags {flags}', (-4, y, z), (4, y, z),
                 [shape(flags=flags, part=1)], weapon=5)
    placement = [0, 0, 1, 5000, 0, 1, 0, -1500, -1, 0, 0, 8000]
    joints = [identity(), [2, .2, 0, 3, 0, 1.5, 0, 1, .1, 0, .5, -2]]
    authored('actor and one based raw joint placement', (4998, -1496, 7995),
             (4998, -1496, 8003), [shape(part=2)], joints, placement)
    authored('actor and model shape placement', (5000, -1499, 7995),
             (5000, -1499, 8005), [shape(part=0)], joints, placement)

    rng = random.Random(0xD37)
    for index in range(count):
        center = [rng.uniform(-20000, 20000) for _ in range(3)]
        axis = [rng.uniform(-1, 1) for _ in range(3)]
        magnitude = math.sqrt(sum(value * value for value in axis))
        axis = scale(axis, 1 / magnitude)
        if index % 7 == 0:
            axis = [0, -1 if index % 2 else 1, 0]
        length = 0 if index % 11 == 0 else rng.uniform(.1, 3000)
        radius = rng.uniform(.1, 1000)
        bottom, top = add(center, scale(axis, -length / 2)), add(center, scale(axis, length / 2))
        direction = [rng.uniform(-1, 1) for _ in range(3)]
        reach = rng.uniform(100, 10000)
        jitter = [rng.uniform(-radius * 2, radius * 2) for _ in range(3)]
        start = add(add(center, jitter), scale(direction, reach))
        end = add(add(center, jitter), scale(direction, -reach))
        if index % 13 == 0:
            start = center
        if index % 17 == 0:
            end = start
        capsule(f'random capsule {index}', start, end, top, bottom, radius)

        yaw, pitch, roll = [rng.uniform(-math.pi, math.pi) for _ in range(3)]
        cx, sx, cy, sy, cz, sz = (math.cos(pitch), math.sin(pitch),
                                  math.cos(yaw), math.sin(yaw),
                                  math.cos(roll), math.sin(roll))
        # Independent Rz*Ry*Rx matrix, plus nonuniform bone scaling and a
        # nonzero offset so the original face order is exercised in world space.
        rotation = [[cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx],
                    [sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx],
                    [-sy, cy * sx, cy * cx]]
        sizes = [rng.uniform(.2, 2) for _ in range(3)]
        matrix = [value for row, translation in zip(rotation, center)
                  for value in [*(component * size for component, size in zip(row, sizes)), translation]]
        dimensions = [rng.uniform(.1, 1200) for _ in range(3)]
        offset = [rng.uniform(-400, 400) for _ in range(3)]
        start = add(center, scale(direction, reach))
        end = add(center, scale(direction, -reach))
        if index % 19 == 0:
            end = start
        box(f'random transformed box {index}', start, end, matrix, offset, dimensions)

        if index % 2 == 0:
            joint_matrices = [identity(), matrix]
            # Nearby local shapes in independently generated bone space. The
            # final duplicate confirms linked-order equality under transforms.
            shapes = [shape(offset, dimensions[0], dimensions[1], dimensions[2],
                            flags=flag, part=part)
                      for flag, part in [(1, 0), (3, 1), (5, 2), (9, 2)]]
            shapes.append(shapes[-1][:])
            authored(f'random posed linked shapes {index}', start, end, shapes,
                     joint_matrices, maximum=1e12, weapon=1 if index % 4 else 5)
    return [(name, kind, [f32(value) for value in values]) for name, kind, values in result]


def read_results(data, count):
    assert len(data) == count * 24, (len(data), count)
    return list(struct.iter_unpack('>I3fIf', data))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tool', type=Path)
    parser.add_argument('--cases', type=int, default=2000)
    arguments = parser.parse_args()
    assert 1 <= arguments.cases <= 100000
    root = Path(__file__).resolve().parent.parent
    test_cases = cases(arguments.cases)
    encoded = ''.join(kind + ' ' + ' '.join(format(value, '.9g') for value in values)
                       + '\n' for _, kind, values in test_cases).encode()
    with tempfile.TemporaryDirectory(prefix='re4-hit-reference-') as temporary:
        oracle, source_hash = build_oracle(root, Path(temporary))
        reference = read_results(subprocess.check_output([str(oracle)], input=encoded), len(test_cases))
        actual = read_results(subprocess.check_output([str(arguments.tool.resolve())], input=encoded), len(test_cases))
    maximum = hits = exact = rejected = maximum_len = maximum_relative_len = 0
    named_results = {}
    for (name, kind, values), observed, expected in zip(test_cases, actual, reference):
        assert observed[0] == expected[0], (name, 'hit/miss', observed, expected, values)
        named_results[name] = expected
        if observed == expected:
            exact += 1
        if expected[0] == 2:
            rejected += 1
        if expected[0] == 1:
            assert all(math.isfinite(value) for value in observed[1:4]), (name, observed)
            difference = max(abs(a - b) for a, b in zip(observed[1:4], expected[1:4]))
            maximum = max(maximum, difference)
            # World coordinates may be tens of thousands of game units. SDK
            # host floats and libm variants allow a small absolute rounding gap.
            assert difference <= .04, (name, difference, observed, expected, values)
            if kind == 'A':
                assert observed[4] == expected[4], (name, 'selected linked part', observed, expected)
                len_difference = abs(observed[5] - expected[5])
                maximum_len = max(maximum_len, len_difference)
                maximum_relative_len = max(maximum_relative_len,
                                           len_difference / max(1, expected[5]))
                assert len_difference <= max(.01, expected[5] * 2e-5), (
                    name, 'squared distance', observed, expected)
            hits += 1
    for name in ['strict cylinder tangency', 'sphere strict tangency',
                 'zero segment outside capsule', 'box start inside original miss',
                 'box start on face original miss', 'box end on face original miss',
                 'downward axis original lower miss']:
        assert named_results[name][0] == 0, (name, named_results[name])
    for name in ['start inside cylinder', 'zero segment inside cylinder',
                 'start inside top sphere', 'downward axis original upper hit']:
        assert named_results[name][0] == 1, (name, named_results[name])
    assert named_results['equal nearest part later wins'][4] == 1
    assert named_results['farther later part retains nearer'][4] == 0
    assert named_results['nearer later part replaces farther'][4] == 1
    assert named_results['disabled first linked part'][4] == 1
    assert named_results['nearest boundary inclusive'][0] == 1
    assert named_results['nearest boundary outside'][0] == 0
    assert named_results['nearest uses three dimensional distance'][0] == 0
    assert named_results['downward axis degenerate projection rejects'][0] == 2
    for name in ['actor and one based raw joint placement',
                 'actor and model shape placement']:
        assert named_results[name][0] == 1
    for flags in [3, 5, 7]:
        assert named_results[f'authored axis/flags {flags}'][0] == 1
    for weapon in range(1, 0x2e):
        assert named_results[f'original HandgunCk weapon {weapon}'][0] == (
            0 if weapon in [1, 2, 3, 4, 0x11, 0x26, 0x2b] else 1)
    assert hits > 10
    print(f'Original hit geometry PASS: {len(test_cases)} geometry cases, '
          f'{hits} hits, {exact} exact records, max difference {maximum:.7f} game units; '
          f'{rejected} nonfinite source results safely rejected')
    print(f'Verbatim upstream game/SDK extraction {source_hash}; '
          'strict tangency, inside/zero segments, downward/oblique axes, '
          'transformed/ordered box faces, posed authored shapes, one-based parts, '
          'HandgunCk, inclusive range and later nearest-part ties verified; '
          f'max squared-distance gap {maximum_len:.7f} '
          f'(relative {maximum_relative_len:.8f})')


if __name__ == '__main__':
    main()
