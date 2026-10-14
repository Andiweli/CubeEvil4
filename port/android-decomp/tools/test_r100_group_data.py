#!/usr/bin/env python3
"""Compare seven original r100 EM12 actor/tool poses against original MotionMove.
Usage: test_r100_group_data.py r100_group_tool EM12.drs emleon00.esl
All original source/data stay read-only and local; reference host builds in temp.
"""
from pathlib import Path
import hashlib
import subprocess
import sys
import tempfile
import numpy as np
import test_action_poses as action_reference
import test_enemy_data as enemy_reference


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    root = Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(root / 'upstream/re4/tools'))
    import drs
    from motion import fcv, modelbin
    tool, path, esl = [Path(p).resolve() for p in sys.argv[1:]]
    data = path.read_bytes()
    assert hashlib.sha256(data).hexdigest() == enemy_reference.EM12_SHA
    archive = drs.Drs(data)
    # index/model/tool/left, source profile; every original weapon family,
    # mirrored short sickle and all three source hoe-height motion choices.
    profiles = [(3, 4, 2, False, 26), (4, 3, 1, False, 27),
                (4, 3, 1, False, 28), (4, 3, 1, False, 29),
                (5, 1, 3, True, 26), (0x12, 3, 1, False, 28),
                (0x13, 4, 2, True, 26), (0x26, 1, 3, False, 26),
                (0x27, 0, 2, False, 26)]
    ticks_total = 0
    maximum_root = maximum_attachment = 0.
    old_sources = enemy_reference.SOURCES
    with tempfile.TemporaryDirectory(prefix='re4-r100-group-source-') as temp:
        lib = enemy_reference.build_reference(root, Path(temp) / 'host')
        for index, model, weapon, left, motion in profiles:
            body_entry = {0: 440, 1: 468, 3: 472, 4: 476}[model]
            body = modelbin.parse(archive.entries[body_entry][1])
            if weapon == 1:
                idle, walk, seq = 326, 327, 328 + index % 5
                attack, attackseq = {27: (345, 346), 28: (347, 348), 29: (349, 350)}[motion]
            else:
                idle, walk, seq = 1, 4, 5 + index % 5
                attack, attackseq = 124, 125
            sources = [(idle, 0, 10, 5), (walk, seq, 7, 5), (40, 41, 6, 1),
                       (34, 35, 6, 1), (attack, attackseq, 10, 1)]
            motions = [fcv.parse(archive.entries[entry][1]) for entry, *_ in sources]
            sequences = [fcv.parse_seq(archive.entries[entry][1]) if entry else None
                         for _, entry, _, _ in sources]
            enemy_reference.SOURCES = sources
            fixture = action_reference.Fixture(subprocess.check_output(
                [str(tool), str(path), str(esl), str(index), str(motion)]))
            assert fixture.fields('9I') == (0x4d333641, model, weapon, 16 if left else 10,
                                           left, attack, attackseq, 10, 0x41 if left else 1)
            with enemy_reference.EnemyReference(lib, body, motions, sequences, fcv) as reference:
                frames, maximum = fixture.fields('If')
                observed_cycle = np.array(fixture.fields('4f'), dtype=np.float32)
                expected_cycle = reference.cycle(4)
                assert frames == expected_cycle[0] and maximum == motions[4].max_frame
                assert np.max(np.abs(observed_cycle - expected_cycle[1:])) < .005
                ticks = fixture.word()
                assert ticks == frames + 16
                ticks_total += ticks
                reference.start(4, left)
                # Original em10WeaponSet's source offsets/angles for right
                # and left parent parts; attachment translation remains local.
                if weapon == 1:
                    y = np.float32(.7166758 if left else -.7166758)
                    z = np.float32(0)
                    xyz = [843.58 if left else -843.58, 5.81, 866.87]
                else:
                    y = np.float32(-1.0402162)
                    z = np.float32(3.1415927 if left else 0)
                    xyz = [313.85 if left else -313.85, -21.2, 102.21]
                cy, sy, cz, sz = np.cos(y), np.sin(y), np.cos(z), np.sin(z)
                local = np.array([[cz*cy, -sz, cz*sy, xyz[0]],
                                  [sz*cy, cz, sz*sy, xyz[1]],
                                  [-sy, 0, cy, xyz[2]]], dtype=np.float32)
                free_values = set()
                for tick in range(ticks):
                    root_step = np.array(fixture.fields('4f'), dtype=np.float32)
                    curve, sequence, state, free = fixture.fields('ff2I')
                    expected_root, expected_state, result, pose = reference.tick(1)
                    assert (curve, sequence, state, free) == tuple(expected_state[:4]), (index, motion, tick)
                    assert state == result
                    difference = float(np.max(np.abs(root_step-expected_root)))
                    assert difference < .005
                    maximum_root = max(maximum_root, difference)
                    fixture.compare_pose(pose, ('original ambush tool', index, motion, tick))
                    observed = np.array(fixture.fields('12f'), dtype=np.float32).reshape(3, 4)
                    hand = pose[16 if left else 10].reshape(3, 4)
                    expected = np.empty((3, 4), dtype=np.float32)
                    expected[:, :3] = hand[:, :3] @ local[:, :3]
                    expected[:, 3] = hand[:, :3] @ local[:, 3] + hand[:, 3]
                    difference = float(np.max(np.abs(observed-expected)))
                    assert difference < .005, (index, motion, tick, difference)
                    maximum_attachment = max(maximum_attachment, difference)
                    free_values.add(free)
                assert any(v & 1 for v in free_values) and any(v & 0x20 for v in free_values)
            assert fixture.cursor == len(fixture.data)
    enemy_reference.SOURCES = old_sources
    print(f'M36 seven source families/tool MotionMove PASS: profiles={len(profiles)}, '
          f'ticks={ticks_total}, floats={ticks_total*(34*12+12+4)}, '
          f'maxRoot={maximum_root}, maxAttachment={maximum_attachment}')


if __name__ == '__main__':
    main()
