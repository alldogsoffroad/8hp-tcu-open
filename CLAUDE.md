# CLAUDE.md — project handoff

You are joining the 8hp-tcu-open project mid-flight. This file is your orientation: what this is, where it stands, how work flows here, and the rules that exist because breaking them cost real bench time. Read it fully before touching anything. The git log's commit messages are detailed engineering records — the project's real history lives there and in `~/tcu_sweep/`.

## What this is

An open C reimplementation of the firmware for the Bosch transmission control unit of the ZF 8HP (P/N 1034420267, Renesas SH7254R, SH-2A big-endian, superloop, no RTOS). The stock firmware is fully decompiled (10,982 functions, Ghidra sweep, specs 19–26 in `docs/spec/`); our rewrite lives in `firmware/` and boots, walks, and converses with the supervisor chip on the real bench. The goal: **raise the solenoid outputs to the engaged rail (~1.77 A) under open-firmware control** — the stock does it in under half a second from power-on; our firmware has not yet been granted it. Everything else (M5 limp, M6 shifting, M7 Nissan integration) follows the rail.

## The one-sentence state (as of 2026-11-11 close)

The bench runs r02a; the walk tick is alive at stock cadence; the supervisor chip's certification gate (`0x14` bit 1) has refused six wire-law hypotheses; the mechanical transcript tool caught that our per-pass word SEQUENCE was reversed vs the stock — fixed in commit f716fd7 — and five smaller deltas are enumerated. **The chip's gate is flash-reachable (the stock opens it every boot on this same bench) — never accept "the wire can't do it."**

**CORRECTION (2026-11-11 evening, supersedes the earlier close):**
- **r02a is in a restart loop, and has been since it was flashed** [O]. Passive 0x7F0 capture (90 s, 919 frames): every frame strictly alternates boot-init (`val=3F walk=00 mode=00`) / just-armed (`val=06 walk=07 mode=07`), never a third frame from one boot — app life 105–205 ms. `s_now_ms` reads alias at a ~196 ms period. The restart lands within ~10 ms of the silence window releasing [I]. `s_wovf_count` flat at 50 across ~170 restarts and no exception-announce frames — not the MCU WDT, not a CPU exception; the supervisor's reset line is the lead [I].
- **"The reset loop is dead" was wrong for r02a; the silence-shape "refutation" was not clean** (the chip never got a steady conversation); **the "FSM-advance wedge" does not exist** — on the host the FSM leaves supervise at 3.705 s; DUTY=0 on r02a was the restarts wiping the counters. The r02a witness log already showed it: 51 of 90 rounds at init values (`st84=00 REQ=00 r14=00`).
- **The stock has NO batch silence** [O, decode]: `0x8EE82` is entry 26 of the 5 ms task's mode-0 list (row 7 col 0, list `0x42CD8`, task object `0x41378`); the OS tick (CMT 2.5 ms, `0x91A30(0x186A,0x19)`) starts on the 2nd main-loop pass (`0x7DAA4` → `0xFFF93B8C` → `0x6F144` → `0xFFF93B40` → `0x67A7C`). The state-3 gate `0x67B16` holds only the background list (row 0). Spec 21's "+95–105 ms" row is the first variant-bank pass, not the batch start. `first500ms_diff.md`'s `0x45D24…`/`0x7FE40` are ELF file offsets — flash addresses are `0x44D24…`/`0x7EE40`.
- **r02b (f716fd7 as built) must not be flashed** — it keeps the silence gate and would restart identically. **r02c** = pool-order fix + dwell stamp, batch silence gate removed (monitor lead-in unchanged): built, `build/r02c_IFla.bin`, NOT flashed.
- **The heartbeat's uptime bytes always read 0** on these builds (`s_tick_count` is not advanced on the CMT path) — use the cell bytes as the restart detector.
- **Open, undecoded:** in modes 1–3 the stock's batch is absent from the 5 ms lists and sits in the 10 ms lists (row 6, entry 76 of 91) — if the unit leaves mode 0 the stock's batch cadence is 10 ms, ours is 5 ms forever. Which mode the bench unit reaches, and when, is not decoded. The monitor `0x873F0` start time is still [I].

## The rules (each one earned)

These are the user's standing directives. They are not suggestions.

1. **No flash without a prediction.** Before staging any bench flash, derive and state whether it will succeed from evidence already in hand. If you can derive failure, do the desk work instead. Bench flashes are the scarce resource. (Stated 2026-11-11 after I staged a flash whose failure was derivable in two sentences.)
2. **Decode before drive.** A flash executes a decoded recipe, never a guess. Ghidra/disassembly/session-log evidence first.
3. **Inventory logged data before new runs.** Before proposing a bench run for information, assemble the answer from existing captures/dumps/decodes. The user challenges redundant runs — rightly.
4. **"Stock-shaped" ≠ decode.** Verifying that words/parameters match the stock is not verifying behavior. Sequence, timing, mechanism, and count must each be verified against the stock's actual bytes. Two session-losing errors (the silence window, the pool order) were "words right, structure unverified."
5. **User bench truth over theory.** The user's physical observations are ground truth. Label inferences as inferences. Correct records immediately when refuted.
6. **Bench protocol:** the wake storm is armed ONLY against a dark bus (arming against a live board = instant false catch); storm and the install chain run as SEPARATE background tasks; one process per serial port (the meas/witness/ammeter tools conflict); `meas 0x02` (and verify the reply) before every read session — the periodic 0x398 stream races the meas replies and garbles reads; the ammeter never co-runs with the storm/install; announce any current-moving event before it happens; power cycles are the user's physical act — off, then arm, then on.
7. **Preserve displayed messages and written flash.** Never rewrite history the user has seen; diagnostics go in prose and the status file.
8. **Subagents for deep decodes; you validate before flashing.** The pattern that works: brief an agent with the full evidence chain and corpus pointers, validate its commit (build, tests, CRC — run them yourself), then apply. The user will tell you when they want an agent vs. direct work — "we've already logged and identified this; implement it now" means do it yourself, now.

## Where everything lives

- `docs/spec/` — the specification, specs 1–26. Specs 19–26 are the function catalog and top-down syntheses (chronology, the 14-job supervisor protocol, walk reconciliation, latch conditions, chip ID). **Spec 21 §1 is the power-on chronology — the boot truth table.**
- `firmware/` — the open reimplementation. Platform (`platform/sh7254r/`), supervisor service (`bsw/sup/`), the app (`app/main.c`). Host build + tests: `build/host` (21/21 must pass; the transcript tool is a test). Target build: `build/sh2a`.
- `firmware/tests/unit/transcript.c` — **the byte-identity verification tool**: drives 100 passes, prints every bus-B word. Any sequence drift fails the build. Extend it; it is the project's most valuable new instrument.
- `~/tcu_sweep/` — the decode corpus: `first500ms_diff.md` (the stock's opening, word by word), `boot_jump_law.md`, `gate_timeline_mining.md`, `walktmr_map_fix.md`, `duty_wedge_trace.md`, `w1_wedge_fix.md`, plus the 47-unit Ghidra sweep. Read `first500ms_diff.md` first.
- Bench tools: `~/tcu_bench_tools/` (watch.py passive heartbeat, amps.py ammeter → /tmp/amps.log, chatter_watch.py). Install chains: `/tmp/install_*.py` (regenerate per flash — see the sig-table law below). Measurement reads: `tcu_bench.meas(b,1,[0x2A,0,addr...])` via `tools/bench/tcu_bench.py`.
- The stock reference images: `~/Downloads/260924131233 1034420267_IFla.bin` (pristine September read-out — the restore source). Our images: `build/*_IFla.bin`.

## The bench mechanics (established laws, don't relearn them)

- **Install:** `tools/build_image.py --app build/sh2a/tcu_app.hex --base <previous>_IFla.bin -o build/<new>_IFla.bin --stock-ident --gap-keep-above 0x50000`. The sig table for the install script = `/tmp/r01h_sigs.json` (the learned table) with `0x40000/0x10000` set to `None` (force-write). **The skip check means "live == TARGET sig"** — feeding it current-flash sigs writes nothing (cost one wasted cycle). After each install the chain learns the new block sig back into the table.
- **The flash cycle:** user says "off" → arm storm + launch install (two background tasks) → user says "on" → storm catches → install writes (~4 min) → user "off"/"on" again → witness reads.
- **Cell addresses:** nm `build/sh2a/tcu_app.elf` every time — never trust remembered addresses (two wrong-struct-read incidents). The cell map is `firmware/bsw/sup/sup_cells.h`, pinned at stock RAM addresses.
- **The walk timer is a DOWN COUNTER** (DCNTD00 @0xFFFFF5B0, vector 224 = underflow): load `0xB000` (~4.5–4.95 ms at its ~9–10 MHz clock), then strobe DSTRD0 @0xF585. Never a compare register — that misread cost a day.
- **The tamper wall:** forcing the letters arm past the chip's selftest gate triggers a latched anti-tamper reset loop (~1.15 s period, until power-off). The interlock in `sup_letters_bench_arm` refuses while the gate is closed — never bypass it.
- **The stock reference measurement:** genuine stock → rail at **<0.5 s** from power-on; by the first readable moment (+13 s): `0x14=C7`, gate=1, letters='c' earned, EN90=0x20. The chip is healthy; the bench is rail-capable; the stock earns certification in its first ~150–250 ms.

## The immediate work (open, in priority order)

1. **Decode and close the five enumerated transcript deltas** (from the tool's output, f716fd7): (a) the FLT re-fire count/position (`0x14&0xE0` law on our faulted D5 chip — is a second FLT per pass stock-true, and where?); (b) the walk words' interleave position (ours queues inside the batch; the stock's rides the tick context); (c) J7's slot; (d) the ask rides ~2–3 passes late (ours P25, stock ~P22 — the one-shot latency chain); (e) the duty token bytes (ours FF/0F/F0/00 vs the stock dump's 01/F1/41/B1 — verify the token law: same selector → same token?).
2. ~~Fix the FSM-advance wedge~~ — **closed, no wedge** (see the CORRECTION above: r02a was restarting). **Replaced by:** decode the stock's mode switch (`0xFFF93B33`/`0xFFF93B34`, requester `0x6F0B4`) and the batch's 10 ms cadence in modes 1–3 before deciding to flash.
3. **Then the flash, with the stated prediction:** r02c is the first sequence-true build that can hold a conversation. Prediction: no restarts (r01y's batch-from-boot opening ran 294 s steady); the FSM leaves supervise at ~3.7 s and DUTY climbs (host trace); **the gate: no prediction** — first bench test of the pool order. Witness with the heartbeat cells first (strict init/armed alternation = restarting). If the echo `0A` never matches the staged value, expect the confirm-timeout restart to flip the walk back to level mode ~400 ms into the duty era (host trace; a port issue to fix if seen).
4. Deferred: solenoid setpoints at engagement (spec 12/E13 — an interactive decision with the user), the 0x398 read-race root cause, DCSELDn0 field decode, D7 J1_ERR map.

## How to work here

- The user is at the bench and hands-on: they power-cycle on request, watch the ammeter, and know the hardware's sounds and smells. They ask sharp methodological questions — "why are we trial-and-erroring?", "why is the order reversed?" — and the right response is to own the drift and fix the method, not to defend it. They will say "have a subagent X" when they want an agent and "implement it now" when they don't.
- Commits carry full evidence in their messages (see git log — they read like lab notes). Follow that style.
- When refuted, bank it immediately: update the project-state record, state what the refutation eliminates, and name the next discriminator. Clean refutations are progress here — seven of them narrowed the gate hunt from "everything" to five enumerated deltas.
- The percentages in README.md's status table move with milestones — update at each one.

## The people

A human/AI pair produced this project (see CONTRIBUTORS.md). Early commits were Claude-assisted; the recent arc is ZCode/GLM with human direction; you are the next collaborator. The work product discipline matters more than the tool: everything cited, everything reproducible, nothing claimed beyond its evidence grade ([M]/[O]/[I]/[D] — see README's conventions).

*Prepared 2026-11-11 at the close of an eleven-flash day: the tick alive, the resets dead, the tamper wall mapped, the sequence caught. The rail is one honest byte away.*
