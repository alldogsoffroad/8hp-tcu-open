# Spec 11 — Shift execution (clutch control during a gear change)

Status: **draft.** The shift-element matrix, the parameter organisation and the phase engine are
established. The role of each individual pressure map, the pressure-to-solenoid path and the
adaptation of fill values are **open**.
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference, **[D]** derived.

Calibration values are Bosch/ZF work product (README conventions). Several of the maps below
(`0x1BD17C–0x1BEAF2`) are also covered by the protection data (spec 06 §6.1). The project must
re-derive pressures for its own hardware and vehicle; this spec gives the structure.

## 1. Shift elements [O]

The 8HP has five shift elements, A–E. A 6-byte-per-row table at `0x505A4…0x50633` (users
`0xDC3DC`, `0xDC410`) lists which elements are engaged. Column 0 is unused; columns 1–5 are
A–E:

| State | Row address | A | B | C | D | E |
|---|---|---|---|---|---|---|
| R | `0x505E0` | ● | ● | | ● | |
| N | `0x505E6` | | | | | |
| 1 | `0x505EC` | ● | ● | ● | | |
| 2 | `0x505F2` | ● | ● | | | ● |
| 3 | `0x505F8` | | ● | ● | | ● |
| 4 | `0x505FE` | | ● | | ● | ● |
| 5 | `0x50604` | | ● | ● | ● | |
| 6 | `0x5060A` | | | ● | ● | ● |
| 7 | `0x50610` | ● | | ● | ● | |
| 8 | `0x50616` | ● | | | ● | ● |

This matches the published ZF 8HP element scheme. Every single-step shift therefore releases
one element and applies one other, with two elements held. Rows before `0x505E0` are special
states (open).

## 1a. Transmission hardware [M]

Source: ZF 8HP70 repair manual (user's copy, ZF Services 2014), power-flow diagram and
hydraulic schematic E26/1. Facts only; the diagrams are ZF copyright.

**Gear ratios:** 1st 4.70, 2nd 3.13, 3rd 2.10, 4th 1.67, 5th 1.29, 6th 1.00, 7th 0.84,
8th 0.67, R −3.30. (The FCA workbook prints 1st as 4.69 [M].)

**Elements:** A and B are **brakes**, C, D and E are **clutches**. The engaged elements per gear
in the power-flow diagram agree with the firmware table in §1.

**Solenoids in the mechatronic: 7 pressure regulators + 2 on/off actuators** (park release valve
and park hold magnet; the ZF repair manual shows only the hydraulic ones). Types and channels:
spec 12 §1.1.

| Solenoid | Type | Acts on |
|---|---|---|
| EDS A … EDS E | pressure regulator | shift elements A–E, through clutch valves KV-A … KV-E |
| EDS WK | pressure regulator | torque-converter clutch, through converter clutch valve WKV |
| EDS Sys | pressure regulator | system (line) pressure, through main pressure valve Sys-V |
| MV Pos | on/off | park-lock position valves (Pos-V, PS-V) and park-lock cylinder |

- Brake B has two pressure areas (B1, B2), each with its own clutch valve and holding valve.
- Each element has a holding valve (HV-A … HV-E) next to its clutch valve.
- The manual treats the mechatronic as a sealed unit; it gives no electrical data.
- The solenoids are driven through the SPI driver chips, not directly by timer outputs
  (spec 12).
- Bench limit: maximum oil temperature on a test bench **120 °C** [M].

Additional facts from the FCA *8-Speed Automatic Transmission Diagnosis and Repair* student workbook (2015), user's copy [M]:

- Physical element order front to rear: A, B, E, C, D.
- Extra element states: **P** = A + B; **N**: A applied, B held at contact-point pressure;
  **1st with neutral idle control**: A + C, B in controlled slip. (The firmware table in §1 has an
  empty N row; [I] these states are handled by the special rows before `0x505E0`, open item 6.)
- Brake B has an apply chamber (B1, applied in three stages) and a release chamber (B2, held at
  about 1.9 bar).
- Torque converter: multi-disc lockup clutch, can be applied in any gear 1–8; converter
  pressure at least 0.35 bar with lockup open and 1.0 bar with lockup applied.
- Park system: spring-loaded towards Park; hydraulic release (park release solenoid) plus a
  mechanical hold magnet (park hold solenoid); park position sensor with "Park" and "Not Park"
  Hall signals.
- **Default mode:** if power or the TCM is lost while moving forward, the hydraulics keep C, D
  and E applied, i.e. **6th gear**; after key-off the vehicle cannot move.
- Cooler bypass valve opens to the cooler at 90 °C. Pump: chain-driven double-stroke vane pump.
- ATRA *ZF8HP Introduction* webinar (2015), user's copy [M]: neutral idle control slips brake B (about 10 % converter slip); on
  Dodge 8HP45 a thermal management unit holds the fluid near 80 °C; engine stop/start (HIS
  accumulator with a latching solenoid) was not used on 2012 Chrysler applications. (The
  webinar's 8th-gear slide lists A, D, C, which is the 7th-gear set; the firmware table, FCA and
  ZF all give 8th = A, D, E.)

## 1b. Gen 1 vs Gen 2 [M]

Source: Sonnax *Chrysler ZF8 (Gen. 1 & 2) Identification Guide* (J. Dial, 2023). The specs in this document were derived from a **Gen 1** firmware
(8HP45 / 70 / 90). Gen 2 (8HP50 / 75 / 95, from 2019 on some Chrysler/Ram/Jeep) differs:

| Item | Gen 1 | Gen 2 |
|---|---|---|
| Brake B | apply chamber B1 + release chamber B2 | apply only; B2 replaced by a return spring |
| Brakes A and B | — | wave springs between steel plates (faster release) |
| Valve body | B2 circuit; five valves in the upper casting | B2 circuit removed; no valves in the upper casting |
| Solenoid order (left → right) | A, D, B, E, C, TCC, LP, park release | A, **B, D**, E, C, TCC, LP, park release |
| Solenoid caps | orange = normally low, white = normally high | cream = normally low, blue = normally high; not interchangeable with earlier solenoids (snout diameter) |
| Gear set | — | changed internal parts and gear ratios |

[I] A Gen 2 TCU therefore needs at least a different channel-to-solenoid assignment, a different
brake-B control (no B2), and its own ratios and calibration. Whether Gen 2 uses the same
controller and software structure is **open** until a Gen 2 dump is analysed. Gen 3 (Infineon
TriCore TCU) is out of scope.

## 2. Parameter organisation [O]

The shift-control object holds six family descriptors (`+0x3C … +0x50`, loaded by
`0x10F584` for a variant number). Each family is a list of (application copy, calibration
copy) block pointers, selected through index tables:

| Indexed by | Index table(s) | Families (list address, entries per block) |
|---|---|---|
| **gear pair** (1↔2 … 7↔8 → 0…6) | `0x5E0A8` (higher gear − 2), `0x5DE20` (lower gear − 1) | `0x58C98` (4), **`0x58EB4` (17)**, `0x59538` (8), `0x5A274` (18), `0x59F54` (39), `0x59418` (5) |
| **shift type** 0…27 = 7 × kind + gear pair | `0x10F4EC`: four 9 × 9 (from, to) tables `0x5E330` (kind 0), `0x5E474` (1), `0x5E5B8` (2), `0x5E6FC` (3) | `0x59370` (5 entries: fill timing curves) |
| **shift type** 0…42 | `0x10F526`: three 9 × 9 tables | `0x59E34` (13 entries: curves, 2 pressure maps, end-of-shift values) |

[I] The four kinds are power-on upshift, power-off upshift, power-on downshift and power-off
downshift; the exact assignment is open.

### 2.1 Pressure maps [O][I]

Most pressure maps share one shape, 8 × 10:

- **x** = transmission temperature, 8 points `19…172` = **°C + 40** (−21 … +132 °C) [I]; the
  same +40 offset is used by the converter's temperature window (spec 09 §3.3);
- **y** = torque, 10 points `−102 … 900` [I: input torque, Nm];
- **z** = clutch pressure in **mbar** (stock range about 0–7,100) [I].

The main gear-pair family (`0x58EB4`) has, per gear pair: 2 bytes, 5 torque curves, 2 small
correction maps (4 × 6), and **8 pressure maps** (slots `+0x24…+0x40`). [I] These are the
on-going and off-going pressure levels for the shift phases (§3); slot-by-slot roles are open.

## 3. Phase engine [O]

Each shift element taking part in a shift is handled by a phase object (code
`0x10F7D0–0x111400`):

- `0x10FB72` steps through the phases: while the current phase reports "done" (`0x1106DC`),
  it takes the next phase from a **sequence table** (`0x63555`, entry `step × 3 + type`) and
  runs the phase entry (`0x10FFBE`).
- The sequence **type** (`0x10FF9E`) depends on the element's role in the shift:

| Type | Phase sequence | [I] Role |
|---|---|---|
| 0 | 12 → 13 → 14 → 15 → 17 | off-going element (release) |
| 1 | 1 → 2 → 3 → 4 → 9 → 17 | on-going element, prepare only (fill and wait) |
| 2 | 1 → 10 → 11 → 12 → 13 → 14 → 15 → 17 | on-going element, full shift |

What each phase does [O]; names [I]:

| Phase | Entry (`0x10FFBE`) | Completion (`0x1106DC`) | [I] Name |
|---|---|---|---|
| 1 | duration from the shift-type table (`0x59370`) | timer, or a torque condition | fill start |
| 2 | fast-fill routine | `0x110430`: temperature/torque condition | fast fill |
| 3 | pressure = base + offset; start ramp | `0x11058A`: timer or pressure reached | fill compensation |
| 4 | ramp object reset | `0x10FA28` or speed threshold | fill hold |
| 9 | — | speed threshold (`ctx + 10`) | wait for synchronous point |
| 10 | target pressure from the shift context; torque-dependent offset | timer | torque phase start |
| 11 | duration from a calibration curve | timer | torque phase ramp |
| 12 | pressure ramp; torque-based target | speed condition against map (`+0x18` of the 43-type block) | torque handover |
| 13 | ramp | progress ≥ threshold (`+0x30` of the 43-type block) | inertia (speed change) phase |
| 14 | — | progress and speed difference | end of inertia |
| 15 | ramp to **20,000 mbar** (`0x4E20`) | timer | close (full apply) |
| 17 | — | — | done |

Phase durations are counted in the object's tick counter (`+0x12`) against a target (`+0x20`).

## 3a. Line (system) pressure [O]

The line-pressure strategy is an object at `0xFFF8BCB0` (update `0xE9CE0`, called through
`0xBABEE`; result read by `0xBAA58`). Its result is sent each cycle as the **Sys** pressure request
(source id `0x15`, `0xDC4C4`) and then converted to current like every other regulator
(spec 12 §2a). Calibration group `0xFFF84D70` (objects 10408–10453; per-gear maps 10358–10375).

**Principle: maximum of demands.** Each demand is offered with `0xEA806(obj, pressure_mbar,
reason)`. The object keeps the largest "normal" demand (`+0x0C`, reason `+0x18`) and the largest
demand of a second class (`+0x0A`, reason `+0x17`, filtered at `+0x50`); `0xEA774` takes the
larger. A forced value with priority (`0xEA84E`, `+0x28`) overrides both. Result `+0x00` (mbar),
`+0x02` = result / 10, `+0x0E` = result − 5,000 if ≥ 5,000.

| Reason | Source | Demand (stock calibration) |
|---|---|---|
| `0x19` | steady state, `0xEA37E` | map 10430 (3 × 3, 6,000 … 10,000 mbar) + temperature curve 10428 (zero) |
| `0x18` | steady state, `0xEA37E` | **per-gear map** (gear code → index table `0x51DE0`; maps 10358, 10360 … 10374): x = [I] transmission input torque `0xFFF8E34A` (−200 … about +1,300 Nm, breakpoints per gear), y = engine speed (2,000 / 3,000 / 7,000 rpm), z = 5,500 … 19,000 mbar; + temperature curve 10427 (zero) |
| `0x11` and from/to gear | during a shift, `0xEA28C` / `0xE9E6A` | maximum of the steady-state demands of both gears, plus a shift demand (`0xE9FF0`) |
| `0x0A` | `0xEA89C`, per element A–E | curve of [I] element (clutch) temperature (`0xEEF78`, fallback fluid temperature): 0 below 450 °C, 8,000 mbar at 500 °C (curves 10432–10436); combined with map 10431 (zero); active when a value exceeds 8,000 (10437) |
| `0x2D` | fluid temperature | curve 10429: 8,000 mbar up to 15 °C, 0 from 20 °C (cold-fluid demand) |
| `0x48` | degraded mode | 9,000 mbar (10450) |
| `0x01` | forced maximum | per-gear maximum (10451: 20,000 mbar) |
| `0x21`, `0x03`, `0x04`, `0x06`, `0x0F`, `0x1A`, `0x31` | individual conditions (fixed values) | 6,000; 6,000; 6,000; 9,500; 10,000; 10,000; 8,500 mbar |
| `0x05`, `0x0C`, `0x13`, `0x1C`, `0x3B`, `0x02` | conditions with their own tables | for example `0x1C` = map 10445 (10,000 flat), `0x13` = curve 10438 (15,000) |
| priority 8 (forced) | `0xEA4C8` first block | 0 mbar ([I] engine not running) |

Then: clamp to the per-gear maximum (`0xB6C9E`, table 10451), selection/filtering (`0xEA2F8`,
`0xEA33E`), and an optional additive offset (curve 10419 × map 10420, engine speed × temperature;
stock zero).

**Conditions of the individual demands** (predicates called in `0xEA4C8`; owner = module that
writes the flag) [O], names [I]:

| Reason | Value | Condition (getter → flag → owner) | [I] Meaning |
|---|---|---|---|
| forced, priority 8 | 0 mbar | input object flags `+0xD4` and (`+0xB2` or `+0xD5` or `0xB80D8` → `0xFFF8B10C`, owner `0xD8122`) | engine not running / transmission not powered |
| `0x02` | hold at limit 8,500 (`0xEAAC4`) | `0xAEA24` → `0xFFF89B8C`, element-adaptation module `0xB3xxx–0xB5xxx` | constant line pressure while an element adaptation is measured |
| `0x31` | 8,500 | `0xAEA5C` → `0xFFF8E6ED`, written by `0x103A58` (shift module) | shift-related condition |
| `0x3B` | value `0xFFF89994` | `0xAEAC0` → `0xFFF89992` | requested by the same interface (`0xFFF8E69A` object) |
| `0x03` | 6,000 | `0xAEB8C` → object `0xFFF89BB4` (`0xB5B0C`: state ≠ 0, 1) | element-adaptation state |
| `0x21` | 6,000 | `0xBA362`: converter state `0xFFF8B5A8[5]` = 3 or 4 (state machine `0xDEA5E`, spec 09 §3) | torque-converter lockup engaged / controlled |
| `0x04` | 6,000 | `0xBA19E` → `0xFFF8AFD4` (state ≠ 0), owner `0xD70DC…0xD7454` | [I] thermal / warm-up mode (see below) |
| `0x05` | table 10418 indexed by `0xFFF8B298` | written by the shift module `0x10B2EA`, also read by the converter (`0xDF922`, `0xDFD2C`) | shift/converter state dependent |
| `0x06` | 9,500 | `0xB8064` → `0xFFF8B101`, owner `0xD8122` | [I] engine start-related state (see below) |
| `0x0F` | 10,000 | `0xBA73C` → `0xFFF8C35C`, written by fault monitor `0xF31B2` (reports fault groups 10 and 104) | fault reaction |
| `0x0C` | element-specific value | `0xEA9EE`: temperature / speed / torque window with hysteresis (calibration `+0x84…+0xA4`) | open |
| `0x1C` | map 10445 | `0xEAB0A`: speed value above threshold `+0x88` (hysteresis `+0x90`) | open |
| `0x13` | curves 10438/10439 | `0xEAB90`: gear `0xFFF89CDC`… vs gear 8, state 3, timer | open |
| `0x1A` | 10,000 | request object state (`+0x19` = 1, `+0x16` ≠ 1, `+0x26`) | hold of the previous high demand |
| `0x2D` | curve 10429 | input flag `+0x2B` = 1 | cold fluid |
| `0x48` | 9,000 | `0xFFF89C3C[0xB8]` = 4 and `0xFFF8BD15` = 0, or `0xFFF8BD74` = 4 | degraded mode |

**Module `0xD8122` (reasons `0x06`, forced zero)** [O], meaning [I]:

- Input: a 4-bit **mode code in CAN `0x2F8` byte 6 bits 3..0** (with check copy), valid when
  byte 0 bit 7 = 1. The safety layer (`0x141346`, object `0xFFF8EA08`) maps it to four 2-bit
  status values (`0xFFF8EA14/15/17/18`) and a state (`+0x1B`): code `0xF` → error, 0 → idle,
  3 → state 3, 1 or 4 → state 4, 2 or 9 → state 5.
- `0xD9B24` turns the status values into the module state (`+0x20`): code 3 → 1, codes 1/4 → 2,
  codes 2/9 → 4, error → 5. The module runs only when input flag `0xFFF89C4F` = 1.
- It compares engine speed (`0xFFF89CD0`) and a second speed (`0xFFF89CB8`) with calibrated
  thresholds (group `0xFFF84CF8`), uses timers, and selects tables by range code (P/N/D/R → index
  0–3). Outputs: `0xFFF8B10C` (part of the forced-zero condition) and `0xFFF8B101` (reason `0x06`,
  9.5 bar).
- [I] **Engine-state handling** (engine stopped / cranking / running, possibly automatic
  stop–start): the code drives line pressure to zero with the engine stopped and requests a high
  pressure in a start-related state. The meaning of the `0x2F8` mode codes needs a capture
  (crank, start, stall, and stop–start if fitted).

**Module `0xD70DC…` (reason `0x04`)** [O], meaning [I]:

- State machine in object `0xFFF8AFA0`, state at `+0x34` (`0xFFF8AFD4`); reason `0x04` (6 bar) is
  active whenever the state is not 0. States 0–6 (`0xD7454`), entry decision in `0xD71B2`
  compares a modelled value with a threshold (`0xFFF8CCF0`).
- It drives a floating-point model class (functions `0x1052E8…0x106610`, object `0xFFF8E2EE`)
  that integrates temperatures (initialised from fluid temperature and input-object values,
  `0x105A98`), and checks the park-block flag `0xFFF89C30`. Calibration: objects 2567–2590
  (group `0xFFF84CC4`) and group `0xFFF84CDC`.
- [I] A **thermal model / warm-up function** that holds line pressure at 6 bar while active.
  Not confirmed.


**For the project firmware** [D]: implement the max-of-demands structure with reason codes (useful
for diagnostics), the per-gear torque × engine-speed map and the temperature-dependent demands;
re-derive all values for the target engine's torque and the stock pump.

## 3b. Fill parameters and adaptation (partial) [O]

**Where the fill values come from (phase entry `0x10FFBE`):**

| Phase | Value | Source |
|---|---|---|
| 1 fill start | duration | either the shift-type table family `0x59370` (when global flag `0xFFF8BD14[2]` = 1), or shift context `+0xC4` + curve(fluid temperature) + map − 0x80 |
| 2 fast fill | pressure / time | `0x111A06`: fast-fill object at context `+0xB0` (`0x112A54`) + a shift-type term (`0x11202C`) or a temperature map − 0x80 |
| 3 fill compensation | pressure | `0x1114CC`: element map (element family, x = context `+0x0C`, y = summary `+0x16`) + shift-type term (`0x11200E`) + requested offset + optional torque-dependent term `0x111D14` (per shift kind); `0x1115C2` alternative |
| 4 fill hold | — | ramp object reset |

**Adaptation storage** (EEPROM block `0x0304`, RAM `0xFFF80E28`, length 0xE10):

- About 200 accessor functions (`0xBC000–0xC0700`), one per stored variable, each guarded by a
  validity check and writing both the RAM copy and the EEPROM image.
- **Per-element state** (elements A–E): five byte arrays at `+0xE8`, `+0xED`, `+0xF2`, `+0xF7`,
  `+0xFC` and two 16-bit arrays; reset per element by `0xB3B4C`, maintained by the element
  adaptation manager (`0xB3xxx–0xB5xxx`, object with state `+0x88`, type `+0x89`, element
  `+0x94`).
- **Trim tables**: three sets, each 448 signed bytes plus seven 16-bit counters: `+0x1B0` / `+0x370`,
  `+0x37E` / `+0x54E`, `+0x550` / …; further sets at `+0x71E`, `+0x8F0` (0x318 bytes). [I] Seven
  counters = one per gear pair (1↔2 … 7↔8), so 64 cells per gear pair. The module `0xBE000–0xBF000`
  only resets, saves and restores these areas.
- A generic access interface (vtable at `0xBA670`: `0xA3D88`, `0xA3E60`, `0xA446E` = array
  pointer by id 1–0x43, `0xA437E` = element size by id, …) gives indexed access to all adaptation
  arrays; [I] it serves diagnostics (read/reset adaptation values) and possibly the learners.
- **Learners** (update rule pattern, for example `0xCE038`): a table-driven state machine adds
  calibrated increments, then filters towards the target
  (new = old + (target − old) × w1 / (w1 + w2)), clamps to calibrated limits and stores through
  the accessor. Learner clusters: `0xA12FC–0xA2C88`, `0xCC9EE–0xCF48C`, `0xE0868–0xE2F46`
  (converter / lockup area), `0xD3D12`, `0xD48AE`, `0xE8058`.

**Open:** the path from the stored trims into the fill values above (shift context `+0xB0`,
`+0xC4`, element map offsets) was not found by direct references; it most likely runs through the
generic access interface (virtual calls). Mapping each learner to the quantity it learns is also
open.

## 4. For the project firmware

- Reproduce: the element matrix (§1), the per-element phase engine with the three sequences
  (§3), and table selection by gear pair and shift type (§2).
- Re-derive pressure levels for the actual hardware (solenoid characteristic, line pressure)
  and for the target vehicle's engine torque.
- [I] Fill timing is normally adapted over time (learned values). Where the stock firmware stores
  them (candidate: EEPROM application blocks `0x0304/0x0309/0x0310`) is open.

## 5. Open items

1. Role of each of the 8 pressure maps per gear pair and of the 43-type blocks.
2. Assignment of shift kinds 0–3 (power-on/off, up/down).
3. Solenoid → driver channel mapping: confirmed in code (spec 12 §2a); bench check pending.
   The pressure → current path (requests, curves, temperature correction) is in spec 12 §2a.
4. Fill-time adaptation: storage and learner pattern in §3b; link from trims to fill values open.
5. Engine torque reduction during the inertia phase ([I] via the 13-bit values in CAN `0x037`
   and `0x2F9`, spec 08).
6. Special rows of the element table before `0x505E0` (garage shifts, limp-home).
7. Element pressure limits: not in the repair manual (§1a). Line pressure: strategy in §3a
   (5.5 … 20 bar in the stock calibration); conditions of the fixed-value demands open.
8. Element-combination fault monitors (P1D8F…P1DA8, one DTC per element, pair and gear
   triple): spec 13 §2b.
