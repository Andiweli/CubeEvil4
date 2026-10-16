#!/usr/bin/env python3
"""Builds a small Sofdec movie (.sfd) for the movie test of the port (M9).

The GameCube's movies are CRI Sofdec files: an MPEG-1 program stream in
0x800-byte packs whose first pack carries the Sofdec header (a private
stream 2 packet with "SofdecStream" at file offset 0x20), MPEG-1 video in
stream 0xE0 and ADX sound in stream 0xC0 (its header padded to 0x120 bytes
ending in "(c)CRI"). This tool muxes an MPEG-1 video elementary stream and
an ADX file (both from ffmpeg) into that layout, with PTS/DTS on the packets
that start a picture and on every audio packet. No game data is involved.

    make_test_sfd.py --video in.m1v --audio in.adx --out test.sfd

The header fields follow what src/lib/sfh_main.c reads (little-endian words,
element records at 0x180); the CRI player takes the picture size from the
MPEG sequence header anyway (sfd_cre.c sfcre_AnalyMpv).
"""
import argparse
import struct
import sys

PACK = 0x800
CLOCK = 90000
PICTURE_RATES = {1: 24000 / 1001, 2: 24.0, 3: 25.0, 4: 30000 / 1001, 5: 30.0, 6: 50.0,
                 7: 60000 / 1001, 8: 60.0}


def pack_header(scr, mux_rate):
    return bytes([0, 0, 1, 0xBA,
                  0x21 | ((scr >> 29) & 0x0E),
                  (scr >> 22) & 0xFF,
                  ((scr >> 14) & 0xFE) | 1,
                  (scr >> 7) & 0xFF,
                  ((scr << 1) & 0xFE) | 1,
                  0x80 | ((mux_rate >> 15) & 0x7F),
                  (mux_rate >> 7) & 0xFF,
                  ((mux_rate << 1) & 0xFE) | 1])


def timestamp(prefix, ts):
    return bytes([(prefix << 4) | ((ts >> 29) & 0x0E) | 1,
                  (ts >> 22) & 0xFF,
                  ((ts >> 14) & 0xFE) | 1,
                  (ts >> 7) & 0xFF,
                  ((ts << 1) & 0xFE) | 1])


def system_header(mux_rate):
    body = bytes([0x80 | ((mux_rate >> 15) & 0x7F), (mux_rate >> 7) & 0xFF, ((mux_rate << 1) & 0xFE) | 1,
                  (1 << 2) | 0x00,      # audio bound 1, not fixed, not CSPS
                  0xE0 | 1,             # audio/video locked, video bound 1
                  0xFF,
                  0xE0, 0xE0 | 0x20 | 0x00, 0xE8,   # video: STD scale 1, 232 KB
                  0xC0, 0xC0 | 0x00, 0x20])         # audio: STD scale 0, 4 KB
    return bytes([0, 0, 1, 0xBB]) + struct.pack(">H", len(body)) + body


def pes(stream_id, room, data, pts=None, dts=None):
    """One PES packet in `room` bytes; the rest of a last, short packet is a padding packet."""
    if pts is None:
        opt = b"\x0F"
    elif dts is None or dts == pts:
        opt = timestamp(2, pts)
    else:
        opt = timestamp(3, pts) + timestamp(1, dts)
    head = 6 + len(opt)
    take = min(len(data), room - head)
    left = room - head - take
    stuffing = left if left <= 16 else 0
    body = b"\xFF" * stuffing + opt + data[:take]
    out = bytes([0, 0, 1, stream_id]) + struct.pack(">H", len(body)) + body
    if left > 16:
        out += padding(left)
    return out, take


def padding(room):
    return bytes([0, 0, 1, 0xBE]) + struct.pack(">H", room - 6) + b"\xFF" * (room - 6)


def parse_video(es):
    """Pictures of an MPEG-1 elementary stream: (offset, type, temporal reference), the size and rate."""
    i = es.find(b"\x00\x00\x01\xB3")
    if i < 0:
        sys.exit("no sequence header")
    w = (es[i + 4] << 4) | (es[i + 5] >> 4)
    h = ((es[i + 5] & 0xF) << 8) | es[i + 6]
    rate_code = es[i + 7] & 0xF
    bitrate = ((es[i + 8] << 10) | (es[i + 9] << 2) | (es[i + 10] >> 6)) * 400
    pictures = []
    gop = 0
    seen = 0
    pos = 0
    while True:
        pos = es.find(b"\x00\x00\x01", pos)
        if pos < 0 or pos + 6 > len(es):
            break
        code = es[pos + 3]
        if code == 0xB8:  # GOP: temporal references restart
            gop = seen
        elif code == 0x00:
            tref = (es[pos + 4] << 2) | (es[pos + 5] >> 6)
            ptype = (es[pos + 5] >> 3) & 7
            pictures.append((pos, ptype, gop + tref))
            seen += 1
        pos += 3
    return w, h, rate_code, bitrate, pictures


def parse_adx(data):
    if data[0:2] != b"\x80\x00":
        sys.exit("not an ADX file")
    offset = struct.unpack(">H", data[2:4])[0]
    channels = data[7]
    rate = struct.unpack(">I", data[8:12])[0]
    body = data[offset + 4:]
    return channels, rate, data[:offset + 4], body


def sofdec_adx_header(head, total_samples):
    """The ADX header as the Sofdec muxer writes it: 0x120 bytes, "(c)CRI" before the data."""
    h = bytearray(0x120)
    h[0:0x14] = head[0:0x14]
    h[2:4] = struct.pack(">H", 0x120 - 4)
    h[0x0C:0x10] = struct.pack(">I", total_samples)
    h[0x12] = 3  # version 3, no loop
    h[0x13] = 0
    h[0x120 - 6:0x120] = b"(c)CRI"
    return bytes(h)


def sofdec_header(w, h, rate_code, bitrate, frames, channels, sfreq, byterate, max_vid, max_aud):
    hdr = bytearray(PACK - 12 - 6)
    base = 0x12  # file offset of the payload

    def put(ofs, data):
        hdr[ofs - base:ofs - base + len(data)] = data

    put(0x20, b"SofdecStream            ")
    put(0x38, bytes([1, 19]))
    put(0x60, b"RE4 port test muxer Ver.1.19".ljust(0x20, b" "))
    put(0x80, struct.pack("<I", PACK))      # header size
    put(0x84, bytes([1]))                   # pack type
    put(0x88, struct.pack("<H", 2))         # packet size field length
    put(0x8C, struct.pack("<I", PACK))      # pack size
    put(0xB0, bytes([2, 1, 1, 0]))          # elements: total, audio, video, private
    put(0xB4, struct.pack("<I", byterate))
    put(0xB8, struct.pack("<I", max_aud))
    put(0xBC, struct.pack("<I", max_vid))
    put(0xC0, struct.pack("<I", frames))
    vid = bytearray(0x40)
    vid[0x18] = 0xE0
    vid[0x19] = 1
    vid[0x1A:0x1C] = struct.pack("<H", min(bitrate // 400, 0xFFFE))
    vid[0x1C:0x1F] = bytes([w >> 4, ((w & 0xF) << 4) | (h >> 8), h & 0xFF])
    vid[0x1F] = rate_code
    aud = bytearray(0x40)
    aud[0x18] = 0xC0
    aud[0x19] = 2
    aud[0x1B] = channels
    aud[0x1C:0x20] = struct.pack("<I", sfreq)
    put(0x180, bytes(vid))
    put(0x1C0, bytes(aud))
    return bytes([0, 0, 1, 0xBF]) + struct.pack(">H", len(hdr)) + bytes(hdr)


def mux(video, audio):
    # the sequence end code after the last picture (ffmpeg's raw MPEG-1 output
    # leaves it out; the Sofdec decoder shows the last reference picture on it)
    if not video.endswith(b"\x00\x00\x01\xB7"):
        video = video + b"\x00\x00\x01\xB7"
    w, h, rate_code, bitrate, pictures = parse_video(video)
    fps = PICTURE_RATES[rate_code]
    channels, sfreq, adx_head, adx_body = parse_adx(audio)
    frame_bytes = 18 * channels
    nframes = len(adx_body) // frame_bytes
    # the stream ends with an end code frame (0x8001): keep it as data
    samples = nframes * 32
    adx = sofdec_adx_header(adx_head, samples) + adx_body
    delay = 0.2  # seconds of decoder delay before the first presentation (PTS offset)

    # video access units in decode order with their decode / presentation time
    units = []
    for n, (pos, ptype, disp) in enumerate(pictures):
        end = pictures[n + 1][0] if n + 1 < len(pictures) else len(video)
        start = 0 if n == 0 else pos
        dts = int((delay + (n - 1) / fps) * CLOCK) if n > 0 else int((delay - 1 / fps) * CLOCK)
        pts = int((delay + disp / fps) * CLOCK)
        units.append((start, end, pts, dts))
    # audio: ADX frames in time order (32 samples a frame)
    aud_time = lambda byte: delay + max(0, (byte - 0x120) // frame_bytes) * 32 / sfreq

    mux_rate = 50000  # 2.5 MB/s in 50-byte units: room for the test streams
    out = bytearray()
    scr = 0
    byterate = mux_rate * 50
    room = PACK - 12
    # pack 0: the Sofdec header
    out += pack_header(scr, mux_rate) + sofdec_header(w, h, rate_code, bitrate, len(pictures),
                                                        channels, sfreq, byterate, room - 6, room - 6)
    # pack 1: system header and padding
    sh = system_header(mux_rate)
    out += pack_header(scr, mux_rate) + sh + padding(room - len(sh))

    vpos, apos, unit = 0, 0, 0
    max_vid = max_aud = 0
    while vpos < len(video) or apos < len(adx):
        scr += int(PACK / byterate * CLOCK)
        # the stream whose next data is due earlier, video ahead by a little
        vt = units[unit][3] / CLOCK if vpos < len(video) else 1e9
        at = aud_time(apos) if apos < len(adx) else 1e9
        if vt - 0.1 <= at:
            pts = dts = None
            # a packet that starts a picture carries its PTS/DTS
            while unit + 1 < len(units) and units[unit + 1][0] <= vpos:
                unit += 1
            starts = [u for u in units if vpos <= u[0] < vpos + room - 20 and (u[0] > 0 or vpos == 0)]
            if starts:
                pts, dts = starts[0][2], starts[0][3]
            data, take = pes(0xE0, room, video[vpos:], pts, dts)
            vpos += take
            max_vid = max(max_vid, take)
            for k in range(unit, len(units)):
                if units[k][1] <= vpos:
                    unit = min(k + 1, len(units) - 1)
        else:
            pts = int(aud_time(apos) * CLOCK)
            data, take = pes(0xC0, room, adx[apos:], pts)
            apos += take
            max_aud = max(max_aud, take)
        out += pack_header(scr, mux_rate) + data
        assert len(out) % PACK == 0, len(out)
    # the program end code in a last pack
    out += pack_header(scr, mux_rate) + padding(room - 4) + b"\x00\x00\x01\xB9"
    return bytes(out), dict(width=w, height=h, fps=fps, frames=len(pictures), channels=channels,
                            sfreq=sfreq, samples=samples)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--video", required=True)
    ap.add_argument("--audio", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    data, info = mux(open(args.video, "rb").read(), open(args.audio, "rb").read())
    open(args.out, "wb").write(data)
    print(f"{args.out}: {len(data)} bytes, {info}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
