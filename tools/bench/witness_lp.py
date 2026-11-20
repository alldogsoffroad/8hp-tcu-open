#!/usr/bin/env python3
"""witness_lp.py - the READ-ONLY healthy line-pressure (Sys) and torque-intervention shadows
(firmware/app/lp_shadow.c, firmware/app/ti_shadow.c; ~/tcu_sweep/lp_ti_shadow_port.md).

Resolves its addresses FROM THE ELF by symbol (sh-elf-nm): __lp_shadow_start (struct lp_t, 0xB4 bytes,
section .lp_shadow) and __ti_shadow_start (struct ti_t, 0xC0 bytes, section .ti_shadow). Reads them over
meas (NOT atomic - fields tear between blocks) and prints the would-be line-pressure target / reason /
Sys current against the LIVE Sys current (0xFFF94ED4 + 12, the law's channel 6), and the torque-intervention
object, request and the 13-bit fields of 0x2F9 / 0x037 (NEVER transmitted).

    witness_lp.py [rounds] [--elf PATH]

Expected on the dry bench with the brake + engine 700 sims (P, no power flow, pos 3, healthy = reaction 50
held 0): reason 0x19, res 6000, the would-be Sys ~ 7265 (live 500: diff counts every pass); during a garage
commit reason 0x01 (20000 -> Sys 500) for ~2 passes, then 0x10 (9000 -> ~5971) until completion; in D at
idle 0x18 -> Sys 8500; leaving P (position 3, 2 s): 0x13 15000 -> ~3625. The TI shadow arms (class 2) at
each garage engagement and sequences 0xA -> 0xB -> 0xC; the fields stay 1FFE / 0A28 (no amount decoded).
`meas 0x02` hygiene first (CLAUDE rule 6).
"""
import os
import struct
import subprocess
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench  # noqa: E402
import witness_sm  # noqa: E402

LP_SIZE = 0xB4
TI_SIZE = 0xC0
LPW = ["pass", "skip_mode", "cmp", "diff", "err", "si_cond", "si_r0a", "si_class2", "si_var", "sys_bin", "fault",
       "fill", "pt1"]
TIW = ["pass", "skip_mode", "arm[0]", "arm_up", "arm_down", "arm_garage", "end", "phase", "si_dwell", "si_amount",
       "si_misc", "req", "r23", "err", "red", "inc", "ramp", "rsvd0", "rsvd1"]


def elf_syms(elf):
    out = subprocess.run(["sh-elf-nm", elf], capture_output=True, text=True, check=True).stdout
    s = {}
    for ln in out.splitlines():
        p = ln.split()
        if len(p) == 3:
            s[p[2]] = int(p[0], 16)
    return s


def u16(d, o):
    return struct.unpack(">H", d[o:o + 2])[0]


def s16(d, o):
    return struct.unpack(">h", d[o:o + 2])[0]


def show_lp(d):
    print("  LP    res %u (p10 %u) winner reason %02X  normal %u (reason %02X)  steady reason %02X  start-max %u "
          "done %u counter %u  range-mem %u w22 %u  forced %u  last %u  e34a %d  cf5 %u" %
          (u16(d, 0), u16(d, 2), d[22], u16(d, 12), d[24], d[26], d[18], d[29], d[31], d[32], u16(d, 38),
           struct.unpack(">I", d[40:44])[0], u16(d, 66), s16(d, 68), d[70]))
    print("  SYS   would-be p %d -> %u (0.1 mA)   LIVE %u   [the healthy case: reaction 50 HELD 0]" %
          (s16(d, 72), u16(d, 74), u16(d, 84 + 40)))
    w = struct.unpack(">13I", d[128:128 + 52])
    print("  LPW   " + " ".join("%s %u" % (n, v) for n, v in zip(LPW, w)))


def show_ti(d):
    print("  TI    armed %u class %u step %u phase %02X tmr %u out %d mode %u type %u delta %d  req %d (x4 Nm) dreq %d "
          "limit %d Nm  FIELDS req %04X lim %04X (never sent)" %
          (d[0], d[1], d[2], d[3], u16(d, 4), s16(d, 6), d[9], d[10], s16(d, 12), s16(d, 14), s16(d, 16),
           s16(d, 18), u16(d, 20), u16(d, 22)))
    w = struct.unpack(">19I", d[0x3C:0x3C + 0x4C])
    print("  TIW   " + " ".join("%s %u" % (n, v) for n, v in zip(TIW, w)))
    # the amounts of the 2026-11-20 port (ti_amounts_port.md): the stock object's own fields
    print("  TIX   inc %u  a22 %d v6 %d v8 %d v2a %d flare %d last20 %d  blend start %d frac %d  pc %u t1a %u rc %u rlen %u  hd %u  "
          "bd78 %u c18 %u c1c %u c12 %u  idx gear %u up %u dn %u d4 %u c %u  E3DE-en %u" %
          (d[0x0B], s16(d, 0x88), s16(d, 0x8A), s16(d, 0x8C), s16(d, 0x8E), s16(d, 0xBA), s16(d, 0x92), s16(d, 0x98),
           struct.unpack(">i", d[0x9C:0xA0])[0], u16(d, 0xA0), u16(d, 0xA2), d[0xA4], d[0xA5], d[0xA6],
           d[0xAF], d[0xB4], d[0xB5], d[0xB6], d[0xAA], d[0xAB], d[0xAC], d[0xAD], d[0xAE], d[0xBC]))


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 3
    elf = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../build/sh2a/tcu_app.elf")
    if "--elf" in sys.argv:
        elf = sys.argv[sys.argv.index("--elf") + 1]
    syms = elf_syms(elf)
    lp_a = syms["__lp_shadow_start"]
    ti_a = syms["__ti_shadow_start"]
    print("ELF %s: lp_t @ %08X, ti_t @ %08X" % (elf, lp_a, ti_a))
    b = tcu_bench.Buses(witness_sm.A())
    h = None
    for _ in range(10):
        h = tcu_bench.meas(b, 1, [0x02], timeout=1.5)
        if h:
            break
        time.sleep(0.4)
    print("hyg 0x02:", h.hex() if h else None, flush=True)
    for r in range(rounds):
        print("--- round %d %s" % (r, time.strftime("%H:%M:%S")), flush=True)
        d = witness_sm.read(b, 1, lp_a, LP_SIZE)
        if d is None:
            print("  lp read FAIL")
        else:
            show_lp(d)
        t = witness_sm.read(b, 1, ti_a, TI_SIZE)
        if t is None:
            print("  ti read FAIL")
        else:
            show_ti(t)
        time.sleep(1.0)


if __name__ == "__main__":
    main()
