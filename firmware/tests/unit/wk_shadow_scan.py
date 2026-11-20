#!/usr/bin/env python3
"""wk_shadow_scan.py - THE TORQUE-CONVERTER LOCK-UP (WK) SHADOW IS READ-ONLY AND NEVER TRANSMITS
(ours, 2026-11-20, ~/tcu_sweep/wk_shadow_port.md). Comments are stripped first.

  wk_shadow_scan.py outputs FILE...  no output path / consumer (the request law, the engine, the monitor,
                                     the range and park chain, the supervisor, the temperature chain, the
                                     other shadows) may name the shadow: headers, API, types, macros, RAM
                                     (.wk_shadow 0xFFF935D0..0xFFF9366B)
  wk_shadow_scan.py shadow FILE...   the shadow (wk_shadow.c / .h, wk_cal.c / .h) includes only its own headers
                                     (+ app/cal_inplace.h), calls no live module and NO TRANSMIT path (fca_*,
                                     com_*, can), names no live cell (no 0xFFF8xxxx / 0xFFF9xxxx / 0xFFFFxxxx
                                     literal), no volatile pointer cast, no store through a literal address,
                                     and names none of the stock functions kept out (the engagement counter
                                     0xBF8E8, the fill-adaptation learner 0xAEE26 / 0xAEE1C / 0xAEE2E)
  wk_shadow_scan.py hook FILE...     the gather (app/wk_hook.c) reads live cells only through const volatile,
                                     never names a cell the shadow models as its own (the lock-up object
                                     0xFFF8B294.., the request level 0xFFF8B524.., the speed conditioning
                                     0xFFF8B5FC.., the lock-up request / mode 0xFFF8B2AA / 0xFFF8B4AC, the torque
                                     raise 0xFFF8B5E0..E2, the converter object 0xFFF8E612) and names no
                                     transmit path
Exit 1 ("lock-up shadow read-only violation") on any hit.
"""
import re
import sys

OUT_PATTERNS = [
    r"\bwk_shadow", r"\bwk_hook", r"\bwk_cal", r"\bwk_in_t\b", r"\bwk_t\b", r"\bwk_w_t\b", r"\bwk_law\b",
    r"\bwk_gate\b", r"\bwk_b5e[12]\b", r"\bWK_[A-Z0-9_]+", r"\.wk_shadow\b",
    r"0[xX][fF]{3}935[dD][0-9a-fA-F]", r"0[xX][fF]{3}936[0-6][0-9a-fA-F]",
]
SHADOW_PATTERNS = [
    r'#\s*include\s*"(?!app/wk_shadow\.h"|app/wk_cal\.h"|app/cal_inplace\.h")[^"]*"',
    r"\b(rq_prod|drv|sup|park|plat|vehicle|sensors|rng_chain|kl15|tmp_chain|tmp_safe|sol|com|meas|gs|g6a|"
    r"od|fb|pa7|tsel|sched|sm_shadow|sm_hook|fr|gp|lp|ti|crc8|os_sel|fca|can|uds)_[a-z0-9_]+\s*\(",
    r"\bfca_[a-z0-9_]+", r"\bcom_[a-z0-9_]+", r"\bplat_[a-z0-9_]+",
    r"\bP?CELL_[A-Z0-9_]+",
    r"0[xX][fF]{3}[89][0-9a-fA-F]{4}",
    r"0[xX][fF]{4}[0-9a-fA-F]{4}",
    r"\bvolatile\s+[a-z0-9_]+\s*\*\s*\)",
    # the stock functions kept out (the EEPROM engagement counter, the fill-adaptation learner)
    r"(?i)\b[a-z_0-9]*(bf8e8|aee26|aee1c|aee2e|cee94|ce48c|ce00c)[a-z_0-9]*\b",
]
HOOK_PATTERNS = [
    r"(?<!const )\bvolatile\b",
    r"0[xX][fF]{3}8[bB]2[9a-fA-F][0-9a-fA-F]", r"0[xX][fF]{3}8[bB][34][0-9a-fA-F]{2}",
    r"0[xX][fF]{3}8[bB]4[aA][cC]", r"0[xX][fF]{3}8[bB]5[0-9a-fA-F]{2}", r"0[xX][fF]{3}8[bB]6[0-9a-fA-F]{2}",
    r"0[xX][fF]{3}8[eE]612", r"0[xX][fF]{3}8[eE]61[acAC]",
    r"\bfca_[a-z0-9_]+", r"\bcom_[a-z0-9_]+", r"\bplat_can[a-z0-9_]*",
    r"0[xX]2[fF]9\b", r"0[xX]037\b",
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
        if mode == "shadow":
            # preprocessor directives are not calls (only #include is checked, above)
            incs = re.findall(r'^\s*#\s*include\s*"[^"]*"', txt, flags=re.M)
            txt = re.sub(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif|define|undef|error)\b[^\n]*", " ", txt, flags=re.M)
            # the section attribute names the shadow's own RAM section, not a live cell
            txt = re.sub(r'__attribute__\(\(section\("[^"]*"\)\)\)', " ", txt)
            txt = txt + "\n" + "\n".join(incs)
        for p in pats:
            for m in re.finditer(p, txt):
                bad.append("%s: %s" % (f, m.group(0)))
    if bad:
        print("lock-up shadow read-only violation (%s):\n  %s" % (mode, "\n  ".join(bad)))
        return 1
    print("lock-up shadow read-only scan (%s): %d sources clean" % (mode, len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
