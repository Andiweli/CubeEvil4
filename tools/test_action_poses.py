#!/usr/bin/env python3
"""Independent original-source M18 action, cMot3, Hokan and waist comparison.

Usage: test_action_poses.py action_pose_tool PL00.drs WEP01.drs
Requires numpy, g++, make and Linux x86-64 for the private original-code host.
The original tree stays read-only; all reference sources/builds use a tempdir.
The ARM64 runtime itself owns native pointers and never uses fixed mappings.
No game assets are distributed.
"""
from pathlib import Path
import ctypes as C
import shutil
import subprocess
import sys
import hashlib
import struct
import tempfile
import numpy as np

P=C.c_void_p
U8P=C.POINTER(C.c_uint8)
F32P=C.POINTER(C.c_float)
U16P=C.POINTER(C.c_uint16)


def build_reference(project_root, host):
    up=Path(project_root).resolve()/'upstream/re4'
    host=Path(host)
    shutil.copytree(up/'tools/motion/host',host,
                    ignore=shutil.ignore_patterns('build','__pycache__'))
    make=host/'Makefile'
    text=make.read_text().replace('ROOT   := $(abspath ../../..)', 'ROOT   := '+str(up))
    text=text.replace('sdk_extract.cpp motion_host.cpp','sdk_extract.cpp motion_host.cpp cmot3.cpp')
    make.write_text(text)
    header=(up/'include/player.h').read_text()
    first=header.index('class cMot3 {')
    last=header.index('\n};',first)+3
    stub=host/'stub/re4_host_stub.h'
    text=stub.read_text().rsplit('#endif',1)[0]+'\n'+header[first:last]+'\n#endif\n'
    stub.write_text(text)
    subprocess.check_call([sys.executable,str(host/'prepare.py'),'extract',
                           str(up/'src/game/pl_class.cpp'),str(host/'cmot3.cpp'),
                           'cMot3::set','cMot3::set0','cMot3::move'])
    source=(up/'src/game/pl_class.cpp').read_text()
    first=source.index('cMot3::cMot3()')
    last=source.index('\n}',first)+2
    cpp=host/'cmot3.cpp'
    # The decomp's register r9 is the incoming u8 b. Portable equivalent:
    text=cpp.read_text().replace('register int rb;', '')
    text=text.replace('int mode = (u8) rb;', 'int mode = (u8) b;')
    cpp.write_text(text+'\n'+source[first:last]+'\n')
    driver=host/'motion_host.cpp'
    driver.write_text(driver.read_text()+'\n'+BLEND_API)
    result=subprocess.run(['make','-s'],cwd=host,capture_output=True,text=True)
    if result.returncode:
        raise RuntimeError('Independent original host build failed:\n'+result.stdout+result.stderr)
    return load_reference(host/'build/libmotion_host.so')


def load_reference(path):
    lib=C.CDLL(str(path))
    lib.mot_model_create.argtypes=[C.c_int,C.POINTER(C.c_int),F32P]
    lib.mot_model_create.restype=P
    lib.mot_model_destroy.argtypes=[P]
    lib.mot_model_blend_table.argtypes=[P,U16P]
    lib.mot_model_set.argtypes=[P,U8P,C.c_size_t,C.c_int,C.c_int]
    lib.mot_model_frame.argtypes=[P,C.c_float]
    lib.mot_model_read.argtypes=[P,F32P,F32P,F32P,F32P,F32P,F32P,C.POINTER(C.c_uint32)]
    lib.mot_blend_create.argtypes=[P,U8P,C.c_size_t,U8P,C.c_size_t,U8P,C.c_size_t,
                                 U8P,C.c_size_t,C.c_int,C.c_int,C.c_int]
    lib.mot_blend_create.restype=P
    lib.mot_blend_destroy.argtypes=[P]
    lib.mot_blend_sample.argtypes=[P,C.c_float,C.c_float]
    lib.mot_blend_tick.argtypes=[P,C.c_float]
    lib.mot_blend_tick.restype=C.c_uint16
    lib.mot_blend_cross.argtypes=[P,C.c_float]
    lib.mot_blend_cross.restype=C.c_int
    lib.mot_blend_reset.argtypes=[P,C.c_int,C.c_int]
    lib.mot_blend_state.argtypes=[P,F32P]
    lib.mot_blend_waist.argtypes=[P,C.c_float]
    lib.mot_blend_clock.argtypes=[C.c_float]
    lib.mot_blend_speed.argtypes=[P,C.c_float]
    return lib


class BlendReference:
    def __init__(self, lib, model, motions, base_idle, fcv,
                 hokan=0, flags=4, frame=0, sequence=None):
        self.lib=lib
        lib.mot_host_clear_errors()
        self.n=model.n_parts
        parent=(C.c_int*self.n)(*(p.parent for p in model.parts))
        rest=(C.c_float*(self.n*3))(*[v for p in model.parts for v in p.pos])
        self.h=lib.mot_model_create(self.n,parent,rest)
        self.table=None
        if model.blend_table:
            words=[len(model.blend_table),0]+[w for e in model.blend_table for w in e]
            self.table=(C.c_uint16*len(words))(*words)
            lib.mot_model_blend_table(self.h,self.table)
        # Match AnimationRig::reset's idle pose before cMot3 Hokan is captured.
        image=fcv.serialise(base_idle,'<')
        buf=(C.c_uint8*len(image)).from_buffer_copy(image)
        lib.mot_blend_clock(0)
        lib.mot_model_set(self.h,buf,len(image),4,1)
        lib.mot_model_frame(self.h,0)
        images=[]
        for motion in motions:
            image=fcv.serialise(motion,'<')
            images.append((C.c_uint8*len(image)).from_buffer_copy(image))
        args=[self.h]
        for image in images:args.extend([image,len(image)])
        seq=None
        if sequence is not None:
            image=fcv.serialise_seq(sequence,'<')
            seq=(C.c_uint8*len(image)).from_buffer_copy(image)
        self.b=lib.mot_blend_create(*args,seq,0 if seq is None else len(seq),hokan,flags,frame)
        self.mat=(C.c_float*(self.n*12))()
        self.check_errors('create')

    def check_errors(self, operation):
        if self.lib.mot_host_errors():
            raise RuntimeError('Original host logged errors: '+operation)

    def read(self):
        self.lib.mot_model_read(self.h,None,None,None,None,self.mat,None,None)
        pose=np.array(self.mat,dtype=np.float32).reshape(self.n,12)
        assert np.isfinite(pose).all()
        return pose

    def sample(self,frame,rate,waist=None):
        self.lib.mot_blend_clock(0)
        self.lib.mot_blend_sample(self.b,frame,rate)
        if waist is not None:self.lib.mot_blend_waist(self.b,waist)
        self.check_errors('sample')
        return self.read()

    def tick(self,rate,waist=None,playback=1):
        self.lib.mot_blend_clock(1)
        self.lib.mot_blend_speed(self.b,playback)
        self.result=self.lib.mot_blend_tick(self.b,rate)
        if waist is not None:self.lib.mot_blend_waist(self.b,waist)
        self.check_errors('tick')
        return self.read()

    def state(self):
        values=(C.c_float*8)()
        self.lib.mot_blend_state(self.b,values)
        # Main/secondary Seq_frame, main/secondary Mot_frame, Brate,
        # main/secondary Hokan_cnt, selected secondary (1 positive or 2 negative).
        return list(values)

    def close(self):
        if self.b:
            self.lib.mot_blend_destroy(self.b)
            self.b=None
        if self.h:
            self.lib.mot_model_destroy(self.h)
            self.h=None

    def __enter__(self):return self
    def __exit__(self,*args):self.close()

BLEND_API=r"""
// Private M18 harness only. Original cMot3 methods copied into cmot3.cpp.
struct HostBlend {
    HostModel* h;
    cMot3 controller;
    u8* image[3];
    size_t size[3];
    u8* sequence;
    int hokan, flags, frame;
};
API HostBlend* mot_blend_create(HostModel* h, const u8* m0, size_t n0,
                             const u8* m1, size_t n1, const u8* m2, size_t n2,
                             const u8* seq, size_t nseq, int hokan, int flags, int frame)
{
    HostBlend* b = new HostBlend{};
    b->h = h;
    b->size[0]=n0; b->size[1]=n1; b->size[2]=n2;
    const u8* source[3]={m0,m1,m2};
    for (int i=0;i<3;++i) {
      // GameCube MotionSetCore detects relocation by the key pointer sign.
      // Repeated set0 needs bit31 set; MAP_32BIT yields addresses below 2 GB
      // and would cause duplicate relocation on every sign crossing.
      static uintptr_t nextAddress=0x90000000u;
      uintptr_t address=nextAddress;
      nextAddress+=(b->size[i]+4095)&~uintptr_t(4095);
      b->image[i] = (u8*) mmap((void*)address,b->size[i],PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
      if(b->image[i]==MAP_FAILED) abort();
      memcpy(b->image[i], source[i], b->size[i]);
    }
    if(nseq) { b->sequence=(u8*)malloc(nseq); memcpy(b->sequence,seq,nseq); }
    b->hokan=hokan;b->flags=flags;b->frame=frame;
    b->controller.set(&h->model,b->image[0],b->image[1],b->image[2],b->sequence,(u8)hokan,0,(u16)flags,(u16)frame);
    return b;
}
API void mot_blend_destroy(HostBlend* b)
{
    b->h->model.pMotionB=0;
    for(int i=0;i<3;++i) munmap(b->image[i],b->size[i]);
    free(b->sequence);
    delete b;
}
API void mot_blend_reset(HostBlend* b, int hokan, int frame)
{
    b->controller.set(&b->h->model,b->image[0],b->image[1],b->image[2],b->sequence,(u8)hokan,0,(u16)b->flags,(u16)frame);
}
API u16 mot_blend_tick(HostBlend* b,float rate)
{
    b->controller.move(rate);
    return MotionMove(&b->h->model,0);
}
API u16 mot_blend_sample(HostBlend* b,float frame,float rate)
{
    b->controller.move(rate);
    b->h->model.Motion.Mot_attr|=0x8000;
    b->h->model.Motion.Seq_frame=frame;
    b->controller.work.Mot_attr|=0x8000;
    b->controller.work.Seq_frame=frame;
    return MotionMove(&b->h->model,0);
}
API void mot_blend_state(HostBlend* b,float* f)
{
    f[0]=b->h->model.Motion.Seq_frame;
    f[1]=b->controller.work.Seq_frame;
    f[2]=b->h->model.Motion.Mot_frame;
    f[3]=b->controller.work.Mot_frame;
    f[4]=b->controller.work.Brate;
    f[5]=b->h->model.Motion.Hokan_cnt;
    f[6]=b->controller.work.Hokan_cnt;
    f[7]=(b->controller.work.pMot==(MotionData*)b->image[1]?1:2);
}
// Original body waist is applied after MotionMove, before the player's second
// partsWorldCalc; all three 0x40000000 flags are consumed by that calculation.
API void mot_blend_waist(HostBlend* b,float waist)
{
    for(int i=1;i<=3;++i) {
      cParts* p=b->h->model.getPartsPtr(i);
      p->motParts.flags|=0x40000000;
      p->inv_offset={0.f,i==3?-waist:waist*.5f,0.f};
    }
    b->h->model.partsWorldCalc();
}
API void mot_blend_clock(float speed) { pG->Speed=speed; }

API void mot_blend_speed(HostBlend* b,float speed) { b->h->model.Motion.Seq_speed=speed; }

API int mot_blend_cross(HostBlend* b,float frame) {
    return MotionCheckCrossFrame(&b->h->model.Motion,frame);
}
"""


class Fixture:
    def __init__(self, data):
        self.data = data
        self.cursor = 0
        self.count = 0
        self.maximum = 0.

    def fields(self, fmt):
        values = struct.unpack_from('>' + fmt, self.data, self.cursor)
        self.cursor += struct.calcsize('>' + fmt)
        return values

    def word(self):
        return self.fields('I')[0]

    def compare_pose(self, expected, label):
        observed = np.frombuffer(self.data, dtype='>f4', count=expected.size,
                                 offset=self.cursor).reshape(expected.shape)
        self.cursor += expected.size * 4
        difference = float(np.max(np.abs(observed - expected)))
        assert np.isfinite(observed).all() and difference < .005, (label, difference)
        self.maximum = max(self.maximum, difference)
        self.count += 1

    def compare_state(self, reference):
        curve, sequence, state, frame12, frame31, frame36 = self.fields('ff4I')
        original = reference.state()
        assert curve == original[2] and sequence == original[0], (curve, sequence, original)
        assert state == reference.result, (state, reference.result)
        expected_events = [reference.lib.mot_blend_cross(reference.b, f)
                           for f in [12, 31, 36]]
        assert [frame12, frame31, frame36] == expected_events


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    root = Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(root / 'upstream/re4/tools'))
    import drs
    from motion import fcv, modelbin

    tool, pl, wep = [Path(p).resolve() for p in sys.argv[1:]]
    for path, sha in [
        (pl, '5ea520d7026c48e33e12256ef82173140e72cf4019758167a7d5aa5b6e356090'),
        (wep, 'fb838948d4161ee04da306ff0910980ff1b9e3b5701b43983a36e1d21bca3689'),
    ]:
        assert hashlib.sha256(path.read_bytes()).hexdigest() == sha, path
    player = drs.Drs(pl.read_bytes())
    weapon = drs.Drs(wep.read_bytes())
    body = modelbin.parse(player.entries[0][1])
    gun = modelbin.parse(weapon.entries[2][1])
    idle = fcv.parse(weapon.entries[7][1])
    entries = [30, 32, 34, 35, 36, 37, 38, 39, 41]
    motions = {i + 7: fcv.parse(weapon.entries[e][1]) for i, e in enumerate(entries)}
    gun_entries = [48, 46, 50, 53, 51, 47]
    gun_motions = [fcv.parse(weapon.entries[e][1]) for e in gun_entries]
    fixture = Fixture(subprocess.check_output([str(tool), str(pl), str(wep)]))
    assert fixture.fields('3I') == (0x4d313800, body.n_parts, gun.n_parts)
    assert body.n_parts == 119 and gun.n_parts == 4

    with tempfile.TemporaryDirectory(prefix='re4-action-reference-') as tmp:
        lib = build_reference(root, Path(tmp) / 'host')
        assert fixture.word() == 54
        for clip in range(7, 16):
            motion = motions[clip]
            for fraction in [0, .125, .25, .5, .875, 1]:
                ci, frame = fixture.fields('If')
                assert ci == clip and frame == motion.max_frame * fraction
                # cMot3 weight zero evaluates its primary like a normal action;
                # secondary IK initialization uses the same original FCV.
                with BlendReference(lib, body, [motion] * 3, idle, fcv) as ref:
                    fixture.compare_pose(ref.sample(frame, 0), ('action', clip, frame))

        for indices in [[9, 10, 11], [12, 13, 14]]:
            triplet = [motions[i] for i in indices]
            frames = triplet[0].max_frame + 1
            assert fixture.word() == 5 * frames
            for rate in [-1, -.5, 0, .5, 1]:
                for frame in range(frames):
                    assert fixture.fields('ff') == (rate, frame)
                    with BlendReference(lib, body, triplet, idle, fcv) as ref:
                        fixture.compare_pose(ref.sample(frame, rate), ('blend', indices, rate, frame))

        assert fixture.word() == 9
        aim = [motions[i] for i in [9, 10, 11]]
        for rate in [-1, 0, 1]:
            for waist in [-.35, 0, .35]:
                observed_rate, observed_waist = fixture.fields('ff')
                assert observed_rate == rate and abs(observed_waist - waist) < 1e-7
                with BlendReference(lib, body, aim, idle, fcv) as ref:
                    fixture.compare_pose(ref.sample(12, rate, waist), ('waist', rate, waist))

        crossing = [1, 1, 0, -1, -1, 0, .5, -.5, 1]
        hokan = [-.5, -.5, 0, .5, .5, -.5, -.5, 0, .5]
        for blend_frames, rates in [(0, crossing), (3, hokan)]:
            assert fixture.word() == 9
            with BlendReference(lib, body, aim, idle, fcv, hokan=blend_frames) as ref:
                for rate in rates:
                    assert fixture.fields('f')[0] == rate
                    expected = ref.tick(rate)
                    fixture.compare_state(ref)
                    fixture.compare_pose(expected, ('transition', blend_frames, rate))

        assert fixture.word() == 3
        for clip, entry, flags in [(7, 30, 4), (8, 32, 4), (15, 41, 0)]:
            assert fixture.fields('2I') == (clip, 60)
            sequence = fcv.parse_seq(weapon.entries[entry + 1][1]) if clip != 15 else None
            with BlendReference(lib, body, [motions[clip]] * 3, idle, fcv,
                                hokan=3, flags=flags, sequence=sequence) as ref:
                event_ticks = {12: [], 31: [], 36: []}
                for tick in range(60):
                    expected = ref.tick(0)
                    fixture.compare_state(ref)
                    fixture.compare_pose(expected, ('action sequence', clip, tick))
                    for frame in event_ticks:
                        if lib.mot_blend_cross(ref.b, frame):
                            event_ticks[frame].append(tick + 1)
                if clip == 15:
                    # Reload milestones are original curve-frame crossings;
                    # the displayed pose precedes the advanced Seq_frame.
                    assert event_ticks[31] == [32] and event_ticks[36] == [37]
                    assert ref.result == 4

        assert fixture.word() == 36
        for clip, motion in enumerate(gun_motions):
            for fraction in [0, .125, .25, .5, .875, 1]:
                ci, frame = fixture.fields('If')
                assert ci == clip and frame == motion.max_frame * fraction
                with BlendReference(lib, gun, [motion] * 3, gun_motions[0], fcv) as ref:
                    fixture.compare_pose(ref.sample(frame, 0), ('weapon', clip, frame))
    assert fixture.cursor == len(fixture.data), (fixture.cursor, len(fixture.data))
    print(f'Original action/cMot3/IK/Hokan/waist reference PASS: {fixture.count} poses, '
          f'119 body + 4 weapon joints, max difference {fixture.maximum:.8f} game units')
    print('Original action state/curve/SEQ/event timing PASS: draw, holster, '
          'reload, sign crossings and Hokan transitions; source root placement suppressed')


if __name__ == '__main__':
    main()
