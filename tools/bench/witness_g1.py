#!/usr/bin/env python3
"""witness_g1.py — THE GARAGE BRIDGE (firmware/app/gs_bridge.c): G1 (computed,
recorded, NOT applied; ~/tcu_sweep/garage_g1_port.md), G2..G4b (the bridge of the
shadow, r04i..r04n) and — G6b (~/tcu_sweep/g6b_port.md, r04o on) — THE MONITOR:
the candidate (app/gs_cand.c) is the producer of the live A..E currents; the
seam carries it every pass unless the E28E fence hands back the live producer
(src LIVE: the boot transient, the engine-stop all-release) or the monitor has
latched the P content (src PCONT: a violation, until power-off).
r04q (D-G6-13/14, ~/tcu_sweep/r04q_port.md): no boot fence — the candidate's
stock boot sequence is carried under the once-per-boot BOOT ALLOWANCE (gsb_t
+212: state, entries, evaluations, entry pass); src LIVE = before the boot pass
only; the engine-stall fence hands the seam THE P CONTENT (src FENCE, passes
at +220).
In G6b the state's "shadow" A..E and the recorder's phases are THE CANDIDATE's;
"applied" = the seam returned a value (candidate or P content); "would-armed" =
the candidate's engine on the wire (owning or non-P content).
r04r (~/tcu_sweep/r04r_port.md): the live holder is retired — the engine is the
sole producer of ch 0..6 (WK / Sys through the seam, the law passing only 500);
"agree / disagree" now compare the candidate with the CONSTANT P CONTENT (the
seam's fallback; 0 before the boot pass), no longer with the holder's 0x84B78.

Reads over meas (6 bytes per request; NOT atomic — counters tear between
blocks; read after the lever sequence, with the bridge idle in P):
  gsb_win_t  20 B at 0xFFF87200 (.gsb_win)   the .noinit windows
  gsb_t     224 B at 0xFFF87220 (.gsb_state) state, witnesses, slot headers,
                                              the OS-mode-2 witness (0x1418E2),
                                              G6b: the monitor block (+172)
  gsb_rec_t 112 x 36 B at 0xFFF911A0 (.gsb_rec0, the last D; G6c — r04o 0xFFF8BCC0)
  gsb_rec_t 112 x 36 B at 0xFFF8EE00 (.gsb_rec1, the last R; G6c — r04o 0xFFF89D20)
plus, for comparison, LIVE cells the bridge must never change (D6D6, the park
model, the OS mode, seq2 / letter, the law's gated current 0x94ED4 for A D B E C
WK Sys, the fault word 0x93004 A..E). r04r: the live holder (app/rq_prod.c) is
retired — 0xFFF84B78 is no longer written (stock RAM, stale until power-off);
the engine produces ch 0..6, and the law's 0x94ED4 shows what reached the law.

    witness_g1.py [rounds] [--rec N] [--quick] [--raw]

  --rec N   read only the first N recorder entries of each slot (default 112;
            a full slot is ~670 meas requests)
  --quick   no recorder (state, windows, slot headers, mode-2 witness only)

Addresses from nm of THIS build (sh-elf-nm build/sh2a/tcu_app.elf | grep gsb):
_s_gsb_win 0xFFF87200, _s_gsb 0xFFF87220, _s_gsb_rec0 0xFFF911A0,
_s_gsb_rec1 0xFFF8EE00 (r04p, G6c: off the stock cells into the deleted
shadow's RAM). Re-check before every use.
G6c: the shadow is deleted — the monitor's "candidate != shadow" check with it
(latch bit 0x20 SHADOW never set, the shadow-diff counters stay 0).

Reading a G1 run (the gs-check with --sim-brake --sim-engine 700):
  - slot D / R: arm_on 0 (would-arm from the commit pass), done_at ~93..96
    (cause 1), arm_off = the lever-P pass (why RANGE|PARK|IDLE|DIR), back_at
    = arm_off (+0..2), guard_n 0;
  - each entry: st bit0 set, why '-', the would-be words = the shadow's
    currents / 6 (B / C in D; B / D in R), the live staged words = the P
    content (ED88 F588 E853 F588 ED88 +-1: G1 applies nothing);
  - windows: commits 2, latch 00, acc ~3.7 s per gs-check (D + R held 2 s each);
  - mode-2 witness: req 2 from the first mode-1 pass with the engine sim on
    (latched: stays 2 after the sim stops), live os 1 (never applied).
"""
import struct
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench  # noqa: E402

WIN = 0xFFF87200
ST = 0xFFF87220
REC = (0xFFF911A0, 0xFFF8EE00)   # r04p (G6c); r04o 0xFFF8BCC0 / 0xFFF89D20
RECN = 112
RECSZ = 36
LIVE = [
    ("D6D6", 0xFFF8D6D6, 1), ("park model BC70", 0xFFF8BC70, 1), ("OS mode 0x93B33", 0xFFF93B33, 1),
    ("seq2 0x94FD0", 0xFFF94FD0, 1), ("letter 0x90944", 0xFFF90944, 1),
    ("law current A D B E C WK Sys 0x94ED4", 0xFFF94ED4, 14), ("fault word A..E 0x93004", 0xFFF93004, 10),
]
WHY = ["IDLE", "DIR", "RAIL", "OS", "RANGE", "PARK", "MON", "DIAG", "RELWIN", "WIN", "ENGT", "COMMITS",
       "AGREE(r04o: cand!=shadow, retired G6c)", "G_RANGE", "G_FIXED", "G_LATCH"]
LATCH = ["WIN 30s", "ENGT 3.5s", "COMMITS 16", "GUARD bounds/envelope", "PERMIT", "SHADOW", "IDLE non-P", "?"]
SRC = ["LIVE (before the boot pass)", "CAND", "PCONT (latched)", "FENCE (stall: the P content)"]
BOOT = {0: "not yet", 1: "all-release row", 2: "A's switch-on fill", 3: "used (ended on the P content)"}
BOOTV = {1: "a second boot entry", 2: "content off the allowed rows", 3: "all-release after the fill",
         4: "beyond 15 evaluations", 5: "the entry not the all-release row"}
ST_SZ = 224


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


def why(w):
    s = [WHY[k] for k in range(16) if (w >> k) & 1]
    return "|".join(s) if s else "-"


def none(v):
    return "-" if v == 0xFFFF else str(v)


def latch_str(v):
    s = [LATCH[k] for k in range(8) if (v >> k) & 1]
    return "|".join(s) if s else "none"


def show_win(d):
    print("  windows: magic %08X acc %u ms (inv ok %s) commits %u (inv ok %s) latch %02X [%s] (inv ok %s) warm %u "
          "broken %u"
          % (u32(d, 0), u32(d, 4), u32(d, 8) == (~u32(d, 4) & 0xFFFFFFFF), u16(d, 12),
             u16(d, 14) == (~u16(d, 12) & 0xFFFF), d[16], latch_str(d[16]), d[17] == (~d[16] & 0xFF), d[18],
             d[19]))


def show_mon(d):
    m = 172
    tp = u32(d, m + 8)
    print("  G6b MONITOR: seam src now %s | fence %u owns %u non-P %u | passes: candidate %u, before the boot "
          "pass %u, P content %u, stall fence %u | stall-fence entries %u exits %u (first exit at bridge pass %u)"
          % (SRC[d[m]] if d[m] < 4 else "?", d[m + 1], d[m + 2], d[m + 3], u32(d, m + 12), u32(d, m + 16),
             u32(d, m + 20), u32(d, 220), u16(d, m + 28), u16(d, m + 30), u32(d, m + 32)))
    b = d[212]
    print("  r04q BOOT ALLOWANCE: %s | entries %u (want 1) | evaluations %u (<= 15) from bridge pass %u"
          % (("VIOLATED: " + BOOTV.get(b & 0x7F, "?")) if b & 0x80 else BOOT.get(b, "?"), d[213], u16(d, 214),
             u32(d, 216)))
    print("  G6b trips %u (want 0): first at pass %s why %s ch %s v %u | shadow-diff passes %u (run %u; retired G6c, 0)"
          % (u16(d, m + 38), "-" if tp == 0xFFFFFFFF else tp, why(u16(d, m + 6)),
             "-" if d[m + 5] == 0xFF else d[m + 5], u16(d, m + 36), u32(d, m + 24), d[m + 4]))


def show_state(d):
    print("  bridge: passes %u would-armed %u applied %u (G1: 0) seam %u (G1: 0) | agree %u disagree %u (last %u)"
          % (u32(d, 0), u32(d, 4), u32(d, 8), u32(d, 12), u32(d, 16), u32(d, 20), u32(d, 24)))
    print("  events: arm %u disarm %u guard %u (last ch %u v %u why %s) | why now %s | eng_t %u on %u | "
          "armed %u applied %u agree_run %u pf %u slot_cur %u"
          % (u16(d, 36), u16(d, 38), u16(d, 40), d[52], u16(d, 44), why(u16(d, 42)), why(u16(d, 46)),
             u16(d, 48), d[57], d[53], d[54], d[55], d[56], d[58]))
    print("  A..E shadow %s | live %s | would-be %s" %
          (" ".join("%5u" % u16(d, 60 + 2 * c) for c in range(5)),
           " ".join("%5u" % u16(d, 70 + 2 * c) for c in range(5)),
           " ".join("%5u" % u16(d, 80 + 2 * c) for c in range(5))))
    for k in range(2):
        o = 100 + 20 * k
        print("  slot %u (%s): t0 %u n %u pf %u | arm_on %s arm_off %s (%s) done_at %s cause %u back_at %s "
              "armed_n %u guard_n %u"
              % (k, "D" if k == 0 else "R", u32(d, o), d[o + 16], d[o + 17], none(u16(d, o + 4)),
                 none(u16(d, o + 6)), why(u16(d, o + 8)), none(u16(d, o + 10)), d[o + 18],
                 none(u16(d, o + 12)), u16(d, o + 14), d[o + 19]))
    m = 140
    print("  OS-mode-2 witness (0x1418E2, NOT applied): req %u app %u | live os %u E9FF %u 9584A %u | eng %u st %u "
          "| tur %u st %u | supply %u | n %u n2 %u first2 %u changes %u"
          % (d[m], d[m + 1], d[m + 2], d[m + 3], d[m + 4], u16(d, m + 8), d[m + 5], u16(d, m + 10), d[m + 6],
             u16(d, m + 12), u32(d, m + 16), u32(d, m + 20), u32(d, m + 24), u32(d, m + 28)))


def show_rec(k, d, n):
    print("  slot %u (%s) entries 0..%u:" % (k, "D" if k == 0 else "R", n - 1))
    print("   pass ph0 ph1 ph2 st why            | would-be A D B E C       | live staged A D B E C    "
          "| est A D B E C")
    for i in range(n):
        e = d[i * RECSZ:(i + 1) * RECSZ]
        print("   %4u %3X %3X %3X %02X %-14s | %s | %s | %s" %
              (i, e[0], e[1], e[2], e[3], why(u16(e, 4)),
               " ".join("%04X" % u16(e, 6 + 2 * c) for c in range(5)),
               " ".join("%04X" % u16(e, 16 + 2 * c) for c in range(5)),
               " ".join("%5u" % u16(e, 26 + 2 * c) for c in range(5))))


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 1
    nrec = RECN
    if "--rec" in sys.argv:
        nrec = max(0, min(RECN, int(sys.argv[sys.argv.index("--rec") + 1])))
    if "--quick" in sys.argv:
        nrec = 0
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
        w = read(b, 1, WIN, 20)
        s = read(b, 1, ST, ST_SZ)
        if w is None or s is None:
            print("  read FAIL")
            continue
        if raw:
            for o in range(0, ST_SZ, 32):
                print("  +%03X %s" % (o, s[o:o + 32].hex(" ")))
        show_win(w)
        show_state(s)
        show_mon(s)
        live = []
        for name, a, n in LIVE:
            v = read(b, 1, a, n)
            live.append("%s %s" % (name, v.hex() if v is not None else "??"))
        print("  LIVE (never written by the bridge): " + " | ".join(live))
        if nrec and r == rounds - 1:
            for k in range(2):
                d = read(b, 1, REC[k], nrec * RECSZ)
                if d is None:
                    print("  slot %u read FAIL" % k)
                    continue
                show_rec(k, d, nrec)
        time.sleep(1.0)
    b.shutdown()
    print("witness done", flush=True)


if __name__ == "__main__":
    main()
