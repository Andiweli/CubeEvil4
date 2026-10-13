#!/usr/bin/env python3
"""Validate measured ABI/repetition evidence; emit metadata only, no game assets."""
import argparse
import json
from pathlib import Path
from original_import.pipeline import external_root, write_json
from analyze_controller_capture import require_standard_controls


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True, type=Path)
    parser.add_argument('--device', required=True, type=Path)
    parser.add_argument('--controllers', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    output = external_root(args.output, Path(__file__).resolve().parents[2], [])
    traces, counts, isolated_traces = {}, set(), set()
    results = []
    for abi, bits, directory in [('host-32', 32, args.host), ('host-64', 64, args.host),
                                 ('armeabi-v7a', 32, args.device), ('arm64-v8a', 64, args.device)]:
        document = json.loads((directory / ('game-runtime-' + abi + '.json')).read_text())
        records = document['results']
        identities = [(r['round'], r['disc']) for r in records]
        if len(identities) != len(set(identities)) or set(identities) != {(n, d) for n in (1, 2) for d in (0, 1)}:
            raise ValueError('missing/duplicated two-round disc results: ' + abi)
        for r in records:
            if r['abi'] != abi or r['failed'] or r['isolated']['failed'] or r['isolated']['pointer_bits'] != bits or r['actual']['pointer_bits'] != bits:
                raise ValueError('actual process ABI or test failure: ' + abi)
            traces.setdefault(r['disc'], set()).add(r['trace_sha256'])
            counts.add(r['isolated']['checks'])
            isolated_traces.add(json.dumps(r['isolated']['trace']))
        results.append(dict(abi=abi, pointer_bits=bits, rounds=2, passed=sum(r['passed'] for r in records),
                            failed=0, executable_sha256=sorted({r['executable_sha256'] for r in records})))
    if any(len(v) != 1 for v in traces.values()) or len(counts) != 1 or len(isolated_traces) != 1:
        raise ValueError('original runtime results differ across ABI/repetition')
    physical = []
    for bits in (32, 64):
        report = json.loads((args.controllers / ('controller-arm' + str(bits) + '-corrected.json')).read_text())
        if report['pointer_bits'] != bits:
            raise ValueError('physical process ABI differs')
        require_standard_controls(report)
        physical.append(dict(pointer_bits=bits, human_confirmed=True, raw_events=report['raw_events'],
                             native_snapshots=report['native_snapshots'], required_controls_verified=True,
                             native_trigger_levels=[len(v) for v in report['native_trigger_values']]))
    summary = dict(runtime_checks_per_run=next(iter(counts)), real_data_preparations=results,
                   identical_data_traces={str(k):next(iter(v)) for k, v in traces.items()},
                   identical_isolated_traces=True, physical_controller_captures=physical,
                   scope='Partial original runtime, isolated consumers, real archive/ESL preparation; full GameTask/r120 event blocked')
    write_json(output / 'game-runtime-summary.json', summary)
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
