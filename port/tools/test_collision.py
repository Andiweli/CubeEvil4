#!/usr/bin/env python3
"""Private real SAT/EAT tests, independent triangle oracle, host and dual ABI.

Ray inputs are geometry-derived test probes, never player/camera start positions.
No collision data or reports are written into Git.
"""
import argparse,json,math,struct,sys,uuid
from pathlib import Path
from original_import.pipeline import external_root,write_json
from test_engine_loading import run
from test_gx_scene import sha
sys.dont_write_bytecode=True

def parse(blob):
    base=struct.unpack_from('>I',blob,4)[0] if blob[0]!=255 and blob[0]&128 else 0
    fields=struct.unpack_from('>BB9H',blob,base)
    nv,nn,ne,np,nf,ns,nw,nb=fields[2],fields[3],fields[4],fields[6],fields[7],fields[8],fields[9],fields[10]
    at=base+20
    vertices=[struct.unpack_from('>3f',blob,at+12*n) for n in range(nv)];at+=12*nv
    normals=[struct.unpack_from('>3f',blob,at+12*n) for n in range(nn)];at+=12*(nn+ne)
    polys=[struct.unpack_from('>8HI',blob,at+20*n) for n in range(np)]
    return dict(counts=[nv,nn,ne,np,nb],groups=[nf,ns,nw],vertices=vertices,normals=normals,polys=polys)

def sub(a,b):return [x-y for x,y in zip(a,b)]
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def oracle(data,a,b,flags,mask,effect):
    nf,ns,nw=data['groups'];indices=range(nf+ns) if flags&64 else range(nf+ns,nf+ns+nw) if flags&128 else range(len(data['polys']))
    delta=sub(b,a);closest=1.;point=list(b);normal=[0.,0.,0.];attribute=0
    for no in indices:
        poly=data['polys'][no];v=[data['vertices'][i] for i in poly[:3]];n=data['normals'][poly[3]];attr=poly[8]
        # Independent directed plane and barycentric triangle intersection.
        denom=dot(delta,n)
        if denom>=-1e-10:continue
        t=dot(sub(v[0],a),n)/denom
        if t<0 or t>=closest:continue
        p=[x+t*d for x,d in zip(a,delta)]
        e0=sub(v[1],v[0]);e1=sub(v[2],v[0]);q=sub(p,v[0]);aa=dot(e0,e0);ab=dot(e0,e1);bb=dot(e1,e1)
        det=aa*bb-ab*ab
        if abs(det)<1e-12:continue
        u=(dot(q,e0)*bb-dot(q,e1)*ab)/det;w=(dot(q,e1)*aa-dot(q,e0)*ab)/det
        if min(u,w,1-u-w)<-1e-8:continue
        reject=any(flags&f and attr&bit for f,bit in ([(0x400,0x4000),(0x800,0x400000)] if effect else [(0x1000,0x400000),(0x2000,0x4000),(0x4000,0x40),(0x800,0x8000),(0x8000,0x800000)]))
        if not effect and attr&0x400 and not flags&0x8000:reject=True
        if reject or attr&mask:continue
        closest=t;point=p;normal=list(n);attribute=attr|0x1000000
    return dict(attribute=attribute,point=point,normal=normal)

from retroid_adb import hardware_sequence

@hardware_sequence()
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--data',required=True);p.add_argument('--output',required=True)
    p.add_argument('--host-exe');p.add_argument('--adb');p.add_argument('--serial');p.add_argument('--repeat',type=int,default=2)
    a=p.parse_args();root=Path(__file__).resolve().parents[2];data=external_root(a.data,root,[]);out=external_root(a.output,root,[])
    if data==out or not 1<=a.repeat<=10 or not (a.host_exe or a.adb):raise ValueError('separate output/host or device/repeat')
    out.mkdir(parents=True,exist_ok=True)
    manifest=json.loads((data/'manifest.json').read_text());cases=[]
    for disc,room in [(0,'r100'),(0,'r102'),(0,'r119'),(0,'r120'),(1,'r300')]:
        f=next(f for f in manifest['files'] if f['disc_number']==disc and f['path'].lower()==f'st{room[1]}/{room}.das')
        body=(data/'blobs'/f['analysis']['body_sha256']).read_bytes()
        for tag in ['SAT','EAT']:
            e=next(e for e in f['analysis']['entries'] if e['tag']==tag);blob=body[e['offset']:e['offset']+e['size']]
            if sha(blob)!=e['sha256']:raise ValueError('original collision block integrity')
            parsed=parse(blob);queries=[]
            for no in range(0,len(parsed['polys']),max(1,len(parsed['polys'])//96)):
                poly=parsed['polys'][no];v=[parsed['vertices'][i] for i in poly[:3]];n=parsed['normals'][poly[3]]
                if dot(n,n)<1e-10:continue
                c=[sum(t[k] for t in v)/3 for k in range(3)];start=[c[k]+128*n[k] for k in range(3)];end=[c[k]-128*n[k] for k in range(3)]
                for flags in [0,64,128,0x1000,0x2000,0x8000,0x400,0x800]:queries.append((start,end,flags,0))
                if n[1]>.8:queries.append(([c[0],c[1]+256,c[2]],[c[0],c[1]-256,c[2]],64,0))
            name=f'd{disc}-{room}-{tag}';source=out/(name+'.bin');source.write_bytes(blob);qpath=out/(name+'.queries')
            qpath.write_text('\n'.join(' '.join(format(x,'.9g') for x in (*s,*t,fl,mask)) for s,t,fl,mask in queries)+'\n')
            # Parse the emitted decimal probes, which the native process consumes.
            def single(x):return struct.unpack('<f',struct.pack('<f',float(x)))[0]
            queries=[([single(x) for x in s],[single(x) for x in t],f,m) for s,t,f,m in queries]
            refs=[oracle(parsed,*q,tag=='EAT') for q in queries];cases.append((name,source,qpath,tag=='EAT',parsed,refs))
    results=[]
    def execute(command,bits,abi,paths):
        for repeat in range(a.repeat):
            for case,path in zip(cases,paths):
                name,src,qfile,effect,parsed,refs=case
                got=json.loads(run([*command,'collision',path[0],path[1],int(effect)]))
                if got['pointer_bits']!=bits or got['counts']!=parsed['counts'] or got['sha256']!=sha(src.read_bytes()) or not got['released_and_repeated']:raise ValueError('collision ownership/ABI/counts/hash')
                if len(got['queries'])!=len(refs):raise ValueError('collision query count')
                error=0
                for n,(actual,expected) in enumerate(zip(got['queries'],refs)):
                    if actual['attribute']!=expected['attribute']:raise ValueError(f'{name} query {n}: hit attribute {actual} != {expected}')
                    delta=max(abs(x-y) for x,y in zip(actual['point'],expected['point']));error=max(error,delta)
                    if delta>.2 or max(abs(x-y) for x,y in zip(actual['normal'],expected['normal']))>.0001:raise ValueError(f'{name} query {n}: intersection/normal mismatch')
                results.append(dict(abi=abi,repeat=repeat+1,name=name,max_point_error=error,actual=got))
                write_json(out/f'collision-{abi}.json',dict(results=[r for r in results if r['abi']==abi]))
                print(f'PASS {abi} {name}: {len(refs)} original queries, error {error:.6g}',flush=True)
    if a.host_exe:
        exe=Path(a.host_exe).resolve();bits=64 if 'ninja' in str(exe) else 32
        execute([exe],bits,f'host-{bits}',[(s,q) for _,s,q,*_ in cases])
    if a.adb:
        if not a.serial:raise ValueError('ADB serial required')
        def adb(*args):return run([a.adb,'-s',a.serial,*args])
        remote='/data/local/tmp/re4-collision-'+uuid.uuid4().hex;adb('shell','mkdir','-p',remote)
        paths=[]
        for name,s,q,*_ in cases:
            dst=(remote+'/'+s.name,remote+'/'+q.name);adb('push',s,dst[0]);adb('push',q,dst[1]);paths.append(dst)
        for abi,bits in [('armeabi-v7a',32),('arm64-v8a',64)]:
            exe=root/'port/build'/('android-'+abi)/'re4_game_tool';target=remote+'/test-'+abi;adb('push',exe,target);adb('shell','chmod','755',target)
            execute([a.adb,'-s',a.serial,'shell',target],bits,abi,paths)
    write_json(out/'collision-summary.json',dict(kind='isolated_original_queries_not_actor_simulation',results=results))

if __name__ == '__main__':
    raise SystemExit(main())
