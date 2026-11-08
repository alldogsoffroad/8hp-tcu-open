# 26 — Chip identification (top-down artifact 6)

> Fingerprint + 12-candidate strict match. VERDICT: part number UNRESOLVED (no public
> datasheet); family PROBABLE with high confidence — a coordinated Bosch in-house ASIC
> chipset: supervisor SSLB1 is the switch-output + watchdog + certification variant of
> the same SPI family as the four bus-A 2-channel current drivers. Generated 2026-11-08.

# ARTIFACT 6 — Chip identification (supervisor SSLB1, drivers SSLA3–SSLA0)

Bosch 1034420267 TCU, SH7254R, sealed unit (no board photos / no BOM). Mission: part-number
identification of the external supervisor chip (RSPI-B, chip select SSLB1) and, secondarily,
the four bus-A solenoid driver chips (SSLA3/2/1/0). A positive ID with a public datasheet
would hand over the real register map — in particular 0x14 bit 1 (spec 23's ranked #1
missing latch condition).

Markers: **[O]** observed in stock code/bench, **[M]** Renesas manual, **[L]** literature
(searched 2026-11-08, links inline), **[D]** decoded/derived.

Citations: `docs/spec/03-spi-devices.md` ("spec 03 §…"), `docs/spec/22-supervisor-protocol.md`
("spec 22 §…"), `docs/spec/23-rail-latch-conditions.md` ("spec 23"), `docs/spec/12-solenoid-outputs.md`
("spec 12"), decompile `~/tcu_sweep/decomp/<region>/0x<ADDR>.c`.

---

## 0. VERDICT (read first)

- **Part number: UNRESOLVED** — no public datasheet matches the observed SPI grammar
  (strict multi-fragment test; every era-plausible catalog family tested fails, §2).
- **Nature: PROBABLE (high confidence) — a Bosch in-house ASIC chipset**, one SPI-grammar
  family shared by the four bus-A 2-channel current drivers, the SSLB1 supervisor variant
  (switch outputs + watchdog + certification ladder), and the SSLB0 system chip. Bosch's own
  TCU documentation states the TCU is a "microcontroller plus application-specific integrated
  circuits (ASICs) optimized for specific applications" [L], and Bosch hybrid driver ASICs
  carry internal production codes (F00C…/Y-numbers) that are not publicly cross-referenced [L].
- **0x14 bit 1 therefore has no datasheet quote available.** Best evidence-based meaning
  remains: *commissioning-commit/lock status of the supervisor's certification state machine*
  (our hand-fed commit `0x1200` → `0xAAC2` sets bit 1 of the 0x12 commit word while 0x14
  still reads D5; stock's C7 carries it; the self-test phase-2 gate hard-fails without it —
  spec 03 §6.2ba, spec 23 §A1b). The settling evidence is the planned stock observation run
  (spec 03 §6.2be), not a datasheet.
- **What would settle the ID** (all routes concrete, §4): read the die markings under the
  gel on the spare electronics module (spec 01 §2.9 documents a spare unit exists on this
  bench), cross-reference via Bosch ESI[tronic] against the unit's 0 265… order number, or
  obtain markings photos from the VAG sibling TCU community (same Bosch board family,
  spec 01 §1).

---

## 1. THE ASSEMBLED FINGERPRINT (what any candidate must match)

### 1.1 SPI electricals and framing [O/M]

| Property | Value | Citation |
|---|---|---|
| Bus / select | supervisor RSPI-B (`0xFFFFB800`) SSLB1; drivers RSPI-A (`0xFFFFB000`) SSLA0–3 (+dead SSLA5) | spec 03 §1; spec 22 §1 |
| SPCMD (supervisor, boot+app) | `0xAF11` — decode [M §17.3.12, /tmp/sh7254r_man.txt]: SCKDEN=1 (RSPCK delay per SPCKD), SLNDEN=0, SPNDEN=1 (next-access delay per SPND), LSBF=0 (MSB first), SPB=1111 (**16-bit frame**), SSLKP=0 (select negated between frames), SSLA=001 (this unit's SSL1), BRDV=00 (base rate), **CPOL=0, CPHA=1 (SPI mode 1)** | `decomp/r00_boot/0x00002290.c` writes `0xFFFFB810 = 0xAF11`; spec 03 §4.4 |
| Bus-A five-select SPCMD | {`AF31, AF21, AF11, AF01, AF51`} — chips SSLA3/2/1/0/5, frame i → SPCMD[i mod 5]; identical mode-1/16-bit/MSB format | spec 22 §2 (table `0x40CE4`); spec 03 §6.2aw |
| SSLB2 exception | SPCMD `0xAF20` → **CPHA=0 (mode 0)**, 16-bit | spec 22 §1 |
| Bit rate | SPBR=5, BRDV=0 → Pφ/12 = **1.667 MHz** (Pφ = 20 MHz); boot flash mode halves to SPBR=0xB (833 kHz) | `decomp/r00_boot/0x00002290.c` (SPBR cell `0xFFFFB80A`=5); spec 05 §1.3, spec 03 §4.4 |
| Idle polarity / SPCR | SPCR=0x48 CPU mode (SPE+MSTR); app live idle SPCR=0x08, armed per transaction | spec 03 §4.4, §6.2ak/ar |
| Frame grammar (chipset-wide) | 16 bits = `[command byte][data byte]`; **bit 7 of the command byte = write**; bit 7 clear = read; data OR-ed into low byte (or low 11 bits for setpoints) | spec 03 §4; spec 12 §2b |
| Service cadence | supervisor control word every **5 ms** (boot programs AND app task `0x8EE82`); driver cyclic 45 frames per **10 ms** current-control loop | spec 03 §4.4; spec 22 §2.2; spec 12 §2 |

### 1.2 Register-map fragments actually observed (the match target set)

Supervisor SSLB1 (bench live values in brackets are ours; stock engaged values noted):

| Addr | Access | Observed meaning | Citation |
|---|---|---|---|
| `0x06` | R | identification read; reply header 0xA8/0xAA-class, data `0x14` [ours]; second ID word `0x00` → data `0xBC` | spec 03 §4.4; handler `0x92AC0` spec 22 §4.1 |
| `0x8E` | W | **control word**: bit0 output-enable request, bit1 0x90-composer bit, bit2 life-sign toggle (normal dialect), bit4 life-sign toggle (engaged), bit6 power-down (mode 6; bench: 0.169→0.018 A latched), bit7 engaged-dialect marker | builder `0x91C86`; spec 22 §3 |
| `0x22` | R | **echo of the 0x8E control byte** + supervisor's own bits: bit0 = grant (feeds `0xFFF90954`), bit5 = rail/VS telemetry (excluded from the stock's echo match mask 0xDF) | handler `0x92D94`; spec 22 §4.4 |
| `0x90` | W | **switch-output enables**: bits 5/6/7 = outputs 0/1/2 (bit5 = output 0 = the solenoid power rail; letters 'c' from classifier `0x8C7A8`); data composed `0x91758`, fault-masked `~(0x14 & 0xE0)` before send | spec 22 §4.5; spec 23 §A1 |
| `0x24` | R | **readback of 0x90 / certification data** (stock engaged: `0xAA20`; ours `0xAA00`; SSLB0 misaddressed: `0xAAFF`); write protocol: `{90|value, 24}` then cyclic; stock compare mask 0x15 (bits 0/2/4) | handlers `0x92ED4`/`0x92FD0`; spec 22 §4.5, §6.2ax |
| `0x14` | R | **switch-output status**: bits 5–7 = per-output fault flags (any set → fault-path re-send job 0x40C30); bit1 = commit/lock (stock C7 vs our D5); bit0 → boot handover readiness; bit4 open | handler `0x92BBE`; spec 22 §4.2; spec 23 §E |
| `0x84` | W | **PWM level register** — the only latching write pre-certification; values 0x3F/0x06/0x1F; job `{84|lvl, 0A, 12, 0C}` cyclic at timer cadence, dwell `value*200+2400` ticks | builder `0x91190`; spec 22 §2.1/W2 |
| `0x0A` | R | commanded-level echo (exact echo: 6→AA06, 0x1F→AA1F) | spec 22 §4.8 |
| `0x12` | R | **commit status** (bit0 challenge-rotation held, bit1 the "commit accepted" bit — hand-fed commit answers AAC2); checked `(0x12 & 5) == phase-mask` per reply | spec 22 §4.8; spec 03 §6.2ba |
| `0x0C` | R | feedback: low nibble = challenge selector (rotating), bits 4–6 = achieved level (0–7) | spec 22 §4.8 |
| `0x88` | W×4 | rolling challenge-response tokens (4 bytes OR'd onto four 0x88 template words; `token = ~(xor_table[sel] ^ 0xAAAA ^ 0xB32F8A6A)`, 16-entry table @0x4526C) | spec 22 §5 |
| `0x28`/`0x2E` | R | telemetry: 10-bit value `(rx1&0x3F)|((rx0&0xF)<<6)` (stock live 373–380) + valid flag (rx0&0x40); 0x2E drifting counter (0x1F–0x25) | handler `0x93096`; spec 22 §4.6 |
| `0x18`/`0x1E`/`0x06`/`0x00` | R | status words (ours: 0x80 / 0x0A / 0x14 / 0xBC) | spec 03 §4.4 |

Driver chips (bus A, ×4 + dead SSLA5):

| Addr | Access | Observed meaning | Citation |
|---|---|---|---|
| `0x06`/`0x00` | R | ID read, **reply high byte must be `0xA2`** (code check `(rx & 0xFF00) == 0xA200`); ID byte `0xBD` selects a chip revision handled specially | `decomp/r08_app/0x000921E0.c`; spec 12 §2b |
| `0x88` | W | channel mode bits: ch0 bits 1..0, ch1 bits 3..2 (0 disabled/1 off/2 current control/3 full-on), bit4 calib flag, bit5 reduced-frequency mode | spec 12 §2b; builder `0x924B0` |
| `0x8E` | W | PWM/dither frequency divider `32000/f − 1` (7-bit) + calib bit7 (stock 0x9F = 159 ⇒ f = 32000/160 = **200 Hz** per the formula reading [D]) | spec 22 §2.2; spec 12 §2b |
| `0x90`/`0x96` | W | configuration bytes; bits 7..6 = readback range (stock 0x21 / 0x55) | spec 12 §2b |
| `0xA0`/`0xA6` | W | per-channel **dither amplitude** | spec 12 §2b |
| `0xE8`/`0xF0` | W | per-channel **11-bit current setpoint** (full scale 2047 = 0x2FFA current units ≈ 1.23 A; step ≈ 0.6 mA [I]) | spec 12 §2b |
| `0x60`/`0x66`/`0xC0`/`0xD8` | R | same-cycle readbacks of dither/setpoint words | spec 12 §2b |
| `0x18`/`0x1E`/`0x28` | R | ch0/ch1/chip diagnostic status (bench driver status 0x28 → `0xA240`) | spec 12 §2b; spec 03 §6.2az |

### 1.3 Reply-header classes (the chipset convention) [O]

| Chip | Class check | Bench headers |
|---|---|---|
| Drivers (bus A) | high byte `0xA2` (ID/diag/config); cyclic replies top-5-bits `10100` (0xA0-class) | `0xA2xx` |
| Supervisor SSLB1 | `(rx & 0xF900) == 0xA800` — 0xA8xx/0xAAxx pass, 0xABxx fails | `0xAAxx` |
| SSLB0 | `(rx & 0xFE00) == 0xAA00` — AAxx/ABxx pass | `0xAAxx` (ID data 0xD0) |
| SSLB2 | no header check; answers everything with a rolling counter | `0x0ECx–0x0F6x`, +4/job |

All family members except SSLB2 answer with top nibble `1010` — one vendor's framing
convention across at least five silicon parts.

### 1.4 Behavioral fingerprint (supervisor) [O]

1. **Watchdog with reset authority**: a stalled 5 ms mirror stream resets the MCU (E27/E28:
   reset at the stall instant with the internal WDT disarmed; threshold bracketed ~11–20 ms;
   E59's 20 ms guard loop alone could trip it) — spec 22 NR-5, spec 03 §6.2aw/ar.
2. **Bit-6 power-down** latches the unit's whole draw down 0.169→0.018 A (bench 2026-10-30)
   — spec 03 §4.4.
3. **The life sign must alternate** (bit 2 toggle normal / bit 4 engaged); a constant word
   resets in a loop — spec 03 §4.4, spec 22 NR-4.
4. **Owns the solenoid power rail as switch output 0** (0x90 bit5; the rail is never commanded
   via a direct wire — its state is visible only as 0x22 echo bit 5) — spec 22 §6.2ax/ay, NR-8.
5. **Certification/commissioning state machine**: rolling challenge-response tokens (16-entry
   XOR table, selector rotating under the chip's control), a level ladder 0x3F→0x06→0x1F with
   echo + commit-mask verification per reply, per-output "letter" bits, and the 0x24
   certification byte — anti-counterfeit sequencing unknown in any catalog part — spec 22 §5–6.
6. MCU **reset authority** (never-rules violated → reset loops, E16/E42/E60m/n classes);
   READY law: ignores bit-0 asks ~100–200 passes after each restart, then grants — spec 22 §8.
7. ID data bytes `0xBC` (supervisor) vs `0xBD` (one driver revision) — adjacent die/mask
   revision codes, consistent with one ASIC family's ID scheme [I].

### 1.5 Topology [O]

RSPI-A: 4 × 2-channel current drivers (0xA2 class) + dead SSLA5 position (variant slot; the
bus-A five-select always walks 5 selects). RSPI-B: supervisor SSLB1 + system chip SSLB0
(0xD0 ID, C9 mirror register, inverted 4-bit status nibble) + SSLB2 rolling-counter chip
(mode 0). 8 solenoid channels = EDS A/B/C/D/E + WK + EDS-Sys + MV-Pos (spec 12 §1);
supervisor output 0 = shared 12 V EDS rail, outputs 1/2 unfitted on the bench hybrid.
FWE raise/drop is done by circuitry inside the ceramic hybrid "supervisor die or discrete"
(spec 01 §2.8 FWE note).

---

## 2. CANDIDATES TESTED (strict match: register addresses AND bit meanings AND write/commit
protocol AND watchdog behavior; a name/functional similarity alone does not count)

| # | Candidate (family, era) | Links | Matches | Mismatches (decisive) | Verdict |
|---|---|---|---|---|---|
| 1 | **Infineon TLE7241E** — dual-ch constant-current solenoid driver, SPI, 2006-2009 (functionally the closest era-catalog part to our drivers) | [product](https://www.infineon.com/part/TLE7241E), [DS PDF](https://media.digikey.com/pdf/Data%20Sheets/Infineon%20PDFs/TLE7241E.pdf) (fetched, §5.6.2 checked) | 16-bit SPI; 2 channels; dither; setpoint+status echo; transmission-marketed | MOSI = B15 channel + B14-13 instruction code (**no register-address byte, no 0x80-write convention, no 0x06 ID, no 0x8E/0x90/0x96/0xA0/0xE8/0xF0 addresses**); **10-bit** setpoint (not 11); external sense resistor + freewheel diode + DEFAULT pin; reply header not 0xA2-class; no commit word | **REJECT** |
| 2 | **Infineon TLE7242-2G** — 4-ch constant-current pre-driver | [DS](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle7242-2g-ds-en.pdf) (fetched) | constant-current, dither, transmission use | **32-bit SPI** (channel number in bits 25-24); **pre-driver** (external N-FETs + POS/NEG sense pins); 4 channels; no shared addresses/headers | **REJECT** |
| 3 | **Infineon TLE7244SL (SPIDER)** | [product](https://www.infineon.com) | 16-bit SPI | It is an **8-channel relay/low-side switch** (DS fetched: "SPI Driver for Enhanced Relay Control"), not a current driver; daisy-chain shift-register protocol | **REJECT** |
| 4 | **Infineon TLE92464ED / TLE92466ED "NextGen"** 4-ch solenoid driver (2016+) | [DS](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92464ed-datasheet-en.pdf) (fetched) | function set (current control + dither + SPI watchdog) is the modern descendant of our driver role | **32-bit SPI with 8-bit CRC**; 15-bit setpoint; integrated 140 mΩ shunt; wrong era (post-2015) | **REJECT** (closest modern relative for the driver role) |
| 5 | **Infineon TLE82453-x** 3-ch low/high-side current-control IC | [product](https://www.infineon.com/part/TLE82453-3SA), [DS](https://www.mouser.com/datasheet/2/196/Infineon-TLE82453-DS-v01_00-en-540145.pdf), [register-map KB](https://community.infineon.com/t5/Knowledge-Base-Articles/TLE82453-SA-IC-SPI-register-address-mapping-details/ta-p/1232460) | 16-bit SPI, current control, transmission use | 3 channels; different register map (public register-address tables share no observed fragment); later era | **REJECT** |
| 6 | **ST L9305** 4-ch valve driver | [product](https://www.st.com/en/automotive-linear-and-logic-ics/l9305.html) | transmission/EDS target, dither, SPI | 4 channels; modern (2016+); SPI framing/register map unrelated to the observed grammar | **REJECT** |
| 7 | **NXP/Freescale MC3390x SBC family** (MC33903/905/907/908, 2008+) | NXP product pages | era-correct "SBC" role match for the supervisor: watchdog + supply + low/high-side outputs + 16-bit SPI | Public register maps are mode/voltage/status registers, not a per-output enable register with classifier letters, no `{90|value,24}` write+readback protocol, no 0x14-style per-output fault register, **no challenge-response commissioning ladder anywhere in the family**; grammar would not be shared with the four driver chips | **REJECT** (closest catalog ROLE relative for SSLB1) |
| 8 | **NXP MC33800** engine control IC (2 constant-current drivers + dither, 16-bit SPI) | NXP | 2-ch current control w/ dither via 16-bit SPI | single engine IC (ignition/lambda architecture), not a chipset; not used 5×; no shared grammar evidence | **REJECT** |
| 9 | **Infineon TLE7230R** octal low-side switch w/ SPI | [DS](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle7230r-datasheet-en.pdf) (fetched) | multi-channel switch outputs via SPI (supervisor's output stage concept) | daisy-chain shift register, no register-addressed map, no watchdog (DS checked), not current-controlled | **REJECT** (conceptual relative for the switch-output half) |
| 10 | **Elmos E523.x0 family** (E523.30B–38B etc.) | [DS](https://www.mouser.com/datasheet/2/594/e523-30b-38b_elmos_ds-270834.pdf), [E523.39](https://www.elmos.com/english/products/motor-control-ics/stepper-motor-controller-ic/e52339-driver-ic.html) | German Tier-1 valve/motor ASIC house; SPI variants exist | family is full-bridge motor / stepper / LIN-PWM valve controllers with own CPU (8-bit core, firmware); no 2-ch × 5-chip transmission chipset with our grammar | **REJECT** |
| 11 | **Melexis** (MLX10407/10410, MLX81xxx) | [MLX10410](https://www.alldatasheet.com/datasheet-pdf/pdf/224151/MELEXIS/MLX10410.html) | automotive serial-link drivers | no transmission multi-channel constant-current family exists in the public catalog; grammar unrelated | **REJECT** |
| 12 | **Bosch in-house transmission ASIC chipset** | [Bosch TCU page ("microcontroller plus ASICs")](https://www.bosch-mobility.com/en/solutions/control-units/transmission-control-unit), [Bosch Semiconductors automotive ICs](https://www.bosch-semiconductors.com/products/automotive-ics), [EEVblog thread on identifying Bosch driver ICs](https://www.eevblog.com/forum/beginners/help-identifying-bosch-driver-ics/) | Explains ALL of: one grammar across 5+ chips (§1.3); ID revision codes 0xBC/0xBD; challenge-response commissioning (anti-tamper, OEM-specific); ceramic-hybrid construction with internal part markings; era (2007 design) predating mature catalog SBCs | no public datasheet exists → cannot satisfy the "datasheet register map" test by definition | **PROBABLE** (see §0) |

Teardown-literature search (8HP TCU / mechatronic / SH7254 ECU): no chip-level teardown of the
Bosch 8HP ceramic hybrid is publicly indexed (openinverter 8HP threads
([t=6047](https://openinverter.org/forum/viewtopic.php?t=6047) and the bootloader thread),
[Turbolamik](https://turbolamik.com), [RM-Primis](https://rmracingelectronics.com),
[damienmaguire/8HP-TCU](https://github.com/damienmaguire/8HP-TCU),
[dspl1236/vag-tcu-tools](https://github.com/dspl1236/vag-tcu-tools) all work at
firmware/connector level or replace the board; a Facebook "Electronic Parts" group post
"Insides of an 8-speed gearbox control unit (TCU) from ZF" exists but is login-walled and
unconfirmed at chip level).

---

## 3. Consequences for the open questions

1. **No public register map will arrive from this route.** The 0x14-bit-1 semantics must be
   settled empirically: the ranked #1 missing condition (spec 23 §2) is already best attacked
   by the stock observation run (restore stock, read `0xFFF90551/0xFFF907D7/0xFFF90944/
   0xFFF93BA1` live through the first 15 s — spec 03 §6.2be), which directly observes the
   stock's own commissioning timeline instead of a datasheet.
2. Our firmware-side 0x14 decode stands as the working truth: bits 5–7 per-output faults
   (E0 mask), bit1 commit/lock (selftest phase-2 gate `0x8BB34:13`, mode-'L' entry
   `0x85160:126-131`, speed interlock `0x85160:23`), bit0 handover-readiness (`0x7291A:12`),
   bit4 open (set for us, clear for stock) — spec 23 §E.
3. 0x90/0x24: the `{90|value}` write + `{24}` readback + 0x15-mask compare is fully decoded
   (spec 22 §4.5); the chip-internal latch law for 0x90 bit 5 (what flips 0x24 from 0x00 to
   0x20) is chip-internal and is candidates #1/#2 of spec 23 — again only the observation run
   or the markings route can close it.

---

## 4. VERDICT and the settling evidence

**VERDICT: UNRESOLVED (part number) / PROBABLE Bosch in-house ASIC chipset (family), confidence
high on the family classification, zero public datasheet.**

Distinguishing predictions of the Bosch-ASIC reading (falsifiable): (a) die markings on the
hybrid are Bosch-internal codes (F00C…/Y-number/date code), not Infineon/ST/NXP/Elmos logos;
(b) the four driver dies and the supervisor die share one mask family (adjacent revision bytes
0xBD/0xBC already hint this); (c) no ESI[tronic] public cross-reference will exist.

Exactly what evidence would settle it, in order of cost:

1. **Photograph the spare module's hybrid under the gel** (the spare electronics module on
   this bench, spec 01 §2.9, is the same board family): oblique-light macro of each die
   marking. Markings survive under silicone gel; Bosch hybrids typically laser-mark each
   die (internal number + 4-digit date). 15 minutes of bench time; zero risk to the unit
   used for experiments if done on the spare.
2. **Bosch ESI[tronic] cross-reference**: take the unit's Bosch order number (0 265 …, the
   10-digit form of 1034420267-family numbers on the label) to Bosch Electronic Service /
   a workshop with SIS+ESIS; hybrids list their component set in the circuit-documentation
   layer (NDA-tier but often visible to service partners).
3. **VAG sibling community**: the VAG 8HP TCUs run the same Bosch platform (SH72549,
   `dspl1236/vag-tcu-tools`); request/locate a markings photo of that board — same ASIC set.
4. **If markings are ground off** (production ASICs on hybrids sometimes are): X-ray the
   die count/placement, or decap one driver die from the spare and read the mask ROM ID —
   last resort, destructive.

Chip-marking readback is the only route that can produce a part number; everything else in
this artifact is already settled.

---

## 5. Sources (literature)

- Bosch Mobility, Transmission Control Unit — "microcontroller plus ASICs":
  https://www.bosch-mobility.com/en/solutions/control-units/transmission-control-unit
- Bosch Semiconductors, automotive ICs: https://www.bosch-semiconductors.com/products/automotive-ics
- EEVblog, "Help identifying Bosch driver ICs" (internal marking schemes):
  https://www.eevblog.com/forum/beginners/help-identifying-bosch-driver-ics/
- Infineon TLE7241E product + datasheet:
  https://www.infineon.com/part/TLE7241E ,
  https://media.digikey.com/pdf/Data%20Sheets/Infineon%20PDFs/TLE7241E.pdf
- Infineon TLE7242-2G datasheet:
  https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle7242-2g-ds-en.pdf
- Infineon TLE7230R datasheet:
  https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle7230r-datasheet-en.pdf
- Infineon TLE92464ED datasheet:
  https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92464ed-datasheet-en.pdf
- Infineon TLE82453 product/DS/register KB:
  https://www.infineon.com/part/TLE82453-3SA ,
  https://www.mouser.com/datasheet/2/196/Infineon-TLE82453-DS-v01_00-en-540145.pdf ,
  https://community.infineon.com/t5/Knowledge-Base-Articles/TLE82453-SA-IC-SPI-register-address-mapping-details/ta-p/1232460
- ST L9305: https://www.st.com/en/automotive-linear-and-logic-ics/l9305.html
- Elmos E523.30B–38B DS: https://www.mouser.com/datasheet/2/594/e523-30b-38b_elmos_ds-270834.pdf
- Melexis MLX10410: https://www.alldatasheet.com/datasheet-pdf/pdf/224151/MELEXIS/MLX10410.html
- Community (no chip-level teardowns found): https://openinverter.org/forum/viewtopic.php?t=6047 ,
  https://github.com/damienmaguire/8HP-TCU , https://github.com/dspl1236/vag-tcu-tools ,
  https://turbolamik.com , https://rmracingelectronics.com
- Renesas SH7254R Group hardware manual Rev 4.00 (R01UH0480EJ0400) §17.3.12 SPCMD — local
  copy /tmp/sh7254r_man.txt.
