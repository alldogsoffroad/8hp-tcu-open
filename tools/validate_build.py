#!/usr/bin/env python3
"""validate_build.py — the per-flash validation in one command (2026-11-20).

  validate_build.py snapshot <name>
      Save the current SH outputs (build/sh2a) as the flashed baseline
      build/flashed/<name>/ (app ELF/hex, RAM app, loader, flasher).

  validate_build.py check <new> <base> [--no-build] [--allow-differ FILE ...]
      1. build host + SH and run ctest (unless --no-build)
      2. RAM symbols (sh-elf-nm b/B/d/D): report added / removed / MOVED vs
         build/flashed/<base>/  -> FAIL on any removed or moved symbol
      3. the RAM app, loader and flasher byte-identical to the baseline
         (FAIL unless listed with --allow-differ)
      4. every new RAM section window (__x_start .. __x_end pairs that are
         new) is zero in the stock driving dump
      5. build build/<new>_IFla.bin on build/<base>_IFla.bin, report the
         app end and the changed 64 KB blocks; FAIL at app end >= 0x80000
      6. write /tmp/install_<new>.py and /tmp/<new>_sigs.json from the
         base's (every changed block forced: null)
      Prints a short PASS/FAIL summary; exit code 0 only on PASS.
"""
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SH = os.path.join(ROOT, "build", "sh2a")
HOST = os.path.join(ROOT, "build", "host")
FLASHED = os.path.join(ROOT, "build", "flashed")
DUMP = os.path.join(ROOT, "stockram_driving_20261104.bin")
DUMP_BASE = 0xFFF80000
CAL_START = 0x80000
OUTPUTS = ["tcu_app.elf", "tcu_app.hex", "tcu_app_ram.bin", "tcu_loader.hex", "tcu_flasher.bin"]
IDENTICAL = ["tcu_app_ram.bin", "tcu_loader.hex", "tcu_flasher.bin"]
NM = shutil.which("sh-elf-nm") or "sh-elf-nm"


def run(cmd, cwd=ROOT):
    return subprocess.run(cmd, cwd=cwd, shell=isinstance(cmd, str), capture_output=True, text=True)


def ram_syms(elf):
    out = run([NM, elf]).stdout
    syms = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3 and p[1] in "bBdD":
            syms[p[2]] = int(p[0], 16)
    return syms


def snapshot(name):
    d = os.path.join(FLASHED, name)
    os.makedirs(d, exist_ok=True)
    for f in OUTPUTS:
        shutil.copy2(os.path.join(SH, f), d)
    print(f"snapshot: build/sh2a outputs saved as build/flashed/{name}/")


def check(new, base, build=True, allow=()):
    fails, notes = [], []
    bdir = os.path.join(FLASHED, base)
    if not os.path.isdir(bdir):
        sys.exit(f"no baseline build/flashed/{base}/ (run: validate_build.py snapshot {base} on that build)")

    if build:
        for d in (HOST, SH):
            r = run(["cmake", "--build", d, "-j8"])
            if r.returncode != 0:
                fails.append(f"build failed in {os.path.relpath(d, ROOT)}: "
                             + (re.findall(r"(FAILED.*|.*error.*)", r.stdout + r.stderr) or ["?"])[0][:160])
        if not fails:
            r = run(["ctest", "-j8"], cwd=HOST)
            m = re.search(r"(\d+)% tests passed, (\d+) tests failed out of (\d+)", r.stdout)
            if not m or m.group(2) != "0":
                fails.append("ctest: " + (m.group(0) if m else "no summary"))
                fails += ["  " + l.strip() for l in r.stdout.splitlines() if "(Failed)" in l][:10]
            else:
                notes.append(f"host tests {m.group(3)}/{m.group(3)}")
        if fails:
            return report(new, fails, notes)

    old, cur = ram_syms(os.path.join(bdir, "tcu_app.elf")), ram_syms(os.path.join(SH, "tcu_app.elf"))
    added = sorted(set(cur) - set(old), key=lambda s: cur[s])
    removed = sorted(set(old) - set(cur))
    moved = sorted(s for s in set(old) & set(cur) if old[s] != cur[s])
    for s in removed:
        fails.append(f"RAM symbol removed: {s} (was {old[s]:08X})")
    for s in moved:
        fails.append(f"RAM symbol MOVED: {s} {old[s]:08X} -> {cur[s]:08X}")
    notes.append("RAM: " + (", ".join(f"{s} {cur[s]:08X}" for s in added) if added else "no new symbols"))

    for f in IDENTICAL:
        same = open(os.path.join(bdir, f), "rb").read() == open(os.path.join(SH, f), "rb").read()
        if not same:
            (notes if f in allow else fails).append(f"{f} differs from {base}" + (" (allowed)" if f in allow else ""))

    if os.path.exists(DUMP):
        dump = open(DUMP, "rb").read()
        for s in added:
            if s.startswith("__") and s.endswith("_start"):
                e = s[:-6] + "_end"
                if e in cur:
                    a, b = cur[s] - DUMP_BASE, cur[e] - DUMP_BASE
                    if 0 <= a <= b <= len(dump):
                        nz = sum(1 for x in dump[a:b] if x)
                        if nz:
                            fails.append(f"new window {s[2:-6]} {cur[s]:08X}..{cur[e]:08X} has {nz} nonzero dump bytes")
                    else:
                        notes.append(f"new window {s[2:-6]} outside the dump range (not checked)")
    else:
        notes.append("stock dump missing: new windows not checked")

    img_base = os.path.join(ROOT, "build", f"{base}_IFla.bin")
    img_new = os.path.join(ROOT, "build", f"{new}_IFla.bin")
    r = run([sys.executable, "tools/build_image.py", "--app", os.path.join(SH, "tcu_app.hex"), "--base", img_base,
             "-o", img_new, "--stock-ident", "--gap-keep-above", "0x50000"])
    end = re.search(r"ends at (0x[0-9a-fA-F]+)", r.stdout + r.stderr)
    crc = re.search(r"CRC 0x\S+ = ([0-9A-F]+)", r.stdout + r.stderr)
    if r.returncode != 0 or not crc:
        fails.append("build_image failed: " + (r.stderr or r.stdout)[-200:])
        return report(new, fails, notes)
    app_end = int(end.group(1), 16) if end else None
    a, b = open(img_base, "rb").read(), open(img_new, "rb").read()
    blocks = [o for o in range(0, len(a), 0x10000) if a[o:o + 0x10000] != b[o:o + 0x10000]]
    if any(o >= CAL_START for o in blocks) or (app_end and app_end >= CAL_START):
        fails.append(f"image reaches the calibration region (app end {app_end:#x}, blocks {[hex(o) for o in blocks]})")
    notes.append(f"image build/{new}_IFla.bin CRC {crc.group(1)}, app end {app_end:#x} "
                 f"({(CAL_START - app_end) // 1024} KB spare), blocks {', '.join(hex(o) for o in blocks)}")

    src, sig = f"/tmp/install_{base}.py", f"/tmp/{base}_sigs.json"
    if os.path.exists(src) and os.path.exists(sig):
        txt = open(src).read().replace(base, new)
        open(f"/tmp/install_{new}.py", "w").write(txt)
        sigs = json.load(open(sig))
        for k in sigs:
            if int(k.split("/")[0], 16) in blocks:
                sigs[k] = None
        json.dump(sigs, open(f"/tmp/{new}_sigs.json", "w"))
        forced = [k for k, v in sigs.items() if v is None]
        notes.append(f"install /tmp/install_{new}.py, sigs forced: {', '.join(forced)}")
        missing = [hex(o) for o in blocks if not any(int(k.split('/')[0], 16) == o for k in sigs)]
        if missing:
            fails.append(f"changed blocks missing from the sig table: {missing}")
    else:
        fails.append(f"no {src} / {sig} to derive the install script from")
    return report(new, fails, notes)


def report(new, fails, notes):
    for n in notes:
        print("  " + n)
    for f in fails:
        print("  FAIL " + f)
    print(f"{new}: {'PASS' if not fails else 'FAIL'}")
    return 0 if not fails else 1


def main():
    a = sys.argv[1:]
    if len(a) >= 2 and a[0] == "snapshot":
        snapshot(a[1])
        return 0
    if len(a) >= 3 and a[0] == "check":
        allow = a[a.index("--allow-differ") + 1:] if "--allow-differ" in a else []
        return check(a[1], a[2], build="--no-build" not in a, allow=allow)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
