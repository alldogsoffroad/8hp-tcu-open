#!/usr/bin/env python3
"""tsel_shadow_scan.py - THE ECU-TEMPERATURE SHADOW IS READ-ONLY (ours,
2026-11-18, ~/tcu_sweep/diag_temp_shadow_port.md). Comments are stripped
first.

  tsel_shadow_scan.py outputs FILE...  no output path / consumer (the
                                       diagnosis, the request law, drv_temp,
                                       the feedback chain, the safety
                                       layer's temperature chain, the
                                       supervisor) may name the shadow: its
                                       header, API, types, cal macros, RAM,
                                       the hook's definition
  tsel_shadow_scan.py shadow FILE...   the shadow includes only its own
                                       header, calls no live module, names
                                       no live cell (no 0xFFF8xxxx /
                                       0xFFF9xxxx / 0xFFFFxxxx literal)
  tsel_shadow_scan.py hook FILE...     the gather (app/tsel_hook.c) reads the
                                       live cells only through const
                                       volatile and never names a cell the
                                       shadow models (0x9393A..F, 0x940E0..
                                       E8, 0x940FC..0x94101, 0x93AB2..C3,
                                       0x93AF0..F7, 0x9415C..E, 0x94170,
                                       0x93916, 0x94730, 0x9477A/C, 0x94823)
Exit 1 ("temperature shadow read-only violation") on any hit.
"""
import re
import sys

OUT_PATTERNS = [
    r"tsel_shadow", r"tsel_hook", r"\btsel_", r"\bTSEL_", r"\bs_tsel\b", r"\.tsel_shadow\b",
    r"app_temp_hub_hook\s*\(\s*\)\s*\{",
    r"0[xX][fF]{3}87[cCdD][0-9a-fA-F]{2}",
]
SHADOW_PATTERNS = [
    r'#\s*include\s*"(?!app/tsel_shadow\.h")[^"]*"',
    r"\b(rq_prod|drv|sup|park|plat|vehicle|rng_chain|kl15|tmp_chain|tmp_safe|sol|com|meas|sensors|gs|od|fb|pa7|sched)_[a-z0-9_]+\s*\(",
    r"\bP?CELL_[A-Z0-9_]+",
    r"0[xX][fF]{3}[89][0-9a-fA-F]{4}",
    r"0[xX][fF]{4}[0-9a-fA-F]{4}",
    r"\bvolatile\s+[a-z0-9_]+\s*\*\s*\)\s*\(?\s*(uintptr_t\s*\)\s*)?\(?\s*0[xX]",
]
HOOK_PATTERNS = [
    r"(?<!const )\bvolatile\b",
    r"0[xX][fF]{3}9393[a-fA-F]",
    r"0[xX][fF]{3}940[eE][0-8]",
    r"0[xX][fF]{3}940[fF][c-fC-F]", r"0[xX][fF]{3}9410[01]",
    r"0[xX][fF]{3}93[aA][bB][2-9a-fA-F]", r"0[xX][fF]{3}93[aA][cC][0-3]",
    r"0[xX][fF]{3}93[aA][fF][0-7]",
    r"0[xX][fF]{3}9415[c-eC-E]", r"0[xX][fF]{3}94170",
    r"0[xX][fF]{3}93916", r"0[xX][fF]{3}94730", r"0[xX][fF]{3}9477[aAcC]", r"0[xX][fF]{3}94823",
    r"\bCELL_TMP_", r"\bCELL_DIR_BITS\s*=",
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
        print("temperature shadow read-only violation (%s):\n  %s" % (mode, "\n  ".join(bad)))
        return 1
    print("temperature shadow read-only scan (%s): %d sources clean" % (mode, len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
