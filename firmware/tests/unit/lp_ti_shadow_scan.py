#!/usr/bin/env python3
"""lp_ti_shadow_scan.py - THE LINE-PRESSURE (Sys) AND TORQUE-INTERVENTION SHADOWS ARE
READ-ONLY AND NEVER TRANSMIT (ours, 2026-11-20, ~/tcu_sweep/lp_ti_shadow_port.md).
Comments are stripped first.

  lp_ti_shadow_scan.py outputs FILE...  no output path / consumer (the request law, the engine,
                                        the monitor, the range and park chain, the supervisor,
                                        the temperature chain, the other shadows) may name either
                                        shadow: headers, API, types, macros, RAM (.lp_shadow
                                        0xFFF8BA40..0xFFF8BAF3, .ti_shadow 0xFFF929C0..0xFFF92A7F)
  lp_ti_shadow_scan.py shadow FILE...   the shadows (lp_shadow.c / .h, ti_shadow.c / .h, lp_cal.c / .h)
                                        include only their own headers (+ app/cal_inplace.h), call no
                                        live module and NO TRANSMIT path (fca_*, com_*, can), name no
                                        live cell (no 0xFFF8xxxx / 0xFFF9xxxx / 0xFFFFxxxx literal), no
                                        volatile pointer cast, no store through a literal address
  lp_ti_shadow_scan.py hook FILE...     the gather (app/lp_hook.c) reads live cells only through const
                                        volatile, never names a cell the shadows model as their own
                                        (the line-pressure object 0xFFF8BCB0.., the input torque
                                        0xFFF8E34A, 0xFFF8513E, 0xFFF89CF5, 0xFFF8E21E, the torque-
                                        intervention object 0xFFF8B9D8.., the request 0xFFF8AEC8.., the
                                        hand-over 0xFFF850C6 / 0xFFF850D0) and names no transmit path
  (2026-11-23) THE NISSAN LINE-PRESSURE LIVE SEAM (app/nissan_lp.h / .c, NISSAN_LP_LIVE): in
  `outputs` mode no output path may name the seam (nissan_lp_*, app/nissan_lp.h) - EXCEPT the
  monitor app/gs_bridge.c, which may include app/nissan_lp.h and call nissan_lp_sys_request (its
  one export the seam drv_gsbr_take needs), each only inside an `#if DRV_RQ_SYS_LIVE` block (the
  nearest enclosing conditional). The seam itself (app/nissan_lp.c) is checked in `hook` mode.
Exit 1 ("line-pressure / torque-intervention shadow read-only violation") on any hit.
"""
import re
import sys

OUT_PATTERNS = [
    r"\blp_shadow", r"\blp_hook", r"\blp_cal", r"\bti_cal", r"\blp_in_t\b", r"\blp_t\b", r"\blp_w_t\b", r"\blp_sys_law\b",
    r"\blp_input_torque\b", r"\bLP_[A-Z0-9_]+", r"\bLPV_", r"\bs_lp\b", r"\.lp_shadow\b",
    r"\bti_shadow", r"\bti_hook", r"\bti_in_t\b", r"\bti_t\b", r"\bti_w_t\b", r"\bti_field13\b", r"\bTI_[A-Z0-9_]+",
    r"\bs_ti\b", r"\.ti_shadow\b",
    r"0[xX][fF]{3}8[bB][aA][4-9a-fA-F][0-9a-fA-F]",
    r"0[xX][fF]{3}929[c-fC-F][0-9a-fA-F]", r"0[xX][fF]{3}92[aA][0-7][0-9a-fA-F]",
]
SHADOW_PATTERNS = [
    r'#\s*include\s*"(?!app/lp_shadow\.h"|app/lp_cal\.h"|app/ti_shadow\.h"|app/ti_cal\.h"|app/cal_inplace\.h")[^"]*"',
    r"\b(rq_prod|drv|sup|park|plat|vehicle|sensors|rng_chain|kl15|tmp_chain|tmp_safe|sol|com|meas|gs|g6a|"
    r"od|fb|pa7|tsel|sched|sm_shadow|sm_hook|fr|gp|crc8|os_sel|fca|can|uds)_[a-z0-9_]+\s*\(",
    r"\bfca_[a-z0-9_]+", r"\bcom_[a-z0-9_]+", r"\bplat_[a-z0-9_]+",
    r"\bP?CELL_[A-Z0-9_]+",
    r"0[xX][fF]{3}[89][0-9a-fA-F]{4}",
    r"0[xX][fF]{4}[0-9a-fA-F]{4}",
    r"\bvolatile\s+[a-z0-9_]+\s*\*\s*\)",
]
# the Nissan live seam (see the docstring): forbidden in every output path but the monitor's block
NLP_PATTERNS = [r"\bnissan_lp_[a-z0-9_]*", r'#\s*include\s*"app/nissan_lp\.h"']
NLP_MONITOR = "app/gs_bridge.c"
NLP_MONITOR_ALLOW = {"nissan_lp_sys_request"}
HOOK_PATTERNS = [
    r"(?<!const )\bvolatile\b",
    r"0[xX][fF]{3}8[bB][cC][b-fB-F][0-9a-fA-F]", r"0[xX][fF]{3}8[bB][dD]0[0-9a-fA-F]",
    r"0[xX][fF]{3}8[eE]34[aA]", r"0[xX][fF]{3}8513[eE]", r"0[xX][fF]{3}89[cC][fF]5", r"0[xX][fF]{3}8[eE]21[eE]",
    r"0[xX][fF]{3}8[bB]9[d-fD-F][0-9a-fA-F]", r"0[xX][fF]{3}8[bB][aA][0-2][0-9a-fA-F]",
    r"0[xX][fF]{3}8[aA][eE][cC][0-9a-fA-F]", r"0[xX][fF]{3}8[aA][eE][dD][0-9a-fA-F]", r"0[xX][fF]{3}8[aA][eE][eE][0-7]",
    r"0[xX][fF]{3}850[cC][6-9a-fA-F]", r"0[xX][fF]{3}850[dD][0-1]",
    r"\bfca_[a-z0-9_]+", r"\bcom_[a-z0-9_]+", r"\bplat_can[a-z0-9_]*",
    r"0[xX]2[fF]9\b", r"0[xX]037\b",
]


def strip(txt):
    txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
    txt = re.sub(r"//[^\n]*", " ", txt)
    return txt


def enclosing_if(txt, pos):
    """the nearest unclosed #if / #ifdef / #ifndef / #elif / #else line before pos"""
    stack = []
    for m in re.finditer(r"^[ \t]*#[ \t]*(if|ifdef|ifndef|elif|else|endif)\b[^\n]*", txt[:pos], flags=re.M):
        d = m.group(1)
        if d in ("if", "ifdef", "ifndef"):
            stack.append(m.group(0).strip())
        elif d in ("elif", "else"):
            if stack:
                stack[-1] = m.group(0).strip()
        elif stack:
            stack.pop()
    return stack[-1] if stack else ""


def nlp_check(f, txt):
    hits = []
    mon = f.replace("\\", "/").endswith(NLP_MONITOR)
    for p in NLP_PATTERNS:
        for m in re.finditer(p, txt):
            tok = m.group(0)
            if mon and (tok in NLP_MONITOR_ALLOW or tok.endswith('"app/nissan_lp.h"')):
                if re.match(r"#\s*if\s+DRV_RQ_SYS_LIVE\b", enclosing_if(txt, m.start())):
                    continue
                hits.append("%s: %s (outside an #if DRV_RQ_SYS_LIVE block)" % (f, tok))
            else:
                hits.append("%s: %s (the Nissan live seam named by an output path)" % (f, tok))
    return hits


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
        if mode == "outputs":
            bad.extend(nlp_check(f, txt))
    if bad:
        print("line-pressure / torque-intervention shadow read-only violation (%s):\n  %s" % (mode, "\n  ".join(bad)))
        return 1
    print("line-pressure / torque-intervention shadow read-only scan (%s): %d sources clean" % (mode, len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
