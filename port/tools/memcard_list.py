#!/usr/bin/env python3
"""Lists a memory card file of the port and checks it.

    memcard_list.py CARD [--expect-system] [--expect-save N]
    memcard_list.py FILE.gci [--expect-system] [--expect-save N]

CARD: the card file of runtime/memcard.cpp (a Memory Card 251, 2 MB, 8 KB
blocks; in the app <data folder>/memcard/card-a.bin). The port's card keeps
the SDK's structures in the byte order of the device (little-endian), not
the big-endian one of a GameCube card (decision of the project owner,
16.10.2026), so Dolphin's .raw files are not read here.

Printed: the header (formatted?, format time, encoding), which of the two
directory and FAT copies is current and whether their checksums hold, then
every file with its name, game, size, banner/icon formats and comment. The
game's own files are opened up (src/game/card.cpp): bh4_system with its
CRC and the options it holds (language, brightness, controller layout,
sound), bh4_dataNN with both CRCs, the save date, the room and the chapter.

--expect-system / --expect-save N: exit code 1 unless the system file and
save file N are there with valid CRCs. Exit code 1 as well when a checksum
of the card's own structures is wrong.

A .gci (one file as Dolphin and the GameCube's copy tools keep it, what the
app's menu exports, M11): the 64-byte directory entry and the file's blocks,
all in GameCube byte order (big-endian). Read here on their own, without the
port's converter (runtime/gci.cpp), so the check does not repeat its rules:
the entry's fields, the game's CRCs over the big-endian bytes, the version,
the room and the save date.
"""
import argparse
import struct
import sys
from pathlib import Path

BLOCK = 0x2000
CARD_BYTES = 2 * 1024 * 1024
DIR_ENTRIES = 127
EPOCH_2000 = 946684800
TIMER_CLOCK = 162000000 // 4  # OSGetTime ticks per second (bus clock / 4)


def checksum(data):
    """__CARDCheckSum (CARDCheck.c): the u16 words and their complements."""
    words = struct.unpack(f"<{len(data) // 2}H", data)
    s = sum(words) & 0xFFFF
    inv = sum((~w) & 0xFFFF for w in words) & 0xFFFF
    return (0 if s == 0xFFFF else s), (0 if inv == 0xFFFF else inv)


def _crc_table():
    table = []
    for i in range(256):
        c = i << 24
        for _ in range(8):
            c = ((c << 1) ^ 0x04C11DB7) if c & 0x80000000 else (c << 1)
        table.append(c & 0xFFFFFFFF)
    return table


CRC_TABLE = _crc_table()


def crc32_game(data):
    """CRCCalc of src/game/card.cpp: CRC-32 MSB first, polynomial 0x04C11DB7, start 0."""
    crc = 0
    for b in data:
        crc = ((crc << 8) & 0xFFFFFFFF) ^ CRC_TABLE[(crc >> 24) ^ b]
    return crc


def text(raw):
    raw = raw.split(b"\0", 1)[0]
    try:
        return raw.decode("shift_jis")
    except UnicodeDecodeError:
        return raw.decode("latin-1")


def calendar(raw, order="<"):
    """OSCalendarTime: sec, min, hour, mday, mon (0-11), year, wday, yday, msec, usec."""
    sec, mins, hour, mday, mon, year = struct.unpack_from(order + "6i", raw)
    return f"{year:04d}-{mon + 1:02d}-{mday:02d} {hour:02d}:{mins:02d}:{sec:02d}"


def newer(code0, code1):
    """The copy the SDK takes as current (CARDCheck.c): the higher check code, s16 wrap."""
    d = (code0 - code1) & 0xFFFF
    return 0 if d < 0x8000 else 1


class Card:
    def __init__(self, data):
        self.data = data
        self.problems = []

    def block(self, n):
        return self.data[n * BLOCK:(n + 1) * BLOCK]

    def header(self):
        h = self.block(0)[:0x200]
        device_id, size, encode = struct.unpack_from("<3H", h, 0x20)
        check, check_inv = struct.unpack_from("<2H", h, 0x1FC)
        ok = checksum(h[:0x1FC]) == (check, check_inv)
        if not ok:
            return None
        (fmt_time,) = struct.unpack_from("<q", h, 12)
        seconds = fmt_time // TIMER_CLOCK
        return {"size": size, "encode": encode, "device": device_id, "formatted": seconds}

    def current(self, first, kind):
        copies = []
        for i in range(2):
            b = self.block(first + i)
            if kind == "dir":
                code, s, inv = struct.unpack_from("<hHH", b, 0x1FFA)
                ok = checksum(b[:0x1FFC]) == (s, inv)
            else:
                s, inv, code = struct.unpack_from("<HHh", b, 0)
                ok = checksum(b[4:]) == (s, inv)
            copies.append((ok, code & 0xFFFF))
        if copies[0][0] and copies[1][0]:
            pick = newer(copies[0][1], copies[1][1])
        elif copies[0][0] or copies[1][0]:
            pick = 0 if copies[0][0] else 1
            self.problems.append(f"{kind}: copy {1 - pick} broken (the SDK repairs it at the next write)")
        else:
            self.problems.append(f"{kind}: both copies broken")
            return None, copies
        return pick, copies

    def files(self, dir_block):
        out = []
        b = self.block(dir_block)
        for i in range(DIR_ENTRIES):
            e = b[i * 64:(i + 1) * 64]
            if e[:4] == b"\xff\xff\xff\xff":
                continue
            game, company = e[:4], e[4:6]
            banner = e[7]
            name = text(e[8:40])
            tm, icon_addr, icon_fmt, icon_speed = struct.unpack_from("<IIHH", e, 40)
            perm, copies, start, length = struct.unpack_from("<BBHH", e, 52)
            (comment,) = struct.unpack_from("<I", e, 60)
            out.append({"no": i, "game": game.decode("latin-1"), "company": company.decode("latin-1"),
                        "name": name, "time": tm, "banner": banner, "icon_addr": icon_addr,
                        "icon_fmt": icon_fmt, "icon_speed": icon_speed, "start": start,
                        "blocks": length, "comment": comment, "perm": perm})
        return out

    def read(self, fat, f):
        """The file's bytes along its FAT chain."""
        out, n, seen = bytearray(), f["start"], 0
        while 5 <= n < CARD_BYTES // BLOCK and seen < f["blocks"]:
            out += self.block(n)
            seen += 1
            nxt = fat[n]
            if nxt == 0xFFFF:
                break
            n = nxt
        if seen != f["blocks"]:
            self.problems.append(f"{f['name']}: FAT chain has {seen} of {f['blocks']} blocks")
        return bytes(out)


BANNER = {0: "none", 1: "C8", 2: "RGB5A3"}


def describe_game_file(f, body, lines, order="<"):
    if f["name"] == "bh4_system":
        size = 0x1E7C
        (crc,) = struct.unpack_from(order + "I", body, 0x1E78)
        ok = crc == crc32_game(body[:0x1E78])
        cfg, ext, lang, country, bright, pad, snd = struct.unpack_from(order + "IIBBBBB", body, 0x1E40)
        lines.append(f"      system file: CRC {'ok' if ok else 'WRONG'}; language {lang}, country {country}, "
                     f"brightness {bright}, controller layout {pad}, sound mode {snd}, "
                     f"config flags 0x{cfg:08x}, extras 0x{ext:08x}")
        return ok, size
    if f["name"].startswith("bh4_data"):
        (crc,) = struct.unpack_from(order + "I", body, 0xEAF8)
        ok_file = crc == crc32_game(body[:0xEAF8])
        hcrc, magic = struct.unpack_from(order + "II", body, 0x2000)
        ok_hdr = hcrc == crc32_game(body[0x2004:0x2200])
        when = calendar(body[0x2008:0x2030], order)
        (mode,) = struct.unpack_from(order + "I", body, 0x2030)
        chapter = body[0x203E]
        (count,) = struct.unpack_from(order + "H", body, 0x2040)
        (room,) = struct.unpack_from(order + "H", body, 0x2050)
        lines.append(f"      save: CRC file {'ok' if ok_file else 'WRONG'}, header {'ok' if ok_hdr else 'WRONG'}, "
                     f"version 0x{magic:x}, saved {when}, room r{room:03x}, chapter {chapter}, "
                     f"save count {count}, mode {mode}")
        return ok_file and ok_hdr and magic == 0x116, 0xEAFC
    return True, 0


def list_gci(path, data, args):
    """One .gci: the directory entry and the file, big-endian."""
    import datetime
    lines, bad = [], False
    if len(data) < 0x40 + BLOCK or (len(data) - 0x40) % BLOCK:
        print(f"{path}: {len(data)} bytes, not 0x40 + a multiple of 8 KB")
        return 1
    e = data[:0x40]
    game, company, banner = e[:4].decode("latin-1"), e[4:6].decode("latin-1"), e[7]
    name = text(e[8:40])
    tm, icon_addr, icon_fmt, icon_speed = struct.unpack_from(">IIHH", e, 40)
    perm, copies, start, length = struct.unpack_from(">BBHH", e, 52)
    (comment,) = struct.unpack_from(">I", e, 60)
    when = datetime.datetime(2000, 1, 1) + datetime.timedelta(seconds=tm)
    lines.append(f"gci {Path(path).name}: {name} {game}{company}, {length} blocks, {when:%Y-%m-%d %H:%M:%S}, "
                 f"banner {BANNER.get(banner & 3, '?')}, icon at 0x{icon_addr:x}, permission {perm}")
    body = data[0x40:]
    if length * BLOCK != len(body):
        lines.append(f"  FEHLER: the entry says {length} blocks, the file holds {len(body) // BLOCK}")
        bad = True
    if comment != 0xFFFFFFFF and comment + 64 <= len(body):
        lines.append(f"  comment: \"{text(body[comment:comment + 32])}\" / \"{text(body[comment + 32:comment + 64])}\"")
    ok, _ = describe_game_file({"name": name}, body, lines, ">")
    bad = bad or not ok
    if args.expect_system and name != "bh4_system":
        lines.append("  FEHLER: not the system file")
        bad = True
    for n in args.expect_save:
        if name != f"bh4_data{n:02d}":
            lines.append(f"  FEHLER: not save bh4_data{n:02d}")
            bad = True
    if not ok:
        lines.append("  FEHLER: the game's CRCs do not hold in GameCube byte order")
    print("\n".join(lines))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("card")
    ap.add_argument("--expect-system", action="store_true")
    ap.add_argument("--expect-save", type=int, action="append", default=[])
    args = ap.parse_args()
    data = Path(args.card).read_bytes()
    if args.card.lower().endswith(".gci"):
        return list_gci(args.card, data, args)
    if len(data) != CARD_BYTES:
        print(f"memory card {args.card}: {len(data)} bytes, not 2 MB (a card of the port has 2097152)")
        return 1
    card = Card(data)
    lines = []
    head = card.header()
    if head is None:
        print(f"memory card {args.card}: not formatted (no valid header)")
        return 1 if (args.expect_system or args.expect_save) else 0
    when = Path(args.card).name
    import datetime
    fmt = datetime.datetime(2000, 1, 1) + datetime.timedelta(seconds=head["formatted"])
    lines.append(f"memory card {when}: formatted {fmt:%Y-%m-%d %H:%M}, {head['size']} Mbit, "
                 f"encoding {'SJIS' if head['encode'] else 'ANSI'}")
    d, dirs = card.current(1, "dir")
    fa, fats = card.current(3, "fat")
    if d is None or fa is None:
        lines += [f"  FEHLER: {p}" for p in card.problems]
        print("\n".join(lines))
        return 1
    fat_block = card.block(3 + fa)
    fat = struct.unpack("<4096H", fat_block)
    free = fat[3]
    lines.append(f"  directory copy {d} (check code {dirs[d][1]}), FAT copy {fa} (check code {fats[fa][1]}), "
                 f"{free} of 251 blocks free")
    files = card.files(1 + d)
    used = sum(f["blocks"] for f in files)
    if used + free != 251:
        card.problems.append(f"blocks: {used} used + {free} free is not 251")
    good = {}
    for f in files:
        tm = datetime.datetime(2000, 1, 1) + datetime.timedelta(seconds=f["time"])
        icons = []
        for i in range(8):
            fm = (f["icon_fmt"] >> (2 * i)) & 3
            if fm:
                icons.append(BANNER.get(fm, "?"))
        lines.append(f"  file {f['no']:3d} {f['name']:<12} {f['game']}{f['company']}  {f['blocks']} blocks "
                     f"(block {f['start']}), {tm:%Y-%m-%d %H:%M:%S}, banner {BANNER.get(f['banner'] & 3, '?')}, "
                     f"icons {'+'.join(icons) or 'none'} at 0x{f['icon_addr']:x}")
        body = card.read(fat, f)
        if f["comment"] != 0xFFFFFFFF and f["comment"] + 64 <= len(body):
            c = body[f["comment"]:f["comment"] + 64]
            lines.append(f"      comment: \"{text(c[:32])}\" / \"{text(c[32:])}\"")
        ok, _ = describe_game_file(f, body, lines)
        good[f["name"]] = ok
    lines += [f"  FEHLER: {p}" for p in card.problems]
    bad = bool(card.problems)
    if args.expect_system and not good.get("bh4_system"):
        lines.append("  FEHLER: no valid system file bh4_system")
        bad = True
    for n in args.expect_save:
        if not good.get(f"bh4_data{n:02d}"):
            lines.append(f"  FEHLER: no valid save bh4_data{n:02d}")
            bad = True
    print("\n".join(lines))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
