#!/usr/bin/env python3
"""Test of the picture and GX comparisons of the 32/64-bit compare run (no disc data):
port/tools/picture_compare.py on PNG and PPM pictures (equal, one pixel off), and
gx_chunk_inspect.py diff on synthetic GX chunks (equal, only addresses moved, an XF value,
a vertex, texels). Exit code 0 when every case is judged as expected.

    port/tools/test_compare_tools.py
"""

import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent


def png(path, w, h, pixels):
    raw = b"".join(b"\0" + pixels[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def gx_chunk(path, xf=1.0, vertex=0.0, texel=0, address=0x80400000):
    ops = []
    ops += [7 | 256 << 8] + [r << 24 for r in range(256)]                  # BP state
    ops += [1 | 1 << 8, 0x94 << 24 | (address >> 5) & 0xFFFFFF]             # texture base: an address
    ops += [2 | 13 << 8, 0x0000] + [struct.unpack("<I", struct.pack("<f", v))[0]
                                    for v in (xf, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, -500)]
    blob = bytes([texel] + [0x11] * 15)
    ops += [4 | 12 << 8, 0, 0x1234, 0, 0, address, 4, 4, 1, 1, 0, 0, len(blob)]
    vertices = struct.pack("<9f", vertex, 0, 0, 10, 0, 0, 0, 10, 0)
    ops += [3 | 10 << 8, 0, 0, 12, 0, 3, 0, 0, 0, 0, 0]
    head = [1, 0, 1, len(ops), len(vertices), 0, len(blob), 0, 640, 448, 1, 3, len(blob), 301]
    path.write_bytes(b"RE4GXCH1" + struct.pack("<14I", *head) + struct.pack(f"<{len(ops)}I", *ops) + vertices + blob)


def run(args):
    r = subprocess.run([sys.executable, *map(str, args)], capture_output=True, text=True)
    return r.returncode, r.stdout


def main():
    tmp = Path(tempfile.mkdtemp(prefix="compare-tools-"))
    bad = 0

    def check(title, ok, out=""):
        nonlocal bad
        bad += not ok
        print(f"{'ok' if ok else 'FEHLER'}: {title}")
        if not ok:
            print("  " + out.replace("\n", "\n  "))

    a, b = tmp / "a", tmp / "b"
    a.mkdir()
    b.mkdir()
    w, h = 16, 8
    pixels = bytes((x * 13 + y * 7 + c) & 0xFF for y in range(h) for x in range(w) for c in range(3))
    png(a / "gx-000001.png", w, h, pixels)
    png(b / "gx-000001.png", w, h, pixels)
    (a / "gx-000002.ppm").write_bytes(b"P6\n%d %d\n255\n" % (w, h) + pixels)
    other = bytearray(pixels)
    other[(5 * w + 9) * 3 + 1] ^= 0x20
    (b / "gx-000002.ppm").write_bytes(b"P6\n%d %d\n255\n" % (w, h) + bytes(other))
    code, out = run([HERE / "picture_compare.py", a, b, "--list", tmp / "list.txt"])
    check("Bildvergleich: gleiches PNG, ein Pixel im PPM anders",
          code == 1 and "gx-000001: gleich" in out and "gx-000002: 1 Pixel verschieden (höchstens 32 von 255), im "
          "Bereich (9,5)-(9,5)" in out and (tmp / "list.txt").read_text() == "gx-000002\n", out)

    gx_chunk(tmp / "base.bin")
    cases = [
        ("GX gleich", {}, 0, "GX-Daten gleich"),
        ("GX: nur Adressen verschoben (64-bit-Heaps)", {"address": 0x80500000}, 0, "GX-Daten gleich"),
        ("GX: ein XF-Wert anders", {"xf": 1.0001}, 1, "Adresse 0000+0: 1 / 1.0001"),
        ("GX: eine Ecke anders", {"vertex": 0.25}, 1, "Ecken: 3 Ecken, größter Unterschied der Gleitkommawerte 0.25"),
        ("GX: Texel anders", {"texel": 7}, 1, "1 von 16 Bytes der Texel anders"),
    ]
    for title, kw, want, text in cases:
        gx_chunk(tmp / "other.bin", **kw)
        code, out = run([HERE / "gx_chunk_inspect.py", "diff", tmp / "base.bin", tmp / "other.bin"])
        check(title, code == want and text in out, out)
    print(f"compare tools: {1 + len(cases) - bad} von {1 + len(cases)} Fällen richtig beurteilt")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
