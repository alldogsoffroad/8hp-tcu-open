#!/usr/bin/env python3
"""g6s_gold_diff.py — r04s (~/tcu_sweep/r04s_port.md): THE r04r -> r04s
DIFFERENCE OF EVERY test_g6b RUN, confined to the reversal.

argv: <r04r gold dir> <r04r extra gold dir> <r04s gold dir> [--strict]

The r04r goldens: tests/unit/golden/g6r_r04r_test_g6b.tgz (HEAD e606b07's
build) plus, in g6s_r04s_test_g6b.tgz's gold_r04r_extra/, the one run r04s
adds to test_g6b ("R -> D direct", the reversal's mirror) written by the r04r
engine (HEAD 2884c9b) with only the harness's scenario added. The r04s
goldens: g6s_r04s_test_g6b.tgz's gold/, written by this build's test_g6b /
test_g6b_englim (G6C_GOLD_OUT).

r04s ports 0xDC0F8 (E6C1 / E6C2), the E33C+0x12 counter, the start's flag
(0x1066D6 / 0xDDCA2) and its consumers: 0x10D130's +0x1E and 0x10F084
(+0x3A of the class-4 sub-object, read as the third element's fill time
through 0xBA7DC), 0x10E5EE's −100 mbar (R), 0xEC6FE's BD1C / BD1D. They act
only when a reversal's start follows a release within 10 passes (no P
between: a P engages the park and the stock's park latency puts the start
16 passes after the release's end on every run). The pinned sub-objects and
the 0x106698 prologue / epilogue change nothing the goldens see.

Rules: the same runs on both sides; every run byte-identical (every frame's
wire and cell hash, every tick, gsb_t, the windows, the release header, the
end-of-run window) EXCEPT the run "D -> R direct (no P between)", where
  - ticks may differ only in the staged words of D (field 10) and B (11),
    and only from the R start on (not before tick 300);
  - frames may differ only from frame 600 on;
  - gsb_t may differ only in the two engagement timers +0x30 / +0x82;
  - the end-of-run window only in D's request object's fill times (+0x36,
    +0x3C: 0xFFF92716 / 0xFFF9271C, the candidate's objects at 0xFFF925C0);
  - and it MUST differ (the port must act there).
--strict allows nothing (the negative: must FAIL).
"""
import glob
import os
import sys

REV = "D____R_direct__no_P_between_"
REV_FIELDS = {10, 11}
REV_TICK0, REV_FRAME0 = 300, 600
REV_GSB = {0x30, 0x31, 0x82, 0x83}
REV_WIN = {0xFFF92716, 0xFFF92717, 0xFFF9271C, 0xFFF9271D}


def load(d, tag):
    with open(os.path.join(d, tag + ".txt")) as f:
        lines = [x.rstrip("\n") for x in f]
    with open(os.path.join(d, tag + ".win"), "rb") as f:
        win = f.read()
    return lines, win


def check(tag, a_dir, b_dir, strict):
    la, wa = load(a_dir, tag)
    lb, wb = load(b_dir, tag)
    if la == lb and wa == wb:
        return [], "identical"
    if strict or tag != REV:
        return ["differs (%s)" % ("strict" if strict else "not the reversal run")], ""
    bad = []
    if len(la) != len(lb):
        return ["line counts %u vs %u" % (len(la), len(lb))], ""
    ticks = frames = 0
    for x, y in zip(la, lb):
        if x == y:
            continue
        p, q = x.split(), y.split()
        if p[0] == "T":
            ticks += 1
            f = {i for i in range(len(p)) if p[i] != q[i]}
            if not f <= REV_FIELDS or int(p[1]) < REV_TICK0:
                bad.append("tick %s: fields %s (allowed %s from tick %u)" % (p[1], sorted(f), sorted(REV_FIELDS),
                                                                           REV_TICK0))
        elif p[0] == "F":
            frames += 1
            if int(p[1]) < REV_FRAME0:
                bad.append("frame %s differs (allowed from %u)" % (p[1], REV_FRAME0))
        elif p[0] == "B":
            a, b = bytes.fromhex(p[1]), bytes.fromhex(q[1])
            d = {i for i in range(len(a)) if a[i] != b[i]}
            if len(a) != len(b) or not d <= REV_GSB:
                bad.append("gsb_t bytes %s (allowed %s)" % (sorted(hex(i) for i in d), sorted(hex(i) for i in REV_GSB)))
        else:
            bad.append("line differs: %r vs %r" % (x[:80], y[:80]))
    d = {0xFFF80000 + i for i in range(min(len(wa), len(wb))) if wa[i] != wb[i]}
    if len(wa) != len(wb) or not d <= REV_WIN:
        bad.append("end window bytes %s" % sorted(hex(i) for i in d))
    if ticks == 0:
        bad.append("the reversal run's ticks did not change (the port must act here)")
    return bad, "ticks %u frames %u window bytes %u" % (ticks, frames, len(d))


def main():
    a_dir, x_dir, b_dir = sys.argv[1], sys.argv[2], sys.argv[3]
    strict = "--strict" in sys.argv[4:]
    names = lambda d: {os.path.basename(p)[:-4]: d for p in glob.glob(os.path.join(d, "*.txt"))}
    ta = names(a_dir)
    ta.update(names(x_dir))
    tb = names(b_dir)
    fails = 0
    if set(ta) != set(tb):
        print("FAIL: the run sets differ: only r04r %s, only r04s %s" %
              (sorted(set(ta) - set(tb)), sorted(set(tb) - set(ta))))
        fails += 1
    for t in sorted(set(ta) & set(tb)):
        bad, info = check(t, ta[t], b_dir, strict)
        if bad:
            fails += 1
            print("FAIL %s:\n  %s" % (t, "\n  ".join(bad)))
        else:
            print("OK   %s (%s)" % (t, info))
    print("g6s_gold_diff: %u runs, %u failing%s" % (len(set(ta) & set(tb)), fails, " (strict)" if strict else ""))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
