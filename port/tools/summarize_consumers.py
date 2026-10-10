#!/usr/bin/env python3
"""Cross-check Phase-4c measurements; summary contains metadata, never asset bytes."""
import argparse
import json
from pathlib import Path
from original_import.pipeline import external_root, sha, write_json


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host',required=True,type=Path)
    parser.add_argument('--device',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    output=external_root(args.output,Path(__file__).resolve().parents[2],[])
    output.mkdir(parents=True,exist_ok=True)
    room_traces={}
    event_traces=set()
    native_traces=set()
    checks=set()
    measured=[]
    for abi,bits,folder in [('host-32',32,args.host),('host-64',64,args.host),
                            ('armeabi-v7a',32,args.device),('arm64-v8a',64,args.device)]:
        rooms=json.loads((folder/f'game-runtime-{abi}.json').read_text())['results']
        events=json.loads((folder/f'consumers-{abi}.json').read_text())['results']
        if sorted((r['round'],r['disc']) for r in rooms)!=[(1,0),(1,1),(2,0),(2,1)]:
            raise ValueError('missing/duplicate room rounds: '+abi)
        if sorted((r['round'],r['disc']) for r in events)!=[(1,0),(2,0)]:
            raise ValueError('missing/duplicate opening-package rounds: '+abi)
        for record in rooms:
            if record['abi']!=abi or record['failed'] or record['isolated']['failed']:
                raise ValueError('runtime failure: '+abi)
            if record['actual']['pointer_bits']!=bits or record['isolated']['pointer_bits']!=bits:
                raise ValueError('runtime ABI: '+abi)
            actual=record['actual']['rooms']
            if sha(json.dumps(actual,sort_keys=True).encode())!=record['trace_sha256']:
                raise ValueError('room evidence hash: '+abi)
            room_traces.setdefault(record['disc'],set()).add(record['trace_sha256'])
            native_traces.add(json.dumps(record['isolated']['trace'],sort_keys=True))
            checks.add(record['isolated']['checks'])
        for record in events:
            actual=record['actual']
            if record['abi']!=abi or actual['pointer_bits']!=bits:
                raise ValueError('consumer ABI: '+abi)
            if sha(json.dumps(actual['packages'],sort_keys=True).encode())!=record['trace_sha256']:
                raise ValueError('consumer evidence hash: '+abi)
            event_traces.add(record['trace_sha256'])
            for package in actual['packages']:
                if not package['retired'] or package['callback_mode']!=3:
                    raise ValueError('original cancel/lifetime: '+abi)
                if package['flags_after']!=(package['flags_before']|0x10):
                    raise ValueError('original scenario flag: '+abi)
        measured.append(dict(abi=abi,pointer_bits=bits,rounds=2,
                             room_preparations=sum(r['passed'] for r in rooms),
                             event_packages=sum(len(r['actual']['packages']) for r in events),
                             failed=0,executable_sha256=sorted({r['executable_sha256'] for r in rooms})))
    if any(len(v)!=1 for v in room_traces.values()) or len(event_traces)!=1 or len(native_traces)!=1 or checks!={503}:
        raise ValueError('ABI/repetition semantic results differ')
    sample=json.loads((args.host/'consumers-host-64.json').read_text())['results'][0]['actual']['packages']
    packages=[dict(path=p['path'],sha256=p['sha256'],bins=len(p['bins']),native_cpu_models=p['native_models'],
                   english_mdt=[dict(name=b['name'],count=b['original_english_count'])
                                for b in p['bins'] if 'original_english_count' in b]) for p in sample]
    sample=json.loads((args.host/'game-runtime-host-64.json').read_text())['results']
    coverage={r['id']:r['consumers'] for record in sample for r in record['actual']['rooms']}
    summary=dict(schema='RE4-PHASE4C-MEASUREMENTS-1',checks_per_native_run=503,
                 measured=measured,identical_native_traces=True,
                 identical_room_traces={str(k):next(iter(v)) for k,v in room_traces.items()},
                 identical_event_trace=next(iter(event_traces)),packages=packages,room_consumer_coverage=coverage,
                 scope='Partial CPU consumers and controlled original r120 init failure/cancel; no Event::Init/packet playback/full game start')
    write_json(output/'consumer-runtime-summary.json',summary)
    print(json.dumps(summary,indent=2))


if __name__=='__main__':
    main()
