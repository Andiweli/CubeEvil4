#!/usr/bin/env python3
"""Private real-file tests through the shared frame runtime, not a playable game."""
import argparse
import json
import struct
import uuid
import sys
import re
from pathlib import Path
from original_import.pipeline import external_root, sha, write_json
from test_engine_loading import prepare, run


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--data', required=True)
    p.add_argument('--output', required=True)
    p.add_argument('--host-exe')
    p.add_argument('--adb')
    p.add_argument('--serial')
    p.add_argument('--repeat', type=int, default=2)
    args = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    data = external_root(args.data,root,[])
    output = external_root(args.output,root,[])
    if data == output or not 1 <= args.repeat <= 10 or not (args.host_exe or args.adb):
        raise ValueError('separate output / repeat 1..10 / host or adb required')
    cases = prepare(data,output,False)
    manifest = json.loads((data/'manifest.json').read_text())
    sys.path.insert(0,str(root/'tools'))
    import drs
    from motion import fcv
    registrations=re.findall(r'EvtMgr.SetBin\("([^"]+)", PL_ARC_PTR\(pG->pPlayer, ([\dxA-F]+)\), 0, 2\);',
                             (root/'src/game/event.cpp').read_text())
    if len(registrations)!=6: raise ValueError('original player event bin registration changed')
    player_files={f['disc_number']:f for f in manifest['files'] if f['path']=='em/pl00.drs'}
    player_bytes=(data/'blobs'/player_files[0]['sha256']).read_bytes()
    player_drs=drs.Drs(player_bytes)
    external={name:player_drs.entries[int(index,0)-4][1] for name,index in registrations}
    files = {(f['disc_number'],f['path'].lower()):f for f in manifest['files']}
    # Named packages are evidenced by the original source references, not guessed room rules.
    refs=json.loads((data/'source-references.json').read_text())
    referenced={identity for row in refs['literals'] if row['source']=='src/st1/r120.cpp'
                for identity in row.get('resolved',[])}
    packages=[f for f in manifest['files'] if f['id'] in referenced and f.get('analysis',{}).get('format')=='event']
    if len(packages)!=2: raise ValueError('original opening EVD source references differ')
    # Additional model packages are selected universally by FST inventory and
    # model presence: the smallest non-opening package on each disc.
    additional=[]
    for disc in sorted({f['disc_number'] for f in manifest['files']}):
        candidates=[f for f in manifest['files'] if f['disc_number']==disc and f not in packages
                    and f.get('analysis',{}).get('format')=='event'
                    and any(e['tag']=='BIN' for e in f['analysis']['entries'])]
        if not candidates: raise ValueError('no additional disc model package')
        additional.append(min(candidates,key=lambda f:(f['size'],f['path'])))
    digests = {r['source_sha256'] for group in cases.values() for r in group}
    digests.update(b['sha256'] for group in cases.values() for r in group for b in r['blocks'])
    # All original ESL variants, including the two that the original table never selects.
    digests.update(f['sha256'] for f in manifest['files'] if f['path'].lower().endswith('.esl'))
    digests.update(f['sha256'] for f in packages)
    digests.update(f['sha256'] for f in additional)
    digests.update(f['sha256'] for f in player_files.values())
    boot_files=[f for f in manifest['files'] if f['path'].lower() in ['etc/core.das','font/common_p.fnt','rel/sscrn.rel'] or f['path'].lower().startswith('ss/eng/') or f['path'].lower().startswith('ss/cmn/map_obj')]
    digests.update(f['sha256'] for f in boot_files)
    for digest in digests:
        if sha((data/'blobs'/digest).read_bytes()) != digest: raise ValueError('source blob differs')
    catalog = output/'engine-catalog.tsv'
    results = []
    consumer_results=[]
    model_results=[]
    message_results=[]
    def verify_messages(actual,disc,bits):
        if actual['pointer_bits']!=bits or actual['kind']!='actual_CoreDataRead_MessageControl_init_gameInit_Font_create':
            raise ValueError('message original execution/ABI')
        cf=files[(disc,'etc/core.das')]
        source=(data/'blobs'/cf['sha256']).read_bytes()
        core=drs.Drs(source)
        if actual['core_body_sha256']!=cf['analysis']['body_sha256']: raise ValueError('CoreDataRead body integrity')
        def tpl(blob):
            v,n,table=struct.unpack_from('>III',blob)
            if v!=0x20af30: raise ValueError('original TPL reference signature')
            result=[]
            for i in range(n):
                at,cl=struct.unpack_from('>II',blob,table+8*i)
                h,w,fmt,pixel=struct.unpack_from('>HHII',blob,at)
                x,y,z={0:(8,8,32),1:(8,4,32),2:(8,4,32),3:(4,4,32),4:(4,4,32),5:(4,4,32),6:(4,4,64),8:(8,8,32),9:(8,4,32),10:(4,4,32),14:(8,8,32)}[fmt]
                r=dict(width=w,height=h,format=fmt,sha256=sha(blob[pixel:pixel+((w+x-1)//x)*((h+y-1)//y)*z]))
                if cl:
                    entries=struct.unpack_from('>H',blob,cl)[0]; pf,off=struct.unpack_from('>II',blob,cl+4)
                    r.update(palette_entries=entries,palette_format=pf,palette_sha256=sha(blob[off:off+2*entries]))
                result.append(r)
            return result
        expected=[]
        for slot in [4,17,18,16]:
            for r in tpl(core.entries[slot-4][1]): expected.append({k:r[k] for k in ['width','height','format','sha256']})
        if actual['core_textures']!=expected: raise ValueError('actual original SpecularInit/GlobalIlmTexInit state')
        if actual['thermal_sha256']!=tpl(core.entries[19-4][1])[0]['palette_sha256']: raise ValueError('original Thermo TLUT')
        ff=files[(disc,'font/common_p.fnt')]; font=(data/'blobs'/ff['sha256']).read_bytes()
        at,width=struct.unpack_from('>II',font)
        if actual['font_textures']!=tpl(font[at:width]) or actual['widths_sha256']!=sha(font[width:]): raise ValueError('actual original English FNT sheets/CLUT/width data')
        glyphs=[]
        for n in range((len(font)-width)//2):
            l,r=struct.unpack_from('>bb',font,width+(n if n else 1)*2); glyphs.append([l,r,r-l])
        if actual['glyphs']!=glyphs: raise ValueError('original MessageFont::getSize bearings')
        layout=[[22,32,65535,25],[23,32,0,25],[18,24,65535,19],[21,25,0,20],[21,28,0,25],[23,23,0,28],[16,21,65535,0],[20,25,65535,21],[20,25,65535,21]]
        if actual['layouts']!=layout: raise ValueError('original English layout presets')
        for type,r in enumerate(actual['messages']):
            blob=core.entries[(21 if type==3 else 10)-4][1]
            at=struct.unpack_from('>I',blob,8)[0]; count=struct.unpack_from('>I',blob,at+4)[0]
            end=min([n for n in struct.unpack_from('>6I',blob,4) if n>at]+[len(blob)])
            offsets=struct.unpack_from('>'+str(count)+'I',blob,at+8)
            hashes=[sha(blob[at+off:min([at+n for n in offsets if n>off]+[end])]) for off in offsets]
            if r!=dict(type=type,count=count,payload_hashes=hashes): raise ValueError('original MessageData references/spans')
        ss=actual['subscreen']; address=0xD00000
        for r,path in zip(ss['resources'],['rel/sscrn.rel','ss/eng/ss_cmmn.dat','ss/eng/ss_pzzl.dat']):
            f=files[(disc,path)]; dma=(f['size']+31)&~31
            if r!=dict(path=f['path'],size=f['size'],sha256=f['sha256'],aram=address,dma_bytes=dma):
                raise ValueError('original SubScreen DMA resource/hash/offset')
            address+=dma
        if len(ss['resources'])!=3 or ss['free_offset']!=address-0xD00000 or ss['inventory_slots']!=384 or len(ss['item_info'])!=256 or len(ss['map_paths'])!=18:
            raise ValueError('original SubScreen inventory/map state')
        if ss['trace'].count('SubScreenGameInit:original-complete')!=2:
            raise ValueError('original SubScreen repeated initialization incomplete')
        expected_archives=[f for f in boot_files if f['disc_number']==disc and
                           (f['path'].lower().startswith('ss/cmn/map_obj') or (f['path'].lower().startswith('ss/eng/') and f['path'].lower().endswith('.dat')))]
        if [r['path'] for r in ss['archives']]!=[f['path'] for f in expected_archives]: raise ValueError('SubScreen resource coverage')
        for r,f in zip(ss['archives'],expected_archives):
            if r['sha256']!=f['sha256']: raise ValueError('SubScreen archive hash')
            from original_import.formats import body_index
            blob=(data/'blobs'/f['sha256']).read_bytes(); entries,_=body_index(blob)
            if len(r['entries'])!=len(entries): raise ValueError('SubScreen block count')
            for value,entry in zip(r['entries'],entries):
                if any(value[k]!=entry[k] for k in ['index','tag','offset','size']) or value['sha256']!=sha(blob[entry['offset']:entry['offset']+entry['size']]): raise ValueError('SubScreen original block integrity')
        if len(ss['model_consumers'])!=6 or any(r['rounds']!=2 for r in ss['model_consumers']): raise ValueError('original inventory/map model initialization')
        if actual['blocker']!='Original consumer unavailable: Cckpt.gameInit()' or not actual['repeated_init_released'] or actual['checks']<960:
            raise ValueError('actual GameTask progress or message lifecycle')
        stable={k:v for k,v in actual.items() if k!='pointer_bits'}
        return sha(json.dumps(stable,sort_keys=True).encode())
    def verify_consumers(actual,expected,bits,opening=True):
        if actual['pointer_bits']!=bits or len(actual['packages'])!=len(expected): raise ValueError('consumer ABI/package count')
        for value,f in zip(actual['packages'],expected):
            if value['path']!=f['path'] or value['sha256']!=f['sha256'] or not value['retired']: raise ValueError('consumer original file/lifetime')
            if value['original_init_blocker']: raise ValueError('original CPU init dependency')
            if opening:
                if value['callback_mode']!=3 or value['flags_after']!=(value['flags_before']|0x10): raise ValueError('actual original event cancel persistence')
            elif value['callback_mode']!=-1 or value['flags_after']!=value['flags_before']: raise ValueError('diagnostic may not execute other-room callbacks')
            if len(value['bins'])!=len(f['analysis']['entries']): raise ValueError('EVD entry count')
            source=(data/'blobs'/f['sha256']).read_bytes()
            for observed,entry in zip(value['bins'],f['analysis']['entries']):
                if any(observed[k]!=entry[k] for k in ['name','offset','size','sha256']): raise ValueError('EVD bin identity/reference differs')
                if entry['tag']=='BIN':
                    if observed['original_model_parts']!=entry['validation']['parts']: raise ValueError('original model joint count')
                    raw=source[entry['offset']:entry['offset']+entry['size']]
                    joint=int.from_bytes(raw[:4],'big')
                    parents=[raw[joint+i*16+1] for i in range(raw[0x19])]
                    if observed['parents']!=parents: raise ValueError('original model parent indices')
                if entry['tag']=='MDT' and observed['original_english_count']!=entry['validation']['english']['message_count']:
                    raise ValueError('original event English MDT lookup differs')
            if value['native_models']!=sum(e['tag']=='BIN' for e in f['analysis']['entries']): raise ValueError('model registry count')
            at,size=struct.unpack_from('>II',source,0x40); end=at+size
            refs=[]
            def text(pos,n):
                raw=source[pos:pos+n]
                if b'\0' not in raw: raise ValueError('unbounded reference')
                return raw.split(b'\0')[0].decode('ascii')
            entries={e['name']:e for e in f['analysis']['entries']}
            expected_external=[dict(name=name,size=len(blob),sha256=sha(blob)) for name,blob in external.items()] if opening else []
            if value['external_bins']!=expected_external: raise ValueError('original PL_ARC resource bindings/hash')
            if value['player_archive_sha256']!=(player_files[f['disc_number']]['sha256'] if opening else ''):
                raise ValueError('player archive identity differs')
            def resource(name):
                if name in entries:
                    e=entries[name]; return source[e['offset']:e['offset']+e['size']]
                return external.get(name) if opening else None
            while at<end:
                kind=struct.unpack_from('>I',source,at)[0]; n=struct.unpack_from('>h',source,at+12)[0]
                if n<16 or at+n>end: raise ValueError('reference packet boundary')
                if kind in (3,4,11):
                    p=at+(40 if kind==4 else 28)
                    refs.append(dict(offset=at,type=kind,model=text(at+16,12),parent=text(at+28,12) if kind==4 else '',
                                     bin=text(p,48),tpl=text(p+48,48) if kind!=11 else '',bin_in_package=text(p,48) in entries))
                at+=n
            if refs!=value['event_resource_references']: raise ValueError('original event model/motion references differ')
            pairs=list(dict.fromkeys((r['bin'],r['tpl']) for r in refs if r['type']!=11))
            if len(pairs)!=len(value['model_initializations']): raise ValueError('model pair count')
            for (bn,tn),init in zip(pairs,value['model_initializations']):
                if (init['bin'],init['tpl'])!=(bn,tn): raise ValueError('model/TPL association changed')
                raw,rawtex=resource(bn),resource(tn)
                if raw is None or rawtex is None:
                    if init['status']!='external-resource-not-bound': raise ValueError('missing resource falsely bound')
                    continue
                if bn not in entries and next(r for r in refs if r['bin']==bn)['type']==4:
                    if init['status']!='original-SetPartsSub-in-registration-pass': raise ValueError('sparse attachment used as standalone skeleton')
                    continue
                if init['status']!='original-modelInit-two-rounds-released': raise ValueError('original model execution missing')
                n=struct.unpack_from('>H',raw,0x38)[0]; off=struct.unpack_from('>I',raw,0x30)[0]
                vertices=[struct.unpack_from('>hhh',raw,off+8*i) for i in range(n)]
                if not vertices: raise ValueError('empty reference bbox must be reviewed')
                low=[min(v[k] for v in vertices)/(1<<raw[0x28]) for k in range(3)]
                high=[max(v[k] for v in vertices)/(1<<raw[0x28]) for k in range(3)]
                box=[(low[k]+high[k])/2 for k in range(3)]+[(high[k]-low[k])/2 for k in range(3)]
                if init['parts']!=raw[0x19] or init['vertices']!=n or init['textures']!=struct.unpack_from('>I',rawtex,4)[0]: raise ValueError('model counts/TPL descriptors')
                if any(abs(a-b)>max(1e-4,abs(b)*1e-5) for a,b in zip(init['bounding_box'],box)): raise ValueError('original bounding box differs from independent vertices')
            bodies=list(dict.fromkeys(r['model'] for r in refs if r['type']==3 and resource(r['bin']) is not None and resource(r['tpl']) is not None))
            parts=list(dict.fromkeys(r['model'] for r in refs if r['type']==4))
            registered=value['event_registration']
            if [r['name'] for r in registered]!=bodies+parts: raise ValueError('original SetMod/SetPartsSub registration coverage')
            for slot,r in enumerate(registered):
                if r['slot']!=slot or r['type']!=(2 if slot<len(bodies) else 3): raise ValueError('original model table type/order')
                if r['type']==3:
                    ref=next(ref for ref in refs if ref['model']==r['name'] and ref['type']==4)
                    if r['parent']!=ref['parent'] or r['parts']!=resource(ref['bin'])[25]: raise ValueError('attached hierarchy/model references')
            expected_motions=[r for r in refs if r['type']==11 and r['model'] in bodies and resource(r['bin'])[:2]!=b'\0\0']
            if len(value['motion_initializations'])!=len(expected_motions): raise ValueError('actual MotionSetCore coverage')
            def axis_value(axis,fmt,frame,max_frame):
                values=[fcv.to_float(fcv.FCC_FMT[fmt][0],k[0]) for k in axis.keys]
                if frame>=max_frame: return values[-1]
                if frame in axis.frames: return values[axis.frames.index(frame)]
                i=next(i for i in range(len(axis.frames)-1) if axis.frames[i]<frame<axis.frames[i+1])
                t=(frame-axis.frames[i])/(axis.frames[i+1]-axis.frames[i]); t2=t*t; t3=t2*t
                tangents=fcv.FCC_FMT[fmt][1]
                a=fcv.to_float(tangents,axis.keys[i][2]); b=fcv.to_float(tangents,axis.keys[i+1][1])
                return (2*t3-3*t2+1)*values[i]+(-2*t3+3*t2)*values[i+1]+(t3-2*t2+t)*a+(t3-t2)*b
            for got,ref in zip(value['motion_initializations'],expected_motions):
                motion=fcv.parse(resource(ref['bin']))
                if got['model']!=ref['model'] or got['bin']!=ref['bin'] or got['joints']!=len(motion.joints) or got['max_frame']!=motion.max_frame:
                    raise ValueError('original motion identity/frame/joint count')
                for field,kind in [('null_pos',1),('null_rot',0x40)]:
                    expected=next((i for i,j in enumerate(motion.joints) if j.kind==kind),65535)
                    if got[field]!=expected: raise ValueError('original MotionSetCore root assignment')
                if not motion.joints:
                    if got['samples']: raise ValueError('empty player FCV must not fabricate samples')
                    continue
                first=motion.joints[0]
                for n,sample in enumerate(got['samples']):
                    reference=[axis_value(a,first.fcc_type,motion.max_frame*n/2,motion.max_frame) for a in first.axes]
                    if any(abs(a-b)>max(2e-5,abs(b)*2e-5) for a,b in zip(sample,reference)):
                        raise ValueError('actual original Hermite differs from independent FCV curves')
            player_motions=[(i,blob) for i,(tag,blob) in enumerate(player_drs.entries) if tag==b'FCV\0'] if opening else []
            if len(value['player_motion_initializations'])!=len(player_motions): raise ValueError('player FCV execution coverage')
            for got,(index,blob) in zip(value['player_motion_initializations'],player_motions):
                motion=fcv.parse(blob)
                if got['index']!=index or got['sha256']!=sha(blob) or got['joints']!=len(motion.joints) or got['max_frame']!=motion.max_frame:
                    raise ValueError('player FCV identity/ABI data')
                for field,kind in [('null_pos',1),('null_rot',0x40)]:
                    expected=next((i for i,j in enumerate(motion.joints) if j.kind==kind),65535)
                    if got[field]!=expected: raise ValueError('player original motion root assignment')
                if not motion.joints:
                    if got['samples']: raise ValueError('empty player FCV must not fabricate samples')
                    continue
                first=motion.joints[0]
                for n,sample in enumerate(got['samples']):
                    reference=[axis_value(a,first.fcc_type,motion.max_frame*n/2,motion.max_frame) for a in first.axes]
                    if any(abs(a-b)>max(2e-5,abs(b)*2e-5) for a,b in zip(sample,reference)):
                        raise ValueError('player original Hermite differs from independent FCV curves')
        return sha(json.dumps(actual['packages'],sort_keys=True).encode())
    def verify(actual,reference,disc):
        if len(actual['rooms']) != len(reference): raise ValueError('room result count')
        for value,expected in zip(actual['rooms'],reference):
            for key in ['id','source_sha256','body_sha256','body_size']:
                if value[key] != expected[key]: raise ValueError('original archive result differs: '+key)
            if value['entries'] != len(expected['entries']): raise ValueError('archive entry count')
            if value['blocks'] != [{k:b[k] for k in ['path','sha256']} for b in expected['blocks']]: raise ValueError('block result differs')
            esl = files[(disc,value['esl_path'].lower())]
            if value['esl_sha256'] != esl['sha256'] or value['esl_size'] != esl['size'] or not value['original_read_observed']:
                raise ValueError('actual original readEmList differs')
            archive=next(f for f in manifest['files'] if f['disc_number']==disc and f['sha256']==value['source_sha256'])
            for consumer in value['consumers']:
                entry=next(e for e in archive['analysis']['entries'] if e['index']==consumer['index'])
                if consumer['sha256']!=entry['sha256'] or consumer['tag']!=entry['tag']: raise ValueError('CAM/MDT source identity')
                if consumer['tag']=='MDT':
                    english=expected['english_mdt']
                    if consumer['status']!=('open' if english['status']=='open' else 'indexed'): raise ValueError('English MDT status differs')
                    if consumer['status']=='indexed' and consumer['count']!=english['count']: raise ValueError('original English message count')
                elif consumer['status']!='indexed' and consumer.get('reason')!='CAM pFrame outside immutable resource; relocation semantics unresolved':
                    raise ValueError('unexpected CAM validation failure: '+consumer.get('reason',''))
        if actual['scenario_blocker'] != 'Original consumer unavailable: SceAtSetSaveItem': raise ValueError('shared scenario missing-dependency behavior')
        return sha(json.dumps(actual['rooms'],sort_keys=True).encode())
    def execute(command,abi,bits,targetRoot,targetCatalog,exeHash,fixtureRoot):
        for repeat in range(1,args.repeat+1):
            isolated = json.loads(run([*command,'self-test',str(fixtureRoot)+'/game-fixtures-'+abi]))
            if isolated['pointer_bits'] != bits or isolated['failed'] or isolated['checks'] < 634:
                raise ValueError('isolated native runtime test/ABI')
            for disc in sorted(int(d) for d in cases):
                boot=json.loads(run([*command,'messages',targetRoot,targetCatalog,str(disc)]))
                boot_trace=verify_messages(boot,disc,bits)
                message_results.append(dict(abi=abi,round=repeat,disc=disc,trace_sha256=boot_trace,actual=boot,executable_sha256=exeHash))
                write_json(output/f'messages-{abi}.json',dict(results=[r for r in message_results if r['abi']==abi]))
                print(f'PASS {abi} round {repeat} disc {disc}: original CoreDataRead, Font/Message init, actual GameTask beyond cMes.gameInit',flush=True)
            for disc in sorted({f['disc_number'] for f in packages}):
                expected=[f for f in packages if f['disc_number']==disc]
                actual=json.loads(run([*command,'consumers',targetRoot,targetCatalog,str(disc),*[f['path'] for f in expected]]))
                trace=verify_consumers(actual,expected,bits)
                consumer_results.append(dict(abi=abi,round=repeat,disc=disc,trace_sha256=trace,actual=actual,executable_sha256=exeHash))
                write_json(output/f'consumers-{abi}.json',dict(results=[r for r in consumer_results if r['abi']==abi]))
                print(f'PASS {abi} round {repeat}: {len(expected)} real EVD packages, original modelInit/TPL/matrices and cancel callbacks; Event::Init remains open',flush=True)
            for disc in sorted({f['disc_number'] for f in additional}):
                expected=[f for f in additional if f['disc_number']==disc]
                actual=json.loads(run([*command,'models',targetRoot,targetCatalog,str(disc),*[f['path'] for f in expected]]))
                trace=verify_consumers(actual,expected,bits,False)
                model_results.append(dict(abi=abi,round=repeat,disc=disc,trace_sha256=trace,actual=actual,executable_sha256=exeHash))
                write_json(output/f'models-{abi}.json',dict(results=[r for r in model_results if r['abi']==abi]))
                print(f'PASS {abi} round {repeat} disc {disc}: additional original models from '+','.join(f['path'] for f in expected),flush=True)
            for disc,group in cases.items():
                actual = json.loads(run([*command,'data',targetRoot,targetCatalog,disc,*[r['id'] for r in group]]))
                if actual['pointer_bits'] != bits: raise ValueError('actual ABI')
                trace = verify(actual,group,int(disc))
                results.append(dict(abi=abi,round=repeat,disc=int(disc),passed=len(group),failed=0,
                    trace_sha256=trace,isolated=isolated,actual=actual,executable_sha256=exeHash))
                write_json(output/f'game-runtime-{abi}.json',dict(results=[r for r in results if r['abi']==abi],catalog_sha256=sha(catalog.read_bytes())))
                print(f'PASS {abi} round {repeat} disc {disc}: {len(group)} real archive/ESL preparations; full scenario explicitly blocked',flush=True)
    if args.host_exe:
        exe=Path(args.host_exe).resolve()
        probe=json.loads(run([exe,'self-test',output/'host-probe']))
        execute([exe],'host-'+str(probe['pointer_bits']),probe['pointer_bits'],data,catalog,sha(exe.read_bytes()),output)
    if args.adb:
        if not args.serial: raise ValueError('serial required')
        def adb(*parts): return run([args.adb,'-s',args.serial,*parts])
        supported=adb('shell','getprop','ro.product.cpu.abilist').split(',')
        remote='/data/local/tmp/re4-game-'+uuid.uuid4().hex
        adb('shell','mkdir','-p',remote+'/blobs'); adb('push',catalog,remote+'/catalog.tsv')
        if adb('shell','sha256sum',remote+'/catalog.tsv').split()[0] != sha(catalog.read_bytes()): raise ValueError('catalog transfer')
        for digest in sorted(digests):
            adb('push',data/'blobs'/digest,remote+'/blobs/'+digest)
            if adb('shell','sha256sum',remote+'/blobs/'+digest).split()[0] != digest: raise ValueError('blob transfer')
        write_json(output/'device.json',dict(serial=args.serial,android=adb('shell','getprop','ro.build.version.release'),
            model=adb('shell','getprop','ro.product.model'),supported_abis=supported,remote=remote))
        for abi,bits in [('armeabi-v7a',32),('arm64-v8a',64)]:
            if abi not in supported: raise ValueError('unsupported ABI')
            exe=root/f'port/build/android-{abi}/re4_game_tool'
            target=remote+'/game-'+abi
            adb('push',exe,target); adb('shell','chmod','755',target)
            digest=sha(exe.read_bytes())
            if adb('shell','sha256sum',target).split()[0] != digest: raise ValueError('executable transfer')
            execute([args.adb,'-s',args.serial,'shell',target],abi,bits,remote,remote+'/catalog.tsv',digest,remote)
    for disc in cases:
        if len({r['trace_sha256'] for r in results if r['disc']==int(disc)}) != 1: raise ValueError('ABI/repeat difference')
    if len({r['trace_sha256'] for r in consumer_results})!=1: raise ValueError('consumer ABI/repeat difference')
    if len({r['trace_sha256'] for r in message_results})!=1: raise ValueError('message ABI/repeat difference')
    for disc in {r['disc'] for r in model_results}:
        if len({r['trace_sha256'] for r in model_results if r['disc']==disc})!=1: raise ValueError('additional model ABI/repeat difference')


if __name__ == '__main__': main()
