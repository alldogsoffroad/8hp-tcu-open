# Reference — Nissan Frontier/Xterra CAN (preliminary)

**Status: reference only. Nissan-side work has not started** (see `docs/spec/README.md`). This
file preserves findings from an earlier, preliminary session (Aug–Oct 2026) so they are not
lost. It is **not** part of the specification and has not been re-verified in this project.

Source: the user's own CAN captures on a **2015 Nissan Frontier (VQ40DE + RE5R05A)** made with a
Teensy sniffer, about 64 upshifts analysed with scripts. These are measured values from one
vehicle; tyre size affects the speed-based figures.

## 1. Engine/vehicle signals

From the user's candidate DBC `nissan_frontier_2015_candidate.dbc` (repository
`alldogsoffroad/NissanDBCFilez`):

| ID | Signal | Layout | Scaling |
|---|---|---|---|
| `0x1F9` | engine speed | bytes 2–3, big-endian | 0.125 rpm/bit (mirror on `0x180`) |
| `0x280` | vehicle speed | bytes 4–5, big-endian | 0.01 mph/bit |
| `0x160` | throttle | byte 3 | 0.392 %/bit (message also holds a pedal candidate) |
| `0x421` | gear selector | — | lever position only, not the engaged gear |

Also present in the DBC: wheel speeds, brake switch/lamp, coolant temperature, oil temperature,
`0x580`. Byte order differs per signal (engine speed big-endian, steering little-endian).

## 2. Shift torque-reduction handshake

### `0x177` — torque-reduction request (TCM → ECM, assumed)

DLC 6, about 10 ms period.

| Byte | Content |
|---|---|
| 0, 1 | `0x00` |
| 2 | slow sequence value (seen 01, 02, 0A, 0B; range 00–0C); [I] gear-related |
| 3 | toggles 00/01; [I] gear or shift-type flag |
| 4 | **request level**: 0 between shifts, a flat value for about 150 ms around each upshift |
| 5 | status (seen 08, 10, 18, 78) |

No rolling counter and no checksum (checked over 1,080 frames). **Not confirmed** that the TCM is
the sender (test: unplug the TCM and see whether `0x177` disappears).

### `0x182` — engine torque (ECM)

DLC 8.

| Byte | Content |
|---|---|
| 0 | `0xFF`, constant |
| 4 | delivered torque, about 0.392 %/bit (`0xFF` = 100 %); dips during shifts (e.g. FF → 7D) |
| 5 | low nibble = rolling counter (mod 16) |
| 6 | toggles 00/01 |
| 7 | checksum: not a sum, XOR or two's complement; [I] CRC-8, polynomial not found |

Idle and wide-open-throttle end points of the torque scaling were not pinned.

### Request level by upshift and engine speed (raw `0x177` byte 4)

| Shift | Level @ engine rpm |
|---|---|
| 1→2 | 51 @ 2280–2660; 77 @ ~2960; 102 @ 3300–3650; 205 @ 5810 |
| 2→3 | 83 @ 2130–2280; 128 @ 2420–2510; 154 @ 3130–4830; 160 @ 5360; 167 @ 5810–5850 |
| 3→4 | 51 @ 2620–2690; 77 @ 3040–3530; 102 @ 3760–4100 |
| 4→5 | 0 @ 1810; 13 @ 1950; 26 @ 2200–2480; 51 @ 2480–2730; 64 @ 2960–3670 |

Levels are discrete steps, so this is a lookup table, not a formula. (An early linear fit was an
averaging artefact.) [I] The real axis may be turbine speed rather than engine speed.

## 3. Gear ladder

Engine rpm per mph, as measured: 1st ≈ 106, 2nd ≈ 68, 3rd ≈ 44, 4th ≈ 28, 5th ≈ 22.

## 4. Relevance to the project (later)

- The project firmware would have to **send `0x177`** during upshifts and **read `0x182` byte 4**
  (engine torque) and `0x1F9` (engine speed). This is the Nissan counterpart of the FCA torque
  fields in `0x037`/`0x2F9` (spec 08).
- Open before use: sender of `0x177`, checksum of `0x182`, torque scaling end points, and a
  second vehicle for confirmation.

Unverified notes from the same session: the RE5R05A TCM was said to be a Bosch unit based on an
MPC555; VQ40DE torque about 380 Nm. Treat both as unconfirmed.

## 5. Source material held by the user

DBC `nissan_frontier_2015_candidate.dbc`; logs `2026-09-11_torque_shifts.txt`,
`2026-09-11_shiftdiag_heartbeat.txt`, a ~10-minute 27-shift drive log and a manual-sweep log;
scripts `shift_diff.py`, `shift_traj.py`, `shift_analyze.py`; Teensy code `TorqueCapture.h`,
`TorqueInject.h` and the PlatformIO `shiftdiag` sniffer project.
