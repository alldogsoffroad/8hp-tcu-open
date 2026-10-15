# Spec 12 — Solenoid output chain (pressure request → SPI driver channel)

Status: **draft.** The chain from the application's per-actuator current to the SPI driver chips
is established [O]. The assignment of solenoids to channels is supported by two independent
static sources **and by FCA documentation** (§1); bench confirmation is still required before
outputs are driven (README item 3). The park hold solenoid is driven by a separate on/off
output path: **channel 11 → timer output TOE31 (pin PL6)** (§1.2).
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference, **[D]** derived.

## 1. Result: channel assignment [I][M]

| Driver channel | SPI chip / setpoint | Solenoid (spec 11 §1a) | Fault group / DTCs (`1034420267_dtc_list.csv`) |
|---|---|---|---|
| 0 | chip 0, `0xE8` | **EDS A** | 1–2: P0972–P0974, P0750 (shift solenoid A) |
| 1 | chip 0, `0xF0` | **EDS D** | 3–4: P0981–P0983 (shift solenoid D) |
| 2 | chip 1, `0xE8` | **EDS B** | 5–6: P0975–P0977, P0755 (shift solenoid B) |
| 3 | chip 1, `0xF0` | **EDS E** | 7–8: P0984–P0986 (shift solenoid E) |
| 4 | chip 2, `0xE8` | **EDS C** | 9–10: P0978–P0980 (shift solenoid C) |
| 5 | chip 2, `0xF0` | **EDS WK** (converter clutch) | 11–12: P2761–P2764 (TCC pressure control) |
| 6 | chip 3, `0xE8` | **EDS Sys** (line pressure) | 13–14: P0960–P0963 (pressure control solenoid A) |
| 7 | chip 3, `0xF0` | **MV Pos** (park-lock position) | 15–16: P1D77–P1D79 (manufacturer-specific) |
| 8, 9 | chip 4 | not used | — |

Chip numbering follows the order of the frames in the cyclic SPI job (spec 03 §4.1: chip
*n* = frame index mod 5; chip 0 = first chip in the order SSLA3, SSLA2, SSLA1, SSLA0, SSLA5).

Evidence:

1. **Channel enable mask** (calibration `0x19154C` = `0x00FF`): channels 0–7 are used,
   matching the 7 + 1 solenoids of the manual [O].
2. **Object layout**: the application's actuator objects for indices 0–4 are 0x28-byte objects
   placed in memory in the order 0, 2, 4, 1, 3 (`0xFFF8B154 … 0xFFF8B1F4`, lookup `0xDA032`),
   followed by 0x10-byte objects for 5, 6, 7 (and unused 10, 11) [O]. [I] If the five element
   objects are declared in element order A–E, then A = 0, B = 2, C = 4, D = 1, E = 3.
3. **Fault groups** are numbered per channel in the same order, and their SAE codes name
   solenoids A, D, B, E, C, then converter clutch, then pressure control (line pressure), then
   a manufacturer-specific group [O]. [I] This assumes the SAE solenoid letters follow the ZF
   element letters, which is FCA practice for this transmission but is not shown in the
   firmware.

4. **FCA documentation** [M] (FCA *8-Speed Automatic Transmission Diagnosis and Repair* student workbook (2015), user's copy): the TCM output diagram (Fig. 47) lists
   the solenoid outputs in the order SOL-A, SOL-D, SOL-B, SOL-E, SOL-C, TCC, Line Pressure,
   Park Release, which is exactly channels 0–7. FCA names the solenoids "A–E Clutch Pressure
   Control Solenoid", so FCA's letters are ZF's element letters (assumption in item 3 holds).
   The solenoid lead-frame photo (Fig. 51) shows the same physical sequence.

5. **Sonnax Gen 1 valve-body photo** [M] (Sonnax *Chrysler ZF8 (Gen. 1 & 2) Identification Guide* (J. Dial, 2023), Fig. 6): solenoids left to right
   A (NL), D (NH), B (NL), E (NH), C (NH), TCC (NL), LP (NH), park release; park hold separate.
   Same order and normally-low/high types as channels 0–7. (Gen 2 swaps B and D, spec 11 §1b;
   this firmware is Gen 1.)

6. **Sonnax solenoid identification guide** [M] (Sonnax *Chrysler 845RE; ZF8HP45, ZF8HP55,
   ZF8HP70 Solenoid Identification Guide*, 2022): valve-body positions A–H hold Park, LP, TCC,
   C, E, B, D, A, i.e. the Fig. 6 order read from the other end. The valve body carries
   **8 solenoids**; the park hold magnet is not among them. Chrysler 8HP45/70 and 845RE share
   the layout. Equivalent names:

   | Solenoid | BMW | Audi (8HP55) |
   |---|---|---|
   | A | EDS-1 | N215 |
   | TCC | EDS-2 | N371 |
   | B | EDS-3 | N216 |
   | E | EDS-4 | N233 |
   | C | EDS-5 | N217 |
   | D | EDS-6 | N218 |
   | Line pressure | EDS-7 | N443 |
   | Park | MV-1 | SV-1 (N88) |

   **E-Shift vs M-Shift:** on shift-by-wire ("E-Shift", the Chrysler version) valve bodies the
   park solenoid controls the position valve and the park-lock valve; on mechanically shifted
   ("M-Shift") valve bodies its output goes to a closed passage and it has no function.

7. **Firmware fault groups** [O] (spec 13 §2a): the electrical monitor `0x7B222` assigns
   channels 0–7 to the DTC families of shift solenoids A, D, B, E, C, TCC, line pressure and
   park release, in that order.

All sources give the same order. **Bench confirmation** (README item 3) remains necessary
before any output is driven; the risk of a wrong assignment is now low.

**MV naming:** field and technician literature refers to the two park on/off solenoids as
**MV2 = park release** (ch 7) and **MV3 = park hold** (ch 11 → TOE31). This identification is
the user's, from the P1B14 technician video together with the Sonnax photo (the photo itself
labels them "Park Release" and "Park Hold"). MV numbering is not universal: BMW calls the park
release solenoid **MV-1** (source item 6), so match "MV" numbers by function, not by number.

### 1.1 Solenoid types and park actuators [M]

| Solenoid | Channel | Type (FCA) | De-energised |
|---|---|---|---|
| A, B | 0, 2 | variable force, PWM-controlled | **normally closed** (no pressure) |
| C, D, E | 4, 1, 3 | variable force | **normally open** (pressure) |
| TCC (WK) | 5 | variable force | normally closed |
| Line pressure (Sys) | 6 | variable force | normally open (high line pressure) |
| Park release (MV Pos) | 7 | on/off | normally closed per FCA; energised = pressure to the park release cylinder (park disengaged). The ATRA webinar calls it "normally open"; FCA is taken as authoritative, confirm on the bench |
| **Park hold** | **11 → TOE31 (PL6)** (§1.2) | on/off electromagnet, own actuator-supply branch | when energised, latches the park release piston so the pawl cannot engage |

- The park hold solenoid is energised whenever the engine runs and the selector is not in P,
  and stays energised with the engine off in N ("car wash" function) [M]. Without it the pawl
  can re-engage when line pressure drops. **Safety-relevant.** Driver located in §1.2.
- FCA shows all solenoids switched to a common internal "actuator supply" [M]. The internal
  terminal diagram (spec 01 §2.9) confirms it: the 8 lead-frame solenoids share a 12 V supply
  (B10–B13) and each has its own return line (B2–B9), so the drivers **switch the low side**.
- Coil resistance (ATRA *ZF8HP Introduction* webinar (2015), user's copy) [M]: variable-force solenoids (A–E, TCC, line pressure)
  about **5.5 Ω**; park release and park hold solenoids about **25 Ω**. Audi part names: park
  release N88, park hold N486.
- [D] Plausibility of the current unit: the channel maximum `0x2FFA` read as 0.1 mA steps is
  about 1.2 A, which a 5.5 Ω coil reaches at about 6.8 V, well inside the supply range. The
  25 Ω park hold coil draws about 0.5 A at 12–14 V.
- [M] Some vehicles add a solenoid holding a stop/start pressure accumulator; [I] this may use
  the spare chip-4 channels on other variants.

### 1.2 Actuator construction and the on/off output path [O]

The static initialiser `0x15BF2C` creates every actuator object with an explicit channel number
(base constructor `0xD9C54` stores it in byte 0):

| Object | Class (constructor) | Channel | Parameter | Solenoid |
|---|---|---|---|---|
| `0xFFF8B154` | pressure (`0x107B80`, 0x28 bytes) | 0 | type 0 | A |
| `0xFFF8B17C` | pressure | 2 | type 0 | B |
| `0xFFF8B1A4` | pressure | 4 | type 1 | C |
| `0xFFF8B1CC` | pressure | 1 | type 1 | D |
| `0xFFF8B1F4` | pressure | 3 | type 1 | E |
| `0xFFF8B21C` | small (`0x107F6E`) | 5 | type 0 | WK |
| `0xFFF8B22C` | small | 6 | type 1 | Sys |
| `0xFFF8B23C` | on/off (`0x107EFE`) | 7 | request 2 | MV Pos (park release) |
| `0xFFF8B24C` | on/off | **11** | request 3 | **park hold** |
| `0xFFF8B25C` | on/off | **10** | request 4 | open (see below) |

The type parameter is 0 for the normally-closed and 1 for the normally-open regulators,
matching FCA's types (§1.1) for every solenoid; this is a further confirmation of §1.

**On/off output path (actuator slots 10–14).** `0x7B6D8` copies slots 0–9 to the SPI channels
(§2) and slots 10–14 to a second array `0xFFF94FAA[0..4]`. `0x849D4` (switch-output manager,
enable mask calibration `0x1919B4` = `0x0B`) and `0x8ADF4` turn each into a PWM duty in per mille
(battery-voltage compensated) at `0xFFF9086A[k]`; `0x91500` writes it to the timer E duty
registers. Descriptor table `0x40CB4`:

| Switch output | Slot | Timer output (pin) | Enabled | Function |
|---|---|---|---|---|
| 0 | 10 | TOE30 (PL5) | yes | request 4 = status byte `0xFFF8B100`; meaning open ([I] TRS Park signal or switched supply) |
| 1 | 11 | **TOE31 (PL6)** | yes | **park hold solenoid** |
| 2 | 12 | TOE33 (PL8) | no | unused (empty source) |
| 3 | 13 | TOE32 (PL7) | yes | value from module `0x152xxx` (`0xFFF95F4E`), threshold-switched; meaning open |
| 4 | 14 | TOE22 (PL3) | no | unused |

These are exactly the five PWM group A channels of spec 02 §3.1 (same sub-blocks, 1 kHz period
and staggered offsets).

**Park module** (`0xE7000–0xE9200`, output routine `0xE88FA`, outputs via `0xE89D0` into
`0xFFF851A8[k]` with inverted copies at `0xFFF851AD[k]`):

- Park-lock decision (object byte 1): true when Park is requested and a release condition
  (`0xE8B56`, with hysteresis) holds, or when a force flag (`0xFFF89CF2`) is set; blocked by a
  flag (`0xFFF89C30`).
- Output 2 (MV Pos, park release) = energised when Park is **not** requested.
- Output 3 (ch 11 → TOE31, park hold) = energised whenever the park-lock decision is **false**.
- Outputs 1–4 can be overridden by a diagnostic actuator test.

[I] This matches FCA's description: the hold magnet keeps the release piston latched until
the park lock is allowed.

#### Park-lock decision in detail [O] (names and units [I])

Inputs come from the converter/park input block `0xFFF89C3C…` (filled by `0xB912C`, spec 09
§3.2), the safety-monitor objects `0xFFF8E890…` and calibration group `0xFFF84D3C` (struct
`0x56EC4`).

1. **Park request** (`0xFFF8AE29`, set by `0xCFFDE`): true when the range code
   `0xFFF89CDC` equals **8** ([I] P selected; range code from module `0x9B588`) and the
   auxiliary request `0xFFF89CF2` is clear, **or** when the auxiliary request `0xFFF89CF2` is
   set and flag `0xFFF8E69B` is clear. [I] `0xFFF89CF2` is an automatic park request (for example
   ignition off) and `0xFFF8E69B` a "stay in N" (car wash) inhibit.
2. **Park-lock decision** (park object byte 1, `0xE88FA`): set when not blocked (`0xFFF89C30`
   clear) and either the auxiliary request is set, or Park is requested and the lock is
   currently set or permitted by `0xE8B56`.
3. **Speed interlock** (`0xE8B56`): engagement is **refused** only if all of these hold:
   - a calibration map (group `+0x08`, indexed by an input byte) enables the interlock;
   - `0xE8B7E` is false: vehicle speed **> 10** (calibration `+0x0C`, units [I] about km/h)
     **and** a second speed value `0xFFF89CB8` **< 5** (calibration `+0x10`) **and** the speed
     signal is valid;
   - `0xE8BAA` is false (override: calibration `+0x14` = 0 and flag `0xFFF8B272` set).
4. **Outputs** (§1.2 table): park release (ch 7) energised when Park is not requested; park
   hold (ch 11 → TOE31) energised while the park-lock decision is false.

**Speed source:** the speed compared with 10 comes from the safety-monitor object
`0xFFF8E8B0`, which takes the **11-bit CAN signal in message `0x14B`** (byte 0 bits 2..0 + byte 1,
`0x7FF` = invalid; diagnostic ID `0x5034`), filters it, divides by 5 and, in the input
interface, by 2. [I] Vehicle speed from the brake system. A second monitored input uses
**CAN `0x140` byte 0 bits 3..2** (2-bit state; value 1 sets a flag used by the input interface).
The monitor objects keep inverted copies and **halt the controller** on any mismatch (safety
monitoring layer, `0x135xxx–0x136xxx`, gatherer `0x14FC5C`).

**Related fault codes** (`1034420267_dtc_list.csv`): **P1B14-00 "Unintended Park Position"**
(event 186, group 113) and P1B13-00 (event 187, group 114). Field report (technician video,
2015 Durango 8HP70): P1B14 caused by low line pressure from low fluid level; other reported
causes are a faulty MV2 (park release) or MV3 (park hold) solenoid [M]. [I] The monitor compares the park position with the commanded
state: line pressure holds the release piston out, so low pressure can let the pawl engage.
The monitor is decoded in spec 13 §4.1 (enable `0xC5E40`, report `0xC5E78`): it fails when two
bytes of the park/range state `0xFFF8D61A` (`[0x2E]` and `[0x06]`) are both 1.

For the project firmware: the park hold output must stay energised in every state except an
explicitly permitted park engagement, and engagement must be refused above the speed limit
while the speed signal is valid. The exact meanings of the range codes, the auxiliary request
and the second speed value (`0xFFF89CB8`, computed by `0xB5DA6`) are open; they can be read
live with the measurement protocol (spec 10).

### 1.3 Physical wiring [M][I]

Lead-frame terminals (spec 01 §2.9), using the FCA valve numbering of the lead-frame photo
(1 park release, 2 line pressure, 3 TCC, 4 C, 5 E, 6 B, 7 D, 8 A); [I] that the community
diagram uses the same numbering is an assumption:

| Terminal | B2 | B3 | B4 | B5 | B6 | B7 | B8 | B9 |
|---|---|---|---|---|---|---|---|---|
| Valve | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 |
| Solenoid | A | D | B | E | C | TCC | line pressure | park release |
| Channel | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |

The terminal order is the same as the channel order.

**Valve 9 = park hold solenoid.** The community diagram labels valve 9 (terminals C14 = 12 V,
C7 = return) as the valve that "releases the park position". FCA (lead-frame item 9) and the
ATRA webinar both identify #9 as the **park lock mechanical hold solenoid**; the hydraulic park
release valve is #1 (B9). [I] C7 is therefore the park hold return line, switched low-side by
TOE31 (§1.2), on its own supply as FCA shows.

## 2. Output chain [O]

| Step | Code / data | Content |
|---|---|---|
| 1 | per-actuator objects `0xFFF8B154…` (`0xDA032`); `0xD9F70(obj, current)` | the application sets a current for actuator *k*; byte 0 of the object is its channel |
| 2 | array `0xFFF84B78[12]` (+ inverted copy at `+0x18`) | current per actuator; getter `0xD9E4A` |
| 3 | `0xA3646` → `0xFFF850D6…` | copied per channel (channel 0 stored at `0xFFF850F2`) |
| 4 | `0xBC0F4` → `0xFFF89EAC[12]` | per channel, with plausibility checks and inverted copies |
| 5 | `0x7B6D8` → `0xFFF84890[10]` → `0xFFF94EE8[10]` | hand-over to the driver layer (getters `0xA4670…0xA46A6`) |
| 6 | `0x83DB0` → `0xFFF94F58[10]`, mode `0xFFF94FD8[10]` | channel request; mode 1 = off, 2/3 = current control, 4 = full on |
| 7 | `0x89540` (per channel) → `0xFFF90230[10]`, `0xFFF90812[10]` | current and mode for the chip driver; maximum `0x2FFA` |
| 8 | `0x901A0`, `0x90118` | per chip (struct `0xFFF91108 + 0x1C × chip`): setpoint 0 at `+0x0C`, setpoint 1 at `+0x14` (11 bits); dither amplitude from `0xFFF90320[ch]` |
| 9 | `0x924B0` | builds the 45-frame cyclic SPI job (buffer `0xFFF88518`), ORing the setpoints into command frames `0xE8` / `0xF0` |

Every stage keeps an inverted copy of the value and reports a fault (`0x226`, `0x238`) on a
mismatch. A reimplementation must provide equivalent safety (spec 03 §5).

Channel mode letters used by the driver (`0xFFF9081C`): `0x21` = current-controlled, `0x43` =
full on, `0x32` = off/fault [O].

## 2a. Pressure request → solenoid current [O]

This is the stage in front of §2 step 1. Calibration values are quoted only to show structure.

**1. Pressure requests.** Each regulator has a request object:

| Request object | Solenoid | Getter |
|---|---|---|
| `0xFFF8B7C8`, `B828`, `B888`, `B8E8`, `B948` (0x60 each) | A, B, C, D, E | `0xE513C(element 1–5)`; tables `0x507C4` / `0x507DC` |
| `0xFFF8B9B8` | WK (TCC) | |
| `0xFFF8B9A8` | Sys (line pressure) | |

Callers set a pressure with `0xE53DA(request, pressure_mbar, mode, priority 0–4, source id 0–0x29)`
(source ids above 0x29 are reported as an internal fault). The request object stores the call,
logs the source id (`0xFFF8E374`), and its virtual method arbitrates between sources; the
resulting pressure is at `request + 2`, the mode at `+7`. [I] Sources are the shift-phase engine
(spec 11 §3), the line-pressure and lockup strategies (`0xDC4C4` uses source ids `0x15` for Sys
and `0x16` for WK), and diagnostics.

**2. Actuator objects** (constructed in `0x15BF2C`, see §1.2) read their request and compute a
current each cycle (virtual method at vtable `+0x14`, called from `0xD9C7C`; when a diagnostic
override is active, `0xBB100` supplies the current directly instead):

| Object | Channel | Type | Request | Class / method |
|---|---|---|---|---|
| `0xFFF8B154` | 0 | 0 (normally closed) | A | element, `0x107BB8` |
| `0xFFF8B17C` | 2 | 0 | B | element |
| `0xFFF8B1A4` | 4 | 1 (normally open) | C | element |
| `0xFFF8B1CC` | 1 | 1 | D | element |
| `0xFFF8B1F4` | 3 | 1 | E | element |
| `0xFFF8B21C` | 5 | 0 | WK | supply, `0x107FA6` |
| `0xFFF8B22C` | 6 | 1 | Sys | supply |

**3. Current calculation.** Element regulators (`0x107C3E`); WK and Sys (`0x107FE0`):

```
i = curve_ch(p)                         0xD9D18: 12-point pressure→current curve per channel
  + temp_corr_type(i, T_fluid)          0xD9F96/0xD9EA4: 2-D map, current × fluid temperature
  + supply_corr_type(i, X)              0xD9F10: float map, current × 0xFFF8BCC0 (stock: all zero)
  + [B only] map(…, T_fluid)            0xD9EF6 (stock: all zero)
  + [Sys only] map(p, engine speed)     0xD9F44 (stock: all zero)
  + [WK only] offset 0xBA310            (value at 0xFFF8E612)
i = clamp(i, i_min, i_max)              0xD9CBC / 0xD9CEE, group 0xFFF84C94: 500 / 8500
0xD9F70(actuator, i)                    → §2 step 1
```

- **Curve per channel** (`0xD9D18`, group `0xFFF84D18`): channel 0 → 3371, 1 → 3374, 2 → 3372,
  3 → 3375, 4 → 3373, 5 → 3377, 6 → 3376. With the channel map of §1 this is A, D, B, E, C, WK,
  Sys, **confirming in code** the curve assignment of §3 and again the channel map. For each
  channel a per-unit curve (structure `0xFFF8CDFC`, pairs of curve pointer and valid flag) replaces
  the calibration curve when its flag is 1 [O]; [I] these are the end-of-line characteristics
  loaded from EEPROM (`0xE32FE`).
- **Interpolation** (`0xB6EAE`): linear between points, clamped at both ends; 2-D maps
  (`0xB7164`) likewise.
- **Temperature correction** (group `0xFFF84D14`): one map for normally-closed and one for
  normally-open regulators (each with a per-unit override), 12 current points × 5 temperatures
  (°C + 40 = 20, 40, 50, 85, 150, i.e. −20 … 110 °C). The correction is zero at 45 °C and at the
  curve ends; stock magnitudes are up to about ±300 current units at −20 °C. The fluid temperature
  comes from `0xFFF89C13` (spec 07 §7.1).
- **Fast fill / step mode:** when the request mode (`+7`) is 1, the element method runs a small
  state machine (`0x107D90`) using a fill pressure from the request (`+0x42`) and a time limit.
- **Plausibility:** a pressure request below the line-pressure value `0xFFF8BCB0` and below
  12,750 mbar while the actuator is in limitation is reported (`0x1C4`/`0x1C2`).

**Units.** Curves map mbar to 500 … 8,500 current units, and the driver accepts up to `0x2FFA`
(12,282). [I] 1 unit = 0.1 mA, giving 50 … 850 mA, a normal range for these variable-force
regulators. Each curve has a step between 0 and 10 mbar (for example A: 500 → 2,120): [I] the
regulator is held at minimum current for "no pressure" and starts at its threshold current for
any pressure request.

**For the project firmware** [D]: implement request arbitration, the curve, the temperature
correction and the clamp; take the curves from the unit's own EEPROM characteristics where
available (§3), and re-derive the temperature correction for the actual solenoids.

## 2b. Driver chip protocol (bus 0, chips 0–4) [O]

Per-chip state record `0xFFF91108 + 0x1C × chip` (fields used below) and three SPI jobs, each
sending the same command to all five chips before the next command (frame *n* → chip *n* mod 5).
Commands with bit 7 set are writes; the data is OR-ed into the low byte (or low 11 bits).

**Cyclic job** (builder `0x924B0`, template `0x40D0C`, buffer `0xFFF88518`, 45 frames):

| Frames | Command | Data | Meaning |
|---|---|---|---|
| 0–4 | `0x88` | record `+0x07` | **mode bits**: ch 0 in bits 1..0, ch 1 in bits 3..2 (0 = disabled, 1 = off, 2 = current control, 3 = full on); bit 4 = calibration flag (stock 1), bit 5 = a reduced-frequency mode (`0xFFF9091B` = 1) |
| 5–9 | `0xA0` | record `+0x10` | ch 0 dither amplitude = request × 0.0215 (`1/46.5`); 0 below 50 or when not in current control |
| 10–14 | `0xE8` | record `+0x0C` | **ch 0 current setpoint, 11 bit** |
| 15–19 | `0xA6` | record `+0x18` | ch 1 dither amplitude |
| 20–24 | `0xF0` | record `+0x14` | **ch 1 current setpoint, 11 bit** |
| 25–44 | `0x60`, `0xC0`, `0x66`, `0xD8` | — | reads: [I] status and measured current per channel |

**Setpoint scaling** (`0x90118`): `setpoint = (I + offset) / 6`, with I in the driver's current unit
(maximum `0x2FFA` = 12 282 → setpoint 2047, the 11-bit full scale) and `offset` = `0xFFF9090A`
(a small state value, 0–4). With [I] I in 0.1 mA, one setpoint step = **0.6 mA**, full scale
≈ **1.23 A**. Mode letters (`0x89540`): `0x21` current control, `0x43` full on (setpoint forced to
`0x2FFA`), `0x32` off. The software re-reads the setpoint from the transmit buffer and reports
`0x238` if it differs from the commanded value by more than 15 units (buffer integrity, not a
current measurement).

**Configuration job** (builder `0x92344`, template `0x40AD8`, 40 frames): writes `0x8E`, `0x90`,
`0x96`, `0x88`, then reads `0x28`, `0x30`, `0x2E`, `0x36`:

| Command | Data (record field) | Stock value per chip | Meaning |
|---|---|---|---|
| `0x8E` | `+0x09` | `32000 / f − 1` (7 bit) + bit 7 from calibration (`0x80`) | [I] dither / PWM frequency divider for frequency *f* (`0x8FC5E`, set from `0x89708`) |
| `0x90` | `+0x0A` | `0x21` | configuration; bits 7..6 = readback range; set to `…10b` when a channel's current exceeds 10 000 while the other is below |
| `0x96` | `+0x0B` | `0x55` | configuration; bits 7..6 = readback range |
| `0x88` | `+0x07` | mode bits as above | |

Stock values come from calibration `0x1918B4…0x1918C2` (driver configuration, not shift
calibration). In the reduced-frequency mode the values are rescaled by 1.4 / 0.714 (`0x8FDB6`).

**Reply format** (completion handlers `0x921E0`, `0x92298`, `0x923BC`, `0x92530`) [O]: every reply
frame carries a fixed header — high byte `0xA2` for ID, diagnostic and configuration frames,
top five bits `10100b` (`0xA000` under mask `0xF800`) for cyclic frames, followed by 11 data
bits. A wrong header marks the chip's job as failed (record `+0x01` / `+0x02` flags).
- Cyclic job: the read frames return the values written in the same cycle — reply of frame
  `0x60` = data of `0xA0` (ch 0 dither), `0xC0` = `0xE8` (ch 0 setpoint), `0x66` = `0xA6`,
  `0xD8` = `0xF0`. A mismatch is a fault; a non-zero setpoint readback sets a per-channel
  "active" bit (record `+0x06` bits 1 / 3).
- Diagnostic job (`0x18`, `0x1E`, `0x28`): ch 0 status → record `+0x12`, ch 1 status → `+0x1A`,
  chip status → `+0x08`.
- Configuration job: the reads return the configuration bytes and are compared with what was
  written.
- ID job: `0x06` → record `+0x05`, `0x00` → `+0x04`; record `+0x04` = `0xBD` selects a chip
  revision handled specially (`0x901A0`, `0x92A04`).

The cyclic job therefore does **not** return a measured current. The values that `0x904FA`
scales (record `+0x0E`, `+0x16`) have no writer in the driver code that was traced; [I] the
chips regulate current internally and report faults through the status bytes. Measuring the
actual current is a bench task (§4).

**Readback** (`0x904FA`): per channel an 11-bit signed value from the receive data (record `+0x0E`
for ch 0, `+0x16` for ch 1), scaled by `324.92 / (n + 1)`, n from the readback-range bits of
`0x90`/`0x96` (stock n = 4 → factor 65.0), sign inverted for odd channels → `0xFFF90258[ch]`.
Status bits (record `+0x08`, `+0x0E` / `+0x10`) are combined into a per-chip fault byte
(`0x931F8`). [I] This is the measured channel current; its unit and the receive-frame mapping
are still open.

**ID job** (template `0x40A84`, 10 frames): `0x06`, `0x00` per chip; reply high byte `0xA2`.

## 3. Solenoid characteristics [O]

Seven pressure → current curves (calibration objects 3371–3377, group `0xFFF84D18`, struct
`0x54C54 + 8`) have 12 points each, from 0 up to 10.5–20 bar:

| Object | Pressure range (mbar) | Shape |
|---|---|---|
| 3371 | 0 – 10,500 | rising |
| 3372 | 0 – 13,400 | rising |
| 3373 | 0 – 16,500 | falling |
| 3374 | 0 – 10,500 | falling |
| 3375 | 0 – 16,500 | falling |
| 3376 | 5,662 – 20,000 | falling |
| 3377 | 0 – 11,817 | rising |

- Rising curves belong to normally-closed regulators and falling curves to normally-open ones.
  With the FCA types (§1.1), the pattern rising, rising, falling ×4, rising matches the order
  **A, B, C, D, E, Sys, WK** exactly [D], and the code confirms it (§2a) [O]:

| Object | 3371 | 3372 | 3373 | 3374 | 3375 | 3376 | 3377 |
|---|---|---|---|---|---|---|---|
| Solenoid | A | B | C | D | E | Sys (line pressure) | WK (TCC) |

  The 5.7–20 bar range of 3376 fits line pressure. Bench check still needed.
- `0xE32FE` compares each curve with a per-unit copy loaded from EEPROM. [I] Each mechatronic
  carries its own end-of-line characteristics, and the calibration copy is the fallback.
  The project firmware should read and use those per-unit values.

## 4. Open items

1. Bench confirmation of the channel table (§1).
2. Units of the channel current: the curves span 500 … 8,500 and the driver maximum is `0x2FFA`;
   [I] 0.1 mA steps (§2a). Unit of the 11-bit chip setpoint.
3. EEPROM block holding the per-unit characteristics (`0xFFF8CDFC`, §2a); meaning of the
   inactive correction inputs `0xFFF8BCC0` and `0xFFF8E612`; the arbitration rule of the request
   objects and the list of source ids.
5. Meaning of switch outputs TOE30 and TOE32 (§1.2), one of which is probably the hard-wired TRS
   Park output (spec 01 §2.8).
6. Park logic inputs (§1.2): range codes, the auxiliary park request `0xFFF89CF2`, the inhibit
   `0xFFF8E69B`, the second speed value `0xFFF89CB8`, and the units of the CAN `0x14B` speed.
4. Identity of the SPI driver chips (spec 03) and the meaning of their other command frames.
