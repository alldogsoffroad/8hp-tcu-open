#!/usr/bin/env python3
"""TCU bench tool (Teensy 4.1 SLCAN adapter or any python-can interface).

Bus naming follows the project: bus 1 = CAN-C (TCU pins 5/6), bus 2 = shifter bus (pins 3/4).
Which TCU pin pair is RCAN bus 1 in the firmware is bench item 18; `monitor` reports where
0x7F0 (sent on firmware bus 1 only) arrives.

Interfaces:
  default: the Teensy directly (--iface slcan --bus1 /dev/ttyACM0 --bus2 /dev/ttyACM1, no slcand / root)
  --iface socketcan --bus1 can0 --bus2 can1                 (after slcand, see tools/bench/README.md)

Commands:
  monitor                 decode bench frames 0x7F0..0x7F3, detect resets, list IDs per bus
  log FILE [--active]     log both buses in candump -L format (replayable by the simulator).
                          With the Teensy (slcan) the adapter is LISTEN-ONLY with automatic bit
                          rate by default, so it never disturbs the bus (a normal node broke an
                          ACDP-2 flash read); rate changes appear as frames 1FFFFF00#<kbit/s>.
                          --active: acknowledge frames like a normal node (other adapters).
  uds [--bus N] HEX...    send one UDS request on 0x7E1, print the response from 0x7E9
  info                    software version and status identifiers FD00..FD02 (project firmware)
  meas-ident [--bus N]    measurement protocol identification (0x01 / 0x21)
  meas-read ADDR LEN [--bus N] [-o FILE]   read memory with measurement command 0x2A
  item20                  bench item 20 on the STOCK firmware: dump the boot request record
  solenoid [--csv FILE]   guided M3 solenoid test (DUMMY LOADS ONLY), records ammeter readings
  dump-userboot [-o FILE] PROJECT firmware: copy the 32 KB user boot MAT to RAM (routine FE20),
                          download it with the measurement protocol, check the CRC-32
  meas-read ... --compare IMAGE   compare the read bytes with a full flash image (_IFla.bin)
  app-flash [IMAGE]       spec 18: write the RAM application (build/sh2a/tcu_app_df.bin) to data
                          flash through the loader (reached via FE30 from a running application,
                          or switch the TCU on when asked). Header written last, power-loss safe.
  sensors [--period S]    PROJECT firmware: live battery, fluid temperature (with AN8/AN1), park
                          contacts and speed channels from the measurement table (bench 22/23)
  app-selftest            spec 18: erase / program / verify / erase data-flash block DB07
  flash IMAGE [--dry-run] [--yes]   PROJECT firmware: rewrite the application / calibration blocks
                          that differ from IMAGE (_IFla.bin) over bus 1 with the RAM flasher
                          (spec 16). Power-loss safe order; --dry-run only compares.
  flash-selftest IMAGE [--block ADDR]   erase / program / verify / erase one block that IMAGE and
                          the TCU leave blank (default 0x100000); spec 16 section 8 step 3
  flash-invalidate        erase the application marker block ON PURPOSE (safety-net check, spec 16
                          section 8 step 4); afterwards only the ACDP-2 can restore the TCU
  plant-entry [--mode can|kcn]   send the user boot MAT plant entry (29-bit 0x1F0C3CFE,
                          "PlantCan") at 666.667 kbit/s while the TCU is switched on; log replies
                          (spec 17). Read-only; power-cycle the TCU afterwards.
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
    can = None                  # checked when a bus is opened (tests import this module without it)


# ----------------------------------------------------------------------------- buses

class Buses:
    def __init__(self, args):
        if can is None:
            sys.exit("python-can is required: pip install python-can")
        self.names = {1: args.bus1, 2: args.bus2}
        self.bus = {}
        self._lock = threading.Lock()
        if args.iface == "slcan" and getattr(args, "teensy_mode", b"P0"):
            # the Teensy keeps listen-only (P1/P2, set by `log`) until reset and then silently
            # drops every frame we send: return to normal mode unless the caller set a mode
            teensy_passive([args.bus1, args.bus2], getattr(args, "teensy_mode", b"P0"))
        for n, chan in self.names.items():
            if not chan:
                continue
            kw = {"interface": args.iface, "channel": chan}
            if args.iface == "slcan" and not getattr(args, "keep_rate", False):
                kw["bitrate"] = 500000            # USB CDC: the serial baud rate is irrelevant
            self.bus[n] = can.Bus(**kw)

    def get(self, n):
        if n not in self.bus:
            sys.exit(f"bus {n} is not configured")
        return self.bus[n]

    def send(self, n, arb_id, data):
        data = bytes(data)
        with self._lock:                  # park-check sends from a second thread
                self.get(n).send(can.Message(arbitration_id=arb_id, data=data, is_extended_id=arb_id > 0x7FF))

    def shutdown(self):
        for b in self.bus.values():
            try:
                b.shutdown()
            except Exception:
                pass                      # already closed (canstat uses the raw ports)
        self.bus = {}


# ----------------------------------------------------------------------------- ISO-TP / UDS

class Uds:
    """Minimal ISO-TP client: single- or multi-frame requests and responses."""

    REQ, RSP = 0x7E1, 0x7E9         # --uds-ext: 29-bit 0x07E00000 / 0x07E80000 (images built before
                                    # the MCR15 fix, 2026-10-17: their identifiers are shifted)

    def __init__(self, buses, n=1, req=None, rsp=None):
        self.b, self.n = buses, n
        self.req = Uds.REQ if req is None else req
        self.rsp = Uds.RSP if rsp is None else rsp
        # minimum gap between our consecutive frames, whatever the receiver's STmin says: the
        # flasher / loader poll one receive mailbox and lose back-to-back frames (bench 2026-10-18)
        self.cf_gap_s = 0.0
        self.honor_stmin = True     # tests on the simulated link (no frame loss) switch it off
        self.lock = threading.Lock()
        self._tp = None

    def _wait_fc(self, bus, timeout=1.0):
        end = time.time() + timeout
        while time.time() < end:
            m = bus.recv(0.05)
            if m is not None and m.arbitration_id == self.rsp and m.data[0] >> 4 == 3:
                if m.data[0] & 0x0F == 0:
                    return m.data[1], m.data[2]
                if m.data[0] & 0x0F == 2:
                    return None
                end = time.time() + timeout          # wait
        return None

    def _send(self, bus, payload):
        if len(payload) <= 7:
            self.b.send(self.n, self.req, bytes([len(payload)]) + payload + b"\xAA" * (7 - len(payload)))
            return True
        self.b.send(self.n, self.req, bytes([0x10 | len(payload) >> 8, len(payload) & 0xFF]) + payload[:6])
        fc = self._wait_fc(bus)
        if fc is None:
            return False
        bs, stmin = fc
        off, sn, left = 6, 1, bs
        while off < len(payload):
            chunk = payload[off:off + 7]
            self.b.send(self.n, self.req, bytes([0x20 | sn & 0x0F]) + chunk + b"\xAA" * (7 - len(chunk)))
            off, sn = off + 7, sn + 1
            gap = stmin / 1000.0 if stmin <= 0x7F else (stmin - 0xF0) / 10000.0 if 0xF1 <= stmin <= 0xF9 else 0.001
            gap = max(gap if stmin and self.honor_stmin else 0.0, self.cf_gap_s)
            if gap:
                time.sleep(gap)
            if bs and off < len(payload):
                left -= 1
                if left == 0:
                    fc = self._wait_fc(bus)
                    if fc is None:
                        return False
                    left = fc[0]
        return True

    def request(self, payload, timeout=1.0):
        payload = bytes(payload)
        with self.lock:
            bus = self.b.get(self.n)
            while bus.recv(0) is not None:      # drop stale frames
                pass
            if not self._send(bus, payload):
                return None
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


def teensy_passive(ports, mode=b"P2"):
    """Set the Teensy channels to listen-only (+auto bit rate) before python-can opens them."""
    import serial
    for port in ports:
        if port:
            s = serial.Serial(port, 2000000, timeout=0.3)
            s.write(b"C\r" + mode + b"\r")
            time.sleep(0.05)
            s.read(16)
            s.close()


def cmd_log(b, args):
    names = {1: "can0", 2: "can1"}      # simulator replay: can0 = bus 1, can1 = bus 2
    n_frames = 0
    if args.iface == "slcan" and not args.active:
        b.shutdown()
        if args.rate == "666k":
            # fixed 666.667 kbit/s, listen-only: the user boot MAT's plant session (spec 17)
            teensy_passive([args.bus1, args.bus2], b"P1")
            teensy_passive([args.bus1, args.bus2], b"S9")
            args.keep_rate = True
            print("listen-only, fixed 666.667 kbit/s")
        else:
            teensy_passive([args.bus1, args.bus2])
            print("listen-only, automatic bit rate")
        args.teensy_mode = None             # keep listen-only when reopening
        b.__init__(args)
    with open(args.file, "w", buffering=1) as f:          # line-buffered: live progress
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
                    if m.arbitration_id == 0x1FFFFF00:
                        f.flush()
                        print(f"bus {n}: bit rate now {int.from_bytes(m.data[:2], 'big')} kbit/s")
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
    t0 = time.time()
    data = read_mem(b, args.bus, addr, length)
    if length <= 256:
        dump(addr, data)
    print(f"{length} bytes in {time.time() - t0:.1f} s")
    if args.output:
        open(args.output, "wb").write(data)
        print(f"saved {args.output}")
    if args.compare:
        ref = open(args.compare, "rb").read()[addr:addr + length]
        diff = [i for i in range(min(len(ref), len(data))) if ref[i] != data[i]]
        if not diff and len(ref) == len(data):
            print(f"identical to {args.compare} at 0x{addr:06X}..0x{addr + length - 1:06X}")
        else:
            print(f"{len(diff)} differing bytes; first at 0x{addr + diff[0]:06X}" if diff else
                  f"length mismatch: image has {len(ref)} bytes there")


def cmd_dump_userboot(b, args):
    import zlib
    u = Uds(b, 1)
    r = u.extended()
    if not r or r[0] != 0x50:
        sys.exit(f"extended session failed: {hexs(r)}")
    stop_tp = u.start_tester_present()
    try:
        r = u.request(b"\x31\x01\xFE\x20", timeout=2.0)
        if not r or r[0] != 0x71 or len(r) < 15:
            sys.exit(f"routine FE20 refused: {hexs(r)} (project firmware only)")
        status, addr = r[4], int.from_bytes(r[5:9], "big")
        length, crc = int.from_bytes(r[9:11], "big") or 0x10000, int.from_bytes(r[11:15], "big")
        if status != 1:
            sys.exit("the firmware could not copy the user boot MAT")
        print(f"user boot MAT copied to RAM 0x{addr:08X}, {length} bytes, CRC-32 0x{crc:08X}; downloading")
        t0 = time.time()
        data = read_mem(b, args.bus, addr, length)
        got = zlib.crc32(data) & 0xFFFFFFFF
        print(f"downloaded in {time.time() - t0:.0f} s, CRC-32 0x{got:08X}:",
              "OK" if got == crc else "MISMATCH - do not use this file")
        name = args.output or f"userboot_mat_{time.strftime('%Y%m%d_%H%M%S')}.bin"
        open(name, "wb").write(data)
        print(f"saved {name}  (Bosch boot code: keep it private, never commit it)")
    finally:
        stop_tp.set()


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


# ----------------------------------------------------------------------------- project reflash (spec 16)

def enter_flasher(b):
    """Return a tcu_flash.Flasher talking to the RAM flasher on bus 1 (starting it if needed)."""
    import tcu_flash
    u = Uds(b, 1)
    u.cf_gap_s = 0.001
    fl = tcu_flash.Flasher(u)
    info = fl.ident()
    if info is None:
        r = u.extended()
        if not r or r[0] != 0x50:
            sys.exit(f"extended session failed: {hexs(r)}")
        r = u.request(b"\x31\x01\xFE\x30", timeout=1.0)
        if not r or r[0] != 0x71:
            sys.exit(f"routine FE30 refused: {hexs(r)} (project firmware only; engine off, no output "
                     "test running; 0x22 also means the FWE pin is low)")
        time.sleep(0.3)
        info = fl.ident()
        if info is None:
            sys.exit("the flasher does not answer after FE30")
    print(f"flasher v{info['version']}: FWE {'high' if info['fwe'] else 'LOW'}, FSTATR0 {info['fstatr0']:02X}, "
          f"FSTATR1 {info['fstatr1']:02X}, FASTAT {info['fastat']:02X}, last error {info['last_error']}")
    if not info["fwe"]:
        fl.reset()
        sys.exit("FWE is low: the flash cannot be programmed (spec 16 section 8 step 1)")
    return fl


def load_image(path):
    import tcu_flash
    img = open(path, "rb").read()
    bad = tcu_flash.check_image(img)
    if bad:
        sys.exit("image refused:\n  " + "\n  ".join(bad))
    return img[:tcu_flash.FLASH_LEN]


def cmd_flash(b, args):
    import tcu_flash
    img = load_image(args.image)
    fl = enter_flasher(b)
    # The TCU is reset only when nothing was written or the write completed. After a failure the
    # flasher keeps running so the same command can retry; a reset there could leave the
    # application invalid (recoverable only with the ACDP-2).
    try:
        if fl.crc(0, tcu_flash.WIN_LO) != tcu_flash.crc(img[:tcu_flash.WIN_LO]):
            print("note: the boot area 0x000000-0x03FFFF differs from the image (it is never written)")
        todo = tcu_flash.plan(fl, img)
        if not todo:
            print("the TCU already holds this image: nothing to write")
            fl.reset()
            return
        for name, blks in todo.items():
            print(f"{name}: {len(blks)} block(s) to rewrite: " + " ".join(f"{x:#08x}" for x in blks))
        if args.dry_run or (not args.yes and input(
                "write now? Do not remove power until it says done. [yes/no] ").strip() != "yes"):
            print("nothing written")
            fl.reset()
            return
        t0 = time.time()
        pages = tcu_flash.write(fl, img, todo, progress=print)
    except tcu_flash.FlashError as e:
        sys.exit(f"FLASH FAILED: {e}\nThe flasher is still running: rerun the same command to retry. "
                 "If the TCU was reset or lost power, recover with the ACDP-2.")
    print(f"done: {pages} pages in {time.time() - t0:.0f} s; resetting into the new image")
    fl.reset()


def cmd_flash_selftest(b, args):
    """Spec 16 section 8 step 3: erase / program / verify / erase one block the image leaves blank."""
    import tcu_flash
    img = load_image(args.image)
    blk = int(args.block, 0)
    n = tcu_flash.block_size(blk)
    if blk != tcu_flash.block_of(blk) or not tcu_flash.WIN_LO <= blk < tcu_flash.WIN_HI:
        sys.exit("block must be the start of an erase block inside 0x040000-0x1FFFFF")
    if any(blk <= r["marker1"] < blk + n for r in tcu_flash.REGIONS):
        sys.exit("that block holds a validity marker; choose another one")
    if img[blk:blk + n] != b"\xFF" * n:
        sys.exit("the image is not blank in that block; choose a block the image does not use")
    fl = enter_flasher(b)
    try:
        if fl.crc(blk, n) != tcu_flash.crc(b"\xFF" * n):
            sys.exit("that block is not blank on the TCU either; it is in use, choose another one")
        pattern = bytes((i * 7 + 1) & 0xFF for i in range(16 * tcu_flash.PAGE))
        print(f"erase {blk:#08x}"); fl.erase(blk, n)
        print(f"program 16 pages at {blk:#08x}"); fl.program(blk, pattern)
        ok = fl.crc(blk, len(pattern)) == tcu_flash.crc(pattern)
        print("read back:", "OK" if ok else "MISMATCH")
        print(f"erase {blk:#08x} again"); fl.erase(blk, n)
        blank = fl.crc(blk, n) == tcu_flash.crc(b"\xFF" * n)
        print("blank afterwards:", "OK" if blank else "NO")
        print("self-test", "PASSED" if ok and blank else "FAILED")
    except tcu_flash.FlashError as e:
        sys.exit(f"self-test FAILED: {e}")
    finally:
        fl.reset()


def cmd_flash_invalidate(b, args):
    """Spec 16 section 8 step 4: make the application invalid on purpose (safety-net check)."""
    import tcu_flash
    print("This erases block 0x160000 (application markers): the TCU will NOT start the application")
    print("until it is rewritten, and only the ACDP-2 can do that. Use only for the safety-net check.")
    if input("type INVALIDATE to continue: ").strip() != "INVALIDATE":
        return
    fl = enter_flasher(b)
    try:
        fl.erase(0x160000, tcu_flash.block_size(0x160000))
        print("application invalidated. Power-cycle the TCU, check it stays silent, then write the")
        print("project image with the ACDP-2 and check the heartbeat 0x7F0 comes back.")
    except tcu_flash.FlashError as e:
        sys.exit(f"FAILED: {e}")
    finally:
        fl.reset()


# ----------------------------------------------------------------------------- user boot MAT plant entry (spec 17)

def cmd_plant_entry(b, args):
    """Send the user boot MAT's plant entry frame at 666.667 kbit/s while the TCU is switched on,
    and log every frame received (bus 1 only, Teensy). Read-only: the TCU stays in the boot MAT
    session until it is power-cycled."""
    import serial
    b.shutdown()
    payload = b"PlantCan" if args.mode == "can" else b"PlantKcn"
    frame = b"T1F0C3CFE8" + payload.hex().upper().encode() + b"\r"
    s = serial.Serial(args.bus1, 2000000, timeout=0)
    for c in (b"C\r", b"P0\r", b"S6\r", b"O\r", b"C\r", b"S9\r", b"O\r"):   # O/C clears listen-only
        s.write(c); time.sleep(0.05)
    s.reset_input_buffer()
    args._out_default = "plant_entry"
    name = args.output or f"plant_entry_{time.strftime('%Y%m%d_%H%M%S')}.log"
    out = open(name, "w", buffering=1)
    print(f"sending {payload.decode()} on 0x1F0C3CFE at 666.667 kbit/s every {args.period_ms} ms;")
    print(f"switch the TCU supply ON now (sending for {args.send_s} s, then listening). Log: {name}")
    t0 = time.time(); buf = b""; seen = 0; sent = 0; stop_send = t0 + args.send_s
    end = stop_send + args.listen_s
    next_tx = t0
    while time.time() < end:
        now = time.time()
        if now < stop_send and now >= next_tx:
            s.write(frame); sent += 1; next_tx = now + args.period_ms / 1000.0
        buf += s.read(4096)
        while b"\r" in buf:
            line, buf = buf.split(b"\r", 1)
            line = line.decode(errors="replace")
            if not line or line[0] not in "tT":
                continue
            ts = time.time() - t0
            if line[0] == "T":
                ident, dlc, data = line[1:9], int(line[9]), line[10:10 + 2 * int(line[9])]
            else:
                ident, dlc, data = line[1:4], int(line[4]), line[5:5 + 2 * int(line[4])]
            out.write(f"{ts:9.4f} {ident}#{data}\n")
            seen += 1
            if seen <= 40:
                print(f"  {ts:8.3f} s  {ident} [{dlc}] {data}")
            if ident.upper() == "1FFFAFFE" and now < stop_send:
                stop_send = now                     # boot MAT answered: stop sending the entry
                end = now + args.listen_s
        time.sleep(0.0005)
    if args.probe and seen:
        plant_probe(s, out, t0)
    for c in (b"C\r", b"S6\r"):
        s.write(c); time.sleep(0.05)
    s.close()
    print(f"sent {sent} entry frames, received {seen} frames. Power-cycle the TCU to leave the boot MAT.")


def plant_probe(s, out, t0):
    """Harmless requests (ISO-TP single frames) to learn the session's addressing and bit rate."""
    def collect(t):
        end = time.time() + t; buf = b""; got = []
        while time.time() < end:
            buf += s.read(4096)
            while b"\r" in buf:
                line, buf = buf.split(b"\r", 1)
                if line[:1] in (b"t", b"T"):
                    got.append(line.decode())
                    out.write(f"{time.time() - t0:9.4f} {line.decode()}\n")
            time.sleep(0.002)
        return got
    reqs = ["023E00", "022701", "021001", "0322F190", "0322F195"]
    for rate, code in (("666.667k", b"S9"), ("500k", b"S6")):
        for c in (b"C\r", code + b"\r", b"O\r"):
            s.write(c); time.sleep(0.05)
        s.reset_input_buffer()
        for ident, ext in (("7E1", False), ("1F0C3CFE", True)):
            for r in reqs:
                data = (r + "AA" * 8)[:16]
                frame = ("T" + ident + "8" if ext else "t" + ident + "8") + data + "\r"
                out.write(f"{time.time() - t0:9.4f} TX {rate} {ident}#{data}\n")
                s.write(frame.encode())
                got = collect(0.3)
                print(f"  {rate:9s} {ident:8s} {r:10s} -> {' '.join(got) if got else '-'}")


def plant_buses(b, args):
    """Reopen bus 1 at 666.667 kbit/s in normal mode (TCU already in the boot MAT session)."""
    b.shutdown()
    teensy_passive([args.bus1], b"P0")
    teensy_passive([args.bus1], b"S9")
    args.keep_rate = True
    args.teensy_mode = None
    args.bus2 = ""
    b.__init__(args)


def cmd_plant_seedscan(b, args):
    """In the boot MAT plant session, sweep SecurityAccess (0x27) seed sub-functions across the
    reachable sessions, to find which level returns a seed (0x67) and how long it is. Read-only.
    The TCU must already be in the session (run `plant-entry` during power-up first, or pass
    --enter here to send the entry while you power the TCU on)."""
    import time
    import serial
    if args.enter:
        b.shutdown()
        payload = b"PlantCan" if args.mode == "can" else b"PlantKcn"
        frame = b"T1F0C3CFE8" + payload.hex().upper().encode() + b"\r"
        raw = serial.Serial(args.bus1, 2000000, timeout=0)
        for c in (b"C\r", b"P0\r", b"S6\r", b"O\r", b"C\r", b"S9\r", b"O\r"):
            raw.write(c); time.sleep(0.05)
        raw.reset_input_buffer()
        print("power the TCU ON now; sending", payload.decode(), "for up to", args.send_s, "s")
        t_end = time.time() + args.send_s; nxt = 0.0; got = b""
        while time.time() < t_end:
            now = time.time()
            if now >= nxt:
                raw.write(frame); nxt = now + 0.002
            got += raw.read(256)
            if b"1FFFAFFE" in got.upper():
                print("boot MAT answered the entry"); break
            time.sleep(0.0005)
        for c in (b"C\r",):
            raw.write(c); time.sleep(0.05)
        raw.close()
    plant_buses(b, args)                       # bus 1 at 666.667 kbit/s, normal mode
    u = Uds(b, 1)
    for ses in [None] + [int(x, 16) for x in args.sessions.split(",") if x.strip()]:
        if ses is not None:
            r = u.request(bytes([0x10, ses]), timeout=0.5)
            tag = f"session {ses:02X}: {hexs(r)}"
            if not r or r[0] != 0x50:
                print(tag, "(not entered, skipping)"); continue
            print(tag)
        else:
            print("default session")
        for sub in range(1, 0x80, 2):          # odd = request-seed levels
            r = u.request(bytes([0x27, sub]), timeout=0.3)
            if r and r[0] == 0x67:
                print(f"  27 {sub:02X} -> SEED {hexs(r)}")
            elif r and not (r[0] == 0x7F and len(r) > 2 and r[2] in (0x12, 0x7F)):
                print(f"  27 {sub:02X} -> {hexs(r)}")

def cmd_plant_uds(b, args):
    plant_buses(b, args)
    u = Uds(b, 1)
    for req in " ".join(args.hex).split(","):
        req = bytes.fromhex(req.replace(" ", ""))
        r = u.request(req, timeout=args.timeout)
        print(f"{req.hex(' ').upper():24s} -> {hexs(r)}")


# ----------------------------------------------------------------------------- data-flash loader (spec 18)

def enter_loader(b, args):
    """Return a tcu_flash.Flasher talking to the data-flash loader on bus 1. Tries, in order: the
    loader already running; FE30 from a running RAM application; the loader's stay request
    (31 01 FE 40) sent while the TCU is switched on."""
    import serial
    import tcu_flash
    u = Uds(b, 1)
    u.cf_gap_s = 0.001
    fl = tcu_flash.Flasher(u)
    info = fl.ident(kinds=("LD",))
    if info is None and u.request(b"\x3E\x00", timeout=0.5):
        r = u.extended()
        if r and r[0] == 0x50:
            r = u.request(b"\x31\x01\xFE\x30", timeout=1.0)
            if r and r[0] == 0x71:
                time.sleep(0.3)
                info = fl.ident(kinds=("LD",))
    if info is None:
        b.shutdown()
        raw = serial.Serial(args.bus1, 2000000, timeout=0)
        for c in (b"C\r", b"P0\r", b"S6\r", b"O\r"):
            raw.write(c); time.sleep(0.05)
        raw.reset_input_buffer()
        print(f"switch the TCU supply ON now (sending the loader stay request for up to {args.send_s:.0f} s)")
        end = time.time() + args.send_s; got = b""; nxt = 0.0; seen = False
        while time.time() < end and not seen:
            now = time.time()
            if now >= nxt:
                raw.write(b"t7E18043101FE40AAAAAA\r"); nxt = now + 0.005
            got += raw.read(512)
            seen = b"t7E98047101FE40" in got.upper()
            time.sleep(0.0005)
        raw.write(b"C\r"); time.sleep(0.05); raw.close()
        if not seen:
            sys.exit("the loader did not answer the stay request (is the loader installed?)")
        b.__init__(args)
        u = Uds(b, 1)
        u.cf_gap_s = 0.001
        fl = tcu_flash.Flasher(u)
        time.sleep(0.1)
        for _ in range(3):
            info = fl.ident(kinds=("LD",))
            if info:
                break
    if info is None:
        sys.exit("no answer from the data-flash loader")
    entry = ("" if "fwe_entry" not in info
             else f", FWE at boot hand-over {'HIGH' if info['fwe_entry'] else 'low'} "
                  f"(FPMON {info['fpmon_entry']:02X})")
    print(f"loader v{info['version']}: FWE {'high' if info['fwe'] else 'low'} (not needed), "
          f"FSTATR0 {info['fstatr0']:02X}, last error {info['last_error']}{entry}")
    return fl


def cmd_app_flash(b, args):
    import tcu_flash
    img = open(args.image, "rb").read()
    bad = tcu_flash.df_image_check(img)
    if bad:
        sys.exit("image refused:\n  " + "\n  ".join(bad))
    ver = img[0x18:0x28].rstrip(b"\0").decode(errors="replace")
    print(f"image {args.image}: {len(img) - tcu_flash.DF_HDR} payload bytes, version '{ver}'")
    fl = enter_loader(b, args)
    if getattr(args, "slow", False):
        fl.page_delay_s = 0.2               # power-cut test: payload phase takes about 35 s
    try:
        cur = fl.crc(tcu_flash.DF_BASE, len(img))
        if cur == tcu_flash.crc(img):
            print("the data flash already holds this image")
        else:
            if not args.yes and input("write now? [yes/no] ").strip() != "yes":
                print("nothing written"); fl.reset(); return
            t0 = time.time()
            tcu_flash.df_write(fl, img, progress=print)
            print(f"done in {time.time() - t0:.1f} s")
    except tcu_flash.FlashError as e:
        sys.exit(f"APP-FLASH FAILED: {e}\nThe loader keeps running: rerun the same command. A power "
                 "loss leaves no valid image; the loader then waits in loader mode (spec 18).")
    print("resetting: the loader starts the application from data flash")
    fl.reset()


def cmd_app_selftest(b, args):
    """Spec 18 section 7 step 4: erase / program / read back / erase data-flash block DB07."""
    import tcu_flash
    fl = enter_loader(b, args)
    a = tcu_flash.DF_BASE + 7 * tcu_flash.DF_BLOCK
    pattern = bytes((i * 7 + 3) & 0xFF for i in range(1024))
    try:
        print("erase DB07"); fl.erase(a, tcu_flash.DF_BLOCK)
        print("program 1 KB"); fl.program(a, pattern)
        ok = fl.crc(a, len(pattern)) == tcu_flash.crc(pattern)
        print("read back:", "OK" if ok else "MISMATCH")
        print("erase DB07 again"); fl.erase(a, tcu_flash.DF_BLOCK)
        print("self-test", "PASSED" if ok else "FAILED")
    except tcu_flash.FlashError as e:
        sys.exit(f"self-test FAILED: {e}")
    print("the loader stays in loader mode; `uds 11 01` or a power cycle leaves it")


def meas_table_addr(elf, min_len):
    """RAM address of the project measurement table _s_m (app/meas_table.c) from the ELF."""
    import subprocess
    out = subprocess.run(["sh-elf-nm", "-S", elf], capture_output=True, text=True, check=True).stdout
    sym = [l.split() for l in out.splitlines() if l.split()[-1:] == ["_s_m"]]
    if not sym or int(sym[0][1], 16) < min_len:
        sys.exit(f"measurement table _s_m (at least {min_len} bytes) not found in {elf} - rebuild?")
    return int(sym[0][0], 16)


def cmd_sensors(b, args):
    """Live sensor values of the PROJECT firmware (bench items 22/23): reads the measurement table
    (app/meas_table.c, 48 bytes) from RAM with the measurement protocol every --period seconds."""
    addr = meas_table_addr(args.elf, 48)
    u16 = lambda d, o: int.from_bytes(d[o:o + 2], "big")
    s16 = lambda d, o: int.from_bytes(d[o:o + 2], "big", signed=True)
    park_names = {0: "unknown", 1: "in P", 2: "not in P"}
    print(f"measurement table at 0x{addr:08X} ({args.elf}); Ctrl-C to stop")
    print("  uptime  battery   fluid temp  AN8 mV  AN1 mV  ratio   faults  park (bands)       speed0/1/2 rpm")
    n = 0
    try:
        while args.count == 0 or n < args.count:
            d = read_mem(b, args.bus, addr, 48)
            an8, an1 = u16(d, 44), u16(d, 46)
            ratio = (an8 - an1) / an1 if an1 else float("nan")
            print(f"  {u16(d, 24):5d} s  {u16(d, 40) / 1000:6.2f} V  {s16(d, 36) / 10:7.1f} C  {an8:6d}  {an1:6d}  "
                  f"{ratio:6.3f}  0x{d[38]:02X}    {park_names.get(d[42], d[42]):9s} (0x{d[43]:02X})  "
                  f"{u16(d, 30)}/{u16(d, 32)}/{u16(d, 34)}")
            n += 1
            time.sleep(args.period)
    except KeyboardInterrupt:
        pass


PARK_STATES = ["init", "in P", "releasing", "holding", "engaging", "SAFE"]
PARK_REASONS = ["-", "lever P", "moving", "park request", "lever R/N/D", "auto P"]


def cmd_park(b, args):
    """Range / park control of the PROJECT firmware (M4a, docs/firmware/01-m4-range-park.md):
    lever, park target and reason, sequencing state, the requested (not driven) outputs and the
    fault bits, from the measurement table every --period seconds."""
    addr = meas_table_addr(args.elf, 54)
    u16 = lambda d, o: int.from_bytes(d[o:o + 2], "big")
    levers = {0: "-", 5: "D", 6: "N", 7: "R", 8: "P"}
    srcs = {0: "CAN-C", 1: "shifter bus", 2: "none"}
    park_names = {0: "unknown", 1: "in P", 2: "not in P"}
    print(f"measurement table at 0x{addr:08X} ({args.elf}); outputs are requests only (M4a); Ctrl-C to stop")
    print("  uptime  engine  lever (source)      contacts   target   reason        state      release hold  faults ctl/mon")
    n = 0
    try:
        while args.count == 0 or n < args.count:
            d = read_mem(b, args.bus, addr, 54)
            tgt = {8: "engage", 6: "release"}.get(d[48], f"?{d[48]}")
            rsn = PARK_REASONS[d[49]] if d[49] < len(PARK_REASONS) else f"?{d[49]}"
            st = PARK_STATES[d[50]] if d[50] < len(PARK_STATES) else f"?{d[50]}"
            lev = f"{levers.get(d[19], d[19])} ({srcs.get(d[20], d[20])})"
            print(f"  {u16(d, 24):5d} s  {u16(d, 0):5d}   {lev:18s}  {park_names.get(d[42], d[42]):9s}  {tgt:7s}  "
                  f"{rsn:12s}  {st:9s}  {'on' if d[51] & 1 else 'off':7s} {'on' if d[51] & 2 else 'off':4s}  "
                  f"0x{d[52]:02X}/0x{d[53]:02X}")
            n += 1
            time.sleep(args.period)
    except KeyboardInterrupt:
        pass


def crc8_j1850(data):
    """SAE J1850 CRC-8 (spec 07 section 2): poly 0x1D, init 0xFF, final XOR 0xFF."""
    c = 0xFF
    for b in data:
        c ^= b
        for _ in range(8):
            c = ((c << 1) ^ 0x1D) & 0xFF if c & 0x80 else (c << 1) & 0xFF
    return c ^ 0xFF


def lever_frames(code, ctr, bad_crc=False):
    """(0x2EA data, 0x210 data) for lever code 1 P, 2 R, 3 N, 4 D (spec 14 section 5a): 0x2EA
    with counter (byte 3 upper nibble) and CRC-8 (byte 4, variant 1); 0x210 unprotected."""
    d = bytearray([code & 0x07, 0, 0, (ctr & 0x0F) << 4, 0])
    d[4] = crc8_j1850(d[:4]) ^ (0x55 if bad_crc else 0)
    return bytes(d), bytes([code & 0x07, 0, 0, 0, 0])


class LeverSender(threading.Thread):
    """Sends the shifter messages every 10 ms: 0x2EA on bus 1 and 0x210 on bus 2."""
    def __init__(self, b):
        super().__init__(daemon=True)
        self.b = b
        self.code, self.on_2ea, self.on_210, self.bad_crc = 1, False, False, False
        self.stop = threading.Event()

    def run(self):
        ctr, t = 0, time.monotonic()
        while not self.stop.is_set():
            ctr = (ctr + 1) & 0x0F
            d2ea, d210 = lever_frames(self.code, ctr, self.bad_crc)
            try:
                if self.on_2ea:
                    self.b.send(1, 0x2EA, d2ea)
                if self.on_210:
                    self.b.send(2, 0x210, d210)
            except Exception as e:                     # keep going; the step check will show it
                print("  send error:", e)
            t += 0.010
            time.sleep(max(0.0, t - time.monotonic()))


def cmd_park_check(b, args):
    """Bench check of M4a (docs/firmware/01-m4-range-park.md): sends lever frames, reads the
    lever decode and the park target from the measurement table and compares them with the
    expected values. Module alone: no park contacts, so the sequencing ends in SAFE (outputs
    are requests only and are never driven in M4a)."""
    addr = meas_table_addr(args.elf, 54)
    P, R, N, D = 1, 2, 3, 4
    # (description, code, 0x2EA on, 0x210 on, bad CRC, lever expected or None, source expected,
    #  kind: "P" engage by lever, "RND" lever R/N/D, "none" no valid lever)
    steps = [
        ("no shifter frames",              P, False, False, False, None, 2, "none"),
        ("P on both buses",                P, True,  True,  False, 8,    0, "P"),
        ("R on both buses",                R, True,  True,  False, 7,    0, "RND"),
        ("N on both buses",                N, True,  True,  False, 6,    0, "RND"),
        ("D on both buses",                D, True,  True,  False, 5,    0, "RND"),
        ("code 0 (no change) after D",     0, True,  True,  False, 5,    0, "RND"),
        ("0x2EA bad CRC, 0x210 R",         R, True,  True,  True,  7,    1, "RND"),
        ("0x2EA missing, 0x210 N",         N, False, True,  False, 6,    1, "RND"),
        ("0x2EA D, 0x210 missing",         D, True,  False, False, 5,    0, "RND"),
        ("invalid code 7 on both",         7, True,  True,  False, None, 2, "none"),
        ("both missing again",             D, False, False, False, None, 2, "none"),
        ("P on both buses",                P, True,  True,  False, 8,    0, "P"),
    ]
    snd = LeverSender(b)
    snd.start()
    results, still = [], None
    srcs = {0: "CAN-C", 1: "shifter bus", 2: "none"}
    print(f"measurement table at 0x{addr:08X}; {args.settle:.1f} s per step")
    try:
        for desc, code, a, c, bad, lev, src, kind in steps:
            snd.code, snd.on_2ea, snd.on_210, snd.bad_crc = code, a, c, bad
            time.sleep(args.settle)
            d = read_mem(b, args.bus, addr, 54)
            if still is None:                          # first step: no lever -> Auto P or moving
                still = d[49] == 5
                print(f"  output speed {'valid and stationary' if still else 'not valid / moving'}"
                      f" (reason {d[49]})")
            want_t, want_r = {"P": (8, 1), "RND": (6, 4 if still else 2),
                              "none": (8, 5) if still else (6, 2)}[kind]
            ok = d[20] == src and (lev is None or d[19] == lev) and d[48] == want_t and d[49] == want_r
            results.append(ok)
            print(f"  {'PASS' if ok else 'FAIL'}  {desc:30s} lever {d[19]} ({srcs.get(d[20], d[20])})  "
                  f"target {d[48]} reason {PARK_REASONS[d[49]] if d[49] < len(PARK_REASONS) else d[49]}"
                  + ("" if ok else f"   expected lever {lev} src {src} target {want_t} reason {want_r}"))
    finally:
        snd.stop.set()
        snd.join(1.0)
    d = read_mem(b, args.bus, addr, 54)
    st = PARK_STATES[d[50]] if d[50] < len(PARK_STATES) else d[50]
    print(f"  sequencing: state {st}, requests release {'on' if d[51] & 1 else 'off'} / hold "
          f"{'on' if d[51] & 2 else 'off'}, faults ctl 0x{d[52]:02X} mon 0x{d[53]:02X} (module alone: SAFE "
          f"with ctl 0x02 engage timeout is expected, no contacts)")
    print(f"{sum(results)}/{len(results)} steps passed")
    return 0 if all(results) else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--iface", default="slcan", help="slcan (Teensy, default) or socketcan")
    ap.add_argument("--bus1", default=None, help="channel for TCU pins 5/6 (default /dev/ttyACM0, or can0)")
    ap.add_argument("--bus2", default=None, help="channel for TCU pins 3/4 ('' = none; default /dev/ttyACM1, or can1)")
    ap.add_argument("--uds-ext", action="store_true",
                    help="talk to images built before the MCR15 fix: UDS on 29-bit 0x07E00000/0x07E80000")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("monitor")
    p = sub.add_parser("log"); p.add_argument("file"); p.add_argument("--active", action="store_true")
    p.add_argument("--rate", choices=["auto", "666k"], default="auto",
                   help="Teensy only: 666k = fixed 666.667 kbit/s listen-only (plant session)")
    p = sub.add_parser("uds"); p.add_argument("--bus", type=int, default=1); p.add_argument("hex", nargs="+")
    p = sub.add_parser("info"); p.add_argument("--bus", type=int, default=1)
    p = sub.add_parser("meas-ident"); p.add_argument("--bus", type=int, default=2)
    p = sub.add_parser("meas-read"); p.add_argument("addr"); p.add_argument("len")
    p.add_argument("--bus", type=int, default=2); p.add_argument("-o", "--output")
    p.add_argument("--compare", help="full flash image to compare against")
    p = sub.add_parser("dump-userboot"); p.add_argument("--bus", type=int, default=2)
    p.add_argument("-o", "--output")
    p = sub.add_parser("item20"); p.add_argument("--bus", type=int, default=2)
    p = sub.add_parser("solenoid"); p.add_argument("--bus", type=int, default=1)
    p.add_argument("--csv", default="m3_solenoid_results.csv")
    p = sub.add_parser("canstat"); p.add_argument("--send", action="store_true")
    p = sub.add_parser("flash"); p.add_argument("image"); p.add_argument("--dry-run", action="store_true")
    p.add_argument("--yes", action="store_true")
    p = sub.add_parser("flash-selftest"); p.add_argument("image"); p.add_argument("--block", default="0x100000")
    sub.add_parser("flash-invalidate")
    p = sub.add_parser("sensors"); p.add_argument("--bus", type=int, default=2)
    p.add_argument("--elf", default="build/sh2a/tcu_app_ram.elf"); p.add_argument("--period", type=float, default=1.0)
    p.add_argument("--count", type=int, default=0)
    p = sub.add_parser("park"); p.add_argument("--bus", type=int, default=2)
    p.add_argument("--elf", default="build/sh2a/tcu_app_ram.elf"); p.add_argument("--period", type=float, default=1.0)
    p.add_argument("--count", type=int, default=0)
    p = sub.add_parser("park-check"); p.add_argument("--bus", type=int, default=2)
    p.add_argument("--elf", default="build/sh2a/tcu_app_ram.elf"); p.add_argument("--settle", type=float, default=1.0)
    p = sub.add_parser("app-flash"); p.add_argument("image", nargs="?", default="build/sh2a/tcu_app_df.bin")
    p.add_argument("--yes", action="store_true"); p.add_argument("--send-s", type=float, default=60.0)
    p.add_argument("--slow", action="store_true", help="power-cut test: stretch the write to about 35 s")
    p = sub.add_parser("app-selftest"); p.add_argument("--send-s", type=float, default=60.0)
    p = sub.add_parser("plant-uds"); p.add_argument("hex", nargs="+", help="requests, comma-separated")
    p.add_argument("--timeout", type=float, default=1.0)
    p = sub.add_parser("plant-seedscan"); p.add_argument("--enter", action="store_true")
    p.add_argument("--sessions", default="60"); p.add_argument("--send-s", type=float, default=60.0)
    p.add_argument("--mode", choices=["can", "kcn"], default="can")
    p = sub.add_parser("plant-entry"); p.add_argument("--mode", choices=["can", "kcn"], default="can")
    p.add_argument("--period-ms", type=float, default=2.0); p.add_argument("--send-s", type=float, default=20.0)
    p.add_argument("--listen-s", type=float, default=5.0); p.add_argument("-o", "--output")
    p.add_argument("--probe", action="store_true", help="after the entry reply, send harmless requests")
    args = ap.parse_args()
    teensy = args.iface == "slcan"
    if args.bus1 is None:
        args.bus1 = "/dev/ttyACM0" if teensy else "can0"
    if args.bus2 is None:
        args.bus2 = "/dev/ttyACM1" if teensy else "can1"
    if args.uds_ext:
        Uds.REQ, Uds.RSP = 0x07E00000, 0x07E80000

    b = Buses(args)
    try:
        {"monitor": cmd_monitor, "log": cmd_log, "uds": cmd_uds, "info": cmd_info,
         "meas-ident": cmd_meas_ident, "meas-read": cmd_meas_read, "item20": cmd_item20,
         "solenoid": cmd_solenoid, "canstat": cmd_canstat,
         "dump-userboot": cmd_dump_userboot, "flash": cmd_flash,
         "flash-selftest": cmd_flash_selftest, "flash-invalidate": cmd_flash_invalidate,
         "plant-entry": cmd_plant_entry, "plant-uds": cmd_plant_uds,
         "plant-seedscan": cmd_plant_seedscan, "app-flash": cmd_app_flash,
         "app-selftest": cmd_app_selftest, "sensors": cmd_sensors, "park": cmd_park, "park-check": cmd_park_check}[args.cmd](b, args)
    finally:
        b.shutdown()


if __name__ == "__main__":
    main()
