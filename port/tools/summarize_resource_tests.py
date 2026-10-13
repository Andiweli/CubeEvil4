#!/usr/bin/env python3
"""Validate final binaries, private reference hashes and all Phase-3c ABI reports."""
import argparse
import csv
import json
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from original_import.pipeline import external_root, sha, write_json
from test_engine_loading import verify


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--resources', required=True)
    p.add_argument('--engine-host', required=True)
    p.add_argument('--engine-device', required=True)
    args = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = external_root(args.resources, root, [])
    engine_host = external_root(args.engine_host, root, [])
    engine_device = external_root(args.engine_device, root, [])
    read = lambda path: json.loads(path.read_text())
    cases = read(output/'resource-cases.json')
    rooms = read(output/'engine-cases.json')
    catalog_hash = sha((output/'engine-catalog.tsv').read_bytes())
    rows, all_results = [], []
    platforms = [('host-32', 'host-x86', 32), ('host-64', 'host-ninja', 64),
                 ('armeabi-v7a', 'android-armeabi-v7a', 32), ('arm64-v8a', 'android-arm64-v8a', 64)]
    for label, build, bits in platforms:
        report = read(output/f'{label}-resources.json')
        exe = root/f'port/build/{build}/re4_resource_tool'
        if label.startswith('host'):
            exe = exe.with_suffix('.exe')
        exe_hash = sha(exe.read_bytes())
        if report['catalog_sha256'] != catalog_hash or len(report['results']) != 8:
            raise ValueError('incomplete/stale resource report')
        identities = set()
        for r in report['results']:
            identity = (r['round'], r['disc'], r['kind'])
            if identity in identities or r['round'] not in [1, 2] or r['disc'] not in [0, 1]:
                raise ValueError('duplicate/invalid round')
            identities.add(identity)
            if r['executable_sha256'] != exe_hash or r['pointer_bits'] != bits or r['isolated']['pointer_bits'] != bits or r['failed'] != 0:
                raise ValueError('resource executable/ABI/failure differs')
            expected = cases[str(r['disc'])] if r['kind'] == 'resources' else rooms[str(r['disc'])]
            actual = r['result'][r['kind']]
            if r['passed'] != len(expected) or len(actual) != len(expected):
                raise ValueError('resource coverage differs')
            for a, e in zip(actual, expected):
                if r['kind'] == 'resources':
                    if a['path'] != e['path'] or a['sha256'] != e['sha256'] or a['states'] != [1, 2, 6, 4, 5, 2]:
                        raise ValueError('resource data/state mismatch')
                elif a['id'] != e['id'] or a['body_sha256'] != e['body_sha256'] or a['block_transitions'] != len(e['blocks']):
                    raise ValueError('room data/block mismatch')
                rows.append(dict(platform=label, round=r['round'], disc=r['disc'], kind=r['kind'],
                                 resource=a.get('path', a.get('id')), passed=True,
                                 sha256=a.get('sha256', a.get('body_sha256'))))
            trace = sha(json.dumps(actual, sort_keys=True, separators=(',', ':')).encode())
            if r['trace_sha256'] != trace:
                raise ValueError('reported trace does not match output')
            all_results.append(r)
    if len({r['isolated']['checks'] for r in all_results}) != 1:
        raise ValueError('isolated check count differs across ABI')
    for disc in ['0', '1']:
        for kind in ['resources', 'rooms']:
            if len({r['trace_sha256'] for r in all_results if str(r['disc']) == disc and r['kind'] == kind}) != 1:
                raise ValueError('resource ABI semantic traces differ')
    engine_results = []
    engine_files = [(engine_host/'host-32-engine-tests.json', 'host-x86', 32),
                    (engine_host/'host-64-engine-tests.json', 'host-ninja', 64),
                    (engine_device/'android-engine-tests.json', None, None)]
    for path, build, bits in engine_files:
        j = read(path)
        if j['catalog_sha256'] != catalog_hash:
            raise ValueError('engine catalog differs')
        for r in j['results']:
            selected = build or 'android-'+r['abi']
            exe = root/f'port/build/{selected}/re4_loader_tool'
            if build:
                exe = exe.with_suffix('.exe')
            if sha(exe.read_bytes()) != (j['executable_sha256'] if build else r['executable_sha256']):
                raise ValueError('stale engine binary')
            if bits and r['engine']['pointer_bits'] != bits:
                raise ValueError('host engine ABI differs')
            if r['failed'] or r['trace_sha256'] != verify(r['engine'], rooms[str(r['disc'])]):
                raise ValueError('engine reference differs')
            engine_results.append(r)
    for disc in [0, 1]:
        if len({r['trace_sha256'] for r in engine_results if r['disc'] == disc}) != 1:
            raise ValueError('engine ABI trace differs')
    imports = []
    for label, build in [('x86', 'host-x86'), ('x64', 'host-ninja')]:
        j = read(output/f'host-{label}-import-tests.json')
        if j['failed'] or sha((root/f'port/build/{build}/re4_asset_tool.exe').read_bytes()) != j['executable_sha256']:
            raise ValueError('stale/failing host import tests')
        imports.append(j)
    j = read(output/'android-import-tests.json')
    if len(j['results']) != 4:
        raise ValueError('incomplete repeated Android corpus')
    for r in j['results']:
        if r['failed'] or sha((root/f"port/build/android-{r['abi']}/re4_asset_tool").read_bytes()) != r['executable_sha256']:
            raise ValueError('stale/failing Android import tests')
        imports.append(r)
    if len({r['passed'] for r in imports}) != 1 or len({r['trace_sha256'] for r in j['results']}) != 1:
        raise ValueError('import corpus coverage/ABI trace differs')
    with (output/'resource-coverage.csv').open('w', encoding='utf-8', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    summary = dict(resource_cases=sum(r['passed'] for r in all_results),
                   engine_cases=sum(r['passed'] for r in engine_results),
                   import_cases=sum(r['passed'] for r in imports), failed=0,
                   isolated_checks_per_run=all_results[0]['isolated']['checks'],
                   catalog_sha256=catalog_hash,
                   semantic_traces={f"{disc}-{kind}": next(r['trace_sha256'] for r in all_results if r['disc'] == disc and r['kind'] == kind)
                                    for disc in [0, 1] for kind in ['resources', 'rooms']})
    write_json(output/'verification-summary.json', summary)
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
