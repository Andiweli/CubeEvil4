#!/usr/bin/env python3
"""Check final executable provenance and cross-ABI engine results, then save a private summary."""
import argparse
import json
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from original_import.pipeline import external_root, write_json, sha
from test_engine_loading import verify


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--focused',required=True)
    parser.add_argument('--all-host',required=True)
    parser.add_argument('--all-device',required=True)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[2]
    focused=external_root(args.focused,root,[])
    broad_host=external_root(args.all_host,root,[])
    broad_device=external_root(args.all_device,root,[])
    reports=[]
    traces={}
    catalogs=set()
    counts={}
    for scope,folder,files in [
        ('focused',focused,['host-32-engine-tests.json','host-64-engine-tests.json','android-engine-tests.json']),
        ('all',broad_host,['host-32-engine-tests.json','host-64-engine-tests.json']),
        ('all',broad_device,['android-engine-tests.json'])]:
        cases=json.loads((folder/'engine-cases.json').read_text())
        catalog=sha((folder/'engine-catalog.tsv').read_bytes())
        catalogs.add(catalog)
        for name in files:
            path=folder/name
            report=json.loads(path.read_text())
            if report['catalog_sha256']!=catalog:raise ValueError('catalog provenance mismatch')
            for result in report['results']:
                bits=result['isolated']['pointer_bits']
                abi=result.get('abi',f'host-{bits}')
                exe=root/('port/build/host-ninja/re4_loader_tool.exe' if abi=='host-64' else
                          'port/build/host-x86/re4_loader_tool.exe' if abi=='host-32' else
                          f'port/build/android-{abi}/re4_loader_tool')
                actual=sha(exe.read_bytes())
                if actual!=result.get('executable_sha256',report.get('executable_sha256')):
                    raise ValueError(f'{abi}: stale executable test report')
                if result['failed'] or result['passed']!=len(cases[str(result['disc'])]):
                    raise ValueError('incomplete engine result')
                trace=verify(result['engine'],cases[str(result['disc'])])
                if trace!=result['trace_sha256']:raise ValueError('altered result trace')
                traces.setdefault((scope,result['disc']),set()).add(trace)
                counts[(scope,abi)]=counts.get((scope,abi),0)+result['passed']
            reports.append(dict(path=str(path),sha256=sha(path.read_bytes()),scope=scope))
    if len(catalogs)!=1 or any(len(values)!=1 for values in traces.values()):
        raise ValueError('cross-ABI identity/semantic mismatch')
    summary=dict(catalog_sha256=next(iter(catalogs)),reports=reports,
        loads=[dict(scope=scope,abi=abi,passed=n) for (scope,abi),n in sorted(counts.items())],
        semantic_traces=[dict(scope=scope,disc=disc,sha256=next(iter(values)))
                         for (scope,disc),values in sorted(traces.items())],
        total_successful_loads=sum(counts.values()),failed=0,
        limitations=['All-room/device data tests are native CLI processes, not APK asset access.',
                     'Frames until I/O completion are not asserted equal.',
                     'MRAM path; no ARAM residency, sound/model initialization, or REL execution.'])
    write_json(focused/'verification-summary.json',summary)
    print(json.dumps({k:summary[k] for k in ['loads','total_successful_loads','failed','semantic_traces']},indent=2))


if __name__=='__main__':main()
