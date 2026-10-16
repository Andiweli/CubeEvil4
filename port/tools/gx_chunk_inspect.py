#!/usr/bin/env python3
"""Shows what a dumped GX chunk does (gx-*.bin, re4_gx_frame_io.h): the GX data
of the app (R3, hits) or of the PC check (RE4_GX_DUMP).

    gx_chunk_inspect.py copies CHUNK.bin
        EFB copies (source rectangle, size, destination, format, half size,
        display copy) and the textures bound from a copy, by draw number
    gx_chunk_inspect.py draws CHUNK.bin FIRST LAST
        the draws FIRST..LAST: primitive, vertex layout, the first 4 vertices
        (position, colours, texture coordinates), the texture binds and the BP
        and short XF writes before each

Found with it (16.10.2026): copies of rows 56-447 read as a 256x168 texture
(the dark bar at the bottom, gx_gl.cpp BindTexture).
"""
import struct
import sys

OP_BP, OP_XF, OP_DRAW, OP_TEXTURE, OP_BP_STATE = 1, 2, 3, 4, 7


def load(path):
    data = open(path, "rb").read()
    if data[:8] != b"RE4GXCH1":
        sys.exit(f"{path}: no GX chunk")
    head = struct.unpack_from("<14I", data, 8)
    op_words, vertex_bytes, index_count = head[3], head[4], head[5]
    ops = struct.unpack_from(f"<{op_words}I", data, 64)
    at = 64 + op_words * 4
    vertices = data[at:at + vertex_bytes]
    return head, ops, vertices


def walk(ops):
    """(op, payload, draw number before it) for each op."""
    at, draw = 0, 0
    while at < len(ops):
        op, n = ops[at] & 0xFF, ops[at] >> 8
        yield op, ops[at + 1:at + 1 + n], draw
        if op == OP_DRAW:
            draw += 1
        at += 1 + n


def copies(path):
    _, ops, _ = load(path)
    bp = [0] * 256
    draws = 0
    for op, p, draw in walk(ops):
        draws = draw
        if op == OP_BP_STATE:
            for r in range(min(len(p), 256)):
                bp[r] = p[r] & 0xFFFFFF
        elif op == OP_BP:
            reg, value = p[0] >> 24, p[0] & 0xFFFFFF
            bp[reg] = value
            if reg == 0x52:
                src, size = bp[0x49], bp[0x4A]
                x, y = src & 0x3FF, (src >> 10) & 0x3FF
                w, h = (size & 0x3FF) + 1, ((size >> 10) & 0x3FF) + 1
                dest = (bp[0x4B] & 0x1FFFFF) << 5
                fmt = ((value >> 4) & 7) | ((value >> 3) & 1) << 3
                kind = "display" if value & (1 << 14) else "texture"
                print(f"draw {draw:4d}: {kind} copy ({x},{y}) {w}x{h} -> 0x{dest:08x}, format {fmt}, "
                      f"half {(value >> 9) & 1}, clear {(value >> 11) & 1}")
        elif op == OP_TEXTURE and len(p) >= 8 and p[3] & 1:
            print(f"draw {draw:4d}: map {p[0]} = copy at 0x{p[4]:08x} as {p[5]}x{p[6]}, format {p[7]}")
    print(f"{draws} draws")


def floats(words):
    return " ".join("%.4g" % struct.unpack("<f", struct.pack("<I", w))[0] for w in words)


def draws(path, first, last):
    _, ops, vertices = load(path)
    for op, p, draw in walk(ops):
        if not first <= draw <= last:
            continue
        if op == OP_BP:
            print(f"   bp {p[0] >> 24:02x} = {p[0] & 0xFFFFFF:06x}")
        elif op == OP_XF and len(p) <= 13:
            print(f"   xf {p[0]:04x}: {floats(p[1:])}")
        elif op == OP_TEXTURE and len(p) >= 8:
            print(f"   map {p[0]}: {'copy ' if p[3] & 1 else ''}0x{p[4]:08x} {p[5]}x{p[6]} format {p[7]}")
        elif op == OP_DRAW:
            prim, layout, stride, offset, count, _, indices = p[:7]
            print(f"draw {draw}: primitive 0x{prim:x}, layout 0x{layout:x}, stride {stride}, "
                  f"{count} vertices, {indices} indices, matrices {p[7]:08x}")
            for v in range(min(count, 4)):
                base = offset + v * stride
                pos = struct.unpack_from("<3f", vertices, base)
                at = 12 + (12 if layout & 1 else 0) + (24 if layout & 2 else 0)
                extra = ""
                for bit in (4, 8):
                    if layout & bit:
                        extra += f" rgba {list(vertices[base + at:base + at + 4])}"
                        at += 4
                for t in range(8):
                    if layout & (16 << t):
                        extra += " st%d %.3f %.3f" % ((t,) + struct.unpack_from("<2f", vertices, base + at))
                        at += 8
                print(f"     {v}: pos {pos[0]:.1f} {pos[1]:.1f} {pos[2]:.1f}{extra}")


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "copies":
        copies(sys.argv[2])
    elif len(sys.argv) >= 5 and sys.argv[1] == "draws":
        draws(sys.argv[2], int(sys.argv[3]), int(sys.argv[4]))
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
