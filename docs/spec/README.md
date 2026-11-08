# Specification — Bosch TCU 1034420267 (SH7254R)

Behavioral and interface specifications derived from the hardware manual and static analysis of
the stock firmware. Markers in each document: **[M]** manual, **[O]** observed in stock firmware,
**[I]** inference, **[D]** derived by calculation.

## Project scope

- **Purpose:** improve the fuel efficiency of older vehicles originally fitted with fewer-speed
  transmissions, by fitting a ZF 8HP transmission controlled by this TCU running project firmware.
- **Primary target vehicles:** 2005–2019 Nissan Frontier and 2005–2015 Nissan Xterra (originally
  equipped with the RE5R05A five-speed automatic). Coverage may expand later.
- **Transmission generations:** the project targets the **Gen 1** (8HP45 / 8HP70 / 8HP90) and
  **Gen 2** (8HP50 / 8HP75 / 8HP95, from 2019 on some Chrysler/Ram/Jeep) ZF 8HP with their
  Renesas SH725x-based TCUs. **Gen 3 is out of scope** (Infineon TriCore TCU). The analysed
  firmware (1034420267) is a **Gen 1** unit (its solenoid channel order matches the Gen 1 valve
  body, spec 12 §1.1); Gen 2 coverage is to be confirmed later with a Gen 2 firmware dump (differences:
  spec 11 §1b). **Current focus: Gen 1 only** (decision 2026-10-14); Gen 2 work is deferred.
- **Status:** these specifications describe the TCU and its stock (Chrysler) environment only.
  Vehicle-side (Nissan) integration has **not started** and will be added when the project is
  ready for it.

## Project conventions

- **Clean-room process: not required (as of 2026-10-14).** Specification and implementation work
  may be done by the same people.
- **Respect for the original work:** calibration tables, limits, curves and other protected work
  product of Bosch and ZF quoted in these documents should be **changed or re-derived** (e.g.
  from sensor datasheets or measurement) before being used in the open-source implementation, to
  protect and respect the original developers.
- **Code-flash and recovery tool — Teensy 4.1: done.** The project set out to make the Teensy
  the only tool needed for code-flash writes and recovery, so that no proprietary hardware is
  required. **Current state (2026-10-30): achieved.** The Teensy reads, erases, programs and
  verifies code flash end-to-end through the boot MAT plant path (spec 17) with the companion
  tool [8hp-teensy-flasher](https://github.com/alldogsoffroad/8hp-teensy-flasher); it also
  updates the project application in data flash (spec 18). The proprietary ACDP-2 (module 19)
  that this work started around is retired from the toolchain — remaining references to it in
  the specs are historical bench records. Chip boot-mode programming (spec 01 §1, which erases
  all flash, the user boot area and the EEPROM) is **not** part of the plan unless a specific
  need arises.

## Documents

| Spec | Topic | Status |
|---|---|---|
| [01](01-hardware-io.md) | Memory map, pin functions, safe start-up state; case-pin measurements; overview diagram [`../diagrams/tcu-hardware.svg`](../diagrams/tcu-hardware.svg) | draft |
| [02](02-timers.md) | ATU-III clock tree, 1 ms tick, PWM groups, speed inputs, timer G triggers | draft |
| [03](03-spi-devices.md) | SPI buses, chip selects, framing, job schedule | draft; chip identities open |
| [04](04-analog-inputs.md) | ADC setup, scaling, temperature sensor, multiplexed AN0, switch inputs | draft |
| [05](05-clock-watchdog-powerup.md) | Clocks, watchdog, power-up order | draft; Pφ 20 MHz and Iφ 120 MHz confirmed on the bench |
| [06](06-boot-application-interface.md) | Flash partitioning, block headers, CRC-32, validity markers, application entry contract | draft; boot selection and protection check open |
| [07](07-can-receive.md) | CAN receive: message objects, CRC-8/counter protection, timing, 241 signal layouts | draft; signal meanings open |
| [08](08-can-transmit.md) | CAN transmit: 8 periodic messages, packers, CRC-8/counter, 83 field layouts | draft; field meanings open |
| [09](09-shift-and-lockup.md) | Calibration access, shift maps and gear decision, driver-type blending, converter lockup | draft; input signals and program meanings open |
| [10](10-measurement-protocol.md) | Measurement protocol on CAN 0x361/0x398: RAM read by address or index, periodic lists, 470-entry index | draft; project implementation in daily bench use, stock behaviour untested |
| [11](11-shift-execution.md) | Shift execution: element matrix A–E, pressure parameter families, phase engine | draft; map roles and solenoid path open |
| [12](12-solenoid-outputs.md) | Solenoid output chain: pressure request → current (curves, temperature correction) → SPI driver channel; channel table (EDS A–E, WK, Sys, MV Pos) | draft; channel table confirmed in code, bench check pending |
| [13](13-fault-reporting.md) | Fault memory: event table, reporting API, fault-group identification, decoded monitors (P1B14) | draft |
| [14](14-park-range.md) | Park/range module: input struct, park-target arbiter, lever request object, redundant safety-layer check | draft; range-code names and several inputs inferred |
| [15](15-diagnostic-services.md) | UDS addressing, service table, sessions; hand-over to the boot software | draft; transport parameters and tool sequence open |
| [16](16-can-reflash.md) | Project CAN reflash: RAM flasher, FCU driver, protocol, power-loss-safe write order | implemented, host-tested; **not usable on this unit**: FWE is low at boot hand-over (§10, bench 2026-10-18); code-flash duty taken over by the plant path (spec 17) and [8hp-teensy-flasher](https://github.com/alldogsoffroad/8hp-teensy-flasher) |
| [17](17-userboot-plant-protocol.md) | User boot MAT plant (production) protocol: "PlantCan" entry at 666.667 kbit/s, UDS services, the plant programming path | **SecurityAccess broken (2026-10-25): RIPEMD-160 digest, RSA-1024 e=3 PKCS#1 v1.5 satisfied by integer-cube forgery — no private key exists; forged key accepted live (`67 62`), RequestDownload positive (`74`, §5i–§5k). Code-flash write proven (2026-10-30): the session engine's encrypted 500 kbit/s session erases/programs/verifies code flash, executed Teensy-only — tool: [8hp-teensy-flasher](https://github.com/alldogsoffroad/8hp-teensy-flasher)** |
| [18](18-dataflash-loader.md) | Data-flash application loader: loader in code flash, RAM application in data flash, Teensy updates (`app-flash`) | in use on the bench; all bench checks passed |
| [19](19-function-catalog.md) | Function catalog: full-firmware decompile sweep — 10,898/10,898 code-bearing functions classified, named, and cataloged; names applied to the Ghidra project | complete (2026-11-08) |
