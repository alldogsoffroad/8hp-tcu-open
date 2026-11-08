# Unit 26 (r0c_app)

Slice 0x000C8D2A–0x000D07B0 of the r0c_app region (application). Clusters in unit-file order.
Region context: unit starts inside a table/callback-driven read-and-encode framework (0xC8Dxx),
then a text-table accessor (0xC91xx), a sliding-window linear-regression monitor state machine
(0xC92xx–0xC97xx), and continues into further application modules.

#### 0x000C8D2A `tbl_id_mode_valid` — logic
Validator taking (id, mode): mode 0 always passes; mode 1 passes only if callback PTR_FUN_000c8dd4(id)
returns >0 (not -1/0); any other mode fails. Called only by 0x000C8C4A within the same read framework.
Single-member cluster. [I]

#### 0x000C8D58 `tbl_status_bit_check` — logic
Three-mode check used by 0x000C8C06/0x000C8C4A: calls snapshot callback PTR_FUN_000c8dcc, then mode 0
returns "status bit 0x20 (from PTR_FUN_000c8dd0) clear", mode 1 returns it set, mode 2 returns
PTR_FUN_000c8dd8(2,buf,val)==0 on a 56-byte local buffer. Returns 0/1. Single-member cluster. [I]

#### 0x000C8DDC `diag_read_dispatch` — dispatch
Guarded read pipeline called from 0x000AA2FC (r08_app): requires enable flag *PTR_DAT_000c8f78 and
non-null out length/count pointers; validates sub-command via 0x000C8F88 (0xff = loop over the 2 handler
IDs in table PTR_DAT_000c8f84), resolves the entity via 0x000C8A70, validates args via 0x000C8C06, then
encodes via 0x000C901E, advancing the caller buffer/length. Status codes 0–5 (4=encode failed, 5=short
buffer) are remapped by the caller. Single-member cluster. [I] possibly the spec 10/15 read-by-ID path.

#### 0x000C8F88 `tbl_subcmd_valid` — logic
Accepts a sub-command byte iff it is 0xff or matches one of the two entries of list PTR_DAT_000c9178.
Returns 1/0. Called only by 0x000C8DDC. Single-member cluster. [O]

#### 0x000C8FB6 `tbl_delta_vs_ref` — logic
Collects values via callbacks PTR_FUN_000c917c/0x9180/0x9184/0x9188 (missing → 0) and returns the selected
current value minus reference byte struct[7]; the result feeds record byte 9 in 0x000C901E. Called by
0x000C901E. Single-member cluster. [I]

#### 0x000C901E `diag_encode_record` — logic
Encodes one record into the caller buffer: with param_1==1 it only reports lengths (10 bytes for type 1,
3 for type 2, written via param_8); otherwise type 1 builds a 10-byte record from struct fields +0x12,
+0x25, +6 plus delta byte from 0x000C8FB6, type 2 builds a 3-byte record with a decrementing counter and
a delta. Returns 4 on unknown type, 5 when the out-buffer is too short. Called by 0x000C8DDC.
Single-member cluster. [O] record layout, [I] purpose.

#### 0x000C9190 `tbl_rec19_addr` — library
Pure address arithmetic: base + 0x12 + (index-1)*0x13, i.e. field 0x12 of the (index-1)-th 0x13-byte
record. Caller 0x000AC48E clears 0x13-byte records through it. Single-member cluster. [O]

#### 0x000C919E `tbl_foreach_slot12` — logic
Invokes callback PTR_FUN_000c9250(param_1,param_2,slot) twice, for slot 1 and slot 2. Called by
0x000AC324 (r08_app). Single-member cluster. [O] structure, [I] "slots".

#### 0x000C91B8 `tbl_text_row_fetch` — logic
For table index <0x18: reads string IDs (shorts, stride 0x26) from the base returned by PTR_FUN_000c9254,
fetches each string through PTR_FUN_000c925c(id,&len) (NULL or len 0 → substitute 0x00), and copies the
bytes into the caller buffer up to param_2, padding with zeros; returns 0xffff if index >= 0x18. Callers
0x000AC48E. Looks like a 24x38 text/string ID table. Single-member cluster. [I]

#### 0x000C9260 `trend_mon_init` — logic
No static callers (pointer-table entry): resolves the monitor object via PTR_FUN_000c94ec(0xdc), sets
enabled (+8)=1, clears +0xb, applies configuration through 0x000C9560, then latches +0xb into +0x10.
Part of the sliding-window regression monitor (see 0x000C92C2–0x000C972C). Single-member cluster. [I]

#### 0x000C92A6 `trend_hold_clear` — logic
Clears monitor flag +10 and, if the monitor is enabled (+8==1), calls 0x000C95E8 to restart the fill
countdown and mark the object changed. Called by 0x000AEE62 (r08_app dispatch). Single-member cluster. [I]

#### 0x000C92B6 `trend_hold_set` — logic
Complement of 0x000C92A6: if enabled (+8==1) sets flag +10=1 (which 0x000C972C treats as a hold/abort of
result reporting). Called by 0x000AEE6A. Single-member cluster. [I]

#### 0x000C92C2 `trend_win_config` — logic
(Re)configures the regression window: rows clamped to 3..0x47 (param_2<3→3, >=0x48→0x47) and cols to
<=0x10; computes baseline +0x14 = rows*25-25, least-squares denominator +0x20 = Σ(i*50-baseline)²/50,
ring geometry +0x1c = cols*50+rows*25-25; keeps head +0xf consistent modulo 0x57 when shrinking an active
window and rebuilds the running sum +0x24 over the window; sets fill counter +0xb = rows+cols. Writes only
the caller object plus indirect calls. Single-member cluster. [O] arithmetic, [I] regression semantics.

#### 0x000C93C6 `trend_fit_eval` — logic
Evaluates the windowed least-squares fit: mean = sum(+0x24)*50/rows; accumulates Σ(dx_i*(y_i*50-mean))
over the ring starting at head +0xf (mod 0x57); on integer overflow reports through
PTR_FUN_000c94fc(PTR_DAT_000c94f8,0x29a,0xf,...) (fault/assert with file-ish args), sets state byte 0=4
and returns 0x7fff; otherwise slope*denominator → fitted value at +4 via PTR_FUN_000c94f4, returns
current(+2) minus fit. Single-member cluster. [I]

#### 0x000C949C `trend_state_transition` — logic
State-machine gate: with confirm flag set, a transition state→new is allowed only if the 6x6 matrix
PTR_PTR_000c9500[state*6+new] is non-zero, then applies 0x000C9560(obj,new) and latches +8=new; otherwise
requires state==new, forces +8=1 and applies state 1. Callers 0x001039C0, 0x001039F0, 0x000CE62A.
Single-member cluster. [O] matrix logic, [I] state meanings.

#### 0x000C9504 `trend_state_active2` — data-accessor
Predicate: monitor object is in requested state (+8) AND status byte 0 == 2. Callers 0x000CF700,
0x00103A1E, 0x000CE62A, 0x000B4B56, 0x000B4D0C. Single-member cluster. [O]

#### 0x000C9518 `trend_src_mode_update` — logic
Updates input-source mode +9 from state +8: state 1 → PTR_FUN_000c9750(config *PTR_DAT_000c9748,
*PTR_DAT_000c974c); states 2/3/5 → 1; other states leave it; if the mode changed, calls 0x000C95E8 to
mark the object changed. Called by 0x000C95F4. Single-member cluster. [I]

#### 0x000C9560 `trend_state_apply_cfg` — logic
Applies a state's configuration: states 3/4/5 reconfigure the window from config fields +4/+8 (config
block *PTR_DAT_000c9748); state 1 may set hold flag +10=1 based on globals PTR_DAT_000c9754[0x2b..0x2c]
or PTR_FUN_000c975c(PTR_DAT_000c9758), window again +4/+8; state 2 uses config +0xc/+0x10. Always ends
by calling 0x000C92C2 with the chosen rows/cols, latching +0xe=+0xb and setting changed flags (+1=1, 0=1).
Callers 0x000C949C, 0x000C9260. Single-member cluster. [I]

#### 0x000C95E8 `trend_mark_restart` — data-accessor
Three-field mutator: +0xe = +0xb (restart fill countdown), +1=1, byte 0=1. Called from 0x000C92A6,
0x000C9518, 0x000C972C. Single-member cluster. [O]

#### 0x000C95F4 `trend_step_sample` — logic
Periodic step called by 0x000C9298: first 0x000C9518 (refresh source mode), then the phase byte +1
(1 → 2 once fill countdown +0xe hits 0), samples the selected input via 0x000C9632 into +2, then pushes
it into the ring via 0x000C96AE; returns 0 when idle. Single-member cluster. [I]

#### 0x000C9632 `trend_input_select` — logic
Selects the monitor input by mode +9: mode 0 → PTR_FUN_000c9764 over PTR_DAT_000c9760, mode 1 → same
over PTR_DAT_000c9768, otherwise a product formula ((PTR_DAT_000c9770+0x90) * PTR_FUN_000c9778(
PTR_DAT_000c9774, *PTR_DAT_000c976c))/1000 + (+0x92), halved. Result is then passed through
PTR_FUN_000c9780(new, previous +2, cfg +0x14, +0x18) — a rate/slew limiter. Single-member cluster. [I]

#### 0x000C96AE `trend_ring_push` — logic
Pushes one sample into the 0x57-entry short ring at +0x2c: subtracts the outgoing head value from the
running sum +0x24, writes the new value at (head+count) mod 0x57, advances head mod 0x57, decrements fill
countdown +0xe when non-zero. Mutates only the caller object. Single-member cluster. [O]

#### 0x000C972C `trend_poll_result` — logic
Result poller: state 1 returns the phase byte (unless already 2); state 2 → if hold flag +10 set, promotes
to state 3 and returns 1, otherwise evaluates the fit via 0x000C93C6 and stores it at +6; state 3 returns
+10 or recomputes as state 2; state 4 resets via 0x000C95E8. Called by 0x000C9298. Single-member cluster. [I]

#### 0x000C97B4 `tblcoord_get_or_alloc` — logic
Returns param_1 or allocates a 0xc-byte object via PTR_FUN_000c9a7c(0xc) when NULL, stores the dims-spec
pointer param_3 at offset 0, then initializes the coordinate fields via 0x000C97E2(obj, param_2). Callers
0x000B0E44, 0x000B0860 (r08_app). Single-member cluster. [I]

#### 0x000C97E2 `tblcoord_decompose` — library
Decomposes a 1-based linear index against a dims spec (bytes [0]+1 x [1]+1 per plane): writes column
(+6) = (idx-1 mod product) mod (cols+1), row (+7) = that / (cols+1), plane (+9) = (idx-1)/product, and a
clamped plane+1 byte at +2 via PTR_FUN_000c9a8c; idx<1 zeroes the fields and sets +2=1. Division/modulo
through callbacks PTR_FUN_000c9a80..0x9a8c. Single-member cluster. [O] math, [I] table use.

#### 0x000C9888 `tbl_decay_kernel_apply` — logic
Walks outward ring-by-ring (up to 14 rings, 4-bit edge mask bStack_44 clearing directions 1/2/4/8 at grid
borders) around a cell of the 2D grid described by *param_2 (rows byte0, cols byte1, plane stride short+4,
byte +9); for each ring computes an amplitude via 0x000C9AFC and issues vtable method +0x14(context,
cellIndex, amplitude, width, plane) for the four half-rows/columns. Called by 0x000B0860; part of the
table-blending engine with 0x000C97B4/0x000C9C02. Single-member cluster. [I]

#### 0x000C9AFC `lib_fixp_pow_scaled` — library
Computes (param_3/100)^param_1 in Q15 (x = param_3<<15/100, exponentiation by squaring), scales param_2 by
it sign-preservingly, and saturates to short through PTR_FUN_000c9d10; returns param_2 unchanged for
exponent 1. Single-member cluster. [O]

#### 0x000C9B64 `tbl_lerp2_eval` — library
Two-stage linear interpolation over a 10-short descriptor (x/y breakpoints at [0..3], second-stage [4..9])
in Q7 fixed point; multiplies by 10 when param_2 != 0; saturates via PTR_FUN_000c9d10. Called by
0x000B0E44. Single-member cluster. [O]

#### 0x000C9C02 `tbl_fetch_corners4` — logic
From the coordinate object (col +6, row +7, plane +9 over dims *param_2) computes the 4 interpolation
corner indices with border corrections (rows/cols 0 and max fold inward), then fetches each corner value
through vtable method +0x14(context, saturated index) into param_1[0..3] (ushorts). Called by 0x000B0E44.
Single-member cluster. [I]

#### 0x000C9D18 `strat_step_current` — logic
Steps the currently selected strategy object (global DAT_fff8ad88): reads its state via vtable +0x68; state
0 → start handler chain (+0x20/+0x24 slots), state 1 → +0x3c slot, states 2/3 → refresh (+0x38) then find
the next matching strategy via 0x000C9F2C and re-point DAT_fff8ad88 (index 10 = none). Part of the
10-entry strategy table PTR_PTR_000c9fac. Single-member cluster. [I]

#### 0x000C9DC6 `strat_call_vt50` — logic
Guarded thunk: if a strategy object is selected (DAT_fff8ad88 != 0) calls vtable method +0x50 with context
+0x48 on it. Member 0x000C9E30 is the same pattern with vtable +0x14/context +0xc (different method, so
differently named). Cluster of 2. [O]

#### 0x000C9DDE `strat_reset_all` — logic
Resets all 10 strategy objects: vtable +0x38 (context +0x30) and state byte +6 = 0 for each entry of
PTR_PTR_000c9fac; then selects the first matching strategy via 0x000C9F6C into *param_1 / DAT_fff8ad88
(10 → NULL). Single-member cluster. [O]

#### 0x000C9E48 `strat_idx_valid` — data-accessor
Returns *param_1 < 10 — is a strategy index valid. Callers 0x000AEF78, 0x000C9EA2. Single-member cluster. [O]

#### 0x000C9E54 `strat_all_matched_state2` — logic
Scans all 10 strategy objects; returns 0 as soon as one whose mode matches (0x000C9FB0 on vtable +0x5c
value) is not in state 2, else 1 after the full scan. Called by 0x000AEF78. Single-member cluster. [O]

#### 0x000C9EA2 `strat_any_matched_state3` — logic
When the current index is invalid: scans the 10 strategies and returns 1 on the first matching one in
state 3, else 0. Called by 0x000AEF78. Single-member cluster. [O]

#### 0x000C9EF8 `strat_get_obj` — data-accessor
Returns PTR_PTR_000c9fac[idx] when idx < 10, else 0. Callers 0x000AF2F8/0x000AF334/0x000AF3A8/0x000AF282/
0x000AF2BC/0x000AF22C. Single-member cluster. [O]

#### 0x000C9F12 `strat_get_field5` — data-accessor
Returns the sign-extended byte at strategy object +5 for idx < 10, else 0. Callers 0x000AF266, 0x000AEF78.
Single-member cluster. [O]

#### 0x000C9F2C `strat_find_next` — logic
Searches strategies *param_1+1 .. 9 for the next one whose mode matches (0x000C9FB0 with vtable +0x5c
value); returns the index, or >9 when none. Called by 0x000C9D18. Single-member cluster. [O]

#### 0x000C9F6C `strat_find_first` — logic
Like 0x000C9F2C but scanning indices 0..9. Called by 0x000C9DDE. Single-member cluster. [O]

#### 0x000C9FB0 `strat_mode_match` — logic
Match predicate: PTR_FUN_000c9fd4(config *PTR_DAT_000c9fd0 + 0x44, id-1, value) != 0. Shared by the
strategy scanners. Callee 0x000B6CBC (r08_app). Single-member cluster. [O]

#### 0x000C9FD8 `seq_pair_setup` — logic
Sequencer setup: clamps two gear-like params to 0..8, maps them through curve PTR_FUN_000ca230 into +8/+7,
sets ordering flag +0x22 = (+7 < +8), derives +0x1d/+0x1e from table PTR_DAT_000ca238 via
PTR_FUN_000ca23c/0x240 on the scaled pair, sets +0x1f=1, +0x1c=param_4, then issues three module-begin
calls (PTR_FUN 0xca248/0xca250/0xca258). Called by 0x000AED02. [I] possibly gear-pair thresholds (spec 09/11).

#### 0x000CA07A `seq_reconfig` — logic
Reconfiguration variant of 0x000C9FD8: sets enable byte +3 from (param_5||param_4), begins modules
PTR_FUN_000ca25c and DAT_000ca260(+cfg, enable, param_6), re-clamps the 0..8 pair into +8/+7, refreshes
+0x22/+0x1d/+0x1e, sets request byte +2=1 and +0x1f=0. Called by 0x000AED3A. Single-member cluster. [I]

#### 0x000CA148 `seq_permitted` — stub
Tail wrapper: 0x000CA154(param_1, 0) — permission gate with default flags. Callers 0x000B18E0, 0x000CF700,
0x000CF48C, 0x000B2378, 0x000B5FCC, 0x000CFAEC, 0x000CE62A, 0x000B4B56, 0x000AECF4, 0x000B2028.
Single-member cluster. [O]

#### 0x000CA154 `seq_permitted_flags` — stub
Tail wrapper: 0x000CA65C(param_1, param_2, 0). Callers 0x000CA148, 0x000AFFA4, 0x000B0708, 0x000AFB66.
Single-member cluster. [O]

#### 0x000CA162 `seq_req_matches` — logic
Predicate: param_3 == 1 && param_2 == param_1+6 (request field equals the sequencer's current command).
Called by 0x000AECC0. Single-member cluster. [O]

#### 0x000CA176 `seq_step_poll` — logic
Main sequencer poll (called by 0x000AEE72): state byte +6 == 0x10 latches done (+0x28); otherwise, per
state value (4, 0x0b, 0x12..0x19, 0x20, 0x21, 0x28, 0x29, 0x55, 0x72..0x76, 0x75, 0x74, 0x73) starts the
corresponding sub-module (paired PTR_DAT/PTR_FUN begin calls), or, when running (+0x28), steps sub-objects
via counter *PTR_DAT_000ca274 and their vtable +0x38/+0x14; finishes via 0x000CA6D4 which watches a command
byte and dispatches a 19-case jump table. Single-member cluster. [I] candidate spec 11 phase engine.

#### 0x000CA3A6 `seq_stop_all` — logic
Shutdown: 0x000CA822() plus seven module-end callbacks (PTR_FUN 0xca4d8..DAT_000ca500). Called by
0x000AEE7A. Single-member cluster. [O]

#### 0x000CA56A `seq_set_targets` — logic
Refreshes change flag via 0x000CA7F8 then stores target pairs: raw param_2/param_3 at +0xc/+0x10 and their
/10 values at +0x14/+0x18, flags param_4/param_5 at +5/+4. Called by 0x000AECD8. Single-member cluster. [I]

#### 0x000CA59A `objflag22_clear` — stub
Single store: *(obj+0x22) = 0 (clears the sequencer ordering/abort flag set by 0x000C9FD8/0x000CA07A).
Cluster of 12 byte-equivalent copies spread across regions (0x000CA59A, 0x000CB8EC, 0x000CD90E, 0x000D325C,
0x000D3264, 0x000D924C, 0x000E1DF8, 0x000E958A, 0x000EE2B6, 0x000F05FA, 0x00109B72, 0x0014DCA0) — one
per-module accessor, all named the same. [O]

#### 0x000CA5A2 `seq_request_restart` — stub
Single store: *(obj+2) = 1, the request byte consumed by 0x000CA07A/0x000CA176. Called by 0x000AED22.
Single-member cluster. [O]

#### 0x000CA5A8 `lib_pct_scale_curve` — library
Percentage scaling: curve lookup PTR_FUN_000ca738(param_3), multiply by param_2, divide by 100, saturate
via PTR_FUN_000ca73c. Callers 0x000CF1EC, 0x000B0860. Single-member cluster. [O]

#### 0x000CA5C8 `seq_query_slot5` — logic
For slot param_2 in 1..5: gate on table entry PTR_FUN_000ca744(module PTR_DAT_000ca740 cfg+0x18, slot-1)
then run PTR_FUN_000ca74c(PTR_DAT_000ca748, slot); returns 0 outside 1..5. Callers 0x000B18E0, 0x000B3FA2.
Single-member cluster. [I]

#### 0x000CA60A `seq_stop_submods4` — logic
Issues four module-end callbacks (PTR_FUN_000ca754 twice with PTR_DAT_000ca750/0x758/0x75c and
PTR_FUN_000ca764). Companion of 0x000CA3A6 (called by the neighbouring thunk 0x000AEE82).
Single-member cluster. [O]

#### 0x000CA624 `seq_gate_ok2` — logic
Returns 1 unless (obj+0x24 set OR module PTR_DAT_000ca740 field +4 != 1) AND table lookup
PTR_FUN_000ca744(cfg+0x10, *PTR_DAT_000ca768) is non-zero. Callers 0x000B6260, 0x000B5FCC, 0x000B4B56.
Single-member cluster. [O]

#### 0x000CA65C `seq_permitted_check` — logic
Permission gate behind 0x000CA148/0x000CA154: requires globals PTR_DAT_000ca76c[2]!=0, [0xb5]==0,
*PTR_DAT_000ca770!=1, callback 0xca778==0, callback 0xca780!=1, not (PTR_DAT_000ca784==1 and module+0x14
field==1), 0x000CA90E clear, and callback 0xca78c non-zero; result latched to obj+0x21 and returned.
Callers 0x000CA154, 0x000AF43E, 0x000B31D8. Single-member cluster. [O] conditions, [I] purpose.

#### 0x000CA6D4 `seq_cmd_dispatch19` — dispatch
Watches the command byte of module object *PTR_DAT_000ca740 (change vs *PTR_DAT_000ca790 while the
sequencer command obj+6 is idle) and dispatches the new value <0x13 through jump table
switchD_000ca70c (19 cases). Called by 0x000CA176. Single-member cluster. [O]

#### 0x000CA7F8 `seq_targets_changed` — logic
Change detector for the target pair: reads c = PTR_FUN_000ca934(PTR_DAT_000ca930); if armed (+0x1f!=0)
and +0x20 != c, promotes +0x1f to 2; latches +0x20 = c. Called by 0x000CA56A. Single-member cluster. [O]

#### 0x000CA822 `seq_timer_gate_update` — logic
Maintains a hold value at obj+0x2c (short from PTR_DAT_000ca938+0x11c times 300, recomputed on trigger
byte [0x120]==2), stores the /300 short at +0x26, runs a 3-state machine on +0x24 driven by
PTR_DAT_000ca938[0x50] and clamp curve PTR_FUN_000ca948, and finishes with the 0x000CA624-style gate into
+0x23. Called by 0x000CA3A6. Single-member cluster. [I]

#### 0x000CA90E `seq_in_state1_10` — logic
Returns 1 when PTR_DAT_000ca938[0xe8] is 1 or 0x10; when true and param_2 given, sets bit 0 of
param_2[3]. Used by the 0x000CA65C permission gate. Single-member cluster. [O]

#### 0x000CA954 `seq_req_ack_step` — logic
Two-phase request/ack byte machine on obj[0..4]: (arm[3] set and idle[0]==0) → clear arm, set run[2],
pulse *PTR_DAT_000cab9c=1; (run[2] and done[4] and [1]==0) → clear both. No static callers (vtable
method). Single-member cluster. [I]

#### 0x000CA986 `subobj_clear_pair` — logic
Invokes shared clear/reset function *DAT_000caba0() twice — once bare and once with param_1+1 (arming
the two halves of a 5-byte sub-object pair). Callee 0x000BAD64. Called by 0x000CAE06. Single-member
cluster. [I]

#### 0x000CA99A `obj44_get_or_init` — logic
Returns param_1 or allocates a 0x2c-byte object via *DAT_000caba4(0x2c) when NULL, storing param_2 at
offset 0 (the module/config vtable). Same get-or-alloc pattern as 0x000C97B4. Single-member cluster. [O]

#### 0x000CA9B8 `shift_phase_init` — logic
Initializes a 0x2c-byte phase object for channel param_2, type param_3: zeroes the state bytes, selects
parameter family A (type 1: global *_LAB_000caba8=0, parent+0x9a output pointer, curve ctx+0x10) or B
(type 2: global=1, parent+0x9c, ctx+0x18, config +0x24) or reports fault 0x25d and marks invalid; then
loads five curve parameters (ctx+0xc/+0x10/+0/+8/+4) indexed by channel-1 into +0x12/+0x14/+0x16/+0x18/
+0x1a. No static callers (vtable init). Matches the spec 11 pressure parameter-family structure. [I]

#### 0x000CAE06 `subobjs_reset3` — logic
Reset helper: PTR_FUN_000caeb4(param_1+0xe) then 0x000CA986 on sub-objects at +4 and +9. Called by
0x000B2ECA. Single-member cluster. [I]

#### 0x000CAE20 `shift_phase_dispatch` — dispatch
Dispatches on the phase type byte at obj+4: type 0 → fault 0x89, ret 3; type 1 → accumulate
0x000CB188(0) delta into the parent+0x9a output or promote family state; types 2/3/4 → the steppers
0x000CAF74/0x000CAFD0/0x000CB064; default → 0x000CB080 (fault 0xee). Unexpected family states report
fault 0x9c via PTR_FUN_000cb17c(PTR_DAT_000cb178,0x25d,...). No static callers (vtable method).
Single-member cluster. [I] spec 11 phase engine.

#### 0x000CAEE0 `shift_phase_abort3` — logic
Fault variant: reports (PTR_DAT_000cb178, 0x25d, 3, obj+0xac, +0x94, +0x9e, +0x95) through
PTR_FUN_000cb17c then invokes PTR_FUN_000cb180 on the parent. No static callers (vtable). Single-member
cluster. [O]

#### 0x000CAF74 `shift_phase2_steps` — logic
Type-2 phase stepper on parent state +0x96: 1→2, 2→3 with delta accumulation into *obj[9] via
0x000CB188 and clearing tick counter +0x98, 3→4, 4 sets done flag +0xaa. Uses unaffiliated r12 (object
passed in register by the dispatcher). Single-member cluster. [I]

#### 0x000CAFD0 `shift_phase3_steps` — logic
Type-3 phase stepper: states 1/2 report fault 0xca (code 5) and stop; states 3/4 use a 2-tick counter at
+0x98, accumulating 0x000CB188 delta every second tick and returning to state 3. Single-member cluster. [I]

#### 0x000CB064 `shift_phase4_accum` — logic
Type-4 phase: unconditionally accumulates the 0x000CB188(0) delta into *obj[9] and sets family state
+0x96 = 1. Single-member cluster. [O]

#### 0x000CB080 `shift_phase_fault_ee` — logic
Default phase branch: reports fault (0x25d, 6, ..., 0xee) and stops via PTR_FUN_000cb180. Called only by
0x000CAE20. Single-member cluster. [O]

#### 0x000CB0D8 `shift_threshold_monitor` — logic
No static callers (vtable method): compares shorts at obj+0x12/+0x18/+0x14/+0x1a against the live value
*(short*)(PTR_DAT_000cb184+6), setting direction flags at +0x1c/+0x1d/+0x1e (1=below, 2=above); clamps an
accumulator at +0x20 to the live value and steps +0xe = +0x2a+1 with hysteresis check at +0x16/+0x22.
Single-member cluster. [I]

#### 0x000CB188 `shift_family_param_read` — logic
Reads one parameter for the active family: family flag parent+0x95 selects table pair
(PTR_DAT_000cb274 / PTR_DAT_000cb278); mode 0 → curve lookup PTR_FUN_000cb27c(ctx, parent+0x9e-1),
modes 1..3 → the shorts at ctx+4/+8/+0xc (or +0xc/+0x10/+0x14); other modes report fault 0x67 (code 1)
and return 0. Callers 0x000CAF74/0x000CAFD0/0x000CB064/0x000CAE20. Single-member cluster. [I] spec 11
pressure parameter families.

#### 0x000CB294 `lever_map_state5` — logic
Maps RAM global DAT_fff8e695 (0→0, 1/2/3→1, 4→*param_1, other→fault 0x27d/0xf and skip) into param_1[1],
then always marks *param_1 = 0xff. Called by 0x000B26FE. Consistent with a lever/range input decoder
(spec 14). Single-member cluster. [I]

#### 0x000CB2F6 `timer_bump_mode34` — logic
For mode 3 adds 100 (mode 4 adds 1) to the counter behind PTR_FUN_000cb364/0x36c/0x370 handle
PTR_DAT_000cb368 and writes it back via PTR_FUN_000cb374, then triggers the module behind
PTR_PTR_000cb380. Other modes return immediately. Called by 0x000B26FE. Single-member cluster. [I]

#### 0x000CB348 `counter_get` — data-accessor
Reads the 16-bit counter via PTR_FUN_000c000cb36c(PTR_DAT_000cb368). Called by 0x000AEA7A.
Single-member cluster. [O]

#### 0x000CB396 `obj24_get_or_init` — logic
Get-or-allocate constructor: param_1 or a 0x18-byte object from PTR_FUN_000cb630(0x18); stores descriptor
DAT_000cb634 at +0 and param_2 at +4. Same family as 0x000CA99A/0x000CC10E. Single-member cluster. [O]

#### 0x000CB3B8 `shift_famA_target_dev` — logic
Returns (family-A output short at parent+0x9a) − curve value PTR_FUN_000cb63c(PTR_DAT_000cb638, obj+0x13,
parent) plus the short behind PTR_PTR_000cb640→+4→+0x28. No static callers (vtable method).
Single-member cluster. [I]

#### 0x000CB3E6 `shift_famB_target_dev` — logic
Family-B twin of 0x000CB3B8 using the output short at parent+0x9c and curve PTR_FUN_000cb644, without the
extra chained term. Single-member cluster. [I]

#### 0x000CB7EC `shift_fam2_setpoint_apply` — logic
Computes a clamped setpoint for channel obj+0x13 (family 2): lower-bounds curve(ctx+0x1c, ch-1) by
curve2(PTR_DAT_000cb928, ch) − width(PTR_DAT_000cb930, ch), clamps (param_2 + live short at
PTR_PTR_000cb944→+4→+8) into [curve(ctx+0x18,ch-1), sVar5], subtracts the centre, clamps to ±sVar8 from
family call PTR_FUN_000cb93c(parent+4, ch, 2), then applies via PTR_FUN_000cb94c(...) and writes it with
PTR_FUN_000cb950(PTR_DAT_000cb928, ch, value). No static callers (vtable). Single-member cluster. [I]

#### 0x000CB8B8 `shift_slot_bitmask_set` — logic
For slot param_2 in 1..5 gated by table entry PTR_FUN_000cb958(module PTR_DAT_000cb908 cfg+0x28, slot-1):
stores the single-bit mask 1<<slot into obj+0x14. No static callers (vtable). Single-member cluster. [O]

#### 0x000CB9A6 `obj12_get_or_init` — logic
Get-or-allocate constructor for a 0xc-byte object (PTR_FUN_000cbc2c), descriptor DAT_000cbc30 at +0,
param_2 at +4. Single-member cluster. [O]

#### 0x000CB9C8 `shift_famA_dev2` — logic
Returns (family-A output short at parent+0x9a) − curve PTR_FUN_000cbc38(PTR_DAT_000cbc34, obj+9).
Member 0x000CB9EA uses parent+0x9c and curve PTR_FUN_000cbc3c instead (family B), hence a different name.
Cluster of 2. [I]

#### 0x000CBD0E `shift_fam1_setpoint_apply` — logic
Family-1 twin of 0x000CB7EC: curve centre PTR_FUN_000cbdfc(PTR_DAT_000cbdf8, obj+9), width from
PTR_FUN_000cbe04(parent+4, obj+9, 1)/PTR_FUN_000cbe08(ctx+0x18), input param_2 + 10*char from
PTR_PTR_000cbe0c chain, clamped between curves (ctx+0x14/ctx+0x10) via PTR_FUN_000cbe14, offset by centre,
clamped by −width, applied via PTR_FUN_000cbe1c(...,1) and PTR_FUN_000cbe20. No static callers (vtable).
Single-member cluster. [I]

#### 0x000CBE28 `iter_find_state0` — logic
Scans the row/col matrix iterator of the object: 0x000CC04E init, then while not (0x000CC026 cell-selected
&& state==0 via vtable+0x5c) advance with 0x000CC07C; returns 0 when exhausted, 1 when a matching cell was
found. No static callers (vtable). Single-member cluster. [I]

#### 0x000CBE68 `iter_all_done2` — logic
Like 0x000CBE28 but scans until a selected cell with state != 2 (0x000CBFFE); returns 1 only when the
whole matrix was exhausted (all cells state 2), 0 when such a cell was found. Single-member cluster. [I]

#### 0x000CBEA8 `iter_restart_state1` — logic
Walks every cell: when vtable+0x20 (cell state) == 1, re-invokes vtable+0x14(ctx, 0) on it; advances via
0x000CC07C until exhausted. Single-member cluster. [I]

#### 0x000CBEDC `iter_start_all` — logic
Walks every cell unconditionally invoking vtable+0x14(ctx, 0). Single-member cluster. [I]

#### 0x000CBF9A `iter_cell_selected` — logic
Cell-selection predicate: the vtable+0x2c value (masked byte) in 1..5 and the family-specific table entry
(obj+2 selects table +0x40/+0x44 of *_LAB_000cc074) non-zero. Shared by 0x000CBFFE/0x000CC026.
Single-member cluster. [I]

#### 0x000CBFFE `iter_cell_not_done` — logic
Selected (0x000CBF9A) AND vtable+0x20 state != 2. Called by 0x000CBE68. Single-member cluster. [O]

#### 0x000CC026 `iter_cell_idle` — logic
Selected (0x000CBF9A) AND vtable+0x20 state == 0. Called by 0x000CBE28. Single-member cluster. [O]

#### 0x000CC04E `iter_init` — data-accessor
Initializes the 3-byte iterator state: [0]=0 (row), [1]=0 (col), [2]=1 (sub-tick). Single-member cluster. [O]

#### 0x000CC07C `iter_next` — logic
Advances the iterator: sub-tick [2] cycles 1..3; on wrap the column [1] increments up to the vtable+0x50
limit, then the row [0] up to the vtable+0x44 limit; returns 0 when the matrix is exhausted, 1 otherwise.
Callers 0x000CBEA8/0x000CBE68/0x000CBE28/0x000CBEDC. Single-member cluster. [O]

#### 0x000CC10E `obj52_get_or_init` — logic
Get-or-allocate constructor for a 0x34-byte object (PTR_FUN_000cc394): vtable PTR_LAB_000cc398 at +0,
param_2 at +4, and an extra pointer PTR_LAB_000cc39c at +0x24. Single-member cluster. [O]

#### 0x000CC3D8 `quad_enable_set` — data-accessor
Stores param_2 into *PTR_DAT_000cc550 and the three sibling module globals PTR_DAT_000cc554/0x558/0x55c.
Called by 0x000AEF78. Single-member cluster. [O]

#### 0x000CC3EA `quad_begin_mode` — logic
Begins the four sub-modules: PTR_FUN_000cc560(0x554), 0x564(0x550, mode), 0x568(0x558), 0x56c(0x55c).
Callers 0x000AEF78, 0x000CC40C. Single-member cluster. [O] call structure, [I] lifecycle role.

#### 0x000CC40C `quad_begin0` — stub
Tail wrapper: 0x000CC3EA(param_1, 0). Called by 0x000B3370. Single-member cluster. [O]

#### 0x000CC410 `quad_step_all` — logic
Invokes the second method group on all four sub-modules (PTR_FUN 0x570/0x574/0x578/DAT_000cc57c).
Called by 0x000AEF78. [I] step role inferred from dispatcher ordering.

#### 0x000CC42C `quad_stop3` — logic
Invokes a method subgroup on three of the four modules (PTR_FUN 0x580 on 0x558, 0x584 on 0x55c, 0x588 on
0x554). No static callers (vtable thunk). [I]

#### 0x000CC442 `quad_end_all` — logic
Invokes the third method group on all four sub-modules (PTR_FUN 0x58c/0x590/0x594/0x598). Called by
0x000AEF78. [I] end role inferred from dispatcher ordering.

#### 0x000CC45E `quad_all_ok4` — logic
Four-stage chained check: PTR_FUN_000cc59c(0x554) → 0x5a0(0x550, prev) → 0x5a4(0x558) → 0x5a8(0x55c, prev);
returns 1 only when every stage is non-zero. Callers 0x000AF2F8, 0x000AEF78, 0x000AF334, 0x000AF3A8,
0x000AF282, 0x000B33A6, 0x000AF2BC, 0x000AF22C. Single-member cluster. [O]

#### 0x000CC4A2 `quad_all_ok3` — logic
Three-stage variant: 0x59c(0x554) → 0x5a4(0x558, prev) → 0x5a8(0x55c); returns 1 when all non-zero.
Callers 0x000B33BE, 0x000AEF78. Single-member cluster. [O]

#### 0x000CC4DA `quad_status_latch4` — logic
Chained status collection over the four quad sub-modules (0x554/0x558/0x55c/0x550 via PTR_FUN 0x5ac/0x5b4/
0x5b8/0x5bc); any non-zero stage latches a code into *PTR_DAT_000cc5b0 (0x18, 0x17, 0x15) and the OR is
returned. Callers 0x000B33E8, 0x000AEF78. Single-member cluster. [O]

#### 0x000CC52E `quad_state_sync_chk` — logic
Consistency check on the quad state object PTR_DAT_000cc5c0: byte[0x2b] must be 1 and equal byte[0x2c],
otherwise latches code 0xd at PTR_DAT_000cc5b0 and returns 1. Called by 0x000CC5C6. [O]

#### 0x000CC5C6 `quad_module_fsm` — dispatch
State machine over a quad module object (state byte +1), driven by jump table switchD_000cc5f0 through
states 1..10; invokes module method pointers 0x748-0x764 on PTR_DAT_000cc744/0x75c. State 8 terminates
(state 9, substate 0); failure paths set state 11 (0xb); state 7 clears bit 2 of param_2+3 on completion.
Called by 0x000CC3EA (quad_begin_mode). [O] structure, [I] state semantics.

#### 0x000CC6C8 `quad_state_in_7_10` — data-accessor
Predicate: module state byte at +1 is 7 or 10. Referenced from PTR_FUN tables used by 0x000CC45E
(quad_all_ok4) and 0x000CC4A2 (quad_all_ok3). Single-member cluster. [O]

#### 0x000CC6DA `quad_state_in_3_7` — data-accessor
Predicate: module state byte at +1 strictly between 2 and 8 (3..7). Called by 0x000B39B8. [O]

#### 0x000CC6F0 `quad_state_is_11` — data-accessor
Predicate: module state byte at +1 == 11 (0xb), the abort state set by quad_module_fsm failure paths.
Called by 0x000B39B8, 0x000CC4DA. [O]

#### 0x000CC6F8 `quad_state_clear` — stub
Clears the module substate (+2) and state (+1) bytes to zero. Called by 0x000CC442 (quad_end_all). [O]

#### 0x000CC700 `quad_state_restart` — logic
State transition byte +1: 0 -> 1, 7 -> 4, all other states left untouched. Called by 0x000CC410
(quad_step_all). Single-member cluster. [O]

#### 0x000CC716 `quad_state_set8` — stub
Stores 8 into the module state byte +1 (stop request feeding quad_module_fsm state 8). Called by
0x000CC42C (quad_stop3). Single-member cluster. [O]

#### 0x000CC71C `quad_sync_mismatch_chk` — logic
Checks quad object PTR_DAT_000cc768: bytes [0x21] and [0x27] and param_1+2 must all agree; mismatch
latches 0xff at *PTR_DAT_000cc76c and returns 1. Called from quad_module_fsm state 7. [O]

#### 0x000CC838 `geareng_handshake_edge` — logic
Edge-triggered handshake over a 5-byte flag struct {0..4}: armed(3)&&!started(0) -> clear 3, set 2, and
set global *PTR_DAT_000ccaa0=1; pending(2)&&req(4)&&!busy(1) -> clear 4 and 2. Called by 0x000CC9EE,
0x000CDD6C, 0x000CD978 (geareng family). [O]

#### 0x000CC86A `timer_u8_pair_tick` — logic
Ticks two byte timers at param_1 and param_1+1 by calling DAT_000ccaa4 (= 0x000BAD64 timer_u8_left_tick)
on each. Called by 0x000CCC2E. [O]

#### 0x000CC87E `obj30_get_or_set` — logic
Returns param_1, or allocates 0x30 bytes via DAT_000ccaa8 when null; stores param_2 as the first word.
Called by 0x000B3A90. Single-member cluster. [O]

#### 0x000CC89C `geareng_ctrl_init` — logic
Initializes the gear-engagement controller object (zeros ~20 flag/short fields, +0x22 = 0x7fff) from the
config block *param_1: mode byte +0x89 (1/2 select different PTR_FUN/PTR_DAT sets, anything else reports
fault component 0x24c), channel +0x94, and caches initial positions/shorts via callbacks. Called by
0x000B4046 (geareng_setup_gear_request). [O]

#### 0x000CC9EE `geareng_ctrl_step` — dispatch
Main engagement FSM at +0x11 (states 0..6): state 1 runs sub-FSMs 0x000CCEEC/0x000CD086 and derives a
phase value 0..2 from status bytes; state 2 selects a phase byte from PTR_LAB_000ccadc[(cfg+0x8b)*3+phase],
executes it via 0x000CCC50 and saturates the target short at param_1[10]; invalid states report fault
0x24c codes 0xa8/0xda. Called by 0x000B45E0 (seq_cyclic_main). [O]

#### 0x000CCC2E `geareng_ctrl_reset` — logic
Reset helper: fnptr calls at +0x10/+0xe (PTR_FUN 0xcd80/0xcd84) and ctor pair ticks (0x000CC86A) at +4
and +9. Called by 0x000B492C (seq_reset_timers_subobjs). Single-member cluster. [O]

#### 0x000CCC50 `geareng_phase_exec` — dispatch
Executes the phase byte at +0x13 (0..4): phase 1-2 advance the cfg+0x8b stage machine (1..4) updating the
target short via 0x000CD124 and status +0x21; phase 3 ticks a 2-count counter; phase 4 finalizes and, for
cfg+0x88==6, invokes PTR_FUN_000cd000. Fault codes 0x53/0x66/0x96/0xc8 via component 0x24c. Called by
0x000CC9EE. [O]

#### 0x000CCEEC `geareng_substep_fsm` — logic
Sub-state machine at +0x12 (0..5): state 0 arms when a fnptr check passes and cfg +0x74/+0x7e are 4/5;
states 1-3 compare the position short +0xe against module thresholds (PTR_DAT_000cd008/+0x4c/+0x50/
+0x54) and step the counter; invalid states report fault 0x24c code 0x3b. Called by 0x000CC9EE. [O]

#### 0x000CD086 `geareng_thresh_monitor` — logic
Monitors position shorts +0x14/+0x1a/+0x16/+0x1c/+0x22 against the reference short at PTR_DAT_000cd228+6,
latching direction status bytes at +0x1e/+0x1f/+0x20 (values 1/2) used as phase inputs by 0x000CC9EE.
Called by 0x000CC9EE. Single-member cluster. [O]

#### 0x000CD124 `geareng_param_fetch` — logic
Fetches an s16 parameter by selector param_2 (0..3) from mode-dependent table objects (mode 1 via
PTR_DAT_000cd22c, mode 2 via PTR_DAT_000cd230) indexed by channel (cfg+0x94)-1; any other selector
reports fault 0x24c code 0x32. Called by 0x000CCC50, 0x000CC9EE. [O]

#### 0x000CD238 `obj12_get_or_set` — logic
Returns param_1 or allocates 0xc bytes via PTR_FUN_000cd424; stores param_2 as the first word. Called by
0x000B3A90. Two-member cluster; 0x000CD808 is a byte-identical copy. [O]

#### 0x000CD256 `geareng_arbiter_fsm` — logic
Three-state arbiter at +6: idle clears the output short (+8) and flags; state 1 gates on a chain of fnptr
checks plus a channel-indexed table value (PTR_DAT_000cd438+0x24), then computes a setpoint short from
cfg+0x84/+0x86 (type 2 blends a table byte, type 5 uses cfg+0x84 directly) until it passes the threshold
at table+0x20, ending with output 0; state 2 waits for +0xb then returns to idle. Uses gate global
PTR_DAT_000cd450 and active flag PTR_DAT_000cd428 (set when cfg+0x88==6). Called by 0x000B45E0
(seq_cyclic_main). [I] setpoint purpose inferred.

#### 0x000CD41C `timer_u16_tick4` — stub
Ticks the u16 timer at param_1+4 via DAT_000cd458 (= 0x000BADA8 timer_u16_left_tick). Called by
0x000B492C. Single-member cluster. [O]

#### 0x000CD45C `strategy3_select_eval` — logic
Chain-of-responsibility selector: walks up to 3 handler objects from PTR_DAT_000cd738, calling vtable+0x14
to find a ready one (cached in RAM DAT_fff8add8, default sentinel &DAT_fff8adc4), then evaluates
vtable+0x20 with the +0x18 context. Called by 0x000AEA16. Single-member cluster. [O]

#### 0x000CD4D2 `strat3_flag_get` — stub
Returns *DAT_000cd734 != 0 — a flag accessor beside the 3-strategy selector state. Called by 0x000AEA24.
Single-member cluster. [O]

#### 0x000CD4DC `strat3_byte73c_get` — stub
Returns the byte at *DAT_000cd73c. Called by 0x000AEA5C. Single-member cluster. [O]

#### 0x000CD4E4 `strat3_byte740_get` — stub
Returns the byte at *DAT_000cd740. Called by 0x000AEAC0. Single-member cluster. [O]

#### 0x000CD4EC `strat3_none_selected` — stub
Predicate: RAM selector DAT_fff8add8 still equals the default sentinel &DAT_fff8add4, i.e. no handler from
the 3-strategy chain is active. Called by 0x000AEAD4. Single-member cluster. [O]

#### 0x000CD4FC `strat3_call_vt38` — logic
Thunk: calls vtable+0x38 with the +0x30 context on the currently selected strategy object (DAT_fff8add8),
returns the s16 result. Called by 0x000AE9D0. Two-member cluster; 0x000CD564 identical copy. [O]

#### 0x000CD516 `strat3_call_vt50` — logic
Thunk: calls vtable+0x50 with the +0x48 context on the current strategy object. Called by 0x000AE9DE.
Two-member cluster; 0x000CD530 identical copy. [O]

#### 0x000CD54A `strat3_call_vt5c` — logic
Thunk: calls vtable+0x5c with the +0x54 context on the current strategy object, returns u16. Called by
0x000AE9FA. Single-member cluster. [O]

#### 0x000CD586 `ev_report_comp256_f` — logic
Function-pointer-table thunk with no static callers: reports an event (component 0x256, code 0xf, flags
0x4c) via PTR_FUN_000cd748 (= event_report_18b_record family) and returns 0. Five-member cluster
(0x000CD5B6/0x000CD5E6/0x000CD616/0x000CD646 byte-identical copies, one per fp-table slot). [O]

#### 0x000CD676 `geareng_active_tramp` — logic
fp-table trampoline: calls DAT_000cd750 (= 0x000B2754 geareng_state_machine) on PTR_DAT_000cd74c and
returns result != 0 — "engagement FSM active" export. No static callers. Single-member cluster. [O]

#### 0x000CD69E `geareng_subobj_vt5c_tramp` — logic
fp-table trampoline: resolves the +0x84 sub-object via PTR_FUN_000cd760 (= 0x000B2EFC
geareng_get_subobj_84) on PTR_DAT_000cd74c, then reads an s16 through its vtable+0x5c. Single-member
cluster. [O]

#### 0x000CD6C0 `geareng_poscode_tramp` — logic
fp-table trampoline: calls PTR_FUN_000cd768 (= 0x000B268E geareng_pos_code_5) on PTR_DAT_000cd74c and
returns the u16 result. No static callers. Single-member cluster. [O]

#### 0x000CD6FC `geareng_dev_tramp` — logic
fp-table trampoline: calls PTR_FUN_000cd780 on PTR_DAT_000cd77c, resolving to 0x000CD826
(geareng_target_dev_calc); returns its s16. No static callers. Single-member cluster. [O]

#### 0x000CD712 `geareng_poscode6_tramp` — logic
fp-table trampoline: calls PTR_FUN_000cd788 (= 0x000B3BF4 geareng_pos_code_6) on PTR_DAT_000cd76c,
returns u16. No static callers. Single-member cluster. [O]

#### 0x000CD720 `reqsm_run_tramp` — logic
fp-table trampoline: runs PTR_FUN_000cd790 (= 0x000AEF78 request_state_machine) on PTR_DAT_000cd78c,
then returns *PTR_DAT_000cd794 != 0. No static callers. Single-member cluster. [O]

#### 0x000CD798 `reqres_u8_tramp` — logic
fp-table trampoline: calls PTR_FUN_000cd7f0 (= 0x000AF22C request_result_u8_f74) on PTR_DAT_000cd7ec,
returns u8. Three-member cluster (0x000CD7A6/0x000CD7D0 copies). [O]

#### 0x000CD7B4 `reqres_s16_tramp` — logic
fp-table trampoline: calls PTR_FUN_000cd7f8 (= 0x000AF282 request_result_s16_f80) on PTR_DAT_000cd7ec,
returns s16. Two-member cluster (0x000CD7C2 copy). [O]

#### 0x000CD7DE `reqres_u16_tramp` — logic
fp-table trampoline: calls PTR_FUN_000cd804 (= 0x000AF334 request_result_u16_f98) on PTR_DAT_000cd7ec,
returns u16. No static callers. Single-member cluster. [O]

#### 0x000CD826 `geareng_target_dev_calc` — logic
Computes the engagement deviation: target short at cfg+0x92 minus a channel-indexed value (cfg+0x94
passed to PTR_FUN_000cd9ac); when flag obj+6 is set it adds a correction read from a 2-D s16 table
(PTR_FUN_000cd9b8 over PTR_DAT_000cd9b4+0x14, channel-1, *PTR_DAT_000cd9b0). Called by 0x000CD6FC,
0x000CD8A0. Single-member cluster. [O]

#### 0x000CD870 `geareng_pos_offset_calc` — logic
Evaluates a boolean via PTR_FUN_000cd9bc on the config, picks short slot 0xc or 0x10 from the module
table at PTR_PTR_000cd9c0+4, and adds the base short at table+0x14. Called by 0x000B4D0C. Single-member
cluster. [O]

#### 0x000CD8A0 `geareng_dev_plus_extra` — logic
Returns the 0x000CD826 deviation plus, when obj+6 == 4, an extra short selected (via PTR_FUN_000cd9bc)
from table slots 0x18/0x1c of PTR_PTR_000cd9c0+4. Called by 0x000B4F2E. Single-member cluster. [O]

#### 0x000CD8DC `geareng_slot_bitmask_set` — logic
For channel param_2 in 1..5 that passes the predicate DAT_000cd9c4 on the table at PTR_DAT_000cd9b4+0x1c
(index channel-1), stores 1<<channel into obj+0xb — the per-slot engagement bitmask. Called by
0x000CD978. Single-member cluster. [O]

#### 0x000CD914 `geareng_mode_select` — logic
Zeroes obj+6, then loads a u32 mode word from PTR_DAT_000cd9c8[channel*4] into *PTR_PTR_000cd9c0;
values outside 0..4 report a fault (PTR_FUN_000cd9d8) and keep the previous mode, otherwise a second
u32 is fetched from PTR_PTR_000cd9d0 + cfg_byte*4 + mode*8. Called by 0x000B4046
(geareng_setup_gear_request). Single-member cluster. [O]

#### 0x000CD978 `geareng_slot_req_fsm` — dispatch
Jump-table FSM (states 1..5 over +6): gates on type byte PTR_DAT_000cdc50[0x88] and state byte
*PTR_DAT_000cdc54==7, scales a position input (x*byte/0xff), tracks the counter short at +4, issues
per-slot requests by writing values from module table slots 0x20/0x24/0x28/0x2c into cfg+0x44..0x4d and
triggering PTR_FUN_000cdc68, calls 0x000CD8DC for the slot bitmask, and objflag22_clear (0x000CD90E) at
completion; states >= 10 report fault 0x24e/0xf/0xc1. Called by 0x000B45E0 (seq_cyclic_main). [O]

#### 0x000CDBB4 `geareng_setpoint_apply` — logic
Channel-indexed (cfg+0x94) pipeline over PTR_FUN_000cdc78..0x98: reads a base short from module table+8,
combines two fnptr-fetched values, subtracts offsets, blends via PTR_FUN_000cdc8c/0x90/0x94 and applies
the result with PTR_FUN_000cdc98. Called by 0x000CC9EE (geareng_ctrl_step). Single-member cluster. [O]
calls, [I] setpoint role.

#### 0x000CDC4A `timer_u16_arm4` — stub
Arms the u16 deadline at param_1+4 via DAT_000cdc9c (= 0x000BAD8C timer_u16_add_tick). Called by
0x000B492C (seq_reset_timers_subobjs). Single-member cluster. [O]

#### 0x000CDCA0 `obj8_get_or_set` — logic
Returns param_1 or allocates 8 bytes via PTR_FUN_000cdd94; stores param_2 as the first word. Called by
0x000B3A90. Single-member cluster. [O]

#### 0x000CDCBE `geareng_target_dev_m1` — logic
Mode-1 deviation calc: target short at cfg+0x90 minus a channel-indexed fnptr value, plus a 2-D s16
table correction (PTR_DAT_000cdda4+0x10, channel-1, *PTR_DAT_000cdda0). Called by 0x000B4D0C.
Single-member cluster. [O]

#### 0x000CDD08 `geareng_mode_select2` — logic
Second instance of the mode selector: clears byte +4, loads the u32 mode word from
PTR_DAT_000cddac[channel*4] into *PTR_PTR_000cddb0 (0..4 valid, else fault via PTR_FUN_000cddc0 and keep
old), then the follow-up u32 from PTR_PTR_000cddb8 + cfg_byte*4 + mode*8. Called by 0x000B4046
(geareng_setup_gear_request). Single-member cluster. [O]

#### 0x000CDD6C `geareng_slot_req_fsm2` — dispatch
Jump-table FSM (states 1..4 over +4), the second slot-request engine: writes request values derived from
module table slots +8/+0xc/+0x10/+0x14 plus cfg+0x90/10 into cfg+0x44..0x4d, triggers
PTR_FUN_000cdfc8, checks completion flags, and reports fault 0x250/0x7c for states >= 9. Called by
0x000B45E0 (seq_cyclic_main). [O]

#### 0x000CDF42 `geareng_setpoint_apply2` — logic
Mode-2 variant of the setpoint pipeline (0x000CDBB4) using the PTR_FUN_000cdfec..0x000ce008 chain with
char-scaled inputs; combines param_2 + table char*10 and applies per channel. Called by 0x000CC9EE.
Single-member cluster. [O]

#### 0x000CE00C `ctr16_inc_on_state` — logic
When PTR_FUN_000ce268 (state getter) returns 1, increments the u16 at PTR_DAT_000ce26c via
PTR_FUN_000ce270/0x274, saturating at 0xffff. Reached through forwarder 0x000AEE2E. Single-member
cluster. [O]

#### 0x000CE038 `posblend_step` — logic
Steps a small state struct {state,+1 counter,+2 pos short,+3} through a 3x3-byte descriptor table at
PTR_LAB_000ce280 (index param_1[8]*3 + state*9): byte0 selects the pos increment source, byte1 the
counter action, byte2 the next state (3 = finish when counter passes table+0x88). State 4 computes a
float blend ((pos+offset-meas)*a/(a+b)+meas), rounds and clamps between table shorts +0x18/+0x14, then
applies it (PTR_FUN_000ce2a0) and pulses 0x000CE3E6/0x000CE412. Fault component 0x28b codes
0x4e/0x6e/0x94/0xc6; publishes state/counter/pos via PTR_FUN 0x43c/0x444/0x44c. Called by 0x000CE62A.
[O]

#### 0x000CE308 `poswin_check` — logic
Clears the status byte +8; unless param_2==1 (status 2), checks the pos shorts +6/+4 against the window
shorts at PTR_DAT_000ce450 (+100, +0x68, +0x74) and sets status 1 when outside. Called by 0x000CE62A.
Single-member cluster. [O]

#### 0x000CE34A `minmax_reset` — logic
Resets the min/max tracker: min short +4 = 0x7fff, max short +6 = 0x8000, state +9 = 0. Called by
0x000CE48C and 0x000CE35C. Single-member cluster. [O]

#### 0x000CE35C `minmax_track` — logic
Tracks the short at PTR_DAT_000ce454 into min (+4) and max (+6) with a one-shot init state at +9;
unexpected state re-inits via 0x000CE34A. Called by 0x000CE62A. Single-member cluster. [O]

#### 0x000CE3A0 `posblend_state_read` — logic
Reads back the state byte and counter published by 0x000CE038 (PTR_FUN 0x458/0x45c); when state is 0,
seeds the pos short (+2) from table char (+0x40) plus an fnptr value and advances to state 1. Called by
0x000CE48C. Single-member cluster. [O]

#### 0x000CE3E6 `ctr16_inc_on_state2` — logic
Second counter instance: when PTR_FUN_000ce474 state == 1, increments the u16 at PTR_DAT_000ce478
(saturation 0xffff). Called by 0x000CE038. Single-member cluster. [O]

#### 0x000CE412 `zero_on_state` — logic
When PTR_FUN_000ce474 state == 1, writes 0 through PTR_FUN_000ce488. Called by 0x000CE038. Single-member
cluster. [O]

#### 0x000CE48C `posblend_arm_gated` — logic
Heavily gated arm routine: requires mode byte PTR_DAT_000ce6b4[0x21] in 1..8, state == 3, param_3 == 1,
several threshold comparisons (table shorts at PTR_DAT_000ce6c8 +0x34/+0x38/+0x1c/+0x20/+0x24/+0x28) and
clear flags at PTR_DAT_000ce6ec[0xc2/0xc6/0xeb/0xed/0xc7/0xc8]; then runs three fnptr module calls,
seeds the state struct (+0xc=1, pos short +0xe from param_2 + 0x000CEDC6, byte +4, shorts +6/+8/+0x10)
and sets state 4. Reached via forwarder 0x000AEE1C. Single-member cluster. [O] guards, [I] role.

#### 0x000CE62A `posctrl_main_fsm` — logic
792-instruction guarded controller over a ~0x15-byte state struct (states 0..7): each state re-evaluates
a permission chain (PTR_FUN_000ce940..0x974 with table thresholds and +0x2c flags), advances 0 -> 1 ->
2 -> 3, integrates the position window check 0x000CE308, the min/max tracker 0x000CE35C, and the blend
stepper 0x000CE038, publishes the output short at +0x10 from module tables (+0x60), and calls the trend
state machine (0x000C949C/0x000C9504) and seq_permitted (0x000CA148). Called by 0x000CA3A6
(seq_stop_all — likely a stop/abort path entry). Single-member cluster. [O] structure, [I] purpose.

#### 0x000CEDC6 `posblend_scale_calc` — logic
Zeroes the short at +2, and when PTR_FUN_000cee74 state == 1 fills it from PTR_DAT_000cee78 (if
obj+0xc==1) or PTR_FUN_000cee80; then scales it by a fnptr ratio blended through table +0x44 and divides
by 100. Called by 0x000CE48C and forwarder 0x000AEE0E. Single-member cluster. [O]

#### 0x000CEE94 `posblend_abort_adj` — logic
Clears byte +0xc; when param_2 == 0 and the state byte *param_1 is 4 or 5, forces it to 7 (abort).
Reached via forwarder 0x000AEE26. Single-member cluster. [O]

#### 0x000CEEB4 `slot12_fsm` — logic
State machine 0..4 over a ~0x20-byte tracker: maintains a x300-scaled timer (+0x20/+0x12) from
PTR_FUN_000cf070, aborts to state 0 when both gate bytes at PTR_DAT_000cf07c[0x2b/0x2c] are clear;
state 1 initializes 12 sub-slots via PTR_FUN_000cf088 and waits on timer +0x1c vs table+0x4c; state 2
runs the start gates 0x000CF48C, clears via 0x000CF5FA, computes the 2-D threshold index
0x000CF3B4 and loads a u16 from PTR_FUN_000cf090 table into +4; state 3 re-gates with 0x000CF55E and
accumulates curve outputs into +0xc; state 4 delegates to 0x000CF1EC; anything else reports fault
0x29b/0x5c. Called by 0x000CA3A6. Single-member cluster. [O]

#### 0x000CF0A4 `curve4_sum_arbiter` — logic
When the mode char at table+4 is 1: computes the 0x000CF3B4 index term blended via PTR_FUN_000cf1dc and
sums four 3-arg curve lookups (PTR_FUN_000cf1e0 over table slots +0x24/+0x1c/+0x20/+0x28 with the
input short/char) into param_1+2; otherwise leaves +2 = 0x8000. Reached via forwarder 0x000AEDF0.
Single-member cluster. [O]

#### 0x000CF188 `timer_u16_triplet_add` — logic
Adds object time16 (DAT via 0x000BAE5E timer_u16_add_objtime) to the u16 timers at param_1+0x1a,
+0x1c and +0x18. Called by 0x000CA60A. Single-member cluster. [O]

#### 0x000CF1AC `bool_latch_true` — logic
If *param_1 != 0, sets *param_1 = 1 (boolean latch/normalize). Called by 0x000C9FD8
(strat_mode_match). Single-member cluster. [O]

#### 0x000CF1EC `slot12_redistribute` — logic
Slot-stepper for the 12 counters behind PTR_FUN_000cf444/0x450/0x468/0x478: computes the active slot's
value (+6) from the accumulated +0xc divided by the table rate (+0x50) minus a curve value, blends and
clamps via table +0x2c/+0x30/+0x48, accumulates into +4/+8, advances the slot counter (sat 0xff), and
when the 12-slot total drops below the table+0x58 threshold redistributes table0x5c% of the value to
the other slots. Called by 0x000CEEB4. Single-member cluster. [O] math, [I] purpose.

#### 0x000CF3B4 `thresh2d_index4x3` — logic
Builds an index from two threshold walks: stage = first of 3 table+0x44 thresholds exceeded by the u16
at PTR_DAT_000cf47c+0x94, row = first of 4 table+0x40 thresholds exceeded by byte +0x96; index =
(row-1)*3 + stage when both non-zero, then mapped through PTR_FUN_000cf488. Called by 0x000CF0A4,
0x000CF55E, 0x000CEEB4. Single-member cluster. [O]

#### 0x000CF48C `slot12_start_gates` — logic
Start-permission chain: mode char == 1, two fnptr states, mode-byte pair at PTR_DAT_000cf62c[0x21/0x27]
must be (1,1) or (2,2), counter-minus-0x96 below table+0x10, byte ranges around table +8/+0xc/+0x14,
fnptr == 1, u16 below table+0x3c and the +0x18 short above table+0x54. Returns 1 only if all pass.
Called by 0x000CEEB4. Single-member cluster. [O]

#### 0x000CF55E `slot12_pos_gates` — logic
Position gates: recomputes the 0x000CF3B4 index, takes |PTR_DAT_000cf648 - +0x14| and
|PTR_DAT_000cf654 - +0x16| (via abs fnptr 0x650), verifies both below the table+0x38 limit, that the
index equals the tracked +1 (non-zero), and the new index is within the table byte via fnptr 0x65c.
Called by 0x000CEEB4. Single-member cluster. [O]

#### 0x000CF5FA `slot12_state_clear` — logic
Zeroes the ~0x18-byte 12-slot tracker struct (fields +1..+0x16). Called by 0x000CEEB4. Single-member
cluster. [O]

#### 0x000CF660 `gearreq_latch` — logic
Latches a gear request: stores param_3 at +4, and when idle with gear param_2 in 1..5 snapshots the
selector-state bytes PTR_DAT_000cf8b8[0x2b]/[0x21] into +2/+3, clears DAT_fff8ae06, sets state 1,
stores the gear at +1, reads a table byte via PTR_FUN_000cf8c0 (gear-1) into +5 and publishes
DAT_fff8ae00 = gear. Called by 0x000CCC50, 0x000B45E0, 0x000B4D0C. Single-member cluster. [O]

#### 0x000CF6B6 `gearreq_step_publish` — logic
Steps the request validator 0x000CF700, then publishes the gear byte (+5) to DAT_fff8ae02 and a
transformed value of the short PTR_DAT_000cf8c4 (via PTR_FUN_000cf8c8) to DAT_fff8ae04. Called by
0x000B45E0 (seq_cyclic_main). Single-member cluster. [O]

#### 0x000CF6D8 `timer_u8_tick5` — stub
Ticks the byte timer at param_1+5 via DAT_000cf8cc (= 0x000BAD64 timer_u8_left_tick). Reached via
forwarder 0x000AEEA2. Single-member cluster. [O]

#### 0x000CF6DE `obj6_get_or_init` — logic
Returns param_1 or allocates 6 bytes via PTR_FUN_000cf8d0, then sets +2 = 0 and +3 = 6. No static
callers (fp-table constructor). Single-member cluster. [O]

#### 0x000CF700 `gearreq_validate_step` — logic
229-instruction validator for the latched gear request: requires state 1 (else fault 0x291), gear 1..5
with fnptr state 1 or 3, a speed check (fnptr 0x8dc/0x8e0), clear flags at PTR_DAT_000cf8e4
[0xc2/0xc7/0xed/0xeb], additional fnptr gates, and that the snapshotted bytes +2/+3 still match the
live selector bytes cf8b8[0x2c]/[0x27]; when +5 is set it checks the table threshold (selected by +4
from table slots +4/+8, gear-1) against the live short cf8c4 and, below it, pulses DAT_fff8ae06 = 1
and notifies via PTR_FUN 0x904/0x908; any failure clears the latch. Called by 0x000CF6B6. Single-member
cluster. [O]

#### 0x000CF94C `valchg_monitor_step` — logic
Value-change monitor over a ~0x14-byte struct: shifts the input byte from PTR_DAT_000cfabc+0x10 (+2 old,
+1 new), resets the hold/init flags on change, reads the monitored u16 via 0x000CF9DE, tracks the last
value at +10, computes |delta| via PTR_FUN_000cfac0, scales x10 into +0x10 and maintains a x300-scaled
timer at +0x14 via PTR_FUN_000cfac8. Called by 0x000CFA94. Single-member cluster. [O]

#### 0x000CF9DE `mon_val_read_bymode` — logic
Reads the monitored value by mode byte +1: 0 from PTR_DAT_000cfacc, 1 from PTR_DAT_000cfad0 (both
transformed by PTR_FUN_000cfad4 into +4); any other mode reports fault 0x28f/0xf8 and sets the error
flag +0. Called by 0x000CF94C. Single-member cluster. [O]

#### 0x000CFA3A `gearreq2_latch` — logic
Second gear-request latch: when idle with gear param_2 in 1..5, clears DAT_fff8e28c, snapshots the u16
at PTR_DAT_000cfae0 into +2, sets state 1, stores the gear at +1, a table byte via PTR_FUN_000cfae4
(gear-1) at +0x1c, and publishes DAT_fff8e284 = gear. Reached via forwarder 0x000AEDFE. Single-member
cluster. [O]

#### 0x000CFA86 `gearreq2_clear` — logic
Clears the second gear-request struct: +2 = 0, state/gear/+0x1c = 0. Reached via forwarder 0x000AEE06.
Single-member cluster. [O]

#### 0x000CFA94 `gearreq2_step_publish` — logic
Composite step: runs the value-change monitor 0x000CF94C on +4, the validator 0x000CFAEC, then publishes
+2 to DAT_fff8e286, byte +0x1c to DAT_fff8e288 and short +0x16 to DAT_fff8e28a. Reached via forwarder
0x000AEE92. Single-member cluster. [O]

#### 0x000CFAB6 `timer_u8_tick1c` — stub
Ticks the byte timer at param_1+0x1c via DAT_000cfae8 (= 0x000BAD64 timer_u8_left_tick). Reached via
forwarder 0x000AEE9A. Single-member cluster. [O]

#### 0x000CFAEC `gearreq2_validate_step` — logic
Validator for the second gear request: state 1 required (else fault 0x28f/0x6f), gear 1..5 with fnptr
state 2 or 3 plus speed checks, guard flags +4/+10/+0xb, delta vs PTR_DAT_000cfcd0+0x6e under a table
limit, byte/short ranges at the puVar10 tables (+0x96/+0xc4/+100), the selector pair
PTR_DAT_000cfce4[0x2b]==1 and ==[0x2c], and the gear-indexed threshold vs short +0x16; below it pulses
DAT_fff8e28c = 1 and notifies (PTR_FUN 0xce8/0xcf0), otherwise the latch is cleared. Called by
0x000CFA94. Single-member cluster. [O]

#### 0x000CFCFC `obj1_get_or_zero` — logic
Returns param_1 or allocates 1 byte via PTR_FUN_000cfe70 and zeroes it — fp-table constructor. Six-member
cluster: 0x00103DC4/0x00103F38/0x0011837C/0x0011C540/0x001274F4 are byte-identical copies instantiated
elsewhere (cross-region instances of the same module pattern). [O]

#### 0x000CFD1A `gearstate_arbiter` — logic
Gated gear-state arbiter: checks the requested mode at PTR_DAT_000cfe74 (mode 1 requires
cf e78[3] != -1, mode 2 requires cfe7c/cfe80 == -1, others fault 0x90/0x60), aborts when
PTR_DAT_000cfe78[0]==1, requires 0x000CFE62 to see a difference and (on non-idle) a fnptr confirmation;
then copies the new state into *param_1 and *(PTR_DAT_000cfe94)+0x2c, marks the transition-to-0 case at
PTR_DAT_000cfe98[6]/[7], and fires the notification chain PTR_FUN 0xea4/0xeac/0xeb0/0xeb8/0xec0/0xec8/
0xed0/0xed4 with old/new values. Called by 0x000D01C4, 0x000D01E6. Single-member cluster. [O]

#### 0x000CFE56 `gearstate_copy_out` — data-accessor
Copies the mode byte at PTR_DAT_000cfe74 into *param_1 and *PTR_DAT_000cfed8. Called by 0x000CFFDE.
Single-member cluster. [O]

#### 0x000CFE62 `gearstate_diff_chk` — data-accessor
Predicate: *PTR_DAT_000cfedc != *param_1 — detects a gear-state difference. Notably also called from
0x00103B36/0x00103C2A/0x00103CEC/0x00103DE2/0x00104A28 (cross-region users). Single-member cluster. [O]

#### 0x000CFEE0 `obj8_get_or_zero` — logic
Returns param_1 or allocates 8 bytes via PTR_FUN_000d010c and zeroes the first byte — fp-table
constructor. No static callers. Single-member cluster. [O]

#### 0x000CFEFE `pair_fnptr_call` — logic
Invokes PTR_FUN_000d0114 on PTR_DAT_000d0110 and PTR_FUN_000d011c on PTR_DAT_000d0118 — a two-step
dispatch on module objects. Called by 0x000B6BFC. Single-member cluster. [O]

#### 0x000CFF0E `mode_consist_chk_evt` — logic
Runs two fnptr steps and a plausibility check over the mode bytes PTR_DAT_000d0134/0x138/0x144/0x13c/
0x140: patterns like (134 != 138 && 13c != 140), (134==1 && 13c==-1), (134==2 && 13c != -1), or
144/138 disagreement report event 0x7e code 2 and invoke PTR_FUN_000d015c(...,3). Called by 0x000B6BDC.
Single-member cluster. [O]

#### 0x000CFFDE `selpos_eval_step` — logic
Selector-position evaluator: steps 0x000D0262, maps the selector state byte PTR_DAT_000d0170[0xa0]
(< 9) through calibration table PTR_LAB_00063444_1_000d0174 into *param_1, computes the validity flag
+1 from state 8 / byte 0xb6 / PTR_DAT_000d0138, reports event 0x7e code 3 for states >= 9, then runs a
battery of module steps with param_2 selecting between the 0x188/0x18c and 0x198..0x1b4 callback
groups. Called by 0x000B6BE4. Single-member cluster. [O]

#### 0x000D01C4 `objpair_set_step` — logic
Sets a value via PTR_FUN_000d029c on PTR_DAT_000d0298 with param_2, and if PTR_FUN_000d02a4 returns
non-zero, sets that result too; then invokes PTR_FUN_000d02ac on PTR_DAT_000d02a8. Called by 0x000B6BEC.
Single-member cluster. [O]

#### 0x000D01E6 `objpair_toggle_step` — logic
Module step: sets 1 via PTR_FUN_000d02b4 and clears it when PTR_FUN_000d02bc reports non-zero, runs
0x2c4, performs the gated double-set pattern (0x29c/0x2a4) twice, then 0x2cc and publishes 0x2d4(0x2f).
Called by 0x000B6BF4. Single-member cluster. [O]

#### 0x000D0232 `char3_extrema_chk` — library
Pure predicate over three chars: returns 0 if any is -1 or the middle argument is neither a strict
maximum nor a strict minimum; 1 otherwise. Called from 0x00103CEC/0x00104A28 (cross-region). Single-
member cluster. [O]

#### 0x000D0262 `selpos_status_read` — logic
Reads four status bytes from PTR_DAT_000d02d8 via PTR_FUN 0x2dc/0x2e0/0x2e4/0x2e8 into param_1 +2..+5
(+4 stored as boolean == 1) and clears +6. Called by 0x000CFFDE (selpos_eval_step). Single-member
cluster. [O]

#### 0x000D02EC `tbl_u8_get_d0558` — data-accessor
Reads a u8 from the table at PTR_DAT_000d055c+0x38 indexed by *PTR_DAT_000d0558. Called by 0x000B6B28.
Single-member cluster. [O]

#### 0x000D0304 `shiftcand_step` — logic
Shift-candidate stepper: ready check via vtable+0x20 (module at +0x1c), candidate verdict via vtable+0x2c
against the mode byte from 0x000D0422; verdict 1 latches the mode short (0x000D040C) as countdown +0x26
and submits via 0x000D073E (state 2), verdict 2 resubmits (state 1), otherwise falls back to
0x000D05AE modes 3/4 using the mode-branch bytes. Called by 0x00104A28 (cross-region). Single-member
cluster. [O]

#### 0x000D03A6 `shiftcand_complete_chk` — logic
In state 2: when the countdown short +0x26 reaches zero (or the vtable+0x2c verdict says done) and byte
+0x1a equals *PTR_DAT_000d0564, returns to state 1, calls 0x000D073E and the vtable+0x14 method.
Called by 0x000D04FA. Single-member cluster. [O]

#### 0x000D040C `shiftcand_mode_s16_get` — data-accessor
Returns the short at table+0x34 (PTR_DAT_000d055c) when mode byte +8 == 2, else the short at table+0x30.
Called by 0x000D0304. Single-member cluster. [O]

#### 0x000D0422 `shiftcand_mode_u8_get` — data-accessor
Returns the byte behind table+8 (PTR_DAT_000d055c) when mode byte +8 == 2, else behind table+0xc. Called
by 0x000D0304, 0x000D03A6. Single-member cluster. [O]

#### 0x000D0440 `shiftcand_check` — logic
Builds a 24-byte candidate descriptor via PTR_FUN 0x568/0x56c from (param_2,param_3), reads the mode
branch byte (table+8/+0xc), evaluates it with 0x000D06EA and latches +3 = 1 on verdict 1. Called by
0x000D165A (shift_chain_select). Single-member cluster. [O]

#### 0x000D0496 `shiftcand_eval5` — logic
Same candidate pattern as 0x000D0440 but keyed on the byte behind table+0x10; verdict 1 submits
0x000D05AE mode 5 with the captured byte and returns 1, verdict 2 submits mode 5/0. Called by
0x000D1990 (shift_map_select_gear). Single-member cluster. [O]

#### 0x000D04FA `shiftcand_tick` — logic
Ticks the candidate object: decrements the countdown short +0x26, runs the completion check 0x000D03A6,
and when 0x000D06DE reports ready and *PTR_DAT_000d0570 is clear, runs the verdict FSM 0x000D05DC(0).
Called by 0x000CFFDE (selpos_eval_step). Single-member cluster. [O]

#### 0x000D052E `shiftcand_start_eval` — logic
Ready check via vtable+0x20 (module at +0x3c), then the verdict FSMs 0x000D05DC(1) and 0x000D068C.
Called by 0x000D140C (mode_change_apply_notify). Single-member cluster. [O]

#### 0x000D0574 `shiftcand_reset` — logic
Zeroes state byte +0 (and +1 when 0x000D06DE reports not ready), clears +3, and invokes the vtable+0x14
method. Called by 0x00103B36 (cross-region). Single-member cluster. [O]

#### 0x000D05A0 `shiftcand_method14` — logic
Invokes the vtable+0x14 method on the module at +0x3c (Ghidra could not recover the jump table).
Called by 0x000D15A6 (shift_gear_target_calc). Single-member cluster. [O]

#### 0x000D05AE `shiftreq_submit_mode15` — dispatch
Shift-request submitter: when param_4 is set with param_3==1 it latches +4 and a short from
PTR_FUN_000d0778 into +6; then selects object/byte pairs from PTR_PTR_000ba034/38/3c/40/44/4c by mode
1..5 and calls the global request entry PTR_PTR_000ba048 (= 0x000BABF6, resolving into the 0xEEF78-area
arbiter). Called by 0x000D0304/0x000D0496/0x000D05DC/0x000D068C/0x000D073E. Single-member cluster. [O]

#### 0x000D05DC `shiftcand_verdict_fsm` — logic
Verdict FSM over byte +1: compares the candidate (0x000D06EA against the byte behind
PTR_DAT_000d0784), the gear window check 0x000D0704 on +1, and readiness 0x000D06DE; transitions
0..3 with submissions via 0x000D05AE mode 1 (including the special 3->2 case and the value-latched
submit). Called by 0x000D04FA, 0x000D052E. Single-member cluster. [O]

#### 0x000D068C `shiftcand_verdict2` — logic
Second verdict FSM over byte +0 using the byte behind table+4: verdict 1 submits 0x000D05AE mode 2 with
the +0x36 byte (state 3), verdict 2 submits mode 2/0 (state 2). Called by 0x000D052E. Single-member
cluster. [O]

#### 0x000D06DE `shift_ready_flag_get` — data-accessor
Returns the byte behind PTR_DAT_000d0784+0x14 — the shift-module readiness flag. Called by 0x000D04FA,
0x000D05DC, 0x000D0574. Single-member cluster. [O]

#### 0x000D06EA `shiftcand_compare` — logic
Calls the candidate-compare vtable+0x2c on the module pointer found at sub-struct+0x14 with the
sub-struct base (+0x24 context) and the candidate byte. Called by 0x000D0440, 0x000D0496, 0x000D05DC,
0x000D068C. Single-member cluster. [O]

#### 0x000D0704 `gear_window_chk` — logic
Compares the current short (PTR_FUN_000d078c) against the table short (+0x24 of PTR_DAT_000d0784) with
hysteresis from the byte behind +0x2c: 1 within window, 2 when below by more than the hysteresis, and
state param_2==3 forces 1. Called by 0x000D05DC. Single-member cluster. [O]

#### 0x000D073E `shiftreq_submit_34` — logic
Submit helper: param_3==0 cancels via 0x000D05AE(3,0,0); otherwise submits mode 3 (when param_2 set)
or mode 4 with the captured byte. Called by 0x000D0304, 0x000D03A6. Single-member cluster. [O]

#### 0x000D0790 `obj8_get_or_init` — logic
Returns param_1 or allocates 8 bytes via PTR_FUN_000d08d8, zeroing bytes +1 and +7 — fp-table
constructor. No static callers. Single-member cluster. [O]

#### 0x000D07B0 `manualmode_gate_fsm` — logic
Gate flag + FSM: sets *param_1 when PTR_FUN_000d08e8(...) == 0 and the byte pair
*PTR_DAT_000d08ec == 1 / *PTR_DAT_000d08f0 == 2 (else clears); then a 4-state FSM at +1 — state 0
latches +7, two shorts (+2/+4) and byte +6 from PTR_DAT_000d08fc/0x0900 when PTR_FUN_000d08f8 == 1 or
the gate is set, states 1..3 cycle on PTR_FUN_000d08f8, invalid states report fault 0x84/7. Called by
0x000CFFDE (selpos_eval_step). Single-member cluster. [O] structure, [I] manual-mode reading.

## Unit summary

Unit 26 (region `r0c_app`, 0x000C8D2A-0x000D07B0): 220 clusters, 248 functions named, completed across
two agent passes (101 + 119 clusters). Zero `unknown` entries — the region is dominated by three
well-anchored module families plus trampoline/getter glue.

Counts by kind (248 names rows):
- logic: 190
- stub: 26
- data-accessor: 17
- dispatch: 9
- library: 6
- irq: 0, entry: 0, unknown: 0

Unknown entries: none.

Content map:
- 0x000C8D2A-0x000CA9B8: diag read/encode pipeline, trend (LSQ window fit) monitors, strategy objects,
  and the sequencer (~20-state `seq_*` machine) with its permission gates.
- 0x000CA9B8-0x000CC4A2: shift-phase machines (faults 0x25d/0xca/0xee/0x67), iter/quad module groups
  (4 sub-module begin/step/end/readiness chains over PTR_DAT_000cc550..0x55c).
- 0x000CC4DA-0x000CD978 (this pass): quad state predicates/FSM (states 7/10 done, 11 abort), the
  gear-engagement controller (`geareng_*`: ctrl init/step FSM with phase table at PTR_LAB_000ccadc,
  threshold monitors, deviation calcs, slot-request FSMs writing cfg+0x44..0x4d, fault component
  0x24c/0x250), and a 3-strategy chain-of-responsibility selector (RAM DAT_fff8add8).
- 0x000CD914-0x000CF5FA: second geareng instance (mode select 2, deviation/setpoint variants),
  position-blend controller with float math (`posblend_*`, fault component 0x28b), 12-slot
  redistribute tracker (fault 0x29b), gear-request latch/validate pairs (`gearreq*`, faults
  0x291/0x28f).
- 0x000CF94C-0x000D07B0: value-change monitors, gear-state arbiter with notify fan-out, selector
  position evaluator (calibration table at 0x00063444), and the shift-candidate/verdict/submit family
  (`shiftcand_*`, `shiftreq_submit_*` feeding the arbiter behind PTR_PTR_000ba048 / 0xEEF78).

Cross-region leads:
- fp-table thunk farms with no static callers: `ev_report_comp256_f` x5 (0x000CD586 cluster) and the
  `reqres_*`/`geareng_*_tramp` thunks at 0x000CD676-0x000CD7DE — the owning tables are in RAM data;
  units covering the 0xAEEx forwarder block already mirror several of these (0x000AEE0E/26/2E/A2).
- Cluster members living outside this unit's address range: `obj1_get_or_zero` copies at 0x00103DC4,
  0x00103F38, 0x0011837C, 0x0011C540, 0x001274F4, plus `objflag22_clear` copies up to 0x0014DCA0 —
  units owning 0x103xxxx-0x14Dxxx ranges should not re-document these as local patterns.
- Heavy callers into 0x000B3xxx-0x000B8xxx (geareng state machine 0x000B2754, event recorder
  0x000B79E4 with 199 callers, timer libs 0x000BAD64/0x000BADA8/0x000BAD8C) — those units hold the
  shared infrastructure this unit is built on.
- `gearstate_diff_chk` (0x000CFE62) and `shiftcand_*` helpers are called from 0x00103B36-0x00104A28
  and 0x000D140C/0x000D15A6/0x000D165A/0x000D1990 (mode_change_apply_notify / shift gear target /
  chain select / map select) — the shift-scheduling unit should cross-check these verdict paths.
