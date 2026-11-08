# 20 — Supervisor walk reconciliation (top-down artifact 5: the persistence model)

> Working artifact of the top-down program (2026-11-08): answers whether the stock
> re-runs the commissioning walk every power-on or skips it on a warm chip. Generated
> from the fully-named codebase (spec 19) with the sweep tooling; original at
> `~/tcu_sweep/reconciliation.md`.

# RECONCILIATION — does the stock re-run the supervisor commissioning walk every power-on?

**VERDICT: WALK-PER-BOOT** — the stock re-runs the walk on every boot; there is NO branch on
the 0x0C level nibble (or any supervisor reply) that skips the 0x06 era or the walk when the
chip reports itself already commissioned/warm. **E60o's premise: SURVIVES.**

Basis: exhaustive static decode [O]. The held bench data contains no time-resolved stock
SPI/RAM capture of the walk (see §3) — the verdict rests on code, which is now fully named
and was swept exhaustively for every read of the level cell.

---

## 1. Static evidence [O] (source A: ~/tcu_sweep/decomp, names from docs/spec/catalog/all_names.csv)

### 1.1 The decisive fact: the 0x06-era request fires before any level can be known

`0x000873F0 fault_manager_poll` (r08_app/0x000873F0.c) — the cyclic monitor that owns the
walk AND the monitored-RAM checksum list (calls `0x0007F1A6 rammon_periodic_check` and the
`0x0007EE8A` checksum walker every pass). Per pass it first calls the walk task, then the
outer FSM `0xFFF95010`:

```c
if ((DAT_fff93b94 == '\0') && (DAT_fff93ba1 < 7)) {   // one-shot ENABLE (not the walk)
    (*(code *)PTR_FUN_000876c8)(1);                    // -> 0x0006C612 park_store_request_state
    DAT_fff93b94 = '\x01';
}
if (DAT_fff93ba3 != '\0') goto LAB_000876f2;           // walk active -> outer FSM parked
...
if (bVar6 != 8) { ... if (bVar6 != 0) { ... }
    // outer state 0:
    cVar7 = (*(code *)puVar1)(&DAT_fff9500e,&DAT_fff94e86);      // boot flag (set by init!)
    if (cVar7 != '\0') {
        DAT_fff9500e = 0; DAT_fff94e86 = 0xff;
        (*(code *)PTR_FUN_000876d0)(5);                          // -> 0x0008D136 mode_req_set(5)
        if (DAT_fff93848 != 0x4b0) {
            (*(code *)PTR_FUN_000876dc)(0x4b0,&DAT_0000fb4f,2);  // -> 0x0006C48C park_pwm_learn_value(0x4b0,...)
        }                                                        // 0x4b0 = 0x06*200 : THE 0x06-ERA REQUEST
    }
    DAT_fff95010 = 8; DAT_fff94e88 = 0xf7; ...
```

Pointer table at 0x0876B0–0x0876E0 read from the ELF (big-endian), resolving every indirect
call: 0x876C4→`0x0006C63E` (walk task), 0x876C8→`0x0006C612`, 0x876CC→`0x0008CEDC`
mode_seq_step, 0x876D0→`0x0008D136` mode_req_set, 0x876D4→`0x0008D1BA` mode_timeout_check,
0x876D8→`0x0008D108` mode_target_reached, 0x876DC→`0x0006C48C` (era writer), 0x876E0→
`0x0008D020` mode_transition_supervise.

Why no skip is even possible here:

- `0x0008C1EA` (the reply processor inside the walk task) is the **only** harvester of the
  level nibble in the whole image (`DAT_fff93ba1 = DAT_fff90550 >> 4 & 7;`) and it is called
  only from `0x0006C63E` states 7/0x19 — i.e. only **after** the walk has been started. At the
  moment outer state 0 issues the 0x06-era request, `0xFFF93BA1` still holds its boot-init
  value **7**. The request is issued with zero knowledge of the chip's level.
- `0xFFF9500E` (the boot flag that arms the request) is set to 1 by `0x00087732
  fault_manager_init` at **every** boot, and re-armed by the state setter `0x00087766`
  whenever the FSM enters states {0, 0x12, 0x23}.
- `0x00087732` → `0x0006C12C park_pwm_init` (its only caller) unconditionally rewrites per
  boot: `DAT_fff94fe8 = 0x3f` (+complement `DAT_fff94e60 = 0xc0`), `DAT_fff93ba2 = 7`,
  `DAT_fff93ba1 = 7`, era counter `DAT_fff94fe6/0x94e5e = 1/0xfe`, period
  `DAT_fff93848 = 0x3138` (= 0x3F*200). Answer to question (c): the walk state is reset by
  per-boot code re-init (plus RAM clear for the state cells `0x93ba3`, `0x95010`), not by any
  retained/warm RAM state and not conditionally.
- `0x00087732` (init) and `0x000873F0` (task) are registered as a pair in an unexported task
  registrar whose literal pool sits at 0x07EE38–0x07EE44 (`{0x00090000, 0x00087A1E,
  0x000873F0, 0x00087732}`, OS service calls 0x46524/0x46538/0x46541); the cooperative OS is
  `0x00067A68 main_loop` / `0x00067B16 os_sched_service_step`, init table 0x66E8C–0x66EA8 run
  by `0x00066DA4 init_tbl_run_timed`. Exact poll period not decoded [I]; era dwell constants
  below are unit-consistent with the 1 ms timer-H tick.

### 1.2 Exhaustive list of every branch on the level nibble — none is a skip

`grep` over all decomp for reads of `DAT_fff93ba1` (the 0x0C high-nibble level cell) yields
exactly 12 branch sites:

| Site | Code | Effect |
|---|---|---|
| `0x0006C63E:35` | `if ((DAT_fff93ba1 < 6) \|\| (DAT_fff90954 == '\0')) DAT_fff800e8 = 0; else {...}` | **Output gate** (certified output / rail-enable telltale), level>=6 AND the 0x22-echo bit0. Not walk control. |
| `0x0006C63E:41` | `if (DAT_fff93ba1 < 7) ... else {DAT_fff93b9a++; if==2 {...}}` | two-pass certification-progress counter after the gate passes |
| `0x0006C1EA:64` | `if ((DAT_fff94997 < DAT_fff93ba1) && (DAT_fff93b99 != 0)) DAT_fff94997 = ...` | max-level latch (bookkeeping) |
| `0x000873F0:44` | `if ((DAT_fff93b94 == 0) && (DAT_fff93ba1 < 7)) { 0x6C612(1); }` | one-shot **enable request** when NOT warm; never fires on a level-7 chip. This is the enable/WDT pattern, not the walk |
| `0x000873F0:53` | `if (DAT_fff93ba1 < 5) { mode_seq_step; stay; } else state=0x34;` | state 0x45 **proceed gate**: a warm chip (7) passes instantly, a cold chip WAITS. Opposite of a skip |
| `0x0008799C:9,21` | `if (DAT_fff93ba1 == 0) {...}` | chooses whether to re-request the 0x1F era (`PTR 0x87A98 -> 0x6C48C(0x1838=0x1F*200)`) or run mode supervision |
| `0x0008CEDC:29` | `if (DAT_fff93ba1 == 0) {...}` | mode_seq_step sub-state force-advance |
| `0x0008D020:39` | `if ((bVar2 == 0) \|\| (bVar2 < DAT_fff93ba1)) {...}` | picks duty re-arm (`0x7EE8A`+`0x6C802`) vs walk restart (`0x6C720`) — both (re)start exchanges; neither idles the machine when warm |
| `0x0008D108` (mode_target_reached) | `level != param` / `<5` / `>4` | target-reached predicate used for sequencing and timeout budget (`0x0008D136`) |

The walk task itself (`0x0006C63E` → era stepper `0x0006C542`, reply processor `0x0006C1EA`,
staging ping-pong `0x0006C742`) contains **no** level branch at all. `0x0006C542` steps eras
whenever the FRT delta exceeds `DAT_fff93848 + 0x960` (period = value*200 + 2400 → 0x06 era
3600 ms, 0x1F era 8600 ms — the decoded ~3.6 s / ~8.6 s dwells) and bumps the *expected*
level toward 7; `0x0006C1EA` compares expected vs reported and flags mismatches (bits in
`0x93BA7`), it never aborts.

### 1.3 The walk chain and the cyclic job

- Outer FSM `0xFFF95010` (in `0x000873F0`): **state 0** (boot, flag-armed) → requests value
  0x06 (period 0x4b0) via `0x0006C48C` → **state 8** → `0x0008D020 mode_transition_supervise`
  → system-mode chain (`0x00085160` → setter `0x00087766`) enters **0x12/0x23** → at 0x23:
  timeout path to 0x34, or `mode_target_reached(0)` (level==0) && `0x0006C48C(0x1838)` →
  **the 0x1F era** → state **0x45** (level>=5 proceed gate) → 0x34 → `0x0008799C
  fault_state_advance`. Value chain 0x3F (init `0x0006C12C`) → 0x06 (state 0) → 0x1F (state
  0x23) — matching spec 03 §6.2bd's decode.
- `0x0006C48C park_pwm_learn_value` writes the value cell `0xFFF94FE8`/`0xFFF94E60`
  (diverse pair) or stages the second byte `0xFFF94FE9`, then `0x0006C0BA
  park_pwm_program_timer` sets `0xFFF93B9B = 7` (level-job mode) and programs the compare.
- `0x0006C1BA park_pwm_service` sends per service tick: `0xFFF93B9B==0` → duty job 0x409F4
  (builder `0x00091086`); else **level job 0x40A14** via `0x00091190(level)` — which stages
  the value at `0xFFF9054E`, ORs it into template word `0x8400|v`, and queues the cyclic
  4-word job `{84|v, 0A, 12, 0C}` (spec 03 line 85, 305–308).
- Completion handlers: `0x000911D8` (level job: echo word→`0xFFF9054F`, level word→
  `0xFFF90550`, commit-status word→`0xFFF90551`; validates all-0xA8xx headers), `0x000910EE`
  (duty job, 7 words), `0x00092D94 shifter_resp_echo` (control words incl. the 0x22 echo →
  `0xFFF907D5`/OK bits). None of them branches on the level to skip anything.

### 1.4 Question (b): persist/store-certification command — none exists [O]

The complete job-descriptor list (0x409F4–0x40C74, 16 jobs; spec 03 §4.3 table, templates in
ELF flash) contains write opcodes 88 (duty), 84 (level), 8E (SSLB1 control), C9 (SSLB0
control), 90 (output enables) only. **Every** register-write job ends with the commit word
`0x1200` (spec 03 lines 305–316) — cyclic, riding each job, not a one-shot "remember this"
command. There is no store/persist/flash opcode anywhere in the job table and no code path
issuing a one-shot certification-store job. If the chip remembers, it self-persists [I];
bench evidence supports chip-side memory: the supervisor "powers up at level 0x1F, already
engaged-ready" (spec 03 §6.2j, line 1053) and keeps latched state across application reboots
while powered (spec 03, 2026-10-31 note, line ~286).

## 2. What this means for warm (level 7 at boot)

The stock never consults the chip's level before starting the walk; the 0x06 era is armed on
the first monitor pass of every boot. The only warm-sensitive behaviours are: the one-shot
enable request is *not* sent when already at 7 (the chip doesn't need it), the 0x45 gate is
passed instantly, and the 0x1F-era request at state 0x23 carries a `mode_target_reached(0)`
= level==0 condition — i.e. on a boot-warm chip the TCU first requests mode 0 (drag the chip
down) and requests 0x1F once the chip reports 0. Net: a warm chip is walked *through* the
same 0x3F→0x06→0x1F sequence, if anything with different era-2 timing. Residual unknown [I]:
the exact era-2 timing on a boot-warm chip (chip-side level dynamics between 7 and 0 during
the 0x06 era); does not affect the verdict.

## 3. Capture evidence (source B) — what is actually held

- `/home/chadk/Documents/REA Projects/bench_logs/20261017_stock_first_power.log` (44 KB,
  candump -L, stock, no vehicle): 1001 frames over **3.99 s**, 10 IDs (can0 037/2F9 ~50 Hz,
  144/170/1A0/2A8 ~25 Hz, 178 x18, 3D1 x3, 639 x2; can1 300 ~50 Hz). **No 0x361/0x398
  measurement-protocol frames at all**; cyclic traffic steady from t=0.03 s with idle
  payloads per spec 08 (lines 50–53). The bus-B walk is SPI and invisible here — this log
  cannot address the question. README: "first frames ≈ 15 s in, about 5 s of traffic".
- Engaged-stock supervisor shadows, three live captures 2026-10-31 (RAM via diag cmd 0x2A;
  spec 03 lines 241–247): steady-state engaged cells only (`0xFFF907D4`=0x91/0x81,
  echo 0xFFF907D5=0xb1/0xa1, `0xFFF90953`=1, `0xFFF90954`=1, letters 'c', `0xFFF907D6`=0x20)
  — **no 84-walk-era words (84|06) quoted anywhere; single engagement session, not multiple
  power-ons.**
- Boot-era capture (spec 03 §6.2i, lines 1007–1013, 2026-11-05): the closest thing to
  time-resolved stock — meas-protocol RAM dump from power-on, **first read at boot +15 s
  already fully engaged** ("write job at full 0x8E9F from the first capture (no level
  ramp)", `0xFFF90953=1`), stock commissioning "~15 s end to end". The first sample lands
  after the walk would have finished (3.6+8.6 s + overhead ≈ 12–13 s; also consistent with
  the E13 enable-tolerance band ≈ [+0.5 … +15 s], line 1026). Consistent with WALK-PER-BOOT
  but not resolved through the eras.
- **No logic-analyzer SPI capture of the stock exists** (spec 03 line 288 names it "the
  decisive next step"; §6.2be lines 2787–2796 plan the observation run — reading the stock's
  own commissioning cells (0xFFF9054E, 0xFFF90551, 0xFFF93BA1, letters 0xFFF90944) live via
  meas through the first ~15 s — as still to be done).
- `tools/bench/session_scripts/bench_status_*.txt`: replica/experiment session logs only
  (e.g. bench_status_20261105.txt:8 references `0xFFF90953/0xFFF93BA1` of *our* firmware).
  No stock-run RAM reads are held.

Honest statement: **no time-resolved stock SPI or RAM capture of the walk exists in the held
data.** The verdict is static-decode-based; the planned 6.2be observation run (stock
restored, 0x2A RAM dumps at ≤1 s cadence through the first 15 s, watching 0xFFF94FE8 /
0xFFF9054E / 0xFFF93BA1) would give the wire-level confirmation and settle the era-2 timing
residual.

## 4. Confidence

- [O] Every code fact above (functions, branches, pointer-table resolutions from the ELF,
  job templates, per-boot init writes) — observed in the decompile/ELF/spec tables.
- [O] Absence claims ("no warm-skip branch", "no persist job") are exhaustive greps over the
  fully-exported decomp (all readers/writers of 0xFFF93BA1, 0xFFF93BA3, 0xFFF95010,
  0xFFF94FE8, 0xFFF90550 enumerated) plus the complete job-descriptor table.
- [I] Monitor poll period (registrar not decoded); era-2 timing on a boot-warm chip;
  supervisor self-persistence across full power-off (bench chip powers up at 0x1F).
