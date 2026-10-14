#!/usr/bin/env python3
"""Reproducible source inventory and optional ARM compiler probes; never builds the game."""
import argparse
import json
import re
import subprocess
import tempfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--compiler', help='NDK aarch64-linux-android21-clang++ executable')
p.add_argument('--output', default='docs/upstream-audit.json')
args = p.parse_args()
root = Path(__file__).resolve().parent.parent
upstream = root / 'upstream/re4'
sources = sorted((upstream / 'src').rglob('*'))
headers = sorted((upstream / 'include').rglob('*'))
files = [f for f in sources + headers if f.is_file()]
patterns = {
    'inline_asm': r'\basm\s*(?:volatile\s*)?\(',
    'powerpc_register_pin': r'\basm\s*\(\s*"r\d+"',
    '32bit_cast': r'\(\s*[us]32\s*\)',
    'hardware_address': r'\b0x(?:CC|CD|cc|cd)[0-9a-fA-F]{6}\b',
    'sdk_module_link': r'\bOS(?:Link|Unlink)\s*\(',
}
result = {
    'upstream_commit': '9b76ff003081d6ece8b838453fb8310743a972c7',
    'source_files': len([f for f in sources if f.is_file()]),
    'header_files': len([f for f in headers if f.is_file()]),
    'matches_are_candidates_not_proven_bugs': True,
    'patterns': {}, 'probes': [],
}
for name, pattern in patterns.items():
    hits = []
    for f in files:
        for n, line in enumerate(f.read_text(errors='replace').splitlines(), 1):
            if re.search(pattern, line):
                hits.append({'file': str(f.relative_to(root)), 'line': n, 'text': line.strip()[:200]})
    result['patterns'][name] = {'matching_lines': len(hits), 'examples': hits[:20]}
if args.compiler:
    with tempfile.TemporaryDirectory() as temp:
        probe = Path(temp) / 'type_probe.cpp'
        probe.write_text('#include "types.h"\nstatic_assert(sizeof(u32)==4,"u32 must have 32 bits");\n')
        for f in [probe] + [upstream / 'src/game' / n for n in ['rnd.cpp', 'pad.cpp', 'read.cpp', 'main.cpp', 'model.cpp']]:
            command = [args.compiler, '-std=c++17', '-fsyntax-only', '-ferror-limit=5',
                       '-I' + str(upstream / 'include'), str(f)]
            r = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
            text = r.stdout.replace(str(root) + '/', '').replace(temp, '<temporary>')
            result['probes'].append({'file': f.name, 'exit_code': r.returncode, 'diagnostics': text[:7000]})
out = root / args.output
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
print('Audit written:', out)
