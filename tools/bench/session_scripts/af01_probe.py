#!/usr/bin/env python3
"""af01 (SSLB1, five-select position 3) campaign.
Usage: af01_probe.py read [reg ...]   — read-only register probes (safe)
       af01_probe.py write <word>     — ONE write word to af01 slot (gated:
                                        announce + ammeter + explicit go)
Writes go ONLY to position 3 (af01); all other slots get reg-0 reads."""
import sys, threading, time
sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
import tcu_bench
from ammeter import Ammeter

class A:
    iface = "slcan"; bus1 = "/dev/ttyACM0"; bus2 = None   # ACM1 free for the ammeter
    teensy_mode = None; keep_rate = True

def ts(m): print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)

# live ammeter tap on the Teensy second port (needs the 2026-11-02 dual-port
# firmware; without it the logger is absent and we fall back to your PSU eyes)
am = None
am_log = []
def am_poll():
    while not stop.is_set():
        try:
            ma = am.poll()
            if ma is not None:
                am_log.append((time.time(), ma))
        except Exception:
            return
        time.sleep(0.1)
try:
    am = Ammeter(port="/dev/ttyACM1")
    stop = threading.Event()
    threading.Thread(target=am_poll, daemon=True).start()
    ts(f"live ammeter on ACM1: {am.ma if am.ma is not None else 'warming up'} mA")
except Exception as e:
    ts(f"NO live ammeter ({e.__class__.__name__}) — PSU verdict still needed")

def am_window(t0):
    """min/max/last of the samples since t0"""
    w = [ma for (t, ma) in am_log if t >= t0]
    if not w:
        return "I=n/a"
    return f"I={w[-1]:.0f} mA (min {min(w):.0f} / max {max(w):.0f})"

b = tcu_bench.Buses(A())
u = tcu_bench.Uds(b, 1)
r = u.extended()
if not r or r[0] != 0x50:
    sys.exit(f"extended failed: {r.hex() if r else None}")
tp = u.start_tester_present()

SEL = ["af31", "af21(SSLB2)", "af11(SUP)", "af01(SSLB1)", "af51"]

def fivesel(words, name):
    """words: list of up to 5 raw 16-bit words; padded with 0x0000 (reg-0 reads)."""
    words = (list(words) + [0x0000] * 5)[:5]
    t0 = time.time()
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
    ts(f"{name}: " + "  ".join(f"{SEL[i%5]}={rx[i]:04x}" for i in range(n)))
    ts(f"    [{am_window(t0)}]")
    return rx

try:
    r = u.request(b"\x22\xf1\x95")
    ts(f"ident: {r[3:].decode(errors='replace') if r else 'NO REPLY'}")

    mode = sys.argv[1] if len(sys.argv) > 1 else "read"

    if mode == "read":
        regs = [int(x, 16) for x in sys.argv[2:]] or [0x24, 0x90, 0x2E]
        for reg in regs:
            fivesel([reg << 8], f"read 0x{reg:02X}")
        ts("*** read phase done ***")

    elif mode == "write":
        word = int(sys.argv[2], 16)
        # one write word at position 3 (af01); slots 0,1,2,4 = reg-0 reads
        fivesel([0x0000, 0x0000, 0x0000, word, 0x0000], f"WRITE {word:04x} -> af01")
        for reg in (0x24, 0x90, 0x2E):
            fivesel([reg << 8], f"post-read 0x{reg:02X}")
        ts("*** write phase done — ammeter verdict please ***")

    elif mode == "pair":
        # stock job 0x40C0C shape: 90|enables then 24 read, same burst, on af01
        en = int(sys.argv[2], 16)
        w = [0x0000, 0x0000, 0x0000, 0x9000 | en, 0x0000,
             0x0000, 0x0000, 0x0000, 0x2400,      0x0000]
        rx = fivesel(w, f"PAIR 90|{en:02x} + 24read (stock job2 shape)")
        if rx and len(rx) >= 9:
            ts(f"    -> af01 word1 (90|{en:02x}) reply: {rx[3]:04x}   "
               f"af01 word2 (24 read) reply: {rx[8]:04x}   <<< the stock's 0x07D7 slot")
        time.sleep(0.5)
        for reg in (0x24, 0x2E):
            fivesel([reg << 8], f"post-read 0x{reg:02X}")
        ts("*** pair phase done — ammeter verdict please ***")

    elif mode == "repeat":
        # sustained pair rotation: N iterations of the 10-word one-shot, no
        # results reads in between, then one results read + post-read
        en = int(sys.argv[2], 16)
        n = int(sys.argv[3]) if len(sys.argv) > 3 else 50
        payload = bytes([0x31, 0x01, 0xFE, 0x13, 0x0A])
        for w in (0x0000, 0x0000, 0x0000, 0x9000 | en, 0x0000,
                  0x0000, 0x0000, 0x0000, 0x2400,      0x0000):
            payload += w.to_bytes(2, "big")
        t0 = time.time()
        ok = 0
        for i in range(n):
            r = u.request(payload)
            if r and r[0] == 0x71:
                ok += 1
        dt = time.time() - t0
        time.sleep(0.4)
        r = u.request(b"\x31\x03\xFE\x13")
        rx = None
        if r and len(r) > 6 and r[4] == 1 and r[5] > 3:
            k = r[5]
            rx = [int.from_bytes(r[6+2*j:8+2*j], "big") for j in range(k)]
            ts(f"REPEAT 90|{en:02x} x{n}: {ok}/{n} acked in {dt:.2f}s "
               f"({ok/dt:.0f}/s)  final af01 reply: {rx[3]:04x}")
        ts(f"    during-burst ammeter: {am_window(t0)}   "
           f"({len([1 for (t,_) in am_log if t>=t0])} samples)")
        time.sleep(0.5)
        fivesel([0x24 << 8], "post-read 0x24")
        ts("*** repeat done — ammeter verdict please ***")
    else:
        sys.exit(f"unknown mode {mode}")
finally:
    try:
        u.request(bytes([0x31, 0x02, 0xFE, 0x13]))
    except Exception:
        pass
    tp.set(); b.shutdown()
