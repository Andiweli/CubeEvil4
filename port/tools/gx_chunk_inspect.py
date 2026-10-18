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
    gx_chunk_inspect.py diff A.bin B.bin [--lines N]
        the same frame of two builds (the 32/64-bit compare run of the PC
        check): op by op, the first differences with the values of both sides
        (BP register, XF address and floats, draw and its vertices, texture
        and its texels, palette), and how many ops of each kind differ.
        Addresses are left out (texture and copy bases, TLUT loads, blob
        offsets): the heaps lie elsewhere on 64 bit. Exit code 0 when the
        chunks agree, 1 otherwise.

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


# BP registers that hold an address (the texture bases TX_SETIMAGE3 of maps 0-7, the EFB copy
# destination, the TLUT load source): they differ between builds without changing the picture
BP_ADDRESS = {0x4B, 0x64} | set(range(0x94, 0x98)) | set(range(0xB4, 0xB8))
OP_NAMES = {OP_BP: "BP", OP_XF: "XF", OP_DRAW: "Zeichenauftrag", OP_TEXTURE: "Textur", 5: "TLUT", 6: "FORGET",
            OP_BP_STATE: "BP-Zustand"}


def load_all(path):
    data = open(path, "rb").read()
    if data[:8] != b"RE4GXCH1":
        sys.exit(f"{path}: no GX chunk")
    head = struct.unpack_from("<14I", data, 8)
    op_words, vertex_bytes, index_count, blob_bytes = head[3], head[4], head[5], head[6]
    ops = struct.unpack_from(f"<{op_words}I", data, 64)
    at = 64 + op_words * 4
    vertices = data[at:at + vertex_bytes]
    at += vertex_bytes
    indices = data[at:at + index_count * 2]
    at += index_count * 2
    return head, ops, vertices, indices, data[at:at + blob_bytes]


def fl(w):
    return struct.unpack("<f", struct.pack("<I", w))[0]


def draw_vertices(p, vertices, indices):
    """the floats and the bytes of a draw's vertices (and its indices)"""
    stride, offset, count, ioff, icount = p[2], p[3], p[4], p[5], p[6]
    raw = vertices[offset:offset + stride * count]
    return raw, indices[ioff * 2:(ioff + icount) * 2]


def vertex_diff(layout, stride, ra, rb):
    """largest difference of the floats (position, normals, texture coordinates) and the number
    of differing colour / matrix bytes"""
    worst, bytes_diff = 0.0, 0
    floats = 3 + (3 if layout & 1 else 0) + (6 if layout & 2 else 0)
    for v in range(min(len(ra), len(rb)) // stride if stride else 0):
        base = v * stride
        fa = struct.unpack_from(f"<{floats}f", ra, base)
        fb = struct.unpack_from(f"<{floats}f", rb, base)
        worst = max([worst] + [abs(x - y) for x, y in zip(fa, fb)])
        at = base + floats * 4
        colors = ((1 if layout & 4 else 0) + (1 if layout & 8 else 0)) * 4
        bytes_diff += sum(1 for x, y in zip(ra[at:at + colors], rb[at:at + colors]) if x != y)
        at += colors
        tex = sum(1 for t in range(8) if layout & (16 << t))
        if tex:
            ta = struct.unpack_from(f"<{tex * 2}f", ra, at)
            tb = struct.unpack_from(f"<{tex * 2}f", rb, at)
            worst = max([worst] + [abs(x - y) for x, y in zip(ta, tb)])
            at += tex * 8
        if layout & (1 << 12):
            bytes_diff += sum(1 for x, y in zip(ra[at:at + 12], rb[at:at + 12]) if x != y)
    return worst, bytes_diff


def diff(path_a, path_b, max_lines=12):
    ha, oa, va, ia, ba = load_all(path_a)
    hb, ob, vb, ib, bb = load_all(path_b)
    lines, counts = [], {}
    keys = {}  # texture key of A -> key of B (same texels under another key: fine)

    def note(kind, draw, text):
        counts[kind] = counts.get(kind, 0) + 1
        if len(lines) < max_lines:
            lines.append(f"  Zeichenauftrag {draw:4d}: {kind}: {text}")

    wa, wb = list(walk(oa)), list(walk(ob))
    for i, ((op_a, pa, da), (op_b, pb, db)) in enumerate(zip(wa, wb)):
        if op_a != op_b or (op_a in (OP_XF, OP_BP_STATE) and len(pa) != len(pb)):
            lines.append(f"  Op {i} (Zeichenauftrag {da}): der Ablauf trennt sich: {OP_NAMES.get(op_a, op_a)} / "
                         f"{OP_NAMES.get(op_b, op_b)}")
            counts["Ablauf"] = 1
            break
        if op_a == OP_BP:
            reg = pa[0] >> 24
            if pa[0] != pb[0] and reg not in BP_ADDRESS:
                note("BP", da, f"Register {reg:02x}: {pa[0] & 0xFFFFFF:06x} / {pb[0] & 0xFFFFFF:06x}")
        elif op_a == OP_BP_STATE:
            for r, (x, y) in enumerate(zip(pa, pb)):
                if x != y and r not in BP_ADDRESS:
                    note("BP-Zustand", da, f"Register {r:02x}: {x & 0xFFFFFF:06x} / {y & 0xFFFFFF:06x}")
        elif op_a == OP_XF:
            if pa[0] != pb[0]:
                note("XF", da, f"Adresse {pa[0]:04x} / {pb[0]:04x}")
            elif pa != pb:
                k = next(k for k in range(1, len(pa)) if pa[k] != pb[k])
                worst = max(abs(fl(x) - fl(y)) for x, y in zip(pa[1:], pb[1:]))
                note("XF", da, f"Adresse {pa[0]:04x}+{k - 1}: {fl(pa[k]):.7g} / {fl(pb[k]):.7g} "
                     f"(Wort {pa[k]:08x} / {pb[k]:08x}; größter Unterschied der {len(pa) - 1} Werte {worst:.3g})")
        elif op_a == OP_DRAW:
            if (pa[0], pa[1], pa[2], pa[4], pa[6], tuple(pa[7:10])) != (pb[0], pb[1], pb[2], pb[4], pb[6], tuple(pb[7:10])):
                note("Zeichenauftrag", da, f"Art/Ecken {pa[0]:x} {pa[1]:x} {pa[4]} {pa[6]} / {pb[0]:x} {pb[1]:x} {pb[4]} {pb[6]}")
                continue
            ra, xa = draw_vertices(pa, va, ia)
            rb, xb = draw_vertices(pb, vb, ib)
            if ra != rb or xa != xb:
                worst, nbytes = vertex_diff(pa[1], pa[2], ra, rb)
                note("Ecken", da, f"{pa[4]} Ecken, größter Unterschied der Gleitkommawerte {worst:.3g}, "
                     f"{nbytes} abweichende Farb-/Matrix-Bytes{', Indizes anders' if xa != xb else ''}")
        elif op_a == OP_TEXTURE:
            fa, fb = pa[3] & 1, pb[3] & 1
            shape_a, shape_b = (pa[0], fa, pa[5], pa[6], pa[7], pa[8]), (pb[0], fb, pb[5], pb[6], pb[7], pb[8])
            if shape_a != shape_b:
                note("Textur", da, f"Karte/Größe/Format {shape_a} / {shape_b}")
            elif not fa:
                ka, kb = (pa[1], pa[2]), (pb[1], pb[2])
                if pa[11] and pb[11]:
                    if ba[pa[10]:pa[10] + pa[11]] != bb[pb[10]:pb[10] + pb[11]]:
                        tex_a, tex_b = ba[pa[10]:pa[10] + pa[11]], bb[pb[10]:pb[10] + pb[11]]
                        n = sum(1 for x, y in zip(tex_a, tex_b) if x != y) + abs(len(tex_a) - len(tex_b))
                        note("Textur", da, f"Karte {pa[0]} {pa[5]}x{pa[6]} Format {pa[7]}: {n} von {pa[11]} Bytes der "
                             "Texel anders")
                    keys[ka] = kb
                elif keys.get(ka, kb) != kb:
                    note("Textur", da, f"Karte {pa[0]}: eine andere schon gesendete Textur")
        elif op_a == 5:  # TLUT: offset, bytes, blob offset
            if pa[:2] != pb[:2] or ba[pa[2]:pa[2] + pa[1]] != bb[pb[2]:pb[2] + pb[1]]:
                note("TLUT", da, f"TMEM {pa[0]:x} {pa[1]} Bytes / {pb[0]:x} {pb[1]} Bytes oder andere Einträge")
    if len(wa) != len(wb) and "Ablauf" not in counts:
        lines.append(f"  Ende: {len(wa)} / {len(wb)} Ops")
        counts["Ablauf"] = 1
    total = sum(counts.values())
    if total == 0:
        print(f"GX-Daten gleich ({len(wa)} Ops, {ha[10]} Zeichenaufträge)")
        return 0
    print(f"GX-Daten verschieden: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())) +
          f" (von {len(wa)} / {len(wb)} Ops); die ersten:")
    for line in lines:
        print(line)
    return 1


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "copies":
        copies(sys.argv[2])
    elif len(sys.argv) >= 5 and sys.argv[1] == "draws":
        draws(sys.argv[2], int(sys.argv[3]), int(sys.argv[4]))
    elif len(sys.argv) >= 4 and sys.argv[1] == "diff":
        n = int(sys.argv[sys.argv.index("--lines") + 1]) if "--lines" in sys.argv else 12
        return diff(sys.argv[2], sys.argv[3], n)
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
