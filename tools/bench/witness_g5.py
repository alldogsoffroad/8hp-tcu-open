#!/usr/bin/env python3
"""witness_g5.py — G5b: THE LIVE OS MODE (firmware/safety/os_sel.c;
~/tcu_sweep/g5b_port.md). Read-only, over meas (6 bytes per request; NOT
atomic — fields tear between blocks).

Reads, every round:
  the mode cells   0x848CF (applied, boot copy), 0x93B32 boot state,
                   0x93B33 THE MODE, 0x93B34 pending, 0x93B56 the echo's
                   last-seen mode, 0x93B5B..5E the request chain (cur, result,
                   supervisor request, 0x93B5E = the selector's hand-off)
  the slot-26 flags 0x89C3C / 3D / 3E and 0x89C53 / 54 / 55
  the selector object 0x20 B at 0xFFF95F7C (the stock 0xFFF9547C, relocated
                   into r04r's retired pad): +4 engine (E90E), +6 status,
                   +7 REQUEST (0x95483), +8 applied echo (0x95484), +9 the
                   0x9584A stand-in, +A E9FF read, +B turbine status, +C
                   turbine, +E supply, +10 passes, +14 passes asking 2, +18
                   TCNTA of the first echo of 2, +1C changes, +1E last
                   hand-off, +1F speed term
  E9FF (kl15, 0xFFF8E9FF) and the supply 0x94F86
  the monitor's cross-check gsb_t.m2 (0xFFF87220 + 0x8C, 32 B): its request,
                   its passes and "disagree" (+0xE) — must stay 0

Expected: plain boot (no 0x108): mode 1, request 1, 0x93B5E 1, flags 3D/55,
n2 0. With the engine sim (>= 150 rpm) or a turbine >= 100 rpm from any
moment in mode 1: mode 2 within one 5 ms batch of the requesting 10 ms task,
flags 3E/53, LATCHED until power-off / a key-off (E9FF 4). Disagree 0.

Addresses: re-check with sh-elf-nm build/sh2a/tcu_app.elf before every use
(_tcu_rq_retired_pad must be 0xFFF95F7C, _s_gsb 0xFFF87220). `meas 0x02`
hygiene first (CLAUDE rule 6).

    witness_g5.py [rounds]
"""
import struct
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench  # noqa: E402

SEL = 0xFFF95F7C
M2 = 0xFFF87220 + 0x8C
CELLS = [("848CF app", 0xFFF848CF), ("93B32 state", 0xFFF93B32), ("93B33 MODE", 0xFFF93B33),
         ("93B34 pend", 0xFFF93B34), ("93B56 echo-last", 0xFFF93B56), ("93B5B cur", 0xFFF93B5B),
         ("93B5C res", 0xFFF93B5C), ("93B5D sup", 0xFFF93B5D), ("93B5E handoff", 0xFFF93B5E)]
FLAGS = [("3C", 0xFFF89C3C), ("3D", 0xFFF89C3D), ("3E", 0xFFF89C3E), ("53", 0xFFF89C53),
         ("54", 0xFFF89C54), ("55", 0xFFF89C55)]


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


def byte(b, a):
    v = read(b, 1, a, 1)
    return v[0] if v else None


def show_round(b):
    cells = ["%s %s" % (n, "%02X" % v if v is not None else "??") for n, v in
             ((n, byte(b, a)) for n, a in CELLS)]
    print("  MODE  " + " | ".join(cells))
    fl = ["%s %s" % (n, "%u" % v if v is not None else "?") for n, v in ((n, byte(b, a)) for n, a in FLAGS)]
    print("  FLAGS 0x89C" + " ".join(fl))
    s = read(b, 1, SEL, 0x20)
    if s is None:
        print("  selector: read FAIL")
    else:
        print("  SEL   tbl %08X | eng %u st %u | REQUEST %u echo %u | 9584A %u E9FF %u | tur %u st %u | supply %u "
              "| passes %u asking-2 %u | first-2 TCNTA %u | changes %u | last hand-off %s | speed term %u"
              % (u32(s, 0), u16(s, 4), s[6], s[7], s[8], s[9], s[10], u16(s, 12), s[11], u16(s, 14),
                 u32(s, 16), u32(s, 20), u32(s, 24), u16(s, 28), "none" if s[30] == 0xFF else "%u" % s[30],
                 s[31]))
    e9 = byte(b, 0xFFF8E9FF)
    sup = read(b, 1, 0xFFF94F86, 2)
    print("  IN    E9FF %s | supply 0x94F86 %s mV" % (e9, u16(sup, 0) if sup else "??"))
    m = read(b, 1, M2, 32)
    if m is None:
        print("  m2: read FAIL")
    else:
        print("  M2 (monitor cross-check) req %u app %u live os %u E9FF %u | passes %u asking-2 %u first2 %u "
              "changes %u | DISAGREE %u" % (m[0], m[1], m[2], m[3], u32(m, 16), u32(m, 20), u32(m, 24),
                                           u32(m, 28), u16(m, 14)))


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 3
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
        show_round(b)
        time.sleep(1.0)


if __name__ == "__main__":
    main()
