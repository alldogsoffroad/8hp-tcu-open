# Project firmware

Open C firmware for the Bosch ZF 8HP TCU (SH7254R). Architecture: `docs/firmware/00-architecture.md`.

Current milestone: **M0** — boots in place of the stock application, serves the watchdog and
sends a bench heartbeat on CAN bus 1. It drives **no outputs**: all solenoid, PWM and switch
outputs stay in the safe state set up by `port_init()` (same pin state as the stock firmware
before its drivers start).

## Build

Host build (simulator and unit tests, runs on the PC):

```
cmake -S firmware -B build/host -G Ninja
cmake --build build/host
ctest --test-dir build/host
TCU_SIM_MS=1000 build/host/tcu_sim
```

Target build (needs `sh-elf-gcc` in `~/opt/sh-elf` and Debian's `binutils-sh-elf`):

```
cmake -S firmware -B build/sh2a -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/cmake/toolchain-sh2a.cmake"
cmake --build build/sh2a
python3 tools/build_image.py --app build/sh2a/tcu_app.hex --base "<your read-out>_IFla.bin" -o build/m0_IFla.bin
```

`build_image.py` keeps the boot blocks and the calibration block of your own read-out,
replaces the application block, writes the header CRC and the validity markers, and checks the
header fields that the Bosch boot manager verifies (spec 06).

## M0 heartbeat (bench only)

CAN bus 1 (vehicle connector pins 5/6, CAN-C), 500 kbit/s, identifier `0x7F0`, every 100 ms:

| Byte | Content |
|---|---|
| 0 | rolling counter, restarts at 0 after every reset |
| 1 | bit 0: a 1 ms tick was missed |
| 2–3 | uptime, seconds |
| 4–7 | idle-loop count in the last 100 ms (gives the CPU clock, spec 05 §1.2) |

`0x7F0` is not cleared for use on a vehicle bus. M0 is for a bench controller only.

## M0 bench checklist (spare controller, never the vehicle's unit)

1. Make and keep a full read-out (flash + EEPROM) of the spare before writing anything.
2. Build the image from **that spare's** read-out (`--base`).
3. Write with the ACDP-2 (module 19). Note whether the tool writes the markers itself.
4. Power the controller on the bench (B+, ignition, ground; spec 01 §2.8) with a CAN interface
   on pins 5/6 and 120 Ω termination.
5. Expected: `0x7F0` frames every 100 ms with the counter rising and no restarts.
6. If the counter restarts every ~30–60 ms or a few seconds: watchdog or external supervisor
   (spec 03 §5) reset. If nothing is sent: the boot side did not start the application
   (TPROT / marker / CRC question, spec 06 §8) — the bootloader should then be reachable for
   reprogramming the stock image.
7. Write the stock read-out back afterwards to confirm the unit recovers.
