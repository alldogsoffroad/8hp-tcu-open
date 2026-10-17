# Spec 01 — Hardware I/O map (SH7254R, Bosch TCU 1034420267)

Status: **draft, phase 1 of the I/O map** (pin functions, peripheral usage, startup state).
Audience: implementers of the open-source firmware. This document describes the hardware
interface and observed behavior; it does not reproduce the original code.

Sources: Renesas SH7254R Group hardware manual Rev. 4.00 (R01UH0480EJ0400), and static analysis of
the stock flash image (2.5 MB, sha256 `b8a9a9b4…619d`). Each fact is marked
**[M]** (from the manual), **[O]** (observed in the stock firmware) or **[I]** (inference).

## 1. Device and memory map

| Region | Address range | Size | Notes |
|---|---|---|---|
| Code flash (user MAT) | `0x0000_0000–0x0027_FFFF` | 2.5 MB | [M] SH7254R 2.5 MB variant |
| User boot MAT | `0x0000_0000–0x0000_7FFF` when started in user boot mode | 32 KB | [M] separate array; **not included in the available flash dump** [O] |
| Data flash (EEPROM) | `0x8010_0000–0x8011_FFFF` | 128 KB | [M]; 8 KB erase blocks [M] |
| On-chip RAM | `0xFFF8_0000–0xFFF9_FFFF` | 128 KB | [M] |
| Peripheral registers | `0xFFFC_0000–0xFFFF_FFFF` | 256 KB | [M] |

CPU: SH-2A, big-endian, with FPU [M].

Programming: the project reads and writes the controller with an **ACDP-2 (module 19)**, which
works with the stock Bosch software already on the chip (see the README). For reference only, the
chip's own boot mode runs over SCI channel A (`PJ5/TxD_A`, `PJ6/RxD_A`), selected by MD4–MD0 and
FWE; a matching key code causes the chip to **erase the user MAT, user boot MAT and data MAT**
before programming is accepted [M §25.5]. Boot mode is not part of the plan.

## 2. Pin function assignment

Pin functions as configured by the stock firmware's main application after its port initialisation
[O]. The sequence is: (1) every port control register cleared to general I/O with safe levels,
(2) peripheral functions selected. Pins not listed remain general-purpose I/O.

### 2.1 Timer (ATU-III) outputs — 22 pins, all `TOExx` (timer E PWM outputs)

| Pin | Function | | Pin | Function |
|---|---|---|---|---|
| PE6 | TOE00 | | PL2 | TOE21 |
| PE7 | TOE01 | | PL3 | TOE22 |
| PE8 | TOE02 | | PL4 | TOE23 |
| PE9 | TOE03 | | PL5 | TOE30 |
| PE10 | TOE10 | | PL6 | TOE31 |
| PE11 | TOE11 | | PL7 | TOE32 |
| PE12 | TOE12 | | PL8 | TOE33 |
| PE13 | TOE13 | | PD5 | TOE40 |
| PD3 | TOE53 | | PD6 | TOE41 |
| PD10 | TOE50 | | PD7 | TOE42 |
| PD11 | TOE51 | | PD9 | TOE43 |

[I] Likely PWM / enable lines to external solenoid driver ICs (see §3). Channel-to-solenoid
assignment and frequencies are **open** (spec 02).

### 2.2 Timer (ATU-III) inputs — 20 pins, `TIFxx` (timer F pulse/period measurement)

| Pins | Functions |
|---|---|
| PH0, PH1, PH2 | TIF0A, TIF1A, TIF2A |
| PH3, PH4, PH5 | TIF3, TIF4, TIF5 |
| PF0–PF7 | TIF6–TIF13 |
| PF8–PF13 | TIF14–TIF19 |

[I] Speed sensors and other frequency/duty inputs. Assignment of each input to a physical sensor
is **open**.

### 2.3 Timer (ATU-III) capture/compare I/O — `TIOCxx`

| Pin | Function | Note |
|---|---|---|
| PD0 | TIOC00 | [O] switched between GPIO and TIOC00 at run time |
| PD1 | TIOC01 | |
| PD2 | TIOC02 | |
| PD4 | TIOC10 | [O] `PDIOR` bit 4 set as output before function select |
| PD8 | TIOC20 | [O] switched between GPIO and TIOC20 at run time |

### 2.4 Serial peripheral interface (RSPI) — 3 channels, 12 chip selects

| Channel | Clock | MOSI | MISO | Chip selects |
|---|---|---|---|---|
| RSPI-A | PK0 RSPCKA | PK1 MOSIA | PK2 MISOA | PG0–PG5: SSLA0–SSLA5 |
| RSPI-B | PK3 RSPCKB | PK4 MOSIB | PK5 MISOB | PG6–PG8: SSLB0–SSLB2 |
| RSPI-C | PK9 RSPCKC | PK10 MOSIC | PK11 MISOC | PG9–PG11: SSLC0–SSLC2 |

### 2.5 CAN (RCAN-TL1)

| Pins | Function |
|---|---|
| PJ0 / PJ1 | CTx_A & CTx_B / CRx_A & CRx_B — **channels A and B share one physical bus** [M][O] |
| PJ2 / PJ3 | CTx_C / CRx_C — second, separate bus |

Correction to earlier notes: CAN "channel B" is not a separate bus. The stock firmware uses
channel B's mailboxes as extra receive capacity on the channel-A bus (it transmits nothing on B).
The message list in `1034420267_can_messages.csv` should be read as two physical buses:
**bus 1 = channels A+B, bus 2 = channel C**.

### 2.6 Serial (SCI)

| Pins | Function | Note |
|---|---|---|
| PJ5 / PJ6 | TxD_A / RxD_A | Also the boot-mode programming port [M] |
| PJ8 / PJ9 | TxD_B / RxD_B | [I] purpose open |

### 2.7 General-purpose I/O

Ports A, B and C are left as general I/O (all inputs at initialisation) [O]; several functions
later switch individual port A and C pins to outputs at run time. Their signal meanings are
**open**.

Port G pins not assigned to RSPI (PG12–PG15) remain general-purpose outputs. [I] They are the
address lines of an external analog multiplexer feeding AN0, written by DMA on every timer G2
compare match (see spec 04 §4.2).

### 2.8 Vehicle connector and sensors [M]

Source: FCA *8-Speed Automatic Transmission Diagnosis and Repair* student workbook (2015), user's copy (wiring diagram for a DS-type vehicle).
TCM connector: 16-pin round.

| Pin | Function | Circuit |
|---|---|---|
| 3 | dedicated shifter CAN (−) ("eShift" bus to the electronic shifter module) | D440 |
| 4 | dedicated shifter CAN (+) | D441 |
| 5 | CAN-C (+) | D29 |
| 6 | CAN-C (−) | D30 |
| 9 | ignition run/start input | F1 |
| 13 | battery (fused B+) | A190 |
| 14 | ground | Z908 |
| 15 | **TRS Park signal output** (hard-wired, to another module) | T824 |

**Bench measurement (2026-10-17, spare unit, unpowered):** 27.68 kΩ between pins 5–6 and
27.68 kΩ between pins 3–4, i.e. **no internal CAN termination** on either bus; the value is the
transceivers' unpowered input resistance. On the bench each bus needs 120 Ω at the TCU end as
well as at the tester end.

Pins 1, 2, 7, 8, 10, 11, 12, 16 are not shown in that document. Mapping to MCU pins: bus 1 /
bus 2 assignment of the two CAN pairs is in spec 07 §1; the TRS Park output driver is open.

Sensors inside the mechatronic: input-speed (turbine) sensor ISS (Hall, on the input-side
carrier), output-speed sensor OSS (Hall), oil temperature thermistor, park position sensor (two
Hall signals "Park" / "Not Park" plus sensor ground). ISS and OSS share supply and return lines
[M]; [I] two-wire current-output sensors. ATRA *ZF8HP Introduction* webinar (2015), user's copy [M]: ISS (Audi G182) reads an encoder
wheel with a magnetic ring; OSS (Audi G195) reads the **extended teeth on the parking gear**;
both are called "intelligent" Hall sensors. [I] OSS pulses per revolution = parking-gear tooth
count (spec 02 assumes 60; README item 6). The TCM also has an internal temperature sensor. No
selector switches are wired to the TCM: the range comes over CAN.

### 2.9 Internal mechatronic terminals [M]

Source: community diagram "ZF 8HP Inside Mechatronics Pinout" (user's copy, no author/source given). Three terminal rows on the controller connect it to the case connector (A), the
solenoid lead frame (B) and the sensors / valve 9 (C).

**A row → 16-pin case connector** (all 16 case pins are wired):

| Terminal | A1 | A2 | A3 | A4 | A5 | A6 | A7 | A8 | A9 | A10 | A11 | A12, A13 | A14, A15 | A16 | A17 | A18 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Case pin | 10 | 8 | 3 | 4 | 1 | 5 | 6 | 2 | 12 | 7 | 9 | 14 (ground) | 13 (battery) | 16 | 15 | 11 |

Case pins with known function (§2.8): 3, 4, 5, 6, 9, 13, 14, 15. Pins 1, 2, 7, 8, 10, 11, 12, 16
are wired but their function is **open**.

**B row → 8 solenoids on the lead frame:** B2 … B9 = "Valve 8 … Valve 1, on/off signal" (one
return line per solenoid); B10 – B13 = "Valves – 12 V" (shared supply); B1 not connected.
Solenoid order on B: spec 12 §1.3.

**C row → sensors and valve 9:**

| Terminal | Function |
|---|---|
| C2 | output speed sensor – signal |
| C3 | input speed sensor – signal |
| C4 | input and output speed sensors – shared ground |
| C5 | parking lock position sender |
| C7 | "valve 9" – ground return ([I] park hold solenoid, spec 12 §1.3) |
| C12 | temperature sensor – signal |
| C13 | temperature sensor – ground |
| C14 | "valve 9" – 12 V |
| C1, C6, C8–C11, C15–C18 | not connected |

- Each speed sensor has only a signal line plus the shared ground: [I] two-wire current-output
  Hall sensors (supply and signal on one wire), consistent with §2.8.
- The diagram shows **one** park-position line (C5), whereas FCA describes two Hall signals
  ("Park" / "Not Park"). To check on the bench (README item 17).

## 3. SPI traffic architecture

- A-DMAC channels are hard-wired to the RSPI data registers [M Table 12.11]:
  56/57 = RSPI-A receive/transmit, 58/59 = RSPI-B, 60/61 = RSPI-C.
- The stock firmware programs these channels and moves SPI frames between RAM buffers and the
  RSPI units by DMA [O]; the CPU writes the RSPI data register directly only on RSPI-B.
- RSPI-B is also initialised and used by all three boot-stage software blocks [O].
  [I] RSPI-B most likely connects to a device that must be serviced from power-on (supervisor /
  watchdog / system-basis chip).
- [I] RSPI-A (6 chip selects) and RSPI-C (3 chip selects) are the most likely buses for the
  solenoid driver ICs. Device identification is **open** (spec 03: decode the frame builders).

## 4. Safe startup state (behavior to reproduce)

Before any peripheral function is selected, the stock firmware sets [O]:

| Port | Direction (`PxIOR`) | Level (`PxDR`) | Meaning |
|---|---|---|---|
| G | all outputs | all high | SPI chip selects inactive |
| J | outputs on PJ0, PJ2, PJ5, PJ8 | high | CAN TX recessive, SCI TX idle |
| K | outputs on PK0, PK1, PK3, PK4, PK9, PK10 | high | SPI clock/MOSI idle |
| L | outputs on PL2–PL8 | low | future PWM outputs off |
| D | output on PD4 | low | |
| A, B, C, E, F, H | inputs | — | |

Additional setting: `PFPSR` = `0x8000`, `PJPSR` = `0x024A`, `PKPSR` = `0x0824` (pin state
selection; see manual §22 for bit meanings).

The stock firmware also **periodically re-reads** the port F and port G control registers and
checks them against the expected values [O] — a run-time configuration-integrity check. A
reimplementation should include an equivalent check.

## 5. Peripheral usage summary (stock main application)

Counts of code references to each peripheral's registers [O]:

| Peripheral | References | Functions | Use |
|---|---|---|---|
| ATU-III timers | 746 | 189 | PWM outputs, input capture, timebase |
| PFC / I/O ports | 312 | 43 | pin setup, discrete I/O |
| A-DMAC | 149 | 19 | SPI (and possibly SCI) transfers |
| INTC | 135 | 38 | interrupt configuration |
| ROM / FCU | 169 | 41 | flash and EEPROM programming |
| RCAN-TL1 | 128 | 31 | CAN |
| RSPI | 86 | 13 | SPI |
| WDT | 81 | 16 | internal watchdog |
| ADC | 65 | 10 | analog inputs |
| CMT | 23 | 6 | compare-match timers |

## 6. Open items (next specs)

1. **Spec 02 — timers:** ATU-III configuration: clock sources, PWM periods and duty-cycle update
   paths for each TOE output; timer F measurement modes for each TIF input.
2. **Spec 03 — SPI devices:** decode the SPI frame builders to identify each chip on RSPI-A/B/C,
   its command set as used, and the polling schedule.
3. **Spec 04 — analog inputs:** ADC channels in use, sampling, and conversion to engineering
   units (pressure, temperature, supply voltage).
4. **Spec 05 — discrete I/O:** meanings of the port A/B/C pins.
5. Clock and power-up: CPG settings, watchdog configuration and service timing.
