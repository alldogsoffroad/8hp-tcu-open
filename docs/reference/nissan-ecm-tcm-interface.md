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
**None found.** No 0x174/0x177 value reaches ignition, fuel or throttle in this ROM; the
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
