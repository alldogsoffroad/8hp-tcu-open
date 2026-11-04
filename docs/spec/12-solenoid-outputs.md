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

### 2c. Project implementation (M3, extended in M4b)

`firmware/io/actuators/sol_drv.c` reproduces the four bus-0 jobs with the stock frame layouts and
reply checks; RSPI-A is driven by the CPU with the five-command sequence of the stock job start
(`0x96468`). Configuration bytes `0x8E` = 0x00 [I], `0x90` = 0x21, `0x96` = 0x55, mode bit 4 set;
dither amplitude 0 (dither not used). A reply fault latches and forces all channels off; a
configuration read-back mismatch is counted (`cfg_mismatch`) but not faulted — on the live chips
the configuration reads return 0x00 whatever was written (§2d). Channels 0–6 are only driven by
the bench test `firmware/app/bench_out.c` (safety design in `firmware/README.md`). M4b
(2026-10-30): channel 7 is driven full-on by the park module (`app/park.c`, compile gate
`TCU_M4B_OUTPUTS`) or the FE11 commissioning routine; the park hold (§1.2, TOE31) is driven by a
new timer-E platform output. The supervisor output enable is **not** requested by the project
firmware: on the bench, bit 0 of the `0x8E` word sent without the stock start-up register
initialisation resets the TCU (spec 03 §4.4); the per-source request API
(`SUP_EN_BENCH` | `SUP_EN_PARK`) exists but stays unused until the enable path is understood.
The full-on chip mode field and the TOE31 polarity are to be verified by FE11 on the bench
(bench items 1/16) — blocked by §2d.

### 2d. Bench check 2026-10-30 — live chips, spare E-Shift mechatronic [M]

First live-chip session (real coils connected, 12.5–13.8 V bench supply, TCU in the case,
firmwired via the RAM loader). Firmware-side results:

- **Exchange protocol confirmed.** The stock CPU-mode recipe (spec 03 §4.4, `0x1D430`) is what
  the hardware needs: full register re-setup per exchange, then SPDR write **plus the explicit
  start trigger** (read SPSR with SPTEF set, then write 0) — without the trigger the peripheral
  never clocks. The first exchange after power-up misaligns; the ID job is retried (5×) and then
  runs clean forever.
- **Four chips, not five.** Select 4 (`i % 5 == 4`) consistently returns `0x0000` — the fifth
  device is unpopulated on this board. `SOL_CHIPS_REAL = 4`; channel map unchanged (8 channels
  on 4 two-channel chips).
- **Command map probed live** (FE12 type 3, one command per chip at a time): `0x06` → `0xA212`,
  `0x00` → `0xA2BD`, `0x28` → `0xA240` (constant, both chips, always), `0x2E`/`0x30`/`0x36`
  reads return `0x00`-class data; writes `0x8E`/`0x88`/`0x90`/`0x96` acknowledge with the
  `0xA0`-class echo, setpoints `0xE8`/`0xF0` likewise; unknown commands answer `0xAAxx`;
  `0x0000` sometimes. So the chips accept everything — and keep answering the same `0xA240`
  status.
- **Configuration writes do not stick**: every configuration read-back returns 0x00 regardless
  of the value written (demoted to a counter, see §2c).
- **No current flows.** With the M3 ladder running clean on the firmware side (all 10 steps,
  end reason 0x01 = completed, setpoints acknowledged, no reply faults, status `0xA240`
  fault-free), the bench ammeter shows no movement (≤ 1 mA variance) on any channel 0–6 at
  12.5 V and 13.8 V, supply delta invisible on the Teensy rail monitor. The coils are ~25 Ω
  and directly wired, so a commanded 1 A could not hide.

**Blocker, ranked.** The driver chips talk but do not drive; the most probable cause is the
switched solenoid supply (VS rail to B10-13, spec 01 §2.9) never asserted:

1. **Supervisor start-up register init missing.** The stock *application* drives the same
   `0x8E` word with bit 0 = output enable — but only after start-up writes to supervisor
   registers `0x84`, `0x88` and `0x90` (switch-output enables, bits 5–7) that the project has
   never sent (spec 03 §4.4/§6). On our bench, bit 0 sent cold — in the *boot*-style cyclic
   word — **resets the TCU**. The constant `0xA240` chip status may simply mean "no VS".
   Next: decompile the app's start-up sequence (`0x91DD2` area, staging words
   `0xFFF907D4/0xD6`, reply handler `0x92AC0` → replies are `0xAAxx`/`0xA8xx` class) and replay
   the register init through a new FE12 type / RSPI-B lab before touching bit 0 again.
2. Chip-level unlock/config semantics (writes ack'd, reads 0x00 — the stock sends the
   configuration job cyclically, maybe for a reason; or an unlock command we have not found).
3. Mode-byte semantics (mode bits acknowledged but meaning something else on this revision —
   ID `0xA212`/`0xBD` revision handling in the stock, §2b).

### 2d addendum — the gate localized to the supervisor echo grant (2026-11-04) [O]

Reproduced with the engagement machinery live: FE10 accepted and completed (end 0x01,
setpoints ack'd, `0x7F3` echoed the commanded current) with **zero ammeter movement**, while
the supervisor conversation simultaneously reached the full ladder end-state (HOLD, level
0x1F commanded *and echoed*, commissioning values held, dither cycling — the same visible
state the driving stock presents). The rail stays closed-off because the supervisor's cyclic
`0x22` echo never grants: our every cyclic register read returns `0xAA00`, while the stock's
cached echo byte (`0xFFF907D5`) reads `0xA1` — bit 0 set — in the dump taken while it drove
the mechatronic at 1.773 A (spec 03 §6.2a). The `0xFFFC0D04` boot-mailbox/mode-3 path is
**ruled out** as the gate: the driving stock never ran that handshake. What earns the grant
is somewhere in the cyclic RSPI-B conversation the stock keeps and we only partly mirror —
that diff is the open work item (spec 03 §6.2a, "the open question").

The chips' markings are unobtainable (shielded TCU, no marks found), so the datasheet route is
closed; the stock code is the datasheet.

**Update 2026-10-31 (FE13 supervisor lab):** the supervisor-side gate was exercised directly —
the full stock cyclic pattern (three jobs, toggle alternating, ID-job enumeration, `0x90`
switch bits 5–7 stepped one at a time) leaves the driver status at `0xA240` and all config
read-backs at 0x00. Every attempt to assert the `0x8E` output-enable bit resets the TCU
instantly (see spec 03 §4.4 for the measured word semantics: bit 2 = life sign, bit 6 =
power-down, bit 0 = reset). The remaining stock behavior not yet replayed: the app's WDT-mode
switch on enable transitions (`0x920A4`) and the handover job frames `[0x18, 0x0C, 0x1E,
0x06, 0x14]` (registers `0x0C`/`0x14` now readable — `0xDF`/`0xD5`).

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
4. Identity of the SPI driver chips: not marked (bare dies on the ceramic hybrid, user photos
   2026-10-15); protocol decoded from the firmware (§2b).
7. Output enable path: SSLB1 control word bit 0 and the supervisor service (spec 03 §4.4).
   Bench check that a channel drives a dummy load only with the enable set.

**Bench 2026-10-31 (running stock, live 0x361 reads):** the app-side solenoid
command pair lives around **0xFFF90586** (`11 xx 00 01 11 xx` — two channel
commands mirrored); in the idle-hold state (~1.85 A) the stock **cycles
combinations on its own** (observed xx = 0x41/0x34/0x3A/0x27/0x0E/0x1F/0x3C/
0x26/0x44 over minutes — a keep-warm/pressure-priming state machine, which also
explains the self re-engagement after a comm-timeout release). Companion state
cells: 0xFFF93B92 = state enum (observed 0..4), 0xFFF90862 = 16-bit live value
(0x1300-0x1F50 range, tracks the mode), 0xFFF9054A/0x90544 = mode bytes. The
word/echo (0xFFF907D4/0x7D5), the level cell (0x9054F = 0x1F) and the flag
area read IDENTICAL across engaged and released states — output gating is the
0x90586-class command cells, not the supervisor cells. Sweep tool:
`/tmp/ramsweep.py` (267 cells, 5-7 s when engaged); `/tmp/ramwatch.py` polls
the state cells and auto-fires a full sweep on a flip.

**Ammeter video 2026-10-31 (76.3 s, lever-sequence run):** transcription of the
PSU display at 21 points (every read cross-checked against the display's own
W = V·A). Aligned to the run log (t₀ ≈ 14:56:27.7): released 0.169–0.172 A
(feed off) → engage climb t≈8 (1.566 A) → **lever holds P/D1/R/N/D2/P2 all at
1.855–1.868 A** — no detectable phase difference at display resolution.
The three full dropouts to ~0.17 A (t≈9–18, t=28.3–30, t=48–~66) are **not
lever responses**: each coincides (±0.2 s) with `leverseq.py`'s measurement
polling bursts (baseline t=8.3+, D1-end t=28.3, N-end t=48.3) which stall the
46-node feed on the shared bus; the stock times out on missing node frames,
releases (audible click), and re-engages ~1–2 s after the feed resumes. The
2–4 s between-phase gaps (lever unchanged, feed alive — t=40 and t=44 reads
1.857/1.858) do **not** drop it, so the node-frame timeout is ≥4 s while total
feed loss releases within a burst. Third dropout was the longest (~18 s),
matching the known plant-poll latency growth with uptime (0.3 s → 2.5 s).
Working numbers for the open firmware: **engaged hold ≈ 1.85–1.87 A**
(user-confirmed idle 1.870), **released ≈ 0.17 A**, re-engage climb ≈ 1.5–1.7 A
over ~1–2 s.

**Clean-feed re-run + stop tests 2026-10-31 (post-video):** re-ran the full
P-D-R-N-D-P sequence with ALL bus polling removed (feed-only, 0 errors) —
started from the overnight self-held 1.905, ran all six phases, ended at
1.871; transcribed below. Then the release discriminator, live at
the ammeter: a **D-feed stop releases to ~0.173 A** (third confirmation; the
two 2026-10-31 mid-day D-feeder stops behaved the same), while every
**P-ending stop self-holds at ~1.87 A** (twice: overnight 1.870, today 1.871).
The no-comm safe state is **lever-gated: P keeps the solenoids held (park
lock), D drops them**. After the D-stop release the box **re-engaged itself
to ~1.8 A at ~1 min 9 s with no feed** — the 0x90586 keep-warm/priming
machine (§ above) pulling it back up autonomously; the released window was
~1 min that time (later the same evening the same silence protocol re-engaged
in ~10 s — the window is a variable internal timer, see the sweep-session
note below). This also explains the
ammeter video starting at 0.170 (inside the released window) vs the overnight
1.87 hold (P-ending, never released).

**Clean-run video transcription 2026-10-31 (IMG_2635, 93.2 s, 41 display reads
at 2 fps, every read W = V·A cross-checked):** video t₀ pinned to the run log
two ways — the phone's `creation_date` tag (video leads the 15:32:02.142 run
start by 16.1 s) and the audio track (a single loud solenoid clack at video
t=21.16 vs logged feed-on 21.14), so the alignment is exact to ~0.2 s.
Run-relative map: feed-on P = run 5, D 13, R 23, N 31, D 37, P 45, stop 53.
1. **Feed-on from silence releases the engaged self-hold.** Flat 1.905–1.906
   through pre-feed (video 0–21), one loud clack at the P burst, display at
   0.170–0.172 solid by video 22.5 (the 21.5/22.0 frames still read 1.906 —
   PSU display refresh straddling the fall). The box stays released through
   the whole P phase AND past the D boundary.
2. **Autonomous silent re-engage ~13 s after feed-on** (edge in video
   (34.5,35] = run ≈18.7, ~5.5 s into D; no audible click). The window is
   feed-on + ~13 s, **not phase-correlated** — released at D+0.8 through
   D+5.5. Reads like an ignition-on engagement procedure settling, not a
   lever response.
3. After re-engage: **flat 1.905 through the entire D→R→N→D→P sequence**
   (six samples video 35–65) — mid-feed lever commands do not disturb the
   engaged hold at display resolution (confirms the first video's finding
   without the polling confound).
4. **Second, uncommanded release late in the final P phase** (onset run
   ≈49–52, quiet), **re-engaging exactly at the feed stop** (run 53.0: video
   69.0 still 0.171, 69.5 mid-rise 0.884, 70.0 = 1.698, twin clicks audio
   69.37/69.48). With the stop discriminator above: stop-from-P-engaged →
   self-hold; stop-from-D-engaged → release; stop-while-released → engage.
   The stop event forces a hold-state re-evaluation in both directions.
5. **Post-stop hold is not flat:** engaged hold wanders 1.70–1.91 A over the
   next 20 s (1.698→1.847→1.906→1.709→1.875, never releases) — the 0x90586
   keep-warm machine cycling hold combinations, matching the user's live
   1.871 post-run read.
The first video "starting at 0.170" is thereby explained the same way: both
runs show feed-on-from-silence knocking out the self-hold for ~13 s before
an autonomous re-engage.

**0xFFF8xxxx measurement-cell sweep + stop-oscillation video 2026-10-31 evening
#4 (IMG_2637, 75.6 s, 20 display reads, every read W = V·A cross-checked):**
new tool `/tmp/sweep8.py` — cmd `0x20` read-words proven live on the bench
(3 × A16 words per request on 0x361, reply `20 81` on 0x398; 401 cells — all
392 CSV measurement addresses in 0xFFF8xxxx, 8 park-logic cells, the 6 FFF9
anchors — in ~3 s per sweep, 3× the 0x2A transport; 0 fails on all three
sweeps). Three states captured with the ammeter as discriminator: **C**
16:02:52 pre-feed, D-last engaged (1.912 A); **A** 16:03:45 = +6.2 s after
the P-stop (engaged); **B** 16:04:23 = +4.1 s after the D-stop.

1. **No gear/range cell hides in the measurement table.** The only cells that
   differ across C/A/B beyond ±1-LSB monotonic counters (e.g. 0xFFF80F2E
   0619→061a→061b) are the known state pair: 0xFFF93B92 (C=03 with D-engaged,
   A=01 with P-engaged, B=02 transient) and 0xFFF90862 (0x1357/0x1b57/0x1e98).
   The lever information that drives the stop decision is **not** among the
   392 CSV measurement addresses — it lives outside 0xFFF8xxxx or in derived
   form (0x2EA counters → 0xFFF9xxxx state machine).
2. **Keep-warm commands continue while released:** sweep B (box released at
   capture, video below) still reads a cycling command pair at 0x90586
   (0x11 0x16). "Released = zero commands" is dead — the output gating sits
   downstream of the command cells.
3. **The stop is an immediate re-engage in both lever contexts, then a
   state-dependent settle — caught on video this time.** P feed: released
   0.170–0.174 for the whole 10 s, engaged by the first post-stop frame
   (t=9.5 → 0.172, t=10.0 → 1.809), then keep-warm wander (1.81 → 1.57).
   D feed: released 0.171–0.173 for the whole 10 s, **engaged ≤0.2 s after
   the stop** (t=48.5 → 0.172, t=49.5 → 1.318 mid-climb, t=50 → 1.806),
   **released again ~3–4 s later** (t=53 → 0.176 — exactly the sweep-B
   window, hence the enum-02 / 0x1e98 snapshot), **re-engaged by t=56**
   (1.680), then held engaged 1.68–1.91 to the end of the recording (user's
   live read 1.898). A drop→rise→drop→rise oscillation within ~7 s of the
   stop. Video alignment: phone creation tag 16:03:29 ≈ P-feed start
   16:03:29.137; both stop edges land inside the observed engage brackets.
4. This re-frames the "D-ending stop releases" law above: every earlier
   D-stop release was observed with the box **already released at the stop**
   (the 10 s feeder runs sit inside the ~13 s feed-on settling window, and
   today's video proves a 10 s D feed never engages mid-feed), and a ≤0.5 s
   post-stop engage blip is easy to miss by eye.

**Discriminating test executed 2026-10-31 evening #5 (/tmp/stoptest.py,
identical 10 s D-feeds, ammeter-timed):** the 0x361-traffic hypothesis is
**refuted**. Silence run (stop 16:21:43.8, then 120 s of zero bus traffic,
verified 0 requests sent): engaged 1.912 pre-feed → 0.170 after the stop,
held ~10 s, rose to 1.904, held to +120. Poll run (stop 16:26:19.5, then
60 s of sustained cmd-0x20 reads, 2,994 requests / 2,993 replies at
50 req/s): engaged 1.904 → 0.179 → back to 1.908 and held (window un-timed,
brief). Bus traffic is neither required for a fast return nor does silence
force a long one — and the box answers 0x361 reads normally while released.
The post-stop released window is a **variable internal timer**: observed
~3–4 s (sweep session), ~10 s (silence), brief (poll), and ~69 s only in the
2026-10-30 dstop run, which is the outlier. What sets the period is unknown
([I] candidates: prior cycle history, time since feed-on, internal phase of
the keep-warm machine). Working summary for the open firmware: after losing
comm in D the stock re-primes the hold on its own within seconds-to-minutes;
it does not latch released.
