#!/usr/bin/env python3
"""Private Phase-3b catalogs and real engine-load verification on hosts/devices.

This consumes Phase-3a evidence. No guessed room rules, assets in Git, or language
substitution. Only referenced complete CAS files are sent to Android.
"""
import argparse
import hashlib
import json
import re
import subprocess
import sys
import uuid
from pathlib import Path
sys.dont_write_bytecode = True
from original_import.pipeline import external_root, write_json, sha, walk_entries


def entries(analysis):
    return [[e['index'], e['offset'], e['size'], e['occurrence'], e['tag'][:3]]
            for e in analysis['entries']]


def prepare(data, output, all_rooms):
    manifest = json.loads((data/'manifest.json').read_text())
    language=json.loads((data/'languages.json').read_text())
    language_blocks={(m['file_id'],m['entry']):next(b for b in m['blocks'] if b['language']=='eng')
                     for m in language['message_blocks']}
    by_id = {f['id']: f for f in manifest['files']}
    catalog = ['RE4-IMPORT-CATALOG-1']
    for f in manifest['files']:
        if not re.fullmatch('[0-9a-f]{64}', f['sha256']) or '\t' in f['path'] or '\n' in f['path']:
            raise ValueError('invalid manifest identity')
        catalog.append(f"{f['disc_number']}\t{f['index']}\t{f['size']}\t{f['path']}\t{f['sha256']}")
    output.mkdir(parents=True, exist_ok=True)
    (output/'engine-catalog.tsv').write_text('\n'.join(catalog)+'\n', encoding='utf-8', newline='\n')
    cases = {}
    for path in sorted((data/'rooms').glob('r*-disc*.json')):
        room = json.loads(path.read_text())
        if not all_rooms and (room['disc'], room['id']) not in [(0,'120'),(0,'100'),(0,'101'),(1,'400'),(1,'402'),(1,'405')]:
            continue
        original = by_id[room['file_id']]
        analysis = original['analysis']
        blocks = []
        for dep in room['block_dependencies']:
            resolved = [by_id[k] for k in dep['resolved'] if by_id[k]['disc_number']==room['disc']]
            if len(resolved)!=1:
                raise ValueError(f"unresolved/ambiguous block dependency {dep}")
            block = resolved[0]
            blocks.append(dict(path=block['path'],size=block['size'],sha256=block['sha256'],entries=entries(block['analysis'])))
        expected = dict(id=room['id'],source_sha256=original['sha256'],body_sha256=analysis['body_sha256'],
                        body_size=analysis['decode']['size'],entries=entries(analysis),blocks=blocks)
        mdt=next((e for e in analysis['entries'] if e['tag']=='MDT'),None)
        english=language_blocks[(original['id'],str(mdt['index']))] if mdt else dict(status='missing',count=0)
        expected['english_mdt']={k:english[k] for k in ['status','count'] if k in english}
        cases.setdefault(str(room['disc']),[]).append(expected)
    # Intentional repeats prove unloading, queue/data-unit reuse, and retained spans.
    for disc, group in cases.items():
        if not all_rooms:
            start = next((r for r in group if r['id']=='120'), group[0])
            second = next((r for r in group if r['id']=='100'), group[-1])
            cases[disc] = [start, second, *[r for r in group if r not in [start,second]], start,second]
    if not cases: raise ValueError('no engine cases')
    write_json(output/'engine-cases.json', cases)
    refs=json.loads((data/'source-references.json').read_text())
    incomplete=[]
    for message in language['message_blocks']:
        english=next(b for b in message['blocks'] if b['language']=='eng')
        if english['status']!='open':continue
        original=by_id[message['file_id']]
        entry=next(e for name,e in walk_entries(original['analysis']['entries']) if name==message['entry'])
        static=[r for r in refs['literals'] if message['file_id'] in r.get('resolved',[])]
        incomplete.append(dict(file_id=message['file_id'],path=original['path'],entry=message['entry'],
            size=entry['size'],sha256=entry['sha256'],reason=english['detail'],static_references=static,
            needed_at='roomInit registers RoomMes; actual dereference in MessageData::getAddr/getMesNum'
                if original['path'].lower().endswith('.das') else
                'Event::ExePacket_SetMdt selects named GetBin, then MessageData::getAddr/getMesNum',
            executed_message_reachability='open: room/event gameplay is not yet running'))
    write_json(output/'english-mdt-reachability.json',dict(reference='eng',incomplete=incomplete,
        count=len(incomplete),policy='preserve bytes; lazy bounds failure; no replacement/fallback text',
        evidence=['src/game/read.cpp:ReadAreaData','src/game/mes.cpp:MessageControl::roomInit',
            'src/game/mes.cpp:MessageData::getAddr/getMesNum','src/game/event.cpp:Event::ExePacket_SetMdt']))
    return cases


def verify(result, expected):
    if result['pointer_bits'] not in [32,64]:raise ValueError('invalid process ABI')
    actual = result['rooms']
    if len(actual)!=len(expected):raise ValueError('room count differs')
    for room, reference in zip(actual, expected):
        room=dict(room)
        functions=room.pop('flow')
        for name in ['void ReadAreaData()','void cDvd::ReadProc()','native decodeData/Yz2DecodeExec','native GetDataExt (bounded BE)']:
            if name not in functions:raise ValueError(f'original engine path not observed: {name}')
        if reference['blocks'] and 'int cBlock::getBlockWork(u8 no)' not in functions:
            raise ValueError('original block path not observed')
        if room!=reference:raise ValueError(f"engine output differs for r{reference['id']}")
    return sha(json.dumps(actual,sort_keys=True,separators=(',',':')).encode())


def run(command, timeout=600):
    r=subprocess.run([str(p) for p in command],capture_output=True,text=True,timeout=timeout)
    if r.returncode:raise RuntimeError(r.stderr.strip() or r.stdout.strip())
    return r.stdout.strip()


from retroid_adb import hardware_sequence

@hardware_sequence()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data',required=True)
    parser.add_argument('--output',required=True)
    parser.add_argument('--host-exe')
    parser.add_argument('--adb')
    parser.add_argument('--serial')
    parser.add_argument('--abi',action='append',choices=['armeabi-v7a','arm64-v8a'])
    parser.add_argument('--repeat',type=int,default=2)
    parser.add_argument('--all-rooms',action='store_true')
    args=parser.parse_args()
    if not 1<=args.repeat<=10:raise ValueError('repeat outside 1..10')
    root=Path(__file__).resolve().parents[2]
    data=external_root(args.data,root,[])
    output=external_root(args.output,root,[])
    if output==data:raise ValueError('keep Phase-3a evidence separate')
    cases=prepare(data,output,args.all_rooms)
    def verify_input(digest):
        path=data/'blobs'/digest
        if sha(path.read_bytes())!=digest:raise ValueError('private input hash mismatch')
        return path
    digests=sorted({r['source_sha256'] for group in cases.values() for r in group}|
                   {b['sha256'] for group in cases.values() for r in group for b in r['blocks']})
    for digest in digests:verify_input(digest)
    results=[]
    catalog=output/'engine-catalog.tsv'
    if args.host_exe:
        for repeat in range(1,args.repeat+1):
            isolated=json.loads(run([args.host_exe,'self-test',Path(args.host_exe).resolve().parent/'loader-tests']))
            for disc,group in cases.items():
                actual=json.loads(run([args.host_exe,'rooms',data,catalog,disc,*[r['id'] for r in group]]))
                trace=verify(actual,group)
                results.append(dict(round=repeat,disc=int(disc),passed=len(group),failed=0,trace_sha256=trace,
                                    isolated=isolated,engine=actual))
                print(f"PASS host {isolated['pointer_bits']}-bit round {repeat} disc {disc}: {len(group)} real engine loads",flush=True)
        write_json(output/f"host-{isolated['pointer_bits']}-engine-tests.json",dict(results=results,
            executable_sha256=sha(Path(args.host_exe).read_bytes()),catalog_sha256=sha(catalog.read_bytes())))
    if args.adb:
        if not args.serial or not re.fullmatch('[A-Za-z0-9_.:-]+',args.serial):raise ValueError('invalid device serial')
        def adb(*parts,timeout=600):return run([args.adb,'-s',args.serial,*parts],timeout)
        info=dict(serial=args.serial,model=adb('shell','getprop','ro.product.model'),
            android=adb('shell','getprop','ro.build.version.release'),api=adb('shell','getprop','ro.build.version.sdk'),
            supported_abis=adb('shell','getprop','ro.product.cpu.abilist').split(','))
        remote='/data/local/tmp/re4-engine-'+uuid.uuid4().hex
        adb('shell','mkdir','-p',remote+'/blobs')
        adb('push',catalog,remote+'/catalog.tsv')
        if adb('shell','sha256sum',remote+'/catalog.tsv').split()[0]!=sha(catalog.read_bytes()):raise ValueError('catalog transfer differs')
        print(f"{info}: deploying {len(digests)} verified original files",flush=True)
        for digest in digests:
            adb('push',verify_input(digest),remote+'/blobs/'+digest)
            if adb('shell','sha256sum',remote+'/blobs/'+digest).split()[0]!=digest:raise ValueError('device input hash differs')
        device_results=[]
        for abi in args.abi or ['armeabi-v7a','arm64-v8a']:
            if abi not in info['supported_abis']:raise ValueError(f'{abi} unsupported')
            exe=root/f'port/build/android-{abi}/re4_loader_tool'
            target=remote+'/loader-'+abi
            adb('push',exe,target);adb('shell','chmod','755',target)
            if adb('shell','sha256sum',target).split()[0]!=sha(exe.read_bytes()):raise ValueError('device executable differs')
            for repeat in range(1,args.repeat+1):
                isolated=json.loads(adb('shell',target,'self-test',remote+'/synthetic-'+abi))
                if isolated['pointer_bits']!=(32 if abi=='armeabi-v7a' else 64):raise ValueError('wrong process ABI')
                for disc,group in cases.items():
                    actual=json.loads(adb('shell',target,'rooms',remote,remote+'/catalog.tsv',disc,*[r['id'] for r in group]))
                    if actual['pointer_bits']!=isolated['pointer_bits']:raise ValueError('room process ABI differs')
                    trace=verify(actual,group)
                    device_results.append(dict(abi=abi,round=repeat,disc=int(disc),passed=len(group),failed=0,
                        trace_sha256=trace,isolated=isolated,engine=actual,executable_sha256=sha(exe.read_bytes())))
                    write_json(output/'android-engine-tests.json',dict(device=info,remote_directory=remote,
                        results=device_results,catalog_sha256=sha(catalog.read_bytes())))
                    print(f'PASS {abi} round {repeat} disc {disc}: {len(group)} real engine loads',flush=True)
        results+=device_results
    for disc in cases:
        traces={r['trace_sha256'] for r in results if str(r['disc'])==disc}
        if len(traces)>1:raise ValueError('ABI/repeat semantic trace differs')
    print(f"PASS: catalog {len(json.loads((data/'manifest.json').read_text())['files'])} original files; tested {sum(map(len,cases.values()))} room loads per run",flush=True)


if __name__ == '__main__':
    raise SystemExit(main())
