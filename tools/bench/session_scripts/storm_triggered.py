#!/usr/bin/env python3
"""Ammeter-triggered install chain (user's idea, 2026-11-02): watch the P-line
feed voltage on ACM1 (Teensy dual-port telemetry); only start the PlantCan
storm AFTER the feed returns — the storm never runs into a dead bus, so the
controller never saturates bus-off and the boot's announce gets ACKed.
Then the standard chain: catch -> probe -> key -> engine -> prime -> loader."""
import serial, subprocess, sys, time
sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
sys.path.insert(0, "/home/chadk/Documents/acdp2-re/tools")
import tcu_bench

BUS1 = "/dev/ttyACM0"     # ch[0]: CAN-B? no — bus-1 commands + slcan bridge
AM   = "/dev/ttyACM1"     # ch[1]: telemetry (P/A lines) via the dual-port fw

def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)

# ---- telemetry monitor on ACM1 ------------------------------------------------
mon = serial.Serial(AM, 115200, timeout=0.2)
mon.write(b"A1\r")        # global: power monitoring on
time.sleep(0.3); mon.reset_input_buffer()

def latest():
    """(last A mA, last P mV) from the telemetry stream"""
    a = p = None
    for ln in mon.read(4096).decode(errors="replace").splitlines():
        ln = ln.strip()
        if ln.startswith("A") and ln[1:].lstrip("-").isdigit():
            a = int(ln[1:])
        elif ln.startswith("P") and ln[1:].isdigit():
            p = int(ln[1:])
    return a, p

OFF_MA, ON_MA = 40, 80      # the pin-14 feed sense sits on the PSU side of the
                            # user's switch (P never drops) — the CURRENT is the edge

# ---- the storm: alive the whole time, rate-shaped by the power state ---------
import tcu_write as tw

class A:
    iface = "slcan"; bus1 = BUS1; bus2 = None    # the monitor owns ACM1
    teensy_mode = None; keep_rate = True

for cmd in (b"C\r", b"P0\r", b"S9\r", b"O\r"):
    tcu_bench.teensy_passive([A.bus1], cmd)
b = tcu_bench.Buses(A())

caught = False
powered = None          # unknown yet
last_hb = 0.0
t0 = time.time()
ts("storm live — power the TCU OFF, then ON; rate shapes automatically")
while time.time() - t0 < 600.0:
    a, p = latest()
    if a is not None:
        if powered is None or (powered and a < OFF_MA):
            if powered:
                ts(f"current dropped ({a} mA) — heartbeat storm (TEC-safe)")
            powered = a > ON_MA
            if powered:
                ts(f"*** POWER EDGE at {a} mA (t+{time.time()-t0:.1f}s) — full-rate storm")
        powered = a > ON_MA
    if powered:
        b.send(1, 0x1F0C3CFE, b"PlantCan")
        time.sleep(0.002)
    elif powered is False and time.time() - last_hb >= 2.0:
        b.send(1, 0x1F0C3CFE, b"PlantCan")     # one heartbeat / 2 s: +8 TEC each,
        last_hb = time.time()                   # stays under bus-off for ~60 s of off
    msg = b.get(1).recv(timeout=0.0)
    while msg is not None:
        if msg.arbitration_id == 0x1FFFAFFE:
            caught = True
        msg = b.get(1).recv(timeout=0.0)
    if caught:
        break
if not caught:
    ts(f"window closed after {time.time()-t0:.1f}s, no catch")
    mon.write(b"A0\r"); mon.close(); b.shutdown(); sys.exit(1)
ts(f"caught at t+{time.time()-t0:.1f}s — probing the plant session NOW")

mon.write(b"A0\r")
mon.close()

from tcu_write import UdsCapped
u = UdsCapped(b, 1)
r = u.request(bytes.fromhex("1060"), timeout=2.0)
if not r or r[:2] != b"\x50\x60":
    ts("*** 10 60 dead at catch"); b.shutdown(); sys.exit(2)

u.cf_gap_s = 0.004
u.honor_stmin = False
import hashlib
from tcu_write import forge_key_dom
for attempt in range(16):
    r = u.request(bytes.fromhex("2761"), timeout=5)
    if not r or r[:2] != b"\x67\x61":
        ts("seed failed"); b.shutdown(); sys.exit(3)
    key = forge_key_dom(hashlib.new("ripemd160", r[2:18]).digest())
    if key is None:
        continue
    time.sleep(0.2)
    r = u.request(bytes([0x27, 0x62]) + key, timeout=30)
    if r and r[:2] == b"\x67\x62":
        ts("*** 67 62 (forged domain key) ***"); break
else:
    ts("auth failed"); b.shutdown(); sys.exit(4)

image = open(tw.ENGINE, "rb").read()
r = u.request(bytes([0x34, 0x00, 0x34]) + (0xFFF88400).to_bytes(4, "big")
              + len(image).to_bytes(3, "big"), timeout=15)
if not r or r[0] != 0x74:
    ts("34 failed"); b.shutdown(); sys.exit(5)
seq = 1
for off in range(0, len(image), 0x100):
    ok = False
    for retry in range(3):
        r = u.request(bytes([0x36, seq]) + image[off:off + 0x100], timeout=15)
        if r and r[0] == 0x76:
            ok = True; break
        ts(f"36 block {seq} retry {retry+1}"); time.sleep(0.3)
    if not ok:
        ts(f"36 block {seq} failed"); b.shutdown(); sys.exit(6)
    seq += 1
    time.sleep(0.002)
u.request(b"\x37", timeout=15)
for i in range(1, 401):
    r = u.request(bytes.fromhex("3101FF01"), timeout=5)
    if r and r[:4] == bytes([0x71, 0x01, 0xFF, 0x01]):
        break
    time.sleep(0.01)
else:
    ts("FF01 never acked"); b.shutdown(); sys.exit(7)
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
