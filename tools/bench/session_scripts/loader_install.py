#!/usr/bin/env python3
"""loader_install.py — install the M5 data-flash loader into code flash (spec 18).

Target: build/m5_loader_IFla.bin vs current flash (pristine stock). The plan
(erase blocks + program units) is computed from the diff at start; every step
is resume-safe: op-01 spot reads decide erase/skip and program/skip, so the
script can be re-run with --resume (resident engine, no power cycle) after a
stall, or fresh after a power cycle.

Erase grammar proven at 0xE0000 (round 54); blocks >=0x100000 (FENTRY1 split,
spec 16 §11) get the standard op-20 first, then two fallback forms, each
verified by an op-01 blank check before proceeding.
"""
import sys, time
sys.path.insert(0, '/home/chadk/Documents/acdp2-re/tools')
sys.path.insert(0, '/home/chadk/Documents/REA Projects/tools/bench')
sys.path.insert(0, "/tmp")
import tcu_write as tw
from tcu_write import crc16, ck7

BASE = "/home/chadk/Documents/REA Projects/260924131233 1034420267_IFla.bin"
TGT  = "/home/chadk/Documents/REA Projects/build/m4u_fivesel_IFla.bin"
LO, HI = 0x40000, 0x180000

base = open(BASE, "rb").read()
tgt = open(TGT, "rb").read()

def erase_blocks():
    a = LO
    while a < HI:
        sz = 0x10000 if a < 0xA0000 else 0x20000
        yield a, min(a + sz, HI)
        a += sz

def ck7_(h): return h + bytes([(sum(h) + 0x55) & 0xFF])

# ---------------- plan ----------------
plan_blocks = []          # (start, end) needing erase vs base
plan_units = []           # (offset, data256) needing program vs base
for s, e in erase_blocks():
    if base[s:e] != tgt[s:e]:
        plan_blocks.append((s, e))
        for o in range(s, e, 0x100):
            u = tgt[o:o+0x100]
            if u != b"\xff" * 0x100 and base[o:o+0x100] != u:
                plan_units.append((o, u))
tw.tws.ts(f"plan: {len(plan_blocks)} erase blocks, {len(plan_units)} program units")

# ARM with the 1 MB MAT split key (spec 16 §11, restore_body pattern)
def arm_key_frames(offset):
    key = 1 if offset < 0x100000 else 2
    cf = (0x80800000 + offset).to_bytes(4, "big") + bytes([0x00, 0x01, 0xE8, key])
    ff = ck7_(bytes([0x11, 0x10, 0x08, 0x00]) + crc16(cf).to_bytes(2, "big") + bytes([0x00]))
    return ff, cf

def arm_program(t, offset, data256):
    ff, cf = arm_key_frames(offset)
    reps = t.cmd("ARM", [ff, cf])
    if not reps or reps[0][0] != 0x51:
        raise RuntimeError(f"ARM {offset:#x} refused: {reps}")
    reps = t.cmd("program", tw.compose_program(data256), tmo=8.0)
    if not reps or reps[0][0] != 0x52:
        raise RuntimeError(f"program {offset:#x} refused: {reps}")

def read4(t, addr):
    """op-01 default-view 4-byte read (restore_body pattern)."""
    t.sess.send_pt(ck7_(bytes([0x01, 0x05]) + addr.to_bytes(4, "big") + bytes([4])))
    r = t.sess.recv_pt(timeout=2.0)
    return int.from_bytes(r[2:6], "big") if r and r[0] == 0x41 else None

def read4r(t, addr, tries=4):
    """read4 with retries — the FCU stays busy for a while after a real erase."""
    for i in range(tries):
        v = read4(t, addr)
        if v is not None:
            return v
        time.sleep(2.5)
    return None

def fmt(v):
    return "none" if v is None else f"{v:08x}"

def block_discriminator(s, e):
    """First offset in the block where target differs from the stock base —
    the header magic is identical in both, so the block start cannot tell
    'ours' from 'stock'."""
    for o in range(s, e, 4):
        if tgt[o:o+4] != base[o:o+4]:
            return o
    return s

def erase_variants(t, block):
    """Standard op-20 (the only proven erase form); for the FENTRY1 split
    blocks >=0x100000, a key-2 ARM precedes the erase. Returns True on 60."""
    if block >= 0x100000:
        ff, cf = arm_key_frames(block)
        t.cmd("ARM(pre-erase key)", [ff, cf])
    reps = t.cmd(f"erase {block:#x}", list(tw.compose_erase(block)), tmo=8.0)
    if reps and reps[0][0] == 0x60:
        if reps[0][1] & 0x80:
            tw.tws.ts("  real erase: busy-wait 2.5s")
            time.sleep(2.5)
        return True
    tw.tws.ts(f"  refused: {[r.hex() for r in reps]}")
    return False

# ---------------- session ----------------
t = tw.TCU()
if '--resume' in sys.argv:
    import tcu_bench as _tb
    import importlib.util as _ilu
    _spec = _ilu.spec_from_file_location('tws', '/tmp/teensy_write_session.py')
    tws = _ilu.module_from_spec(_spec); sys.modules['tws'] = tws
    try: _spec.loader.exec_module(tws)
    except SystemExit: pass
    _img = open(tw.ENGINE, 'rb').read()
    t.b = _tb.Buses(tws.A())
    t.n = 1
    t.sess = tws.ToolSession(t.b, t.n, _img)
    t.sess.handshake()
    tw.tws.ts('resumed ToolSession (resident engine)')
else:
    t.chain()
tw.tws.ts("*** LOADER INSTALL: erase phase (stock app is invalidated from the first "
          "erase; recovery = finish this script or restore stock) ***")

# ---------------- erase phase ----------------
# op-01 reads serve a stale cache after erase-only ops (the priming ladder
# cached the stock bytes); the op-08 region checksum is the fresh oracle —
# content-only (two distinct blank 64K regions return the same signature).
# 64K-blank and 128K-blank signatures measured on the bench this session;
# 128K value is the tool's own constant (spec 17 §5l).
BLANK_SIGS = {0x10000: "de547e4e81cb0165", 0x20000: "14b603cca7a12f57"}

def verify_region(t, start, size, tries=5):
    cf = start.to_bytes(4, "big") + size.to_bytes(4, "big")
    ff = ck7_(bytes([0x08, 0x10, 0x08, 0x00]) + crc16(cf).to_bytes(2, "big") + bytes([0x00]))
    for i in range(tries):
        reps = t.cmd(f"verify {start:#x}/{size:#x}", [ff, cf], nreply=2, tmo=4.0)
        if reps and reps[0][0] == 0x48:
            return reps
        tw.tws.ts(f"  verify busy/pending ({[r.hex() for r in reps]}), retry {i + 1}")
        time.sleep(2.5)
    return reps

def region_sig(t, start, size):
    reps = verify_region(t, start, size)
    return reps[-1].hex() if reps else None

def region_blank(t, start, size=None):
    if size is None:
        size = 0x10000 if start < 0xA0000 else 0x20000
    sig = region_sig(t, start, size)
    return sig == BLANK_SIGS[size]

erased = 0
for s, e in plan_blocks:
    blk_blank = tgt[s:e] == b"\xff" * (e - s)
    if blk_blank:
        if region_blank(t, s):
            tw.tws.ts(f"block {s:#x}: op-08 blank — skipping erase")
            erased += 1; continue
    else:
        # content block: the CRC field at 0x40044 discriminates ours vs stock
        d = block_discriminator(s, e)
        cur = read4r(t, d)
        tw.tws.ts(f"block {s:#x}: disc@{d:#x} cur={fmt(cur)} tgt={tgt[d:d+4].hex()}")
        if cur == int.from_bytes(tgt[d:d+4], "big"):
            tw.tws.ts("  discriminator matches target — already ours, skipping erase")
            erased += 1; continue
        # not ours and nothing programmed (units go ascending from block start)
        # — a stale stock read here is harmless: the erase is idempotent
    if not erase_variants(t, s):
        tw.tws.ts(f"*** block {s:#x} erase FAILED — stop cleanly; resume with --resume "
                  f"or after a power cycle ***")
        sys.exit(2)
    if blk_blank:
        ok = False
        for i in range(6):
            if region_blank(t, s):
                ok = True; break
            tw.tws.ts(f"  blank-check retry {i + 1}")
            time.sleep(2.5)
        if not ok:
            tw.tws.ts(f"*** block {s:#x} NOT BLANK by op-08 after erase — ABORT ***")
            sys.exit(2)
    erased += 1
tw.tws.ts(f"erase phase done: {erased}/{len(plan_blocks)} blocks")

# ---------------- program phase ----------------
done = 0
t0 = time.time()
for off, unit in plan_units:
    got = read4r(t, off)
    if got is not None and got == int.from_bytes(unit[:4], "big"):
        done += 1; continue                      # already programmed
    ok = False
    for attempt in range(6):
        try:
            arm_program(t, off, unit)
            ok = True; break
        except RuntimeError as ex:
            tw.tws.ts(f"  {ex} — retry {attempt + 1}")
            time.sleep(3.0)
    if not ok:
        tw.tws.ts(f"*** unit {off:#x} REFUSED — stop cleanly; resume with --resume "
                  f"(or power cycle + fresh chain) at this offset ***")
        sys.exit(2)
    done += 1
    if done % 10 == 0:
        tw.tws.ts(f"  {done}/{len(plan_units)} units ({time.time()-t0:.0f}s)")
tw.tws.ts(f"program phase done: {done}/{len(plan_units)} units in {time.time()-t0:.0f}s")

# ---------------- verify phase ----------------
# op-01 reads serve the stale (post-erase) cache after program ops; one op-08
# resets the read path (spec 17 section 5l) — send one before trusting op-01.
verify_region(t, 0x40000, 0x10000)
time.sleep(1.0)
bad = 0
for off, unit in plan_units:
    got = read4r(t, off)
    want = int.from_bytes(unit[:4], "big")
    if got != want:
        bad += 1
        tw.tws.ts(f"  VERIFY DIFF {off:#x}: {fmt(got)} vs {want:08x}")
for s, e in plan_blocks:
    if tgt[s:e] == b"\xff" * (e - s):
        got = read4r(t, s)
        if got != 0xFFFFFFFF:
            bad += 1
            tw.tws.ts(f"  VERIFY NOT BLANK {s:#x}: {fmt(got)}")
mk = read4r(t, 0x17FE00)
tw.tws.ts(f"marker1 0x17FE00: {fmt(mk)}")
if bad:
    tw.tws.ts(f"*** {bad} VERIFY DIFFS — do NOT power off yet; re-run failing units ***")
    sys.exit(1)
tw.tws.ts("*** LOADER INSTALL COMPLETE — power-cycle the TCU: it must come up in "
          "loader mode (uds 22 F1 95 -> 'LD'), then app-flash tcu_app_df.bin ***")
