#!/usr/bin/env python3
"""The ACDP-2 play (2026-11-03): FWE is at reset level from power-on; the boot's
500k reflash responder is the tool's front door. On the current edge, chain on
500k IMMEDIATELY: session -> seed -> forge -> engine download -> FF01 ->
ToolSession -> FWE probe -> install if high. Falls back to reporting."""
import sys, time, hashlib
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
from tcu_write import UdsCapped, forge_key_dom

BUS1, AM = "/dev/ttyACM0", "/dev/ttyACM1"
def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)

# LED: fast while waiting for power-on (the user's signal), slow once chaining
tcu_bench.teensy_passive([BUS1], b"C\r"); tcu_bench.teensy_passive([BUS1], b"P0\r")
tcu_bench.teensy_passive([BUS1], b"S6\r"); tcu_bench.teensy_passive([BUS1], b"O\r")
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

ts("race500k armed — waiting for confirmed OFF state before the signal")
t_off = time.time()
while time.time() - t_off < 300:
    a = latest()
    if a is not None and a < 40:
        break
    time.sleep(0.3)
else:
    ts("never saw the off state"); mon.close(); sys.exit(10)
ts("off state confirmed — LED FAST = power on now")
tcu_bench.teensy_passive([BUS1], b"B1\r")
t0 = time.time()
while time.time() - t0 < 900:
    a = latest()
    if a is not None and a > 80:
        break
    time.sleep(0.2)
else:
    ts("no power edge in 15 min"); mon.close(); sys.exit(1)
ts(f"edge at {a} mA — LED slow, chaining on 500k NOW")
tcu_bench.teensy_passive([BUS1], b"B0\r")
mon.write(b"A0\r"); mon.close()

b = tcu_bench.Buses(A())
u = UdsCapped(b, 1)
# poll the responder hard from the first second
sess = None
t1 = time.time()
while time.time() - t1 < 120:
    for sid in ("1002", "1001", "1003"):
        r = u.request(bytes.fromhex(sid), timeout=1.0)
        if r and r[0] == 0x50:
            sess = sid; break
    if sess: break
    time.sleep(0.2)
if not sess:
    ts("500k responder never answered in 120 s"); b.shutdown(); sys.exit(2)
ts(f"500k responder up ({sess}) at t+{time.time()-t1:.1f}s")
u.cf_gap_s = 0.004; u.honor_stmin = False

for sid in ("1002", "1003"):
    r = u.request(bytes.fromhex(sid), timeout=2.0)
    ts(f"session {sid}: {r.hex() if r else 'no reply'}")
seed_sid = None
for sub in ("01", "03", "11", "61", "71"):
    r = u.request(bytes([0x27]) + bytes.fromhex(sub), timeout=3)
    ts(f"27 {sub}: {r.hex() if r else 'no reply'}")
    if r and r[0] == 0x67:
        seed_sid = sub; break
if not seed_sid:
    ts("no security subfunction gave a seed"); b.shutdown(); sys.exit(3)
ts(f"seed subfunction = {seed_sid}")
key = forge_key_dom(hashlib.new("ripemd160", r[2:18]).digest())
if key is not None and seed_sid != "61":
    ts("note: non-plant subfunction — domain forge may not validate")
if key is None:
    ts("forge failed"); b.shutdown(); sys.exit(4)
time.sleep(0.2)
r = u.request(bytes([0x27]) + bytes([int(seed_sid, 16) + 1]) + key, timeout=30)
if not (r and r[:2] == b"\x67\x62"):
    ts(f"500k key rejected: {r.hex() if r else None}"); b.shutdown(); sys.exit(5)
ts("*** 67 62 on the 500k responder — the front door opens ***")

image = open(tw.ENGINE, "rb").read()
r = u.request(bytes([0x34, 0x00, 0x34]) + (0xFFF88400).to_bytes(4, "big")
              + len(image).to_bytes(3, "big"), timeout=15)
if not r or r[0] != 0x74:
    ts(f"34 rejected: {r.hex() if r else None}"); b.shutdown(); sys.exit(6)
seq = 1
for off in range(0, len(image), 0x100):
    ok = False
    for retry in range(3):
        r = u.request(bytes([0x36, seq]) + image[off:off + 0x100], timeout=15)
        if r and r[0] == 0x76: ok = True; break
        time.sleep(0.3)
    if not ok:
        ts(f"36 block {seq} failed"); b.shutdown(); sys.exit(7)
    seq += 1
    time.sleep(0.002)
u.request(b"\x37", timeout=15)
ts("engine downloaded — FF01")
r = None
for i in range(1, 401):
    r = u.request(bytes.fromhex("3101FF01"), timeout=5)
    if r and r[:4] == bytes([0x71, 0x01, 0xFF, 0x01]): break
    time.sleep(0.01)
if not (r and r[:4] == bytes([0x71, 0x01, 0xFF, 0x01])):
    ts(f"FF01 not accepted on 500k: {r.hex() if r else None}"); b.shutdown(); sys.exit(8)
ts("*** engine launched via the front door — ToolSession + FWE probe ***")

b.shutdown()
for cmd in (b"C\r", b"P0\r", b"S6\r", b"O\r"):
    tcu_bench.teensy_passive([BUS1], cmd)
b = tcu_bench.Buses(A())
t = tw.TCU(); t.b = b; t.n = 1
t.sess = tws.ToolSession(b, 1, image)
t.sess.handshake()
def ck7_(h): return h + bytes([(sum(h) + 0x55) & 0xFF])
def read4(addr):
    t.sess.send_pt(ck7_(bytes([0x01, 0x05]) + addr.to_bytes(4, "big") + bytes([4])))
    r = t.sess.recv_pt(timeout=2.0)
    return int.from_bytes(r[2:6], "big") if r and r[0] == 0x41 else None
ff, cf = tw.compose_arm(0x40000)
reps = t.cmd("ARM probe", [ff, cf])
fer = read4(0xFFFFA902)
if fer is None or (fer & 0x03) == 0:
    ts(f"*** FWE LOW even at the front door ({fer}) ***"); b.shutdown(); sys.exit(9)
ts(f"*** FWE HIGH (FENTRYR={fer:#x}) — priming + install NOW ***")
t.prime()
ts("primed — installer")
b.shutdown()
import subprocess
subprocess.run(["python3", "/tmp/install_m4v2.py"])
