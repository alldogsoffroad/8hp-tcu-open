#!/usr/bin/env python3
"""witness_fbshadow.py — the READ-ONLY PWM-feedback shadow (app/fb_shadow.c;
~/tcu_sweep/feedback_shadow_port.md). Reads struct fbs_t (0x34C bytes at
0xFFF86A00, section .fb_shadow — nm-check the build: `_s` in .fb_shadow)
over meas (6 bytes per request: a full read takes ~2-3 s and is NOT atomic —
counters and per-pass cells tear between blocks), plus the walk's
poll / ISR counters, and prints per channel the real chain next to the
stand-in.

    witness_fbshadow.py [rounds] [--raw] [--no-rec]

r04d+: after the rounds it reads the DISAGREEMENT RECORDER (struct fbr_t,
0xBE8 bytes at 0xFFF85400, section .fb_rec — nm `_s_rec`): the header, the
per-channel histogram of disagreeing (real, stand-in) class pairs, and the
first n_rec <= 128 recorded (pass, channel) events with their context (the
law's mode / current, seq2, ladder phase, the full-on flags 0x94114 /
0x94115, OS mode). Only the recorded entries are read (~20 bytes each; the
whole recorder takes ~10 s). --no-rec skips it.

Addresses: the r04b / r04c builds (nm): fbs_t 0xFFF86A00; s_walk_poll_count
0xFFF955BC, s_walk_isr_count 0xFFF955C0, s_now_ms 0xFFF956A8 (unchanged
from r04a). Re-check with sh-elf-nm before every use.
"""
import struct
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench  # noqa: E402

FBS = 0xFFF86A00
SIZE = 0x34C
FBR = 0xFFF85400            # fbr_t (r04d): +0 n_dis u32, +4 n_rec u16, +6 cap u16,
FBR_HIST, FBR_E, FBR_SPAN = 0x008, 0x170, 0xB70   # +8 hist[10][6][3] u16, +0x170 e[128] x 20 B,
                                                # +0xB70 span[10] (first_us, last_us u32, first/last pass u16)
RIDX = [0x00, 0x10, 0x20, 0x21, 0x31, None]
SIDX = [0x10, 0x21, None]
WALK_POLL, WALK_ISR, NOW_MS = 0xFFF955BC, 0xFFF955C0, 0xFFF956A8
NAMES = ["A", "D", "B", "E", "C", "WK", "Sys", "rel", "ch8", "ch9"]


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


def show(d, walk):
    print("  sampler: state %u time %06X iv %u %u %u reload %u | ISR %u loops %u max %u us last %u us | "
          "lock max %u us | windows %u / %u | state-1 (readback NOT launched) %u | passes %u | "
          "R updates %u | 0x6BDFE mismatches %u | armed %u" %
          (d[7], u32(d, 8), u16(d, 0), u16(d, 2), u16(d, 4), d[6], u32(d, 0x254), u32(d, 0x258),
           u32(d, 0x25C), u32(d, 0x280), u32(d, 0x260), u32(d, 0x264), u32(d, 0x268), u32(d, 0x26C),
           u32(d, 0x270), u32(d, 0x278), u32(d, 0x274), u32(d, 0x27C)))
    print("  R model: Tdrv %d Tfl %d (%.1f C) amb %d (%.1f C) 9410C/D/E %u/%u/%u countdown %u flag %u | "
          "seed waited %u passes | finit %04X" %
          (s16(d, 0x242), s16(d, 0x244), s16(d, 0x244) / 16.0, s16(d, 0x246), s16(d, 0x246) / 128.0,
           d[0x24E], d[0x24F], d[0x250], u16(d, 0x24C), d[0x251], u16(d, 0x252), u16(d, 0x240)))
    if walk:
        print("  walk: poll %u isr %u (poll share %.1f %%) now_ms %u" %
              (walk[0], walk[1], 100.0 * walk[0] / max(1, walk[0] + walk[1]), walk[2]))
    print("  ch   cap  P0   low0  low1 | REAL cls duty dutyf  est    R  rise hold | STAND-IN cls duty  est    R"
          " | agree   dis  last-dis  dduty")
    for ch in range(10):
        st = 0x10 + 12 * ch
        cmp = 0x284 + 20 * ch
        print("  %-4s  %02X %5u %5u %5u |      %02X %4u %5u %5u %5u %5u %3u |          %02X %4u %5u %5u"
              " | %5u %5u  %02X/%02X  %5u" %
              (NAMES[ch], d[st + 8], u16(d, 0xD8 + 4 * ch), u16(d, 0x100 + 4 * ch), u16(d, 0x102 + 4 * ch),
               d[0x218 + ch], u16(d, 0x128 + 2 * ch), u16(d, 0x13C + 2 * ch), u16(d, 0x1B4 + 2 * ch),
               u16(d, 0x1C8 + 2 * ch), u16(d, 0x1DC + 2 * ch), d[0x222 + ch],
               d[cmp + 16], u16(d, cmp + 8), u16(d, cmp + 10), u16(d, cmp + 12),
               u32(d, cmp), u32(d, cmp + 4), d[cmp + 17], d[cmp + 18], u16(d, cmp + 14)))


def show_rec(b):
    hdr = read(b, 1, FBR, 8)
    if hdr is None:
        print("  recorder: read FAIL")
        return
    n_dis, n_rec, cap = u32(hdr, 0), u16(hdr, 4), u16(hdr, 6)
    print("--- disagreement recorder (0x%08X): %u events, %u recorded (cap %u)%s" %
          (FBR, n_dis, n_rec, cap, "" if cap == 128 else "  ** cap != 128: wrong build / address? **"))
    hist = read(b, 1, FBR + FBR_HIST, 360)
    if hist is not None:
        print("  histogram (disagreements only, real x stand-in, saturating 65535):")
        for ch in range(10):
            cells = []
            for a in range(6):
                for k in range(3):
                    v = u16(hist, 2 * ((ch * 6 + a) * 3 + k))
                    if v:
                        ra = "%02X" % RIDX[a] if RIDX[a] is not None else "??"
                        sk = "%02X" % SIDX[k] if SIDX[k] is not None else "??"
                        cells.append("%s/%s=%u" % (ra, sk, v))
            print("    %-4s %s" % (NAMES[ch], " ".join(cells) if cells else "-"))
    sp = read(b, 1, FBR + FBR_SPAN, 120)
    if sp is not None:
        print("  per-channel span of ALL disagreements (TCNTA us / shadow pass):")
        for ch in range(10):
            o = 12 * ch
            if u32(sp, o + 4):
                print("    %-4s first %10u (pass %5u)  last %10u (pass %5u)" %
                      (NAMES[ch], u32(sp, o), u16(sp, o + 8), u32(sp, o + 4), u16(sp, o + 10)))
    n = min(n_rec, 128)
    if n == 0:
        return
    e = read(b, 1, FBR + FBR_E, 20 * n)
    if e is None:
        print("  recorder entries: read FAIL")
        return
    t0 = u32(e, 0)
    print("    #   t_us(TCNTA)   +ms  pass ch    real duty  si mode     I  phase seq2 f114 f115 os")
    for k in range(n):
        o = 20 * k
        t = u32(e, o)
        print("  %3u %11u %7.1f %5u %-4s   %02X %4u  %02X  %u  %6d  %04X  %02X   %02X   %02X  %u" %
              (k, t, ((t - t0) & 0xFFFFFFFF) / 1000.0, u16(e, o + 4), NAMES[e[o + 6]] if e[o + 6] < 10 else "?",
               e[o + 7], u16(e, o + 10), e[o + 8], e[o + 9], s16(e, o + 12), u16(e, o + 14), e[o + 16],
               e[o + 17], e[o + 18], e[o + 19]))


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
        w = read(b, 1, WALK_POLL, 8)
        n = read(b, 1, NOW_MS, 4)
        d = read(b, 1, FBS, SIZE)
        if d is None:
            print("  read FAIL")
            continue
        if raw:
            for o in range(0, SIZE, 32):
                print("  +%03X %s" % (o, d[o:o + 32].hex(" ")))
        walk = (u32(w, 0), u32(w, 4), u32(n, 0)) if (w and n) else None
        show(d, walk)
        time.sleep(1.0)
    if "--no-rec" not in sys.argv:
        show_rec(b)
    b.shutdown()
    print("witness done", flush=True)


if __name__ == "__main__":
    main()
