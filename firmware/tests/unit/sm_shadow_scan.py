#!/usr/bin/env python3
"""sm_shadow_scan.py - THE SHIFT-MAP SHADOW IS READ-ONLY (ours, 2026-11-19,
~/tcu_sweep/sm_shadow_port.md). Comments are stripped first.

  sm_shadow_scan.py outputs FILE...  no output path / consumer (the request
                                     law, the engine, the monitor, the range
                                     and park chain, the supervisor, the
                                     temperature chain) may name the shadow:
                                     its headers, API, types, stand-in macros,
                                     its RAM (.sm_shadow, 0xFFF868xx..869xx)
  sm_shadow_scan.py shadow FILE...   the shadow (sm_shadow.c / .h, sm_cal.c /
                                     .h) includes only its own headers, calls
                                     no live module, names no live cell (no
                                     0xFFF8xxxx / 0xFFF9xxxx / 0xFFFFxxxx
                                     literal), no volatile pointer cast
  (2026-11-19: + the gear feedback chain app/sm_gear.c — the shadow set, the
   outputs may not name it, the hook may not read its modelled cells)
  sm_shadow_scan.py hook FILE...     the gather (app/sm_hook.c) reads live
                                     cells only through const volatile and
                                     never names a cell the shadow models
                                     (the strategy objects 0xFFF8A004..
                                     0xFFF8A7FF incl. A697 / A0AA / A786 /
                                     A694 / S = 0xFFF8A4E4, AB30..AB5F,
                                     0x89CE8, 0x89C28)
Exit 1 ("shift-map shadow read-only violation") on any hit.
"""
import re
import sys

OUT_PATTERNS = [
    r"\bsm_shadow", r"\bsm_hook", r"\bsm_cal", r"\bSM_SI_", r"\bSM_CAL_", r"\bs_sm\b",
    r"\.sm_shadow\b", r"\bsm_in_t\b", r"\bsm_t\b",
    r"\bsm_gear", r"\bgf_t\b", r"\bgf_in_t\b", r"\bgf_tr_t\b", r"\.sm_gear\b", r"\bGF_SI_", r"\bs_gf\b",
    r"0[xX][fF]{3}86[89][0-9a-fA-F]{2}",
]
SHADOW_PATTERNS = [
    r'#\s*include\s*"(?!app/sm_shadow\.h"|app/sm_cal\.h")[^"]*"',
    r"\b(rq_prod|drv|sup|park|plat|vehicle|sensors|rng_chain|kl15|tmp_chain|tmp_safe|sol|com|meas|gs|g6a|od|fb|pa7|tsel|sched)_[a-z0-9_]+\s*\(",
    r"\bP?CELL_[A-Z0-9_]+",
    r"0[xX][fF]{3}[89][0-9a-fA-F]{4}",
    r"0[xX][fF]{4}[0-9a-fA-F]{4}",
    r"\bvolatile\s+[a-z0-9_]+\s*\*\s*\)",
]
HOOK_PATTERNS = [
    r"(?<!const )\bvolatile\b",
    r"0[xX][fF]{3}8[aA][0-7][0-9a-fA-F]{2}",
    r"0[xX][fF]{3}8[aA][bB][3-5][0-9a-fA-F]",
    r"0[xX][fF]{3}89[cC][eE]8", r"0[xX][fF]{3}89[cC]28",
    # (2026-11-19) the gear feedback chain's own cells (app/sm_gear.c): CCE4..7,
    # E4A4..E4A6, E69C..E69E, E6A0 / E6A1, 0x89C25..2E, E33C..E34F, 0x8519F /
    # 0x851A0, AE2A..AE2F — the live E4A7 / E69F are read for the witness only
    r"0[xX][fF]{3}8[cC][cC][eE][4-7]", r"0[xX][fF]{3}8[eE]4[aA][4-6]",
    r"0[xX][fF]{3}8[eE]69[c-eC-E]", r"0[xX][fF]{3}8[eE]6[aA][01]",
    r"0[xX][fF]{3}89[cC]2[5-9a-eA-E]", r"0[xX][fF]{3}8[eE]3[34][0-9a-fA-F]",
    r"0[xX][fF]{3}8519[fF]", r"0[xX][fF]{3}851[aA]0", r"0[xX][fF]{3}8[aA][eE]2[a-fA-F]",
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
        print("shift-map shadow read-only violation (%s):\n  %s" % (mode, "\n  ".join(bad)))
        return 1
    print("shift-map shadow read-only scan (%s): %d sources clean" % (mode, len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
