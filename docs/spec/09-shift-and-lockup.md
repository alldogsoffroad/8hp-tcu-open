# Spec 09 — Shift strategy and torque-converter lockup

Status: **draft.** The calibration access scheme, the shift-point lookup and gear decision, the
driver-type blending, and the lockup request, engagement and slip-control chain are established. Several input
signals (which RAM value is pedal, turbine speed, temperature) and the meaning of most shift
programs are **open**.
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference, **[D]** derived.

Calibration values quoted here are Bosch/ZF work product. Per the project conventions (README),
they are given only to explain structure and must be **re-derived or changed** before use in the
project firmware. `cal_catalog.py` (project root) reads every table from the user's own dump, so
the project does not need to store them.

Fuel-economy relevance: the shift points (§2) and lockup engagement (§3) are the two levers that
decide engine speed at a given road speed. Both are fully table-driven; their **structure** is
what the project firmware needs to reproduce, while the **values** will be re-calibrated for the
target vehicle (vehicle-side work has not started; see README).

## 1. How the application reaches calibration data

### 1.1 Object index and group pointers

- The calibration block holds an **index of 12,198 object addresses** at `0x180280–0x18C117`
  [O]. The application holds an identical copy at `0x56C90` (application copy entry *i* is at
  `0x56C90 + 4i`, calibration copy at `0x180280 + 4i`) [O].
- Objects are grouped by function. Each group has a **RAM pointer** (`0xFFF84D0C`,
  `0xFFF84E24 … 0xFFF84F64`, …) that points either at the application copy or at the
  calibration copy of the group's slice of the index. Small setter functions (pairs at
  `0x11DA78 … 0x11DBxx`, `0x107250/0x107258`) switch it [O]. `0xF7950` calls all setters of one
  kind when the selector byte `0xFFF8A075` changes: 0 selects the application copy, 1 the
  calibration copy [O]. [I] The application copy is a built-in default data set; what sets the
  selector (calibration validity, coding) is open.
- Code reads a table as `(*group_ptr)[k]`, so tables have **no direct cross-references**; the
  group pointer and offset `k` identify them.

Groups used here:

| RAM pointer | Index range | Content | Users |
|---|---|---|---|
| `0xFFF84EB0` | 11642–11748 | shift maps (§2.1), program fallback table, flags | `0x1277D8–0x12973C` |
| `0xFFF84E74` | 11381–11399 | driver-type level curves (§2.4) | `0x121ED0–0x122164` |
| `0xFFF84EBC` | 11751–11806 | manual-mode (tip) shift limits (§2.5) | `0x12EEB0–0x130284`, `0xFB4B0` |
| `0xFFF84F08` | 11944–11946 | rev-limit forced upshift (§2.5) | `0x122536`, `0x12B4EE` |
| `0xFFF84F5C` | 12151–12194 | lockup strategy: request level 0–3 (§3.1) | `0x124570–0x1251BC` |
| `0xFFF84D08` | struct `0x53FAC` | converter control: demand, inhibits, engagement (§3.3) | `0xDE000–0xE3100` |
| `0xFFF84D0C` | 2920–3079 (struct `0x54198` / `0x183020`) | converter slip control (§3.4) | `0xE0440–0xE2B7C`, `0x10A930–0x10C640` |
| `0xFFF84D10` | struct `0x54680` | converter state-machine thresholds (§3.3) | `0xDE000–0xE3100` |

The whole shift-program module uses the groups from index 10,969 upward (code
`0xF7F50–0x1318D8`) [O].

### 1.2 Table formats [O]

All values are big-endian.

| Type | Layout | Lookup |
|---|---|---|
| curve | `u32 n`, `s16 x[n]`, `s16 y[n]` | linear interpolation, clamped at the ends |
| map | `u32 nx`, `u32 ny`, `s16 x[nx]`, `s16 y[ny]`, `s16 z[ny][nx]` (rows = y) | bilinear interpolation (`0x121A56`), clamped |
| byte / word table | `u32 n1`, `u32 n2`, then `n1 × n2` bytes or words | `t[b × n1 + a]`, out of range → 0 (`0xB6CBC` bytes, `0xB6D14` words) |
| byte array | `u32 n`, `n` bytes | `t[i]`, index check (`0x121432`, `0xB6C8A`) |
| scalar | 1 or 2 bytes in `0x1FE000–0x1FFD5F` | — |

Some objects carry 1–3 pad bytes after the data.

## 2. Shift strategy

### 2.1 Shift maps [O]

105 maps (group `0xFFF84EB0`, offsets 0–104) share one format:

- **x = shift index 1–14**: 1–7 = upshifts 1→2 … 7→8; 8–14 = downshifts 2→1 … 8→7.
- **y = pedal position**, 12 points on 0–255 (= 0–100 %).
- **z = output-shaft speed in rpm** at which the shift is requested.

Check [D]: the full-pedal upshift values times the 8HP gear ratios give about 5,000–5,600 engine
rpm for every gear, consistent with full-throttle shifts near the engine's speed limit. Gear ratios: 4.70, 3.13, 2.10, 1.67, 1.29, 1.00, 0.84, 0.67 (ZF 8HP70 repair manual,
spec 11 §1a).

For a current gear *g*, `0x128D00` gives the upshift column *g* (for *g* = 1…7) and the
downshift column *g* + 6 (for *g* = 2…8) [O].

### 2.2 Program selection [O]

`FUN_001290A8(obj, out, program, column, pedal)` maps a **program number** (0–99) to one of the
105 maps and interpolates it:

- Programs **0–24** form a 5 × 5 grid: `program = 5 × level + base`, using map offset
  `5 × base + level`. *base* (0–4) is a driving situation; *level* (0–4) is the driver-type
  level (§2.4).
- Programs 25–99 map one-to-one to further maps (offsets 25–104; a few numbers are unused and
  return error 0x38). Programs 47–49 use a per-variant table (`0xFFF8D4A4/5` select an entry).
- A byte table at group offset `+0x1A4` (index 11747, 100 entries) **remaps** program numbers;
  uncalibrated programs fall back to program 5 [O].

The meanings of *base* 0–4 and of programs 25–99 are **open** [I: normal, warm-up, altitude/hot,
tow/haul, manual range, cruise control and limp-home are the usual candidates]. Observed
differences: bases 3 and 4 shift much earlier (full-pedal 1→2 at about 2,700 engine rpm); many
programs from 55 upward use pedal-independent maps.

### 2.3 Blending and gear decision [O]

- `0x128D9C` evaluates up to **four programs** and blends them bilinearly with two weights in
  percent: `a = p1 + w1·(p2 − p1)`, `b = p3 + w1·(p4 − p3)`, `result = a + w2·(b − a)`.
  Program code `0x21` means "same as the previous one".
- `0x128E94` applies further corrections to the threshold (`0x1281A6`, `0x128894`, `0x128C04`;
  [I] hysteresis, grade/load and temperature offsets — open) and stores up/down thresholds in the
  shift object (`+0xD8` up, `+0xDA` down).
- **Gear decision** `0x1277D8`: starting from the current gear, it compares the output speed with
  the up and down thresholds and steps the target gear by one per iteration (at most 8 steps),
  so skip shifts arise naturally. Extra conditions limit multi-step jumps and handle kickdown
  (`+0xB6`) and forced shifts (`+0xDE = 3`, e.g. speed limits at `+0xE6/+0xE8/+0xEA/+0xEC`). The
  result is written to the caller's target-gear byte [O].
- About 25 callers (`0x129B70 … 0x12D7CC`, `0x1302A8`) are situation handlers. Each picks the
  programs and weights and calls the gear decision [O].

### 2.4 Driver-type level (adaptive shifting) [O]

- A **sportiness value** (s16 at RAM `0xFFF8AB58`) is converted by a calibrated curve (group
  `0xFFF84E74`; a calibration byte selects one of five curves) into a level of 0–400, i.e.
  0.00–4.00 [O]. The value is written through an object pointer (no direct reference); the
  writer has **not been found** yet. It is listed in the 470-entry RAM measurement table at
  `0x635F4`, together with neighbouring driver-type values [O]; [I] that table may allow reading
  it live over diagnostics on the bench.
- `0x122066` splits the level into a lower level, an upper level and a weight `level % 100`, and
  picks the integer level with a hysteresis band of `50 ± calibration offset` percent [O].
- The situation handlers call `5 × level + base` for both neighbouring levels and blend them with
  that weight, so the shift points move smoothly between economy and sport [O].
- Observation (base 0, 10 % pedal): level 1 shifts earliest (1→2 at about 1,130 engine rpm),
  level 2 later, levels 3 and 4 later still; level 0 equals levels 3–4 [O][D]. [I] Level 0 may
  be a start-up or default level.

### 2.4a Strategy inputs from FCA documentation (leads) [M]

The FCA *8-Speed Automatic Transmission Diagnosis and Repair* student workbook (2015), user's copy names the inputs of the driving-program and driver-type logic. These
are **leads** for the meanings of bases 0–4, programs 25–99 and the sportiness value:

- Driver-type assessment: drive-off behaviour, pedal (SISP), engine torque, kick-down,
  longitudinal and lateral acceleration.
- Driving programs: city, rolling resistance (drag / grade), low friction, cruise control,
  warm-up, high temperature, ESC intervention, program switch; plus manual selection, fast-off
  and corner detection.
- **Warm-up** when engine coolant < 30 °C (driver type and drag calculation disabled).
  **Hot mode** when oil > 120 °C: warning lamp, later shifts and changed lockup to cool the
  fluid (matches the 120 °C limits in specs 04 and 11).
- **Neutral idle control**: brake B slipped (slip = engine − turbine), C pre-closed; exits on
  pedal, output rotation or brake release.
- Low friction: higher gear, fewer downshifts at speed. "Low": top gear limited. "Sport":
  aggressive map; paddles switch to manual (in D only temporarily).

- **Direct (skip) downshifts** listed in the ATRA *ZF8HP Introduction* webinar (2015), user's copy [M]: 8→6, 8→4, 8→2;
  7→5, 7→1; 6→4, 6→3; 5→3, 5→1; 4→2; 3→1. Cross-check for the gear decision of §2.3, which can
  step several gears in one pass.

### 2.5 Related limits [O]

- **Manual (tip) mode** group `0xFFF84EBC`: 9 maps of the same 14-column format, constant over
  pedal. Upshift columns give the minimum output speed for an upshift and downshift columns the
  maximum speed for a downshift ([I] over-rev protection), plus 7 × 6 maps of related offsets.
- **Rev-limit upshift** group `0xFFF84F08`: two 7-column maps (gear × pedal) of output speeds at
  which an upshift is forced; these correspond to about 5,600 engine rpm in every gear [D].

## 3. Torque-converter lockup

Lockup is split into two layers [O]:

- the **lockup strategy** (shift-program module, §3.1) decides *how much* lockup is wanted:
  level 0–3;
- the **converter control module** (§3.2–3.4) turns that request into a demand, runs the
  engagement state machine and controls slip.

### 3.1 Lockup strategy — request level 0–3 [O]

`FUN_00124570` (object `0xFFF8AB90`, calibration group `0xFFF84F5C`, objects 12151–12194)
runs once per cycle and produces a **request level 0–3** (0 = open, 3 = fully closed). The
request reaches the converter module as `0xFFF8AB94 → 0xFFF8A0A1 → 0xFFF89CEC` (copied by
`0xF7864` and by the converter input interface `0xB912C`, §3.2).

- **Same programs as shifting.** It uses the four program numbers and two blend weights of the
  shift object (`+0xC7…+0xCC`, §2.3). A remap table (`+0x00`, 100 entries) reduces each program
  to a lockup family. In this calibration there are only two families.
- **Allow table** (`+0x04`, 8 gears × 100 entries): gives a lockup map index *p* (1–90) for
  each gear and family; 0 = no lockup in that gear. All four programs must allow lockup, or
  the request is 0.
- **Threshold maps** (`+0x24 … +0x44`, 9 maps): map number `(p − 1) / 10`, column
  `((p − 1) mod 10) × 4 + t`, row = pedal (10 points on 0–255); value = **output speed (rpm)**,
  10000 = never. Transition *t*: 0 = 1→0 release, 1 = 0→1 engage, 2 = 1→2, 3 = 2→3; 2→1 and 3→2
  use columns 2 and 3 minus fixed hysteresis offsets (`+0x0C`, `+0x10`).
- The four program thresholds are blended exactly like shift points (`0x1249DA`), and optional
  per-gear offsets are added (`+0x48/+0x4C/+0x50` arrays, selected by transition).
- **Level stepping:** in level *n*, the request rises to *n* + 1 when output speed ≥ the
  "up" threshold and falls to *n* − 1 when output speed < the "down" threshold.
- **Inhibits** (`0x124DDE`, `0x124ED4`, `0x12501E`, `0x1250F4`): a bit set forces level 0,
  with these reasons: a calibration enable bit, an external request, a temperature or
  engine-speed condition (bit 3), a load condition with hysteresis (bit 4), a time-limited
  condition (bit 5), and a diagnostic condition (bit 7). Three special cases override the
  map-based level with fixed values (`+0x14/+0x18/+0x1C`).

Stock calibration [O][D]:

- Lockup is allowed in **every gear, including 1st**.
- Level 1 engages at about **1,000–1,250 engine rpm** in every gear (for example 1st at 260
  output rpm, 8th at 1,475 output rpm), with 30–150 rpm hysteresis.
- Level 2 engages close to level 1 at medium pedal, but much later at light pedal.
- Level 3 is never requested by the maps.

### 3.2 Converter control module — inputs [O]

The module sits in `0xDE000–0xE3100` and `0x104C00–0x10CA00`, with main task `0xE0FE4`.
Calibration groups are `0xFFF84D00 … 0xFFF84D1C`; the main ones are `0xFFF84D08` (struct
`0x53FAC`) and `0xFFF84D10` (`0x54680`).

- `0xB912C` copies about 70 inputs from other modules through getter functions into the block
  `0xFFF89C3C…`. The lockup request is `0xFFF89CEC`; the temperature byte is at `+0x96`.
- `0xE1AF6` derives the speed block `0xFFF8B5FC` (filtered, every task):

| Offset | Content |
|---|---|
| `+0x00` | engine speed (rpm) |
| `+0x10` | turbine speed (rpm) |
| `+0x08` | converter **slip** = engine − turbine (rpm); float copy at `+0x1C` |
| `+0x12` | output speed (rpm, minimum 32) |
| `+0x14` | turbine/output speed ratio |

- `0xDFAAE` sets the **drive/coast flag** `0xFFF8B525`: 1 when converter torque
  (`0xFFF8B542`) ≥ +100, 0 when ≤ −100, and slip-sign hysteresis in between. Slip setpoints are
  negated in coast.

### 3.3 Demand and engagement state machine [O]

- **Demand** `0xFFF8B526` (0–3) is set by `0xDFD2C`: the strategy request mapped through a
  byte table (`0xFFF84D08 + 0x24`; identity in this calibration), then limited by inhibit flags
  from `0xDFE68`. Those flags force the demand to 0, 1 or 2 and record a reason code: open
  request, minimum engine/turbine speed per gear (tables `+0x144`/`+0x148`), temperature,
  low-speed/torque conditions and diagnostics.
- **Operating state** `0xFFF8B524` = `5 × mode + sub-index` (40 values, `0xDF922`).
- **Engagement state machine** `0xDEA5E` (12 states in the module object, run from `0xDE688`).
  The main path is:
  1 → 2 → 3 → 4 → 5/6/8 ⇄ 7 / 9 / 10, with 11/12 as exit states; most transitions back to 1
  leave the clutch open [O]. [I] State names: 1 open, 2–3 filling, 4 engaging, 5–10 slip control
  sub-modes, 11–12 release/error.
  - Entry 1→2 (`0xDF3E4`) needs a demand, engine speed > 500 rpm (`+0x13C`) and turbine speed
    > 750 rpm (`+0x140`), timers expired, slip inside the calibrated window and no inhibit.
  - The other guards check timers, slip thresholds, a demand change or the drive/coast flag.
- **Engagement permission** `0x10AC86` is one additional path (per-gear limits, ATF
  temperature window `+0xF0 < T < +0xEC`, range D/M). Its per-gear enable table is all zero in
  this calibration, so that path is inactive.

### 3.4 Slip control [O]

`0x10C402` selects one of **74 situations** (table chain `+0x24`, `+0x28`, `+0x2C` of
`0xFFF84D0C`: gear status 1–8 / 10 / 11 / 20 / 22 × operating mode). Each situation has a
**slip-target map** (list `0x5442C`, objects 3006–3079; rpm against engine speed and torque,
stock values 10–80 rpm), a ramp rate (`+0x30`) and a gain (`+0x34`). `0x10BFC2`, `0x10A968` and
`0x10AB02` rate-limit the target and add per-gear feed-forward maps and an open-converter
model (object 2950). Pressure output to the actuator is in the driver layer (bench item 3).

## 4. Implications for the project firmware (fuel economy)

- **Shift points:** reproduce the map format (§2.1), the program/level blending (§2.2–2.4) and
  the stepping gear decision (§2.3). For economy, the main parameters are the light- and
  part-pedal upshift points and the driver-type level. A fixed or capped level gives
  predictable early upshifts. Limits: avoid lugging (minimum engine speed per gear under load)
  and keep downshift hysteresis to prevent hunting.
- **Lockup:** reproduce the request-level strategy (§3.1), the demand/inhibit layer and
  engagement state machine (§3.3), and slip control by situation (§3.4). The stock calibration
  already locks up early, at about 1,000–1,250 engine rpm in every gear. For economy, the
  remaining levers are using level 2–3 (less slip) at light pedal, and keeping lockup engaged
  through shifts where the hardware allows. Limits: converter clutch heat (slip × torque),
  driveline vibration at low engine speed, and the temperature window. On the target vehicles,
  a lower-revving engine may need the engagement speeds raised, not lowered.
- **Re-calibration is required** in any case: the Nissan targets differ in engine torque curve,
  axle ratio and tyre size, so output-speed shift points must be re-derived (for example from a
  target engine speed band and the new final drive), not copied.

## 5. Open items

1. Which RAM values are pedal, engine speed, turbine speed, output speed, engine torque and ATF
   temperature (trace the input-gathering routine, spec 07 §4; or read them over diagnostics
   while driving).
2. The sportiness calculation that feeds `0xFFF8AB58` (driver-type evaluation).
3. Meaning of shift bases 0–4 and programs 25–99, and which situation handler runs when.
4. Correction functions `0x1281A6`, `0x128894`, `0x128C04` (hysteresis, grade, temperature).
5. Lockup: the exact meaning of request levels 1–3 in clutch terms (slip versus closed), the
   remaining state-machine guards (`0xDF58C … 0xDF7A6`), and the units of the converter torque
   input `0xFFF8B542`.
6. Shift execution (clutch pressures and phases) is covered in spec 11.
