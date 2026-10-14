#!/usr/bin/env python3
"""Independent rest hierarchy/matrix/rigid world-mesh reference.
Usage: test_scene_data.py yz2_tool scene_tool GAME_ROOT
Checks original S8/S16 normal arrays, separate normal indices and inverse-transpose world normals.
Requires numpy. No original data are distributed.
"""
from pathlib import Path
import hashlib, math, struct, subprocess, sys, tempfile
import numpy as np
if len(sys.argv)!=4: raise SystemExit(__doc__)
yz2,tool=[str(Path(p).resolve()) for p in sys.argv[1:3]]; game=Path(sys.argv[3]).resolve();root=Path(__file__).resolve().parent.parent
normals8=normals16=normal_corners=singular_total=zero_total=0
rooms=sm_count=joints=matrices=included=skipped=triangles=0; device={}
def u32(d,p):return struct.unpack_from('>I',d,p)[0]
def topo(op,n):
 if op==128:return [(i,i+1,i+2) for i in range(0,n,4)]+[(i,i+2,i+3) for i in range(0,n,4)] # quad ordering fixed below
 if op==144:return [(i,i+1,i+2) for i in range(0,n,3)]
 if op==152:return [(i,i+1,i+2) if not i%2 else(i+1,i,i+2) for i in range(n-2)]
 if op==160:return [(0,i,i+1) for i in range(1,n-1)]
 return []
with tempfile.TemporaryDirectory(prefix='re4-scene-') as tmp:
 out=Path(tmp)/'room.dat'
 for line in (root/'app/src/main/assets/rooms.tsv').read_text().splitlines():
  if line.startswith('#'):continue
  disc,path,size,sha,srcsha=line.split('\t');src=game/('disc'+disc)/'files'/path
  assert hashlib.sha256(src.read_bytes()).hexdigest()==srcsha
  subprocess.run([yz2,str(src),str(out)],check=True,stdout=subprocess.DEVNULL);data=out.read_bytes();assert len(data)==int(size) and hashlib.sha256(data).hexdigest()==sha
  actual=subprocess.check_output([tool,str(out)]); cursor=0;n=u32(data,0);offsets=struct.unpack_from('>'+str(n)+'I',data,16);occ=-1;stats=[]
  for e,o in enumerate(offsets):
   if data[16+n*4+e*4:19+n*4+e*4]!=b'SMD':continue
   occ+=1;s=data[o:min((v for v in offsets if v>o),default=len(data))]
   if s[0]!=64:continue
   wc=struct.unpack_from('>H',s,2)[0];start=20+4*u32(s,16) if s[1]&1 else 16;work=[s[p:p+72] for p in range(start,start+wc*72,72)]
   count=max((w[36] if w[36]!=255 else 0 for w in work if w[39]!=255 and not(u32(w,68)&16)),default=-1)+1;bt=u32(s,4);model=[];expected=[];normal_indices=[]
   for i in range(count):
    b=bt+u32(s,bt+i*4);h=b+u32(s,b);parts=s[b+25];centers=[];attach=[]
    for j in range(parts):
     parent=s[h+j*16+1];c=np.array(struct.unpack_from('>3f',s,h+j*16+4),dtype=np.float32)
     if parent!=255:assert parent<j;c=(c+centers[parent]).astype(np.float32)
     centers.append(c);expected.extend(c);attach.append(s[h+j*16]);joints+=1
    nc=struct.unpack_from('>H',s,b+58)[0];normal_start=b+u32(s,b+52);byte_normal=bool(u32(s,b+32)&0x20000000)
    normals=np.frombuffer(s,dtype=np.int8 if byte_normal else '>i2',count=nc*4,offset=normal_start).reshape(-1,4)[:,:3].astype(np.float64)/(64 if byte_normal else 16384)
    if byte_normal:normals8+=nc
    else:normals16+=nc
    normal_indices.extend(range(len(expected),len(expected)+nc*3));expected.extend(normals.flatten())
    model.append((b,centers,attach,normals))
   roots=[]
   for w in work:
    p=np.array(struct.unpack_from('>3f',w),dtype=np.float64);x,y,z=struct.unpack_from('>3f',w,12);scale=np.array(struct.unpack_from('>3f',w,24))
    rx=np.array([[1,0,0],[0,math.cos(x),-math.sin(x)],[0,math.sin(x),math.cos(x)]]);ry=np.array([[math.cos(y),0,math.sin(y)],[0,1,0],[-math.sin(y),0,math.cos(y)]]);rz=np.array([[math.cos(z),-math.sin(z),0],[math.sin(z),math.cos(z),0],[0,0,1]])
    m=np.column_stack(((rz@ry@rx)@np.diag(scale),p));expected.extend(m.flatten());roots.append(m);matrices+=1
   inc=skip=tris=singular=zero=0;mesh=[];normal_mesh=[]
   for w,m in zip(work,roots):
    if w[39]==255:continue
    if u32(w,68)&16 or w[38]!=255:skip+=1;continue
    b,centers,attach,normals=model[w[36] if w[36]!=255 else 0]
    if s[b+25]!=1 or s[b+24]>1 or struct.unpack_from('>H',s,b+42)[0]>255 or attach[0]!=0 or s[b+u32(s,b)+1]!=255:skip+=1;continue
    nv=struct.unpack_from('>H',s,b+56)[0];v=b+u32(s,b+48);verts=np.frombuffer(s,dtype='>i2',count=nv*4,offset=v).reshape(-1,4)[:,:3].astype(np.float64)*2**(-s[b+40])
    ids=[];nids=[];part=b+u32(s,b+28);stride=8 if u32(s,b+32)&0x80000000 else 6
    for _ in range(struct.unpack_from('>H',s,b+26)[0]):
     cur=part+32;end=cur+u32(s,part+24)
     while cur<end:
      op=s[cur];cur+=1
      if not op:continue
      number=struct.unpack_from('>H',s,cur)[0];cur+=2;refs=[struct.unpack_from('>H',s,cur+k*stride)[0] for k in range(number)];nrefs=[struct.unpack_from('>H',s,cur+k*stride+2)[0] for k in range(number)];cur+=number*stride
      ts=topo(op,number)
      if op==128:ts=[t for i in range(0,number,4) for t in[(i,i+1,i+2),(i,i+2,i+3)]]
      ids.extend(refs[c] for t in ts for c in t);nids.extend(nrefs[c] for t in ts for c in t)
     part=end
    if not ids:skip+=1;continue
    if np.any(np.array(struct.unpack_from('>3f',w,24))==0):
     wn=np.zeros((len(nids),3));singular+=1
    else:wn=normals[nids]@np.linalg.inv(m[:,:3])
    zero+=int(np.count_nonzero(np.all(wn==0,axis=1)));normal_mesh.extend(wn.flatten());normal_corners+=len(nids)
    points=verts[ids]+centers[0];world=points@m[:,:3].T+m[:,3];mesh.extend(world.flatten());tris+=len(ids)//3;inc+=1
   expected.extend(mesh);normal_indices.extend(range(len(expected),len(expected)+len(normal_mesh)));expected.extend(normal_mesh);si,ai,askip,at,number,actual_zero,actual_singular=struct.unpack_from('>7I',actual,cursor);cursor+=28;
   assert (actual_zero,actual_singular)==(zero,singular),(path,'normal diagnostics')
   zero_total+=zero;singular_total+=singular;
   floats=np.frombuffer(actual,dtype='>f4',count=number,offset=cursor);cursor+=number*4
   assert (si,ai,askip,at)==(occ,inc,skip,tris),(path,(si,ai,askip,at),(occ,inc,skip,tris))
   assert len(floats)==len(expected)
   diff=np.abs(floats-np.array(expected));assert np.all(diff<=.125),(path,float(diff.max()),int(diff.argmax()))
   assert np.allclose(floats[normal_indices],np.array(expected)[normal_indices],rtol=1e-4,atol=1e-5),(path,'original/world normals')
   stats.append((occ,inc,skip,tris));sm_count+=1;included+=inc;skipped+=skip;triangles+=tris
  assert cursor==len(actual)
  if path in('St1/r100.das','St3/r300.das'):device[path]=stats
  rooms+=1
  if rooms%20==0:print(rooms,'rooms checked',flush=True)
print(rooms,'rooms;',sm_count,'SMD;',joints,'rest joints;',matrices,'placement matrices;',included,'rigid placements;',skipped,'excluded;',triangles,'world triangles PASS (absolute float tolerance 0.125 world units)')
print('Device:',device)

print(normals8,'S8 normal entries;',normals16,'S16 normal entries;',normal_corners,'world normal corners;',zero_total,'zero normals;',singular_total,'singular placements: PASS')
