#!/usr/bin/env python3
"""gp_shadow_scan.py - THE GEAR-PAIR SHIFT-ENGINE SHADOW IS READ-ONLY (ours,
2026-11-20, ~/tcu_sweep/gp_shadow_port.md). Comments are stripped first.

  gp_shadow_scan.py outputs FILE...  no output path / consumer (the request
                                     law, the engine, the monitor, the range
                                     and park chain, the supervisor, the
                                     temperature chain, the other shadows) may
                                     name the shadow: its headers, API, types,
                                     macros, its RAM (.gp_shadow / .gp_gear
                                     0xFFF86570..0xFFF86793, .gp_ext
                                     0xFFF86794..0xFFF867FF, .gp_ramp
                                     0xFFF869BC..0xFFF869FF)
  gp_shadow_scan.py shadow FILE...   the shadow (gp_shadow.c / .h, gp_gear.c,
                                     gp_cal.c / .h) includes only its own
                                     headers (+ app/sm_shadow.h, app/sm_cal.h,
                                     app/sm_gear.c — the coupled chain's
                                     template), calls no live module, names no
                                     live cell (no 0xFFF8xxxx / 0xFFF9xxxx /
                                     0xFFFFxxxx literal), no volatile pointer
                                     cast
  gp_shadow_scan.py hook FILE...     the gather (app/gp_hook.c) reads live
                                     cells only through const volatile and
                                     never names a cell the shadow models as
                                     its own (the shift object's blocks A / B
                                     0xFFF8BD60..0xFFF8BFFF, E33C 0xFFF8E33C..
                                     0xFFF8E34F, CCE4 0xFFF8CCE4..7, E4A4..6)
Exit 1 ("gear-pair shadow read-only violation") on any hit.
"""
import re
import sys

OUT_PATTERNS = [
    r"\bgp_shadow", r"\bgp_hook", r"\bgp_cal", r"\bgp_gear", r"\bgp_cpl_", r"\bGP_EV_", r"\bGP_SI_",
    r"\bGP_CAL_", r"\bgp_t\b", r"\bgp_in_t\b", r"\bgp_blk_t\b", r"\bgp_po_t\b", r"\bgp_w_t\b",
    r"\bgp_ph", r"\bgp_pb_t\b", r"\bgp_pw_t\b", r"\.gp_phase\b", r"\bs_gph\b", r"\bGP_PH_", r"\bgp_(ua|ub|uc|o40|o3c|o4c|c134)_t\b",
    r"\.gp_shadow\b", r"\.gp_gear\b", r"\.gp_ext\b", r"\.gp_ramp\b", r"\bs_gp\b", r"\bs_gx\b", r"\bs_gr\b",
    r"\bgp_x_t\b", r"\bgp_xblk_t\b", r"\bgp_xpo_t\b", r"\bgp_xw_t\b", r"\bgp_in2_t\b", r"\bgp_ramp_",
    r"\bgp_class_", r"\bGP_NEG_",
    r"0[xX][fF]{3}87[eE][b-fB-F][0-9a-fA-F]", r"0[xX][fF]{3}87[fF][0-9a-fA-F]{2}",
    r"0[xX][fF]{3}865[7-9a-fA-F][0-9a-fA-F]", r"0[xX][fF]{3}866[0-9a-fA-F]{2}", r"0[xX][fF]{3}867[0-9][0-9a-fA-F]",
    r"0[xX][fF]{3}867[a-fA-F][0-9a-fA-F]", r"0[xX][fF]{3}869[b-fB-F][0-9a-fA-F]",
]
SHADOW_PATTERNS = [
    r'#\s*include\s*"(?!app/gp_shadow\.h"|app/gp_cal\.h"|app/gp_cal_ph\.h"|app/sm_cal\.h"|app/sm_shadow\.h"|app/sm_gear\.c"|app/cal_inplace\.h")[^"]*"',
    r"\b(rq_prod|drv|sup|park|plat|vehicle|sensors|rng_chain|kl15|tmp_chain|tmp_safe|sol|com|meas|gs|g6a|"
    r"od|fb|pa7|tsel|sched|sm_shadow|sm_hook|fr|crc8|os_sel)_[a-z0-9_]+\s*\(",
    r"\bP?CELL_[A-Z0-9_]+",
    r"0[xX][fF]{3}[89][0-9a-fA-F]{4}",
    r"0[xX][fF]{4}[0-9a-fA-F]{4}",
    r"\bvolatile\s+[a-z0-9_]+\s*\*\s*\)",
]
HOOK_PATTERNS = [
    r"(?<!const )\bvolatile\b",
    r"0[xX][fF]{3}8[bB][dD][6-9a-fA-F][0-9a-fA-F]", r"0[xX][fF]{3}8[bB][eEfF][0-9a-fA-F]{2}",
    r"0[xX][fF]{3}8[eE]3[3-4][0-9a-fA-F]", r"0[xX][fF]{3}8[cC][cC][eE][4-7]",
    r"0[xX][fF]{3}8[eE]4[aA][4-6]",
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
            # the generated tables' mutation macro (a value) is not a call
            txt = re.sub(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif|define)\b[^\n]*", " ", txt, flags=re.M)
            txt = re.sub(r"GP_NEG_E\(\s*\d+u\s*\)", "0u", txt)
        for p in pats:
            for m in re.finditer(p, txt):
                bad.append("%s: %s" % (f, m.group(0)))
    if bad:
        print("gear-pair shadow read-only violation (%s):\n  %s" % (mode, "\n  ".join(bad)))
        return 1
    print("gear-pair shadow read-only scan (%s): %d sources clean" % (mode, len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
