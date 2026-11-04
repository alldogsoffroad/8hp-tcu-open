#!/usr/bin/env python3
"""park-check over the status frames: the m4a lever-decode bench check reading
0x7F1 (inputs: lever, source) instead of the measurement table (written when the
meas responder looked dead; resolved 2026-11-04 — it was the RCAN mailbox reorder
dropping 0x361, fixed in bac8716 and proven live: ident + word-read on 0x398).
Steps and expectations mirror tcu_bench.py park-check / m4 doc §7a."""
import sys, threading, time
sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
sys.path.insert(0, "/home/chadk/Documents/acdp2-re/tools")
import tcu_bench
from tcu_bench import LeverSender


class A:
    iface = "slcan"; bus1 = "/dev/ttyACM0"; bus2 = "/dev/ttyACM1"
    teensy_mode = None; keep_rate = True


def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)


for cmd in (b"C\r", b"P0\r", b"S6\r", b"O\r"):
    tcu_bench.teensy_passive([A.bus1, A.bus2], cmd)
b = tcu_bench.Buses(A())
bus = b.get(1)

def read_7f1(settle=1.2):
    """latest 0x7F1 frame: (lever, source)"""
    end = time.time() + settle
    last = None
    while time.time() < end:
        m = bus.recv(0.05)
        if m is not None and m.arbitration_id == 0x7F1:
            last = m.data
    return (last[2], last[3]) if last else (None, None)


# (desc, code, 0x2EA on, 0x210 on, bad CRC, lever want, source want)
# SEND codes are the wire coding (lever_frames: 1 P, 2 R, 3 N, 4 D, 0 no-change, 7 invalid);
# EXPECTED values are the status coding (0x7F1 byte2: 8 P, 7 R, 6 N, 5 D, 0 none)
STEPS = [
    ("no shifter frames",            1, False, False, False, 0, 2),
    ("P on both buses",              1, True,  True,  False, 8, 0),
    ("R on both buses",              2, True,  True,  False, 7, 0),
    ("N on both buses",              3, True,  True,  False, 6, 0),
    ("D on both buses",              4, True,  True,  False, 5, 0),
    ("code 0 (no change) after D",   0, True,  True,  False, 5, 0),
    ("0x2EA bad CRC, 0x210 R",       2, True,  True,  True,  7, 1),
    ("0x2EA missing, 0x210 N",       3, False, True,  False, 6, 1),
    ("0x2EA D, 0x210 missing",       4, True,  False, False, 5, 0),
    ("invalid code 7 on both",       7, True,  True,  False, 0, 2),
    ("both missing again",           4, False, False, False, 0, 2),
    ("P on both buses",              1, True,  True,  False, 8, 0),
]

snd = LeverSender(b)
snd.start()
srcs = {0: "CAN-C", 1: "shifter", 2: "none"}
passed = 0
try:
    for desc, code, a, c, bad, wlev, wsrc in STEPS:
        snd.code, snd.on_2ea, snd.on_210, snd.bad_crc = code, a, c, bad
        time.sleep(1.5)
        lev, src = read_7f1()
        ok = lev == wlev and src == wsrc
        passed += ok
        ts(f"{'PASS' if ok else '*** FAIL ***'} {desc:28s} lever={lev} src={srcs.get(src, src)}"
           f" (want {wlev}/{srcs[wsrc]})")
finally:
    snd.stop.set()
ts(f"--- {passed}/{len(STEPS)} steps passed ---")
b.shutdown()
