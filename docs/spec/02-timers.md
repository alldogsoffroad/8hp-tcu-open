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
| H | 4 MHz | TCRH = `0x10`, compare OCR1H = 4000 | **1 ms periodic tick** [O][D]; same setting in the boot software |
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

### 3.3 Single internal channel [O]

TOE52 (sub-block 5, channel 2; **not routed to a pin**): period from calibration = 200 ticks
(**10 kHz**). [I] Internal trigger (e.g. for ADC or DMA).

### 3.4 Unassigned outputs

Configured as TOE pins but not set up by the routines found so far: TOE00–TOE03 (PE6–PE9),
TOE10–TOE13 (PE10–PE13), TOE23 (PL4), TOE40 (PD5), TOE41 (PD6), TOE53 (PD3). **Open.**

## 4. Pulse / frequency inputs (timer F)

At start-up all 20 channels are set to a neutral state (4 MHz clock, no edges enabled,
noise-filter value 50) and started [O]. Modes are then assigned:

| Mode | Channels | Setup |
|---|---|---|
| **Rotation speed / pulse measurement** (MDF = 101) | **TIF12, TIF13, TIF14** (PF6, PF7, PF8); the same table also lists TIF0–TIF2 | edge per configuration; [I] the transmission speed sensors |
| **PWM input waveform timing** (MDF = 100) | TIF0–TIF5, TIF10, TIF11, TIF18, TIF19 (paired with group B outputs) | [O] |
| **PWM input waveform timing** (MDF = 100) | TIF6, TIF7, TIF8, TIF15, TIF16, TIF17 | separate routine [O]; [I] PWM-output sensors or driver feedback |
| unused / open | TIF9 | |

Noise cancellation is enabled on all timer F inputs and on timer B inputs (filter value `0x32`)
[O].

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
[I] The 8HP family has two transmission speed sensors (turbine and output), consistent with two
channels in use. Which of A/B is turbine and which is output could not be resolved statically
(the application layer accesses them through indirect calls) — **open, resolve on the bench**.
60 pulses per revolution is a calibration value and must be confirmed for the target
transmission.

## 4.2 Timer G — ADC and multiplexer triggers

All six timer G channels: 4 MHz clock, compare-match pulse output enabled (`TCRG = 0x12`); the
counter clears on compare match, so the period is OCRG + 1 ticks [M][O].

| Channel | OCRG | Start count | Period | Use |
|---|---|---|---|---|
| G1 | 50 | `0xFFD8` (−40) | 12.75 µs | triggers AN0 conversion (spec 04 §4.2) |
| G2 | 50 | 0 | 12.75 µs | DMA request: writes multiplexer address to port G |
| G4 | 4000 | `0xFFB0` (−80) | ≈1.000 ms | [I] scan-conversion trigger |
| G5 | 4000 | `0x0F64` | ≈1.000 ms | [I] second 1 ms trigger, phase-offset |

## 5. Timer C

Capture/compare channels TIOC00–TIOC02, TIOC10, TIOC20 (spec 01 §2.3) are configured by a small
number of routines; pins PD0 and PD8 are switched between GPIO and timer function at run time [O].
Roles **open**.

## 6. Open items

1. Signal meaning of every PWM group and input (needs bench measurement or shift-control analysis).
2. Duty-cycle update paths and rates for group A.
3. Setup of the unassigned TOE outputs (§3.4) and timer C.
4. Mapping of speed-sensor channels to transmission shafts.
