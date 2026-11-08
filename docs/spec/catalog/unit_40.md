# Unit 40 (r10_app)

Application-region clusters 0x0012295A-0x0012C9BC.
#### 0x0012295A `forced_shift_latch_55_aa` — logic
Two-code latch on ctx byte +0x22: code 'U' (0x55) sets it to 1, code 0xAA returns 1 and
clears it when it was set; everything else clears. Called by 0x00122570 (405-instr evaluator
next to the rev-limit forced upshift 0x122536, spec 09 §2.5). Cluster of 1. [I].

#### 0x001229A4 `cfg_pred_3a8_eq2` — data-accessor
Writes *param_1 = 1 when config bytes at DAT_001229c8+0x3a8 == 2 and +0x3a9 in {0,1}, else 0.
Called by 0x000F90E8 (ctx_refresh_tail, unit 32). Cluster of 1. [O].

#### 0x001229CC `gear_change_eval` — logic
Sub-object method (via 0x000F9124) that evaluates an impending gear change: guards a working
window (rpm/gear bounds from cfg PTR_DAT_00122c54 +0x20..+0x34), then sets eight condition
bits through the bitmask helper PTR_FUN_00122c90 on ctx+0xe (bits 0,1,2,4,5,6,7; bit 1 also
latches ctx+0xf/+0x14/+0x16 for a hold window); bit 3 computes the gear-pair transition from
current gear [0x18] vs target [0x1b] through the pair table at cfg+0x10 into ctx+0x15 and
ORs it via the same helper. Result codes: ctx+0x13 pending / ctx+0x12 released, return 0/1.
Callees 0x00123174, 0x001230B0. Cluster of 1. [I] (bit meanings open).

#### 0x001230B0 `gear_change_cond2_gate` — logic
Condition-bit-2 gate for 0x001229CC: returns 1 only when a nine-clause conjunction holds
(speed window via 0x121A56-style lookups on cfg+0x60/+0x70, flags at +0x78/+0x7c, gear [9],
vehicle bytes 0x389/0x38a/0x6c and ctx+0x11 from 0x00123174). Cluster of 1. [I].

#### 0x00123174 `gear_change_release_cond` — logic
Computes ctx+0x11 (0/1) for 0x001229CC from paired speed comparisons: gear bytes [0x11]/[0x1b],
two vtable lookups on cfg+0x68/+0x74 and two cfg table lookups at +0x64 s — result is 1
unless every release side (low side AND high side) rejects. Cluster of 1. [I].

#### 0x00123264 `ramp_to_limit_sm` — logic
Two-state ramp on the sub-object refreshed by 0x000F9038: state 0 latches a start value
(ctx+2 = cfg base) when speeds/flags trip; state 1 adds a rate each pass
(vtable on cfg+8/+0xc, or a fixed cfg+0x1c..+0x20 alternative), clamps to the cfg limits
+0x14/+0x18, sets ctx+0 done inside the window and falls back to state 0 outside; ctx+4
latches a second boolean from a parallel speed test. Cluster of 1. [I].

#### 0x00123410 `shift_window_calc` — logic
Builds a min/max u16 window (defaults 10000 = never) for the ctx refreshed by ctx_refresh_tail
0x000F90E8: eight config-gated optional sources (cfg_opt_get_* family, unit 32) each
contribute a candidate through FUN_0012367e percentage scaling; stores low pair +2/+6/+0xC,
high pair +4/+0x10, enable bytes +0/+1/+0xA/+0xE. Same "optional feature tightens the
window" shape as the shift-threshold corrections (spec 09 §2.3). Cluster of 1. [I].

#### 0x0012367E `lib_pct_scale_curve` — library
One-line helper of 0x00123410: returns curve_0x121CAA((value * byte_percent) / 10) —
percent scaling (units of 0.1 %) followed by a calibrated curve clamp. Cluster of 1. [O].

#### 0x001236E4 `tree_walk_eval` — dispatch
Generic recursive node-graph walker: nodes carry a type code 0..4 and a child list; a
7-method vtable object (param_2, slots +0x14..+0x5c: find/enter/leave/test/report) drives
the traversal — types 0/3 recurse into children, 2/4 return the node (terminal), unknown
codes are reported. Self-recursive; helper 0x00123934 wraps the enter+recurse step.
14 callers in r0c_app (0x000FD96E … 0x00102DCC) — a shared arbitration/sequence engine.
Cluster of 1. [I] (node semantics open).

#### 0x00123916 `tree_node_flag_get` — library
Looks the node up via vtable slot +0x50 (find-by-key on byte param_1+8) and returns the
node's flag byte +8. Companion of tree_walk_eval 0x001236E4. Cluster of 1. [O].

#### 0x00123934 `tree_node_enter_eval` — library
Node-entry step for tree_walk_eval: if param_1[1] set, pushes param_1[0] into the found
node, runs the vtable leave/enter methods, then recurses into 0x001236E4 when the node has
children; returns 0 when the subtree is a leaf. Cluster of 1. [O].

#### 0x00123990 `arb_code_scan_18` — logic
Priority arbiter behind seq_eval_218 (0x000F846E, unit 32): scans codes 0..0x11 (18) via
PTR_FUN_00123b30(table, i, base); code 'c' aborts with 0x34, '\f' sets a latch, '&' and '0'
trigger side calls, otherwise a per-code predicate (PTR_FUN_00123b44 with ctx+0xd0 speeds,
pedal u16, base bytes) either accepts the code (latching it with its parameters into
param_1[0..7]) or continues. Cluster of 1. [I].

#### 0x00123AA2 `arb_code_apply` — logic
Applies the code selected by 0x00123990 for seq_next_code_apply (0x000F8490): evaluates the
code through PTR_FUN_00123b48 (returns '4' by default), moves ctx+8 into +10, forces +8 = 0x30
when a gate rejects, sets +9 = 0xC and flag bit PTR_DAT_00123b54[4].1 on the second gate,
and ORs status PTR_DAT_00123b58 into ctx+0xC. Cluster of 1. [I].

#### 0x00123B62 `shiftidx_step_blend_pct` — logic
Steps a 0..14 index against a 15-entry u16 threshold table (calibration object slot
PTR_DAT_00123da0+8): fetches two blend weights (channels 0/1, clamped to 1000) for the
sub-object, clamps index bounds +0x59/+0x5a to 14, then a do-while steps the index up/down
one at a time until the current speed (+0x40) brackets the table value (event 0x3E when it
leaves the band); stores four channel lookups +0x5c/+0x5b/+0x5e/+0x5f and a 0..100
percentage between two of them into +0x5d (event 0x6B if >100). Same stepping shape as the
gear decision (spec 09 §2.3). Called by ctx_copy_clamp_select 0x000F92A0. Cluster of 1. [I].

#### 0x001240C0 `thunk_129a52_ptr` — stub
3-instr indirect tail call through DAT_001240d4 (target 0x00129A52); reached via the pointer
table behind 0x000F929A. Cluster of 1. [O].

#### 0x00124138 `shift_skip_gate_calc` — logic
Method behind ctx_16c_set_u16 (0x000F8DB6, unit 32): a ten-clause conjunction over gear
code [0x18] (index = code-3 when >2), speed-table lookups on cfg+0x1C/+0x20/+0x28, ring
averages and flags (bytes 0x6E/0x71/0x72, cfg byte 0 = mode, 0x78/0x7A in {0,2}) that sets
param_1[3] = 1 — an allow gate for a limited multi-step change (spec 09 §2.3 skip limits).
Also refreshes param_1[7] and clamps param_1[10]. Cluster of 1. [I].

#### 0x0012435C `shift_skip_abort_cond` — logic
Abort side of the skip-limiter family (behind 0x000F8DCC): when any release condition holds
(gear-based lookup uVar4 via cfg+0x2C, delta-bytes vs 0x00124534 pedal/gear byte, speed
deltas, cfg+0x3C == 1 flag) it clears the whole latched state — *param_1, +8/+0xA speeds,
and the shared state byte *PTR_DAT_00124558 — and copies the result to *param_2.
Cluster of 1. [I].

#### 0x00124410 `shift_skip_reset` — logic
Straight reset of the skip-limiter state: zeroes shared byte *PTR_DAT_00124558 and the
object's flag, u16 +8 and byte +0xA. Behind ctx_128_call1-style forward 0x000F8DE2.
Cluster of 1. [O].

#### 0x0012441E `shift_skip_limiter_step` — logic
Steps the limited target toward (requested - 2): while active (param_1[4]) it forces
*param_3 = param_2 - 2, latches the mode byte (1/2) into *PTR_DAT_00124558, flag bit
PTR_DAT_00124560[4].1 and the reference speed/pedal pair (+8/+0xA); when the target reaches
the limit it calls 0x001244B0 and propagates the done byte param_1[6]. Behind 0x000F8DD6.
Cluster of 1. [I].

#### 0x001244B0 `shift_skip_done_check` — logic
Completion check for 0x0012441E: when the limited target changed, looks the mode byte
(cfg byte 0), a table value (cfg+0x28 slot, column = gear-3) and mode gates decide the
done flag param_1[6] (0/1) or keep it. Cluster of 1. [I].

#### 0x00124570 `lockup_strategy_step` — logic
The lockup-strategy cycle step (spec 09 §3.1): object 0xFFF8AB90, calibration group
0xFFF84F5C (objects 12151-12194). Pulls the four program numbers + two blend weights from
the shift object (param_2+0xC9..+0xCC), runs the four inhibit checks 0x00124DDE/0x00124ED4/
0x0012501E/0x001250F4, then a per-mode (ctx+0x22 = 0..3) threshold evaluation via
0x001249DA with map lookups 0x00124C6A, stepping the request level 0-3 with hysteresis;
codes >3 report event 0x68. Output request via PTR_FUN_00124b0c. Called by seq_push_228
(0x000F84EE). Cluster of 1. [O] (spec 09 §3.1).

#### 0x001249DA `lockup_threshold_blend` — logic
Blends the four program threshold values exactly like shift points (spec 09 §2.3/§3.1):
0x00124C6A fetches each corner (own cell cache +0x1B..+0x1E), then
a = w1*(p2-p1)+p1, b = w1*(p4-p3)+p3, result = w2*(b-a)+a; adds the slip/grade correction
(negative-clamped deltas at +0x26 via cfg+0x54 gate) and the transition offset from
0x00124C04. Called by 0x00124570. Cluster of 1. [O] (spec 09 §3.1).

#### 0x00124C04 `lockup_offset_add` — logic
Selects the per-transition offset array by transition code (0/1 -> cfg+0x48, 2/4 -> +0x4C,
3/5 -> +0x50, else event 0x68 and 0) and reads the array element for gear-1 via
PTR_FUN_00124cec. Called by 0x001249DA. Cluster of 1. [O] (spec 09 §3.1 offsets).

#### 0x00124C6A `lockup_thr_map_lookup` — logic
Threshold-map lookup for one program: point code p (1..90) gives map (p-1)/10 (case 0..8
selecting the object slot PTR_DAT_00124fac+0x24..+0x44, default event 0x61), column
((p-1)%10)*4 + transition (4->2, 5->3 aliases), pedal row param_6; transitions 4/5 subtract
the fixed hysteresis columns (+0xC/+0x10). Caches row/col into the caller's cell pointers.
Spec 09 §3.1 transition table. Cluster of 1. [O].

#### 0x00124DDE `lockup_inhibit_a` — logic
First lockup inhibit (spec 09 §3.1): gated by cfg byte +0x5C, tests the gear/enable byte
pair (ctx 0x18/0x11, enable *PTR_DAT_00124fd0), a table lookup on cfg+0xA4 (row gear-1,
col speed-1), a second on cfg+0xAC and a speed+delta comparison against the u16 at ctx+0x40;
any pass sets ctx+0x2C = 1 (forces request level 0), else 0. Called by 0x00124570.
Cluster of 1. [O].

#### 0x00124ED4 `lockup_inhibit_load_hyst` — logic
Second lockup inhibit (bit 4 source, spec 09 §3.1): with hysteresis state ctx+0x30 it computes
an engine-speed threshold from gear-indexed table lookups (cfg +0x68/+0x6C/+0x70/+0x78/+0x80,
row gear-1) plus deltas, stores it at ctx+0x2E, and flips the latch when the ctx speed at
+0x46 crosses it in either direction. 0x001250F4 turns this into inhibit bit 4. Cluster of 1. [O].

#### 0x0012501E `lockup_inhibit_timed` — logic
Third lockup inhibit (bit 5 source, spec 09 §3.1): when the mode byte cfg+0x84 == 1 and a
conjunction of speed windows (0x121A56-style lookups on cfg+0x94/+0x98/+0x9C), gear band and
the mode byte *PTR_DAT_001251a0 hold, it sets ctx+0x31 = 1 and zeroes the u16 timer at
PTR_DAT_001251a4; the latch clears again once a second lookup uVar10 rises past the timer.
Cluster of 1. [O].

#### 0x001250F4 `lockup_inhibit_bits_build` — logic
Aggregates the lockup inhibit bits into ctx+0x32 through the bitmask helper PTR_FUN_001251ac:
bit 1 = calibration enable word (cfg+8 bit 0 inverted), bit 2 = external request
*PTR_DAT_001251b0, bit 3 = ctx+0x2C (0x00124DDE), bit 4 = ctx+0x30 (0x00124ED4), bit 5 =
ctx+0x31 (0x0012501E), bit 7 = diagnostic gate PTR_FUN_001251b8. Matches the inhibit list in
spec 09 §3.1. Cluster of 1. [O] (spec 09 §3.1).

#### 0x001251BC `lockup_special_case_step` — logic
Two-step override state machine (spec 09 §3.1 "three special cases override the map-based
level"): refreshes the base value via PTR_FUN_00125280(gear+10), forces it when a cfg trio
at PTR_DAT_00125284 (+0x4E/+0x52/+0x54) demands, then steps param[2] 0->1->2 while the
0x00125290 lookup stays below the byte at PTR_DAT_00125288; codes >2 report event 0x68.
Called by 0x00124570. Cluster of 1. [I].

#### 0x0012529C `lib_curve_interp_u16s16` — library
Generic curve interpolation: linear interpolation of y[i] over ascending u16 x[i] with
clamping at both ends (returns y[0] below, y[n-1] above). Callers 0x000C6394/0x000C6462/
0x000C6F44/0x000C6FC6 (r0b region). Cluster of 1. [O].

#### 0x00125308 `slot5_set_s16_a` — logic
Gated indexed setter: when 0 < slot < 6 and the enable gate (PTR_FUN_001253c4 -> 0x000BD548)
passes, writes (slot-1, s16) into the object at PTR_DAT_001253c8 via setter 0x000BDDB0.
Caller 0x0010338C. Cluster of 1. [O].

#### 0x00125336 `slot5_set_u16_a` — logic
Same gated 5-slot setter pattern with u16 value; setter 0x000BDC9C on PTR_DAT_001253d0.
Caller 0x0010338C. Cluster of 1. [O].

#### 0x00125364 `slot5_set_s16_b` — logic
Same gated 5-slot setter pattern; setter 0x000BD84E on PTR_DAT_001253d8. Caller 0x001036E0.
Cluster of 1. [O].

#### 0x00125392 `slot5_set_u16_b` — logic
Same gated 5-slot setter pattern with u16 value; setter 0x000BD72C on PTR_DAT_001253e0.
Caller 0x001036E0. Cluster of 1. [O].

#### 0x001253E8 `pid_s16_step` — library
Fixed-point PID step on a shared controller object (config byte +0x16: bit7 P, bit6
modulus-0x2000 integrator with quotient/remainder at +0x14/+0x10, bit5 D against last input
+0x10, bit4 integral scale /0x10000), gains [2]..[5], output clamped to [6]/[7], with
anti-windup recompute of the input when saturated; result at [9], accumulators [0]/[1].
Used by the shift-phase engine (caller 0x00117032). Cluster of 1. [O] (structure;
application open).

#### 0x001255D6 `pid_s16_mode_decode` — library
Maps mode 1..7 onto the s16 controller's term-enable bits (0x80/0x40/0x20/0x10 at +0x16) —
which PID terms are active; modes 4/6 also enable the D+I pair. Helper of 0x00125660.
Cluster of 1. [O].

#### 0x00125660 `pid_s16_init` — library
Controller initialization: decode the mode (0x001255D6), store the four gains (zeroed when
their term is disabled) and the min/max clamp pair, and clear all state words. Caller
0x00116DC4 (shift-phase area). Cluster of 1. [O].

#### 0x001256EA `pid_s16b_step` — library
Variant-B s16 controller step: recomputes gains from flags, then either integrates with
anti-windup (flag 0x40 path keeps the output inside the clamp by inverting the accumulation)
or precharges the integrator term when the I gain is absent; error = param_2 - param_3 via
the saturation helper 0x000B7888. Caller 0x00116DC4. Cluster of 1. [O].

#### 0x001257EE `pid_lim_set_s16` — stub
Stores the min/max output-limit pair into the controller object (+0xC/+0xE). Caller
0x00117032. Cluster of 1. [O].

#### 0x001257FC `lib_err_norm_32767` — library
Normalized error helper: returns sat_0x000B7888(((a - b) * 0x7FFF) / c) — the standard
s16 error scaling (32767 = 100 %). Callers 0x00117032/0x00116DC4. Cluster of 1. [O].

#### 0x00125818 `pid_f32_step` — library
Float PID step, same object layout as the s16 family (flags [0xB], gains [2]..[5], clamp
[6]/[7], last input [8], outputs [9]): computes P/I/D terms, clamps, and on saturation
recomputes the effective input (anti-windup). Callers 0x00105368/0x001099D4/0x0010A3A0.
Cluster of 1. [O].

#### 0x00125914 `pid_s16b_mode_decode` — library
Byte-identical twin of 0x001255D6 writing the mode bits at +0x2C (variant-B object).
Helper of 0x0012599E/0x00125A32. Cluster of 1. [O].

#### 0x0012599E `pid_s16b_init` — library
Variant-B controller init: mode decode (0x00125914), store four gains, the two float-register
limits and clear state. Callers 0x001052E8/0x00109A5E/0x00109B7A/0x0010A7CA. Cluster of 1. [O].

#### 0x00125A32 `pid_s16b_gains_set` — logic
Gain-update-only variant for the +0x2C-flag controller: mode decode + store the four gains
at +8/+0xC/+0x10/+0x14 (no state clear). Callers 0x00105368/0x00109BC4/0x0010A3A0/0x0010A7CA.
Cluster of 1. [O].

#### 0x00125A9E `pid_f32_gains_set_windup` — library
Float-controller gain update with integrator precharge: refreshes gains [2]..[5] from the
float args (flag-gated), and when the I gain is absent back-computes the integrator state
*param_5 so the clamped output matches; same flag-bit layout as 0x00125818 shifted to bits
31..28. Callers 0x001052E8/0x00109A5E/0x0010A7CA. Cluster of 1. [O].

#### 0x00125B4A `pid_f32b_lim_set` — stub
Stores the u32 min/max limit pair at +0x18/+0x1C of the variant-B float controller object.
Callers 0x00105368/0x00109BC4/0x0010A3A0. Cluster of 1. [O].

#### 0x00125B54 `tbl9_ptr_get` — data-accessor
Returns the 4-byte entry PTR_LAB_00125b68[param_1] for param_1 < 9, else 0 — a 9-entry
pointer/table lookup. Caller 0x0011D2E8. Cluster of 1. [O].

#### 0x00125B6C `tbl_small_idx_map` — data-accessor
Maps an index -1..2 (0 rejected) to the DAT_fff8d4d4[i+1] table entry, defaulting to entry
1 — a small mode/mapping selector. Caller 0x0010D130. Cluster of 1. [O].

#### 0x00125B90 `vtbl_m44_then_step` — logic
Runs vtable method +0x44 on the sub-object at param+8 (self pointer via vtbl+0x3C), then
calls 0x00125C0A(param_1, param_2, param_3). Caller 0x00118DA8 (shift-phase engine area).
Cluster of 1. [I].

#### 0x00125BB0 `obj12_alloc_ctor` — logic
Constructor: takes an object or allocates 0xC bytes through the allocator 0x0009AA34, then
installs the method pointer PTR_LAB_00125d10 at +8, the value param_3 at +0, byte param_2
at +4 and clears +5. Caller 0x00126BF0. Cluster of 1. [O].

#### 0x00125BDC `diag_pub_pair_595_ch12` — logic
Publishes two monitor bytes (+4/+5) through diag_trace2_u8 (0x000D5A7C) on channels 1 and 2
with trace code 0x595. Caller 0x00126C28 (monitor family). Cluster of 1. [O].

#### 0x00125C0A `cmp_class_012` — logic
Classifies a comparison into 0/1/2 at param+5: equal inputs go through the keyed lookup
PTR_FUN_00125d18(param_3, +4), unequal through PTR_FUN_00125d1c(param_2); result stored as
0/1/2. Companion of 0x00125B90. Cluster of 1. [O].

#### 0x00125C46 `vtbl_m44_then_step_f32` — logic
Float twin of 0x00125B90: runs vtable method +0x44 on the sub-object at param_3+0xC then
calls 0x00125CCE(param_1, param_2, param_3). Caller 0x00119428. Cluster of 1. [O].

#### 0x00125C6E `obj16_alloc_ctor` — logic
Constructor: uses or allocates 0x10 bytes (allocator 0x0009AA34), installs method pointer
PTR_LAB_00125d20 at +0xC, value at +0, arg at +4, zero at +8. Caller 0x00126C9E. Cluster of 1. [O].

#### 0x00125CA0 `diag_pub_pair_595_ch6566` — logic
Publishes two monitor bytes via diag_trace_u8 (0x000D572C) on channels 0x65/0x66 with trace
code 0x595 (source/target swapped vs 0x00125BDC). Caller 0x00126CDC. Cluster of 1. [O].

#### 0x00125CCE `cmp_class_012_f32` — logic
Float twin of 0x00125C0A: stores 0.0/1.0/2.0 at param_3+8 from the equal/unequal lookup pair
(PTR_FUN_00125d28/0x00125d2c). Companion of 0x00125C46. Cluster of 1. [O].

#### 0x00125D30 `obj6_alloc_ctor` — logic
Allocates (or reuses) a 6-byte monitor cell: zeroes +0/+1/+4 and presets the u16 at +2 to
0xFFFF (counter preset). Caller 0x0015D1D0 (r14_app). Cluster of 1. [O].

#### 0x00125D5A `mon_cond_eval` — logic
Monitor condition evaluation: six flag-pointer pairs (PTR_DAT_00125f10 slots +4/+8/+0x10/
+0x14 with partners 0x00125f14/18/28/2c), one keyed lookup (PTR_FUN_00125f24) and two speed
bounds (<= 500 and < 0x1F5 via PTR_FUN_00125f34) all gate the result byte *param_1 = 0/1.
Called by 0x00125E7C. Cluster of 1. [O].

#### 0x00125E14 `mon_qual_flag` — logic
Sets param_1[1] = NOT(global flag *PTR_DAT_00125f3c OR condition param_1[0]) — the
monitor-qualified (tested) flag. Called by 0x00125E7C. Cluster of 1. [O].

#### 0x00125E2A `mon_counter_step` — logic
Monitor timer: increments the u16 at +2 (saturating at 0xFFFF), resets it when the latched
state (+4) leaves 5/7 or when the enable pair (PTR_DAT_00125f3c[1] set, flag clear) trips,
then latches the new state. Called by 0x00125E7C. Cluster of 1. [O].

#### 0x00125E7C `mon_step_3part` — logic
Monitor cycle step (called by the r11 monitor scheduler 0x0011A494): trace marker 0x62
(0x000F4700), then condition eval 0x00125D5A, qualification 0x00125E14 and the timer step
0x00125E2A on the monitor object. Cluster of 1. [O].

#### 0x00125E96 `diag_pub_quad_56f` — logic
Publishes the four monitor fields (condition +1, counter u16 +2, state +4, result +0) through
diag_trace2_u8/u16 (0x000D5B32/0x000D5A7C) on channels 1-4 with code 0x56F. Caller 0x0011A494.
Cluster of 1. [O].

#### 0x00125EDE `obj16_ctor_vdat` — logic
Allocates or reuses a 0x10-byte object and installs the pointer DAT_00125f50 at +8 with
zeroed +0/+4/+0xC. Callers '-' (reached via a pointer table). Cluster of 1. [O].

#### 0x00125F54 `mon_cond_eval_f32` — logic
Float twin of 0x00125D5A: paired float pointers (PTR_DAT_00126110 +4/+8/+0x10/+0x14 with
partners), a keyed lookup (PTR_FUN_00126128) and two value bounds (DAT_0012613c) gate
*param_1 = 1.0/0.0. Caller 0x001260AE. Cluster of 1. [O].

#### 0x00126020 `mon_qual_flag_f32` — logic
Float twin of 0x00125E14: param_1[1] = 1.0 only when the source pair differs and the
condition *param_1 <= 0.0. Cluster of 1. [O].

#### 0x00126044 `mon_counter_step_f32` — logic
Float twin of 0x00125E2A: counts +8 up to DAT_0012614c, resets on a state change (states
DAT_00126150/0x00126154 vs the latched 0x00126158) or the enable pair at 0x00126144/0x00126148,
then latches. Cluster of 1. [O].

#### 0x001260C0 `diag_pub_quad_56f_bcde` — logic
Publishes the same four monitor fields as 0x00125E96 but on channels 0xB-0xE (code 0x56F),
closing with trace marker 99. Caller 0x0011A494. Cluster of 1. [O].

#### 0x0012618C `mon_confirm_count_report` — logic
Confirmation counter of the fault monitor (spec 13 pattern): keeps the u16 *param_1 counting
up to the calibrated limit (PTR_FUN_001263dc) while the gear/enable conditions
(PTR_DAT_001263c0[3]/0x001263c4+0x20, cfg thresholds +4/+8) hold; on reaching the limit it
calls report3 0x000A105E (via PTR_PTR_001263d0/d4/d8 slot) with 0/1, on release it counts
back down. Trace marker 0x68. Caller 0x0011A494. Cluster of 1. [O] (spec 13 §report).

#### 0x00126274 `diag_pub_pair_57d_35` — logic
Publishes two monitor shorts (counters) via diag_trace2_u16-family (PTR_FUN_001263e0 ->
0x000D5B32) on channels 3/5 with code 0x57D. Caller 0x0011A494. Cluster of 1. [O].

#### 0x001262C0 `mon_confirm_count_f32` — logic
Float confirmation counter: saturates param_1[1] against the table value at
PTR_DAT_001263e8+0x44, counts *param_1 up (to the limit from PTR_FUN_00126400 + 1) or back
down, and reports 0xFE/0xFF through the 0x000A107C slot (PTR_PTR_00126404) on trip/heal with
counter reset. Caller 0x0011A494. Cluster of 1. [O].

#### 0x00126408 `diag_pub_pair_57d_46` — logic
Publishes two monitor shorts on channels 4/6 (code 0x57D) with trace marker 0x69. Caller
0x0011A494. Cluster of 1. [O].

#### 0x00126440 `mon_pair_check_report` — logic
Two-sub-monitor decision (trace 0x6A): refreshes both sub-objects (PTR_DAT_0012656c/
0x00126574), evaluates two predicates on each; any early predicate reports through the
PTR_PTR_00126594 slot with mode 1, the full-qualification path with mode 0 — the
report(group, data, failed, tested) shape of spec 13. Caller 0x0011A494. Cluster of 1. [O].

#### 0x001264C0 `mon_pair_refresh` — logic
Two update calls: 0x0012E448/0x0012DAE6 move the one monitor object pair
(0x0012656c <- 0x00126598, 0x00126574 <- 0x001265a0). Caller 0x0011A494. Cluster of 1. [O].

#### 0x001264D4 `mon_pair_check_report_f32` — logic
Float variant of 0x00126440: predicates return 1.0; early trip reports code 0xFE, full
qualification 0xFF, through the 0x000A107C slot (PTR_PTR_001265c0). Caller 0x0011A494.
Cluster of 1. [O].

#### 0x0012654E `mon_pair_refresh_rev` — logic
Reverse-direction refresh of the monitor pair (0x0012E05C: 0x00126598 <- 0x0012656c,
0x0012E618: 0x001265a0 <- 0x00126574) with trace marker 0x6F. Caller 0x0011A494. Cluster of 1. [O].

#### 0x001265CC `mon2ph_ctor` — logic
Constructor for the two-phase counter monitor: uses/allocates 0x10 bytes (allocator
0x0009AA34), stores the two context args and zeroes both u16 counters (+8/+0xA) and phase
flags (+0xC/+0xD). Caller 0x0011AD58. Cluster of 1. [O].

#### 0x001265F8 `mon2ph_counter_step` — logic
Two-phase counter step (trace family of 0x0011A494): on first run latches the reference
table byte (ctx+8); each pass evaluates the vtable predicate (methods +0x74/+0x38 on the
context) against a selected limit (0x00126892) and ramps the active u16 counter (+8 or +0xA)
up/down; reaching the active limit flips the phase byte +0xC and zeroes the counter.
Caller 0x0011ADBA. Cluster of 1. [O].

#### 0x001266E2 `diag_pub_tri_577_123` — logic
Publishes the two counters (+8/+0xA as s16) and the phase byte (+0xC) via diag_trace
(0x000D5B32/0x000D5A7C) on channels 1-3 with code 0x577. Caller 0x0011AEE8. Cluster of 1. [O].

#### 0x00126720 `mon2ph_ctor_f32` — logic
Float-layout constructor twin of 0x001265CC: 0x18 bytes, two context args, four zeroed words
(float counters/flags). Caller 0x0011B01C. Cluster of 1. [O].

#### 0x00126754 `mon2ph_counter_step_f32` — logic
Float twin of 0x001265F8: same two-phase ramp via vtable +0x74/+0x38 predicates and
0x001268B4 limit selection; counters as floats at [2]/[3], phase words at [4]/[5].
Caller 0x0011B120. Cluster of 1. [O].

#### 0x00126854 `diag_pub_tri_577_bcd` — logic
Publishes the float monitor's two counters and phase on channels 0xB-0xD with code 0x577.
Caller 0x0011B120. Cluster of 1. [O].

#### 0x00126892 `sel_ptr_deref_s16` — library
Pointer-select helper: returns *(short*)(param_2==1 ? *param_1 : *(param_1+2)) when the
chosen pointer differs from param_1 itself, else 0 — the "optional second source" idiom of
the monitor framework. 2-member cluster with 0x00126BA4 (identical copy, no direct caller).
Cluster of 2. [O].

#### 0x001268B4 `sel_ptr_call_f32` — library
Float twin of 0x00126892: selects *param_2 or param_2[1] and, when distinct, calls the
helper at PTR_FUN_001268e4 (0x000F37DA) to fetch the value. Used by 0x00126754. Cluster of 1. [O].

#### 0x00126908 `diag_pub_byte_561` — logic
Publishes one monitor byte via diag_trace_u8 (0x000D572C) channel 2 with code 0x561.
Callers '-' (pointer table). Cluster of 1. [O].

#### 0x00126920 `obj12_ctor_m26b30` — logic
Allocates (0x0009AA34) or reuses a 0xC-byte object, stores the context arg and zeroes the
state byte; installs method pointer PTR_LAB_00126b30 at +8. Called by 0x00126A70. Cluster of 1. [O].

#### 0x00126948 `mon_sat_counter_step` — logic
Saturating byte counter at +4 (0..0xFE): direction selected by param_2, step allowed only
while the vtable predicate (method +0x20 on the ctx at +8) matches. Callers 0x00119AAC/
0x00119DCC. Cluster of 1. [O].

#### 0x001269C4 `obj12_ctor_m26b3c` — logic
Variant of 0x00126920 with method pointer PTR_LAB_00126b3c; called by 0x00126AFA. Cluster of 1. [O].

#### 0x001269EE `mon_sat_counter_f32` — logic
Float saturating counter at +4 (0..DAT_00126b40): +1.0/-1.0 steps gated by the same vtable
predicate; callers 0x00119F48/0x0011A2E4. Cluster of 1. [O].

#### 0x00126A70 `obj16_ctor_wrap_26920` — logic
Wrapper constructor: allocates 0x10 bytes, runs the inner ctor 0x00126920 (0xC-byte prefix),
then installs method pointer PTR_LAB_00126b48 at +8 and param_2 at +0xC. Callers '-'
(pointer table). Cluster of 1. [O].

#### 0x00126AE2 `vtbl_m20_get_u16` — data-accessor
One-call getter: runs vtable method +0x20 (arg 1) on the object stored at +0xC and returns
the u16 result. Callers '-' (pointer table). Cluster of 1. [O].

#### 0x00126AFA `obj16_ctor_wrap_269c4` — logic
Twin of 0x00126A70 using the 0x001269C4 inner ctor and method pointer PTR_LAB_00126b4c.
Callers '-' (pointer table). Cluster of 1. [O].

#### 0x00126BC6 `sel_ptr_call_f32_b` — library
Second copy of the 0x001268B4 pointer-select idiom calling the code label at 0x00126BEC.
Callers '-' (pointer table). Cluster of 1. [O].

#### 0x00126BF0 `obj16_ctor_wrap_25bb0` — logic
Wrapper constructor around 0x00125BB0 (0xC-byte prefix) with method pointer PTR_LAB_00126d74
at +8 and a zero byte at +0xC. Caller 0x0015CF46 (r14_app). Cluster of 1. [O].

#### 0x00126C28 `diag_pub_byte_597_ch3` — logic
Trace preamble (PTR_FUN_00126d78) then publishes the state byte at +0xC via diag_trace_u8
channel 3, code 0x597. Caller 0x00118B26. Cluster of 1. [O].

#### 0x00126C9E `obj20_ctor_wrap_25c6e` — logic
Wrapper constructor around 0x00125C6E (0x10-byte prefix) with method pointer PTR_LAB_00126d84
at +0xC and a zero word at +0x10. Caller 0x0015CF46. Cluster of 1. [O].

#### 0x00126CDC `diag_pub_byte_597_ch67` — logic
Publishes the state byte at +0xC via diag_trace_u8 channel 0x67, code 0x597 (twin of
0x00126C28 with its own preamble). Caller 0x00119140. Cluster of 1. [O].

#### 0x00126D90 `lab_f32_incr` — stub
Increments a global float at 0x00126F08 by 1.0. Callers '-' (pointer table). Cluster of 1. [O].

#### 0x00126D9C `matrix9x6_byte_lookup` — data-accessor
Bounded matrix lookup: PTR_PTR_00126f0c[col + (row+1)*6] for row in -1..8 and col in 1..5,
else 0 — a 9x6 permission/value matrix indexed by two state codes. Called by 0x00125C0A.
Cluster of 1. [O].

#### 0x00126DCE `matrix_9x9_then_6_lookup` — data-accessor
Two-level lookup: 9x9 byte matrix PTR_DAT_00126f10[current + expected*9] selects the row of
a *6-byte matrix PTR_LAB_00126f14[col + row*6] (col 1..5) — transition-state pair -> value.
Called by 0x00125C0A. Cluster of 1. [O].

#### 0x00126E0A `matrix26x6_byte_lookup` — data-accessor
Bounded lookup PTR_LAB_00126f14[col + row*6] for row < 0x1A (26) and col in 1..5 — a 26x6
state matrix. Caller 0x00118E8C. Cluster of 1. [O].

#### 0x00126E36 `matrix_f32_lookup_62eb6` — data-accessor
Float-indexed bounded matrix lookup: table PTR_LAB_00062eb6, result = t[col + (row+1)*6]
for row in -1..DAT_00126f18, col 1..DAT_00126f1c; out-of-range returns 2.0. Float twin of
the 0x00126D9C integer matrix, feeding cmp_class_012_f32. Cluster of 1. [O].

#### 0x00126E74 `matrix_f32_2level_62c46` — data-accessor
Two-level float lookup: a first table indexed (row, col in 0..DAT_00126f24) at PTR_DAT_00126f28
(row stride 0x24) selects the row of the *6 matrix PTR_LAB_00062c46. Called by 0x00125CCE.
Cluster of 1. [O].

#### 0x00126ECE `matrix_f32_lookup_62c46` — data-accessor
Single-level float lookup into PTR_LAB_00062c46 (t[col + row*6], row 1..DAT_00126f30,
col 1..DAT_00126f1c, default 2.0). Caller 0x0011956C. Cluster of 1. [O].

#### 0x00126F34 `obj8_ctor_counters` — logic
Allocates (0x0009AA34) an 8-byte monitor record: zero byte + three zeroed u16 counters.
Caller 0x0015D578 (r14_app). Cluster of 1. [O].

#### 0x00126F58 `mon_confirm_2counter` — logic
Two-counter confirmation monitor (spec 13 pattern, trace 0x74): state byte +0 and timers
+2/+4/+6; when the operating state is 8 and enable flags hold, ramps the pending counter +6
(against the cfg limit at +0x3C) and switches the fail counter +4 up/down on the speed test
(cfg +0x30) / predicate (PTR_FUN_001271a4); crossing the calibrated limit calls the
report3 0x000A105E slot (PTR_PTR_001271b0) with failed/tested. Caller 0x0011A494. Cluster of 1. [O].

#### 0x001270A6 `diag_pub_quad_589_1234` — logic
Publishes the four monitor fields (byte +0, counters +2/+4/+6) via diag_trace on channels
1-4 with code 0x589. Caller 0x0011A494. Cluster of 1. [O].

#### 0x001270EE `obj16_ctor_zero4` — logic
Allocates a 0x10-byte object and zeroes its four words. Callers '-' (pointer table).
Cluster of 1. [O].

#### 0x00127118 `mon_confirm_2counter_f32` — logic
Float twin of 0x00126F58 (same counters as floats [0..3], report via the 0x000A107C slot
with codes 0xFE/0xFF, trace markers 0x76/0x77 region). Caller 0x0011A494. Cluster of 1. [O].

#### 0x00127300 `diag_pub_quad_589_bcde` — logic
Publishes the float monitor's four fields on channels 0xB-0xE (code 0x589) with trace
marker 0x75. Caller 0x0011A494. Cluster of 1. [O].

#### 0x001273B0 `mon_step_pair_check` — logic
Monitor wrapper (trace 0x72/0x73): runs the confirmation counter 0x001273F0, then two
predicate checks; when either fails it publishes (1, code 0x587) through the 0x000D5DE4
trace family and runs a reset call. Caller 0x0011A494. Cluster of 1. [O].

#### 0x001273F0 `mon_confirm_count_report_2` — logic
Confirmation counter (u16 pair) twin of 0x00126F58's core: ramps the pending/fail counters
against the calibrated limit (cfg +0x38) and the enable-flag pair, reporting through the
report3 slot PTR_PTR_001274f0 on trip/heal. Called by 0x001273B0. Cluster of 1. [O].

#### 0x00127512 `mon_speed_band_report` — logic
Speed-band monitor (trace 0x70): evaluates the condition 0x001275C6 into *param_1, then
reports failed=1 when the speed is below the low threshold with the extra predicates, or
failed=0 in the release branch — both through the report slot PTR_PTR_00127770.
Caller 0x0011A494. Cluster of 1. [O].

#### 0x001275C6 `mon_speed_band_cond` — logic
Condition half of 0x00127512: sets *param_1 = 1 when the cfg speed at +0x1C exceeds the
first threshold and the state byte pair (PTR_DAT_0012777c+0x20/[0x29]) and the second speed
test (cfg +0x20 vs PTR_DAT_00127758) agree. Cluster of 1. [O].

#### 0x0012762A `mon_step_f32_report` — logic
Float monitor step: gates on PTR_DAT_00127780, runs condition 0x001276F0, then reports
0xFE (early trip) or 0xFF (full) via the 0x000A107C slot PTR_PTR_001277a0 with two release
guards. Caller 0x0011A494. Cluster of 1. [O].

#### 0x001276F0 `mon_cond_f32_2762a` — logic
Condition half of 0x0012762A: state word at PTR_DAT_001277a8+0x64 must match, the cfg +0x1C
value must exceed the table value at +0x44, plus the two speed/gate tests; result stored as
0/1.0. Cluster of 1. [O].

#### 0x001277D8 `shift_gear_decision` — logic
The gear decision (spec 09 §2.3): 12-arg entry used by the 22 situation handlers
(0x129B70-0x12D7CC, 0x1302A8). Remaps the four program codes through the +0x1A4 byte table
("same as previous" 0x21 pulls the live values from +0xC9..+0xCC via 0x00128076 hysteresis
setup), then loops at most 8 times: computes up/down shift columns (0x0012D0xx helpers),
compares the (hysteresis-adjusted, +0xDC) output speed with the thresholds at +0xD8/+0xDA,
steps the target gear one per iteration; kickdown latch +0xB6, forced-shift code +0xDE=3
with speed limits +0xE6/+0xE8/+0xEA/+0xEC; finally stores the decided gear/program set
(+0xBA..+0xC3, +0xE2) and writes the caller's target byte. Cluster of 1. [O] (spec 09 §2.3).

#### 0x001279C2 `shift_gear_decide_main` — logic
Register-passing variant of the same decision loop as 0x001277D8 (decompiler shows unaff
registers) whose header resolves the full chain: program blend 0x00128D9C, column picker
0x00128D00, corrections 0x00128E94 and hysteresis 0x00128076 — i.e. the main task entry
behind the pointer table (callers '-'). Cluster of 1. [I].

#### 0x00127D3E `shift_seq_step_toward_target` — logic
Walks the decided multi-gear jump one single shift at a time: from the current gear +0xC2
toward the target +0xE2 it re-checks each intermediate step (0x001284CC per gear), updates
the single-shift target +0xDF and code +0xDE=1, maintains the skip-limit status bits
(+0x8C/+0x94 bit 10/11) and finally publishes the program/gear codes via 0x001280F4.
Called by the situation handlers (0x129D1A cluster members and 20 more). Cluster of 1. [I].

#### 0x00127F14 `shift_seq_step_loop_frag` — logic
Register-only fragment of the 0x00127D3E stepping loop (same 0x001284CC check, +0xDE/+0xDF
update, status bits, final 0x001280F4 publish) — an alternate entry of the same machinery
with callers '-' via the pointer table. Cluster of 1. [I].

#### 0x00127F66 `shift_seq_step_upshift` — logic
Upshift side of the sequential walk (called by 0x00127D3E): steps the working gear down
toward the current gear +0xC2 with per-step 0x001284CC checks, the gear-8 special case via
PTR_FUN_001280d8, then the same +0x84/+0xAD clear and 0x001280F4 publish. Cluster of 1. [I].

#### 0x00128076 `shift_speed_hyst_adjust` — logic
Stores the working output speed at +0xDC; when the shift state (+0xAC) is not 0xF and the
gate passes, subtracts the calibrated slip value (0x001280EC lookup by direction param_3)
through the 0x001280F0 helper — the hysteresis adjustment the decision loop compares against.
Called by 0x001277D8/0x001279C2. Cluster of 1. [O].

#### 0x001280F4 `shift_program_codes_store` — logic
Publishes the decided program set into the shift object: gear code -> +0xC4 and the remapped
family -> +0xC6 (both through the +0x1A4 table); handles program 0x21 ('!') by copying the
previous values, else fills the four program numbers +0xC9/+0xC8/+0xCA and weights +0xCB/
+0xCC — exactly the +0xC7..+0xCC block the lockup strategy reads (spec 09 §3.1).
Callers 0x00127D3E/0x00127F66/0x00129D96/0x0012A138/0x0012B454/0x0012CDBC. Cluster of 1. [O].

#### 0x00128186 `shift_obj_program_refresh` — logic
Refresh of the decided-program block: copies +0xC4 -> +0xC5, then 0x00128C9C (program
pending?), the 0x0012E750/0x0012E8E8 thunks and a final refresh call. Caller 0x0012123C.
Cluster of 1. [I].

#### 0x001281A6 `shift_thr_correction_a` — logic
One of the three open threshold corrections (spec 09 §2.3/§5.4: 0x1281A6, 0x128894,
0x00128C04 — hysteresis/grade/temperature): combines the mode byte (+0xAE), the offset
short +0xB2, pedal/gear-indexed lookups on PTR_DAT_00128358 (columns gear-1/gear-2, special
modes 0xF/8 with the flag at PTR_DAT_001284bc/0x00128364) and clamps the result to the
param_6 ceiling. Callers 0x00128E94/0x00128894/0x0012F67A/0x0012FEB8. Cluster of 1. [O]
(structure; which physical correction is open).

#### 0x00128494 `shift_status_words_set` — logic
Stores the two u32 status words of the shift object (+0x8C/+0x94 — the skip-limit bit sets
touched by 0x00127D3E) and tails into the refresh thunk 0x0012EDF8. Caller 0x0012106A.
Cluster of 1. [O].

#### 0x001284A2 `shift_status_bits_or` — logic
ORs param_2/param_3 into the two u32 status words of the shift object (+0x8C/+0x94).
Callers 0x0012B6E8/0x00129F6E/0x0012ABB8 (situation handlers). Cluster of 1. [O].

#### 0x001284CC `shift_step_check` — logic
Per-step evaluation of one candidate gear change (from param_2 to param_3) used by the
sequential walker: remaps the four program codes ('!' pulls the live set from +0xC9..+0xCC),
picks the map column via 0x00128D00, blends the threshold via 0x00128D9C, checks abort code 3
against the kickdown guards (PTR_FUN_00128794/0x00128798/0x001287a0 vs PTR_DAT_0012879c),
computes the guard speed via 0x00129064 into +0xC0, force codes and min-clamps +0xE0.
Cluster of 1. [O].

#### 0x001284CE `shift_step_check_entry` — logic
Stack-parameter twin of 0x001284CC (identical body with in_stack args); callers '-' — the
pointer-table entry other units call into. Cluster of 1. [O].

#### 0x00128876 `thunk_12e902_ptr` — stub
3-instr indirect tail call through DAT_00128a14 to 0x0012E902. Caller 0x00120858. Cluster of 1. [O].

#### 0x0012887C `thunk_12ec10_ptr` — stub
Indirect tail call through DAT_00128a18 to 0x0012EC10. Caller 0x00121326. Cluster of 1. [O].

#### 0x00128882 `thunk_12ec2e_ptr` — stub
Indirect tail call through DAT_00128a1c to 0x0012EC2E. Caller 0x00121330. Cluster of 1. [O].

#### 0x00128894 `shift_forced_limits_calc` — logic
Computes the forced-shift speed limits of the decision (spec 09 §2.3): two pedal/gear-indexed
threshold pairs through the correction 0x001281A6 into +0xE6/+0xE8 (up) and +0xEA/+0xEC
(down), with 10000/0 defaults when both guards reject. Callers 0x00128E94/0x00128A2C/
0x0012B536/0x0012F67A/0x0012FEB8. Cluster of 1. [O].

#### 0x00128A2C `shift_gear_decision_alt` — logic
Compact variant of the gear decision (same loop skeleton as 0x001277D8: up to 8 iterations,
kickdown latch +0xB6, forced code +0xDE=3, 6-step jump limit) built on 0x00128894 limits and
the step guards 0x001297CC/0x001297D0. Caller 0x0012B3C4. Cluster of 1. [O].

#### 0x00128C04 `shift_thr_correction_c` — logic
Third threshold correction of spec 09 §2.3 (open item §5.4): forwards six args plus the
state byte +0xAE through DAT_00128ce0 and stores the s16 result at +0xB2. Callers
0x00128E94/0x0012F67A/0x0012FEB8. Cluster of 1. [O].

#### 0x00128C54 `lib_blend2_f32` — library
Pure two-stage percentage blend: a = p2 + w1*(p3-p2), result = a + w2*(p5 + w1*(p6-p5) - a),
weights scaled by DAT_00128cec (100.0). Caller 0x0012B90C. Cluster of 1. [O].

#### 0x00128C9C `fwd_12e8f6_c4` — stub
Forwards (obj, gear code +0xC4) to 0x0012E8F6. Caller 0x00128186. 2-member cluster with
0x00128CB2 (identical copy). Cluster of 2. [O].

#### 0x00128D00 `shift_map_columns_get` — logic
Column picker of the 14-column shift maps (spec 09 §2.1): gear 1 -> upshift column 1, gears
2-8 -> upshift column g and downshift column g+6 (8..14), plus the downshift-into column
(0..6) at param_5; other gear codes report event 0x39. Callers 0x001279C2/0x001284CC/0x001284CE.
Cluster of 1. [O] (spec 09 §2.1).

#### 0x00128D9C `shift_programs_blend` — logic
The four-program bilinear blend (spec 09 §2.3): maps each program number to its map value
via 0x001290A8 (code '!' = reuse the first program's value), then
a = p1 + w1*(p2-p1), b = p3 + w1*(p4-p3), result = a + w2*(b-a) (weights in percent,
clamped by 0x121CFA). Callers 0x00128E94/0x001279C2/0x001284CC/0x001284CE. Cluster of 1. [O]
(spec 09 §2.3).

#### 0x00128E94 `shift_thresholds_final` — logic
Threshold finalization (spec 09 §2.3): blends the up-threshold via 0x00128D9C, pulls the
pedal pair (+0xAF/+0xB0) through the +0x1A8-keyed table, runs correction 0x00128C04 (copying
+0xB2 -> +0xB4 for the up case), blends the down-threshold, applies 0x001281A6, computes the
forced limits 0x00128894 and stores up/down pairs at +0xD0..+0xD6 (working thresholds
+0xD8/+0xDA live in the caller). Caller 0x001279C2. Cluster of 1. [O] (spec 09 §2.3).

#### 0x00129064 `lib_blend4_u16` — library
u16 variant of the two-weight blend of four values (same formula as 0x00128D9C, final clamp
via 0x121CFA). Caller 0x001284CC/0x001284CE. Cluster of 1. [O].

#### 0x001290A8 `shift_map_lookup_program` — dispatch
Program-to-map dispatcher (spec 09 §2.2): switch over program 0-99 selecting one of the 105
shift-map object slots (table PTR_DAT_0012948c / 0x0012972c, stride 4; 0x2F-0x31 use the
per-variant selectors PTR_DAT_00129490/0x00129494), then interpolates the chosen map
(0x121A56 bilinear) with the shift column and pedal; unknown programs report event 0x38.
Cluster of 1. [O] (spec 09 §2.2).

#### 0x0012973C `shift_abort_code2_check` — logic
Abort guard of the upshift walk: sets the shift code +0xDE = 2 when the external conditions
(two gates on PTR_DAT_001297dc, the cfg mode at +0x1A8, byte +0xB7 and the scaled speed
+0xCE + +0xB4 vs PTR_DAT_001297e8 != 0xFFFF) demand it; returns whether +0xDE is non-zero.
Called by 0x00127F66. Cluster of 1. [O].

#### 0x001297EC `speed_clamp_store` — logic
Clamps a value into the +0x42/+0x44 window of the speed object and stores the curve-scaled
result (PTR_FUN_00129a54) at +0x40. Called by 0x001298AE/0x001298C8/0x00123B62. Cluster of 1. [O].

#### 0x00129816 `speed_add_clamp` — logic
Adds a delta to the speed at +0x40 with clamping to +0x42/+0x44 (conversion via
PTR_FUN_00129a58). Called by 0x00129852/0x00129AE0. Cluster of 1. [O].

#### 0x00129852 `speed_slew_pedal_update` — logic
Pedal-weighted slew update of the speed: blends the delta with the last accumulator
(*PTR_DAT_00129a68) weighted by a table value (0x00129B12 on PTR_DAT_00129A64), sets the
direction flag +0x46 and applies the result through 0x00129816. Called by 0x001298C8/0x00129AE0.
Cluster of 1. [O].

#### 0x001298AE `speed_min_lim_set` — logic
Stores the smaller of two limits at +0x42 (lower window bound) and re-clamps +0x40 through
0x001297EC. Called by 0x00123B62. Cluster of 1. [O].

#### 0x001298C8 `speed_adjust_arbiter` — logic
Priority arbiter over ~8 speed-adjustment causes: five vtable predicates fill the condition
shorts +0x48/+0x4A/+0x4C/+0x4E and pair +0x50/+0x52 and set/clear the cause bits in +0x3E
(0x00129AC8); the first matching cause (rows 1..8 of PTR_FUN_00129A70) becomes the priority
+0x47, and its weighted delta (0x00129A88 lookup, 0x00129AE0 apply, slew via 0x00129852)
adjusts the speed. Called by 0x00123B62. Cluster of 1. [I] (cause semantics open).

#### 0x00129A88 `arb_code_priority_lookup` — logic
Searches the cause table (PTR_FUN_00129B5C, rows 1..8) for the row matching the cause code
and returns its entry for the given priority. Called by 0x001298C8. Cluster of 1. [O].

#### 0x00129AC8 `arb_cond_bits_set` — logic
Sets or clears one bit of the cause mask at +0x3E of the speed object. Called by 0x001298C8.
Cluster of 1. [O].

#### 0x00129AE0 `speed_delta_apply` — logic
Applies a percentage-weighted delta ((delta * pct)/100, curve 0x00121CD6) to the speed —
direct via 0x00129816, or slewed via 0x00129852 when param_4 is set. Called by 0x001298C8.
Cluster of 1. [O].

#### 0x00129B12 `slew_rate_table_sel` — logic
Mode selector 1/2/3 picks a table slot (param_1 +4/+8/+0xC, other codes event 0x45) and
feeds it through the 0x00129B5C-family lookup — the weight source of the slew update
0x00129852. Cluster of 1. [O].

#### 0x00129B70 `shift_situation_handler_a` — logic
Situation handler (spec 09 §2.3): builds the program numbers exactly as level*5+base
(table lookup via PTR_FUN_00129d4c times 5 plus the base bytes at PTR_DAT_00129d50+0x6C/+0x6D)
and calls the gear decision 0x001277D8 twice (up/down), latching the state bytes and
releasing on the speed/flag guards. Caller 0x001208C0. Cluster of 1. [O] (spec 09 §2.3).

#### 0x00129D1A `shift_walk_fwd` — stub
8-instruction forwarder (args param_2/param_3) to the sequential walker 0x00127D3E; every
member has its own pointer-table entry and the same caller 0x00120D34. 17-member cluster:
0x00129F18, 0x0012B6B6, 0x0012B8BA, 0x0012BB5E, 0x0012BC94, 0x0012BD54, 0x0012BDEE,
0x0012BEC6, 0x0012C122, 0x0012C6C2, 0x0012D3F2, 0x0012D4B8, 0x0012D52C, 0x0012D5CC,
0x0012D6B2, 0x0012D7A6. Cluster of 17. [O].

#### 0x00129D60 `shift_gated_pred_1` — logic
Predicate helper: when a gated config lookup (PTR_FUN_00129e38) passes and param_3 != 1,
sets *param_2 = 1. Caller 0x001208C0. Cluster of 1. [O].

#### 0x00129D96 `shift_target_pm1_check` — logic
Runs the step check (PTR_FUN_00129e3c) for gear and gear-1 with the '!' program codes,
picks the target byte accordingly, then republishes the program codes via PTR_FUN_00129e40.
Caller 0x00120D34. Cluster of 1. [O].

#### 0x00129DFA `shift_gated_pred_2` — logic
Predicate helper: gated config lookup passes and the gear byte *PTR_DAT_00129e44 < 8 sets
*param_2 = 1. Caller 0x0012106A. Cluster of 1. [O].

#### 0x00129E4C `shift_situation_handler_b` — logic
Second situation handler shape: gate pair (PTR_FUN_00129f30 on PTR_DAT_00129f28), a
0x3E-keyed five-value program table (PTR_FUN_00129f38) blended through two lookups
(PTR_FUN_00129f44), then one gear-decision call with the '!' codes. Caller 0x001208C0.
Cluster of 1. [O].

#### 0x00129F50 `tipmode_state_reset` — logic
Resets the tip/manual sub-module: two reset calls (0x0010179C/0x00101B76 on +0x34) and
clears the state bytes +0x60/+0x61. Caller 0x00120858. Cluster of 1. [O].

#### 0x00129F6E `tipmode_handler` — logic
Manual/tip-mode handler (spec 09 §2.5 area): computes four map lookups (two program slots
x two pedal variants, must stay < 100 or it returns 'f'), calls the gear decision
(PTR_FUN_0012a1ec) with those weights plus the level byte +0x71, then runs the follow-up
chain 0x0012A5E6/0x0012A7BE/0x0012A834/0x0012A954 and sets the two 0x100 status bits on
the +0x77/+0x78 flags. Caller 0x001208C0. Cluster of 1. [I].

#### 0x0012A138 `shift_target_from_pending` — logic
Publishes the working target: when the pending gear +0x73 equals the active +0x72 it uses
the walker (PTR_FUN_0012a200); otherwise it runs the step check for both candidates,
picks the feasible one and republishes the codes. Caller 0x00120D34. Cluster of 1. [O].

#### 0x0012A208 `tipmode_refresh` — logic
Cycle refresh of the tip-mode sub-module (clears +0x73 and, unless mode ',', +0x64/+0x65/
+0x66): runs the sub-steps 0x0012A2B6, 0x0012A40A, 0x0012A534, 0x0012A576, 0x0012A68C,
0x0012A728, 0x0012A3B6, a thunk, and the +0x34 update with speed +0x68/flag +0x6A.
Caller 0x0012123C. Cluster of 1. [O].

#### 0x0012A262 `tipmode_release_flags` — logic
Computes the two release/hold flags (*param_2, *param_3) from the +0x77/+0x78 state bytes
and the gate on +0x34 (after the 0x0012A834/0x0012A954 refresh pair). Caller 0x0012106A.
Cluster of 1. [O].

#### 0x0012A2B6 `tipmode_enable_latch` — logic
Enable-latch of the tip module: rising edge of the cfg byte +0x390 sets +0x61, the cfg-gated
enable lands at +0x6B, and the release path (cfg pairs 0x0012a43c/0x0012a440+0xC/+0x50)
clears +0x6B/+0x6C/+0x73/+0x74 and resets the +0x34 sub-object. Called by 0x0012A208.
Cluster of 1. [O].

#### 0x0012A3B6 `tipmode_hold_check` — logic
Hold flag +0x6C: set while the rising-edge byte +0x61 is active and the speed margin
(+0x68 minus the 0x0012A464 lookup) stays above the u16 at PTR_DAT_0012a460, cleared
otherwise. Called by 0x0012A208. Cluster of 1. [O].

#### 0x0012A40A `tipmode_speed_calc` — logic
Refreshes the working pair: speed +0x68 = 0x0012A468(source from 0x0012A4B8) and flag
+0x6A = 0x0012A4F6. Called by 0x0012A208. Cluster of 1. [O].

#### 0x0012A468 `tipmode_speed_curve` — logic
Sub-object curve evaluation: clamps the pedal byte into the calibrated band (+0x84 of
PTR_DAT_0012a6ec), feeds it with the source value through the vtable method +0x14 of the
sub-object at +0x4C and converts via 0x0012A6F4. Called by 0x0012A40A. Cluster of 1. [O].

#### 0x0012A4B8 `tipmode_speed_src_sel` — logic
Speed source select: default the u16 at PTR_DAT_0012a6dc+0x396, or the cfg-gated lookup
(PTR_FUN_0012a6f8) when the gate passes; result converted by 0x0012A6FC. Cluster of 1. [O].

#### 0x0012A4F6 `tipmode_flag_src_sel` — logic
Flag source select: 0/2 from byte +0x392, or the cfg-gated lookup PTR_FUN_0012a700.
Cluster of 1. [O].

#### 0x0012A534 `tipmode_margin_calc` — logic
Margin +0x6E: speed +0x68 minus the calibrated u16 at PTR_DAT_0012a6dc+0x394 (curve-scaled)
when flag +0x6A is clear, else fixed 500 with flag +0x70 = 2. Called by 0x0012A208.
Cluster of 1. [O].

#### 0x0012A576 `tipmode_weight_calc` — logic
Percentage +0x71 = (margin - lo)*100 / (hi - lo) from the two map lookups on
cfg+0x28 (PTR_FUN_0012a708, pedal 0/1), clamped to 100. Called by 0x0012A208. Cluster of 1. [O].

#### 0x0012A5E6 `tipmode_limit_step` — logic
Gear-limit step: sets the compare flag +0x7C (speed +0x6E vs cfg short), and on the
state transitions (+0x64/+0x6C edges, mode 3, or +0x66 clear with +0x7C) recomputes the
limit +0x74 via 0x0012A772; +0x76 = gear [0x18] - limit clamped against the tip-event
counter (PTR_FUN_0012a718 with code 0x2C). Called by 0x00129F6E. Cluster of 1. [O].

#### 0x0012A68C `tipmode_torque_pct` — logic
Percentage +0x75: curve-scaled margin through the cfg+0x2C map, weighted by the cfg+0x30
lookup on PTR_DAT_0012a720 and divided by 100. Called by 0x0012A208. Cluster of 1. [O].

#### 0x0012A728 `tipmode_value_274` — logic
Scaled value +0x7A = (u16_39A * short * 0x274) / u16_398 / 100 — two table values from
PTR_DAT_0012a920 combined with a vtable lookup (0x0012A924) on cfg+0x4C. Called by
0x0012A208. Cluster of 1. [O] (meaning open).

#### 0x0012A772 `tipmode_gear_limit_scan` — logic
Scans the gear-limit table downward: starting at gear PTR_DAT_0012a92c[0x18] bounded by the
tip counter (code 0x2C lookup), decrements while +0x75 is still >= the per-gear table value
(PTR_FUN_0012a934); returns the highest gear that satisfies the limit. Called by 0x0012A5E6.
Cluster of 1. [O].

#### 0x0012A7BE `tipmode_target_gear_sel` — logic
Selects the tip target +0x73: clamps the current limit +0x74 against the active gear +0x72,
takes the sub-object result (+0x34 via 0x0012A93C) when the limit is 0, and otherwise
respects the +0x76 margin against the base gear byte PTR_DAT_0012a938. Called by 0x00129F6E.
Cluster of 1. [O].

#### 0x0012A834 `tipmode_status2_set` — logic
Sets the tip-module status +0x77: on gear change with the module enabled (+0x6B) it records
the matrix lookup (PTR_FUN_0012A94C, gear-1 x base-1) into PTR_DAT_0012a950 (1->2), latches
+0x77 = 1 while the +0x34 predicate is quiet, and releases when the recorded value clears.
Callers 0x00129F6E/0x0012A262. Cluster of 1. [O].

#### 0x0012A954 `tipmode_status3_set` — logic
Sets tip status +0x78: blends the margin +0x6E with a cfg+0x44 lookup and the gear-indexed
percentage (cfg+0x38/+0x48), then a six-clause conjunction (pedal band, hold clear, mode != 3,
margin < the cfg+0x40 speed, recorded value near 0xFF, +0x7A and target +0x73 bounds) decides
the flag. Callers 0x00129F6E/0x0012A262. Cluster of 1. [O].

#### 0x0012AA84 `shift_flag_pair_get` — data-accessor
Copies the global byte *DAT_0012aa90 into both out-params. Caller 0x0012106A. Cluster of 1. [O].

#### 0x0012AA94 `shift_situation_handler_c` — logic
Situation handler with a 0x15-keyed program table (PTR_FUN_0012ab64): two gear/level lookups
must stay < 100 (else error 0x81) and feed one gear-decision call with '!' codes.
Caller 0x001208C0. Cluster of 1. [O] (spec 09 §2.3).

#### 0x0012AB42 `shift_walk_fwd_copy` — logic
Runs the walker (PTR_FUN_0012ab74 -> 0x00127D3E) and copies the resulting target byte into
param_1[0]. Caller 0x00120D34. Cluster of 1. [O].

#### 0x0012AB78 `shift_flag_pair_gated` — logic
Two gated predicate flags: *param_2 = gate PTR_FUN_0012abb0, *param_3 = gate
PTR_FUN_0012abb4 on the same object. Caller 0x0012106A. Cluster of 1. [O].

#### 0x0012ABB8 `shift_situation_kickdn` — logic
Large kickdown-shaped situation handler: refreshes the sub-module state (mode via 0x0012B068,
speeds +0x10/+0x12 and margin +0x14/+0x16, weights via 0x0012B1A6 - error 0x66 above 99),
calls the gear decision (PTR_FUN_0012ae40) and the walker (PTR_FUN_0012ae44), runs the two
guard checks 0x0012AD84/0x0012AE5C into the 0x100 status bits, and latches *param_3 on
success. Caller 0x001208C0. Cluster of 1. [I] (kickdown reading from the shape).

#### 0x0012AD3C `shift_walk_store_1c` — logic
Walker wrapper (PTR_FUN_0012ae44 -> 0x00127D3E): stores the walked target at +0x1C and clears
the active flags +0x18/+0xB when the target changed, the shift code cleared, or the target
fell to the +0xDF limit; also zeroes *PTR_DAT_0012ae4c. Caller 0x00120D34. Cluster of 1. [O].

#### 0x0012AD84 `kickdn_state_check_a` — logic
First guard of 0x0012ABB8: gear-pair latch (+0x19) driven by the matrix lookup
(PTR_FUN_0012ae54 on cfg+0x10, gear-1 capped 8->7, hold byte +9) — records 1->2 into
PTR_DAT_0012ae58 while the main flag is clear, clears when the record clears; result byte
*param_2. Cluster of 1. [O].

#### 0x0012AE5C `kickdn_state_check_b` — logic
Second guard of 0x0012ABB8: computes a load estimate +0x1A = value*0x274*0x39A/0x398/0x18
from the vtable speed (0x0012B0DC) and, per sub-state (+9 = 1 or 2), compares it against the
gear-indexed threshold (cfg+0x20/+0x14, flag 1->2 near the 0xFF bound) into the +0x18/+0xB
flags; gear < 2 forces the fixed 0x7F/-1 window. Cluster of 1. [O] (units open).

#### 0x0012B068 `kickdn_mode_calc` — logic
Sub-module mode select +9 (0/1/2) from the cfg-state byte (PTR_DAT_0012b0cc byte 0) with the
enable gates (0x0012b0f8, bytes 0x0012b0fc/0x0012b104, cfg+0xC), storing gear*3 into
PTR_DAT_0012b2d4 for the weight step. Called by 0x0012ABB8. Cluster of 1. [O].

#### 0x0012B1A6 `kickdn_weights_calc` — logic
Weight set of the kickdown handler: a 0x10-keyed five-value table (PTR_FUN_0012b2e4) fills
the four program weights (+1/+2/+4/+5) via cfg+0x34/+0x30/+0x18 lookups, and the pedal-
between-thresholds percentage (+0x6/+0x7 vs the +0x14 margin) becomes weight +0xA (capped
at 100, 0 when the margin window is the fixed 300). Called by 0x0012ABB8. Cluster of 1. [O].

#### 0x0012B2F8 `shift_gear_speed_pred` — logic
Predicate: with the module gate (PTR_FUN_0012b3ac) in mode 1/2 and gear [0x18] != 8, the
gear-indexed table lookup (PTR_FUN_0012b3b8 on PTR_DAT_0012b3b4) plus the pair
(+0x11 < [0x18] or the u16 at 0x0012b3c0) sets *param_2 = 1. Caller 0x0012106A. Cluster of 1. [O].

#### 0x0012B3C4 `kickdn_limit_handler` — logic
Limit arbiter: caps the gear to the enable byte (8 when above), scans down with 0x0012B4EE,
overwrites with the external clamp (PTR_FUN_0012b5c0 on param_4) and raises *param_3 when
the result stays below the request. Caller 0x001208C0. Cluster of 1. [O].

#### 0x0012B454 `shift_walk_target_pm1` — logic
Walker wrapper: runs the step check for the gear and gear+1 candidates (PTR_FUN_0012b5c4),
picks the feasible target into *param_4 and republishes the codes (PTR_FUN_0012b5c8).
Caller 0x00120D34. Cluster of 1. [O].

#### 0x0012B4AC `kickdn_gear_limits_calc` — logic
Combines the two gear limits: +2 = 0x0012B4EE scan (1..8 against the speed) and
+3 = 0x0012B536 threshold scan, storing the maximum at +4. Caller 0x0012123C. Cluster of 1. [O].

#### 0x0012B4EE `kickdn_gear_scan_down` — logic
Scans the candidate gear downward while the table lookup (PTR_FUN_0012b5d4 on cfg+4/+8 by
flag) stays above the speed ceiling; stops below 2. Called by 0x0012B3C4/0x0012B4AC. Cluster of 1. [O].

#### 0x0012B536 `kickdn_gear_scan_thr` — logic
Threshold-based gear scan: steps gear 8 down to 2, each time computing the forced limits
through PTR_FUN_0012b5e0/0x0012b5e4 and stopping when the up-limit drops below the speed.
Called by 0x0012B4AC. Cluster of 1. [O].

#### 0x0012B5E8 `shift_situation_handler_d` — logic
Situation handler with a 0x1C-keyed program table: when the module byte [0x39] is set, two
config lookups (mode-selected PTR_FUN_0012b6dc/0x0012b6d8) become the two decision weights.
Caller 0x001208C0. Cluster of 1. [O].

#### 0x0012B6E8 `shift_situation_handler_e` — logic
Situation handler: runs the sub-checks 0x0012B90C into the two 0x100 status bits, then a
per-gear switch (1..8) over the cfg slots at PTR_DAT_0012b8e8 (+0xC..+0x24) picks the speed
base blended with the pedal lookup (0x0012B8F0) into *param_1, and the gear decision
(PTR_FUN_0012b8f8, speed delta via 0x0012B8F4) runs with '!' codes, setting status bit 8.
Caller 0x001208C0. Cluster of 1. [O].

#### 0x0012B8CA `kickdn_flag2_clear` — stub
Clears byte +2 of the kickdown sub-module object. Caller 0x0012123C. Cluster of 1. [O].

#### 0x0012B90C `kickdn_speed_eval` — logic
Speed evaluation for 0x0012B6E8: blends the four program numbers of the shift object
(remapped via cfg+4) with the float blend 0x00128C54 into a float factor, multiplies the
gear-indexed base (PTR_FUN_0012bac4, divided by DAT_0012badc when the gate is set) into the
speed word +4, and sets the two flags (*param_2 rising-edge/threshold, *param_3 pedal-band).
Callers 0x0012B6E8/0x0012106A. Cluster of 1. [O].

#### 0x0012BAF0 `shift_situation_handler_f` — logic
Situation handler: 0x1B-keyed table lookup, active when the byte at PTR_DAT_0012bb78 is set;
the first program weight is the table value + '2' (0x32). Caller 0x001208C0. Cluster of 1. [O].

#### 0x0012BB84 `shift_situation_handler_g` — logic
Situation handler with a gear-latch: sets +1 while the mode byte is 7, the map lookup
(PTR_FUN_0012bcd0) stays under the u16 speed and not both gate bytes are set, clearing on
the opposite edge; when latched, the cfg+4 lookup becomes the first decision weight.
Caller 0x001208C0. Cluster of 1. [O].

#### 0x0012BCAA `shift_flag1_clear_if0` — logic
One-edge clear: when [0] is 0 and [1] set, clears [1]. Caller 0x001212F4. Cluster of 1. [O].

#### 0x0012BCE0 `shift_gate_flag_get` — logic
Copies the gated byte (PTR_FUN_0012bcfc on PTR_DAT_0012bcf8 -> 0x000F8DF6) to *param_2 and
clears *param_3. Caller 0x0012106A. Cluster of 1. [O].

#### 0x0012BD00 `shift_situation_handler_h` — logic
Minimal situation handler: when the enable byte at PTR_DAT_0012bd64 is set, the first program
weight comes from the global at PTR_DAT_0012bd68 and one gear-decision call runs with '!'
codes. Caller 0x001208C0. Cluster of 1. [O].

#### 0x0012BD74 `shift_gated_bool_get` — logic
Gate (PTR_FUN_0012bd94 -> 0x000F8EF6) converted to bool at *param_2. Caller 0x0012106A.
Cluster of 1. [O].

#### 0x0012BD98 `shift_situation_handler_i` — logic
Twin of 0x0012BD00 with its own enable/weight globals (PTR_DAT_0012be00/0x0012be04, argument
order swapped in the decision call). Caller 0x001208C0. Cluster of 1. [O].

#### 0x0012BE10 `shift_situation_handler_j` — logic
Situation handler: 0x1E-keyed table (PTR_FUN_0012bee0) plus a mode-selected pair of config
lookups (PTR_FUN_0012bee8 on slot 0/1 and cfg+0/+4) as the two decision weights, active when
the module byte [0x3A] is set. Caller 0x001208C0. Cluster of 1. [O].

#### 0x0012BEF4 `shift_gated_pair_eq2` — logic
Sets *param_2 = 1 only when the gate (PTR_FUN_0012bf20) is 1 and the byte at
PTR_DAT_0012bf24 is 2. Caller 0x0012106A. Cluster of 1. [O].

#### 0x0012BF28 `shift_flag_zero_stub` — stub
Two-instruction stub: *param_2 = 0. Caller 0x0012106A. Cluster of 1. [O].

#### 0x0012BF2E `shift_situation_fastoff` — logic
Downshift-trigger state machine (fast-off/kickdown shape): start when 0x0012C230 reports all
nine conditions (mask 0x1FF), pick the direction (1/2) with 0x0012C132, blend the pedal
weights ((pedal*speed - 100)/100 via 0x0012C1BC) into the working byte +0, hold with the
mode-0..3 release logic (cfg+0x1C modes, threshold from the +0x20/+0x24 lookups), and fire
one gear decision (PTR_FUN_0012c1e0) on release; clears the whole state on trip.
Caller 0x001208C0. Cluster of 1. [I] (trigger semantics open).

#### 0x0012C132 `shift_direction_pick` — logic
Compares the two channel percentages from 0x0012C15E against the byte at PTR_DAT_0012c1e8
and returns 2 when channel 2 drops below it, else 1. Called by 0x0012BF2E. Cluster of 1. [O].

#### 0x0012C15E `shift_channel_pct` — logic
Channel percentage for 0x0012BF2E: channel 1 uses the cfg+4/+0xC slots, channel 2 the
cfg+8/+0x10 slots (PTR_FUN_0012c1f8/0x0012c3dc), multiplied /100 via 0x0012C3E0.
Called by 0x0012C132/0x0012C230. Cluster of 1. [O].

#### 0x0012C230 `shift_start_conds_1ff` — logic
Start detector of 0x0012BF2E: builds the nine-bit condition mask (downshift candidate,
pedal band, pedal < limit, speed window, both channel percentages vs byte 0x71, band
0x38..0x3C, short window +0x30/+0x34, mode 0/1 gate, and the shift-pair bound at cfg+0x50
with the 0x5F/0x9C pair) plus one trial gear decision (PTR_FUN_0012c3fc); start = mask
0x1FF. Called by 0x0012BF2E. Cluster of 1. [O].

#### 0x0012C410 `shift_pedal_value_eval` — logic
Pedal-derived value set for the strategy: map lookups on the cfg slots (+0xC, +8/0x10..0x28
by mode 1..7) blended /100, a 0xFF-compensated copy at [1] (band byte 0x64/0x66), the
compensated variant at [2] and the mode-selected raw value at [3]; *param_2 = overall enable
requiring all values non-zero and the pedal band gates. Caller 0x0012106A. Cluster of 1. [I].

#### 0x0012C634 `shift_situation_handler_k` — logic
Situation handler: requires gate clear, the gated lookup pair (PTR_FUN_0012c6d8) and the
cfg byte 0 == 1; the weight comes from the cfg+4 slot on *PTR_DAT_0012c6e8. Caller
0x001208C0. Cluster of 1. [O].

#### 0x0012C6F8 `shift_hold_flag_get` — logic
Runs the latch step 0x0012C710 and the condition 0x0012C742, returning the flag byte +2.
Caller 0x0012106A. Cluster of 1. [O].

#### 0x0012C70C `shift_latch_and_flag_fwd` — stub
Two-instruction forward to 0x0012C816(param_1, param_2). Caller 0x001212F4. Cluster of 1. [O].

#### 0x0012C710 `shift_latch_step` — logic
Latch byte [0] handling: set on the 0x0012C83E start condition (clearing the u16 at
PTR_DAT_0012c974 on the rising edge), refresh via 0x0012CA1E, clear on the 0x0012C9BC
release. Called by 0x0012C6F8. Cluster of 1. [O].

#### 0x0012C742 `shift_hold_cond` — logic
Hold flag [2]: set while the latch is active and gear [0x18] > 1, cleared when the map
lookups (cfg+0x40/+0x44/+0x48, /100 blend) or the gear/speed pair (u16 PTR_DAT_0012c990,
byte 0x0012c97c vs the +0x80 short) release. Called by 0x0012C6F8. Cluster of 1. [O].

#### 0x0012C816 `shift_latch_and_flag` — logic
param_1[1] = gate(cfg byte via PTR_FUN_0012c9a4) AND latch [0]. Called by 0x0012C70C.
Cluster of 1. [O].

#### 0x0012C83E `shift_start_conds_c83e` — logic
Start condition for 0x0012C710: an eleven-clause conjunction over speed lookups (cfg+4/
+0x1C/+0x10/+0x14), the band short pair (+8/+0xC), gear bytes [0x12] against cfg+0x20/
+0x24, the u16 at PTR_DAT_0012c9b0 vs cfg+0x28/+0x2C and the shift-pair bound (cfg+0x30,
1->2, vs 0x0012c9b4/0x0012c9b8). Cluster of 1. [O].

#### 0x0012C9BC `shift_release_cond_c9bc` — logic
Release condition for 0x0012C710: blends the cfg+0x34 lookup with the cfg+0x3C value
(/100) and tests it through 0x0012CA50 on PTR_DAT_0012ca80. Cluster of 1. [O].

## Unit summary

220 clusters, 238 named functions (rep + every member; the 17-member forwarder cluster
0x00129D1A and the 2-member clusters 0x00126892/0x00126BA4 and 0x00128C9C/0x00128CB2 account
for the extra rows).

Counts by kind: logic 177, stub 29, library 19, data-accessor 11, dispatch 2, irq 0, entry 0,
**unknown 0** (every function had enough observable structure to classify; the [I]-marked
entries name the open semantics rather than the structure).

What this slice is:
- **0x0012295A-0x0012441E** — shift-object sub-methods reached through the r0c_app ctx
  framework (unit 32 callers 0x000F84xx-0x000F92xx): gear-change evaluation with eight
  condition bits, ramp/window builders, the skip-limiter family (gear code-3 indexing,
  target clamped to requested-2).
- **0x00124570-0x001251BC** — the torque-converter lockup strategy, exactly as spec 09 §3.1
  documents it (request level 0-3, four-program blend, inhibit bits, threshold maps with
  transitions 0-5). All cross-checked against the spec.
- **0x0012529C-0x00125B4A** — a three-flavour PID/PD controller library (s16, s16 variant B,
  float) with mode->term-bits decoding, used by the shift-phase engine (0x00116-0x0011A,
  unit 37's region) and 0x00105-0x0010A controllers.
- **0x00125B54-0x001277xx** — diagnostic monitor framework (spec 13 pattern): condition
  evals, saturating/confirmation counters, report3/report slots (0x000A105E/0x000A107C),
  diag-trace publishers (codes 0x561/0x56F/0x577/0x587/0x589/0x595/0x597), permission
  matrices (9x6, 9x9->6, 26x6, float variants) and the small object ctors behind them.
- **0x001277D8-0x0012C9BC** — the shift strategy core of spec 09 §2: gear decision
  (0x001277D8), program->map dispatcher (0x001290A8, 105 maps), four-program blend
  (0x00128D9C), column picker (0x00128D00), threshold corrections (0x001281A6/0x00128894/
  0x00128C04 — still open which is hysteresis/grade/temperature), the sequential walker
  chain (0x00127D3E + 17 thunks), the tip/manual-mode module (0x00129F50-0x0012A954), and
  the situation handlers (0x129B70...0x12D7CC band; eleven in this unit).

Unknown entries: none.

Cross-region leads:
- Unit 32 (r0c_app): my functions are the method bodies behind its ctx_128_call1/ctx_16c_set
  forwards — cross-check 0x001229CC/0x0012435C/0x0012441E against its ctx-map entries.
- Units covering 0x001208xx-0x00122570 (same r10 region, earlier unit): 0x001208C0 is the
  situation-handler scheduler, 0x00120D34 drives the walker thunks, 0x0012106A/0x0012123C/
  0x001212F4 run the predicate/latch refresh chain documented here.
- Units covering 0x0012Dxxx-0x0012Exxx: the monitor helpers (0x0012E448/0x0012DAE6/
  0x0012E05C...) and the shift-object thunks (0x0012E75E-0x0012ECDE) named here as callees
  live there; the object layouts I describe (+0x3E cause mask, +0x40/+0x42/+0x44 speed
  window, monitor cell layouts) should match.
- Spec 09 open items §5.3/§5.4: the situation-handler set is now fully enumerated on this
  side (handlers a-k plus kickdown/fastoff); assigning driving situations to them needs the
  caller context in the 0x0012Exxx unit.
