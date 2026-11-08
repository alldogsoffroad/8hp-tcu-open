# Unit 41 (r10_app)

Region 0x12CA1E–0x134AD8: tail of the shift-program module (spec 09). Starts with the
manual/tip-mode request evaluator object (0x12C7xx–0x12CFxx, reached via virtual tables from
0x12123C/0x1212F4), then the situation handlers that pick shift programs for the gear decision
0x1277D8 (dispatched by 0x1208C0/0x120D34, spec 09 §2.3), the manual-mode (tip) shift limits
(0x12EEB0–0x130284, spec 09 §2.5), then a long run of tiny getters/setters (likely an inlined
accessor/VTable block at 0x131FF0–0x13224x), and ends past the shift module (0x1318D8 per spec
09) in the CAN-receive conditioning area (0x1348D6/0x134918, spec 07).

#### 0x0012CA1E `tip_clear_req_below_thresh` — logic
Manual/tip-mode helper of the 0x12C7xx block: reads the M-state input byte through the
pointer chain `*(PTR_DAT_0012ca60)+0x38`, and when it (as s16) is below RAM threshold
`PTR_DAT_0012ca84`, zeroes the RAM word at `PTR_DAT_0012ca88` (tip request output). Called from
the module cycle 0x12C710 after activation detect 0x12C83E. Cluster of 1. [I] (part of manual-mode request clearing)

#### 0x0012CA38 `tip_curve_lookup_adapter` — dispatch
Adapter that calls the function pointer at `PTR_FUN_0012ca90` with (`PTR_DAT_0012ca8c` = curve
data, the input byte from `*(obj+0x18)`) and returns its s16 result — a calibrated curve lookup
used by 0x12C83E's activation evaluation. Cluster of 1. [I]

#### 0x0012CA50 `tip_speed_below_limit_pred` — logic
Predicate: `param_2 < *(ushort *)(param_1 + 0x7e)` — compares a speed-like value against the
per-object limit at +0x7e; used by 0x12C9BC (tip deactivation check in the same block). Cluster
of 1. [I]

#### 0x0012CA94 `tip_set_state9a_remap1to2` — data-accessor
Stores `param_2` into `param_1+0x9a` remapping value 1 to 2. Called throughout the tip-mode
block (0x12CDBC, 0x12D072, 0x12D19E) right after clearing a module speed-delta global; the
field's exact meaning (mode or gear request) is open. Member 0x12EE4C is an identical copy used
by the tip-limits functions. Cluster of 2. [I]

#### 0x0012CAB0 `tip_fwd_call_ptr_ccac` — stub
3-instruction forwarding stub: tail-calls the function pointer variable `DAT_0012ccac`
(overridable hook) with its two arguments. Reached from module dispatch 0x1212F4. Cluster of
1. [O]

#### 0x0012CABC `tip_request_state_machine` — logic
Runs the tip-request phase machine on object `param_1`: state byte +0x43 (0/1/2), latch +0x44,
valid flag +0x38; enters state 1 when the lever/M input (via `PTR_DAT_0012ccb0` chain) passes
the gate, leaves on position mismatch, and publishes outputs to `param_2+0xb8/+0xb9`; marks
+0x45 with 0xfe/0xff (invalid) codes. Ends with an indirect notify. Called from 0x12123C.
Cluster of 1. [I]

#### 0x0012CBEA `tip_request_validate` — logic
Gate check before 0x12CABC's state update: requires flags at +0x44/+0x45, an input below
threshold (`*(obj+0x34) <= *PTR_DAT_0012ccb4`) and a gear-indexed map value (indirect call with
`pos-1`); on pass latches request global `PTR_DAT_0012ccd0` (state 1→2) and resets the request
state via 0x12D298. Clears the request when the two position bytes (+0x18/+0x19) differ and
writes the result to `param_1+0x46`. Cluster of 1. [I]

#### 0x0012CC86 `tip_get_flag_2c` — data-accessor
Getter: returns `**(char **)(*(PTR_DAT_0012ccb0) + 0x2c)` — one byte of the tip-module input
descriptor. Called by 0x1208B2. Cluster of 1. [O]

#### 0x0012CCA0 `tip_gate_sit0f_active` — logic
Bool test `(byte at DAT_0012ccd8+0x2f) & 2` — the gate predicate both situation dispatchers
(0x1208C0 case 0x0f and 0x120D34) call before invoking the manual-mode handler. Bit likely =
"manual mode selected". Cluster of 1. [I]

#### 0x0012CCDC `tip_sit0f_eval_request` — logic
Situation-0x0F (manual mode) handler of dispatcher 0x1208C0: checks `param_6 == 0x0f` and the
D-position special case, maintains the request global `PTR_DAT_0012cf7c` (state 1→2), resets
via 0x12D298, computes the per-gear tip up/down speed limits (0x12CFC0), writes
`param_2+0xb7` and hands the request to the gear-decision wrapper with the limits. Cluster of
1. [O] (situation code observable; spec 09 §2.3/§2.5)

#### 0x0012CDBC `tip_sit0f_apply_program` — logic
Manual-mode apply pass from the second dispatcher 0x120D34: gathers module state, runs the
gear-step limiter 0x12D072, and when a tip request is pending forces the shift-program set to
code 0x21 (`PTR_FUN_0012cfb4)(p2,0x21,0x21,0,0x21,0x21,0,1)`), syncs the current gear from the
module object and calls 0x12D19E (displayed-gear update) and 0x12CA94. Also touches the tip
limits 0x12EEB0 and 0x12F91C/0x12F4E4/0x12F67A/0x130300. Cluster of 1. [I] (0x21 = "same as
previous" program, spec 09 §2.3)

#### 0x0012CFC0 `tip_limits_lookup_by_gear` — logic
Computes the per-gear manual-mode speed limits into the tip object: up limit `+0x3a` and down
limit `+0x3c` from maps selected by gear (`gear-2`, `gear-1`; gear<2 → 0, gear>7 clamp), each
via 0x12D2C4 (7-map selector), then stores the differences from the current speed at `+0x3e`.
Gated on two input flags. Cluster of 1. [O] (spec 09 §2.5 tip limits)

#### 0x0012D072 `tip_gear_step_limiter` — logic
Limits a manual-mode gear request to single steps: requested `param_2` vs current `+0x42`;
differences beyond ±1 are clamped to current±1, aborts reset state (+0x34=0, request global),
holds gear when the up limit (`+0x1c <= +0x3e`) or down limit (`param_3 < +0x1a`) is violated,
and finally clamps the result to 1..8. Cluster of 1. [O]

#### 0x0012D19E `tip_displayed_gear_update` — logic
Updates the tip object's target/displayed gear `+0x41`: advances by ±1 from the current gear
when the tip direction flags allow (globals `PTR_DAT_0012d230[0x17]/[0x18]`), resets to
`+0x42` when `+0x22` is set (with 0x12CA94), clamps to 1..8 and publishes the result to global
`PTR_DAT_0012d344`. Called from 0x12CDBC. Cluster of 1. [I]

#### 0x0012D298 `tip_request_state_reset` — logic
Resets the tip request state: syncs current gear `+0x42` (and `+0x41` when `param_2==1`) from
the module object `PTR_DAT_0012d348+0x18`, clears `+0x2a/+0x30/+0x34`, then tail-notifies via
`PTR_FUN_0012d34c`. Called from 0x12CCDC and 0x12CBEA. Cluster of 1. [O]

#### 0x0012D2C4 `tip_map7_interp_select` — data-accessor
Selects one of seven map pointers stored at `param_1+0x10 … +0x28` by index 0..6 and
interpolates it with (param_3, param_4) through `PTR_FUN_0012d350`; index >6 reports fault code
0x76 via `PTR_FUN_0012d358` and returns 0. This is the "7 × 6 related offsets" reader of the
manual-mode group (spec 09 §2.5). Cluster of 1. [O]

#### 0x0012D35C `shift_sithandler_tbl_b` — logic
Situation handler (one of ~25 dispatched by 0x1208C0, spec 09 §2.3): when enabled flag
`PTR_DAT_0012d40c` is set, reads two program bytes from byte-table index 0xb (via the
0x121432-style reader) plus a third from the table, and calls the gear-decision wrapper
0x1277D8 with programs (p1,p2,p3,0x21) — a table-selected program situation. Sets the caller's
activity flag. Cluster of 1. [I]

#### 0x0012D420 `shift_sithandler_level_blend` — logic
Situation handler dispatched from 0x1208C0: the standard driver-type blend — reads the two
neighbouring driver-type levels from byte-table index 1 and builds programs
`level*5 + base` for both `param_7` and `param_8` bases, then calls the gear decision 0x1277D8
with the four programs (two levels × two bases). Exactly the §2.3/§2.4 blending scheme.
Cluster of 1. [O]

#### 0x0012D4D8 `shift_sithandler_fixed_prog` — logic
Situation handler from 0x1208C0: when enabled, takes a single fixed program number from
`**(PTR_DAT_0012d540)` and calls the gear decision 0x1277D8 with programs (p, 0x21, 0x21, 0x21)
and weights (0,1) — i.e. one fixed program overrides. Cluster of 1. [I]

#### 0x0012D54C `shift_sithandler_two_state_prog` — logic
Situation handler from 0x1208C0: reads a state byte (via `PTR_FUN_0012d5e0` getter+validity);
when non-zero selects program index 1 (clamped from value-1) from a byte table via 0x121432
and calls the gear decision 0x1277D8 with programs (p, p, 0x21, 0x21) — a two-entry
program switch by state. Cluster of 1. [I]

#### 0x0012D5F4 `shift_speed_below_map_pred` — logic
Predicate for 0x12106A: sets `*param_2 = 1` when a mode byte equals 1, gear
(`PTR_DAT_0012d6c4`) < 8 and the speed global `PTR_DAT_0012d6d0` is below the curve value
indexed `gear-1` (interpolated via 0x121482/0x121482-family) — [I] a "speed too low for this
gear" test used to gate a request. Cluster of 1. [I]

#### 0x0012D640 `shift_sithandler_global_prog` — logic
Situation handler from 0x1208C0: when the state byte from getter `PTR_FUN_0012d6dc` is
non-zero, takes the program from global `**(*(PTR_DAT_0012d6d4)+4)` and calls gear decision
0x1277D8 with programs (p, 0x21, 0x21, 0x21) weights (0,1). Same shape as 0x12D4D8 with a
different program source. Cluster of 1. [I]

#### 0x0012D6E8 `shift_sithandler_two_prog_blend` — logic
Situation handler from 0x1208C0: gated on module flags `PTR_DAT_0012d7b8[0x3b]/[0x22]`; reads
byte-table index 0x1f into a 3-byte stack, maps two of those bytes through table lookups
(`PTR_FUN_0012d7c8`, selecting table slot 0 or 1 by flag 0x22) and calls the gear decision
0x1277D8 with the two derived programs plus 0x21 fillers. Cluster of 1. [I]

#### 0x0012D7D4 `lockup_set_flag_plus4` — stub
Setter: `*(param_1 + 4) = param_2`. Called only from the lockup strategy 0x124570 (spec 09
§3.1) — writes a state/level byte of the lockup object. Cluster of 1. [O]

#### 0x0012D7DC `lockup_lvl_cap_below_speed` — logic
Lockup-strategy helper from 0x124570: for request levels 2 and 3 (param_2), when enable flag
`PTR_DAT_0012d830` is clear and a condition byte passes, if the speed global
`PTR_DAT_0012d83c` is below the curve value indexed `gear-1` (byte-table 0x121432 + curve
0x121482), the level is reduced to 1. [I] low-speed lockup cap. Cluster of 1. [I]
#### 0x0012D848 `range_req_obj_ctor` — logic
Constructor of the 0x10-byte range-request state object: zeroes the words, sets the request
word `[3]` to the invalid marker 0xffff, the two request-code bytes at +9/+0xa to 6 (a range
code in the 5–8 set, spec 14 §4.1) and clears the rest; allocates 0x10 bytes via the
allocator fn-ptr when param_1 is NULL. Caller 0x15D5C0 (init). Cluster of 1. [O]

#### 0x0012D882 `range_pair_state_step` — logic
Main step of the range pair evaluator (caller 0x126440): runs the P/N sequence machine
0x12DC2E, gates on the request-code pair (values 5/7 with the `+0xd`/`+0x2b` state bytes and
the global at PTR_DAT_0012da1c), reads the paired inputs through fn-ptrs, calls the hold-level
evaluator 0x12DBC6, steps the two saturating counters at param_1[0]/[1] and resets the object
when the pair goes inactive. 264 instructions. Cluster of 1. [I]

#### 0x0012DAE6 `range_rec_evt57f_slots1` — logic
Records a 10-value snapshot of a paired-evaluator struct: calls the recorder fn-ptr
(0xD5B32/0xD5A7C family) ten times with slots 1..10 and event id 0x57f, taking four s16 and
six u8 fields from `param_1`. Called from pair driver 0x1264C0 — freeze-frame for the range
safety check. Cluster of 1. [I]

#### 0x0012DB80 `range_val_below_limit_pred` — logic
Predicate of the range pair evaluator: returns 1 unless `*param_1 <= map limit` (byte chain
`PTR_DAT_0012dca0+0x50`) and the flag at `param_1+8` is clear — i.e. "condition no longer
below limit". Cluster of 1. [I]

#### 0x0012DB9E `range_limit_between_vals_pred` — logic
Returns 1 when 0x12DB80 reports the first value inactive AND the limit lies below the second
value `*(param_1+2)` — "threshold between the two paired values". Cluster of 1. [I]

#### 0x0012DBC6 `range_req_pn_compare` — logic
Compares the two range-request bytes (`PTR_DAT_0012dcb4`/`PTR_DAT_0012dcb8` with values 5 and
7 — P/N codes, spec 14 §4.1): returns the hold level at `+0xc` while the speed input stays
above the `+0xc`-chain limit, otherwise advances a saturating counter when the requests swap
5↔7 and reports takeover (`+0x8=1`). Cluster of 1. [I]

#### 0x0012DC2E `range_pn_seq_state_machine` — logic
Three-state machine (states 0/1/2 at `+4`) driven by the request byte at `+10` compared
against float constants (5.0/7.0 at DAT_0012dcbc/DAT_0012dcc0) and the paired object's request
byte; steps a counter at `+6` up to the chain limit and re-runs until the state settles.
Cluster of 1. [I]

#### 0x0012DD46 `range_pair_obj_ctor` — logic
Constructor of the paired-evaluator float object: allocates 0x28 bytes via the allocator
fn-ptr when param_1 is NULL, then initializes 10 words including vtable-like pointers
PTR_DAT_0012deac/DAT_0012dea8/DAT_0012deb0. Reached through a pointer table (no direct
callers). Cluster of 1. [O]

#### 0x0012DD94 `range_pair_debounce_step` — logic
Main step of the float debounce state machine: checks the paired request (float at
PTR_DAT_0012deb8+0x1c against const floats), timer/limit getters via fn-ptrs, and walks the
two ±1.0 counters `param_1[0]/[1]` up/down toward calibrated limits; resets on takeover,
calls 0x12E194 (post check) and publishes the mode to `param_1[6]`. Caller 0x1264D4.
Cluster of 1. [I]

#### 0x0012E05C `range_rec_evt57f_slots2` — logic
Second 0x57f snapshot recorder: same ten-record pattern as 0x12DAE6 but with slots 0xb..0x14,
then triggers fn-ptr(0x6d). Called from 0x12654E. Cluster of 1. [I]

#### 0x0012E0FC `range_fpred_limit_or_flag` — logic
Float predicate: returns 1.0f when the fn-ptr value from `~PTR_DAT_0012e18c +0x50` exceeds
counter `param_1[0]` or `param_1[4] == 1.0`. Used by 0x12E130 and 0x1264D4. Cluster of 1. [I]

#### 0x0012E130 `range_fpred_limit_high` — logic
Float predicate: when 0x12E0FC is not set, returns 1.0f if the same fn-ptr limit is below the
field at `param_1+4`. Cluster of 1. [I]

#### 0x0012E194 `range_debounce_post_check` — logic
Post-step of the debounce machine: compares the counter at `+0xc` with a fn-ptr limit, sets
the result flag `+0x10` (1.0), maintains the secondary counter `+0x20` (+1.0 steps) while two
float globals match, and clears it when the counter drops below 1.0. Cluster of 1. [I]

#### 0x0012E22C `range_state_walk_transitions` — logic
Walks a transition table (entries of three floats: current, next, next) at PTR_DAT_0012e34c:
increments the hold counter `+0xc` toward a limit, matches the state float `+8` against table
entries, follows the gated next state (flag at PTR_DAT_0012e344+8), latches the previous state
at `+0x14` and resets the counters on transition. Cluster of 1. [I]

#### 0x0012E372 `range_req3_state_counters` — logic
Three-state machine at `param_1+2` driven by request bytes 5/7 and 6/8 (P/R/N/D band codes,
spec 14 §4.1) on the pair objects: alternates states on condition changes and increments one
of two saturating counters (`param_1[0]`, `param_1[1]`, −1 = never) while resetting the
other. Caller 0x126440. Cluster of 1. [I]

#### 0x0012E482 `range_speed_above_val_pred` — logic
Predicate: the byte-chain value at `PTR_DAT_0012e4e8+0x2c` (as u16) is below the object field
`*(param_1+2)`. Cluster of 1. [O]

#### 0x0012E494 `range_speed_in_band_pred` — logic
Returns 1 when 0x12E482 is false and the same chained value is below `*param_1` — the speed
sits inside the object's band. Cluster of 1. [I]

#### 0x0012E500 `range_req3_state_machine_f` — logic
Float variant of the 3-state request machine: state `param_1[2]` advances 0→1→2 on two
condition sets built from the fn-ptr flag (`+0x28` chain) and request float compares
(PTR_DAT_0012e6b8+0x44 vs const floats), with two ±1.0 counters saturating at
DAT_0012e6d4. Caller 0x1264D4. Cluster of 1. [I]

#### 0x0012E658 `range_fpred_2c_field4` — logic
Float predicate: 1.0f when the fn-ptr value from `~PTR_DAT_0012e6b0 +0x2c` is below the field
at `param_1+4`. Cluster of 1. [O]

#### 0x0012E680 `range_fpred_2c_field0` — logic
Float predicate: when 0x12E658 is not set, 1.0f if the same `+0x2c` fn-ptr value is below
`*param_1`. Cluster of 1. [O]

#### 0x0012E6E4 `tip_map5_interp_select` — data-accessor
Selects one of five curve/map pointers (param 0..4 → offsets +4..+0x14 of object
PTR_DAT_0012e910) and interpolates it with (param_2, param_3); index >4 reports fault code
0x7a and falls back to slot 0. Same pattern as 0x12D2C4 but 5 maps. Cluster of 1. [O]

#### 0x0012E74C `tip_inhibit_word_combine` — logic
Combines the bitfield at `param_1+0x8c` with the mask at `+0x80` (only when the byte at
DAT_0012ee48 is 1) into the inhibit/status word at `+0x90` and returns it. Called by 0x12E7EE
and 0x12EDB0. Cluster of 1. [O]

#### 0x0012E750 `tip_fwd_gearreq_monitor` — stub
Forwarding stub: calls the fn-ptr DAT_0012e918 (resolved target 0x131968, tip gear-request
monitor step) with `param_1+0x4c`. Cluster of 1. [O]

#### 0x0012E75E `tip_fwd_gearreq_check` — stub
Forwarding stub: calls PTR_FUN_0012e920 (target 0x1319FE, gear-request completion check) with
(`param_1+0x4c`, param_2, param_3). Cluster of 1. [O]

#### 0x0012E768 `tip_fwd_gearreq_flags` — stub
Forwarding stub: calls DAT_0012e924 (target 0x131ACE, gear-request reached flags) with
`param_1+0x4c`. Cluster of 1. [O]

#### 0x0012E76E `tip_scale_speed_threshold` — logic
Computes a scaled speed threshold into `*param_5`: when the gate at `+0x84` is clear returns
0 (or 0xffff when param_4==1 = invalid), otherwise interpolates
`(param_2 * param_3 * 100) / 10000` through PTR_FUN_0012e928; special input 0xff with
param_4==1 yields 10000 (the "never" marker of spec 09 §3.1). Cluster of 1. [O]

#### 0x0012E7BC `tip_speed_cmp_global_pred` — logic
Band predicate against the global u16 `PTR_DAT_0012e92c`: mode 0 returns 1 when the global is
≥ param_3, mode 1 when it is < param_3. Cluster of 1. [O]
#### 0x0012E7EE `tip_limits_inhibit_eval` — logic
Evaluates the tip up/down inhibit speeds: for upshifts (param_2 < param_3) walks the combined
inhibit bit word (0x12E74C), maps each set bit via a byte table to a slot, interpolates the
5-map set (0x12E6E4) and stores the maximum at `+0xa4`; for downshifts walks the bit word at
`+0x94` and stores the minimum at `+0xa6`. Callers 0x127D3E/0x1284CE/0x127F66. Cluster of 1.
[O]

#### 0x0012E8E8 `tip_fwd_130b74` — stub
Forwarding stub: calls fn-ptr DAT_0012e938 (target 0x130B74) with `param_1+0x7c`. Cluster of
1. [O]

#### 0x0012E8F6 `tip_fwd_130ebc` — stub
Forwarding stub: calls fn-ptr DAT_0012e940 (target 0x130EBC) with (param_1, param_2).
Cluster of 1. [O]

#### 0x0012E8FC `tip_fwd_130f72` — stub
Forwarding stub: calls fn-ptr DAT_0012e944 (target 0x130F72) with (param_1, param_2).
Cluster of 1. [O]

#### 0x0012E94C `tip_gear_limit_eval_programs` — logic
Sweep over the tip-limit tables: for mode 0 walks 22 entries (PTR_DAT_0012ebfc), for mode 1
walks 14 (the shift-index columns, PTR_DAT_0012ec0c); per entry checks the enable bit
(0x12EDC8/0x12EDB0/0x12EDBE) and evaluates the four active program numbers (param_3/4/5/6,
with 0x21 = "same as previous", spec 09 §2.3) through the byte-table reader
(`PTR_FUN_0012ec08` on table +0x18/+0x1c), keeping the max (mode 0) or min (mode 1) per slot
into `+0x98..+0x9f` and `+0x85..+0x88`; 0xff values invalidate the set (`+0xa0=0`); bad mode
returns error 0x79. Cluster of 1. [I]

#### 0x0012EC10 `tip_obj54_reset` — logic
Clears the sub-object fields `+0x58/+0x59/+0x54` and calls the reset fn-ptr (target 0x131ECC)
on `param_1+0x54`. Caller 0x12887C. Cluster of 1. [O]

#### 0x0012EC34 `tip_get_limit_5e` — data-accessor
Getter: validity from fn-ptr (target 0x131C38) on `obj+0x54`, then selects the u16 at
`+0x5e` or `+0x60` by param_3 into `*param_2`. Twin of 0x12EC5A with different fields.
Cluster of 2 (with 0x12EC5A). [O]

#### 0x0012EC80 `tip_fwd_131bce` — stub
Forwarding stub: passes 7 arguments through fn-ptr DAT_0012ee20 (target 0x131BCE) to
`param_1+0x54`. Cluster of 1. [O]

#### 0x0012ECAC `tip_get_s16_62` — data-accessor
Calls the update fn-ptr (target 0x131ECC) on `obj+0x54` and returns the s16 at `+0x62`.
Cluster of 1. [O]

#### 0x0012ECBE `tip_get_s16_64` — data-accessor
Same as 0x12ECAC but returns the s16 at `+0x64`. Cluster of 1. [O]

#### 0x0012ECEC `tip_set_inhibit_bits_4_8` — logic
Sets inhibit bits 4 and 8 in the object byte `+0x8f` when two fn-ptr condition checks
(targets 0x130D40 family) return true. Caller 0x127F66. Cluster of 1. [O]

#### 0x0012ED3E `tip_set_inhibit_bits_94` — logic
Sets bit 4 of byte `+0x97` and bit 0x40000 of the inhibit word `+0x94` from two condition
checks (targets 0x130CE4/0x130E20/0x130B7A). Caller 0x127D3E. Cluster of 1. [O]

#### 0x0012EDA2 `tip_load_inhibit_mask` — data-accessor
Copies the mask word from `**(DAT_0012ee3c + 0x20)` into the object field `+0x80` (the
AND-mask used by the inhibit combine). Caller 0x128186. Cluster of 1. [O]

#### 0x0012EDB0 `tip_test_inhibit_bit` — logic
Bit test: `(combined_inhibit_word & param_2) != 0`, using the combine 0x12E74C. Cluster of
1. [O]

#### 0x0012EDBE `tip_test_word94_bit` — logic
Bit test against the object's inhibit word at `+0x94`. Cluster of 1. [O]

#### 0x0012EDC8 `tip_bit_to_index` — library
Converts a single-bit value (1,2,4,…) to its index 0..23; if no match within 24 shifts,
reports fault code 0x7a and returns index 0x17. Cluster of 1. [O]

#### 0x0012EDF8 `tip_inhibit_word_combine__2` — logic
Real body behind the 0x12E74C thunk: `word(+0x8c)` ANDed with mask `(+0x80)` when the byte at
DAT_0012ee48 is 1, stored to `+0x90`. Cluster partner of 0x12E74C. Cluster of 1 (separate
cluster entry; byte-identical pattern). [O]

#### 0x0012EE5C `tip_limits_state_reset` — logic
Resets the tip-limits object: syncs gear bytes from the module object
(`PTR_DAT_0012f04c[0x18]`), clears counters `[4]/[5]/[0x1e]/[0x1f]`, and with param_2==1 also
clears the module deltas (`+0x3e/+0x40/+0x42`) and re-latches the M-state via 0x12EE4C.
Called by 0x12D298 and 0x12FBBE. Cluster of 1. [O]

#### 0x0012EEB0 `tip_limits_main_eval` — logic
The manual-mode limits evaluator (spec 09 §2.5 anchor): reads current gear from the module
object, computes the feasible-gear floor (0x12F67A, which dispatches situation 0x0f), handles
up/down tip requests with multi-step walking (0x12FDBC/0x12FEB8 with the calibrated step
limit), latches the M-state (1→2 remap) into the module object `+0x3e`, evaluates
kickdown-style extra steps under `+0x22`, and finally clamps the result gear 1..8 into
`*param_13` and `+0x21`. 633 instructions; callees include 0x12FB02/0x12FB82/0x12FBBE/
0x12FD0E. Cluster of 1. [O] (structure) / [I] (details)

#### 0x0012F4E4 `tip_auto_step_state` — logic
Computes the per-gear limit pair (0x130144 indexed gear-2/gear-1) and runs the +0x11 state
byte: enters with an implicit upstep (`+1 = param_5+1`, request global `PTR_DAT_0012f7dc`
with 1→2 latch) when speed is below the `+0x1c` threshold and the gear conditions hold, exits
when the computed limit is reached or the fallback condition (param_6, bit from table +0x90)
fires. Caller 0x12CDBC. Cluster of 1. [I]

#### 0x0012F67A `tip_min_feasible_gear` — logic
Iterates gear candidates downward from param_3: for each candidate dispatches situation 0x0f
(`PTR_FUN_0012f804`(obj,0xf,…)), computes the two speed thresholds (0x13004C) and the
corrected limit, and stops when the speed is below the limit or the candidate reaches the
current gear / 1; writes the resulting floor gear (clamped ≤ 8) to `*param_8`. Called by
0x12CDBC and 0x12EEB0. Cluster of 1. [I]

#### 0x0012F80C `tip_m_input_gate_eval` — logic
Evaluates the M-input validity: fn-ptr predicates on the input object, invalid → marker 0xffff
and flag `+0xd`; clears `+0x22/+0xd` when deactivated; activates `+0x22/+0xe` when the speed
headroom (`uVar4 <= 0xffff - (obj+0x46)`) allows, latching the calibration byte at chain
`+0xc0` (1→2) into PTR_DAT_0012fa5c. Caller 0x12CABC. Cluster of 1. [I]

#### 0x0012F908 `tip_m_flags_clear_off_sit` — logic
When the situation code param_2 is neither 0x0f (manual) nor 0x30, clears the M-active flags
`+0x22/+0xd` of the tip object. Reached through the hook stub 0x12CAB0. Cluster of 1. [O]

#### 0x0012F91C `tip_request_pulse_publish` — logic
Reads the module up/down request pulses `[0x17]/[0x18]` and publishes a gear request into the
shift object (`+0x40`/`+0x42`, clearing the twin) using the calibrated gear values from chains
`+0xd8/+0xdc` (1→2 latch); with M active also latches the state into `+0x9a`. Caller
0x12CDBC. Cluster of 1. [I]

#### 0x0012FA06 `tip_map_by_sitcode_interp` — data-accessor
Selects a map/curve by situation code param_2 (0x19, 0x1a…0x22 → chain slots +0xc/+0x1c and
+0x2c…+0x8c of PTR_DAT_0012fc1c) and interpolates it at (gear−1, param_4); unknown codes
report fault 0x68 and fall back to slot +0x2c. Caller 0x12FEB8. Cluster of 1. [O]
#### 0x0012FB02 `tip_endstop_gear_latch` — logic
Gear end-stop handling inside the tip limits: with an up request at gear 8 (or down at gear 1)
clears the module delta via 0x12EE4C; at gear 8 with the M-input still valid latches the
calibration byte from chain `+0xb8` (1→2) into `PTR_DAT_0012fc3c` and sets `+0xc`. Caller
0x12EEB0. Cluster of 1. [I]

#### 0x0012FB82 `tip_step_dir_match` — logic
Sets the object flag `+4` when the direction of (gear − current, from fn-ptr compare vs
`PTR_DAT_0012fc40`) matches param_2 (+1/−1). Caller 0x12EEB0. Cluster of 1. [O]

#### 0x0012FBBE `tip_down_hold_state` — logic
Down-hold state machine (`+0x1e`): enters when the speed window (chain `+0xa0/+0xa4`, object
`[0x6e]`), gear ≥ 2 and M flags allow, holds the floor gear (current−1, min 1) in `+0x21`,
latches the request global (`PTR_DAT_0012fe6c`, 1→2), exits via the reset 0x12EE5C and
reports the flags to the caller. Caller 0x12EEB0. Cluster of 1. [I]

#### 0x0012FD0E `tip_up_hold_state` — logic
Up-hold twin (`+0x1f`): enters when the tip speed delta (`+0xa` ≤ `+2`) and state `+8==2`
allow, holds target gear+1 (max 8) in `+0x21`, latches `PTR_DAT_0012fe7c` (1→2), clears when
the target is reached or the speed falls below `PTR_DAT_0012fe78`. Caller 0x12EEB0. Cluster
of 1. [I]

#### 0x0012FDBC `tip_gear_step_apply` — logic
Applies a gear step in manual mode: +1 or +2 up (param_2==1) / −1 or −2 down (param_2==−1)
depending on the double-step latches (`PTR_DAT_0012fe80/0x88`, chain `+0xb0` allow, `+0xac`
double-step request), clamps the result into 2..8 (floor 1) and refreshes a state byte from
chain `+0x9c`. Caller 0x12EEB0. Cluster of 1. [I]

#### 0x0012FEB8 `tip_max_feasible_gear` — logic
Upward twin of 0x12F67A: walks gear candidates up from `+0x10`, dispatches situation 0x0f,
computes the two thresholds (0x13004C, plus the second map 0x12FA06 when param_8 set) and
stops at the highest gear whose limit still exceeds the speed; writes the ceiling gear
(floor 1) to `*param_7`. Caller 0x12EEB0. Cluster of 1. [I]

#### 0x0013004C `tip_map9_bilinear_lookup` — data-accessor
Selects one of the nine tip-limit maps by situation code param_2 (0x19…0x22 → 16-byte-stride
slots of the table behind `PTR_DAT_0012fc1c`) and bilinear-interpolates it (0x121A56) with
(param_3, param_4); unknown codes report fault 0x68. Spec 09 §2.5 "9 maps of the same
14-column format". Cluster of 1. [O]

#### 0x00130144 `tip_map9_curve_lookup` — data-accessor
Same 9-entry selection (slots +4…+0x84 of `PTR_DAT_00130284`) but interpolates the curve
pointer with one x value (0x121482 family); fault 0x68 on bad code. Caller 0x12F4E4. Cluster
of 1. [O]

#### 0x001301EC `tip_map9_get_scalar` — data-accessor
Returns the s16 scalar stored at slot +8…+0x88 of the same 9-entry table, selected by
situation code 0x19…0x22; fault 0x68 on bad code. Cluster of 1. [O]

#### 0x0013029A `tip_sit_resolve_get` — logic
Small wrapper: refreshes the tip situation code (0x13038C) and returns `*param_1`. Caller
0x12CDBC. Cluster of 1. [O]

#### 0x001302A8 `shift_sithandler_tip_mode` — logic
Known situation handler (spec 09 §2.3): stores param_4, resolves the tip situation code via
0x13038C, then calls the gear decision 0x1277D8 with programs (`*param_1`, 0x21, 0x21, 0x21)
and weights (0,1) — the manual-mode entry of the situation family, called from 0x12CCDC.
Cluster of 1. [O]

#### 0x00130300 `tip_target_gear_compare` — logic
Compares the tip target byte `+1` with the current gear `*param_4`: below → sets `+2`=1 and
`*param_3`=2 (with fn-ptr 0xFC0E2); equal → nothing; above with state `+0x11` → re-syncs the
target, otherwise the other fn-ptr (0xFC0CA) and `*param_3`=2. Caller 0x12CDBC. Cluster of
1. [I]

#### 0x00130348 `tip_flags_to_code2` — library
Combines two flag globals into 0…3: `(g1 != 0) + 2*(g2 != 0)`. Cluster of 1. [O]

#### 0x00130360 `tip_sitcode_valid` — library
Validates a tip situation code: returns 1 for 0x19…0x22 (the nine tip-map codes), else 0.
Cluster of 1. [O]

#### 0x0013038C `tip_situation_resolve` — logic
Resolves the active tip situation: flags→index (0x130348 → `[3]`), copies the selector byte
global into `[4]`, reads the situation code from a byte table indexed by `[3]`, and validates
it (0x130360); invalid → default code 0x1b and fault 0x93. Cluster of 1. [O]

#### 0x001303FC `tip_speed_guard_eval` — logic
Speed-guard helper for situation handler 0x1298C8: interpolates the limit (pedal `[0x6e]`,
offset `+0x82`) and, when the speed global exceeds `PTR_DAT_00130504`, latches the M-state
bytes of the module object `[0x23]/[0x24]` (1→2), tracks the max limit in `param_1[1]` and
produces two output speeds (`*param_1`/`*param_4` kickdown-style, `*param_5` ramp) via
fn-ptr interpolation. Cluster of 1. [I]

#### 0x00130518 `gearmat_phase_state_read` — logic
Phase helper of the gear-combination situation 0x1298C8: derives `*param_1` from the module
counter `[0x26]` (bit-complement form); on state 1 latches `**(obj+8)` into
`PTR_DAT_001305ac` and interpolates a value into `*param_2`; state 2 resets the counter to
0xff; state 3 reads the scalar path when `[0x25]` is clear. Cluster of 1. [I]

#### 0x001305B8 `gearmat_phase_speeds_setup` — logic
Fills the phase struct (target `+8`, context `+0xc`, speed `+0x10`, flags `[0x12]/[0x13]`),
derives the direction bit `[6]` by comparing two interpolations, and for pedal byte
`PTR_DAT_0013084c` < 7 runs the speed search 0x13068C, mapping the result to `*param_1` and
`*param_5` (invalid marker 0xff when the index or direction mismatches). Caller 0x1298C8.
Cluster of 1. [I]

#### 0x0013068C `gearmat_phase_speed_search` — logic
Walks the gear index down from 8 (or up from 0, by direction `[6]`) comparing the object
speed `+2` against the interpolated map values (chain `+8/+0xc`), stores the matching index
in `[0x16]`, computes a percent-scaled result into `+8` (100 fallback, byte-table factor from
chain `+0x18`), latches the state byte `PTR_DAT_00130868` (1→2) and falls back to 0x13080A
when the table result is 0. Cluster of 1. [I]

#### 0x0013080A `gearmat_phase_limit_pick` — logic
Picks the limit from the u16 array at `+0xc` indexed by `[0x12]`, adds the base speed `+0x10`
and clamps between the two default chains (`+0x28` / table `+0x24`); equal values set state
10; otherwise interpolates the difference. Caller 0x13068C. Cluster of 1. [I]

#### 0x001308C0 `gearmat_phase_hold_machine` — logic
Three-state hold machine (state at `+0xb`) driven by a speed window (chains `+0x18/+0x1c` vs
counter byte `PTR_DAT_00130adc`): state 0 arms when the counter leaves the window (latch
1→2 into module `[0x29]`), state 1 waits for the exit condition, state 2 computes the final
value via 0x1309C2/0x130A76 and returns 1 when non-zero; bad state reports fault 0x68.
Caller 0x1298C8. Cluster of 1. [I]

#### 0x001309C2 `gearmat_phase_result_calc` — logic
Computes the phase result: walks 9 map values (0x121A56) against `param[7]`, interpolates the
base value by (`[5]`, index), applies a percent factor from chain `+0x14` when ≥ 1, sets the
validity `[9]` and latches the state byte `PTR_DAT_00130ae8` (1→2); resets `[7]`. Caller
0x1308C0. Cluster of 1. [I]

#### 0x00130A76 `gearmat_phase_limit_pick2` — logic
Twin of 0x13080A using the default chains `+4`/`+8` of `PTR_DAT_00130ad8`: picks the limit
for `[5]`, adds base `+4`, clamps or interpolates the difference; equal → result 0. Caller
0x1308C0. Cluster of 1. [I]

#### 0x00130B18 `gearmat_phase_read_scalar` — logic
Gated on two enable globals: selects the u16 at `[0]` or `[1]` of the chained table
(`PTR_DAT_00130b6c`), interpolates it through the fn-ptr and latches the selector global
`PTR_DAT_00130b68` (1→2). Caller 0x1298C8. Cluster of 1. [I]

#### 0x00130B7A `tip_inhibit_cond_machine` — logic
Two-state condition machine (object `[1]/[2]`) for the tip inhibit path: gated on chain
`+0x18`, interpolates gear/pedal/speed limits (bilinear 0x121A56, curve 0x1218FA), and enters
state 1 when the gear window (current vs limit), pedal window and speed conditions line up;
exits restoring `PTR_DAT_00130d28` from chain `+0x14`; bad state → fault 0x68. Callers
0x12ED3E and 0x130CD8. Cluster of 1. [I]
#### 0x00130CD8 `tip_inh_cond_gate` — logic
Gate wrapper: runs the condition machine 0x130B7A only while the state byte at param_1 is 0.
Cluster of 1. [O]

#### 0x00130CE4 `tip_inh_gate_pred` — logic
Predicate: `*param_2 = 1` when the object's state `[2]` is 1 and the chain byte
(`PTR_DAT_00130d04+0x18`) is 1. Used by the inhibit setter 0x12ECEC. Cluster of 1. [O]

#### 0x00130D40 `tip_inh_up_eval` — logic
Up-inhibit evaluation (direction marker `*param_1 = 1`): checks the code gate 0x1317FC and
the change check 0x131744, runs the big bit assembler 0x131012 plus the extra bit 0x13140A,
combines `[0x22] = [0x29] & [0x27]` and reports the inhibit via `*param_4`, with a time limit
from the fn(+0x38) value (1→2 remap) against the counter `PTR_DAT_00130fc0`. Caller 0x12ECEC.
Cluster of 1. [I]

#### 0x00130E20 `tip_inh_down_eval` — logic
Down-inhibit twin (marker `*param_1 = 2`, extra bit into `[0x28]`, combined
`[0x21] = [0x28] & [0x26]`), same gate/assemble structure as 0x130D40. Caller 0x12ED3E.
Cluster of 1. [I]

#### 0x00130EBC `tip_inh_main_eval` — logic
Main inhibit pass (reached via stub 0x12E8F6): loads the module state, arms `+0x1e` on the
pedal/time conditions, marks invalid (0xff), resets via 0x130F72, runs the assembler
0x131012 and the bounds pair 0x1314B8/0x131660, and stores the fn(+0x78) result in `+0x4a`.
Cluster of 1. [I]

#### 0x00130F72 `tip_inh_state_reset` — logic
Full state reset of the inhibit object (bytes +0x19…+0x29 plus the invalid marker
`PTR_DAT_00130fc0=0xff`), gated on an active phase `[0x23]` and (param_2 set or a gear change
`[0x11] != [0x18]`). Cluster of 1. [O]

#### 0x00130FE0 `tip_inh_inputs_load` — logic
Snapshots the inhibit inputs into the object: bytes `+0x2a/+0x2b/+0x2c`, the u16 `+0x2e` and
byte `+0x30` from the module globals, clearing `+0x1d`. Reached via stub 0x12E902's family.
Cluster of 1. [O]

#### 0x00131012 `tip_inh_bits_assemble` — logic
408-instruction core of the tip inhibit: sets the direction state (`*param_1` 0/1/2, fault
0x68 otherwise), clamps the speed `+0x2e` and pedal `+0x30` by direction, then assembles the
two inhibit bit-sets `+0x27` (up) and `+0x26` (down) from about eight sources — tip-code map
lookups (fn +0x1c/+0x20, codes 0..3), pedal/gear changes, the hysteresis deltas `+0x32/
+0x34`, speed and pedal bands (bits 4–7) — and finally clears the opposite direction's set.
Cluster of 1. [I]

#### 0x0013140A `tip_inh_extra_bit_eval` — logic
Evaluates one extra inhibit bit from the two tip situation codes param_2/param_3: skipped
when either code is 0x21 ("same as previous"); by state `+0x1d` and the code range
(0x19 = first tip code) it maps code−0x18 through the chain-+0x18 (or +0x78) byte table.
Callers 0x130D40/0x130E20. Cluster of 1. [I]

#### 0x001314B8 `tip_inh_speed_bounds_calc` — logic
Builds the two pairs of min/max speed bounds: shifts the current speeds into `+2/+4` and
`+0x10/+0x12`, reads the two map pairs (chain +0x24/+0x2c), tracks min/max with hysteresis
(0x1315DE) and stores the results (`+0x36/+0x38`, `+0x3e/+0x40`) plus the deltas
(`+0x3a/+0x3c`, `+0x42/+0x44`). Cluster of 1. [I]

#### 0x001315DE `tip_inh_minmax_hysteresis` — logic
Min/max tracker with hysteresis: clamps the running min (`*param_6`) and max (`*param_7`) to
param_2, resets both on a gear change (`[0x18] != [0x11]`) or the `+0x1f` flag, otherwise
re-arms each bound only after the hysteresis margins param_4/param_5. Cluster of 1. [O]

#### 0x00131660 `tip_inh_speed_band_flags` — logic
Compares the computed bounds `+0x3a/+0x3c` and `+0x42/+0x44` against the interpolated limits
(chain +0x28/+0x30) and sets the out-of-band flags `+0x48`/`+0x49` and their OR `+0x46`.
Caller 0x130EBC. Cluster of 1. [O]

#### 0x00131744 `tip_inh_code_change_check` — logic
Detects a change between the two tip situation codes: on change (or class change) sets
`+0x1f`, clears `+0x1d` and rebuilds the bounds (0x1314B8); on stable codes with the band
flags and phase conditions sets the hold flag `+0x1d`. Callers 0x130D40/0x130E20. Cluster
of 1. [I]

#### 0x001317FC `tip_inh_gate_codes_differ` — logic
Skip-gate for the inhibit evaluators: returns 1 (skip) when either code is 0x21, or the codes
differ while both are below 0x19, or the code classes (map lookup via chain +0x78) differ.
Cluster of 1. [O]

#### 0x0013185A `tip_inh_get_s16_by_idx` — data-accessor
Switch getter: returns the s16 behind chain slots `+0x40…+0x58` (stride 4) selected by
param_2 = 1..7; 0 otherwise. Caller 0x131012. Cluster of 1. [O]

#### 0x001318D8 `tip_inh_get_u8_by_idx` — data-accessor
Byte twin of 0x13185A: chain slots `+0x5c…+0x74` selected by param_2 = 1..7. This is the
last function of the shift-program module per spec 09 §1.1 (`code 0xF7F50–0x1318D8`).
Cluster of 1. [O] (spec 09 §1.1)

#### 0x0013195C `tip_gearreq_get_mode` — data-accessor
Getter: returns `**(PTR_DAT_00131b70 + 0x0c)` — the gear-request mode byte of the request
object. Reached via stub 0x12E8D4's family. Cluster of 1. [O]

#### 0x00131968 `tip_gearreq_monitor_step` — logic
Steps the gear-request monitor: validity+byte from a fn-ptr getter into `[0]`, shifts the
current request into `[2]` and loads `[1]` from the global `PTR_DAT_00131b7c`; while active
(`[3]`) tracks rising/falling edges and sets the state to 2 on a rise, calling the abort
0x131B5A on a falling edge. Cluster of 1. [I]

#### 0x001319C0 `tip_gearreq_abort_gate` — logic
Gate for the request abort: when the mode (0x131B84 getter) is 1 and param_2 is at or above
`PTR_DAT_00131b88` (or mode≠1 and param_2 ≥ `PTR_DAT_00131b8c`), calls the abort 0x131B5A.
Reached via stub 0x12E756. Cluster of 1. [O]

#### 0x001319FE `tip_gearreq_complete_check` — logic
Completion check of a gear request: while active (`[0]`) and the requested gear `[1]` is
below the chain-+8 limit, arms `[3]` and latches the requested gear (table lookup
chain+0x04, index `[1]−1`, remapped 1→2) into `PTR_DAT_00131b98`; completes (`[4]`=0) when
the request+1 equals the current gear or the speed window (`0xffff −
PTR_DAT_00131b9c`) closes; output through `*param_4`. Reached via stub 0x12E75E. Cluster
of 1. [I]

#### 0x00131ACE `tip_gearreq_reached_flags` — logic
Compares the requested gear byte (1..8, from `PTR_FUN_00131ba0`) with the current request
global `PTR_DAT_00131b7c` and the speed pair: sets the reached flags `+5/+4/+6` when they
match and the speed window holds, clears them on mismatch; outputs the two flags. Reached
via stub 0x12E768. Cluster of 1. [I]

#### 0x00131B5A `tip_gearreq_abort` — logic
Abort helper: clears the request state `[3]` and, when the mode byte (chain +0xc) is 0, also
`[4]`. Called from 0x131968 and 0x1319C0. Cluster of 1. [O]

#### 0x00131BCE `tip_limits_pair_calc` — logic
Computes the two limit pairs of the tip-limits sub-object by direction param_4: mode 0 calls
0x131C54/0x131D70 into `+0xa/+6/+0x14`; mode 1 into `+0xc/+8` with the 0x131E7C variant when
param_7 is set; publishes the second result to `+0x1a`. Reached via stub 0x12EC80. Cluster
of 1. [I]

#### 0x00131BA8 `tip_gearreq_activate` — logic
Activates the gear-request monitor (`+4 = 1`), runs the update 0x131ECC and, when either
state byte `+0x18/+0x17` is set, raises the global `PTR_DAT_00131e00` and the flag `+5`.
Caller 0x12EC2E. Cluster of 1. [I]
#### 0x00131C38 `tip_obj_update_get_17` — data-accessor
Runs the tip-limits object update 0x131ECC and returns the validity byte at `+0x17`. Cluster
of 3: twin 0x131C46 returns `+0x18`; third copy 0x14E0B6 lives in the r14 measurement block
(calls its own update 0x14E0FC, returns `+0x11`). Cluster of 3. [O]

#### 0x00131C54 `tip_limit3_max_eval` — logic
Evaluates up to three limit maps selected by the object flags `+0x22/+0x1e/+0x26` (map pairs
at chain slots +0x08/+0x0c, +0x24…+0x30, +0x34/+0x38 by selector `PTR_DAT_00131e08`), takes
the maximum and returns it. Caller 0x131BCE. Cluster of 1. [I]

#### 0x00131D70 `tip_limit_scaled_eval` — logic
Computes a scaled speed limit: three interpolations (chain +0x18/+0x1c/+0x20) with a percent
factor (`sVar5 * 100 * factor / 10000` via the byte at `+0x16`), plus the optional pedal/gear
map from slot +0x10/+0x14 (default 10000 = never), summed and converted through the 0x1216E4
family. Callers 0x131BCE/0x131E7C. Cluster of 1. [I]

#### 0x00131E7C `tip_limit_offset_eval` — logic
Wrapper of 0x131D70: when the current gear byte is below `PTR_DAT_00131fc0`, evaluates at
gear−1 and subtracts the offset `**(chain+4)` (floor 0); otherwise evaluates directly. Caller
0x131BCE. Cluster of 1. [O]

#### 0x00131ECC `tip_limits_obj_update` — logic
Update pass of the tip-limits sub-object: when active (`+4==1`) evaluates the three input
validities (fn-ptr checks on `+0x20/+2/+0x1c/+0x24` into `+0x22/+1/+0x18/+0x1e/+0x26`),
combines them into the overall validity `+0x17` and keeps the max input u16 at `+0xe`;
otherwise clears all fields back to the 10000 "never" defaults. Cluster of 1. [O]

#### 0x00131FD8 `init_modules_via_ptrs` — logic
No direct callers (init-table reach): invokes three init cascades through fn-ptrs — 0x14F34C
(which constructs/registers nearly every app module), 0x14F850 and 0x14FC5C. Part of the
application start-up sequence. Kind listed as logic; effectively an init stage. Cluster of
1. [O]

#### 0x00131FEE `mod_setter_pair_call` — stub
Generated module-I/O accessor: calls one setter fn-ptr on `PTR_DAT_00132220` and one on
`PTR_DAT_00132228` (targets 0x1513F8 + 0x14F7A0). No direct callers (reached from the
setter sweep, cf. 0xF7950, spec 09 §1). Cluster of 3 (copies 0x132024 → 0x151782/0x14F7C2
and 0x13209A → 0x151C8C/0x14F848). [O]

#### 0x00131FFE `mod_setter_single_call` — stub
Single setter fn-ptr call on `PTR_DAT_00132220` (target 0x151658). Cluster of 4 (0x13206A →
0x151C86, 0x13207A → 0x151C88, 0x13208A → 0x151C8A). [O]

#### 0x0013200E `mod_setter_pair2_call` — stub
Two setter fn-ptr calls (targets 0x151754 + 0x14F7AA), second with the fn-ptr itself as
argument. Cluster of 2 (0x132034 → 0x14F7CC/0x151784). [O]

#### 0x0013204A `mod_setter_pair3_call` — stub
Two setter calls (targets 0x1517E0 + 0x14F7EA) on the same two objects. Cluster of 1. [O]

#### 0x0013205A `mod_setter_pair4_call` — stub
Two setter calls (targets 0x151C3E + 0x14F784). Cluster of 1. [O]

#### 0x001320BA `mod_set_byte98_1` — stub
Writes the constant 1 into the module I/O slot `DAT_00132298`. Cluster of 2: 0x1320BC is the
identical copy actually called from boot init 0x68EA8. [O]

#### 0x001320C4 `mod_set_byte98_2` — stub
Writes the constant 2 into `DAT_00132298`. Called from 0x68EA8. Cluster of 1. [O]

#### 0x001320C6 `mod_set_byte98_r0` — stub
Pass-through setter: writes the incoming register value (r0) into `DAT_00132298`. Cluster of
1. [O]

#### 0x001320CC `mod_set_byte98_3` — stub
Writes the constant 3 into `DAT_00132298`. Called from 0x68EA8. Cluster of 1. [O]

#### 0x001320D4 `mod_get_byte94` — data-accessor
Getter: returns the module I/O byte at `DAT_00132294`. Caller 0x68EFE (boot init). Cluster
of 1. [O]

#### 0x001320DC `mod_get_byte9c` — data-accessor
Getter: returns the byte at `DAT_0013229c`. Callers 0xF7298 and 0xB912C — the module input
copiers (cf. spec 09 §3.2: 0xB912C copies about 70 inputs into the converter block). Cluster
of 1. [O]

#### 0x001320E4 `mod_get_byte_a0` — data-accessor
Getter through the pointer slot `PTR_DAT_001322a0`; caller 0xF7298. Cluster of 1. [O]

#### 0x001320EC `mod_get_byte_a4` — data-accessor
Getter through `PTR_DAT_001322a4`; caller 0xF7298. Cluster of 1. [O]

#### 0x001320F4 `mod_get_byte_a8` — data-accessor
Getter through `PTR_DAT_001322a8`; caller 0xF7298. Cluster of 1. [O]

#### 0x001320FC `mod_get_u16_fn` — data-accessor
Getter that computes: calls the fn-ptr `DAT_001322b0` (target 0x13854A) with
`PTR_DAT_001322ac` and returns the u16. Callers 0x75EC6/0x75EFC/0x81A20/0x75F38/0x75BE6
(diagnostic area). Cluster of 1. [O]

#### 0x0013210A `mod_get_s16_b4` — data-accessor
Getter: returns the s16 behind `PTR_DAT_001322b4`; caller 0xF7298. Cluster of 1. [O]

#### 0x00132110 `mod_get_byte_b8` — data-accessor
Getter: byte behind `PTR_DAT_001322b8`; caller 0xF7298. Cluster of 1. [O]

#### 0x00132118 `mod_get_s16_bc` — data-accessor
Getter: s16 behind `PTR_DAT_001322bc`; caller 0xF7298. Cluster of 1. [O]

#### 0x0013211E `mod_get_byte_c0` — data-accessor
Getter: byte behind `PTR_DAT_001322c0`; caller 0xF7298. Cluster of 1. [O]
#### 0x00132126 `mod_get_s16_c4` — data-accessor
Getter: s16 behind `PTR_DAT_001322c4`; caller 0xF7298 (module input copy). Cluster of 1.
[O]

#### 0x0013212C `mod_get_byte_c8` — data-accessor
Getter: byte behind `PTR_DAT_001322c8`; caller 0xF7298. Cluster of 1. [O]

#### 0x00132134 `mod_get_s16_cc` — data-accessor
Getter: s16 behind `PTR_DAT_001322cc`; caller 0xF7298. Cluster of 1. [O]

#### 0x0013213A `mod_get_byte_d0` — data-accessor
Getter: byte behind `PTR_DAT_001322d0`; caller 0xF7298. Cluster of 1. [O]

#### 0x00132142 `diag_call_144108_3arg` — stub
Boundary wrapper: passes (u16, p2, p3) through the fn-ptr (target 0x144108). Callers
0x74532/0x80E08/0x6CBDA in the boot/diagnostic region. Cluster of 1. [O]

#### 0x00132156 `diag_call_147bec` — stub
Boundary wrapper: (u16, p2) through the fn-ptr (target 0x147BEC); caller 0x81804. Cluster of
1. [O]

#### 0x00132168 `diag_call_14ebf0` — stub
No direct callers (pointer-table reach): calls the fn-ptr (target 0x14EBF0) with
`PTR_FUN_001322e0`. Cluster of 2 (twin 0x132176 → target 0x14EC10). [O]

#### 0x00132184 `diag_call_14ec52` — stub
No direct callers: fn-ptr call (target 0x14EC52) with `PTR_FUN_001322e0`, returning u16.
Cluster of 1. [O]

#### 0x00132192 `diag_call_14ec64` — stub
Fn-ptr call (target 0x14EC64) with one argument; caller 0x139CE4. Cluster of 1. [O]

#### 0x0013219C `diag_call_139b9c` — stub
Fn-ptr call (target 0x139B9C) with one argument; callers 0x134F0C and 0x7A0EA. Cluster of 1.
[O]

#### 0x001321AC `diag_call_139c40` — stub
Fn-ptr call (target 0x139C40) with one argument; caller 0x7A3DC. Cluster of 1. [O]

#### 0x001321BC `diag_call_139df4` — stub
Fn-ptr call (target 0x139DF4) with one argument, u16 result; callers 0x7A26E/0x7A3DC.
Cluster of 1. [O]

#### 0x001321CC `diag_call_139ce4_2arg` — stub
Fn-ptr call (target 0x139CE4) with two arguments; caller 0x7A130. Cluster of 1. [O]

#### 0x001321DE `diag_call_139e98_i7` — stub
Fn-ptr call (target 0x139E98) with the constant 7; no direct callers (table reach). Cluster
of 1. [O]

#### 0x001321EE `diag_call_139e98_i3` — stub
Fn-ptr call (target 0x139E98) with the constant 3; caller 0x74B0C. Cluster of 1. [O]

#### 0x001321FE `diag_call_139e98_i8` — stub
Fn-ptr call (target 0x139E98) with the constant 8; caller 0x74B0C. Cluster of 1. [O]

#### 0x00132206 `lib_identity_r0` — stub
Identity: returns the incoming r0 value unchanged (no-op adapter, e.g. default handler in a
function-pointer table). Cluster of 1. [O]

#### 0x0013220E `diag_call_139e98_i12` — stub
Fn-ptr call (target 0x139E98) with the constant 0x0c; caller 0x74B0C. Cluster of 1. [O]

#### 0x0013230C `diag_set_idx7` — stub
Setter through the fn-ptr on `PTR_DAT_001323bc` with fixed index 7 and the argument; callers
0x76588/0x7665E. Cluster of 1. [O]

#### 0x00132316 `diag_set_idx12` — stub
Same setter pattern with fixed index 0x0c; callers 0x74B0C/0x7634E. Cluster of 1. [O]

#### 0x00132320 `diag_get_13a148` — stub
Getter through the fn-ptr (target 0x13A148) on `PTR_DAT_001323bc`; caller 0xB912C (converter
input copier, spec 09 §3.2). Cluster of 1. [O]

#### 0x0013232E `mod_get_dat23c8` — data-accessor
Getter: returns the byte at `DAT_001323c8`; caller 0x134D68. Cluster of 1. [O]

#### 0x00132340 `diag_call_get_14dc70` — stub
Calls the fn-ptr (target 0x14DC70) on `PTR_DAT_001323cc` and returns the byte behind
`PTR_DAT_001323d4`; caller 0x7B686. Cluster of 1. [O]

#### 0x00132350 `diag_call_get_14dca0` — stub
Twin of 0x132340 with target 0x14DCA0 and word at `DAT_001323dc`; caller 0x7B686. Cluster of
1. [O]

#### 0x00132360 `diag_get_14e0b6` — stub
Fn-ptr call (target 0x14E0B6 — the r14 twin of this unit's 0x131C38 accessor) on
`PTR_DAT_001323cc`; callers 0xB90B2 and 0xF70B4. Cluster of 1. [O]
#### 0x0013236E `diag_call_13a9d4` — stub
Boundary wrapper: one argument through the fn-ptr (target 0x13A9D4) with
`PTR_DAT_001323e4`; caller 0xB90B2 (converter input copier area). Cluster of 1. [O]

#### 0x0013238C `meas_reg_block_1` — logic
Measurement-protocol registration block (spec 10): reads two source bytes through fn-ptrs,
and when the state global `PTR_DAT_001323f4` is clear, registers about twenty data pointers
via the registrar 0x151DC0(label, selector) into the handle globals `PTR_DAT_00132630…`.
Called from the init cascade 0x14F34C and 0x15A67C. Cluster of 1. [O]

#### 0x001324A2 `meas_reg_block_2` — logic
Second registration block, invoked as a callback with the registrar in r12: registers ~40
(label, selector) pairs into `PTR_DAT_00132690…` and also obtains a handle via 0x134380. No
direct callers. Cluster of 1. [O]

#### 0x00132526 `meas_reg_block_3` — logic
Third registration block (registrar in r12): registers a large set of measurement entries,
using the handle helpers 0x134508/0x13451A/0x134554. No direct callers. Cluster of 1. [O]

#### 0x00132980 `app_tasklist_dispatch` — dispatch
Table-driven invoker: walks a list of (fn, arg) pairs at `0x132B14…` calling each entry —
the resolved callee list (~90 functions across 0x135xxx–0x14Exxx) is effectively the
application's task/init call list. No direct callers (pointer-table reach). Kind dispatch.
Cluster of 1. [I]

#### 0x00132B8A `unknown_baddata_132b8a` — unknown
Listed as an entry of the 0x132980 call table, but Ghidra reports bad instruction data and
the decompiled body is nonsense — likely a table entry the analyzer cannot disassemble, not
a real function. Honest unknown. Cluster of 1. [O]

#### 0x00132D8E `diag_tasklist_dispatch` — dispatch
Companion table walker (493 instrs, no direct callers): invokes a list of functions with one
argument each; the resolved callees concentrate in the boot/diagnostic region
(0x76xxx–0x77xxx) plus app-side helpers. Kind dispatch. Cluster of 1. [I]

#### 0x00133476 `meas_snapshot_collect_1` — logic
Measurement snapshot collector (spec 10): calls a series of getter fn-ptrs (CAN-receive
getters at 0xA39xx/0xA3Bxx and 0x77xxx) and stores the returned bytes/words into the
snapshot struct; also invokes the sub-collector 0x133740. No direct callers (callback reach).
Cluster of 1. [O]

#### 0x00133740 `meas_snapshot_collect_2` — logic
Sub-collector of 0x133476: fills the struct range +0x12…+0x26+ from getter fn-ptrs
(0x77xxx/0x7Cxxx and 0x137668/0x140A0A). Cluster of 1. [O]

#### 0x00133946 `meas_snapshot_collect_3` — logic
Another snapshot collector: gathers bytes/words from 0x76Dxx/0xA46xx/0x7C1xx getters into
the struct (+0x14…+0x46) and runs the small float collectors 0x1345CE/0x13452C/0x1345F4.
Cluster of 1. [O]

#### 0x00133C4E `meas_snapshot_collect_4` — logic
The largest collector (576 instrs): fills the struct from +0x104 upward using getter pairs
(0x77xxx, 0x7BFxx, 0x7B0xx, 0xA46xx, 0x9B6xx) and calls the float sub-collectors 0x134460
and 0x134566; callee list includes 0x13CD80 (spec 14 shifter evaluator) and 0x135D9C.
Cluster of 1. [O]

#### 0x00134380 `meas_handle_get_18` — logic
Registration helper from 0x1324A2: calls the registrar 0x151DC0 with (label, selector) and
stores the returned handle at `param_1+0x18`. Cluster of 1. [O]

#### 0x00134460 `meas_collect_floats_a` — logic
Sub-collector: calls ten getter fn-ptrs (0xA4670–0xA46D0 family) and stores the u16 values
as floats into the struct at +0x2c…+0x50. Called from 0x133C4E. Cluster of 1. [O]

#### 0x00134508 `meas_handle_get_0` — logic
Registration helper from 0x132526: registrar 0x151DC0(label @0x134638, selector) → handle
into `*param_1`. Cluster of 1. [O]

#### 0x0013451A `meas_handle_get_0b` — logic
Same pattern with label @0x134640. Cluster of 1. [O]

#### 0x0013452C `meas_collect_floats_1c` — logic
Small sub-collector from 0x133946: two getter fn-ptrs (0x7C418/0x7C422) stored as floats at
+0x1c/+0x20. Cluster of 1. [O]

#### 0x00134554 `meas_handle_get_4c` — logic
Registration helper with label @0x13464C. Cluster of 1. [O]

#### 0x00134566 `meas_collect_floats_24` — logic
Sub-collector from 0x133C4E: six getter fn-ptrs (0x7BF40–0x7BF68) stored as floats at
+0x24…+0x38. Cluster of 1. [O]

#### 0x001345CE `meas_collect_floats_2` — logic
Small sub-collector from 0x133946: two getters (0x7C418/0x7C422) into floats [0]/[1].
Cluster of 1. [O]

#### 0x001345F4 `meas_collect_inverted_8` — logic
Sub-collector from 0x133946: one getter (0x7C422), stored as `(float)~value` at +8. Cluster
of 1. [O]

#### 0x00134674 `meas_init_invalid_ff` — logic
Initialiser of three measurement/result bytes (+1/+3/+7) to the invalid marker 0xff; entry
of the 0x132980 call list. Cluster of 1. [O]

#### 0x0013467E `diag_reg_1_2` — stub
One fn-ptr registration call (target 0x148D84) with constants (1, 2); caller 0x148DBA.
Cluster of 1. [O]

#### 0x0013468A `canrx_monitor_run` — logic
Runs the complement-check monitor 0x1346B4 between two gate calls (fn-ptr with mode 2) and
clears the armed flag `PTR_DAT_00134744` afterwards. Called from the init cascade 0x14F34C.
Cluster of 1. [I]

#### 0x001346B4 `canrx_complement_check` — logic
Plausibility check of a value/complement pair (spec 07 pattern): `[0] == ~[1]` and
`[2] == ~[3]`; sets the validity bytes `[6]/[7]` (plus the all-zero special case) and
publishes the seven bytes through a fn-ptr (target 0x148ACC). Caller 0x13468A. Cluster of
1. [O]

#### 0x00134760 `lever_state_flags_decode` — logic
Decodes a 3-state input byte `[9]` (values 1/2/3) into two flags `*param_1` (state 2 or 3)
and `[7]` (state 1 or 3), clearing `[1..3]`; inactive when the enable global
`PTR_DAT_001347f8` is not 1. Caller 0x134752. [I] contact/switch pair decode. Cluster of
1. [I]

#### 0x001347A6 `cond_bytes_combine` — logic
Combines eight condition bytes (+0xb..+0x13): when all "error" bytes (odd offsets) are clear,
sets the OK flag `[4]` from the four state bytes ==1, otherwise flags `[6]=2`; clears `[5]`.
Caller 0x134752. Cluster of 1. [I]
#### 0x001347FC `mod_get_byte_8a4` — data-accessor
Getter: byte behind `PTR_DAT_001348a4`; callers 0xF7298 and 0xB912C (module input copiers).
Cluster of 1. [O]

#### 0x00134804 `meas_init_byte5_ff` — stub
Initialises the object byte at +5 to the invalid marker 0xff; entry of the 0x132980 task
list. Cluster of 1. [O]

#### 0x0013480A `diag_reg_1_b6` — stub
Registration call (target 0x148D84) with constants (1, 0xb6); caller 0x148DBA. 0xb6 may be
a monitor/DTC id. Cluster of 1. [O]

#### 0x00134818 `dtc_mon_b6_run` — logic
Runs the evaluator 0x134846 between two gate calls (fn-ptr mode 0xb6) and clears the armed
flag `PTR_DAT_001348b4`; same shape as 0x13468A with id 0xb6. Called from init 0x14F34C.
Cluster of 1. [I]

#### 0x00134846 `dtc_mon_b6_eval` — logic
Evaluates a status byte `[1]`: inactive → validity `[6]=0`, state `[2]=0`, local = `[0]==0`;
active → state 2; publishes seven bytes (states, validity) through the fn-ptr (target
0x148ACC), the same publisher as 0x1346B4. Caller 0x134818. Cluster of 1. [I]

#### 0x001348C0 `mod_get_s16_910` — data-accessor
Getter: s16 behind `PTR_DAT_00134910`; caller 0xF7298 (module input copy). Cluster of 1.
[O]

#### 0x001348C6 `mod_get_byte_914` — data-accessor
Getter: byte behind `PTR_DAT_00134914`; caller 0xF7298. Cluster of 1. [O]

#### 0x001348CE `meas_init_word2_ff` — stub
Initialises the u16 at `param_1+2` to 0xff. Cluster of 2 (0x134926 identical copy); both
entries of the 0x132980 task list. [O]

#### 0x001348D6 `canrx_dynamics_cond` — logic
Conditioning of the signed dynamics value of CAN message `0x14C` byte 4 (spec 07 §, known
anchor): with status `[1]` clear and raw ≠ 0xff computes
`(raw * 0x100 − 0x8000) / 100` into the s16 at `+2` (validity `[4]=0`), otherwise marks
invalid (`[4]=2`, `[5]=1`) and stores 0. Cluster of 1. [O] (spec 07)

#### 0x00134918 `canrx_dynamics_get` — data-accessor
Getter for the conditioned dynamics value: s16 behind `PTR_DAT_00134968`; callers 0xF7298
and 0xB912C (known anchor, spec 07). Cluster of 1. [O]

#### 0x0013491E `mod_get_byte_96c` — data-accessor
Getter: byte behind `PTR_DAT_0013496c`; callers 0xF7298/0xB912C. Cluster of 1. [O]

#### 0x0013492E `canrx_dynamics_cond__2` — logic
Structural twin of 0x1348D6 (same formula and validity codes, restructured branches) — a
second conditioning path for the same dynamics value; caller 0x14F34C (init). Cluster of
1. [O]

#### 0x00134970 `mod_get_s16_980` — data-accessor
Getter: s16 behind `PTR_DAT_00134980`; caller 0xA7C9C (CAN area). Cluster of 1. [O]

#### 0x00134978 `meas_copy_word_prev` — stub
Copies `*param_1` into `param_1[1]` (keeps the previous value of a u16 pair); caller
0x14F34C. Cluster of 1. [O]

#### 0x00134984 `diag_did_read_wrapper` — stub
Passes (index, out-ptr) to the DID reader 0x134A28 with the fixed table `DAT_00134aac`;
callers 0xAD6D4/0xAD976/0xC91B8 (diagnostic session region). Cluster of 1. [O]

#### 0x0013498E `diag_session_state_eval` — logic
Evaluates diagnostic session state: reads four bytes through fn-ptrs into +0x4c/+0x4e/
+0x9c/+0x9d; for session values 0/1/3/5/6 runs 0x135658, else derives a 9×N table lookup
(`PTR_LAB_00134ac4[9c + 9d*9]`) or index-based value into +0x9e, finishing with 0x13568E.
Caller 0x14F7C2. Cluster of 1. [I]

#### 0x00134A28 `diag_did_read_dispatch` — dispatch
Reads a data item by index into `*param_3` with status in `DAT_fff952a4`: indices <1001 go
to 0x134AD8, the ranges 1001–2000/2001–3000/3001–4000/4001–5000/5001–6000 to five different
fn-ptr handlers, others rejected (0). Reached from the diagnostic session via 0x134984.
Cluster of 1. [O]

#### 0x00134AD8 `diag_did_low_dispatch` — dispatch
Jump table for the sub-1001 indices: dispatches ~56 codes (1…0x6f, 100, 0x42…) to the
readers at 0x134C80–0x135642, defaulting to 0x135642. Also called directly from 0x144108.
Cluster of 1. [O]
## Unit summary

220 clusters, 233 functions named (rep + members).

Counts by kind (per function): logic 129, data-accessor 41, stub 54, dispatch 5, library 3,
unknown 1.

Sub-regions documented:
- 0x12CA1E–0x12D7DC: manual/tip-mode request evaluator of the shift-program module (spec 09
  §2.5) plus six situation handlers (§2.3) and two lockup helpers (§3.1, called from
  0x124570).
- 0x12D848–0x12E7BC: range/park request pair evaluation (request codes 5–8, spec 14 §4.1
  style) and its 0x57f snapshot recorders.
- 0x12E7EE–0x131BCE: tip limits + up/down inhibit machine (inhibit words +0x90/+0x94),
  gear-request monitor, and the end of the shift-program module (0x1318D8 per spec 09 §1.1).
- 0x131FD8–0x13236E: init cascades, generated module-I/O accessors (consumed by 0xF7298 and
  0xB912C) and diagnostic boundary wrappers.
- 0x13238C–0x134AD8: measurement-protocol registration and snapshot collectors (spec 10),
  the task-list walkers, and the CAN-receive dynamics conditioning/getter (spec 07 anchor
  0x1348D6/0x134918).

unknown entries:
- 0x00132B8A — bad instruction data; likely a mis-disassembled table entry in the 0x132980
  call list, not a real function.

Cross-region leads:
- Table/interp library calls into 0x121432/0x121482/0x12155E/0x1218FA/0x121A56/0x121Cxx
  (byte array, curve, map interpolation) — the unit owning 0x1214xx–0x121Dxx should
  cross-check these accessors.
- The 0x131FEE–0x13236E accessor block is the module boundary copied by 0xF7298 and
  0xB912C (spec 09 §3.2) — units owning those input copiers should cross-check.
- Many forwarding stubs resolve into the r14_app service block: 0x148ACC (fault publisher),
  0x148D84, 0x14EBFx–0x14ECxx, 0x151DC0 (measurement registrar), 0x14F34C (init cascade) —
  the r12–r14 unit should cross-check.
- 0x132980 and 0x132D8E walk task lists referencing ~90 functions across 0x135xxx–0x14Exxx
  and the boot-diag region 0x76xxx–0x77xxx — those units will see themselves called only
  indirectly (no edges.csv entries).
- Event 0x57f snapshot recorders (0x12DAE6/0x12E05C) call 0xD5A7C/0xD5B32 in the r0c_app
  fault/freeze-frame area — cross-check there.
- Cluster 3f25921616c88ebf has member 0x14E0B6 in r14_app (documented here as
  tip_obj_update_get_17__3); the r14 unit will encounter it again.
