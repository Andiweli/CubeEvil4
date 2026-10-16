#!/usr/bin/env python3
"""Summarises a CPU profile of the app taken on the device with simpleperf
(port/scripts/test-core-app.ps1, key P): where the time of each thread goes,
by library and by function.

    android_profile.py RAW.txt THREADS.txt [DIR...]

RAW.txt:     simpleperf report --sort tid,dso,vaddr_in_file -n
THREADS.txt: simpleperf report --sort tid,comm -n
DIR:         folders searched for the libraries with symbols (default: the core
             build of whole_build_android.sh and the app's native build, as in
             android_symbolize.sh). The addresses of the game core and the app
             are turned into function names with llvm-symbolizer of the NDK;
             system libraries (the GPU driver, libc) are counted as a whole.
"""
import os
import re
import shutil
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NDK = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT") or "/opt/android-ndk-r28c"


def rows(path):
    """(percent, samples, tid, rest...) of each data line of a simpleperf report."""
    out = []
    for line in Path(path).read_text(errors="replace").splitlines():
        m = re.match(r"\s*([\d.]+)%\s+(\d+)\s+(\d+)\s+(.*)$", line)
        if m:
            out.append((float(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4).strip()))
    return out


def find_libs(dirs):
    """Library name -> newest armeabi-v7a copy with symbols."""
    found = {}
    for d in dirs:
        if not os.path.isdir(d):
            continue
        for dirpath, _, files in os.walk(d):
            if re.search(r"arm64|x86", dirpath):
                continue
            for f in files:
                if f.endswith(".so"):
                    p = os.path.join(dirpath, f)
                    if f not in found or os.path.getmtime(p) > os.path.getmtime(found[f]):
                        found[f] = p
    return found


def symbolize(lib, addresses):
    """Address -> function name (the function the code is in, not inlined callees)."""
    tool = next(Path(NDK).glob("toolchains/llvm/prebuilt/*/bin/llvm-symbolizer"), None) or shutil.which("llvm-symbolizer")
    if not tool:
        return {}
    text = "\n".join(hex(a) for a in addresses) + "\n"
    proc = subprocess.run([str(tool), "--obj=" + lib, "-C", "--inlining=false", "--functions=linkage"],
                          input=text, capture_output=True, text=True)
    names, block = [], []
    for line in proc.stdout.splitlines():
        if line.strip() == "":
            if block:
                names.append(block[0])
                block = []
        else:
            block.append(line.strip())
    if block:
        names.append(block[0])
    return dict(zip(addresses, names))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    raw, threads = rows(sys.argv[1]), rows(sys.argv[2])
    dirs = sys.argv[3:] or [os.path.expanduser("~/re4-kern/RE4/port/build/whole-android-armv7"),
                            str(ROOT / "port/build/whole-android-armv7"),
                            str(ROOT / "port/android-decomp/app/build/intermediates/cxx")]
    total = sum(r[1] for r in raw) or 1
    names = {tid: rest for _, _, tid, rest in threads}
    m = re.search(r"--duration\s+(\d+)", Path(sys.argv[1]).read_text(errors="replace"))
    seconds = int(m.group(1)) if m else 0

    by_thread = defaultdict(int)
    by_lib = defaultdict(int)
    addresses = defaultdict(lambda: defaultdict(int))  # lib name -> address -> samples, per thread below
    samples = []
    for _, n, tid, rest in raw:
        parts = rest.split()
        if len(parts) < 2:
            continue
        dso, vaddr = parts[0], parts[-1]
        try:
            address = int(vaddr, 16)
        except ValueError:
            continue
        lib = dso.split("/")[-1].split("!")[-1]
        by_thread[tid] += n
        by_lib[(tid, lib)] += n
        addresses[lib][address] += n
        samples.append((tid, lib, address, n))

    libs = find_libs(dirs)
    own = [lib for lib in addresses if lib.startswith("libre4") or lib == "libc++_shared.so"]
    func = {}
    missing = []
    for lib in own:
        if lib in libs:
            func[lib] = symbolize(libs[lib], sorted(addresses[lib]))
        else:
            missing.append(lib)

    print(f"Messung: {total} Proben, {seconds or '?'} s (1000 Proben je Sekunde, solange ein Thread rechnet)")
    print("Prozentwerte: Anteil an allen Proben")
    print()
    print("Threads (Auslastung: Anteil eines Prozessorkerns):")
    for tid, n in sorted(by_thread.items(), key=lambda t: -t[1])[:8]:
        busy = f"{100.0 * n / (seconds * 1000):5.0f}% eines Kerns" if seconds else ""
        print(f"  {100.0 * n / total:5.1f}%  {tid:6d}  {names.get(tid, '?'):24s} {busy}")
    print()
    print("Bibliotheken je Thread:")
    for tid, _ in sorted(by_thread.items(), key=lambda t: -t[1])[:4]:
        parts = sorted(((n, lib) for (t, lib), n in by_lib.items() if t == tid), reverse=True)[:8]
        print(f"  {tid} {names.get(tid, '?')}:")
        for n, lib in parts:
            print(f"     {100.0 * n / total:5.1f}%  {lib}")
    print()
    per_func = defaultdict(int)
    for tid, lib, address, n in samples:
        name = func.get(lib, {}).get(address)
        if name is None or name == "??":
            name = "(ohne Symbol)" if lib in own else "(gesamt)"
        per_func[(tid, lib, name)] += n
    print("Funktionen (die 70 groessten):")
    for (tid, lib, name), n in sorted(per_func.items(), key=lambda t: -t[1])[:70]:
        print(f"  {100.0 * n / total:5.1f}%  {tid:6d}  {lib:22s} {name[:110]}")
    if missing:
        print()
        print("Ohne Bibliothek mit Symbolen auf diesem PC: " + ", ".join(missing))
    return 0


if __name__ == "__main__":
    sys.exit(main())
