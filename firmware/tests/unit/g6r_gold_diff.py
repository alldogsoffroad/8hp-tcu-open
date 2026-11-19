#!/usr/bin/env python3
"""g6r_gold_diff.py — r04r (~/tcu_sweep/r04r_port.md): THE r04q -> r04r
DIFFERENCE OF EVERY test_g6b RUN, confined.

argv: <r04q gold dir> <r04r gold dir> [--strict]

The r04q goldens: tests/unit/golden/g6q_r04q_test_g6b.tgz (HEAD 130793e).
The r04r goldens: tests/unit/golden/g6r_r04r_test_g6b.tgz, written by this
build's test_g6b / test_g6b_englim (G6C_GOLD_OUT) — the harness replaying the
retired live holder (tests/unit/test_gs_bridge_chain.c G6R_HOLDER_REPLICA 1)
so that its stock cells hold what r04q's cell-window hashes saw.

r04r retires the live holder (app/rq_prod.c): the engine supplies WK / Sys
(ch 5 / 6) and the start of the producer content; the monitor's agreement
witness's reference gsb_t.lv becomes the constant P content. For every run
present in both, the ONLY difference allowed is in gsb_t:

  agree_n (+16) and disagree_n (+20) — the witness counters of that
  reference: a pass moves from one to the other, so their SUM must be
  identical (no pass gained or lost).

Everything else must be byte-identical: every frame (the wire hash — every
bus-A word in wire order + the park demands — AND the cell-window hash),
every tick (frame, monitor pass, seam source, latch, fence, trips, lever,
the five staged A..E words), every other gsb_t byte, the .noinit windows
gsb_win_t, the release header, the tick count, the end-of-run window. The
same runs must exist on both sides. --strict allows nothing (the negative:
must FAIL, since agree_n / disagree_n do move).
"""
import glob
import os
import sys

AGREE, DISAGREE = 16, 20


def load(d, tag):
    lines = {}
    other = []
    with open(os.path.join(d, tag + ".txt")) as f:
        for line in f:
            p = line.rstrip("\n")
            if p.startswith("B "):
                lines["B"] = bytes.fromhex(p[2:].strip())
            else:
                other.append(p)
    with open(os.path.join(d, tag + ".win"), "rb") as f:
        W = f.read()
    return lines["B"], other, W


def u32le(b, o):
    return int.from_bytes(b[o:o + 4], "little")


def check(tag, a_dir, b_dir, strict):
    Ba, Oa, Wa = load(a_dir, tag)
    Bb, Ob, Wb = load(b_dir, tag)
    bad = []
    if Oa != Ob:
        n = sum(1 for x, y in zip(Oa, Ob) if x != y) + abs(len(Oa) - len(Ob))
        first = next((i for i, (x, y) in enumerate(zip(Oa, Ob)) if x != y), min(len(Oa), len(Ob)))
        bad.append("%u F/T/W/R/END lines differ (first: %r vs %r)" %
                   (n, Oa[first] if first < len(Oa) else None, Ob[first] if first < len(Ob) else None))
    if Wa != Wb:
        diff = [i for i in range(min(len(Wa), len(Wb))) if Wa[i] != Wb[i]]
        bad.append("end-of-run window: %u bytes differ (first 0x%08X)" %
                   (len(diff), 0xFFF80000 + diff[0] if diff else 0))
    if len(Ba) != len(Bb):
        bad.append("gsb_t size %u vs %u" % (len(Ba), len(Bb)))
        return bad, 0
    dbytes = [i for i in range(len(Ba)) if Ba[i] != Bb[i]]
    moved = 0
    for i in dbytes:
        if strict or not (AGREE <= i < AGREE + 4 or DISAGREE <= i < DISAGREE + 4):
            bad.append("gsb_t +0x%02X differs (%02X vs %02X)" % (i, Ba[i], Bb[i]))
    sa = u32le(Ba, AGREE) + u32le(Ba, DISAGREE)
    sb = u32le(Bb, AGREE) + u32le(Bb, DISAGREE)
    if sa != sb:
        bad.append("agree_n + disagree_n %u vs %u (a pass gained or lost)" % (sa, sb))
    moved = u32le(Bb, DISAGREE) - u32le(Ba, DISAGREE)
    return bad, moved


def main():
    a_dir, b_dir = sys.argv[1], sys.argv[2]
    strict = "--strict" in sys.argv[3:]
    ta = sorted(os.path.basename(p)[:-4] for p in glob.glob(os.path.join(a_dir, "*.txt")))
    tb = sorted(os.path.basename(p)[:-4] for p in glob.glob(os.path.join(b_dir, "*.txt")))
    fails = 0
    if ta != tb:
        print("FAIL: the run sets differ: only r04q %s, only r04r %s" %
              (sorted(set(ta) - set(tb)), sorted(set(tb) - set(ta))))
        fails += 1
    for t in sorted(set(ta) & set(tb)):
        bad, moved = check(t, a_dir, b_dir, strict)
        if bad:
            fails += 1
            print("FAIL %s:\n  %s" % (t, "\n  ".join(bad)))
        else:
            print("OK   %s (passes moved agree -> disagree: %d)" % (t, moved))
    print("g6r_gold_diff: %u runs, %u failing%s" % (len(set(ta) & set(tb)), fails, " (strict)" if strict else ""))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
