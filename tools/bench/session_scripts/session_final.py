#!/usr/bin/env python3
"""Final session: boot shadows + blank-oracle sweep + guaranteed-fresh rewrite
of every non-blank m4v unit (all ops proven tonight). --resume opens the
ToolSession on the resident engine."""
import sys, time
sys.path.insert(0, '/home/chadk/Documents/acdp2-re/tools')
sys.path.insert(0, '/home/chadk/Documents/REA Projects/tools/bench')
sys.path.insert(0, "/tmp")
import tcu_bench as tb
import tcu_write as tw
from tcu_write import crc16
import importlib.util as ilu
spec = ilu.spec_from_file_location('tws', '/tmp/teensy_write_session.py')
tws = ilu.module_from_spec(spec); sys.modules['tws'] = tws
try: spec.loader.exec_module(tws)
except SystemExit: pass
img = open(tw.ENGINE, 'rb').read()
TGT = "/home/chadk/Documents/REA Projects/build/m4v_ioread_IFla.bin"
tgt = open(TGT, "rb").read()
def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)
def ck7_(h): return h + bytes([(sum(h) + 0x55) & 0xFF])

BLANK = {0x10000: "de547e4e81cb0165", 0x20000: "14b603cca7a12f57"}
UNITS = [o for o in range(0x40000, 0x180000, 0x100) if tgt[o:o+0x100] != b"\xff"*0x100]

b = tb.Buses(tws.A())
t = tw.TCU(); t.b = b; t.n = 1
t.sess = tws.ToolSession(b, 1, img)
t.sess.handshake()
ts("ToolSession up")

def read4(addr, tries=2):
    for i in range(tries):
        t.sess.send_pt(ck7_(bytes([0x01, 0x05]) + addr.to_bytes(4, "big") + bytes([4])))
        r = t.sess.recv_pt(timeout=2.0)
        if r and r[0] == 0x41:
            return int.from_bytes(r[2:6], "big")
        time.sleep(1.0)
    return None

# ---- 1. boot shadows ----
ts("--- boot state shadows ---")
for a in range(0xFFF93B30, 0xFFF93B70, 4):
    v = read4(a)
    ts(f"  {a:#x} = {v:08x}" if v is not None else f"  {a:#x} = none")
for a in (0xFFF94710, 0xFFF94711 - 0xFFF94711 + 0xFFF94710, ):
    pass
for a in (0xFFF94710, 0xFFF94714):
    v = read4(a)
    ts(f"  {a:#x} = {v:08x}" if v is not None else f"  {a:#x} = none")

# ---- 2. FWE probe (fixed check) ----
ts("--- FWE probe ---")
ff, cf = tw.compose_arm(0x40000)
reps = t.cmd("ARM probe", [ff, cf])
fer = read4(0xFFFFA902)
ts(f"FENTRYR after ARM: {fer:#06x}" if fer is not None else "no readback")
if fer is None or (fer & 0x03) == 0:
    ts("*** FWE LOW — stop ***"); b.shutdown(); sys.exit(3)

# ---- 3. blank-oracle sweep ----
ts("--- blank sweep (blocks that must be blank) ---")
def region_sig(start, size, tries=4):
    for i in range(tries):
        reps = t.cmd(f"verify {start:#x}/{size:#x}", tw.compose_verify(start, size), nreply=2, tmo=4.0)
        if reps and reps[0][0] == 0x48:
            return reps[-1].hex()
        ts("  busy, retry"); time.sleep(2.5)
    return None
bad = 0
for s in (0x50000, 0x60000, 0x70000, 0x80000, 0x90000):
    sig = region_sig(s, 0x10000)
    ok = sig == BLANK[0x10000]
    bad += not ok
    ts(f"  {s:#x}/64K: {sig} {'BLANK OK' if ok else '*** NOT BLANK ***'}")
for s in (0xA0000, 0xC0000, 0xE0000, 0x100000, 0x120000, 0x140000):
    sig = region_sig(s, 0x20000)
    ok = sig == BLANK[0x20000]
    bad += not ok
    ts(f"  {s:#x}/128K: {sig} {'BLANK OK' if ok else '*** NOT BLANK ***'}")
ts(f"blank sweep: {bad} anomalies")

# ---- 4. fresh rewrite: erase the two content blocks, rewrite ALL units ----
ts(f"--- fresh rewrite: erase 0x40000 + 0x160000, program {len(UNITS)} units ---")
for blk in (0x40000, 0x160000):
    ff, cf = tw.compose_erase(blk)
    reps = t.cmd(f"erase {blk:#x}", [ff, cf], tmo=8.0)
    if not (reps and reps[0][0] == 0x60):
        ts(f"*** erase {blk:#x} refused: {[r.hex() for r in reps]} ***"); b.shutdown(); sys.exit(4)
    if reps[0][1] & 0x80:
        ts("  busy 2.5s"); time.sleep(2.5)

def arm_key_frames(offset):
    key = 1 if offset < 0x100000 else 2
    cf = (0x80800000 + offset).to_bytes(4, "big") + bytes([0x00, 0x01, 0xE8, key])
    ff = ck7_(bytes([0x11, 0x10, 0x08, 0x00]) + crc16(cf).to_bytes(2, "big") + bytes([0x00]))
    return ff, cf

done = 0; t0 = time.time()
for off in UNITS:
    unit = tgt[off:off+0x100]
    ok = False
    for attempt in range(4):
        try:
            ff, cf = arm_key_frames(off)
            reps = t.cmd("ARM", [ff, cf])
            if not (reps and reps[0][0] == 0x51):
                raise RuntimeError(f"ARM {off:#x}: {[r.hex() for r in reps]}")
            reps = t.cmd("program", tw.compose_program(unit), tmo=8.0)
            if not (reps and reps[0][0] == 0x52):
                raise RuntimeError(f"PGM {off:#x}: {[r.hex() for r in reps]}")
            ok = True; break
        except RuntimeError as e:
            ts(f"  {e} retry {attempt+1}"); time.sleep(2.0)
    if not ok:
        ts(f"*** unit {off:#x} FAILED ***"); b.shutdown(); sys.exit(5)
    done += 1
    if done % 40 == 0: ts(f"  {done}/{len(UNITS)} ({time.time()-t0:.0f}s)")
ts(f"programmed {done}/{len(UNITS)} in {time.time()-t0:.0f}s")

# ---- 5. verify: op-08 cache reset + first AND last word of every unit ----
region_sig(0x40000, 0x10000)
time.sleep(1.0)
vbad = 0
for off in UNITS:
    for a, want in ((off, tgt[off:off+4]), (off + 0xFC, tgt[off+0xFC:off+0x100])):
        got = read4(a)
        if got != int.from_bytes(want, "big"):
            vbad += 1
            ts(f"  VERIFY DIFF {a:#x}: {got:08x} vs {int.from_bytes(want,'big'):08x}")
ts(f"verify: {vbad} diffs across {len(UNITS)} units (first+last word each)")
for a in (0x40044, 0x17FBFC, 0x17FE00, 0x17FF00):
    v = read4(a)
    ts(f"final {a:#x} = {v:08x}")
ts("*** SESSION FINAL COMPLETE — power-cycle for boot ***")
b.shutdown()
