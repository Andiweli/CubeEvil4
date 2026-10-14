#!/usr/bin/env python3
"""Independent BE SAT variant-0 table/geometry oracle over both extracted discs.
Usage: test_sat_data.py yz2_tool sat_tool GAME_ROOT
No game data is redistributed. Includes all SAT occurrences, not other variants.
"""
from pathlib import Path
import struct, subprocess, sys, tempfile, hashlib
if len(sys.argv)!=4: raise SystemExit(__doc__)
yz,tool=map(lambda p:str(Path(p).resolve()),sys.argv[1:3]); game=Path(sys.argv[3])
root=Path(__file__).resolve().parent.parent
h=lambda b,p:struct.unpack_from('>H',b,p)[0]
u=lambda b,p:struct.unpack_from('>I',b,p)[0]
rooms=entries=polygons=blocks=refs=0
with tempfile.TemporaryDirectory() as t:
 out=Path(t)/'room.dat'
 for line in (root/'app/src/main/assets/rooms.tsv').read_text().splitlines():
  if line.startswith('#'):continue
  disc,path,size,sha,source_sha=line.split('\t'); source=game/('disc'+disc)/'files'/path
  assert hashlib.sha256(source.read_bytes()).hexdigest()==source_sha
  subprocess.run([yz,str(source),str(out)],check=True,stdout=subprocess.DEVNULL)
  data=out.read_bytes();assert len(data)==int(size) and hashlib.sha256(data).hexdigest()==sha
  n=u(data,0); offsets=[u(data,16+i*4) for i in range(n)]; expected=[]
  for i,o in enumerate(offsets):
   if data[16+n*4+i*4:19+n*4+i*4]!=b'SAT':continue
   b=data[o:min([v for v in offsets if v>o],default=len(data))]
   p=u(b,4) if b[0]&128 and b[0]!=255 else 0
   nv,nn,ne,unused,np,nf,ns,nw,nb=struct.unpack_from('>9H',b,p+2)
   v=p+20; norm=v+nv*12; poly=norm+(nn+ne)*12; block=poly+np*20
   hashval=2166136261
   def add(value):
    global hashval
    for byte in struct.pack('>I',value):hashval=((hashval^byte)*16777619)&0xffffffff
   for j in range(np):
    q=poly+j*20
    ids=struct.unpack_from('>8H',b,q)
    for value in ids:add(value)
    add(u(b,q+16))
    for index in ids[:3]:
     for k in range(3):add(u(b,v+index*12+k*4))
    for k in range(3):add(u(b,norm+ids[3]*12+k*4))
   pending=[block] if nb else [];seen=set();references=0
   while pending:
    q=pending.pop();assert q not in seen;seen.add(q)
    counts=struct.unpack_from('>3H',b,q+24)
    if h(b,q+30)&1:pending.append(q+36)
    else:
     references+=sum(counts)
     for j in range(sum(counts)):assert h(b,q+36+j*2)<np
    if u(b,q+32):pending.append(q+u(b,q+32))
   assert len(seen)==nb
   expected.append([nv,nn,ne,np,nf,ns,nw,nb,references,hashval,np])
   entries+=1;polygons+=np;blocks+=nb;refs+=references
  observed=[list(map(int,s.split())) for s in subprocess.check_output([tool,str(out)],text=True).splitlines()]
  assert expected==observed,(path,expected,observed)
  rooms+=1
print(f'PASS {rooms} rooms, {entries} SAT entries (variant 0), {polygons} polygons, {blocks} blocks, {refs} references; all indices, attributes, position/normal bits match')
