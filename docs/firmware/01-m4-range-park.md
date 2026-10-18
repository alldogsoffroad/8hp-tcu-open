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
| **M4a** (now) | range decision, park actuator sequencing, independent monitor, park mechanism model | host unit tests and closed-loop simulation |
| **M4b** (later) | park release (ch 7) and park hold (ch 11 → TOE31) driven on hardware | needs M3 confirmed with loads and an E-Shift mechatronic |

In M4a the actuator requests are computed and published (measurement protocol), but **no code
path drives channel 7 or 11**: the output gate refuses them in every build until M4b is enabled
explicitly (compile option `TCU_M4B_OUTPUTS`, off, not set by any target).

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

## 8. Open for M4b

Hardware sequencing times; ch 7 behaviour while holding; contact polarity (spec 04 §4.4 [I]);
behaviour of `0x112` byte 0 (spec 14 §8.4) and of `D5F8`; DTC mapping (P1B13 / P1B14).
