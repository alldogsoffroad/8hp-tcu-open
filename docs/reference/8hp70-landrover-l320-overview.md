# ZF 8HP70, Land Rover Range Rover Sport L320 (2009–2013): workshop overview, notes

Source: "ZF 8HP70 Transmission Overview and Specs", 182 pages, on Scribd
(https://www.scribd.com/document/809748208/8HP70-8-Speed-Auto-Transmission). It is a Land Rover
workshop-manual extract: Description and Operation, Diagnosis, and removal/installation
procedures. Reviewed 2026-11-16 through the in-app browser. Pages 1–129 were read as text,
except page 40; pages 130–182 did not load. Pages 22 onward are service procedures (tools,
torques) with nothing on control.

**Applicability:** this is the **first-generation 8HP70 in a Land Rover** with a Land Rover TCM.
Our unit is the Bosch 1034420267 (FCA, shift-by-wire). Hardware facts (gear train, coil
resistances, the park-lock mechanism) are likely shared. Anything about solenoid polarity,
sensors or park control must be checked against our own decode before use. Grade everything here
[M], meaning manual, a different variant.

Facts are paraphrased with page numbers. The source text is not reproduced.

## Mechatronic contents (p. 9–10)
- TCM, **seven pressure regulators (EPRS)**, **two park-lock solenoids**, 21 spool valves, a
  temperature sensor, a turbine speed sensor and an output shaft speed sensor.
- **No park position sensor is listed**, unlike our FCA unit (AN5/AN6). This document therefore
  cannot settle our park-sensor polarity question.
- Solenoid legend: EPRS A (brake A), D (clutch D), B (brake B), E (clutch E), C (clutch C),
  SYS (system pressure), WK (converter lock-up); **MV1 "pressure reducing valve"**; **MV2
  "magnet valve for electrical park interlock (hold out of park)"**.

## Coil resistances at 20 °C (p. 11–12), against our bench [D] (supply-step estimates, warm coils plus harness)
| Coil | Manual | Ours (CLAUDE.md, r03e/r03h/r03i) |
|---|---|---|
| EPRS (all seven) | **5.05 Ω** | ≈ 7 Ω (850 mA step) |
| MV1 (on/off) | **10–11 Ω** | park release, bus-A ch 7: ≈ 11 Ω |
| MV2 (on/off, park hold) | **25 Ω** | park hold, TOE31: ≈ 20 Ω (PWM D²V/ΔI estimate) |

The release and hold figures match well. **Spec 12's "~25 Ω" is the park-hold coil, not the
pressure solenoids.** That resolves the conflict noted at r03e.

## EPRS characteristic (p. 11)
- 12 V, PWM, control pressure 0–4.7 bar.
- **A, B, D, E, WK:** pressure *rises* with current (orange connector cap).
- **C, SYS:** pressure *falls* with current (grey cap; 4.7 → 0 bar).
- **Conflicts with spec 12 §1.1 (FCA), which lists D and E as normally open.** Different variant
  or different source. Do not change spec 12 on this document alone.

## Park lock (p. 12)
- Shift-by-wire, no Bowden cable. An emergency release cable is provided.
- **Engaged by spring:** a pre-tensioned torsion spring at the park-lock disc pulls the piston to
  "park".
- **Released by pressure:** main pressure on the park-lock piston pushes it back.
- **Held out of park by MV2:** when energised, its claws latch the piston in the released
  position. De-energised in P, so the piston is free to spring into park and is held there by the
  claws.
- The leaving-P sequence: MV2 energised (the claws release the piston from its park-locked
  position), and pressure pushes the piston back.
- **N with the engine off:** pressure is gone, MV2 stays powered and holds the piston out of park
  (the car-wash case). If the battery voltage falls too low to keep MV2 energised, park engages.
- This matches the stock law we ported: release and hold both on out of P, both off in P
  (r03j/r03k).

## Sensors (p. 12–13)
- **Turbine and output speed sensors are Hall-effect sensors inside the mechatronic**, not
  serviceable. Turbine speed is used for converter-clutch and clutch slip. Output speed is
  compared with engine speed from CAN for slip and plausibility.
- Temperature sensor in the mechatronic; on failure the TCM uses a default and stores a fault.

## Gear train and hydraulics (p. 8–9, 14, 17)
- Four simple planetary sets, 3 clutches (C, D, E) and 2 brakes (A, B). Two elements are open in
  each gear. Output is always through the carrier of set 4.
- **Ratios:** 1st 4.714, 2nd 3.143, 3rd 2.106, 4th 1.667, 5th 1.285, 6th 1.000, 7th 0.839,
  8th 0.667, R 3.317.
- All 1↔8 shifts are overlap (clutch-to-clutch) shifts.
- **Standstill decoupling:** at standstill with the brake applied, the converter is decoupled by
  actuating **clutch B**, depending on load and output speed.
- Pump: double-stroke vane pump, chain-driven from the converter, 50 cm³/rev, relief valve
  limits system pressure to **32 bar**.
- The shift-element application matrix is a figure (p. 17–18) and was not extracted as text.
