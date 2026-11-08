# Unit 20 (r08_app)

Region 0x000B4372-0x000B83C2 of the application flash (r08_app), continuing directly
after unit 19. This band sits after the gear-pair helpers and contains a stateful
request/response protocol object family (0x000B4372+), then a large run of small
accessor/thunk functions.

#### 0x000B4372 `seq_state_publish_and_reset` — logic
Completion step of a 1..5-step request sequence: when +0x94 (step) is 1..5 it copies the step
number to +0xa9, publishes the two u16 results (+0x90/+0x92) and five status bytes (+0x89,
+0x8b..+0x8e) through seven indirect setters `PTR_FUN_000b45a8..d8(obj, step-1, value)`, then
clears all step state, restarts the timeout from table +0x3c/+0x40 (state +0x88 -> 2) and
finishes via `PTR_FUN_000b45dc(+0x78)`. If +0x88 was 0xB it drops to state 0 instead. On an
invalid step it logs event 0x254/0xea (module 6) and calls the abort setter 0x000B455A. Called
only from 0x000B49FC. 1 member. [I]

#### 0x000B455A `seq_abort_error_set` — logic
Abort setter for the sequence object: forces sub-states +0x88=0xB, +0x74=8, +0x7e=9, +0x51=6
(error codes). Called from ten places across the family (0x000B4372, 0x000B45E0, 0x000B4D0C,
0x000B3B4C, 0x000B4046 and r0c handlers 0x000CCC50/0x000CCEEC/0x000CC9EE/0x000CDD6C/0x000CD124).
1 member. [O]

#### 0x000B4574 `seq_channel_arm` — logic
Arms the sequence: state +0x88 = 1, reloads timeout +0x84 from global table +0x3c
(`PTR_DAT_000b45a0`), clears +0x74/+0x7e and sets handshake +0x3a = 1. Called from interface
thunk 0x000AEA54. 1 member. [O]

#### 0x000B45E0 `seq_cyclic_main` — logic
Cyclic driver (320 instr): validates the +0x88/+0x94/+0x89/+0x74/+0x7e/+0x51 state consistency,
logging 0x254/0xdb and aborting via 0x000B455A on contradiction; runs the readiness check
0x000B4B02 and advances the phase; maintains handshake bytes +0x9c/+0x9d/+0x9e/+0xa0 against a
global request object `PTR_DAT_000b4974`; ticks sub-objects (+0x34/+0x78/+0x70) and 0x000B4D0C/
0x000B4F2E; finally loads per-step configuration tables (8 setters, index +0xac-1) into
+0xad..+0xb8. Called from r0c task 0x000CA3A6. 1 member. [I]

#### 0x000B48F8 `seq_state_clear_or_release` — logic
If state +0x88 < 4 it resets it to 0, otherwise it sets the "release" states (+0x88=10,
+0x74=7, +0x7e=8, +0x51=5); always clears handshake +0x3a. Called from thunk 0x000AEA4C
(callers 0x000D140C, 0x000CFD1A in r0c/r14). 1 member. [O]

#### 0x000B492C `seq_reset_timers_subobjs` — logic
Resets sequence scratch: one indirect reset on +0x9a, two timer clears on +0x84/+0x86, and
indirect resets of sub-objects +0x34/+0x78/+0x40. Called from init dispatcher 0x000A2A60.
1 member. [I]

#### 0x000B49FC `seq_request_dispatch_entry` — logic
Entry point with no static callers (reached via interface table): checks the global readiness
bytes (+0x2b==+0x2c, +0x21==+0x27, +0x24==+0x27) plus two indirect gates; when armed and
param_2==0 it collects four values via getters at vtable +0x68/+0x6c/+0x70/+0x74 of the global
at `PTR_DAT_000b4bf0` and builds the request with 0x000B3C80, else validates param_2 via
0x000B3F02. Ends by launching 0x000B4372 or resetting via 0x000B4046. Callee of many r0c/r10
callers through 0x000B6C8A-style wrapper chains. 1 member. [I]

#### 0x000B4AF0 `seq_req_type_is_1_or_2` — data-accessor
Returns 1 when mode byte +0xab is 1 or 2. Called from r0c handlers 0x000CD8A0/0x000CC89C/
0x000CD870. 1 member. [O]

#### 0x000B4B02 `seq_time_window_active` — logic
Readiness check for 0x000B45E0: true when state +0x88 is 2..0xA, global `PTR_DAT_000b4bf8` is
0, +0x82 is 0, and an indirect compare of a threshold at `PTR_FUN_000b4c04` vs a value at
`PTR_FUN_000b4c08(global+8)` says the measured window is below the limit. 1 member. [I]

#### 0x000B4B56 `seq_preconditions_ok` — logic
Six-way gate: an indirect flag (arg 1), an inverse flag, a third flag plus `PTR_DAT_000b4c24`,
bits 0xC2/199/0xEB/0xED of `PTR_DAT_000b4c30`, and a final compare with `PTR_DAT_000b4c34`.
Returns 1 only when all pass. Called from 0x000B3F02/0x000B3C80/0x000CD256 (r0c). 1 member. [I]

#### 0x000B4C3C `seq_window_flag_update` — logic
Hysteresis window evaluation: validates channel param_2 in 1..5 and index param_4 in 1..2
(else event 0x254/0xae), then stores pass/fail into the per-channel flag array
`PTR_DAT_000b4d48[param_2*3 + param_4]` using either a mode-specific window (when
`table[+3]==1`) with bounds param_5/param_6 or a slack window (param_5, param_6-param_7).
Called from the two attempt starters 0x000B5086/0x000B54E4. 1 member. [I]

#### 0x000B4D0C `seq_phase_state_step` — dispatch
The phase engine (214 instr): a do/while that runs states +0x88 0..0xB to fixpoint — state 0
clears timers; 1..3 poll gate `PTR_FUN_000b5008(.,1)`; 2 waits for both timer words 0; 4 loads
timeout from calibration +0x44 and goes to 5; 5 computes a scaled timeout (`*(+0x96 + sub_obj
result + cal lookup)/10`, increments retry counter +0x8d capped at -1) and goes to 6; 6 samples
a result word from sub-object +0x70 (channel 1) or +0x78 (channel 2) into +0x84 and notifies via
`PTR_FUN_000b5024`; 7 -> 8; 8..0xB just clear the timer. Unknown states log 0x254/0xc and abort.
Called from 0x000B45E0. 1 member. [I]

#### 0x000B4F2E `seq_result_word_capture` — logic
When the global phase `DAT_fff89b80` is 7, captures a result word from sub-object +0x70
(channel 1) or +0x78 (channel 2) into +0x98, else stores 0. Called from 0x000B45E0. 1 member. [O]

#### 0x000B4F64 `seq_wait_timeout_check` — logic
Wait/abort evaluator called from 0x000B4372 when flag +0xa8 is set: increments retry counter
+0x8c, fetches a per-step limit through `PTR_FUN_000b503c/48` and validates it via
`PTR_FUN_000b5040/44`; depending on mode +0xa8 (1 = retry limit exceeded, 2 or 3 = immediate)
it cancels the step (indirect cancel on the step table, clears the result fields and sets
completion flag +0xa5 = 1). 1 member. [I]

#### 0x000B5086 `seq_attempt_start_ch1` — logic
Channel-1 attempt starter (479 instr): selects slot by param_3 (0->offset 1/period 2, 1->3/1,
2->5/0), validates step param_2 in 1..5, reads the step table via `PTR_FUN_000b52e8`, resolves
a per-step config through the calibration dispatch (`PTR_LAB_000b52f4` index table with bounds
check event), computes the start window, applies modifiers (+0x3c or timeout zero) from the
indirect step-status getters, publishes failure results through the seven setters on error
(event 0x254/0xf7, module 9), then updates the window flag via 0x000B4C3C. Called from
0x000B3C80. 1 member. [I]

#### 0x000B54E4 `seq_attempt_start_ch2` — logic
Channel-2 twin of 0x000B5086 (474 instr): slot offsets 2/4/6 by param_3, error event
0x254/0xa6 (module 10), same seven-setter failure publish and 0x000B4C3C window update with
channel index 2. Called from 0x000B3C80. 1 member. [I]

#### 0x000B5996 `seq_pair_read_sel_u16` — data-accessor
Returns *param_1[0] or *param_1[1] (a two-entry u16 pointer pair selector). Called from
0x000B4046. 1 member. [O]

#### 0x000B59F8 `monit_timer_enable_disable` — logic
Enable/disable of a small monitoring timer object: enable (param_2==1) calls 0x000B633C, latches
param_3 at +4, copies a global byte to +5, sets run flag +0xc=1 and samples +0x19 into +0x18;
disable calls 0x000B68D0, clears +0xc, sets the period word at +0 to 20000 and restarts via
0x000B633C. Called from thunk 0x000AEB9A. 1 member. [I]

#### 0x000B5A38 `monit_latch_flag_on_enable` — logic
If the monitor run flag +0xc is set, latches +0x1a = 1. Called from thunk 0x000AEBA4
(callers 0x000DD280 etc.). 1 member. [O]

#### 0x000B5A46 `monit_gate_check_triplet` — logic
Builds a triplet of calibration-factored samples (three indirect getters gated by
`PTR_FUN_000b5c00`), evaluates them with `PTR_FUN_000b5c10` against calibration bytes +0x21/
+0x27 of `PTR_DAT_000b5bf4`, then chains the deeper gates 0x000B5DD2 and 0x000B5FCC(p,
cal[0x2b], cal[0x2c], mode, 5). Called from thunk 0x000AEBAC. 1 member. [I]

#### 0x000B5B0C `monit_state_not_idle` — data-accessor
Returns 1 when state byte +0xc is neither 0 nor 1 (i.e. a non-idle/active state), else 0.
Called from thunk 0x000AEB8C. 1 member. [O]

#### 0x000B5B1E `monit_is_enabled` — data-accessor
Returns run flag (+0xc != 0). 2 members (copy at 0x001289EC in r10_app). [O]

#### 0x000B5B26 `monit_is_disabled` — data-accessor
Returns the inverse (+0xc == 0). 3 members (0x000B5B26, 0x000B5B2E, 0x000D9650). [O]

#### 0x000B5B36 `monit_value_in_cal_window` — logic
Gate: only when 0x000B5F8C passes; reads a value via one of two getters
(`PTR_FUN_000b5c18/1c`), resolves a calibration cell through the 4-entry dispatch table
`PTR_LAB_000b5c20` (bounds-checked), scales it via `PTR_FUN_000b5c38`, and returns 1 when the
result is below calibration byte [0x96] and that byte is below a global limit. Called from
0x000B5FCC and 0x000A23F4. 1 member. [I]

#### 0x000B5C40 `monit_engagement_dual_gate` — logic
Two independent gates returning 1 if either holds: path A (param_2 in 1..3, param_3==1,
param_4==1) requires global `PTR_DAT_000b5e88`, object flag +6 and table +0x34 lookup of
param_2-1 == 1; path B (param_2 in 1,2,5 with param_3==2, param_4==2) mirrors it on flag +7
and table +0x38. Called from 0x000B5FCC and 0x000A23F4. 1 member. [I]

#### 0x000B5D78 `monit_result_register_subs` — logic
Init-time registration: passes offsets +0xe, +0x10, +0x12 and +0x20 of the monitor object to
a registrar via `PTR_FUN_000b5ea4`/`DAT_000b5ea8` (subscribe result fields). Called from init
dispatcher 0x000A2A60. 1 member. [I]

#### 0x000B5DA6 `monit_slip_value_calc` — logic
Computes `scale2 - (scaled(sample of *PTR_DAT_000b5eac) * *PTR_DAT_000b5eb0) / 1000` through
`PTR_FUN_000b5eb4/ebc` — a slip/difference value in 1/1000 units. Called from 0x000B62D8
(the monitor cyclic). 1 member. [I]

#### 0x000B5DD2 `monit_meas_below_cal_limit` — logic
Returns (u16 at +0x10) < calibration cell resolved through the 4-entry dispatch table
`PTR_LAB_000b5ec0` (bounds-checked, event 0x25A/0xF on bad index); requires channel index
check 0x000B5F8C, else logs event 0x25A with subcodes (3, 0x9d). Called from 0x000B5A46.
1 member. [I]

#### 0x000B5ED8 `monit_meas_above_cal_limit` — logic
Mirror of 0x000B5DD2: returns calibration cell < (u16 at +0x10), dispatch table
`PTR_LAB_000b5fac`, event subcodes (4, 199) on failure. Called from 0x000B61C8/0x000B6366.
1 member. [I]

#### 0x000B5F8C `monit_channel_index_valid` — library
Pure check: param_2 in {1,2,3,5}. Used as the guard by all limit/gate functions of the
monitor family. 1 member. [O]

#### 0x000B5FCC `monit_release_gates` — logic
Bitmask-gated release-condition evaluator (param_5 bits 0x1/0x2/0x4/0x8/0x10): bit 0 checks
two global flags plus 0x000B5F8C, 0x000B5C40, a gate pair, a speed-vs-+0x20 window
(`*global+4 * 100`) and two calibration bytes; bit 2 compares `*global+0x20 < *PTR_DAT_000b61a8`
and clears flag +8 on failure; bit 4 requires +0x1a or 0x000B5B36; bit 8 checks a ramp window
around +0x14 with calibrations +0x94/+0x9c and table lookups +0x18/+0x30/+0xc/+0x14; bit 0x10
clears +8. Called by phase drivers 0x000B6366/0x000B662C/0x000B6710/0x000B67C4 and 0x000B5A46.
1 member. [I]

#### 0x000B61C8 `monit_shift_release_check` — logic
Per-channel release check: pulls three values from `PTR_DAT_000b6410` getters and computes a
target index (<6) with `PTR_FUN_000b6424`; requires 0x000B5ED8 to fail or the target to match
object +4 or `*PTR_DAT_000b6428 != channel`, then accepts via `PTR_FUN_000b642c(obj, target)
== 1`. 1 member. [I]

#### 0x000B625E `monit_run_gates_eval_stub` — stub
One-instruction variant/entry of 0x000B6260 (same decompiled body; tail-jump into the logic).
1 member. [I]

#### 0x000B6260 `monit_run_gates_eval` — logic
Top gate evaluation: when calibration bytes [0x27] in 1..2, [0x2b]==[0x2c]==1 and an indirect
gate is clear, tries channel 2 then channel 1 via 0x000B61C8; stores 1 into +9 if either
succeeds. Also calls 0x000CA624. 1 member. [I]

#### 0x000B62D8 `monit_cyclic_watchdog` — logic
Cyclic monitor step: when the global threshold at table+0x1c is below `*PTR_DAT_000b6440`
sets flags +6/+7 (word idx 3/3.5); clears param_1[8] on two indirect gates; computes the slip
value 0x000B5DA6 into param_1[5]; and when state +6(word3) is neither 0 nor 1 and the speed at
table+0 exceeds param_1[9] it restarts the 20000-period timer and clears the state. 1 member. [I]

#### 0x000B633C `monit_clear_results` — logic
Clears the monitor result block: +0x14 word = 0xFFFF, bytes +0x1a/+8/+0x1b/+4/+5/+0x18 and
words +0x1c/+0x1e = 0. Called from 0x000B59F8 (enable path). 1 member. [O]

#### 0x000B635A `monit_timer_reset` — stub
Period word at +0 = 20000, byte +0xE = 0. 1 member. [O]

#### 0x000B6366 `monit_phase_check_start` — logic
Phase 2 entry: gate 0x000B5FCC(...,5), then 0x000B5ED8 must pass, latches
`*PTR_DAT_000b6454` into param_1[10] and re-checks gates with mask 10; on success state=2 and
clears [9]; else resets (state 0, period 20000). 1 member. [I]

#### 0x000B63D0 `monit_phase_target_setup` — logic
Phase 3 setup (237 instr): resolves five calibration cells through the 4-entry dispatch table
`PTR_LAB_000b6458` (bounds-checked event), computes a target `*param_1` from a scaled cell
(cell+0x24 with `PTR_FUN_000b66f0/f4`), adds a second table value (cell+0xc), latches byte
[0xe] from cell+4, computes a slope `[0xf] = -table(+0xc) scaled *100 / [0xe]` via
`PTR_FUN_000b6700`, registers a ramp callback (`**(param_1+0x1a)+0x14` vtable call with
&LAB_00004e20) and sets state=3. 1 member. [I]

#### 0x000B662C `monit_phase_ramp_run` — logic
Phase 4: gates mask 0xF; runs the ramp via vtable+0x20 (object at +0x1a) storing the remaining
period; when [0xe] < [7] re-derives the slope from cal cell+8 scaled *100, reloads [0xe] from
table +0x28 and advances state=4; failure resets (state 0, 20000). 1 member. [I]

#### 0x000B6710 `monit_phase_ramp_check` — logic
Phase 5: gates mask 0xF; on gate failure latches a fault: if byte +0x1b < [7] sets [0xb] =
`PTR_FUN_000b69ac([7]) - +0x1b`, sets fault flag +4=1, else resets; on pass, runs the ramp and
when table+0x10 < [5] or table+0x28 < [7] latches the same fault, zeroes [7], fires the
vtable+0x14 callback and sets state=5. 1 member. [I]

#### 0x000B67C4 `monit_phase_ramp_extend` — logic
Phase 6: gates mask 0x13 plus the inverse of `PTR_FUN_000b69b4`; on pass computes slope
`PTR_FUN_000b69cc(cell+0x10, cal[0x19]) * 100` into [0xf], runs the ramp (vtable+0x20) and adds
`PTR_FUN_000b69cc(cell+0x14, [7])` to the period; failure resets. 1 member. [I]

#### 0x000B68D0 `monit_stop_evaluate` — logic
Disable-path evaluator from 0x000B59F8: when the active channel fails 0x000B5F8C it logs event
0x25A (subcodes 2,5,0x92) and clears per-mode flags; else (when the limit
`*PTR_DAT_000b69d4 <= table+0x24`) computes the final result word (+2) from cal cell+0x1c
scaled by `PTR_FUN_000b69d0`, publishing via either the `PTR_FUN_000b6a74/a78/a7c` setter
chain (with a re-read) or the `PTR_FUN_000b69e0/e4/e8` chain; clears activity flags +8/+6/+7.
1 member. [I]

#### 0x000B6A88 `flag_get_b6c04` — data-accessor
Returns global flag `*PTR_DAT_000b6c04`. Called from shift-module code 0x00106356/0x00105752
and r0c 0x000D64D2/0x000D4C52. 1 member. [I]

#### 0x000B6A90 `flag_get_b6c08` — data-accessor
Returns `*PTR_DAT_000b6c08`; consumers in r0c/r10/r14 (0x000E88FA, 0x000DC1E2, 0x0010834C...).
1 member. [I]

#### 0x000B6A98 `state_obj_flag_get_a` — data-accessor
Returns one boolean method of the global object `*PTR_DAT_000b6c0c` via
`PTR_FUN_000b6c10`; consumers 0x0010850A/0x000DC1E2/0x0010D130. 4 members
(0x000B6A98/0x000B6B50/0x000B6B5E/0x000B6B6C = four different getter slots of the same
object, all called from 0x000CC5C6 or shift code). [I]

#### 0x000B6AA6 `flag_get_b6c14` — data-accessor
Returns `*DAT_000b6c14`; called from 0x000B62D8, 0x000CA176, 0x000F2F28, 0x000C9560, 0x000E8CF6.
1 member. [I]

#### 0x000B6AAE `flag_get_b6c18` — data-accessor
Returns `*DAT_000b6c18`; called only from 0x000EBA6E. 1 member. [I]

#### 0x000B6AB6 `state_obj_flag_get_b` — data-accessor
Indirect getter on global `*PTR_DAT_000b6c1c` via `PTR_FUN_000b6c20` (callee 0x000CFE62);
called from 0x000B62D8, 0x000CE48C, 0x000CE62A, 0x000E8CF6. 1 member. [I]

#### 0x000B6AC4 `flag_is_mode2_b6c24` — data-accessor
Returns `*PTR_DAT_000b6c24 == 2` (a mode selector check); consumers 0x000EDE82/0x000DDE78/
0x000DDFD8. 1 member. [I]

#### 0x000B6ACE `b6c28_mode_in_1_2` — data-accessor
Returns 1 when byte [1] of the global struct `*PTR_DAT_000b6c28` is 1 or 2. Consumers
0x000EDE82/0x000DD862/0x000DC4C4/0x000DD7B8. 1 member. [I]

#### 0x000B6AE2 `b6c28_short_select` — data-accessor
Returns short at +4 of `*PTR_DAT_000b6c28` when param_2 equals byte [6], else the short at +2.
1 member. [I]

#### 0x000B6AF8 `b6c28_differs_while_active` — data-accessor
Returns param_2 != byte [6] but only when mode [1] is 1 or 2 and [6] != 0. Called from
0x000DC4C4. 1 member. [I]

#### 0x000B6B1E `flag_b6c2c_is_3` — data-accessor
Returns `*PTR_DAT_000b6c2c == 3` (state==3 check); consumer 0x00117B4A. 1 member. [I]

#### 0x000B6B28 `obj_b6c30_call_m0` — data-accessor
Invokes method slot `DAT_000b6c34` on global object `*PTR_DAT_000b6c30` (callee 0x000D02EC)
and returns its byte result. Consumer 0x00117B4A. 1 member. [I]

#### 0x000B6B36 `flag_b6c30_is_3` — data-accessor
Returns `*PTR_DAT_000b6c30 == 3`; consumers 0x000EE38C/0x000EC956. 1 member. [I]

#### 0x000B6B40 `flag_get_b6c38` — data-accessor
Returns `*DAT_000b6c38`; consumer 0x000EE46E. 1 member. [I]

#### 0x000B6B48 `obj_b6c0c_call_m1` — data-accessor
Invokes method slot `PTR_FUN_000b6c3c` on the global object `*PTR_DAT_000b6c0c` (callee
0x000D11A0). Called from 0x000CC5C6. 1 member. [I]

#### 0x000B6B7A `flag_get_b6c4c` — data-accessor
Returns the global byte at `*_LAB_000b6c4c`; consumer 0x000B31D8. 1 member. [I]

#### 0x000B6B82 `flag_get_b6c50_nz` — data-accessor
Returns `*PTR_DAT_000b6c50 != 0`; seven consumers across r0c/r10 (0x000DFD2C, 0x000CF48C,
0x000DD2C4, 0x000D9448...). 1 member. [I]

#### 0x000B6B8C `obj_b6c54_flag_get_a` — data-accessor
One of three boolean getter slots (`PTR_FUN_000b6c58/5c/60`) on global object
`*PTR_DAT_000b6c54`; this one via 0xb6c58 (callee 0x000D0C88). 3 members
(0x000B6B8C a / 0x000B6B9A b / 0x000B6BA8 c). [I]

#### 0x000B6BB6 `flag_get_s8_b6c64` — data-accessor
Returns the signed byte `*PTR_DAT_000b6c64`; consumers 0x000EDE82/0x00117438. 1 member. [I]

#### 0x000B6BBC `obj_b6c64_call5` — data-accessor
Five-argument method invocation on global `*PTR_DAT_000b6c64` via `PTR_FUN_000b6c68`
(callee 0x000D165A); consumer 0x000DE190. 1 member. [I]

#### 0x000B6BDC `obj_b6c04_call_a` — data-accessor
Invokes method slot `PTR_FUN_000b6c6c` on global `*PTR_DAT_000b6c04` (callee 0x000CFF0E);
called from init 0x000A2F0C. 4 members — invoker variants 0x000B6BEC (slot 0xb6c74 ->
0x000D01C4), 0x000B6BF4 (slot 0xb6c78 -> 0x000D01E6), 0x000B6BFC (slot 0xb6c7c -> 0x000CFEFE,
called from 0x000BAEB4). [I]

#### 0x000B6BE4 `obj_b6c04_call_set` — data-accessor
One-argument method invocation (slot `PTR_FUN_000b6c70`, callee 0x000CFFDE) on global
`*PTR_DAT_000b6c04`; called from init 0x000A2F0C. 1 member. [I]

#### 0x000B6C80 `tbl_read_s8` — library
Reads the signed byte at `param_1 + 4 + index` from a {u32 n; s8 t[n]} table layout.
1 member. [O]

#### 0x000B6C8A `tbl_read_u8` — library
Byte-array calibration getter `t[i]` from {u32 n; u8 t[n]} — the getter cited in spec 09
shift-and-lockup ("t[i], index check (0x121432, 0xB6C8A)"). Called from 75+ sites across the
whole application. 3 members (copies 0x00151DAC and 0x001552D0 in the r14 region).
[O] (spec 09 calibration access)

#### 0x000B6CA8 `tbl_read_u32` — library
u32 element read at `param_1 + 4 + index*4` ({u32 n; u32 t[n]} layout). Consumers
0x0010BC24/0x001088DE/0x000DE688 etc. 1 member. [O]

#### 0x000B6CB2 `tbl_read_s16` — library
s16 element read at `param_1 + 4 + index*2` ({u32 n; s16 t[n]} layout); consumer 0x000E0440.
1 member. [O]

#### 0x000B6CBC `tbl2_read_u8` — library
2-D byte table read: {u32 n1; u32 n2; u8 t[n1][n2]} at +8, indexes bounds-checked
(returns 0 outside). 21 consumers across shift/clutch code (0x000B043C/0x000B04B8/0x000E51C8...).
1 member. [O]

#### 0x000B6CE8 `tbl2_read_s16` — library
2-D s16 read (stride *2) of the same {n1; n2; data} layout with bounds check -> 0. 2 members
(0x000B6CE8, 0x000B6D14); 16+ consumers. [O]

#### 0x000B6D40 `tbl2_read_u32` — library
2-D u32 read (stride *4) of the same layout with bounds check -> 0. 1 member. [O]

#### 0x000B6D6C `tbl_curve_u8_interp` — library
1-D u8 curve interpolation over byte breakpoints: {u32 n; u8 x[n]; u8 y[n]} with x at +4,
y at +4+n; linear interpolation in float between bracketing breakpoints, clamped at the ends.
58 consumers. 1 member. [O]

#### 0x000B6E10 `tbl_curve_s16_interp` — library
1-D s16 curve interpolation ({u32 n; s16 x[n]; s16 y[n]}, x at +4, y at +4+2n), linear in
float, clamped. 118 consumers. 1 member. [O]

#### 0x000B6EAE `tbl_curve_u16_interp` — library
1-D u16 curve interpolation ({u32 n; u16 x[n]; u16 y[n]}), linear in float, clamped.
68 consumers. 1 member. [O]

#### 0x000B6F5C `tbl_curve_f32_interp` — library
1-D float curve interpolation over float breakpoints ({u32 n; f32 x[n]; f32 y[n]}, x at +4,
y at +4+4n); linear in float, clamped at both ends. 27 consumers (0x00105B36, 0x001064C6,
0x0010B2EA...). 1 member. [O]

#### 0x000B6FD6 `tbl2_curve_u8_interp` — library
2-D u8 map with bilinear interpolation: {u32 n1; u32 n2; u8 x1[n1]; u8 x2[n2]; u8 y[n1][n2]}
(byte axes); clamps at the edges (divisor 1 at ends). 31 consumers. 1 member. [O]

#### 0x000B7164 `tbl2_curve_s16_interp` — library
2-D s16 map with bilinear interpolation (s16 axes and values, float math), clamped at edges.
102 consumers. 1 member. [O]

#### 0x000B72D8 `tbl2_curve_u16_interp` — library
2-D u16 map with bilinear interpolation (u16 axes and values, float math), clamped at edges.
13 consumers. 1 member. [O]

#### 0x000B7472 `tbl2_curve_f32_interp` — library
2-D float map with bilinear interpolation (f32 axes and values), clamped at edges. 28
consumers. 1 member. [O]

#### 0x000B7594 `lib_default_if_zero` — library
Returns param_1, or the result of `PTR_FUN_000b76e0(1)` (a default-value getter, callee chain
into 0x0009AA34) when param_1 == 0. 5 members: 0x000B7594, 0x000D7C24, 0x000D7C4E, 0x000F644C,
0x000F6566 (spread across regions — same helper duplicated). [O]

#### 0x000B75A6 `event_end_then_flush` — logic
Ends an event/trace record via 0x000B7620 then calls the flush/dispatch pointer
`**PTR_PTR_000b76e4` (callee 0x0007A914 chain). Called from 0x000B75B4 and eight others.
1 member. [I]

#### 0x000B75B4 `event_log_id1c_6bytes` — logic
Compact event logger: passes event id 0x1C plus six payload bytes (packed from three u16
arguments) through `PTR_FUN_000b76ec(PTR_DAT_000b76e8, ...)`, then closes via
0x000B75A6(param, 7). 99 callers across the whole application — this is the module's standard
"trace point" call. 1 member. [O]

#### 0x000B7620 `event_close_code_dispatch` — logic
Maps a close reason 1..7 to codes 0x66/0x67/0x68 (others log event 0x1C with subcode 1 and
use 0x65) and dispatches through the jump-table pointer `PTR_PTR_000b76f0`. Called from
0x000B75A6. 1 member. [I]

#### 0x000B767E `snapbuf_write_record18` — logic
When channel flag `PTR_FUN_000b76f8` is 1: reads the record index (cap 9) via
`PTR_FUN_000b7700`, writes 18 (0x12) bytes from param_2 into slot `idx*0x12 + i` of buffer
`PTR_DAT_000b7704` via `PTR_FUN_000b7708`, then increments the counter via
`PTR_FUN_000b770c`. Called from 0x000B79E4. 1 member. [I]

#### 0x000B7710 `lib_eq_1` — library
Returns param_1 == 1. 1 member. [O]

#### 0x000B7718 `lib_ne_0` — library
Returns param_1 != 0. 1 member. [O]

#### 0x000B7720 `lib_s8_of_u8` — library
Clamps a byte to signed range (returns 0x7F when >= 0x80, else the signed value). 1 member. [O]

#### 0x000B772C `lib_clamp_u8` — library
Clamps param_1 (byte domain) to [param_2, param_3]. 4 members (0x000B772C, 0x000B7786,
0x00154956, 0x0015509E). [O]

#### 0x000B7746 `lib_s8_clamp_lo0` — library
Clamps a signed char to >= 0 (saturation at zero). 2 members (0x000B7746, 0x00154986).
43 callers. [O]

#### 0x000B7752 `lib_clamp_s8_3` — library
Clamps param_1 (char) to [param_2, param_3]. 1 member. [O]

#### 0x000B7768 `lib_s16_of_u16` — library
Clamps a u16 to positive s16 range: returns 0x7FFF when >= 0x8000. 2 members
(0x000B7768, 0x00121C86). 64 callers. [O]

#### 0x000B7776 `lib_u8_of_u16` — library
Clamps a u16 to 0..0xFF (returns 0xFF when >= 0x100). 2 members (0x000B7776, 0x00121CC6).
29 callers. [O]

#### 0x000B77A0 `lib_s16_abs` — library
Absolute value of s16 with -0x8000 mapped to 0x7FFF. 33 callers. 1 member. [O]

#### 0x000B77BC `lib_s16_clamp_lo0` — library
Clamps an s16 to >= 0. 2 members (0x000B77BC, 0x00121C9E). [O]

#### 0x000B77C8 `lib_u8_of_s16` — library
Clamps an s16 into 0..0xFF returned as u8. 2 members (0x000B77C8, 0x00121D26). [O]

#### 0x000B77E0 `lib_s8_of_s16` — library
Clamps an s16 into -0x80..0x7F. 2 members (0x000B77E0, 0x00121D3E). [O]

#### 0x000B77F2 `lib_clamp_s16_3` — library
Clamps an s16 to [param_2, param_3]. 3 members (0x000B77F2, 0x00154970, 0x001550C8).
23 callers. [O]

#### 0x000B7808 `lib_clamp_upto_const` — library
Returns min(param_1, DAT_000b78e8). 1 member. [O]

#### 0x000B7812 `lib_s16_of_u16_2` — library
Duplicate of the 0x000B7768 clamp (u16 -> positive s16, 0x7FFF saturation). 1 member. [O]

#### 0x000B781E `lib_u16_of_u32` — library
Clamps a u32 to 0xFFFF. 5 members (0x000B781E, 0x000B782C, 0x00121CAA, 0x00121CB8,
0x00154992). 18 callers. [O]

#### 0x000B783A `lib_s32_abs` — library
Absolute value of s32 with the INT_MIN case mapped to `DAT_000b78e8`. 13 callers. 1 member. [O]

#### 0x000B7854 `lib_s32_clamp_lo0` — library
Clamps an s32 to >= 0. 1 member. [O]

#### 0x000B785C `lib_u16_of_s32` — library
Clamps an s32 into 0..0xFFFF. 5 members (0x000B785C, 0x000B7872, 0x00121CFA, 0x00121D10,
0x00155102). 47 callers. [O]

#### 0x000B7888 `lib_s16_of_s32` — library
Clamps an s32 into -0x8000..0x7FFF. 4 members (0x000B7888, 0x00121CD6, 0x001549A0,
0x001550EE). 149 callers. [O]

#### 0x000B789C `lib_s8_of_s32` — library
Clamps an s32 into -0x80..0x7F. 2 members (0x000B789C, 0x00121CEA). [O]

#### 0x000B78AC `lib_clamp_s32_3` — library
Clamps an s32 to [param_2, param_3]. 2 members (0x000B78AC, 0x001550DE). [O]

#### 0x000B78BC `lib_f32_abs` — library
Float absolute value. 8 callers. 1 member. [O]

#### 0x000B78C8 `lib_u16_of_f32_sat` — library
Float to u16 conversion with saturating bounds: negative -> 0, above `DAT_000b78ec` -> 0xFFFF
(masked). 8 callers. 1 member. [O]

#### 0x000B78F0 `lib_u8_of_f32_sat` — library
Float to u8 with saturation against `DAT_000b79c4` (overflow -> 0xFF, negative -> 0).
1 member. [O]

#### 0x000B7910 `lib_s16_of_f32_sat` — library
Float to s16 clamped into [`DAT_000b79cc`, `DAT_000b79c8`] with -0x8000/0x7FFF ends.
48 callers. 1 member. [O]

#### 0x000B7936 `lib_s32_of_f32_sat` — library
Float to s32 clamped into [`DAT_000b79d4`, `DAT_000b79d0`] with INT_MIN/`DAT_000b79d8` ends.
1 member. [O]

#### 0x000B795A `lib_s8_of_f32_sat` — library
Float to s8 clamped into [`DAT_000b79e0`, `DAT_000b79dc`] with -0x80/0x7F ends. 1 member. [O]

#### 0x000B797C `lib_clamp_f32_3` — library
Clamps a float to [param_2, param_3]. 14 callers. 1 member. [O]

#### 0x000B798E `lib_s16_round_to_10` — logic
Rounds an s16 to the nearest multiple of 10 after clamping to +/-0x7FF8 (32760), adjusting by
+5/-4 before the integer divide. Called from 0x000B0860. 1 member. [O]

#### 0x000B79E4 `event_report_18b_record` — logic
The module's standard event reporter (199 callers): packs param_2 (u16), twelve byte params
and the u32 at *param_1 into an 18-byte record at param_1+1..+0x15 and hands it to
0x000B767E (snapshot buffer write). This is the function behind the `PTR_FUN_.../0x254/...`
reporter calls seen throughout units 19/20. 1 member. [O]

#### 0x000B7A96 `obj_b7aa8_call_m0` — data-accessor
Forwarding thunk: invokes method slot `PTR_FUN_000b7aac` on global `*PTR_DAT_000b7aa8`
(callee 0x000BAE76); called from 0x000BAEB4. 1 member. [I]

#### 0x000B7AB0 `obj_b7d2c_m_a` — data-accessor
Thunk family on the global object `*PTR_DAT_000b7d2c` (methods live in r0c at 0x000D2154+
area): this one takes a u16 index and returns s16 (slot 0xb7d30 -> 0x000D2256). Called from
0x000B986C. 3 members (0x000B7AB0, 0x000B7AC0, 0x000B7AD0 — duplicated thunks). [I]

#### 0x000B7AE0 `obj_b7d2c_m_b` — data-accessor
Same object, slot 0xb7d3c (s16 arg -> s16, callee 0x000D2154). 1 member. [I]

#### 0x000B7AF0 `obj_b7d2c_m_c` — data-accessor
Same object, slot 0xb7d40 (two u16 args -> u16, callee 0x000D2192). 2 members
(0x000B7AF0, 0x000B7B02). [I]

#### 0x000B7B14 `obj_b7d48_call` — data-accessor
Invokes slot `PTR_FUN_000b7d4c` on global `*PTR_DAT_000b7d48` (callee 0x000D2294); consumer
0x000DDCA2. 1 member. [I]

#### 0x000B7B1C `obj_b7d2c_m_d` — data-accessor
Same object as 0x000B7AB0, slot 0xb7d50 (char + u16 -> u16, callee 0x000D21F2); consumers
0x000B108C/0x000B1FEE/0x000B148E/0x000B0860. 1 member. [I]

#### 0x000B7B2E `obj_b7d2c_m_e` — data-accessor
Same object, slot 0xb7d54 (two s16 -> s16, callee 0x000D2170). 1 member. [I]

#### 0x000B7B40 `glob_b7d5c_s16_get` — data-accessor
Returns the s16 at `*PTR_DAT_000b7d5c`; consumer 0x00108D7C. 1 member. [I]

#### 0x000B7B46 `flag_get_b7d48` — data-accessor
Returns the byte `*PTR_DAT_000b7d48`; consumers 0x0010BC24/0x0010B79E/0x000E47CA.
1 member. [I]

#### 0x000B7B4E `flag_get_b7d60` — data-accessor
Returns `*DAT_000b7d60`; consumer 0x000A32E4. 1 member. [I]

#### 0x000B7B56 `flag_get_s8_b7d64` — data-accessor
Returns the signed `*DAT_000b7d64`; consumer 0x000A32E4. 1 member. [I]

#### 0x000B7B5C `obj_b7d58_call_m0` — data-accessor
Invokes slot `PTR_FUN_000b7d68` on global `*PTR_DAT_000b7d58` (callee 0x000D325C); consumer
0x000D140C. 2 members (0x000B7B5C, 0x000B7B64). [I]

#### 0x000B7B6C `obj_b7d58_m2_s16` — data-accessor
Two-byte-arg method (slot `DAT_000b7d70` -> 0x000D326C) returning s16 on object
`*PTR_DAT_000b7d58`; consumers 0x000A32E4/0x000A34D8. 1 member. [I]

#### 0x000B7B7E `obj_b7d74_call` — data-accessor
Invokes slot `PTR_FUN_000b7d78` on `DAT_000b7d74` (callee 0x000D48AE); consumer 0x000A32E4.
1 member. [I]

#### 0x000B7B86 `obj_b7d58_flag_get` — data-accessor
Boolean getter (slot 0xb7d7c -> 0x000D37BC) on `*PTR_DAT_000b7d58`; consumer 0x000B986C.
2 members (0x000B7B86, 0x000B7C4A). [I]

#### 0x000B7B94 `flag_get_b7d80` — data-accessor
Returns `*DAT_000b7d80`; consumer 0x000A3952. 1 member. [I]

#### 0x000B7B9A `flag_get_b7d84` — data-accessor
Returns `*DAT_000b7d84`; consumer 0x000A37DC. 1 member. [I]

#### 0x000B7BA0 `obj_b7d88_m_a` — data-accessor
s16-returning method (slot 0xb7d8c -> 0x000D4580, s16 arg) on `*PTR_DAT_000b7d88`; consumer
0x000B986C. 2 members (0x000B7BA0, 0x000B7BB0). [I]

#### 0x000B7BC0 `obj_b7d88_m_b` — data-accessor
s16 method (slot 0xb7d94 -> 0x000D45C2, two s16 args) on `*PTR_DAT_000b7d88`; consumer
0x000B986C. 2 members (0x000B7BC0, 0x000B7BD2). [I]

#### 0x000B7BE4 `obj_b7d88_m_c` — data-accessor
s16 method (slot 0xb7d9c -> 0x000D4684, byte arg) on `*PTR_DAT_000b7d88`; consumer 0x000B986C.
1 member. [I]

#### 0x000B7BF4 `obj_b7da0_flag_get` — data-accessor
Boolean getter (slot 0xb7da4 -> 0x000D5368) on `*PTR_DAT_000b7da0`; consumer 0x000B986C.
2 members (0x000B7BF4, 0x000B7D10). [I]

#### 0x000B7C02 `obj_b7da0_m2_flag` — data-accessor
Two-char-arg boolean method (slot 0xb7da8 -> 0x000D4F86) on `*PTR_DAT_000b7da0`; 11 consumers
(0x000DDBB4, 0x000C9FD8, 0x000B108C, 0x000EB9EC...). 2 members (0x000B7C02, 0x000B7C14). [I]

#### 0x000B7C26 `obj_b7da0_m3_flag` — data-accessor
Three-arg boolean method (slot 0xb7db0 -> 0x000D4FBA) on `*PTR_DAT_000b7da0`; consumers
0x000B986C/0x000EC788. 1 member. [I]

#### 0x000B7C3A `obj_b7da0_m_s16_flag` — data-accessor
s16-arg boolean method (slot 0xb7db4 -> 0x000D522A) on `*PTR_DAT_000b7da0`; consumer
0x000B986C. 1 member. [I]

#### 0x000B7C58 `obj_b7d58_s16_get` — data-accessor
s16 getter (slot 0xb7dbc -> 0x000D37FE) on `*PTR_DAT_000b7d58`; consumer 0x000A32E4.
1 member. [I]

#### 0x000B7C66 `obj_b7dc0_call` — data-accessor
Invokes slot 0xb7dc4 (callee 0x000D4DC2) on `*PTR_DAT_000b7dc0`; consumer 0x000B986C.
1 member. [I]

#### 0x000B7C6E `obj_b7dc0_flag_get` — data-accessor
Boolean getter (slot 0xb7dc8 -> 0x000D4E8E) on `*PTR_DAT_000b7dc0`; consumer 0x000B986C.
2 members (0x000B7C6E, 0x000B7C7C). [I]

#### 0x000B7C8A `obj_b7dd0_m_byte` — data-accessor
Byte-arg boolean method (slot 0xb7dd4 -> 0x000D4CC4) on `*PTR_DAT_000b7dd0`; consumer
0x000B986C. 1 member. [I]

#### 0x000B7C9A `obj_b7da0_m_byte` — data-accessor
Byte-arg boolean method (slot 0xb7dd8 -> 0x000D5174) on `*PTR_DAT_000b7da0`; consumer
0x000A32E4. 1 member. [I]

#### 0x000B7CAA `obj_b7ddc_m_byte` — data-accessor
Byte-arg boolean method (slot 0xb7de0 -> 0x000D2844) on `*PTR_DAT_000b7ddc`; consumer
0x000A360A. 2 members (0x000B7CAA, 0x000B7CBA). [I]

#### 0x000B7CCA `obj_b7ddc_m_s16` — data-accessor
s16-arg method (slot 0xb7de8 -> 0x000D2B8A) returning s16 on `*PTR_DAT_000b7ddc`; consumer
0x000A360A. 2 members (0x000B7CCA, 0x000B7CDA). [I]

#### 0x000B7CEA `obj_b7da0_m2b_flag` — data-accessor
Two-byte-arg boolean method (slot 0xb7df0 -> 0x000D525C) on `*PTR_DAT_000b7da0`; consumer
0x000A32E4. 1 member. [I]

#### 0x000B7CFC `obj_b7da0_m3_set` — data-accessor
Three-arg void method (slot 0xb7df4 -> 0x000D52CA) on `*PTR_DAT_000b7da0`; consumer
0x000B986C. 1 member. [I]

#### 0x000B7D08 `flag_get_b7da0` — data-accessor
Returns the state byte `*PTR_DAT_000b7da0`; consumer 0x000B5FCC. 1 member. [I]

#### 0x000B7E00 `obj_b7ec8_s16_get` — data-accessor
s16 getter (slot 0xb7ecc -> 0x000D5384) on `*PTR_DAT_000b7ec8`; consumers 0x000D934E/
0x000D9448/0x000D9AA6/0x000D8734/0x000D88E4/0x000D8C3A/0x000D82AE. 1 member. [I]

#### 0x000B7E0E `flag_get_b7ed0` — data-accessor
Returns `*DAT_000b7ed0`; consumer 0x000A32E4. 1 member. [I]

#### 0x000B7E14 `flag_get_b7ed4` — data-accessor
Returns `*DAT_000b7ed4`; consumer 0x000A32E4. 1 member. [I]

#### 0x000B7E1A `flag_get_b7ed8` — data-accessor
Returns `*DAT_000b7ed8`; consumer 0x000A32E4. 1 member. [I]

#### 0x000B7E22 `flag_get_b7edc` — data-accessor
Returns `*DAT_000b7edc`; consumer 0x000A32E4. 1 member. [I]

#### 0x000B7E2A `obj_b7ee4_set2` — data-accessor
Two-s16 setter method (slot 0xb7ee8 -> 0x000D2EB0) on `*PTR_DAT_000b7ee4`; consumers
0x000B986C/0x000D5E30/0x000D5FBA/0x000DE494. 2 members (0x000B7E2A, 0x000B7E4E). [I]

#### 0x000B7E34 `glob_b7eec_s16_get` — data-accessor
Returns the s16 at `*PTR_DAT_000b7eec`; consumers 0x000D5E30/0x000D5FBA. 1 member. [I]

#### 0x000B7E3A `glob_b7ef0_s16_get` — data-accessor
Returns the s16 at `*PTR_DAT_000b7ef0`; consumer 0x000B986C. 1 member. [I]

#### 0x000B7E40 `obj_b7ee4_s16_get` — data-accessor
s16 getter (slot 0xb7ef4 -> 0x000D2E88) on `*PTR_DAT_000b7ee4`; consumers 0x000B986C/
0x000D5E30/0x000D5FBA/0x000DE494. 1 member. [I]

#### 0x000B7E58 `glob_b7ee4_s16_get` — data-accessor
Returns the s16 at `*PTR_DAT_000b7ee4` (direct state word); consumer 0x000DE494. 1 member. [I]

#### 0x000B7E5E `obj_b7efc_call` — data-accessor
Invokes slot 0xb7f00 (callee 0x000D22BE) on `DAT_000b7efc`; consumer 0x000A2E20.
2 members (0x000B7E5E, 0x000B7E66). [I]

#### 0x000B7E6E `obj_b7f08_call_a` — data-accessor
Invokes slot 0xb7f0c (callee 0x000D315A) on `*PTR_DAT_000b7f08`; consumer 0x000BAEB4.
1 member. [I]

#### 0x000B7E76 `obj_b7f10_call` — data-accessor
Invokes slot 0xb7f14 (callee 0x000D4CB8) on `*PTR_DAT_000b7f10`; consumer 0x000BAEB4.
1 member. [I]

#### 0x000B7E7E `obj_b7ec8_call` — data-accessor
Invokes slot 0xb7f18 (callee 0x000D4F7A) on `*PTR_DAT_000b7ec8`; consumer 0x000BAEB4.
1 member. [I]

#### 0x000B7E86 `obj_b7f1c_call` — data-accessor
Invokes `DAT_000b7f20` on `*PTR_DAT_000b7f1c` (no resolved callee); consumer 0x000BAEB4.
1 member. [I]

#### 0x000B7E8E `obj_b7f08_call_b` — data-accessor
Second method (slot 0xb7f24 -> 0x000D31D0) on `*PTR_DAT_000b7f08`; consumer 0x000A308C.
1 member. [I]

#### 0x000B7E96 `obj_b7f28_call` — data-accessor
Invokes slot 0xb7f2c (callee 0x000D46A0) on `*PTR_DAT_000b7f28`; no static callers
(interface entry). 1 member. [I]

#### 0x000B7E9E `obj_b7f30_call` — data-accessor
Invokes slot 0xb7f34 (callee 0x000D4AC8) on `DAT_000b7f30`; consumer 0x000A2DCA.
2 members (0x000B7E9E, 0x000B7EA6). [I]

#### 0x000B7EAE `obj_b7f3c_call` — data-accessor
Invokes `DAT_000b7f40` on `DAT_000b7f3c` (callee 0x000D280C); consumer 0x000A2DCA.
1 member. [I]

#### 0x000B7EB6 `obj_b7ed0_call` — data-accessor
Invokes slot 0xb7f44 (callee 0x000D30CE) on `DAT_000b7ed0`; no static callers.
1 member. [I]

#### 0x000B7EBE `obj_b7ee0_call` — data-accessor
Invokes slot 0xb7f48 (callee 0x000D5418) on `DAT_000b7ee0`; consumer 0x000A2EFC.
1 member. [I]

#### 0x000B7F4C `module_shutdown_seq` — logic
Six-step shutdown/teardown: an indirect call with 0x20, four plain indirect calls
(`PTR_FUN_000b7f80/88/90/94/98` — the callees resolved to 0x000D7DFA/0x000D7F8C/0x000BA6F4/
0x000D7C36/0x000D7C60/0x000D7F4E), then a final call with 0x26 — bracketing a group of
module resets with codes 0x20 and 0x26. Consumers 0x000A2D5E/0x000A2EBC. 1 member. [I]

#### 0x000B7F9C `obj_store_triplet_21` — logic
Stores a (value, value+1, value-or-0-if-negative) byte triple at object +0x21..+0x23
(param_2 negative clamps the third byte to 0). 3 members: 0x000B7F9C (+0x21), 0x000B7FBC
(+0x24), 0x000B7FDC (+0x27) — three consecutive triples in the same object. No static
callers (interface entries). [I]

#### 0x000B8004 `glob_b8018_set_ptr_a` — stub
Sets the global pointer `*PTR_DAT_000b8018 = PTR_PTR_000b8014`. Consumer 0x000DA418.
1 member. [O]

#### 0x000B800C `glob_b8018_set_ptr_b` — stub
Sets `*PTR_DAT_000b8018 = PTR_PTR_000b801c` (alternate pointer). Consumer 0x000DA418.
1 member. [O]

#### 0x000B8020 `glob_b8034_set_ptr_a` — stub
Sets `*PTR_DAT_000b8034 = PTR_PTR_000b8030`. Consumer 0x000DA418. 1 member. [O]

#### 0x000B8028 `glob_b8034_set_ptr_b` — stub
Sets `*PTR_DAT_000b8034 = PTR_PTR_000b8038` (alternate). Consumer 0x000DA418. 1 member. [O]

#### 0x000B803C `flag_get_b813a` — data-accessor
Returns `*_LAB_000b813a`; consumers 0x000E39DC/0x000E3836. 1 member. [I]

#### 0x000B8044 `flag_get_b8140` — data-accessor
Returns `*PTR_DAT_000b8140`; consumers 0x000F3550/0x000A32E4/0x000A34D8. 1 member. [I]

#### 0x000B804C `flag_get_b8144` — data-accessor
Returns `*DAT_000b8144`; consumers 0x000A32E4/0x000A34D8. 1 member. [I]

#### 0x000B8054 `flag_get_b8148` — data-accessor
Returns `*PTR_DAT_000b8148`; 11 consumers (0x00113788, 0x000E4E4A, 0x000EC6FE...).
1 member. [I]

#### 0x000B805C `flag_get_b814c` — data-accessor
Returns `*PTR_DAT_000b814c`; consumer 0x000E8888. 1 member. [I]

#### 0x000B8064 `flag_get_b8150` — data-accessor
Returns `*PTR_DAT_000b8150`; consumer 0x000EA4C8. 1 member. [I]

#### 0x000B806C `flag_get_b8154` — data-accessor
Returns `*PTR_DAT_000b8154`; consumer 0x000D64D2. 1 member. [I]

#### 0x000B8074 `flag_get_b8158` — data-accessor
Returns `*PTR_DAT_000b8158`; consumers 0x000CA65C/0x000B5FCC. 1 member. [I]

#### 0x000B807C `flag_get_b815c` — data-accessor
Returns `*DAT_000b815c`; consumer 0x000A39DC. 1 member. [I]

#### 0x000B8084 `flag_get_b8160` — data-accessor
Returns `*DAT_000b8160`; consumer 0x000A39EA. 1 member. [I]

#### 0x000B808C `flag_get_b8164` — data-accessor
Returns `*DAT_000b8164`; consumer 0x000A39F8. 1 member. [I]

#### 0x000B8094 `flag_get_b8168` — data-accessor
Returns `*DAT_000b8168`; consumer 0x000A39CE. 1 member. [I]

#### 0x000B809C `flag_get_b816c` — data-accessor
Returns `*DAT_000b816c`; consumer 0x000A3A06. 1 member. [I]

#### 0x000B80A4 `flag_get_b8170` — data-accessor
Returns `*PTR_DAT_000b8170`; consumers 0x000D4F24/0x000F31B2. 1 member. [I]

#### 0x000B80AC `obj_b8140_call_a` — data-accessor
Invokes slot 0xb8174 (callee 0x000D82AE) on `*PTR_DAT_000b8140`; consumer 0x000B986C.
3 members: 0x000B80AC (slot 0xb8174), 0x000B80E0 (slot 0xb818c -> 0x000D846C),
0x000B8106 (slot 0xb8194 -> 0x000D8122, no static callers). [I]

#### 0x000B80B4 `obj_b8140_m1_call` — data-accessor
One-byte-arg method (slot 0xb8178 -> 0x000D8678) on `*PTR_DAT_000b8140`; consumer
0x00103DE2. 1 member. [I]

#### 0x000B80BC `obj_b8140_m2_call` — data-accessor
Two-byte-arg method (slot 0xb817c -> 0x000D8662) on `*PTR_DAT_000b8140`; consumer
0x000CFD1A. 1 member. [I]

#### 0x000B80C6 `flag_b8180_is_1` — data-accessor
Returns `*PTR_DAT_000b8180 == 1`; consumers 0x000D07B0/0x00106AE6. 1 member. [I]

#### 0x000B80D0 `flag_get_b8184` — data-accessor
Returns `*PTR_DAT_000b8184`; consumers 0x000D326C/0x00106AE6. 1 member. [I]

#### 0x000B80D8 `flag_get_b8188` — data-accessor
Returns `*PTR_DAT_000b8188`; consumers 0x000B986C/0x000EA4C8/0x000EA230. 1 member. [I]

#### 0x000B80E8 `flag_get_b8190` — data-accessor
Returns `*DAT_000b8190`; consumer 0x000A3AE0. 1 member. [I]

#### 0x000B80F0 `obj_b8140_flags_clear_check` — logic
Returns 1 when bytes at +0x21 and +2 of object `*PTR_DAT_000b8140` are both zero (an
"idle/finished" check); consumer 0x000D134A. 1 member. [I]

#### 0x000B810E `multi_obj_reset_seq` — logic
Reset sweep over seven global objects: five calls of `PTR_FUN_000b819c` (callees
0x000BADA8/0x000BAD8C family) on `PTR_DAT_000b8198/81a0/81a4/81a8/81b4/81b8` interleaved
with two calls of `PTR_FUN_000b81b0` on 0xb81ac/0xb81bc; consumer 0x000BAEB4. 1 member. [I]

#### 0x000B7D1E `obj_b7da0_u16_get` — data-accessor
u16-returning getter (slot 0xb7dfc -> 0x000D537A) on `*PTR_DAT_000b7da0`; consumers
0x000D934E/0x000D846C/0x000D9448/0x000D9AA6/0x000D8734/0x000D88E4/0x000D8C3A/0x000D82AE.
1 member. [I]

#### 0x000B81C0 `obj_slot_b8460_u16` — data-accessor
Calls the global function pointer `DAT_000b8460` (callee 0x000D2140) with the channel byte
and returns u16; consumers 0x000A3B62/0x000A3B72/0x00105F9C/0x00106064/0x000E2194/0x000C9632.
1 member. [I]

#### 0x000B81CE `obj_b8464_flag_get` — data-accessor
Boolean getter (slot 0xb8468 -> 0x000E358C) on `*PTR_DAT_000b8464`; consumer 0x000B986C.
1 member. [I]

#### 0x000B81DC `obj_by_chan_s16_at0` — data-accessor
Resolves an object from the channel byte via `PTR_FUN_000b846c` (callee 0x000E55DC) and
returns the s16 at +0. 4 consumers in the 0x000EF4xx cluster family. 1 member. [I]

#### 0x000B81EA `obj_by_chan_s16_at2` — data-accessor
Same channel lookup, returns the s16 at +2. 1 member. [I]

#### 0x000B81F8 `obj_by_chan_vt_flag_a` — data-accessor
Channel lookup via `PTR_FUN_000b846c` then a boolean virtual call: vtable slot +0x44 with
the sub-object at `+0x3c` of the resolved object. 5 members differing only in the virtual
slot/sub-object pair: 0x000B81F8 (+0x44/+0x3c), 0x000B8212 (+0x2c/+0x24), 0x000B822C
(+0x38/+0x30), 0x000B8246 (+0x5c/+0x54), 0x000B8260 (+0x50/+0x48). Consumers 0x000B5086/
0x000B54E4 (the attempt starters). [I]

#### 0x000B827A `obj_chan_release_check` — logic
Default-1 gate: resolves the current object via `PTR_FUN_000b8470` (callee 0x000E513C),
evaluates virtual slot +0x50 on sub-object +0x48, then for the channel object requires both
bytes +0x56 and +0x57 to be zero to return 0 (i.e. "held" only when the virtual flag is clear
and both inhibit bytes are clear). Consumers 0x00117970/0x0010D130/0x000B3FA2. 1 member. [I]

#### 0x000B82BC `flag_get_b8474_nz` — data-accessor
Returns `*PTR_DAT_000b8474 != 0`; consumer 0x000D8C3A. 1 member. [I]

#### 0x000B82C6 `flag_get_b8478_nz` — data-accessor
Returns `*DAT_000b8478 != 0`; consumers 0x000CFAEC/0x000E9CE0. 1 member. [I]

#### 0x000B82D0 `obj_m_b847c_s16` — data-accessor
Two-arg method (slot 0xb847c -> 0x000DCC70) returning s16; consumer 0x000F0728. 1 member. [I]

#### 0x000B82E2 `obj_chan_m_b8480_s16` — data-accessor
Channel lookup via `PTR_FUN_000b8470` (0x000E513C) then s16 method slot 0xb8480
(0x000E4C00); consumers 0x000DD3C2/0x0010D5E4/0x000DD65C/0x0010D9B6. 1 member. [I]

#### 0x000B82F6 `obj_chan_s16_at_0x42` — data-accessor
Channel lookup then returns the s16 at +0x42; consumers 0x000CB7EC/0x000CDBB4/0x0010E850/
0x0010D5E4/0x000EFDB4. 1 member. [I]

#### 0x000B8306 `obj_chan_m_u16_a` — data-accessor
Channel lookup via `PTR_FUN_000b8470` then u16 method (slot 0xb8484 -> 0x000E4BAE).
3 members: 0x000B8306 (slot 0xb8484), 0x000B836E (slot 0xb8490), 0x000B8382 (slot 0xb8494).
Consumers 0x001037EC/0x000B4D0C/0x000B3610. [I]

#### 0x000B831A `obj_chan_calc_scaled10` — logic
Eight-argument calculation: channel lookup then `DAT_000b8488(obj, 6 bytes)`; result is
`(ret + 9) / 10` — a value scaled to units of 10. Consumers 0x00113788/0x00112FB4.
1 member. [I]

#### 0x000B8356 `obj_chan_m_u16_arg` — data-accessor
Channel lookup then two-arg u16 method (slot 0xb848c -> 0x000E4C18); consumers
0x00111CAE/0x001147C8. 1 member. [I]

#### 0x000B8396 `obj_b8464_flag_get2` — data-accessor
Boolean getter (slot 0xb8498 -> 0x000E369E) on `*PTR_DAT_000b8464`; consumers
0x0010485E/0x000DB63C. 1 member. [I]

#### 0x000B83A4 `obj_chan_flag_at_0x51` — data-accessor
Channel lookup then returns the byte at +0x51; consumer 0x000CA5C8. 1 member. [I]

#### 0x000B83B4 `flag_get_b849c` — data-accessor
Returns `*PTR_DAT_000b849c`; consumers 0x000E993A/0x001066D6/0x000DDCA2. 1 member. [I]

#### 0x000B83BC `flag_get_s8_b84a0` — data-accessor
Returns the signed byte `*PTR_DAT_000b84a0`; consumers 0x000E993A/0x0010D130/0x0010F084/
0x000E984A/0x000E9B1A. 1 member. [I]

#### 0x000B83C2 `flag_get_b84a4` — data-accessor
Returns `*PTR_DAT_000b84a4`; consumer 0x000DFE68. 1 member. [I]

## Unit summary

220 clusters, 287 functions (reps + members) documented in unit order.

Counts by kind (per function):
- data-accessor: 155 (global flag getters and vtable-slot thunks on module objects
  `*0xb6c04`, `*0xb6c0c`, `*0xb6c54`, `*0xb6c64`, `*0xb7d2c`, `*0xb7d58`, `*0xb7d88`,
  `*0xb7da0`, `*0xb7dc0`, `*0xb7ddc`, `*0xb7ec8`, `*0xb7ee4`, `*0xb8140`, `*0xb8464`,
  and the channel-object family via `PTR_FUN_000b846c/8470`)
- library: 77 (calibration-table readers/interpolators + numeric clamp/convert helpers)
- logic: 48 (the sequence/monitor state machines and event/report plumbing)
- stub: 6
- dispatch: 1 (0x000B4D0C phase engine)
- unknown: 0

Content bands:
1. 0x000B4372-0x000B5C40: a two-channel, 5-step "request/sequence" state machine
   (`seq_*`) with per-step calibration tables, publish setters and an abort/error path,
   plus a small monitor (`monit_*`) with ramp phases 2..6, calibration windows and a
   20000-period watchdog timer. Exact vehicle function (adaptation run? engagement test?)
   not provable from this band alone — names stay generic.
2. 0x000B6C80-0x000B7472: the generic calibration access layer — `tbl_read_*`,
   `tbl2_read_*`, `tbl_curve_*_interp`, `tbl2_curve_*_interp` (u8/s16/u16/f32). These are
   the firmware-wide table helpers; 0x000B6C8A is the spec-09-cited byte getter.
3. 0x000B7594-0x000B798E: numeric helper library (clamps, saturating float converts,
   abs, round-to-10) with many cross-region duplicate copies.
4. 0x000B75A6-0x000B767E: event/trace plumbing — `event_log_id1c_6bytes` (99 callers) and
   `event_report_18b_record` (199 callers) writing 18-byte records into a 10-slot snapshot
   buffer; these are the target of the `PTR_FUN(PTR_DAT, 0x254, ...)` reporter calls seen
   throughout units 19/20.
5. 0x000B7B4E-0x000B83C2: hundreds of one-line getters/invoker thunks exposing module
   globals and object methods to the rest of the application (callers in r0c/r10/r14
   regions: 0xC0000-0x117FFF). Named `flag_get_<ptr>`, `obj_<ptr>_<method>` for
   traceability; semantics of the individual flags are not decidable here.

No `unknown` entries were needed — even the anonymous accessors are unambiguous about what
they touch.

Cross-region leads:
- The `seq_*`/`monit_*` machines are driven from r0c (0x000CA3A6 task, error/abort callers
  0x000CCC50/0x000CC9EE/0x000CCEEC/0x000CD124/0x000CDD6C) — the r0c unit that owns those
  callers holds the semantic key to this module and should cross-check.
- The objects behind `*0xb7d2c`, `*0xb7da0`, `*0xb8140`, `*0xb8464` implement their methods
  in r0c (0x000D2140-0x000D8678) — those r0c functions should reuse the method names given
  here (slots are documented per thunk).
- Duplicated library copies exist in r10 (0x00121C86-0x00121D3E, 0x001289EC), r0c
  (0x000D7C24/0x000D7C4E, 0x000D9650) and r14 (0x001549xx/0x001550xx/0x001552D0) — the
  units covering those addresses should keep the `__2..__n` suffixes assigned here.
- 0x000B6C8A `tbl_read_u8` is the calibration getter cited by spec 09 (shift-and-lockup
  calibration access); spec cross-ref noted in its entry.
