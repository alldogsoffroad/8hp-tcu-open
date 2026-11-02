#!/usr/bin/env python3
"""Storm + immediate session probe on the SAME buses (no re-init gap).
If the 10 60 probe answers, hand off to the installer chain in-process."""
import subprocess
import sys
import time

sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
sys.path.insert(0, "/home/chadk/Documents/acdp2-re/tools")
import tcu_bench
import tcu_write as tw


class A:
    iface = "slcan"
    bus1 = "/dev/ttyACM0"
    bus2 = "/dev/ttyACM1"
    teensy_mode = None
    keep_rate = True


def ts(m):
    print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)


ts("storming PlantCan @666.667k — power the TCU now (OFF 30 s, ON)")
for cmd in (b"C\r", b"P0\r", b"S9\r", b"O\r"):
    tcu_bench.teensy_passive([A.bus1], cmd)   # S9: the MAT listens at 666.667k
b = tcu_bench.Buses(A())
caught = False
t0 = time.time()
while time.time() - t0 < 240.0:
    b.send(1, 0x1F0C3CFE, b"PlantCan")
    time.sleep(0.002)
    msg = b.get(1).recv(timeout=0.0)
    while msg is not None:
        if msg.arbitration_id == 0x1FFFAFFE:
            caught = True
        msg = b.get(1).recv(timeout=0.0)
    if caught:
        break
if not caught:
    ts("window closed, no catch")
    b.shutdown()
    sys.exit(1)
ts(f"caught after {time.time()-t0:.1f}s — probing the plant session NOW (same buses)")

from tcu_write import UdsCapped

n = None
for cand in (1, 2):
    try:
        u = UdsCapped(b, cand)
        r = u.request(bytes.fromhex("1060"), timeout=2.0)
    except SystemExit:
        continue
    if r and r[:2] == b"\x50\x60":
        n = cand
        break
if n is None:
    ts("*** 10 60 NOT answered even on the same buses — session dead at catch time")
    b.shutdown()
    sys.exit(2)
ts(f"*** plant session ALIVE on bus {n} — handing to the installer chain")

# The installer chain's only precondition (driver gone) is satisfied; run it.
t = tw.TCU()
t.b = b
t.n = n
u = UdsCapped(b, n)
u.cf_gap_s = 0.004
u.honor_stmin = False
for attempt in range(16):
    r = u.request(bytes.fromhex("2761"), timeout=5)
    if not r or r[:2] != b"\x67\x61":
        ts("seed failed")
        b.shutdown()
        sys.exit(3)
    from tcu_write import forge_key_dom
    import hashlib
    key = forge_key_dom(hashlib.new("ripemd160", r[2:18]).digest())
    if key is None:
        continue
    time.sleep(0.2)
    r = u.request(bytes([0x27, 0x62]) + key, timeout=30)
    if r and r[:2] == b"\x67\x62":
        ts("*** 67 62 (forged domain key) ***")
        break
else:
    ts("auth failed")
    b.shutdown()
    sys.exit(4)

image = open(tw.ENGINE, "rb").read()
r = u.request(bytes([0x34, 0x00, 0x34]) + (0xFFF88400).to_bytes(4, "big")
              + len(image).to_bytes(3, "big"), timeout=15)
if not r or r[0] != 0x74:
    ts("34 failed")
    b.shutdown()
    sys.exit(5)
seq = 1
for off in range(0, len(image), 0x100):
    r = u.request(bytes([0x36, seq]) + image[off:off + 0x100], timeout=15)
    if not r or r[0] != 0x76:
        ts(f"36 block {seq} failed")
        b.shutdown()
        sys.exit(6)
    seq += 1
u.request(b"\x37", timeout=15)
for i in range(1, 401):
    r = u.request(bytes.fromhex("3101FF01"), timeout=5)
    if r and r[:4] == bytes([0x71, 0x01, 0xFF, 0x01]):
        break
    time.sleep(0.01)
else:
    ts("FF01 never acked")
    b.shutdown()
    sys.exit(7)
ts("*** engine launched — completing the chain (500k + ToolSession + prime) ***")

port = b.names[n]
b.shutdown()
for cmd in (b"C\r", b"P0\r", b"S6\r", b"O\r"):
    tcu_bench.teensy_passive([port], cmd)
b = tcu_bench.Buses(A())
t = tw.TCU()
t.b = b
t.n = n
t.sess = tw.tws.ToolSession(b, n, image)
t.sess.handshake()
t.prime()
ts("*** chain complete — spawning the installer (--resume) ***")
b.shutdown()
subprocess.run(["python3", "/tmp/loader_install.py", "--resume"])

