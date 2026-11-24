#!/usr/bin/env python3
"""
jl_shifter_capture.py - capture the JL Wrangler electronic shifter (P68268664AG) on the bench.

The shifter is powered alone; one Teensy SLCAN channel (firmware d40_kline) is wired to one of
its CAN pairs. Only the "wake" mode sends anything: one frame, ID 0x7F0, eight zero bytes (a neutral ID, not the
TCM's 0x300), to wake a sleeping shifter. The "tcm" mode replays the stock TCU's own 0x300 on the
shifter bus (bench log 20261017_stock_first_power.log, stock firmware, no vehicle): 6 frames
00 00 FF 00, 3 frames 00 00 4E 02, then 00 00 4E 42, every 20 ms, counter in the high nibble of
byte 4 (first value 1), CRC-8 SAE J1850 over bytes 0-4 in byte 5. "waketcm" = wake (one 0x7F0,
retried by the controller until acknowledged), then the stock 0x300 from the first 0x210 on.
sniff and ack never send.

  sniff  listen-only + automatic bit rate (firmware P2): the Teensy never acknowledges and never
         sends error flags, so a lone shifter keeps repeating its first unacknowledged frame.
         Use it to find the bit rate.
  wake   as ack, plus the one 0x7F0 frame 1 s after opening; the error counters are polled every 0.5 s and printed when they change
         (TEC > 0 with ACK errors = nothing acknowledged it).
  ack    normal open at a fixed bit rate, still sending nothing: the Teensy controller
         acknowledges each frame the way a TCU would, so the shifter runs its whole message set.

Every received frame is written to the log (host time in seconds, ID, DLC, data). The console
shows each new ID, each data change of a watched ID (--watch, default 0x210 0x2EA 0x200 0x2FA),
and at the end a per-ID summary (count, mean period, last data).

Usage:
  python3 jl_shifter_capture.py sniff --seconds 10 --log jl_sniff.log
  python3 jl_shifter_capture.py ack --rate 500 --seconds 60 --log jl_idle.log
  python3 jl_shifter_capture.py ack --rate 500 --seconds 90 --log jl_lever.log --changes all
"""
import argparse
import sys
import time

import serial

RATE_CODE = {10: "S0", 20: "S1", 50: "S2", 100: "S3", 125: "S4", 250: "S5", 500: "S6",
             800: "S7", 1000: "S8", 666: "S9"}
AUTO_MARKER = 0x1FFFFF00


def crc8_j1850(d):
    c = 0xFF
    for b in d:
        c ^= b
        for _ in range(8):
            c = ((c << 1) ^ 0x1D) & 0xFF if c & 0x80 else (c << 1) & 0xFF
    return c ^ 0xFF


def stock_300(n):
    """n-th (0-based) frame of the stock TCU's 0x300 sequence."""
    head = [0x00, 0x00, 0xFF, 0x00] if n < 6 else [0x00, 0x00, 0x4E, 0x02] if n < 9 \
        else [0x00, 0x00, 0x4E, 0x42]
    d = head + [((n + 1) & 0xF) << 4]
    return bytes(d + [crc8_j1850(d)])


def cmd(sp, s, wait=0.05):
    sp.write((s + "\r").encode())
    time.sleep(wait)


def parse(line):
    """'tIIILDD..' / 'TIIIIIIIILDD..' -> (id, ext, data) or None."""
    if not line or line[0] not in "tT":
        return None
    ext = line[0] == "T"
    n = 8 if ext else 3
    try:
        cid = int(line[1:1 + n], 16)
        dlc = int(line[1 + n], 16)
        data = bytes.fromhex(line[2 + n:2 + n + 2 * dlc])
    except (ValueError, IndexError):
        return None
    if len(data) != dlc:
        return None
    return cid, ext, data


def main():
    ap = argparse.ArgumentParser(description="JL shifter bench capture (receive only)")
    ap.add_argument("mode", choices=["sniff", "ack", "wake", "tcm", "waketcm"])
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--rate", type=int, default=500, help="kbit/s for ack mode (sniff starts at 500)")
    ap.add_argument("--seconds", type=float, default=30)
    ap.add_argument("--log", required=True)
    ap.add_argument("--watch", default="0x210,0x2EA,0x200,0x2FA",
                    help="IDs whose every data change is printed")
    ap.add_argument("--changes", default="watch", choices=["watch", "all"],
                    help="print data changes of the watched IDs only, or of every ID")
    a = ap.parse_args()
    if a.rate not in RATE_CODE:
        sys.exit(f"rate must be one of {sorted(RATE_CODE)}")
    watch = {int(x, 0) for x in a.watch.split(",") if x}

    sp = serial.Serial(a.port, 2000000, timeout=0.05)
    cmd(sp, "C")
    cmd(sp, "Z0")
    if a.mode == "sniff":
        cmd(sp, "P2")
        cmd(sp, "S6")
    else:
        cmd(sp, "P0")
        cmd(sp, RATE_CODE[a.rate])
    sp.reset_input_buffer()
    cmd(sp, "O")
    print(f"{a.mode}: {a.port} open ({'listen-only, auto bit rate' if a.mode == 'sniff' else f'normal, {a.rate} kbit/s, acknowledging' + (', ONE 0x7F0 frame at 1 s' if a.mode == 'wake' else ', sending the stock 0x300 every 20 ms' if a.mode == 'tcm' else ', wake frame at 1 s, then the stock 0x300 from the first 0x210' if a.mode == 'waketcm' else ', sending nothing')})"
          f" for {a.seconds:.0f} s -> {a.log}")

    stats, last, buf = {}, {}, b""
    t0 = time.monotonic()
    sent = a.mode not in ("wake", "waketcm")
    tcm_on = a.mode == "tcm"
    n300, t300 = 0, 0.0
    t_poll, last_e = 0.0, None
    with open(a.log, "w") as log:
        log.write(f"# jl_shifter_capture {a.mode} port={a.port} rate={a.rate} start={time.ctime()}\n")
        try:
            while time.monotonic() - t0 < a.seconds:
                now = time.monotonic() - t0
                if tcm_on and now >= t300:
                    t300 = max(t300 + 0.020, now - 0.010)
                    fr = stock_300(n300)
                    n300 += 1
                    sp.write(("t3006" + fr.hex().upper() + "\r").encode())
                    if n300 <= 10 or n300 % 500 == 0:
                        log.write(f"{now:.4f} SENT 300 6 {fr.hex(' ').upper()}\n")
                    if n300 % 250 == 0:
                        sp.write(b"E\r")
                if not sent and now >= 1.0:
                    sp.write(b"t7F080000000000000000\r")
                    sent, t_sent = True, now
                    print(f"{now:7.3f}  SENT 7F0 [8] 00 00 00 00 00 00 00 00")
                    log.write(f"{now:.4f} SENT 7F0 8 00 00 00 00 00 00 00 00\n")
                elif a.mode in ("wake", "waketcm") and sent and not tcm_on and now >= t_poll:
                    t_poll = now + 0.5
                    sp.write(b"E\r")
                buf += sp.read(4096)
                *lines, buf = buf.split(b"\r")
                for raw in lines:
                    line = raw.decode(errors="replace").strip("\x07\n ")
                    fr = parse(line)
                    if fr is None:
                        if line.startswith("E") and len(line) >= 13 and line[:5] != last_e \
                                and line[1:5] != "0000":
                            last_e = line[:5]
                            print(f"{time.monotonic() - t0:7.3f}  error state {line}"
                                  f" (TEC {int(line[1:3], 16)}, REC {int(line[3:5], 16)})")
                            log.write(f"{time.monotonic() - t0:.4f} ESTATE {line}\n")
                        continue
                    cid, ext, data = fr
                    t = time.monotonic() - t0
                    if ext and cid == AUTO_MARKER:
                        print(f"{t:7.3f}  bit rate now {int.from_bytes(data[:2], 'big')} kbit/s")
                        log.write(f"{t:.4f} AUTO {int.from_bytes(data[:2], 'big')}\n")
                        continue
                    idtxt = f"{cid:08X}" if ext else f"{cid:03X}"
                    if a.mode == "waketcm" and not tcm_on and cid == 0x210 and not ext:
                        tcm_on, t300 = True, time.monotonic() - t0
                        print(f"{t300:7.3f}  first 0x210: starting the stock 0x300 every 20 ms")
                        log.write(f"{t300:.4f} TCM_START\n")
                    log.write(f"{t:.4f} {idtxt} {len(data)} {data.hex(' ').upper()}\n")
                    s = stats.setdefault(idtxt, [0, t, t])
                    s[0] += 1
                    s[2] = t
                    if idtxt not in last:
                        print(f"{t:7.3f}  NEW {idtxt} [{len(data)}] {data.hex(' ').upper()}")
                    elif last[idtxt] != data and (a.changes == "all" or cid in watch):
                        print(f"{t:7.3f}      {idtxt} [{len(data)}] {data.hex(' ').upper()}")
                    last[idtxt] = data
        except KeyboardInterrupt:
            print("\nstopped")
        finally:
            sp.write(b"E\r")
            time.sleep(0.1)
            err = sp.read(256).decode(errors="replace").replace("\r", " ").strip()
            cmd(sp, "C")
            cmd(sp, "P0")
            sp.close()
    print(f"\nerror state (E<TEC><REC><ESR1>): {err}")
    print(f"{'ID':>8} {'count':>7} {'period ms':>10}  last data")
    for idtxt in sorted(stats, key=lambda k: int(k, 16)):
        n, first, lastt = stats[idtxt]
        per = (lastt - first) / (n - 1) * 1000 if n > 1 else float("nan")
        print(f"{idtxt:>8} {n:7d} {per:10.1f}  {last[idtxt].hex(' ').upper()}")


if __name__ == "__main__":
    main()
