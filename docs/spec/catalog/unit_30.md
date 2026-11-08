# Unit 30 (r0c_app: 0x000E513C-0x000EB640)

Module context: pressure-request subsystem (0xE513C-0xE5A20, spec 12 §2a), park module (0xE7000-0xE9200, spec 12 §1.2), line-pressure strategy (0xE9CE0-0xEB640, spec 11 §3a). Shared helpers from r08_app: 0xB7164 2D-map interpolation, 0xB6D6C 1D curve interp, 0xB6CBC 2D byte-table get, 0xB79E4 event/error report, 0xB6EAE 1D curve interp.
#### 0x000E513C `preq_get_element_req_obj` — data-accessor
Returns the pressure-request object pointer for element index param_1 (0-5) from table `PTR_LAB_000e5334`; index 0 or >5 triggers error report via 0xE510A (file 0x19f, code 0xb2) and falls back to entry 2. Spec 12 §2a identifies this as the getter for the five element request objects at 0xFFF8B7C8/B828/B888/B8E8/B948. Cluster of 1. [O]
#### 0x000E5164 `set_u16_2e_clamp_3a` — data-accessor
Stores param_2 as u16 at object+0x2e, clamped to the u16 maximum held at object+0x3a. No other effects; callers 0xE3CA0 and 0xE44B6 pass a shared object. Cluster of 1. [O]
#### 0x000E517A `map_pct_scaled_quantize` — logic
Interpolates a 2D map reached at o->(+0x10)+0x70 with (param_2, param_3) via 0xB7164, scales by the u16 factor at o->(+0x10)+0x6c /100, clamps to 100, and quantizes the percent to a byte via PTR_FUN/DAT 0x000e5344. Called by converter-module task 0xE39DC. Cluster of 1. [I] purpose: percent demand from calibration map.
#### 0x000E51C8 `tbl2d_byte_gear_state_check` — logic
If param_2 < 6 and object+0x4d < 5, returns the byte from 2D table (PTR_DAT 0x000e5348 +8 data) indexed [param_2][o+0x4d] via a 0xB6CBC-style lookup; otherwise reports fault (file 0x19f, code 0xae) and returns false. Looks like a gear x mode permission check. Cluster of 1. [I]
#### 0x000E5236 `curve_val_by_idx_to_float` — logic
Looks up a curve (PTR_DAT 0x000e5350) selected by object byte +0x32 through 0xAEAE2/interpolation and returns the s16 result as float. Callers 0xE39DC, 0xE3E28, 0xE476E. Cluster of 1. [O]
#### 0x000E524C `map2d_interp_bank_a` — logic
Selects a 2D map pointer from object slots +0x24/+0x28/+0x2c (modes 2 and 3 share)/+0x30 by mode param_2 (0-4), then interpolates via 0xB7164 with x = s16 at table 0x000e5358+8 and y = s16 at +0x16. Member 0x000E52AA is the identical pattern over the second bank of slots (+0x38..+0x44). Both called from 0xE469C. Cluster of 2. [O]
#### 0x000E52AA `map2d_interp_bank_b` — logic
Second copy of 0x000E524C: same mode select and 0xB7164 interpolation but over map slots +0x38/+0x3c/+0x40 (2,3 shared)/+0x44 of the same object. Cluster member of 0x000E524C (constant difference = which map bank). [O]
#### 0x000E530C `map_val_plus_offset_quantize` — logic
By type param_2 (1/2/other) picks map pointer at o+0x80/+0x84/+0x88, evaluates it via PTR_FUN 0x000e53b0 at x from 0x000e53ac; for flag param_3==0 returns the quantized value, otherwise adds the byte at **(char**)(o+0x8c) before quantizing via 0x000e53b4. Caller 0xE4CF4. Cluster of 1. [I] map value plus per-object offset.
#### 0x000E53B8 `preq_small_obj_ctor` — logic
Allocates a 0x10-byte object (allocator reached via PTR_FUN 0x000e5648 / direct callee 0x9AA34), sets its vtable slot +0xc to PTR_LAB_000e564c and byte +7 = 1. Called from 0x15C0BC (construction area of spec 12 §2a) and 0xE3710. Cluster of 1. [I] constructor for a request-related small object.
#### 0x000E53DA `preq_set_pressure_request` — logic
The documented request entry: stores pressure (param_2), mode (param_3) at +7, source id (param_5) at +8, priority (param_4) at +9 into request object param_1, flags +6 = 1, then calls the arbitration vmethod at o->(+0xc vtable)+0x14 and logs the source value via 0xE56F0 into 0xFFF8E374. Reports internal faults for priority >= 5 (code 0xe5), mode 2 without flag (0xe9), source id > 0x29 (0xe1). Spec 12 §2a. Cluster of 1. [O]
#### 0x000E552C `chk_flag_5660_set_5664_clear` — data-accessor
Returns 1 iff global byte at PTR_DAT 0x000e5660 equals 1 and the byte at PTR_DAT 0x000e5664 is not 1, else 0. Caller 0xE3836. Cluster of 1. [O]
#### 0x000E5548 `preq_cycle_reset` — logic
For request-table ids 1..10 except 6,7,8 (looked up via 0xE55DC) clears byte +6 (request-pending marker); then calls 0xE5706 to latch and clear the source-id log block at 0xFFF8E374. Called each cycle by 0xDC4C4, the Sys/WK pressure source. Spec 12 §2a. Cluster of 1. [I] begin-of-cycle arbitration reset.
#### 0x000E5578 `preq_err_bad_index_c4` — stub
Single error report (module 0x19a, line 0xf, class 7/2, code 0xc4) — the invalid-index path of 0xE55DC. Cluster of 1. [O]
#### 0x000E55AA `preq_err_bad_index_d9` — stub
Single error report (module 0x19a, line 0xf, class 8/2, code 0xd9) — invalid-index path of the 0xE56xx element getters. Cluster of 1. [O]
#### 0x000E55DC `preq_get_by_index` — data-accessor
Returns request-object pointer by id 0..10 from table PTR_PTR 0x000e5668; id 0, >10, or null entry reports via 0xE5578 and returns entry 0. Widely used (19 callers incl. 0xDC4C4 and 0xE56xx phases). Spec 12 §2a request object table. Cluster of 1. [O]
#### 0x000E5602 `preq_elems_call_vt_0x80` — logic
For element ids 1..5 (via 0xE55DC) calls virtual method at o->vtable(+0xc)+0x80 with context o->(+0xc)+0x78. Reached through r08 thunk 0xB859E, i.e. a periodic phase over the five element request objects. Member 0x000E5624 is the same loop with vmethod +0x8c / context +0x84. Cluster of 2. [I]
#### 0x000E5624 `preq_elems_call_vt_0x8c` — logic
Member of 0x000E5602: loop over element ids 1..5 calling virtual method +0x8c (context +0x84); reached via r08 thunk 0xB85AA. Constant difference selects a different vmethod, so named separately. [I]
#### 0x000E566C `preq_elems_call_vt_0x74` — logic
Loop over element ids 1..5 calling virtual method +0x74 (context +0x6c), then copies global PTR_DAT 0x000e57f4 into 0x000e57f8. Reached via r08 thunk 0xB85A4. Cluster of 1. [I] per-cycle element phase plus global latch.
#### 0x000E56F0 `preq_log_source_value` — data-accessor
Stores param_3 (u16) into the source log at 0xFFF8E374-based block at +0x16 + 2*source-id, ids < 0x2a. Spec 12 §2a "logs the source id (0xFFF8E374)". Cluster of 1. [O]
#### 0x000E5706 `preq_latch_and_clear_source_log` — logic
Refreshes the log block at 0xFFF8E374: sets the active flag from PTR_DAT 0x000e57fc, moves pending values (chosen between +0x18/+0x22, +0x1a/+0x20, +0x1c/+0x26, +0x1e/+0x24, +0x28/+0x2a pairs) into output slots +2..+0xe based on state bytes at PTR_DAT 0x000e5804[0x2b]/[0x2c], resolves two curve lookups into +0x12/+0x14 via 0xAEAE2, and zeroes the 0x2a source-id entries. Called from 0xE5548 at cycle start. Cluster of 1. [I]
#### 0x000E5814 `tbl2d_u32_sel_class4` — data-accessor
Selects one of four base tables (PTR_LAB 0x000e59c4/59c8/59cc/59d0) by param_1 (1/2/3/other) and returns the u32 at base + col(param_3)*4 + row(param_2)*0x24 (9 entries per row). No bounds checks. Cluster of 1. [O]
#### 0x000E584E `tbl2d_u32_sel_class5` — data-accessor
Same 2D u32 table access (stride 0x24) but over five tables selected by param_1 (1-4/other): PTR_LAB 0x000e59d4..59e4. Cluster of 1. [O]
#### 0x000E5892 `tbl2d_u32_sel_class_alt` — data-accessor
Same 2D u32 table access (stride 0x24) over four tables selected by param_1 (1/2/4/other): PTR_LAB 0x000e59e8/59ec/59f0 + DAT 0x000e59f4. Ten callers across this module. Cluster of 1. [O]
#### 0x000E58D0 `pred_enabled_and_checks` — logic
Returns 1 iff object byte +0x17 is non-zero and (check vmethod PTR_FUN 0x000e59f8(o, o[+0x28]) passes, or o+4 == 1 and check 0x000e59fc(o) fails). A predicate used by line-pressure callers 0xEB640 and 0xEB288. Cluster of 1. [I]
#### 0x000E590C `lp_pred_state5_11` — logic
Returns 1 iff object byte +0x0b is 5 or 11, else 0. Caller 0xEB5A6 (line-pressure) uses it as a state/phase gate; the +0x0b field is the phase byte of the 0xE5EAA phase machine object (q.v. below). Cluster of 1. [I] phase semantics.
#### 0x000E591E `cal_id_resolve_read_s16` — data-accessor
Maps param_2/param_3 through vmethod 0xE5A04 to (x,y) indices, resolves a calibration object id via 0xE5814 (class-4 table), range-checks 0..0x1b with error-report fallback (0xB75B4 path), then reads the s16 value from the object pointer table PTR_PTR_000E5A0C[idx*8+chan*4] via vmethod 0xE5A18. Caller 0x112DB0 (r10_app). Cluster of 1. [O]
#### 0x000E5990 `pred_phase11_mode1_ready` — logic
True iff object +0x28 == 0, +0x17 enabled, phase +0x0b == 11, mode +4 == 1, and vmethod 0xE5A1C() <= param_2. Caller 0x115F6E (r10_app). Cluster of 1. [I] readiness gate for the phase-11 state of the 0xE5EAA machine.
#### 0x000E5A20 `shift_cal_set_select` — logic
Validates variant param_2 ∈ {1,2,3} and sets machine enable +0x17; stores +4 = table 0xE5C80[param_2]; stages 5-6 calibration object ids (via 0xE584E/0xE5814/0xE5892 lookups, each with 0..N range check and error-report fallback) into slot pairs (id, resolved pointer) reached through PTR_PTR chains 0xE5C98..; param_3==4 selects an alternate table pair; ends with +0x36 = 1. Called from 0xED92E. Cluster of 1. [I] variant/calibration selection for the 0xE5EAA phase machine.
#### 0x000E5E48 `shift_mach_reset_state` — logic
When machine enable +0x17 is set, zeroes the state fields (+0x24, +0xf, phase +0xb, +0x1c, +0x1a, +0x2e, +0x28, +2, +0x16, +3, +0x14, +0x15, +0x18, conditional +0x19) and sets +0x34 from a vmethod on 0xE5F18 and byte DAT_000E5F20 < 3. Called from 0xEDC5A. Cluster of 1. [O]
#### 0x000E5EAA `shift_phase_engine_step` — logic
The phase sequencer, twin of the spec 11 §3 engine (same tick counter +0x12 / target +0x20 layout and sequence table entry step*3+type at 0xE5F34): disabled branch runs 0xE6B6E reset path; enabled branch refreshes tick via 0xE6C78, then while completion check 0xE630C reports done it advances the phase from the sequence table indexed [tick+1]*3 + type(+2) and runs phase entry 0xE6210; then computes the phase target via 0xE65F4, converts via vmethod 0xE6188 into result +0, and stores +0x22. Final block latches/clears +8. Called per cycle by task 0xDE0D8 (shift-module area, spec 09). Cluster of 1. [O] mechanics, [I] shift-element identification.
#### 0x000E5FF6 `shift_mach_stop_latch` — logic
Stops the machine: if global mode 0xE6198 != 1 (or enable +0x17 == 0) disables +0x17 and clears target +0x20; otherwise resolves a stop value by type +2 (slot +4 ctx, +0x24 ctx, or cal lookup via 0xE5892) into +0x1f, clears +0x17, moves result into +0x20, and if +0x1b set latches type into +0x35 and copies context words to +0x22/+0x24. Callers 0xEBC60/0xEBB6C (both in the 0xEB640+ controller). Cluster of 1. [O] mechanics, [I] teardown.
#### 0x000E6144 `obj_publish_fields_vt` — logic
Publishes six fields of param_1 (+0xc, +0x1c, +0x1e, +0xa, +0x1a, +0x19) into the consumer object PTR_DAT_000E61D0 through three vtable register methods (0xE61D4/0xE61D8/0xE61DC). Caller 0xDE0FE. Cluster of 1. [I]
#### 0x000E61E0 `shift_mach_ctor` — logic
Allocates a 0x58-byte object (allocator 0x9AA34), stores context pointer PTR_PTR_000E6230 at +0x54 and zeroes +0x10, +0x44, +0x48. No resolved callers — reached via a constructor/pointer table. The 0x58-byte layout with ctx at +0x54 matches the phase-machine object. Cluster of 1. [I]
#### 0x000E6210 `shift_phase_entry` — dispatch
Phase-entry actions by phase +0x0b: case 3 computes target via two vmethods into +6/+8 and sets +3; case 4 loads target from +0x22 and ramps; case 6 loads +6 from +0x22 unless held; cases 9/10 drive the ramp object at +0x38 (vmethod 0xE633C) toward +0x22; case 11 copies +0x22 to +0x2a. Clamps ramp accumulator +0x26 to >= 0. Called from the sequencer 0xE5EAA. Cluster of 1. [O]
#### 0x000E630C `shift_phase_done_check` — dispatch
Per-phase completion test (returns true = advance): phase 3 via 0xE727A or 0xE728A; phase 4 via 0xE727A, cal lookups (0xE5892) and signed speed-window comparisons; phase 5 via 0xE727A + speed at cal-object offset vs 0xE65EC; phase 6/11 wait on +0x22 and elapsed +0x1c vs context; phases 8/9 arm the sync threshold via 0xE6AE8 (+ 0xE72E6); phase 10 combines 0xE6AE8 and 0xE74E2 with a timer. Default true. Cluster of 1. [O] mechanics, [I] phase names.
#### 0x000E65F4 `shift_phase_target_calc` — dispatch
Computes the phase output value by phase +0x0b: phase 3 via 0xE73AC gate + vmethod; phases 4/5 via 0xE69F4/0xE7126 with torque-% scaling (x*y/100, x*y/0xff) and min-clamp against +0x22; phase 6 scales +6 by a cal map; phase 10/11 combine 0xE69F4, 0xE6CA0 and ramp objects with /100 and /1000 factors; phase 11 also supports the type-2 ctx path through 0xE728A. Zeroes ramp +0x1c on exit. Cluster of 1. [O] mechanics.
#### 0x000E69F4 `shift_val_by_mode_select` — logic
Selects a source value by mode +4: 0 -> 0xE7126 (cal temp), 1 -> 0xE6F5E (cal temp, 2 groups), 2 -> map read scaled (b*6-0x366) or ctx vmethod; result then passes through 0xE7782 and, unless mode 2, a /1000 scaling vmethod when negative. Callers 0xE65F4, 0xE7A56, 0xE7844. Cluster of 1. [O]
#### 0x000E6AE8 `shift_sync_threshold_arm` — logic
Arms the synchronous-point speed threshold +0x26: param_2 == 0x7fff forces flag +0x14; when armed and global gate objects (0xE6D28/0xE6D30) allow and DAT_000E6D38 == 0, computes +0x26 = vmethod(0xE6D3C(global) + 0xE6E56(param)) and clears +0x14. +0x26 is the threshold compared in 0xE630C/0xE72E6. Cluster of 1. [I] sync-point semantics.
#### 0x000E6B6E `shift_mach_inactive_result` — logic
Result path when the machine is inactive (param_2 == 0): by mode +4 computes a fallback from cal lookups (states +0xf = 6/7), or applies ramp vmethod 0xE6D44 for negative +0x20 (states 2/3, 8), scaling by the +0x20/+0x30 values; final value via ramp vmethod 0xE6D48. Called from 0xE5EAA both branches. Cluster of 1. [O] mechanics.
#### 0x000E6C78 `shift_mach_tick_update` — logic
Reads the tick value via vmethod 0xE6D68 on global 0xE6D64 and feeds it with a scale factor (**PTR_DAT_000E6D6C) into the machine's tick block +0x4c via vmethod 0xE6D70; returns the tick used by the sequencer. Caller 0xE5EAA. Cluster of 1. [O]
#### 0x000E6CA0 `cal_obj_read_s16_1b` — data-accessor
Resolves a calibration object id via the class-4 table (range 0..0x1b, error-report fallback), then reads the s16 value from the resolved object via vmethod 0xE6D90(ptr, obj+0xa, obj+0xe). Callers 0xE65F4, 0xE5990. Cluster of 1. [O]
#### 0x000E6D94 `cal_obj_read_scaled_pct` — data-accessor
Resolves a calibration object id via the class-alt table (range 0..0x3c, error-report fallback), reads a value via vmethod 0xE7050(ptr, param_3, param_4) and scales it by param_2 percent ((v*param_2)/100). Caller 0xE78EE. Cluster of 1. [O]
#### 0x000E6E20 `shift_mode_offset_get` — data-accessor
Returns a signed offset by mode +4: mode 2 -> vmethod(ctx+0x24, global) - 0x80; mode 1 -> vmethod(ctx(param+0x54)+4); otherwise 0. Caller 0xE74E2. Cluster of 1. [O]
#### 0x000E6E56 `shift_sync_offset_sum` — logic
Sums calibration-derived offsets by mode +4: mode 2 reads vmethod(ctx+0x28); mode 1 resolves two class-4 cal objects (range 0x1b) and sums their vmethod values (short + char). Feeds the sync threshold in 0xE6AE8. Cluster of 1. [O] mechanics.
#### 0x000E6F5E `cal_temp_read_2grp` — data-accessor
Reads a temperature as (byte*6 - 0x366) (i.e. 0.6 C per count, -87.0 C offset) from a calibration object: +0xd == 0 uses group 0xE7320/0xE732C (range 0..0x22); otherwise group 0xE702C/0xE703C and, under a speed condition (0xE7098), adds a second reading. Caller 0xE69F4. Cluster of 1. [O] mechanics, [I] unit.
#### 0x000E7126 `cal_temp_read_mode_sel` — data-accessor
Reads a temperature ((b-0x91)*6) from calibration group 0xE7320: layout selected by group flag [4], slot by mode (0/1/2 -> +0/+0x10/+0x18 or +0x14/+0x1c/+8); also calls 0x10FB52 (r10_app, spec 11 phase-engine region — cross-region lead). Callers 0xE65F4, 0xE69F4. Cluster of 1. [O] mechanics.
#### 0x000E727A `shift_chk_elapsed_gt` — logic
Returns true iff *(context(0xE7364)+0x10 deref) < elapsed counter +0x1c of the machine object. Simple threshold comparison used by phase completion. Cluster of 1. [O]
#### 0x000E728A `shift_pred_sync_gate2` — logic
Gate predicate over global objects 0xE7368/0xE7324: true unless both a time condition (value - 0xE737C threshold vs context speed) and a speed condition (vmethod value < +0x26) fail. Caller 0xE65F4 phase-10 type-2 path. Cluster of 1. [I]
#### 0x000E72E6 `shift_pred_sync_gate` — logic
Returns true when global 0xE7364+8 flag is clear or a context value exceeds 0xE7384; otherwise requires global 0xE75F4 set or the speed condition (vmethod value < +0x26) to pass. Caller 0xE630C phase-9. Cluster of 1. [I]
#### 0x000E73AC `shift_pred_cal_cond_met` — logic
Returns 1 (condition met) normally; when flag PTR_DAT_000E75F8[4] == 0 and machine +0x28 != 0, resolves two class-alt cal values (speed-type at +0x24, temp-type at +0x28) and returns 0 when their signed sum is within +0x1a and the 0xE7628 speed value exceeds a curve of +0x14. Callers 0xE65F4 (phase 3), 0xE7B2C. Cluster of 1. [O] mechanics, [I] purpose.
#### 0x000E74E2 `shift_pred_hold_gate` — logic
Phase-hold completion gate: if machine +0x17 not enabled it reports event 0x37c (module 0xf, class 2/0x12) and returns false; mode 0 reads a cal map (class-alt, param_2 picks slot +0xc/+0x2c) and tests (v-0x80) <= threshold at 0xE75FC+0xa; global 0xE7630 bypasses to the group byte; modes 1/2 compare the 0xE6E20 offset against 0xE7634 and require +0x19 clear. Callers 0xE630C (phase 10) and 0xE58D0. Cluster of 1. [O] mechanics, [I] purpose.
#### 0x000E76DE `shift_temp_scaled_read` — logic
Mode 0 reads a temperature (byte*6-0x366) from cal group 0xE78C0 via a class-alt resolve (range 0x3c), other modes give 0; the value is then scaled by 0xE7782. Callers 0xE7844, 0xE795A, 0xE78EE, 0xE65F4, 0xE6210. Cluster of 1. [O]
#### 0x000E7782 `shift_scale_pct_255` — logic
Scales param_2 by a context factor: (param_2 * factor)/0xff where the factor byte comes from ctx slots selected by mode +4 and the sign of param_2 (ctx chains 0xE78D4/0xE78D8/0xE78DC, factor at 0xE78D0/[0x14]). Callers 0xE76DE, 0xE69F4. Cluster of 1. [O]
#### 0x000E7814 `shift_pred_engage_precond` — logic
Precondition gate: mode 0 -> 0xE7844; mode 1 -> 0xE7844 must return 1, then 0xE7A72; other modes 0. Called from the sequencer 0xE5EAA. Cluster of 1. [I]
#### 0x000E7844 `shift_pred_ctx_val_ok` — logic
True when the ctx gate flags (0xE78DC+0x20/+0x24) are set and the scaled temperature/value from 0xE76DE (mode 0) or 0xE69F4 (mode 1) is positive and ctx+4 byte stays below 0xE78E4. Caller 0xE7814. Cluster of 1. [I]
#### 0x000E78EE `shift_tgt_hold_update` — logic
Under a signed speed condition (ctx+8 value vs 0xE7B50) and machine flag +0x29: takes the 0xE76DE value as a floor into +6 (setting +0x2e, conditionally +0x2f when below +0x32) and returns a scaled cal read via 0xE6D94(+0x30, +0xa, +8); otherwise zeroes +6/+0x2e/+0x30. Caller 0xE65F4. Cluster of 1. [O] mechanics.
#### 0x000E795A `shift_tgt_blend_ramp` — logic
Takes 0xE76DE as reference into +8 (max), blends (v_ref - value)*cal-factor/100 (map at +0x18 slot keyed by +0xc) through vmethod 0xE7B7C, clamps to the reference, and forces <= 0 unless +0x28 is set. Caller 0xE65F4. Cluster of 1. [O]
#### 0x000E7A1E `shift_tgt_apply_derate` — logic
When +0x28 set, mode 0 and +0x2e clear, reduces param_2 through 0xE7AC4 and latches state +0xf = 4 when the value changed. Callers 0xE7E08, 0xE5EAA. Cluster of 1. [O]
#### 0x000E7A56 `shift_tgt_clamp_vmethod` — logic
Clamps param_2 down to the vmethod 0xE7B80() value when that is below the machine target +0x22. Caller 0xE65F4. Cluster of 1. [O]
#### 0x000E7A72 `shift_pred_speed_ok` — logic
Speed condition: value from ctx slot +0x2c (byte) or 0xB7164-interpolated +0x18 slot; returns 0 when +0x28 is clear and (global 0xE7B8C is 0 or the value is above 0xE7B50). Callers 0xE7814, 0xE58D0. Cluster of 1. [I]
#### 0x000E7AC4 `shift_derate_factor_calc` — logic
Derate: map read (ctx slot +0xc at key +0x2c) scaled by param_2 percent; if cal group byte 0xE7B5C-path == 4, a second map (0xE7B94+0x20 slot at 0xE7B98) applies another /100 factor. Caller 0xE7A1E. Cluster of 1. [O]
#### 0x000E7B2C `shift_phase_run_gate` — dispatch
Per-phase start/run gate for the sequencer: phase 3 needs 0xE73AC; phases 5/6 need param_3, value >= 1 and type +2 == 1; phase 4 needs param_3; phase 11 needs type 1 and !param_4; phase 10 needs mode +4 != 2; param_4 blocks 4/5/6/11. Called from 0xE5EAA. Cluster of 1. [O] mechanics.
#### 0x000E7C08 `mach_get_tgt_22` — stub
Two-instruction getter: returns the short at object+0x22 (the machine target). Duplicated 22 times across the app (members incl. 0xF7250, 0xF8B68, 0xF8D80, 0xF932C-0xF934A, 0xF9CF8-0xF9D16, 0xFA368, 0x10FF9A, 0x1132B8, 0x123B5C, 0x137630, 0x138B7A-0x138DDE, 0x14975E) — byte-identical helper copies; member 0x10FF9A sits in the spec-11 phase-engine region. Cluster of 22. [O]
#### 0x000E7C0E `mach_get_result_0` — stub
Two-instruction getter: returns the short at object+0x00 (the machine result). Caller 0xE6210. Cluster of 1. [O]
#### 0x000E7C12 `shift_speed_below_check` — logic
Compares a calibration speed (map read via ctx 0xE7DB0+0x2c slot at 0xE7DB4[0x22]) against the threshold at 0xE7DB8+0x6c, latching flag +0xe when below; mode 1 additionally tests value <= ctx+0x30+threshold and copies the flag to +0xd. Caller 0xE5EAA. Cluster of 1. [O] mechanics.
#### 0x000E7C8E `shift_state19_init_latch` — logic
One-shot (guarded by +0x18): when global 0xE7DC0 and the 0xE7DC8 vmethod are clear sets +0x19 = 0xff; otherwise by mode — mode 1 resolves a class-4 cal object (range 0x22), reads it (vmethod 0xE7DEC) and quantizes via 0xE7DF0 into +0x19, mode 2 copies the ctx byte at +0x2c; then sets +0x18. Caller 0xE5EAA. Cluster of 1. [O] mechanics.
#### 0x000E7D5E `shift_temp_clamp_min` — logic
Reads a temperature ((v-0x91)*6) from a ctx slot (0xE7DF8 chain), clamps it through vmethod 0xE7E04, stores it at +0x12 and returns the smaller of it and param_2 when the temperature is negative. Caller 0xE65F4. Cluster of 1. [O] mechanics.
#### 0x000E7E08 `shift_mach_rampdown_result` — logic
Result path while target +0x20 > 0: computes a floor from a ramp object — type 1 (+0x35) via 0xE8018(obj at +0x44, +0xa, +0x20) scaled /100, type 0 via ctx+0x48 slot +0x34 map — applies the derate 0xE7A1E, latches state +0xf = 0x13 when the floor wins, else clears +0x20. Caller 0xE6B6E. Cluster of 1. [O]
#### 0x000E7EB0 `slot_safe_read_u8` — data-accessor
Null-object-safe slot read: returns the byte at *param unless the slot still points at the object itself (empty sentinel). Cluster of 3 with 0x111FE4 and 0x115346 (r10_app — 0x111FE4 is called by 0x10FFBE, the spec-11 phase-entry function). Cluster of 3. [O]
#### 0x000E7EC0 `slot4_safe_call_u8` — data-accessor
Calls vmethod 0xE803C on the object stored at +4 (unless the slot is the self-sentinel) and returns the byte; 0 when empty. Caller 0xE65F4. Cluster of 1. [O]
#### 0x000E7EDA `slot30_safe_map_read` — data-accessor
Calls the 2D-map vmethod 0xE8038 on the object at +0x30 (unless self-sentinel) with (param_2, param_3); 0 when empty. Member 0xE8018 is the identical helper reading the slot at +0xc. Cluster of 2. [O]
#### 0x000E7EF6 `slot_off8_safe_map_read` — data-accessor
Calls map vmethod 0xE8038 on the object at (chan*4 + param + 8), chan from 0xE8040[2]; 0 when the slot is the self-sentinel. Caller 0xE6CA0. Cluster of 1. [O]
#### 0x000E7F2C `slot10_safe_map_read` — data-accessor
Calls map vmethod 0xE8038 on the object at +0x10 with fixed keys 0xE8040[0xa]/[0x18]; 0 when empty. Callers 0xE6E56, 0xE591E. Cluster of 1. [O]
#### 0x000E7F4E `slot_off18_safe_read_s16` — data-accessor
Returns the s16 behind the pointer at (chan*4 + param + 0x18) unless it is the self-sentinel (then 0). Member 0xE7F76 reads the slot at +0x20 instead. Caller 0xE65F4. Cluster of 2. [O]
#### 0x000E7F9E `slot_off0_safe_read_off80` — data-accessor
Calls curve vmethod 0xE803C on the object at (chan*4 + param) and returns (v - 0x80); 0 when the slot is the self-sentinel. Caller 0xE6E20. Cluster of 1. [O]
#### 0x000E7FCE `slot14_safe_read_off80` — data-accessor
Calls vmethod 0xE803C on the object at +0x14 with key 0xE8044 and returns (v - 0x80); 0 when empty. Caller 0xE6E56. Cluster of 1. [O]
#### 0x000E7FEE `slot30_safe_temp_read` — data-accessor
Calls vmethod 0xE804C on the object at +0x30 with (param_2, 0xE8048) and converts to a temperature (b*6 - 0x366); 0 when the slot is the self-sentinel. Caller 0xE69F4. Cluster of 1. [O]
#### 0x000E8050 `shift_coord_tick` — stub
Four-instruction tail wrapper: calls the method pointer DAT_000E82B8 on object 0xE82B4 (which is 0xE8842). Reached via r08 dispatch 0xBA54C. Cluster of 1. [O]
#### 0x000E8058 `shift_coord_task` — logic
Cyclic task (r08 slot 0xBA554): edge-detects the request byte behind ctx(0xE82C0)+0x1c (notify 0xE82C8, latch 0xE82BC), and when vmethod 0xE82D0 == 1 copies the ctx+0x20 selection byte through 0xE82C8/0xE82DC; then runs eight vmethod update calls over the two coordinator objects 0xE82B4/0xE82E4 and finally 0xE8594 and 0xE85CC. Cluster of 1. [O] mechanics, [I] coordinator purpose.
#### 0x000E80F8 `shift_ctr_inc_group_1b0` — logic
Counter-group increment: gated by 0xE8382 (mode pair 1/1 or 2/2) calls 0xE8492(param, list 0xE8310, buffer at param, param_2, size 0x1b0, limit **(ctx 0xE82C0 + 0x10)). Cluster of 1. [O]
#### 0x000E8128 `shift_ctr_inc_group_40` — logic
Counter-group increment via gate 0xE83B0 (mode 1/1 only): 0xE8492(param, list 0xE8314, buffer param+0x1b0, size 0x40, limit ctx+0x10). Reached via r08 thunk 0xBA4F4. Cluster of 1. [O]
#### 0x000E8190 `shift_ctr_inc_group_8` — logic
Counter-group increment via 0xE83B0: 0xE8492(param, list 0xE831C, buffer param+0x1f8, size 8, limit ctx+0x14). No resolved callers (pointer table). Cluster of 1. [O]
#### 0x000E81C4 `shift_ctr_inc_group2_8` — logic
Counter-group increment via 0xE83B0: 0xE8492(param, list 0xE8320, buffer param+0x200, size 8, limit ctx+0x14). No resolved callers. Cluster of 1. [O]
#### 0x000E81F8 `shift_ctr_inc_group3_4` — logic
Counter-group increment via 0xE83B0: 0xE8492(param, list 0xE8324, buffer param+0x208, size 4, limit ctx+0x14). Cluster of 1. [O]
#### 0x000E822C `shift_ctr_inc_group4_8` — logic
Counter-group increment via 0xE83B0: 0xE8492(param, list 0xE8328, buffer param+0x20c, size 8, limit ctx+0x10). Reached via r08 thunk 0xBA514. Cluster of 1. [O]
#### 0x000E8270 `shift_ctr_inc_group5_28` — logic
Counter-group increment via 0xE83B0: 0xE8492(param, list 0xE8334, buffer param+0x214, size 0x28, limit ctx+0x14). Reached via r08 thunk 0xBA524. Cluster of 1. [O]
#### 0x000E8260 `shift_evt_mark_8` — stub
Wrapper: marks event slot list DAT_000E832C via 0xE8512(param, list, param_2, 8). Reached via r08 thunk 0xBA51C. Cluster of 1. [O]
#### 0x000E8268 `shift_evt_mark_8b` — stub
Wrapper: marks event slot list 0xE8330 via 0xE8512(param, list, param_2, 8). Caller 0xE8686. Cluster of 1. [O]
#### 0x000E82A4 `shift_evt_mark_1f` — stub
Wrapper: marks event slot list 0xE8338 via 0xE8512(param, list, param_2, 0x1f). Caller 0xE874A. Cluster of 1. [O]
#### 0x000E82AC `shift_evt_mark_11` — stub
Wrapper: marks event slot list 0xE833C via 0xE8512(param, list, param_2, 0x11). Caller 0xE8774. Cluster of 1. [O]
#### 0x000E8340 `shift_evt_mark_1` — stub
Wrapper: marks event slot list 0xE8574 via 0xE8512(param, list, param_2, 1). Caller 0xE8842. Cluster of 1. [O]
#### 0x000E8348 `shift_evt_mark_gear` — logic
Range-checks param_2 (1..5) then marks event slot list 0xE8578 via 0xE8512(param, list, param_2, 5). Reached via r08 thunk 0xBA544. Cluster of 1. [O]
#### 0x000E835E `shift_ctr_gate_check` — logic
Predicate: ctx byte behind 0xE857C+0x18 == 1 and vmethod 0xE8584(0xE8580) == 1 — the enable gate shared by the counter/mark helpers. Callers 0xE8492, 0xE8512. Cluster of 1. [O]
#### 0x000E8382 `shift_mode_pair_1_2_active` — logic
True when the mode bytes PTR_DAT_000E8588[0x2c]/[0x2b] are both 1 or both 2. Callers 0xE80F8, 0xE8594, 0xE85CC. Cluster of 1. [O]
#### 0x000E83B0 `shift_mode_pair_1_active` — logic
True when PTR_DAT_000E8588[0x2c] == 1 and [0x2b] == 1 (stricter than 0xE8382). Callers the six counter-group wrappers. Cluster of 1. [O]
#### 0x000E83CA `ctr3_bytes_increment` — library
Increments a 3-byte counter read/written through vtable methods (+0x14 read / +0x20 write on the param_2 descriptor): carries from byte 2 to 1 to 0, treats 0xff/0xff/0xff as absent and skips. Callers 0xE8492, 0xE8512. Cluster of 1. [O]
#### 0x000E8492 `shift_ctr_inc_wrap` — logic
Aging/event counter: when gate 0xE835E passes and param_4 != 0 — if param_5 < param_4 reports error 0x489 (module 0xf, class 1/3, code 0x6e), otherwise increments the byte at param_3 + param_4-1; when it reaches (ctx limit param_6)/3 it wraps to 0 and carries into the 3-byte counter via 0xE83CA at (param_4-1)*3. Cluster of 1. [O]
#### 0x000E8512 `shift_evt_mark_wrap` — logic
Event marker: when gate 0xE835E passes and param_3 != 0 — error 0x489 (class 2/3, code 0x97) when param_4 < param_3, else marks the 3-byte entry at (param_3-1)*3 through 0xE83CA. Six wrappers feed it. Cluster of 1. [O]
#### 0x000E8594 `shift_level_search_mark` — logic
Searches level 1..7 for the first vmethod 0xE87D0(ctx(0xE87C8)+0xc, idx) value >= threshold 0xE87CC, then (gate 0xE8382) marks the level in the counter group at buffer +0x1f0 (8 entries) via 0xE8492 with limit ctx+0x14. Caller 0xE8058. Cluster of 1. [O] mechanics.
#### 0x000E85CC `shift_3d_index_update` — logic
Computes a 3D selection index — x = first of 1..5 with vmethod 0xE87E8(ctx+4, i) >= 0xE87E4+0x7e, y = first of 1..7 with 0xE87EC(ctx+8, i) >= 0xE87E4+100 short, z = 9 for mode pair 2/2 else mapped from PTR_DAT_000E87DC[0x21] (1..8) — and pushes (y-1)*6 + x + (z-1)*0x30 through vmethod 0xE87F0 on coordinator 0xE87D4. Caller 0xE8058. Cluster of 1. [O] mechanics, [I] coordinator purpose.
#### 0x000E8686 `shift_preselect_state_mach` — logic
Three-state machine on param_1[0]: state 0 starts when global 0xE87F4 == 1 and scans levels 1..7 (vmethod 0xE87D0 on ctx 0xE87C8+0xc vs threshold 0xE87CC) into +1; state 1 ticks the short at +2 (increments of 3) until 0xc9 then pushes the level through vmethod 0xE87F8 on coordinator 0xE87D4 and moves to 2; state 2 waits for global 0xE87FC != 1 to reset; other states report error 0x48b (code 0xb9). Caller 0xE8058. Cluster of 1. [O] mechanics, [I] purpose.
#### 0x000E874A `shift_type_from_gears_tbl` — logic
When param_4 == 1 and both gears positive, selects byte table9x9 at 0x634da [param_2*9 + param_3] and pushes it through vmethod 0xE880C on coordinator 0xE87D4 (else 0). Reached via r08 thunk 0xBA52C. Table sits next to the spec-11 sequence table 0x63555. Cluster of 1. [O] mechanics, [I] type semantics.
#### 0x000E8774 `shift_type_by_pair_kind` — logic
Selects a type by (param_2, param_3) combinations — (0/4, 2) -> 8, (0/4 with param_3 0/4?) -> 0x11, (0/3, 1) -> table 0xE8810[param_4 1..8], (0/3 with param_2 1) -> table 0x63534[param_4] — and pushes it through DAT_000E8878 on object 0xE8874. Reached via r08 thunk 0xBA538. Cluster of 1. [O] mechanics, [I].
#### 0x000E8842 `shift_req_latch_notify` — logic
Edge detector on the byte behind 0xE8880: when the previous (0xE887C) is not 1 and the new value is 1, calls vmethod 0xE8884(0xE8874, 1); latches the previous value. Called through 0xE8050. Cluster of 1. [O]
#### 0x000E8888 `diag_forced_pressure_10bar` — data-accessor
Returns 10000 (mbar) when the selected diagnostic source is non-zero — param_2 3/2 read bytes 0xE8A88/0xE8A8C, 4 reads vmethod 0xE8A94(0xE8A90), 1 returns 0, anything else reports error 0x195 (code 0xdb). No resolved callers (pointer table). Matches the line-pressure forced-override path of spec 11 §3a. Cluster of 1. [I]
#### 0x000E88FA `park_output_update` — logic
The park-module output routine (spec 12 §1.2): refreshes +3 from a cal byte when gate 0xE8AA0[0x2c]; recomputes the park-lock decision +1 = (!blocked 0xE8AB0) && (aux request 0xE8AB4 || (park-request vmethod 0xE8ABC(0xE8AB8) && release condition 0xE8B56)); clears +2 on decision change; drives output 1 constant, output 2 = !park-request, output 3 = !decision via 0xE89D0, then applies the diagnostic overrides for outputs 1-4 (list 0xE8AA4) and runs the release sub-fsm 0xE89DE. Called via r08 thunk 0xB85F0. Cluster of 1. [O] structure matches spec 12 §1.2.
#### 0x000E89BA `park_out_init` — logic
Initialises the two output blocks at param+2/+3 through vmethods 0xE8AC8/DAT_000E8ACC. Reached via r08 thunk 0xB85E8 (paired with 0xE88FA's slot 0xB85F0). Cluster of 1. [O]
#### 0x000E89D0 `park_out_write` — data-accessor
Writes output byte param_3 at index param_2 into PTR_DAT_000E8AD4[idx] with the inverted copy ~param_3 at 0xE8AD0[idx] — the 0xFFF851A8/0xFFF851AD pair of spec 12 §1.2. Cluster of 1. [O]
#### 0x000E89DE `park_lock_sub_fsm` — logic
Fixed-point loop over the park state byte +0: states 1-4 — 3 -> 4 when a calibrated value (vmethod 0xE8AAC on 0xE8AA8 ctx with key 0xE8AD8) is <= +2 and flag 0xE8A88 == 0, and -> 2 when decision +1 == 0; 4 -> 2 when vmethod 0xE8ABC == 0; 2 -> 1 when the speed behind 0xE8AA8+4 <= +2, -> 3 when +1 == 1; invalid states go to 4 with error 0x195 (code 0xb0); >10 iterations abort with error 0xb6. Cluster of 1. [O] mechanics, [I] state meanings.
#### 0x000E8B56 `park_release_cond` — logic
Release condition with hysteresis (spec 12 §1.2 names this address): returns 1 unless (+3 set && 0xE8B7E != 1 && 0xE8BAA == 0) — i.e. a speed/pressure window (0xE8B7E) or the force condition (0xE8BAA) can keep the lock engaged. Cluster of 1. [O]
#### 0x000E8B7E `park_release_window_ok` — logic
Returns 0 (window not ok) when the speed behind ctx(0xE8BCC)+0xc is below cal 0xE8BD0+0xc4 and the value 0xE8BD0+0x7c is below ctx+0x10 and cal byte 0xE8BD0[0x10d] is 0 — the hysteresis window behind 0xE8B56. Cluster of 1. [O]
#### 0x000E8BAA `park_force_release_cond` — logic
Returns 1 when the byte behind ctx(0xE8BCC)+0x14 is 0 and global 0xE8BD4 is non-zero. Cluster of 1. [O]
#### 0x000E8BD8 `park_fsm3_ctor` — logic
Allocates a 3-byte object (allocator 0x9AA34) and sets [0] = 2, [2] = 0. No resolved callers — reached via a pointer/constructor table; the state values match the 0xE89DE fsm. Cluster of 1. [I]
#### 0x000E8BFA `diag_test_seq_step` — dispatch
Diagnostic actuator-test sequencer (r08 slot 0xBA652): overrides flags from 0xE8E30[0xd8]/[0xd9] into 0xE8E38/0xE8E3C; states 2 (run 0xE9048 then -> 4), 4 (0xE90E0 -> 3), 3 (0xE916A), 5/6 (0xE9340 checks, -> 6/5), 7 -> 8, 8 -> 2 when 0xE8E30[0x50] == 2; unknown states report 0x2c3 (code 0xc9) and go to 5; latches 0xE8E44 from ctx+4 each pass. Cluster of 1. [O] mechanics, [I] test semantics.
#### 0x000E8CF6 `diag_test_results_eval` — logic
Evaluates actuator-test results (r08 slot 0xBA65A) when the test-active gates (0xE8E50, vmethods 0xE8E58/0xE8E5C) and state 5 hold: per test slot 1-3 iterates the test objects (counters at 0xE8E60), reporting 0x2c3 faults (codes 0xe/0x14, 0x2a/0x30, 0x45/0x4b, and 0x56 for none) and writes the result class 1/2/3 into 0xE8E60[3]. Cluster of 1. [O] mechanics.
#### 0x000E9030 `diag_test_is_state5` — stub
Predicate: object state byte == 5. Reached via r08 thunk 0xBA594. Cluster of 1. [O]
#### 0x000E9038 `diag_test_phase_is_1` — stub
Predicate: object byte +2 == 1. Reached via r08 thunk 0xBA636. Cluster of 1. [O]
#### 0x000E9040 `diag_test_phase_is_2` — stub
Predicate: object byte +2 == 2. Reached via r08 thunk 0xBA644. Cluster of 1. [O]
#### 0x000E9048 `diag_test_phase2_run` — logic
Phase 2 of the actuator test: iterates the test-object list (count 0xE90EC, get 0xE90F0); objects whose result (+0x20 vmethod) is 0x80 (idle) get their conditions (+0x5c/+0x74) checked — pass: start (+0x14) and hold setpoint 0x80 (+0x2c), phase +2 = 1; fail: phase +2 = 2. Caller 0xE8BFA. Cluster of 1. [O] mechanics.
#### 0x000E90E0 `diag_test_phase3_reset` — logic
Iterates the second test-object list (count 0xE93A0, get 0xE939C): objects with a non-0x80 result get reset (+0x14 vmethod) when their conditions fail, and every object is returned to setpoint 0x80 (+0x2c). Caller 0xE8BFA. Cluster of 1. [O] mechanics.
#### 0x000E916A `diag_test_step_iter` — logic
Steps the per-object test state: reads each object's state (+0x20 vmethod) and records a sub-state 1-10 into param+1 — 0 -> 5/6, 1 -> 1/2, 2 -> 3/4 (condition vmethods decide), 4 -> 7 (with reset+0x80), 0x80 -> 8, other -> 9 (error 0x2c3 code 0xef); special-cases global 0xE93B8 == 2 (reset, 10). Advances and wraps the counter at 0xE93A4; returns done. Caller 0xE8BFA. Cluster of 1. [O] mechanics.
#### 0x000E9340 `diag_test_iter_apply` — logic
Optionally resets the iteration counter (param_2 == 1), then iterates the test-object list calling the vmethods at +0x50/+0x80 (ctx +0x48/+0x78) on each; returns true when the counter reaches the list size. Caller 0xE8BFA states 5/6. Cluster of 1. [O] mechanics.
#### 0x000E93C0 `vt_reset_e93dc` — stub
Four-instruction r08 dispatch wrapper (slot 0xBA754): calls the method PTR_FUN_000E93E0 on object 0xE93DC, which resolves to 0xF34B2 (zeroes word +6 — reset). Cluster of 3 with 0xE93CA/0xE93D2 (same wrapper shape, different lifecycle methods). [O]
#### 0x000E93CA `vt_step_e93dc` — stub
Sibling wrapper (slot 0xBA760): calls 0xF31B2 — the 303-instruction cyclic update of the 0xE93DC subsystem object. [O]
#### 0x000E93D2 `vt_publish_e93dc` — stub
Sibling wrapper (slot 0xBA766): calls 0xF3180 — publishes fields +8/+6/+0x10/+0x12 of the 0xE93DC object to a consumer and pokes 0xF3350. [O]
#### 0x000E93FC `shift_api_call0a_9498` — stub
Four-instruction interface wrapper: calls method PTR_FUN_000E94A0 on module object 0xE9498 (resolves to r10 0x1163F6). Member 0xE9404 is the same wrapper for method 0xE94A4 (r10 0x116724). Callers 0xDE0FE / 0xEDC5A. Cluster of 2. [O]
#### 0x000E940C `shift_api_req_2arg` — stub
Interface wrapper: calls method 0xE94A8 on object 0xE9498 with (param_2, param_3), forwarding to r10 0x116422. Caller 0xED92E. Cluster of 1. [O]
#### 0x000E9416 `shift_api_tick_94ac` — stub
Wrapper: calls PTR_LAB_000E94B0 on object 0xE94AC. Caller 0xEBDBC (line-pressure controller). Cluster of 1. [O]
#### 0x000E941E `shift_api_req4_94ac` — stub
Wrapper: calls method 0xE94B4 on object 0xE94AC with four args and returns its byte (r10 0x115566). Callers 0xEBDBC, 0xED9B2. Cluster of 1. [O]
#### 0x000E943E `shift_get_flag_e94b8` — stub
Three-instruction getter: returns the byte behind PTR_DAT_000E94B8. Callers 0xEDE82 and 0x10FA28 (r10 phase-engine region — cross-lead). Cluster of 1. [O]
#### 0x000E9446 `shift_api_call3_9498` — stub
Wrapper: calls method 0xE94BC on object 0xE9498 with three args (r10 0x1167CC). Callers 0x11755E, 0x10D0B4. Cluster of 1. [O]
#### 0x000E9452 `shift_get_e94c0` — stub
Getter: returns byte behind DAT_000E94C0. Reached via r08 thunk 0xBA8A0. Cluster of 1. [O]
#### 0x000E9458 `shift_get_e94c4` — stub
Getter: returns byte behind DAT_000E94C4. Reached via r08 thunk 0xBA8F4. Cluster of 1. [O]
#### 0x000E945E `shift_get_e94c8_s16` — stub
Getter: returns the short behind PTR_DAT_000E94C8. Callers 0xEDC5A, 0x10E896, 0x110C8E, 0xEBDBC. Cluster of 1. [O]
#### 0x000E9464 `shift_get_e94cc` — stub
Getter: returns the byte behind DAT_000E94CC. Callers 0x1144B0, 0xEDC5A, 0x10E896, 0x1150D4, 0x1132F4, 0xEBDBC — the most widely read state byte of this module. Cluster of 1. [O]
#### 0x000E946A `shift_get_9498_state` — stub
Getter: returns the first byte (state) of module object 0xE9498. Reached via r08 thunk 0xBA93E. Cluster of 1. [O]
#### 0x000E9470 `shift_api_call0b_9498` — logic
Wrapper: calls method 0xE94D0 on object 0xE9498, returning its byte (r10 0x116806). Member 0xE947E is the same wrapper for method 0xE94D4 (r10 0x116818). r08 thunks 0xBAA20 / 0xBA94C. Cluster of 2. [O]
#### 0x000E948C `shift_get_e94d8` — stub
Getter: returns the byte behind DAT_000E94D8. Reached via r08 thunk 0xBAA2E. Cluster of 1. [O]
#### 0x000E94DC `shift_obj14_ctor` — logic
Allocates a 0xe-byte object (allocator 0x9AA34) when param_1 is null and returns it. No resolved callers — constructor reached via a pointer table. Cluster of 1. [I]
#### 0x000E94FC `shift_pub_field_d` — stub
Publishes param+0xd to consumer 0xE96D8 via method 0xE96DC (r08 0xBAE46 family). Caller 0xDE0FE. Cluster of 1. [O]
#### 0x000E9508 `shift_seq_last_step_reset` — logic
On the last sequence step (param_2 > 1 and param_3 == param_2-1): resolves a calibration id from table 0xE96E8 [vmethod()*4 + param_3*0x24] with the 0..6 validation/fallback pattern into a slot pair, clears +0xd, sets +0xc = 1 and result *param_1 = 0. Caller 0xDDDE8. Cluster of 1. [O] mechanics.
#### 0x000E9590 `shift_m2_step_all` — logic
Runs the machine-2 stepper 0xE95A2 in a loop until it reports no state change. Called from task 0xDE0D8 via 0xE94EE. Cluster of 1. [O]
#### 0x000E95A2 `shift_m2_step` — dispatch
Machine 2 state byte +0xc: 0 idle (clears result/flags); 1 checks engage condition 0xE971C — pass sets +4 flag, +6 = ctx+0x28 byte, -> 2, fail waits for +0xd > ctx+0x24 byte; 2 waits for global 0xE9700 then -> 3; 3 computes result *param_1 from a ctx map (key param_1[4], 0xE9704) and -> 4 when the ctx+8 speed (minus 0x80) <= 0xE9704; 4 holds result scaled (0xE970C map * +2 / 0xff via 0xE9710). Abort condition 0xE9696 forces 0. Cluster of 1. [O] mechanics, [I] purpose.
#### 0x000E9696 `shift_m2_abort_cond` — logic
Returns 0 (no abort) when cal 0xE9714[0x9c] <= ctx(0xE96FC)+8 byte and the speed window (cal +0x94 - ctx+0x14 short <= cal +0x7e + 0xE9718) holds or global 0xE9700 is set. Caller 0xE95A2. Cluster of 1. [O]
#### 0x000E971C `shift_m2_engage_cond` — logic
Engage window chain: ctx(0xE97F8)+0x18 == 1, cal 0xE97FC <= ctx+8, map flag 0xE9804(ctx+0x1c, 0xE9800[0x23]) == 1, then a four-sided window on ctx-list values vs cal 0xE9808 (+0x86/+0x96 shorts, bytes) and a speed condition (cal +0x94 + 0xE980C+0x10 >= map read 0xE9810) with cal[0xb0] clear. Caller 0xE95A2. Cluster of 1. [O] mechanics.
#### 0x000E97C6 `shift_m2_out8_calc` — logic
Computes machine-2 byte +8: vmethod 0xE981C(cal u16 at 0xE9808+0x94 - (u16 at +0x7c * global 0xE9814 factor)/1000). Caller 0xE94EE (task 0xDE0D8). Cluster of 1. [O]
#### 0x000E9820 `shift_m2b_step_entry` — logic
Entry point: calls 0xE9A98(param, *DAT_000E9A50, 0). Called from task 0xDE0D8. Cluster of 1. [O]
#### 0x000E982A `shift_m2b_active_pred` — stub
Predicate: object +5 flag != 0 (machine 2b active). Caller 0x10E5EE (r10_app). Cluster of 1. [O]
#### 0x000E9832 `shift_m2b_mode_pred` — stub
Predicate: object +5 == param_2 or object +6 == param_2. Caller 0x10D9B6 (r10_app). Cluster of 1. [O]
#### 0x000E984A `shift_m2b_start` — logic
Starts machine 2b: by param_2 sets +4 = 2 (or 1, with the mode vmethod 0xE9A58 == 2 variant selecting cal set 2); resolves two calibration ids (tables at 0xE9A60[set]/[set+3], range 0..5, error-report fallback) into slot pairs at +0xc and +0x24; then steps via 0xE9A98(param, cal[0x2c], 1). Caller 0xDDCA2. Cluster of 1. [O] mechanics.
#### 0x000E992E `shift_m2b_pub_field_a` — stub
Publishes param+0xa to consumer 0xE9A74 via method 0xE9A78. Caller 0xDE0FE. Cluster of 1. [O]
#### 0x000E993A `shift_m2b_gate_cond` — logic
Engage gate for machine 2b: requires mode change (param_2 != 0xEA7C value), cal 0xE9A68[0x27] < 3, and a window over the ctx list 0xE9A80 (temp byte > 0xE9A84[0x9c], speeds +0x7c/+0x94, byte +0x96); when param_3 == 0 returns 1, else checks the mode vmethod 0xE9A58 (1/2/-1 pass) and change-condition via 0xE9A88. Caller 0xE9A98. Cluster of 1. [O] mechanics.
#### 0x000E99E8 `shift_m2b_out0_ramp` — logic
Computes output 0 (*param_1): picks a context slot (+4 or +8 offset by a flag pair from vmethods 0xE9A8C/0xE9A90 on 0xE9A54 and the speed 0xE9A94 window), reads the map behind ctx 0xE9A80+0xc through the +0x20 vmethod of the ctx object at +0x20, clamped >= 0. Caller 0xE9C06. Cluster of 1. [O] mechanics.
#### 0x000E9A98 `shift_m2b_step` — logic
Machine-2b stepper: refreshes +8 = 0xE9CB4(cal 0x9CB0+0x94 - +0x7e), clears +0xa on param_2; gate 0xE993A — fail or ctx+0x14 byte < +0xa resets via 0xE9B0C, otherwise 0xE9C06 (param_3 == 0) or 0xE9B1A, clearing +4 when both output flags drop. Callers 0xE9820, 0xE984A. Cluster of 1. [O] mechanics.
#### 0x000E9B0C `shift_m2b_reset` — logic
Zeros machine-2b state: +5 flag, result *param_1, +6, +2 (param_1[1]) and +4. Caller 0xE9A98. Cluster of 1. [O]
#### 0x000E9B1A `shift_m2b_outputs_update` — logic
Updates both outputs: mode byte from 0xE9C3E(param_2, 2/4-with-limit 5-or-3 by vmethod 0xE9CC0/0xE9CCC), on change reads the map (0xE9CC8 on ctx +0x10, key param_1[4]/0xE9CC4) and writes it through the ctx +0x14 vmethod (slots +0x10/+0x28 of the two ctx objects at +0xc/+0x24 of the machine); clears everything when the stop vmethod 0xE9CD0 fails. Caller 0xE9A98. Cluster of 1. [O] mechanics.
#### 0x000E9C06 `shift_m2b_outputs_hold` — logic
Holds/refreshes both outputs: output 0 from 0xE99E8 (clear +5 when < 1), output 1 from 0xE9C4A (clear +6 when < 1). Caller 0xE9A98. Cluster of 1. [O]
#### 0x000E9C3E `shift_m2b_sel_param` — library
Two-way select: returns param_3 when param_2 == 1 else param_4. Caller 0xE9B1A. Cluster of 1. [O]
#### 0x000E9C4A `shift_m2b_out1_ramp` — logic
Output-1 twin of 0xE99E8 over the second ctx object pair (slots +0x24/+0x38, cal 0xE9CD4/0xE9CBC, threshold 0xE9CDC), map read through +0x20 vmethod, clamped >= 0. Caller 0xE9C06. Cluster of 1. [O]
#### 0x000E9CE0 `lp_strategy_update` — logic
The line-pressure strategy update (spec 11 §3a, object 0xFFF8BCB0, called via 0xBABEE): cycle reset 0xEA230, steady-state demands 0xEA03A, hold-min offer 0xEA07E, demand collection 0xEA4C8, then result param_1[2] = max of both classes via 0xEA774; ramps up to the minimum when below ctx+4; clamps to the limit curve (0xE9F50 on ctx+0xac, [0x27]+1); adds the warm-up extra param_1[10] (product of two curves /10, latched by +0x13); filters through the ramp object at +0x50 (rate ctx+0xb4 * 1000) into *param_1; +0x02 = result/10; filtered copy +0x10 via 0xE9F70; +0x0e = result - 5000 when > 4999. Cluster of 1. [O] — field offsets match spec 11 §3a exactly.
#### 0x000E9E5A `lp_shift_end_reset` — logic
Calls the end-of-shift detector 0xEA0EE and clears flag +0x1d. Caller 0xEDC5A. Cluster of 1. [O]
#### 0x000E9E6A `lp_ss_demand_2gears` — logic
Steady-state demand across two speed classes (spec 11 §3a "from/to gear"): 0xEA37E for the [0x22] gear, plus ctx slot +0x20 (mode 1/3) or +0x24 (mode 2/4) value, both clamped by 0xE9F74 and offered with reasons +0x1a byte and 0x10. Caller 0xEA03A. Cluster of 1. [O]
#### 0x000E9ED2 `lp_cal_id_pick_12` — data-accessor
Resolves a calibration object id by param_2 (1: table 0xE9F7C, range 0..6; 2: table 0xEA1CC, range 0..11) using the (id, validated pointer) slot-pair pattern into 0xE9F78 / 0xEA1C4. Caller 0xED92E. Cluster of 1. [O]
#### 0x000E9FF0 `lp_shift_demand_get` — logic
Returns the extra shift demand (spec 11 §3a): when global 0xEA1DC != 1 the short behind ctx(0xEA1C4)+4; else the first (flag 0xEA1E0 == 0) or second short behind ctx(0xEA1E4)+4. Caller 0xEA28C. Cluster of 1. [O]
#### 0x000EA01E `lp_pub_fields_1f_22` — stub
Publishes fields +0x1f and +0x22 to consumer 0xEA1E8 (methods 0xEA1EC/0xEA1F0, 0xBAE46/0xBAE5E family). Caller 0xDE0FE. Cluster of 1. [O]
#### 0x000EA03A `lp_ss_demand_update` — dispatch
Dispatches the steady-state demand: gear-steady ([0x2b] == [0x2c] and [0x21] == [0x27]) -> 0xEA37E offered with reason +0x1a; gear change -> 0xEA28C (shift); mode mismatch -> 0xE9E6A (two speed classes). Caller 0xE9CE0. Cluster of 1. [O] — the three paths of spec 11 §3a.
#### 0x000EA07E `lp_offer_max_reason1` — logic
When global 0xEA1F8 is set or flag +0x12 is set, offers the limit-curve value (0xEA200 on ctx+0xac with key [0x1fc]+1) with reason 1. Caller 0xE9CE0. Cluster of 1. [O] mechanics, [I] purpose.
#### 0x000EA0B2 `lp_obj_ctor` — logic
Allocates the 0x64-byte line-pressure object (allocator 0x9AA34) and initialises: flags +0x12/+0x1d/+0x13 = 0, +0x20 = 6, shorts +8/+0xa/+0xc/+0x14 = 0, reason bytes +0x17/+0x18 = 0xff. No resolved callers (constructor table). Cluster of 1. [O] — size and fields match the 0xFFF8BCB0 object.
#### 0x000EA0EE `lp_shift_end_detect` — logic
End-of-shift detection: enter (+0x12 = 1) when mode 0xEA1DC == 3 with vmethod 0xEA20C == 1 (or 0xEA210 == 1 and mode != 4) and the speed window (ctx+0x14 byte < [0x96], +0x94 < ctx+0x18) holds, clearing +0x1f; completes (+0x1d = 1) when the progress curve 0xEA21C(ctx+0x48) quantized by 0xEA220 passes +0x1f; exits on release conditions ([1]==0, [2] != 3 with 0xEA228, or vmethod 0xEA22C with mode 3). Callers 0xEA230, 0xE9E5A. Cluster of 1. [O] mechanics.
#### 0x000EA230 `lp_cycle_reset` — logic
Per-cycle reset: latches +0x1e = +0x12 and +0x19 = +0x16, +6 = +4, then zeroes the demand state (shorts +4/+8/+0xa/+0xc, reason bytes +0x16/+0x17/+0x18 = 0xff, ramp +0x28), keeps +0x1c unless it is 2, refreshes +0x26 from vmethods 0xEA470/0xEA474, and runs 0xEA0EE. Caller 0xE9CE0. Cluster of 1. [O]
#### 0x000EA28C `lp_shift_demand_offer` — logic
During-shift demand (spec 11 §3a reason 0x11): max (0xEA480) of the two steady-state demands 0xEA37E for gears 0xEA478/0xEA47C, plus the shift demand 0xE9FF0; adds the curve at ctx+0x40 when mode 0xEA484 == 3; offers with reasons +0x1a and 0x11. Also calls r10 0x104D58. Caller 0xEA03A. Cluster of 1. [O]
#### 0x000EA2F8 `lp_ramp_gate_pred` — logic
Gate for result ramping: true when (+0x1e && !+0x12) or (+0x19 == 1 && +0x16 != 1 && +0x26). Caller 0xE9CE0. Cluster of 1. [O]
#### 0x000EA33E `lp_ramp_to_min` — logic
When the value is below the previous minimum 0xEA490 and the gate param_3 is clear, ramps it through the ramp object at +0x58 (vmethod 0xEA494) toward the floor **(ctx 0xEA48C+0x1c); otherwise resets the ramp (0xEA498); stores the result back to 0xEA490. Caller 0xE9CE0. Cluster of 1. [O]
#### 0x000EA37E `lp_ss_demand_calc` — logic
Steady-state demand per gear (spec 11 §3a reasons 0x19/0x18): resolves the per-gear cal map id (table 0xEA49C, range 0..8); single-gear case reads the 3D map (0xEA4B4 on ctx+0x58) + temperature curve 0xEA4B8 (reason 0x19); gear-change case reads the map behind ctx 0xEA4A0+4 (keys 0xEA4BC, +0xc) + curve 0xEA4B8 on ctx+0x4c (reason 0x18); clamped by 0xEA4C4. Callers 0xEA28C, 0xEA03A, 0xE9E6A. Cluster of 1. [O]
#### 0x000EA4C8 `lp_collect_demands` — logic
Collects all line-pressure demands: forced override 0xEA84E(0,0,8) under diag gates ([0xd4] with [0xb2]/[0xd5] or vmethod 0xEA630); offers with reasons 2 (0xEAAC4), 0x31, 0x3b, 3, 0x21, 4, 5 (curve 0xEA664), 6, 0xc (gated by 0xEA9EE with cal resolve + 0xEAC0E), 0xf, 0x1c (0xEAB0A + curve 0xEA878), 0x13 (0xEAB90), 0x1a (post-shift condition), 0x2d (curve 0xEA87C when [0x2b] == 1), 0x48 (mode 4); and 0xEA89C when the speed exceeds [0x94]. Caller 0xE9CE0. Cluster of 1. [O] mechanics, [I] reason meanings.
#### 0x000EA774 `lp_pick_max_demand` — logic
Takes the larger demand (spec 11 §3a): ramps the second-class max +0xa through the filter at +0x50 (vmethod 0xEA888) into +8, then picks +8 (reason byte +0x17) or the normal max +0xc (reason +0x18) into reason +0x16; a forced value (+0x28 != 0) returns +4 directly. Caller 0xE9CE0. Cluster of 1. [O] — matches spec 11 §3a field-for-field.
#### 0x000EA7BA `lp_reason_class_check` — data-accessor
Validates the reason byte (<= 100, else error 0x363 code 0xd0) and returns the class-2 classification from vmethod 0xEA898 on ctx+0xb0. Caller 0xEA806. Cluster of 1. [O]
#### 0x000EA806 `lp_offer_demand` — logic
The demand-offer entry of spec 11 §3a: classifies the reason via 0xEA7BA; class 1 competes for max +0xc (reason +0x18), class 2 for max +0xa (reason +0x17); both ignore while a forced value (+0x28) is active. Callers 0xEA4C8, 0xEA89C, 0xEA28C, 0xEA07E, 0xEA03A, 0xE9E6A. Cluster of 1. [O]
#### 0x000EA84E `lp_offer_forced` — logic
Forced-value offer (spec 11 §3a): stores (param_2, param_3) into +4/+0x16 with priority param_4 into +0x28 when it exceeds the current priority. Caller 0xEA4C8. Cluster of 1. [O]
#### 0x000EA89C `lp_torque_demands_offer` — logic
Five torque-class demands (speed-gated from 0xEA4C8): for classes 1-5 reads a curve per ctx slots +0x60/+0x64/+0x68/+0x6c/+0x70 (keys via 0xEAB4C/0xEAB50), combines with the map value on ctx+0x5c (0xEAB58/0xEAB5C), quantizes via 0xEAB60 and offers each with reason 10. Cluster of 1. [O] mechanics.
#### 0x000EA9EE `lp_torque_gate_latch` — logic
Latches gate +0x24 for the torque demands: enter when mode 0xEAB54[0x21] ∈ {1,2,-1} and the temp/speed thresholds (0xEAB64[0x96]/+0x94/+0x64) are passed; exit with hysteresis (delta vmethod 0xEAB68 and window +0xa0/+0xa4). Caller 0xEA4C8. Cluster of 1. [O] mechanics.
#### 0x000EAA9A `lp_demand2_ramp` — logic
When latch +0x1c is set, ramps the class-2 demand through the ramp object at +0x3c (vmethod 0xEAB6C, rate = ctx+8 short * 1000). Caller 0xEAAC4. Cluster of 1. [O]
#### 0x000EAAC4 `lp_demand2_offer` — logic
Class-2 demand: value = short behind ctx(0xEAB44)+4; when it exceeds max(+8, +0xc) it latches +0x1c, floors +6, and returns the ramped value from 0xEAA9A. Caller 0xEA4C8 (reason 2). Cluster of 1. [O]
#### 0x000EAB0A `lp_speed_window_latch` — logic
Latches +0x25: gate vmethod 0xEAB74(0xEAB70), speed window — value from 0xEAB78(0xEAB70,1,0) vs ctx+0x88 short and the delta against ctx+0x90. Caller 0xEA4C8 (reason 0x1c). Cluster of 1. [O] mechanics.
#### 0x000EAB90 `lp_countdown_demand` — logic
Mode-8 demand: when cal 0xEAC1C[0xa0] == 8 and +0x20 == 8 and [0xa2] set, reads the curve behind ctx+0x78 (key [0xe1]) — zeroing +0x22 unless [0xa2] == 3 with the +0x7c-curve condition — and latches the mode into +0x20. Caller 0xEA4C8 (reason 0x13). Cluster of 1. [O] mechanics, [I].
#### 0x000EAC0E `slot4_safe_read_s16` — data-accessor
Null-object-safe read: returns the s16 behind the pointer at +4 unless it is the self-sentinel (then 0). Caller 0xEA4C8. Cluster of 1. [O]
#### 0x000EAC2C `lp_pred_speedwin_thunk` — logic
r08 entry (slot 0xBAC0E) that is a thunk/duplicate of 0xEAC64. Cluster of 1. [O]
#### 0x000EAC46 `lp_pred_vmethod_gate` — logic
Predicate: vmethod 0xEAD70 on ctx(0xEAD6C)+8 with param_2 returns non-zero. Callers 0xEACB0, 0x112DB0 (r10). Cluster of 1. [O]
#### 0x000EAC64 `lp_pred_speedwin` — logic
Computes the speed-window gate into *param_1: list values (0xEAD6C ctx) below cal 0xEAD74+0x94/+0x96 window and gear-mode pair 0xEAD78[0x2b] == 1 == [0x2c]. Caller 0xDDDE8. Cluster of 1. [O]
#### 0x000EACB0 `lp_pred_speed2_eval` — logic
Evaluates +1 = (state 0, or global 0xEAD7C != 1, or gate 0xEAC46(0xEAD80) fails) ? 0 : 1. Caller 0xEAC30. Cluster of 1. [O]
#### 0x000EAC30 `lp_pred_speed2_update` — logic
Update path: param_2 == 0 or +2 clear resets +1; otherwise evaluates 0xEACB0. Callers 0xEDC5A, 0xEBC60, 0xEBB6C (the 0xEB640+ controller). Cluster of 1. [O]
#### 0x000EACE0 `lp_pred_pressure_ok` — logic
Computes +2: requires param_2, +3 armed, the curve value (0xEAD88 on ctx+0x10, key 0xEAD84) above +4, and mode pair [0x2b] == 1; param_3 optionally re-checks [0x21]/[0x27] and disarms +3. Callers 0xEDC5A, 0xEBC60, 0xEBDBC. Cluster of 1. [O] mechanics.
#### 0x000EAD56 `lp_req_set_flag3` — stub
r08 entry (0xBA794): sets +3 = param_2 and zeroes the u16 at +4. Cluster of 1. [O]
#### 0x000EAD60 `lp_pub_field_4` — stub
Publishes +4 to consumer 0xEAD8C via method 0xEAD90 (0xBAE5E family). Caller 0xDE0FE. Cluster of 1. [O]
#### 0x000EADA6 `lp_pred_flag2c` — logic
r08 entry (0xBAB50): refreshes via vmethod 0xEAFE4 and returns +0x2c != 0. Cluster of 1. [O]
#### 0x000EADBA `lp_pred_flag2c_10_30` — logic
r08 entry (0xBAB9C): refreshes via 0xEAFE4 and returns +0x2c ∈ {10, 30}. Cluster of 1. [O]
#### 0x000EADD6 `lp_pred_temp_ok` — logic
r08 entry (0xBAB8E): true when ctx(0xEAFE8)+0xc byte < +0x54, vmethod 0xEAFEC passes, and mode +0x6c ∈ {1,3}. Cluster of 1. [O]
#### 0x000EAE18 `lp_get_value_or_fault` — logic
r08 entry (0xBABE0): when +1 == 0 reports error 0x36b (code 0x6c) and returns 0; mode +3 == 4 returns the u16 from vmethod 0xEAFF4(0xEAFF0), else the short at +0x7c. Cluster of 1. [O]
#### 0x000EAE68 `lp_get_byte_or_vmethod` — data-accessor
Returns the byte at +0x10c, or the vmethod 0xEB000(0xEAFF0) value when mode +3 == 4. Callers 0xE6AE8, 0xE728A, 0xBAB3A. Cluster of 1. [O]
#### 0x000EAE82 `get_flag_fff8bda6` — stub
Three-instruction getter: returns RAM global byte DAT_fff8bda6. Caller 0xDD862. Cluster of 1. [O]
#### 0x000EAE8C `lp_get_speed5` — data-accessor
Returns a speed s16: mode +3 == 4 path checks vmethod 0xEB004(0xEAFF0) and reads 0xEB008(...,5); otherwise vmethod 0xEB00C on the sub-object at +0x108. Callers 0xDDB04, 0xEEDFC, 0x116DC4, 0xDD7B8, 0xEEEA8, 0xEECEC, 0x115566. Cluster of 1. [O] mechanics.
#### 0x000EAEC4 `lp_get_speed4` — data-accessor
Twin of 0xEAE8C with key 4 and sub-object slot +0x8c. Callers 0xBA928, 0xDE3E0, 0x116DC4, 0xDD7B8, 0x115566. Cluster of 1. [O]
#### 0x000EAEFC `lp_ramp_blend_calc` — library
Generic ramp/blend: ramps param_7*param_6/100, blends param_2..param_3 by param_4/100, and interpolates through vmethods 0xEB014/0xEB018/0xEB01C choosing the ramp-up branch when param_5 < ramp value, else the ramp-down branch. Callers 0x110CDC, 0x114E86 (r10). Cluster of 1. [O] mechanics.
#### 0x000EAFA0 `lp_pred_active` — logic
Predicate: (+0x98 || +0x129) || +0x10e. r08 entry 0xBAA3C. Cluster of 1. [O]
#### 0x000EAFC8 `lp_pred_98_129` — logic
Predicate: +0x98 || +0x129. Caller 0x117622 (r10). Cluster of 1. [O]
#### 0x000EB020 `lp_get_byte_10e` — data-accessor
Returns byte +0x10e, or the vmethod 0xEB178(0xEB174) value when mode +3 == 4. Callers 0xEB8A8, 0xEFB48, 0x115566. Cluster of 1. [O]
#### 0x000EB03A `lp_shift_phase_classify` — dispatch
r08 entry 0xBABAA: classifies the current state — mode vmethod 0xEB17C then a chain of condition vmethods (0xEB184 general, 0xEB188/0xEB18C/0xEB190 by mode 4/3/2) selecting class 2/4/6/8/9, finally 0xEB194(param, class, 0, param_3). Callees 0xEC182/0xEC20A/0xEC362/0xEC4D8/0xEC578/0xEC614. Cluster of 1. [O] mechanics, [I] class meanings.
#### 0x000EB118 `lp_class_from_bd73` — logic
Returns 4 when the range/gear code DAT_fff8bd73 == 9, else 0xEB194(&DAT_fff8bd14, code, 1, DAT_fff8bd71). Callers 0x113788, 0xEA28C, 0x110CDC, 0xBA90A. Cluster of 1. [O]
#### 0x000EB140 `lp_gear_group_get` — logic
Returns the gear-group of 0xEB158 with group 3 collapsed to 2. Callers 0xEB5A6, 0xEC956. Cluster of 1. [O]
#### 0x000EB158 `lp_gear_group_map` — dispatch
Maps the gear/range code DAT_fff8bd73: 3/4 -> 1, 5/6/7/9 -> 2, 8 -> 3, 1/2 -> 0, default reports error 0x36b (code 0xf7) and returns 0. Callers 0xEB140, 0xEC916. Cluster of 1. [O]
#### 0x000EB1E8 `obj_set_flag_byte` — stub
Three-instruction setter: writes 1 to object byte +0x2d. Cluster of 7 byte-identical-pattern members with differing offsets: 0xEB49C (+0x2d), 0xEB8F8 (+0x31), 0x101848/0x101BB6 (+0xc, r10), 0x12307C (+0x16), 0x13936C (+0x1e). Cluster of 7. [O]
#### 0x000EB1F0 `lp_pred_2_or_vt` — logic
r08 entry 0xBAB5E: returns 1 when +2 == 0 or vmethod 0xEB3C8 passes, else DAT_000EB3CC(param). Cluster of 1. [O]
#### 0x000EB21C `lp_lookup_by_key` — logic
Key lookup: compares param_2 against object bytes +0x263 (-> +0x254), +0x11b (-> +0x10c) and three vmethod values on 0xEB3D0, returning the matched value byte (default 1). r08 entry 0xBABC8. Cluster of 1. [O] mechanics, [I].
#### 0x000EB288 `lp_pred_lockup_ok` — logic
Latches +0x26: true when +1 and DAT_fff8bef0 are set and (vmethod 0xEB3EC(+0x8c) or DAT_000EB3F4(0xEB3F0) or the previous +0x26) — feeds the 0xE58D0 predicate as documented in the line-pressure caller list. Caller 0xDE0F6. Cluster of 1. [O] mechanics.
#### 0x000EB2D0 `lp_pred_gear_ok` — logic
r08 entry 0xBAB1E: requires +1 and gear +2 ∈ {1,2}, gate 0xEC182(param, +0x5f, 1, +0x5d); maps +2 (1->3, 2->4, else 4) into +0x2e (1-4 -> 0-3) and checks the pair table vmethod 0xEB3FC(**(0xEB3F8), +0xa != 1, idx). Cluster of 1. [O] mechanics.
#### 0x000EB348 `lp_state_init` — logic
r08 entry 0xBAD28: resets the sub-objects at +0x250/+0x108/+0x1d4/+0x8c (vmethods 0xEB400/0xEB404), the consumers 0xEB3D0/0xEB40C and RAM blocks DAT_fff8bf64/fff8be1c/fff8bee8/fff8bda0 (mode +3 != 4 guard); also calls r10 0x10FB72 (the spec-11 phase-engine stepper), 0x10FA7E, 0x112888, 0x112916. Cluster of 1. [O] mechanics.
#### 0x000EB420 `lp_pred_speed_ok2` — logic
Speed gate: 0xEC05C value vs curve 0xEB67C(DAT_fff8bd80); when below, still passes if DAT_000EB684 <= **(0xEB680)+0x170 and DAT_fff8be21 is set. Caller 0xEDE82. Cluster of 1. [O]
#### 0x000EB476 `lp_calc_clamp_5000` — logic
Computes +0x18 = clamp(vmethod 0xEB688(+0x16 * 10), 0, 5000) via 0xEB68C. Caller 0xEBDBC. Cluster of 1. [O]
#### 0x000EB4A4 `lp_get_short_b0` — stub
Returns the s16 behind **(0xEB680 + 0xb0). Callers 0xEDE1C, 0x117F48, 0xEDBEA, 0xEE098. Cluster of 1. [O]
#### 0x000EB556 `lp_get_torque_10a` — data-accessor
Returns the short at +0x10a, or (mode +3 == 4) the gated vmethod 0xEB6B4(0xEB6AC) value. Member 0xEB57E reads the slot at +0x8e instead. Callers 0x117032, 0x116DC4. Cluster of 2. [O]
#### 0x000EB5A6 `lp_shift_allowed_pred` — logic
Shift-permission predicate (caller 0xDE190): gate chain vmethod 0xEB6B8(param, gears), global 0xEB6C0(0xEB6BC), the gear-pair table 0xEB6C8(slot (0xEB680+0x3c+chan*4), gear+1, gear+1) returning class 1-5, combined with the gear group from 0xEB140. Also uses 0xE590C and 0xEB140/0xB6CBC. Cluster of 1. [O] mechanics, [I] class semantics.
#### 0x000EB640 `lp_pred_enabled` — logic
Requires DAT_fff8bef0 set (else error 0x36b code 0x31, return 0), then vmethods 0xEB8C4(+0x8c) and 0xEB8CC(0xEB8C8) must both pass; feeds the 0xE58D0 predicate (its documented caller). Caller 0x1138FC (r10). Cluster of 1. [O]
#### 0x000E93EC `lp_set_flag_e9494` — stub
Four-instruction setter: writes 1 to the byte behind PTR_DAT_000E9494. Caller 0xEBDBC (line-pressure controller). Cluster of 1. [O]
#### 0x000E93F4 `lp_call_vt_9498` — stub
Four-instruction wrapper: calls PTR_LAB_000E949C on module object 0xE9498. Caller 0xEB348 (state init). Cluster of 1. [O]

## Unit summary

Unit 30 (r0c_app slice 0x000E513C-0x000EB640): 220 clusters, 258 named functions (reps + all members), completed by continuation after 22 clusters.

Counts by kind (names rows, 258 total):
- logic: 140
- stub: 68
- data-accessor: 38
- dispatch: 9
- library: 3
- irq: 0, entry: 0
- unknown: 0 — no function resisted a mechanics-level reading; low-confidence semantic names carry [I] markers in the catalog instead (e.g. `shift_m2_*` machine-2 family, `lp_shift_phase_classify`, `shift_type_*`).

Module map established for this slice:
- 0xE513C-0xE5A20 pressure-request subsystem (spec 12 §2a) — finished by continuation (0xE590C-0xE5A20: predicates, variant/cal staging, error stubs).
- 0xE5E48-0xE7FEE: second instance of the spec 11 §3 shift phase engine (tick +0x12, target +0x20, sequence table step*3+type) plus its calibration accessors, safe-slot readers and ramp helpers; driven by task 0xDE0D8 (shift-module area 0xDE000-0xE3100).
- 0xE8050-0xE88xx: shift coordination (type tables 0x634da/0x63534 next to the spec-11 sequence table 0x63555), event-mark/aging counters, level searches.
- 0xE88FA-0xE9340: park module core (spec 12 §1.2 — 0xE88FA output routine, 0xE89D0 output write, 0xE8B56 release condition) and the diagnostic actuator-test sequencer (module 0x2c3).
- 0xE940C-0xE9C4A: r08 interface getters/wrappers plus the two small state machines (m2, m2b) of the 0xDE0D8 task.
- 0xE9CE0-0xEB640: line-pressure strategy (spec 11 §3a — confirmed field-for-field: 0xE9CE0 update, 0xEA806 offer, 0xEA774 max, 0xEA84E forced, 0xE9FF0 shift demand) and its r08 predicate/getter API (slots 0xBAB1E-0xBAD28).

Unknown entries: none by address.

Cross-region leads:
- r10_app 0x10F7D0-0x111400 (spec 11 phase engine) is the twin of this unit's 0xE5E48-0xE73AC machine; cluster members 0x10FF9A (target getter) and 0x111FE4 (called by phase entry 0x10FFBE) land in that region — cross-check naming there.
- This unit calls into r10_app: 0xE7126 -> 0x10FB52, 0xEB348 -> 0x10FB72 (spec-11 stepper), 0xEA28C -> 0x104D58, 0xEA89C -> 0x104D4C.
- The 0xEB640-0xEE000 controllers (0xEBC60, 0xEBB6C, 0xEB9EC/0xEBA6E, 0xEBDBC, 0xED92E, 0xEDC5A, 0xEDE82) are this unit's heaviest callers — that unit should cross-check the `shift_`/`lp_` names established here.
- Duplicated helper copies of this unit's clusters live across r0c/r10/r13 regions (0xF7250-0x14975E, 22-copy +0x22 getter; 7-copy flag setters) — other units' sweeps will meet them again.
