#!/usr/bin/env python3
"""Independent original-source M21 knife/kick FCV and cMot3 comparison.

Usage: test_melee_poses.py melee_pose_tool PL00.drs WEP01.drs EM12.drs
Requires numpy, g++, make and the private Linux x86-64 original-code host.
Uses the source-extracted M18 cMot3/MotionMove/IK/Hokan/waist oracle. The
upstream tree stays read-only; reference builds use a temporary directory.
Original game data is supplied locally and is never copied into a deliverable.
"""
from pathlib import Path
import hashlib
import ctypes as C
import subprocess
import sys
import tempfile
import numpy as np

import test_action_poses as reference


FRACTIONS = [0, .125, .25, .5, .875, 1]
TRIPLETS = [[21, 22, 23], [24, 25, 26]]
SINGLE_API = r"""
// Private reference entrypoint: the kick uses MotionSetCore directly,
// without cMot3's secondary IK or a zero-weight duplicate motion.
struct HostMeleeSingle { HostModel* h; u8* image; size_t size; };
API void mot_melee_clear(HostModel* h) { MotionClear(&h->model,0); }
API HostMeleeSingle* mot_melee_single_create(HostModel* h,const u8* bytes,size_t size,int hokan,int flags) {
    HostMeleeSingle* s=new HostMeleeSingle{h,0,size};
    static uintptr_t next=0xb0000000u;
    uintptr_t address=next; next+=(size+4095)&~uintptr_t(4095);
    s->image=(u8*)mmap((void*)address,size,PROT_READ|PROT_WRITE,
       MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    if(s->image==MAP_FAILED) abort();
    memcpy(s->image,bytes,size);
    MotionSetCore(&h->model,&h->model.Motion,s->image,0,hokan,flags,0);
    return s;
}
API u16 mot_melee_single_tick(HostMeleeSingle* s) {
    return MotionMove(&s->h->model,0);
}
API void mot_melee_single_destroy(HostMeleeSingle* s) {
    munmap(s->image,s->size); delete s;
}
API void mot_melee_single_state(HostMeleeSingle* s,float* values) {
    auto& w=s->h->model.Motion;
    values[0]=w.Seq_frame;values[2]=w.Mot_frame;
}
API int mot_melee_single_cross(HostMeleeSingle* s,float frame) {
    return MotionCheckCrossFrame(&s->h->model.Motion,frame);
}
"""


class SingleReference:
    def __init__(self, lib, model, idle, motion, fcv, blend, flags):
        self.lib = lib
        lib.mot_host_clear_errors()
        self.n = model.n_parts
        parents = (C.c_int * self.n)(*(part.parent for part in model.parts))
        rest = (C.c_float * (self.n * 3))(*[v for part in model.parts for v in part.pos])
        self.h = lib.mot_model_create(self.n, parents, rest)
        self.table = None
        if model.blend_table:
            words = [len(model.blend_table), 0] + [v for row in model.blend_table for v in row]
            self.table = (C.c_uint16 * len(words))(*words)
            lib.mot_model_blend_table(self.h, self.table)
        lib.mot_melee_clear(self.h)
        data = fcv.serialise(idle, '<')
        image = (C.c_uint8 * len(data)).from_buffer_copy(data)
        lib.mot_blend_clock(0)
        lib.mot_model_set(self.h, image, len(image), 4, 1)
        lib.mot_model_frame(self.h, 0)
        data = fcv.serialise(motion, '<')
        image = (C.c_uint8 * len(data)).from_buffer_copy(data)
        self.b = lib.mot_melee_single_create(self.h, image, len(image), blend, flags)
        self.matrix = (C.c_float * (self.n * 12))()

    def state(self):
        values = (C.c_float * 8)()
        self.lib.mot_melee_single_state(self.b, values)
        return list(values)

    def read(self):
        self.lib.mot_model_read(self.h, None, None, None, None, self.matrix, None, None)
        return np.array(self.matrix, dtype=np.float32).reshape(self.n, 12)

    def tick(self):
        self.lib.mot_blend_clock(1)
        self.result = self.lib.mot_melee_single_tick(self.b)
        assert not self.lib.mot_host_errors()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.lib.mot_melee_single_destroy(self.b)
        self.lib.mot_model_destroy(self.h)


def compare_tick(fixture, ref, label, single=False):
    if single:
        curve, sequence, state, frame12, frame31, frame36 = fixture.fields('ff4I')
        original = ref.state()
        assert curve == original[2] and sequence == original[0] and state == ref.result
        expected = [ref.lib.mot_melee_single_cross(ref.b, frame) for frame in [12, 31, 36]]
        assert [frame12, frame31, frame36] == expected
    else:
        fixture.compare_state(ref)
    events = fixture.fields('4I')
    cross = ref.lib.mot_melee_single_cross if single else ref.lib.mot_blend_cross
    expected = tuple(cross(ref.b, frame)
                     for frame in [4, 6, 10, 17])
    assert events == expected, (label, 'source crossing events', events, expected)
    expected_pose = ref.read()
    observed = np.frombuffer(fixture.data, dtype='>f4', count=expected_pose.size,
                             offset=fixture.cursor).reshape(expected_pose.shape)
    maximum = np.unravel_index(np.argmax(np.abs(observed - expected_pose)), observed.shape)
    if np.abs(observed[maximum] - expected_pose[maximum]) >= .005:
        print('Pose mismatch diagnostic:', label, 'joint/component', maximum,
              'native/original', observed[maximum], expected_pose[maximum], file=sys.stderr)
    fixture.compare_pose(expected_pose, label)


def main():
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    root = Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(root / 'upstream/re4/tools'))
    import drs
    from motion import fcv, modelbin

    tool, pl, wp, em = [Path(path).resolve() for path in sys.argv[1:]]
    for path, digest in [(pl, '5ea520d7026c48e33e12256ef82173140e72cf4019758167a7d5aa5b6e356090'),
                         (wp, 'fb838948d4161ee04da306ff0910980ff1b9e3b5701b43983a36e1d21bca3689'),
                         (em, '6468f2d05d496b27d193ad79b3afeb20f40edcd0f54d3c2d16da7a3c03c16e7b')]:
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, path
    player, weapon, enemy = [drs.Drs(path.read_bytes()) for path in [pl, wp, em]]
    model = modelbin.parse(player.entries[0][1])
    idle = fcv.parse(weapon.entries[7][1])
    entries = [31, 125, 127, 129, 126, 128, 130, 131]
    motions = {20 + i: fcv.parse(player.entries[entry][1])
               for i, entry in enumerate(entries)}
    motions[28] = fcv.parse(weapon.entries[45][1])
    motions[29] = fcv.parse(enemy.entries[665][1])
    fixture = reference.Fixture(subprocess.check_output(
        [str(tool), str(pl), str(wp), str(em)]))
    assert fixture.fields('2I') == (0x4d323100, 119)
    with tempfile.TemporaryDirectory(prefix='re4-melee-reference-') as temporary:
        original_api = reference.BLEND_API
        reference.BLEND_API = original_api + SINGLE_API
        try:
            lib = reference.build_reference(root, Path(temporary) / 'host')
        finally:
            reference.BLEND_API = original_api
        lib.mot_melee_clear.argtypes = [C.c_void_p]
        lib.mot_melee_single_create.argtypes = [C.c_void_p, reference.U8P, C.c_size_t,
                                               C.c_int, C.c_int]
        lib.mot_melee_single_create.restype = C.c_void_p
        lib.mot_melee_single_tick.argtypes = [C.c_void_p]
        lib.mot_melee_single_tick.restype = C.c_uint16
        lib.mot_melee_single_destroy.argtypes = [C.c_void_p]
        lib.mot_melee_single_state.argtypes = [C.c_void_p, reference.F32P]
        lib.mot_melee_single_cross.argtypes = [C.c_void_p, C.c_float]
        lib.mot_melee_single_cross.restype = C.c_int
        assert fixture.word() == 60
        for clip in range(20, 30):
            motion = motions[clip]
            for fraction in FRACTIONS:
                index, frame = fixture.fields('If')
                assert index == clip and frame == motion.max_frame * fraction
                with reference.BlendReference(lib, model, [motion] * 3, idle, fcv) as ref:
                    fixture.compare_pose(ref.sample(frame, 0), ('FCV', clip, frame))
        for indices in TRIPLETS:
            triplet = [motions[index] for index in indices]
            frames = triplet[0].max_frame + 1
            assert fixture.word() == 5 * frames
            for rate in [-1, -.5, 0, .5, 1]:
                for frame in range(frames):
                    assert fixture.fields('ff') == (rate, frame)
                    with reference.BlendReference(lib, model, triplet, idle, fcv) as ref:
                        fixture.compare_pose(ref.sample(frame, rate),
                                             ('cMot3', indices, rate, frame))
        assert fixture.word() == 18
        for indices in TRIPLETS:
            triplet = [motions[index] for index in indices]
            for rate in [-1, 0, 1]:
                for waist in [-.35, 0, .35]:
                    index, observed_rate, observed_waist = fixture.fields('Iff')
                    assert index == indices[0] and observed_rate == rate
                    assert abs(observed_waist - waist) < 1e-7
                    with reference.BlendReference(lib, model, triplet, idle, fcv) as ref:
                        fixture.compare_pose(ref.sample(8, rate, waist),
                                             ('waist', indices, rate, waist))
        assert fixture.word() == 4
        for indices in TRIPLETS:
            triplet = [motions[index] for index in indices]
            for blend in [0, 3]:
                assert fixture.fields('3I') == (indices[0], blend, 9)
                rates = ([1, 1, 0, -1, -1, 0, .5, -.5, 1] if blend == 0 else
                         [-.5, -.5, 0, .5, .5, -.5, -.5, 0, .5])
                with reference.BlendReference(lib, model, triplet, idle, fcv,
                                              hokan=blend) as ref:
                    for tick, rate in enumerate(rates):
                        assert fixture.fields('f')[0] == rate
                        ref.tick(rate)
                        compare_tick(fixture, ref, ('Hokan/sign', indices, blend, tick))
        assert fixture.word() == 4
        for clip, blend, flags in [(20, 3, 4), (27, 5, 0), (28, 5, 0), (29, 6, 0)]:
            assert fixture.fields('2I') == (clip, 60)
            sequence = fcv.parse_seq(player.entries[32][1]) if clip == 20 else None
            # Actor placement is owned externally. Source kick flag1 is
            # suppressed identically to AnimationRig::startActionFlags.
            context = (SingleReference(lib, model, idle, motions[clip], fcv, blend, flags)
                       if clip == 29 else
                       reference.BlendReference(lib, model, [motions[clip]] * 3, idle,
                                                fcv, hokan=blend, flags=flags,
                                                sequence=sequence))
            with context as ref:
                for tick in range(60):
                    if clip == 29:
                        ref.tick()
                    else:
                        ref.tick(0)
                    compare_tick(fixture, ref, ('action/events', clip, tick), clip == 29)
    assert fixture.cursor == len(fixture.data), (fixture.cursor, len(fixture.data))
    print(f'Original knife/kick FCV/cMot3/IK/Hokan/waist reference PASS: '
          f'{fixture.count} poses, 119 joints, maximum error {fixture.maximum:.8f}')
    print('Original draw/sheath/knife-to-gun/kick state, SEQ and frame-crossing '
          'events PASS; original cinematic cameras are not installed')


if __name__ == '__main__':
    main()
