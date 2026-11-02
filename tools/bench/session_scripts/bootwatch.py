#!/usr/bin/env python3
"""Boot watcher for the AUTO-ENGAGE build: from the first seconds after
power-on, read the pattern status (the 0x22 echo = the grant bit) WITHOUT
touching the self-armed sequence (sub-07 re-inits the pattern only)."""
import sys, time
sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench

class A:
    iface = "slcan"; bus1 = "/dev/ttyACM0"; bus2 = "/dev/ttyACM1"
    teensy_mode = None; keep_rate = True

def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)

b = tcu_bench.Buses(A())
u = tcu_bench.Uds(b, 1)

# connect as early as possible after power-on
t0 = time.time()
while time.time() - t0 < 30.0:
    try:
        r = u.request(b"\x22\xf1\x95")
    except Exception:
        r = None
    if r and r[3:12].decode(errors="replace").startswith("8HPTCU"):
        ts(f"app answers at t+{time.time()-t0:.1f}s")
        break
    time.sleep(0.2)
else:
    ts("app never answered in 30 s")
    sys.exit(1)

r = u.extended()
ts(f"extended: {r.hex() if r else None}")
tp = u.start_tester_present()
r = u.request(bytes([0x31, 0x01, 0xFE, 0x13, 0x07, 0x00, 0x20]))
ts(f"pattern status expose (engage untouched): {r.hex() if r else None}")

try:
    for i in range(240):
        time.sleep(1.0)
        r = u.request(b"\x31\x03\xFE\x13")
        if not r or len(r) < 27 or r[4] != 2:
            ts(f"[{i:3d}] raw: {r.hex() if r else None}")
            continue
        rw = [int.from_bytes(r[5+2*k:7+2*k], "big") for k in range(8)]
        eok = int.from_bytes(r[21:23], "big")
        ebad = int.from_bytes(r[23:25], "big")
        grant = "GRANTED ***" if rw[2] & 1 else "no"
        ts(f"[{i:3d}] t+{i+1}s 8E={rw[0]:04x} 22echo={rw[2]:04x} ({grant}) "
           f"09={rw[3]:04x} cycOK={eok} cycBAD={ebad}")
        if rw[2] & 1:
            ts("*** ACCEPTANCE GRANTED AT t+%ds — WATCH THE AMMETER: THE RAIL! ***" % (i + 1))
    ts("*** boot watch done — ammeter verdict please ***")
finally:
    try:
        u.request(bytes([0x31, 0x02, 0xFE, 0x13]))
    except Exception:
        pass
    tp.set(); b.shutdown()
