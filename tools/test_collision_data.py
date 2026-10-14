#!/usr/bin/env python3
"""Independent line/ground reference and 4-second walk traces over both discs.
Usage: test_collision_data.py yz2_tool collision_tool GAME_ROOT
"""
from pathlib import Path
import struct,subprocess,sys,tempfile,hashlib,math
if len(sys.argv)!=4:raise SystemExit(__doc__)
yz,tool=map(lambda p:str(Path(p).resolve()),sys.argv[1:3]);game=Path(sys.argv[3]);root=Path(__file__).resolve().parent.parent
h=lambda b,p:struct.unpack_from('>H',b,p)[0];u=lambda b,p:struct.unpack_from('>I',b,p)[0]
sub=lambda a,b:tuple(x-y for x,y in zip(a,b));dot=lambda a,b:sum(x*y for x,y in zip(a,b))
cross=lambda a,b:(a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def accepted(a):return not(a&0x400000 or a&0x400)
def intersect(a,b,t):
 ab=sub(t[1],t[0]);ac=sub(t[2],t[0]);d=sub(b,a);n=cross(ab,ac);den=dot(n,d)
 if dot(n,n)<1e-18 or abs(den)<=math.sqrt(dot(n,n)*dot(d,d))*1e-12:return None
 fraction=dot(n,sub(t[0],a))/den
 if not 0<=fraction<1:return None
 p=sub(tuple(x+fraction*y for x,y in zip(a,d)),t[0]);aa=dot(ab,ab);bb=dot(ab,ac);cc=dot(ac,ac);pa=dot(p,ab);pc=dot(p,ac);det=aa*cc-bb*bb
 if det<=1e-12:return None
 v=(cc*pa-bb*pc)/det;w=(aa*pc-bb*pa)/det
 return fraction if v>=-1e-9 and w>=-1e-9 and v+w<=1+1e-9 else None
rooms=queries=spawns=walks=0
with tempfile.TemporaryDirectory() as tmp:
 out=Path(tmp)/'room.dat'
 for row in (root/'app/src/main/assets/rooms.tsv').read_text().splitlines():
  if row.startswith('#'):continue
  disc,path,size,sha,srcsha=row.split('\t');src=game/('disc'+disc)/'files'/path
  assert hashlib.sha256(src.read_bytes()).hexdigest()==srcsha
  subprocess.run([yz,str(src),str(out)],check=True,stdout=subprocess.DEVNULL);data=out.read_bytes()
  assert len(data)==int(size) and hashlib.sha256(data).hexdigest()==sha
  n=u(data,0);offs=[u(data,16+i*4) for i in range(n)];sat=[o for i,o in enumerate(offs) if data[16+n*4+i*4:19+n*4+i*4]==b'SAT']
  rows=subprocess.check_output([tool,str(out)],text=True).splitlines();rooms+=1
  if not sat:assert not rows;continue
  o=sat[0];b=data[o:min([v for v in offs if v>o],default=len(data))];p=u(b,4) if b[0]&128 and b[0]!=255 else 0
  nv,nn,ne,_,np,nf,ns,nw,nb=struct.unpack_from('>9H',b,p+2);vbase=p+20;nbase=vbase+nv*12;poly=nbase+(nn+ne)*12
  vertices=[struct.unpack_from('>3f',b,vbase+i*12) for i in range(nv)];normals=[struct.unpack_from('>3f',b,nbase+i*12) for i in range(nn)]
  tris=[tuple(vertices[h(b,poly+i*20+k*2)] for k in range(3)) for i in range(np)];attrs=[u(b,poly+i*20+16) for i in range(np)]
  for row in rows:
   parts=row.split()
   if parts[0]=='Q':
    j=int(parts[1]);c=tuple(sum(v[k] for v in tris[j])/3 for k in range(3));norm=normals[h(b,poly+j*20+6)]
    a=tuple(c[k]+norm[k]*500 for k in range(3));end=tuple(c[k]-norm[k]*500 for k in range(3));best=None
    for index,t in enumerate(tris):
     if not accepted(attrs[index]):continue
     rate=intersect(a,end,t)
     if rate is not None and (best is None or rate<best[0]):best=(rate,index)
    if best is None:assert parts[2]=='-1',(path,j,parts)
    else:
     rate,index=best;assert int(parts[2])==index and int(parts[3])==attrs[index]|0x1000000,(path,j,best,parts)
     assert abs(float(parts[4])-rate)<1e-7
     expect=tuple(a[k]+rate*(end[k]-a[k]) for k in range(3))
     assert all(abs(float(parts[5+k])-expect[k])<1e-4 for k in range(3)),(path,j)
    queries+=1
   elif parts[0]=='W' and parts[1]=='1':
    spawns+=1;pos=tuple(map(float,parts[2:5]));index=int(parts[5]);assert index<nf+ns and accepted(attrs[index]);assert all(math.isfinite(v) for v in pos)
    top=(pos[0],pos[1]+1,pos[2]);bottom=(pos[0],pos[1]-1,pos[2]);rate=intersect(top,bottom,tris[index]);assert rate is not None and abs(rate-.5)<1e-4,(path,'ground trace',pos,index)
    walks+=1
print(f'PASS {rooms} rooms; {queries} nearest line queries match independent reference; {spawns} free diagnostic starts / {walks} grounded four-second walk traces')
