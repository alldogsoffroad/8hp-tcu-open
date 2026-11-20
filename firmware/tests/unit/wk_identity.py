#!/usr/bin/env python3
"""wk_identity.py - THE LOCK-UP (WK) SHADOW LEAVES THE LIVE PATH BIT-IDENTICAL (ours, 2026-11-20,
~/tcu_sweep/wk_shadow_port.md).

  wk_identity.py <test_g6b_wk | test_g6b_wkleak>

The binary is test_g6b (every gs-check scenario + the monitor negatives, each run compared frame by frame and
tick by tick with the goldens: every bus-A word, the park demands, the cell window 0xFFF80000..0xFFF96000, the
end-of-run window byte for byte) built with the shift-map, gear-pair, line-pressure / torque-intervention AND
the lock-up shadows ticking at app/main.c's slots (SM_TEST, GP_TEST, LP_TEST, WK_TEST). It must pass exactly as
test_g6b does, AND its stderr must show the lock-up shadow ran in every scenario (passes > 0, compared with the
live 500 every pass) with no table error and NEVER permitted on these dry-bench inputs (no pass left state 1).
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
    runs = re.findall(r"WK run '([^']*)': passes (\d+) reset (\d+) gate (\d+) cmp (\d+) diff (\d+) perm (\d+) err (\d+) "
                      r"state (\d+) wk (\d+)", r.stderr)
    if len(runs) < 9:
        print("FAIL: the lock-up shadow reported %d runs (expected >= 9 scenarios)" % len(runs))
        ok = False
    for x in runs:
        name, passes, cmp_n, diff, perm, err = x[0], int(x[1]), int(x[4]), int(x[5]), int(x[6]), int(x[7])
        if passes == 0 or cmp_n != passes:
            print("FAIL: shadow run '%s' did not run (passes %d, compared %d)" % (name, passes, cmp_n))
            ok = False
        if err != 0:
            print("FAIL: shadow run '%s': %d table errors" % (name, err))
            ok = False
        if perm != 0 or diff != 0:
            print("FAIL: shadow run '%s': the shadow left state 1 (%d passes) / differed from the live 500 (%d)" % (name, perm, diff))
            ok = False
    if ok:
        print("wk identity: the live path matches the goldens in every run; the lock-up shadow ran %d runs, never permitted" % len(runs))
        for x in runs:
            print("  %-34s passes %6s reset %6s gate %5s state %s wk %s" % (x[0][:34], x[1], x[2], x[3], x[8], x[9]))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
