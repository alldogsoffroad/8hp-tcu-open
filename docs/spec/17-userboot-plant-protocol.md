# Spec 17 — User boot MAT: plant (production) programming protocol

Status: **plant entry reproduced on the bench with the Teensy (2026-10-17).** Source: the unit's own user boot MAT, read out with
routine FE20 (spec 06 §8). The dump is private and is not in the repository; this spec describes
behaviour, not code. Markers: **[O]** observed in the code, **[B]** bench, **[I]** inference.

## 1. Why this matters

FWE (the flash write-enable mode pin, ball A11) reads **low** whenever an application runs
(spec 16 §10), so no code started from the user MAT can program flash. The user boot MAT runs
only when FWE (and MD2) are high at reset (manual table 3.2). The ACDP-2 and the bench power the
TCU through the same pins (5, 6, 9, 13, 14), and the ACDP-2 always enters through the user boot
MAT, so the board starts in user boot mode at every power-up and something drops FWE during
start-up [I]. A tester that completes the user boot MAT's own session programs flash with FWE
high: this is the route to replace the ACDP-2 with the Teensy.

## 2. Start-up [O]

1. RAM tests, peripheral set-up, CAN set-up on **RCAN-A only** (TCU pins 5/6). The boot MAT uses
   the RCAN ID-reorder layout (MCR15 = 1).
2. Entry window (state byte `0xFFF8525A`), a few milliseconds after reset, timed on the free-
   running counter `0xFFFFF220`:
   - state 1: RCAN-A at **500 kbit/s**; if the receive-error count rises within `0x1450` ticks
     (traffic at another bit rate), switch to **666.667 kbit/s** (table value `0xA2C2B` = 666 667);
     no activity → window closes;
   - state 2: wait `0x1451` ticks; no new errors → state 3;
   - state 3: **one** read of mailbox 2; the frame must already be there.
3. Entry frames (mailbox 2, **29-bit ID `0x1F0C3CFE`**, DLC 8, ASCII):
   - `PlantCan` → mode 5
   - `PlantKcn` → mode 6
   A match sets the session flag; responses use mailbox 3, **29-bit ID `0x1FFFAFFE`**.
4. Session flag set and mode 5/6 → session loop (state `0x19`; supervisor word `0x8E80` with bit 4
   toggling, spec 03); first message: report `0x53` with the RAM-test result. Otherwise: `OKEY` /
   `BAD_` + address into the hand-over area `0xFFF84008` and start of the user MAT.

So a tester must already be sending the entry frame at 666.667 kbit/s when the TCU powers up —
consistent with the ACDP-2 switching the TCU's supply itself.

## 3. Mailboxes and bit rates [O]

| Mailbox | ID | Direction | Use |
|---|---|---|---|
| 0 | `0x7E1` (11-bit) | receive | session (500 kbit/s side) [I] |
| 1 | `0x7E9` (11-bit) | transmit | session [I] |
| 2 | `0x1F0C3CFE` (29-bit) | receive | plant entry and session |
| 3 | `0x1FFFAFFE` (29-bit) | transmit | plant responses |

Bit-rate table: 500k, 1M, 250k, 666.667k, 125k (20 MHz CAN clock).

## 4. Session layer [O]

- ISO-TP framing (single / first / consecutive / flow control) and a UDS-style server
  (positive response = SID + 0x40, negative `7F sid nrc`, `0x78` pending).
- Service table in RAM at `0xFFF83710` (20-byte entries: SID, sub-function, session mask,
  security mask, handler); initialisation source still to be traced.
- Memory-access map at `0x240` (area, rights): RAM `0xFFF80000–0xFFF800FF`, download windows
  `0xFFF80100–0xFFF81FFF` and **`0xFFF88400–0xFFF8FFFF`**, `0xFFF90000–0xFFF9FFFF`, flash
  `0x000000–0x1FFFFF`, FCU firmware area, data flash, FCU RAM.
- The session loop calls into a table at **`0xFFF88410`** when a flag is set: code downloaded
  into RAM is run by the boot MAT. The ACDP-2's "Downloading routine data error" fits this.

## 5. The ACDP-2's sessions [B][I]

Earlier 500 kbit/s captures (2026-10-17) show silence of 47 s / 184 s, then `7E1#D7656915`, a
4-byte reply and encrypted 8-byte frames. `D7656915` is not ISO-TP and occurs in no Bosch code:
[I] it is the protocol of the routine the ACDP-2 downloads in the 666.667 kbit/s plant session
(invisible to a 500k listener). For our purpose only the Bosch part (entry, services, download
and start of a RAM routine) is needed; our own routine replaces the ACDP-2's.

## 5a. Bench results (2026-10-17) [B]

The Teensy (no ACDP-2 on the bus) reproduced the entry and holds the session:

- Send `PlantCan` (ASCII) on 29-bit `0x1F0C3CFE`, DLC 8, every 2 ms at 666.667 kbit/s **while the
  TCU powers up**. The boot MAT replies once on `0x1FFFAFFE`: `53 11 A5 00 00 00 00 00` (report
  `0x53`, the RAM-test result of §2). It then stays in the session until the next power cycle.
- The session then speaks ordinary ISO-TP UDS on **`0x7E1` → `0x7E9` at 666.667 kbit/s** (not on
  the 29-bit IDs, and nothing at 500 kbit/s). Tools: `tcu_bench.py plant-entry` to enter,
  `plant-uds` to talk.
- Services that answer (NRC, not "service not supported"): `10`, `22`, `27`, `2E`, `34`, `36`,
  `37`, `3E`. So the session offers SecurityAccess, WriteDataByIdentifier and the download services
  — the reprogramming set.
- Sessions: `10 01` → `50 01 00 32 01 F4` (P2 50 ms, P2* 5 s); `10 60` accepted; `10 02`/`10 03`
  → NRC `0x12`. Default session: `3E 00` → `7E 00`; `22 F190` → NRC `0x31`.
- SecurityAccess: in the default session `27` → NRC `0x7F` (not in this session). After `10 60`,
  send-key sub-functions (`27 02`, `04`, …) → NRC `0x24` (request sequence error, i.e. seed first);
  the seed-request sub-functions are not those odd numbers. The seed/key sub-function layout and
  the algorithm are the next unknown.

**So the FWE problem is bypassed:** this session runs in the power-up window with FWE high, and
`27`/`34`/`36`/`37` are present. Programming flash now needs the SecurityAccess algorithm and the
download/transfer format, which the boot MAT code (and the ACDP-2 write captures) should yield.

## 6. Open items

1. SecurityAccess: which sub-function requests the seed, and the key algorithm (boot MAT code at
   the `0x27` handler; cross-check against the ACDP-2 challenge `D7656915`/4-byte reply in the
   500 kbit/s captures, which may be this exchange seen from the tester side). Passive capture of a
   live ACDP-2 session is not possible — the Teensy on the bus breaks the ACDP-2's power-up (bench
   2026-10-17), so this is from the dumped code.
2. Download (`34`) / transfer (`36`) / exit (`37`) parameters and whether the transfer is
   encrypted or plain once security is open.
3. Whether the downloaded routine must be authenticated (block 0 Auth / SHA-1, spec 06 §6.0a).
4. Which start-up step drops FWE (only relevant for understanding; the session avoids it).
