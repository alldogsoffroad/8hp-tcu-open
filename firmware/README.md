# Project firmware

Open C firmware for the Bosch ZF 8HP TCU (SH7254R). Architecture: `docs/firmware/00-architecture.md`.

Current milestone: **M3** — solenoid driver chips (`io/actuators/sol_drv.c`, spec 12 §2b) and a
bench-only solenoid test (`app/bench_out.c`); outputs are off unless a bench test is running.
M2: the transmission's own sensors (speeds, fluid temperature, supply, park contacts;
`io/sensors`, specs 02 §4.1a and 04 §4.1). M1 summary: boots in place of
the stock application, serves the watchdog,
receives and checks the vehicle bus (CRC-8, rolling counter, timeouts; spec 07), decodes the
traced signals into project units (`io/vehicle/profile_fca.c`), answers diagnostics on
`0x7E1`/`0x7DF` → `0x7E9`, offers a read-only measurement protocol on `0x361` → `0x398`
(spec 10 §6a), and has the stock periodic transmit messages ready but **disabled** (spec 08 §4a) and hands over to the Bosch boot software on `0x10 0x02` (spec 06 §5a, spec 15) — historically used so the
ACDP-2 could still reprogram the unit. It drives **no outputs**: all
solenoid, PWM and switch outputs stay in the safe state set up by `port_init()` (same pin state
as the stock firmware before its drivers start).

**Before any bench write of a project image, confirm the way back.** ~~The hand-over (bench item 21
in `docs/spec/README.md`) is unverified. If it fails, recovery depends on how ACDP-2 module 19
reaches the controller: a connection that needs the running application's cooperation (CAN
diagnostics) would be blocked, while a bench / boot-mode connection would not. Whether the
Bosch boot manager offers a programming window at power-up independent of the application is
not known (spec 06 §8).~~ **Superseded (2026-10-30): the boot MAT opens a programming window at
every power-up (spec 17), and the Teensy flasher
([8hp-teensy-flasher](https://github.com/alldogsoffroad/8hp-teensy-flasher)) reads and writes
the unit through it regardless of the application's state — recovery is guaranteed and needs
no proprietary tool. Still keep the spare's full read-out before writing.**

## Build

Host build (simulator and unit tests, runs on the PC):

```
cmake -S firmware -B build/host -G Ninja
cmake --build build/host
ctest --test-dir build/host
TCU_SIM_MS=1000 build/host/tcu_sim
TCU_SIM_MS=5000 TCU_SIM_CANLOG=capture.log build/host/tcu_sim   # replay a candump -L log
```

In the replay, `can0` is bus 1 (CAN-C) and `can1` is bus 2.

Target build (needs `sh-elf-gcc` in `~/opt/sh-elf` and Debian's `binutils-sh-elf`):

```
cmake -S firmware -B build/sh2a -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/cmake/toolchain-sh2a.cmake"
cmake --build build/sh2a
python3 tools/build_image.py --app build/sh2a/tcu_app.hex --base "<your read-out>_IFla.bin" -o build/m2_IFla.bin
```

`build_image.py` keeps the boot blocks and the calibration block of your own read-out,
replaces the application block, writes the header CRC and the validity markers, and checks the
header fields that the Bosch boot manager verifies (spec 06).

## Bench status frames (bench only)

CAN bus 1 (vehicle connector pins 5/6, CAN-C), 500 kbit/s, identifier `0x7F0`, every 100 ms:

| Byte | Content |
|---|---|
| 0 | rolling counter, restarts at 0 after every reset |
| 1 | bit 0: a 1 ms tick was missed; bit 1: supervisor reply valid; bit 2: bad supervisor reply seen; bit 3: output enable sent |
| 2–3 | uptime, seconds |
| 4–7 | idle-loop count in the last 100 ms (gives the CPU clock, spec 05 §1.2) |

`0x7F1` every 100 ms: decoded inputs (engine speed, lever request and source, quality bits,
pedal, brake); layout in `app/main.c`. The same bytes are readable as UDS identifier `0xFD00`.

`0x7F2` every 100 ms: sensor status (speed channels TIF12/TIF13, fluid temperature, battery,
park, quality bits, temperature faults); layout in `app/main.c`, also UDS identifier `0xFD01`.

`0x7F0`/`0x7F1`/`0x7F2` are not cleared for use on a vehicle bus. The firmware is for a bench
controller only.

## Diagnostics

| Request | Response |
|---|---|
| `10 01` / `10 03` | `50 xx 00 32 01 F4` (P2 50 ms, P2* 5 s); extended session falls back after 5 s without requests |
| `10 02` | `7F 10 78`, then reset into the Bosch boot software, which answers (engine must not be running) |
| `11 01` | `51 01`, then reset |
| `22 F1 95` | software version `8HPTCU-OPEN M1` |
| `22 FD 00` | decoded input status (8 bytes, as `0x7F1`) |
| `22 FD 01` | sensor status (8 bytes, as `0x7F2`) |
| `3E 00` | `7E 00` |

## Solenoid bench test (M3) — DUMMY LOADS ONLY

Never on a transmission or a vehicle. Connect a dummy load (for example a 6–10 Ω power resistor,
or a spare solenoid on the bench) and an ammeter to the channel under test; leave every other
output unconnected or loaded.

Safety design (agreed 2026-10-15), enforced in `app/bench_out.c`:
- outputs and the supervisor output enable are off unless a test is running;
- a test starts only in the extended session (`10 03`), engine not running, driver chips
  identified and configured, supervisor replying;
- one channel at a time, channels 0–6 only (park release 7 and park hold excluded), 10–1000 mA,
  1–5 s;
- the test ends on timeout, stop, leaving the extended session (keep `3E 00` coming), engine
  speed above 50 rpm, or any driver / supervisor fault.

| Request | Meaning |
|---|---|
| `10 03` | extended session (send `3E 80` at least every 4 s to stay in it) |
| `31 01 FE 10 ch I s` | start: channel `ch` (0 A, 1 D, 2 B, 3 E, 4 C, 5 TCC, 6 line pressure), current `I` × 10 mA, `s` seconds |
| `31 02 FE 10` | stop |
| `31 03 FE 10` | results: state, channel, mA, remaining 0.1 s, end reason, driver faults |
| `22 FD 02` | output status (as `0x7F3`) |

## User boot MAT read-out (routine FE20)

The 32 KB user boot MAT (power-on start-up code) is not in the ACDP-2 read-out. In the extended
session, `31 01 FE 20` copies it into a RAM buffer (flash MAT switched from RAM-resident code,
interrupts masked, under 1 ms) and replies `status addr(4) len(2) crc32(4)`; the buffer is then read
with the measurement protocol. Bench tool: `tcu_bench.py dump-userboot -o userboot.bin` (checks
the CRC-32). The file is Bosch boot code: keep it with the other read-outs, never in the
repository. Cross-check the reading path first: `tcu_bench.py meas-read 0x0 0x40000 --compare
"<read-out>_IFla.bin"` must report the boot blocks identical.

End reasons: 1 timeout, 2 stopped, 3 session ended, 4 engine running, 5 driver fault,
6 supervisor fault. Driver fault bits (`0x7F3` byte 0): 0x01 bus timeout, 0x02 ID, 0x04
configuration read-back, 0x08 reply header, 0x10 setpoint read-back, 0x20 start-up not done.

First bench steps:
1. With nothing connected, check `0x7F3`: byte 0 = 0 (driver chips identified and configured,
   no faults) and `0x7F0` byte 1 bit 1 = 1 (supervisor replying). If not, stop here and record
   the values: the reply formats in spec 12 §2b are taken from the stock checks and are not yet
   confirmed on hardware.
2. Channel 6 (line pressure) at 300 mA for 2 s into the dummy load; read the ammeter. Expected
   ≈ 300 mA if the 0.1 mA unit and the ÷6 setpoint scaling (spec 12 §2b) are right.
3. Repeat at 100, 500, 1000 mA to check linearity; then each channel 0–6 once.
4. Record channel, requested and measured current (bench item 3 / spec 12 §4).

## Measurement protocol (read-only, spec 10)

Requests on `0x361`, responses on `0x398`, either bus (the first bus used is latched).
Examples:

| Request (`0x361`) | Response (`0x398`) |
|---|---|
| `01 00 00 00 00 00 00 00` | `21 01 '8' 'H' 'P' 'T' 'C' 'U'` (then `21 …` for the next parts) |
| `2A 00 FF F8 00 80 00 00` | `2A 81` + 6 bytes at `0xFFF80080` |
| `E0 81 00 02 00 0D 00 00` | periodic `0x368…0x398` every 10 ms: engine speed (2 bytes), lever |

Project index list: `app/meas_table.c` (index 2 = engine speed, 13 = lever, 15 = quality bits, …).

The same commands work against the **stock** firmware (with the stock index list), which is
how bench item 20 reads the stock boot request record: `2A 00 FF F8 00 80`, `…00 86`, … up to
`0xFFF800C7`.

## Bench checklist (spare controller, never the vehicle's unit)

1. Make and keep a full read-out (flash + EEPROM) of the spare before writing anything.
2. Build the image from **that spare's** read-out (`--base`).
3. Write with the Teensy flasher
   ([8hp-teensy-flasher](https://github.com/alldogsoffroad/8hp-teensy-flasher), spec 17 path;
   historically done with the ACDP-2, module 19). Note whether the tool writes the markers
   itself.
4. Power the controller on the bench (B+, ignition, ground; spec 01 §2.8) with a CAN interface
   on pins 5/6 and 120 Ω termination.
5. Expected: `0x7F0` and `0x7F1` frames every 100 ms, the counter rising and no restarts.
   Send `7E1: 03 22 F1 95` and check the version response.
6. If the counter restarts every ~30–60 ms or a few seconds: watchdog or external supervisor
   reset. The firmware services the external supervisor on RSPI-B (SSLB1, spec 03 §4.4) every
   5 ms: heartbeat byte 1 bit 1 = its last reply was valid, bit 2 = a bad reply was seen. If bit 1
   never comes on, the supervisor link is not working and resets are expected. If nothing is sent: the boot side did not start the application
   (TPROT / marker / CRC question, spec 06 §8); [I] the boot side should then stay in the
   bootloader, but this is unconfirmed.
7. Write the stock read-out back with the Teensy flasher: this exercises the write path
   and confirms the unit recovers. (Historically the `10 02` hand-over + ACDP-2 write served
   as this check, bench item 21.)
