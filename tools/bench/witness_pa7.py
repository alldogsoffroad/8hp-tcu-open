#!/usr/bin/env python3
"""witness_pa7.py — the READ-ONLY PA7 / "TRS Park signal" shadow
(firmware/app/pa7_shadow.c; ~/tcu_sweep/r04d_recorder_pa7_shadow.md).

Reads struct pa7s_t (0x124 bytes at 0xFFF87880, section .pa7_shadow — nm
`_s_pa7`) over meas (6 bytes per request; NOT atomic — the counters tear
between blocks), plus, for comparison, the LIVE cells the shadow must never
write: the PA7 mirror 0xFFF90920 (ours: 0 from init), the C9 bit-7 cell
0xFFF9095A (0), 0x9095B (ours: never written, 0), the J1 C9 frame 0xFFF88426
(C900), the supervisor nibble 0xFFF90958, AN7 0xFFF9019A, the supply
0xFFF94F86 / status 0xFFF94FEC and the range D6D6 / lever D672 / D673.
--padr also reads port A (0xFFFE3800..05; PADR = +2, PA7 = bit 7 of +3).

    witness_pa7.py [rounds] [--raw] [--padr]

Addresses: the r04d build (nm): s_pa7 0xFFF87880. Re-check with sh-elf-nm
before every use.
"""
import struct
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench  # noqa: E402

PA7 = 0xFFF87880
SIZE = 0x124
LIVE = [
    ("PA7 mirror 0x90920", 0xFFF90920, 1), ("C9b7 0x9095A", 0xFFF9095A, 1),
    ("0x9095B", 0xFFF9095B, 1), ("J1 C9 word 0x88426", 0xFFF88426, 2),
    ("nibble 0x90958", 0xFFF90958, 1), ("AN7 0x9019A", 0xFFF9019A, 2),
    ("supply 0x94F86", 0xFFF94F86, 2), ("status 0x94FEC", 0xFFF94FEC, 1),
    ("D6D6", 0xFFF8D6D6, 1), ("D672", 0xFFF8D672, 1), ("D673", 0xFFF8D673, 1),
    ("OS mode 0x93B33", 0xFFF93B33, 1),
]


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
    print("  range part: E567 %02X D712 %02X D713 %02X A6D6E %3u | inputs D6D6 %u D6DA %u lever %u src %u "
          "D65E %u D5F8 %u D5F9 %u D60A %u D65A %d os %u" %
          (d[0], d[1], d[2], d[3], d[0x4C], d[0x4D], d[0x4E], d[0x4F], d[0x50], d[0x51], d[0x52], d[0x53],
           s16(d, 0x54), d[0x56]))
    print("  law: 94122 %02X  SHADOW PA7 %u  SHADOW C9b7 (0x9095A) %u  9095B(model) %02X | 94766 %u 94768 %u" %
          (d[4], d[5], d[6], d[7], u16(d, 8), u16(d, 10)))
    print("  AN7: 93A94 %u 93B2A %u mV | 9417B %02X 9417C %02X | blank 94784 %u 94834 %u | "
          "93B2C (fail) %04X 93B2E (tested) %04X valid %u 9411F %u" %
          (u16(d, 12), u16(d, 14), d[0x13], d[0x14], u16(d, 0x10), d[0x12], u16(d, 0x16), u16(d, 0x18),
           d[0x1A], d[0x15]))
    print("  pin-cut inputs: t %u n5 %u nib %02X sup_st %u os %u AN7 %d sup %d" %
          (u32(d, 0x58), d[0x5C], d[0x5D], d[0x5E], d[0x5F], s16(d, 0x60), s16(d, 0x62)))
    print("  counts: range %u pin %u law %u (PA7 hi %u, C9b7 %u) changes %u reports %u | fault passes 4:%u 8:%u "
          "0x100:%u 0x200:%u | first law at TCNTA %u" %
          (u32(d, 0x1C), u32(d, 0x20), u32(d, 0x24), u32(d, 0x28), u32(d, 0x2C), u32(d, 0x30), u32(d, 0x34),
           u32(d, 0x38), u32(d, 0x3C), u32(d, 0x40), u32(d, 0x44), u32(d, 0x48)))
    n = min(u32(d, 0x30), 16)
    head = d[0x1B]
    if n:
        print("  last %u PA7 / C9b7 changes (oldest first):" % n)
        for k in range(n):
            i = (head - n + k) % 16
            o = 0x64 + 12 * i
            out = d[o + 11]
            print("    t %10u pin pass %5u D6D6 %u lever %u src %u E567 %u D712 %u -> PA7 %u C9b7 %u" %
                  (u32(d, o), u16(d, o + 4), d[o + 6], d[o + 7], d[o + 8], d[o + 9], d[o + 10], out & 1,
                   (out >> 1) & 1))


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
        d = read(b, 1, PA7, SIZE)
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
        print("  LIVE (must be unchanged by the shadow): " + " | ".join(live))
        if "--padr" in sys.argv:
            p = read(b, 1, 0xFFFE3800, 6)
            if p is not None:
                print("  port A 0xFFFE3800..05: %s  (PA7 = %u)" % (p.hex(" "), (p[3] >> 7) & 1))
        time.sleep(1.0)
    b.shutdown()
    print("witness done", flush=True)


if __name__ == "__main__":
    main()
