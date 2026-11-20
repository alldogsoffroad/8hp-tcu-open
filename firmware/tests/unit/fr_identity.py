#!/usr/bin/env python3
"""fr_identity.py - THE FAULT-REACTION SHADOW LEAVES THE LIVE PATH BIT-IDENTICAL
(ours, F1 2026-11-19, ~/tcu_sweep/f1_fr_shadow_port.md).

  fr_identity.py <test_g6b_norep_fr | test_g6b_fr | test_g6b_frleak>

The binary is test_g6b(_norep) (every gs-check scenario + the monitor
negatives, each run compared frame by frame and tick by tick with the
goldens: every bus-A word, the park demands, the cell window 0xFFF80000..
0xFFF96000 — the DSM image / NVM / record windows included — the end-of-run
window byte for byte) built with the shift-map shadow AND the fault-reaction
shadow ticking at app/main.c's slots (SM_TEST, FR_TEST: the raw frames before
vehicle_rx, the 10 ms tick after the monitor). It must pass exactly as
test_g6b does, AND its stderr must show the fault-reaction shadow ran in
every scenario: 10 ms passes > 0, mode passes > 0, raw frames matched, no
unported branch / table error / bad group, the group-109 reports forwarded
when rng_chain made them, and — the D-FR-3 comparator — its U1267 / P0850
verdicts (the generic fault manager) equal rng_chain's per-group model on
EVERY pass (c109dis = c98dis = 0).
"""
import re
import subprocess
import sys


def main():
    r = subprocess.run([sys.argv[1]], capture_output=True, text=True)
    ok = True
    if r.returncode != 0 or "test_g6b: ok" not in r.stdout:
        print("FAIL: the live path differs from the goldens (exit %d)" % r.returncode)
        print("\n".join(l for l in r.stdout.splitlines() if "FAIL" in l or "differ" in l)[:4000])
        ok = False
    runs = re.findall(r"FR run '([^']*)': passes (\d+) mon (\d+) frames (\d+) other (\d+) u11e4 (\d+) "
                      r"u1267 (\d+) p0850 (\d+) u0100 (\d+) u11e3 (\d+) m0 ([0-9A-F]+) m1 ([0-9A-F]+) "
                      r"code (\d+) c43 (-?\d+) cf5 (\d+) ff (-?\d+) c109dis (\d+) c98dis (\d+) "
                      r"d661dis (\d+) tgtdis (\d+) rep109 (\d+) rep98 (\d+) unp (\d+) fferr (\d+) badg (\d+) "
                      r"t3 (\d+) t3gate (\d+) t3code ([0-9A-F]+)",
                      r.stderr)
    if len(runs) < 9:
        print("FAIL: the fault-reaction shadow reported %d runs (expected >= 9 scenarios)" % len(runs))
        ok = False
    for (name, passes, mon, frames, other, u11e4, u1267, p0850, u0100, u11e3, m0, m1, code, c43, cf5, ff,
         c109, c98, d661, tgt, rep109, rep98, unp, fferr, badg, t3, t3gate, t3code) in runs:
        bad = []
        if int(passes) == 0 or int(mon) == 0:
            bad.append("did not run")
        if int(unp) != 0 or int(fferr) != 0 or int(badg) != 0:
            bad.append("unp %s fferr %s badg %s" % (unp, fferr, badg))
        if int(c109) != 0 or int(c98) != 0:
            bad.append("the U1267 / P0850 comparator disagrees with rng_chain (%s / %s passes)" % (c109, c98))
        if int(t3) == 0:
            bad.append("the TOE32 test (groups 27 / 28) did not run")
        if int(rep109) == 0 or int(rep98) == 0:
            bad.append("no group-109 / -98 report forwarded")
        if bad:
            print("FAIL: fault-reaction shadow run '%s': %s" % (name, "; ".join(bad)))
            ok = False
    if ok:
        first = runs[0]
        print("fr identity: the live path matches the goldens in every run; the shadow ran %d runs, "
              "U1267 / P0850 agree with rng_chain on every pass (first run '%s': masks %s / %s, code %s, "
              "0x89C43 %s, fault-fed target %s)" % (len(runs), first[0], first[10], first[11], first[12],
                                                    first[13], first[15]))
        for x in runs:
            print("  %-34s u11e4 %5s u1267 %5s p0850 %5s u0100 %5s u11e3 %5s m %s/%s code %3s c43 %2s cf5 %s ff %2s "
                  "tgtdis %s" % (x[0][:34], x[5], x[6], x[7], x[8], x[9], x[10], x[11], x[12], x[13], x[14],
                                 x[15], x[19]))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
