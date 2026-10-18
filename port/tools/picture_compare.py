#!/usr/bin/env python3
"""Compares the pictures of two builds pixel by pixel (the 32/64-bit compare run of the PC check:
bilder-vergleich/i686 and bilder-vergleich/x86_64, the same frames drawn from each side's GX data
by gx_chunk_render). Reads 8-bit RGB PNG (as whole_r100_check.sh writes them) and PPM (P6), with
nothing but the standard library.

    port/tools/picture_compare.py DIR_A DIR_B [--names i686,x86_64] [--list OUT]

Per picture present on both sides: "gleich", or how many pixels differ, by how much at most, and
the rectangle they lie in. --list writes the names (without extension) of the differing pictures,
one per line. Exit code 0 when all agree, 1 otherwise, 2 on bad input.
"""

import argparse
import struct
import sys
import zlib
from pathlib import Path


def read_png(path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("no PNG")
    at, idat, w = 8, b"", None
    while at < len(data):
        n, kind = struct.unpack_from(">I4s", data, at)
        body = data[at + 8:at + 8 + n]
        if kind == b"IHDR":
            w, h, depth, ctype = struct.unpack_from(">IIBB", body)
            if depth != 8 or ctype not in (2, 6):
                raise ValueError(f"PNG depth {depth} colour type {ctype} not supported")
            bpp = 3 if ctype == 2 else 4
        elif kind == b"IDAT":
            idat += body
        at += 12 + n
    raw = zlib.decompress(idat)
    row = w * bpp
    out = bytearray(w * h * 3)
    prev = bytearray(row)
    for y in range(h):
        f = raw[y * (row + 1)]
        line = bytearray(raw[y * (row + 1) + 1:(y + 1) * (row + 1)])
        for x in range(row):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 0xFF
            elif f == 2:
                line[x] = (line[x] + b) & 0xFF
            elif f == 3:
                line[x] = (line[x] + (a + b) // 2) & 0xFF
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        prev = line
        if bpp == 3:
            out[y * w * 3:(y + 1) * w * 3] = line
        else:
            for x in range(w):
                out[(y * w + x) * 3:(y * w + x) * 3 + 3] = line[x * 4:x * 4 + 3]
    return w, h, bytes(out)


def read_ppm(path):
    data = path.read_bytes()
    parts = data.split(b"\n", 3)
    if parts[0] != b"P6":
        raise ValueError("no PPM (P6)")
    w, h = map(int, parts[1].split())
    return w, h, parts[3][:w * h * 3]


def read(path):
    return read_png(path) if path.suffix.lower() == ".png" else read_ppm(path)


def compare(a, b):
    """None when equal, else (pixels, largest difference, (x0, y0, x1, y1))"""
    wa, ha, pa = a
    wb, hb, pb = b
    if (wa, ha) != (wb, hb):
        return ("size", (wa, ha), (wb, hb))
    if pa == pb:
        return None
    count, worst = 0, 0
    x0, y0, x1, y1 = wa, ha, -1, -1
    for i in range(0, len(pa), 3):
        d = max(abs(pa[i] - pb[i]), abs(pa[i + 1] - pb[i + 1]), abs(pa[i + 2] - pb[i + 2]))
        if d:
            count += 1
            worst = max(worst, d)
            p = i // 3
            x, y = p % wa, p // wa
            x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x), max(y1, y)
    return (count, worst, (x0, y0, x1, y1))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--names", default="i686,x86_64")
    ap.add_argument("--list", help="write the names of the differing pictures to this file")
    args = ap.parse_args()
    na, nb = (args.names.split(",") + ["2"])[:2]
    da, db = Path(args.a), Path(args.b)
    pics = {p.stem: p for p in sorted(da.glob("*")) if p.suffix.lower() in (".png", ".ppm")}
    both = [s for s in pics if any((db / (s + e)).exists() for e in (".png", ".ppm"))]
    if not both:
        print(f"  Bildvergleich: keine Bilder auf beiden Seiten ({da}, {db})")
        return 2
    differ = []
    for stem in both:
        other = next(db / (stem + e) for e in (".png", ".ppm") if (db / (stem + e)).exists())
        try:
            r = compare(read(pics[stem]), read(other))
        except (ValueError, OSError, zlib.error, struct.error) as e:
            print(f"  {stem}: nicht lesbar ({e})")
            differ.append(stem)
            continue
        if r is None:
            print(f"  {stem}: gleich")
        elif r[0] == "size":
            print(f"  {stem}: andere Größe {r[1][0]}x{r[1][1]} / {r[2][0]}x{r[2][1]}")
            differ.append(stem)
        else:
            count, worst, (x0, y0, x1, y1) = r
            print(f"  {stem}: {count} Pixel verschieden (höchstens {worst} von 255), im Bereich "
                  f"({x0},{y0})-({x1},{y1})")
            differ.append(stem)
    print(f"  Bildvergleich {na} / {nb}: {len(both) - len(differ)} von {len(both)} Bildern gleich")
    if args.list:
        Path(args.list).write_text("".join(s + "\n" for s in differ))
    return 1 if differ else 0


if __name__ == "__main__":
    sys.exit(main())
