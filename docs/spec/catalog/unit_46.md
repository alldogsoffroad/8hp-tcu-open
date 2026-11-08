# Unit 46 (r14_app)

## Module context
Unit 46 continues the r14_app safety-layer block (units 43-45 covered 0x14Axxx-0x14CExx). The
block 0x1518xx-0x153A5x is one module: ~19 sub-block structs (PTR_DAT literal-pool objects)
each with a conditioner function, run per-cycle by 0x0015387E and reset by 0x00153910 (both
reached via pointer tables). Integrity is enforced by complementary (inverted) copies with a
fatal handler + halt on mismatch (same pattern as unit 45's `chk_compl_fatal`). Later in the
unit: measurement protocol 0x153xxx-0x155xxx (spec 10) and park/range safety layer 0x158xxx
(spec 14 §6).

Batch 1 (clusters 1-25)

#### 0x00152CB6 `blk_setpoint_rate_calc` — logic
Setpoint computation for the 0x152xxx module block: mode byte at +0xC selects between
(rate × u16@PTR_00152e2c × **(cfg+8)) / divisor(+8) and (rate × ((raw(+6) × **(cfg+8)) >> 8)) / 1000,
guarding divide overflow against 0xffffffff/rate. Result clamped to 0..0x7FE through 0x001550B8
and stored at +0xA. Called only by 0x00152D74. Cluster of 1. [I]
#### 0x00152D28 `blk_crossing_latch` — logic
Direction-aware threshold latch on the sub-block: compares u16 at +8 against the u16 at *(+0);
in mode +0xC==0 sets the latch on rising crossing, in mode 1 clears it on falling crossing,
otherwise holds. Returns the latch. Cluster of 1. [O]
#### 0x00152D64 `blk_init_100_1_7ff` — stub
Sub-block reset (from 0x00153910): byte +4 = 100, u16 +8 = 1, u16 +0xA = 0x7FF. Cluster of 1. [O]
#### 0x00152D74 `blk_gated_setpoint_step` — logic
Requires seven chained validity callbacks (PTR_PTR_00152e34..0x00152e50, all must return 0);
then a module state byte at PTR_DAT_00152e54 of 5/7 runs the crossing latch 0x00152D28 plus the
rate calc 0x00152CB6, 6/8 forces output 0, anything else forces 0x7FF (neutral/fail value) into
short at +0xA. Cluster of 1. [I]
#### 0x00152E58 `get_u8_2e7c` — data-accessor
3-instr getter: returns the byte behind literal-pool pointer DAT_00152e7c (module state); polled
by the giant r14_app gatherer 0x00144108 and r0c_app 0x00077C54 (CAN TX field source). Cluster of 1. [O]
#### 0x00152E62 `blk_and2_flags` — logic
Combines two flags of the sub-block: +5 = (**(byte**)+0 != 0) && (byte +4 != 0). From 0x0015387E. [O]
#### 0x00152E80 `get_u8_2ea0` — data-accessor
3-instr getter (byte via DAT_00152ea0); polled by 0x00144108 and 0x00077B90. Cluster of 1. [O]
#### 0x00152E8A `blk_not_state_2_4` — logic
Writes 0 when the module state byte at PTR_DAT_00152ea4 is 2 or 4, else 1 — a "state not in
{2,4}" flag for the sub-block. From 0x0015387E. [O]
#### 0x00152EA8 `get_i16_2ee8` — data-accessor
Returns (sign-extended) word behind DAT_00152ee8; polled by 0x0014FC5C (module 2 gatherer). [O]
#### 0x00152EB0 `blk_clamp_0_30` — logic
When two module status bytes at PTR_DAT_00152eec[8]/[0xC] are clear and the source flag at
**(char**)+0 == 1, clamps the short at +4 to 0..30 via 0x001550C8 into +6, else 0. [I]
#### 0x00152EF4 `get_u8_2f54` — data-accessor
3-instr getter (DAT_00152f54); polled by 0x0013498E (r10_app). Cluster of 1. [O]
#### 0x00152EFE `blk_change_hold_timer` — logic
Edge/hold step: +5 = (PTR_DAT_00152f58[0xD] != [0xE]); on edge with state byte PTR_DAT_00152f5c
== 1 reloads hold byte +7 from the source flag; each pass copies +7 to +6 and decrements +7
when held. [I]
#### 0x00152F60 `get_u8_2ff4` — data-accessor
3-instr getter (DAT_00152ff4); polled by 0x00144108, 0x00077AC4, 0x00077B90. Cluster of 1. [O]
#### 0x00152F68 `get_i16_2ff8` — data-accessor
Returns word behind DAT_00152ff8; polled by 0x0007B6D8 and 0x00135010. Cluster of 1. [O]
#### 0x00152F6E `get_i16_2ffc` — data-accessor
Returns word behind DAT_00152ffc; polled by 0x0007B6D8. Cluster of 1. [O]
#### 0x00152F74 `blk_set_ffff_c` — stub
Reset step (from 0x00153910): u16 at +0xC = 0xFFFF (invalid marker). Cluster of 1. [O]
#### 0x00152F7A `blk_search16_match` — logic
Scans a 16-entry table through the accessor at PTR_FUN_00153004: entry idx matches when
fn(src, idx, 0) equals the target byte at PTR_DAT_00153008; records idx at +4, found at +6, and
fn(src, idx, 1) result (10000 stored at +0xA when it returns 1), with the complement of +0xA at
+0xC. Same search is inlined twice in 0x0015387E. [O]
#### 0x0015300C `get_i16_3168` — data-accessor
Returns word behind DAT_00153168; polled by 0x00144108 and 0x0007784C. Cluster of 1. [O]
#### 0x00153012 `get_u8_316c` — data-accessor
Returns byte behind DAT_0015316c; polled by 0x00144108 and 0x0007784C. Cluster of 1. [O]
#### 0x0015301A `chk_compl_pair_a` — logic
Complementary-copy integrity check of the sub-block (u16 +0xE vs ~+0x10, byte +0x12 vs ~+0x13):
on mismatch invokes the fatal-handler pair (PTR_PTR_00153170/74/78 = 0x0007A914, 0x000794C8)
then spins forever. Same pattern as unit 45 `chk_compl_fatal`. [O] spec 13 §2
#### 0x00153056 `chk_compl_pair_b` — logic
Second complementary-pair check of the same sub-block (u16 +8 vs ~+0xA, byte +0xC vs ~+0xD),
same fatal-handler + halt path. [O] spec 13 §2
#### 0x00153090 `blk_init_invalid2` — stub
Reset step (from 0x00153910): +0xA = 0xFFFF, +0xD = 0xFF, +0xE = 0xFFF, +0x10 = 0xF000
(complement), +0x13 = 0xFF. [O]
#### 0x001530AC `rpt_evt_1_32` — stub
Single indirect report call report(PTR_DAT_0015317c, 1, 0x32) — spec-13 fault/event reporting
with group/ID 0x32; called from 0x00153996. [I] spec 13 §2
#### 0x001530B8 `blk_cond_rpt32` — logic
Per-cycle conditioner (from 0x0015387E): starts event 0x32 tracking (PTR_FUN_00153184(PTR_DAT,0x32)),
verifies integrity (0x0015301A); when the source mode byte at **(+4) == 0xAA computes
value = fn(*(float*)(short +8)/const at DAT_00153188) clamped 0..0xFFE into +0xE (complement
+0x10, byte copy +0x12/+0x13), else zeroes them; re-checks via 0x00153056; on completion
(*param==1) stops the event (…,1,0x32) once. Callees 0x00154F2C/0x00154F66/0x00154FF8/
0x00155118/0x00155030/0x00155054/0x0015509E are report/clamp helpers. [I] spec 13 §2
#### 0x001531A0 `get_u8_32f0` — data-accessor
3-instr getter (DAT_001532f0); polled by 0x00144108 and 0x0007784C. Cluster of 1. [O]

Batch 2 (clusters 26-50)

#### 0x001531A8 `get_u8_32f4` — data-accessor
3-instr getter (DAT_001532f4); polled by 0x00144108 and 0x0007784C. Cluster of 1. [O]
#### 0x001531B0 `get_u8_32f8` — data-accessor
3-instr getter (DAT_001532f8); polled by 0x00144108 and 0x00077AC4. Cluster of 1. [O]
#### 0x001531B8 `get_ind_u8_32fc` — data-accessor
7-instr indirect getter: tail-calls the getter behind PTR_PTR_001532fc (0x0009B686); polled by
0x00144108 and 0x00077B90. Cluster of 1. [O]
#### 0x001531C6 `get_ind_u8_3300` — data-accessor
7-instr indirect getter through PTR_PTR_00153300 (0x0009B696); polled by 0x00144108 with
0x00077AC4 (rep) and 0x0007784C (member 0x001531D4). Cluster of 2. [O]
#### 0x001531E2 `get_ind_u8_3304` — data-accessor
7-instr indirect getter through DAT_00153304 (0x0009B68E); polled by 0x00144108, 0x00077AC4. [O]
#### 0x001531F0 `blk_init_f_11` — stub
Reset step (from 0x00153910): +0xF = 0xF, +0x11 = 0. Cluster of 1. [O]
#### 0x00153310 `get_u8_33e4` — data-accessor
3-instr getter (DAT_001533e4); polled by 0x00144108 and 0x00077AC4. Cluster of 1. [O]
#### 0x00153318 `get_u8_33e8` — data-accessor
Same shape, DAT_001533e8. Cluster of 1. [O]
#### 0x00153320 `get_u8_33ec` — data-accessor
Same shape, DAT_001533ec. Cluster of 1. [O]
#### 0x00153328 `get_u8_33f0` — data-accessor
Same shape, DAT_001533f0. Cluster of 1. [O]
#### 0x00153330 `get_u8_33f4` — data-accessor
Same shape, DAT_001533f4. Cluster of 1. [O]
#### 0x00153338 `blk_remap_lt4_else1` — logic
Maps the byte at +2: values 0..3 pass through to +0xA, anything >= 4 becomes 1. Called by
0x0015334A. Cluster of 1. [O]
#### 0x0015334A `blk_qual_status` — logic
Qualification/status step of the 0x152xxx module (from 0x0015387E): selects on *param (0/2),
validates the lever source through PTR_PTR_001533fc, and fills flags +5..+9, +0xB
(+9 = param[1]==8), then delegates to 0x00153338. Callees 0x000A031C, 0x00153338. [I]
#### 0x00153400 `blk_gather_thunk` — stub
3-instr scheduler thunk: runs the input gatherer 0x001534FC on object PTR_DAT_001534f4.
Cluster of 2 — member 0x00153424 instead runs the conditioner driver 0x0015387E (semantically
different callee, so separately named `blk_run_thunk`). Both called from 0x0015346C. [O]
#### 0x00153424 `blk_run_thunk` — stub
Member of the 0x00153400 cluster: runs 0x0015387E on PTR_DAT_001534f4. [O]
#### 0x0015346C `blk_sched_run3` — logic
7-instr scheduled driver (callers=-, so reached from a task/pointer table): runs gather thunk
0x00153400, run thunk 0x00153424, then one empty slot stub 0x00153448. Cluster of 5 — members
0x0015347A/0x00153488/0x00153496 call other step-thunk triples; member 0x001534A4 calls only
empty stubs (0x0015341E/0x00153442/0x00153466) and is a no-op filler. The 4-instr empty stubs
live in a 131-member cross-region cluster documented by unit 1. [O]
#### 0x001534B2 `blk_sched_phase10` — logic
Phase-stepped driver: runs steps 0x00153418/0x0015343C/0x00153460 (empty slot stubs here),
increments the phase counter at PTR_DAT_001534f8, and on reaching 10 wraps to 0 and runs
0x001534A4. Callers=- (pointer-table scheduled). Cluster of 1. [O]
#### 0x001534FC `blk_gather_60_inputs` — logic
277-instr input snapshot for the 0x152xxx module: invokes ~60 getter function pointers from the
literal pool (PTR_PTR_0015373c-0x00153874) — including r0c_app CAN getters (0x0009B5xx-0x000A4xxx,
0x0013BFxx, 0x0014C5xx) — storing bytes/words into the module object and two sub-structs, and
setting "fresh" flags (writes 1 at +0, +0x1c, +0x14 etc.). Called via 0x00153400. [O] spec 07
#### 0x0015387E `blk_run_19_cond` — logic
Per-cycle conditioner driver: invokes 19 (object, function) pairs from literal-pool tables
0x001539c0-0x00153a54 (several are empty stubs), then performs the 16-entry code search of the
lever/range object twice (both selector banks at *(obj+0) and *(obj+4), fn = PTR_FUN_00153308)
writing found index, match flag and the fn(...,1) result into the module struct, and derives a
change flag (bytes 0xD vs 0xE). Reached via 0x00153424. [I] spec 14 §5a (16-entry lever decode)
#### 0x00153910 `blk_reset_19_cond` — logic
Reset/init driver (callers=-, pointer-table reached): sets object header (*param=5, +0xA=0xFF,
+0xE=1) and invokes 19 (object, reset-function) pairs (0x00152014...0x001549C4; many are empty
stubs). Mirror of 0x0015387E for the init pass. [O]
#### 0x00153996 `blk_rpt5_evt` — logic
Runs five registered (object, function) report pairs from literal-pool slots
(PTR_DAT_001539e0/0x1539e8/0x153a00/0x153a08/0x153a28); callees include the event-0x32 report
stub 0x001530AC. Callers=- (pointer-table scheduled). [I] spec 13 §2
#### 0x00153AC6 `get_flag_by_sel` — data-accessor
Selector-mapped flag read for the monitor blocks: bitmask arg (1,2,4,5,6) picks one of five
flag pointers (PTR_PTR_00153b0c-0x00153b1c) and returns its byte; 0 returns 0. Called by
0x001424EE, 0x0014072C and 0x00142FBA. Cluster of 1. [O]
#### 0x00153B20 `meas_copy_thunk` — library
2-instr tail-jump to the byte-copy loop 0x00153B2C; used throughout the measurement-protocol
handlers (0x001543F8 dispatcher family) to fill response buffers. Cluster of 1. [O] spec 10 §3
#### 0x00153B2C `meas_copy_bytes` — library
Byte copy loop: n bytes from param_2 to param_1 (Ghidra names it thunk_FUN_00153b2c). [O]
#### 0x00153B34 `meas_copy3_skip1` — library
Copies 3 bytes src[1..3] to dst[1..3] (byte 0 untouched on both sides); used by 0x00153ED8. [O]
#### 0x00153B46 `meas_copy3_to_front` — library
Mirror of 0x00153B34: copies src[1..3] to dst[0..2]; used by 0x00153CBC (cmd 0x32 handler). [O]

Batch 3 (clusters 51-75) — measurement protocol (spec 10)

#### 0x00153BB4 `meas_cmd_ident` — logic
Identification handler (cmd 0x01/0x21): each call replies 0x21 + part number (1..3, then 0x84)
and copies 6 bytes from PTR_FUN_00153dc8/dd0/dd4/dd8 tables into the response buffer; part 4
sets state 0x18 which terminates the exchange. Matches spec 10 §3 row 0x01/0x21. [O] spec 10
#### 0x00153C66 `meas_cmd_22_addr16` — logic
Cmd 0x22: converts the request's measurement index (u16 at req+2) to its address through
0x0015548E (PTR_FUN_00153de0); if the address is above 0xFFF8FFFF replies 0xFFFF, else writes
the 16-bit offset; response 0x22 0x81 + 2 bytes. [O] spec 10 §3
#### 0x00153CBC `meas_cmd_32_addr24x2` — logic
Cmd 0x32: converts two indices (req+2, req+4) via 0x0015548E and appends the low 3 bytes of
each address (RAM scratch DAT_fff9606c/DAT_fff95dd8) through 0x00153B46; response 0x32 0x81 +
2 × 3 bytes. [O] spec 10 §3
#### 0x00153D1A `meas_cmd_11_addr32` — logic
Cmd 0x11: 32-bit RAM address of measurement index (u16 at req+2) via PTR_FUN_00153de4
(0x001554A2); response 0x11 0x81 + 4 bytes. [O] spec 10 §3
#### 0x00153D5E `meas_cmd_12_empty` — stub
Cmd 0x12: writes response header 0x11 0x81 with no payload (buffer clear only). [O] spec 10 §3
#### 0x00153D6E `meas_a16_list_check` — logic
Validates the 8-entry A16 address list at DAT_fff95dc8: for each entry (0xFFFFFFFF/unset gets
the default PTR_DAT_00153de8) computes addr = (low16 − 0x80000) (= 0xFFF8xxxx form) and runs
the permission check 0x001554BC(0,0,addr,2); allowed entries are stored back, any denial
clears the all-ok return. Called from cmd 0x23 handler 0x00153E46. [O] spec 10 §2/§3
#### 0x00153E04 `meas_a24_list_check` — logic
Same for the 8-entry A24 list at DAT_fff95ddc: entries forced to 0xFF000000|value and checked
via 0x001554BC(0,0,addr,4). Called from cmd 0x33 handler 0x00153ED8. [O] spec 10 §2/§3
#### 0x00153E46 `meas_cmd_23_a16_list` — logic
Cmd 0x23: accumulates request frames (part 1/2/0x83, 6 bytes = 3 A16s each) into the list
scratch DAT_fff95dc8/ce/d4, tracking a part mask in PTR_DAT_0015403c[0]; when all three parts
arrive (mask 7) runs the list check 0x00153D6E — failure replies 0x03 and clears, success arms
the periodic A16 read (state 0x80). [O] spec 10 §3
#### 0x00153ED8 `meas_cmd_33_a24_list` — logic
Cmd 0x33: same construction for the A24 list (parts 1/2/3/0x84 into DAT_fff95ddc..f8, mask
0xF) using 0x00153B34 copies; completes via the A24 list check 0x00153E04. [O] spec 10 §3
#### 0x00153FBC `meas_a16_snap4` — library
Gathers 4 words (2 bytes each) from the A16 list entries (stored as low16 offsets; byte access
at u16−0x80000 and −0x7FFFF) into an 8-byte transmit buffer. Called by 0x00153FFC. [O] spec 10 §4
#### 0x00153FE0 `meas_a24_snap4` — library
Gathers 4 words from raw 32-bit pointers of the A24 list into an 8-byte transmit buffer.
Called by 0x00154054. [O] spec 10 §4
#### 0x00153FFC `meas_a16_periodic_snap` — logic
When the A16 list is armed (state −0x80 at PTR_DAT_0015403c), snapshots both 8-entry lists
(0x00153FBC ×2) into the buffers behind PTR_DAT_00154044/0x00154048 and raises the two
transmit-ready flags at PTR_DAT_0015404c/0x00154050. Called from 0x00154500. [O] spec 10 §4
#### 0x00154054 `meas_a24_periodic_snap` — logic
A24 twin of 0x00153FFC: when armed (PTR_DAT_001542f4 == −0x80) snapshots both A24 lists via
0x00153FE0 into PTR_DAT_001542f8/0x001542fc buffers and sets ready flags 0x00154300/0x00154304. [O]
#### 0x00154088 `meas_cmd_20_read_words` — logic
Cmd 0x20: reads up to 3 A16 words from the request (loop while index < len 8), each through the
permission check 0x001554BC — allowed words are copied, denied ones become 0xFFFF. [O] spec 10 §3
#### 0x001540EE `meas_cmd_30_read3b` — logic
Cmd 0x30: two A24 addresses (3 bytes each) from the request; each allowed address contributes
3 bytes, denial yields 0xFF 0xFF 0xFF. [O] spec 10 §3
#### 0x00154166 `meas_cmd_28_2a_read6` — logic
Cmds 0x28 (absolute 16-bit address, taken as A16 low word) and 0x2A ('*' = full 32-bit
address): reads 6 consecutive bytes; each denied byte becomes 0x0E. Matches spec 10 §2 denied
block-read fill. [O] spec 10 §3
#### 0x001541EC `meas_cmd_29_reject` — stub
Cmd 0x29: writes 0x03 (rejected) to the response buffer. [O] spec 10 §3
#### 0x001541F6 `meas_cmd_02_reset` — logic
Cmd 0x02: replies 0x01, invokes the mode/reset thunk 0x000A0E84 (PTR at 0x00154314) and clears
the response buffer tail via 0x00153B58. [O] spec 10 §3
#### 0x0015420A `meas_cmd_0e_value` — logic
Cmd 0x0E: echoes request byte 1 and appends the 16-bit measurement value for index (u16 at
req+2) obtained through PTR_FUN_00154318 (0x00155534). [O] spec 10 §3
#### 0x0015424C `meas_cmd_04_05_wr_byte` — logic
Cmds 0x04/0x05: write-byte at the A16 address (req+2); permission check 0x001554BC(1,0,addr)
first — success replies 0x01, denial 0x03. [O] spec 10 §3
#### 0x0015429A `meas_cmd_0a_0b_wr_word` — logic
Cmds 0x0A/0x0B: write-word variant of 0x0015424C (check arg 2 = size). [O] spec 10 §3
#### 0x0015431C `meas_cmd_34_35_wr_byte_a24` — logic
Cmds 0x34/0x35: write-byte at the A24 address (req bytes 2..4, addr − 0x1000000) via check
PTR_FUN_001544ec (0x001554BC); replies 1/3. [O] spec 10 §3
#### 0x00154370 `meas_cmd_3a_3b_wr_word_a24` — logic
Cmds 0x3A/0x3B: write-word at A24, two data bytes from req+5/+6. [O] spec 10 §3
#### 0x001543D0 `meas_cmd_default_log` — logic
Default/unknown command: replies 0x03 and records the command byte into the 8-slot ring at
DAT_fff95dc0 (index at PTR_DAT_001544f4, stops at 8). [O] spec 10 §3
#### 0x001543F8 `meas_dispatch` — dispatch
Measurement-protocol command dispatcher (called by 0x00154500): byte 0 of the request selects
among 0x01/0x21→0x00153BB4, 0x02→ack, 0x08→0x001541F6, 0x20/0x30/0x0E/0x11/0x12/0x22/0x23/
0x28/0x29/0x2A/0x04/0x05/0x0A/0x0B/0x32/0x33/0x34/0x35/0x3A/0x3B handlers above; 0xE0 (−0x20)
clears the periodic-list state and arms list building; anything else → 0x001543D0. Matches
spec 10 §1 "dispatcher 0x1543F8". [O] spec 10

#### 0x00153B58 `meas_buf_clear_tail` — library
Zeroes the response buffer (PTR_DAT_00153da4) bytes from index param_1 (low byte) through 7;
used by nearly every measurement-protocol handler to blank the unused response tail. [O] spec 10
#### 0x00153B72 `meas_req_timeout_tick` — logic
Request-timeout watchdog step (called from the protocol scheduler 0x00154500): while
counter PTR_DAT_00153dac < PTR_DAT_00153da8 and a request flag object (PTR_DAT_00153db0) is
set, fires its one-shot callback (PTR_FUN_00153db4), clears the request and two associated
flag locations; always increments the counter at PTR_DAT_00153da8, then delegates to
0x001553B8. [I] spec 10

Batch 4 (clusters 76-100) — protocol engine + module libraries

#### 0x00154500 `meas_sched_step` — logic
Scheduler step of the measurement protocol (called by monitor scheduler 0x0014F34C): when the
state getter (PTR_FUN_00154558) reports active, runs the timeout tick 0x00153B72; on a new
request frame (flag at **PTR_DAT_00154560) invokes the dispatcher 0x001543F8, then raises the
response-ready flag and clears the request; finally, if a response is pending, refreshes the
periodic A24/A16 snapshots 0x00154054/0x00153FFC. [O] spec 10
#### 0x0015456C `meas_e0_frame_intake` — logic
Periodic-list intake (cmd 0xE0): for frame number n = (b1 & 0xF) copies 3 u16 indices into the
32-entry list at PTR_DAT_001547cc, marks the frame in a bitmask (PTR_DAT_001547bc[2]) and, on
first receipt of a frame, validates its indices through 0x001546BA; b1 in 0x81..0x8E marks the
last frame → state 0xE → resolve via 0x00154718; state 0xAB triggers the TX scatter 0x00154800;
a 1000-tick stall counter (PTR_DAT_001547dc) aborts with 0xE1. Matches spec 10 §4 intake. [O]
#### 0x001546BA `meas_e0_check3` — logic
First-receipt check of an 0xE0 frame's three indices: for each index calls the two-byte bitmap
test 0x001552F2 and advances the slot counter (PTR_DAT_001547bc[3], two slots for two-byte
measurements); exceeding 0x1E (30) sets state 0xE (list overflow). [O] spec 10 §4
#### 0x00154718 `meas_e0_resolve_addrs` — logic
Converts the up-to-32-entry index list into RAM addresses: 0x0015548E (PTR_FUN_001547f4) maps
index→address, permission check 0x001554BC(0,0,addr,1) stores the address (or the denied
filler PTR_DAT_001547fc) into the 4-byte-stride list at DAT_fff95dfc; two-byte measurements
also claim the following slot (addr+1) via the 0x001552F2 bitmap. Matches spec 10 §4. [O]
#### 0x00154800 `meas_e0_scatter_tx` — logic
Scatters one byte from each of the 32 resolved addresses (DAT_fff95dfc, stride 4) into four
8-byte transmit buffers (PTR_DAT_00154878/7c/80/84) and raises the four transmit-ready flags
(0x00154888..0x00154894) — channels 0-7→0x368 … 24-31→0x398 per spec 10 §4. [O] spec 10 §4
#### 0x00154898 `lib_min_u16` — library
7-instr min(a,b) on 16-bit operands (returns b when b<=a). Cluster of 2 (member 0x001549B4,
different caller set: 0x13661C CAN monitor + 0x14976A vs 0x14D79E). [O]
#### 0x001548A6 `lib_max_u16` — library
7-instr max(a,b) on 16-bit operands; member 0x001548B4 is the byte-wide variant (callers
0x13FB6C, 0x13DFDC). Cluster of 2. [O]
#### 0x001548C2 `lib_min_i16` — library
6-instr signed min of two shorts; called by 0x1389E4. Cluster of 1. [O]
#### 0x001548CE `lib_abs_i32` — library
5-instr absolute value of a signed int (negates when negative). Cluster of 2 — member
0x00155094 identical, called by 0x001549CA. [O]
#### 0x001548D8 `lib_absdiff_u32` — library
|a−b| for unsigned operands using an unsigned b<=a compare; called by 0x148432. Cluster of 1. [O]
#### 0x0015490A `lib_mix_step` — library
Returns a + b − b/(n+1): one exponential-smoothing step toward b with rate n; called by
0x1389E4. Cluster of 1. [I]
#### 0x00154918 `lib_weighted_avg` — library
Bounded weighted average of x (u16) and y (u16) with weights m (u8, min 1) and k (u8):
asymmetric — y below x adds +1 (round-up); result (m*x + k*y (+k))/(m+k). Called by 0x13608C. [O]
#### 0x001549C4 `blk_init_ff7` — stub
Reset step (from 0x00153910): byte +7 = 0xFF. Cluster of 1. [O]
#### 0x001549CA `mon_ratio_windows` — logic
575-instr conditioner of the 0x152xxx module (from 0x0015387E): snapshots speed inputs
(PTR_DAT_00154c60/64/68) into the block, gates on two validity callbacks and mode bytes
(+0x22 in 0/1/2); a phase engine at +6 (states 0-3) accumulates position deltas (+0x10/+0x14)
from (target +0x1E − base +8/+0x26) with dwell counters (+0x33/+0xD); in state 3 computes the
ratio (acc2*100)/acc1 via PTR_FUN_00154f04 and compares it against two calibrated threshold
windows (cfg +0x1C/+0x20/+0x24/+0x28 and +0x30/+0x34/+0x38/+0x3C, gated by mode bytes +0x18/
+0x2C), setting eight out-of-window flags (+0x2B..+0x2E, +0x2F..+0x32) that latch until the
report timer (+2) expires; finally reports both flag groups through the spec-13 API pair at
PTR_PTR_00154f10..0x00154f24 (only when mode byte at PTR_DAT_00154f0c == 2). Callees
0x000A105E/0x00155088/0x001552D0/0x00155094/0x000A031C/0x001550EE. [I] spec 13 §2 (shift/ratio
plausibility monitor)
#### 0x00154F2C `chk_ramwr_byte` — logic
RAM-write verification: saves the byte at *param, writes 0xAA then 0x55, requires both to read
back, restores the original; failure invokes the fatal internal-error handler pair
(0x0007A914/0x000794C8) and halts. Ten callers across the module (0x001530B8 etc.). [O] spec 13 §2
#### 0x00154F66 `chk_ramwr_word` — logic
Word variant of 0x00154F2C (patterns 0xAA55.../0x5555); same fatal path. [O] spec 13 §2
#### 0x00154FA4 `chk_ramwr_dword` — logic
Dword variant using two literal-pool patterns (DAT_00154ff0/0x00154ff4); same fatal path;
cluster member 0x00154FDE is a 2-instr thunk of it. [O] spec 13 §2
#### 0x00154FDE `chk_ramwr_dword_thunk` — stub
2-instr tail-jump to 0x00154FA4; called from 0x0015261C. [O]
#### 0x00154FF8 `rpt_evt_pair` — logic
Event-report pair used by the module's report stubs (0x001530AC, 0x00152610, 0x00152412,
0x00152174, 0x00152B72): calls the spec-13 report entry (*DAT_0015507c = 0x0007A6E0 chain)
twice — once with data = 17000000+param_3 and once with data = 17999999−param_3 and the extra
flag param_4 (occurrence/aging counters or timestamp-coded data). [I] spec 13 §2
#### 0x00155030 `rpt_evt_hi_gate` — logic
Gated single report: when the enable getter (PTR_PTR_00155080) returns non-zero, reports
param_2 + 17000000 through the chain at PTR_PTR_00155084 (callees 0x0007A520, 0x00068EA0);
sibling of 0x00155054 with the mirrored constant. [I] spec 13 §2
#### 0x00155054 `rpt_evt_lo_gate` — logic
Mirror of 0x00155030: gated report of 17999999 − param_2. [I] spec 13 §2
#### 0x001550B8 `lib_clamp_u32` — library
8-instr clamp: returns param_2 when value < lo, param_3 when value > hi; used by 0x00152CB6. [O]
#### 0x00155118 `lib_f2u16_round` — library
Float→u16 conversion with offset (DAT_00155140, rounding) and saturation at DAT_00155144 /
0.0 (negative → 0, overflow → 0xFFFF); returns the low 16 bits. Used by 0x001530B8. [O]
#### 0x00155148 `blk_reselect_tables` — logic
Variant/table reselect (from 0x0015387E and 0x0015B010): reads a selector byte and its
complement through two getter pointers; when the mode int at PTR_DAT_00155248 is non-zero
forces selector 0 / complement 0xFF; on any selector change re-derives eleven pointer slots
(PTR_DAT_00155258..0x001552a8) from labelled anchors via PTR_FUN_00155254, writes the pair
back through four setter calls and verifies the complement — mismatch → fatal handler + halt. [I]
#### 0x001552E4 `meas_buf_setup_gate` — logic
Gate: when the enable byte at PTR_DAT_00155554 is non-zero, runs the buffer setup 0x0015531A.
Cluster of 2 — member 0x001553B8 identical, called from 0x00153B72 instead of 0x0015456C. [O]
spec 10 §1

Batch 5 (clusters 101-125) — measurement index core + lever monitors (P1614-00)

#### 0x001552F2 `meas_is_2byte_idx` — logic
Two-byte measurement test: builds the bit mask (0x8000 shifted by index mod 16) and returns
whether the corresponding bit is set in the 32-entry bitmap at PTR_LAB_00155558. Used by the
0xE0 list validator/resolver. Matches spec 10 §4 "bitmap at 0x155558, 0x1552F2". [O] spec 10
#### 0x0015531A `meas_buf_setup` — logic
Buffer/flag setup for the protocol: invokes ten getter pointers (PTR_PTR_0015555c-0x00155584)
into RAM scratch DAT_fff95f88..0x001555fac and clears the enable byte at PTR_DAT_00155554.
Matches spec 10 §1 "buffers set up by 0x15531A". [O] spec 10
#### 0x001553C6 `meas_ident_build` — logic
Builds the identification payload: 14 bytes at DAT_fff960c8 from a per-index table fn
(PTR_FUN_00155588), where codes 0xC9..0xE0 are replaced by bytes from the table behind
PTR_PTR_0015558c; copies them to DAT_fff95fb0 and appends the count 700 (u16 at +0xE) plus 8
raw bytes from PTR_PTR_00155590. Feeds the 0x21 responses of 0x00153BB4. [O] spec 10 §3
#### 0x0015548E `meas_idx_addr` — data-accessor
Measurement index → RAM address: index < 700 → the converter at PTR_FUN_00155594 (→0x00135788),
else 0xFFFFFFFF. Used by cmds 0x22/0x32 and the 0xE0 resolver. [O] spec 10 §5
#### 0x001554A2 `meas_idx_addr_alt` — data-accessor
Second index→address map with limit 100 (PTR_FUN_00155598 → 0x0013579C); used by cmd 0x11. [O]
#### 0x001554B4 `thunk_reset_a0e84` — stub
4-instr indirect tail-call of 0x000A0E84 (reset/mode request, spec 10 §3 cmd 0x08 note). [O]
#### 0x001554BC `meas_addr_perm_check` — logic
The address permission gate (spec 10 §2): for reads (arg1==0) an address inside
0xE000–0xFFFF is delegated directly to the region-check fn (PTR_PTR_001555a0, →0x000A3D1C
chain), otherwise every byte of the range must fall in 0xE000–0xFFFF; for writes (arg1==1)
runs the session check (PTR_PTR_001555a4) then the write-region fn (PTR_PTR_001555a8). Ten
callers — every read/write handler. [O] spec 10 §2
#### 0x00155534 `meas_idx_value` — data-accessor
Measurement value by index: optional byte-mask gate (PTR_FUN_001555ac, →0x000A10EA) then the
value getter (PTR_PTR_001555b0); returns u16. Used by cmd 0x0E. [O] spec 10 §3
#### 0x001555B4 `lever_snap_pair` — logic
Builds two lever-request bytes with complements from the object at PTR_DAT_00155808
(byte at +4 and byte at +0 → param[0]/[4], complements [1]/[5], init flags [2]/[6] and
0xFF pairs); called by r08_app 0x0009B228. [I] spec 14 (lever request object)
#### 0x001555E4 `lever_mon_p1614` — logic
P1614-00 lever monitor driver (group 0x23=35, called from r08_app 0x0009B39E): for each of
three conditions — source mismatch 0x001556AA, debounce A 0x001556D4, debounce B 0x00155890 —
first checks the calibration pointer (via PTR_FUN_00155810, skipping the 0xDEAC "not
calibrated" marker) and on trigger reports fault 0x23 with data 0xFF/1/2 (PTR_FUN_00155814)
and latches the event state (PTR_FUN_0015581c); finishes with the trace mark 0xCE4C85. [O]
spec 13 (fault group 35 = P1614-00), spec 14 §5a
#### 0x0015567E `lever_rpt23_2` — logic
Compact report path: reports fault 0x23 with data 2 and closes out with the 0xCE4C85 mark;
callers=- (reached from a pointer table). [I] spec 13
#### 0x001556AA `lever_src_differs` — logic
Change detector: compares byte 0 and byte 1 of the two lever objects at PTR_DAT_00155824 and
PTR_DAT_00155808; returns 1 when they differ (shifter vs translated range disagreement). [O]
#### 0x001556D4 `lever_debounce_a` — logic
194-instr debounce/translation step: reads the lever request byte (PTR_PTR_00155828) and the
error/status code (PTR_FUN_0015582c, 0xF7..0xFA), builds six plausibility conditions on the
request value (0xD, 0xB, 0, 1..10, 0xF vs the error codes — the spec 14 §5a code table); when
any condition holds, runs the RAM self-test/decrement 0x00155A50 on the debounce counter
pair (param[0]/[1] with complement, expiry flag param[2]/[3]); otherwise reloads from the
source object. Returns the expired flag. [I] spec 14 §5a
#### 0x00155890 `lever_debounce_b` — logic
207-instr sibling of 0x001556D4 operating on the second counter pair (param+4) with an extra
qualifier byte (cVar11 from FUN_00155A50 on PTR_DAT_00155a48) distinguishing request sets;
same six-condition plausibility family and counter handling. [I] spec 14 §5a
#### 0x00155A50 `chk_byte_selftest` — library
Byte RAM self-test + debounce decrement used 14 times across the lever/park modules: writes
0x00/0xFF/0xAA/0x55 to *param requiring read-back (fail → PTR_FUN_00155ab8), restores and
verifies the complement at param[1] (fail → 0x00155abc), then returns the current counter
value. Cluster of 14 duplicated copies (0x0015616E, 0x001573B6, 0x00157546, 0x00157820,
0x00157C42, 0x00157DC2, 0x0015806A, 0x00158588, 0x001589DE, 0x00159070, 0x001596CA,
0x00159FBC, 0x00159FBE). [O] spec 13 §2 (self-test pattern)
#### 0x00155AC0 `lever_snap_wrap` — logic
Wrapper (from r08_app 0x0009B306): emits trace marks 0xC68450/0xCE2570 around the 247-instr
input snapshot 0x00155AE4. [O]
#### 0x00155AE4 `mon2_snap_all` — logic
247-instr monitor input snapshot: ~60 getter pointers (PTR_PTR_00155d50-0x00155dfc) storing
value+complement pairs into a 0x44-byte block (bytes at 0/4/0x10..0x1A and words at
+6/+0xC/+0x1A/+0x20/+0x2A/+0x3A/+0x3E). Getters include r0c_app CAN getters (0x000A46xx,
0x0013BFxx) and r14_app measurement getters. Same role as 0x001534FC for the 0x155Exx
monitor family. [O] spec 07
#### 0x00155E02 `mon2_run_checks` — logic
Driver of the second lever monitor family (from r08_app 0x0009B546): runs the lever decoder
0x00155F8A, then four calibration-gated predicate/report steps (0x00155E2C data 1,
0x00155E72 data 2, 0x00155EB8 data 4, 0x00155EFE data 5, 0x00155F44 data 3 — all reporting
group 0x1E=30) and closes with trace mark 0xCE5068. [I] spec 13
#### 0x00155E2C `mon2_chk_cal_f` — logic
Gated predicate step: calibration slot 0xF (via PTR_FUN_00156050, skip when 0xDEAC); when
0x00156068 returns 1 (lever self-test/consistency fail), reports fault 0x1E data 1 and
latches the event. [O] spec 13
#### 0x00155E72 `mon2_chk_cal_10` — logic
Same pattern, calibration slot 0x10, predicate 0x001560B8, fault 0x1E data 2. [O] spec 13
#### 0x00155EB8 `mon2_chk_cal_12` — logic
Same pattern, calibration slot 0x12, predicate 0x00156102, fault 0x1E data 4. [O] spec 13
#### 0x00155EFE `mon2_chk_cal_15` — logic
Same pattern, calibration slot 0x15, predicate 0x0015612A, fault 0x1E data 5. [O] spec 13
#### 0x00155F44 `mon2_chk_cal_11` — logic
Same pattern, calibration slot 0x11, predicate 0x0015614C, fault 0x1E data 3. [O] spec 13
#### 0x00155F8A `lever_decode_ascii` — logic
ASCII lever decoder: maps the shifter character (getter PTR_PTR_00156060) to the range
request — ' '→0, 'P'→8, 'R'→7, 'N'→6, '1'..'8'/'D'/'L'/'S'→5 — with error codes 0xF7/0xF8/
0xF9/0xFA per group and 0xF0 unknown, stored with complements at param[0..3]; a second
status byte (PTR_PTR_00156064, 0/1) fills param[2..3] (0xFF/0xFE complements). This is the
"translated elsewhere" monostable-lever path of spec 14 §5a. [O] spec 14 §5a
#### 0x00156068 `mon2_pred_1` — logic
Predicate 1 of the 0x1E monitor: verifies the complement pairs of two inputs (PTR_PTR_
001561d4/d8 and PTR_FUN_001561dc/e0) and that the decoded lever value (0x0015616E self-test)
is not 0x0F (invalid); returns 1 = report. [O]

Batch 6 (clusters 126-150) — park/range module monitors (fault group 0x18=24, P1D8B/P1D8C)

Common pattern in this family: fetch calibration slot n through the mirror-checked table
read 0x001564AC (0xDEAC = not calibrated → skip), run a predicate, report fault group 0x18
with a step-specific data code via PTR_FUN_00156e5c/0x0015718c (spec-13 API), set the module
state byte (PTR_DAT_00156e68) and reset the lever-request object to the 1/0xFE pattern, then
emit a trace mark (0xCExxxx/0xCE25xx).

#### 0x001560B8 `mon2_pred_2` — logic
Predicate 2: when the decoded lever (0x0015616E) is non-zero, requires it to stay at 7 or 5
across two consecutive calls matching the reference (PTR_FUN_001561dc); returns 1 = report. [O]
#### 0x00156102 `mon2_pred_4` — logic
Predicate 4: decoded lever == 8 (P) while the reference (PTR_FUN_001561dc) is not 8. [O]
#### 0x0015612A `mon2_pred_5` — logic
Predicate 5: reference (PTR_FUN_001561e4) differs from the decoded lever of the second block
(param+2 via 0x0015616E). [O]
#### 0x0015614C `mon2_pred_3` — logic
Predicate 3: reference (PTR_FUN_001561dc) == 0 while the decoded lever is non-zero. [O]
#### 0x001561F0 `fatal_halt2` — logic
Fatal-halt helper (11 callers): invokes the two fatal-handler pointers (PTR_PTR_00156450/54 =
0x0007A914, 0x000794C8 chain) and spins forever. [O] spec 13 §2
#### 0x00156202 `park_mon_init_check` — logic
Init/check entry (from r08_app 0x0009B228): runs two registered functions, zeroes the block
(+0/+1 = 0/0xFF); when a check fn (PTR_FUN_0015646c) fails, reports (0x18, 10), sets state
PTR_DAT_00156474 = 0xF and resets the lever object pair to 1/0xFE. [I] spec 14 §6
#### 0x0015627A `park_mon_run10` — logic
Runs the ten park monitor steps (0x00156AA0, 0x00156B80, 0x00156CDA, 0x00156D5C, 0x00156E98,
0x00156F14, 0x00156F90, 0x0015700C, 0x00157088, 0x00157104, 0x001571C4, 0x00157254) each
wrapped in trace marks 0xC68452..0xC6845D; called from r08_app 0x0009B384. [O]
#### 0x00156332 `park_mon_run3` — logic
Second monitor chain (from 0x0009B39E): runs 0x00157270, 0x001572BE (+cluster twin 0x0015733A)
under marks 0xC6AB60..64 and, gated by PTR_FUN_00156498, the RAM-flag writer 0x0015643E. [O]
#### 0x0015639A `park_mirror_gate` — logic
Gate (callers=-): runs the 607-instr mirror compare 0x00156568 and the self-test 0x001573B6;
if the compare fails or the self-test reports 1, executes the failure report 0x001563D2;
wrapped in marks 0xC6D270/0xCE7390. [O]
#### 0x001563D2 `park_rpt18_ff` — logic
Mirror-mismatch path: when the calibration pointer at *(cfg+4)+0x50 != 0xDEAC, reports
(0x18, 0xFF), sets state PTR_DAT_00156474 = 0x11 and resets the lever object (1/0xFE pair,
0xFE twice on the second slot). [I] spec 14 §6
#### 0x0015643E `park_ram_flag_write` — logic
8-instr sequence of two indirect calls (0x000A78F2, 0x000A790C) plus 0x000A79C8 on RAM flag
DAT_fff8e551 (safety-layer RAM area). [I]
#### 0x001564AC `cal_get_mirror_chk` — library
Calibration-table fetch with redundancy: returns table[idx] (idx*4) from the config chain at
PTR_DAT_00156540, comparing against the mirror table behind DAT_00156544; on divergence sets
the caller's flag byte pair to 1/0xFE. 23 callers across the park/lever monitors. [O]
#### 0x001564D0 `park_chk_rpt18_0e` — logic
Gated report: when two getters (PTR_FUN_00156548/4c) differ and the second is non-zero,
reports (0x18, 0x0E), sets state PTR_DAT_00156554 = 0x10 and resets the lever object. [O]
#### 0x00156568 `park_mirror_compare` — logic
607-instr field-by-field redundancy compare of the park module's main struct
(PTR_DAT_001568c4, offsets +8..+0xCC) against its mirrored copies (0x001568c8/cc/d0/d4/d8/
dc/e0/e4/e8/ec and 0x00156b28-30): ~50 word/byte fields plus a 21-byte block compare at
puVar14[0x35]; interates gear index via 0x001573B6 self-test reads. Returns 1 only when
everything matches. The "computed twice and cross-checked" property of spec 14 §6. [O]
spec 14 §6
#### 0x00156AA0 `park_chk_cal0_fatal` — logic
Monitor step: runs six registered init/getter fns, cal slot 0 via 0x001564AC; when the
predicate (PTR_FUN_00156b70) triggers, reports (0x18, 0x11) and enters the fatal halt
0x001561F0. Mark 0xCE2572. [I]
#### 0x00156B80 `park_chk_cal123` — logic
Monitor step with three cal slots (1, 2, 3): predicates PTR_FUN_00156e58/74/7c; reports
(0x18, 0x16), (0x18, 1) and (0x18, 2), setting state 1/2/3 and resetting the lever object
each time. Mark 0xCE2573. [I]
#### 0x00156C1A `park_rpt18_2_variant` — logic
Direct report path (callers=-, pointer-table reached): reports (0x18, 2) and sets state 3
plus lever reset; then cal-gated second report (0x18, 2)/state 3 via 0x001564AC. Mark
0xCE2573. [I]
#### 0x00156C82 `park_rpt18_2b` — logic
Compact variant: single report (0x18, 2), state 3, lever reset, mark 0xCE2573; callers=-. [I]
#### 0x00156CDA `park_chk_cal4` — logic
Monitor step, cal slot 4 (predicate 0x00159256 via PTR_FUN_00156e8c): report (0x18, 3). [I]
#### 0x00156D5C `park_chk_cal5` — logic
102-instr monitor step, cal slot 5 (predicates 0x00159380/0x00159294): reports (0x18, 4) and
(0x18, 0x20). [I]
#### 0x00156E98 `park_chk_cal7` — logic
Cal slot 7 (predicate 0x001593FA): report (0x18, 5). [I]
#### 0x00156F14 `park_chk_cal8` — logic
Cal slot 8 (predicate 0x00159454): report (0x18, 6). [I]
#### 0x00156F90 `park_chk_cal9` — logic
Cal slot 9 (predicate 0x001594A0): report (0x18, 7). [I]
#### 0x0015700C `park_chk_cal10` — logic
Cal slot 10 (predicate 0x0015959C): report (0x18, 9). [I]

Batch 7 (clusters 151-175) — park monitor steps + park-target arbiter internals

#### 0x00157088 `park_chk_cal_b` — logic
Cal slot 0xB (predicate 0x00159616): report (0x18, 8), state 0xB, lever reset; mark 0xCE257A. [I]
#### 0x00157104 `park_chk_cal_c` — logic
Cal slot 0xC (predicate via PTR_FUN_001571bc): report (0x18, 0x1D), state 0x18, lever reset;
mark 0xCE257B. [I]
#### 0x001571C4 `park_chk_cal_d` — logic
Cal slot 0xD (predicate 0x00159194): report (0x18, 0x15), state 0xC; additionally runs the
getter-mismatch check 0x001564D0 when calibrated (cfg+4)+0x38; mark 0xCE257C. [I]
#### 0x00157254 `park_chain_tail` — logic
13-instr final step of the 0x0015627A chain: two registered calls (0x00158056, 0x00157478
chain) + mark 0xCE257D. [O]
#### 0x001572BE `park_chk_cal_10` — logic
Cal slot 0x10 (predicate 0x0015799C family): report (0x18, 0xB), state 0xD; mark 0xCE4C82.
Cluster of 2 — member 0x0015733A uses the sibling predicate 0x00157A24. [I]
#### 0x00157478 `park_snap_byte_pairs` — logic
Snapshots the self-test read 0x00157546 into param[2]/[3] plus two getter bytes (PTR_FUN_
001575ac/b0) with complements into param[0]/[1] and [6]/[7]. [O]
#### 0x0015749C `park_snap_plus_fn` — stub
7-instr: runs 0x00157478 then one registered function (DAT_001575b8); called from 0x0009B228. [O]
#### 0x001574AA `park_snap_inputs` — logic
Input/driver step of the park module (from r08_app 0x0009B384): mark 0xC68451, runs ~16
registered init/getter functions, computes the change byte (getter 0x001575ac vs snapshot)
stored with complement at +4/+5, closes with mark 0xCE2571. [O]
#### 0x00157608 `park_arb_load_cnt` — logic
Arbiter branch: when request A (0x00157820 self-test of PTR_DAT_00157808) == 7 and request B
== 1 or the qualifier byte at (cfg+0x88) is 0, loads the counter = qualifier+1 with
complements and expiry flag pair; returns 1. [I] spec 14 §4.2
#### 0x0015765A `park_arb_zero_cnt` — logic
Arbiter branch: request A == 7 and the block's own flag (param+3) == 1 → zero the counter
pair (0/0xFFFF flags) and return 1. [I] spec 14 §4.2
#### 0x0015768E `park_arb_change_detect` — logic
Change detect: two arbiter request bytes (PTR_DAT_00157818/1c) differ → flag pair 1/0xFE at
+8, else 0/0xFF; on change, resets the counter block via 0x001576D0. [O]
#### 0x001576D0 `park_arb_clr_cnt` — stub
9-instr reset of an 8-byte arbiter counter block (u16 0, complement 0xFFFF, flags 0/0xFF). [O]
#### 0x001576E2 `park_arbiter_main` — logic
103-instr park-target arbiter state machine (from 0x00156B80): reads the two request codes
(PTR_DAT_00157808/14) through the 0x00157820 self-test and dispatches — (7, not-6) branch via
0x00157608; (0, non-0) via 0x0015765A/0x001577B0; (0 or 0,0) decrements the u16 counter via
0x00157886 with expiry flags; the default resets via 0x001576D0 when both requests equal;
always finishes with the change detect 0x0015768E. [I] spec 14 §4.2
#### 0x001577B0 `park_arb_mismatch` — logic
Plausibility predicate for the arbiter: request B in {7, 5} with request A (PTR_DAT_00157818)
not matching, or B == 6 with A == 8 → mismatch = 1. [O] spec 14 §4.2
#### 0x00157886 `chk_word_selftest` — library
16-bit RAM self-test + counter read: writes 0x0000/0xFFFF/0xAAAA/0x5555 requiring read-back
(fail → PTR_FUN_001578f8), restores and checks the complement at param[1]; returns the u16
counter. Cluster of 7 copies (0x00157E28, 0x001585EE, 0x00158A44, 0x001590D6, 0x00159730,
0x0015A022). [O] spec 13 §2
#### 0x00157900 `park_snap_cfg98` — logic
Snapshots the byte at (cfg+0x98) twice into param[0..3] and param[8..0xB] with complements
and init flags; called from 0x00156202. [O]
#### 0x00157934 `park_decode_wrap` — logic
Wrapper: runs the range-request decode 0x00157948 and emits mark 0xCE4C80; called from
0x00156332. [O]
#### 0x00157948 `park_decode_request` — logic
Range-request decode into the block: the lever request fn (PTR_FUN_00157b30) values 0/5/6/7/8
(none/D/N/R/P) are stored with complement at +0x12/+0x13; anything else becomes 0xF/0xF0
(error). [O] spec 14 §5a
#### 0x0015798C `park_request_invalid` — logic
8-instr: self-test read (0x00157C42) of the decoded request at +0x12 == 0x0F (invalid). [O]
#### 0x0015799C `park_deb_gate_a` — logic
Debounce gate A (predicate of 0x001572BE): if the decoded request is 0, or the reference
(PTR_FUN_00157b38) is not 7/5 while the request equals it, or qualifier B != 1 / C != 0, or
request == 5, or D == 0 — takes the hold path 0x00157BA2; else the engage/decrement path
0x00157ADA. [I] spec 14 §4.2
#### 0x00157A24 `park_deb_gate_b` — logic
Debounce gate B (predicate of 0x0015733A): engage only when the reference is 8 with C == 0
(or 6) and the request block +0x12 reads 8; otherwise the hold path; operates the counter
pair at +8/+0xC. [I] spec 14 §4.2
#### 0x00157A90 `park_triple_mirror_chk` — logic
Compares three getter pairs (PTR_FUN_00157b30/34/50 vs PTR_PTR_00157b48/4c/54); returns 1
when any pair differs — three-value mirror check used by 0x00157104. [O]
#### 0x00157ADA `park_deb_engage` — logic
Engage/decrement step: twice decrements the counter block param_2 (self-test 0x00157C42),
reloads the result block param_3 from (cfg+0x98) with complements, and sets state +0x10 to
param_4 (or 3/0xFC when the counter expired); returns expired. [O] spec 14 §4.2
#### 0x00157BA2 `park_deb_hold` — logic
Hold counterpart of 0x00157ADA: decrements the other counter block (param_3) twice; when the
request counter (param_2) is still running and param_3 expired, reloads param_2 from
(cfg+0x98) and clears the state at +0x10 when it equals param_4. [O] spec 14 §4.2
#### 0x00157CB4 `park_release_flag` — logic
Combinatorial flag (from 0x00156AA0): sets param 1/0xFE when the six-way condition on the
arbiter bytes (PTR_DAT_00157e9c..0x00157eb0 = 6,1,≠2,1,1,1) holds, and clears to 0/0xFF when
the release condition (0, or 8, or (1 and ≠2), or 0) holds — the park-lock release/hold
output pair. [I] spec 14

Batch 8 (clusters 176-218) — park sub-steps + redundant safety layer (spec 14 §6)

#### 0x00157D50 `park_speed_window` — logic
Sets the in-window flag pair (+2 = 1/0xFE) when the u16 at PTR_DAT_00157eb8 (self-test read
0x00157E28) lies inside the calibrated band (cfg+8 … cfg+0xC), and clears it (0/0xFF) when the
request byte (PTR_FUN_00157ec0) is 8 or the qualifier conditions (0x00157DC2 on 0x00157eb4/
0x00157ea4) hold — the §4.2 vehicle-speed window of the park arbiter. [O] spec 14 §4.2
#### 0x00157ECC `park_snap_1byte` — logic
10-instr snapshot: one self-test byte (0x0015806A) from PTR_DAT_001580d0 into param[0]/[1]
with complement; called from 0x001574AA. [O]
#### 0x00157EE0 `park_snap_change_range` — logic
Snapshots the change byte (+2/+3 = new value when the request moved) and copies a valid range
code (8/7/6/5) from the lever object at PTR_DAT_001580d0 into +0xA/+0xB with complement. [O]
#### 0x00157F30 `park_range_out` — logic
101-instr range-output combiner: builds the output code pair at +6/+7 — priority handling for
8 (P), 7/5/6 passthrough with complements, and special codes 9/0xA/0xB gated by presence
booleans of the request in both blocks; consults the interlock 0x00157FFA and self-test reads
of +2/+0xA. [I] spec 14 §4.2
#### 0x00157FFA `park_range_interlock` — logic
Interlock predicate: when the qualifier at PTR_DAT_001580dc is 1 and the request (+2
self-test) is 6/7/5, forces the output pair (+6/+7) to 0/0xFF and returns 1. [O]
#### 0x00158044 `park_snap_flag_byte` — logic
9-instr: getter byte (PTR_FUN_001580e0 → 0x000A798E) into +8/+9 with complement; from
0x00156AA0. [O]
#### 0x00158056 `park_snap_req_byte` — logic
10-instr: self-test byte from PTR_DAT_001580d4 into +4/+5 with complement; called from the
chain tail 0x00157254. [O]
#### 0x001580EC `park_speed_band_sel` — logic
136-instr speed-band selector: branches on two request bytes (0x00158588 reads) — case (0)
copies the u16 (0x001585EE) to +0x16/+0x18 flag 0; case (8 + qualifier 0) and (6 + qualifier
0→third byte) copy a second u16 (PTR_DAT_00158340) with flag 1/0xFE; default flag 2/0xFD;
continues with further band comparisons. [I] spec 14 §4.2 (speed weighing)
#### 0x0015820C `park_deb_counter_step` — logic
101-instr counter/debounce step: decrements the u16 counter via 0x001585EE, and when the
secondary counter (+0xB block) lies between calibrated bounds (cfg+0x1C/+0x18) or the third
counter expired, reloads the counter from cfg+0x40 with fresh flags. [I] spec 14 §4.2
#### 0x001582E4 `park_qual_flag4` — logic
Four-way qualifier (reads 0x00158588 on PTR_DAT_00158348..54: 1,0,1,≠2) → flag pair at
+0xE/+0xF; called from 0x001574AA. [O]
#### 0x00158358 `park_speed_cond3` — logic
Three speed-derived conditions (state ≠2 with non-zero u16 counter; state 2 + qualifier 1 +
≠2; state 2 with qualifier 0-or-2 and counter above cfg+0x44) OR-ed into the flag pair at
+0x24/+0x25. [I] spec 14 §4.2
#### 0x00158444 `park_target_step` — logic
141-instr park-target step: snapshots the state byte (+0x10) to +0x12; when state == 0 and
the counter exceeds cfg+0x20 (unless state(+0x14)≠2), checks two chained sub-flags (+0xC, +0x22,
+0x24) and forces state 3/0xFC or propagates; heavy use of calibrated limits (cfg+0x14/+0x20). [I]
spec 14 §4.2
#### 0x00158668 `park_qual_init3` — logic
13-instr composite (from 0x0015749C): runs the qualifier 0x00158990, sets +6 = 1/0xFE, then
0x001588A0 and 0x0015895A on the DAT_001588b8 object. [O]
#### 0x00158682 `park_state_map` — logic
111-instr state mapping: snapshots a request byte (+0x14/+0x15), and with the qualifier at
PTR_DAT_001588c0 selects state +2 = 0/1/2 (complements 0xFF/0xFE/0xFD) — the 1-vs-2 split
depends on the qualifier and the cfg+0xBC byte; further transitions of +2 follow. [I]
#### 0x0015876A `park_eq_flags` — logic
Equality/qualifier flags: +0x16 = (two request bytes equal), +0x18 = (third byte == 2); sets
+6 = 1/0xFE when +0x18 is 1, and clears +6 again when +6==1, +0x18==0 and +0x16==1. [O]
#### 0x001587F4 `park_snap_word` — logic
10-instr: u16 self-test read (0x00158A44) of PTR_DAT_001588d8 into +8/+0xA with complement. [O]
#### 0x00158808 `park_speed_band2` — logic
Two-qualifier speed band: depending on the qualifier at PTR_DAT_001588dc (0 or 1) compares the
u16 counter (+8, via 0x00158A44) against calibrated pairs (cfg+0x5C/+0x60 or +0x54/+0x58)
producing band 0/1, else 2 (complements 0xFF/0xFE/0xFD). [O] spec 14 §4.2
#### 0x001588A0 `park_qual_flag_e` — logic
18-instr single-qualifier flag: byte at PTR_DAT_001588e0 → +0xE = 0/1 pair. [O]
#### 0x001588F0 `park_cond_1e` — logic
Compound condition: (qualifier ≠1 or second ≠0) and (u16 counter ≤ cfg+0x24 or third ≠0 or
fourth ==0) → +0x10 = 0/0xFF else 1/0xFE. [O]
#### 0x0015895A `park_qual_flag_12` — logic
24-instr two-qualifier flag (0 then 1) → +0x12 = 1/0xFE else 0/0xFF. [O]
#### 0x00158990 `park_speed_match9c` — logic
Flag +0x1A = 1/0xFE when the u16 at PTR_DAT_00158ad4 == 0x9C and the request byte
(PTR_DAT_00158ad8) is 0xB/6/7 — a specific speed value plus range combination. [O]
#### 0x00158B4E `park_snap5_getters` — logic
Snapshot of five getter bytes (PTR_FUN_00158d28..38) into five u32 slots (param[0..9]) each
with its bitwise complement; called from 0x00156AA0. The five fields mirror the lever object
of spec 14 §4.3. [O] spec 14 §4.3
#### 0x00158C30 `park_clr_lever_obj` — stub
7-instr reset used by 12 park predicates: zeroes the u32 pair at param and invokes the
lever-object reset (PTR_FUN_00158d4c = 0x000A7C00 chain). [O] spec 14 §6
#### 0x00158B9C `sl_verify_lever_defaults` — logic
Safety-layer step 1 (from 0x00158C3E): for each of the five lever-object getters
(PTR_FUN_00158d28..38) compares against the corresponding read of the snapshot helpers
(0x00158AE4/0x00158AF4/0x00158B04/0x00158B14/0x00158B24); on divergence writes the default
back through the setters (+0x00→0, +0x08→0, +0x10→0, +0x18→2, +0x20→0) — exactly spec 14 §6
step 1. [O] spec 14 §6
#### 0x00158C3E `sl_park_check_main` — logic
Safety-layer entry (from 0x00156202): runs the default-restore check 0x00158B9C, recomputes
the park target via 0x00158D86 and compares the recomputed result (self-test read 0x00159070)
against the application's target getter (PTR_FUN_00158d54) — returns mismatch. Spec 14 §6
step 2/3. [O] spec 14 §6
#### 0x00158C60 `sl_park_rule_a` — logic
104-instr park-target rule (from 0x00158D86): engage-park chain checking the calibrated
speed limit (cfg+0x80), the lever fields (0x00158B24 == 1, 0x00158AF4 == 5, 0x00158B04 == 1,
0x00158B34 == 1) and the interlock bytes at cfg+0xA8/+0xB0/+0xB4, emitting the target and
error codes (0xF9 = target 6). Spec 14 §6 "same rules as §4.2". [I] spec 14 §6
#### 0x00158D86 `sl_park_target_recalc` — logic
124-instr dispatcher of the safety-layer park-target recompute: short-circuits via
0x00158C60, applies the special conditions (cfg+0xC0/+0xC4 with lever fields and speed) that
force target 6 (0xF9), then selects one of five mode rules by the calibration byte cfg+0xB8
(0→0x00158E96, 1→0x00158ED6, 2→0x00158EFA, 3→0x00158F98, more→0x00158FF8). Spec 14 §6. [O]
spec 14 §6
#### 0x00158E96 `sl_park_rule_m0` — logic
Mode-0 rule: speed (cfg+0x10 limit vs counter 0x001590D6) and lever != 8 → target 6/0xF9,
else target 8/0xF7 (park). [O] spec 14 §6
#### 0x00158ED6 `sl_park_rule_m1` — logic
Mode-1 rule: interlock (0x00159070) == 1 → target 8/0xF7 else 6/0xF9. [O] spec 14 §6
#### 0x00158EFA `sl_park_rule_m2` — logic
Mode-2 rule: interlock 1, or (lever not in 7/6/5 and speed within limit and qualifiers 0/2 or
lever 8) → target 8, else 6. [O] spec 14 §6
#### 0x00158F98 `sl_park_rule_m3` — logic
Mode-3 rule: speed above limit → 6; else interlock 1 or lever in 7/6/5 → 8. [O] spec 14 §6
#### 0x00158FF8 `sl_park_rule_m4` — logic
Mode-4 rule: lever 8, or (speed within limit and interlock 1 and lever in 7/6/5) → target 8,
else 6. [O] spec 14 §6

#### 0x00159160 `park_call_registered` — stub
4-instr indirect call (*DAT_00159344)(DAT_00159340); called from 0x00156202. [O]
#### 0x00159168 `park_deb_step_591` — logic
Debounce step: u16 counter read via self-test 0x00159730, decrement with expiry flag at +2
(complement at +5); called from 0x00156AA0. [O]
#### 0x00159194 `park_pred_1d` — logic
12-instr predicate: two getters differ (PTR_FUN_00159348 vs PTR_PTR_0015934c); used by the
cal-0xD monitor step 0x001571C4. [O]
#### 0x001591AC `park_add_cfg9c` — logic
Adds the calibrated byte (cfg+0x9C) to the u16 counter pair at +6/+8 with overflow → −1
(0xFFFF), complement maintained. [O]
#### 0x001591E8 `park_pred_cal4` — logic
Predicate of the cal-4 step: lever in 5/7/6 with qualifier 0 → runs the additive step
0x001591AC and reloads via 0x00159730; second lever block == 8 (first ≠ 8) → zero the counter
pair. [I] spec 14 §4.2
#### 0x00159256 `park_pred_cal4b` — logic
Second predicate of cal-4: lever 6 with second block 8 and expired counter (0x00159730 == 0)
→ fires the lever-object reset (PTR_FUN_00159368) and returns 1. [O]
#### 0x00159294 `park_pred_cal5a` — logic
84-instr first predicate of cal-5 (0x00156D5C): with the enable byte at cfg(+0xD4)+0xE —
lever in 5/7 (bVar1) or 6 with qualifier and non-zero ptr (bVar2); requires the reference
getter == 8 or five qualifier bytes all zero; fires the reset (PTR_FUN_00159368) when both
hold. [I]
#### 0x00159380 `park_pred_cal5b` — logic
Second predicate of cal-5: enable cfg(+0xD4)+0xB == 1, lever in 5/7/6, second byte == 1 while
block +0xA ≠ 1, plus two zero qualifiers → fires the reset (PTR_FUN_00159584) and latches the
byte pair +0xA/+0xB. [I]
#### 0x001593FA `park_pred_cal7b` — logic
Predicate of cal-7: min(cfg+0x5C, cfg+0x54) window vs the counter 0x00159730; lever == 8 and
second lever block == 8 and qualifier == 0 → reset fired. [I]
#### 0x00159454 `park_pred_cal8b` — logic
Predicate of cal-8: lever not 6/8, second block in 7/5, third == 6 → fires the reset
(PTR_FUN_00159584). [I]

#### 0x00157270 `park_chk_cal_f` — logic
Cal slot 0xF (predicate PTR_FUN_0015745c): report (0x18, 0x12) and latch the event state
(PTR_FUN_00157460); mark 0xCE4C81. Cluster of 1. [I]

## Unit summary

220 clusters, 250 named functions (all members named; multi-member clusters are duplicated
helper copies of the RAM self-test pattern `chk_byte_selftest` ×14 and `chk_word_selftest` ×7,
the `lib_min/max` pairs and the scheduler thunk triples).

Counts by kind: logic 167, library 40, data-accessor 26, stub 16, dispatch 1, irq 0, entry 0,
unknown 0. The zero-unknown result reflects heavy spec anchoring rather than over-claiming:
the slice is dominated by three well-documented blocks —

1. **0x152CB6-0x153A5x — "0x152xxx module block"** (units 43-45 context): ~19 registered
   conditioner sub-blocks with inverted-copy integrity, event-0x32 reporting, RAM write tests
   and a 16-entry code search; driven per-cycle by 0x0015387E / reset by 0x00153910. The exact
   module identity remains open (its 0x7FF/10000 constants suggest an actuator ramp/position
   context) — names use the honest `blk_` prefix. Lead for units covering 0x1518xx-0x152Bxx.
2. **0x153A5x-0x1555xx — measurement protocol** (spec 10): every command handler, the 0xE0
   periodic-list engine, index→address maps, permission gate 0x1554BC and the 2-byte bitmap.
   Cross-check: r08_app 0x0009B228/0x0009B306/0x0009B39E/0x0009B546/0x0009B384/0x0009B39E
   schedule this module; 0x135768 gates it on calibration 0x195D6E.
3. **0x1555B4-0x159454 — lever/range + park module with the redundant safety layer**
   (spec 14): ASCII lever decode 0x155F8A, P1614 monitor family (fault group 35), park-target
   arbiter + debounce engine (fault group 0x18=24), and the §6 safety layer
   0x158C3E/0x158B9C/0x158D86/0x158C60 with its five mode rules. Cross-check: lever-object
   getters/setters 0xA51E4-0xA5448 and arbiters 0xA56D8/0xC7C0C live in r08_app units; the
   0x158B9C default-restore values (+0/+8/+0/+2/+0) match spec 14 §6 exactly.

Cross-region leads:
- The empty 4-instr scheduler slot stubs (0x153406-0x153466, 0x151xxx-0x152xxx) cluster into
  unit 1's 131-member `FUN_0010 8dc` family — unit 1 should note their role here.
- Report-pair helpers with the 17000000+/17999999− data constants (0x154FF8, 0x155030,
  0x155054) — the meaning of those bases is unresolved; spec 13 owners may decode them.
- `mon_ratio_windows` 0x1549CA is the largest unanchored monitor; its two threshold windows
  (cfg +0x1C..0x28 / +0x30..0x3C) deserve a fault-groups cross-check.
