#!/usr/bin/env python3
"""Levels of a WAV the core wrote (RE4_AUDIO_WAV, 32 kHz stereo, 16 bit): per
window of N seconds the loudness (RMS in dBFS) and peaks of left and right,
clipped samples, and whether the window is silent. For the reports of
whole_r100_check.sh and test-core-app.ps1.

  audio_wav_report.py ton.wav [seconds per window]
"""
import math
import struct
import sys
import wave


def db(v):
    return -120.0 if v <= 0 else 20.0 * math.log10(v / 32768.0)


def main():
    path = sys.argv[1]
    window = float(sys.argv[2]) if len(sys.argv) > 2 else 5.0
    try:
        w = wave.open(path, "rb")
    except (OSError, EOFError, wave.Error) as e:
        print(f"  WAV nicht lesbar: {e}")
        return 1
    rate, channels, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
    frames = w.getnframes()
    if channels != 2 or width != 2:
        print(f"  WAV-Format unerwartet: {channels} Kanäle, {width * 8} bit")
        return 1
    print(f"  {frames / rate:.1f} s Ton ({rate} Hz stereo); je {window:g} s: Lautheit L/R in dBFS, Spitze, übersteuert")
    per = int(rate * window)
    loud_total, silent, clipped_total, t = 0, 0, 0, 0.0
    while True:
        data = w.readframes(per)
        if not data:
            break
        n = len(data) // 4
        v = struct.unpack("<%dh" % (n * 2), data[: n * 4])
        left, right = v[0::2], v[1::2]
        rl = math.sqrt(sum(x * x for x in left) / n)
        rr = math.sqrt(sum(x * x for x in right) / n)
        peak = max(max(abs(x) for x in left), max(abs(x) for x in right))
        clipped = sum(1 for x in v if x >= 32767 or x <= -32768)
        clipped_total += clipped
        quiet = peak < 64
        silent += quiet
        loud_total += not quiet
        print(f"    {t:6.1f} s: {db(rl):6.1f} / {db(rr):6.1f} dB, Spitze {db(peak):6.1f} dB"
              f"{', übersteuert ' + str(clipped) if clipped else ''}{', still' if quiet else ''}")
        t += n / rate
    print(f"  Zusammen: {loud_total} Abschnitte mit Ton, {silent} still, {clipped_total} übersteuerte Werte")
    return 0


if __name__ == "__main__":
    sys.exit(main())
