#!/usr/bin/env python3
"""Bench ammeter logger — the ACS712 telemetry stream from the Teensy bridge
(spec: 8hp-teensy-flasher docs/protocol.md §7; calibrated 2026-11-02 against
the PSU: ±6 mA absolute, ~±3 mA on a 1 s average, rail transitions instant).

Run standalone for a live log:
    python3 tools/bench/ammeter.py [seconds] [outfile]
Or import Ammeter for scripted use (context manager; .ma property = the
latest 1 s rolling mean; the log carries wall-clock + monotonic stamps so
current lines align with every bench script's log).

Telemetry: the Teensy emits 'A<milliamps>' lines on /dev/ttyACM0 when power
monitoring is on ('A1' enables, 'A0' stops). NOTE: the enable command and the
slcan bridge share the port — close or pause this logger before running any
script that opens ACM0 itself, or point your scripts' bus1 elsewhere.
"""
import serial
import statistics
import sys
import time


class Ammeter:
    PORT = "/dev/ttyACM0"
    BAUD = 1_500_000

    def __init__(self, port=None):
        self.ser = serial.Serial(port or self.PORT, self.BAUD, timeout=0.05)
        self.ser.reset_input_buffer()
        self.ser.write(b"A1\r")
        self.samples = []          # (monotonic_t, ma)
        self._drain()

    def _drain(self):
        now = time.monotonic()
        for ln in self.ser.read(4000).decode(errors="replace").splitlines():
            ln = ln.strip()
            if ln.startswith("A") and ln[1:].lstrip("-").isdigit():
                self.samples.append((now, int(ln[1:])))
        self.samples = [s for s in self.samples if now - s[0] <= 5.0]

    def poll(self):
        self._drain()
        return self.ma

    @property
    def ma(self):
        """rolling ~1 s mean, None before the first samples arrive"""
        now = time.monotonic()
        w = [ma for (t, ma) in self.samples if now - t <= 1.0]
        return statistics.mean(w) if w else None

    def close(self):
        try:
            self.ser.write(b"A0\r")
        except Exception:
            pass
        self.ser.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def main():
    secs = float(sys.argv[1]) if len(sys.argv) > 1 else 30.0
    out = open(sys.argv[2], "a") if len(sys.argv) > 2 else None
    with Ammeter() as am:
        t0 = time.time()
        last = None
        while time.time() - t0 < secs:
            ma = am.poll()
            time.sleep(0.1)
            if ma is None:
                continue
            if last is None or abs(ma - last) >= 2.0:
                line = (f"{time.strftime('%H:%M:%S')} t={time.time()-t0:7.1f}s "
                        f"I={ma:7.1f} mA n={len(am.samples)}")
                print(line, flush=True)
                if out:
                    out.write(line + "\n")
                    out.flush()
                last = ma
        print(f"final: {am.ma:.1f} mA")


if __name__ == "__main__":
    main()
