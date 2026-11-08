# Unit 45 (r14_app)

Batch 1 (clusters 1-25)

#### 0x0014AE38 `mon_deb_cur_vs_trk` — logic
Debounce/hysteresis monitor step in the 0x14A6E8 safety-monitor block: compares |tracked byte (+0xc4) − current (+0xba)| against a threshold from the config struct at *(+0x94)+0x24, then ticks a complementary counter pair (+0x98/+0x9c, reloaded from cfg +0x14/+0x18) through PTR_FUN_0014af6c and toggles latch +0xcb. Same cfg-offset pattern as 0x14AF70/0x14B27A; cluster of 1. [I] spec 13 (safety-monitoring layer).
#### 0x0014AF70 `mon_deb_dual_window` — logic
Second monitor step from 0x14A6E8: two independent two-window hysteresis checks of byte +0xc2 against references +0xb8 and +0xba using inner (cfg +0xc) and outer (cfg +0x10) tolerances, driving counter pairs +0xa0/+0xa4 and +0xa8/+0xac (reload values cfg +0x1c/+0x20) and latches +0xcc/+0xcd. Called only by 0x14A6E8. [I] spec 13.
#### 0x0014B27A `mon_deb_absdiff` — logic
Third monitor step from 0x14A6E8: raw flag = |+0xb8 − +0xba| > threshold (cfg +0x30); on latch edge reloads counter pair +0xb0/+0xb4 from cfg +0x28/+0x2c, and after the complementary counter expires copies the debounced flag to +0xce (edge history kept in +0xd0). [I] spec 13.
#### 0x0014B34A `mon_vote_2of3` — logic
Combines the three debounced monitor flags: (+0xcc&&+0xcd)||+0xbd → +0xd5; (+0xcc&&+0xce)||+0xb9 → +0xd2; (+0xcd&&+0xce)||+0xbb → +0xd3; then any two of {+0xd2,+0xd3,+0xd5} sets summary +0xd7. Classic 2-of-3 voted fault summary of the 0x14A6E8 monitor block. [O] spec 13 (fault-group voting).
#### 0x0014B404 `mon_get_wrap_a` — stub
7-instr wrapper: tail-calls 2-instr getter 0x14B43C on the monitor struct reached via PTR_DAT_0014b4a4; members 0x14B412/0x14B420/0x14B42E wrap sibling getters 0x14B442/0x14B448/0x14B44E on the same struct. All called from 0x000F7298 (r0c_app polling loop) — per-field status fetch. Cluster of 4. [O]
#### 0x0014B454 `init_blk_4000_60` — stub
Init stub for a small block: u16 = 4000, bytes +5/+7 = 0x3c (60). Called by 0x00132980 (r10_app) once; constants match the conversion blocks that divide by 100 / multiply by 60. [I]
#### 0x0014B462 `cnv_u16_div100_st` — logic
Signal conversion with range check: if enable byte (+2) is 0 and raw u16 < 0x30d5 (12499), stores raw/100 at +5 with status 0, else status 2; status copied to +7 unless -1 (then 0xff); result code at +8. Called from 0x00133740 and scheduler step 0x14F7EA. [O]
#### 0x0014B4A8 `get_u32_glob_ptr` — data-accessor
Returns the u32 at the RAM global pointed to by PTR_DAT_0014b4f4; polled by the big r08_app gatherer 0x000B912C. [O]
#### 0x0014B4AE `get_u8_glob_ptr` — data-accessor
Returns the u8 at the RAM global pointed to by PTR_DAT_0014b4f8; polled by 0x000B912C. [O]
#### 0x0014B4B8 `cnv_u16_x60_st` — logic
Conversion step: when source flag byte is 0 and raw u16 (offset 2) != 0xfff, stores raw*0x3c (×60) at +8 with status 0, else status 2; keeps previous value otherwise. Scheduler step (0x14F7EA). Units suggest seconds→ticks or minutes→seconds. [I]
#### 0x0014B4FC `get_blk_b9_wrap` — stub
Wrapper: calls 2-instr getter 0x14B518 (byte field of struct PTR_DAT_0014b5a8); member 0x14B50A wraps 0x14B51E (byte at +9 of the same struct) with a different caller set (0x144108/0x077B90 vs 0x0F7298/0x0A7C9C). Cluster of 2. [O]
#### 0x0014B51E `get_blk9_byte9` — data-accessor
2-instr getter: returns byte at offset 9 of the passed block pointer. [O]
#### 0x0014B524 `copy_u16_deref` — stub
Copies ***(u16**)param into +4 of the block; part of the r10_app 0x132980 init/refresh chain. [O]
#### 0x0014B52E `sig_sample_hold_st` — logic
Samples a u16 through the block's source pointer with a hold-off timer: mode gate PTR_DAT_0014b5ac[0x8c]/[0x61]; while blocked it decrements the counter at +2 and forces status 2 when it reaches 0; status codes 0/1/2 mirrored to +9. Called from 0x133476 and monitor scheduler 0x14F34C. [I]
#### 0x0014B5B0 `get_u8_glob_w0` — data-accessor
Returns *PTR_DAT_0014b7b4 (first of four consecutive pointer slots b7b4/b7b8/b7bc/b7c0); polled by r08_app 0x000A7C9C. Members of the quadruple: 0x14B5B8 (w1), 0x14B5C0 (w2), 0x14B5C8 (w3). [O]
#### 0x0014B5B8 `get_u8_glob_w1` — data-accessor
Second of the quadruple: returns *PTR_DAT_0014b7b8; polled by r08_app 0x000A7C9C. [O]
#### 0x0014B5C0 `get_u8_glob_w2` — data-accessor
Third of the quadruple: returns *PTR_DAT_0014b7bc; polled by r08_app 0x000A7C9C. [O]
#### 0x0014B5C8 `get_u8_glob_w3` — data-accessor
Fourth of the quadruple: returns *PTR_DAT_0014b7c0; polled by r08_app 0x000A7C9C. [O]

#### 0x0014B5FC `clr_2x8_result` — stub
Clears two consecutive 8-byte result slots starting at +5 of the block; prologue of the 0x14B5D2 FSM sequence. [O]
#### 0x0014B5D2 `fsm_seq_monitor` — logic
Sequence driver: clears the two result slots (0x14B5FC), runs three sub-FSMs (0x14B6E6 mode-select, 0x14B872 direction-select, 0x14B96E gated copy), then 0x14B9A2 clamps each result slot to boolean 0/1 and latches it. Called by monitor scheduler 0x14F34C. [I]
#### 0x0014B61A `mon_report_u16lt_a` — logic
Fault monitor reporting through the spec-13 API: failed = (block flag +0x21==1 && **(cfg+0xc)==1); tested = (PTR_DAT_0014b7c8==1 && speed **(cfg+0x10) < *PTR_DAT_0014b7cc && +0x1a==0); passes both to the report(group,data,failed,tested) entry 0x000A0E70 via interface slot PTR_PTR_0014b7d4. Scheduler step (0x14F7EA). [O] spec 13 §2.
#### 0x0014B674 `mon_report_u16lt_b` — logic
Sibling fault monitor of 0x14B61A with the same report call: failed = (+0x2e==1 && **(cfg+4)!=0); tested = (PTR_DAT_0014b7c8==1 && **(cfg+0x14) < *PTR_DAT_0014b7cc) combined with the direction latch pair (+0x27/+0x29 vs PTR_DAT_0014b7d8). Scheduler step (0x14F7EA). [O] spec 13 §2.
#### 0x0014B6E6 `fsm_mode_sel2` — logic
Two-way mode selection FSM (mode from PTR_DAT_0014b7e0 = 1/2, qual gate PTR_DAT_0014b7e4): validates counters +0x19 (<4) / +0x1b (<3) and request states +0x19/+0x1b (0/1/2), producing selected state +4 in {0..3}, request flags +0x1d/+0x1e, busy +0x21; resets its 5-byte status via 0x14B9CC and copies to result slot via 0x14B9DA. Part of 0x14B5D2 sequence. [I]
#### 0x0014B872 `fsm_dir_sel` — logic
Direction-selection FSM gated by PTR_DAT_0014ba38==0x0e and PTR_DAT_0014ba34[0x6d] in {1,2}: maps request states +0x26/+0x27/+0x29 in {0,2,3} (with qualifier [0x46]) to exclusive outputs +0x2a/+0x2b and done +0x2e; bit 0x20 of [0x4c] forces +0x2e. Part of 0x14B5D2 sequence. [I]
#### 0x0014B96E `gate_copy_en` — logic
Gated copy step of 0x14B5D2: if the byte at the third source pointer is non-zero, copies +0x2f→+0xe and +0xc→+0x37; always refreshes the status slot via 0x14B9CC/0x14B9DA. [O]

Batch 2 (clusters 26-50)

#### 0x0014B9A2 `fsm_clamp_bool` — logic
Final step of the 0x14B5D2 sequence, run twice: forces the two leading bytes of each 8-byte result slot to boolean (values >1 become 0) and copies bytes [0..3] to [4..7] as the latched previous result. [O]
#### 0x0014B9CC `clr_status5` — stub
Zeroes the 5 status bytes (+8..+0xc) of a sub-block; prologue used by all three sub-FSMs of 0x14B5D2. [O]
#### 0x0014B9DA `fsm_or_result_bits` — logic
Merges sub-FSM status bytes into the two 8-byte result slots: for slot index b in {1,2}, if (param_2 & b) the slot's bytes 0/1/2/3 get OR-ed with +8/+9/+0xa/+0xb/+0xc of the sub-block. Called by 0x14B6E6/0x14B872/0x14B96E. [O]
#### 0x0014BA3C `get_u8_glob_bb40` — data-accessor
Returns *PTR_DAT_0014bb40; polled by r08_app gatherer 0x000B912C. [O]
#### 0x0014BA44 `init_sts_0_2_1` — stub
Init stub: bytes +0xa=0, +0xb=2, +0xc=1; called once by 0x00132D8E (r10_app). [O]
#### 0x0014BA52 `fsm_seq_wait5` — logic
Five-state sequencer (state at +0xc in 1..5): each state waits for specific condition bytes (+5/+7/+9/+6/+2/+1) to clear, then sets outputs +0xa/+0xb and advances to the next state (2→3→4→5 via values 3/4/5); timeouts reported as state 1/2. Scheduler step of 0x14F34C. [I]
#### 0x0014BB34 `get_u8_deref` — data-accessor
4-instr getter: returns **(u8**)blk (first byte of the block's source pointer). [O]
#### 0x0014BB48 `get_i16_glob_bb8c` — data-accessor
Returns the i16 at PTR_DAT_0014bb8c; polled by 0x000B912C. [O]
#### 0x0014BB50 `cnv_raw_m2000_d4` — logic
Conversion step of 0x14F34C: when enabled and raw u16 (+2) != 0x1fff, stores (raw−2000)/4 at +0xc with status 0; raw==0x1fff gives status 1, disabled gives 2 with output 0. Offset/slope suggests a scaled sensor (e.g. angle or temperature). [I]
#### 0x0014BB90 `meas_get_i16_be14` — data-accessor
Getter family polled across regions (0x0F7298, 0x0A7C9C, 0x1534FC, 0x0F471C, 0x155AE4, 0x0B912C) — consumers include the 0x155xxx measurement-protocol block (spec 10). This one returns the i16 at PTR_DAT_0014be14. Siblings: 0x14BB96 (be18), 0x14BB9C (be1c), 0x14BBA2 (be20), 0x14BBA8 (u8 be24), 0x14BBB0 (be28), 0x14BBB6 (be2c). [I] spec 10.
#### 0x0014BB96 `meas_get_i16_be18` — data-accessor
Returns i16 at PTR_DAT_0014be18; polled by 0x0F471C and 0x155AE4 (measurement block). [I]
#### 0x0014BB9C `meas_get_i16_be1c` — data-accessor
Returns i16 at DAT_0014be1c; polled by 0x144108 (the 5322-instr r14 giant) and r04_boot 0x077AC4. [I]
#### 0x0014BBA2 `meas_get_i16_be20` — data-accessor
Returns i16 at PTR_DAT_0014be20; polled by 0x0ADEA0 (r08_app). [I]
#### 0x0014BBA8 `meas_get_u8_be24` — data-accessor
Returns u8 at DAT_0014be24; polled by 0x0F7298, 0x1350DC, 0x0AE5A0 and 0x0B912C. [I]
#### 0x0014BBB0 `meas_get_i16_be28` — data-accessor
Returns i16 at PTR_DAT_0014be28; polled by 0x0F471C. [I]
#### 0x0014BBB6 `meas_get_i16_be2c` — data-accessor
Returns i16 at PTR_DAT_0014be2c; polled by 0x0F471C. [I]
#### 0x0014BBBC `chk_compl_fatal` — logic
Safety guard: verifies the complementary pair (*p == ~p[1]); on mismatch calls the fatal internal-error handler (0x07A914/0x0794C8, spec 13 §2 0x6865C-family) through PTR_PTR_0014be34/be38 and spins forever. Member 0x14BBE4 is byte-identical with different callers (0x14BD30/0x14C23E vs 0x14C460). Cluster of 2. [O] spec 13.
#### 0x0014BC0C `mon_snap_floats` — logic
Snapshot step of the 0x14BD30 float monitor: copies u16/byte signal pairs from global structs (PTR_DAT_0014be3c/be40/be44/be48) into block floats +0xc..+0x54 with complements, computes (int-scaled * factor)/const & 0xffff into +0x44 and a table lookup via PTR_FUN_0014be54 into +0x38; releases locals via PTR_FUN destructors at the end. [I]
#### 0x0014BD30 `mon_fsel_debounce` — logic
Float selection monitor: picks source value/limit (param[9]/[10], 2.0f marker 0x40000000) from the block mode (**(cfg) 0/1/>=2) and validity floats; when the cfg+0xc gate is 1, debounces the switch with the complementary counter pair +4/+8 guarded by 0x14BBE4; writes selected pair to +0xb/+0xc and refreshes complements. Called by 0x14BF86. [I]
#### 0x0014BF34 `call_rpt_1_83` — stub
Single indirect call (*DAT_0014c09c)(PTR_DAT_0014c098, 1, 0x83) — reports event/entry with constants 1 and 0x83 (131); called from 0x148DBA. [I]
#### 0x0014BF42 `mon_blk_init` — stub
Init of the 0x14BF86 monitor block: seeds complementary copies of the cfg u16 (+4) and byte (+0x10) samples, clears latches +0x9e/+0x9f, sets +0x70=~+0x6e and +0x72=0xffff. [O]
#### 0x0014BF86 `mon_plaus_main` — logic
Main speed-signal monitor (221 instr, from 0x133946 and scheduler 0x14F34C): refreshes inputs (0x14C23E), runs the float select (0x14BD30), then performs three independent plausibility checks — each compares a computed float against a stored u16 complement pair within ±9 and |diff| <= DAT_0014c0a8 — and on any mismatch jumps to the fatal handler (PTR_PTR_0014c2c4) + infinite loop; otherwise updates complements and runs the ramp (0x14C460) and filter (0x14C500) steps. [I] spec 13 (safety-monitoring layer).
#### 0x0014C196 `mon_snap_scale` — logic
Input snapshot of the 0x14BF86 module: copies raw u16/byte values from PTR_DAT_0014c2cc/d0/d4/d8 into +0x80..+0x9a with complements, computes (u16 * s32)/10000 into +0x88 and a table lookup (PTR_FUN_0014c2dc, limit +0x80) into +0x96. [O]
#### 0x0014C23E `mon_signal_select` — logic
Validity/selection FSM of the 0x14BF86 module: derives quality +0x76 (0/1/2) from mode **(cfg) and invalid flags +0x8c/+0x8e/+0x94/+0x82, selects raw (+0x80) vs computed (+0x88) into +0x6e, debounces the fallback to the substitute value +0x96 through the complementary counter guarded by 0x14BBE4, and finally verifies the block checksum via PTR_FUN_0014c528 (expects 'Z') else fatal halt. [I]
#### 0x0014C460 `mon_ramp_hold` — logic
Ramp/hold step of 0x14BF86: latches "started" (+0x9e) on first non-zero input; while the complementary counter +0x5c/+0x60 (guard 0x14BBBC) counts down and not started, output is the cfg hold value **(cfg+8), otherwise the ramp value +0x74; result clamped to 0xffff when invalid flags set. [I]

Batch 3 (clusters 51-75)

#### 0x0014C500 `mon_set_subst_flag` — logic
Substitute-mode flag of the 0x14BF86 module: sets +0x7e = 1 when the counter PTR_DAT_0014c538 exceeds 3 or signal quality +0x76 is non-zero (i.e. substituted/invalid). [O]
#### 0x0014C53C `meas_get_i16_c7bc` — data-accessor
Measurement-value getter family (consumers 0x1534FC, 0x134D8E, 0x0B912C etc.): returns i16 at PTR_DAT_0014c7bc. Siblings in this batch: 0x14C542 (u8 c7c0), 0x14C54A (i16 c7c4), 0x14C550 (u8 c7c8), 0x14C558 (i16 c7cc), 0x14C55E (u8 c7d0), 0x14C566 (i16 c7d4), 0x14C56C (u8 c7d8), 0x14C574 (u8 c7dc). [I] spec 10.
#### 0x0014C542 `meas_get_u8_c7c0` — data-accessor
Returns u8 at DAT_0014c7c0; polled by 0x1350DC, 0x1534FC, 0x0B912C. [I]
#### 0x0014C54A `meas_get_i16_c7c4` — data-accessor
Returns i16 at PTR_DAT_0014c7c4; polled by 0x0F7298, 0x0A7C9C, 0x0F471C, 0x155AE4. [I]
#### 0x0014C550 `meas_get_u8_c7c8` — data-accessor
Returns u8 at DAT_0014c7c8; polled by 0x0A7C9C, 0x1534FC, 0x155AE4. [I]
#### 0x0014C558 `meas_get_i16_c7cc` — data-accessor
Returns i16 at PTR_DAT_0014c7cc; polled by 0x0F471C. [I]
#### 0x0014C55E `meas_get_u8_c7d0` — data-accessor
Returns u8 at PTR_DAT_0014c7d0; polled by 0x1534FC. [I]
#### 0x0014C566 `meas_get_i16_c7d4` — data-accessor
Returns i16 at DAT_0014c7d4; polled by 0x144108 (r14 giant) and r04_boot 0x077AC4. [I]
#### 0x0014C56C `meas_get_u8_c7d8` — data-accessor
Returns u8 at PTR_DAT_0014c7d8; polled by 0x0B912C. [I]
#### 0x0014C574 `meas_get_u8_c7dc` — data-accessor
Returns u8 at PTR_DAT_0014c7dc; polled by 0x0B912C. [I]
#### 0x0014C57C `chk_compl_fatal_c` — logic
Third copy of the complementary-pair guard (*p == ~p[1], else fatal handler + infinite loop) used by 0x14CBF2; identical in shape to cluster 0x14BBBC but bound to different literal-pool slots (PTR_PTR_0014c7e0-e8). [O] spec 13.
#### 0x0014C5A4 `mon2_snap_floats` — logic
Float snapshot of monitor module 2 (0x14C818): reads PTR_DAT_0014c7ec/7f0/7f4/800/804 and two indirect flag reads (PTR_PTR_0014c7fc), builds complement float pairs at param[3..6], param[0xc..0x11]; "all clear" test sets valid float param[0xb] = 0.0/1.0; selects output value/status pair param[0x12]/[0x13] (status 0 or 2.0). [I]
#### 0x0014C746 `mon2_blk_init` — stub
Init of monitor module 2 (from 0x132980): seeds two complementary counter pairs from cfg(+0x50)+0x10, clears latches, sets 0xffff/0xff defaults across +0x74..+0xa0. [O]
#### 0x0014C818 `mon2_main` — logic
Main monitor of module 2 (211 instr, from 0x133946 and scheduler 0x14F34C): enter-critical (0x71), snapshot (0x14CAA8, 0x14C5A4), verifies float-vs-u16 complement consistency (mismatch → fatal handler + halt), ramps the setpoint into +0x72 via PTR_FUN_0014ca78, checks the block checksum via PTR_FUN_0014ca84 ('Z' expected, else fatal), writes outputs +0x66/+0x1a/+0x9a/+0x27, runs the dual ramp (0x14CBF2) and mode steps (0x14CA16, 0x14CA32), finishes with a complement sweep (fatal on break) and a one-shot exit-critical call. [I] spec 13.
#### 0x0014CA16 `mon2_set_subst` — logic
Substitute flag of module 2: +0x88 = 1 when PTR_DAT_0014caa0 > 3 or +0x68 non-zero. [O]
#### 0x0014CA32 `mon2_mode_map` — logic
Maps PTR_DAT_0014caa4[0x51] (1/2) to output mode +0x78 (1/2) with OK status +0x79 = 0; when substituted (+0x88 != 0) forces +0x78 = 0 and status 2. [O]
#### 0x0014CAA8 `mon2_snap_select` — logic
Snapshot/selection of module 2: gathers raw bytes/u16s from PTR_DAT_0014cd20/24/28/34 plus a computed pair through PTR_PTR_0014cd30 with complements, derives the "any invalid" bool +0x9f, selects raw (+0x7c) or alternate (+0x90) into +0x6a with quality +0x6c (0/2). [I]
#### 0x0014CBE6 `call_rpt_1_71` — stub
Single indirect report call (PTR_DAT_0014cd40, 1, 0x71); sibling of 0x14BF34 with constant 0x71 instead of 0x83; called from 0x148DBA. [I]
#### 0x0014CBF2 `mon2_ramp_dual` — logic
Dual ramp/hold of module 2 (mirror of 0x14C460 for two channels): ramps +0x84 and +0x86 from +0x7c/+0x80 via PTR_FUN_0014cd48 with hold values from cfg(+0x50)+0x14 while the complementary counters +0x54/+0x5c (guard 0x14C57C) run; clamps to 0xfffe and 1; outputs +0x74/+0x76; invalid forces 0xffff/0. [I]
#### 0x0014CDFC `meas_get_i16_d074` — data-accessor
Returns i16 at PTR_DAT_0014d074; polled by 0x0F471C. [I]
#### 0x0014CE02 `meas_get_i16_d078` — data-accessor
Returns i16 at DAT_0014d078; polled by 0x0F471C. [I]
#### 0x0014CE08 `meas_get_i16_d07c` — data-accessor
Returns i16 at PTR_DAT_0014d07c; polled by 0x0F7298. [I]
#### 0x0014CE0E `meas_get_u8_d080` — data-accessor
Returns u8 at PTR_DAT_0014d080; polled by 0x0F7298. [I]
#### 0x0014CE16 `meas_get_i16_d084` — data-accessor
Returns i16 at PTR_DAT_0014d084; polled by 0x0A7C9C, 0x155AE4, 0x0AE5A0. [I]
#### 0x0014CE1C `meas_get_u8_d088` — data-accessor
Returns u8 at PTR_DAT_0014d088; polled by 0x0A7C9C. [I]

Batch 4 (clusters 76-100)

#### 0x0014CE24 `meas_get_i16_d08c` — data-accessor
Returns i16 at PTR_DAT_0014d08c; polled by 0x155AE4 (measurement block). [I]
#### 0x0014CE2A `meas_get_i16_d090` — data-accessor
Returns i16 at PTR_DAT_0014d090; polled by 0x0F7298, 0x132D8E and 0x14FC5C. [I]
#### 0x0014CE30 `meas_get_u8_d094` — data-accessor
Returns u8 at PTR_DAT_0014d094; polled by 0x0F7298. [I]
#### 0x0014CE38 `chk_compl_fatal_d` — logic
Fourth copy of the complementary-pair guard (*p == ~p[1], else fatal handler + halt); used by 0x14D48A and 0x14CE60. [O] spec 13.
#### 0x0014CE60 `mon3_compute1` — logic
First compute step of monitor module 3 (496 instr): builds float complement snapshots from PTR_DAT_0014d0a4-c4, computes a ratio (u16*u16*K)/(u16*K2) into param[9], a weighted blend (a*w1+b*w2)/(w1+w2) into param[10] (with RAM one-shot DAT_fff952b0/b4), a scaled product into param[0xe], and selects param[5]/[6] quality/value (1.0/2.0 markers); ends with a ramp/latch block using the 0x14CE38 counter guard. [I]
#### 0x0014D384 `mon3_blk_init` — stub
Init of monitor module 3 (from 0x132980): 0xffff/0xff defaults at +0x50..+0x72 and complementary seeds from cfg(+0xf)+0xc. [O]
#### 0x0014D38E `mon3_blk_init_b` — stub
Variant init of module 3 with register-passed seed values (decompiled with unaff registers; callers='-', likely an alternate entry of the same init). [I]
#### 0x0014D3E8 `call_rpt_1_72` — stub
Single indirect report call (PTR_DAT_0014d5bc, 1, 0x72); sibling of 0x14BF34/0x14CBE6. [I]
#### 0x0014D3F4 `cnv_u16_shr3_st` — logic
Range-checked conversion: u16 at +0x48 divided by 8 into +0x6e with status +0x70 (0 ok / 2 invalid when raw > 0xfffe or gate +0x4a set); complements at +0x72/+0x74. Part of module 3. [O]
#### 0x0014D406 `cnv_u16_shr3_fix` — logic
Unconditional variant of 0x14D3F4 (value>>3, status 0, complements) taking the input in a register; callers='-'. [O]
#### 0x0014D446 `mon3_compute2` — logic
Second compute step of module 3: ratio (u16a*u16b*12)/(u16c*125) into +0x76; blend with weights from cfg(+0x3c); second scaled product ((u16b*(x/1000)*6)/10000) into +0x82; quality/value selection +0x4e/+0x4c; fallback formula ((u34*(3*max(...))/u30)/50000) into +0x54 with debounce via the 0x14CE38 counter pair +0x40. [I]
#### 0x0014D48A `mon3_compute2_alt` — logic
Register-argument duplicate of the 0x14D446 body (callers='-'); Ghidra shows it as a separate 309-instr entry sharing the same code — treat as the same compute step reached by another path. [I]
#### 0x0014D79E `mon3_main` — logic
Main of monitor module 3 (513 instr, from 0x133946 and scheduler 0x14F34C): enter-critical (0x72), converts raw (0x14D3F4), sweeps complement pairs of the input structs (mismatch → fatal handler + halt), runs compute steps 0x14CE60/0x14D446, then six chained float-vs-u16 consistency checks (±9, tolerance DAT_0014dae0) — any failure is fatal; finally refreshes all complements. Same architecture as 0x14BF86/0x14C818. [I] spec 13.
#### 0x0014DC68 `meas_get_u8_de88` — data-accessor
Returns u8 at DAT_0014de88; polled by r04_boot 0x0794FC/0x079570. [I]
#### 0x0014DC70 `cond_not_active` — logic
Small condition step: +0x12 = 0 when +0x15==1 or the indirect query PTR_PTR_0014de90 returns non-zero, else 1 (an "enable/active" flag used by the 0x14DCB2 monitor chain). [I]
#### 0x0014DCA8 `copy_u16_cfg28` — stub
Copies u16 **(cfg+0x28) into +8 of the block; reload step from 0x132D8E. [O]
#### 0x0014DCB2 `mon4_seq` — logic
Sequence of monitor module 4 (from scheduler 0x14F784 and 0x133C4E): runs the state FSM 0x14E170, manages the timer u16 from cfg+0x28 (decrement under condition, reload when PTR_DAT_0014de98==1), then 0x14DC70, 0x14DCA0 and the reporting step 0x14E162. [I]
#### 0x0014DD24 `mon_report_st8_4` — logic
Fault-report monitor: failed = (+0xa != 8 && +0xe == 4), tested always 1; passes both to report() (0x000A0E70) via interface slot PTR_PTR_0014dea4. Scheduler step 0x14F784. [O] spec 13 §2.
#### 0x0014DD52 `mon_report_speed_cond` — logic
Fault-report monitor (140 instr): builds tested/failed from speed-window comparisons (thresholds in PTR_DAT_0014deac/deb0), enable flags, mode codes +0xa (6 or 8 — gear/lever codes), the +0x12 latch and an indirect qualifier PTR_PTR_0014deb8; reports via 0x000A0FA0/0x000A0E70 slots. Called by 0x14E162. [I] spec 13 §2.
#### 0x0014E03E `mon_report_u16lt_c` — logic
Fault-report monitor of scheduler 0x14F7EA: failed combines (+0x12==1) with speed +0xc < **(cfg+0x1c) or the window flag from DAT_0014e0c8+10; tested = (+0xc >= **(cfg+0x1c)); report through PTR_PTR_0014e0f0 (report API). [O] spec 13 §2.
#### 0x0014E0FC `mon4_timer_status` — logic
Timer/status step: when both indirect conditions and the timer allow, sets status +0x11=0 and reloads the timer from cfg+0x28, else status 1/2. Called from 0x14E0B6. [I]
#### 0x0014E162 `mon_report_gear68_b` — logic
Reporting step of module 4: calls 0x14DD52 then evaluates the same speed-window/mode-6-or-8 condition set, manages the +6 timer and the one-shot RAM latch DAT_fff95fca/e8, and ends with a report() call through PTR_PTR_0014e0f0 with tested = (+0xc <= threshold). [I] spec 13 §2.
#### 0x0014E170 `mon4_state_fsm` — logic
3-state FSM of module 4 (states +0x15 = 0/1/2, first-run latch via DAT_fff952bc/fca): state 0 requires speed windows (cfg+0x1c/+0x20) and mode 8 / (mode 6 && **(cfg+0x30)==1) to set request +4 and move to 1; state 1 decrements the timer (cfg+0x2c reload) and clears on timeout windows; state 2 waits for speed drop. [I]
#### 0x0014E274 `get_u16_wrap_e2bc` — stub
7-instr wrapper: getter 0x14E282 on PTR_DAT_0014e2bc; polled by 0x0F7298. [O]
#### 0x0014E288 `cnv_byte_x25_st` — logic
Conversion: when both gate bytes are 0 and raw byte < 0xff, out = (flag==1) ? raw*0x19 (25) : 0 with status 0; else out 0 status 2. Scheduler step (0x14F34C/0x133476). [O]

Batch 5 (clusters 101-125)

#### 0x0014EB1C `cnv_i8_u16_scale_st` — logic
Conversion step (from 0x14E2F0): when disabled byte (+2) → status 2; when gate (+20) is 0 computes (i8 at +21) * (u16 at +0) * DAT_0014eb60 into +16 with status 0; gate set gives status 2 with hold flag +26=1. [I]
#### 0x0014EB6E `blk_inval_7pairs` — logic
Applies the helper PTR_FUN_0014ecc0 to the 7 complement pairs at +0x76/+0x78/+0x7a/+0x7c/+0x7e/+0x80/+0x82 — invalidates/ages the safety block fields; called by monitor 0x14A6E8. [I]
#### 0x0014EBA6 `blk_inval_2pairs` — logic
Same invalidate helper on the two pairs at +0x84/+0x85; called from 0x140FCC. [I]
#### 0x0014EBC2 `blk_inval_4pairs` — logic
Same pattern via PTR_FUN_0014ecc4 on the four pairs at +0x86/+0x88/+0x8a/+0x8c; called from 0x13583C. [I]
#### 0x0014EBF0 `blk_valid_all` — logic
Aggregate validity check: returns 1 only when the full complement sweep 0x14F01C passes AND the ring checksum 0x14EF84 passes. Called from 0x139B9C/0x132168. [O]
#### 0x0014EC10 `blk_process_if_pending` — logic
If the global block at PTR_DAT_0014ecc8 is not in the (0, -1) empty state, calls 0x14EB64 on it and returns 1 — flush/process of a pending safety block. Called from 0x139C40/0x132176. [I]
#### 0x0014EC52 `ring_blk_size` — library
Returns ((base+0xa4)&0xffff − base&0xffff) + 4 — byte size of the ring-buffer safety block (fields end at +0xa4). [O]
#### 0x0014ECCC `blk_init_zero_compl` — logic
Zero-initializes the whole complement-pair block (+0x10..+0x8c: byte pairs as 0/0xff, words as 0/0xffffffff, u16 pairs as 0/0xffff), validates the +0x62 pair with 0x14F300, and marks the block dirty via *PTR_DAT_0014efbc = 1/[1] = 0xfe. Called from 0x14EC64/0x14EC7A/0x14EC9C. [O]
#### 0x0014EEA2 `ring8_push_pair` — logic
Pushes a (b1,b2) entry into the 8-slot ring at +0x8e: dedups against the previous entry, keeps count +0x9e and the running sum +0xa0 (adds new pair bytes, subtracts replaced), writes through the write-gate PTR_DAT_0014efc0[0x26], and marks the block dirty (1/0xfe). Called from 0x143686 — this is the event/freeze-frame journal of spec 13. [I] spec 13.
#### 0x0014EF84 `ring8_sum_ok` — logic
Verifies the ring checksum: re-sums the 8 byte pairs and compares against the stored sum +0xa0. [O]
#### 0x0014EFC4 `ring8_clear` — logic
Clears the 8 ring slots (through the write-gate), count and sum, then marks the block dirty (1/0xfe). [O]
#### 0x0014F01C `blk_compl_check_all` — logic
292-instr complement sweep: verifies ~40 x/~x pairs across +0x10..+0x8c of the safety block (including u16/u32 pairs) plus the 0/1-pair validator 0x14F300 on +0x62; returns 0 on any mismatch. [O]
#### 0x0014F300 `compl_bool_ok` — library
Validates a boolean complement pair (b,~b): returns 0xff when b>1, partner != ~b, or partner < 0xfe; 0 when valid. [O]
#### 0x0014F32C `reg_two_handlers` — stub
Two indirect registration calls (PTR_FUN_0014f340/348 with PTR_DAT blocks); direct tail into 0x148DBA/0x151C9C; no resolved callers (pointer-invoked). [I]
#### 0x0014F34C `mon_sched_master` — logic
Giant monitor-scheduler (297 instr) from 0x131FD8: runs ~90 step functions in a fixed order through PTR_FUN/PTR_DAT tables (including this unit's 0x14B52E, 0x14B5D2, 0x14BB50, 0x14BC0C, 0x14BF86, 0x14C818, 0x14D79E, 0x14E288 and many r10/r13 steps), then executes a conditional chain with one-shot flags. This is the periodic safety-monitor pass of spec 13. [O] spec 13.
#### 0x0014F784 `sched4_mon4` — dispatch
Four-step indirect scheduler from 0x13205A: resolves to 0x14DCB2 (module-4 sequence), 0x14DD24, 0x14171A and 0x13D7BC. [O]
#### 0x0014F7A0 `call_step_13626a` — stub
Single indirect step call (resolves to 0x13626A) from scheduler 0x131FEE. [O]
#### 0x0014F7AA `sched3_steps` — dispatch
Three-step indirect scheduler from 0x13200E: resolves to 0x13BC32, 0x140EAC and (thunk) 0x141126. [O]
#### 0x0014F7C2 `call_step_13498e` — stub
Single indirect step call (resolves to 0x13498E) from 0x132024. [O]
#### 0x0014F7CC `sched4_steps` — dispatch
Four-step indirect scheduler from 0x132034: resolves to 0x13C06A, 0x13C1AA, 0x142544, 0x13BBCC. [O]
#### 0x0014F7EA `sched15_monitors` — dispatch
Fifteen-step indirect scheduler from 0x13204A (shown earlier): runs the monitor block 0x14A6E8 plus r10/r13/r14 steps (incl. 0x14B462, 0x14B4B8, 0x14B61A, 0x14B674, 0x14E03E) via PTR_FUN/PTR_DAT tables. [O] spec 13.
#### 0x0014F848 `call_step_140a0a` — stub
Single indirect step call (resolves to 0x140A0A) from 0x13209A. [O]
#### 0x0014F850 `call2_copy_blk` — stub
Two indirect step calls around a block copy (PTR_DAT_0014fa04[0x2f] = [0x28]); resolves to 0x1438AC family; from 0x131FD8. [I]
#### 0x0014FA0C `snap_u16_gather13` — logic
Gathers 13 u16 sensor values through getter pointers (PTR_PTR_0014fbf4..0014fc18 → the r08 getter bank) into the float fields +0x2c..+0x50 of the monitor block. Called from 0x14FC5C. [O]
#### 0x0014FAB4 `snap_compl_floats` — logic
Snapshot with complements: builds float pairs param[1]/[2] (~u16, u16) and param[0]/[8]/[0xb]/[0xc] from byte getters (PTR_PTR_0014fc1c..2c). Called from 0x14FC5C. [O]

Batch 6 (clusters 126-150)

#### 0x0014FB24 `snap_byte_inv_f` — logic
Single-value snapshot from a getter pointer (PTR_PTR_0014fc30 → r08 getter 0x07C422): stores (~byte) as float at +8 of the block. Part of the 0x14FC5C binding master. [O]
#### 0x0014FB3E `snap_2bytes_f` — logic
Two getter calls (PTR_PTR_0014fc34/38 → 0x07C418/0x07C422) stored as floats at +0x1c/+0x20. [O]
#### 0x0014FB66 `snap_6bytes_f` — logic
Six getter calls (PTR_PTR_0014fc3c..4c → 0x07BF4x family) stored as floats at +0x24..+0x38; called from the gather step 0x1517E0. [O]
#### 0x0014FBCE `snap_2bytes_f2` — logic
Two getter calls (PTR_PTR_0014fc54/58) stored as floats param[1]/param[0]. [O]
#### 0x0014FC5C `signal_bind_master` — logic
1811-instr table-binding master from scheduler 0x131FD8: wires hundreds of getter pointers (PTR_PTR_0014febc..001505xx, resolving to the r04/r08 getter banks 0x076xxx/0x077xxx/0x07Axxx/0x0A46xx) into the signal/data blocks, marking each block's refresh flags (param[0x12], [0x1c]..[0x21] = 1); also invokes the snapshot helpers 0x14FA0C/0x14FAB4/0x14FB24/0x14FB3E/0x14FBCE. This is the input-acquisition wiring of the r14 monitor/signal layer. [I]
#### 0x001513F8 `gather_bytes_53` — logic
Scheduler step (0x131FEE): calls 53 getter pointers in sequence (PTR_PTR_0015165c..1734 → r04/r08 getters) storing bytes into PTR_DAT_00151660[0..0x34], plus u16 at +0xac and indexed queries (getter(1), getter(2)) into +0xb8/+0xae/+0xbd of a second block. [O]
#### 0x00151754 `gather4_glob` — logic
Four getter calls (PTR_PTR_001517ac..c0) stored into globals PTR_DAT_001517b4..c8; scheduler step 0x13200E. [O]
#### 0x00151784 `gather_blk4` — logic
Scheduler step 0x132034: four getter calls into block PTR_DAT_001517d0[4]/[5]/[6..7 u16]/[8]. [O]
#### 0x001517E0 `gather_mon_inputs` — logic
Big scheduler step (0x13204A, 365 instr): ~60 getter calls into block PTR_DAT_00151a50 (+0x04..+0x47 byte/u16 mix) and satellite blocks (+0xb8..+0xbd, +0x2c..+0x2f, float at +8), then a second block +4..+9 and u32 at PTR_DAT_00151d28; includes the 0x14FB66 snapshot. Input refresh of the monitor chain. [O]
#### 0x00151C3E `gather_blk10` — logic
Scheduler step 0x13205A: one step call plus 8 getter calls into blocks at PTR_DAT_00151d68/d80 (+0..+3, +0xa/+0xc/+0xe). [O]
#### 0x00151C8C `gather1_glob` — stub
Single getter call stored to PTR_DAT_00151d8c; scheduler step 0x13209A. [O]
#### 0x00151C9C `gather_blk2c_refr` — logic
Refreshes block +0x2c..+0x2f and the float at +8 through the 0x151cfc getter chain, wrapped by PTR_FUN_00151d90/94 (resolves to 0x1401C2/0x140416); called from 0x14F32C and 0x15A67C. [O]
#### 0x00151D98 `tbl_map2d_get` — library
2D byte-map accessor: returns *(base + row*stride + col + 8) where stride = *(int*)base — byte element of a map whose header is 8 bytes. Member 0x1552BC is the identical copy with a single caller (0x152F7A); cluster of 2. [O] spec 09 (map/curve table family).
#### 0x00151DD8 `tbl_curve_i8` — library
Byte-curve interpolation (spec-09 "curve" layout: count, x[n] bytes, y[n] bytes): clamps below/above the axis, otherwise linearly interpolates between bracketing points and returns the byte result. Called from 5 monitor-layer functions. [O] spec 09.
#### 0x00151E70 `tbl_curve_i16` — library
u16-curve interpolation, same layout with s16 axis/values (clamped ends, linear between points); the generic curve evaluator used by 0x135A88/0x135AD0/0x1411AC. [O] spec 09.
#### 0x00151F10 `ctr7_get_0` — stub
Wrapper: counter-family accessor 0x151FB8(PTR_DAT_00152018, 0) — returns (after tick) counter 0 of the 7-counter array. Siblings 0x151F1E..0x151F64 select counters 1..6. Polled by the r14 giant 0x144108, r04_boot 0x06Exxx and r08 0x0AD430. [I]
#### 0x00151F1E `ctr7_get_1` — stub
Counter 1 accessor (0x151FB8(PTR_DAT_00152018, 1)). [I]
#### 0x00151F2C `ctr7_get_2` — stub
Counter 2 accessor. [I]
#### 0x00151F3A `ctr7_get_3` — stub
Counter 3 accessor. [I]
#### 0x00151F48 `ctr7_get_4` — stub
Counter 4 accessor. [I]
#### 0x00151F56 `ctr7_get_5` — stub
Counter 5 accessor. [I]
#### 0x00151F64 `ctr7_get_6` — stub
Counter 6 accessor. [I]
#### 0x00151FB8 `ctr7_get_adv` — logic
Ticks the selected counter (0x151FD6 — increments 0..15 cyclically unless the mode query 0x151F72 disables it), publishes it via PTR_FUN_00152028 and returns the new value. [O]
#### 0x00151FD6 `ctr7_tick` — logic
Counter tick: reads the counter byte, queries the mode gate 0x151F72; when the gate returns 0 advances the byte modulo 16 (0..15 wrap); result published via PTR_FUN_00152028. [O]
#### 0x0015202C `meas_get_u8_2064` — data-accessor
Returns *DAT_00152064; polled by 0x144108 and r04_boot 0x077C54. [I]

Batch 7 (clusters 151-175)

#### 0x00152034 `set_val_0f` — stub
Setter stub: writes 0x0f to param+1; called from 0x153910 (measurement-protocol area). [O]
#### 0x0015203A `map_lvl_4step` — logic
Maps an input level (0/1/2/>2, overridden to 14 when *PTR_DAT_00152068 == 2) to 0/7/10/14 — a 4-step level decoder; called from 0x15387E. [O]
#### 0x0015206C `meas_get_u8_22fc` — data-accessor
Start of another u8 getter wall (DAT_1522fc, 152300, 152304, 152308, 15230c, 152310, 152314, 152318, 15231c for 0x15206C..0x1520AC), polled by the r14 giant 0x144108 and r04_boot consumers (0x077xxx/0x06Exxx/0x0784C). [I]
#### 0x00152074 `meas_get_u8_2300` — data-accessor
Returns *DAT_00152300. [I]
#### 0x0015207C `meas_get_u8_2304` — data-accessor
Returns *DAT_00152304; polled by r04_boot 0x06EB4E/0x06EB80. [I]
#### 0x00152084 `meas_get_u8_2308` — data-accessor
Returns *DAT_00152308. [I]
#### 0x0015208C `meas_get_u8_230c` — data-accessor
Returns *DAT_0015230c. [I]
#### 0x00152094 `meas_get_u8_2310` — data-accessor
Returns *DAT_00152310. [I]
#### 0x0015209C `meas_get_u8_2314` — data-accessor
Returns *DAT_00152314. [I]
#### 0x001520A4 `meas_get_u8_2318` — data-accessor
Returns *DAT_00152318. [I]
#### 0x001520AC `meas_get_u8_231c` — data-accessor
Returns *DAT_0015231c. [I]
#### 0x001520BC `get_u16_inv_b4` — stub
Complement getter: returns ~0x1520B4() — the inverted half of a u16 complement pair; member 0x1520CA wraps 0x1520B8 the same way. Cluster of 2; both called from r08 0x07B6D8. [O]
#### 0x001520D8 `get_u8_pp_2320` — stub
Indirect getter wall: (**PTR_PP_00152320)() → resolves to r08 sensor getter 0x09B62A. Siblings 0x1520E6 (2324→0x09B5FA), 0x1520F4 (2328→0x09B602), 0x152102 (232c→0x09B5DA), 0x152110/0x15211E (2330→0x09B60A, duplicate copies), 0x15212C (2334→0x09B5F2), 0x15213A (2338→0x09B634), 0x152148 (233c→0x09B5E2), 0x152156 (2340→0x09B5EA). [O]
#### 0x001520E6 `get_u8_pp_2324` — stub
(**PTR_PP_00152324)() → 0x09B5FA. [O]
#### 0x001520F4 `get_u8_pp_2328` — stub
(**PTR_PP_00152328)() → 0x09B602. [O]
#### 0x00152102 `get_u8_pp_232c` — stub
(**PTR_PP_0015232c)() → 0x09B5DA. [O]
#### 0x00152110 `get_u8_pp_2330` — stub
(**PTR_PP_00152330)() → 0x09B60A; member 0x15211E is the duplicate copy. Cluster of 2. [O]
#### 0x0015212C `get_u8_pp_2334` — stub
(**PTR_PP_00152334)() → 0x09B5F2. [O]
#### 0x0015213A `get_u8_pp_2338` — stub
(**PTR_PP_00152338)() → 0x09B634. [O]
#### 0x00152148 `get_u8_pp_233c` — stub
(**PTR_PP_0015233c)() → 0x09B5E2. [O]
#### 0x00152156 `get_u8_pp_2340` — stub
(*DAT_00152340)() → 0x09B5EA. [O]
#### 0x00152164 `init_4x_ff` — stub
Init stub: sets +0xe/+0x10/+0x12/+0x19 to 0xff (complement defaults); called from 0x153910. [O]
#### 0x00152174 `call_rpt_1_0c` — stub
Report wrapper: (PTR_DAT_00152344, 1, 0xc); resolves to 0x154FF8. [I]
#### 0x00152180 `mon5_main` — logic
Monitor module 5 main (from 0x15387E): enter-critical (0xc), copies the source byte, maps state +0xc (0/1/2) to +0x17, boolean query via PTR_PP_00152354, validates and repairs the +0xd/+0xe complement pair (status +0x18/+0x19: 0/-1 ok, 1/-2 broken), derives +0x15/+0x16 from bit 1 of +0xb and +0x13/+0x14 from +8, runs the report step 0x152364 and releases fields; one-shot exit-critical when *param==1. [I] spec 13.
#### 0x00152364 `mon5_report` — logic
Report step of module 5: builds the enable flag (*PTR_DAT_001523f0==1), a u16 from PTR_DAT_001523ec and the error flag (from +0x18/+0x19), then — when enabled — calls the two report entries PTR_PTR_001523f8/001523fc (resolve to 0x0A105E/0x0A107C, the report-variant API of spec 13 §2). [O] spec 13.

Batch 8 (clusters 176-200)

#### 0x00152408 `meas_get_u8_245c` — data-accessor
Getter wall continues: *DAT_0015245c; polled by 0x144108 and r04_boot 0x07792C. [I]
#### 0x00152412 `call_rpt_1_0f` — stub
Report wrapper (PTR_DAT_00152460, 1, 0xf) resolving to 0x154FF8. [I]
#### 0x0015241E `cnv_lim_div4_m125` — logic
Conversion step from 0x15387E: enter-critical (0xf), computes via PTR_FUN_0015246c(u16, 500, 0x5ec) then stores result/4 − 0x7d (−125) at +4; one-shot exit-critical. [O]
#### 0x00152474 `meas_get_i16_26b4` — data-accessor
i16/u8 getter wall at 526b4..526dc (0x152474 i16, 0x15247A i16 526b8, 0x152480 i16 526bc, 0x152486 i16 526c0, 0x15248C u8 526c4, 0x152494 u8 526c8, 0x15249C u8 526cc, 0x1524A4 u8 526d0, 0x1524AC i16 526d4, 0x1524B2 i16 526d8, 0x1524B8 u8 526dc); polled by 0x144108 and r04_boot (0x06EAxx, 0x0779xx, 0x0AD072). [I]
#### 0x0015247A `meas_get_i16_26b8` — data-accessor
Returns i16 at PTR_DAT_001526b8. [I]
#### 0x00152480 `meas_get_i16_26bc` — data-accessor
Returns i16 at DAT_001526bc. [I]
#### 0x00152486 `meas_get_i16_26c0` — data-accessor
Returns i16 at DAT_001526c0. [I]
#### 0x0015248C `meas_get_u8_26c4` — data-accessor
Returns u8 at DAT_001526c4. [I]
#### 0x00152494 `meas_get_u8_26c8` — data-accessor
Returns u8 at PTR_DAT_001526c8. [I]
#### 0x0015249C `meas_get_u8_26cc` — data-accessor
Returns u8 at DAT_001526cc. [I]
#### 0x001524A4 `meas_get_u8_26d0` — data-accessor
Returns u8 at DAT_001526d0. [I]
#### 0x001524AC `meas_get_i16_26d4` — data-accessor
Returns i16 at DAT_001526d4. [I]
#### 0x001524B2 `meas_get_i16_26d8` — data-accessor
Returns i16 at DAT_001526d8. [I]
#### 0x001524B8 `meas_get_u8_26dc` — data-accessor
Returns u8 at DAT_001526dc. [I]
#### 0x001524C0 `chk_compl6_fatal` — logic
Complement-pair guard over six pairs (+0x20/+0x22, +0x24/+0x26, +0x28/+0x29, +0x2a/+0x2b, +0x36/+0x37, +0x38/+0x39); mismatch → fatal handler (0x07A914/0x0794C8) + infinite loop. Used by 0x15261C. [O] spec 13.
#### 0x0015253E `chk_compl7_fatal` — logic
Second guard of module 6: seven pairs (+8/+9, +0xe/+0xf, +0x10/+0x11, +0x12/+0x13, +0x14/+0x16, +0x18/+0x19, plus global PTR_DAT_001526ec[9]/[10]); same fatal-halt behavior. [O]
#### 0x001525C4 `mon6_blk_init` — stub
Init of the module-6 block (from 0x153910): 0xff/0xffff complement defaults and 0x1fff/0xe000 u16 complement pairs at +0x20..+0x30. [O]
#### 0x00152610 `call_rpt_1_11` — stub
Report wrapper (PTR_DAT_001526f0, 1, 0x11). [I]
#### 0x0015261C `mon6_main` — logic
Monitor module 6 main (367 instr, from 0x15387E): enter-critical (0x11), runs 0x1524C0 guard, snapshots; computes setpoints (i16−byte)*4+2000 clamped to 0x1ffe through PTR_FUN_00152708 with lookups 0x1550xx (PTR_FUN_0015270c/29ac); states +0x18 (1/2) select source values; validates with threshold compare via PTR_FUN_001529a4; writes all complement pairs; second guard 0x15253E; report step 0x1529CC; one-shot exit-critical. [I] spec 13.
#### 0x001529CC `mon6_report` — logic
Report step of module 6: failed/tested derived from the +0x36/+0x38 status flags masked against bits 0/4 of the cfg byte; reports through PTR_PTR_00152a84/88 (resolve to 0x0A105E/0x0A107C variant entries) when the enable flag *PTR_DAT_00152a74 is 1. [O] spec 13 §2.
#### 0x00152A94 `get_u8_pp_2aa8` — stub
Indirect getter (**PTR_PP_00152aa8)() → 0x09B744. [O]
#### 0x00152AAC `meas_get_u8_2ad8` — data-accessor
Returns *DAT_00152ad8. [I]
#### 0x00152AB6 `test_bitmask2` — logic
Bit test step from 0x15387E: +0xc = 1 when (**(uint**)cfg & param[1]) or (**(uint**)(cfg+4) & param[2]) — masks against two config words. [O]
#### 0x00152ADC `meas_get_u8_2c50` — data-accessor
Returns *DAT_00152c50. [I]
#### 0x00152AE4 `meas_get_u8_2c54` — data-accessor
Returns *DAT_00152c54. [I]

Batch 9 (clusters 201-220, incl. two missed entries 0x14E2C0/0x14E2F0 and 0x151F72)

#### 0x0014E2C0 `mon_4ch_blk_init` — stub
Init of the 4-channel speed module: seeds the reference u16 at +0xe via PTR_FUN_0014e538, clears +0x10, and re-inits the four channel sub-blocks at +0x38/+0x50/+0x68/+0x80 via 0x14EB1A. From 0x132980. [O]
#### 0x0014E2F0 `mon_4ch_avg_main` — logic
4-channel speed-value monitor (796 instr, from 0x133946 and scheduler 0x14F34C): each channel u16 == 0x3fff marks invalid; valid channels compute (ref(+0xe) * raw * 6) / 0x30d4 (12499) into +0x4a/+0x62/+0x7a/+0x92; per-channel status 0/1/2 with substitutes from PTR_DAT_0014e544/770 when flagged; mode +0x11 (1: (a+b)>>1, 2: (c+d)>>1, 0/3: (a+b+c+d)>>2 via PTR_FUN_0014e778) produces the averaged output +0x2c with status +0x30 (2 = no valid channel); finishes with the 0x14EB1C conversions per channel. [I] spec 13.
#### 0x00151F72 `ctr7_mode_query` — logic
Mode-condition query used by the 0x151FD6 tick: mode 2/6 selects the PTR_PTR_00152020 source, 0/3 the PTR_PTR_0015201c source, other modes disable; result byte obtained via PTR_PTR_00152024 and published through PTR_FUN_00152028. [O]
#### 0x00152AEC `meas_get_u8_2c58` — data-accessor
Final getter wall (DAT_152c58..152c88): u8 at 152c58; polled by 0x144108 and r04_boot 0x077C54. [I]
#### 0x00152AF4 `meas_get_u8_2c5c` — data-accessor
Returns u8 at DAT_00152c5c. [I]
#### 0x00152AFC `meas_get_u8_2c60` — data-accessor
Returns u8 at DAT_00152c60. [I]
#### 0x00152B04 `meas_get_i16_2c64` — data-accessor
Returns i16 at DAT_00152c64. [I]
#### 0x00152B0A `meas_get_u8_2c68` — data-accessor
Returns u8 at DAT_00152c68. [I]
#### 0x00152B12 `meas_get_u8_2c6c` — data-accessor
Returns u8 at DAT_00152c6c. [I]
#### 0x00152B1A `meas_get_i16_2c70` — data-accessor
Returns i16 at DAT_00152c70. [I]
#### 0x00152B20 `meas_get_u8_2c74` — data-accessor
Returns u8 at DAT_00152c74. [I]
#### 0x00152B28 `meas_get_u8_2c78` — data-accessor
Returns u8 at DAT_00152c78. [I]
#### 0x00152B30 `meas_get_u8_2c7c` — data-accessor
Returns u8 at DAT_00152c7c. [I]
#### 0x00152B38 `meas_get_u8_2c80` — data-accessor
Returns u8 at DAT_00152c80. [I]
#### 0x00152B40 `meas_get_i16_2c84` — data-accessor
Returns i16 at DAT_00152c84. [I]
#### 0x00152B46 `meas_get_u8_2c88` — data-accessor
Returns u8 at DAT_00152c88. [I]
#### 0x00152B4E `mon7_blk_init` — stub
Init of monitor module 7 (from 0x153910): 0x1fff/0xe000 complement u16 pairs at +6/+0xa/+0xc/+0x14/+0x16 and 0xff defaults. [O]
#### 0x00152B72 `call_rpt_1_15` — stub
Report wrapper (PTR_DAT_00152c8c, 1, 0x15) resolving to 0x154FF8. [I]
#### 0x00152B7E `mon7_compl_refr` — logic
Module-7 main (98 instr, from 0x15387E): enter-critical (0x15), verifies the four complement pairs (+0xa/+0xc, +0x14/+0x16, +0xe/+0xf, +0x18/+0x19); when all pass, refreshes complements and releases fields with one-shot exit-critical; any mismatch → fatal handler + infinite loop. Pure integrity-refresh monitor. [O] spec 13.
#### 0x00152CB0 `meas_get_i16_2e28` — data-accessor
Returns i16 at DAT_00152e28; polled by 0x144108 and r04_boot 0x07792C. [I]

## Unit summary

220 clusters, 228 named functions (7 multi-member clusters: 0x14B404×4, 0x14B4FC×2, 0x14BBBC×2, 0x151D98×2, 0x152110×2, 0x1520BC×2, 0x152110 dup).

Kind counts (by function, not cluster):
- logic: 90 — the monitor modules (0x14A6E8-family debounce/vote steps; plausibility monitors 0x14BF86/0x14C818/0x14D79E; module-4/5/6/7 chains 0x14DCB2/0x152180/0x15261C/0x152B7E; 4-channel averaging monitor 0x14E2F0), the complement-pair safety blocks, conversions, ring journal, table interpolation, gather steps.
- data-accessor: 75 — the huge `meas_get_*` / `get_u8_pp_*` getter banks (PTR_DAT/DAT literal-pool slots 0x14bb8c–0x152e28) polled by r08/r0c/r04 and the 0x155xxx measurement block.
- stub: 55 — report wrappers (`call_rpt_1_XX`), block inits, single-call scheduler thunks, getter wrappers.
- dispatch: 4 — the indirect mini-schedulers 0x14F784/0x14F7AA/0x14F7CC/0x14F7EA.
- library: 4 — `tbl_map2d_get` (+copy), `tbl_curve_i8/i16`, `ring_blk_size`, `compl_bool_ok`.
- unknown: 0 — every function had observable structure; the weakest identifications are the `ctr7_*` counter family (0x151F10–0x151FD6: purpose of the 7 mod-16 counters is unclear), `mon4_*` mode codes 6/8 (0x14DCB2–0x14E170), and the module-3 compute formulas (0x14CE60/0x14D446) — all marked [I].

Architectural picture of this slice (0x14AE38–0x152CB0): it is the safety-monitoring /
signal-conditioning tail of r14 (spec 13 "safety-monitoring layer 0x135xxx–0x14Exxx" plus its
0x150xxx continuation). Recurring idioms: complementary x/~x pairs everywhere (counters,
statuses, u16 values, even a 1/0xfe dirty flag), fatal-handler + infinite-loop on any mismatch
(0x07A914/0x0794C8 → spec 13 §2 0x6865C family), fault reporting through 0xA0E70 /
0xA105E / 0xA107C interface slots, enter/exit-critical pairs keyed by constants 0x71/0x72/0x83/
0x0c/0x0f/0x11/0x15, and scheduler steps wired through PTR_FUN/PTR_DAT tables.

Cross-region leads for other units:
- r10_app units: the 0x131FD8–0x13209A scheduler chain drives this unit's steps (0x14F34C master scheduler, 0x14F7xx wrappers, gathers 0x1513F8–0x151C9C); 0x133946/0x133C4E/0x133476/0x133740 also call in.
- r08_app units: this unit's getters resolve to the r08 sensor-getter bank (0x09B5xx–0x09B7xx, 0x077xxx, 0x07Cxxx, 0x0A46xx) and are polled by gatherers 0x000B912C/0x000A7C9C — those units should cross-check slot meanings.
- r04_boot units: heavy polling of this unit's getter wall (0x06Exxx–0x077Dxx callers).
- Measurement-protocol area (0x153xxx–0x155xxx, spec 10): calls this unit's getters (0x1534FC, 0x155AE4) and drives monitor modules 5–7 (0x15387E/0x153910/0x153996) — that unit owns the entry points; this unit owns the bodies.
- 0x144108 (5322-instr r14 giant, earlier r14 unit) is the single largest consumer of the getter wall — cross-check its region assignments against the `meas_get_*_<slot>` names here.
- `caseD_*` pre-named functions: none appear in this unit.
