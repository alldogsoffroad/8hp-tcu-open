# Spec 14 — Park/range module (park target arbitration)

Status: **draft.** The module structure, its input struct, the park-target arbiter and its
redundant safety-layer copy are established [O]. The meaning of several input fields and the
exact range-code names are **inferred** and marked [I].
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference, **[D]** derived.

Calibration values quoted here are Bosch/ZF work product (README conventions); they show the
structure and must be re-derived before use.

Related: spec 12 §1.2 (park hold / release outputs), spec 13 §4.1 (P1B14, P1B13).

## 1. Overview [O]

Code range `0xA4xxx–0xA9xxx` plus monitors in `0xC2xxx–0xC8xxx`. One cycle, called from the
task at `0x9B306` / `0x9B39E`:

1. **Input copy:** `0xA7C9C(0xFFF8D61A)` fills the module's input struct (§2) from about 120
   interface slots (`0x4B6xx–0x4B8xx`), one getter per field. `0xA81D8` fills the remaining
   fields (`+0x7C…+0x81`).
2. **Derived inputs:** `0xA8EFE(0xFFF8D5E6)` evaluates the input block (§3).
3. **Arbitration:** `0xA548C` → `0xA557C` computes the **park target** (§4) and stores it
   with an inverted copy at `0xFFF8D6D6` / `0xFFF8D6D7`.
4. **Output:** `0xC709C` acts on the target (towards the park actuators, spec 12).

The park logic assumes an **E-Shift (shift-by-wire) valve body**, on which the park solenoid
drives the position and park-lock valves; on an M-Shift valve body that solenoid has no
function [M] (spec 12 §1, source item 6). Relevant when sourcing a valve body for a conversion.

All persistent values are stored with an **inverted copy** (`x`, `~x`) and checked on read,
as in the rest of the firmware.

## 2. Input struct `0xFFF8D61A` [O]

Fields used by the logic in this spec (getter → origin):

| Offset | RAM | Width | Source getter → origin | Use / [I] meaning |
|---|---|---|---|---|
| `+0x01` | `D61B` | u8 | `0x1369AC` | enables the calibration override of the lever request (§4.3) |
| `+0x06` | `D620` | u8 | `0xA3960` → `*0xFFF8BC70` | **P1B14 failure byte 2**; origin open |
| `+0x0D` | `D627` | u8 | `0x14CE1C` → `0xFFF8EADE` | lever/request plausibility; value 2 blocks request handling |
| `+0x15` | `D62F` | u8 | `0x14926C` → `0xFFF8EA70` | blocks the range request in `0xC7C0C` when 1 |
| `+0x27/+0x28` | `D641/D642` | u8 | `0x140AE8` / `0x140AF0` | auxiliary park request state (§3) |
| `+0x2C` | `D646` | u16 | `0x14BB90` → `0xFFF8EE6E` | **output speed (rpm)** [O] (same value feeds the application's output speed, spec 07 §7.1); compared to calibration 80 |
| `+0x2E` | `D648` | u8 | `0x141AD4` → `0xFFF8EA40` | **P1B14 failure byte 1**; park request from the safety layer (§5) |
| `+0x2F/+0x30` | `D649/D64A` | u8 | `0x1394A4` / `0x1394AC` → `0xFFF956E9/EA` | [I] direction of travel (1 / 2) and its state |
| `+0x36` | `D650` | u16 | `0x14CE16` → `0xFFF8EADC` | [I] speed; compared to calibration 320 |
| `+0x38` | `D652` | u8 | `0x141AE4` → `0xFFF8EA42` | qualifier of `+0x2E` (0 = valid) |
| `+0x3A` | `D654` | u8 | `0x13722C` | → `D5F9` (§3) |
| `+0x3E` | `D658` | u16 | `0x7C400` → `0xFFF93922` (copy of `0xFFF94ED0`) | P1B14 test-enable limit; [I] supply voltage |
| `+0x43…+0x51` | `D65D…D66B` | u8 | `0xA031C(id)` | fault-status flags of other components (ids from the identity table, spec 13 §2) |
| `+0x58` | `D672` | u8 | `0x13CC64` → `0xFFF8E964` | **range request** (values 5–8, §4.1) computed in the safety layer |
| `+0x59` | `D673` | u8 | `0x13CC44` → `0xFFF8E95D` | second request value from the safety layer |

The full field list (106 entries) can be regenerated from `0xA7C9C`; the safety-layer sources
(`0x13xxxx–0x14xxxx`) are themselves computed by that layer, not raw inputs.

## 3. Derived inputs `0xFFF8D5E6` [O]

`0xA8EFE` runs twelve evaluators on the block `0xFFF8D5E6`:

| RAM | Set by | Rule |
|---|---|---|
| `D5F2` (`+0x0C`) | `0x9B306` | auxiliary park request active: `D61A[0x27]` ∈ {1, 2, 3} and state `D5F3` < 2 (or = 2 with `0xFFF893E8` ≠ 0) |
| `D5F3` (`+0x0D`) | `0x9B306` | request state 0 / 1 / 2 from `D61A[0x27]`, `[0x28]` and a calibration flag (`0xFFF84B14 + 0x88`) |
| `D5F4` / `D5F5` | `0x9B306` | rising / falling edge of `D5F2` |
| `D5F6` (`+0x10`) | `0xA9086` | copy of `D650` (speed) |
| `D5F8` (`+0x12`) | `0xA9126` | **= 1 when `D61A[0x2E]` = 1 and `[0x38]` = 0** (valid park request from the safety layer) |
| `D5F9` (`+0x13`) | `0xA908E` | `D654` ≠ 0 |
| `D5FA` (`+0x14`) | `0xA8F38` | last valid range request (only 5, 6, 7 or 8 are accepted) |
| `+0x15` | `0xA9144` | latch while `D673` = 2 |
| `+0x19…+0x1B` | `0xA9190` | three-level state machine from temperature/pressure-type thresholds (calibration `0xFFF84B14`); [I] hydraulic readiness for park release |
| `+0x20`, `+0x21`, `+0x22`, `+0x23`, `+0x24` | `0xA9668`, `0xA90A0`, `0xA96DE`, `0xA902E`, `0xA9748` | auxiliary flags (`+0x22` = target ≠ previous target; `+0x23` = Park left without request) |

## 4. Park target arbitration [O]

### 4.1 Codes

The **range request** (lever object, §4.3) uses the values **5, 6, 7, 8**; other values are
treated as "no valid request". **5 = D, 6 = N, 7 = R, 8 = P** [O]: confirmed by the shifter
decode table (§5a), by the P1B13 monitor and by the direction checks in `0xA52A2` / `0xA5352`.

The **park target** `0xFFF8D6D6` written by the arbiter only takes the values **8** (engage
Park), **6** (Park released) and **5** (released, drive-related special case). A reason code
(`0x01…0x17`) is stored alongside in the result object (byte 2).

### 4.2 Arbiter `0xA557C`

Calibration group `0xFFF84AB0` (ROM struct `0x4ED14`, values at `0x195FD4…`):

| Offset | Stock | Role |
|---|---|---|
| `+0x00` | 80 | speed limit `L1` (compared with `D646`) |
| `+0x08` | 320 | speed limit (compared with `D650`, `0xA54A4`) |
| `+0x0C` | 320 | speed limit `L2` (compared with `D650`) |
| `+0x18` | `0xFF` | lever-request override value (`0xFF` = off), §4.3 |
| `+0x1C`, `+0x24`, `+0x28` | 0, 1, 1 | enables of the special cases in `0xA54A4` |
| `+0x20` | 1 | enable of the "stay released while D and moving" case |
| `+0x2C` | **4** | arbitration mode (selects §4.2 step 5 variant) |
| `+0x30`, `+0x34` | 1, 1 | enables of the forced-release cases |

Order of evaluation:

1. `0xA54A4` (special cases): target **5** or **6** when the lever request and direction
   checks match and speed `D650` < 320 (reasons 1–3).
2. Forced release **6** (reason 5) when `+0x30` = 1, lever flag `+0x20` = 1 and `D650` > 320.
3. Forced release **6** (reason 6) when `+0x34` = 1, lever request = 0, `+0x10` = 0 and
   `D650` > 320.
4. Target **5** (reason 8) when `+0x20` = 1, lever request = 5, `+0x10` = 1 and `D650` > 320.
5. Otherwise by mode `+0x2C`: 0 → `0xA56A4`, 1 → `0xA56D8`, 2 → `0xA56FA`, 3 → `0xA5778`,
   other → `0xA57DA` (**stock: 4 → `0xA57DA`**).

`0xA57DA` (active in stock calibration):

- lever request = 8 → **8** (reason `0x15`);
- else, if `D646` > 80 (moving) **or** `D5F8` ≠ 1 (no valid park request):
  - lever request 7, 6 or 5, or moving → **6** (reason `0x16`),
  - otherwise (stationary, no valid lever request) → **8** (reason `0x17`);
- else (stationary **and** `D5F8` = 1) → **8** (reason `0x15`).

So with the stock calibration, Park is engaged when the driver selects P, when the vehicle is
stationary without a valid lever request, or when the safety layer reports a valid park
request while stationary. [I] The last case matches FCA's **Auto Park** behaviour (Park engaged
automatically when stopped, e.g. driver leaving); not confirmed.

The other mode variants differ in how speed and `D5F8` are weighed (for example `0xA56D8`
uses `D5F8` only).

### 4.3 Lever request object `0xFFF8304C` [O]

Inverted-copy fields, each read through a checked getter:

| Field | Getter | Writer | Content |
|---|---|---|---|
| `+0x00` | `0xA51E4` | `0xA768C` (via `0xA7C00`) | lever range request (5–8). If `D61B` = 1 and calibration `+0x18` ≠ `0xFF`, the calibration value replaces the request (test/override); the result is mirrored to `0xFFF8E571` |
| `+0x08` | `0xA51F4` | `0xA7C1A` | second request value |
| `+0x10` | `0xA5204` | `0xA52A2` | 0 / 1 / 2: request plausible with direction of travel (1 = D-side, 2 = R-side) |
| `+0x18` | `0xA5214` | `0xA5352` | 0 / 1 / 2: direction-mismatch flag (default 2) |
| `+0x20` | `0xA5224` | `0xA5448` | 1 when `D5F9` = 1 and `D650` > 320 |
| `+0x28/+0x29` | byte | `0x9B6C8` / `0x9B6D4` | status bytes, mirrored to `0xFFF83080` |

The park arbiter's lever input comes from CAN as described in §5a (via `D61A[0x58]`). The
only writer of this object's `+0x00` found so far is the safety-layer reset (§6); whether the
object is also filled from `E964` on another path is **open**.

### 4.4 Third arbiter `0xC7C0C`

Used by the monitors. Input is the range value `0xFFF8D6CE` (getter `0xA7966`), active only
when `0xFFF893E8` is 1 or 2 and `D62F` ≠ 1: value 7 → 7 (or 6 when the reverse conditions on
`0xFFF8D604` and lever `+0x10` fail), value 5 with conditions (`0xFFF8D605`, lever `+0x10` = 1)
→ 5, otherwise 6.

## 5. Safety-layer inputs [O]

- `0xFFF8EA40` (`D61A[0x2E]`) and `0xFFF8EA41` are produced by `0x141A2C` from the object at
  `0xFFF8EA38`: depending on a calibration flag, from status byte `0xFFF8EA3C/3D`
  (`0xFFF84A1F/20`, written by the status handler `0x7C4F6`, which also reports **fault group
  33**) or from `0xFFF8EA3E/3F` (`0xFFF92FE6/7`). Value 2 in `+9` marks the input invalid.
- `0xFFF8E964` (lever request) and `0xFFF8E95D` (source status) are computed by the
  safety layer's shifter evaluator from two redundant CAN messages (§5a).
- These sources are part of the layer's own computation; the gatherer `0x14FC5C` does not copy
  them.

## 5a. Lever request from CAN [O]

The lever position reaches the park module (`D61A[0x58]` = `0xFFF8E964`) through the safety
layer's **shifter evaluator** `0x13CD80` (program-flow checkpoint `0x37`), object
`0xFFF8E91C`; `E964` = object `+0x48`, `E95D` = `+0x41`.

**Two redundant sources.** The shifter message is received twice, once on CAN-C (bus 1) and
once on the dedicated shifter bus (bus 2). Which pair is used depends on a **shifter-type
variant** byte `0xFFF9531E`:

| Variant byte | Type | Channel A (CAN-C, bus 1) | Channel B (shifter bus, bus 2) | Lever code format |
|---|---|---|---|---|
| 9, 10, 12, 13 | 0 | `0x2FA` byte 0 | `0x200` byte 0 | low nibble = code, high nibble = complement |
| 6, 7, 11 | 1 | `0x2EA` byte 0 bits 2..0 | `0x210` byte 0 bits 2..0 | 3-bit code, checked against an inverted second copy kept by the receive driver |
| 14 | 2 | as type 1 | as type 1 | as type 1; also uses `0x2EA` byte 0 bits 5..3 |

All four messages are DLC 5, period 2 / timeout 5 (raw units, spec 07 §3). `0x2EA` and `0x2FA`
carry CRC-8 and a rolling counter (spec 07 §2). Bus-2 handles: `0x44` = `0x210`, `0x45` = `0x200`
(mailbox table `0x43CE4`, `0x43CF8`).

**Variant of this TCU:** `0xFFF9531E` is copied (`0x137668`) from EEPROM block `0x0310`, offset
`0x3A`, when calibration flag `+0x20` = 1 (stock: 1); otherwise from calibration `+0x18`
(stock: 14). The user's EEPROM dump holds **11** (complement `0xF4` follows), so this unit is
**type 1: lever code from `0x2EA` (CAN-C) and `0x210` (shifter bus)**. [I] Block `0x0310` is
written through a UDS `0x31` routine, i.e. the shifter type is a configuration set at the
plant or by a scan tool.

**Source selection** (`0x13DB34`, calibration mode at object calibration `+0x08`, stock **1**):

| Mode | Behaviour |
|---|---|
| 0 | prefer B, fall back to A |
| 1 | **prefer A (CAN-C), fall back to B (shifter bus)** — stock |
| 2 | A only |
| 3 | B only |

A source is usable when its message status byte is 0 (no timeout / integrity error) and its
code passes the table check below. `E95D` = 0 (preferred source used), 1 (fallback used),
2 (no valid source).

**Decode table** `0x646D8` (7-byte entries, `type × 0x70 + code × 7`; byte 0 = range request
`E964`, byte 5 = 0 marks the code invalid). Types 1 and 2:

| Lever code | Range request | Meaning |
|---|---|---|
| 0 | — | no change (last value kept) |
| 1 | 8 | **P** |
| 2 | 7 | **R** |
| 3 | 6 | **N** |
| 4 | 5 | **D** |
| 5 | 5 | D with an extra flag (type 1: byte 6; type 2: byte 4) — [I] manual/low range |
| 6 | — | no change |
| 7–15 | invalid | error codes in byte 1 (`0xFA…0xF7`) |

Type 0 maps codes 2–8 to `0x15…0x20` instead of 5–8; [I] a different (monostable lever)
shifter whose positions are translated elsewhere. Not used by this unit.

Integrity: every field of the evaluator object is held with an inverted copy; any mismatch
calls the fatal handler (spec 13 §2).

**For a replacement shifter / vehicle swap** [D]: to drive the stock controller, a gateway must
send `0x2EA` on CAN-C (lever code in byte 0 bits 2..0, valid CRC-8 and counter) and/or `0x210`
on the shifter bus, at the 2-tick period. Other fields of `0x2EA` (byte 0 bits 6..3, byte 1,
byte 3) are read too; their meaning is open.

## 6. Redundant check in the safety layer [O]

`0x158C3E` (safety layer, object `0xFFF83080`):

1. `0x158B9C` snapshots the five lever-object getters (§4.3). When a getter differs from its
   snapshot, it writes the default back through the lever-object setters (`+0x00` → 0, `+0x08`
   → 0, `+0x10` → 0, `+0x18` → 2, `+0x20` → 0).
2. `0x158D86` / `0x158C60` recompute the park target with the same rules as §4.2, from
   calibration `0xFFF85350` and speed `0xFFF971D4`.
3. The result is compared with the application's target (`0xA7B78` = `D6D6`); a mismatch is
   returned to the monitor.

For the project firmware this means the park decision is computed **twice** by independent
code paths and cross-checked.

## 7. Monitors using this module

- **P1B14-00** (group 113): spec 13 §4.1. Failure when `D61A[0x2E]` = 1 and `[0x06]` = 1.
  With §3 and §4.2: `[0x2E]` is the safety layer's park request that also drives `D5F8`.
- **P1B13-00** (group 114): watches transitions of `D6D6` to 8.

## 8. Open items

1. Origin and meaning of `D61A[0x06]` (`0xFFF8BC70`).
2. Lever request: CAN source resolved (§5a). Open: the normal writer of `0xFFF8304C + 0`, the
   meaning of the other `0x2EA` / `0x210` fields, and of lever code 5.
3. Unit of `D650` (limit 320); `D646` is output speed in rpm, so the 80 limit is 80 output rpm
   [O].
4. Meaning of `D61A[0x27]` / `[0x28]` (auxiliary park request; `[0x27]` comes from CAN `0x112`
   byte 0 bits 2..0, a 3-bit status from the ESC/ABS group, [I] park-brake or hold status, spec 07
   §7.1) and of fault group 33.
5. Bench confirmation of the lever decode (§5a) and of the Auto Park interpretation (bench
   or vehicle capture: select P/R/N/D and log `D6D6`, `D672` via the measurement protocol,
   spec 10).
