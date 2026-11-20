#!/usr/bin/env python3
"""lp_identity.py - THE LINE-PRESSURE / TORQUE-INTERVENTION SHADOWS LEAVE THE LIVE PATH
BIT-IDENTICAL (ours, 2026-11-20, ~/tcu_sweep/lp_ti_shadow_port.md).

  lp_identity.py <test_g6b_lp | test_g6b_lpleak>

The binary is test_g6b (every gs-check scenario + the monitor negatives, each run compared
frame by frame and tick by tick with the goldens: every bus-A word, the park demands, the
cell window 0xFFF80000..0xFFF96000, the end-of-run window byte for byte) built with the
shift-map, gear-pair AND the line-pressure / torque-intervention shadows ticking at
app/main.c's slots (SM_TEST, GP_TEST, LP_TEST). It must pass exactly as test_g6b does, AND
its stderr must show the shadows ran in every scenario (passes > 0, compared with the live
Sys every pass), with no table error.
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
    runs = re.findall(r"LP run '([^']*)': passes (\d+) cmp (\d+) diff (\d+) err (\d+) fill (\d+) sysbin (\d+) "
                      r"res (\d+) sys (\d+) \| TI passes (\d+) arms (\d+)/(\d+)/(\d+) ends (\d+) phases (\d+) err (\d+)",
                      r.stderr)
    if len(runs) < 9:
        print("FAIL: the line-pressure shadow reported %d runs (expected >= 9 scenarios)" % len(runs))
        ok = False
    garage = 0
    for x in runs:
        name, passes, cmp_n, err = x[0], int(x[1]), int(x[2]), int(x[4])
        tpasses, terr = int(x[9]), int(x[15])
        if passes == 0 or cmp_n != passes or tpasses == 0:
            print("FAIL: shadow run '%s' did not run (lp %d, compared %d, ti %d)" % (name, passes, cmp_n, tpasses))
            ok = False
        if err != 0 or terr != 0:
            print("FAIL: shadow run '%s': %d / %d table errors" % (name, err, terr))
            ok = False
        garage += int(x[12])
    if runs and garage == 0:
        print("FAIL: the torque-intervention shadow never armed on a garage engagement in any run")
        ok = False
    if ok:
        print("lp identity: the live path matches the goldens in every run; the shadows ran %d runs" % len(runs))
        for x in runs:
            print("  %-34s lp passes %6s fill %3s | ti arms up/down/garage %s/%s/%s ends %s phases %s" %
                  (x[0][:34], x[1], x[5], x[10], x[11], x[12], x[13], x[14]))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
