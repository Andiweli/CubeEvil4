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
def run(cmd,timeout=120):
    r=subprocess.run(list(map(str,cmd)),capture_output=True,timeout=timeout)
    if r.returncode: raise RuntimeError(r.stderr.decode(errors='replace')+'\n'+r.stdout.decode(errors='replace'))
    return r.stdout

def oracle(root,data,files,room_path='St1/r120.das',event_path='Evd/r120s00.evd'):
    sys.path.insert(0,str(root/'tools'))
    from motion import archive,gxtex,fcv
    f=files[room_path];a,_=archive.load_das(data/'blobs'/f['sha256'])
    smd=next(b for tag,b in a.entries if tag==b'SMD\0');smx=next(b for tag,b in a.entries if tag==b'SMX\0')
    def u32(b,p):return struct.unpack_from('>I',b,p)[0]
    n=struct.unpack_from('>H',smd,2)[0];bt,tt,mt=struct.unpack_from('>3I',smd,4)
    def entry(table,limit,index):
        start=u32(smd,table+index*4);offsets=[u32(smd,table+4*i) for i in range(u32(smd,table)//4)]
        end=min([o for o in offsets if o>start]+[limit-table]);return smd[table+start:table+end]
    textures={};hashes=[];materials=[];primitive_count=triangles=models=0
    for i in range(n):
        at=16+72*i;bid,tid,mot,work=smd[at+36:at+40]
        if work==255:continue
        if u32(smd,at+68)&16 or mot!=255:raise ValueError('oracle outside static local SMD scope')
        b=entry(bt,tt,bid);tpl=entry(tt,mt,tid)
        if tid not in textures:
            count,table=struct.unpack_from('>II',tpl,4);textures[tid]=(len(hashes),count)
            for k in range(count):
                header,clut=struct.unpack_from('>II',tpl,table+k*8)
                if clut:raise ValueError('r120 oracle unexpected CLUT')
                h,w,fmt,pixel=struct.unpack_from('>HHII',tpl,header);levels=[]
                for level in range(tpl[header+34]+1):
                    size=gxtex.base_size(fmt,w,h);levels.append(gxtex.decode(fmt,w,h,tpl[pixel:pixel+size]).tobytes());pixel+=size;w=max(1,w//2);h=max(1,h//2)
                hashes.append(sha(b''.join(levels)))
        base,count=textures[tid];part=u32(b,28);np=struct.unpack_from('>H',b,26)[0];cull=1;blend=0;color=0xffffffff;ot=3;flags=0;uv=[0.,0.]
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
        models+=1
    evd=(data/'blobs'/files[event_path]['sha256']).read_bytes();offset,size=struct.unpack_from('>II',evd,64)
    while offset<struct.unpack_from('>I',evd,64)[0]+size:
        typ=u32(evd,offset);sz=struct.unpack_from('>H',evd,offset+12)[0]
        if typ==6:camera=evd[offset+16:offset+64].split(b'\0')[0].decode();break
        offset+=sz
    motion=fcv.parse(dict(archive.parse_event(evd))[camera]);tracks={j.parts_no:j for j in motion.joints}
    def first(j):return [fcv.to_float(fcv.FCC_FMT[j.fcc_type][0],a.keys[0][0]) for a in j.axes]
    cp=first(tracks[0])+first(tracks[1])+[first(tracks[2])[1] if 2 in tracks else 0.,first(tracks[3])[1]*180/3.1415927]
    lit=next(b for tag,b in a.entries if tag==b'LIT\0');cut=struct.unpack_from('>H',evd,offset+8)[0]
    light_at=u32(lit,4+4*cut)
    return dict(models=models,triangles=triangles,primitives=primitive_count,draws=len(materials),textures=len(hashes),texture_hashes=hashes,materials=materials,camera=camera,camera_point=cp,smd_sha256=sha(smd),room_sha256=f['sha256'],lit_sha256=sha(lit),lit_cut=cut,selected_light_records=u32(lit,light_at+4),lighting_calls=len(materials),ot_execution=list(range(22)))

def verify(actual,expected):
    for k,v in expected.items():
        if k=='camera_point':
            if any(abs(a-b)>max(1e-5,abs(b)*1e-6) for a,b in zip(actual[k],v)):raise ValueError('original camera track mismatch')
        elif k=='materials':
            if len(actual[k])!=len(v):raise ValueError('material count')
            for a,b in zip(actual[k],v):
                if a[:15]!=b[:15] or any(abs(x-y)>1e-9 for x,y in zip(a[15:],b[15:])):raise ValueError('GX material identity/state mismatch')
        elif actual[k]!=v:raise ValueError('GX oracle mismatch: '+k)
    if actual['triangles']==0 or actual['kind']!='isolated_original_SMD_GX_OT_TEV_LIT_cut':raise ValueError('no original scene')
    order=actual['render_order'];draws=actual['materials']
    if not order or len(order)!=len(set(order)) or any(i<0 or i>=len(draws) for i in order):raise ValueError('OT draw ownership')
    # Original ModelTrans table/bucket mapping; ExecOt far-first and LIFO.
    previous=None
    for i in order:
        row=draws[i];key={2:(11,4),3:(11,2),4:(11,1),5:(16,0)}[row[13]]
        sequence=(key[0],-key[1],-row[0],row[4])
        if previous is not None and sequence<previous:raise ValueError('original OT bucket/LIFO/part order')
        previous=sequence
    if len(actual['tev_states'])!=len(draws):raise ValueError('original TEV pairing')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--data',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--host-exe',type=Path);p.add_argument('--adb',type=Path);p.add_argument('--serial',default='dbdcacc');p.add_argument('--repeat',type=int,default=2)
    p.add_argument('--room',type=lambda s:int(s,16),default=0x120);p.add_argument('--event',default='Evd/r120s00.evd')
    args=p.parse_args();root=Path(__file__).resolve().parents[2];data=args.data.resolve();output=args.output.resolve()
    if output==data or output.is_relative_to(root) or not 1<=args.repeat<=10:raise ValueError('separate private output / repeat1..10')
    room_path=f'St{args.room>>8:x}/r{args.room:03x}.das'
    output.mkdir(parents=True,exist_ok=True);manifest=json.loads((data/'manifest.json').read_text());files={f['path']:f for f in manifest['files'] if f['disc_number']==0 and f['path'] in [room_path,args.event,'etc/core.das']}
    if set(files)!={room_path,args.event,'etc/core.das'}:raise ValueError('required original room/event/core references absent')
    catalog=output/'catalog.tsv';catalog.write_text('RE4-IMPORT-CATALOG-1\n'+''.join(f"0\t{f['index']}\t{f['size']}\t{f['path']}\t{f['sha256']}\n" for f in files.values()),encoding='utf-8',newline='\n')
    for f in files.values():
        if sha((data/'blobs'/f['sha256']).read_bytes())!=f['sha256']:raise ValueError('original CAS integrity')
    expected=oracle(root,data,files,room_path,args.event);(output/'oracle.json').write_text(json.dumps(expected,indent=2)+'\n')
    semantic=[]
    if args.host_exe:
        results=[]
        for i in range(args.repeat):
            actual=json.loads(run([args.host_exe,'scene',data,catalog,0,f'{args.room:x}',args.event,0]));verify(actual,expected);results.append(actual)
            semantic.append((actual['material_state_sha256'],actual['render_order']))
        (output/f"host-{results[0]['pointer_bits']}.json").write_text(json.dumps(results,indent=2)+'\n');print('PASS host',results[0]['pointer_bits'],'original scene, independent primitive/material/camera/TPL mip oracle',flush=True)
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
                actual=json.loads(adb('shell',target,'scene',remote,remote+'/catalog.tsv',0,f'{args.room:x}',args.event,0));verify(actual,expected)
                if actual['pointer_bits']!=bits:raise ValueError('actual process ABI mismatch')
                semantic.append((actual['material_state_sha256'],actual['render_order']))
                results.append(actual)
            (output/f'android-{abi}.json').write_text(json.dumps(results,indent=2)+'\n')
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
                adb('shell','input','keyevent','KEYCODE_WAKEUP');adb('shell','am','start','-n','org.re4.port/.GeometryActivity','--es','root','/data/user/0/org.re4.port/files/phase5a','--es','catalog','/data/user/0/org.re4.port/files/phase5a/catalog.tsv','--ei','room',args.room,'--es','event',args.event,'--es','capture',session)
                deadline=time.monotonic()+60
                while time.monotonic()<deadline:
                    time.sleep(.5)
                    try:gpu=json.loads(adb('shell','run-as','org.re4.port','cat',report_file))
                    except (RuntimeError,json.JSONDecodeError):continue
                    if gpu.get('evidence_session')!=session:raise ValueError('stale Activity evidence')
                    if gpu['geometry_frames']>=12:break
                else:raise RuntimeError('no completed geometry frames: '+adb('logcat','-d','-t','80').decode(errors='replace'))
                verify(gpu['scene'],expected)
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
                (output/f'display-{abi}-{repeat+1}.png').write_bytes(adb('exec-out','screencap','-p'))
                print('PASS GPU',abi,repeat+1,gpu['geometry_frames'],'frames',gpu['non_clear_pixels'],'pixels',flush=True)
                adb('shell','input','keyevent','KEYCODE_BACK');time.sleep(.7)
            adb('shell','am','force-stop','org.re4.port')
        images=[f.read_bytes() for f in sorted(output.glob('gpu-*.rgba'))]
        if any(b!=images[0] for b in images):raise ValueError('repeated/dual-ABI GPU output differs')
        print('PASS repeated ARM32/ARM64 GPU readbacks are byte-identical',flush=True)
    if any(state!=semantic[0] for state in semantic):raise ValueError('OT/TEV/light states differ across runs or ABIs')
    print('PASS repeated native/ABI OT + TEV + light state equality',flush=True)

if __name__=='__main__':main()
