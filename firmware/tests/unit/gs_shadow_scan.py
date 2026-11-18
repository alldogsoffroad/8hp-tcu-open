#!/usr/bin/env python3
"""gs_shadow_scan.py - THE GARAGE SHADOW IS READ-ONLY (ours, 2026-11-17,
~/tcu_sweep/garage_shadow_port.md). Comments are stripped before scanning.

  gs_shadow_scan.py outputs FILE...   every output path may not name the
                                      shadow (its header, API, types, RAM)
  gs_shadow_scan.py shadow FILE...    the shadow may not include a live
                                      header, call a live module or name a
                                      live stock cell (its only live input is
                                      gs_in_t, by value; app/gs_fill.c may
                                      include exactly app/rq_fill.c, the
                                      machinery it re-instantiates)
Exit 1 ("garage shadow read-only violation") on any hit.
"""
import re
import sys

OUT_PATTERNS = [
    r"gs_shadow", r"gs_fill", r"gs_cal", r"\bgsf_", r"\bGS_OBJ_", r"\bGS_REQ_",
    r"\bgs_in_t\b", r"\bgs_rec_t\b", r"\bgs_wit_t\b", r"0[xX][fF]{3}911[aA]0",
    r"\.gs_(req|obj|rec)\b",
]
SHADOW_PATTERNS = [
    # live headers
    r'#\s*include\s*"app/(rq_prod|park|park_law|park_cells|rng_chain|tmp_chain|range|main)\.h"',
    r'#\s*include\s*"bsw/(drv|sup|com|meas|diag)/', r'#\s*include\s*"(safety|io|platform)/',
    # live modules / writers
    r"\b(rq_prod|drv|sup|park|plat|vehicle|rng_chain|kl15|tmp_chain|sol|com|meas)_[a-z0-9_]+\s*\(",
    r"\bP?CELL_[A-Z0-9_]+",
    # live stock cells by number (request objects, currents, fill cells, the
    # 0x89C3C block, the park object, the range cells, the law cells)
    r"0[xX][fF]{3}8[bB][7-9a-fA-F][0-9a-fA-F]{2}", r"0[xX][fF]{3}84[bB][7-9][0-9a-fA-F]",
    r"0[xX][fF]{3}851[aA][0-9a-fA-F]", r"0[xX][fF]{3}8[eE]38[4-9]", r"0[xX][fF]{3}89[cCdD][0-9a-fA-F]{2}",
    r"0[xX][fF]{3}8[dD]6[0-9a-fA-F]{2}", r"0[xX][fF]{3}8[aA][eE]2[0-9a-fA-F]",
    r"0[xX][fF]{3}94[eEfF][0-9a-fA-F]{2}", r"0[xX][fF]{3}902[3-5][0-9a-fA-F]",
]
ALLOWED_INCLUDE = {"app/gs_fill.c": r'#\s*include\s*"app/rq_fill\.c"'}


def strip(txt):
    txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
    txt = re.sub(r"//[^\n]*", " ", txt)
    return txt


def main():
    mode, files = sys.argv[1], sys.argv[2:]
    pats = OUT_PATTERNS if mode == "outputs" else SHADOW_PATTERNS
    bad = []
    for f in files:
        txt = strip(open(f, encoding="utf-8", errors="replace").read())
        for key, allow in ALLOWED_INCLUDE.items():
            if f.endswith(key):
                txt = re.sub(allow, " ", txt)
        for p in pats:
            for m in re.finditer(p, txt):
                bad.append("%s: %s" % (f, m.group(0)))
    if bad:
        print("garage shadow read-only violation (%s):\n  %s" % (mode, "\n  ".join(bad)))
        return 1
    print("garage shadow read-only scan (%s): %d sources clean" % (mode, len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
