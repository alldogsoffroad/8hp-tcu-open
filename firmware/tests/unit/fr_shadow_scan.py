#!/usr/bin/env python3
"""fr_shadow_scan.py - THE FAULT-REACTION SHADOW IS READ-ONLY (ours, F1
2026-11-19, ~/tcu_sweep/f1_fr_shadow_port.md). Comments are stripped first.

  fr_shadow_scan.py outputs FILE...  no output path / consumer (the request
                                     law, the engine, the monitor, the range
                                     and park chain, the supervisor, the
                                     temperature chain, the gear chain) may
                                     name the shadow: its headers, API, types,
                                     macros, its RAM (.fr_shadow 0xFFF860xx..
                                     865xx, .fr_fm 0xFFF887xx..891xx)
  fr_shadow_scan.py shadow FILE...   the shadow (fr_shadow.c / .h, fr_fm.c,
                                     fr_cal.h) includes only its own headers
                                     (+ app/sm_cal.h: the gear-pair tables),
                                     calls no live module, names no live cell
                                     (no 0xFFF8xxxx / 0xFFF9xxxx / 0xFFFFxxxx
                                     literal), no volatile pointer cast; and
                                     only app/fr_fm.c names the fault
                                     manager's RAM (s_fm, a non-const
                                     fr_fm_t pointer)
  fr_shadow_scan.py cal FILE...      the generated app/fr_cal.c: its own
                                     header only, no call, no volatile (its
                                     0xFFF84xxx RX status addresses are stock
                                     witness data, never dereferenced)
  fr_shadow_scan.py hook FILE...     the gather (app/fr_hook.c) reads live
                                     cells only through const volatile and
                                     never names a cell the shadow models
                                     (0x89C41..44, 0x89CED, 0x89CF5, AE2A..2F,
                                     the comm / 0x13A9D4 / 0x14E0B6 objects
                                     0xFFF95750..0xFFF958FF, the DSM arrays
                                     0xFFF8D700..0xFFF8DEFF, the masks
                                     0xFFF897D0..EF / refcounts 0xFFF89810..
                                     9F, the RX states 0xFFF84000..44FF)
Exit 1 ("fault-reaction shadow read-only violation") on any hit.
"""
import os
import re
import sys

OUT_PATTERNS = [
    r"\bfr_shadow", r"\bfr_hook", r"\bfr_cal", r"\bfr_fm", r"\bFR_EV_", r"\bFRC_", r"\bFR_CAL_",
    r"\bfr_t\b", r"\bfr_in_t\b", r"\bfr_fm_t\b", r"\bfr_rx_t\b", r"\bfr_code_t\b", r"\bfr_comm_t\b",
    r"\.fr_shadow\b", r"\.fr_fm\b", r"\bs_fr\b", r"\bs_fm\b",
    r"0[xX][fF]{3}86[0-5][0-9a-fA-F]{2}", r"0[xX][fF]{3}88[7-9a-fA-F][0-9a-fA-F]{2}",
    r"0[xX][fF]{3}891[0-9a-fA-F]{2}",
]
SHADOW_PATTERNS = [
    r'#\s*include\s*"(?!app/fr_shadow\.h"|app/fr_cal\.h"|app/sm_cal\.h")[^"]*"',
    r"\b(rq_prod|drv|sup|park|plat|vehicle|sensors|rng_chain|kl15|tmp_chain|tmp_safe|sol|com|meas|gs|g6a|"
    r"od|fb|pa7|tsel|sched|sm_shadow|sm_gear|sm_hook|crc8|os_sel)_[a-z0-9_]+\s*\(",
    r"\bP?CELL_[A-Z0-9_]+",
    r"0[xX][fF]{3}[89][0-9a-fA-F]{4}",
    r"0[xX][fF]{4}[0-9a-fA-F]{4}",
    r"\bvolatile\s+[a-z0-9_]+\s*\*\s*\)",
]
FM_ONLY = [r"\bs_fm\b", r"\(\s*fr_fm_t\s*\*\s*\)", r"(?<!const )(?<!volatile )\bfr_fm_t\s*\*"]
CAL_PATTERNS = [
    r'#\s*include\s*"(?!app/fr_cal\.h")[^"]*"',
    r"\b[a-z_][a-z0-9_]*\s*\(",
    r"\bvolatile\b",
]
HOOK_PATTERNS = [
    r"(?<!const )\bvolatile\b",
    r"0[xX][fF]{3}89[cC]4[1-4]", r"0[xX][fF]{3}89[cC][eE][dD]", r"0[xX][fF]{3}89[cC][fF]5",
    r"0[xX][fF]{3}8[aA][eE]2[a-fA-F]",
    r"0[xX][fF]{3}957[5-9a-fA-F][0-9a-fA-F]", r"0[xX][fF]{3}958[0-9a-fA-F]{2}",
    r"0[xX][fF]{3}8[dD][7-9a-eA-E][0-9a-fA-F]{2}",
    r"0[xX][fF]{3}897[dDeE][0-9a-fA-F]", r"0[xX][fF]{3}898[1-9][0-9a-fA-F]",
    r"0[xX][fF]{3}84[0-4][0-9a-fA-F]{2}",
]


def strip(txt):
    txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
    txt = re.sub(r"//[^\n]*", " ", txt)
    return txt


def main():
    mode, files = sys.argv[1], sys.argv[2:]
    pats = {"outputs": OUT_PATTERNS, "shadow": SHADOW_PATTERNS, "hook": HOOK_PATTERNS,
            "cal": CAL_PATTERNS}[mode]
    bad = []
    for f in files:
        txt = strip(open(f, encoding="utf-8", errors="replace").read())
        if mode == "cal":
            # the generated tables: the FR_NEG_B(v) macro (a value, not a call) and
            # the #define lines are not calls
            txt = re.sub(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif|define)\b[^\n]*", " ", txt, flags=re.M)
            txt = re.sub(r"FR_NEG_B\(\s*\d+u\s*\)", "0u", txt)
        for p in pats:
            for m in re.finditer(p, txt):
                bad.append("%s: %s" % (f, m.group(0)))
        if mode == "shadow" and os.path.basename(f) != "fr_fm.c":
            for p in FM_ONLY:
                for m in re.finditer(p, txt):
                    bad.append("%s: %s (only app/fr_fm.c writes the fault manager's RAM)" % (f, m.group(0)))
    if bad:
        print("fault-reaction shadow read-only violation (%s):\n  %s" % (mode, "\n  ".join(bad)))
        return 1
    print("fault-reaction shadow read-only scan (%s): %d sources clean" % (mode, len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
