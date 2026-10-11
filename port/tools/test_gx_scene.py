"""Private original SMD/display-list/TPL oracle + actual dual-ABI Vulkan evidence.

No gameplay-success claim. GPU images and original inputs always stay outside Git.
"""
import argparse
import hashlib
import json
import struct
import subprocess
import sys
import time
import uuid
from pathlib import Path
sys.dont_write_bytecode=True

def sha(b): return hashlib.sha256(b).hexdigest()
def texture_level(gxtex,fmt,w,h,raw):
    if fmt!=6:
        size=gxtex.base_size(fmt,w,h)
        return size,gxtex.decode(fmt,w,h,raw[:size]).tobytes()
    # GX RGBA8: 4x4 tiles, separate AR and GB planes, 64 bytes per tile.
    # Independent oracle; native decoding lives in gx_scene.cpp.
    size=((w+3)//4)*((h+3)//4)*64
    if len(raw)<size:raise ValueError('oracle RGBA8 tile boundary')
    pixels=bytearray(w*h*4);at=0
    for y in range(0,h,4):
        for x in range(0,w,4):
            for r in range(4):
                for c in range(4):
                    if x+c>=w or y+r>=h:continue
                    k=at+2*(r*4+c);p=((y+r)*w+x+c)*4
                    pixels[p:p+4]=bytes((raw[k+1],raw[k+32],raw[k+33],raw[k]))
            at+=64
    return size,bytes(pixels)
def run(cmd,timeout=120):
    r=subprocess.run(list(map(str,cmd)),capture_output=True,timeout=timeout)
    if r.returncode: raise RuntimeError(r.stderr.decode(errors='replace')+'\n'+r.stdout.decode(errors='replace'))
    return r.stdout

def sample_curve(axis,fmt,frame,maximum,fcv):
    values=[fcv.to_float(fcv.FCC_FMT[fmt][0],k[0]) for k in axis.keys]
    if frame>=maximum:return values[-1]
    if frame in axis.frames:return values[axis.frames.index(frame)]
    i=next(i for i in range(len(axis.frames)-1) if axis.frames[i]<frame<axis.frames[i+1])
    t=(frame-axis.frames[i])/(axis.frames[i+1]-axis.frames[i]);t2=t*t;t3=t2*t
    kind=fcv.FCC_FMT[fmt][1];a=fcv.to_float(kind,axis.keys[i][2]);b=fcv.to_float(kind,axis.keys[i+1][1])
    return (2*t3-3*t2+1)*values[i]+(-2*t3+3*t2)*values[i+1]+(t3-2*t2+t)*a+(t3-t2)*b

def oracle(root,data,files,room_path='St1/r120.das',event_path='Evd/r120s00.evd',frame=0,requested_cut=None,model='',geometry_only=False):
    sys.path.insert(0,str(root/'tools'))
    from motion import archive,gxtex,fcv
    f=files[room_path];a,_=archive.load_das(data/'blobs'/f['sha256'])
    smd=next(b for tag,b in a.entries if tag==b'SMD\0');smx=next(b for tag,b in a.entries if tag==b'SMX\0')
    def u32(b,p):return struct.unpack_from('>I',b,p)[0]
    primary=smd;common=next((b for tag,b in list(a.entries)[list(a.entries).index((b'SMD\0',smd))+1:] if tag==b'SMD\0'),None)
    ng=u32(smd,16) if smd[1]&1 else 0
    groups=list(struct.unpack_from(f'>{ng}I',smd,20)) if ng else []
    sources=[(-1,smd)];block_hashes=[];active_blocks=[]
    for path,fblock in files.items():
        if path.lower().startswith(room_path[:-4].lower()+'_') and path.lower().endswith('.dat'):
            no=int(path.rsplit('_',1)[1][:-4],16)
            block=next(b for tag,b in archive.parse_body((data/'blobs'/fblock['sha256']).read_bytes()) if tag=='SMD')
            if groups and struct.unpack_from('>H',block,2)[0]!=groups[no]:raise ValueError('original block/group count')
            sources.append((no,block));block_hashes.append(fblock['sha256']);active_blocks.append(no)
    if groups and len(active_blocks)!=len(groups):raise ValueError('original groups absent')
    def entry(source,table,limit,index):
        start=u32(source,table+index*4);offsets=[u32(source,table+4*i) for i in range(u32(source,table)//4)]
        end=min([o for o in offsets if o>start]+[limit-table]);return source[table+start:table+end]
    def palette(source,tid):
        bt,tt,mt=struct.unpack_from('>3I',source,4);return entry(source,tt,mt,tid)
    additional=palette(common,0) if common else b''
    placements=[];n=0
    for block_no,source in sources:
        nlocal=struct.unpack_from('>H',source,2)[0];base=20+4*u32(source,16) if source[1]&1 else 16
        for local in range(nlocal):placements.append((n+local,source,base+72*local,block_no))
        n+=nlocal
    if n!=struct.unpack_from('>H',primary,2)[0]+sum(groups):raise ValueError('original getWorkNum differs')
    textures={};hashes=[];materials=[];part_specular=[];primitive_count=triangles=models=0
    for i,smd,at,block_no in placements:
        bid,tid,mot,work=smd[at+36:at+40]
        if work==255:continue
        flag=u32(smd,at+68)
        if mot!=255:raise ValueError('oracle outside initial scroll FCV scope')
        source=common if flag&16 else smd
        if source is None:raise ValueError('original common table absent')
        bt,tt,mt=struct.unpack_from('>3I',source,4);b=entry(source,bt,tt,bid)
        tpl=bytes.fromhex('0020af30000000000000000c') if flag&16 else palette(smd,tid)
        key=(None if flag&16 else (id(smd),tid),id(common) if common else None)
        if key not in textures:
            base=len(hashes);count=0
            for t in [tpl]+([additional] if additional else []):
                nt,table=struct.unpack_from('>II',t,4);count+=nt
                for k in range(nt):
                    header,clut=struct.unpack_from('>II',t,table+k*8)
                    h,w,fmt,pixel=struct.unpack_from('>HHII',t,header);levels=[]
                    if clut:raise ValueError('independent scroll oracle CLUT unsupported')
                    for level in range(t[header+34]+1):
                        size,rgba=texture_level(gxtex,fmt,w,h,t[pixel:]);levels.append(rgba);pixel+=size;w=max(1,w//2);h=max(1,h//2)
                    hashes.append(sha(b''.join(levels)))
            textures[key]=(base,count)
        base,count=textures[key];part=u32(b,28);np=struct.unpack_from('>H',b,26)[0];cull=1;blend=0;color=0xffffffff;ot=3;flags=0;uv=[0.,0.]
        for k in range(smx[1]):
            p=16+k*144
            if smx[p]==work:
                cull=[1,2,0][smx[p+3]];col=u32(smx,p+12);blend=col&255;color=(col|255) if col&~255 else 0xffffffff;ot=smx[p+2];flags=u32(smx,p+8);uv=list(struct.unpack_from('>2f',smx,p+136));break
        for k in range(np):
            size=u32(b,part+24);cursor=part+32;end=cursor+size;index_count=0
            while cursor<end:
                op=b[cursor];cursor+=1
                if not op:continue
                nv=struct.unpack_from('>H',b,cursor)[0];cursor+=2
                if op==0x80:nt=nv//4*2
                elif op==0x90:nt=nv//3
                elif op==0x98:nt=nv-2
                else:raise ValueError('oracle unsupported FIFO opcode')
                cursor+=8*nv;index_count+=nt*3;primitive_count+=1
            fl=b[part+11];tex=b[part+12];alpha=b[part+14] if fl&4 else tex
            materials.append([i,work,bid,tid,k,base+tex,base+alpha,fl,128 if flags&8 else b[part+20],cull,index_count,blend,color,ot,flags,*uv]);triangles+=index_count//3;part=end
            head=part-size-32;part_specular.append([b[head+13],b[head+15],*b[head+16:head+20],b[head+21],b[head+23]])
        models+=1
    smd=primary
    if geometry_only:
        return dict(models=models,triangles=triangles,primitives=primitive_count,draws=len(materials),textures=len(hashes),texture_hashes=hashes,materials=materials,part_specular=part_specular,smd_sha256=sha(smd),room_sha256=f['sha256'],scroll_work_count=n,active_blocks=active_blocks,block_hashes=block_hashes)
    evd=(data/'blobs'/files[event_path]['sha256']).read_bytes();offset,size=struct.unpack_from('>II',evd,64)
    while offset<struct.unpack_from('>I',evd,64)[0]+size:
        typ=u32(evd,offset);sz=struct.unpack_from('>H',evd,offset+12)[0]
        if typ==6 and (requested_cut is None or struct.unpack_from('>h',evd,offset+8)[0]==requested_cut):camera=evd[offset+16:offset+64].split(b'\0')[0].decode();break
        offset+=sz
    motion=fcv.parse(dict(archive.parse_event(evd))[camera]);tracks={j.parts_no:j for j in motion.joints}
    def first(j):return [sample_curve(a,j.fcc_type,frame,motion.max_frame,fcv) for a in j.axes]
    cp=first(tracks[0])+first(tracks[1])+[first(tracks[2])[1] if 2 in tracks else 0.,first(tracks[3])[1]*180/3.1415927]
    lit=next(b for tag,b in a.entries if tag==b'LIT\0');cut=struct.unpack_from('>H',evd,offset+8)[0]
    light_at=u32(lit,4+4*cut)
    if model:
        import re
        from motion import meshbin
        resources=dict(archive.parse_event(evd));player=archive.load_das(data/'blobs'/files['em/pl00.drs']['sha256'])[0]
        refs=re.findall(r'EvtMgr\.SetBin\("([^"]+)",\s*PL_ARC_PTR\(pG->pPlayer,\s*([^)]+)\)',(root/'src/game/event.cpp').read_text())
        for name,index in refs:resources.setdefault(name,player.entries[int(index,0)-4][1])
        at=struct.unpack_from('>I',evd,64)[0];registrations=[]
        while at<struct.unpack_from('>I',evd,64)[0]+size:
            typ=u32(evd,at);length=struct.unpack_from('>H',evd,at+12)[0]
            name=evd[at+16:at+28].split(b'\0')[0].decode(errors='replace')
            parent=evd[at+28:at+40].split(b'\0')[0].decode(errors='replace') if typ==4 else name
            if typ in (3,4) and (name==model or parent==model):
                bp,tp=(28,76) if typ==3 else (40,88)
                registrations.append((name,evd[at+bp:at+bp+48].split(b'\0')[0].decode(),evd[at+tp:at+tp+48].split(b'\0')[0].decode()))
            at+=length
        if not registrations:raise ValueError('original event model reference missing')
        bindings={}
        for name,bin_name,tpl_name in registrations:
            mesh=meshbin.parse(resources[bin_name]);tpl=resources[tpl_name]
            if tpl_name not in bindings:
                base=len(hashes);count,table=struct.unpack_from('>II',tpl,4);bindings[tpl_name]=(base,count)
                for k in range(count):
                    header,clut=struct.unpack_from('>II',tpl,table+k*8)
                    if clut:raise ValueError('event oracle CLUT not implemented')
                    h,w,fmt,pixel=struct.unpack_from('>HHII',tpl,header);levels=[]
                    for level in range(tpl[header+34]+1):
                        size_level=gxtex.base_size(fmt,w,h);levels.append(gxtex.decode(fmt,w,h,tpl[pixel:pixel+size_level]).tobytes());pixel+=size_level;w=max(1,w//2);h=max(1,h//2)
                    hashes.append(sha(b''.join(levels)))
            base,count=bindings[tpl_name]
            for k,part in enumerate(mesh.parts):
                indices=len(part.triangles())*3;primitive_count+=len(part.prims);triangles+=indices//3
                materials.append([n,n,0,0,k,base+part.tex_id,base+(part.alpha_tex if part.flags&4 else part.tex_id),part.flags,part.alpha_ref,1,indices,0,0xffffffff,0,0,0.,0.])
                part_specular.append([part.bump_tex,part.spec_tex,*part.spec_rgb,part.spec_type,part.spec_pow,part.spec_tex_org])
            models+=1
    if any(row[7]&0x13 for row in materials):
        core=archive.load_das(data/'blobs'/files['etc/core.das']['sha256'])[0]
        for slot in (4,17,18,16):
            tag,tpl=core.entries[slot-4]
            if tag!=b'TPL\0':raise ValueError('original core texture tag')
            count,table=struct.unpack_from('>II',tpl,4)
            for k in range(count):
                header,clut=struct.unpack_from('>II',tpl,table+k*8);h,w,fmt,pixel=struct.unpack_from('>HHII',tpl,header)
                if clut:raise ValueError('core oracle CLUT unsupported')
                hashes.append(sha(gxtex.decode(fmt,w,h,tpl[pixel:pixel+gxtex.base_size(fmt,w,h)]).tobytes()))
    return dict(models=models,triangles=triangles,primitives=primitive_count,draws=len(materials),textures=len(hashes),texture_hashes=hashes,materials=materials,part_specular=part_specular,camera=camera,camera_point=cp,smd_sha256=sha(smd),room_sha256=f['sha256'],lit_sha256=sha(lit),lit_cut=cut,selected_light_records=u32(lit,light_at+4),lighting_calls=len(materials),ot_execution=list(range(22)),scroll_work_count=n,active_blocks=active_blocks,block_hashes=block_hashes)

def verify(actual,expected,geometry_only=False):
    for k,v in expected.items():
        if k=='part_specular':
            if [p['specular'] for p in actual['material_programs']]!=v:raise ValueError('original part specular/bump header mismatch')
        elif k=='camera_point':
            if any(abs(a-b)>max(.01,abs(b)*2e-6) for a,b in zip(actual[k],v)):raise ValueError('original camera track mismatch')
        elif k=='materials':
            if len(actual[k])!=len(v):raise ValueError('material count')
            for a,b in zip(actual[k],v):
                if a[:15]!=b[:15] or any(abs(x-y)>1e-9 for x,y in zip(a[15:],b[15:])):raise ValueError('GX material identity/state mismatch')
        elif actual[k]!=v:raise ValueError('GX oracle mismatch: '+k)
    if geometry_only:
        if actual['triangles']==0 or actual['kind']!='original_SMD_registration_geometry_only' or actual['material_lighting_ready'] or actual['render_order'] or actual['camera']:
            raise ValueError('SMD creation must not claim a camera/light/GPU frame')
        if 'BlockCreate' in actual['scroll_functions'] and 'BlockDestroy/BlockCreate/BlockDestroy' not in actual['scroll_functions']:
            raise ValueError('original block lifetime evidence absent')
        return
    if actual['triangles']==0 or actual['kind']!='isolated_original_SMD_GX_OT_TEV_LIT_cut':raise ValueError('no original scene')
    order=actual['render_order'];draws=actual['materials']
    if not order or len(order)!=len(set(order)) or any(i<0 or i>=len(draws) for i in order):raise ValueError('OT draw ownership')
    # Original ModelTrans table/bucket mapping; ExecOt far-first and LIFO.
    previous=None
    for i in order:
        row=draws[i];key={0:(13,0),1:(17,0),2:(11,4),3:(11,2),4:(11,1),5:(16,0)}[row[13]]
        sequence=(key[0],-key[1],-row[0],i)
        if previous is not None and sequence<previous:raise ValueError('original OT bucket/LIFO/part order')
        previous=sequence
    if len(actual['tev_states'])!=len(draws):raise ValueError('original TEV pairing')
    verify_material_programs(actual)

def verify_material_programs(actual):
    """Independent original part/state checks, not a GPU image-quality oracle."""
    import numpy as np
    sb,sc,ib,ic,gb,gc=actual['core_texture_bindings']
    view=np.eye(4)
    # Use the captured original view matrix for the isolated adapter comparison;
    # curve/orientation fidelity is covered by the camera tests and SDK bodies.
    camera_matrix=actual.get('view_matrix')
    if camera_matrix is None:raise ValueError('original view matrix evidence missing')
    view[:3]=np.array(camera_matrix).reshape(3,4)
    for row,material,p in zip(actual['materials'],actual['tev_states'],actual['material_programs']):
        flags=row[7];bump,spec,r,g,b,kind,decay,external=p['specular'];program=p['program']
        spec_active=bool(flags&0x13);variant=bool(flags&0x80 and flags&3)
        stages=2+(3 if variant else (2 if kind==0 else 1) if spec_active else 0)+int(bool(flags&4))
        if material['stages']!=stages:raise ValueError('original shaderSetup stage count')
        ind=bool(variant or flags&1)
        if p['indirect_meta'][0]!=int(ind):raise ValueError('original bump indirect count')
        if spec_active:
            if variant:
                if p['textures'][1:3]!=[p['binding'][0]+bump]*2:raise ValueError('original bump warp texture binding')
                if p['textures'][3]!=ib or program[2][5]!=[1,0,2,7]:raise ValueError('original specularSetup2 warp state')
                if p['indirect_matrices'][2:4]!=[[0,-20,0,-1],[20,0,0,-1]]:raise ValueError('original warp coefficient quantization')
            else:
                selected=(0 if spec==255 else spec)
                if not flags&16 and (selected>=sc or p['textures'][1]!=sb+selected):raise ValueError('original Specular[] binding')
                if flags&16 and p['textures'][1]!=p['binding'][0]+external:raise ValueError('original model specular texture binding')
                if flags&1 and p['textures'][2]!=p['binding'][0]+bump:raise ValueError('original bump texture binding')
                if program[1][0]!=([15,8,10,15] if kind==0 else [15,9,0,0]):raise ValueError('original specular TEV inputs')
                if flags&1 and program[1][5]!=[1,0,1,7]:raise ValueError('original bump XYZ state')
                model=np.eye(4);model[:3]=np.array(p['model_matrix']).reshape(3,4)
                scale=np.array(p['model_scale']);adjust=np.diag([*1/scale,1.])@model if any(scale!=1) else model
                normal=np.linalg.inv(view@adjust).T[:3,:4]
                power=(decay*.01 if decay else .5)*(.5 if p['model_flags']&0x20000000 else 1)
                tex=np.diag([power,-power,0.,1.]);translate=np.eye(4);translate[:3,3]=[.5,.5,1.]
                expected=(translate@tex@np.vstack([normal,[0,0,0,1]]))[:3]
                gen=p['texgens'][1]
                if gen[0]!=[0,1,0,0] or not np.allclose(np.array(gen[1:]),expected,rtol=3e-6,atol=2e-6):raise ValueError('original normal specular texture matrix')
                if flags&1:
                    matrix=normal[:2,:3]*.4;matrix[1,1]*=-1
                    quant=np.trunc(matrix*1024).astype(int);quant=((quant+1024)%2048)-1024
                    got=np.array(p['indirect_matrices'][:2])[:,:3]
                    if np.max(np.abs(got-quant))>1:raise ValueError('original indirect S0.10 matrix')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--data',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--host-exe',type=Path);p.add_argument('--adb',type=Path);p.add_argument('--serial',default='dbdcacc');p.add_argument('--repeat',type=int,default=2)
    p.add_argument('--room',type=lambda s:int(s,16),default=0x120);p.add_argument('--event',default='Evd/r120s00.evd')
    p.add_argument('--model',default='');p.add_argument('--cut',type=int);p.add_argument('--frame',type=int,default=0)
    p.add_argument('--geometry-reference',type=Path,help='Compare native vertex/index output with a private SMD output directory')
    p.add_argument('--smd-only',action='store_true',help='Actual SMD/BlockCreate registration and release; no rendering success')
    p.add_argument('--baseline-output',type=Path,help='GPU evidence for the identical room/camera/frame without the selected event model')
    args=p.parse_args();root=Path(__file__).resolve().parents[2];data=args.data.resolve();output=args.output.resolve()
    if output==data or output.is_relative_to(root) or not 1<=args.repeat<=10:raise ValueError('separate private output / repeat1..10')
    room_path=f'St{args.room>>8:x}/r{args.room:03x}.das'
    required={room_path} if args.smd_only else {room_path,args.event,'etc/core.das'}
    if args.model:required.add('em/pl00.drs')
    manifest=json.loads((data/'manifest.json').read_text());required.update(f['path'] for f in manifest['files'] if f['disc_number']==0 and f['path'].lower().startswith(room_path[:-4].lower()+'_') and f['path'].lower().endswith('.dat'))
    output.mkdir(parents=True,exist_ok=True);files={f['path']:f for f in manifest['files'] if f['disc_number']==0 and f['path'] in required}
    if set(files)!=required:raise ValueError('required original room/event/core/player references absent')
    catalog=output/'catalog.tsv';catalog.write_text('RE4-IMPORT-CATALOG-1\n'+''.join(f"0\t{f['index']}\t{f['size']}\t{f['path']}\t{f['sha256']}\n" for f in files.values()),encoding='utf-8',newline='\n')
    for f in files.values():
        if sha((data/'blobs'/f['sha256']).read_bytes())!=f['sha256']:raise ValueError('original CAS integrity')
    expected=oracle(root,data,files,room_path,args.event,args.frame,args.cut,args.model,args.smd_only);(output/'oracle.json').write_text(json.dumps(expected,indent=2)+'\n')
    scene_args=[f'{args.room:x}',args.event,args.frame]
    if args.model or args.cut is not None:scene_args += [args.model or '-',args.cut if args.cut is not None else 4294967295]
    if args.smd_only:scene_args=[f'{args.room:x}']
    command='scroll' if args.smd_only else 'scene'
    semantic=[];geometry_samples=[]
    if args.host_exe:
        results=[]
        for i in range(args.repeat):
            actual=json.loads(run([args.host_exe,command,data,catalog,0,*scene_args,*([output/'geometry-host.bin'] if args.smd_only else [])]));verify(actual,expected,args.smd_only);results.append(actual)
            if args.smd_only:geometry_samples.append((output/'geometry-host.bin').read_bytes())
            semantic.append((actual['texture_hashes'],actual['materials']) if args.smd_only else (actual['material_state_sha256'],actual['render_order']))
        (output/f"host-{results[0]['pointer_bits']}.json").write_text(json.dumps(results,indent=2)+'\n');print('PASS host',results[0]['pointer_bits'],'original SMD creation/release + primitive/material/TPL oracle' if args.smd_only else 'original scene + primitive/material/camera/TPL oracle',flush=True)
    if args.adb:
        def adb(*a):return run([args.adb,'-s',args.serial,*a])
        remote='/data/local/tmp/re4-phase5a';adb('shell','mkdir','-p',remote+'/blobs');adb('push',catalog,remote+'/catalog.tsv')
        for f in files.values():adb('push',data/'blobs'/f['sha256'],remote+'/blobs/'+f['sha256'])
        for abi,bits in [('armeabi-v7a',32),('arm64-v8a',64)]:
            exe=root/f'port/build/android-{abi}/re4_gx_tool';target=remote+'/gx-'+abi;adb('push',exe,target);adb('shell','chmod','755',target);results=[]
            adapters=json.loads(adb('shell',target,'self-test'))
            if adapters['pointer_bits']!=bits or adapters['failed']!=0:raise ValueError('GX adapter test ABI/failure')
            (output/f'adapters-{abi}.json').write_text(json.dumps(adapters,indent=2)+'\n')
            for i in range(args.repeat):
                actual=json.loads(adb('shell',target,command,remote,remote+'/catalog.tsv',0,*scene_args,*([remote+'/geometry.bin'] if args.smd_only else [])));verify(actual,expected,args.smd_only)
                if args.smd_only:
                    raw_geometry=adb('exec-out','cat',remote+'/geometry.bin');geometry_samples.append(raw_geometry);(output/f'geometry-{abi}.bin').write_bytes(raw_geometry)
                if actual['pointer_bits']!=bits:raise ValueError('actual process ABI mismatch')
                semantic.append((actual['texture_hashes'],actual['materials']) if args.smd_only else (actual['material_state_sha256'],actual['render_order']))
                results.append(actual)
            (output/f'android-{abi}.json').write_text(json.dumps(results,indent=2)+'\n')
            if args.smd_only:
                print('PASS original SMD creation/release',abi,'repeated room loads and block recreation',flush=True);continue
            apk=root/'port/android/app/build/outputs/apk/debug/app-debug.apk';adb('install','-r','--abi',abi,apk)
            package=adb('shell','dumpsys','package','org.re4.port').decode()
            if 'primaryCpuAbi='+abi not in package:raise ValueError('APK process ABI')
            adb('shell','run-as','org.re4.port','mkdir','-p','files/phase5a/blobs');adb('shell','run-as','org.re4.port','cp',remote+'/catalog.tsv','files/phase5a/catalog.tsv')
            for f in files.values():adb('shell','run-as','org.re4.port','cp',remote+'/blobs/'+f['sha256'],'files/phase5a/blobs/'+f['sha256'])
            adb('shell','am','force-stop','org.re4.port');process=None
            for repeat in range(args.repeat):
                # Never pair a previous Activity's status with the new owner's image.
                # Each recreation gets its own evidence files; PID reuse still proves teardown.
                session='phase5a-'+uuid.uuid4().hex
                report_file=f'files/{session}-graphics.json';image_file=f'files/{session}-gpu.rgba'
                intents=['--ei','frame',args.frame]
                if args.model:intents+=['--es','model',args.model]
                if args.cut is not None:intents+=['--ei','cut',args.cut]
                adb('shell','input','keyevent','KEYCODE_WAKEUP');adb('shell','am','start','-n','org.re4.port/.GeometryActivity','--es','root','/data/user/0/org.re4.port/files/phase5a','--es','catalog','/data/user/0/org.re4.port/files/phase5a/catalog.tsv','--ei','room',args.room,'--es','event',args.event,'--es','capture',session,*intents)
                deadline=time.monotonic()+60
                while time.monotonic()<deadline:
                    time.sleep(.5)
                    try:gpu=json.loads(adb('shell','run-as','org.re4.port','cat',report_file))
                    except (RuntimeError,json.JSONDecodeError):continue
                    if gpu.get('evidence_session')!=session:raise ValueError('stale Activity evidence')
                    if gpu['geometry_frames']>=12:break
                else:raise RuntimeError('no completed geometry frames: '+adb('logcat','-d','-t','80').decode(errors='replace'))
                verify(gpu['scene'],expected)
                if args.model and (gpu['scene']['skinned_models']<1 or gpu['scene']['event_model']!=args.model or not any(gpu['scene']['materials'][i][0]==expected['materials'][-1][0] for i in gpu['scene']['render_order'])):raise ValueError('requested original weighted pose was not rendered')
                semantic.append((gpu['scene']['material_state_sha256'],gpu['scene']['render_order']))
                if gpu.get('validation_errors',0):raise ValueError('Vulkan validation errors')
                pid=adb('shell','pidof','org.re4.port').decode().strip()
                if process is not None and process!=pid:raise ValueError('Activity recreation did not reuse the process; native teardown not proven')
                process=pid;gpu['process_pid']=pid
                if gpu['pointer_bits']!=bits or gpu['non_clear_pixels']<100 or gpu['graphics_domains']!=1:raise ValueError('GPU geometry/lifetime evidence')
                prefix=output/f'gpu-{abi}-{repeat+1}';prefix.with_suffix('.json').write_text(json.dumps(gpu,indent=2)+'\n')
                rgba=adb('exec-out','run-as','org.re4.port','cat',image_file)
                if len(rgba)!=512*448*4 or sha(rgba)!=gpu['rgba_sha256']:
                    prefix.with_suffix('.failed-readback.bin').write_bytes(rgba)
                    raise ValueError(f"GPU readback: bytes={len(rgba)}, hash={sha(rgba)}, report={gpu['rgba_sha256']}")
                prefix.with_suffix('.rgba').write_bytes(rgba)
                from PIL import Image
                Image.frombytes('RGBA',(512,448),rgba).save(prefix.with_suffix('.png'))
                if args.baseline_output:
                    baseline=args.baseline_output/f'gpu-{abi}-1';reference=json.loads(baseline.with_suffix('.json').read_text())
                    for field in ['room_sha256','camera','camera_point','frame','vp']:
                        if reference['scene'][field]!=gpu['scene'][field]:raise ValueError('pixel baseline room/camera/frame mismatch')
                    if reference['scene']['event_model'] or not args.model:raise ValueError('pixel baseline must omit only the selected event model')
                    before=baseline.with_suffix('.rgba').read_bytes()
                    if len(before)!=len(rgba):raise ValueError('pixel baseline extent')
                    changed=[i//4 for i in range(0,len(rgba),4) if before[i:i+4]!=rgba[i:i+4]]
                    if not changed:raise ValueError('weighted event model produced no visible fragments')
                    gpu['event_model_changed_pixels']=len(changed);gpu['event_model_pixel_bounds']=[min(i%512 for i in changed),min(i//512 for i in changed),max(i%512 for i in changed),max(i//512 for i in changed)]
                    prefix.with_suffix('.json').write_text(json.dumps(gpu,indent=2)+'\n')
                    print('PASS weighted model visible pixels',abi,len(changed),flush=True)
                (output/f'display-{abi}-{repeat+1}.png').write_bytes(adb('exec-out','screencap','-p'))
                print('PASS GPU',abi,repeat+1,gpu['geometry_frames'],'frames',gpu['non_clear_pixels'],'pixels',flush=True)
                adb('shell','input','keyevent','KEYCODE_BACK');time.sleep(.7)
            adb('shell','am','force-stop','org.re4.port')
        images=[f.read_bytes() for f in sorted(output.glob('gpu-*.rgba'))]
        if not args.smd_only and any(b!=images[0] for b in images):raise ValueError('repeated/dual-ABI GPU output differs')
        if not args.smd_only:print('PASS repeated ARM32/ARM64 GPU readbacks are byte-identical',flush=True)
    if args.smd_only:
        import numpy as np
        if args.geometry_reference:
            reference=next(args.geometry_reference.glob('geometry-*.bin'),None)
            if reference is None:raise ValueError('native geometry reference absent')
            geometry_samples.insert(0,reference.read_bytes())
        nv=actual['vertices'];ni=actual['triangles']*3;extent=nv*48+ni*4
        maximum=[0.,0.,0.]
        ref=geometry_samples[0]
        if len(ref)!=extent:raise ValueError('native geometry binary extent')
        dtype=np.dtype([('position','<f4',3),('normal','<f4',3),('uv','<f4',2),('color','u1',4),('raw_normal','<f4',3)])
        a=np.frombuffer(ref[:nv*48],dtype=dtype)
        for b in geometry_samples[1:]:
            if len(b)!=extent or b[nv*48:]!=ref[nv*48:]:raise ValueError('native index/extent ABI mismatch')
            v=np.frombuffer(b[:nv*48],dtype=dtype)
            if not np.array_equal(a['color'],v['color']) or not np.array_equal(a['uv'],v['uv']) or not np.array_equal(a['raw_normal'],v['raw_normal']):raise ValueError('native source attribute ABI mismatch')
            dp=float(np.max(np.abs(a['position']-v['position'])));dn=float(np.max(np.abs(a['normal']-v['normal'])))
            if not np.isfinite(dp+dn) or dp>.05 or dn>2e-6:raise ValueError(f'native SDK transform ABI deviation: {dp}, {dn}')
            maximum[0]=max(maximum[0],dp);maximum[1]=max(maximum[1],dn)
        comparison=dict(samples=len(geometry_samples),vertices=nv,indices=ni,max_position_delta=maximum[0],max_normal_delta=maximum[1],indices_source_colors_uv_raw_normals='byte-identical',scope='native SDK transform comparison, not PPC or visual fidelity')
        (output/'geometry-comparison.json').write_text(json.dumps(comparison,indent=2)+'\n')
        print('PASS native geometry ABI comparison',comparison,flush=True)
    if any(state!=semantic[0] for state in semantic):raise ValueError('OT/TEV/light states differ across runs or ABIs')
    print('PASS repeated native/ABI geometry, resource IDs and texture equality' if args.smd_only else 'PASS repeated native/ABI OT + TEV + light state equality',flush=True)

if __name__=='__main__':main()
