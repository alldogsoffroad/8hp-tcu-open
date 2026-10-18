#!/usr/bin/env python3
"""Host test of the project reflash (spec 16 section 7): tcu_bench.Uds + tcu_flash on top of the
real flasher code (fl_core + fl_fcu) running on the simulated flash controller (tcu_flasher_sim).

Checks:
  * a write produces exactly the image in 0x040000-0x1FFFFF and leaves the boot area alone;
  * a second run finds nothing to do;
  * power cut at many points of the write: whenever a region's markers say "valid" (marker 1
    programmed, marker 2 erased) its content equals the image; the boot area is never changed;
    a new write afterwards (the ACDP-2 stands in for this on the bench only if the TCU was reset)
    completes the image.

Usage: test_tcu_flash.py PATH/TO/tcu_flasher_sim
"""
import os
import random
import select
import subprocess
import sys
import tempfile
import types

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tcu_bench      # noqa: E402  (python-can not needed for this test)
import tcu_flash      # noqa: E402

SIM = sys.argv[1] if len(sys.argv) > 1 else "tcu_flasher_sim"
N = tcu_flash.FLASH_LEN


class SimGone(Exception):
    pass


class SimBus:
    """python-can-like bus on top of tcu_flasher_sim's stdin/stdout."""

    def __init__(self, rom_in, rom_out, cut=None):
        cmd = [SIM, rom_in, rom_out] + (["--cut", str(cut)] if cut is not None else [])
        self.p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, bufsize=0)
        self.buf = b""

    def recv(self, timeout):
        # own line buffer on the raw pipe: a buffered reader would hide lines from select()
        while b"\n" not in self.buf:
            r, _, _ = select.select([self.p.stdout], [], [], timeout)
            if not r:
                return None
            chunk = os.read(self.p.stdout.fileno(), 65536)
            if not chunk:
                raise SimGone()
            self.buf += chunk
            timeout = 0
        line, self.buf = self.buf.split(b"\n", 1)
        return types.SimpleNamespace(arbitration_id=0x7E9, data=bytes.fromhex(line.decode()))

    def send(self, data):
        try:
            self.p.stdin.write((data.hex(" ") + "\n").encode())
        except BrokenPipeError:
            raise SimGone()

    def close(self):
        try:
            self.p.stdin.close()
        except BrokenPipeError:
            pass
        err = self.p.stderr.read().decode()
        code = self.p.wait(10)
        return code, err


class SimBuses:
    def __init__(self, bus):
        self.bus = bus

    def get(self, n):
        return self.bus

    def send(self, n, arb_id, data):
        assert arb_id == 0x7E1
        self.bus.send(bytes(data))


def make_roms(seed=1):
    rnd = random.Random(seed)
    rb = lambda n: bytes(rnd.getrandbits(8) for _ in range(n))     # noqa: E731
    base = bytearray(b"\xFF" * N)
    base[0:0x40000] = rb(0x40000)                       # boot area (never written)
    base[0x40000:0x50000] = b"\x87\x65\x43\xC1" + rb(0x10000 - 4)   # old application
    base[0x180200:0x184000] = b"\x87\x65\x43\x83" + rb(0x3E00 - 4)  # calibration header + data
    base[0x1A0000:0x1A4000] = rb(0x4000)
    for m1, m2 in ((0x17FE00, 0x17FF00), (0x180000, 0x180100)):
        base[m1:m1 + 256] = b"\x00" * 256
        base[m2:m2 + 256] = b"\xFF" * 256
    img = bytearray(base)
    img[0x40000:0x50000] = b"\xFF" * 0x10000
    img[0x40000:0x45000] = b"\x87\x65\x43\xC1" + rb(0x5000 - 4)     # new, smaller application
    img[0x45100:0x45200] = b"\xFF" * 256                           # a blank page inside the run
    img[0x1A1000:0x1A2000] = rb(0x1000)                            # calibration change
    return bytes(base), bytes(img)


def valid(rom, r):
    return (rom[r["marker1"]:r["marker1"] + 256] == b"\x00" * 256 and
            rom[r["marker2"]:r["marker2"] + 256] == b"\xFF" * 256)


def run(rom_in, rom_out, img, cut=None):
    bus = SimBus(rom_in, rom_out, cut)
    fl = tcu_flash.Flasher(tcu_bench.Uds(SimBuses(bus), 1))
    outcome = "done"
    try:
        info = fl.ident()
        assert info and info["fwe"], info
        pages = tcu_flash.write(fl, img)
        fl.reset()
    except (SimGone, tcu_flash.FlashError) as e:
        outcome, pages = f"stopped ({type(e).__name__})", None
    code, err = bus.close()
    stats = err.strip().splitlines()[-1] if err.strip() else ""
    if code == 3:
        raise AssertionError(f"flash controller rule broken: {err}")
    return outcome, pages, stats


def ops(stats):
    # "tcu_flasher_sim: erases E, programs P, ..."
    parts = stats.split()
    return int(parts[2].rstrip(",")) + int(parts[4].rstrip(","))


def main():
    fails = 0
    base, img = make_roms()
    assert not tcu_flash.check_image(img + b"\xFF" * 0x80000), tcu_flash.check_image(img)
    with tempfile.TemporaryDirectory() as d:
        p_base, p_out, p_out2 = (os.path.join(d, n) for n in ("base.bin", "out.bin", "out2.bin"))
        open(p_base, "wb").write(base)

        outcome, pages, stats = run(p_base, p_out, img)
        out = open(p_out, "rb").read()
        print(f"full write: {outcome}, {pages} pages; {stats}")
        if outcome != "done" or out != img:
            print("FAIL: written flash differs from the image")
            fails += 1
        total = ops(stats)

        # second run: plan finds nothing
        bus = SimBus(p_out, p_out2)
        todo = tcu_flash.plan(tcu_flash.Flasher(tcu_bench.Uds(SimBuses(bus), 1)), img)
        bus.close()
        if todo:
            print(f"FAIL: second run wants to write {todo}")
            fails += 1

        # power cuts
        # --cut k: power fails during the k-th erase/program command (1-based)
        cuts = sorted(set(list(range(1, 9)) + list(range(9, total, max(1, total // 25))) +
                          list(range(max(1, total - 5), total + 1))))
        for k in cuts:
            outcome, _, stats = run(p_base, p_out, img, cut=k)
            rom = open(p_out, "rb").read()
            states = []
            for r in tcu_flash.REGIONS:
                v = valid(rom, r)
                same = rom[r["lo"]:r["hi"]] == img[r["lo"]:r["hi"]]
                old = rom[r["lo"]:r["hi"]] == base[r["lo"]:r["hi"]]
                states.append(f"{r['name'][:3]} {'valid' if v else 'INVALID'}"
                              f"{' new' if same else ' old' if old else ''}")
                if v and not (same or old):
                    print(f"FAIL: cut after {k} ops: {r['name']} marked valid with mixed content")
                    fails += 1
            if rom[:0x40000] != base[:0x40000]:
                print(f"FAIL: cut after {k} ops changed the boot area")
                fails += 1
            # the application must never be valid while the calibration is half written
            app_ok = valid(rom, tcu_flash.REGIONS[0])
            cal_ok = valid(rom, tcu_flash.REGIONS[1])
            if app_ok and not cal_ok:
                print(f"FAIL: cut after {k} ops: application valid but calibration invalid")
                fails += 1
            # retry with the flasher (as if it were still running)
            if k % 5 == 0:
                outcome2, _, _ = run(p_out, p_out2, img)
                if outcome2 != "done" or open(p_out2, "rb").read() != img:
                    print(f"FAIL: rewrite after a cut at {k} did not complete the image")
                    fails += 1
            if k == total and not (cal_ok and rom[0x180000:0x200000] == img[0x180000:0x200000]):
                print("FAIL: cut during the last command (application marker 1): calibration not committed")
                fails += 1
            print(f"cut during op {k:3d}/{total}: {', '.join(states)}")
    print("passed" if fails == 0 else f"FAILED ({fails})")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
