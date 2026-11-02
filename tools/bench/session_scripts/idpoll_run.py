#!/usr/bin/env python3
"""ID-poll run: the full stock commissioning order (ID poll -> write ->
setpoints -> reads/dither) + tokens + echo-confirmed ramp + 0x1F, enable as
shadow. Enable is A-law gated - this run cannot reset the TCU. Verifies arms
by RESULTS MODE (replies drop intermittently)."""
import os, sys, time
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

def read_status():
    r = u.request(b"\x31\x03\xFE\x13")
    if not r or len(r) < 6:
        return None, f"raw: {r.hex() if r else None}"
    if r[4] == 3 and len(r) >= 43:
        rw = [int.from_bytes(r[20+2*k:22+2*k], "big") for k in range(8)]
        d = dict(state=r[5], sel=r[6], lvl=r[7], cmd=r[8], rot=r[9], jobs=r[10],
                 tok=(r[17] << 8) | r[18], en=r[19], echo22=rw[2],
                 dj=r[40], dbad=r[41],
                 da8=int.from_bytes(r[42:44], "big"),
                 daa=int.from_bytes(r[44:46], "big"),
                 da2=int.from_bytes(r[46:48], "big") if len(r) >= 48 else -1,
                 dwr=r[48] if len(r) >= 49 else -1,
                 did=r[49] if len(r) >= 50 else -1)
        return 3, d, (f"state={d['state']} sel={d['sel']} lvl={d['lvl']} cmd={d['cmd']} "
                      f"tok={d['tok']} en={d['en']:03b} 22echo={d['echo22']:04x}"
                      f"({'GRANTED' if d['echo22'] & 1 else 'no'}) "
                      f"drv j={d['dj']} id={d['did']} wr={d['dwr']} "
                      f"a8:{d['da8']} aa:{d['daa']} a2:{d['da2']} b{d['dbad']}")
    if r[4] == 2:
        return 2, None, f"mode2 22echo={int.from_bytes(r[9:11], 'big'):04x}"
    return r[4], None, f"raw: {r.hex()}"

try:
    r = u.request(b"\x22\xf1\x95")
    ts(f"ident: {r[3:].decode(errors='replace') if r else 'NO REPLY'}")
    ts("*** ID-POLL RUN PENDING — waiting for go-file. WATCH THE AMMETER. ***")
    go = False
    for _ in range(900):
        if os.path.exists("/tmp/tcu_go"):
            os.unlink("/tmp/tcu_go"); go = True; break
        time.sleep(1.0)
    if not go:
        ts("no go — exiting"); raise SystemExit
    ts("GO — pattern + engage (full stock order, enable as shadow)")
    u.request(bytes([0x31, 0x01, 0xFE, 0x13, 0x07, 0x00, 0x20]))
    u.request(bytes([0x31, 0x01, 0xFE, 0x13, 0x09, 0x02]))
    for i in range(180):
        time.sleep(1.0)
        m, d, s = read_status()
        ts(f"[{i:3d}] {s}")
        if m == 3 and isinstance(s, str) and "GRANTED" in s:
            ts("*** GRANT SEEN — word enables within 5 ms. WATCH THE AMMETER. ***")
    ts("*** watch done — ammeter verdict please ***")
finally:
    try:
        u.request(bytes([0x31, 0x02, 0xFE, 0x13]))
    except Exception:
        pass
    tp.set(); b.shutdown()
