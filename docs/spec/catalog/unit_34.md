# Unit 34 (r10_app)

Region r10_app: 220 clusters, 222 functions (0x001000D6-0x00106EBC). Cluster-order entries below.
#### 0x001000D6 `condmon_init_cbs_1` — stub
Registers handler-table pointer PTR_PTR_001001b8 (count 1) at sub-object +4/+8; called directly by manager constructor 0x000F8FC4 (r0c_app), which builds the ~0x1b0-byte monitor object from sub-objects at +0x1c..+0x1ac. Cluster of 1. [O]
#### 0x001000E0 `condmon_s1_set_state` — logic
State-request handler for monitor sub-object 1 (this ptr param_1, request 0-5): 0-3 latch state into +0xd; 4 clears +0xd and copies a configured flag (**(char**)(cfg 0x0010019c +0x38), remapping 1->2) to global PTR_DAT_0010018c; 5 sets +0xd=1, does a fn-ptr table eval (cfg+8, table 0x001001a0) and stores 0xff + status at PTR_DAT_00100194[0x14]/[0x73]. Reached via the cb table registered by 0x001000D6. Cluster of 1. [I]
#### 0x001001BC `condmon_s2_set_state` — logic
Sibling state handler for sub-object 2 (cfg ptrs 0x00100314-0x0010032c): request 5 runs a fn-ptr map eval when buffers differ and a byte flag is 0; requests 0-4/0x10 pass through; anything else is reported via error hook (*DAT_0010032c)(...,0x68). Cluster of 1. [I]
#### 0x00100232 `condmon_s2_in_window_check` — logic
Returns 1 when map-derived value (FUN_001002c2 gear-group map + FUN_001002ac lane lookup, u16-saturated via 0x00121CFA) stays below threshold *(u16*)(PTR_DAT_00100330+0x394) and mode byte at +0x391 != 2. Gear byte PTR_DAT_00100338 maps 1-4->0, 5-8->1-4, else 3 (gear group index). Cluster of 1. [I]
#### 0x001002AC `condmon_tbl_lookup_u8u8` — library
Thin wrapper: calls fn-ptr 0x00121508 (u8 x u8 word-table evaluator with bounds error hook 0x4d) on table at param+0x24, lane param_2, row PTR_DAT_00100340; returns s16 sign-extended. Only caller 0x00100232. Cluster of 1. [O]
#### 0x001002C2 `condmon_sel_map_by_gear` — logic
Selects a map pointer from sub-object slots +0x28/+0x2c/+0x30/+0x34 based on mode byte PTR_DAT_00100338, evaluates it via fn-ptr 0x001218FA (3-arg variant for mode 6) and saturates the s16 result to u16 via 0x00121CFA. Cluster of 1. [I]
#### 0x0010040E `condmon_s3_build_status` — logic
Builds a 16-bit condition-status word into param+0x16 from ~10 comparisons: indirect flag checks (bits 0x1/0x2), cfg flag (0x4), mode!=6 (0x8), rpm<=threshold at obj+8 (0x10), byte at obj+0xc (0x20), byte at obj+0x14 (0x40), speed<value at obj+0x18 (0x80), plus two global flags (0x100/0x200). Callers=- (fn-ptr). Callees 0x000F82E0/0x000FC0FA/0x00121432 are flag getters. Cluster of 1. [I]
#### 0x00100492 `condmon_s4_build_status` — logic
Same status-word construction (bits 2..0x200 into param_1+5+0x16) but with inputs carried in registers (unaff_r8-r13, param_2..param_10) — a register-interface twin of 0x0010040E reached via a dispatch table edges.csv cannot see. Cluster of 1. [I]
#### 0x001004EA `condmon_init_cbs_2` — stub
Registers handler table PTR_PTR_001005a4 (count 1) at sub-object +8/+0xc; second of four cb-registration stubs called by ctor 0x000F8FC4. Cluster of 1. [O]
#### 0x001004F4 `condmon_s2_cyclic` — logic
Cyclic step for sub-object 2 (caller 0x000F9088): runs the shared hierarchical-state-machine helpers (PTR_FUN_001005ac = 0x001236E4 step, PTR_FUN_001005b0 = 0x00123916 active-state query, cfg PTR_PTR_001005a8) and latches the returned byte into obj+0x10. Cluster of 1. [I]
#### 0x00100674 `condmon_init_cbs_3` — stub
Registers handler table PTR_PTR_00100908 (count 1) at +4/+8; third cb-registration stub from ctor 0x000F8FC4. Cluster of 1. [O]
#### 0x0010067E `condmon_s3_cyclic` — logic
Cyclic step for sub-object 3 (caller 0x000F9088): reads gear-indexed selector entry (*PTR_DAT_00100910)[*PTR_DAT_0010090c]*4, reads a speed via fn-ptr and compares against *(u16*)(obj+0x30) storing the bool at +0x10; below-threshold promotes mode 1->2, other modes write global PTR_DAT_0010091c; then HSM step/query latches a byte into +0xd. Cluster of 1. [I]
#### 0x00100A76 `condmon_test_event_bit` — data-accessor
Returns bit (param_2 & 0x1f; param_2!=0) of the u16 word at *(int*)PTR_DAT_00100a90 + 0x54 — event/request bitmask query. Callers=- (fn-ptr). Cluster of 1. [I]
#### 0x00100AB4 `condmon_calc_target_value` — logic
Mode-based target computation into param+0x18 with validity +0x1a: mode 0 reads a value via fn-ptr and accepts it when nonzero and above *(u16*)(cfg+0x14); mode 1 with +0x14==1 interpolates via fn-ptr map (cfg+0xc, x = +0x10); other modes zero both and raise error 0x68. Called by sub-object cyclic 0x00100C42. Cluster of 1. [I]
#### 0x00100B32 `condmon_init_cbs_4` — stub
Registers handler table PTR_PTR_00100d20 (count 1) at +4/+8; fourth cb-registration stub from ctor 0x000F8FC4. Cluster of 1. [O]
#### 0x00100C58 `condmon_window_state_machine` — logic
4-state window/hysteresis machine on param+0x1b (0 idle, 1 active, 2 reset, 3 latched): thresholds bVar13/14/15 come from a 4-byte-entry table lane 0/1/2 via fn-ptr 0x00100d38, compared against speed byte PTR_DAT_00100d2c; target window sVar11..sVar12 (+-hysteresis sVar4 at obj+0x34) from map 0x00100d44; entering the band arms state 1 and latches a map value into +0x1e; leaving the band or value<+0x10 aborts to 2; state 3 re-arms when outside window. Illegal state -> error 0x68. Cluster of 1. [I]
#### 0x00100EA8 `condmon_accumulator_update` — logic
When global PTR_DAT_00100f30[0x52]==0: accumulates (u16 at +0x3c << 16) / (3600000 / tick-ms byte PTR_DAT_00100f34) into u32 +0xc (saturating 0xFFFFFFFF), then curve lookup (0x00121CAA) on acc>>13 writes +0x16 and +0x10 — a time-integral of a rate feeding a threshold curve. Part of sub-object cyclic 0x00100C42. Cluster of 1. [I]
#### 0x00100EF0 `condmon_s5_cyclic` — logic
HSM step (0x001236E4) + active-state query (0x00123916) on cfg PTR_PTR_00100f3c, latching the result byte into param+0x14. Called by 0x00100C42. Cluster of 1. [O]
#### 0x00100F4C `condmon_edge_detect` — logic
Reads two bit-inputs via fn-ptrs (PTR_FUN_00100f9c/0x00100fa0) into obj+5/+8, returns 1 when mode byte (*(char**)(cfg 0x00100fa4))==1 and either input shows a 0->1 edge against the stored copies at *param+0x39/0x3a. Called by r0c_app cyclic 0x000C20BA. Cluster of 1. [I]
#### 0x00100FA8 `condflag_tick_dispatcher` — logic
Periodic driver (caller 0x000C21E4) of the decaying flag-array object: updates the adaptive schedule (0x00100FF4), decays fast flags every tick (0x00101060), slow flags every 3rd tick (0x001011EC), slowest every 0x21 ticks (0x00101496, then resets divider), and increments tick counter +0xbb. Cluster of 1. [O]
#### 0x00100FF4 `condflag_period_update` — logic
Adaptive task-period logic: when mode (*(char**)PTR_DAT_001011e8)!=1 uses fixed 0x1e; otherwise period = clamp(round-to-10((timer-delta & 0xff)+4), 10, 100) from *obj+0x80, stored at +0x21; rotates counters +1/+2/+3. Called every tick by 0x00100FA8. Cluster of 1. [I]
#### 0x00101060 `condflag_decay_fast` — logic
Every-tick decay pass over ~40 byte/word flag slots at obj+0x36..+0xb4 via helpers 0x00101506 (word), 0x0010151E, 0x001014CC (byte), 0x001014E4. Cluster of 1. [O]
#### 0x001011EC `condflag_decay_slow` — logic
Every-3rd-tick decay pass over ~80 flag slots at obj+0x10..+0xa4 using the same helpers (mostly 0x001014CC byte decay). Cluster of 1. [O]
#### 0x00101496 `condflag_decay_slowest` — logic
Every-0x21-tick decay pass over 6 slots (+0x22,+0x4b,+0x5a,+0x7a,+0x98) using byte/word/0x001014E4 helpers. Cluster of 1. [O]
#### 0x001014CC `condflag_decay_byte` — library
Decays one byte flag: *p = (*p > ticks) ? *p - ticks : 0, where ticks = obj+0xba elapsed count. Used ~130x by the three decay passes. Cluster of 1. [O]
#### 0x001014E4 `condflag_grow_byte` — library
Saturating-grow twin of 0x001014CC: *p = min(0xFF, *p + elapsed_ticks(obj+0xba)) — "time since event" counters. Used by all three decay passes. Cluster of 1. [O]
#### 0x00101506 `condflag_decay_word` — library
Word-width decay: *p = (*p > ticks) ? *p - ticks : 0, ticks = obj+0xba. Cluster of 1. [O]
#### 0x0010151E `condflag_grow_word` — library
Word-width saturating grow toward 0xFFFF by obj+0xba ticks. Cluster of 1. [O]
#### 0x0010153E `condflag_tickdiv_x10_update` — logic
Updates the x10 time base for the fast flag group: elapsed = obj+0xc - obj+0xb6 (values >0xf9 treated as 0), ticks = (elapsed+5)/10 stored at +0xba, base +0xb6 += ticks*10. Called first in every fast pass. Cluster of 1. [O]
#### 0x00101576 `condflag_tickdiv_x100_update` — logic
Same scheme for the x100 group: limit 0xcc, ticks = (elapsed+0x32)/100, base +0xb7 += ticks*100. Called by the every-3rd-tick pass. Cluster of 1. [O]
#### 0x001015AE `condflag_tickdiv_x1000_update` — logic
Same scheme for the x1000 group with u16 base +0xb8, limit 0xfe0a, ticks = (elapsed+500)/1000, base += ticks*1000. Called by the every-0x21-tick pass. Cluster of 1. [O]
#### 0x0010179C `sigcond_s1_init` — stub
Sub-object init for the signal-conditioning module owned by 0x00129F50: registers cb table PTR_PTR_0010189c (count 1) and zeroes +0xc and +0x24. Cluster of 1. [O]
#### 0x001017AC `sigcond_s1_cyclic` — logic
Cyclic step (caller 0x0012A208): reads input via fn-ptr, raises it to floor cfg[0x84], feeds interpolator object at +0x10 (vtable +0x14 call), converts result to +0x2c, floor-mapped u16 into +0x2e via 0x00121BF0-family helper, then HSM step/query (0x001236E4/0x00123916) latches +0x30. Cluster of 1. [I]
#### 0x00101B76 `sigcond_s2_init` — stub
Second sub-object init for the 0x00129F50 module: cb table PTR_PTR_00101c24, zeroes +0xc..+0x10. Cluster of 1. [O]
#### 0x00101B8C `sigcond_s2_eval` — logic
Latches three inputs (byte +0xd, u16 +0xe, byte +0x10) then runs HSM step/query on cfg PTR_PTR_00101c28, storing the state byte at +0x11. Caller 0x0012A208. Cluster of 1. [O]
#### 0x00101E9C `shcrit_gear_map_eval` — logic
Gear-indexed map evaluation: param_2 in 2..8 selects map slot +0x18..+0x30 of param_1, evaluated with (param_3, fn(param_4)) via 0x001218FA; any other gear value reports error 0x68 and returns 0. Callers=- (fn-ptr). Gear numbers 2-8 match the 8HP gear range. Cluster of 1. [I]
#### 0x00101FA0 `rangemon_set_status_code` — logic
Computes range-monitor status code into +0x12 (default 0xff): 0 keeps zeros; mismatch cases yield 3 (param_4==0 while active), 4 (both zero, prev!=0xf), 5 (param_5==0xf while active), 6 (param_3==0xf with param_4 set) — 0x0f acting as invalid lever position. Copies +0x17 to +0x16 and +0x14 to +0x15. Caller r0c_app 0x000F8D26. Cluster of 1. [I]
#### 0x00102010 `rangemon_hsm_eval` — logic
Sets busy flag +0x10=1, runs HSM step (0x001236E4) + state query (0x00123916) on cfg 0x001021d8/0x001021e0, latches result byte to +0x13, clears busy. Callers=- (fn-ptr). Cluster of 1. [O]
#### 0x0010203A `rangemon_apply_code` — logic
When +0x13, +0x18 set and +0x12 != -1: queries condition slot (cfg+8, code) and latches it via arbiter 0x00102DCC; re-checks codes 3/4/5/6 to set +0x1c bits 3/4/5/6; when +0x17==0 clears +0x20/+0x1a. Caller 0x000F8D54. Cluster of 1. [I]
#### 0x00102118 `condmon_init_cbs_5` — stub
Registers cb table PTR_PTR_001021ec (count 1) at +4/+8; fifth cb-registration stub from ctor 0x000F8FC4. Cluster of 1. [O]
#### 0x00102122 `shcrit_cyclic` — logic
Main cyclic of the shift-criteria sub-object (caller 0x000F912E): refreshes gear byte (*cfg+0xb0), runs condition slots 0/1/2/7 (0x00102AC6/0x00102B12/0x00102B6A/0x00102C52), source selector 0x00102BC6, result gatherer 0x00102E96 into +0x40, an enable gate +0x22 from ~8 cfg conditions, conditional HSM step into +0x13, and per-gear targets via 0x0010225E for current gear and (gear-1) stored at +0x30..+0x36 pairs. Spec 09 shift-map style access. Cluster of 1. [I]
#### 0x0010225E `shcrit_calc_gear_target` — logic
Per-gear target computation: base = map(cfg+0xb8, speed) * torque_pct(+0x20) / 100 into +0x38; when engaged (+0x17) and gear>=2 evaluates maps 0x00103052/0x00102FD0 into +0x30/+0x34 (current gear) or +0x32/+0x36 (other gear), builds a per-mille limit +0x24 (default 1000) from up to five cfg limiters, optionally re-blends the two map outputs with per-mille weights from cfg+0x9c; returns max(result,0). Heavy 2D-interp use (0x00121A56). Cluster of 1. [I]
#### 0x001028FA `condmon_lt_compare_thunk` — logic
Comparison thunk with unrecovered signature: returns (fn-ptr at 0x00102a20 result) < r0 input; callee is shared helper 0x0012164A. Callers=- (fn-ptr). Cluster of 1. [I]
#### 0x00102A56 `shcrit_state_request` — logic
State-request handler for the shift-criteria module: request 0x0b evaluates map (cfg+0x10, ptrs 0x00102c9c/0x00102ca4) and maps result 1->2 into global PTR_DAT_00102cb0; request 0x15 does the same with cfg+0x14 and status bytes PTR_DAT_00102cb8[0x12]/[0x4f]; other values pass through. Uses interp 0x00121A56 and byte-saturate 0x00121CC6. Cluster of 1. [I]
#### 0x00102AC6 `shcrit_cond_slot0` — logic
Condition slot 0: fires when state=='c' or u16 global PTR_DAT_00102cbc < speed at cfg+0x54; queries evaluator slot 0 (cfg+8), latches through arbiter 0x00102DCC and sets +0x1c bit 0. Cluster of 1. [I]
#### 0x00102B12 `shcrit_cond_slot1` — logic
Condition slot 1: gated by mode(cfg+0x78)==1 and predicate on PTR_DAT_00102cc4==1 (unless state=='c'); queries evaluator slot 1, arbiter 0x00102DCC, sets +0x1c bit 1. Cluster of 1. [I]
#### 0x00102B6A `shcrit_cond_slot2` — logic
Condition slot 2: gated by input-byte bits 0/1 at cfg+0xc together with cfg flags [0x17]/[0x18] (or state=='c'); evaluator slot 2, arbiter 0x00102DCC, +0x1c bit 2. Cluster of 1. [I]
#### 0x00102BC6 `shcrit_select_source` — logic
Source selector: mode = evaluator(cfg+0x80, fn()) in 0..2; when engaged (+0x17) computes +0x1e (0x00102E22), +0x3e (0x00102E3E), mapped +0x3c (cfg+0x84), +0x3a (0x00102E60); +0x20 = mode==1 ? +0x3a : mode==2 ? min(+0x3c,+0x3a) : 0; returns mode. Cluster of 1. [I]
#### 0x00102C52 `shcrit_cond_slot7` — logic
Condition slot 7: gated by global *PTR_DAT_00102cd8 or state=='c'; evaluator slot 7, arbiter 0x00102DCC, sets +0x1c bit 7 (0x80). Cluster of 1. [I]
#### 0x00102CDC `shcrit_peak_hold_clamp` — logic
Peak-hold filter at +0x2a of fn(*(short*)(param+0x38)): held = max(held, value), optionally raised to floor uVar8 (cfg+0xa4) when engaged (+0x17==1) and bVar1<bVar2; returns max(map1(cfg+0xa0 lane1), min(map0 lane0, held)). Callers=- (fn-ptr); 0x00102D1E is its register-interface twin. Cluster of 1. [I]
#### 0x00102DCC `shcrit_priority_arbiter` — logic
Priority arbiter for the shift-criteria sub-object: a new condition level (0/1/2) only overrides the latched level at +0xc when higher; level!=0 then runs an HSM step (0x001236E4) + state query (0x00123916) latching +0x13; illegal levels report error 0x3b via PTR_FUN_00102f98. Callers 0x00102AC6/0x00102B12/0x00102B6A/0x00102C52/0x0010203A. Cluster of 1. [O]
#### 0x00102D1E `shcrit_peak_hold_clamp__2` — logic
Register-interface twin of 0x00102CDC (same peak-hold at +0x2a with floor option and min/max map clamp) with Ghidra-unrecovered params (unaff_r12/r13, stack bytes). Separate cluster only because constants live in registers. Cluster of 1. [I]
#### 0x00102D82 `shcrit_percent_of_map` — logic
Percent-of-map computation: evaluates map cfg+0xa8 (via 0x001218FA) with (PTR_DAT_00102f7c, LAB), converts to a percent factor, multiplies map2(cfg tables, +0x2c) result and returns (pct*value)/100 u16-saturated. Callers=- (fn-ptr). Cluster of 1. [I]
#### 0x00102E22 `shcrit_eval_base_map` — logic
Evaluates map at cfg+0x18 with x = *(short*)(data+0x3ae) and y = *(short*)(data+900) via 0x001218FA; helper of the source selector 0x00102BC6 (stored to +0x1e). Cluster of 1. [I]
#### 0x00102E3E `shcrit_delta_from_base` — logic
Computes s16( param+0x1e - map(data, param+0x11) ) via 0x00121CD6 clamp — deviation of the base-map value from a per-state offset; stored by caller to +0x3e. Cluster of 1. [I]
#### 0x00102E60 `shcrit_pct_of_speed_map` — logic
value = fn(PTR_DAT_00102fa8); map(cfg+0xb8, value) scaled by u16 global PTR_DAT_00102fb0 as percent: returns (map*pct)/100. Stored to +0x3a by 0x00102BC6. Cluster of 1. [I]
#### 0x00102E96 `shcrit_mode_factor_calc` — logic
Mode selector (cfg+0xbc, param byte [5]) 1/2/3 evaluates maps cfg+0xc0/+0xc4/0xc8 with sVar3(PTR_DAT_00102fc0), then per-gear table fn (0x00121482) on cfg+0xcc/+0xd0/+0xd4 with gear-1 (byte at PTR_DAT_00102fc4[1]); result = factor*value/1000; default 1000*10000/1000. Result feeds +0x40 in 0x00102122. Cluster of 1. [I]
#### 0x00102FD0 `shcrit_gear_map_bank1_eval` — logic
Gear 2-8 selects map slot +0x38..+0x50 of the cfg object, evaluated 3-arg via 0x001218FA; any other gear reports error 0x55 and returns 0. Called by 0x0010225E for the current-gear target. Cluster of 2 (member 0x00103052 uses bank-2 slots +0x1c..+0x34 for the alternate gear). [I]
#### 0x001031A6 `vt_call_38_thunk` — stub
7-instruction vtable thunk: calls (*(obj->vt+0x38))(obj->vt[0x30] + obj) and discards the result. Callers=- (fn-ptr). Cluster of 1. [O]
#### 0x001032A6 `adpt_a_get_result` — logic
Returns u16-saturated (u16 at +8 minus 10*u16 at +0x14) when gear field +6 != 0 and engage flag +7 == 1, else 0 — result readout of adaptation tracker A (x10 scaled). Callers=- (fn-ptr). Cluster of 1. [I]
#### 0x001032D2 `vt_call_8c_is_zero` — stub
Vtable predicate: returns ((*(vt+0x8c))(vt[0x84]+obj) == 0). Callers=- (fn-ptr). Cluster of 1. [O]
#### 0x00103316 `adpt_a_out_of_bounds` — logic
Returns 1 when global LAB_00103384 < per-gear-table(cfg+8, gear-1) value or table(cfg+0xc, gear-1) < the same global — bounds check of a global against two per-gear limits (tables via s16-array read 0x000B6C94). Callers=- (fn-ptr). Cluster of 1. [I]
#### 0x0010338C `adpt_a_compute_send` — logic
Per-gear computation for adaptation tracker A: +0x12 = clamp(u16(+0xc)*10 - (fn(+8) + tbl[g-1]*-10)); +0x10 = fn(tbl(+0x10)*10 + +0x12) then 3-way clamped blend between tbl(+0x14)*10 and tbl(+0x18)*10; when engage flag +7==1 sends (gear, value)/(gear,1) via PTR_FUN_00103468/0x0010346c; sets done +0xe=1. Cluster of 1. [I]
#### 0x00103658 `adpt_b_get_result` — logic
Result readout of adaptation tracker B: gear +6, engage +7==1 and phase +8==1 required; returns u16-sat(u16(+0xa) - 10*u16(+0x14)). Callers=- (fn-ptr). Cluster of 1. [I]
#### 0x001036A2 `adpt_b_above_min` — logic
Returns threshold(*(short*)**(PTR_DAT_001036cc)) < s16_clamp(u16(+0xc) - global PTR_DAT_001036d8) — minimum-value check for tracker B (clamp via 0x000B7888). Callers=- (fn-ptr). Cluster of 1. [I]
#### 0x001036E0 `adpt_b_cyclic_eval` — logic
Gear 1-5 gated evaluation for tracker B: window check of +0x10 against per-gear deltas (tables +0x24/+0x28/+0x2c); outside the window evaluates per-gear maps +0x30..+0x40 with (fn(+0x10)-fn(tbl+0x24)); bad gear sets state +7=3; when +7==1 sends (gear,1) and (gear,value). Cluster of 1. [I]
#### 0x001037EC `adpt_b_start` — logic
Phase 1 entry: computes +0xa = u16-sat(fn(LAB_00103928-data, gear) + tbl(+8)[gear-1]*10), +0x16 = value/10+1, clears +0x14, sets phase +8=1. Cluster of 1. [O]
#### 0x0010383C `adpt_b_begin_blend` — logic
Phase 2: +0xc = global 0x00103934; +0x18 = tbl(+0xc)[gear-1]; initializes interpolator object at +0x20 through its vtable (+0x14) with (tbl(+0x20)[gear-1], 0, 0x7fff); +0x1c = tbl(+0xc)+tbl(+0x10); +0x1a = 0; phase +8=2. Cluster of 1. [O]
#### 0x001038B8 `adpt_b_set_final` — logic
Phase 3: +0x18 = tbl(+0x10)[gear-1]; sets phase +8=3. Cluster of 1. [O]
#### 0x001038D8 `adpt_b_hold_result` — logic
Phase 4: copies +0x1a into +0x10, sets +0x18=500, phase +8=4. Cluster of 1. [O]
#### 0x001038E8 `adpt_b_reset` — logic
Phase 5 / reset: clears +4, +0x12 = tbl(+4)[gear-1], phase +8=5. Cluster of 1. [O]
#### 0x00103938 `diagsm_run` — dispatch
Run-to-completion loop over state byte +1 (states 0-4, mirrored into global PTR_DAT_00103a48): state 1 requests a session switch (0x001039F0) -> 2 on success else 4 (setting bit 1 in param_2+3); state 2 -> 3 when 0x00103A1E != 0; state 3 -> 4 when 0x00103A1E == 0. Caller r0c_app 0x000CC3EA. Session codes via 0x000C949C diag-session validator. Spec 15 cross-ref. Cluster of 1. [I]
#### 0x001039B0 `diagsm_is_state3` — stub
Returns *(char*)(param+1) == 3. Caller 0x000CC45E. Cluster of 1. [O]
#### 0x001039B8 `diagsm_is_state4` — stub
Returns *(char*)(param+1) == 4. Caller 0x000CC4DA. Cluster of 1. [O]
#### 0x001039C0 `diagsm_reset` — logic
Resets state +1 to 0; when request byte +0 is 1 or 3 asks session validator 0x000C949C for session 3 or 5 respectively. Caller 0x000CC442. Cluster of 1. [I]
#### 0x001039F0 `diagsm_request_session` — logic
Maps request byte +0: 1 -> session 3, 3 -> session 5, others return 0; queries 0x000C949C(cfg 0x00103a4c, code, 1). Called by 0x00103938. Cluster of 1. [O]
#### 0x00103A1E `diagsm_check_session` — logic
Non-mutating twin of 0x001039F0: request byte 1 -> session 3, 3 -> session 5, else 0; only queries the session state via 0x000C9504. Called by the diag state runner 0x00103938. Cluster of 1. [O]
#### 0x00103A58 `diagsm2_run` — dispatch
Second run-to-completion state machine (state byte +1, states 0-6, output flag +2, mirrored to global PTR_DAT_00103ae4): 0 stays 0 (out 0); 1->2 (out 1); 2->3; 3 stays; 4->5 (out 0); 5 stays; anything else ->6 (out 0). Caller 0x000CC3EA. Cluster of 1. [O]
#### 0x00103AB0 `diagsm2_is_active` — stub
Returns state +1 in {3,5}. Callers 0x000CC45E/0x000CC4A2. Cluster of 1. [O]
#### 0x00103AC2 `diagsm2_is_state6` — stub
Returns state +1 == 6. Caller 0x000CC4DA. Cluster of 1. [O]
#### 0x00103AD0 `diagsm2_start` — stub
If state != 3, sets state 1. Caller 0x000CC410. Cluster of 1. [O]
#### 0x00103ADC `diagsm2_set_state4` — stub
Forces state +1 = 4. Caller 0x000CC42C. Cluster of 1. [O]
#### 0x00103AE8 `stmach_cell4_new` — stub
Allocates a 4-byte state cell via allocator 0x0009AA34(4) when param is NULL and initializes it to (6,0,0,0) — default state 6. Callers=- (fn-ptr). Cluster of 1. [O]
#### 0x00103B0E `evtpub_latch_code` — logic
Copies global code byte DAT_00103c8c into param_1 and writes the triple (+0x21 = code, +0x22 = code+1, +0x23 = code<0 ? 0 : code) into the listener object *(int*)PTR_DAT_00103c90. Caller 0x000CFFDE. Cluster of 1. [I]
#### 0x00103B36 `evtpub_publish_change` — logic
Change-publisher (caller 0x000D01E6): clears edge flags (0x00103C20), optionally evaluates edges (0x00103CEC), arms the latch on condition mismatch, and when a trigger fires (param[3] or 0x00103C2A) copies the new code via the +0x21..+0x23 triple, clears the latch, and fires a five-callback notification chain (PTR_FUN_00103cac..0x00103cd4) with (code, flag1, flag2). Cluster of 1. [I]
#### 0x00103C20 `evtpub_clear_edges` — stub
Zeroes bytes +1/+2/+3 of the 4-byte event cell. Cluster of 1. [O]
#### 0x00103C2A `evtpub_trigger_check` — logic
Composite gate returning 0 only when: first predicate 0, second predicate 0, state == 1, PTR_DAT_00103ca4[3]==0, [6]!=1, third predicate != 1, and globals 0x00103ce4 != 2 and 0x00103ce8 != 1. Otherwise 1 (trigger). Cluster of 1. [I]
#### 0x00103CEC `evtpub_eval_edges` — logic
Sets edge flags param[1] (via fn(cfg 0x00103da8, code, old, new)), param[2] (via fn(cfg 0x00103db0, old)) and param[3] under equality/none (-1) sentinel conditions on the old/new code pair — classic old-vs-new position edge detection. Cluster of 1. [I]
#### 0x00103DE2 `evtpub_publish_change2` — logic
Second publisher (callers 0x000D01C4/0x000D01E6): when any of ~7 mirror conditions differ, copies the new code into param_1 and listener +0x2b, then fires the notify chain (fn(cfg, old, new, ...), status calls, pending-flag handling) and finally syncs the mirror bytes. Cluster of 1. [I]
#### 0x00103EC0 `evtpub2_latch_code` — stub
Copies global *PTR_DAT_00103ed0 into param_1 and mirror *PTR_DAT_00103f34. Caller 0x000CFFDE. Cluster of 1. [O]
#### 0x00103F56 `posdec_eval` — logic
Gated table decode: when predicates 1/2 fail, global 0x00104030 set, third predicate fails and state not 3: if flag [4]==0 and state in {1,2} return without change; else *param_1 = PTR_PTR_00104048[*PTR_DAT_00104040 + *PTR_DAT_00104044*3]; the [4]!=0 path yields 1 only when *PTR_DAT_00104044==1, else 0. Cluster of 1. [I]
#### 0x00103FD6 `posdec_update` — logic
Wrapper: skipped when global PTR_DAT_0010404c != 0; predicate 1 -> result 0; predicate 2 (0x00104050) -> run 0x00103F56. Caller 0x000D01E6. Cluster of 1. [I]
#### 0x0010400E `posdec_table_get` — data-accessor
Returns byte PTR_PTR_00104048[param_3 + (param_2 & 0xff)*3] — row-of-3 decode table read. Caller 0x000D07B0. Cluster of 1. [O]
#### 0x00104054 `tempmon_new` — logic
Constructs the 0x18-byte temperature-tracker cell (allocates 0x18 bytes via 0x0009AA34 when param NULL): vtable DAT_001042c0 at +0x14, state 0xff, all u16 temps 0, later defaulted to -40 by 0x00104156. Callers 0x000D0496/0x000D0440. Cluster of 1. [O]
#### 0x00104090 `tempmon_eval_channels` — logic
When mode +0x11 in {1,2}: evaluates two channel picks (+0x10 via fn(cfg,+0x12,+0x13) and validity +0xf via 3-arg variant); maps both through fn(PTR_FUN_001042d4) into +0xa/+0xc with -40 (0xffd8) defaults — paired temperature readout with validity. Cluster of 1. [I]
#### 0x001040FA `tempmon_table_read` — logic
Per-channel table read: mode param_2 in {1,2,3} selects table (cfg 0x001042d8 +0x18/+0x1c/+0x20), index param_3 in 1..5 (index-1 via s16 array read 0x000B6C94); out-of-range yields 0; result s16-handled via 0x000B77BC. Cluster of 1. [I]
#### 0x00104156 `tempmon_reset` — logic
Re-initializes the tracker: state 0xff, mode/validity bytes 0, temps +0xa/+0xc = -40 (0xffd8), counters 0. Callers=- (fn-ptr). Cluster of 1. [O]
#### 0x00104182 `tempmon_select_source` — logic
On a changed nonzero (group param_2, index param_3) pair: invokes the tracker vtable method (*(vt+0x14)) to reset, stores the pair at +0x12/+0x13, sets mode *param = 1/2 by global PTR_DAT_001042e4, and evaluates a predicate into +0x11; returns 1 when changed. Cluster of 1. [I]
#### 0x001041E6 `tempmon_cyclic` — logic
Full evaluation: runs 0x00104090, then for group param_2 in 1..3 and mode 1/2 reads channel temps into +2/+4 (0x001040FA), subtracts offset byte (cfg+0x28) into +6/+8, and picks +0xe from +0x10/+0xf with mode/threshold plausibility checks (returns 1 selected, 2 fallback path, 0 implausible). Callers=- (fn-ptr). -40 defaults confirm temperature semantics; spec 04 analog-inputs cross-ref. Cluster of 1. [I]
#### 0x001042F4 `stmach_cell20_new` — logic
Allocates a 0x20-byte state cell (allocator PTR_FUN_00104540(0x20) when param NULL), registers vtable PTR_LAB_00104548 at +0x14, default state 6 at +0x18 and clears the five flag bytes +0x19..+0x1d. Caller 0x0015BD60. Cluster of 1. [O]
#### 0x001044BE `stmach_cell_flag_get` — data-accessor
Returns flag byte +0x19..+0x1d of the 0x20-byte state cell for index 1..5 (0 for other indices). Callers=- (fn-ptr). Cluster of 1. [O]
#### 0x001044FE `stmach_cell_flag_set` — data-accessor
Writes flag byte (index 1-5) of the 0x20-byte state cell to +0x19..+0x1d. Callers=- (fn-ptr). Cluster of 1. [O]
#### 0x00104568 `dtcmon_report_range_codes` — logic
Plausibility reporter (caller 0x000CFF0E): compares the three published position codes (PTR_DAT_00104808, [1], [3] with complement check against [4]) against the listener mirror (+0x21/+0x24/+0x27 of *(int*)PTR_DAT_00104804); on mismatch or out-of-range (-2..9 nonzero validity window) fires DTC event 0xac sub 1/2/3 with old/new environment bytes, then flags status via 0x000B75A6/0x000B79E4 helpers. Spec 13 cross-ref. Cluster of 1. [I]
#### 0x00104712 `dtcmon_report_range_codes2` — logic
Companion reporter: checks two more codes (*PTR_DAT_00104820, *PTR_DAT_0010481c) against listener +0x2b/+0x2c, firing DTC event 0xac sub 4/5 when >2 or mismatched. Caller 0x000CFF0E. Cluster of 1. [I]
#### 0x0010483C `stmach_cell2_new` — stub
Allocates a 2-byte state cell via allocator 0x0009AA34(2) when NULL; init (6,0). Callers=- (fn-ptr). Cluster of 1. [O]
#### 0x0010485E `posmon_pick_active_code` — logic
Selects the active position code into *param_1 from a fallback chain: default global 0x00104998, overridden by fn(cfg 0x0010499c)==1 -> 0x001049a4, flag [4]==1 -> fn(cfg), state [1] in {1,2} -> [7], flag [5]==1 -> 0x001049b8, else previous held 0x001049bc when cell[1]==1. Caller 0x000CFFDE. Cluster of 1. [I]
#### 0x001048C8 `dtcmon_lever_transition_check` — logic
Lever-transition plausibility with state +1 (0/1/2): in state 0 requires code in valid order (10x10 transition table at 0x0004f6a2 indexed [new+1][cur+1]), predicate checks and two confirmations via fn(cfg 0x001049d4) before latching 1; illegal path latches 0 and reports event 0x96 with 14 args via PTR_FUN_001049e8; state 2 exits silently. Caller 0x000CFFDE. Cluster of 1. [I]
#### 0x00104986 `evtpub_clear_transient` — stub
If cell state +1 == 2, sets it to 0. Called from the publisher 0x00103B36. Cluster of 1. [O]
#### 0x001049EC `stmach_cell3_new` — stub
Allocates a 2-byte state cell via 0x0009AA34(2); init (0,6) — state at byte 1. Callers=- (fn-ptr). Cluster of 1. [O]
#### 0x00104A0E `posmon_reset_triple` — logic
Sets cell state +1 = 6 and writes the (code, code+1, code) triple 6/7/6 into the listener at +0x24/+0x25/+0x26. Caller 0x000CFFDE. Cluster of 1. [O]
#### 0x00104A28 `posmon_resolve_current` — logic
Big fallback resolver (caller 0x000D01C4): picks the current position code from primary fn / secondary source / filtered source (with confirm predicates) and, whenever the choice changes, writes (code, code+1, code<0?0:code) into listener +0x24/+0x25/+0x26 and re-evaluates a fn(cfg 0x00104c0c); sets *param=1 on the filtered path. Cluster of 1. [I]
#### 0x00104C14 `lib_ring16_push` — library
Pushes a u16 into a 10-entry ring buffer (head byte at [0], data at +2+head*2, head wraps at >9). Cluster of 1. [O]
#### 0x00104C32 `lib_ring16_read_back` — library
History read: index = head - clamp(fn(p2/p3), <=9) mod 10; returns the u16 stored that many samples ago. Cluster of 1. [O]
#### 0x00104C70 `lib_ring16_fill` — library
Fills all 10 ring slots with the given u16 and resets head to 0 (initialization). Cluster of 1. [O]
#### 0x00104C90 `lib_ring32_push` — library
Pushes a u32/float into a 10-entry ring (head at [0], data at +4+head*4). Caller 0x00108D7C. Cluster of 1. [O]
#### 0x00104CAE `lib_ring32_read_back` — library
Float-ratio history read for the u32 ring: back-index = head - clamp(fn(p1/p2)) mod 10. Caller 0x00108D7C. Cluster of 1. [O]
#### 0x00104CF4 `lib_ratelimit_step16` — library
Rate-limited tracker: output u16 = s16-clamp(pct * (target - state/300)); then slews the internal state toward target*300 via 0x00104D7C. Callers across r0c/r10 units (0x00106064, 0x0010A930, 0x000D5E30, 0x000E1AF6...). Cluster of 1. [I]
#### 0x00104D24 `lib_diff_lowpass_f` — library
Stores (x - filtered)*gain*const into *p3 and low-passes p3[1] toward x via 0x00104DB8. Cluster of 1. [O]
#### 0x00104D4C `lib_max_s16` — library
max(s16, s16). Cluster of 2: member 0x00155088 is a byte-identical copy compiled into region r14_app (callers 0x001549CA/0x0015261C) — duplicated helper, same name. Cluster of 2. [O]
#### 0x00104D58 `lib_max_u16` — library
max of two values compared as u16. Cluster of 1. [O]
#### 0x00104D66 `lib_max_f32` — library
max(float, float). Cluster of 1. [O]
#### 0x00104D70 `lib_min_f32` — library
min(float, float). Cluster of 1. [O]
#### 0x00104D7C `lib_slew_toward_x300` — library
Percent-step slew: state += pct*(target*300 - state)/100. Widely shared (8 callers across units). Cluster of 1. [O]
#### 0x00104D98 `lib_decay_from_x300` — library
Reverse slew: state = target*300 + (100-pct)*(state - target*300)/100 — decays toward target with rate (100-pct). Cluster of 1. [O]
#### 0x00104DB8 `lib_lowpass_f32` — library
First-order float low-pass: y += gain*(x - y). 9 callers across units. Cluster of 1. [O]
#### 0x00104DC4 `lib_lowpass_tau_f32` — library
Time-constant low-pass: alpha = clamp(p3/p2, [DAT_00104e44, 1.0]); y += alpha*(x-y). Caller 0x00104DFE. Cluster of 1. [O]
#### 0x00104DEC `lib_lp2_init` — library
Initializes a 2nd-order filter cell: position = global *(u16*)PTR_DAT_00104e48, velocity (float at +2) = param_2. Caller 0x000E39DC. Cluster of 1. [O]
#### 0x00104DFE `lib_lp_step_out16` — library
Low-pass step via 0x00104DC4 on (param_2, param_3, timer delta) updating the float at +2, refreshes the timer stamp, returns u16-sat(filtered + DAT_00104e4c). Caller 0x000E3F84. Cluster of 1. [O]
#### 0x00104E54 `lib_lp2_step_scaled` — library
2nd-order lag step on (pos,vel) float pair with integer inputs: y += (p5/K)*v; v += (p5/K)*(((p2 - y)/max(p3/K,min) + K2*(p4/K)*v)/(p3/K)). Callers 0x000E2468, 0x0010A968, 0x0010B0B4, 0x0010C238, 0x0010C3E0. Cluster of 1. [O]
#### 0x00104EB0 `lib_lp2_step_f32` — library
Plain float 2nd-order lag: y += dt*v; v += dt*(((x-y)/tau + K*dt*v)/tau), tau floored at DAT_00104f24. Callers 0x00105B36, 0x00105876, 0x001064C6, 0x000D2F14, 0x001088DE, 0x000DE688. Cluster of 1. [O]
#### 0x00104EE2 `lib_lp2_step_dtfloor` — library
Same 2nd-order lag with dt floored at DAT_00104f2c and tau floored at dt. Caller 0x000EFE24. Cluster of 1. [O]
#### 0x00104F30 `lib_lin_scale` — library
Returns p1 + (p2-p3)*DAT_00104f5c/p4 — linear scaled offset. Callers 0x000E2F46, 0x000D777E. Cluster of 1. [O]
#### 0x00104F40 `lib_apply_scaled_fn` — library
Calls fn-ptr(PTR_FUN_00104f64) with p1*p2*DAT_00104f60. Callers 0x000E2F46, 0x000D777E. Cluster of 1. [O]
#### 0x00104F4C `lib_reg_math_expr` — library
Register-carrying float expression (fr8 + p4*(p3/fr9)) * (p1-p2); part of an inlined expression sequence (in_fr8/in_fr9 unrecovered). Cluster of 1. [I]
#### 0x00105034 `lib_get_u16_global` — stub
Copies *DAT_0010503c into *param_1 (single u16 global read). Caller 0x000D4C12. Cluster of 1. [O]
#### 0x00105040 `meas_scale_offset_calc` — logic
Calibration scaling: byte at param_1[2] mapped via fn(0x001050b8), multiplied by signed scale (cfg+0x7c, sign flag [0x122]) /1000 into param_1[1], then signed offset (cfg+0x7e, flag [0x124]) subtracted into *param_1; u16 conversions via 0x001050bc. Caller 0x000D5418. Cluster of 1. [I]
#### 0x0010508E `avg_u16_cell_new` — logic
Allocates a 6-byte average cell (0x0009AA34(6) when NULL): {u16 acc, u16, byte param_2}. Callers=- (fn-ptr). Cluster of 1. [O]
#### 0x001050C4 `avg_u16_cell_init` — logic
Allocates a 0x10-byte window cell (PTR_FUN_00105268), initializes it via 0x000D55C4(param_2,param_3) and zeroes the accumulator at +8. Callers=- (fn-ptr). Cluster of 1. [O]
#### 0x001050F8 `avg_u16_update` — logic
Sliding-window average: window change (param_3 vs +0xc) resets; pushes u16 param_2 through the ring (fn 0x0010527c), on full window acc = acc - oldest(0x00105278) + new, result = acc/window (0xffff on overflow). Caller 0x000F3F60. Cluster of 1. [O]
#### 0x00105184 `avg_f32_cell_new` — logic
Allocates a 0x18-byte float window cell (0x0009AA34(0x18)), init via 0x000D54D0, zeroes float acc +0x10. Callers=- (fn-ptr). Cluster of 1. [O]
#### 0x001051BE `avg_f32_update` — logic
Float sliding-window average (same push/pop/accumulate pattern, window param_2, value param_1); when the window-full flag returns 1.0 the average acc/window is u16-truncated. Caller 0x000F3F60. Cluster of 1. [O]
#### 0x00105298 `ramtest_byte` — library
Byte RAM writability test: write 0xAA-ish (-0x56), verify, write 'U', verify, restore original; returns 1 on success. Caller 0x000D56D8. Cluster of 1. [O]
#### 0x001052B8 `ramtest_word` — library
16-bit RAM writability test with 0xAAAB/0x5555 patterns, restore on success. Callers 0x000D56D8, 0x001273B0. Cluster of 1. [O]
#### 0x001052E8 `est_update_basic` — logic
Basic estimator step (callers 0x000D71B2/0x000D7454): reload input (0x0010535A), load coefficients (0x00105462), run a 4-point interpolation chain (PTR_FUN_001054a4 with +0x2c/+0x30/+0x34/+0x38 into +0x40) and a second blend (PTR_FUN_001054ac) from cfg 0x001054a8; stores +0x14/+0x10 and product +0x18 = +0x2c * +0x28. Cluster of 1. [I]
#### 0x0010535A `est_input_reload` — stub
Copies +0x20 into +0x3c (filter state) and +0x1c (current input). Cluster of 1. [O]
#### 0x00105368 `est_update_full` — logic
Full estimator step: input chosen by state (PTR_DAT_001054b4[0x34] 2/3 -> cfg 0x001054b8 value, else fn(+0x1c, cfg)), low-passed via 0x00105444; two interpolation chains produce +0x40/+0x44 stored to +0x14/+0x10, product +0x18, cfg output +0x24 (PTR_DAT_001054b4) and running sum +0x20; scaled u16 published to PTR_DAT_001054d8. Cluster of 1. [I]
#### 0x00105444 `est_input_lowpass` — logic
Low-passes +0x20 toward param via fn-ptr(PTR_FUN_001054dc) with rate +0xc into +0x3c, copied to +0x1c. Cluster of 1. [O]
#### 0x00105462 `est_load_coeffs` — logic
Fills coefficient words param[0xb..0xe] as scaled products of param[0..2] with constants 0x001054e0-0x001054f0. Cluster of 1. [O]
#### 0x001054F4 `est2_reset` — logic
Resets estimator-2 state: +0x30=0, +0x28=+0x24, +0x44=60000, +0x42=0, global 0x001056c8=0, *param_1 = *PTR_DAT_001056c4. Caller 0x000D7454. Cluster of 1. [O]
#### 0x0010551E `est2_cyclic` — logic
Floors *param at cfg 0x001056cc+0x1c, then picks the input through state/flag branches (flags 0x35, 0x001056d4, 0x001056dc; interp fns 0x001056d8/0x001056e4), accumulates +0x24 = +0x28 + value, copies to +0x20 and publishes scaled u16 to PTR_DAT_001056f0. Caller 0x000D7454. Cluster of 1. [I]
#### 0x001055C0 `est2_calc_outputs` — logic
Computes the two outputs +0x14 and +0x10 for estimator-2 from chained interpolations of (scaled u16 0x001056f4, cfg 0x001056c4, cfg tables) with adders 0x00105700/0x0010570c and the 0x001056d8 blend; negative branch when cfg +0x1c value < +8. Caller 0x000D7454. Cluster of 1. [I]
#### 0x0010566A `est2_clear_out10` — logic
Clears estimator-2 output +0x10 and reloads +0x14 from cfg (PTR 0x001056fc +0x18). Callers 0x000D7224/0x000D7454. Cluster of 1. [O]
#### 0x00105680 `est2_reset_from_s16` — logic
Resets estimator-2: +0x24 = s16(0x00105710)/const, copies to +0x20, scaled u16 to +0x2c, clears +0x10 and reloads +0x14 from cfg. Callers 0x000D71B2/0x000D7454. Cluster of 1. [O]
#### 0x00105714 `est3_reset` — logic
Resets estimator-3 state (cfg PTR_DAT_001059dc): 60000/0 counter pairs at +0x42/+0x44 and +0x46/+0x48, +0x30=0, loads +0x50/+0x54 from cfg 0x001059e0, *param=0, param[2] from 0x001059e4. Caller 0x000D7454. Cluster of 1. [O]
#### 0x00105752 `est3_calc_all` — logic
Fills estimator-3 outputs via interpolation chains (fn 0x001059f4/0x00105a04 with cfg tables 0x001059f0/0x00105a00): mode byte (0x001059fc) 2 vs other selects table banks; writes +0x80, +0x94 = base + product, +0x98, +0x7c, +0x74, +0x78 and delta +0x9c = const - param+8. Caller 0x000D7454. Cluster of 1. [I]
#### 0x00105876 `est3_cyclic` — logic
Cyclic step of estimator-3: threshold timer pair +0x46/+0x48 (reset 60000/0), branchy input selection with interpolations, 2nd-order filter call (0x00104EB0), accumulates param_1[1], publishes +0x14 and clamps +0x10 to min(0, value) when flag 0x00105a18==0. Caller 0x000D7454. Cluster of 1. [I]
#### 0x00105A98 `est4_init` — logic
Initializes estimator-4 (cell param_1 + cfg 0x00105d4c): 60000/0 pairs, +0x30=0, loads +0x58/+0x1c/+0x50/+0x54 from cfg 0x00105d50/0x00105d58, sets *param=1, param[1] = threshold compare, float param+8 = difference, counter pair at +0xc/+0xe. Caller 0x000D7454. Cluster of 1. [O]
#### 0x00105B36 `est4_cyclic` — logic
Cyclic step of estimator-4: timer pair maintenance, input select by interp compare, three-way state machine on param[0] (0 hold+count, 1 fast path, 2 alternate with codes 0xEA/0x60 written at +0xe/+0xf), publishes +0x14, clamps +0x10 min(0,...) when flag 0x00105d68==0. Caller 0x000D7454. Cluster of 1. [I]
#### 0x00105DAC `est5_calc_all` — logic
All-outputs computation for estimator-5 (cfg 0x00105f64): base value into +0x80, mode-2 scaling factor, product chains into +0x94/+0x98, table lookups into +0x7c/+0x74/+0x78, delta +0x9c. Caller 0x000D7454. Cluster of 1. [I]
#### 0x00105F9C `spdtrk_init` — logic
Initializes the speed tracker from cfg 0x00106258: primary speed (+0xfc) into +0xe/+0x1e/+0x1c/+0x20 and x300 copies (+0x3c/+0x28), +0x14 float, secondary (+0xfa) into +0xc/+0x40 with slew wrapper (PTR_FUN_0010625c), third (+0xf8) into +2/+0x30/+0x44, ratio-derived +0x1a, difference into +6/+8, and +0x38 = s16(+0xfe)*300; ring buffers primed via 0x00104C70. Caller 0x000D70DC. Cluster of 1. [O]
#### 0x00106064 `spdtrk_cyclic` — logic
Speed-tracker cyclic (caller 0x000DD2F8): slew-limited follow of the primary speed into +0x10 with hysteresis ((new-old)/2), offset accumulation into +0x11, x300 slew + divide-back (+0x1e -> +7/+8), second/third trackers with rate limits (0x00104CF4) and ring-buffer push/read (0x00104C14/0x00104C32), ratio calc, and difference +3/+4; condition-gated value into +0xa. Cluster of 1. [I]
#### 0x00106298 `est6_sum_clamp` — logic
Estimator-6 output: two interpolated values (mode branch on cfg[0x3c]) summed and clamped to cfg+0x28 into +0x24, copied to +0x20, scaled u16 published to PTR_DAT_0010651c. Callers 0x000D71B2/0x000D7454. Cluster of 1. [I]
#### 0x00106326 `est6_mux_select` — data-accessor
Selects between two cfg pointers (+0x18/+0x1c of 0x00106524) by flag/limit conditions and copies the pointed value into global 0x0010652c. Callers 0x000D71B2/0x000D7454. Cluster of 1. [O]
#### 0x00106356 `est6_reset` — logic
Resets estimator-6 (cfg 0x001064f8): 60000/0 pair +0x44/+0x42, +0x30=0, flag +0x3c from cfg, loads +0x50/+0x54/+0xc, +0x28=+0x24, and adds a branchy interp delta to +0x24. Callers 0x000D71B2/0x000D7454. Cluster of 1. [O]
#### 0x001063EC `est6_calc_all` — logic
Fills estimator-6 outputs: +0x80 from cfg, +0x98 when flag[0x35]==1 (else 0), +0x94 interp, +0x7c and +0x78 as scaled products, +0x74 = 0, +0x9c = +0xc - const. Callers 0x000D71B2/0x000D7454. Cluster of 1. [I]
#### 0x001064C6 `est7_input_prep` — logic
Estimator-7 input preparation: mode-2 branch selects input via interp, optional floor from per-index table (fn with index-1), stores difference into cfg+0x38, then output interp and final blend call with DAT const; publishes *PTR_DAT_00106688. Caller 0x000D7454. Cluster of 1. [I]
#### 0x00106610 `est7_reset` — logic
Resets estimator-7 (cfg 0x00106674): 0/60000 pair, +0x30=0, +0x28=+0x24, flag +0x3c, *param and +0x1c from cfg. Caller 0x000D71B2. Cluster of 1. [O]
#### 0x001066D6 `cantx3_send_triple` — logic
Sends a value through the three-message transmit group: change detection via fn(cfg 0x001068e8, param_2), prep 0x00106AE6, then three signal-set callbacks (cfg 0x001068e4/0x001068d4/0x001068e0) with (value, byte, mode, changed); clears +4/+7, stamps u16 +2 from 0x001068f4. Caller 0x000EEB5E (r0c_app CAN manager). Spec 08 cross-ref. Cluster of 1. [I]
#### 0x00106754 `cantx3_send_single` — logic
Mode-gated single send: clears change flag when mode byte differs, preps 0x00106AE6, sends (cfg 0x001068d4, 1, param_2, param_3, 0) and marks both [0x15] and [9] active. Caller 0x000EEBA6. Cluster of 1. [O]
#### 0x001067A4 `cantx3_stop_one` — logic
Stops the primary message: clears global 0x001068f8 on mode change, clears +8, stops via fn(0x001068fc), marks active flags. Caller 0x000EEBB0. Cluster of 1. [O]
#### 0x001067D6 `cantx3_complete_one` — logic
Clears +8, completes the message via fn(0x00106900) on cfg 0x001068d4, clears +5. Caller 0x000EEBB8. Cluster of 1. [O]
#### 0x001067EC `cantx3_complete_cond` — logic
When all three param bytes are 0: latch +9 = (+8==2) and clear +8; conditionally completes each of the three messages (fn 0x00106900 on cfgs e4/d4/e0); clears +5. Caller 0x000EEB80. Cluster of 1. [O]
#### 0x00106834 `cantx3_stop_all` — stub
Calls the stop fn (PTR_FUN_00106904) on all three message cfgs (e4/d4/e0). Caller 0x000EEB8C. Cluster of 1. [O]
#### 0x00106848 `cantx3_set_period_all` — logic
Sets the period via fn(0x0010690c, param_1+6) then updates all three message cfgs via fn(0x00106910). Caller 0x000EEB94. Cluster of 1. [O]
#### 0x00106866 `cantx3_set_mode` — logic
Sets message mode codes [0x15] of the three cfgs: param_2==4 branch distinguishes param_3==3 (d4=1, e4=0, e0=2) with global 0x00106914 = 3/4 by +8, else e4=6/d4/e0=6. Caller 0x000EEB9C. Cluster of 1. [O]
#### 0x001068AC `cantx3_any_active` — data-accessor
Returns 1 when any of the three message cfgs is active (nonzero) or primary active without the change flag. Caller 0x000EEA96. Cluster of 1. [O]
#### 0x00106918 `cantx3_ready_check` — logic
Transmit-readiness predicate: mode==4 path requires flag or FUN_001069DA(4) plus two fn checks; otherwise flag-gated fn(0x00106b2c) on cfg 0x00106b28/0x00106b30 or direct fn(0x00106b18, param_2). Caller 0x000EEAA4. Cluster of 1. [I]
#### 0x00106988 `cantx3_msgs_enabled` — data-accessor
Returns 1 only when global 0x00106b34 set and both message cfgs (0x00106b28/0x00106b30) have flag [0x16] set or [0x20] clear. Caller 0x000EEAB4. Cluster of 1. [O]
#### 0x001069BC `cantx3_flags_any_set` — data-accessor
Returns 1 when any of the three globals 0x00106b38/0x00106b3c/0x00106b40 is nonzero. Caller 0x000EEAC2. Cluster of 1. [O]
#### 0x001069DA `cantx3_get_s16_by_slot` — logic
Returns *(short*)(cfg+10) selecting cfg 0x00106b18 (slots 4/5), 0x00106b28 (6/7) or 0x00106b30 (8/9); other slots report DTC event 0x349 (code 0xa7) and return 0. Callers 0x00106918, 0x000EEB32. Cluster of 1. [O]
#### 0x00106A3E `cantx3_get_u8_by_slot` — logic
Byte variant: cfg[0x10] for slots 4/5 and 8/9, global byte 0x00106b4c for 6/7; else DTC 0x349 (code 0xd6), return 0. Caller 0x000EEB40. Cluster of 1. [O]
#### 0x00106AC4 `cantx3_state_in_set` — data-accessor
Writes 1 to *param when message state byte [0x11] (cfg 0x00106b18) is 11, 12 or 14, else 0. Callers=- (fn-ptr). Cluster of 1. [O]
#### 0x00106AE6 `cantx3_prepare_schedule` — logic
Message preparation: clears +8, computes a status code (1/2 by fn gates on 0x00106b54, 5 when +9 set, 6/7 on the alternate gate set), and when status in {1,2,5,7} and slot param_2 < 3 loads the period/id word from table PTR_LAB_00106cac[slot+1] into the message object (invalid values report DTC via 0x00106cc0), else selects from table 0x00106cb8. Callers 0x00106754/0x001066D6. Cluster of 1. [I]
#### 0x00106C16 `cantx3_arm_and_min_time` — logic
Arms +7 when a limit at cfg 0x00106cc8+0xc is reached and keeps *(short*)(param+2) at or below the global 0x00106cc4 value (minimum-time clamp). Caller 0x00106AB6. Cluster of 1. [I]
#### 0x00106C4A `cantx3_mode_gate` — logic
Returns gated mode: requires +8==1; returns cached +4 when set; else mode byte 0x00106cb4[0x2c] 1/2 resolved by threshold compares against cfg 0x00106cc8+4/+8 (latched to +4). Caller 0x00106AB6. Cluster of 1. [I]
#### 0x00106CD4 `inv_pair_check_report` — logic
Silent when (param_2 == param_1[0] && param_2 == ~param_1[1]) — a value/complement consistency pair; otherwise calls the report fn PTR_FUN_00106cf0. Caller 0x000DA418. Cluster of 1. [O]
#### 0x00106CF4 `selwr_106d08_a` — stub
Selector write: *PTR_DAT_00106d08 = DAT_00106d04 (one of a pair with 0x00106CFC). All 0x00106CF4-0x00106DB8 setters are called from the giant r0c_app dispatcher 0x000DA418. Cluster of 1. [O]
#### 0x00106CFC `selwr_106d08_b` — stub
*PTR_DAT_00106d08 = DAT_00106d0c (second value of the pair). Cluster of 1. [O]
#### 0x00106D10 `selwr_106d24_a` — stub
*PTR_DAT_00106d24 = DAT_00106d20. Cluster of 1. [O]
#### 0x00106D18 `selwr_106d24_b` — stub
*PTR_DAT_00106d24 = DAT_00106d28. Cluster of 1. [O]
#### 0x00106D2C `selwr_106d40_a` — stub
*PTR_DAT_00106d40 = DAT_00106d3c. Cluster of 1. [O]
#### 0x00106D34 `selwr_106d40_b` — stub
*PTR_DAT_00106d40 = DAT_00106d44. Cluster of 1. [O]
#### 0x00106D48 `selwr_106d5c_a` — stub
*PTR_DAT_00106d5c = DAT_00106d58. Cluster of 1. [O]
#### 0x00106D50 `selwr_106d5c_b` — stub
*PTR_DAT_00106d5c = DAT_00106d60. Cluster of 1. [O]
#### 0x00106D64 `selwr_106d78_a` — stub
*PTR_DAT_00106d78 = DAT_00106d74. Cluster of 1. [O]
#### 0x00106D6C `selwr_106d78_b` — stub
*PTR_DAT_00106d78 = DAT_00106d7c. Cluster of 1. [O]
#### 0x00106D80 `selwr_106d94_a` — stub
*PTR_DAT_00106d94 = DAT_00106d90. Cluster of 1. [O]
#### 0x00106D88 `selwr_106d94_b` — stub
*PTR_DAT_00106d94 = DAT_00106d98. Cluster of 1. [O]
#### 0x00106D9C `selwr_106db0_a` — stub
*PTR_DAT_00106db0 = DAT_00106dac. Cluster of 1. [O]
#### 0x00106DA4 `selwr_106db0_b` — stub
*PTR_DAT_00106db0 = DAT_00106db4. Cluster of 1. [O]
#### 0x00106DB8 `selwr_106dcc_a` — stub
*PTR_DAT_00106dcc = DAT_00106dc8 (pair with 0x00106DC0). Cluster of 1. [O]
#### 0x00106DC0 `selwr_106dcc_b` — stub
*PTR_DAT_00106dcc = DAT_00106dd0 (pair b with 0x00106DB8). Cluster of 1. [O]
#### 0x00106DD4 `selwr_106de8_a` — stub
*PTR_DAT_00106de8 = DAT_00106de4 (pair with 0x00106DDC). Cluster of 1. [O]
#### 0x00106DDC `selwr_106de8_b` — stub
*PTR_DAT_00106de8 = DAT_00106dec. Cluster of 1. [O]
#### 0x00106DF0 `selwr_106e04_a` — stub
*PTR_DAT_00106e04 = DAT_00106e00 (pair with 0x00106DF8). Cluster of 1. [O]
#### 0x00106DF8 `selwr_106e04_b` — stub
*PTR_DAT_00106e04 = DAT_00106e08. Cluster of 1. [O]
#### 0x00106E0C `selwr_106e20_a` — stub
*PTR_DAT_00106e20 = DAT_00106e1c (pair with 0x00106E14). Cluster of 1. [O]
#### 0x00106E14 `selwr_106e20_b` — stub
*PTR_DAT_00106e20 = DAT_00106e24. Cluster of 1. [O]
#### 0x00106E28 `selwr_106e3c_a` — stub
*PTR_DAT_00106e3c = DAT_00106e38 (pair with 0x00106E30). Cluster of 1. [O]
#### 0x00106E30 `selwr_106e3c_b` — stub
*PTR_DAT_00106e3c = DAT_00106e40. Cluster of 1. [O]
#### 0x00106E44 `selwr_106e58_a` — stub
*PTR_DAT_00106e58 = DAT_00106e54 (pair with 0x00106E4C). Cluster of 1. [O]
#### 0x00106E4C `selwr_106e58_b` — stub
*PTR_DAT_00106e58 = DAT_00106e5c. Cluster of 1. [O]
#### 0x00106E60 `selwr_106e74_a` — stub
*PTR_DAT_00106e74 = DAT_00106e70 (pair with 0x00106E68). Cluster of 1. [O]
#### 0x00106E68 `selwr_106e74_b` — stub
*PTR_DAT_00106e74 = DAT_00106e78. Cluster of 1. [O]
#### 0x00106E7C `selwr_106e90_a` — stub
*PTR_DAT_00106e90 = DAT_00106e8c (pair with 0x00106E84). Cluster of 1. [O]
#### 0x00106E84 `selwr_106e90_b` — stub
*PTR_DAT_00106e90 = DAT_00106e94. Cluster of 1. [O]
#### 0x00106E98 `selwr_106eac_a` — stub
*PTR_DAT_00106eac = DAT_00106ea8 (pair with 0x00106EA0). Cluster of 1. [O]
#### 0x00106EA0 `selwr_106eac_b` — stub
*PTR_DAT_00106eac = DAT_00106eb0. Cluster of 1. [O]
#### 0x00106EB4 `selwr_106ec8_a` — stub
*PTR_DAT_00106ec8 = DAT_00106ec4 (pair with 0x00106EBC). Cluster of 1. [O]
#### 0x00106EBC `selwr_106ec8_b` — stub
*PTR_DAT_00106ec8 = DAT_00106ecc. Cluster of 1. [O]

## Unit summary

Unit 34 (r10_app slice 0x001000D6-0x00106EBC): 220 clusters, 222 functions, 0 unknown.
Every function was classifiable from its decompile plus edges/manifest anchoring — no honest
`unknown` entries were needed.

Kinds (by names-CSV rows): logic 122, stub 58, library 31, data-accessor 9, dispatch 2.

Module map recovered for this slice:
- 0x001000D6-0x00100B32: "condmon" manager sub-objects (cb-table registration stubs, state
  request handlers, status-word builders, HSM cyclic steps) driven by r0c_app ctor 0x000F8FC4 /
  cyclic 0x000F9088.
- 0x00100C58-0x00100EF0: window/hysteresis state machine, accumulator integrator, HSM sample
  helpers of one sub-object (cyclic driver 0x00100C42).
- 0x00100F4C-0x001015AE: decaying/growing flag-array manager (tick dispatcher, adaptive period,
  x10/x100/x1000 time bases, decay/grow helpers) — event hold-time matrix.
- 0x0010179C-0x00102FD0: shift-criteria calculator ("shcrit": per-gear maps for gears 2-8,
  condition slots 0/1/2/7, priority arbiter 0x00102DCC, peak-hold, per-mille limit stack) —
  spec 09 shift-and-lockup cross-ref.
- 0x00102D1E-0x001039F0: two per-gear adaptation trackers (phases 1-5, x10 scale, send-when-
  engaged) and the first diag session state machine (session codes via 0x000C949C).
- 0x00103A1E-0x00104A28: second diag state machine, state-cell allocators (default state 6),
  position-code event publishers and resolvers, DTC reporters for event 0xac sub 1-5 and the
  lever-transition check (10x10 table at 0x0004F6A2, event 0x96) — spec 13/15 cross-refs.
- 0x00103DE2-0x00104090: temperature monitor (0xffd8 = -40 defaults, channels 1-5, modes 1-3,
  plausibility pick) — spec 04 cross-ref.
- 0x00104C14-0x00104DC4 and 0x00104DEC-0x001052B8: generic library (ring buffers, 1st/2nd-order
  filters, min/max, RAM tests, sliding-window averages).
- 0x001052E8-0x00106610: a family of float estimators (est..est7) plus a speed tracker (x300
  scaling, slew/ring history) driven by r0c_app cyclics 0x000D70DC/0x000D71B2/0x000D7454.
- 0x001066D6-0x00106C4A: a three-message CAN transmit group controller ("cantx3") driven by
  r0c_app 0x000EEA96-0x000EEBB8 — spec 08 cross-ref.
- 0x00106CD4-0x00106EBC: paired 4-instruction selector-write stubs (one of two constants into a
  RAM global) all dispatched from the giant r0c_app handler 0x000DA418 (which also owns the
  0x00106Exx-0x00107Axx handler bank outside this unit).

Cross-region leads for other units:
- r0c_app owns the managers driving this slice: 0x000F8FC4 (ctor), 0x000F9088/0x000F9062
  (cyclic), 0x000C21E4 (tick), 0x000D7454 (estimators), 0x000EEA96-0x000EEBB8 (cantx3),
  0x000DA418 (selector dispatcher) — those units should cross-check the call lists here.
- Shared r10_app helpers in other units: HSM engine 0x001236E4/0x00123916, table evaluators
  0x00121508/0x001218FA/0x00121A56/0x00121CFA, allocator 0x0009AA34 (r08_app), s16-array read
  0x000B6C94 and clamp family 0x000B77xx-0x000B78xx (r08_app).
- 0x00155088 (r14_app) is a byte-identical duplicate of lib_max_s16 0x00104D4C.
