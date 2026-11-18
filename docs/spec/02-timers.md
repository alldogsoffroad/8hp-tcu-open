# Spec 02 — Timers (ATU-III), PWM outputs and pulse inputs

Status: **draft.** Clock tree, tick, the configured PWM groups and the input modes are established;
the role of each signal and the unassigned outputs are **open**.
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference, **[D]** derived.
Pφ = 20 MHz (spec 05).

## 1. ATU-III clock tree

Prescaler division = PSC + 1 [M §13.7].

| Clock-bus line | Prescaler setting | Frequency | Period |
|---|---|---|---|
| 0 | PSCR0 = 19 | **1 MHz** | 1 µs |
| 1 | PSCR1 = 4 | **4 MHz** | 250 ns |
| 2 | PSCR2 = 6 | 2.857 MHz | 350 ns |
| 3 | PSCR3 = 0 | 20 MHz | 50 ns |

The ATU master enable is set after the prescalers [O]. The software re-checks the prescaler values
and master-enable bits at run time [O]; a reimplementation should do likewise.

## 2. Time base

| Timer | Clock | Setting | Use |
|---|---|---|---|
| A — free-running counter TCNTA | 1 MHz | CKSELA = 0 | **µs timestamp**, read throughout the software (≈220 reads) [O] |
| H | 4 MHz | TCRH = `0x10`, compare OCR1H = 4000 | ~~**1 ms periodic tick** [O][D]~~ configured for 1 ms, but its compare interrupt CMIH (vector 376) is never enabled in the app (stub handler, IPR20 not written); the app's 1 ms tick is **ATU-D1 compare CMID10** (OCRD10 `0xFFFFF5D0`, vector 228, IPR10 nibble 5 → 6) — corrected 2026-11-09, `~/tcu_sweep/spec_arch_validation.md` item 1 (spec 24 §1.4); same setting in the boot software |
| J (channels 0, 1) | 4 MHz | free-running, compare = `0xFFFF` | [I] auxiliary timing |

## 3. PWM outputs (timer E)

Timer E: all six sub-blocks use clock-bus line 1 (4 MHz) with sub-block prescaler PSCRE = 1
(÷2) → **2 MHz count clock, 0.5 µs per tick** [O][M]. All channels of all sub-blocks are started
[O]. Each channel has a period register (CYLR) and a duty register (DTR) with reload buffers [M].

### 3.1 Group A — five channels, 1 kHz, staggered start [O]

| Output | Pin | Period | Initial counter offset | Polarity setting |
|---|---|---|---|---|
| TOE30 | PL5 | 2000 ticks = 1 ms | 327 | 1 |
| TOE31 | PL6 | 2000 | 1452 | 1 |
| TOE33 | PL8 | 2000 | 702 | 1 |
| TOE32 | PL7 | 2000 | 1077 | 1 |
| TOE22 | PL3 | 2000 | 702 | 1 |

Duty is updated at run time. **Function [O]: group A is the on/off ("switch") output driver**
(spec 12 §1.2): TOE31 = park hold solenoid; TOE30 and TOE32 enabled with meanings open; TOE33
and TOE22 unused in this calibration. Duty in per mille, battery-voltage compensated. (Earlier
guess that group A drives the bus-0 chips is withdrawn; the solenoids are on SPI.)

### 3.2 Group B — five channels, 1 kHz, fixed 50 µs pulses, each paired with two inputs [O]

| Output | Pin | Paired timer F inputs | Phase offset |
|---|---|---|---|
| TOE42 | PD7 | TIF10, TIF11 | 0 |
| TOE43 | PD9 | TIF0, TIF1 | 200 µs |
| TOE50 | PD10 | TIF2, TIF3 | 400 µs |
| TOE51 | PD11 | TIF4, TIF5 | 600 µs |
| TOE21 | PL2 | TIF18, TIF19 | 800 µs |

Period = calibration value × 2 ticks; the stock calibration gives 1000 → **2000 ticks = 1 kHz**.
Pulse width fixed at 100 ticks = **50 µs**. Offsets from a calibration table (0, 400, 800, 1200,
1600 ticks). Enable mask from calibration (stock: all five).
[I] Narrow, staggered pulses with two measured inputs per output look like a trigger/excitation
signal with feedback — e.g. sensor excitation or driver diagnostics — rather than solenoid drive.

**Correction / resolution (2026-11-13 and 2026-11-18, sources: `~/tcu_sweep/solenoid_chain_decode.md` §0 item 1; `~/tcu_sweep/feedback_chain_decode.md` §0; applied
2026-11-18):** group B is **one pulse per bus-A driver chip** (TOE42 ↔ chip 0 … TOE21 ↔ chip 4), at the same
frequency the chips' `0x8E` register is programmed with (`0x8E = 32000/f − 1 | 0x80` = 0x9F for f = 1000) [O link; I
= a PWM sync to the chips]. The two paired Timer F inputs per output are the chip's two channels' **PWM feedback**:
each bus-A channel presents a 1 kHz active-low signal whose low time is the chip's switch-on time per chopper period
(ch 0–9 → TIF10, TIF11, TIF0A, TIF1A, TIF2A, TIF3, TIF4, TIF5, TIF18, TIF19) [O table `0x40D96`] — spec 12 §2b.

### 3.3 Single internal channel [O]

TOE52 (sub-block 5, channel 2; **not routed to a pin**): period from calibration = 200 ticks
(**10 kHz**). [I] Internal trigger (e.g. for ADC or DMA).

### 3.4 Unassigned outputs

Configured as TOE pins but not set up by the routines found so far: TOE00–TOE03 (PE6–PE9),
TOE10–TOE13 (PE10–PE13), TOE23 (PL4), TOE40 (PD5), TOE41 (PD6), TOE53 (PD3). **Open.**

**Addition (2026-11-13, sources: `~/tcu_sweep/solenoid_chain_decode.md` §0 item 1; `~/tcu_sweep/timerE_0b_port.md`; bench r03c, CLAUDE.md; applied 2026-11-18):**
these channels have **no writer at all**. `0x97198` sets only TCRE/PSCRE/TIERE/SSTRE/TSTRE/IPR, so they keep reset
DTR = CYLR = 0xFFFF with the counter started → **constant HIGH ≈ 33 ms after TEE comes on, every boot** [O + M].
They are 12 pins (the decode's "11" miscounted its own list). The stock clears TEE (`0x96F68(700)`) before `0x97198`,
zeroes the used channels' duty with TEE off (`0x9136E` group A, `0x919A0` TOE52, `0x90010` → `0x8FF66` group B) and
sets TEE in the mode-0 init list (`0x91F3E`). Bench r03c reproduced this with no current change. Function of the 12
pins: open.

## 4. Pulse / frequency inputs (timer F)

At start-up all 20 channels are set to a neutral state (4 MHz clock, no edges enabled,
noise-filter value 50) and started [O]. Modes are then assigned:

| Mode | Channels | Setup |
|---|---|---|
| **Rotation speed / pulse measurement** (MDF = 101) | **TIF12, TIF13, TIF14** (PF6, PF7, PF8); the same table also lists TIF0–TIF2 | edge per configuration; [I] the transmission speed sensors |
| **PWM input waveform timing** (MDF = 100) | TIF0–TIF5, TIF10, TIF11, TIF18, TIF19 (paired with group B outputs) | [O]; **the bus-A driver chips' per-channel PWM feedback** (2026-11-18, `~/tcu_sweep/feedback_chain_decode.md` — §3.2 correction, spec 12 §2b) |
| **PWM input waveform timing** (MDF = 100) | TIF6, TIF7, TIF8, TIF15, TIF16, TIF17 | separate routine [O]; [I] PWM-output sensors or driver feedback |
| unused / open | TIF9 | |

Noise cancellation is enabled on all timer F inputs and on timer B inputs (filter value `0x32`)
[O].

**Addition (2026-11-18, sources: `~/tcu_sweep/feedback_chain_decode.md` §0, §1, §3.2; `~/tcu_sweep/feedback_shadow_port.md` correction; `~/tcu_sweep/feedback_pins_decode.md` §7; applied 2026-11-18):** the
feedback channels capture one PWM period each at 4 MHz (GRBF = 1); a Timer-D0 compare ISR (vector 216, `0x90A72` →
`0x90844`) snapshots two windows 5.12 ms apart [D: if TCNT1D0 counts at 1 MHz] and the classifier `0x89A8C` gives
0x10 (PWM), 0x20 (always active), 0x21 (inactive), 0x31 (invalid). **Inside the 980–1020 µs period window class 0x20
needs A ≥ P − 10 µs (990 ‰); the 950 ‰ saturation rule applies only outside the window** [O `0x89B5A–0x89BA6`].
The six other PWM inputs (TIF6/7/8/15/16/17, `0x91884`/`0x918F6`, cfg `0x19040C`) are not part of this chain;
their function is undecoded. Measurement note [O]: PxPR is the pin state in every function, but a meas read is
CMT-locked, so periodic pin signals sampled that way show a constant phase (`0xFFFFF600` is the Timer D2 block, not
Timer F — TSTRF is `0xFFFFFA00`).

### 4.1 Speed measurement (TIF12, TIF13, TIF14)

Rotation-speed mode is only available on timer F sub-blocks 12–15 [M], which is why these three
inputs are used. Three speed channels are processed every cycle [O]:

| Channel | Input | Pin | Pulses per revolution (stock calibration) |
|---|---|---|---|
| 0 | TIF12 | PF6 | 60 |
| 1 | TIF13 | PF7 | 60 |
| 2 | TIF14 | PF8 | 60 |

Hardware captures per edge [M]: low time (GRAF), period (GRCF), accumulated time (GRDF) and a
running rising-edge count (ECNTBF); the counters carry 8 fraction bits, so the software shifts
them right by 8 to obtain 4 MHz ticks [O].

Processing (behavior to reproduce) [O]:

1. **Consistent read:** the capture registers are read repeatedly until the edge count is stable
   across the read, so all values belong to the same edge.
2. **Classification** per update, using calibrated minimum/maximum edge intervals: first edge
   after start, valid period, implausible period, or timeout (shaft stopped).
3. **Speed** from a short ring buffer of (time, edge count) samples:

   ```
   speed = 0.5 + scale × Δedges / Δtime_ticks          (0.5 = rounding)
   scale = cal_factor × 960 000 000 / pulses_per_rev   (cal_factor = 1.0 in stock data)
   ```

   With 4 MHz ticks, `960 000 000 = 60 s/min × 4 MHz × 4`, so the result is in
   **¼ rpm units** (the software divides by 4 where whole rpm is needed) [D].
4. **Decay when pulses stop:** if `scale / (time since last edge) × 1.1` is less than the last
   computed speed, the speed is replaced by that bound, so it falls smoothly toward zero instead of
   holding the last value [O].
5. The result is saturated to 16 bits and published per channel, together with a status code
   [O].

Routing to the application [O]: the transmission application does not read channels directly.
Two adapters copy speeds into application signals, each taking its channel number from
calibration:

| Application speed signal | Source channel (stock calibration) |
|---|---|
| A (RAM `0xFFF93044`) | channel 1 = **TIF13** |
| B (RAM `0xFFF93048`) | channel 0 = **TIF12** |
| — | channel 2 = TIF14 is not routed to the application in this calibration |

Both signals are then gathered with many others into the application's input structure.

**CORRECTION (2026-11-16) [O]** (`~/tcu_sweep/park_position_speed_decode.md`, re-checked against the bytes 2026-11-16):
- A/B (`0x93044`/`0x93048`) are **not speeds**. They are u32 copies of word +4 of the per-channel record `0xFFF90444 + 12·ch` (a time base, [I] µs; the dump reads FFFFFFFF at rest).
- The safety layer's `0x143686` turns them, with the edge counts `0x940F0`/`0x940EC`, into rpm, using 40 (output, B) / 30 (turbine, A) pulses per revolution.
- `0x14C23E` selects that rpm into output speed `0xFFF8EE6E`; turbine goes via `0x14C818` → `0xFFF8EF06`.
- On a sensor fault, output speed is a substitute 0xFFF8E798 × 0xFFF955B4 / 10000 ([I] wheel speed × axle ratio).
[I] The 8HP family has two transmission speed sensors (turbine and output), consistent with two
channels in use. Which of A/B is turbine and which is output could not be resolved statically
(the application layer accesses them through indirect calls) — **open, resolve on the bench**.
60 pulses per revolution is a calibration value and must be confirmed for the target
transmission.

**Bench resolution (2026-11-16) [O]:** steel passed across the sensor at the rear of the transmission (the output speed sensor, identified by the user) counted on TIF12 only (ECNTBF(12) +1 per pass, 7 passes; F0–F15 otherwise unchanged). So **channel 0 = TIF12 = output speed**, and stock signal B (`0xFFF93048`) is output speed. Signal A (`0xFFF93044`, TIF13) is turbine speed [O bench 2026-11-16: a magnet with alternating poles passed across the input speed sensor counted on TIF13 only, +1 per pass, 6 passes; that sensor reads a magnetic ring, so steel does not trigger it]. The path from A/B into the safety layer's `0xFFF8EE6E` / `0xFFF8EF06` (spec 07 §7.1) is now decoded (correction above).

**Pulses per revolution, project (2026-11-16):** the stock's speed computation `0x143686`, which feeds output speed `0xFFF8EE6E` (D646) and turbine speed `0xFFF8EF06`, uses **40 for output and 30 for turbine** (cal `0x195DCD` = 0x28, `0x195DCE` = 0x1E) [O bytes; `~/tcu_sweep/park_position_speed_decode.md`], not the Timer F table's 60. The project now uses 40 (channel 0, output) / 30 (channel 1, turbine) / 60 (channel 2, unused), on the user's decision to follow the stock. Not physically counted: parking-gear teeth and magnetic-ring poles remain to be checked when the transmission is open.

### 4.1a Stock register values and project implementation (M2)

Stock set-up [O]: prescalers `PSCR0..3 = 19, 4, 6, 0` and noise-canceller mode `NCMR = 0x9F`
(`0x96F28`); all 20 timer F control bytes `0x20` (clock bus 1, no edge), noise filter `NCRFAn = 0x32`,
noise cancel enabled and all subblocks started (`NCCRF = TSTRF = 0xFFFFF`, `0x9733C`); the speed
channels F12–F14 (table `0x40DA5`) then get mode `101` and the edge from their configuration
(`0x97444`; stock: rising) → `TCRF = 0x35`. ATU master enable through a shadowed helper
(`0x96F56`); timer F needs `ATUENR.TFE` (bit 6) and the prescaler enable (bit 0).

Project (`firmware/platform/sh7254r`, `firmware/io/sensors/speed.c`): the same register values for
F12–F14 only. Every 10 ms the edge count (`ECNTBF`), last-edge time (`GRDF >> 8`) and time since
the last edge (`ECNTCF >> 8`) are read consistently; speed = edges / time between edges, decaying
with a 1.1 margin when pulses stop and reported 0 after 1 s without an edge; samples above
12 000 rpm are rejected. Pulses per revolution are configuration (stock 60). The channel-to-shaft
assignment is configuration: output = TIF12 (channel 0), turbine = TIF13 (channel 1), per the stock routing and the bench (2026-11-16). Until 2026-11-16 the project default was the reverse (turbine = TIF12, output = TIF13 [I]), which the bench refuted.

## 4.2 Timer G — ADC and multiplexer triggers

All six timer G channels: 4 MHz clock, compare-match pulse output enabled (`TCRG = 0x12`); the
counter clears on compare match, so the period is OCRG + 1 ticks [M][O].

| Channel | OCRG | Start count | Period | Use |
|---|---|---|---|---|
| G1 | 50 | `0xFFD8` (−40) | 12.75 µs | triggers AN0 conversion (spec 04 §4.2) |
| G2 | 50 | 0 | 12.75 µs | DMA request: writes multiplexer address to port G |
| G4 | 4000 | `0xFFB0` (−80) | ≈1.000 ms | ~~[I] scan-conversion trigger~~ **ADC_A scan trigger** (ADCSR0 TRGE, `0x95F68`) → DMAC ch5 → DEI5 [O, 2026-11-12, `~/tcu_sweep/adc_chain_port.md`] |
| G5 | 4000 | `0x0F64` | ≈1.000 ms | ~~[I] second 1 ms trigger, phase-offset~~ **ADC_B scan trigger** (ADCSR1 TRGE, `0x95F7A`) → DMAC ch4 → DEI4 [O, 2026-11-12, `~/tcu_sweep/adc_chain_port.md`] |

*Note (2026-11-12, `~/tcu_sweep/adc_chain_port.md`):* Timer G1's trigger is an AN0 interrupt conversion (ITTRGS = 1); Timer G2's DMA request is
CMEG2 (TCRG2 = 0x13, set by `0x96900`). ATUENR.TGE stays 0 until `0x91F3E` (`ATUENR |= 0x3FF`), so the counters are
frozen through init [O].

## 5. Timer C

Capture/compare channels TIOC00–TIOC02, TIOC10, TIOC20 (spec 01 §2.3) are configured by a small
number of routines; pins PD0 and PD8 are switched between GPIO and timer function at run time [O].
Roles **open**.

**Addition (2026-11-12, sources: `~/tcu_sweep/atu_init_audit.md`; CLAUDE.md r02h/r02i/r02j blocks and the ladder decodes; applied
2026-11-18):** the stock's Timer C init is **`0x96FC8`** (inside the ATU init `0x91FA4`): table `0x40F68`, per channel n
TIORC = table word, TCRC = table byte (**TCRC1 = TCRC2 = 0x09: PWM mode, clock-bus line 1**; TIORC1 = 0x4443), TIERC =
0, TSRC cleared, TCNTC = 0; NCRC 0xFFFFF414 + 8n..+3 = 0x32; NCCRC 0xFFFFF402 + n = 0x0F; IPR07 low byte 0x77; TSTRC =
0x1F [O bytes]; `0x91ED4` then retunes channels 1/2. **TIOC10/TIOC20 (PD4/PD8) carry a PWM clock that the supervisor
chip monitors:** 0x14 bit 4 = "TIOC10/20 signal absent" (cleared on the bench once Timer C ran in this configuration,
r02i) and bit 1 needs the clock present [O bytes, I meaning; spec 23/26 corrections]. The self-test ladder's pin-cut
test (`0x85788` → `0x91E6C` cut / `0x91E4E` restore) switches PD4/PD8 to GPIO for one tick and expects 0x14 bit 1 to
drop. *(The "PD0" above is not re-checked against this; the sources name PD4/PD8.)*

## 6. Open items

1. Signal meaning of every PWM group and input (needs bench measurement or shift-control analysis).
2. Duty-cycle update paths and rates for group A.
3. Setup of the unassigned TOE outputs (§3.4) and timer C. *(Timer C: decoded 2026-11-12, §5 addition; the
   §3.4 pins have no writer, §3.4 addition.)*
4. ~~Mapping of speed-sensor channels to transmission shafts.~~ Resolved 2026-11-16 (§4.1: TIF12 output,
   TIF13 turbine).

## 7. The stock's scheduling architecture: bare metal, no RTOS (2026-11-03) [O]

A structural sweep of the full dump (boot area, stock application, session engine, boot MAT)
found **no real-time OS anywhere** — answering the standing question whether the unit runs a
Renesas HI7000/ITRON kernel. The evidence is structural, not string-based (HI7000/4 ships as
source and embeds no mandatory banner): **zero `trapa` instructions at any genuine instruction
boundary** across all 323 byte-candidates (cross-checked against the seeded function map — a
service-call mechanism does not exist); the application entry (`0x66CA8`) makes ~20 direct
init calls and tail-jumps into a **busy superloop** (`0x67A68`); the vector table holds a
dozen real ISRs with unexpected-interrupt reporter stubs (any kernel trampoline would surface
there as logged faults); and no context-dispatch routine (full-register save/restore from
TCBs) exists anywhere in the image.

The periodic work — including the 15.625 ms (1/64 s) CAN schedule of spec 08 — is driven by
the tick ISR (vector 228, `0x677F8`). **2026-11-04 correction (full sh2a disassembly, AUTOSAR
follow-up below):** the tick does *not* invoke callbacks directly. It walks two **flat cyclic
tables** of 20-byte entries `{enable@0, arg-u16@+2, period-u16@+4 (min-entry wins), arg-u32@+8,
len-u8@+12, data-ptr@+16}` — base `0xFFF908DC` (8 entries) and `0xFFF909DC` (1 entry; the
earlier `0xFFF9097C` was just the end of table 1, 8×20=0xA0) — and the `+16` word is a **data
pointer, not a callback**. Each firing hands `{arg, data[len], id@+12, flags@+14}` to
dispatcher `0x934AC`, which **enqueues a 16-byte slot into a per-type ring buffer** (control
blocks at RAM `0xFFF943E8 + 8*type`: `{limit, cursor, max, bufptr}`); the superloop workhorse
`0x67FBE` (called from `0x67A68`) drains the rings and executes the work. Entry enable-byte is
cleared after firing (one-shot) except index 0 (the periodic anchor); the ISR tail re-arms from
the flash rate table `0x66468` via cursor RAM `0xFFF909F8`. So the mechanism to reproduce is
tick → 20-byte cyclic tables → per-type rings → main-loop consumer, with SR.I-level lock
helpers (`0x96650`/`0x9665C`) guarding the queues. The one remaining unknown is the
runtime-constructed session/security code (spec 17 §5e), which is a CAN session handler and
not a scheduler; a live `0x2A` RAM read of the engaged stock over `0xFFF80000–0xFFF9FFFF`
would close it conclusively (any kernel would show TCB/stack arrays and priority ready-lists;
bare metal predicts the flat tables, cursor, and rings above).

### 7b. AUTOSAR OS: also absent (2026-11-04) [O]

Asked whether the *AUTOSAR* OS (SWS "Specification of Operating System" R22-11 — a different
lineage from ITRON, descended from OSEK/VDX) runs on the stock. **No, high confidence.**
The ITRON verdict's `trapa` evidence does **not** transfer (AUTOSAR OS services are plain
function calls), but the structural evidence does, re-proven with correct SH-2A encodings:

- **No context switcher anywhere.** Full-app scan of push banks (`movmu.l` ×7,317 — all
  ordinary prologues, max run 10 and those are data coincidences; zero `sts.l pr` + ≥4-push
  sites), multi-register `movml.l` at only 9 sites (ISR trampolines `0x96684`/`0x9A53A`, hash
  `0x99AD8` — same-stack save/restore, `rte`, no SP reload), zero `ldc ssr/spc`, and exactly
  one `mov rN,r15` in the app (a 2-instruction leaf at `0x95336`). No code loads a register
  set from any TCB array.
- **No AUTOSAR static config shapes.** Startup `0x66CA8` makes ~20 direct init calls — no
  StartOS structure, no hook invocations (StartupHook/PreTaskHook/... are mandated in every
  conformance class), no autostart-task activation loop. The flash descriptor table
  `0x43794` (20-byte entries, 14 literal-pool references) has no priority/stack/activation
  fields — it is signal/measurement metadata for the `0x92–0x96` subsystem.
- **Tick architecture is incompatible.** AUTOSAR counters drive alarms with absolute-expiry
  and *task activation queues*; the stock's per-entry down-counters feeding message rings are
  a hand-rolled cyclic scheduler.
- **Strings: zero hits** for AUTOSAR/OSEK/OSAL/MICROSAR/RTA-OS/tresos/ERIKA and all ~40
  API+hook names — weak evidence both ways (SWS_Os_NA_00767: no mandated version banner), so
  the verdict rests on structure. The plant engine image also scans clean.

A minimal non-preemptive BCC1/SC1 build could *resemble* a superloop, but it would still leave
an activation queue, task descriptors with priorities, and hook call sites — none exist.

### 7c. Runtime confirmation — live 0x2A dump of the actuating stock (2026-11-04) [O]

The stock app (only block `0x40000` restored over our firmware; its body/tail/calibration were
still in flash beneath) booted, **self-engaged, and drove the bench mechatronic** — ammeter
1.773 A (in the user's remembered 1.7–1.86 A solenoid-driving family, 2026-11-04), solenoids
audibly humming on the valve body — while its entire on-chip RAM `0xFFF80000–0xFFFA0000` was
dumped through its own measurement protocol (`0x361` cmd `0x2A`, 6 B per round-trip, strict
ping-pong; 118,887 B read, 0 lost, 12,189 denied in three windows: `0xFFF8F400–0xFFF90000`,
`0xFFF94A00–0xFFF94D00`, `0xFFF9E000–0xFFFA0000` — the last is the active I/O buffer pool the
schedule table points into).

- **The flat schedule table is live at `0xFFF909C0`**: 7 × 20-byte records
  `{buf-ptr@0 → 0xFFF9EExx/0xFFF90Fxx, seq@4, type@8, pad@10, CAN-id@12, flags@16}`.
  Six of the seven ids equal the live bus-1 census (`0x144/0x170/0x1A0/0x2A8/0x178/0x3D1`;
  7th is `0x300`), and `type` is a **rate divisor of a common ≈104 Hz base**: 2 → 52 Hz,
  `0x0A` → 10.4 Hz, `0x32` → 2.1 Hz — each matching its census count exactly. This is spec 08's
  1/64 s schedule mechanism, observed mid-operation. (The §7 static decode described the flash
  initializer's entry layout; the live record differs — buffer pointer at +0, id at +12 — and
  the "cursor at `0xFFF909F8`" was a record's flags field.)
- **Zero kernel signatures anywhere in readable RAM**: no TCB cadence (64 B / 140 B structure
  scans at noise level), no task-stack ladder (47% of RAM is sparse static emptiness in ~20
  fragments — static buffers, not N stacks), no priority ready-list.
- The architecture was doing hard real-time hydraulic control (solenoid hold currents) as a
  bare superloop over flat tables and buffers while all of the above held.

**The §7 verdict is runtime-confirmed: no RTOS of any family runs on this unit.** Question
closed for ITRON and AUTOSAR alike.
