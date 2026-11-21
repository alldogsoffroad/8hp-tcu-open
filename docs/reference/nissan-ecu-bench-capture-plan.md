# Nissan ECU — bench capture plan (for the M7 fork)

**Status: plan, 2026-11-20. Nothing here has been done yet.** Purpose: fill the gaps the
fork-readiness audit (`~/tcu_sweep/fork_readiness_audit.md`) and the line-pressure / torque decode
(`~/tcu_sweep/line_pressure_torque_decode.md`) found in the Nissan CAN picture, so the fork's
vehicle profile can be written from measurements. Known so far: `nissan-frontier-can.md` (this
folder) and `github.com/alldogsoffroad/NissanDBCFilez`.

Pinouts are deliberately not given here: take them from the factory service manual (EC / engine
control section, "ECM terminals and reference values") for the exact ECM part number on the bench.
Do not guess power or CAN pins from another model year.

## 1. What the bench ECU can and cannot tell us

| Question | Bench ECU alone | Needs the truck |
|---|---|---|
| ECM transmit set: IDs, DLCs, **periods**, counters/checksums, "not available" values at rest | yes | — |
| ECM behaviour with **no TCM on the bus** (which messages change, DTC/limp flags it sets) | yes (the bench has no TCM) | compare with TCM present |
| Engine speed / torque scaling under load, pedal end points | no (engine not running) | yes |
| What the **old Nissan TCM** transmits (0x177, 0x421, others) | no | yes: capture with the TCM unplugged vs plugged |
| ABS / BCM messages (wheel speeds, brake) | no | yes |

So: one bench session for the ECM's own frame set and its no-TCM reaction, and one truck
session for the dynamic signals and the TCM's transmit set.

## 2. Bench setup

1. Power the ECM from a current-limited bench supply (12–13.5 V; limit as low as it will boot
   with). Battery + / IGN / ground pins and the immobiliser (NATS) requirement from the service
   manual for this part number. If NATS blocks normal operation, note which messages still run.
2. CAN-H / CAN-L from the ECM connector to the Teensy logger, with 120 Ω termination at the
   logger end (the ECM may terminate the other end — measure H-L resistance with power off:
   ≈ 60 Ω = both ends terminated, ≈ 120 Ω = one end).
3. Record listen-only first: `python3 tools/bench/tcu_bench.py log <capture>.log`
   (listen-only, automatic bit rate; records the bit rate it locks to). Expected 500 kbit/s [I —
   unverified for this ECU].
4. Never transmit to the bench ECU in the first session. Never connect it to the TCU bench bus.

## 3. Captures to make (bench)

| # | Condition | Duration | Gives |
|---|---|---|---|
| B1 | power on, ignition ON, engine not running | 60 s | the full transmit set at rest, periods, counters, NA values |
| B2 | ignition OFF → ON → OFF cycles (×3) | 3 × 20 s | start-up/shut-down order, which messages stop, ignition-state signals |
| B3 | throttle/pedal input exercised (if the pedal sensor can be connected or simulated) | 60 s | pedal signal and its scaling at rest |
| B4 | after ≥ 2 min with no TCM | 60 s | the ECM's reaction to a missing TCM (flags/bytes that change vs B1) |

For each: write down the ECM part number, supply voltage, and anything on the ECM's own
diagnostic readout if available.

## 4. Captures to make (truck, when convenient)

| # | Condition | Gives |
|---|---|---|
| T1 | key on, engine off, Nissan TCM connected | the TCM's transmit set at rest |
| T2 | same, Nissan TCM **unplugged** | which IDs disappear (the TCM's set); confirms 0x177 / 0x421 senders — **0x421 staying = the lever is directly usable (D-N1)** |
| T3 | idle in P/N, then D with brake | engine torque at idle, pedal at rest, gear indicator |
| T4 | gentle drive, full upshift ladder; one wide-open-throttle pull | torque scaling end points, the 0x177 handshake, pedal end points |
| T5 | engine start and stop | the ECM's engine-state / crank signals |

## 5. What each capture must answer (the fork's input list)

- **Per message:** ID, DLC, transmit period (min / typical / max), byte order, rolling counter
  (position, modulus, step), checksum (position; algorithm if it can be found), "not available"
  value, sender (ECM / TCM / ABS / BCM).
- **Engine torque in Nm:** actual, driver-demanded, and any limit signals; scale and offset
  (0x182 byte 4 is a candidate in %, scaling not pinned).
- **Pedal:** the confirmed signal, its rest and full values, "not available".
- **Ignition / engine state:** any ignition, crank or engine-running signal (the fork otherwise
  falls back to the supply-voltage path).
- **The old TCM's transmit set:** every ID that disappears in T2, with its content during shifts
  — at least 0x177 (torque reduction, bytes 2/3/5 as well as 4) and 0x421 (gear indicator).
- **The ECM's no-TCM reaction:** what it flags or changes in B4 / T2 — the fork must either
  reproduce the TCM messages that prevent it or accept it.
- **The lever (decision D-N1, 2026-11-20: the factory Nissan shifter first; fallback a Jeep JL
  8HP-style shifter).** The RE5R05A is electronically selected (no cable). 0x421 byte 0 (P 0x08,
  R 0x10, N 0x18, D 0x20, 0x00 between detents) may be the lever's own position message. Answer:
  - **who sends 0x421** — T2: does it stay with the old TCM unplugged? (stays → the lever / another
    module, directly usable; gone → the TCM derives it, and we need the lever's own signal);
  - its period, any counter / checksum, and whether a second, independent lever signal exists
    (the stock cross-checks two sources);
  - manual positions (2 / 1, a manual gate, OD-off button) and how they appear on CAN;
  - any lever wiring to the old TCM (park / neutral signal, the shift-lock solenoid, starter
    inhibit, reverse lights) that our TCU or the vehicle must still provide.

## 6. After the captures

Update `NissanDBCFilez` (periods, counters, senders), then a desk mapping of every signal the
stock strategy reads (the FCA profile's 9 messages + 0x210, `~/tcu_sweep/can_inputs_decode.md`)
against the Nissan set, giving the fork's `profile_nissan.c` input table and its TX table.
