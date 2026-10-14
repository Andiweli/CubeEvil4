#!/usr/bin/env python3
"""Independent UV and base-texture reference from room bytes.
Usage: test_material_data.py yz2_tool material_tool GAME_ROOT
No game data bundled. Includes alignment padding in UV section bounds.
"""
from functools import lru_cache
from pathlib import Path
import hashlib, struct, subprocess, sys, tempfile
if len(sys.argv) != 4:
    raise SystemExit(__doc__)
yz2, tool = [str(Path(v).resolve()) for v in sys.argv[1:3]]
game = Path(sys.argv[3]).resolve()
root = Path(__file__).resolve().parent.parent
u32 = lambda b,p: struct.unpack_from('>I',b,p)[0]
u16 = lambda b,p: struct.unpack_from('>H',b,p)[0]
def byte_hash(values):
    value=2166136261
    for v in values:value=((value^v)*16777619)&0xffffffff
    return value
@lru_cache(maxsize=None)
def neutral_hash(corners):return byte_hash(bytes([255])*(corners*4))
color_entries = color_draws = color_triangles = 0
def triangles(op,n):
    if op == 128: return [t for i in range(0,n,4) for t in ((i,i+1,i+2),(i,i+2,i+3))]
    if op == 144: return [(i,i+1,i+2) for i in range(0,n,3)]
    if op == 152: return [(i,i+1,i+2) if i%2==0 else (i+1,i,i+2) for i in range(n-2)]
    if op == 160: return [(0,i,i+1) for i in range(1,n-1)]
    return []
mip_images = mip_levels = mip_pixels = bounded_chains = 0
rooms = models = uv_pairs = draws = textured = fallback = alpha_yes = alpha_no = intensity_images = 0
with tempfile.TemporaryDirectory(prefix='re4-material-') as tmp:
    out = Path(tmp)/'room.dat'
    for line in (root/'app/src/main/assets/rooms.tsv').read_text().splitlines():
        if line.startswith('#'): continue
        disc,path,size,sha,srcsha = line.split('\t')
        src = game/('disc'+disc)/'files'/path
        assert hashlib.sha256(src.read_bytes()).hexdigest() == srcsha
        subprocess.run([yz2,str(src),str(out)],check=True,stdout=subprocess.DEVNULL)
        data = out.read_bytes()
        assert len(data)==int(size) and hashlib.sha256(data).hexdigest()==sha
        actual = subprocess.check_output([tool,str(out)])
        cursor = 0
        def words(n):
            global cursor
            v = struct.unpack_from('>'+str(n)+'I',actual,cursor);cursor+=n*4
            return v
        def floats(n):
            global cursor
            v = struct.unpack_from('>'+str(n)+'f',actual,cursor);cursor+=n*4
            return v
        n=u32(data,0); offsets=[u32(data,16+i*4) for i in range(n)]; occurrence=-1
        for i,o in enumerate(offsets):
            if data[16+n*4+i*4:19+n*4+i*4] != b'SMD': continue
            occurrence+=1
            s=data[o:min((v for v in offsets if v>o),default=len(data))]
            if s[0]!=64: continue
            start=20+4*u32(s,16) if s[1]&1 else 16
            works=[s[start+j*72:start+(j+1)*72] for j in range(u16(s,2))]
            local=[w for w in works if w[39]!=255 and not(u32(w,68)&16)]
            bins=max((0 if w[36]==255 else w[36] for w in local),default=-1)+1
            tpls=max((0 if w[37]==255 else w[37] for w in local),default=-1)+1
            mots=max((w[38] for w in works if w[39]!=255 and not(u32(w,68)&64) and w[38]!=255),default=-1)+1
            resources=[]
            tables=[u32(s,f) for f in (4,8,12)]
            arrays=[[t+u32(s,t+j*4) for j in range(count)] for t,count in zip(tables,(bins,tpls,mots))]
            resources=tables+[v for a in arrays for v in a]+[len(s)]
            assert words(2)==(occurrence,bins)
            model=[]
            for b in arrays[0]:
                end=min(v for v in resources if v>b)
                colored=bool(u32(s,b+32)&0x80000000)
                ambient=bool(u32(s,b+32)&0x40000000)
                fields=[0,12,16,20,28,44,48,52]+([64,68] if u32(s,b+60)==0x20030818 else [])
                sections=[b+u32(s,b+f) for f in fields if u32(s,b+f) and u32(s,b+f)<=end-b]
                rel=u32(s,b+16);uv=[]
                if rel>=64:
                    v=b+rel;stop=min([end]+[a for a in sections if a>v])
                    fmt='>hh' if colored else '>HH';scale=256 if colored else 32768
                    uv=[tuple(x/scale for x in struct.unpack_from(fmt,s,p)) for p in range(v,v+(stop-v)//4*4,4)]
                color_data=b''
                rel=u32(s,b+12)
                if colored and rel>=64:
                    c=b+rel;stop=min([end]+[a for a in sections if a>c])
                    color_data=s[c:c+(stop-c)//4*4]
                assert words(5)==(len(uv),colored,ambient,len(color_data)//4,byte_hash(color_data)),(path,b,'color array')
                color_entries+=len(color_data)//4
                assert floats(len(uv)*2)==tuple(x for pair in uv for x in pair),(path,b,'UV')
                uv_pairs+=len(uv);models+=1
                part=b+u32(s,b+28);parts=[]
                stride=8 if colored else 6
                for block in range(u16(s,b+26)):
                    image=s[part+12];alpha=bool(s[part+11]&4);alpha_image=s[part+14];alpha_ref=s[part+20];cur=part+32;stop=cur+u32(s,part+24)
                    while cur<stop:
                        op=s[cur];cur+=1
                        if op==0:continue
                        count=u16(s,cur);cur+=2
                        refs=[u16(s,cur+j*stride+stride-2) for j in range(count)]
                        color_refs=[u16(s,cur+j*stride+4) for j in range(count)] if colored else []
                        cur+=count*stride
                        ids=[refs[k] for t in triangles(op,count) for k in t]
                        if ids:
                            vertex_colors=colored and ambient
                            if vertex_colors:
                                color_bytes=bytes(v for t in triangles(op,count) for k in t for v in color_data[color_refs[k]*4:color_refs[k]*4+4])
                                assert len(color_bytes)==len(ids)*4
                                color_checksum=byte_hash(color_bytes)
                            else:color_checksum=neutral_hash(len(ids))
                            parts.append((image,alpha,alpha_image,alpha_ref,[x for k in ids for x in uv[k]],vertex_colors,color_checksum))
                    part=stop
                head=b+u32(s,b)
                rigid=s[b+25]==1 and s[b+24]<=1 and u16(s,b+42)<=255 and s[head]==0 and s[head+1]==255
                model.append((colored,rigid,parts))
            expected=[];first=0;yes=no=0;loaded={}
            for w in works:
                if w[39]==255 or u32(w,68)&16 or w[38]!=255:continue
                colored,rigid,parts=model[0 if w[36]==255 else w[36]]
                if not rigid:continue
                palette=0 if w[37]==255 else w[37];tpl=arrays[1][palette];count=u32(s,tpl+4)
                for image,alpha,alpha_image,alpha_ref,uv,vertex_colors,color_checksum in parts:
                    corners=len(uv)//2;resolved=image<count and image<248
                    for image_id in ([image,alpha_image] if alpha else [image]):
                        if image_id<count and image_id<248:
                            loaded.setdefault((palette,image_id,colored),None)
                    alpha_resolved=alpha and resolved and alpha_image<count and alpha_image<248
                    expected.append(((first,corners,palette,image,colored,resolved,alpha,alpha_image,alpha_ref,alpha_resolved,vertex_colors,color_checksum),uv));first+=corners
                    if vertex_colors:
                        color_draws+=1;color_triangles+=corners//3
                    if alpha:
                        if alpha_resolved:alpha_yes+=corners//3
                        else:alpha_no+=corners//3
                    if resolved:yes+=corners//3
                    else:no+=corners//3
            assert words(3)==(len(expected),yes,no),(path,'draw counts')
            for fields,uv in expected:
                assert words(12)==fields,(path,'material mapping')
                assert floats(len(uv))==tuple(uv),(path,'corner UV')
            assert words(1)==(len(loaded),)
            for palette,image_id,colored in loaded:
                tpl=arrays[1][palette];table=tpl+u32(s,tpl+8);head=tpl+u32(s,table+image_id*8)
                h=u16(s,head);w=u16(s,head+2);fmt=u32(s,head+4)
                aw,ah,af,checksum=words(4)
                assert (aw,ah,af)==(w,h,fmt)
                lo,hi,levels=words(3)
                assert (lo,hi)==(s[head+33],s[head+34])
                requested=hi if lo!=hi else 0
                image_start=tpl+u32(s,head+8)
                boundaries=[len(s)]
                for entry in range(u32(s,tpl+4)):
                    ih=tpl+u32(s,table+entry*8)
                    boundaries.extend((ih,tpl+u32(s,ih+8)))
                    ph=u32(s,table+entry*8+4)
                    if ph:
                        ph+=tpl;boundaries.extend((ph,tpl+u32(s,ph+8)))
                image_end=min(v for v in boundaries if v>image_start)
                fw,fh=w,h;position=image_start;available=-1
                ftw,fth,fbs={0:(8,8,32),1:(8,4,32),2:(8,4,32),3:(4,4,32),4:(4,4,32),5:(4,4,32),6:(4,4,64),8:(8,8,32),9:(8,4,32),10:(4,4,32),14:(8,8,32)}[fmt]
                for level in range(requested+1):
                    if position==image_end:break
                    required=((fw+ftw-1)//ftw)*((fh+fth-1)//fth)*fbs
                    if position+required>image_end:
                        assert (image_end-position)%32==0
                        break
                    position+=required
                    available=level;fw,fh=max(fw//2,1),max(fh//2,1)
                assert levels==available
                bounded_chains+=levels<requested
                tex=tpl+u32(s,head+8)
                mw,mh=w,h
                tw,th,bs={0:(8,8,32),1:(8,4,32),2:(8,4,32),3:(4,4,32),4:(4,4,32),5:(4,4,32),6:(4,4,64),8:(8,8,32),9:(8,4,32),10:(4,4,32),14:(8,8,32)}[fmt]
                if levels:mip_images+=1
                for level in range(1,levels+1):
                    tex+=((mw+tw-1)//tw)*((mh+th-1)//th)*bs
                    mw,mh=max(mw//2,1),max(mh//2,1)
                    lw,lh,rgba_hash=words(3)
                    assert (lw,lh)==(mw,mh)
                    assert fmt in (0,1,14),(path,image_id,'new mip format needs reference',fmt)
                    value_hash=2166136261
                    for y in range(mh):
                        for x in range(mw):
                            tile=tex+(y//th*((mw+tw-1)//tw)+x//tw)*bs
                            if fmt in (0,1):
                                k=(y%th)*8+x%8
                                v=s[tile+(k//2 if fmt==0 else k)]
                                if fmt==0:v=(v>>4 if k%2==0 else v&15)*17
                                color=(v,v,v,v)
                            else:
                                block=tile+((y%8)//4*2+(x%8)//4)*8
                                a,b=u16(s,block),u16(s,block+2)
                                def rgb(v):
                                    r,g,b=v>>11,(v>>5)&63,v&31
                                    return ((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2))
                                c,d=rgb(a),rgb(b)
                                selector=(s[block+4+y%4]>>(6-2*(x%4)))&3
                                if selector<2:color=(*(c if selector==0 else d),255)
                                elif a>b:
                                    weights=(5,3) if selector==2 else (3,5)
                                    color=(*( (v*weights[0]+z*weights[1])//8 for v,z in zip(c,d)),255)
                                else:color=(*( (v+z)//2 for v,z in zip(c,d)),255 if selector==2 else 0)
                            for v in color:value_hash=((value_hash^v)*16777619)&0xffffffff
                    assert rgba_hash==value_hash,(path,image_id,level,'stored mip RGBA')
                    mip_levels+=1;mip_pixels+=mw*mh
                if fmt in (0,1):
                    tex=tpl+u32(s,head+8);hash_value=2166136261
                    for y in range(h):
                        for x in range(w):
                            tile_height=8 if fmt==0 else 4
                            tile=(y//tile_height*((w+7)//8)+x//8)*32
                            offset=(y%tile_height)*8+x%8
                            value=s[tex+tile+(offset//2 if fmt==0 else offset)]
                            if fmt==0:value=((value>>4) if offset%2==0 else (value&15))*17
                            hash_value=((hash_value^value)*16777619)&0xffffffff
                    assert checksum==hash_value,(path,image_id,'intensity alpha')
                    intensity_images+=1
            draws+=len(expected);textured+=yes;fallback+=no
        assert cursor==len(actual)
        rooms+=1
        if rooms%20==0:print(rooms,'rooms checked',flush=True)
print(f'{rooms} rooms; {models} models; {uv_pairs} UV pairs; {draws} draws; {textured} base-textured triangles; {fallback} unresolved triangles; {alpha_yes} alpha-tested triangles; {alpha_no} unresolved-alpha triangles; {intensity_images} I4/I8 alpha images: PASS')

print(f'{mip_images} stored mip textures; {mip_levels} extra levels; {mip_pixels} independently checked RGBA pixels; {bounded_chains} bounded chains: PASS')

print(f'{color_entries} RGBA entries; {color_draws} vertex-color draws; {color_triangles} vertex-color triangles: PASS')
