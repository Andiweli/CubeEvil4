#!/usr/bin/env python3
"""Compare first-house axe pose/root/SEQ and attachment to original MotionMove.

Usage: test_r100_enemy_data.py r100_enemy_tool EM12.drs
Original copyrighted assets remain local; this test builds read-only source
copies in a temporary directory using the existing motion reference host.
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


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    root = Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(root / 'upstream/re4/tools'))
    import drs
    from motion import fcv, modelbin
    tool, path = [Path(p).resolve() for p in sys.argv[1:]]
    data = path.read_bytes()
    assert hashlib.sha256(data).hexdigest() == enemy_reference.EM12_SHA
    archive = drs.Drs(data)
    body = modelbin.parse(archive.entries[440][1])
    sources = [(1, 0, 10, 5), (4, 5, 7, 5), (40, 41, 6, 1),
               (34, 35, 6, 1), (124, 125, 10, 1)]
    motions = [fcv.parse(archive.entries[entry][1]) for entry, *_ in sources]
    sequences = [fcv.parse_seq(archive.entries[entry][1]) if entry else None
                 for _, entry, _, _ in sources]
    fixture = action_reference.Fixture(subprocess.check_output([str(tool), str(path)]))
    assert fixture.fields('6I') == (0x4d333541, 34, 124, 125, 10, 0x41)
    old_sources = enemy_reference.SOURCES
    enemy_reference.SOURCES = sources
    maximum_root = maximum_attachment = 0.
    free_values = set()
    with tempfile.TemporaryDirectory(prefix='re4-r100-axe-source-') as temp:
        lib = enemy_reference.build_reference(root, Path(temp) / 'host')
        with enemy_reference.EnemyReference(lib, body, motions, sequences, fcv) as reference:
            observed_frames, maximum = fixture.fields('If')
            observed_cycle = np.array(fixture.fields('4f'), dtype=np.float32)
            expected_cycle = reference.cycle(4)
            assert observed_frames == expected_cycle[0]
            assert maximum == motions[4].max_frame
            assert np.max(np.abs(observed_cycle - expected_cycle[1:])) < .005
            ticks = fixture.word()
            assert ticks == observed_frames + 16
            reference.start(4, True)
            # Source em10WeaponSet type2 + flag0x01000000:
            # local offset313.85,-21.2,102.21; rotationY−1.0402162,Zpi;
            # cObj parent is original left hand part0x10.
            y, z = np.float32(-1.0402162), np.float32(3.1415927)
            cy, sy, cz, sz = np.cos(y), np.sin(y), np.cos(z), np.sin(z)
            local = np.array([[cz*cy, -sz, cz*sy, 313.85],
                              [sz*cy, cz, sz*sy, -21.2],
                              [-sy, 0, cy, 102.21]], dtype=np.float32)
            for tick in range(ticks):
                root_step = np.array(fixture.fields('4f'), dtype=np.float32)
                curve, sequence, state, free = fixture.fields('ff2I')
                expected_root, expected_state, result, pose = reference.tick(1)
                assert (curve, sequence, state, free) == tuple(expected_state[:4])
                assert state == result
                assert expected_state[4] == max(0, 10-tick-1)
                difference = float(np.max(np.abs(root_step-expected_root)))
                assert difference < .005
                maximum_root = max(maximum_root, difference)
                fixture.compare_pose(pose, ('original r100 mirrored axe', tick))
                attachment = np.array(fixture.fields('12f'), dtype=np.float32).reshape(3, 4)
                hand = pose[16].reshape(3, 4)
                expected = np.empty((3, 4), dtype=np.float32)
                expected[:, :3] = hand[:, :3] @ local[:, :3]
                expected[:, 3] = hand[:, :3] @ local[:, 3] + hand[:, 3]
                difference = float(np.max(np.abs(attachment-expected)))
                assert difference < .005, ('source hand attachment', tick, difference)
                maximum_attachment = max(maximum_attachment, difference)
                free_values.add(free)
    enemy_reference.SOURCES = old_sources
    assert fixture.cursor == len(fixture.data)
    assert any(v & 1 for v in free_values), free_values
    assert any(v & 0x20 for v in free_values), free_values
    print(f'Original r100 mirrored axe PASS: {fixture.count} complete poses; '
          f'max pose {fixture.maximum:.8f}, root {maximum_root:.8f}, '
          f'attachment {maximum_attachment:.8f}; SEQ Free {sorted(free_values)}')


if __name__ == '__main__':
    main()
