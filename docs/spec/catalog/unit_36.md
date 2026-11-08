# Unit 36 (r10_app)

Application-region clusters 0x00108294–0x0010FA7E (plus duplicated members in 0x00112xxx/0x001173xx).

#### 0x00108294 `app_iface_ready_check` — logic
Reads an OS-service object via pointer table (PTR_FUN_0010833c/40/44) and the vtable-style call
`(**(code**)(*(int*)(obj+0xc)+0x38))(...)`; returns 1 while an async operation is pending or ready
to collect. Calls 0x001084CE/0x001084D4; called by 0x00108184 (service shutdown sequence from
0x000DC4C4). Cluster of 1. [I]

#### 0x0010834C `app_async_op_state_step` — logic
Four-state machine over struct {idx, state, u16 result}: state 0 starts a service job via
PTR_FUN_00108420 (mode gate on PTR_DAT_00108414/18), 1-2 poll completion via PTR_FUN_0010842c,
3 waits while *p < element count at (*(int*)PTR_DAT_00108430)+0xc. Finally fetches the u16 result
through FUN_001083F4 and scales it via PTR_FUN_00108434 into p+2. Called by 0x0010843C. Cluster of 1. [I]

#### 0x001083F4 `app_async_result_read` — logic
Returns the s16 element `*p` read via PTR_FUN_00108438 from the buffer at object+8
(PTR_DAT_00108430) when the state byte p[1]==3, else 0. Callee of 0x0010834C and 0x00108476. [O]

#### 0x0010843C `app_svc_check_then_collect` — logic
Sequencer: invokes PTR_FUN_0010853c(PTR_DAT_00108538) then FUN_00108476 (result collect step).
9 instrs; called by 0x00108184 in the service-drain sequence. Cluster of 1. [I]

#### 0x0010844E `app_thunk_arg_plus4` — stub
3-instr tail thunk: `(*DAT_00108540)(param_1 + 4)` — pointer-table trampoline adjusting the arg
by +4. Called from 0x000DBEEC (r0c_app). Cluster of 1. [O]

#### 0x00108454 `app_async_op_restart` — logic
Calls FUN_00108470 (start primitive, outside unit) then clears the state byte *param=0 so the
0x0010834C state machine restarts from idle. Callers 0x001081FC, 0x00108574, 0x0010858A. [I]

#### 0x00108462 `app_latch_flag_nonzero` — logic
7 instrs: if *param != 0 then param[1] = 1 — one-way latch of a boolean pair. Called by
0x00108184. Cluster of 1. [O]

#### 0x00108476 `app_result_collect_step` — logic
If the 6-byte ctx (p[1]) is active: checks readiness via the vtable call at (obj+0xc)->+0x38,
then computes a u16 through PTR_FUN_00108554 (using PTR_DAT_00108548/4c/50) plus an offset from
PTR_FUN_00108558/5c, storing it at p+2. Cluster of 1. [I]

#### 0x001084D4 `app_queue_count_lt` — logic
Single comparison: returns **(byte**)(*(int*)PTR_DAT_00108548+4) < *(byte*)(param+4) — element
count of a buffer object vs. an index; guard used by 0x00108294. Cluster of 1. [O]

#### 0x001084E8 `app_ctx6_get_or_alloc` — logic
Returns param or allocates a 6-byte block via PTR_FUN_00108560(6) (allocator 0x0009AA34 family),
then zeroes the {idx,state,u16 result} ctx. Callee of 0x001085E2. Cluster of 1. [O]

#### 0x0010850A `app_gate_param_by_flags` — logic
Clears param_2 when service flag PTR_FUN_00108568(PTR_DAT_00108564)==1, or when
PTR_FUN_0010856c(...)!=0 and *PTR_DAT_00108570==1 — a validity gate. Called by 0x00108590 path.
Cluster of 1. [I]

#### 0x00108574 `app_lookup_wrap_store` — logic
Wrapper used by 0x001081C0/0x001081DC/0x00108234: calls PTR_FUN_0010860c() then
FUN_00108590(param, sel) and stores the returned byte at *param. Cluster of 1. [I]

#### 0x00108590 `app_table_lookup_assert` — dispatch
Range-checks selector -1..8; out of range raises the DET-style error call
(PTR,0x188,0xf,0,1,0,0xba,...) and returns 0, otherwise dispatches through the jump table
PTR_LAB_00108610 and converts via PTR_FUN_00108614. Cluster of 1. [O]

#### 0x001085E2 `app_ctx6_init_or_check` — logic
If param is NULL, allocates 6 bytes (PTR_FUN_00108620, allocator family 0x0009AA34); on success
runs the checker PTR_FUN_00108624 on the ctx and returns it, else 0. Callers resolved: none
(pointer-table use). Cluster of 1. [I]

#### 0x00108628 `app_obj32_alloc_init` — logic
Allocates a 0x20-byte object if param==0 (PTR_FUN_001086a4) and seeds +4/+8 from the constant
pair DAT_001086a8/001086ac (linked-list head/tail style init). Callers resolved: none. [I]

#### 0x00108652 `torque_ratio_filter_hold` — logic
Big (196 instr) transmission-math helper: computes a gear-style ratio f(int16 PTR_DAT_001086b0,
args) and its reciprocal into p[1]/p[2] (sign by param_2), then a switch(param_3) 0..0xb sums
interpolated terms from module tables (+0xc/+0x10/+0x14). Tail is a hold/ramp limiter: hold
counter p+0x16, rates from tables at +0x1a8/+0x1ac/+0x1b0, threshold +0x1bc, filtered through
PTR_FUN_00108890(p[0],p[4],p+7). Returns the hold flag p[5]. 17 callers across 0x10A-0x10C.
Cluster of 1. [I]

#### 0x00108864 `app_set_u16_field_0x18` — stub
3 instrs: *(u16*)(param_1+0x18) = param_2 — field setter used by 0x000DE614/0x0010B1EA. [O]

#### 0x00108894 `tbl_2d_bool_select_read` — data-accessor
Two-dimension read of a u32/float table: base = PTR_LAB_00108aec or PTR_LAB_00108af0 by boolean,
entry = base + (param_3&0xff)*4 + (param_2&0xff)*0x24 (rows of 9 entries). Callee of 0x001093EC
and 0x001096FC. Cluster of 1. [O]

#### 0x001088B6 `app_obj_alloc_init_0x9c` — logic
Allocates a 0x9c-byte object if param==0 (PTR_FUN_00108af4(0x9c)) and writes the same constant
DAT_00108af8 to +0x38 and +0x4c (queue/list anchors). Called by 0x000DE5CC (r0c_app). [I]

#### 0x001088DE `torque_target_mode_calc` — logic
330-instr mode engine over a float ctx: mode byte at +0x14 (0..4); modes 1-4 build *p as a
product of two interpolation calls PTR_FUN_00108b28 using tables reached via PTR_PTR_00108b20
(+4/+0x10..+0x1c) and module struct +0x290/+0x294, then convert through PTR_FUN_00108b34 into
p[0x15]; mode 0 calls FUN_00108c8a (reset). Gear-state gating ('\\x06','\\x08') selects the
FUN_00108c18 path; DET error (PTR,0x43d,0xf,0,1,4,0x8f) on bad mode. Called by 0x00108D7C. [I]

#### 0x00108C18 `torque_hold_mode_set` — logic
Sets ctx+0x50 to 1..4 from state byte 8-vs-other at PTR_DAT_00108d60[0]/[1] and routes a table
(+8 or +0xc via PTR_PTR_00108d64+4) with puVar1[0xe] through PTR_FUN_00108d68 into ctx+0x98.
Callee of 0x001088DE; also calls FUN_001093B4 first. Cluster of 1. [I]

#### 0x00108C6A `torque_ctx_reset` — logic
Clears ctx+0x50 then runs the three reset helpers FUN_001096fc/0x001093b4/0x001093ec. Called by
0x000DE614 (r0c_app). Cluster of 1. [I]

#### 0x00108C82 `app_clear_field_0x10` — stub
3 instrs: *(u32*)(param_1+0x10)=0. Called by 0x0010B1EA. Cluster of 1. [O]

#### 0x00108C8A `torque_integrator_reset` — logic
Zeroes the whole filtered-torque state: mode-selected pair p[0x22]/p[0x23] or p[0x24], *p,
+0x14, +0x26, +0xd; returns the mode byte from *(PTR_DAT_00108d4c+700). Callers 0x001088DE,
0x0010B172, 0x0010B1EA. Cluster of 1. [O]

#### 0x00108CBE `torque_ramp_delta_calc` — logic
If the gate call PTR_FUN_00108d68(*(module+0xa8), *PTR_DAT_00108d6c,...) is true, computes
(*PTR_DAT_00108d70 - p[+8]) * (s8)p[+0x58] into p[+0x24] and the global at PTR_DAT_00108d74,
else zeroes p[+0x24]. Called by 0x0010B70C. Cluster of 1. [I]

#### 0x00108D04 `torque_ctx_sign_init` — logic
Clears module fields PTR_DAT_00108d78+0xec/+0xf0 and ctx+4; when state byte changed
(PTR_DAT_00108d60[1] vs [0xc]) sets the direction byte ctx+0x58 to 1 or 0xff. Feeds the sign
used by 0x00108CBE. Cluster of 1. [I]

#### 0x00108D32 `torque_clear_accum_fields` — logic
6 instrs: zeroes ctx+0x94, ctx+0x20 and the packed u16 at ctx+0x2a — partial state reset.
Callers 0x0010B172/0x0010B1EA. Cluster of 1. [I]

#### 0x00108D7C `torque_path_calc_main` — logic
252-instr core of this module: interpolates a base value by mode byte ctx+0xc (tables at
module+0x6c/+0x70/+0x74) into ctx+0x44, multiplies by a rate into +0x1c, adds the 0x00108FD8
term (+0x18) and optional state-6/8 term (+0x30), accumulates into +0x3c, forms the delta +0x10
(states 2/4) and total +0x14 clamped at 0 by direction, packs u16 +0x48 and final +8 = +0x14 * 
ratio. DET error (0x43d,0xf,0,1,5,0x91) on bad mode. 8 callers. Cluster of 1. [I]

#### 0x00108FD8 `torque_addend_interp` — logic
Small addend used by 0x00108D7C: interpolation PTR_FUN_0010904c over table (module+4) with a
boolean third arg (sign of short/const), scaled by short/const. Cluster of 1. [I]

#### 0x00109094 `torque_split_gate_calc` — logic
154 instr: base by mode (+0x88/+0x8c/+0x90) into ctx+0x38; special path when state=='5' and
gate true (ctx+0x28 = table(+0x28c) result, global PTR_DAT_00109314 = (ctx+0x24*b)/DAT); else
filter call PTR_FUN_00109318(0, +0x38). Direction branches (states 2/6/4 vs 9/10) set ctx+0x58=1
and module+0xf0. Final ctx+4 = sign*(sum of the two filtered values). Cluster of 1. [I]

#### 0x00109200 `torque_rate_calc_pack` — logic
Combines direction inputs into a selector (puVar5[0x11]/[0x12]), interpolates into ctx+0x40
(0 when selector==2), second interp by ctx+0xc into ctx+0x4c (DET err 6 otherwise), filters the
pair into ctx+0x94 (PTR_FUN_001095d8), stores +0x20 and packs scaled u16 +0x2a. Zeroes +0x94 on
state 6. Cluster of 1. [I]

#### 0x001093B4 `tbl_mode_pair_select` — data-accessor
Writes a {0|1, table_entry} pair through PTR_PTR_001095e8: entry from PTR_PTR_001095f0 +
idx*4 (+8 when flag set). Callee of 0x00108C18/0x00108C6A. Cluster of 1. [O]

#### 0x001093EC `shift_phase_table_lookup` — dispatch
323 instr mode switch (0..4 via PTR_FUN_001095f8): each case runs the 2D lookup
FUN_00108894(sel, state[0x29], state[0x23]), range-checks 0..0x26 (out-of-range raises the
error call with value, 0x26 and the output address), then indexes an 8-byte-entry second table
by *PTR_DAT_001095ec; stores the phase byte to ctx+0xc. Same shape per case (unrolled switch).
Cluster of 1. [O]

#### 0x001096FC `shift_phase_default_lookup` — logic
Fixed-argument variant of the 0x001093EC lookup chain: FUN_00108894(0,2,1), same 0x26
range-check + second-table resolution. Called by 0x00108C6A reset. Cluster of 1. [O]

#### 0x00109770 `torque_ratio_calc` — logic
Builds p1 = module(+0xa0)+module(+0x9c), p0 = FUN_00109976() (+ correction table/(x*x) by
flags), p2 = p0*p1/(p0+p1) — a parallel-combination of two quantities; when the mode flag
(module+0x8c) is set it routes through FUN_00109816 for p[5]. Called by 0x000E0FE4. [I]

#### 0x00109816 `torque_ratio_clamp_check` — logic
Clamps ctx+0xc between two service bounds (PTR_FUN_001099c0/c4; 2.0f when uVar5==1 and
bVar4==0), computes |x - 2.0|-style deviations and returns which side exceeded (0/1/bVar4).
Callee of 0x00109770. Cluster of 1. [I]

#### 0x0010996A `app_read_tbl_a0` — data-accessor
5 instrs: returns **(undefined4**)(module+0xa0) — dereferenced table head getter. Called by
0x000E0504. Cluster of 1. [O]

#### 0x00109976 `torque_base_calc` — logic
Returns **(float**)(module+0xa4) + interpolation PTR_FUN_001099d0(module+0x98, two state bytes)
— base term of the 0x00109770 combination. Cluster of 1. [I]

#### 0x001099D4 `shift_pack_torque_u16` — logic
Packs three floats into scaled u16s via PTR_FUN_00109c98 with scale DAT_00109c94 (ctx+0x10,
+0x1c, +0x6a from *p, p[3], p[5]), then computes the float residual p[4]=p0-p3-p5 and s16
residual +0x1a so the packed values stay consistent. Callee of 0x00109A5E/0x00109BC4. [O]

#### 0x00109A5E `shift_sched_active_calc` — logic
Active-phase scheduler: forces ctx+0x44=1.0f on state change, adjusts the fVar3 base for
state 8, runs FUN_00109c02 (blend weights) and FUN_00109df0 (torque share), two filter calls
(PTR_FUN_00109ca4/cb0 over +0x30/+0x5c/+0x64/+0x60), then packs via FUN_001099d4 and fills the
3-tap u16 history; clears +0x70/+0x1e/+0x1f. Cluster of 1. [I]

#### 0x00109AFC `shift_sched_inactive_calc` — logic
Same pipeline as 0x00109A5E but with FUN_00109b7a (output reset) instead of the extra filter
calls; state gate: only forces ctx+0x44=1.0 when state[0x1a]==7 while state[4]!=7. [I]

#### 0x00109B7A `shift_outputs_reset` — logic
Flushes the filter (PTR_FUN_00109ca4 with ctx+0xc/+0x17/+0x19/+0x18) then zeroes every packed
output (p[0], +0x10, p[3], +0x1c, p[5], +0x6a, p[4], +0x1a). Cluster of 1. [O]

#### 0x00109BC4 `shift_sched_hold_calc` — logic
Hold-phase variant: FUN_00109c02 + FUN_00109df0, one extra interp (PTR_FUN_00109cb4 on module
+0x20/+0x24) and filter (PTR_FUN_00109cb8), then pack FUN_001099d4 and history shift
FUN_0010a298. Called by 0x0010C8A6. Cluster of 1. [I]

#### 0x00109C02 `shift_blend_weights_calc` — logic
157 instr: renormalizes ctx+0x44 (via **(u16**)(module+0xc4)/DAT vs 2.0f and 1-x), then blends
three channel bases with table sets module+0xac..+0xcc (+0xb0..+0xc0 alternate by flag):
+0x54/+0x50/+0x58 = base + w*(x - base). Cluster of 1. [O]

#### 0x00109DF0 `shift_torque_share_calc` — logic
Computes the three weighted products ctx+0x30/+0x24/+0x28 (interp(ctx+4, ctx+0x18, table) *
weight * scale, two table sets by direction flag), runs FUN_0010a2f0 direction detect, then the
handshake state machine ctx+0x20 (0..3, DET error 0x441/100 on bad) selecting ctx+0x6c from
module+0x9c/+0xa0/+0xa4; final normalized share ctx+0x60 = (2*x + c1)/(2*x + c2),
ctx+0x64 = (2*ctx+0x24)/same. Cluster of 1. [I]

#### 0x0010A004 `shift_share_error_path` — logic
Cold continuation using unaff_r14 (jump-table arrival): raises the DET error (0x441,...) then
computes the same normalized share as 0x00109DF0's tail. Callers resolved: none. [I]

#### 0x0010A016 `shift_torque_share_alt` — logic
222-instr alternate share computation for non-5 states: three products from alternate table
sets (module+0x104..+0x128 selected by state 8 and flag) with floor clamps at DAT_0010a2dc,
then the same 2x/(2x+k) split; clears handshake flags. Callee of 0x00109DF0. [I]

#### 0x0010A26A `shift_delta_filter` — logic
ctx+4 = *PTR_DAT_0010a2d4 - *PTR_DAT_0010a2e4 (two module floats), filtered via
PTR_FUN_0010a2e8(diff, module+300 table) into ctx+0x18. Called by 0x000DE688. [I]

#### 0x0010A298 `shift_u16_history_shift` — logic
Slides the 3-entry u16 window ctx+0x7c/+0x7e/+0x80, appending a freshly packed value
(PTR_FUN_0010a2ec on ctx+0x14). Callee of 0x00109BC4. Cluster of 1. [O]

#### 0x0010A2F0 `shift_direction_detect` — logic
Monotonicity detect over the u16 window: rising → ctx+0x1e=1, falling → ctx+0x1f=1; copies the
window to ctx+0x48..+0x4c and decrements the hold counter ctx+8. Callee of 0x00109DF0. [O]

#### 0x0010A354 `shift_phase9_init` — logic
Init action of the 0x000DEA5E shift-sequence coordinator: zeroes the phase flag *p, arms the
service via PTR_FUN_0010a5f4(PTR_DAT_0010a5f0, gear, 9), sets module+0xc = (m[0xf8]+m[0xfc])*
m[+8], p+8 = (m0 - m+0xc) - m2+0xc, p+0xc=0, state byte at module2+0x14 = 3. Cluster of 1. [I]

#### 0x0010A3A0 `shift_torque_phase_step` — logic
215-instr main step of a two-stage torque handover: refreshes four module services, builds the
interp stack (short/DAT over module+0x140 etc.), branch 0 accumulates p+8 with rate DAT_0010a648
and filters into module0[0]; branch 1 pushes the value through PTR_FUN_0010a644/0010a64c (mode 7)
and stores the delta p+0xc = result - base. Phase flips 0→1 via FUN_0010a7ca when the threshold
(base + p+8) is crossed. Cluster of 1. [I]

#### 0x0010A576 `shift_phase10_init` — logic
Init for the mode-10 phase: p+4 = module0 - m2+0xc, p+0x14=0, arms PTR_FUN_0010a5f4(...,10);
computes module+0xc from (m[0xf8]+m[0xfc])*factor; selects a u16 from module+0x154 interp or the
direct table at +0x158 into p+0x12; sets state byte *PTR_DAT_0010a66c = 3. Cluster of 1. [I]

#### 0x0010A670 `shift_speed_phase_step` — logic
90 instr: decrements the hold counter p+0x12, refreshes services, recomputes module+0xc, then
fVar9 = p+4 - module term, minus an interp over module+0x144/0x148 (flag-selected), and ramps
PTR_FUN_0010a8b8(fVar9, limit, p+0x14); module0 = (p+4 - p+0x14) + m2+0xc. Cluster of 1. [I]

#### 0x0010A742 `shift_phase_teardown` — logic
Closes the phase: unlock/flush sequence of six pointer-table service calls (mode 6) — pure
teardown, no computation. Called by 0x000DEA5E. Cluster of 1. [O]

#### 0x0010A782 `shift_phase_wrapup` — logic
Maps state byte PTR_DAT_0010a8d4[2] (2/3/other) to 2/3/1 into module+0x1b, clamps module0 at 0,
computes module+0x10 = module0 * DAT_0010a8d8, runs the flush PTR_FUN_0010a8dc and zeroes
PTR_DAT_0010a8e0. Cluster of 1. [I]

#### 0x0010A7CA `shift_phase_handover_arm` — logic
Arms the second stage from 0x0010A3A0: two limit calls (PTR_FUN_0010a8f8/0010a900 with mode 7)
against the module+0x160 reference, recomputes the interp base and pushes
(target, ref + base, delta - module+0x15c) through PTR_FUN_0010a910. Cluster of 1. [I]

#### 0x0010A930 `sol_periodic_upkeep` — logic
Decrements the hold counter p+0x18, refreshes the tracking value at p+0x1c via PTR_FUN_0010aba8
(short, table module+0x84), then FUN_0010afe6(p, mode). Called by 0x000E2682. Cluster of 1. [I]

#### 0x0010A968 `sol_duty_ramp_calc` — logic
Ramped PWM-style duty generator: max duty = clamp(interp(byte*param_4/100) - param_3, 0,
0x1fc0=8128 counts), target = min(param_2*p[3]/100, max), then ramps the internal float p+8
toward it via PTR_FUN_0010abc4 (rate from table module+0xb8) and converts back to u16 output.
Drives FUN_0010af0a underneath. Cluster of 1. [O]

#### 0x0010AA82 `sol_engage_action_init` — logic
Sets the mode byte p+0xc from *PTR_DAT_0010abc8, runs FUN_0010afe6, and loads the hold counter
p+0x18 from table module+0xc8(200). Callee of 0x0010AF0A state 0→1. Cluster of 1. [O]

#### 0x0010AAA2 `sol_duty_output_calc` — logic
If the enable flag (module+0xc0) is clear, output 0; else floor = max(*PTR_DAT_0010abc8, scaled
interp of module+0xc4), converts p+0x10 through PTR_FUN_0010abc0 into u16 p+8, runs
FUN_0010afe6 and reloads the hold counter. Cluster of 1. [O]

#### 0x0010AB02 `sol_duty_gear_offset` — logic
Switch on *PTR_DAT_0010abb8 1..8 selects one of the tables module+0x88..+0x9c (+0xa0/+0xa4 for
7/8), interpolates against p+10, then p+6 = (result * percentage)/100 + p+8 — per-gear duty
offset. DET error (0x465,0xf,0,1,2,0x28) on unknown mode. Cluster of 1. [I]

#### 0x0010AC16 `sol_duty_gear_offset_err` — logic
Cold continuation (unaff_r13, callers resolved none) of 0x0010AB02: raises the same DET error
(0x465/0x28) then performs the identical percentage tail computation. Cluster of 1. [O]

#### 0x0010AC6C `sol_output_reset` — logic
12 instrs: p[0xc] = (*p != 0) ? **(short**)(module+0xcc) : 0, and p[3] = 0. Callee of
0x0010AF0A/0x0010AFF4 resets. Cluster of 1. [O]

#### 0x0010AC86 `sol_engage_cond_check` — logic
119-instr guard stack: speed-ratio threshold (module+0xe0 vs p+0x1c/10), counter check
(module+0xf4), temp gate (module+0xe4), slip window (module+0xe8), ratio window (+0xf8 with
flag==2), both shaft speeds around band (0xf0/0xec/0xd4/0xdc vs byte [0x96]), hold counter
expired, gear-state flags (1/2), drive enable — returns 1 only when all pass. [O]

#### 0x0010ADAE `sol_release_cond_check2` — logic
Second release predicate: interp gate on module+0xfc (flag==2 variant), flag *PTR_DAT_0010ae90,
gear != 7, and a module+0xd0 interp value <= p+10. Returns 1 when satisfied. [O]

#### 0x0010AE08 `sol_release_cond_check3` — logic
Third release predicate: (flag==0 or gear==7) and module+0xd0 interp <= p+10. Complement of
0x0010ADAE's gate side. Cluster of 1. [O]

#### 0x0010AE94 `sol_release_cond_check` — logic
Primary release/end predicate: speed below table (module+0xd4), pedal gates, hold counter,
mode-3 special with a float threshold (module+0xd8), and gear 6/8 required. Returns 0 to
release. Cluster of 1. [O]

#### 0x0010AF0A `sol_handover_state_machine` — dispatch
4-state machine on p+4: 0 = wait for FUN_0010ac86 then FUN_0010aa82 (engage); 1 = run
FUN_0010ae94/0x0010adae (not in unit)/FUN_0010ae08 gates, FUN_0010aaa2 on each, →2 on the third
gate; 2 = FUN_0010ae94 then reset via FUN_0010ac6c; 3 = FUN_0010ab02 offset tail. DET error
(0x465,0xf,0,1,3,0xb) on bad state. Cluster of 1. [O]

#### 0x0010AFE6 `sol_hold_counter_load` — logic
6 instrs: *(u16*)(p+10) = (u8)*PTR_DAT_0010b02c - (param_2 & 0xff) — loads the hold countdown
from a calibrated byte minus the current mode. Cluster of 1. [O]

#### 0x0010AFF4 `sol_state_reset` — stub
3 instrs: p+4 = 0 then tail into FUN_0010ac6c reset. Callers 0x0010A922/0x0010A914. [O]

#### 0x0010AFFA `sol_ctx_clear` — stub
7 instrs: zeroes p[1], p+8, p+10 (u32s) and the u16 output *p. Cluster of 1. [O]

#### 0x0010B030 `sol_duty_or_abort_timer` — logic
Two modes: gated path computes *p = clamp(table_byte*param_2/100 - param_3 ± direction byte)
and decrements p[1]; else zeroes *p and sets p[1] = interp(module+0x108)*4/10 — an abort/timeout
timer. Called by 0x000E2682. Cluster of 1. [I]

#### 0x0010B0B4 `sol_duty_ramp_simple` — logic
Simple ramp: interp target from module+0x108, scale via PTR_FUN_0010b14c, then ramp p+2 toward
*param_1 with PTR_FUN_0010b150 (rate byte from module+0x104, 10). Cluster of 1. [O]

#### 0x0010B0F4 `sol_clear_pair` — stub
4 instrs: zeroes the u32 pair at p+4/p+8. Callers 0x000E2682/0x000E289E. [O]

#### 0x0010B100 `sol_duty_scaled_init` — logic
20 instrs: interp target from module+0x108 (shorts), scale (x*4)/10 via PTR_FUN_0010b148 into
the u16 at p+2 — the init form of 0x0010B030's timer branch. Called by 0x000E2682. [O]

#### 0x0010B154 `app_ctx50_get_or_enable` — logic
Returns param or allocates a 0x50-byte block (PTR_FUN_0010b3b8, allocator family) and sets
*p = 1. Called by 0x000DE5CC (r0c_app). Cluster of 1. [O]

#### 0x0010B172 `shift_phase_init_common` — logic
Shared init used by 0x000DEA5E and 0x0010B2EA: four service refreshes, latches the direction
flag into PTR_DAT_0010b3dc, loads/reloads the hold byte p+0x3a from the module+0x23c interp,
zeroes module3e8+8/+0xc and arms PTR_FUN_0010b3f4. Cluster of 1. [I]

#### 0x0010B1EA `shift_phase_post_init` — logic
Post-init: PTR_FUN_0010b3fc(module table +0x1b8 short), computes the u16 limit from
module+0x240 interp into p+0x3c, then clears module+0x1c/p+0x14/p+0xa/p+0x18 and runs the four
service teardowns. Cluster of 1. [I]

#### 0x0010B248 `shift_ramp_step` — logic
Decrements or reloads (from module+0x244) the counter p+0x3e, computes the ramp increment
through PTR_FUN_0010b414/0010b41c (bounded by 1.0f) into p+0x1c/p+0x4c, subtracts it from the
remaining p+0x48, decrements p+0x3c, sets *p=1. Cluster of 1. [I]

#### 0x0010B2EA `shift_sync_phase_step` — logic
125 instr: selects the base table by mode byte PTR_DAT_0010b428[0xb8] (1/2/4/other →
module+0x218/+0x21c/+0x220/+0x214), interpolates, then the release branch: if the hold flag is
set or module0 <= table(+0x228), either runs the 0x0010B1EA/0x0010B172 finish (when
module+0x278 flag clear) or pushes the abort path with *PTR_DAT_0010b6a4 = 0xc and
FUN_0010bbf4; otherwise ramps module0 += DAT*base*factor(+0x224). Cluster of 1. [I]

#### 0x0010B490 `shift_flag_clear_action` — logic
14 instrs: PTR_FUN_0010b6b4(PTR_DAT_0010b6b0,0), clears p+0x14 and p+8, sets *PTR_DAT_0010b6b8
= 1. Simple action of the coordinator. Cluster of 1. [O]

#### 0x0010B4AE `shift_sync_cond_check` — logic
Sets p+0x14 = 1 when service states are quiet (PTR_FUN_0010b6c0/0010b6c8) and the hold counter
p+0x46 expired; additionally p+8 = 1 when the module+0x134 interp is below p+4 (torque low
enough). Clears p+0x14 when busy again. Cluster of 1. [I]

#### 0x0010B534 `shift_fill_init` — logic
Init of the fill phase: copies mode byte into p+0x21, selects table +0x1f8/+0x1fc by flag,
converts through PTR_FUN_0010b6f0 into the ramp bound p+0x28 (u16/DAT) and p+0x40, zeroes the
ramp p+0x24, runs FUN_0010b79e (gear setpoint) into p+0x10 and module+0x6fc, computes the u16
count p+0x46 from module+0x168. Cluster of 1. [I]

#### 0x0010B5F8 `shift_fill_step` — logic
Steps the fill: decrements counters p+0x40/p+0x46, and by flag (module+0x234 == 1) either ramps
p+0x10 from p+0x2c with rate DAT_0010b708 then recomputes the setpoint stack, or adds
setpoint*p+0x24; clamps p+0x24 growth by the p+0x28 bound. Cluster of 1. [I]

#### 0x0010B70C `shift_sched_phase5_init` — logic
Init of the mode-5 scheduler action: five service calls (mode 5), gates the module+0xa8 interp,
and on success computes PTR_DAT_0010b9c0 = (m - m+0xc)*rate and flushes two services; else the
simple close PTR_FUN_0010b9cc. Cluster of 1. [I]

#### 0x0010B79E `shift_gear_setpoint_calc` — logic
341-intr per-gear setpoint engine: gear byte p+0x21 (1..8) selects two interpolation tables
(module+0x198/+0x19c/+0x1a0/+0x1a4/+0x1a8/+0x1ac/+0x1b0/+0x1b4 with companions +0x1bc..+0x1d8)
and an output pointer (+0x178..+0x194); special paths when flag module+0x230 != 1 or p+0x20 set
(tables +0x1e0/+0x1dc/+0x208/+0x210). Stores the sum into p+0x30. DET error (0x445,0xf,0,1,4,
0xaf) on unknown gear. Callee of 0x0010B534/0x0010B5F8. Cluster of 1. [I]

#### 0x0010BB42 `shift_ramp_init` — logic
Init of the timed ramp: clears *PTR_DAT_0010bc64 and *p, computes the u16 bound from the
module+0x1f0 interp into p+0x40 and p+0x28 (scaled by DAT_0010bc7c), zeroes p+0x24 and the
global. Cluster of 1. [I]

#### 0x0010BB8A `shift_ramp_limited_step` — logic
Steps the ramp: decrements p+0x40, advances p+0x24 by DAT_0010bc88 bounded at p+0x28
(PTR_FUN_0010bc8c), then scales (p+0x24/p+0x28) through PTR_FUN_0010bc94 with the module+1f4
table into PTR_DAT_0010bc80. Cluster of 1. [I]

#### 0x0010BBF4 `shift_accum_step` — logic
Integrates the module+0x27c interp value into p+0x18 and publishes p+0xa = convert(p+0x18/100).
Callee of 0x000DEA5E and 0x0010B2EA abort path. Cluster of 1. [O]

#### 0x0010BC24 `shift_offset_multi_init` — logic
108-instr offset init: base term by flag puVar1[0x2d] (module+0x200 interp), arms the mode-3
service, module+0x16c interp added to the state base, plus two more interp terms
(module+0x1e8/+0x1ec) — all converted into p+0x38; sets p+0x44=0xffff, p+0x42=0, optionally
pushes p+0x38 through the service filter, then p+0x34 = readback and p+0x36 = p+0x38+p+0x34.
Cluster of 1. [I]

#### 0x0010BD7E `shift_offset_step` — logic
Follow-up step of 0x0010BC24: re-arms mode 3, reads back p+0x34 via PTR_FUN_0010be00, publishes
p+0x36 = p+0x38 + p+0x34, flushes the state and bumps the p+0x42 progress counter. [O]

#### 0x0010BE0C `tbl_u32_index_read` — data-accessor
5 instrs: returns *(u32*)(PTR_LAB_0010c058 + param_1*4) — direct indexed table read. Callee of
0x0010C402. Cluster of 1. [O]

#### 0x0010BE16 `sol_posctrl_init_base` — logic
Init of the position controller: base from p+0x38 or via FUN_0010c5a0, sets p+0x2c (float from
short table), p+0x30, converts p+4; presets p+0x20/p+0xc = 0xff and the scaled accumulators
p+0x24 = p+0x1c = rate*100; loads p+0x2a from table+0x70. Callers 0x000E2682/0x000E27D2. [I]

#### 0x0010BEA2 `sol_posctrl_init_table` — logic
Table-driven init variant: entry chosen by the byte PTR_DAT_0010c078 from PTR_PTR_0010c07c
(+8/+4 and [0]/[1] variants by flags), interp against the short base, forced 0 when
*PTR_DAT_0010c084 set; sets the same p+0x2c/p+0x30/p+4/p+0x2a/p+0x28/p+0x11/p+1 fields. [I]

#### 0x0010BF40 `sol_posctrl_init_zero` — logic
Zero variant of the init: p+0x2c = short table, p+0x30 = 0, p+4 = convert(p+0x2c), clears
p+0x2a/p+0x28/p+0x11/p+1. Called by 0x000E27D2. Cluster of 1. [O]

#### 0x0010BF74 `sol_posctrl_init_signed` — logic
Signed variant: value = ±interp(module+0x48, PTR_DAT_0010c070[0xe]) by flag p[1], stored as the
float p+0x2c with p+0x30=0 and the standard output preset. Cluster of 1. [O]

#### 0x0010BFC2 `sol_duty_track_step` — logic
252-instr duty/position tracking step: fast path for states 3/4 ((x&0x3fff)<<2/10 from
module+0x40 and ±symmetric duty from module+0x3c); otherwise the table stack interp
(PTR_PTR_0010c300) clamped to 0x1fc0, percentage-scaled (param_2/100) with lower bound
0xffffe040, direction-negated by flag, hold counter p+0x2a vs the puVar13+8 feedback, the
FUN_0010c5a0/PTR_FUN_0010c328 ramp into p+0x1c (x100 scaling), and the final follow of p+0x2c
toward the commanded duty. Cluster of 1. [I]

#### 0x0010C238 `sol_posctrl_step` — logic
162-instr: percent-position calc (0x639c ratio) into p+0x20/p+0x16/p+0xc by ramp position,
the p+0x14 u16 from module+0x64/+0x68/+0x6c tables, optional boost via PTR_FUN_0010c5fc,
ramp of p+0x2c toward p+6 with rate p+0x16 (PTR_FUN_0010c600), linear rundown of p+4 by
delta/counter p+0x18, float→u16 conversion and FUN_0010c672 tail. Cluster of 1. [I]

#### 0x0010C3E0 `sol_aux_ramp_clear` — logic
16 instrs: single ramp call on p+0x34 (target short PTR_DAT_0010c608, rate 0x7d) and clears
p+0x11. Callee of 0x000E2682. Cluster of 1. [O]

#### 0x0010C402 `shift_mode_table_resolve` — logic
185-instr: computes three mode/compare bytes (p+0xd/+0xe/+0xf) via PTR_FUN_0010c610 against
module tables (+0x28/+0x24/+0x38/+0x2c), then the two-level lookup: FUN_0010be0c() value
range-checked 0..0x49 (73, DET-style error with value+addr on violation) indexing the 8-byte
entry table at PTR_PTR_0010c61c (byte PTR_DAT_0010c620 selects the row set); the same block is
repeated for three gate variants. Finishes with p+0x16 and u16 p+0x14. Cluster of 1. [O]

#### 0x0010C5A0 `shift_signed_interp_read` — logic
Direction-aware read: table module+0x54/+0x58/+0x5c chosen by two flags, interpolated against
shorts (PTR_DAT_0010c634+8/+0x14), negated in the first branch. Callee of 0x0010BFC2/0x0010BE16.
Cluster of 1. [I]

#### 0x0010C640 `shift_state_gate_sample` — logic
Samples the state word from *DAT_0010c748: when state==1 and PTR_DAT_0010c74c==5 waits on the
float threshold (returns p+2=1 once below), else copies the u16 puVar2[2] into p+0x18 and
clears p+2. Called by 0x000E2682. Cluster of 1. [I]

#### 0x0010C672 `sol_settle_detect` — logic
Compares converted p+4 vs p+6 (±1 tolerance): mismatch sets p[0x11]=1; also latches p[1] when
the settled flag disagrees with *p and updates p[0x10]. Tail of 0x0010C238. Cluster of 1. [I]

#### 0x0010C6CC `shift_range_mode_map` — logic
Maps the selector global *PTR_DAT_0010c758 onto p[0]: 0x11→1, 0x01→2, 0x10→3, 0→0, anything
else raises DET error (0x471,0xf,0,0xd,3,0x47); latches previous into p[0x3c]. Called by
0x000DF922. Cluster of 1. [O]

#### 0x0010C764 `shift_torque_agg_calc` — logic
Float aggregation over three module objects: m1+0xc = (f8+fc)*rate, m1+8 = rate*m3,
m1+0x10 = f8+fc+m3, m1[0] = m2+0xc + rate*m1+0x10. Callee of 0x0010C896/0x0010C8A6. [I]

#### 0x0010C7AC `shift_phase7_init` — logic
Init of the mode-7 action: service refresh + arm, m1[0x14]=1, m1+0x10 = PTR_DAT_0010c934 *
m1[0], zeroes m1+0x188. Callee of the 0x000DEA5E coordinator. Cluster of 1. [I]

#### 0x0010C7E0 `shift_phase7_step` — logic
Mode-7 step: like 0x0010C7AC but the input is filtered first (PTR_FUN_0010c93c(0, module+0x248,
param)), then m1[0] = p0 + m2+0xc and m1+0x10 = m2[4]*(sum). Cluster of 1. [I]

#### 0x0010C826 `shift_phase6_teardown` — logic
Pure teardown of the mode-6 action: nine pointer-table service calls, no computation. [O]

#### 0x0010C866 `shift_flag_copy_flush` — logic
13 instrs: copies the byte *PTR_DAT_0010c968 into *PTR_DAT_0010c96c, flushes the service
PTR_FUN_0010c970(PTR_DAT_0010c924) and zeroes *PTR_DAT_0010c974. Action of the 0x000DEA5E
coordinator. Cluster of 1. [O]

#### 0x0010C880 `app_copy_flag_global` — stub
5 instrs: *PTR_DAT_0010c96c = *PTR_DAT_0010c968 — byte copy between two globals. [O]

#### 0x0010C88A `shift_ctx_zero_flush` — stub
6 instrs: *param_1 = 0 plus the flush call PTR_FUN_0010c978(PTR_DAT_0010c924). [O]

#### 0x0010C896 `shift_flag_copy_agg` — stub
7 instrs: copies **(byte**)(module+0x98) into *PTR_DAT_0010c97c then runs FUN_0010c764. [O]

#### 0x0010C8A6 `shift_phase6_step` — logic
56-instr action: arms the service with the mode argument *PTR_DAT_0010c980, refreshes, special
flush when states are 5/6, conditional flush on state change, then the FUN_0010c764
aggregation. Callee of 0x000DEA5E. Cluster of 1. [I]

#### 0x0010C994 `app_clear_ctx2` — stub
5 instrs: clears p+2 and *p. Called by 0x000E0FE4. Cluster of 1. [O]

#### 0x0010C99E `shift_ready_state_machine` — logic
Three-state gate machine (p+2): 0 waits for states 4/5 or flag==3 then runs FUN_0010ca60; 1
waits until the service value passes two thresholds and state 5; 2 waits for flag 3. DET error
(0x47c,0xf,0,1,1,0x2d) on any other state. Called by 0x000E106E. Cluster of 1. [I]

#### 0x0010CA60 `shift_scaled_accum_step` — logic
Accumulation step: p[1] = service(module+4) * (m0 - m2+0xc); *p += DAT_0010cad8 * service(m0)
* p[1] * DAT_0010cad4. Callee of 0x0010C99E. Cluster of 1. [I]

#### 0x0010CADC `shift_gear_blend_calc` — logic
Gear-transition blend: when state[0x2b] is 1/2, interpolates between the two gear lookups
FUN_0010cba8(param,[0x27]) and FUN_0010cba8(param,[0x21]) by the progress ratio
(u16 - lo)/(hi - lo) clamped 0..1 (ratios from FUN_0010cbb8); special max value for param_1==4;
result scaled (u16 * blend)/DAT through PTR_FUN_0010cbf0. Called by 0x000E447A. Likely the
shift-map clutch-value blend (spec 09 cross-ref). Cluster of 1. [I]

#### 0x0010CBA8 `tbl_gear_val_read` — logic
8-instr two-argument lookup wrapper: PTR_FUN_0010cbf4(selector, index) → u16. Callee of
0x0010CADC. Cluster of 1. [O]

#### 0x0010CBB8 `tbl_gear_val_scaled` — logic
Returns (lookup(param) * *PTR_DAT_0010cbe8)/DAT_0010cbec as float — scaled form of the gear
value used for the blend ratio. Cluster of 1. [O]

#### 0x0010CBFC `shift_sum_clamp_convert` — logic
Pipeline: FUN_0010cc78 (flag-table term) + FUN_0010cc2a (interp on param_2+0x94) → FUN_0010cc42
min-clamp → convert via PTR_FUN_0010cc88. Called by 0x000E4A4E. Cluster of 1. [O]

#### 0x0010CC2A `tbl_interp_94` — logic
10 instrs: interp of *(param_2+0x94) against PTR_DAT_0010cc8c via PTR_FUN_0010cc90 → s16.
Cluster of 1. [O]

#### 0x0010CC42 `lib_min1_assert` — logic
If param < 1 raises DET error (0x1a3,0xf,0,1,0,0xf7) and returns 1, else passes through —
floor-with-trace helper. Callee of 0x0010CBFC. Cluster of 1. [O]

#### 0x0010CC78 `tbl_flag_read` — logic
8 instrs: PTR_FUN_0010cca0(PTR_DAT_0010cc9c, param_2) → s16 flag-selected table read. [O]

#### 0x0010CCA4 `app_pct_ctx_get_or_init` — logic
Returns param or allocates 4 bytes (PTR_FUN_0010cd5c, allocator family) and sets *p = 100 —
percentage-counter ctx. Callers resolved: none (pointer-table use). Cluster of 1. [I]

#### 0x0010CCC2 `app_pct_ctx_init` — stub
7 instrs: p[1]=1, *(u16*)(p+2)=0, *p=100 — inline init of the same 4-byte percentage ctx.
Called by 0x000DC448. Cluster of 1. [O]

#### 0x0010CCD0 `app_pct_apply_final` — logic
Final-applier of the 4-byte percentage ctx: when armed (p[1]), optionally recomputes the
percentage from a table (FUN_0010cd38, special key 10000 → second table at +0x7c) when the
service flag (FUN_0010cd26) is set, converts via the routine at 0x0010CD60 and stores *p;
always disarms. Callers resolved: none (pointer-table use). Cluster of 1. [I]

#### 0x0010CD0E `app_pct_callback_run` — logic
9 instrs: when p[1] is armed, invokes (*DAT_0010cd64)(p+2) — runs the registered callback on
the u16 value. Called by 0x000DBEEC (r0c_app shutdown path). Cluster of 1. [I]

#### 0x0010CD26 `app_pct_service_flag` — logic
9 instrs: returns PTR_FUN_0010cd6c(PTR_DAT_0010cd68) != 0 — service-condition gate for the
percentage ramp. Cluster of 1. [O]

#### 0x0010CD38 `tbl_pct_lookup` — logic
Two-table lookup: param_2==10000 selects the table at ctx+0x7c (and swaps in param_3) else
ctx+0x78; result via PTR_FUN_0010cd70. Cluster of 1. [O]

#### 0x0010CD80 `app_obj8_vtbl_ctor` — logic
Get-or-alloc an 8-byte object (PTR_FUN_0010cda8, allocator family) and installs the function
pointer PTR_LAB_0010cdac at *p with byte p+4 = 0 — vtable-style ctor. Callers resolved: none.
Cluster of 1. [I]

#### 0x0010CDB0 `dtc_fault_sum_if_mode3` — logic
When the mode global *PTR_DAT_0010cfcc == 3, returns the sum of two pointer-table reads
(PTR_FUN_0010cfd0/0010cfd4); otherwise 0x7fff (invalid marker). Called by 0x000EEA2E. [I]

#### 0x0010CDDC `dtc_monitor_status_get` — data-accessor
11 instrs: returns the current-status byte p+0x20 if nonzero, else the pending byte p+0x21.
Callers 0x000EEA5A/0x000EEAEE/0x00106918/0x000EEB0C. Cluster of 1. [O]

#### 0x0010CDFA `dtc_monitor_precond_check` — logic
Precondition gate: raises DET error (0x34a,0xf,0,3,0x19,0x49) when *p is set without a status;
when active and (p[0x14]==0 or u16 p+0xe < p+0x12) verifies the status through
(*DAT_0010cfe4)(PTR_DAT_0010cfe0, p[0x20]) and param_2. Cluster of 1. [I]

#### 0x0010CE72 `dtc_monitor_ready_check` — logic
11 instrs: returns 1 when p+9 == 0, else FUN_0010cdfa(p, 1). Called by 0x000EEAE0. [O]

#### 0x0010CE88 `dtc_monitor_service` — logic
Periodic service: clears u16 p+0x3e when the state global != 5; when either status byte is set
runs PTR_FUN_0010cff0/0010cff4, FUN_0010d070 (the sub-state pump) and publishes u16 p+0xc from
PTR_FUN_0010cff8. Callers resolved: none (vector use). Cluster of 1. [I]

#### 0x0010CECA `dtc_monitor_status_collect` — logic
Sets p+0x10=1 by default; with no status → zeroes u16 p+10; else copies p+0xc (or the special
p+0x1c==2 read) into p+10, validates by p+0x11 (1/0xf/0x11) and p+0x21 (2 invalidates); returns
the p+0x21 byte. Called by 0x00106834. Cluster of 1. [I]

#### 0x0010CF4E `dtc_monitor_field_publish` — logic
Registers four ctx fields (p+0x32, p+0x34, p+0xe, p+0x3c) with the service at PTR_DAT_0010d000
via PTR_FUN_0010d004/0010d008. Called by 0x00106848. Cluster of 1. [I]

#### 0x0010CF80 `dtc_monitor_completion_check` — logic
Returns 1 when p+0x20 is clear; otherwise requires the PTR_FUN_0010d00c read (status table at
PTR_DAT_0010cfe0) >= param_2 and the enable byte p+0x16. Called by 0x0010D9B6. [I]

#### 0x0010CFB6 `dtc_monitor_code_by_status` — data-accessor
8 instrs: if param_2 == p[0x20] return the s16 code at p+0x3a else 0 — status-keyed code
lookup. Called by 0x000EEB22. Cluster of 1. [O]

#### 0x0010D010 `dtc_monitor_ctor` — logic
Allocates the 0x6c-byte monitor object (PTR_FUN_0010d28c), installs two vtable pointers at
+0x50, stores the test type param_2 at +0x1c, and resolves three channel pointer pairs at
+0x58/+0x5c/+0x60 from parallel tables indexed by (param_2-2)*4; presets +0x15=6, +0x11=0x1e,
+5=+6=1. Called by 0x0015CEC0. Cluster of 1. [I]

#### 0x0010D070 `dtc_monitor_pump` — logic
Nested-loop pump over the test sequence: while FUN_0010d9b6 reports work, increments the phase
counter p+0x22 and steps FUN_0010d5c4 until the step differs from p+0x11, runs FUN_0010d5e4 and
converts the u16 accumulator p+0x32 into p+0x2e. Callee of 0x0010CE88. Cluster of 1. [O]

#### 0x0010D0B4 `dtc_monitor_reset` — logic
38 instrs: full clear of the monitor ctx (status bytes p+0x20/p+0x21, counters, u16 fields
+0x3e/+0xc/+10/+0x24/+0x3a), resets the global, re-arms the service (PTR_FUN_0010d2b0 with
1,1). Callers 0x001067D6/0x001067EC. Cluster of 1. [O]

#### 0x0010D10C `dtc_monitor_type3_ready` — logic
17 instrs: 1 only when test type p[0x1c]==3, armed p[9]!=0 and FUN_0010cdfa(p,1) passes.
Callers resolved: none (vector use). Cluster of 1. [O]

#### 0x0010D130 `dtc_monitor_start` — logic
274-intr start/run: resolves the three channel pairs from PTR_LAB_0010d2b4/d2c8/d2d0 (indexed
(param_4+1)*4, range-checked 0..8 with DET-style report, second 8-byte table by variant byte
PTR_DAT_0010d2b8); param_2 selects p+0x21 vs p+0x20 status path (type-2 special), sets the
u16 timer p+0x38 from PTR_DAT_0010d2e0, evaluates enable/limit conditions (module+0x10 read vs
PTR_DAT_0010d52c), the type-4 completion extra (PTR_FUN_0010d534), p+4=1, and the per-status
value at p+0x64 from PTR_LAB_0010d538. Callers 0x00106754/0x001066D6. Cluster of 1. [I]

#### 0x0010D3F4 `dtc_monitor_resume` — logic
167-instr: the type-3 resume form of 0x0010D130 — same three-channel two-level resolution from
the global's stored selector, param_2 sets p+0x20/*p directly, clears the run fields, p+0x18=1,
per-status value at p+0x64 from PTR_LAB_0010d608, then PTR_FUN_0010d60c(param). Called by
0x001067A4. Cluster of 1. [I]

#### 0x0010D5C4 `dtc_monitor_phase_next` — logic
13 instrs: next phase = PTR_LAB_00063174_2[p[0x15] + p[0x22]*6] (6-column phase table) unless the
done-flag p[0x14] is set, then 0xe (14 = terminal). Callee of 0x0010D070. Cluster of 1. [O]

#### 0x0010D5E4 `dtc_monitor_phase_action` — dispatch
326-intr per-phase action switch (phases 0..0x17): arms flags p+0x16/+0x17/+0x1/+0x2/+0x3/+0x7,
resets counters and u16 fields, calls the channel vtable method at (p+0x50)->+0x14 and the type
gates, computes values via FUN_0010e52c/0010e5ee/0010e994, and finally converts the accumulated
results into u16 p+0x30/p+0x24. DET error (0x34a,0xf,0,1,0x15,0x2c) on unknown phase. [O]

#### 0x0010D97E `dtc_monitor_phase_action_err` — logic
Cold continuation (unaff_r13, callers resolved none) of 0x0010D5E4: raises the same DET error
(0x2c) then performs the two u16 conversions into p+0x30/p+0x24. Cluster of 1. [O]

#### 0x0010D9B6 `dtc_monitor_phase_done_check` — dispatch
451-intr completion oracle per phase 0..0x17: mixes status-table reads (PTR_FUN_0010dcc4 on
PTR_DAT_0010dcc0), the 0x0010CF80 completion check, counter comparisons (u16 p+0x32 vs p+0x30),
gate flags (p+0x1d, p+0x56, p+0x1f), the special mode-4/6/7 branches and the final default
p+0x30 <= p+0x32. Callee of 0x0010D070. Cluster of 1. [O]

#### 0x0010DE46 `dtc_monitor_phase_result` — dispatch
577-intr result computation per phase: builds the result from channel vtable calls
((p+0x50)->+0x20) plus the term helpers (FUN_0010e486/0010e958/0010edfc/0010ee4c/0010ef04/
0010ee84/0010f00c/0010f030), clamps negative results to 0 and converts via PTR_FUN_0010e68c.
DET error (0x34a,...,0xfb) on unknown phase. Callee of 0x0010CE88. Cluster of 1. [O]

#### 0x0010E486 `dtc_term_sum_calc` — logic
Sums the term stack: FUN_0010eb10 + channel(+0x10) interp + the alternate branch
(FUN_0010eb9e + module(+0x2c) interp when the mode flag is 1, else FUN_0010ed6c) + module
(+0x28 or channel+0x84) interp + optional FUN_0010ef76 extra; converts the total. [O]

#### 0x0010E52C `dtc_base_term_type3` — logic
Type-3 base term: FUN_0010ed4a (type 4) with the channel(+0x54) hysteresis combine, or
FUN_0010ed1a plus gated adds (module interp when mode-2+flag; channel(+0x4c) offset when
monitor flags set). Callee of 0x0010D5E4 phase 1/0x11. Cluster of 1. [O]

#### 0x0010E5EE `dtc_base_term_type34` — logic
Type-3/4 stack: FUN_0010e9e0 or FUN_0010ea1c base, gated adds from mode tables (PTR_DAT_0010e6c8
+0x14/+0x18, +0x1c/+0x24), plus FUN_0010ea4c(p, p[5]) and the final conversion. Callee of
0x0010D5E4 phase 4. Cluster of 1. [O]

#### 0x0010E71A `dtc_limit_select` — logic
Selects the limit by test type: 3 → FUN_0010ec78, 4 → FUN_0010ecc6, otherwise FUN_0010ec40 plus
the FUN_0010efe2 extra when the flag is set. Callee of 0x0010CDB0/0x0010D5E4. Cluster of 1. [O]

#### 0x0010E75E `dtc_latch_check` — logic
15 instrs: latches the done-flag p+0x14 = 1 once armed (p[1]) and u16 p+0x1a <= p+0xe.
Callee of 0x0010CE88. Cluster of 1. [O]

#### 0x0010E780 `dtc_threshold_load` — logic
Loads the u16 threshold p+0x1a from the channel-3 table (or the type-4 pair via PTR_FUN_0010e904
/ FUN_0010f06a), then computes p+0x12 = convert(channel-3 value + FUN_0010eb02). Callee of
0x0010D130/0x0010CE88/0x0010D3F4. Cluster of 1. [O]

#### 0x0010E7D8 `dtc_limit_load` — logic
Loads the s16 limit into p+0x36 while unarmed: mode 1 → channel(+0x34/+0x38) interps; mode 6 →
FUN_0010f45a; default → channel(+0x94) interp. Callee of 0x0010CDB0/0x0010D5E4. [O]

#### 0x0010E850 `dtc_term_status2_read` — logic
15 instrs: only for pending-status 2, reads the PTR_DAT_0010e928 table entry and converts.
Callee of 0x0010D9B6. Cluster of 1. [O]

#### 0x0010E872 `dtc_range_check` — logic
16 instrs: 1 when (channel-1 module+0x98 signed byte - 0x80) <= short threshold and the armed
flag p+4 is clear. Callee of 0x0010D9B6 phases 4/9/10. Cluster of 1. [O]

#### 0x0010E896 `dtc_offset_apply` — logic
Adds a mode-dependent offset (PTR_FUN_0010e940/0010e94c) to param_2; negative offsets take the
saturation path through PTR_FUN_0010e8f8 and a table-specific limiter. Callee of 0x0010CECA. [O]

#### 0x0010E958 `dtc_filter4_calc` — logic
Four-input service computation: converts p+0x32 and p+0x30, filters param_2 through
PTR_FUN_0010ebec, then combines via PTR_FUN_0010ebf0(u2, u3, p+0x54, s1). Callee of 0x0010DE46
phase 9. Cluster of 1. [I]

#### 0x0010E994 `dtc_term_78_read` — logic
Interp on channel-1(+0x78); mode 6 swaps in FUN_0010f3da, mode 1 the module(+0x24) table.
Callee of 0x0010DE46. Cluster of 1. [O]

#### 0x0010E9E0 `dtc_term_7c_read` — logic
Interp on channel-1(+0x7c); alternate table +0xac when the flag is set. Callee of 0x0010E5EE.
Cluster of 1. [O]

#### 0x0010EA1C `dtc_term_80_read` — logic
Interp on channel-1(+0x80) with a third argument; mode 6 swaps in FUN_0010f3fa. Callee of
0x0010E5EE. Cluster of 1. [O]

#### 0x0010EA4C `dtc_term_20_gated` — logic
17 instrs: channel-2(+0x20) interp returned only when enabled and the type-3 flag holds.
Cluster of 1. [O]

#### 0x0010EA72 `dtc_term_14_read` — logic
Interp on channel-1(+0x14 + byte*4); mode 6 → FUN_0010f2e8, mode 1 → FUN_0010eb9e. Callee of
0x0010DE46 phase 4. Cluster of 1. [O]

#### 0x0010EABE `dtc_term_a4_read` — logic
Interp on channel-1(+0xa4) with the converted table short; mode 6 → FUN_0010f308. Callee of
0x0010D5E4/0x0010DE46. Cluster of 1. [O]

#### 0x0010EB02 `dtc_channel3_flag_read` — data-accessor
5 instrs: returns **(byte**)(channel-3 + 4). Callee of 0x0010D5E4/0x0010E780. [O]

#### 0x0010EB10 `dtc_term_8_read` — logic
Interp on channel-1(+8); mode 6 → FUN_0010f2ae; the extra flag adds FUN_0010f22c/0010f24c.
Callee of 0x0010E486/0x0010DE46. Cluster of 1. [O]

#### 0x0010EB76 `dtc_counter_gate` — logic
17 instrs: when the u16 at PTR_DAT_0010ec10+0xc is below the converted table value, clears the
p+0x3c counter flag. Callee of 0x0010CE88. Cluster of 1. [O]

#### 0x0010EB9E `dtc_term_pct_calc` — logic
Mode-1 alternate of the term stack: FUN_0010f1c0 interp on module(+0x18/+0x1c) with a converted
base, times the byte read at module(+0x3c)/10, all converted. Callee of 0x0010E486/0x0010EA72.
Cluster of 1. [O]

#### 0x0010EC40 `dtc_limit_default` — logic
Default limit: interp on channel-1(+0x64 + byte*4); mode 6 swaps in FUN_0010f41a. Callee of
0x0010E71A. Cluster of 1. [O]

#### 0x0010EC78 `dtc_limit_type3` — logic
Type-3 limit: channel-1(+0x6c) interp (+0xb4 alternate when flag set); when p[0x18] is armed,
the linked-channel value FUN_0010f1a2(p+0x64). Callee of 0x0010E71A. Cluster of 1. [O]

#### 0x0010ECC6 `dtc_limit_type4` — logic
Type-4 limit: channel-1(+0x70) interp; mode 6 → FUN_0010f43a, mode 1 → module(+0x10) interp.
Callee of 0x0010E71A. Cluster of 1. [O]

#### 0x0010ED1A `dtc_base_50_read` — logic
Interp on channel-1(+0x50); mode 6 → FUN_0010f47e (linked channel at +0xf4). Callee of
0x0010E52C. Cluster of 1. [O]

#### 0x0010ED4A `dtc_gated_58_read` — logic
15 instrs: returns **(byte**)(channel-1+0x58) only when the global gate pair (DAT_fff8d15e==0,
DAT_fff8d168!=0) holds, else 0. Cluster of 1. [O]

#### 0x0010ED6C `dtc_term_8c_read` — logic
Interp on channel-1(+0x8c + byte*4); mode 6 → FUN_0010f28e (linked +0xbc). Callee of
0x0010E486. Cluster of 1. [O]

#### 0x0010EDB2 `dtc_term_c_read` — logic
Interp on channel-1(+0xc); mode 6 → FUN_0010f342 (linked +0xcc), mode 1 → module(+0x14) path.
Callee of 0x0010DE46 phase 4. Cluster of 1. [O]

#### 0x0010EDFC `dtc_term_1c_read` — logic
Interp on channel-1(+0x1c + byte*4); mode 6 → FUN_0010f360 (linked +0xd0), mode 1 →
module(+0x20). Callee of 0x0010DE46 phase 10. Cluster of 1. [O]

#### 0x0010EE4C `dtc_term_24_read` — logic
Interp on channel-1(+0x24 + byte*4); mode 6 → FUN_0010f37e (linked +0xd4). Cluster of 2 with
0x0010EE84, which is the same pattern reading +0x30 (linked +0xd8) — byte-identical helper
duplicated for a second table offset. Callee of 0x0010DE46 phase 0xb. [O]

#### 0x0010EF04 `dtc_term_3c_read` — logic
Interp on channel-1(+0x3c + byte*4) with a third table byte; mode 6 → FUN_0010f3ba (linked
+0xdc). Callee of 0x0010DE46 phase 0x10. Cluster of 1. [O]

#### 0x0010EF48 `dtc_term_4_read` — logic
Interp on channel-1(+4); mode 6 → FUN_0010f49e (linked +0xf8). Callee of 0x0010DE46 phase 0xc/0xe.
Cluster of 1. [O]

#### 0x0010EF76 `dtc_term_44_read` — logic
12 instrs: interp on the module(+0x44) table — the optional extra term of 0x0010E486. [O]

#### 0x0010EF92 `dtc_term_9c_read` — logic
15 instrs: interp on channel-1(+0x9c + byte*4) with the caller's param_2. Callee of 0x0010D5E4.
Cluster of 1. [O]

#### 0x0010EFB4 `dtc_term_2c_read` — logic
10 instrs: interp on channel-1(+0x2c) with param_2. Cluster of 3: 0x0010EFB4 (+0x2c), 0x0010EFCA
(+0x88) and a duplicated copy 0x0011736E in the 0x112xxx monitor module (channel at p+0x138,
table +0x1c) — same helper instantiated per table offset/owner. Callee of 0x0010DE46. [O]

#### 0x0010EFE2 `dtc_limit_link_read` — logic
12 instrs: interp on the linked module (+4) table — extra limit term when the mode flag is set.
Callee of 0x0010E71A. Cluster of 1. [O]

#### 0x0010EFFC `dtc_flag_40_read` — data-accessor
6 instrs: returns **(byte**)(module + 0x40). Callee of 0x0010D5E4. Cluster of 1. [O]

#### 0x0010F00C `dtc_term_pair_14` — logic
Interp of (p+0x32) then combines with the s16 at p+0x38 over the channel-2(+0x14) table via
PTR_FUN_0010f1e8. Callee of 0x0010DE46 phase 0x13. Cluster of 1. [O]

#### 0x0010F030 `dtc_term_pair_1c` — logic
Interp of (p+0x32) then combines with a table byte over channel-2(+0x1c + byte*4). Callee of
0x0010DE46 phase 0x13. Cluster of 1. [O]

#### 0x0010F06A `dtc_term_8_alt` — logic
11 instrs: interp on channel-3(+8) — the type-4 alternate of 0x0010E780. Cluster of 1. [O]

#### 0x0010F084 `dtc_code_load` — logic
Loads the fault code into p+0x3a: selector from PTR_FUN_0010f218 (1/2/-1 → module +0x58/+0x54/
+0x50 tables), result converted through PTR_FUN_0010f1fc. The code field read by 0x0010CFB6.
Callee of 0x0010D130. Cluster of 1. [O]

#### 0x0010F0E4 `dtc_result_blend` — logic
58 instrs: phase-0x15 result: two interps on channel-2(+0x24/+0x18) blended with a percentage
term (uVar4*term/100) plus the +0x34 interp, capped at the stored p+0x26. Callee of
0x0010D5E4/0x0010DE46. Cluster of 1. [O]

#### 0x0010F16C `dtc_link_34_read` — logic
14 instrs: when the link pointer at p+0x34 is not the self sentinel, interp it against
PTR_DAT_0010f220. Cluster of 1. [O]

#### 0x0010F188 `dtc_link_38_read` — logic
13 instrs: linked read through p+0x38 with the caller's param_2. Callee of 0x0010DE46. [O]

#### 0x0010F1A2 `dtc_link_3c_read` — logic
14 instrs: linked read through p+0x3c with the u16 table PTR_DAT_0010f224. Callee of
0x0010EC78. Cluster of 1. [O]

#### 0x0010F1C0 `dtc_link_1c18_interp` — logic
15 instrs: interp over the link's +0x1c or +0x18 table by a boolean arg. Callee of 0x0010EB9E.
Cluster of 1. [O]

#### 0x0010F22C `dtc_link_dyn_read` — logic
15 instrs: dereferences *param_1 (dynamic link) and interps via PTR_FUN_0010f4c4 with
PTR_DAT_0010f4c0. Callee of 0x0010EB10. Cluster of 1. [O]

#### 0x0010F24C `dtc_link_plus4_read` — logic
15 instrs: linked read through param_1+4 with PTR_DAT_0010f4c0. Callee of 0x0010EB10. [O]

#### 0x0010F26C `dtc_short_select_read` — logic
16 instrs: **(short**) (link + *DAT_0010f4c8*4 + 0x3c or +0x44) by boolean — indirect short
pick. Callee of 0x0010D9B6. Cluster of 1. [O]

#### 0x0010F28E `dtc_link_bc_read` — logic
15 instrs: linked read through +0xbc (byte arg) with PTR_DAT_0010f4cc. Callee of 0x0010ED6C. [O]

#### 0x0010F2AE `dtc_link_c0_read` — logic
26 instrs: linked read through +0xc0 with the converted PTR_DAT_0010f4d0+0xc short. Callee of
0x0010EB10. Cluster of 1. [O]

#### 0x0010F2E8 `dtc_link_c4_read` — logic
15 instrs: linked read through +0xc4 with PTR_DAT_0010f4cc. Callee of 0x0010EA72. [O]

#### 0x0010F308 `dtc_link_c8_read` — logic
26 instrs: linked read through +0xc8 with the converted table short. Callee of 0x0010EABE. [O]

#### 0x0010F342 `dtc_link_cc_read` — logic
14 instrs: linked read through +0xcc via PTR_FUN_0010f4d8 with PTR_DAT_0010f4cc. Cluster of 4:
+0xcc (0x0010F342), +0xd0 (0x0010F360), +0xd4 (0x0010F37E), +0xd8 (0x0010F39C) — the same
optional-linked-channel helper instantiated for four neighbouring link slots. [O]

#### 0x0010F3BA `dtc_link_dc_read` — logic
15 instrs: linked read through +0xdc with a byte third arg. Callee of 0x0010EF04. [O]

#### 0x0010F3DA `dtc_link_e0_read` — logic
14 instrs: linked read through +0xe0 with PTR_DAT_0010f4dc. Cluster of 2 with 0x0010F3FA
(+0xe4). Callees of 0x0010E994/0x0010EA1C. [O]

#### 0x0010F41A `dtc_link_e8_read` — logic
14 instrs: linked read through +0xe8 via PTR_FUN_0010f4e0. Cluster of 3: +0xe8 (0x0010F41A),
+0xec (0x0010F43A), +0xf8 (0x0010F49E). Callees of 0x0010EC40/0x0010ECC6/0x0010EF48. [O]

#### 0x0010F45A `dtc_link_f0_read` — logic
15 instrs: linked read through +0xf0 with the table pair PTR_DAT_0010f4d0[0x18]/[0xf]. Callee
of 0x0010E7D8. Cluster of 1. [O]

#### 0x0010F47E `dtc_link_f4_read` — logic
14 instrs: when the link at +0xf4 is not the self sentinel, reads it via PTR_FUN_0010f4e8 with
PTR_DAT_0010f4e4 — the mode-6 alternate of 0x0010ED1A. Cluster of 1. [O]

#### 0x0010F4EC `tbl_2d_4way_read` — data-accessor
Four-way 2D read: selector 1/2/3/other picks PTR_LAB_0010f76c/70/74 or DAT_0010f778; entry =
base + (param_3&0xff)*4 + (param_2&0xff)*0x24 — same layout family as 0x00108894. Used by the
second monitor framework. Cluster of 1. [O]

#### 0x0010F526 `tbl_2d_3way_read` — data-accessor
Three-way variant: PTR_LAB_0010f77c/80 or DAT_0010f784, same 0x24/4 strides. Cluster of 1. [O]

#### 0x0010F556 `mon2_ctor` — logic
Allocates a 0x7c-byte monitor object (PTR_FUN_0010f788, allocator family), zeroes the counter
fields (+2, +9, +0xe, +0x14) and installs the vtable PTR_LAB_0010f78c at +0x68. Called by
0x00117240. Cluster of 1. [O]

#### 0x0010F584 `mon2_channels_resolve` — logic
Resolves the six channel/link pointers at ctx+0x3c..+0x50 from six parallel pointer tables by
index (param_2*4) — the channel hookup of the second monitor framework. Called by 0x00117292.
Cluster of 1. [O]

#### 0x0010F5B6 `mon2_variant_tables_load` — logic
459-intr: for mode param_2 1/2 and 3/4 resolves five channel pairs (p+0x3c/+0x40/+0x44/+0x48/
+0x4c/+0x50) through the 0x24-stride 2D variant tables (PTR_LAB_0010f7b4/0010f7c8/0010f7d4/
0010fa14 + FUN_0010f4ec/0010f526), each range-checked (6/0x11/0x1b/0x2a) with the DET-style
report and the variant-byte second table — the framework twin of 0x0010D130's resolution.
Finally loads p+0x54 from PTR_LAB_0010fa24 by the p+8 byte. Called by 0x001176E2. [O]

#### 0x0010FA28 `dtc_gate2_check` — logic
16 instrs: returns 1 only when both the service flag (PTR_FUN_0010fc30) and *PTR_DAT_0010fc34
are set. Called by 0x001106DC. Cluster of 1. [O]

#### 0x0010FA48 `dtc_threshold_lt_check` — logic
8 instrs: **(short**)(channel+0x14) < *(short*)(p+0xe) — threshold comparison. Called by
0x000EDBEA. Cluster of 1. [O]

#### 0x0010FA5A `dtc_clamp_then_convert` — logic
13 instrs: clamps param_2 at 0 via FUN_0010fa74 then converts through PTR_FUN_0010fc38 with
param_3. Cluster of 2 with 0x00112A30 — an identical copy wired into the 0x112xxx module's
pointer table. Callees of 0x00110CDC/0x001115E0. [O]

#### 0x0010FA74 `lib_clamp_zero` — library
5 instrs: max(0, param). Cluster of 2 with 0x00112A4A — same helper duplicated for the second
monitor framework. Callers 0x0010E896/0x0010FA5A/0x00110C8E. [O]

#### 0x0010FA7E `mon2_service_step` — logic
23 instrs: when the channel(+0x3c) flag is set, publishes u16 p+0xe (from p+2 or the special
read PTR_FUN_0010fc3c when channel+3 is set), runs FUN_0010faae and stores its result at p+6.
Called by 0x000EB348. Cluster of 1. [I]

## Unit summary

Unit 36 covers 220 clusters / 231 functions in `r10_app` (0x00108294–0x0010FA7E, plus duplicated
helper copies at 0x00112A30/0x00112A4A/0x0011736E in the 0x112xxx/0x117xxx monitor framework).

Counts by kind: logic 202, data-accessor 10, stub 11, dispatch 6, library 2, irq 0, entry 0.
`unknown` entries: none — every function's structural role was observable; however most
*module attributions* are inferences ([I]) built on the anchors below, not symbol facts.

Module map established by this unit:
- **0x001082xx–0x00108Fxx**: torque-path/handover math (ratio+reciprocal, mode engines,
  hold/ramp limiters, 2D 0x24-stride table reads 0x00108894) feeding the shift coordinators.
- **0x0010A3xx–0x0010C8xx**: per-phase actions of the 12-state shift-sequence coordinator
  0x000DEA5E (r0c_app) — torque/speed phase steps, fill ramps, gear 1-8 setpoint engine
  (0x0010B79E), teardowns.
- **0x0010A9xx–0x0010B1xx + 0x0010BExx–0x0010C6xx**: solenoid duty/position controllers
  (PWM-count domain, max 0x1fc0=8128) driven from 0x000E2682/0x000E27D2, incl. the 4-state
  handover machine 0x0010AF0A. Possible spec-12 (solenoid channels) cross-ref.
- **0x0010CCD0–0x0010CDDC**: percentage-ramp ctx objects used by the r0c shutdown path
  (0x000DBEEC/0x000DC4C4).
- **0x0010CE88–0x0010FA7E**: two generations of a DTC/monitor framework ("dtc_monitor_*",
  "mon2_*"): ctor + variant-table channel resolution (two-level 0x26/0x49/8-entry lookups),
  24-phase (0..0x17) test state machines (action/done/result split across three big
  dispatchers 0x0010D5E4/0x0010D9B6/0x0010DE46), term readers at fixed channel offsets, and a
  family of "optional linked channel" readers (self-pointer sentinel pattern) at +0x34..+0xf8.
  The fault-code field p+0x3a keyed by status byte (0x0010CFB6/0x0010F084) anchors the DTC
  interpretation. Spec-10 (diagnostics) cross-ref suggested.

Weakest attributions (structural facts solid, semantics inferred): 0x00109770/0x00109816
(parallel-combination p0*p1/(p0+p1) formula), 0x0010C640/0x0010C764 (state sampler/float
aggregation), and the exact physics of the 0x00108D7C accumulation chain.

Cross-region leads for other units:
- r08_app owns the shared service layer this unit calls constantly via PTR_FUN: interpolators
  (0x000B7164/0x000B6E10/0x000B6D6C/0x000B6D40), converters (0x000B7888/0x000B79E4), filters
  (0x000B7472/0x000B7B40), allocator 0x0009AA34, and the DET-style error reporter invoked as
  (ptr, line, 0xf, 0, 1, N, code) with file ids 0x188/0x1a3/0x34a/0x441/0x465/0x471/0x47c.
- r0c_app drivers to cross-check: 0x000DEA5E (shift-sequence coordinator), 0x000E2682/
  0x000E27D2 (solenoid scheduler), 0x000DBEEC/0x000DC4C4 (shutdown), 0x000DE614/0x000DE664/
  0x000DE688/0x000DF7FC/0x000E0FE4/0x000E2682 (per-phase glue), 0x000E447A/0x000E4A4E (gear
  blend callers), 0x000EEA2E–0x000EEB94 (monitor status consumers).
- The 0x112xxx/0x117xxx monitor framework duplicates three of this unit's helpers
  (0x00112A30/0x00112A4A/0x0011736E) — that unit should reuse these names.
- 0x0015CEC0 constructs the type-2/3/4 monitor objects (ctor 0x0010D010); 0x00106834/
  0x00106848/0x00106918/0x00106754/0x001066D6/0x001067A4/0x001067D6/0x001067EC are the
  monitor task layer just below this unit.
