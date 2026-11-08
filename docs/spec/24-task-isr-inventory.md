# 24 — Task/ISR inventory and reset-cause verdict (top-down artifact 4)

> The definitive execution inventory: 7 boot schedulers + the app superloop chain, 5 cyclic engines, 38 ISRs across 45 vector slots, peripheral lifecycle — and the reset-cause verdict: no recovery path exists to replicate. Generated 2026-11-08.

# ARTIFACT 4 — Task / ISR inventory, peripheral lifecycle, reset-cause handling

Stock firmware Bosch 1034420267 (8HP TCU, Renesas SH7254R). Sources: decompile tree
`~/tcu_sweep/decomp/<region>/0x<ADDR>.c`, name map `docs/spec/catalog/all_names.csv`
(mirrored `~/tcu_sweep/all_names.csv`), unit catalogs `docs/spec/catalog/unit_*.md`,
specs 02/03/05/06, Renesas manual `/tmp/sh7254r_man.txt` (R01UH0480EJ0400 Rev.4.00),
ELF `1034420267_sh7254r.elf` (flash vaddr 0 at file+0x1000; vectors and pointer tables
re-extracted from the ELF for this artifact). Markers: [O] observed in code/ELF, [I] inference.

Vector numbering: SH-2A event N lives at `VBR + 4N`; app VBR = 0x40070 (header+0x70), so
vector slots are 0x40074 (v1) .. 0x4086C (v511). **The CPU vector table ends at v511**
(manual table 7.3: peripheral interrupts 64..511). Everything the flash image holds beyond
0x40870 (my scan initially read "vectors" up to 1200) is application **data** — task and
pointer tables — not vectors. [O]

---

## 1. TASK TABLE — everything cyclic

No RTOS (spec 02 §7/§7b, runtime-confirmed §7c): bare superloops + timer ISRs + flat tables.

### 1.1 Boot-side schedulers (r00_boot)

| Entry | Name | Loop shape / period | Unit |
|---|---|---|---|
| 0x00010644 | `bootloader_entry_main` (kind=entry) | bootloader init chain then infinite loop; arms WDT via 0x1D1A4→0x1DBC8 at start | 01 |
| 0x00010E58 | `boot_sched_loop` | bootloader cooperative scheduler main loop (seed anchor, verified in name map) | 01 |
| 0x00013780 | boot background job dispatcher | runs erase/download/verify jobs during reprogramming sessions only (spec 06 §6.1) | 01 |
| 0x00024974 | `bootmgr_main_loop` (kind=entry) | boot-manager scheduler main loop (never returns); starts via 0x22224 which calls the WDT kick chain 0x30F30→0x3180C | 04 |
| 0x0001D758 | `boot_irq_dispatch` | boot IRQ common dispatcher: source index (table at 0xFFF8571C) → 0x14-byte handler record → call fnptr at +0x10; handlers installed by 0x0001DD44 `boot_irq_vector_program` | 02 |
| 0x0001D1A4 | `boot_shift_periodic_base` | per-loop WDT feed with FRT (0xFFFFF220) backdate −3000 (≈5 ms gate) | 02 |
| 0x00030F30 | `boot_periodic_kick` | boot-manager copy of the periodic WDT feed (−3000 FRT) → 0x3180C | 04 |

Boot tick: same timer-H/D register set as the app (spec 02 §2: 4 MHz, compare 4000 = 1 ms
in boot software) [O via spec 02].

### 1.2 Application core scheduler (r04_boot region = app startup band 0x40000+)

| Entry | Name | Period / driver | Role |
|---|---|---|---|
| 0x00066CA8 | app entry (header+0x7C) | one-shot | raises SR.I=0xF, sets VBR=0x40070, ~20 init calls, tail-jumps into main loop (spec 06 §4) |
| 0x00066DA4 | `init_tbl_run_timed` | one-shot at boot | runs the init block 0x66E8C..0x66EA8 and times it with FRT 0xFFFFF220 (delta→0xFFF90A50); startup table it consumes listed in §1.3 |
| 0x00067A68 | `main_loop` (kind=entry) | superloop, never returns | forever { service-cb(0x66B50 pool); 0x00067B16 `os_sched_service_step`; service-cb } |
| 0x00067B16 | `os_sched_service_step` | every superloop pass (gated on boot-state 0xFFF93B32==3) | runs measure pair, then **0x00067BE4 `sched_run_task_arrays`** (the task-array executor), config loader 0x0006839C, top-5 clear 0x000683C8, monitor reset 0x0006856C |
| 0x00067BE4 | `sched_run_task_arrays` | per pass | walks NULL-terminated fn-pointer task arrays (param_3); optional per-task instrumentation (overrun report vs threshold 0xFFF90A8C, top-5 runtime list 0x000682EA, trace buffers 0x00068440+) |
| 0x00067E3C | `task_dispatch_wrapped` | per task | critical-section + "current task" swap (0xFFF8401C), run counter, measure bracket, call task(idx) |
| 0x00067EC4/0x00067F02/0x00067F50 | load sampling helpers | per pass | CPU-load window 0xA0 samples |
| 0x00067FBE | `load_mon_cyclic` | every main-loop pass when tick flag 0xFFF848CF set | cyclic CPU-load monitor (idle-count window 0x00068088); also the **workhorse that drains the ring buffers the tick ISR fills** (spec 02 §7) |
| 0x00067F66 | `load_mon_calibrate` | one-shot | idle-loop reference calibration (word 0xCA35 gate) |

App task arrays (flash, walked by 0x67BE4): the record table at **0x411A0+** holds
{mask-lo, mask-hi, fn} records — `0x411A8 os_task_step_b`, `0x411B8 0x6A0E4`, `0x411C8
0x6A0F4`, `0x411D8 0x6A078`, `0x411E8 0x6A098`, ... `0x41218 0x6A054 load_mon_task_step`,
`0x41228 0x6A118`, `0x41238 0x6A104 boot_init_hooks2__3`, `0x41248 0x69FA4
os_cyclic_task_batch`; per-profile task-array heads are the ten descriptors 0x41090..0x410FC
selected through the 6×10 table at 0x43430 (indexed by RAM 0xFFF90AB8). [O] (ELF dump)

### 1.3 App startup table (flash 0x66D50..0x66DA0, in execution order) [O]

`lib_empty_hook` 0x95334 → `timer_stamp_buf_init` 0x67124 → `boot_periph_init_full__2`
0x9533C → `boot_periph_reg_init__3` 0x950A0 → **`wdt_kick` 0x95408** (first app WDT feed) →
`bus_enable2_job` 0x8EEB4 → `ram_ranges_test` 0x676A4 → `ram_data_init` 0x67740 →
`bus_chip_detect` 0x95260 → **`boot_handshake_start` 0x67200** (reset/boot-cause eval, §4) →
`ram_blk_validate_repair` 0x67400 → `crt_init_array_run` 0x9A89C → `exc_record_clear`
0x66EC0 → `sched_batch_g` 0x8EEDC → `boot_handshake_advance` 0x6728E → `sched_batch_h`
0x8EF1A → `io_ffe3810_clear` 0x67350 → `boot_handshake_wait` 0x67384 →
`plant_cmd_po_window_step__2` 0x9524A → `init_tbl_run_timed` 0x66DA4.

Init block 0x66E8C..0x66EA8 run by 0x66DA4: `load_mon_calibrate` 0x67F66, `crt_ram_init`
0x9A46C, `sched_overrun_cfg_load` 0x6839C, (0x66E98), `tbl_6x10_lookup` 0x679AC, (0x41274),
`sched_run_task_arrays` 0x67BE4, **`main_loop` 0x67A68** — i.e. the last init call enters
the scheduler. [O]

### 1.4 Interrupt-driven cyclic engines

| Engine | Vector / source | Period | What it runs |
|---|---|---|---|
| **Tick ISR** 0x000677F8 `os_sched_run_due` | v228 = ATU-D1 compare match **CMID10** (OCRD10 0xFFFFF5D0 re-armed each tick from TCNT1D1 0xFFFFF530; count clock 4 MHz with 8 fraction bits — `(delta + TCNT>>8)<<8`) | nominal 1 ms [I: calibration rate pointer table 0x66468 dereferences 0x190128=1000, 0x191480=100; spec 02 §2] | scans two 20-byte cyclic tables at RAM 0xFFF9099C (8 entries) and 0xFFF90A3C (1 entry) — counts at flash 0x468E1/0x468E3 [O]; fires the min-entry via dispatcher **0x000934AC `svc_tbl_request_dispatch`** which enqueues 16-byte slots into per-type rings at 0xFFF943E8+8*type; rings drained by 0x00067FBE in the main loop; entry-enable cleared after fire (one-shot) except index 0 |
| **CMT batch** 0x0009A518 (v140 stub) → **0x00069FA4 `os_cyclic_task_batch`** | v140 = CMT0 compare CMI0; CMCR/CMCNT/CMCOR at 0xFFFEC010/12/14 programmed by 0x00091A30 `timer_cmp_set_ec` (called from 0x00067A7C first boot phase); CMF cleared each service (0xFFFEC011 &= 0xFE) — CMT auto-reloads | calibration-driven [I]; nominal 1 ms-class countdown tick | 0x00067DF0 `task_timer_tick` (per-task u16 countdown, reload from record+7, retry counter +4, done-flag +9) over the task block at 0xFFF9103C, plus 8 table-driven jobs (pool 0x6A014 pairs from 0x6A00C..0x6A04C) |
| **Timer-list A** 0x0007835C `timer_sched_run_list_a` | called (polled) from 0x0006E882 `spi_bus_setup` pass | per pass; per-record thresholds at +2/+6 | 0x50-byte records, list base flash **0x415C8**; four callbacks per record (+0x18/+0x1C/+0x2C/+0x30); marks channel event into 0xFFF9099C via 0x0007844E (priority-mask 6 lock) — links timer lists to the tick tables |
| **Timer-list B** 0x00078368 `timer_sched_run_list_b` | same | same | list base flash **0x42AB8**; event area 0xFFF90A3C; callbacks reach the signal-pool packers 0x0006E98C/0x0006E95C |
| **10 ms fan-out** 0x000BAEB4 `timer_cycle_delta_fanout` | from sequencer step `seq_2ee0` 0xA2EE0 / `seq_2d5e` 0xA2D5E (pointer-table dispatched, refs at 0x4D8A4/0x4DA9C/0x4DC94/0x4D7C4) | computes `(delta+5)/10` rounded **10 ms** cycle units from FRT object+0xBC | 21 indirect tick slots, table 0xBAF7C..0xBAFF4 (§1.5) |
| **fault manager** 0x000873F0 `fault_manager_poll` | registered as {init 0x87732, task 0x873F0} via OS-service registrar, literal pool 0x07EE38..0x07EE44 | period not statically decoded [I]; era dwell constants consistent with the 1 ms FRT tick (reconciliation.md) | owns the supervisor commissioning walk task 0x6C63E + monitored-RAM checksum walker 0x7EE8A + outer FSM 0xFFF95010 |
| cyclic poll tasks, units 09/10 | timer lists A/B callbacks | per-record | dataset-block 16-channel transfer wrappers 0x79C28+ (callers='-', table-dispatched), status getters |

### 1.5 The 10 ms fan-out slot table (flash 0xBAF7C..0xBAFF4, from ELF) [O]

Each slot = {RAM obj at −4, fn}: (obj 0xFFF8E5C1:) 0xB7E6E `obj_b7f08_call_a`, 0xB7E7E
`obj_b7ec8_call`, 0xB7E76 `obj_b7f10_call`, 0xB7E86 `obj_b7f1c_call`; (0xFFF8E61B:)
0xBAD10 `lib_call_ifd34`; (0xFFF8E5C5:) 0xB85E8, 0xB85E0 `lib_b85f8_update__4`, 0xB85AA
`lib_thunk_indirect__19` (→0xE5624); (0xFFF89BEC:) 0xB7A96 `obj_b7aa8_call_m0`;
(0xFFF8E5BE:) 0xB6BFC `obj_b6c04_call_d`; (0xFFF8E5BC:) 0xAEE8A `fwd_obj_aef4c_call`,
0xAEE82 `fwd_obj_aeeb4_call__3`, 0xAEE9A `fwd_obj_aeefc_call2__2`, 0xAEEA2
`fwd_obj_aef5c_call`; (0xFFF8E61D:) 0xBB19C `shiftx1d8_op_264` (shift-execution per-element
op); (0xFFF8E5C3:) 0xB810E `multi_obj_reset_seq`; (0xFFF8E611:) 0xBA426 `lib_call_if4f0`;
(0xFFF8E619:) 0xBA766 `lib_thunk_indirect__20` (→0xE93D2), 0xBB1CC `lib_empty_hook__42`;
(0xFFF8E617, arg 0x2C:) 0xBA6F4 `lib_evt_trigger_a`. — 21 tick slots ≈ the "~25 module
tick" seed; the rest ride the sequencer wrappers. [O]

### 1.6 Cyclic batch dispatchers (r08_app, table-dispatched, callers='-') [O]

| Entry | Name | Jobs (from edges/callees) |
|---|---|---|
| 0x0008EE82 | `sched_batch_f` | 7 jobs incl. 0x91C86 `power_state_update` (supervisor echo bits 0xFFF907D4 → WDT start/stop gating), 0x91AB8, 0x90ACC, 0x90B98, 0x90D58 — referenced from 5 diag/service-table sites 0x44D24/0x44E6C/0x44FC4/0x45114/0x45208 |
| 0x0008EEDC | `sched_batch_g` | enables CAN ctrl bits 0xFFFFD000/D800/E000, then 0x8FB96, 0x96010, 0x91FA4, **thunk 0x920F8 → 0x95408 `wdt_kick` (the cyclic WDT feed)**, 0x961F0, 0x91A80, 0x91DCC |
| 0x0008EF1A | `sched_batch_h` | 0x8F9EC (→ 0x96900 `spi_engine_hw_init`), 0x90010, 0x90ACC, 0x90AEC, 0x90D12, 0x90F7A, ... |

CAN cyclic schedule: the live RAM dump (spec 02 §7c) shows the flat schedule table at
0xFFF909C0 (7×20-byte records) as rate divisors of a ≈104 Hz base: 2→52 Hz, 0x0A→10.4 Hz,
0x32→2.1 Hz — spec 08's 1/64 s (15.625 ms) message schedule. [O]

---

## 2. ISR TABLE

### 2.1 Application vector table (VBR=0x40070) — real handlers only [O: ELF extraction]

| Vector | Manual source | Handler | Name / role | IPR setting site |
|---|---|---|---|---|
| 4 | General illegal instruction | 0x6866E | unexpected-event counter stub (GBR counter bank) | — |
| 6 | Slot illegal instruction | 0x68672 | counter stub | — |
| 9 | CPU address error | 0x6873E | trampoline → **fatal path 0x6865C/0x686D6 → 0x66F82 `exc_capture_and_halt`** | — |
| 10 | DMAC address error | 0x68676 | counter stub | — |
| 11 | NMI | 0x6867A | counter stub | — |
| 12 | **User break (UBC)** | 0x96684 | `unknown_96684_dispatch` — multi-register save trampoline (`movml.l`), register-pool dispatch, tail 0x96D6A; the debugger-break entry | — (level 15 fixed) |
| 13 | FPU exception | 0x68682 | counter stub | — |
| 15/16/17/18 | bank ovf/undf, div0/div-ovf | 0x68686/0x6868A/0x6868E/0x68692 | counter stubs | — |
| 64–71 | IRQ0–IRQ7 | 0x6869A | counter stubs (no external pin IRQs used) | IPR01/02 (0xFFFE0818/A) never written [O] |
| 79 | RAM RAME | 0x960C8 | real handler (RAM controller event) | level 15 fixed |
| 80 | ROM FIFE | 0x961A4 | real handler (flash-if event) | level 15 fixed |
| 106 | SINT2 (soft int 2) | 0x677DE | clears request byte then tail 0x686A6 stub | level 2 fixed |
| 124 | **DMAC4 DEI4** | 0x95FEA | DMA transfer-end A — **ADC data DMA** (dmac_init 0x967F4 points ch at 0xFFFFE800 = ADC_A regs) | IPR04 (0xFFFE0C02) nibble15-12 = **0xE** @0x967F4 |
| 128 | **DMAC5 DEI5** | 0x95FC4 | DMA transfer-end B — ADC_B (0xFFFFEC00) | IPR04 nibble11-8 = **4** @0x967F4 |
| 140 | **CMT0 CMI0** | 0x9A518 → 0x69FA4 | `os_cyclic_task_batch` tick (§1.4) | IPR05 (0xFFFE0C04) hi = 1, CMI1 = 2 (@0x31C4C boot, 0x96E80 app) |
| 148 | WDT ITI | 0x686BE | **stub — WDT runs in watchdog-reset mode, no interval interrupt** | IPR05 lo = 0xE written by every wdt program/kick |
| 216/217/218 | ATU-D0 CMID00/01/02 | 0x90A72/0x90A90/0x90AAE | compare-match handlers (loop on 0x90844; distinct arg 0/1/2) | IPR10 (0xFFFE0C0E) nibble15-12 = 6 @0x970FC |
| 224 | ATU-D0 UDID00 | 0x9106C | → calls `park_pwm_service` 0x6C1BA + 0x6C802 + pool 0x68FA2 — the **park/supervisor PWM-servo compare ISR** | IPR10 nibble7-4 = 6 @0x970FC, →8 @0x97198 |
| 226 | ATU-D0 UDID02 | 0x92078 | short stub → arg 0x9225C | IPR10 nibble7-4 = 8 @0x97198 |
| 228 | **ATU-D1 CMID10 — THE TICK** | 0x677F8 `os_sched_run_due` | scheduler tick (§1.4) | IPR10 nibble3-0 = 5 @0x970FC, **raised to 6 by 0x6791A `os_timer_rearm`** |
| 229 | ATU-D1 CMID11 | 0x677CC | unexpected-tick counter (tail 0x686D6) | (same IPR nibble) |
| 424 | SCI-A ERIA (rx err) | 0x95E8A | serial rx-error handler (register-save stub, calls into 0x95xxx SCI driver) | IPR26 (0xFFFE0C2E) nibble15-12 — not directly observed [I: set with bus init] |
| 425 | SCI-A RXIA | 0x95E38 | serial-A receive | 〃 |
| 426 | SCI-A TXIA | 0x95C50 | serial-A transmit-ready | 〃 |
| 427/430/431 | SCI-A TEIA / SCI-B TXIB/TEIB | 0x95C6A | shared tx-end stub | 〃 |
| 428/429 | SCI-B ERIB / RXIB | 0x95EA4 / 0x95E52 | serial-B error/receive | 〃 |
| 444/446 | RSPI-A SPEIA/SPTIA | 0x68716 | **stub — error/tx-done interrupts unused** | IPR27 (0xFFFE0C30) |
| **445** | **RSPI-A SPRIA (rx)** | **0x96590** | RX ISR: SPCR(0xFFFFB000)=8 stop, SPSR clear/read, dispatch **0x93A74 `spi_frame_done_worker`** (state 0x18→0x09 restart / 0x20 finish) or **0x93A08 `spi_frame_abort_worker`** (force 0x40, free slot, next job) | **IPR27 nibble15-12 = 4** @0x963CC `rspi_bus_start` |
| 448/450 | RSPI-B SPEIB/SPTIB | 0x68716 | stub | 〃 |
| **449** | **RSPI-B SPRIB (rx)** | **0x965E4** | twin of 0x96590 on RSPI-B (0xFFFFB800), same workers with bus=1 | **IPR27 nibble11-8 = 4** @0x963CC |
| 452–454 | RSPI-C SPEI/SPRI/SPTI | 0x68716/0x6866A | stubs — RSPI-C unused | masked by 0x963A4 `rspi_all_units_idle` (IPR27 &= 0xF000) |
| 456/457 | RCAN-A ERS/OVR | 0x9565E | combined error/overrun entry | IPR28 (0xFFFE0C32) nibble15-12 = **5** @0x9838C `can_group_hw_init` |
| 458 | RCAN-A RMA0 | 0x9546C | `can_err_isr_a` (catalog name; error/status walk, pool-locked wait, tail 0x933E0) | 〃 |
| 459 | RCAN-A RMA1 | 0x954E6 | `can_rx_isr_a` — walk 0xFFFFD040 rx flags, read buffers, dispatch frames | 〃 |
| 460 | RCAN-A SLEA | 0x955E2 | `can_txend_isr_a` — ack 0xFFFFD030, call tx pump 0x93426 | 〃 |
| 461 | RCAN-A MBEA | 0x6871E | stub | 〃 |
| 464/465 | RCAN-B ERSB/OVRB | 0x95894 | combined error entry | IPR28 nibble11-8 = 5 @0x9838C |
| 466 | RCAN-B RMB0 | 0x95714 | `can_rx_isr_b` (0xFFFFD840/D900) | 〃 |
| 467 | RCAN-B RMB1 | 0x95814 | `can_txend_isr_b` (ack 0xFFFFD830) | 〃 |
| 472/473 | RCAN-C ERSC/OVRC | 0x95ACC | combined error entry | IPR28 nibble7-4 = 5 @0x9838C |
| 474 | RCAN-C RMC0 | 0x9592E | `can_rx_isr_c` (0xFFFFE040/E100) | 〃 |
| 475 | RCAN-C RMC1 | 0x95A4C | `can_txend_isr_c` (ack 0xFFFFE030) | 〃 |
| 476/477 | RCAN-C SLEC/MBEC | 0x6871E | stubs | 〃 |
| 488/489 | A-DMAC TE | 0x68726 | **stub — the A-DMAC never raises a CPU interrupt**; completion flows through the RSPI RX ISRs | IPR29 (0xFFFE0C34) never observed written [O] |
| 152–215, 232–311, 312–367, 368–379, 380–391, 392–423 | ATU-A/B/C, ATU-D2-3, ATU-E, ATU-F, ATU-G/H, ATU-J, ADC ADI/ADID | 0x686xx counter stubs | **no ATU capture/compare or ADC interrupts in the app — ATU inputs are polled, ADC is DMA-driven** [O] |

Counter stubs: the 0x68662–0x6872E family each bump one GBR-relative diagnostic byte
(bytes at 0x6866A: `mov #imm,r0; mov.b r0,@(disp,GBR)`), i.e. unexpected-interrupt
reporters, one per vector group. [O]

### 2.2 Pointer-table-reached workers (callers='-', not CPU vectors) [O]

| Handler | Reached from | Role |
|---|---|---|
| 0x00093A74 `spi_frame_done_worker` / 0x00093A08 `spi_frame_abort_worker` | RSPI RX ISRs 0x96590/0x965E4 | frame completion state machine |
| 0x000910EE `serial_resp_handler7` (7-word duty job), 0x000911D8 `serial_resp_handler4` (4-word level job), 0x000921E0/0x92298/0x923BC/0x92530 solenoid response handlers, 0x00092AC0/0x92BBE/0x92CC8/0x92D94/0x92ED4/0x92FD0/0x93096/0x931D4 shifter/serial response handlers | per-job completion pointers in the SPI job descriptors (16 jobs 0x409F4–0x40C74, spec 03) | harvest 0xA8xx echo frames into 0xFFF905xx/0xFFF909xx cells |
| 0x000967C4 `can_err_dispatch` | CAN error ISRs | status read 0x96768 + worker |
| boot: 0x0001E49E `can_irq_handler`, 0x0001E4C2 `can_irq_tx_confirm` | boot vector table via 0x1D758 | bootloader CAN (flash session) |
| boot: 0x00032770 `boot_sio_irq_status_poll`, 0x000327D6 `boot_sio_irq_gate_release`, 0x0003286A `boot_sio_irq_gate_hold` | boot vector table via 0x1D758 | boot SCI serial, clock-gate acks |

---

## 3. PERIPHERAL LIFECYCLE (boot → app handover)

| Peripheral | Boot arms? | App re-configures / runs | Masked/killed? | Fed/serviced? |
|---|---|---|---|---|
| **WDT** | YES — all three boot programs (0x1DBC8 bootloader / 0x3180C bootmgr / 0x3E930): WTCNT=0x5AB7 or 0x5A6E (clock-strap dependent), WRCR=0x5A80 (RSTE=1 → overflow resets chip), WTCR=0xA567 (watchdog mode CKS=7), IPR05 nibble0=0xE; fed per loop 0x1D1A4 / 0x30F30 (FRT −3000) | first feed in startup table (0x66D64 → 0x95408); cyclic feed via `sched_batch_g`→thunk 0x920F8→0x95408 (WTCNT=0x5AB7, WTCR=0xA567) | **conditionally STOPPED**: `power_state_update` 0x91C86 (supervisor echo bits 0xFFF907D4) → 0x920A4 `state_9096a_update` → `wdt_mode_start` 0x97850 (arm) or `wdt_mode_stop` 0x978A4 (clears TME via 0xA5xx keys) — power-state gated | YES cyclically (≈30.3 ms margin per spec 05); fatal path 0x66F82 deliberately forces quick reset via 0x966B8 (WRCR=0x5A80, WTCR=0xA563 CKS=3) [O] |
| **INTC** | boot clears to known state (0x1DAFC/0x31744/0x9533C walk &DAT_fffe0c00); arms ATU-F IPR16–18 = 7 (0x31D48) or 9 (0x1E1D8), ATU-H CMIH = 2 (0x1E260, IPR20 0xFFFE0C22), WDT ITI = 0xE; masks RCAN-A nibble (0x1DE9C) / clears IPR28 (0x329F8, 0x1DCF8) | app sets: CMI0/CMI1 = 1/2 (0x96E80), DMAC4/5 = 0xE/4 (0x967F4), ATU-D/E = 5/6/8/4 (0x970FC IPR10-14, 0x97198 IPR14/15), tick CMID10 5→6 (0x6791A), CAN A/B/C = 5 (0x9838C), RSPI A/B = 4 (0x963CC) | app masks: RSPI all idle (0x963A4, IPR27&=0xF000), DMAC2/3 prio cleared (0x96FA4), ATU-A IPR06 = 0 (0x96F7C `boot_frc_irq_disable__3`), ADC/ATU-F/G/H/J IPRs never enabled | ATU prescalers/master enable re-checked at runtime [O spec 02 §1] |
| **RSPI-A** (0xFFFFB000) | disabled, then boot SPI layer starts (spec 05 §4) | `rspi_all_units_idle` 0x963A4 (SPCR=8); `spi_engine_hw_init` 0x96900 (programs A-DMAC handshake cells 0xFFFE6060-96, staging 0xFFF8844C); `spi_job_dmac_launch` 0x96468 — per job program RSPI+DMAC then **SPCR=0xF8 launch**; RX ISR stops with SPCR=8 | SPTI/SPEI never enabled (v444/446 stubs) | interrupt-driven RX at IPR 4 (v445) [O] |
| **RSPI-B** (0xFFFFB800) | 〃 | 〃 (bus index 1); supervisor chip on SSLB1 (spec 03 §5) | 〃 | interrupt-driven RX at IPR 4 (v449) [O] |
| **RSPI-C** (0xFFFFC000) | SPCR=8 | left idle (0x963A4 covers it; stubs at v452-454) | masked | no |
| **ATU-III** | prescalers PSCR0..3=19/4/6/0, master enable; boot arms ATU-F (IPR16-18=7/9) | app: timer A TCNTA 1 MHz free-run timestamp (≈220 reads); timer D1 = 1 ms-class tick (v228); timer D0 CMID00-02/UDID00/02 = park servo compares (v216-226); timer E = 16 PWM channels (2 MHz, groups per spec 02 §3) **interrupt-free**; timer F = 20 input channels polled; timer G = ADC triggers G1/G2 12.75 µs, G4/G5 ≈1 ms; IPRs per §2.1 | ATU-A IPR cleared in app (0x96F7C) | polled consistently (spec 02 §4.1) |
| **DMAC (ch 4/5)** | reset then enabled (spec 05 §4) | `dmac_init` 0x967F4: 2 channels pointed at ADC_A (0xFFFFE800) / ADC_B (0xFFFFEC00), end callbacks registered, IPR04 = 0xE4xx | — | DEI4/DEI5 ISRs 0x95FEA/0x95FC4 [O] |
| **A-DMAC (0xFFFE60xx)** | reset then enabled (spec 05 §4) | 0x96900 programs handshake/alias cells; per-job launch 0x96468 | CPU interrupt never enabled (v488 stub, IPR29 untouched) | completion via RSPI RX ISR + job-descriptor callbacks [O] |
| **ADC** | — | DMA-driven scan (timer G triggers); **no ADC CPU interrupts** (v392-423 all stubs) | ADI/ADID IPRs never set | results consumed cyclically (spec 04) |
| **RCAN-TL1 A/B/C** | boot masks/clears IPR28 (0x1DE9C/0x329F8/0x1DCF8); bootloader uses its own CAN handlers via 0x1D758 | `can_group_hw_init` 0x9838C per controller: reset, IPR 5, bitrate 500 kbit/s, 32 mailboxes DLC/0x2700; cyclic tx pump 0x93426; schedule table 0xFFF909C0 | each controller enable-bit driven by `sched_batch_g` (0xFFFFD000/D800/E000 \|= 1) | rx/tx/error ISRs v456-477 [O] |
| **CMT0** | — | 0x96E80 clears CMSTR/CMCRs at init; 0x91A30 programs CMCNT/CMCOR + CMCR|=0x40 (interrupt enable); hw masks 0x96ED2/0x96EF8 gate CMSTR via shadow 0xFFF94722 | stoppable via mask shadow | CMI0 v140 → os_cyclic_task_batch [O] |
| **SCI-A/B** | boot SIO handlers via 0x1D758 | app SCI driver 0x95C50-0x95EA4, ERIB/RXIB real — serial links kept alive (plant/diag [I]) | SCI-C unused (stubs) | interrupt-driven [O] |
| **STBCR / AUD-II** | — | `boot_handshake_start` 0x67200 (else-branch) and `diag_mode_switch` 0x68778 (leaving mode 3) write **STBCR (0xFFFE0400) |= 0x3C04 → MSTP2 = AUD-II debug module clock-stopped** | AUD-II killed when no debugger handshake | — |

---

## 4. RESET CAUSE

### 4.1 What the hardware offers (manual) [M]

- The SH7254R has **only a power-on reset** (manual §7.2.1: "This LSI supports only a
  power-on reset") — from the RES pin or a WDT overflow in watchdog mode. There is **no
  CWRS/reset-status register**; searches for CWRS / reset cause / manual reset / reset
  source in the manual return nothing.
- The **only reset-cause discriminator** is **WTSR (0xFFFE0004)**: bit 7 WOVF (watchdog
  overflow), bit 3 IOVF. WTSR is initialized to 0 by a RES-pin reset but is **NOT
  initialized by a WDT-overflow reset** (manual §7.2.2(2) and table 7.5: "WTSR of WDT ...
  Not initialized"). So after a supervisor... after a WDT-caused reset, WOVF reads 1;
  after a true power-on/RES reset it reads 0. WOVF clears by writing 0 with key 0xA5.
- WRCR (0xFFFE0006) likewise survives WDT resets (RSTE stays set).

### 4.2 What the stock firmware does [O]

1. **WTSR is read in exactly four functions** — the WDT program/kick family:
   boot `boot_wdt_program` 0x0001DBC8 (bootloader), copies 0x0003180C (boot manager) and
   0x0003E930, and app `wdt_kick` 0x00095408. All four do the same read-modify-write:
   `DAT_fffe0004 = (DAT_fffe0004 & 0xff77) | 0xa500` — i.e. **read WTSR, clear WOVF+IOVF
   with the 0xA5 key, and return the old WTSR value**.
2. **Every caller discards the return value.** Verified callers: bootloader
   0x1D1A4 (void), bootmgr 0x30F30 (void), app thunk 0x920F8 → `sched_batch_g` 0x8EEDC
   (void call). A whole-image grep finds no other read of 0xFFFE0004 and **no branch on
   WOVF (0x80) or IOVF anywhere**. The stock thus *erases* the reset-cause bit on its
   first WDT service of every boot without ever consuming it.
3. **The one reset-cause-adjacent read that IS consumed**: `boot_handshake_start`
   0x00067200 (app startup table slot 10) reads the **undocumented I/O word 0xFFFC0D00**
   (undocumented in the manual; adjacent to the AUD-II block 0xFFFC0C00-0xC14), latches it
   to 0xFFF93538, and branches: value 9 or 10 → handshake state 0x55; 0xD or 0xE → state
   0x56; anything else → state 0 and, if a diag-mode query returns 0, **STBCR |= 0x3C04
   (AUD-II module stop)**. States 0x55/0x56 are progressed by `boot_handshake_advance`
   0x6728E (magic 0x5555 / 0x3737 write to 0xFFFC0D04) and awaited by 0x67384. 0xFFFC0D04
   is written by boot manager 0x24B70 and app 0x8316C — a bidirectional mailbox. [O]
   Interpretation [I]: 0xFFFC0D00 reports a boot/debug-peer status (values 9/10/0xD/0xE =
   "debugger/plant-tool handshake" states), not WDT-vs-power-on; the ordinary path simply
   disables the AUD-II debug module.
4. **No recovery path keyed on reset cause**: the persistent-RAM blocks (0xFFF80100
   record, fatal-event ring 0xFFF80D24, boot request record 0xFFF80000) are
   validated/repaired **unconditionally every boot** (0x67400 `ram_blk_validate_repair`,
   0x66EC0 `exc_record_clear` resets the exception ring only when its magics mismatch —
   i.e. after any reset that did NOT preserve RAM it starts clean, after a WDT reset that
   DID preserve RAM it keeps the occurrence counter and bumps it: `DAT_fff80e08++` when
   both 0xA835 magics are intact). The commissioning walk is re-run every boot regardless
   (reconciliation.md: WALK-PER-BOOT, no level/warm-skip branch). The exception ring is
   the *only* structure that behaves differently after a watchdog reset that preserved
   RAM, and its use is bookkeeping (event class/cells read back by 0x670BA/0x670C2/
   0x670D2/0x670E6 accessors) — no alternate init, no re-certification, no different walk.
5. **Deliberate reset**: the only software reset mechanism is the fatal path
   0x66F82 → 0x966B8 (WRCR=0x5A80, WTCR=0xA563 fast clock) which halts in an infinite
   loop until the WDT overflows — plus the diagnostic handover reset of spec 06 §5a.

### 4.3 Verdict for the project

The stock does **not** implement a supervisor-reset recovery path: it never reads reset
cause (WTSR.WOVF) as a *branch condition*, and the chip itself only distinguishes
WDT-overflow from RES-pin resets through WTSR retention. After a watchdog reset the stock
re-runs the identical init/walk and merely inherits (a) preserved RAM — the fatal-event
occurrence counter increments instead of the ring being rebuilt, and the boot request
record survives (0x67740's read/write-back init) — and (b) WRCR/WTSR retention until the
first WDT service clears WOVF. Our firmware ignoring reset cause matches the stock's
behavior except for the fatal-ring bookkeeping; if parity is wanted, the cheap equivalent
is: read WTSR bit 7 once at startup before the first WDT feed, and count occurrences.

---

## 5. Gaps / open

1. **Periods of the RAM cyclic tables** (0xFFF9099C/0xFFF90A3C) and of the CMT0 batch are
   runtime/calibration values — statically only the 1 ms-class tick rate pointers
   (0x190128=1000) and the 10 ms fan-out rounding are pinned. A live 0x2A dump of the
   engaged stock over 0xFFF9099C..0xFFF90A5C would close all task periods at once. [I]
2. **0xFFFC0D00** identity (undocumented): boot/debug-peer status hypothesis rests on the
   AUD-II adjacency and the MSTP2 else-branch; no writer of 0xFFFC0D00 exists in the
   dumped image, so it is hardware- or user-boot-MAT-written. The user boot MAT (not
   dumped) may read/branch on reset cause before any dumped code runs — cannot be
   excluded [I].
3. **0xFFFF1001 bit 0x40** (`hw_cond_once_1001_40` 0x68BE0, 11 callers incl. the fatal
   path and boot handshake): an undocumented strap/status bit read once per boot;
   meaning unresolved. It is the only other always-read "hardware condition" bit.
4. SCI-A vs SCI-B roles (which serial link each serves) — handlers exist for both; role
   assignment [I] plant protocol vs diag.
5. WDT stop condition semantics: which value of 0xFFF9096A (set by `power_state_update`
   from supervisor echo bits) selects `wdt_mode_stop` vs `wdt_mode_start` — the state
   value comes from the SPI supervisor response, exact mapping not decoded. [I]
6. The 6×10 table at 0x43430 (u16: 4 / 11276 / 11812 / 12328 / 12844 / ... per column)
   selects the per-profile task arrays; the unit (looks like enable-keys + periods) is
   not decoded. [I]
7. Boot-block vector tables (bootloader/boot-manager VBR region) not extracted — boot ISR
   → 0x1D758 dispatch mapping is inferred from the handler records, not from boot vector
   slots. [I]
