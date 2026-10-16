#!/usr/bin/env python3
"""Movie test of the whole-program port (M9) without game data.

Builds a synthetic Sofdec movie with ffmpeg (MPEG-1 video with B pictures,
stereo ADX sound: two sine tones) and port/tools/make_test_sfd.py, puts it
into a synthetic disc folder (movie/test.sfd) and plays it with
`re4_host --movie-test`, which drives the CRI middleware of src/lib the way
game/sofdec.cpp does (tests/movie_probe.cpp). Then:

  - every frame the player handed out is compared with ffmpeg's decode of
    the same MPEG-1 stream (PSNR per plane; the CRI IDCT is a float AAN
    transform, ffmpeg's an integer one, so a little noise is expected),
  - frames come in order, at the movie's frame rate (fields per frame),
  - the sound (32 kHz stereo from the AI output) holds the two tones on the
    right channels, and starts close to the first frame (A/V sync).

    test_whole_movie.py BUILD_DIR [runner ...]     (runner: e.g. qemu-arm)
Exit code 0 = passed; skipped (exit 0) without ffmpeg.
"""
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WIDTH, HEIGHT, SECONDS = 320, 240, 4
TONE_L, TONE_R = 440.0, 660.0


def build_fst(entries):
    """entries: [(path parts, size)] of files; returns the FST (big-endian, like the disc)."""
    names = bytearray()
    nodes = [None]  # root

    def add_name(n):
        off = len(names)
        names.extend(n.encode() + b"\0")
        return off

    dirs = {(): 0}
    offset = 0x100000
    for parts, size in entries:
        for depth in range(1, len(parts)):
            key = tuple(parts[:depth])
            if key not in dirs:
                dirs[key] = len(nodes)
                nodes.append(["dir", add_name(parts[depth - 1]), dirs[tuple(parts[:depth - 1])], key])
        nodes.append(["file", add_name(parts[-1]), offset, size])
        offset += (size + 0x7FFF) & ~0x7FFF
    out = bytearray()
    for i, n in enumerate(nodes):
        if i == 0:
            out += struct.pack(">III", 0x01000000, 0, len(nodes))
        elif n[0] == "dir":
            last = max(j for j, m in enumerate(nodes) if m is not None and m[0] == "file"
                       and j > i) + 1
            out += struct.pack(">III", 0x01000000 | n[1], n[2], last)
        else:
            out += struct.pack(">III", n[1], n[2], n[3])
    return bytes(out + names)


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, **kw)


def psnr(a, b):
    if len(a) != len(b) or not a:
        return 0.0
    err = sum((x - y) * (x - y) for x, y in zip(a, b)) / len(a)
    return 99.0 if err == 0 else 10 * math.log10(255 * 255 / err)


def tone_power(samples, rate, freq):
    """Goertzel power of `freq` relative to the signal power."""
    k = 2 * math.cos(2 * math.pi * freq / rate)
    s1 = s2 = 0.0
    for x in samples:
        s0 = x + k * s1 - s2
        s2, s1 = s1, s0
    p = s1 * s1 + s2 * s2 - k * s1 * s2
    total = sum(x * x for x in samples) or 1.0
    return p / (total * len(samples) / 2)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    build = Path(sys.argv[1]).resolve()
    runner = sys.argv[2:]
    if not shutil.which("ffmpeg"):
        print("movie test skipped: ffmpeg missing")
        return 0
    work = Path(tempfile.mkdtemp(prefix="re4-movie-"))
    failures = []
    try:
        m1v, adx, sfd = work / "test.m1v", work / "test.adx", work / "test.sfd"
        run(["ffmpeg", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
             f"testsrc=size={WIDTH}x{HEIGHT}:rate=30000/1001:duration={SECONDS}",
             "-c:v", "mpeg1video", "-b:v", "2M", "-bf", "2", "-g", "15", "-f", "mpeg1video", str(m1v)])
        run(["ffmpeg", "-loglevel", "error", "-y",
             "-f", "lavfi", "-i", f"sine=frequency={TONE_L}:duration={SECONDS}:sample_rate=48000",
             "-f", "lavfi", "-i", f"sine=frequency={TONE_R}:duration={SECONDS}:sample_rate=48000",
             "-filter_complex", "[0][1]amerge=inputs=2", "-ac", "2", "-c:a", "adpcm_adx", "-f", "adx", str(adx)])
        out = run([sys.executable, str(ROOT / "port/tools/make_test_sfd.py"), "--video", str(m1v),
                   "--audio", str(adx), "--out", str(sfd)]).stdout.decode().strip()
        print(f"  {out}")
        ref = run(["ffmpeg", "-loglevel", "error", "-idct", "faani", "-i", str(m1v), "-f", "rawvideo",
                   "-pix_fmt", "yuv420p", "-"]).stdout
        fsize = WIDTH * HEIGHT * 3 // 2
        ref_frames = [ref[i:i + fsize] for i in range(0, len(ref), fsize)]

        data = work / "data"
        (data / "disc0/sys").mkdir(parents=True)
        (data / "disc0/files/movie").mkdir(parents=True)
        shutil.copy(sfd, data / "disc0/files/movie/test.sfd")
        (data / "disc0/sys/fst.bin").write_bytes(build_fst([(["movie", "test.sfd"], sfd.stat().st_size)]))
        (data / "disc0/sys/boot.bin").write_bytes(b"G4BE08".ljust(0x440, b"\0"))

        yuv, wav = work / "out.yuv", work / "out.wav"
        fields = int(SECONDS * 60 + 120)
        env = dict(os.environ, RE4_DATA_DIR=str(data), RE4_VI_FAST="1")
        proc = subprocess.run([*runner, str(build / "re4_host"), "--movie-test", "movie/test.sfd", str(yuv),
                               str(wav), str(fields)], cwd=build, env=env, capture_output=True, timeout=600)
        log = proc.stderr.decode(errors="replace")
        for line in log.splitlines():
            if "[movie]" in line or "movie-test" in line or "stub" in line.lower():
                print("  " + line.replace("[re4] ", ""))
        if proc.returncode != 0:
            failures.append(f"re4_host --movie-test exit {proc.returncode}")
            print("\n".join("    " + l for l in log.splitlines()[-25:]))

        # frames
        got = yuv.read_bytes() if yuv.exists() else b""
        frames = []
        pos = 0
        while pos + 16 <= len(got):
            magic, fno, field, size = struct.unpack("<IIII", got[pos:pos + 16])
            w, h = size & 0xFFFF, size >> 16
            n = w * h * 3 // 2
            frames.append((fno, field, got[pos + 16:pos + 16 + n]))
            pos += 16 + n
        worst = 99.0
        fnos = [f[0] for f in frames]
        for fno, field, planes in frames:
            if fno >= len(ref_frames):
                failures.append(f"frame number {fno} beyond the {len(ref_frames)} frames of the movie")
                break
            r = ref_frames[fno]
            ys, cs = WIDTH * HEIGHT, WIDTH * HEIGHT // 4
            p = min(psnr(planes[:ys], r[:ys]), psnr(planes[ys:ys + cs], r[ys:ys + cs]),
                    psnr(planes[ys + cs:], r[ys + cs:]))
            worst = min(worst, p)
        print(f"  frames: {len(frames)} of {len(ref_frames)}, numbers {fnos[:3]}..{fnos[-3:] if fnos else []}, "
              f"worst PSNR {worst:.1f} dB")
        if len(frames) < len(ref_frames) * 0.9:
            failures.append(f"only {len(frames)} of {len(ref_frames)} frames shown")
        if fnos != sorted(fnos):
            failures.append("frames out of order")
        if worst < 35:
            failures.append(f"a frame differs from ffmpeg's decode (PSNR {worst:.1f} dB)")
        if len(frames) > 10:
            span = frames[-1][1] - frames[0][1]
            per = span / max(1, frames[-1][0] - frames[0][0])
            print(f"  pacing: {per:.2f} fields per frame (29.97 fps = 2.00), first frame at field {frames[0][1]}")
            if not 1.8 <= per <= 2.2:
                failures.append(f"frame pacing {per:.2f} fields per frame")

        # sound
        raw = wav.read_bytes()[44:] if wav.exists() else b""
        pcm = struct.unpack(f"<{len(raw) // 2}h", raw[:len(raw) // 4 * 4])
        left, right = pcm[0::2], pcm[1::2]
        first = next((i for i, v in enumerate(left) if abs(v) > 1000), None)
        if first is None:
            failures.append("no sound")
        else:
            seg_l = left[first + 3200:first + 3200 + 8000]
            seg_r = right[first + 3200:first + 3200 + 8000]
            pl, pr = tone_power(seg_l, 32000, TONE_L), tone_power(seg_r, 32000, TONE_R)
            xl = tone_power(seg_l, 32000, TONE_R)
            peak = max(abs(v) for v in seg_l) if seg_l else 0
            start_field = first / (32000 / 59.94)
            print(f"  sound: {len(left)} frames, starts at field {start_field:.0f}, peak {peak}, "
                  f"tone share left {pl:.2f} right {pr:.2f} (other tone on the left {xl:.3f})")
            if pl < 0.8 or pr < 0.8 or xl > 0.05:
                failures.append("the tones are not on their channels")
            if frames:
                lag = (start_field - frames[0][1]) / 59.94
                print(f"  A/V: sound starts {lag * 1000:+.0f} ms after the first frame")
                if abs(lag) > 0.15:
                    failures.append(f"sound and picture start {lag * 1000:+.0f} ms apart")
    finally:
        if os.environ.get("RE4_MOVIE_KEEP"):
            print(f"  kept: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)
    for f in failures:
        print(f"  FAILED: {f}")
    print("movie test: " + ("passed" if not failures else f"{len(failures)} failed"))
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
