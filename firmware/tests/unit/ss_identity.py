#!/usr/bin/env python3
"""ss_identity.py - THE BENCH-ONLY SIMULATED-SPEED FRAME LEAVES THE LIVE PATH BIT-IDENTICAL
(ours, 2026-11-20, ~/tcu_sweep/shadow_sim_port.md).

  ss_identity.py <test_g6b_ss | test_g6b_ssleak>

The binary is test_g6b (every gs-check scenario + the monitor negatives, each run compared frame by
frame and tick by tick with the goldens: every bus-A word, the park demands, the cell window
0xFFF80000..0xFFF96000, the end-of-run window byte for byte) built with the shift-map, gear-pair
shadows AND the simulated-speed frame active (SS_TEST: a 0x7F6 frame every 10 ms from the mode-1
switch, output speed a triangle 0 -> 2600 -> 0 rpm; the REAL sensors read zero). It must pass exactly
as test_g6b does, AND its stderr must show the frame was received and used: frames > 0 and valid in
every run, and the shift-map target left 1st (maxtgt > 1) in the runs long enough for the ramp.
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
    runs = re.findall(r"SS run '([^']*)': frames (\d+) valid (\d+) maxtgt (\d+) maxgear (\d+) gp_commit (\d+) "
                      r"gp_end (\d+) sm_walks (\d+)", r.stderr)
    if len(runs) < 9:
        print("FAIL: the sim frame reported %d runs (expected >= 9 scenarios)" % len(runs))
        ok = False
    shifted = 0
    for x in runs:
        name, frames, valid = x[0], int(x[1]), int(x[2])
        if frames == 0 or valid != 1:
            print("FAIL: run '%s': frames %d valid %d" % (name, frames, valid))
            ok = False
        if int(x[3]) > 1 and int(x[5]) > 0:
            shifted += 1
    if runs and shifted == 0:
        print("FAIL: in no run did the shadows shift on the simulated speeds (target / gear-pair commits)")
        ok = False
    if ok:
        print("ss identity: the live path matches the goldens in every run; %d runs, the shadows shifted in %d" %
              (len(runs), shifted))
        for x in runs:
            print("  %-34s frames %6s maxtgt %s maxgear %s gp commits %s ends %s sm walks %s" %
                  (x[0][:34], x[1], x[3], x[4], x[5], x[6], x[7]))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
