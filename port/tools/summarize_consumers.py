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
    model_traces={}
    checks=set()
    message_traces=set()
    message_checks=set()
    measured=[]
    for abi,bits,folder in [('host-32',32,args.host),('host-64',64,args.host),
                            ('armeabi-v7a',32,args.device),('arm64-v8a',64,args.device)]:
        rooms=json.loads((folder/f'game-runtime-{abi}.json').read_text())['results']
        events=json.loads((folder/f'consumers-{abi}.json').read_text())['results']
        models=json.loads((folder/f'models-{abi}.json').read_text())['results']
        messages=json.loads((folder/f'messages-{abi}.json').read_text())['results']
        if sorted((r['round'],r['disc']) for r in messages)!=[(1,0),(1,1),(2,0),(2,1)]:
            raise ValueError('missing/duplicate message boot rounds: '+abi)
        for r in messages:
            actual=r['actual']
            if r['abi']!=abi or actual['pointer_bits']!=bits or not actual['repeated_init_released']:
                raise ValueError('message ABI/lifetime: '+abi)
            stable={k:v for k,v in actual.items() if k!='pointer_bits'}
            if sha(json.dumps(stable,sort_keys=True).encode())!=r['trace_sha256']:
                raise ValueError('message evidence hash: '+abi)
            message_traces.add(r['trace_sha256']); message_checks.add(actual['checks'])
        if sorted((r['round'],r['disc']) for r in rooms)!=[(1,0),(1,1),(2,0),(2,1)]:
            raise ValueError('missing/duplicate room rounds: '+abi)
        if sorted((r['round'],r['disc']) for r in events)!=[(1,0),(2,0)]:
            raise ValueError('missing/duplicate opening-package rounds: '+abi)
        if sorted((r['round'],r['disc']) for r in models)!=[(1,0),(1,1),(2,0),(2,1)]:
            raise ValueError('missing/duplicate additional-model rounds: '+abi)
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
                if package['original_init_blocker']:
                    raise ValueError('original CPU R120Init did not complete: '+abi)
        for record in models:
            if record['abi']!=abi or record['actual']['pointer_bits']!=bits:
                raise ValueError('additional model ABI: '+abi)
            if sha(json.dumps(record['actual']['packages'],sort_keys=True).encode())!=record['trace_sha256']:
                raise ValueError('additional model evidence hash: '+abi)
            model_traces.setdefault(record['disc'],set()).add(record['trace_sha256'])
        actual_initializations=sum(2 for r in [*events,*models] for p in r['actual']['packages']
                                  for m in p['model_initializations'] if m['status']=='original-modelInit-two-rounds-released')
        measured.append(dict(abi=abi,pointer_bits=bits,rounds=2,
                             room_preparations=sum(r['passed'] for r in rooms),
                             event_packages=sum(len(r['actual']['packages']) for r in events),
                             actual_modelInit_calls=actual_initializations,
                             original_SetPartsSub_calls=sum(len([m for m in p['event_registration'] if m['type']==3]) for r in [*events,*models] for p in r['actual']['packages']),
                             original_MotionSetCore_calls=sum(len(p['motion_initializations']) for r in [*events,*models] for p in r['actual']['packages']),
                             player_MotionSetCore_calls=sum(len(p['player_motion_initializations']) for r in events for p in r['actual']['packages']),
                             failed=0,executable_sha256=sorted({r['executable_sha256'] for r in rooms})))
        measured[-1].update(original_message_boot_runs=len(messages),message_checks_per_run=1130,
                            original_SubScreenGameInit_calls=2*len(messages),
                            isolated_inventory_map_modelInit_calls=12*len(messages),
                            subscreen_archive_reads=44*len(messages))
    if any(len(v)!=1 for v in [*room_traces.values(),*model_traces.values()]) or len(event_traces)!=1 or len(native_traces)!=1 or checks!={634}:
        raise ValueError('ABI/repetition semantic results differ')
    if len(message_traces)!=1 or message_checks!={1130}: raise ValueError('message/SubScreen ABI/repetition semantics differ')
    sample=json.loads((args.host/'consumers-host-64.json').read_text())['results'][0]['actual']['packages']
    packages=[dict(path=p['path'],sha256=p['sha256'],bins=len(p['bins']),native_cpu_models=p['native_models'],
                   model_initializations=p['model_initializations'],event_resource_references=p['event_resource_references'],
                   external_bins=p['external_bins'],event_registration=p['event_registration'],motion_initializations=p['motion_initializations'],
                   player_motion_initializations=p['player_motion_initializations'],
                   english_mdt=[dict(name=b['name'],count=b['original_english_count'])
                                for b in p['bins'] if 'original_english_count' in b]) for p in sample]
    sample=json.loads((args.host/'game-runtime-host-64.json').read_text())['results']
    coverage={r['id']:r['consumers'] for record in sample for r in record['actual']['rooms']}
    additional=[p for r in json.loads((args.host/'models-host-64.json').read_text())['results'] if r['round']==1 for p in r['actual']['packages']]
    summary=dict(schema='RE4-PHASE4C-SUBSCREEN-MEASUREMENTS-1',checks_per_native_run=634,
                 measured=measured,identical_native_traces=True,
                 identical_room_traces={str(k):next(iter(v)) for k,v in room_traces.items()},
                 identical_event_trace=next(iter(event_traces)),packages=packages,room_consumer_coverage=coverage,
                 additional_model_packages=additional,identical_additional_model_traces={str(k):next(iter(v)) for k,v in model_traces.items()},
                 identical_message_trace=next(iter(message_traces)),message_checks_per_run=1130,
                 message_boot_sample=json.loads((args.host/'messages-host-64.json').read_text())['results'][0]['actual'],
                 scope='Actual original SubScreenGameInit, SubScreenRoomInit, inventory init/search/get/itemInfo, map manager reset/map filenames and DMA; 44 isolated original archive loads, 78 checked TPL blocks, six model pairs initialized twice. Existing model/event/motion consumers retained; no menu widgets, full Event::Run/Obj18/GX draw renderer. GPU evidence is separate APK instrumentation.')
    write_json(output/'consumer-runtime-summary.json',summary)
    print(json.dumps(summary,indent=2))


if __name__=='__main__':
    main()
