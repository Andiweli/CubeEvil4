#!/usr/bin/env python3
"""Test of port/tools/trace_compare.py with synthetic state traces (no disc data): identical
runs, float noise within the tolerance, a run that parts at a known frame (random generator,
GP counts, an enemy, a position beyond the tolerance), a run that ends early and one with
allocations that found no room. Exit code 0 when every case is judged as expected.

    port/tools/test_trace_compare.py
"""

import json
import random
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOL = HERE / "trace_compare.py"


def frame(n, rnd=None, gx=None, em_hp=500, pos=None, hf=0):
    rec = {"frame": n, "Rno0": 3, "room": "0x100", "pl": pos or [1000.0 + n, 860.0, -38480.0 + n * 0.5],
           "cam": {"eye": [0.0, 1000.0, 2000.0 + n], "at": [0.0, 0.0, 0.0], "fov": 50.0, "sw": "C", "r0": 10,
                   "ctrl": [1.0, 2.0, 3.0]},
           "em": [{"slot": 1, "id": "0x12", "r": [1, 11, 0, 0], "hp": em_hp, "pos": [5.0, 6.0, 7.0 + n]}],
           "rnd": rnd if rnd is not None else (n * 7919) & 0xFFFF, "plr": [1, 0, 0, 0, 1000], "n": [40, 1, 12],
           "evt": [n], "gx": gx or [900, 250, 7000, 30, 400, 600, 90, 12000, 1, 2, 3, 0]}
    if n % 60 == 0:
        rec["heap"] = [[0, 0x100000, 0x800000], [11, 0x2000, 0xBC7C0]]
        rec["hf"] = hf
    return rec


def write(path, records):
    with open(path, "w") as f:
        for r in records:
            f.write(json.dumps(r) + "\n")


def run(a, b, extra=()):
    r = subprocess.run([sys.executable, str(TOOL), str(a), str(b), "--names", "i686,x86_64", *extra],
                       capture_output=True, text=True)
    return r.returncode, r.stdout


def main():
    tmp = Path(tempfile.mkdtemp(prefix="trace-compare-"))
    base = [frame(n) for n in range(300)]
    write(tmp / "a.jsonl", base)
    cases = []

    write(tmp / "same.jsonl", base)
    cases.append(("gleiche Läufe", "same", 0, None))

    rng = random.Random(1)
    noisy = []
    for r in base:
        r = json.loads(json.dumps(r))
        r["pl"] = [v + rng.uniform(-0.01, 0.01) for v in r["pl"]]
        r["cam"]["eye"] = [v + rng.uniform(-0.01, 0.01) for v in r["cam"]["eye"]]
        noisy.append(r)
    write(tmp / "noisy.jsonl", noisy)
    cases.append(("Gleitkomma-Rauschen in der Toleranz", "noisy", 0, None))

    write(tmp / "rnd.jsonl", base[:150] + [frame(n, rnd=1) for n in range(150, 300)])
    cases.append(("Zufall ab Bild 150", "rnd", 1, 150))

    gx = list(base)
    gx[200] = frame(200, gx=[900, 251, 7003, 30, 400, 600, 90, 12000, 1, 2, 3, 0])
    write(tmp / "gx.jsonl", gx)
    cases.append(("ein Zeichenauftrag mehr in Bild 200", "gx", 1, 200))

    write(tmp / "em.jsonl", base[:77] + [frame(n, em_hp=480) for n in range(77, 300)])
    cases.append(("Gegner-HP ab Bild 77", "em", 1, 77))

    pos = list(base)
    pos[123] = frame(123, pos=[1000.0 + 123 + 5.0, 860.0, -38480.0 + 61.5])
    write(tmp / "pos.jsonl", pos)
    cases.append(("Leon 5 Einheiten daneben in Bild 123", "pos", 1, 123))

    write(tmp / "short.jsonl", base[:180])
    cases.append(("Lauf endet bei Bild 179 (Absturz)", "short", 1, None))

    write(tmp / "heap.jsonl", [frame(n, hf=3 if n >= 120 else 0) for n in range(300)])
    cases.append(("Speicheranforderungen ohne Platz", "heap", 1, None))

    bad = 0
    for title, name, want_code, want_frame in cases:
        code, out = run(tmp / "a.jsonl", tmp / f"{name}.jsonl", ["--json", str(tmp / f"{name}.json")])
        got = json.loads((tmp / f"{name}.json").read_text()).get("first")
        ok = code == want_code and got == want_frame
        bad += not ok
        print(f"{'ok' if ok else 'FEHLER'}: {title} (Exit {code}, erstes Bild {got})")
        if not ok:
            print("  " + out.replace("\n", "\n  "))
    print(f"trace_compare: {len(cases) - bad} von {len(cases)} Fällen richtig beurteilt")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
