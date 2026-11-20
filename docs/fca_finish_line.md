# The FCA build's finish line

**Purpose (2026-11-20).** The plan is to finish the open FCA (Ram 1500 8HP70) firmware as the
stock-faithful reference, then fork it for the Nissan ECU. This file defines "finished" as a checklist
so the fork point is a fact, not a feeling. Update the State column at each milestone; the evidence
for every row is in `docs/lab_record.md` (search the build name).

**Definition.** The FCA build is finished when every function the stock needs to drive a vehicle is
(a) **live** and bench-verified, or (b) a **verified read-only shadow** with its live switch written
down — the switch, the prediction for flipping it, and the hardware it needs — and nothing on the
list is **missing**. Functions whose honest live behaviour depends on a real vehicle CAN set (the
stock on this bench is in CAN-fault limp) are finished as shadows: they go live in the Nissan fork,
bound to the Nissan messages.

States: **LIVE** · **SHADOW** (read-only, verified on the target) · **STOOD IN** (a labelled constant on
the live path) · **MISSING** (not ported) · **UNDECODED** (inside a ported function, counted).

## 1. Platform, supervisor, outputs

| Function | State | Evidence | Live switch / remaining |
|---|---|---|---|
| Boot, scheduler, RSPI buses, ATU timers, ADC/mux chain, WDT | LIVE | r02g–r03a | — |
| Supervisor certification, ladder, letter 'c', rail | LIVE | r02j–r03f | — |
| Bus-A output cadence, request law, driver chips | LIVE | r03b–r03z | — |
| Feedback chain, output diagnosis, current bounds | LIVE | r04b–r04j | — |
| Temperature chain (fluid, die, board, selection, fault flags) | LIVE | r03u, r04j, r04m | — |
| Timer E group A (park hold, TOE30/32), group-A full-on tests | LIVE / STOOD IN | r03c–r03j | the full-on tests held (D-M4-4/5 stage 2) |

## 2. Range and park (M4)

| Function | State | Evidence | Live switch / remaining |
|---|---|---|---|
| Cyclic range chain (lever, brake, engine interlocks, U1267/P0850) | LIVE | r03t | — |
| Park outputs (release + hold), 30 s release limit | LIVE | r03h–r03t | — |
| Key-off auto-P, PA7, C9 bit 7 | LIVE | r03w, r04e, r04g | — |
| Park position sensor | SHADOW (read-only) | r03m | wire into range control after the polarity is checked on an assembled transmission |
| Fault-reaction auto-P (0x89CF2) | STOOD IN (0) | r03w | goes live with the fault chain (fork) |

## 3. Garage engagement and engine (M5)

| Function | State | Evidence | Live switch / remaining |
|---|---|---|---|
| Engagement engine, holder, fill machinery, release, boot pass | LIVE | G6, r04o–r04s | — |
| OS mode selection, mode flags, mode-1 commit gate | LIVE | r04u, r04v | — |
| Latching monitor of ours (bounds, envelope, windows) | LIVE | G6b | windows revisited before a vehicle (D-G6-6) |
| Gear target | STOOD IN (1st) | decision A | the honest chain in the fork (option C) |
| Engine-stall all-release (E28E) | STOOD IN (fence) | D-G6-14 | lift with a monitor allowance |
| Sys (line pressure) | STOOD IN (500) / SHADOW (healthy law) | r05d (shadow Sys 7271 in P) | fork: healthy law live with the Nissan torque/CAN inputs; on this bench the stock is 500 (reaction 50) |
| Reaction 37 (minimum current 0) | STOOD IN (0) | D-G6-10 | goes live with the fault chain |
| WK lock-up | MISSING | — | decode → shadow → live (needs real speeds) |
| Table before the boot pass | STOOD IN (r03f table) | D-G6-15 | optional: the stock stages zeros |

## 4. Shifting (M6)

| Function | State | Evidence | Live switch / remaining |
|---|---|---|---|
| Shift map (target) | SHADOW | r04t, r05d | live = feed the engine's target from it (needs real speeds) |
| Gear feedback chain (commanded gear) | SHADOW (+ E4A7 hold live) | r04w, r04y | — |
| Fault-reaction / gear-code chain (195 groups) | SHADOW (= the stock dump's masks) | r04x, r05a | fork, bound to Nissan messages |
| Gear-pair shift engine: skeleton, kind/class, counters, swap | SHADOW | r04z, r05b | live with real speeds |
| Phase bodies: all upshifts, class-1 downshifts | SHADOW | r05c, r05d (14 shifts on simulated speeds) | class-0 / skip downshifts UNDECODED |
| Torque intervention (request to the ECM) | SHADOW (sequence only) | r05d | per-phase amounts UNDECODED; live = transmit (FCA 0x2F9/0x037; Nissan 0x177 in the fork) |
| Line pressure during shifts | SHADOW | r05d | as Sys above |
| Adaptation (learned fill/kiss offsets) | STOOD IN (reads only; learners kept out) | adaptation_decode | the learners need an EEPROM decision — never without the user |

## 5. What remains before the fork

1. **UNDECODED → decoded and shadowed:** torque-intervention amounts — SHADOW (r05e); WK lock-up — SHADOW (r05e) with the per-state ramps and the request producer decoded (`~/tcu_sweep/gaps3_decode.md`) but not yet ported; class-0 / skip downshifts — decoded, not yet ported. Remaining undecoded: the WK closed micro-slip hold (B388/B424).
2. **Flash space:** solved 2026-11-20 — the app limit was wrongly held at 0x80000; it is now 0xA0000 (136 KB spare at r05d; the application block runs to 0x17FFFF). Reading the live path's calibration in place is optional now.
3. **Fork prep:** DONE 2026-11-20 — the transmission tests run on the `vehicle_in` injector (lockstep-equivalent to the FCA profile), the neutral transmit seam `io/vehicle/vehicle_tx.h`; image unchanged.
4. **Decisions for the fork, not the FCA build:** D-N1 the lever source; D-N4 debug IDs on bus 2; which shadows go live in which order once the Nissan inputs exist.
5. **Hardware, not blocking the fork:** an assembled transmission (park polarity, real shifts); the Nissan ECU capture (`docs/reference/nissan-ecu-bench-capture-plan.md`).

When rows 1 and 3 are done the FCA build meets the definition above and the fork can be cut.
