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
| Open firmware, bench-proven (milestones M0–M6, [§8](docs/firmware/00-architecture.md)) | **83 %** | Scored by the rule below: **M0–M3 complete (55 %)** + **M4 12 %** + **M5 10 %** + **M6 6 %** (of 15 % each). **M3 (closed 2026-11-13):** the supervisor certifies, the stock self-test ladder runs to completion, the engaged rail holds, and the stock P-state currents (1.775 A, the stock's own 1.773 A) are driven on a real mechatronic. **M4:** the stock cyclic range chain owns the range live (brake/engine/lever interlocks, U1267/P0850 debounce), both park outputs at stock values with a 30 s release limit, key-off auto-P, the PA7 Park signal and C9 bit 7 live; the park sensor read-only. Missing: pawl engagement and sensor polarity on an assembled transmission. **M5:** the stock garage-engagement engine is the sole producer of the solenoid requests, at the stock RAM addresses, behind a latching monitor of ours — D and R engage and release on the real coils exactly as the decoded stock (completion ≈ 0.95 s, releases 1.2 s); the stock OS-mode selection and commit gate live; output diagnosis on the real feedback chain. Missing: line pressure and WK in drive (stood in), the honest gear target (forced 1st — on this bench the stock itself is in CAN-fault limp), a transmission test. **M6:** every stock function needed to shift runs on the target as a verified read-only shadow — the shift map, the gear chain, the fault/gear-code chain over all 195 DSM groups (its masks equal the stock dump's), the complete gear-pair shift engine with its phase bodies, healthy line pressure, torque intervention with its amounts, WK lock-up with its request producer; **a bench-only simulated-speed input ran a full 1→8→1 sequence (14 shifts) on the target with the live outputs provably unchanged (r05d–r05f)**. Missing: any live shift (needs real vehicle inputs and turning shafts), the WK closed hold and a few counted stand-ins. Full record: [docs/lab_record.md](docs/lab_record.md); the fork-point checklist: [docs/fca_finish_line.md](docs/fca_finish_line.md) |
| Vehicle integration (M7: Nissan profile, on-car validation) | **20 %** | The Nissan version is a **second vehicle profile in the same codebase** (`TCU_VEHICLE=NISSAN`; the FCA build stays byte-identical; not flashable yet). **2026-11-23:** the bench Nissan ECM's diagnostics found on CAN and its 1 MB ROM (SH7058) dumped and decoded: it needs **0x174 / 0x176 / 0x177 (10 ms) and 0x560 (100 ms)** from the transmission controller (U1001 / U0101 on loss) and applies the 0x177 torque cut as spark retard. The profile reads real engine torque (actual / demand), the checked pedal, real timeouts and counters; **the TCU's four-frame transmit set is accepted by the real ECM on the bench with no transmission fault**, driven by the true 8HP gears and the 8HP torque-intervention amounts. Decision D-N5: the ECM is reflashed to accept 8 gears (separate project, [8hp-d40-ecm-flash](https://github.com/alldogsoffroad/8hp-d40-ecm-flash)). Missing: the fault chain bound to the Nissan messages, line pressure live, the lever (D-N1), the ECM 8-gear patch, on-car validation. Spec: [nissan-ecm-tcm-interface.md](docs/reference/nissan-ecm-tcm-interface.md); DBC: [NissanDBCFilez](https://github.com/alldogsoffroad/NissanDBCFilez) |

**Overall: ~73 %** — the straight average of the four areas. Gen 2 (8HP50/75/95)
coverage is deferred and deliberately excluded from the figure.

How the numbers move: closing a spec's open items (e.g. a signal-meaning table)
moves row 1; tooling polish moves row 2; each completed milestone moves row 3
(M4–M6 are 15 % each); row 4 begins with the first Nissan-side work. Static
analysis alone is well advanced — bench and vehicle validation are what the
percentages now track. *Last updated: 2026-11-20.*

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
