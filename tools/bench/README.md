# TCU bench set-up

Spare controller only — never the vehicle's unit. Keep the spare's full read-out (flash +
EEPROM) before writing anything to it.

## 1. Hardware

| Item | Notes |
|---|---|
| Bench power supply | 12–13.5 V, **current limit 2 A** for M0–M2 (no loads); raise only for the M3 test |
| Teensy 4.1 + **two 3.3 V CAN transceivers** | for example SN65HVD230 or TJA1051T/3 boards; the Teensy's CAN pins are logic level only |
| 2 × 120 Ω resistors | CAN termination, see §3 |
| Switch | ignition (pin 9) |
| M3 only: power resistor ~6–10 Ω / 25 W (or a spare solenoid) and an ammeter | dummy load |

## 2. Wiring

TCU 16-pin round connector (spec 01 §2.8):

| TCU pin | Signal | Connect to |
|---|---|---|
| 13 | battery B+ | supply + (through a fuse) |
| 9 | ignition run/start | supply + through the ignition switch |
| 14 | ground | supply − |
| 5 | CAN-C (+) CAN_H | transceiver A CANH |
| 6 | CAN-C (−) CAN_L | transceiver A CANL |
| 4 | shifter CAN (+) CAN_H | transceiver B CANH |
| 3 | shifter CAN (−) CAN_L | transceiver B CANL |

Teensy 4.1 to transceivers (3.3 V supply from the Teensy, common ground with the bench supply):

| Teensy pin | Transceiver |
|---|---|
| 22 (CAN1 TX) | A: TXD |
| 23 (CAN1 RX) | A: RXD |
| 1 (CAN2 TX) | B: TXD |
| 0 (CAN2 RX) | B: RXD |
| 3.3 V, GND | VCC, GND of both (and S/RS pin to GND = high-speed mode, if present) |

**Waveshare SN65HVD230 CAN board:** pins are labelled `3V3`, `GND`, `CTX`, `CRX`, `CANH`, `CANL`.
`CTX` goes to the Teensy TX pin (22 or 1), `CRX` to the RX pin (23 or 0). The board carries its
own **120 Ω termination** between CANH and CANL, so it already terminates the Teensy end; do not
add a second resistor there (§3).

## 3. Termination

With everything unpowered, measure the resistance between TCU pins 5 and 6, and between 3 and 4:
- about 120 Ω → the TCU terminates that bus: add **one** 120 Ω at the transceiver end;
- open (very high) → add 120 Ω at both ends (or a single 60 Ω on a short bench harness).
Many transceiver boards already carry a 120 Ω resistor; check before adding another.
Record the readings (they tell us whether the TCU has internal termination).

**Measured on the spare (2026-10-17): 27.68 kΩ on both pairs → the TCU has no termination.**
With the Waveshare boards (120 Ω on board) at the Teensy end, add **one 120 Ω resistor across
TCU pins 5–6 and one across pins 3–4**. With everything connected and unpowered, each bus should
then read about 60 Ω.

## 4. Teensy firmware

Arduino IDE with Teensyduino: open `teensy_slcan/teensy_slcan.ino`, select **Teensy 4.1** and
**Tools → USB Type → Dual Serial**, upload. The two USB serial ports appear as
`/dev/ttyACM0` (bus 1, pins 5/6) and `/dev/ttyACM1` (bus 2, pins 3/4); check with
`ls /dev/ttyACM*` after plugging in.

Command line instead (arduino-cli in `~/opt/arduino-cli`, Teensy core 1.62.0 installed):

```
~/opt/arduino-cli/arduino-cli compile --fqbn teensy:avr:teensy41:usb=serial2 tools/bench/teensy_slcan
~/opt/arduino-cli/arduino-cli board list
~/opt/arduino-cli/arduino-cli upload --fqbn teensy:avr:teensy41:usb=serial2 -p <port from board list> tools/bench/teensy_slcan
```

Uploading as a normal user needs PJRC's udev rule once:
`wget https://www.pjrc.com/teensy/00-teensy.rules && sudo cp 00-teensy.rules /etc/udev/rules.d/`
(then unplug and replug the Teensy).

## 5. PC software

```
sudo apt install can-utils python3-can
```

Either use the ports directly:

```
python3 tools/bench/tcu_bench.py monitor          # Teensy on /dev/ttyACM0 + ACM1 is the default
```

or make them SocketCAN interfaces (needed for `candump`):

```
sudo slcand -o -s6 /dev/ttyACM0 can0 && sudo ip link set can0 up
sudo slcand -o -s6 /dev/ttyACM1 can1 && sudo ip link set can1 up
python3 tools/bench/tcu_bench.py --iface socketcan monitor
```

## 6. Session plan

**A. Stock firmware (before anything is written to the spare)**
1. Power up, `monitor`: list the IDs the stock TCU sends on each bus (it should transmit
   0x037, 0x144, 0x170, … on bus 1). This also confirms the wiring and termination.
2. `meas-ident` and `meas-read 0xFFF8B5FC 2` (engine speed variable) on bus 2: confirms the
   measurement protocol (spec 10 §6).
   **Sniffing is always listen-only**: `log` puts the Teensy in listen-only mode by default.
   An acknowledging node on the bus made an ACDP-2 flash read fail (spec 06 §5b).
3. `item20`: dumps the stock boot request record and walks through logging the ACDP-2
   sequence (spec 06 §5a, spec 15). **Stop the ACDP-2 before it erases anything** unless the
   image being written is this unit's own stock read-out.
4. `log stock_idle.log` for a minute: replay material for the simulator.

**B. Project firmware (M0–M3)**, only after A and with a recovery plan (memory note: the ACDP-2
reaches the TCU only over CAN, so the hand-over must work):
1. Write the image built from the spare's own read-out; `monitor`:
   - `0x7F0` every 100 ms, counter rising, `sup_ok=1`, no reset messages;
   - which bus `0x7F0` arrives on = which pin pair is firmware bus 1 (bench item 18).
2. `info`: software version, inputs, sensors, outputs (`driver faults [none] ready=1`).
2a. Back up the user boot MAT: first prove the reading path with
    `meas-read 0x0 0x40000 --compare "<read-out>_IFla.bin"` (boot blocks identical), then
    `dump-userboot -o userboot_mat.bin` (CRC-32 must report OK). Keep the file private.
3. Hand-over test: ACDP-2 writes the stock image back (bench item 21). If this works, the way
   back is proven.
4. M3: `solenoid` with a dummy load (procedure in `firmware/README.md`).

**C. Project reflash without the ACDP-2 (spec 16)**, after B.1 with an image that contains the
flasher (built 2026-10-17 or later). Bus 1 only (TCU pins 5/6). Never with the engine running.
1. `uds 22 FD 03` → `62 FD 03 01` (FWE high). `01` is required; `00` = stop.
2. `flash IMAGE --dry-run`: starts the flasher, compares every block, prints the plan, resets.
3. `flash-selftest IMAGE`: erase / program / verify / erase one block that neither the image nor
   the TCU uses (default 0x100000). The markers are not touched.
4. Safety net, once: `flash-invalidate`, power-cycle (TCU must stay silent), restore with the
   ACDP-2 power-up write, check `0x7F0` returns.
5. `flash IMAGE`: real write. Do not remove power until it reports done. If it fails, the
   flasher keeps running: rerun the same command. The flash is rated for 1000 erase cycles per
   block; only differing blocks are rewritten.
