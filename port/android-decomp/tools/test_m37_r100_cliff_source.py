#!/usr/bin/env python3
"""Original MotionSetCore/Move oracle: EM12 cliff types1/3/0 + room ARC2e/2f/30.
Usage: test_m37_r100_cliff_source.py TOOL EM12 ESL R100
The frozen source builds privately; assets remain local and are not distributed.
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
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    root = Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(root / 'upstream/re4/tools'))
    import drs
    from motion import fcv, modelbin
    tool, enemy, esl, room = [Path(p).resolve() for p in sys.argv[1:]]
    data = enemy.read_bytes()
    assert hashlib.sha256(data).hexdigest() == enemy_reference.EM12_SHA
    archive = drs.Drs(data)
    roomdata = room.read_bytes()
    # DAT directory records contain offsets, independent from production ARC.
    count = struct.unpack_from('>I', roomdata)[0]
    offsets = struct.unpack_from(f'>{count}I', roomdata, 16)
    old_sources = enemy_reference.SOURCES
    total = 0
    maximum_root = 0.
    with tempfile.TemporaryDirectory(prefix='re4-r100-cliff-source-') as temp:
        lib = enemy_reference.build_reference(root, Path(temp) / 'host')
        for index, model, maximum in [(6, 1, 190), (7, 3, 255), (8, 0, 180)]:
            entry = index + 36
            motion = fcv.parse(roomdata[offsets[entry]:offsets[entry+1]])
            assert motion.max_frame == maximum
            body = modelbin.parse(archive.entries[{0: 440, 1: 468, 3: 472}[model]][1])
            enemy_reference.SOURCES = [(entry, 0, 0, 5)] * 5
            fixture = action_reference.Fixture(subprocess.check_output(
                [str(tool), str(enemy), str(esl), str(room), str(index)]))
            assert fixture.fields('6I') == (0x4d333743, index, model, entry, 34, 5)
            assert fixture.fields('f')[0] == maximum
            ticks = fixture.word()
            assert ticks == maximum + 16
            with enemy_reference.EnemyReference(lib, body, [motion]*5, [None]*5, fcv) as reference:
                reference.start(0)
                for tick in range(ticks):
                    observed_root = np.array(fixture.fields('4f'), dtype=np.float32)
                    state = fixture.fields('ff2I')
                    expected_root, expected_state, result, pose = reference.tick(1)
                    assert state == tuple(expected_state[:4]), (index, tick, state, expected_state)
                    assert result == state[2]
                    difference = float(np.max(np.abs(observed_root-expected_root)))
                    assert difference < .005, (index, tick, difference)
                    maximum_root = max(maximum_root, difference)
                    fixture.compare_pose(pose, ('original cliff', index, tick))
                assert fixture.cursor == len(fixture.data)
                total += ticks
    enemy_reference.SOURCES = old_sources
    print(f'M37 original cliff MotionMove PASS: actors=3 ticks={total} '
          f'floats={total*(34*12+4)} maxRoot={maximum_root}')


if __name__ == '__main__':
    main()
