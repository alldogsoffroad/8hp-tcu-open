#!/usr/bin/env python3
"""witness_g6a.py — THE COMPARATOR (firmware/app/g6a.c, g6a.h) of G6a and G6b, the
candidate (firmware/app/gs_cand.c + gs_cand_fill.c) and — G6b — the monitor
(firmware/app/gs_bridge.c mon_tick); ~/tcu_sweep/g6a_port.md, ~/tcu_sweep/g6b_port.md.

G6c (r04p on, ~/tcu_sweep/g6c_port.md): THE SHADOW IS DELETED. Every 10 ms:
candidate (the engine) -> monitor (the seam: the candidate, or the E28E
fence's live producer, or the latched P content) -> the wire self-check.
Classes (one per pass):
  AGREE    wire == candidate (r04o's FSMTAIL passes count here now)
  BOOT     expected: the boot window (the boot fence: the live producer's boot
           fill on the wire while the candidate's E28E transient runs; <= 16).
           r04q (D-G6-13): no boot fence — the candidate's stock boot sequence is
           on the wire (the boot allowance), so BOOT reads 0 and the window
           closes at the end of A's fill (pass ~12)
  FENCED   expected: wire != candidate because the monitor fenced (E28E: the
           engine-stop all-release) or latched
  FSMTAIL  retired (never counted since G6c)
  UNEXP    the seam carried the candidate and the wire differs — must be 0
r04r (~/tcu_sweep/r04r_port.md): the live holder is retired; the wire replica
now covers WK / Sys (ch 5 / 6) from the engine through the 500-only guard.
Retired fields (always 0 since G6c): cand != shadow, the shadow's tick time,
the records' shadow phases. Also printed: the engine's objects at their stock
addresses (D-G6-9): E28E, E4A7, E4AC, E69B, E69F, E6A1, E6A8, E6C5, E6EE,
E6EF and the shift object BD14 (BD16, BD17, BD2E, BD30, BD6C, BD76, BD8E) —
dump-comparable cells (the driving dump, P long-idle, holds E28E 00, E4B1 01,
E4B2 FF, E6A8 01 00 00 00 02, BD2E FFFF, BD30 FF, BD6C 01, BD6D 03, BD76 7FFF,
BD8E FFFF; ours differ by decision in E4A7 / E69F / E6A1 (1st, D-G6-3) and
E6C5 (r04b..r04u FF: 0x89C3D forced 0, D-G6-2; r04v G5c: 00 in OS mode 1 — the
stock's slot-40 clear — and counting to FF from the mode-2 entry)).

Reads over meas (6 bytes per request; NOT atomic — read in P, after the lever
sequence): g6a_t 1008 B at 0xFFF93C08 (.g6a): the head (164 B), the records,
the G6b CPU block (+932, 76 B); the candidate's gs_wit_t (76 B); the monitor block of gsb_t (+172, 40 B at 0xFFF87220 + 172) and the
window latch (0xFFF87200 + 16); the walk poll / ISR counters and now_ms.

    witness_g6a.py [rounds] [--recs] [--raw]

Addresses from nm of THIS build (sh-elf-nm build/sh2a/tcu_app.elf):
_s_g6a 0xFFF93C08, the candidate's _g_w 0xFFF928BC (r04p; the only _g_w since
G6c), _s_ae28_x 0xFFF9293C (r04s; r04p..r04r 0xFFF92A98), _s_gsb 0xFFF87220, _s_gsb_win
0xFFF87200, _s_walk_poll_count 0xFFF955BC, _s_walk_isr_count 0xFFF955C0,
_s_now_ms 0xFFF956A8. Re-check before every use.

r04s (~/tcu_sweep/r04s_port.md): also the element sub-objects at their stock
addresses (0xFFF8D0DC / D148 / D1B4, 0x6C each: phase, class, element, +0x1E,
+0x3A, the vtable word 0x49FC0 at +0x50, the three cell pointers
0xFFF8CF94 / CF0C / CE7C + 8·(class − 2)), E6C1 / E6C2 (0xDC0F8), E4B3 (E4AC+7,
the pedal latch: 01 from the first slot-35 pass, the pedal stand-in 23 >= 13),
E4AC+0 (D0DC phase in 0x0B/0x0C/0x0E), E33C+0x12 (E34E: passes since 0x89C2F
read 0, saturating at FFFF) and BD1C / BD1D / BD44 (0xEC6FE / 0xEB900).

CPU (G6b, the bench's CPU stop conditions: scheduler missed_ticks > 0, any ring
overflow, the walk polled share > 10 %): the candidate's tick by slot group —
head, s28 (timers, element tick / update), s29 (engine state), s30 (AE28,
E28E, request), s31/32 (commit, strobe), s34 (sync progress), s35 (element
steps / outputs), s42 (producer + actuators), tail — max, last and AT THE
candidate's max pass (names the peak's source); the monitor; the comparison.
"""
import struct
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench  # noqa: E402

G6A = 0xFFF93C08
G6A_SZ = 1008
CPU_OFF = 932
GSB = 0xFFF87220
GSB_WIN = 0xFFF87200
STG = ['head', 's28', 's29', 's30', 's31/32', 's34', 's35', 's42', 'tail']
SRC = ['LIVE (before the boot pass)', 'CANDIDATE', 'P CONTENT (latched)', 'STALL FENCE (P content)']
CAND_W = 0xFFF928BC         # r04p (G6c): the candidate's _g_w (r04o 0xFFF928D0)
AE28X = 0xFFF9293C          # r04s: _s_ae28_x (r04p..r04r 0xFFF92A98, r04o 0xFFF92AD8)
# G6c (D-G6-9): the engine's objects at their stock addresses (name, addr, len)
PINNED = [("E28E st..t8", 0xFFF8E28E, 10), ("E33C..E34F", 0xFFF8E33C, 0x14), ("E4A7", 0xFFF8E4A7, 1),
          ("E4AC..E4B5", 0xFFF8E4AC, 10), ("E6C1/E6C2", 0xFFF8E6C1, 2),
          ("E69B", 0xFFF8E69B, 1), ("E69F", 0xFFF8E69F, 1), ("E6A1", 0xFFF8E6A1, 1), ("E6A8", 0xFFF8E6A8, 5),
          ("E6C5", 0xFFF8E6C5, 1), ("E6EE", 0xFFF8E6EE, 1), ("E6EF", 0xFFF8E6EF, 1),
          ("BD14..BD31", 0xFFF8BD14, 0x1E), ("BD6C..BD8F", 0xFFF8BD6C, 0x24)]
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
    print("  mode/state-only passes %u | (retired: cand != shadow) %u | seam "
          "value passes %u | first/last UNEXP %s / %s"
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


def show_r04s(b):
    """r04s: the reversal cells and the element sub-objects at their stock addresses."""
    e6 = read(b, 1, 0xFFF8E6C1, 2)
    ea = read(b, 1, 0xFFF8E4AC, 10)
    e3 = read(b, 1, 0xFFF8E33C, 0x14)
    bd = read(b, 1, 0xFFF8BD14, 0x32)
    if e6 and ea and e3 and bd:
        print("  r04s: E6C1 %02X E6C2 %d (0xDC0F8) | E4B3 %02X (pedal latch) E4AC+0 %02X +2 %04X +5 %02X +9 %02X | "
              "E34E %04X | BD1C %02X BD1D %02X BD44 %02X"
              % (e6[0], struct.unpack("b", e6[1:2])[0], ea[7], ea[0], u16(ea, 2), ea[5], ea[9], u16(e3, 0x12),
                 bd[8], bd[9], bd[0x30]))
    for k, nm in enumerate(("D0DC", "D148", "D1B4")):
        s = read(b, 1, 0xFFF8D0DC + 0x6C * k, 0x6C)
        if s is None:
            print("  sub %s read FAIL" % nm)
            continue
        pv = [u32(s, o) for o in (0x58, 0x5C, 0x60)]
        want = [0xFFF8CF94 + 8 * k, 0xFFF8CF0C + 8 * k, 0xFFF8CE7C + 8 * k]
        cells = []
        for a in pv:
            c = read(b, 1, a, 8) if 0xFFF8CE7C <= a < 0xFFF8CFAC else None
            cells.append("%08X:%s" % (a, c.hex().upper() if c else "?"))
        print("  sub %s: phase %02X out_mode %02X cls %u eng %u rel %u act %u +1E %u +3A %d type %u | vtbl %08X (want "
              "00049FC0) | cells %s%s | +64 %08X +68 %08X"
              % (nm, s[0x11], s[0x10], s[0x1C], s[0x20], s[0x21], s[0], s[0x1E],
                 struct.unpack(">h", s[0x3A:0x3C])[0], s[0x15], u32(s, 0x50), " ".join(cells),
                 "" if pv == want else " (POINTERS NOT THE STOCK CELLS)", u32(s, 0x64), u32(s, 0x68)))


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
        x = read(b, 1, AE28X, 4)
        wk = read(b, 1, WALK, 8)
        nw = read(b, 1, NOW, 4)
        cpu = read(b, 1, G6A + CPU_OFF, 76)
        if cpu:
            print("  CPU (us) candidate stages max:   " + " ".join("%s %u" % (STG[k], u16(cpu, 2 * k)) for k in range(9)))
            print("  CPU (us) candidate stages last:  " + " ".join("%s %u" % (STG[k], u16(cpu, 18 + 2 * k)) for k in range(9)))
            print("  CPU (us) stages at the max pass (comparator pass %u): " % u32(cpu, 64)
                  + " ".join("%s %u" % (STG[k], u16(cpu, 36 + 2 * k)) for k in range(9)))
            print("  CPU (us) monitor max %u last %u | comparison last %u (retired since G6c: shadow tick %u/%u)"
                  % (u16(cpu, 58), u16(cpu, 60), u16(cpu, 62), u16(cpu, 54), u16(cpu, 56)))
        mon = read(b, 1, GSB + 172, 52)                  # r04q: + the boot allowance, fence_n (+212..223)
        gw = read(b, 1, GSB_WIN, 20)
        if mon and gw:
            tp = u32(mon, 8)
            print("  r04q boot allowance %02X (03 = used, 8x = violated) entries %u (want 1) evaluations %u "
                  "from pass %u | stall-fence passes %u" % (mon[40], mon[41], u16(mon, 42), u32(mon, 44),
                                                           u32(mon, 48)))
            print("  MONITOR: seam now %s | candidate %u pre-boot %u P content %u passes | stall-fence entries %u "
                  "exits %u (first exit pass %u) | trips %u (want 0)%s | latch %02X (want 00) acc %u ms commits %u"
                  % (SRC[mon[0]] if mon[0] < 4 else mon[0], u32(mon, 12), u32(mon, 16), u32(mon, 20),
                     u16(mon, 28), u16(mon, 30), u32(mon, 32), u16(mon, 38),
                     "" if tp == 0xFFFFFFFF else " first at pass %u why %04X ch %u v %u" % (tp, u16(mon, 6), mon[5],
                                                                                      u16(mon, 36)),
                     gw[16], u32(gw, 4), u16(gw, 12)))
        if cw:
            show_wit("candidate", cw)
        if x:
            print("  candidate live-AE28 vs own lookup mismatches %u (want 0)" % u32(x, 0))
        pv = []
        for nm, a, n in PINNED:
            v = read(b, 1, a, n)
            pv.append("%s %s" % (nm, v.hex().upper() if v else "?"))
        print("  stock objects (G6c pinned): " + " | ".join(pv))
        show_r04s(b)
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
