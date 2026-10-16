#!/usr/bin/env python3
"""Pictures and a summary of a movie played by `re4_host --movie-test` (M9).

The frame records of tests/movie_probe.cpp (planar YUV 4:2:0 of every new
frame) become PNG stills, one every SECONDS of play (by the field the frame
appeared in) plus the first and the last frame; YCbCr is turned into RGB
with the BT.601 studio range, as MPEG-1 video is coded. The summary line
names the frames, the frame numbers that are missing in between (skipped)
and the frames per second.

    movie_report.py FRAMES.yuv OUT_DIR NAME [--every SECONDS] [--sparse]

--sparse: the probe kept only some frames (RE4_MOVIE_EVERY); every record
becomes a still and the summary names no frame rate.
"""
import argparse
import struct
import sys
import zlib
from pathlib import Path


def png(path, width, height, rgb):
    raw = b"".join(b"\0" + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))

    def chunk(kind, data):
        c = kind + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
                           + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def to_rgb(planes, w, h):
    y = planes[:w * h]
    cb = planes[w * h:w * h + (w // 2) * (h // 2)]
    cr = planes[w * h + (w // 2) * (h // 2):]
    out = bytearray(w * h * 3)
    cw = w // 2
    for j in range(h):
        row = j * w
        crow = (j // 2) * cw
        for i in range(w):
            yy = 1.164 * (y[row + i] - 16)
            u = cb[crow + i // 2] - 128
            v = cr[crow + i // 2] - 128
            r = yy + 1.596 * v
            g = yy - 0.392 * u - 0.813 * v
            b = yy + 2.017 * u
            k = (row + i) * 3
            out[k] = min(255, max(0, int(r + 0.5)))
            out[k + 1] = min(255, max(0, int(g + 0.5)))
            out[k + 2] = min(255, max(0, int(b + 0.5)))
    return bytes(out)


def frames(path):
    data = Path(path).read_bytes()
    pos = 0
    while pos + 16 <= len(data):
        magic, fno, field, size = struct.unpack("<IIII", data[pos:pos + 16])
        w, h = size & 0xFFFF, size >> 16
        n = w * h * 3 // 2
        if magic != 0x4D5246 or pos + 16 + n > len(data):
            break
        yield fno, field, w, h, data[pos + 16:pos + 16 + n]
        pos += 16 + n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("yuv")
    ap.add_argument("out")
    ap.add_argument("name")
    ap.add_argument("--every", type=float, default=2.0)
    ap.add_argument("--sparse", action="store_true")
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    every = int(args.every * 59.94)
    count, last, gaps, first_field, last_field = 0, None, 0, None, None
    next_still = 0
    kept = 0
    final = None
    for fno, field, w, h, planes in frames(args.yuv):
        count += 1
        if last is not None and fno > last + 1:
            gaps += fno - last - 1
        if first_field is None:
            first_field = field
        last_field = field
        last = fno
        final = (fno, w, h, planes)
        if args.sparse or field >= next_still:
            png(out / f"{args.name}-{fno:05d}.png", w, h, to_rgb(planes, w, h))
            kept += 1
            next_still = field + every
    if final and kept and not (out / f"{args.name}-{final[0]:05d}.png").exists():
        png(out / f"{args.name}-{final[0]:05d}.png", final[1], final[2], to_rgb(final[3], final[1], final[2]))
        kept += 1
    if not count:
        print(f"{args.name}: no frames")
        return 1
    if args.sparse:
        print(f"{args.name}: {kept} pictures ({final[1]} x {final[2]}), frame numbers to {last}")
        return 0
    seconds = max(1, last_field - first_field) / 59.94
    print(f"{args.name}: {count} frames ({final[1]} x {final[2]}), numbers to {last}, {gaps} missing in between, "
          f"{count / seconds:.2f} frames per second, first at field {first_field}, {kept} pictures")
    return 0


if __name__ == "__main__":
    sys.exit(main())
