#!/usr/bin/env python3
"""gp_identity.py - THE GEAR-PAIR SHADOW LEAVES THE LIVE PATH BIT-IDENTICAL
(ours, 2026-11-20, ~/tcu_sweep/gp_shadow_port.md).

  gp_identity.py <test_g6b_gp | test_g6b_gpleak>

The binary is test_g6b (every gs-check scenario + the monitor negatives, each
run compared frame by frame and tick by tick with the goldens: every bus-A
word, the park demands, the cell window 0xFFF80000..0xFFF96000, the end-of-run
window byte for byte) built with the shift-map shadow AND the gear-pair shadow
ticking at app/main.c's slots (SM_TEST, GP_TEST). It must pass exactly as
test_g6b does, AND its stderr must show the gear-pair shadow ran in every
scenario (10 ms passes > 0), with no table error, no NV call (the would-be
counts are the stub's), and — the garage engine owns the shift object during
the scenarios' engagements — the live-garage mirror seen.
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
    runs = re.findall(r"GP run '([^']*)': passes (\d+) presel (\d+) t0 (\d+) commit (\d+) ps (\d+) done (\d+) "
                      r"end (\d+) gend (\d+) nv_end (\d+) nv_abort (\d+) own (\d+) err (\d+) report (\d+) "
                      r"dis (\d+) e4a7 (-?\d+) e69f (-?\d+) bd15 (\d+) bd16 (\d+) bd17 (\d+) strobes (\d+)",
                      r.stderr)
    if len(runs) < 9:
        print("FAIL: the gear-pair shadow reported %d runs (expected >= 9 scenarios)" % len(runs))
        ok = False
    owned = 0
    for x in runs:
        name, passes, err = x[0], int(x[1]), int(x[12])
        if passes == 0:
            print("FAIL: gear-pair shadow run '%s' did not run" % name)
            ok = False
        if err != 0:
            print("FAIL: gear-pair shadow run '%s': %d table errors" % (name, err))
            ok = False
        owned += int(x[11])
    if runs and owned == 0:
        print("FAIL: the live garage never owned the shift object in any run (the mirror is not fed)")
        ok = False
    if ok:
        print("gp identity: the live path matches the goldens in every run; the shadow ran %d runs" % len(runs))
        for x in runs:
            print("  %-34s passes %6s commits %3s ends %3s gend %3s own %5s nv_end %s nv_abort %s e69f %3s "
                  "bd15 %s bd16 %s bd17 %s" % (x[0][:34], x[1], x[4], x[7], x[8], x[11], x[9], x[10], x[16],
                                                x[17], x[18], x[19]))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
