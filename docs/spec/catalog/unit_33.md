# Unit 33 (r0c_app)

Slice 0x000FBA2C–0x000FFDD8. The bulk of this slice is one undocumented class family around
the 0x000FB4B0/0x000FC112 update chain (reached from task registration at 0x000C2254/0x000C2192/
0x000C21E4): a position/permission-mask selection class (0x000FBA2C–0x000FBFF4) and a gear
determination / gear-target rate-limiter class (0x000FC044–0x000FC73C). Shared calibration
accessors 0x001213F6 (bit-set), 0x00121432 (u8 array), 0x001214B4 (u8 2D), 0x00121482 (u16
array), 0x0012155E (s16 2D) all raise diag event 0x4d on bounds failure. Indirect calls go
through PTR_FUN_ relocation tables near each function and are absent from edges.csv.

#### 0x000FBA2C `sel_build_perm_mask` — logic
Builds the 16-bit permission mask stored at obj+0x2c: starts from a gear-dependent table
lookup (singleton `PTR_DAT_000fbc44+8` indexed by ctx+0x3d-1, bit set when result <8 via
0x001213F6), then sets bit 5 when (*ctx)+0x8d != 0 and bit 2 when obj+0x22 != 0. The mask
gates the index search in 0x000FBA82. Called from the 0x000FB4B0 update; singleton member
of its cluster. [I]
#### 0x000FBA82 `sel_search_valid_index` — logic
Searches upward from the current index (obj+0x35) to 0x31 through a 3-row 2D table at
singleton+0xc (s16 rows 0/1, byte row 2): accepts the first index whose row0/row1 masks fit
inside the allowed masks (row0 ⊆ mask from 0x000FBBB8, row1 ⊆ obj+0x28), then stores the
row2 byte at obj+0x32. If nothing matches it zeroes obj+0x30/+0x2e and calls the fn-ptr at
`PTR_FUN_000fbc5c(PTR_DAT_000fbc58, 0x7d)`. [I]
#### 0x000FBB50 `sel_find_first_valid_index` — logic
Fresh (0..0x31) variant of the mask search: returns the row2 byte for the first index whose
row0 mask fits the allowed mask computed by 0x000FBBB8 and row1 fits obj+0x28; returns 0
when no index qualifies. Writes the found index to obj+0x16. Used by 0x000FB4B0 as the
primary index pick. [I]
#### 0x000FBBB8 `sel_calc_perm_mask` — logic
Computes (without storing) the permission mask used by the two search functions: bits from a
gear-dependent lookup (singleton+8 indexed by ctx+0x3d-1, set when <8) plus bit 2 when
ctx+0x22 != 0, accumulated through bit-set helper 0x001213F6. Same shape as 0x000FBA2C
minus the obj+0x8d-condition bit. [I]
#### 0x000FBBFA `sel_dir_hysteresis` — logic
Hysteresis comparator for the direction flag obj+0x38: takes a u16 from the fn-ptr at
`PTR_FUN_000fbc64(PTR_DAT_000fbc60, param_2)`; when flag is clear, sets it if the value
exceeds *(singleton+0x74) and a byte gate *(singleton+0x7c) is exceeded; when set, clears it
if the value drops below *(singleton+0x74)-*(singleton+0x78) (spread via 0x000F9D10). [I]
#### 0x000FBC84 `sel_decode_status_bits` — logic
Decodes the status word at obj+0x2e/+0x2f into booleans: bit0→+0x20, 0x200→+0x21,
0x10→+0x39, 0x20→+0x3a, 0x40→+0x3b, 0x80→+0x3c, bit1→+0x24 (with previous kept at +0x25),
calls 0x000FBBFA, then +0x37 = (word&0x100)||(+0x38==1) and +0x3f = word&0x1000. [O]
#### 0x000FBD30 `sel_calc_mode_state` — logic
Small state derivation: +0x1c ← +0x1d, +0x1d ← +0x1b, then +0x1b = 0/1/2 from flags
obj+0x24 and obj+0xd (both set → 2). Reads as a mode escalation with two-cycle history. [I]
#### 0x000FBD56 `sel_build_condition_bits` — logic
Builds a 12-bit condition word at obj+0x40 (int* param +0x10): bits from two table compares
via `PTR_FUN_000fbeec` (singleton+0x5c and +0x14), a threshold compare at singleton+0x40,
state checks at singleton+0x3c/+0x60, a lookup at singleton+0x18 (bit 0x20 when result==0),
plus direct copies of ctx flags (+0x3c→0x40, +0x38→0x100, +0x3d→0x200, +0x3e→0x400) and a
0x80 bit when +0x3d != +0x3e. [O] (bit assignment) [I] (meaning)
#### 0x000FBEDA `sel_test_flag_bit1` — stub
Four-instruction predicate: returns ((*param_1)+0x37 & 2) != 0. Used by 0x000FB4B0 and
0x000FB25E as a condition on the shared context object. [O]
#### 0x000FBF00 `sel_track_peak` — logic
Peak tracker for the u16 at obj+0x14: reads a table value via `PTR_FUN_000fc014`
(singleton+0x28); when gate `PTR_FUN_000fc01c(PTR_DAT_000fc018)` is false keeps
max(value, obj+0x14), otherwise replaces obj+0x14 with a second lookup (singleton+0x2c). [I]
#### 0x000FBF4C `sel_scale_ramp` — logic
Percentage scaling of obj+0x14: when the lookup at singleton+0x28 is below the current
value, mixes a table step (singleton+0x38 indexed by obj+0xc-1) with a percent factor
(singleton+0x30 × lookup(singleton+0x34)/100) via the s8/s16 accessors; result clamped by
`PTR_FUN_000fc03c`. When obj+0x17 or obj+0x18 is set, obj+0x14 is overwritten from
singleton+0x2c instead. [I]
#### 0x000FBFF4 `sel_lookup_scaled_val` — data-accessor
Returns a u16 array lookup on the object at obj+0x58 with a fixed index from
`PTR_DAT_000fc040` (fn-ptr at `PTR_FUN_000fc038`). Called by the 0x000FB4B0 update only. [I]
#### 0x000FC044 `gear_set_current` — stub
Setter: stores the byte at obj+0x18 and mirrors it into global `DAT_000fc234`. 0x000FC112
passes the determined gear, so this publishes "current gear". Callers 0x000F83A0 and
0x000FC112. [O]
#### 0x000FC04E `gear_get_clamped` — data-accessor
Returns the gear byte at (*param_1)+0x3f saturated to 0..8: negative or >8 yields 8.
Reads as a "get gear, 8 = invalid" accessor; called from top-level 0x000C2192 and
0x000F8420. [I]
#### 0x000FC06E `tbl_lookup_1d_w` — data-accessor
Thin wrapper: returns u16 fn-ptr lookup `PTR_FUN_000fc238(*param_1, param_2)` (resolved
callee 0x000F7266). Used by 0x000FC6CE, 0x000FC73C and 0x000FA0BA for 1-D word lookups. [I]
#### 0x000FC07E `gear_fallback_cond` — logic
Returns 1 only when singleton+0x18 state ==1, obj+0x7e ==1, and counter thresholds hold
(`PTR_DAT_000fc240+0x4e` < *(singleton+0x10), `PTR_DAT_000fc240+0x52`==0,
`PTR_DAT_000fc244+0x384` < *(singleton+0x14), `PTR_DAT_000fc244+0x388`==0). Result feeds
0x000FC112 as "use fallback gear" condition. [I]
#### 0x000FC0CA `sel_map_state_a` — logic
Maps the state behind singleton+0x4 (1→2, else unchanged) into global `PTR_DAT_000fc248`.
Called from 0x00130300 (outside this unit — cross-region lead). [I]
#### 0x000FC0E2 `sel_map_state_b` — logic
Same 1→2 remap for the state behind singleton+8, published to `PTR_DAT_000fc24c`; also
called from 0x00130300. [I]
#### 0x000FC102 `sel_init_defaults` — init
Constructor-style init: stores param_2 at *param_1 and sets defaults 10000 (0x2710) at
+0x36 and +0x34. No resolved callers (reached via constructor/vtable). [I]
#### 0x000FC112 `gear_determine` — logic
Main gear determination (called from task 0x000C21E4): copies input words, searches gear
index 1..7 by comparing a table (fn-ptr `PTR_FUN_000fc250`) against the input at (*p)+0xa,
clamps +0x44-sourced values to 0x21c, picks gear from (*p)+0x40/+0x46 or fallback +0x1b
(gated by 0x000FC07E and 0x000FC4E8), treats 0/9 as invalid, writes up/down-shift markers
(0xff at `PTR_DAT_000fc4a0+0x5e`, `*PTR_DAT_000fc4a4`, `*PTR_DAT_000fc4ac`), gear-change
flag +0x16, publishes via 0x000FC044 and `*PTR_DAT_000fc4b0`, checks magic 0x0F/'!' from
two getters for +0x45, then calls 0x000FC6CE and 0x000FC73C. [O] (structure) [I] (semantics)
#### 0x000FC358 `gear_calc_step_limit` — logic
Computes a step limit: base from table singleton+0xc row (gear-1), optionally min-ed with
row 0x2f (selector param_3); a special case (states 0x0c/0x0f/0x01 + '!' + param_3==0)
further clamps to row 0xb and can force 1 when (*p)+0x18 >1 and a singleton+0x58 lookup
exceeds a limit. Heavily reused (0x001277D8, 0x0012A5E6, 0x0012EEB0, 0x0012A772). [I]
#### 0x000FC43E `gear_update_trackers` — logic
Propagates shift history (+0x28←+0x27, +0x2a←+0x29, +0x2c←+0x2b, +0x2e←+0x2d) then runs
the tracker chain 0x000FCAAA, 0x000FCACE, 0x000FCB22, 0x000FCC44 and returns the
0x000FCDFE(param_1, param_2) result. Called from 0x000FC5DE. [O]
#### 0x000FC472 `gear_eval_flag25` — logic
Sets bool obj+0x25 from (table lookup at singleton+0x1c indexed by a getter of
`PTR_DAT_000fc4b4`) == 1. Called by 0x000F8490. [I]
#### 0x000FC4E8 `gear_calc_bit_cond` — logic
Computes (a & b) == 0 && (a & c) != 0 from three u16 fn-ptr getters on *param_1
(`PTR_FUN_000fc6f8/000fc6fc/000fc700`); result gates gear selection in 0x000FC112 (+0x11). [I]
#### 0x000FC55A `gear_publish_shift_events` — logic
Compares previous gear +0x1b against current +0x18: on upshift writes 0xff at
`PTR_DAT_000fc710+0x65` and a 1→2-remapped state at +0x3d; on downshift the remap goes to
`*PTR_DAT_000fc714`. Also sets +0x5 when +0x1b exceeds +0x1c (unless a getter returns '!'),
clears +0x6, and sets +0x26 from a speed threshold compare (PTR_DAT_000fc708 vs
**(singleton+0x54)). [O] (writes) [I] (meaning)
#### 0x000FC5DE `gear_limit_target` — logic
Gear-target rate limiter: computes down-step limit via 0x000FC358(...,0,...) (negated
through `PTR_FUN_000fc724`) and up-step limit via 0x000FC358(...,1,...), clamps the
requested target within [current-stepdn, current+stepup] (current = +0x18, step stored at
+0x1d), runs 0x000FCA3A and 0x000FC43E, floors the result at min(+0x17,+0x18) with fn-ptr
diag event 0x4f, forces >=2 when +0x24==1, and on invalid (0 or >8) keeps +0x18 with diag
event 0x96. Called from 0x000FC112 and 0x000F8490. [O]

#### 0x000FC6CE `gear_calc_limit_current` — logic
Computes a scaled limit for the current gear: word lookup via 0x000FC06E(obj, gear at
+0x18) multiplied by global `PTR_DAT_000fc734` /1000, passed through the clamp fn-ptr at
`PTR_FUN_000fc738` and stored at obj+0x46. Called at the end of 0x000FC112. [O]
#### 0x000FC73C `gear_calc_limit_per_gear` — logic
Fills the per-gear u16 array obj+0x48..+0x56: for gears 1..8 takes 0x000FC06E(obj, i) ×
global `PTR_DAT_000fc9c0` /1000 through the same clamp fn-ptr (`PTR_FUN_000fc9c4`). Read
back per gear by the 4-copy cluster 0x000FCE78/0x000FD638/0x001018C8/0x001240D8. [O]
#### 0x000FC7FE `gear_hunt_enable_gate` — logic
Sets obj+0x39 = 0 when global `*PTR_DAT_000fc9c8` is 7/8/9 or `*PTR_DAT_000fc9cc` == 2,
else 1. Gates the three change counters' increment in the hunting monitor. [O]
#### 0x000FC828 `gear_hunt_reset_cond` — logic
Clears counters/one-shots obj+0x3d/+0x3e/+0x3a/+0x3b/+0x3c when the byte counter at
`PTR_DAT_000fc9d4+0x70` has nearly wrapped (0xff-x < 1) or `PTR_DAT_000fc9d4+0x98` exceeds
**(singleton+0x50). Periodic reset of the shift-hunting counters. [I]
#### 0x000FC860 `gear_hunt_reset_publish` — logic
When the published state word `*PTR_DAT_000fc9d8` is 0, clears counters +0x3a/+0x3b/+0x3c
and republishes **(singleton+0x40) remapped 1→2 into that word. [O]
#### 0x000FC890 `gear_cnt_src4b_changes` — logic
Direction tracker for the ctx byte (*p)+0x4b (0 replaced by 8): rising/falling edge flags at
obj+0x8/obj+0x24 (int* offsets), and increments change counter obj+0x3a while the enable
gate obj+0x39 is 1. Last value kept at obj+0xe (int offset = +0x38). [O] (mechanics)
#### 0x000FC8DC `gear_cnt_src4a_changes` — logic
Same tracker for ctx byte (*p)+0x4a (0 replaced by 1): edge flags at obj+0x28/+0x2c,
counter obj+0x3b, last value at obj+0xf. Feeds the second hunting detector 0x000FC9E8. [O]
#### 0x000FC928 `gear_cnt_gear_changes` — logic
Same tracker for the module's own gear byte obj+0x18 vs previous obj+0x10: edge flags at
+0xc/+0xd (int offsets = +0x30/+0x34), counter +0x3c, previous updated at the end. [O]
#### 0x000FC970 `gear_hunt_detect_a` — logic
One-shot hunting detector A: when counters +0x3a and +0x3c exceed thresholds
**(singleton+0x44) / **(singleton+0x4c) and +0x3d is still 0, sets +0x3d=1 and records an
event via 0x000C2696(`PTR_DAT_000fc9dc`, [+0x12]+1, +0x12, +0x13). Cross-region: 0x000C2696
is the event recorder in the 0xC0xxx area. [I]
#### 0x000FC9E8 `gear_hunt_detect_b` — logic
Hunting detector B on counters +0x3b/+0x3c against **(singleton+0x48)/**(singleton+0x4c)
with one-shot +0x3e; event via 0x000C2696(`PTR_DAT_000fcc1c`, [+0x14]+1, ...). [I]
#### 0x000FCA3A `gear_hunt_monitor_run` — logic
Orchestrates the hunting monitor: enable gate (0x000FC7FE), resets (0x000FC828, 0x000FC860),
the three counters (0x000FC890/0x000FC8DC/0x000FC928), both detectors (0x000FC970,
0x000FC9E8); then mirrors +0x3d/+0x3e into globals `*PTR_DAT_000fcc24`/`[1]` and clears
`*PTR_DAT_000fcc28` when both detectors inactive or newly triggered. Called by 0x000FC5DE. [O]
#### 0x000FCAAA `gear_track_peak_targets` — logic
obj+0x27 = max(ctx+0x4b, ctx+0x4a) when 0x4b < 0x4a and 0x4b != 0, else ctx+0x4b; obj+0x29
= ctx+0x4a. Feeds the history propagation in 0x000FC43E (+0x28←+0x27, +0x2a←+0x29). [O]
#### 0x000FCACE `gear_publish_dir_events` — logic
Publishes 1→2-remapped states from singleton+0x38/+0x3c into `*PTR_DAT_000fcc2c` /
`*PTR_DAT_000fcc30` when the tracked peak crosses windows (+0x27 < +0x28 <= +0x2b and
+0x2a < +0x29 with +0x2a <= +0x2d). [I]
#### 0x000FCB22 `gear_step_low_bound` — logic
Maintains the lower bound obj+0x2b (walks 1..8, reset to 8) of the allowed gear window:
inactive (ctx+0x4b ==0, hunting flag +0x3d set, or state !=1) resets +0x32/+0xc and +0x2b=8;
active path computes thresholds via 0x000FCD3A rows 0/1 and steps +0x2b up (clamp 8) or down
(clamp 1, publishing the singleton+0x38 remap into `*PTR_DAT_000fcc2c`). [I]
#### 0x000FCC44 `gear_step_high_bound` — logic
Mirror of 0x000FCB22 for the upper bound obj+0x2d (walks 1..8, reset to 1, defaults 10000
for +0x36/+0xd) driven by ctx+0x4a with thresholds from 0x000FCD9C; publishes the
singleton+0x3c remap into `*PTR_DAT_000fce60`. [I]
#### 0x000FCD3A `gear_thr_low_calc` — logic
Threshold used by 0x000FCB22: (u8 2D lookup singleton+0x28[param_3][gear-1] × s16 2D lookup
singleton+0x30[gear-1][param_2]) /100 through `PTR_FUN_000fce6c`, then min() with the pair
value from `PTR_FUN_000fce70(PTR_DAT_000fce4c, out, param_2)`. Uses accessors 0x001214B4 /
0x0012155E. [O]
#### 0x000FCD9C `gear_thr_high_calc` — logic
Same computation for 0x000FCC44 with tables singleton+0x2c/+0x34 and the max() variant
(`PTR_FUN_000fce74`); accessors 0x001214B4/0x0012155E. [O]
#### 0x000FCDFE `gear_clamp_target_range` — logic
Clamps the gear target into the window [obj+0x2b, obj+0x2d]: if the target already lies
below the lower bound (or lower==8) and above the upper bound (or upper==1) it passes
through and sets obj+0x38=0; otherwise clamps to the window and sets obj+0x38=1. Result
returned to 0x000FC43E/0x000FC5DE. [O]
#### 0x000FCE78 `gear_get_limit_for_gear` — data-accessor
Switch on gear 1..8 returning the u16 at +0x48/+0x4a/+0x4c/+0x4e/+0x50/+0x52 (default)/
+0x54/+0x56 — reads the per-gear limit array filled by 0x000FC73C. Cluster of 4 identical
copies (0x000FCE78, 0x000FD638, 0x001018C8, 0x001240D8 — the latter two live in r10_app and
serve their own module objects; 0x001240D8 is called by 0x00124138). [O]
#### 0x000FCED8 `est_publish_word` — stub
Stores the word at obj+0x4e and mirrors it into global `*PTR_DAT_000fcf40`. Called by
0x000FCFC2 with (*p)+0x58. [O]
#### 0x000FCEE2 `est_lookup_scaled` — logic
Floors a singleton+0x1c word at `PTR_DAT_000fcf48[0x84]`, feeds a bound pair via
`PTR_FUN_000fcf4c`, calls through the object vtable (+0x14 method with +0xc offset) and
converts the s16 result through `PTR_FUN_000fcf50`. Returns a byte used by 0x000FCFC2's
state selection. [I]
#### 0x000FCF28 `est_get_per_gear` — data-accessor
Switch on gear 1..8 returning the u16 at +0x54/+0x56/+0x58/+0x5a/+0x5c/+0x5e (default)/
+0x60/+0x62 — the per-gear table computed at the end of 0x000FCFC2 via 0x000FD2D8; invalid
gear raises diag event 0x68 through `PTR_FUN_000fd178`. Consumed by 0x000FA0BA. [O]
#### 0x000FCFBA `est_get_word_9a` — stub
Three-instruction getter: returns the s16 at (*param_1)+0x9a. Consumed by 0x00100AB4 in
r10_app (cross-region). [O]
#### 0x000FCFC2 `est_periodic_update` — logic
300-instruction periodic update (task entry 0x000C226A) of an estimator class
(singleton `PTR_DAT_000fd17c`): scales an input by 0x1e78/10000, selects word +0x10 from
**(singleton+0x0) or (*p)+0x50 by (*p)+0x52==2, transforms +0x36 ((*p)+0x4c) into +0xe with
sign flag +0x3a, publishes (*p)+0x58 through 0x000FCED8, computes +0xf = ((x×+0x4e)>>5)×
0x417/10000, maintains a 9-entry u16 history at +0xc..+0x1c and a slope estimate at +0x14
((new-history[b-1])×1000/((b-1)×`PTR_DAT_000fd30c`)), writes a 0xff event marker at
`*PTR_DAT_000fd314` on +0x35 transitions 0→1/2, resolves +0x53 by state (0x000FCEE2), and
finally fills the per-gear table +0x54..+0x62 with 0x000FD2D8(1..8). [O] (structure)
[I] (semantics)
#### 0x000FD2D8 `est_calc_per_gear` — logic
Per-gear value for 0x000FCFC2: base from 0x000FCE78(`PTR_DAT_000fd308`, gear) multiplied by
the 2D lookup at singleton+0x30 (base, gear) and scaled /100 by the byte at obj+0x53. [O]

#### 0x000FD324 `lib_divmod_step` — library
Integer divider with remainder carry for s16 streams: computes ((acc + input - out×factor) +
prevRemainder) / divisor (+0xc), stores the new remainder at +0x4, saturates the quotient to
±0x7fff and remembers the input at +0x8. Disabled when +0x10 != 0. No resolved callers —
library helper. [O]
#### 0x000FD38A `lib_ramp_calc_step` — library
Computes a ramp step from range params: denominator <1 → status 1, param_3 < param_2 →
status 3, negative step count from 0x000FD41A → status 4 (each zeroing +0xc and setting
+0xe=0); success stores step+1 at +0xc, residual 1-step at +0xe and status 0 at +0x10. [O]
#### 0x000FD3E0 `lib_ramp_update_notify` — library
Change-detecting wrapper around 0x000FD38A: snapshots +0xc/+0xe, runs the step calc, and
when both changed invokes the vtable method at (*p)+0x20 with ((*p)+0x18 + p, +0xa) —
listener notification on ramp-parameter change. Used widely (0x0011FD44, 0x0011E23C,
0x0012227C, 0x000F9A1C, 0x0011F410, 0x000FCEE2, 0x0011FF6A, 0x0011E56C). [O]
#### 0x000FD41A `lib_ramp_steps_u16` — library
Step-count helper: ((param_3 & 0x7fff) << 1) / param_2, saturating to 0xffffffff (returned
as negative by callers) on overflow. [O]
#### 0x000FD564 `lib_ramp_calc_step32` — library
32-bit twin of 0x000FD38A: same status codes 1/3/4 with dwords at +0x10/+0x14 and status at
+0x18, using 0x000FD60C for the step count. [O]
#### 0x000FD5B6 `lib_ramp_update_notify32` — library
32-bit twin of 0x000FD3E0: snapshots param[4]/param[5], calls 0x000FD564, notifies the
(*p)+0x20 vtable method with param_1[3] on change. Callers 0x00120378 and 0x0011F410. [O]
#### 0x000FD60C `lib_ramp_steps32` — library
Step-count helper, 32-bit form: (param_3 << 1) / param_2, saturated at DAT_000fd620. [O]
#### 0x000FD624 `obj_check_init_mark` — logic
Predicate with side effect: when obj+0x20 == 0 it stores 0xff at obj+0x11 and returns true.
Cluster of 5 byte-identical copies (0x000FD624, 0x000FE89C, 0x000FF3B8, 0x000FFDC4,
0x00101F84 — the last two in r10_app), one per monitored object; all reached via pointer
tables (no direct callers). Reads as "not yet initialized → mark invalid". [I]
#### 0x000FD964 `obj_init_vtable` — init
Five-instruction initializer: stores `PTR_PTR_000fd9ec` at obj+4 and 1 at obj+8. Called by
0x000F8FC4. Reads as vtable/first-element setup for a one-entry list. [I]
#### 0x000FD96E `obj_bind_input` — logic
Copies **(singleton `PTR_DAT_000fd998`+0x50) into obj+0xc, registers the object via
`PTR_FUN_000fd9f4(PTR_PTR_000fd9f0, p, 0)` and stores the registration result at obj+0x10
(resolved callees 0x00123916/0x001236E4 = list register/query pair). Called by 0x000F9062. [I]
#### 0x000FDCF6 `vec_cb_store_state` — logic
Register-context callback body (unaff r12/r13, in_sr — no direct callers, entered via
pointer table): when SR bit0 is set and obj+0x18 == 0, clears obj+0xe; always stores r13 at
obj+0xd. Looks like a state-store callback invoked from assembly glue. [I]
#### 0x000FDD0A `evt_publish_state_cond` — logic
No-arg event publisher (pointer-table entry): when **(singleton+0x3c) <
`PTR_DAT_000fde90[0x73]` and `[0x74]` ==1, evaluates the `_LAB_000fdea4/8/c/ac/b0` fn-ptr
chain on singleton+0x38, remaps the result 1→2 and publishes it through `*_LAB_000fdeb4`.
Uses table accessors 0x001218FA/0x00121D26/0x00121C86. [I]
#### 0x000FDD56 `evt_publish_on_change` — logic
No-arg event publisher: when `PTR_DAT_000fde98[0x18] != [0x11]` and **(singleton)==0,
evaluates the `_LAB_000fdebc/c0` fn-ptr chain on singleton[0x10] with the byte/word pair at
`_LAB_000fdeb8+4/+6`, writes 0xff at `PTR_DAT_000fde8c[0x11]` and the 1→2-remapped result at
[0x20]. [I]
#### 0x000FDE3E `tbl_dispatch_signal_lookup` — dispatch
Selector dispatch on param_2 (2..8): picks sub-objects at +0xc/+0x10 (cases 2,3 → lookup
pair `_LAB_000fdea8`/`_LAB_000fdeac`) or +0x14/+0x18/+0x1c/+0x2c/+0x30 (cases 4..8 → pair
`_LAB_000fdf42_2`/`_LAB_000fdf48`), calls pair(object, param_3, lookup(param_4)); any other
selector raises diag event 0x68 via `_LAB_000fdf50`. [O]
#### 0x000FE15C `tbl_select_member_ptr` — data-accessor
Maps selector 4 → obj+4 and 9 → obj+0xc, returning the member address; other selectors get
diag event 0x68 and NULL. Pointer-table entry (no direct callers). [O]
#### 0x000FE186 `tbl_select_member_ptr__2` — data-accessor
Same selector mapping to obj+8 (4) and obj+0x10 (9) with the same 0x68 diag on default. [O]
#### 0x000FE1B0 `obj_init_vtables2` — init
Initializer storing two vtable pointers (`PTR_PTR_000fe1f0` at +4, `PTR_PTR_000fe1f4` at
+0xc) with count 1 at +8/+0x10. Called by 0x000F8FC4 — two-entry list init. [I]
#### 0x000FE26C `tbl_reset_word_by_selector` — dispatch
Selector dispatch: 6 → 0x000FE314(p,0) nonzero zeroes `*PTR_DAT_000fe3b0`; 1 →
0x000FE314(p,1) nonzero zeroes `*PTR_DAT_000fe3b4`; 0/4/5/9 are accepted no-ops; anything
else raises diag 0x68. Pointer-table entry. [O]
#### 0x000FE2DA `obj_bind_inputs2` — logic
Registers the object with two lists (`PTR_PTR_000fe3b8`, `PTR_PTR_000fe3c4` via the
`PTR_FUN_000fe3bc/000fe3c0` register/query pair, resolved 0x00123916/0x001236E4), stores the
first result at +0x16 and the second minus 5 at +0x14, then evaluates 0x000FE358. Called by
0x000F90C4. [I]
#### 0x000FE314 `tbl_state_in_set` — logic
Returns 1 when the global state byte `*PTR_DAT_000fe3c8` equals any of 6 entries of a u8 2D
table (singleton `PTR_DAT_000fe3cc`, accessor 0x001214B4) in the column selected by
param_2 (only param_2 <3); otherwise 0. [O]
#### 0x000FE358 `obj_eval_flag18` — logic
Stores (0x000FE314(p,2) != 0) at obj+0x18. One-line evaluation of set membership for column
2. [O]
#### 0x000FE372 `obj_check_val_below_thr` — stub
Five-instruction predicate: param_2 <= *(ushort *)(p + off). Cluster of 5 copies differing
only in the compared field offset: +0xa6 (rep 0x000FE372), +0xa8 (0x000FE37E), +0xaa
(0x000FE38A), +0xac (0x000FE396), +0x92 (0x000FE3D4) — per-signal "value at/below limit"
comparators invoked from pointer tables. [O]

#### 0x000FE658 `sig_update_output` — logic
Periodic update of a signal object (called by 0x000F9088): copies singleton+4 byte to +0x16,
runs plausibility 0x000FE774, computes two interpolations through `PTR_FUN_000fe854` with a
state byte and s16 input, registers the object with the 0x00123916/0x001236E4 list pair
(+0x14), sets active flag +0x11 (clearing global `*PTR_DAT_000fe86c` when inactive), and
selects the output word +0xe: 0 inactive; `*PTR_DAT_000fe870`/0x000FE824/0x000FE80E variants
when state==2; otherwise the first interpolation. [I]
#### 0x000FE760 `sig_init_obj` — init
Initializer: zeroes +0x12 and global `*PTR_DAT_000fe86c`, stores vtable `PTR_PTR_000fe878`
at +4 with count 1. Called by 0x000F8FC4. [O]
#### 0x000FE774 `sig_calc_plaus` — logic
Plausibility/correction gate for 0x000FE658: reads singleton (+0xc char, +0x10 byte, +0x18
object) and a lookup chain; when the char is 0 takes `[0x4d]`, else when the byte is below
`*PTR_DAT_000fe888` or the lookup is ≤ `[0x36]` publishes the 1→2 remapped word to
`*PTR_DAT_000fe890`. Sets +0x12 and, on the 0→1 edge of that flag with +0x12 previously 1,
writes 0xff to `*PTR_DAT_000fe86c`. Returns the selected byte. [I]
#### 0x000FE80E `sig_lookup_2c` — data-accessor
2D lookup on the object at singleton+0x2c with the global byte `*PTR_DAT_000fe894` and the
passed word; returns u16 (accessor family 0x00121A56). [O]
#### 0x000FE824 `sig_lookup_30` — data-accessor
2D lookup on singleton+0x30 with `*PTR_DAT_000fe894` and an s16; returns s16 (accessor
0x001218FA). [O]
#### 0x000FE83A `sig_get_flag_34` — stub
Three-instruction getter: returns **(byte **)(obj + 0x34). [O]
#### 0x000FE8B0 `sig_calc_percent_out` — logic
Computes the percent-scaled output byte +0x16: runs the 5-buffer extraction
`PTR_FUN_000fe96c(singleton,1,...)`, converts via two fn-ptrs, evaluates a lookup on
singleton+0x64, then scales (lookup(singleton+0x68, `PTR_DAT_000fe984`, +0x15) × +0x16)/100.
Called by 0x000FEDDE. [I]
#### 0x000FEB72 `vec_cb_store_cmp` — logic
Register-context callback (in_r0/r2/r7, unaff r12; no direct callers): stores r0 at (r7+1)
and returns ((r12+0xc as s16) < r2) && (r12+0x15 == 2). Comparison predicate glued in from
assembly. [I]
#### 0x000FEDBA `caseD_16` — stub
Pre-named default case of `switchD_000fe960`: raises diag event 0x68 via
`*_LAB_000fef10(PTR_DAT_000fef0c, 0x68, 0)` and returns 0. Name kept as-is per sweep rules. [O]
#### 0x000FEDD4 `obj_init_vtable__2` — init
Same five-instruction vtable init as 0x000FD964 with `PTR_PTR_000fef14` at +4 and count 1 at
+8. Called by 0x000F8FC4. [O]
#### 0x000FEDDE `sig_update_words` — logic
Second signal-class update (called by 0x000F9088): computes two output words
obj+0xc/obj+0xe as (word3 × (word1 + s16a))/100 and (word4 × (word2 + s16b))/100 from six
singleton lookups (+0x14/+0x18/+0xc/+0x10/+0x58/+0x5c, anchor `PTR_DAT_000fef20+0x3a6`),
copies **(singleton+0x60) to +0x10, runs 0x000FE8B0, copies `*PTR_DAT_000fef34` to +0x14
unless +0x12 == 8, and re-registers with the list pair (result to +0x12). [I]
#### 0x000FF0C0 `evt_dispatch_state_lo` — dispatch
State dispatch for codes 0..9 of an event/state class: 1 clamps +0x13 to
`PTR_DAT_000ff1b0[0x6e]`, sets +0x14 and flags +0x15=2 when +0xe < `[0x71]`; 2 sets +0x17=1
when +0x16 < `*PTR_DAT_000ff1a0`; 3/4/6 fall through to +0x14 = `*PTR_DAT_000ff1b4`; 7 also
runs 0x000FF330 and, unless `PTR_DAT_000ff1b8[0x18]==[0x11]` or a gate is set, publishes the
0xff marker + 1→2 remap at `PTR_DAT_000ff1bc[0x10]/[0x1a]`; 8 sets +0x11=1; 9 is a pass-
through; other codes raise diag 0x68. Pointer-table entry. [O] (writes) [I] (meaning)
#### 0x000FF1D4 `evt_dispatch_state_hi` — dispatch
Jump-table dispatch (switchdataD_000ff1f4) for codes 0x0a..0x1d: 0x0c publishes
**(singleton+0x44) to `*PTR_DAT_000ff374`; 0x11 evaluates a lookup chain on singleton+0x34
and publishes the 1→2 remap to `*PTR_DAT_000ff384`; 0x14 falls into 0x18 (setting +0x15=2)
when `PTR_DAT_000ff388[0x72]`==0 and +0xe < `[0x71]`; 0x16/0x1a/0x1b map to the default diag
0x68; 0x1d runs 0x000FF330 and publishes marker/remap at `PTR_DAT_000ff38c[0x10]/[0x1a]`. [O]
#### 0x000FF2A8 `evt_test_thr_le` — stub
Nine-instruction predicate: **(singleton+0x50) <= (byte)`*PTR_DAT_000ff398`. Pointer-table
entry. [O]
#### 0x000FF2BC `evt_test_thr_lt` — stub
Companion predicate: (byte)`*PTR_DAT_000ff398` < **(singleton+0x4c). [O]
#### 0x000FF2D0 `evt_check_block_conds` — logic
Multi-condition gate returning 1: when singleton+0x54 is 0, returns 1 if
`*PTR_DAT_000ff39c`==1, or the 1→2-remapped **(singleton+8) exceeds the near-wrapped counter
`PTR_DAT_000ff38c[0x5f]`, or `PTR_DAT_000ff38c[0x9c]` <= **(singleton+8); additionally
returns 1 when singleton+0x28 ==1 and the `PTR_FUN_000ff3a4` check passes. Calls the 0xFB
module getter 0x000FB188 (obj+0x42) — cross-class read. [I]
#### 0x000FF330 `evt_calc_percent` — logic
Computes the percent byte used by both dispatchers: (lookup(singleton+0x40,
`*PTR_DAT_000ff3b0`) × lookup(singleton+0x3c, `PTR_DAT_000ff3a8[1]`, +0x6 word))/100 through
`PTR_FUN_000ff380`. Accessors 0x00121A56/0x00121CC6/0x001215B6. [O]

#### 0x000FF432 `vec_cb_calc_delta` — logic
Register-context callback (five params with in_r2; no direct callers): computes
lookup(param_1, param_2, *in_r2) through `*_LAB_000ff60c/000ff610`, forms the byte delta
result - param_5 clamped into 0..0xfe (else 0), stores it at `*_LAB_000ff614`, and returns
FUN_000ff3b8() == 1. [I]
#### 0x000FF5D6 `evt_default_case_68` — stub
Unnamed default case of the 0x000FF1D4-area switch family: raises diag event 0x68 through
`*_LAB_000ff634(PTR_DAT_000ff630, 0x68, 0)` and returns 0. [O]
#### 0x000FF5F0 `obj_init_vtable__3` — init
Third copy of the five-instruction vtable initializer (`PTR_PTR_000ff638` at +4, count 1 at
+8), called by 0x000F8FC4. [O]
#### 0x000FF63C `mon_update_periodic` — logic
Periodic update of the monitor class (singleton `PTR_DAT_000ff8ac`, input word
`*PTR_DAT_000ff8b0`): +0x10 = (0x000FFCA6(singleton, word) × lookup(singleton+0xc,
`PTR_DAT_000ff8b4`))/10, raising diag event 0x62 when +0x10 < **(singleton+0x10) or
0x000FFB9C(singleton, word) < **(singleton+0x54); sets band flags +0xd/+0xc (word vs
**(singleton+0x5c)/**(singleton+0x58)), +0xe (**(singleton+0x8c)==1) and +0xf
(**(singleton+0x80)==1 plus `PTR_FUN_000ff8cc` check); writes 0xff to
`*PTR_DAT_000ff8d0` when **(singleton+4) == -1; registers with the 0x00123916/0x001236E4
pair (+0x12), then runs 0x000FF970 and 0x000FFA2A. [O] (structure) [I] (semantics)
#### 0x000FF72C `mon_dispatch_state` — dispatch
State dispatch 0..4 of the monitor class: 0 clears +0x11/+0x1c; 1 sets +0x11; 3 clears
+0x11 and publishes the 1→2 remap of **(singleton+0x98) to `*PTR_DAT_000ff8e0`; 4 sets
+0x11, evaluates a lookup on singleton+0x90 and publishes the 0xff marker + remap at
`PTR_DAT_000ff8f0[0x13]/[0x67]`; 2 is a pass-through; else diag 0x68. [O]
#### 0x000FF970 `mon_hyst_state13` — logic
Two-state hysteresis on +0x13: computes two values from lookups on singleton+0x18/+0x30
(anchor `PTR_DAT_000ffb78+0x394`) and a percent-scaled row value from singleton+0x2c;
state 0 → 1 when the first value exceeds `[0x389]` and the scaled value >=
`*PTR_DAT_000ffb58`, state 1 → 0 when the reverse margins hold; any other state resets to
0 with diag 0x68. Returns `[0x389]`. [I]
#### 0x000FFA2A `mon_run_timer_fsm` — logic
Three-state timer FSM on +0x14 driven by a lookup on singleton+0x20 (`PTR_DAT_000ffb78+
0x38c/+0x384` anchors), margin uVar9 = lookup - **(singleton+0x24), byte pair
`*PTR_DAT_000ffb58`/**(singleton+0x28) and the singleton+0x80 gate: transitions reset the
timer word `*PTR_DAT_000ffb90` to 0 or 0xffff; at the end forces 0xffff when the gate is
active or +0xd set, then stores +0x16 = lookup(singleton+0x14,...) - `*PTR_DAT_000ffb90`,
rearming (0xffff) when it reaches 0. [I]
#### 0x000FFB9C `mon_dispatch_mode_thr` — dispatch
Dispatches on the mode byte `*PTR_DAT_000ffdb4` (1..8) to the first set of per-mode
characteristic lookups — 0x000FFC1C(+0x34), 0x000FFC2C(+0x38), 0x000FFC3C(+0x3c),
0x000FFC4C(+0x40), 0x000FFC5E(+0x44), 0x000FFC70(+0x48), 0x000FFC82(+0x4c),
0x000FFC94(+0x50); invalid modes raise diag event 0x8f and return 0xffff. Called by
0x000FF63C for the threshold-side value. [O]
#### 0x000FFCA6 `mon_dispatch_mode_val` — dispatch
Mirror dispatcher for the value side: mode 1..8 selects 0x000FFDA2(+0x7c), 0x000FFD90
(+0x78), 0x000FFD7E(+0x74), 0x000FFD6C(+0x70), 0x000FFD5A(+0x6c), 0x000FFD48(+0x68),
0x000FFD36(+0x64), 0x000FFD24(+0x60) — the same 15-member lookup cluster in reverse
table order; invalid mode raises diag 0x8f and returns 10. The 8+8 slot layout reads as a
variant/gearbox-type characteristic selection. [O] (structure) [I] (purpose)
#### 0x000FFC1C `mon_tbl_lookup_34` — data-accessor
Eight-instruction wrapper: u16 1D lookup (fn-ptr `PTR_FUN_000ffdc0`, accessor 0x001216E4)
on the table pointer at obj+0x34 with the passed word. Member 1 of the 15-copy cluster;
each copy differs only in the table slot: +0x34 (rep), +0x38, +0x3c, +0x40, +0x44, +0x48,
+0x4c, +0x50 (set A) and +0x60, +0x64, +0x68, +0x6c, +0x70, +0x74, +0x7c (set B). [O]
#### 0x000FFDD8 `obj_bind_set_c` — logic
Stores param_2 at obj+0xc, registers the object with the 0x00123916/0x001236E4 list pair
(`PTR_PTR_000fff08`) and stores the query result at obj+0xe. Called by 0x000F90C4. [O]
#### 0x000FFD36 `mon_tbl_lookup_64` — data-accessor
Singleton cluster (byte-identical to the 15-copy family but recorded separately by the
sweep): u16 1D lookup (fn-ptr `PTR_FUN_000ffdc0`, accessor 0x001216E4) on the table at
obj+0x64 — the set-B slot selected by mode 7 in dispatcher 0x000FFCA6. [O]

## Unit summary

- 102 clusters documented, 127 name rows (4 multi-member clusters: 0x000FCE78 ×4,
  0x000FD624 ×5, 0x000FE372 ×5, 0x000FFC1C ×15).
- Kinds: logic 65, data-accessor 28, stub 14, library 7, dispatch 7, init 6. Unknown: 0
  (every entry has at least mechanically observable behavior; semantics of the `sel_`
  permission-mask search and the `est_`/`mon_` classes remain inferred [I]).
- Class map of the slice: `sel_` permission-mask selection (0x000FBA2C–0x000FBFF4, driven
  by 0x000FB4B0), `gear_` gear determination + target rate limiter + hunting monitor
  (0x000FC044–0x000FCE78, driven by 0x000FC112/0x000FC5DE from task 0x000C21E4), `est_`
  estimator with history/slope (0x000FCED8–0x000FD2D8, task 0x000C226A), `lib_` ramp/divmod
  helpers (0x000FD324–0x000FD60C), `obj_`/`vec_`/`evt_`/`sig_` pointer-table glue for the
  input-binding framework around 0x000F8FC4/0x000F9062/0x000F9088/0x000F90C4, and `mon_`
  variant-keyed monitor (0x000FF432–0x000FFDD8).
- Cross-region leads: event recorder 0x000C2696 is called from 0x000FC970/0x000FC9E8;
  0x000FC0CA/0x000FC0E2 are called from 0x00130300; getter family 0x000FB188 is consumed
  by 0x00102B12/0x0012C410/0x0012435C/0x000FF2D0; 0x000FCFBA is consumed by 0x00100AB4
  (all r10_app); cluster members 0x001018C8, 0x001240D8, 0x00101F84 live in r10_app and
  should be cross-checked there; the ramp helpers 0x000FD324–0x000FD60C are shared with
  the 0x11E000–0x12F000 area.
- Spec gap: no spec doc covers 0x000FB000–0x000FFFF (specs 09/11/14 cover 0xDE000–0xE3100,
  0x104C00–0x10CA00, 0x1052E8–0x106610). The gear-determination class here is the natural
  companion to spec 09/11 and the mode byte `*PTR_DAT_000ffdb4` (1..8 characteristic
  selection) is worth a spec 01/04 cross-check against transmission-variant coding.
