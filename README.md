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
| Open firmware, bench-proven (milestones M0–M6, [§8](docs/firmware/00-architecture.md)) | **78 %** | Scored by the rule below: **M0–M3 complete and bench-proven (55 %)** + **M4 partial (12 % of its 15 %)** + **M5 partial (10 % of its 15 %)** + **M6 started (1 % of its 15 %: the stock shift-map runs as a read-only shadow on the target, r04t)**. **M4 (2026-11-18):** the stock's own cyclic range chain owns the range live (r03t) with its brake/engine/lever interlocks and the U1267/P0850 fault debounce, holding P on this bench and releasing it with simulated brake + engine (13/13 both ways); key-off auto-P (the terminal-15 object); both park outputs at the stock values with a 30 s release limit; the stock PA7 Park signal driven live (r04e); the park position sensor ported read-only. Missing: pawl engagement verified on a transmission, the sensor wired into range control. **M5 (2026-11-18):** the stock request producer, holder table and element fill machinery drive the live P-state currents (r03n–r03u, fill bench-traced pass for pass); the garage-shift engagement runs as a read-only shadow matching the decoded stock (completion 0.93–0.94 s on the dry bench, r04a); the stock output diagnosis runs on the real measured solenoid feedback with no fault (r04f); the bus-A output cadence is the stock's (r03z). **2026-11-18 (r04i): the first gear-selection engagements under open firmware** — lever D and R engage 1st and reverse on the real B/C/D coils through a guarded bridge from the ported stock engagement engine (boost, fill, lock hold, ramp; completion ≈ 0.94 s), the stock's own feedback chain measuring the coils follow (B 36 → 892 mA, C 939 → 47 mA), with no diagnosis reaction. **2026-11-19 (r04k–r04p): the stock structure.** The stock's 1.2 s garage release runs on the real coils (r04l); the stock engagement engine is the live producer of the A–E currents (G6, r04o), the bridge retired to a latching independent monitor of ours, and the engine re-pinned at the stock RAM addresses (r04p), the stock boot pass with its one-pass all-release (r04q), and the old holder retired so the stock engine is the only producer of the A–E/WK/Sys requests (r04r) — bench: engagements 0.95 s cause 1, releases 120 passes, no monitor trip; the selected ECU temperature, the diagnosis current bounds and the temperature fault flags are live (r04j, r04m). **2026-11-19 (r04s–r04v):** every stock engine object at its stock address with 0xDC0F8 and 0x106698 ported (r04s); OS mode 2 chosen by the stock selector, latching until key-off (r04u, key-off check 8/8); the engine's mode-1 commit gate live (r04v). Missing: the gear target (forced 1st — the stock picks 3rd on this bench through a CAN-fault reaction), the engine-stall transient (fenced), line pressure and WK in drive, the fault reactions, and any test on a transmission. **M3 closed 2026-11-13 (r02w–r03f):** the stock supervisor self-test ladder runs to completion, the permanent letter 'c' and the `0x9020` engaged word hold the rail, the OS switches to mode 1; the stock P-state request set drives **1.775 A — the stock's own 1.773 A** — on a real mechatronic. Next: the fault-reaction / gear-code chain (the honest gear target), the M6 gear feedback chain |
| Vehicle integration (M7: Nissan profile, on-car validation) | **0 %** | not started |

**Overall: ~67 %** — the straight average of the four areas. Gen 2 (8HP50/75/95)
coverage is deferred and deliberately excluded from the figure.

How the numbers move: closing a spec's open items (e.g. a signal-meaning table)
moves row 1; tooling polish moves row 2; each completed milestone moves row 3
(M4–M6 are 15 % each); row 4 begins with the first Nissan-side work. Static
analysis alone is well advanced — bench and vehicle validation are what the
percentages now track. *Last updated: 2026-11-18.*

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
