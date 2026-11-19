#!/usr/bin/env python3
"""witness_g6a.py — G6a, THE DUAL-RUN COMPARATOR (firmware/app/g6a.c, g6a.h;
the candidate firmware/app/gs_cand.c + gs_cand_fill.c; ~/tcu_sweep/g6a_port.md).

The candidate (instance 2 of the garage engine template, the merged producer
in its G6b form, on its OWN RAM) runs every 10 ms beside the live path and
writes nothing live; the comparator records, every pass, the candidate's
A..Sys currents / A..E modes and fill states against what the F stage takes
(the bridge's value while applied, else the live producer through the
guard), classified:
  AGREE    no difference
  BOOT     expected: the boot window (E28E starts at 1, A refills) — closes at
           the first full agreement with E28E 0 and nothing filling, <= 16
  FENCED   expected: the bridge not applied, the shadow's engine owns the
           content (pf / releasing / pending / active / E28E != 0) and the
           candidate == the shadow
  FSMTAIL  expected: the bridge not applied, candidate == shadow, currents
           and modes == the wire, only non-filling fill states differ (the
           live objects never ran the engagement)
  UNEXP    everything else — the G6a discriminator

Reads over meas (6 bytes per request; NOT atomic — read in P, after the lever
sequence): g6a_t 932 B at 0xFFF93C08 (.g6a); the candidate's gs_wit_t (76 B)
and the shadow's (76 B); the walk poll / ISR counters and now_ms.

    witness_g6a.py [rounds] [--recs] [--raw]

  --recs    also print the first 16 disagreeing passes and the first 8
            unexpected ones (~160 more meas requests)

Addresses from nm of THIS build (sh-elf-nm build/sh2a/tcu_app.elf):
_s_g6a 0xFFF93C08, the candidate's _g_w 0xFFF928D0 (the second _g_w; the
shadow's is 0xFFF91440), _s_ae28_x 0xFFF92AD8, _s_walk_poll_count 0xFFF955BC,
_s_walk_isr_count 0xFFF955C0, _s_now_ms 0xFFF956A8; the shadow's _s_e469c_off
0xFFF916D8 (unchanged since r04a). Re-check before every use. The last one can
also be read ZERO-FLASH on r04m (same address) before installing r04n.

Bench prediction (r04n, ~/tcu_sweep/g6a_port.md §8):
  plain boot: magic 47364131, booted 1, boot window closed at pass ~13 with ~6
    disagreeing (A at 500 for 2 passes, then its refill 2 passes late), every
    later pass AGREE, UNEXP 0, law checks ~= passes with 0 mismatches, guard
    0, ae28_x 0; cand_us_max / task_us_max: the CPU witness (read and record).
  gs-check (--sim-brake --sim-engine 700 --start-before-power --hold 8):
    BOOT ~6; FSMTAIL after each release (~76 per release: B / the third
    element ramping +0x46 in the candidate, at 0 in the live objects);
    UNEXP 0 IF the shadow's 0xE469C garage fill-time term is on — BUT the
    r04l recorder shows it OFF on this bench (the third element's fill 8
    passes where the host's is 10: instance 1's host-negative flag
    s_e469c_off sits in uncleared NOLOAD RAM). Then: UNEXP ~85 per engagement
    (~170 for D + R), from the commit + 9 to the shadow's completion + 2, on
    B / C (D) and B / D (R) only, cand != shadow while applied ~= UNEXP,
    every record a 2-pass phase shift (candidate's third element still in
    phase 2 where the shadow's is in 4).
"""
import struct
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench  # noqa: E402

G6A = 0xFFF93C08
G6A_SZ = 932
CAND_W = 0xFFF928D0
SH_W = 0xFFF91440
AE28X = 0xFFF92AD8
SH_E469C_OFF = 0xFFF916D8   # instance 1's (the shadow's) s_e469c_off: NOLOAD, never written on SH
WALK = 0xFFF955BC
NOW = 0xFFF956A8
CLS = ["AGREE", "BOOT", "FENCED", "FSMTAIL", "UNEXP"]
CH = ["A", "D", "B", "E", "C", "WK", "Sys"]


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


def u32(d, o):
    return struct.unpack(">I", d[o:o + 4])[0]


def show_head(d):
    print("  magic %08X (want 47364131) booted %u boot_open %u | passes %u" % (u32(d, 0), d[140], d[141], u32(d, 4)))
    print("  classes: " + "  ".join("%s %u" % (CLS[k], u32(d, 8 + 4 * k)) for k in range(5)))
    print("  boot window closed at pass %s (%u disagreeing passes inside)"
          % ("open" if u16(d, 124) == 0xFFFF else u16(d, 124), u16(d, 126)))
    print("  per channel disagreeing: " + " ".join("%s %u" % (CH[c], u32(d, 28 + 4 * c)) for c in range(7)))
    print("  per channel UNEXPECTED:  " + " ".join("%s %u" % (CH[c], u32(d, 56 + 4 * c)) for c in range(7)))
    fu, lu = u32(d, 96), u32(d, 100)
    print("  mode/state-only passes %u | cand != shadow while applied %u | applied %u | first/last UNEXP %s / %s"
          % (u32(d, 84), u32(d, 88), u32(d, 92), "-" if fu == 0xFFFFFFFF else fu, "-" if lu == 0xFFFFFFFF else lu))
    print("  law cross-checks %u, mismatches %u (want 0) | guard fallbacks %u (want 0)"
          % (u32(d, 104), u32(d, 108), u32(d, 112)))
    print("  CPU: candidate tick max %u us (last %u) | comparison max %u us | boot pass %u us | task_10ms max %u us "
          "(last %u) over 1 ms %u of %u"
          % (u16(d, 128), u16(d, 130), u16(d, 132), u16(d, 134), u16(d, 136), u16(d, 138), u32(d, 120),
             u32(d, 116)))
    print("  predicted wire (last pass) A D B E C WK Sys: " + " ".join("%u" % u16(d, 144 + 2 * c) for c in range(7)))


def show_rec(tag, e):
    dms = u16(e, 4)
    print("   %s pass %5u %-7s dcur %02X modes %02X states %02X src %02X | cand E28E %u pf %u ph %02X/%02X | "
          "shadow ph %02X/%02X | cand %s | wire %s"
          % (tag, u16(e, 0), CLS[e[2]] if e[2] < 5 else "?", e[3], dms & 0xFF, dms >> 8, e[6], e[7] & 0x0F,
             e[7] >> 4, e[8], e[9], e[10], e[11], " ".join("%4u" % u16(e, 12 + 2 * c) for c in range(5)),
             " ".join("%4u" % u16(e, 22 + 2 * c) for c in range(5))))


def show_wit(name, w):
    print("  %s: passes %u commits %u (last %u -> %u at %u) done cause %u at %u | release passes %u | unported %u "
          "| E28E %u E6A8 %u pf %u gear %d ph %02X/%02X/%02X | cur %s"
          % (name, u32(w, 0), u32(w, 4), w[12], w[13], u32(w, 8), w[14], u32(w, 16), u32(w, 36), u32(w, 32),
             w[50], w[51], w[52], struct.unpack("b", w[56:57])[0], w[57], w[58], w[59],
             " ".join("%u" % u16(w, 60 + 2 * c) for c in range(7))))


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 1
    recs = "--recs" in sys.argv
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
        d = read(b, 1, G6A, 164)
        if d is None:
            print("  read FAIL")
            continue
        if raw:
            for o in range(0, 164, 32):
                print("  +%03X %s" % (o, d[o:o + 32].hex(" ")))
        show_head(d)
        cw = read(b, 1, CAND_W, 76)
        sw = read(b, 1, SH_W, 76)
        x = read(b, 1, AE28X, 4)
        eo = read(b, 1, SH_E469C_OFF, 1)
        wk = read(b, 1, WALK, 8)
        nw = read(b, 1, NOW, 4)
        if cw:
            show_wit("candidate", cw)
        if sw:
            show_wit("shadow   ", sw)
        if x:
            print("  candidate live-AE28 vs own lookup mismatches %u (want 0)" % u32(x, 0))
        if eo:
            print("  the SHADOW's s_e469c_off (0xFFF916D8, uncleared NOLOAD) = %02X: %s" % (
                eo[0], "0 - the shadow's garage fill-time term is ON (UNEXP 0 expected)" if eo[0] == 0 else
                "nonzero - the shadow's 0xE469C term is OFF: ~85 UNEXP per engagement expected (g6a_port.md 6)"))
        if wk and nw:
            p, i = u32(wk, 0), u32(wk, 4)
            print("  walk: poll %u isr %u (poll share %.1f %%) now_ms %u" % (p, i, 100.0 * p / max(1, p + i),
                                                                             u32(nw, 0)))
        if recs and r == rounds - 1:
            rn, un = d[142], d[143]
            rd = read(b, 1, G6A + 164, 32 * 24)
            if rd is None:
                print("  records read FAIL")
            else:
                print("  first %u disagreeing passes:" % rn)
                for k in range(min(rn, 16)):
                    show_rec("rec ", rd[32 * k:32 * k + 32])
                print("  first %u UNEXPECTED passes:" % un)
                for k in range(min(un, 8)):
                    show_rec("UNEX", rd[32 * (16 + k):32 * (16 + k) + 32])
        time.sleep(1.0)
    b.shutdown()
    print("witness done", flush=True)


if __name__ == "__main__":
    main()
