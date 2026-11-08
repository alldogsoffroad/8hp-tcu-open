# Unit 42 (r10_app)

#### 0x00134B22 `diag_pid_read_dispatch` — dispatch
Dispatches on the selector in r0 (values 0x0A,0x0B,0x0D–0x20,0x24,0x42,0x44–0x4B,0x51,100/0x64,0x65–0x6F — a range that matches OBD Mode-$01 PID numbering) to one of ~44 per-PID encode handlers (0x00134D8E…0x00135642), defaulting to 0x00135642. No direct callers (callers=-): reached via a function-pointer table from the diagnostic response engine around 0x00134AD8. Member count 1. [I]
Cross-ref: candidate OBD PID service — spec 12/13 diag docs should cross-check.

#### 0x00134C80 `diag_pid_rd_out8e` — logic
Per-selector read handler shared by dispatcher 0x00134AD8: calls getter via PTR_PTR_00134d14 (resolved 0x0007BF40, r04_boot), stores raw byte in the request frame at +0x0c and the encoded response byte at +0x8e, sets *DAT_00135738 (response-data pointer) and length 1. Cluster of 1. [O]

#### 0x00134C94 `diag_pid_rd_out8f` — logic
Read handler: u16 getter via PTR_PTR_00134d18 (0x0007C410), raw at +0x0e, encoded as raw/0x50 saturated to 0xff at 0x4FB0 (i.e. 0..20336 -> 0..255), out byte +0x8f, len 1. Called from 0x00134AD8. [O]

#### 0x00134CBA `diag_pid_rd_out90` — logic
Read handler: u32 getter via PTR_PTR_00134d1c (resolved 0x00138C36, in-unit), raw at +0x10 copied to +0x90, responds 3 bytes (returns +0x91, *unaff_r13=3) via *DAT_00135738. Called from 0x00134AD8. [O]

#### 0x00134CD8 `diag_pid_rd_out94` — logic
Read handler: u16 getter via PTR_PTR_00134d24 (0x0007C408), raw +0x14, /0x50 saturate encoding, out +0x94, len 1. From 0x00134AD8. [O]

#### 0x00134CFE `diag_pid_rd_out95` — logic
Read handler: byte getter via PTR_PTR_00134d28 (0x0007BF50), raw +0x16, pass-through out +0x95, len 1. From 0x00134AD8. [O]

#### 0x00134D2C `diag_pid_rd_out96` — logic
Read handler: u16 getter via PTR_PTR_00134f58 (0x0007C400), raw +0x18, /0x50 saturate, out +0x96, len 1. From 0x00134AD8. [O]

#### 0x00134D52 `diag_pid_rd_out97` — logic
Read handler: byte getter via PTR_PTR_00134f5c (resolved 0x0013C99C), raw +0x1a, pass-through out +0x97, len 1. From 0x00134AD8. [O]

#### 0x00134D68 `diag_pid_rd_out98` — logic
Read handler: u16 getter via PTR_PTR_00134f60 (0x0013232E), raw +0x1c, raw>>5 saturated at 0x1FE1 (0..8160 -> 0..255), out +0x98, len 1. From 0x00134AD8. [O]

#### 0x00134D8E `diag_pid_rd_out99` — logic
PID-0x0A handler (dispatch 0x00134B22 case 10): u16 getter via PTR_PTR_00134f64 (0x0014C53C), raw>>5 saturate 0x1FE0 -> byte at +0x99, response ptr *DAT_00135738, len 1. [O] (PID id [I])

#### 0x00134DB4 `diag_pid_rd_out9a` — logic
PID-0x0B handler (case 0xb): u16 getter via PTR_PTR_00134f68 (0x0013BF24), raw>>5 saturate, out byte +0x9a, len 1. [O]

#### 0x00134DE0 `diag_pid_rd_out9b` — logic
PID-0x0D handler (case 0xd): signed s16 getter via PTR_PTR_00134f6c (0x0013C384); if -100<=v<0x399 encodes (v+100)>>2 else 0xff — a temperature-style offset/scale encoding; out +0x9b, len 1. [O]

#### 0x00134E1E `diag_pid_rd_out9d` — logic
PID-0x0E handler: byte getter via PTR_PTR_00134f70 (0x000A3776), writes both +0x9c and +0x9d, responds with +0x9d, len 1. [O]

#### 0x00134E34 `diag_pid_rd_out9f` — logic
PID-0x0F handler: u32 getter via PTR_PTR_00134f74 (0x000A1026), raw word at +0x24, responds with byte 0 of it at +0x9f, len 1. [O]

#### 0x00134E48 `diag_pid_rd_outa0` — logic
PID-0x10 handler: same u32 getter (0x000A1026) via PTR_PTR_00134f78, raw at +0x28, responds byte 1 at +0xa0, len 1. Part of a 4-byte quantity split across PIDs 0x0F–0x12. [O]

#### 0x00134E64 `diag_pid_rd_outa1` — logic
PID-0x11 handler: same getter, raw at +0x2c, responds byte 2 at +0xa1, len 1. [O]

#### 0x00134E80 `diag_pid_rd_outa2` — logic
PID-0x12 handler: same getter, raw at +0x30, responds byte 3 at +0xa2, len 1. [O]

#### 0x00134E9E `diag_pid_rd_outa3` — logic
PID-0x13 handler: u32 getter via PTR_PTR_00134f84 (0x000A102C), raw +0x34, responds byte 0 at +0xa3, len 1. First of another 4-byte split (0x13–0x16). [O]

#### 0x00134EB2 `diag_pid_rd_outa4` — logic
PID-0x14 handler: same getter via PTR_PTR_00134f88, raw +0x38, responds byte 1 at +0xa4, len 1. [O]

#### 0x00134ECE `diag_pid_rd_outa5` — logic
PID-0x15 handler: same getter via PTR_PTR_00134f8c, raw +0x3c, responds byte 2 at +0xa5, len 1. [O]

#### 0x00134EEA `diag_pid_rd_outa6` — logic
PID-0x16 handler: same getter via PTR_PTR_00134f90, raw +0x40, responds byte 3 at +0xa6, len 1. [O]

#### 0x00134F0C `diag_pid_rd_outa8` — logic
PID-0x17 handler: getter called with literal arg 6 via PTR_PTR_00134f94 (0x0013219C — an indexed getter), result copied to +0xa7/+0xa8, responds +0xa8, len 1. [O]

#### 0x00134F24 `diag_pid_rd_outa9` — logic
PID-0x18 handler: byte getter via PTR_PTR_00134f98 (0x00140AE8, r14_app), raw +0x44, pass-through out +0xa9, len 1. [O]

#### 0x00134F3A `diag_pid_rd_out48` — logic
PID-0x19 handler: u32 getter via PTR_PTR_00134f9c (0x0007A90C, r04_boot), raw at +0x48, responds all 4 bytes (len 4) via *PTR_DAT_00134fa0. [O]

#### 0x00134F50 `diag_pid_rd_out9e` — stub
Three-instruction PID-0x1A handler: no getter; publishes the pre-existing byte at (r1+0x9e) as the 1-byte response through *DAT_00135738. [O]

#### 0x00134FA4 `diag_pid_rd_outaa` — logic
PID-0x1B handler (dispatch case 0x1b): queries four boolean getters via PTR_PTR_001351d8/e0/e4 table (resolved 0x0007B002/0x0007AFEE/0x0007AFE4/0x0007AFF8, r04_boot) and OR-packs them into bits 0x80/0x40/0x20/0x10 of the response byte at +0xaa, len 1. [O]

#### 0x00135010 `diag_pid_rd_outab` — logic
PID-0x1C handler: reads eight s16 getters (PTR_PTR_001351e8..0x00135204 -> 0x000A46B2/0x000A46A0/0x001520B4/0x000A469A/0x001520B8/0x000A46AC/0x00152F68/0x000A46A6), raws at +0x56..+0x64, OR-packs nonzero flags as bits 0x80..1 into +0xab, len 1. Eight-flag status byte. [O]

#### 0x001350DC `diag_pid_rd_outac` — logic
PID-0x1D handler: seven flags — getters via PTR_PTR_00135208..0x00135220 (0x00141AE4/0x0014BBA8/0x0014C542/0x0007BF68/0x00140060/0x000A37B4) plus direct byte *PTR_DAT_0013520c — OR-packed 0x40..1 into +0xac, len 1. [O]

#### 0x00135184 `diag_pid_rd_outad` — logic
PID-0x1E handler: three getters via PTR_PTR_00135224/28/2c (0x0013C390/0x0013C9A4/0x0013BF2A) OR-packed 0x80/0x40/0x20 into +0xad, len 1. [O]

#### 0x00135230 `diag_pid_rd_outaf` — logic
PID-0x1F handler: getters via PTR_PTR_0013547c/80/88 (0x00136048/0x00135D64/0x0013CC44, all in-unit) packed 8/4/0x40/0x40, plus direct struct flags PTR_DAT_00135484[0x25]/[0x24] as bits 2/1, into +0xaf, len 1. [O]

#### 0x0013529E `diag_pid_rd_outb0` — logic
PID-0x20 handler: byte getter via PTR_PTR_0013548c (0x0007BF60, r04_boot), raw +0x17, pass-through response +0xb0, len 1. [O]

#### 0x001352B4 `diag_pid_rd_outb3` — logic
PID-0x24 handler: u16 getter via PTR_PTR_00135490 (0x000A38A6), raw>>1 saturated at 0x200, stores u16 at +0xb2 and answers with the high byte at +0xb3, len 1. [O]

#### 0x001352E4 `diag_pid_rd_outb4` — logic
PID-0x42 handler: u16 getter via PTR_PTR_00135494 (0x000A37C8), raw +0x74, unsealed 2-byte response at +0xb4 via *PTR_DAT_001354ac. [O]

#### 0x001352FA `diag_pid_rd_outb9` — logic
PID-0x44 handler: flag via indexed getter PTR_PTR_00135498 (0x0013237E) as bit 0, direct byte *PTR_DAT_0013549c copied to +0xb6 as bit 1, getter via PTR_PTR_001354a0 as bit 2; response byte +0xb9, len 1. [O]

#### 0x0013534C `diag_pid_rd_outba` — logic
PID-0x45 handler: byte getter via PTR_PTR_001354a4 (0x000769E2, r04_boot), raw +0x77, pass-through response +0xba, len 1. [O]

#### 0x00135362 `diag_pid_rd_outbc` — logic
PID-0x46 handler: u16 getter via PTR_PTR_001354a8 (0x00076742), raw +0x78, unscaled 2-byte response +0xbc via *PTR_DAT_001354ac. [O]

#### 0x00135380 `diag_pid_rd_outbe` — logic
PID-0x47 handler: byte getter via PTR_PTR_001354b0 (0x00076A10), raw +0x7a, pass-through response +0xbe, len 1. [O]

#### 0x00135396 `diag_pid_rd_outc4` — logic
PID-0x4A handler: reads the u32 at *PTR_DAT_001354b4 directly (no getter call), stores it at frame+0xc0, encodes raw/0xC80 saturated at 0xC7380 into +0xc4, len 1 — a large counter (0..815488 -> 0..255; /3200 looks like seconds->minutes style). [O]

#### 0x001353BC `diag_pid_rd_outc5` — logic
PID-0x4B handler: u16 getter via PTR_PTR_001354b8 (0x0007A7E0, r04_boot), raw +0x7c, /0x50 saturated at 0x4FB0 into +0xc5, len 1. [O]

#### 0x001353E8 `diag_pid_rd_outca` — logic
PID-0x51 handler: calls in-unit FUN_001356b4 (which fills frame+0xca..), then publishes a 2-byte response at +0xca via *PTR_DAT_001354ac. [O]

#### 0x00135400 `diag_pid_rd_s64` — logic
PID-0x64 (selector 100) handler: u16 getter via PTR_PTR_001354bc (0x000A4670), raw +0x80, raw/0x28 saturated at 0x27D9, response byte at frame+0. First of a 10-entry run (PIDs 0x64–0x6D, getters 0x000A4670..0x000A46A6, raws +0x80..+0x8a, responses at frame+0..+9). [O]

#### 0x00135428 `diag_pid_rd_s65` — logic
PID-0x65 handler: u16 getter 0x000A4676 via PTR_PTR_001354c0, raw +0x82, /0x28 saturate 0x27D9, response frame+1, len 1. Second of the 0x64-0x6D run. [O]

#### 0x00135452 `diag_pid_rd_s66` — logic
PID-0x66 handler: u16 getter 0x000A467C via PTR_PTR_001354c4, raw +0x84, /0x28 saturate, response frame+2. [O]

#### 0x001354C8 `diag_pid_rd_s67` — logic
PID-0x67 handler: u16 getter 0x000A4682 via PTR_PTR_00135714, raw +0x86, /0x28 saturate, response frame+3. [O]

#### 0x001354F2 `diag_pid_rd_s68` — logic
PID-0x68 handler: u16 getter 0x000A4688 via PTR_PTR_00135718, raw +0x88, /0x28 saturate, response frame+4. [O]

#### 0x0013551C `diag_pid_rd_s69` — logic
PID-0x69 handler: u16 getter 0x000A468E via PTR_PTR_0013571c, raw +0x8a, /0x28 saturate, response frame+5. [O]

#### 0x00135546 `diag_pid_rd_s6a` — logic
PID-0x6A handler: u16 getter 0x000A4694 via PTR_PTR_00135720, raw +0x8c, /0x28 saturate, response frame+6. [O]

#### 0x00135570 `diag_pid_rd_s6b` — logic
PID-0x6B handler: u16 getter 0x000A469A via PTR_PTR_00135724, raw +0x56, /0x28 saturate, response frame+7. [O]

#### 0x00135598 `diag_pid_rd_s6c` — logic
PID-0x6C handler: u16 getter 0x000A46A0 via PTR_PTR_00135728, raw +0x58, /0x28 saturate, response frame+8. [O]

#### 0x001355C0 `diag_pid_rd_s6d` — logic
PID-0x6D handler: u16 getter 0x000A46A6 via PTR_PTR_0013572c, raw +0x5a, /0x28 saturate, response frame+9. Last of the 0x64-0x6D run. [O]

#### 0x001355E8 `diag_pid_rd_s6e` — logic
PID-0x6E handler (dispatch case 0x6e): u16 getter 0x000A46AC via PTR_PTR_00135730, raw +0x5c, /0x28 saturated at 0x27D9, response at frame+10 (0xa). [O]

#### 0x00135610 `diag_pid_rd_s6f` — logic
PID-0x6F handler: u16 getter 0x000A46B2 via PTR_PTR_00135734, raw +0x5e, /0x28 saturate, response frame+0xb. Last of the 0x64-0x6F run of ten /40-scaled readings. [O]

#### 0x00135642 `diag_pid_rd_none` — logic
Default arm of dispatcher 0x00134B22 (unsupported selector): sets *DAT_00135738 = 0 and *unaff_r13 = 0 — returns an empty (zero-length) response. [O]

#### 0x00135650 `diag_frm_set_prev8` — stub
Three-instruction setter called by 0x00132980: stores literal 8 into the diag-frame byte at param+0x4d (the "previous state" slot used by 0x00135658's transition table). [I]

#### 0x00135658 `diag_state_code_lookup` — logic
Reads a state byte via PTR_PTR_0013573c (0x0009B588); if 5<=v<=8 writes frame+0x9e = PTR_LAB_00135740[(v-5)+(prev-5)*4] (4x4 transition-code table over states 5..8) and updates the prev slot at +0x4d. This maintains the +0x9e byte published as PID-0x1A by 0x00134F50. [I]

#### 0x0013568E `diag_mode_bits_encode` — logic
Reads a byte via PTR_PTR_00135744 (0x000A3798); stores (v & 3) at frame+0x4f and ORs (v<<6) into the PID-0x1A response byte at +0x9e. Called by 0x0013498E. [O]

#### 0x001356B4 `diag_pid51_pack` — logic
Helper of the PID-0x51 handler (0x001353E8): u16 via PTR_PTR_00135748 (0x00076714) low nibble + bits from struct PTR_DAT_0013574c ([0x2d]&3)<<4, ([0x2a]&1)<<6, ([0x21]&3)<<7 — packs a 2-byte status response into frame+0xca/+0xcb. [O]

#### 0x00135750 `inst_rd_byte0` — data-accessor
Returns the first byte of the object referenced indirectly via global PTR_DAT_001357b8 (a pointer-to-pointer to a singleton). Caller 0x00154500. Minimal getter; the owning module is outside this unit. [O]

#### 0x0013575C `inst_rd_byte4` — data-accessor
Same singleton: returns the byte at object+4. Caller 0x00155534. [O]

#### 0x00135768 `inst_rd_byte8` — data-accessor
Same singleton: returns the byte at object+8. Caller 0x0015456C. [O]

#### 0x00135774 `inst_call_cb10` — logic
Calls the function pointer PTR_FUN_001357bc with ( *(int*)(obj+0x10), param ) — passes one member of the singleton at +0x10 plus a caller argument to a callback. Caller 0x001553C6. [O]

#### 0x00135788 `tbl_get_special1` — data-accessor
Table accessor: selector 1 returns *PTR_DAT_001357c0, otherwise PTR_DAT_001357c4[param*4] — a pointer table with an aliased first entry. Caller 0x0015548E. [O]

#### 0x0013579C `tbl_idx_get_1357c8` — data-accessor
Plain indexed lookup: returns PTR_DAT_001357c8[param*4]. Caller 0x001554A2. [O]

#### 0x001357A6 `diag_frm_set_word4` — logic
Calls *_LAB_001357cc (resolved 0x000A10FA) and stores the u32 result at diag-frame +4. Called by 0x00132980 — part of that frame-builder family. [O]

#### 0x001357D0 `saf_chk_pair_halt` — logic
Verifies the complementary pair param[1] == ~param[2]; on violation invokes the fault pair (PTR_PTR_00135a2c -> 0x0007A914, PTR_PTR_00135a30 -> 0x000794C8) and then spins forever — deliberate halt as fault reaction. Called from 0x00135B34 and 0x0013583C. [O]

#### 0x001357F8 `saf_timeout_init` — logic
Initializes the monitor context: param[1] = byte_at(frame+8)*100 with complement at [2]; param[3]/[4] complementary copy of byte at frame+0x1c; +0x19 = ~state(+0x18); clears +0x16/+0x17; then verifies pairs via 0x00135BFE. Called by 0x00132980. [O]

#### 0x0013582E `os_release_res_d6` — stub
Single indirect call (*DAT_00135a3c)(PTR_DAT_00135a38, 1, 0xd6) — releases resource/service id 0xd6 (the same id acquired at the top of 0x0013583C and released at its end), OSEK-style resource protocol [I]. Caller 0x00148DBA. [O]

#### 0x0013583C `saf_monitor_task_body` — logic
Periodic monitor body under resource 0xd6: reads two masked input words, latches previous masks at +0x1c/+0x20 on first pass, debounces (countdown pairs at +4/+0xc), evaluates state==1 with *PTR_DAT_00135a4c, checks all-conditions via 0x00135A88 (32-entry scan) plus globals 0x00135a58/0x00135a5c/struct 0x00135a60, then reports through 0x001359B4; finally releases the resource and re-arms. 179 instrs. [I] — safety/alive-monitor flavored (complementary RAM everywhere).

#### 0x001359B4 `saf_fault_shutdown` — logic
On confirmed fault (and only when a threshold at *(frame+0x10) exceeds *PTR_DAT_00135a34): sets flag +0x16 (+complement), logs via PTR_PTR_00135a7c/0x00135a80, increments counter pair via 0x00135BEA, marks DAT[2]=1/[3]=0xfe, stores scan result 0x00135AD0 with complement, then calls the fault pair with +'d' offset and halts forever. Called from 0x0013583C. [I]

#### 0x00135A88 `saf_scan32_for_active` — logic
Scans 32 entries (bVar5 = 0..0x1f) through handler PTR_FUN_00135c60 over *(frame+0xc) (query then fetch), passing each u16 to predicate 0x000A0FA0; returns 1 on first hit, 0 otherwise. Cluster of 2 (member 0x00135AD0 differs only in the frame slot it scans, +0x18). [O]

#### 0x00135B18 `saf_state_set` — logic
Stores the requested state byte at ctx+0x18(+complement +0x19) and refreshes the complementary pair ctx[3]/[4] from byte *(frame+0x1c). Called from 0x00135BA8/0x00135B34. [O]

#### 0x00135B34 `saf_debounce_tick` — logic
Ticks the complementary countdown at ctx+0xc/+0x10 (via 0x001357D0 check); on reaching zero runs 0x00135AD0's 32-entry scan and increments one of two u16 counter pairs (at PTR_FUN_00135c68+0x86 or +0x8a, with complements) depending on the result, then clears the state via 0x00135B18(ctx,0). [I]

#### 0x00135BA8 `saf_ram_cell_test` — logic
If the handshake pair (+2==1, +3==0xfe) is set, clears it and sets state=1; otherwise clears state and calls 0x00135C2E. Then walks a RAM cell at +3 with 0xAA(-0x56)/'U'(0x55) patterns; on a stuck-bit mismatch invokes the fault pair (PTR_PTR_00148c68/6c) and halts. Caller 0x00140E52. [I] — classic complementary-RAM cell test.

#### 0x00135BEA `saf_cnt_pair_inc` — logic
Increments a byte and recomputes its complement (param[1] = ~new), keeping the safety pair consistent. Called from 0x001359B4. [O]

#### 0x00135BFE `saf_pairs_verify3` — logic
Verifies three consecutive complementary byte pairs; if any mismatch, calls 0x00135C2E (fault path). Called from 0x001357F8. [O]

#### 0x00135C2E `saf_ctx_reset` — logic
Resets the monitor context: three complementary byte pairs at [0..5] to (0,0xff) and re-arms the timeout pair (ctx+4/+8) with byte_at(frame+8)*100 / complement. Called from 0x00135BA8 and 0x00135BFE fault paths. [O]

#### 0x00135C74 `meas_get_135cc4` — data-accessor
Returns the s16 at *PTR_DAT_00135cc4. Caller 0x000F7298 — part of the measurement-getter pool consumed by the diag/PID layer. [O]

#### 0x00135C7A `meas_get_135cc8` — data-accessor
Returns the byte at *PTR_DAT_00135cc8. Caller 0x000F7298. [O]

#### 0x00135C82 `diag_frm_set_prev2` — stub
Stores literal 2 into diag-frame byte param+4. Caller 0x00132980 — sibling of 0x00135650 (same frame-builder family, different slot). [I]

#### 0x00135C88 `meas_scale_x10_cap` — logic
If the quality byte param[1]==0 and raw != 0xff: stores raw*10 as u16 at +2 (clamped to 0x7D9=2001) and clears quality +4; else marks quality 2 with value 0. Called by 0x0014F34C. Value*10 with cap 2001 [I]. [O]

#### 0x00135CCC `meas_get_135d24_b0` — logic
Wraps FUN_00135CE8(global 0x00135d24): returns the struct byte at offset 0. Caller 0x000F7298. Cluster of 2 — member 0x00135CDA fetches offset 3 of the same struct instead (via 0x00135CEE). [O]

#### 0x00135CEE `meas_get_field3` — stub
Two-instruction field getter: returns *(byte*)(param+3). Cluster of 3; members 0x0013BC8A and 0x001419D6 are byte-identical compiler duplicates used by other modules. [O]

#### 0x00135CF6 `meas_qual_gate` — logic
Calls predicate 0x0014BB34 via PTR_FUN_00135d2c on global 0x00135d28; if false outputs 0, if the disable flag param[1]!=0 outputs 0 with marker 2, else passes the raw byte param[0] through to param[2] (quality param[3]=0). Called by 0x00132D8E and 0x0014F34C. [I]

#### 0x00135D30 `meas_get_135d5c` — data-accessor
Returns the byte at *PTR_DAT_00135d5c. Callers 0x000F7298, 0x000B912C. [O]

#### 0x00135D38 `meas_get_135d60` — data-accessor
Returns the byte at *PTR_DAT_00135d60. Callers 0x000F7298, 0x000B912C. [O]

#### 0x00135D42 `meas_invalid_flag2` — logic
If the enable byte param[1]==0 copies raw param[0] to param[2] (quality 0), else stores 2 (invalid). Same value/quality convention as 0x00135C88/0x00135CF6. Caller 0x0014F34C. [O]

#### 0x00135D64 `meas_get_136000` — data-accessor
Returns byte at DAT_00136000. Directly consumed by the PID-0x1F handler (0x00135230) and 0x000F7298/0x000B912C/0x00155AE4/0x00133476/0x0014FC5C — a widely-shared status flag. [O]

#### 0x00135D6C `meas_get_136004` — data-accessor
Returns byte at *PTR_DAT_00136004. Same caller pool as 0x00135D64. [O]

#### 0x00135D74 `meas_get_136008` — data-accessor
Returns byte at *PTR_DAT_00136008. Callers 0x00133476/0x0014FC5C/0x00155AE4. [O]

#### 0x00135D7C `meas_get_13600c` — data-accessor
Returns byte at *PTR_DAT_0013600c. Callers 0x00133476/0x0014FC5C/0x00155AE4. [O]

#### 0x00135D84 `diag_frm_init_ff` — logic
Sets four diag-frame bytes (+5, +0xb, +0xe, +0xf) to 0xff — initializes "not available" markers in the snapshot frame. Caller 0x00132980. [I]

#### 0x00135D90 `os_release_res_0b` — stub
Single indirect call (*DAT_00136014)(PTR_DAT_00136010, 1, 0xb): releases resource id 0x0b, matching the wrapper 0x00135D9C. Caller 0x00148DBA. [I]

#### 0x00135D9C `saf_task_wrap_res0b` — logic
Acquires resource 0xb (PTR_FUN_00136018 on 0x00136010), runs monitor body 0x00135DC6, and conditionally releases the resource (flag *PTR_DAT_0013601c). Mirror of the resource-0xd6 pattern in 0x0013582E/0x0013583C. Callers 0x0014F34C, 0x00133C4E. [O]

#### 0x00135DC6 `saf_monitor_body_res0b` — logic
183-instr monitor core: copies raw inputs to working slots, verifies six complementary byte pairs (+4/+5, +6/+7, +8/+9, +0xa/+0xb) halting via the 0x0007A914/0x000794C8 fault pair on violation, runs a state machine over +0x13/+0x14 with sub-check 0x00135F54 (threshold compare), writes the value/quality pair +0xc/+0xd with complements, then passes ~19 addresses through scrub hook PTR_FUN_00136030. [I]

#### 0x00135F54 `saf_thresh_check` — logic
Reads global struct PTR_DAT_00136034 (bytes 0xe/0xf, u16s +0xa/+0xc), verifies their complementary pairs (halt on mismatch), and if enabled sets quality=2/+0x12=1/+0xd=0 with +0x11 = (raw < threshold) and returns 1; else returns 0. Called only from 0x00135DC6. [I]

#### 0x0013603C `meas_get_136208` — data-accessor
Returns the s16 at *PTR_DAT_00136208. Callers 0x000F7298, 0x000A7C9C, 0x00155AE4, 0x000B912C. [O]

#### 0x00136042 `meas_get_13620c` — data-accessor
Returns the s16 at *PTR_DAT_0013620c. Caller 0x00155AE4. [O]

#### 0x00136048 `meas_get_136210` — data-accessor
Returns the byte at DAT_00136210 — a getter directly consumed by the PID-0x1F handler (0x00135230) among others. [O]

#### 0x00136050 `meas_get_136214` — data-accessor
Returns the byte at *PTR_DAT_00136214. Caller 0x00155AE4. [O]

#### 0x00136058 `diag_frm_init_fffd` — logic
Initializes complementary u16 pairs in the diag frame (+0x12=0/+0x16=0xffff, +6=0xffff, +9=0xff, +0xc=0xffff, +0xf=0xff) and recomputes +0x14 = ~+0x10. Caller 0x00132980. [I]

#### 0x00136080 `os_release_res_0c` — stub
Single indirect call (*DAT_0013621c)(PTR_DAT_00136218, 1, 0xc): releases resource id 0x0c used by wrapper 0x00136142. Caller 0x00148DBA. [I]

#### 0x0013608C `saf_avg_body_res0c` — logic
Monitor body (under resource 0x0c): verifies four complementary pairs via 0x0013616C; when mode==0 and raw s16 != 0x7ff updates a u16 accumulator through PTR_FUN_00136220 (prev + inputs), divides by 5, stores value/complement at +0x28/+0xc with quality pair; error paths set quality 2/1. Scrub hooks PTR_FUN_00136224/0x00136228 then 0x001361C4 final check. Looks like a filtered/averaged measurement with safe storage [I]. [O]

#### 0x00136142 `saf_task_wrap_res0c` — logic
Acquires resource 0x0c (PTR_FUN_0013622c on 0x00136218), runs body 0x0013608C, conditionally releases (flag *PTR_DAT_00136230). Same OSEK pattern as 0x00135D9C/0x0013583C. Caller 0x0014F34C. [O]

#### 0x0013616C `saf_chk4_pairs_halt` — logic
Verifies four complementary pairs (+0xe/+0xf byte, +0xa/+0xc u16, +0x10/+0x14 byte, +0x12/+0x16 u16); on violation fault pair 0x0007A914/0x000794C8 + infinite halt. Called from 0x0013608C. [O]

#### 0x001361C4 `saf_status_mark_verify` — logic
ORs 0xf800 into the status u16 at +0xc then re-verifies the +8/+9 byte pair, halting on mismatch. Called from 0x0013608C. [I]

#### 0x00136244 `diag_frm_fill53` — logic
Fills the u16 array at frame+0x3e (53 entries) with per-index values from table function 0x00151DB6 called over *(frame+0x20) — these are the per-DTC debounce times consumed by 0x0013661C. Caller 0x00132980 (frame builder). [I]

#### 0x0013626A `warn_task_body` — logic
Top-level warning/lamp task body (caller 0x0014F7A0): latches enable flag +0xb1 from global 0x0013639c, runs enable-edge detect 0x001364C6, timer tick 0x00136506, DTC-range scans 0x001362B0 (ranges 0-0x32 and 0x33-0x34) into +0xb7/+0xbf, then condition eval 0x0013631A and the 53-entry DTC monitor 0x0013661C. [I]

#### 0x001362B0 `warn_dtc_range_active` — logic
Scans DTC indices param2..param3 via status handler PTR_FUN_001363a0 over *(frame+0x28): if an entry has bits 0x81 set but the frame byte (+4+i) lacks bit 0, returns 0 (unreported active); returns 1 if any qualifying active entry. [I]

#### 0x001362F4 `warn_report_evt` — logic
Helper for 0x0013631A: when flag==1 fetches a level byte via PTR_PTR_001363a4 and calls reporter PTR_PTR_001363a8(channel, level, flag, gate). [I]

#### 0x0013631A `warn_cond_eval` — logic
166-instr condition evaluation: compares live u16s (frame+8/+0x14/+0x18) against thresholds (+0x3a/+0x2a/+0xaa counters), gated by enable +0xb1/+0xb2 and bits of byte *(frame+0x2c); sets warn flags +0xba/+0xc2/+0xbb/+0xc3, computes masked bytes +0xb6/+0xbe, and reports through PTR_PTR_001365fc and 0x001362F4 twice. [I]

#### 0x001364C6 `warn_enable_latch` — logic
Edge/latch logic on global 2-byte state at 0x0013660c: rising enable sets [1]=1; produces +0xb2 ("was active") and stores the current enable back. Called from 0x0013626A. [O]

#### 0x00136506 `warn_timer_tick` — logic
Manages four debounce counters (+0x3a, +0xf, +0x2a, +0xaa as s16): resets them via 0x00136952 on out-of-range raw or global reset, conditionally zeroes each when its gate clears, increments non-0xffff counters, and latches the four gate bytes (+ global 0x00136618) into struct 0x00136614. [O]

#### 0x0013661C `dtc_monitor_eval53` — logic
331-instr DTC monitor core: iterates 53 entries using a 3-byte-per-entry table at PTR_DAT_00136874; reads per-entry status via PTR_FUN_00136878 over *(frame+0x28), gates by 0x00136966 group flags, matches condition bits (0x1/0x2/0x4/0x8 against frame bytes) to severity sources (*PTR_DAT_00136880 / *PTR_PTR_00136884), debounces via times fetched through PTR_FUN_0013687c over *(frame+0x20) and counters at +0x3e; then maps 12 output slots through reporters PTR_PTR_0013699c/0x001369a0 and latches aggregate +0xaf. [I]

#### 0x00136952 `warn_cnt_reset4` — stub
Zeroes the four u16 debounce counters (+0x3a, +0x3c, +0xa8, +0xaa). Called from 0x00136506 and 0x00140E52. [O]

#### 0x00136966 `dtc_group_gate` — logic
Group gate for 0x0013661C: entry class '&' checks struct 0x001369a4[0x8c]/[0x8b]; '.'/'/' check *PTR_DAT_001369a8; anything else passes (returns true). [I]

#### 0x001369AC `meas_get_1369f8` — logic
Wrapper getter: returns FUN_001369BA(global 0x001369f8). Callers 0x000A7C9C, 0x001534FC, 0x000B912C. [O]

#### 0x001369C6 `magic_c0de_baad_chk` — logic
Checks raw s16 == 0xC0DE (displayed -0x3F22) and (prior marker 0xBAAD (-0x4553) or latch already set) and latches the found flag at param+1; stores the raw back. Magic-marker detector (code/baad tags — calibration or development marker). Caller 0x0014F34C. [O]

#### 0x001369FC `saf_chk_u32pair_halt` — logic
Verifies the u32 complementary pair param[1] == ~param[2]; violation triggers the fault pair (PTR_PTR_00136bb8/bbc) and infinite halt. Called from 0x00136FC8/0x001370D6. [O]

#### 0x00136A24 `saf_ctx_init_f` — logic
Initializes the monitor context: 0xff/0xffff markers, the float-complement pair param_1[0x11] = -1.0 - param_1[0x10], and three u16 inputs copied into u32 pairs with complements ([1]/[2], [3]/[4], [5]/[6]). Caller 0x00132980. [O]

#### 0x00136A78 `os_release_res_fffa` — stub
Releases resource id 0xfffa via (*DAT_00136bc4)(PTR_DAT_00136bc0, 1, 0xfffa). Caller 0x00148DBA. [I]

#### 0x00136A86 `saf_mon5_task_body` — logic
549-instr task body under resource 0xfffa: verifies float-complement and global pair 0x00136bcc[0x2c/2d], copies ten raw u16s to float slots [0x14..0x1d], runs a status state machine with sub-checks 0x00136FC8/0x001370D6 (result enum 0/1.0/2.0 in [0x10]), refreshes pairs via 0x0013715E, verifies ~20 complementary pairs, calls checker PTR_FUN_00136da8(param, mask|0xfffe) and requires the answer 'Z' (0x5A) else halts, recomputes the float complement, then scrubs ~30 slots and releases the resource. [I]

#### 0x00136FC8 `saf_subchk_debounce` — logic
Sub-check for 0x00136A86 keyed on float status [0x15]: compares [0x14] against u16 thresholds (*(frame+0x14)/+0x18), reloads or debounces the u32 pairs [1]/[2] (or [3]/[4]) via 0x001369FC; returns 1.0 (active) or 0. [I]

#### 0x001370D6 `saf_subchk2_debounce` — logic
Second sub-check: predicate 0x001371CE==1 gates a debounce on pair [5]/[6]; threshold compare against struct 0x00137224 (+0xe u16, [0x12] byte); returns 1.0 when the armed condition holds. [I]

#### 0x0013715E `saf_pairs_refresh` — logic
Refreshes the complementary u32 pairs [1]/[2] and [3]/[4] from the u16 inputs at *(frame) and *(frame+4), unless status [0x15] equals the two gating float constants. [O]

#### 0x00137194 `meas_get_s16_pp` — data-accessor
Four-instruction double-indirect getter: returns **(short**)param_1. Callers 0x001373C8, 0x0013727C, 0x00137538. [O]

#### 0x0013719C `meas_get_s16_off4` — data-accessor
Four-instruction double-indirect s16 getter at (*(ctx))+4. Callers 0x0013727C, 0x00137538. [O]

#### 0x001371A4 `meas_get_s16_off8` — data-accessor
Same pattern, slot +8. Callers 0x001374C8, 0x0013727C. [O]

#### 0x001371AC `meas_get_s16_slot` — data-accessor
Base of a 6-member cluster of identical double-indirect s16 getters; only the slot constant differs: 0x001371AC (+0xc), 0x001371BE (+0x14), 0x001371C6 (+0x18), 0x00138238 (+0x4c), 0x00138242 (+0x48), 0x0013824C (+0x44). Callers 0x001374C8/0x001373C8/0x0014E2C0/0x00148432. [O]

#### 0x001371B4 `meas_get_u8_off10` — data-accessor
Double-indirect byte getter at slot +0x10. Caller 0x001372C4. [O]

#### 0x001371CE `saf_all_zero8_f` — logic
Returns 1.0 only when all eight floats at ctx+0x58..+0x74 are zero — the quiescent predicate used by sub-check 0x001370D6. [O]

#### 0x0013722C `meas_get_13747c` — logic
Wrapper getter: FUN_00137270(global 0x0013747c). Cluster of 2; member 0x0013723A calls 0x00137276 on the same global (different field). [O]

#### 0x00137248 `saf_chk_u32pair_halt2` — logic
Verifies the u32 complementary pair param[1] == ~param[2]; violation triggers fault pair (PTR_PTR_00137480/84/88) + infinite halt. Used by 0x001373C8/0x001374C8. [O]

#### 0x0013727C `saf_ctx_init3` — logic
Frame/context init (caller 0x00132980): reads three u16s via PTR_FUN_00137490/94/98 over global 0x0013748c into u32 complementary pairs [0..5], sets +0x2d=0xff. [O]

#### 0x001372B6 `os_release_res_fffb` — stub
Releases resource id 0xfffb via (*DAT_001374a0)(PTR_DAT_0013749c, 1, 0xfffb). Caller 0x00148DBA. [I]

#### 0x001372C4 `saf_mon_task_fffb` — logic
121-instr monitor under resource 0xfffb: master predicate PTR_FUN_001374a8 on 0x0013748c; when inactive derives status (0/1/2 via sub-checks 0x001373C8, 0x001374C8 and *PTR_DAT_001374ac==2), when active runs both sub-checks and encodes 1/2/3 flags at +0x2c/+0x2e/+0x2f; refreshes pairs via 0x00137538, recomputes complement +0x2d, scrubs 14 slots, releases the resource. [I]

#### 0x001373C8 `saf_win_debounce_a` — logic
Sub-check A: mode byte +0x1a (3 or 4). Mode 3: raw u16 +0x18 must sit between getters PTR_FUN_001374c0/c4 (window), else debounces u32 pair [0]/[1] via 0x00137248; mode 4 debounces pair [2]/[3]. Returns 1 while idle/timeout. [I]

#### 0x001374C8 `saf_win_debounce_b` — logic
Sub-check B: predicate 0x00137578 (eight s16s zero); debounces u32 pair (+0x40/+0x50) with threshold *PTR_DAT_001375c4, getter 0x001375c8 and flag *PTR_DAT_001375cc; returns 1 when armed inside the window. [I]

#### 0x00137538 `saf_pairs_refresh2` — logic
Reloads pairs [0]/[1] (and [2]/[3] unless mode==4) from getters PTR_FUN_001375d0/0x001375d4 unless mode==3 skips the first. [O]

#### 0x00137578 `saf_all_zero8_s16` — logic
Returns 1 only when the eight s16s at ctx+0x1c..+0x2a are all zero. [O]

#### 0x001375D8 `meas_get_137844` — logic
Base of a 4-member cluster of wrapper getters over global 0x00137844 (fields via 0x0013761E/0x00137624/0x0013762A/0x00137636); callers 0x000A7C9C / 0x00155AE4. [O]

#### 0x00137602 `meas_get_137844_u16` — logic
Same singleton, returns a u16 via 0x00137630. Callers 0x000F7298, 0x000A7C9C, 0x00155AE4. [O]

#### 0x0013763C `state_latch_ctx_consts` — logic
Seeds the latch-task context constants: +0x3a=3, +0x44=3, u16 timeouts 3000/3000/3000/2160 (0x870), +0x7e = *_LAB_00137848. Caller 0x00132980. [O]

#### 0x00137668 `state_latch_task` — logic
468-instr state-latch task (callers 0x00133740, 0x0014F7EA): for seven input groups either copies live values (via pointers at *(ctx+0x00..0x70)) or, under fault gates, falls back to default tables PTR_FUN_00137850/0x00137abc; runs group debouncers 0x00137B68 (8 bytes), 0x00137E1A (3 bytes), 0x00137F56 (1 byte), 0x00137FEC (4 u16), 0x00138196 (1 u16), 0x00138256 (4 bytes), 0x0013848E; finally classifies byte +0x4e (0x0C/0x0A/0x0E->1, 0x0D->2, 0x09->3, 0x05/06/07/0B->4) into +0x85 and computes +0x8e via 0x001383E0. The +0x4e codes look like a lever/gear class [I]. [O]

#### 0x00137B2E `state_latch_ctx_init` — logic
Initializes latch-task markers: seven "pending" flags (+0x50,+0x5f,+0x68,+0x7c,+0x84,+0x88,+0x97)=1 and seven progress/confirm flags (+0x52,+0x5c,+0x63,+0x72,+0x80,+0x86,+0x95)=0. Caller 0x00140E52. [I]

#### 0x00137B68 `deb_latch_8b` — logic
Group debouncer for 8 bytes (5-cycle period, 3 consecutive stable matches required): on stability, gated by freeze flag *PTR_DAT_00137e70, updates mirrors ctx+0x48..+0x4f and complementary copies in table 0x00137e74[0x2c..0x3c] (skipping sentinel values '?', -1, 0x1f, 0x0f), calling 0x0013852E after each change; clears pending when done. [O]

#### 0x00137E1A `deb_latch_3b` — logic
Same debounce-latch pattern for 3 bytes (20-cycle period, sentinel checks), updating mirror ctx+0x60..+0x62 and table 0x00138054[0x34/0x3e/0x40] with complements + 0x0013852E notification. [O]

#### 0x00137F56 `deb_latch_1b` — logic
Single-byte debounce-latch (20-cycle period): mirror ctx+0x5b, table 0x00138054[0x42/0x43]. [O]

#### 0x00137FEC `deb_latch_4w` — logic
Four-u16 debounce-latch (10-cycle period, sentinels 0x3fff and -1): mirrors ctx+0x6a/+0x6c/+0x6e/+0x70 and table 0x001382c0[0x44..0x52] with u16 complements + 0x0013852E. [O]

#### 0x00138196 `deb_latch_1w` — logic
Single-u16 debounce-latch (20-cycle): mirror ctx+0x7e, table 0x001382c0[0x54/0x56]. [O]

#### 0x00138256 `deb_latch_4b` — logic
Four-byte debounce-latch (20-cycle): mirrors ctx+0x8b..+0x94 and table 0x001383fc[0x58..0x60] with complements + 0x0013852E notification. [O]

#### 0x001383E0 `tbl_lookup_4a_const` — data-accessor
Pure lookup switch on the latched state byte at ctx+0x4a (cases 2..0x11) returning fixed u16 constants (0x348, 0x63, 0x7c, 0x33a, 0x188, 0x114, 0x2f4, 0x1ca, 0x24, 0x168, 0x2aa, 0x35e, 0x1e4, 0x4c, 0x9c, 0x332; default 0) — a per-state time/threshold table feeding +0x8e in the latch task. [O]

#### 0x0013848E `deb_latch_1b2` — logic
Single-byte debounce-latch (10-cycle period, 3 stable matches): updates mirror ctx+0x98 and table 0x0013853c[0x5a/0x5b] with complement, notifies 0x0013852E, gated by freeze flag *PTR_DAT_00138538. [O]

#### 0x00138544 `diag_frm_clr_word8` — stub
Stores 0 to the u16 at param+8. Caller 0x00132D8E (frame builder family). [O]

#### 0x0013854A `cond_mask_build` — logic
126-instr condition-mask builder (callers 0x001320FC, 0x001420C8): fetches u16/byte getters via PTR_PTR_0013865c/60/64 (0x000773E0, 0x000774F0, 0x00076DD6), then ORs per-condition constant masks from global structs (0x00138668/+0xa threshold, 0x00138670 windows, 0x00138680, 0x00138688) while their threshold/flag tests hold; final result = **(ushort**)(*(ctx+0x10)) & mask at ctx+8. [I]

#### 0x00138698 `meas_get_13889c` — data-accessor
s16 getter at DAT_0013889c. Callers 0x001534FC, 0x000B912C. [O]

#### 0x0013869E `meas_get_1388a0` — data-accessor
s16 getter at *PTR_DAT_001388a0. Caller 0x000F471C. [O]

#### 0x001386A4 `meas_get_1388a4` — data-accessor
s16 getter at *PTR_DAT_001388a4. Caller 0x000F471C. [O]

#### 0x001386AA `meas_get_1388a8` — data-accessor
Byte getter at *PTR_DAT_001388a8. Caller 0x001534FC. [O]

#### 0x001386B2 `meas_get_1388ac` — data-accessor
Byte getter at *PTR_DAT_001388ac. Caller 0x001534FC. [O]

#### 0x001386BA `saf_compl_refresh4` — logic
Recomputes four complements (+0x18 from +0x16, +0x1b from +0x1a, +0xc from +0xa, +0xf from +0xe). Caller 0x00132980. [O]

#### 0x001386D8 `os_release_res_14` — stub
Releases resource id 0x14 via (*DAT_001388b4)(PTR_DAT_001388b0, 1, 0x14). Caller 0x00148DBA. [I]

#### 0x001386E4 `saf_sense_eval_res14` — logic
199-instr sensor evaluator under resource 0x14: gear/state byte +0x12 (5..8 -> 0..3, else 4) selects a constant via PTR_FUN_001388c0 into +8; verifies three complementary pairs (halt on violation, incl. global 0x001388c4[0x50/0x51]); when quality +0xe==0 and raw u16 +0xa != 0x1fff computes (raw>>2)-500 into +0x18 else marks error 2/quality; latches value +0x16 with complement and scrubs 12 slots. 0x1fff is the SPI/ADC full-scale sentinel; (raw>>2)-500 spans -500..1547 — temperature-style scaling [I]. [O]

#### 0x001388E8 `meas_get_1389d8` — logic
Wrapper getter: FUN_001388F6(global 0x001389d8) — byte at +8 of that struct. Callers 0x000F7298, 0x000B912C. [O]

#### 0x001388F6 `meas_get_field8` — stub
Two-instruction field getter: *(byte*)(param+8). Cluster of 4; members 0x00141A16, 0x0014B44E, 0x0014B518 are byte-identical duplicates in the r14_app region. [O]

#### 0x001388FE `state_decode_2bit` — logic
104-instr state decoder with hysteresis: two gate branches (flag *(ctx+4)) combine three input bits (ctx+6 in 0/1/5/7, ctx+5, ctx+2 old state) into codes 0/1/0x10/0x11, latching the result back into ctx+2; returns 0 when disabled. Callers 0x00132D8E, 0x0014F34C. [I]

#### 0x001389DC `meas_get_138ae0` — data-accessor
s16 getter at *PTR_DAT_00138ae0. Caller 0x000B912C. [O]

#### 0x001389E4 `sens_eval_integ` — logic
110-instr sensor evaluator: state byte (5..8 -> 0..3 else 4) selects a constant via PTR_FUN_00138ae8 into +0x1f; when enabled and raw s16 +0x18 != 0x1fff encodes (raw-2000)/4 into +0x12 with quality; integrates via PTR_FUN_00138af0(value, acc, byte) into [6], divides by byte+1 into s16 [5], subtracts offset +0x14... plus alternate paths via PTR_FUN_00138afc or **(short**)(*(ctx+8)) into +0xe. Frequency/speed-style scaling ((raw-2000)/4) [I]. [O]

#### 0x00138B00 `meas_get_138c2c_s8` — logic
Wrapper getter returning the s16 at +8 of global 0x00138c2c (via 0x00138B70). Caller 0x000F7298. Cluster of 3 with 0x00138B1C, 0x00138B54 (same pattern, u16 fields). [O]

#### 0x00138B0E `meas_get_138c2c_b` — logic
Base of a 5-member cluster of wrapper getters over global 0x00138c2c, each returning one byte field (via 0x00138B74/0x00138B7E/0x00138B84/0x00138B8A/0x00138B94); all called from 0x000F7298. [O]

#### 0x00138B70 `meas_get_field_s8` — stub
Two-instruction s16 field getter at +8. Cluster of 2; member 0x00149754 is a byte-identical copy in r14_app (caller 0x0014971C). [O]

#### 0x00138B74 `meas_get_field_b10` — stub
Field getter: byte at +0xa. Cluster of 2; member 0x00149758 is the r14_app duplicate. [O]

#### 0x00138B9A `diag_frm_init4ff` — stub
Sets four frame bytes ([0],[2],[3],[4]) to 0xff — not-available markers. Caller 0x00132980. [I]

#### 0x00138BA6 `meas_enc4_chan` — logic
Four-channel raw->physical encoding: ch0 pass-through (cap 0xfb) to u16, ch1 *10, ch2 pass (cap 0xff), ch3 *10; each gated by its quality byte and marked 0/2, ch0 also gets an out-of-range boolean at +0xb. Callers 0x00132D8E, 0x0014F34C. [O]

#### 0x00138C30 `meas_get_138d48` — data-accessor
s16 getter at *PTR_DAT_00138d48. Caller 0x000B912C. [O]

#### 0x00138C36 `meas_get_138d4c_u32` — data-accessor
u32 getter at DAT_00138d4c — the data source for the index-3 PID handler (0x00134CBA); also called from 0x000AD606/0x000AD85C/0x000AD80C/0x000ADC1E (SPI/r08 units). [O]

#### 0x00138C3C `meas_get_138d50_u32` — data-accessor
u32 getter at *PTR_DAT_00138d50. Callers 0x000ADC1E, 0x000AE5A0, 0x000B912C. [O]

#### 0x00138C42 `tbl_copy_2w_frame` — logic
Copies two u32s from default table PTR_FUN_00138d54 (+0x24 -> frame+0xc, +0x1c -> frame+8). Caller 0x00132980. [O]

#### 0x00138C4E `meas_odometer_eval` — logic
59-instr odometer-style evaluator (caller 0x0014F7EA): sentinel 0xffffff -> error 2/flag; otherwise u16 = raw/100 (capped 0x9FFF5) at +6, raw/10 at +8, and latches the /10 value into table 0x00138d54[+0x1c] with complement, notifying via 0x00138D3C when changed. /10 and /100 factors read as 0.1-km/1-km style counters [I]. [O]

#### 0x00138CCC `accum_frac_totalizer` — logic
Fractional totalizer (caller 0x00138C4E): float accumulator *PTR_DAT_00138d74 += (u16@0x00138d64+0x6e * u16@0x00138d6c) / (s16@0x00138d68+4 * DAT_00138d70) while mode byte 0x00138d64[0x76]!=2 and denominator != 0; on reaching 1.0 increments the u32 at ctx+0xc, subtracts 1.0, and latches the counter into table 0x00138d54[+0x24] with complement + notify 0x00138D3C. Numerator/denominator accumulation reads as consumption/distance totalizing [I]. Cluster of 1. [O]

#### 0x00138D7A `meas_flag_qual2` — logic
If the flag byte param[1]!=0 outputs value 2/quality 0, else passes raw param[0] through with quality param[1]!=0. Caller 0x0014F34C. [O]

#### 0x00138D98 `meas_get_138f90_u16` — logic
Wrapper getter returning a u16 via FUN_00138DDE on global 0x00138f90. Caller 0x000A7C9C. [O]

#### 0x00138DA6 `meas_get_138f90_b` — logic
Base of a 4-member cluster of wrapper getters over global 0x00138f90 (fields via 0x00138DE2/0x00138DE8/0x00138DEE/0x00138DF4); callers 0x000A7C9C and 0x000F7298. [O]

#### 0x00138DFA `diag_frm_init_1a` — logic
Frame init (caller 0x00132980): +0x1a = *_LAB_00138f94, +0x1f = 1, and copies the u16 at **(frame+4) into +4. [O]

#### 0x00138E12 `lever_gear_interp` — logic
421-instr lever/gear interpreter (callers 0x00132D8E, 0x0014F34C): resolves the active mode 1..7 via 0x00139406, then per-mode state machines over lever bytes (+0x12: 0..0x0f) and gear-digit bytes ('1'..'8' = 0x31..0x38 at +0x11) assemble a command bitmask at +0x18 (bits 0x8/0x20/0x40/0x80/0x100/0x200/0x800/0x1000/0x1800/0x808), decrementing a hold counter at +4. Sub-modifiers: 0x001391C4 (bit 0x100 toggle), 0x0013921E (bit 0x200 handshake), 0x0013931A (state->bitmap), 0x0013945E (gate flag). [I] — cross-ref spec 09/11 shift/lever docs.

#### 0x001391C4 `lever_bit8_toggle` — logic
Edge-toggles the +0x1d handshake flag when input +0x20 rises (gated by *PTR_DAT_001393c8/0x001393cc) and sets or clears bit 0x100 of the command word accordingly. [O]

#### 0x0013921E `lever_bit9_update` — logic
107-instr: on +0x1e handshake, classifies 2 bits at 0xc00 of *PTR_DAT_001393d0 into states 2/3/4 (with sub-flag *PTR_DAT_001393d4); then evaluates gear-digit/lever conditions (0x31..0x38, status -4/0x40, bit 0x8000 at +0x18) to set/clear bit 0x200 and latch +0x1a. [I]

#### 0x0013931A `lever_state_bitmap` — logic
Maps the state byte at +0x18 to a single command bit: 0->0x8000, 1->8, 2->0x80, 3->0x100, 4->4, 0xa->0x20, 0xb->0x40, preserving bit 0x200. [O]

#### 0x00139374 `lever_latch_notify` — logic
Writes the selected latch byte (0xff, 0xfe or the +0x1a value) into table 0x001393e0/0x00139494[0x16] with complement, gated by freeze flag *PTR_DAT_001393dc, then notifies 0x0013948A. Caller 0x00140E94. [O]

#### 0x00139406 `lever_mode_find32` — logic
Scans up to 32 entries via handler PTR_FUN_001394a0(*(frame), i, 0) for a match against *PTR_DAT_0013949c, fetching the code with arg 1 on hit; sanitizes to 1..7 (0 or >7 -> 1). Uses 0x00151D98 tables. [O]

#### 0x0013945E `lever_gate_flag` — logic
Sets +0x22 = (+0x17==0 && +0x16==2 && +0x15==0 && +0x14==2) — a four-condition enable gate. [O]

#### 0x001394A4 `meas_get_1395b8` — data-accessor
Byte getter at *PTR_DAT_001395b8. Six callers (0x000F7298, 0x00132D8E, 0x000A7C9C, 0x000F471C, 0x0014FC5C, 0x000B912C). [O]

#### 0x001394AC `meas_get_1395bc` — data-accessor
Byte getter at *PTR_DAT_001395bc. Same caller pool. [O]

#### 0x001394C4 `tbl_copy_2b_frame` — logic
Copies two default bytes from table PTR_DAT_001395c0 ([0x35] -> +9, [0x34] -> +10). Caller 0x001394B6. [O]

#### 0x001394D4 `lever_mode_filter` — logic
103-instr mode filter: branches on the source byte (0..3) gating on globals 0x001395c4[0x7e], *PTR_DAT_001395c8, 0x001395cc[0x29]; produces the mode at +7 with quality +8 (0/1/2), holds last-good (+5/+6) when invalid, and decrements the hold counter +4. Caller 0x001394B6. [I]

#### 0x001395D0 `meas_get_1395f8` — logic
Wrapper getter via FUN_001395DE on global 0x001395f8. Callers 0x00144108, 0x00077AC4. [O]

#### 0x001395E6 `meas_flag_qual3` — logic
Passes the raw byte through, or substitutes 3 when the flag param[1]!=0. Callers 0x00132D8E, 0x0014F34C. [O]

#### 0x001395FC `meas_get_139754` — logic
Base of a 4-member cluster of wrapper getters over global 0x00139754 (via 0x00139634 and siblings); caller 0x000F7298. [O]

#### 0x0013964E `lever_sel_eval` — logic
106-instr selection evaluator: computes the enable +0x20 from (+0xa==0 && +9==1), then per-source branches (mode 0/2/3 at **(char**)frame) pick outputs +0x21..+0x23 (value/quality) with ready +0x1e and quality +0x1f, falling through to 0x0013999C for the default path. Callers 0x00132D8E, 0x0014F34C. [I]

#### 0x00139758 `prio_arbiter7` — logic
230-instr 7-way priority arbiter: for each candidate i (0..6) enabled by param flag and mask bit (1<<i of byte *(ctx+8)), fetches a cost via PTR_FUN_001399f8(*(ctx+4), i); keeps the strictly-lowest non-zero cost and sets exactly one of the one-hot flags (+0x2c, +0xb4, +0xb8, +0xbc, +0x30, +0xc4, +0xc8), returning the winner index 1..7 (0 = none). [O]

#### 0x0013999C `prio_select_eval` — logic
Builds the seven candidate gates (+0x25..+0x2b) from paired status bytes (e.g., +0xd/+0xf zero with +0xc/+0xe set, +0x1a/+0x1b in {1,2}, value-range checks on +0x1c in 0..0xe), runs arbiter 0x00139758, stores the winner to *PTR_DAT_00139b54 and maps it to outputs +0x21..+0x24 (winner 7 takes +0x10/+0x12 path). [I]

#### 0x00139AE6 `prio_select_eval_ptr` — logic
Callers=- variant of the 0x0013999C tail using the register-held context (unaff_r14): calls 0x00139758 directly and maps the winner to +0x21..+0x24. Reached via a function-pointer table. [I]

#### 0x00139B58 `diag_resp_timeout_chk` — logic
When the pending flag +0x4bb==1: compares a timer via getter PTR_PTR_00139d3c against (+0x4b4 + byte +0x4ba); then compares the expected u16 +0x4b8 with getter PTR_PTR_00139d40 — clears the flag on match, else escalates via 0x0013A102. A response/handshake timeout checker [I]. Caller 0x0014F34C. [O]

#### 0x00139B9C `chan_status_collect` — logic
Indexed status collector (caller 0x0013219C, the same indexed getter the PID-0x17 handler uses with arg 6): for channel param_2 (3..0xc) calls the matching getter (PTR_PTR_00139d44/48/54/58/5c/60, 0x00139EF2, PTR_FUN_00139d50, *DAT_00139d64) and appends {counter++, result} into the per-channel slot at ctx + param_2*0x12. [O]

#### 0x00139C40 `chan_rd_collect` — logic
Indexed channel reader (caller 0x001321AC): for channel param_2 (3..0xc) calls the matching getter (PTR_PTR_00139d68..0x00139d84, 0x00139FB4 for ch 5) and appends {counter++, value} at ctx + param_2*0x12 fields +2/+3. Sibling of 0x00139B9C with different slot fields. [O]

#### 0x00139CE4 `chan_ctrl_apply` — logic
Indexed channel control (caller 0x001321CC): calls the channel setter with param_2 (PTR_PTR_00139d88..0x00139f7c, 0x0013A022 for ch 5), records {counter++, result, command} at fields +4/+5/+6. [O]

#### 0x00139DF4 `chan_rd_u16` — logic
Indexed channel u16 reader (caller 0x001321BC): getter per channel (PTR_PTR_00139f80..0x00139fa0, 0x0013A09E for ch 5), records {counter++, u16} at +7/+8. [O]

#### 0x00139E98 `chan_evt_poll` — logic
For a non-zero channel index calls getter PTR_PTR_00139fa4 (0x00079792) and records {counter++, byte} at +0xa/+0xb. Callers 0x0013A102, 0x001321DE/EE/FE, 0x0013220E. [O]

#### 0x00139EC4 `chan_evt_set` — logic
Calls PTR_PTR_00139fa8 (0x000797E0) with (index, value) for non-zero index and records {counter++, value} at +0x11/+0x10. Caller 0x00132316. [O]

#### 0x00139EF2 `chan5_sub_collect_b` — logic
Channel-5 sub-collector: fills a 5-byte local with 1, overrides [0]/[2] from getters (0x000A479E, 0x0009B63E), classifies (any 0 -> 0, any 2 -> 2, else 1), then appends {counter++, value} into five sub-slots at ctx+0x480 (stride 10, fields +0/+1). Called from 0x00139B9C. [O]

#### 0x00139FB4 `chan5_sub_collect_a` — logic
Same five-sub-slot pattern with local init 0, overrides via 0x000A47AC/0x0009B64C, result 1 if any entry ==1; records fields +2/+3. Called from 0x00139C40. [O]

#### 0x0013A022 `chan5_sub_ctrl` — logic
Five-sub-slot control variant (called from 0x00139CE4): local init 1, calls PTR_PTR_0013a168 then overrides [2] via PTR_PTR_0013a16c(command); result 0 if any entry 0 else 1; records {counter++, value, command} at +4/+5/+6. [O]

#### 0x0013A09E `chan5_sub_sum` — logic
Five-sub-slot s16 variant: local s16s init 0, overrides [0]/[2] via 0x000A47BA/0x0009B65A, sums all five as the return; records {counter++, s16} at +7/+8. Called from 0x00139DF4. [O]

#### 0x0013A102 `diag_resp_arm` — logic
Arms a pending response (callers 0x00139B58, 0x00142112): samples three getters (PTR_PTR_0013a178/7c/80) into +0x4ba/+0x4b4/+0x4b8, polls channel 6 via 0x00139E98, and sets the pending flag +0x4bb=1. [O]

#### 0x0013A138 `chan10_status_call` — stub
Calls the channel-10 status function via *DAT_0013a184 with arg 10 and returns its byte. Caller 0x0013E5FC. [O]

#### 0x0013A148 `meas_get_gated_3a188` — logic
Returns getter PTR_PTR_0013a18c's byte only when *PTR_DAT_0013a188 == 0, else 0. Caller 0x00132320. [O]

#### 0x0013A190 `meas_get_13a3c8` — logic
Wrapper getter via FUN_0013A1D4 on global 0x0013a3c8. Cluster of 2; member 0x0013A19E uses field getter 0x0013A1DA on the same global. Callers 0x000F471C, 0x000B912C. [O]

#### 0x0013A1AC `saf_chk_u32pair_halt3` — logic
Fourth copy of the u32 complementary-pair check with the 0x0007A914/0x000794C8 fault pair and infinite halt (indirects at 0x0013a3cc/d0/d4). Caller 0x0013A350. [O]

#### 0x0013A1E0 `saf_ctx_init_2000` — logic
Frame init (caller 0x00132980): five bytes to 0xff, param[0]=2000 and param[1]=0xfffff82f (the 32-bit complement of 2000). [O]

#### 0x0013A1FE `os_release_res_ffe4` — stub
Releases resource id 0xFFE4 via (*DAT_0013a3dc)(PTR_DAT_0013a3d8, 1, &DAT_0000ffe4). Caller 0x00148DBA. [I]

#### 0x0013A20C `saf_mon_task_ffe4` — logic
148-instr monitor body under resource 0xFFE4 (callers 0x00133476, 0x0014F34C): verifies three complementary byte pairs (+0xe..+0x13, halt on violation), computes the enable +0x10 = (+8==1 && +9==1 && +0xc==0), runs sub-body 0x0013A350, recomputes complements, scrubs 12 slots, verifies two more pairs (+9/+0xa, +0xc/+0xd) and releases the resource. [I]

## Unit summary
Unit 42 covers the r10_app slice 0x00134B22–0x0013A20C: 220 clusters, 251 named functions, 0 honest-`unknown` (every function got at least a structural name; strongly semantic names are marked [I] where inferred).

Kind counts (by names row): logic 225, stub 25, dispatch 1.

### Functional map of the unit
1. **Diagnostic PID read family (0x00134B22…0x00135642)** — dispatcher on an OBD-Mode-$01-like selector set (0x0A–0x6F) plus ~44 per-PID encode handlers (getters via flash pointer tables, scaling /80, >>5, (v+100)>>4, /40, bit-packing), default empty response. A second dispatcher 0x00134AD8 (outside unit) shares handlers by index. Response plumbing: *DAT_00135738 + *unaff_r13 length.
2. **Safety/monitor framework (0x001357D0…0x001372C4, 0x001386E4…, 0x0013A20C)** — complementary-pair (x/~x) RAM integrity checks with halt fault pair (0x0007A914/0x000794C8), OSEK-style resource wrappers (ids 0x0b, 0x0c, 0x14, 0xd6, 0xfffa, 0xfffb, 0xffe4), debounce/timeout monitors, RAM cell AA/55 test, 0xC0DE/0xBAAD magic markers.
3. **Warning/DTC monitor (0x00136244…0x00136966)** — 53-entry DTC monitor with group gates, per-entry debounce times, 12 report slots; warning condition eval and lamp event reporting.
4. **State-latch task (0x00137668 + deb_latch_* 0x00137B68…0x0013848E)** — live-vs-default value selection, 3-stable debounce before mirroring values with complements into flash-resident tables + 0x0013852E notification.
5. **Lever/gear interpreter (0x00138E12 + 0x001391C4…0x0013964E)** — mode find/filter, command-bitmap assembly (bits 8…0x1000), gear digits '1'-'8'; priority arbiter 0x00139758/0x0013999C (7 sources, lowest-cost).
6. **Measurement getters (~35 small accessor wrappers)** — singletons 0x0013747c/0x00137844/0x00138c2c/0x00138f90/0x00139754/0x0013a3c8 and RAM globals, consumed by the PID layer and by callers in r08_app (0x000F7298 getter pool, 0x000B912C, 0x00155AE4).
7. **Channel subsystem (0x00139B58…0x0013A148)** — indexed (3..12) channel read/control/collect with per-channel 0x12-stride slot records; channel-5 five-sub-slot collectors; response arm/timeout.

### Unknown / low-confidence entries
- 0 unknown-kind rows; the weakest inferences (flagged [I] in entries): diag_* PID numbering (OBD mapping inferred from selector range + scaling shapes), saf_* monitor purposes, lever_* semantics, accum_frac_totalizer, prio_arbiter7 cost meaning.

### Cross-region leads
- Getter targets live in **r04_boot** (0x0007A9xx/0x0007BFxx/0x0007C4xx), **r08_app** (0x000A10xx/0x000A46xx/0x000A47xx, 0x0009B6xx) and **r14_app** (0x00140Axx, 0x0014C5xx, 0x0014BBA8/0x0014EBC2, 0x00151Dxx tables) — those units should cross-check the getter semantics used here.
- Duplicated cluster members physically in r14_app: 0x00141A16, 0x0014B44E, 0x0014B518, 0x00149754, 0x00149758 (byte-identical field getters) — r14's unit should reference these names.
- 0x001321xx (channel accessors), 0x00132980 (diag frame builder calling ~20 of this unit's init helpers), 0x00132D8E, 0x00134AD8 (PID index dispatcher), 0x0014F34C (task scheduler calling most monitor bodies), 0x00148DBA (resource-release stub caller) are outside this unit but tightly coupled — recommend documenting them in their own units against these names.
- 0x0013852E (latch notify) and 0x0013A350 (sub-body of the 0xFFE4 monitor) are called from this unit but not in it.
