#!/usr/bin/env python3
"""witness_fr.py — the READ-ONLY fault-reaction / gear-code shadow
(firmware/app/fr_shadow.c + fr_fm.c; F1, ~/tcu_sweep/f1_fr_shadow_port.md,
decode ~/tcu_sweep/f0_fault_chain_decode.md).

Reads over meas (6 bytes per request; NOT atomic — fields tear between
blocks):
  fr_t    (0x568 B at 0xFFF86000, section .fr_shadow — nm `_s_fr`): the code
          block (0x89C41..44, 0x89CED, 0x89C43, AE2D, 0x89CF5, the fault-fed
          E4A4' / E6A1', the 0x13A9D4 latches), the comm monitor's flags and
          bus timers and the 53 RX status copies, the witnesses / comparators
  fr_fm_t (0xA1C B at 0xFFF88740, section .fr_fm — nm `_s_fm`): the masks
          0xFFF897D0 / D8 and the pending masks (ours), the time base, the
          counters, the status / timer of the bench groups, and the event
          recorder (the first 64 transitions: debounce start, pending on /
          off, confirm, direct confirm, heal, recycle, code / target / CF5 /
          mask changes, with the shadow's ms)
and, for comparison, the LIVE OS mode, range D6D6 and rng_chain's own U1267 /
P0850 verdicts (`_s_rc_m` +2 / +3) — the D-FR-3 comparator.

    witness_fr.py [rounds] [--events] [--raw]

Addresses: from `sh-elf-nm build/sh2a/tcu_app.elf` when present (checked
against the constants below), else the constants. `meas 0x02` hygiene first
(CLAUDE rule 6). One process per serial port.

Expected (see the report §8): plain boot — U11E4 confirm ≈ +1.6 s (pending
reactions ≈ 400 ms before), U1267 ≈ +1.65 s, P0850 ≈ +3.15 s, U11E3 ≈ +2.1 s,
masks 00813240 / E8260014, code 0x20, 0x89C43 3, AE2D 1, CF5 1, fault-fed
target 3; with the sims from power-on — U11E4 never fails, U0121 ≈ +1.45 s,
U0100 / U0140 / U0212 / U0102 ≈ +1.8 s, U0155 ≈ +6.2 s, P0850 ≈ +3.15 s,
masks 00812420 / C0260014, code 0x20 in P (3 from U0100's failing start),
4 → 0x1C → 0x20 at D, the fault-fed target 3 in P, −1 in R; c109dis =
c98dis = 0.
"""
import os
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench  # noqa: E402

ELF = "/home/chadk/Documents/REA Projects/build/sh2a/tcu_app.elf"
FR, FR_SIZE = 0xFFF86000, 0x568
FM, FM_SIZE = 0xFFF88740, 0xA1C
RC_M = 0xFFF87000
# fr_t offsets (fr_shadow.h _Static_asserts)
O_CM, O_CD, O_IN, O_W = 0x3BA, 0x484, 0x4CC, 0x504
# fr_fm_t offsets
O_S, O_T, O_TMAX, O_R, O_M0, O_NOW, O_REC = 0x000, 0x186, 0x492, 0x618, 0x7A0, 0x7F0, 0x81C
GROUPS = [(28, "P1D8E"), (98, "P0850"), (109, "U1267"), (111, "U0001"), (112, "U0074"), (136, "U11E4"),
          (139, "U0100"), (140, "U0121"), (141, "U0102"), (142, "U0103"), (143, "U11E3"), (145, "U0212"),
          (146, "U0140"), (147, "U0155")]
EV = {1: "start", 2: "CONFIRM", 3: "heal", 4: "pend-on", 5: "pend-off", 6: "direct", 7: "recycle",
      0x10: "code", 0x11: "target", 0x12: "cf5", 0x13: "masks"}
WIT = ["passes", "mon", "rx_frames", "rx_other", "c109dis", "c98dis", "d661dis", "tgtdis", "cf5_n",
       "cf5_off_n", "c109_first", "c98_first", "tgt_first", "unp", "ev49c", "ev49a", "ff_lim", "ff_err",
       "t_us_last", "t_us_max", "plaus_untested", "rep109_seen", "rep98_seen", "rep109_fwd", "rep98_fwd"]


class A:
    iface = "slcan"
    bus1 = "/dev/ttyACM0"
    bus2 = "/dev/ttyACM1"
    teensy_mode = None
    keep_rate = True


def nm_addrs():
    global FR, FM, RC_M
    if not os.path.exists(ELF):
        return
    out = subprocess.run(["sh-elf-nm", ELF], capture_output=True, text=True).stdout
    for name, var in (("_s_fr", "FR"), ("_s_fm", "FM"), ("_s_rc_m", "RC_M")):
        m = re.search(r"^([0-9a-f]{8}) [bBdD] %s$" % name, out, flags=re.M)
        if m:
            a = int(m.group(1), 16)
            if a != globals()[var]:
                print("NOTE: %s at 0x%08X (constant 0x%08X) — using nm" % (name, a, globals()[var]))
            globals()[var] = a


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


def sb(d, o):
    return struct.unpack("b", d[o:o + 1])[0]


def show_code(cd):
    print("  CODE  0x89C41 %02X 0x89C42 %02X 0x89C44 %u 0x89CED %u | 0x89C43 %d AE2D %u | 0x89CF5 %u | "
          "fault-fed E4A4' %d E6A1' %d | prev code 0x8519E %02X 519D %u" %
          (cd[0x3C], cd[0x3D], cd[0x3E], cd[0x3F], sb(cd, 0x40), cd[0x41], cd[0x44], sb(cd, 0x45), sb(cd, 0x46),
           cd[0x43], cd[0x42]))
    print("  13A   fwd %u o19(D6D6) %u reinit %u 1D %u 1E %u 1F %u A2B %u latch %s 39 %u 3A %u res %02X | t10 %u t12 %u "
          "t14 %u t16 %u | m24 %08X m28 %08X | 14E t %u r %u" %
          (cd[0], cd[2], cd[4], cd[5], cd[6], cd[7], cd[8], cd[9:15].hex(" "), cd[0xF], cd[0x10], cd[0x11],
           u16(cd, 0x16), u16(cd, 0x18), u16(cd, 0x1A), u16(cd, 0x1C), u32(cd, 0x30), u32(cd, 0x34),
           u16(cd, 0x38), cd[0x3A]))


def show_comm(cm):
    print("  COMM  t3a %u t3c %u ta8 %u taa %u | sup %u | b1 %u b2 %u | U0001 f/t %u/%u U11E4 f/t %u/%u act %u/%u | "
          "node en1 %u en2 %u any-conf %u" %
          (u16(cm, 0x36), u16(cm, 0x38), u16(cm, 0x3A), u16(cm, 0x3C), u16(cm, 0xA8), cm[0xAE], cm[0xAF],
           cm[0xB0], cm[0xB4], cm[0xB1], cm[0xB5], cm[0xB2], cm[0xB3], cm[0xB6], cm[0xBD], cm[0xAC]))
    st = cm[0:53]
    print("  RX st " + " ".join("%02X" % x for x in st))


def show_wit(w):
    vals = [u32(w, 4 * i) for i in range(len(WIT))]
    print("  WIT   " + " ".join("%s %u" % (n, v) for n, v in zip(WIT, vals)))


def show_fm(b, hdr):
    m0, m1, p0, p1 = u32(hdr, 0), u32(hdr, 4), u32(hdr, 8), u32(hdr, 12)
    now = u32(hdr, 0x50)
    cyc, rec_n = hdr[0x54], hdr[0x55]
    print("  FM    now %u ms cyc %u | MASKS %08X / %08X | PENDING %08X / %08X | rec %u (seen %u) | rep %u conf %u "
          "heal %u direct %u recycle %u dep %u rule_c2 %u badg %u" %
          (now, cyc, m0, m1, p0, p1, rec_n, u32(hdr, 0x58), u32(hdr, 0x5C), u32(hdr, 0x60), u32(hdr, 0x64),
           u32(hdr, 0x68), u32(hdr, 0x6C), u32(hdr, 0x70), u32(hdr, 0x74), u32(hdr, 0x78)))
    line = []
    for g, name in GROUPS:
        s = read(b, 1, FM + O_S + 2 * (g - 1), 2)
        t = read(b, 1, FM + O_T + 2 * (g - 1), 2)
        r = read(b, 1, FM + O_R + (g - 1), 1)
        if s is None or t is None or r is None:
            line.append("%s ??" % name)
            continue
        line.append("%s S%04X T%u R%02X" % (name, u16(s, 0), u16(t, 0), r[0]))
    print("  GRP   " + " | ".join(line))
    return rec_n


def show_events(b, n):
    if n == 0:
        return
    d = read(b, 1, FM + O_REC, 8 * n)
    if d is None:
        print("  events read FAIL")
        return
    print("  EVENTS (the first %u, shadow ms):" % n)
    for i in range(n):
        o = 8 * i
        kind, g, v = d[o + 4], d[o + 5], u16(d, o + 6)
        name = EV.get(kind, "%02X" % kind)
        extra = ""
        if kind == 0x10:
            extra = " code %02X -> 0x89C43 %d" % (g, sb(bytes([v & 0xFF]), 0))
        elif kind == 0x11:
            extra = " E6A1' %d (E4A4' %d)" % (sb(bytes([g]), 0), sb(bytes([v & 0xFF]), 0))
        elif kind == 0x12:
            extra = " 0x89CF5 = %u" % g
        else:
            extra = " g %u v %u" % (g, v)
        print("    %7u ms %-8s%s" % (u32(d, o), name, extra))


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 2
    events = "--events" in sys.argv
    raw = "--raw" in sys.argv
    nm_addrs()
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
        cd = read(b, 1, FR + O_CD, 0x48)
        cm = read(b, 1, FR + O_CM, 0xC8)
        w = read(b, 1, FR + O_W, 0x64)
        hdr = read(b, 1, FM + O_M0, 0x7C)
        if None in (cd, cm, w, hdr):
            print("  read FAIL")
            continue
        if raw:
            for nm, d in (("cd", cd), ("cm", cm), ("w", w), ("fm", hdr)):
                for o in range(0, len(d), 32):
                    print("  %s+%03X %s" % (nm, o, d[o:o + 32].hex(" ")))
        show_code(cd)
        show_comm(cm)
        show_wit(w)
        n = show_fm(b, hdr)
        if events or r == rounds - 1:
            show_events(b, n)
        live = []
        for name, a, k in (("OS mode", 0xFFF93B33, 1), ("D6D6", 0xFFF8D6D6, 1), ("rng g109/g98", RC_M + 2, 2),
                           ("rng fm_ms", RC_M + 0x20, 4)):
            v = read(b, 1, a, k)
            live.append("%s %s" % (name, v.hex() if v is not None else "??"))
        print("  LIVE: " + " | ".join(live))
        time.sleep(1.0)


if __name__ == "__main__":
    main()
