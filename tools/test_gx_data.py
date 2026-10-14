#!/usr/bin/env python3
"""Byte-compare native GX references/topology with an independent room reference.
Usage: test_gx_data.py yz2_tool gx_tool GAME_ROOT
No original game data are bundled or included in the produced source package.
"""
from pathlib import Path
import hashlib
import struct
import subprocess
import sys
import tempfile

if len(sys.argv)!=4:
    raise SystemExit(__doc__)
yz2, tool = [str(Path(p).resolve()) for p in sys.argv[1:3]]
game = Path(sys.argv[3]).resolve()
root = Path(__file__).resolve().parent.parent
rooms = models = commands = corners = triangles = 0
kinds = {}
expected_rooms = {}

def u32(data, offset):
    return struct.unpack_from('>I', data, offset)[0]

def topology(op, count):
    if op == 0x80:
        return [t for start in range(0, count, 4) for t in [(start, start+1, start+2), (start, start+2, start+3)]]
    if op == 0x90:
        return list(zip(range(0,count,3), range(1,count,3), range(2,count,3)))
    if op == 0x98:
        return [(i, i+1, i+2) if i%2==0 else (i+1,i,i+2) for i in range(count-2)]
    if op == 0xa0:
        return [(0,i,i+1) for i in range(1,count-1)]
    return []

with tempfile.TemporaryDirectory(prefix='re4-gx-') as temp:
    out = Path(temp) / 'room.dat'
    for line in (root/'app/src/main/assets/rooms.tsv').read_text().splitlines():
        if line.startswith('#'):
            continue
        disc, path, size, expected_hash, source_hash = line.split('\t')
        source = game/('disc'+disc)/'files'/path
        assert hashlib.sha256(source.read_bytes()).hexdigest()==source_hash
        subprocess.run([yz2,str(source),str(out)],check=True,stdout=subprocess.DEVNULL)
        data=out.read_bytes()
        assert len(data)==int(size) and hashlib.sha256(data).hexdigest()==expected_hash
        n=u32(data,0); offsets=struct.unpack_from('>'+str(n)+'I',data,16)
        expected=bytearray(); occurrence=0; room_stats=[]
        for entry, off in enumerate(offsets):
            if data[16+4*n+4*entry:19+4*n+4*entry]!=b'SMD':
                continue
            end=min((p for p in offsets if p>off),default=len(data)); s=data[off:end]
            si=occurrence; occurrence+=1
            if s[0]!=0x40:
                continue
            count=struct.unpack_from('>H',s,2)[0]
            work_start=20+4*u32(s,16) if s[1]&1 else 16
            bin_ids=[(s[p+36] if s[p+36]!=255 else 0) for p in range(work_start,work_start+count*72,72) if s[p+39]!=255 and not (u32(s,p+68)&16)]
            local_count=max(bin_ids,default=-1)+1; bt=u32(s,4)
            smd_commands=smd_corners=smd_triangles=0
            for model in range(local_count):
                base=bt+u32(s,bt+model*4); flags=u32(s,base+32)
                colored=bool(flags&0x80000000); stride=8 if colored else 6
                nv,nn=struct.unpack_from('>HH',s,base+56)
                nl=struct.unpack_from('>H',s,base+26)[0]; p=base+u32(s,base+28)
                encoded=[]; headers=[]
                for display_list in range(nl):
                    headers.append(s[p:p+32])
                    cur=p+32; end=cur+u32(s,p+24)
                    while cur<end:
                        op=s[cur]; cur+=1
                        if not op:
                            continue
                        assert op in (0x80,0x90,0x98,0xa0,0xa8,0xb0,0xb8), (path,si,model,op)
                        nrefs=struct.unpack_from('>H',s,cur)[0]; cur+=2
                        assert nrefs>0 and cur+nrefs*stride<=end
                        attrs=list(struct.iter_unpack('>4H' if colored else '>3H',s[cur:cur+nrefs*stride])); cur+=nrefs*stride
                        assert all(v[0]<nv and v[1]<nn for v in attrs)
                        tris=topology(op,nrefs)
                        record=bytearray(struct.pack('>4I',display_list,op,nrefs,len(tris)))
                        for v in attrs:
                            record.extend(struct.pack('>4I',*v) if colored else struct.pack('>4I',v[0],v[1],0,v[2]))
                        for t in tris:
                            record.extend(struct.pack('>3I',*t))
                        encoded.append(record)
                        commands+=1; corners+=nrefs; triangles+=len(tris)
                        smd_commands+=1; smd_corners+=nrefs; smd_triangles+=len(tris)
                        kinds[op]=kinds.get(op,0)+1
                    assert cur==end
                    p=end
                expected.extend(struct.pack('>4I',si,model,len(encoded),len(headers)))
                for header in headers:
                    expected.extend(header)
                for record in encoded:
                    expected.extend(record)
                models+=1
            room_stats.append((si,smd_commands,smd_corners,smd_triangles))
        actual=subprocess.check_output([tool,str(out)])
        if actual!=expected:
            first=next((i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b),min(len(actual),len(expected)))
            raise SystemExit(f'{path}: byte mismatch at {first}; native={len(actual)} reference={len(expected)}')
        if path in ('St1/r100.das','St3/r300.das'):
            expected_rooms[path]=room_stats
        rooms+=1
        if rooms%20==0:
            print(rooms,'rooms byte-compared',flush=True)
print(rooms,'rooms;',models,'BIN entries;',commands,'primitives;',corners,'vertex references;',triangles,'triangles including degenerates PASS')
print('Opcodes:',{hex(k):v for k,v in kinds.items()})
print('Device reference summaries:',expected_rooms)
