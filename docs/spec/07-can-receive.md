# Spec 07 — CAN receive interface (signals the TCU reads)

Status: **draft.** Message handling, integrity protection, timing and the bit layout of 241
received signals are established; signal **meanings and scaling are open**.
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference, **[D]** derived.

Data files (project root):

- `1034420267_can_rx_messages.csv` — one row per received message: bus, DLC, period/timeout,
  CRC/counter protection, number of decoded signals.
- `1034420267_can_rx_signals.csv` — one row per signal: message, byte, bit position, length,
  getter address in the stock firmware.

Bus numbering follows spec 01: **bus 1** = RCAN channels A+B (shared pins PJ0/PJ1), **bus 2** =
channel C. All 59 application receive messages are on bus 1; bus 2 carries the diagnostic and
request/response traffic (spec in the CAN message list).

[M] FCA documentation (spec 01 §2.8) shows the TCM on **CAN-C** and on a **dedicated CAN bus to
the electronic shifter module**; the shifter messages on that bus duplicate those on CAN-C.
[O] Confirmed in firmware: the lever position is read redundantly from **`0x2EA` (bus 1) and
`0x210` (bus 2)** on this unit (shifter type 1), or from `0x2FA` (bus 1) and `0x200` (bus 2) on
shifter type 0; details in spec 14 §5a. Bus 2 is the dedicated shifter bus as well as the
diagnostic path; transmit `0x300` on bus 2 is [I] the TCM's message to the shifter. Which TCM pin pair is bus 1 and which is bus 2 must be confirmed on the bench. For the
Nissan swap the shifter messages will have to be emulated (later work).
[O bench 2026-11-24] A JL Wrangler shifter (P68268664AG) on its own: the shifter bus is 500 kbit/s,
the shifter sends only `0x210` (20 ms) and only while it receives `0x300` from the TCU; layout and
lever codes in spec 14 §5a.

## 1. Receive path (behavior to reproduce)

1. A mailbox receives the frame and the driver copies it into a per-message RAM buffer [O].
2. A table of **59 message objects** (0x50 bytes each) describes every received message:
   handle, expected period, timeout, DLC, buffer pointers, optional integrity checks and
   per-message calibration enables [O].
3. If the message is protected (§2), the checksum and the rolling counter are verified.
4. The validated data is copied into an application copy (8 bytes); application code reads
   signals only from that copy, through one small getter per signal, with interrupts briefly
   disabled for multi-byte values so the two bytes belong to the same frame [O].
5. Missing messages are detected against the timeout [O]; the reaction (substitute values,
   fault codes) is per signal and **open**.

Byte order: multi-byte signals are **big-endian** (most significant byte first, Motorola
layout) [O].

## 2. Integrity protection (16 of 59 messages)

Protected messages: `0x037 0x108 0x10C 0x10D 0x11C 0x120 0x140 0x14B 0x161 0x170 0x29D 0x2A4
0x2EA 0x2F8 0x2F9 0x2FA` (see the message CSV, columns `crc8…`/`counter…`).

~~A coded **protocol variant** (1 or 2) selects the exact form [O]:~~ (corrected below)

| Item | Variant 1 | Variant 2 |
|---|---|---|
| Checksum location | last data byte (`DLC − 1`) | last data byte |
| Checksum | CRC-8 over bytes `0 … DLC−2` | CRC-8 over the **inverted** bytes `0 … DLC−2`, result compared **inverted** |
| Counter location | upper nibble of byte `DLC − 2` | same |
| Counter rule | each new value = previous + n, 1 ≤ n ≤ calibrated maximum (mod 16) | counts **down** |

CRC-8 definition [O][D] — the SAE J1850 CRC-8 (verified by regenerating the 256-entry table from
the polynomial):

```
poly = 0x1D, init = 0xFF, no reflection, final XOR = 0xFF
crc = 0xFF
for each byte b: crc = TABLE[crc ^ b]
checksum = crc ^ 0xFF
```

A gateway or replacement node feeding this TCU must generate both fields exactly, or the
messages are rejected.

**Correction (2026-11-17, source: `~/tcu_sweep/can_inputs_decode.md` §1, §2, §9 items 2 and 10; applied 2026-11-18):**
- **There is one protection form on the wire (the "Variant 1" column).** "Variant 2" is not a coded
  alternative: it is the stock re-checking the *inverted image* of the same frame for its safety path
  (E2E byte, inverted copy), run on every protected frame [O 0x6EC80 structure; I purpose].
- **Counter step:** 1 ≤ n ≤ **3** (cal 0x1918FE = 03, via table 0x664D8[0xFFF90AB8]); a repeat counts as an
  error; the first frame is accepted [O].
- **`0x210` (bus 2, shifter) is protected too** — CRC-8 + counter (bus-2 handle 0x44, enable 0x3B) [O; the
  handle → ID assignment 0x44 = 0x210 is I strong]. It is not in the 16-message list above (that list is
  bus 1).

## 3. Timing

Each message object carries an expected period and a timeout (≈ 2 × period) in raw units:
1, 2, 4, 5, 6, 10, 50, 100, 200 (timeouts 5 … 400) [O]. On the transmit side the same raw unit
was measured as **15.625 ms** (spec 08 §2, bench 2026-10-17); [I] the receive table uses the same
unit, giving 15.6 ms … 3.1 s periods. The project profile still uses 10 ms × raw timeouts
(shorter, i.e. stricter); to be revised with a Ram capture.

**Correction (2026-11-17, source: `~/tcu_sweep/can_inputs_decode.md` §9 item 3; applied 2026-11-18):** ~~[I] the receive table
uses the same unit (15.625 ms)~~ — **the RX raw timeout unit is 10 ms** [O]: the RX scan 0x6E83C sits in the
row-6/10 ms task lists of modes 0–3 (0x42D54, 0x42F1C, 0x43120, 0x43324 inside objects
0x42D6C/0x42F44/0x43148/0x4334C). The 15.625 ms hypothesis does not apply to RX. Timeouts: 0x108 / 0x112 /
0x120 / 0x140 / 0x14B / 0x2EA / 0x2F8 / 0x210 = 50 ms, 0x180 = 200 ms, 0x334 = 2000 ms — the project profile's
values are the stock's.

## 4. Diagnostic read-back of received signals

157 of the 241 signals have a **signal identifier** (e.g. `0x5008` = the 16-bit value at bytes 0–1
of `0x108`) handled by a central dispatcher [O]. The dispatcher is reached from the UDS
ReadDataByIdentifier handler (service `0x22`) through a generic read-by-identifier service, and
from WriteDataByIdentifier (`0x2E`) [O]. [I] A scan tool can therefore read the TCU's received
value of each signal live, using the identifier as the DID (session or security requirements per
identifier are **open**). The identifiers are listed in the `diag_id_uds22` column of the signal
CSV. This gives a direct validation route: replay or capture a Ram's CAN traffic into a bench TCU
and read the identifiers back.

The transmission application itself collects its inputs through a separate input-gathering
routine [O]; tracing that routine's consumers is the static route to signal meanings.

## 5. Observations

- `0x037`, `0x2F9` and `0x170` are messages the TCU itself **transmits**; it also receives them
  back with CRC and counter checks but no application reader [O]. [I] Own-transmission
  monitoring.
  **Conflict:** the communication monitor (spec 13 §2c) assigns `0x037` and `0x170` to the ABS
  node and `0x2F9` to the steering-column node. To be settled with a bus capture.
  **Correction (2026-11-17, source: `~/tcu_sweep/can_inputs_decode.md` §9 item 1; applied 2026-11-18): settled — they are
  transmitted.** 0x037, 0x2F9 and 0x170 are transmit objects (handles 4/3/6 of the TX list 0x44120); their
  receive echoes are coded out and they are **not** monitored. Spec 13 §2c's entry → message map was wrong
  (corrected there) [O].
- **Sender of each received message:** spec 13 §2c lists, for 53 messages, the node whose
  communication fault (U-code) is set when the message is missing or invalid.
- Signal **scaling is not applied in the getters**; raw values are returned and scaled by the
  consumers [O].

## 6. Leads from public sources (unconfirmed)

Names from comma.ai opendbc (Chrysler definitions) for IDs that appear in our receive table.
They are **leads only**: opendbc describes other FCA vehicles, and nothing here has been checked
against this TCU or a Ram capture.

| ID | opendbc name | Note |
|---|---|---|
| `0x101` | ACCEL_RELATED_101 | DLC 5 in both; "energy or rpm" at byte 3 |
| `0x134` | ACCEL_GAS_134 | accelerator pedal in byte 5 (0–127) |
| `0x14B`, `0x14C` | brake / steering related | — |
| `0x2E0` | TRIP | — |
| `0x2E2` | BRAKE_2 | — |
| `0x2EA` | GEAR | confirmed role: shifter lever position, byte 0 bits 2..0 (spec 14 §5a) |
| `0x2FA` | CRUISE_BUTTONS | — |

opendbc lists `0x144` (SPEED_2) as a message received by other modules; in this TCU `0x144` is
**transmitted** (spec 08). Newer Ram models (2019+) use about 12-bit engine torque request and
actual-torque fields on a different ID scheme, similar in size to the 13-bit fields in
`0x037`/`0x2F9`.

## 7. Input signal path inside the firmware [O]

Received signals reach the transmission application in **three layers**; the application never
calls the CAN getters itself:

1. **CAN getters** (`0x76xxx–0x77xxx`, one per signal, listed in the signal CSV).
2. **Safety layer.** Gatherers `0x14FC5C` and `0x1517E0` copy about 106 signals into the safety
   layer's RAM (`0xFFF8E7xx–0xFFF8EFxx`, `0xFFF953xx–0xFFF958xx`). Conditioning functions then
   produce validated values: typical pattern (`0x13BB78`): raw `0xFFFF` or a set status byte
   means invalid → substitute value (another signal or the last value) → clamp; a quality byte
   (0 = OK, 1 = substituted, 2 = default) is kept next to the value. Safety-layer getters
   (`0x13xxxx–0x14xxxx`) expose the results.
3. **Application.** `0xB912C` / `0xB9646` copy 76 safety-layer values into the application
   input object `0xFFF89C3C`; `0xB986C` converts them (for example `+0x94` → engine speed
   `+0xFC`, `+0x92` → turbine speed `+0xFA`, `+0x90` → output speed `+0xF8`), and the speed block
   `0xFFF8B5FC` (spec 09 §3) is derived from those.

So the meaning of a CAN signal is found by following it through the safety-layer conditioning,
which costs several steps per signal. The function `0x144108` is the safety layer's
**ReadDataByIdentifier data handler**: it packs gathered values into identifier responses with
explicit scaling. For example identifier `0x51F` sends engine speed ×4 (0.25 rpm steps,
saturating at 16,383 rpm), and `0x515` converts the lever code 1–4 to the range codes 8–5 (spec 14).

### 7.1 Signals traced so far

| Signal | CAN source | Path | Scaling |
|---|---|---|---|
| Engine speed | `0x108` bytes 0–1 (diag ids `0x5008`, `0xF40C` = OBD PID 0x0C) | `0xFFF8E908` → conditioned `0xFFF8E90E` (getter `0x13BF24`) → app `+0x2C` → `+0x94` → `+0xFC` → `0xFFF8B5FC+0` | [I] 1 rpm/bit (identifier `0x51F` multiplies by 4 to give 0.25 rpm steps) |
| Output speed | none: speed sensor | `0xFFF8EE6E` (getter `0x14BB90`) → app `+0x2A` → `+0x90` → `+0xF8` | rpm |
| Turbine speed | none: speed sensor | `0xFFF8EF06` (getter `0x14C53C`) → app `+0x2E` → `+0x92` → `+0xFA` | rpm |
| Lever position | `0x2EA` / bus-2 `0x210` (this unit) | spec 14 §5a | code 1–4 = P/R/N/D |
| Accelerator pedal [I] | `0x120` byte 2 (diag id `0x5017`) | gathered `0xFFF9545C` (status `0xFFF9545D`) → `0x13CA22` → `0xFFF9545E` (getter `0x13C99C`) → app `+0x28` → `+0x9C` → `+0x100` (float %) | raw 0–250 = 0–100 % (0.4 %/bit); `0xFF` = invalid → calibrated substitute; conditioned value rescaled to 0–254 (`raw × 127 / 125`). Read by the converter/lockup and shift-execution code (pedal axis 0–255) |
| Transmission fluid temperature | none: internal NTC (spec 04 §4.1) | ~~1/16 °C → `0xFFF94101` = °C + 40 → `0xFFF8E9C8` (getter `0x140050`) → app `+0x1B` → `+0x96` → `+0xF0` (float °C)~~ **corrected 2026-11-17, see the note below the table:** 1/16 °C `0xFFF94F98` → `0xFFF94102` = round(/16) + 40 → `0x940E6` → `0xFFF8E9C5` → `0xFFF8E9C8` (getter `0x140050`) → app `+0x1B` and `+0x96` (both from `0xB9646`, 30 ms) → `0x89C13` | byte − 40 = °C (same +40 offset as the pressure-map temperature axis, spec 11 §2.1) |
| Engine temperature [I] (coolant) | `0x180` byte 1 | gathered `0xFFF9548C` → `0x13BABA` → `0xFFF9548E` (getter `0x13BAA8`) → app `+0x1C` → `+0x97` → `+0x104` (float °C) | CAN byte − 40 = °C; stored as byte − 48 (conditioning adds 8); `0xFF` invalid → substitute = transmission fluid temperature |
| Engine torque T1 [I] (actual) | `0x108` bytes 2–3 (13 bit) | gathered `0xFFF8E8D2` → `0x1386E4` → torque `0xFFF8E8DE`, torque minus correction `0xFFF8E8D8` = `0xFFF8E8DC` (getter `0x138698`) → app `+0x22` → `+0x6E` → `+0x108` (×10) | Nm = raw × 0.25 − 500; `0x1FFF` = invalid |
| Engine torque T2 | `0x2F8` bytes 0–1 (13 bit) | gathered `0xFFF8E8EA` → `0x1389E4` (filtered) → `0xFFF8E8F2` (getter `0x1389DC`) → app `+0x24` → `+0x78` → `+0x10A` (×10) | same encoding |
| Engine torque T3 | `0x2F8` bytes 2–3 (13 bit) | gathered `0xFFF8EB70` → (`0x13C51A`) → `0xFFF8EB80` (getter `0x13C384`) → app `+0x20` → `+0x6C` → `+0xFE` (×10) | same encoding |
| Speed for park interlock [I] (vehicle speed) | `0x14B` bytes 0–1 (11 bit, diag id `0x5034`) | gathered `0xFFF8E8B4` → conditioned `0xFFF8E8BA` (inverted copy `+0xBC`, status `0xFFF8E8BE`); compared with a calibration limit in `0x135F54` | `0x7FF` = invalid; unit open |

**Speed sources (2026-11-16) [O]** (`~/tcu_sweep/park_position_speed_decode.md` §3): output speed `0xFFF8EE6E` comes from TIF12 (40 pulses/rev) via `0x143686` → `0x14C23E`, and turbine `0xFFF8EF06` from TIF13 (30 pulses/rev) via `0x14C818`. Both are bench-confirmed on their sensors (spec 02 §4.1).

| Brake pedal switch [I] | `0x140` byte 0 bits 3..2 (2-bit, with check copy); second contact [I] byte 1 bits 3..2 | gathered `0xFFF8E89C` → `0x135DC6` (object `0xFFF8E898`) → status `0xFFF8E8A4`, value `0xFFF8E8A5` (getters `0x135D64/6C`) → app `+0xC9`, `+0x33/+0x34` → flags `+0xC1/+0xC2`; also park module `D61A[0x31]` | 0 = off, 1 = on, 2 = error, 3 = not available; ~~if invalid, substitute "on" when the `0x14B` speed exceeds a calibrated limit (10 raw)~~ **(corrected 2026-11-17, see the note below the table: no substitution in the conditioning)** |
| Engine-off (soak) time [I] | `0x334` bytes 3–4 (12 bit, diag id `0x5100`) | gathered `0xFFF958F0` → `0x14B4B8` → `0xFFF958F4` (u32, getter `0x14B4A8`), quality `0xFFF958F8` → app `+0x130`, `+0x138` | raw × 60 (minutes → seconds); `0xFFF` = invalid → calibrated substitute |
| ~~Park-request / hold status [I]~~ **Ignition status (corrected 2026-11-17)** | `0x112` byte 0 bits 2..0 (3-bit, with check copy) | gathered `0xFFF8E9FA` → ~~`0xFFF8E9FF`~~ terminal-15 FSM `0x140B80`/`0x140EB0` → state `0xFFF8E9FF` (getter `0x140AE8`) → park module `D61A[0x27]` = D641 (spec 14 §3) | ~~values 1–3 make the auxiliary park request active; 0 and ≥ 5 invalid~~ codes 4 and 5 = ignition ON, 7 = SNA; see the note below the table |
| Signed dynamics value [I] (longitudinal acceleration or slope) | `0x14C` byte 4 | gathered `0xFFF96026` → `0x1348D6` → `0xFFF96028` (getter `0x134918`) → app `+0x11C` | (raw × 256 − 32768) / 100 = (raw − 128) × 2.56; `0xFF` invalid |

| Engine mode code [I] | `0x2F8` byte 6 bits 3..0 (4-bit, check copy; valid when byte 0 bit 7 = 1) | safety layer `0x141346` → `0xFFF8EA14…EA18` → module `0xD8122` (spec 11 §3a) | codes 0, 1/4, 2/9, 3 distinguished; `0xF` = invalid; [I] engine state (stopped / cranking / running) |

**Corrections to the table above (applied 2026-11-18; the struck text is the earlier claim):**
- **Fluid temperature (2026-11-17, sources: `~/tcu_sweep/m5_producer_inputs_decode.md` §9 item 1,
  `~/tcu_sweep/temp_byte_live_port.md` §1.1):** the fluid byte is **`0xFFF94102`** (from `0x94F98`,
  `0x887D8` @0x88938: clamp(((s16)0x94F98 + 8) >> 4 + 40, 0..255)) → `0x7C100` publishes `0x940E6` (= 0xFF when
  the fault flag `0x940E8` is set) → gathered `0xFFF8E9C5` → `0xFFF8E9C8`. `0xFFF94101` comes from `0x9393E`
  (`0x908CC >> 3`), a second temperature (33 °C in the dump), gathered at `0xFFF8E9C4`. *(Refined 2026-11-19,
  `~/tcu_sweep/diag_temp_inputs_decode.md` §1.3, §6 items 1, 5 [O conversion, I strong device]: that second temperature
  is the **board temperature sensor on SSLB2** (the J4 job, spec 22 §4.7), the first-priority source of the selected
  ECU temperature `0x9393A`.)* `0xFFF8E9C8` has two
  writers [O]: boot `0x140416` (E9C8 = E9C5, or cal `0x195DB8` = 0x82 on a fault) at startup and boot state 1,
  and the cyclic `0x1405FA` in the **100 ms task, modes 1–3** (list `0x4D9E4` entry 0), through a second safety
  object `0xFFF8ED20` with a slew limiter `0x14ACF0`. The app copies `+0x1B` (`0x89C57`) and `+0x96`
  (`0x89CD2`) are **both** written by `0xB9646` (**30 ms**, modes 1–3 + boot pass); `0xB986C` (10 ms) latches
  **`0x89C13 = 0x89CD2`**. It is not a "+0x1B → +0x96" chain.
  *Addition (2026-11-19, source: `~/tcu_sweep/temp_stage2b_port.md` §3 [O bytes `0x1405FA..0x1406D2`, `0x140416`]; bench
  r04m, CLAUDE.md r04m blocks):* on a **fluid-sensor failure** the cyclic `0x1405FA` substitutes the **board byte**
  (`0x940E3`) minus cal `0x195DB7` (= 0), floored at 0 (status 1) while the board sensor is valid (ED20 D5 = 1,
  D3 = 0); only with the board also failed (D3 = 1) does it fall back to cal `0x195DB8` = 0x82 (90 °C). The die
  temperature never stands in. The boot pass `0x140416` has no board path (0x82 on a failure present at boot).
- **Brake (2026-11-17, source: `~/tcu_sweep/park_interlock_inputs_decode.md` §10 item 1):** with the brake
  invalid, `0x135DC6`/`0x135F54` give E8A4 = 2 and E8A5 = 0 always; the speed comparison sets only
  `0xFFF8E8A9` (+17). The "brake-equivalent above speed 10" law is D607's second branch (`0xA90A0`, cal
  `0x196130` = 10, spec 14). The D631 encoding is 0 = valid, 2 = invalid.
- **`0x112` (2026-11-17, sources: `~/tcu_sweep/can_inputs_decode.md` §5 and §9 items 4–5; `~/tcu_sweep/park_chain_loose_ends_decode.md`
  §8 item 4):** it is the **ignition status code** [O]. Bits 2..0 → E9FA → FSM `0x140B80` → **E9FF** (0 init,
  1 starting, 2 on, 3 shutting, 4 off; FSM `0x140EB0`: 0 → 1 if on else 4; 1 → 2; 2 → 3 if off; 3 → 4; 4 → 1 if
  on). "On" = code 4 or 5 (table `0x191D0C`); code 3 sets the flag E9D4+0x2F; 7 = SNA. When `0x112` is invalid
  (RX status `0xFFF8403C` = 01) the FSM falls back on a ≥ 4000 mV voltage path (enabled by cal `0x195DBF` = 1;
  `0xFFF8403C` is the `0x112` RX status byte, not a selector). Consumers: D641 = E9FF and D642 = its source
  quality (spec 14), the OS mode (mode 2 → 1 on E9FF = 4). Any valid code other than 4/5 turns the ignition
  logically off (park held, OS mode 1, comm monitoring off). "Values 1–3 make the auxiliary park request
  active" is wrong.
- **`0x2F8`:** the "body-controller group" assignment below is corrected — spec 13 §2c's corrected map puts
  `0x2F8` in the ECM group (139) [O, `~/tcu_sweep/can_inputs_decode.md` §4a].

**Not received:** individual wheel speeds. The TCU uses its own output-speed sensor; the only
vehicle-speed-type CAN value is the 11-bit `0x14B` signal.

**Not yet traced:** ~~ignition / power mode~~ (ignition = `0x112`, traced 2026-11-17 — see above) (earlier candidates: the 3-bit fields in `0x122` byte 2, sent
by the body-controller group, diag ids `0x501E`/`0x501F`; the TCU also has its own ignition pin,
spec 01 §2.8), ambient temperature (candidate `0x180` byte 2, gathered at `0xFFF96036`), roles
of torques T2/T3 (T3 is read by the converter state machine `0xDEA5E`), unit of `0x14B`.

Torque roles: T1 travels with engine speed in the ECM message `0x108` and has a loss/reference
correction applied, so [I] it is the actual engine torque. Which of T2/T3 is driver-requested
and which is a limit (maximum/minimum) torque is **open**. Note that the communication monitor
(spec 13 §2c) assigns `0x2F8` to the body-controller group, which needs checking against a bus
capture.

Working files (project root, scratch): the application input map (`0xFFF89C3C` field → safety
getter → safety RAM) is reproducible from `0xB912C`.

## 8. Open items

1. Meaning, unit and scaling of the remaining signals: ignition / power mode, ambient
   temperature, roles of torques T2/T3, unit of `0x14B`, and confirmation of all [I] rows in §7.1. Follow the
   path in §7, or compare with captures of a stock vehicle (faster).
2. Timeout reactions and substitute values per signal.
3. ~~Period unit (§3).~~ RX closed 2026-11-17 (10 ms, §3 correction); the TX unit stays as spec 08.
4. ~~How the protocol variant (1/2) is selected (coding).~~ Closed 2026-11-17: one wire form; see §2's
   correction.
