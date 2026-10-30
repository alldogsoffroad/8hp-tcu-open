# Firmware architecture — proposal (for review)

Status: **agreed 2026-10-14** (decisions in §9; licence deferred). Implementation starts with M0. It turns the specifications
(`docs/spec/`) into a module structure for an open C firmware running on the stock Bosch TCU
(SH7254R, ZF 8HP Gen 1). Open questions for the project owner are listed in §9.

## 1. Goals and constraints

- **Runs on the stock controller**, written with the project's Teensy flasher
  ([8hp-teensy-flasher](https://github.com/alldogsoffroad/8hp-teensy-flasher), spec 17 path)
  into the **application** and **calibration** blocks only; the Bosch boot blocks stay (spec 06).
- **Vehicle-independent core.** The transmission logic works on normalised vehicle signals
  (engine speed, torque, pedal, brake, lever …). Stock FCA CAN is one "vehicle profile"; the
  Nissan Frontier/Xterra profile is added later without touching the core.
- **Own calibration.** The firmware defines its own calibration structures with placeholder or
  re-derived values; it does not copy Bosch/ZF tables (project convention).
- **Testable on a PC.** Everything above the hardware layer builds and runs on the host, with a
  simulation harness and unit tests, before anything is flashed.
- **Safe by construction.** One defined safe state, an independent monitor layer, and no
  solenoid output without the monitor's consent.

## 2. Image and start-up (spec 06, spec 05)

| Item | Requirement |
|---|---|
| Application block | `0x040000–0x17FFFF`; header at `0x40000` (type `C1`), chained to the calibration header |
| Calibration block | `0x180000–0x1FFFFF`; header at `0x180200` (type `83`), project-defined content |
| CRC-32 | computed by the build (zlib CRC-32 over the header range), like `fix_header_crc.py` |
| Markers | `0x17FE00` / `0x180000` programmed (0x00), `0x17FF00` / `0x180100` erased (0xFF) |
| Entry | address at header `+0x7C`; vector table from `+0x80`; entry code sets SP, SR.IMASK = 15, VBR |
| RAM | do not use `0xFFF84000–0xFFF8403F` (boot handover) |
| Watchdog | first overflow ≈ 60 ms, then ≤ 30 ms between services (spec 05 §2) |
| Tick | 1 ms (timer H, spec 02) |

No TPROT reference value or self-check is included (project decision, spec 06 §6.1).

## 3. Layers

```
 ┌───────────────────────────────────────────────────────────────┐
 │ app/      transmission functions (range, strategy, lockup,    │
 │           shift execution, line pressure, pressure → current) │
 ├───────────────────────────────────────────────────────────────┤
 │ io/       normalised inputs (vehicle profile, sensors, lever) │   safety/
 │           and outputs (actuator requests)                     │   independent
 ├───────────────────────────────────────────────────────────────┤   monitor,
 │ bsw/      scheduler, CAN stack, NVM, diagnostics & fault      │   gates all
 │           memory, measurement protocol                        │   outputs
 ├───────────────────────────────────────────────────────────────┤
 │ platform/ sh7254r: start-up, clocks, WDT, ports, timers/TOE,  │
 │           RCAN, RSPI + driver chips, ADC, data flash          │
 │           host: simulated peripherals                         │
 └───────────────────────────────────────────────────────────────┘
```

Rules: a layer only calls the layer directly below it (`app` → `io`/`bsw` interfaces, never
`platform`). `safety/` reads inputs and requested outputs, and owns the final "outputs enabled"
decision.

## 4. Directory layout

```
firmware/
  CMakeLists.txt              two targets: sh7254r image, host simulator + tests
  cmake/toolchain-sh2a.cmake
  include/tcu/                shared types: units.h, result.h, status.h
  platform/
    sh7254r/                  start.S, vectors.c, header.c, linker.ld,
                              clock.c, wdt.c, port.c, atu.c (TOE on/off outputs),
                              rcan.c (3 channels), rspi.c, drvchip.c (solenoid driver ICs),
                              adc.c, dflash.c
    host/                     sim_*.c: same API, backed by the simulator
  bsw/
    sched/                    1 ms tick, task table (1 / 10 / 100 ms), overrun check
    com/                      CAN message objects, CRC-8 + rolling counter (spec 07 §2),
                              timeouts, TX scheduling (spec 08)
    nvm/                      EEPROM-emulation blocks in data flash (own layout)
    diag/                     UDS server (0x10/0x11/0x14/0x19/0x22/0x2E/0x31/0x3E),
                              fault memory (groups, status bits, debounce)
    meas/                     measurement protocol (spec 10, compatible framing)
  io/
    vehicle/                  vehicle_if.h: normalised signals + quality;
                              profile_fca.c (stock CAN, spec 07 §7);
                              profile_nissan.c (later)
    sensors/                  speed sensors (turbine, output), fluid NTC (spec 04 §4.1),
                              park position, supply voltage
    lever/                    lever / range request decode (spec 14 §5a)
    actuators/                actuator request interface → platform drivers
  app/
    range/                    park target arbitration, park release / hold (spec 14, 12 §1.2)
    strategy/                 shift maps, driver type, gear decision (spec 09 §1–2)
    lockup/                   converter lockup state machine and slip control (spec 09 §3)
    shift/                    element matrix, shift types, phase engine (spec 11 §1–3)
    linepress/                max-of-demands line pressure (spec 11 §3a)
    pressure/                 request arbitration, pressure → current (spec 12 §2a)
    adapt/                    fill adaptation (stub; spec 11 §3b)
  safety/                     input plausibility, output gating, program-flow check,
                              inverted-copy helpers, safe-state handling
  cal/                        cal_types.h (versioned calibration layout),
                              cal_default.c (placeholder values), cal_access.c
tools/
  build_image.py              link output → application/calibration blocks, header, CRC, markers
  cal_tool.py                 edit/export the project calibration
tests/
  unit/                       per-module tests (host)
  sim/                        closed-loop simulator: engine + converter + gearset + vehicle
```

## 5. Key interfaces (sketch)

```c
/* include/tcu/units.h — fixed-point units used on every interface */
typedef int16_t  rpm_t;      /* 1 rpm                     */
typedef int16_t  nm10_t;     /* 0.1 Nm                    */
typedef uint16_t mbar_t;     /* 1 mbar                    */
typedef int16_t  degc10_t;   /* 0.1 °C                    */
typedef uint16_t ma10_t;     /* 0.1 mA (solenoid current) */
typedef uint16_t permil_t;   /* 0.1 % (pedal, 0..1000)    */

typedef enum { Q_OK, Q_SUBSTITUTED, Q_DEFAULT, Q_INVALID } quality_t;

/* io/vehicle/vehicle_if.h — what the core needs from any vehicle */
typedef struct {
  rpm_t     engine_speed;        quality_t q_engine_speed;
  nm10_t    engine_torque;       quality_t q_engine_torque;
  nm10_t    driver_torque_req;   quality_t q_driver_torque_req;
  permil_t  pedal;               quality_t q_pedal;
  bool      brake_pressed;       quality_t q_brake;
  degc10_t  engine_temp;         quality_t q_engine_temp;
  range_req_t lever;             quality_t q_lever;   /* P, R, N, D, manual +/- */
  uint16_t  vehicle_speed_kph10; quality_t q_vehicle_speed;
  engine_state_t engine_state;   /* stopped, cranking, running */
} vehicle_inputs_t;

typedef struct {
  nm10_t  torque_limit_req;      /* torque reduction / limitation during shifts */
  bool    torque_limit_active;
  uint8_t gear_display;          /* engaged gear / range for the cluster        */
  range_t range_display;
} vehicle_outputs_t;

/* app/pressure — every pressure consumer goes through one arbiter per regulator */
typedef enum { REG_A, REG_B, REG_C, REG_D, REG_E, REG_TCC, REG_LINE, REG_COUNT } regulator_t;
void pressure_request(regulator_t r, mbar_t p, uint8_t priority, uint8_t source_id);
mbar_t pressure_resolved(regulator_t r);

/* io/actuators — the only path to the solenoids */
void actuator_set_current(regulator_t r, ma10_t i);       /* via safety gate */
void actuator_set_onoff(onoff_out_t o, bool on);          /* park release / hold */
```

Calibration is reached only through `cal/cal_access.h` (typed getters, with the calibration
version and CRC checked at start-up).

## 6. Execution model

| Task | Period | Content |
|---|---|---|
| 1 ms | tick | watchdog service window check, SPI driver job, speed-sensor capture |
| 10 ms | main | CAN RX → vehicle profile → inputs → safety input checks → range → strategy → lockup → shift → line pressure → pressure arbitration → current → safety output gate → actuators → CAN TX |
| 100 ms | slow | NVM, diagnostics, fault-memory housekeeping, thermal model |
| background | idle | measurement protocol, UDS session handling |

One fixed order inside the 10 ms task (no hidden callbacks), so a step can be reproduced
exactly in the simulator.

## 7. Safety concept (first cut)

- **Safe state:** all pressure regulators to their de-energised state, on/off outputs off, TCC
  open. The hydraulics then default as described in spec 11 §1a (6th gear forward while moving;
  park engages via spring when stationary). To be validated on the bench before any vehicle use.
- **Independent monitor (`safety/`):** recomputes the park target and the allowed element
  combination from raw inputs (the stock firmware does the same, spec 14 §6), checks lever-vs-
  speed plausibility, solenoid current feedback, and supply voltage; on violation it forces the
  safe state and records a fault.
- **Output gate:** `actuator_*` calls are applied only while the monitor's enable is set; the
  platform driver also checks a second, inverted copy of each request (as the stock firmware does,
  spec 12 §2).
- **Program flow and watchdog:** checkpoints per task (like spec 13 §3); the watchdog is served
  only after all checkpoints of the cycle are seen.

## 8. Milestones

| # | Result | Proves |
|---|---|---|
| M0 | Image builds; header, CRC and markers correct; boots on a spare TCU; sends a CAN heartbeat; watchdog served | toolchain, boot hand-over, TPROT question (spec 06 §8) |
| M1 | CAN RX/TX with CRC/counter; measurement protocol; UDS read of inputs | bus access on the bench |
| M2 | Sensor inputs (speeds, NTC, park sensor, supply) | input layer |
| M3 | Solenoid outputs on the bench with dummy loads; current measured | output chain, units (spec 12 §4) |
| M4 | Range / park control on a bench transmission or vehicle at standstill | park safety |
| M5 | Fixed-gear "limp" mode (select one forward gear and R, no shifting) | element control, line pressure |
| M6 | Shifting with simple fixed calibration; lockup | full chain |
| M7 | Nissan vehicle profile; gateway decisions | target vehicle |

Each milestone has host tests and a bench checklist before moving on.

## 9. Decisions (2026-10-14)

1. **Toolchain:** GCC for SH-2A (`sh-elf-gcc`, `-m2a -mb`), built from source on Debian's
   `binutils-sh-elf`. — agreed
2. **Language level:** C11, no dynamic memory, MISRA-C-like subset (no recursion, fixed-width
   types, explicit units). — agreed
3. **Floating point:** fixed-point on all interfaces; float allowed inside a module. — agreed
4. **Measurement protocol:** compatible with the stock framing (spec 10). — agreed
5. **Licence:** deferred until the repository is made public.
6. **Host simulator:** I/O stubs for M0–M3; closed-loop drivetrain model with M5. — agreed
