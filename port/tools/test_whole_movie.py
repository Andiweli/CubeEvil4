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
    right channels, and starts close to the first frame (A/V sync),
  - frame 10 drawn as the game draws it (cSofdec::drawTex) through the app's
    GX renderer has the colours of the decoded frame (if it is built),
  - the same movie in fast mode (RE4_VI_FAST, as the PC check runs the game:
    fields at once, the CRI clock counts them) still plays to its end, at
    the movie's frame rate and in sync.

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
import time
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


def read_frames(path):
    """The frame records of tests/movie_probe.cpp: (frame number, field, planes)."""
    got = path.read_bytes() if path.exists() else b""
    frames = []
    pos = 0
    while pos + 16 <= len(got):
        magic, fno, field, size = struct.unpack("<IIII", got[pos:pos + 16])
        w, h = size & 0xFFFF, size >> 16
        n = w * h * 3 // 2
        frames.append((fno, field, got[pos + 16:pos + 16 + n]))
        pos += 16 + n
    return frames


def read_sound(path):
    """Left and right samples of the WAV and the first sample over 1000 (or None)."""
    raw = path.read_bytes()[44:] if path.exists() else b""
    pcm = struct.unpack(f"<{len(raw) // 2}h", raw[:len(raw) // 4 * 4])
    left, right = pcm[0::2], pcm[1::2]
    first = next((i for i, v in enumerate(left) if abs(v) > 1000), None)
    return left, right, first


def fields_per_frame(frames):
    span = frames[-1][1] - frames[0][1]
    return span / max(1, frames[-1][0] - frames[0][0])


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
        gxdir = work / "gx"
        gxdir.mkdir()
        # real time as on the device (the probe paces the fields itself; no
        # RE4_VI_FAST, that switches the CRI clock to counting fields)
        env = {k: v for k, v in os.environ.items() if k != "RE4_VI_FAST"}
        env.update(RE4_DATA_DIR=str(data), RE4_MOVIE_GX=str(gxdir))
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
        frames = read_frames(yuv)
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
            per = fields_per_frame(frames)
            print(f"  pacing: {per:.2f} fields per frame (29.97 fps = 2.00), first frame at field {frames[0][1]}")
            if not 1.8 <= per <= 2.2:
                failures.append(f"frame pacing {per:.2f} fields per frame")

        # the picture as the game draws it (cSofdec::drawTex: Y8 and C4:4 textures,
        # the YUV -> RGB TEV set-up) through the app's GX renderer, if built
        # (port/build/app-host-gl, RE4_BUILD_GL_CHECK=ON): against the BT.601
        # conversion of the decoded frame; the chroma is filtered bilinearly by
        # the texture unit, so edges differ, the colours on average must not
        render = Path(os.environ.get("RE4_GX_RENDER", ROOT / "port/build/app-host-gl/gx_chunk_render"))
        chunk = gxdir / "gx-movie-00010.bin"
        if render.exists() and chunk.exists():
            subprocess.run([str(render), str(gxdir), str(chunk)], capture_output=True, timeout=120)
            ppm = gxdir / "gx-movie-00010.ppm"
            frame10 = next((f for f in frames if f[0] == 10), None)
            if ppm.exists() and frame10:
                head = ppm.read_bytes().split(b"\n", 3)
                pw, ph = map(int, head[1].split())
                px = head[3]
                sys.path.insert(0, str(ROOT / "port/tools"))
                import movie_report
                exp = movie_report.to_rgb(frame10[2], WIDTH, HEIGHT)
                ox, oy = (512 - WIDTH) // 2, (448 - HEIGHT) // 2 - 0
                err, diff, n = 0.0, [0.0, 0.0, 0.0], 0
                for yy in range(8, HEIGHT - 8, 2):
                    for xx in range(8, WIDTH - 8, 2):
                        for c in range(3):
                            a = px[((oy + yy) * pw + ox + xx) * 3 + c]
                            b = exp[(yy * WIDTH + xx) * 3 + c]
                            err += (a - b) ** 2
                            diff[c] += a - b
                        n += 1
                mse = err / (3 * n)
                p_gx = 99.0 if mse == 0 else 10 * math.log10(255 * 255 / mse)
                mean = [d / n for d in diff]
                print(f"  GX picture (drawTex through the app's renderer): PSNR {p_gx:.1f} dB, mean difference "
                      f"R {mean[0]:+.1f} G {mean[1]:+.1f} B {mean[2]:+.1f}")
                if p_gx < 20 or max(abs(m) for m in mean) > 4:
                    failures.append("the movie picture through GX has the wrong colours")
            else:
                failures.append("no GX picture of the movie")
        else:
            print("  GX picture: skipped (no gx_chunk_render or no chunk)")

        # sound
        left, right, first = read_sound(wav)
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

        # fast mode (RE4_VI_FAST=1 as the PC check runs the game; the probe takes
        # the fields from vi.cpp, the CRI clock counts them: runtime/vi.cpp
        # re4_cri_timer): the movie must still end on its own, in step
        fyuv, fwav = work / "fast.yuv", work / "fast.wav"
        env = dict(os.environ, RE4_DATA_DIR=str(data), RE4_VI_FAST="1", RE4_MOVIE_FAST="1")
        began = time.monotonic()
        proc = subprocess.run([*runner, str(build / "re4_host"), "--movie-test", "movie/test.sfd", str(fyuv),
                               str(fwav), str(fields)], cwd=build, env=env, capture_output=True, timeout=600)
        took = time.monotonic() - began
        flog = proc.stderr.decode(errors="replace")
        end = next((l.split("[movie] end: ", 1)[1] for l in flog.splitlines() if "[movie] end: " in l), "no end line")
        fframes = read_frames(fyuv)
        _, _, ffirst = read_sound(fwav)
        per = fields_per_frame(fframes) if len(fframes) > 10 else 0.0
        lag = (ffirst / (32000 / 59.94) - fframes[0][1]) / 59.94 if fframes and ffirst is not None else None
        print(f"  fast mode (RE4_VI_FAST, the PC check): {len(fframes)} frames in {took:.1f} s, "
              f"{per:.2f} fields per frame, A/V {'?' if lag is None else f'{lag * 1000:+.0f} ms'}; end: {end}")
        if proc.returncode != 0:
            failures.append(f"fast mode: the movie did not end (re4_host --movie-test exit {proc.returncode})")
        if len(fframes) < len(ref_frames) * 0.9:
            failures.append(f"fast mode: only {len(fframes)} of {len(ref_frames)} frames shown")
        if not 1.8 <= per <= 2.2:
            failures.append(f"fast mode: frame pacing {per:.2f} fields per frame")
        if lag is None or abs(lag) > 0.15:
            failures.append("fast mode: sound and picture apart")
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
