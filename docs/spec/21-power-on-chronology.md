# 21 — Unified power-on chronology (top-down artifact 1)

> The stock's complete T+0–T+10 s event schedule from the fully-named codebase (spec 19). Generated 2026-11-08; original at ~/tcu_sweep/artifact1_chronology.md.

# ARTIFACT 1 — Unified power-on chronology, stock firmware (Bosch 1034420267, SH7254R, SH-2A big-endian)

Scope: T+0 (release of reset) through T+10 s, one document. Sources: static decode
(`~/tcu_sweep/decomp`, names from `docs/spec/catalog/all_names.csv`, edges.csv, task-bank
dumps from the flash image `260924131233 1034420267_IFla.bin`), specs 02/03/05/06/07/08/12,
and the held bench evidence (1 kHz switch-on current capture 2026-11-05, candump
2026-10-17, live 0x2A RAM dump 2026-11-04, reconciliation.md). Confidence marks:
**[O]** observed in code/binary or bench; **[D]** derived by arithmetic from [O] constants;
**[I]** inference / ordered-but-untimed.

Headline corrections to the assumed order (details in §5):
1. The "init dispatcher" 0x000DA418 is **not** a boot init — its real entry 0x000DA388
   `cfg_variant_apply` runs lazily from the **first cyclic application pass**
   (0xA2D5E/0xA2EBC → 0xB912C `meas_ram_refresh_task` → 0xDA388), i.e. after the scheduler
   is already running. Ghidra split the function mid-body; 0xDA418 has no caller and no
   literal-pool reference anywhere in the image.
2. The solenoid leap (+200–250 ms) happens **during** the commissioning walk's 0x06 era
   (which runs ≈ T+0.25 s → T+3.85 s): drive is **not** coupled to walk completion.
3. The supervisor rail is serviced **from T+0 by the boot software** (5 ms words,
   bootloader 0x1D0B4 / boot manager 0x30E40) and handed to the app gap-free; the app's
   own 5 ms batch 0x8EE82 only takes over at handover (~T+0.2 s).
4. The output-enable bit-0 one-shot fires at ~+105 ms **inside the app**, before the leap
   and independent of it (pass ≈20–21 of the 5 ms task).

---

## 1. THE TIMELINE (T+0 → T+10 s)

| Time / order | Event | Subsystem | Function(s) (addr `name`) | Trigger | Conf |
|---|---|---|---|---|---|
| T+0 | Reset released; CPU starts in the **user boot MAT** (32 KB, not dumped). Reads reset-cause mailbox 0xFFFC0D00, checks boot-request record at 0xFFF80080 (magic 0xA55A/0x5AA5, len 0x48, src/tgt) | boot MAT | (not in dump); record check paths `0x00024A26` [I], `0x0003D6E6 boot_handover_final` | hardware reset | [I] |
| T+0..~ms | *(only with plant tool on bus)* user-boot MAT answers the encrypted power-up session: `7E1#D7 65 69 15` challenge → reply on 0x7E9 within 0.4 ms | boot MAT CAN | not in dump (const absent from image) | tool wake-up frame | [O-spec06§5b] |
| T+0..~ms | Boot-side clock/watchdog constants: `0x00024144 bootmgr_clock_init_wrap` → `0x0002230A bootmgr_clock_mode_init` (called with 0x2F0 on the no-record path); boot WDT programmed `0x0001DBC8 boot_wdt_program` (WRCR=0x5A80, WTCR=0xA567 → first overflow ≈59.8 ms) | clock/WDT | as listed | MAT start path | [O][D] |
| T+0 (order 1) | Two-image validation + final handover: `0x0003D6E6 boot_handover_final` validates app+cal headers (two PTR calls must both return 1); if record magic 0xA55A/0x48/src2/tgt2/trailer 0x5AA5 at 0xFFF80080 → run RAM helper entry instead; clears marker pages 0x187F00/0x1BFF00; **tail-jumps to the application entry read from the validated block chain** (app header +0x7C = 0x00066CA8) | boot manager | 0x0003D6E6 | MAT, no pending record | [O] (jump itself in Ghidra-lost tail [I]) |
| ≈T+0..0.2 s | Boot software **services the external supervisor every 5 ms from T+0**: `0x8E00|bits` to RSPI-B/SSLB1, bit 2 toggled per call (bootloader `0x0001D0B4`, boot manager `0x00030E40 boot_periodic_supervise`; suspended during flash ops by 0x139FE). **The rail never drops across the whole power-on.** | supervisor (RSPI-B) | 0x1D0B4 / 0x30E40 | boot 5 ms scheduler | [O] |
| (boot, if record targets boot) | Bootloader path instead: `0x00010644 bootloader_entry_main` init chain (§2), then diag scheduler `0x00010E58 boot_sched_loop`; record write/read/clear `0x00011E34/0x00011EA6/0x00011EBE` | bootloader | as listed | record target 1/2 | [O] |
| **≈T+0.2 s** (bench: app alive & driving by 0.25 s; our-image boots take 3–7 s, stock is 10–30× faster) | **Application entry 0x00066CA8**: SR.I←0xF, VBR←0x40070, then the 19-call init sequence of §3.1 | app bring-up | 0x00066CA8 (header +0x7C) | boot tail-jump | [O] (order), [O-bench] (duration class) |
| order 3.1-5 | Peripheral full bring-up + register constants + **first watchdog feed** (`0x00095408 wdt_kick`: WTCNT=0x5AB7, WTCR=0xA567; WDT IPR05 nibble 0xE) | timers/WDT | 0x9533C, 0x950A0, 0x95408 | app entry | [O] |
| order 3.1-6 | RAM self-test ranges + RAM data init (.data copy) + reset-cause handshake start (`0x00067200` reads 0xFFFC0D00: 9/10→state 0x55, 0xD/0xE→0x56; writes 0x1234 to 0xFFFC0D04) | RAM / boot cause | 0x676A4, 0x67740, 0x67200 | app entry | [O] |
| order 3.1-14 | C-runtime static-init array walk `0x0009A89C crt_init_array_run` (0xC-byte records, fn@+4) | C++ ctors | 0x9A89C | app entry | [O] |
| order 3.1-17/20 | Boot-cause handshake advance+wait: `0x0006728E` (mailbox 0xFFFC0D04 → 0x5555/0x3737, states 0xAA/0xAB) and `0x00067384` (waits on 0xFFF90AA8, budgets 4000/8000 units) | monitor mailbox | 0x6728E, 0x67384 | app entry | [O] (budget units [I]) |
| order 3.1-21 | **SR.I lowered 0xF→0xE** (maskable interrupts below level 14 enabled) | CPU | 0x66CA8+0x92 | after init calls | [O] |
| order 3.1-23 | `0x00066DA4 init_tbl_run_timed` (§3.2): CPU-load calibration (0x67F66, 20000-iter idle loop) → crt_ram_init → overrun cfg → **OS init task bank** (§3.3) → records init duration (FRT delta) to 0xFFF90A50 → `0x00067A68 main_loop` | OS bring-up | 0x66DA4, 0x67A68 | app entry | [O] |
| within init bank (order §3.3 arr[1]) | ~30 lib/module inits incl. EEPROM table registration (0x6BC90), **fault-manager registrar 0x7EE34** → thunk into `0x00087732 fault_manager_init` (arms boot slot: 0xFFF9500E=1, walk state re-init via 0x6C12C; registers cyclic 0x873F0, pool 0x7EE38 {0x90000, 0x87A1E, 0x873F0, 0x87732}) | fault manager | 0x7EE34→0x87732 | OS init bank | [O] |
| within init bank (arr[2]) | **Tick-table registration** `0x0006E804 init_chain_e804`: installs the two 20-byte-entry cyclic tables 0xFFF9099C / 0xFFF90A3C that the 1 ms tick ISR walks | scheduler | 0x6E804 | OS init bank | [O] |
| within init bank (arr[4]) | CAN/Diag init: `0x0006F26C can_rx_init_timeouts` (timeouts 6000), `0x0006CAD4 diag_register_handlers` (UDS), `0x0006D9FC` | CAN/diag | as listed | OS init bank | [O] |
| OS phases 0→1→2→3 | OS state machine (0xFFF93B32): 0→1 `0x00067A7C` (gated by 0xFFF848E6), 1→2 `0x00067AD4`, 2→3 when 0xFFF848CF set (CPU-load window 0x67FBE, cal word 0x53AC); phase dwell countdowns `0x00067DF0` | OS phases | 0x67A7C/0x67AD4/0x67DF0 | main loop | [O] (dwell values [I]) |
| **1 ms cadence, from first tick-arm** | Tick ISR (vector 228 = **0x000677F8 os_sched_run_due**): walks tables 0xFFF9099C/0xFFF90A3C (20-byte entries, min-period due entry), hands work to `0x000934AC svc_tbl_request_dispatch` → per-type ring buffers; tail re-arms compare 0xFFFFF5D0 from calibration rate table (0x66468 → 0x19xxxx per bank); `0x0006791A os_timer_rearm` sets IPR(0xFFFE0C0E)=6 | scheduler tick | 0x677F8, 0x934AC, 0x6791A | timer H 1 ms (spec 02: 4 MHz, compare 4000) | [O] |
| **first cyclic app pass ≈ +95–105 ms into app life** (spec 03 §6.2aw; i.e. ≈T+0.3 s absolute) | First application task-array pass (variant banks): `0x000A2D5E seq_2d5e` → **`0x000B912C meas_ram_refresh_task`** → (first run) `0x000DA388 cfg_variant_apply` (the "0xDA418 init dispatcher": fan-out init lists, rebinds ~70 pointer tables, ~200 solenoid-band inits 0x106CD4–0x107AC8) and **`0x000BAEB4 timer_cycle_delta_fanout`** (~20 module tick slots, 10 ms granularity) | app tasks / variant | 0xA2D5E, 0xB912C, 0xDA388(+0xDA418), 0xBAEB4 | OS cyclic arrays / rings | [O] |
| from first 10 ms pass | **SPI driver config job commissions the 4 driver chips** (config job before bit-0: "config from the first 10 ms pass"); cyclic 45-frame job is **DMA-launched**: SPCR=0xF8, DMAC count 45, TX buffer 0xFFF88518, launcher `0x00096468`, A-DMAC config `0x000980E0`, burst-end vector 445 (0x40764→0x096590); RSPI ISRs vectors 424–476 (e.g. 424→0x95E8A, 476→0x95A4C); **both RSPI buses interrupt-driven, IPR27 nibbles 4/4** | solenoid SPI (RSPI-A) | 0x96468, 0x980E0, 0x924B0 (45-frame builder) | first cyclic pass | [O] |
| from handover, every 5 ms | App supervisor batch `0x0008EE82 sched_batch_f` (7 jobs: 0x91C86 `power_state_update` — control word 0xFFF907D4, bit-2 complement toggle, bit 0 = output enable —, 0x90B98, 0x91E12, 0x90ACC, 0x90D58, 0x91DAA, 0x91FF8, +0x921A0/0x92A84); init wrote enable cells 0xFFF90953=0, 0xFFF9094A=1 (`0x91DD2 serial_rx_init`) | supervisor (RSPI-B) | 0x8EE82, 0x91C86 | 5 ms task | [O] |
| **≈T+105 ms into app life** (mirror pass ≈20–21 × 5 ms) | **Output-enable one-shot**: `0x000873F0 fault_manager_poll` first pass: `if (0xFFF93B94==0 && 0xFFF93BA1<7) → 0x0006C612 park_store_request_state(1)` → 0xFFF9094A=0, 0xFFF90953=1 (enable requested); bit 0 rides the next 5 ms word via 0x91C86; echo bit0 grants; `0x000920A4` switches own WDT to interval mode (IPR05 low nibble 0xE). NOT engage-coupled | enable path | 0x873F0→0x6C612→0x91C86; 0x920A4 | first monitor pass | [O] (+105 ms [O-bench E60i: pass≈21]) |
| ≈ same first passes | CAN cyclic-tx groups armed (init `0x00094994 can_cyclic_group_init` from ROM template; slot maps `0x00094062`; group HW `0x0009838C` reset+IPR5+bitrate+32 mailboxes; pools `0x000788E4`); first periodic frames **after** init, schedule unit **15.625 ms (1/64 s)** × {1,2,10,50}; live rate table 0xFFF909C0 (7×20 B records, CAN-ids 144/170/1A0/2A8/178/3D1/300, type=rate divisor of ≈104 Hz base) | CAN tx (bus 1/2) | 0x94994, 0x94062, 0x9838C, 0x93FE0+ scheduler | tick-derived schedule | [O] (absolute first-frame time [I], see §6) |
| **T+0.20 s** | **Output rail leaps** (1 kHz current capture: logic ~190 mA until 0.20 s; 1225 mA bin at 0.20 s; **1910 mA fully engaged by T+0.25 s**). Mechanism: park arbiter target 8/0x17 → ~100 ms fill phases → supervisor letters 0→0x21 → records → cyclic words step | solenoid drive / park | park module 0xE7000–0xE9200 (0xE88FA outputs); switch-output manager 0x85828/0x8C7A8 | enable (from +105 ms) + park arbiter | [O-bench], decode [O] |
| ≈T+0.25–0.30 s | **Commissioning walk era 1 (0x06) request issued**: first `0x000873F0` pass with boot flag 0xFFF9500E (set by fault_manager_init) → state 0 → `0x0008D136 mode_req_set(5)`, `0x0006C48C park_pwm_learn_value(0x4B0)` (0x4B0 = 0x06×200); level job 0x40A14 via 0x91190 rides the cyclic RSPI-B conversation; park task `0x0006C63E`, era stepper `0x0006C542` | supervisor walk | 0x873F0, 0x6C48C, 0x6C542, 0x6C63E | first monitor pass, every boot (WALK-PER-BOOT) | [O] |
| T+0.6–0.95 s | Control-transition dip #1 (~1620 mA) | drive strategy | (unattributed) | bench | [O-bench] |
| T+1.15–1.75 s | Control-transition dip #2 (~1550 mA) | drive strategy | (unattributed) | bench | [O-bench] |
| ≈T+3.85–3.9 s | **Walk era 1 dwell expires** (FRT delta > period+2400: 0x4B0+0x960 = **3600 ms** [D]); outer FSM 0xFFF95010 state 8→0x12/0x23; at 0x23 with level==0: `0x0006C48C(0x1838)` → **era 2 (0x1F) request** (0x1838 = 0x1F×200) | supervisor walk | 0x6C542, 0x8D020, 0x6C48C | FRT dwell | [O][D] |
| T+2.1 s onward | Steady engaged hold 1875–1890 mA (bench) | drive | — | bench | [O-bench] |
| ≈T+12.2–12.5 s (beyond window) | Era-2 dwell expires (0x1838+0x960 = **8600 ms** [D]); state 0x45 (level≥5 proceed gate: warm chip passes instantly) → 0x34 `0x0008799C fault_state_advance`; walk end-to-end ≈12.5 s ("~15 s" first-sample bench) | supervisor walk | 0x8799C, 0x8CEDC | FRT dwell | [O][D] |
| continuous | CPU-load monitor in main loop `0x00067FBE` (cal gate 0x53AC; max-tracking 0xFFF935C2/0xFFF935C0); overrun instrumentation in `0x00067BE4 sched_run_task_arrays` (threshold 0xFFF90A8C); WDT fed cyclically (0x95408; 30.3 ms headroom per service) | OS | 0x67FBE, 0x67BE4 | main loop / 5 ms | [O] |

---

## 2. Boot-side init order — bootloader entry `0x00010644 bootloader_entry_main` [O]

Literal pool 0x106CC–0x10708 resolved from the image; call order as decompiled:

| # | Call | Name | What it does |
|---|---|---|---|
| 0 | regs | — | 0xFFFE1200 bit0←0; 0xFFFE6000←0 (DMA off); 0xFFFE080E←0xE000 |
| 1 | 0x0001DAF4 | lib_empty_hook | nop |
| 2 | 0x0001DAFC | boot_periph_init_full | full 0xFFFE/0xFFFF peripheral bring-up |
| 3 | 0x0001D9C8 | boot_periph_reg_init | peripheral init constants (0xFFFF08xx/A8xx) |
| 4 | 0x0001DBC8 | boot_wdt_program | watchdog regs, key protocol 0xA5/0x5A |
| 5 | 0x0001CFF0 | boot_standby_enter | gate clocks D000/D800/E000.1 |
| 6 | 0x00010986 | boot_scatter_load | readback init, zero 0xFFF84000+0x3400, scatter copy |
| 7 | 0x0001DA24 | boot_detect_hw_variant | variant detect at 0x80100001-6 |
| 8 | 0x0001F394 | boot_init_table_run | runs 0xC-stride fn-ptr init table |
| 9 | 0x0001D034 | boot_standby_exit | ungate clocks, run init callbacks |
| 10 | 0x000109F4 | boot_selftest_crc_check | periph RAM tests (0xFFFFD000/D800/E000) + region integrity; error codes 0x218/0x219 |
| 11 | 0x0001D068 | boot_pwr_mode_change | mode callbacks, rebase periodic base |
| 12 | 0x0001D076 | boot_pwr_resume_check | post-wake callback + pin check |
| 13 | 0x00010AB0 | boot_session_state_clear | clear session state → starts `0x00010E58 boot_sched_loop` (never returns) |

Boot-manager equivalents (when a record targets it): `0x00022444 bootmgr_ram_init`
(readback-init 0xFFF80000–FF / 0xFFF84000–FF **without clearing** — record survives reset;
copy template 0xA700→0xFFF84100; RAM test; list inits) → `0x00024974 bootmgr_main_loop`;
record consumption `0x0002264A bootmgr_handover_consume` (validates magic then clears);
handover-to-bootloader router `0x000105A6 bootmgr_handover_route` (record target → enters
0x10644 with state 2/3/5/0). Boot request records: write `0x00011E34`, pending-check
`0x00011EA6` (target 1 + 'w' → stay in bootloader), clear `0x00011EBE`. [O]

Power-on selection (user boot MAT, not dumped — [I] per spec 06 §5a): no record →
`0x00024144(0x2F0)` clock wrapper, return → MAT proceeds to start the application through
the validation handover (0x3D6E6). No code listens on CAN at power-up before the app
without a record [O-grep].

## 3. Application init order

### 3.1 App entry `0x00066CA8` (header +0x7C) — disassembled from the image [O]

1. `stc sr; or #0xF0; ldc sr` → **SR.I = 0xF** (all maskable interrupts off)
2. **VBR = 0x40080 − 0x10 = 0x40070** (vector *n* at 0x40080+4(n−4); verified: vector 228 →
   0x000677F8, vector 424 → 0x00095E8A, vector 476 → 0x00095A4C)
3. `0x00095334` empty hook
4. `0x00067124 timer_stamp_buf_init` (7-slot FRT timestamp buffer 0xFFF80C64)
5. `0x0009533C boot_periph_init_full` (app copy: ATU prescalers PSCR0..3=19/4/6/0, timer E
   2 MHz groups, timer F 20 channels, timer G 12.75 µs/1 ms triggers — spec 02)
6. `0x000950A0 boot_periph_reg_init`
7. `0x00095408 wdt_kick` (first feed)
8. `0x0008EEB4 bus_enable2_job`
9. `0x000676A4 ram_ranges_test` (fault 0x211)
10. `0x00095334` empty hook
11. `0x00067740 ram_data_init` (0xFFF80000 rw pass, 0xFFF84000 copy, .data table copy)
12. `0x00095260 bus_chip_detect` (controller id @0x80100000)
13. `0x00067200 boot_handshake_start` (reset cause → state 0x55/0x56/0)
14. `0x00067400 ram_blk_validate_repair` (fault 0x212)
15. `0x0009A89C crt_init_array_run` (static ctors; e.g. actuator objects via 0x15BF2C, spec 12)
16. `0x00066EC0 exc_record_clear` (fatal-event record, magic 0xA835)
17. `0x0008EEDC sched_batch_g` (enable bits 0xFFFFD000/D800/E000 + 7 jobs)
18. `0x0006728E boot_handshake_advance` (mailbox 0xFFFC0D04 → 0x5555/0x3737, state 0xAA/0xAB)
19. `0x0008EF1A sched_batch_h` (14 jobs 0x919A0…0x91852 — serial/SPI driver band)
20. `0x00067350 io_ffe3810_clear`; `0x00067384 boot_handshake_wait` (budgets 4000/8000)
21. `stc sr; or #0xE0; ldc sr` → **SR.I = 0xE**
22. `0x0009524A plant_cmd_po_window_step`
23. `0x00066DA4 init_tbl_run_timed` → … → `0x00067A68 main_loop`; entry then self-loops as a
    safety net (`bra` to itself).

### 3.2 Init table run by `0x00066DA4 init_tbl_run_timed` (pool 0x66E8C–0x66EA8) [O]

| Slot | Target | Name | Role |
|---|---|---|---|
| 0x66E8C | 0x00067F66 | load_mon_calibrate | idle-loop calibration (20000 iters) for CPU-load reference |
| 0x66E90 | 0x0009A46C | crt_ram_init(1) | ROM→RAM data copy, bss zero, kernel hooks |
| 0x66E94 | 0x0006839C | sched_overrun_cfg_load | overrun threshold 0xFFF90A8C, watched task ptr 0xFFF90A88 |
| 0x66E98/9C | 0x00041279 → `tbl_6x10_lookup(0, 9)` | variant table @0x43430 | selects init bank **0x42C8C** |
| 0x66EA0/4 | 0x00067BE4 | sched_run_task_arrays(0, 0x41274, 0x42C8C) | runs the bank with per-task timing |
| — | FRT delta | — | init duration stored to 0xFFF90A50 (µs, saturated 0xFFFF) |
| 0x66EA8 | 0x00067A68 | main_loop | never returns |

### 3.3 OS init task bank 0x42C8C (array-of-arrays; executed in array order) [O]

- arr[1] @0x044C38 (30 entries): `0x7DBEC, 0x7DCCA, 0x67140 timer_stamp_capture, 0x7CB3E,
  0x7CB00/06/0C/12/18/1E (stub run), 0x7D7DC, 0x6BC90 eeprom_register_tables, 0x6B618,
  0x6BC16, 0x7C8D0, **0x7EE34 → fault_manager_init 0x87732** (registers poll 0x873F0),
  0x7EAC0, 0x7DD3C init_event_rec_2e14, 0x7E06C init_channel_95007, 0x7E3F4
  init_channel_9500a, 0x7E29C, 0x8EF56 sched_one_91f3e, 0x6B31C, 0x98896, 0x6AA80,
  0x6B4B4 eeprom_set_retry_lt3_flag, 0x7CB5C, 0x7CB90 boot_init_hooks2, 0x7CBDC
  notify_84_group, 0x7A628 trace_snap_all` (unlisted = unnamed small stubs, band 0x7C–0x7E)
- arr[2] @0x042C5C: `0x6F210, 0x6CF3C, 0x6C838, 0x95B44, 0x6CA44 diag_install_hooks,
  **0x6E804 init_chain_e804 — registers tick tables 0xFFF9099C/0xFFF90A3C**`
- arr[3] @0x04D788: `0xA4B40 heap hook, 0x1320B6, 0xA0E20 dtc_init_pair_call`
- arr[4] @0x042C78: `0x6F26C can_rx_init_timeouts, 0x6CAD4 diag_register_handlers, 0x6D9FC`
- arr[5] @0x044CB4: `0x7A680 trace_snap_all, 0x7E8C4, 0x72F24, 0x8EF6A sched_retry_call,
  0x67140 timer_stamp_capture`

OS phases after this: state 0→1→2→3 (§1 rows); cyclic phase byte 0xFFF93B33 picks banks per
variant (this unit's cyclic OS-level banks, variant 0, are near-empty — the application
work rides the tick tables/rings and the OS service objects below). [O]

---

## 4. Cyclic task table

| Task | Period | Entry function | Driver / mechanism |
|---|---|---|---|
| OS tick / due-task dispatch | 1 ms | `0x000677F8 os_sched_run_due` (vector 228) | timer H 4 MHz/compare 4000; tables 0xFFF9099C/0xFFF90A3C → `0x934AC` → per-type rings; re-arm `0x6791A` from cal rate table (0x66468→0x19xxxx) |
| Supervisor word (RSPI-B/SSLB1) | 5 ms | boot: `0x1D0B4`/`0x30E40`; app: `0x8EE82 sched_batch_f` → `0x91C86 power_state_update` (job 0x40BE8, read-back 0x22) | 5 ms task; bit-2 complement toggle; bit 0 = output enable |
| Fault-manager poll (owns walk + RAM monitors) | [I] (~5–10 ms; registrar period word 0x90000 not decoded) | `0x000873F0 fault_manager_poll` (init pair 0x87732; registrar pool 0x7EE38) | OS service object; calls walk task 0x6C63E, rammon 0x7F1A6, checksum walker 0x7EE8A |
| Application measurement/variant pass | 10 ms-granularity cycle (delta computed per pass) | `0xA2D5E/0xA2EE0` seqs → `0xB912C meas_ram_refresh_task` → `0xBAEB4 timer_cycle_delta_fanout` (~20 object ticks) | OS task arrays (variant banks, e.g. 0x42C38/0x42F44/0x43148/0x4334C); once-only `0xDA388 cfg_variant_apply` |
| Cyclic timer engines (event lists) | per-event periods in list descriptors | `0x7835C timer_sched_run_list_a` (list 0x415C8), `0x78368 …_b` (0x42AB8) | called from `0x6E882 spi_bus_setup` (also CAN `0x97B7C can_all_groups_sync`) in the variant banks |
| CAN periodic tx (bus 1) | 15.625 ms × {1,2,10,50} → 15.6/31.3/156/781 ms | packers per message; scheduler 0x93FE0+; groups init `0x94994` | tick-derived schedule; live rate table 0xFFF909C0 (≈104 Hz base divisors) |
| CAN bus 2 (0x300) | 20 ms measured | same scheduler | [O-bench] |
| Solenoid cyclic SPI job (RSPI-A, 4 chips × 9 cmd frames = 45 frames) | per cyclic pass (DMA) | builder `0x924B0`, launcher `0x96468`, DMAC `0x980E0` (SPCR=0xF8, count 45, TX 0xFFF88518) | A-DMAC; burst-end ISR vector 445; both RSPI IPR27 = 4/4 |
| EEPROM record engines | event-driven, 15 s timeout | `0x6A86C eeprom_state_machine_a`, `0x6AB8E …_b` | OS cyclic banks (phases 1–3, variant 0) |
| u8/u16 deadline timers (unit 22) | 1 ms base (tick counters 0xFFF8E220/0x8E222) | `0xBAD48 timer_u8_add_tick`, `0xBAF72`, fan-out §1 | deadline library, 10 ms cycle fan-out |
| CPU-load monitor | main-loop continuous | `0x67FBE` (gate cal 0x53AC); `0x6A054 load_mon_task_step`, `0x6A0A8 os_task_step_b` | superloop 0x67A68 |
| Watchdog service | cyclic (~<30 ms headroom) | `0x95408 wdt_kick` (0x5AB7/0xA567); mode switch `0x920A4` on enable edges; alt quick-reset `0x966B8` (0xA563) | WDT Pφ/16384 (819.2 µs/count) |
| Boot supervisor 5 ms (until handover) | 5 ms | `0x1D0B4` (bootloader), `0x30E40 boot_periodic_supervise` (boot mgr; uptime cap, WD magic 0x5AB7) | boot schedulers 0x10E58/0x24974 |

## 4a. Commissioning-walk chronology (detail; WALK-PER-BOOT per reconciliation.md)

1. Every boot: `0x87732 fault_manager_init` re-inits walk state (era value 0x3F, expected
   level 7, era counter 1, period 0x3138) and arms boot flag 0xFFF9500E=1. [O]
2. First `0x873F0` pass: one-shot enable `0x6C612(1)` (level<7 only) → bit-0 flow ≈+105 ms.
   Outer FSM state 0 issues the **0x06-era request** `0x6C48C(0x4B0)` → state 8 →
   `0x8D020 mode_transition_supervise` → 0x12/0x23. [O]
3. Era dwell = period+2400 ms: **0x06 era 3600 ms; 0x1F era 8600 ms** (stepper `0x6C542`,
   FRT delta). Era 2 requested at state 0x23 once level==0 (`0x8D108`). [O][D]
4. No skip branch exists on the reported level; warm chip passes the 0x45 gate instantly.
   Total ≈ 12.2 s + boot ≈ 0.25 s ≈ 12.5 s (bench first sample +15 s already engaged). [O]

## 5. Contradictions / corrections vs. the assumed order

1. **Init is not all before the first cyclic pass.** The ~200-function variant/solenoid
   parameter init (0xDA388/"0xDA418", incl. the 0x106xxx band constructors) runs lazily
   from the FIRST application measurement pass (0xB912C), after main_loop is already
   running. Boot-side init tables and the app entry cover only bring-up; the deepest
   application config lands inside the first ~10 ms of cyclic life.
2. **First frame vs. init still holds for CAN**: CAN driver/group init (0x6F26C, 0x94994,
   0x94062, 0x9838C) is in the OS init bank, before main_loop → first CAN tx strictly
   after init. [O-order]
3. **Walk vs. leap**: the walk *starts* (0x06-era request, ≈first monitor pass,
   ~T+0.25–0.3 s) at roughly the same time as the leap (+0.20–0.25 s), runs *through* it,
   and completes ≈T+12.5 s. Solenoid drive at +250 ms is inside era 1 — the leap is NOT
   gated on walk completion (enable bit-0 at +105 ms is the earlier, independent gate).
4. **Rail continuity**: the supervisor conversation is boot-owned from T+0; there is no
   app-side "bus quiet" gap at handover on the stock image (our re-implementation's 3–7 s
   boot-manager reign is the anomaly, spec 03 §6.2v).
5. 0x0001D552 is `boot_irq_table_init` (vector maps/channel registration), not the boot
   CAN driver init; the boot-manager CAN init family is 0x32C8C/0x329F8/0x31744. [O-name fix]

## 6. Gaps — cannot be timed statically (needs the observation run)

1. **User boot MAT internals** (not dumped): T+0 sequence, app-validation cost, exact
   register/stack state at handover. Only its bench duration class is known (stock
   handover ≈0.2 s; our-image boots 3–7 s).
2. **Absolute time of the first CAN frame** after power-on (candump 2026-10-17 was not
   power-synced; steady traffic with idle payloads once started). Needs a power-on-synced
   capture.
3. **fault_manager_poll exact period** (registrar word 0x90000 semantics), OS phase-dwell
   counts (0x67DF0 table), handshake-wait units (4000/8000).
4. **App init duration**: stored at runtime to 0xFFF90A50 (FRT µs); readable only live.
5. **Which variant bank the stock actually executes** cyclically (selector byte 0x414C1=0
   gives near-empty OS banks; the full ~50-task arrays sit in variant-6 banks selected per
   OS-service-object descriptor byte at +0x29) — registrar wiring not fully decoded.
6. **Era-2 timing on a boot-warm chip** (chip-side level dynamics 7→0 during era 1).
7. Attributions of the two 1–1.75 s current dips (drive-strategy phases vs. walk echoes).
