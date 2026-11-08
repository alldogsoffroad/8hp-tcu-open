# Unit 32 (r0c_app)

Region 0x000F55A8–0x0014B43C (r0c_app slice): monitor/diagnostic framework support library.
Dominant pattern: per-monitor state structs + debounce state machines + diag-trace publishers
(`diag_trace2_*` sinks 0x000D572C/0x000D5A7C/…, class tags 0x54d/0x559/0x561/0x585/0x59f/0x5a5/0x56b)
+ vtable-driven 5-slot channel banks. Cross-refs spec 13 (fault reporting).

#### 0x000F55A8 `mon_lim_min2` — library
Returns min(x, f1(x), f2(min, param_2)) as u16, where f1 = PTR_FUN_000f5610 (0x0011A458) and
f2 = PTR_FUN_000f5614 (0x00118718) — two curve/limit lookups in r11/r12 regions. Called only by
chan12_count_monitor (0x000BC0F4, class 0x5eb), i.e. a lower limit envelope for a monitored
value. Single-member cluster. [I] spec 13 monitor inputs.

#### 0x000F55DC `mon_lim_max2` — library
Mirror of 0x000F55A8: returns max(x, f1(x), f2(max, param_2)) via PTR_FUN_000f5618/0x000f561c
(0x0011A46E, 0x00118748) — upper limit envelope. Also called only by 0x000BC0F4 (class 0x5eb
12-channel monitor). Single-member cluster. [I] spec 13 monitor inputs.

#### 0x000F5620 `mon49_state_ctor` — library
Default-constructs the 18-byte monitor-0x49 state: bytes 0..9 zero, two u16 timers at +0xa/+0xc
set to 0xffff, codes +0xe..+0x11 = 6. Allocates 0x12 bytes via operator new (0x0009AA34) when
passed NULL. Called from thunk 0x0015C49C (r14_app). Single-member cluster. [O]

#### 0x000F5664 `mon49_eval` — logic
Per-step evaluation of monitor 0x49: calls guard 0x4a (lib_evt_trigger_a), resets/reads the two
channel objects (PTR_DAT_000f586c/0x000f5874 vtables), fills state bytes 0..8 from getters,
runs debounce state machine 0x000F58C8 (result -> +8) and debounce tick 0x000F581C, stores the
two new codes at +0x10/+0x11 and old-code byte param_4 at +0xf. Called by mon49_debounce_sm
(0x000BB8F6). Single-member cluster. [O] spec 13.

#### 0x000F56F0 `mon49_gate_active` — logic
Returns 1 unless state+4 == 0 AND (state active byte *param_1 == 0 OR debounce test
0x000F584E fails). One of the four next-state gates of mon49_next_state (0x000BBA28).
Single-member cluster. [O]

#### 0x000F5712 `mon49_gate_flags` — logic
Returns 1 unless (global *PTR_DAT_000f589c == 0 OR state+4 != 0 OR global *PTR_DAT_000f58a0 == 0)
AND (state active byte == 0 OR debounce test 0x000F584E passes). Second gate of
mon49_next_state (0x000BBA28); gates on two global flags. Single-member cluster. [O]

#### 0x000F5744 `mon49_gate_mode_lt3` — logic
Returns 1 unless state+6 == 0 AND (mode byte *PTR_DAT_000f58a4 < 3 OR *PTR_DAT_000f58a0 != 0).
Third gate of mon49_next_state (0x000BBA28), branching on a 3+ value mode global. Single-member
cluster. [O]

#### 0x000F5762 `mon49_gate_mode_ge3` — logic
Returns 1 unless state+7 == 0 AND mode byte *PTR_DAT_000f58a4 > 2 — the complementary gate of
0x000F5744 in mon49_next_state (0x000BBA28). Single-member cluster. [O]

#### 0x000F5778 `mon49_trace_59f` — logic
Publishes nine monitor-0x49 state bytes/words to the diag trace sink as ids 1..9 under class
0x59f (getter calls via PTR_FUN_000f58ac..0x000f58c0). Called by mon5a1_report (0x000BB9D8).
Single-member cluster. [O] spec 13 measurement/report ids.

#### 0x000F581C `mon49_deb_tick` — logic
Debounce tick: clears counter state+9 when either code (+0x10/+0x11) changes; while the monitor
is active (*param_1 != 0) and the two codes differ and counter != -1, increments it. Called from
mon49_eval 0x000F5664. Single-member cluster. [O]

#### 0x000F584E `mon49_deb_active` — logic
Returns state+9 counter < limit byte read through object PTR_DAT_000f58c4 vtable +0x14 —
"debounce still counting" test shared by gates 0x000F56F0/0x000F5712. Single-member cluster. [O]

#### 0x000F58C8 `mon49_deb_sm` — logic
Monitor-0x49 debounce state machine: two u16 timers (+0xa, +0xc) ticked while != 0xffff;
transitions for code pairs 5/6, 6/5, 6/7, 7/6, 5/8, 8/5 (5↔7↔6, 5/7↔8 blocks move +0xe to
+0xf and swap timers); final test returns timer(+0xa) < limit from PTR_DAT_000f5b00 object.
Called by mon49_eval. Single-member cluster. [O]

#### 0x000F598A `monf_state_ctor` — library
Default-constructs the 36-byte float-monitor state: words 0..2 zero, words 3..8 = the DAT pair
000f5b08/000f5b0c. Allocates 0x24 bytes via operator new when passed NULL; callers '-' (used by
float monitor objects). Single-member cluster. [O]

#### 0x000F59D0 `monf_eval` — logic
Float-monitor per-step evaluation: guard calls, runs float debounce SM 0x000F5CF0 into result+1,
a getter into result+0, debounce tick 0x000F5C42, then stores the two float inputs at +6/+7/+8.
Called by monf_sm_debounce (0x000BBB28). Single-member cluster. [O]

#### 0x000F5A1C `monf_slot_test` — logic
Returns 1.0 iff slot getter PTR_FUN_000f5b24 (0x00119238) returns exactly 1.0. 4-member
byte-equivalent cluster: 0x000F5A38 (getter 0x00119256, called by monf_sm_debounce/
monf_debounce_done), 0x000F5A54, 0x000F5A9E — one "slot test passed" evaluator per monitored
channel slot. [O]

#### 0x000F5A70 `monf_slot_test_or` — logic
Returns 1.0 iff slot getter (0x00119282) == 1.0 OR (input float == 1.0 AND debounce test
0x000F5C7A == 1.0). Used by monf_result_calc (0x000BBC78). Single-member cluster. [O]

#### 0x000F5ABA `monf_slot_test_and` — logic
Returns 1.0 iff (input-float at PTR_DAT_000f5b10+0x24 == 1.0 AND slot getter == 0.0 AND global
float PTR_DAT_000f5b38 == 1.0) OR (input float == 1.0 AND 0x000F5C7A == 0.0). Inverted-slot
variant used by monf_result_calc. Single-member cluster. [O]

#### 0x000F5B3C `monf_test_thresh` — logic
Returns 1.0 iff slot getter (0x001192D2 via PTR_FUN_000f5ca8) == 1.0 OR (threshold DAT_000f5cb0
<= global float PTR_DAT_000f5cac AND global PTR_DAT_000f5cb4 == 0.0). Threshold variant for
monf_result_calc. Single-member cluster. [O]

#### 0x000F5B6E `monf_test_below` — logic
Returns 1.0 iff slot getter (0x001192EE) == 1.0 OR global float PTR_DAT_000f5cac < threshold
DAT_000f5cb0 — below-threshold variant for monf_result_calc. Single-member cluster. [O]

#### 0x000F5B96 `monf_trace_59f` — logic
Publishes the float-monitor state as trace ids 0x65..0x6d under class 0x59f (9 values, byte/
word getters via PTR_FUN_000f5cd0..0x000f5cd8) and triggers event 0x4f. Called by
mon5a1_report2 (0x000BBC22). Single-member cluster. [O] spec 13.

#### 0x000F5C42 `monf_deb_tick` — logic
Float debounce tick: clears counter (+2) when either float input changes vs stored +7/+8;
while active (+0 > 0) and inputs differ and counter < DAT_000f5ce4, increments counter.
Called from monf_eval. Single-member cluster. [O]

#### 0x000F5C7A `monf_deb_active` — logic
Returns 1.0 iff input float+8 counter < limit float from object PTR_DAT_000f5ce8 vtable (+0x14)
— "float debounce still counting" test used by 0x000F5A70/0x000F5ABA. Single-member cluster. [O]

#### 0x000F5CF0 `monf_deb_sm` — logic
Float debounce state machine (123 instr), float mirror of 0x000F58C8: timers +0xc/+0x10 ticked
toward DAT_000f5e14; state-code constants DAT_000f5e1c/0x000f5e20/0x000f5e24/0x000f5e28 drive
the same 5/6/7/8 transitions, moving code +0x18 -> +0x14 and swapping timers; result compares
timer +0xc against object limit (PTR_DAT_000f5e18 vtable). Called by monf_eval. [O]

#### 0x000F5E30 `mon5_gate_all5` — logic
Returns 1 only when all five slot getters (mode-2 calls through PTR_DAT_000f5efc..0x000f5f10)
return non-zero — AND gate over the 5-channel slot bank used by monitor driver 0x000F43A8
(the class 0x54d/0x559/0x561/0x585 family). Single-member cluster. [O]

#### 0x000F5E82 `mon54d_trace_id1` — stub
Tail publisher: diag_trace2_u8 (0x000D5A7C) with id 1, class 0x54d, value. 4-member cluster,
one per monitor class: 0x0011C55E `mon559_trace_id1`, 0x001268F0 `mon561_trace_id1`,
0x001275B6 `mon585_trace_id1` (r10_app). [O] spec 13.

#### 0x000F5E92 `mon5_gate_all5f` — logic
Float variant of 0x000F5E30: sets *param_1 = 1.0 only when all five mode-2 float getters
(PTR_FUN_000f5f1c via PTR_DAT_000f5f18..0x000f5f2c) return exactly 1.0, else 0.0. Called by
monitor driver 0x000F43A8. Single-member cluster. [O]

#### 0x000F5EDC `mon54d_trace_idb_evt40` — stub
Tail publisher: diag_trace2_u8 id 0xb class 0x54d plus event trigger 0x40 (lib_evt_trigger_a).
3-member cluster, one per monitor class: 0x0011C67E `mon559_trace_idb_evt48` (evt 0x48),
0x001276D6 `mon585_trace_idb_evt71` (evt 0x71). [O] spec 13.

#### 0x000F5F34 `mon56b_ctor4` — library
Default ctor of the 4-byte class-0x56b monitor state (u16 max, byte count, bool flag),
new(4) fallback via operator new. Callers '-' (constructed from r14_app thunks).
Single-member cluster. [O]

#### 0x000F5F54 `mon_slots_max_u16` — logic
Lazy max across the 6-entry slot bank PTR_DAT_000f61a8 (index clamped 1..5): each slot's vtable
+0x20 word getter (obj+0x18). Result cached in state+0 until non-zero. Used by 0x000F607A,
0x000F62EE and cross-region 0x0012618C (r10_app). Single-member cluster. [O]

#### 0x000F5FA2 `mon56b_eval_u16` — logic
Class-0x56b word-bank evaluation: event guard 0x60, copies byte at obj+0x54 of PTR_DAT_000f61b0
into its **-target, then for slots 1..5 counts vtable+0x2c true flags; increments state+2 per
slot where the +0x14 getter or slot test 0x000F6178 fires; sets state+3 = (4 <= flag count).
Called by 0x0011A494 (r11_app monitor step). Single-member cluster. [O]

#### 0x000F6036 `mon_slot_ctr_word` — data-accessor
Switch on index 1..5 returning RAM counter words DAT_fff8c8ac / fff8c948 / fff8c9e4 /
fff8ca80 / fff8cb1c, 0 otherwise. Reached through iface8ac_idx_counter (0x000BB84C).
Single-member cluster. [O]

#### 0x000F607A `mon56b_trace_and_clear` — logic
Class-0x56b word report: traces state+2 (id 1), max word 0x000F5F54 (id 2), state+3 (id 4),
then invokes vtable+0x38 on ten slot objects (PTR_DAT_000f61c0 +0..+0x28, skipping +0x14) with
RAM pointers fff8c858/8f4/990/a2c/ac8/8b0/94c/9e8/a84/b20 — clearing/resetting the per-slot
counters read by 0x000F6036. Called by 0x0011A494. Single-member cluster. [O]

#### 0x000F6178 `mon_slot_flag_u8` — stub
Index-clamped (1..5, 0→5) tail call of slot vtable +0x14 byte getter on bank
PTR_DAT_000f61c4. Single-member cluster. [O]

#### 0x000F61C8 `mon56b_ctor12` — library
Default ctor of the 12-byte float class-0x56b state ([0]=max float, [4]=count, [8]=flag),
new(0xc) fallback. Single-member cluster. [O]

#### 0x000F61EA `mon_slots_max_f32` — logic
Float mirror of 0x000F5F54: lazy max over float slot bank PTR_DAT_000f6424 (slots 1..N,
N=DAT_000f6420), vtable +0x20 float getter. Used by 0x000F607A/0x000F62EE and cross-region
0x001262C0. Single-member cluster. [O]

#### 0x000F6242 `mon56b_eval_f32` — logic
Float-bank evaluation for class 0x56b: stores getter via PTR_DAT_000f6428+0x54 path into
PTR_DAT_000f6430, then per slot counts vtable+0x2c == 1.0 into float, and adds to state+4 when
the +0x14 getter or slot test 0x000F63F2 is > 0; state+8 = 1.0 iff count > DAT_000f6434.
Called by 0x0011A494. Single-member cluster. [O]

#### 0x000F62EE `mon56b_trace_clear2` — logic
Second class-0x56b report: traces state+2 (id 0xb), max float 0x000F61EA (id 0xc), state+3
(id 0xe), clears the same ten slot counters as 0x000F607A (different pointer set
PTR_DAT_000f6440, RAM fff8c834/8d0/96c/a08/aa4/898/934/9d0/a6c/b08) and triggers event 0x61.
Called by 0x0011A494. Single-member cluster. [O]

#### 0x000F63F2 `mon_slot_flag_f32` — stub
Float-bank slot test: index clamp 1..DAT_000f6420 then tail call of vtable +0x14 on bank
PTR_DAT_000f6448 (Ghidra shows the result call as unemulated indirect jump). Single-member
cluster. [O]

#### 0x000F645E `varsel_flag_calc` — logic
Variant-selected flag: mode byte (**(char**)(*(int*)PTR_DAT_000f6640+0x58)) 0 -> compute via
PTR_FUN_000f6644 on &DAT_fff8e490, 1 -> via PTR_FUN_000f6648 on &DAT_fff8cb40, else error
handler PTR_FUN_000f664c (0x000D5E0C fatal err 0x69). Reached via iface8b4_mode_flag
(0x000BB85C). [I] two hardware/sensor-set variants.

#### 0x000F64A2 `varsel_flag1_zero` — logic
Same variant select (mode 0: DAT_fff8e49d, mode 1: DAT_fff8cb91, else fatal 0x69); returns
flag == 0. Callers 0x00127512/0x00127512-family in r10/r12_app. Single-member cluster. [O]

#### 0x000F64E4 `varsel_byte_get` — data-accessor
Variant-selected byte: mode 0 -> DAT_fff8e49c, mode 1 -> DAT_fff8cb90, else fatal handler.
Callers 0x0012D882/0x00127512/0x001275C6 (r10/r12_app). Single-member cluster. [O]

#### 0x000F6524 `varsel_byte_zero` — logic
Same addresses as 0x000F64E4 but returns byte == 0 as bool. Callers 0x0012D882/0x00127512.
Single-member cluster. [O]

#### 0x000F6578 `varsel_flagf1_zero` — logic
Float variant test: getter (PTR_FUN_000f6654 on object PTR_DAT_000f6650+0x58) == 1.0 ->
DAT_fff8cc10; second getter == 0.0 -> DAT_fff8cc38; mismatch -> fatal handler; returns
value == 0.0. Caller 0x0012762A (r12_app). Single-member cluster. [I]

#### 0x000F65CC `varsel_word_get` — data-accessor
Variant-selected word: getter == 1.0 -> DAT_fff8cc0c, getter == 0.0 -> DAT_fff8cc34, else
fatal handler. Callers 0x001276F0/0x0012DD94/0x0012762A. Single-member cluster. [O]

#### 0x000F6616 `varsel_flagf2_zero` — logic
Second float variant test (object PTR_DAT_000f6688, error handler PTR_FUN_000f6690): returns
(DAT_fff8cc0c / DAT_fff8cc34 selection) == 0.0. Callers 0x0012DD94/0x0012762A. Single-member
cluster. [O]

#### 0x000F6694 `monobj_ctor4` — library
Generic 4-byte monitor-object ctor (2 words zeroed, new(4) fallback). 3-member byte-equal
cluster: 0x0012616C, 0x00127390 (r10_app copies). [O]

#### 0x000F66B4 `mon5a5_delta_u16` — logic
Monitor-0x51 pair calc: event guard 0x52, clamps a getter result against PTR_DAT_000f6818+0x10,
then writes two clamped u16 deltas (PTR_DAT_000f681c/+0x1c minus reference) into state[0..1].
Float twin is 0x000F6766. Called by mon51_driver (0x000BBF5A). Single-member cluster. [O]

#### 0x000F671A `mon5a5_trace_ids12` — logic
Traces the two u16 deltas of 0x000F66B4 as ids 1/2 under class 0x5a5 (diag_trace_u16
0x000D5B32). Called by mon51_driver. Single-member cluster. [O]

#### 0x000F6744 `monobj_ctor8` — library
Generic 8-byte monitor-object ctor (2 words zeroed, new(8) fallback). 3-member cluster:
0x000F6F78, 0x0012629E (r10_app copy). [O]

#### 0x000F6766 `mon5a5_delta_f32` — logic
Float twin of 0x000F66B4: clamps a getter float to [DAT_000f6828, 0], reference PTR_DAT_000f682c
+0x2c clamped to [that, DAT_000f6830], then two deltas from PTR_DAT_000f6834/+0x3c clamped to
[0, DAT_000f6838]. Called by mon51_driver; publishes via 0x000F67DA. Single-member cluster. [O]

#### 0x000F67DA `mon5a5_trace_idbc` — logic
Traces the two float deltas of 0x000F6766 as ids 0xb/0xc under class 0x5a5 and triggers event
0x53. Called by mon51_driver (0x000BBF5A). Single-member cluster. [O]

#### 0x000F684C `mon5ad_ctor_a` — library
C++ ctor of the 0x20-byte class-0x5ad monitor object: zeroes fields, installs vtable
PTR_LAB_000f6ae0 at +0xc, runs two sub-object ctors (PTR_FUN_000f6ae4) at +0x18/+0x1c;
new(0x20) fallback. Called from 0x0015B274 (r14_app construction site). Single-member
cluster. [O]

#### 0x000F6896 `mon5ad_trace_ids1_5` — logic
Reports the 0x20-byte object as trace ids 1..5 under class 0x5ad (byte, two words via
diag sinks 0x000D5C7C/0x000D5D2A, word at +0x10, byte at +0x14). Called by mon51_driver
(0x000BBF5A). Single-member cluster. [O]

#### 0x000F69D2 `mon5ad_ctor_b` — library
Second class-0x5ad object ctor, same 0x20 layout but vtable PTR_LAB_000f6b20; published by
0x000F6A22 (ids 0xb..0xf). new(0x20) fallback; callers '-'. Single-member cluster. [O]

#### 0x000F6A22 `mon5ad_trace_idsb_f` — logic
Float report of the second 0x5ad object: ids 0xb..0xf under class 0x5ad via sinks
0x000D58F8/0x000D572C/0x000D59AC. Called by mon51_driver. Single-member cluster. [O]

#### 0x000F6C14 `mon5ab_ctor_a` — library
Ctor of the 0x14-byte class-0x5ab object: fields zeroed, vtable PTR_LAB_000f6db8 at +0xc,
new(0x14) fallback. Called from 0x0015B274. Single-member cluster. [O]

#### 0x000F6C3E `mon5ab_trace_ids1_4` — logic
Reports the 0x14-byte 0x5ab object as trace ids 1..4 (byte, words at +4/+8, byte at +0x10)
via sinks 0x000D5C7C/0x000D5A7C. Called by mon51_driver. Single-member cluster. [O]

#### 0x000F6CD4 `mon5ab_ctor_b` — library
Second 0x5ab object ctor (vtable PTR_LAB_000f6ddc at +0xc), published by 0x000F6D02.
Single-member cluster. [O]

#### 0x000F6D02 `mon5ab_trace_idsb_e` — logic
Float report of the second 0x5ab object: ids 0xb..0xe class 0x5ab via sinks
0x000D58F8/0x000D572C. Called by mon51_driver. Single-member cluster. [O]

#### 0x000F6D4E `monf_test_noteq` — logic
Vtable method (callers '-'): returns 1.0 iff global float PTR_DAT_000f6de8 != PTR_DAT_000f6dec
OR getter PTR_FUN_000f6df0 (0x0011C838) returns 0.0. Float "test passed" predicate.
Single-member cluster. [O]

#### 0x000F6E04 `monobj_ctor6` — library
Generic 6-byte monitor-object ctor (bytes/words zeroed, new(6) fallback). Called from
0x0015B274. Single-member cluster. [O]

#### 0x000F6E28 `mon_min2_u16` — library
Returns min(getter_a(param_1), getter_b()) as u16 via PTR_FUN_000f7050/0x000f7054
(0x0011C6E8/0x0011C720). Exposed as iface898_dt_a (0x000BB7EA). Single-member cluster. [O]

#### 0x000F6E4C `mon_deltamin2_u16` — logic
For the two channel objects DAT_fff8cc48 and DAT_fff8cc74: word = vtable+0x5c getter minus
offsets DAT_fff8cc40/fff8cc6c, clamped 0..0xffff; returns the smaller delta. Exposed as
iface898_dt_b (0x000BB7F8). Single-member cluster. [O]

#### 0x000F6EBC `mon5b3_eval_u8` — logic
Class-0x5b3 byte eval: event guard 0x58, refresh pair via PTR_FUN_000f705c on &DAT_fff8cc3c /
&DAT_fff8cc68; state[0] = (cc3c || cc68), stores cc40/cc6c words at +2/+4, state[1] =
((cc4d && cc3c) || (cc79 && cc68)). Called by mon51_driver. Single-member cluster. [O]

#### 0x000F6F34 `mon5b3_trace_ids12` — logic
Refresh pair via PTR_FUN_000f7060 (fff8cc3c->fff8cc50, fff8cc68->fff8cc7c), then traces
state[0]/[1] as ids 1/2 class 0x5b3. Called by mon51_driver. Single-member cluster. [O]

#### 0x000F6F9A `mon5b3_eval_f32` — logic
Float 0x5b3 eval: refresh fff8cc50/fff8cc7c via PTR_FUN_000f7068; state[0] = 1.0 unless both
floats 0.0; state[1] = 1.0 unless ((cc64 && cc50) || (cc90 && cc7c)). Called by mon51_driver.
Single-member cluster. [O]

#### 0x000F7002 `mon5b3_trace_idbc` — logic
Float 0x5b3 report: refresh pair via PTR_FUN_000f706c, trace ids 0xb/0xc class 0x5b3, event
0x5d. Called by mon51_driver. Single-member cluster. [O]

#### 0x000F7074 `monobj_ctor2` — library
Generic 2-byte monitor-object ctor (two bytes zeroed, new(2) fallback). 2-member cluster with
0x0011D340 (r10_app copy). [O]

#### 0x000F7094 `monobj_ctor2_ff` — library
2-byte ctor variant initialising both bytes to 0xff (the "no code yet" state of mon49-style
state machines). Callers '-'. Single-member cluster. [O]

#### 0x000F70B4 `mon5e9_trace_edges` — logic
Class-0x5e9 report with edge capture: guards events 0x29/0x2a, reads two booleans through
PTR_PTR_000f71bc/0x000f71cc vtables, keeps previous value (fff8e49e/fff8e49f) and inverted
state (fff8e4a0/fff8e4a1), traces ids 1/0xb and 3/0xd class 0x5e9 twice each (before/after
update) via 0x000D572C/0x000D5A7C. Reached via thunk_f70b4 (0x000BB86A). [O]

#### 0x000F71D0 `lib_sum_bytes_u16` — library
Additive checksum primitive: sums param_3 bytes from param_1 into *param_2. 21 callers, all in
the 0x000Bxxxx/0x000Cxxxx CAN packer region (r08/r0c). [O] spec 08 checksum byte.

#### 0x000F71EC `lib_chksum_replace` — library
Checksum update on byte replacement: for i in 0..n-1: *sum = *sum - old[i] + new[i]. 122
callers in the CAN packer region — the dominant message-checksum maintenance helper.
Single-member cluster. [O] spec 08.

#### 0x000F7214 `lib_chksum_verify` — library
Recomputes the additive sum over a buffer with 0x000F71D0 and compares it with *param_1
(bool). 20 callers in the CAN packer region. Single-member cluster. [O] spec 07/08.

#### 0x000F7234 `vec_get_9b760` — stub
Tail call of (**PTR_PTR_000f7284)() — forwards to lib_get_9b760 (0x0009B5BA). Called by
0x00100F4C. Single-member cluster. [O]

#### 0x000F7242 `vec_get_9b75c` — stub
Tail call forwarding to lib_get_9b75c (0x0009B5B2) via PTR_PTR_000f7288. Called by 0x00100F4C.
Single-member cluster. [O]

#### 0x000F7256 `vec_get_word_a` — stub
Returns (int)**(short **)PTR_PTR_000f728c — indirect word getter behind a double pointer.
Called by 0x000FC4E8. Single-member cluster. [O]

#### 0x000F725E `vec_get_word_b` — stub
Returns (int)**(short **)PTR_PTR_000f7290 — second indirect word getter behind a double
pointer. Called by 0x000FC4E8. Single-member cluster. [O]

#### 0x000F7266 `vec_get_u16_idx` — stub
Forwards (int)param_2 to (**PTR_PTR_000f7294)() and returns the u16 result. Called by
0x000FC06E (gear area). Single-member cluster. [O]

#### 0x000F7298 `seq_snapshot_gather` — logic
432-instr gather: fills a ~0x8a-byte snapshot struct from ~106 indirect getter calls
(PTR_PTR_000f750c..0x000f75xx), mixing byte and u16 fetches at scattered offsets ([0x89],
[0x66..0x69], ring at +6..+0x3a). Called by seq_reset_snapshot_cfg (0x000C21E4) and
seq_poll5_update_obj (0x000C2192). Single-member cluster. [O]

#### 0x000F785C `fwd_get_flag_f7908` — stub
Returns *PTR_DAT_000f7908 (byte flag behind a pointer). Called by 0x000F7864 only.
Single-member cluster. [O]

#### 0x000F7864 `seq_status_rec_build` — logic
Builds the 15-byte status record (bytes 0..0xe) from ~14 getters (PTR_DAT/PTR_FUN f7910-f794c);
variant path when *PTR_DAT_000f790c == 2 zeroes the record except [0]/[10] = PTR_DAT_000f7910
[0x3f] clamped to 8 when negative. Called by seq_poll5_update_obj (0x000C2192) and
0x000C22DA. Single-member cluster. [O]

#### 0x000F7950 `seq_mode_dispatch` — dispatch
Mode-change dispatcher (362 instr): on change of mode byte *PTR_DAT_000f7ad0 vs stored
*PTR_DAT_000f7adc — invalid (>1) traced as (0x5f,0) and forced to 0; mode 0 invokes ~72 reset
functions (PTR_FUN_000f7ae0..0x000f7c20), mode 1 invokes ~110 others (PTR_FUN_000f7d80..
0x000f7ec0, arg from *PTR_DAT_000f7adc); stores the new mode. Reached via vec_dispatch_thunk
0x000C218C/0x000C21DE. Single-member cluster. [O]

#### 0x000F7EC8 `lib_event_hist_push` — library
Pushes into a 10-deep byte history ([0..9] shifted, new code at [0]) and a 10-deep u16 history
(+0xa..+0x1c shifted, new word at +0xa) unless (code == 0xff && old == -1). 77 callers across
the whole app — the shared event-history recorder. Single-member cluster. [O]

#### 0x000F7F2C `rec_flag_at4` — stub
Returns *(char *)(*param_1 + 4) != 0 — flag accessor on a record pointed to by the first
argument. Caller 0x00129B70 (r12_app). Single-member cluster. [O]

#### 0x000F7F36 `rec_byte_at2` — stub
Returns *(undefined1 *)(*param_1 + 2). Caller 0x001230B0 (r12_app). Single-member cluster. [O]

#### 0x000F7F3E `rec_flags45_or` — stub
Returns 1 if byte at param+4 or param+5 is set. Callers 0x0011E56C/0x00123B62.
Single-member cluster. [O]

#### 0x000F7F50 `seq_engage_sm` — logic
379-instr two-channel state machine: computes active flags (param+2 from state==1, param+9)
with hold latches (PTR_DAT_000f81a4[0x5b]/[0x59] and code-1->2 rewrite into f81a0/f81a8),
speed-window tests via curve lookups PTR_FUN_000f81b0 (window uVar10 < ref uVar5 < uStack_28),
invalid codes traced as event 0x68, states 0/1/2 at +0xb with latching into +6/+7 and a final
u16-threshold arbitration from PTR_DAT_000f82bc. Called by seq_refresh_2mods (0x000C2254).
[I] engagement/ratio-style gate pair.

#### 0x000F8340 `seq_sub_init229` — logic
Guard call via PTR_FUN_000f8538 then initializer PTR_FUN_000f853c on param+0x229 (sub-record
of the seq context). Callers '-'. Single-member cluster. [O]

#### 0x000F8356 `seq_step_entry` — logic
Entry step: zeroes *PTR_DAT_000f8540; if *PTR_DAT_000f8544 == 2 runs the alternate path
FUN_000f8420, else two setter calls on PTR_DAT_000f854c (with *PTR_DAT_000f8554 short) and two
calls with param+0x220, returning a getter result. Called by seq_call_c2398 (0x000C22C2).
Single-member cluster. [O]

#### 0x000F83A0 `seq_retry_loop` — logic
Skip when mode 2; if PTR_DAT_000f8568[0x1a] == 0 single FUN_000f846E call, else loop calling
PTR_FUN_000f856c and FUN_000f846E until [0x1b] <= [0x18] or [0x1b] == 8. Called by
seq_call_c2398__2 (0x000C22CA). Single-member cluster. [O]

#### 0x000F83E4 `seq_step_exit` — logic
No-op when mode 2, otherwise tail-calls FUN_000f8490. Called by seq_call_c2398__3
(0x000C22D2). Single-member cluster. [O]

#### 0x000F83F2 `seq_step_refresh` — logic
When mode != 2: FUN_000f84ee(param), refresh via PTR_FUN_000f8570 on PTR_DAT_000f854c, then
PTR_FUN_000f8578/0x000f857c pair on PTR_DAT_000f8574 with param+0x220, then FUN_000f864e.
Called by 0x000C22DA. Single-member cluster. [O]

#### 0x000F8420 `seq_step2_entry` — logic
Mode-2 entry: pulls four bytes through PTR_FUN_000f8580 into PTR_DAT_000f8568 [0x1b]/[0x18]/
[0x17] and PTR_DAT_000f8574[1]; clears [4], three zero-setters on PTR_DAT_000f854c and a
sub-init via PTR_FUN_000f8fa0 on +0x128. Called by 0x000F8356. Single-member cluster. [O]

#### 0x000F846E `seq_eval_218` — logic
Zero-initialises an 8-byte local and calls PTR_FUN_000f8598 (0x00123990) on param+0x218 with
*PTR_DAT_000f8594. Called by 0x000F83A0. Single-member cluster. [O]

#### 0x000F8490 `seq_next_code_apply` — logic
Fetches next code via PTR_FUN_000f859c on param+0x218 (default *PTR_DAT_000f8594), copies
[0x1b]->[0x1c], applies it via PTR_FUN_000f85a0 into *PTR_DAT_000f85a4, then two follow-up
calls and a final call with param+0x220. Called by 0x000F83E4. Single-member cluster. [O]

#### 0x000F84EE `seq_push_228` — logic
Gets a value via PTR_FUN_000f85b4 and feeds it to PTR_FUN_000f85b8 on param+0x228 — a
one-value history push. Called by 0x000F83F2. Single-member cluster. [O]

#### 0x000F8506 `fwd_call3` — stub
8-instr forward of (p1,p2,p3) through PTR_FUN_000f85bc. 2-member cluster with 0x000F85C0
(same pattern, own pointer); caller 0x000FB25E. [O]

#### 0x000F85DC `fwd_get_u8_a` — stub
Tail call of (*DAT_000f8670)() returning a byte (target 0x00120814). Called by
seq_status_rec_build 0x000F7864. Single-member cluster. [O]

#### 0x000F860C `fwd_get_u8_b` — stub
Tail call of (*DAT_000f8680)() (target 0x001208B2). Caller 0x000FC358 (gear_calc_step_limit).
Single-member cluster. [O]

#### 0x000F8630 `fwd_get_u8_c` — stub
Tail call of (*DAT_000f868c)() (target 0x00121370). Caller 0x000FA9DC (in this unit).
Single-member cluster. [O]

#### 0x000F8642 `fwd_get_s8_d` — stub
Tail call of (*DAT_000f8690)() sign-extended to int (target 0x00121382). Called by
0x000F7864. Single-member cluster. [O]

#### 0x000F869C `seq_edges_and_ringavg` — logic
431-instr context update: for several boolean pairs builds 2-bit edge codes (prev/cur -> 0..3)
at +0x66/+0x77/+0x7b; code at +0x6e maps -1->-2 with override 0xff; computes a scaled value
via PTR_FUN_000f8900 (x*100/0xd1e), pushes it into a u16 ring buffer indexed by +0x14 with
depth from PTR_DAT_000f88fc (invalid depths traced as event 0x4c, clamped 1..10), then ring
average -> +0x71..+0x74; mirrors fields from *(param+0x60) and maintains a second ring at
+0x20 indexed by +0x48 with depth from byte [1] (limit 0x15). Called by seq_reset_snapshot_cfg
(0x000C21E4). Single-member cluster. [I]

#### 0x000F8ABE `cfg_opt_get_u8` — data-accessor
Config-gated optional field get: if enable flag **(obj PTR_DAT_000f8d48+0x1c) is set, writes
ctx+0x65 to *param_2 and returns 1, else *param_2 = 0. 3-member cluster (0x000F8C5A,
0x000F8C76 — same pattern on other ctx offsets/flags). Callers in r12_app (0x0012B2F8). [O]

#### 0x000F8ADA `cfg_opt_get_u8_d` — data-accessor
Returns ctx+0x124 when enable flag (PTR_DAT_000f8d48+0x28) is set, else 0. 4-member cluster:
0x000F8C0E, 0x000F8C92, 0x000F8D12 (same gated-byte pattern, other fields). Caller 0x0012AA94.
[O]

#### 0x000F8AEE `cfg_opt_get_u8_b` — data-accessor
Gated by PTR_DAT_000f8d48+0x20: returns (ok, ctx+0xe). 4-member cluster: 0x000F8B04,
0x000F8BF6, 0x000F8C42. Caller 0x00129DFA. [O]

#### 0x000F8B20 `cfg_opt_get_u8_c` — data-accessor
Gated by PTR_DAT_000f8d48+0x0: returns (ok, ctx+0x7d). Callers 0x0012B6E8/0x0012B90C.
Single-member cluster. [O]

#### 0x000F8B38 `cfg_opt_flag_7f` — data-accessor
Gated by PTR_DAT_000f8d48+0x0: returns ctx+0x7f == 1. 2-member cluster with 0x000F8B50
(ctx+0x80 == 1). Caller 0x0012B90C. [O]

#### 0x000F8B74 `ctx_c8_call6` — stub
Forwards six args to PTR_FUN_000f8d4c (0x00121ED0) on the sub-object at ctx+200 (0xc8).
13 callers in r12_app. Single-member cluster. [O]

#### 0x000F8B9A `ctx_c8_err_or_cb` — logic
If the short at ctx+0xc8 is < -99, calls the 6-arg helper with code 0xfe (error report) and
returns its first local byte; otherwise returns ctx+0xcb. Callers 0x00131660/0x0013068C/
0x001309C2/0x00129852. Single-member cluster. [O]

#### 0x000F8BE4 `ctx_get_s16_c8` — data-accessor
Two-instruction getter: returns *(short *)(ctx + 0xc8). Caller 0x000FE8B0. Single-member
cluster. [O]

#### 0x000F8BEA `cfg_flag8_get` — data-accessor
Returns **(undefined1 **)(*(int *)PTR_DAT_000f8d48 + 8) — config enable flag byte. Callers
0x000F9570/0x000F9E26 (this unit). Single-member cluster. [O]

#### 0x000F8C22 `cfg_opt_get_u8_ec` — data-accessor
Gated by PTR_DAT_000f8d48+0x10: *param_2 = ctx+0xec; returns 1 only when flag and field set.
Caller 0x0012D54C. Single-member cluster. [O]

#### 0x000F8CA6 `cfg_opt_get_s16_a8` — data-accessor
Gated by PTR_DAT_000f8d48+0xc: returns short ctx+0xa8, else 0. Caller 0x00123410.
Single-member cluster. [O]

#### 0x000F8CBA `cfg_opt_get_s16_a6` — data-accessor
Gated by PTR_DAT_000f8d48+0xc: returns short ctx+0xa6, else 0xffff. Caller 0x00123410.
Single-member cluster. [O]

#### 0x000F8CD0 `cfg_opt_get_u8_e` — data-accessor
Gated by PTR_DAT_000f8d48+4: returns byte ctx+0x31 (default 0). 3-member cluster: 0x000F8CE4
(+0x33), 0x000F8CF8 (+0x34). Caller 0x0012AB78. [O]

#### 0x000F8D26 `ctx_128_call5` — stub
Forwards five args to PTR_FUN_000f8d50 (0x00101FA0) on sub-object ctx+0x128. Caller
0x00123990. Single-member cluster. [O]

#### 0x000F8D54 `ctx_128_call1` — stub
Single-arg tail call of PTR_FUN_000f8f9c (0x0010203A) on ctx+0x128. 9-member cluster:
0x000F8DCC/0x000F8DE2/0x000F8DEC/0x000F9124/0x000F912E/0x000F9CB6/0x000F9CC0/0x0012EC2E —
repeated one-liners around the ctx+0x128 sub-object. [O]

#### 0x000F8D8C `ctx_sel_s16_142_156` — data-accessor
Selector: returns short ctx+0x142 when param_2 == 0, else ctx+0x156. 3-member cluster with
0x00128BB0/0x00128BC2 (r10_app copies). Caller 0x00128076. [O]

#### 0x000F8D9E `cfg_opt_get_u8_108` — data-accessor
Second config object family (PTR_DAT_000f8fa4): gated by flag +0x24, returns (ok, ctx+0x108).
Callers 0x0012C634/0x00131968. Single-member cluster. [O]

#### 0x000F8DB6 `ctx_16c_set_u16` — stub
Forwards (ctx+0x16c, u16) to PTR_FUN_000f8fa8 (0x00124138). Called by seq_step_entry
0x000F8356. Single-member cluster. [O]

#### 0x000F8DC2 `ctx_16f_latch` — data-accessor
Copies ctx+0x16f into ctx+0x170 (previous-value latch). 2-member cluster with 0x001438A2
(r14_app copy). Caller 0x00123990. [O]

#### 0x000F8DD6 `ctx_16c_set_u8` — stub
Forwards (ctx+0x16c, byte) to PTR_FUN_000f8fb0 (0x0012441E). 3-member cluster with
0x0012E756/0x0012E8EE. Caller 0x00127D3E. [O]

#### 0x000F8DF6 `cfg_opt_get_u8_185` — data-accessor
Gated by PTR_DAT_000f8fa4+0x2c: returns ctx+0x185, else 0. 2-member cluster with 0x000F8E98
(another field). Caller 0x0012BCE0. [O]

#### 0x000F8E0A `cfg_flag30_get` — data-accessor
Returns **(undefined1 **)(*(int *)PTR_DAT_000f8fa4 + 0x30) — enable flag of the second config
object. Caller 0x0012C410. Single-member cluster. [O]

#### 0x000F8E22 `cfg_opt_get_val_18c` — data-accessor
Gated by PTR_DAT_000f8fa4+0x38 == 1: *param_2 = u16 ctx+0x18c, returns ctx+0x18a when the
value < 10000; otherwise clamps out to 10000 and returns 0. Caller 0x00131ECC. Single-member
cluster. [O]

#### 0x000F8E4E `cfg_opt_get_pair` — data-accessor
Gated by PTR_DAT_000f8fa4+0x38 == 1: returns (short ctx+0x18e, flag ctx+0x18b) when the short
is non-zero, else (0,0). 3-member cluster: 0x000F8ECE (pair +0x192/+0x194), 0x000F8F30 (pair
+0x196/+0x198). Caller 0x00131ECC. [O]

#### 0x000F8E76 `cfg_opt_get_u16_46` — data-accessor
Config-gated (PTR_DAT_000f8fa4+0x3c): *param_2 = u16 ctx+0x46, returns ctx+0x48 == 1; (0,false)
when the feature is off. 2-member cluster with 0x000F8EAC (flag +0x10, field +0xe8/+0xea).
Callers 0x000F916E/0x00123410. [O]

#### 0x000F8EF6 `cfg_opt_get_u8_188` — data-accessor
Gated by PTR_DAT_000f8fa4+0x34 == 1: returns ctx+0x188, else 0. Caller 0x0012BD74.
Single-member cluster. [O]

#### 0x000F8F0A `ctx_19e_call1` — stub
One-arg forward through PTR_FUN_000f8fbc (0x001224D0) on sub-object ctx+0x19e. 15-member
byte-equal cluster — the most duplicated one-liner in this unit: 0x000F9442/0x000F9454/
0x00120802/0x00120874/0x00120888/0x00120896/0x001208A4/0x0012133A/0x00121390/0x001213A2/
0x0012CC92/0x0012E8D4/0x0012ECD0/0x0012ECDE (each with its own pointer table entry).
[O]

#### 0x000F8F1C `ctx_19e_call2` — stub
Two-arg forward through PTR_FUN_000f8fc0 (0x00122506) on ctx+0x19e. Caller 0x0012ECEC.
Single-member cluster. [O]

#### 0x000F8F58 `cfg_opt_get_u16_1a2` — data-accessor
Gated by PTR_DAT_000f8fa4+0x40: *param_2 = u16 ctx+0x1a2, returns byte ctx+0x1a0 (0 when
disabled). Caller 0x000F916E/0x00123410. Single-member cluster. [O]

#### 0x000F8F78 `cfg_opt_get_u16_1a6` — data-accessor
Gated by PTR_DAT_000f8fa4+0x40: *param_2 = u16 ctx+0x1a6 (default 10000), returns byte
ctx+0x1a4. Caller 0x000F916E/0x00123410. Single-member cluster. [O]

#### 0x000F8FC4 `ctx_reset_all` — logic
Whole-context reset (vtable method, callers '-'): ctx+0xc8 = 0xff9c (-100), then reset calls
on eleven sub-objects at +0x54/+0x6c/+0xfc/+0x128/+0x178/+0x8c/+0x1ac/+0x38/+0x1c/+0xd0 plus
a global one. Single-member cluster. [O]

#### 0x000F9038 `ctx_refresh_f4` — logic
Refresh step: sub-call on +0xf4, gated get 0x000F8BF6, sub-call on +0xa4. Called by
0x000C2272. Single-member cluster. [O]

#### 0x000F9062 `ctx_reset_d0_6c` — logic
Reset step: sub-calls on +0xd0 and +0x6c, ctx+0xc8 = -100, then whole-object call via
PTR_FUN_000f9248. Called by 0x000C2284. Single-member cluster. [O]

#### 0x000F9088 `ctx_reset_54_fc` — logic
Reset/refresh of five sub-objects: +0x54, +0xfc, +0x110, (+0x38 with byte from +0x124), and
+0x1ac. Called by 0x000C2284. Single-member cluster. [O]

#### 0x000F90C4 `ctx_set_178_1c` — logic
Pushes the config byte from PTR_DAT_000f9260+0x2c into sub-object +0x178 and refreshes +0x1c.
Called by 0x000C2284. Single-member cluster. [O]

#### 0x000F90E8 `ctx_refresh_tail` — logic
Tail refresh: sub-calls on +0x188, +0x1a0 (twice), +0x1a8, +0x18a, then the status-bit
builder 0x000F916E. Called by 0x000C22A6. Single-member cluster. [O]

#### 0x000F9138 `cfg_opt_get_u16_1aa` — data-accessor
Gated by PTR_DAT_000f9260+0x44: *param_2 = u16 ctx+0x1aa, returns byte ctx+0x1a8. Callers
0x000F916E/0x00123410. Single-member cluster. [O]

#### 0x000F9158 `cfg_opt_get_u8_1c0` — data-accessor
Gated by PTR_DAT_000f9260+0x48 == 1: returns ctx+0x1c0, else 0. Caller 0x0012BEF4.
Single-member cluster. [O]

#### 0x000F916E `ctx_status_bits_build` — logic
Aggregates six config-gated tests (0x000F8F58, 0x000F9138, 0x000F8E76, 0x000F8C92, 0x000F8EAC,
0x000F8F78 + global *PTR_DAT_000f9288) into two status bytes: ctx+0x19c = bits 2|4|8|0x10|0x40,
ctx+0x19d = bit2|bit8. Called by ctx_refresh_tail. Single-member cluster. [O]

#### 0x000F9290 `ctx_set68_mirror` — data-accessor
Stores param_2 at ctx+0x68 and mirrors it into the global *PTR_DAT_000f930c. Called by
0x000F92A0. Single-member cluster. [O]

#### 0x000F929A `thunk_1240c0` — stub
3-instr indirect tail call through PTR_thunk_FUN_00129a52_000f9310 to 0x001240C0; callers '-'
(reached via pointer table). Single-member cluster. [O]

#### 0x000F92A0 `ctx_copy_clamp_select` — logic
Copies ctx+0x58 into +0x68 (via 0x000F9290), a u16 getter result into +0x6a, three bytes
+0x5f/+0x5e/+0x5d into +0x6c/+0x6d/+0x6f with both clamp<=4, and selects +0x6e = +0x6d when
+0x6f < 0x33 else +0x6c. Returns the unclamped +0x6d. Called by 0x000C22B8. [I]

#### 0x000F931C `ctx_sel_s16_39c_3b8` — data-accessor
Returns short ctx+0x3b8 when param_2 == 1 else ctx+0x39c. 17 callers across r0c/r12_app
(including 0x000F99EE in this unit). Single-member cluster. [O]

#### 0x000F9350 `ctx_180_step_calc` — logic
Drives the sub-object at ctx+0x180: helper call (PTR_FUN_000f952c) with two mode bytes ->
bool into *param_7; selector getter (PTR_FUN_000f9530) feeds 0x000F931C to pick a limit short
(+0x39c/+0x3b8); final call (PTR_FUN_000f9534) returns the computed short. Caller 0x00128C04.
Single-member cluster. [O]

#### 0x000F93BE `obj90_call1_u16` — stub
Forwards (obj PTR_DAT_000f9538+0x90, byte) to PTR_FUN_000f953c (0x00121482), returning u16.
Caller 0x001281A6. Single-member cluster. [O]

#### 0x000F93D4 `obj94_get_s16` — data-accessor
Returns **(short **)(*(int *)PTR_DAT_000f9538 + 0x94). 2-member cluster with 0x000F93E0
(object +0x98 slot). Callers 0x001281A6. [O]

#### 0x000F93EC `ctx_180_call_a` — stub
Forwards (ctx+0x180, short ctx+0x3ac) to PTR_FUN_000f9540 (0x000FA36E). 2-member cluster
with 0x000F9402 (twin call to 0x000FA38A). Callers 0x00128E94/0x0012F67A/0x0012FEB8. [O]

#### 0x000F9418 `ctx_180_call3` — stub
Three-arg forward through DAT_000f9548 (0x000FA3A6) on ctx+0x180. Callers
0x00128E94/0x0012F67A/0x0012FEB8. Single-member cluster. [O]

#### 0x000F9426 `rec_flag_at_d` — stub
Returns *(char *)(*param_1 + 0xd) != 0 — flag accessor used by seven r12_app callers.
Single-member cluster. [O]

#### 0x000F9466 `obj8_get_s16` — data-accessor
Returns **(short **)(*(int *)PTR_DAT_000f9554 + 8). Callers 0x0011F410/0x0011F3E6.
Single-member cluster. [O]

#### 0x000F9470 `obj1c_call2` — stub
Calls PTR_FUN_000f955c (0x00121432) with (obj PTR_DAT_000f9554+0x1c value, *PTR_DAT_000f9558),
returns byte. Callers 0x0011E56C/0x0011F6BC. Single-member cluster. [O]

#### 0x000F9488 `seq_speed_sel3` — logic
Computes min(g2, max(g1, ctx+0x17c)) from two indirect u16 getters (PTR_DAT_000f9554 slots
+0x20/+0x24) and the ctx word. Feeds 0x000F9570 (+0x3b6). Single-member cluster. [O]

#### 0x000F94AE `seq_delta_3b6` — data-accessor
Returns short ctx+0x3b6 minus the indirect getter at PTR_DAT_000f9554+8; stored to +0x3b4 by
0x000F9570. Single-member cluster. [O]

#### 0x000F94BE `rec_byte_at0` — stub
Returns *(undefined1 *)*param_1 — first byte of a record. Caller 0x00120378. Single-member
cluster. [O]

#### 0x000F94C6 `rec_byte_at1` — stub
Returns *(undefined1 *)(*param_1 + 1). Caller 0x00120378. Single-member cluster. [O]

#### 0x000F94CE `obj_init_all_p2` — logic
Bulk initializer (callers '-'): stores param_2 at [0]/[0x7c]/[0x20], sets flags +8/+0xf9/
+0xfa/+0x122, resets sub-objects at +0x18 and +0x4c, +0x22e = 0xffff, and pushes param_2 into
sub-objects +0xd2 and +0x60. Members of its 4-member cluster (0x0011E238, 0x001207DC,
0x000FA728) apply the same pattern. [O]

#### 0x000F9570 `seq_main_step` — logic
Main per-step engine of the seq/gear context (264 instr): reads mode via PTR_DAT_000f9800
(codes 0..4), conditions inputs (0x000F9848), speed select 0x000F9488 -> +0x3b6, delta
0x000F94AE -> +0x3b4, drives the +0x1f0/+0x180/+0x218 sub-engines with a 12-arg call, then
mode-dependent window handlers (+0x238 for modes 1-4, +0x80 for modes 0/1/3/4) each latching
result words +0x3a6/+0x3aa/+0x3ac/+0x3ae behind a gate (PTR_FUN_000f982c); mode 3 runs
0x000F9E26; finishes with +0x380/+0x350 pushes, 0x000FA004 -> +0x3bc, 0x000FA064, 0x000FA12C.
Called by seq_reset_snapshot_cfg (0x000C21E4). [I] spec 11 shift execution.

#### 0x000F9848 `seq_inputs_prep` — logic
Input conditioning (181 instr): mirrors source-record fields into ctx (+0x382, +0xe2, +0xe5,
+0x391, +0x38a, +0xe8 with code==2 special-case -> +0x39e = 0), maps the u16 at +0x92 through
a curve object (PTR_FUN_000f9af8, retry over 3 rows, x*10 scaling) into +0xe6 and a class byte
+0x3b3 (clamped to 1..3, event 0x5f, index -> global *PTR_DAT_000f9b0c), clamps +0x39a to
1000, +0xec = (+0x8a)/10 clamped 0xcb2, copies +0xe4/+0x396/+0x392/+0xea/+0x3a9/+0x3b2 and
calls 0x000F9F12. Called by 0x000F9570. [I]

#### 0x000F99EE `seq_push_180_calc` — logic
Pushes a calculation request into the +0x180 sub-object: limit from 0x000F931C(flag 0),
arguments *PTR_DAT_000f9b10, ctx+900, ctx+0x3ac. Called by seq_refresh_2mods (0x000C2254).
Single-member cluster. [O]

#### 0x000F9A1C `seq_curve_4c_lookup` — logic
Clamps a global short (PTR_DAT_000f9af4+0x18) to at least table byte PTR_DAT_000f9b18[0x84],
feeds both into the +0x4c sub-object (PTR_FUN_000f9b1c), then a vtable+0x14 curve lookup with
ctx+0x39e -> +0x3a2. Called by 0x000F9570. Single-member cluster. [O]

#### 0x000F9A5E `seq_ring_rate_avg` — logic
203-instr ring/average engine: shifts the 5-word ring +0xa..+0x12 and the 5-dword ring
+0x14..+0x24 (new value from PTR_DAT_000f9b20), delta -> +4 (event 0x5e when zero), rate
= (word[+0x12]-word[+0xa])*1000/(delta*6) -> +0x3a4 with a divider counter at PTR_DAT_000f9dcc
(16-word ring +0x28..+0x44 advanced on modulo), then two window averages over depths from
obj+0x30/+0x34 (clamped 0xf, event 0x53) -> +0x39c and +0x3b8 — the two limits selected by
0x000F931C. Called by 0x000F9570. [I] spec 11.

#### 0x000F9C4C `seq_rings_reset_cond` — logic
On change of global *PTR_DAT_000f9dd0 vs ctx+0x46, if the +0x80 sub-object test
(PTR_FUN_000f9dd4) fails: zeroes the 5-word ring +0xa..+0x12 and +0x39c/+0x3a4/+0x3b8, resets
+0x80 (PTR_FUN_000f9dd8) and latches +0x46. Called by 0x000F9570. Single-member cluster. [O]

#### 0x000F9CCA `ctx_180_get_s16` — stub
Returns short from PTR_FUN_000f9de4 (0x000FA368) on ctx+0x180. Caller 0x001249DA.
Single-member cluster. [O]

#### 0x000F9CDC `ctx_130_call2` — stub
Two-u16-arg call through PTR_FUN_000f9de8 (0x0011F410) on ctx+0x130, returns u16. Caller
0x0011E56C. Single-member cluster. [O]

#### 0x000F9D1C `rec_byte_get` — data-accessor
Returns *(undefined1 *)(*param_1 + 0x94) — 9-member byte-equal cluster of per-object byte
accessors at varying offsets: 0x000F9D4E, 0x000F9D56, 0x000F9D5E, 0x000F9E16, 0x000F9E1E,
0x000FB20A, 0x000FB218, 0x000FC0FA. [O]

#### 0x000F9D24 `fwd_obj_call1_a` — stub
Forwards *param_1 to PTR_FUN_000f9dec (0x000F7276), returns byte. 2-member cluster with
0x000F9D32 (0x000F727C). Callers 0x000FE658/0x00129E4C. [O]

#### 0x000F9D40 `rec_u16_1a_div10` — data-accessor
Returns *(ushort *)(*param_1 + 0x1a) / 10. Caller 0x0012A4B8. Single-member cluster. [O]

#### 0x000F9D66 `ctx_gear_byte_sel` — data-accessor
Eight-way switch on param_2 (1..8) returning ctx bytes +0x3ce..+0x3d5 (one per gear), any
other value traces event 0x68 and returns 0. Caller 0x0012A772. Single-member cluster. [I]
spec 11 per-gear values.

#### 0x000F9E26 `seq_mode3_dir_calc` — logic
Mode-3 path of 0x000F9570: gated by PTR_FUN_000fa048 (clears +0x3a6/+0x3aa/+0x3ac/+0x3ae when
off); classifies direction byte +8 into 1 (both >=0), 2 (both <1) or 3 (mixed, thresholds from
PTR_DAT_000fa04c+0x28/+0x2c) from shorts +0x320/+0x324 vs +0x106/+0x108/+0x10a/+0x10e, then
per-direction max/min selections into the +0x3a6/+0x3aa/+0x3ac result words. [I] spec 11.

#### 0x000F9F12 `seq_type_lookup_20` — logic
Searches a 20-row table (via curve getters on PTR_DAT_000fa04c+0x40) for the short at
(*p)+0x7c; loads two class bytes into +0x3ba/+0x3bb, each clamped 1..3. Called from
seq_inputs_prep. Single-member cluster. [I]

#### 0x000F9FBC `seq_typea_global_set` — data-accessor
Publishes (ctx+0x3ba - 1), clamped <= 2 with event 0x5f on violation, into global
*PTR_DAT_000fa058. Called from 0x000F9848. Single-member cluster. [O]

#### 0x000F9FE0 `seq_typeb_global_set` — data-accessor
Twin of 0x000F9FBC for ctx+0x3bb into *PTR_DAT_000fa05c. Called from 0x000F9848.
Single-member cluster. [O]

#### 0x000FA004 `seq_centered_byte_calc` — logic
Selects short +0x3ae or +0x3ac by config flag (PTR_DAT_000fa04c+0x3c), scales it by config
byte (+0x38)/10 through curve PTR_FUN_000fa060 and adds 0x80 — an offset-binary centered
output byte, stored to +0x3bc by 0x000F9570. Single-member cluster. [I]

#### 0x000FA064 `seq_per_gear_calc` — logic
Fills the eight per-gear u16 fields +0x3be/+0x3c0/+0x3c2/+0x3c4/+0x3c6/+0x3c8/+0x3ca/+0x3cc
by calling FUN_000fa0ba(ctx, gear 1..8) eight times. Called by 0x000F9570. Single-member
cluster. [I] spec 11 per-gear parameters.

#### 0x000FA0BA `gear_param_calc` — logic
Per-gear parameter: v1 = gear-indexed lookup (PTR_DAT_000fa2d8), v2 = second lookup
(fa2e0), row from obj+0x48 by gear-1 scaled /100 and /1000 through curve f2f0, finally
* ctx+0x398 / max(1, ctx+0x39a). Called by seq_per_gear_calc 0x000FA064. Single-member
cluster. [I] spec 11 per-gear pressure/torque parameter.

#### 0x000FA12C `gear_flags_calc8` — logic
Selects inputs by flag at PTR_DAT_000fa2e8+0x44 (pair +0x38e/+0x3ae/+0x54 vs +0x17c/+0x33a/
+0x58), then calls FUN_000fa264 for gears 1..8 storing bytes into +0x3ce..+0x3d5 (the
per-gear selectors read back by 0x000F9D66). Called by seq_main_step. Single-member cluster.
[I] spec 11.

#### 0x000FA264 `gear_flag_one` — logic
Single per-gear flag: two curve lookups (obj+0x50 by ctx+0x394; obj+0x4c by gear-1), then
((param6 - v2) - (param5*param4*param3)/100000)*10 / max(1, param3+v1) through curve
PTR_FUN_000fa2fc. Single-member cluster. [O]

#### 0x000FA300 `gear_cond_dispatch` — dispatch
Switch on code 4/5/6/7/8/9/10 dispatching to predicates 0x000FA498/0x000FA632/0x000FA686/
0x000FA5A4/0x000FA3F0/0x000FA3E4/0x000FA41C; anything else returns 0. Callers '-' (vtable
method). Single-member cluster. [I] per-condition enablement gates.

#### 0x000FA36E `ctx_68_calc_a` — logic
Calls helper 0x00121432 with (obj fa474+4, ctx+0x68) and stores the byte to ctx+0x6a.
Called by ctx_180_call_a 0x000F93EC. Single-member cluster. [O]

#### 0x000FA38A `ctx_68_calc_b` — logic
Twin of 0x000FA36E using obj slot +8, storing to ctx+0x69. Called by 0x000F9402.
Single-member cluster. [O]

#### 0x000FA3B2 `obj_c4_flag_get` — data-accessor
Returns **(undefined1 **)(*(int *)PTR_DAT_000fa474 + 0xc4). Called by ctx_180_step_calc
0x000F9350. Single-member cluster. [O]

#### 0x000FA3E4 `cond_zero_fa47c` — data-accessor
Predicate: (short)*PTR_DAT_000fa47c == 0. Reached via 0x000FA300 (code 9). Single-member
cluster. [O]

#### 0x000FA3F0 `cond_not_low` — data-accessor
Predicate: 0 iff (byte fa480 < **obj+0xb8) or (u16 fa484 <= **obj+0x68). Reached via
0x000FA300 (code 8). Single-member cluster. [O]

#### 0x000FA41C `cond_below_b8` — data-accessor
Predicate: (byte)*PTR_DAT_000fa480 < **(byte **)(obj fa474 + 0xb8). Reached via 0x000FA300
(code 10). Single-member cluster. [O]

#### 0x000FA468 `fwd_reset_call` — stub
Calls PTR_FUN_000fa494 (0x001236E4) with (PTR_PTR_000fa490, param_1, 0). Called by
shift_request_set 0x000FA780. Single-member cluster. [O]

#### 0x000FA498 `gear_cond_scan4` — logic
107-instr predicate (code 4): requires ctx codes +0x3d == 6 and +0x32 == 4; scans up to nine
rows of table obj+0x9c for the short at PTR_DAT_000fa6dc+0x28; mode at obj+0xb0 vs global
*PTR_DAT_000fa6e8; final window (f6f0 constants vs obj+0xa8/+0xc0/+0xa0/+0xa4/+0x70 and
param+0x3a) AND param+0x6d == 0. Returns 0 only when all pass. [O]

#### 0x000FA5A4 `gear_cond_7` — logic
Code-7 predicate: byte override from *(param+0x34)+0x95 when obj+0xc8 == 1; window over
obj+0x64/+0xb4/+0xbc/+0x50/+0x5c against constants f6f0/f6f4/f6f8/f6fc. Single-member
cluster. [O]

#### 0x000FA632 `gear_cond_5` — logic
Code-5 predicate: (obj+0x64 <= f704) OR (divider curve on byte diff obj+0x50-obj+0x54 >=
*PTR_DAT_000fa700) OR (short f6f8 < **obj+0x5c). Single-member cluster. [O]

#### 0x000FA686 `gear_cond_6` — data-accessor
Code-6 predicate: u16 *PTR_DAT_000fa70c <= **(ushort **)(obj fa6e0 + 0xb4). Single-member
cluster. [O]

#### 0x000FA710 `init_once_fa920` — logic
One-shot init: when *DAT_000fa91c == 0, copies *PTR_DAT_000fa920 into ctx+0xe and +0x3a and
sets the flag. Called by 0x000F9CC0. Single-member cluster. [O]

#### 0x000FA728 `obj_init_vt` — logic
Initializer (member of 0x000F94CE's cluster via byte-pattern grouping is separate here):
stores param_2 at +0x34, vtable PTR_PTR_000fa924 at +4, flag 1 at +8. Called by 0x000F94CE.
Single-member cluster. [O]

#### 0x000FA734 `gear_calc_apply` — logic
Wraps the big calculator 0x000FA9DC: result stored at +0x60 when param_2 == 0, +0x62 when 1,
else 0/zero both. Called by ctx_180_step_calc 0x000F9350. Single-member cluster. [O]

#### 0x000FA780 `shift_request_set` — logic
88-instr shift-request entry: clamps target (param_2) and source (param_2-1) into 1..7 stored
at +0x20/+0x22; optional reset via 0x000FA468; when ctx+0xc == 1 fetches two table words
(PTR_FUN_000fa934 on obj+0x60) into +0x10/+0x12; runs the chain 0x000FAE88, 0x000FAEBE,
0x000FAEF6, 0x000FAF40(p4), 0x000FAF7A(p5), 0x000FAFB0 and optional snapshot 0x000FAE1C;
publishes +0x3a via PTR_FUN_000fa93c. Called by 0x000F99EE. [I] spec 11 shift request.

#### 0x000FA84A `tbl_and_lookup` — logic
AND-composes up to four table lookups on obj+0x74 (params; '!' entries skipped) into a u16
mask. Called by 0x000FA9DC and ctx_180_step_calc. Single-member cluster. [O]

#### 0x000FA8B4 `ctx_flag_6d_calc` — logic
Predicate into ctx+0x6d: 1 only when obj+0xcc != 0 AND f944+0x68 > **obj+0xd0 AND
f944[0x6e] >= **obj+0xd4 AND **obj+0xd8 > param_2 AND **obj+0x64 < f948. Called by
seq_main_step. Single-member cluster. [O]

#### 0x000FA94C `elem_overlap_collect` — logic
Scans nine rows of table obj+0x78 (codes 1..9): for each row whose code is 1..9 and whose
slot in the param_2 array is non-zero, accumulates and clears the entry; records the row byte
in +0x6c and slot number in +0x6b; converts the sum through curve 0x001214B4. Called by
0x000FA9DC. [I] spec 11 shift-element overlap.

#### 0x000FA9DC `shift_path_calc` — logic
442-instr main calculator (called via 0x000FA734): seeds from obj+0xc4, builds a local
working set, mixes table lookups (0x000FA84A, 0x000F8630, 0x000F931C), the overlap collector
0x000FA94C and curve calls (0x00121D10/0x00121CD6/0x001218FA/0x0012145A) into the resulting
short. Single-member cluster. [I] spec 11.

#### 0x000FAE1C `shift_state_snapshot` — logic
Copies the working fields +0xc..+0x2e (byte + 16 words) into the shadow block +0x38..+0x5e —
previous-shift snapshot. Called by shift_request_set. Single-member cluster. [O]

#### 0x000FAE88 `shift_curve_lookup_a` — logic
Two curve lookups (PTR_FUN_000fb078 on **PTR_DAT_000fb05c) by target (+0x20) and source
(+0x22) gear into +0x14/+0x16. First of the parameter-fetch chain of shift_request_set.
Single-member cluster. [O]

#### 0x000FAEBE `shift_curve_lookup_b` — logic
Two curve lookups (PTR_FUN_000fb078 on obj fb05c+0x44) by target/source gear (+0x20/+0x22)
into +0x18/+0x1a. Second of the parameter-fetch chain of shift_request_set. Single-member
cluster. [O]

#### 0x000FAEF6 `shift_curve_lookup_c` — logic
Gated pair fetch: when PTR_DAT_000fb080[0x3f] == 0, two curve lookups (obj fb05c+0x48) by
target/source into +0x1c/+0x1e (else both zero). Third of the chain. Single-member cluster.
[O]

#### 0x000FAF40 `shift_curve_lookup_d` — logic
Two curve lookups (obj fb05c+0x4c) by target (+0x20) and source (+0x22, arg param_2) into
+0x24/+0x26. Fourth of the chain. Single-member cluster. [O]

#### 0x000FAF7A `shift_curve_lookup_e` — logic
Two lookups via FUN_000fafe4 with gear-1 for target/source into +0x28/+0x2a. Fifth of the
chain. Single-member cluster. [O]

#### 0x000FAFB0 `shift_curve_lookup_f` — logic
Two lookups via FUN_000fb0ae with gear-1 and the global short *PTR_DAT_000fb084 into
+0x2c/+0x2e. Last of the chain. Single-member cluster. [O]

#### 0x000FAFE4 `gear_table_lookup_7a` — dispatch
Gear 0..6 switch selecting table pointer (ctx+0xc/+0x10/+0x14/+0x18/+0x1c/+0x20/+0x24) and
flag byte (PTR_DAT_000fb088, fb148 for gear 6), then the lookup helper (PTR_FUN_000fb078 or
fb14c) with (table, param_3, flag); other gears trace event 0x6a and return 0. Single-member
cluster. [O]

#### 0x000FB0AE `gear_table_lookup_7b` — dispatch
Twin of 0x000FAFE4 over tables ctx+0x28..+0x40 (flag PTR_DAT_000fb148, helper fb14c, event
0x6a on invalid). Single-member cluster. [O]

#### 0x000FB158 `ctx_change_det_32_33` — logic
Stores +0x34 = (+0x32 != +0x33) and returns it — pair-change detector used by 0x000FB25E and
five r12_app callers. Single-member cluster. [O]

#### 0x000FB16A `ctx_changed_or_3d` — logic
Returns 1 unless (+0x3d == +0x3e AND +0x1e == +0x1f) — the complementary change detector of
0x000FB158. Callers include 0x000F7F50 and 0x000FB25E. Single-member cluster. [O]

#### 0x000FB18E `obj64_flag_get` — data-accessor
Returns **(undefined1 **)(*(int *)PTR_DAT_000fb338 + 100). Caller 0x0012FB02. Single-member
cluster. [O]

#### 0x000FB19C `ctx_mode12_update` — logic
Mode byte machine at ctx+0x12 (0/1/2): mode 0 requires the fb33c test then checks obj+0x68
and *PTR_DAT_000fb344 != 8 -> 1 else 2; mode 1 reads fb348[0x11] (8 -> 2, else 0/return by
[0x1c]<=[0x1b]); other values trace event 0x68. Called by 0x000FB4B0. Single-member cluster.
[O]

#### 0x000FB212 `rec_byte_at4` — data-accessor
Two-instruction accessor *(p+4). 5-member byte-equal cluster: 0x0012B4E8, 0x001369BA,
0x0013C164, 0x00141FE8 (r10/r13/r14_app copies at +4). [O]

#### 0x000FB220 `rec_byte_at5` — data-accessor
Two-instruction accessor *(p+5). 4-member cluster: 0x00140A02, 0x00141FEE, 0x0014B43C.
Callers 0x0012F80C/0x0012F91C. [O]

#### 0x000FB226 `pair_state_reset` — logic
Resets the pair-change state: +0x32/+0x33 = 0 (mirrored to *DAT_000fb354), +0x1e/+0x1f =
FUN_000fbeda(), +0x3d/+0x3e = (*p)+0x3b + 1. Called by seq_poll5_update_obj (0x000C2192).
Single-member cluster. [O]

#### 0x000FB25A `lib_store32` — library
Two-instruction *param_1 = param_2. 4-member cluster: 0x00100F48, 0x0011E238, 0x001207DC
(copies in r10/r11_app). Callers '-'. [O]

#### 0x000FB25E `gear_change_detect` — logic
222-instr change/event engine: samples new pair codes via table lookups (PTR_FUN_000fb49c/
fb4a0/fb4ac with old/new +0x3d/+0x3e and +0x32/+0x33), combines with the detectors
0x000FB158/0x000FB16A, publishes the change code (1->2 rewrite) to *PTR_DAT_000fb4a4 and
PTR_DAT_000fb4a8[0x16]/[0x17], latches +0x42. Called by seq_refresh_2mods (0x000C2254). [I]
spec 11/09 gear change events.

#### 0x000FB4B0 `gear_pair_engine` — logic
501-instr gear-pair selection engine (the biggest function in this unit): seeds from ctx
(+0x36 -> +0xa/+0x2 word), runs the sub-engines FUN_000fbb50, 0x000FB19C, 0x000fbf4c and a
large gate tree over table predicates (fb6f0 slots 0x14/0x18/0x24/0x3c/0x40/0x44/0x5c/0x60),
then dispatches into the FBAxx/FBCxx/FBDxx/FBFxx helper family to advance the +0xd state
machine. Called by 0x000FB25E. [I] spec 11.

#### 0x000FB9B4 `ctx_cond_code_f_or_30` — logic
Returns 1 only when ctx+0xd != 0 and the code lookup via DAT_000fbc40 (on PTR_DAT_000fbc3c)
returns 0x0f or 0x30. Called by seq_status_rec_build 0x000F7864. Single-member cluster. [O]

#### 0x000FB9E0 `elem_mask_pack` — logic
Converts a 16-bit element mask into a packed word: for each set bit (0..15) fetches the code
(obj fbc44+4 by bit index) and pushes it through PTR_FUN_000fbc4c into a u16 builder;
returns the composed short. Called by gear_pair_engine. [I] spec 11 element matrix.

## Unit summary

Region r0c_app slice 0x000F55A8-0x0014B43C: 220 clusters documented, all functions named.
This slice is three stacked layers of one subsystem:

1. **Monitor/diag framework (0x000F55A8-0x000F7094)** — per-monitor state ctors, debounce
   state machines (codes 5/6/7/8, int and float twins), config-gated test predicates,
   diag-trace publishers by class (0x54d/0x559/0x561/0x585/0x59f/0x5a5/0x5ab/0x5ad/0x5b3/
   0x5e9/0x5eb), event triggers (0x29-0x71), 6-slot vtable channel banks with counter
   clear/reset (RAM words fff8c834-cb20), and variant-selected flag accessors
   (fff8cc0c-cc90 pairs). Cross-ref spec 13; callers in r08_app (mon49/mon5a1/mon51 drivers,
   chan12_count_monitor) are named by the r08 unit.
2. **CAN checksum helpers (0x000F71D0-0x000F7214)** — additive byte-sum checksum
   (compute/update/verify) used by ~160 callers in the 0x000Bxxxx/0x000Cxxxx packer regions.
   Cross-ref spec 08 checksum byte; spec 07/08 CRC-8 is elsewhere.
3. **Shift/gear sequence engine (0x000F725E-0x000FB9E0)** — the seq_* context object
   (config-gated optional fields, ring buffers, mode dispatch, status bits), the seq_main_step
   engine (modes 0-4, window averages +0x39c/+0x3b8, rate +0x3a4), per-gear parameter/flag
   calculation (gears 1-8, fields +0x3be-+0x3cc/+0x3ce-+0x3d5), the gear-condition dispatch
   (codes 4-10), shift_request_set with its curve-lookup chain and table lookups (gears 0-6),
   gear_change_detect, the 501-instr gear_pair_engine, and elem_mask_pack. Cross-ref spec 11
   (shift execution, element matrix, per-gear parameters) and spec 09.

Kind counts (by names-CSV rows): logic 116, data-accessor 79, stub 69, library 29,
dispatch 4, irq 0, entry 0, unknown 0.
Unknown entries: none — every cluster fit an observable pattern; the least-certain names
(marked [I] in entries) are the seq_/gear_/shift_ semantics in 0x000F7F50-0x000FB9E0.

Cross-region leads:
- Curve/limit helpers 0x0011A458/0x0011A46E (r11_app) and 0x00118718/0x00118748 (r12_app)
  feed mon_lim_min2/max2 — those units should cross-check against class-0x5eb monitor use.
- The ~106 PTR_PTR getter objects in seq_snapshot_gather 0x000F7298 and the FBAxx/FBCxx/
  FBD56/FBFxx helper family called by gear_pair_engine 0x000FB4B0 live in the r10/r12/r13
  slices (0x0011Dxxx-0x0014Bxxx) — units covering those addresses should link back here.
- r08_app monitor drivers 0x000BB8F6-0x000BC0F4 call this slice's mon49/monf/mon54d family;
  naming there already matches (mon49_*, mon5a1_*, mon51_driver).
- gear_set_current 0x000FC044 / gear_calc_step_limit 0x000FC358 (r0c_app, other unit) are
  the gear source for this engine — cross-check +0x20/+0x22 gear pairs against them.
