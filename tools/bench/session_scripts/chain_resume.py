#!/usr/bin/env python3
"""Resume the install chain from a LIVE plant session (no storm needed):
forged key -> engine download -> FF01 -> 500k -> ToolSession -> prime -> loader --resume."""
import subprocess, sys, time
sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
sys.path.insert(0, "/home/chadk/Documents/acdp2-re/tools")
import tcu_bench
import tcu_write as tw
from tcu_write import UdsCapped, forge_key_dom
import hashlib

class A:
    iface = "slcan"; bus1 = "/dev/ttyACM0"; bus2 = "/dev/ttyACM1"
    teensy_mode = None; keep_rate = True

def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)

for cmd in (b"C\r", b"P0\r", b"S9\r", b"O\r"):
    tcu_bench.teensy_passive([A.bus1, A.bus2], cmd)
b = tcu_bench.Buses(A())

u = UdsCapped(b, 1)
u.cf_gap_s = 0.004       # THE fix: back-to-back CF loss without the gap (bench 2026-10-18)
u.honor_stmin = False
r = u.request(bytes.fromhex("1060"), timeout=2.0)
if not r or r[:2] != b"\x50\x60":
    ts(f"plant session dead: {r.hex() if r else None}")
    b.shutdown(); sys.exit(1)
ts("plant session alive on bus 1")

for attempt in range(16):
    r = u.request(bytes.fromhex("2761"), timeout=5)
    if not r or r[:2] != b"\x67\x61":
        ts("seed failed"); b.shutdown(); sys.exit(2)
    key = forge_key_dom(hashlib.new("ripemd160", r[2:18]).digest())
    if key is None:
        continue
    time.sleep(0.2)
    r = u.request(bytes([0x27, 0x62]) + key, timeout=30)
    if r and r[:2] == b"\x67\x62":
        ts("*** 67 62 (forged domain key) ***"); break
else:
    ts("auth failed"); b.shutdown(); sys.exit(3)

image = open(tw.ENGINE, "rb").read()
r = u.request(bytes([0x34, 0x00, 0x34]) + (0xFFF88400).to_bytes(4, "big")
              + len(image).to_bytes(3, "big"), timeout=15)
if not r or r[0] != 0x74:
    ts("34 failed"); b.shutdown(); sys.exit(4)
seq = 1
for off in range(0, len(image), 0x100):
    r = u.request(bytes([0x36, seq]) + image[off:off + 0x100], timeout=15)
    if not r or r[0] != 0x76:
        ts(f"36 block {seq} failed"); b.shutdown(); sys.exit(5)
    seq += 1
u.request(b"\x37", timeout=15)
for i in range(1, 401):
    r = u.request(bytes.fromhex("3101FF01"), timeout=5)
    if r and r[:4] == bytes([0x71, 0x01, 0xFF, 0x01]):
        break
    time.sleep(0.01)
else:
    ts("FF01 never acked"); b.shutdown(); sys.exit(6)
ts("*** engine launched — 500k + ToolSession + prime ***")

port = b.names[1]
b.shutdown()
for cmd in (b"C\r", b"P0\r", b"S6\r", b"O\r"):
    tcu_bench.teensy_passive([port], cmd)
b = tcu_bench.Buses(A())
t = tw.TCU(); t.b = b; t.n = 1
t.sess = tw.tws.ToolSession(b, 1, image)
t.sess.handshake()
t.prime()
ts("*** chain complete — spawning the installer (--resume) ***")
b.shutdown()
subprocess.run(["python3", "/tmp/loader_install.py", "--resume"])
