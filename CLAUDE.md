# CLAUDE.md — project orientation

You are joining 8hp-tcu-open mid-flight. This file is the orientation: what this is, where it stands, the rules, and where everything lives. **The full dated lab record (every build's prediction and bench result since 2026-11-10, every decode summary) is `docs/lab_record.md`** — append-only, moved out of this file verbatim on 2026-11-20. Read the parts you need from it; do not load all of it.

## What this is

An open C reimplementation of the firmware for the Bosch transmission control unit of the ZF 8HP (P/N 1034420267, Renesas SH7254R, SH-2A big-endian, superloop, no RTOS). The stock firmware is fully decompiled (10,982 functions, Ghidra sweep, specs 19–26 in `docs/spec/`); our rewrite lives in `firmware/` and runs on the real bench controller with a real (dry) mechatronic. The goal of M3 — the engaged rail and the stock's P-state solenoid currents under open-firmware control — was met on 2026-11-13. The current arc is M5 (garage engagement) and M6 (shifting), built by porting the stock's own logic, mostly first as read-only shadows, then live.

**The plan (user decision 2026-11-20):** finish the open FCA (Ram 1500 8HP70) firmware as the stock-faithful reference, then FORK it and adapt the fork to a Nissan Frontier/Xterra ECU on the Nissan CAN bus. A Nissan ECU is on the bench (not wired in yet). Nissan sniff data: `github.com/alldogsoffroad/NissanDBCFilez`. Keep FCA-specific CAN knowledge inside `firmware/io/vehicle/` (the seam scan enforces it).

## The state now (2026-11-20, r05c on the bench)

- **M3 (closed 2026-11-13):** the supervisor certifies (0x14 = C7, gate 1); the stock self-test ladder runs to 0xAA; letter 'c' / J2 0x9020 hold the rail; the stock P set drives 1.775 A (the stock's own 1.773 A).
- **M4 (partial):** the stock cyclic range chain owns D6D6 live (brake/engine/lever interlocks, U1267/P0850 debounce); both park outputs at stock values (30 s release limit, .noinit); key-off auto-P; PA7 and C9 bit 7 live; park sensor ported read-only. Open: sensor polarity on an assembled transmission.
- **M5 (partial):** the stock engagement engine is the sole producer of the A–E/WK/Sys requests, at the stock RAM addresses, behind a latching monitor of ours; the stock 1.2 s release; OS mode 2 chosen by the stock selector; the mode-1 commit gate live; the commanded gear E4A7 holds E69F while a change is pending; BD15 cleared at the end (r04y). Output diagnosis on the real feedback chain; current bounds and temperature chain live.
- **M6 (started), all READ-ONLY shadows on the target:** shift map (r04t), gear feedback chain (r04w), the fault-reaction / gear-code chain over all 195 DSM groups — its masks equal the stock dump exactly, 00813240 / E8260114 (r04x, r05a), the gear-pair shift-engine skeleton with a coupled gear chain (r04z), its kind/class machine, BD1B, counters and block swap (r05b) and the phase bodies for every upshift and the class-1 downshifts (r05c); shadows read stock calibration at 0x80000+ in place.
- **Live stand-ins (labelled deviations):** gear target forced 1st (**decision A, 2026-11-20** — the stock picks 3rd on this bench via CAN-fault reaction 13; the honest chain belongs to the Nissan fork); Sys 500 / reaction 37 = 0 / WK not ported; the engine-stall fence; the r03f table before the boot pass; park_law's 0x89C30/E69B stand-ins.
- **Bench limits:** dry mechatronic, no turning shafts; brake/engine/lever simulated on CAN; supply limit 5 A.
- **Running numbers (r05a):** P ≈ 1.8 A, D/R ≈ 3.0 A; engagements complete at pass 96 (D) / 95 (R) cause 1; releases 120 passes; task_10ms ≈ 0.6 ms typical, ≤ 1.62 ms max on engagement passes; app end 0x7AFC4 (install blocks 0x40000–0x70000; block 0x80000 is calibration — never write it); host tests 372.

## The immediate work (2026-11-20)

1. **The gear-pair shadow is complete as decoded (r05c).** Remaining undecoded inside it: class-0/skip downshifts, the torque-phase terms of class 1, the adaptation value 0xAED5C — all counted stand-ins.
2. **Before any live shift:** line pressure during shifts and torque intervention (`~/tcu_sweep/line_pressure_torque_decode.md`; the input-torque writer 0xDDFD8), D-FR-5 (the monitor latches P on any gear-code change while engaged) stays until then.
3. **Fork prep (cheap, optional):** re-base the 11 FCA-frame host tests on the `vehicle_in` injector; a neutral TX seam (`main.c` still calls `fca_tx_*`). Open decisions for the fork: D-N1 the lever source (a Nissan cabin has no CAN lever), D-N4 moving debug/meas IDs to bus 2.
4. **Before any vehicle:** the honest gear target and fault reactions (in the Nissan fork), line pressure in drive, WK lock-up, a test on an assembled transmission.

## The rules (each one earned)

These are the user's standing directives. They are not suggestions.

1. **No flash without a prediction.** Before staging any bench flash, derive and state whether it will succeed from evidence already in hand. If you can derive failure, do the desk work instead. Bench flashes are the scarce resource. (Stated 2026-11-11 after I staged a flash whose failure was derivable in two sentences.)
2. **Decode before drive.** A flash executes a decoded recipe, never a guess. Ghidra/disassembly/session-log evidence first.
3. **Inventory logged data before new runs.** Before proposing a bench run for information, assemble the answer from existing captures/dumps/decodes. The user challenges redundant runs — rightly.
4. **"Stock-shaped" ≠ decode.** Verifying that words/parameters match the stock is not verifying behavior. Sequence, timing, mechanism, and count must each be verified against the stock's actual bytes. Two session-losing errors (the silence window, the pool order) were "words right, structure unverified."
5. **User bench truth over theory.** The user's physical observations are ground truth. Label inferences as inferences. Correct records immediately when refuted.
6. **Bench protocol:** the wake storm is armed ONLY against a dark bus (arming against a live board = instant false catch); storm and the install chain run as SEPARATE background tasks; one process per serial port (the meas/witness/ammeter tools conflict); `meas 0x02` (and verify the reply) before every read session — the periodic 0x398 stream races the meas replies and garbles reads; the ammeter never co-runs with the storm/install; announce any current-moving event before it happens; power cycles are the user's physical act — off, then arm, then on.
7. **Preserve displayed messages and written flash.** Never rewrite history the user has seen; diagnostics go in prose and the status file. (The lab record is append-only; corrections are new entries.)
8. **Subagents for deep decodes; you validate before flashing.** The pattern that works: brief an agent with the full evidence chain and corpus pointers, validate its commit (build, tests, CRC — run them yourself), then apply. The user will tell you when they want an agent vs. direct work — "we've already logged and identified this; implement it now" means do it yourself, now.

Standing safety constraints (from the lab record): FE13 sub 16 stays refused; stock values only; the engine sim OFF for every install; never send 0x112 with codes other than 4/5 (keyoff-check is project-firmware only); brake/engine/lever sims are dry-bench only; **never port an EEPROM/NVM/DSM-record/event-log/fatal writer** (adaptation learners, 0x7F734, the plausibility monitors, the 0xEBB6C/0xEBC60 NV counters, 0xE8CF6, 0xD229E, 0xE8BFA, 0xB79E4, 0xB75A6/0x7A914/0x6865C) without the user's decision — the link scans (`firmware/tests/unit/g6a_scan.py`) refuse them; no existing RAM symbol may move (diff `sh-elf-nm` b/B/d/D against the last flashed build; new RAM only in windows that are zero in `stockram_driving_20261104.bin`, ASSERT-bounded); the RAM app, loader and flasher stay byte-identical unless a change is meant for them.

## Where everything lives

- `docs/lab_record.md` — the full dated lab record (append new results at its end).
- `docs/spec/` — the specification, specs 1–26. Specs 19–26 are the function catalog and top-down syntheses. **Spec 21 §1 is the power-on chronology.**
- `firmware/` — the open reimplementation: platform (`platform/sh7254r/`, `linker.ld` with the pinned stock-address sections), supervisor (`bsw/sup/`), drivers (`bsw/drv/`), vehicle CAN (`io/vehicle/` — the FCA profile, `fca_tx`, `diag_ids.h`, the host-only `vehicle_inject`), the app (`app/`: `main.c`, the live engine `gs_engine.c`/`gs_cand.c`, the monitor `gs_bridge.c`, the range chain `rng_chain.c`, the shadows `sm_*`, `fr_*`, `gp_*`, `fb_*`, `tsel_*`, `pa7_*`), safety (`safety/`). Host build + tests: `build/host` (`ctest`, 372). Target: `build/sh2a` (Ninja, `firmware/cmake/toolchain-sh2a.cmake`); POST_BUILD scans run there. CMake `TCU_VEHICLE` = FCA.
- `firmware/tests/unit/transcript.c` — the bus-B byte-identity tool; any sequence drift fails the build.
- `~/tcu_sweep/` — the decode corpus and every port report (index below).
- Generators (calibration tables byte-checked against the stock image): `tools/gen_*_cal.py`. The read-only shadows (`fr_*`, `sm_*`, `gp_*`) read stock calibration at 0x80000+ **in place** (`firmware/app/cal_inplace.h`, the one address list; host tests load the stock image via `TCU_STOCK_IMAGE` and SKIP loudly without it); only tables from below 0x80000 stay copies.
- Bench tools: `tools/bench/tcu_bench.py` (meas reads, `gs-check`, `park-check`, `range-check`, `keyoff-check`, the CAN sims with drained Teensy ports), witnesses `tools/bench/witness_{g6a,g1,g5,sm,gp,fr,temps,fbshadow,pa7}.py`, `/tmp/witness_r03x.py` (supervisor/health), `/tmp/witness_odiag_r03x.py`; `~/tcu_bench_tools/amps.py` (ammeter → /tmp/amps.log); storm `~/Documents/8hp-teensy-flasher/tools/arm_storm.py --mode 5`.
- Stock reference: image `~/Downloads/260924131233 1034420267_IFla.bin` (flash base 0); RAM dump `stockram_driving_20261104.bin` in the repo root (0x20000 bytes from 0xFFF80000; never commit it). Our images: `build/*_IFla.bin`.

## The bench mechanics (established laws, don't relearn them)

- **Validation in one command (2026-11-20):** `python3 tools/validate_build.py check <new> <base>` builds host + SH, runs ctest, diffs RAM symbols against `build/flashed/<base>/`, checks the RAM app/loader/flasher identity and new RAM windows against the stock dump, builds `build/<new>_IFla.bin`, and writes `/tmp/install_<new>.py` + sigs — PASS/FAIL summary. After a flash is confirmed: `validate_build.py snapshot <new>` (from that build) so it becomes the next baseline.
- **Image:** `tools/build_image.py --app build/sh2a/tcu_app.hex --base build/<previous>_IFla.bin -o build/<new>_IFla.bin --stock-ident --gap-keep-above 0x50000`. Check the block list against the previous image (an app past a block boundary must write that block too — r02w crashed on this).
- **Install script:** copy the previous `/tmp/install_<prev>.py` with the build name substituted; its sig table `/tmp/<new>_sigs.json` sets every block the image changes (today 0x40000–0x70000) to `null` (force-write). The skip check means "live == TARGET sig". Expect 21 "OK" verifies.
- **The flash cycle:** user "off" → arm storm + launch install (two background tasks) → user "on" (the fast Teensy LED is the cue) → install writes (~5 min) → user "off" → (ammeter logger) → "on" → witness reads.
- **gs-check (the engagement test, sims from power-on):** make a fifo, hold it open (`sleep 900 > fifo`), run `tcu_bench.py gs-check --sim-brake --sim-engine 700 --start-before-power --hold 8 < fifo` in the background, answer `yes\nyes\n`, user powers on, send `\n`; **call out each lever step live** (P, D, P, R, P, 8 s each) so the user can match the ammeter; ask afterwards whether it behaved as predicted. For a read-only shadow flash the gs-check alone covers the live path and the shadow (the separate plain boot is optional).
- **Cell addresses:** resolve from `build/sh2a/tcu_app.elf` by symbol every time. While an agent rebuilds `build/sh2a`, read the bench with an ELF built from the flashed commit.
- **The walk timer is a DOWN COUNTER** (DCNTD00 @0xFFFFF5B0, vector 224 = underflow), 1 µs/tick (measured r02g); never a compare register.
- **The tamper wall:** never bypass the letters interlock.
- **The stock reference measurement:** genuine stock → rail at < 0.5 s; by +13 s: 0x14 = C7, gate 1, letter 'c', EN90 0x20.

## Index — decode reports (`~/tcu_sweep/`) and where their summaries are in `docs/lab_record.md`

Search the lab record for the capitalised heading or the build name.
- Supervisor / certification: `first500ms_diff.md`, `gate_bit1_decode.md`, `letters_law_decode.md`, `ladder_blockers_decode.md`, `l1_port.md`, `l3_port.md`, `mode1_port.md` (r02a–r02w).
- Bus A / outputs: `busA_errors_decode.md`, `solenoid_chain_decode.md`, `request_law_port.md`, `timerE_0b_port.md`, `output_cadence_decode.md`/`_port.md`, `feedback_chain_decode.md`, `feedback_shadow_port.md`, `diag_real_feedback_dryrun.md`, `diag_temp_inputs_decode.md`, `diag_temp_shadow_port.md`, `temp_stage2a/2b_port.md` (r02v–r04m).
- Park / range: `park_outputs_decode.md`, `range_logic_port.md`, `park_sensor_decode.md`, `park_position_speed_decode.md`, `m5_reverse_range_decode.md`, `range_shadow_*`, `park_interlock_inputs_decode.md`, `u1267_debounce_decode.md`, `range_chain_live_port.md`, `park_chain_loose_ends_decode.md`, `keyoff_autop_port.md`, `pa7_decode.md`, `c9_bit7_decode.md` (r03h–r04g).
- Engagement engine (M5): `m5_request_producer_decode.md`, `m5_transient_decode.md`, `m5_1_fill_port.md`, `m5_producer_inputs_decode.md`, `garage_shift_decode.md`, `shift_engine_decode.md`, `garage_progress_decode.md`, `garage_shadow_v2_port.md`, `garage_live_plan.md`, `garage_g1_port.md`, `garage_release_decode.md`, `g4a/g4b_*_port.md`, `g6_migration_plan.md`, `g6a/g6b/g6c_port.md`, `b1_e28e_timing_decode.md`, `b2_slots_subobj_decode.md`, `r04q/r04r/r04s_port.md` (r03n–r04s).
- OS mode / line pressure / adaptations: `os_mode_selection_decode.md`, `g5_mode2_plan.md`, `g5b/g5c_port.md`, `mode2_functions_decode.md`, `line_pressure_decode.md`, `line_pressure_torque_decode.md`, `adaptation_decode.md`, `actuator_reactions_decode.md`.
- Shifting (M6): `shift_map_decode.md`, `sm_shadow_port.md`, `m6_shiftmap_gaps_decode.md`, `gear_feedback_shadow_port.md`, `shift_engine_gearpair_decode.md`, `gp_shadow_port.md`, `shift_phase_bodies_decode.md`, `shift_class_counters_decode.md` (r04t–r04z).
- Faults / CAN: `fault_manager_decode.md`, `gear_code_decode.md`, `can_inputs_decode.md`, `fault_reaction_gearcode_plan.md`, `f0_fault_chain_decode.md`, `group28_decode.md`, `group28_port.md` (r04x, r05a).
- Fork: `fork_readiness_audit.md`, `fork_seams_port.md`.
- Decision codes (D-G1…D-G6-15, D-G5-1…9, D-FR-1…9, D-M4-1…9, D-N1/D-N4): search the lab record for the code.

## How to work here

- The user is at the bench and hands-on: they power-cycle on request, watch the ammeter, and know the hardware's sounds and smells. They ask sharp methodological questions — "why are we trial-and-erroring?", "why is the order reversed?" — and the right response is to own the drift and fix the method, not to defend it. They will say "have a subagent X" when they want an agent and "implement it now" when they don't.
- Commits carry full evidence in their messages (see git log — they read like lab notes). Follow that style. Commit when a result is recorded; push only when the user says so.
- When refuted, bank it immediately: append to `docs/lab_record.md`, state what the refutation eliminates, and name the next discriminator. Keep this file's "state now" and "immediate work" current (short).
- **Token economy (user, 2026-11-20):** brief subagents with specific report sections, not "read CLAUDE.md / the lab record"; use Opus for byte-level decodes and first-of-a-kind ports, a cheaper model for pattern ports, test conversions and audits; ask for short final reports; avoid running many agents at once.
- The percentages in README.md's status table move with milestones — update at each one.

## The people

A human/AI pair produced this project (see CONTRIBUTORS.md). Early commits were Claude-assisted; the recent arc is ZCode/GLM and Claude with human direction; you are the next collaborator. The work product discipline matters more than the tool: everything cited, everything reproducible, nothing claimed beyond its evidence grade ([M]/[O]/[I]/[D] — see README's conventions).
