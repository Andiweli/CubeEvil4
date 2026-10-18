#!/usr/bin/env python3
"""Compares two state traces (runtime/state_trace.cpp, RE4_TRACE) frame by frame: the 32-bit
and the 64-bit run of the same input (whole_r100_check.sh --compare, milestone 12).

Both runs see the same key presses at the same frames (RE4_PAD_SCRIPT, RE4_VI_FAST, the disc
reads complete at once), so both traces should agree frame by frame. Compared per frame:

  exact     game step, room, the random generator, the player's routine and hit points, the
            live works (objects, events, effect slots), the frames of the running events, every
            enemy (slot, id, routine, hit points), the sound streams, the camera switches and
            routine, the room save flags, the GP counts of the frame (commands, draws,
            vertices, display list calls, XF/BP writes, textures, texture bytes, copies, TLUT
            loads, unknown commands)
  tolerant  positions (player, enemies, camera eye / target / control, field of view): the
            32-bit build runs other code for some float operations (libm), small differences are
            no error (--abs, --rel)

Reported: the first frame where the two part (with the values of both sides), the first frame
of each group, how far the positions drift before the first exact difference, the end of each
trace (a crash ends one early), and the heaps: the least free space of each heap on both sides
and the allocations that found no room (patch 0081).

    port/tools/trace_compare.py REF.jsonl OTHER.jsonl [--names i686,x86_64] [--abs 1.0]
        [--rel 1e-4] [--json OUT]

The traces may be gzipped (.gz). Exit code 0 when the traces agree (within the tolerance) up to
the end of the shorter one and both end at the same frame, 1 otherwise, 2 on bad input.
"""

import argparse
import gzip
import json
import sys

# fields compared exactly; the value is the group name in the report
EXACT = {
    "Rno0": "Spielschritt", "room": "Raum", "rnd": "Zufall", "plr": "Spieler (Routine, HP)",
    "n": "Anzahl Objekte/Ereignisse/Effekte", "evt": "Ereignisbild", "rsf": "Raum-Flags",
    "str": "Tonspuren", "gx": "GX-Kennzahlen",
}
GX_NAMES = ["Befehle", "Zeichenaufträge", "Ecken", "Listenaufrufe", "XF", "BP", "Texturen", "Texturbytes",
            "Bildkopien", "Texturkopien", "TLUT", "unbekannt"]
PLR_NAMES = ["r0", "r1", "r2", "r3", "HP"]


def read(path):
    """frame -> record; the frames in their order"""
    opener = gzip.open if str(path).endswith(".gz") else open
    frames, order = {}, []
    with opener(path, "rt", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line.startswith("{"):
                continue
            try:
                rec = json.loads(line)
            except ValueError:
                continue  # the last line of a crashed run may be cut
            n = rec.get("frame")
            if isinstance(n, int) and n not in frames:
                frames[n] = rec
                order.append(n)
    return frames, order


def near(a, b, abs_tol, rel_tol):
    return abs(a - b) <= max(abs_tol, rel_tol * max(abs(a), abs(b)))


def vec_diff(a, b):
    if not isinstance(a, list) or not isinstance(b, list) or len(a) != len(b):
        return float("inf")
    return max((abs(x - y) for x, y in zip(a, b)), default=0.0)


def floats(rec):
    """name -> vector of the positions of a record"""
    out = {}
    if "pl" in rec:
        out["Leon"] = rec["pl"]
    cam = rec.get("cam") or {}
    for k, name in (("eye", "Kamera-Auge"), ("at", "Kamera-Ziel"), ("ctrl", "CamCtrl-Auge")):
        if k in cam:
            out[name] = cam[k]
    if "fov" in cam:
        out["Kamera-Winkel"] = [cam["fov"]]
    for e in rec.get("em") or []:
        out[f"Gegner {e.get('slot')} ({e.get('id')})"] = e.get("pos")
    return out


def exact(rec):
    """group -> value of the exactly compared fields"""
    out = {}
    for k, name in EXACT.items():
        if k in rec:
            out[name] = rec[k]
    cam = rec.get("cam") or {}
    if cam:
        out["Kamera-Schalter"] = [cam.get("sw"), cam.get("r0")]
    out["Gegner"] = [[e.get("slot"), e.get("id"), e.get("r"), e.get("hp")] for e in rec.get("em") or []]
    return out


def show(group, value):
    if group == "GX-Kennzahlen" and isinstance(value, list):
        return ", ".join(f"{n} {v}" for n, v in zip(GX_NAMES, value))
    if group == "Spieler (Routine, HP)" and isinstance(value, list):
        return " ".join(f"{n} {v}" for n, v in zip(PLR_NAMES, value))
    text = json.dumps(value, separators=(",", ":"))
    return text if len(text) <= 300 else text[:300] + " ..."


def detail(group, a, b):
    """the parts of a group that differ (GX counts, player fields, enemies by slot)"""
    if isinstance(a, list) and isinstance(b, list) and group in ("GX-Kennzahlen", "Spieler (Routine, HP)"):
        names = GX_NAMES if group == "GX-Kennzahlen" else PLR_NAMES
        return [f"{n}: {x} / {y}" for n, x, y in zip(names, a, b) if x != y]
    if group == "Gegner":
        sa = {e[0]: e for e in a}
        sb = {e[0]: e for e in b}
        out = []
        for slot in sorted(set(sa) | set(sb), key=lambda s: (s is None, s)):
            if sa.get(slot) != sb.get(slot):
                out.append(f"Platz {slot}: {show(group, sa.get(slot))} / {show(group, sb.get(slot))}")
        return out[:6]
    return [f"{show(group, a)} / {show(group, b)}"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ref")
    ap.add_argument("other")
    ap.add_argument("--names", default="32 bit,64 bit", help="names of the two runs in the report")
    ap.add_argument("--abs", type=float, default=1.0, help="tolerated difference of a position (game units)")
    ap.add_argument("--rel", type=float, default=1e-4, help="tolerated relative difference of a position")
    ap.add_argument("--json", help="write the result as JSON (first frame, groups) to this file")
    args = ap.parse_args()
    na, nb = (args.names.split(",") + ["2"])[:2]
    try:
        fa, oa = read(args.ref)
        fb, ob = read(args.other)
    except OSError as e:
        print(f"Vergleich: {e}")
        return 2
    if not oa or not ob:
        print(f"Vergleich: leerer Trace ({na}: {len(oa)} Bilder, {nb}: {len(ob)} Bilder)")
        return 2
    common = [n for n in oa if n in fb]
    first = None          # (frame, [lines])
    first_group = {}      # group -> frame
    first_exact = None
    drift = {}            # name -> (largest difference, frame) before the first exact difference
    for n in common:
        a, b = fa[n], fb[n]
        lines = []
        ea, eb = exact(a), exact(b)
        exact_diff = False
        for g in sorted(set(ea) | set(eb)):
            if ea.get(g) != eb.get(g):
                exact_diff = True
                first_group.setdefault(g, n)
                lines += [f"{g}: {d}" for d in detail(g, ea.get(g), eb.get(g))]
        pa, pb = floats(a), floats(b)
        for name in sorted(set(pa) & set(pb)):
            d = vec_diff(pa[name], pb[name])
            if first_exact is None:
                if d > drift.get(name, (0.0, n))[0]:
                    drift[name] = (d, n)
            va, vb = pa[name], pb[name]
            if d != float("inf") and all(near(x, y, args.abs, args.rel) for x, y in zip(va, vb)):
                continue
            first_group.setdefault("Positionen", n)
            lines.append(f"{name}: {show('', va)} / {show('', vb)} (Unterschied {d:.3f})")
        if exact_diff and first_exact is None:
            first_exact = n
        if lines and first is None:
            first = (n, lines)
    print(f"Vergleich {na} / {nb}: {len(oa)} / {len(ob)} Bilder im Trace, {len(common)} gemeinsame")
    result = {"frames": [len(oa), len(ob)], "common": len(common), "first": None, "groups": {}}
    code = 0
    if first is None:
        print(f"  Alle gemeinsamen Bilder stimmen überein (Positionen bis {args.abs} Einheiten / "
              f"{args.rel:g} relativ toleriert).")
    else:
        code = 1
        n, lines = first
        before = max([m for m in common if m < n], default=None)
        print(f"  ERSTES ABWEICHENDES BILD: {n} (Spielschritt {fa[n].get('Rno0')}, Raum {fa[n].get('room')}); "
              f"Werte {na} / {nb}:")
        for line in lines[:24]:
            print(f"    {line}")
        if len(lines) > 24:
            print(f"    (+{len(lines) - 24} weitere)")
        if before is not None:
            print(f"  Bild davor ({before}) stimmt noch überein.")
        print("  Erstes Bild je Bereich:")
        for g, m in sorted(first_group.items(), key=lambda x: x[1]):
            print(f"    {g}: Bild {m}")
        if drift:
            worst = sorted(drift.items(), key=lambda x: -x[1][0])[:4]
            print("  Größte Abweichung der Positionen vor der ersten genauen Abweichung: " +
                  ", ".join(f"{k} {v[0]:.3f} (Bild {v[1]})" for k, v in worst))
        result["first"] = n
        result["groups"] = first_group
        result["gx_frames"] = [m for m in (before, n) if m is not None]
    last_a, last_b = oa[-1], ob[-1]
    if last_a != last_b:
        code = 1
        shorter = na if last_a < last_b else nb
        print(f"  Ende: {na} bei Bild {last_a}, {nb} bei Bild {last_b} ({shorter} endet früher: Absturz oder "
              f"Stillstand?)")
    else:
        print(f"  Ende: beide bei Bild {last_a}")
    # heaps: the least free space of each heap, and failed allocations
    def heaps(frames, order):
        low, fails = {}, 0
        for n in order:
            for h in frames[n].get("heap") or []:
                if len(h) == 3 and h[1] >= 0:
                    no, free, size = h
                    if no not in low or free < low[no][0]:
                        low[no] = (free, size, n)
            fails = max(fails, frames[n].get("hf", 0))
        return low, fails
    ha, fla = heaps(fa, oa)
    hb, flb = heaps(fb, ob)
    if ha or hb:
        print("  Heaps (wenigster freier Platz, alle 60 Bilder gemessen): Heap: " + na + " / " + nb)
        for no in sorted(set(ha) | set(hb)):
            def fmt(x):
                return f"{x[0]:#x} frei von {x[1]:#x} (Bild {x[2]})" if x else "-"
            warn = ""
            if no in hb and hb[no][1] > 0 and hb[no][0] < hb[no][1] * 0.05:
                warn = "  <- auf " + nb + " fast voll"
            print(f"    {no}: {fmt(ha.get(no))} / {fmt(hb.get(no))}{warn}")
        print(f"  Anforderungen ohne Platz (patch 0081): {na} {fla}, {nb} {flb}" +
              ("  <- mehr auf " + nb if flb > fla else ""))
        if flb > fla:
            code = 1
        result["heap_failures"] = [fla, flb]
    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(result, f, indent=1)
    return code


if __name__ == "__main__":
    sys.exit(main())
