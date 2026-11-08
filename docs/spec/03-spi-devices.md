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

**E21–E23 (2026-11-05, late evening) — the grant's law refined; a wrong dialect bricked a boot.**

**E21 (`m4ao`, CRC `A6593396`):** the grant edge fires the stock's enable-edge WDT
switch (0x920A4 → WTCR interval + IPR 0xE). Result: grant **reproduced deterministically**
— pass 128 (+640 ms), one pass earlier than E20's 129; ~400 ms decision latency after
the ask starts, every time. **The WDT switch did not raise the rail** (echo bit 5
stays 0; ammeter 0.190 → 0.166). **E22 (`m4ap`, CRC `85765317`):** the composite
first-pass edge (WDT switch at pass 1 + ask at pass 1) — window still closes at 64,
**and no grant at all**: asking too early forfeits it. The grant's law is now:
**full mirror + HOLD complete + ask sustained** → grant ~400 ms later; the ask must
wait for HOLD (~pass 50+). E13's old "tolerance band" [+0.5…15 s] = the acceptance
window under exactly these conditions.

**E23 (`m4aq`, CRC `86E710B3`) — FAILED, bench recovered.** Hypothesis: the stock
app's first phase is state 6 (bit 6, constant `0x8E40`), matching the boot manager's
dialect for a gap-free handover. Implementation: 100 passes of constant `0x8E40`
(`sup_phase6_arm`). Result: **the TCU dropped to 0.017–0.018 A with 5–9 A peaks, no
CAN, no heartbeat — an unrecoverable boot loop** (the supervisor holds the box in a
power-on-class reset; every boot re-enters the phase). **The error: state 6 is the
boot's MAT-FLASH-PROGRAMMING state, not a benign opening dialect** — §4.4 said it
all along ("bit 6 = 0x40 … in special boot states"). Sending 100 passes of
"programming flash" from a non-boot app is a fault condition. Recovery proved the
discipline: 30 s power-off (rail bleed), storm caught the fresh front-door window,
recovery chain (reinstall m4ap) completed clean. **Lesson recorded: never mirror a
boot state word into the app stream without decoding what the state MEANS; the
supervisor punishes role confusion with a latched fault.** Bench state after
recovery: E22 (m4ap) running healthy.

**Where the grant question stands:** grant achieved and deterministic (mirror +
HOLD + ask); rail still not re-raised (bit 5 stays 0; the window closes at +320 ms
before the grant can arrive — the ask cannot start before HOLD at pass ~49, and the
grant decision takes ~400 ms, landing at pass ~128, always past the window). The
rail's renewal is now proven NOT to be: word content, job set, letters, level, HOLD,
keep-warm, 0x90 word, nibble poll, bus-0 cadence, WDT-mode timing, or the ask
itself. The stock keeps its rail from power-on continuously — the difference must
be in what the BOOT MANAGER's service does before/during handover (its bit-2-toggle
normal words + WTCNT every 5 ms, vs our app's engaged dialect), or in
supervisor-internal state that no app wire can reproduce. The stock's own words
during the window (boot-era dump at +0.1–0.5 s) remain the decisive unread
reference.

### 6.2p The wire-content axis closed — our granted stream IS the stock's stream (2026-11-05, night) [O]

**The stock boot-era cell dump** (stock restored one-block; meas responder up at
**+5.6 s** — not +13 s; `stockboot_dump.py`), reading the supervisor cells, letters,
enable shadow, and all four bus-1 staging windows while the stock runs engaged
(ammeter 1.755 A, rail continuously up since power-on):

| Cell / staging | Stock at +5.6 s (engaged) | Ours (E20/E21 granted) | Match |
|---|---|---|---|
| cyclic job1 TX | `8E91 C900 2200 0900` | `8E91 C900 2200 0900` | **exact** |
| job1 echo (0x22 reply) | `AAB1` (grant+rail) | `AA91` (grant, rail=0) | word matches; bit 5 is the supervisor's own output |
| enables TX | `9020 2400` | `9020 2400` | **exact** |
| word-out cell 0xFFF907D4 | `0x91` | `0x91` | **exact** |
| echo cell 0xFFF907D5 | `0xB1` | `0x91`+bit5 | **exact** modulo rail |
| letters 0xFFF90944 | `63 00 00` | (hardcoded in 0x90 word) | **exact** |
| ID job staging | `0600 0300 0000 0000` | same | **exact** |
| status/nibble staging | `1800 0C00 1E00 0600 1400` | same | **exact** |

**Every byte on the wire matches.** Once granted, our stream IS the stock's
engaged stream — there is no additional word, job, or content to send. The rail
bit (echo bit 5) is the supervisor's own output state, not something we command.
**The renewal of the ~320 ms rail window is not a wire behavior.**

The stock's commissioning completes in **under 5.6 s** from power-on with the rail
up continuously. The remaining difference is structural: the stock app inherits a
supervisor conditioned by ~1–2 s of **boot-manager service** (bit-2-toggle words +
WTCNT reload every 5 ms, §4.4) before the app's words take over; our app replaces
the boot's service from a supervisor that grants exactly 320 ms of rail. The next
decode target: the boot manager's handover — whether it writes supervisor state
beyond the wire (via the 0x72EB6 bookkeeping or other MMIO), or whether the rail
window's renewal is electrical (the WDTOVF pin, supply sequencing) rather than
protocol.

**Live-stock comprehensive dump (2026-11-05 night; stock engaged at 1.755 A).**
Six phases read while the stock drove: boot-handover bookkeeping, supervisor
cells, keep-warm, state-letter machinery, full bus-0 staging, and 60 s of
2-second dynamic intervals. Key findings:

1. **The engaged steady state is stable**: over 60 s, ONLY the challenge
   selector (0xFFF907D2 area, rolling 0x1130–0x1144 / 0x381D–0x384B) and the
   10-bit telemetry (0xFFF907CC, 373–380) moved. Word-out 0x91, echo 0xB1, the
   0x90 data 0x20, letters 63 00 00, staging — all frozen. **The 0x90 tail bit
   is NOT dynamic in steady state** (0x20 constant).
2. **Keep-warm values cycle on a >60 s period**: 0x53→0x3A and 0x47→0x30
   between two dumps 20 min apart — both in the known list (41/34/3A/.../44).
   Genuine dynamic, bus-0 (not the rail's gate).
3. **The boot-handover bookkeeping cells** (Phase 1): the `0xA5`-magic at
   `0xFFF80CE4/CE6 = A5A5 A5A5`, `DEA7/FADE/DEA6` markers, and the boot-managed
   counter at `0xFFF80CD4 = 0` (consumed by the app). These are the cells the
   0x72EB6 bookkeeping writes at the enable edge — present and live.
4. **THE MAILBOX INPUT WORD DIFFERS**: `0xFFF80D00 = 0x0000` in the engaged
   stock — vs **`15` (0x000F) on every one of OUR boots**, same bench, same
   hardware. The stock's running software drives the input word to 0; ours
   reads 15 from power-on and it never changes. E4's init-mirror did not
   reproduce this. While the handshake itself was ruled out as the rail's gate
   (the stock drives with letter 0x01 = handshake never ran), the input-word
   difference is a genuine unexplained state delta between the stock's software
   and ours.
5. The state cell `0xFFF90950 = 0x0119` — state 0x19 (engaged) in the high
   byte. The enable-shadow cell `0xFFF90952 = 0x0001` (bit-3 source active).
   The composed `0x90` data cell `0xFFF90964 = 0x0020` (letter bit only —
   confirms the E16 value from the source side).

**Deep probe of the live stock (immediately after the comprehensive dump).**
Four more phases: boot-residue sweep (0xFFF80400-0xFFF81400), supervisor
neighborhood, **peripheral I/O reads via the stock's meas** (the stock ALLOWS
I/O reads where our firmware denies them — first live peripheral state ever),
and setpoint staging with live keep-warm. Key findings:

**I/O reads (the stock's meas allows them):**
| Register | Stock (engaged) | Ours | Note |
|---|---|---|---|
| 0xFFFC0D00 (mailbox input) | **0x07D0** | 0x000F | **REAL hardware state delta** |
| 0xFFFC0D04 (mailbox output) | 0x07D0 | 0x0000 | output reflects input |
| 0xFFFF1000 (chip ID) | 0x0021 | 0x0021 | same |
| 0xFFFE0000 (WTCR) | **0x0027** | varies | watchdog config differs |
| 0xFFFE0002 (WTCNT) | 0x00E5 | varies | being serviced (snapshot) |
| 0xFFFE0006 (WRCR) | 0x0080 | 0x0080 | same |
| 0xFFFE0802 (module-stop val) | 0x0000 | 0x0000 | same (0x5555 doesn't persist) |
| 0xFFFC1400 (RCCR) | 0x0001 | — | matches init-mirror write |
| 0xFFFE0400 (port ctrl A) | 0x0014 | — | differs from raw state |

**The mailbox input word delta is the sharpest hardware-state difference:**
0x07D0 (2000) vs 15 (0x000F). Neither is in the handshake's valid set {9,10,13,14},
so the handshake still wouldn't fire — but the peripheral is genuinely configured
differently. The stock's software drives the input register to 0x07D0 through a
mechanism E4's init-mirror didn't reproduce (the module-stop 0x5555 write doesn't
land in either system).

**Keep-warm cycle confirmed ongoing**: the cell at 0xFFF90586 read 0x53 → 0x3A →
0x2B across three snapshots (~minutes apart) — all from the cycle list
(41/34/3A/27/0E/1F/3C/26/44, note 0x2B=43 ≈ the 0x2C=44 entry). The setpoint
rows also show the low-nibble ticking (E853→E854).

**Boot-residue**: the sweep found the 0xA5A5 magic + DEA7/FADE markers at
0xFFF80CD6-0xFFF80CE6, and a large cluster of structured data at
0xFFF80E00-0xFFF81200 (timer/callback tables, the cyclic schedule structures of
spec 02 §7, counters) — the boot manager's and app's shared scheduling
infrastructure, live while engaged.

**THE I/O READ CAPABILITY**: the stock's meas protocol reads peripheral
registers (0xFFFC0000-0xFFFFFFFF range) that our meas denies. This is itself a
measurable behavioral difference — our `plat_mem_read` limits reads to
flash + RAM 0xFFF80000-0xFFF9FFFF; the stock's allows I/O. For future
diagnostics: any peripheral state question can be answered by restoring the
stock and reading through its meas.

**Live-stock I/O sweep + schedule tables (immediately after the deep probe).**
Full peripheral map + boot/app scheduling infrastructure from the engaged stock.
NOTE: the I/O sweep script included reply-header bytes (0x2A81) in its word
parsing — the raw data is sound but some values below are extracted after
stripping artifacts.

**Peripheral state summary (engaged stock, artifacts stripped):**

| Block | Key readings | Note |
|---|---|---|
| RSPI-B | SPCR=0x0800, SPSR=0x0020 | supervisor bus active |
| CMT timers | CMSTR=0x0001 (running), counters ~0x0B6C/0x1869 | the 5 ms tick source |
| Port ctrl A | PCR=0x0014, PDR=0xE000 (at 0xFFFE0432) | port direction differs |
| IRQ control | **IPR05=0xE400** (at 0xFFFE0C04) | **the WDT interrupt priority = 0xE in the high nibble** — the stock's watchdog interrupt IS at priority 0xE |
| INTC | full vector table populated | ~20 interrupt vectors live |
| Module-stop | 0xFFFF0802=0x00FF, 0xFFFF0804=0x00FF | both key regs read 0xFF |
| System ctrl | chip ID 0x0021, SCKCR=0x0007 | |
| **Mailbox/c counterpart** | **entire 0xFFFC0C00-0xFFFC0C82 block reads 0x07D0** | either open-bus (module stopped) or a uniform data pattern; differs from our 0x000F |
| Clock gen | RCCR=0x0001, PLL config 0x0F08/0x4412 | matches init-mirror |
| BSC | active bus state control values | |
| **WDT** | **WTCR=0x0027, WTCNT=~0x00DE (varying), WRCR=0x0080** | **WTCR 0x27 = watchdog RESET mode (bit 6/ TME = 0)** — the engaged stock does NOT run its WDT in interval mode |
| ADC | active, multiple channels | |
| CAN/RCAN | full vector set + control | |

**KEY WDT FINDING: the engaged stock's WTCR = 0x27 (TME = 0, watchdog/reset
mode).** Our firmware sets WTCR = 0xA567 (low byte 0x67, TME = 1, interval
mode) and our E21 additionally switches modes on the grant edge. The stock runs
its WDT in RESET mode even while engaged and granted — §4.4's "enabled → WTCR
out of reset mode (interval timer)" may describe a momentary edge or a different
bit field interpretation. The IPR05 = 0xE400 confirms the WDT interrupt is at
priority 0xE (high nibble of the byte), matching the 0x978A4 decode.

**Schedule tables**: the spec 02 §7 table format did not parse cleanly through
the 0x361 read path (header-byte contamination in the 20-byte entries). The
scheduling area 0xFFF80E00-0xFFF81400 shows a large cluster of structured
18-byte repeating records with constant fields (04 73 0f 00 01 01 f8 00) and
varying ID fields — likely the CAN transmit schedule or RCAN mailbox table,
not the cyclic callback table. No obvious flash-pointer callback entries
(0x0009xxxx / 0x0040xxxx) were found outside the known supervisor scheduler
area, so **no surviving boot-manager supervisor-service callback was
identified** in this pass.

**Final stock grab: schedule tables resolved, cell neighborhood complete, RSPI-B
detail.** The "cyclic schedule table" at 0xFFF908DC (spec 02 section 7) is
**individual supervisor state cells, not a callback table** — no
{enable, period, callback} structure exists there. **No surviving boot-manager
supervisor-service callback was found** — the boot's service stops cleanly at
handover. Two new discoveries in the full 0xFFF90600-0xFFF90A00 sweep:
- **ASCII identification strings at 0xFFF90980**: "bhie01", "bhjd0701",
  "bhjx2201" — version/date identifiers in the supervisor cell area
- **Buffer pointer routing table at 0xFFF909A0-0xFFF909F8**: seven entries
  pointing into 0xFFF90E84-0xFFF90EFC — the app's internal message routing

RSPI-B peripheral detail (readable through the stock's meas): 16-bit words
at B800=0x0800, B802=0x0020, B80A=0x0500, SPCMD0/1=0x0000. *** CORRECTED
2026-11-05 (spec 03 6.2t): the sweep's register LABELS were wrong — the
manual's map (17.3) has SPCR@B800, SPPCR@B802, SPSR@B803, SPDR@B804 (8-bit
regs), SPBR@B80A. The reads decode as SPPCR=0x00 + SPSR=0x20 (status), and
SPBR=0x05 + zero — the bit rate MATCHES ours (SPBRB=5). There is no SSLP or
SPDCR in this map at those offsets; the "SSLP/SPDCR deltas" were artifacts.
Never-measured as a result: SPCKD@B80C / SSLND@B80D / SPND@B80E (the RSPI-B
select/clock TIMING registers — ours 0/0/1, minimum select negation). Their
stock values sit in the flash config table at 0x40D60+ (raw dump banked in
bench status 20261105) — decode offline before E30.
**SPDCR and SSLP differ from our settings** (ours: 0x0000, 0x0000) — these
affect the SPI data latching and select polarity, not the wire content.

The fast echo poll (toggle cadence measurement) did not complete — the script
stalled. This is the one data point not captured from the live stock.

**Gap sweep of the never-read RAM (0xFFF91000-0xFFF93C00) + ammeter drift.**
117 nonzero regions found in the previously-unexamined area. Key discoveries:
- **A VIN at 0xFFF93BDA**: ASCII "1C6RR7FT9JS284765" — the vehicle
  identification number, stored in RAM from the mechatronic communication
- **ASCII version strings**: "c01026" at 0xFFF91B88 (build/date?)
- **Calibration/configuration structures**: pointers into 0xFFF90Axx, float
  values (~3.6, ~3.99, ~3.98 — voltage or current measurements), structured
  config blocks with repeated patterns
- **Lookup table at 0xFFF917F8**: `01 05 09 0b 13 00` (repeated twice)
The sweep covered ~0xFFF91000-0xFFF93C00; the remaining gap to 0xFFF9E000 and
the DMA/SPDR reads were not completed.

**Ammeter drift observation (user)**: the stock's driving current climbs slowly
over extended runtime — 1.755 A at power-on → **1.932 A after ~15 minutes**
engaged. The stock actively manages its output currents (thermal compensation
or pressure-control strategy). This confirms the stock is performing live
closed-loop control on the mechatronic.

**DMA + SPDR resolution (final two unknowns).** The RSPI-B DMA request
registers (`0xFFFE6074/76`) read **zero** in the engaged stock — **the stock
does NOT use DMA for its supervisor-bus exchanges**. The "DMA-driven
architecture" assumption is corrected: the stock's normal app drives RSPI-B from
the CPU, same as our firmware. **The DMA-timing hypothesis (different exchange
timing causing the rail window difference) is eliminated.** SPDR
(`0xFFFFB804`) reads `0x0000` through the meas across 100 fast polls — the
register is write-only/clear-on-read through the memory path; the toggle
cadence is known from the disassembly (5 ms) and our wire matches. The DMAC
channel registers (0xFFFEC400-0xFFFEC700) also read zero through the meas
(likely write-only); the DMAC OR/status at 0xFFFE4400 shows active interrupt
vectors matching the INTC data.

### 6.2q E24/E25 — the grant law rewritten: ask anytime; the grant is a lease that ends in an MCU reset; every MCU restart re-opens the rail window (2026-11-05, late) [O]

**E24 (m4ar, CRC 2F056AF9): WDT switches removed — refused.** The E21/E22
runtime WDT-mode switches were deleted (the stock's engaged WTCR = 0x27, live
I/O read 6.2p — the switches moved the wrong way). `wdt_init` still writes
0xA567/0x67. The boot executed E20's wire exactly: ladder RAMP→HOLD at pass 49
(state hist), rail up 1-63, drop at 64, ask fired and completed
(`s_boot_bit0_delay=0`, `s_boot_bit0_left=0` — all 2000 bit-0 passes sent,
addresses 0xFFF84698/0xFFF8469A per tcu_app.map). No grant through any of it.
Diff against the E20 commit (495129c): only the post-grant WDT switch and
inert E23 leftovers — the wire was byte-identical to E20's granted run.
**Conclusion: boot-to-boot variability near the rail window** (grants at
identical timing: E20 ✓, E21 ✓, E24 ✗).

**E25 (same image, power cycle + identical ask): granted — twice, and the
picture changed completely.**

- **Run 1**: heartbeat +7.0 s (the boot manager reigns ~7 s after power-on).
  Ladder SYNC@1 → RAMP → HOLD@49. The echo went B0 (rail+toggle) passes 2-47,
  then **fully silent (0x0000) at pass 48** — rail window closed early AND the
  mirror stopped; the ladder demoted itself to IDLE at pass 76 (no code path
  found that calls `sup_engage_arm(false)`; run 2 did not reproduce it — open).
  The spam's last magic frame opened the window at ~pass 150: **one bit-0
  word, grant on the next pass** (`left` froze at 1999 — the engage enable
  branch took over bit 0 the pass after the first ask).
- **The lease**: the grant lived ~160 s (run 1) / ~122 s (in-place, run 2) with
  the rail down, then **the MCU was reset** — warm handover, fast (≪1 s; no
  boot-manager reign), and `g_sched_ms`/`sent` of the new instance date the
  restart to the second. Every ungranted boot we have ever run sat for hours
  with no reset; resets have only ever followed a granted period.
  Operational law: *grant + rail-down ≈ 2 min → the supervisor gives up on the
  app and resets it.* (Lease length not constant: 160 vs 122 s — not a fixed
  timer, not a pass counter; possibly its own debounce period.)
- **Every MCU restart re-opens the rail window**: the post-reset instance's
  ring shows echo 0x80 → **0xB0 at pass 2** (rail UP) → 0x90 from pass 65
  (325 ms — the classic window). The supervisor raises the rail at each app
  restart un-prompted and closes it ~320 ms later. The stock's rail never
  drops; whatever holds it lives inside that window.
- **In-place ask (E12's mailbox-latch claim does not hold on warm resets)**:
  ONE magic frame sent to a 7.5-minute-old HOLD instance armed the window;
  the ask fired, **grant landed within one pass, at pass ~94,000**. Age does
  not gate acceptance. The E13-era "asks at minutes never granted" was a
  stream-shape problem, not age. Refined law: *full mirror + ladder HOLD +
  bit-0 ask = grant in one pass, at any age* — asks launched during the rail
  window itself are unreliable (E24 ✗, E20 slow at +78 passes).
- Grant-state telemetry: the grant changes ONLY the 0x22 echo bit 0
  (AA91/AA81 pair while granted, live 8E95/8E91 tx). 0x24=AAFF, 0x28=AA45,
  0x2E=ABFF are unchanged from the ungranted state. `echo_ok`/`echo_bad`
  alternate 1:1 (toggle-bit check artifact, expected).
- **Nobody has yet held the grant while the rail is up** (E20's grant came at
  pass 129, post-drop; E25's grants were minutes in). The rail window opens at
  every restart — an ask timed inside it (the stock asks from its first words)
  is the next cell. The magic frame's `data[5]` makes ask-delay a bench
  parameter: E26 = delay 2 (ask at pass 3, one mirror exchange first, inside
  the window), no reflash needed.

**Instrument note**: E25's run-1 early rail close at pass 48 with echo silence
(vs 65 + mirror-continues in run 2 and 64 in E24) — the close point and the
echo's survival vary boot to boot. Open items: the IDLE@76 demotion, the echo
silence, the lease-length variance.

### 6.2r E26/E27 — the ask law closed; the rail bit decoded; the scheduler collision (2026-11-05, night) [O]

**E26 (delay sweep, no reflash — the magic frame's data[5] IS the ask delay).**
- **delay=2 (ask from pass 3, inside the rail window): REFUSED.** Rail up 2-64,
  drop 65 (325 ms, standard), ladder HOLD@50, ask ran pass 3→2002 — the
  supervisor ignored 2000 consecutive bit-0 words.
- **The unification — the supervisor has a READY moment.** After each MCU
  (re)start the supervisor ignores bit-0 asks for a variable period (~pass
  100-200+, boot-dependent); the first bit-0 word sent after ready = grant in
  one pass. Evidence: E20/E21 granted at 128/129 (ready came early under their
  running ask); E22/E24/E26-d2's asks expired before ready; a late RE-ask on
  the E26-d2 instance (first ask pass 3, 2000 ignored asks) granted instantly
  — **no poison: early asks are ignored, not penalized.** Bench recipe (5/5):
  wait ≥1 s after boot, then ask. Ask-during-the-rail-window never grants and
  the grant×rail overlap theory is dead — the supervisor will not grant while
  its window is still open.
- **Grant lifetime is finite and variable: 160 s, 122 s, 324-417 s (three
  measurements), ending in a warm MCU reset** (fast handover; the rail window
  re-opens each time). One 5 s-interval poller ran through the 324 s+ lease —
  a heavy read burst (~90 reads/s for 1.5 s) coincided with grant-1's death —
  cadence sensitivity suspected but unproven.

**The rail bit decoded (spec correction).** The stock's cached 0x22 echo byte
(0xFFF907D5 = 0xA1, captured while driving 1.773 A) = bit 0 GRANT **and bit 5
RAIL, both set**. Our granted echo (0x91/0x81 pattern): grant set, **bit 5
clear — the supervisor grants acceptance and never raises the rail.** Nobody
has observed bit 5 set on our stream outside the power-on window. The rail is
the sole remaining prize; the grant is solved.

**E27 (the duty-stream test — and the scheduler collision).** Fresh boot, ask
delay=150: grant at ~1 s (recipe 5/5). UDS session 03 (0x7E1; the session
request is load-bearing — raw routine sends without it get no reply) then
RoutineControl 0xFE13 in[0]=14 in[1]=1: **accepted (71 01 FE13), and the app
RESTARTED at that exact second** (g_sched_ms dates the new instance to the
duty-cyclic acceptance; sol cycles confirm; no fault latch — faults 0x00 on
the fresh instance). Mechanism: the 40-word duty job lands in the same 10 ms
tick as the 45-word cyclic job (and the 5 ms supervisor stream) — the tick
overruns, the watchdog resets the MCU. The grant "loss" was the restart. The
stock runs all of these concurrently through its ring-buffer scheduler (spec
02 §7 enqueue/consume split) — **our inline scheduler is now a bench-proven
blocker for any sustained duty stream.**

**E28 (next): interleave the duty job** — spread the 40 words across passes
(the stock's model) or slot them into the stream's idle gaps, keeping the
5 ms supervisor cadence and the WDT serviced. Then the duty-stream rail
hypothesis (the supervisor may want to SEE the duty traffic before raising
the rail) finally becomes testable, and the grant lease can be watched under
a properly loaded stream.

Ammeter note (E27 power-on): .211 blip → .166 — higher than the usual .19
settle; consistent with the restart's rail window (E25: every restart re-opens
~320 ms of rail) stacked near the boot window.

### 6.2s E28/E29 — the supervisor's reset law, the stall threshold, and the pure stream (2026-11-05, late night) [O]

**E28 (m4as, WTCR = 0xA527 interval from init): the reset was never ours.**
The hardware manual (R01UH0480EJ0400 s14.3.1) corrected a core decode: bit 6
WT/IT — 1 = watchdog (overflow = internal reset via WRCR + 64-cycle WDTOVF
pin pulse), 0 = interval timer (masked ITI only, no reset, no pin). Our
wdt_init had run 0xA567 = WATCHDOG since E1; the engaged stock reads 0x27 =
INTERVAL. E28 ships interval mode (the stock's engaged word) — our self-reset
path is gone entirely. CKS=111 overflow budget 104-210 ms; plat_wdt_service
gives ~15-30 ms headroom per call; plat_drv_xfer's guard loops never service.

**E28 bench: the duty-cyclic under grant STILL reset the MCU at the exact
duty-fire second (g_sched_ms dates the instance to it) — with our watchdog
disarmed. THE SUPERVISOR RESET THE MCU.** It owns the reset line; a stalled
5 ms stream while granted = immediate reset. Ungranted + duty = NO reset
(n=2, E27-era and E28). The law: **after the grant the supervisor enforces
the stream; before it, hiccups are tolerated.** This retroactively explains
the grant-lease "expiries" (variable 122-417 s lifetimes = accumulated
stream hiccups under grant) and the heavy-read-burst death of grant-1.

**E29 (m4at, the bus-0 stream gate): purity is exonerated for the rail.**
First boot ever with a pure supervisor stream (sol_drv gated OFF at init;
FE13 sub 15 toggles live; tests open the gate explicitly). Grant recipe 7/7
(delay 150). Results:
- **Rail bit NEVER set on the granted pure stream** (30 s watch, tokens
  flowing, echo AA91 throughout).
- **Rail window closes at pass 65** — identical to every noisy boot. The
  window is not shortened by our bus-0 traffic, and not extended by purity.
- **The cyclic under a live grant is TOLERATED** (gate opened mid-grant:
  instance continuous, grant survived) — the stall threshold sits between
  ~11 ms (cyclic alone) and ~20 ms+ (cyclic+duty stacked). The duty job
  remains the only fatal stall (n=2 under grant).
- Live first: granted enable stream + cyclic driver jobs simultaneously —
  the stock's engaged wire shape, running.

**Rail suspects remaining (both hardware-config):** SPDCR (stock 0x0500 vs
our 0x0000) and SSLP (stock 0x0020 vs our 0x0000) — the RSPI-B electrical
config; SSLP bit 5 is plausibly the SUPERVISOR'S OWN SELECT-LINE POLARITY
(SSL1P). E30 = match them (manual-checked bit decode first). Also open: the
boot handover gap timing (continuity theory's dead gap).

### 6.2u The stock switch-on ammetry — the rail never opens (2026-11-05, night close) [O]

**The continuity theory's missing observation, delivered by the user's eyes at
switch-on (instrument-verified minutes later): the STOCK goes 1.7 A → dip
1.5 A → 1.812 A.** The rail never opens — there is no idle phase, no .166
state, no visible window. Full solenoid-class current flows from essentially
the first instant, with one dip (plausibly the boot→app handover perturbing
one channel) and immediate recovery to the engaged level. Automated ACS712
minutes later: 1.865 A stable (vs the user's 1.812 bench ammeter — ~3%
instrument agreement, both trustworthy).

**Consequence for the hunt:** our firmware and the stock are in different
REGIMES, not different states of the same machine. Our rail window (up at
every MCU start, ~320 ms, down) has no stock counterpart at power-on — the
stock's rail-hold mechanism acts within the first moments (the boot
manager's own supervisor words + the app's immediate engagement) and never
lapses. The re-raise/hold mechanism lives in the earliest boot sequence.

Instrument note: the switch-on capture script's raw ammeter reader died
silently (tcu_bench Buses opened ACM1 as bus2 and collided with the raw
serial open — the ammeter must be used ALONE on ACM1 per the E6-era
pattern); CAN census captured the stock schedule boot (037/2F9/144/170/1A0/
2A8/178/639) but no current profile. The user's observation carries the
result. Bench left on STOCK, engaged ~1.86 A — power off to rest; restores
on resume: /tmp/install_e29.py (E29), or any prior build from /tmp/eNN sigs.

### 6.2v The 1 kHz switch-on profile — the stock drives at 250 ms; our boot manager takes 3-7 s (2026-11-05, final) [O]

**The stock's switch-on current, captured at ~1 kHz (ACS712 raw A-line
stream, bulk-read pattern — readline() hangs on this port; see tooling
note):** logic phase ~190 mA from t+0 to t+0.20 s; the output rail LEAPS at
t+0.20 s (1225 mA bin) to FULL ENGAGED CURRENT 1910 mA by t+0.25 s; two
control-transition dips (~1620 mA at 0.6-0.95 s, ~1550 mA at 1.15-1.75 s —
the user's eyes saw exactly these as "dip to 1.5"); rock-steady 1875-1890 mA
from t+2.1 s onward. No idle phase, no window, no drop — the rail delivers
solenoid current at 250 ms and never stops.

**The decisive comparison:** the stock's app is ALIVE AND DRIVING by ~250 ms.
Our app's first exchange lands at +2.9 to +7 s (heartbeat timing across
tonight's boots — VARIABLE), i.e. our BOOT MANAGER REIGNS 10-30x longer
before handing to the app. Same boot code, same stock ident, same-size app
region. By the time our engaged dialect (byte-identical to the stock's
engaged stream) finally starts, the supervisor has held the rail through
seconds of boot-manager words — and drops it 64 of OUR passes later (the
~320 ms "window"). The stock hands over gap-free at 250 ms and the rail
never knows a transition.

**E30 (redefined): find why the boot manager is slow for our image.**
Candidates: the app-region verification path (gap-keep body vs the stock's
uniform body), a boot-side wait our image answers late or never, or
state the boot reads from the app block. If our app can start at ~250 ms
and open the engaged dialect gap-free behind the boot's words, the rail may
never drop. (The E23 lesson stands: never blind-copy the boot's own 0x8E40
state-6 word from the app — decode its meaning first.)

**Tooling (banked the hard way):** the ACS712 stream needs BULK
`ser.read(4000)` parsing (the Ammeter class's pattern) — `readline()` hangs
indefinitely on this 1.5 Mbaud port; the ammeter must run ALONE on ACM1
(any second opener - Buses(bus2) or another process - silently kills the
reader); the raw stream is ~1.9 kHz combined A/P (A ≈ 900 Hz) — the
"steady" 1.88 A is fast PWM modulation (A-lines swing 1.5-2.5 A; P-peaks
~8.9 A on RSPI exchange spikes).

### 6.2w E30/E31/E31b — the boot-delay artifact, the bench-layer assassin, and the VS convergence (2026-11-06, morning) [O]

**E30: the "3-7 s boot delay" never existed.** Anchored captures (ammeter
power-on anchor + passive CAN, listen-only): the STOCK's first CAN frame at
+0.088 s, OURS at +0.042 s — both apps live within 100 ms of power-on. The
3-7 s heartbeat timings of 2026-11-05 were measured from script launch, and
the delta was the operator's hand reaching the switch. E30's premise
dissolved; the boot-manager architecture decoded along the way (RAM init +
copy tables at 0x22444; main = 8-state flash-session server + ms timer
engine at 0x270bc vs the 0xFFFFF200 tick) stands as context.
**Anchored boot profiles (50 ms bins, ammeter):** stock ~190 mA to +0.20 s
then the solenoid leap (1.9 A at +0.25 s, dips = engagement choreography,
steady 1.88 A); ours ~199-209 mA to ~+0.37 s then decay to the .16x idle.
Same window clock, opposite rail outcome.

**E31/E31b: the load-latch test — twice sabotaged, then the convergence.**
E31 (m4au): drivers commissioned at init + 7×250 mA from the first cyclic —
NO delivery; post-boot read: mode[]=OFF, current[]=0. Cause:
`bench_channels_off()` in the idle bench tick has been forcing channels 0-6
OFF every 10 ms in every build — **keep-warm never flowed either** (the
2026-11-05 ".217 blip = keep-warm delivery" attribution is CORRECTED: it
was rail-up logic current). E31b (m4av): idle wipe compiled out under
AUTOENGAGE — setpoints now stage and flow (mode[]=CURRENT, current[]=250×7,
the engaged 0x9F row active — cfg_mismatch exploded from 36 to 38004 = the
engaged row's read-backs), drivers return live status bytes (12 BD) —
**and STILL zero current.**

**The convergence (spec 12 §2c's October blocker, now explained):** the
driver chips have never had VS (their switched supply). VS is asserted by
SUPERVISOR registers 0x84/0x88/0x90 bits 5-7 — written by the stock app at
start-up through RSPI-B (the 0x91DD2 area, staging 0xFFF907D4/D6, AAxx/A8xx
replies — the supervisor's bus, which is why E5's bus-0 five-select write
was ignored: right registers, wrong bus). Our E2-era FE13 probes read
0x84/0x88/0x90 = AA00 (never initialized) vs 0x96 = AAFF — the evidence was
banked in November. Strong unification hypothesis: the ~370 ms "rail window"
IS the VS switch's power-on default grace; the stock's register init latches
it on inside the window (solenoids at +250 ms); our missing init = VS off at
+370 ms — and no dialect/ask/grant experiment could ever have mattered.
**E32 = decode the exact init words (0x91DD2 + the driving dump) and send
them through our RSPI-B cyclic from app start.**

### 6.2x E32 — the build-death class strikes; the morning's ledger (2026-11-06, morning) [O]

**E32 (m4aw, D6E69D61): bit 0 from word one + E31b's driver load — never
ran.** Two boots, both dead: the app never transmitted (zero CAN, meas dead,
front door dead, ammeter .165 = logic only) — the m4w BOOT-DEATH CLASS
(spec 06 §7 4b era): a build-level crash pre-CAN, reproducible, power-cycle
immune. The E32 tree shifted ~31 kB of code (literal-pool realignment); the
bit-0 semantics are UNIMPEACHED (E22's pass-1 bit 0 survived) — the cell
"the stock's exact wire from word one" remains UNTESTED. Restored E31b
(heartbeat verified). NEXT BUILD RULE: after any bench build, byte-diff the
.text against the last proven build and eyeball the entry/init path before
installing — the m4w archaeology (byte-exact toolchain, only source deltas
matter) applies: something in the E32 source delta (the `#if` block's pool
shift?) broke init. Rebuild E32 as a MINIMAL delta (arm the raw shape via a
one-byte static, not a code block).

**Morning ledger (all banked):** (1) the "3-7 s boot delay" was a
measurement artifact — anchored captures: stock first-CAN +88 ms, ours
+42 ms; (2) `bench_channels_off()` idle wipe found — keep-warm NEVER flowed
in any prior build (E19's delivery attribution corrected); (3) E31b: with
the wipe fixed, setpoints stage and flow, the engaged 0x9F row active,
drivers answer status — and still ZERO current: **the VS-supply refusal is
confirmed load-independent** (commanded 1.75 A in-window made no difference)
— consistent with the Nov-2 decomposition (refusal downstream of the rail);
(4) the full supervisor enable-path was re-walked on-spec: 0x40C0C output-
enable job, letters→0x90-bits matched, 0x24 = the free FF→20 instrument
(still FF in every state incl. granted). The rail latch's remaining unknowns
narrow to: the supervisor's first-400 ms state machine (what it expects
BEFORE its grace expires) — the stock's first passes carry the enable bit
from word one; E32 was that test.

### 6.2y E32r — the reset class reproduced, pinned to word one (2026-11-06, morning close) [O]

**E32r (m4ax, B2431638): the one-immediate build — bits8e 0x90→0x91, a
ONE-BYTE .text delta vs E31b (plus 4 CRC bytes; verified before install).
The minimal-delta discipline worked: the build booted, the rail window ran
its normal course (~350 ms, ammeter 169→195→155 mA) — and the app CAME UP
DEAD: zero CAN, meas dead, ammeter settling ~155 mA = the average of a
RESET LOOP.** October's finding is reproduced as a clean boot loop: the
word shape `0x8E91/0x8E81` (bit 7 held + bit 4 toggle + BIT 0 ON) sent from
WORD ONE, before any ladder/token/context, is the supervisor's reset class
("bit 7 held (word shape 0x91) reboots the TCU, uptime-verified" — bench
2026-10-31). The stock sends the identical pair from its first exchange and
is NOT reset — its legitimacy is contextual (the 0xFFF90951-status /
mode-chain legitimization Oct-31 named as the next lead, or the handover
state the boot manager leaves behind).

**Eliminations this morning, final form:** (1) boot timing — identical,
both <100 ms; (2) load in window — no effect (1.75 A commanded, zero
delivered: VS off, refusal load-independent); (3) the engaged word from
word one — RESET LOOP (context-gated, not shape-gated). The rail latch's
remaining space: the supervisor's context/legitimization state — what the
stock's first exchanges establish BEFORE the enable becomes legal. Next
decode targets: the mode-0x31 chain / 0xFFF90951 writer (Oct-31's lead),
the boot-manager-to-app handover state, and the first-400 ms choreography
difference. Restored E31b; bench healthy.

### 6.2z The E32r constant — six boot-deaths, one root cause; the startup handshake is safe (2026-11-06, afternoon) [O]

**ROOT CAUSE OF ALL BOOT-DEATHS (E32 through E35n): a single unreverted constant.**
E32r changed `sup_app_pattern3(true, 0x90u, ...)` to `0x91u` — sending
`0x8E91/0x8E81` (bit 0 from word one) = the supervisor's context-gated
RESET class (Oct-31's finding). The constant was never reverted. Every
subsequent build (E32, E33, E34, E35, E35m, E35n) inherited it, and every
"boot-death" was the same supervisor reset loop — NOT a pool-shift, NOT a
code-size issue, NOT a build artifact. The "pool shift" theory wasted hours
of binary comparison that all came back clean because the code WAS correct;
the supervisor was rejecting the word content. **Lesson: `git diff` the
working tree against the last proven build BEFORE hypothesizing about
binary corruption.**

**E35r (m4bd, A980A925): the startup handshake — finally on a clean baseline.**
With 0x90 reverted, E35r sends the stock's startup jobs (`8800×4 + 1200 +
0C00 + 0A00`, then `841F + 0A00 + 1200 + 0C00`) through the existing
alternating transport, after the first cyclic exchange, before the
scheduler starts. **THE APP BOOTS.** First CAN at +0.062s, 688 frames,
ladder reaches HOLD (state=3, 0x1F confirmed), rail window normal (drop at
pass 65), 0x24 still reads FF (output refused).

**The startup handshake is SAFE but INSUFFICIENT.** The supervisor accepts
the words without consequence — no reset, no state change, no rail latch.
The stock's startup sequence alone doesn't unlock the output stage. The
engagement requires something the startup words don't carry.

**What we know the supervisor's engagement DOESN'T depend on:**
- Wire content (byte-identical cyclic, spec 6.2p)
- Timing (both apps live <100 ms, spec 6.2w)
- Load (1.75 A commanded in-window, zero delivered, spec 6.2w)
- Register values (0x88/0x90 accepted, don't latch, spec 6.2v)
- Startup handshake (safe, no effect, this section)
- Grant alone (earned 7/7, no rail)
- Driver chip commissioning (identical framing, SPCMD audit clean)
**What remains:** the supervisor's internal state machine has a state we
haven't reached. The mode-0x31 chain (0xFFF90951 writer, Oct-31's lead)
and the ring-buffer scheduler architecture (spec 02 §7) are the last
decode targets.

### 6.2aa E36 — the CAN schedule: active, transmitted, and eliminated (2026-11-06, afternoon) [O]

**E36 (m4be, 6573E57B): the stock's full CAN transmit schedule enabled.**
All 8 vehicle IDs (037/2F9 at ~730 frames, 144/170/1A0/2A8 at ~375, 178 at
76, 3D1 at 16) transmitting at stock rates alongside the heartbeat. App
alive at +0.053s, HOLD reached, rail window normal (close at ~350ms),
peak 431 mA. **0x24 still reads FF, grant not earned, rail not latched.**
The CAN schedule is NOT the output gate.

**Complete elimination table for the supervisor's engagement:**
| Hypothesis | Eliminated by |
|---|---|
| Wire content (byte-identical cyclic) | 6.2p |
| Transport framing (SPCMD audit) | 6.2z |
| Boot timing (both <100ms) | 6.2w |
| Load in window (1.75A, zero delivered) | 6.2w |
| Startup handshake (safe, no effect) | 6.2z |
| Register writes (accepted, don't latch) | 6.2v |
| Grant alone (earned 7/7, no rail) | 6.2q |
| Engagement heartbeat (already in stream) | 6.2z |
| CAN schedule (stock IDs at stock rates) | this section |

**Remaining hypotheses (architectural):**
1. Bus-0 88-mode byte match — stock writes {1A,1A,1A,12,15}, ours differ on chips 3-4
2. Ring-buffer scheduler — the stock NEVER blocks the 5ms cadence; we do
3. Mode-0x31 chain — the stock's internal state manager (0xFFF90951 writer)
4. An unobserved hardware signal (GPIO, voltage, current sense)

### 6.2ab E38/E39 — the grant-breaker is the CAN schedule; the grant recipe is the latch age (2026-11-07, morning) [O]

**E38 (m4bg, 39B1D5CE): E37 minus the startup handshake.** The handshake
theory died: with the handshake removed and CAN schedule + mode bytes +
keep-warm + bus-0 all retained, the proven ask (magic frame, spam→hb5,
delay=150) was refused. The install itself validated a new flow: the
bootloader HELD its plant session ~3 minutes (storm catch → chain attach
with no re-cycle; sig-first install green, app block 165 units).

**E39 (m4bh, 755A96D4): E38 minus the CAN schedule enable loop.** The
grant returned IMMEDIATELY — and was cancelled at `left=1999`, i.e.
accepted on the FIRST bit-0 word (the E25 law, verbatim). Combined with
the parting observation (E31b granted 7/7 with bus-0 traffic from app
start, no CAN), the matrix closes:

| Config | bus-0 | CAN | mode/kw | Grant |
|---|---|---|---|---|
| E31b | ON | off | no | ✅ 7/7 |
| E38 | ON | ON | yes | ❌ |
| E38 + live bus-0 quiet (FE13 sub-15=0) | off | ON | inert | ❌ |
| E39 | ON | off | yes | ✅ word-one |

**The CAN transmit schedule prevents the grant from being earned.** Bus-0
traffic, the {1A,1A,1A,12,15} mode bytes and keep-warm cycling are all
exonerated. Mechanism undecoded (RCAN TX activity vs the supervisor's
monitor is the suspicion — the supervisor never sees CAN frames, so the
interference is at the MCU: mailbox/IRQ load or a shared clock domain).
Until decoded: **the engagement build must not enable fca_tx messages.**

**The grant recipe (the latch age), pinned by a runner bug turned probe:**
the magic-frame spam must continue to the FIFTH heartbeat (latch ≈ pass
85+). hb1-break latches (pass ≤20) were refused for the FULL 2000-pass
ask, at every ask start tried (72, 150) — while spam→hb5 granted 3/3 on
E39 and matches every grant in project history (E25's 7/7 all used it).
The ask's landing pass is NOT the discriminator (137 ✓ vs 140 ✗ observed);
the latch moment is. Every magic frame re-arms (resets) the delay, so the
spam pattern IS part of the ritual — treat spam→hb5, delay=150 as the
recipe and do not "optimize" it until the acceptance law is decoded.

**E39window (delay=52, hb1-break): bit 0 inside the rail window is dead.**
The ring (rx1[2] per pass) shows the rail UP passes 2–64 (echo low byte
0xB0, bit 5) and dropped at 65 (~320 ms into our stream = the ~370 ms
power-on window) on every boot, granted or not. An early ask was refused
outright (and poisoned the whole 10 s ask). Since acceptance requires a
late latch and the window closes at pass 65, **the ask can never be both
accepted and inside the window — the rail is not earned by ask timing.**
The stock keeps its rail without bit 0 (engages at ~+15 s), so the
window-era hold signal is something else in the stock's stream.

**The cyclic reply rows on a granted vs refused stream (E39, both read;
CORRECTED 2026-11-07 afternoon — the first read mis-mapped the struct:
rx_words starts at s_st+22, not +18, and last_rx IS rx_words[2]):**
| field (correct) | refused | granted | stock driving (6.2b/6.2d) |
|---|---|---|---|
| rx_words[0] ctrl reply | AA00 | AA00 | AA00 |
| rx_words[2] **0x22 mirror (= last_rx = ring)** | AA90 | **AA91** | AAA1 (grant+RAIL) |
| rx_words[4] 0x90-register reply | AA00 | AA00 | AA00 |
| rx_words[5] **0x24 switch echo** | AAFF | **AAFF** | **AA20** |
| rx_words[7] 0x2E | AAFF | AAFF | AA05 |

1. Our cyclic is stock-exact on every row we control — granted or
   refused. NO row regression vs the E17 ledger (the apparent "0x22 went
   AA90→AA00" was the offset mis-read; s_st holds busy/enable/app_pattern
   at +16..+21 before rx_words).
2. The grant bit appears in rx_words[2] (the 0x22 mirror) — the same
   slot the ring and sup_engage_echo_bit0 consume. The ring IS
   grant-reliable: g3's first mirror-bit0 at pass 230 = latch(~80) +
   delay(150) exactly; the mirror carries rail in bit 5 (stock AAB1).
3. The remaining true deltas are the supervisor's own state words —
   0x24 AAFF-vs-AA20 (E16 already proved it invariant to our written
   data: its own switch-state word), 0x2E, 0x28 — consistent with E16's
   "effects of the dead rail, not causes." The wire side is CLOSED; the
   gap lives in supervisor state upstream of the cyclic stream.

### 6.2ac E40/E40b — the engaged write job, twice wrong-bus; the grant withdraws under burst load (2026-11-07, afternoon) [O]

**E40 (m4bi, EC903279) / E40b (m4bj, 34D2FF06): a repeating engaged write job,
armable live.** Magic frame data[6] != 0 arms a 100 ms re-send of the 40-word
configuration job with the 0x8E row = data[6] (0x9E = the stock constant minus
bit 0; 0x9F = stock-exact), grant-gated in task_10ms (the arm lands at hb5,
bit 0 must flow first). Three lessons, in order:

1. **E40's stages were no-ops** — sol_engage_resend() reset its divider on every
   10 ms gate call, starving the 100 ms counter (frozen div=1, zero mismatch
   step). E40b fixes the reset to state-transitions only.
2. **Both stage values are SAFE on a granted stream when not transmitting**
   (E40: grant held 45 s through both arms, 0x24 = AAFF throughout).
3. **E40b transmitted — and the grant WITHDREW within 3 s** (mirror AA91 →
   AA90, no reset, heartbeat alive). Root cause chain: the re-send rides
   sol_drv / plat_drv_xfer = **RSPI-A**, a bus the supervisor and af01 never
   see — so the engaged values went nowhere useful, while the extra 40-frame
   burst (on top of the 45-frame cyclic, every 100 ms) stressed the 5 ms
   cadence; one late mirror pass flips echo_grant off, the A-law shadow stops,
   and the supervisor withdraws the grant. First observation of a WITHDRAWAL
   (vs reset) class.

**Topology corollary (settles a standing ambiguity): af01 and the supervisor
are RSPI-B devices; sol_drv's commissioning (the identical 8-register shape)
rides RSPI-A to the mechatronic driver chips and cannot reach them.** The
correct feeder is the retired bus-B driver-job layer (sup_drv_poll, §6.2e
"wrong-bus emitters retired"): DRV_WRITE_WORDS = 40 (8 regs x five selects,
**the supervisor's select INCLUDED** — its 0x84/0x88/0x90 read empty when we
skip select 4, bench 2026-11-04), DRV_SP_WORDS = 36 (setpoints, supervisor
excluded — the four-select write transport plat_sup_xfer4). E41 = revive the
write job on RSPI-B, post-grant, v8e live-settable, at a cadence that does
not stress the 5 ms pass (the m4r era ran it ~every 2 s without stream
damage).

Also today: two reader-side struct-address errors caught and corrected (rx_words
at s_st+22 not +18; fca_tx's s_st at 0xFFF84DD0 vs sol_drv's at 0xFFF850D8) —
always take live-struct addresses from the build's map file, not from memory
of a previous build.

### 6.2ad E40–E42d — the bus-B feeders, the withdrawal class, and the five-select wall (2026-11-07, afternoon) [O]

**The arc.** E40 (magic data[6], RSPI-A re-send): starved by a per-call divider
reset (no-op stages), then E40b transmitted at 100 ms and the grant died —
first sight of the "withdrawal". E41/E41b moved the write job to bus B
(sup_drv_poll revived, four-select, ~2 s cadence, grant-gated): TRANSMITTED
cleanly (drv_writes at the 2 s cadence), grant held, af01 AA-acknowledged
every frame — and 0x24 stayed AAFF. The commissioning write values are
exonerated: af01 hears them and does not care. E42 (45-word cyclic-companion
on the five-select) and E42c/E42d (dump-exact 10-word 0x2D, five-select)
both ended in **supervisor-initiated MCU resets** — caught at 250 ms
resolution by E42d's sample train: arm lands (armed=1, ask re-armed) →
divider elapses → job fires → next sample reads BSS wiped, ask 0/0, and
**mirror AAB0 with the rail bit UP = the fresh power-on window**.

**The five-select wall (law):** register-class words on the supervisor's
select — outside the engage/token context — reset the MCU immediately.
plat_sup_xfer4's design comment ("the supervisor stays in the read
rotation") is absolute; the stock's 0x2D broadcast (88-mode rows to all
five selects) is context-legitimized like its 0x8E9F. The 10-word shape
proves it is the SELECT, not the burst length: 40-word four-select jobs
are stream-safe, 10-word five-select jobs reset. Any further af01 feeding
must ride the four-select.

**The boot-manager handover wipes .noinit** (E42c's boot-trip counter read
1 both before and after a confirmed reset — the boot manager's RAM init
covers it). A noinit survivor must live where the boot manager does not
clear, or the counter is only a power-cycle counter.

**Two distinct grant-death classes, both mapped:**
- RESET: supervisor-initiated MCU reset (register-words-in-its-select;
  the E28 stall class). Signature: BSS wipe + mirror returns to the
  window shape (AAB0, rail bit up) + hb gap. Ammeter: a ~370 ms .2x
  blip as the window reopens (bench-confirmed pending user report).
- WITHDRAWAL (12:21:43 event): no reset, no BSS wipe, hb continuous —
  the mirror simply drops bit 0 (~55 s into that grant), and the resumed
  ask is REFUSED for its full 2000 passes (poisoned, like the early-ask
  refusal). Cause unknown; the arm frames of that run apparently never
  landed (e42_armed stayed 0 — no code path clears it), making it
  spontaneous from our side. ONE observation; do not build on it until
  reproduced.

**Bench state: E42c image (m4bn, 03773801). The 2 s four-select write job
is proven stream-safe and ack'd — it remains available for combined tests.**
Next candidates: the four-select setpoint shape (af01's 0x12 + A000 minus
the supervisor — low expectation, af01 already ignored the same bytes in
the write job), the withdrawal/ask-refusal law (reproduce, then decode),
mode-0x31 chain, ring scheduler.

### 6.2ae E43/E44 — the window-load race, the delivery cap, and the primed-rail reading (2026-11-07, evening) [O]

**E43 (m4bo, 4A63ACA2): channels 0-6 at 250 mA each for the first 500 ms**
(re-testing 6.2w's load-in-window elimination, which ran with the
bench_channels_off assassin active). Anchored ammeter: the window era shows
a sustained +10-75 mA elevation over the .165 idle, ending exactly at the
rail close — **current flowed in the window for the first time** — but at
~50-75 mA REGARDLESS of the 1750 mA commanded. The keep-warm-era boots
(78 mA commanded) showed the same ~+45 mA. The cap does not scale with
command.

**E44 (m4bp, 34E75D21): the drivers' 0x8E config = 0x9F** (the stock's
engaged constant, no level ramp) from init and at every engaged re-send —
same ~50 mA cap, same profile. The 0x9F output-enable theory is exonerated;
the 0x80|level law and the 0x00 initial value are both retired (tests
updated).

**Synthesis — the primed-rail reading.** The stock's anchored profile
(6.2w): ~190 mA through its window era (SAME baseline as ours), then the
leap to 1.9 A at +0.25 s, steady 1.88 A. The fork at +250 ms cannot be a
current limit we fail to satisfy (we cannot exceed ~75 mA at any command —
if the closure needed current first, the stock's own pre-leap era could not
open it either). The economical reading: **the mirror's bit-5 window state
(echo 0xB0, passes 2-64) means PRIMED, not CONDUCTING — our rail never
closes at any command level; the +40 mA is board logic.** The stock's rail
closes at ~+250 ms because its app EARNS the closure then; the leap flows
after, not as the cause.

**Tomorrow's headline decode target: the stock's +200-250 ms leap gate.**
Something in the stock app times or conditions its first large setpoint
write at +200-250 ms after power-on. The composer/sender paths are known
(0x924B0, cells 0xFFF90586/8A); the GATE that holds them until +250 ms is
the undecoded piece — it is the condition we have never replicated, and
every surviving hypothesis (mode-0x31 chain, ring scheduler, an elapsed-time
or task-sequence condition) lives behind it. Static decode, no bench cost.

### 6.2af The leap-gate decode and the E45 companion (2026-11-07, evening) [O]

**The static session settled three unknowns:**
1. **The +250 ms leap** = the park arbiter's power-on fail-safe (spec 14 §4.2,
   `0xA57DA` mode 4: stationary + no valid lever request -> park target 8 =
   ENGAGE, reason 0x17). Our park module is decisions-only; we never engage.
2. **The mode-0x31 chain** (Oct-31's lead): `0xFFF90951` flips 0x19 -> 0x31
   when it reads 0x19, clears on bit 6 + counter>2, and caps 0x31 at 120 s
   (0xFFF907D8, 120000 ms ticks) — long-term engaged maintenance, not the gate.
3. **The bus topology error**: the stock's 45-frame driver cyclic (template
   0x40D0C = 8800/A000/E800/A600/F000/6000/C000/6600/D800 rows, per-chip ORs
   from records 0xFFF91108+0x1C*chip) rides **bus B** on the five-select
   rotation `0x40CE4` = af31/af21/**af11(SUPERVISOR at frame 2)**/af01/af51 —
   the supervisor WITNESSES THE SETPOINTS every 10 ms from app start. Our
   driver cyclic rode RSPI-A: the supervisor never saw a setpoint.

**E45 (m4bq, 28D907B1): the cyclic companion on bus B** (frame 2 = benign
0x10 mode + zero setpoints, real setpoints to the other four frames, 10 ms
cadence, grant-independent, magic data[7]=2). Result: the arm LANDS and the
companion TRANSMITS (drv_jobs=50 in 0.5 s) — and the TCU RESETS within ~1 s.
On the E45 boot the spam-phase arm produced a reset loop (five heartbeats =
up to five cycles; the surviving boot reads cc_on=0, ask 0/0, clean).

**REFINED LAW (supersedes "five-select wall" as stated):** register-class
words on the supervisor's select reset the TCU **while bit 0 is in play**
(asked or granted). Every companion reset (E42, E42d, E45) had bit 0 live or
counting; the reset timing matches the ask's delay elapsing. The stock runs
the identical cyclic while engaged — its frame 2 carries the SUPERVISOR'S
OWN chip-record values (0xFFF91108+0x1C*2: mode +0x07, setpoints +0x0C/+0x14,
maintained by the app), not zeros — the app mirrors a model OF the supervisor
back to it. **NEXT DECODE: who writes record 2's fields, and what values it
holds across phases — the mirrored-model hypothesis.**

### 6.2ag E46–E48 — the companion wall: content, cadence and mode eliminated; the ring remains (2026-11-07, afternoon) [O]

**E46 (0x881A + real setpoints at frame 2, 10 ms)**: reset loop during the
spam phase (cc_on=0, ask 0/0, counters wiped). **E47 (same shape, 2 s)**:
the companion fired ONCE with the grant live (mirror AA91, drv jobs=1) and
the TCU reset within a second — 2 s cadence exonerated (E41b's four-select
40-framer is stream-safe at the same rate). **E48 (E47 + the 0x9021 switch-
word bit, E16's "transient" re-read as the driver-comms mode)**: identical
reset. Content (0x10 vs 0x1A, zero vs real setpoints), cadence (10 ms vs
2 s), and the mode bit are ALL eliminated.

**The surviving differentiator is the transport.** The stock's whole RSPI-B
conversation runs as ONE continuous hardware-sequenced ring (spec 02 §7):
the SPCMD/job tables are walked by the RSPI engine with no CPU blocking and
no bus-idle between jobs — supervisor frames, register frames and reads
arrive as one unbroken choreography. Our stream is blocking job-by-job with
full peripheral re-init between exchanges; the companion's register frames
reach the supervisor's select as a standalone burst whose bus-level framing
(select assert/release pattern, inter-frame gaps, clock restarts) differs
from the stock's ring in ways no word value can replicate.

**NEXT (major): the ring-transport build** — restructure the SPI layer to a
hardware-sequenced job ring (the stock's architecture), starting with the
cyclic jobs 1-3 + the companion as one linked sequence. This supersedes the
per-job xfer model for bus B. (Also pending from 6.2af: the mirrored-model
read — capture the stock's 0xFFF88518 cyclic buffer + record 2 live while
engaged, for the frame-2 values; both tasks pair naturally.)

### 6.2ah E49 and the audit that closes the day (2026-11-07, afternoon) [O]

**E49 (m4bu, D91807E2): E35r's startup commissioning restored** (0x8800x4 +
commit + reads before sched_init, the stock's first SSLB1 jobs per transport
0x40CA0) on top of E48's full stack. Result: identical — grant live, one
companion job fired, BSS wipe. SIX hypotheses for the companion wall now
eliminated: content (mode 0x10/0x1A, setpoints 0/real), cadence (10 ms/2 s),
the 0x9021 driver-mode bit, the software ring (spec 02 §7 — a work queue,
not SPI), the startup commissioning, and burst-in-pass placement.

**The audit that settles the premise**: the SPCMD table 0x40CE4's only code
reference (0x92672) sits in the RSPI descriptor init — five per-chip SPCMD
copies into a RAM job structure (base 0xFFF90630, SPCMD shadow at +0x7E,
mode bytes, buffer pointer at +0x10, stride-0x50 records) with a literal
pool binding to the cyclic sender module (0x924B0's submit path). The stock's
45-frame cyclic DOES ride the five-select with the supervisor at frame 2 —
the companion premise was correct, and the register words really do reach
the supervisor's select every 10 ms without harm.

**The surviving difference — wire-level timing.** The descriptor structure
is a DMA table: the stock's frames clock out back-to-back (zero inter-frame
gaps, DMA-fed SPDR); ours are CPU-polled with per-frame SPTEF waits
(microsecond gaps between all 45 frames). Six eliminated hypotheses leave
the gap pattern as the discriminator: the supervisor may time inter-frame
gaps and reject gapped register sequences as protocol violations.

**NEXT (the platform build): the DMA/linked-job SPI transport** — rewrite
the bus-B exchange path to DMA-driven descriptor chains (the 0xFFF90630
structure class), eliminating inter-frame gaps. This is the architectural
close of the E40-E49 arc. Pairs with: the mirrored-model read (stock's
0xFFF88518 + record 2 live values).

### 6.2ai E50–E53 — the bit-0-free law, the mirrored capture, the topology correction, and the writes-don't-stick signature (2026-11-08) [O]

**The mirrored-model capture (the stock restored and read while ENGAGED at
1.786 A, park target 8):** the live 45-frame cyclic TX buffer (0xFFF88518)
= modes {881A, 881A, 881A, 8812, 8815}, ch0 setpoints {E853, ED89, ED89,
E853, E800}, ch1 {F589, F589, F053, F000, F000}, dithers zero, read rows
6000/C000/6600/D800; the chip records mirror it; the mode cell reads 0x19
EVEN WHILE ENGAGED (the 0x31 flip is not the engaged state); the echo
cells read 0x81/0xA1 (grant+rail live). Frame 2 of the rotation carries
0x881A + 855 mA + 50 mA — captured verbatim, banked at
/tmp/stock_cells_dump.bin.

**E50/E50b: THE BIT-0-FREE LAW.** The verbatim words on our bus-B companion
reset with the ask in play — but with the ask delayed 30 s, the companion's
register jobs ran 15-for-15 SURVIVED bit0-free, and the job that landed
with the grant live reset the TCU. Register-class words are fatal ONLY
while bit 0 is in play. E51 (row-per-pass from app start) and E52 (the
full 45-frame contiguous burst every 10 ms) then streamed HUNDREDS and
THOUSANDS of jobs bit0-free with zero resets — the burst shape and cadence
are safe; the E45-era "stall" attribution is dead; and the mirror's rail
bit appears only in the normal window. The grant was never the rail's
prerequisite (the stock's rail is up at +250 ms, 15 s before any bit 0).

**E53: THE TOPOLOGY CORRECTION.** Our bus-B probe found NO A2-class chips,
yet the stock's ID job reports four from its five-select — the 0x40CE4
table values are PERIPHERAL-RELATIVE and the stock's driver cyclic rides
RSPI-A (where our own sol_drv polls A2 chips today). The supervisor never
receives register-class driver jobs at all; our bus-B companion words are
ignored bit0-free and fault its parser when bit 0 collides (the reset
mechanism, finally explained). Our RSPI-A cyclic now sends the stock's
captured words VERBATIM (E53, m4by): the drivers receive them — headers
pass, no faults, cycles at the 10 ms cadence — **but the writes do not
stick: all four chips' read-backs mismatch EVERY cycle (400/s), the
ammeter stays .166.**

**The live lead: the writes-don't-stick signature.** Reads work, ID works,
headers work, writes never latch — same words, same SPCMD values, one
contiguous burst. The remaining deltas are the RSPI-A TRANSPORT PARAMETERS:
SPBRA (the bit rate — ours 5, verified only for bus B; driver chips often
latch writes only below a max clock), SSLNDA/SPNDA (the AE next-command
delays), the SPCRA mode bits, and the DMA-driven framing. The stock's RSPI-A
init writes these through computed addresses (literal 0xFFFFB000 at ROM
0x40E74, referenced from 0x963EC-class code) — the next decode session's
first target. One of these parameters is why every write since October has
been silently dropped — and with it, very plausibly, the rail itself.

### 6.2ak The live-stock transport read — SPCR=0x08 on BOTH buses; the engaged echo shape (2026-11-08, evening) [O]

Five one-shot reads off the engaged stock (1.789 A) before the E55 install:

1. **RSPI register blocks (both buses, live):** SPCRA = SPCRB = **0x08** — bit 6
   CLEAR on both; our transports have run 0x48 (bit 6 set) on both since
   October. The +A byte = 5 on both (our SPBR offset/value match). The
   stock's SPCMD registers read ZERO while running — its select configs
   live in the RAM shadow (0xFFF90630+0x7E), reloaded per transaction,
   the same discipline as our per-exchange re-init.
2. **The engaged echo shape (never before read live at these cells):**
   word shadow 0xFFF907D4 = **0x91**; echo 0xFFF907D5 = **0xB1** —
   grant (bit 0) AND rail (bit 5) both live — the engaged mirror we have
   never reproduced (ours: 0x90 no-grant / 0x91 grant-only).
3. **af01's cross-check shadow 0xFFF907D7 = 0x20** — the engaged
   switch-state echo, live-confirmed (ours reads AAFF = refusal).
4. **Mode cell 0xFFF90951 still 0x19** minutes into engagement, counter
   0xFFF907D8 = 0 — the 0x31 flip is NOT time-based; its trigger stays
   unknown (and non-essential: the stock drives at 0x19).
5. **The cyclic setpoints WALK:** second buffer read vs first —
   E853→E854, ED89→ED8A/ED8B, F053→F054 — a ±1-LSB closed-loop
   regulation walk (pressure control), not static values. The E53
   verbatim replay freezes one snapshot; the walk's magnitude is tiny
   and benign for replay.

**E55 (m4c0, B3DEF2F7): SPCRA 0x48 → 0x08 on the driver transport** —
matching both stock buses. If bit 6 is the clock-output mode, the driver
chips have been latching our frames on the wrong edge since October:
reads decode, writes never do — the writes-don't-stick signature, one byte.

### 6.2al E54/E55 verdicts — the transport wall is the ACCESS MODEL (2026-11-08, evening) [O]

**E54 (the bit-rate sweep, live-settable SPBRA):** mismatch held at exactly
4/cycle across SPBRA 2..0xFF — the bit rate is ELIMINATED as the
writes-don't-stick cause.

**E55 (SPCRA 0x48 -> 0x08, the stock's live value on BOTH buses):** zeroes
EVERYTHING — mirror 0000, 0x24 = AA00, no boot spike, bus B (config
untouched) also dead. Bit 6 of SPCR is load-bearing for OUR POLLED
transport: the 2026-10-30 bench lesson (the peripheral accepts writes but
never shifts without the full-setup discipline) extends — the polled
framing needs bit 6 set. The stock runs 0x08 because its transaction
engine is interrupt/DMA-driven (the 0xFFF90630 descriptor structure, the
ring scheduler of spec 02 §7).

**The convergence:** every surviving hypothesis now names the SAME build —
the stock's DMA/interrupt transaction engine for RSPI (the "ring
transport"). The stock's access model (0x08 + descriptors + DMA) is why
its writes latch; our polled model (0x48 + per-frame CPU framing) is why
ours never have. That is the next major build: replace the polled
per-exchange loop with the descriptor-driven engine on bus A first (the
driver cyclic — the ammeter path), bus B second.

Bench state: E55 image was NOT reinstalled after the verdict boot; the
revert (E55b = 0x48 restored, 19/19) is built (m4c1) — install it next
session start to return the bench to the E54-equivalent baseline before
the DMA work.

### 6.2am–an The stock's transaction engine decoded — and its ISR pairing proven (2026-11-08, night) [O]

**The decode chain (static, one evening):** the stock's submit path (0x92758
region) writes SPCMDs directly at the RSPI base; transactions LAUNCH via
software IRQ 0x253 into dispatch 0x66F82; the vector table's RSPI family =
vectors 424-476 (0x954xx-0x95Axx handlers), with **vector 449's ISR
(0x965E4) arming SPCR = 0x08 ON ENTRY** and calling the frame workers
(0x93A08/0x93A74); and the frame feed itself (0x93C30) is **a memcpy:
per frame, one unconditional 32-bit packed {SPCR, SPSR} store at the base
plus the 16-bit SPDR write — no SPTEF polling anywhere**. The "DMA engine"
of the convergence is a three-instruction loop in interrupt context.

**E55 (SPCR=0x08, our polled loop): dead. E56 (the exact stock feed,
unconditional stores): dead the same way** — mirror 0000, jobs=0, reset
loop. The verdict: **the feed loop and the vector-449 ISR are a PAIR.**
In 0x08 mode the peripheral asserts an interrupt; our vector table's
default_handler (hang) meets it with the watchdog resetting the TCU in a
loop. The stock works because its ISR services the interrupt (resbank,
arms SPCR=0x08, runs the workers) on every transaction.

**The complete build, now fully specified:** vector-table entry for the
RSPI interrupt + a minimal ISR (mask/ack, run the unconditional feed from
the buffer, flag completion) + the E56 feed function — together replacing
the polled exchange on the driver bus. Every piece is decoded; none of it
is speculative. E56b (m4c3) restores the polled 0x48 baseline; the bench
rests healthy.

### 6.2ao E57 — the minimal-ack verdict: the ISR is a full protocol (2026-11-08, night) [O]

**E57 (m4c4, 86FB0F16): vector 445 armed** (rspi_a_isr: SPSRA = 0 ack,
verified in-image at slot 0x40764) **+ the unconditional feed.** Result:
dead both buses again — mirror 0000, jobs=0, +109 s to first app answer.
The minimal SPSR acknowledge is INSUFFICIENT. The stock's vector-445 ISR
(0x96590) on every entry also: saves/sets SPSR via the packed store,
reads AND writes the **clock-gate block at 0xFFFE6000+0x30** (the
clock-gate acknowledge — without it the shared peripheral clock domain
drops, killing BOTH RSPI buses), checks the gate status bits, and
dispatches the frame workers (0x93A08/0x93A74) with register-bank
(resbank) context.

**The remaining work is now precisely bounded:** the FULL ISR protocol —
the clock-gate acknowledge sequence at 0xFFFE6030, the resbank context,
and the worker dispatch — decoded and transplanted as our rspi_a_isr.
Every address is known (0x96590's entry sequence disassembled; the
workers at 0x93A08/0x93A74 decoded in 6.2am); the work is the careful
transplant, not more discovery. E57b (m4c5, 69A843F8) restores the
polled-0x48 healthy baseline; install it at next-session start.

### 6.2ap The deep-decode findings — the DMA revelation (2026-11-08, night) [O]

**The complete call chain, traced end to end (zero bench cost):**

The frame processor (0x90DA2) per bus: (1) 0x97444 config from ROM,
(2) **0x93C42 the buffer pump** — fills 8-byte-per-frame RAM slots
{32-bit SPCR/pad, 16-bit SPDR data}, NOT the SPI registers —,
(3) 0x93E6C executor → 0x93FE0 hardware setup → **0x980E0 the DMA
configurator**.

**0x980E0 decoded: the actual hardware driver is DMA.** It writes SPCMD
registers with computed offsets, configures per-SPCMD DMA enable bits in
tables at 0xFFF94588/0xFFF9457C/0xFFF94570/0xFFF94564, writes the SPBNDT
bound register, and sets **SPCMD bit 13 (0x2000) = the DMA-enable bit**.
The frames are clocked by the DMA controller from the RAM buffer,
gaplessly, with zero CPU per-frame involvement.

**The SPCR state machine (0x93DBE):** the stock cycles SPCR through
0x09 (MSTR+SPE, active), 0x08 (MSTR only, armed), 0x06 (SPMS+TXMD,
transmit), 0x05 (SPE+SPMS), 0x00 (idle) — a per-frame state machine,
not a fixed value. The workers (0x93A74) set 0x09 on transaction start.

**Our E56/E58 approach is architecturally different:** CPU-loop direct
SPCR+SPDR writes vs the stock's buffer-then-DMA. The two fixed bugs
(byte order: 0x20000008 → 0x09000020; SPCR: 0x08 → 0x09) were severe
enough to fully explain the dead bus. E58b (m4c7, 4FB56F0F) tests the
corrected CPU-loop — one cycle answers whether the peripheral accepts
CPU-fed frames in 0x09 mode. If not, the DMA path is the confirmed
next build with every address already decoded.

### 6.2aq E58b: the CPU-loop verdict — DMA is mandatory (2026-11-08, final) [O]

**E58b (m4c7, 4FB56F0F): the corrected CPU-loop feed** (byte-order fixed
to 0x09000020, SPCR=0x09/SPE+MSTR, full ISR with clock-gate enable).
Result: dead both buses — mirror 0000, jobs=0, no boot spike. Same as
every 0x08-mode attempt.

**The definitive verdict: the peripheral REQUIRES DMA-driven frames.** No
combination of CPU-written SPCR+SPDR values works in 0x08/0x09 mode. The
stock's architecture (buffer in RAM → DMA descriptors → SPCMD bit 13
enable → hardware clocks frames gaplessly) is the only mechanism this
peripheral accepts for the register-write class. Our polled 0x48 mode
works for reads (the interrupt never fires, the peripheral runs in a
polled subset) but cannot latch writes.

**THE COMPLETE PICTURE:** the DMA build requires:
1. Frame buffer in RAM (the pump's output — 8-byte slots, already
   understood)
2. DMA descriptor setup per-SPCMD (0x980E0's tables at 0xFFF94564-88)
3. SPCMD with bit 13 (0x2000) = DMA enable
4. SPCR=0x09 + the ISR pair (v445, clock-gate acknowledge)
5. The completion callback (the executor 0x93E6C's post-processing)

Every address decoded. E58c (m4c8, 177DE8C4) restores the polled
baseline.

### 6.2ar E59/E60 — the interrupt hardware was never on: SR.I, IPR27, and the SPCR layout correction (2026-11-08, night) [O]

**E59 (m4c9) boot: .220 A settle → .177 A** — 11 mA above every previous
idle (.166 A). Inference: the held 0xFFFE6030=0xF3 clock gate itself draws
the difference; no rail movement. Live 0x361 probes:

- `count=0, idx=1` stable across reads — a burst staged, timed out, cleaned
  up. **The ISR never fired.**
- `rx_buf` all zeros (only the ISR writes it).
- `tx_buf[10..44]` = the PHASE2 engaged table exactly (the leap ran and
  staged); `[0..4] = 0x0600×5` = sol_id_poll's put_cmd(0, 0x0600) overwrite
  of the shared buffer after the cyclic's timeout. Everything staged,
  nothing clocked.

**Three independent reasons, all found statically:**

1. **SR.I = 15 since boot.** start.S masks all interrupts (`ldc 0xF0, sr`)
   and nothing ever lowers it — no maskable interrupt can fire, ever.
2. **IPR27 = 0.** Manual table 8.4: RSPI_A's three sources (SPEIA 444,
   SPRIA 445, SPTIA 446) share IPR27 bits 11-8 (IPR27 @ 0xFFFE0C30); reset
   value 0 = masked at the controller. We never programmed it.
3. **SPCR=0x09 has SPE clear.** Manual 17.3.1: **bit7 SPRIE, bit6 SPE,
   bit5 SPTIE, bit4 SPEIE, bit3 MSTR, bit2 MODFEN; bits 1-0 reserved.**
   The E58b-era decode "0x09 = SPE+MSTR" had SPE wrong — 0x09 is MSTR plus
   a reserved bit. **The E58b "CPU-loop definitively dead / DMA required"
   verdict is VOID: those builds never enabled the peripheral at all.**
   Our proven polled 0x48 = SPE(bit6)+MSTR(bit3) — SPE was the load-bearing
   bit all along. (The stock's decoded SPCR cycle 0x09/0x08/0x06/0x05/0x00
   re-reads as: 0x09/0x08 = MSTR, 0x06/0x05 = SPTIE|MODFEN — with SPE=0,
   SPTEF=1, setting SPTIE immediately raises SPTIA per the manual's own
   note: a software interrupt kick. Parked for a future engine; not needed
   by E60.)

**Live INTC read (the boot's leftovers):** IPR05=0x120E (WDT ITI at 0xE,
CMT nibbles), IPR16-19=0x7777/0x7000 (**ATU-III at priority 7**), IPR20=
0x0020, IPR26=0x3000 — all still armed when our app starts. Had E59 gotten
past the SR mask, the first ATU tick would have vectored to
default_handler (an infinite loop) and hung the app. Post-timeout RSPI-A
regs: SPCRA=0x00, SPSRA=0x20 (SPTEF set, SPE off — consistent).

**E60 (m4ca): the corrected interrupt-driven build.**
1. `plat_drv_init`: zero IPR03..IPR29 (hygiene), then IPR27 bits 11-8=0xE.
2. `plat_drv_xfer_irq`: stage under SR mask; SPCR=**0xC8** (SPRIE+SPE+MSTR);
   start frame 0 in the proven polled order (manual 17.4.9 CPU start
   trigger: SPTEF read → SPDR write → SPSR read → SPSR=0); open SR.I only
   for the burst window; restore; SPCR=0x08 (disarm, the stock's live idle).
3. `rspi_a_isr`: reply collected at **idx-1** (E59 stored the reply of
   frame k at rx[k+1] and never collected the final reply — an off-by-one
   that would have scrambled every read-back even if the ISR had fired);
   feed reproduces the polled order verbatim; spurious-entry guard; burst
   end writes SPCR=0x08 and count=0.
4. The bus-B polled transport and the 0x48 driver-read path are unchanged.

Vector-table arithmetic re-verified against the manual: table offset =
4 × vector number (SPRIA 445 → 0x6F4; our array index 441 at header+0x764,
VBR = header+0x70) — no off-by-one.

### 6.2as E60b-e — the diagnostic ladder to the bank-overflow killer (2026-11-08, late) [O]

**E60b (staged arm):** the SR window stays closed for the first second. Boot:
heartbeats flowing (app init innocent), arm at heartbeat seq 10 (bit 4 stamped),
DEADBABE in the same 100 ms, then silence. Delivery works; the landing is wrong.

**E60c (announce net):** vectors 444/446/447/448 announce themselves; the ISR
announces entry. Boot: 11 ISR entries with idx feeding across the id_poll
(count=10) and cyclic (count=0x2d) bursts — **THE INTERRUPT-DRIVEN FEED WORKS**
(the E59/E60 architecture is proven on silicon) — then DEADBABE mid-cyclic while
the in-flight burst kept completing in interrupt land (the 7 TX mailboxes
saturated; arrival order ≠ send order).

**E60d (exception-frame reader):** default_handler reads its hardware frame
(@r15 = PC, @(r15,4) = SR; manual 7.5.3). The verdict frame:
`dead 000416FE 0001` — the interrupted PC is `dt r1` INSIDE the open-window
guard loop (where the mainline always is), i.e. not a faulting instruction but
an interrupt-context capture; and the burst never completed afterward (the
RSPI was masked) — an interrupt at ≥0xE delivered to a default slot, with SR.I
raised to the RSPI's own priority. None of 444/446/447/8 announced.

**The answer — REGISTER BANK OVERFLOW (manual 7.4 + 8.3.6).** The boot enables
banked interrupt entry (IBNR BE[1:0]); banked ISRs must end with RESBANK to
release the bank. GCC's stack-based `interrupt_handler` (plain rte) never does
— every interrupt LEAKS a bank. Fifteen banks: id_poll's 10 interrupts plus
the cyclic's first frames ≈ the 16th interrupt overflows, the bank-overflow
EXCEPTION vectors to a default slot, and (per 7.4.2) the exception sets SR.I
to the overflow-causing interrupt's priority — freezing the RSPI at 0xE while
the mainline hangs. The interrupt count matches the death point exactly.

**E60e: `INTC_IBNR = 0`** (BE=00: register banks disabled for all interrupts,
BOVE=0: overflow exception off) in the INTC hygiene block — pure stack-based
handling, GCC's model. The staged arm, announce net and exception-frame reader
stay in place: the next boot should show paired burst start/end announces
flowing indefinitely and heartbeats past the arm moment forever.

### 6.2au The grant bisect — THE BOOT COMPANION IS THE GRANT-BREAKER (2026-11-08, night) [O]

**The reference experiments (all with the proven recipe: magic spam→hb5, delay
150, watched via the heartbeat enable bit — the universal probe):**

| build | bus-0 wire | boot companion | verdict |
|---|---|---|---|
| E39 (m4bh) | dead (timeouts) | none | **GRANT** — echo AA91, grant=1, echo_grant=1, tokens 877→2888 over 30 s |
| E49 (m4bu) | dead | none | **GRANT** — enable bit 0x0a |
| E53 (m4by) | LIVE (polled) | **on (since E51)** | REFUSED |
| E60f/g | live / gated-quiet | on | REFUSED (both — bus-0 state irrelevant!) |
| **E60h (m4ch)** | gated-quiet | **OFF** | **GRANT** — 0x12 → 0x1a |

**THE LAW: E51's boot companion (`sup_drv_cyclic_boot(true)` — the stock-shaped
driver cyclic sent to the supervisor on bus B continuously from app start,
bit0-free) prevents the grant.** It landed in E51, "survived" (no reset — the
E50b bit0-free law), and **the grant was never re-tested after it arrived** —
the breaker hid through E53-E60g while the transport work took the blame.
E60g's quiet-bus-A failure is now explained: the companion rides bus B; bus-0
silence was never the variable.

**The withdrawal returns (the open problem):** with the grant standing
(E60h, enable 0x1a), opening the bus-0 stream live (FE13 sub0F) → the enable
bit falls back within seconds (0x1a → 0x12) and the ammeter holds .166 A.
The E40b withdrawal class: the driver cyclic's bursts block the mainline
(the ISR feed completes but each xfer waits in the guard loop) and delay the
supervisor's 5 ms mirror passes; one late pass flips echo_grant off. The
ammeter has never moved on ANY granted stream (E39/E49/E60h): the rail bit
was 0 throughout — grant ≠ conduction; the drivers must be driven WHILE the
grant holds, which the current 10 ms cyclic cadence breaks.

### 6.2av CORRECTION to 6.2au: there was no withdrawal — it was the E42 RESET WALL (2026-11-08, night) [O]

The "withdrawal" recorded at the end of 6.2au (stream-open under a standing
grant → enable falls within seconds) was misread. The heartbeat **uptime
counter** (0x7F0 data[2..3]) read 876 s at 20:24:34 — the app (re)started at
**20:09:58, the exact second of the FE13 gate-open**. The enable bit "falling"
0x1a → 0x12 was the REBOOTED app's first heartbeats.

**Reconstruction:** the grant landed at 20:08:48 (companion off). The FE13
stream-open at 20:09:57 fired the lazy commissioning immediately — `job_id` +
`job_config(CFG_8E)`: 40 words, register-class, five-select WITH the
supervisor's select in the rotation, **bit0 in play** → the E42 five-select
wall → supervisor-initiated MCU reset, instantly. The fresh app then ran the
same job_config once more **ungated** (a seam: job_config bypasses the
stream gate; it fires at ladder level changes regardless) — and it SURVIVED,
because bit0 was no longer in play. Both directions of the law observed
inside one minute:

**THE UNIFIED LAW (supersedes the separate E40b-withdrawal reading):**
register-class words reaching the supervisor's bus-A select —
- bit0-free → no reset, but the supervisor REFUSES all grants thereafter
  (E60g's frozen AA90; the E50b survival law's flip side), and
- with bit0 in play → immediate MCU reset (E42d's wall; tonight 20:09:58).

Other notes: boot_trips read 1 after the reset — CONSISTENT, because the
boot manager wipes .noinit on every handover (the counter cannot count soft
resets; the uptime field is the reliable reboot witness). The frozen tx_buf
audited at 20:20 was the post-reset app's lone config job; the engaged
cyclic never ran on that boot (stream closed at reboot, per the QUIET gate).

PENDING (the four parallel investigations): the stock's true select map,
the E42d-era experimental danger map, the +250 ms choreography, and the
alignment reconciliation — after which the next flash executes the decoded
recipe rather than testing a hypothesis.

### 6.2aw The four-agent decode and THE DECODED RECIPE (E60i) (2026-11-08, night end) [O]

Four parallel static investigations (Ghidra select map; bench-log danger map;
+250 ms choreography decode; alignment reconciliation) converged without
contradiction and settled the topology, the ammeter theory, and the recipe.

**TOPOLOGY, SETTLED:** bus A (RSPI-A) = four driver chips (SSLA3/SSLA2/SSLA1/
SSLA0, the 0xA2 class) + one dead select (SSLA5, 0x0000 replies) — word i →
SPCMD[i mod 5], every job restarting at SPCMD0 (launcher 0x96468, table
0x40CE4). **The supervisor is NOT in the bus-A rotation at all** — it is
RSPI-B/SSLB1, and RSPI-B physically lacks SSL3/SSL5, so it cannot hear a
bus-A frame. The origin of every conflicting note: the supervisor's bus-B
select word is numerically 0xAF11 = bus-A SPCMD2's value (SPCMD values are
peripheral-relative). All register-word danger lives on bus B SSLB1: bit0-free
→ grant-poison (the companion, Law C), with bit0 → reset (E42-E50). The
stock's own write job sends 8E9F/9021/9655 on BUS A (driver PWM/config
registers — 0x8E9F is the driver 1 kHz PWM constant, NOT a supervisor word).

**THE AMMETER THEORY, UNIFIED:** the stock timeline = rail (boot manager's
gapless 5 ms service, from T+0 — the rail never drops) + bit0 flowing from
~T+105 ms (one-shot enable 0x873F0 → 0x91C86; NOT engage-coupled, NOT
bench-armed) + the leap at T+200-250 ms (park arbiter target 8/0x17 → ~100 ms
fill phases → letters 0→0x21 → records → cyclic words step; 1910 mA at
+250 ms) — all inside the ~320 ms window. **No test of ours ever had all
three coincidences:** every ask landed outside the window (the proven recipe's
pass 150 = +750 ms; "the one cell never tested: asking while the rail is
still up" — the E20 note), and window-open boots never had grant+drivers.
Additionally: the stock runs BOTH buses interrupt-driven at equal priority
(IPR27 nibbles = 4/4) — neither bus ever waits on the mainline; our polled
bus-B mirror under bus-A burst load is the lateness class (withdrawal at
~11-20 ms late; hard reset past the E28 threshold). The E59 burst guard
(500000 ≈ 20 ms) equaled the reset threshold BY ITSELF — one hung burst was
sufficient; the leading mechanism for the 20:09:58 reset (6.2av's open item),
now that the word-wall reading is impossible for bus A.

**THE E60I RECIPE (the decoded choreography, executed):**
1. Companion OFF (Law C) — the stock never sends driver-cyclic words on bus B.
2. Stream OPEN from init; the config job commissions the drivers BEFORE bit0
   (the stock's order: config from the first 10 ms pass, bit0 from +105 ms).
3. THE SELF-ASK: sup_bit0_boot_arm(2000, 20) at init — bit0 flows from mirror
   pass ~20 (+100 ms), inside the window, the stock's one-shot timing.
4. THE LEAP: phase 1 = 15 ticks (150 ms) → phase 2 (the captured engaged
   values) at ~+245 ms from power-on (sched starts ~+95 ms after the boot
   handover; the E59 value of 25 ticks landed at ~+345 ms — past the close).
5. Guard 500000 → 100000 (~4 ms): a hung burst can never approach the 20 ms
   stall threshold; the 5 ms task order already runs the mirror before the
   bus-A bursts.
Corrections recorded: 0x980E0 = the A-DMAC channel configurator (6.2ap's
"SPCMD bit 13 = DMA enable" is wrong — SPCMD bit 13 is SPNDEN; the |0x2000 is
DMAC-side); 0x96468 = the job launcher; the stock's cyclic is DMA-launched
(SPCR=0xF8, DMAC count 45, TX 0xFFF88518) with vector 445 as the burst-end
handler; idle mode words are 0x8810-class (calibration bit 4), not bare 0x8800.

### 6.2ax E60i verdict — the recipe executed; the rail = a CERTIFICATION PATH (2026-11-08, night end) [O]

**E60i booted and executed the decoded recipe completely:** the self-ask
fired at ~pass 21 (+105 ms, INSIDE the window — delay=0/left=1999 = the grant
landed on the FIRST ask pass); the leap fired (tx holds the phase-2 engaged
setpoints e853/ed89); the drivers answered live (A212 headers, A200-class
replies to the engaged words); the enable held for 18+ minutes (bits 0x1a).
The ammeter: .233 spike, settle .167 — **no conduction.**

**The live state at full grant:** last_tx=0x8E91 / last_rx=0xAA91 (the
engaged shadow pair; echo AA91 = bit0 grant SET, bit4 life), the 0x22 echo =
AA91 — and **0x24 = AAFF, the refusal byte** (the stock's engaged reading is
AA20). The grant bit and the rail bit are DIFFERENT things: the stock's
engaged 0x22 echo = AAA1 (bit5 = RAIL up); ours = AA91 (bit5 = 0).

**THE RAIL'S TRUE NATURE (the spec's own 6.2i role decode + the choreography,
now unified): the rail is switch output 0 on the SSLB1 chip** — a driver-
family chip WITH SWITCH OUTPUTS whose 0x90 register bit 5 certifies output 0
on (the 'c' state letter from the switch-output manager 0x85828/0x8C7A8; the
stock's steady 0x90 word = 0x9020, masked ~(status&0xE0) so faulted outputs
clear). 0x24=AA20 is that chip ACKNOWLEDGING the output on. **We have sent
0x9021 since E10** (the "switch-enable bit 0" theory — wrong bit).

**Live test on the granted boot (FE13 sub08, no flash): pattern set to the
stock's 0x9020 — 0x24 STAYED AAFF.** The certification bit alone is not
sufficient: the stock queues the ENABLES JOB (0x40C0C) on every letter change
(0x92E80) — a separate bus-B job that actually enables the output — plus the
classifier-fed letter machinery (0xFFF90944) and the status-feedback mask.
That certification path is the one subsystem we have never exercised.

**Remaining work (bounded): decode the enables job 0x40C0C's frames and the
letter-change path, then one build that runs the certification at the grant.**
Everything else — transport, ask, grant, leap, drivers — is proven end to end.

### 6.2ay The ring verdict — continuity DEAD; the output stage is everything (2026-11-08, night end) [O]

**The E60i boot's echo ring (g_sup_rx_hist, the 0x22 echo per pass):**
passes 2-20 = AAB0 (bit5 = the rail PRIMED from our first passes — the
handover was gapless; the 2026-11-05 continuity theory is DEAD); pass 21 =
AAB1 (the self-ask fired at its armed pass and the supervisor accepted on
the FIRST pass — grant + primed rail coexisted for 44 passes); pass 65 =
AA91 (bit5 dropped at the window close — the "primed, not conducting" law
confirmed to the pass). The ammeter never moved during the entire B1 era.

**Therefore:** the conversation is flawless and accepted, the window was
used correctly, the drivers were commanded with the engaged values while
the rail was primed — and the output stage still never closed (0x24 = AAFF
throughout; the stock reads AA20 at the same point; the E6-era table's
other unexplained deltas: job3 replies ours AA45/AAFF vs stock AA46/AA05).
The sole remaining question: what closes SSLB1's output — the enables-job
path decode (in flight) targets exactly this.

### 6.2az THE WRONG-CHIP DISCOVERY — the AAFF was SSLB0 answering (2026-11-08, night end) [O]

The enables-path decode (descriptors verified from ROM): the stock's
0x40C0C and 0x40C54 load ONE SPCMD (0xAF11) — **both frames of each job go
to SSLB1**. Our mirror ran jobs 2 and 3 through plat_sup_xfer (the
SSLB1/SSLB0 alternation) — each job's SECOND frame (the 0x24 and 0x2E
reads) landed on SSLB0. **The entire AAFF "refusal" history was the wrong
chip's answer to registers it doesn't have.**

Live proof (the one-shot slate, hand-fed via the five-select):
- SSLB1 reg 0x14 = AAD5 — fault bits 6/7 (outputs 1/2, unpopulated — the
  stock's C7 shows the same), **bit5 CLEAR: output 0 (the rail) unfaulted**
- write 0x9020 → AA00; read 0x24 → **AA00 (no refusal)** — acknowledged,
  data 0x00 (the certification latches only in the granted context, which
  the probe had stopped); 0x28 → AA45; 0x2E → AA3B (real data)

**E60j = E60i + jobs 2/3 routed through plat_sup_burst (single-select
SSLB1) + the 0x90 pattern 0x21→0x20 (the stock's steady letter bit).**

### 6.2ba THE MISSING COMMISSIONING — the handshake had the same wrong-chip bug (2026-11-08, night end) [O]

E60j's live probes: the commit word 0x1200 ANSWERS (AAC2 — bit1 set, the
exact bit the stock's 0x14 carries and we lacked) but the certification
still reads 0x24=AA00. The spec's own 6.2ah hypothesis is the mechanism:
the certification latches only on a COMMISSIONED chip, and our E49 startup
handshake rode plat_sup_xfer's alternation — half its words (including
every commit) went to SSLB0. The stock's commissioning job 0x40A14 loads
ONE SPCMD: {84|value, 0A, 12(commit), 0C} — ALL to SSLB1.
**E60k = E60j + the handshake via plat_sup_burst (single-select SSLB1),
stock word order, sent twice.**

### 6.2bc The session ledger — E60l stable, E60m regression, the last byte (2026-11-08, night end) [O]

**E60l verdict:** 0x24 = AA00 (clean ack, data 0x00) with the full-stock
handshake — mode preamble + commit sequence, all to SSLB1. The certification
remains ack'd-but-unlatched with every wire word now stock-correct.

**E60m (ladder wire-jobs silenced): REGRESSION — a reset loop** (uptime 0 on
every heartbeat, 0x361 dead between samples). The engage path depends on the
ladder's poll/replies somewhere beyond the echo bit (to be mapped — the
suspect: the ladder's state machine starving breaks a downstream expectation).
**Rolled back: E60l restored 22:34:28 — the bench's stable reference.**

**THE REMAINING GAP, ONE BYTE: 0x24's data (0x00 vs the stock's 0x20).**
Undecoded candidates, in order: (1) the 84-write's runtime VALUE (the stock's
builder 0x91190 — ours is the guessed 0x1F); (2) the classifier letters'
internal conditions (0x85828/0x8C7A8/0x8C6D4 — what makes letters[0]='c' at
power-on); (3) the engage-path dependency E60m exposed.
