#!/usr/bin/env python3
"""pa7_shadow_scan.py - THE PA7 SHADOW IS READ-ONLY (ours, 2026-11-18,
~/tcu_sweep/r04d_recorder_pa7_shadow.md). Comments are stripped first.

  pa7_shadow_scan.py outputs FILE...  no output path / consumer (the C9
                                      composer, the PA7 pin owner, the bus-B
                                      handlers, the drivers, the park path)
                                      may name the shadow: its header, API,
                                      types, cal macros, RAM, the hook's
                                      definition
  pa7_shadow_scan.py shadow FILE...   the shadow includes only its own header,
                                      calls no live module, names no live cell
                                      (no 0xFFF8xxxx / 0xFFF9xxxx / 0xFFFExxxx
                                      literal: not 0xFFF90920, not PADR
                                      0xFFFE3802, not the C9 cell 0xFFF9095A)
  pa7_shadow_scan.py hook FILE...     the gather (app/pa7_hook.c) reads the
                                      live cells only through const volatile
                                      and writes no port register
Exit 1 ("PA7 shadow read-only violation") on any hit.
"""
import re
import sys

OUT_PATTERNS = [
    r"pa7_shadow", r"pa7_hook", r"\bpa7s_", r"\bpa7_(rng|pin|tr)_", r"\bPA7_CAL_", r"\bPA7_TR_N\b",
    r"\bs_pa7\b", r"\.pa7_shadow\b", r"app_pin_cut_hook\s*\(\s*\)\s*\{",
    r"0[xX][fF]{3}878[89a-fA-F][0-9a-fA-F]", r"0[xX][fF]{3}879[0-9a-fA-F]{2}",
]
SHADOW_PATTERNS = [
    r'#\s*include\s*"(?!app/pa7_shadow\.h")[^"]*"',
    r"\b(rq_prod|drv|sup|park|plat|vehicle|rng_chain|kl15|tmp_chain|sol|com|meas|sensors|gs|od|fb|sched)_[a-z0-9_]+\s*\(",
    r"\bP?CELL_[A-Z0-9_]+",
    r"0[xX][fF]{3}[89][0-9a-fA-F]{4}",
    r"0[xX][fF]{4}[0-9a-fA-F]{4}",
    r"\bvolatile\s+[a-z0-9_]+\s*\*\s*\)\s*\(?\s*(uintptr_t\s*\)\s*)?\(?\s*0[xX]",
]
HOOK_PATTERNS = [
    r"(?<!const )\bvolatile\b",
    r"0[xX][fF]{4}[eE][0-9a-fA-F]{3}",          # no port A..C register (PADR 0xFFFE3802)
    r"0[xX][fF]{3}9095[aA]", r"0[xX][fF]{3}90920",   # no write path to the live PA7 / C9 cells (named at all)
]


def strip(txt):
    txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
    txt = re.sub(r"//[^\n]*", " ", txt)
    return txt


def main():
    mode, files = sys.argv[1], sys.argv[2:]
    pats = {"outputs": OUT_PATTERNS, "shadow": SHADOW_PATTERNS, "hook": HOOK_PATTERNS}[mode]
    bad = []
    for f in files:
        txt = strip(open(f, encoding="utf-8", errors="replace").read())
        for p in pats:
            for m in re.finditer(p, txt):
                bad.append("%s: %s" % (f, m.group(0)))
    if bad:
        print("PA7 shadow read-only violation (%s):\n  %s" % (mode, "\n  ".join(bad)))
        return 1
    print("PA7 shadow read-only scan (%s): %d sources clean" % (mode, len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
