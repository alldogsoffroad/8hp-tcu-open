# 8hp-tcu-open

Reverse-engineering notes and tools for the Bosch transmission control unit of the ZF 8HP
(FCA / Chrysler / Ram, Gen 1: 8HP45 / 70 / 90), software 1034420267 on a Renesas SH7254R,
working towards an open C reimplementation that runs on the stock controller.

The long-term goal is to use the 8HP behind other engines (first target: 2005–2019 Nissan
Frontier / 2005–2015 Xterra). Nissan-side work has not started.

## Contents

| Path | Content |
|---|---|
| `docs/spec/` | The specification, one file per subsystem (start with `docs/spec/README.md`). This is the project's source of truth. Specs 19–26 are the 100 % function catalog and the top-down syntheses (chronology, supervisor protocol, latch conditions, task inventory, chip ID) that the rebuilt firmware is written against. |
| `firmware/` | **The open reimplementation** — rewritten top-down from the specs (2026-11-09): platform (interrupt-per-frame RSPI engine, the ATU timer fabric, WDT, CAN), a cooperative scheduler, the full supervisor service (the 14-job engine, the walk FSM, the certification ladder, the cell map at the stock's RAM addresses so bench tooling reads it unchanged), bus-A drivers, solenoid control. 20/20 host tests; boots, walks and converses on the stock controller. |
| `docs/reference/` | Reference material that is not part of the specification (e.g. preliminary Nissan CAN notes). |
| `1034420267_*.csv` | Data tables extracted during the analysis (CAN messages and signals, fault groups, DTCs, EEPROM records, measurement list). |
| `1034420267_sh2a.seeds.tsv` | Function entry seeds for the disassembler. |
| `cal_catalog.py` | Lists and dumps calibration objects from a firmware read-out. |
| `eeprom_diff.py` | Reassembles and compares EEPROM blocks. |
| `fix_header_crc.py`, `make_sh7254r_elf.py` | Helpers for building an analysable image from a read-out. |

## What is not in this repository

Firmware, calibration and EEPROM dumps, and anything built from them (ELF images, disassembler
projects), are excluded (`.gitignore`). They are Bosch / FCA work product, and an EEPROM
read-out contains vehicle-specific data. To reproduce the analysis, use a read-out of your own
unit (read it with [8hp-teensy-flasher](https://github.com/alldogsoffroad/8hp-teensy-flasher),
the project's Teensy 4.1 flasher, or any tool you have).

## Related repository

**[8hp-teensy-flasher](https://github.com/alldogsoffroad/8hp-teensy-flasher)** — the Teensy 4.1
code-flash reader/writer built from this project's findings (specs 16–18). Wake, plant
SecurityAccess, encrypted session, erase/program/verify: everything needed to read and write the
TCU with a ~$30 Teensy and no proprietary hardware. The earlier work in these specs was oriented
around the YanHua ACDP-2 diagnostic tool; it is now only of historical interest — every read and
write operation it performed is reproduced (and documented) by the open flasher.

## Conventions

- Facts in the specs are marked **[M]** manual, **[O]** observed in the stock firmware,
  **[I]** inference, **[D]** derived.
- Calibration values are Bosch/ZF work product. Where the specs quote stock values, it is to
  show structure and to validate behaviour; the project firmware must change or re-derive them.
- Code-flash and recovery tool: **Teensy 4.1 — done.** The goal was "no proprietary hardware
  needed"; as of 2026-10-30 the Teensy, running
  [8hp-teensy-flasher](https://github.com/alldogsoffroad/8hp-teensy-flasher), does the full job
  on its own: code-flash read, erase, program, verify and recovery (specs 17–18), plus the
  routine data-flash application updates. The proprietary ACDP-2 tool is no longer needed for
  anything.

## Status

| Area | Complete | Basis |
|---|---|---|
| Stock-firmware reverse engineering (specs 01–26) | **95 %** | every subsystem's structure is mapped; **the full 10,982-function decompile is cataloged (spec 19, 10,898/10,898 classified)**; the top-down synthesis complete (specs 20–26: walk reconciliation, power-on chronology, the 14-job supervisor protocol, rail latch conditions, task/ISR inventory, chip identification); the supervisor's protocol/word semantics bench-measured; the stock runs on the bench for live probing; RTOS question closed at runtime (no ITRON, no AUTOSAR); the supervisor's full driving-time conversation reconstructed, the `0x22` echo/grant law decoded, the boot-mailbox handshake ruled off the drive path; open: CAN signal fine meanings, chip part numbers |
| Code-flash independence — read / write / recover (specs 16–18, [8hp-teensy-flasher](https://github.com/alldogsoffroad/8hp-teensy-flasher)) | **95 %** | Teensy-only write and restore proven on the bench; remaining: an end-to-end shakedown of the released tool suite as packaged |
| Open firmware, bench-proven (milestones M0–M6, [§8](docs/firmware/00-architecture.md)) | **78 %** | **the full top-down rebuild (P0–P7, 2026-11-09/10): the whole firmware rewritten from the validated specs — 20/20 host tests, boots, walks, converses on the bench.** The commissioning walk runs the complete FSM ladder to steady supervision; the bus-B conversation flows at full stock cadence; **the chip answers the 84-class walk words with real replies (rx_level live)**; the certification chain fires end-to-end on the bench arm (letters → the stock's exact `0x9020` engaged pair on the wire); both buses' RSPI interrupts deliver; every timer/interrupt register law decoded from the stock (the U-compare walk tick, the byte idioms, IPR nibble map, SPSCR=selects−1). Remaining to the rail: the level-nibble harvest store (one decoded function — FB_0C frozen), then the one-shot → `0x8E91` → the 0x24 latch verdict; then M5 limp mode, M6 shifting |
| Vehicle integration (M7: Nissan profile, on-car validation) | **0 %** | not started |

**Overall: ~67 %** — the straight average of the four areas. Gen 2 (8HP50/75/95)
coverage is deferred and deliberately excluded from the figure.

How the numbers move: closing a spec's open items (e.g. a signal-meaning table)
moves row 1; tooling polish moves row 2; each completed milestone moves row 3
(M4–M6 are 15 % each); row 4 begins with the first Nissan-side work. Static
analysis alone is well advanced — bench and vehicle validation are what the
percentages now track. *Last updated: 2026-11-10.*

Static analysis of the stock firmware is well advanced (hardware I/O, CAN, measurement protocol,
shift strategy, lockup, shift execution, line pressure, solenoid outputs, park/range, fault
memory). Bench and vehicle validation are pending. Gen 2 (8HP50 / 75 / 95) is in scope but
deferred; Gen 3 (Infineon TriCore TCU) is out of scope.

## License

Copyright 2026 the 8hp-tcu-open authors.

This repository — the specifications, the project firmware, and the tools — is licensed under
the **GNU General Public License v3**; see [LICENSE](LICENSE). Firmware, calibration and EEPROM
dumps remain excluded (see above): they are Bosch / FCA work product and are not covered by
this license because they are not part of this repository.

## Acknowledgements

Findings built on public SH7254R documentation, the reference material listed in
[`docs/spec/README.md`](docs/spec/README.md), and the author's own bench captures. Inspired by
the wider community of open ECU tools; the sibling project
[dspl1236/vag-tcu-tools](https://github.com/dspl1236/vag-tcu-tools) corroborated the platform
family (spec 01 §1).

This project was produced by a human/AI pair: see [CONTRIBUTORS.md](CONTRIBUTORS.md).
