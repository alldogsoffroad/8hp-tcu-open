# Unit 28 (r0c_app)

Region 0x000D7DDE–0x000DE0F6 of r0c_app. Three bands: (1) 0xD7DDE–0xD80D4 a module
registration/variant block — lazy static-init guards, a bounds-checked variant table lookup
(err id 0x66), and ~20 paired pointer-slot setters applied by the 1461-instr variant applier
0x000DA418 together with ~180 setters in the 0x106CD4–0x107AC8 solenoid band; (2) 0xD8122–
0xD9B24 the core of a 9-state sequencer object (state u8 at +0x21, inhibit bitfield at +0x53,
value/~value output pairs at +0/+1, +7/+8, +9/+10) whose entry is ROM-facade 0x000B8106 and
whose step companions are facades 0x000B80AC/0x000B80B4/0x000B80BC/0x000B80E0 (unit 20's
`obj_b8140_*` slots); it consumes range codes 5–8 (spec 14 §4.1 code domain) and a redundant
4-channel selector decode (0x000D9B24 → modes 0–5); (3) 0xD9C54–0xDE0F6 sequencer periphery
(guards, timers, output helpers) plus a small library tail. The event reporter signature
`(0x2af, 0xf, 0, n, m, k)` recurs throughout band 2.

---

#### 0x000D7DDE `modreg_static_guard_1` — logic
Lazy one-time constructor for the static object at 0x000D7EE8: when the guard at +0x1c is 0
it installs a pointer table value at +0x00 (from 0x000D7EEC), a label ptr at +0x10
(0x000D7EF0), a constant at +0x18 (DAT_000D7EF4) and sets the guard to 1. Returns the guard.
Callers 0x000D7D0E, 0x000D7C8C, 0x000D7DFA, 0x000D7F4E, 0x000D7D62. [O]

#### 0x000D7DFA `modreg_report_variant_1` — logic
Registration/report step called from start-up path 0x000B7F4C: calls fn(0x000D7EF8, 0x24),
then both lazy guards (0x000D7DDE, 0x000D7F3A), then reports `(1.0f, 1, 0x66, guard)`
through the fn ptr at 0x000D7F00. Module id 0x66. Callees 0x000BA6F4, 0x000D5A7C. [I]

#### 0x000D7E20 `modreg_table_lookup10` — library
Bounds-checked table lookup for the 0x66 module: table pointer is selected from the 2-entry
pointer table at 0x000D7F04 by the 0x000D7F3A guard value (0/1); index = float truncated to
u8, must be 0..10, returns table[i]. Out-of-range or missing table calls the error pair
PTR_FUN_000D7ED8(1,0x66) + PTR_FUN_000D7EDC (halt). Callers 0x000D7E86, 0x000D7EBA. [O]

#### 0x000D7E86 `modreg_table_lookup_m1` — library
Range-gates the 0xD7E20 lookup: input must be in [-1.0, DAT_000D7F0C] else the 0x66 error
pair fires; otherwise delegates with param+1.0. Callers 0x00119946, 0x0011C5B6, 0x00118A4A,
0x0011C8C8, 0x000D7EBA (r10_app/r14_app users of the same library). [O]

#### 0x000D7EBA `modreg_thresh_ptr_select` — logic
Calls 0x000D7E86 (no visible argument — float arrives in a register), then returns
&DAT_000D7F74 when the result is <= DAT_000D7F10, else &DAT_000D7F10: selects a pointer to
one of two constants by threshold. Caller 0x000BB4EA. [I]

#### 0x000D7F3A `modreg_static_guard_2` — logic
Second lazy constructor, object at 0x000D7F78: installs ptr 0x000D7F7C at +0x00 and sets
guard +0x18=1 on first call; returns guard. Callers 0x000D7DFA, 0x000D7E20, 0x000D7F4E. [O]

#### 0x000D7F4E `modreg_report_variant_2` — logic
Mirror of 0x000D7DFA from the same caller 0x000B7F4C: guards 0x000D7DDE/0x000D7F3A, then
reports `(1.0f, 0xb, 0x66, guard)` via 0x000D7F80 and calls fn(0x000D7F84, 0x25). Callees
0x000BA6F4, 0x000D572C. [I]

#### 0x000D7F8C `modreg_report_id_3` — stub
Single indirect call fn(DAT_000D7F94, 0x21); sibling of the two report steps above, caller
0x000B7F4C, callee resolves to 0x000BA6F4. [I]

#### 0x000D7F9C `cfg_ptr_slot1_set_a` — data-accessor
Writes pointer option A (PTR_PTR_000D7FAC) into the config slot at 0x000D7FB0. One of the
setter pairs invoked by variant applier 0x000DA418. Cluster of 1. [O]

#### 0x000D7FA4 `cfg_ptr_slot1_set_b` — data-accessor
Writes pointer option B (PTR_PTR_000D7FB4) into the same config slot 0x000D7FB0. Invoked by
0x000DA418. [O]

#### 0x000D7FB8 `cfg_ptr_slot2_set_a` — data-accessor
Config slot 0x000D7FCC := option A (0x000D7FC8); caller 0x000DA418. [O]

#### 0x000D7FC0 `cfg_ptr_slot2_set_b` — data-accessor
Config slot 0x000D7FCC := option B (0x000D7FD0); caller 0x000DA418. [O]

#### 0x000D7FD4 `cfg_ptr_slot3_set_a` — data-accessor
Config slot 0x000D7FE8 := option A (0x000D7FE4); caller 0x000DA418. [O]

#### 0x000D7FDC `cfg_ptr_slot3_set_b` — data-accessor
Config slot 0x000D7FE8 := option B (0x000D7FEC); caller 0x000DA418. [O]

#### 0x000D7FF0 `cfg_ptr_slot4_set_a` — data-accessor
Config slot 0x000D8004 := option A (0x000D8000); caller 0x000DA418. [O]

#### 0x000D7FF8 `cfg_ptr_slot4_set_b` — data-accessor
Config slot 0x000D8004 := option B (0x000D8008); caller 0x000DA418. [O]

#### 0x000D800C `cfg_ptr_slot5_set_a` — data-accessor
Config slot 0x000D8020 := option A (0x000D801C); caller 0x000DA418. [O]

#### 0x000D8014 `cfg_ptr_slot5_set_b` — data-accessor
Config slot 0x000D8020 := option B (0x000D8024); caller 0x000DA418. [O]

#### 0x000D8028 `cfg_ptr_slot6_set_a` — data-accessor
Config slot 0x000D803C := option A (0x000D8038); caller 0x000DA418. [O]

#### 0x000D8030 `cfg_ptr_slot6_set_b` — data-accessor
Config slot 0x000D803C := option B (0x000D8040); caller 0x000DA418. [O]

#### 0x000D8044 `cfg_ptr_slot7_set_a` — data-accessor
Config slot 0x000D8058 := option A (0x000D8054); caller 0x000DA418. [O]

#### 0x000D804C `cfg_ptr_slot7_set_b` — data-accessor
Config slot 0x000D8058 := option B (0x000D805C); caller 0x000DA418. [O]

#### 0x000D8060 `cfg_ptr_slot8_set_a` — data-accessor
Config slot 0x000D8074 := option A (0x000D8070); caller 0x000DA418. [O]

#### 0x000D8068 `cfg_ptr_slot8_set_b` — data-accessor
Config slot 0x000D8074 := option B (0x000D8078); caller 0x000DA418. [O]

#### 0x000D807C `cfg_ptr_slot9_set_a` — data-accessor
Config slot 0x000D80A0 := option A (0x000D809C) and mirrors the inverted pointer value to
RAM global 0xFFF8B0F4 (value/~value pair, spec 14 §1 pattern); caller 0x000DA418. [O]

#### 0x000D808C `cfg_ptr_slot9_set_b` — data-accessor
Config slot 0x000D80A0 := option B (0x000D80A4) plus inverted-pointer mirror to RAM
0xFFF8B0F4; caller 0x000DA418. Cluster of 1. [O]

#### 0x000D80A8 `cfg_ptr_slot10_set_a` — data-accessor
Config slot 0x000D80CC := option A (0x000D80C8) plus ~ptr mirror to RAM 0xFFF8B0F8;
caller 0x000DA418. [O]

#### 0x000D80B8 `cfg_ptr_slot10_set_b` — data-accessor
Config slot 0x000D80CC := option B (0x000D80D0) plus ~ptr mirror to RAM 0xFFF8B0F8;
caller 0x000DA418. [O]

#### 0x000D80D4 `rseq_state_preset` — logic
Default preset of the sequencer state object: if handed NULL obtains it via registry call
PTR_FUN_000D82E4(0x58); sets output pairs (+1=0xff for out0=0, +8=1/+9=0xfe, +10=1),
+0x19=1, range defaults +0x4d..+0x50=6 (range code 6 in the 5–8 domain), +0x55=0. callers=-
(pointer table). Callee 0x0009AA34 (heap). [O]

#### 0x000D8122 `rseq_step` — logic
Main cyclic step of the 9-state sequencer (entry facade 0x000B8106). Gated on the input
object at 0x000D82E8 (+0x13==1, [1]==0): runs mode capture 0x000D8698, two ptr-table calls,
fill 0x000D86E4, timers 0x000D8FD8/0x000D88E4/0x000D8F58/0x000D8862, abort predicates
0x000D8878/0x000D888A → +0x0e, timestamp window → +0x0d bit0, else-path clears all u16
timers (+0x30..+0x3e) and outputs and calls reset 0x000D924C/0x000D8A2C. Sets hold request
+0x1e from +0x53 and computes qualifier +0x06. Callees include 0x00107A6A/0x00107A58
(solenoid band). [I]

#### 0x000D82AE `rseq_arbiter` — logic
Engage/hold arbiter (facade 0x000B80AC, consumer 0x000B986C): if the input block at
0x000D82E8 is disabled it zeroes engage flags and returns 0; otherwise runs precondition
collector 0x000D8C3A, sets engage +5 when a speed window (calib +0x24/+0x28 vs input[0x96])
plus timestamp margin at input+0x94 hold, clears it on the mirrored exit conditions, and
latches hold +0xf from a calibration byte × factor vs +0x24. Returns 0x000D91E4. [I]

#### 0x000D846C `rseq_range_watch` — logic
Range-code monitor (facade 0x000B80E0, consumer 0x000B986C): shifts history (+0x4f←+0x4d,
+0x50←+0x4e), samples input[0xa1]→+0x4d and input[0xa0]→+0x4e (codes 5–8, spec 14 §4.1
domain) with change flags +0x4b/+0x4c; when enabled maps code 8→0, 6→1, 5→2, 7→3 through
selector queries (PTR_FUN_000D8720 on calib +0x70/+0x74) computing permission +0x49 and
validity +0x4a. [I]

#### 0x000D8662 `rseq_hold2_latch` — logic
If +0x22==1 and arg==0, latches +0x22=2 (hold-in-progress handshake). Facade 0x000B80BC,
caller 0x000CFD1A. [O]

#### 0x000D8678 `rseq_hold2_clear` — logic
If +0x22==2: clears +2 and +0x22; with arg==1 also calls DAT_000D8728(PTR_DAT_000D8724,1).
Facade 0x000B80B4, caller 0x00103DE2 (r10_app solenoid band). Callee 0x000BA794. [O]

#### 0x000D8698 `rseq_mode_capture` — logic
Reads the selector decode 0x000D9B24 through PTR_FUN_000D872C into mode +0x20 and clears
timer +0x34 when mode !=2, +0x32 when not 3/4, +0x3e when mode 0 or +0x21 !=1. Caller
0x000D8122. [O]

#### 0x000D86E4 `rseq_sum_window_toggle` — logic
Toggles +0x54 when the sum of the two u16 values behind calib slots +8 and +0xc crosses the
threshold at 0x000D8730 (with the +0xc term only on the falling side) — a speed-sum window
latch. Caller 0x000D8122. [I]

#### 0x000D8734 `rseq_phase_done_check` — logic
Per-mode completion predicate used by 0x000D8862: for mode 0/1/2/3/4/5 it queries selector
state tables (PTR_FUN_000D8990 with indices 0..4 on calib +0x80) and speed thresholds
(calib +0x14/+0x30/+0x34 vs +0x32/+0x34) plus a timestamp margin (0x000D899C/0x000D89A0
pair vs 0x000D8994); returns 1 when the phase condition is met. Callees 0x000B6C8A,
0x000B7E00, 0x000B7D1E. [I]

#### 0x000D8862 `rseq_timer3a_gate` — logic
If 0x000D8734 reports not-done, clears the timer at +0x3a. Caller 0x000D8122. [O]

#### 0x000D8878 `rseq_timer3a_reached` — stub
Predicate: u16 at calib (+0x3c of 0x000D898C block) <= this+0x3a (timer elapsed). Caller
0x000D8122. [O]

#### 0x000D888A `rseq_override_check` — logic
Returns 1 when +0x1f is set, +4 clear, flag 0x000D9348 (+0x46) clear, mode +0x21 not 4/0/6,
and the selector query (idx 5) is set — an override/force condition feeding +0x0e in
0x000D8122. Callees 0x000B6C8A, 0x000D9348. [I]

#### 0x000D88E4 `rseq_hold_state_machine` — logic
Manages the hold latch +0x22: maps range codes 5/6/7/8 (input block byte) to selector 0..3
via calib +0x60 query; archives +2→+3; while +0x22==1 clears the latch when the selector
u16 <= timer +0x38 or the timestamp margin expires; any other value clears + reports event
(0x2af,0xf,0,4,9,0x2e). Callees 0x000B6C8A, 0x000B7E00, 0x000B79E4, 0x000B7D1E. [I]

#### 0x000D89DE `rseq_curve5_monotonic` — logic
Validation loop i=1..5: value table (PTR_FUN_000D8BD0 on object 0x000D8BC8, idx i) must be
strictly less than the reference pair (PTR_FUN_000D8BD4 on calib+0x90, idx i-1); returns 0
on the first violation. Caller 0x000D9448 (state-0 permission). Callees 0x000B6C94,
0x000BABF6. [O]

#### 0x000D8A2C `rseq_clear_flags` — logic
Zeroes the sub-state and condition flag block +0x13, +0x11, +0x1c, +0x14..+0x1b. Callers
0x000D9AA0 (thunk), 0x000D8122, 0x000D9410. [O]

#### 0x000D8A4A `rseq_substate_compute` — logic
Computes sub-state +0x13 (0..4) from the condition flags +0x14..+0x1b (set from mode +0x21
and calib +0x54 states) with hysteresis between 0/1/2 and terminal 3/4; illegal values
report event (0x2af,0xf,0,5,7,0xed); sets +0x11/+0x1c when sub-state is 1/2. Callee
0x000B79E4. Caller 0x000D8122. [I]

#### 0x000D8C3A `rseq_ramp_machine` — logic
Five-state float ramp engine on +0x44 (0 hold, 1 ramp toward target, 2 window wait, 3 latched
countdown writing u16 +0x3c, 4 ramp-down): target from table byte via calib+0x58 query ×
DAT_000D8E80, step = delta/period (PTR_FUN_000D8E84), clamps to a calibration byte ×factor;
transitions 0→1→2→3→4 with guards on speed window calib +0x24/+0x28 vs input[0x96] and
timestamp margins; default state reports event (0x2af,0xf,0,3,2,0x1b) and resets. Caller
0x000D82AE. [I]

#### 0x000D8F58 `rseq_substate55_step` — logic
Three-state loop on +0x55 (0/1/2) setting byte +0x10 from the +2 flag and a speed threshold
comparison (input+0x94 timestamp vs calib+100 u16 pair) — a direction/hysteresis sub-machine.
Caller 0x000D8122. [I]

#### 0x000D8FD8 `rseq_state_dispatch` — dispatch
Nine-state dispatcher on +0x21 (0..8): abort flag (0x000D97D4, +0xd bit0) forces state 6;
per-state condition handlers (0x000D9448 for 0 →1; 0x000D9582/0x000D983E for 1 →8/2;
0x000D95F4/0x000D97DE for 2 →8/3; 0x000D9724/0x000D97E8/0x000D9658 for 3 →7/4/0;
0x000D96CC for 4, 0x000D9790/0x000D97E8 for 7, 0x000D9650 for 8, 0x000D9816 for 6 →0);
transitions via 0x000D9234; on +0x47 change runs the per-state entry action (0x000D98AE,
0x000D98D4, 0x000D98F8, 0x000D99DA, 0x000D9A04, 0x000D9A3E, 0x000D9A74, 0x000D9AA0) then
the per-state query (0x000D98D2, 0x000D98F6, 0x000D9916, 0x000D9A02, 0x000D9A3C,
0x000D9A72, 0x000D9A9E, 0x000D9AA6); illegal state reports (0x2af,0xf,0,1,10,0x4c). [O]

#### 0x000D91E4 `rseq_target_index_report` — logic
When +0x1f set and +4 clear: scans i=1..7 comparing global byte (PTR_DAT_000D92BC) against
the calib +0x6c table values and reports the first passing index through
DAT_000D92C8(PTR_DAT_000D92C4, idx). Caller 0x000D82AE; callees 0x000B6C8A, 0x000BA51C. [I]

#### 0x000D9234 `rseq_state_set` — logic
Sets state +0x21 to the argument and latches the changed flag +0x47=1 when it differs.
Callers 0x000D9410, 0x000D8FD8. [O]

#### 0x000D9254 `rseq_hold_active_flag` — stub
+0x1d = (byte DAT_000D92CC != 0 && state +0x21 in {3,7}). Caller 0x000D8122. [O]

#### 0x000D9274 `rseq_flag46_state` — logic
Two-state latch on +0x45 (set when state +0x21==3, cleared in states 0/6): while latched sets
+0x46 and +0x12 when the global code at 0xD9424==8; else mirrors code 0xD92D0==8 into +0x46;
illegal +0x45 reports event (0x2af,0xf,0,5,0xb,0x12). Caller 0x000D8122. [O]

#### 0x000D934E `rseq_substate52_step` — logic
Three-state loop on +0x52 (0/1/2) driven by the timestamp margin pair (0xD943C/0xD9440) vs
speed samples at input+0x94 and thresholds behind 0xD9430 (+0x14/+0x84); state 1 exit sets
done +0x51=1 and drops to 0. Caller 0x000D8122. Callees 0x000B7E00, 0x000B7D1E. [I]

#### 0x000D9410 `rseq_state0_reset` — logic
Composite reset: forces state 0 via 0x000D9234, clears the flag block through thunk
0x000D8A2C, then runs the state-0 query 0x000D9AA6. Caller 0x000D8122. [O]

#### 0x000D9448 `rseq_state0_permission` — logic
State-0 entry permission: requires input[0x11]==1, mode +0x20 in {0,1}, +2 clear, selector
query clear, and archived flags clear; then ORs failure causes into +0x53 (bits 0x10
permission, 1/2 speed window, 0x20 curve monotonicity via 0x000D89DE, 0x40 timestamp
margin, 8 +4/+0x46, 4 selector idx5) returning true only when none fired. Caller 0x000D8FD8. [I]

#### 0x000D9582 `rseq_state1_cond` — logic
State-1 condition (→8): returns 0 (condition met) only when input[0x11] enabled (not 0/2),
permission +0x49 set, selector query not 1 or mode 0, +0x54 set, two selector queries not 1,
and +4 or +0x46 set. Caller 0x000D8FD8. [I]

#### 0x000D95F4 `rseq_state2_cond` — logic
State-2 condition (→3/8): returns 0 only when permission +0x49 set, mode +0x20 not 0/3/4,
+0x54 set, two selector queries not 1, and +4 or +0x46 set. Caller 0x000D8FD8. [I]

#### 0x000D9658 `rseq_state3_cond` — logic
State-3 condition (→7/0): returns 0 only when +0x51 != 1, mode +0x20 not 0/1, +0x54 set,
selector query not 1, permission +0x49 set. Caller 0x000D8FD8. [I]

#### 0x000D96CC `rseq_state4_cond` — logic
State-4 condition: true (stay) when mode +0x20 in {0,1}, or selector query==1, or ramp value
+0x24 (float) below the calib +0x48 byte with +0x46 clear. Caller 0x000D8FD8. [I]

#### 0x000D9724 `rseq_state3_alt_cond` — logic
State-3 alternate condition (→7): needs mode +0x20 not 0/1 with +0x54==1 and selector query
clear AND one of {global 0xD9944==1 with calib +0x5c/+0x68 flags, global 0xD9948==1}.
Caller 0x000D8FD8. [I]

#### 0x000D9790 `rseq_state7_cond` — logic
State-7 condition: returns 0 only when timer u16 +0x30 != 0, +0x51 != 1, mode +0x20 not 0/1,
+0x54 set, selector query not 1. Caller 0x000D8FD8. [I]

#### 0x000D97D4 `rseq_abort_flag` — stub
Returns (+0xd bit 0) == 1 — the abort/force bit the dispatcher 0x000D8FD8 uses to force
state 6. [O]

#### 0x000D97DE `rseq_mode2_check` — stub
Returns mode byte +0x20 == 2 (dispatcher state-2 extra path). [O]

#### 0x000D97E8 `rseq_state2_exit_cond` — logic
Returns 1 when mode +0x20==4, or mode 3 with calib +0x7c u16 <= timer +0x32, while substate
+0x52==2. Caller 0x000D8FD8. [I]

#### 0x000D9816 `rseq_state6_exit_cond` — logic
State-6 (abort) exit: returns 1 when the selector query (0xD9938/0xD993C) is clear and the
abort bit +0xd bit0 is clear. Caller 0x000D8FD8. [I]

#### 0x000D983E `rseq_state1_alt_cond` — logic
State-1 alternate condition (→2): needs global u16 0xD994C <= calib +0x10 pair and (selector
query via 0xD9950 set or calib +0x50 flag); mode +0x20 in {1,2}; a failed timestamp check
(0xD9958 query vs +0x3e) clears the result and sets +0x53 bit 2. Callees 0x000BA212,
0x000AEBD6. [I]

#### 0x000D98AE `rseq_state1_entry` — logic
State-1 entry action: zeroes output bytes +0/+1(=0xff)/+7/+8/+9(=0xff)/+0xa/+0xb/+0xc and
+0x48. Caller 0x000D8FD8. [O]

#### 0x000D98D4 `rseq_state2_entry` — logic
State-2 entry action: same output reset as 0x000D98AE but +0xa=0. Caller 0x000D8FD8. [O]

#### 0x000D98F8 `rseq_state3_entry` — logic
State-3 entry action: partial output preset +7=0,+8=1,+9=0xfe,+0xa=1,+0xc=0,+0x48=0.
Caller 0x000D8FD8. [O]

#### 0x000D9916 `rseq_state1_query` — logic
State-1 cyclic query: under calib +0x78 flag, latch +0xb (0/1) from the speed-sum window
(calib +8/+0xc vs 0xD9BD0 and selector margin) snapshotting timer +0x30 from calib +0x2c;
output byte +0/+1(0xff/0xfe) follows calib +0x88 flag and global 0xD9BDC==1. Caller
0x000D8FD8. [I]

#### 0x000D99DA `rseq_state4_entry` — logic
State-4 entry action: output preset +0=1,+1=0xfe,+7=1,+8/+9(0xff)/+0xa/+0xb/+0xc=0,+0x48=0.
Caller 0x000D8FD8. [O]

#### 0x000D9A04 `rseq_state7_entry` — logic
State-7 (hold) entry action: output preset +7=1,+8=1,+9=0xfe,+0xa=1,+0x48=1; when +0x46
clear also requests hold (+2=1, +0x22=1) and zeroes timer +0x38. Caller 0x000D8FD8. [I]

#### 0x000D9A3E `rseq_state8_entry` — logic
State-8 entry action: output preset +0=1,+1=0xfe,+7=1,+8/+9(0xff)/+0xa=0,+0xb=1,+0xc=0 and
loads timer +0x30 from the calib u16 at +0x2c. Caller 0x000D8FD8. [O]

#### 0x000D9A74 `rseq_state6_entry` — logic
State-6 (abort) entry action: output preset +7=0,+8=1,+9=0xfe,+0xa=1,+0xb/+0xc=0,+0x48=1.
Caller 0x000D8FD8. [O]

#### 0x000D9AA6 `rseq_state0_query` — logic
State-0 entry/query: resets output bytes (+0/+1=0xff/+7/+8=1/+9=0xfe/+0xa=1/+0xb/+0xc);
raises hold flag +0x48=1 only when the input block is disabled (input[0x11] 0/2 with calib
+0x20 byte clear, or ==2), permission +0x49 and validity +0x4a clear, and the timestamp
margin covers speed input+0x94. Callers 0x000D8FD8, 0x000D9410. [I]

#### 0x000D9B24 `rseq_selector_decode` — logic
Decodes the three selector switch inputs at block bytes +0xb/+0xd/+0xf (states 0/1/2/3,
input[0x11]==1 required) into mode codes: any input 3→5, (1,1,2)→1, (0/1,1,1)→2,
(2,1,1/2)→3, (0/1/2 pattern)→4, (0,1,2)→0; unrecognized patterns report event
(0x2af,0xf,0,2,4,0x10) and return 0. Caller 0x000D8698 (mode capture). Callee 0x000B79E4
(event report). [I]

#### 0x000D9C54 `solcur_obj_new` — logic
Allocates the 12-byte channel object via heap fn(0xc) when handed NULL, installs the
vtable-ish PTR_LAB_000D9E64 at +8 and stores the two id bytes at +0/+1. Callers
0x00107B80, 0x00107EFE, 0x00107F6E (r10_app solenoid band, spec 12). [O]

#### 0x000D9C7C `solcur_chan_value_get` — data-accessor
Resolves a channel value: if the selector lookup (0xD9E68/0xD9E6C) misses, calls the
object's vtable method (+0x14 offset table at +8); otherwise fetches the u16 via 0xD9E70
and publishes it through 0x000D9F70 (value + ~value mirror tables). Caller 0x000DA00C. [I]

#### 0x000D9CBC `solcur_mode56_value` — logic
Returns the s16 at table 0xD9E7C[0] when the selector query is clear or the mode byte is
5/6, else 0. Callers 0x00107CFC, 0x00107D90, 0x00107D6A, 0x0010810A, 0x0010809C,
0x000D9FEC. [I]

#### 0x000D9CEE `solcur_entry1_get` — stub
Returns the s16 at *(0xD9E7C table)+4 — the second entry of the same table read by
0x000D9CBC. Callers 0x00107CFC, 0x00107D6A, 0x00107D90, 0x0010809C, 0x0010810A. [O]

#### 0x000D9CF8 `solcur_tables_clear` — logic
Clears 12 slots of the parallel u16 tables at DAT_000D9E80 (values := 0) and its +0x18
companion (mirror := 0xffff). Caller 0x000DA00C. [O]

#### 0x000D9D18 `solcur_demand_curve` — logic
Per-solenoid demand lookup for ids 0..6: selects the primary table pointer (0xD9E84) or an
alternate source (override-flag object from 0x000D9FA6, flags at +0/+4/…/+0x34), interpolates
via 0xD9E88(value, x) and post-transforms with 0xD9E9C; unknown id reports event
(0x1b9,10,id) and halts(4). Callers 0x00107FE0, 0x00107C3E (solenoid current compute,
spec 12). [I]

#### 0x000D9E4A `solcur_mirror_get` — stub
Returns the u16 at DAT_000D9E80[idx] — the value table written (with ~mirror) by
0x000D9F70. Facade caller 0x000B8558. [O]

#### 0x000D9E54 `solcur_table2_get` — stub
Same indexed u16 read but on the second table behind PTR_DAT_000D9EA0. Facade caller
0x000B8566. [O]

#### 0x000D9EA4 `solcur_interp2src` — logic
Two-source interpolation: source byte 0/1 selects a pointer (override flags from the
0xD9FAA object at +0xc/+4) out of the table behind 0xD9FB0, then the curve-map call
0xD9FB8(ptr, x, byte 0xD9FB4). Caller 0x000D9F96. Callees 0x000D9FAA, 0x000B7164. [I]

#### 0x000D9EF6 `solcur_interp_fixed` — library
Single interpolation through 0xD9FB8 on the pointer at table(0xD9FBC)+4 with the shared
scale byte 0xD9FB4. Caller 0x00107C3E. [O]

#### 0x000D9F10 `solcur_tempcomp_calc` — logic
Float compensation: takes the source value via 0x000D9F62, selects table +0x24/+0x28 by
byte +1 of the argument object, computes via float op 0xD9FC0 and converts with 0xD9FC4 —
temperature-style correction of a demand value. Callers 0x00107FE0, 0x00107C3E. Callees
0x000B7910, 0x000B7472, 0x000D9F62. [I]

#### 0x000D9F44 `solcur_xform_interp` — logic
Transforms the u16 argument via 0xD9FC8 then interpolates it through 0xD9FB8 on table
slot 0xD9FCC with the s16 scale at 0xD9FD0. Caller 0x00107FE0. [I]

#### 0x000D9F62 `solcur_src_u16_get` — stub
Indirect getter: returns the u16 from PTR_FUN_000D9FD8(PTR_DAT_000D9FD4). Caller
0x000D9F10. Callee 0x000BAA5E. [O]

#### 0x000D9F70 `solcur_value_write` — data-accessor
Publishes a channel value: stores value at +2 and ~value at +4 of the channel descriptor,
then writes both u16s into the parallel tables at 0xD9FDC/0xD9FE0 indexed by the channel
byte at +0. Caller 0x000D9C7C. [O]

#### 0x000D9F96 `solcur_interp_chan` — stub
Wrapper: passes the object's source byte (+1) and x into the two-source interpolation
0x000D9EA4. Callers 0x00107FE0, 0x00107C3E. [O]

#### 0x000D9FA6 `solcur_flagtbl_get` — stub
Returns PTR_DAT_000D9FE4 — the override-flag object consumed by 0x000D9D18. Caller
0x000D9D18 only. [O]

#### 0x000D9FAA `solcur_flagtbl2_get` — stub
Returns PTR_DAT_000D9FE8 — the flag object consumed by 0x000D9EA4. Caller 0x000D9EA4
only. [O]

#### 0x000D9FEC `solcur_chan_output_get` — logic
Per-channel output read: resolves the channel object via 0x000DA032 and returns the s16
from indirect call 0xDA0B8(obj) (0 when the channel id resolves to NULL). Facade caller
0x000B8450. [I]

#### 0x000DA00C `solcur_allchans_step` — logic
Cyclic driver over all 12 channel objects: one global call 0xDA0BC first, then per index
0..11 resolves the object via 0x000DA032 and calls 0xDA0C0(obj). Caller 0x000DC076. [I]

#### 0x000DA032 `solcur_chanobj_addr` — data-accessor
Maps channel index to its 0x28-byte RAM object: 0→0xFFF8B154, 2→0xFFF8B17C, 4→0xFFF8B1A4,
1→0xFFF8B1CC, 3→0xFFF8B1F4, 5→0xFFF8B21C, 6→0xFFF8B22C, 7→0xFFF8B23C, 11→0xFFF8B24C,
10→0xFFF8B25C; other ids return NULL. Callers 0x000DA00C, 0x000D9FEC. [O]

#### 0x000DA0C4 `solcur_deact_if_flag` — logic
When the channel object is enabled (+1) and the selector query (0xDA324/0xDA320) is set,
runs the deactivation 0x000DA152. Caller 0x00108184. Callee 0x000BAA88. [I]

#### 0x000DA0E2 `solcur_latch_step` — logic
Channel update: source byte from 0x000DA1FC, filtered through 0x000DA2E8, archived via
0x000DA2D2, stored at +0, then the timer/report path 0x000DA2A6 with the argument byte.
Callers 0x001081DC, 0x00108234. Callees 0x000DA2A6, 0x000DA1FC, 0x000DA2D2, 0x000DA2E8. [I]

#### 0x000DA10A `solcur_debounce_step` — logic
Debounced variant of 0x000DA0E2: takes the source twice via 0x000DA1FC; equal values that
differ from the stored byte are forced to 0 before the same filter/archive/store/timer
chain. Caller 0x001081C0. [I]

#### 0x000DA152 `solcur_deactivate` — logic
Full channel deactivation: when enabled (+1), runs the report/query 0xDA328 on 0xDA320 and
clears both the value byte +0 and the enable +1. Callers 0x000DA1A8, 0x000DA0C4,
0x000DA2D2, 0x001081FC. Callee 0x000BAAF0. [I]

#### 0x000DA16C `solcur_disable` — stub
Clears value +0 and enable +1 when the channel object is enabled. Caller 0x00108212. [O]

#### 0x000DA17A `solcur_enable_on_match` — logic
Enables the channel object when the mapped source 0x000DA1FC equals the wanted non-zero
value: sets enable +1, stores the value and notifies via 0xDA32C(0xDA320, value, param_3).
Caller 0x000DC48A. Callees 0x000DA1FC, 0x000BAB0A. [O]

#### 0x000DA1A8 `solcur_mode5_gate` — logic
Mode-5 gated activate/deactivate: deactivates (0x000DA152) unless mode (0xDA334) is 5 with
stored value 1 and query 0xDA338 clear; symmetrically enables (+1=1, value 1, notify
0xDA33C) when disabled under those conditions. Caller 0x00108224. [I]

#### 0x000DA1FC `solcur_srcmap_lookup` — library
Byte-table lookup PTR_LAB_000DA340[idx+1] for indices -1..8; out-of-range reports event
(0x18c,0xf,0,1,1,0x80) and returns 0. Callers 0x000DA10A, 0x000DA0E2, 0x000DA17A. [O]

#### 0x000DA248 `solcur_enabled_p` — stub
Returns *obj != 0 — the channel-enable predicate. Cluster of 2: this copy is called by
0x000DA2A6/0x000DA250; the 0x000DD930 copy by 0x000BA8D0. [O]

#### 0x000DA250 `solcur_state_ready_p` — logic
When the channel is enabled: resolves its service object via 0xDA34C and returns 1 when
the vtable +0x38 method (object +0x30 payload) reports 0. Caller 0x00108294. Callee
0x000E55DC. [I]

#### 0x000DA280 `solcur_query7_byte` — stub
Indirect byte query (0xDA350)(0xDA320, 7). Caller 0x000DC4C4. Callee 0x000BA8C2. [O]

#### 0x000DA290 `solcur_query7_word` — stub
Chained query: s16 = (0xDA354)(0xDA320,7) then (0xDA358)(s16). Caller 0x000DC4C4. Callees
0x000BA8B4, 0x000B77BC. [O]

#### 0x000DA2A6 `solcur_change_notify` — logic
Edge-triggered notify: when the channel is enabled and (+1 is clear or param differs from
the global at 0xDA35C), latches +1 and reports through 0x000DA310. Callers 0x000DA10A,
0x000DA0E2. [O]

#### 0x000DA2D2 `solcur_archive_or_deact` — logic
When enabled and the stored value differs from the new one, runs the deactivation
0x000DA152 (value-change teardown). Callers 0x000DA10A, 0x000DA0E2. [I]

#### 0x000DA2E8 `solcur_zero_if_flag` — logic
Suppresses a non-zero candidate value when the service object's vtable +0x5c method
returns set (inhibit filter). Callers 0x000DA10A, 0x000DA0E2. Callee 0x000E55DC. [I]

#### 0x000DA310 `solcur_notify_call` — stub
Single indirect call (0xDA360)(0xDA320, value, param). Caller 0x000DA2A6. Callee
0x000BAAF8. [O]

#### 0x000DA364 `solcur_obj8_new` — logic
Allocates the 8-byte service object via heap fn(8) when handed NULL and presets +1=0xff,
+3=0xff. Callers=- (pointer-table ctor). Callee 0x0009AA34. [O]

#### 0x000DA388 `cfg_variant_apply` — logic
Validates the variant id read indirectly into RAM 0xFFF8B270..0xFFF8B273 (mirror pairs,
sum-check 0xff; failures report 0x76/0xf events and force 0), then on change stores the
mode byte into the object at 0xDA598, runs one of two large fan-out lists of indirect
init calls (mode==1 vs other) and rebinds ~70 two-tier pointer tables to
table[mode*4 + cur*8] — variant-dependent calibration pointer re-binding. Caller facade
0x000B912C. Callees 0x00143A80, 0x0007C6E0, 0x00143A8E, 0x000B79E4, 0x0007C708. [O]

#### 0x000DA418 `cfg_variant_full_init` — logic
1461-instruction master variant/configuration initializer (callers=-, pointer table;
Ghidra shows unaffiliated r11/r12/r13 state): forces the cached variant invalid
(+2=0/+3=0xff), re-runs the 0x000DA388 fan-out and pointer-table rebinds, then calls all
ten cfg_ptr_slot a/b setters (0x000D7F9C..0x000D80B8) plus ~200 init functions across the
0x106Cxx-0x107Axx solenoid band. Spec 12 solenoid configuration entry point. [I]

#### 0x000DB620 `modreq_obj8_new` — logic
Allocates the 8-byte object via heap fn(8) when handed NULL and zeroes +4. Caller
0x0015BFE8. Callee 0x0009AA34. [O]

#### 0x000DB63C `modreq_decode` — dispatch
Big request-code decoder over the state object (PTR_DAT_000DB6A4: mode at +6, code at +7):
codes 0x00..0x28 map to results 0..8 with plausibility gates on state[0x2b]/[0x2c] and
queries (0xDB95C/0xDB964); param_3==2 attaches the 0xFFF8B27C object (vtable +0x20/+0x38)
and stores param_2 at 0xFFF8B280; invalid codes report event (0x49a,sub,param_4); latches
the code into 0xDBAB4 and notifies channel 0x27 via 0xDBABC when flag 0xDBAB0 is set.
Serves the facade family 0x000BB014..0x000BB098. Callees 0x000B8396, 0x000BA6F4,
0x000B79E4, 0x000BAB7A. [I]

#### 0x000DB99E `modreq_hold_p` — logic
Returns 1 (hold/inhibit) unless global 0xDBAC0 is clear and both queries (0xDBAC8 ptr-pair
and 0xDBAD0) return clear. Facade 0x000BB028. Callees 0x000BB7D4, 0x000A031C. [I]

#### 0x000DB9D2 `modreq_hold2_p` — logic
Variant of 0x000DB99E: inhibit unless the 0xDBAC8 query and global byte 0xDBAD8 are both
clear. Facade 0x000BB036. Callee 0x000A031C. [I]

#### 0x000DB9FA `modreq_flag_p` — stub
Returns *PTR_DAT_000DBAC0 != 0. Facade 0x000BB044. [O]

#### 0x000DBA04 `modreq_mode_active_p` — logic
True when state[7] set and mode state[6] in {2,3,4,6,8,0x1a} (with 3 requiring
*0xDBAE0) or submode state[0xb1] in {1,2,7}. Facade 0x000BB052. [I]

#### 0x000DBA62 `modreq_mode_group2_p` — logic
True when state[0xb5] clear and mode state[6] in {0x12,4,0x17,0x19,0x18,'#',0x1b,0x1c,
0x1d,0x1e,'%','\''} or global 0xDBAE4 set. Facade 0x000BB060. [I]

#### 0x000DBAE8 `modreq_inhibit_p` — logic
Composite inhibit predicate: state[0xb2], state[7], mode/submode exclusions, the
0x000DBDE6 gate and the state[0x2c]/[0x2b] pair gates all combine — returns 0 only when
every branch is inactive. Facade 0x000BB06E. Callee 0x000DBDE6. [I]

#### 0x000DBBB4 `modreq_pair_active_p` — logic
Table-style predicate over (state[7], state[6]) pairs — (5,0x17),(5,0x19),(3,0x18),
(1,'#'),(2,0x1b),(3,0x1c),(4,0x1d),(6,0x1e),(7,'%'),(8,'\''),(6,'5') — or mode/submode
{6,8,0x1a}/{1,2,7}. Facade 0x000BB07C. [I]

#### 0x000DBC86 `modreq_state_e0_get` — stub
Returns *DAT_000DBCE0. Facade 0x000BB08A. [O]

#### 0x000DBC8E `modreq_state_e4_get` — stub
Returns *DAT_000DBCE4. Facade 0x000BB098. [O]

#### 0x000DBC96 `modreq_state_e8_get` — stub
Returns *DAT_000DBCE8. Facade 0x000BB0A6. [O]

#### 0x000DBC9E `modreq_flag_ec_p` — stub
Returns *DAT_000DBCEC != 0. Facade 0x000BB0B4. [O]

#### 0x000DBCA8 `modreq_mode_ok_p` — logic
Returns 0 when state[7] is set and the mode-class table 0x0006344C[mode] is 1 or submode
state[0xb1] is set or global 0xDBCF4 is set. Facade 0x000BB0C2. [I]

#### 0x000DBCF8 `modreq2_mode_ok_p` — logic
Second state instance (0xDBEC0): true when state[7] set and mode +6 is 7, 0x15 or 0x13.
Facade 0x000BB0D0. [I]

#### 0x000DBD18 `modreq2_dispatch1` — stub
Forwards (param_2) to service 0x000F0612 via PTR_FUN_000DBEC8/DAT_000DBEC4. Facade
0x000BB0DE. Cluster of 2 with 0x000DBD20 (differing service pointer). [O]

#### 0x000DBD20 `modreq2_dispatch2` — stub
Identical forwarder but through PTR_FUN_000DBECC → service 0x000F0602. Facade 0x000BB0E6. [O]

#### 0x000DBD28 `modreq2_dispatch3` — stub
Argument-less forward through PTR_FUN_000DBED0 → service 0x000F055E. Facade 0x000BB0EE.
Cluster of 2 with 0x000DBD30. [O]

#### 0x000DBD30 `modreq2_dispatch4` — stub
Argument-less forward through PTR_FUN_000DBED4 → service 0x000F05FA. Facade 0x000BB0F8. [O]

#### 0x000DBD38 `modreq_vt2c_call` — logic
If the attached object (param+4) is non-NULL, invokes its vtable +0x2c method with the
+0x24 payload. Facade 0x000BB1C4. [O]

#### 0x000DBD4E `modreq_request_set` — logic
Validated request setter: accepts (value,code) pairs 0, 1/2 with nonzero value, 0x2e and
0x2f; stores {+0 value, +1 code, +3 sub when code 2}; invalid pairs report event
(0x49a,2,value,code,7,0xac); then notifies via 0xDBEE0 and clears value/+3 when code 0.
Caller 0x000DBDCA. Callees 0x000B90B2, 0x000B79E4. [I]

#### 0x000DBDCA `modreq_request_set2` — logic
Wraps 0x000DBD4E with sub=1 and sets the validity flag +2 = (code != 0). Callers
0x000F1C16, 0x000EFF9C, 0x000F21BA, 0x000F1F38 (0xF00xx request source module). [I]

#### 0x000DBDE6 `modreq2_inhibit_p` — logic
Mirror of the 0x000DBBB4 (code,mode) pair-table predicate on the second state 0xDBEC0
(mode 3 also requires global 0xDBEE4). Caller 0x000DBAE8. [I]

#### 0x000DBEE8 `pmsvc_query_store` — stub
Duplicate of 0x000DCC92: stores the byte from service (0xDCE1C)(0xDCE18) into param+1.
Facade caller 0x000B85D0. Callee 0x000AEA16. [O]

#### 0x000DBEEC `pmsvc_register` — logic
Publishes param+6 into the registry service (0xDC148/0xDC144) and registers four more
service callbacks, including the solenoid channel-object resolver 0x000DA030. Facade
caller 0x000B85E0. Callees 0x0010844E, 0x000BAE46, 0x0010CD0E, 0x000DA030. [I]

#### 0x000DBF12 `perm_pair_dispatch` — logic
Walks the pair lists for row param_2 (1..5, stride 0xe) selected by param_3 (1 → table
0xDC178, 3 → 0x506e2 variant): pairs with exactly one zero side report event
(0x193,0xf,0xe/0xd,0xe7 or 0xf/0xe,1); complete pairs dispatch through (0xDC174)(state,
1|3,a,b); out-of-range row/type reports (0x193,0xe,0x10/0x11,0x14/0x1b). Facade caller
0x000B854E. Callees 0x000AEB6A, 0x000B79E4. [I]

#### 0x000DC076 `solcur_periodic_step` — logic
Cyclic tick: indirect call 0xDC17C then (0xDC184)(0xDC180, 0x34), resolving to the
solenoid-channel sweep 0x000DA00C and 0x000BA6F4. Facade caller 0x000B85C0. [I]

#### 0x000DC08A `perm_check_p` — logic
Returns the permission verdict of 0x000DCD5A(param_1, param_2). Facade 0x000B8410. [O]

#### 0x000DC096 `perm_check_clear_p` — logic
Negation of 0x000DC08A (returns 1 when 0x000DCD5A reports 0). Facade 0x000B8420. [O]

#### 0x000DC0A6 `perm_err_report` — logic
param_2==1 reports event (0x193,0xf,9,0xe8); param_2==3 returns 2 when global 0xDC188
< 3 else 0. Facade 0x000B8430. Callee 0x000B79E4. [O]

#### 0x000DC0F8 `perm_follow_verified` — logic
Follows the verified code: when state(0xDC18C)[0x2c] equals param+2 copies state[0x27]
to param+3; otherwise when the gate differs from 0 runs 0x000DC1C4 and the service
object's vtable +0x38 check to latch param+2 from global 0xDC194. Facade 0x000B85D8.
Callees 0x000DC1C4, 0x000E55DC. [I]

#### 0x000DC198 `perm_tbl2d_get` — data-accessor
Returns table 0x000DC3DC[param_2+1 + (param_3+1)*10] — a 2D permission matrix (10x10).
Callers 0x000DC48A, 0x000B852A. [O]

#### 0x000DC1AE `perm_tbl2d_b_get` — data-accessor
Same 2D indexing on the second matrix DAT_000DC3E0. Facade 0x000B853C. [O]

#### 0x000DC1C4 `perm_tbl_a_get` — data-accessor
Returns the byte vector DAT_000DC3E4[param_2]. Callers 0x000DC0F8, 0x000B84FA,
0x000DC1E2, 0x000DC282. [O]

#### 0x000DC1CE `perm_tbl_b_get` — data-accessor
Returns the byte vector DAT_000DC3E8[param_2]. Callers 0x000B850A, 0x000DC282. [O]

#### 0x000DC1D8 `perm_tbl_c_get` — data-accessor
Returns the byte vector DAT_000DC3EC[param_2]. Callers 0x000DC282, 0x000B851A. [O]

#### 0x000DC1E2 `perm3d_get` — logic
Mode-dependent permission lookup for byte param_4: default 3D table
0xDC410[param_4 + (param_3+1)*6 + param_2*0x3c]; the service mode (0xDC3F4) selects
special tables (mode 2 → 0xDC40C, mode 5 → 0xDC408) and a query/inhibit path can force
the 0x63126 variant or 0 when the gate matches. Callers 0x000DCD5A, 0x000DC4C4. Callees
0x000DC1C4, 0x000B6A98, 0x000B6B9A, 0x000B6A90. [I]

#### 0x000DC282 `perm_gated_get` — logic
Selector 1/2/3 picks the vector table (0xDC1C4/0xDC1CE/0xDC1D8) by param_4; types 2/3
apply the service vtable +0x5c inhibit and the 0xDC424/0xDC428 consistency query; other
selectors report event (0x193,0xf,0xc,0x11,0xcf). Facade 0x000B84E6. Callees 0x000DC1C4,
0x000DC1CE, 0x000DC1D8, 0x000BA1AE, 0x000B79E4, 0x000E55DC, 0x000BA1D8. [I]

#### 0x000DC37E `perm_speed_ok_p` — logic
True when param_2 is 0, or global 0xDC438==1 with 0xDC434 clear and the calib byte
behind 0xDC43C+8 <= speed byte at param+6. Facade 0x000B8440. [I]

#### 0x000DC3AA `solcur_change_report` — logic
Reports a pending code change: when both new flags are 0, the codes differ and param+4 is
clear, dispatches (0xDC444)(0xDC440, p2, p3) resolving into the 0x1081C0 debounce path.
Facade 0x000B83CA. [I]

#### 0x000DC448 `solcur_flag_dispatch` — logic
Selector: param_2==0 calls (0xDC644)(0xDC640) → 0x00108212, else (0xDC63C)(0xDC638) →
0x0010CCC2. Facade 0x000B83E6. [O]

#### 0x000DC45E `solcur_flag_dispatch2` — logic
Selector: param_2==0 calls (0xDC64C)(0xDC638) → 0x001081F6, else (0xDC648)() →
0x001081FC. Facade 0x000B83EE. [O]

#### 0x000DC472 `solcur_req_set` — stub
Sets request byte +5=1 and calls (0xDC650)(0xDC638) → 0x00108224 (mode-5 gate path).
Facade 0x000B83F6. [O]

#### 0x000DC47E `solcur_req_clear` — stub
Clears +5 and calls (0xDC654)(0xDC638, p2) → 0x00108234 (latch-step path). Facade
0x000B83FE. [O]

#### 0x000DC48A `perm_matrix_enable` — logic
Matrix-gated channel enable: looks up the 2D matrix at RAM 0xFFF8E6BF via 0x000DC198 and
feeds (value, param_3) into 0x000DA17A (enable-on-match). Facade 0x000B8406. [I]

#### 0x000DC4A8 `meas_word_get` — library
For param_3==0: chains (0xDC660)(param_2) → 0x000E4C8E then (0xDC664)(obj) → 0x000E513C
and returns the s16 — a measurement/signal-block word fetch. Facade 0x000B84D4. [I]

#### 0x000DC4C4 `perm_step_arbiter` — logic
782-instruction master step of the permission/output arbiter: refreshes the source object
(0xDC670/0xDC674) and applies code-tagged actions (0xDC680/0x000DCDF4/0x000DCB7C with
tags 0,1,2,3,4..0x11,0xb..0xf) driven by per-channel checks; loops channels 1..5 through
the 3D permission lookup 0x000DC1E2 with the exclusion predicate 0x000DCE9A and master
gate 0x000DCE40; ends with the 0x33 notify and 0xDCE08 tick. Facade 0x000B85C8; callees
span the 0xBA7xx-0xBAExx service band plus 0x000DCE9A/0x000DCE40/0x000DC1E2/0x000DA280/
0x000DA290. [I]

#### 0x000DCC70 `meas_scaled_get` — logic
Scaled query: u16 = (0xDCE0C)(param_1, p2), multiplied by s16 at 0xDCE10 / 1000, converted
via 0xDCE14 → s16. Facade 0x000B82D0. Callees 0x000B7888, 0x000D7D62. [O]

#### 0x000DCC92 `pmsvc_query_store` — stub
Stores the service byte (0xDCE1C)(0xDCE18) into param+1. Callers=- (pointer table; the
duplicate 0x000DBEE8 serves facade 0x000B85D0). Callee 0x000AEA16. [O]

#### 0x000DCCA2 `perm_chan_map_init` — logic
Registers the channel map (1→9, 2→10, 3→1, 4→2, 6→3, 8→4) through 0x000DCCDC and
notifies channel 9 via the service vtable +0x98. Caller thunk 0x000DC086. [O]

#### 0x000DCCDC `perm_chan_register` — logic
Registers one channel pair: param_2 > 9 reports event (0x193,0xf,1,0xe,0x41); param_3 < 6
is pushed through the service object vtable +0x98. Caller 0x000DCCA2. Callees 0x000B79E4,
0x000E55DC. [O]

#### 0x000DCD38 `perm_park_ok_p` — logic
True when query (0xDCE2C,5) is clear and global 0xDCE34 is clear. Caller 0x000DCE40.
Callee 0x000BB13E. [I]

#### 0x000DCD5A `perm3d_verdict` — logic
Composite verdict for the 0xDC08A/0xDC096 facades: gated by query (0xDCE2C,0xff); requires
the 3D permission 0x000DC1E2 on both the current (+0x21) and verified (+0x27) codes with
the service vtable +0x20/+0x38 checks and the state[0x2c] flag. Callees 0x000BB13E,
0x000E513C, 0x000DC1E2. [I]

#### 0x000DCE40 `perm_master_gate` — logic
Master enable: state 0xDCEC0 pair (+0x2b/+0x2c==1) or query 0xDCEC8==1, the park check
0x000DCD38, global 0xDCECC clear and query 0xDCED0==1. Caller 0x000DC4C4. Callees
0x000BA2AC, 0x000BA2A4, 0x000DCD38. [I]

#### 0x000DCE9A `perm_case5_excl` — logic
Exclusion for the arbiter loop: true when param_3==5, param_2==1 and query 0xDCED8 is
clear. Caller 0x000DC4C4. Callee 0x000B6BA8. [O]

#### 0x000DCEDC `permchan_value_get` — logic
Per-code channel value: codes 7/0xa/0xb short-circuit to 0; code 6/5 consult selector
ptr-pair 0xDD0E8/0xDD0EC; code 4 needs query 0xDD0F8; otherwise returns
(0xDD104)(0xDD100, code) when global 0xDD0FC is clear. Facade 0x000BB100. Callees
0x000BA74C, 0x000DD1C2, 0x000A031C, 0x000B8450. [I]

#### 0x000DCF7C `permchan_active_p` — logic
Per-code activity: code 3 → global 0xDD108, code 4 → 0xDD114, else the state[0xb2]/
[0xd5] gate plus global 0xDD110. Facade 0x000BB110. [I]

#### 0x000DCFCE `permchan_code6_p` — stub
Wrapper: 0x000DCFDA(param_1, 6). Facade 0x000BB130. [O]

#### 0x000DCFDA `permchan_code_p` — logic
General per-code gate (codes 4/5/6/0xa/0xb): combines the state[0xb2]/[0xd5] gate, the
selector queries, submode state[0xb1] in {1,2,7} for code 5, and the global 0xDD110
fallback. Callers 0x000DCFCE, 0x000BB13E, 0x000DD1B4. Callees 0x000BA74C, 0x000DD1C2,
0x000A031C. [I]

#### 0x000DD118 `permchan_code_validate` — logic
Validates codes {0,1,2,7} (6 maps to 0); anything else reports event (0x49c,0xf,1,6,0x1a)
and returns 1. Facade 0x000BB15C. Callee 0x000B79E4. [O]

#### 0x000DD16C `permchan_idx_validate` — logic
Validates index <= 0x35; out-of-range reports event (0x49c,0xf,2,6,0x41) and returns 0.
Facade 0x000BB16C. Callee 0x000B79E4. [O]

#### 0x000DD1B4 `permchan_all_p` — stub
Wrapper: 0x000DCFDA(param_1, 0xff). Facade 0x000BB14E. [O]

#### 0x000DD1C2 `permchan_mode_query` — logic
Codes 7/0xa/0xb select the payload ptr-pairs 0xDD204/0xDD208/0xDD20C and return the query
through 0xDD210; other codes return 0. Callers 0x000DCFDA, 0x000DCEDC. Callee 0x000A031C. [O]

#### 0x000DD214 `reqarb_obj12_new` — logic
Allocates the 0x12-byte object via heap fn(0x12) and presets +2=0x31ce, +8=1, +0xe=1000.
Callers=- (pointer-table ctor). Callee 0x0009AA34. [O]

#### 0x000DD23E `reqarb_code_get` — logic
Returns the code byte +0 only when enabled (+4); disabled reports event
(0x2ee,0xf,4,2,0xa4) and returns 0. Facade 0x000BA1D8 (used by 0x000DC282/0x000DC4C4).
Callee 0x000B79E4. [O]

#### 0x000DD280 `reqarb_start` — logic
Start: indirect (0xDD434)(0xDD430) then sets *PTR_DAT_000DD438 = 1. Facade 0x000BA1F4
(caller 0x00104A28). Callee 0x000AEBA4. [O]

#### 0x000DD292 `reqarb_flag5_set` — stub
Sets request byte +5 = 1. Facade 0x000BA178 (caller 0x0010D5E4). Cluster of 2 with
0x00141714 (r14_app copy, caller 0x00132D8E). [O]

#### 0x000DD298 `reqarb_flag5_clear` — stub
Clears request byte +5 = 0. Facade 0x000BA180 (caller 0x000CFD1A, the rseq hold path). [O]

#### 0x000DD29E `reqarb_ready_p` — logic
True when param_2 non-zero, query (0xDD43C)(0xDD430) set and global 0xDD440 clear.
Facade 0x000BA202 (caller 0x00103CEC). Callee 0x000AEBC8. [O]

#### 0x000DD2C4 `reqarb_hold_p` — logic
Hold predicate: 1 unless global 0xDD444 clear, query (0xDD44C)(0xDD448) clear, 0xDD450
clear and query (0xDD458)(0xDD454,0) clear. Callers 0x000BA1E6, 0x000DD79E. Callees
0x000BB036, 0x000B6B82. [I]

#### 0x000DD2F8 `reqarb_step` — logic
Cyclic arbiter step over the {code+0, substate+1 (0/1/3), enable+4} state: refreshes four
source queries; active+ (query(code) or 0x000DD524) tears the request down through
0x000DD400(0,0,sub) with notify; disabled path latches substate 1/code from query, else
substate 3 with the verify chain (0xDD490/0xDD494/0x000DD4F8); success applies
0x000DD400(1,code,sub) and notifies. Facade 0x000BA238 (caller 0x000A2F34). Callees
0x000DD400, 0x000D64D2, 0x000B8420, 0x000BAB14, 0x000D777E, 0x000DD79E, 0x000DD4F8,
0x000AEBAC, 0x000DD524, 0x000B8410, 0x00106064, 0x000D5FBA, 0x000B8430. [I]

#### 0x000DD3C2 `reqarb_target_fetch` — logic
Substate-3 target: query (0xDD498)(0xDD47C); when set stores the u16 from
(0xDD49C)(0xDD47C, sub) at +0xa, then (0xDD4A4)(0xDD4A0); substate 0 returns
(0xDD4A8)(0xDD4A0) directly. Caller 0x000DD400. Callees 0x000B82E2, 0x000D71B2,
0x000B822C, 0x000D7224. [I]

#### 0x000DD400 `reqarb_apply` — logic
Applies the arbiter decision: enable+4 := param_2; substate 1 dispatches
(0xDD4AC)(0xDD430,p2,p3), substate 3 runs 0x000DD3C2 then (0xDD718)(0xDD714,p2,p3);
other substates report event (0x2ee,0xf,7,5,0x2e). Caller 0x000DD2F8. Callees 0x000BAAB2,
0x000DD3C2, 0x000B79E4, 0x000AEB9A. [I]

#### 0x000DD4F8 `reqarb_verify_gate` — logic
Verify gate: false when +5 is clear and the state pair 0xDD724[0x2b]/[0x2c] differ;
otherwise returns query (0xDD72C)(0xDD728). Caller 0x000DD2F8. Callee 0x000D724C. [I]

#### 0x000DD524 `reqarb_substate_query` — logic
Substate dispatch: 1 → query (0xDD734)(0xDD730), 3 → query (0xDD738)(0xDD728), 0 →
event (0x2ee,0xf,6,0x92), others → event (0x2ee,0xf,7,0xba) returning 1. Caller
0x000DD2F8. Callees 0x000D725A, 0x000B79E4, 0x000AEBBA. [O]

#### 0x000DD59E `reqarb_done_p` — logic
Completion: with enable+4==1, substate 1 needs query (0xDD73C)(0xDD730)==1; substate 3
needs global 0xDD740 in {0,6}; other substates report event (0x2ee,0xf,8,3,0xcc); result
latched at +0x10. Facade 0x000BA1B6. Callees 0x000B79E4, 0x000AEB8C. [I]

#### 0x000DD61A `reqarb_delta_calc` — logic
Returns (s16)(0xDD748)(0xDD730,p2) − (s16)(0xDD74C)(0xDD730,p2) when the mode query
(0xDD744) is set, else 0. Caller 0x000DD65C. Callees 0x000AEB1A, 0x000AEB0A, 0x000AEAFA. [O]

#### 0x000DD654 `reqarb_mode_capture` — stub
Stores *PTR_DAT_000DD740 into +0xc. Caller 0x000DD65C. [O]

#### 0x000DD65C `reqarb_timeouts_update` — logic
Refresh: target u16 via (0xDD754/0xDD758)(0xDD750, code) into +0xa, mode captures
(0xDD75C)(0xDD730) and (0xDD760)(0xDD728), 0x000DD654 and 0x000DD76C; computes the
timeout s16 at +2 — substate 1: 0x000DD61A delta + (0xDD764)(0xDD730); substate 3:
delta + *(s16*)0xDD768; other substates event (0x2ee,0xf,7,6,0x6b); default 20000.
Facade 0x000BA240. Callees 0x000B82E2, 0x000AEB84, 0x000AEB76, 0x000D7102, 0x000DD61A,
0x000DD76C, 0x000DD654, 0x000B822C, 0x000B79E4. [I]

#### 0x000DD76C `reqarb_period_fetch` — logic
Fetches two u16s via (0xDD7B0) from the payloads at 0xDD7B4 and (0xDD7AC+0x10 when
state[0x34] set) into +0xe and +6. Caller 0x000DD65C. Callee 0x000B7910. [O]

#### 0x000DD7B8 `lever_code_word_get` — dispatch
s16 getter by code 0..9: 0 selects (0xDD87C)(0xDD878) or (0xDD888/0xDD880), 1..3 fixed
queries (0xDD890/0xDD894/0xDD89C), 4..9 the shared (0xDD8A4)(0xDD8A0); other codes report
event (0x373,0xf,1,0xc,0x3f). Facade 0x000BA8B4 (also reached from solcur_query7_word).
Callees 0x000B6ACE, 0x000B6AE2, 0x000EAE8C, 0x0010FF9A, 0x000EAEC4, 0x000B79E4,
0x001132B8, 0x000EEB32. [I]

#### 0x000DD862 `lever_code_byte_get` — dispatch
Byte getter by code 0..9 mirroring 0x000DD7B8 (globals 0xDDAC4/0xDDAC8/0xDDAD4 and
queries 0xDDAD0/0xDDADC); default reports event (0x373,0xf,1,0xc,0x88). Callers
0x000BA8C2, 0x000DDB04, 0x000DE3E0. Callees 0x000B6ACE, 0x000EAE82, 0x000EEB40. [I]

#### 0x000DD916 `lever_evt_report` — stub
Reports event (0x373,0xf,1) unconditionally. Callers=- (pointer table). Callee
0x000B79E4. [O]

#### 0x000DD938 `lever_substate_p` — stub
Returns substate byte +1 != 0. Facade 0x000BA8DE. [O]

#### 0x000DD940 `lever_tbl_query` — stub
Query (0xDDAE8)(0xDDACC, state+7, param_2). Facade 0x000BAA96. Callee 0x000EB7BC. [O]

#### 0x000DD954 `lever_position_step` — logic
Position tracker: computes the candidate via 0x000DE190 from state 0xDDAF0 codes
(+0x21/+0x24/+0x27) and param+7; on change with the pending/verified handshake latches
+0x14/+1 (conflict reports 0x373/0x1b/6/0xd6), stores the new position, notifies via
(0xDDAF4)(0xDDACC, verified, current, +5, +6), archives 0xDDAEC, and flags +8 when
current matches +0x24. Facade 0x000BAC1E. Callees 0x000DE190, 0x000DDBB4, 0x000EB9EC,
0x000B79E4. [I]

#### 0x000DDA54 `lever_target_reached_p` — logic
True when position +7 equals the target and the 0xDDAF0 pair 0x2b/0x2c is 1/1; when the
0xDDACC flag is set also requires globals 0xDDAF8/[+4] and then fires (0xDDAFC).
Facade 0x000BA84E. Callee 0x000EB1E8. [I]

#### 0x000DDAAA `lever_pos_match_p` — stub
Returns *PTR_DAT_000DDB00 == position +7. Caller 0x000EE18E. [O]

#### 0x000DDAB6 `stbyte_set` — stub
Two-instruction byte setter `obj+off = val`. Cluster of 7 duplicated field-setters, each
bound to a different owner/offset: 0x000DDAB6→+0x14 (lever pending latch, caller
0x000DDB04), 0x000EAD94→+0x29 (callers 0x0010D5E4/0x001132F4), 0x000F8AB8→+0x65,
0x000F8B1A→+0x7d, 0x000F8D0C→+0x9f (callers 0x000F8420), 0x000FA3A6→+0x68 (caller
0x000F9418), 0x00101B5E→+0x15 (r10_app, caller 0x0012A2B6). [O]

#### 0x000DDB04 `lever_release_check` — logic
When position +7 differs from verified 0x27: engaged pair (*p==+6, +7==+0x24 of state
0xDDD64) with query 0xDDD6C and the margin test (0xDDD70 + 0xDDD78(p,code,byte 0xDD862)
vs 0xDDD80) clears the request; and when +1==+5 with queries 0xDDD88/0x000DE3E0 passes,
archives +1 into +0x14 via 0x000DDAB6, clears +1 and notifies (0xDDD84)(0xDDD68).
Facade 0x000BAC16. Callees 0x000B84D4, 0x000EB712, 0x000EAE8C, 0x0010FF9A, 0x000DD862,
0x000DDAB6, 0x000DE3E0, 0x000EB6FC, 0x000EB8F8. [I]

#### 0x000DDBA8 `lever_pos_changed_p` — stub
Returns position +7 != *PTR_DAT_000DDD8C. Callers 0x000BA85E, 0x000EE18E, 0x000EB73E,
0x000E630C, 0x00117622, 0x000E65F4. [O]

#### 0x000DDBB4 `lever_pair_update` — logic
Updates the pending/verified byte pairs of the lever state (+0/+1 or +5/+6) through the
table queries (0xDDD98/0xDDD9C)(0xDDD74, a, b) gated by query 0xDDD94; param_4 selects
which pair. Callers 0x000DDDE8, 0x000DDE78, 0x000DD954. Callees 0x000B853C, 0x000B7C02,
0x000B852A. [I]

#### 0x000DDC14 `lever_notify_2` — stub
Indirect notify (0xDDDA0)(0xDDD68, p2, p3). Callers 0x000BAAF8, 0x000DDC26. Callee
0x000EBD84. [O]

#### 0x000DDC1E `lever_notify_1` — stub
Indirect notify (0xDDDA4)(0xDDD68, p2). Facade 0x000BAB02. Callee 0x000EBD9E. [O]

#### 0x000DDC26 `lever_flagd_notify` — stub
Sets +0xd=1 then delegates to 0x000DDC14. Facade 0x000BAB0A. [O]

#### 0x000DDC30 `lever_action_call` — stub
Argument-less indirect action (0xDDDA8)(0xDDD68). Facade 0x000BAAF0. Callee 0x000EBDB4. [O]

#### 0x000DDC38 `lever_commit` — logic
Commit path: 0x000DE2AC(param,p4,1); equal codes report event (0x373,0xf,10,10,10);
notify (0xDDDB4)(0xDDD68,p2,p3,0,0,0) then (0xDDDBC)(0xDDDB8,0,0) and (0xDDDC4)(0xDDDC0).
Facade 0x000BAAA6. Callees 0x000EEB80, 0x000EEF42, 0x000B79E4, 0x000EBB6C, 0x000DE2AC. [I]

#### 0x000DDCA2 `lever_engage` — logic
Engage path: 0x000DE2AC(param,p4,0); equal codes report event (0x373,0xf,0xb,10,0x5c);
optional query (0xDDDCC)(0xDDD90); refresh 0x000DE2FA; copies input+0x94/+0x7e into
+0x10/+0xa; the six-argument notify (0xDDDD8)(0xDDD68,p3,+2,+3,+4,*0xDDD8C,changed-flag)
plus (0xDDDDC)(0xDDDC0,+2,p3!=0) and (0xDDDE4)(0xDDDE0,p3). Facade 0x000BAABC. Callees
0x000B83B4, 0x000DE2FA, 0x000B7B14, 0x000EEDFC, 0x000B79E4, 0x000E984A, 0x000DE2AC,
0x000EB900. [I]

#### 0x000DDDE8 `lever_preset` — logic
Preset/refresh: (0xDE044)(0xDE040), 0x000DE2FA, pair update 0x000DDBB4(p2,p3,0), copies
input 0x94/0x7e into +0x10/+0xa, notifies (0xDE050)(0xDE04C,...,p4,p5) and (0xDE058)
(0xDE054), gates (0xDE064) on global 0xDE05C in {0,3,4}. Facade 0x000BAAC8. Callees
0x000DDBB4, 0x000DE2FA, 0x000EBA6E, 0x000E9508, 0x000EAC64, 0x000EECEC. [I]

#### 0x000DDE78 `lever_restore` — logic
Save/restore variant: keeps the old code, updates pairs via 0x000DDBB4(p2,p2,0), runs
four notifies (0xDE068/0xDE06C/0xDE070/0xDE078/0xDE080) and rolls the code back when
query (0xDE088)(0xDE084) is set. Facade 0x000BAAE4. Callees 0x000DDBB4, 0x000B6AC4,
0x000AEE06, 0x000EBC60, 0x000EEF42, 0x000B8574, 0x000E958A. [I]

#### 0x000DDED6 `lever_abort` — logic
Abort: equal codes report event (0x373,0xf,0xc,0xb,0x96); with p3 set and state 0xDE08C
pair 0x2b!=0x2c runs 0x000DE2AC(param, state[0x27], 0) and the two flag notifies on
+2/+3/+4. Facade 0x000BAB14. Callees 0x000EEB80, 0x000B79E4, 0x000EBB6C, 0x000DE2AC. [I]

#### 0x000DDF76 `lever_query_a4` — stub
Returns (0xDE0A4)(0xDE09C). Facade 0x000BAA88. Callee 0x000EEAE0. [O]

#### 0x000DDF84 `lever_min_time_get` — logic
Returns the s16 at 0xDE0A8 clamped to at least the s16 at 0xDE054 (minimum dwell).
Facade 0x000BA874. [I]

#### 0x000DDF98 `lever_dir_classify` — logic
Classifies +0xc := 0/1/2 by the sign of s16 0xDE0A8 (zero with 0xDE054==0 → 0, negative
→ 2, else 1) — direction of the pending lever travel. Caller 0x000DE0D8. [I]

#### 0x000DDFBC `lever_flush5` — stub
Runs the same action (0xDE0B0) over the five service objects 0xDE0AC/0xDE0B4/0xDE0B8/
0xDE0BC/0xDE0C0. Facade 0x000BAD18. Callee 0x000EFC3C. [O]

#### 0x000DDFD8 `lever_duty_calc` — logic
With global 0xDE0C4 set runs the 0xDE3B8/0xDE3CC refresh; computes the u16 at +0xe as
interp(0xDE0CC)(0xDE0C8, input+0x76, input+0x70) → (0xDE0D0)(v, input+0x6e) →
v × byte[0xbf]/100 (a duty/percent); then 0x000DE2FA and the auto-clear when the
query is clear and 0x27==0x21. Facade 0x000BAC26. Callees 0x000B6AC4, 0x000B7BD2,
0x000B7888, 0x000DE2FA, 0x00104D4C, 0x000DE3B8, 0x000DE3CC. [I]

#### 0x000DE0D8 `lever_dir_update` — logic
Two source queries (0xDE30C/0xDE314) then the direction classify 0x000DDF98 and the
notify (0xDE31C)(0xDE318). Facade 0x000BAC2E. Callees 0x000E94EE, 0x000DDF98,
0x000E5EAA, 0x000E9820. [O]

#### 0x000DE0F6 `lever_tick` — stub
Single indirect tick (0xDE324)(0xDE320). Facade 0x000BAC36. Callee 0x000EB288. [O]

## Unit summary
Unit 28 (r0c_app) — 220 clusters, 230 named functions (10 clusters have extra member
copies). Kinds: logic 137, stub 56, data-accessor 28, library 5, dispatch 4. Unknown: 0 —
every cluster fit a named module family, though many carry [I] inference markers.

Module map established by this sweep:
- `modreg_/cfg_ptr_slot*` 0x000D7DDE–0x000D80B8 — variant/modulation register reports and
  the ten variant config-pointer slots applied by 0x000DA418.
- `rseq_*` 0x000D80D4–0x000D9B24 — the 9-state range-sequencer (selector decode, ramp
  engine, per-state condition/entry/query handlers); range codes 5–8 (spec 14 §4.1
  domain, spec 09 context).
- `solcur_*` 0x000D9C54–0x000DA388 — solenoid-current demand module: 12 channel objects in
  RAM 0xFFF8B154..0xFFF8B25C, value/~mirror u16 tables, per-solenoid demand curves with
  alternate-source selection (spec 12).
- `cfg_variant_*` 0x000DA388/0x000DA418 — variant validate/apply fan-out and the master
  init (~200 calls into 0x106Cxx–0x107Axx).
- `modreq_*` 0x000DB620–0x000DBF12 (+0x000DBD18/0x000DBD20/0x000DBD28/0x000DBD30 members)
  and `pmsvc_*` — request-code manager behind facade family 0x000BB014..0x000BB1C4.
- `perm*/*` 0x000DBF12–0x000DCE9A — permission-matrix module (2D/3D tables at
  0x000DC3DC..0x000DC410, event 0x193) behind facades 0x000B84xx.
- `permchan_*` 0x000DCEDC–0x000DD1C2 — channel-permission getters/validators
  (event 0x49c) behind facades 0x000BB100..0x000BB16C.
- `reqarb_*` 0x000DD214–0x000DD7B8 — request arbiter with substate 0/1/3 verify chain
  (event 0x2ee) behind facades 0x000BA1xx/0x000BA240.
- `lever_*` 0x000DD7B8–0x000DE0F6 — lever/position manager (pending/verified code pairs,
  engage/commit/abort/duty, event 0x373) behind facades 0x000BA8xx–0x000BAC36.
- `stbyte_set` cluster — 7 duplicated 2-instr field setters across r0c/r10/r14.

Cross-region leads:
- 0x000DA418 calls ~200 init functions in 0x106Cxx–0x107Axx (r10_app solenoid band) —
  the r10_app unit should cross-check those init targets against cfg_variant_full_init.
- Facade families in r08_app (0x000B84xx/0x000B85xx, 0x000BA1xx–0x000BAC36,
  0x000BB014–0x000BB1C4) front this unit's managers; consumers span r08 (0x000A2xxx,
  0x000B0xxx, 0x000B5xxx), r0c (0x000CFD1A, 0x000DD2F8 chain), r10 (0x00103CEC,
  0x00104A28, 0x0010D5E4, 0x0012A2B6) and r14 (0x00132D8E).
- Event families for spec 13 cross-referencing: 0x2af (rseq), 0x1b9/0x18c (solcur),
  0x49a/0x49c (modreq/permchan), 0x193 (perm), 0x2ee (reqarb), 0x373 (lever), 0x76
  (variant validation).
- 0x000DE190/0x000DE2AC/0x000DE2FA/0x000DE3E0 (lever helpers) and 0x000F05xx–0x000F21xx
  (modreq subscribers) are r0c_app functions outside this unit's cluster slice — whoever
  holds those clusters should cross-check the lever/reqarb interfaces documented here.
