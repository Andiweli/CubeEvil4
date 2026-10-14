#!/usr/bin/env python3
"""Independent original-code FCV/IK pose and BIN skin comparison.

Usage: test_actor_data.py actor_tool PL00.drs WEP01.drs
Requires numpy, g++, make, and Linux x86-64 for the upstream reference helper.
Only the private reference helper uses MAP_32BIT; the Android port never does.
No original game assets are distributed. The pinned upstream tree is read-only.
"""
from pathlib import Path
import hashlib
import shutil
import struct
import subprocess
import sys
import tempfile
import numpy as np

if len(sys.argv)!=4: raise SystemExit(__doc__)
root=Path(__file__).resolve().parent.parent
up=root/'upstream/re4'
sys.path.insert(0,str(up/'tools'))
import drs
from motion import fcv,modelbin,meshbin,evalhost
tool,pl,wep=[Path(p).resolve() for p in sys.argv[1:]]
for p,sha in [(pl,'5ea520d7026c48e33e12256ef82173140e72cf4019758167a7d5aa5b6e356090'),
              (wep,'fb838948d4161ee04da306ff0910980ff1b9e3b5701b43983a36e1d21bca3689')]:
    assert hashlib.sha256(p.read_bytes()).hexdigest()==sha,p
player=drs.Drs(pl.read_bytes());weapon=drs.Drs(wep.read_bytes())
data=subprocess.check_output([str(tool),str(pl),str(wep)])
n,corners=struct.unpack_from('>2I',data);cursor=8
model=modelbin.parse(player.entries[0][1]);assert n==model.n_parts==119
maximum=0
with tempfile.TemporaryDirectory(prefix='re4-motion-reference-') as tmp:
    host=Path(tmp)/'host'
    shutil.copytree(up/'tools/motion/host',host,ignore=shutil.ignore_patterns('build','__pycache__'))
    make=host/'Makefile'
    make.write_text(make.read_text().replace('ROOT   := $(abspath ../../..)','ROOT   := '+str(up)))
    evalhost.HOST_DIR=str(host)
    for clip,index in enumerate([7,13,17]):
        motion=fcv.parse(weapon.entries[index][1])
        reference=evalhost.Player(model,motion,loop=True,ik=True)
        for expected_frame in [0,motion.max_frame*.125,motion.max_frame*.25,
                               motion.max_frame*.5,motion.max_frame*.875,motion.max_frame]:
            ci,frame=struct.unpack_from('>If',data,cursor);cursor+=8
            assert ci==clip and frame==expected_frame
            observed=np.frombuffer(data,dtype='>f4',count=n*12,offset=cursor).reshape(n,12);cursor+=n*48
            expected=np.array(reference.frame(frame).mat,dtype=np.float32)
            difference=float(np.max(np.abs(observed-expected)))
            maximum=max(maximum,difference)
            assert np.isfinite(observed).all() and difference<.08,(clip,frame,difference)
print(f'Original MotionMove/IK/blend reference PASS: 18 poses, {n} joints, max difference {maximum:.7f} game units')
trace=np.frombuffer(data,dtype='>f4',count=300*n*12,offset=cursor).reshape(300,n,12);cursor+=300*n*48
assert np.isfinite(trace).all() and np.max(np.abs(trace))<10000
# Root bone and left/right leg endpoints change in walk/run; stopped idle also
# varies. This is an animation trace, not a cPlayer or scenario simulation.
assert np.max(np.abs(trace[130]-trace[145]))>10
assert np.max(np.abs(trace[240]-trace[249]))>10
print('SEQ/loop/transition trace PASS: 300 finite poses over idle, walk, run')

def triangles(op,n):
    if op==128:return [t for i in range(0,n,4) for t in [(i,i+1,i+2),(i,i+2,i+3)]]
    if op==144:return [(i,i+1,i+2) for i in range(0,n,3)]
    if op==152:return [(i,i+1,i+2) if i%2==0 else (i+1,i,i+2) for i in range(n-2)]
    if op==160:return [(0,i,i+1) for i in range(1,n-1)]
    return []
count=0
for arc,index in [(player,0),(player,6),(player,4),(player,2),(player,5),(weapon,6),(player,20)]:
    mesh=meshbin.parse(arc.entries[index][1])
    for part in mesh.parts:
        for primitive in part.prims:
            for triangle in triangles(primitive.op,len(primitive.verts)):
                for corner in triangle:
                    vi,ni,ci,ti=primitive.verts[corner]
                    observed=struct.unpack_from('>6f',data,cursor);cursor+=24
                    expected=(*mesh.position(vi),*mesh.normal(ni))
                    assert np.allclose(observed,expected,atol=1e-6),(index,vi,ni)
                    for matrix_index in [mesh.vtx[vi][3],mesh.nrm[ni][3]]:
                        weight=mesh.weights[matrix_index]
                        observed_count=struct.unpack_from('>I',data,cursor)[0];cursor+=4
                        assert observed_count==weight.num
                        expected_pairs=weight.pairs()
                        for j in range(3):
                            ji,rate=struct.unpack_from('>If',data,cursor);cursor+=8
                            expected_joint,expected_rate=expected_pairs[j] if j<weight.num else (0,0)
                            assert ji==expected_joint and abs(rate-expected_rate)<1e-6,(index,vi,ni,j)
                    count+=1
assert count==corners==9096*3 and cursor==len(data),(count,corners,cursor,len(data))
print(f'Independent BIN skin PASS: 7 models, {count} corners, separate position/normal weights, 9096 triangles')
