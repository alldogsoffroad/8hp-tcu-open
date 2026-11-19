#!/usr/bin/env python3
"""witness_sm.py — the READ-ONLY shift-map shadow (firmware/app/sm_shadow.c;
~/tcu_sweep/sm_shadow_port.md, decode ~/tcu_sweep/shift_map_decode.md).

Reads struct sm_t (0x120 bytes at 0xFFF86800, section .sm_shadow — nm `_s_sm`)
over meas (6 bytes per request; NOT atomic — the fields tear between blocks)
and prints the strategy cells (S = 0xFFF8A4E4 offsets), the target A697 /
A0AA / 0x89CE8 (shadow), the AB30 filter, the minimum gear, the program
block, the witnesses and the last target changes. For comparison it reads the
LIVE 0x89CE8 — no section of ours covers it and nothing of ours writes it (the
engine's gear code is the compile-time 0x89C2B, also printed); it must not
change between rounds (the shadow writes nothing live).

    witness_sm.py [rounds] [--raw]

Expected on the bench (dry, standstill, any lever position): A697 = A0AA =
CE8 = 1, candidate 1, gear_in 3, DA 160 (2->1 at pedal 23), D2 160, D6 750,
pedal 23 (no 0x120), AB30 0, min gear 1, errors 0, the arbitration skipping
4 ids per pass, the confirm stand-in counted every pass, ~33 passes / s in
modes 1..3. Addresses: re-check with sh-elf-nm before every use. `meas 0x02`
hygiene first (CLAUDE rule 6).
"""
import struct
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench  # noqa: E402

SM = 0xFFF86800
SIZE = 0x120
LIVE = [("0x89CE8", 0xFFF89CE8, 1), ("0x89C2B", 0xFFF89C2B, 1), ("OS mode 0x93B33", 0xFFF93B33, 1)]
WIT = ["init", "pass", "skip_mode", "walk", "lookup", "iter_max", "err", "err_last", "arb_skip",
       "arb_none", "corr(unported)", "conf(standin)", "clamp", "win", "mingear(0x4F)", "range(0x96)",
       "gear_inv", "chg", "reserved"]


class A:
    iface = "slcan"
    bus1 = "/dev/ttyACM0"
    bus2 = "/dev/ttyACM1"
    teensy_mode = None
    keep_rate = True


def read(b, bus, addr, length, tries=5):
    out = bytearray()
    a = addr
    while len(out) < length:
        got = None
        for _ in range(tries):
            try:
                r = tcu_bench.meas(b, bus, [0x2A, 0, a >> 24 & 0xFF, a >> 16 & 0xFF, a >> 8 & 0xFF, a & 0xFF],
                                   timeout=0.9)
            except Exception:
                r = None
            if r and len(r) >= 2 and r[0] == 0x2A and r[1] == 0x81:
                got = bytes(r[2:8])
                break
            time.sleep(0.1)
        if got is None:
            return None
        out += got
        a += 6
    return bytes(out[:length])


def u16(d, o):
    return struct.unpack(">H", d[o:o + 2])[0]


def s16(d, o):
    return struct.unpack(">h", d[o:o + 2])[0]


def u32(d, o):
    return struct.unpack(">I", d[o:o + 4])[0]


def show(d):
    print("  S     prog %u ae %u af %u b0 %u b2 %d b4 %d b6 %u | BA..BF %s | gear_in C2 %u C3 %u DE %u DF %u "
          "CAND E2 %u | last lookup y %u line %u" %
          (d[0], d[1], d[2], d[3], s16(d, 4), s16(d, 6), d[8], d[9:15].hex(" "), d[0x0F], d[0x10], d[0x11],
           d[0x12], d[0x13], d[0x14], d[0x15]))
    print("  THR   UP D8 %u (D0 %u, D4 %u, full load CE %u) | DOWN DA %u (D2 %u, D6 %u) | speed DC %u | "
          "limits E6 %u E8 %u EA %u EC %u" %
          (u16(d, 0x20), u16(d, 0x18), u16(d, 0x1C), u16(d, 0x16), u16(d, 0x22), u16(d, 0x1A), u16(d, 0x1E),
           u16(d, 0x24), u16(d, 0x26), u16(d, 0x28), u16(d, 0x2A), u16(d, 0x2C)))
    print("  A67C  min gear %u A694 %u invalid %u step %u hold2 %u winclamp %u | TARGET A697 %u A0AA %u "
          "0x89CE8(shadow) %u | A699 %u | AB30 %u pedal A786 %u | block %s" %
          (d[0x2E], d[0x2F], d[0x30], d[0x31], d[0x32], d[0x33], d[0x34], d[0x36], d[0x37], d[0x35],
           u16(d, 0x38), d[0x3A], d[0x3B:0x45].hex(" ")))
    print("  WIT   " + " ".join("%s %u" % (n, u32(d, 0x48 + 4 * i)) for i, n in enumerate(WIT)))
    print("  IN    t %u d646 %u pedal %u gear %d os %u q_speed %u q_pedal %u" %
          (u32(d, 0x94), u16(d, 0x98), d[0x9A], struct.unpack("b", d[0x9B:0x9C])[0], d[0x9C], d[0x9D],
           d[0x9E]))
    n = min(u32(d, 0x48 + 4 * 17), 8)
    head = d[0x45]
    if n:
        print("  last %u target changes (oldest first):" % n)
        for k in range(n):
            i = (head - n + k) % 8
            o = 0xA0 + 16 * i
            print("    t %10u pass %5u AB30 %5u gear_in %u cand %u A697 %u pedal %u | D8 %u DA %u" %
                  (u32(d, o), u16(d, o + 4), u16(d, o + 6), d[o + 8], d[o + 9], d[o + 10], d[o + 11],
                   u16(d, o + 12), u16(d, o + 14)))


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 3
    raw = "--raw" in sys.argv
    b = tcu_bench.Buses(A())
    h = None
    for _ in range(10):
        h = tcu_bench.meas(b, 1, [0x02], timeout=1.5)
        if h:
            break
        time.sleep(0.4)
    print("hyg 0x02:", h.hex() if h else None, flush=True)
    for r in range(rounds):
        print("--- round %d %s" % (r, time.strftime("%H:%M:%S")), flush=True)
        d = read(b, 1, SM, SIZE)
        if d is None:
            print("  read FAIL")
            continue
        if raw:
            for o in range(0, SIZE, 32):
                print("  +%03X %s" % (o, d[o:o + 32].hex(" ")))
        show(d)
        live = []
        for name, a, n in LIVE:
            v = read(b, 1, a, n)
            live.append("%s %s" % (name, v.hex() if v is not None else "??"))
        print("  LIVE: " + " | ".join(live))
        time.sleep(1.0)


if __name__ == "__main__":
    main()
