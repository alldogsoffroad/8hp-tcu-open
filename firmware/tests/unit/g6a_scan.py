#!/usr/bin/env python3
"""g6a_scan.py - G6a: THE CANDIDATE AND ITS COMPARATOR ARE READ-ONLY (ours,
2026-11-19, ~/tcu_sweep/g6a_port.md). Comments are stripped before the
source scans.

  g6a_scan.py outputs FILE...   no output path may name the candidate or the
                                comparator (gs_cand*, gcf_*, g6a*, .gsc_*,
                                their RAM) - EXCEPT bsw/sup/sup_mode.c: the
                                one weak boot hook g6a_boot_pass (declared
                                weak, called once, under !TCU_RAM_APP)
  g6a_scan.py cmp FILE...       the comparator (app/g6a.c) calls only the
                                allowed READ getters, never stores through a
                                literal address, names no CELL_/PCELL_
                                macro, includes only its allowed headers
  g6a_scan.py link NM OBJ...    THE LINK-MAP SCAN (plan §3, common gates):
                                every undefined symbol of the candidate's
                                and the comparator's objects (`NM -u`) is in
                                the allowed set, no defined symbol names an
                                EEPROM / NVM / DSM / event-log / fatal
                                routine of the stock (0xE8BFA, 0xE8CF6,
                                0xD229E, 0xF71EC, 0xA2028, 0xA1712, 0xBDDB0,
                                0xBD84E, 0x6AB8E, 0x71594, 0x720C8, 0xB79E4,
                                0xA0E70, 0x6865C), and no object holds the
                                EEPROM dirty flag 0xFFF85175 / lock cells
                                0xFFF85174..76 as a literal (objdump -s of
                                the data/literal sections)
Exit 1 ("G6a read-only violation") on any hit.
"""
import re
import subprocess
import sys

OUT_PATTERNS = [r"gs_cand", r"\bgcf_", r"\bg6a", r"\bG6A_", r"\bGSC_", r"\.gsc_(req|obj)\b",
                r"0[xX][fF]{3}925[cC]0", r"0[xX][fF]{3}93[cC]08"]
HOOK_FILE = "bsw/sup/sup_mode.c"

CMP_INCLUDES = {"app/g6a.h", "app/gs_cand.h", "app/gs_bridge.h", "bsw/drv/drv_rqprod.h",
                "bsw/drv/drv_reqlaw.h", "bsw/drv/drv_jobs.h", "platform/platform.h"}
CMP_CALL_ALLOW = {
    # the live path, read getters only
    "rq_prod_started", "rq_prod_ch", "rq_prod_fill_state", "drv_rq_fill_ok", "drv_out_wit",
    "gs_bridge_state", "plat_frt_us",
    # instance 1 (the shadow) accessors
    "gs_shadow_cur", "gs_shadow_reqmode", "gs_shadow_fillst", "gs_shadow_pf", "gs_shadow_phase",
    "gs_shadow_releasing", "gs_shadow_pending", "gs_shadow_wit",
    # instance 2 (the candidate) - its own RAM
    "gs_cand_init", "gs_cand_boot_pass", "gs_cand_tick_10ms", "gs_cand_cur", "gs_cand_reqmode",
    "gs_cand_fillst", "gs_cand_pf", "gs_cand_phase", "gs_cand_releasing", "gs_cand_wit",
    # this TU's own functions
    "g6a_init", "g6a_boot", "g6a_tick_10ms", "g6a_task_time", "g6a_state", "wire_cur", "idle_or_rel",
    "put_rec", "compare", "us16",
    # C keywords / operators that look like calls
    "if", "for", "while", "switch", "return", "sizeof", "defined", "_Static_assert",
}
CMP_PATTERNS = [
    r"\bP?CELL_[A-Z0-9_]+",
    # a non-const volatile cast of a literal address (a live write path)
    r"\(\s*volatile[^)]*\*\s*\)\s*(\(\s*uintptr_t\s*\)\s*)?\(?\s*0[xX]",
]

NM_ALLOW = {
    # the candidate's const calibration (app/gs_cal.c lookup, app/rq_fill_cal.c blobs)
    "gs_cal_lookup", "RQF_BLOBS",
    # the comparator's allowed getters (above) and libc/libgcc helpers
    "rq_prod_started", "rq_prod_ch", "rq_prod_fill_state", "drv_rq_fill_ok", "drv_out_wit",
    "gs_bridge_state", "plat_frt_us",
    "gs_shadow_cur", "gs_shadow_reqmode", "gs_shadow_fillst", "gs_shadow_pf", "gs_shadow_phase",
    "gs_shadow_releasing", "gs_shadow_pending", "gs_shadow_wit",
    "memset", "memcpy",
    # the candidate's own TUs call each other (engine <-> fill instance, comparator -> candidate)
}
NM_ALLOW_RE = [r"^gs_cand_", r"^gcf_", r"^__[a-z0-9_]+$", r"^_?_?gsc_obj_(start|end)$",
               r"^__stack_chk", r"^_GLOBAL_OFFSET_TABLE_$"]
STOCK_FORBIDDEN = ["e8bfa", "e8cf6", "d229e", "f71ec", "a2028", "a1712", "bddb0", "bd84e",
                   "6ab8e", "71594", "720c8", "b79e4", "a0e70", "6865c"]
LIT_FORBIDDEN = [0xFFF85174, 0xFFF85175, 0xFFF85176]


def strip(txt):
    txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
    txt = re.sub(r"//[^\n]*", " ", txt)
    return txt


def scan_outputs(f, txt, bad):
    if f.endswith(HOOK_FILE):
        decl = re.findall(r"extern\s+void\s+g6a_boot_pass\s*\(\s*void\s*\)\s*__attribute__\s*\(\(\s*weak\s*\)\)\s*;", txt)
        calls = re.findall(r"\bg6a_boot_pass\s*\(\s*\)\s*;", txt)
        if len(decl) != 1 or len(calls) != 1:
            bad.append("%s: want exactly one weak declaration and one call of g6a_boot_pass (got %d / %d)"
                       % (f, len(decl), len(calls)))
        txt = re.sub(r"\bg6a_boot_pass\b", " ", txt)
    for p in OUT_PATTERNS:
        for m in re.finditer(p, txt):
            bad.append("%s: %s" % (f, m.group(0)))


def scan_cmp(f, txt, bad):
    for inc in re.findall(r'#\s*include\s*"([^"]+)"', txt):
        if inc not in CMP_INCLUDES:
            bad.append("%s: #include \"%s\" (not allowed in the comparator)" % (f, inc))
    for p in CMP_PATTERNS:
        for m in re.finditer(p, txt):
            bad.append("%s: %s" % (f, m.group(0)))
    # every identifier followed by '(' that is not a cast / declaration of a known type
    # (preprocessor lines aside)
    code = re.sub(r"^[ \t]*#[^\n]*", " ", txt, flags=re.M)
    for m in re.finditer(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", code):
        name = m.group(1)
        if name in CMP_CALL_ALLOW:
            continue
        if re.fullmatch(r"(u?int(8|16|32)_t|bool|void|uintptr_t|unsigned|const|volatile|LRD8|LRD16|LIVE_MODE|"
                        r"LIVE_ST|LAW_I|C|G6A_SEC|__attribute__|section|uint8_t|uint16_t|uint32_t)", name):
            continue
        bad.append("%s: call of %s( (not an allowed read getter)" % (f, name))


def scan_link(nm, objs, bad):
    for o in objs:
        und = subprocess.run([nm, "-u", o], capture_output=True, text=True, check=True).stdout.split()
        und = [s for s in und if s not in ("U", "w", "v")]
        for s in und:
            name = s[1:] if s.startswith("_") and not s.startswith("__") else s
            if name in NM_ALLOW or any(re.match(r, name) or re.match(r, s) for r in NM_ALLOW_RE):
                continue
            bad.append("%s: undefined symbol %s (a call / reference outside the candidate's allowed set)" % (o, s))
        defs = subprocess.run([nm, o], capture_output=True, text=True, check=True).stdout.lower()
        for k in STOCK_FORBIDDEN:
            if re.search(r"\b_?[a-z]*%s\b" % k, defs):
                bad.append("%s: a symbol names the stock routine 0x%s (EEPROM / NVM / DSM / event log / fatal)"
                           % (o, k.upper()))
        objdump = nm.replace("nm", "objdump")
        dump = subprocess.run([objdump, "-s", o], capture_output=True, text=True).stdout.lower()
        hexes = "".join(re.findall(r"^ [0-9a-f]+ ((?:[0-9a-f]{2,8} ?){1,4})", dump, flags=re.M)).replace(" ", "")
        for lit in LIT_FORBIDDEN:
            be = "%08x" % lit
            le = "".join(reversed([be[i:i + 2] for i in range(0, 8, 2)]))
            if be in hexes or le in hexes:
                bad.append("%s: holds the EEPROM cell 0x%08X as a literal" % (o, lit))


def main():
    mode = sys.argv[1]
    bad = []
    if mode == "link":
        scan_link(sys.argv[2], sys.argv[3:], bad)
        n = len(sys.argv) - 3
    else:
        files = sys.argv[2:]
        n = len(files)
        for f in files:
            txt = strip(open(f, encoding="utf-8", errors="replace").read())
            if mode == "outputs":
                scan_outputs(f, txt, bad)
            else:
                scan_cmp(f, txt, bad)
    if bad:
        print("G6a read-only violation (%s):\n  %s" % (mode, "\n  ".join(bad)))
        return 1
    print("G6a read-only scan (%s): %d inputs clean" % (mode, n))
    return 0


if __name__ == "__main__":
    sys.exit(main())
