#!/usr/bin/env python3
"""m4v2 installer (stock-body + m4v app). Sig-first resume: per erase block,
one op-08 vs the offline table decides skip/blank/partial. Airtight verify:
every block's live signature must equal build/m4v2's table at the end."""
import sys, time, json
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
TGT = "/home/chadk/Documents/REA Projects/build/m4v2_stockbody_IFla.bin"
tgt = open(TGT, "rb").read()
SIGS = json.load(open("/tmp/m4v2_sigs.json"))
BLANK = {0x10000: "de547e4e81cb0165", 0x20000: "14b603cca7a12f57"}
def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)
def ck7_(h): return h + bytes([(sum(h) + 0x55) & 0xFF])

def blocks():
    a = 0x40000
    while a < 0x180000:
        sz = 0x10000 if a < 0xA0000 else 0x20000
        yield a, sz; a += sz

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

def region_sig(start, size, tries=4):
    for i in range(tries):
        reps = t.cmd(f"verify {start:#x}/{size:#x}", tw.compose_verify(start, size), nreply=2, tmo=4.0)
        if reps and reps[0][0] == 0x48:
            return reps[-1].hex()
        ts("  busy, retry"); time.sleep(2.5)
    return None

# ---- FWE probe first ----
ff, cf = tw.compose_arm(0x40000)
reps = t.cmd("ARM probe", [ff, cf])
fer = read4(0xFFFFA902)
if fer is None or (fer & 0x03) == 0:
    ts(f"*** FWE LOW ({fer}) — re-power and rerun ***"); b.shutdown(); sys.exit(3)
ts(f"FWE high (FENTRYR={fer:#x})")

# ---- block pass ----
def arm_key_frames(offset):
    key = 1 if offset < 0x100000 else 2
    cf = (0x80800000 + offset).to_bytes(4, "big") + bytes([0x00, 0x01, 0xE8, key])
    ff = ck7_(bytes([0x11, 0x10, 0x08, 0x00]) + crc16(cf).to_bytes(2, "big") + bytes([0x00]))
    return ff, cf

written = skipped_blocks = 0
t0 = time.time()
for s, sz in blocks():
    want = SIGS[f"{s:#x}/{sz:#x}"]
    live = region_sig(s, sz)
    if live == want:
        ts(f"block {s:#x}: sig MATCH — done"); skipped_blocks += 1; continue
    tgt_blank = tgt[s:s+sz] == b"\xff"*sz
    if tgt_blank and live == BLANK[sz]:
        ts(f"block {s:#x}: blank as target — done"); skipped_blocks += 1; continue
    units = [o for o in range(s, s+sz, 0x100) if tgt[o:o+0x100] != b"\xff"*0x100]
    if not tgt_blank:
        ts(f"block {s:#x}: sig {live} vs {want} -> {len(units)} units")
    else:
        ts(f"block {s:#x}: unexpected content -> erase + {len(units)} units")
        ff, cf = tw.compose_erase(s)
        reps = t.cmd(f"erase {s:#x}", [ff, cf], tmo=8.0)
        if not (reps and reps[0][0] == 0x60):
            ts(f"*** erase refused: {[r.hex() for r in reps]} ***"); b.shutdown(); sys.exit(4)
        if reps[0][1] & 0x80:
            time.sleep(2.5)
    for off in units:
        unit = tgt[off:off+0x100]
        ok = False
        for attempt in range(3):
            try:
                ff2, cf2 = arm_key_frames(off)
                reps = t.cmd("ARM", [ff2, cf2])
                if not (reps and reps[0][0] == 0x51):
                    raise RuntimeError(f"ARM {off:#x}: {[r.hex() for r in reps]}")
                reps = t.cmd("program", tw.compose_program(unit), tmo=8.0)
                if not (reps and reps[0][0] == 0x52):
                    raise RuntimeError(f"PGM {off:#x}: {[r.hex() for r in reps]}")
                ok = True; break
            except RuntimeError as e:
                ts(f"  {e} retry {attempt+1}"); time.sleep(2.0)
        if not ok:
            ts(f"*** unit {off:#x} FAILED — rerun this script after a power cycle ***")
            b.shutdown(); sys.exit(5)
        written += 1
        if written % 200 == 0:
            ts(f"  {written} units ({time.time()-t0:.0f}s)")
ts(f"pass done: {written} units written, {skipped_blocks} blocks complete ({time.time()-t0:.0f}s)")

# ---- final verify: every block must match the table ----
ts("--- final signature verify ---")
bad = 0
for s, sz in blocks():
    want = SIGS[f"{s:#x}/{sz:#x}"]
    live = region_sig(s, sz)
    ok = live == want
    bad += not ok
    ts(f"  {s:#x}: {live} {'OK' if ok else f'*** WANT {want} ***'}")
ts("*** M4V2 INSTALL COMPLETE — power-cycle for boot ***" if bad == 0
   else f"*** {bad} BLOCKS OFF — rerun after power cycle ***")
b.shutdown()
