#!/usr/bin/env python3
"""The disc-data mode of re4_format_compare, tested without disc data.

Writes the synthetic samples as a disc folder (--write-samples), packs the
room archive into st1/r100.das with the Yz2 fixture encoder of
generate_yz2_fixtures.py (literal symbols, checked by tools/motion/yz2.py),
then runs re4_format_compare --data on the folder: every file must be found
(the room file through the app's Yz2 decoder, em/pl00.drs through the DVD
container) and compare equal.

    test_whole_format_compare.py BUILD_DIR [RUNNER ...]   (RUNNER: e.g. qemu-arm)
"""
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_yz2_fixtures import encode  # noqa: E402
import yz2  # noqa: E402  (tools/motion, put on the path by generate_yz2_fixtures)


def yz2_file(data: bytes) -> bytes:
    """data as a Yz2 file the way the disc stores it: "packed<TAB>size<LF>",
    padding to 32, the stream and the coder's four flush bytes (the packed
    field excludes them; ports/yz2.cpp checks both)."""
    image = encode([(0, 0x400 + b) for b in data], data)
    stream = image[yz2.parse_header(image)[2]:]
    header = f"{len(stream):X}\t{len(data):X}\n".encode("ascii")
    return header + bytes(((len(header) + 32) & ~31) - len(header)) + stream + bytes(4)


EXPECTED = ["st1/r100.das", "etc/emleon00.esl", "debug/roominfo.dat", "Evd/r100s03.evd", "em/pl00.drs", "op/op01.das",
            "SS/cmn/map_obj1.dat", "SS/cmn/itm1a.bin"]


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    build = Path(sys.argv[1]).resolve()
    tool = [*sys.argv[2:], str(build / "re4_format_compare")]
    with tempfile.TemporaryDirectory(prefix="re4-format-compare-") as tmp:
        subprocess.run([*tool, "--write-samples", tmp], check=True, capture_output=True)
        st1 = Path(tmp) / "disc0" / "files" / "st1"
        archive = (st1 / "r100.arc").read_bytes()
        (st1 / "r100.das").write_bytes(yz2_file(archive))
        (st1 / "r100.arc").unlink()
        run = subprocess.run([*tool, "--data", tmp], capture_output=True, text=True)
    lines = run.stdout.splitlines()
    missing = [name for name in EXPECTED
               if not any(line.split()[:2] == ["OK", name] for line in lines if line.strip())]
    if run.returncode != 0 or missing:
        print(run.stdout)
        print(f"FAIL: exit {run.returncode}, not compared equal: {', '.join(missing) or '-'}")
        return 1
    print(f"PASS: {len(EXPECTED)} files of a synthetic disc folder compare equal "
          f"({lines[-1].split(': ', 1)[-1]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
