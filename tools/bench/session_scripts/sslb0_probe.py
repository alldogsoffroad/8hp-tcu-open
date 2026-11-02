#!/usr/bin/env python3
"""SSLB0 probe matrix on the five-select (FE13 sub-0A; word i -> select i%5;
SSLB0 = position 3). Phase 1 (reads only, no gate needed): map the registers.
Then STOP and announce before any write step."""
import sys, time
sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench

class A:
    iface = "slcan"; bus1 = "/dev/ttyACM0"; bus2 = "/dev/ttyACM1"
    teensy_mode = None; keep_rate = True

def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)

b = tcu_bench.Buses(A())
u = tcu_bench.Uds(b, 1)
r = u.extended()
if not r or r[0] != 0x50:
    sys.exit(f"extended failed: {r.hex() if r else None}")
tp = u.start_tester_present()

def fivesel(words, name):
    """words: list of up to 5; padded to 5 with 0x0000 (reg-0 reads)."""
    words = (list(words) + [0x0000] * 5)[:5]
    payload = bytes([0x31, 0x01, 0xFE, 0x13, 0x0A])
    for w in words:
        payload += w.to_bytes(2, "big")
    r = u.request(payload)
    if not r or r[0] != 0x71:
        ts(f"{name}: REJECTED {r.hex() if r else 'no reply'}")
        return None
    time.sleep(0.4)
    r = u.request(b"\x31\x03\xFE\x13")
    if not r or len(r) < 6 or r[4] != 1:
        ts(f"{name}: results bad {r.hex() if r else None}")
        return None
    n = r[5]
    rx = [int.from_bytes(r[6+2*k:8+2*k], "big") for k in range(n)]
    sel = ["S3", "S2", "S1", "S0", "S5"]
    ts(f"{name}: " + "  ".join(f"{sel[i%5]}={rx[i]:04x}" for i in range(n)))
    return rx

try:
    r = u.request(b"\x22\xf1\x95")
    ts(f"ident: {r[3:].decode(errors='replace') if r else 'NO REPLY'}")
    # stop the auto-engage + pattern for a clean one-shot slate
    u.request(bytes([0x31, 0x01, 0xFE, 0x13, 0x09, 0x00]))
    u.request(bytes([0x31, 0x02, 0xFE, 0x13]))
    ts("engage disarmed, pattern stopped — one-shot slate clean")
    time.sleep(0.5)

    # Phase 1: read SSLB0's register map (position 3). Reads only — safe.
    for reg in (0x24, 0x2E, 0x30, 0x28, 0x36, 0x06, 0x00):
        fivesel([reg << 8], f"read 0x{reg:02X} on all 5")

    ts("*** Phase 1 done (reads only). Write steps are announced + gated. ***")
finally:
    try:
        u.request(bytes([0x31, 0x02, 0xFE, 0x13]))
    except Exception:
        pass
    tp.set(); b.shutdown()
