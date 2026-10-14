#!/usr/bin/env python3
"""Independent original-source M20 EM12 grab-motion comparison.

Usage: test_enemy_grab_data.py enemy_grab_tool EM12.drs
Requires numpy, g++, make and Linux x86-64 for the private original-code host.
All original sources remain read-only; reference builds use a temporary copy.
Original game assets are supplied locally and are never distributed.
"""
from pathlib import Path
import hashlib
import struct
import subprocess
import sys
import tempfile

import numpy as np

import test_action_poses as action_reference
import test_enemy_data as enemy_reference


SOURCES = enemy_reference.SOURCES + [
    (130, 131, 10, 1),  # em10_R1_Catch: ARC86/87, front lunge.
    (651, 0, 5, 1),     # em10_R1_NeckHang: ARC28f, no SEQ.
    (652, 653, 5, 1),   # Failed resistance: ARC290/291.
    (654, 655, 5, 1),   # Successful resistance: ARC292/293.
    (105, 106, 15, 1),  # em10_R1_DownWake: ARC6d/6e.
]
POSE_FRACTIONS = [0, .125, .25, .5, .875, 1]


def build_reference(root, host):
    # Enlarge only the private original-source host. The first five slots keep
    # M19's numbering; reset must visit every loaded clip like AnimationRig's
    # constructor, while the unused slots must never reach MotionSetCore.
    original = enemy_reference.ENEMY_API
    reset = 'for(int i=0;i<5;++i) mot_enemy_set(e,i,0,4,0);'
    assert original.count(reset) == 1
    api = original.replace(reset,
        'for(int i=0;i<64;++i) if(e->image[i]) mot_enemy_set(e,i,0,4,0);')
    for declaration in ['image[5]', 'size[5]', 'sequence[5]']:
        assert api.count(declaration) == 1
        api = api.replace(declaration, declaration.replace('[5]', '[64]'))
    api = api.replace('for(int i=0;i<5;++i)', 'for(int i=0;i<64;++i)')
    enemy_reference.ENEMY_API = api
    try:
        return enemy_reference.build_reference(root, host)
    finally:
        enemy_reference.ENEMY_API = original


class GrabReference(enemy_reference.EnemyReference):
    def start(self, clip, mirrored=False):
        self.lib.mot_enemy_reset(self.e)
        hokan, flags = SOURCES[clip][2:]
        self.lib.mot_enemy_set(self.e, clip, hokan,
                               flags | (0x40 if mirrored else 0), 0)
        self.check_errors('grab start')


def compare_tick(fixture, reference, label, hokan=None):
    playback = fixture.fields('f')[0]
    assert playback == 1, (label, 'playback', playback)
    observed_root = np.array(fixture.fields('4f'), dtype=np.float32)
    observed_state = fixture.fields('ff2I')
    expected_root, expected_state, result, pose = reference.tick(playback)
    assert observed_state == tuple(expected_state[:4]), (
        label, 'curve/SEQ/state/Free', observed_state, expected_state)
    assert observed_state[2] == result, (label, 'MotionMove return', result)
    if hokan is not None:
        assert expected_state[4] == hokan, (label, 'original Hokan', expected_state[4])
    delta = float(np.max(np.abs(observed_root - expected_root)))
    assert np.isfinite(observed_root).all() and delta < .005, (
        label, 'root XYZ/yaw', delta, observed_root, expected_root)
    fixture.compare_pose(pose, label)
    return delta, observed_root, observed_state, pose


def check_geometry(archive, modelbin, meshbin):
    body = modelbin.parse(archive.entries[440][1])
    body.check_tree()
    assert body.n_parts == 34
    counts = {}
    draws = []
    corner = 0
    for entry in [440, 442, 444, 449, 445, 450]:
        attachment = modelbin.parse(archive.entries[entry][1])
        assert attachment.n_parts == body.n_parts
        assert [(p.attach, p.parent) for p in attachment.parts] == [
            (p.attach, p.parent) for p in body.parts], ('Attachment hierarchy', entry)
        assert np.allclose([p.pos for p in attachment.parts],
                           [p.pos for p in body.parts], atol=1e-9, rtol=0), (
            'Attachment rest pose', entry)
        mesh = meshbin.parse(archive.entries[entry][1])
        counts[entry] = enemy_reference.triangle_count(mesh)
        for weight in mesh.weights:
            pairs = weight.pairs()
            assert 1 <= weight.num <= 3 and all(
                0 <= joint < 34 and 0 <= rate <= 1 for joint, rate in pairs), (
                'Original owning joint / weight', entry, pairs)
            assert abs(sum(rate for _, rate in pairs) - 1) < 1e-6
        for part in mesh.parts:
            for primitive in part.prims:
                vertices = len(primitive.verts)
                triangles = vertices // 4 * 2 if primitive.op == 0x80 else (
                    vertices // 3 if primitive.op == 0x90 else vertices - 2)
                if triangles:
                    draws.append((entry, corner, triangles * 3))
                    corner += triangles * 3
    assert sum(counts[e] for e in [440, 442, 444, 449]) == 3013
    assert struct.unpack_from('>2I', archive.entries[441][1]) == (0x0020af30, 3)
    counts['draws'] = draws
    return body, counts


def compare_fixture(fixture, lib, body, motions, sequences, fcv, counts):
    assert fixture.fields('3I') == (0x4d323000, 34, 10)
    maximum_root = 0.
    events = {}
    normal_wake = None
    with GrabReference(lib, body, motions, sequences, fcv) as reference:
        cycles = []
        for clip, source in enumerate(SOURCES):
            assert fixture.fields('4I') == source, ('Source', clip)
            assert fixture.fields('f')[0] == motions[clip].max_frame, ('FCV maximum', clip)
            frames = fixture.word()
            root_cycle = np.array(fixture.fields('4f'), dtype=np.float32)
            original_cycle = reference.cycle(clip)
            assert frames == original_cycle[0], ('Sequence length', clip)
            assert np.max(np.abs(root_cycle - original_cycle[1:])) < .005, (
                'Original cycle XYZ/yaw', clip, root_cycle, original_cycle)
            cycles.append(original_cycle)

        assert fixture.word() == 30
        for clip in range(5, 10):
            for fraction in POSE_FRACTIONS:
                observed_clip, frame = fixture.fields('If')
                assert observed_clip == clip and frame == motions[clip].max_frame * fraction
                fixture.compare_pose(reference.sample(clip, frame), ('EM12 grab FCV', clip, frame))

        assert fixture.word() == 6
        for group in range(6):
            clip = group + 5 if group < 5 else 9
            mirrored = group == 5
            frames = int(cycles[clip][0])
            source = SOURCES[clip]
            ticks = frames + 16
            assert fixture.fields('5I') == (
                clip, mirrored, source[3] | (0x40 if mirrored else 0), source[2], ticks)
            reference.start(clip, mirrored)
            # Cycle metadata uses loop flags4. A no-SEQ action started with
            # flags1 instead has maxFrame+1 frames and clamps at maxFrame.
            end_frame = motions[clip].max_frame if source[1] == 0 else frames - 1
            seen_free = set()
            wake = []
            was_ended = False
            for tick in range(ticks):
                delta, root, state, pose = compare_tick(
                    fixture, reference, ('EM12 grab tick', clip, mirrored, tick),
                    max(0, source[2] - tick - 1))
                maximum_root = max(maximum_root, delta)
                seen_free.add(state[3])
                if clip == 9:
                    wake.append(pose)
                if state[2] == 4:
                    assert state[1] == end_frame, ('Nonloop clamp', clip, state)
                if was_ended:
                    assert np.max(np.abs(root)) < .005, ('Ended root must stop', clip, root)
                was_ended = state[2] == 4
            assert state[2] == 4 and state[1] == end_frame, (
                'Nonloop motion must end', clip, mirrored, state)
            events[(clip, mirrored)] = seen_free
            if clip == 9:
                if mirrored:
                    assert np.max(np.abs(np.array(wake) - normal_wake)) > 1, (
                        'Original mirrored Wake must change the pose')
                else:
                    normal_wake = np.array(wake)

        compare_footer(fixture, reference, counts, int(cycles[8][0]))
    assert fixture.cursor == len(fixture.data), (fixture.cursor, len(fixture.data))
    return maximum_root, events


def compare_footer(fixture, reference, counts, throw_frames):
    # Source DownWakeWait advances the already ended ThrowOff. It does not
    # initialize another FCV, recapture Hokan, or select the authored rest pose.
    reference.start(8)
    for _ in range(throw_frames + 16):
        root, state, result, pose = reference.tick(1)
    assert state[2] == result == 4 and state[1] == throw_frames - 1
    held_pose, held_state = pose.copy(), tuple(state[:4])
    assert fixture.word() == 10
    for tick in range(10):
        _, root, state, pose = compare_tick(
            fixture, reference, ('EM12 ended ThrowOff/DownWakeWait', tick), 0)
        assert np.max(np.abs(root)) < .005 and state == held_state
        assert np.array_equal(pose, held_pose), ('Original ended down pose', tick)

    # Native rendering retains both authored hand variants in one GPU owner.
    # Check every draw range against the separately parsed original GX lists,
    # including order and source BIN identity, not just aggregate geometry.
    entries = [440, 442, 444, 449, 445, 450]
    triangles = sum(counts[entry] for entry in entries)
    assert fixture.fields('5I') == (6, triangles, triangles * 3, 3, len(counts['draws']))
    for draw in counts['draws']:
        assert fixture.fields('3I') == draw, ('Original open-hand draw', draw)
    assert counts[445] > 0 and counts[450] > 0


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    root = Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(root / 'upstream/re4/tools'))
    import drs
    from motion import fcv, modelbin, meshbin

    tool, path = [Path(p).resolve() for p in sys.argv[1:]]
    image = path.read_bytes()
    assert hashlib.sha256(image).hexdigest() == enemy_reference.EM12_SHA, path
    archive = drs.Drs(image)
    body, counts = check_geometry(archive, modelbin, meshbin)
    motions = [fcv.parse(archive.entries[entry][1]) for entry, *_ in SOURCES]
    sequences = [fcv.parse_seq(archive.entries[entry][1]) if entry else None
                 for _, entry, _, _ in SOURCES]
    fixture = action_reference.Fixture(subprocess.check_output([str(tool), str(path)]))
    with tempfile.TemporaryDirectory(prefix='re4-enemy-grab-reference-') as temp:
        lib = build_reference(root, Path(temp) / 'host')
        maximum_root, events = compare_fixture(
            fixture, lib, body, motions, sequences, fcv, counts)
    print(f'Original EM12 grab MotionMove/IK/Hokan/mirror reference PASS: '
          f'{fixture.count} poses, 34 joints, max difference {fixture.maximum:.8f}')
    print(f'Original EM12 grab root/SEQ/Free reference PASS: 6 traces, '
          f'max root difference {maximum_root:.8f}; event sets {events}')
    print('Original EM12 ThrowOff/DownIdle and open-hand BIN/TPL layout PASS')


if __name__ == '__main__':
    main()
