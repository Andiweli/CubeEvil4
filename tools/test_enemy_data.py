#!/usr/bin/env python3
"""Independent original-source M19 EM12 pose, root, SEQ and mirror comparison.

Usage: test_enemy_data.py enemy_tool EM12.drs
Requires numpy, g++, make and Linux x86-64 for the private original-code host.
The original tree stays read-only; the copied helper builds inside a tempdir.
Original game assets are supplied locally and are never distributed.
"""
from pathlib import Path
import ctypes as C
import hashlib
import struct
import subprocess
import sys
import tempfile

import numpy as np

import test_action_poses as action_reference


P = C.c_void_p
U8P = C.POINTER(C.c_uint8)
F32P = C.POINTER(C.c_float)
EM12_SHA = '6468f2d05d496b27d193ad79b3afeb20f40edcd0f54d3c2d16da7a3c03c16e7b'
# R100's native ESL record index is 37 (0x25), selecting the third walking
# sequence in em10SetWalkMotion. The record's setNo is a different field.
SOURCES = [(1, 0, 10, 5), (4, 7, 7, 5), (40, 41, 6, 1),
           (34, 35, 6, 1), (99, 100, 15, 1)]
FLIP = [0, 1, 2, 3, 4, 11, 12, 13, 14, 15, 16, 5, 6, 7, 8, 9, 10,
        17, 22, 23, 24, 25, 18, 19, 20, 21, 26, 27, 28, 29, 30, 31, 32, 33]


ENEMY_API = r"""
// Private M19 driver. MotionSetCore/GetSpeed/Move and all pose/IK/matrix
// functions below are the separately compiled, unmodified original sources.
struct HostEnemy {
    HostModel* h;
    u8* image[5];
    size_t size[5];
    u8* sequence[5];
    u16 flip[34];
};
API HostEnemy* mot_enemy_create(HostModel* h,const u16* flip) {
    HostEnemy* e=new HostEnemy{};
    e->h=h;
    memcpy(e->flip,flip,sizeof(e->flip));
    h->model.pXFlip=e->flip;
    return e;
}
API void mot_enemy_load(HostEnemy* e,int clip,const u8* image,size_t size,
                        const u8* sequence,size_t seqsize) {
    // Preserve the original pointer-sign relocation check on repeated sets.
    // MAP_32BIT alone returns positive signed pointers and would relocate a
    // reused FCV twice. MAP_FIXED_NOREPLACE never overwrites another mapping.
    static uintptr_t nextAddress=0xa0000000u;
    uintptr_t address=nextAddress;
    nextAddress+=(size+4095)&~uintptr_t(4095);
    e->image[clip]=(u8*)mmap((void*)address,size,PROT_READ|PROT_WRITE,
                  MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    if(e->image[clip]==MAP_FAILED) abort();
    e->size[clip]=size;
    memcpy(e->image[clip],image,size);
    if(seqsize) {
        e->sequence[clip]=(u8*)malloc(seqsize);
        memcpy(e->sequence[clip],sequence,seqsize);
    }
}
API void mot_enemy_destroy(HostEnemy* e) {
    e->h->model.pXFlip=0;
    for(int i=0;i<5;++i) {
        if(e->image[i]) munmap(e->image[i],e->size[i]);
        free(e->sequence[i]);
    }
    delete e;
}
API void mot_enemy_set(HostEnemy* e,int clip,int hokan,int flags,int frame) {
    cEm* m=&e->h->model;
    MotionSetCore(m,&m->Motion,e->image[clip],e->sequence[clip],hokan,flags,frame);
}
API void mot_enemy_reset(HostEnemy* e) {
    cEm* m=&e->h->model;
    // Mirror AnimationRig's constructor initialization and reset, including
    // its original idle frame zero before the next Hokan snapshot is taken.
    for(int i=0;i<5;++i) mot_enemy_set(e,i,0,4,0);
    MotionClear(m,0);
    m->Motion.Seq_speed=1.f;
    mot_enemy_set(e,0,0,4,0);
    pG->Speed=0.f;
    MotionMove(m,0);
}
API void mot_enemy_cycle(HostEnemy* e,int clip,float* values) {
    mot_enemy_set(e,clip,0,4,0);
    MOTION_INFO* w=&e->h->model.Motion;
    values[0]=(float)w->Seq_frame_num;
    values[1]=w->Pos_dist.x;
    values[2]=w->Pos_dist.y;
    values[3]=w->Pos_dist.z;
    values[4]=w->Ang_dist.y;
}
API u32 mot_enemy_sample(HostEnemy* e,int clip,float frame) {
    cEm* m=&e->h->model;
    MotionSetCore(m,&m->Motion,e->image[clip],0,0,4,0);
    m->Motion.Mot_attr|=0x8000;
    m->Motion.Seq_frame=frame;
    pG->Speed=0.f;
    return MotionMove(m,0);
}
API u32 mot_enemy_tick(HostEnemy* e,float playback,float* root) {
    cEm* m=&e->h->model;
    Vec position,rotation;
    m->Motion.Seq_speed=playback;
    pG->Speed=1.f;
    MotionGetSpeed(m,&m->Motion,0,&position,&rotation);
    root[0]=position.x; root[1]=position.y; root[2]=position.z;
    root[3]=rotation.y;
    // The caller accepts root displacement through scene collision. Keeping
    // the hosted model at zero makes these raw parts matrices model-local;
    // suppress only MotionMove's second extraction/application of that root.
    u16 rootFlag=m->Motion.Mot_attr&1;
    m->Motion.Mot_attr&=~1;
    u32 result=MotionMove(m,0);
    m->Motion.Mot_attr|=rootFlag;
    return result;
}
API void mot_enemy_state(HostEnemy* e,float* values) {
    MOTION_INFO* w=&e->h->model.Motion;
    values[0]=w->Mot_frame;
    values[1]=w->Seq_frame;
    values[2]=(float)w->Mot_state;
    values[3]=(float)w->Seq_old.Free;
    values[4]=(float)w->Hokan_cnt;
}
"""


def build_reference(root, host):
    original_api = action_reference.BLEND_API
    action_reference.BLEND_API = original_api + ENEMY_API
    try:
        lib = action_reference.build_reference(root, host)
    finally:
        action_reference.BLEND_API = original_api
    lib.mot_enemy_create.argtypes = [P, C.POINTER(C.c_uint16)]
    lib.mot_enemy_create.restype = P
    lib.mot_enemy_load.argtypes = [P, C.c_int, U8P, C.c_size_t, U8P, C.c_size_t]
    lib.mot_enemy_destroy.argtypes = [P]
    lib.mot_enemy_set.argtypes = [P, C.c_int, C.c_int, C.c_int, C.c_int]
    lib.mot_enemy_reset.argtypes = [P]
    lib.mot_enemy_cycle.argtypes = [P, C.c_int, F32P]
    lib.mot_enemy_sample.argtypes = [P, C.c_int, C.c_float]
    lib.mot_enemy_sample.restype = C.c_uint32
    lib.mot_enemy_tick.argtypes = [P, C.c_float, F32P]
    lib.mot_enemy_tick.restype = C.c_uint32
    lib.mot_enemy_state.argtypes = [P, F32P]
    return lib


class EnemyReference:
    def __init__(self, lib, model, motions, sequences, fcv):
        self.lib = lib
        lib.mot_host_clear_errors()
        self.n = model.n_parts
        assert self.n == len(FLIP) == 34
        parents = (C.c_int * self.n)(*(part.parent for part in model.parts))
        rest = (C.c_float * (self.n * 3))(*[v for part in model.parts for v in part.pos])
        self.h = lib.mot_model_create(self.n, parents, rest)
        self.blend = None
        if model.blend_table:
            words = [len(model.blend_table), 0] + [v for row in model.blend_table for v in row]
            self.blend = (C.c_uint16 * len(words))(*words)
            lib.mot_model_blend_table(self.h, self.blend)
        flip = (C.c_uint16 * self.n)(*FLIP)
        self.e = lib.mot_enemy_create(self.h, flip)
        for clip, motion in enumerate(motions):
            image = fcv.serialise(motion, '<')
            buf = (C.c_uint8 * len(image)).from_buffer_copy(image)
            sequence = None
            if sequences[clip] is not None:
                image = fcv.serialise_seq(sequences[clip], '<')
                sequence = (C.c_uint8 * len(image)).from_buffer_copy(image)
            lib.mot_enemy_load(self.e, clip, buf, len(buf), sequence,
                               0 if sequence is None else len(sequence))
        self.matrix = (C.c_float * (self.n * 12))()
        lib.mot_enemy_reset(self.e)
        self.check_errors('create/reset')

    def check_errors(self, operation):
        assert not self.lib.mot_host_errors(), ('Original host logged an error', operation)

    def pose(self):
        self.lib.mot_model_read(self.h, None, None, None, None, self.matrix, None, None)
        values = np.array(self.matrix, dtype=np.float32).reshape(self.n, 12)
        assert np.isfinite(values).all()
        return values

    def cycle(self, clip):
        values = (C.c_float * 5)()
        self.lib.mot_enemy_cycle(self.e, clip, values)
        self.check_errors('cycle')
        return np.array(values, dtype=np.float32)

    def sample(self, clip, frame):
        self.lib.mot_enemy_reset(self.e)
        self.lib.mot_enemy_sample(self.e, clip, frame)
        self.check_errors('sample')
        return self.pose()

    def start(self, clip, mirrored=False):
        self.lib.mot_enemy_reset(self.e)
        hokan, flags = SOURCES[clip][2:]
        self.lib.mot_enemy_set(self.e, clip, hokan, flags | (0x40 if mirrored else 0), 0)
        self.check_errors('start')

    def tick(self, playback):
        root = (C.c_float * 4)()
        result = self.lib.mot_enemy_tick(self.e, playback, root)
        self.check_errors('tick')
        return np.array(root, dtype=np.float32), self.state(), result, self.pose()

    def state(self):
        values = (C.c_float * 5)()
        self.lib.mot_enemy_state(self.e, values)
        return list(values)

    def close(self):
        if self.e:
            self.lib.mot_enemy_destroy(self.e)
            self.e = None
        if self.h:
            self.lib.mot_model_destroy(self.h)
            self.h = None

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


def triangle_count(mesh):
    total = 0
    for part in mesh.parts:
        for primitive in part.prims:
            count = len(primitive.verts)
            if primitive.op == 0x80:
                total += count // 4 * 2
            elif primitive.op == 0x90:
                total += count // 3
            elif primitive.op in (0x98, 0xa0):
                total += max(0, count - 2)
            else:
                raise AssertionError(('Unexpected original primitive', primitive.op))
    return total


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    root = Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(root / 'upstream/re4/tools'))
    import drs
    from motion import fcv, modelbin, meshbin

    tool, enemy_path = [Path(p).resolve() for p in sys.argv[1:]]
    data = enemy_path.read_bytes()
    assert hashlib.sha256(data).hexdigest() == EM12_SHA, enemy_path
    archive = drs.Drs(data)
    body = modelbin.parse(archive.entries[440][1])
    body.check_tree()
    assert body.n_parts == 34
    triangles = 0
    for entry in [440, 442, 444, 449]:
        model = modelbin.parse(archive.entries[entry][1])
        assert model.n_parts == body.n_parts
        assert [(p.attach, p.parent) for p in model.parts] == [
            (p.attach, p.parent) for p in body.parts], ('Attachment hierarchy', entry)
        assert np.allclose([p.pos for p in model.parts], [p.pos for p in body.parts],
                           atol=1e-9, rtol=0), ('Attachment rest pose', entry)
        triangles += triangle_count(meshbin.parse(archive.entries[entry][1]))
    assert triangles == 3013
    assert struct.unpack_from('>2I', archive.entries[441][1]) == (0x0020af30, 3)
    motions = [fcv.parse(archive.entries[entry][1]) for entry, *_ in SOURCES]
    sequences = [fcv.parse_seq(archive.entries[entry][1]) if entry else None
                 for _, entry, _, _ in SOURCES]
    fixture = action_reference.Fixture(subprocess.check_output([str(tool), str(enemy_path)]))
    assert fixture.fields('7I') == (0x4d313900, 34, 4, triangles, triangles * 3, 3, 5)

    root_maximum = 0.
    loops = {}
    free_values = {}
    normal_reactions = {}
    with tempfile.TemporaryDirectory(prefix='re4-enemy-reference-') as temp:
        lib = build_reference(root, Path(temp) / 'host')
        with EnemyReference(lib, body, motions, sequences, fcv) as reference:
            cycles = []
            for clip, source in enumerate(SOURCES):
                assert fixture.fields('4I') == source
                assert fixture.fields('f')[0] == motions[clip].max_frame
                observed_frames = fixture.word()
                observed_cycle = np.array(fixture.fields('4f'), dtype=np.float32)
                expected = reference.cycle(clip)
                assert observed_frames == expected[0]
                assert np.max(np.abs(observed_cycle - expected[1:])) < .005, ('Cycle', clip)
                cycles.append(expected)

            assert fixture.word() == 30
            for clip, motion in enumerate(motions):
                for fraction in [0, .125, .25, .5, .875, 1]:
                    ci, frame = fixture.fields('If')
                    assert ci == clip and frame == motion.max_frame * fraction
                    fixture.compare_pose(reference.sample(clip, frame), ('EM12 FCV', clip, frame))

            assert fixture.word() == 7
            for group in range(7):
                clip = group if group < 5 else group - 3
                mirrored = group >= 5
                frames = int(cycles[clip][0])
                ticks = frames * 3 + 5 if clip == 1 else frames + 16
                source = SOURCES[clip]
                assert fixture.fields('5I') == (
                    clip, mirrored, source[3] | (0x40 if mirrored else 0), source[2], ticks)
                reference.start(clip, mirrored)
                loop_ticks = []
                seen_free = set()
                reaction = []
                for tick in range(ticks):
                    playback = fixture.fields('f')[0]
                    assert playback == 1
                    observed_root = np.array(fixture.fields('4f'), dtype=np.float32)
                    curve, sequence, state, free = fixture.fields('ff2I')
                    expected_root, expected_state, result, expected_pose = reference.tick(playback)
                    assert (curve, sequence, state, free) == tuple(expected_state[:4]), (
                        'EM12 state/SEQ/Free', clip, mirrored, tick, expected_state)
                    assert state == result
                    # No native Hokan counter is exposed. The complete posed
                    # matrix comparison below checks its exact interpolation;
                    # also check the original's one-decrement-per-tick clock.
                    assert expected_state[4] == max(0, source[2] - tick - 1)
                    delta = float(np.max(np.abs(observed_root - expected_root)))
                    assert np.isfinite(observed_root).all() and delta < .005, (
                        'EM12 root', clip, mirrored, tick, delta)
                    root_maximum = max(root_maximum, delta)
                    fixture.compare_pose(expected_pose, ('EM12 tick', clip, mirrored, tick))
                    if state & 1:
                        loop_ticks.append(tick + 1)
                    seen_free.add(free)
                    if clip in (2, 3):
                        reaction.append(expected_pose)
                loops[(clip, mirrored)] = loop_ticks
                free_values[(clip, mirrored)] = seen_free
                if clip in (2, 3):
                    if mirrored:
                        assert np.max(np.abs(np.array(reaction) - normal_reactions[clip])) > 1, (
                            'Original mirror must change the reaction', clip)
                    else:
                        normal_reactions[clip] = np.array(reaction)
                if clip >= 2:
                    assert state == 4 and sequence == frames - 1

    assert fixture.cursor == len(fixture.data), (fixture.cursor, len(fixture.data))
    walk_frames = len(sequences[1].keys)
    assert loops[(1, False)] == [walk_frames, walk_frames * 2, walk_frames * 3]
    assert free_values[(3, False)] == {0, 4}
    assert free_values[(4, False)] == {0, 128, 129}
    print(f'Original EM12 MotionMove/IK/Hokan/mirror reference PASS: {fixture.count} poses, '
          f'34 joints, max difference {fixture.maximum:.8f} game units')
    print(f'Original EM12 root/SEQ/Free reference PASS: 7 traces, 3 walk loops, '
          f'max root difference {root_maximum:.8f}; R100 ESL index 37 -> walk SEQ entry 7')
    print('Original EM12 BIN/TPL layout PASS: body/head/two hands, 3013 triangles, '
          '9039 corners, shared 34-part hierarchy and 3 textures')


if __name__ == '__main__':
    main()
