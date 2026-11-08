# Unit 29 (r0c_app)

Converter/lockup control module (spec 09 §3): engagement state machine, demand/inhibit logic,
drive-coast detection, slip-control learners (spec 11), EEPROM end-of-line characteristics and
pressure-request objects (spec 12). Range 0x000DE0FE-0x000E510A.

#### 0x000DE0FE `lockup_init_call_subsystems` — logic
Called once from 0x000BAD10 (init chain 0x000BAEB4); fires ~8 paired object/method pointers
(0x000DE320–0x000DE354), passing `param+0x12` to one of them. Looks like the converter module's
post-construction "start all sub-objects" pass. Indirect calls only, so edges.csv shows nothing.
[I]

#### 0x000DE13A `lockup_speed_u16_get` — data-accessor
Returns a u16 speed via virtual getter (resolves to 0x000EEA2E → 0x0010CDB0); when the mode byte
at 0x000DE360 == 4 it instead returns 0x00112A08 (simulation source: 0x7FFF unless sim active,
then measured+simulated). Used by 0x000E6AE8. [I]

#### 0x000DE158 `lockup_state_u8_get` — data-accessor
Byte getter: `*PTR_DAT_000de358`, or virtual 0x000EEA3C (`*DAT_000eebcc`) when mode byte
0x000DE360 == 4. Callers 0x000E72E6/0x000E728A/0x000E6AE8/0x000BAB2C — a state flag the
surrounding strategy block reads. [I]

#### 0x000DE174 `lockup_flag_u8_get` — data-accessor
Third of the getter trio: `*PTR_DAT_000de370`, or virtual 0x000EEA44 (0 unless `ptr[8]==1`)
when mode byte 0x000DE360 == 4. Callers 0x000E72E6/0x000E728A. [I]

#### 0x000DE190 `lockup_step_arbiter` — logic
Steps a 1..8 value one unit toward a requested target (±1 from `current`), checks the range,
then scales two table values (+0x94/+0x7E of the 0x000DE390 struct) by per-step ratios and asks
a virtual validator (PTR_FUN_000de3a0) to accept the step. Caller 0x000DD954 compares the result
with its stored value and logs event 0x373 on mismatch. Cluster of 1. [I]

#### 0x000DE2AC `lockup_ctx_status_write` — logic
Fills ctx+2/+3/+4 with results of virtual queries (indices 1/2/3 on the 0x000DE3AC object,
using a previous byte as carry), or zeroes all three when param_3 is set. Called by the
0x000DDC38/0x000DDCA2/0x000DDED6 strategy block. [I]

#### 0x000DE2FA `lockup_ctx_field12_zero` — stub
If global flag `*PTR_DAT_000de3b4` is set, clears ctx+0x12 (u16). Called from 0x000DDFD8 after
it computes a blended value into ctx+0xE. [O]

#### 0x000DE3B8 `lockup_ctx_field10_floor` — stub
Clamps ctx+0x10 (u16) up to the table minimum at 0x000DE468+0x94; run from 0x000DDFD8 when flag
`*PTR_DAT_000de0c4` is set. [O]

#### 0x000DE3CC `lockup_ctx_field0a_floor` — stub
Same pattern as 0x000DE3B8 for ctx+0x0A against 0x000DE468+0x7E. [O]

#### 0x000DE3E0 `lockup_tq_threshold_pred` — logic
Predicate comparing a fetched value (PTR_FUN_000de470) against `getter(PTR_FUN_000de488) +
map(state+5, FUN_000dd862(ctx,1))` — direction of the comparison flips with a condition pair
(0x000DE480 flag, calibrated bytes at 0x000DE474+0x21/+0x27). Called by 0x000DDB04. [I]

#### 0x000DE494 `lockup_setpoint_follower` — logic
Reads current setpoint/value pair from a regulator object (value and value/10 into +0/+2),
computes a rate term `(x - last) * k`, scales it by a coefficient map (object +0x184), blends a
delta between two table values ((100-p)*A + p*B)/100 and writes `base + delta` back through a
virtual setter. Callers: 0x000E106E and the slip-control cluster 0x0010A742/0x0010B70C/0x0010C826.
[I]

#### 0x000DE5CC `lockup_module_ctor` — entry
Constructor for the converter-module object: allocates 0x290 bytes via virtual new
(PTR_FUN_000de810, fails → 0), initializes sub-blocks +0xF4 and +0x218 (0x0010B154/0x001088B6),
sets state +4=1 (open) and +0x1A=1. No direct callers — reached via the init function tables at
0x000BA2xx–0x000BA4xx. [O]

#### 0x000DE614 `lockup_module_reset` — logic
Per-reset reinit from main task 0x000E0FE4: state +4=1, +0x14=0, resets the +0x218 sub-object
and timer block +0xF4, clears +0x16, seeds float +0x260 from a u16 calibration (+0x44)/constant.
[O]

#### 0x000DE664 `lockup_conv_model_eval` — logic
Wrapper: virtual tick (0x00109200), init +0xF4 (0x00108D7C), run 0x000DE890 (converter torque
model), then close (0x00109094). Called only from the slip-control cluster 0x0010B2EA…0x0010C7E0
— the "evaluate converter model" entry used there. [I]

#### 0x000DE688 `lockup_engagement_run` — logic
Engagement-chain runner (spec 09 §3.3: state machine "run from 0xDE688"): temperature window
check against float tables, drive-direction flag at +0x33, ramp generator into +0x288/+0x21C,
then FUN_000dea5e (state machine) and FUN_000df0a8, sub-state flags +0x2B/+0x2C (state 3/4),
timer +0x28 bookkeeping, finally FUN_000df004. Called from 0x000E106E. [O] (spec 09)

#### 0x000DE890 `lockup_conv_torque_model` — logic
Open-converter torque model: maps a speed ratio (u16/const via PTR_FUN_000dea9c) to a capacity,
then `T = (map - offset(+0xC)) * gain(+0x4)`; stores into +0x20 or +0x24 depending on a
direction byte and returns it. Called by slip control 0x0010A3A0…0x0010C8A6 and 0x000DE664.
Matches the "open-converter model" of spec 09 §3.4. [I]

#### 0x000DE902 `lockup_target_map_plus_trim` — data-accessor
When sub-flag +0x218 is clear: bilinear map (0x000B7164) over calib struct +0x68 using ctx+0x16
and a table x-axis (0x000DEAA8), plus stored trim +0x222. Called from 0x000E0F28. [I]

#### 0x000DE930 `lockup_state_clamp02` — data-accessor
Clamps the engagement sub-state byte at +0x14 into 0..2 (helper 0x000B772C through vtable slot
0x000DEAB0) and stores the result at +0x32. Also called from init 0x000BA2F2. [O]

#### 0x000DE94A `lockup_state1b_clamp02` — data-accessor
Same clamp of byte +0x1B into 0..2, returns without storing. Called from 0x000BA2BA and
0x000DE964. [O]

#### 0x000DE95E `lockup_glob_u32_get` — stub
Returns `*DAT_000deab4` (a value fetched through a pointer global). Trivial getter. [O]

#### 0x000DE964 `lockup_state_delta_calc` — logic
Computes change flag +0x30 = (clamped +0x14 state != clamped +0x1B state) and time +0x2E =
`*DAT_000deab4` × 10. Called from init 0x000BA41E. [O]

#### 0x000DEA16 `lockup_run_gate` — logic
Returns 0 (block) only when module flag +0x14 is set AND `*PTR_DAT_000dead0` is set, else 1.
Polled from init/registration 0x000BA3F8. [I]

#### 0x000DEA2C `lockup_vget_index5` — data-accessor
Single virtual getter call: `(obj 0x000DEAD4, index 5)` → u16 (resolves to 0x000B8558). Used by
state-machine guard 0x000DF658. [I]

#### 0x000DEA3C `lockup_slip_err_record` — stub
Virtual set (0x000DEAA0 object, count from 0x000DEAA4, index 4) and records float slip error
`f[0] - threshold(0x000DEAE0)` into +0x214. Entered on the 4→7 transition of the state machine.
[I]

#### 0x000DEA5E `lockup_engagement_sm` — dispatch
The engagement state machine itself (spec 09 §3.3): 12-state switch on +4 with guard functions
0x000DF1FE…0x000DF7A6, pressure/fill sub-object ops at +0x85/+0x86/+0x9A, and event 1099
logged from the default state via the 0x000DF000 logger. Main path 1→2→3→4→5/6/8⇄7/9/10,
11/12 exit states. 545 instructions, called only from 0x000DE688. [O] (spec 09 §3.3)

#### 0x000DF004 `lockup_slip_targets_update` — logic
Computes the SM slip-target working set: converts turbine/input slip floats through scale
constants (DAT_000df0e0/f4) into u16 targets at ctx+0x34/+0x50/+0x58, and a delta at ctx+0x18 =
target minus map value (*100) read via PTR_FUN_000df114 from 0xDF0F8-obj +0x2a8. If ctx+0x86 is
set the delta path is zeroed (ctx+0x1C target zeroed). Called from engagement runner 0x000DE688.
[O]

#### 0x000DF0A8 `lockup_sm_event_decode` — dispatch
Decodes the next transition event for the 12-state engagement SM (spec 09 §3.3): indexes
switchdata 0xDF120 by state (+4) 1..12, combines mode bytes 0xDF118[2]/[6] and sub-mode cell
PTR_PTR_000df11c[u3+u1*4] into event code 0..12 stored at ctx+0x31. State >12 → code 0xF plus
event-1099 log via 0x000DF35C logger. Called only from 0x000DE688. [O] (spec 09 §3.3)

#### 0x000DF1FE `lockup_guard_cnt258_pending` — logic
SM guard: returns 0 only when ctx short +0x258 != 0, byte +0x220 == 0, and global
*PTR_DAT_000df364 != 1; else 1. Called from state 0x000DEA5E. [O]

#### 0x000DF21C `lockup_guard_torque_avail` — logic
SM guard: virtual flag on 0xDF368 object; 1 when that flag set AND (torque-obj +0x3c flag set OR
*PTR_DAT_000df374 == 0). Calls classifier 0x000DFC5E. [O]

#### 0x000DF248 `lockup_guard_release_states` — logic
SM guard for states 5/6/10: 1 when 0xDF368 vflag mismatch-or-changed AND state in {5,6,10} AND
gear-mode byte at 0xDF370-obj +0x40 in {1, (0 with *0xDF374==0), (2 with *0xDF37c==0)}.
Calls torque-band guard 0x000DFB7C. [O]

#### 0x000DF2B8 `lockup_guard_speed_band_lt3` — logic
SM guard: 1 when mode *PTR_DAT_000df380 == 3 and both short thresholds 0xDF384/0xDF38C
(scaled via PTR_FUN_000df388) are <= speed byte at 0xDF370-obj +0x58. [O]

#### 0x000DF300 `lockup_pred_cnt_24e_le_25a` — stub
Single predicate: ctx u16 +0x24E <= ctx u16 +0x25A (counter/timer pair compare in the engagement
context). Guard called from 0x000DEA5E. [O]

#### 0x000DF30E `lockup_guard_speed_dev_open` — logic
SM guard: 1 when speed deviation (byte at 0xDF370-obj +0x5c vs short 0xDF390+8, sign selected by
*0xDF394) exceeds the band OR ctx short +0x27A == 0, AND mode *0xDF380 < 3. [O]

#### 0x000DF398 `lockup_pred_mode_lt3` — stub
Returns *PTR_DAT_000df5d0 < 3 (mode byte test). Guard called from 0x000DEA5E. [O]

#### 0x000DF3A4 `lockup_guard_slip_band_mode3` — logic
SM guard: 1 when slip deviation (short 0xDF5d8+8 vs speed byte at 0xDF5d4-obj +0x58, sign by
0xDF5dc[1]) exceeds band AND 0xDF5dc[2] == 3. [O]

#### 0x000DF3E4 `lockup_guard_engage_ready` — logic
Largest SM guard (188 instr): builds ctx+0x1C "ready" flag from slip windows ctx+0x48/+0x4C and
+0x3C/+0x40, accel/torque ramps at 0xDF5e8-obj, margin flags from 0xDF5ec/+0xf0 thresholds,
mode 0xDF5dc[2], zeroed ctx counters +0x254/+0x256, and chained virtual flags 0xDF600/0xDF608.
Returns 1 when ready AND (margin bVar5 OR range bVar4 OR accel bVar1). [O] (spec 09 §3.3)

#### 0x000DF58C `lockup_flag_df610_get` — stub
Returns *PTR_DAT_000df610. Guard input for 0x000DEA5E. [O]

#### 0x000DF594 `lockup_guard_torque_margin` — logic
SM guard: 1 when *0xDF610 set; else 0 if 0xDF5d4-obj +0x278 flag set or scaled torque
(float at +0x228 * DAT_000df614, via PTR_FUN_000df618) < ctx short +0x34. [O]

#### 0x000DF61C `lockup_guard_hold_release` — logic
SM guard: 0 only when *0xDF840 == 0 AND *0xDF844 != 0 AND ctx byte +0x33 == 0; else 1.
Identical twin at 0x000DF63A (duplicated helper copy). Cluster count 2. [O]

#### 0x000DF658 `lockup_guard_slip_target_ok` — logic
SM guard: 0 (condition met) when *0xDF840 == 0 and ratio target from vget 0x000DEA2C >=
0xDF848-obj +0x280 ratio *10, plus band checks vs 0xDF84c/+0x284/+0x288/+0xDF850 and target
!= 500. [O]

#### 0x000DF6B4 `lockup_guard_df844_release` — logic
SM guard: 0 when *0xDF844 != 0 AND ctx byte +0x33 == 0; else 1. [O]

#### 0x000DF6CA `lockup_guard_release_clear` — logic
SM guard: 1 when *0xDF844 == 0 AND (short 0xDF854 == 0 OR ctx short +0x27A == 0). [O]

#### 0x000DF6EC `lockup_pred_df844_clear` — stub
Returns *PTR_DAT_000df844 == 0. Guard called from 0x000DEA5E. [O]

#### 0x000DF6F6 `lockup_guard_slip_margin` — logic
SM guard: 1 when 0xDF858[2] == 0 and (slip counter 0xDF854 == 0 OR current slip (conversion of
0xDF848-obj +0x30 vs 0xDF858[0xe], offset *0xDF85c) stays below it). [O]

#### 0x000DF738 `lockup_pred_cnt258_zero` — stub
Returns ctx short +0x258 (== +600) == 0. Guard called from 0x000DEA5E. [O]

#### 0x000DF742 `lockup_guard_downshift_slip` — logic
SM guard: override latch when 0xDF864[0xc] flag and short 0xDF864+0x3a set; else 1 when
0xDF858[2] == 1 and speed deviation (byte at 0xDF848-obj +100 vs short 0xDF868+8, sign by
0xDF858[1]) exceeds band OR state == 5. [O]

#### 0x000DF7A6 `lockup_guard_upshift_slip` — logic
SM guard: same override latch (0xDF864[0xc] && 0xDF864+0x3a); else 1 when 0xDF858[2] in {2,3}
and threshold 0xDF86c (scaled via PTR_FUN_000df870) <= speed byte at 0xDF848-obj +0x60 OR
state == 5. [O]

#### 0x000DF7FC `lockup_torque_windows_load` — logic
Loads torque/slip float windows into ctx +0x3C/+0x40/+0x48/+0x4C from pointer tables at
0xDF848-obj +8/+0x10/+0x18/+0x24 indexed by selector byte (0xDF878[2] or 0xDF8f4[2]); range
check (0xDF874 vs obj +0x50/+0x54) picks fixed vs indexed window and sets flag ctx+0x2A.
Optionally calls PTR_FUN_000df8fc on sub-object ctx+0xf4. From runner 0x000DE688. [O]

#### 0x000DF900 `lockup_ctx2_reset` — logic
Zeroes release-sub-ctx bytes +6..+0xB and loads u32 timer +0x18 = *PTR_DAT_000dfb48 * 300
(0.3 s units). Called from lockup main task 0x000E0FE4. [O]

#### 0x000DF922 `lockup_gear_calc_update` — logic
Refreshes release-ctx gear bytes +7/+10/+6/+8/+9 by converting 0xDF848...0xDFb4c-obj +0x28/+0x34/
+0x38/+0x30/+0x2c through PTR_FUN_000dfb54/0xdfb58, then computes gear byte [0] from a 40-entry
identity table indexed by gear selector 0xDFb50[0xb8] + 5*mode (mode from 0xDFb64/0xDFb68 pairs
0..7). From main task 0x000E0FE4. [I]

#### 0x000DFAAE `lockup_release_ctx_update` — logic
Updates the release sub-context each cycle from 0x000E0FE4: restarts timer +0x14 via
PTR_FUN_000dfb70 with *0xDFb48 and 0xDFb4c-obj +0x188, copies state +4/+0xc, evaluates torque
short 0xDFb74 against obj +0x3c/+0x40/+0x4c bands to set dir byte +1, then chains mode decode
0x000DFD2C and flag update 0x000DFE06. [O]

#### 0x000DFB7C `lockup_guard_torque_band` — logic
Classifies torque short 0xDFd20 against bands 0xDFd1c-obj +0x3c/+0x40/+0x4c and 0xDFd24 by
direction pair (ctx+1, obj +0x198/+0x19c) into 0..3; returns 1 unless class 0 with torque
outside every band. Shared guard: called from SM 0x000DEA5E and 0x000DF248. [O]

#### 0x000DFC5E `lockup_pred_slip_band_class` — logic
Seven-way slip classification (returns nonzero=matched) using 0xDFd1c-obj +0x3c/+0x44/+0x4c/
+0x40, counters 0xDFd20/0xDFd24, direction ctx+1 and mode bytes obj +0x1a0/+0x1a4 (AND/OR
semantics per mode 0/1/2, negated bands for release direction). Called from 0x000DF21C. [O]

#### 0x000DFD2C `lockup_release_mode_decode` — logic
Decodes release sub-mode: ctx+0xd via conversion (0xDFe34-obj +0x24, mode 0xDFe30[0xb0] forced
to 3 when ctx+0xa set), then flag lattice over 0xDFe3c[1..7]/0xDFe40/0xDFe4c/0xDFe50 and virtual
0xDFe48 picks ctx+2 in 0..3 with reason codes 1,4,5,6,7,8 written to ctx+0x10. Chain from
0x000DFAAE. [I]

#### 0x000DFE06 `lockup_release_flag_update` — stub
Sets release ctx+0xb = 1 only when virtual flag 0xDFe60 == 1 and **PTR_DAT_000dfe34 == 1;
else 0. Called from 0x000DFAAE. [O]

#### 0x000DFE68 `lockup_release_ctx_eval` — logic
314-instr release/blending monitor from main task 0x000E0FE4: runs 0x000E02A6 and 0x000E0440,
then evaluates sub-ctx flags +3/+0xb/+0xc/+0xd/+5/+6/+7/+0xe/+2 against gear byte
0xE00a8[4], speed/torque ramps at 0xDF370-obj (+0x128/+0x130/+0x134/+0x138/+0x144/+0x148/+0x160/
+0x164/+0x1c0/+0x1c4), latches timer +0x20, and folds everything into master enable byte +1.
[I]

#### 0x000E016E `lockup_ctx2_targets_load` — data-accessor
Loads u16 targets into release ctx +0x1C/+0x1A/+0x1E/+0x20 from 0xE0328-obj fields +0x154/+0x15c/
+0x158/+0x12c and clears bytes +0xA/+4/+0x18. Called from lockup main task 0x000E0FE4. [O]

#### 0x000E01A6 `lockup_ctx2_flags_pack` — logic
Packs nine boolean conditions into status bitfield ctx u16 +0x10: bits 0..5,7 from ctx bytes
[0],+7,+0xa,+0xb,+0xc,+0xd,+0xe and bits 8/9 from globals 0xE032c[0]/[0x12]. From 0x000E0FE4. [O]

#### 0x000E0272 `lockup_shift_active_gate` — data-accessor
Returns ctx byte +0xA only when shift-active flag (0xE0328-obj +0x118) is set, else 0.
Called from 0x000E10A6. [O]

#### 0x000E0288 `lockup_vpred_obj168_query` — stub
Single virtual predicate: (*DAT_000e0334)(0xE0328-obj +0x168, *PTR_DAT_000e0330) != 0.
Called from 0x000BA2AC (registration/init area). [I]

#### 0x000E02A6 `lockup_gear_target_track` — logic
Gear-pair tracker (158 instr) from 0x000DFE68: enable ctx+0x12 requires gear 0xE0344[4] in 6..10,
shift-active at 0xE0328-obj +0x118, and vflag 0xE034c; else targets +0x1C/+0x1A reload from obj
+0x154/+0x15c. Enabled path ramps the two counters toward each other (0xE0354-0xE0358/0xE04dc,
scaled check via 0xE04e8 vs obj +0x11c/+0x120), decrementing per pass and latching flags +0xA/
+4/+0x18 and dwell counter +0x1E. [I]

#### 0x000E0440 `lockup_speed_status_pack` — logic
From 0x000DFE68: sets ctx[0] when 0xE04f8-obj +0xfc flag and u16 +0x100 below 0xE04fc, clears
when 0xE0500+10 falls below (+0x100 value − obj +0x104 byte); ctx[9] compares map getter
(0xE04f0-obj +0x74) against 0xE0500+10; packs both into 2-bit status ctx[8] (0..3). [O]

#### 0x000E0504 `lockup_setpoint_calc` — logic
225-instr setpoint generator from 0x000E0FE4: slews three paired counters (+0x14/+1,
+0x12/+0x29, +0x10/+0x1e) toward raw targets built from 0xE0714-obj speeds (+0x54/+0x58/+0x5c/
+0x60 bytes ×10 + 0xE0718 offsets), converting through /300 ramp helpers and /DAT_000e0720
float scale; blends high/low ramps by ratio DAT_000e0740 and computes delta output +8/+9 from
current setpoint minus torque-scaled term (obj +0x9c float + 0xE0748 getter). Calls table
select 0x000E06FA. [I]

#### 0x000E06FA `lockup_setpoint_tbl_select` — data-accessor
Refreshes the pair behind PTR_PTR_000e0734: [0]=0, [1]=table 0xE075c[byte *PTR_DAT_000e0758].
Called from 0x000E0504. [O]

#### 0x000E0776 `lockup_seq_state5_load` — logic
Sequence-SM state-5 entry (from 0x000E0D98): reloads timer ctx+0x26 from 0xE09b0-obj +0x20
(−1), loads ctx+0x18 from conversion of *0xE09b4×DAT_000e09b8, and seeds +0x20/+0x10 from
shorts 0xE09c0/0xE09c4. [O]

#### 0x000E07B6 `lockup_seq_state4_accum` — logic
State-4 body: decrements timer ctx+0x26 and accumulates per-cycle increments into ctx+0x18
(conversion *0xE09b4×DAT_000e09b8), +0x20 (short 0xE09c0) and +0x10 (short 0xE09c4). [O]

#### 0x000E07F8 `lockup_seq_state3_enter` — logic
State-3 entry: reloads timer ctx+0x26 from 0xE09b0-obj +0x24 (−1); sets ctx+2 latch when gear
byte *0xE09c8 in {9,10} and 0xE09b0-obj +0x40 flag set. [O]

#### 0x000E083A `lockup_seq_timer_tick` — stub
Decrements sequence timer ctx+0x26 when nonzero. Cluster of 3 identical copies
(0x000E083A, 0x000E1836, 0x0010886A). [O]

#### 0x000E084C `lockup_seq_state2_reload` — stub
Reloads sequence timer ctx+0x26 from 0xE09b0-obj +0x1C (−1) — state-2 entry from 0x000E0D98. [O]

#### 0x000E0868 `lockup_seq_output_adjust` — logic
Applies sequence output: copies ctx+6→ctx+4; when vflag 0xE09cc==1 computes
clamp(ctx+4 + delta(0xE09d4), 0xE09b0-obj +0x38, +0x34) via PTR_FUN_000e09dc and stores it back
through setter PTR_FUN_000e09e0. From 0x000E0D98. [I]

#### 0x000E0970 `lockup_seq_active_flag` — logic
Sets ctx+3 = 1 unless sequence state ctx+5 in {0,1,5} — i.e. active-latch for states 2/3/4.
Runs each cycle after the SM (caller 0x000E0760). [O]

#### 0x000E0988 `lockup_pred_cnt_lt_thr` — stub
Returns *PTR_DAT_000e0a08 < 0xE09b0-obj +4 short (counter below threshold) — state-1 exit test
in 0x000E0D98. [O]

#### 0x000E099A `lockup_pred_cnt_gt_thr5` — stub
Returns 0xE09b0-obj +4 short + 5 < *PTR_DAT_000e0a08 (counter past threshold with +5
hysteresis) — state-2 exit test. [O]

#### 0x000E0A0C `lockup_seq_state2_exit_guard` — logic
Guard: 1 when timer ctx+0x26 running, 0xE0c18[0xc] flag, *0xE0c1c clear, 0xE0c20 short below
**PTR 0xE0c24, and virtual 0xE0c28 pattern (==1 with *0xE0c2c clear and [4] clear, or ==0).
Calls 0x000E28D4. State-2→3 transition test in 0x000E0D98. [I]

#### 0x000E0A66 `lockup_pred_seqtimer_zero` — stub
Returns sequence timer ctx+0x26 == 0. Six identical copies: 0x000E0A66, 0x000E0A70, 0x000E0B8A,
0x000E1350, 0x000E18C8, 0x0010ADA4 (duplicated timer-done predicates). [O]

#### 0x000E0A7A `lockup_seq_start_guard` — logic
State-0 entry guard (124 instr): requires 0xE0c24-obj +0x3c flag, positive slip float 0xE0c20+4,
vflag 0xE0c3c, converted turbine speed (0xE0c4c on obj +0x58) below 0xE0c40, speed byte window
obj +0x2c/+0x30 around 0xE0c50[0x96], short window obj +0x14/+0x18, mode flags 0xE0c30[0]/
0xE0c50[0xc1] clear, gear *0xE0c58 in {6,8} or {9,10 with obj +0x40}, 0xE0c50[0xeb]/[0xed]
clear with *0xE0c1c set, counter window obj+4/+0x10 and vflag 0xE0c60 clear. [I]

#### 0x000E0B94 `lockup_seq_gear_ok_pred` — logic
Returns 0 (sequence not allowed) when gear *PTR_DAT_000e0c58 > 5 and it is neither 9/10-with-
obj+0x40-flag nor 7 and *PTR_DAT_000e0c1c == 0; else 1. Called from SM 0x000E0D98 and
0x000E0BD8. [O]

#### 0x000E0BD8 `lockup_seq_state4_exit_guard` — logic
State-4 exit test: 0 (done) when gear predicate 0x000E0B94 clears AND diff of shorts
0xE0c64−0xE0c68 within 0xE0c24-obj +0x28 AND short 0xE0c20 within obj +8/+0xc band. [O]

#### 0x000E0C6C `lockup_seq_hold_guard` — logic
98-instr hold/abort guard polled each state: 0 (may proceed) when speed ±10 window (0xE0e60+10
vs 0xE0e5c-obj +0x14/+0x18), pedal byte ±2 window (0xE0e68[0x96] vs obj +0x2c/+0x30), flags
*0xE0e64 and 0xE0e68[0xc1] clear, state-or-flags 0xE0e68[0xeb]/[0xed], 0xE0e68[0xb0] set, gear
in {6,8,7,9/10-with-flag}, vflag 0xE0e74 clear and short 0xE0e78 <= obj +0x10. [I]

#### 0x000E0D44 `lockup_seq_finish_pred` — logic
Returns 0 (sequence finished) when 0xE0e7c[1] == 0xE0e7c[0xc] and *PTR_DAT_000e0e64 == 0; else
1. State-5 finish test. [O]

#### 0x000E0D5E `lockup_seq_precheck` — logic
Per-cycle precheck of the sequence runner 0x000E0760: when 0xE0e5c-obj +0x3c flag set and
*PTR_DAT_000e0e80 clear, computes latch ctx+0x1c from getter 0xE0e88 and conversion chain
0xE0e90/0xE0e94; else 0. [I]

#### 0x000E0D98 `lockup_seq_sm` — dispatch
Six-state sequence state machine on ctx+5 (0..5), driven from 0x000E0760: state 0 start guard
0x000E0A7A, hold guard 0x000E0C6C, counter tests 0x000E0988/0x000E099A, timer reloads
0x000E084C/0x000E07F8/0x000E0776, accumulate 0x000E07B6, exit guards 0x000E0A0C/0x000E0BD8/
0x000E0A66/0x000E0D44, output adjust 0x000E0868. Invalid state → reset +0x5/+2 and event 1083
(0x43b) via logger 0x000E0EFE. [O]

#### 0x000E0EFE `lockup_seq_evt_invalid_log` — stub
Single call: event logger 0xB79E4 with (0x43b=1083, 0xF, 1, 3, 0x53). callers=-, so reached
via pointer/tail from the sequence SM. [O]

#### 0x000E0F28 `lockup_seq_output_calc` — logic
Sequence output: 0 when enable byte 0xE0fc8[0x218] set or gear <4 path; else base map+trim from
getter 0xE0fd0 (resolves to lockup map 0x000DE902 family), slew via 0xE0fd8/0xE0fdc on
*0xE0fc8 float × DAT_000e0fd4 against 0xE0fcc-obj +0x5c, scaled ×ctx+0xE/100 into ctx[0]. [I]

#### 0x000E0FE4 `lockup_task_dispatch` — dispatch
Main cyclic lockup task entry (caller 0x000BA406): fires one of two ~9-entry function-pointer
lists selected by mode *PTR_DAT_000e10f8; resolved targets include 0x000E016E, 0x000E11FA,
0x000DF922, 0x000E14D0, 0x000E01A6, 0x000E267C, 0x000E0504, 0x000E1AF6, 0x000DFAAE,
0x000DF900, 0x000DFE68, 0x000E1DB8, 0x000E1A60, 0x000E2C2E, 0x000DE614 and 0x10C994/0x109770/
0x000E2EB2. [O]

#### 0x000E105C `lockup_task2_tick` — stub
Cyclic entry (caller 0x000BA40E): single virtual call PTR_FUN_000e1174 on 0xE1114 object, only
when mode *PTR_DAT_000e10f8 == 0. Resolves to 0x000E2682. [I]

#### 0x000E106E `lockup_task3_dispatch` — dispatch
Third cyclic entry (caller 0x000BA416): when mode *PTR_DAT_000e10f8 == 0 fires six virtual
pre-calls (0xE1178/0xE111c/0xE1134/0xE1188/0xE1124/0xE113c) then 0x000E10A6; resolved callees
include 0x000E14EC, 0x000E11D0, 0x000E0760 (sequence runner), 0x000DE494 (setpoint follower)
and 0x000DE688 (engagement runner). [O]

#### 0x000E10A6 `lockup_status_publish` — logic
Publishes sequence status: reads vflag DAT_000e1198 on 0xE112c; when set (or fallback
*PTR_DAT_000e11a8) calls the function pointer at PTR_PTR_000e11a4 with two dereferenced bytes
(0xE119c/0xE11a0) plus state flag — sink callback registration. Calls 0x000E0272. [I]

#### 0x000E11AC `lockup_ramp_ctx_ctor` — entry
Allocates a 0x10-byte context via allocator PTR_FUN_000e1414 (0x0009AA34) when called with
NULL, then initializes u16[0]=0x5DC (1500) and state byte[2]=1. callers=-, reached from a
pointer table. [O]

#### 0x000E11D0 `lockup_ramp_dispatch` — dispatch
Gear-selected ramp driver from task 0x000E106E: gear *PTR_DAT_000e1418 in {6,8,9} → 3-state
integrator SM 0x000E13DE, else 0x000E11FA; finally copies computed u16 ctx[2] (+4) to output
ctx[0] via 0x000E14C0. [O]

#### 0x000E11FA `lockup_ramp_init_state1` — stub
Sets ramp state ctx+2 = 1 and calls state-1 enter 0x000E1304. Called from 0x000E11D0 and main
task 0x000E0FE4. [O]

#### 0x000E1200 `lockup_ramp_s3_hold_pred` — logic
In ramp state 3 (ctx+2==3): 1 when scaled counter (0xE0141c-obj +0x25c via 0xE1420) <=
ctx+6−ctx+4 AND map(obj+0x268, short 0xE1428) result <= getter 0xE142c; else 0. Called from
gear tracker 0x000E02A6. [I]

#### 0x000E1260 `lockup_ramp_load_cnt` — data-accessor
Loads u16 ctx+0xC from 0xE0141c-obj +0x264 — state-1→2 transition payload. [O]

#### 0x000E126E `lockup_ramp_state2_tick` — logic
State-2 body: reloads ctx+6 from short 0xE1428 and decrements dwell counter ctx+0xC. [O]

#### 0x000E1286 `lockup_ramp_state3_enter` — logic
State-3 entry: ctx+6 = ctx+4 = short 0xE1428 and seeds i32 integrator ctx+8 = ctx+6 ×1000. [O]

#### 0x000E129C `lockup_ramp_integrate` — logic
State-3 integrator: ctx+8 += (obj +0x254 short) × (map(obj+0x268, 0xE1428) − getter 0xE142c),
floored at (obj +0x25c + ctx+6)×1000, then converts back /1000 into u16 ctx+4. [O]

#### 0x000E1304 `lockup_ramp_state1_enter` — stub
State-1 (re)entry: ctx+6 = short 0xE1428, ctx+4 = 0x7FFF (saturated max), ctx+8 = 0x1F3FC18. [O]

#### 0x000E131C `lockup_ramp_s1_exit_pred` — logic
State-1 exit: 1 when obj 0xE0141c +0x260 flag set AND torque-rise predicate 0x000E1372. [O]

#### 0x000E1340 `lockup_pred_not_ramp_ready` — stub
Returns !0x000E1372() (state-2 exit test in 0x000E13DE). [O]

#### 0x000E135A `lockup_ramp_s3_exit_pred` — logic
State-3 exit: 1 when short 0xE1438 < ctx+4 − 0xE0141c-obj +0x258 value. [O]

#### 0x000E1372 `lockup_pred_torque_rising` — logic
Shared predicate: *0xE143c set, map(obj+0x268, 0xE1428) result <= getter 0xE142c, byte
obj+0x26c < *0xE1440, and map(float *0xE1444 × DAT_000e1448) above short at obj+0x270. [I]

#### 0x000E13DE `lockup_ramp_sm` — dispatch
Three-state ramp SM on ctx+2 (1→2→3→1): state 1 waits on 0x000E131C then loads count
0x000E1260; state 2 ticks 0x000E126E until timer 0x000E1350 expires → state 3 via
0x000E1286; state 3 integrates 0x000E129C until 0x000E135A. Invalid state → event 0x47A
(1146) log. Called from 0x000E11D0. [O]

#### 0x000E14C0 `lockup_ramp_out_copy` — stub
Copies computed u16 ctx[2] (+4) to output u16 ctx[0]. Tail of 0x000E11D0. [O]

#### 0x000E14D0 `lockup_ctx3_reset` — logic
Zeroes 12 fields of the third lockup context (bytes [0],[1],[2],[6],[0x18],[0x19], u16 +8/
+0xa/+0x10/+0x12) — per-reset init from main task 0x000E0FE4. [O]

#### 0x000E14EC `lockup_apply_ctrl` — logic
Apply-pressure wrapper (123 instr) from task 0x000E106E: when enable obj 0xE16e4 +0x2dc flag
or ctx[0] clear or state 4 → hold floor (0xE16e8+2); else clamps ramp ctx+8 to 0..0x9C4
(2500) with limit ctx+0x12−floor (latching +0xe) or floor from map obj+0x2c4; alt path loads
ctx+0x14/+0x16 from vmaps 0xE16fc/0xE1700 and runs 0x000E19D0. Ends with /10 conversion into
ctx[1] and runs SM 0x000E15F0. [I]

#### 0x000E15F0 `lockup_apply_sm` — dispatch
Six-state apply SM on ctx+6 (0..5) inside 0x000E14EC: state 0 entry guard 0x000E1876 → 1;
guard 0x000E190E gates every state (→5, 0x000E17FA); 0x000E1848 → 4; state 2 body 0x000E17C6
with exit 0x000E1966 → 3 (0x000E180E); state 4 body 0x000E1756; state 5 exit test
0x000E18D2 → 0. Invalid state → event 0x44F (1103). [O]

#### 0x000E1738 `lockup_apply_s4_enter` — logic
State-4 entry: ramps ctx+0xA = ctx+8×10, clears ctx+0xC, loads dwell u16 ctx+0x1A from
0xE01994-obj +0x2E4. [O]

#### 0x000E1756 `lockup_apply_s4_tick` — logic
State-4 body: ctx+0xA += short at 0xE01994-obj +0x2C8, clamped by helper 0xE1998 (0xffff9e58/
0x61a8 table), decrementing dwell ctx+0x1A. [O]

#### 0x000E1790 `lockup_apply_s2_enter` — logic
State-2 entry: loads dwell u16 ctx+0x1A from obj +0x2E0 and ramp rates ctx+0x10/ctx+0x12 from
maps on obj +0x2CC/+0x2D0 with short 0xE199c. [O]

#### 0x000E17C6 `lockup_apply_s2_tick` — logic
State-2 body: ctx+0xA += rate ctx+0x10, clamped by helper 0xE1998, decrementing dwell
ctx+0x1A. [O]

#### 0x000E17FA `lockup_apply_s5_enter` — logic
State-5 entry: clears ctx[0]/[2] and loads dwell u16 ctx+0x1A from 0xE01994-obj +0x2FC. [O]

#### 0x000E180E `lockup_apply_s3_enter` — logic
State-3 entry: loads dwell u16 ctx+0x1A from 0xE01994-obj +0x2F8. [O]

#### 0x000E181C `lockup_apply_s1_enter` — logic
State-1 entry: ctx[0]=1, ctx[2]=1, clears +0xE, zeroes ramp u16 +0xA, dwell u16 +0x1A from
obj +0x300. [O]

#### 0x000E1848 `lockup_apply_abort_pred` — logic
Returns 0 (abort apply) when (state != 3 OR dwell +0x1A != 0) AND 0xE19a4[0x14] == 0 AND
0xE19a4[0x1c] != 0 AND short 0xE19a8 < 1; else 1. [O]

#### 0x000E1876 `lockup_apply_s0_entry_guard` — logic
State-0 entry guard: *0xE19ac set, counter 0xE19a8 <= 0xE01994-obj +0x2F0, pedal byte 0xE19b0
<= obj +0x2E8, vflag 0xE19b8 clear, and *0xE19bc/*0xE19c0 both set. [O]

#### 0x000E18D2 `lockup_apply_s5_exit_pred` — logic
State-5 exit: 1 unless (dwell +0x1A != 0 OR obj +0x2F4 flag) with *0xE19c4 clear and
(byte 0xE19c8 < 6 OR obj +0x2F4 flag clear). [O]

#### 0x000E190E `lockup_apply_gate_guard` — logic
Per-state gate: skips (returns 1) while state 4 with dwell and +8 > 0; returns 0 (proceed to
state 5) when pedal byte 0xE19b0 within obj +0x2E8/+0x2EC window, vflag 0xE19b8 clear, and
short 0xE19cc < ctx+0x12. [O]

#### 0x000E1966 `lockup_apply_s2_exit_pred` — logic
State-2 → 3 test: 0 when dwell +0x1A != 0 AND *0xE19c0 set AND +0xE == 0 AND +0x18 == 0. [O]

#### 0x000E19D0 `lockup_apply_alt_calc` — logic
Alt-path computation from 0x000E14EC: latch +0x18 = (u16 +0x14 <= 0xE1a34-obj +0x2D4 value);
when set and +0x16 > 0x14, sets ramp +8 = +0x16−0x14 and +0xA = (+8−+0xC)×10 (unless state 4
already latched via +0x19), zeroing +8 below 0. [I]

#### 0x000E1A38 `lockup_speedobs_ctx_ctor` — entry
Allocates the 0xB0-byte speed-observer context (allocator 0x0009AA34 via 0xE1d74) and seeds
u16 [0]/[0x18]/[0x19] = 0x20 (32). callers=-, ctor via pointer table. [O]

#### 0x000E1A60 `lockup_speedobs_init` — logic
Per-reset init from 0x000E0FE4: copies speed short from 0xE1d78-obj +0xFC into [0]/[0x18]/
[0x19] with float mirror +0x18, seeds slew counters +0x3C/+0x3E/+0x40 (+0x32) ×300 from obj
+0xFC/+0xFA/+0xF8, ramps +0x42/+0x4D via 0xE1d7c, and computes [4]=[0]−[8]. [O]

#### 0x000E1AF6 `lockup_speedobs_update` — logic
265-instr speed-observer update: target +0x1A = map(0xE1d84-obj +0xA8, (+0x18−+0x19)/2) +
obj+0xFC history; offset +0x13 = map(+0xB0,[1])×target/10000 + byte obj+0xB4; slews +0x3C→[0]
(+0xC float), +0x42→[8]/+0x40, +0x4D→[9] (floor 0x20)→+0x30→[0x12]; ratio [0xA]=[8]×[0x30]/
[9]; then shifts the delta history +0x1C..+0x26 and runs a 5-tap IIR with coefficients at
0xE1db4 to produce filtered output +0x16. [I]

#### 0x000E1DB8 `lockup_range_dispatch` — logic
From main task 0x000E0FE4: copies ctx[0]→+0x10 then runs range pipeline 0x000E202C,
SM 0x000E20A6 and table mapper 0x000E2124 on global context 0xFFF8B6AC. [O]

#### 0x000E1DD4 `lockup_range_s6_enter` — logic
Range state-6 entry: +0x13 = 0xE2004[0x29], +2 = 0xE2004[0x23], state +3 = 6. [O]

#### 0x000E1DEA `lockup_range_s1_enter` — logic
Range state-1 entry: +0x13 = *PTR_DAT_000e2008, state +3 = 1. [O]

#### 0x000E1E00 `lockup_range_pos_update` — logic
104-instr position tracker: detects transitions between tracked +2/+0x13 and 0xE2004[0x23]/
[0x29], maps through the 9-wide table PTR_LAB_00063484_1 (index pos+sub×9) into +1, sets
state 4, manages debounce +0x19 and counter +0x1B via vmap 0xE2018 (selecting 0xE2020-obj
+0x174/+0x178/+0x17c) and conversion 0xE2024 into +0x16. [I]

#### 0x000E1F0A `lockup_range_tgt_reload` — stub
Reloads +0x13 = *PTR_DAT_000e2008. Called from range SM 0x000E20A6. [O]

#### 0x000E1F14 `lockup_range_s2_enter` — logic
Range state-2 entry: +1 = table[*PTR_DAT_000e2008 + ctx+2 ×9], state +3 = 2. [O]

#### 0x000E1F30 `lockup_range_reset` — logic
Range reset: state +3 = 6, +2 = *PTR_DAT_000e2008, +1 = 0. [O]

#### 0x000E1F40 `lockup_range_moving_pred` — logic
Returns 1 when +0x14 clear and tracked +2 differs from both 0xE2004[0x23] and [0x29]. [O]

#### 0x000E1F64 `lockup_range_left_lo_pred` — logic
Returns 1 when +0x13 != 0xE2004[0x29] AND +2 == 0xE2004[0x23]. Twin at 0x000E1F82 compares
against [0x29]. Cluster count 2. [O]

#### 0x000E1FA0 `lockup_range_hold_pred` — logic
Hold predicate: conversion 0xE2024 over 0xE2020-obj +0x170/+0x16C (selected by *0xE2028) with
*0xE201c; returns 1 when that set AND (+0x18 OR +0x15). [I]


#### 0x000E1FE8 `lockup_range_wait_done_pred` — logic
Returns 0 (keep waiting) while counter +0x1B != 0 and 0xE2004[0x29] != [0x23]; else 1. [O]

#### 0x000E20A6 `lockup_range_sm` — dispatch
Range state machine on 0xFFF8B6AC context: waiting latch +0x1A with 0x000E1FE8 → reset
0x000E1F30; left-lo predicates 0x000E1F64/0x000E1F82 select state 1 (0x000E1DEA) or reload
0x000E1F0A; hold 0x000E1FA0 → state 2 (0x000E1F14 + debounce clear 0x000E1DF8); moving
0x000E1F40 → state 6 (0x000E1DD4); always ends in position update 0x000E1E00. [O]

#### 0x000E2124 `lockup_range_map_update` — logic
Maps current range state through the 9-wide table PTR_LAB_00063484_1 (row *0xE21c4, col +2)→[0],
converts (val+2) via 0xE21c8 into +0x11/+0x12, bumps zero sub-positions, and computes float
ratios +4/+8/+0xC from 0x000E2194 on +2/+0x13. [I]

#### 0x000E2194 `lockup_range_ratio_lookup` — library
Two-step lookup: conversion 0xE21cc(value) then map 0xE21d4 → u16 / DAT_000e21d8 as float. [O]

#### 0x000E21DC `torque_req_ctx_ctor` — entry
Allocates the 0x1C-byte torque-request context (allocator 0x0009AA34 via 0xE2490) and seeds
state bytes +2/+3 = 0, u16 +0xC = 0x3C (60), byte +0xE = 0x7D (125). callers=-, ctor via
pointer table. [O]

#### 0x000E2204 `torque_req_mode1_sm` — dispatch
Four-state sequencer (state +1) for mode 1: state 1 loads count [6] from 0xE2494-obj +0x120 and
tests 0xE2498([0],[5],2); state 2 waits u32 +0xC (from obj +0x128); state 3 recomputes [5] via
map 0xE249c on float +8, reloads [6] (obj +0x124), resets and calls clear 0x000E23B0; invalid
→ event 0x469 (1129) log. Driven from 0x000E24C0. [I]

#### 0x000E22D8 `torque_req_mode2_sm` — dispatch
Structural twin of 0x000E2204 for mode 2: state lives at +3, done flag +7, count from obj
+0x12C, event code 0xFF. Driven from 0x000E24C0. [I]

#### 0x000E23B0 `torque_req_ctx_clear` — logic
Clears the 0x1C-byte torque-request context (fields [0],[1],[2],[3],[5],+7,+0xF, dwords +8/
+0x10/+0x18/+0xC). Shared by 0x000E24C0, both mode SMs and 0x000E2440. [O]

#### 0x000E23CE `torque_req_scaled_max` — library
Returns max(ctx[5], byte at 0xE2494-obj +0x134 × char *0xE24a8) using abs-compare helper
0xE24ac. Called from 0x000E2B1A. [O]

#### 0x000E2408 `torque_req_floor_calc` — library
Same abs-max pattern as 0x000E23CE but against ctx[0]; yields the request floor value. Called
from 0x000E2682. [O]

#### 0x000E2440 `torque_req_flag_set_clear` — logic
Sets +0xE from map 0xE24b8 on 0xE2494-obj +0x34 with params 0xE24b4/0xE24b0, then clears the
context via 0x000E23B0. Called from 0x000E2682 and 0x000E27D2. [O]

#### 0x000E2464 `thunk_torque_req_ctx_clear` — stub
Compiler-generated thunk of 0x000E23B0 (identical body, separate entry). Callers 0x000E2682/
0x000E27D2. [O]

#### 0x000E2468 `torque_req_ramp_update` — logic
Ramps the request: slew helper 0xE24bc on (+8, [0], [6], +7, 10) then maps 0xE249c over the
result into [5]. Called from 0x000E2682. [O]

#### 0x000E24C0 `torque_req_coord` — dispatch
179-instr dual-mode coordinator: disabled (0xE264c-obj +0x130 flag) → clear; mode byte
0xE2650[0x59] (1/2) picks mode SM 0x000E2204 (fields +0xF/+1) or 0x000E22D8 (+0xF/+3), each
seeding [0] = obj(+0x118/+0x11c) byte × *0xE2664; eligibility gate uses 0xE2654 <2,
*0xE2658, gear *0xE265c in {6,8} and 0xE2660[5] in {3,4}; on finish maps 0xE2668 into float
+8; sign flip when 0xE2674[1] != [0xC]; event 0x469 on invalid state. callers=- via 0x000E2646
thunk from 0x000E2682. [I]

#### 0x000E2640 `lockup_task4_thunk` — stub
Tail-call wrapper: (*DAT_000e2678)(ctx + 0x18) — cyclic entry 0x000BA426 forwarding into the
0x10C000-area library. [I]

#### 0x000E2646 `thunk_torque_req_coord` — stub
Compiler thunk of 0x000E24C0 (identical body). Called from 0x000E2682. [O]

#### 0x000E267C `lockup_task1_thunk` — stub
Tail-call wrapper: (*DAT_000e28e4)(ctx + 0x10) — called from main task 0x000E0FE4; target
resolves to 0x0010C402. [O]

#### 0x000E2682 `torque_req_tick` — logic
161-instr cyclic tick (from 0x000E105C): runs pre-scan 0x000E2984 and vtable refresh on
sub-objects +0x10/+0x5c; when 0xE28f4[0x14] clear just refreshes; else debounce +0xA4, gate
gear != 7, then computes floor (0xE28f8-obj +0x74 map/0xE2900), ramp limits (obj +0x78/+0x7c),
runs coordinator thunk on +0x88, blends result via 0x000E2AFC into the +0x50 output object
and closes with 0x2B1A post-processing. [I]

#### 0x000E27D2 `torque_req_gear_reset` — logic
Gear-driven reset of the torque-request module (gear *PTR_DAT_000e2958): 7 → reload debounce
+0xA4 from maps on 0xE2968[gear byte] tables and reset sub-objects +0x10/+0x5c/+0x88 (incl.
0x000E2440/0x000E2464); 8/10/4 → partial vtable resets; other → event 0x473 (1139) log. Always
clears +0xC, refreshes +0x50 and runs post-step 0x000E2B1A. Callers in 0x10A-0x10C area. [I]

#### 0x000E289E `torque_req_reset_log` — stub
Logs event 0x473 (1139, 0xF, 1, 1, 0xF8), clears +0xC, refreshes +0x50 and runs 0x000E2B1A;
uses unaff r14 → compiler-split tail of the 0x000E27D2 invalid path. callers=-. [I]

#### 0x000E28D4 `torque_req_enabled_pred` — stub
Returns **(0xE28f8-obj +0x130) == 1. Called from sequence guard 0x000E0A0C. [O]

#### 0x000E2984 `torque_req_prescan` — logic
Pre-scan latch: sets ctx[0] = 1 when 0xE2b7c-obj +0x1c flag set, gear 0xE2b80[4] in 5..9 and
speed windows match (0xE2b84/0xE2b88 vs obj +0x14/+0xC, with +0x18/+0x10 byte slack on the
hold test). From tick 0x000E2682. [I]

#### 0x000E2A0A `torque_req_status_bits` — logic
Packs status bits ctx+4: bits 0/1 from mode bytes +0x8A/+0x8B == 2 with slip in window
(ctx+0x16 ± obj+0x110/+0x114 vs 0xE2b84+8), bits 3/4 from +0x10/+0x11 with window
obj+0x138/+0x13c; whole byte cleared when gear 0xE2b80[4] is 7 or outside 5..8. [I]

#### 0x000E2AFC `torque_req_pick_bound` — library
Direction-select bound: when *0xE2b8c == 0 returns the smaller (a<b→a), else the larger of
the two shorts. Shared by 0x000E2B1A/0x000E2682. [O]

#### 0x000E2B1A `torque_req_output_calc` — logic
Output assembly: status bits 0x000E2A0A; when *0xE2b90 == 0 sums vget 0xE2b94(+0x88),
bound-pick 0x000E2AFC with +0x5C and vget 0xE2b98(+0x54) into u16 +6; final request +2 = +6 +
+0x14 (forced to +0x14 alone when ctx[0] == 1); float mirror +8. [O]

#### 0x000E2C1C `lockup_press_pred_low` — stub
Returns **(0xE2d3c-obj +4) float <= *PTR_DAT_000e2d50. Caller 0x000BA2DC (registration). [O]

#### 0x000E2C2E `lockup_press_plaus_run` — logic
248-instr pressure plausibility monitor (from 0x000E0FE4/0x000E2EB2): float ramp counters
+0x10/+0xF/+0xE/+0xD with latches +0x31/+0x32, converts through limiter 0xE2d60/quantizer
0xE2d68 into u16 +6/+0xA, blends averaged counts (÷DAT_000e2fd8) with vmap offsets 0xE2fe0/
0xE2fec, applies slew 0xE2ff4 against 0xE2ff0-obj +0x108/+0x10C, floors at 0xE2d54-obj +0xF0,
publishes via setters 0xE3000/0xE3004, copies ref u16 +0x114 into +0x44 and forces output +2
to 0x21C (540) when status [1] == 2. [I]

#### 0x000E2EB2 `lockup_press_plaus_dispatch` — logic
Monitor dispatcher from main task 0x000E0FE4: latches +0x24 on vflag 0xE300c with ref +0x44;
resets counts when idle flag 0xE3010[0xD4] clear; runs continuous filter 0x000E2C2E while
ramp active, else publishes held +6/+0xA, sets +0x31, and runs hold chain 0x000E2F46 +
0x000E31DA. [I]

#### 0x000E2F46 `lockup_press_hold_calc` — logic
242-instr hold-path computation from 0x000E2EB2: branch on gear <5 vs >=5 builds base
param_1[3] from maps 0xE3028/0xE3294 or 0xE2ff4/0xE3030 chains over 0xE2ff0/0xE3280 objs,
scales ratio fVar12 (*0xE3014/DAT_000e3018), blends ramps 0xE32a4/0xE32a8 with floors at
0xE32ac-obj +0xF0, quantizes 0xE32bc/0xE32c4 into u16 +6/+0xA, publishes via 0xE32CC/0xE32D4,
copies +6→+2 with 0x21C override when status [1] == 2. [I]

#### 0x000E31DA `lockup_press_hold_status` — logic
Hold-path status: one-shot latch DAT_fff8b7b8/first gear snapshot DAT_fff8e6c8 from 0xE32ac[0xA0];
counts down +0x44; sets transition flag +0x14 when leaving gear 6/8; result +4 = 2 when
0xE32ac[0x50] == 2 with flags [0xCA]/[0xCD]/[0xCF] and counter expired. [I]

#### 0x000E32D8 `diag_ramcheck_ctx_ctor` — entry
Allocates the 0xC-byte RAM-check context (allocator 0x0009AA34 via 0xE3554) and seeds +1=0xFF,
+3=0, dwords +4/+8=0. callers=-, ctor via pointer table. [O]

#### 0x000E32FE `diag_ramcheck_run` — logic
209-instr RAM-block plausibility check (caller 0x000B8596): allocates two size-class-2 blocks
and distributes 0x0E/0x1B/0x28/0x35/0x42/0x4F/0x5C/0x85 offset pointer arrays across 0xE355c/
0xE3564 objects; compares each block pair against reference 0xE3560-obj latching status 2;
final result 1 (ok) / 2 / 3 (size) / 4 via 0x000E36A8, with event 0x1B1 (433) log and
callback 0xE3580 when a change is detected. [I]

#### 0x000E34AA `diag_ramcheck_evt_log` — stub
Logs event 0x1B1 (433, 0xF, 5, 4) and latches ctx[0] = 1; uses unaff r14 → compiler-split
tail fragment. callers=-. [I]

#### 0x000E3546 `diag_ramcheck_set_result` — stub
Tail-jump into 0x000E36A8 (result setter). Called from 0x000E32FE. [O]

#### 0x000E358C `diag_check_result_get` — stub
Returns ctx byte +2. Eight identical copies: 0x000E358C, 0x000EAC5E, 0x00130294, 0x00135CE8,
0x001395DE, 0x0013BC84, 0x001419D0, 0x001485C2 — per-module status getters spread across
regions; caller of this copy is 0x000B81CE. [O]

#### 0x000E3592 `diag_ramcheck_status_merge` — logic
133-instr status merger (caller 0x000B858E): drives the seven block-status dwords of
0xE36f8-obj (0/1, keeping latched 2) from enable flags 0xE36f4[0]/ctx[0], maps combined state
through table 0xE3704[cond + flag×2] into ctx[1], and propagates 0/1/2 to 0xE36fc-obj. [I]

#### 0x000E369E `diag_const6_get` — stub
Returns constant 6. Caller 0x000B8396. [O]

#### 0x000E36A2 `diag_ramcheck_stage4_set` — stub
Writes stage marker 4 into ctx+4. Called from RAM-check run 0x000E32FE. [O]

#### 0x000E36A8 `diag_ramcheck_result_set` — logic
Result setter: clears ctx[0]; on first call (latch +3) logs event 0x1B1 (433, 0xF, param_2,
4, 0x35) via 0xE370c. Reached through 0x000E3546 from 0x000E32FE. [O]

#### 0x000E36C6 `chan_evt433_frag` — logic
Logs event 0x1B1 (433, 0xF, 4, 0x35) and latches +3 = 1 on an unaff-r14 context — compiler-
split tail fragment. callers=- via 0x000E436A. [I]

#### 0x000E3710 `chan_ctor` — entry
Constructs the 0x60-byte channel object (allocator 0x0009AA34 via 0xE3958): vtable
PTR_LAB_000e3960, config ptr +0x10, channel id +0x32 validated 1..5 (else event 0x19F=415
(13,5,0xFC)), state +0x34 = 6, latches +0x55/+0x58 = 1, clears u16 timers +0x36..+0x48 and
byte flags. Five instances (ids 1..5) are built by 0x0015C064 at RAM 0xFFF8B7C8/0xFFF8B828/
0xFFF8B888/0xFFF8B8E8/0xFFF8B948. [O]

#### 0x000E3836 `chan_service` — logic
Main per-cycle service (callers=-, via vtable 0xE3960): sets latches +0x57/+0x52/+0x56 from
vflags 0xE397c/0xE3984, id==2 path calls vtable+0x38 pred, E4994 map and E3998 action with
u16 +0x46; then time update 0x000E3E28 and the stepping loop while 0x000E3CE8: output
0x000E3CA0 → state step 0x000E38F2 → state actions 0x000E39DC; closes with timer latch
0x000E3C1E. [I]

#### 0x000E38F2 `chan_state_step` — dispatch
State transition table for state +0x34 (0..6) keyed on enable flag +0x53: 0→1, 1→2, 2→3,
3→4, 4→5 or reset to 0, 5→1/0, 6 stays; invalid → event 0x19F (1,7,0x74). [O]

#### 0x000E3902 `chan_state_step_regarg` — dispatch
Register-argument clone of 0x000E38F2 (same state table, params in r0/sr per Ghidra); kept
as separate analysis entry. callers=-. [I]

#### 0x000E39DC `chan_state_actions` — logic
250-instr per-state actions: state 0 clears the shared bitmap bit (1<<(id−1) at 0xE3c68),
resets u16 +0x46 and loads timer +0x18 = map(0xE3c74)+0x1C ×100; state 1 sets both bitmap
bytes, +0x4E=1, runs 0x000E4086 and vtable 0xE3c84; state 2 sets bitmap, copies +0x46→+0x48;
state 3 clears hi/lo bytes via 0xE3c88, resets latches +0x55/+0x57/+0x56/+0x5B, sets +0x48 =
10000 and +0x46 = 10000, reloads +0x14 ×100 when +0x51/+0x18; state 4 clears bitmap and
timers +0x14/+0x1C; state 5 starts map chain 0x000E447A. [I]

#### 0x000E3C1E `chan_timer_latch` — logic
Edge-latches timers: +0x1C = +0x14 (clearing +0x51/+0x20/+0x18 when falling), +0x20 = +0x18
(setting +0x51 when rising); clears +0x57 while in state 3. [O]

#### 0x000E3CA0 `chan_state_output` — logic
State-1 output through vtable target 0xE3e92 with u16 +0x2C (0 when +0x53 set); state 5 sets
latch +0x57 when global *_LAB_000e3e98 == 1 (clearing +0x30). [O]

#### 0x000E3CE8 `chan_state_done_pred` — logic
137-instr per-state completion predicate: state 0 on +0x53; state 1 when u16 +0x3A <= +0x2C;
state 2 via vtable+0x38 pred and +0x3C != 0; state 4 when +0x40 >= +0x42 (overridden when
+0x5A==1 and map 0xE3ea0 of obj+0x9C byte ×10 exceeds +0x2C); state 5 via vtable+0x2C;
invalid → event 0x19F (1,7,0x1A). [I]

#### 0x000E3E28 `chan_time_update` — logic
Computes u16 time +0x46 per state: state 1/2 delegate to window update 0x000E4086; state 2
blends (u32 +0x3C × +0x48)/10000 against +0x2C via 0xE3eac; state 3 pins 10000; state 5 loads
+0x38 via 0xE4104/0xE4108, computes scaled term +0x3E (maps 0xE4110/0xE4114 ×10) and blends
(0xE4120); invalid → event 0x19F (1,9,0x53). [I]

#### 0x000E3F84 `chan_ramp_update` — logic
Computes u16 ramp +0x40 per state: state 2 = +0x42 + 0x000E4C8E; states 3/4 clamp vmap
0xE4130 / 0x000E4CCC against u16 +2, fall back to map 0xE4138 by id and slew 0xE4140 on
+0x24 capped at +0x42; state 5 = (+0x46 × +0x42)/10000; invalid → event 0x19F (1,10,0x34). [I]

#### 0x000E4086 `chan_windows_update` — logic
Refreshes timing windows: base 0x000E47CA → +0x36, scale 0x000E476E → +0x38, temp offset
0x000E44B6 → +0x3A, and delta +0x3C = quantize(+0x36 − +0x3A) via 0xE4118. Called from
states 1/2 of 0x000E39DC and 0x000E3E28. [O]

#### 0x000E40DA `chan_reqmode_dispatch` — dispatch
Jump-table body on request mode +8 (0..0xF): modes 0/2/5/7/9 clear flags +0x4C/+0x4D/+0x52
and +0x5B (modes 0xB/0xC instead OR in predicate 0x000E4E38); modes 1/3/6/8/10 load +0x4C/
+0x4D from vflags 0xE433c/0xE4340 and OR +0x5B with 0x000E4E38; modes 4/0xD/0xE and default
(pre-named caseD_4 at 0x000E41E6) log event 0x19F (8,0xB,0x66). callers=- (table). [O]

#### 0x000E41E6 `caseD_4` — stub
Pre-named switch case of 0x000E40DA: single event log 0x19F (415, 0xF, 8, 0xB, 0x66). Name
kept as found. [O]

#### 0x000E4214 `chan_blend_output` — logic
Computes the blended u16 output [2]: base from maps 0xE4358 over obj(+0x10) fields +4/+8/+0xC
with profile bytes 0xE4350(+0xF/+0x18), headroom 0x000E4C8E + [0x42] (mode 0xF adds 0xE4360),
proportional blend at (delta & 0xFFFF) < 10, id<5 override to constant LAB_00004e20 via
0x000E4DC8, quantize 0xE4368 into [1]/[2] and publish channel id into the global slot behind
FUN_000e436a. [I]

#### 0x000E436A `chan_out_update_tbl` — logic
Table-variant output update (calls 0x000E36C6 first): by mode [9] and flag [7], u16 [1] =
[0] + [0x21] + 0x000E4C8E, clamped against 0xE45a8 into [2]; negative sums log event 0x19F
(6,0xC,0xA4) and force 1. callers=-. [I]

#### 0x000E4374 `chan_out_update` — logic
Same output update as 0x000E436A without the leading 0x000E36C6 call — the two are
alternative entry points of one routine. [O]

#### 0x000E4424 `chan_override_latch` — logic
Latch +0x5A: from vtable+0x38 pred when mode +8 == 2 (else forced when +0x53 set, cleared
when 0xE45b8 <= byte at 0xE45b4-obj +0x3C); value from vflag 0xE45b0. callers=-. [I]

#### 0x000E447A `chan_map_by_id` — data-accessor
u16 +0x5C = map 0xE45bc(channel id +0x32). Called from states 5 of 0x000E39DC/0x000E47CA. [O]

#### 0x000E4490 `chan_active_flag_upd` — logic
Active flag +0x53 = 1 unless [0] == 0 and +7 set; on change runs edge hook 0x000E4522. [O]

#### 0x000E44B6 `chan_temp_offset_calc` — library
Temperature-offset term: maps 0xE45c4 on obj(+0x10) +0x58 and (when flagged) +0x5C (−0x80
signed), combined via 0xE45c8, ×10 and floored at u16 +0x2E (calling 0x000E5164 when below). [I]

#### 0x000E4522 `chan_active_edge` — logic
On activation edge: clears +0x52 then sets it from vflag 0xE45cc == 1. Called from
0x000E4490. [O]

#### 0x000E4548 `chan_bitmap_update` — logic
Sets +0x4F: 0xFF when vtable pred 0xE45d0 on obj+0x38 fails or +0x4E != 1, else scaled
(0x000E4AB0 result or 0xE45d8)/10 through 0xE45dc; then bit-tests the global bitmap 0xE4824
by id and clears +0x4F when the id bit is absent. [I]

#### 0x000E4636 `chan_bit_check` — logic
Three-way check: vpred 0x000E4E4A; vtable 0xE4838 pred on 0xE4834-obj (+0xC by id) or base
value 1/2 (value 2 runs 0xE4840 map and 0x000E51C8). [I]

#### 0x000E469C `chan_adj_lookup` — logic
Adjustment lookup by param_2: 1/2 pick maps 0xE524C/0xE52AA on obj(+0x10) (only id <5, else
event 0x19F (…,0xE,0x76/0x58/0x99)), 3 delegates to 0x000E4752, 4/0 no-op. [I]

#### 0x000E470A `chan_identity_ret` — data-accessor
Register-argument identity: returns in_r0 unchanged. callers=- (vtable filler). [O]

#### 0x000E4752 `chan_map_e484c` — data-accessor
Single map 0xE484c on obj(+0x10)+0x20 with params 0xE483c[0xF]/+0x16. Case 3 of
0x000E469C. [O]

#### 0x000E476E `chan_scale_calc` — logic
Scale term: base byte 100, or obj+0x48 byte (+0x28 byte for id 5 with vflag 0xE4850), times
float 0x000E5236 ÷ DAT_000e4854 through 0xE4858. [O]

#### 0x000E47CA `chan_base_sum` — logic
166-instr base-value assembly: id-map 0x000E447A; id-5 special path maps 0xE485c/0xE4834-obj
(+0x30/+0x34), else maps 0xE4a74 on obj(+0x10) +0x10/+0x14 and 0xE4a78 offset; adds
conditional terms 0x000E469C/0xE4a7c (when +0x2B/+0x2C profile flags), 0x000E476E scale,
+0x55 flag map 0xE4a84/0xE4a7c, +0x57 short obj+0x50, per-id map 0xE4a88 and +0x54 flag map;
floors at 0x000E44B6 result + +0x2E and quantizes 0xE4a8c. [I]

#### 0x000E497A `chan_force_update` — logic
Forced refresh: sets time +0x46 = param_2, clears the shared bitmap bit and +0x4E, sets
state +0x34 = 0 / 1 / 3 (3 when param == 10000), then runs state actions 0x000E39DC. Called
from service 0x000E3836. [O]

#### 0x000E4A4E `chan_map_e4aac` — data-accessor
u16 +0x42 = virtual map 0xE4aac on (0xE4aa8, obj, id, mode +8). Caller 0x000DC4C4 (lockup
engagement SM region). [O]

#### 0x000E4AB0 `chan_scaled_value` — logic
State-scaled value: base 0x000E47CA then state 1/2/5 apply temp offset 0x000E44B6 and
proportional (+0x46 /10000) reduction via 0xE4d58/0xE4d5c (+0x2C term in state 1); states
3/4 return the base; invalid state → event 0x19F (1,0xF,0x28). [I]

#### 0x000E4BAE `chan_value_get0` — library
Forwarder: 0x000E4AB0(ctx,0,0,0,0,0,0). Caller 0x000B8306 (query interface). [O]

#### 0x000E4BC6 `chan_value_get_ctx` — library
Forwarder: 0x000E4AB0 with context flags +0x4C/+0x4D/+0x52/+0x5B. Called from 0x000E4548. [O]

#### 0x000E4BEC `chan_state3_tenth_get` — data-accessor
Returns u16 +0x2C / 10 when state == 3, else 0. Caller 0x000B836E. [O]

#### 0x000E4C00 `chan_delta_get` — data-accessor
Returns map 0xE4d68 of (u16 [2] − [0x42]). Caller 0x000B82E2. [O]

#### 0x000E4C18 `chan_headroom_calc` — logic
Headroom for mode 0/1: vtable+0x38 pred gate, value = 0x000E4CCC − [0x42] (minus 0x000E4C8E
in mode 0); other modes log event 0x19F (7,0xC,0xD5). Caller 0x000B8356. [I]

#### 0x000E4C8E `chan_map_e4d70` — logic
Map 0xE4d70 on obj(+0x10)+0x74 with 0xE4d6c; scaled ×(vflag 0xE4d78)/100 when mode +8 == 6.
Shared by five call sites. [O]

#### 0x000E4CCC `chan_map_e4d80` — data-accessor
u16 = virtual map 0xE4d80 on (0xE4d7c-obj +0x10, +0x4B). Shared query. [O]

#### 0x000E4CE2 `chan_short_at_98_get` — data-accessor
Returns **(short **)(obj(+0x10) +0x98). Two identical copies (0x000E4CE2, 0x000E51BC);
callers 0x00107D90/0x00107BD2. [O]

#### 0x000E4CF4 `chan_late_latch` — logic
With target-reached 0x000E4D92: in state 3 with param_2 set loads 0x000E530C result into
+0x31; sets +0x54 = 1 only for state 3 with +0x31 clear and target reached. [I]

#### 0x000E4D92 `chan_target_reached` — logic
Returns (**short **)(0xE4ff0-obj +0x14) + 0x000E4CCC result <= u16 +2 — target-quantity
reached test. [O]

#### 0x000E4DBA `chan_time_full_pred` — stub
Returns u16 +0x46 == 10000 (timer saturated). Caller 0x0010834C. [O]

#### 0x000E4DC8 `chan_id_lt5_pred` — logic
id <5: vflag 0xE4ff4 on 0xE4ff0-obj +0x20 with +0x33 == 1; id >=5 logs event 0x19F (5,0x10,
0x98). Called from blend output 0x000E4214. [O]

#### 0x000E4E30 `chan_pred_mode1` — stub
Returns byte +9 == 1. callers=- (query). [O]

#### 0x000E4E38 `chan_vpred_e5000` — stub
Virtual predicate (*DAT_000e5004)(0xE5000) != 0. Two identical copies (0x000E4E38,
0x000E4E4A). [O]

#### 0x000E510A `chan_evt415_log` — stub
Single event log 0x19F (415, 0xF, 0xC, 0x13, 0xB2). Caller 0x000E513C (outside slice). [O]

#### 0x000E08AC `lockup_seq_result_calc` — logic
Sequence end-of-run result (state-5 entry from 0x000E0D98): increments counter +0x14 via
getter/setter 0xE09e8/0xE09ec (with −1 guard), resets 0xE09f0, normalizes the accumulated
counters +0x18/+0x20/+0x10 by u16 0xE09b0-obj +0x20 through conversion 0xE09f8 into u16 +8/
+0x24/+0xC, sums +0xA = map(obj +0x54, +0x24) + +8, and when (+0xC − *0xE0a00) <= byte
obj +0x48 sets result +6 = convert(map(obj +0x50, +0xA)) else 0xFF. [I]

#### 0x000E202C `lockup_range_precheck` — logic
Range-module precheck from 0x000E1DB8: clears state +3 and transition flag +0x17; virtual
flag 0xE21b8 on 0xE21b4 with edge detection against +0x18 sets state 3 (+0x17) on rise and
state 5 on fall; likewise +0x15/+0x14 edge pair from vflag 0xE21bc. [O]

## Unit summary

Unit 29 (r0c_app, 0x000DE0FE–0x000E510A) is the torque-converter **lockup module** plus three
embedded satellite modules, all driven from cyclic task entries 0x000BA406/0x000BA40E/
0x000BA416/0x000BA426:

- **Lockup core** (0x000DE0FE–0x000DEAB4 + guards 0x000DF1FE–0x000DF7A6): converter module
  ctor/reset, 12-state engagement SM 0x000DEA5E (spec 09 §3.3) with event decoder
  0x000DF0A8, torque-window loader 0x000DF7FC and ~20 guard/predicate helpers.
- **Release/sequence logic** (0x000DF900–0x000E10A6): release sub-context update/eval, gear
  calculator, six-state sequence SM 0x000E0D98 with its guard set, output calc, and the three
  cyclic dispatchers 0x000E0FE4/0x000E105C/0x000E106E.
- **Ramp/apply/observer** (0x000E11AC–0x000E1AF6): 3-state ramp integrator (×1000 scale),
  6-state apply-pressure SM with clamp 0..2500, and the 0xB0-byte speed observer with a
  5-tap IIR delta filter.
- **Range tracker** (0x000E1DB8–0x000E2194): selector-range SM on global 0xFFF8B6AC with the
  9-wide transition table at 0x63484.
- **Torque-request module** (0x000E21DC–0x000E2B1A): dual-mode coordinator 0x000E24C0,
  two 4-state mode sequencers, ramp/blend output chain, ticked by 0x000E2682; talks to the
  0x10A000–0x10C400 library cluster.
- **Pressure plausibility** (0x000E2C1C–0x000E31DA): monitor dispatch 0x000E2EB2 with
  continuous-filter 0x000E2C2E and hold-path 0x000E2F46/0x000E31DA (0x21C override).
- **RAM plausibility/diag** (0x000E32D8–0x000E369E): block-compare run 0x000E32FE with result
  codes 1–4, event 433, status merger; eight `diag_check_result_get` copies spread across
  0x000E–0x0014 regions.
- **Channel manager** (0x000E3710–0x000E510A): five 0x60-byte objects (ids 1..5) with 6-state
  lifecycle, shared bitmap at 0xE3c68, ×100 timers, temperature-compensated map chains
  (0x000E47CA/0x000E4AB0), pre-named `caseD_4` kept at 0x000E41E6. Module identity beyond
  "five-channel lifecycle manager" is not asserted.

Counts by kind (220 clusters, 238 names): logic 134, stub 56, data-accessor 20, dispatch 15,
entry 6, library 7, irq 0, unknown 0. Unknown entries: none — but the `chan_*` family
(0x000E3710–0x000E510A) and several torque-request/pressure summaries are [I]-inferred;
their names stay generic on purpose.

Cross-region leads: (1) heavy indirect-call traffic into the 0x0010A000–0x0010C400 library
cluster (torque-request tick, setpoint slews, maps) — that unit should cross-check; (2) the
`diag_check_result_get` copies at 0x0010886A, 0x0010ADA4, 0x00130294, 0x00135CE8, 0x001395DE,
0x0013BC84, 0x001419D0, 0x001485C2 belong to other units' slices; (3) chan_ctor instances
built by 0x0015C064 (r14_app); (4) 0x000DC4A8/0x000DC4C4 (lockup engagement region) call into
chan queries 0x000E4C8E/0x000E4A4E — unit owning 0x000DC4xx should cross-check.
