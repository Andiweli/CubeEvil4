#!/usr/bin/env python3
"""The 32/64-bit comparison without game data (milestone 12): the synthetic Sofdec movie of
port/tools/test_whole_movie.py played by two builds (i686 and x86_64), each writing a state trace
per field (tests/movie_probe.cpp, RE4_TRACE: the frame shown, the player status, checksums of the
decoded picture and of the sound, the GP counts), compared frame by frame with
port/tools/trace_compare.py. The CRI middleware is the largest part of the game code with views of
its own structures (patch 0070); both builds must decode, time and hand out the very same pictures
and sound. Both run in fast mode (RE4_VI_FAST, as the PC check runs the game) with the float
functions of the C library through their double versions (host/det_math.c, as in
whole_r100_check.sh --compare).

    port/tools/test_movie_compare.py BUILD_A BUILD_B [--names i686,x86_64]

Exit code 0 when the traces agree; skipped (exit 0) without ffmpeg.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import test_whole_movie as M  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--names", default="i686,x86_64")
    args = ap.parse_args()
    if not shutil.which("ffmpeg"):
        print("movie comparison skipped: ffmpeg missing")
        return 0
    work = Path(tempfile.mkdtemp(prefix="re4-movie-compare-"))
    try:
        m1v, adx, sfd = work / "test.m1v", work / "test.adx", work / "test.sfd"
        M.run(["ffmpeg", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
               f"testsrc=size={M.WIDTH}x{M.HEIGHT}:rate=30000/1001:duration={M.SECONDS}",
               "-c:v", "mpeg1video", "-b:v", "2M", "-bf", "2", "-g", "15", "-f", "mpeg1video", str(m1v)])
        M.run(["ffmpeg", "-loglevel", "error", "-y",
               "-f", "lavfi", "-i", f"sine=frequency={M.TONE_L}:duration={M.SECONDS}:sample_rate=48000",
               "-f", "lavfi", "-i", f"sine=frequency={M.TONE_R}:duration={M.SECONDS}:sample_rate=48000",
               "-filter_complex", "[0][1]amerge=inputs=2", "-ac", "2", "-c:a", "adpcm_adx", "-f", "adx", str(adx)])
        M.run([sys.executable, str(M.ROOT / "port/tools/make_test_sfd.py"), "--video", str(m1v),
               "--audio", str(adx), "--out", str(sfd)])
        data = work / "data"
        (data / "disc0/sys").mkdir(parents=True)
        (data / "disc0/files/movie").mkdir(parents=True)
        shutil.copy(sfd, data / "disc0/files/movie/test.sfd")
        (data / "disc0/sys/fst.bin").write_bytes(M.build_fst([(["movie", "test.sfd"], sfd.stat().st_size)]))
        (data / "disc0/sys/boot.bin").write_bytes(b"G4BE08".ljust(0x440, b"\0"))
        fields = int(M.SECONDS * 60 + 120)
        traces = []
        for i, build in enumerate((Path(args.a).resolve(), Path(args.b).resolve())):
            trace = work / f"trace-{i}.jsonl"
            gx = work / f"gx-{i}"
            gx.mkdir()
            # RE4_MOVIE_GX: frames 10, 60 and 100 drawn by cSofdec::drawTex through the GX emulation
            env = dict(os.environ, RE4_DATA_DIR=str(data), RE4_VI_FAST="1", RE4_MOVIE_FAST="1", RE4_TRACE=str(trace),
                       RE4_MOVIE_GX=str(gx))
            det = build / "libre4_detmath.so"
            if det.exists():
                env["LD_PRELOAD"] = str(det)
            proc = subprocess.run([str(build / "re4_host"), "--movie-test", "movie/test.sfd", str(work / f"{i}.yuv"),
                                   str(work / f"{i}.wav"), str(fields)], cwd=build, env=env, capture_output=True,
                                  timeout=600)
            if proc.returncode != 0:
                print(f"movie comparison: {build.name}: re4_host --movie-test exit {proc.returncode}")
                print("  " + "\n  ".join(proc.stderr.decode(errors="replace").splitlines()[-8:]))
                return 1
            traces.append(trace)
        r = subprocess.run([sys.executable, str(HERE / "trace_compare.py"), str(traces[0]), str(traces[1]),
                            "--names", args.names], capture_output=True, text=True)
        print("  " + r.stdout.strip().replace("\n", "\n  "))
        print("movie comparison: " + ("passed" if r.returncode == 0 else "the builds part"))
        return r.returncode
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
