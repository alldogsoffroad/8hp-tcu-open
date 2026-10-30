# M4 — Range and Park control (design)

Status: **draft for review** (2026-10-18). Behaviour reference: spec 14 (park arbitration),
spec 12 §1.1–1.2 (park actuators), spec 07 §2 (CAN protection). Architecture: 00 §5–8.

## 1. Scope and decisions

Decisions (user, 2026-10-18):

1. Lever input: **stock Chrysler/Ram format** (shifter type 1, `0x2EA` bus 1 preferred, `0x210`
   bus 2 fallback, spec 14 §5a). Other shifters wait for M7 (vehicle profile / gateway).
2. **Automatic Park** when stationary without a valid lever request: included (stock rule).
3. Stationary limit: **80 output rpm** (stock).
4. **Simulation only for now.** Hardware test later.

So M4 is delivered as:

| Part | Content | Test |
|---|---|---|
| **M4a** (2026-10-18) | range decision, park actuator sequencing, independent monitor, park mechanism model | host unit tests, closed-loop simulation, bench check 12/12 (§7a) |
| **M4b** (2026-10-30) | park release (ch 7) and park hold (ch 11 → TOE31) driven on hardware | host tests incl. output-path assertions; bench session §8 (E-Shift mechatronic) |

In builds without `TCU_M4B_OUTPUTS` the actuator requests are computed and published
(measurement protocol) but **no code path drives channel 7 or 11** (the output gate is a
no-op). With the option set (the RAM bench application) the outputs are driven as §8.

## 2. Inputs (existing interfaces)

| Input | Source | Quality handling |
|---|---|---|
| lever request P/R/N/D, source | `vehicle_in()->lever`, `q_lever`, `lever_src` (profile_fca.c, already done in M1) | `Q_INVALID` = no valid request |
| output speed | `sensors_in()->output_speed`, `q_output_speed` | invalid → treated as **moving** (§3) |
| park position contacts | `sensors_in()->park`, `q_park` | used by sequencing and monitor only |
| engine running | `vehicle_in()->engine_speed` > 400 rpm with `Q_OK` | line pressure available [I] |
| supply | `sensors_in()->battery` | below 9 V → no release |

The stock "valid park request from the safety layer" (`D5F8`, spec 14 §3; origin open, [I]
Auto Park / driver leaving) has no source yet; it is fixed to **0** and the rule that uses it is
present but inactive.

## 3. Park target (`app/range.c`)

Output: `PARK_ENGAGE` (8) or `PARK_RELEASE` (6), with a reason code. Stock mode 4 (`0xA57DA`,
spec 14 §4.2), with the stock special cases 1–4 left out (they only refine "released"):

| # | Condition (evaluated in order) | Target | Reason |
|---|---|---|---|
| 1 | lever = P (valid) | ENGAGE | `R_LEVER_P` |
| 2 | moving (output speed > 80 rpm, or speed invalid) | RELEASE | `R_MOVING` |
| 3 | park request (inactive, §2); stationary from row 2 | ENGAGE | `R_PARK_REQ` |
| 4 | lever = R, N or D (valid) | RELEASE | `R_LEVER_RND` |
| 5 | stationary, no valid lever request | ENGAGE | `R_AUTO_P` |

Row 3 before row 4 as in stock `0xA57DA`: a valid park request while stationary engages Park
even with R/N/D selected.

Notes:
- Row 1 follows stock: P selected while moving gives ENGAGE. The *sequencing* (§4) does not
  drop the park hold above the limit, so the pawl is not released onto a moving output; it
  engages once below 80 rpm. **Review point:** this is a project choice on top of stock.
- Row 2 before row 4: a lost lever signal while moving never engages Park.
- Lever source loss (`LEVER_SRC_NONE`) keeps the profile's last decoded value only for the
  profile's own timeout; after that `q_lever = Q_INVALID` and rows 2/4 apply.
- Target and reason are stored with an inverted copy and checked on every read (as stock).

## 4. Park actuator sequencing (`app/park_ctl.c`)

States and output requests (ch 7 = park release, ch 11 = park hold; spec 12 §1.1):

| State | ch 7 | ch 11 | Leave when |
|---|---|---|---|
| `IN_P` | off | off | target RELEASE, stationary, engine running, supply ≥ 9 V → `RELEASING` |
| `INIT` | off | off | first cycle: contacts `IN_P` → `IN_P`; engine running or moving → `HOLDING` (re-latch after a reset); otherwise `ENGAGING` |
| `RELEASING` | on | off | contacts `NOT_P` → `HOLDING`; target ENGAGE or engine stopped → `ENGAGING`; 500 ms without → fault, `ENGAGING`, no new release until P is selected again |
| `HOLDING` | off [I] | on | target ENGAGE and stationary → `ENGAGING`; engine stops in N → stays (car wash, spec 12 §1.1) |
| `ENGAGING` | off | off | contacts `IN_P` → `IN_P`; 1 s without → fault, `SAFE` (outputs stay off; spring) |
| `SAFE` | off | off | monitor fault; left only by power cycle |

- Park hold stays on in `HOLDING` while moving, whatever the target, until stationary.
- Whether ch 7 can drop once the hold latches is **[I]** (stock may keep both); a
  calibration flag selects "release stays on while holding", default **on** (more conservative
  hydraulically). To be settled in M4b.
- Timings 500 ms / 1 s are project values, to be measured in M4b.

## 5. Independent monitor (`safety/park_mon.c`)

Separate code path (own functions, own copy of the limits), as the stock safety layer does
(spec 14 §6):

1. Recomputes the target from the same inputs with independently written rules; mismatch for
   more than 2 cycles (20 ms) → `SAFE`.
2. Release allowed only when stationary and engine running at the moment `RELEASING` starts.
3. Contacts vs state: `IN_P` while contacts say `NOT_P` for > 200 ms (or the reverse in
   `HOLDING`) → fault.
4. Integrity: inverted-copy mismatch of target, state or outputs → `SAFE`.
5. The output gate applies ch 7 / ch 11 only while the monitor's enable is set (and, in M4a,
   never).

Faults are recorded as project fault codes (own numbering, spec 13 style) and shown in the
measurement status; DTC mapping waits for the fault-memory work.

## 6. Simulation (`tests/`)

- **Park mechanism model:** pawl position, release piston needs line pressure (engine running)
  and ch 7; hold magnet latches the piston while ch 11 is on; spring engages when neither holds
  and output speed < ratchet limit (model parameter); contacts derived from the pawl position
  with a configurable delay.
- **CAN stimulus:** `0x2EA` frames with correct CRC-8 / counter and `0x210` frames, at the
  stock period, with fault injection (bad CRC, stuck counter, timeout, sources disagreeing).
- **Scenarios** (each a closed-loop case in `tests/unit/test_park.c`, ctest `park`):
  P→R→N→D→P at standstill; engine start/stop in P and N; P selected at 30 km/h; lever lost
  while moving and while stationary; bus 1 lost (fallback to bus 2); contacts stuck; monitor
  mismatch forced; supply low; car-wash (N, engine off).
- Pass criteria per scenario: no release without engine running and stationary; no ENGAGE
  output while above the limit; every fault ends in `SAFE` with outputs off.

## 7. Measurement and tools

- Measurement table (spec 10): target, reason, park_ctl state, requested ch 7 / ch 11,
  monitor status and fault bits.
- `tcu_bench.py`: a `park` view of those fields (usable on the bench without outputs).

## 7a. Bench check 2026-10-18 (M4a, module alone) [M]

RAM image `RAM 261018-0237` loaded with `app-flash` (6.8 s); `tcu_bench.py park-check` sends
`0x2EA` (bus 1, CRC-8 + counter) and `0x210` (bus 2) every 10 ms and reads the measurement
table. Output speed read valid and stationary. **12/12 steps passed:** P/R/N/D from CAN-C; code 0
keeps the last range; bad CRC or missing `0x2EA` → `0x210` (shifter bus) and back; invalid
code or both missing → Auto P. Without park contacts the sequencing goes to `SAFE` with
`PC_F_ENGAGE_TIMEOUT` (0x02) after about 1 s, as designed; both output requests stayed off and
the monitor recorded no fault. With no valid lever the measurement index 13 keeps the last
decoded range (source 2 = none, quality invalid); the decision uses the quality.

## 8. M4b — hardware outputs (design, 2026-10-30)

The output path, compiled in when `TCU_M4B_OUTPUTS` is defined (the RAM bench application;
the code-flash application stays gated until the bench check passes):

- **Platform, park hold (TOE31 / PL6)**: `plat_toe_out_init()` / `plat_toe_out_set(permil)` —
  timer E sub-block 3 channel 1 (spec 02 §3.1): clock-bus line 1 with PSCR1 = 4 (4 MHz) and
  PSCRE3 = 1 (2 MHz count clock), cycle 2000 ticks = 1 kHz, counter offset 1452 as stock, duty
  through the reload buffer. **Polarity decision:** the output is high from cycle start and
  falls at the DTR match; the project drives it non-inverted (TONE = 0), so DTR = energisation
  and 0 keeps the pin low — the same level as the port-mode safe state, and the pin function is
  switched last so the level never glitches. Stock instead inverts (polarity 1) with a
  battery-compensated duty; the FE11 commissioning step measures the actual magnet current in
  both states and settles the mapping (flip `TOCRE3` and the DTR inversion if the driver stage
  turns out active-low). First-cut hold duty: 1000 permille (fully on).
- **Park release (channel 7)**: `SOL_MODE_FULL_ON` through the existing driver layer. The
  staging previously collapsed every non-CURRENT mode to OFF — fixed; the chip mode field for
  full-on is verified by FE11 before the valve is trusted (spec 12 §2b).
- **Enable arbitration**: `sup_enable_set/clear(SUP_EN_BENCH | SUP_EN_PARK)` — the supervisor
  output-enable bit is the OR of the live source requests, so the bench idle path can no longer
  remove the park module's enable, and an enable still never outlives its requester's 5 ms tick.
- **Ownership**: the park module owns channel 7 and TOE31 whenever no bench test is active; the
  bench idle forcing covers channels 0–6 only. While a bench test runs (`bench_active()`), the
  park module suspends its outputs (requests keep being computed and published).
- **FE11 commissioning routine** (`31 01 FE 11 out seconds [duty_%]`, out 0 = ch 7 full on,
  out 1 = TOE31 duty): same interlocks as FE10 (extended session, engine off, drivers and
  supervisor healthy, re-checked every 10 ms), one output at a time, 1–5 s. The duty is one
  byte in percent so the request fits a single ISO-TP frame.

Bench sequence (E-Shift mechatronic, rig at standstill): (A) the M3 solenoid checklist on the
real coils (channels 0–6 — the prerequisite, previously unrecorded); (B) FE11 commissioning —
park hold current at C7 (~0.5 A expected, scope the 1 kHz PWM), park release current and
click, settle the TOE31 polarity and the full-on chip field; (C) read the AN5/AN6 bands and
fix the park-contact polarity in `io/sensors/sensors.c` if inverted (bench items 16/17);
(D) `park-check --hw`: the lever scenario with outputs live. **Physics caveat:** a bare
mechatronic has no pump, and the park release is hydraulic — the release piston may not move
dry, so the contacts may stay "in P" and RELEASING can hit its 500 ms timeout (a valid
exercise of the fault path). Full pawl engagement against a driveline needs a bench
transmission or vehicle; M4b-mechatronic level proves the complete electrical chain with the
real actuators and the real position sensor.

## 9. Open after the M4b bench session

Hardware sequencing times (replace the 500 ms / 1 s project values); ch 7 behaviour while
holding (the `keep_release` flag); the TOE31 polarity and full-on chip field (settled by FE11);
contact polarity (spec 04 §4.4 [I]); behaviour of `0x112` byte 0 (spec 14 §8.4) and of `D5F8`;
the battery-compensated hold duty; DTC mapping (P1B13 / P1B14).
