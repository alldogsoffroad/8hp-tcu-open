# Spec 03 — SPI devices and transfer schedule

Status: **draft.** Bus, chip-select, frame format and transfer structure are established; device
part numbers and full register semantics are **open**.
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference.

## 1. Buses in use

| Bus | RSPI unit | DMA channels (RX/TX) | Chip selects used | Notes |
|---|---|---|---|---|
| 0 | RSPI-A (`0xFFFFB000`) | 56 / 57 [M] | SSLA0, SSLA1, SSLA2, SSLA3, SSLA5 | SSLA4 pin is configured but never asserted [O] |
| 1 | RSPI-B (`0xFFFFB800`) | 58 / 59 [M] | SSLB0, SSLB1, SSLB2 | also used directly by the CPU at boot and in the shutdown path [O] |
| — | RSPI-C (`0xFFFFC000`) | 60 / 61 | none | pins are configured as RSPI-C, but the unit is only ever disabled [O] |

## 2. Electrical / framing parameters (all jobs)

From the SPCMD words used (`0xAFx1`, one job `0xAF20`) [O], decoded per manual §17.3.12 [M]:

| Field | Value |
|---|---|
| Frame length | 16 bits |
| Bit order | MSB first |
| Mode | CPOL = 0, CPHA = 1 (SPI mode 1); **except** the SSLB2 job: CPOL = 0, CPHA = 0 (mode 0) |
| Chip select | asserted per frame, released after each frame (SSLKP = 0) |
| Clock delay / next-access delay | enabled, programmed per bus (SPCKD / SPND) |
| Bit rate | SPBR = 5, BRDV = 0 → **Pφ / 12**; Pφ (peripheral clock) is open (spec 05) |

## 3. Transfer mechanism (behavior to reproduce)

- Transfers are organised as **jobs**. A job names a bus, a frame count, a sequence of up to 8
  per-frame commands (chip select + format), a transmit buffer and a receive buffer, and a
  completion handler [O].
- The SPI unit runs the command sequence repeatedly until the frame count is reached; A-DMAC feeds
  the transmit data register and drains the receive data register [O][M].
- On bus 0, every job addresses **all five chips in a fixed order**: SSLA3, SSLA2, SSLA1, SSLA0,
  SSLA5. Frame *n* goes to chip *n mod 5* [O].
- A job scheduler queues jobs per bus and starts the next job from the completion interrupt [O].
  Each job record carries an integrity marker that is checked before use; a mismatch is reported as
  an internal error [O].

## 4. Frame format of the bus-0 devices (and SSLB1)

Every 16-bit frame is `[command byte][data byte]` [O]:

- Command bytes with **bit 7 set are writes**; the data byte (or low bits of the frame) carries
  per-chip values from the software's state for that chip [O].
- Command bytes with bit 7 clear are reads [O].
- Command `0x06` is an **identification read**. A valid reply has high byte `0xA2`; the low byte
  of two consecutive replies is stored as the chip's ID [O].

### 4.1 Bus-0 jobs (5 chips each)

| Job | Frames | Commands sent per chip, in order | Purpose |
|---|---|---|---|
| ID | 10 | `0x06`, `0x00` | read identification; reply must start `0xA2` |
| Diagnostic read | 15 | `0x18`, `0x1E`, `0x28` | [I] status / fault registers |
| Configuration | 40 | `0x8E`, `0x90`, `0x96`, `0x88` (writes), then `0x28`, `0x30`, `0x2E`, `0x36` (reads) | [I] configure and read back |
| Cyclic control | 45 | `0x88`, `0xA0`, `0xE8`, `0xA6`, `0xF0`, `0x60`, `0xC0`, `0x66`, `0xD8` | [I] per-cycle setpoint update and status |

In the cyclic job, frames 3 and 5 for each chip (commands `0xE8` and `0xF0`) carry a full 16-bit
per-chip value OR-ed into the command template [O]. [I] These are most likely **two current
setpoints per chip**, i.e. about **10 current-controlled solenoid channels** across the five
bus-0 chips. Confirmed as current setpoints; channel use and solenoid assignment in spec 12.

### 4.2 Bus-1 jobs

| Chip | Commands observed | Notes |
|---|---|---|
| SSLB1 | `0x06` (ID), `0x28`, `0x2E` (reads), `0x90`, `0x8E` (writes), plus a 7-frame and a 4-frame job | same command family as the bus-0 chips → [I] a sixth driver of the same type |
| SSLB0 | `0x03`, `0x09`, `0xC9`, … interleaved with SSLB1 frames | different device; [I] role open |
| SSLB2 | one 16-bit frame, SPI mode 0 | different device; [I] role open |

### 4.3 Complete job list and bus-1 protocol [O]

Job descriptors (`0x409F4…0x40C74`, integrity marker `0x45B9F72D`): frame count, transmit and
receive buffers, chip-select command table, completion handler.

| Descriptor | Bus | Frames → device | Transmit (command \| data) | Handler | Notes |
|---|---|---|---|---|---|
| `0x40A98` | 0 | 10 → chips 0–4 | `06`, `00` | `0x921E0` | ID |
| `0x40AB8` | 0 | 15 | `18`, `1E`, `28` | `0x92298` | status |
| `0x40B28` | 0 | 40 | `8E 90 96 88`, reads `28 30 2E 36` | `0x923BC` | configuration |
| `0x40B48` | 0 | 45 | cyclic (spec 12 §2b) | `0x92530` | setpoints |
| `0x409F4` | 1 | 7 → SSLB1 | `88`×4 (data bytes), `12`, `0C`, `0A` | `0x910EE` | builder `0x91086` |
| `0x40A14` | 1 | 4 → SSLB1 | `84`\|value, `0A`, `12`, `0C` | `0x911D8` | builder `0x91190` |
| `0x40B80` | 1 | 4 → SSLB1/SSLB0 | `06`, `03`, `00`, `00` | `0x92AC0` | identification of both |
| `0x40BA0` | 1 | 5 → B1/B0/B1/B0/B1 | `18`, `0C`, `1E`, `06`, `14` | `0x92BBE` | status of both |
| `0x40BC0` | 1 | 1 → SSLB0 | (built inline) | `0x92CC8` | SSLB0 status nibble |
| `0x40BE8` | 1 | 4 → B1/B0/B1/B0 | `8E`\|`0xFFF907D4`, `C9`\|`0xFFF9097B`, `22`, `09` | `0x92D94` | **control words**, read back and compared |
| `0x40C0C` | 1 | 2 → SSLB1 | `90`\|enables, `24` | `0x92ED4` | **output enables** (`0x92E80`) |
| `0x40C30` | 1 | 2 → SSLB1 | `90`\|enables, `24` | `0x92FD0` | same, from the fault path (`0x92F7C`) |
| `0x40C54` | 1 | 2 → SSLB1 | `28`, `2E` | `0x93096` | 10-bit value + flag → `0xFFF907CC` |
| `0x40C74` | 1 | 1 → SSLB2 (mode 0) | `0x00FF` | `0x931D4` | 16-bit value → `0xFFF908EE` |

Reply headers: SSLB1 `0xA8` (mask `0xF900`, same family as the bus-0 drivers), SSLB0 `0xAA`
(mask `0xFE00`).

[I] Roles, from the data flow:
- **SSLB1** is a driver chip of the same family with switch outputs. Its register `0x90` gets
  `0xFFF907D6` (`0x91758`): bits 5–7 are set when switch outputs (state letter `'c'` in
  `0xFFF90944[…]`) are on, bit 3 from `0xFFF90952`, plus bits from `0xFFF90963`. The transmit
  value is masked with `~(status & 0xE0)`, so outputs reporting a fault are switched off.
  Its register `0x8E` gets the control word `0xFFF907D4` (`0x91C86`).
- **SSLB0** is a different device (supervisor / system chip). Its register `0xC9` gets
  `0xFFF9097B` (`0x91C86`, bits from `0xFFF90957`, `0xFFF90962`, `0xFFF9094D`, `0xFFF9095A`); one
  status read is stored inverted as a 4-bit status `0xFFF90958`.
- **SSLB2** returns a 16-bit value each cycle; meaning open.

The steady-state values of the SSLB1 `0x8E`/`0x90` and SSLB0 `0xC9` words, which are the
likely output-enable path, still have to be evaluated (spec 12 §4).

### 4.4 SSLB1 = external supervisor / watchdog, and the output enables [O]

**SSLB1 is the external supervisor** of spec 05 (open item "external supervisor on RSPI-B"):

- Both boot programs service it directly from the CPU every **5 ms** (bootloader `0x1D0B4`, boot
  manager `0x30E40`): write `0x8E00 | bits` to RSPI-B, with **bit 2 toggled** (0x04 / 0x00) on every
  call while running normally; bit 6 = 0x40 and bit 7 / bit 4 in special boot states.
- The application keeps the same register in its control word `0xFFF907D4` (`0x91C86`, periodic task
  `0x8EE82`, job `0x40BE8` with read-back `0x22`): **bit 2 = complement of the bit 2 read back** (so it
  toggles every cycle), **bit 0 = output enable** (`0xFFF90953`), bit 1 from `0xFFF90964`, bits 3/4/7
  cleared in normal running, bit 6 in state 6.
- Output enable: `0xFFF90953` = 0 and `0xFFF9094A` = 1 at initialisation (`0x91DD2`). Once the start-up
  checks pass, the monitor `0x873F0` calls `0x6C612(1)` → `0xFFF9094A` = 0, `0xFFF90953` = 1. The
  hand-over to the boot software (spec 06 §5a) calls `0x6C612(0)` → outputs disabled.
- When the enable changes, `0x920A4` also switches the MCU's own watchdog: enabled → WTCR out of
  reset mode (interval timer, IPR05 low nibble = 0xE); disabled → back to reset mode,
  `WRCR = 0x5A80` (`0x978A4` / `0x97850`). [I] While outputs are active the external supervisor is the
  watchdog that matters.
- Register `0x90` of SSLB1 holds switch-output enables (`0xFFF907D6`, `0x91758`): bits 5–7 follow
  switch outputs 0–2 (`0xFFF90944[i]` = `'c'` when on, set by the switch-output manager `0x85828`
  via `0x8C7A8`); bits 5–6 are cleared when `0xFFF90969` is set (calibration mask `0x03`);
  calibration bits `0x1918EB/EC/EE/EF` are 0 in stock data. Faulty outputs are masked with
  `~(status & 0xE0)` before sending.

**SSLB0** register `0xC9` (`0xFFF9097B`): calibration base 0, **bit 4** set while the shutdown-path
self-test (`0x86916` via `0x8C808`) clears `0xFFF9094D`, **bit 7** from `0xFFF9095A` (variant-dependent,
`0x8CE48`). Normal running value [I]: `0x00`.

**CPU-mode access** (as the boot software and the application's fatal path do, `0x9210C` /
`0x1D430`): disable the RSPI-B DMA requests (`0xFFFE6074` / `0xFFFE6076` = 0), `SPSR = 0`, `SPPCR = 0`,
`SPBR = 5`, `SPDCR = 0`, `SPCKD = 0`, `SSLND = 0`, `SPND = 1`, `SPCMD0 = 0xAF11` (SSLB1, 16 bit, mode 1),
`SPCR = 0x48`; then write a frame to `SPDR` (`0xFFFFB804`) and poll `SPSR` bit `0x20`. The bootloader
routine `0x1D478` sends `0x8E01` followed by eleven `0x8401`. Bench-confirmed 2026-10-30: the explicit
**start trigger after the SPDR write is mandatory** (read SPSR with SPTEF set, then write 0) — without
it the peripheral never clocks; this matches the driver-chip exchange (spec 12 §2d). In flash mode
(`0xFFFF1000` bit 2) the boot software halves the rate: `SPBR = 0xB`.

**Cyclic-task state machine** (`0x1D0B4`, twin `0x30E40`; wrapper `0x139FE` suspends it while flash
activity is flagged `0xFFF84946`): the data byte of `0x8E00|data` follows a state byte
(`0xFFF856F7`): state 6 → `0x40` (bit 6); state `0xE8` → one-shot `0x1D478` (`0x8E01` + 11 × `0x8401`),
then `0x04`; states `0x19`/`0x31` (set by `0x1389A`: flash mode / normal) → bit 7 (flash request,
`0x19` only) with the bit-4 flash-toggle handshake (`0x10` until acknowledged, then 0, gated on
`0xFFF800EC ≤ 0x3F`); otherwise the bit-2 toggle. After 120 000 unanswered frames (10 min at 5 ms)
the state clears and `0xFFFE3902` takes `0x10` (and loses `0x20`). Each pass ends with the internal
watchdog feed `0xFFFE0002 = 0x5AB7` (`0x5ADB` in flash mode).

**Boot manager reads** (`0x3D84` in the boot MAT, via the one-word helper `0x22CC` with a
1000-iteration settle): sends `0x2800` then `0x2E00` and harvests `(reply & 0xF) << 6 | (reply & 0x3F)`
— 10 configuration bits. The supervisor therefore shares the driver-chip command grammar
(`0x28` status, `0x2E` read), and its **reply frames are `0xAAxx` and `0xA8xx` class** (DMA
completion handler `0x92AC0` validates the headers and stores status to `0xFFF90977/78` and
`0xFFF907CF/D0`).

**Bench 2026-10-30/31 [M] — the 0x8E data byte, empirically:**
- **bit 2 = the watchdog life sign.** A *constant* word (no alternation) makes the supervisor
  reset the TCU in a loop that self-breaks after minutes; the word must alternate every frame.
- **bit 6 = power-down.** `0x8E40` held: the bench supply showed the TCU's own draw collapse
  0.169 A → 0.018 A, latched > 4 min. "State 6" in the stock state machine is therefore the
  shutdown/sleep path, not normal driving.
- **bit 0 = instant reset** — in every shape tried: the bare 5 ms word (2026-10-30), the full
  cyclic pattern with toggle + ID-job enumeration, and the boot burst `0x8E01` + 11 × `0x8401`
  itself (the request dies before its UDS response). On this supervisor bit 0 is a
  reset/attention command, or the stock's "output enable" reading of `0xFFF90953` needs a
  precondition not yet found (the app's own WDT-mode switch at `0x920A4` on enable changes is
  the remaining untried stock behavior).
- The stock's reply masks are loose: `0xF900` accepts both `0xA8xx` and `0xAAxx`, `0xFE00`
  accepts `0xABxx` — the class bit that varies is data-dependent, not per-chip.

**Bench 2026-10-31 (FE13 + full decompile):** the stock's enable is a *confirmation
handshake* — the app sets bit 0 and only switches its own watchdog to interval mode when the
`0x22` echo shows the bit latched (`0x91C86`, `0x978A4`); our supervisor's echo stays `0x00`,
so the enable never latches and asserting it resets the TCU (also at PWM level 6/31, with
enumeration and full cyclic context). **The PWM level register is the one register that
latches**: `0x84 <level>` (level = stock setpoint/200; start-up `0x4B0` → 6, shutdown-path
`0x1838` → 31) and the `0x0A` read returns the achieved level — levels 1/6/7/0x1F all track
on the bench. The stock checks this feedback with retry/fault-latch (`0x6C742`), gates
outputs on level ≥ 6 **and** the echo bit (`0x6C63E`), and the fine 32-bit duty job
(`0x409F4`: `[0x29xx ×2, 0x88xx ×4, 0x12]`) reports the level in its replies (`0x910EE`).
**SSLB2 is alive** (fourth reply class `0x0Cxx`, live data — the stock's cyclic `0x00FF`
job reads a varying 16-bit value). Register `0x24` = the `0x90` register readback (reads
`0x00` — the `0x90` write still never latches); `0x29` mirrors the `0x28` status (`0x45`).

**Bench 2026-10-31 afternoon — the decisive correction:** the data-flash loader (spec 18 §5)
is the code-flash *application* on this unit since 2026-10-18. **The stock application has not
run during any supervisor bench work** — every "mute stock" observation (refused DIDs, silent
CAN, dead measurement protocol, no engagement) was the minimal loader, not a fail-safe stock.
The stock app was overwritten when the loader was installed; the 2026-10-29 "pristine restore"
repaired only the 0xE0000 marker block. A stock-firmware run therefore requires restoring the
application region (and, suspected, the stock data-flash identification ring the boot reads —
0x0F10/0x0F11/0x0F15/0x0F20 records; the 2026-10-17 stock run had a populated ring, today's is
blank). The stock-run campaign state is recorded in spec 18 §8c.

**Supervisor register map, live (FE13 lab, SSLB1 unless noted):** `0x06` → `0x14` (ID),
`0x18` → `0x80`, `0x1E` → `0x0A`, `0x22` → control echo (`0x00` with the idle word; latches
and echoes ≈ every other frame when the full three-job pattern runs), `0x28` → `0x45`,
`0x2E` → a drifting counter (0x1F–0x25, follows the 5 ms activity), `0x30`/`0x36` → `0xFF`,
`0x14` → `0xD5`, `0x00` → `0xBC`. **SSLB0:** `0x06` → `0xD0` (ID), `0x03` → `0x42`,
`0x0C` → `0xDF`, `0x28` → `0xFF`. Register `0x90` switch-bit writes (bits 5–7) are accepted
without error but nothing observable changes (driver-chip status stays `0xA240`, config reads
stay 0x00 — spec 12 §2d).

**Project firmware:** `firmware/bsw/sup` (from commit after M2) sends the control word every 5 ms
with bit 2 toggling, using the CPU-mode set-up above (start trigger included), and checks the
`0xA8` reply header; the heartbeat `0x7F0` byte 1 reports the result. Output-enable requests are
**removed** after the 2026-10-30 finding above; the request-OR API (`SUP_EN_BENCH` |
`SUP_EN_PARK`) is dormant until the start-up register init is replayed. The supervisor timeout
is open.

## 5. Shutdown behavior

On a fatal internal condition the software reinitialises RSPI-B, sends a frame on SSLB1 from the
CPU, and then deliberately stops in an endless loop [O]. [I] The design relies on an external
device (watchdog / supervisor) to cut the outputs and reset the controller. A reimplementation
must provide an equivalent fail-safe path.

## 6. Open items

1. **Identify the chips.** The firmware does not name them. The reliable route is physical:
   photograph the markings of the ICs on a spare controller board. Their datasheets then define
   the full register set — and they are the best source for driver behavior.
2. Map each driver channel (chip, channel) to a transmission solenoid (needs spec 02 timers and
   the shift-control analysis, or bench measurement).
3. Decode the per-chip state record (28 bytes per chip in RAM) fields that feed the write frames.
4. Job timing: rate of the cyclic job and its relation to the control loop.
5. ~~Roles of SSLB0 and SSLB2~~ — SSLB1 is the supervisor (§4.4); SSLB0 a second supervisor /
   system chip; SSLB2 open. Supervisor timeout and the meaning of SSLB1 registers `0x84`, `0x88`
   (written at start-up) are open.

**Bench 2026-10-31 evening (M4b gate session, engaged-stock shadows + replay ladder):**
- **Engaged-stock supervisor shadows** (three live captures, RAM read via cmd 0x2A):
  control word `0xFFF907D4` = **`0x91`/`0x81` alternating — bit 7 held, BIT 4 the life sign,
  bit 0 the enable** (not the boot software's bit-2 toggle); echo `0xFFF907D5` = `0xb1`/`0xa1`
  (bit 0 SET = accepted); `0xFFF90953` = 1 (enable request), **`0xFFF90954` = 1 — the
  confirmation the output gate checks**; `0xFFF90944[0]` = `'c'` (switch output 0 on);
  `0xFFF907D6` = 0x20 (the 0x90 shadow).
- **Decoded:** the reply processor (`0x91ACA`) sets `0xFFF90954 = echo bit 0`; the word
  builder (`0x91C86`) derives bit 7 from a supervisor-status branch (`0x90951 & 0x11 ==
  0x11`), bit 2 in the normal branch as the complement of the echo's bit 2, bit 6 in state 6,
  and ORs the enable from `0xFFF90953`. The output gate (`0x6C63E`) requires **level ≥ 6 AND
  `0xFFF90954`** and then sets the master RAM gate `0xFFF800E8`.
- **Replay ladder (our firmware, ammeter 0.169 A flat throughout):** PWM level 31 (readback
  `AA 1F` ✓, the one latching write), `0x88 ← 0x20`, `0x90 ← 0x20`, the six-job cyclic
  pattern incl. SSLB2 — then 500 mA channel bursts (FE10) and the FE11 park-hold magnet
  (TOE31, direct path): **no current through any output; both paths share the upstream
  switched supply rail, which never energises.**
- **`0x88 ← 0x20` removes the instant reset on the held enable bit** (the TCU survives bit 0
  in the pattern — previously fatal) — but the `0x22` echo still reads `0x00`: the
  supervisor receives the enable without accepting it. **Bit 7 held (word shape `0x91`)
  reboots the TCU** (uptime-verified) despite the full context — the stock's bit 7 is
  legitimised by the status cell `0xFFF90951` (writer untyped in the decompile: a reply-path
  alias — the next lead).
- Housekeeping: FE13 lab patterns need a **continuous tester-present session** — separate
  `uds` invocations let the bench session time out and clear the lab state between commands.
  FE13 gained type 8 (`sup_app_pattern3`): caller-selected life-sign bit, default-0x20
  switch byte, single-frame request.

**Decompile 2026-10-31 night (reply-path dig, fresh Ghidra import + AddrXref):**
- **`0xFFF90951` is the supervisor MODE byte, not a fault flag.** Writer
  `FUN_0007DC76`: `= 0` normally, `= 0x31` when `0xFFF93B8D ≠ 0` (a system-mode
  condition set through the trivial setter `0x6BC76`, called via function pointer from
  the mode manager `0x68830`/`0x8316C`, gated on system state `0xFFF93B42 == 3`);
  `== 6` is the power-down state. The word builder's bit-7 branch fires on
  `(mode & 0x11) == 0x11` — i.e. **mode 0x31 is the engaged stock's operating mode**,
  and its word shape (bit 7 + bit-4 toggle) is that mode's LEGAL dialect. The NORMAL
  mode shape is the bit-2 toggle we already send. Our bit-7 reset = speaking the 0x31
  dialect outside that mode.
- The disabler `FUN_000932F2` clears the enable (`0xFFF90953 = 0`, `0xFFF9094A = 1`)
  on a job-reply condition (param bit 0x40, counter `0xFFF907D8`).
- `FUN_00091C86` enable dispatch (fresh decompile): enable-requested + echo bit 0 →
  calls the WDT-switch path; without the echo it goes nowhere — confirming the echo
  latch is the gate.
- **Bench consequence:** the enable-pattern replay behavior is NONDETERMINISTIC
  run-to-run — identical sequences (state-set + normal shape + enable) survived twice
  (17:22, 17:44 run 1) and rebooted the app twice (17:31, 17:44 run 2). The supervisor
  keeps latched state across application reboots (it stays powered), so its internal
  phase/counter differs per attempt. Blind replay has hit its limit; the decisive next
  step is a logic-analyzer capture of a live engaged stock run (word sequence, echo
  responses, job interleave — one capture replaces all guessing).
- Tooling: `tools/ghidra_scripts/AddrXref.java` (address xrefs + decompile, house
  style).

**Start-up register order decoded (2026-10-31 late night — the rail-gate sequence):**
The supervisor jobs are flash templates installed by `FUN_000937F8(&flash_descr)`; the
literal word sequences (read from the ELF) and their callers give the stock's order:

1. **Init `0x91DD2`:** enable cleared, flags set, cyclic ID job `0x40B80` installed —
   template `[0x0600, 0x0300, 0x0000, 0x0000, 0x0100, 0x0402]` (register reads 0x06/0x03/
   0x00/0x01 + …). The 5 ms word runs the NORMAL dialect (bit-2 toggle) while mode = 0.
2. **Monitor `0x873F0` state 0x45 → `0x8CEDC` → duty arm `0x6C802(duty, 0x1E)`:** duty
   computed by the calibration walker `0x7EE8A`, stored at `0xFFF92414`, state set to 7.
   The gate pass (`0x6C63E → 0x6C1BA`) then sends the **duty job `0x409F4`** — template
   `[0x8800, 0x8800, 0x8800|hi, 0x8800|lo, 0x1200, 0x0C00, 0x0A00]`: four 0x88 words
   (the 16-bit duty split into words 2/3), the **commit word 0x1200**, then 0x0C/0x0A reads.
3. **Level job `0x40A14`** (via `0x91190(level)` from `0x6C1BA`'s other branch, a ramp
   state machine at `0x94FE8`): template **`[0x8400|level, 0x0A00, 0x1200, 0x0C00]`** —
   the 0x84 write is followed by the achieved-level READ, the 0x12 COMMIT, and a 0x0C read.
   Our earlier bare `0x841F` omitted the commit and the reads.
4. **Enable:** when the monitor's checks pass and level < 7 → `0x6C612(1)`; the 5 ms word
   carries bit 0; the echo bit 0 confirms; the WDT switches to interval mode. Outputs gate
   on level ≥ 6 (cell `0x93BA1`, harvested from feedback `0x90550` by `0x6C1EA`) AND the
   confirm `0x90954`.
5. Mode transitions (0 → 0x19/0x31 via the system-mode chain) switch the word dialect.

**Key discovery: the `0x1200` commit word.** Every stock register write job ends with
`0x1200` — a commit/execute command we have never sent. Our bare `0x841F`/`0x8820`/`0x9020`
writes lacked it; the level happened to latch anyway, but the duty job — never sent at
all — plus the commits are the outstanding deltas. The duty VALUE (`0xFFF92414`, output of
the `0x7EE8A` calibration walker; first guess 0x1838 from the setpoint math) is readable
from live stock via cmd 0x2A next time stock runs.

**Replay test ready:** all job templates fit the existing FE13 one-shot burst (sub-04),
so the sequence is testable on the bench without stock: duty job → level job → enable
pattern, ammeter + echo as the verdict.

**Replay executed (2026-10-31 late night, flash app, ammeter verdict):** the decoded
start-up sequence was replayed live — duty job `[8800 8800 8818 8838 1200 0C00 0A00]`
(accepted: `AA00`×4 + commit ack `AAC5/C7`), level job `[8406 0A00 1200 0C00]`
(**achieved level 6 latched, read-back `AA06`**), then the enable pattern — on both a
warm and a fully power-cycled (fresh) supervisor. **Result: no rail current in any run
(0.167 A throughout), and the enable arm reboots the flash app deterministically (3/3)**
(the RAM app survived it 2/4 — nondeterministic there; the echo never latched anywhere).
Remaining deltas for the next session: (a) the REAL duty value — our 0x1838 is a guess;
read `0xFFF92414` from live engaged stock via cmd 0x2A (needs a stock restore cycle);
(b) the monitor's level RAMP (0x8CEDC dwell states, not a single level write);
(c) diagnose the flash-app reboot at the enable arm (crash vs supervisor reset —
read the reset-cause registers after a repro).

**Challenge-response decoded (2026-10-31 session 2 — the duty "value" is a rolling token):**
A fresh AddrXref pass over the ramp cells dissolved all three deltas at once. The stock's
"duty job" payload is **not a physical duty — it is a challenge-response token**, which is
why every blind replay (0x1838 and friends) left the rail closed:

- **The 0x0C read reply is the challenge.** Both job-completion handlers parse it into the
  feedback byte `0xFFF90550`; the harvester `0x6C1EA` splits it: `selector = 0x90550 & 0xF`
  (→ `0xFFF93BA6`), `achieved level = (0x90550 >> 4) & 7` (→ `0xFFF93BA1`). The `0x0A` read
  reply → `0xFFF9054F` = commanded-level echo; the `0x1200` commit reply → `0xFFF90551` =
  status bits, compared `& 5` against the ramp marker `0xFFF93BA5`.
- **The token formula is fully static:** the calibration walker `0x7EE8A` returns
  `~(xor_table[selector] ^ 0xAAAA ^ 0xFFF93320)`, where `0xAAAA` is what `0xFFFC1C04`
  deterministically holds at walk return (the walker sets it at entry; `0x6BE34` never
  writes it), `0xFFF93320` has two flash initializers that AGREE
  (`flash[0x7F424]^0xAAAA` = `flash[0x8D20C]` = `0xB32F8A6A`), and the xor table lives at
  flash `0x4526C` (pointer cell `0x7F12C`). All dumped from the ELF:

  | sel | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
  |-----|---|---|---|---|---|---|---|---|
  | token | FF0FF000 | B040BF4F | E919E616 | A656A959 | 75857A8A | 3ACA35C5 | 63936C9C | 2CDC23D3 |

  Table entries pair-differ by exactly `0x4F4F4F4F` (0↔1, 2↔3, …) — a scrambler, not
  physics. Wire form: token bytes OR'd onto the four template words → e.g. sel 6 =
  `[8863 8893 886C 889C …]`. The real duty template is **8 words** (16 B at `0x40CA6`):
  `[8800 8800 8800 8800 1200 0C00 0A00 0C01]` — the trailing `0x0C01` word we never sent.
- **The level ramp law:** init `0x6C12C` sets level cells 7, commanded `0xFFF94FE8 = 0x3F`,
  time-per-level `0xFFF93848 = level×200`; `0x6C542` raises target +1 per
  `level×200 + 2400` ticks; `0x6C48C(time)` kicks the LEVEL job (`level = time×0x148>>16`
  = time/200) via `0x6C0BA` (which sets job-selector `0xFFF93B9B = 7` and bumps the target);
  `0x6BF9C` (job-selector = 0) programs the dwell timer (0xFFFFF5B0) and kicks the DUTY job;
  on echo/level mismatch the harvester forces `0xFFF93BA4 = 7` → `0x8CEDC` state 0x2A →
  `0x6C720` → bump + resend level job. While achieved level == 0 the duty arms cycle
  contexts 0 → 7 → 0x19, and **state 0x19 sends the COMPLEMENT of the token** (`~uVar2`).
  Once level ≠ 0 the steady context is 0x1E (plain token). All owned by the monitor task's
  dispatch table (0x876C4–0x87AA0).
- **Consequence for the plan:** the queued stock-restore duty read is UNNECESSARY — the
  token is computable from the selector our own bursts already receive. Open item settled
  next bench run by printing every reply position: whether last night's `AA06` was the 0x0A
  echo (level echo 6 — commanded accepted) or the 0x0C feedback (achieved 0, selector 6).
  The remaining unknowns are only procedural: the supervisor's acceptance criteria for the
  token/commit chain, and the enable-arm reboot cause.

**Token exchange proven live (2026-10-31 session 2, host scripts v2/v3):** replaying the
jobs with computed tokens against the live supervisor: `AA06` was the **0x0A echo**
(level echo — commanded level accepted); the 0x0C feedback carried the challenge.
Answering each challenge rotates it: observed trace `8,12 → 9,13 → 2,6 → 3,7` (residues
march 0-3 with +4 inside each), and the 0x1200 commit status byte's bit 0 correlates
perfectly with rotation (0 = challenge advanced, 1 = held) — the supervisor announces
its rotations. The table is **16 entries** (selector = full low nibble; beyond 0x452AB
starts the walker's node descriptors — RAM addresses + sizes at 0xFFF925xx/0xFFF940xx).
Levels echo every command exactly (6→`AA06`, 7→`AA07`). **Architectural find: the FE13
one-shot bursts stop the cyclic 5 ms pattern** (exchange counters freeze from the first
burst; a re-arm does not revive them) — and the acceptance echo (0x22 bit 0) lives in
that stream, so host-scripted replay can never latch it.

**Native engage sequence (firmware `sup_engage`, same session):** the stock's law now
runs from the 5 ms tick itself — read-only probe `[0A00 0C00 0C01]` harvests the live
challenge, duty jobs answer `token(sel)`, the level ramps 1→7 (300 ms/job, duty arms
between), HOLD continues token rounds; the enable bit joins the 0x8E word only at
level ≥ 6. FE13 sub-09 arms it, results mode 3 reports state/sel/lvl/rotations/tokens;
raw one-shots are refused while it owns the bus. Flash image `m4i_engage_IFla.bin`
(CRC FEAF929A) installed via the engine writer (the op-01 verify diffs after
programming are the known stale-read artifact — one op-08 resets the read path; the
patched installer verified clean). Bench result: **probe found the supervisor still at
level 7, straight to HOLD, 249 token challenges answered with zero bad replies over
75 s, the cyclic stream unbroken the whole time (echo counters ~100/s).** The
challenge sat at selector 0 unrotated ("held") under the clean stream — rotation
appears to be the supervisor's MISS/retry behavior under the stuttering host-burst
regime, not a requirement.

**Enable arm rebooted the app — with full context (same session):** the enable
request on the live sequence (HOLD, level 7, tokens flowing, 0x90 = 0x20) reset the
TCU within a second (request unanswered, app back in default session `7F 31 7F`;
1/1 on this build, 3/3 the previous night on the same step). Conclusion: **the enable
bit in the normal (bit-2 toggle) dialect is rejected regardless of context.** The
engaged stock never sends that combination: while enabled the word is `0x8E91/0x8E81`
— bit 7 held, bit 4 toggling, **enable riding bit 0 of the same shape**. The untried
legitimate move is the dialect switch on top of the live token chain (sup pattern3,
0x91/0x81). Note for that test: bit 7 outside engaged-class state rebooted the box
once (2026-10-30) — but never with the token chain running.

**Engaged-dialect test (same session, final step):** the dialect switch via FE13
sub-08 taught us two mechanics before it taught us the answer. (1) **The 8-byte
sub-08 request is multi-frame, and multi-frame RX fails while the engage sequence
owns the 5 ms tick** — the CF window is missed, the request is silently dropped
(no reply, NO execution; the earlier v2/v3 multi-frame duty bursts worked only
because the cyclic pattern was dead then). Single-frame forms (≤ 7 bytes) always
get through: sub-08's 3-byte form (`31 01 FE 13 08 91 10`, bits90 defaults 0x20)
is the bench-safe shape. (2) The **`0x8E91/0x81` engaged shape with the enable bit
reset the TCU too** — delivered, executed, reboot within a second, ammeter profile
identical (brief .001 dip, back to 0.167). So with 350+ clean token rounds at
level 7 behind it, **both enable-bearing word shapes are rejected**. Open
decomposition: engaged shape WITHOUT the enable (`0x8E90/0x80`) — if that survives,
the enable bit alone is the trigger and the missing legitimiser is the stock's
mode-0x31 chain (system state `0xFFF93B42 == 3` → `0x6BC76` → mode byte 0x31), i.e.
a state the supervisor expects the app to reach BEFORE enabling; identifying that
chain's wire side (a register job or a CAN-level state) is the next decode target.
Also queued: reset-cause capture at app init (crash-vs-supervisor proof), and the
`jobs` counter in the engage status is uint8 and wraps at 256 (cosmetic, note for
the next results reader).

**Grant-law firmware + cold watch (2026-11-01, commits 3f23794):** with the echo-
gated enable installed (`m4k_echogate_IFla.bin`), the engage ran from a cold
power cycle for 150 s: probe → straight to HOLD (the 0x0C feedback nibble read
level 7 seconds after power-on — the nibble wanders even cold) → **498 token
challenges answered, zero bad, no reset, enable shadow never spent** (`en=101`:
requested, from-start, never sent). The supervisor granted nothing (0x22 echo
flat `AA00`, challenge held at sel 0). Findings: (1) the grant law is SAFE and
correct — resets are now impossible from our side; (2) our simplified ramp never
runs when the feedback nibble reads ≥ 6, but the STOCK does not trust that
nibble — it initialises commanded to 0x3F, lets the harvester flag the mismatch,
and drives the **bump-and-resend level-job loop until the 0x0A ECHO matches**
(0x6C12C init → 0x6C1EA mismatch → 0x6C720 → 0x6C0BA; the echo 0x9054F is the
sync signal, not the fb nibble). That exchange is the top remaining candidate
for what the supervisor wants to witness before granting. Also decoded this
session: the driver register jobs are RUNTIME state-sync from the current-
control loop (0x901A0 sends on channel-state change; senders 0x9225C/0x92344/
0x924B0), not a boot one-shot; the installer 0x937F8 is a job QUEUE (10 cyclic
+ 5 register slots with integrity pairs); `AA45` is likely just the normal
reply to the 0x9020 switch word (mode-2 readouts with bits90=0x20 didn't exist
before 2026-10-31). Next levers, ranked: (1) the 0x3F-init level exchange in
sup_engage; (2) driver register sync with the stock's exact job words; (3)
reset-cause capture at app init.

**Level exchange bench-proven (2026-11-01, commit a67d4da, image m4l):** the
echo-confirmed ramp ran from a cold power cycle — **levels 1..7 then the engaged
0x1F, each confirmed by the supervisor's 0x0A echo (8/8, zero mismatches, zero
retries, ~4 s)** — the first fully witnessed ramp; the feedback nibble wandered
6↔7 throughout exactly as predicted (the echo is the truth). HOLD at commanded
31 with the token chain running and the enable shadow unspent; **still no grant**
(0x22 echo flat AA00, ammeter flat 0.167). This isolates the last big structural
delta: **our driver transport is RSPI-A (plat_drv_xfer: SPCRA/SPCMDA — the
supervisor never sees a word of it), while the stock's driver register sync runs
on RSPI-B with the five-select rotation af31/af21/af11/af01/af51 — the
supervisor's own select IN the rotation, every driver word on its bus** (write
template 8E 90 96 88 28 30 2E 36 per chip, reads 18 1E 28 88 A0 E8 A6 F0 60 C0
66 D8; jobs 0x40A9E/0xAB8/B28/B48, senders 0x9225C/0x92344/0x924B0 from the
current-control loop 0x901A0). **Driver-sync campaign (2026-11-01, commits 2bf23db..54639d8):** the five-select
read job (12 regs x SSLB3/2/1/0/5, every 2 s) runs clean — chips answer (24
strict-AA + mixed per job; our supervisor answers AA-class on everything, so
a8:0 is expected). The zero-value write pass (m4n) also survived — no grant.
The LIVE-VALUE write (m4o, values swept from the engaged stock's RAM:
0x8E=0x55, 0x90=0x02, 0x96=0x9F, 0x88=0) **RESET the TCU**: with a fresh RSPI
setup the command sequence restarts at SPCMD0, so the supervisor's select
received 0x8E55 as a control word — bits 0,2,4,6 = an ungranted enable plus
the POWER-DOWN bit. The stock never does this because its rotation phase is
CONTINUOUS across jobs. Fix (m4p, `54639d8`): driver WRITES go to the four
chips only (af31/21/01/51, 32 words, plat_sup_xfer4); the supervisor stays
in the read rotation where it only sees bare reads. **The m4p run: the full
decoded commissioning — tokens, echo-confirmed level exchange to 0x1F,
live-value driver writes, periodic reads, enable as an obedient shadow — runs
stably and indefinitely with NO reset, and still no grant** (0x22 echo flat,
ammeter 0.167). **Setpoint + dither (2026-11-01, commits 241ae12/4e9f118):** the 0x2D job decoded
and sent — [88 A0 E8 A6 F0 60 C0 66 D8] x chips with the 11-bit current
setpoints in E8/F0 (two per chip, stock sender 0x924B0 positions). Static
keep-warm class 0x1141: no grant. The DITHER (periodic jobs alternating
60-word reads with setpoint passes cycling the engaged stock's captured
command sequence 1141/1134/113A/1127/110E/111F/113C/1126/1144, m4r): stable,
587 tokens + 92 driver jobs in 180 s, **the supervisor answered 5 words in
its own 0xA8 class inside the driver jobs** (first time — its reply class
varies by word/phase), still no grant, ammeter flat 0.167. Both passes ride
the four-select rotation (supervisor excluded from writes). The keep-warm
presence AND liveness hypotheses are both negative in static/dither form.

**ID poll + auto-engage (2026-11-02, commits 684d39b/0244d90, images m4s/m4t):**
the 0x0A job decoded ([0x0600 x5, 0x0000 x5] five-select chip-ID poll,
0xA2xx replies into the 0x91108 channel block — sender 0x921A0/handler
0x921E0) and implemented as the commissioning opener: ID poll -> write ->
setpoints -> reads/dither. Bench (m4s): healthy, 591 tokens + 98 driver
jobs, no grant — **the job registry is EXHAUSTED**; the question is formally
outside it. Then the TIMING hypothesis (m4t, TCU_AUTOENGAGE): the
commissioning self-arms at boot (the stock's grant latches within seconds
of power-up; our host-armed runs were minutes late). Bench: pattern +
full sequence from app start (counters live from the first readable
second), **240 s / 25,641 frames on watch, zero grants, ammeter flat
0.167** — on-time complete lawful commissioning, still withheld. Also this
session: the ACS712 bench ammeter deployed (±3 mA, tools/bench/ammeter.py,
telemetry in the Teensy bridge protocol §7) and a blown bus-1 transceiver
driver diagnosed by error signature (bit errors, no ACK errors, one-
directional) and fixed by the bus-2 module swap; /tmp/storm_and_probe.py =
the one-process installer chain (storm -> session probe -> engine ->
ToolSession -> installer, no re-init gap — retired the flaky two-process
flow; NOTE it sets S9 itself after a fix — keep_rate alone inherits
whatever the last chain left at S6).

**The five-chip map (2026-11-02 night, FE13 sub-0A, image m4u/1d4d819):** the
five-select probe (word i -> select i, one-hot verified) mapped the RSPI-B
devices: **af31 silent** (no chip); **af21 = SSLB2 — answers EVERYTHING with
a rolling counter** (0x0ECx..0x0F6x, +4/job — this IS the sslb2_rx counter
seen in every mode-2 dump); **af11 = the supervisor** (reg 0 = 0xBC, data
registers read 0x00); **af01 = the second system chip** (reg 0 = 0xBB,
0x24 = 0xFF under our driver writes — the engaged stock's shadow 0x07D7
reads **0x20**); **af51 = a fifth chip** (0x24 = 0xBB). The stock's job2/job3
handlers (0x92ED4/0x92FD0) cross-check af01's 0x24/0x2E replies against the
0x90 switch word (mask 0x15) and store the reply at shadow 0xFFF907D7 — a
cross-chip confirmation loop. **Write attempts on af01's 0x24**: 0xA420 (M3
reg|0x80 convention) with and without the 0x1200 commit — the register
holds 0xFF (low byte) with a live high nibble (a/ab varying = status).
The SSLB2 counter discovery also explains one old mystery: the mode-2 out[25:26]
byte was never noise.

**CLOSED 2026-11-02 (evening, one-shot campaign on m4u):** `0x24` is the
switch-state ECHO and `0xFF` is the chip's REFUSAL byte, not a writable
register. Handler 0x92ED4 disassembly (sh2a, big-endian): the reply is stored
RAW — `mov.b r4,@0xFFF907D7` keeps the reply's low byte untransformed, so the
engaged stock's shadow `0x20` IS the raw reply; the cross-check is
`(reply ^ sent_word) & 0x15` (pass flags `bset #4` into 0x07C8+word) — the chip
echoes the SENT data when it accepts. Engaged stock: sends `0x9020`, chip
answers `AA20`, pass. Bench matrix (five-select one-shot, ammeter live
throughout — dual-port Teensy firmware this same evening): idle af01 answers
EVERY read word with its reg-0 ID (`aabb`); after `0x90`-register traffic the
0x24-slot reply becomes `AAFF` — for enables 0x00 AND 0x20, single pair and
100× sustained at 91/s, ammeter flat throughout (no output conduction either
way); the status decays back to the ID echo ~1 s after traffic stops. The
armed auto-engage rotation (main.c `sup_app_pattern2(true, 0, 0x20, true)`)
has been sending job2 `90|0x20 + 24` continuously since boot with the same
`FF` answer — same words as the stock, different chip state. Conclusion:
`0x24 = 0x20` is a SYMPTOM of the energized/engaged state (per the app's own
`~(status & 0xE0)` masking, outputs reporting a fault are switched off), not a
lever into it — the refusal is downstream of the grant/rail. Free instrument:
the moment our rotation's 0x24 reply flips `FF → 20`, the SSLB1 output stage
is live.

Remaining leads, ranked: (1) the 0x2D job — the 11-bit
CURRENT SETPOINTS (the stock's keep-warm machine commands real currents from
boot; our 0x88 stays 0 — the supervisor may require seeing commanded load);
(2) the 0x0A-class job (10 words, handler 0x921E0, never decoded); (3) the
rotation-phase fidelity question; (4) system-mode wire consequences.

Next lever: a driver-sync job on RSPI-B with the
stock's exact template interleaved into the 5 ms stream alongside the engage —
the commissioning the supervisor has never witnessed from us.
**Structural decode 2026-11-01 (job registry + the enable's true order):** a hexdump
of the job-table region (0x40980–0x40E00) decoded the full descriptor format —
32-byte records `[01 xx n xx | staging RAM | reply RAM | template ptr | status cell
| completion handler | cfg ptr | 45B9F72D]` — giving the COMPLETE supervisor job
registry: cyclic ID/mixed/job1 `[8E C9 22 09]` (handler 0x92D94, staging shadow
0xFFF88424)/job2/job3/SSLB2, the register duty (0x910EE) and level (0x911D8) jobs,
FOUR 5-select driver jobs (handlers 0x921E0/0x92298/0x923BC/0x92530) whose
templates write `8E 90 96 88 28 30 2E 36` and read `18 1E 28 88 A0 E8 A6 F0 60
C0 66 D8` per chip — the never-run driver commissioning (register 0x96 etc.).
**There is no mode-set register job** — the mode-0x31 chain is app-internal, and
since the engaged shape is accepted in mode 0 anyway, the mode theory for the
enable is dead. The decisive find is in the monitor `0x873F0` (fresh decompile):
after the gate poll it latches **`0x6C612(1)` — the enable — on its FIRST pass,
one-shot via `0xFFF93B94`, while the level is still 0**. The stock's bit 0 rides
the 5 ms word from the very first frame of the ramp (still normal dialect; the
0x91 shape comes later with mode 0x31), and the echo acceptance arrives DURING
the enabled ramp. Our sequence inverted this — enable only after level 7 in
HOLD — i.e. an enable appearing mid-stream after start-up, which is exactly the
violation the supervisor resets on. Firmware: `sup_engage` gained the stock-order
arm (FE13 sub-09 `in[1]=2`: enable from the first frame), image
`m4j_engagestart_IFla.bin`. The decisive bench test: fresh power cycle (cold
supervisor, level 0) → engage armed enable-from-start → the supervisor WITNESSES
the 0→7 ramp with bit 0 present from frame one → echo bit 0 + ammeter verdict.

**Decomposition closed (2026-10-31 final test):** the engaged shape WITHOUT the
enable — `0x8E90/0x8E80`, single-frame sub-08 `31 01 FE 13 08 90 10` — **survived
the full 45 s watch with the token chain running, and the supervisor answered it**:
the 0x22 echo moved from flat `0xAA00` to **`0xAA90`** (mirroring the engaged base
bits through the bit-5-masked echo), and the 0x90-register reply changed
`0xAA00 → 0xAA45` (a dialect-acknowledgment status we had never seen; bits
0x45 = 0100_0101). Echo counters climb 1:1 — the arithmetic of the bit-4 toggle
against the constant echo, as expected. **So: bit 7 and the bit-4 life-sign are
accepted in our state; the enable bit ALONE is the reset trigger, in both
dialects.** The rail-gate question is now precisely: what legitimises bit 0? The
stock's own sequence between the ramp and `0x6C612(1)` is the mode-0x31 chain
(system state `0xFFF93B42 == 3` → setter `0x6BC76` sets `0xFFF93B8D` → mode byte
`0xFFF90951 = 0x31`), and possibly the supervisor needing to SEE a fresh 0→7 ramp
(our level 7 came from its own latch, not from a ramp it observed) or a longer
correct-token history. Next decode targets, in order: (1) the wire side of the
`0x6BC76` mode-set — find what it writes (register job or supervisor word field);
(2) reset-cause capture in our app at init (crash-vs-supervisor proof for the
enable reset); (3) a fresh power-cycled supervisor with the engage running from
cold, so the ramp 0→7 happens under observation.

### 6.1 Mode manager decoded (2026-11-02 evening; target (1) above answered)

Static decode, `sh2a` big-endian (`sh-elf-objdump -m sh2a -EB`, image vma
`0x08000000 + file offset`). Target (1) resolved: **`0x6BC76` is a one-instruction
latch thunk** — `mov.b r4,@0xFFF93B8D` (twin `0x6BC7E` → `0xFFF93B90`) — called by
the mode machinery, never a register job. The full structure:

- **Mode byte `0xFFF93B42`**: values 0–5 valid, **15 = locked** (the setter
  `0x68776` rejects any change into/out of 15; new mode must be ≤ 5). On a valid
  change it stores the mode and copies a per-mode config byte from the 6-entry
  table at **flash `0x46814`** = `[00, 02, 01, 00, 10, 00]` into shadow
  `0xFFF84A48` (mode 4 carries bit 0x10 — the same switch-bit family as SSLB1's
  0x90 bits).
- **Leaving mode 3** (`0x68776` tail): word at peripheral `0xFFFE0400 |= 0x3C04`
  and `0xFFF94710 = 0` — real hardware consequences on the mode-3 exit. The byte
  pair `0xFFF94710/0xFFF94711` is the **engaged latch**: bit 0 of `0xFFF94711` is
  set by the mode-3 applier path and gated in a dozen readers across the app
  (0x6886C, 0x689D8, 0x68B5A, 0x78BC6, 0x7C738, 0x90B4A, 0x952A6, 0x98000,
  0x98B80, 0x98CA2, 0x98CD0).
- **Applier `0x8316C`**: per-mode 16-byte entries at **flash `0x46764`** →
  `[RAM struct ptr | dest | power-of-two index | 0x60]`, structs at
  `0xFFF80134+` (stride 0x188), destinations `0xFFF89018+` (stride 0x60),
  indices 1/2/4/8/0x10/0x20 for modes 0–5. When **mode == 3** and the struct's
  active byte is 1: calls `0x6BC76(1)` (sets latch `0xFFF93B8D`), then bulk-copies
  8/4/2-byte arrays from the struct's lists into the destination — a full buffer
  world-switch on entering mode 3.
- **Mode 3 transitions run through a TAG state machine `0x68C04`**: tag cell
  `0xFFF80C4C` (32-bit ASCII); branches on **"WPRQ"** (`0x57505251`) and
  **"RPRQ"** (`0x52505251`); the paths write **"WPER"/"WPAK"/"RPAK"**
  (`0x57504552`/`0x5750414B`/`0x5250414B`) and **call the mode-3 entry
  `0x6884E` directly** (twice in one path). The WPRQ path reads a further
  record at `0xFFF8010C` (fields `+8 == 0x11`, `+28 == 1`, args `+32/+36`) and
  a word from peripheral port `0xFFFFAD00`. The tag successor function
  `0x68D52` operates on a record at **`0xFFF89000`** — the head cell of the
  mode-destination array (`0xFFF89018+0x60*mode` follow it): `+0` must be 3,
  `+4 == 0x100`, `+8` the tag — **"FBRQ"** (`0x46425251`) answered with
  **"FBAC"** (`0x46424143`, latch `0xFFF94711` bit 0 permitting, counter
  `0xFFF9472C := 10`) or **"FBER"** (`0x46424552`); FBAC with counter 0
  proceeds to `0x68ADA` with args from `+12/+16/+20`. Full tag family:
  WPRQ/RPRQ, WPER/RPER, WPAK/RPAK, FBRQ/FBAC/FBER.
- **Dispatch** (case bodies at `0x7CB00+`): mode indices 0–5 tail-jump to the
  applier with their index; **index 6 runs the chain `0x68C04 → 0x68D52 →
  `0x6A7F6` → `0x68B58`** (the tag machine + successors, still undecoded).
- The mode-set entry also checks `0xFFF94FE4` (values 5 and 10 steer
  calibration-region handling, `0x180000`).

Address-correction note (same evening): the first pass of this section
carried three wrong address conversions (FBRQ record 0xFFF884E8 → actually
0xFFF89000; engaged latch 0xFFF927D1 → 0xFFF94711; cal select 0xFFF9095C →
0xFFF94FE4). The "RSPI staging neighborhood" connection was an artifact of
the first error — the FBRQ record is NOT in the RSPI staging area; it heads
the mode-destination block.

Open: the tag SOURCE — what writes `0xFFF80C4C` and the `0xFFF89000` record
with ASCII tags (only `0x68D52` references 0xFFF89000 directly; the writer
must use a computed pointer — suspect the block initializer of the
`0xFFF89000` mode-destination area). That is the engagement trigger surface.
`0x6A7F6`/`0x68B58` undecoded; the RAM structs' runtime builder (who fills
`0xFFF80134+`) unknown.

### 6.2 The engagement handshake at boot (2026-11-02 night, second decode)

The mode-3 entry has a PRIMARY path that runs **before the scheduler starts**:
the startup chain at `0x66CC0` (init calls in order, ending in the scheduler
`0x9524A` and a deliberate halt) calls the handshake pair directly:

- **Trigger `0x67200`**: reads the input word at **`0xFFFC0D00`** (peripheral
  block based `0xFFFC0C00`); values 9/10 → state letter **85**, 13/14 →
  letter **86** stored at `0xFFF93B43`; writes the request magic **`0x1234`
  to `0xFFFC0D04`**.
- **Sibling `0x6728E`**: waits up to **667 × 100 ms** for `0xFFFC0D04` to be
  **cleared by the counterpart** (the acknowledgment). On clear:
  **mode-set(3)** + reply magic `0x5555` (letter 85 path) / `0x3737`
  (letter 86 path) written back to `0xFFFC0D04`; on timeout: mode-set(0),
  letter `0xF0`, and the peripheral word `0xFFFE0400 |= 0x3C04`.

So the stock enters the engaged system mode **at every boot**, gated on a
hardware counterpart acknowledging through the `0xFFFC0D04` mailbox.

- **Module clocks** (addresses corrected from the disassembly 2026-11-04; the earlier
  note conflated two registers): the stock's low-level init `0x9533C` writes the
  module-stop protocol as a KEY at `0xFFFF0802 := 0x69FF` followed by the VALUE at
  `0xFFFE0802 := 0x5555`, plus `0xFFFF0842 := 0x68FF` and the RAM access-cycle
  `0xFFFF0812 := 0x7821/0x7800` (chip ID `& 3 == 0` selects 0x7821; this unit's ID
  `0x0021` → `0x7821`), enables the ROM cache `RCCR (0xFFFC1400) := 1` + flush, sets
  `0xFFFE0400 |= 0x3C10`, and reads the chip ID at `0xFFFF1000`. Our app configures
  none of this; a 2026-11-04 mirror of this subset (E4 build) ran without harm but the
  `0x5555` value read back `0x0000` — the write did not verifiably land
  (write-through-key semantics or rejected without further protocol).
- **Trigger input gating** (disassembled 2026-11-04): only input values 9/10 (letter 85)
  and 13/14 (letter 86) start the handshake — **any other value writes letter 0 and never
  sends the 0x1234 magic**. Input 10 additionally mode-sets(3) inside the trigger.
- **Bench status**: our firmware's measurement gate (`readable()` in
  `plat_sh7254r.c`) whitelists only flash ≤ 0x1FFFFF and RAM
  0xFFF80000–0xFFF9FFFF — the all-`0x0E` live reads of `0xFFFC0C00` and
  `0xFFFE0800` tonight were **our own DENIED_BYTE**, not the bus. Whether the
  counterpart module exists and what `0xFFFC0D00` holds on our bench is
  **undetermined**.

NEXT (2026-11-03): an m4-series build extending `readable()` with the I/O area
(0xFFFC0000–0xFFFFFFFF), then live-read `0xFFFC0C00–0xFFFC0D10`,
`0xFFFE0800–0xFFFE0842`, `0xFFFE0400` and the chip ID at `0xFFFF1000`.

### 6.2a The mailbox ruled off the drive path; the grant decoded (2026-11-04) [O]

The readable() extension shipped with the m4v source; the live reads plus four
instrumented boots (E2–E4) and a re-read of the 12:49 driving-state RAM dump
settled the question, against the mode-3 hypothesis:

- **The input word on this bench is 15 (0x000F) at power-up** — captured at the first
  instruction of `main()`, identical across three boots, constant through a 512-sample
  1 ms history, and 0 by the time CAN reads are possible. 15 is outside the stock's
  {9,10,13,14} gate: with this input **the stock itself would not handshake**.
- **The driving stock never ran the handshake**: in the dump taken while it drove the
  mechatronic at 1.773 A, the mode letter `0xFFF93B43` reads `0x01` — not 85/86 (and not
  the timeout letters 0xF0/0xF1). Mode 3 is not on the drive path.
- **The grant while driving**: the cyclic word builder `0x91C86` loads the enable from
  **bit 0 of byte `0xFFF907D5`** (the supervisor's 0x22 echo, cached), branches past the
  enable when clear (the A-law, confirmed in the code), and composes the outgoing word
  byte at `0xFFF907D4`. In the driving dump: `0xFFF907D5 = 0xA1` (**bit 0 set — the
  supervisor had granted**), `0xFFF907D4 = 0x81` (enable + toggle sent), enable shadow
  `0xFFF90953 = 1`. Our firmware reaches the same ladder end-state (HOLD, level 0x1F
  commanded and echoed, commissioning values held, dither cycling) but every cyclic
  register read returns `0xAA00` — the supervisor answers the ladder and stays mute in
  the cyclic conversation.
- **The open question**: what in the stock's cyclic RSPI-B job stream earns the grant.
  The stock's cached conversation (`0xFFF907C8+`: `14c7 000a 81a1 2020 …`) is richer
  than the all-`AA00` replies we receive; decode that cache and diff against our
  captured replies to find the divergent job or step.

### 6.2b The staged conversation, reconstructed from the driving dump (2026-11-04) [O]

The stock's job table (`0x40A60–0x40C80`) is a list of descriptors — `{code, count?, TX-ptr,
RX-ptr}` — whose staging buffers live in RAM (`0xFFF881xx`, `0xFFF884xx–0xFFF886xx`), all
inside the 12:49 driving-state dump. Read together they reconstruct the exact cyclic
conversation the stock kept while driving the mechatronic:

| Job | Words (TX) | Replies (RX) | Note |
|---|---|---|---|
| 0x0A (ID) | `0600 ×5, 0000 ×5` | `A212 ×4, 0000` | chips ID; select 4 silent by design |
| 0x28 (write) | `8E9F ×5, 9021 ×5` | `A200 ×4, 0000` | **supervisor ignores the write job**; live data `8E9F/9021` |
| 0x2D (setpoint) | `881A/1A/1A/12/15, A000 ×5` | `A200 ×4, 0000` | supervisor silent |
| 0x05 (read set) | `1800 0C00 1E00 0600 1400` | `AA00 AADF AA0A AAD0 AAC7` | **supervisor answers `0x1400` with data** |
| cyclic job1 | `8E81 C900 2200 0900` | `AA00 AA00 AAA1 AA00` | **control word carries bit 0; the `0x22` echo mirrors it (`0xA1`)** |
| job2/3 | `9020 2400 / 2800 2E00` | `AA00 AA20 / AA46 AA05` | |
| unidentified | `C070 80B0 D090 30A0 E0D0 7050 80B0 D010` | | a duty-class cyclic job we have never sent; near it `0x119F` (keep-warm setpoint family) and `FF40 8000 8630 8640 5E50 7680 B640 FF00 8A80` |

Bench same-day probes of the same jobs from our firmware (ladder-complete state,
one-shots): job 0x05 returns `AA00 AADF AA0A AAD0 AAD5` — **byte-identical on the first four
words**; the supervisor's live `0x1400` value reads `0xD5` (bit 0 set). The supervisor serves
us the same read set as the stock; it is not protocol-gating us.

**Synthesis.** The `0x22` register echoes the app's own control word: the stock sends
`0x8E81` (enable + toggle) every cycle and the echo returns `0xA1`; we send `0x8E00` — the
A-law withholds bit 0 pending an echo that by construction mirrors what we send. The
never-tested branch is whether the supervisor, in the ladder-complete state we demonstrably
reach, **accepts and echoes our enable bit** (grant) or **rejects it** (the 2026-10-30 reset
class). The mis-framed `0x8E55` one-shot of 2026-11-04 hung the exchange but did not reset —
the reset class appears narrower than "any unsolicited bit 0".

### 6.2c The A-law bypass experiment — refused by hang, not by reset (2026-11-04) [O]

FE13 sub 12 (E6 build) forces the enable bit into the cyclic control word for a bounded
window. Verdict across three windows in the ladder-complete state:

- **No reset** — the 2026-10-30 reset class did not fire; the app, heartbeat and status
  frames continue throughout.
- **The supervisor hangs the exchange on the enable word**: every UDS request inside the
  window goes unanswered, the session starves (subsequent requests refused `NRC 0x7F`
  SNSIAS), and the bench interlock cancels a running FE10 the moment the window opens
  (`end=00`). The behavior matches the mis-framed `0x8E55` one-shot hang: the supervisor
  refuses to clock the bit-0 word from us.
- Therefore the stock's continuously-sent `0x8E81` works because its supervisor is in an
  accepting state ours never reaches. **The gate is upstream of the enable bit.** The
  leading candidates from the §6.2b reconstruction: the unidentified duty-class cyclic job
  (`C070 80B0 D090 30A0 E0D0 7050 80B0 D010`), the live write data (`0x8E9F/0x9021`), the
  setpoint job's per-chip flags (`881A/1A/1A/12/15`), or the reply classes our stream has
  never drawn (`ABxx`, `98xx` — the latter observed in our own capture while at HOLD).

Sequencing rules for the next attempt (all learned the hard way): a window without a live
setpoint is uninformative; FE10 must start before the window (its test is autonomous);
observation must ride the 0x7F3 frames (the UDS server does not survive the window);
re-arming the lab resets the ladder (minutes of re-climb) while the boot ladder continues
underneath.

### 6.2d Our side of the conversation — the same-day diff table (2026-11-04) [O]

One-shot probes from E6 at the ladder-complete boot, against the §6.2b stock column:

| job / register | ours | stock (driving) |
|---|---|---|
| cyclic job1 `8E00 C900 2200 0900` | `AA00 AA00 AA00 AA00` | `AA00 AA00 AAA1 AA00` |
| job2 `9020 2400` | `AA00 AAFF` | `AA00 AA20` |
| job3 `2800 2E00` | `AA45 AAFF` | `AA46 AA05` |
| job6 (5 words) | `AA00 AADF AA0A AAD0 AAD5` | `AA00 AADF AA0A AAD0 AAC7` |
| ID `0600 ×5, 0000 ×5` | `AA14 AAD0 ×…` (data class) | `A212 ×4 0000, A2BD ×4 0000` (ID class) |
| `0x0A` level | `AA1F` | same class |
| `0x0C` fb | `AAF3` | live |
| `0x22` echo | `AA00` | `AAA1` |
| `0x28 / 0x2E` | `AA45 / AAFF` | `AA46 / AA05` |
| `0x14` (supervisor) | `AAD5` | `AAC7` |

The conversation is fundamentally correct: job1 and job6 match the stock byte-for-byte
except the echo and the live values. Three precise divergences remain — (1) the `0x22`
echo (the gate), (2) the commissioning-touched registers (`0x24`, `0x2E`) reading the
never-written `0xFF` for us — our five-select write still does not land where the stock's
lands, and (3) the ID class: the chips return data-class (`AAxx`) to our `0x06` reads
instead of the stock's ID-class (`A2xx`), suggesting a register-phase or select-mapping
difference in our RSPI framing. The `0201`/`0402` descriptor headers (words, select?) and
the duty-job composer (`C0xx/D0xx/Exxx`) are the next decode targets.

### 6.2e The header decode, the bus-0 proof, and the duty job (2026-11-04, evening) [O]

**Descriptor headers.** The cyclic-entry second word is `{frame count, select index}`:
`0x0402` = 4 frames / two-command alternate (cfg `0x40B74` = `AF11 AF01`, even frame →
SSLB1, odd → SSLB0), `0x0502` = 5 frames (same two-command run), `0x0201`/`0x0101` = 2/1
frames, and the commissioning class (`0A05`/`2805`/`2D05` = `{register, count}`) uses the
five-select cfg `0x40CE4` = `AF31 AF21 AF11 AF01 AF51`. Full entry layout (job `0x40BE8`
= cyclic job1): `{0100, count|select, TX-ptr, RX-ptr, select-cfg-ptr, status-cell
(0xFFF908xx), completion-handler (reply checker, e.g. `0x92D94` checks the AA class and
sets status bits), trailing-records ptr, 45B9F72D}`. The `0004 0xxx` words are 32-bit
flash pointers (byte-greps miss them; SH-2A `movi20` immediates encode RAM addresses as
signed 20-bit, e.g. `0xFFF8844C` = −490420).

**The commissioning jobs are bus-0 jobs.** The live TX staging of the write job
(`0xFFF88194`, 40 words, register-major 8 rows × 5 chips) is served by descriptor
`0x40B28` — the §4.3/§4.1 bus-0 configuration job — and its live RX staging reads
`A200 ×4, 0000`: four real driver chips plus the dead fifth select, the exact bus-A
signature of our own FE12 dumps. The bus-B five-select never draws A2-class replies
(E4/E5: `0x0000` from the fourth select). **The E4/E5 conclusion "the supervisor ignores
write/ID jobs" is retracted — those experiments emulated bus-0 jobs on bus B.** The
platform.h note that the supervisor "sits in the rotation and witnesses" them was a
misread of cfg `0x40CE4` (bus-0 SPCMD values); corrected 2026-11-04.

**The live write rows** (driving dump, staging `0xFFF88194`): `0x8E ← 0x9F`, `0x90 ←
0x21`, `0x96 ← 0x55`, `0x88 ← {1A, 1A, 1A, 12, 15}` per chip, then bare reads
`28/30/2E/36`. The `0x9F` low byte `0x1F` is **degenerate at HOLD**: spec 12 §2b decodes
the `0x8E` data as the PWM-frequency divider `32000/f − 1` + calibration bit 7 — `0x1F`
= divider 31 = *f* = 1 kHz — while the ladder level at HOLD is also 0x1F. Which law the
stock applies while the ladder climbs is open (both predict `0x9F` at HOLD; a mid-ramp
staging capture would separate them). Our October ramsweep had attached the shadow bytes
{55, 9F} to the wrong registers, and `0x02` from a loop constant to `0x90`. The mode
flags decode as per-channel modes: chips 0–2 = `(CURRENT, CURRENT)`, chip 3 =
`(CURRENT, DISABLED)` (line pressure current-mode, park release disabled), chip 4 =
`(OFF, OFF)` — the engaged driving pattern. The setpoint job's `E8`/`F0` rows are
`(register << 8) | 12-bit setpoint`; live values `{853, 589, 58A, 853, 800}` /
`{589, 589, 53, 0, 0}`.

**The duty job.** The unidentified staging tail at `0xFFF8844C` is a 40-word stream
seeded from flash template `0x40ECC` (composer `0x96900`: engine reset `0x967F4` →
byte-copy of the 128-byte template → DMAC channel at `0xFFFE6000` armed for an 80-byte
move toward `0xFFF885E8`, the live duty array, 161 words terminated by magic `0x5B97`,
checked at `0x8F816`). The 40 words are **eight five-select rotations**: slot 2 (SSLA1)
receives `0xD0 | duty[r]` each rotation — live duties `90 10 20 40 40 60 00 00` — while
the other selects carry fixed maintenance words (SSLA2 `0x80←0xB0` constant, SSLA0
`0x30/0xF0` alternating, SSLA5 `0xE0←0xD0` constant, SSLA3 the varying slot-0 row). A
parameter table at `0x40DD8` carries `{0003, D090}` — the first duty as data.

**E7 (`m4z`, CRC `499C62A0`)** implements the corrections: `sol_engage_config(level)`
re-runs the bus-A configuration job with `0x8E = 0x80 | level` on every ladder change
(stock live `0x9F` at HOLD — under the level reading this mirrors the ladder, under the
§2b divider reading the ramp values are benign higher PWM frequencies with zero
setpoints commanded; the divergence is observable via FE12 after boot); the engaged
channel staging matches the live `0x88` flags (all pressure channels CURRENT at zero
setpoint, park release DISABLED); `sol_duty_job()` sends the 40-word burst one-shot
(bench routine FE13 sub 13, replies in the mode-3 results, bytes 182–198); the wrong-bus
emitters in `sup_drv_poll` are retired. The open question the burst answers: whether the
supervisor's accepting state (the `0x22` grant) requires the duty stream / the
correctly-valued commissioning — i.e. whether the gate is closed by anything we can now
send.

### 6.2f E7 live: the supervisor woke up — enforcement, not ignorance (2026-11-04, evening) [O]

E7's first bench session changed the supervisor's behavior class:

- **Registers moved.** During the ladder's 7→`0x1F` transition the never-written registers
  read live values for the first time (`0x24` = `AA45`, `0x2E` = `8315`); at steady HOLD
  they settle to `0x24`=`AAFF`, `0x28`=`AA45` (stock `AA46` — one state bit), `0x2E`=
  `ABFF` — the class nibble moved A→B relative to E6's `AAFF`. The supervisor demonstrably
  watches something E7's commissioning now does (bus-A law, engaged modes, or the tighter
  bus-B stream after the retirement).
- **The duty job lands.** FE13 sub 13 (stock live duties) is accepted with the ammeter
  flat at the 0.167 A idle: the slot-2 chip (SSLA1) answers the eight `0xD0` writes with a
  new reply class — `A7FF` for the first write (duty `0x90`; cyclic-header family, data
  `0x7FF` = full scale), then `AFFF ×7`. The writes reach the chip; the rail stays gated.
- **The reset trip.** A bench current test (FE10, ch 0, 300 mA, 5 s) now ends in a **hard
  reset at ~4 s** — twice observed via heartbeat-uptime anchor arithmetic (the "dead bench,
  reason 00" reads after each trip were the fresh app's `bench_init()` state; the E6-era
  "session interlock cancels FE10" reading of the same signature is corrected). The
  ammeter is flat through every trip: the supervisor resets on the **sustained setpoint
  attempt** (~3–4 s), not on current. E1's 2 s FE10 "ran to completion" by ducking under
  the trip window. Bench rule until understood: current tests ≤ 2 s while ungranted.
- **Verdict: E7 moved the supervisor from ignoring to enforcing.** In E1–E6 it ignored our
  current attempts; now it watchdog-resets on them. A dead monitor does not enforce — the
  commissioning chain (bus-0 write values + engaged modes + the level law) advanced it
  into the active-monitor state. The `0x22` grant remains the one missing piece, and
  attempting outputs without it is now *punished* rather than ignored.

Remaining differences from the stock's stream, in suspicion order: the duty job's cadence
(the stock sends it cyclically; we sent it once), the exact `0x8E` ramp law (level vs
1 kHz divider — degenerate at HOLD), and the reply-class details (`ABxx`). Sequencing
notes: mode-3 reads need the UDS session held (`7F317F` after expiry — keep tester-present
running); a `Uds.request` flushes the CAN receive queue, so never interleave UDS polls
with CAN-frame observation of the same window.

### 6.2g The mirror law, the engaged word, and the final gate (2026-11-04, evening) [O]

**The mirror law.** The `0x22` echo mirrors the sent control word (established §6.2b);
combined with the driving dump's live word `0x8E91`/`0x8E81` (bits 7 + 0 held, **bit 4**
alternating — the 2026-10-31 engaged capture, encoded then in `sup_app_pattern3` but never
wired), this means the awaited bit-0 "grant" can never arrive unsolicited: **the stock
sends bit 0 continuously and the supervisor echoes it.** E1–E8 ran the boot-software shape
(base `0x00`, bit 2 toggling) — bit 7 was never sent, and every prior bypass attempt used
a word shape the stock never uses.

**E9 (`m4ab`): the engaged word accepted.** The cyclic base became `0x8E90`/`0x8E80`
(bit 7 held, bit 4 toggling, bit 0 still A-law-withheld). The exchange survives it and
the `0x22` echo mirrors `AA90` — the first stock-shaped echo ever observed. The sub-12
window on top (the stock-exact `0x91`/`0x81` pair) = **instant reset, ~5 ms** (heartbeat
counter 0 at the first frame after the window).

**E8 (`m4aa`): the duty stream cyclic (FE13 sub 14).** The 40-word duty job at ~10 Hz
(HOLD-gated, bench-interlocked): no reset, no grant — and the exchange survives it, unlike
the E6 windows. The supervisor accepts the duty stream as legitimate traffic. Cyclic duty
alone does not earn the grant.

**E10 (`m4ac`): register `0x90` data `0x21`.** Our job-2 word had been `0x9020` since E1;
the stock's live word is `0x9021` (the switch-enable register's bit 0). It rides
transparently (job-2/3 replies unchanged) — and the complete-stock-word window with
`0x9021` present still resets instantly.

**Final verdict.** The cyclic conversation is now stock-exact in every observable byte
except the withheld `0x8E` bit 0, and two enforcement paths are mapped: the ~5 ms hard
reject on the bit-0 word and the ~4 s trip on sustained setpoints — both protocol-level
(the ammeter stays flat). The bit-0 acceptance is gated on **internal supervisor state
the SPI conversation cannot reach**. Remaining candidates, all next-session threads: the
stock's boot-time handshake internals (the §6.2 mailbox path was ruled out, but the
supervisor's own power-up sequencing with the mechatronic is unexplored), hardware lines
unobservable on the sealed TCU, and the `0xFFF90944` state-letter machinery feeding the
supervisor's switch-enable bits.

Bench notes: E10 = `m4ac` (CRC `C7087523`) is the current image; install chain
`/tmp/install_e10.py`, sig table `/tmp/e10_sigs.json`; install sequencing rule — storm and
chain armed in the dark window, the whole install runs in one power state (§6.2f lesson).

### 6.2h The boot-window hypothesis — tested and dead (2026-11-05) [O]

**The stock's own enable law** (monitor `0x873F0`, decoded from the flash): the enable
setter `0x6C612(1)` fires when a state byte at `0xFFF93BA1` is ≤ 6 — state 0 in the
driving dump, i.e. always true in normal running; the surrounding monitor is a
timing-consistency check (free-running counter vs calibration `0x1912D4`, fault 547 on
drift). The setter runs at raised IPL, does boot-handover bookkeeping (`0x72EB6`: the
boot-managed `0xFFF80CD4` counter and `0xFFF80D19` `0xA5`-magic cells), latches bit 6 of
`0xFFFF1001` (`0x68BE0`), then sets `0xFFF90953 = 1`. The app-side gate is weak — the
stock arms its enable in the first monitor pass, and the 2026-10-31 shadow-poll saw the
echo bit 0 latched at the first readable instant. Hypothesis: acceptance is a
boot-transition window, and every bit-0 word we ever forced was minutes late.

**E11 (`m4ad`): the self-armed window — and the no-RAM finding.** A `.noinit` trip
counter (3 boots × 200 exchanges of unconditional bit 0) produced a **silent reset
loop**: no heartbeat ever, ammeter at idle, power-off to kill. Two findings: early bit 0
rejects (or cannot even be observed unbounded), and **no RAM state survives the reject
reset** — the counter never capped the loop, so the boot clears user RAM on every
handover or the reject is power-on-class. Any cross-reset bounding in RAM is dead on
this bench.

**E12 (`m4ae`): the host-armed window — bounded by construction.** No self-arm: a magic
CAN frame (id `0x7F4`, `E1 12 B0 0B`), handled in the 1 ms task, arms the 200-exchange
window; the bench host spams it from power-on until the first heartbeat or 10 s. Reset
boots never see the frame again (RCAN mailboxes latch once), so a rejecting supervisor
produces a bounded silent storm and then a clean observable boot. Result: exactly that —
~10 s of silent cycling, then a normal boot with the A-law holding and the ammeter flat
at idle throughout.

**Verdict.** The boot-window hypothesis is dead: early bit 0 (from the first exchanges,
~25–50 ms into the app) is rejected exactly like late bit 0. Bit 0 has now been refused
in every word shape (E7–E10), every timing (25 ms to minutes), and every context we can
construct. The supervisor's bit-0 acceptance is **not temporal and not reachable through
the SPI conversation**. The remaining candidates live outside our app's behavior: the
supervisor–counterpart (mechatronic) relationship at power-up, hardware lines
unobservable on the sealed TCU, or supervisor-internal state conditioned by the Bosch
boot software's own supervisor service — the next decode targets are the boot programs'
word shapes (§4.4: bit 6 = `0x40`, bits 7/4 in special boot states) and whether the boot
manager's service differs by boot path.

Bench infrastructure notes: verify the bus is DARK (4 s of listening) before arming any
install chain — the front door answers while the old app still runs, and two premature
catches cost 15 min. E12 chain `/tmp/install_e12.py`, block sig `1aa7a9593a1c0e87`,
image CRC `3ECFBD47`.

### 6.2i The timing axis closed — no age earns bit-0 acceptance (2026-11-05) [O]

**The boot-era capture.** A stock restore plus a measurement-protocol dump from power-on
(request format `[0x2A, 0x00, addr…]`; `0x0E` bytes = the deny marker) caught the stock
**fully engaged at boot +15 s**: cyclic word `0x8E91` accepted, echo `0xAAB1` = mirror +
**supervisor bit 5 — the rail-state telemetry, visible in the echo all along** (our
echoes read pure mirror = outputs off), the state letter `'c'` (`0x63`) already written at
`0xFFF90944`, the write job at full `0x8E9F` from the first capture (no level ramp),
`0xFFF90953 = 1`, `0xFFF93BA1 = 1`. The stock's commissioning is ~15 s end to end; ours
takes minutes.

**The WDT pairing eliminated** (live, no build): watchdog in interval mode first, then
the bit-0 window = instant reset — and since our own WDT cannot reset in interval mode,
**the reject is the supervisor's own hard reset line.**

**E13 (`m4af`): the host-armed window with a live delay byte.** The magic frame's
`data[4]` selects the delay; results across the day: bit-0 at **+0.2 s** = rejected
(a 3-second spam window produced a textbook reset storm — heartbeat counter frozen at 0
while boots cycled every ~0.3 s: arm → bit-0 → reject → reboot); **+3.2 s** (the
never-tested gap) = no grant; **+6.2 s and +14 s** = **tolerated — the first bit-0 words
ever not to reset** (the app ran on, 71 s+ continuous); **+23 s** and minutes = rejected.
The tolerance band is ≈ [+0.5 s … +15 s], matching the stock's own engagement period.

**The rail blip.** Early observations of `AAB0` echoes were a timestamp artifact (script
time vs app time); the blips were reads of a boot's **first half-second** — during which
**the supervisor's rail bit is up in every boot**, then drops. Bit-0 inside a boot that
follows a reject (the storm boots) is rejected all the same — the post-reject window
hypothesis is dead.

**Verdict.** The age map is complete: there is no age at which bit-0 alone earns
acceptance. With the word (§6.2g), the buses, the registers, the WDT pairing, and now
the timing all eliminated, the remaining known difference is subsystem-level: the stock's
**state-letter machinery** — `'c'` written into `0xFFF90944[]` before/during engagement,
feeding the supervisor's `0x90` switch-enable bits (`0x91758`) and the `0x6C612` enable
chain. Our firmware has never written those letters. That is the next decode target.

Bench scripts: `e13_watch*.py` (delay window + echo), `e14_postreject.py` (storm
anatomy), `e15_gap.py`, `bootera_dump.py`. Image `m4af` (CRC `E42D0199`), chain
`/tmp/install_e13.py`, sig `3d58e77515ec10ab`.

### 6.2j The no-disturb engagement — the supervisor powers up ready (2026-11-05) [O]

Two bench observations closed the pace question and opened the real one. Our firmware
powers on to the 0.167 A idle directly (no current spike) — the rail is not an
unconditional power-on default; the stock's immediate 1.8 A is conversation-driven. And
the "minutes-long ladder" note was a myth: `JOB_PERIOD = 60 × 5 ms = 300 ms` per job puts
the full ramp at ~10–12 s, comparable to the stock.

**The discovery:** the probe job's level echo reads `0x1F` *before we command anything* —
**the supervisor powers up at level `0x1F`, already engaged-ready** — and the 1→7 ramp
we have run since the first engage build drags its state cell down and back up. The
stock's own law (`0x873F0`) arms its enable in one monitor pass with no ramp.

**E14 (`m4ag`, CRC `5ED82461`):** when the probe reads `0x1F`, the ladder commands
`0x841F` — confirming what is already there — and enters HOLD on the first matching echo;
the ramp machinery stays for genuine cold starts. The engagement is now **one job cycle
(~2 s)**. Visible state unchanged (echo still a pure mirror, `0x24/0x2E` steady, ammeter
at idle).

**Bit-0 on never-disturbed boots:** minutes-age still rejected; +4.2 s (instant-HOLD
boot, delay-armed window) **tolerated — no reset, no grant**. The tolerance band extends
down to ~+4 s on clean boots. With the level undisturbed, the commissioning, the
stock-exact word, instant HOLD, and tolerated bit-0 all assembled, the grant still does
not come — the echo never mirrors bit 0 and the rail stays off.

**The remaining known difference:** the state-letter machinery — the stock writes
`'c'` (`0x63`) into `0xFFF90944[]` at engagement, feeding the supervisor's `0x90`
switch-enable bits (`0x91758`). Our firmware has never written those letters. That
subsystem is the next decode target (find the writers of `0xFFF90944`, the letter
sequence, and what conditions each letter).

Bench note (twice learned): mode-3 echo reads require the display mode armed first
(FE13 sub 9,2); observation runs that skip it lose the echo silently.

### 6.2k The state-letter machinery decoded (2026-11-05) [O]

**The letters are binary flags.** The setter at `0x8C7A8` (`set_letter(slot, value,
cond)`): `LETTERS[slot] = (cond ? 'c' (0x63) : 0)` — a byte array at `0xFFF90944[]`,
each slot either the letter `'c'` or zero, plus a parallel state array written at
`0xFFF93A8E[slot]` and per-slot state at `0xFFF94FD0/0xFFF94FCE`. The initializer at
`0x093298` clears letters[0..1] at start-up.

**The consumer** (the supervisor's register-`0x90` composer, `0x91758`): the base data =
`0xFFF90963 & 0x1D`, then three letter lookups — slot indices from flash tables at
`0x1918E8[0..2]` — each `'c'` sets one bit: **bit 5, bit 6, bit 7 of the `0x90` register
data, one per switch-output letter**. The stock's live `0x9021` = base `0x21` with letter
slot 0's `'c'` set (bit 5). *Our E10+ hardcoded `0x9021` already equals this engaged
composition* — the letter's effect on the `0x90` word is not the missing piece.

**The condition sources** (the caller state machine at `0x85B00-0x85CC0`): the letter
slot for each switch output derives from a vehicle-state classifier — code 17 (`0x11`)
and code 68 (`0x44`), code 51/102, code 34 — with per-slot cells at `0xFFF90947` and
condition bits from a calibration-indexed table (`0x1914AA`, bit tests per slot). This is
the stock's **switch-output arbitration**: which outputs the state letters certify as
allowed. The `'c'` letter = the certification that our `0x9021` bit 5 represents.

**Implication:** the letters feed MORE than the `0x90` word (which we already match).
The remaining consumer is inside the control-word composer (`0x91C8A`, reading the
`0xFFF9094A` cell block via GBR) — i.e., the letters may gate the WORD composition
itself through the state cells. That composer path is the next decode: what the word
builder does with `0xFFF90962/63` (the cells downstream of the letters).

### 6.2l The letters close — and a missing job surfaces (2026-11-05) [O]

**The letter composition completes.** The `0x90` register's base byte is a flash
constant: `0x1918EF → 0xFFF90963`, value `0x00` on this unit — the base contributes
nothing. The whole register data = the letter bits (identity-mapped: slot 0→bit 5, slot
1→bit 6, slot 2→bit 7) plus continuation bits (bit 3 from `0xFFF90952`, bit 0 from the
composer's tail). **The driving dump's letters = `63 00 00` — exactly one `'c'`, slot 0.**
The stock's live `0x9021` = slot-0's `'c'` (bit 5) + the tail's bit 0. Our hardcoded
`0x9021` matches the complete composition; the letters are not a `0x90` delta.

**The gap that surfaced instead: job `0x40BC0` is missing from our stream.** The stock's
§4.3 table includes it — a one-word job (`0x0000`) to **SSLB0** (the second system
chip), the SSLB0 status-nibble poll (completion handler `0x92CC8`), staged at
`0xFFF885E0`. Our cyclic stream sends job1/job2/job3 (=`0x40BE8`/`0x40C0C`/`0x40C54`),
job4 (`0x40C74`, SSLB2) and job5/job6 (`0x40B80`/`0x40BA0`) — **never `0x40BC0`**. The
stock's full rotation includes a poll we have never sent. The stock's dispatcher order
(the function-pointer schedule around `0x92740`) also remains undecoded — our
interleave order is ad hoc. E15 candidate: add the `0x40BC0` word to the rotation and
verify the stock's job order/cadence against the schedule table.

### 6.2m The scheduler decoded — and E15 (2026-11-05, evening) [O]

**The "schedule table" 0x92718–0x9273C resolved.** It is not a walked schedule: it is the
literal pool of the bus-0 job-descriptor builder (`0x9263C`, 72-byte control blocks at
`0xFFF94148+72·idx`), whose first three words also serve a tail-call. The dispatcher
question dissolves into three small helpers every module shares — `0x93B68(desc,flag)`
clear the status cell / bump a use counter at `0xFFF94178+2·desc[0]`, `0x93B8E(desc)`
check-free, `0x937F8(desc)` queue — plus per-job **starters** that re-stage the template
and queue: `0x92A84` (ID `0x40B80`), `0x92B82` (status `0x40BA0`), `0x92D38` (control
`0x40BE8`), `0x92E80` (enables `0x40C0C`), `0x93198` (SSLB2 `0x40C74`). The 5 ms task
`0x8EE82` = compose the control word (`0x91C86` → `0xFFF907D4`, the `0xC9` byte via
`0x93126`) → queue **status → control → ID** (`0x91DAA`, `0x91E12`) → kick the RSPI/DMA
engine (`0x90ACC`→`0x921A0`, which also queues the bus-0 ID job `0x40A98` every pass).
There are no per-job timers: **cadence = queue drain**; a busy job is simply skipped by
the next check-free. The enables-job starter lives behind the letter machinery (called
via pool `0x9187C`), its word = `0xFFF907D6 & ~(0xFFF907D1 & 0xE0) | template` (faulted
outputs masked off, §4.4) — queued when that state changes, not every pass [I].

**Job `0x40BC0` pinned.** Descriptor `0x40BC0` = 1 frame → SSLB0, select cfg `0x40B76`
(= SPCMD `0xAF01`, the odd-frame select), TX `0xFFF885E0`, RX `+2`, status cell
`0xFFF908E0`, handler `0x92CC8`. The driving dump's live staging: **TX `0x0000`, RX
`0x0000`** — the word is `0x0000`, and the real chip's reply is *not* AA-class; the
handler (`0x92CC8`) stores `(rx & 0x0F) ^ 0x0F → 0xFFF90958` and `rx & 0xFF →
0xFFF90979`, setting only an error bit on the failed header — **the stock sends this
poll every rotation and tolerates its failing reply check**. No code references the
descriptor at all ("built inline"); its queuer was not found (candidates: the `0xC9`
machinery at `0x93126`) — cadence taken as every-pass from the live staging [I].

**The setpoint question corrected.** The stock's setpoint job (`0x40B48`, header
`{reg 0x2D, 5 chips}` = the handoff's "0x2D05") is cyclic, and so is **ours** —
`sol_drv.job_cyclic()` has sent the full 45-frame job every 10 ms since M4 (the
handoff's "one-shot bench paths" note was stale). The real deltas: (1) **values** — the
driving stock stages `881A/881A/881A/8812/8815` mode rows with E8/F0 setpoints
`053,589,58A,053,000` / `589,589,053,000,000`: two `0x53`-class **keep-warm** channels
(chip 0 ch 0, chip 3 ch 0 — the keep-warm machine's live cells `0xFFF90586/8A` = `{11,
53}/{11, 47}`) plus `0x589`-class *driving* currents (≈855 mA each — the bench's 1.8 A
hum). We send zeros. (2) the keep-warm **value cycle** (`0x90586` walks
41/34/3A/27/0E/1F/3C/26/44) is not decoded [I]. The `0x90` register word is also
dynamic in the stock — the driving dump stages `0x9020` while the Nov-5 echo read saw
`0x9021` (the composer tail bit 0 varies; source not pinned) — our hardcoded `0x9021`
is a snapshot, noted open.

**E15 (`m4ah`, CRC `E36A4BDA`, gap-keep `0x50000`, one block from E14):**
1. The missing seventh job: one word `0x0000` to SSLB0 alone (new `plat_sup_burst0`,
   SPCMD `0xAF01`) appended to the full-fidelity 5 ms pass after the mixed jobs; the
   reply is class-counted (`nib_aa`/`nib_other`), never faulted — the stock tolerates
   the failing header too.
2. Keep-warm setpoints: chip 0 ch 0 and chip 3 ch 0 commanded `498` (0.1 mA units →
   setpoint `0x53` = 49.8 mA) instead of 0 — matching the stock's staged keep-warm
   class. The `0x589` driving currents are deliberately NOT replicated: if the rail
   ever closes, the bench sees ~100 mA (announce-class), not 1.8 A.

Prediction: if the missing poll or the keep-warm staging is what the supervisor's
grant conditions on, the echo gains bit 0 (grant) / bit 5 (rail) within the stock's
~15 s commissioning window. If nothing changes, both additive deltas are eliminated and
the remaining suspects are the `0x90` tail bit, the keep-warm value cycle, and the
aux-job cadences (enables/SSLB2/nibble queuers, [I] above).

**E15 bench result (2026-11-05, evening).** Install: one block (0x40000, sig
`a523e40219207134`), all 21 verified. Boot: instant HOLD intact (+5 s), tokens at the
stock cadence, ammeter flat at 0.167 A, echo a pure mirror (`0xAA90`) through app+70 s —
**no grant, no rail**. The nibble poll is mechanically perfect: RAM counters
(`0xFFF846EC..F4`) read `nib_aa = 43937 ≈ sent = 43932`, `nib_other = 0`,
`nib_rx = 0xAABB` — job 7 ran every pass and SSLB0 answered AA-class every time (on our
bench SSLB0's status reads 0xBB-data, where the driving stock's captured reply was
0x0000 — the nibble differs by state, worth remembering when comparing dumps). **Both
additive deltas are eliminated as sufficient.** Remaining known differences: the `0x90`
word's dynamic tail bit (staged `0x9020` in the dump vs `0x9021` read live), the
keep-warm value cycle, and the aux-job cadences (enables/SSLB2/nibble queuers) [I]; past
those, the difference is structural (the supervisor–mechatronic power-up relationship,
hardware lines).

### 6.2n The 0x90 tail bit settled — and the reply ledger mapped (2026-11-05, night) [O]

**The composer decode (0x91758).** The `0x90` register's data byte = `0xFFF90963 &
0x1D` (the flash constant `0x1918EF` = **0x00** on this unit — the base contributes
nothing) | letter bits (slots 0/1/2 → bits 5/6/7, table `0x1918E8`) | calib `0x1918EB
<< 5` | bit 3 = (`0xFFF90952` == 1), with `0x1918EC << 5` as an AND-mask applied only
when the helper `0x9209C` returns nonzero, and a second `0x9209C` call deciding
`0xFFF90965` (= cell & ~1, or 1). **The word's bit 0 comes only from the base cell —
0 in steady state.** The enables starter `0x92E80` stages `template(0x40C08) |
(0xFFF907D6 & ~(0xFFF907D1 & 0xE0))` — the fault mask clears letter bits of faulted
outputs. So the stock's steady `0x90` word = **`0x9020`** (driving dump: TX `0x9020`,
`0x24` reply `0xAA20`); the boot-era `0x9021` was the transient pre-settling base.
The `0x9021` we hard-coded since E10 (matching that transient) is corrected.

**E16 (`m4ai`, CRC `C77D67BE`, one block, sig `b83313bc14c9887d`):** the one-word
change `0x9021 → 0x9020`. Result: no grant (echo `AA90` pure mirror through +70 s) and
**the `0x24` read-back is still `0xAAFF`, invariant to our written byte** — SSLB1's
`0x24` answer is its own state word, not a mirror of register `0x90`; the
AA20-vs-AAFF delta is upstream of our data. **The `0x90` tail bit is eliminated as a
grant condition.**

**The full cyclic reply ledger (ours at idle vs the stock driving):** `8E`→AA00 both;
`C9`→AA00 both; `22`→AA90 ours (pure mirror) vs `AAB1` stock (mirror+rail); `09`→AA00
both; `90`→AA00 both; `24`→**AAFF ours vs AA20 stock**; `28`→**AA45 ours vs AA46
stock**; `2E`→**ABFF ours vs AA05 stock** (different reply class — bit 11 flag + data).
The three deltas smell like telemetry (the `0x2E` job stores a 10-bit value + flag →
`0xFFF907CC`, §4.3) — plausibly effects of the dead rail, not causes. Decode targets:
what the `0x24/0x28/0x2E` reads mean (handler `0x93096` for `0x28/0x2E`; the stock's
`0x92D94` class checks) — if they are rail/downstream telemetry, the cyclic stream is
CLOSED and the hunt moves entirely to the structural suspects.

### 6.2o The rail is up at our first exchange — and drops mid-ladder (2026-11-05, night) [O]

**E17 (`m4aj`, CRC `0F24DBB1`)**: pure instrumentation — a 64-pass `0x22`-echo ring
recorded from pass one (`g_sup_rx_hist`, sup.c) plus ms-stamps (`g_sched_ms`,
`g_first_sup_ms`, main.c), read via `0x2A` after one power cycle. Results:
**first supervisor pass at +3 ms** of scheduler time (no startup gap — the init chain
is not the problem), and **the rail bit (echo bit 5) is UP from pass 1 through pass
63 (+320 ms), every pass `AAB0`** — grant bit never set. By app+1 s the echo reads
`AA90`: the rail drops between +320 ms and +1 s. §6.2i's "rail up in every boot's
first half-second then drops" is confirmed at pass resolution — and extended: the
drop happens DESPITE a live, stock-exact cyclic stream, well into our app's reign.

**The timing correlates with our ladder pacing.** Our register jobs: probe at pass ~2,
first token job at pass ~62 (~+310 ms — exactly where the ring ends), level-confirm
and HOLD at ~0.7–1 s. The stock paces its register jobs with calibration dwells
(`level*200 + 2400` ticks, engine `0x6BF9C`) — milliseconds at engaged level, not our
bench-era `JOB_PERIOD` = 300 ms; its write job runs at full value `0x8E9F` from the
first capture. Hypothesis (E18): the supervisor's rail grace expires unless the
commissioning exchange completes early; our 300 ms ladder dwells run out the clock.
E18 = the fast ladder (first register job immediately, dwell from the stock's law or
tens of ms) + the ring extended to 512 passes to place the drop instant against the
ladder events. Also pending: the user's ammeter AT SWITCH-ON — with E15+'s keep-warm
commanded and the rail up for ~1 s, a ~0.27 A blip then settle to 0.167 would confirm
current genuinely flowed.

**E18 (`m4ak`, CRC `4F0754BA`) — the drop is on film.** Fast ladder (`JOB_PERIOD`
60→2 passes, 10 ms rungs) + the ring at 512 passes with a per-pass ladder-state
track. Result: rail UP from pass 1, **HOLD reached at pass 49 (+245 ms) with the
rail still up** — the ladder beat the window — and **the rail dropped at pass 64
(+320 ms), ~15 passes after HOLD**. The ladder-completes-in-time hypothesis is
dead in the cleanest way: completion changes nothing. Combined with E17 (rail up
through pass 63, drop at the same boundary), the law is: **a ~320 ms rail window
opens at our first word and expires regardless of content** — word shape, jobs,
letters, level, HOLD, keep-warm, `0x90` word all eliminated as renewal
conditions. User ammeter ground truth: **0.180 A during the window** (the rail
stage's own ~14 mA — NOT our 100 mA keep-warm, which never flowed), settling to
0.166 the moment it drops: the echo's rail bit ≠ power delivery; the output stage
never closes without the grant. The stock's window never expires — its renewal
lives outside the mirrored conversation. Remaining un-mirrored 5 ms behavior from
the scheduler decode: the stock queues the **bus-0 ID job every pass**
(`0x921A0`); ours runs it once at init. The boot manager's own supervisor service
(word rate/content, §4.4: bit 6 = 0x40) is the other open door.

**E19 (`m4al`, CRC `3F2C767D`) — the last 5 ms behavior eliminated.** The bus-0 ID
job queued every pass (`sol_id_poll`, mirroring `0x921A0`). Result: the ring is
**identical to E18** — rail up passes 1–63, HOLD at 49, drop at exactly pass 64
(+320 ms). The window is not renewed by bus-0 activity. The ammeter ground truth
changed though: **0.217 A during the window** (idle + ~50 mA ≈ exactly one
keep-warm channel) vs 0.180 in E18 — with bus-0 alive the rail delivered real load
current for the first time, then still closed at +320 ms. **Conclusion: every
app-side behavior — cyclic words, register jobs, ladder timing, bus-0 cadence,
setpoints, `0x90` word, nibble poll — is stock-mirrored and none renews the
window.** The renewal lives at the boot→app handover or inside the supervisor's
own conditions. Next: restore the stock and dump its TX staging + echo cells at
+0.1–0.5 s (the bootera dump tooling) — the stock's own words during the window
are the remaining reference; then the boot manager's supervisor service (§4.4).

**E20 (`m4an`, CRC `A0AE3359`) — THE GRANT.** The E13 host-armed window extended
with pass-granular delay (magic frame `data[5]` = 5 ms passes): bit 0 fired from
pass 50 (+250 ms, just after HOLD, ~70 ms of rail window left), 2000 cycles, held
on. Film: rail up 1–63, drop at 64 as always — and at **pass 129 (+645 ms) the
echo's bit 0 went high: `0xAA91`, the first GRANT ever, latched through the end of
the ring (+2.5 s)**. Not a mirror — a mirror would answer at pass 51; the ~400 ms
hold-back is a decision, and +645 ms sits square inside the old "tolerance band"
[+0.5…15 s], which we now understand was never tolerance: it was the ACCEPTANCE
WINDOW, gated on the ask coming from the fully-mirrored stream (E13's +6.2/+14 s
asks got no grant because the stream then carried 0x9021, no nibble poll, no bus-0
poll, slow ladder, zero setpoints — the context, not the bit, was refused).
**Both conditions were necessary: the full mirror AND the ask.** The rail bit
itself stayed 0 (the window had closed at 320 ms, before the grant arrived) and
the ammeter read idle — grant without rail. A display re-arm later unlatched the
sustained grant (the known lab quirk: sub 9,2 memsets the ladder) — the A-law
should self-sustain once granted; do not re-arm while observing it. **E21
(`m4ao`, CRC `A6593396`): the stock's enable-edge WDT switch (0x920A4) fires on
the grant latch — WTCR to interval mode, IPR 0xE — the WDTOVF pin change the
supervisor may need to raise the rail** (§4.4: "while outputs are active the
external supervisor is the watchdog that matters").
