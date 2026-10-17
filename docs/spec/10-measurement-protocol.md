# Spec 10 — Measurement protocol on CAN `0x361` / `0x398`

Status: **draft.** Transport, enable condition, command set, address permissions and the
measurement index list are decoded from the stock firmware. **Confirmed on the bench spare
(2026-10-17):** on bus 2, `0x2A` reads return `2A 81` + 6 bytes, and identification `0x01`
returns `0x03` ("not in sequence") as predicted in §6.
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference, **[D]** derived.

Why it matters: this protocol lets a tester read any application RAM variable of the running
stock TCU, either by address or by a numbered measurement list. That is the most direct way to
answer the open items of specs 07–09 (signal meanings and units, sportiness value, lockup
levels) on a bench or in the Ram truck. The project firmware may also offer a compatible
interface for its own development.

**Safety:** the protocol also has RAM **write** commands. Writing RAM of a running
transmission controller can cause wrong gear or clutch commands. Use read commands only, and
only on a bench or a stationary vehicle, unless a test plan says otherwise.

## 1. Transport [O]

| Direction | CAN ID | Buffer | Content |
|---|---|---|---|
| tester → TCU | `0x361` | `0xFFF925B8` | 8-byte command frame |
| TCU → tester | `0x398` | `0xFFF925B0` | 8-byte response frame |
| TCU → tester | `0x368`, `0x378`, `0x388` | `0xFFF925A0/A8/C0` | periodic data (§4) |

- The IDs exist on bus 2 (RCAN C) and on bus 1 (RCAN A); see the CAN message list (rows 92–101).
  The first bus on which a valid `0x361` arrives is latched (`0xFFF80D1A`: 1 = bus 2,
  2 = bus 1). On bus 1 the frame must additionally match a stored value (`0x77F3C`) [O].
  [I] Use bus 2.
- Response byte 0 echoes the command; byte 1 = `0x81` for a positive data response. For write
  commands the response byte 0 is `0x01` = done or `0x03` = rejected. Unknown commands get `0x03`
  [O].
- The protocol only runs when the calibration byte at `0x195D6E` is `1`, which it is in this
  calibration (gate `0x135768`, checked by `0x15456C`) [O]. No seed/key unlock was found.
- Main code: frame intake `0x15456C`, dispatcher `0x1543F8`, buffers set up by `0x15531A`.

## 2. Address permissions [O]

Every read and write passes `0x1554BC`. It allows `0xE000–0xFFFF` directly and checks other
addresses against the same region tables the diagnostic memory services use (`0x7D3CC`).
There are two region tables: `0x435C0` for the default session and `0x43620` when the
session function `0x7F70A` reports state 2 and a flag is set.

| Range | Default session | Special session |
|---|---|---|
| `0x000000–0x17FFFF` code | no read | no read |
| `0x180000–0x1FFFFF` calibration | read | read |
| `0x600000–0x67FFFF` [I] calibration overlay RAM | read/write | read/write |
| `0x80100000–0x8011FFFF` EEPROM | no read | no read |
| `0xFFF80000–0xFFF9AFFF` application RAM | **read** | read/write |
| `0xFFF9B000–0xFFF9DFFF` RAM | read/write | read/write |
| `0xFFF9E000–0xFFF9FFFF` RAM | no | no |
| `0xFFFC0000–0xFFFFFFFF` I/O registers | read | read |

An exception list (`0x43680`, 14 entries of 12 bytes: start, end, access byte) overrides the
region table: access `0x01` = no access, `0x02` = read only [O].

| Range | Access |
|---|---|
| `0xFFF80000–0xFFF80100` (boot request record, spec 06 §5a) | **read only** |
| `0xFFF80134–0xFFF80C4C` | read only |
| `0xFFF80E28`, `0xFFF8303C`, `0xFFF85354`, `0xFFF95264` (single addresses) | none |
| `0xFFF949A4–0xFFF94D04` | none |
| `0xFFF8F400–0xFFF90000` | none |
| `0x5DE0–0x5E00` | none |

Denied reads return `0xFF` per byte for word reads (`0x20`) and `0x0E` per byte for block
reads (`0x28`, `0x2A`, `0x30`) [O].

## 3. Commands [O]

Address forms: **A16** = `0xFFF80000 + 16-bit offset` (bytes *n*, *n*+1, big-endian);
**A24** = `0xFF000000 + 24-bit value`; **A32** = full 32-bit address.

| Cmd | Request (bytes 1…7) | Response | Function |
|---|---|---|---|
| `0x01`, `0x21` | — | `0x21`, part, 6 bytes | identification, 4 parts returned in turn (`0x153BB4`) |
| `0x02` | — | `0x01`, zeros | [I] reset protocol state |
| `0x08` | — | — | calls `0xA0E84` ([I] reset/mode request — **do not use** until verified) |
| `0x0E` | b2–b3 = index | `0x0E`, b1 echo, 2 bytes | value from `0x155534` ([I] measurement value) |
| `0x11` | b2–b3 = index | `0x11 0x81`, 4 bytes | 32-bit RAM address of measurement *index* |
| `0x12` | — | `0x11 0x81` | — |
| `0x20` | b2–b7 = up to 3 × A16 | `0x20 0x81`, 3 × 2 bytes | **read words** (`0xFFFF` if denied) |
| `0x22` | b2–b3 = index | `0x22 0x81`, 2 bytes | 16-bit address of measurement *index* (`0xFFFF` if outside `0xFFF8xxxx`) |
| `0x23` | b1 = part (1, 2, `0x83`), 3 × A16 / 2 × A16 | — | define an 8-entry A16 address list |
| `0x28` | b2–b3 = 16-bit address (absolute, `0x0000–0xFFFF`, not A16) | `0x28 0x81`, 6 bytes | read 6 bytes |
| `0x2A` | b2–b5 = A32 | `0x2A 0x81`, 6 bytes | **read 6 bytes at any permitted 32-bit address** |
| `0x29` | — | `0x03` | rejected |
| `0x30` | b2–b7 = 2 × A24 | `0x30 0x81`, 2 × 3 bytes | read 3 bytes each |
| `0x32` | b2–b3, b4–b5 = indices | `0x32 0x81`, 2 × 3 bytes | 24-bit addresses of two measurements |
| `0x33` | b1 = part (1, 2, 3, `0x84`), 2 × A24 | — | define an 8-entry A24 address list |
| `0x04`, `0x05` | b2–b3 = A16, b4 = data | `0x01`/`0x03` | write byte |
| `0x0A`, `0x0B` | b2–b3 = A16, b4–b5 = data | `0x01`/`0x03` | write word |
| `0x34`, `0x35` | b2–b4 = A24, b5 = data | `0x01`/`0x03` | write byte |
| `0x3A`, `0x3B` | b2–b4 = A24, b5–b6 = data | `0x01`/`0x03` | write word |
| `0xE0` | b1 low nibble = frame number *n* (1…11); b2–b7 = 3 × 16-bit index | none | build the periodic list (§4) |

## 4. Periodic data [O]

`0xE0` frames carry up to 32 measurement indices, three per frame; frame *n* fills list
positions 3*n*−3 … 3*n*−1. A frame whose b1 is `0x81…0x8E` is the **last** one [O] (`0x15456C`).
When the list is complete,
`0x154718` converts each index to its RAM address (skipping denied ones). `0x154800` then copies
**one byte** from each address into four 8-byte transmit buffers, sent on `0x368`, `0x378`,
`0x388` and `0x398`: 32 byte channels in total. Two-byte measurements take two consecutive
channels (bitmap at `0x155558`, `0x1552F2`).

Channel bytes 0–7 go to `0x368`, 8–15 to `0x378`, 16–23 to `0x388`, 24–31 to `0x398` [I]
(buffer order in `0x154800`). The sending period was not traced.

## 5. Measurement index list [O]

The table at `0x635F4` (header magic `"IXRA"` at `0x635EC`) maps **470 measurement indices to
RAM addresses**: index *i* (2…471) is at `0x635EC + 4i` (lookup `0x135788`). The full list is in
`1034420267_measurements.csv` (project root), labelled where the variable has been identified
in specs 07–09. Examples:

| Index | Address | Meaning |
|---|---|---|
| 181 | `0xFFF8B604` | converter slip, engine − turbine (rpm) |
| 182 | `0xFFF8B60C` | turbine speed, filtered (rpm) |
| 193 | `0xFFF8B525` | drive/coast flag |
| 336 | `0xFFF89CEC` | lockup request level seen by the converter module |
| 358 | `0xFFF8A786` | [I] pedal input of the shift/lockup maps |
| 367 | `0xFFF8AB58` | driver-type sportiness value |
| 369 | `0xFFF8AB30` | [I] output speed used for shift/lockup decisions |

Variables not in the list (for example engine speed `0xFFF8B5FC`, lockup demand `0xFFF8B526`)
can still be read by address with `0x20` or `0x2A`.

## 6. Suggested first bench test

1. Stock TCU powered on a bench with bus 2 connected; listen for any traffic.
2. Send `0x361: 22 00 B5 00 00 00 00 00` (index 181) and expect `0x398: 22 81 B6 04 …`, the
   address of the slip variable.
3. Read a word: `0x361: 20 00 B6 04 00 00 00 00` and expect `0x398: 20 81 hh ll FF FF FF FF`.
4. Optional: `0x361: 01 …` for identification. The reply depends on a sequence state
   (`0xFFF85342`) whose start value is not confirmed; `0x03` means "not in sequence".
5. If these work, log the indices of interest in the Ram truck with `0xE0` periodic lists.

The checksum/counter protection of spec 07 does **not** apply to these IDs (they are not in
the protected list).

## 6a. Project firmware implementation (M1)

`firmware/bsw/meas` implements a **read-only** subset with the stock frame layouts: `0x01`/`0x21`
identification (`8HPTCU-OPEN M1` in four 6-byte parts), `0x02`, `0x11`/`0x22`/`0x32` index
addresses, `0x20`, `0x28`, `0x2A`, `0x30` reads, and `0xE0` periodic lists sent every 10 ms.
All write commands, `0x08`, `0x0E`, `0x12`, `0x23`, `0x29`, `0x33` answer `0x03`.
Differences from stock:
- requests are accepted on either bus; the first bus used is latched (no `0x77F3C` check on bus 1);
- readable areas: user MAT flash `0x000000–0x1FFFFF` (boot blocks, application, calibration —
  for cross-checking read-outs) and RAM `0xFFF80000–0xFFF9FFFF`; I/O registers are not readable;
- measurement indices are the project's own list (`firmware/app/meas_table.c`), not the stock
  list of §5.

## 7. Open items

1. Behavior on hardware (all of the above is static analysis).
2. Exact meaning of `0x02`, `0x08`, `0x0E`, and what the identification parts contain.
3. Session handling: what puts the TCU in the special session that allows RAM writes
   (`0xFFF84A1A`, `0x7F70A`).
4. Value expected on bus 1 (`0x77F3C`).
