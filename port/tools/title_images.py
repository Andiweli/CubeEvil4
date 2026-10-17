#!/usr/bin/env python3
"""The pictures of the title sequence (logos, health warning, title screen).

The opening of the game is no movie: title.cpp loads SS/<lang>/title.dat, an
archive with a table of contents, registers its id texture data (entry 4,
IdTexDataLoad: the EFF layout, version 0xB) and plays one id layout (entry 6,
or 7 once everything is unlocked) along a frame counter: health warning from
frame 0, Capcom from 105, CRI from 230, Dolby from 330, the title from 585
(include/title.h TTL_START_*). The Nintendo logo is layout 0xB.

This tool writes every texture of the id texture data as a PNG and lists each
layout: which texture each unit draws, where, how big, and in which frames
its curves have keys, so that one sees whether a screen is one picture or a
logo over a background.

    title_images.py --data DIR [--lang eng] --out DIR

DIR: the disc data as the PC check takes it (the port's disc0/files folder,
an extracted disc with files/, a folder that contains SS/ directly, or a
folder with the disc images *.iso / *.gcm, read in place). Reads only;
nothing is written next to the disc data. Plain Python, no packages.
"""
import argparse
import os
import struct
import sys
import zlib
from pathlib import Path

TTL_START = [(0, "health warning"), (105, "Capcom"), (230, "CRI"), (330, "Dolby"), (585, "title"),
             (615, "menu")]


def be32(d, o):
    return struct.unpack_from(">I", d, o)[0]


def be16(d, o):
    return struct.unpack_from(">H", d, o)[0]


def bef(d, o):
    return struct.unpack_from(">f", d, o)[0]


def find(root: Path, rel: str):
    """rel under root, case-insensitive, also below files/ or disc0/files/"""
    for base in (root, root / "files", root / "disc0" / "files", root / "disc1" / "files"):
        p = base
        ok = base.is_dir()
        for part in rel.split("/"):
            if not ok:
                break
            match = [c for c in os.listdir(p) if c.lower() == part.lower()] if p.is_dir() else []
            if not match:
                ok = False
                break
            p = p / match[0]
        if ok and p.is_file():
            return p
    return None


def from_image(root: Path, rel: str):
    """rel read from a GameCube disc image in root (original_import.gcm, the reader
    of whole_disc_folder.py)"""
    if not root.is_dir():
        return None
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from original_import.gcm import Disc  # noqa: E402
    for image in sorted(root.iterdir()):
        if image.suffix.lower() not in (".iso", ".gcm"):
            continue
        disc = Disc(image)
        for entry in disc.files:
            if entry["path"].lower() == rel.lower():
                return disc.read(entry["offset"], entry["size"])
    return None


# ---- PNG ----------------------------------------------------------------------------

def write_png(path: Path, w, h, rgba: bytes):
    def chunk(kind, data):
        c = struct.pack(">I", len(data)) + kind + data
        return c + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    raw = b"".join(b"\0" + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    path.write_bytes(png)


# ---- GameCube texture formats ------------------------------------------------------------

def rgb565(v):
    return ((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31, 255


def rgb5a3(v):
    if v & 0x8000:
        return ((v >> 10) & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, (v & 31) * 255 // 31, 255
    return ((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17, ((v >> 12) & 7) * 255 // 7


def ia8(v):
    i = v & 0xFF
    return i, i, i, v >> 8


FORMATS = {0: ("I4", 8, 8, 4), 1: ("I8", 8, 4, 8), 2: ("IA4", 8, 4, 8), 3: ("IA8", 4, 4, 16), 4: ("RGB565", 4, 4, 16),
           5: ("RGB5A3", 4, 4, 16), 6: ("RGBA8", 4, 4, 32), 8: ("C4", 8, 8, 4), 9: ("C8", 8, 4, 8),
           10: ("C14X2", 4, 4, 16), 14: ("CMPR", 8, 8, 4)}


def decode(d, o, w, h, fmt, palette):
    name, bw, bh, bpp = FORMATS[fmt]
    out = bytearray(w * h * 4)

    def put(x, y, px):
        if x < w and y < h:
            out[(y * w + x) * 4:(y * w + x) * 4 + 4] = bytes(px)

    tiles_x = (w + bw - 1) // bw
    tiles_y = (h + bh - 1) // bh
    p = o
    for ty in range(tiles_y):
        for tx in range(tiles_x):
            x0, y0 = tx * bw, ty * bh
            if fmt == 14:  # CMPR: four DXT1 blocks of 4x4 per 8x8 tile
                for sub in range(4):
                    c0, c1 = be16(d, p), be16(d, p + 2)
                    bits = be32(d, p + 4)
                    p += 8
                    a, b = rgb565(c0), rgb565(c1)
                    if c0 > c1:
                        cols = [a, b, tuple((2 * a[i] + b[i]) // 3 for i in range(3)) + (255,),
                                tuple((a[i] + 2 * b[i]) // 3 for i in range(3)) + (255,)]
                    else:
                        cols = [a, b, tuple((a[i] + b[i]) // 2 for i in range(3)) + (255,), (0, 0, 0, 0)]
                    sx, sy = x0 + (sub & 1) * 4, y0 + (sub >> 1) * 4
                    for k in range(16):
                        put(sx + (k & 3), sy + (k >> 2), cols[(bits >> (30 - 2 * k)) & 3])
                continue
            if fmt == 6:  # RGBA8: AR pairs of the tile, then GB
                for k in range(16):
                    a_, r = d[p + 2 * k], d[p + 2 * k + 1]
                    g, b = d[p + 32 + 2 * k], d[p + 33 + 2 * k]
                    put(x0 + (k & 3), y0 + (k >> 2), (r, g, b, a_))
                p += 64
                continue
            for k in range(bw * bh):
                x, y = x0 + k % bw, y0 + k // bw
                if bpp == 4:
                    v = d[p + k // 2]
                    v = v >> 4 if k % 2 == 0 else v & 15
                elif bpp == 8:
                    v = d[p + k]
                else:
                    v = be16(d, p + 2 * k)
                if fmt == 0:
                    px = (v * 17,) * 3 + (255,)
                elif fmt == 1:
                    px = (v,) * 3 + (255,)
                elif fmt == 2:
                    px = ((v & 15) * 17,) * 3 + ((v >> 4) * 17,)
                elif fmt == 3:
                    px = ia8(v)
                elif fmt == 4:
                    px = rgb565(v)
                elif fmt == 5:
                    px = rgb5a3(v)
                else:  # C4 / C8 / C14X2
                    idx = v & 0x3FFF
                    px = palette[idx] if palette and idx < len(palette) else (255, 0, 255, 255)
                put(x, y, px)
            p += bw * bh * bpp // 8
    return name, bytes(out)


def tpl_images(d, base):
    """(width, height, format name, rgba) of every image of the TPL at base"""
    if be32(d, base) != 0x0020AF30:
        return []
    n = be32(d, base + 4)
    table = base + be32(d, base + 8)
    images = []
    for i in range(n):
        th, ch = be32(d, table + i * 8), be32(d, table + i * 8 + 4)
        t = base + th
        h, w, fmt, data = be16(d, t), be16(d, t + 2), be32(d, t + 4), base + be32(d, t + 8)
        palette = None
        if ch and fmt in (8, 9, 10):
            c = base + ch
            num, pfmt, pdata = be16(d, c), be32(d, c + 4), base + be32(d, c + 8)
            conv = {0: ia8, 1: rgb565, 2: rgb5a3}.get(pfmt, rgb5a3)
            palette = [conv(be16(d, pdata + 2 * k)) for k in range(num)]
        if fmt not in FORMATS:
            images.append((w, h, f"format {fmt}", None))
            continue
        name, rgba = decode(d, data, w, h, fmt, palette)
        images.append((w, h, name, rgba))
    return images


# ---- the title archive -------------------------------------------------------------------

def archive(d):
    n = be32(d, 0)
    offs = [be32(d, 16 + 4 * i) for i in range(n)]
    tags = [d[16 + 4 * n + 4 * i:16 + 4 * n + 4 * i + 4].split(b"\0")[0].decode("latin-1") for i in range(n)]
    return offs, tags


def id_textures(d, base):
    """{texture id: [(w, h, fmt, rgba)...]} of an id texture data block (IdTexDataLoad)"""
    if be32(d, base) != 0xB:
        return {}
    ids = base + be32(d, base + 4)
    tpls = base + be32(d, base + 0x18)
    out = {}
    for i in range(be32(d, ids)):
        tex_id = be16(d, ids + 4 + i * 8) & 0xFF
        tpl = tpls + be32(d, tpls + 4 + i * 4)
        out.setdefault(tex_id, []).extend(tpl_images(d, tpl))
    return out


def window(frames):
    """the logo screen(s) a range of key frames falls into"""
    names = []
    for i, (start, name) in enumerate(TTL_START):
        end = TTL_START[i + 1][0] if i + 1 < len(TTL_START) else 1 << 30
        if frames and min(frames) < end and max(frames) >= start:
            names.append(name)
    return ", ".join(names)


def layout(d, base, lines):
    ver = d[base:base + 4].decode("latin-1", "replace")
    units = d[base + 5]
    size = 0x88 if ver.startswith("1") else 0x8C
    ofs_at = 0x70 if size == 0x88 else 0x74
    lines.append(f"  version {ver}, {units} units")
    for u in range(units):
        o = base + 8 + u * size
        tex_id, no, parent, kind = d[o + 0x0B], d[o + 5], d[o + 7], d[o + 9]
        x, y, z = bef(d, o + 0x14), bef(d, o + 0x18), bef(d, o + 0x1C)
        sx, sy = bef(d, o + 0x50), bef(d, o + 0x54)
        frames = []
        for c in range(2, 6):
            co = be32(d, o + ofs_at + 4 * c)
            if co:
                n = struct.unpack_from(">i", d, base + co)[0]
                frames += [bef(d, base + co + 4 + 16 * k) for k in range(max(0, min(n, 256)))]
        when = f"keys at frames {min(frames):.0f}..{max(frames):.0f} ({window(frames)})" if frames else "no curves"
        lines.append(f"  unit {u:2d} (no {no}, parent {parent}, kind {kind}): texture {tex_id:3d}, "
                     f"position {x:7.1f} {y:7.1f} {z:5.1f}, size {sx:6.1f} x {sy:6.1f}, {when}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data", required=True, type=Path)
    ap.add_argument("--lang", default="eng")
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()
    rel = f"SS/{args.lang}/title.dat"
    path = find(args.data, rel)
    if path:
        d = path.read_bytes()
    else:
        d = from_image(args.data, rel)
        path = f"{args.data} (disc image) {rel}"
    if d is None:
        print(f"{rel} not found under {args.data}")
        return 1
    args.out.mkdir(parents=True, exist_ok=True)
    offs, tags = archive(d)
    lines = [f"{path}: {len(d)} bytes, {len(offs)} entries"]
    for i, (o, t) in enumerate(zip(offs, tags)):
        lines.append(f"entry {i:2d} at 0x{o:06x}: {t or '(no tag)'}")
    lines.append("")
    textures = id_textures(d, offs[4]) if len(offs) > 4 else {}
    lines.append(f"id texture data (entry 4): {len(textures)} textures")
    for tex_id, images in sorted(textures.items()):
        for k, (w, h, fmt, rgba) in enumerate(images):
            name = f"texture_{tex_id:03d}" + (f"_{k}" if len(images) > 1 else "") + ".png"
            if rgba:
                write_png(args.out / name, w, h, rgba)
            lines.append(f"  texture {tex_id:3d}: {w} x {h} {fmt} -> {name if rgba else '(not decoded)'}")
    for entry, what in ((6, "title sequence"), (7, "title sequence, all unlocked"), (0xB, "Nintendo logo"),
                        (8, "main menu"), (9, "main menu, all unlocked")):
        if entry < len(offs):
            lines.append("")
            lines.append(f"layout entry {entry} ({what}):")
            try:
                layout(d, offs[entry], lines)
            except (struct.error, IndexError) as e:
                lines.append(f"  not readable: {e}")
    report = "\n".join(lines) + "\n"
    (args.out / "title_images.txt").write_text(report, encoding="utf-8")
    print(report)
    print(f"pictures and this list: {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
