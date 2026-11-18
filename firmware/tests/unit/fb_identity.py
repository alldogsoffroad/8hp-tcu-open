#!/usr/bin/env python3
"""fb_identity.py - THE FEEDBACK SHADOW LEAVES THE LIVE PATH BIT-IDENTICAL
(ours, 2026-11-18, ~/tcu_sweep/feedback_shadow_port.md).

  fb_identity.py <test_odiag> <test_odiag_fbs>

Runs both with --full (tests/unit/test_odiag.c: parts 1-4, an exact hash of
the whole cell window + every bus-A word + the cadence witnesses per output
tick) and requires the two stdouts byte-identical, both runs PASS, and the
shadow build's stderr to show the shadow ran (sampler ISRs, windows, passes,
every channel compared).
"""
import re
import subprocess
import sys


def main():
    a = subprocess.run([sys.argv[1], "--full"], capture_output=True, text=True)
    b = subprocess.run([sys.argv[2], "--full"], capture_output=True, text=True)
    la, lb = a.stdout.splitlines(), b.stdout.splitlines()
    ticks = sum(1 for x in la if x.startswith("F "))
    bad = [i for i in range(min(len(la), len(lb))) if la[i] != lb[i]]
    ok = True
    if a.returncode != 0 or b.returncode != 0:
        print("FAIL: exit codes %d / %d" % (a.returncode, b.returncode))
        ok = False
    if len(la) != len(lb) or bad:
        print("FAIL: outputs differ (%d / %d lines, %d differ); first: %s" %
              (len(la), len(lb), len(bad), bad[:3]))
        for i in bad[:3]:
            print("  without: %s\n  with:    %s" % (la[i], lb[i]))
        ok = False
    m = re.search(r"fbs isr (\d+) windows (\d+)/(\d+) ticks (\d+) r_upd (\d+) pair_bad (\d+) "
                  r"channels-compared (\d+)", b.stderr)
    if not m or int(m.group(1)) == 0 or int(m.group(4)) == 0 or int(m.group(7)) != 10:
        print("FAIL: the shadow did not run (%s)" % b.stderr.strip())
        ok = False
    print("feedback shadow identity: %d output ticks, %d lines, %s; %s" %
          (ticks, len(la), "identical" if not bad and len(la) == len(lb) else "DIFFERENT",
           b.stderr.strip()))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
