# Unit 38 (r10_app)

Region 0x118494–0x11D97C. Dominant pattern: this slice is the shared **condition-object /
fault-monitor helper library** — C++-style classes whose evaluation methods are installed into
monitor trees by wiring functions in r0c_app (0xF43A8 et al., see spec 13 fault-reporting).
Second pattern: tiny factory constructors registered into RAM object tables (0x45/0x46 etc.).
Third pattern: slew/ratio math helpers. Batch entries follow unit order.

#### 0x00118494 `diag_cond_indirect_call` — stub
Calls the function pointer stored at literal 0x001184BC with the argument from 0x001184B8
(Ghidra-resolved target 0x000BC82C, an OS/resource-state check that returns 1 when state==0x80
and a flag is clear) and returns its byte result. Looks like a condition-object callback thunk
reached through a pointer pair. 7 instrs, callers='-' (indirect). [I] spec 13 monitors.
Cluster of 1.

#### 0x001184CC `diag_cond_fsm_arm` — data-accessor
Setter for the debounce-FSM object used with 0x001184D4: writes 1 to flags at obj+0xC and
obj+0xD (arm + restart request). Called only by r0c 0x000F31B2 (303-instr mode/gear handler,
which also calls the FSM stepper). [O] Cluster of 1.

#### 0x001184D4 `diag_cond_fsm_step` — logic
Five-state debounce/sequence condition machine (states 1..5) driven entirely through a method
table at obj+0x10 (getters at vt+0x14/0x20/0x2C/0x44, setter at vt+0x38). Uses mode selector
PTR_DAT_00118674, thresholds PTR_DAT_00118678[+0x94], flags PTR_DAT_0011867C/0x00118680 and
time base PTR_DAT_00118684 (adds `ticks*100` to the value at obj+8 — 10ms→1ms scaling).
State 3 arms on threshold pair, 4 waits, 5 re-latches. 199 instrs; single caller 0x000F31B2.
[I] spec 13 condition framework. Cluster of 1.

#### 0x0011868C `lib_scale_x100_store` — library
Stores `param_2 * 100` into *param_1 (u16→int scaling to 1/100 units); the same *100 scaling
the FSM at 0x001184D4 applies to its time base. Caller 0x000F684C (r0c). [O] Cluster of 1.

#### 0x00118696 `lib_slew_i16_step` — library
One-step integer slew limiter: `step = (target*100 - cur)/param_3` clamped to int16 range
(±0x7FFF), `cur += step`, returns step; guards param_3==0→1. No direct callers (indirect via
method table). Companion float version at 0x001186D2. [O] Cluster of 1.

#### 0x001186C8 `lib_fmul_store` — library
Stores `param_1 * DAT_0011870C` (ROM float constant) into *param_2 — pure float scale helper.
Caller 0x000F69D2 (r0c). [O] Cluster of 1.

#### 0x001186D2 `lib_fslew_clamp_step` — library
Float slew limiter: `step = int((param_1*k - cur)/param_2)` clamped to [DAT_00118710,
DAT_00118714], `cur += step`. Reached indirectly (callers='-'); int twin at 0x00118696. [O]
Cluster of 1.

#### 0x00118718 `diag_cond_gate_by_mode` — logic
Returns param_1 unchanged only when selector param_2 < 12 and global mode byte
(PTR_DAT_001187A8) is not 1, 7, or 2-with-special-cases; otherwise returns 0 — gates a value by
operating mode/lever state inside a monitor chain. Caller 0x000F55A8 (r0c). [I] spec 13.
Cluster of 1.

#### 0x00118748 `diag_cond_fault_mask` — logic
For selector param_2 > 0xF3: forces 0xFFFF (invalid marker) when mode byte PTR_DAT_001187AC
is -2 or -8; when mode == -3 only for param_2 in {0xF4, 0xF5}. Otherwise passes value
through — invalidates a measurement under specific fault states. Caller 0x000F55DC. [I]
Cluster of 1.

#### 0x001187B0 `diag_cond_thr_cross_strict` — logic
Threshold-cross condition with direction flag at obj+4: flag==1 → `param_2 < threshold`,
flag==0 → `threshold < param_2`; returns 1 when crossed. Wrapper-called via 0x000F4266 from
monitor 0x000BB290. [O] Cluster of 1.

#### 0x001187DA `diag_cond_thr_beyond_limits` — logic
Direction-aware plausibility of the *configured threshold*: flag==1 requires threshold >
0x2133 (8499), flag==0 requires threshold < 0x1F5 (501) — i.e. the threshold must lie beyond
the fixed 501..8499 band on its own side. Inverse test at 0x00118832. Used via 0x000F4276 by
monitor 0x000BB290. [I] spec 13. Cluster of 1.

#### 0x00118808 `diag_cond_thr_reach_inclusive` — logic
Inclusive twin of 0x001187B0: flag==1 → `threshold <= value`, flag==0 → `value <= threshold`.
Wrapper 0x000F4284. [O] Cluster of 1.

#### 0x00118832 `diag_cond_thr_inside_band` — logic
Inverse of 0x001187DA: flag==1 → threshold < 0x1F5, flag==0 → threshold > 0x2133 — threshold
falls inside/below the 501..8499 band. Wrapper 0x000F4294. [I] Cluster of 1.

#### 0x00118860 `diag_cond_thr_cross_f` — logic
Float threshold condition: mode word at obj+8 selects polarity (compared against constants
PTR_DAT_00118908[0]/[1]); returns 1.0f when `param_1` crosses threshold *obj on the active
side. Reached via 3-instr wrapper 0x000F430E. [O] Cluster of 1.

#### 0x00118886 `diag_cond_thr_bound_f_a` — logic
Float variant of the fixed-limit checks: with mode at obj+8, returns 1.0f when threshold is
beyond the ROM float bounds PTR_DAT_00118908[+8]/[+0xC] on the active side. Wrapper 0x000F4314.
[O] Cluster of 1.

#### 0x001188DA `diag_cond_thr_bound_f_b` — logic
Same pattern as 0x00118886 but against the second bound pair PTR_DAT_00118908[+0x10]/[+0x14].
Wrapper 0x000F4320. [O] Cluster of 1.

#### 0x0011890C `lib_new_vobj8_vt_a` — library
Factory: allocate 8 bytes through heap helper 0x0009AA34, store vtable pointer 0x001189AC and
payload param_2; returns object or NULL. Registered from r14 0x0015CF08 context (object
table). [O] Cluster of 1.

#### 0x0011892E `lib_boxed_short_nonzero` — library
Returns `**(short**)(param_1+4) != 0` — non-zero test on a boxed value behind a handle.
Callers 0x00126F58 and 0x001273F0 (both in this region). [O] Cluster of 1.

#### 0x00118956 `lib_new_vobj8_vt_b` — library
Twin of 0x0011890C with vtable 0x001189B0 — allocate 8 bytes (0x0009AA34), store vtable +
payload. [O] Cluster of 1.

#### 0x00118978 `lib_boxed_float_ge1` — library
Returns 1.0f when the boxed float at **(param_1+4) is >= 1.0, else 0.0f. Caller 0x00127118.
[O] Cluster of 1.

#### 0x001189B8 `lib_new_byte_u16_rec` — library
Factory: allocate 4 bytes (0x0009AA34), store byte param_2 and zero the u16 at +2. Registered
by r14 0x0015CF08 as the constructor for object slot 0x45 (RAM table 0xFFF8D298). [O] Cluster
of 1.

#### 0x001189DA `diag_ratio_permille_calc` — logic
Snapshot call via PTR_FUN_00118AB4 (0x000D7C8C), then computes per-mille ratio
`min(param_3*1000/param_2, 0xFFFF)` (param_2==0 → default via PTR_FUN_00118AB8) and stores the
u16 at obj+2. Part of the ratio-object pair with 0x00118A14. Called from wiring 0x000F43A8. [I]
Cluster of 1.

#### 0x00118A14 `diag_ratio_publish_55b` — logic
Publishes the ratio stored by 0x001189DA: calls 0x000D5B32 (float convert/publish path) with
(object, 1, 0x55B, *(short*)(param_1+2)) — channel id 0x55B. Also invoked from wiring
0x000F43A8. [I] spec 10 measurement channels. Cluster of 1.

#### 0x00118A26 `lib_new_byte_u32_rec` — library
Factory: allocate 8 bytes (0x0009AA34), store byte param_2 and zero the u32 at +4. Registered
by r14 0x0015CF12 for object slot 0x46 (RAM table 0xFFF8D29C). [O] Cluster of 1.

#### 0x00118A4A `lib_fdiv_clamp_store` — logic
If param_1 <= 0 stores 1.0f at param_3+4 (via float path 0x000D7E86 for the conversion),
else stores clamp((param_2 * PTR_DAT_00118AC0)/param_1, 0, DAT_00118AC4). A ratio/factor
computation with clamping, wired from 0x000F43A8. [O] Cluster of 1.

#### 0x00118A8C `diag_ratio_publish_11_55b` — logic
Twin of 0x00118A14: publishes the u16 at param_2+2 through PTR_FUN_00118ACC (→0x000F4700
publish path) with sub-id 0xB on channel 0x55B, then re-snapshots via PTR_FUN_00118AB4.
Called from wiring 0x000F43A8. [I] spec 10 measurement channels. Cluster of 1.

#### 0x00118AD0 `diag_elem_cnt_ctor` — library
Allocate 10 bytes via 0x0009AA34 and zero all of them — constructor of the 10-byte element
counter struct tallied by 0x00118DA8. callers='-' (registered). [O] Cluster of 1.

#### 0x00118B00 `diag_elem_cnt_eval_publish` — logic
Bracket id 0x4B (fault group 75, shift-element combination family, spec 13 §2b): runs tally
0x00118DA8(param_1,param_2,param_3), then stores pattern results — 0x00118D1C at +8 and
0x00118E8C at +9. Caller 0x000F5664 (r0c wiring, itself bracketed 0x4A). [O] Cluster of 1.

#### 0x00118B26 `diag_elem_states_publish_59b` — logic
Pairs the five element condition objects 0xFFF8D2CC…0xFFF8D30C with target combos
0xFFF8D31C…0xFFF8D36C (PTR_FUN_00118D14), then publishes ten byte counters with sub-ids
1,2,3,4,5,8,9,10,12 on measurement channel 0x59B (via 0x000D5A7C path). Caller 0x000F5778.
[I] spec 10/13. Cluster of 1.

#### 0x00118BF4 `diag_elem_cnt_pat_a` — logic
Predicate on the 10-byte element tally struct: counters [0]==2 && [1]==3 && [8]==0. One of the
combination-state patterns consumed by 0x000F5664 (group 74/75 monitors). Float twin
0x00119214. [I] spec 13 §2b. Cluster of 1.

#### 0x00118C0C `diag_elem_cnt_pat_b` — logic
Predicate: [+3]==2 && [+4]==3 on the same tally struct. Float twin 0x00119238. [I] Cluster of 1.

#### 0x00118C1E `diag_elem_cnt_pat_c` — logic
Predicate: [0]==1 && [4]==3. Float twin 0x00119256. [I] Cluster of 1.

#### 0x00118C30 `diag_elem_cnt_pat_d` — logic
Predicate: [+3]==2. Float twin 0x00119270. [I] Cluster of 1.

#### 0x00118C38 `diag_elem_cnt_pat_e` — logic
Predicate: [+3]==1 && [+4]==2 && [+5]==2. Float twin 0x00119282. [I] Cluster of 1.

#### 0x00118C50 `diag_elem_cnt_pat_f` — logic
Predicate: [+3]==1 && [+4]==2 && [+5]==1 && [+2]==1. Float twin 0x001192A6. [I] Cluster of 1.

#### 0x00118C6E `diag_elem_cnt_pat_g` — logic
Predicate: [+6]==4 && [+3]==1 (no float twin in this unit). [I] Cluster of 1.

#### 0x00118C80 `diag_elem_alt_pattern_ok` — logic
Evaluates the five element objects (via 0x001190AE idx 1..5) with alternating methods:
vt+0x20 must be false for 1,3,5 and vt+0x20 true for 2,4 — returns 1 only for that exact
alternating pattern. Installed by 0x000F5664. [I] spec 13 §2b. Cluster of 1.

#### 0x00118D1C `diag_elem_all5_pattern_ok` — logic
Evaluates the five element objects alternating vt+0x2C (1,3,5) and vt+0x20 (2,4); returns 1
only when all five are non-zero. Result stored at +8 by 0x00118B00. [O] Cluster of 1.

#### 0x00118DA8 `diag_elem_cnt_tally` — logic
Zeroes the 10-byte tally struct, then for element objects 1..5 (via 0x001190AE): applies
config (PTR_FUN_00118E88 with param_2/param_3), counts vt+0x20 true into [0] (and [3] when
type byte at +5 == 0), vt+0x2C true into [1] (and [4] when type == 2), vt+0x38 into [5] and
vt+0x20 into [2] when type == 1, and non-passing into [6]/[7]. Feeds predicates
0x00118BF4…0x00118C6E. [O] Cluster of 1.

#### 0x00118E8C `diag_elem_combo_row_match` — logic
26-row scan (0x1190D8-compare against table 0xFFF8D2D0): for each row and each of the five
element objects, looks up expected state (0/1/2) and requires the matching vtable eval
(vt+0x20/0x2C/0x38) to hold; returns 1 on the first fully matching combination row. Result
stored at +9 by 0x00118B00. This is the "actual element set matches an expected gear combo"
check of the group-56..79 monitors. [I] spec 13 §2b. Cluster of 1.

#### 0x001190AE `diag_elem_obj_get` — logic
Returns element object for index 1..5 from pointer table PTR_LAB_001190DC (defaults to
0xFFF8D2CC), calling trap/error helper PTR_FUN_001190E0 on out-of-range. Used by the tally,
pattern and combo-match functions. [O] Cluster of 1.

#### 0x001190E4 `diag_elem_mon_ctor` — library
Allocate 0x28 bytes via 0x0009AA34 and zero ten words — constructor of the 40-byte
element-monitor struct (fields used by 0x00119126 et al.). callers='-' (registered). Member
0x0011A9AA is the identical pattern reached from r14 0x0015C6C8 (registered ctor). [O]
Cluster of 2 (0x001190E4, 0x0011A9AA).

#### 0x00119126 `diag_elem_mon_calc` — logic
Runs FUN_00119428 (reset), then stores FUN_0011939C(param_1) at +0x20 and FUN_0011956C(param_1)
at +0x24 of the 0x28-byte monitor struct. Called by 0x000F59D0 (r0c wiring of the float-typed
element monitor). [O] Cluster of 1.

#### 0x00119140 `diag_elem_publish_59b_ids` — logic
Pairs the five element objects with target combos (PTR_FUN_00119384, mirrored from
0x00118B26) and publishes ten values with sub-ids 0x65..0x70 on channel 0x59B; closes with
bracket id 0x4D. Caller 0x000F5B96. [I] spec 10/13. Cluster of 1.

#### 0x00119214 `diag_elem_flt_pat_a` — logic
Float twin of 0x00118BF4: [0]==2.0f && [1]==DAT_00119390 && [8]==0.0f → 1.0f. Installed by
0x000F59D0 (float element monitor wiring). [I] spec 13 §2b. Cluster of 1.

#### 0x00119238 `diag_elem_flt_pat_b` — logic
Float predicate: +0xC==2.0f && +0x10==DAT_00119390. Twin of 0x00118C0C. Caller 0x000F5A1C. [I]
Cluster of 1.

#### 0x00119256 `diag_elem_flt_pat_c` — logic
Float predicate: [0]==1.0f && [4]==DAT_00119390. Twin of 0x00118C1E. Caller 0x000F5A38. [I]
Cluster of 1.

#### 0x00119270 `diag_elem_flt_pat_d` — logic
Float predicate: +0xC==2.0f. Twin of 0x00118C30. Caller 0x000F5A54. [I] Cluster of 1.

#### 0x00119282 `diag_elem_flt_pat_e` — logic
Float predicate: +0xC==1.0f && +0x10==2.0f && +0x14==2.0f. Twin of 0x00118C38. Callers
0x000F5A70/0x000F5ABA. [I] Cluster of 1.

#### 0x001192A6 `diag_elem_flt_pat_f` — logic
Float predicate: +0xC==1.0f && +0x10==2.0f && +0x14==1.0f && +8==1.0f. Twin of 0x00118C50.
Caller 0x000F5A9E. [I] Cluster of 1.

#### 0x001192D2 `diag_elem_flt_pat_g` — logic
Float predicate: +0x18==DAT_00119394 && +0xC==1.0f. Companion of the byte tally predicates;
element-state enums are held as float constants at 0x119390–0x119398. Caller 0x000F5B3C. [I]
spec 13 §2b. Cluster of 1.

#### 0x001192EE `diag_elem_flt_alt_pattern_ok` — logic
Float twin of 0x00118C80: over the five element objects (via 0x0011978A with float ids
1.0..DAT_00119398) requires vt+0x20 false,true,false,true,false in that order; returns 1.0f
on match. Caller 0x000F5B6E. [O] Cluster of 1.

#### 0x0011939C `diag_elem_flt_all5_pattern_ok` — logic
Float twin of 0x00118D1C: all five element objects must evaluate true alternating vt+0x2C
(1,3,5 via DAT_0011955C/DAT_00119564) and vt+0x20 (2 via DAT_00119560). Result stored at
+0x20 by 0x00119126. [O] Cluster of 1.

#### 0x00119428 `diag_elem_flt_tally` — logic
Float twin of 0x00118DA8: loops element ids 1.0..DAT_00119564, applies config via
PTR_FUN_00119568, and tallies vt+0x20/vt+0x2C/vt+0x38 results plus type-field (obj+8) cases
into eight float counters. Called by 0x00119126. [O] Cluster of 1.

#### 0x0011956C `diag_elem_flt_combo_row_match` — logic
Float twin of 0x00118E8C: scans combo rows (PTR_FUN_001197B0 against tables 0xFFF8D320…) up
to DAT_001197AC and requires all five element objects (0xFFF8D31C…0xFFF8D36C) to match the
expected state via vt+0x20/0x2C/0x38; result stored at +0x24 by 0x00119126. [I] spec 13 §2b.
Cluster of 1.

#### 0x0011978A `diag_elem_flt_obj_get` — logic
Float-typed element-object getter: validates id in [1.0, DAT_001197B8] (else trap via
PTR_FUN_001197E0), indexes table PTR_DAT_001197DC, defaults to 0xFFF8D31C. Twin of
0x001190AE. [O] Cluster of 1.

#### 0x001197E4 `lib_new_u16_u16_u8_rec` — library
Allocate 6 bytes via 0x0009AA34 and zero (u16,u16,u8) — constructor of the range-window result
struct written by 0x00119852. Registered from r14 0x0015CFF4; identical member 0x0012E350 is
registered from 0x0015D5DC. [O] Cluster of 2 (0x001197E4, 0x0012E350).

#### 0x00119806 `diag_range_win_calc_bracket` — logic
Fault-bracket id 0x4C (group 76, element family) around a call to window calculator
0x00119852. Caller 0x000F5664. [O] spec 13 §2b. Cluster of 1.

#### 0x00119818 `diag_range_win_publish_59d` — logic
Publishes the three window fields (u16 low, u16 high, u8 flag) with sub-ids 1,2,3 on channel
0x59D. Member 0x0012E448 is the identical pattern on channel 0x581 (different measurement
channel — kept as a distinct name). Callers 0x000F5778 / 0x001264C0. [O] spec 10. Cluster of 2
(0x00119818, 0x0012E448).

#### 0x00119852 `diag_range_win_calc` — logic
Computes a plausibility window from two signed margins (PTR_DAT_00119A4C/0x00119A54): sorts
them, then low = min*(100−obj[+0xC])/100 and high = max*(obj[+0x10]+100)/100, both clamped to
0xFFFF; flag = 1 when the reference threshold (PTR_DAT_00119A5C+2) falls outside [low,high].
[O] Cluster of 1.

#### 0x00119902 `diag_range_win_flt_thunk` — stub
Two-instruction tail-jump thunk to 0x00119946 (float window calculator), called by 0x000F59D0.
The inlined body shown by Ghidra is the target's. [O] Cluster of 1.

#### 0x00119906 `diag_range_win_publish2_59d` — logic
Publishes the float window trio with sub-ids 0xB,0xC,0xD on channel 0x59D, bracket id 0x4E.
Member 0x0012E618 publishes on channel 0x581 with bracket 0x6E — same pattern, different
channel. [O] Cluster of 2 (0x00119906, 0x0012E618).

#### 0x00119946 `diag_range_win_flt_calc` — logic
Float window calculator (twin of 0x00119852): reads two values via PTR_FUN_00119A6C, scales
low by (DAT_00119A78 − cfg[+0xC])/100 and high by (cfg[+0x10] + DAT_00119A78)/100 with
2^32-wrap fixups, clamps to 0xFFFF, and sets flag when the reference (PTR_DAT_00119A80+4)
falls outside. Reached only through thunk 0x00119902. [O] Cluster of 1.

#### 0x00119A84 `diag_elem_ramp_ctor` — library
Allocate 6 bytes via 0x0009AA34 and zero — constructor of the byte ramp FSM struct used by
0x00119AAC (counters [0]/[1], direction [2], count [3], flag [4], active-count [5]).
Registered ctor (callers='-'). [O] Cluster of 1.

#### 0x00119AAC `diag_elem_ramp_fsm` — logic
Bracket id 0x77 (group 119): gated by 0x00119D32 (else clear via 0x00119DCC); when fewer than
3 elements active it ramps counters down (and [3] up), else ramps up (and clears [3]),
applying 0/1 to the five element objects (table PTR_DAT_00119C84). When a counter reaches the
config threshold (via 0x00119E0A) it reports through 0x00119C98; after 4 misses it triggers
the fault-event call (PTR_PTR_00119C94)(…,0,1) and flips direction. Wired from 0x000F43A8. [I]
spec 13 §2b. Cluster of 1.

#### 0x00119C04 `diag_elem_ramp_eval` — logic
Resets the five element objects (vt+0x14) then counts how many evaluate true (vt+0x20) into
struct[5]. Called at the top of 0x00119AAC. [O] Cluster of 1.

#### 0x00119C98 `diag_elem_ramp_report` — logic
Finds the element with the highest byte field +4 (table PTR_DAT_00119F0C) and maps the winner
(3, 4 or 5 → PTR_PTR slots, default slot) to a fault-event call
(PTR_PTR_00119F20)(obj, slot, 1, 1). "Name the offending element" half of the ramp monitor.
[O] Cluster of 1.

#### 0x00119D32 `diag_elem_ramp_gate` — logic
Enables the ramp monitor when at least one of the five element objects (vt+0x2C value) or two
extra sources (PTR_FUN_00119F2C/0x00119F34) exceed the u16 threshold at PTR_DAT_00119F24,
and the mode byte PTR_DAT_00119F38 is neither 5 nor 7. [O] Cluster of 1.

#### 0x00119DCC `diag_elem_ramp_clear` — logic
Resets the five element objects (PTR_FUN_00119F3C with 0) and zeroes the four state bytes of
the ramp struct — the "monitor disabled" path of 0x00119AAC. [O] Cluster of 1.

#### 0x00119E0A `diag_elem_ramp_get_cfg` — data-accessor
Returns *(undefined4*)PTR_DAT_00119F24 — fetches the config object holding the ramp
thresholds used by 0x00119AAC. [O] Cluster of 1.

#### 0x00119E10 `diag_elem_ramp_publish_593` — logic
Pushes new config values (vt+0x38 setter) into the five element objects
0xFFF8D3A4…0xFFF8D424, then publishes six state bytes with sub-ids 1,3,5,6,7,8 on channel
0x593. Wired from 0x000F43A8. [I] spec 10. Cluster of 1.

#### 0x00119ED6 `diag_elem_ramp_flt_ctor` — library
Allocate 0x18 bytes via 0x0009AA34 and zero six words — constructor of the float ramp FSM
struct used by 0x00119F48. Registered ctor. [O] Cluster of 1.

#### 0x00119F48 `diag_elem_ramp_flt_fsm` — logic
Float twin of 0x00119AAC: gate via 0x0011A22C, ramp counters up/down toward DAT_0011A18C with
direction flag at [2], apply values to element objects (table PTR_DAT_0011A194), report
through 0x0011A168 when a counter reaches the config threshold, trigger the fault event
(PTR_PTR_0011A1AC)(…,0xFF) after DAT_0011A1A0 misses and flip direction. Wired from
0x000F43A8. [I] spec 13 §2b. Cluster of 1.

#### 0x0011A0E0 `diag_elem_ramp_flt_eval` — logic
Float twin of 0x00119C04: reset all element objects (vt+0x14), then count vt+0x20-true
objects into param_1[+0x14]. Called at the top of 0x00119F48. [O] Cluster of 1.

#### 0x0011A168 `diag_elem_ramp_flt_report` — logic
Float twin of 0x00119C98: finds the element with the highest float field +4 over
DAT_0011A1B0..DAT_0011A190 and triggers (PTR_PTR_0011A420)(obj, slot, 0xFE) with the winner
slot. Called when the ramp counter expires in 0x00119F48. [O] Cluster of 1.

#### 0x0011A22C `diag_elem_ramp_flt_gate` — logic
Float twin of 0x00119D32: counts element objects (table 0x11A408) whose vt+0x2C value exceeds
the threshold read via PTR_FUN_0011A428 (config at ~PTR_DAT_0011A424, complement-stored), plus
two extra sources; returns 1.0f when any exceed and mode PTR_DAT_0011A444 is neither
DAT_0011A42C nor DAT_0011A448. [O] Cluster of 1.

#### 0x0011A2E4 `diag_elem_ramp_flt_clear` — logic
Float twin of 0x00119DCC: passes 0 to all five element objects (PTR_FUN_0011A44C) and zeroes
the first four state floats of the ramp struct. [O] Cluster of 1.

#### 0x0011A334 `diag_elem_ramp_flt_get_cfg` — data-accessor
Returns ~*(u32*)PTR_DAT_0011A424 — the config object pointer, stored complemented in RAM
(same complement-load pattern as 0x00119946/0x0011A9EC). Twin of 0x00119E0A. [O] Cluster of 1.

#### 0x0011A33C `diag_elem_ramp_flt_publish_593` — logic
Pushes config values into the five element objects 0xFFF8D3B4…0xFFF8D434 (vt+0x38) and
publishes six state bytes with sub-ids 0xB,0xD,0xF,0x10,0x11,0x12 on channel 0x593, closing
bracket 0x78. Wired from 0x000F43A8. [I] spec 10/13. Cluster of 1.

#### 0x0011A458 `diag_cond_zero_on_sel10` — logic
Zeroes the passed value when selector param_2 == 10 and flag PTR_DAT_0011A48C is set — a
gear/slot-dependent gate in a monitor chain (companion of 0x00118718). Caller 0x000F55A8. [I]
Cluster of 1.

#### 0x0011A46E `diag_cond_inval_on_flag` — logic
Forces 0xFFFF when selector param_2 == 0xF5 (245) and mode byte PTR_DAT_0011A490 == -1;
otherwise passes the value through (companion of 0x00118748). Caller 0x000F55DC. [I] Cluster
of 1.

#### 0x0011A494 `diag_mon_wiring_95_118` — logic
Master wiring for a monitor suite, fault-bracketed 0x5F…0x76 (groups 95–118): constructs ~10
condition objects in pairs (install/AND-combine through PTR_FUN slots 0x11A5A4–0x11A66C) and
invokes 34 monitor sub-functions across this region and 0x12xxxx (0x0011A6A0, 0x0011ADBA,
0x0011A9EC, 0x0011AC7C, publishes, ctors …). Called from 0x000F43A8. [I] spec 13. Cluster of 1.

#### 0x0011A670 `diag_mon_pair_ctor` — library
Allocate 0xE bytes via 0x0009AA34 and zero — constructor of the timer-pair monitor struct
consumed by 0x0011A6A0 (two u16 timers, five flag bytes, debounce counter, latched result).
Registered from r14 0x0015C6C8. [O] Cluster of 1.

#### 0x0011A6A0 `diag_mon_timer_pair_eval` — logic
237-instr monitor step (wired from 0x0011A494): reads four condition inputs
(PTR_DAT_0011A8A0…) into flags; two saturating u16 timers (0…0xFFFF) count up unless their
inputs (config +0xC/+0x14 vs +0x44) are set; a five-term condition chain plus thresholds
(config +0x14/+0x18) sets the combined flag; debounce counter [5] counts to config +0x4C only
when flags [2]/[5] set and [3]/[4] clear; result latched to +0xD when the value at cfg+0x20 is
below PTR_DAT_0011A8D8. [I] spec 13. Cluster of 1.

#### 0x0011A8DC `diag_mon_publish_571_odd` — logic
Publishes nine monitor fields with odd sub-ids 1,3,5,7,0xB,0xD,0xF,0x11,0x13,0x15 on channel
0x571; null-guard via PTR_FUN_0011AAF0. Wired from 0x0011A494. [I] spec 10. Cluster of 1.

#### 0x0011A982 `meas_flags_pack5` — library
Packs the five flag bytes at +4…+8 into a bitmask (0x01|0x02|0x04|0x08|0x10). Caller
0x000F645E (r0c publish step). [O] Cluster of 1.

#### 0x0011A9EC `diag_mon_timer_flt_eval` — logic
Float twin of 0x0011A6A0 (267 instrs): loads four conditions into fields [2]..[6], increments
two saturating float counters toward DAT_0011AB28 when their inputs are clear, runs the nested
condition chain over config slots (+0x14/+0x18/+0x4C…), debounces [7] against cfg+0x4C and
latches [9]=[8] when the speed source (cfg+0x20) is below PTR_DAT_0011AD44. Wired from
0x0011A494. [I] spec 13. Cluster of 1.

#### 0x0011AC7C `diag_mon_publish_571_even` — logic
Publishes the float monitor fields with even sub-ids 2,4,6,8,0xC,0xE,0x10,0x12,0x14,0x16 on
channel 0x571, closing bracket 0x65; null-guard via PTR_FUN_0011AD50. Wired from 0x0011A494.
[I] spec 10. Cluster of 1.

#### 0x0011AD58 `diag_mon_grp_ctor` — library
Allocate 0x54 bytes (PTR_FUN_0011AFA8) and initialize five 0x10-byte sub-objects at +0,+0x10,
…,+0x40 via 0x001265CC with config pairs (PTR_DAT_0011AFAC…), then zero result bytes +0x50/
+0x51. Struct consumed by 0x0011ADBA. Registered from r14 0x0015C6C8. [O] Cluster of 1.

#### 0x0011ADBA `diag_mon_grp_eval` — logic
Bracket 0x66: evaluates the five sub-objects (PTR_FUN_0011AFDC) and counts set flags at
+0xC/+0x1C/+0x2C/+0x3C/+0x4C. More than two set → checks alternating patterns (1,0,1,0,1 or
1,1,0,1,0) → result 1 (fault pattern), else 2; ≤2 set → 0. Result at +0x50, latched to +0x51
when speed (cfg+0x20) is below PTR_DAT_0011AFE4 or result is 0/2. Wired from 0x0011A494. [I]
spec 13. Cluster of 1.

#### 0x0011AEAC `meas_flags_pack5_ofs0c` — library
Same packing as 0x0011A982 but over bytes at +0xC,+0x1C,+0x2C,+0x3C,+0x4C — bitmask of the
group monitor's five sub-objects. Caller 0x000F645E. [O] Cluster of 1.

#### 0x0011AEE8 `diag_mon_grp_pair_publish` — logic
Links the group monitor's sub-objects with new config blocks (param_2+0x18/0x30/0x48/0x60 via
PTR_FUN_0011AFE8) and publishes +0x50/+0x51 with sub-ids 1,2 on channel 0x575. Wired from
0x0011A494. [I] spec 10. Cluster of 1.

#### 0x0011AF42 `diag_mon_grp2_ctor` — library
Allocate 0x80 bytes and initialize five 0x18-byte sub-objects at +0,+0x18,…,+0x60 via
0x00126720, then zero the result slots +0x78/+0x7C. Struct consumed by 0x0011B01C.
Registered from r14 0x0015C6C8. [O] Cluster of 1.

#### 0x0011B01C `diag_mon_grp2_eval` — logic
Float group monitor: evaluates the five sub-objects (PTR_FUN_0011B180); when the sum of the
five float values reaches DAT_0011B184, checks alternating sign patterns (>0,≤0,>0,≤0,>0 or
>0,>0,≤0,>0,≤0) → 1.0f else 0.0f; sum short of the threshold → 2.0f. Latched from +0x78 to
+0x7C when speed is low or result 0/2. Wired from 0x0011A494. [I] spec 13. Cluster of 1.

#### 0x0011B114 `meas_get_fld40` — data-accessor
Two-instruction getter: returns *(u32*)(param_1+0x40). Called by monitor task 0x0012DD94.
Member 0x0011B11A is the identical pattern at +0x70 — different field, distinct name. [O]
Cluster of 2 (0x0011B114, 0x0011B11A).

#### 0x0011B120 `diag_mon_grp2_publish` — logic
Links the float group monitor's sub-objects with config blocks (param_2+0x10…+0x40) and
publishes +0x78/+0x7C with sub-ids 0xB,0xC on channel 0x575, closing bracket 0x67. Wired from
0x0011A494. [I] spec 10. Cluster of 1.

#### 0x0011B1A0 `lib_new_u32_box` — library
Allocate 4 bytes via 0x0009AA34 and store param_2 — boxed u32 constructor (identical member
0x0011B2EE). [O] Cluster of 2 (0x0011B1A0, 0x0011B2EE).

#### 0x0011B1BE `tbl_min_sel_curve` — data-accessor
Selects a table by the selector byte PTR_DAT_0011B464 (slot at *(obj)+0x30+idx*4) and returns
the minimum of its data words via 0x000F3D56. Member 0x0011B1DA is the same pattern calling
the max helper 0x000F3DF4 — different aggregate, distinct name. Calibration-table bounds
access (spec 09 calibration access). [I] Cluster of 2 (0x0011B1BE, 0x0011B1DA).

#### 0x0011B1F6 `tbl_interp_sel_curve` — logic
Selects the curve as 0x0011B1BE does and evaluates it with (PTR_DAT_0011B470, param_2) through
the 2-D interpolation kernel 0x000B7164 (186-instr curve/table interp used by ~100 callers).
[I] spec 09 calibration access. Cluster of 1.

#### 0x0011B21C `tbl_sel_get_word40` — data-accessor
Returns **(short**)(*(int*)*param_1 + 0x40) — reads a short through a double handle plus 0x40
in the same table-descriptor family as 0x0011B1BE/0x0011B1F6. [I] Cluster of 1.

#### 0x0011B228 `tbl_lookup_sel_curve_byte` — data-accessor
Evaluates the curve at *(handle)+0x38 + sel*4 (selector byte PTR_DAT_0011B464) with x =
PTR_DAT_0011B470 through the byte-axis 1-D interpolation kernel 0x000B6D6C. Part of the
calibration-access layer (spec 09 §1). [I] Cluster of 1.

#### 0x0011B24A `tbl_interp_limit_slots0` — logic
Picks a limit short from DAT_0011B480 (+0x10 or +0x1C depending on the mode flag behind
PTR_DAT_0011B47C) and evaluates slot pair +0/+0x18 of the object via 0x0011B430 (validated-slot
interp). Member 0x0011B278 is the identical pattern calling 0x0011B530 (slot pair +8/+0x20) —
different slots, distinct name. [I] Cluster of 2 (0x0011B24A, 0x0011B278).

#### 0x0011B2A6 `tbl_interp_flag_sel` — logic
Wrapper: evaluates 0x0011B5E6 with x = PTR_DAT_0011B470 — validated-slot interpolation on
slots +0x10/+0x28 selected by the flag argument. [I] Cluster of 1.

#### 0x0011B2BC `tbl_lookup_curve44` — data-accessor
Byte-axis interpolation (0x000B6D6C) on the fixed slot *(handle)+0x44 with x =
PTR_DAT_0011B470. [I] Cluster of 1.

#### 0x0011B2D6 `tbl_get_flag4` — data-accessor
Returns the flag byte at object+4 behind PTR_DAT_0011B47C (double handle). [O] Cluster of 1.

#### 0x0011B2E2 `tbl_get_flag8` — data-accessor
Same as 0x0011B2D6 at object+8. [O] Cluster of 1.

#### 0x0011B30C `tbl_stats_sel_curve_a` — logic
Passes the selector-indexed curve (complement-stored pointer at *(handle)+0x30+sel*4) to the
table-statistics routine 0x000F3D82 (iterates all words of the 2-D word table; twin kernel
0x000F3E22). Validation/plausibility of the selected curve. [I] Cluster of 1.

#### 0x0011B324 `tbl_stats_sel_curve_b` — logic
Identical to 0x0011B30C but calls the twin statistics kernel 0x000F3E22 (different ROM
constant). [I] Cluster of 1.

#### 0x0011B33C `tbl_interp2d_sel_curve` — logic
Two-axis float interpolation (0x000F3A18, 203-instr kernel) with x from PTR_DAT_0011B48C and
the selector-indexed curve of param_2. [I] spec 09. Cluster of 1.

#### 0x0011B358 `tbl_get_word40_f` — data-accessor
Reads the u16 at selected-curve+0x40 and converts to float via 0x000F37DA. [O] Cluster of 1.

#### 0x0011B368 `tbl_interp_bytex_sel` — logic
Byte-axis float interpolation (0x000F37F8) with x = PTR_DAT_0011B48C over the selected curve
(*(handle)+0x38+sel*4). [I] Cluster of 1.

#### 0x0011B382 `tbl_interp_signed_a` — logic
Reads a float (PTR_FUN_0011B4A0 on complement-stored config 0x11B49C); by its sign picks slot
PTR_DAT_0011B4A4+0x2C (<=0) or +0x3C (>0) and evaluates via 0x0011B4E0 (slots +0/+0x18).
Member 0x0011B3BE identical via 0x0011B592 (slots +8/+0x20). [I] Cluster of 2 (0x0011B382,
0x0011B3BE).

#### 0x0011B3FA `tbl_interp2d_wrap_642` — logic
Wrapper for 0x0011B642: evaluates with x = PTR_DAT_0011B48C and the complement-stored curve
from *param_2. [I] Cluster of 1.

#### 0x0011B406 `tbl_interp_bytex_curve44` — logic
Byte-axis float interpolation (0x000F37F8) with x = PTR_DAT_0011B48C on the fixed +0x44 slot.
[I] Cluster of 1.

#### 0x0011B418 `tbl_get_cfg_f4` — data-accessor
Reads byte field +4 of the complement-stored config (0x11B49C) as float (via 0x000F37C8
byte→float). Caller 0x0011BD9E. [O] Cluster of 1.

#### 0x0011B424 `tbl_get_cfg_f8` — data-accessor
Same as 0x0011B418 at config+8. Caller 0x0011BD9E. [O] Cluster of 1.

#### 0x0011B430 `tbl_interp_slot_chk_a` — logic
Validated-slot curve evaluation: checks the slot pointer (stored complemented) really
complements the object base, then interpolates slot +0 (flag==1, index from PTR_DAT_0011B4A8)
or +0x18 through the 2-D kernel 0x000B7164 with (limit, param_4). Returns 0 when validation
fails. [O] Cluster of 1.

#### 0x0011B4E0 `tbl_interp2d_slot_chk_a` — logic
Float-twin pattern of 0x0011B430 with kernel 0x000F3A18: slots +0 (param_1==1.0) or +0x18,
selector index PTR_DAT_0011B684[2], validated complement slots. Called by 0x0011B382. [O]
Cluster of 1.

#### 0x0011B530 `tbl_interp_slot_chk_b` — logic
Validated-slot interpolation like 0x0011B430 but on slots +8/+0x20, kernel 0x000B7164. Called
by 0x0011B278. [O] Cluster of 1.

#### 0x0011B592 `tbl_interp2d_slot_chk_b` — logic
Validated-slot float interpolation (0x000F3A18) on slots +8/+0x20. Called by 0x0011B3BE. [O]
Cluster of 1.

#### 0x0011B5E6 `tbl_interp_val_slot_chk` — logic
Validated-slot evaluation on slots +0x10/+0x28 via kernel PTR_FUN_0011B690 (→0x000B6E10) with
one argument. Called by 0x0011B2A6. [O] Cluster of 1.

#### 0x0011B642 `tbl_interp2d_slot_chk_c` — logic
Validated-slot evaluation on slots +0x10/+0x28 via kernel PTR_FUN_0011B694 (→0x000F38AA) with
x from param_2. Called by 0x0011B3FA. [O] Cluster of 1.

#### 0x0011B698 `diag_obj24_vt_ctor` — library
Allocate 0x24 bytes (0x0009AA34), store vtable 0x11B8B0, two payload pointers (+8/+0xC), and
zero the state fields — constructor of a monitor/task object. callers='-' (registered). [O]
Cluster of 1.

#### 0x0011BA90 `tbl_max_sel_curve_floor0` — data-accessor
Returns max(0, curve-max) where the max is taken by 0x0011B1DA over the curve at param_1+0xC
— upper bound of a selected curve floored at zero. [O] Cluster of 1.

#### 0x0011BAC8 `diag_obj40_vt_ctor` — library
Allocate 0x40 bytes (0x0009AA34), store vtable 0x11BB94 and two payload pointers, zero the
remaining 16 words — larger monitor/task object constructor. callers='-' (registered). [O]
Cluster of 1.

#### 0x0011BD9E `diag_dual_timer_flt_fsm` — logic
Float condition object (state at param_1): only active when [0]==1.0; clamps [9] to [0,
_LAB_0011BE14] and pushes it through a vtable call at cfg+0x50; two saturating counters [10]
(target via cfg 0x0011BE2C) and [11] (target via PTR_FUN_0011BF9C, gated by
*_LAB_0011BF94 == *PTR_DAT_0011BF98); when both counters reach their targets it copies
[7]=[6] and resets [10], [11], [0]. Uses config getters 0x0011B418/0x0011B424. [I] Cluster
of 1.

#### 0x0011BFB4 `diag_obj18_vt_ctor` — library
Allocate 0x18 bytes via 0x0009AA34, store vtable 0x11C200 plus two payload pointers (+8/+0xC)
and zero state — constructor of the plausibility-condition objects (0x0011C0FC family).
[O] Cluster of 1.

#### 0x0011C0FC `diag_plaus_window_u16` — logic
Plausibility condition between two vtable inputs (vt+0x2C and vt+0x20 of the object at
param_1+8): reads two bounds via interp 0x000B6E10 on cfg slots 0/4, computes |a-b| clamped
to [0,0xFFFF] (0x80000000 guard), and returns 0 (fault) when the pair lies outside the
configured window or the difference exceeds the third bound (cfg+8 via 0x000B6EAE). [I] spec
13 plausibility monitors. Cluster of 1.

#### 0x0011C1C0 `diag_obj20_vt_ctor` — library
Allocate 0x20 bytes via 0x0009AA34, store vtable 0x11C230 and two payload pointers, zero 5
words — constructor of the float plausibility objects (0x0011C354 family). [O] Cluster of 1.

#### 0x0011C354 `diag_plaus_window_flt` — logic
Float twin of 0x0011C0FC: bounds via 0x000F395C/0x000F38AA on the complement-stored config,
|v2-v1| clamped to [0, _LAB_0011C460] with sign handling, returns 1.0f when the values fall
outside the window or the difference reaches the bound. [I] spec 13. Cluster of 1.

#### 0x0011C468 `diag_speed_vs_gear_mon` — logic
Bracket 0x47 (group 71, element family): loop over gear index −1..8 (0..8 when mode byte at
cfg+0xD is 7); expected = |cfg_u16(+6)*value(idx)/1000 − cfg_u16(+0x18)| clamped to
[0,0xFFFF] (PTR_FUN_0011C6A8 supplies per-index value); compares with the actual value from
PTR_FUN_0011C6B4(cfg+0x20, idx+1) and flags 0 (fault) as soon as actual < expected. Wired
from 0x000F43A8. [I] spec 13 §2b. Cluster of 1.

#### 0x0011C50E `diag_speed_vs_gear_mon_loop` — logic
Decompiler artifact: the inner loop body of 0x0011C468 re-emitted as a separate entry (uses
unaffiliated registers unaff_r12/r13/r14); same per-index expected-vs-actual comparison,
ends via 0x000B6C9E (word at param+4+idx*2). Kept separate because the sweep treats it as a
function. [I] Cluster of 1.

#### 0x0011C56E `diag_speed_vs_gear_flt_mon` — logic
Float twin of 0x0011C468: index loop −1..DAT_0011C6CC over the config object
PTR_DAT_0011C6C0; expected = clamp|int(interp(idx)*cfg(+0xC)/PTR_DAT_0011C6D4 − cfg(+0x34)|
to [0, DAT_0011C6C8]; compares with interp(idx+1, cfg+0x20 via PTR_FUN_0011C6E0); result 0.0
as soon as actual < expected. Wired from 0x000F43A8. [I] Cluster of 1.

#### 0x0011C5B6 `diag_speed_vs_gear_flt_loop1` — logic
Decompiler artifact of 0x0011C56E's loop tail (unaffiliated registers); same clamp/compare
body. [I] Cluster of 1.

#### 0x0011C636 `diag_speed_vs_gear_flt_loop2` — logic
Decompiler artifact of 0x0011C56E's second loop path (unaffiliated registers; ends via
0x000F37E4). [I] Cluster of 1.

#### 0x0011C660 `lib_new_zero4_rec` — library
Allocate 4 bytes via 0x0009AA34 and zero them — minimal object constructor (registered).
Identical member 0x0012760C in the 0x12xxxx area. [O] Cluster of 2 (0x0011C660, 0x0012760C).

#### 0x0011C6E8 `tbl_cfg_interp_sel_u16` — data-accessor
Evaluates the curve at complement-stored cfg+4+sel*4 (selector PTR_DAT_0011C8B8) with x =
PTR_DAT_0011C8B0 via the u16 interpolation kernel 0x000B6EAE. First of the big cfg-accessor
family against PTR_DAT_0011C8B4/0x11C8C0. Caller 0x000F6E28. [I] spec 09. Cluster of 1.

#### 0x0011C70C `tbl_cfg_get_word_34` — data-accessor
Reads the short behind *(cfg0x11C8C0)+0x34 (double handle). Members are the same 5-instr
pattern at +0x38 (0x0011C716), +0x14 (0x0011C742) and +0x3C (0x0011C758) — different fields,
distinct names. [O] Cluster of 4 (0x0011C70C, 0x0011C716, 0x0011C742, 0x0011C758).

#### 0x0011C720 `tbl_cfg_interp_sel_c` — data-accessor
Interpolation (0x000B6EAE) on cfg slot (+0xC + sel*4) with x = PTR_DAT_0011C8B0. Caller
0x000F6E28. [O] Cluster of 1.

#### 0x0011C74C `tbl_cfg_get_flag0` — data-accessor
Reads the flag byte at the front of the cfg object (PTR_DAT_0011C8C0, double handle). [O]
Cluster of 1.

#### 0x0011C762 `tbl_cfg_interp_18` — data-accessor
Interpolation (0x000B6EAE) on cfg+0x18 with x = PTR_DAT_0011C8C4. Members identical at +0x1C
(0x0011C77A) and +0x24 (0x0011C7C8). Callers 0x0011D044 / 0x000F69C6. [O] Cluster of 3
(0x0011C762, 0x0011C77A, 0x0011C7C8).

#### 0x0011C792 `tbl_cfg_get_flag28` — data-accessor
Reads the flag byte behind cfg+0x28; twin member 0x0011C79E reads +0x2C. [O] Cluster of 2
(0x0011C792, 0x0011C79E).

#### 0x0011C7AA `tbl_cfg_interp2d_30` — data-accessor
Two-axis interpolation (0x000B72D8, 198-instr kernel) on cfg+0x30 with x=PTR_DAT_0011C8B0,
y=PTR_DAT_0011C8C4. [O] Cluster of 1.

#### 0x0011C7E0 `tbl_cfg_interp2f_sel` — data-accessor
Float 2-D interpolation (0x000F395C) with x = PTR_DAT_0011C8CC on the complement-stored cfg
slot (+4+sel*4). [O] Cluster of 1.

#### 0x0011C7FA `tbl_cfg_get_i16_34_f` — data-accessor
Reads the s16 at complement-cfg+0x34 as float via 0x000F37D2. Members at +0x38 (0x0011C806)
and +0x3C (0x0011C844). Callers 0x0011D16C / 0x000F6766. [O] Cluster of 3 (0x0011C7FA,
0x0011C806, 0x0011C844).

#### 0x0011C812 `tbl_cfg_interp2f_sel_c` — data-accessor
Float 2-D interpolation (0x000F395C) with x = PTR_DAT_0011C8CC on cfg slot (+0xC+sel*4). [O]
Cluster of 1.

#### 0x0011C838 `tbl_cfg_get_flag0_f` — data-accessor
Reads the byte at complement-cfg+0 and converts to float (0x000F37C8). Caller 0x000F6D4E. [O]
Cluster of 1.

#### 0x0011C850 `tbl_cfg_interp2f_18` — data-accessor
Float 2-D interpolation (0x000F395C) with x = PTR_DAT_0011C8E0 on cfg+0x18; twin member
0x0011C860 at +0x1C. Caller 0x0011D26A. [O] Cluster of 2 (0x0011C850, 0x0011C860).

#### 0x0011C870 `tbl_cfg_get_flag28_f` — data-accessor
Reads complement-cfg+0x28 as float (0x000F37C8); twin member 0x0011C87C at +0x2C. [O]
Cluster of 2 (0x0011C870, 0x0011C87C).

#### 0x0011C888 `tbl_cfg_interp2d2_30` — data-accessor
Calls the 201-instr 2-D float kernel 0x000F3BAE with the two handle values
(_LAB_0011C8E8/+4, _LAB_0011C8E4/+4) and cfg+0x30 — the pair-input evaluation of the same
curve family. [O] Cluster of 1.

#### 0x0011C8F0 `diag_mon_gearreq_ctor` — library
Allocate 0xC bytes (0x0009AA34) and preset: [0]=1, u16s at +2/+4/+6 = 0, [8]=6, [9]=1, [10]=6
— constructor of the gear-request monitor struct used by 0x0011C924. Registered from r14
0x0015D10C. [O] Cluster of 1.

#### 0x0011C924 `diag_mon_gearreq_fsm` — logic
Bracket 0x79 (group 121): gates via 0x0011CA64, snapshots the speed word (0x0011CAC0), scales
it (PTR_FUN_0011CB5C value * cfg /1000) and compares against the struct u16; two s16 counters
at +4/+6 step toward −1/+0xFFFE depending on gate state and speed ordering; fires the fault
event (PTR_PTR_0011CB68)(…,1) on request/limit violations (also mode 5 + state 6 path) or
(…,0,1) on the counter overflow path; remembers last mode bytes at +8/+10. Wired from
0x000F43A8. [I] spec 13. Cluster of 1.

#### 0x0011CA64 `diag_mon_gearreq_gate` — logic
Gate for 0x0011C924: three mode objects (PTR_DAT_0011CB6C/74/78, arg 5) must all fail for the
gate to open (bVar1), combined with the per-state flag table PTR_LAB_0011CB80[param_2]
(param_2 clamped to 5 via trap PTR_FUN_0011CB7C when >8); writes the result to [0] and [9]. [O]
Cluster of 1.

#### 0x0011CAC0 `diag_mon_gearreq_snap` — data-accessor
Copies the u16 behind **PTR_DAT_0011CB50 into param+2 — the speed snapshot of the gear-request
monitor. [O] Cluster of 1.

#### 0x0011CACC `diag_mon_gearreq_publish` — logic
Publishes seven monitor fields (byte, s16 x3, byte x2, char) with sub-ids 1,3,4,5,6,7,0x16 on
channel 0x5BF. Wired from 0x000F43A8. [I] spec 10. Cluster of 1.

#### 0x0011CB90 `diag_mon_gearreq_flt_ctor` — library
Allocate 0x1C bytes and preset floats: [0]=1.0, [1..3]=0, [4]=[6]=DAT_0011CDD4 — float twin
struct for 0x0011CBC8. Registered from r14 0x0015D10C. [O] Cluster of 1.

#### 0x0011CBC8 `diag_mon_gearreq_flt_fsm` — logic
Float twin of 0x0011C924: gate via 0x0011CD56, speed via 0x0011CE30 scaled by (cfg+0x3C *
PTR_DAT_0011CDE4)/PTR_DAT_0011CDF0, counters [2]/[3] step toward DAT_0011CDF8/0, fault events
0xFF/0xFE through PTR_PTR_0011CE08 with the mode-change condition, latches modes into
[4]/[6]. Wired from 0x000F43A8. [I] spec 13. Cluster of 1.

#### 0x0011CD4E `diag_mon_gearreq_get_cfg` — data-accessor
Returns ~*(u32*)PTR_DAT_0011CE10 — complement-stored config pointer for the float gear-request
monitor. [O] Cluster of 1.

#### 0x0011CD56 `diag_mon_gearreq_flt_gate` — logic
Float gate: three mode objects (PTR_DAT_0011CE14/1C/20, arg 5); range-checks param_1 to
[0, DAT_0011CE24] (trap via PTR_FUN_0011CE28); sets [0]/[5] to 1.0 when the mode table
DAT_fff8d470[idx] matches PTR_DAT_0011CE2C and the mode condition holds. [O] Cluster of 1.

#### 0x0011CE30 `diag_mon_gearreq_flt_snap` — data-accessor
Reads the u16 at the head of the config (via 0x0011CD4E + PTR_FUN_0011CEC0) as float into
param+4 — float speed snapshot. [O] Cluster of 1.

#### 0x0011CE44 `diag_mon_gearreq_flt_publish` — logic
Publishes the float monitor fields with sub-ids 0x65,0x67..0x6B,0x7A on channel 0x5BF and
closes bracket 0x7A (PTR_DAT_0011CED0). Wired from 0x000F43A8. [I] spec 10. Cluster of 1.

#### 0x0011CEE4 `diag_obj14_vt_ctor` — library
Allocate 0x14 bytes, zero three words, store vtable 0x11D0B0, payload byte at +0x10, zero
+0x11 — constructor of the object published by 0x0011CF14. Registered from r14 0x0015C714. [O]
Cluster of 1.

#### 0x0011CF14 `diag_mon_5af_publish` — logic
Publishes five fields (byte, byte, u32, u32, byte) with sub-ids 1..5 on channel 0x5AF. Caller
0x000F6F34. [I] spec 10. Cluster of 1.

#### 0x0011D044 `tbl_cfg_pick_18_or_1c` — dispatch
Dispatches on the mode byte at param+0x10: 1 → cfg interp 0x0011C7FA-side getter
(PTR_FUN_0011D0E4 → 0x0011C762), 2 → the +0x1C twin (0x0011C77A), anything else → trap
(PTR_FUN_0011D0BC); returns the selected s16. [O] Cluster of 1.

#### 0x0011D074 `diag_obj18_vt_ctor2` — library
Allocate 0x18 bytes, zero three words, store vtable 0x11D0EC and payload param_1 at +0x10 —
constructor for the object published by 0x0011D0F0. Registered from r14 0x0015C714. [O]
Cluster of 1.

#### 0x0011D0F0 `diag_mon_5af_publish2` — logic
Publishes the float twin's five fields with sub-ids 0xB..0xF on channel 0x5AF. Caller
0x000F7002. [I] spec 10. Cluster of 1.

#### 0x0011D16C `diag_cond_range_and_speed` — logic
Condition: mode at PTR_DAT_0011D2AC+0x1C outside {DAT_0011D2B0, DAT_0011D2B4}, a flag pair
(PTR_DAT_0011D2B8/D2BC), value (getter 0x0011C7FA side) below cfg+0x3C, and value (getter
0x0011C806 side) above (cfg+0x34 − cfg+4) → returns 1.0f. [I] Cluster of 1.

#### 0x0011D26A `tbl_cfg_pick2f_18_or_1c` — dispatch
Float dispatch twin of 0x0011D044: 1.0 → 0x0011C850, 2.0 → 0x0011C860, else trap
(PTR_FUN_0011D2A8). [O] Cluster of 1.

#### 0x0011D2E8 `diag_event_on_change` — logic
Edge-triggered event relay: reads the boolean via PTR_FUN_0011D334; on a change from the
stored byte it fires the event call (PTR_PTR_0011D33C)(obj, 0/1) and updates the memory.
Caller 0x000F44E4. [O] Cluster of 1.

#### 0x0011D360 `diag_cond_snap_eval` — logic
Bracket 0x41 (group 65, element family): copies [0] to [1] and stores the boolean result of
PTR_FUN_0011D438(0) into [0]. Wired from 0x000F43A8. [O] Cluster of 1.

#### 0x0011D384 `diag_cond_publish_54f` — logic
Publishes the two condition bytes with sub-ids 1,2 on channel 0x54F. Wired from 0x000F43A8.
[I] spec 10. Cluster of 1.

#### 0x0011D3B0 `diag_obj8_pair_ctor` — library
Allocate 8 bytes and copy the two pointers from PTR_DAT_0011D440 into [0]/[1] — constructor
of the two-slot condition object used by 0x0011D3D6. [O] Cluster of 1.

#### 0x0011D3D6 `diag_cond_snap_eval_f` — logic
Float twin of 0x0011D360: [1]=[0], then selects the pointer PTR_DAT_0011D440[0] or [1] by the
sign of the condition float from PTR_FUN_0011D444(0). Wired from 0x000F43A8. [O] Cluster of 1.

#### 0x0011D3FE `diag_cond_publish2_54f` — logic
Publishes the float condition pair with sub-ids 0xB,0xC on channel 0x54F and closes bracket
0x42. Wired from 0x000F43A8. [I] spec 10. Cluster of 1.

#### 0x0011D44C `diag_ptr_table_selfcheck` — logic
Bracket 0x3A (group 58, element family): for ten 2-instr getters in 0x1075A8–0x107A48 it
compares the returned pointer against the expected RAM word (PTR_DAT_0011D650…0x11D6B4) and,
in a second pass, the complemented word (~PTR_DAT_0011D6BC…0x11D70C); on any mismatch it calls
the error handlers PTR_FUN_0011D658/0x0011D708. Self-check of the redundant pointer/config
table at initialization. Wired from 0x000F43A8. [I] spec 13. Cluster of 1.

### Variant pointer-install stub family (0x11D714–0x11D97C)

Forty-five 4-instruction stubs, all with the same shape: `*(u32*)PTR_DAT_x = DAT_y;` — each
writes one ROM function-pointer literal into a RAM pointer slot. They are invoked in bulk by
the variant dispatcher 0x000F7950 (r0c, itself called from 0x000C218C/0x000C21DE): a selector
byte (PTR_DAT_000F7AD0, values 0/1; anything >1 is logged with id 0x5F and treated as 0)
selects one of two installer families — the 0x11D7xx set here and its 0x11D9xx–0x11DFxx
counterparts (other units) — and the install is re-run whenever the selector changes. Each
RAM slot is written by exactly one `_a`/`_b` pair (alternate implementations); the letter
suffix denotes the position inside the pair. Kind `stub`, confidence [O] for the mechanism,
[I] for the "variant" interpretation (likely hardware/market/mechatronics variant of the
same function). Each cluster has 1 member.

#### 0x0011D714 `var_set_ptr_728_a` — stub
Writes literal DAT_0011D724 into RAM slot PTR_DAT_0011D728; twin 0x0011D71C writes the
alternate pointer. Part of variant-0 install. [O]
#### 0x0011D71C `var_set_ptr_728_b` — stub
Alternate pointer DAT_0011D72C into the same slot 0x11D728. [O]
#### 0x0011D730 `var_set_ptr_744_a` — stub
DAT_0011D740 into slot PTR_DAT_0011D744; twin 0x0011D738. [O]
#### 0x0011D738 `var_set_ptr_744_b` — stub
DAT_0011D748 into slot 0x11D744. [O]
#### 0x0011D74C `var_set_ptr_760_a` — stub
DAT_0011D75C into slot PTR_DAT_0011D760; twin 0x0011D754. [O]
#### 0x0011D754 `var_set_ptr_760_b` — stub
DAT_0011D764 into slot 0x11D760. [O]
#### 0x0011D768 `var_set_ptr_77c_a` — stub
DAT_0011D778 into slot PTR_DAT_0011D77C; twin 0x0011D770. [O]
#### 0x0011D770 `var_set_ptr_77c_b` — stub
DAT_0011D780 into slot 0x11D77C. [O]
#### 0x0011D784 `var_set_ptr_798_a` — stub
DAT_0011D794 into slot PTR_DAT_0011D798; twin 0x0011D78C. [O]
#### 0x0011D78C `var_set_ptr_798_b` — stub
DAT_0011D79C into slot 0x11D798. [O]
#### 0x0011D7A0 `var_set_ptr_7b4_a` — stub
DAT_0011D7B0 into slot PTR_DAT_0011D7B4; twin 0x0011D7A8. [O]
#### 0x0011D7A8 `var_set_ptr_7b4_b` — stub
DAT_0011D7B8 into slot 0x11D7B4. [O]
#### 0x0011D7BC `var_set_ptr_7d0_a` — stub
DAT_0011D7CC into slot PTR_DAT_0011D7D0; twin 0x0011D7C4. [O]
#### 0x0011D7C4 `var_set_ptr_7d0_b` — stub
DAT_0011D7D4 into slot 0x11D7D0. [O]
#### 0x0011D7D8 `var_set_ptr_7ec_a` — stub
DAT_0011D7E8 into slot PTR_DAT_0011D7EC; twin 0x0011D7E0. [O]
#### 0x0011D7E0 `var_set_ptr_7ec_b` — stub
DAT_0011D7F0 into slot 0x11D7EC. [O]
#### 0x0011D7F4 `var_set_ptr_808_a` — stub
DAT_0011D804 into slot PTR_DAT_0011D808; twin 0x0011D7FC. [O]
#### 0x0011D7FC `var_set_ptr_808_b` — stub
DAT_0011D80C into slot 0x11D808. [O]
#### 0x0011D810 `var_set_ptr_824_a` — stub
DAT_0011D820 into slot PTR_DAT_0011D824; twin 0x0011D818. [O]
#### 0x0011D818 `var_set_ptr_824_b` — stub
DAT_0011D828 into slot 0x11D824. [O]
#### 0x0011D82C `var_set_ptr_840_a` — stub
DAT_0011D83C into slot PTR_DAT_0011D840; twin 0x0011D834. [O]
#### 0x0011D834 `var_set_ptr_840_b` — stub
DAT_0011D844 into slot 0x11D840. [O]
#### 0x0011D848 `var_set_ptr_85c_a` — stub
DAT_0011D858 into slot PTR_DAT_0011D85C; twin 0x0011D850. [O]
#### 0x0011D850 `var_set_ptr_85c_b` — stub
DAT_0011D860 into slot 0x11D85C. [O]
#### 0x0011D864 `var_set_ptr_878_a` — stub
DAT_0011D874 into slot PTR_DAT_0011D878; twin 0x0011D86C. [O]
#### 0x0011D86C `var_set_ptr_878_b` — stub
DAT_0011D87C into slot 0x11D878. [O]
#### 0x0011D880 `var_set_ptr_894_a` — stub
DAT_0011D890 into slot PTR_DAT_0011D894; twin 0x0011D888. [O]
#### 0x0011D888 `var_set_ptr_894_b` — stub
DAT_0011D898 into slot 0x11D894. [O]
#### 0x0011D89C `var_set_ptr_8b0_a` — stub
DAT_0011D8AC into slot PTR_DAT_0011D8B0; twin 0x0011D8A4. [O]
#### 0x0011D8A4 `var_set_ptr_8b0_b` — stub
DAT_0011D8B4 into slot 0x11D8B0. [O]
#### 0x0011D8B8 `var_set_ptr_8cc_a` — stub
DAT_0011D8C8 into slot PTR_DAT_0011D8CC; twin 0x0011D8C0. [O]
#### 0x0011D8C0 `var_set_ptr_8cc_b` — stub
DAT_0011D8D0 into slot 0x11D8CC. [O]
#### 0x0011D8D4 `var_set_ptr_8e8_a` — stub
DAT_0011D8E4 into slot PTR_DAT_0011D8E8; twin 0x0011D8DC. [O]
#### 0x0011D8DC `var_set_ptr_8e8_b` — stub
DAT_0011D8EC into slot 0x11D8E8. [O]
#### 0x0011D8F0 `var_set_ptr_904_a` — stub
DAT_0011D900 into slot PTR_DAT_0011D904; twin 0x0011D8F8. [O]
#### 0x0011D8F8 `var_set_ptr_904_b` — stub
DAT_0011D908 into slot 0x11D904. [O]
#### 0x0011D90C `var_set_ptr_920_a` — stub
DAT_0011D91C into slot PTR_DAT_0011D920; twin 0x0011D914. [O]
#### 0x0011D914 `var_set_ptr_920_b` — stub
DAT_0011D924 into slot 0x11D920. [O]
#### 0x0011D928 `var_set_ptr_93c_a` — stub
DAT_0011D938 into slot PTR_DAT_0011D93C; twin 0x0011D930. [O]
#### 0x0011D930 `var_set_ptr_93c_b` — stub
DAT_0011D940 into slot 0x11D93C. [O]
#### 0x0011D944 `var_set_ptr_958_a` — stub
DAT_0011D954 into slot PTR_DAT_0011D958; twin 0x0011D94C. [O]
#### 0x0011D94C `var_set_ptr_958_b` — stub
DAT_0011D95C into slot 0x11D958. [O]
#### 0x0011D960 `var_set_ptr_974_a` — stub
DAT_0011D970 into slot PTR_DAT_0011D974; twin 0x0011D968. [O]
#### 0x0011D968 `var_set_ptr_974_b` — stub
DAT_0011D978 into slot 0x11D974. [O]
#### 0x0011D97C `var_set_ptr_990_a` — stub
DAT_0011D98C into slot PTR_DAT_0011D990; its `_b` twin lives beyond this unit (0x11D984).
[O]

## Unit summary

**220 clusters, 240 named functions, 0 unknown.** Kinds: logic 113, stub 47, data-accessor 43,
library 35, dispatch 2. No `unknown` entries: every function fits one of the unit's five
clear patterns, though many names carry [I] confidence on *what* the monitored quantity is.

### What this slice is

1. **Fault-monitor condition framework** (0x118494–0x11932A and the byte/float monitor pairs
   through 0x11C924/0x11CBC8): C++-style condition objects evaluated through vtables,
   threshold/hysteresis predicates (fixed band 501/8499 in the u16 family), debounce/ramp
   FSMs, per-mille ratio objects, publishes on measurement channels 0x55B, 0x571, 0x575,
   0x593, 0x59B/0x59D/0x59F, 0x5AF, 0x5BF, 0x54F, and fault-event triggers with brackets
   0x39–0x7B (fault groups 57–123). The five-element (A..E) sub-condition sets, the 26-row
   combination tables and the 0x43/0x4A/0x4B/0x66/0x67/0x77/0x78 brackets tie this firmly to
   the **shift-element combination monitors of spec 13 §2b** (groups 56–79) plus adjacent
   monitors (95–123) installed by 0x0011A494 / 0x000F43A8.
2. **Calibration access layer** (0x11B1BE–0x11C888): curve/table getters and validated-slot
   interpolations over a selector-indexed curve set (PTR_DAT_0011B464 / 0x11C8B8), against
   interpolation kernels 0x000B6D6C/0x000B6E10/0x000B6EAE/0x000B7164/0x000B72D8 and float
   twins 0x000F38AA/0x000F395C/0x000F3A18/0x000F3BAE. Spec 09 cross-ref (calibration access);
   the selector is plausibly the driver-type/program selector of spec 09 [I].
3. **Object factory ctors** (0x11890C–0x11D3B0): heap-allocated (via 0x0009AA34) monitor/
   condition objects, several with vtables, registered from r14_app registration functions
   (0x15C714, 0x15C6C8, 0x15CF08, 0x15D10C, 0x15D5DC) into RAM object tables (slots 0x45,
   0x46 at 0xFFF8D298/D29C).
4. **Variant pointer-install stubs** (0x11D714–0x11D97C): 45 four-instruction installers in
   `_a`/`_b` pairs over 22+ RAM slots, driven by variant dispatcher 0x000F7950 (selector byte
   0/1, re-installed on change, invalid values logged id 0x5F).
5. **Decompiler artifacts**: 0x11C50E/0x11C5B6/0x11C636 are loop-body re-emissions of
   0x11C468/0x11C56E (unaffiliated registers) — documented as such, not counted unknown.

### Cross-region leads

- The wiring/eval callers live in **r0c_app 0xF4244–0xF7EC8** (0xF43A8 master wiring,
  0xF5664/0xF5778/0xF59D0/0xF5A1C–0xF5B96 element-monitor wiring, 0xF7950 variant
  dispatcher) — those units should cross-check these names.
- Registered ctors are installed from **r14_app 0x15C714–0x15D5DC**; the RAM object tables
  at 0xFFF8D298+ (slots 0x45/0x46) belong to that unit's documentation.
- Monitor twins continue outside this unit: 0x11D998–0x11DFFC installer set (variant 1 of
  the 0x11D7xx family) and the 0x12xxxx monitor suite (0x12DD94, 0x12E350, 0x12E448,
  0x12E618, 0x12760C members already named here).
- Fault-event targets (PTR_PTR_0011CB68, 0011CE08, 0011A1AC, 0011C94 …) resolve into the
  fault-reporting API of spec 13 §1 — worth resolving there to name the exact DTC per
  bracket id.
