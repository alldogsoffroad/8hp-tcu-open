#!/usr/bin/env python3
"""Manual probe of the boot's plant session (S9/666.667k) — 10 60 on both buses."""
import sys, time
sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
sys.path.insert(0, "/home/chadk/Documents/acdp2-re/tools")
import tcu_bench
from tcu_write import UdsCapped

class A:
    iface = "slcan"; bus1 = "/dev/ttyACM0"; bus2 = "/dev/ttyACM1"
    teensy_mode = None; keep_rate = True

def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)

for cmd in (b"C\r", b"P0\r", b"S9\r", b"O\r"):
    tcu_bench.teensy_passive([A.bus1, A.bus2], cmd)
b = tcu_bench.Buses(A())
for n in (1, 2):
    try:
        u = UdsCapped(b, n)
        r = u.request(bytes.fromhex("1060"), timeout=2.0)
    except SystemExit:
        ts(f"bus {n}: no reply path"); continue
    if r and r[:2] == b"\x50\x60":
        ts(f"*** PLANT SESSION ALIVE on bus {n} — the chain can be completed by hand")
    else:
        ts(f"bus {n}: 10 60 -> {r.hex() if r else 'no reply'}")
b.shutdown()
