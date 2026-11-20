#!/usr/bin/env python3
"""witness_wk.py - the READ-ONLY lock-up shadows: the WK state machine (firmware/app/wk_shadow.c, struct wk_t 0xB4 bytes at
0xFFF935D0, section .wk_shadow - nm `_K`) and the lock-up REQUEST PRODUCER of the shift-map shadow (app/sm_shadow.c lu_pass, struct
sml_t 0x80 bytes at 0xFFF9369C, section .sm_lock - nm `_s_lu`). 2026-11-20, ~/tcu_sweep/gaps3_port.md.

    witness_wk.py [rounds]

Reads over meas (6 bytes per request, NOT atomic: fields tear between blocks). Expected on the dry bench (OS mode 1 or 2, standstill,
no simulated speeds): request 0, level 0, inhibit 0x00 (the producer holds reaction 49 = 0, the HEALTHY case; the bench's fault
manager has it active, so the WK gate stays shut) or 0x02 (gear), the latch state 2 once the fluid byte reaches 57, WK state 1,
perm 0. With the bench's simulated speeds (drive-cycle) the request follows the gear / output speed (6th: 1 at ~1050, 2 at ~2300).
Addresses: re-check with sh-elf-nm before every use. `meas 0x02` hygiene first (CLAUDE rule 6)."""
import struct
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench  # noqa: E402
import witness_sm  # noqa: E402

WK = 0xFFF935D0
WK_SIZE = 0xB4
LU = 0xFFF9369C
LU_SIZE = 0x80
SLWIT = ["pass", "chg", "err", "cap", "speedcap", "unp"]


def u16(d, o):
    return struct.unpack(">H", d[o:o + 2])[0]


def u32(d, o):
    return struct.unpack(">I", d[o:o + 4])[0]


def f32(d, o):
    return struct.unpack(">f", d[o:o + 4])[0]


def show_lu(d):
    print("  LOCK-UP REQUEST: req %u (AB94) A0A1 %u | level %u new %u | inhibit %02X (b3 %u b4 %u b5 %u) gate_ok %u" %
          (d[0x21], d[0x35], d[0x22], d[0x23], d[0x32], d[0x2C], d[0x30], d[0x31], d[0x36]))
    print("    allow p %s fam %s | w1 %u w2 %u | thr dn %u up %u | row y %u col %u | latch L+1 %u L+2 %u L+3 %u" %
          (list(d[0x11:0x15]), list(d[0x17:0x1B]), d[0x15], d[0x16], u16(d, 0x28), u16(d, 0x2A), d[0x24], d[0x25], d[1], d[2], d[3]))
    print("    inputs: react49 %u ovr e4/a4/c2 %u/%u/%u a1f8+34 %u a3bc+55 %u ab9a %u temp(A68E) %u a666 %u a65a %u" %
          (d[0x38], d[0x39], d[0x3A], d[0x3B], d[0x3C], d[0x3D], d[0x3E], d[0x3F], u16(d, 0x40), d[0x42]))
    print("    witnesses: pass %u inh[1..7] %s chg %u err %u cap %u speedcap %u unp %u lvl[0..3] %s si %u" %
          (u32(d, 0x44), [u16(d, 0x48 + 2 * i) for i in range(8)], u32(d, 0x58), u32(d, 0x5C), u32(d, 0x60), u32(d, 0x64),
           u32(d, 0x68), [u32(d, 0x6C + 4 * i) for i in range(4)], u32(d, 0x7C)))


def show_wk(d):
    print("  WK: state %u prev %u closed %u b4ac %u inhibit %u fl_ok %u req %u trac %u gate %u | b5e1 %u b5e2 %u mode_out %u" %
          (d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[9], d[0x0E], d[0x0B], d[0x0F]))
    print("    t_pre %u t_fill %u t_s10 %u | B2AA %u B2AC %d p %.3f bar (p_mbar %u) ceil %.2f wk_i %u (live %u) req_mbar %u" %
          (u16(d, 0x10), u16(d, 0x12), u16(d, 0x14), u16(d, 0x16), struct.unpack(">h", d[0x18:0x1A])[0], f32(d, 0x1C),
           u16(d, 0x1A), f32(d, 0x28), u16(d, 0x32), u16(d, 0x34 + 28), u16(d, 0x30)))
    print("    RAMPS (gaps3): ol24 %.2f s ol28 %.2f s target %.2f slope %.3f bar/s | fill_len %u (maps %u) hold %u" %
          (f32(d, 0x9C), f32(d, 0xA0), f32(d, 0xA4), f32(d, 0xA8), u16(d, 0xAC), u16(d, 0xB0), u16(d, 0xAE)))
    w = d[0x54:0x54 + 0x48]
    names = ["pass", "reset", "gate", "cmp", "diff", "perm", "eng_cnt(NOT CALLED)", "abort", "err", "fault", "si_req",
             "si_ramp", "si_filt", "si_flag", "trans", "us_last", "us_max"]
    print("    W " + " ".join("%s %u" % (n, u32(w, 4 * i)) for i, n in enumerate(names)) + " seen %04X" % u16(w, 0x44))


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 3
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
        d = witness_sm.read(b, 1, LU, LU_SIZE)
        print("  lock-up request read FAIL" if d is None else "", end="")
        if d is not None:
            show_lu(d)
        k = witness_sm.read(b, 1, WK, WK_SIZE)
        if k is None:
            print("  WK read FAIL")
        else:
            show_wk(k)
        time.sleep(1.0)


if __name__ == "__main__":
    main()
