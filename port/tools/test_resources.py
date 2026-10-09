#!/usr/bin/env python3
"""Phase-3c original DC/ARAM and real-room tests. All asset/report output is private."""
import argparse
import json
import re
import sys
import uuid
from pathlib import Path
sys.dont_write_bytecode = True
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
    if not 1 <= args.repeat <= 10 or not (args.host_exe or args.adb):
        raise ValueError('an executable/device and repeat 1..10 are required')
    root = Path(__file__).resolve().parents[2]
    data = external_root(args.data, root, [])
    output = external_root(args.output, root, [])
    if data == output:
        raise ValueError('preserve the Phase-3a input/evidence directory')
    cases = prepare(data, output, True)
    manifest = json.loads((data/'manifest.json').read_text())
    selected = {}
    for disc in cases:
        group = []
        families = {}
        # Selection is entirely inventory driven, includes opaque resources as
        # byte-residency tests (no claim of a sound/model/animation interpreter).
        for f in sorted(manifest['files'], key=lambda f: f['path']):
            if str(f['disc_number']) != disc or not 0 < f['size'] <= 5*1024*1024 or len(f['path']) >= 32:
                continue
            family = f['path'].split('/')[0].lower()
            if families.get(family, 0) >= 2:
                continue
            families[family] = families.get(family, 0)+1
            group.append(dict(path=f['path'], sha256=f['sha256'], size=f['size']))
        for room in cases[disc]:
            for b in room['blocks']:
                if not any(f['path'] == b['path'] for f in group):
                    group.append({k: b[k] for k in ['path', 'sha256', 'size']})
        selected[disc] = group
    write_json(output/'resource-cases.json', selected)
    catalog = output/'engine-catalog.tsv'
    digests = sorted({r['source_sha256'] for g in cases.values() for r in g} |
                     {b['sha256'] for g in cases.values() for r in g for b in r['blocks']} |
                     {f['sha256'] for g in selected.values() for f in g})
    for digest in digests:
        if sha((data/'blobs'/digest).read_bytes()) != digest:
            raise ValueError('private original input hash differs')

    def verify(kind, actual, expected, bits):
        if actual['pointer_bits'] != bits:
            raise ValueError('actual process ABI differs')
        if kind == 'rooms':
            if len(actual['rooms']) != len(expected):
                raise ValueError('room count differs')
            for a, e in zip(actual['rooms'], expected):
                if a['id'] != e['id'] or a['body_sha256'] != e['body_sha256'] or a['block_transitions'] != len(e['blocks']):
                    raise ValueError('real room/block lifetime output differs')
                if e['body_size'] <= 16*1024*1024 and not a['decoded_cache_reused']:
                    raise ValueError('eligible room body was not reused')
        else:
            if len(actual['resources']) != len(expected):
                raise ValueError('resource count differs')
            for a, e in zip(actual['resources'], expected):
                if a['path'] != e['path'] or a['sha256'] != e['sha256'] or a['states'] != [1, 2, 6, 4, 5, 2]:
                    raise ValueError('real resource integrity/state differs')
            for name in ['void cDataCtrl::check()', 'void cDataUnit::checkMramToAram()',
                         'void cDataUnit::checkAramToMram()']:
                if name not in actual['functions']:
                    raise ValueError('original controller path not executed')
        # Addresses, timing/work/caching counters are measurements, not ABI semantics.
        return sha(json.dumps(actual[kind], sort_keys=True, separators=(',', ':')).encode())

    results = []
    def exercise(label, execute, bits, exe_hash):
        for round_no in range(1, args.repeat+1):
            isolated = json.loads(execute('self-test'))
            if isolated['pointer_bits'] != bits:
                raise ValueError('isolated process ABI differs')
            for disc in cases:
                for kind, expected in [('resources', selected[disc]), ('rooms', cases[disc])]:
                    actual = json.loads(execute(kind, disc, *[e['path' if kind == 'resources' else 'id'] for e in expected]))
                    trace = verify(kind, actual, expected, bits)
                    results.append(dict(platform=label, pointer_bits=bits, round=round_no,
                                        disc=int(disc), kind=kind, passed=len(expected), failed=0,
                                        trace_sha256=trace, isolated=isolated, result=actual,
                                        executable_sha256=exe_hash))
                    write_json(output/f'{label}-resources.json', dict(results=[r for r in results if r['platform'] == label],
                               catalog_sha256=sha(catalog.read_bytes())))
                    print(f'PASS {label} round {round_no} disc {disc}: {len(expected)} {kind}', flush=True)
    if args.host_exe:
        exe = Path(args.host_exe).resolve()
        isolated = json.loads(run([exe, 'self-test', exe.parent/'resource-tests']))
        bits = isolated['pointer_bits']
        def host(kind, *parts):
            return run([exe, kind, exe.parent/'resource-tests'] if kind == 'self-test'
                       else [exe, kind, data, catalog, *parts])
        exercise(f'host-{bits}', host, bits, sha(exe.read_bytes()))
    if args.adb:
        if not args.serial or not re.fullmatch('[A-Za-z0-9_.:-]+', args.serial):
            raise ValueError('invalid device serial')
        def adb(*parts):
            return run([args.adb, '-s', args.serial, *parts])
        info = dict(serial=args.serial, model=adb('shell', 'getprop', 'ro.product.model'),
                    android=adb('shell', 'getprop', 'ro.build.version.release'),
                    api=adb('shell', 'getprop', 'ro.build.version.sdk'),
                    abis=adb('shell', 'getprop', 'ro.product.cpu.abilist').split(','))
        remote = '/data/local/tmp/re4-resources-'+uuid.uuid4().hex
        adb('shell', 'mkdir', '-p', remote+'/blobs')
        adb('push', catalog, remote+'/catalog.tsv')
        if adb('shell', 'sha256sum', remote+'/catalog.tsv').split()[0] != sha(catalog.read_bytes()):
            raise ValueError('device catalog differs')
        write_json(output/'resource-device.json', dict(device=info, remote_directory=remote))
        print(f'{info}: deploying {len(digests)} verified original blobs', flush=True)
        for n, digest in enumerate(digests):
            adb('push', data/'blobs'/digest, remote+'/blobs/'+digest)
            if adb('shell', 'sha256sum', remote+'/blobs/'+digest).split()[0] != digest:
                raise ValueError('device blob integrity differs')
            if (n+1) % 40 == 0:
                print(f'deploy {n+1}/{len(digests)}', flush=True)
        for abi, bits in [('armeabi-v7a', 32), ('arm64-v8a', 64)]:
            if abi not in info['abis']:
                raise ValueError('required ABI unavailable')
            exe = root/f'port/build/android-{abi}/re4_resource_tool'
            target = remote+'/'+abi
            digest = sha(exe.read_bytes())
            adb('push', exe, target)
            adb('shell', 'chmod', '755', target)
            if adb('shell', 'sha256sum', target).split()[0] != digest:
                raise ValueError('device test executable differs')
            def device(kind, *parts):
                return adb('shell', target, kind, remote+'/synthetic-'+abi) if kind == 'self-test' else adb('shell', target, kind, remote, remote+'/catalog.tsv', *parts)
            exercise(abi, device, bits, digest)
    for kind in ['resources', 'rooms']:
        for disc in cases:
            if len({r['trace_sha256'] for r in results if r['kind'] == kind and str(r['disc']) == disc}) != 1:
                raise ValueError('repeated ABI semantic trace differs')


if __name__ == '__main__':
    main()
