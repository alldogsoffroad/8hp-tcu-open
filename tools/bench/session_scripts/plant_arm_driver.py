#!/usr/bin/env python3
"""plant_arm_driver.py — PlantCan wake storm (spec 17 §2).

Sends the mode-5 entry frame on 29-bit 0x1F0C3CFE at 666.667 kbit/s (S9)
continuously so the TCU's boot listen window catches it during power-up,
watching for the reply on 0x1FFFAFFE. Prints TCU FRAME and exits on catch
so the session tool (tcu_write/loader_install chain) can take the ports.

USAGE: start this, then power the TCU: OFF, hold ~30 s (rails discharge), ON.
One shot per power-up; if it times out, repeat the 30 s OFF / ON cycle.
"""
import sys, time
sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench

PORT = "/dev/ttyACM0"          # CAN1 = TCU pins 5/6
ENTRY_ID = 0x1F0C3CFE
REPLY_ID = 0x1FFFAFFE
ENTRY = b"PlantCan"            # mode 5
WINDOW_S = 240

for cmd in (b"C\r", b"P0\r", b"S9\r", b"O\r"):
    tcu_bench.teensy_passive([PORT], cmd)
    time.sleep(0.05)

class A:
    iface = "slcan"
    bus1 = "/dev/ttyACM0"
    bus2 = "/dev/ttyACM1"
    teensy_mode = None          # P0 already sent raw above
    keep_rate = True            # keep the S9 666.667k we just set

b = tcu_bench.Buses(A())
bus = b.get(1)
t0 = time.time()
sent = 0
print(f"[storm] armed: {ENTRY} on {ENTRY_ID:08X} @666.667k — power the TCU now (OFF 30 s, ON)", flush=True)
while time.time() - t0 < WINDOW_S:
    try:
        b.send(1, ENTRY_ID, ENTRY)
        sent += 1
    except Exception as e:
        print(f"[storm] send error: {e}", flush=True)
        time.sleep(0.2)
    m = bus.recv(0)
    while m is not None:
        if m.arbitration_id == REPLY_ID:
            print(f"[storm] TCU FRAME {bytes(m.data).hex()} after {time.time()-t0:.1f}s "
                  f"({sent} sent) — session open, exiting", flush=True)
            b.shutdown()
            sys.exit(0)
        m = bus.recv(0)
    time.sleep(0.002)
print(f"[storm] window closed ({WINDOW_S}s, {sent} sent) — no reply; "
      f"power OFF 30 s and retry", flush=True)
b.shutdown()
sys.exit(1)
