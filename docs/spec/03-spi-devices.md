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
