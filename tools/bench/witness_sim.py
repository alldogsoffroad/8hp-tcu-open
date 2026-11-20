#!/usr/bin/env python3
"""witness_sim.py - the BENCH-ONLY simulated-speed input (firmware/io/vehicle/shadow_sim.c,
TCU_SHADOW_SIM; ~/tcu_sweep/shadow_sim_port.md).

Reads the 0x20-byte witness at __shadow_sim_start (section .shadow_sim, 0xFFF87F10; the address is
resolved from the ELF by nm) over meas and prints: frames received (good / refused / valid->invalid
timeouts), the last values (output rpm, turbine rpm, torque Nm, pedal raw), the valid flag, the age
in 10 ms ticks and the magic (53534944 'SSIM' once shadow_sim_init has run).

    witness_sim.py [rounds] [--elf PATH]

On a plain boot with no frames: frames 0, valid 0, magic 53534944. During `tcu_bench.py drive-cycle`:
frames climbing at ~100/s, valid 1, age 0..1. After the command ends: valid 0 within 100 ms,
timeouts +1. `meas 0x02` hygiene first (CLAUDE rule 6).
"""
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tcu_bench  # noqa: E402
import witness_sm  # noqa: E402


def sym(elf, name):
    out = subprocess.run(["sh-elf-nm", elf], capture_output=True, text=True, check=True).stdout
    for ln in out.splitlines():
        p = ln.split()
        if len(p) == 3 and p[2] == name:
            return int(p[0], 16)
    sys.exit("symbol %s not found in %s (built without TCU_SHADOW_SIM?)" % (name, elf))


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 3
    elf = os.path.join(HERE, "../../build/sh2a/tcu_app.elf")
    if "--elf" in sys.argv:
        elf = sys.argv[sys.argv.index("--elf") + 1]
    addr = sym(elf, "__shadow_sim_start")
    print("ELF %s: the sim witness @ %08X" % (elf, addr))
    b = tcu_bench.Buses(witness_sm.A())
    h = None
    for _ in range(10):
        h = tcu_bench.meas(b, 1, [0x02], timeout=1.5)
        if h:
            break
        time.sleep(0.4)
    print("hyg 0x02:", h.hex() if h else None, flush=True)
    for r in range(rounds):
        d = witness_sm.read(b, 1, addr, 0x20)
        if d is None:
            print("--- round %d read FAIL" % r)
            continue
        u = lambda o, n: int.from_bytes(d[o:o + n], "big")
        print("--- round %d %s: frames %u bad %u timeouts %u | last out %u rpm turb %u rpm tq %d Nm pedal %u | "
              "valid %u age %u | magic %s%s" %
              (r, time.strftime("%H:%M:%S"), u(0, 4), u(4, 4), u(8, 4), u(12, 2), u(14, 2),
               int.from_bytes(d[16:18], "big", signed=True), d[18], d[19], d[20], d[24:28].hex(),
               "" if d[24:28].hex() == "53534944" else "  (NOT INITIALISED)"), flush=True)
        time.sleep(1.0)


if __name__ == "__main__":
    main()
