# Spec 15 — Diagnostic services (UDS) of the stock application

Status: draft. Source: application of 1034420267 [O] unless marked. Hand-over to the boot
software for reprogramming: spec 06 §5a.

## 1. Addressing [O]

| Bus | Request | Response | Notes |
|---|---|---|---|
| 1 (CAN-C) | `0x7E1` physical, `0x7DF` functional | `0x7E9` | |
| 2 (shifter / diagnostic bus) | `0x7E1` physical | `0x7E9` | `0x6D4` / `0x6D3` also used on bus 2, role open |

Transport parameters (padding byte, block size, STmin) of the stock transport layer were not
traced. [I] The ACDP-2 talks to the Bosch bootloader once the hand-over is done, so only the
first request (`0x10 0x02` and anything the tool sends before it) reaches the application.

## 2. Service table `0x4553C` [O]

20 entries of 0x14 bytes: SID, session mask, handler, confirmation, configuration.
Referenced from the diagnostic configuration `0x456CC` (count 0x14).

| SID | Service | Session mask | Handler | Configuration |
|---|---|---|---|---|
| `0x10` | DiagnosticSessionControl | all | `0x80A10` | `0x46660` (§3) |
| `0x11` | ECUReset | all | `0x81126` | `0x4585C` |
| `0x14` | ClearDiagnosticInformation | all | `0x8086C` | — |
| `0x19` | ReadDTCInformation | all | `0x81054` | `0x466E0` |
| `0x22` | ReadDataByIdentifier | all | `0x80C78` | `0x45828` |
| `0x23` | ReadMemoryByAddress | `0x20` | `0x812D4` | — |
| `0x27` | SecurityAccess | `0x20` | `0x81490` | `0x46708` |
| `0x28` | CommunicationControl | `0x20` | `0x80808` | — |
| `0x2C` | DynamicallyDefineDataIdentifier | all | `0x741F4` | `0x45728` |
| `0x2E` | WriteDataByIdentifier | `0x20` | `0x81804` | `0x4673C` |
| `0x31` | RoutineControl | `0x20` | `0x81D46` | `0x466A0` |
| `0x3E` | TesterPresent | all | `0x817D0` | — |
| `0x85` | ControlDTCSetting | `0x20` | `0x809C4` | — |
| `0x01`–`0x04`, `0x07`, `0x09`, `0x0A` | OBD modes | all | `0x81FC2` | `0x465D4…0x465EC` |

[I] The session mask is a bit set over the session index of §3; `0x20` = index 5 = session
`0x03` (extended), so the protected services are available in the extended session only.

## 3. Sessions [O]

Session list `0x46927`, handlers `0x4660C`, timing `0x46628` (P2 / P2* in µs):

| Index | Session | Handler | P2 | P2* | Behaviour |
|---|---|---|---|---|---|
| 0 | `0x01` default | `0x75EBE` | 40 ms | 2 s | accepted |
| 1 | `0x60` | `0x75EC2` | 50 ms | 5 s | returns 8 → NRC `0x12` [I] |
| 2 | `0x61` | `0x75EC6` | 50 ms | 5 s | preconditions, then hand-over to the **bootloader** (target 1) |
| 3 | `0x6B` | `0x75F38` | 50 ms | 5 s | preconditions, then hand-over (target 1 if a configuration flag is set, else 2) |
| 4 | `0x02` programming | `0x75EFC` | 40 ms | 2 s | preconditions (variant-specific mask), then hand-over to the **boot manager** (target 2) |
| 5 | `0x03` extended | `0x75F64` | 40 ms | 2 s | accepted |
| 6 | `0x66` | `0x75F68` | 40 ms | 2 s | returns 8 → NRC `0x12` [I] |

Hand-over sequence and record: spec 06 §5a. Negative response `0x22` when the preconditions
fail; `0x78` (response pending) while the shutdown preparation runs.

## 4. Project firmware (M1)

The project application implements `0x10` (`0x01`, `0x03`, `0x02` = hand-over), `0x11 0x01`,
`0x22` (`0xF195` software version, `0xFD00` decoded input status) and `0x3E`, on the
addresses of §1. `0x10 0x02` answers `0x7F 0x10 0x78`, writes the record of spec 06 §5a and
resets; the Bosch boot software then sends the positive response. Allowed only while the
engine is not running.

Bench additions (extended session): `0x31` routines FE10 (solenoid test, M3), FE20 (user boot MAT
copy), FE30 (start the project flasher, spec 16); `0x22` identifiers FD00–FD02 (status) and FD03
(FWE pin level, spec 16).

## 4a. Bench observations (stock firmware, 2026-10-17)

- Physical requests on `0x7E1` are answered on `0x7E9` on **both** buses; single frames are
  padded with `0xAA` (same as the project transport).
- `3E 00` → `7E 00`; `22 F1 95` → `7F 22 31` (identifier not supported).
- `10 01` → `50 01 00 28 00 C8` (P2 40 ms, P2* 2 s); `10 03` → `50 03 00 28 00 C8`.
- The **first** `10 03` after power-up (and a `10 03` on bus 2 while bus 1 held the extended
  session) got **no response**; after a `10 01`, `10 03` was accepted. Testers should send `10 01`
  first. Reason open.
- After these requests the boot request record held the timing words of spec 06 §5a
  (`+0x0C` = 40 000, `+0x10` = 5 000 000, `+0x14` = 5 000, `+0x18` = 500 000, `+0x29` = `+0x2E` =
  `0x18`); the project firmware now writes these values.

## 5. Open items

1. Transport-layer parameters of the stock application (padding, BS, STmin) and the role of
   `0x6D4` / `0x6D3`.
2. The request sequence the ACDP-2 sends before `0x10 0x02` (session `0x03` first? security
   access?). Capture it against the stock application (README bench item 20).
3. Precondition masks of the session handlers (`0x76048` bits) and the shutdown preparation
   `0x728F6`.
4. Whether the boot software needs the timing words (+0x08…+0x18) of the record.
