# CLAUDE.md — project handoff

You are joining the 8hp-tcu-open project mid-flight. This file is your orientation: what this is, where it stands, how work flows here, and the rules that exist because breaking them cost real bench time. Read it fully before touching anything. The git log's commit messages are detailed engineering records — the project's real history lives there and in `~/tcu_sweep/`.

## What this is

An open C reimplementation of the firmware for the Bosch transmission control unit of the ZF 8HP (P/N 1034420267, Renesas SH7254R, SH-2A big-endian, superloop, no RTOS). The stock firmware is fully decompiled (10,982 functions, Ghidra sweep, specs 19–26 in `docs/spec/`); our rewrite lives in `firmware/` and boots, walks, and converses with the supervisor chip on the real bench. The goal: **raise the solenoid outputs to the engaged rail (~1.77 A) under open-firmware control** — the stock does it in under half a second from power-on; our firmware has not yet been granted it. Everything else (M5 limp, M6 shifting, M7 Nissan integration) follows the rail.

## The one-sentence state (as of 2026-11-11 close)

The bench runs r02a; the walk tick is alive at stock cadence; the reset loop is dead; the supervisor chip's certification gate (`0x14` bit 1) has refused seven wire-law hypotheses, all cleanly refuted; the mechanical transcript tool just caught that our per-pass word SEQUENCE was reversed vs the stock — fixed in commit f716fd7 (r02b, built, not yet flashed) — and five smaller deltas are enumerated for the next session. **The chip's gate is flash-reachable (the stock opens it every boot on this same bench) — never accept "the wire can't do it."**

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
2. **Fix the FSM-advance wedge:** DUTY=0 for 283 s on r02a — the advance past supervise still never fires despite the dedicated dwell stamp (r01z's `s_sup_dwell_base`). Diagnose on the desk (the transcript tool will show it — walk the FSM in the tool and find where the advance stalls).
3. **Then the flash, with the stated prediction:** r02b+ is the first sequence-true build. Prediction to state and test: the gate — `CELL_SSLB0_BIT2` @0xFFF90959 → 1 within ~2 s of release, letters earned within ~4 passes, then GRANT/`0x8E91`/0x24 verdict/the rail. If D5 again: the five deltas were the remaining wire divergence — close them and re-derive.
4. Deferred: solenoid setpoints at engagement (spec 12/E13 — an interactive decision with the user), the 0x398 read-race root cause, DCSELDn0 field decode, D7 J1_ERR map.

## How to work here

- The user is at the bench and hands-on: they power-cycle on request, watch the ammeter, and know the hardware's sounds and smells. They ask sharp methodological questions — "why are we trial-and-erroring?", "why is the order reversed?" — and the right response is to own the drift and fix the method, not to defend it. They will say "have a subagent X" when they want an agent and "implement it now" when they don't.
- Commits carry full evidence in their messages (see git log — they read like lab notes). Follow that style.
- When refuted, bank it immediately: update the project-state record, state what the refutation eliminates, and name the next discriminator. Clean refutations are progress here — seven of them narrowed the gate hunt from "everything" to five enumerated deltas.
- The percentages in README.md's status table move with milestones — update at each one.

## The people

A human/AI pair produced this project (see CONTRIBUTORS.md). Early commits were Claude-assisted; the recent arc is ZCode/GLM with human direction; you are the next collaborator. The work product discipline matters more than the tool: everything cited, everything reproducible, nothing claimed beyond its evidence grade ([M]/[O]/[I]/[D] — see README's conventions).

*Prepared 2026-11-11 at the close of an eleven-flash day: the tick alive, the resets dead, the tamper wall mapped, the sequence caught. The rail is one honest byte away.*
