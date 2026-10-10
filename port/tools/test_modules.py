#!/usr/bin/env python3
"""Native module execution and original REL evidence on hosts/Android; assets stay private."""
import argparse
import json
import re
import struct
import sys
import uuid
from pathlib import Path
sys.dont_write_bytecode = True
from original_import.pipeline import external_root, sha, write_json
from original_import.formats import validate_rel
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
    if not 1 <= args.repeat <= 10 or not (args.host_exe or args.adb): raise ValueError('executable/device and repeat 1..10 required')
    root = Path(__file__).resolve().parents[2]
    data, output = external_root(args.data, root, []), external_root(args.output, root, [])
    if output == data: raise ValueError('preserve original import evidence')
    prepare(data, output, True)
    sys.path.insert(0, str(root/'tools'))
    from drs import Drs
    manifest = json.loads((data/'manifest.json').read_text())
    source_manifest = root/'port/build/host-ninja/original/module-manifest.json'
    config = json.loads(source_manifest.read_text())
    cases = {}
    for m in config['modules']:
        path = m['object'].removeprefix('files/')
        if path.startswith('em/'): path = path.removesuffix('.rel')+'.drs' # tools/extract_orig.py's archive REL rule
        found = [f for f in manifest['files'] if f['path'].lower() == path.lower()]
        if not found: raise ValueError('configured original module data not found: '+path)
        f = sorted(found, key=lambda f: f['disc_number'])[0]
        raw = (data/'blobs'/f['sha256']).read_bytes()
        if sha(raw) != f['sha256']: raise ValueError('original file integrity differs')
        rel = Drs(raw).rel if path.endswith('.drs') else raw
        index = validate_rel(rel)
        if index['module_id'] != m['module_id'] or set(index['imports'])-{0,m['module_id']} != set(m['imports']): raise ValueError('original REL/config imports differ')
        exact = rel[:index['exact_size']]
        empty = []
        for section in ['.ctors', '.dtors']:
            off, size = struct.unpack_from('>II', exact, 0x4c+8*m['sections'][section])
            empty.append(size == 4 and exact[off&~3:(off&~3)+size] == b'\0'*4)
        if m['name'].startswith('st') and empty != [True, True]: raise ValueError('stage requires original constructor/destructor bindings')
        pairs = sum(r['compiled'] and not r['resident_imports'] and r['module'] == m['module_id'] for r in config['rooms'])
        blockers = sum(r['resident_imports'] and r['module'] == m['module_id'] for r in config['rooms'])
        cases.setdefault(str(f['disc_number']), []).append(dict(id=m['module_id'], path=f['path'], file_sha256=f['sha256'],
            rel_sha256=sha(exact), rel_bytes=len(exact), bss_bytes=struct.unpack_from('>I',exact,0x20)[0], rejected=7, original_pairs=pairs,
            resident_import_blockers=blockers))
    write_json(output/'module-cases.json', cases)
    write_json(output/'module-manifest.json', config)
    results = []
    def exercise(label, exe_hash, bits, execute):
        for round_no in range(1, args.repeat+1):
            isolated = json.loads(execute('self-test'))
            if isolated['pointer_bits'] != bits or isolated['failed'] or isolated['checks'] < 210 or isolated['catalog_sha256'] != config['sha256']: raise ValueError('module isolated proof differs')
            for disc, expected in cases.items():
                actual = json.loads(execute('data', disc, *[str(c['id'])+':'+c['path'] for c in expected]))
                if actual['pointer_bits'] != bits or actual['catalog_sha256'] != config['sha256']: raise ValueError('actual module ABI/catalog differs')
                comparison = [{k:v for k,v in c.items() if k != 'path'} for c in expected]
                if actual['modules'] != comparison: raise ValueError('real REL hashes, lifecycle or bounds differ')
                record = dict(platform=label, round=round_no, disc=int(disc), passed=len(expected), failed=0,
                    executable_sha256=exe_hash, isolated=isolated, result=actual,
                    trace_sha256=sha(json.dumps(actual['modules'],sort_keys=True,separators=(',',':')).encode()))
                results.append(record)
                write_json(output/f'{label}-modules.json', dict(results=[r for r in results if r['platform']==label], module_catalog_sha256=config['sha256']))
                print(f'PASS {label} round {round_no} disc {disc}: {len(expected)} original RELs, partial native stage registration/Init execution', flush=True)
    if args.host_exe:
        exe = Path(args.host_exe).resolve()
        def host(mode, *parts): return run([exe, mode, exe.parent/'module-tests'] if mode == 'self-test' else [exe, mode, data, output/'engine-catalog.tsv', *parts])
        bits = json.loads(host('self-test'))['pointer_bits']
        exercise(f'host-{bits}', sha(exe.read_bytes()), bits, host)
    if args.adb:
        if not args.serial or not re.fullmatch('[A-Za-z0-9_.:-]+', args.serial): raise ValueError('device serial required')
        def adb(*parts): return run([args.adb, '-s', args.serial, *parts])
        device = dict(serial=args.serial, model=adb('shell','getprop','ro.product.model'), android=adb('shell','getprop','ro.build.version.release'),
                      api=adb('shell','getprop','ro.build.version.sdk'), abis=adb('shell','getprop','ro.product.cpu.abilist').split(','))
        remote = '/data/local/tmp/re4-modules-'+uuid.uuid4().hex
        adb('shell','mkdir','-p',remote+'/blobs')
        catalog = output/'engine-catalog.tsv'; adb('push',catalog,remote+'/catalog.tsv')
        if adb('shell','sha256sum',remote+'/catalog.tsv').split()[0] != sha(catalog.read_bytes()): raise ValueError('device catalog differs')
        write_json(output/'module-device.json',dict(device=device,remote_directory=remote))
        for digest in sorted({c['file_sha256'] for group in cases.values() for c in group}):
            adb('push',data/'blobs'/digest,remote+'/blobs/'+digest)
            if adb('shell','sha256sum',remote+'/blobs/'+digest).split()[0] != digest: raise ValueError('device module source hash differs')
        for abi, bits in [('armeabi-v7a',32),('arm64-v8a',64)]:
            if abi not in device['abis']: raise ValueError('required Android ABI unsupported')
            exe = root/f'port/build/android-{abi}/re4_module_tool'; target = remote+'/'+abi
            exe_hash = sha(exe.read_bytes()); adb('push',exe,target); adb('shell','chmod','755',target)
            if adb('shell','sha256sum',target).split()[0] != exe_hash: raise ValueError('actual device executable differs')
            def execute(mode, *parts): return adb('shell',target,mode,remote+'/fixture-'+abi) if mode == 'self-test' else adb('shell',target,mode,remote,remote+'/catalog.tsv',*parts)
            exercise(abi,exe_hash,bits,execute)
    for disc in cases:
        if len({r['trace_sha256'] for r in results if str(r['disc']) == disc}) != 1: raise ValueError('module ABI/repeat data/state trace differs')


if __name__ == '__main__': main()
