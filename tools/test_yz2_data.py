#!/usr/bin/env python3
"""Verify every supplied room against frozen original-PowerPC fingerprints.
Usage: test_yz2_data.py yz2_tool GAME_ROOT [--oracle]
GAME_ROOT contains disc1/files, disc2/files and disc1/sys/main.dol.
With --oracle, also execute original PowerPC code and compare actual bytes.
"""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent.parent
if len(sys.argv) not in (3, 4):
    raise SystemExit(__doc__)
tool = str(Path(sys.argv[1]).resolve())
game = Path(sys.argv[2]).resolve()
use_oracle = len(sys.argv) == 4 and sys.argv[3] == "--oracle"
if len(sys.argv) == 4 and not use_oracle:
    raise SystemExit("Unknown option")
if use_oracle:
    from yz2_oracle import decode
count = 0
with tempfile.TemporaryDirectory(prefix="re4-yz2-") as directory:
    output = Path(directory) / "decoded.dat"
    for line in (root / "app/src/main/assets/rooms.tsv").read_text().splitlines():
        if line.startswith("#"):
            continue
        disc, path, size, expected, source_hash = line.split("\t")
        source = game / ("disc" + disc) / "files" / path
        if hashlib.sha256(source.read_bytes()).hexdigest() != source_hash:
            raise SystemExit("Wrong source: " + str(source))
        subprocess.run([tool, str(source), str(output)], check=True, stdout=subprocess.DEVNULL)
        data = output.read_bytes()
        if len(data) != int(size) or hashlib.sha256(data).hexdigest() != expected:
            raise SystemExit("Different output: " + str(source))
        if use_oracle and decode(source, game / "disc1/sys/main.dol") != data:
            raise SystemExit("Different PowerPC output: " + str(source))
        count += 1
        if count % 10 == 0:
            print(count, "rooms PASS", flush=True)
print(count, "room archives PASS")
