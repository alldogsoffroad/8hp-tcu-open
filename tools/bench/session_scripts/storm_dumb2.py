#!/usr/bin/env python3
"""dumb storm v2: on the current edge, wait 3 s for the TCU's CAN to come up,
reset the controller (C/O clears any early bus-off), then full-rate with a
C/O reset every 5 s as bus-off insurance (10 ms deaf gap is negligible)."""
import serial, sys, time
sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench
BUS1, AM = "/dev/ttyACM0", "/dev/ttyACM1"
def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)
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
for cmd in (b"C\r", b"P0\r", b"S9\r", b"O\r"):
    tcu_bench.teensy_passive([BUS1], cmd)
b = tcu_bench.Buses(A())
bus = b.get(1)
ser = getattr(bus, "ser", None) or getattr(bus, "serial", None)
def ctl_reset():
    if ser is not None:
        try: ser.write(b"C\rO\r")
        except Exception as e: ts(f"ctl reset err: {e}")
    else:
        ts("no ser handle — C/O via passive")
        tcu_bench.teensy_passive([BUS1], b"C\r"); tcu_bench.teensy_passive([BUS1], b"O\r")
ENTRY = 0x1F0C3CFE
t0 = time.time(); powered = False; last = 0.0; last_rst = 0.0
ts("dumb storm v2 live: burst 20 s @1 fps; edge -> 3 s wait -> C/O -> full-rate (+C/O every 5 s)")
tcu_bench.teensy_passive([BUS1], b"B1\r")   # LED FAST at arm: the user's signal to power on
while True:
    now = time.time()
    a = latest()
    if a is not None and a > 80 and not powered:
        powered = True
        ts(f"*** POWER EDGE at {a} mA (t+{now-t0:.1f}s) — 3 s CAN settle, then full-rate")
        tcu_bench.teensy_passive([BUS1], b"B0\r")   # LED slow: signal consumed, storm serving
        time.sleep(3.0)
        ctl_reset()
        last = now
    if (now - t0 < 20.0 and not powered and now - last >= 1.0):
        b.send(1, ENTRY, b"PlantCan"); last = now
    elif powered:
        if now - last_rst >= 5.0:
            ctl_reset(); last_rst = now
        if now - last >= 0.002:
            b.send(1, ENTRY, b"PlantCan"); last = now
