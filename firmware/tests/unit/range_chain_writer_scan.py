#!/usr/bin/env python3
"""range_chain_writer_scan.py - THE RANGE WRITER GUARD (ours, 2026-11-17,
~/tcu_sweep/range_chain_live_port.md).

Only app/rng_chain.c (with its header app/rng_chain.h and the read-only cell
header app/park_cells.h) may name the range cells D6D6..D6D9, the init
arbiter's E549..E54B, the published range object 0xFFF8304C..63, the chain's
pinned objects, the writer macros PCELL_*_W, or define TCU_RNG_CHAIN_TU (which
unlocks the writers). Every other firmware source is scanned with its
comments stripped. Also: the request producer, the request law / bus A and
the supervisor (app/rq_*, bsw/drv/*, bsw/sup/*) may not read the range or the
park path's cells. Exit 1 (and "range chain writer guard violation") on any hit.

  range_chain_writer_scan.py FIRMWARE_DIR [--only FILE ...]
"""
import os
import re
import sys

ALLOWED = {"app/rng_chain.c", "app/rng_chain.h", "app/park_cells.h"}
DIRS = ("app", "bsw", "safety", "io", "platform", "loader", "flasher", "include")
PATTERNS = [
    r"0[xX][fF]{3}8[dD]6[dD][6-9]",          # D6D6..D6D9
    r"0[xX][fF]{3}8[eE]54[9a-bA-B]",         # E549..E54B
    r"0[xX][fF]{3}830[4-6][0-9a-fA-F]",      # 304C..3063
    r"\bPCELL_(TGT|TGT_C|ARB)_W\b",
    r"#\s*define\s+TCU_RNG_CHAIN_TU\b",
    r"\b(rc|RC)_(d6ca|D6CA|e530|E530|304c|304C)\b",
    r"\bRC_ADDR_",
]


# the request producer, the request law / bus A and the supervisor must not
# read the range or the park path's stock cells at runtime (the producer's
# AE29 is the compile-time RQ_M5_PARKREQ, asserted in app/rq_prod.h); their
# only park inputs are the slot getters park_slot7_release /
# park_slot11_hold (bsw/drv/drv_park.h)
READER_GLOBS = ("app/rq_", "bsw/drv/", "bsw/sup/")
READER_PATTERNS = [
    r"\bPCELL_", r"park_cells\.h", r"\brng_chain", r"\bpark_status\b",
    r"0[xX][fF]{3}89[cC][dD][cC]", r"0[xX][fF]{3}8[aA][eE]2[89]", r"0[xX][fF]{3}89[cC]6[cC]",
]


def strip_comments(t):
    t = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    return re.sub(r"//[^\n]*", " ", t)


def main():
    root = sys.argv[1]
    if "--only" in sys.argv:
        files = sys.argv[sys.argv.index("--only") + 1:]
    else:
        files = []
        for d in DIRS:
            for dp, _, fn in os.walk(os.path.join(root, d)):
                for f in fn:
                    if f.endswith((".c", ".h", ".S")):
                        files.append(os.path.join(dp, f))
    bad = []
    for f in sorted(files):
        rel = os.path.relpath(f, root)
        if rel in ALLOWED and "--only" not in sys.argv:
            continue
        txt = strip_comments(open(f, encoding="utf-8", errors="replace").read())
        for p in PATTERNS:
            if re.search(p, txt):
                bad.append(f"{rel}: {p}")
        if rel.startswith(READER_GLOBS) or "--only" in sys.argv:
            for p in READER_PATTERNS:
                if re.search(p, txt):
                    bad.append(f"{rel}: reads the range / park path: {p}")
    if bad:
        print("range chain writer guard violation:\n  " + "\n  ".join(bad))
        return 1
    print(f"range chain writer guard: {len(files)} sources clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
