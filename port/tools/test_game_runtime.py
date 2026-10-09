#!/usr/bin/env python3
"""Private real-file tests through the shared frame runtime, not a playable game."""
import argparse
import json
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
    digests = {r['source_sha256'] for group in cases.values() for r in group}
    digests.update(b['sha256'] for group in cases.values() for r in group for b in r['blocks'])
    # All original ESL variants, including the two that the original table never selects.
    digests.update(f['sha256'] for f in manifest['files'] if f['path'].lower().endswith('.esl'))
    for digest in digests:
        if sha((data/'blobs'/digest).read_bytes()) != digest: raise ValueError('source blob differs')
    catalog = output/'engine-catalog.tsv'
    results = []
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
        if actual['scenario_blocker'] != 'Original consumer unavailable: SceAtSetSaveItem': raise ValueError('shared scenario missing-dependency behavior')
        return sha(json.dumps(actual['rooms'],sort_keys=True).encode())
    def execute(command,abi,bits,targetRoot,targetCatalog,exeHash,fixtureRoot):
        for repeat in range(1,args.repeat+1):
            isolated = json.loads(run([*command,'self-test',str(fixtureRoot)+'/game-fixtures-'+abi]))
            if isolated['pointer_bits'] != bits or isolated['failed'] or isolated['checks'] < 140:
                raise ValueError('isolated native runtime test/ABI')
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


if __name__ == '__main__': main()
