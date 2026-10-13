"""Synthetic fixtures, independently checked by the repository's PPC reference.

The encoder reverses the decoder's exact range operations; no original assets.
--check recomputes all fixtures and verifies checked-in bytes/manifest unchanged.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'motion'))
import yz2  # Existing original-assembly-derived reference, never modified.


def update(m, s):
    m.freq[s] += 1
    m.total += 1
    if m.bits <= 14 and m.total == m.range:
        shift = 15 - m.bits
        m.cum_f = [v << shift for v in m.freq]
        m.bits += 1
        m.range = 1 << m.bits
    elif m.bits > 14 and m.total > 0x7fff:
        m.cum_f = m.freq[:]
        m.freq = [max(1, v >> 1) for v in m.freq]
        m.total = sum(m.freq)
    else:
        return
    total = 0
    m.cum_s = []
    for v in m.cum_f:
        m.cum_s.append(total)
        total += v
    assert total == 0x8000


def encode(operations, expected):
    models = [yz2._Model(yz2.N_MAIN), yz2._Model(yz2.N_SIDE)]
    ranges = []
    r = 0x80
    for which, s in operations:
        count = 0
        if r <= 0x800000:
            count = 1 if r > 0x8000 else 2 if r > 0x80 else 3
            r = (r << (count * 8)) & 0xffffffff
        m = models[which]
        step = r >> 14
        start = step * m.cum_s[s]
        ranges.append((count, start, r))
        r = (step * m.cum_f[s]) >> 1
        update(m, s)
    # Reverse every subtract/renormalize operation. Choosing the final residual
    # zero yields an in-range code; extracted low bytes become stream bytes.
    c = 0
    pieces = []
    for count, start, normal_range in reversed(ranges):
        c += start
        # R uses a half-range convention (cum has 2**15 entries, R>>14).
        assert c < 2 * normal_range and c <= 0xffffffff
        pieces.append((c & ((1 << (count * 8)) - 1)).to_bytes(count, 'big'))
        c >>= count * 8
    assert c <= 0xff
    stream = bytes([c]) + b''.join(reversed(pieces))
    header = f'{len(stream):X},{len(expected):X}'.encode('ascii')
    image = header + bytes(((len(header) + 32) & ~31) - len(header)) + stream
    decoded, packed, used = yz2.decode(image)
    assert decoded == expected and packed == len(stream) and used == len(stream)
    return image


def fixtures():
    literals = bytes(range(256))
    yield 'literals', encode([(0, 0x400 + b) for b in literals], literals), literals
    ops = [(0, 0x441), (0, 0x441)]
    expected = bytearray(b'AA')
    # Newest dictionary slot: (0x1ff + cnt) & 0x1ff = cnt - 1.
    ops += [(0, 0x1ff), (1, 7)]  # overlapping run of 6, output now 8 As
    expected += b'A' * 6
    ops += [(0, 0x3ff)]  # stored length (6), output now 14 As
    expected += b'A' * 6
    for prefix, data in [(2, [0, 5]), (1, [0, 0, 5]), (0, [0, 0, 0, 5])]:
        ops += [(0, 0x1ff), (1, prefix)] + [(1, b) for b in data]
        expected += b'A' * 4
    yield 'matches', encode(ops, expected), bytes(expected)
    # Both model-halving paths and dictionary wrap; generated data stays tiny.
    ops = [(0, 0x441), (0, 0x441)] + [(which, s) for _ in range(35000)
        for which, s in [(0, 0x1ff), (1, 3)]]
    large = b'A' * 70002
    yield 'adaptive', encode(ops, large), large
def generated():
    lines = ['// Generated synthetic data; regenerate: port/tools/generate_yz2_fixtures.py',
             '#pragma once', '#include <cstddef>', '#include <cstdint>',
             'namespace re4::port::fixtures {']
    manifest = {'reference': 'tools/motion/yz2.py', 'fixtures': []}
    for name, image, expected in fixtures():
        lines.append(f'inline constexpr std::uint8_t {name}[] = {{')
        for i in range(0, len(image), 16):
            lines.append('    ' + ','.join(f'0x{b:02X}' for b in image[i:i+16]) + ',')
        lines.append('};')
        lines.append(f'inline constexpr std::size_t {name}OutputBytes = {len(expected)};')
        manifest['fixtures'].append({'name': name, 'input_bytes': len(image),
            'output_bytes': len(expected), 'input_sha256': hashlib.sha256(image).hexdigest(),
            'output_sha256': hashlib.sha256(expected).hexdigest()})
    lines.append('} // namespace re4::port::fixtures')
    return '\n'.join(lines) + '\n', json.dumps(manifest, indent=2) + '\n'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    header, manifest = generated()
    for relative, content in [('port/tests/yz2_fixtures.h', header),
                              ('port/tests/yz2_fixtures.json', manifest)]:
        path = ROOT / relative
        if args.check:
            assert path.read_text(encoding='utf-8') == content, f'Stale fixture: {relative}'
        else:
            path.write_text(content, encoding='utf-8', newline='\n')
    print('PASS: 3 synthetic fixtures match tools/motion/yz2.py byte for byte; manifest verified')


if __name__ == '__main__':
    main()
