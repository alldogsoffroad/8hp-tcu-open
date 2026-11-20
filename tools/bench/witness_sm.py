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

(2026-11-19, ~/tcu_sweep/gear_feedback_shadow_port.md, r04w) it also reads
the shadow's GEAR FEEDBACK CHAIN, struct gf_t (0x9C bytes at 0xFFF86920,
section .sm_gear — nm `_s_gf`): E4A4 / E6A1 / E4A7 (the commanded gear =
the shift target's gear_in 0x89C28) / E69F / CCE4 / 0x89C25..2E / E33C+7,
its witnesses and last changes, and the LIVE engine's E4A7 / E69F / E69B
for comparison (read-only both ways).

Expected on the bench (dry, standstill): A697 = A0AA = CE8 = 1, candidate 1,
gear_in = the chain's 0x89C28 = 1 in P and D (0xFF while R is engaged → the
A694 "negative → 9" path, neg9 counting, A694 1), DA 160 (2->1 at pedal 23),
D2 160, D6 750, pedal 23 (no 0x120), AB30 0, min gear 1, errors 0, the
arbitration skipping 4 ids per pass, the confirm counted every pass (E0 0xFFFF
/ 0 by direction, C0 FF on a down check), ~33 passes / s in modes 1..3; the
chain ~100 passes / s, E4A7 = E69F = live E4A7 = live E69F at rest, err 0,
unp 0, strobes = done. Addresses: re-check with sh-elf-nm before every use.
`meas 0x02` hygiene first (CLAUDE rule 6).
"""
import struct
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench  # noqa: E402

SM = 0xFFF86800
SIZE = 0x120
LIVE = [("0x89CE8", 0xFFF89CE8, 1), ("OS mode 0x93B33", 0xFFF93B33, 1),
        ("live E4A7", 0xFFF8E4A7, 1), ("live E69B", 0xFFF8E69B, 1), ("live E69F", 0xFFF8E69F, 1),
        ("live E6EE/EF", 0xFFF8E6EE, 2)]
GF = 0xFFF86920
GF_SIZE = 0x9C
GF_WIT = ["init", "pass", "skip_mode", "strobe", "done", "e4a7_chg", "d1990", "d1b10", "lim", "ret1",
          "unp", "err", "agree", "dis", "gp(unported)"]
WIT = ["init", "pass", "skip_mode", "walk", "lookup", "iter_max", "err", "err_last", "arb_skip",
       "arb_none", "corr(unported)", "conf", "clamp", "win", "mingear(0x4F)", "range(0x96)",
       "gear_inv", "chg"]


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
    print("  CONF  E0 %u C0 %u AD %u 84 %u | neg9 %u" % (u16(d, 0x46), d[0x90], d[0x91], d[0x92], d[0x93]))
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


def sb(d, o):
    return struct.unpack("b", d[o:o + 1])[0]


def show_gf(g):
    print("  GEAR  E4A4 %d E4A5 %u E4A6 %u E4A7 %d (cmd) | E6A1 %d | E69C %s E69F %d E6A0 %02X | CCE4 %d %d %d %d" %
          (sb(g, 0), g[1], g[2], sb(g, 3), sb(g, 4), g[5:8].hex(" "), sb(g, 8), g[9],
           sb(g, 0xA), sb(g, 0xB), sb(g, 0xC), sb(g, 0xD)))
    print("        0x89C25.. %d %d %d | 0x89C28.. %d %d %d | 0x89C2B.. %d %d %d %d | E33C+7 %d +8 %u | "
          "8519F %d 851A0 %u | AE2E %u AE2F %d" %
          tuple([sb(g, 0xE + i) for i in range(3)] + [sb(g, 0x11 + i) for i in range(3)] +
                [sb(g, 0x14 + i) for i in range(4)] + [sb(g, 0x18), g[0x19], sb(g, 0x1A), g[0x1B], g[0x1C],
                                                      sb(g, 0x1D)]))
    print("  GWIT  " + " ".join("%s %u" % (n, u32(g, 0x20 + 4 * i)) for i, n in enumerate(GF_WIT)))
    o = 0x5C
    print("  GIN   E69B %u E6EE %u E6EF %u E28E %u c3d %u os %u CD2 %u live E4A7 %d live E69F %d | CB8 %u CBA %u "
          "CD0 %u" % (g[o], g[o + 1], g[o + 2], g[o + 3], g[o + 4], g[o + 5], g[o + 6], sb(g, o + 7), sb(g, o + 8),
                      u16(g, o + 10), u16(g, o + 12), u16(g, o + 14)))
    head = g[0x1E]
    print("  last chain changes (ring of 6, oldest first):")
    for k in range(6):
        i = (head + k) % 6
        q = 0x6C + 8 * i
        if u16(g, q) == 0:
            continue
        print("    pass %5u E4A4 %d E6A1 %d E4A7 %d E69F %d | live E4A7 %d E69B %u" %
              (u16(g, q), sb(g, q + 2), sb(g, q + 3), sb(g, q + 4), sb(g, q + 5), sb(g, q + 6), g[q + 7]))


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
        g = read(b, 1, GF, GF_SIZE)
        if g is None:
            print("  gear chain read FAIL")
        else:
            if raw:
                for o in range(0, GF_SIZE, 32):
                    print("  g+%03X %s" % (o, g[o:o + 32].hex(" ")))
            show_gf(g)
        live = []
        for name, a, n in LIVE:
            v = read(b, 1, a, n)
            live.append("%s %s" % (name, v.hex() if v is not None else "??"))
        print("  LIVE: " + " | ".join(live))
        time.sleep(1.0)


if __name__ == "__main__":
    main()
