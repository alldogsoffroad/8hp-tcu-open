#!/usr/bin/env python3
"""witness_gp.py — the READ-ONLY gear-pair shift-engine shadow
(firmware/app/gp_shadow.c + the coupled gear chain app/gp_gear.c;
~/tcu_sweep/gp_shadow_port.md, decode ~/tcu_sweep/shift_engine_gearpair_decode.md).

Resolves its addresses FROM THE ELF by symbol (sh-elf-nm): __gp_shadow_start
(struct gp_t, 0x188 bytes, section .gp_shadow) and __gp_gear_start (the
coupled chain's gf_t, 0x9C, section .gp_gear). Reads them over meas (6 bytes
per request; NOT atomic — fields tear between blocks) and prints the shadow's
shift object (BD15 / BD16 / BD17 / BD18 / BD1A / BD1E / BD41 …), E33C (the
active / pre-selected element pairs), blocks A and B (from / to / kind / sync /
slip / hold, the on-coming and off-going phase objects), the would-be producer
requests ids 1..4, the witnesses (incl. the NV calls COUNTED, never made:
nv_end = 0xEBB6C's 0xBA538, nv_abort = 0xEBC60's 0xBA52C), the event ring and
the coupled chain (as witness_sm.py prints the r04w chain). The LIVE shift
object (0xFFF8BD15..17, BD41) is read for comparison — the shadow never writes
it.

    witness_gp.py [rounds] [--raw] [--elf PATH]

2026-11-20 (the stand-in retirements, ~/tcu_sweep/gp_standins_port.md) adds gp_x_t (__gp_ext_start, 94 B) and the
four ramp objects (__gp_ramp_start, 64 B). Expected on the dry bench (output 0, turbine 0, no 0x120 / torque frames):
the idle kind machine runs every pass — kind(ctx+7) 1 and class 0/0 on both blocks (0x1180BE's first test: no shift
owned), BD1E 0 (0x89CFA), BD68 0; BD1B 0; the counters grow with the pass count: BD5F / BD51 / BD67 saturate at 255, the
phase objects' +0x1A / +0x12 saturate at 255 (u8), ctx+0x22 and +0xA / +0xC / +0x3E count in u16; BD2E counts the passes since the settle commit (u16; block A is synchronised, 0x11755E p1 -> 0x117F48 p3) and BD30 saturates at 255
k_cyc counts two 0x117B4A calls per pass (A and B), k_start / k_ovr / repin / k_reload 0, k_chg 2 (the init and the settle commit), bd1b_set 0, class1_rows 0, k_a1032 / k_adapt 0 (both evaluated only while a shift is owned);
the ramp objects all 0; in2 pedal 23 (no 0x120 frame), tq 0.

Expected on the dry bench (output 0, turbine 0): plain boot — passes ~100/s
in modes 1..3, one commit (the boot settle 6 -> 1: no direct shift, BD16 0,
BD15 0), every pre-select of type 0 (presel = t0), no commit after the
settle, done 0, end = commit, nv_end 0, nv_abort 0, err 0, BD17 04 (the
dump's), blocks A / B with no elements, the coupled chain's E69F = the r04w
chain's (dis 0 in P). gs-check with sims: the live garage owns the shift
object during each engagement / release (own counting, BD16 3 / 4 mirrored,
gend = the number of garage ends, nv_end 0: our live engine never sets +0x2D),
the coupled chain's R commit 1 -> -1 (type 0) and back; still no gear-pair
shift (the output speed is 0). `meas 0x02` hygiene first (CLAUDE rule 6).
"""
import os
import struct
import subprocess
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench  # noqa: E402
import witness_sm  # noqa: E402

GP_SIZE = 0x188
GF_SIZE = 0x9C
GX_SIZE = 94        # gp_x_t, section .gp_ext (__gp_ext_start): the kind / class state, the counters' extensions
GR_SIZE = 64        # 4 x gp_ramp_t (16 B), section .gp_ramp (__gp_ramp_start): [block A on, A off, B on, B off]
XW = ["k_start", "k_cyc", "k_chg", "k_ovr", "k_reload(NOT PORTED)", "k_a1032(stand-in 0)", "k_adapt(pass-through)",
      "c_pass", "repin", "ramp", "bd1b_set", "class1_rows"]
WIT = ["init", "pass", "skip_mode", "presel", "presel_t0", "presel_busy", "commit", "commit_ps", "mismatch",
       "done", "end", "gend", "nv_end(NOT CALLED)", "nv_abort(NOT CALLED)", "si_phase", "si_appl", "si_misc",
       "unp", "report", "err", "own", "block", "dis", "us_last", "us_max", "eb1e8"]
EV = {1: "PRESEL", 2: "COMMIT", 3: "SWAP", 4: "DONE", 5: "END", 6: "PH_ON", 7: "PH_OFF", 8: "GEND"}
LIVE = [("live BD15..17", 0xFFF8BD15, 3), ("live BD41", 0xFFF8BD41, 1), ("live E69F", 0xFFF8E69F, 1),
        ("live E69B", 0xFFF8E69B, 1), ("OS mode", 0xFFF93B33, 1)]


def elf_syms(elf):
    out = subprocess.run(["sh-elf-nm", elf], capture_output=True, text=True, check=True).stdout
    s = {}
    for ln in out.splitlines():
        p = ln.split()
        if len(p) == 3:
            s[p[2]] = int(p[0], 16)
    return s


def sb(d, o):
    return struct.unpack("b", d[o:o + 1])[0]


def u16(d, o):
    return struct.unpack(">H", d[o:o + 2])[0]


def s16(d, o):
    return struct.unpack(">h", d[o:o + 2])[0]


def u32(d, o):
    return struct.unpack(">I", d[o:o + 4])[0]


def po(d, o):
    return "elem %u ph %02X step %u type %u f7 %u f4 %u mode %u tin %u out %u n %u" % (
        d[o], d[o + 1], d[o + 2], d[o + 3], d[o + 4], d[o + 5], d[o + 6], d[o + 7], u16(d, o + 8), u16(d, o + 10))


def xblk(name, d, o):
    print("  %s kind(ctx+7) %u f8(ctx+8) %u class %u/%u (call-site flag 0/1) ov(ctx+0x30) %u | on +0x10 %u +0x1C %u "
          "+0xA %u +0xC %u +0x3E %u +0x34(old) %u | off +0x2C %u +0x22(old) %u | ctx+0x22 %u | on ptr ctx %u pairs %s "
          "| off ptr ctx %u pairs %s" %
          (name, d[o], d[o + 1], d[o + 2], d[o + 3], d[o + 4], d[o + 5], d[o + 6], u16(d, o + 12), u16(d, o + 14),
           u16(d, o + 16), u16(d, o + 10), d[o + 7], u16(d, o + 18), u16(d, o + 8), d[o + 20], list(d[o + 21:o + 24]),
           d[o + 24], list(d[o + 25:o + 28])))


def show_x(d, r):
    xblk("A", d, 0)
    xblk("B", d, 28)
    print("  X     in2 tq %d pedal %u now_ms %u | BD2E %u BD30 %u BD5F %u BD51 %u | 0x89C1C %u 0x89C0E %u" %
          (s16(d, 56), d[58], u16(d, 60), u16(d, 62), d[64], d[65], d[66], d[67], d[68]))
    print("  XW    " + " ".join("%s %u" % (n, u16(d, 70 + 2 * i)) for i, n in enumerate(XW)))
    if r is not None:
        for i, nm in enumerate(["A on", "A off", "B on", "B off"]):
            o = 16 * i
            v, ts, mn, mx = struct.unpack(">fHxxff", r[o:o + 16])
            print("  RAMP  %-5s v %g ts %u min %g max %g" % (nm, v, ts, mn, mx))


def blk(name, d, o):
    print("  %s sync %u to %d from %d act %u f4 %u kind %u dir %02X hold %u p27 %u x1e %u slip %d" %
          (name, d[o], sb(d, o + 1), sb(d, o + 2), d[o + 3], d[o + 4], d[o + 5], d[o + 6], d[o + 7], d[o + 8],
           d[o + 9], s16(d, o + 10)))
    print("        on  " + po(d, o + 12))
    print("        off " + po(d, o + 24))


def show(d):
    print("  BD14  BD15 %u BD16 %u BD17 %u BD18 %u BD1A %u BD1B %u BD1E %u BD22 %02X BD3C %u BD40 %u BD41 %u "
          "BD45 %u BD48 %u BD49 %u +0x49 %u +0x53 %02X +0x54 %u | +0x24 %u" %
          tuple(list(d[0:17]) + [u16(d, 18)]))
    print("  E33C  +0 %u +1 %u (ids 1/2) | +5 %u +6 %u (ids 3/4) | +0x14 %u | own %u | +0xA %d +0x10 %d" %
          (d[20], d[21], d[22], d[23], d[24], d[25], s16(d, 26), s16(d, 28)))
    blk("A", d, 0x20)
    blk("B", d, 0x44)
    for i in range(4):
        o = 0x68 + 8 * i
        print("  PROD id %u elem %u phase %02X mode %u value %u passes %u (would-be, NOT applied)" %
              (i + 1, d[o], d[o + 1], d[o + 2], u16(d, o + 4), u16(d, o + 6)))
    print("  WIT   " + " ".join("%s %u" % (n, u32(d, 0xA0 + 4 * i)) for i, n in enumerate(WIT)))
    o = 0x88
    print("  IN    E69B %u E6EE %u E6EF %u E28E %u c3d %u os %u CD2 %u CB8 %u CBA %u CD0 %u | live BD15 %u BD16 %u "
          "BD17 %u BD41 %u D151 %u D0E0 %u | r04w E4A7 %d E69F %d" %
          (d[o], d[o + 1], d[o + 2], d[o + 3], d[o + 4], d[o + 5], d[o + 6], u16(d, o + 10), u16(d, o + 12),
           u16(d, o + 14), d[o + 16], d[o + 17], d[o + 18], d[o + 19], d[o + 20], d[o + 21], sb(d, o + 22),
           sb(d, o + 23)))
    head = d[0x1E]
    print("  events (ring of 16, oldest first):")
    for k in range(16):
        i = (head + k) % 16
        q = 0x108 + 8 * i
        if d[q + 2] == 0:
            continue
        print("    pass %5u %-7s a %d b %d c %u d %u e %u" %
              (u16(d, q), EV.get(d[q + 2], "?%u" % d[q + 2]), sb(d, q + 3), sb(d, q + 4), d[q + 5], d[q + 6],
               d[q + 7]))


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 3
    raw = "--raw" in sys.argv
    elf = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../build/sh2a/tcu_app.elf")
    if "--elf" in sys.argv:
        elf = sys.argv[sys.argv.index("--elf") + 1]
    syms = elf_syms(elf)
    gp_a = syms["__gp_shadow_start"]
    gf_a = syms["__gp_gear_start"]
    gx_a = syms["__gp_ext_start"]
    gr_a = syms["__gp_ramp_start"]
    print("ELF %s: gp_t @ %08X, the coupled gf_t @ %08X, gp_x_t @ %08X, the ramp objects @ %08X" %
          (elf, gp_a, gf_a, gx_a, gr_a))
    b = tcu_bench.Buses(witness_sm.A())
    h = None
    for _ in range(10):
        h = tcu_bench.meas(b, 1, [0x02], timeout=1.5)
        if h:
            break
        time.sleep(0.4)
    print("hyg 0x02:", h.hex() if h else None, flush=True)
    for r in range(rounds):
        print("--- round %d %s" % (r, time.strftime("%H:%M:%S")), flush=True)
        d = witness_sm.read(b, 1, gp_a, GP_SIZE)
        if d is None:
            print("  read FAIL")
            continue
        if raw:
            for o in range(0, GP_SIZE, 32):
                print("  +%03X %s" % (o, d[o:o + 32].hex(" ")))
        show(d)
        x = witness_sm.read(b, 1, gx_a, GX_SIZE)
        rr = witness_sm.read(b, 1, gr_a, GR_SIZE)
        if x is None:
            print("  gp_x_t read FAIL")
        else:
            show_x(x, rr)
        g = witness_sm.read(b, 1, gf_a, GF_SIZE)
        if g is None:
            print("  coupled chain read FAIL")
        else:
            print("  -- the coupled gear chain (.gp_gear):")
            witness_sm.show_gf(g)
        live = []
        for name, a, n in LIVE:
            v = witness_sm.read(b, 1, a, n)
            live.append("%s %s" % (name, v.hex() if v is not None else "??"))
        print("  LIVE: " + " | ".join(live))
        time.sleep(1.0)


if __name__ == "__main__":
    main()
