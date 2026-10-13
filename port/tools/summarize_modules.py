#!/usr/bin/env python3
"""Verify current binaries and all four ABI module reports; write private coverage."""
import argparse
import csv
import json
import re
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from original_import.pipeline import external_root, sha, write_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--modules', required=True)
    parser.add_argument('--resources', required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = external_root(args.modules, root, [])
    resources = external_root(args.resources, root, [])
    read = lambda p: json.loads(p.read_text(encoding='utf-8'))
    manifest = read(root/'port/build/host-ninja/original/module-manifest.json')
    if read(output/'module-manifest.json') != manifest:
        raise ValueError('stale private module manifest')
    cases = read(output/'module-cases.json')
    regressions = read(resources/'verification-summary.json')
    if regressions['failed'] or [regressions[k] for k in
            ['resource_cases', 'engine_cases', 'import_cases']] != [3600, 852, 4548]:
        raise ValueError('incomplete previous-phase regression evidence')
    results, traces, counts, core_traces = [], set(), set(), set()
    platforms = [('host-32', 'host-x86', 32), ('host-64', 'host-ninja', 64),
                 ('armeabi-v7a', 'android-armeabi-v7a', 32),
                 ('arm64-v8a', 'android-arm64-v8a', 64)]
    apk_hash = sha((root/'port/android/app/build/outputs/apk/debug/app-debug.apk').read_bytes())
    for label, build, bits in platforms:
        report = read(output/f'{label}-modules.json')
        exe = root/f'port/build/{build}/re4_module_tool'
        if label.startswith('host'):
            exe = exe.with_suffix('.exe')
        exe_hash = sha(exe.read_bytes())
        if report['module_catalog_sha256'] != manifest['sha256'] or len(report['results']) != 4:
            raise ValueError('incomplete module report')
        seen = set()
        for r in report['results']:
            identity = (r['round'], r['disc'])
            if identity in seen or r['round'] not in [1, 2] or str(r['disc']) not in cases:
                raise ValueError('duplicate/invalid module round')
            seen.add(identity)
            isolated, actual = r['isolated'], r['result']
            if r['platform'] != label or r['failed'] or r['executable_sha256'] != exe_hash:
                raise ValueError('stale/failing module executable')
            if isolated['pointer_bits'] != bits or actual['pointer_bits'] != bits:
                raise ValueError('actual module ABI differs')
            if isolated['catalog_sha256'] != manifest['sha256'] or actual['catalog_sha256'] != manifest['sha256']:
                raise ValueError('module source catalog differs')
            if isolated['failed'] or isolated['checks'] < 215 or isolated['original_registration_components'] != 14 or isolated['original_init_main_pairs'] != 2:
                raise ValueError('missing original execution/lifecycle evidence')
            expected = [{k: v for k, v in c.items() if k != 'path'} for c in cases[str(r['disc'])]]
            if actual['modules'] != expected or r['passed'] != len(expected):
                raise ValueError('original REL reference hashes/bounds/dispatch differ')
            trace = sha(json.dumps(actual['modules'], sort_keys=True, separators=(',', ':')).encode())
            if trace != r['trace_sha256']:
                raise ValueError('reported module trace differs')
            counts.add(isolated['checks'])
            traces.add(tuple(isolated['trace']))
            results.append(r)
        if not label.startswith('host'):
            log = (root/f'port/build/instrumentation-{label}.txt').read_text(encoding='utf-8-sig')
            if 'OK (15 tests)' not in log or f'primaryCpuAbi={label}' not in log or apk_hash != re.search(r'ApkSHA256=([0-9a-fA-F]+)', log)[1].lower():
                raise ValueError('stale/failing app-process proof')
            for round_no in [1, 2]:
                log = (root/f'port/build/device-{label}-round{round_no}.txt').read_text(encoding='utf-8-sig')
                core = root/f'port/build/{build}/re4_core_tests'
                total = 13386 if bits == 32 else 13387
                if f'pointer={bits} bits;' not in log or f'TOTAL: {total} checks, 0 failures' not in log or sha(core.read_bytes()) != re.search(r'ExecutableSHA256=([0-9a-fA-F]+)', log)[1].lower():
                    raise ValueError('stale/failing native regression proof')
                recorded = tuple(re.findall(r'^\s*(TRACE (?:scheduler_|background|yz2).+)$', log, re.M))
                if len(recorded) != 7:
                    raise ValueError('missing native regression traces')
                core_traces.add(recorded)
    if len(traces) != 1 or len(counts) != 1:
        raise ValueError('isolated lifecycle semantics/count differs across ABIs')
    if len(core_traces) != 1:
        raise ValueError('native core ABI/repeat semantics differ')
    for disc in [0, 1]:
        if len({r['trace_sha256'] for r in results if r['disc'] == disc}) != 1:
            raise ValueError('original REL ABI/repeat semantics differ')
    def coverage(path, rows):
        with (output/path).open('w', encoding='utf-8', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)
    coverage('room-module-coverage.csv', [dict(room=f"r{r['id']:03x}",
        file_index=r['file_index'], module_id=r['module'], init=r['init'], main=r['main'],
        variants=';'.join(r['variants']), native_pair=r['compiled']) for r in manifest['rooms']])
    coverage('module-coverage.csv', [dict(module_id=m['module_id'], name=m['name'],
        file_index=m['file_index'], imports=';'.join(map(str, m['imports'])),
        coverage='partial original stage registration' if re.fullmatch(r'st[1-4]_\d+', m['name'])
        else 'REL metadata verified; native gameplay unit not compiled') for m in manifest['modules']])
    summary = dict(module_cases=sum(r['passed'] for r in results),
        malformed_rel_rejections=sum(c['rejected'] for r in results for c in r['result']['modules']),
        original_pair_lifecycles=sum(c['original_pairs'] for r in results for c in r['result']['modules']),
        isolated_checks_per_run=next(iter(counts)), modules=len(manifest['modules']),
        room_bindings=len(manifest['rooms']), original_registration_components=14,
        compiled_room_pairs=sum(r['compiled'] for r in manifest['rooms']), failed=0,
        regression_cases=sum(regressions[k] for k in ['resource_cases','engine_cases','import_cases']),
        module_catalog_sha256=manifest['sha256'], semantic_trace=list(next(iter(traces))),
        device=read(output/'module-device.json')['device'])
    write_json(output/'module-verification-summary.json', summary)
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
