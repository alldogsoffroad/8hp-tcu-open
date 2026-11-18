#!/usr/bin/env python3
"""fb_identity.py - THE FEEDBACK SHADOW LEAVES THE LIVE PATH BIT-IDENTICAL
(ours, 2026-11-18, ~/tcu_sweep/feedback_shadow_port.md).

  fb_identity.py <test_odiag> <test_odiag_fbs> [--pa7]

(--pa7, 2026-11-18 r04d: the second build also carries the PA7 shadow's
hooks; its stderr must show the range part, the pin-cut part and the law ran.
--tsel, 2026-11-18: the second build carries the ECU-temperature shadow's hub
hook; its stderr must show it ran its valid, not-ready and 0x8001 paths.)

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
    if "--pa7" in sys.argv[3:]:
        mp = re.search(r"pa7 rng (\d+) pin (\d+) law (\d+)", b.stderr)
        if not mp or int(mp.group(2)) == 0 or int(mp.group(3)) == 0 or int(mp.group(1)) == 0:
            print("FAIL: the PA7 shadow did not run (%s)" % b.stderr.strip())
            ok = False
    if "--tsel" in sys.argv[3:]:
        mt = re.search(r"tsel pass (\d+) sel8 (\d+) brd8 (\d+) die8 (\d+) ff03 (\d+) r8001 (\d+) tr (\d+)",
                       b.stderr)
        if not mt or int(mt.group(1)) == 0 or int(mt.group(2)) == 0 or int(mt.group(5)) == 0 \
                or int(mt.group(6)) == 0 or int(mt.group(7)) < 3:
            print("FAIL: the temperature shadow did not run its paths (%s)" % b.stderr.strip())
            ok = False
    print("feedback shadow identity: %d output ticks, %d lines, %s; %s" %
          (ticks, len(la), "identical" if not bad and len(la) == len(lb) else "DIFFERENT",
           b.stderr.strip()))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
