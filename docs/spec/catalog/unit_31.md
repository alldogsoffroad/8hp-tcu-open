# Unit 31 (r0c_app: 0x000EB6FC-0x000F4E54)

Module context: continuation of the 0x000DD940-0x000F5xxx maneuver/shift-execution band. The
context object (param_1, ~0x270 bytes) has: engaged flag +0x01, state +0x02 (values 0-4, 0x32 =
abort marker), range/mode code +0x03 (4 special-cased), sub-objects +0x58 and +0x1a0 driven
through vtables, +0x108 object, last from/to codes +0x1a1/+0x1a2. Globals DAT_fff8bd6d/6e are
from/to maneuver codes, DAT_fff8bd73 a state the step task watches for skip transitions
(2->3/4/5/6, 3->5, 4->7). Error/event reports use module id 0x36b, line 0xf (helper family
0x000B79E4, cf. unit 30 header). Direct callers 0x000DD940-0x000DE190 are facade-slot
implementations behind the 0x000BAC78 interface object (unit 21 `lib_*_ifc*`), not yet
catalogued themselves. Names use prefix `shift_` as working hypothesis [I]; per-entry
confidence marked [O]/[I].

#### 0x000EB6FC `shift_pred_fl_1d9_10c` — logic
Predicate on the module context: returns 1 iff flags at ctx+0x1d9 and ctx+0x10c are both set.
Called from the module step 0x000DDB04. Cluster of 1. [O]

#### 0x000EB712 `shift_pred_fl_8d90_254` — logic
Predicate: (ctx+0x8d || ctx+0x90) && (ctx+0x254 || (ctx+2==1 && ctx+0x77)). Same caller
0x000DDB04. Cluster of 1. [O]

#### 0x000EB73E `shift_engage_gate_chk` — logic
Start-permission gate for the module: queries validator singleton (PTR_FUN_000eb8d4); on
failure reports event (0x36b,0xf,0,0xd,0x12,3). When global DAT_fff8bd70 is clear, runs a
second validator with (ctx, ctx+0x5a, ctx+0x59, ctx+0x1a1); returns 1 only if that passes and
ctx+0x1d9==0 and ctx+0x26!=0. Called by lockup_step_arbiter 0x000DE190 (unit 29). Cluster of 1. [I]

#### 0x000EB7BC `shift_settle_perm_chk` — logic
Returns 1 (default) or, when ctx+1 (engaged): requires param_3==param_2 and either
param_3==DAT_fff8bd6d (current code) or ctx+0x26 set with both validators
(PTR_FUN_000eb8e4 and *DAT_000eb8e8 on (ctx,0,ctx+0x59,param_2)) returning 0. Callees
0x000EC89A/0x000EC90A; called via facade impl 0x000DD940 (slot 0x000BAC80). [I] "may the
maneuver settle at this code". Cluster of 1.

#### 0x000EB80E `shift_flag_10f_get` — data-accessor
If range code ctx+3==4, returns virtual query on singleton (PTR_FUN_000eb8f0/DAT 0x000eb8ec);
otherwise returns byte ctx+0x10f. Callee 0x000EEAD8. Cluster of 1. [O]

#### 0x000EB828 `shift_hold_active_pred` — logic
Composite predicate: early 1 if (ctx+5 && FUN_000eb8a8) or FUN_000eb86e; else 0 only when
ctx+7==0 && ctx+0x31==0 && FUN_000eb80e==0. Uses helpers 0x000EB86E/0x000EB80E/0x000EB8A8;
caller 0x00116CDE (r10_app). Cluster of 1. [I]

#### 0x000EB86E `shift_pred_8c_93_58` — logic
Range-4 special case returns a virtual query; otherwise 1 unless (ctx+0x8c==0 &&
((ctx+0x93!=1 || ctx+2!=2)) && ctx+0x58==0). Callee 0x000EEA8E. Cluster of 1. [O]

#### 0x000EB8A8 `shift_flag_58_or_eb020` — data-accessor
If state ctx+2==2 returns byte ctx+0x58, else returns FUN_000eb020() (neighbor predicate,
other unit). Cluster of 1. [O]

#### 0x000EB900 `shift_engage_start` — logic
Begin-maneuver entry (facade impl 0x000DDCA2): if engaged and range!=4 first aborts via
0x000EBC60 and latches ctx+0x34=1; derives state 0/4/3 from (target param_2, param_3) and
stores it at ctx+2 (param_5 kept at ctx+0x30 when target==0); arms sub-object ctx+0x58 through
three vtable calls with the new codes; sets engaged, calls 0x000EC6FE(ctx,target,param_7),
clears ctx+4/6/0x2c/0x28 and starts via vmethod. Cluster of 1. [I]

#### 0x000EB9EC `shift_engage_start_alt` — logic
Alternative begin path (facade impl 0x000DD954): returns early with singleton flag
(PTR_FUN_000ebbc0 on 0x000ebbb4 object); when 0, resets sub-object ctx+0x1a0 (two vmethods with
param_4/param_5), validates the pair via PTR_FUN_000ebbc8, computes the 5-8 skip code with
0x000EC134 into ctx+0x1be, then either restarts (1,1) or sets engaged=1 and vmethod(ctx,0).
Cluster of 1. [I]

#### 0x000EBA6E `shift_engage_start_resume` — logic
Begin with resume flag param_7 (facade impl 0x000DDDE8): non-resume arms ctx+0x58, takes state
from singleton query, clears DAT_fff8bd8a; resume path reports event (0x36b,0xf,0,2,0x13,0x53)
when the new (to,from,p4,p5) differs from stored ctx+0x1a1/0x1a2/0x263/0x1dc, resets via
PTR_FUN_000ebbe0 and clears ctx+0x49. State!=0 -> engaged, ctx+0x28=param_6, ctx+4=param_7,
vmethod(ctx). Cluster of 1. [I]

#### 0x000EBB6C `shift_disengage_end` — logic
End-of-maneuver path (callers 0x000DDED6/0x000DDC38): when param_4==0 optional range-4 validity
via 0x000EAD9A+singleton, clears engaged; if state ctx+2!=0 notifies transition observers
(PTR_FUN_000ebdfc with (0,DAT_fff8bd6e,DAT_fff8bd6d), PTR_FUN_000ebe04 with
(param_2,param_3,ok)) and restarts ctx+0x58; when ctx+0x2d==1 && state!=0 && param_2!=param_3
fires observer PTR_FUN_000ebe14 (param_2,param_3,DAT_fff8bd6d); final vmethod on 0x000ebe18
object. Cluster of 1. [I]

#### 0x000EBC60 `shift_cancel_and_notify` — logic
Cancel path (callers 0x000DDE78, 0x000EB900): reports event (0x36b,0xf,0,7,0x16,0xb0,+param_4)
when param_4&&param_5; clears state 0x32; when ctx+0x34==0 captures a u16 from ctx+0x8c
(vmethod) into ctx+0x24, and if state!=0 clears engaged, notifies observers with
(DAT_fff8bd6d,DAT_fff8bd6d,...) and (+0x2d path via PTR_FUN_000ebe34), resets ctx+0x58 and
restarts; else just clears ctx+0x34. Returns the 0x000ebe3c query on ctx+0x108. Cluster of 1. [I]

#### 0x000EBD84 `shift_pair_set_state3_fwd` — logic
Two vtable forwards: sub-state :=3 on ctx (PTR_FUN_000ebe40(ctx,3)), then singleton
0x000ebe44 method with (param_2,param_3). Callees 0x000EEBA6/0x000ED988; facade impl of
0x000DDC14. Cluster of 1. [O]

#### 0x000EBD9E `shift_byte_set_state3_fwd` — logic
Same pattern with one byte: sub-state :=3 (PTR_FUN_000ebe40(ctx,3)) then singleton method
(param_2). Callees 0x000EEBB0/0x000ED988; facade impl of 0x000DDC1E. Cluster of 1. [O]

#### 0x000EBDB4 `shift_obj_notify_noargs` — stub
Single 4-instruction tail: vmethod call on singleton 0x000ebe44 with no arguments (callee
0x000EEBB8); facade impl of 0x000DDC30. Cluster of 1. [O]

#### 0x000EBDBC `shift_task_step` — logic
Main periodic step of the module (264 instrs, called via veneer 0x000EB344). Early-out with
event (0x36b,0xf,0,8,10,0x3b) when ctx+0x34 set. Captures u16 (PTR_FUN_000ec068) into ctx+0x1e
and a difference (+0x94 - +0x7e of the 0x000ec074 object) into ctx+0x14. When not engaged:
clears outputs, steps ctx+0x58/0x1a0 to 0 and runs 0x000EB476. When engaged: recomputes target
via 0x000EC788(ctx, ctx+0x5a, ctx+0x59, ctx+10, ctx+0x29) and on change notifies via
PTR_FUN_000ec084/0x000ec08c (also on DAT_fff8bebb/DAT_fff8bd73 edge changes); updates ctx+5
from singleton query (PTR_FUN_000ec090 with (ctx+3,ctx+5)); steps both sub-objects and two more
ctx vmethods. Final block latches ctx+0x33 "skip transition" when DAT_fff8bd73 moved
2->3/4, 2->5/6, 3->5 or 4->7 (then calls 0x000ec0c8), and forwards two s16 values of the
0x000ec06c singleton plus (ctx+5, ctx+0x58) to the 0x000ec0d4 observer. Cluster of 1. [I]

#### 0x000EC05C `shift_ptr_b4_s16_get` — data-accessor
Returns the s16 pointed to by the pointer at singleton 0x000ec0dc object +0xb4. Caller
0x000EB420 (other unit). Cluster of 1. [O]

#### 0x000EC0E0 `shift_meas_register_fields` — logic
Initialization-time registration (caller 0x000DE0FE lockup_init_callsystems, unit 29):
forwards six context fields (+0x54,+0x1c,+0x4b,+0x1a,+0x53,+0x3d) to a registrar singleton
(PTR_FUN_000ec278), then two more registrations (0x000ec284 object) and init/reset calls on
globals DAT_fff8bd6c and DAT_fff8beb4. [I] measurement/telemetry field registration (spec 10
pattern). Cluster of 1.

#### 0x000EC134 `shift_skip_target_calc` — logic
Maps (param_2,param_3,lever byte ctx+0x5a) to codes 5-8: (3,4,6)->6, (3,4,8)->5, (2,3,6)->7,
(2,3,5)->8; default returns ctx+0x5d. Output domain 5-8 matches the range-code domain of
spec 14 §4.1. Called by 0x000EB9EC. Cluster of 1. [I]

#### 0x000EC182 `shift_code_class_0_4` — logic
Pure classifier of a 0-9 code (likely gear/range) into classes 0-3: outside 3..9 -> 1 only
when param_4==4; 3..9 -> 1 (4, or any with param_3 set), 2 for 5..9-with-flag/6/8-clear/9,
3 for 8-with-flag, 1 for 7-clear. 49 callers across 0x000E5xxx-0x00118xxx (measurement/task
band) — a shared domain helper of this module. Cluster of 1. [I]

#### 0x000EC1F0 `shift_code_class_wrap9` — logic
Wrapper over 0x000EC182 adding the special case code 9 -> 4. 10 callers in the 0x000E6-E7
band. Cluster of 1. [O]

#### 0x000EC20A `shift_transition_perm_chk` — logic
Permission matrix for (param_2 in 1-4, param_4 in 1-9) maneuvers: fixed pairs
(1,3/4),(2,5/6),(3,8),(4,9) always allowed; 1/2/3/4 targets with param_3==1 not allowed;
anything else allowed; then dynamic gates 0x000EC40E (context ok) and per-type checks
0x000EC614 (type 4), 0x000EC578+0x000EC4D8 (type 3), 0x000EC4D8 (type 2) can veto, plus a
global-object veto via PTR_PTR_000ec4b0. Callers 0x000EB03A, 0x00112DB0, 0x001180BE. [I]
shift-transition allow check. Cluster of 1.

#### 0x000EC362 `shift_tbl_class_lookup` — logic
Index byte at 0x000ec4b8 object +0xb8 clamped <5 (else event 0x36b/0xf code 0x99, use 4);
selects a u32 slot from the 0x000ec4b4 object table (+0x2c/+0x34 by index *PTR_DAT_000ec4c4)
for param_2 in 1-4; calls vmethod (slot,u8) and clamps the char result to 4. Callers
0x000EB03A, 0x00112DB0, 0x001180BE. Cluster of 1. [I]

#### 0x000EC3FC `shift_tbl3_byte_get` — data-accessor
Byte-table read: PTR_LAB_000ec4cc[p3 + p2*3] — a 3-column table indexed by the two byte args.
Caller 0x00115884 (r10 band). Cluster of 1. [O]

#### 0x000EC40E `shift_win_chk_map_d0` — logic
Speed/condition window check used by 0x000EC20A: reads s16 at +100 and u16 +0x7c of the
0x000ec4b8 object, interpolates the calibration maps at slots +0xf0/+0xe0/+0xd0 (indices
param_3+1, param_4+1, helpers 0x000B6CE8/0x000B6D14) and requires low<ref<high; then requires
param_2 below the byte limit selected at +0x80 and above +0x84 (mode 1) / +0x88 (mode 2).
Cluster of 1. [I]

#### 0x000EC4D8 `shift_win_chk_map_d4` — logic
Same window-check family over the 0x000ec75c object: maps +0xf4/+0xe4/+0xd4, byte limits
+0x8c (floor) and +0x90 (mode 1) / +0x94 (mode 2). Callers 0x000EB03A, 0x000EC20A,
0x00112DB0, 0x001180BE. Cluster of 1. [I]

#### 0x000EC578 `shift_win_chk_map_d8` — logic
Window check over the 0x000ec75c object with maps +0xf8/+0xe8/+0xd8; additionally requires
byte limits +0x9c < param_2 < +0x98 and param_5==1. Same 4 callers. Cluster of 1. [I]

#### 0x000EC614 `shift_win_chk_map_dc` — logic
Window check over the 0x000ec75c object with maps +0xfc/+0xec/+0xdc and byte window
+0xa4 < param_2 < +0xa0; used for the type-4 gate in 0x000EC20A. Same 4 callers. Cluster of 1. [I]

#### 0x000EC6A6 `shift_cond_flag7_update` — logic
Updates condition byte ctx+7 (range-4 forces 0; already set keeps value): compares u16 from
0x000EC7CC(ctx) with global 0x000ec768, the speed object (+0xc8 of 0x000ec75c chain) with
0x000ec76c, and byte +0x2c of the 0x000ec770 object (0/3/4 keep old). Called each step by
0x000EBDBC. Cluster of 1. [I]

#### 0x000EC6FE `shift_type_flags_8_9_set` — logic
Called by 0x000EB900 at engage time: clears ctx+8/ctx+9; when param_3 and ctx+0x30 set, two
singleton queries (0x000ec774/0x000ec77c) gate setting ctx+8=1 (type 1) or ctx+9=1 (type 2).
Cluster of 1. [O]

#### 0x000EC74C `shift_flag_ne1_784_get` — data-accessor
5-instruction predicate: returns *DAT_000ec784 != 1. Caller 0x000EB1F0 (other unit). [O]
Cluster of 1.

#### 0x000EC788 `shift_target_code_recalc` — logic
Target-code computation used by the step task and 0x000ED9B2: two global vmethod queries
(0x000ec978/0x000ec97c); if either is non-zero the result is forced 0, otherwise when
param_5==0 or param_4==2 the code is replaced by the vmethod on singleton 0x000ec980 with
(param_2,param_3). Callees 0x0011632E/0x00116356/0x000B7C26. Cluster of 1. [I]

#### 0x000EC7CC `shift_ref_s16_by_state` — data-accessor
Selects an s16 from the 0x000ec988-object slots +0xb8/+0xbc (states other than 1) or
+0xc0/+0xc4 (state 1) depending on state ctx+2 and whether DAT_fff8bd6d or DAT_fff8bd6e is
code 1. Caller 0x000EC6A6 (condition-flag update). [I] reference speed/value by state and
first-gear involvement. Cluster of 1.

#### 0x000EC81C `shift_ratio_window_pred` — logic
Arithmetic window predicate using u16s +0x7c/+0x7e of the 0x000ec98c object: requires
+0x7c < 0x1fe1 and blends (param_4-param_2) by param_3/255 (scaled /1000) to test a value
between the two +0x7e-relative bounds. Caller 0x000EDE82. [I] speed-blend window check.
Cluster of 1.

#### 0x000EC89A `shift_win_chk_obj_988` — logic
When param_2==0: window checks against the 0x000ec98c object — u16 +0x94 below *(obj+0x4c
ptr), s16 +0x86 below *(+0x54 ptr), interpolated map (+0x50 ptr, indices param_3+1/param_4+1)
below +0x86, and byte +0x9c below 0x000EC916(). Callers 0x000EC956 and 0x000EB7BC (validator
pair). Cluster of 1. [I]

#### 0x000EC90A `shift_ptr4_byte_get` — data-accessor
Returns the byte reached through the pointer at 0x000ec988-object +4. Used by 0x000EC956 and
0x000EB7BC. Cluster of 1. [O]

#### 0x000EC916 `shift_mode_byte_select` — logic
Selects a byte pointer from the 0x000ec988 object slots +0x58/+0x5c/+0x60 according to a
global vmethod result of 1/2/other (mode selection). Callee 0x000EB158. Cluster of 1. [O]

#### 0x000EC956 `shift_veto_reason_scan` — logic
Start-veto scan (162 instrs; callers 0x000EB73E engage gate and 0x000EB5A6): writes a distinct
reason code to ctx+0x52 for each failed condition — 0: ctx+0x28 set; 1/2: state 2/1 with
ctx+0xe above byte limits (0x000ecae8 +0x48/+0x44); 2: 0x000EC89A fails; 3: u16 window vs
0x000ecaec +0x7e (state 2/1, limits +0x68/+100); 4: interpolated map (+0xcc ptr) <= s16 +100
of 0x000ecaec; 5: singleton query (0x000ecaf4)==1; 6: state 2 with vmethod on ctx+0x58; 10:
(0x000ecb00 obj)==1 and *(+0x168 ptr)==1; 0xc: state 2 pair check via 0x000ecb10 with
(ctx+0x59, vmethod on ctx). Returns 1 when all pass. Cluster of 1. [I]

#### 0x000ECB14 `shift_ctx_blocks_swap` — logic
990-instruction double-buffer swap: copies the ~0x148-byte working block at ctx+0x58 to the
temp at ctx+0x320, block ctx+0x1a0 to ctx+0x58, and the temp back to ctx+0x1a0 (u1/u2/u4
preserving field widths); then steps both blocks via vmethod 0x000edb20 and sets +0x5b=1,
+0x1a3=0. Called from the resume path 0x000EBA6E. [O] swap of the two maneuver sub-contexts.
Cluster of 1.

#### 0x000ED906 `shift_sub_restart_pair` — logic
Calls 0x000ED988(ctx, state) then restarts (vmethod 0x000edb24 with 1) sub-object B (ctx+0x1a0)
when param_2==0, else A (ctx+0x58). Callers 0x000EDC5A and 0x000EBDBC. Cluster of 1. [O]

#### 0x000ED92E `shift_sub_query_by_sel` — logic
Vmethod 0x000edb28 on sub-object B (param_2==0, result returned) or A; when param_2==1 also
notifies singletons 0x000edb2c/0x000edb34 with (state, DAT_fff8bd71) and returns the
0x000edb3c query. Callers 0x000EDC5A, 0x000EBDBC. Cluster of 1. [O]

#### 0x000ED988 `shift_mode_from_state_tbl` — logic
Derives the range/mode byte ctx+3 (and mirrors it to +0x2f) from table
PTR_LAB_000edb44[state + ctx+10*3]; states 3 and 4 force mode 4. Callers 0x000ED906,
0x000EBD84, 0x000ED9B2, 0x000EBD9E. Cluster of 1. [O]

#### 0x000ED9B2 `shift_sub_select_reset` — logic
Selection/reset helper of 0x000EDC5A: for param_2==1 recomputes the code at ctx+10 via
0x000EC788(ctx, DAT_fff8bd6e, DAT_fff8bd6d, 2, 0), loads ctx+0xe from global 0x000edb4c,
clears ctx+7, sets +0x53=0xff/0 and +0x5c/+0x1a4=0; then vmethod 0x000edb50 on sub-object A
(or B when param_2==0), 0x000ED988(ctx,state), and refreshes ctx+5 from singleton
0x000edb2c (state, +5, 1, 0). Cluster of 1. [I]

#### 0x000EDA48 `shift_range4_ready_chk` — logic
State-dependent readiness (callers 0x000EAD9A, 0x000EB1F0): state 4 — singleton query
(0x000edb58,0) latches ctx+0x2d and compares a u16 (0x000edb60) with DAT_fff8bd8e; state 3 —
query with ctx+0x58, returns the +0x2d latch; state 0 — reports event (0x36b,0xf,4,0x15,0xd1)
when (DAT_fff8be2f==0 && DAT_fff8bf77==0) or state 0 or not engaged, else when ctx+6!=0x32
checks singleton 0x000edb64, ctx+0x58, 0x000edb6c and runs 0x000EDB70(ctx); result 1 when
+0x2d set or (+0x10c && u16(+0x7c)<=u16(+0x112)). Cluster of 1. [I]

#### 0x000EDBA4 `shift_flag_32_speed_cond` — logic
Sets boolean ctx+0x32: true when DAT_fff8bd71 is 1 or 2 AND the byte via 0x000edcc0-object
+0x74 pointer <= ctx+0xe. Caller 0x000EDC5A. Cluster of 1. [O]

#### 0x000EDBCC `shift_start_avail_pred` — logic
Predicate: global *PTR_DAT_000edcc4==1 && *PTR_DAT_000edcc8==0 && ctx+0x31==0. Caller
0x000EDE82. [I] "start available". Cluster of 1.

#### 0x000EDBEA `shift_pred_dbg_downshift` — logic
When the byte via 0x000edcc0-object +0x70 pointer is set and ctx+0x8c==0: compares two s16
queries (0x000edccc on ctx, 0x000edcd0 on DAT_fff8bd80); if second < first and a check on
DAT_fff8bda0 passes with ctx+4==0, ctx+7==0 and a query with ctx+0x94, returns 1. Caller
0x000EDE82. [I] downshift-related condition. Cluster of 1.

#### 0x000EDC5A `shift_engage_notify_all` — logic
Central engage-time fan-out shared by all three begin paths (0x000EB900/0x000EB9EC/0x000EBA6E;
183 instrs). When state!=0: optional 0x000ED9B2 (when param_2==0 or ctx+4==0), then
0x000ED906+0x000ED92E. param_2==0 resets sub-object B (three vmethods); otherwise clears
ctx+0x54/+0x2d, runs 0x000EE2B6, singleton 0x000edf38 call, clears +0x35, restarts A
(0x000edf40/0x000edf44 unless range-4, then 0x000edf48(1)), runs 0x000EDBA4 unless range-4,
and forwards observers: 0x000edf58 with two s16 queries of 0x000edf4c plus (ctx+5, ctx+0x58),
and 0x000edf60 with (DAT_fff8bd6e, DAT_fff8bd6d, DAT_fff8be2f); restarts the 0x000edf64
object. Range-4 additionally notifies 0x000edf74 (bytes +0x27/+0x2c/+0x2b of 0x000edf70) and
0x000edf78 with (ctx+3, ctx+2). For states other than 3/4 restarts the +0x108/+0x250 and
+0x8c/+0x1d4 sub-blocks (selected by param_2); param_2==1 also restarts +0x108(1), calls
0x000edf8c and returns the 0x000edf94 query on 0x000edf4c. Cluster of 1. [I]

#### 0x000EDE1C `shift_flags_b_c_d_update` — logic
Step-task helper (caller 0x000EBDBC), range-4 skipped: ctx+0xb from a vmethod on DAT_fff8bd6c;
ctx+0xc = result of comparing a query with DAT_fff8bd80 (comparison direction flips when
state==1); ctx+0xd = (ctx+0xb || ctx+4) && (ctx+0x58==0 || *PTR_DAT_000edfa0). Cluster of 1. [O]

#### 0x000EDE82 `shift_next_action_code_calc` — logic
Computes the action/reason byte ctx+0x2c (173 instrs; callers 0x000EADBA/0x000EADA6 in the
preceding unit): zeroed unless 0x000EDBCC says start available. For skip codes 3/4 (ctx+0x5d)
uses 0x000EE098 and lever-state comparisons (byte via 0x000ee1f8-obj +0x6c ptr vs 1/2/3) to
set 0x32 or 0x1e; otherwise 0x000EDBEA sets 10, and a +0x32 path with a u16 window
(0x000edfa4/0x000edfa8) plus gear queries (DAT_fff8bd6d/6e via 0x000edfb0/0x000edfb8, code
0x55) sets 0x14. Final gates: singleton 0x000ee1fc must be 1 and 0x000ee200/0x000ee204 both 0,
else code cleared; leftover codes 10 (or 0x1e for skip 3/4). Cluster of 1. [I]

#### 0x000EE098 `shift_pred_cond_ee208` — logic
Downshift-style condition: s16 query (0x000ee208) above the query of s16 ctx+0x6c
(0x000ee20c), byte check (0x000ee210 obj, ctx+0x94), ctx+4==0, ctx+7==0 and the byte via
0x000ee1f8-obj +0x6c pointer non-zero. Callers 0x000EDE82, 0x000EADD6. Cluster of 1. [I]

#### 0x000EE0EC `shift_phase_m1_actions` — logic
Per-phase actions of phase machine 1 (phase byte ctx+0x40): phase 1 with to<from
(+0x1a2<+0x1a1) captures a counter via vmethod on sub-object +0x1a0 into +0x46; phase 2
notifies the +0x300-object vmethod (+0x14 slot) with (+0x46, 0, 0x514) and captures a second
counter into +0x44. Caller 0x000EE18E. Cluster of 1. [O]

#### 0x000EE154 `shift_phase_m1_done_pred` — logic
Machine-1 phase-complete predicate: phase 1 needs ctx+0x58==1; phase 2 needs s16 global
0x000ee220 < counter +0x46; phase 3 only proceeds while trigger ctx+0x41 set (returns 1).
Caller 0x000EE18E. Cluster of 1. [O]

#### 0x000EE18E `shift_phase_m1_step` — logic
Step driver of phase machine 1 (caller 0x000EBDBC): resets to phase 3 / counter 0x514 when
(not engaged or state!=1) and phase!=2; otherwise refreshes trigger +0x41 from two singleton
queries (0x000ee224) requiring ctx+0x58==0 (edge resets index +0x42); while
0x000EE154 says done, advances the phase through the state list PTR_LAB_000ee494 (skipping
duplicates) and runs 0x000EE0EC; finally phase 2 reloads the counter via the +0x300-object
vmethod (+0x20 slot, arg +0x44), other phases park at 0x514 (into +0x46). Cluster of 1. [O]

#### 0x000EE2BE `shift_phase_m2_actions` — logic
Per-phase actions of phase machine 2 (phase byte ctx+0x48): phase 1 loads +0x4c from the
0x000ee498-object +0x160 pointer, +0x304 = s16(*PTR_DAT_000ee49c)<<7, +0x308=+0x4b; phase 3
calls the +0x31c-object vmethod (+0x14 slot) with (+0x50,0,0x514); phase 4 clears +0x49.
Caller 0x000EE38C. Cluster of 1. [O]

#### 0x000EE31E `shift_phase_m2_done_pred` — logic
Machine-2 phase-complete predicate: phase 1 — +0x4c<=+0x4b or not engaged; phase 2 — not
engaged; phase 3 — s16 global 0x000ee4a4 < counter +0x50 or engaged; phase 4 — false unless
engaged with +0x49 set; default returns engaged. Caller 0x000EE38C. Cluster of 1. [O]

#### 0x000EE38C `shift_phase_m2_step` — logic
Step driver of phase machine 2 (caller 0x000EBDBC): latches the singleton enable (0x000ee4a8)
into +0x49 once (resets index +0x4a); when disabled parks at phase 4 / counter 0x514;
otherwise while 0x000EE31E says done, advances the phase through the state list at
PTR_LAB_0006353c_2_000ee4b0 (copy of table 0x6353c), clears +0x4b and runs 0x000EE2BE;
phase 1 computes the counter via 0x000ee4b4 (+0x304,+0x4b,+0x4c, 0x000EE46E result), phase 2
uses 0x000EE46E directly, phase 3 the +0x31c-object vmethod (+0x20 slot) with byte
(0x000ee498-obj +0x164 pointer)*10, else 0x514 -> +0x50. Cluster of 1. [O]

#### 0x000EE46E `shift_phase_m2_base_val` — logic
Chained singleton queries: vmethod (0x000ee4a8 obj) -> curve lookup (0x000ee4bc obj) ->
evaluation over the 0x000ee498-object slot +0x15c; returns an s16 base value for machine 2.
Caller 0x000EE38C. Cluster of 1. [I]

#### 0x000EE4C8 `shift_phase_m3_done_aux` — logic
Auxiliary done-condition shared by machine 3: byte comparison (*PTR_DAT_000ee704 vs query
0x000ee708) or lever byte (0x000ee70c obj +0x16c ptr) == 2, combined with s16 ctx+0x62 vs the
query on sub-object +0x58 (0x000ee710). Caller 0x000EE734. Cluster of 1. [O]

#### 0x000EE516 `shift_phase_m3_step` — logic
Step driver of phase machine 3 (phase byte ctx+0x3e, 215 instrs; caller 0x000EBDBC): bVar1 =
engaged && state==2; parks at phase 7/counter 0x514 when not bVar1 and phase 7; phases 0/1
transition to 6/7 (and reverse 6->0, 7->1) through 0x000EE800 with the lever byte gate
(ctx+0x37 vs *PTR_DAT_000ee714 and the 0x000ee704/0x000ee708 pair); while 0x000EE734 says
done, increments the phase (cap 7, event 0x36b/0xf code 0xd0 on cap) and re-runs 0x000EE800.
Counter +0x2a by phase: 1 -> 0x514; 2 -> query(+0x58,+0x14) + global 0x000ee724 + ctx+0x3a
(rising-edge latched via +0x36); 3/4 -> queries 0x000ee728/0x000ee72c with (+0x3d,+0x3c,+0x3a);
0/5/6 -> the +0x478-object vmethod (+0x20 slot) with 0x000EE730 result on (+0x3f,+0x38,+0x3d);
7 -> 0x514. Cluster of 1. [I]

#### 0x000EE734 `shift_phase_m3_done_pred` — logic
Machine-3 phase-complete predicate (96 instrs): phase 0 — 0x000EE4C8 or s16 global
0x000ee97c < counter +0x2a; 1 — 0x000EE4C8; 2 — 0x000EE8B0 or s16 ctx+0x62 >= query on +0x58
(0x000ee974); 3 — +0x3c<=+0x3d or 0x000EE8B0 or ctx+0x36; 4 — 0x000EE8B0; 5 — not engaged, or
ctx+4 with ctx+0x38 != *PTR_DAT_000ee978; 6 — s16 gate; 7 — false. Cluster of 1. [O]

#### 0x000EE800 `shift_phase_m3_actions` — logic
Machine-3 phase entry actions: phases 0/1 latch *PTR_DAT_000ee978 into +0x37; phase 2 loads
u16 global 0x000ee980 into +0x3a, sets +0x36=1, bytes from PTR_DAT_000ee984 (+0x21 -> +0x38,
+0x27 -> +0x3f) and a query on +0x58 into +0x3c; phase 3 copies counter +0x2a to +0x3a and
clears +0x3d; phase 5 clears +0x3d and calls the +0x478-object vmethod (+0x14 slot) with
(+0x2a, -0xc8, 0x514); phase 7 clears +0x36/+0x37/+0x38/+0x3f. Caller 0x000EE516. Cluster of 1. [O]

#### 0x000EE8B0 `shift_phase_m3_abort_pred` — logic
Abort-style predicate used by phases 2/3/4: when engaged, requires s16 ctx+0x14 < query(+0x58)
AND s16 ctx+0x62 < query(+0x58) (0x000ee98c/0x000ee990), and then still returns 0 when ctx+4==0
or ctx+0x38 == *PTR_DAT_000ee978; returns 1 otherwise (including when not engaged). Caller
0x000EE734. Cluster of 1. [I]

#### 0x000EE902 `shift_counter_min_select` — logic
Selects the minimum of the three phase-machine counters (+0x46, +0x50, +0x2a) into ctx+0x20,
tagging the source in ctx+0x22 (0xc machine 1, 0xd machine 2, 0xe machine 3); 0x514 is the
idle/park value. Caller 0x000EBDBC. [I] overall-shift progress = slowest machine. Cluster of 1.

#### 0x000EE94A `shift_ptr_s16_calc` — logic
Two chained singleton queries (0x000ee994/0x000ee998) producing an s16, second one over ctx
+0x2e8 with a value from the 0x000ee99c-object slot +0x24. Caller 0x000EBDBC. Cluster of 1. [O]

#### 0x000EE9A4 `shift_gearpair_tbl_get` — logic
2D byte-table read (helper 0x000B6CBC) over the 0x000eea18-object slot +0x174 indexed by
(DAT_fff8bd6e+1, DAT_fff8bd6d+1) — direct evidence the two globals are a to/from code pair
(gear-like, 0-based); result is written to ctx[0] only when inside byte limits (+0x178/+0x17c
pointers) and s16 limits (+0x180/+0x184) checked against the 0x000eea20 object (+0x96 byte,
+100 s16), else left unchanged when non-zero. Caller 0x000EBDBC. Cluster of 1. [O]

#### 0x000EEA24 `shift_flag_bc0_eq1_get` — data-accessor
Returns *DAT_000eebc0 == 1. Callers 0x000E5E48, 0x00116792. Cluster of 1. [O]

#### 0x000EEA2E `shift_obj_bc4_s16_get` — stub
Vmethod thunk on the 0x000eebc4 object (slot 0x000eebc8 -> 0x0010CDB0), returns s16. Called
by 0x000DE13A (unit 29 `lockup_speed_u16_get`) — a speed query into the shift-model band.
Cluster of 1. [O]

#### 0x000EEA3C `shift_flag_bcc_get` — stub
Returns byte *DAT_000eebcc. Caller 0x000DE158 (`lockup_state_u8_get`). Cluster of 1. [O]

#### 0x000EEA44 `shift_obj_bc4_34_cond_s8` — logic
Returns the signed byte at 0x000eebc4-object +0x34 when its byte +8 == 1, else 0. Caller
0x000DE174 (`lockup_flag_u8_get`). Cluster of 1. [O]

#### 0x000EEA54 `shift_dat_bd0_s16_get` — stub
Returns (int)*DAT_000eebd0. Callers 0x000EB556/0x000EB57E (preceding unit). Cluster of 1. [O]

#### 0x000EEA5A `shift_obj_bc4_u8_get` — stub
Vmethod thunk (shared slot literal 0x000eebd4 -> 0x0010CDDC) on the 0x000eebc4 object.
Callers 0x000EB556/0x000EB21C/0x000EAE8C/0x000EAEC4/0x000EB57E. Cluster of 1. [O]

#### 0x000EEA68 `shift_dat_bd8_u8_get` — stub
Returns *DAT_000eebd8. Callers 0x00116C1E/0x000EB21C/0x00116948/0x000EAE68. Cluster of 1. [O]

#### 0x000EEA70 `shift_ptr_bdc_u8_get` — stub
Returns *PTR_DAT_000eebdc. Caller 0x00116C1E. Cluster of 1. [O]

#### 0x000EEA78 `shift_dat_be0_u8_get` — stub
Returns *DAT_000eebe0. Caller 0x000EB020 (state-2 branch of 0x000EB8A8). Cluster of 1. [O]

#### 0x000EEA80 `shift_ptr_be4_u8_get` — stub
Returns *PTR_DAT_000eebe4. Caller 0x001175D6. Cluster of 1. [O]

#### 0x000EEA88 `shift_dat_be8_s8_get` — stub
Returns (int)*DAT_000eebe8 (signed). Callers 0x000EAE18, 0x000EDA48. Cluster of 1. [O]

#### 0x000EEA8E `shift_dat_bec_u8_get` — stub
Returns *DAT_000eebec. Caller 0x000EB86E. Cluster of 1. [O]

#### 0x000EEA96 `shift_obj_bf0_u8_get` — stub
No-arg vmethod thunk on the 0x000eebf0 object (the shift-model interface; slot 0x000eebf4 ->
0x001068AC). Cluster of 4: members 0x000EEAB4 (slot -> 0x00106988), 0x000EEAC2 (slot ->
0x001069BC) and 0x000EEB40 (slot -> 0x00106A3E) are the same pattern over different vtable
slots — sibling byte queries of the same object, named `shift_obj_bf0_u8_get__2/__3/__4`.
Callers 0x000BA7CE, 0x000EA0EE, 0x000E7C8E, 0x000DD862. [O]

#### 0x000EEAA4 `shift_obj_bf0_u8_get_arg` — stub
Vmethod thunk with one byte argument on the 0x000eebf0 object (slot 0x000eebf8 ->
0x00106918). Caller 0x000EDA48. Cluster of 1. [O]

#### 0x000EEAD0 `shift_dat_c04_u8_get` — stub
Returns *DAT_000eec04. Caller 0x000EBB6C. Cluster of 1. [O]

#### 0x000EEAD8 `shift_dat_c08_u8_get` — stub
Returns *DAT_000eec08. Caller 0x000EB80E (range-4 virtual query path). Cluster of 1. [O]

#### 0x000EEAE0 `shift_obj_c0c_u8_get` — stub
Vmethod thunk on the 0x000eec0c object. Cluster of 2: member 0x000EEAEE is the same pattern
through the shared slot literal 0x000eebd4 (-> 0x0010CDDC) on the same object, named
`shift_obj_c0c_u8_get__2`. Callers 0x000DDF76, 0x000EB21C. [O]

#### 0x000EEAFC `shift_ptr_c14_u8_get` — stub
Returns *PTR_DAT_000eec14. Caller 0x000EB21C. Cluster of 1. [O]

#### 0x000EEB04 `shift_dat_c18_u8_get` — stub
Returns *DAT_000eec18. Caller 0x000EB9EC. Cluster of 1. [O]

#### 0x000EEB0C `shift_obj_c1c_u8_get` — stub
Vmethod thunk (shared slot 0x000eebd4 -> 0x0010CDDC) on the 0x000eec1c object. Caller
0x000EB21C. Cluster of 1. [O]

#### 0x000EEB1A `shift_ptr_c20_u8_get` — stub
Returns *PTR_DAT_000eec20. Caller 0x000EB21C. Cluster of 1. [O]

#### 0x000EEB22 `shift_obj_c1c_s16_get_arg` — stub
Vmethod thunk with one byte argument on the 0x000eec1c object (slot 0x000eec24 ->
0x0010CFB6), returns s16. Caller 0x000BA7DC. Cluster of 1. [O]

#### 0x000EEB32 `shift_obj_bf0_s16_get` — stub
No-arg vmethod thunk on the 0x000eebf0 object (slot 0x000eec28 -> 0x001069DA), returns s16.
Callers 0x000EAE8C, 0x000EAEC4, 0x000DD7B8. Cluster of 1. [O]

#### 0x000EEB4E `shift_ptr_bf0_obj_get` — stub
Returns the object pointer *PTR_DAT_000eebf0 itself. Caller 0x000BA7EC. Cluster of 1. [O]

#### 0x000EEB56 `shift_obj_bf0_call_lab` — stub
Vmethod call via PTR_LAB_000eec30 on the 0x000eebf0 object, no args, no return use. Caller
0x000EB348. Cluster of 1. [O]

#### 0x000EEB5E `shift_obj_bf0_set5` — stub
Five-argument vmethod forward on the 0x000eebf0 object (slot 0x000eec34 -> 0x001066D6, the
shift-model band of spec 11). Caller 0x000EB900 (engage). Cluster of 1. [O]

#### 0x000EEB80 `shift_obj_bf0_set3` — stub
Three-argument vmethod forward on the 0x000eebf0 object (slot -> 0x001067EC). Callers
0x000DDED6/0x000DDC38 (end/cancel facades). Cluster of 1. [O]

#### 0x000EEB8C `shift_obj_bf0_call0` — stub
No-arg vmethod call on the 0x000eebf0 object (slot 0x000eec3c -> 0x00106834). Cluster of 3:
members 0x000EEB94 (slot -> 0x00106848, caller 0x000EC0E0 init) and 0x000EEBB8 (slot ->
0x001067D6, callee of 0x000EBDB4) are the same pattern over sibling slots, named
`shift_obj_bf0_call0__2/__3`. Callers 0x000EB348, 0x000EBDB4. [O]

#### 0x000EEB9C `shift_obj_bf0_set2` — stub
Two-argument vmethod forward on the 0x000eebf0 object (slot -> 0x00106866). Caller
0x000EDC5A. Cluster of 1. [O]

#### 0x000EEBA6 `shift_obj_bf0_set2b` — stub
Two-argument vmethod forward (byte, char) on the 0x000eebf0 object (slot -> 0x00106754).
Caller 0x000EBD84. Cluster of 1. [O]

#### 0x000EEBB0 `shift_obj_bf0_set1` — stub
One-argument vmethod forward on the 0x000eebf0 object (slot -> 0x001067A4). Caller
0x000EBD9E. Cluster of 1. [O]

#### 0x000EEC54 `shift_elt_task_step` — logic
Periodic step of a second context object (mode byte +2 in 1/2/3; caller facade slot
0x000BAC06): runs 0x000EFB48 first; mode 3 -> 0x000EF138 + 0x000EF576; mode 2 with a lever
pair mismatch on the 0x000eee90 object (+0x21 vs +0x27, +0x2b vs +0x2c) captures +0x51/+0x50
from +0x28/+0x22 when +0x7e/+0x7f are clear, then 0x000EF448 + 0x000EF4B2; otherwise mode is
cleared. Finally mode forced 0 when both +0x7e and +0x7f set and mode!=3, then 0x000EF98A,
0x000EF63E, 0x000EF6E4. [I] element-phase manager (5-element structure, module id 0x36d).
Cluster of 1.

#### 0x000EECEC `shift_elt_engage_pair` — logic
Pair-engagement entry (caller 0x000DDDE8): with both codes non-zero sets mode 2, stores them
at +1 (param_2) and +0 (param_3); selects sub-object pointers from the 5-entry table
PTR_PTR_000eee94 by each code (bounds 1..5, else event via 0x000EFBB6 and index 1) into
+0x94/+0x98; clears the +0x10..+0x3b working block; seeds +0x84 with a global s16 via
(0x000eee98/0x000eee9c) and clears +0x7f; when the previous mode byte +0x83 was 2 also
re-inits the second block via 0x000eeea4, else runs 0x000EF906. Codes zero -> 0x000EF0BC
clear. Cluster of 1. [I]

#### 0x000EEDFC `shift_elt_engage_single` — logic
Single-code engagement (caller 0x000DDCA2): mode 1 with param_2 -> +1; clears the four u32
blocks, seeds +0x84 as above, selects +0x94 from the 5-entry table (bounds 1..5), clears
+0x7f, runs 0x000EF906; finally +0=0 and +0x50/+0x51 from the 0x000eee90 object (+0x22/+0x28).
Zero code -> 0x000EF0BC. Cluster of 1. [I]

#### 0x000EEEA8 `shift_elt_set_code_or_end` — logic
Facade impl (slot 0x000BAAB2): param_2==1 -> +1=param_3, mode 3, clear blocks, seed +0x84
via (0x000ef0f4/0x000ef0f8/0x000ef0fc), select +0x94 from the 5-entry table at
PTR_PTR_000ef100, capture +0x51/+0x50 from the 0x000ef104 object (+0x28/+0x22), clear +0x7f;
otherwise when mode is 3 or 0 runs 0x000EF91A + 0x000EFB0C(0,ctx,0xff) and clears +1/+2.
Cluster of 1. [I]

#### 0x000EEF42 `shift_elt_disengage` — logic
End path (callers 0x000DDE78/0x000DDC38): 0x000EFB0C(0,ctx,0xff); when +0x7e==0 runs
0x000EF906; clears +0; when mode!=3 runs 0x000EF91A if +0x7f==0 and clears +1. Cluster of 1. [I]

#### 0x000EEF78 `shift_elt_limit_get` — logic
Selects the s16 limit for index 1..5 from the five objects PTR_DAT_000ef10c/110/114/118/11c;
any other index reports event (0x36d,0xf,3,6,0x12) and falls back to *PTR_DAT_000ef108 - 0x28.
Callers 0x000EA89C, 0x000BABF6, 0x000EE46E. [O] five-element limit table (spec 11 element
matrix). Cluster of 1.

#### 0x000EEFEA `shift_elt_calc_chain` — logic
When the lever pair on the 0x000ef104 object matches (+0x21==+0x27): seeds ctx+0x7c with
(*PTR_DAT_000ef108 - 0x28) and for each index 1..5, if the vmethod (0x000ef12c, idx) is set,
replaces +0x7c with the curve evaluation (0x000ef130) over (limit_s16, +0x7c); returns +0x7c.
Caller facade slot 0x000BAA72; callee 0x00104D4C (shift-map band, spec 09). Cluster of 1. [I]

#### 0x000EF0A0 `shift_elt_limits_reset` — logic
Calls the vmethod (PTR_FUN_000ef134) on all five limit objects (0x000ef10c..0x000ef11c).
Caller facade slot 0x000BAA80; callee 0x000EFD1A. Cluster of 1. [O]

#### 0x000EF0BC `shift_elt_ctx_clear` — logic
Clears mode +2, the five u32 working words at +0x28..+0x4c, and the latch bytes +0x7e/+0x7f.
Callers 0x000EEDFC, 0x000EECEC. Cluster of 1. [O]

#### 0x000EF138 `shift_elt_torque_calc` — logic
Per-element value computation (159 instrs; callers 0x000EF448, 0x000EEC54): base s16 from a
curve (0x000ef410 obj, ctx+1) plus a map read (0x000ef418-obj slot +0x138, byte
*PTR_DAT_000ef41c) chained through ctx+0x84 and 0x000EFBE6(ctx+0x94 object)*10, clamped >=0
into +4. Mode 1/2 evaluate four maps (0x000ef418-obj slots +0x104/+0x10c/+0x118/+0x11c) with
(+0x50,+0x51); mode 3 takes float +0x10 = query(0x000ef434)/DAT_000ef43c; any other mode
reports event (0x36d,0xf,1,2,0x7c) and zeroes. Modes 1/2: +8 via 0x000EF51C, +0xc via
0x000EF54A, float +0x10 = (+8 * +0xc * DAT_000ef440)/DAT_000ef43c. Peak-holds +0x18, then the
integrator +0x38 += DAT_000ef444 * value, mirrored to output +0x28. Interp helpers
0x000B6CE8/0x000B6D6C/0x000B79E4. [I] per-element torque/energy estimate. Cluster of 1.

#### 0x000EF2BA `shift_elt_oncoming_calc` — logic
Twin of 0x000EF138 for the on-coming code (ctx+0, second field bank: results +6/+0xa/+0xe,
float +0x14, peak +0x1c, integrator +0x3c, output +0x2c): base curve/map read through seed
+0x8c and 0x000EFBF4(ctx+0x98 object), four maps at slots +0x100/+0x108/+0x110/+0x114, slot
+0x13c for the base map, event (0x36d,0xf,1,3,0x73) for bad modes, same peak-hold/integrator
structure. Caller 0x000EF4B2. Cluster of 1. [I]

#### 0x000EF448 `shift_elt_offgoing_step` — logic
Per-step off-going update (ctx+1, latch +0x7f): when the vmethod on (0x000ef690, code) is 0,
recomputes only the base +4 (curve 0x000ef698 + map slot +0x138 via +0x84); otherwise runs
the full 0x000EF138 and restarts the element via 0x000EF576 with the code. Caller
0x000EEC54. Cluster of 1. [I]

#### 0x000EF4B2 `shift_elt_oncoming_step` — logic
Per-step on-coming update: only when global *PTR_DAT_000ef6ac == 2 and latch +0x7e clear; if
*PTR_DAT_000ef6b0 == 0 recomputes base +6 (curve + map slot +0x13c via seed +0x8c), else full
0x000EF2BA plus 0x000EF576 restart with code *param_1. Caller 0x000EEC54. Cluster of 1. [I]

#### 0x000EF51C `shift_elt_energy_calc` — logic
Physics-shaped helper: (param_4*((param_2^2)/200) + param_3*param_2*5)/500000 passed through
vmethod 0x000ef6b4 -> s16. [I] kinetic-energy term (speed squared times an inertia-like
factor) for the element calculation. Callers 0x000EF138/0x000EF2BA; callee 0x000B7888.
Cluster of 1.

#### 0x000EF54A `shift_elt_speed_calc` — logic
Computes (param_2*u16(+0x7e) + param_3*u16(+0x7c))/1000 over the speed object
PTR_DAT_000ef6b8 through vmethod 0x000ef6b4, then a second transform (0x000ef6bc) -> s16.
Callers 0x000EF138/0x000EF2BA. [I] speed blend for the element calculation. Cluster of 1.

#### 0x000EF576 `shift_elt_by_idx_restart` — logic
Restarts one of the five element objects: index 1-5 selects PTR_DAT_000ef6c0/c4/c8/cc/d0,
each restarted via vmethod 0x000ef6d4 with argument 1; other indices report event
(0x36d,0xf,3,4,0x26). Called from 0x000EF448/0x000EF4B2/0x000EF92E/0x000EFB0C/0x000EF95C/
0x000EEC54. [O] five-element restart (spec 11 element matrix). Cluster of 1.

#### 0x000EF5E2 `shift_elt_id_byte_match` — logic
Compares the chained result s16 ctx+0x80 against each of the five element objects' first s16
and copies the matching object's byte +2 into ctx+0x82. Caller 0x000EF63E. Cluster of 1. [O]

#### 0x000EF63E `shift_elt_chain_min` — logic
Folds vmethod 0x000ef6e0 over the five element objects starting from -0x28 into s16
ctx+0x80 (min-style chain), then 0x000EF5E2 maps the winner to a byte. Caller 0x000EEC54;
callee 0x00104D4C. [I] minimum over the five elements. Cluster of 1.

#### 0x000EF6E4 `shift_elt_results_pack` — logic
Scales the float result bank ctx+0x10..+0x4c into integer outputs: four values via float
conversion (0x000ef9d0, factor DAT_000ef9cc) into u32 +0x54/+0x58/+0x5c/+0x60, four via
(0x000ef9d8, DAT_000ef9d4) into u16 +0x64/+0x66/+0x70/+0x72, two u32 (+0x68/+0x6c) and four
u16 (+0x74/+0x76/+0x78/+0x7a) with the remaining converters. Caller 0x000EEC54. [O]
Cluster of 1.

#### 0x000EF7AA `shift_elt_oncoming_derive` — logic
When the on-coming code (ctx+0) is set: derives rates from the bank-B peak/integrator — +0x24
= peak(+0x1c)/curve(slot +0x120, code-1)*k, +0x34 = integrator(+0x2c)/curve(+0x124)*k,
+0x44 = +0x24/map(+0x128,(+0x50,+0x51))*DAT_000ef9ec, +0x4c = +0x34/map(+0x130)*k. Uses the
0x000ef9e0 object and interp helpers 0x000B6C94/0x000B6CE8/0x000B6D40. Caller 0x000EF906.
Cluster of 1. [I]

#### 0x000EF858 `shift_elt_offgoing_derive` — logic
Mirror of 0x000EF7AA for the off-going code (ctx+1) and bank A: +0x20 from peak(+0x18),
+0x30 from integrator(+0x28), +0x40 and +0x48 via maps +0x12c/+0x134. Caller 0x000EF91A.
Cluster of 1. [I]

#### 0x000EF906 `shift_elt_oncoming_finish` — stub
9 instructions: sets latch ctx+0x7e=1, then 0x000EF7AA (derive) and 0x000EF92E (clear bank B
and restart the element). Cluster of 2: member 0x000EF91A is the off-going counterpart
(latch +0x7f=1, 0x000EF858 + 0x000EF95C), named `shift_elt_oncoming_finish__2`. Callers
0x000EFB48, 0x000EEDFC, 0x000EEF42, 0x000EECEC. [O]

#### 0x000EF92E `shift_elt_bankb_clear` — logic
Zeroes the bank-B working fields (+6/+0xa/+0xe u16s, +0x14/+0x1c/+0x2c/+0x3c u32s) and, when
the on-coming code is non-zero, restarts that element via 0x000EF576. Caller 0x000EF906.
Cluster of 1. [O]

#### 0x000EF95C `shift_elt_banka_clear` — logic
Zeroes the bank-A working fields (+4/+8/+0xc u16s, +0x10/+0x18/+0x28/+0x38 u32s) and, when
the off-going code is non-zero, restarts that element via 0x000EF576. Caller 0x000EF91A.
Cluster of 1. [O]

#### 0x000EF98A `shift_elt_matrix_apply` — logic
Element-matrix applier (163 instrs; caller 0x000EEC54): for each of the two codes — off-going
ctx+1 (when latch +0x7f clear; code 1 uses vmethod 0x000ef9f8 on object 0x000ef9f4, codes 2-5
use vmethod 0x000efc08 on 0x000efc04/0c/10/14, bad code -> event (0x36d,0xf,3,4,0xee)) and
on-coming ctx+0 (latch +0x7e; objects 0x000efc20 (code 1) and 0x000efc04..c14; bad code ->
event (0x36d,0xf,3,5,0x14)) — marks the element active; afterwards every element not marked
is stopped (vmethod 0x000efc08 on its object). [I] engage exactly the elements of the
transition (spec 11 element matrix). Cluster of 1.

#### 0x000EFB0C `shift_elt_restart_all_or_one` — logic
With param_3 == -1 restarts all five elements via 0x000EF576 (1..5), else restarts only
element param_3. Callers 0x000EEF42, 0x000EEEA8, 0x000EECEC. Cluster of 1. [O]

#### 0x000EFB48 `shift_elt_finish_scan` — logic
Completion scan run at the top of the step: when the on-coming code is set, its status query
(0x000efc24/0x000efc28) returning 0 with latch +0x7e clear finishes it via 0x000EF906; when
the off-going code is set and both its query (0x000efc2c/0x000efc30) and *PTR_DAT_000efc34
are set with +0x7f clear, finishes it via 0x000EF91A. Callers 0x000EEC54 and 0x000EB020.
Cluster of 1. [O]

#### 0x000EFB98 `shift_elt_prev_mode_latch` — logic
Latches ctx+0x83 from global *PTR_DAT_000efc38, forcing 3 when the global is 0 or 4. Callers
0x000EEDFC, 0x000EECEC. Cluster of 1. [O]

#### 0x000EFBB6 `shift_elt_err_bad_code` — stub
Single error report (module 0x36d, line 0xf, class 1/9, code 0x6e) — the invalid-code path of
the 5-entry table lookups. Callers 0x000EEDFC, 0x000EEEA8, 0x000EECEC. Cluster of 1. [O]

#### 0x000EFBE6 `shift_elt_obj18_byte_get` — data-accessor
Returns the byte through the pointer at object+0x18 when it differs from the object address
itself (self-pointer check), else 0. Caller 0x000EF138. Cluster of 2: member 0x000EFBF4 is
the same check at object+0x1c (caller 0x000EF2BA), named
`shift_elt_obj18_byte_get__2`. [O]

#### 0x000EFC3C `shift_reg_reset` — logic
Resets a small regulator object: float outputs at +0x0c/+0x10 to (byte)*PTR_DAT_000efedc +
DAT_000efee0, clears +0x14, converts to u16 at +0 via 0x000efee4. Caller 0x000DDFBC (facade
band). [I] integrator/regulator reset. Cluster of 1.

#### 0x000EFC6C `shift_elt_restart_math` — logic
Called by 0x000EF576 with the element context: computes float ctx+0x10 = (param_1 * f4 *
DAT_000eff04) / (f5 * (u16 via 0x000efee8-obj +0x140 pointer / DAT_000eff04) * f6) where f5,
f6, f4 come from curves at slots +0x144/+0x148/+0x14c indexed (element code -1) and (lever -
1); param_3==0 forces f4=1. Interp helpers 0x000B6C8A/0x000B6CBC/0x000B6C9E. [I] element
restart scaling (torque/speed ratio). Cluster of 1.

#### 0x000EFD1A `shift_elt_grade_report` — logic
For an element object (s16 id at +0, code byte +2): when a global query is set, scans up to 7
entries of the table via the +0xc pointer chain (slot +0x28) for the first value >= the id,
then reports (code-1)*8 + index + 1 through the 0x000eff08-object vmethod. Caller
0x000EF0A0. [I] element grading/notification. Cluster of 1.

#### 0x000EFD76 `shift_small_obj_ctor` — logic
Allocator-based constructor (0x1c bytes via 0x0009AA34): stores param_2 at +0xc, param_3 at
+2, zeroes the remaining fields. Caller 0x0015C13C (static-init band). Cluster of 1. [O]

#### 0x000EFDB4 `shift_reg_err_calc` — logic
Computes the float at ctx+4 = ((float(+0xc) - (byte)*PTR_DAT_000efedc) + DAT_000eff2c) *
(ratio), where the ratio u16 comes from either 0x000EFEA8 (when query1 < query2 on the
0x000eff08 object with ctx+2) or 0x000EFEC2 (through the +0x18 pointer chain with global
0x000eff28), scaled by 1/DAT_000eff04. Caller 0x000EFD0C (other unit). [I] regulator error.
Cluster of 1.

#### 0x000EFE24 `shift_reg_step` — logic
Regulator step: float +0x18 += DAT_000eff30 * (error(+0x08) - setpoint(+0x04)); clamps via
0x000EFE7C; converts through vmethod 0x000eff34 with the ratio from the 0x000efee8-object
+0x150 pointer; stores the u16 output at +0. Caller 0x000EFD0C. [I] integrator step.
Cluster of 1.

#### 0x000EFE7C `shift_reg_out_clamp` — logic
Clamps a float between DAT_000eff38 (upper) and byte(0x000eff3c obj +0x96)+DAT_000efee0
(lower). Caller 0x000EFE24. Cluster of 1. [O]

#### 0x000EFEA8 `shift_curve_u16_via_2c` — data-accessor
Returns the u16 curve value through the pointer at object+0x2c (0 when the pointer is the
object itself); callee 0x000B6EAE (curve interp). Cluster of 2: member 0x000EFEC2 is the same
pattern at object+0x30, named `shift_curve_u16_via_2c__2`. Caller 0x000EFDB4. [O]

#### 0x000EFF44 `shift_obj_init_ff94` — logic
Initializes a 0x94-byte object with sentinel defaults (bytes 0xff at +0..+3, u16 0xffff at
+4, 0xff at +0xc/+0x3a-family 0x7fff at +0x3a/+0x3c/+0x3e/+0x40/+0x42) and clears +0x58/
+0x60/+0x88/+0x89/+0x8a; allocates via PTR_FUN_000f0208 (0x94) when param is null. No
resolved callers (interface-invoked). Cluster of 1. [O]

#### 0x000EFF9C `shift_ctx_full_init` — logic
300-instruction full context init (callers 0x000F1020, 0x000BB194, 0x000F02EA, 0x000F0728):
resolves two pointer/index chains (u32 tables at PTR_LAB_000f0210/0x000f0218 bounds 8, and
0x000f0224/0x000f022c bounds 0x18; out-of-range reports via 0x000f0220 and keeps slot [1]);
resets the whole context (0xff/0xffff sentinels, 0x7fff s16s, zeroed flags), copies defaults
from the 0x000f0238 object (+0x9c -> +0x55/+0x56, +0xcc -> +0x78 variant) and the 0x000f020c
object (+0x27 -> +0x61, +0x21 -> +0x62/+0x63); seeds u16s +0x46/+0x48 via curves
(0x000f0248/0x000f024c) with the +0x78 variant selecting table halves, mirrored to
+0x82/+0x84; clears the six-entry RAM arrays DAT_fff8e3f2/fff8e3f8/0x000f049c and the
0x19-byte array at DAT_fff8e6ce; finally runs the observer chain when a global lever byte is
1. Cluster of 1. [O]

#### 0x000F02EA `shift_sched_task` — logic
Scheduler task (caller facade 0x000BB18C): when the lever global (0x000f04b0-obj +0x48 ptr)
is 1 and *PTR_DAT_000f04b4 is 0 — run 0x000F0728 (pre-step), then the state machine loop:
0x000F1020(ctx, state +0x4c), 0x000F0D5C done-check, and the vmethod 0x000f04b8 next-state
lookup through the table PTR_LAB_000f04bc; afterwards three wrap-up vmethods
(0x000f04c0/c4/c8), a state-name notification via table 0x00063542 copy when +0x79==0, and
0x000f04d4. Otherwise re-runs 0x000EFF9C (full init). Always notifies 0x000f04dc object
with 0x21. Cluster of 1. [O]

#### 0x000F0386 `shift_obj_e4_call` — stub
5-instruction vmethod call on the 0x000f04e0 object. Caller facade 0x000BB184. Cluster of 1. [O]

#### 0x000F038E `shift_elt_combo_scan` — logic
Scans the six element request flags (RAM DAT_fff8e3f2[1..5] vs 0x000f04a0[1..5]): first
fires the observer pair (0x000f04a4/0x000f04ac/0x000f04a8 and 0x000f04ec) for fresh requests,
then for every 1/2/3-element combination of pending elements computes the activity code via
0x000F068E and notifies the 0x000f04e8/0x000f0770 observers. Caller facade 0x000BB17C.
Cluster of 1. [I] element-combination arbitration.

#### 0x000F055E `shift_sched_lever_case` — logic
Lever-dependent handling (caller 0x000DBD28): when the 0x000f0780-object byte +0x2c is 1 or
2 sets +0x74=1 and captures +0x55/+0x56 from the 0x000f0770 object +0x9c; otherwise resets
the code at +0 to 0xff, sets +0x6f=1, converts the lever byte through three vmethods
(0x000f0788/0x000f0790/0x000f0794 on the 0x000f0784 object) setting flags in the
0x000f078c array (once, guarded by +0x76), then queries with +0x63 via 0x000f0798, or via
0x000f079c when +0x79 && +0x5a==0. Cluster of 1. [I]

#### 0x000F0602 `shift_sched_state_in_set` — logic
Stores param_2 at ctx+0x63, clears +0x66 and the u16 at +0x14, then a vmethod call
(0x000f07a0). Caller 0x000DBD20. Cluster of 1. [O]

#### 0x000F0612 `shift_sched_new_code_latch` — logic
When the 0x000f0780-object byte +0x2c is 1 or 2: sets ctx+0x66=1 and latches +0x6e=1 when
param_2 differs from ctx+0x61. Caller 0x000DBD18. Cluster of 1. [O]

#### 0x000F063A `shift_meas_register_ctx2` — logic
Registers nine context fields (+0, +1, +2, +3, +4, +6, +8, +10, +0xc) to the registrar
singleton (vmethods 0x000f07a8/0x000f07ac on the 0x000f07a4 object) — the same telemetry
pattern as 0x000EC0E0. Caller facade 0x000BB19C; callees 0x000BAE46/0x000BAE5E. Cluster of 1. [I]

#### 0x000F068E `shift_elt_matrix_lookup` — logic
Builds the bitmask 1<<idx for up to three element indices (0 contributes no bit) and looks up
the code byte in PTR_PTR_000f07b0[(mask & 0xff) >> 1]; returns 0 when the global query
(0x000f0770 object) equals the byte via PTR_PTR_000f07b4. Callers 0x000D790A, 0x000F038E,
0x000F21BA. [O] element-combination matrix (spec 11). Cluster of 1.

#### 0x000F06D4 `shift_q_u16_chain` — logic
Two chained queries (0x000f07b8 obj, then the 0x000f07c0-object slot +0x6c) returning a u16.
Callers 0x000F21BA, 0x000F1F38. Cluster of 1. [O]

#### 0x000F06F8 `shift_elt_pend_count_dec` — logic
Decrements (floor 0) the pending-counter byte at DAT_fff8e3f2[idx]. Caller 0x000F24B2.
Cluster of 1. [O]

#### 0x000F0710 `shift_elt_pend_count_inc` — logic
Increments (ceiling 0xff) the pending-counter byte at DAT_fff8e3f2[idx]. Caller 0x000F19E6.
Cluster of 1. [O]

#### 0x000F0728 `shift_sched_pre_step` — logic
424-instruction per-cycle pre-processing of the scheduler context (caller 0x000F02EA):
computes speed ratios +0x44/+0x4a from the 0x000f0770 object u16s (+0x7e/+0x7c) scaled x1000
through 0x000f09a8, negated when the lever pair (+0x2b==+0x2c==2) indicates reverse
direction (cVar17 = -1); stall flag +0x73 (u16 +0x7c below the value via 0x000f09ac+0x38);
on stall-entry/exit clears +0xf/+0x10/+0x3 and for states (+0x5c) 2/4 recomputes +0x82/+0x84
from a curve (0x000f09b8) plus/minus +0x4a and +0x86 from a delta vs +0x8; state 1/3 with
stall sets +0x72; computes the torque-ish u16 +0x4e (direction-aware blend through
0x000f0bd4/0x000f0bd8); optional full reset via 0x000EFF9C when the 0x000f0bdc-object query
clears the code; min/max capture of the +0x9c byte into +0x55/+0x56 (delta +0x54); runs
0x000F0C18; sets +0x76 when states allow; edge-detects +0x71 (clearing u16 +0x8); clears
u16 +0x4 under lever/code mismatch conditions; finally converts three byte lookups
(0x000f0bf8/0x000f0bfc/0x000f0c00 with the lever byte) into the u16 outputs +0x8c/+0x8e/
+0x90 via (0x000f0bf4-obj, 0x000f0c04, 0x000f0c08), sets +0x77 for states 1/3 (speed
threshold via 0x000f0c0c/+0x20 and global 0x000f0c10), and finishes with vmethod 0x000f0c14,
+0x92=0 and +0x78 = (0x000f0770-obj +0xcc == 1). Cluster of 1. [I]

#### 0x000F0C18 `shift_sched_ready_chk` — logic
Readiness gate for the scheduler (callers 0x000F0728, 0x000F13AC): clears +0x4d, then walks a
cascade of gates — target codes vs the 0x000f0d80-object pointers (+0x60/+0x5c), lever byte
+0x2c of 0x000f0d84 in {1,2}, the 0x000f0d88-object query, latches +0x70/+0x72, state +0x4c,
the 0x000f0d90-object query and the 0x000f0d78-object query; per sub-state (+0x5c) extra
limits (s16 via 0x000f0da4 chain +0x10/+0x14, byte 0x000f0da8 +0x96; states 2/4 compare
+0x54 and +0xc against 0x000f0d9c/0x000f0d98). Sets +0x4d=1 when all pass; states 2/4 with
+0x72 clear also set +0x72=1 when not ready. Cluster of 1. [I]

#### 0x000F0D5C `shift_sched_state_done` — logic
257-instruction per-state completion predicate (caller 0x000F02EA) for states 0,2,4,5,6,7
(state 1 and any default answer 1): state 0 requires globals *PTR_DAT_000f0ff4==1 and lever
pairs equal (0x000f0ff8 +0x21/+0x27 and +0x2b/+0x2c); state 2 waits for +0x66 or +0x74; state
4 checks sub-state (+0x5c), lever pairs with latches +0x6e/+0x6f and the 0x000f0ffc queries;
state 5 adds byte-threshold checks (+0x2 via the 0x000f1008/0x000f1018 chains) and
0x000f1010; state 6 adds the u16 +0x8 >= +0x86 window; state 7 requires states 2/4 with equal
lever pair and stall (+0x73) or byte via 0x000f101c +0x4c, or state 5 with u16 +0x4 above
the +0x58 pointer. Cluster of 1. [I]

#### 0x000F1020 `shift_sched_state_step` — logic
365-instruction state step (caller 0x000F02EA): state >8 dispatches to vmethod 0x000f13f0;
the jump table switchD_000f1038 handles 0/1 (no-op), 2 (re-resolve the bounds-8 table chain,
recompute the +0x46/+0x48 pair via curves 0x000f12c0/0x000f12c4 with the +0x78 variant
selecting table halves +8/+0xc and +0x10/+0x14, clear latches +0x6e/+0x6f/+0x75/+0x77/+0x72
and sub-state +0x5c), default (vmethod 0x000f13e8), 6 (per sub-state 1/2/3/4: recompute
+0x46 with blend/delta vmethods 0x000f12c8/0x000f12d0/0x000f12d4/0x000f13d4/0x000f13d8 and
+0x48 via 0x000f13dc; sub-state 5 returns 5; others report event (0x4de,1,...,0x80) and set
+0x4d=0/+0x72=1) and 7/8 (vmethod 0x000f13ec). Callees 0x000F2666/0x000F295E/0x000EFF9C.
Cluster of 1. [I]

#### 0x000F13AC `shift_sched_state_entry` — logic
338-instruction state-entry actions (caller 0x000F02EA). State 2: clears counters
+0xd/+0xe/+0xf/+0x10, increments +0x92, captures the lever pair into +0x61/+0x62 (0x000f167c
obj +0x27/+0x21), re-resolves the bounds-8 chain, clears +0x71 and u16 +0x8; when +0x66 set:
query (0x000f1694, lever) -> +0x60 maps 1->sub-state 1, 2->3, otherwise event (0x4de,0,...
0xef), +0x4d=0, +0x72=1; also resolves the bounds-0x18 chain ([0x23][0x29]), runs the +0x76
flag conversions (0x000f16b4/0x000f16bc/0x000f16c0 into 0x000f16b8), 0x000F24B2(ctx,+0x63)
and +0xc=0xff; when +0x66 clear and +0x74 set: query 0x000f16c4 -> sub-state 5 or
0x000F2406. State 4: query -> clear +0x2, states 2/4 clear +0x3, +0x7c=u16 global
0x000f16d4, +0x75=1. State 5: mirrors +0x46/+0x48 to +0x82/+0x84 and recomputes +0x86 (delta
vs +0x8 via 0x000f16d8/0x000f16dc/0x000f16e0) unless the lever pair is equal; +0x8=0. State
8: states 1/3 with +0xe==0 and +0x4d run the gear observer (0x000f1918/0x000f1920/
0x000f1928) and clear +0x70; +0x6e with +0x92==0 returns 1. Cluster of 1. [I]

#### 0x000F1762 `shift_sched_ramp_update` — logic
Ramp/progress update (255 instrs; caller 0x000F02EA): computes condition +0x67 from the
+0x46/+0x44/+0x48 ordering (not stalled) or, when stalled, sub-states 2/4 with +0x75 and
byte +0x3 window (delta +0x52 = global u16 0x000f1930 - +0x7c via 0x000f1934, bounds via
0x000f1938 chain) or sub-state 0 torque check (+0x4e via 0x000f193c vs u16 through the
0x000f192c-obj +0x2c/+0x30 pointers). Then walks the saturating ramp counters +0xd (state
0), +0xe (1/3), +0xf/+0x10 (2/4, stall selects) up when +0x67 else down; on the up-pass
resets the per-element ramp flags DAT_fff8e3f8[+0x88/+0x89/+0x8a]; on the down-pass
increments them when the thresholds (s16 table 0x000f1ba4 indexed by the element) are passed
by +0x8c/+0x8e/+0x90. Cluster of 1. [I]

#### 0x000F19E6 `shift_sched_sync_scan` — logic
Synchronization scan (218 instrs; caller 0x000F02EA): when not ready clears +0x68/+0x69/+0x6a
and stops; +0x68 = sub-state 0 with counter +0xd at/above the 0x000f1ba8-obj +0x10 limit;
+0x6a = sub-states 2/4 with +0xf at the +0x14 limit, or stalled with +0x75, +0x3 above the
+0x50 limit and +0x10 below the +0x24 limit. When +0x68/+0x6a: increments the pending counts
(0x000F0710) for elements +0x88/+0x89/+0x8a, min-captures +0x8c/+0x8e/+0x90 into the s16
table 0x000f1ba4 and notifies the observer (0x000f1bb4/0x000f1bbc chain). Sub-states 1/3:
+0x69 when +0xe reaches the +0x18 limit (raised by +0x77 via 0x000f1bc4), then notifies,
clears u16 +0x6, and either reports event (0x4de,0,...,0xfb) with +0x4d=0/+0x72=1 when
+0x60==0, or increments the per-gear counter table DAT_fff8e6ce[(+0x62)*4 + +0x61]. Cluster
of 1. [I]

#### 0x000F1C16 `shift_sched_gear_engage` — logic
Next-gear engagement handover (313 instrs; caller 0x000F02EA) against the 0x000f1e7c gear
object (+0xa1 state, +7 code, +0x9c byte, +0xac byte) and 0x000f1e80 object: clears +0x6b
when the object state is 6/7/8 or u16 +0x6 passed (notify (obj,1), clear +0x58, call
0x000f1e94(0,0), +0x93=0); clears +0x6c when the code matches the lever and state 5/7
conditions with speed windows hold (notify (obj,0)); +0x57 special path; +0x6d path (states
>5 with +0x7e==1 or !=7 with +0x7e==2). param_2==0: on +0x68 either arms (+0x57,+0x6c,+0x93=1)
when the new-gear byte is -1, engages via 0x000f2120 with table 0x000f2118/lever +0x22, or
on object byte +6 not in {0,2,3} sets +0x70=1/+0x6c=1/+0x93=6, else reports event
(0x4de,3,0xb,0xe6). param_2!=0: on +0x69 stores +0x58=+0x62, calls 0x000f1e94(+0x62,2); on
object byte +6 not 0/1 sets +0x6b=1/+0x93=2/+0x1=0, else event (0x4de,3,0xb,0xac). [I]
Cluster of 1.

#### 0x000F1F38 `shift_sched_gear_commit` — logic
Gear commit (241 instrs; caller 0x000F02EA): sub-states 1/3 with +0x69: when +0x60==0
reports event (0x4de,0,...,0xc,0x1d) and clears +0x4d (set +0x72); otherwise reads the
per-gear counter DAT_fff8e6ce[+0x62*4 + +0x61] and requires it below the counter threshold
(byte via 0x000f2134 chain +0xc plus 0x000f2138, combined by 0x000f213c); on pass notifies
(0x000f2124 obj,1) and the gear observer (0x000f214c), latches +0x63=+0x62, +0x64=+0x61,
+0x79=1, +0x58=+0x62, clears +0x6b/+0x93, calls 0x000f2120(+0x62,0) and optionally rolls
back (+0x79/+0x63/+0x64=0) when the object byte +6 is 0/1/2. Other sub-states: on +0x68 or
+0x6a either tear down the previous target (+0x63/+0x64 observers via 0x000f2428) and run
0x000F21BA, or when +0x59 set notify the +0x7a target (0x000f2154) and the 0x000f2158
observer with +0x5a=1. Cluster of 1. [I]

#### 0x000F21BA `shift_sched_target_select` — logic
Selects the next target (callers 0x000F1F38, 0x000F055E): computes a base score (byte via the
0x000f2438-obj +0x1c pointer plus 0x000f243c, combined by 0x000f2440) and scans the six
element pending counters DAT_fff8e3f2 for the highest scores, collecting up to three element
indices; the combination code comes from 0x000F068E. When the new code differs from +0x7a:
notifies the old target off (0x000f2448/0x000f2434 chain) and the new target on, sets
+0x79=1, +0x93=0, reports through 0x000f2458 with table 0x000f2450/*PTR_DAT_000f244c,
clears +0x57/+0x6c/+0x6d, and rolls back +0x79 when the 0x000f2424-object byte +6 is 0-3;
single-element results set +0x59=1. Cluster of 1. [I]

#### 0x000F22E4 `shift_sched_status_snapshot` — logic
Copies the module's RAM status arrays into the context for external readout: pending counters
DAT_fff8e3f3..e3f7 -> +0x2f..+0x33, the five bytes of *PTR_DAT_000f2460 -> +0x34..+0x38, the
five u16s of *PTR_DAT_000f245c -> +0x3a..+0x42 (u16 aligned), ramp flags
DAT_fff8e3f9..e3fd -> +0x2a..+0x2e and the 25 per-gear counters DAT_fff8e6ce..e6e6 ->
+0x11..+0x29. Caller 0x000F02EA (wrap-up phase). [O] telemetry snapshot. Cluster of 1.

#### 0x000F2406 `shift_sched_dir_pick` — logic
Direction/sub-state pick (callers 0x000F13AC, 0x000F0728): computes the direction factor from
the 0x000f2424-object bytes +0x127/+0x129/+0x124, copies +0x44 to +0x4a when the u16 +0x7c
is non-zero, evaluates 0x000f26e4(direction * u16(+0x7e)) into +0x80, loads +0x50 from
global 0x000f26e8, and sets the sub-state (+0x5c) to 2 when 0x000f26f0(lever byte of
0x000f26ec) <= 0x000f26f4(+0x4a) else 4. [I] up/down (sync direction) selection. Cluster of 1.

#### 0x000F24B2 `shift_sched_ramp_finish` — logic
Per-element ramp completion (201 instrs; callers 0x000F055E, 0x000F13AC) for the code
param_2: queries the three element indices of the gear (0x000f26fc first element,
0x000f2700/0x000f2704 the other two) from the 0x000f26f8 object; for each resolves the
bounds-4 table chain (0x000f270c/0x000f2714, errors via 0x000f271c) and, when the ramp
counter DAT_fff8e3f8[idx] reached the threshold byte, calls 0x000f2720(ctx,idx) and clears
the counter; resets the s16 DAT_000f2724[idx] to 0x7fff for elements without pending
requests. [I] end-of-ramp per element. Cluster of 1.

#### 0x000F2666 `shift_sched_tgt_state1234` — logic
Curve-based target computation for sub-states 1/2/3/4 (300 instrs; callee of 0x000F1020's
default path): sub-state 1 resolves the bounds-8 table chain (0x000f2728/0x000f2730),
computes +0x46 = curve(+8/+0xc variant) + curve(+0x4 of the 0x000f29d4 chain) through
0x000f29d0/0x000f29d8 and +0x48 as the difference of the +0x10/+0x14 slot curves; sub-state
3 same but second chain via 0x000f29e4/0x000f29e8; sub-state 2 computes +0x46 = +0x4a +
0x000f29f4(...) and +0x48 = curve(+0x10 variant) - curve(+0x4); sub-state 4 mirrors that
(+0x46 = 0x000f29f4 + curve, +0x48 = +0x4a - curve); sub-state 5 returns 5; other sub-states
report event (0x4de,1,...,0xf,0x24) with +0x4d=0/+0x72=1. Cluster of 1. [I]

#### 0x000F295E `shift_sched_tgt_states12345` — logic
Variant target computation (91 instrs; callee of 0x000F1020): for sub-states 1-5 computes
+0x46/+0x48 from the two variant-selected curves (+8/+0xc and +0x10/+0x14 of the
0x000f29dc/0x000f29e0 chain) converted through 0x000f2a00; other sub-states report event
(0x4de,1,...,0xf,0x50) with +0x4d=0/+0x72=1. Cluster of 1. [I]

#### 0x000F2A66 `shift_sched_gear_match_clear` — logic
Clears ctx+0x58 when the 0x000f2aa4-object code +7 equals the 0x000f2a98 lever +0x21, the
lever pair +0x21/+0x27 is equal, and the code also equals +0x58. Callers 0x000F0602,
0x000F0728. Cluster of 1. [O]

#### 0x000F2AA8 `range_tgt_u16_calc` — logic
Computes the u16 target at ctx+8 via vmethod 0x000f2ad0 with (s16 global *PTR_DAT_000f2acc,
u16 through the 0x000f2ac8-object +0x68 pointer). Caller facade 0x000BB1A4; callee
0x000D2058. Cluster of 1. [O]

#### 0x000F2AD4 `range_obj_ctor_16` — logic
Constructor: allocates 0x16 bytes (allocator PTR_FUN_000f2cf4, direct 0x0009AA34 family) and
runs 0x000F2B3A to clear the object. No resolved callers (interface/vtable invoked). [O]
Cluster of 1.

#### 0x000F2AFC `range_task_step` — logic
Task step (caller facade 0x000BB1B4): when global *PTR_DAT_000f2cf8 == 4 only clears via
0x000F2B3A; otherwise runs 0x000F3072 (hold timer/enable), 0x000F2EE8 (enable copy),
0x000F2D98 (channel B), 0x000F2C0E (channel A) and 0x000F2F28 (byte channel). Always
finishes with 0x000F2B56 (lever-change check), ctx[0] = *PTR_DAT_000f2cfc and the
notification (0x000f2d04, 0x22). [I] range/engagement transition controller (codes 4-8,
spec 14 §4.1 domain). Cluster of 1.

#### 0x000F2B3A `range_ctx_clear` — logic
Zeroes the working fields of the small context: u16s +2/+4/+8/+0xa/+0xc, bytes +6/+0xe/
+0x10/+0x11/+0x12. Callers 0x000F2AFC, 0x000F2AD4. Cluster of 1. [O]

#### 0x000F2B56 `range_lever_change_chk` — logic
Post-step lever-change detection: tracks the previous lever code (0x000f2d08-object +0x50,
kept in *PTR_DAT_000f2d0c) and flags the combinations (lever 2 with codes 6/8 and +0xa0 in
5/7; code 8 with +0xa0 6 and +0xa2 in 1/2; previous lever 4 with lever 2); when flagged and
the global byte (0x000f2d10-obj +0x10 ptr) is 1, notifies both observers
(0x000f2d1c/0x000f2d18 and 0x000f2d20 pairs) and clears the channel-B and channel-A state
(+0xe/+0xf/+0x7/+0x6). Caller 0x000F2AFC. Cluster of 1. [I]

#### 0x000F2C0E `range_chan_a_step` — logic
Channel-A stepping (150 instrs; caller 0x000F2AFC): target u16 from the 0x000f2d10-object
(+0x20/+0x24 by the 0x000f2d08-object +0xa0), reference s16 from curve 0x000f2d28 over the
0x000f2d24-object (+0xf, +4); disabled (+0x14==0) clears the counters (+2/+3); the code pair
(6/8) special-case also clears; direction from the s16(+0x8c) comparison steps counter +2
toward the target and counter +4 in the opposite direction; reaching the target fires the
observer pair (0x000f2f04 or 0x000f2d1c/0x000f2d10 variants) and sets the done flags +7/+6.
Cluster of 1. [I]

#### 0x000F2D98 `range_chan_b_step` — logic
Twin of 0x000F2C0E for channel B: counters +0xa/+0xc over the 0x000f2f14/0x000f2f18 objects
(target via +0/+4, curve 0x000f2f20 over 0x000f2f1c), done flags +0xf/+0xe, observer pair
0x000f2f04/0x000f2f10 with the 0x000f2f24 chain. Caller 0x000F2AFC. Cluster of 1. [I]

#### 0x000F2EE8 `range_enable_copy` — logic
Copies the enable: ctx+0x14 = (ctx+0x15 != 0). Caller 0x000F2AFC. Cluster of 1. [O]

#### 0x000F2F28 `range_byte_chan_step` — logic
Byte-channel stepping (134 instrs; caller 0x000F2AFC): active only when the 0x000f3134-object
bytes +0x127/+0x129 are clear and both speed u16s (+0x7c/+0x7e) exceed the value through the
0x000f3130-object +0x2c pointer, with queries (0x000f3138/0x000f3140) returning 0; direction
from the lever-pair combinations (0x000f3148 +0x2b/+0x2c vs 0x000f3134 +0x122/+0x124) steps
counter +0x10 toward the byte target (0x000f3130-obj +0x1c ptr) and counter +0x11 the other
way; reaching the target notifies the 0x000f3154 observer and latches the state (+0x13,
+0x12 = 2/1). Inactive clears +0x10/+0x11. Cluster of 1. [I]

#### 0x000F3072 `range_hold_timer_step` — logic
Hold-timer and enable computation (caller 0x000F2AFC): increments s16 ctx+8 (stop at -1);
clears +8/+9 when the 0x000f3134-object +0xa0 is 6/8 and the code is 5/7; computes the enable
ctx+0x15 from a condition cascade — lever pair 0/0 (0x000f3148), +0xcd and +0xca != 2, speed
+0x94 above the +0x14 pointer, +0xd3==1, +0xb5==0, byte 300 (0x12c) not 4/2, hold time vs
the curve 0x000f3158 value and +0xa0 not 5/7. [I] engagement enable after a hold time.
Cluster of 1.

#### 0x000F315C `range_obj2_ctor_18` — logic
Constructor-style init: allocates 0x18 bytes (PTR_FUN_000f3344) and sets the four u16
sentinels 0xffff at +6/+8/+0x10/+0x12. No resolved callers (interface invoked). Cluster of 1. [O]

#### 0x000F3180 `range_meas_register` — logic
Registers the four u16 fields (+8, +6, +0x10, +0x12) to the registrar singleton
(vmethod 0x000f334c on the 0x000f3348 object) and calls 0x000f3350 — the telemetry pattern.
Caller 0x000E93D2 (preceding unit). Cluster of 1. [I]

#### 0x000F31B2 `range_task2_step` — logic
Second controller step (303 instrs; caller 0x000E93CA of the preceding unit, twin family of
0x000F2AFC): shifts the enable (+0x15 <- 0x000f3358-obj +0xa4, old to +0x16), runs
0x000F3502 and 0x000F3550 and the 0x000f3350-object call; when the global query
(0x000f3360) is set or +0xd3 clear, resets everything (+8/+9/-1, +0/+3/+4/+0xb=0,
DAT_fff8e6e7=0). Engage decision: (+0xd3 set with the +0xc/+0x15 edges or the +0x16==1
falling edge with +0xa5) and +0xca!=2 and +0xa2==1 and queries (0x000f336c, 0x000F34B8)
passing, plus the +0x0e gate (0x000f3370-obj +0x20 ptr) and the 0x000f3378 validation and
+0xd7 clear -> arm (+0/+3/+4/+0xb/+0xd=1, DAT_fff8e6e7=1, +0xa=0, 0x000f337c call). Hold
counter +0xa steps by the u16(+8) vs curve (0x000f3384 over +0x10 slot) comparison with the
+1/+2 mode byte (+0xa2==3 special), the 0x000f338c observer and the byte limit via the
0x000f3370-obj +0x14 pointer (overrun -> notify 0x000f35d8 pair and disarm); falling edge
tears down; final latches +0xc <- +0xd3, +0xd reset when +0x4 fell, +1 <- +0xa2. [I]
Cluster of 1.

#### 0x000F34B2 `range_hold_u16_clear` — stub
3 instructions: clears the u16 at ctx+6. Caller 0x000E93C0. Cluster of 1. [O]

#### 0x000F34B8 `range_hold_done_pred` — logic
Predicate for 0x000F31B2: returns 1 unless (u16 ctx+6 <= the curve value over the
0x000f35e4-object +0 pointer and (the 0x000f35f0 query != 1 or *PTR_DAT_000f35f8 <= the
curve over the +4 slot)). Callees 0x000BA644/0x000B6EAE. Cluster of 1. [O]

#### 0x000F3502 `range_hold_timer_reset` — logic
When the 0x000f35fc-object +0xd3 is clear: clears ctx+6 when the previous +0xc was 1, and
returns early when u16(+6) is still below the curve value; otherwise (also when +0xd3 set)
calls (0x000f3600-object, 0 or 0x5a96). Caller 0x000F31B2. Cluster of 1. [O]

#### 0x000F3550 `range_hold_latch_step` — logic
Hold-latch state machine gated by the 0x000f35e4-object +0x20 pointer byte: on the
0x000f360c query edge clears (+0x10 u16, +0xe); when +0x10 <= the +0x18-pointer u16 and
+0x14==1 and *PTR_DAT_000f3610==0, latches +0xe=1 and seeds +0x12=0 and +0x10 from the
pointer; +0xe clears when +0x12 reaches the +0x1c-pointer u16; stores +0x14 <- global and
+0xf <- query. Caller 0x000F31B2. Cluster of 1. [I]

#### 0x000F3614 `lib_step_u32_follow` — library
Generic setpoint follower for vtable-equipped objects (ctx: +0 output flag, +4/+8 the up/down
u32 counters, +0xc object with method tables): queries enable (+0x44 slot), target (+0x5c)
and direction (+0x50), steps the counters toward each other by 1 per call, and fires the
+0x18/+0x24 method-table entries with the output flag 1/0. Callers 0x000BBF5A, 0x000F6EBC;
callee 0x000F4700. Cluster of 1. [O]

#### 0x000F36DC `lib_step_f_follow` — library
Float twin of 0x000F3614: same vtable queries (+0x14/+0x44/+0x5c/+0x50/+0x38), steps the
two float counters (param_1[1]/[2]) toward the target, sets the float flag *param_1 to
1.0/0.0 and fires the corresponding method; always finishes with the +0x38 query forwarded
through PTR_FUN_000f37c4. Callers 0x000F6F9A, 0x000BBF5A. Cluster of 1. [O]

#### 0x000F37C8 `lib_u8_to_float` — library
4 instructions: byte -> float conversion. 32 callers, all in the 0x000F3F60-0x0012E6xx
measurement/task band. Cluster of 1. [O]

#### 0x000F37D2 `lib_s16_to_float` — library
4 instructions: s16 -> float conversion. Callers 0x0011C7FA/0x0011C844/0x0011C806. [O]
Cluster of 1.

#### 0x000F37DA `lib_u16_to_float` — library
4 instructions: u16 -> float conversion. 12 callers in the 0x0011-0x0012 band. Cluster of 1. [O]

#### 0x000F37E4 `lib_tbl_u16_get_f` — library
Table read through a float index: u16 at param_2+4+((int)param_1 & 0xffff)*2. Caller
0x0011C636. Cluster of 1. [O]

#### 0x000F37F8 `lib_curve_u8_interp` — library
Byte-curve interpolation (84 instrs): breakpoints are the byte array at param_2+4 (count at
param_2[0]), values the byte array at +count+3; below range -> 0, above -> last segment;
linear interpolation between the bracketing breakpoints, result masked to 0..255. Callers
0x0011B406, 0x0011B368. Cluster of 1. [O]

#### 0x000F38AA `lib_curve_s16_interp_f` — library
Float-argument curve interpolation over an s16 breakpoint/s16 value table (count at [0],
breakpoints at +4, values after): clamps below/above to the first/last segment and
interpolates linearly, returning the s16 result as float. Callers 0x0011B642, 0x0011C354.
Cluster of 1. [O]

#### 0x000F395C `lib_curve_u16_interp_f` — library
u16 variant of 0x000F38AA (u16 breakpoints and values, result masked to 0xffff). Callers
0x0011C812/0x0011C354/0x0011C7E0/0x0011C850/0x0011C860. Cluster of 1. [O]

#### 0x000F3A18 `lib_map2d_s16_interp_f` — library
Bilinear 2D-map interpolation, all-s16: x breakpoints at +8, y breakpoints at +8+2*nx, value
matrix after; clamps on both axes and interpolates first in x then in y; returns float.
Callers 0x0011B592, 0x0011B33C, 0x0011B4E0. Cluster of 1. [O]

#### 0x000F3BAE `lib_map2d_u16_interp_f` — library
u16 variant of 0x000F3A18. Caller 0x0011C888. Cluster of 1. [O]

#### 0x000F3D56 `lib_map2d_min_s16` — library
Scans the value matrix (rows at [0], cols at [1], values from +8) and returns the minimum
s16 (initial 0x7fff). Caller 0x0011B1BE. Cluster of 1. [O]

#### 0x000F3D82 `lib_map2d_min_f` — library
Min scan with a float loop counter over the same matrix layout (pointer arrives pre-negated,
`~param_1`); negative counts are wrapped by adding 2^32. Caller 0x0011B30C. Cluster of 1. [O]

#### 0x000F3DF4 `lib_map2d_max_s16` — library
Max counterpart of 0x000F3D56 (initial -0x8000). Caller 0x0011B1DA. Cluster of 1. [O]

#### 0x000F3E22 `lib_map2d_max_f` — library
Max counterpart of 0x000F3D82 (initial DAT_000f3e90). Caller 0x0011B324. Cluster of 1. [O]

#### 0x000F3E94 `lib_idx_limit_tbl_get` — data-accessor
Returns the s16 at *PTR_DAT_000f3f4c[idx*2]; idx > 0xb yields 0xffff. Caller 0x000F3EAA.
Cluster of 1. [O]

#### 0x000F3EAA `lib_limits_plaus_chk` — library
Calibration plausibility loop: for idx 0..0xb requires the limit from 0x000F3E94 to be <= 499
except idx 10/11; returns 1 while all pass (short-circuits 0). Caller 0x0011D360. [I]
calibration sanity check (spec 09 pattern). Cluster of 1.

#### 0x000F3EE4 `lib_tbl_u32_get_f` — data-accessor
u32 from the table *PTR_DAT_000f3f50 by float index (0..255), zeroed above DAT_000f3f54.
Caller 0x000F3EFC. Cluster of 1. [O]

#### 0x000F3EFC `lib_tbl_scan_plaus_chk` — library
Second plausibility loop: scans float index 0..DAT_000f3f54 and requires every 0x000F3EE4
value >= DAT_000f3f58 except the two special indices DAT_000f3f5c and the top; returns
1.0/0.0. Caller 0x0011D3D6. [I] calibration sanity check. Cluster of 1.

#### 0x000F3F60 `meas_hist_block_update` — logic
Measurement/history refresh (303 instrs; caller 0x000F43A8): enters/exits a trace section
(marker 0x3d/0x3e on the 0x000f4220 object); evaluates five channel pairs — raw value via
0x000f4230 over the curve tables at DAT_fff8c3ac/fff8c410/fff8c474/fff8c4d8/fff8c53c (x from
the 0x000f422c table, lever bytes through the 0x000f4228 chain) and a float via 0x000f423c
with pre-negated table pointers from 0x000f4234 combined by _LAB_000f4240 — then shifts the
previous/current history block: DAT_fff8e414..fff8e458 and DAT_fff8c564..fff8c604 are
rolled (old <- current, current <- fresh). Callees 0x001051BE/0x001050F8 (shift-map band).
[I] measurement snapshot with previous-cycle history (spec 10 pattern). Cluster of 1.

#### 0x000F4244 `meas_pair_ctor_a` — logic
8-byte observer pair constructor (allocator PTR_FUN_000f4378, 0x0009AA34 family): stores the
value DAT_000f437c at +0 and param_2 at +4. No resolved callers (vtable invoked). [O]
Cluster of 1.

#### 0x000F4266 `meas_obj_fwd_187b0` — stub
Vmethod forward with a u16 argument on the object pointer at param_1+4 (slot 0x000f4380 ->
0x001187B0). Cluster of 2: member 0x000F4284 is the same pattern over slot 0x000f4388 ->
0x00118808, named `meas_obj_fwd_187b0__2`. Callers 0x00125D5A/0x0011A6A0 (member: none
resolved). [O]

#### 0x000F4276 `meas_obj_fwd_187da` — stub
No-arg vmethod forward on the object pointer at param_1+4 (slot 0x000f4384 -> 0x001187DA).
Cluster of 2: member 0x000F4294 uses slot 0x000f438c -> 0x00118832, named
`meas_obj_fwd_187da__2`. Callers 0x000BB290; member callers 0x0011CA64/0x0011A6A0. [O]

#### 0x000F42D0 `meas_ptr_u16_lt500` — data-accessor
Returns u16 (via the pointer at param_1+4) < 500. Caller 0x000F5E30 (next unit). [O]
Cluster of 1.

#### 0x000F42E0 `meas_ptr_s16_get` — data-accessor
Returns the s16 through the pointer at param_1+4. Cluster of 2: member 0x0011894A (r10_app,
callers 0x00125D5A/0x00119D32) is byte-identical, named `meas_ptr_s16_get__2`. Callers
0x00126F58/0x00119D32. [O]

#### 0x000F42EC `meas_pair_ctor_b` — logic
Twin of 0x000F4244 storing the vtable label PTR_LAB_000f4390 at +0. No resolved callers.
Cluster of 1. [O]

#### 0x000F430E `meas_obj_fwd_18860` — stub
3-instruction no-arg vmethod forward on the object at param_1+4 (slot 0x000f4394 ->
0x00118860). Cluster of 3: members 0x000F4314 (slot 0x000f4398 -> 0x00118886, caller
0x000BB4EA) and 0x000F4320 (slot 0x000f43a0 -> 0x001188DA, callers 0x0011CD56/0x0011A9EC/
0x000BB4EA), named `meas_obj_fwd_18860__2/__3`. Callers 0x0011A9EC, 0x00125F54. [O]

#### 0x000F4356 `meas_f_lt_thresh_pred` — data-accessor
Returns 1.0 when the float through the pointer at param_1+4 is below *PTR_DAT_000f43a4,
else 0. Caller 0x000F5E92 (next unit). Cluster of 1. [O]

#### 0x000F4368 `meas_ptr_u32_get` — data-accessor
Returns the u32 through the pointer at param_1+4. Cluster of 2: member 0x00118998 (r10_app,
callers 0x00125F54/0x0011A22C) is identical, named `meas_ptr_u32_get__2`. Callers
0x00127118/0x0011A22C. [O]

#### 0x000F43A8 `meas_list_refresh_39` — logic
Periodic measurement refresh (156 instrs; caller facade 0x000BB882): marker 0x39 on
0x000f44ec; steps/queries six object pairs (0x000f4500/0x000f4508, 0x000f4518/0x000f4520,
0x000f4530/0x000f4538, 0x000f4554-arg pair, 0x000f4568/0x000f4570, 0x000f4580/0x000f4588,
0x000f45a0/0x000f45a8, 0x000f45b8/0x000f45c0) with paired get/exchange vmethods, plus two
bare queries (0x000f4598/0x000f459c); marker 0x7b at the end. The object set matches the
0x000F3F60/F4700 measurement family. [I] spec 10 periodic list maintenance. Cluster of 1.

#### 0x000F44E4 `meas_obj_call_11d2e8` — stub
Single vmethod call on the 0x000f45d0 object (slot -> 0x0011D2E8). Caller facade 0x000BB888.
Cluster of 1. [O]

#### 0x000F45D8 `meas_dir_to_pm1` — data-accessor
Maps a direction char to +1, or -1 when it is 2. Caller 0x000F471C. Cluster of 1. [O]

#### 0x000F45E4 `meas_dir_to_f` — data-accessor
Float twin of 0x000F45D8: returns -1.0 when (byte)~param_1 == 2.0 else 1.0. Caller
0x000F471C. Cluster of 1. [O]

#### 0x000F45FC `meas_obj_ctor_2c` — logic
Constructor of a 0x2c-byte measurement object (allocator PTR_FUN_000f470c): zeroes the
fields and seeds defaults byte 6 at +0xc and +0xd, 0xa5 at +0x12. Caller 0x0015C480
(static-init band). [O] Cluster of 1.

#### 0x000F466A `meas_obj_ctor_70` — logic
Constructor of a 0x70-byte object: alternating 0xffff u16 / zero u32 pairs with sentinel
bytes 0xf9 (+0xc), 0xff (+0x10, +0x24..+0x34 step 8). No resolved callers (vtable invoked).
[O] Cluster of 1.

#### 0x000F4700 `meas_notify_idx` — stub
5-instruction forward: (PTR_FUN_000f4718)(PTR_DAT_000f4714, idx) — the per-index
measurement notification used by the whole 0x000F3Fxx-0x000F6xxx band (52 callers).
Callee 0x000BA6F4. [O] Cluster of 1.

#### 0x000F471C `meas_list_refresh_529` — logic
Measurement refresh for list id 0x529 (771 instrs; caller facade 0x000BB876): for each of
~17 channels reads the previous value (vtable pair), queries the new value, reports old and
new through the two writer singletons (0x000f4a70/0x000f4a74 and per-type variants, always
with the list id 0x529), then latches the new value into the RAM mirror block
DAT_fff8c6ec..fff8c784 with float complements (e.g. DAT_fff8c71c = (float)(ushort)~new).
Channel indices 1,2,9,10,0x31,0x32,0xb..0x14,0xd..0x12,0x5f/0x60 (direction pair via
0x000F45D8/0x000F45E4), 99/100; the 0xd/0xe pair keeps the old value when the sentinel
0xa5 is seen. Ends with 0x000F4700(0x2b). [I] spec 10 periodic list. Cluster of 1.

#### 0x000F4E54 `meas_list_refresh_52d` — logic
Measurement refresh for list id 0x52d (752 instrs; caller facade 0x000BB87C): same
pattern over ~24 channels (indices 0x1d..0x28, 0x27, 0x28, 1..8, 0x1b/0x1c, 0x2b/0x2c,
0x2f/0x30) with writers 0x000f5194/0x000f5198 and per-type variants; latches into
DAT_fff8e45c..fff8e47d and DAT_fff8c788..fff8c7d0 with float complements; markers 0x37/0x38
via 0x000f5188/0x000f55a4. [I] spec 10 periodic list. Cluster of 1.
## Unit summary

220 clusters, 235 named functions (11 multi-member clusters; two clusters have cross-region
members in r10_app: 0x0011894A and 0x00118998). Kinds: 146 logic, 50 stub, 22 data-accessor,
17 library, 0 irq/entry/dispatch, 0 unknown.

Module map established for the band 0x000EB6FC-0x000F4E54 (all working hypotheses marked
[I] in the entries, anchored on the r08 facade slots of unit 21 and the error-report module
ids):

- 0xEB6FC-0xEDE1C — maneuver (shift) context manager of the 0x000BAC78-interface module:
  engage/disengage/cancel paths (0xEB900/0xEB9EC/0xEBA6E/0xEBB6C/0xEBC60), the 264-instr
  step task 0xEBDBC, and three phase sub-machines (0xEE18E, 0xEE38C, 0xEE516) whose minimum
  progress is selected by 0xEE902. Event id 0x36b/0xf.
- 0xEE9A4 — direct evidence that DAT_fff8bd6e/DAT_fff8bd6d are a to/from code pair (used as
  1-based table indices); 0xEC134 emits codes 5-8 (spec 14 §4.1 domain).
- 0xEEC54-0xEFBF4 — five-element manager (off-going/on-coming banks, 1-5 element objects,
  element matrix restart/applies; module id 0x36d): plausibly the spec 11 element-matrix
  engine's integer side; the FP model at 0x001066D6-0x00106A3E is reached through the
  0x000eebf0-object thunks 0xEEA96-0xEEBB8.
- 0xEFC3C-0xEFEA8 — small regulator (error/integrator with curve-fed ratios).
- 0xEEFF44-0xF2A66 — scheduler state machine (states 0-8, module id 0x4de): pre-step
  0xF0728, entry/step/done 0xF13AC/0xF1020/0xF0D5C, ramps 0xF1762/0xF24B2, gear
  engage/commit 0xF1C16/0xF1F38, target select 0xF21BA, status snapshot 0xF22E4 into ctx.
- 0xF2AA8-0xF3550 — range-transition controller family (two channel steppers + byte
  channel + hold timer; lever codes 4-8), two task variants (0xF2AFC, 0xF31B2).
- 0xF3614-0xF3EFC — generic helpers: vtable setpoint followers (u32/float), float
  conversion/curve/map library and calibration plausibility checks (spec 09 pattern).
- 0xF3F60-0xF4E54 — measurement-protocol maintenance (spec 10): history block refresh,
  object ctors/forwards and the periodic list refreshers for list ids 0x39, 0x529, 0x52d
  (facades 0x000BB882/0x000BB876/0x000BB87C in r08_app).

Unknown entries: none — every cluster got a name; the module-level interpretation (shift /
element / scheduler / range / measurement) remains [I] and is stated per entry.

Cross-region leads:
- The facade implementations 0x000DD940-0x000DE0FE (callers of the first band) belong to a
  future unit; they should adopt the same context model (ctx flags +1/+2/+3, DAT_fff8bd6d/6e
  to/from codes).
- Heavy outbound calls into the r08 helper band (0xB6xxx-0xB8xxx curve/map/event helpers,
  unit 20) and into the 0x001050F8-0x00106A3E / 0x0010CDB0-0x0010CFB6 model bands — those
  units should cross-check the shift-model interpretation.
- Measurement refreshers write RAM blocks 0xFFF8C6EC-0xFFF8C784 and 0xFFF8E45C-0xFFF8E47D
  with list ids 0x529/0x52d — useful anchors for the spec 10 index table.
- Callers of the float library (0x00118xxx-0x0012Exxx band) implement the calibration
  verification loops; 0x000F3F60's history block (0xFFF8C564-0xFFF8C604) is refreshed by
  0x000F43A8 (facade 0x000BB882).
