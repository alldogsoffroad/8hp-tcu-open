#!/usr/bin/env python3
"""Quick bench-state check: is the TCU powered, which build, engage status."""
import sys, time
sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench

class A:
    iface = "slcan"; bus1 = "/dev/ttyACM0"; bus2 = "/dev/ttyACM1"
    teensy_mode = None; keep_rate = True

def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)

b = tcu_bench.Buses(A())
try:
    u = tcu_bench.Uds(b, 1)
    r = u.request(b"\x22\xf1\x95", timeout=1.5)
    if r:
        ts(f"ident: {r[3:].decode(errors='replace')}")
    else:
        ts("no UDS answer — TCU likely powered down")
        sys.exit(0)
    r = u.extended()
    ts(f"extended: {r.hex() if r else None}")
    tp = u.start_tester_present()
    # results mode 2 (pattern status, read-only; does not disturb engage)
    time.sleep(0.3)
    r = u.request(b"\x31\x03\xFE\x13")
    if r and len(r) > 5:
        mode = r[4]
        if mode == 2 and len(r) >= 27:
            rw = [int.from_bytes(r[5+2*k:7+2*k], "big") for k in range(8)]
            eok = int.from_bytes(r[21:23], "big")
            ebad = int.from_bytes(r[23:25], "big")
            ts(f"mode2: 8E={rw[0]:04x} 22echo={rw[2]:04x} "
               f"({'GRANTED' if rw[2] & 1 else 'no grant'}) cycOK={eok} cycBAD={ebad}")
        else:
            ts(f"results mode {mode}: {r.hex()}")
    else:
        ts(f"results: {r.hex() if r else None}")
    tp.set()
finally:
    b.shutdown()
