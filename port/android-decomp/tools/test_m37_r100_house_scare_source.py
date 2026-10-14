#!/usr/bin/env python3
"""Original MotionMove: transient house TurnWalk, turn and both source walk styles.
Usage: test_m37_r100_house_scare_source.py TOOL EM12 ESL
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
    sys.path.insert(0, str(root/'upstream/re4/tools'))
    import drs
    from motion import fcv, modelbin
    tool, enemy, esl = [Path(p).resolve() for p in sys.argv[1:]]
    data = enemy.read_bytes()
    assert hashlib.sha256(data).hexdigest() == enemy_reference.EM12_SHA
    archive = drs.Drs(data)
    body = modelbin.parse(archive.entries[440][1])
    old_sources = enemy_reference.SOURCES
    total = 0
    maximum = 0.
    with tempfile.TemporaryDirectory(prefix='re4-r100-house-source-') as temp:
        lib = enemy_reference.build_reference(root, Path(temp)/'host')
        for walk in [0, 1]:
            walksource = (10, 13, 7, 5) if walk else (4, 7, 7, 5)
            sources = [(1, 0, 0, 5), (20, 21, 10, 1), walksource, (1, 0, 0, 5), walksource]
            enemy_reference.SOURCES = sources
            motions = [fcv.parse(archive.entries[entry][1]) for entry, *_ in sources]
            sequences = [fcv.parse_seq(archive.entries[entry][1]) if entry else None for _, entry, *_ in sources]
            for choice in [0, 1]:
                selected = sources[choice+1]
                fixture = action_reference.Fixture(subprocess.check_output([str(tool), str(enemy), str(esl), str(walk), str(choice)]))
                assert fixture.fields('7I') == (0x4d333748, walk, choice, *selected)
                ticks = fixture.word()
                with enemy_reference.EnemyReference(lib, body, motions, sequences, fcv) as reference:
                    reference.start(choice+1)
                    for tick in range(ticks):
                        observed_root = np.array(fixture.fields('4f'), dtype=np.float32)
                        state = fixture.fields('ff2I')
                        expected_root, expected_state, result, pose = reference.tick(1)
                        assert state == tuple(expected_state[:4]) and result == state[2], (walk, choice, tick)
                        difference = float(np.max(np.abs(observed_root-expected_root)))
                        assert difference < .005, (walk, choice, tick, difference)
                        maximum = max(maximum, difference)
                        fixture.compare_pose(pose, ('original house scare', walk, choice, tick))
                    assert fixture.cursor == len(fixture.data)
                    total += ticks
    enemy_reference.SOURCES = old_sources
    print(f'M37 original house TurnWalk MotionMove PASS: branches=4 ticks={total} '
          f'floats={total*(34*12+4)} maxRoot={maximum}')


if __name__ == '__main__':
    main()
