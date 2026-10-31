# Spec 13 — Fault memory and monitor reporting

Status: **draft.** The fault table, the reporting API and how monitors identify their fault are
established [O]. Individual monitors are documented as they are decoded (§4).
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference, **[D]** derived.

Data file: `1034420267_dtc_list.csv` (project root): one row per fault event with group,
monitor, DTC and the stored status from the EEPROM dump.

## 1. Fault table [O]

- **Event table** at calibration `0x1942E8`: 244 entries of 12 bytes. Each entry: u16 **fault
  group**, u16 **monitor number**, then the 3-byte DTC (stored twice) and padding. Example:
  event 186 = `00 71 00 01 00 1B 14 00 1B 14 00 00` → group `0x71` (113), monitor 1, P1B14-00.
- **173 fault groups** are used, numbered 1 … 195. A group typically bundles the monitors of one
  component (for example group 1 = shift solenoid A performance, group 2 = its electrical
  monitors).
- Event status bytes are kept in RAM at `0xFFF823F7 + N` and stored in EEPROM block `0x0306`
  (spec on EEPROM records, `1034420267_eeprom_records.csv`).

## 2. Reporting API [O]

| Function | Role |
|---|---|
| `0xA0E70` → `0x9BE50` | **report(group, data, failed, tested)**: updates the 16-bit status word of fault group *group* (1–195; array at `PTR 0x9BEBC`), handles confirmation, healing and freeze-frame data |
| `0x9EEBE(group)` | returns bit 0 of the group status ([I] "fault currently present") |
| `0xA105E(group, data, mode)` | variant entry (mode 1 → `0xA11B8`, else `0xA11BC`) |
| `0x6865C` / `0x68736` | **fatal internal-error handler**, not fault memory: called with codes such as `0x226` and `0x238` from safety checks (inverted-copy mismatches, current-plausibility); records the code, caller and stack, then resets/halts |

Application code reaches `0xA0E70` through interface slots (`0x4B888`, `0x4B980`, `0x4CED4`,
…), about 51 call sites in 37 functions, most of them in the safety-monitoring layer
(`0x135xxx–0x14Exxx`) and the park/range module (`0xA5xxx–0xA9xxx`, `0xC2xxx–0xC8xxx`).

**How monitors name their fault:** the group number is **not** an immediate in the code. An
identity table of byte constants at `0x631E1` (the byte at `0x631E1 + N` equals N, for
N = 0 … `0xC4`) is referenced through pointer slots (`0x4B8xx`, `0x4B9xx`), and each monitor loads
its group number from its slot. Example: group 113 is the byte at `0x63252`, slot `0x4B8DC`.
To find the monitor for a fault group N: find the pointer slot holding `0x631E1 + N`, then the
literal pools referencing that slot.

## 2a. Fault group → monitor map [O]

Data files (project root): `1034420267_fault_groups.csv`, one row per group (DTCs with monitor
numbers from the event table, how the reporting code was found, function(s), notes, status
readers); `1034420267_can_comm_monitor.csv`, one row per monitored CAN message (§2c).

**Report entry points.** All take the fault group as the first argument:

| Function | Use |
|---|---|
| `0xA0E70` | `report(group, data, failed, tested)` |
| `0xA105E` | `report3(group, data, mode)` |
| `0x9B918` | `fail_now(group, data)`: sets the failed state directly (group 1–195) |
| `0xA107C` | debounce/qualification step for a group |
| `0x9BE50` | core, reached from `0xA0E70` |

The identity byte table at `0x631E1` supplies group numbers, but it is also used for the
*data* argument in some calls; a second identity table follows at `0x632A6` (sub-codes such as
`0x40`/`0x41`). Group numbers therefore have to be read from the argument position, not from
the mere reference.

**How groups reach the report call** (CSV column `evidence`; 172 rows incl. groups without DTC).
Result: a monitor is identified for **165 of the 171 groups that carry a DTC**.

| Evidence | Groups | Mechanism |
|---|---|---|
| report call | 80 | the group (immediate or identity byte) is the first argument of a report call in the named function |
| provider | 41 | a function returns the group for an index: `0xB9DBE` (shift-element combinations, §2b), `0xB9EC6` (gear 1–8 → P0731–P0735, P0729, P076F, P07D9 = incorrect gear ratio), `0xB9F2A` (five cases → groups 190, 118, 119, 121, 93), `0x1362F4` (groups 136/137 passed in), `0xB9DA4` |
| channel table | 32 | `0x7B222`: SPI output channels 0–9 → groups 1–20 (table `0x45BB4`); `0x7B358`: table `0x45C84`, indices 0–4 → groups 21–30; `0x7B4EC`: groups 31 (actuator supply, P0657–P0659, P1DB2) and 32 |
| CAN table | 12 | communication monitor `0x13661C`, one entry per received message (§2c) |
| id reference | 1 | group 196 (no DTC): loaded by `0xA9FF8`, path not resolved |
| not found | 6 | 39 (P0601), 47 (P0607), 106 (U1412), 157 (P0562), 158 (P1D20), 160 (U1479) |

Functions that only **read** a group's state (`0xA0FA0`, `0xA0FC4`, `0xA0EB2`, `0xA03D6`,
"fault present / confirmed" queries) are listed separately in the CSV column `status_readers`;
they are fault *consumers* (substitute reactions, monitor interlocks), not monitors.

### 2b. Shift-element combination monitors [O]

Groups 56–79 (P1D8F…P1DA8) are indexed by a **set of shift elements**. The monitors
(`0xF038E`, `0xD790A`, `0xF1F38`, `0xF21BA`) form a 5-bit mask (bit 1 = A … bit 5 = E), look
up an index in the table at `0x52250` and get the group from `0xB9DBE`:

| Elements | DTC | Elements | DTC | Elements | DTC |
|---|---|---|---|---|---|
| A | P1D8F | AB | P1D96 | ABC (1st) | P1DA0 |
| B | P1D90 | AC | P1D9C | ABE (2nd) | P1DA1 |
| C | P1D91 | AD | P1D97 | BCE (3rd) | P1DA2 |
| D | P1D92 | AE | P1D9B | BDE (4th) | P1DA3 |
| E | P1D93 | BC | P1D9D | BCD (5th) | P1DA4 |
| | | BD | P1D98 | CDE (6th) | P1DA5 |
| | | BE | P1D99 | ACD (7th) | P1DA6 |
| | | CD | P1D9A | ADE (8th) | P1DA7 |
| | | CE | P1D9E | ABD (R) | P1DA8 |
| | | DE | P1D9F | ACE | (none) |

All nine element triples that form a gear (spec 11 §1) have a DTC; the one triple that is not a
gear (ACE) has none. [I] The singles and pairs flag elements that cannot be applied/released;
the exact fault definitions (FCA DTC texts) are not in the firmware.

### 2c. CAN communication monitor [O]

`0x13661C` (object `0xFFF95750`) checks 53 received messages. `0x1513F8` copies each message's
status byte (byte 8 after the message's alternate copy, spec 07) into `0xFFF95754 + n`; the group
per entry comes from RAM table `0xFFF85289` (3 bytes per entry, initialised by `0x15A458`);
per-entry calibration masks and thresholds are at `0x191C34` / `0x191BB0`. Group meanings below
are the SAE J2012 U-code definitions [M]:

| Group | DTCs | Node (SAE) | Messages |
|---|---|---|---|
| 139 | U0100, U0401 | engine control module | `0x1A0`, `0x3E1`, `0x29D`, `0x14B`, `0x301`, `0x14C`, `0x1F5`, `0x144`, `0x278`, `0x161`, `0x10C`, `0x140`, `0x148`, `0x180` |
| 140 | U0121, U0415 | anti-lock brake module | `0x334`, `0x037`, `0x178`, `0x2E2`, `0x170`, `0x2E1`, `0x271`, `0x308`, `0x112` |
| 141 | U0102, U0403 | transfer case module | `0x2A0`, `0x101`, `0x3F5` |
| 142 | U0103, U0404 | gear shift module | `0x2FA`, `0x2EA` |
| 143 | U11E3, U1461 | (manufacturer) shifter private bus | `0x200`, `0x210` (bus 2) |
| 144 | — | — | `0x1D0` |
| 145 | U0212, U0429 | steering column module | `0x2F9` |
| 146 | U0140, U0422 | body control module | `0x32A`, `0x134`, `0x270`, `0x3E4`, `0x2A4`, `0x330`, `0x2F8`, `0x108`, `0x324`, `0x120`, `0x124`, `0x3E8`, `0x31F`, `0x102`, `0x3E9` |
| 147 | U0155, U0423 | instrument cluster | `0x310` |
| 166 | U0128, U0417 | park brake module | `0x158`, `0x11C` |
| 170, 171 | — | — | `0x118`, `0x122`; `0x1AA` |

This gives the **sending node of each received message**, which is the main input for building a
gateway (Nissan work, later). Note: spec 07 §5 lists `0x037`, `0x2F9` and `0x170` as messages the
TCU transmits; here they are monitored as received from the ABS and steering-column modules.
Which is right is **open** (bus capture).

**Cross-check with spec 12.** The solenoid electrical groups confirm the output channel
order: channel 0 → P0972–P0974 (shift solenoid **A**), 1 → P0981–P0983 (**D**), 2 → P0975–P0977
(**B**), 3 → P0984–P0986 (**E**), 4 → P0978–P0980 (**C**), 5 → P2761–P2764 (**TCC**),
6 → P0960–P0963 (**line pressure**), 7 → P1D77–P1D79 (**park release**); channels 8 and 9 have
no DTC. This matches spec 12 §1 exactly and is an independent seventh confirmation.

## 3. Program-flow checkpoints [O]

Safety-monitor functions bracket their work with `0x148E92(0xFFF95FE8, id)` and
`0x148EB6(0xFFF95FE8, id)`, a program-flow monitor. These ids (for example `0x0B`, `0x0C`,
`0x71`, `0x72`) are **checkpoint numbers, not fault groups**.

## 4. Decoded monitors

### 4.1 P1B14-00 "Unintended Park Position" (group 113, monitor 1) [O]

Code: enable function `0xC5E40`, report at `0xC5E78`; state struct `0xFFF8D61A` (park/range
module).

- **Tested** (`obj + 0x18` = 1) when: `0xFFF8D61A[0x38]` < 2, **and** a value read through
  `0xFFF84ADC` (field `+0x10`) is below the limit `0xFFF8D61A[0x3E]`, **and** `0xA7720` returns
  false.
- **Failed** when `0xFFF8D61A[0x2E]` == 1 **and** `0xFFF8D61A[0x06]` == 1.
- Reported as `report(113, data = 1, failed, tested)`.
- The two failure bytes are exposed by the module's data-identifier handler `0xA6F40` as
  local identifiers **`0x138F`** (`0xFFF8D648` = `[0x2E]`) and **`0x1390`** (`0xFFF8D620` =
  `[0x06]`), so they can be read live.
- Field-reported causes [M]: low fluid / low line pressure; faulty MV2 (park release) or MV3
  (park hold) solenoid (spec 12 §1.1).
- [I] From the field report (spec 12 §1.2) the monitor detects the park pawl engaged while Park
  was not commanded; the exact meaning of the two bytes is **open**.
- `[0x2E]` is the safety layer's park request (`0xFFF8EA40`, produced by `0x141A2C`); the same
  byte, qualified by `[0x38]`, drives the park-target arbiter (spec 14 §3, §4.2). `[0x06]` comes
  from `0xFFF8BC70` (origin open, spec 14 §8).

The neighbouring monitor for **P1B13-00** (group 114) is in `0xC5C7C` / `0xC5E02`; it watches
transitions of the range code (from 5, 6 or 7 to 8 = P) over a timer.

## 5. Open items

1. Meaning of `0xFFF8D61A[0x06]` and of the enable input at `0xFFF84ADC + 0x10`; the struct
   itself is documented in spec 14 §2.
2. Whether the local identifiers `0x1389–0x13A4` are reachable by UDS ReadDataByIdentifier.
3. Fault group map (§2a): the 6 groups not found (39, 47, 106, 157, 158, 160) and group 196;
   the conflict over `0x037`/`0x2F9`/`0x170` (§2c).
**Bench 2026-10-31 (running stock, live):** a synthesized network of ~46 bus-1 node
messages with **zero-filled bodies** (plus proper lever `0x2EA`/`0x210`, RPM `0x108`
with counter+CRC, brake `0x140`) is **accepted by the communication monitor** —
feeding it transitions the TCU from the no-comm outputs-released state (~0.170 A,
periodic still running) to solenoid engagement (**audible clicks**, ~1.7 A transient,
~1.85 A held); a D request during the feed produces a distinct 1.9 A bump → 1.7 A dip
→ re-hold; stopping the feed releases the outputs again (~0.170 A) after the comm
timeout. Per-frame content validation evidently does not gate network-aliveness
(presence + pacing suffice). The driver-feeder pattern: `/tmp/feedmon3.py`
(exception-guarded — the ~5 kHz aggregate feed rate can trip the Teensy serial link;
always guard the sniffer/reader threads).
