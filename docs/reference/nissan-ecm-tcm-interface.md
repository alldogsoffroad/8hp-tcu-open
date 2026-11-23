# Nissan VQ40DE ECM ↔ transmission controller CAN interface (decoded from the ECM ROM)

Source: the 1 MB dump of the bench ECM (`~/Documents/Nissan ECU/dumps/dh1_9CJ3A_pass1.bin`, not committed; method and identity in `~/Documents/Nissan ECU/vq40_ecu_reference.md`) (DH1, 23710-9CF0A hardware,
software 9CJ3A, SH7058, MY2014 Frontier). Static decode 2026-11-23 (Ghidra + objdump). Tags: [V] =
verified in code, [I] = inferred. Items marked ✔ were re-checked by hand in the ROM afterwards.
Units and the meaning of most status bits are NOT established; a vehicle capture (TCM plugged)
is the way to pin them.

## CAN setup
- Message table ROM 0x97B4 ✔ (slot 0 unused): 4-byte entries {config, std ID<<5}; config high byte
  01 = receive (polled), 03 = transmit, low byte = DLC. Slot n → HCAN mailbox n+1, init at
  0xE0290, mailbox data at 0xFFFFD928+32·slot [V].
- Receive records at 0xFFFF3F87 + 10·slot ✔: flags, age counter, then data. Poll 0xE08B4/0xE0796,
  dispatch 0xE0DBC on the new-data flag [V]. **No checksum on any frame** [V].

| Frame | Slot | Data in RAM | Handler | Default handler |
|---|---|---|---|---|
| 0x174, 8 bytes | 11 | 0xFFFF3FF7 | 0xE2674 ✔ | 0xE4BA6 |
| 0x176, 7 bytes | 5 | 0xFFFF3FBB | 0xE28CE | 0xE4BF4 |
| 0x177, 8 bytes | 10 | 0xFFFF3FED ✔ | 0xE297C ✔ | 0xE4C12 |

## What the ECM reads

**0x174** (the main one: gear and shift state)
| Bytes / bits | Use |
|---|---|
| B0:B1[7:4], B1[3:0]:B2 | two 12-bit values, phys = raw·5 − 4000; raw 0xFFF = invalid. Same encoding the ECM uses for its own (torque-like) values in 0x180/0x160 [V scaling, I meaning] |
| B3[7:4], B3[3:0] | gear codes: 1–7 = gear; 9 (R?), 10/0 (P/N?), 11, 15 special. **The two nibbles differing = "shift in progress"** (0xFFFF2457 bit 4). Gear used in ~17 places incl. gear-ratio speed logic [V decode, I meaning] |
| B4[7:5] | mode code 0–7; 3, 5, 6, 7 set flags [V] |
| B4[3:0] | 4-bit rolling counter; stored, no fault found for a wrong counter [V/I] |
| B5 | bits 7:6 = 01/10 → flag; bits 5, 4 → flags; bits 3:2 → 0xFFFF2848 [V] |
| B6, B7 | not read [V] |

**0x176** (speeds)
| Bytes / bits | Use |
|---|---|
| B0:B1 | 16-bit BE → vehicle-speed source (selected over 0x2D1 by cal 0x8AAF bit 7 = 0) [I: vehicle speed] |
| B2:B3 | 16-bit → second speed [I] |
| B6 bit 7 : B4 : B5 | 17-bit value, used in the speed plausibility logic at 0x9B5E0 [V] |
| B6[3:0] | 4-bit counter [V] |
| B6[6:4] | stored, no reader found |

**0x177** (status bits; the expected torque request is NOT used)
| Bytes / bits | Use |
|---|---|
| B5[6:3] | shift-type code, 0xF = no shift; the cross-check against 0x174 (P1780) is switched off by cal 0x13C7A bit 4 = 0 [V] |
| B3[3:0] | compared with 8, 9, 10, 14 at 0x77248–0x77736 [V] |
| B1[7:2], B2 bits 6/3, B3[7:5], B5 bit 1, B6[7:6] | single status flags [V copy, meaning not decoded] |
| B0, B1[1:0], B2[2:0], B4, B6[5:2], B7 | stored only, no reader found |

## Loss of the frames
- Each slot's age counter increments every receive-task pass and resets on reception [V].
  **Age > 199 on 0x174, 0x176 or 0x177 → U1001** (DTC table entry 0x4E @0x14620 = `D0 01` ✔),
  enabled by cal 0x13C73 bit 3 [V]. U1000 (0x251/0x560) is calibrated off ✔ (entry `D0 00`).
- Timeout ≈ 1.0 s if the task runs every ~5.1 ms (ATU timer reading), or 2 s at 10 ms [I].
- Substitute values exist (gears 0, speeds 0/0xFFFF, 0x177 code 0xF) but what enables them is not
  found. **No torque limit, idle-up or MIL effect of the timeouts was found** [I: absence].

## Torque intervention
> **Superseded by the vehicle-log check below: the ECM does reduce torque on 0x177 B4.**

**None found (ROM decode).** No 0x174/0x177 value reaches ignition, fuel or throttle in this ROM; the
torque-format values in 0x174 and 0x177 B0/B4/B7 have no readers; no echo/handshake [I: a
pointer-indexed read could have been missed]. In this software the ECM consumes the gear, the
"shifting" state and speeds, not a torque request.

## What the ECM sends (for the TCU to consume) [V layout, I meaning]
- **0x180:** B0:B1 = engine speed·25 (cap 0xFFFE) [I]; B2–B4 = two 12-bit torque-format values;
  B5 + B6[7:6] = 10-bit 0–1000 (pedal or throttle) [I]; B6[3:0] = rolling counter.
- **0x182:** B5[3:0] = the same counter; B6[1:0]:B7 = 10-bit value, offset +200; B3 = a value >> 3.
- **0x160:** B0–B2 = two 12-bit torque-format values; B3:B4[7:6] and B5:B6[7:6] = 10-bit values.

## For the TCU's Nissan profile
- Minimum to keep this ECM free of U1001: send 0x174 (8 bytes), 0x176 (7 bytes) and 0x177
  (8 bytes), each at well under ~1 s intervals; plausible gear nibbles in 0x174 B3 (equal when
  not shifting), speeds in 0x176, B5[6:3] = 0xF in 0x177 when not shifting.
- Torque reduction during shifts would need another route (none is used by this ECM software).
- Open: units of the speed/torque fields, the meaning of the status bits, transmit periods, and
  whether other ECM software versions behave the same. A capture of a running truck with its
  TCM fills these.

## Vehicle-log check (2026-11-23): a different 2015 Frontier, TCM connected
Logs: `~/Downloads/2026-09-1{0,1}_torque_*.txt` (64 shift-triggered windows, ~117k frames, the
user's logger; ECM software of that truck unknown). Findings [O = observed on the bus]:
- **Periods [O]:** 0x174, 0x176, 0x177 every **10 ms**; 0x180/0x182/0x1F9/0x160 10 ms; 0x421 60 ms.
- **DLC [O]:** 0x174 8, 0x176 7, **0x177 6** (the bench ROM's table copies 8; receive mailboxes
  accept any length, so the table DLC is a copy length, not a requirement [I]). Also 0x560 4
  (table 3), 0x215 6 (table 7).
- **0x174 B3 [O]: low nibble = engaged gear, high nibble = target gear** (steady acceleration
  shows 21, 32, 43, 54; equal nibbles 22/33/44/55 = no shift pending; 34, 45 = downshift
  targets). Sequence at an upshift: 43 → 44 (gear engaged) → 54 (next target). B0–B2 constant
  FF E7 1C; B4[3:0] 4-bit counter (all 16 values); B5 00/80; B6, B7 00.
- **0x176 [O/I]:** B2:B3 ≈ 0.91 × engine rpm and B2:B3 ÷ B0:B1 = 3.36 / 2.24 / 1.50 / 1.00 /
  0.84 by gear (RE5R05A ratios 3.83 / 2.37 / 1.52 / 1.00 / 0.83) → **B2:B3 = turbine speed,
  B0:B1 = output-shaft speed**, same units (≈ 1 rpm/bit [I]); B0:B1 ≈ 25.8 counts per mph.
  B4, B5 = 00; B6[3:0] counter.
- **0x177 [O]:** B0, B1 = 00; B2 slow sequence (00–0C); B3 = 01 only in 1st;
  **B5[6:3] = shift type: 0 = 1→2, 1 = 2→3, 2 = 3→4, 3 = 4→5, 9 = 4→3, 13 = 5→4, 15 = none**
  (matches the ROM's "0xF = no shift"); **B4 = torque-reduction level, non-zero only during
  upshifts** (0x1A–0xA7 seen).
- **The ECM acts on 0x177 B4 [O]:** event 1 (1→2 at 3300 rpm, 40 % throttle): B4 = 0x67 at
  142.335 s → the first 12-bit value in 0x180 (and 0x160 second value) falls 2835 → 1790 within
  ~50 ms while the second value (≈ 3315, driver demand [I]) is unchanged; it recovers after B4
  returns to 0. With phys = raw·5 − 4000 read as 0.1 Nm [I], that is ≈ 283 → 179 Nm actual
  against ≈ 331 Nm demanded.
- **This corrects the ROM decode's "no torque intervention found"**: either the decode missed
  an indexed read of the 0x177 record (no literal reference to its byte-4 RAM cell 0xFFFF3FF1
  exists), or the vehicle's ECM software differs from the bench unit's 9CJ3A. Next
  discriminator: trace reads of the 0x177 receive record (base 0xFFFF3F87 + 10·10) in the bench
  ROM by offset, not by literal.
- 0x421 [O]: 3 bytes, constant 20 00 00 (D per the DBC) in every window; sender not settled.

## Why the ROM decode reported "no torque intervention" (trace, 2026-11-23)
- The 0x177 handler 0xE297C reads the receive record through `mov #offset,r0; mov.b @(r0,r13)`
  with r13 = 0xFFFF3F87 (B0 = offset 102 … B4 = **offset 106**), so the data bytes never appear
  as addresses. **B4 is stored to 0xFFFF289F** (0xE2B4C, in the delay slot of `bt.s`).
- **0xFFFF289F has four writers and no statically resolvable reader:** the 0x177 handler; the
  substitute writers 0xE4B86 (`mov.b r0,@(0,gbr)` with GBR = 0xFFFF289F) and 0xE4C64; and
  0xE1D9C, which, when cal **0x8AAF bit 4 = 1 (set in this calibration)**, overwrites it with the
  byte at 0xFFFF3FE6 (GBR 0xFFFF3F16 + 208 = the 0x560 record, data byte 3).
  Searched: literal pools, raw ROM data (the address occurs only in the three literal pools),
  register + displacement, and GBR/indexed accesses with the r0-offset idiom this compiler uses
  (e.g. `mov.l #0x14B99,r0; tst.b #16,@(r0,gbr)` with GBR 0xFFFF3F16 = cal byte 0x8AAF):
  no read of 0xFFFF289F.
- So the decode's "B4 stored only, no reader found" was accurate as far as static search goes;
  the error was concluding "no torque intervention" from an absence. Two explanations remain:
  (a) a read through a computed pointer that static tracking cannot resolve [I: likely, since
  the firmware maintains substitute values and an alternative source for this cell], or
  (b) the logged truck's ECM software differs from 9CJ3A.
- Discriminators: the logged truck's ECM software ID (`21 FE`), or a bench replay: send 0x174 +
  0x177 with B4 ≠ 0 (and 0x560 B3, since cal 0x8AAF bit 4 routes it into the same cell) and
  watch the ECM's 0x180 actual-torque field.

## Resolved: the 0x177 B4 reader (bench test + trace, 2026-11-23)
**Bench replay** (bench ECM, engine not running): the truck's 0x174/0x176/0x177 frames sent every
10 ms with 0x177 B4 = 00 / 67 / 67 (+0x560 B3 = 67) / 00. `0xFFFF289F` follows B4 within a frame
(read via `23` in session FB). The 0x180 torque fields stayed at −330 / 2090 throughout (engine
stopped: no torque to reduce, so not a test of the effect).
**RAM diff** (all 48 KB, 0xFFFF0000–0xFFFFBFFF, two dumps with B4 = 0 and two with B4 = 67):
44 bytes are stable within each condition and differ between them. Two hold the B4 value itself:
**0xFFFF274F and 0xFFFF3910** (besides 0xFFFF3FF1 and 0xFFFF289F); state bytes such as
0xFFFF253D (63 → 00), 3335, 35B4, 3C66/67, 3E50/54/56, 4392, 449D and 46B5 change too.
**The missing link is a block copy:** at 0x20E04 the firmware calls the generic copy routine
0x21CA4 with length `84 << 2` = **0x150** bytes, destination end 0xFFFF2784 and a **source
pointer computed at run time from GBR + r9**. That copies 0xFFFF2784–0xFFFF28D3 (the receive
variables, including 0xFFFF289F) to 0xFFFF2634–0xFFFF2783, putting B4 at **0xFFFF274F**. The RAM
dumps show the mirror directly (0xFFFF2823–28D3 = 0xFFFF26D3–2783 byte for byte). 0x68AC0 reads
0xFFFF274F into 0xFFFF3910, which the function at 0x68AD4 tests (with counters at 0xFFFF108E and
the flag 0xFFFF253C).
**So explanation (a) holds: software 9CJ3A does consume 0x177 B4**; no literal ever names the
source or the copy, which is why every static search missed it. Not yet traced: the path from
0xFFFF3910 / 0x68AD4 to the actuators (ignition retard / fuel cut / throttle).

## From 0xFFFF3910 toward the actuators (static trace, 2026-11-23) — chain NOT closed
Tags: [V] verified in code ([V✔] re-checked by hand), [I] inferred.
1. **Source [V✔]:** cal 0x8AAF = 0x1C (bit 4 set): 0x68958 copies 0xFFFF274F (B4) into 0xFFFF3910
   directly; the alternative map lookup (tables 0x103C9–0x10455) is not used.
2. **Gate, active flag 0xFFFF2401 bit 6 (0x68AD4) [V]:** needs 0xFFFF241D.6 and 0xFFFF2417.5 set,
   0xFFFF470E.4 clear, 3910 ≠ 0, the two 0x174-derived gear values 0xFFFF108E ≠ 108F (108F > 108E
   = upshift branch), at most one of the 0x177 B3 flags, and a shift timer not expired:
   0xFFFF253D from cal 0x849F = **100** (upshift) [V✔], 0xFFFF253C from 0x849E = **60** (downshift)
   [V✔]. 241D.6 needs engine-running conditions (0xFFFF107F ≥ 0x1E, 0xFFFF44E6 ≥ 0x32 [I: rpm],
   0xFFFF11A1 ≥ 2), so on the stopped bench engine the flag stayed 0 (matches the RAM diff:
   253D counted 0x63 → 0).
3. **Scaling [V code, I meaning]:** 0x68C44: 0xFFFF1856 = [T·(256 − B4) + 256·L] / (T + L), T =
   filtered net torque 0xFFFF18CE, L = losses (0xFFFF2394 + 0xFFFF14D0): **B4/256 = fraction of net
   torque to remove** (0x67 ≈ 40 %, the truck showed ≈ 35 %).
4. **Ramps [V]:** 0x68E62 → 0x69356, ramped request 0xFFFF185A. Upshift: first step 60 % (×0x99/256),
   ramp-in ≈ 3 calls, ramp-out ≈ 17 calls (B4·0x0F per call). Downshift: immediate, ramp-out from
   maps 0xB3B4 / 0xB3A4. 0x6960E / 0x69812 combine it with a second (disabled, 0x8AA6 & 0x18 = 0
   [V✔]) reducer into the torque ratio 0xFFFF185E.
5. **Actuator = spark retard [V]:** 0x6863E inverts the spark-efficiency tables 0xF29C / 0xF6B0
   (lookup 0x23070) to find the retard that gives the requested ratio, floor −7° (cal 0x8237 =
   0xF9 [V✔]) → **0xFFFF234C** (retard, deg·256 [I]). **Fuel cut is calibrated off** (thresholds
   0x8F64 = 0xFF, 0x8301 = 0xFF [V✔]: never), **no throttle path found**.
6. **The open link:** the only code found that consumes 0xFFFF234C is 0x4C7BC (spark limit
   0xFFFF11C6/11C8), and it is **skipped**: at 0x4C790 `tst.b #32,@(r0,gbr)` with GBR 0xFFFF2214,
   r0 0x16897 → cal **0x8AAB = 0xAE, bit 5 set → branch past** [V✔]. The reported torque
   (0xFFFF18CA → 18CC → 18CE → 239A) drops only through 0xFFFF11B8 (applied retard) or the cut
   count, and 0xFFFF11B8 has no visible writer (likely a copied block or the ignition interrupt).
   The 0x180/0x160 packer was not traced.
7. So in this ROM the request becomes a **spark-retard target**; whether and how it reaches the
   coils in the 0x8AAB.5 = 1 mode (likely via the 0xFFFF1826/2390 spark path from 0x4AA40, or a
   copy into 0xFFFF11B8) is not established statically. Discriminator: RAM reads (0x23, session
   FB) of 0xFFFF2401, 0xFFFF234C, 0xFFFF11B8 and 0xFFFF18CE during upshifts on a **running**
   engine. Note: the truck's "actual torque" drop in 0x180 is consistent with either a real
   retard or the ECM reporting its reduced torque target.
