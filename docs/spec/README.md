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
- **Reading and writing the controller:** done with an **ACDP-2 with module 19**, which reads and
  writes the flash and EEPROM images without opening the module. Chip boot-mode programming
  (spec 01 §1, which erases all flash, the user boot area and the EEPROM) is **not** part of the
  plan unless a specific need arises.

## Documents

| Spec | Topic | Status |
|---|---|---|
| [01](01-hardware-io.md) | Memory map, pin functions, safe start-up state | draft |
| [02](02-timers.md) | ATU-III clock tree, 1 ms tick, PWM groups, speed inputs, timer G triggers | draft |
| [03](03-spi-devices.md) | SPI buses, chip selects, framing, job schedule | draft; chip identities open |
| [04](04-analog-inputs.md) | ADC setup, scaling, temperature sensor, multiplexed AN0, switch inputs | draft |
| [05](05-clock-watchdog-powerup.md) | Clocks, watchdog, power-up order | draft; CPU clock open |
| [06](06-boot-application-interface.md) | Flash partitioning, block headers, CRC-32, validity markers, application entry contract | draft; boot selection and protection check open |
| [07](07-can-receive.md) | CAN receive: message objects, CRC-8/counter protection, timing, 241 signal layouts | draft; signal meanings open |
| [08](08-can-transmit.md) | CAN transmit: 8 periodic messages, packers, CRC-8/counter, 83 field layouts | draft; field meanings open |
| [09](09-shift-and-lockup.md) | Calibration access, shift maps and gear decision, driver-type blending, converter lockup | draft; input signals and program meanings open |
| [10](10-measurement-protocol.md) | Measurement protocol on CAN 0x361/0x398: RAM read by address or index, periodic lists, 470-entry index | draft; untested on hardware |
| [11](11-shift-execution.md) | Shift execution: element matrix A–E, pressure parameter families, phase engine | draft; map roles and solenoid path open |
| [12](12-solenoid-outputs.md) | Solenoid output chain: pressure request → current (curves, temperature correction) → SPI driver channel; channel table (EDS A–E, WK, Sys, MV Pos) | draft; channel table confirmed in code, bench check pending |
| [13](13-fault-reporting.md) | Fault memory: event table, reporting API, fault-group identification, decoded monitors (P1B14) | draft |
| [14](14-park-range.md) | Park/range module: input struct, park-target arbiter, lever request object, redundant safety-layer check | draft; range-code names and several inputs inferred |
| [15](15-diagnostic-services.md) | UDS addressing, service table, sessions; hand-over to the boot software | draft; transport parameters and tool sequence open |

Related project data (project root): CAN message list (`1034420267_can_messages.csv`), fault-code
list with status (`1034420267_dtc_list.csv`), EEPROM block map (`1034420267_eeprom_records.csv`).
Calibration tables are read from the user's own dump with `cal_catalog.py` (spec 09 §1).
Measurement index list with identified signals: `1034420267_measurements.csv` (spec 10 §5).

## Static analysis: complete for now

Items that static analysis could not resolve, to be answered on the bench:

| # | Question | Spec | Suggested measurement |
|---|---|---|---|
| 1 | CPU clock (80/120/160/200 MHz) | 05 | bring-up firmware times a loop against a 20 MHz timer |
| 2 | Identity and register meaning of the 8 SPI devices | 03 | capture SPI-A/SPI-B traffic on a running stock unit during shifts |
| 3 | Confirm the solenoid channel table (spec 12 §1: ch 0 A, 1 D, 2 B, 3 E, 4 C, 5 WK, 6 Sys, 7 MV Pos; supported by FCA documentation, low risk) | 03, 11, 12 | correlate SPI setpoint writes (`0xE8`/`0xF0` per chip) with solenoid currents |
| 4 | Roles of PWM groups A and B and the 11 unassigned TOE outputs | 02 | probe TOE pins on a running unit |
| 5 | Meaning of the 16 PWM-input (TIF) channels | 02 | probe TIF pins; correlate with driver/sensor activity |
| 6 | Turbine vs output speed (TIF12/TIF13); OSS pulses per revolution | 02, 01 | drive each input with a signal generator; count parking-gear teeth (OSS target, spec 01 §2.8) |
| 7 | Signals on the 16 AN0 multiplexer inputs | 04 | apply known voltages to module inputs; watch sequence slots |
| 8 | Meaning of the other analog channels | 04 | same as 7 for direct AN inputs |
| 9 | Physical switches on AN5/AN6 (and AN3/AN4 variant) | 04 | toggle candidate switches, observe |
| 10 | External supervisor protocol on RSPI-B (SSLB1) and its timeout | 03, 05 | capture SSLB1 traffic; stop servicing on a spare and observe reset |
| 11 | Discrete port A/B/C signals | 01 | probe with stock firmware running |
| 12 | Does a modified application start? (protection check, CRC, markers) | 06 | on a spare: change one non-functional byte in the application, fix the CRC, write with the ACDP-2, observe start-up |
| 13 | Does the ACDP-2 recalculate CRCs / write markers? | 06 | ACDP-2 documentation; compare a read-back after writing a modified image |
| 14 | EEPROM reaction to a changed application/calibration CRC | 06 | read the EEPROM before and after item 12; compare with `eeprom_diff.py` |
| 15 | Meaning and scaling of the CAN signals (torque, pedal, speeds) | 07, 08, 09 | full CAN-C capture from a stock Ram (below); also log the dedicated shifter bus (TCM pins 3/4) |
| 16 | Confirm **park hold = TOE31 (PL6)** (safety-relevant) and identify TOE30 / TOE32 (one likely the TRS Park output, pin 15) | 01, 02, 12 | measure TOE30–32, internal terminal C7 (park hold return) and case pin 15 with engine running out of P vs in P |
| 19 | Function of case pins 1, 2, 7, 8, 10, 11, 12, 16 (wired, function unknown) | 01 | trace from internal A terminals (spec 01 §2.9) to MCU pins; candidates include the TOE30/TOE32 outputs |
| 17 | Park position sensor inputs (Park / Not Park) | 01, 04 | toggle park state; watch AN5/AN6 and port inputs |
| 18 | Which TCM connector pin pair is bus 1 / bus 2 | 01, 07 | send frames on pins 3/4 and 5/6, see which RCAN channel receives |
| 20 | Stock hand-over to the boot software: the ACDP-2 request sequence and the record contents | 06, 15 | with the **stock** application: log both buses while the ACDP-2 starts a write; before that, read RAM `0xFFF80080–0xFFF800C7` with the measurement protocol (spec 10) in the default and extended sessions |
| 21 | Project hand-over works | 06, 15 | on the spare running the project application: the ACDP-2 must reach the bootloader and be able to write the stock image back |
| 22 | Fluid temperature conversion: AN8/AN1 against a reference thermometer at 3+ temperatures (room, hot water, oven/oil bath) | 04 | read AN8/AN1 mV via the measurement protocol (project firmware indices 27/28); replace the provisional table |
| 23 | Supply and switched-supply channels (AN21, AN19, AN20) | 04 | vary B+ and ignition on the bench; watch `0x7F2` byte 5 / measurement index 24 |

**Ram capture plan (item 15).** 2014–2017 Ram 1500 (DS) has no security gateway: OBD pins 6/14
reach CAN-C (500 kbit/s) directly, so passive logging works; from model year 2018 a gateway is
fitted and needs a 12+8 bypass cable or a direct tap. Log the whole bus, unfiltered, in labelled
segments: key-on, idle, throttle sweep in Park, drive with coast-down, shifts. In parallel, poll
standard OBD Mode 01 PIDs on `0x7E0` (replies `0x7E8`): `0x0C` engine speed = (A·256 + B)/4,
`0x61` driver-demand torque % = A − 125, `0x62` actual torque % = A − 125, `0x63` reference
torque Nm = A·256 + B. These label the torque fields in `0x037`/`0x2F9`. The measurement
protocol of spec 10 can log internal TCU values during the same drive.

Reference documentation used (user's copies, facts only): ZF 8HP70 repair manual; FCA 8-speed
Diagnosis and Repair student workbook (2015); ATRA ZF8HP Introduction webinar (2015); community "ZF 8HP Inside Mechatronics Pinout"
diagram (no author given; treated as lower confidence); technician video "P1B14, Unintended Park
Position, ZF8 Series Transmission" (Gary Ferraro, AA-1 Transmissions; field report); Sonnax
"Chrysler ZF8 (Gen. 1 & 2) Identification Guide" (2023).

Nissan-side reference data from an earlier preliminary session (not active work):
`docs/reference/nissan-frontier-can.md`.
