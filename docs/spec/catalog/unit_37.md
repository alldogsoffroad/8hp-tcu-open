# Unit 37 (r10_app)

Application-region clusters 0x0010FAAE-0x001183A2 (plus duplicated members at 0x001153xx/0x001173xx).
#### 0x0010FAAE `shift_phase_substate_step` — logic
Five-state sub-machine at ctx+0x14 inside the shift phase object (driver 0x10FA7E reloads the
count target into ctx+0xE each cycle). State 0 waits for the arm flag ctx+4, state 1 holds while
the counter ctx+0xE is positive, states 2/3 gate on a family-block threshold
(ctx+0x2 vs block at ctx+0x3C +0xC) and on a vtable predicate (PTR_FUN_0010fc44 with ctx+8),
state 4 is terminal; transitions clear the counter pair ctx+0xE/+0xF. Cluster of 1. [I]
(spec 11 §3 phase object; exact purpose of the sub-machine open).

#### 0x0010FB52 `shift_ctx_mode_4to8_check` — logic
Returns 1 when the config mode byte at (ctx+0x38)+0x1E is one of 4,5,6,7,8, else 0. Used by
0x10FAF4, 0x112BA, 0x1179E, 0x11860 and by the outer shift module 0xE7126 to branch between
calibration source variants. Cluster of 1. [O] (branch set observable; meaning of the modes [I]).

#### 0x0010FB72 `shift_phase_step` — logic
The phase stepper of the shift phase engine (spec 11 §3): while the current phase reports done
(FUN_001106dc), it increments the step counter ctx+0x18, fetches the next phase from the
sequence table 0x63555 (via FUN_00110ade), and runs the phase entry FUN_0010ffbe; afterwards
stores a vtable-computed value into ctx+2. A phase byte >0x11 raises a detour report
(module 0x367, 0xF, 6, 0x10F2) before stepping. Called by 0x000EB348. Cluster of 1. [O]
(spec 11 §3; spec cross-ref 11-shift-execution.md).

#### 0x0010FBFC `shift_phase_obj_finish` — logic
Resets a phase object to the done state: clears the 16-bit fields at +0xE and +0x2, sets phase
(ctx+9) = 0x11 (17 = done) and the sub-state ctx+0x14 = 0. Called by 0x0011755E. Cluster of 1. [O].

#### 0x0010FC0E `shift_pairval_le_global` — logic
Computes FUN_0010fcbe(ctx, global byte at 0x10FC54+2, global byte at 0x10FC54+1) — a per-gear-pair
calibration value — and returns value <= global short at 0x10FC58. Called by 0x1106DC (phase-1
completion branch) and by the outer shift module 0xEB640/0xEB288. Cluster of 1. [I].

#### 0x0010FC5C `shift_phase_obj_init` — logic
Phase-object initialization called by 0x000EDC5A: zeroes the working fields (+0x18,+9,+0x34,
+0xA,+0xB,+1,+5,+0x15,+7,+0,+0xC, u16 +0x2E,+4), computes the sequence type via FUN_0010ff9e
into ctx+0x19, and caches a config-feature flag into ctx+0x16 (vtable call on cfg+0x3C == 0).
Cluster of 1. [O].

#### 0x0010FCA2 `shift_phase_svc_pair_call` — unknown
Calls a registered service-object method (DAT_0010FF04 on object 0x10FF00) twice, with pointers
ctx+0x12 (the phase tick counter) and ctx+0x2C. Likely a timer start/read pair, but the object
is only reachable through its pointer table; callee 0x000BAE46 is a library helper. Called by
0x00117538. Cluster of 1. [I].

#### 0x0010FCBE `shift_pair_block_value_get` — logic
Resolves a calibration value for a gear pair (from,to): equal indices raise detour 0x367/0xF/3/7/0xC
and return -5000 (&DAT_FFFFEC78); otherwise a 9x9 (from,to) type matrix (0x10FF20 / 0x10FF4C,
row stride 0x24) selects a block type 0..6, the block pointer comes from a set-indexed block
list (PTR_PTR_0010FF28/0x10FF50), and the value is read through a per-family vtable call
(curve/map interpreter). Out-of-range types raise detour 0x367/0xF/6. Matches the 9x9 from/to
table families of spec 11 §2. Cluster of 1. [O] mechanism, [I] role.

#### 0x0010FF9E `shift_phase_seq_type_calc` — logic
Computes the phase-sequence type into ctx+0x19: default 2 (on-going element, full shift), 1
(prepare only) when global flag 0x10FF1C0[2] == 1, and 0 (off-going release) when the step
counter 0x10FF1C0[6] == 0x14 and the type was still 2. Matches spec 11 §3 sequence types.
Cluster of 1. [O].

#### 0x0010FFBE `shift_phase_entry` — dispatch
Per-phase entry dispatcher of the phase engine (spec 11 §3 table): switch on the phase byte
ctx+9 covering fill start (1: duration from shift-type table or ctx+0xC4 + curve + map - 0x80),
fast fill (2), fill compensation (3: pressure + ramp start), ramp reset (4), torque phase
(10/11), torque handover (12), inertia (13), end/close (14/15, ramp toward 20000 mbar 0x4E20),
done (17); unknown phases raise detour 0x367/0xF/2/0xB/0x37. Writes targets into ctx+0x20 via
DAT_001104C0. Heavy use of family descriptors ctx+0x3C..0x50 and the 0x10F4EC/0x10F526
index resolvers. Cluster of 1. [O] (spec 11 §3/§3b).

#### 0x00110430 `shift_phase2_done_check` — logic
Phase-2 (fast fill) completion: with cfg+3 == 1, either a 28-entry (0x1B) family value (via
gear-index vtable, compared against curve at cfg+0xB0) or a map read minus 0x80 must fall
below that curve; with cfg+3 == 0 a plain timer (ctx+0x20 <= tick ctx+0x12). Out-of-range
family index raises detour 0x367/0xF/6. Spec 11 §3 row "fast fill". Cluster of 1. [O].

#### 0x0011058A `shift_phase3_done_check` — logic
Phase-3 (fill compensation) completion: timer (ctx+0x20 <= tick ctx+0x12) when cfg+3 == 0;
otherwise immediate done if global 0x10F06C0 != 0, done when the vtable value (PTR_FUN_001106C4)
is in [-1, cfg+0xBA] with cfg+0xB4 set, or when it exceeds the curve at cfg+0xB0. Spec 11 §3
row "timer or pressure reached". Cluster of 1. [O].

#### 0x00110600 `shift_phase_abort_cond` — logic
Computes the abort/interrupt flag stored to ctx+6 by the driver 0x10FA7E: returns 1 when the
phase is 1, or when a global request pair differs / a global enable is 0
(PTR 0x10F06C8 != 0x10F06CC or *0x10F06D0 == 0) and the sub-state, mode byte (PTR 0x10F0698[2]),
arm flags and down-counter ctx+0xE allow it. Meaning [I]: signals the outer module that the
phase object lost its precondition. Cluster of 1. [I].

#### 0x001106DC `shift_phase_done_check` — dispatch
Per-phase completion dispatcher (spec 11 §3): state 1 timer/torque (uses 0x10FC0E branch),
state 2 -> FUN_00110430, state 3 -> FUN_0011058A, state 4 -> FUN_0010FA28 or speed threshold,
state 9 speed threshold against ctx+10, states 10/11 timers, state 12 speed condition against
the map at (+0x18 of the 43-type block), state 13 progress >= FUN_00110bf0 threshold, state 14
progress/speed difference plus 0x10BF0/0x10C80 checks, state 15 timer, 17 false. Recomputes the
cached cfg-feature flag ctx+0x16 when cfg+3 == 0. Cluster of 1. [O].

#### 0x00110ADE `shift_phase_next_get` — data-accessor
Returns the next phase byte from the sequence table at 0x63555, indexed
[sequence type ctx+0x19 + step counter ctx+0x18 * 3]. This is the transition table behind
0x10FB72 (spec 11 §3). Cluster of 1. [O].

#### 0x00110AF4 `shift_ctx_sel_byte_get` — data-accessor
Selects a byte from the phase context's family blocks: global selector 0x10F0B88 == 3 with ctx
mode in 4..8 -> vtable read of (ctx+0x44)+4 for modes 4/7/8, direct byte at block+4 for mode 5
and block+8 for mode 6 (bad mode raises detour 0x367/0xF/10/6/0x7E); selector 1 -> byte from
(ctx+0x3C)+4; otherwise byte at (ctx+0x44)+4. Cluster of 1. [O] mechanism, [I] meaning.

#### 0x00110BF0 `shift_type43_thr_get` — data-accessor
Resolves the 42-entry (0x2A) shift-type family block via the gear-index vtable trio
(PTR_FUN_00110E10/E14/E18 on cfg+7/+1/+2, bounds check -> detour 0x367/0xF/6) and returns the
byte at block+0x30 — the progress threshold used by phase-13 completion (spec 11 §3).
Cluster of 1. [O].

#### 0x00110C80 `shift_fam48_byte_get` — data-accessor
Five-instruction getter: returns the byte at *(ctx+0x48 family block) + 4; used by phase-14
completion in 0x1106DC. Cluster of 1. [O].

#### 0x00110C8E `shift_meas_add_clamp` — logic
Adds offset param_2 to a global measurement read through a vtable getter (PTR_FUN_00110E30 on
object 0x10E2C) with saturation (PTR_FUN_00110E34); a negative raw reading triggers a second
read, clamp and a store call into the context (PTR_FUN_00110E38). Skipped while the phase is
0x11 (done) or 0x0F. Called by the driver 0x10FA7E. Cluster of 1. [I].

#### 0x00110CDC `shift_phase_target_calc` — dispatch
Per-phase target/torque computation dispatcher called by 0x10FB72 after each phase entry
(spec 11 §3): phase 1 passes 0 through the clamp, 2 -> FUN_001115c2 + map interp via
0x10E3C, 3 -> ramp call on ctx+0x74 with FUN_001114cc, 4 -> ramp value plus vtable torque
term, 10 -> FUN_001115e0, 11 -> FUN_001111fe + 0x11CE6 + fast-fill blend (0x1110F0 block),
12 -> FUN_00111860 ramp + 0x1111FE/0x11CE6 + interp (0x11110C), 13 -> ramp on ctx+0x6C with
FUN_001116d0, 14 -> map read (block +0x28) from ctx tick, 15 -> 0x1179E ramp + 0x1111FE blend,
17 -> identity clamp; unknown phases raise detour 0x367/0xF/1/0xC/8. Result clamped by
PTR_FUN_00110E34/001113A4. Cluster of 1. [O] structure, [I] per-phase names.

#### 0x001111FE `shift_press_target_sum` — logic
Pressure-target sum used by phases 12/15: element base FUN_001112ba + family value
((ctx+0x48)+4 block via 0x113AC with a global short) + ctx+0x1E + ctx+0x1C, plus the optional
torque-dependent term FUN_00111d14 gated by global flag 0x10F13B0, then clamped via 0x113A4.
Cluster of 1. [O] (spec 11 §3b sum pattern).

#### 0x00111254 `shift_press_target_sum_b` — logic
Variant of FUN_001111FE for phases 13/14: same base sum (FUN_001112ba + family value +
ctx+0x1E + ctx+0x1C) but adds FUN_00111ce6 when cfg+3 == 1 (instead of unconditionally),
plus the optional torque term FUN_00111d14, clamped via 0x113A4. Cluster of 1. [I].

#### 0x001112BA `shift_elem_base_press_get` — logic
Element base pressure: with the fast path (flag 0x10F1374[4] or cfg+3 == 0) reads through the
vtable (0x11378 on block 0x10F1374) and then, per ctx mode +0x1E (predicate 0x113B4 == 1),
dispatches to the pressure-map readers FUN_00111dc2 (mode 5), FUN_00111e28 (mode 6),
FUN_00111f7c (modes 4/7/8) or FUN_00111ece (predicate != 1); mode 4 in the vtable check raises
detour 0x367/0xF/4/0xD. Slow path resolves the 42-entry (0x2A) shift-type block and reads the
map at block+0x20. Result clamped (0x116B8). Matches spec 11 §2/§3b. Cluster of 1. [O].

#### 0x001114CC `shift_fillcomp_press_calc` — logic
Fill-compensation pressure (spec 11 §3b): element-family map value (0x116B4 on block +4,
x = cfg+0xC, y = summary short +0x16) + shift-type term FUN_0011200e + requested offset
param_2 + optional torque-dependent term FUN_00111d14 (flag 0x10F13B0), clamped at zero and
by 0x116B8. Out-of-range family index -> detour 0x367/0xF/6. Cluster of 1. [O].

#### 0x001115C2 `shift_alt_map_read` — data-accessor
Alternative fill-parameter read (spec 11 §3b): map at family block +0x18 with axes (short at
0x10F16A0+0x16, byte +0xF) through the 0x116B4 interpreter. Cluster of 1. [O].
#### 0x001115E0 `shift_phase10_target_calc` — logic
Phase-10 (torque phase start) target: base pressure sum FUN_001111fe plus the 42-entry
shift-type block value (block slot via 0x10F526-family gear-index trio, curve at +0x18 read by
0x116C8), blended by 0x116CC with the element index ctx+8. Called by 0x110CDC phase 10.
Cluster of 1. [I] (structure [O], role per spec 11 §3 row 10).

#### 0x001116D0 `shift_inertia_term_calc` — logic
Inertia-phase term for phases 13/14: map at (42-entry shift-type block)+0x2C, x = cfg+0x0C
(interp 0x11988), scaled by percent/100 from the byte map at ((ctx+0x48)+4 block)+0xC
(axis byte +0xF). Called by 0x110CDC phases 13/14. Cluster of 1. [I].

#### 0x0011179E `shift_close_curve_sel_get` — logic
Close-phase (15) value: curve slot +0x18 vs +0x14 selected by the mode predicate 0x119A4 and
global 0x10F19A8, from the (ctx+0x44)+4 block in the fast path or the (ctx+0x48)+4 block in the
slow path; read via 0x119B0 with x = global short 0x10F19AC. cfg+5 == 4 raises detour
0x367/0xF/4/0xE/0x6C. Cluster of 1. [I].

#### 0x00111860 `shift_ramp_target_get` — logic
Ramp target for phases 12/15: fast path curve from the (ctx+0x44)+4 block, slot +0x10 (mode
predicate 0x119A4 == 1) or +0xC; slow path the 42-entry shift-type block map at +0x10, x = set
short +0x16; read via 0x119B0. cfg+5 == 4 raises detour 0x367/0xF/4/0xE/1. Cluster of 1. [I].

#### 0x001119B4 `shift_mode_curve8_sel_get` — data-accessor
Curve read selected by global 0x10F1C2C == 2 and ctx mode +0x1E: mode 7 -> block(0x10F1C34)+0x18,
mode 8 -> +0x14, otherwise ((ctx+0x44)+4)+8; selector != 2 -> ((ctx+0x3C)+4)+8; value from
DAT_00111C38(curve, global short 0x10F1C30). Called from the phase-2 entry path in 0x10FFBE.
Cluster of 1. [I].

#### 0x00111A06 `shift_fastfill_press_calc` — logic
Fast-fill pressure (spec 11 §3b): fast-fill object at cfg+0xB0 read through 0x11C3C, plus either
the 27-entry (0x1B) family term FUN_0011202c (block byte +0xE, minus 0x80) or, when global
0x10F1C40 selector != 0, the temperature map at (0x10F1C34 block)+0x3C minus 0x80; result
clamped >= 0. Out-of-range family index -> detour 0x367/0xF/6. Cluster of 1. [O] (spec 11 §3b).

#### 0x00111AE2 `shift_fillcomp_base_get` — logic
Fill-compensation base pressure: fast-fill object cfg+0xB0 (direct +0xBE when cfg+0xB5 set)
plus the 27-entry family term FUN_0011204e or the temperature map at (0x10F1C34)+0x40 minus
0x80, plus the guarded block read FUN_00111ff4 with x = cfg+0x14, folded through 0x11C70.
Used by 0x110CDC phase 3 and the phase-3 completion 0x11058A. Cluster of 1. [I].

#### 0x00111CAE `shift_val_limit_select` — logic
Keeps param_2 only while it does not exceed the vtable value (PTR_FUN_00111EFC on object
0x10F1EF8 with param_3); otherwise returns the no-argument getter PTR_FUN_00111F00. Called by
the phase-object helper 0x10FA5A. Cluster of 1. [I].

#### 0x00111CD6 `shift_store_speed_diff` — logic
Six instructions: ctx+0x30 = global short (0x10F1F04) - ctx+0x32. Called by FUN_00111ce6.
Cluster of 1. [O].

#### 0x00111CE6 `shift_speeddiff_term_get` — logic
Computes the speed-difference term added in phases 13/15: runs FUN_00111cd6 (diff into
ctx+0x30), and when global 0x10F1F08 == 2 reads the map at ((ctx+0x48)+4)+0x20 with x =
ctx+0x30. Returns 0 otherwise. Cluster of 1. [I].

#### 0x00111D14 `shift_torque_term_calc` — logic
The torque-dependent term of spec 11 §3b: active only when the gate flag ctx+0x34 is set (by
0x111D86); selects the map from the ctx+0x54 family by shift kind cfg+7 (2 -> slot +0, 1/3/4 ->
+4, 5/6/7/8 -> +8) and interpolates via 0x111F14 with axes from block 0x10F1F10 (+0xF, +0x16).
Cluster of 1. [O] (spec 11 §3b).

#### 0x00111D86 `shift_torque_term_gate_set` — logic
Sets the torque-term gate ctx+0x34: 1 when the element-status object 0x10F1F18 reports state 1
for element (ctx+8 - 1), the global active-element byte at +0x13 equals ctx+8, and global
0x10F1F20[0x22] == 1; else 0. Called from the phase entry 0x10FFBE. Cluster of 1. [I].

#### 0x00111DC2 `shift_mode5_press_map_get` — logic
Element pressure-map reader for ctx mode 5 (called by 0x112BA): param_3 == 1 -> map at
block+0x24, == 2 -> +0x28, else set-indexed +0x1C; x axis from PTR_FUN_00111F24 (param_2 in the
default branch), y = global short 0x10F1F28, interpolated by 0x111F14. Cluster of 2
(0x00111DC2, 0x00111E28); member 0x00111E28 reads the +0x34/+0x38/+0x2C slots (mode 6).
[O] mechanism, [I] mode meaning.

#### 0x00111E8E `shift_opt_block_byte_get` — data-accessor
Guarded optional-block read: returns the byte at *(p+4) when the block pointer at +4 is not the
self-sentinel (list-end marker), else 0. Cluster of 2 (0x00111E8E called by 0x110AF4, 0x00115388
called by 0x114E86) — same pattern duplicated in the later 0x114xxx module. [O].

#### 0x00111E9E `shift_opt_block_map20_get` — data-accessor
Guarded read at block slot +0x20: y = curve value of global short 0x10F1F30 (PTR_FUN_00111F34),
interpolated by 0x111F14 with x = global short 0x10F1F28; 0 when the block is absent
(self-sentinel at +0x20). Called by 0x10FCBE. Cluster of 1. [I].

#### 0x00111ECE `shift_def_press_map_get` — logic
Element pressure-map reader for the default branch of 0x112BA (mode predicate != 1):
param_3 == 1 -> +0x34, == 2 -> +0x38, else set-indexed +0x24; branch 1 interpolates via
0x111F24/0x111F14/0x10F1F28, the others via 0x112070/0x112078/0x10F12074. Cluster of 1. [I].

#### 0x00111F7C `shift_mode478_press_map_get` — logic
Element pressure-map reader for ctx modes 4/7/8 (called by 0x112BA): param_3 == 1 -> +0x3C,
== 2 -> +0x40, else set-indexed +0x2C; axes via 0x112070 and 0x10F12074, interpolated by
0x112078. Cluster of 1. [I].

#### 0x00111FF4 `shift_opt_block_call8` — data-accessor
Guarded read at block slot +8: PTR_FUN_00112080(*(p+8), param_2) when the pointer at +8 is not
the self-sentinel, else 0. Used by 0x111AE2 (fill-compensation base). Cluster of 1. [O].

#### 0x0011200E `shift_opt_block_call10` — data-accessor
Guarded read at block slot +0x10: PTR_FUN_00112080(*(p+0x10), global DAT_00112084) or 0 — the
shift-type term of the fill-compensation sum (spec 11 §3b "0x11200E"). Cluster of 1. [O].

#### 0x0011202C `shift_opt_block_sbyte_c` — data-accessor
Guarded byte read at block slot +0x0C: (vtable value & 0xFF) - 0x80 or 0; the signed-byte term
used by fast-fill (0x111A06) and phase-2 completion (0x110430). Cluster of 1. [O].

#### 0x0011204E `shift_opt_block_sbyte_4` — data-accessor
Guarded byte read at block slot +4: (vtable value & 0xFF) - 0x80 or 0; used by the
fill-compensation base 0x111AE2. Cluster of 1. [O].

#### 0x00112090 `tbl_gearpair_9x9_get` — data-accessor
Nine-by-nine matrix accessor for the 0x112xxx-0x114xxx module: param_1 (1..4, else 5) selects
one of five base tables (PTR_LAB_00112284/88/8C/90/94), returns the u32 at
table + col*4 + row*0x24 (row stride 9 u32). Same 9x9 from/to shape as the shift-type index
tables of spec 11 §2. Callers include 0x112FB4, 0x113DAA, 0x1136DE, 0x114930. Cluster of 1.
[O] mechanism, [I] table roles.

#### 0x001120D4 `tbl_gearpair_9x9_b_get` — data-accessor
Second nine-by-nine matrix accessor: param_1 (1, 2, 4, else) selects PTR_LAB_00112298/9C/A0 or
DAT_001122A4, entry at table + col*4 + row*0x24. Heavily used by the 0x112xxx-0x114xxx module
(0x112A54, 0x112BFC, 0x1138FC, 0x113DAA, 0x114688, 0x114E86, 0x12142 ...). Cluster of 1.
[O] mechanism, [I] table roles.

#### 0x0011210E `shift_obj84_handler_install` — logic
One-time setup: fetches an object via the service vtable with id 0x84 (or uses param_1), stores
two handler pointers (0x1122AC, 0x1122B0) into its +0x78 slot, and clears the four status bytes
at 0xFFF8E6F4-0xFFF8E6F7. Called by 0x00117240; callee 0x0009AA34 is an OS helper. Cluster of 1. [I].
#### 0x00112142 `shift_progress_grade_calc` — logic
Computes the 0..5 shift progress grade (ctx+0x11, mirror at ctx+0x1B) for the outer shift
module (callers 0xEDC5A, 0xEBC60, 0xEBDBC). Gate: global 0x1122B4[1] == 1 with mode [2] in
{1,2}. Mode 1 stores a target time (ctx+0x14 + FUN_00112a54 - vtable value via 0x122C0/0x122C4)
into ctx+0x1E; mode 2 recomputes the target from the 9x9 table family (0x1120D4 -> 60-entry
blocks, 0x1256C/0x12570) minus a global. When the timer ctx+0x1C reaches the target the grade
becomes (elapsed - ctx+0x16) / rate from a block map (0x12594); grade capped at 4, then 5.
Out-of-range table type -> detour 0x367/0xF/6. Cluster of 1. [I].

#### 0x00112410 `shift2_family_load` — logic
Loads the six family block pointers for the second shift state engine: table entries
PTR_PTR_001125A4/A8/AC/B0/B4/B8[param_2*4] into ctx+0x4C..ctx+0x60, and sets cfg+0x27 = 1.
Analogue of 0x10F584 (spec 11 §2 family descriptors). Called by 0x00117292. Cluster of 1. [O].

#### 0x00112446 `shift2_cfg_ptr_set` — stub
Two instructions: stores param_2 into ctx+0x48 (the config/calibration pointer of the second
engine object). Called by 0x00117CE4. Cluster of 1. [O].

#### 0x0011244C `shift2_type_blocks_load` — logic
Per-mode calibration block installation for the second engine (called by 0x001176E2). Mode
param_2 1/2: three 9x9 (from,to) matrices (0x1125C0/0x1125C8 and 0x112090) resolve block ids
(0..6 and 0..0x22) into the pair-lists behind ctx+0x50/0x4C/0x5C; mode 3/4: matrices
0x112868/0x112878 (0..0x11, 0..6) plus 0x1120D4 (0..0x3C) into ctx+0x58/0x54/0x60 (mode 4 uses
row param_3-1). Finally ctx+0x13 in 1..5 selects the block table at 0x112884 into ctx+0x64.
Every id passes the set-indexed block list with detour 0x367/0xF/6 on range errors. Cluster of 1.
[O] mechanism, [I] block roles.

#### 0x00112888 `shift2_phase_step` — logic
Phase stepper of the second shift state engine (mirror of 0x10FB72, same caller 0x000EB348):
while the done-check (PTR_FUN_00112AE8) reports complete, advances step ctx+0x28, fetches the
next state from the sequence (PTR_FUN_00112AF0), and runs the entry FUN_001132f4; a state byte
> 0x13 raises detour 0x365/0xF/4/0x14/0x72; finally stores a vtable value into ctx+2.
Cluster of 1. [O] structure, [I] state meanings.

#### 0x00112916 `shift2_driver_tick` — logic
Per-cycle driver of the second engine (caller 0x000EB348): when cfg+0xC3 is set, refreshes
ctx+0x16 from ctx+2 or a vtable value (when cfg+3 != 0) and sets ctx+8 = (state == 1).
Cluster of 1. [I].

#### 0x0011294C `shift2_obj_finish` — logic
Resets the second engine object: zeroes the u16 fields at +0x16, +0x2 and +0x3A and the byte
+0x22. Called by 0x0011755E (same caller family as 0x10FBFC). Cluster of 1. [O].

#### 0x0011295C `shift2_svc_fields_call` — unknown
Calls two service-object methods (PTR_FUN_00112AFC / 00112B00 on object 0x112AF8) on six
context fields (+0x1A, +0x10, +0xA, +0xC, +0x3E, +0x1C) - the same pattern as 0x10FCA2, most
likely timer start/read pairs; the objects are only reachable via pointer tables. Called by
0x00117538. Cluster of 1. [I].

#### 0x0011299A `shift2_obj_init` — logic
Initializes the second engine object (caller 0x000EDC5A): sequence type via FUN_001132e2 into
ctx+0x29, clears the working fields (0x28,0x12,0,4,5,6,7,0x10,0xC,0x2E,0x44,0x40,0x42,0x21),
vtable call with arg 1, target via FUN_00113788 into ctx+0x14; when global 0x112B08[6] is 0xA
or 0x1E copies cfg+0x1A into ctx+0x36, else clears it; clears ctx+0x3A. Cluster of 1. [O].

#### 0x00112A08 `shift2_press_sum_conv_get` — logic
Returns FUN_00112a54 + FUN_00112bfc (the two cached second-engine values) when global
0x112B0C == 1, else 0x7FFF. Called by the converter/lockup module 0x000DE13A (spec 09 §3 area).
Cluster of 1. [O].

#### 0x00112A54 `shift2_val24_calc` — logic
Computes and caches the second engine's value at ctx+0x24 (skipped while ctx[0] holds): mode
DAT_00112B08[2] == 1 -> sum of a family map value + two ctx vtable reads, or (predicate &&
cfg+3 == 1) a map at (0x112B20 block)+0x14; mode 2 -> 9x9 table 0x1120D4 -> 60-entry block ->
byte read; other modes raise detour 0x365/0xF/2/0xF/8. Read by the element phase engine
(0x111A06) and the progress tracker. Cluster of 1. [I].

#### 0x00112BFC `shift2_val26_calc` — logic
Computes and caches the second engine's value at ctx+0x26 (skipped while ctx+4 holds): mode 1
-> product terms ((family map byte x curve byte)/100, or x3/10000 with a third factor);
mode 2 -> 9x9 table 0x1120D4 -> 60-entry block -> byte; stores a short at ctx+0x26. Used by
0x12142, 0x112A08 and the element engine (0x111AE2). Cluster of 1. [I].

#### 0x00112D16 `shift2_ref_val_get` — logic
Cross-engine reference for the element phase engine (callers 0x110430/0x11058A/0x110CDC):
refreshes the second engine's target (FUN_00113788) and cached value (FUN_00112a54); while the
second engine is in state 1/2 returns -(ctx+0x14 + value + 1), otherwise -(value - vtable of
ctx+0x3E); while armed (ctx+4) returns the vtable value of ctx+0xA. [I] speed/torque threshold
taken from the coordination object. Cluster of 1.

#### 0x00112DB0 `shift2_torque_est_calc` — logic
Large estimation for the second engine (caller 0x000EBDBC): requires global 0x113014 == 1 and
range byte 0x113018 in 1..7 (else returns clamp(0) via FUN_001132a8). Resolves the shift kind
(1..4) and a stage code (2/4/6/8/9) through vtable probes on object 0x11301C, gathers
signed-byte family terms (block +0x18 deref -0x80), 9x9-table terms via 0x112090 (0..0x22
blocks), optional map read (0x13354) or additional family terms plus FUN_00113eb0, applies a
percent scale ((a*b)/100) and clamps via 0x13368. Cluster of 1. [I] (structure [O]).

#### 0x00112FB4 `shift2_torque_est_variant` — logic
Register-argument variant of the 0x112DB0 estimation (same block reads 0x112090/0x13078
family, same percent scale and clamp); the decompiler shows the parameters live in unassigned
registers, and callers = '-' - it is entered through a function-pointer table, not a direct
call. Cluster of 1. [I].

#### 0x001132A8 `shift2_clamp_zero` — logic
Returns the module limiter applied to zero: (0x13368)(0) - the disabled/fallback result used by
0x112DB0. Cluster of 1. [O].

#### 0x001132BC `shift2_opt_map20_read` — data-accessor
Conditional map read: when param_2 != 0 and global 0x11336C[2] == 1, reads the map at
((ctx+0x64 block))+0x20 with axis byte 0x11336C[0xE] via 0x13370. Called by 0x00113E88.
Cluster of 1. [I].

#### 0x001132E2 `shift2_seq_type_calc` — logic
Sequence type for the second engine into ctx+0x29: 2 when global 0x113374 == 2, else 1 (no
off-going type here). Called from the init 0x0011299A. Cluster of 1. [O].

#### 0x001132F4 `shift2_phase_entry` — dispatch
Per-state entry dispatcher of the second engine (mirror of 0x10FFBE, called by 0x00112888):
state 1 takes FUN_001136de (when cfg+3 != 0); 2 sets cfg+0x1F = 1 and clears ctx+0x22/0x36;
4 resets the ramp object at ctx+0x78 vtable; 5 runs the gate FUN_00113890, sets cfg+6 = 1,
takes FUN_00112a54 and FUN_00113e88 into ctx+0x36; 9 evaluates FUN_001138c6 and FUN_00112bfc
with table sums into ctx+0x18; 10 ramps and optionally calls 0x1135A0; 11 accumulates five
terms into ctx+0x36; 13 sets flags and runs the torque path 0x113838; 14/18 clear cfg+0x31;
19 sets cfg+6 = 1 and takes FUN_00113e88; all results clamped via 0x113574 into ctx+0x32.
Cluster of 1. [O] structure, [I] state names.

#### 0x001136DE `shift2_mode_val_get` — logic
Mode-dependent byte for the second engine: mode 0x113834[2] == 1 -> 9x9 table 0x112090 ->
0x22-entry block -> vtable byte; otherwise the byte at ((ctx+0x58)block)+4. Used by the
state-1 entry and the target calc 0x113788. Cluster of 1. [I].

#### 0x00113788 `shift2_target_calc` — logic
Refreshes the second engine's target ctx+0x14: 0x7FFF when cfg+3 == 0; otherwise base
FUN_001136de (minus ctx+0x1A in state 1) plus a six-argument vtable value (0x113888), clamped
via 0x113830. Called by the init 0x11299A and by 0x112D16. Cluster of 1. [I].

#### 0x0011381E `shift2_store_diff3c` — logic
Seven instructions: ctx+0x3C = global short (0x11388C) - cfg+0x16. Called by 0x114BAA/0x114C50.
Cluster of 1. [O].

#### 0x00113890 `shift2_gate43_set` — logic
Sets gate ctx+0x43: 1 when the element-status object 0x113AA0 reports state 1 for element
(ctx+0x13 - 1), or state 2 while ctx+0x22 == 0; else 0. Mirror of 0x111D86. Called from the
state-5 entry. Cluster of 1. [I].

#### 0x001138C6 `shift2_sync_cond_check` — logic
Synchronous-point condition: 1 only when param_2 == 1 (to-gear), param_3 == 2 (from-gear),
global object 0x113AA8 has +0x7C == 0, the speed byte behind object 0x113AA0+4 is below the
short at 0x113AA8+0xC4, and 0x113AA8[0x4B] == 0. Used by the state-9 entry (wait for sync).
Cluster of 1. [I].

#### 0x001138FC `shift2_phase_done_check` — dispatch
Per-state completion dispatcher of the second engine (mirror of 0x1106DC, called by
0x00112888): state 0 -> done; 1 -> timer/speed condition (0x113AB4 against ctx+0x32/ctx+0x1A),
FUN_00113cbe and the mode-3/4 special-case, or the armed path via ctx+7; 2 -> armed or
0x113AB4; 19(0x13) -> cfg+3/cfg+5 checks plus map -0x80 vs global short; 4/5 -> ctx+0x2E gate
with mode checks and FUN_00113c82/0x113C40; 9/10 -> FUN_00113c82 or armed; 11 -> armed or
global flag; 12 -> 0x3C-block value (9x9 0x1120D4) vs cfg+0x10; 13 -> FUN_00113d28 vs global
byte 0x113D6C. Cluster of 1. [O] structure, [I] names.
#### 0x00113C40 `shift2_press_cond_check` — logic
Condition for the second engine states 4/5 (caller 0x1138FC): reads a family map through
(ctx+0x54)+4, slot +0xC when globals 0x113D70 == 4 and 0x113D74 == 3 (else slot +8), and
returns (vtable value - 0x80) <= global short 0x113D80. Cluster of 1. [I].

#### 0x00113C82 `shift2_speed_cond_check` — logic
Speed condition of the second engine: with flag 0x113D48[7] clear, either the vtable value of
the cfg object (when 0x113D48[2] == 2 and cfg+0x20 == 0) or FUN_00113daa() must be <= the short
at cfg+0x0A. Called twice from 0x1138FC (states 9/10 and 4/5). Cluster of 1. [I].

#### 0x00113CBE `shift2_torque_cond_check` — logic
Torque condition of the second engine state 1: with mode 0x113D88[5] == 4 the map comes from
((ctx+0x54)+4)+4, otherwise from ((ctx+0x4C)+4) when global 0x113D90 == 1 else ((ctx+0x54)+4);
returns map value (x = global short 0x113D8C) <= short at 0x113D88+10. Cluster of 1. [I].

#### 0x00113D0A `shift2_phase_next_get` — data-accessor
Next state of the second engine from its sequence table 0x113D98, indexed
[seq type ctx+0x29 + step ctx+0x28 * 3]; returns 14 when the armed flag ctx+7 is set. Called by
the stepper 0x112888. Cluster of 1. [O].

#### 0x00113D28 `shift2_mode_byte_get` — data-accessor
Returns the byte at ((ctx+0x50)+4 block)+0x14 when global 0x113D90 == 1, else at
((ctx+0x58)+4 block)+0. Used by the state-13 completion. Cluster of 1. [I].

#### 0x00113DAA `shift2_speed_val_get` — logic
Speed/threshold value for the second engine: mode 0x113FF4[2] == 1 -> 9x9 table 0x112090 ->
0x22-entry block -> vtable byte; otherwise a map read ((ctx+0x58)+4 block)+0x3C (cfg+3 == 1
and global 0x113D18 set, x = global + cfg+0xC) or block+0x38 (axis byte +0xE), minus 0x80.
Called by 0x113C82 and the progress tracker 0x12142. Cluster of 1. [I].

#### 0x00113E88 `shift2_state5_target_get` — logic
Target of the second engine states 5/19: FUN_001149e0 + the conditional map FUN_001132bc
(gated by cfg+0x1C), clamped via 0x114028. Called by the entry dispatcher 0x1132F4.
Cluster of 1. [I].

#### 0x00113EB0 `shift2_kind_term_get` — logic
Shift-kind term (param_2 2/3/4 -> slots +0x18/+0x1C/+0x20 of the block behind pointer
0x11402C, else 0), read only when cfg+0x1C is set, through the 0x114020 map interpreter with
two global axis bytes. Used by 0x112A54 and 0x112FB4. Cluster of 1. [I].

#### 0x00113F16 `shift2_phase_target_calc` — dispatch
Per-state target dispatcher of the second engine (mirror of 0x110CDC, called by 0x112888):
states 4/19 add the ramp value FUN_00114baa (state 4 through 0x114040); state 5 adds
FUN_00114cbe/FUN_00114bdc/FUN_00114baa when global 0x114044 != 1; state 9 blends
FUN_001144f0/0x114688/0x1147b0/0x114c50 through 0x114e86 and 0x1142A0; state 10 drives the
ramp object at ctx+0x78 (vtable +0x20) from FUN_00114ad0 or a set-indexed map, then
0x1144F0/0x114688/0x114c50/0x1150D4 and cfg+0x31 = flag[7]; state 11 ramps ctx+0x7C from a
0x3C-block and sums 0x114688/0x114c50; state 12 stores a 0x3C-block value in ctx+0x38; state
13 ramps FUN_0011483c and sums 0x1144F0/0x114688/0x1147b0/0x114b80; states 14/18 use
0x114578; states 4/5 set ctx+0x2E = 1; unknown states raise detour 0x365/0xF/8/0x14/0x43.
Result clamped >= 0. Cluster of 1. [O] structure, [I] names.

#### 0x001144B0 `shift2_meas_add_clamp` — logic
Reads a global measurement through the vtable getter (PTR_FUN_00114588 on 0x114584), adds
param_2 with saturation (0x114564); a negative raw value triggers the re-read plus a store
call into the context (0x114574). Mirror of 0x110C8E; called by the driver 0x112916.
Cluster of 1. [I].

#### 0x001144F0 `shift2_press_base_sum` — logic
Base sum for the second engine (callers 0x113F16, 0x1132F4): FUN_0011505a + the 0x22-block
value (9x9 0x112090 -> FUN_0011526a with x = cfg+0xC) + FUN_00114e54 + map at
((ctx+0x50)+4)+0x28 (x = param_2) + FUN_00114c82; optional max-hold of the block+0x44 map into
ctx+0x40 (gate ctx+0x42), optional cfg+0x1C term from (ctx+0x64 block)+0x24, optional
torque-kind term FUN_00114a1c (gate ctx+0x43) and the flagged block+0x4C map; clamped by
0x114800. Cluster of 1. [I].

#### 0x00114688 `shift2_press_base_sum_b` — logic
Alternate base sum: with cfg+3 == 1 and global 0x114804 set, the map at ((ctx+0x58)+4)+0x30
(set-indexed, x = cfg+0xC, y = global short); otherwise the 0x3C-block (9x9 0x1120D4 ->
FUN_0011542e) value; plus the optional kind term FUN_00114a1c and FUN_00114c16.
Cluster of 1. [I].

#### 0x0011477E `shift2_map1c_term` — logic
Term = map at ((ctx+0x58)+4)+0x1C with x converted from param_2 (0x11482C) plus
FUN_001147b0, clamped by 0x114800. Called by 0x113F16/0x1132F4 state paths. Cluster of 1. [I].

#### 0x001147B0 `shift2_map18_term` — data-accessor
Map read at ((ctx+0x58)+4)+0x18 with x = global short 0x114830. Cluster of 1. [I].

#### 0x001147C8 `shift2_val_limit_select` — logic
Keeps param_2 while it does not exceed the vtable value (0x114838 on object 0x114834 with
param_3); otherwise returns the no-argument getter 0x114800. Mirror of 0x111CAE; called by
0x00112A30. Cluster of 1. [I].

#### 0x0011483C `shift2_prod10_calc` — logic
Product term for the state-13 ramp: mode 0x114A90[2] == 2 -> 0x3C-block (9x9 0x1120D4 ->
FUN_001153c6 with a global short) times the map at ((ctx+0x58)+4)+0x2C; else the maps at
((ctx+0x50)+4)+0x34 (set-indexed) and +0x3C; product divided by 10 via 0x114ABC.
Cluster of 1. [I].

#### 0x00114930 `shift2_mode_val9_get` — logic
State-9 value: mode 0x114A90[2] == 1 -> 9x9 0x112090 -> 0x22-block -> FUN_00115304; else the
map at ((ctx+0x58)+4)+8 (axis byte +0xE). Called by the entry 0x1132F4. Cluster of 1. [I].

#### 0x001149E0 `shift2_mode_map1c_get` — logic
Map read for the state-5 target: global 0x114AC8 == 1 -> ((ctx+0x50)+4)+0x1C (axes +0x16/+0xF),
else ((ctx+0x58)+4)+0xC (axis byte +0xF). Called by 0x113E88. Cluster of 1. [I].

#### 0x00114A1C `shift2_torque_term_calc` — logic
Torque-kind term gated by ctx+0x43: map from (ctx+0x64 block) slot +0xC (kinds 1/2), +0x10
(3/4) or +0x14 (5..8), axes (short +0x16, byte +0xF) from block 0x114AA0. Mirror of
0x111D14; called by the base sums 0x1144F0/0x114688. Cluster of 1. [I].

#### 0x00114AD0 `shift2_mode_val10_get` — logic
State-10 ramp source: mode 0x114D74[2] == 1 -> 9x9 0x112090 -> 0x22-block -> FUN_001152CC;
else the map at ((ctx+0x58)+4)+0x20 (x = global short 0x114D94). Called by 0x113F16.
Cluster of 1. [I].

#### 0x00114B80 `shift2_ramp_src_get` — logic
Map read: global 0x114D9C == 1 -> ((ctx+0x50)+4)+0x2C, else ((ctx+0x58)+4)+0x10, x = ctx+0x1A
(the tick). Used by the state-13 sum. Cluster of 1. [I].

#### 0x00114BAA `shift2_diffc_term_get` — logic
First refreshes the difference ctx+0x3C (vtable call 0x114DA0 = 0x11381E), then reads the map
((ctx+0x50)+4)+8 (global 0x114D9C == 1) or ((ctx+0x58)+4)+0x40, x = ctx+0x3C. Used by states
4/5/19. Cluster of 1. [I].

#### 0x00114BDC `shift2_kind4_term_get` — data-accessor
When the kind byte from block 0x114D74 (vtable 0x114DA8 with axes of 0x114DA4) equals 4,
returns the map at ((ctx+0x54)+4)+0x10 (x = global short 0x114DAC), else 0. Cluster of 2
(0x00114BDC slot +0x10, member 0x00114C16 slot +0x14). [I].

#### 0x00114C50 `shift2_diffc44_term_get` — logic
Refreshes ctx+0x3C (0x114DA0), then reads the map ((ctx+0x50)+4)+0x30 (global 0x114D9C == 1)
or ((ctx+0x58)+4)+0x44, x = ctx+0x3C. Called by 0x113F16/0x1132F4. Cluster of 1. [I].

#### 0x00114C82 `shift2_map0_term_get` — data-accessor
Map read at the first block of (ctx+0x50)+4 with x = global short 0x114DB0; part of the base
sum 0x1144F0. Cluster of 1. [I].
#### 0x00114C9A `shift2_mode_byte2_get` — data-accessor
Byte at ((ctx+0x50)+4 block)+0x10 when global 0x114D9C == 1, else at
((ctx+0x58)+4 block)+0x14. Called by the entry 0x1132F4. Cluster of 1. [I].

#### 0x00114CBE `shift2_interp_val_calc` — logic
Interpolated value with param_2 as x: mode 0x114D74[2] == 1 -> FUN_001151dc on the
(ctx+0x5C)+4 block, or (predicate 0x114DB8, cfg+3 == 1) the map at (0x114DBC block)+0xC with
the short 0x114D94; mode 2 -> 9x9 0x1120D4 -> 0x3C-block -> FUN_001153f6; other modes raise
detour 0x365/0xF/2/0xF/0x52 and return 0. Callers 0x113F16, 0x114FE0. Cluster of 1. [I].

#### 0x00114E26 `shift2_gate2f_set` — logic
Sets the gate ctx+0x2F: 0 when the global short 0x115090 < 1 or param_2 != 0; 1 when the short
behind ((ctx+0x50)+4)+0x24 exceeds the global DAT_00115094; otherwise keeps the old value.
Called at init (0x11299A) and in the state paths. Cluster of 1. [I].

#### 0x00114E54 `shift2_hold30_update` — logic
With gate ctx+0x2F == 1 and cfg+3 == 1, refreshes ctx+0x30 from the map at
((ctx+0x50)+4)+0x20 (x = short 0x115090); always returns the current ctx+0x30. Part of the
base sum 0x1144F0. Cluster of 1. [I].

#### 0x00114E86 `shift2_blend_update` — logic
State-9 blend: mode 0x11509C[2] == 1 takes the bytes at (ctx+0x5C)+4 slots +0x28/+0x2C
(FUN_001152ac/0x1152bc); otherwise two 0x3C-block reads (9x9 0x1120D4 -> FUN_00115346 /
FUN_00115388). Feeds both plus ctx+0x2C, param_2, ctx+0x1A and ctx+0x32 into the seven-argument
vtable call 0x1150B4 on object 0x11509C (ramp/blend object). Cluster of 1. [I].

#### 0x00114FE0 `shift2_state5_ramp_sum` — logic
State-5 sum (caller 0x113F16): x = vtable value of global 0x1150B8 through FUN_00114cbe, plus
the map ((ctx+0x50)+4)+4 (or (0x1150C8 block)+0x10 when the predicate holds) driven through the
ramp object at ctx+0x78 (vtable +0x20), plus param_2. Cluster of 1. [I].

#### 0x0011505A `shift2_holdgate_arm` — logic
Arms the max-hold gate ctx+0x42 = 1 when it is still clear and the global short 0x1150D0 is
below the short behind ((ctx+0x50)+4)+0x40. Called at the top of the base sum 0x1144F0.
Cluster of 1. [I].

#### 0x001150D4 `shift2_negclamp_3a` — logic
Anti-windup helper: with the vtable offset sVar1 (object 0x115358), when ctx+0x38 + param_2 +
sVar1 would go negative, stores the compensated value (0x115360 of -(param_2 + sVar1)) into
ctx+0x3A. Called by 0x113F16 state 10. Cluster of 1. [I].

#### 0x001150FE `shift_elem_satcnt_incr` — logic
Increments one of the four saturating signed counters at 0xFFF8E6F4-0xFFF8E6F7, selected by
the element code ctx+0x13 (1, 3, 4, 5; each counter saturates at -1/0xFF). These are the bytes
cleared by 0x0011210E - per-element hold/attempt counters of the second shift engine.
Cluster of 1. [O] mechanism, [I] meaning.

#### 0x0011516A `shift_elem_cnt_map_get` — logic
Reads the element's counter (FUN_0011518e by ctx+0x13) and interpolates the map at
((ctx+0x50)+4)+0x48 with (counter, global byte 0x115364) via 0x115368. Called by 0x112A54 - so
the cached ctx+0x24 value is counter-compensated. Cluster of 1. [I].

#### 0x0011518E `shift_elem_satcnt_get` — data-accessor
Returns the counter byte 0xFFF8E6F4/F5/F6/F7 selected by element code 1/3/4/5; 0xFF for other
codes. Companion of 0x001150FE. Cluster of 1. [O].

#### 0x001151CC `shift2_opt_block_byte8` — data-accessor
Guarded optional-block byte read at slot +8 (0 when the pointer equals the self-sentinel).
Cluster of 2 (0x001151CC called by 0x1136DE/0x112DB0, 0x00115398 called by 0x112A54).
[O].

#### 0x001151DC `shift2_opt_map_c_get` — data-accessor
Guarded map read at the set-indexed slot +0xC (set byte 0x11536C[2]), x = param_2, y = short
+0x16, via interpreter 0x115370. Called by 0x114CBE. Cluster of 1. [O].

#### 0x00115214 `shift2_opt_map_14_get` — data-accessor
Guarded map read at slot +0x14, x = param_2, y = global short 0x115374. Called by the progress
tracker 0x12142. Cluster of 1. [O].

#### 0x00115232 `shift2_opt_byte_18_get` — data-accessor
Guarded byte read at the set-indexed slot +0x18 with axes (+0x18, +0xA) via 0x115378. Called
by 0x112BFC/0x112FB4. Cluster of 1. [O].

#### 0x0011526A `shift2_opt_map_20_get` — data-accessor
Guarded map read at the set-indexed slot +0x20, x converted from param_2 (0x11537C), y = short
+0x16. Part of the base sum 0x1144F0. Cluster of 1. [O].

#### 0x001152AC `shift2_opt_byte_28_get` — data-accessor
Guarded byte read at slot +0x28. Cluster of 2 (0x001152AC slot +0x28, 0x001152BC slot +0x2C),
both called by the blend 0x114E86. [O].

#### 0x001152CC `shift2_opt_map_30_get` — data-accessor
Guarded map read at the set-indexed slot +0x30 with (short +0x16, short +8). Called by
0x114AD0. Cluster of 1. [O].

#### 0x00115304 `shift2_opt_map_38_get` — data-accessor
Guarded map read at slot +0x38 with (short +0x16, byte +0xF). Called by 0x114930.
Cluster of 1. [O].

#### 0x00115326 `shift2_opt_sbyte_0_get` — data-accessor
Guarded signed-byte read at the first block pointer (*param_1) via 0x115384 with global
0x115380, minus 0x80. Called by 0x113DAA. Cluster of 1. [O].

#### 0x001153A8 `shift2_opt_byte_c2_get` — data-accessor
Guarded byte read at slot +0xC via 0x1154E0 with global 0x1154DC. Called by 0x112BFC.
Cluster of 1. [O].

#### 0x001153C6 `shift2_opt_map_10_get` — data-accessor
Guarded map read at the set-indexed slot +0x10 (set byte 0x1154E4[2]), x = param_2 via
0x1154E8. Callers 0x11483C, 0x12142. Cluster of 1. [O].

#### 0x001153F6 `shift2_opt_map_18_get` — data-accessor
Guarded map read at the set-indexed slot +0x18, x = param_2, y = short +0x16, via 0x1154EC.
Called by 0x114CBE. Cluster of 1. [O].

#### 0x0011542E `shift2_opt_map_20b_get` — data-accessor
Guarded map read at the set-indexed slot +0x20, x converted from param_2 (0x1154F0), y = short
+0x16. Called by the base sum 0x114688. Cluster of 1. [O].

#### 0x00115470 `shift2_ctx28_map_get` — data-accessor
Guarded map read at the block pointer stored in ctx+0x28, x converted from param_2, y = global
short 0x1154F4. Called by 0x113F16. Cluster of 1. [O].
#### 0x0011549C `shift2_opt_map_30b_get` — data-accessor
Guarded map read at slot +0x30, x = param_2, y = global short 0x1154F4. Called by the progress
tracker 0x12142. Cluster of 1. [O].

#### 0x001154BA `shift2_opt_sbyte_2c_get` — data-accessor
Guarded signed-byte read at slot +0x2C via 0x1154E0 with global 0x1154F8, minus 0x80. Callers
0x1138FC, 0x12142. Cluster of 1. [O].

#### 0x001154FC `tbl_gearpair_9x9_c_get` — data-accessor
Third 9x9 (from,to) matrix accessor family (detour module 0x355): param_1 1/2/3/else selects
tables 0x115710/0x115714/0x115718/0x11571C, entry u32 at + col*4 + row*0x24. Callers
0x115B18, 0x115F6E. Cluster of 1. [O] mechanism, [I] roles.

#### 0x00115536 `tbl_gearpair_9x9_d_get` — data-accessor
Fourth 9x9 matrix accessor: param_1 1/2/else selects 0x115720/0x115724/0x115728, same layout.
Callers 0x11590E, 0x115B18. Cluster of 1. [O] mechanism, [I] roles.

#### 0x00115566 `gear_arbitration_check` — logic
Decision arbiter called from the outer shift module 0x000E941E. param_2 4 clears the reason
byte ctx+0x1A and returns a global flag; a disabled global or either inhibitor param forces
reason 2 (returning param_2 in {1,2}); global-object flags set reasons 5 or 6; predicate
0x11573C sets reason 8; with the deviation class of FUN_00115ea2 the request is granted
(reason 10) or denied (reason 9) using gear-pair values. Reason codes 0/2/5/6/8/9/10 in
ctx+0x1A. Cluster of 1. [I].

#### 0x0011577C `gear_arb_precond_eval` — logic
Precondition evaluator for the arbiter (callers = '-': entered via pointer table; it clears
ctx+0xE/+0xF which 0x00115566 turns into reasons 5/6). Converts the gear codes at
0x159BC[0x21]/[0x27] (0x159B4/0x159B8) and gates ctx+0x10 on the pointer pair [0x21] != [0x24];
then compares a mode value (mode 2: **(0x1159C0)+4; mode 1: (0x1159C8)+4+0xC) against the short
0x1159CC and sets ctx+0xE (mode 2, 0x1159D0 > 0) or ctx+0xF (mode 1) when below. Cluster of 1. [I].

#### 0x0011584C `mode_dir_match_check` — logic
Direction/mode consistency test used by the 0x164xxx-0x170xxx state machine: normalizes
param_3 (3 -> 2 or 1 depending on global 0x159D4 and its +0x10 short) and returns 1 when
(mode 1 and param_2 != 0) or (mode 2 and param_2 == 0). Cluster of 1. [I].

#### 0x00115884 `mode_code_set` — logic
Writes the mode byte ctx+0x12 (values 0..6) consumed by 0x11590E: param_3 == 3 -> 4 (or 6 when
global 0x159D8 set); otherwise the vtable conversion of param_2 on object 0x159B0; result 3 is
remapped to 5 while global 0x159E0 == 0x14. Cluster of 1. [I].

#### 0x001158C0 `speeddiff_cond_check` — logic
Returns 1 when the converted speed difference (ctx+0xA - global short 0x159E8, via 0x159EC)
reaches a mode-dependent threshold (global 0x159D8 == 0: short behind (0x1159E4)+4+0x28, else
FUN_00116242) and the global short 0x159F0 is negative. Callers 0x16C1E, 0x16C5E. Cluster of 1. [I].

#### 0x0011590E `ratio_factor_calc` — logic
Ratio/factor computation for the 0x164xxx-0x170xxx module (callers 0x16A64, 0x17032,
0x16DC4): builds a per-mille factor from the gear-index difference of 0x159BC[0x21]/[0x27]
(0x159FC conversions, x1000), refines it with the gear-pair 42-block (9x9 0x115536 ->
set-indexed block, map via 0x115C7C with axes +0x12/+0x18), multiplies and clamps to 1000,
then stores the result byte through 0x115C88 into ctx[0] (clearing global 0x115C8C) and, for
modes other than 4/6, ctx+0x18 = (short 0x115C90 x 100000)/result. Modes 0/1 read
FUN_0011626E, modes 2/3/5 the block path, 4/6 FUN_001160e2 into ctx+0x18; anything else raises
detour 0x355/0xF/0xB/3/0xFF. Cluster of 1. [I].

#### 0x00115A5E `ratio_factor_variant` — logic
Register-argument duplicate of the 0x11590E block math (same detour call 0x115C78, same
0x115C88 store and 0x115C94 conversion); callers = '-' - entered through a function pointer.
Cluster of 1. [I].

#### 0x00115B18 `gear_pair_val_interp` — logic
Gear-pair value with mode/state selection (callers 0x16C88, 0x1682C): (0x115C68[2] == 1)
state 1 -> 9x9 0x1154FC -> 0x1B-block -> FUN_00116250, other states the map at
((0x115CA4)+4)+0x3C; state 2 -> ((0x115CAC)+4)+0x2C; otherwise 9x9 0x115536 -> 0x2A-block ->
map at +8 (axis byte +0xF), interpolated by 0x115CB0. Cluster of 1. [I].

#### 0x00115C2E `mode_curve_val_get` — logic
Curve read by mode byte 0x115C68[2]: 3 -> ((0x115CB4)+4)+8, 1 -> ((0x115CA4)+4)+0x40,
2 -> ((0x115EE8)+4)+0x34, all with x = ctx+0xC; any other mode returns 1. Called by 0x17032.
Cluster of 1. [I].

#### 0x00115CD4 `ratio_settle_check` — logic
Settling check: takes v1 (mode 1: sign-selected block entry via 0x115EEC; else the short at
((0x115EE8)+4)+4) and v2 = global short 0x15EF4, both through the conversion 0x115F00
([I] absolute value); returns 0 only when |v2| < |v1| and FUN_001160b2 is false, else 1.
Callers 0x16AEA, 0x16C88. Cluster of 1. [I].

#### 0x00115D3E `clamp_press_20000` — library
Clamps param_2 into [0, 20000] (20000 = 0x4E20, the full-scale pressure used by the phase
engines); values above return the 20000 constant. Callers 0x17032, 0x16DC4. Cluster of 1. [O].

#### 0x00115D58 `scale_x1p28_conv` — library
Returns the converted value of (param_2 << 6)/50 = param_2 x 1.28 through the conversion
vtable 0x115F04. Callers 0x17032, 0x16DC4. Cluster of 1. [I].

#### 0x00115D6E `mode_short_16_set` — logic
Stores a mode/state-selected short into ctx+0x16: mode 3 -> map at (block)+0xC (axis +0xE) or
FUN_001161E6; mode 1 state 1 -> +0x10, other states +0x30; mode 2 state 2 -> +0xC, else +0x28
(shorts read directly from the family blocks). Called by 0x16DC4. Cluster of 1. [I].

#### 0x00115DE0 `mode_short_14_set` — logic
Twin of FUN_00115d6e storing into ctx+0x14: mode 3 -> map at +0x14 or FUN_00116204; mode 1
state 1 -> +0x14, else +0x2C; mode 2 state 2 -> +8, else +0x24. Called by 0x16DC4.
Cluster of 1. [I].

#### 0x00115E52 `offset_mill_calc` — logic
Subtracts a per-mille product from param_2 (through the conversion vtable 0x115F04): the
product axes come either from the global short 0x115F24 and the conversion of param_3, or
(enabled flag) from 0x115F1C[0x122] x short(+0x86) and a no-arg conversion. Callers = '-'
(pointer-table entry). Cluster of 1. [I].

#### 0x00115EA2 `geardev_class_get` — logic
Deviation classifier: diff = FUN_00115f28(ctx) - ctx[0]; band = byte at ((mode-1 block)+0x18
or +0x10); returns 0 within the band, 1 below it, 2 above. Called by the arbiter 0x115566.
Cluster of 1. [I].

#### 0x00115F28 `ratio_index_conv` — logic
Converts a ratio into an index byte cached at ctx+6: with global 0x11616C set it keeps the
cache; with ctx+8 == 0 it evaluates the curve DAT_00116178 at (byte 0x116174 + 0xFF);
otherwise value = (short at 0x11616C+0x10) x 100 / ctx+8 (via 0x116170) and, when non-negative,
the curve at (byte 0x116174 + value). Callers 0x115EA2, 0x115F6E. Cluster of 1. [I].

#### 0x00115F6E `sync_ctrl_step` — logic
Closed-loop synchronization controller step (callers 0x17032, 0x16DC4): mode 3 passes
ctx+0x18 through; otherwise computes the deviation of FUN_00115f28 from ctx[0] against a band
byte, applies the direction rate from FUN_00116122 (up/down), integrates
(rate x dev)/100 into ctx+0x18 (mode 1 negates) and clamps via 0x116170; in mode 1 a lock
condition (9x9 0x1154FC -> 0x1B-block -> FUN_001162a6, predicate 0x1161A4) arms the ramp
object at ctx+0x1C (0x1161A8, ctx+0x11 = 1); once armed the value comes from the ramp step
0x1161AC using block slots +0x38/+0x34. Cluster of 1. [I].

#### 0x001160B2 `mode1_map_le_check` — logic
Mode-1 comparison used by the settle check 0x115CD4: ctx+8 <= map value at ((0x116180)+4)+8
(axes short +0x16, short +8 of block 0x116190) via 0x1161B4; false for other modes.
Cluster of 1. [I].

#### 0x001160E2 `flag_map_1c_get` — logic
Flag-selected value: global 0x1161B8 == 0 -> map at ((0x1161BC)+4)+0x1C (set-indexed, axes
short 0x1161C0, byte 0x1161C4); else FUN_00116222 of the block with the short. Callers = '-'
(pointer-table entry; reached from 0x11590E modes 4/6). Cluster of 1. [I].

#### 0x00116122 `ctrl_rate_get` — data-accessor
Rate/gradient selector for the sync controller: by the flags 0x11617C[5]/[2] and the direction
param_2 (1/2) returns the short from one of eight block slots - (0x116180)+4: +0x20/+0x28
(flags clear) or +0x1C/+0x24 (set); (0x1162C4)+4: +0x18/+0x20; (0x116184)+4: +0x14/+0x1C.
Cluster of 1. [I].
#### 0x001161E6 `sync3_opt_read_10_get` — data-accessor
Guarded vtable read at block slot +0x10 with the global axis 0x1162C8 (interpreter 0x1162CC);
0 when the pointer is the self-sentinel. Cluster of 2 (0x001161E6 slot +0x10 called by
0x115D6E, member 0x00116204 slot +0x18 called by 0x115DE0). [O].

#### 0x00116222 `sync3_opt_map_24_get` — data-accessor
Guarded map read at block slot +0x24, x = param_2, y = byte 0x1162C8, via 0x1162D0. Called by
0x001160E2. Cluster of 1. [O].

#### 0x00116242 `sync3_opt_short_16_get` — data-accessor
Guarded short read: **(short**)(param+0x16) unless the pointer is the self-sentinel. Called by
the threshold check 0x1158C0. Cluster of 1. [O].

#### 0x00116250 `sync3_opt_byte_0_get` — data-accessor
Guarded byte read at the first block pointer (*param_1) via 0x1162D4 with global 0x1162C8.
Called by 0x115B18. Cluster of 1. [O].

#### 0x0011626E `sync3_opt_read_4_get` — data-accessor
Guarded read at the set-indexed slot +4 (set byte 0x1162D8[2]) with axes (+0xA, +0x18) via
0x1162DC. Called by 0x11590E. Cluster of 1. [O].

#### 0x001162A6 `sync3_opt_byte_c3_get` — data-accessor
Guarded byte read at block slot +0xC via 0x1162D4 with global 0x1162E0. Called by the sync
controller 0x115F6E. Cluster of 1. [O].

#### 0x001162E4 `kind_pair_test_a` — logic
Static predicate: returns 1 for the (param_1,param_2) pairs (1,0), (1,3), (3,0), (2,0), (2,4)
and (4,0) - i.e. kind 1/2/3/4 with specific sub-codes. Cluster of 1. [O].

#### 0x0011632E `gkind_pair_a_check` — logic
Applies FUN_001162e4 to the global kind pair at 0x11636C[0x2B]/[0x2C]; called directly by the
E-region dispatcher 0x000EC788. Cluster of 2 (0x0011632E uses the pair set A; member
0x00116356 tests the same globals against FUN_00116344, i.e. == (0,0)). [O].

#### 0x00116344 `pair_00_test` — logic
Returns 1 only when param_1 == 0 and param_2 == 0. Helper of the cluster above. Cluster of 1. [O].

#### 0x00116370 `tbl_gearpair_9x9_e_get` — data-accessor
Fifth 9x9 (from,to) matrix accessor: param_1 1/2/3/else selects 0x1165DC/0x1165E0/0x1165E4/
0x1165E8, entry at + col*4 + row*0x24. Used by the mode-1 block install 0x0011666E.
Cluster of 1. [O] mechanism, [I] roles.

#### 0x001163AA `tbl_gearpair_9x9_f_get` — data-accessor
Sixth 9x9 matrix accessor: param_1 1/2/else selects 0x1165EC/0x1165F0/0x1165F4, same layout.
Used by the block installer 0x00116422. Cluster of 1. [O] mechanism, [I] roles.

#### 0x001163F6 `shift3_svc_fields_call` — unknown
Calls one service-object method (PTR_FUN_0011660C on object 0x116608) on four context fields
(+0x8, +0x5, +0x9, +0xA) - the same timer/register pattern as 0x10FCA2/0x11295C. Called by
0x000E93FC. Cluster of 1. [I].

#### 0x00116422 `shift3_type_blocks_load` — logic
Calibration block installer of the third state engine (callers 0x00116724, 0x0016948,
0x00116DC4 and the E-region 0x000E940C): first installs the block for the kind byte
0x16610[0x22] (table 0x116614, 0..8 entries), resolves the kind via the vtable call on
0x0011662C/0x116628; param_2 == 1 -> FUN_0011666e installs two more lists (0..6 and 0x1B);
param_2 == 2 -> the 0..0x11 list from the 9x9-indexed read at 0x116634 and the 0..0x2A list
via FUN_001163aa (table index 1 when param_3 == 4 and the kind predicate 0x116630 is clear).
All lists are bounds-checked with the detour reporter 0x116624/0x1168A4. Cluster of 1. [I].

#### 0x0011653A `shift3_block_reload_tail` — logic
Register-argument tail helper (callers = '-'): invokes the range-check reporter
(PTR_FUN_00116624) with param_3/0x2A and re-stores the fallback block pointer from +4 - the
out-of-range repair path of the block installs above. Cluster of 1. [I].

#### 0x0011666E `shift3_blocks_mode1_load` — logic
Mode-1 continuation of the block installer (called by 0x00116422; parameters partly in
registers): installs the 0..6-entry list from the 9x9 matrix at 0x1168A8 (indices
[0x23]*4 + [0x29]*0x24 of block 0x116610) and the 0x1B-entry list via FUN_00116370, each with
detour bounds checks (0x1168A4). Cluster of 1. [I].

#### 0x00116724 `shift3_obj_init` — logic
Init/reset of the third state-engine object (caller 0x000E9404): clears ctx[7],[2],[9],[10],
[0],[0xF],[1] and the u16 pair at 0x1168C0, sets ctx[4] = 1 and the gate ctx+6 via
FUN_00116792; when the global mode 0x1168BC != 4 also installs the calibration blocks
(FUN_00116422 with global 0x1168C8), sets 0x1168C0[4] from a vtable flag and runs the vtable
call (0x1168D0, 0x1168D8, mode). Cluster of 1. [O] structure.

#### 0x00116792 `shift3_gate6_set` — logic
Sets ctx+6 = 1 only when the global mode 0x1168BC == 3, the byte 0x1168E0 < 3, the predicate
vtable 0x1168E8 fires, and the object behind 0x1168EC has a non-zero byte at +0x1C. Called by
0x00116724. Cluster of 1. [I].

#### 0x001167CC `shift3_obj_partial_reset` — logic
Selective reset driven from the E-region (caller 0x000E9446): clears ctx+4 always; zeroes the
u16 at 0x1168C0 (param_2), the u16 0x1168F0 (param_3) and the byte 0x1168F4 on request;
param_4 also clears 0x1168F8 and ctx+3; finally clears the gate ctx+6. Cluster of 1. [O].

#### 0x00116806 `shift3_state_done_check` — logic
Returns 1 when the engine state byte ctx+2 is 9 or 10 (the terminal states of the third
engine). Called by 0x000E9470. Cluster of 1. [O].

#### 0x00116818 `shift3_state8_flag_check` — logic
Returns 1 when the state ctx+2 == 8 and the global 0x1168CC == 1. Called by 0x000E947E.
Cluster of 1. [O].

#### 0x0011682C `shift3_phase_entry` — logic
Per-state entry of the third engine (callers 0x00116A64, 0x00116DC4): state 6 clears ctx[10],
ctx[9], sets ctx[0xF] = 1 and takes the vtable byte of object 0x1168D0 (or 0 when the global
0x1168FC is set); state 7 reads either the byte behind ((0x116908)+4) or the map at +4 (axes
+0xC/+0x19) converted by 0x116910; state 8 runs the vtable call on object 0x116B0C and sets
ctx[3] = ctx[0] = 1; states 9/10 clear ctx[3]; the value lands as u16 in ctx+0xC. Cluster of 1.
[O] structure, [I] names.

#### 0x00116948 `shift3_cycle_step` — logic
Cycle function of the third engine (callers = '-': entered via the pointer table from the
E-region): returns unless ctx+4 is set and the global mode 0x116B14[2] != 4; copies the short
0x116B28 -> 0x116B2C when the predicate 0x116B24 is clear; several flag combinations clear
ctx+9/ctx+10; when the kind byte (vtable 0x116B1C) changes from 0x116B40 it reloads the
calibration blocks (FUN_00116422) and updates the kind (vtable 0x116B44); finally runs the
stepper FUN_00116A64. Cluster of 1. [O] structure.

#### 0x00116A20 `shift3_abort_cond_set` — logic
Sets the abort flag ctx+0xE = 1 when the state ctx+2 < 9 and either timer predicate fires
(vtable 0x116B4C on ctx+0x11 or 0x116B50 on ctx+0x10); always clears ctx+0xE first. Callers =
'-' (pointer-table entry). Cluster of 1. [I].

#### 0x00116A64 `shift3_phase_step` — logic
Stepper of the third engine (caller 0x00116948): on the abort flag ctx+0xE it either walks the
sequence table 0x116B58 ([step*7 + type of block 0x116B54]) forward to the next entry equal to
state 8 and re-runs the entry FUN_0011682C, or clears the state to 0. Then it signals the
service object 0x116B18 with 0x55 and, while the done-check FUN_00116AEA reports complete,
advances the step counter ctx+7 (skipping entries equal to the current state), loads the new
state from the table, runs the entry and clears ctx+8. Cluster of 1. [O] structure, [I] names.

#### 0x00116AEA `shift3_phase_done_check` — dispatch
Per-state completion of the third engine (caller 0x00116A64): state 0 -> done; 1 -> global
0x116CEC or the vtable 0x116CF4; 2 -> flag combination 0x116CF8/(0x116CFC/0x116D00) or vtable;
3 -> 0x116CF8 or vtable; 4 -> 0x116D04 or vtable; 5 -> FUN_00116C1E; 6 -> FUN_00116C88;
7 -> FUN_00116C5E; 8 -> FUN_00116CDE; default not done; the vtable answers additionally
require the global 0x116D08. Cluster of 1. [O] structure.
#### 0x00116C1E `shift3_state5_done_check` — logic
State-5 completion of the third engine: with the gate ctx+6 set the vtable value of object
0x116D0C decides; otherwise done only when the predicate 0x116D14 fires and either 0x116D10
(arg 0) or 0x116D18 (object 0x116CF0) does. Cluster of 1. [I].

#### 0x00116C5E `shift3_state7_done_check` — logic
State-7 completion: not done while (tick byte ctx+8 < target short ctx+0xC) and (the vtable
0x116D18 predicate is clear or the gate ctx+6 is set). Cluster of 1. [I].

#### 0x00116C88 `shift3_state6_done_check` — logic
State-6 completion: immediate done when the flag trio (global 0x116D1C, 0x116D20[10]/[0xB])
is all clear, or when the vtable 0x116CF4 fires with global 0x116D08; otherwise compares the
vtable byte 0x116D24 against ctx+0xA (0x116D1C set) or ctx+9. Cluster of 1. [I].

#### 0x00116CDE `shift3_state8_done_check` — logic
Passthrough of the vtable value of object 0x116D28 - the state-8 completion of the third
engine (callee 0x000EB828 shows the outer module polls the same object). Cluster of 1. [O].

#### 0x00116DBE `shift3_abort5_set` — stub
Three instructions: writes the constant 5 into ctx+0xE (the abort-code field also set by
0x00116A20). Callers = '-' (pointer-table entry). Cluster of 1. [O].

#### 0x00116DC4 `shift3_ramp_pair_setup` — logic
State-8 entry body of the third engine (called by 0x0011682C): sets the mode byte pair,
resolves the kind through object 0x116EEC when the mode 0x116EE8[2] != 4, loads the tick
shorts from the (0x116EF0)+4 block (slots +0x28/+0x2C for kind 3, else +0x20/+0x24, second
negated into ctx+9), signals 0x54, computes the base and hold values (0x116F10/0x116F14,
0x116F18/0x116F1C with the ctx[0]/ctx[1] holds, extra map term for mode 5), evaluates two
ramp/slope values with the 10000-scale helpers 0x116F38/0x11719C (sign-flipped through
0x1171A0 when ctx+2 == 0, mirror kept in global 0x1171A4), gathers five object values
(0x1171A8..0x1171B4) and initializes the two ramp objects at ctx+0x16/ctx+0x18 with the
eight-argument calls 0x1171B8/0x1171BC; the final predicate (0x116EF0)+0x30 vs kind-1 sets
ctx+0x18 = block short x 300. Cluster of 1. [I].

#### 0x00117032 `shift3_state8_value_calc` — logic
State-8 value computation of the third engine (caller 0x00117184): signals 0x55 on object
0x1171C8, builds the low/high values (ctx+0x10/ctx+0x12 plus the vtable base 0x1171D8/0x1171D4)
through the 0x1171B0/0x1171B4 conversions; for the predicate kind the value comes from the
0x1171DC/0x1171E4/0x1171E8 chain (stored via 0x1171EC into ctx+0x30, global 0x1171F0 = /300);
the 10000-scale ratio 0x11719C lands in ctx+0x14 (negated when ctx+4 == 0); the ramp object at
ctx+0x16 is set (0x1171F4/0x1171F8) and scaled x50/64 into ctx+0xA; when the ramp result is
within the object minimum the global 0x1171FC is set; returns the difference clamped against
the ctx+0x10/ctx+0x12 window. Cluster of 1. [I].

#### 0x00117184 `shift3_state8_dispatch` — logic
Runs FUN_00117032(param, param_2) only when the global state 0x117200 == 8; otherwise returns
param_2 unchanged. Callers = '-' (pointer-table entry). Cluster of 1. [O].

#### 0x00117204 `tbl_gearpair_9x9_g_get` — data-accessor
Seventh 9x9 (from,to) matrix accessor: param_1 1/2/else selects 0x1174C8/0x1174CC/0x1174D0,
entry at + col*4 + row*0x24. Callers 0x117D30, 0x1176E2, 0x117DE6. Cluster of 1. [O] mechanism.

#### 0x00117234 `shiftmgr_gearflag_get` — data-accessor
Five instructions: returns the byte at (*DAT_001174D4)+0x10 - a gear/shift flag read shared by
the manager state update 0x00117622 and the engine-1 abort check 0x00110600. Cluster of 1. [O].

#### 0x00117240 `shiftmgr_obj_init` — logic
Constructor of the shift-manager object (callers = '-'): obtains the object from the service
vtable with id 0x148 when param_1 is null, initializes the two list heads (vtable 0x1174DC on
+0x34, 0x1174E0 on +0xB0), sets [2] = [1] = 6, runs FUN_00117CE4 and sets [0x1C] = [0x1D] = 1,
[0] = 1. Cluster of 1. [O] structure.

#### 0x00117292 `shiftmgr_variant_select` — logic
Selects the calibration variant of the manager: stores param_2 in ctx+3, re-initialises the
+0xB0 list (vtable 0x1174E4, resolved to the engine-2 family loader 0x00112410) and the +0x34
list (vtable 0x1174E8, resolved to the engine-1 family loader 0x0010F584 - spec 11 §2 variant
load), then fills ctx+0x134/0x138/0x13C from the three pointer tables 0x1174EC/0x1174F0/
0x1174F4 by param_2*4. Callers = '-' (pointer-table entry). Cluster of 1. [O].

#### 0x001172DA `shiftmgr_curve10_get` — data-accessor
Curve read (interpreter 0x1174FC with the global axis 0x1174F8) on the ((ctx+0x138)+4 block)
slot +0x10; called by 0x000EE4C8. Cluster of 3: 0x001173AE (called by 0x000EE734) reads slot
+0x24, 0x001173D4 (called by 0x000EE8B0) slot +0x2C. [O].

#### 0x001172F4 `shiftmgr_byte_c_get` — data-accessor
Byte at ((ctx+0x138)+4)+0xC; member 0x001173A0 (caller 0x000EE800) reads slot +0x20.
Cluster of 2. [O].

#### 0x00117302 `shiftmgr_pairval_x10_get` — logic
Gear-pair value: 9x9 matrix 0x117500 [param_3*4 + param_2*0x24] -> 0x11-entry block list
(set byte 0x117504, detour bounds check 0x117510) -> map at block+0x14 with param_4,
result x 10. Called by 0x000EE516. Cluster of 1. [O] mechanism.

#### 0x00117384 `shiftmgr_map18_get` — data-accessor
Map at ((ctx+0x138)+4)+0x18 with (param_2, global 0x1174F8) via 0x117514. Called by
0x000EE516. Cluster of 1. [O].

#### 0x001173C8 `shiftmgr_short_28_get` — data-accessor
Short at ((ctx+0x138)+4)+0x28; member 0x0011742C (caller 0x000EE0EC) reads the short at
((ctx+0x134)+4)+0xC. Cluster of 2. [O].

#### 0x001173EE `shiftmgr_mode_byte_get` — data-accessor
Byte from ((ctx+0x134)+4)+4 when the global 0x117518 == 1, else ((ctx+0x138)+4)+8. Callers
0x00117C48 and the engine-2 entry 0x001132F4. Cluster of 1. [O].

#### 0x00117412 `shiftmgr_short10_x10_get` — data-accessor
Conversion (0x11751C) of the short at ((ctx+0x134)+4)+0x10 multiplied by 10. Called by
0x000EE0EC. Cluster of 1. [O].

#### 0x00117438 `shiftmgr_mode_map_get` — logic
Manager value by mode byte ctx+7: 2 -> block at (*DAT_001174D4)+0x14, 4 -> +0x18, 6/8/9 ->
+0x1C, interpolated (0x11752C) with x = global 0x117520+1 and the vtable byte of object
0x117524+1; any other mode raises detour 0x369/0xF/0xB/4/0x36 and returns 0. Called by
0x000EDE82. Cluster of 1. [I].

#### 0x00117538 `shiftmgr_subsvc_register` — logic
Registers the service/timer calls for the manager's sub-objects: the object call (0x1773C,
ctx+0x22) plus the pair initialisers for the +0xB0 and +0x34 sub-objects (resolved to the
engine service helpers 0x0010FCA2/0x0011295C and library 0x000BAE5E). Called by 0x000EC0E0
(manager init). Cluster of 1. [I].

#### 0x0011755E `shiftmgr_sub_reset` — logic
Conditional reset propagated to the sub-engines (callers 0x000EB9EC/0x000EBC60/0x000EBB6C):
ctx+3 == 1 triggers the vtable call on 0x11774C; param_2 clears the engine-2 flag set
(ctx+0x31, +0x27 = 1, +0x1F, +6, +0x30); param_3 refreshes ctx+0x18 from the +0xB0 sub-object
and finishes it; param_4 refreshes ctx+0x1A from the +0x34 sub-object and finishes it
(resolved callees include the engine finish functions 0x0011294C/0x0010FBFC). Cluster of 1. [O].

#### 0x001175D6 `shiftmgr_mode_update` — logic
Manager mode byte update (caller 0x000EBDBC): value from the vtable (global 0x17764 == 4) or
ctx+0x31 into ctx+0x28; runs FUN_00117F48 with ctx+0x27 and FUN_001179D8(0); then a
floating-point evaluation FUN_00117AEC(ctx+0x12, ctx, ctx+0x10, 0) converted by 0x17770 into
ctx+0xA. Cluster of 1. [I].

#### 0x00117622 `shiftmgr_state_update` — logic
Manager state update (caller 0x000EBDBC): FUN_00118082 result into ctx+0x14, then
FUN_00117E98(0) and FUN_00117F28; when the gear flag 0x00117234 is 1 and mode
0x17774[2] == 2 with the predicate 0x17778 clear, sets the bool ctx+0x26 from
(global short 0x1777C <= map((0x117780)+0x20, ctx+1+1, ctx+2+1)); finally ctx+0x20 = 1 unless
the gear flag is set with the condition chain (mode != 2, 0x1778C[8], predicate 0x17790,
global 0x17794) and ctx+0x26 clear. Cluster of 1. [I].

#### 0x001176B6 `shiftmgr_val_e_set` — stub
Two instructions: ctx+0xE = global short 0x117FBC when the byte behind (0x117FB8)+0xAC is
clear, else (ctx+0xC byte) << 5. Called by 0x000EBDBC. Cluster of 1. [O].
#### 0x001176BA `shiftmgr_two_flags_set` — stub
Three instructions: stores param_2 into ctx+0xC3 and param_3 into ctx+0x3C - the two bytes the
reset 0x00117970 later feeds into its vtable pair. Called from the E-region request entry
points (0x000EB9EC/0x000EBA6E/0x000EBB6C/0x000EBC60/0x000EB900). Cluster of 1. [O].

#### 0x001176C4 `shiftmgr_request_set` — logic
Stores a shift request into the manager: param_2 -> ctx+2 (to gear), param_3 -> ctx+1 (from
gear), and the resolved kind byte from the vtable (0x117798, to, from) into ctx+5. Called from
the E-region 0x000EB9EC/0x000EBA6E/0x000EB900. Cluster of 1. [O].

#### 0x001176E0 `shiftmgr_pair_blocks_load` — logic
Installs the gear-pair calibration blocks of the manager by kind ctx+5: kinds 1/2 -> 0..6-entry
list from the 9x9 matrix 0x1177A4 (row ctx+2, col ctx+1) into the ctx+0x134 pair; kinds 3/4 ->
0..0x11 list from 0x1177A30 into ctx+0x138 plus the 0x2A list via FUN_00117204 with
gear-code-converted indices (0x117A3C) into ctx+0x13C; finally passes (kind, to, from) to both
sub-objects (vtables on +0xB0 and +0x34 - the engine-2 and engine-1 objects). Bounds errors go
to the detour reporter 0x117A2C. 0x001176E0 is the 1-instruction alias entry, 0x001176E2 the
body. Cluster of 2. [O].

#### 0x0011793E `shiftmgr_init_flags` — logic
Init-time flag setup (caller 0x000EDC5A): ctx+0x31 = ctx+0x28 = 1; ctx+0x27 = 1 when the
global 0x117A4C[6] is 0x1E or 0xA, else 0; then runs the engage engine FUN_00117F48 with both.
Cluster of 1. [O].

#### 0x00117970 `shiftmgr_obj_reset` — logic
Reset of the manager (caller 0x000ED9B2): clears ctx+0x22 (u16) and ctx[0], loads the global
short 0x117A50 into ctx+0x16, runs FUN_00117E98(1) and FUN_00117F28, clears ctx[7]; when the
global 0x117A34[0x2C] is 1 or 2 takes the vtable pair (0x117A58/0x117A5C on object 0x117A54
with ctx[0xC3]) into ctx+0x1C/+0x1D (else 0/0); clears ctx[0x30] and ctx[0x26]. Cluster of 1. [O].

#### 0x001179D8 `shiftmgr_ramp140_step` — logic
Steps the ramp/integrator object at ctx+0x140 with the negative target ctx+0x10: mode 0 also
adds the map value ((0x117A64)+0x28 block, axis 0x117A68) and stores the result into ctx+0x12;
other modes return the raw ramp value. Callers 0x001175D6, 0x000EDC5A. Cluster of 1. [I].

#### 0x00117A74 `shiftmgr_torque_est_get` — logic
Torque-style estimate: with the flags 0x117C88[0x127]/[0x129] clear the factors come from
[0x122]/[0x124] (negated for kind codes 2/4 at 0x117C8C[0x2C]/[0x2B]), otherwise both are 1;
returns conv((conv(ctx+1) x ushort(0x117C88+0x7C) x factor)/1000 - factor2 x param_2).
Called by the engage engine 0x00117F48. Cluster of 1. [I].

#### 0x00117AEC `shiftmgr_float_grad_calc` — logic
Floating-point gradient: normally (-param_3 x rate)/param_1 (param_1 is the float value of
ctx+0x12), falling back to the constant 0x117CA8 for zero or sign-mismatched inputs; the
alternate branch returns ushort(0x117CA4) x factor; result clamped to >= factor x 0x117CAC.
Called by 0x001175D6. Cluster of 1. [I].

#### 0x00117B4A `shiftmgr_mode_arbitrate` — logic
Runs the mode resolver FUN_001180BE, then (when ctx+3 == 1) either keeps the current ctx+8
(when the vtable byte 0x117CB8 is 0 or the param_2/condition chain with the global kind byte
and the map checks (0x117CBC)+0x158 / +0x154 holds) or maps a fresh vtable byte 0x117CD0 to
the mode codes 1->4, 2->6, else 2, latching them into ctx+7 with ctx+0x30 and ctx+8. Callers
0x000ED906, 0x000EBDBC. Cluster of 1. [I].

#### 0x00117C0C `shiftmgr_gate4_set` — logic
Sets the latch ctx+4 = 1 when the global 0x117CD4 != 4, ctx+0xB4 != 0 and the threshold pair
ctx+0x2E <= ctx+0xBA holds; param_2 0xA or 0x1E forces it as well. Called by 0x000EBDBC.
Cluster of 1. [O].

#### 0x00117C48 `shiftmgr_thresh_2e_calc` — logic
Curve read into ctx+0x2E (interp 0x117CDC with axis byte 0x117CD8[0xE]) from
**((ctx+0x134)+4) when 0x117CD8[2] == 1, else ((ctx+0x138)+4)+4; adds the mode byte
FUN_001173ee and converts (0x117CE0) into ctx+0x24. Called by 0x000EDC5A. Cluster of 1. [I].

#### 0x00117CE4 `shiftmgr_eng2_register` — logic
Registers the engine-2 sub-object: vtable call (0x117F70) with (ctx+0xB0, ctx) and stores the
self-pointer into ctx+0x6C - the self-link that all the "guarded optional block" readers in
this unit test for (empty list sentinel). Callers 0x00117240, 0x000ECB14. Cluster of 1. [O].

#### 0x00117D02 `shiftmgr_thr_cmp_check` — logic
Mode-dependent threshold comparison: global 0x117F78 == 1 -> NOT(short(((ctx+0x134)+4)+8) <=
ctx+0x14); otherwise short(**((ctx+0x138)+4)) < ctx+0x14. Called by 0x000EDE1C. Cluster of 1. [O].

#### 0x00117D30 `shiftmgr_pairval_ffff_get` — logic
Cross-engine gear-pair value (callers 0x00113F16, 0x001106DC, 0x001138FC - all three engines):
resolves the 0x2A-entry block via FUN_00117204 with gear-code conversions (object 0x117F7C
with ctx+7/ctx+5, 0x117F84 with ctx+1/ctx+2), reads the map at block+4 (axes +0xE, +0x18),
subtracts 0xFF, and when ctx+0xA is below the result converts the difference (0x117F9C) and
returns it clamped by 0x117FA0. Cluster of 1. [I].

#### 0x00117DE6 `shiftmgr_pairval_le_check` — logic
Same 0x2A-block resolution as FUN_00117D30, but returns the boolean
(map value - 0xFF) <= ctx+0xA while the gate ctx+0x20 is clear. Callers 0x00113C82, 0x001106DC,
0x000EC956. Cluster of 1. [I].

#### 0x00117E98 `shiftmgr_ratioband_update` — logic
Ratio-band update: ctx+0x2A = (ushort(0x117FA4+0x7C) x conv(ctx+1))/1000; then the band byte
ctx+0xC - for modes 1/3/4 with ctx+3 set it keeps the current band unless the byte
0x117FA4[0x82] is below it (param_2 != 1), otherwise it is recomputed from a second scaled
product (global 0x117FB0) and finally converted (>> 5) by 0x117FB4. Other modes keep ctx+0x2A
(converted the same way). Callers 0x00117970, 0x00117622. Cluster of 1. [I].

#### 0x00117F28 `shiftmgr_ratio_e_set` — logic
Stores the ratio word ctx+0xE: the global short 0x117FBC when the byte behind
(0x117FB8)+0xAC is clear, otherwise (band byte ctx+0xC) << 5 - the inverse of the >> 5 in
0x00117E98. Cluster of 1. [O].

#### 0x00117F48 `shiftmgr_engage_engine` — logic
Engage/release engine of the manager (callers 0x001175D6, 0x0011793E): takes the torque
estimate FUN_00117A74 (with the ratio word 0x117FBC) into ctx+0x10; param_2 resets (flag
ctx[0] = 0, mode ctx+0x2C = -1); an undefined mode resolves from the global 0x181DC (mode 3
when the estimate is positive, else ctx+5); the engaged flag ctx[0] is set when the estimate
crosses the converted threshold (modes 1/2 vs 3/4) or param_3 forces it; the tail clears the
globals 0x181E8/0x181F0 depending on the flag and the vtable value of object 0x181E0.
Cluster of 1. [I].

#### 0x00118082 `shiftmgr_speedcalc_get` — logic
Returns the converted difference (ushort(0x181F8+0x7E) - (ushort(+0x7C) x conv(ctx+2))/1000) -
param_2 while the global 0x181F4 is clear. Called by the state update 0x00117622. Cluster of 1. [I].

#### 0x001180BE `shiftmgr_mode_resolve` — dispatch
Table-driven mode resolver (caller 0x00117B4A; detour module 0x369): builds the candidate
ctx+8 from the vtable chain on object 0x181E0 (0x118204, overridden by 0x11820C/0x118210 for
the kinds 1/3 and param_2), evaluates the three predicates 0x118214/0x118218/0x11821C into
mode-specific booleans, gates on the kind set, then jumps through the switch tables
switchdataD_00118264/0x00118328 by the current mode ctx+7 (0..9) to the new mode (2..9,
with the predicate overrides 9/8/7/6/5/3); codes above 9 raise detour 0x369/0xF/0xB/0xD7,
unreachable ones 0x369/0xF/1/0xA8. Result stored into ctx+7. Cluster of 1. [O] structure, [I]
mode meanings.

#### 0x001183A2 `shift_speed_window_check` — logic
Standalone speed-window check for the F-region caller 0x000F0C18: with the global pair
0x11846C[0xCC]/[0xCD] not 2, evaluates a window of three speed comparisons (object pointers
+0x40/+0x3C/+0x44 vs the shorts at 0x11846C+0x94/+0x7C/+0x7E with mode [0xCA]) plus the extra
gate ([0x124] == -1, map 0x118478 below +0x7E, global 0x11847C clear) and sets the result flag
*param_1 = 1. Cluster of 1. [I].

## Unit summary

Region r10_app, 220 clusters, 232 named functions (0x0010FAAE-0x001183A2 plus duplicated
members at 0x001153xx/0x001173xx). The unit covers the **shift-execution back end** of the
application, organised as four cooperating engines plus a manager:

- **Engine 1 (0x10FAAE-0x111F7C, detour 0x367)** - the per-element **phase object** of spec 11
  §3 (stepper 0x10FB72, entry 0x10FFBE, done 0x1106DC, target 0x110CDC); spec cross-refs
  11-shift-execution.md §2/§3/§3b.
- **Engine 2 (0x112410-0x115470, detour 0x365)** - a second 19-state engine over the same
  design (stepper 0x112888, entry 0x1132F4, done 0x1138FC, target 0x113F16), feeding the
  element engine (0x112D16) and the converter module (0x112A08 <- 0xDE13A), plus the shift
  progress grade (0x12142) and per-element counters at 0xFFF8E6F4-F7.
- **Engine 3 (0x115566-0x1171B4, detour 0x355)** - a third state engine (states 0-10, 7-entry
  sequence table 0x116B58) with a closed-loop sync controller (0x115F6E) and ratio math
  (0x11590E); driven from the E-region dispatchers 0xE93xx-0xE94xx.
- **Shift manager (0x117176-0x1183A2, detour 0x369)** - owns the engine sub-objects
  (constructor 0x117240, variant select 0x117292, pair block load 0x1176E0/0x1176E2, engage
  engine 0x117F48, mode resolver 0x1180BE); its gear-pair values are read by all three
  engines (0x117D30/0x117DE6).

Kind counts: logic 149, data-accessor 66, dispatch 8, library 2, stub 4, unknown 3, entry 0,
irq 0. The many `*_opt_*` guarded readers implement optional calibration blocks: the block
pointer equals its owner (self-link, written by 0x117CE4) when the list is empty.

Unknown entries (honest): 0x0010FCA2, 0x0011295C, 0x001163F6 (service-object calls on context
fields - most likely timer start/read pairs, objects only reachable via pointer tables).

Cross-region leads:
- The 9x9 (from,to) gear-pair matrix pattern (stride 0x24) appears **seven times** in this unit
  (0x112090, 0x1120D4, 0x1154FC, 0x115536, 0x116370, 0x1163AA, 0x117204) - units covering the
  E-region dispatchers (0xEB348, 0xEB640, 0xEB9EC-0xEBC60, 0xEDC5A, 0xE9404-0xE947E,
  0xEE0EC-0xEE8B0) and 0xDE13A (converter, spec 09) should cross-check which global gear codes
  ([0x21]/[0x27], [0x23]/[0x29], ctx+1/ctx+2) feed them.
- Detour/event module ids per engine: 0x367 (element phases), 0x365 (engine 2), 0x355 (engine
  3), 0x369 (manager) - the fault-reporting spec (13) may want these ids.
- 0x001183A2 is called from F-region 0x000F0C18; 0x00112A08 feeds the converter module
  (0xDE13A); 0x000E7126/0x000EB640/0x000EB288 call into engine 1 helpers - units owning those
  callers should cross-link.
- All engines rely on the interpreter/lib layer 0xB6Dxx-0xB85xx (curve/map reads, ramps,
  conversions) - naming that layer would improve every later unit.
