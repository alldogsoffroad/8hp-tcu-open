# Spec 04 — Analog inputs (ADC)

Status: **draft.** Converter setup, data path, scaling and divider ratios are established; the
physical meaning of most channels is **open**.
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference, **[D]** derived.

## 1. Converters and channels

| Unit | Base | Pins | Scanned channels | Conversion time |
|---|---|---|---|---|
| ADC0 | `0xFFFFE800` | AN0–AN27 [M] | **AN1–AN27** (`ADANS0 = 0xFFFE`, `ADANS1 = 0x0FFF`) [O] | 50 Pφ states (CKS = 0) [O][M] |
| ADC1 | `0xFFFFEC00` | AN40–AN48 [M] | **AN40–AN46** (`ADANS3 = 0x7F`); AN47, AN48 unused [O] | 25 Pφ states (CKS = 1) [O][M] |

AN0 is excluded from the scan and instead enabled as a **timer-triggered conversion**
(`ADTRE0` bit 0, trigger source selected by `ADCER0` bit 0) [O][M]. [I] AN0 is sampled
synchronously with a PWM/timer event — typical of solenoid current sensing.

Control values written at start-up [O]: `ADCSR0/1 = 0x10`, `ADCER0 = 0x4F01`, `ADCER1 = 0x4F80`.
Both units have the **self-test function enabled** with the fixed AVref × 1 test voltage
(`DIAGM = 1`, `DIAGLD = 1`, `DIAGVAL = 11`) [O][M]; the self-test result register is checked by
the software (converter plausibility). Result format: left-aligned (`ADRFMT = 0`) [O][M].

## 2. Data path (behavior to reproduce)

1. Conversions are moved to RAM by A-DMAC (ADC channels), into buffers in the DMA alias area,
   which is based at `0xFFF88000` (`ADMAABR = 1`) [O][M]:
   - buffer 0 at alias offset `0x44C`, reload count `0x50`
   - buffer 1 at alias offset `0x5E8`, reload count `0xA0` (and a second `0xA0` reload)
2. Each DMA buffer is followed by a **guard word `0x5B97`**. The processing task checks it every
   cycle; if it was overwritten (DMA overrun) it restores it and increments an error counter [O].
   (The SPI buffers use the same guard.)
3. A periodic task **accumulates** samples per channel and counts them [O].
4. The averaging task converts each accumulated sum to a value in **millivolts at the module
   connector** [O][D]:

```
value_mV = sum × (5000 / 65536) × divider_ratio / sample_count
```

   - `5000 / 65536` = 0.0762939453125 (stored constant) [O] → full scale of the left-aligned
     16-bit result is 5.000 V, i.e. [I] **AVref = 5.0 V**.
   - `divider_ratio` per channel from a calibration table (§3) [O]; [I] these are the external
     resistor-divider ratios.
   - Outputs are 16-bit unsigned millivolts in three RAM tables: AN0–AN27, AN40–AN46, and four
     additional accumulated inputs [O].

## 3. Divider ratios (stock calibration) and voltage ranges

| Channels | Ratio | Input range at connector [D] |
|---|---|---|
| AN0, AN1, AN8, AN11–AN18, AN24–AN27, AN40, AN42–AN46 | 1 | 0–5 V |
| AN2, AN23, AN41 | 2 | 0–10 V |
| AN3, AN4, AN5, AN6, AN9, AN10 | 2.5 | 0–12.5 V |
| AN7 | 4 | 0–20 V |
| AN22 | 4.8 | 0–24 V |
| AN19, AN20, AN21 | 5 | 0–25 V |
| four additional accumulated inputs | 5 | 0–25 V |

[I] Ratio-1 channels are 0–5 V sensor inputs (temperature, pressure, position, current-sense
amplifiers); ratio ≥ 4 channels are supply-level signals.

## 3a. Supply measurement calibration (bench 2026-10-18) [B]

With the stock divider ratio 5, AN21 reads low by a constant factor (bench spare, supply on pins
13 + 9, multimeter at pins 13/14 = 12.47 V while the supply display showed 12.5 V):

| Supply (display) | Reported (ratio 5) |
|---|---|
| 10.0 V | 9.56 V |
| 12.5 V | 11.93 V |
| 14.0 V | 13.35 V |

Linear fit: reported = 0.9476 × supply + 0.084 V (residuals < 2 mV) — a scale error of −5.2 %,
not a fixed diode/lead drop. [I] Actual divider about 5.28, ADC reference slightly high, or
sampling load of the high-impedance divider. The project firmware corrects the reported supply:
true = 1.0553 × reading − 89 mV (`io/sensors/sensors.c`); after the correction it reports
12.50 V at 12.47 V measured. The park-contact classification (§4.4 bands, ratio to supply) keeps
the uncorrected reading, which the stock thresholds were derived with. Not yet checked: whether
the other ratio-5 channels (AN19/AN20 switched supplies) share the factor — only on/off is used.

## 4. Channels identified so far

| Channel | Ratio | Observed use | Interpretation |
|---|---|---|---|
| AN19 | 5 | compared against 3000 mV / 4000 mV with hysteresis → on/off flag [O] | [I] presence of a switched supply / wake signal |
| AN20 | 5 | same, separate flag [O] | [I] presence of a second switched supply |
| AN21 | 5 | used by two supervision routines [O] | [I] supply voltage |
| AN7 | 4 | checked against a calibrated band around a reference; sets diagnostic flags [O] | [I] monitored supply/reference rail |
| AN0 | 1 | timer-triggered; read by four routines [O] | [I] current or fast sensor signal |
| **AN8** | 1 | NTC temperature evaluation, see §4.1 [O] | [I] **transmission fluid temperature** sense voltage |
| **AN1** | 1 | second input of the same evaluation [O] | [I] sensor return / reference voltage |

### 4.1 Temperature sensor evaluation (AN8 / AN1)

Executed periodically by one routine [O]:

1. Read AN8 and AN1 (mV). `V_diff = AN8 − AN1` (0 if negative).
2. Ratio check value `R = (5000 − V_diff) × 1000 / max(AN1, 10)`, capped at 40000.
3. Plausibility checks, each setting an error bit, limits from calibration (stock values, same for
   both sensor types):

   | Check | Limit (stock) | Bit |
   |---|---|---|
   | AN1 below minimum | 5 mV | 0x01 |
   | AN1 above maximum | 423 mV | 0x02 |
   | AN8 below minimum | 508 mV | 0x04 |
   | AN8 above maximum | 4700 mV | 0x08 |
   | V_diff below minimum | 135 mV | 0x10 |
   | V_diff above maximum | 4512 mV | 0x20 |
   | R outside window | 10000 … 15500 | 0x40 |
   | change of V_diff between cycles above limit | 100 mV | 0x100 (only when no other fault; 0x80 = all checks passed) |

4. If valid (bit 0x80 set, 0x100 clear): temperature = curve(AN8 mV) by linear interpolation in
   the table selected by a coded **sensor type** (0 or 1). Result is a signed 16-bit value in
   **1/16 °C**; status = 8.
5. If invalid: either hold the last value (status 7) or use a calibrated substitute
   (stock: **120 °C**, status 3) after a calibrated time.
6. The result is published to the rest of the software as a **value / bitwise-complement pair**
   (corruption check) [O].

Stock conversion curve (both sensor types use identical stock data; stored as 16 points,
temperature encoded as `0x8000 + 16 × °C`) [O]:

| AN8 (mV) | 538 | 568 | 653 | 747 | 880 | 1069 | 1334 | 1692 | 2151 | 2687 | 3241 | 3735 | 4110 | 4355 | 4457 | 4494 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| °C | 170 | 160 | 140 | 125 | 110 | 95 | 80 | 65 | 50 | 35 | 20 | 5 | −10 | −25 | −35 | −40 |

The curve values are calibration data. The implementation should change or re-derive them from the
sensor datasheet or measurement rather than copy this table; it is recorded here to validate
behavior.

**Circuit derived from the stock limits** [D]: the AN1 upper limit (423 mV) and the ratio check
`(5000 − V_diff) / AN1` ≈ (R_pull + R_sense) / R_sense match the topology
5 V → pull-up → AN8 → **NTC sensor** → AN1 → sense resistor → 0 V, with R_pull / R_sense ≈ 10.4–13
(at 170 °C the model gives AN1 ≈ 429 mV). A KTY/PTC model does not fit (error > 20 °C).
Steinhart–Hart fitted to the stock curve shape leaves a residual of about ±3 °C.

**Project implementation (M2)** [D]: `firmware/io/sensors/tft.c` measures
r = (AN8 − AN1) / AN1 = R_sensor / R_sense (independent of the 5 V supply and the pull-up) and
converts it with the project's own table `tft_table.h`, generated by `tools/gen_tft_table.py`
from **provisional** Steinhart–Hart coefficients (−50…180 °C, 5 °C steps). Plausibility limits
are the project's own, derived from the circuit model. The table must be replaced with the
sensor's datasheet values or a bench measurement against a reference thermometer (bench item 22).

**Bench check 2026-10-18 (bench item 22, partial)** [M]: electronics module alone (no valve body),
internal fluid-temperature sensor against a handheld thermometer at the housing:

| Reference | TCU (`tft.c`) | AN8 / AN1 (mV) | Difference |
|---|---|---|---|
| 24.7 °C | 22.4 °C | 3010 / 166 | −2.3 °C |
| 27.8 °C | 26.3 °C | 2876 / 178 | −1.5 °C |

The TCU reads about 2 °C low, within typical NTC tolerance and the uncertainty of measuring at the
housing. Two points 3 °C apart do not show the curve shape, so the provisional table is unchanged.
Open: a soaked point at ≥ 60 °C, or a comparison with a scan tool's oil temperature in a vehicle.

[I] Identification as fluid temperature rests on the NTC curve shape and range (−40 … 170 °C);
the link to DTCs P0710–P0714 has not yet been traced in code.

Most channels are read through an index-based abstraction rather than by direct address, so
their meanings need further tracing (or bench measurement).

## 4.2 AN0 — multiplexed high-rate acquisition

AN0 is not a single signal. It is the input of an **external 16-way analog multiplexer** whose
select lines are driven from port G by DMA, using the SH7254R's built-in "ADC input switch via
port" feature [M §12.3.2][O]:

| Element | Setting | Effect |
|---|---|---|
| Timer G2 | 4 MHz, compare 50 → period 51 ticks = **12.75 µs (78.4 kHz)**, DMA request enabled | each compare match: A-DMAC channel 0 copies the next byte of the sequence list to port G [M][O] |
| Timer G1 | same period, start offset −40 ticks, compare-match pulse output | triggers the AN0 conversion **10 µs after** the multiplexer address changes (settling) [O][D] |
| A-DMAC channel 1 | AN0 result (ADR0) → RAM, ring of 2 × 80 results | ping-pong buffers at `0xFFF885E8` and `0xFFF88688`; software reads the completed half [O] |
| Sequence list | 80 bytes in RAM (alias offset `0x44C`), copied from ROM at start-up and **verified periodically** against the ROM copy | one full sequence = 80 × 12.75 µs = **1.02 ms** [O][D] |

The select value is in the upper nibble of each byte (values `0x00`–`0xF0`) [O]. [I] It drives
the port G pins left as general-purpose outputs (PG12–PG15); PG0–PG11 are SPI chip selects
(spec 01).

Sampling schedule of the stock sequence (multiplexer input = byte >> 4) [O]:

| Multiplexer input | Samples per sequence | ≈ Rate |
|---|---|---|
| 13 | 16 | 15.7 kHz |
| 8, 10, 11, 14 | 8 each | 7.8 kHz |
| 7 | 5 | 4.9 kHz |
| 3, 5, 15 | 4 each | 3.9 kHz |
| 0, 2, 4, 6 | 3 each | 2.9 kHz |
| 1, 9, 12 | 1 each | 1 kHz |

### 4.2.1 Per-input processing (1 ms task)

Once per completed half-buffer (≈1 ms) a single task splits the 80 results by sequence slot.
Every read maps to exactly one multiplexer input, which confirms the model [O]:

| Mux input | Slots used | Processing | Output |
|---|---|---|---|
| 13 | 16 (every 5th) | mean; mean absolute deviation (ripple) plus change versus previous mean; running minimum | published signals [O]; [I] **supply voltage with ripple monitoring** |
| 11 | 8 | mean | published |
| 8 | 8 | mean | published |
| 10 | 8 | mean | published |
| 14 | 8 | mean; flag cleared if below 1000 mV | published |
| 15, 3, 5 | 4 each | accumulated for slower averaging | |
| 1, 9, 7 | 1 each | accumulated | |
| **0, 2, 4, 6** | 2 each | each sample classified against a threshold (raw `0x1168`, ≈340 mV at the pin) into a *high* or *low* accumulator; also accumulated as the four "additional" inputs (analog IDs 200–203, divider ratio 5 → 0–25 V) | [I] **voltages of PWM-switched output nodes**, measured separately in the on and off phases |
| 12 | — | not used by this task | |

Signal meanings beyond these interpretations remain **open**.

Correction to an earlier hypothesis: no evidence was found that the multiplexer carries solenoid
current. [I] Current regulation is most likely done inside the SPI driver chips (spec 03: two
16-bit setpoint writes per chip per cycle); the multiplexed inputs supply voltages and
output-node voltages for supervision.

### 4.2.2 Random-number source

A separate routine builds a 32-bit random value by XOR-ing the least significant conversion bit of
32 consecutive AN0 samples into the timer-A µs timestamp [O]. It is used by the diagnostic layer
[O]. A reimplementation needs an equivalent entropy source; this one depends on analog noise and
should be evaluated for quality before use in anything security-relevant.

A reimplementation must reproduce the timing relationship (address first, conversion after the
settling delay), keep the sequence list intact (it is a safety-relevant configuration), and
process each 80-sample half-buffer within one sequence period.

## 4.3 Analog channel identifiers used by the software

All analog values are read through one accessor that takes a channel identifier [O]:

| Identifier | Value returned |
|---|---|
| 0–99 | ADC0 channel ANn (mV, averaged, divider-corrected) |
| 100–199 | ADC1 channel AN(40 + id − 100) |
| 200–249 | the four additional accumulated inputs |
| `0xFD` | constant 3300 mV |
| `0xFF` | constant 5000 mV |

A separate routine converts several channels to **ratiometric values** (input × 10000 / supply
reference, i.e. 0.01 % units) and compares each against five calibrated thresholds to derive a
discrete position [O]. [I] This is the pattern used for resistor-coded switches (e.g. a selector
or mode switch).

## 4.4 Switch inputs with resistor-coded diagnosis

One routine evaluates up to **six binary switch inputs** [O]. Each input can be evaluated
through an analog channel, a digital port bit, or both; which inputs use which path is selected
by a coded **variant index**.

| Input | Analog channel | Digital bit | Analog evaluation enabled (variant 0 / 1 / 2) |
|---|---|---|---|
| 0 | AN3 | 0 | – / – / yes |
| 1 | AN4 | 1 | – / – / yes |
| 2 | AN5 | 2 | **yes** / **yes** / – |
| 3 | AN6 | 3 | **yes** / **yes** / – |
| 4 | AN9 | — | – |
| 5 | AN10 | — | – |

(All six channels have divider ratio 2.5, i.e. battery-level signals.) In variants 0 and 1 the
analog result of inputs 2 and 3 is also **cross-checked** against digital bits 2 and 3.

Analog evaluation (behavior to reproduce) [O]:

1. Only when the supply reference is at least **6000 mV**.
2. `ratio = input_mV × 10000 / supply_mV` (0.01 % units).
3. Band = number of band thresholds ≤ ratio, thresholds `100, 1900, 5000, 8000, 9500`
   (stock, same for every input):

   | Band | 0 | 1 | 2 | 3 | 4 | 5 |
   |---|---|---|---|---|---|---|
   | Ratio | < 1 % | 1–19 % | 19–50 % | 50–80 % | 80–95 % | ≥ 95 % |
   | Meaning | short to ground | **valid** | fault | **valid** | fault | short to supply |

4. Switch state = `ratio < 50 %` (active low).
5. Outputs: a 6-bit state word, a change mask, a fault mask (invalid band), and a mismatch mask
   (analog and digital disagree). Each state is also stored with its bitwise complement.

[I] These are switch contacts in a resistor network, designed so open circuits and shorts fall
into invalid bands. Which physical switches they are is **open**.

**Project implementation (M2)** [I]: `firmware/io/sensors/sensors.c` evaluates AN5/AN6 against the
supply (AN21) with the same band layout (provisional, to be re-derived from measured contact
levels) and reports Park when contact 0 is closed and contact 1 open, Not Park for the reverse,
unknown otherwise. Supply = AN21, switched supplies AN19/AN20 with 3.0/4.0 V hysteresis.
All channel assignments are [I] and listed for the bench (items 17, 22, 23).

## 5. Open items

0. [I] Candidate for the two-input switch evaluator (§4.4, AN5/AN6): the park position sensor's
   "Park" / "Not Park" pair (spec 01 §2.8), since no selector switches are wired to the TCM [M].

1. Physical meaning of each channel (fluid temperature, pressure sensors, solenoid supply,
   current sense, sensor supplies).
2. Conversion from millivolts to engineering units (e.g. NTC temperature curves) — expected in
   calibration tables.
3. Trigger source for AN0 and its relation to the PWM groups (spec 02).
4. Exact DMA transfer counts per trigger and the accumulation rate.
