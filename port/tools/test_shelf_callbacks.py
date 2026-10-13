"""Real G4BE08 shelf callbacks; GPU is explicitly texture-only diagnostic."""
import argparse,hashlib,json,struct,subprocess,time,uuid
from pathlib import Path
from retroid_adb import hardware_sequence

def run(command,timeout=240):
    p=subprocess.run(list(map(str,command)),capture_output=True,timeout=timeout)
    if p.returncode:raise RuntimeError(f'{command}: {p.stderr.decode(errors="replace")} {p.stdout.decode(errors="replace")}')
    return p.stdout

def f32(x):return struct.unpack('f',struct.pack('f',x))[0]
def sha(b):return hashlib.sha256(b).hexdigest()

def verify_callback(a,room,index,bits):
    if a['pointer_bits']!=bits or a['room']!=room or a['selection']!=index:raise ValueError('callback identity/ABI')
    if [m['work_id'] for m in a['models']]!=({0x103:[[0x57,0x58],[0x59,0x5a],[0x5b,0x5c]],0x106:[[0x29,0x2a],[0x2b,0x2c]]}[room][index]):raise ValueError('original SMD identity')
    for m in a['models']:
        if len(m['bin_sha256'])!=64 or len(m['tpl_sha256'])!=64 or m['parts']<1 or (a['visits']>0 and not m['flags']&0x20):raise ValueError('original model/parts/activation')
    if a['already_opened']:
        if a['sleeps'] or a['sound_requests'] or not a['returned']:raise ValueError('original opened branch')
        if any(abs(x-y)>1e-7 for x,y in zip(a['angles'],[-f32(1.9198622),f32(1.9198622)])):raise ValueError('opened angles')
    else:
        values=list(map(f32,a['initial_angles']))
        for sample in a['sleeps']:
            values=[f32(values[0]+f32(-.063995406)),f32(values[1]+f32(.063995406))]
            if any(abs(x-y)>1e-7 for x,y in zip(values,sample)):raise ValueError('original per-Sleep float accumulation')
        if any(abs(x-y)>1e-7 for x,y in zip(values,a['angles'])):raise ValueError('last resumed pose')
        if a['sound_requests']!=([[6,0x1a]] if a['visits'] else []):raise ValueError('original sound request')
        if a['returned'] and len(a['sleeps'])!=30:raise ValueError('full original callback sleep count')

def verify(result,room,index,bits):
    if result['checks']<16:raise ValueError('lifetime/scheduler checks absent')
    for branch in ['animated','opened','cancelled']:verify_callback(result[branch],room,index,bits)
    if result['cancelled']['returned'] or not result['cancelled']['cancelled'] or len(result['cancelled']['sleeps'])!=7:raise ValueError('cancelled callback resumed')
    if not result['animated']['returned'] or result['before']['geometry_sha256']==result['after']['geometry_sha256']:raise ValueError('no real native geometry change')

def comparable(value):
    if isinstance(value,dict):return {k:comparable(v) for k,v in value.items() if k!='pointer_bits'}
    if isinstance(value,list):return [comparable(v) for v in value]
    return value

@hardware_sequence()
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--data',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--host-exe',type=Path);p.add_argument('--adb',type=Path);p.add_argument('--serial',default='dbdcacc');p.add_argument('--repeat',type=int,default=2)
    args=p.parse_args();root=Path(__file__).resolve().parents[2];data=args.data.resolve();out=args.output.resolve()
    if out==data or out.is_relative_to(root) or not 1<=args.repeat<=4:raise ValueError('private evidence path / repeat')
    out.mkdir(parents=True,exist_ok=True)
    manifest=json.loads((data/'manifest.json').read_text());files=[f for f in manifest['files'] if f['disc_number']==0 and (f['path'] in ['St1/r103.das','St1/r106.das'] or any(f['path'].lower().startswith(f'st1/r{r}_') for r in ['103','106']))]
    if not {'St1/r103.das','St1/r106.das'}.issubset({f['path'] for f in files}):raise ValueError('matching original rooms missing')
    catalog=out/'catalog.tsv';catalog.write_text('RE4-IMPORT-CATALOG-1\n'+''.join(f"0\t{f['index']}\t{f['size']}\t{f['path']}\t{f['sha256']}\n" for f in files),encoding='utf-8',newline='\n')
    for f in files:
        if sha((data/'blobs'/f['sha256']).read_bytes())!=f['sha256']:raise ValueError('CAS integrity')
    (out/'sources.json').write_text(json.dumps(files,indent=2)+'\n')
    cases=[(0x103,i) for i in range(3)]+[(0x106,i) for i in range(2)];semantics=[]
    if args.host_exe:
        results=[]
        for repeat in range(args.repeat):
            for room,index in cases:
                a=json.loads(run([args.host_exe,'shelf',data,catalog,0,f'{room:x}',index]));bits=a['animated']['pointer_bits'];verify(a,room,index,bits)
                results.append(a);semantics.append((room,index,comparable(a)))
        (out/f'host-{bits}.json').write_text(json.dumps(results,indent=2)+'\n');print(f'PASS host {bits}: {len(results)} original callback/lifetime runs',flush=True)
    if args.adb:
        def adb(*a,timeout=240):return run([args.adb,'-s',args.serial,*a],timeout)
        remote='/data/local/tmp/re4-shelf-poc';adb('shell','mkdir','-p',remote+'/blobs');adb('push',catalog,remote+'/catalog.tsv')
        for f in files:adb('push',data/'blobs'/f['sha256'],remote+'/blobs/'+f['sha256'])
        images={}
        try:
            for abi,bits in [('armeabi-v7a',32),('arm64-v8a',64)]:
                exe=root/f'port/build/android-{abi}/re4_gx_tool';target=remote+'/gx-'+abi;adb('push',exe,target);adb('shell','chmod','755',target);results=[]
                for repeat in range(args.repeat):
                    for room,index in cases:
                        a=json.loads(adb('shell',target,'shelf',remote,remote+'/catalog.tsv',0,f'{room:x}',index));verify(a,room,index,bits)
                        results.append(a);semantics.append((room,index,comparable(a)))
                (out/f'android-{abi}.json').write_text(json.dumps(results,indent=2)+'\n');print(f'PASS {abi}: {len(results)} original callback/lifetime runs',flush=True)
                adb('install','-r','--abi',abi,root/'port/android/app/build/outputs/apk/debug/app-debug.apk')
                package=adb('shell','dumpsys','package','org.re4.port').decode()
                if 'primaryCpuAbi='+abi not in package:raise ValueError('APK actual ABI')
                adb('shell','run-as','org.re4.port','mkdir','-p','files/shelf-poc/blobs');adb('shell','run-as','org.re4.port','cp',remote+'/catalog.tsv','files/shelf-poc/catalog.tsv')
                for f in files:adb('shell','run-as','org.re4.port','cp',remote+'/blobs/'+f['sha256'],'files/shelf-poc/blobs/'+f['sha256'])
                for room in [0x103,0x106]:
                    process=None
                    for repeat in range(args.repeat):
                        if repeat==0:adb('shell','am','force-stop','org.re4.port')
                        session='shelf-'+uuid.uuid4().hex
                        adb('shell','input','keyevent','KEYCODE_WAKEUP');adb('shell','am','start','-n','org.re4.port/.GeometryActivity','--es','root','/data/user/0/org.re4.port/files/shelf-poc','--es','catalog','/data/user/0/org.re4.port/files/shelf-poc/catalog.tsv','--ei','room',room,'--ei','shelf_selection',0,'--es','capture',session)
                        stem='files/'+session+'-shelf';deadline=time.monotonic()+180
                        while True:
                            probe=subprocess.run([str(args.adb),'-s',args.serial,'shell','run-as','org.re4.port','cat',stem+'-tick31.json'],capture_output=True,timeout=20)
                            if probe.returncode==0:break
                            if time.monotonic()>deadline:raise TimeoutError('original shelf GPU callback did not return')
                            time.sleep(.5)
                        for tick in [0,15,30,31]:
                            evidence=json.loads(adb('shell','run-as','org.re4.port','cat',stem+f'-tick{tick}.json'));a=evidence['shelf_callback'];verify_callback(a,room,0,bits)
                            if a['visits']!=tick or evidence['validation_errors']!=0 or evidence['non_clear_pixels']<100 or evidence['graphics_domains']!=1:raise ValueError('GPU evidence/tick/validation/owner')
                            rgba=adb('exec-out','run-as','org.re4.port','cat',stem+f'-tick{tick}.rgba')
                            if len(rgba)!=512*448*4 or sha(rgba)!=evidence['rgba_sha256']:raise ValueError('GPU readback integrity')
                            prefix=out/f'gpu-{abi}-r{room:x}-{repeat+1}-tick{tick}';prefix.with_suffix('.json').write_text(json.dumps(evidence,indent=2)+'\n');prefix.with_suffix('.rgba').write_bytes(rgba)
                            from PIL import Image
                            Image.frombytes('RGBA',(512,448),rgba).save(prefix.with_suffix('.png'))
                            images.setdefault((room,tick),[]).append(rgba)
                        base=images[(room,0)][-1];end=images[(room,30)][-1]
                        changed=sum(base[i:i+4]!=end[i:i+4] for i in range(0,len(base),4))
                        if changed<100:raise ValueError('original shelf changes are not visible')
                        (out/f'display-{abi}-r{room:x}-{repeat+1}.png').write_bytes(adb('exec-out','screencap','-p'))
                        pid=adb('shell','pidof','org.re4.port').decode().strip()
                        if process is not None and process!=pid:raise ValueError('surface recreation replaced process; teardown not proven')
                        process=pid
                        adb('shell','input','keyevent','KEYCODE_BACK');time.sleep(.8)
                        log=adb('logcat','-d','--pid='+pid)
                        if log.count(b'Shelf callback owner retired: models=0 stacks=0')<repeat+1:raise ValueError('actual callback surface/model/stack retirement absent')
                        (out/f'log-{abi}-r{room:x}-{repeat+1}.txt').write_bytes(log)
                        print(f'PASS GPU {abi} r{room:x}: 31 scheduler visits / changed pixels {changed}',flush=True)
            if any(any(b!=values[0] for b in values) for values in images.values()):raise ValueError('repeated/dual ABI GPU pixels differ')
        finally:adb('shell','am','force-stop','org.re4.port')
    # Existing host evidence in this same private source catalog participates
    # in the Android comparison too; never compare unrelated data directories.
    for bits in [32,64]:
        path=out/f'host-{bits}.json'
        if path.exists():
            for a in json.loads(path.read_text()):
                room,index=a['animated']['room'],a['animated']['selection'];verify(a,room,index,bits)
                semantics.append((room,index,comparable(a)))
    for room,index in cases:
        values=[v for r,i,v in semantics if r==room and i==index]
        if any(v!=values[0] for v in values):raise ValueError('ABI/repetition callback semantic mismatch')
    (out/'result.json').write_text(json.dumps(dict(status='passed',cases=cases,runs=len(semantics),gpu=bool(args.adb),scope='isolated original callbacks; DEBUG camera/textures; no complete room/item-event/gameplay or audible sound'),indent=2)+'\n')
    return 0

if __name__=='__main__':raise SystemExit(main())
