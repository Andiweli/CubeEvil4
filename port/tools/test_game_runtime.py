#!/usr/bin/env python3
"""Private real-file tests through the shared frame runtime, not a playable game."""
import argparse
import json
import struct
import uuid
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
    for digest in digests:
        if sha((data/'blobs'/digest).read_bytes()) != digest: raise ValueError('source blob differs')
    catalog = output/'engine-catalog.tsv'
    results = []
    consumer_results=[]
    model_results=[]
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
                if bn not in entries or tn not in entries:
                    if init['status']!='external-resource-not-bound': raise ValueError('missing resource falsely bound')
                    continue
                if init['status']!='original-modelInit-two-rounds-released': raise ValueError('original model execution missing')
                entry=entries[bn]; raw=source[entry['offset']:entry['offset']+entry['size']]
                tex=entries[tn]; rawtex=source[tex['offset']:tex['offset']+tex['size']]
                n=struct.unpack_from('>H',raw,0x38)[0]; off=struct.unpack_from('>I',raw,0x30)[0]
                vertices=[struct.unpack_from('>hhh',raw,off+8*i) for i in range(n)]
                if not vertices: raise ValueError('empty reference bbox must be reviewed')
                low=[min(v[k] for v in vertices)/(1<<raw[0x28]) for k in range(3)]
                high=[max(v[k] for v in vertices)/(1<<raw[0x28]) for k in range(3)]
                box=[(low[k]+high[k])/2 for k in range(3)]+[(high[k]-low[k])/2 for k in range(3)]
                if init['parts']!=raw[0x19] or init['vertices']!=n or init['textures']!=struct.unpack_from('>I',rawtex,4)[0]: raise ValueError('model counts/TPL descriptors')
                if any(abs(a-b)>max(1e-4,abs(b)*1e-5) for a,b in zip(init['bounding_box'],box)): raise ValueError('original bounding box differs from independent vertices')
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
            if isolated['pointer_bits'] != bits or isolated['failed'] or isolated['checks'] < 609:
                raise ValueError('isolated native runtime test/ABI')
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
    for disc in {r['disc'] for r in model_results}:
        if len({r['trace_sha256'] for r in model_results if r['disc']==disc})!=1: raise ValueError('additional model ABI/repeat difference')


if __name__ == '__main__': main()
