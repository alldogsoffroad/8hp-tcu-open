#!/usr/bin/env python3
"""READ-ONLY live-stock cell sweep for the first-500ms diff (first500ms_diff.md).

The bench is running the GENUINE STOCK at the rail. This script sends only the
meas-protocol hygiene byte (0x02) and memory reads (0x2A) on 0x361/0x398.
NO writes, NO arms, NO UDS session, NO routine calls. The meas path has been
used on the driving stock before (stockram_driving_20261104.bin was taken with it).
"""
import sys, time
sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench

class A:
    iface = "slcan"; bus1 = "/dev/ttyACM0"; bus2 = ""
    teensy_mode = None; keep_rate = True

def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)

b = tcu_bench.Buses(A())

def read6(addr):
    r = tcu_bench.meas(b, 1, [0x2A, 0, addr >> 24 & 0xFF, addr >> 16 & 0xFF, addr >> 8 & 0xFF, addr & 0xFF])
    if r is None or r[0] != 0x2A or r[1] != 0x81:
        raise RuntimeError(f"bad reply at 0x{addr:08X}: {r.hex() if r else 'none'}")
    return r[2:8]

def sweep(name, addr, n):
    out = bytearray()
    a = addr
    for _ in range(n):
        out += read6(a)
        a += 6
    print(f"{name} 0x{addr:08X}:")
    for i in range(0, len(out), 16):
        print(f"  {addr + i:08X}: " + " ".join(f"{x:02X}" for x in out[i:i+16]))
    return bytes(out)

# 1. HYGIENE — verify the meas engine answers before anything else
r = tcu_bench.meas(b, 1, [0x02])
ts(f"hygiene 0x02 reply: {r.hex() if r else 'NONE'}")
if r is None or r[0] not in (0x01, 0x02):
    sys.exit("meas hygiene failed — aborting (nothing written)")

# 2. protocol cells (letters/outputs/mode/request/grant/nib/bit2/echoes)
sweep("protocol cells", 0xFFF90940, 4)      # 0x940..0x95F: letters[3], faults, outputs_dis, shutdown, mode, bit3src, request, grant, telem?, nib, bit4, c9base, bit2, c9var, dirbits
sweep("ctrl/echo cells", 0xFFF907C8, 3)     # 0x7C8..0x7DF: J1 ok/err, telem, id cells, REG14, stat, ctrl staged, echo22, EN90, echo24
sweep("bus0 id cells",  0xFFF90970, 2)      # 0x970..0x97B incl. id_b0 cells + c9 staged
sweep("sslb2 cells",    0xFFF908CA, 2)      # telem avg + sslb2 copy/raw

# 3. staged wire words
sweep("J1 staged", 0xFFF88414, 2)           # 0x88414..0x88427: J5 stage + J1 8E/C9 staged
sweep("J2/J3 staged", 0xFFF88434, 2)        # 0x88434..0x8844F
sweep("J6 stage + J7", 0xFFF885C8, 4)       # 0x885CC J6 staged + 0x885D6 J6 rx stage + 0x885E0 J7

# 4. walk reply + FSM cells
sweep("walk replies", 0xFFF90540, 2)        # fresh/token/counts/fb/commit/staged84
sweep("FSM cells", 0xFFF93B80, 6)           # 0x3B80..0x3BAF: mode/return/oneshot, period, walk flags, lvl rep/exp, walk state
sweep("outer cells", 0xFFF95008, 1)         # sys mode, boot flag, outer fsm
sweep("value pairs", 0xFFF94FE0, 2)         # era count, duty ctx, value, pending
sweep("use counters", 0xFFF94210, 2)        # 0x94218.. job use counters

ts("sweep complete — no writes performed")
b.shutdown()
