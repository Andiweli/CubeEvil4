"""Private original BIN/FCV tests, independent PSQ oracle and ABI comparisons.

Original blobs/derived fixtures and reports remain outside the repository.
This verifies skeletal poses and weights, not cloth, shape playback or gameplay.
"""
import argparse,ctypes,json,math,struct,sys
from pathlib import Path
sys.dont_write_bytecode=True
from test_gx_scene import run,sha

def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def fma_function():
    if hasattr(math,'fma'):return lambda a,b,c:f32(math.fma(a,b,c))
    lib=ctypes.CDLL('ucrtbase' if sys.platform=='win32' else 'libm.so.6')
    fn=lib.fmaf;fn.argtypes=[ctypes.c_float]*3;fn.restype=ctypes.c_float
    return fn
fma=fma_function()

def quantize(raw,palette,exponent,limit):
    result=[]
    for *xyz,index in raw:
        if not 0<=index<len(palette):raise ValueError('oracle palette index')
        m=palette[index];xyz=[f32(math.ldexp(x,-exponent)) for x in xyz]
        for row in range(3):
            y=m[9+row]
            for col in range(3):y=fma(m[col*3+row],xyz[col],y)
            result.append(math.trunc(max(-limit-1,min(limit,f32(math.ldexp(y,exponent))))))
    return result

def curve(axis,fmt,frame,max_frame,fcv):
    values=[fcv.to_float(fcv.FCC_FMT[fmt][0],k[0]) for k in axis.keys]
    if frame>=max_frame:return values[-1]
    if frame in axis.frames:return values[axis.frames.index(frame)]
    i=next(i for i in range(len(axis.frames)-1) if axis.frames[i]<frame<axis.frames[i+1])
    t=(frame-axis.frames[i])/(axis.frames[i+1]-axis.frames[i]);t2=t*t;t3=t2*t
    tangents=fcv.FCC_FMT[fmt][1];a=fcv.to_float(tangents,axis.keys[i][2]);b=fcv.to_float(tangents,axis.keys[i+1][1])
    return (2*t3-3*t2+1)*values[i]+(-2*t3+3*t2)*values[i+1]+(t3-2*t2+t)*a+(t3-t2)*b

def verify_model(got,raw,motion=None):
    from motion import meshbin,fcv
    import numpy as np
    m=meshbin.parse(raw);p=got['palette'];count=0
    if got['bin_sha256']!=sha(raw):raise ValueError('immutable original BIN identity')
    static=m.weight_palette_num<=1 and m.weight_ext_num<=255 and len(m.heads)==1 and m.shapes is None
    if got['static_bypass']!=static:raise ValueError('original static bypass')
    expected=[x for v in m.vtx for x in v[:3]] if static else quantize(m.vtx,p,m.shift,32767)
    if expected!=got['positions']:raise ValueError('PSQ S16 positions differ')
    count+=len(expected)
    small=bool(m.flags&0x20000000);expected=[x for v in m.nrm for x in v[:3]] if static else quantize(m.nrm,p,-32 if small else -14,127 if small else 32767)
    if expected!=got['normals8' if small else 'normals16']:raise ValueError('PSQ normals differ')
    count+=len(expected);matrix_error=0.;linear_error=0.;translation_error=0.
    def matrix(v):return np.vstack([np.array(v,dtype=np.float64).reshape(3,4),[0,0,0,1]])
    bones=[matrix(v) for v in got['bone_matrices']];bind=[matrix(v) for v in got['bind_matrices']]
    root_index=m.heads[0].parts_no if static else 0
    draw=(bones[root_index]@matrix(got['info_matrix'][0]))[:3,:].flatten()
    if any(abs(a-b)>max(.01,abs(b)*2e-6) for a,b in zip(got['draw_matrix'][0],draw)):raise ValueError('original commonModelTrans weighted/static root matrix')
    count+=12
    if not static:
        if len(p)!=len(m.weights) or got['influences']!=sum(w.num for w in m.weights):raise ValueError('original weight table selection/count')
        inverse=np.linalg.inv(bones[0]);transforms=[inverse@a@b for a,b in zip(bones,bind)]
        for actual,w in zip(p,m.weights):
            value=np.zeros((4,4));total=f32(0)
            for j in range(w.num):
                rate=f32(f32(w.wht[j])*f32(.01))
                if j==w.num-1:rate=f32(1-total)
                total=f32(total+rate);value+=transforms[w.ids[j]]*rate
            ref=value[:3,:].T.flatten();delta=max(abs(float(a)-float(b)) for a,b in zip(actual,ref));matrix_error=max(matrix_error,delta)
            linear_error=max(linear_error,max(abs(float(a)-float(b)) for a,b in zip(actual[:9],ref[:9])))
            translation_error=max(translation_error,max(abs(float(a)-float(b)) for a,b in zip(actual[9:],ref[9:])))
            if any(abs(a-b)>max(.05,abs(b)*2e-5) for a,b in zip(actual,ref)):raise ValueError('independent double-precision bind/weight matrix oracle')
            count+=12
    motion_error=0.;motion_errors=dict(rot=0.,pos=0.,scale=0.)
    if motion:
        parsed=fcv.parse(motion);frame=got['motion_frame']
        if any(j.kind&0x30 for j in parsed.joints):raise ValueError('oracle was given an IK pose without IK')
        for j in parsed.joints:
            if j.channel in (6,7) or j.target() is None:continue
            expected=[curve(a,j.fcc_type,frame,parsed.max_frame,fcv) for a in j.axes]
            if j.target()=='rot':
                expected=[(x+math.pi)%(2*math.pi)-math.pi for x in expected];actual=got['local_angles'][j.parts_no]
            elif j.target()=='pos':actual=got['local_positions'][j.parts_no]
            else:actual=got['local_scales'][j.parts_no]
            for a,b in zip(actual,expected):
                motion_error=max(motion_error,abs(a-b))
                motion_errors[j.target()]=max(motion_errors[j.target()],abs(a-b))
                if abs(a-b)>max(.004,abs(b)*3e-5):raise ValueError('actual MotionMoveCore versus independent original FCV interpolation')
                count+=1
    return dict(scalar_checks=count,psq_quantized_difference=0,weight_matrix_max_abs_error=matrix_error,weight_linear_max_abs_error=linear_error,weight_translation_max_abs_error=translation_error,fcv_max_abs_error=motion_error,fcv_max_abs_error_by_target=motion_errors)

from retroid_adb import hardware_sequence

@hardware_sequence()
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--data',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--host-exe',type=Path);p.add_argument('--adb',type=Path);p.add_argument('--serial',default='dbdcacc');p.add_argument('--repeat',type=int,default=2)
    a=p.parse_args();root=Path(__file__).resolve().parents[2];data=a.data.resolve();out=a.output.resolve()
    if out.is_relative_to(root) or out==data or not 1<=a.repeat<=10:raise ValueError('private separate output / repeat1..10')
    out.mkdir(parents=True,exist_ok=True);sys.path.insert(0,str(root/'tools'));from motion import archive,meshbin,fcv
    manifest=json.loads((data/'manifest.json').read_text());required=[(0,'em/pl00.drs'),(1,'em/pl00.drs'),(0,'Evd/r120s00.evd'),(0,'Evd/r120s01.evd'),(1,'Evd/r317s15.evd'),(0,'St1/r10c.das'),(0,'St2/r22a.das')]
    files={(f['disc_number'],f['path']):f for f in manifest['files'] if (f['disc_number'],f['path']) in required}
    if set(files)!=set(required):raise ValueError('original resource catalog coverage')
    def raw(name,disc=0):
        f=files[disc,name];b=(data/'blobs'/f['sha256']).read_bytes()
        if sha(b)!=f['sha256']:raise ValueError('original import integrity')
        return b
    for disc,name in required:raw(name,disc)
    catalog=out/'catalog.tsv';catalog.write_text('RE4-IMPORT-CATALOG-1\n'+''.join(f"{f['disc_number']}\t{f['index']}\t{f['size']}\t{f['path']}\t{f['sha256']}\n" for f in files.values()),newline='\n')
    import re
    refs=re.findall(r'EvtMgr\.SetBin\("([^"]+)",\s*PL_ARC_PTR\(pG->pPlayer,\s*([^)]+)\)',(root/'src/game/event.cpp').read_text())
    if len(refs)!=6:raise ValueError('original external player registrations changed')
    external={}
    for disc in (0,1):
        player=archive.load_das(data/'blobs'/files[disc,'em/pl00.drs']['sha256'])[0]
        external[disc]={name:player.entries[int(index,0)-4][1] for name,index in refs}
    packages={(disc,name):dict(archive.parse_event(raw(name,disc))) for disc,name in required if name.endswith('.evd')}
    for (disc,name),package in packages.items():
        for k,v in external[disc].items():package.setdefault(k,v)
    # No hand-authored motion: named Mot packets and actual camera-cut frames.
    cases=[(0,'Evd/r120s00.evd',0,0),(0,'Evd/r120s00.evd',0,100),(0,'Evd/r120s00.evd',0,400),(0,'Evd/r120s00.evd',1,30),(0,'Evd/r120s01.evd',0,30),(1,'Evd/r317s15.evd',0,0)]
    fixtures=[]
    for name in ['St1/r10c.das','St2/r22a.das']:
        entries=archive.load_das(data/'blobs'/files[0,name]['sha256'])[0].entries
        for i,(tag,b) in enumerate(entries):
            if tag==b'BIN\0' and struct.unpack_from('>H',b,42)[0]>255:
                if entries[i+1][0]!=b'TPL\0':raise ValueError('extended BIN/TPL archive reference')
                bn=out/f'{Path(name).stem}-bin-{i}.bin';tn=out/f'{Path(name).stem}-tpl-{i+1}.tpl';bn.write_bytes(b);tn.write_bytes(entries[i+1][1]);fixtures.append((bn,tn,b))
    if len(fixtures)!=2:raise ValueError('two original extended weight models required')
    # A full native Leon BIN, not its sparse attachments.
    bn=out/'pl000a.bin';tn=out/'pl000a.tpl';bn.write_bytes(external[0]['em/pl00/pl000a.bin']);tn.write_bytes(external[0]['em/pl00/pl000a.tpl']);fixtures.append((bn,tn,bn.read_bytes()))
    abi_reports={};semantic={};floating={}
    def execute(command,abi,target_root,target_catalog,fixture_base):
        results=[];metrics=[];fingerprints=[]
        for repeat in range(a.repeat):
            adapters=json.loads(run([*command,'self-test']));
            if adapters['failed']:raise ValueError('adapter failures')
            for disc,event,cut,frame in cases:
                value=json.loads(run([*command,'skin',target_root,target_catalog,disc,event,cut,frame]));resource=packages[disc,event]
                if value['event_sha256']!=sha(raw(event,disc)):raise ValueError('original EVD identity')
                for model in value['models']:metrics.append(verify_model(model,resource[model['bin']],resource.get(model['motion'])))
                if not value['models']:raise ValueError('no original skinning execution')
                fingerprints.append([[m['name'],m['motion'],m['motion_frame'],m['positions'],m['normals8'],m['normals16']] for m in value['models']]);results.append(value)
            for b,t,source in fixtures:
                value=json.loads(run([*command,'model',str(fixture_base)+'/'+b.name,str(fixture_base)+'/'+t.name]));metrics.append(verify_model(value['models'][0],source));results.append(value)
                fingerprints.append([[m['positions'],m['normals8'],m['normals16']] for m in value['models']])
        (out/f'skin-{abi}.json').write_text(json.dumps(results)+'\n')
        summary=dict(abi=abi,rounds=a.repeat,executions=len(results),checks=sum(m['scalar_checks'] for m in metrics),adapter_checks=adapters['checks'],psq_quantized_difference=max(m['psq_quantized_difference'] for m in metrics),weight_matrix_max_abs_error=max(m['weight_matrix_max_abs_error'] for m in metrics),fcv_max_abs_error=max(m['fcv_max_abs_error'] for m in metrics),dependencies=sorted(set(d for r in results for d in r['dependencies'])))
        summary.update(weight_linear_max_abs_error=max(m['weight_linear_max_abs_error'] for m in metrics),weight_translation_max_abs_error=max(m['weight_translation_max_abs_error'] for m in metrics),fcv_max_abs_error_by_target={key:max(m['fcv_max_abs_error_by_target'][key] for m in metrics) for key in ('pos','rot','scale')})
        def flatten(value):
            for item in value:
                if isinstance(item,list):yield from flatten(item)
                else:yield item
        floating[abi]=[v for result in results for model in result['models'] for key in ('palette','bone_matrices','bind_matrices','draw_matrix','local_positions','local_angles','local_scales') for v in flatten(model[key])]
        abi_reports[abi]=summary;semantic[abi]=fingerprints;print('PASS',json.dumps(summary),flush=True)
    if a.host_exe:
        bits=json.loads(run([a.host_exe,'self-test']))['pointer_bits'];execute([a.host_exe],f'host-{bits}',data,catalog,out)
    if a.adb:
        def adb(*args):return run([a.adb,'-s',a.serial,*args])
        remote='/data/local/tmp/re4-skinning';adb('shell','mkdir','-p',remote+'/blobs');adb('push',catalog,remote+'/catalog.tsv')
        for f in files.values():adb('push',data/'blobs'/f['sha256'],remote+'/blobs/'+f['sha256'])
        for b,t,_ in fixtures:adb('push',b,remote+'/'+b.name);adb('push',t,remote+'/'+t.name)
        for abi,bits in [('armeabi-v7a',32),('arm64-v8a',64)]:
            exe=root/f'port/build/android-{abi}/re4_gx_tool';target=remote+'/'+abi;adb('push',exe,target);adb('shell','chmod','755',target)
            if json.loads(adb('shell',target,'self-test'))['pointer_bits']!=bits:raise ValueError('executed ABI')
            execute([a.adb,'-s',a.serial,'shell',target],abi,remote,remote+'/catalog.tsv',remote)
        if semantic['armeabi-v7a']!=semantic['arm64-v8a']:raise ValueError('ARM32/ARM64 quantized skin/identity differs; inspect private reports')
        if len(floating['armeabi-v7a'])!=len(floating['arm64-v8a']):raise ValueError('ARM32/ARM64 matrix extent differs')
        delta=max(abs(x-y) for x,y in zip(floating['armeabi-v7a'],floating['arm64-v8a']))
        for report in abi_reports.values():report['arm32_arm64_float_max_abs_delta']=delta
        print('ARM32/ARM64 native float matrix/pose maximum absolute difference',delta,flush=True)
        print('PASS ARM32/ARM64 original skinning results byte-identical',flush=True)
    (out/'skinning-summary.json').write_text(json.dumps(abi_reports,indent=2)+'\n')
if __name__ == '__main__':
    raise SystemExit(main())
