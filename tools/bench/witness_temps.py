#!/usr/bin/env python3
"""witness_temps.py — the READ-ONLY ECU-temperature shadow
(firmware/app/tsel_shadow.c; ~/tcu_sweep/diag_temp_shadow_port.md).

Reads struct tsel_t (0x148 bytes at 0xFFF87C40, section .tsel_shadow — nm
`_s_tsel`) over meas (6 bytes per request; NOT atomic — the fields tear
between blocks), decodes the die / board / selected temperatures, and reads
for comparison the LIVE inputs (the J3 word / average 0x907CC / 0x908CA, the
J4 word 0x908EE / its copy 0x908CC, the comparator flags 0x9095E — our
misnamed CELL_DIR_BITS, the variant 0x90966, the fluid 0x94F98 / 0x94FEE and
0x94F9C / 0x94FF0) and the LIVE cells — stage 1: 0x9393A / 0x940FC / 0x940FF
stay 0 from init; STAGE 2a (TSEL_LIVE): they carry the shadow's selection
(bsw/drv/drv_tsel.c), and the reply gate 0x9095B (0x0F after boot), the
diagnosis bounds 0x93510 / 0x93524, the tested bits 0x94944, the estimates
0x93394 and the fault words 0x93004 / 0x94930 (ch 0..4) are printed too.

    witness_temps.py [rounds] [--raw]

Addresses: the stage-1 build (nm): s_tsel 0xFFF87C40. Re-check with
sh-elf-nm before every use. `meas 0x02` hygiene first (CLAUDE rule 6).
"""
import struct
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench  # noqa: E402

TSEL = 0xFFF87C40
SIZE = 0x148
INPUTS = [
    ("J3 0x907CC", 0xFFF907CC, 2), ("J3 avg 0x908CA", 0xFFF908CA, 2),
    ("J4 0x908EE", 0xFFF908EE, 2), ("J4 copy 0x908CC", 0xFFF908CC, 2),
    ("cmp 0x9095E", 0xFFF9095E, 1), ("var 0x90966", 0xFFF90966, 1),
    ("fluid 0x94F98", 0xFFF94F98, 2), ("st 0x94FEE", 0xFFF94FEE, 1),
    ("fluid 0x94F9C", 0xFFF94F9C, 2), ("st 0x94FF0", 0xFFF94FF0, 1),
    ("OS mode 0x93B33", 0xFFF93B33, 1),
]
LIVE = [
    ("0x9393A..3F", 0xFFF9393A, 6), ("0x93ABA", 0xFFF93ABA, 2), ("0x940FC..0x94101", 0xFFF940FC, 6),
    ("0x9415E", 0xFFF9415E, 1), ("0x94170", 0xFFF94170, 1),
    ("0x940E0..E8", 0xFFF940E0, 9),
]
# stage 2a (2026-11-18, ~/tcu_sweep/temp_stage2a_port.md): the reply
# processor's gate, and the diagnosis current bounds the live status opens
EXTRA = [
    ("gate 0x9095B", 0xFFF9095B, 1),
    ("UB 0x93510[0..4]", 0xFFF93510, 10), ("LB 0x93524[0..4]", 0xFFF93524, 10),
    ("tested 0x94944[0..4]", 0xFFF94944, 10), ("est 0x93394[0..4]", 0xFFF93394, 10),
    ("fw 0x93004[0..4]", 0xFFF93004, 10), ("latched 0x94930[0..4]", 0xFFF94930, 10),
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


def degc(t16):
    return t16 / 16.0


SRC = {0: "default 120C", 1: "fluid", 2: "die", 4: "board"}


def show(d):
    print("  DIE   0x93ABA %5d (%6.2f C) status %u | N %u (prev %u) flags %04X (prev %04X) conv %d | "
          "hold %u start %u | cmp copy %02X var %u" %
          (s16(d, 8), degc(s16(d, 8)), d[0x11], u16(d, 0), u16(d, 2), u16(d, 4), u16(d, 6), s16(d, 0x0A),
           u16(d, 0x0C), d[0x0E], d[0x0F], d[0x10]))
    print("  BOARD 0x9393E %5d (%6.2f C) status %u | word %04X (prev %04X) flags %04X (prev %04X) | "
          "never %u hold %u | byte %02X" %
          (s16(d, 0x1A), degc(s16(d, 0x1A)), d[0x20], u16(d, 0x12), u16(d, 0x14), u16(d, 0x16), u16(d, 0x18),
           u16(d, 0x1C), u16(d, 0x1E), d[0x21]))
    print("  SEL   0x9393A %5d (%6.2f C) STATUS 0x940FC %u source %u (%s) | bytes sel %02X die %02X | "
          "die copy %d / %u" %
          (s16(d, 0x22), degc(s16(d, 0x22)), d[0x26], d[0x2A], SRC.get(d[0x2A], "?"), d[0x29], d[0x28],
           s16(d, 0x24), d[0x27]))
    print("  REP   die mask %02X code %02X word %04X E1 %02X E2 %02X | board mask %03X code %03X E4 %02X E5 %02X" %
          (u16(d, 0x2E), u16(d, 0x2C), u16(d, 0x34), d[0x36], d[0x37], u16(d, 0x32), u16(d, 0x30), d[0x38],
           d[0x39]))
    print("  COUNTS init %u pass %u die8 %u board8 %u sel8 %u | ff03 %u 8001 %u lowbit %u | changes %u | "
          "reports die %u board %u initrep %u | first board 8 at %u, first sel 8 at %u | sel range %d..%d" %
          (u32(d, 0x3C), u32(d, 0x40), u32(d, 0x44), u32(d, 0x48), u32(d, 0x4C), u32(d, 0x50), u32(d, 0x54),
           u32(d, 0x58), u32(d, 0x5C), u32(d, 0x60), u32(d, 0x64), u32(d, 0x68), u32(d, 0x6C), u32(d, 0x70),
           s16(d, 0x74), s16(d, 0x76)))
    print("  LAST IN t %u N %u word %04X fluid %d st %u cmp %02X var %u n5 %u ready %u os %u" %
          (u32(d, 0x78), u16(d, 0x7C), u16(d, 0x7E), s16(d, 0x80), d[0x82], d[0x83], d[0x84], d[0x85], d[0x86],
           d[0x87]))
    n = min(u32(d, 0x5C), 12)
    head = d[0x2B]
    if n:
        print("  last %u status / source changes (oldest first):" % n)
        for k in range(n):
            i = (head - n + k) % 12
            o = 0x88 + 16 * i
            print("    t %10u pass %5u sel %5d st %u src %u | die st %u N %u | board st %u word %04X" %
                  (u32(d, o), u16(d, o + 4), s16(d, o + 6), d[o + 10], d[o + 11], d[o + 12], u16(d, o + 14),
                   d[o + 13], u16(d, o + 8)))


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
        d = read(b, 1, TSEL, SIZE)
        if d is None:
            print("  read FAIL")
            continue
        if raw:
            for o in range(0, SIZE, 32):
                print("  +%03X %s" % (o, d[o:o + 32].hex(" ")))
        show(d)
        ins = []
        for name, a, n in INPUTS:
            v = read(b, 1, a, n)
            ins.append("%s %s" % (name, v.hex() if v is not None else "??"))
        print("  LIVE INPUTS: " + " | ".join(ins))
        live = []
        for name, a, n in LIVE:
            v = read(b, 1, a, n)
            live.append("%s %s" % (name, v.hex() if v is not None else "??"))
        print("  LIVE (stage 2a: must equal the shadow's 0x22/0x24/0x1A/0x08/0x26.. fields; "
              "0x940E1/E2/E4/E5 stay the stand-ins): " + " | ".join(live))
        ex = []
        for name, a, n in EXTRA:
            v = read(b, 1, a, n)
            if v is not None and n >= 4:
                ex.append("%s %s" % (name, " ".join("%d" % w for w in struct.unpack(">%dH" % (n // 2), v))))
            else:
                ex.append("%s %s" % (name, v.hex() if v is not None else "??"))
        print("  2a: " + " | ".join(ex))
        time.sleep(1.0)
    b.shutdown()
    print("witness done", flush=True)


if __name__ == "__main__":
    main()
