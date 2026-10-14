#!/usr/bin/env python3
"""Independent original-source M21 EM12 Dm_Blow comparison.

Usage: test_enemy_blow_data.py enemy_blow_tool EM12.drs
       test_enemy_blow_data.py --self-test
Requires numpy, g++, make and Linux x86-64 for the private original-code host.
The original tree stays read-only; reference builds use a temporary copy.
Original game assets are supplied locally and are never distributed.
"""
from pathlib import Path
import ctypes as C
import hashlib
import subprocess
import sys
import tempfile
from types import SimpleNamespace

import numpy as np

import test_action_poses as action_reference
import test_enemy_data as enemy_reference
import test_enemy_grab_data as grab_reference


BLOW_SOURCES = [(59, 60, 3, 1), (63, 64, 3, 1),
                (71, 72, 3, 1), (73, 74, 3, 1),
                (55, 56, 3, 1), (61, 62, 3, 1)]
SOURCES = grab_reference.SOURCES + [(654, 655, 0, 1)] + BLOW_SOURCES + [
    (107, 108, 15, 1),  # em10_R1_DownWake: face down, ARC6f/70.
    (42, 43, 6, 1),    # em10_R1_Dm_Head: ARC2e/2f.
    (44, 45, 6, 1),    # em10_R1_Dm_Head: ARC30/31.
    (75, 0, 5, 1),     # em10_R1_Dm_Down: face up, ARC4f.
    (76, 0, 5, 1),     # em10_R1_Dm_Down: face down, ARC50.
    (103, 0, 6, 1),    # em10_R1_Die_Down: face up, ARC6b.
    (104, 0, 6, 1),    # em10_R1_Die_Down: face down, ARC6c.
    (36, 37, 6, 1),    # Dm_Small: front alternate, ARC28/29.
    (65, 66, 6, 1),    # Dm_Small: rear torso, ARC45/46.
]
POSE_FRACTIONS = [0, .125, .25, .5, .875, 1]
SAMPLE_CLIPS = list(range(11, 18)) + list(range(20, 26))
TRACE_GROUPS = [(clip, mirrored) for clip in range(11, 18)
                for mirrored in [False, True]] + [
    (20, False), (20, True), (21, False), (21, True),
    (22, False), (23, False),
    (24, False), (24, True), (25, False), (25, True),
]
HOLD_CLIPS = list(range(11, 17)) + [22, 23]

BLOW_API = r"""
// Private M21 driver: the production port is not linked. Mode zero executes
// the Dm_Blow source's first GetSpeed/AddSpeed and MotionMove's automatic
// second GetSpeed/AddSpeed. Modes one/two provide independent, source-based
// comparisons with respectively two/one manual extractions and no root add.
API void mot_enemy_blow_reset(HostEnemy* e) {
    cEm* m=&e->h->model;
    m->pos=vecZero;
    m->ang=vecZero;
    PSMTXIdentity(m->mat);
    // All loaded clips are initialized once by EnemyReference's constructor.
    // Subsequent reset matches AnimationRig::reset: extra MotionSetCore calls
    // would change persistent IK options before the idle snapshot is captured.
    MotionClear(m,0);
    m->Motion.Seq_speed=1.f;
    mot_enemy_set(e,0,0,4,0);
    pG->Speed=0.f;
    MotionMove(m,0);
}
API u32 mot_enemy_blow_tick(HostEnemy* e,float playback,float scale,
                           int boost,int mode,float* values) {
    cEm* m=&e->h->model;
    MOTION_INFO* w=&m->Motion;
    // The fixture exposes model-local deltas and model-local joint matrices.
    // Actor placement is independently owned by SAT/encounter integration.
    m->pos=vecZero;
    m->ang=vecZero;
    PSMTXIdentity(m->mat);
    w->Seq_speed=playback;
    pG->Speed=1.f;
    Vec first,rotation,second=vecZero,rotation2=vecZero;
    MotionGetSpeed(m,w,0,&first,&rotation);
    values[4]=first.x; values[5]=first.y; values[6]=first.z;
    values[7]=rotation.y;
    Vec scaled=first;
    PSVECScale(&scaled,&scaled,scale);
    u32 result;
    u16 rootFlag=w->Mot_attr&1;
    if(mode==0) {
        if(!rootFlag) abort();
        MotionAddSpeed(m,w,&scaled,&rotation);
        // The original MotionMove performs the second extraction itself.
        result=MotionMove(m,0);
        values[0]=m->pos.x; values[1]=m->pos.y; values[2]=m->pos.z;
        values[3]=m->ang.y;
        values[8]=m->pos.x-scaled.x;
        values[9]=m->pos.y-scaled.y;
        values[10]=m->pos.z-scaled.z;
        values[11]=LIMIT_ANGLE(m->ang.y-rotation.y);
    } else {
        if(mode==1) MotionGetSpeed(m,w,0,&second,&rotation2);
        values[0]=scaled.x+second.x;
        values[1]=scaled.y+second.y;
        values[2]=scaled.z+second.z;
        values[3]=rotation.y+rotation2.y;
        values[8]=second.x; values[9]=second.y; values[10]=second.z;
        values[11]=rotation2.y;
        w->Mot_attr&=~1;
        result=MotionMove(m,0);
        w->Mot_attr|=rootFlag;
    }
    // em10_R1_Dm_Blow: post-MotionMove Timer boost, not an authored FCV edit.
    // cEm10::move follows it with partsWorldCalc, before partsFixAdjust.
    if(boost && !result && !(w->Seq_old.Free&0x80))
        m->getPartsPtr(0)->l_mat[1][3]*=1.5f;
    if(mode!=3) m->partsWorldCalc();
    // Remove the source mode's actor root placement for a raw local fixture.
    if(mode==0) {
        Mtx inverse,local;
        if(!PSMTXInverse(m->mat,inverse)) abort();
        for(cParts* p=m->pList;p;p=p->pList) {
            PSMTXConcat(inverse,p->mat,local);
            PSMTXCopy(local,p->mat);
        }
    }
    return result;
}
API void mot_enemy_blow_history(HostEnemy* e,float* values) {
    MOTION_INFO* w=&e->h->model.Motion;
    memcpy(values,&w->Pos_move_old,sizeof(Vec));
    memcpy(values+3,&w->Pos,sizeof(Vec));
    memcpy(values+6,&w->Ang,sizeof(Vec));
}
"""


def build_reference(root, host):
    original = enemy_reference.ENEMY_API
    enemy_reference.ENEMY_API = original + BLOW_API
    try:
        lib = grab_reference.build_reference(root, host)
    finally:
        enemy_reference.ENEMY_API = original
    lib.mot_enemy_blow_reset.argtypes = [enemy_reference.P]
    lib.mot_enemy_blow_tick.argtypes = [enemy_reference.P, C.c_float, C.c_float,
                                      C.c_int, C.c_int, enemy_reference.F32P]
    lib.mot_enemy_blow_tick.restype = C.c_uint32
    lib.mot_enemy_blow_history.argtypes = [enemy_reference.P, enemy_reference.F32P]
    return lib


class BlowReference(enemy_reference.EnemyReference):
    def start(self, clip, mirrored=False, flags=1, hokan=3):
        self.lib.mot_enemy_blow_reset(self.e)
        self.lib.mot_enemy_set(self.e, clip, hokan,
                              flags | (0x40 if mirrored else 0), 0)
        self.check_errors('blow start')

    def tick(self, playback=1, scale=1, boost=False, mode=0):
        values = (C.c_float * 12)()
        result = self.lib.mot_enemy_blow_tick(
            self.e, playback, scale, boost, mode, values)
        self.check_errors('blow tick')
        return np.array(values, dtype=np.float32), self.state(), result, self.pose()

    def history(self):
        values = (C.c_float * 9)()
        self.lib.mot_enemy_blow_history(self.e, values)
        return np.array(values, dtype=np.float32)


def self_test(lib, fcv):
    """Prove the source order without pretending synthetic keys are EM12 assets."""
    parts = [SimpleNamespace(parent=-1 if i == 0 else 0,
                             pos=(0, 100, 0) if i == 0 else (i * 3, 30, 0))
             for i in range(34)]
    body = SimpleNamespace(n_parts=34, parts=parts, blend_table=[])

    def axis(start, end):
        return fcv.Axis([0, 8], [(start, 0., 0.), (end, 0., 0.)])

    def motion(end):
        joints = [fcv.Joint(1, 0, 0, 0, [axis(0., end[0]), axis(0., end[1]),
                                        axis(0., end[2])]),
                  fcv.Joint(4, 0, 0, 0, [axis(0., 0.), axis(100., 160.),
                                        axis(0., 0.)]),
                  fcv.Joint(0x40, 0, 0, 0, [axis(0., 0.),
                       axis(0., .4 if any(end) else 0.), axis(0., 0.)])]
        return fcv.Motion(8, joints, [0, 1, 2])

    motions = [motion((0., 0., 0.)), motion((80., 16., -160.))]
    sequence = fcv.Sequence(0, [fcv.SeqKey(i * 64, 0, 0x80 if i >= 4 else 0)
                               for i in range(9)])
    sequences = [None, sequence]
    maxima = {'pose': 0., 'root': 0., 'second': 0., 'history': 0.}
    saw_boost_end = False
    for flags in [1, 0x401]:
        for mirror in [False, True]:
            for scale in [1., 2.]:
                with BlowReference(lib, body, motions, sequences, fcv) as exact, \
                     BlowReference(lib, body, motions, sequences, fcv) as manual, \
                     BlowReference(lib, body, motions, sequences, fcv) as single, \
                     BlowReference(lib, body, motions, sequences, fcv) as unboosted:
                    for reference in [exact, manual, single, unboosted]:
                        reference.start(1, mirror, flags)
                    for tick in range(15):
                        total, state, result, pose = exact.tick(scale=scale, boost=True)
                        other, other_state, other_result, other_pose = manual.tick(
                            scale=scale, boost=True, mode=1)
                        one, one_state, _, _ = single.tick(scale=scale, boost=True, mode=2)
                        _, plain_state, _, plain_pose = unboosted.tick(scale=scale)
                        assert state == other_state == one_state == plain_state
                        assert result == other_result
                        root_delta = float(np.max(np.abs(total - other)))
                        pose_delta = float(np.max(np.abs(pose - other_pose)))
                        assert root_delta < .0001 and pose_delta < .0001, (
                            'Two manual extractions must equal original Get/Add/Move',
                            flags, mirror, scale, tick, root_delta, pose_delta)
                        assert np.max(np.abs(exact.history() - manual.history())) < .0001
                        maxima['root'] = max(maxima['root'], root_delta)
                        maxima['pose'] = max(maxima['pose'], pose_delta)
                        maxima['second'] = max(maxima['second'],
                                               float(np.max(np.abs(total[8:12]))))
                        maxima['history'] = max(maxima['history'],
                            float(np.max(np.abs(exact.history() - single.history()))))
                        if flags == 1:
                            assert np.max(np.abs(total[8:12])) < .0001, (
                                'Actual Dm_Blow flags do not enable Hokan speed blend', tick, total)
                            assert np.max(np.abs(total[:4] - one[:4])) < .0001
                        elif tick == 1:
                            assert np.max(np.abs(total[8:12])) > .1
                            assert np.max(np.abs(total[:4] - one[:4])) > .1
                        active = not result and not (int(state[3]) & 0x80)
                        if active:
                            assert abs(pose[0, 7] - plain_pose[0, 7] * 1.5) < .0001
                        else:
                            assert np.max(np.abs(pose - plain_pose)) < .0001
                            saw_boost_end = True
    assert saw_boost_end and maxima['second'] > .1 and maxima['history'] > .1
    return maxima


def compare_tick(fixture, reference, label, scale=1, boost=False, mode=0,
                 hokan=None):
    playback = fixture.fields('f')[0]
    assert playback == 1, (label, 'playback', playback)
    observed_root = np.array(fixture.fields('4f'), dtype=np.float32)
    observed_state = fixture.fields('ff2I')
    expected_root, state, result, pose = reference.tick(playback, scale, boost, mode)
    assert observed_state == tuple(state[:4]), (
        label, 'curve/SEQ/state/Free', observed_state, state)
    assert observed_state[2] == result, (label, 'MotionMove return', result)
    if hokan is not None:
        assert state[4] == hokan, (label, 'original Hokan', state[4])
    delta = float(np.max(np.abs(observed_root - expected_root[:4])))
    assert np.isfinite(observed_root).all() and delta < .005, (
        label, 'root XYZ/yaw', delta, observed_root, expected_root)
    fixture.compare_pose(pose, label)
    return delta, observed_root, observed_state, pose, expected_root[8:12]


def compare_fixture(fixture, lib, body, motions, sequences, fcv):
    assert fixture.fields('3I') == (0x4d323142, 34, 26)
    maximum_root = 0.
    events = {}
    normals = {}
    with BlowReference(lib, body, motions, sequences, fcv) as reference, \
         BlowReference(lib, body, motions, sequences, fcv) as placed:
        cycles = []
        for clip, source in enumerate(SOURCES):
            assert fixture.fields('4I') == source, ('Source', clip)
            assert fixture.fields('f')[0] == motions[clip].max_frame, (
                'FCV maximum', clip)
            frames = fixture.word()
            root_cycle = np.array(fixture.fields('4f'), dtype=np.float32)
            cycle = reference.cycle(clip)
            assert frames == cycle[0], ('Sequence length', clip)
            assert np.max(np.abs(root_cycle - cycle[1:])) < .005, (
                'Original cycle XYZ/yaw', clip, root_cycle, cycle)
            cycles.append(cycle)

        assert fixture.word() == 78
        for clip in SAMPLE_CLIPS:
            for fraction in POSE_FRACTIONS:
                observed_clip, frame = fixture.fields('If')
                assert observed_clip == clip and frame == motions[clip].max_frame * fraction
                fixture.compare_pose(reference.sample(clip, frame),
                                     ('EM12 blow FCV', clip, frame))

        assert fixture.word() == 24
        for clip, mirrored in TRACE_GROUPS:
            source = SOURCES[clip]
            frames = int(cycles[clip][0])
            ticks = frames + 16
            # Cycle metadata is initialized with loop flag4. A NoSEQ
            # one-shot instead includes the FCV's final frame (maxFrame+1
            # sequence entries); explicit SEQ length is unchanged by flag4.
            end_frame = frames - 1 if source[1] else motions[clip].max_frame
            boost = clip <= 16
            scale = 2 if clip == 15 and not mirrored else 1
            assert fixture.fields('6If') == (
                clip, mirrored, source[3] | (0x40 if mirrored else 0),
                source[2], ticks, boost, scale)
            reference.start(clip, mirrored, source[3], source[2])
            if boost:
                placed.start(clip, mirrored, source[3], source[2])
            seen_free = set()
            poses = []
            was_ended = False
            for tick in range(ticks):
                delta, root, state, pose, second = compare_tick(
                    fixture, reference, ('EM12 blow tick', clip, mirrored, tick),
                    scale, boost, 1 if boost else 2,
                    max(0, source[2] - tick - 1))
                maximum_root = max(maximum_root, delta)
                seen_free.add(state[3])
                poses.append(pose)
                if boost:
                    # Validate total placement using the actual original
                    # AddSpeed + automatic second Get/Add in MotionMove.
                    # Raw local matrices use the same zero-placement host
                    # convention as M19. Normalizing placed IK matrices
                    # afterwards can magnify a float ULP near full reach.
                    actual, placed_state, result, _ = placed.tick(
                        scale=scale, boost=True, mode=0)
                    placement_delta = float(np.max(np.abs(actual[:4] - root)))
                    assert placement_delta < .005 and tuple(placed_state[:4]) == state
                    assert result == state[2]
                    maximum_root = max(maximum_root, placement_delta)
                    assert np.max(np.abs(second)) < .005, (
                        'Actual Dm_Blow flags do not enable Hokan speed blend',
                        clip, mirrored, tick, second)
                if state[2] == 4:
                    assert state[1] == end_frame, ('Nonloop clamp', clip, state)
                if was_ended:
                    assert np.max(np.abs(root)) < .005, (
                        'Already ended root must stop', clip, mirrored, root)
                was_ended = state[2] == 4
            assert state[2] == 4 and state[1] == end_frame, (
                'Nonloop motion must end', clip, mirrored, state)
            events[(clip, mirrored)] = seen_free
            if mirrored:
                assert np.max(np.abs(np.array(poses) - normals[clip])) > 1, (
                    'Original mirrored blow/wake must change pose', clip)
            else:
                normals[clip] = np.array(poses)
        # Each blow retains its own authored end pose. DownIdle selects no FCV,
        # captures no new Hokan snapshot, and removes the kick-only Y stretch.
        assert fixture.word() == 8
        for clip in HOLD_CLIPS:
            mirrored = False
            assert fixture.fields('3I') == (clip, mirrored, 10)
            source = SOURCES[clip]
            scale = 2 if clip == 15 else 1
            boost = clip <= 16
            reference.start(clip, mirrored, source[3], source[2])
            for _ in range(int(cycles[clip][0]) + 16):
                _, state, result, pose = reference.tick(
                    scale=scale, boost=boost, mode=1 if boost else 2)
            assert state[2] == result == 4
            held_pose = pose.copy()
            held_state = tuple(state[:4])
            for tick in range(10):
                delta, root, state, pose, _ = compare_tick(
                    fixture, reference, ('EM12 ended blow/DownIdle', clip, tick),
                    mode=2, hokan=0)
                maximum_root = max(maximum_root, delta)
                assert np.max(np.abs(root)) < .005 and state == held_state
                assert np.max(np.abs(pose - held_pose)) < .005, (
                    'Original ended blow pose must hold', clip, tick)
    assert fixture.cursor == len(fixture.data), (fixture.cursor, len(fixture.data))
    return maximum_root, events


def main():
    root = Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(root / 'upstream/re4/tools'))
    from motion import fcv, modelbin
    standalone = sys.argv[1:] == ['--self-test']
    if not standalone and len(sys.argv) != 3:
        raise SystemExit(__doc__)
    if not standalone:
        import drs
        tool, path = [Path(p).resolve() for p in sys.argv[1:]]
        image = path.read_bytes()
        assert hashlib.sha256(image).hexdigest() == enemy_reference.EM12_SHA, path
        archive = drs.Drs(image)
        body = modelbin.parse(archive.entries[440][1])
        body.check_tree()
        assert body.n_parts == 34
        motions = [fcv.parse(archive.entries[entry][1]) for entry, *_ in SOURCES]
        sequences = [fcv.parse_seq(archive.entries[entry][1]) if entry else None
                     for _, entry, _, _ in SOURCES]
        fixture = action_reference.Fixture(subprocess.check_output([str(tool), str(path)]))
    with tempfile.TemporaryDirectory(prefix='re4-enemy-blow-reference-') as temp:
        lib = build_reference(root, Path(temp) / 'host')
        proof = self_test(lib, fcv)
        if not standalone:
            maximum_root, events = compare_fixture(
                fixture, lib, body, motions, sequences, fcv)
    print('Original Dm_Blow Get/Add/Move order and Free80/Y-boost synthetic proof PASS:', proof)
    if not standalone:
        print(f'Original EM12 blow MotionMove/IK/Hokan/mirror reference PASS: '
              f'{fixture.count} poses, 34 joints, max difference {fixture.maximum:.8f}')
        print(f'Original EM12 blow root/SEQ/Free reference PASS: 24 traces, '
              f'max root difference {maximum_root:.8f}; event sets {events}')
        print('Original EM12 eight ended blow/down-death/DownIdle poses PASS')


if __name__ == '__main__':
    main()
