#!/usr/bin/env python3
"""fb_shadow_scan.py - THE FEEDBACK SHADOW IS READ-ONLY (ours, 2026-11-18,
~/tcu_sweep/feedback_shadow_port.md). Comments are stripped before scanning.

  fb_shadow_scan.py outputs FILE...  every output path / consumer may not name
                                     the shadow (its header, API, types,
                                     accessors, RAM)
  fb_shadow_scan.py shadow FILE...   the shadow may include only its own
                                     headers and platform/plat_fbhw.h, call no
                                     live module, name no live cell (no
                                     0xFFF8xxxx / 0xFFF9xxxx literal)
  fb_shadow_scan.py hook FILE...     the gather (app/fb_hook.c) reads the live
                                     cells only through const volatile
Exit 1 ("feedback shadow read-only violation") on any hit.
"""
import re
import sys

OUT_PATTERNS = [
    r"fb_shadow", r"fb_cal", r"fb_hook", r"\bfbs_", r"\bFBS_", r"\bfbhw_",
    r"\bFB_(C_|PT_|CFG|PINS|IV_|PEXP|HOLD|COEF|RFIX|R20|SPLIT|INOM|DTBL|CAL_)",
    r"plat_fbsamp", r"app_out_head_hook\s*\(\s*\)\s*\{", r"\.fb_shadow\b",
    r"0[xX][fF]{3}86[a-fA-F][0-9a-fA-F]{2}",
]
SHADOW_PATTERNS = [
    r'#\s*include\s*"(?!app/fb_shadow\.h"|app/fb_cal\.h"|platform/plat_fbhw\.h")[^"]*"',
    r"\b(rq_prod|drv|sup|park|plat|vehicle|rng_chain|kl15|tmp_chain|sol|com|meas|sensors|gs|od)_[a-z0-9_]+\s*\(",
    r"\bP?CELL_[A-Z0-9_]+",
    r"0[xX][fF]{3}[89][0-9a-fA-F]{4}",
]
SHADOW_ALLOW = [r"\bplat_fbsamp_start\s*\("]
HOOK_PATTERNS = [r"(?<!const )\bvolatile\b"]


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
        if mode == "shadow":
            for a in SHADOW_ALLOW:
                txt = re.sub(a, " (", txt)
        for p in pats:
            for m in re.finditer(p, txt):
                bad.append("%s: %s" % (f, m.group(0)))
    if bad:
        print("feedback shadow read-only violation (%s):\n  %s" % (mode, "\n  ".join(bad)))
        return 1
    print("feedback shadow read-only scan (%s): %d sources clean" % (mode, len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
