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
                          section 8 step 4); restore afterwards with the Teensy flasher
                          (8hp-teensy-flasher, spec 17 path)
  plant-entry [--mode can|kcn]   send the user boot MAT plant entry (29-bit 0x1F0C3CFE,
                          "PlantCan") at 666.667 kbit/s while the TCU is switched on; log replies
                          (spec 17). Read-only; power-cycle the TCU afterwards.
  plant-probe34 [--session 60]  probe RequestDownload (34) formats against a RAM window of the
                          plant session; read-only, no 36 is sent (spec 17 follow-up)
  plant-dump-session      download the dumper (firmware/plantdump) through 34/36/37 and read the
                          session RAM out over CAN 0x7EA: the service nodes and handlers incl.
                          the 27 seed/key transform (spec 17 follow-up). RAM only, no flash writes
  park-check [--hw] [--sim-brake] [--sim-engine RPM] [--start-before-power] [--settle S]
                          PROJECT firmware: the shifter sequence against the STOCK range chain
                          (live since 2026-11-17): without the sims every step keeps P; with
                          FAKE 0x140 brake + 0x108 engine (DRY BENCH ONLY) R/N/D release
  range-check [--sim-brake] [--sim-engine RPM] [--start-before-power] [--settle S]
                          (alias shadow-check) PROJECT firmware: lever P R N D P, the live range
                          chain's cells (at the stock addresses) and the park path per step
  gs-check [--sim-brake] [--sim-engine RPM] [--start-before-power] [--hold S]
                          PROJECT firmware: the READ-ONLY garage-engagement shadow (app/gs_shadow.c)
                          - lever P, D, P, R, P; prints the shadow's witnesses and its
                          per-pass engagement recorder after the P->D and P->R commits
  keyoff-check [--off-code C] [--sim-brake] [--sim-engine RPM] [--settle S]
                          PROJECT firmware: KEY-OFF TEST - sends 0x112 (ignition status) 4, then
                          the OFF code C (the only command that may send a code other than 4/5)
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

    def start_drain(self):
        """Read every port continuously from background threads (2026-11-17). If the host stops
        reading a Teensy port, the Teensy's USB serial write blocks until its transmit timeout
        (~70 ms [I: Teensyduino usb_serial TX_TIMEOUT_MSEC]) and its loop stops sending OUR frames
        on both CAN channels - the lever-sim gaps of the r03r shadow-check (the TCU counted
        counter errors on bus 1 and 0x210 timeouts at each moment the script stopped reading).
        After this call get(n).recv() reads from the drained queue; send() is unchanged."""
        import queue
        for n, real in list(self.bus.items()):
            if isinstance(real, _DrainedBus):
                continue
            self.bus[n] = _DrainedBus(real, queue)

    def drain_stats(self):
        return {n: (b.dropped, b.got) for n, b in self.bus.items() if isinstance(b, _DrainedBus)}

    def shutdown(self):
        for b in self.bus.values():
            try:
                b.shutdown()
            except Exception:
                pass                      # already closed (canstat uses the raw ports)
        self.bus = {}


class _DrainedBus:
    """A python-can bus read by a background thread into a bounded queue (see Buses.start_drain)."""
    def __init__(self, real, queue_mod):
        self.real = real
        self.q = queue_mod.Queue(maxsize=20000)
        self._empty = queue_mod.Empty
        self.dropped = self.got = 0
        self.stop = threading.Event()
        self.t = threading.Thread(target=self._run, daemon=True)
        self.t.start()

    def _run(self):
        while not self.stop.is_set():
            try:
                m = self.real.recv(0.01)
            except Exception:
                if self.stop.is_set():
                    return
                continue
            if m is None:
                continue
            self.got += 1
            try:
                self.q.put_nowait(m)
            except Exception:                      # full: drop the oldest, keep the newest
                try:
                    self.q.get_nowait(); self.dropped += 1
                    self.q.put_nowait(m)
                except Exception:
                    pass

    def recv(self, timeout=None):
        try:
            if timeout == 0:
                return self.q.get_nowait()
            return self.q.get(timeout=timeout)
        except self._empty:
            return None

    def send(self, msg, timeout=None):
        return self.real.send(msg)

    def shutdown(self):
        self.stop.set()
        self.t.join(0.5)
        self.real.shutdown()


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
    # application invalid (recoverable with the Teensy flasher, spec 17 path).
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
                 "If the TCU was reset or lost power, recover with the Teensy flasher (spec 17 path).")
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
    print("until it is rewritten (Teensy flasher, spec 17 path). Use only for the safety-net check.")
    if input("type INVALIDATE to continue: ").strip() != "INVALIDATE":
        return
    fl = enter_flasher(b)
    try:
        fl.erase(0x160000, tcu_flash.block_size(0x160000))
        print("application invalidated. Power-cycle the TCU, check it stays silent, then write the")
        print("project image with the Teensy flasher and check the heartbeat 0x7F0 comes back.")
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


# ----------------------------------------------------------------------------- plant session code download (spec 17 follow-up)

PD_WINDOW = 0xFFF88400        # download window whose slot at +0x10 the session's run hook reads
PD_DUMP_ID = 0x7EA            # dumper output (UDS replies stay on 0x7E9)
# region table of firmware/plantdump/pd.c (order matters: the dumper numbers regions by it)
PD_REGIONS = [
    (0xFFF83600, 0x0E00),     # session block: conn table, service nodes, handlers
    (0xFFF82000, 0x2000),     # second session window (install-descriptor target)
    (0xFFF84900, 0x0200),     # session descriptor globals (incl. the 0xFFF84A6C buffer)
    (0xFFF88400, 0x0040),     # downloaded image itself: endianness / integrity check
]

PLANT_NRC = {0x11: "serviceNotSupported", 0x12: "subFunctionNotSupported", 0x13: "incorrectLength",
             0x22: "conditionsNotCorrect", 0x24: "requestSequenceError", 0x31: "requestOutOfRange",
             0x33: "securityAccessDenied", 0x36: "exceedNumAttempts", 0x37: "requiredTimeDelay",
             0x70: "uploadDownloadNotAccepted", 0x71: "transferDataSuspended",
             0x72: "generalProgrammingFailure", 0x78: "responsePending"}


def plant_verdict(r):
    if r is None:
        return "no response"
    if r[0] == 0x7F and len(r) >= 3:
        return f"NRC {r[2]:02X} ({PLANT_NRC.get(r[2], '?')})"
    if r[0] == 0x74:
        bs = int.from_bytes(r[1:3], "big") if len(r) >= 3 else None
        return f"POSITIVE maxBlockLength {bs} ({hexs(r)})"
    return hexs(r)


def cmd_plant_probe34(b, args):
    """Probe RequestDownload (34) request formats against a RAM window of the boot MAT plant
    session. Read-only: no 36 follows, nothing is written. RAM addresses only — the memory map
    also lists code flash, and a 36 against it would write flash."""
    plant_buses(b, args)
    u = Uds(b, 1)
    u.cf_gap_s = 0.001
    if args.session:
        r = u.request(bytes([0x10, args.session]), timeout=0.5)
        print(f"10 {args.session:02X} -> {hexs(r)}")
    addr, ln = args.addr, args.length
    fmts = [
        ("44 addr4 len4  (project flasher)", bytes([0x34, 0x00, 0x44]) + addr.to_bytes(4, "big") + ln.to_bytes(4, "big")),
        ("33 addr3 len3  (classic UDS)", bytes([0x34, 0x00, 0x33]) + (addr & 0xFFFFFF).to_bytes(3, "big") + ln.to_bytes(3, "big")),
        ("44 addr4 len2", bytes([0x34, 0x00, 0x44]) + addr.to_bytes(4, "big") + ln.to_bytes(2, "big")),
        ("bare 34 00", bytes([0x34, 0x00])),
    ]
    for name, req in fmts:
        r = u.request(req, timeout=1.0)
        print(f"{name:36s} -> {plant_verdict(r)}")
        if r and r[0] == 0x74:
            print("  (format accepted; stopping, no 36 sent)")
            break


def plant_download(u, image, watch_s):
    """34/36/37 an image into PD_WINDOW in the plant session; return the frames seen
    afterwards (the run hook may fire on 37)."""
    r = u.request(bytes([0x34, 0x00, 0x44]) + PD_WINDOW.to_bytes(4, "big") + len(image).to_bytes(4, "big"),
                  timeout=1.0)
    print(f"34 (window {PD_WINDOW:#x}, {len(image)} bytes) -> {plant_verdict(r)}")
    if not r or r[0] != 0x74:
        sys.exit("download not accepted (probe formats first: plant-probe34)")
    block = int.from_bytes(r[1:3], "big") or 0x100
    block = min(block - 1, 0x100)            # first byte of each 36 payload is the block counter
    seq = 1
    for off in range(0, len(image), block):
        chunk = image[off:off + block]
        rr = u.request(bytes([0x36, seq & 0xFF]) + chunk, timeout=1.0)
        if not rr or rr[0] != 0x76:
            sys.exit(f"36 block {seq} -> {plant_verdict(rr)}")
        seq = (seq + 1) & 0xFF
    r = u.request(b"\x37", timeout=1.0)
    print(f"36 x {seq - 1} blocks, 37 -> {plant_verdict(r)}")
    end = time.time() + watch_s
    frames = []
    while time.time() < end:
        m = u.b.get(1).recv(0.1)
        if m is None:
            continue
        frames.append((m.arbitration_id, bytes(m.data)))
        if bytes(m.data)[:3] in (b"PDE", b"PDA"):
            break
    return frames


def plant_reassemble(frames):
    """Rebuild the dumper's regions from 0x7EA frames; returns (buffers, problems)."""
    out = [bytearray() for _ in PD_REGIONS]
    problems, cur = [], None
    for aid, d in frames:
        if aid != PD_DUMP_ID or len(d) < 3:
            continue
        if d[:3] == b"PDR":
            idx, base = d[3], int.from_bytes(d[4:8], "big")
            if idx >= len(PD_REGIONS) or base != PD_REGIONS[idx][0]:
                problems.append(f"region header {idx} at {base:#x} unexpected")
                cur = None
            else:
                cur = idx
        elif d[:3] in (b"PDE", b"PDA", b"PDC"):
            cur = None
        elif cur is not None:
            seq = ((d[0] & 0x0F) << 8) | d[1]
            off = seq * 6
            buf = out[cur]
            if off != len(buf):
                problems.append(f"region {cur}: sequence jump {len(buf) // 6} -> {seq}")
                if off > len(buf):
                    buf += b"\x00" * (off - len(buf))
                else:
                    problems.append(f"region {cur}: data overwritten at offset {off}")
            out[cur][off:off + 6] = d[2:8]
    return out, problems


def cmd_plant_dump_session(b, args):
    """Download the dumper (firmware/plantdump) into the boot MAT plant session and read its RAM
    out: the session block (service nodes + handlers, incl. the 27 seed/key transform), the
    second window, the descriptor globals, and the image itself as an integrity check. The TCU
    must already be in the plant session (plant-entry during power-up). RAM only: no flash
    writes, no 27 62 key attempts."""
    import struct
    plant_buses(b, args)
    u = Uds(b, 1)
    u.cf_gap_s = 0.001
    r = u.request(bytes([0x10, args.session]), timeout=0.5)
    if not r or r[0] != 0x50:
        sys.exit(f"session {args.session:02X} not entered ({hexs(r)}); is the TCU in the plant session?")
    print(f"session {args.session:02X} entered")

    def swap32(img):
        return b"".join(struct.pack("<I", *struct.unpack(">I", img[i:i + 4])) for i in range(0, len(img) - 3, 4))

    canary = open(args.canary, "rb").read()
    if args.swap32:
        canary = swap32(canary)
    print(f"-- canary ({len(canary)} bytes): probe the download and run path")
    frames = plant_download(u, canary, args.watch_s)
    for aid, d in frames:
        print(f"  {aid:03X} #{d.hex(' ').upper()}")
    pdc = [d for aid, d in frames if d[:3] == b"PDC"]
    if not pdc:
        print("no canary frame: the run flag did not fire (or the image landed wrong). Nothing was")
        print("harmed; power-cycle and re-probe (plant-probe34, other trigger guesses).")
        return
    count, arg1 = pdc[-1][3], int.from_bytes(pdc[-1][4:8], "big")
    print(f"canary ran: invocation count {count}, hook arg1 {arg1:#010x}"
          f"{'' if arg1 == 0xFFF84A6C else '  (expected 0xFFF84A6C)'}")

    image = open(args.image, "rb").read()
    if args.swap32:
        image = swap32(image)
    print(f"-- dumper ({len(image)} bytes)")
    frames = plant_download(u, image, max(args.watch_s, 8.0))
    bufs, problems = plant_reassemble(frames)
    for p in problems:
        print("  problem:", p)
    ts = time.strftime("%Y%m%d_%H%M%S")
    for idx, ((base, ln), buf) in enumerate(zip(PD_REGIONS, bufs)):
        if len(buf) < ln:
            print(f"  region {idx} ({base:#x}): only {len(buf)} of {ln} bytes received")
        name = f"bench_captures/reads/session_r{idx}_{base:08X}_{ts}.bin"
        with open(name, "wb") as f:
            f.write(bytes(buf[:ln]))
        print(f"  region {idx} {base:#010x} +{ln:#06x} -> {name}")
    self_img = bytes(bufs[3][:0x40])
    if self_img[:16] == image[:16]:
        print("image self-echo: byte-exact (no endianness swap by the transfer)")
    elif self_img == swap32(image)[:len(self_img)]:
        print("image self-echo: WORD-SWAPPED by the transfer (rerun with --swap32)")
    else:
        print(f"image self-echo mismatch (got {self_img[:8].hex(' ')})")
    blk = bytes(bufs[0])
    if len(blk) >= 0x460:
        print("service nodes (SID at +1, handler-array pointer at +4, 0x18 stride):")
        off = 0x36F8 - 0x3600
        while off + 8 <= len(blk):
            sid = blk[off + 1]
            if sid == 0:
                break
            ptr = int.from_bytes(blk[off + 4:off + 8], "big")
            mark = "   <-- SecurityAccess" if sid == 0x27 else ""
            print(f"  SID {sid:02X}  handlers {ptr:#010x}{mark}")
            off += 0x18
    print("done. Power-cycle the TCU to leave the session.")


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


# the stock park module (~/tcu_sweep/range_logic_port.md): timer model 0xFFF8BC70[0] and the
# arbiter's reason codes (0xA557C / 0xA57DA)
PARK_STATES = {0: "boot", 1: "released", 2: "releasing", 3: "engaging", 4: "engaged"}
# 2026-11-17: the range is the stock CYCLIC CHAIN's D6D6 (app/rng_chain.c); the reason is its
# D6D9 (0xC7390 candidates; 0 = the lever request went through)
PARK_REASONS = {0x00: "-", 0x0A: "C7444", 0x14: "C747C P", 0x15: "C747C 6", 0x20: "C75FA",
                0x28: "C77FE", 0x33: "U1267 R (C767C)", 0x3C: "U1267 (C7756)", 0x50: "C7178 P",
                0x51: "C7178 6", 0x52: "C7226 P", 0x53: "C7226 6", 0x59: "C733A",
                0x5A: "C7630", 0x6E: "U D->6 (C76D8)", 0x6F: "U R->6 (C76D8)",
                0x78: "C77A0 P", 0x82: "C785C P", 0x83: "C785C 6", 0x8C: "C78BE"}


def park_line(d):
    """Decode the park block of the measurement table (offsets 48..53, app/meas_table.c)."""
    tgt = {8: "P", 7: "R", 6: "N", 5: "D"}.get(d[48], f"?{d[48]}")
    rsn = PARK_REASONS.get(d[49], f"0x{d[49]:02X}")
    st = PARK_STATES.get(d[50], f"?{d[50]}")
    return (f"range {tgt:2s} reason {rsn:16s} model {st:9s} release {'ON ' if d[51] & 1 else 'off'} "
            f"hold {'ON ' if d[51] & 2 else 'off'} req {d[52] & 1} lock {d[52] >> 1 & 1} mon 0x{d[53]:02X}")


def cmd_park(b, args):
    """Range / park logic of the PROJECT firmware (the stock arbiter + output law,
    ~/tcu_sweep/range_logic_port.md): lever, park target and stock reason, the timer model,
    the two park demands as the drivers pull them (DRV_GA_STAGE 1 drives them), the park
    request / lock and the cross-check faults, from the measurement table every --period s."""
    addr = meas_table_addr(args.elf, 54)
    u16 = lambda d, o: int.from_bytes(d[o:o + 2], "big")
    levers = {0: "-", 5: "D", 6: "N", 7: "R", 8: "P"}
    srcs = {0: "CAN-C", 1: "shifter bus", 2: "none"}
    print(f"measurement table at 0x{addr:08X} ({args.elf}); Ctrl-C to stop")
    n = 0
    try:
        while args.count == 0 or n < args.count:
            d = read_mem(b, args.bus, addr, 54)
            lev = f"{levers.get(d[19], d[19])} ({srcs.get(d[20], d[20])})"
            print(f"  {u16(d, 24):5d} s  out {u16(d, 32):5d} rpm  lever {lev:18s} {park_line(d)}")
            n += 1
            time.sleep(args.period)
    except KeyboardInterrupt:
        pass


def cmd_park_out(b, args):
    """M4b park output commissioning (docs/firmware/01-m4-range-park.md section 8, FE11):
    drives the park release valve (channel 7, full on) or the park hold magnet (TOE31 /
    PL6, duty in percent) one at a time, with the engine off, while the park module is
    suspended. Ammeter expected: park release and park hold coils are ~25 ohm (spec 12
    section 1.1), i.e. ~0.5 A at 13 V. DRY BENCH, SHORT BURSTS ONLY."""
    print("M4b PARK OUTPUT COMMISSIONING (FE11) - E-SHIFT MECHATRONIC, ENGINE OFF, SHORT BURSTS")
    print("out 0 = park release (ch 7, ~25 ohm), out 1 = park hold (TOE31/PL6 -> C7, ~25 ohm)")
    if input("Magnet(s) connected with an ammeter in circuit, transmission NOT connected? [yes/no] ").strip() != "yes":
        return
    u = Uds(b, args.bus)
    r = u.extended()
    if not r or r[0] != 0x50:
        sys.exit(f"extended session failed: {hexs(r)}")
    stop_tp = u.start_tester_present()
    names = ["park release (ch 7)", "park hold (TOE31)"]
    rows = []
    try:
        while True:
            line = input("out (0-1) seconds (1-5) [duty %% 1-100, default 100], or q: ").split()
            if not line or line[0] == "q":
                break
            out_, sec = int(line[0]), int(line[1])
            duty = int(line[2]) if len(line) > 2 else 0
            req = bytes([0x31, 0x01, 0xFE, 0x11, out_, sec]) + (bytes([duty]) if duty else b"")
            r = u.request(req)
            if not r or r[0] != 0x71:
                print("refused:", hexs(r))
                continue
            duty_s = f" at {duty} % duty" if duty and duty != 100 else ""
            print(f"{names[out_]}{duty_s} for {sec} s")
            meas = input("ammeter reading in mA (Enter to skip): ").strip()
            res = u.request(b"\x31\x03\xFE\x11")
            print("result:", hexs(res))
            rows.append({"time": time.strftime("%H:%M:%S"), "out": out_, "duty_pct": duty or 100,
                         "seconds": sec, "measured_mA": meas, "result": hexs(res)})
    finally:
        u.request(b"\x31\x02\xFE\x11")
        stop_tp.set()
    if rows and args.csv:
        with open(args.csv, "a", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0]))
            if f.tell() == 0:
                w.writeheader()
            w.writerows(rows)
        print(f"{len(rows)} rows appended to {args.csv}")


def crc8_j1850(data):
    """SAE J1850 CRC-8 (spec 07 section 2): poly 0x1D, init 0xFF, final XOR 0xFF."""
    c = 0xFF
    for b in data:
        c ^= b
        for _ in range(8):
            c = ((c << 1) ^ 0x1D) & 0xFF if c & 0x80 else (c << 1) & 0xFF
    return c ^ 0xFF


def lever_frames(code, ctr, bad_crc=False):
    """(0x2EA data, 0x210 data) for lever code 1 P, 2 R, 3 N, 4 D (spec 14 section 5a): both
    with counter (byte 3 upper nibble) and CRC-8 (byte 4, variant 1). 2026-11-17: the stock
    checks CRC + counter on 0x210 too (~/tcu_sweep/can_inputs_decode.md); positions as 0x2EA [I].
    bad_crc corrupts 0x2EA only (the park-check fallback step)."""
    d = bytearray([code & 0x07, 0, 0, (ctr & 0x0F) << 4, 0])
    e = bytearray(d)
    d[4] = crc8_j1850(d[:4]) ^ (0x55 if bad_crc else 0)
    e[4] = crc8_j1850(e[:4])
    return bytes(d), bytes(e)


class LeverSender(threading.Thread):
    """Sends the shifter messages every 10 ms: 0x2EA on bus 1 and 0x210 on bus 2; optionally
    (park-check --sim-brake / --sim-engine) the FAKE 0x140 brake pressed and 0x108 engine speed
    on bus 1, E2E-valid (e2e_frame)."""
    def __init__(self, b):
        super().__init__(daemon=True)
        self.b = b
        self.code, self.on_2ea, self.on_210, self.bad_crc = 1, False, False, False
        self.brake, self.rpm = False, 0
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
                if self.brake:
                    self.b.send(1, 0x140, e2e_frame([0x04, 0, 0, 0, 0, 0, 0, 0], ctr))
                if self.rpm:
                    r = self.rpm & 0x7FFF
                    self.b.send(1, 0x108, e2e_frame([r >> 8, r & 0xFF, 0x1F, 0xFF, 0, 0, 0, 0], ctr))
            except Exception as e:                     # keep going; the step check will show it
                print("  send error:", e)
            t += 0.010
            time.sleep(max(0.0, t - time.monotonic()))


def fake_signal_banner(args):
    """The loud banner for the FAKE safety signals (dry bench only)."""
    if not (args.sim_brake or args.sim_engine):
        return
    print("!" * 78)
    print("!! FAKE SAFETY SIGNALS - DRY BENCH ONLY. Never on a vehicle or an assembled transmission:")
    if args.sim_brake:
        print("!!   0x140 BRAKE PEDAL PRESSED (E2E-valid) - the brake interlock is defeated")
    if args.sim_engine:
        print(f"!!   0x108 ENGINE RUNNING at {args.sim_engine} rpm (E2E-valid)")
        print("!!   while it runs: leave_allowed() REFUSES THE FLASH HAND-OVER (>= 50 rpm) and")
        print("!!   bench_out refuses FE10/FE11. STOP THIS COMMAND (the sim stops with it) BEFORE")
        print("!!   ANY INSTALL.")
    print("!! The STOCK range chain is LIVE: with both, R/N/D RELEASE PARK (release 1.0 A + hold).")
    print("!" * 78)


def cmd_park_check(b, args):
    """Bench check of the range/park path (2026-11-17, ~/tcu_sweep/range_chain_live_port.md):
    sends the Chrysler type-1 shifter frames (0x2EA on bus 1 with CRC-8 + counter, 0x210 on
    bus 2, every 10 ms; the firmware's timeout is 50 ms) and reads the lever decode, the range
    (D6D6, the STOCK CYCLIC CHAIN's, live since 2026-11-17) and its reason D6D9, the timer
    model and the two park demands from the measurement table, step by step.

    THE STOCK RULE ON THIS BENCH (the host model test_rng_chain prints the same table):
      no --sim-brake / --sim-engine -> range 8 (P) at EVERY step, both park outputs off:
          the stock keeps P without a pressed brake (0x89434 filter, D607) and a running
          engine (893E8); r03q..r03s showed it with the chain as a shadow;
      --sim-brake --sim-engine 700 (FAKE, dry bench only) ->
          P 8, R 7, N 6, D 5, code 0 after D 5, P 8, bad-CRC R 7, 0x210 N 6, 0x2EA D 5,
          invalid code 7 -> D HELD (5: the stock keeps the last range when the lever is lost
          and only U1267 + R reacts, 0xC76D8), both missing 5, P 8;
          release + hold ON whenever the range is not 8 (~18 s of release budget at --settle 2).
    Without --start-before-power the TCU has been up without lever frames, so U1267 is
    confirmed in the chain; it heals 0.5 s into the first P step (keep --settle >= 1.0), or
    R is refused once (8 / 0x33) and N / D (8 / 0x3C).
    With --hw the demands are expected to reach the drivers (DRV_GA_STAGE 1): release = bus-A
    ch 7 at 1.0 A (under the 30 s accumulated on-time limit), hold = TOE31."""
    b.start_drain()     # keep the Teensy ports drained (lever-sim gaps, 2026-11-17)

    addr = meas_table_addr(args.elf, 54)
    release = bool(args.sim_brake) and (args.sim_engine or 0) > 430
    fake_signal_banner(args)
    if args.hw or release:
        print("PARK OUTPUTS WILL BE DRIVEN (bus-A ch 7 1.0 A + TOE31) WHENEVER THE RANGE LEAVES P")
        print("dry bench, ammeter on, supply limit 4 A; the release is limited to 30 s accumulated"
              " per power cycle")
        if input("Mechatronic dry (no pump, no driveline), ammeter running? [yes/no] ").strip() != "yes":
            return 1
    P, R, N, D = 1, 2, 3, 4
    # (description, code, 0x2EA on, 0x210 on, bad CRC, lever expected or None, source expected,
    #  range expected with the brake + engine sims)
    steps = [
        ("no shifter frames",              P, False, False, False, None, 2, 8),
        ("P on both buses",                P, True,  True,  False, 8,    0, 8),
        ("R on both buses",                R, True,  True,  False, 7,    0, 7),
        ("N on both buses",                N, True,  True,  False, 6,    0, 6),
        ("D on both buses",                D, True,  True,  False, 5,    0, 5),
        ("code 0 (no change) after D",     0, True,  True,  False, 5,    0, 5),
        ("P on both buses",                P, True,  True,  False, 8,    0, 8),
        ("0x2EA bad CRC, 0x210 R",         R, True,  True,  True,  7,    1, 7),
        ("0x2EA missing, 0x210 N",         N, False, True,  False, 6,    1, 6),
        ("0x2EA D, 0x210 missing",         D, True,  False, False, 5,    0, 5),
        ("invalid code 7 on both",         7, True,  True,  False, None, 2, 5),
        ("both missing again",             D, False, False, False, None, 2, 5),
        ("P on both buses",                P, True,  True,  False, 8,    0, 8),
    ]
    snd = LeverSender(b)
    snd.brake = bool(args.sim_brake)
    snd.rpm = int(args.sim_engine or 0)
    if args.start_before_power:
        print("TCU must be OFF now. Starting the frames on a dark bus (as the wake storm does).")
        if input("TCU switched off? [yes/no] ").strip() != "yes":
            return 1
        snd.code, snd.on_2ea, snd.on_210 = P, True, True
    snd.start()
    results = []
    srcs = {0: "CAN-C", 1: "shifter bus", 2: "none"}
    try:
        if args.start_before_power:
            input("frames running (lever P" + (", brake ON" if snd.brake else "")
                  + (f", engine {snd.rpm} rpm" if snd.rpm else "")
                  + ") - switch the TCU ON now, then press Enter ... ")
            print("waiting 5 s for boot, mode 1 and the 0x9B228 one-shot")
            time.sleep(5.0)
        print(f"measurement table at 0x{addr:08X}; {args.settle:.1f} s per step; expected: "
              + ("the stock releases on R/N/D (brake + engine sims)" if release
                 else "the stock keeps P at every step (no brake / engine)"))
        for desc, code, a, c, bad, lev, src, rng_sim in steps:
            snd.code, snd.on_2ea, snd.on_210, snd.bad_crc = code, a, c, bad
            time.sleep(args.settle)
            d = read_mem(b, args.bus, addr, 54)
            want_t = rng_sim if release else 8
            want_out = 3 if want_t != 8 else 0          # release + hold out of P, both off in P
            ok = d[20] == src and (lev is None or d[19] == lev) and d[48] == want_t and d[49] == 0
            ok = ok and (d[51] & 3) == want_out and d[53] == 0
            results.append(ok)
            print(f"  {'PASS' if ok else 'FAIL'}  {desc:30s} lever {d[19]} ({srcs.get(d[20], d[20])})  "
                  + park_line(d)
                  + ("" if ok else f"   expected lever {lev} src {src} range {want_t} reason 0x00"
                                   f" demands {want_out}"))
    finally:
        snd.stop.set()
        snd.join(1.0)
        print("all simulated frames stopped" + (" (lever, brake, engine)" if release else ""))
    print("  (the demands are the path's; whether the drivers apply them depends on DRV_GA_STAGE and"
          " the 30 s release on-time limit — witness s_rq_rel_win, chip 3 8812/881A, TOE31 DTR)")
    print(f"{sum(results)}/{len(results)} steps passed")
    return 0 if all(results) else 1


# ---------------------------------------------------------------- garage-engagement shadow (2026-11-17)

GS_WIT_SIZE = 76            # app/gs_shadow.h gs_wit_t (static-asserted there)
GS_REC_ENTRY = 28           # gs_rec_t (2026-11-18: + BD30, sync, BD76)
GS_REC_N = 112              # per slot (slot 0 _g_rec0 in .gs_rec, slot 1 _g_rec1 in .gs_obj)
GS_CH = ("A", "D", "B", "E", "C", "WK", "Sys")


def gs_wit_parse(w):
    u32 = lambda o: int.from_bytes(w[o:o + 4], "big")
    return {
        "passes": u32(0), "commits": u32(4), "commit_pass": u32(8), "commit_old": w[12],
        "commit_new": w[13], "done_cause": w[14], "rec_slot": w[15], "done_pass": u32(16),
        "loop_cap": u32(20), "e896": u32(24), "lim": u32(28), "unp": u32(32), "rel": u32(36),
        "seq": u32(40), "lockup": u32(44), "rec_n": (w[48], w[49]), "e28e": w[50], "e6a8": w[51],
        "pf": w[52], "e6ef": w[53], "ae28": w[54], "flags": w[55],
        "gear": w[56] - 256 if w[56] > 127 else w[56], "ph": tuple(w[57:60]),
        "cur": tuple(int.from_bytes(w[60 + 2 * i:62 + 2 * i], "big") for i in range(7)),
    }


def gs_wit_line(g):
    f = g["flags"]
    return (f"pf {g['pf']} E6EF {g['e6ef']} AE28 {g['ae28']} gear {g['gear']} eng-running {f & 1} "
            f"pending {(f >> 1) & 1} C3D {(f >> 2) & 1} active {(f >> 3) & 1} "
            f"ph {g['ph'][0]:X}/{g['ph'][1]:X}/{g['ph'][2]:X} E28E {g['e28e']} E6A8 {g['e6a8']} | "
            + " ".join(f"{n} {c}" for n, c in zip(GS_CH, g["cur"])))


def gs_rec_print(slot, raw, n):
    print(f"  recorder slot {slot}: {n} passes from the commit (0.1 mA; ph = D0DC/D148/D1B4 phase;"
          " fl b0-1 E6EF b2 pending b3 active b4 B timeout b5 completion b6 commit b7 C3D;"
          " sy b0 BD6C sync b1 hold b2 BD2E saturated)")
    print("  pass pf fl  ph0 ph1 ph2 md  set gear |    A    D    B    E    C   WK  Sys | bd30 sy   bd76")
    for i in range(n):
        e = raw[i * GS_REC_ENTRY:(i + 1) * GS_REC_ENTRY]
        cur = [int.from_bytes(e[10 + 2 * k:12 + 2 * k], "big") for k in range(7)]
        gear = e[9] - 256 if e[9] > 127 else e[9]
        bd76 = int.from_bytes(e[26:28], "big", signed=True)
        print(f"  {int.from_bytes(e[0:2], 'big'):4d} {e[2]:2d} {e[3]:02X}  {e[4]:3X} {e[5]:3X} {e[6]:3X}"
              f" {e[7]:02X} {e[8]:02X} {gear:4d} | " + " ".join(f"{c:4d}" for c in cur)
              + f" | {e[24]:4d} {e[25]:2X} {bd76:6d}")


GS_REL_SIZE = 0xC04         # gs_rel_t (G4a 2026-11-19, static-asserted in app/gs_shadow.h)
GS_RREC_N = 125
GS_REL_PH = ("12", "13", "14", "--")


def gs_rel_print(raw):
    """The release (G4a): BD8E, the witnesses, the two release slots (0 from D, 1 from R)."""
    u16 = lambda o: int.from_bytes(raw[o:o + 2], "big")
    u32 = lambda o: int.from_bytes(raw[o:o + 4], "big")
    s16 = lambda o: int.from_bytes(raw[o:o + 2], "big", signed=True)
    print(f"release (G4a): BD8E {u16(0)}, recording slot {raw[2]:#04x}, last slot {raw[3]:#04x}; release passes"
          f" {u32(4)}, starts {u32(8)}, unported {u32(12)}, C case {u32(16)}, 0x10E896/D252 stand-in"
          f" {u32(20)}, BD16-0 stand-in {u32(24)}")
    for k in (0, 1):
        h = 28 + 24 * k
        n = raw[h + 10]
        ln = u16(h + 4)
        print(f"  release slot {k} (from {'D' if k == 0 else 'R'}): start pass {u32(h)}, length "
              f"{'-' if ln == 0xFFFF else ln} passes, cause {raw[h + 11]} (5 0x106918, 6 BD8E timer, 4 new"
              f" request), BD8E at the end {u16(h + 6)}, limit D0DC+0x12 {u16(h + 8)}, gear "
              f"{raw[h + 12] - 256 if raw[h + 12] > 127 else raw[h + 12]}, elements {raw[h + 13]}/{raw[h + 14]}/"
              f"{raw[h + 15]}, +0x26 {s16(h + 16)}/{s16(h + 18)}/{s16(h + 20)}, {n} entries")
        if n == 0:
            continue
        print("  pass ph D0DC/D148/D1B4  fl |    A    D    B    E    C   (fl b0 releasing b1 completion"
              " b2 commit b3/b4 vented D0DC/D1B4 b5 speed exit b6/b7 out mode D0DC/D1B4)")
        prev = None
        for i in range(min(n, GS_RREC_N)):
            o = 76 + (k * GS_RREC_N + i) * 12
            e = raw[o:o + 12]
            cur = [int.from_bytes(e[2 * c:2 * c + 2], "big") for c in range(5)]
            ph, fl = e[10], e[11]
            key = (tuple(cur), ph, fl)
            if i < 3 or i % 10 == 0 or i >= n - 3 or fl & 0x06 or (prev is not None and prev[1:] != key[1:]):
                print(f"  {i:4d}    {GS_REL_PH[ph & 3]}/{GS_REL_PH[(ph >> 2) & 3]}/{GS_REL_PH[(ph >> 4) & 3]}"
                      f"     {fl:02X} | " + " ".join(f"{c:4d}" for c in cur))
            prev = key


GSBR_SIZE = 0x450          # gsbr_t (G4b 2026-11-19, static-asserted in app/gs_bridge.h)
GSBR_EST_SIZE = 0x1F4      # gsbr_est_t
GSBR_SLOT = 540
GSBR_REC_N = 125
GSBR_TRIP = {1: "B above its first-pass value", 2: "third below its first-pass value",
             3: "B's first pass above its applied value before", 4: "third's first pass below its applied value before",
             5: "outside [500, 8500] / mode-1 rule", 6: "a pinned channel off 8500", 7: "release length > 125 passes"}


def gsbr_print(raw, est, rel_raw):
    """The BRIDGED release (G4b): live staged words of B and the third element against the shadow's
    release currents (gs_rel_t slots, same pass index) and the real-feedback estimate."""
    u16 = lambda b, o: int.from_bytes(b[o:o + 2], "big")
    u32 = lambda b, o: int.from_bytes(b[o:o + 4], "big")
    print(f"bridged release (G4b): {u32(raw, 0)} bridged passes, {u16(raw, 4)} releases bridged, {u16(raw, 6)} refused"
          f" (bridge not applied before), envelope trips {u16(raw, 8)}"
          + (f" (last: code {raw[12]} = {GSBR_TRIP.get(raw[12], '?')}, ch {raw[13]}, value {u16(raw, 10)})"
             if u16(raw, 8) else "")
          + f"; now on {raw[14]} pf {raw[15]} pass {u16(raw, 16)}")
    for k in (0, 1):
        h = 24 + GSBR_SLOT * k
        sl = raw[h:h + GSBR_SLOT]
        n, pf, xch, code = sl[21], sl[18], sl[19], sl[20]
        if pf == 0:
            print(f"  bridged-release slot {k} (from {'D' if k == 0 else 'R'}): empty")
            continue
        end_t = u16(sl, 6)
        xn = {4: "C", 1: "D"}.get(xch, "?")
        endtxt = {1: "the shadow's completion", 2: f"disarmed (why {u16(sl, 8):04X})"}.get(
            code, f"ENVELOPE TRIP {GSBR_TRIP.get(code - 0x10, '?')}" if code >= 0x10 else "-")
        print(f"  bridged-release slot {k} (from {'D' if k == 0 else 'R'}): start bridge pass {u32(sl, 0)},"
              f" {u16(sl, 4)} passes bridged, ended at release pass {'-' if end_t == 0xFFFF else end_t} by {endtxt};"
              f" B first {u16(sl, 10)} (before {u16(sl, 14)}), {xn} first {u16(sl, 12)} (before {u16(sl, 16)}); {n} entries")
        if n == 0:
            continue
        print(f"  pass ap | staged B  0.1mA shadow B(prev) | staged {xn} 0.1mA shadow {xn}(prev) | est B  est {xn} (0.1 mA, 4 mA steps)")
        prevkey = None
        for i in range(min(n, GSBR_REC_N)):
            ap = (sl[22 + (i >> 3)] >> (i & 7)) & 1
            wb, wx = u16(sl, 38 + 4 * i), u16(sl, 40 + 4 * i)
            eb, ex = est[(k * GSBR_REC_N + i) * 2], est[(k * GSBR_REC_N + i) * 2 + 1]
            # the shadow's release current of the previous pass (gs_rel_t rec[k][i-1]: cur A D B E C)
            sb = sx = None
            if rel_raw is not None and i >= 1:
                o = 76 + (k * GS_RREC_N + i - 1) * 12
                e = rel_raw[o:o + 12]
                sb = int.from_bytes(e[4:6], "big")
                sx = int.from_bytes(e[2 * xch:2 * xch + 2], "big")
            key = (ap, wb, wx)
            if i < 3 or i % 10 == 0 or i >= n - 5 or (prevkey is not None and prevkey[0] != ap):
                print(f"  {i:4d} {ap:2d} | {wb:04X} {6 * (wb & 0x7FF):5d}  {'-' if sb is None else sb:>14} |"
                      f" {wx:04X} {6 * (wx & 0x7FF):5d}  {'-' if sx is None else sx:>14} | {40 * eb:5d}  {40 * ex:5d}")
            prevkey = key


def cmd_gs_check(b, args):
    """The READ-ONLY garage-engagement shadow (app/gs_shadow.c; ~/tcu_sweep/garage_shadow_port.md).
    It DRIVES NOTHING: the stock's garage engagement (direction request, power-flow commit, the
    three-element engine, the producer sources ids 6/8/10, the holder, the element law) is
    computed into the shadow's own RAM. GS_FORCED (the SH build) runs it AS IF OS mode were not 1
    and the gear target the fault-free 1st - the stock itself never commits in mode 1.
    Sequence (lever, --hold s each): P, D, P, R, P. With --sim-brake --sim-engine 700 the LIVE
    range chain releases park on D and R (release 1.0 A + hold: the only current that moves),
    the shadow commits ~150 ms later (the park model) and records 1.12 s of the engagement.
    Expected (host test test_gs_shadow, 2026-11-18 v2 at 38.9 C): P->D commit, C fills first
    (~5930, 10 passes), B waits in 0x0F, B fill ~5710, B phase 4 ~3630 (one pass), sync on the
    next pass (turbine 0), B in 0x0B for 49 passes (~4150 -> 5225, BD30 1..49), the 0x0C ramp,
    completion (cause 1) ~0.95 s after the commit, then A 8500 D 8500 B 8500 E 8500 C 500;
    P->R the same shape with D third, end A 8500 D 500 B 8500 E 8500 C 8500; WK / Sys 500.
    G2 (r04i+): the bridge APPLIES the engagement to the live solenoid currents. G4b (r04l+): after
    each lever P it also applies the stock's 1.2 s RELEASE (120 passes on the dry bench: from D B
    ~4116 -> ~3486, C ~6440 -> ~6670; from R B ~3600 -> ~3486, D ~6150 -> ~6504), then the P content;
    the bridged-release recorder (_s_gsbr / _s_gsbr_est) is printed at the end."""
    b.start_drain()     # keep the Teensy ports drained (lever-sim gaps, 2026-11-17)

    w_addr, w_size = elf_sym(args.elf, "_g_w")
    r_slots = [elf_sym(args.elf, "_g_rec0"), elf_sym(args.elf, "_g_rec1")]
    rel_addr, rel_size = elf_sym(args.elf, "_g_rel")
    if rel_size != GS_REL_SIZE:
        sys.exit(f"_g_rel {rel_size:#x}: not the gs_rel_t layout this tool expects (G4a) - rebuild?")
    if w_size != GS_WIT_SIZE or any(sz != GS_REC_N * GS_REC_ENTRY for _, sz in r_slots):
        sys.exit(f"_g_w {w_size:#x} / _g_rec0/1 {[hex(sz) for _, sz in r_slots]}: not the layout this"
                 " tool expects - rebuild?")
    release = bool(args.sim_brake) and (args.sim_engine or 0) > 430
    print("THE GARAGE SHADOW IS READ-ONLY: it drives no solenoid. The LIVE range chain still drives the"
          " park outputs (release bus-A ch 7 1.0 A + hold TOE31) whenever the range leaves P")
    print("  dry bench, ammeter on, bench supply limit 4 A; ~2 x --hold s of the 30 s release budget")
    print("  both Teensy ports are used (0x210 on bus 2): the ammeter logger cannot run - watch the meter")
    fake_signal_banner(args)
    if input("Mechatronic dry (no pump, no driveline), ammeter running, supply limit 4 A? [yes/no] ").strip() != "yes":
        return 1
    sim = VehicleSim(b)
    sim.code, sim.lever_on = 1, True
    sim.brake = bool(args.sim_brake)
    sim.rpm = int(args.sim_engine or 0)
    if args.start_before_power:
        print("TCU must be OFF now. Starting the frames on a dark bus (as the wake storm does).")
        if input("TCU switched off? [yes/no] ").strip() != "yes":
            return 1
    sim.start()
    try:
        if args.start_before_power:
            input("frames running (lever P" + (", brake ON" if sim.brake else "")
                  + (f", engine {sim.rpm} rpm" if sim.rpm else "")
                  + ") - switch the TCU ON now, then press Enter ... ")
            print("waiting 5 s for boot, mode 1 and the 0x9B228 one-shot")
            time.sleep(5.0)
        for _ in range(10):         # read hygiene (CLAUDE.md rule 6)
            if meas(b, args.bus, [0x02], timeout=1.5):
                break
            time.sleep(0.4)
        else:
            print("  WARNING: meas 0x02 got no reply (continuing with retries)")

        def read_retry(addr, n):
            for k in range(5):
                try:
                    return read_mem(b, args.bus, addr, n)
                except RuntimeError:
                    if k == 4:
                        raise
                    time.sleep(0.2)
        P, R, D = 1, 2, 4
        print(f"shadow witnesses _g_w at 0x{w_addr:08X}, recorder _g_rec0 at 0x{r_slots[0][0]:08X},"
              f" _g_rec1 at 0x{r_slots[1][0]:08X}; {args.hold:.1f} s per step")
        for name, code in (("P", P), ("D", D), ("P", P), ("R", R), ("P", P)):
            sim.code = code
            time.sleep(args.hold)
            g = gs_wit_parse(read_retry(w_addr, GS_WIT_SIZE))
            print(f"  lever {name}: " + gs_wit_line(g))
    finally:
        sim.stop.set()
        sim.join(1.0)
        print("all simulated frames stopped (lever, brake, engine)")
    g = gs_wit_parse(read_retry(w_addr, GS_WIT_SIZE))
    print(f"witnesses: passes {g['passes']} commits {g['commits']} (last {g['commit_old']} -> "
          f"{g['commit_new']} at pass {g['commit_pass']}), completion cause {g['done_cause']} "
          f"(1 normal, 2 timeout, 3 BD16-0 stand-in, 4 other, 5 release 0x106918, 6 release BD8E timer)"
          f" at pass {g['done_pass']}")
    print(f"  stand-ins taken: 0x10E896 term {g['e896']}, 0x1147C8 limit {g['lim']}, release passes {g['rel']}, "
          f"0x104A28 {g['seq']}, lockup head {g['lockup']}, unported {g['unp']}, loop cap {g['loop_cap']}")
    for slot in (0, 1):
        n = min(g["rec_n"][slot], GS_REC_N)
        if n == 0:
            print(f"  recorder slot {slot}: empty" + ("" if release else " (no commit without the sims)"))
            continue
        raw = read_retry(r_slots[slot][0], n * GS_REC_ENTRY)
        gs_rec_print(slot, raw, n)
    # the release object: the 76-byte head, then only the recorded entries of each slot
    raw = bytearray(GS_REL_SIZE)
    raw[0:76] = read_retry(rel_addr, 76)
    for k in (0, 1):
        n = min(raw[28 + 24 * k + 10], GS_RREC_N)
        o = 76 + k * GS_RREC_N * 12
        if n:
            raw[o:o + 12 * n] = read_retry(rel_addr + o, 12 * n)
    gs_rel_print(bytes(raw))
    # G4b: the bridged release (live staged words vs the shadow's release, the estimate)
    try:
        br_addr, br_size = elf_sym(args.elf, "_s_gsbr")
        be_addr, be_size = elf_sym(args.elf, "_s_gsbr_est")
    except SystemExit:
        print("bridged release: no _s_gsbr in this ELF (before G4b)")
        return 0
    if br_size != GSBR_SIZE or be_size != GSBR_EST_SIZE:
        sys.exit(f"_s_gsbr {br_size:#x} / _s_gsbr_est {be_size:#x}: not the G4b layout - rebuild?")
    braw = bytearray(GSBR_SIZE)
    braw[0:24] = read_retry(br_addr, 24)
    for k in (0, 1):
        h = 24 + GSBR_SLOT * k
        braw[h:h + 38] = read_retry(br_addr + h, 38)
        n = min(braw[h + 21], GSBR_REC_N)
        if n:
            braw[h + 38:h + 38 + 4 * n] = read_retry(br_addr + h + 38, 4 * n)
    best = bytes(read_retry(be_addr, GSBR_EST_SIZE))
    gsbr_print(bytes(braw), best, bytes(raw))
    return 0


# ---------------------------------------------------------------- range chain check (live since 2026-11-17)

def e2e_frame(data, ctr):
    """CAN-C variant-1 protection as io/vehicle/profile_fca.c checks it (bsw/com/com.c):
    counter in the upper nibble of byte dlc-2, CRC-8 J1850 over bytes 0..dlc-2 in the last."""
    d = bytearray(data)
    d[-2] = (d[-2] & 0x0F) | ((ctr & 0x0F) << 4)
    d[-1] = crc8_j1850(d[:-1])
    return bytes(d)


class VehicleSim(threading.Thread):
    """Every 10 ms (the firmware's receive timeout is 50 ms for each):
      0x2EA (bus 1, CRC + counter) and 0x210 (bus 2) - the shifter, lever code self.code;
      0x140 (bus 1, 8 bytes, E2E) - brake switch byte 0 bits 3..2 = 01 when self.brake;
      0x108 (bus 1, 8 bytes, E2E) - engine speed bytes 0-1 = self.rpm (torque T1 0x1FFF invalid).
    0x140 / 0x108 are sent only while self.brake / self.rpm are set. FAKE SAFETY SIGNALS."""
    def __init__(self, b):
        super().__init__(daemon=True)
        self.b = b
        self.code, self.lever_on, self.brake, self.rpm = 1, True, False, 0
        self.stop = threading.Event()
        self.sent = 0

    def run(self):
        ctr, t = 0, time.monotonic()
        while not self.stop.is_set():
            ctr = (ctr + 1) & 0x0F
            try:
                if self.lever_on:
                    d2ea, d210 = lever_frames(self.code, ctr)
                    self.b.send(1, 0x2EA, d2ea)
                    self.b.send(2, 0x210, d210)
                if self.brake:
                    self.b.send(1, 0x140, e2e_frame([0x04, 0, 0, 0, 0, 0, 0, 0], ctr))
                if self.rpm:
                    r = self.rpm & 0x7FFF
                    self.b.send(1, 0x108, e2e_frame([r >> 8, r & 0xFF, 0x1F, 0xFF, 0, 0, 0, 0], ctr))
                self.sent += 1
            except Exception as e:                     # keep going; the step check will show it
                print("  send error:", e)
            t += 0.010
            time.sleep(max(0.0, t - time.monotonic()))


# the live range chain (firmware/app/rng_chain.h, 2026-11-17): its cells are the STOCK cells at
# the stock addresses (read directly); the fault-manager model is ours, s_rc_m (rng_chain_model_t)
RC_BLOCKS = {"d5e4": (0xFFF8D5E4, 0x30), "d61a": (0xFFF8D61A, 0x80), "d6ca": (0xFFF8D6CA, 0x16),
             "e530": (0xFFF8E530, 0x1C), "893e0": (0xFFF893E0, 0x18), "304c": (0xFFF8304C, 0x18)}
RCM_SIZE = 0x80       # rng_chain_model_t (0x74 until 2026-11-17: + the DSM operating cycle)
RCM_DSM, RCM_RECYCLE = 0x74, 0x78     # 0xAE890's state, terminal re-cycles (keyoff_autop_port.md)
RCM_G109, RCM_G98, RCM_U_FAILED, RCM_U_TESTED = 0x02, 0x03, 0x00, 0x01
RCM_U_FAIL, RCM_U_PASS, RCM_SKIP, RCM_FM_MS, RCM_FM109, RCM_FM98 = 0x14, 0x18, 0x1C, 0x20, 0x24, 0x4C


def elf_sym(elf, name):
    """(address, size) of a symbol from sh-elf-nm -S."""
    import subprocess
    out = subprocess.run(["sh-elf-nm", "-S", elf], capture_output=True, text=True, check=True).stdout
    for l in out.splitlines():
        f = l.split()
        if len(f) == 4 and f[3] == name:
            return int(f[0], 16), int(f[1], 16)
    sys.exit(f"symbol {name} not found in {elf} - rebuild?")


class ChainCells:
    """The range chain's cells read from RAM, indexed by stock address."""
    def __init__(self, blocks, model):
        self.blk, self.m = blocks, model

    def c(self, a):
        a |= 0xFFF80000
        for base, data in self.blk.values():
            if base <= a < base + len(data):
                return data[a - base]
        raise KeyError(hex(a))

    def w(self, a):
        return self.c(a) << 8 | self.c(a + 1)

    def l(self, a):
        return self.w(a) << 16 | self.w(a + 2)


def chain_line(cc):
    """Decode the live range chain (stock cells + our fault-manager model)."""
    m = cc.m
    u16 = lambda o: int.from_bytes(m[o:o + 2], "big")
    u32 = lambda o: int.from_bytes(m[o:o + 4], "big")
    return (f"CHAIN D6D6 {cc.c(0xD6D6)}/{cc.c(0xD6D7):02X} rsn 0x{cc.c(0xD6D9):02X} "
            f"D6DE {cc.c(0xD6DE)}/{cc.c(0xD6DF):02X} E545 {cc.c(0xE545)} 304C {cc.l(0x304C)} | "
            f"893E8 {cc.c(0x893E8)} D607 {cc.c(0xD607)} D5FB {cc.c(0xD5FB)} D5F2 {cc.c(0xD5F2)} "
            f"D672/3 {cc.c(0xD672)}/{cc.c(0xD673)} | brake D631 {cc.c(0xD631)} D64B {cc.c(0xD64B)} "
            f"eng D63A {cc.w(0xD63A)} D63C {cc.c(0xD63C)} | U1267 g109 {m[RCM_G109]} last f/t "
            f"{m[RCM_U_FAILED]}/{m[RCM_U_TESTED]} D60C {cc.w(0xD60C)} fail/pass "
            f"{u32(RCM_U_FAIL)}/{u32(RCM_U_PASS)} P0850 g98 {m[RCM_G98]} | D660 {cc.c(0xD660)} "
            f"D661 {cc.c(0xD661)} D664 {cc.c(0xD664)} D665 {cc.c(0xD665)} D666 {cc.c(0xD666)} "
            f"skip {u32(RCM_SKIP)} | KL15 D641 {cc.c(0xD641)} D642 {cc.c(0xD642)} D643 {cc.c(0xD643)} "
            f"cycle {m[RCM_DSM]} re-cycles {u32(RCM_RECYCLE)} | FM ms {u32(RCM_FM_MS)} " + " ".join(
                f"{nm}: st {u16(o):04X} t {u16(o + 2)} max {u16(o + 6)} in-masks {m[o + 10]} "
                f"conf {u32(o + 0x14)}@{u32(o + 0x20)} heal {u32(o + 0x18)}@{u32(o + 0x24)}"
                for nm, o in (("g109", RCM_FM109), ("g98", RCM_FM98))))


def cmd_range_check(b, args):
    """The LIVE range chain (app/rng_chain.c; 2026-11-17, ~/tcu_sweep/range_chain_live_port.md;
    the r03q..r03s shadow-check, alias shadow-check) with the lever simulation and OPTIONAL
    fake brake / engine signals. Sequence P, R, N, D, P (--settle s each); reads the chain's
    cells at their stock addresses and the park path from the measurement table.
    Expected (the host model test_rng_chain): with --sim-brake and --sim-engine > 430 the
    range follows the lever, P 8 R 7 N 6 D 5 P 8 (reasons 0) and the park outputs are ON out
    of P; without them the range is 8 throughout and the outputs stay off. The lever is never
    dropped. --start-before-power: the frames start BEFORE the TCU is switched on (U1267 is
    never tested); without it U1267 is confirmed and heals 0.5 s into the first P step."""
    b.start_drain()     # keep the Teensy ports drained (lever-sim gaps, 2026-11-17)

    m_addr = meas_table_addr(args.elf, 54)
    rcm_addr, rcm_size = elf_sym(args.elf, "_s_rc_m")
    if rcm_size != RCM_SIZE:
        sys.exit(f"_s_rc_m is {rcm_size:#x} bytes, this tool expects {RCM_SIZE:#x}: the struct changed")
    d6ca_addr, _ = elf_sym(args.elf, "_rc_d6ca")
    if d6ca_addr != 0xFFF8D6CA:
        sys.exit(f"_rc_d6ca at {d6ca_addr:#x}: not the live-chain build (expected 0xFFF8D6CA)")
    release = bool(args.sim_brake) and (args.sim_engine or 0) > 430
    print("THE STOCK RANGE CHAIN IS LIVE: it drives the park outputs (release bus-A ch 7 1.0 A +"
          " hold TOE31) whenever the range leaves P")
    print("  dry bench, ammeter on, bench supply limit 4 A (~3.0 A warm, out of P); ~6 s of the 30 s"
          " release budget per run at --settle 2 with the sims")
    print("  both Teensy ports are used (0x210 on bus 2): the ammeter logger cannot run - watch the meter")
    fake_signal_banner(args)
    if input("Mechatronic dry (no pump, no driveline), ammeter running, supply limit 4 A? [yes/no] ").strip() != "yes":
        return 1
    sim = VehicleSim(b)
    sim.code, sim.lever_on = 1, True
    sim.brake = bool(args.sim_brake)
    sim.rpm = int(args.sim_engine or 0)
    if args.start_before_power:
        print("TCU must be OFF now. Starting the frames on a dark bus (as the wake storm does).")
        if input("TCU switched off? [yes/no] ").strip() != "yes":
            return 1
    sim.start()
    results = []
    try:
        if args.start_before_power:
            input("frames running (lever P" + (", brake ON" if sim.brake else "")
                  + (f", engine {sim.rpm} rpm" if sim.rpm else "")
                  + ") - switch the TCU ON now, then press Enter ... ")
            print("waiting 5 s for boot, mode 1 and the 0x9B228 one-shot")
            time.sleep(5.0)
        # read hygiene (CLAUDE.md rule 6): meas 0x02 first - the periodic 0x398 stream
        # races the meas replies and garbles reads; then retry each read a few times
        for _ in range(10):
            if meas(b, args.bus, [0x02], timeout=1.5):
                break
            time.sleep(0.4)
        else:
            print("  WARNING: meas 0x02 got no reply (continuing with retries)")

        def read_retry(addr, n):
            for k in range(5):
                try:
                    return read_mem(b, args.bus, addr, n)
                except RuntimeError:
                    if k == 4:
                        raise
                    time.sleep(0.2)
        P, R, N, D = 1, 2, 3, 4
        steps = [("P", P, 8), ("R", R, 7), ("N", N, 6), ("D", D, 5), ("P", P, 8)]
        print(f"measurement table at 0x{m_addr:08X}, chain model s_rc_m at 0x{rcm_addr:08X}; "
              f"{args.settle:.1f} s per step")
        cc = None
        for name, code, lev in steps:
            sim.code = code
            time.sleep(args.settle)
            d = read_retry(m_addr, 54)
            cc = ChainCells({k: (a, read_retry(a, n)) for k, (a, n) in RC_BLOCKS.items()},
                            read_retry(rcm_addr, RCM_SIZE))
            want = lev if (release and not cc.m[RCM_G109]) else 8
            ok = cc.c(0xD6D6) == want and d[48] == want and (d[51] & 3) == (3 if want != 8 else 0) \
                and d[53] == 0
            results.append(ok)
            print(f"  {'PASS' if ok else 'FAIL'} lever {name}: lever {d[19]} src {d[20]} "
                  f"eng {int.from_bytes(d[0:2], 'big')} brake {d[12]} | " + park_line(d))
            print("       " + chain_line(cc) + ("" if ok else f"   expected D6D6 {want}"))
        if release and cc is not None and cc.m[RCM_G109]:
            print("  U1267 is confirmed in the chain (lever missing > 0.5 s into mode 1): R/N/D stay 8"
                  " until it heals (0.5 s of valid lever) - rerun, or use --start-before-power")
    finally:
        sim.stop.set()
        sim.join(1.0)
        print("all simulated frames stopped (lever, brake, engine)")
    print(f"{sum(results)}/{len(results)} steps match the stock chain's prediction "
          f"({'brake + engine > 430: the stock releases' if release else 'no brake/engine: the stock keeps P'})")
    return 0 if all(results) else 1


# ---------------------------------------------------------------- key-off auto-P (2026-11-17)
# 0x112 IS THE IGNITION STATUS (~/tcu_sweep/can_inputs_decode.md, keyoff_autop_port.md): byte 0
# bits 2..0, 4 / 5 = on, 7 = not available; ANY OTHER VALID CODE TURNS THE IGNITION LOGICALLY OFF
# (the terminal-15 state E9FF -> 3 -> 4: the range chain forces P (0xC286C, 8 / 0x50); on the STOCK
# firmware also the OS mode drops to 1 and [I] the power-down path starts). This tool never sends
# 0x112 except from IgnitionSender, and IgnitionSender refuses a code other than 4 / 5 unless it was
# built with allow_off=True - which only cmd_keyoff_check does, after its banner and a yes.
KL15_OBJ, KL15_LEN = 0xFFF8E9D4, 0x31         # safety/kl15.c: the stock object 0x140B80


def ign_frame(code, allow_off=False):
    """0x112 data (dlc 2, no E2E: com.c M_112) for ignition code `code`."""
    if code not in (4, 5) and not allow_off:
        raise ValueError(f"0x112 code {code} would turn the ignition OFF - refused "
                         "(only keyoff-check may send it)")
    if not 0 <= code <= 7:
        raise ValueError(f"0x112 code {code} out of range")
    return bytes([code & 0x07, 0])


class IgnitionSender(threading.Thread):
    """0x112 on bus 1 every 10 ms (the firmware's timeout is 50 ms); self.code = None stops
    sending (0x112 lost -> the voltage path). A code other than 4 / 5 needs allow_off."""
    def __init__(self, b, allow_off=False):
        super().__init__(daemon=True)
        self.b, self.allow_off = b, allow_off
        self.code = 4
        self.stop = threading.Event()

    def run(self):
        t = time.monotonic()
        while not self.stop.is_set():
            code = self.code
            if code is not None:
                try:
                    self.b.send(1, 0x112, ign_frame(code, self.allow_off))
                except ValueError:
                    raise
                except Exception as e:                 # keep going; the step check will show it
                    print("  send error:", e)
            t += 0.010
            time.sleep(max(0.0, t - time.monotonic()))


def cmd_keyoff_check(b, args):
    """KEY-OFF AUTO-P TEST (project firmware with safety/kl15.c, 2026-11-17): drives 0x112 (the
    ignition status) with the lever / brake / engine sims, and reads the terminal-15 object
    (0xFFF8E9D4: E9FF, quality, agreement), the chain's D641 / D642 and the park path per step.
    Expected (host model test_keyoff):
      ign 4 + P            -> E9FF 2, quality 0 (CAN path), range 8, outputs off
      ign 4 + D (sims)     -> range 5, release + hold ON
      ign OFF (--off-code) -> E9FF 4 two passes later; range 8 in the same pass (D6DE 8 / 0x50,
                              0xC286C), both outputs off; mon 0x00
      ign 4 again, lever D -> range stays 8 (the lever latch D5FB), outputs off
      lever P, then D      -> range 5, outputs ON again; mon 0x00
      0x112 stopped (lost) -> the voltage path: E9FF stays 2 while the supply is >= 4 V; after the
                              key-off with the supply on the unit has learned "disagree" (+0x21 =
                              2, after 0.5 s) -> quality 2; with the engine sim running (893E8 >= 1)
                              D5F2 stays 1 and D is KEPT (released); without an engine it would be
                              key-off (RAM only in our firmware - a power cycle restores 1; THE
                              STOCK FIRMWARE WOULD STORE IT IN EEPROM)
    Without --sim-brake / --sim-engine > 430 every step keeps P (the key-off is then only seen in
    E9FF / D641)."""
    b.start_drain()
    if args.off_code in (4, 5) or not 0 <= args.off_code <= 6:
        sys.exit("--off-code must be an 'off' code: 0, 1, 2, 3 or 6")
    m_addr = meas_table_addr(args.elf, 54)
    rcm_addr, rcm_size = elf_sym(args.elf, "_s_rc_m")
    if rcm_size != RCM_SIZE:
        sys.exit(f"_s_rc_m is {rcm_size:#x} bytes, this tool expects {RCM_SIZE:#x}: the struct changed")
    release = bool(args.sim_brake) and (args.sim_engine or 0) > 430
    print("!" * 78)
    print(f"!! KEY-OFF TEST: this command sends 0x112 (IGNITION STATUS) with the OFF code {args.off_code}.")
    print("!! Project firmware only. NEVER on the stock firmware (it would start its power-down path and")
    print("!! learn the CAN / voltage disagreement into EEPROM), never on a vehicle.")
    print("!! The STOCK range chain is LIVE: with the sims, D releases park (release 1.0 A + hold).")
    print("!" * 78)
    fake_signal_banner(args)
    if input("Project firmware, mechatronic dry, ammeter running, supply limit 4 A? [yes/no] ").strip() != "yes":
        return 1
    lev = LeverSender(b)
    lev.code, lev.on_2ea, lev.on_210 = 1, True, True
    lev.brake = bool(args.sim_brake)
    lev.rpm = int(args.sim_engine or 0)
    ign = IgnitionSender(b, allow_off=True)
    ign.code = 4
    results = []
    try:
        lev.start()
        ign.start()
        for _ in range(10):
            if meas(b, args.bus, [0x02], timeout=1.5):
                break
            time.sleep(0.4)

        def read_retry(addr, n):
            for k in range(5):
                try:
                    return read_mem(b, args.bus, addr, n)
                except RuntimeError:
                    if k == 4:
                        raise
                    time.sleep(0.2)
        P, D = 1, 4
        rel = 5 if release else 8
        # (description, lever, 0x112 code or None, expected E9FF, expected quality, expected range)
        steps = [("ign 4, lever P",              P, 4,             2, 0, 8),
                 ("ign 4, lever D",              D, 4,             2, 0, rel),
                 (f"ign OFF ({args.off_code}), lever D", D, args.off_code, 4, 0, 8),
                 ("ign 4 again, lever D",        D, 4,             2, 0, 8),
                 ("ign 4, lever P",              P, 4,             2, 0, 8),
                 ("ign 4, lever D",              D, 4,             2, 0, rel),
                 ("0x112 stopped, lever D",      D, None,          2, 2, rel),
                 ("0x112 stopped, lever P",      P, None,          2, 2, 8)]
        for desc, lv, code, w_ff, w_q, w_rng in steps:
            lev.code, ign.code = lv, code
            time.sleep(args.settle)
            d = read_retry(m_addr, 54)
            o = read_retry(KL15_OBJ, KL15_LEN)
            g = read_retry(0xFFF8D641, 3)
            ok = o[0x2B] == w_ff and o[0x2D] == w_q and d[48] == w_rng and d[53] == 0
            ok = ok and (d[51] & 3) == (3 if w_rng != 8 else 0)
            results.append(ok)
            print(f"  {'PASS' if ok else 'FAIL'}  {desc:30s} E9FF {o[0x2B]} q {o[0x2D]} agree {o[0x21]} "
                  f"CANinv {o[0x20]} code {o[0x26]} V {int.from_bytes(o[0x24:0x26], 'big')} mV | "
                  f"D641/2/3 {g[0]}/{g[1]}/{g[2]} | " + park_line(d)
                  + ("" if ok else f"   expected E9FF {w_ff} q {w_q} range {w_rng}"))
    finally:
        ign.code = None
        ign.stop.set()
        lev.stop.set()
        ign.join(1.0)
        lev.join(1.0)
        print("all simulated frames stopped (0x112, lever, brake, engine); power-cycle the TCU to"
              " restore the agreement flag")
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
    p = sub.add_parser("park"); p.add_argument("--bus", type=int, default=1)
    p.add_argument("--elf", default="build/sh2a/tcu_app.elf"); p.add_argument("--period", type=float, default=1.0)
    p.add_argument("--count", type=int, default=0)
    def sim_args(p):
        p.add_argument("--sim-brake", action="store_true", help="FAKE: send 0x140 brake pressed (dry bench only)")
        p.add_argument("--sim-engine", type=int, default=0, metavar="RPM",
                       help="FAKE: send 0x108 engine speed RPM (dry bench only; stop before any install)")
        p.add_argument("--start-before-power", action="store_true",
                       help="start the frames with the TCU off, then ask for power-on (keeps U1267 untested)")
    p = sub.add_parser("park-check"); p.add_argument("--bus", type=int, default=1)
    p.add_argument("--elf", default="build/sh2a/tcu_app.elf"); p.add_argument("--settle", type=float, default=2.0)
    p.add_argument("--hw", action="store_true", help="the image drives the park outputs (DRV_GA_STAGE 1)")
    sim_args(p)
    for nm in ("range-check", "shadow-check"):
        p = sub.add_parser(nm); p.add_argument("--bus", type=int, default=1)
        p.add_argument("--elf", default="build/sh2a/tcu_app.elf"); p.add_argument("--settle", type=float, default=2.0)
        sim_args(p)
    p = sub.add_parser("gs-check"); p.add_argument("--bus", type=int, default=1)
    p.add_argument("--elf", default="build/sh2a/tcu_app.elf"); p.add_argument("--hold", type=float, default=2.0)
    sim_args(p)
    p = sub.add_parser("keyoff-check"); p.add_argument("--bus", type=int, default=1)
    p.add_argument("--elf", default="build/sh2a/tcu_app.elf"); p.add_argument("--settle", type=float, default=1.0)
    p.add_argument("--off-code", type=int, default=0, help="the 0x112 OFF code sent (0, 1, 2, 3 or 6)")
    sim_args(p)
    p = sub.add_parser("park-out"); p.add_argument("--bus", type=int, default=1)
    p.add_argument("--csv", default="m4b_park_out_results.csv")
    p = sub.add_parser("app-flash"); p.add_argument("image", nargs="?", default="build/sh2a/tcu_app_df.bin")
    p.add_argument("--yes", action="store_true"); p.add_argument("--send-s", type=float, default=60.0)
    p.add_argument("--slow", action="store_true", help="power-cut test: stretch the write to about 35 s")
    p = sub.add_parser("app-selftest"); p.add_argument("--send-s", type=float, default=60.0)
    p = sub.add_parser("plant-uds"); p.add_argument("hex", nargs="+", help="requests, comma-separated")
    p.add_argument("--timeout", type=float, default=1.0)
    p = sub.add_parser("plant-seedscan"); p.add_argument("--enter", action="store_true")
    p.add_argument("--sessions", default="60"); p.add_argument("--send-s", type=float, default=60.0)
    p.add_argument("--mode", choices=["can", "kcn"], default="can")
    p = sub.add_parser("plant-probe34"); p.add_argument("--session", type=lambda x: int(x, 0), default=0x60)
    p.add_argument("--addr", type=lambda x: int(x, 0), default=PD_WINDOW)
    p.add_argument("--length", type=lambda x: int(x, 0), default=0x40)
    p = sub.add_parser("plant-dump-session")
    p.add_argument("--session", type=lambda x: int(x, 0), default=0x60)
    p.add_argument("--image", default="build/plantdump.bin")
    p.add_argument("--canary", default="build/plantcanary.bin")
    p.add_argument("--swap32", action="store_true", help="pre-swap image words if the transfer swaps")
    p.add_argument("--watch-s", type=float, default=3.0, help="how long to listen after each 37")
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
         "plant-probe34": cmd_plant_probe34, "plant-dump-session": cmd_plant_dump_session,
         "app-selftest": cmd_app_selftest, "sensors": cmd_sensors, "park": cmd_park,
         "park-check": cmd_park_check, "park-out": cmd_park_out,
         "range-check": cmd_range_check, "shadow-check": cmd_range_check,
         "keyoff-check": cmd_keyoff_check, "gs-check": cmd_gs_check}[args.cmd](b, args)
    finally:
        b.shutdown()


if __name__ == "__main__":
    main()
