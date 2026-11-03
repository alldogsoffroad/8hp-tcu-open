#!/usr/bin/env python3
"""Front-door flash chain (2026-11-03): the boot's 500k responder, no plant
entry, no storm. 10 61 -> cube-forged key -> engine download -> FF01 ->
ToolSession -> FWE probe -> (high) prime + m4v2 install. Seeds retried until
v2(ripemd160(seed))%3==0 (~1/3)."""
import sys, time, hashlib, subprocess
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

for cmd in (b"C\r", b"P0\r", b"S6\r", b"O\r"):
    tcu_bench.teensy_passive([BUS1], cmd)
tcu_bench.teensy_passive([BUS1], b"B1\r")          # LED fast = power on now

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

ts("frontdoor armed — LED fast; waiting for confirmed OFF, then your power-on")
t_off = time.time()
while time.time() - t_off < 300:
    a = latest()
    if a is not None and a < 40: break
    time.sleep(0.3)
else:
    ts("no off state seen"); mon.close(); sys.exit(10)
ts("off confirmed — LED fast = POWER ON NOW")
t0 = time.time()
while time.time() - t0 < 300:
    a = latest()
    if a is not None and a > 80: break
    time.sleep(0.15)
else:
    ts("no power edge"); mon.write(b"A0\r"); mon.close(); sys.exit(1)
ts(f"edge {a} mA at t+{time.time()-t0:.1f}s — LED slow, chaining on the front door")
tcu_bench.teensy_passive([BUS1], b"B0\r")
mon.write(b"A0\r"); mon.close()

b = tcu_bench.Buses(A())
u = UdsCapped(b, 1)
u.cf_gap_s = 0.004; u.honor_stmin = False

t1 = time.time()
sess = None
while time.time() - t1 < 90:
    r = u.request(bytes.fromhex("1061"), timeout=1.5)
    if r and r[0] == 0x50:
        sess = True; break
    time.sleep(0.15)
if not sess:
    ts("responder never accepted 10 61"); b.shutdown(); sys.exit(2)
ts(f"10 61 accepted at edge+{time.time()-t0:.1f}s")

key = None
for attempt in range(10):
    r = u.request(bytes.fromhex("2777"), timeout=3)
    if not (r and r[0] == 0x67):
        ts(f"27 77: {r.hex() if r else 'no reply'}"); b.shutdown(); sys.exit(3)
    seed = r[2:18]
    d = hashlib.new("ripemd160", seed).digest()
    v2 = (int.from_bytes(d, 'big') & -int.from_bytes(d, 'big')).bit_length() - 1
    if v2 % 3:
        continue
    key = forge_key_dom(d)
    if key: break
if not key:
    ts("no forgeable seed in 10 tries"); b.shutdown(); sys.exit(4)
time.sleep(0.2)
r = u.request(bytes([0x27, 0x78]) + key, timeout=30)
if not (r and r[0] == 0x67):
    ts(f"27 78 rejected: {r.hex() if r else None}"); b.shutdown(); sys.exit(5)
ts(f"*** FRONT DOOR KEYED at edge+{time.time()-t0:.1f}s ***")

image = open(tw.ENGINE, "rb").read()
r = u.request(bytes([0x34, 0x00, 0x34]) + (0xFFF88400).to_bytes(4, "big")
              + len(image).to_bytes(3, "big"), timeout=15)
if not r or r[0] != 0x74:
    ts(f"34: {r.hex() if r else None}"); b.shutdown(); sys.exit(6)
ts("34 accepted — downloading engine")
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
ts("engine downloaded — 31 01 FF01")
launched = False
for i in range(1, 401):
    r = u.request(bytes.fromhex("3101FF01"), timeout=5)
    if r and r[:4] == bytes([0x71, 0x01, 0xFF, 0x01]):
        launched = True; break
    time.sleep(0.01)
if not launched:
    ts(f"FF01 not accepted: {r.hex() if r else None}"); b.shutdown(); sys.exit(8)
ts(f"*** engine launched via the front door at edge+{time.time()-t0:.1f}s ***")

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
t.cmd("ARM probe", [ff, cf])
fer = read4(0xFFFFA902)
if fer is None or (fer & 0x03) == 0:
    ts(f"*** FWE LOW via front door ({fer}) — documented negative ***"); b.shutdown(); sys.exit(9)
ts(f"*** FWE HIGH (FENTRYR={fer:#x}) at edge+{time.time()-t0:.1f}s — priming + install ***")
t.prime()
ts("primed — installer")
b.shutdown()
subprocess.run(["python3", "/tmp/install_m4v2.py"])
