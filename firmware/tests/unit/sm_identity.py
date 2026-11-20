#!/usr/bin/env python3
"""sm_identity.py - THE SHIFT-MAP SHADOW LEAVES THE LIVE PATH BIT-IDENTICAL
(ours, 2026-11-19, ~/tcu_sweep/sm_shadow_port.md).

  sm_identity.py <test_g6b_norep_sm>

The binary is test_g6b_norep (the image's configuration: the engine the sole
producer, every gs-check scenario + the monitor negatives, each run compared
frame by frame and tick by tick with the r04r goldens — every bus-A word, the
park demands, the cell window 0xFFF80000..0xFFF96000, the end-of-run window
byte for byte) built with the shadow at main.c's 30 ms slot (SM_TEST). It must
pass exactly as test_g6b_norep does, AND its stderr must show the shadow ran
in every scenario: passes > 0, no error, the target 1 (A697 = A0AA = 0x89CE8 =
1: standstill on the bench), the four higher-priority programs skipped and the
confirm stand-in counted on every pass.
"""
import re
import subprocess
import sys


def main():
    r = subprocess.run([sys.argv[1]], capture_output=True, text=True)
    ok = True
    if r.returncode != 0 or "test_g6b: ok" not in r.stdout:
        print("FAIL: the live path differs from the r04r goldens (exit %d)" % r.returncode)
        print("\n".join(l for l in r.stdout.splitlines() if "FAIL" in l or "differ" in l)[:4000])
        ok = False
    runs = re.findall(r"SM run '([^']*)': passes (\d+) walks (\d+) lookups (\d+) err (\d+) a697 (\d+) "
                      r"a0aa (\d+) ce8 (\d+) skip_mode (\d+) arb_skip (\d+) corr (\d+) conf (\d+)", r.stderr)
    if len(runs) < 9:
        print("FAIL: the shadow reported %d runs (expected >= 9 scenarios)" % len(runs))
        ok = False
    for (name, passes, walks, lookups, err, a697, a0aa, ce8, skip, arb, corr, conf) in runs:
        p = int(passes)
        if p == 0 or int(err) != 0 or (a697, a0aa, ce8) != ("1", "1", "1") or int(walks) != p \
                or int(conf) != p or int(arb) != 4 * p or int(lookups) == 0:
            print("FAIL: shadow run '%s': passes %s walks %s lookups %s err %s target %s/%s/%s arb %s conf %s"
                  % (name, passes, walks, lookups, err, a697, a0aa, ce8, arb, conf))
            ok = False
    # (2026-11-19) the gear feedback chain (app/sm_gear.c) ran in every run:
    # passes > 0, no error, no unported branch reached, every strobe completed,
    # the commanded gear = the gear code = the shadow's 0x89C28 at the end, 1 or
    # (a run ending in R or its release) -1, and the shadow agreeing with the
    # live engine's E4A7 / E69F on most passes (they differ only while a
    # power-flow change is pending: the live 0x104A28 stand-in writes E6A1,
    # the stock bytes hold E69F)
    gfr = re.findall(r"GF run '([^']*)': passes (\d+) e4a7 (-?\d+) e69f (-?\d+) c28 (-?\d+) e6a1 (-?\d+) "
                     r"strobes (\d+) done (\d+) agree (\d+) dis (\d+) unp (\d+) err (\d+) neg9 (\d+) "
                     r"gear_in (-?\d+)", r.stderr)
    if len(gfr) != len(runs):
        print("FAIL: the gear chain reported %d runs, the shadow %d" % (len(gfr), len(runs)))
        ok = False
    for (name, passes, e4a7, e69f, c28, e6a1, st, dn, ag, dis, unp, err, neg9, gin) in gfr:
        if int(passes) == 0 or int(err) != 0 or int(unp) != 0 or st != dn or not (e4a7 == e69f == c28) \
                or e4a7 not in ("1", "-1") or int(ag) <= int(dis):
            print("FAIL: gear chain run '%s': passes %s e4a7 %s e69f %s c28 %s strobes %s done %s agree %s "
                  "dis %s unp %s err %s" % (name, passes, e4a7, e69f, c28, st, dn, ag, dis, unp, err))
            ok = False
    if ok:
        print("sm identity: the live path matches the r04r goldens in every run; the shadow ran %d runs "
              "(%s passes in the first)" % (len(runs), runs[0][1]))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
