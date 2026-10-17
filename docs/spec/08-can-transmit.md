# Spec 08 — CAN transmit interface (signals the TCU sends)

Status: **draft.** The periodic application messages on bus 1 are decoded (layout, protection,
period); signal **meanings and scaling are open**.
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference, **[D]** derived.

Data file (project root): `1034420267_can_tx_signals.csv` — one row per field: message, DLC,
period, protection, byte, bit position, length, and the stock routine that writes it.

## 1. Transmit path (behavior to reproduce)

1. The transmission software provides each value through a source function; a **packer** routine
   per message writes the values into an 8-byte application copy of the frame [O]. Multi-byte
   fields are written by small helpers with interrupts briefly disabled so a field is never
   half-updated [O].
2. A table of **8 transmit message objects** (same 0x50-byte layout as the receive objects,
   spec 07) gives each message's handle, period, DLC, buffers and pre-send callbacks [O].
3. Before sending, the pre-send callbacks add the **rolling counter** and the **CRC-8** (§3) [O].
4. Ready frames are placed in one of two priority queues and sent through the CAN driver [O].

Byte order: multi-byte fields are **big-endian**, high bits in the first byte [O].

## 2. Periodic messages (bus 1 = RCAN A+B)

| Message | DLC | Period (raw) | CRC-8 | Counter | Content summary [O] |
|---|---|---|---|---|---|
| `0x037` | 8 | 1 | yes | yes | three 13-bit values (bytes 0–1, 2–3, 4–5) + flags |
| `0x2F9` | 8 | 1 | yes | yes | three 13-bit values (bytes 0–1, 2–3, 4–5) + flags |
| `0x144` | 8 | 2 | no | yes | flags, two 4-bit fields, two 16-bit values (bytes 3–4, 5–6) |
| `0x170` | 8 | 2 | yes | yes | flags, multi-bit fields, four whole bytes |
| `0x1A0` | 7 | 2 | yes | yes | 11-bit (bytes 0–1), byte 2, 13-bit (bytes 3–4), flags |
| `0x2A8` | 8 | 2 | yes | yes | 12-bit (bytes 0–1), 4-bit fields, byte 5, flags |
| `0x178` | 3 | 10 | no | no | flags, 4-bit field, byte 1 |
| `0x3D1` | 4 | 50 | no | no | byte 0 |

**Bench measurement (spare unit, stock firmware, no vehicle, 2026-10-17)** [O]:

| Message | Measured period | CRC-8 | Counter |
|---|---|---|---|
| `0x037`, `0x2F9` | 15.5 ms | valid in every frame | +1 per frame |
| `0x144` | 31.0 ms | none | **constant** (byte 6 high nibble does not change) |
| `0x170`, `0x1A0`, `0x2A8` | 31.2 ms | valid | +1 per frame |
| `0x178` | 155 ms | none | none |
| `0x3D1` | 786 ms | none | none |
| `0x639` | (2 frames in 5 s) | — | — |
| `0x300` (bus 2) | 20.0 ms | valid | +1 per frame |

The raw period unit is therefore **15.625 ms (1/64 s)**, not 10 ms: raw 1, 2, 10, 50 →
15.6, 31.3, 156, 781 ms. CRC-8 (SAE J1850) and counter layout of §3 confirmed on hardware.
Idle payloads without a vehicle (first frames): `0x037` / `0x2F9` = `1F FF 1F FF 1F FF …`
(13-bit fields at `0x1FFF` = invalid), `0x144` = `00 FF 00 00 00 00 00 04`, `0x178` = `F0 FF 80`,
`0x3D1` = `01 00 00 00`, `0x300` = `00 00 FF 00 1x CRC`. [I] The three 13-bit values in the
fast, protected messages `0x037` and `0x2F9` have the shape of torque request/limit values sent to
the engine controller; meaning to be confirmed.

The TCU also receives `0x037`, `0x2F9` and `0x170` back and checks them (spec 07 §5).

Note: comma.ai opendbc names `0x144` "SPEED_2" and treats it as received by other modules; in
this TCU it is a transmitted message (packer and table entry observed). See spec 07 §6.

## 3. Protection

Identical to the receive side (spec 07 §2):

- **CRC-8 SAE J1850** (poly `0x1D`, init `0xFF`, final XOR `0xFF`) over bytes `0 … DLC−2`,
  stored in byte `DLC−1`.
- **Rolling counter** in the upper nibble of byte `DLC−2`, incremented per transmission.
- `0x037`, `0x2F9` and `0x170` additionally maintain a second frame image that is the **bitwise
  inverse** of the first with some fields recomputed [O]. [I] Used by the variant-2 protocol.

## 4. Not covered here

Messages sent by other paths: `0x639` (bus 1), `0x300` and `0x6D3` (bus 2), the diagnostic
response `0x7E9`, and the request/response protocols `0x3B1` and `0x368`/`0x378`/`0x388`/`0x398`
(see the CAN message list).

## 4a. Project firmware (M1)

- `firmware/bsw/com/com_tx.c` sends periodic messages: clears the frame, packs the fields, adds
  the rolling counter (upper nibble of byte DLC−2, +1 per transmission, start value 0 [I]) and the
  CRC-8 (byte DLC−1). A frame that cannot be queued is counted and retried on the next tick
  without advancing the counter.
- `firmware/io/vehicle/fca_tx_fields.h` is **generated** from `1034420267_can_tx_signals.csv` by
  `tools/gen_fca_tx.py` (8 messages, 72 data fields; the 6 counters and 5 CRCs are added by the
  layer). A host test regenerates it and fails if it differs from the committed file.
- `firmware/io/vehicle/fca_tx.c` holds one raw value per field (all 0) and **all messages
  disabled**. Nothing from this table is sent until the field meanings and the values a Ram
  expects are known; the inverted second frame image (variant 2, §3) is not implemented.

## 5. Open items

1. Meaning and scaling of each field (compare with a Ram capture; the TCU's own values can also be
   read over diagnostics).
2. Exact counter start value and wrap behavior; when the inverted frame image is sent.
3. The messages in §4.
