#!/usr/bin/env python3
"""shadow_sim_scan.py - THE BENCH-ONLY SIMULATED-SPEED INPUT FEEDS THE READ-ONLY SHADOWS' HOOKS
AND NOTHING ELSE (ours, 2026-11-20, ~/tcu_sweep/shadow_sim_port.md). Comments are stripped first.

  shadow_sim_scan.py tree ROOT      every .c / .h under ROOT/{app,bsw,io,safety,platform} except the
                                    allowlist (io/vehicle/shadow_sim.c/.h, io/vehicle/diag_ids.h, the three
                                    shadow hooks app/sm_hook.c / app/gp_hook.c / app/fr_hook.c, app/main.c)
                                    must not name the interface (shadow_sim*, SHADOW_SIM*, DIAG_ID_SHADOW_SIM)
  shadow_sim_scan.py outputs FILE.. the same rule for explicit files (the negative fixture's mode)
  shadow_sim_scan.py main FILE..    app/main.c may only call shadow_sim_init / _rx / _tick_10ms (and include
                                    the header): no reading of the values (shadow_sim_get, shadow_sim_in_t)
  shadow_sim_scan.py hooks FILE..   the shadow hooks use the values only inside the shadows' feed
                                    functions: they must not write through a volatile pointer or name a
                                    pinned live-path cell they would feed the values into (the engine /
                                    monitor / range / park / supervisor cells 0xFFF8xxxx stay by-value reads)
Exit 1 ("shadow-sim read-only violation") on any hit.
"""
import os
import re
import sys

NAME = [r"\bshadow_sim\w*", r"\bSHADOW_SIM\w*", r"\bDIAG_(ID|BUS)_SHADOW_SIM\b", r"\bTCU_SHADOW_SIM\b"]
ALLOW = {
    "io/vehicle/shadow_sim.c", "io/vehicle/shadow_sim.h", "io/vehicle/diag_ids.h",
    "app/sm_hook.c", "app/gp_hook.c", "app/fr_hook.c", "app/wk_hook.c", "app/main.c",
}
MAIN_OK = {"shadow_sim_init", "shadow_sim_rx", "shadow_sim_tick_10ms"}
HOOK_PATTERNS = [
    r"(?<!const )\bvolatile\s+[a-z0-9_ ]+\*",           # a non-const volatile pointer (a write path)
    r"\)\s*\(?\s*0[xX][fF]{3}[89][0-9a-fA-F]{4}\s*\)?\s*=[^=]",   # *(T*)0xFFF8xxxx = ...
]


def strip(txt):
    txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
    txt = re.sub(r"//[^\n]*", " ", txt)
    return txt


def read(f):
    return strip(open(f, encoding="utf-8", errors="replace").read())


def hits(txt, pats):
    out = []
    for p in pats:
        out += [m.group(0) for m in re.finditer(p, txt)]
    return out


def main():
    mode = sys.argv[1]
    bad = []
    if mode == "tree":
        root = sys.argv[2]
        for sub in ("app", "bsw", "io", "safety", "platform"):
            for d, _, fs in os.walk(os.path.join(root, sub)):
                for f in fs:
                    if not f.endswith((".c", ".h")):
                        continue
                    full = os.path.join(d, f)
                    rel = os.path.relpath(full, root)
                    if rel in ALLOW:
                        continue
                    for h in hits(read(full), NAME):
                        bad.append("%s: %s" % (rel, h))
    elif mode == "outputs":
        for f in sys.argv[2:]:
            for h in hits(read(f), NAME):
                bad.append("%s: %s" % (f, h))
    elif mode == "main":
        for f in sys.argv[2:]:
            txt = read(f)
            txt = re.sub(r"^\s*#\s*include[^\n]*", " ", txt, flags=re.M)
            for h in hits(txt, [r"\bshadow_sim\w*", r"\bDIAG_(ID|BUS)_SHADOW_SIM\b"]):
                if h not in MAIN_OK:
                    bad.append("%s: %s" % (f, h))
    elif mode == "hooks":
        for f in sys.argv[2:]:
            for h in hits(read(f), HOOK_PATTERNS):
                bad.append("%s: %s" % (f, h))
    else:
        print("usage")
        return 2
    if bad:
        print("shadow-sim read-only violation:")
        for b in bad:
            print("  " + b)
        return 1
    print("shadow-sim scan (%s): clean" % mode)
    return 0


if __name__ == "__main__":
    sys.exit(main())
