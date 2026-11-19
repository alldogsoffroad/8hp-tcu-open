#!/usr/bin/env python3
"""gs_bridge_scan.py - THE GARAGE BRIDGE'S ONE SEAM (ours, 2026-11-18,
~/tcu_sweep/garage_g1_port.md; plan ~/tcu_sweep/garage_live_plan.md §2.1).
Comments are stripped before scanning.

  gs_bridge_scan.py outputs FILE...  no output path may name the bridge (its
                                     header, API, types, the seam) - EXCEPT
                                     bsw/drv/drv_reqlaw.c: exactly one
                                     #include of bsw/drv/drv_gsbr.h and
                                     exactly one drv_gsbr_take( call, which
                                     must sit inside `#if GS_LIVE_DIR != 0`
                                     (so a GS_LIVE_DIR 0 build - G1, SH - has
                                     no seam at all)
  gs_bridge_scan.py seam FILE...     FILE is checked by the seam file's rule
                                     (the negative fixture)
  gs_bridge_scan.py bridge FILE...   the bridge reads live cells only through
                                     const volatile, writes no live cell, calls
                                     no live module (only the garage shadow's
                                     accessors) and includes only its own
                                     headers, the shadow's, the seam's and
                                     park_cells.h (PCELL_TGT, read-only)
Exit 1 ("garage bridge seam violation") on any hit.
"""
import re
import sys

OUT_PATTERNS = [r"gs_bridge", r"\bgsb_", r"\bGSB_", r"drv_gsbr", r"\.gsb_(win|state|rec0|rec1)\b"]
SEAM_FILE = "bsw/drv/drv_reqlaw.c"
BRIDGE_INCLUDES = {"app/gs_bridge.h", "app/gs_shadow.h", "bsw/drv/drv_gsbr.h", "app/park_cells.h",
                   # G6b (2026-11-19): the monitored source, the candidate's read accessors
                   "app/gs_cand.h"}
BRIDGE_PATTERNS = [
    r"\bCELL_[A-Z0-9_]+",                           # the live cell macros (writable)
    r"\bPCELL_(?!TGT\b)[A-Z0-9_]+",                 # park_cells.h: only PCELL_TGT (const)
    # a non-const volatile cast of a literal address (a live write path)
    r"\(\s*volatile[^)]*\*\s*\)\s*(\(\s*uintptr_t\s*\)\s*)?\(?\s*0[xX]",
    r"\b(rq_prod|drv|sup|park|plat|vehicle|rng_chain|kl15|tmp_chain|sol|com|meas|sensors|od|fb|pa7|tsel)_[a-z0-9_]+\s*\(",
]
BRIDGE_ALLOW = [r"\bbool\s+drv_gsbr_take\s*\("]     # its own definition of the seam


def strip(txt):
    txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
    txt = re.sub(r"//[^\n]*", " ", txt)
    return txt


def seam_ok(txt, bad, f):
    incs = re.findall(r'#\s*include\s*"bsw/drv/drv_gsbr\.h"', txt)
    calls = [m.start() for m in re.finditer(r"\bdrv_gsbr_take\s*\(", txt)]
    if len(incs) != 1:
        bad.append("%s: %d includes of drv_gsbr.h (want 1)" % (f, len(incs)))
    if len(calls) != 1:
        bad.append("%s: %d drv_gsbr_take( calls (want exactly 1)" % (f, len(calls)))
    for pos in calls:
        stack = []
        for m in re.finditer(r"^[ \t]*#[ \t]*(if|ifdef|ifndef|elif|else|endif)\b([^\n]*)", txt[:pos], flags=re.M):
            d, rest = m.group(1), m.group(2).strip()
            if d in ("if", "ifdef", "ifndef"):
                stack.append((d, rest))
            elif d in ("elif", "else"):
                if stack:
                    stack[-1] = (d, rest)
            elif d == "endif":
                if stack:
                    stack.pop()
        if not any(d == "if" and re.fullmatch(r"GS_LIVE_DIR\s*!=\s*0", r) for d, r in stack):
            bad.append("%s: drv_gsbr_take( outside `#if GS_LIVE_DIR != 0` (enclosing: %s)" % (f, stack))
    # nothing else of the bridge
    rest = re.sub(r'#\s*include\s*"bsw/drv/drv_gsbr\.h"', " ", txt)
    rest = re.sub(r"\bdrv_gsbr_take\s*\(", " (", rest)
    for p in OUT_PATTERNS:
        for m in re.finditer(p, rest):
            bad.append("%s: %s" % (f, m.group(0)))


def main():
    mode, files = sys.argv[1], sys.argv[2:]
    bad = []
    for f in files:
        txt = strip(open(f, encoding="utf-8", errors="replace").read())
        if mode in ("outputs", "seam"):
            if mode == "seam" or f.replace("\\", "/").endswith(SEAM_FILE):
                seam_ok(txt, bad, f)
                continue
            for p in OUT_PATTERNS:
                for m in re.finditer(p, txt):
                    bad.append("%s: %s" % (f, m.group(0)))
        else:
            for inc in re.findall(r'#\s*include\s*"([^"]+)"', txt):
                if inc not in BRIDGE_INCLUDES:
                    bad.append("%s: includes %s" % (f, inc))
            for a in BRIDGE_ALLOW:
                txt = re.sub(a, " (", txt)
            for p in BRIDGE_PATTERNS:
                for m in re.finditer(p, txt):
                    bad.append("%s: %s" % (f, m.group(0)))
            # every stock-RAM literal sits in a const volatile read macro's #define
            for m in re.finditer(r"0[xX][fF]{3}[89][0-9a-fA-F]{4}", txt):
                line = txt[txt.rfind("\n", 0, m.start()) + 1: txt.find("\n", m.start())]
                if not re.match(r"\s*#\s*define\s+L_[A-Z0-9_]+\s", line):
                    bad.append("%s: live literal outside a #define L_*: %s" % (f, line.strip()))
            if f.endswith(".c") and (
                    not re.search(r"#\s*define\s+LIVE_U8\(a\)\s+\(\*\(const volatile", txt) or
                    not re.search(r"#\s*define\s+LIVE_U16\(a\)\s+\(\*\(const volatile", txt)):
                bad.append("%s: LIVE_U8 / LIVE_U16 are not const volatile reads" % f)
    if bad:
        print("garage bridge seam violation (%s):\n  %s" % (mode, "\n  ".join(bad)))
        return 1
    print("garage bridge seam scan (%s): %d sources clean" % (mode, len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
