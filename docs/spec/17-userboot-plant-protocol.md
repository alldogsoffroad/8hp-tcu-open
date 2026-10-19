# Spec 17 — User boot MAT: plant (production) programming protocol

Status: **plant entry reproduced on the bench with the Teensy (2026-10-17). Session architecture
mapped from the dump and the download path confirmed security-gated (2026-10-19, §5e/§5f).** Source: the unit's own user boot MAT, read out with
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
  security mask, handler); initialisation source still to be traced. (Layout corrected 2026-10-19:
  these entries are the 0x1C-byte connection descriptors of §5e.)
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

**Re-interpreted 2026-10-19 (§5g):** the timeline analysis shows the `D7656915` exchange happens
after the ACDP-2 power-cycles the unit (app traffic ends, 47 s silence, boot side answers in
350 µs, the application never restarts). It is a boot-side power-up session of its own — the
ACDP-2's flash-write path at 500 kbit/s — not the plant session's downloaded routine.

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


## 5b. Feasibility: no asymmetric signature on this path [I]

Block 0 (Auth_TSW, spec 06 §6.0a) is an image-authentication engine: it accumulates data in
≤0x80-byte chunks and verifies it against stored constants (`0x0A`, `0x75`), bit-inverted key
bytes and a checksum (functions `0x2060`/`0x20E4`/`0x1F30`/`0x24C8`/`0x2570` in the application
MAT). That is the **signature-protected in-vehicle reflash** path (`Auth_Prog`).

The plant session is **not** gated by it: the ACDP-2, a third-party tool, wrote seven arbitrary
modified images through this session during this bench session (m4…m4g). A Bosch private-key
signature would make that impossible. So the only gate on the plant write path is SecurityAccess
(`0x27`) — a reproducible symmetric seed/key, which the ACDP-2 satisfies. Extracting that
algorithm is the remaining blocker; it is feasible in principle (symmetric, in the dumped code).

## 5c. Next experiment: locate the seed level [B]

The `0x27` handler is built into a RAM dispatch table at `0xFFF83710` (0x14-byte entries, main
handler at +8, periodic callback at +0xC; `FUN_00001E0C`), so it is not a static table in the
dump. Rather than chase the builder blind, sweep `0x27` on the bench to find which sub-function
returns a seed (`0x67`) and in which session:

    tools/bench/tcu_bench.py plant-seedscan --enter --sessions 60,61,6b

Run it while powering the TCU on (`--enter` sends `PlantCan` during power-up, then sweeps odd
`0x27` sub-functions across the default session and each listed session). In session `0x60`,
send-key sub-functions (even) already return NRC `0x24` (seed-first), so a seed level exists;
this finds it. The key algorithm then needs static RE of that specific handler.

**Result (bench 2026-10-17):** session `0x60`, seed sub-function **`0x27 0x61`** returns a
**16-byte seed** (`67 61 <16>`); the key is sent as `0x27 0x62 <16>`. The seed is **random on
every request** (five requests gave five unrelated 16-byte values), so the key is a keyed
transform of the seed, not a constant. Sessions `0x61`/`0x6B` are not enterable here (`10` → NRC
`0x12`); send-key before a seed → NRC `0x24`. Extracting `f(seed)` is the remaining blocker:
static RE of the `0x27 0x61/0x62` handler in the boot MAT (RAM dispatch table `0xFFF83710`).
The 16-byte/666.667 kHz challenge does not match the 4-byte `D7656915` exchange seen at 500 kbit/s
in the older captures (§5), which is a separate/older entry.


## 5d. Key-algorithm RE: state and obstacles (2026-10-18)

Attempted static recovery of `f(seed)` from the boot MAT dump. Established:

- **No crypto primitives in the boot MAT.** No AES S-box / Te / Rcon and no SHA constants
  anywhere in the dump; no XOR-heavy or table-lookup transform in any of the 108 decompiled
  functions (`0x0`–`0x3A00`, SH-2A, Ghidra). So the key function is either a non-obvious custom
  scramble or lives outside the statically-reached code.
- **UDS dispatch is RAM-table-driven.** The service table is at `0xFFF83710` (= config base
  `0xFFF83698` + 0x78; `0xFFF83710` is never a literal), 0x14-byte entries, main handler at +8,
  periodic callback at +0xC, count at `*0xFFF8394B`. The dispatcher is `FUN_000012CC`, the periodic
  walker `FUN_00001E0C`.
- **The table builder was not located.** The region `0xFFF836xx`–`0xFFF839xx` is not a block-copy
  destination (the only `.data` copies land at `0xFFF84000` and `0xFFF851D8`); it is written by
  field-level accesses through base pointers. The code that stores the `0x27` handler pointer into
  the table was not found among the 108 functions, so the handler address is not yet known. The
  session may also stage code into `0xFFF82000` / `0xFFF88400` (memory-map download windows),
  which needs mapping.

So recovering the SecurityAccess key is a dedicated multi-session RE effort: map the boot MAT's
RAM init precisely (including any `0xFFF82000` staging), find the dispatch-table builder, identify
the `0x27 0x61/0x62` handler, then the transform and its embedded secret. There is **no bench
shortcut** — the seed is random per request and the ACDP-2's key cannot be observed (the Teensy
breaks the ACDP-2 session when both are on the bus).

## 5e. Session architecture from the dump (2026-10-19)

Static analysis extended over the whole 32 KB dump: all functions, the previously unanalyzed code
gaps (0x3CD–0x53A, 0x8F4–0xADB, 0x2293–0x2442), the code tail (0x3696–0x36D6), and both copied
images. Decompilations live outside the repository, as before. The start-up path and the session's
RAM layout are now mapped [O]:

- **Reset path** (vector at +4 → `0x53C`): stack switched to `0xFFF88000` (thunk `0x2DD4`,
  `rts; mov r4,r15`); peripheral/CAN/watchdog init (`0x2DDC`, `0x2C04`, `0x2EA8`); RAM march-tests
  over 0xFFF87400, 0xFFF8F000, 0xFFF84000, 0xFFF88000, 0xFFF8C000 (`FUN_00000E94`); init chain
  `FUN_00000680` (CMT 0x2C60/0x2CC2, loader 0x0E2C, timers 0x2626/0x2598, deadline 0x2570); the
  ROM 0x200 config block copied to `0xFFF850CC`.
- **Scatter loader** `FUN_00000E2C`: zero-fills `0xFFF84000`+0x3400 (`FUN_00001F04`), then walks a
  table at ROM 0x384 (entries src/end/dst, 0xC stride, null-terminated): ROM 0x3E70–0x3EFC → RAM
  `0xFFF851D8` (event/notification block: nine function pointers into 0x8F4–0xA5E, a bit-flag
  event system) and ROM 0x36D8–0x3A50 → RAM `0xFFF84000` (888-byte module, ident `bhie0101UNTD`).
- **The UNTD module is the ROMMAT manager** (at RAM 0xFFF84000): maps/unmaps the boot MAT
  (`0xFFFFA820` ← 0x3B01/0x3B00), checks the marker areas 0x5E00/0x7E00, verifies the ROM 0x200
  config signature — and runs the **boot trampoline**: with the user MAT made visible it reads the
  descriptor pointer at user-MAT 0x210 (→ descriptor at 0x258: a ten-entry Auth_TSW function
  table, high-entropy key data at 0x288+), calls its +4 entry (user-MAT `FUN_00002540`), then maps
  the boot MAT back. `FUN_00002540` stages 16 device-unique bytes from the EEPROM
  (0x8010003A/59/32) into the hand-over area `0xFFF80040–0xFFF80063`. The rest of block 0 is the
  asymmetric Auth_Prog engine (SHA-1 at 0x345A, bignum arithmetic) — §5b's conclusion stands.
- **Plant session start** `FUN_00000714` (modes 5/6): RAM-tests the download windows, sends the
  53-report (`FUN_00000FE8`, mailbox 3), sets supervisor state 0x19, runs the session state init
  (`FUN_00000C0C`: 0x0CD2 installs the 0xFFF851D8 pointer, 0x0AF8 installs 0xFFF8493C ← 0xFFF83698
  gated on the count byte at 0xFFF83946, 0x0B4C resets ISO-TP), then jumps to RAM code at
  **0xFFF82000** and **0xFFF8394C** and enters the main loop.
- **Corrected session-block layout** (supersedes the 0x14-byte-entry reading of §4/§5c/§5d): the
  dispatcher `FUN_00002144` walks **0x1C-byte connection descriptors at 0xFFF83698** — SID at
  +0xC, handler-array pointer at +0x10 (its +4 a timeout word), reply CAN ID at +0x18, reply
  buffer at +8; the count is the byte at `0xFFF83946`; handler code follows at `0xFFF8394C`+.
  ISO-TP receive is `FUN_000010C4`, flow control `FUN_00001674`.
- **Run hook for downloaded code**: when the byte at **0xFFF836A3** is 1, the main loop
  (`FUN_0000105A`) tail-calls `*(*(0xFFF88410) + 8)` with (0xFFF84A6C, [0x10BC]). A downloaded
  image at 0xFFF88400 therefore carries a slot pointer at +0x10 to a descriptor whose +8 is the
  entry. The transfer engine (`FUN_00002308`/`0x22E0`, swap16/swap32 backends at ROM
  0x3698/0x36B0) moves segment lists through the session-block functions 0xFFF8394C/0xFFF8397A.
- **The session block is installed at runtime and no static builder exists**: every boot-MAT
  function (≈130), the UNTD image, the user-MAT block 0 and the EEPROM (data records only) were
  checked — no SID tables, no 27/0x61/0x62 constants, no seed/key scramble anywhere. The install
  descriptor `{0x498, 0xFFF83A60, 0xFFF82000}` sits in the UNTD image header (ROM 0x3716); its
  consumer is runtime code. `D7656915` (§5) also appears in no static dump — that handler is
  runtime code as well. A sibling project (vag-tcu-tools, SH72549 ZF 8HP) has no plant-session or
  UDS-27 work to borrow. Further eliminations (2026-10-19, second pass): the boot MAT only
  **resets** the DMAC (0x2DDC writes the DMAOR key 0x5555 and zeroes the channels — no transfer
  is programmed), so the install is not a DMA copy; the remaining unread start-up functions
  (0xFAC entry-state machine, 0x26DA supervisor SPI handshake, 0x0D00 thunk) are not installers;
  the application's own `0x27` handler (0x81490, spec 15) also dispatches through RAM-built
  tables — no pointer tables into the diagnostic module exist anywhere in the app image; and
  both user-MAT boot trampolines (0x210 → 0x2540 EEPROM staging, 0x6010 marker machinery) carry
  no code callback. Conclusion: the service/security layer of this firmware family is constructed
  at runtime from non-obvious sources; static analysis of the available images cannot reach it.
  Progress needs the bench (hot-tap, §5f) or the runtime block itself.

## 5f. Bench: the download is security-gated; dumper tooling (2026-10-19)

- `tcu_bench.py plant-probe34` (read-only, RAM windows only): in session 0x60, both
  `34 00 44 <addr4> <len4>` and the classic `34 00 33 <addr3> <len3>` → **NRC 0x33**
  (securityAccessDenied); malformed requests → 0x13, so format parsing precedes the security
  check. The RAM-routine path sits behind the same SecurityAccess as the flash write: **one key
  opens both** (open item 2's formats must come from the ACDP-2's traffic or from after the key).
- `firmware/plantdump/` builds a 604-byte SH-2A RAM dumper and a 180-byte canary, laid out for
  the run hook (slot at +0x10 → descriptor, entry at +8; watchdog and supervisor serviced; output
  on mailbox 2, ID 0x7EA); `tcu_bench.py plant-dump-session` downloads them through 34/36/37,
  reassembles the regions and checks the transfer endianness. Ready for when a read path exists;
  nothing in it writes flash. Not usable today: 34 is security-gated.
- **Next experiment — ACDP-2 hot-tap.** §5d's "no bench shortcut" holds only for the entry
  window: the recorded failure was the Teensy disturbing the ACDP-2's power-up. Attaching the
  Teensy **after** the session is established — listen-only, high-impedance, no additional
  termination, 666.667 kbit/s — should expose the ACDP-2's own `27 61 / 27 62` exchange (real
  seed/key pairs) and its 34/36/37 usage. One capture would identify or validate f(seed) and
  settle open items 1 and 2.

## 5g. Offline analysis of the existing 500 kbit/s captures (2026-10-19)

Read directly from the 2026-10-17 ACDP-2 captures already in `bench_captures/` — no bench time.
Findings [B]:

- **Session architecture.** The ACDP-2 first talks plain app UDS (`3E`, `22 F1 90` — the VIN),
  then power-cycles the unit (47 s / 76 s silences), then sends `7E1#D7656915` at 500 kbit/s and
  the boot side answers within 350 µs; the application never runs again during the session.
  Three such sessions in one capture (re-authentication per operation). The write captures are
  this session writing flash: **the D7656915 path is itself a flash-write path**, independent of
  the 666.667 kbit/s plant session — a second route to code-flash independence, with a 4-byte
  seed instead of 16.
- **Challenge protocol.** `D7656915` → 4-byte random seed → tester 8-byte response → TCU 8-byte
  reply → everything after is 8-byte frames in both directions, encrypted (301k frames in the
  data phase, no repeats). Eight complete (seed, response) pairs were collected, e.g.
  `B1A461B7→0D5270DF44529494`, `C96E5817→91878AF764E9AD06`, `2EDEEBB7→550C8F2E0CAB0F77`.
- **Excluded.** XOR/add with constant (byte- and word-wise), byte swaps, CRC32 chains,
  byte-local S-boxes, TEA with structured keys (zero / seed-padded), checksum-dispatch of the
  magic (CRC32/16 of `D7656915` appears in no dump). The post-auth stream is not ECB (no
  duplicate ciphertext blocks over huge known-plaintext regions) and shows no aligned LFSR
  structure (Berlekamp-Massey at full linear complexity) — a real cipher or a counter-mode
  stream with a per-session key.
- **The magic is stored nowhere**: `D7656915` (any endianness, whole or as 16-bit halves) is in
  none of the stock application, bootloader, boot-manager, user-boot-MAT or EEPROM images. The
  handler is runtime code, like the plant session block (§5e).

So both ACDP-2 paths hide their handlers the same way. Offline cryptanalysis is exhausted;
progress needs either the runtime-code install mechanism (§5e open lead: DMAC audit, user-MAT
blocks as a data source, the install-descriptor consumer) or the hot-tap (§5f). The eight pairs
above are ready to validate any candidate transform immediately.

## 5h. Live tap of an ACDP-2 session (bench 2026-10-19)

The Teensy attached to the ACDP-2 harness bus (60 Ω measured at its transceiver = a correctly
terminated bus) in listen-only with automatic bit-rate tracking — fully passive, no ACKs, no
transmissions (the auto-step markers are USB reports only, `teensy_slcan.ino`). Results:

- **The ACDP-2 tolerates the tap completely**: a full flash read ran to completion with the
  Teensy on the bus. Passive listening is safe even during the entry phase — the earlier
  "Teensy breaks the ACDP-2" failure must have been an active/termination artifact, not the
  listener itself.
- The read flow against the installed project loader: no app phase (the identify operation
  aborts on the loader's missing `22 F1 90` VIN DID — NRC 0x31), then a power cycle straight
  into the boot-side session: `D7656915` → 4-byte seed → 8-byte tester response → 8-byte reply →
  encrypted transfer (792 commands at ~12 ms cadence, 8.9k data frames, 56 s). No 666.667 kbit/s
  phase and no rate change at any point: **the ACDP-2's only entry is the 500 kbit/s D7656915
  exchange; it never uses the plant session** (§5's original [I] was wrong, and the hot-tap idea
  of §5f cannot yield `27 61/27 62` pairs — that session is ours alone).
- Ninth (seed, response) pair captured live: `928908FF → C7D556074BAA936B` (reply
  `7EF7F7AC80220917`). Also excluded offline: AES-128-ECB/DES under the 16 EEPROM "device" bytes
  of the boot trampoline (§5e) in four key forms, six paddings, both directions — those EEPROM
  bytes are an `MMSBLANK` placeholder pattern anyway, and the loader's data-flash writes have
  since zeroed them.
- The tester response arrives ~1.7 ms after the seed: the ACDP-2 **box** computes it (no phone
  round-trip fits). The algorithm therefore exists in readable form in the ACDP-2's own firmware
  — the natural next target, since the TCU side is runtime-constructed (§5e).

## 6. Open items

1. SecurityAccess: which sub-function requests the seed (answered: `0x61`/`0x62`, §5c), and the
   key algorithm. The handler is runtime-installed session-block code (§5e), so the practical
   route is the ACDP-2 hot-tap (§5f): the recorded passive-capture failure applies only to the
   power-up entry window; a mid-session listen-only tap may still observe the ACDP-2's own
   `27 61/27 62` exchange and its download usage.
2. Download (`34`) / transfer (`36`) / exit (`37`) parameters and whether the transfer is
   encrypted or plain once security is open. Both formats parse (§5f) but are gated by 0x27;
   the ACDP-2 capture of item 1 would show them in use.
3. Whether the downloaded routine must be authenticated (block 0 Auth / SHA-1, spec 06 §6.0a).
4. Which start-up step drops FWE (only relevant for understanding; the session avoids it).
