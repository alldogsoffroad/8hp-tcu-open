#!/usr/bin/env python3
"""THE FAST PATH (2026-11-03): the ACDP's own front door, from the tap —
raw ToolSession hail 54ff3d22 on 0x7E1 at 500k, answered only in the boot's
first seconds (FWE at reset level). Edge -> hail poll -> ToolSession -> FWE
probe -> inline m4v2 install (no plant, no engine, no prime)."""
import sys, time, json
sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
sys.path.insert(0, "/home/chadk/Documents/acdp2-re/tools")
sys.path.insert(0, "/tmp")
import serial
import tcu_bench
import importlib.util as ilu
spec = ilu.spec_from_file_location('tws', '/tmp/teensy_write_session.py')
tws = ilu.module_from_spec(spec); sys.modules['tws'] = tws
try: spec.loader.exec_module(tws)
except SystemExit: pass
import tcu_write as tw
from tcu_write import crc16

BUS1, AM = "/dev/ttyACM0", "/dev/ttyACM1"
def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)

for cmd in (b"C\r", b"P0\r", b"S6\r", b"O\r"):
    tcu_bench.teensy_passive([BUS1], cmd)
tcu_bench.teensy_passive([BUS1], b"B1\r")

mon = serial.Serial(AM, 115200, timeout=0.2)
mon.write(b"A1\r"); time.sleep(0.3); mon.reset_input_buffer()
def latest():
    a = None
    for ln in mon.read(4096).decode(errors="replace").splitlines():
        ln = ln.strip()
        if ln.startswith("A") and ln[1:].lstrip("-").isdigit(): a = int(ln[1:])
    return a
class A:
    iface = "slcan"; bus1 = BUS1; bus2 = None
    teensy_mode = None; keep_rate = True

ts("fronthail armed — waiting for confirmed OFF")
t_off = time.time()
while time.time() - t_off < 300:
    a = latest()
    if a is not None and a < 40: break
    time.sleep(0.3)
else:
    ts("no off state"); mon.close(); sys.exit(10)
ts("off confirmed — LED FAST = POWER ON NOW")
# pre-stage everything: first hail must leave within ~0.3 s of the edge
image = open(tw.ENGINE, "rb").read()
b = tcu_bench.Buses(A())
t = tw.TCU(); t.b = b; t.n = 1
t.sess = tws.ToolSession(b, 1, image)
t0 = time.time()
while time.time() - t0 < 300:
    a = latest()
    if a is not None and a > 80: break
    time.sleep(0.1)
else:
    ts("no edge"); mon.write(b"A0\r"); mon.close(); sys.exit(1)
ts(f"edge {a} mA at t+{time.time()-t0:.1f}s — LED slow; hailing")
tcu_bench.teensy_passive([BUS1], b"B0\r")
mon.write(b"A0\r"); mon.close()

# hammer the hail from t=0 until the seed comes (the reset window is seconds)
got = False
t1 = time.time()
alt = 0
while time.time() - t1 < 60:
    try:
        if alt % 2 == 0 and hasattr(tws, "HAIL"):
            old = tws.HAIL
            tws.HAIL = bytes.fromhex("D7656915")   # the BOOT's opener (spec 17 §5)
            t.sess.HAIL = bytes.fromhex("D7656915") if hasattr(t.sess, "HAIL") else None
        t.sess.handshake()
        got = True
        break
    except SystemExit:
        pass
    alt += 1
    time.sleep(0.2)
if not got:
    ts("no seed after 60 s of hailing — window closed/absent"); b.shutdown(); sys.exit(2)
ts(f"*** BOOT TOOLSESSION UP at edge+{time.time()-t0:.1f}s ***")

def ck7_(h): return h + bytes([(sum(h) + 0x55) & 0xFF])
def read4(addr):
    t.sess.send_pt(ck7_(bytes([0x01, 0x05]) + addr.to_bytes(4, "big") + bytes([4])))
    r = t.sess.recv_pt(timeout=2.0)
    return int.from_bytes(r[2:6], "big") if r and r[0] == 0x41 else None

ff, cf = tw.compose_arm(0x40000)
reps = t.cmd("ARM probe", [ff, cf])
fer = read4(0xFFFFA902)
if fer is None or (fer & 0x03) == 0:
    ts(f"*** FWE LOW at the front door ({fer}) — NRC/diag above — documented negative ***")
    b.shutdown(); sys.exit(3)
ts(f"*** FWE HIGH ({fer:#x}) at edge+{time.time()-t0:.1f}s — installing m4v2 inline ***")

tgt = open("/home/chadk/Documents/REA Projects/build/m4v2_stockbody_IFla.bin", "rb").read()
SIGS = json.load(open("/tmp/m4v2_sigs.json"))
BLANK = {0x10000: "de547e4e81cb0165", 0x20000: "14b603cca7a12f57"}
def blocks():
    a = 0x40000
    while a < 0x180000:
        sz = 0x10000 if a < 0xA0000 else 0x20000
        yield a, sz; a += sz
def region_sig(start, size, tries=4):
    for i in range(tries):
        reps = t.cmd(f"verify {start:#x}/{size:#x}", tw.compose_verify(start, size), nreply=2, tmo=4.0)
        if reps and reps[0][0] == 0x48:
            return reps[-1].hex()
        time.sleep(2.5)
    return None
def arm_key_frames(offset):
    key = 1 if offset < 0x100000 else 2
    cf = (0x80800000 + offset).to_bytes(4, "big") + bytes([0x00, 0x01, 0xE8, key])
    ff = ck7_(bytes([0x11, 0x10, 0x08, 0x00]) + crc16(cf).to_bytes(2, "big") + bytes([0x00]))
    return ff, cf

written = 0; done_blocks = 0; tstart = time.time()
for s, sz in blocks():
    want = SIGS[f"{s:#x}/{sz:#x}"]
    live = region_sig(s, sz)
    if live == want:
        done_blocks += 1; continue
    tgt_blank = tgt[s:s+sz] == b"\xff"*sz
    if tgt_blank and live == BLANK[sz]:
        done_blocks += 1; continue
    units = [o for o in range(s, s+sz, 0x100) if tgt[o:o+0x100] != b"\xff"*0x100]
    if not tgt_blank:
        ts(f"block {s:#x}: {len(units)} units")
    else:
        ts(f"block {s:#x}: erase + {len(units)} units")
        ff, cf = tw.compose_erase(s)
        reps = t.cmd(f"erase {s:#x}", [ff, cf], tmo=8.0)
        if not (reps and reps[0][0] == 0x60):
            ts(f"erase refused: {[r.hex() for r in reps]}"); b.shutdown(); sys.exit(4)
        if reps[0][1] & 0x80: time.sleep(2.5)
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
            ts(f"  {written} units ({time.time()-tstart:.0f}s)")
ts(f"pass: {written} units, {done_blocks} blocks pre-done ({time.time()-tstart:.0f}s)")
ts("--- final sig verify ---")
bad = 0
for s, sz in blocks():
    want = SIGS[f"{s:#x}/{sz:#x}"]
    live = region_sig(s, sz)
    ok = live == want
    bad += not ok
    ts(f"  {s:#x}: {'OK' if ok else f'{live} vs {want}'}")
ts("*** FRONT-DOOR INSTALL COMPLETE — power-cycle for boot ***" if bad == 0
   else f"*** {bad} blocks off — rerun after power cycle ***")
b.shutdown()
