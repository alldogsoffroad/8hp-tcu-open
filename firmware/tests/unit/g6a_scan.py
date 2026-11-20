#!/usr/bin/env python3
"""g6a_scan.py - G6a: THE CANDIDATE AND ITS COMPARATOR ARE READ-ONLY (ours,
2026-11-19, ~/tcu_sweep/g6a_port.md). Comments are stripped before the
source scans.

  g6a_scan.py outputs FILE...   no output path may name the candidate or the
                                comparator (gs_cand*, gcf_*, g6a*, .gsc_*,
                                their RAM) - EXCEPT bsw/sup/sup_mode.c: the
                                one weak boot hook g6a_boot_pass (declared
                                weak, called once, under !TCU_RAM_APP)
  g6a_scan.py monitor FILE...   FILE checked by the G6b monitor's rule
                                (app/gs_bridge.c: candidate READ accessors
                                only, only inside `#if GS_G6B`) - the
                                negative fixture
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
  g6a_scan.py nvm NM OBJ...     F1: the fault-reaction shadow's objects name
                                no forbidden stock routine (STOCK_FORBIDDEN,
                                defined or referenced) and hold no EEPROM /
                                NVM / DSM-record / fatal-log cell
                                (LIT_FORBIDDEN_FR) and no address in the DSM
                                image 0xFFF81D58..0xFFF82E13 or the record
                                counts 0xFFF85114..17, as a data / literal
                                word or a movi20 operand
  g6a_scan.py image NM ELF      G5b (D-G5-7): no defined symbol of the WHOLE
  g6a_scan.py image --nm-text FILE   image names one of the stock routines
                                above (STOCK_FORBIDDEN) or a mode-2-only
                                function kept out (MODE2_FORBIDDEN: 0xE9340,
                                0xBCAD6, 0xF02EA / 0xF0D5C / 0xF0728, the WK
                                lock-up list 0xE0FE4 / 0xE105C / 0xE106E /
                                0xDEA5E, 0xD46A0, 0xCA65C, 0xD8122 / 0xD8C3A,
                                0xDD2C4) - any symbol containing the hex
  g6a_scan.py retired NM ELF    r04r: no defined symbol of the retired live
  g6a_scan.py retired --nm-text FILE   holder (rq_prod_*, its own fill's
                                rqf_*, its recorder / witnesses) in the image
Exit 1 ("G6a read-only violation") on any hit.
"""
import re
import subprocess
import sys

OUT_PATTERNS = [r"gs_cand", r"\bgcf_", r"\bg6a", r"\bG6A_", r"\bGSC_", r"\.gsc_(req|obj)\b",
                r"0[xX][fF]{3}925[cC]0", r"0[xX][fF]{3}93[cC]08"]
HOOK_FILE = "bsw/sup/sup_mode.c"
# G6b (2026-11-19, ~/tcu_sweep/g6b_port.md): THE MONITOR (app/gs_bridge.c) is
# the one place where the candidate reaches the output path (through the
# seam). It may include app/gs_cand.h and call ONLY the candidate's READ
# accessors below — and only inside `#if GS_G6B` blocks; nothing else of the
# candidate (no init / tick / boot pass, no gcf_*, GSC_*, g6a*, its RAM).
MONITOR_FILE = "app/gs_bridge.c"
MONITOR_READS = {"gs_cand_wit", "gs_cand_pf", "gs_cand_releasing", "gs_cand_pending", "gs_cand_cur",
                 "gs_cand_reqmode", "gs_cand_fillst", "gs_cand_phase"}

CMP_INCLUDES = {"app/g6a.h", "app/gs_cand.h", "app/gs_bridge.h",
                "bsw/drv/drv_reqlaw.h", "bsw/drv/drv_jobs.h", "platform/platform.h",
                # G6b: GS_G6B and the P content (DRV_RQ_PCONT_TABLE), macros only
                "bsw/drv/drv_gsbr.h"}
CMP_CALL_ALLOW = {
    # the live path, read getters only (r04r: the retired holder's getters
    # rq_prod_started / rq_prod_ch / rq_prod_fill_state are gone)
    "drv_out_wit",
    "gs_bridge_state", "plat_frt_us",
    # G6b: the monitor's mode getter (read-only)
    "gs_bridge_g6b",
    # instance 1 (the shadow) accessors
    "gs_shadow_cur", "gs_shadow_reqmode", "gs_shadow_fillst", "gs_shadow_pf", "gs_shadow_phase",
    "gs_shadow_releasing", "gs_shadow_pending", "gs_shadow_wit",
    # instance 2 (the candidate) - its own RAM
    "gs_cand_init", "gs_cand_boot_pass", "gs_cand_tick_10ms", "gs_cand_cur", "gs_cand_reqmode",
    "gs_cand_fillst", "gs_cand_pf", "gs_cand_phase", "gs_cand_releasing", "gs_cand_wit",
    # this TU's own functions
    "g6a_init", "g6a_boot", "g6a_boot_compare", "g6a_tick_10ms", "g6a_task_time", "g6a_state", "wire_cur", "idle_or_rel",
    "put_rec", "compare", "us16",
    # G6b: the split tick, the stage / time witnesses, the G6b comparison
    "g6a_stage", "g6a_cand_tick", "g6a_compare_10ms", "g6a_time", "compare_g6b",
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
    "drv_out_wit",
    "gs_bridge_state", "plat_frt_us",
    # G6b: the monitor's mode getter; the candidate's stage marks (app/g6a.c)
    "gs_bridge_g6b", "g6a_stage",
    "gs_shadow_cur", "gs_shadow_reqmode", "gs_shadow_fillst", "gs_shadow_pf", "gs_shadow_phase",
    "gs_shadow_releasing", "gs_shadow_pending", "gs_shadow_wit",
    "memset", "memcpy",
    # the candidate's own TUs call each other (engine <-> fill instance, comparator -> candidate)
}
NM_ALLOW_RE = [r"^gs_cand_", r"^gcf_", r"^__[a-z0-9_]+$", r"^_?_?gsc_obj_(start|end)$",
               r"^__stack_chk", r"^_GLOBAL_OFFSET_TABLE_$"]
STOCK_FORBIDDEN = ["e8bfa", "e8cf6", "d229e", "f71ec", "a2028", "a1712", "bddb0", "bd84e",
                   "6ab8e", "71594", "720c8", "b79e4", "a0e70", "6865c",
                   # F1 (2026-11-19, ~/tcu_sweep/fault_reaction_gearcode_plan.md §5 and
                   # f0_fault_chain_decode.md §7): the DSM record / aging / counters /
                   # UDS cycle / clear / integrity / init, the event log, the fatal
                   # handler and the safety-layer fatal pair, the NV engines and the
                   # warm-start signature — none may be defined in the image
                   "ac324", "9ce52", "ac550", "a0074", "ab79c", "ac114", "a0db4", "ac8fe",
                   "ac97e", "a0816", "9fa5e", "a086a", "ac570", "9f7aa", "abf28", "abf40",
                   "b767e", "bc912", "bc9c0", "bc9e2", "bcc00", "6a86c", "ae906",
                   "abf98", "9d0ca", "9cff0", "9cfc4", "9da1a", "9d232", "9ccae", "9ccf6",
                   "9d090", "ab7b6", "ac07a", "a016a", "a0bd6", "68736", "66f82", "7a914",
                   "794c8",
                   # (2026-11-20, ~/tcu_sweep/gp_shadow_port.md): the gear-pair shift
                   # engine's NV writers — the end 0xEBB6C's shift counter 0xBA538 →
                   # 0xE8774 → 0xE82AC → 0xE8512, the gear-pair end 0xEBC60's counter
                   # 0xBA52C → 0xE874A → 0xE82A4 → 0xE8512, the park-qualifier event
                   # 0xBA54C → 0xE8842, and the shift adaptation learners (slot 41
                   # 0xA30CC → 0xAEE7A → 0xCA3A6 → 0xB45E0 → 0xCC9EE → 0xCDF42 /
                   # 0xCDBB4 / 0xA17CC; the E32A kiss path 0xDD400 → 0xAEB9A → 0xB59F8 →
                   # 0xB68D0; 0xCB7EC, 0xCBD0E, 0x10338C → 0x125308, 0x1036E0 →
                   # 0x125364) — none may be defined in the image
                   "ba538", "ba52c", "e8774", "e874a", "e82ac", "e82a4", "e8512", "ba54c", "e8842",
                   "aee7a", "ca3a6", "b45e0", "cc9ee", "cdf42", "cdbb4", "a17cc", "dd400", "aeb9a",
                   "b59f8", "b68d0", "cb7ec", "cbd0e", "10338c", "125308", "1036e0", "125364",
                   # (2026-11-20, ~/tcu_sweep/group28_port.md): the TOE32 test's keep-outs —
                   # the per-output fault-record store 0x87120 (complement + fatal 0x224 /
                   # 0x226), the counter 0x86414 (0xFFF939BA), the setter's supplier
                   # occurrence table 0x7CA24 (0xFFF948B8..)
                   "87120", "86414", "7ca24"]
LIT_FORBIDDEN = [0xFFF85174, 0xFFF85175, 0xFFF85176]
# F1: the EEPROM / NVM / DSM-record / fatal-log cells no fault-reaction shadow
# object may hold as a literal or a movi20 operand (plan §5, f0 §7)
LIT_FORBIDDEN_FR = LIT_FORBIDDEN + [0xFFF8E613, 0xFFF8BA30, 0xFFF8E621, 0xFFF8E622, 0xFFF89BEC, 0xFFF80D24, 0xFFF800E6,
                                    0xFFF800ED, 0xFFF80E0F, 0xFFF82EF4, 0xFFF82F78, 0xFFF82F79,
                                    0xFFF830AC, 0xFFF830B0, 0xFFF830F0, 0xFFF897E8, 0xFFF8E57A,
                                    0xFFF8E575, 0xFFF8E577]
# the DSM image (memory 1 / the ring / the pointer / the UDS status bytes) and
# the record counts: no literal or movi20 operand inside
RANGE_FORBIDDEN_FR = [(0xFFF81D58, 0xFFF82E13), (0xFFF85114, 0xFFF85117),
                      # groups 27 / 28 (2026-11-20): the stock cells the TOE32 test models
                      # — the shadow keeps its own copies in .fr_toe, never names these:
                      # the countdowns 0x94828..31, 0x94147..4C (cut flag, first call),
                      # 0x93A7A..83 (duty category), the fault / tested bits 0x938F6..
                      # 0x93909, the copies 0x9497E..91, the code 0x940D4..D8, the
                      # summaries 0x940BE..BF, 0x94175..79, the fault occurrence table
                      # 0x94890..0x94957 (0x7CA24), the counter 0xFFF939BA
                      (0xFFF94828, 0xFFF94831), (0xFFF94147, 0xFFF9414C), (0xFFF93A7A, 0xFFF93A83),
                      (0xFFF938F6, 0xFFF93909), (0xFFF9497E, 0xFFF94991), (0xFFF940D4, 0xFFF940D8),
                      (0xFFF940BE, 0xFFF940BF), (0xFFF94175, 0xFFF94179), (0xFFF94890, 0xFFF94957),
                      (0xFFF939BA, 0xFFF939BB)]
# G5b (2026-11-19, ~/tcu_sweep/g5b_port.md, D-G5-7): the stock's mode-2-only
# functions stay out of the image (with STOCK_FORBIDDEN's 0xE8CF6 / 0xD229E /
# 0xE8BFA / 0xF71EC): the bulk-save hook 0xE9340, the record write-back
# 0xBCAD6, the ratio / slip monitor 0xF02EA (+ 0xF0D5C, 0xF0728), the WK
# lock-up active list 0xE0FE4 / 0xE105C / 0xE106E and its engagement SM
# 0xDEA5E, 0xD46A0, 0xCA65C, the restart sequencer 0xD8122 / 0xD8C3A, 0xDD2C4
MODE2_FORBIDDEN = ["e9340", "bcad6", "f02ea", "f0d5c", "f0728", "e0fe4", "e105c", "e106e", "dea5e",
                   "d46a0", "ca65c", "d8122", "d8c3a", "dd2c4"]


def strip(txt):
    txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
    txt = re.sub(r"//[^\n]*", " ", txt)
    return txt


def monitor_reads(f, txt, bad):
    """the monitor's candidate references: each inside an `#if GS_G6B` block,
    each a READ accessor (or the header); returns the text without them"""
    keep = []
    stack = []
    for line in txt.split("\n"):
        m = re.match(r"^[ \t]*#[ \t]*(if|ifdef|ifndef|elif|else|endif)\b(.*)$", line)
        if m:
            d, rest = m.group(1), m.group(2).strip()
            if d in ("if", "ifdef", "ifndef"):
                stack.append((d, rest))
            elif d in ("elif", "else") and stack:
                stack[-1] = (d, rest)
            elif d == "endif" and stack:
                stack.pop()
        in_g6b = any(d == "if" and re.match(r"GS_G6B\b", r) for d, r in stack)
        refs = re.findall(r"\bgs_cand_[a-z0-9_]+|app/gs_cand\.h", line)
        for ref in refs:
            if not in_g6b:
                bad.append("%s: %s outside an `#if GS_G6B` block" % (f, ref))
            elif ref != "app/gs_cand.h" and ref not in MONITOR_READS:
                bad.append("%s: %s (the monitor may only READ the candidate)" % (f, ref))
        keep.append(re.sub(r"\bgs_cand_[a-z0-9_]+|app/gs_cand\.h", " ", line))
    return "\n".join(keep)


def scan_outputs(f, txt, bad):
    if f.endswith(HOOK_FILE):
        decl = re.findall(r"extern\s+void\s+g6a_boot_pass\s*\(\s*void\s*\)\s*__attribute__\s*\(\(\s*weak\s*\)\)\s*;", txt)
        calls = re.findall(r"\bg6a_boot_pass\s*\(\s*\)\s*;", txt)
        if len(decl) != 1 or len(calls) != 1:
            bad.append("%s: want exactly one weak declaration and one call of g6a_boot_pass (got %d / %d)"
                       % (f, len(decl), len(calls)))
        txt = re.sub(r"\bg6a_boot_pass\b", " ", txt)
    if f.replace("\\", "/").endswith(MONITOR_FILE):
        txt = monitor_reads(f, txt, bad)
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


def scan_nvm(nm, objs, bad):
    """F1: the fault-reaction shadow's objects (and the negative fixture): no
    symbol (defined or referenced) names a forbidden stock routine, no data /
    literal word is a forbidden cell or inside a forbidden range, no movi20
    operand either (the shadow keeps its own RAM)."""
    objdump = nm.replace("nm", "objdump")
    for o in objs:
        names = [ln.split()[-1] for ln in subprocess.run([nm, o], capture_output=True, text=True,
                                                         check=True).stdout.lower().splitlines() if ln.split()]
        for k in STOCK_FORBIDDEN:
            if any(k in nmx for nmx in names):
                bad.append("%s: a symbol names the stock routine 0x%s (EEPROM / NVM / DSM record / event log / fatal)"
                           % (o, k.upper()))
        dump = subprocess.run([objdump, "-s", o], capture_output=True, text=True).stdout.lower()
        words = []
        for m in re.finditer(r"^ ([0-9a-f]+) ((?:[0-9a-f]{2,8} ?){1,4})", dump, flags=re.M):
            for chunk in m.group(2).split():
                if len(chunk) == 8:             # one 4-byte aligned word of the section
                    words.append(int(chunk, 16))
        dis = subprocess.run([objdump, "-d", o], capture_output=True, text=True).stdout
        for m in re.finditer(r"movi20\s+#(-?\d+)", dis):
            words.append(int(m.group(1)) & 0xFFFFFFFF)
        for w in set(words):
            if w in LIT_FORBIDDEN_FR:
                bad.append("%s: holds the EEPROM / NVM / DSM cell 0x%08X (a literal or movi20)" % (o, w))
            for (lo, hi) in RANGE_FORBIDDEN_FR:
                if lo <= w <= hi:
                    bad.append("%s: holds 0x%08X inside a forbidden range (the DSM image / record counts / the TOE32 test's stock cells) 0x%08X..0x%08X"
                               % (o, w, lo, hi))


def scan_image(lines, src, bad):
    for ln in lines:
        p = ln.split()
        if len(p) < 3 or p[1] in ("U", "w", "v"):
            continue
        name = p[2].lower()
        for k in STOCK_FORBIDDEN + MODE2_FORBIDDEN:
            if k in name:
                bad.append("%s: defines %s (the stock routine 0x%s must stay out: EEPROM / NVM / mode-2-only)"
                           % (src, p[2], k.upper()))


# r04r (~/tcu_sweep/r04r_port.md): THE RETIRED HOLDER STAYS OUT OF THE IMAGE.
# `retired NM ELF` (POST_BUILD of tcu_app.elf) / `retired --nm-text FILE`
# (the fixtures): no defined symbol of the live holder (app/rq_prod.c) or of
# its own fill instance (app/rq_fill.c compiled as itself: rqf_*, not the
# engine's renamed gcf_*) may be in the image.
RETIRED_RE = [r"^_?rq_prod_", r"^_?rqf_", r"^_?s_m5_(rec|pass|unp_n|f19a_n|f19f_n|f1c4_n)", r"^_?s_fill_live$"]


def scan_retired(lines, src, bad):
    for ln in lines:
        p = ln.split()
        if len(p) < 3 or p[1] in ("U", "w", "v"):
            continue
        name = p[2]
        if any(re.match(r, name) for r in RETIRED_RE):
            bad.append("%s: defines %s (the retired live holder is back in the image)" % (src, name))


def main():
    mode = sys.argv[1]
    bad = []
    if mode in ("retired", "image"):
        if sys.argv[2] == "--nm-text":
            lines = open(sys.argv[3]).read().splitlines()
        else:
            lines = subprocess.run([sys.argv[2], sys.argv[3]], capture_output=True, text=True,
                                   check=True).stdout.splitlines()
        if mode == "retired":
            scan_retired(lines, sys.argv[3], bad)
        else:
            scan_image(lines, sys.argv[3], bad)
        n = 1
    elif mode == "link":
        scan_link(sys.argv[2], sys.argv[3:], bad)
        n = len(sys.argv) - 3
    elif mode == "nvm":
        scan_nvm(sys.argv[2], sys.argv[3:], bad)
        n = len(sys.argv) - 3
    else:
        files = sys.argv[2:]
        n = len(files)
        for f in files:
            txt = strip(open(f, encoding="utf-8", errors="replace").read())
            if mode == "outputs":
                scan_outputs(f, txt, bad)
            elif mode == "monitor":
                monitor_reads(f, txt, bad)     # the monitor's rule on FILE (the negative fixture)
            else:
                scan_cmp(f, txt, bad)
    if bad:
        print("G6a read-only violation (%s):\n  %s" % (mode, "\n  ".join(bad)))
        return 1
    print("G6a read-only scan (%s): %d inputs clean" % (mode, n))
    return 0


if __name__ == "__main__":
    sys.exit(main())
