#!/usr/bin/env python3
"""g6q_gold_diff.py — r04q (D-G6-13/14, ~/tcu_sweep/r04q_port.md): THE r04p
-> r04q DIFFERENCE OF EVERY test_g6b RUN, confined.

argv: <r04p gold dir> <r04q gold dir> [--strict] [--skip-tag TAG]...

The r04p goldens are the r04o ones (tests/unit/golden/g6c_r04o_test_g6b.tgz:
r04p's G6c build compared identical to them except the deleted shadow's
request objects 0xFFF911A0..0xFFF91440, never hashed). The r04q goldens are
written by this build's test_g6b (G6C_GOLD_OUT). For every run present in
both, the ONLY differences allowed are:

  1. THE BOOT WINDOW (D-G6-13): the ticks of r04p's initial E28E fence from
     the first tick the r04q seam carries the candidate (tb) to the fence's
     end (te): the staged word of channel A only (every other channel's
     word identical), the seam source 0 -> 1 and the fence flag 1 -> 0;
     before tb the fence flag 1 -> 0 only (the seam empty in both: r03f);
     wire-hash frames only in [frame(tb) - 2, frame(te) + 2]; cell-window
     hash frames only in [frame(tb) - 2, frame(te) + 300] (A's filtered
     feedback duty 0xFFF93420 and the timers of the candidate's A request
     object settle).
  2. THE STALL FENCE'S SOURCE (D-G6-14): a tick where r04p's fence holds the
     seam (source 0, fence 1) after te may read source 3 (the P content) in
     r04q — with the staged words identical.
  3. gsb_t: the witness counters the two changes move (applied/take/agree/
     disagree/last_dis/agree_run/cand/live passes, fence entries / exits /
     first exit, and in a stall run the end-of-run src / applied / app[]);
     r04q's 12 new bytes (the boot allowance, fence_n) are r04q's own.
  4. The end-of-run window: the candidate's A request object
     (0xFFF925C0..0xFFF9261F: its timers / time-in-state / modelled pressure,
     one pass later) and A's filtered duty 0xFFF93420..21.
Everything else — the latch, the trip counts, the monitor passes, the
lever, every other tick field, gsb_win_t, the release header, the tick
count — must be identical. --strict allows nothing (the negative: must
FAIL). --skip-tag: a run whose in-repo reference is not r04p's — the r04o
tarball's "candidate != shadow" negative tripped r04o's deleted shadow check
(r04p's run did not trip: G6c); against r04p's own goldens it compares OK.
"""
import glob
import os
import sys

GSB_FIELDS = [  # (offset, size, name) of gsb_t's first 212 bytes (r04p's layout)
    (0, 4, "passes"), (4, 4, "armed_n"), (8, 4, "applied_n"), (12, 4, "take_n"), (16, 4, "agree_n"),
    (20, 4, "disagree_n"), (24, 4, "last_dis"), (28, 4, "sh_commits"), (32, 4, "sh_done"), (36, 2, "arm_ev"),
    (38, 2, "disarm_ev"), (40, 2, "guard_n"), (42, 2, "guard_why"), (44, 2, "guard_v"), (46, 2, "why"),
    (48, 2, "eng_t"), (50, 2, "why_armed_or"), (52, 1, "guard_ch"), (53, 1, "armed"), (54, 1, "applied"),
    (55, 1, "agree_run"), (56, 1, "pf"), (57, 1, "eng_on"), (58, 1, "slot_cur"), (59, 1, "back_pend"),
    (60, 10, "sh"), (70, 10, "lv"), (80, 10, "cur"), (90, 10, "app"), (100, 40, "slot"), (140, 32, "m2"),
    (172, 1, "src"), (173, 1, "fence"), (174, 1, "owns"), (175, 1, "nonp"), (176, 1, "shd_run"),
    (177, 1, "trip_ch"), (178, 2, "trip_why"), (180, 4, "trip_pass"), (184, 4, "cand_n"), (188, 4, "live_n"),
    (192, 4, "pc_n"), (196, 4, "shd_n"), (200, 2, "fence_ev"), (202, 2, "unfence_n"), (204, 4, "unfence_pass"),
    (208, 2, "trip_v"), (210, 2, "trip_n"),
]
GSB_ALLOWED = {"applied_n", "take_n", "agree_n", "disagree_n", "last_dis", "agree_run", "cand_n", "live_n",
               "fence_ev", "unfence_n", "unfence_pass"}
GSB_ALLOWED_STALL = {"src", "applied", "app", "lv", "cur"}
WIN_ALLOWED = [(0xFFF925C0, 0xFFF92620), (0xFFF93420, 0xFFF93422)]
SH_REQ = (0xFFF911A0, 0xFFF91440)       # the deleted shadow's request objects (never compared)


def load(d, tag):
    F, T, X = {}, {}, {}
    with open(os.path.join(d, tag + ".txt")) as f:
        for line in f:
            p = line.split()
            if not p:
                continue
            if p[0] == "F":
                F[int(p[1])] = (p[2], p[3])
            elif p[0] == "T":
                T[int(p[1])] = [int(x) for x in p[2:9]] + [int(x, 16) for x in p[9:14]]
            elif p[0] in ("B", "W", "R", "END"):
                X[p[0]] = " ".join(p[1:])
    with open(os.path.join(d, tag + ".win"), "rb") as f:
        W = f.read()
    return F, T, X, W


def field_of(off):
    for o, n, name in GSB_FIELDS:
        if o <= off < o + n:
            return name
    return "?"


def check(tag, a_dir, b_dir, strict, out):
    Fa, Ta, Xa, Wa = load(a_dir, tag)
    Fb, Tb, Xb, Wb = load(b_dir, tag)
    bad = []
    if sorted(Ta) != sorted(Tb):
        bad.append("tick count %u vs %u" % (len(Ta), len(Tb)))
        return bad
    n = len(Ta)
    # r04p's initial fence: ticks 0 .. te (fence 1 from the start)
    te = -1
    for i in range(n):
        if Ta[i][4] == 1:
            te = i
        else:
            break
    tb = None
    for i in range(n):
        if Tb[i][2] == 1:
            tb = i
            break
    stall = False
    words = []
    for i in range(n):
        a, b = Ta[i], Tb[i]
        # frame bpass src latch fence trip lev | st0..st4
        for j, name in ((0, "frame"), (1, "bpass"), (3, "latch"), (5, "trip_n"), (6, "lev")):
            if a[j] != b[j]:
                bad.append("tick %u: %s %u vs %u" % (i, name, a[j], b[j]))
        same_words = a[7:] == b[7:]
        in_bw = (not strict) and tb is not None and tb <= i <= te
        pre_bw = (not strict) and (tb is None or i < tb) and i <= te
        if not same_words:
            if in_bw and a[8:] == b[8:]:
                words.append((i, a[0], a[1], a[7], b[7]))
            else:
                bad.append("tick %u: staged words %s vs %s" % (i, " ".join("%04X" % x for x in a[7:]),
                                                                " ".join("%04X" % x for x in b[7:])))
        if (a[2], a[4]) != (b[2], b[4]):
            ok = False
            if pre_bw and (a[2], a[4], b[2], b[4]) == (0, 1, 0, 0):
                ok = True
            elif in_bw and (a[2], a[4], b[2], b[4]) == (0, 1, 1, 0):
                ok = True
            elif (not strict) and i > te and (a[2], a[4], b[2], b[4]) == (0, 1, 3, 1) and same_words:
                ok = True
                stall = True
            if not ok:
                bad.append("tick %u: src/fence %u/%u vs %u/%u" % (i, a[2], a[4], b[2], b[4]))
    if tb is None:
        bad.append("r04q never carried the candidate")
        return bad
    f_lo = Ta[tb][0] - 2
    f_hi_w = Ta[te][0] + 2 if te >= 0 else -1
    f_hi_c = Ta[te][0] + 300 if te >= 0 else -1
    wf, cf = [], []
    for k in sorted(Fa):
        if Fa[k][0] != Fb.get(k, ("", ""))[0]:
            wf.append(k)
            if strict or not (f_lo <= k + 1 <= f_hi_w):
                bad.append("wire hash frame F%u outside the boot window" % (k + 1))
        if Fa[k][1] != Fb.get(k, ("", ""))[1]:
            cf.append(k)
            if strict or not (f_lo <= k + 1 <= f_hi_c):
                bad.append("cell hash frame F%u outside the boot window + settle" % (k + 1))
    ba, bb = bytes.fromhex(Xa["B"]), bytes.fromhex(Xb["B"])
    gd = sorted({field_of(i) for i in range(len(ba)) if i < len(bb) and ba[i] != bb[i]})
    allow = set() if strict else (GSB_ALLOWED | (GSB_ALLOWED_STALL if stall else set()))
    for name in gd:
        if name not in allow:
            bad.append("gsb_t field %s differs" % name)
    for key in ("W", "R", "END"):
        if Xa.get(key) != Xb.get(key):
            bad.append("%s differs" % key)
    wd = []
    for i in range(min(len(Wa), len(Wb))):
        addr = 0xFFF80000 + i
        if SH_REQ[0] <= addr < SH_REQ[1]:
            continue
        if Wa[i] != Wb[i]:
            wd.append(addr)
            if strict or not any(lo <= addr < hi for lo, hi in WIN_ALLOWED):
                bad.append("end window 0x%08X differs" % addr)
    out.append("RUN %s: boot window ticks %u..%u (frames F%u..F%u); channel A words differing in ticks %s; "
               "wire frames %s; cell frames %u (F%u..F%u); gsb_t fields %s; end window bytes %s%s => %s" % (
                   tag, tb, te, Ta[tb][0], Ta[te][0] if te >= 0 else 0,
                   ", ".join("%u (F%u pass %u: r04p %04X r04q %04X)" % w for w in words) or "none",
                   [k + 1 for k in wf], len(cf), (cf[0] + 1) if cf else 0, (cf[-1] + 1) if cf else 0,
                   ",".join(gd) or "none", " ".join("%08X" % x for x in wd) or "none",
                   " | stall-fence source P content" if stall else "", "OK" if not bad else "FAIL"))
    return bad


def main():
    a_dir, b_dir = sys.argv[1], sys.argv[2]
    strict = "--strict" in sys.argv[3:]
    skip = [sys.argv[i + 1] for i in range(3, len(sys.argv) - 1) if sys.argv[i] == "--skip-tag"]
    out, fails, n = [], 0, 0
    for f in sorted(glob.glob(os.path.join(a_dir, "*.txt"))):
        tag = os.path.basename(f)[:-4]
        if not os.path.exists(os.path.join(b_dir, tag + ".txt")):
            continue
        if tag in skip:
            out.append("RUN %s: skipped (its reference golden is not r04p's)" % tag)
            continue
        n += 1
        bad = check(tag, a_dir, b_dir, strict, out)
        if bad:
            fails += 1
            out.append("   FAIL %s: %s" % (tag, "; ".join(bad[:8])))
    new = [os.path.basename(f)[:-4] for f in sorted(glob.glob(os.path.join(b_dir, "*.txt")))
           if not os.path.exists(os.path.join(a_dir, os.path.basename(f)))]
    print("\n".join(out))
    print("r04q-only runs (the new negatives): %s" % (", ".join(new) or "none"))
    print("g6q_gold_diff: %u runs compared, %u with differences outside the allowed set%s" % (
        n, fails, " (strict)" if strict else ""))
    return 1 if (fails or n == 0) else 0


if __name__ == "__main__":
    sys.exit(main())
