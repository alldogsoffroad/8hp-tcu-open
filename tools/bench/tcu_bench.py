#!/usr/bin/env python3
"""TCU bench tool (Teensy 4.1 SLCAN adapter or any python-can interface).

Bus naming follows the project: bus 1 = CAN-C (TCU pins 5/6), bus 2 = shifter bus (pins 3/4).
Which TCU pin pair is RCAN bus 1 in the firmware is bench item 18; `monitor` reports where
0x7F0 (sent on firmware bus 1 only) arrives.

Interfaces:
  --iface socketcan --bus1 can0 --bus2 can1                 (after slcand, see tools/bench/README.md)
  --iface slcan --bus1 /dev/ttyACM0 --bus2 /dev/ttyACM1     (direct, no slcand / root needed)

Commands:
  monitor                 decode bench frames 0x7F0..0x7F3, detect resets, list IDs per bus
  log FILE                log both buses in candump -L format (replayable by the simulator)
  uds [--bus N] HEX...    send one UDS request on 0x7E1, print the response from 0x7E9
  info                    software version and status identifiers FD00..FD02 (project firmware)
  meas-ident [--bus N]    measurement protocol identification (0x01 / 0x21)
  meas-read ADDR LEN [--bus N] [-o FILE]   read memory with measurement command 0x2A
  item20                  bench item 20 on the STOCK firmware: dump the boot request record
  solenoid [--csv FILE]   guided M3 solenoid test (DUMMY LOADS ONLY), records ammeter readings
  canstat [--send]        Teensy adapter error counters per bus (slcan only); --send first
                          transmits a tester-present frame on each bus to provoke ACK errors
"""
import argparse
import csv
import sys
import threading
import time

try:
    import can
except ImportError:
    sys.exit("python-can is required: pip install python-can")


# ----------------------------------------------------------------------------- buses

class Buses:
    def __init__(self, args):
        self.names = {1: args.bus1, 2: args.bus2}
        self.bus = {}
        for n, chan in self.names.items():
            if not chan:
                continue
            kw = {"interface": args.iface, "channel": chan}
            if args.iface == "slcan":
                kw["bitrate"] = 500000            # USB CDC: the serial baud rate is irrelevant
            self.bus[n] = can.Bus(**kw)

    def get(self, n):
        if n not in self.bus:
            sys.exit(f"bus {n} is not configured")
        return self.bus[n]

    def send(self, n, arb_id, data):
        data = bytes(data)
        self.get(n).send(can.Message(arbitration_id=arb_id, data=data, is_extended_id=False))

    def shutdown(self):
        for b in self.bus.values():
            try:
                b.shutdown()
            except Exception:
                pass                      # already closed (canstat uses the raw ports)
        self.bus = {}


# ----------------------------------------------------------------------------- ISO-TP / UDS

class Uds:
    """Minimal ISO-TP client: single-frame requests, single- or multi-frame responses."""

    def __init__(self, buses, n=1, req=0x7E1, rsp=0x7E9):
        self.b, self.n, self.req, self.rsp = buses, n, req, rsp
        self.lock = threading.Lock()
        self._tp = None

    def request(self, payload, timeout=1.0):
        payload = bytes(payload)
        if len(payload) > 7:
            raise ValueError("only single-frame requests are supported")
        with self.lock:
            bus = self.b.get(self.n)
            while bus.recv(0) is not None:      # drop stale frames
                pass
            self.b.send(self.n, self.req, bytes([len(payload)]) + payload + b"\xAA" * (7 - len(payload)))
            end = time.time() + timeout
            data, need = b"", None
            while time.time() < end:
                m = bus.recv(0.05)
                if m is None or m.arbitration_id != self.rsp:
                    continue
                pci = m.data[0] >> 4
                if pci == 0:
                    resp = bytes(m.data[1:1 + (m.data[0] & 0x0F)])
                    if len(resp) >= 3 and resp[0] == 0x7F and resp[2] == 0x78:
                        end = time.time() + 5.0    # response pending
                        continue
                    return resp
                if pci == 1:
                    need = ((m.data[0] & 0x0F) << 8) | m.data[1]
                    data = bytes(m.data[2:8])
                    self.b.send(self.n, self.req, b"\x30\x00\x00" + b"\xAA" * 5)
                elif pci == 2 and need is not None:
                    data += bytes(m.data[1:8])
                    if len(data) >= need:
                        return data[:need]
            return None

    def extended(self):
        """Enter the extended session. The stock firmware ignores the first 10 03 after power-up,
        so send 10 01 first (spec 15 section 4a); the project firmware accepts either order."""
        self.request(b"\x10\x01")
        return self.request(b"\x10\x03")

    def start_tester_present(self, period=2.0):
        stop = threading.Event()

        def run():
            while not stop.wait(period):
                with self.lock:
                    self.b.send(self.n, self.req, b"\x02\x3E\x80" + b"\xAA" * 5)
        t = threading.Thread(target=run, daemon=True)
        t.start()
        self._tp = stop
        return stop


def hexs(b):
    return " ".join(f"{x:02X}" for x in b) if b is not None else "(no response)"


# ----------------------------------------------------------------------------- decoders

QUAL = ["eng spd", "T1", "T2", "T3", "pedal", "eng temp", "brake", "eng mode", "park spd",
        "soak", "park req", "lever"]
SOL_F = {0x01: "bus", 0x02: "ID", 0x04: "config", 0x08: "header", 0x10: "readback",
         0x20: "not ready"}
END = ["-", "timeout", "stopped", "session", "engine", "driver fault", "supervisor fault"]


def decode(arb, d):
    if arb == 0x7F0 and len(d) == 8:
        f = d[1]
        return (f"heartbeat #{d[0]:3d} up {d[2] << 8 | d[3]:5d}s idle {int.from_bytes(d[4:8], 'big')}"
                f" missed={f & 1} sup_ok={f >> 1 & 1} sup_bad={f >> 2 & 1} out_en={f >> 3 & 1}")
    if arb == 0x7F1 and len(d) == 8:
        q = d[4] << 8 | d[5]
        ok = [QUAL[i] for i in range(12) if q >> i & 1]
        return (f"inputs: engine {d[0] << 8 | d[1]} rpm, lever {d[2]} (src {d[3]}), pedal {d[6] / 2:.1f}%,"
                f" brake {d[7]}; valid: {', '.join(ok) or 'none'}")
    if arb == 0x7F2 and len(d) == 8:
        t = "--" if d[4] == 0xFF else f"{d[4] - 40}"
        park = ["unknown", "P", "not P", "?"][d[6] & 3]
        return (f"sensors: TIF12 {d[0] << 8 | d[1]} rpm, TIF13 {d[2] << 8 | d[3]} rpm, fluid {t} C,"
                f" batt {d[5] / 10:.1f} V, park {park}, wake {d[6] >> 2 & 1}/{d[6] >> 3 & 1},"
                f" valid {d[6] >> 4:04b}, tft faults 0x{d[7]:02X}")
    if arb == 0x7F3 and len(d) == 8:
        faults = ", ".join(v for k, v in SOL_F.items() if d[0] & k) or "none"
        end = END[d[6]] if d[6] < len(END) else str(d[6])
        return (f"outputs: driver faults [{faults}] ready={d[1] & 1} sup_en={d[1] >> 1 & 1}"
                f" bench={d[1] >> 2 & 1} ch {d[2]} {d[3] << 8 | d[4]} mA left {d[5] / 10:.1f}s"
                f" last end: {end}, chip0 status 0x{d[7]:02X}")
    return None


# ----------------------------------------------------------------------------- commands

def cmd_monitor(b, args):
    seen = {1: {}, 2: {}}
    last_hb = None
    t_report = time.time() + 5
    print("monitoring (Ctrl-C to stop)")
    try:
        while True:
            for n, bus in b.bus.items():
                m = bus.recv(0.01)
                if m is None:
                    continue
                seen[n][m.arbitration_id] = seen[n].get(m.arbitration_id, 0) + 1
                txt = decode(m.arbitration_id, m.data)
                if txt:
                    if m.arbitration_id == 0x7F0:
                        if last_hb is not None and m.data[0] != (last_hb + 1) & 0xFF:
                            print(f"*** heartbeat counter jumped {last_hb} -> {m.data[0]}: RESET? ***")
                        last_hb = m.data[0]
                    print(f"[bus {n}] {txt}")
            if time.time() > t_report:
                t_report = time.time() + 5
                for n in seen:
                    if seen[n]:
                        ids = " ".join(f"{k:03X}:{v}" for k, v in sorted(seen[n].items()))
                        print(f"--- bus {n} ({b.names[n]}) IDs in 5 s: {ids}")
                    seen[n] = {}
    except KeyboardInterrupt:
        pass


def cmd_log(b, args):
    names = {1: "can0", 2: "can1"}      # simulator replay: can0 = bus 1, can1 = bus 2
    n_frames = 0
    with open(args.file, "w") as f:
        print(f"logging both buses to {args.file} (Ctrl-C to stop)")
        try:
            while True:
                for n, bus in b.bus.items():
                    m = bus.recv(0.005)
                    if m is None:
                        continue
                    ts = m.timestamp or time.time()
                    f.write(f"({ts:.6f}) {names[n]} {m.arbitration_id:03X}#{m.data.hex().upper()}\n")
                    n_frames += 1
        except KeyboardInterrupt:
            pass
    print(f"{n_frames} frames written")


def cmd_uds(b, args):
    u = Uds(b, args.bus)
    print(hexs(u.request(bytes.fromhex("".join(args.hex)))))


def cmd_info(b, args):
    u = Uds(b, args.bus)
    r = u.request(b"\x22\xF1\x95")
    print("software:", r[3:].decode(errors="replace") if r and r[0] == 0x62 else hexs(r))
    for did in (0xFD00, 0xFD01, 0xFD02):
        r = u.request(bytes([0x22, did >> 8, did & 0xFF]))
        if r and r[0] == 0x62:
            arb = {0xFD00: 0x7F1, 0xFD01: 0x7F2, 0xFD02: 0x7F3}[did]
            print(f"{did:04X}:", decode(arb, r[3:11]))
        else:
            print(f"{did:04X}:", hexs(r))


def meas(b, n, req, timeout=0.5):
    bus = b.get(n)
    while bus.recv(0) is not None:
        pass
    b.send(n, 0x361, bytes(req) + bytes(8 - len(req)))
    end = time.time() + timeout
    while time.time() < end:
        m = bus.recv(0.05)
        if m is not None and m.arbitration_id == 0x398:
            return bytes(m.data)
    return None


def cmd_meas_ident(b, args):
    text = b""
    r = meas(b, args.bus, [0x01])
    for _ in range(4):
        if r is None or r[0] != 0x21:
            print("reply:", hexs(r))
            break
        text += r[2:8]
        r = meas(b, args.bus, [0x21])
    print("identification:", text)


def read_mem(b, n, addr, length):
    out = bytearray()
    a = addr
    while len(out) < length:
        r = meas(b, n, [0x2A, 0, a >> 24 & 0xFF, a >> 16 & 0xFF, a >> 8 & 0xFF, a & 0xFF])
        if r is None or r[0] != 0x2A or r[1] != 0x81:
            raise RuntimeError(f"no / bad reply at 0x{a:08X}: {hexs(r)}")
        out += r[2:8]
        a += 6
    return bytes(out[:length])


def dump(addr, data):
    for i in range(0, len(data), 16):
        print(f"{addr + i:08X}: " + " ".join(f"{x:02X}" for x in data[i:i + 16]))


def cmd_meas_read(b, args):
    addr, length = int(args.addr, 0), int(args.len, 0)
    data = read_mem(b, args.bus, addr, length)
    dump(addr, data)
    if args.output:
        open(args.output, "wb").write(data)


def cmd_item20(b, args):
    print("Bench item 20 (STOCK firmware). Reading the boot request record 0xFFF80080..0xFFF800C7")
    print("through the measurement protocol on bus", args.bus)
    data = read_mem(b, args.bus, 0xFFF80080, 0x48)
    dump(0xFFF80080, data)
    name = f"item20_record_{time.strftime('%Y%m%d_%H%M%S')}.bin"
    open(name, "wb").write(data)
    print(f"saved {name}")
    print("Next: start `tcu_bench.py log item20_acdp2.log`, then start a WRITE with the ACDP-2 and")
    print("stop logging once the tool has entered programming. Do not let the write complete if the")
    print("image is not the unit's own stock image.")


def cmd_solenoid(b, args):
    print("M3 SOLENOID BENCH TEST - DUMMY LOADS ONLY, NEVER ON A TRANSMISSION")
    if input("Dummy load and ammeter connected, transmission NOT connected? [yes/no] ").strip() != "yes":
        return
    u = Uds(b, args.bus)
    r = u.extended()
    if not r or r[0] != 0x50:
        sys.exit(f"extended session failed: {hexs(r)}")
    stop_tp = u.start_tester_present()
    names = ["A", "D", "B", "E", "C", "TCC", "line pressure"]
    rows = []
    try:
        while True:
            line = input("channel (0-6) current_mA (10-1000) seconds (1-5), or q: ").split()
            if not line or line[0] == "q":
                break
            ch, ma, sec = int(line[0]), int(line[1]), int(line[2])
            r = u.request(bytes([0x31, 0x01, 0xFE, 0x10, ch, ma // 10, sec]))
            if not r or r[0] != 0x71:
                print("refused:", hexs(r))
                continue
            print(f"channel {ch} ({names[ch] if ch < 7 else '?'}) at {ma // 10 * 10} mA for {sec} s")
            meas_ma = input("ammeter reading in mA (Enter to skip): ").strip()
            res = u.request(b"\x31\x03\xFE\x10")
            print("result:", hexs(res))
            rows.append({"time": time.strftime("%H:%M:%S"), "channel": ch, "requested_mA": ma // 10 * 10,
                         "seconds": sec, "measured_mA": meas_ma, "result": hexs(res)})
    finally:
        u.request(b"\x31\x02\xFE\x10")
        stop_tp.set()
    if rows and args.csv:
        with open(args.csv, "a", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0]))
            if f.tell() == 0:
                w.writeheader()
            w.writerows(rows)
        print(f"{len(rows)} rows appended to {args.csv}")


def cmd_canstat(b, args):
    import serial
    ports = {1: args.bus1, 2: args.bus2}
    b.shutdown()                                  # use the raw serial ports
    for n, port in ports.items():
        if not port:
            continue
        s = serial.Serial(port, 2000000, timeout=0.3)
        s.reset_input_buffer()
        for cmd in (b"C\r", b"S6\r", b"O\r"):
            s.write(cmd); time.sleep(0.03); s.read(16)
        if args.send:
            for _ in range(3):
                s.write(b"t7E10802" + b"3E00AAAAAAAAAA" + b"\r"); time.sleep(0.05)
            s.read(64)
        time.sleep(0.2)
        s.reset_input_buffer()
        s.write(b"E\r")
        r = s.read_until(b"\r").decode(errors="replace").strip()
        s.write(b"C\r")
        s.close()
        if not r.startswith("E") or len(r) < 13:
            print(f"bus {n} ({port}): no error-state reply ({r!r}); reflash the Teensy sketch")
            continue
        tec, rec, esr = int(r[1:3], 16), int(r[3:5], 16), int(r[5:13], 16)
        conf = ["error active", "error passive", "bus off", "bus off"][esr >> 4 & 3]
        flags = [name for bit, name in ((13, "ACK error"), (14, "bit0 error"), (15, "bit1 error"),
                                        (10, "stuff error"), (11, "form error"), (12, "CRC error"),
                                        (18, "synchronised"), (7, "idle")) if esr >> bit & 1]
        print(f"bus {n} ({port}): TEC={tec} REC={rec} {conf}; {', '.join(flags) or 'no flags'}")
    print("ACK errors / TEC rising = nobody acknowledges (TCU off or not on the bus); "
          "stuff/form/CRC/bit errors or REC rising = CAN_H/CAN_L swapped or wrong bit rate; "
          "not 'synchronised' = the controller sees no bus activity at all.")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--iface", default="socketcan")
    ap.add_argument("--bus1", default="can0", help="interface/channel for TCU pins 5/6")
    ap.add_argument("--bus2", default="can1", help="interface/channel for TCU pins 3/4 ('' = none)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("monitor")
    p = sub.add_parser("log"); p.add_argument("file")
    p = sub.add_parser("uds"); p.add_argument("--bus", type=int, default=1); p.add_argument("hex", nargs="+")
    p = sub.add_parser("info"); p.add_argument("--bus", type=int, default=1)
    p = sub.add_parser("meas-ident"); p.add_argument("--bus", type=int, default=2)
    p = sub.add_parser("meas-read"); p.add_argument("addr"); p.add_argument("len")
    p.add_argument("--bus", type=int, default=2); p.add_argument("-o", "--output")
    p = sub.add_parser("item20"); p.add_argument("--bus", type=int, default=2)
    p = sub.add_parser("solenoid"); p.add_argument("--bus", type=int, default=1)
    p.add_argument("--csv", default="m3_solenoid_results.csv")
    p = sub.add_parser("canstat"); p.add_argument("--send", action="store_true")
    args = ap.parse_args()

    b = Buses(args)
    try:
        {"monitor": cmd_monitor, "log": cmd_log, "uds": cmd_uds, "info": cmd_info,
         "meas-ident": cmd_meas_ident, "meas-read": cmd_meas_read, "item20": cmd_item20,
         "solenoid": cmd_solenoid, "canstat": cmd_canstat}[args.cmd](b, args)
    finally:
        b.shutdown()


if __name__ == "__main__":
    main()
