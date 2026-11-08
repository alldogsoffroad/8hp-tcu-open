# Unit 07 (r04_boot)

#### 0x0006A7F6 `boot_ram_complement_check` — logic
Boot-time RAM sanity check: with interrupts masked (IPL 0xE via 0x0009665C/0x00096674), verifies
DAT_fff90ab8 == ~DAT_fff844a4 (value/complement pair). On mismatch or failed re-write it invokes the
error logger PTR_FUN_0006a860(0x2fd, &DAT_fff90ab8, ...). Called only from boot init FUN_0007cb2a.
[O] for the check mechanics, [I] that 0x2fd is a boot DTC/event number.

#### 0x0006A864 `eeprom_get_flags_9380a` — data-accessor
Returns DAT_fff9380a (flags word of the EEPROM-record engine; caller FUN_00079764 tests bit 0x200
to gate a condition used by ~16 callers). Pure getter. [O]

#### 0x0006A86C `eeprom_state_machine_a` — logic
Scheduler-invoked state machine (callers=-) on DAT_fff91b9e (states 1..8) over the session struct
DAT_fff91b88: 15 s timeout (DAT_fff90628) raises event 0x200 and state 8; state 2 consumes result
codes from PTR_FUN_0006ab00 (1/4/5 -> events 0x1000/0x2000/1); states 3-6 drive PTR_FUN_0006aafc
with op codes 1/2/5/6; final states 7/8 clear status bits in *DAT_fff91be4 and run a verify via
PTR_FUN_0006ab08. Updates result latch via FUN_0006aac8. Companion of FUN_0006ab8e. [I] on the
erase/program split; the state/graph itself is [O].

#### 0x0006AAC8 `eeprom_update_result_latch` — logic
Copies DAT_fff91bc0 into result mirror DAT_fff93b68, ORing 0x80 when session flag bit
(*DAT_fff91be4+1)&2 is set. Called by both state machines FUN_0006a86c/FUN_0006ab8e. [O]

#### 0x0006AAE4 `eeprom_get_result_3b68` — data-accessor
Returns DAT_fff93b68 (latched EEPROM-engine result byte written by FUN_0006aac8). Caller 0x0007A130.
[O]

#### 0x0006AB10 `eeprom_write_cells_retry` — logic
Retry loop of the record-write engine: clamps step count param_1 to 3, computes target
DAT_fff9381a+step (+1 if flag 0x20 set), then calls writer PTR_FUN_0006acb4 up to (target-
DAT_fff9381c) times counting successes; on progress updates DAT_fff9381c and sets status bit 1.
Wrapped in lock/unlock PTR_FUN_0006acb0/0006acb8. Callers 0x00072A22, 0x00079792. [O]

#### 0x0006AB8E `eeprom_state_machine_b` — logic
Main EEPROM-record write engine (callers=-, scheduler task), 749 instrs, state DAT_fff91b9f
(0..0x1b) over session DAT_fff91b88: abort path when DAT_fff91b9d==1/DAT_fff93b7b==1 (event 0x800),
record-table walk via DAT_fff91bb4/DAT_fff91bb8, per-record writes PTR_FUN_0006b124(idx,...) with
table DAT_fff91ba3[] and retry counter DAT_fff80d12 (event 0x10), 15 s timeout -> event 0x200/
state 0x1a, session-invalid -> event 0x400/state 0x1b, completion states 0x15..0x1b rewrite status
bits in *DAT_fff91be4 and restore counters. Cross-check spec 18 data-flash loader records model.
[O] structure, [I] that this is the EEPROM/data-flash record writer.

#### 0x0006B408 `eeprom_count_pending_ops` — logic
Computes pending-work count: (DAT_fff9381c - DAT_fff9381a) +1 if flag 4, +1 if flag 0x20, +1 if
mode (PTR_FUN_0006b51c) is 1/4, plus one per entry of the record table DAT_fff91bb4[0..DAT_fff91bb8)
whose length field !=0 and entry flags &0x10. Used by callers 0x00072A22/0x0007291A/0x000679D4 to
budget time. [O]

#### 0x0006B470 `eeprom_request_abort` — data-accessor
Sets DAT_fff91b9d = 1; FUN_0006ab8e's main loop reads this flag to abort/clean up an active record
write. Caller 0x00072898. [O]

#### 0x0006B47A `eeprom_get_queue_status` — data-accessor
Status getter for the write engine: 1 if DAT_fff9381c != DAT_fff9381a (writes pending), 3 if
DAT_fff9380c & 0xe3e (error bits), 0 if both counters are 0 (idle), else 2. Callers=- (pointer
table). [O]

#### 0x0006B4B4 `eeprom_set_retry_lt3_flag` — logic
If retry counter DAT_fff80d12 < 3, sets status bit 4 in *DAT_fff91be4. Part of the write engine's
degraded-mode handling. Callers=- (pointer table). [O]

#### 0x0006B4CE `eeprom_session_op_thunk` — stub
Tail wrapper: forwards param_1 to PTR_FUN_0006b520(param_1, &DAT_fff91b88) — single-argument
control op on the EEPROM session struct. Callers 0x0007A26E/0x0007A130. [O]

#### 0x0006B524 `boot_ram_pattern_verify_ad04` — logic
Boot RAM read/write verify of the 4-short table at DAT_ffffad04 plus word DAT_ffffad00: with
interrupts masked, sets DAT_fffc1400 bit 1, writes pattern (source param_1, param_2), reads back
and compares. Returns ok flag. Called from boot checks 0x0006884E/0x000689B4. [O] mechanics,
[I] purpose (boot self-test).

#### 0x0006B5E0 `boot_ram_shadow_compare` — logic
Compares int block [PTR_DAT_0006b610 .. PTR_DAT_0006b614] word-by-word against its image at
+0x480000 (RAM mirror/shadow), stopping at first mismatch. Returns 1 when identical. Called from
boot checks 0x000689B4/0x00068ADA. [O]

#### 0x0006BC2C `lib_ptr_thunk` — stub
Single-result tail-call thunk: loads target from an adjacent literal pointer (PTR_FUN_0006bc88 ->
0x000728F6) and jumps. Cluster of 35 byte-identical veneers spread across the image (0x0006BC2C,
0x0006BC38, 0x00073F6A, 0x00073F7C, 0x00073F88, 0x000794D8, 0x00082EA8, 0x0009B594, 0x0009B708,
0x0009B714, 0x0009B72C, 0x0009B738, 0x000C88FC, 0x000EAD9A, 0x000F82C8, 0x000F82D4, 0x000F82E0,
0x000F82FE, 0x000F830A, 0x000F8316, 0x000F8322, 0x000F832E, 0x000F85D0, 0x000F85E8, 0x000F85F4,
0x000F8600, 0x000F8650, 0x000F865C, 0x001277B6, 0x001277C2, 0x00128888, 0x00128BEC, 0x00128BF8,
0x00128C3C, 0x00128C48) — each wraps a different target via its own pointer slot. [O]

#### 0x0006BC44 `eeprom_chk_state_0f` — data-accessor
Returns DAT_fff93b81 == 0x0f (EEPROM-record engine module state check). Caller 0x0006F0FC. [O]

#### 0x0006BC50 `eeprom_get_3b8c` — data-accessor
Returns DAT_fff93b8c. Caller 0x0006F144. Part of the flag block at DAT_fff93b8x. [O]

#### 0x0006BC58 `eeprom_get_3b8b` — data-accessor
Returns DAT_fff93b8b. Caller 0x0006F144. [O]

#### 0x0006BC60 `eeprom_chk_flag_3b8f` — data-accessor
Returns DAT_fff93b8f == 1; FUN_00072a22 sets DAT_fff93b8f=1 when finishing its sub-sequence.
Caller 0x0006F0FC. [O]

#### 0x0006BC6C `eeprom_set_flag_3b8e` — data-accessor
Sets DAT_fff93b8e = 1. Callers=- (reached via pointer table). [O]

#### 0x0006BC76 `eeprom_set_3b8d` — data-accessor
Stores param_1 into DAT_fff93b8d. Callers 0x0008316C/0x00068830. [O]

#### 0x0006BC7E `eeprom_set_3b90` — data-accessor
Stores param_1 into DAT_fff93b90. Caller 0x0007ACF2. [O]

#### 0x0006BC90 `eeprom_register_tables` — logic
Init-style registration (callers=-, vector/pointer table): calls FUN_0006beb6 then thunk
FUN_0006beb0(&DAT_fff94e8c, &DAT_fff95013, &DAT_fff94d04) — hands three RAM work areas
(0xfff94e8c/0xfff95013/0xfff94d04) to the record engine. [O] call shape, [I] registration purpose.

#### 0x0006BD48 `lib_diverse_write_2b_locked` — logic
Writes two bytes (*param_1=param_3, *param_2=param_4) with interrupts masked
(PTR_FUN_0006be9c/0006bea0 = IPL save/set). Half of the value/complement safe-RAM store API used
with FUN_0006bd7a. [O]

#### 0x0006BD68 `lib_diverse_write_2b` — stub
Plain two-byte diverse store (*p1=v1, *p2=v2), no locking — the unlocked variant used by ~29
callers (fault memory and measurement writers). [O]

#### 0x0006BD6E `lib_diverse_write_2w` — stub
Plain two-16-bit diverse store pair (*p1=v1, *p2=v2), unlocked. 16 callers across fault/measurement
writers. [O]

#### 0x0006BD74 `lib_diverse_write_2dw` — stub
Plain two-32-bit diverse store pair. Callers=- (pointer table). [O]

#### 0x0006BD7A `lib_diverse_read_2b_locked_chk` — logic
Locked diverse read: under IPL mask, reads byte pair and checks *param_1 == ~*param_2; on mismatch
calls FUN_0006becc(...,1) history log, stores offending address in DAT_fff90998 and raises event
0x221 via PTR_FUN_0006be98. Returns first byte. Caller 0x00085160. [O]

#### 0x0006BDCA `lib_diverse_read_1b_chk` — logic
Unlocked variant of the complement-checked byte read (error 0x221, history via FUN_0006becc).
~34 callers, e.g. 0x0008E5E8/0x0006BF9C/0x0006C1EA. [O]

#### 0x0006BDFE `lib_diverse_read_2s_chk` — logic
Complement-checked 16-bit read (mismatch -> FUN_0006becc(...,2), DAT_fff90998=addr, event 0x221).
~20 callers. [O]

#### 0x0006BE34 `lib_diverse_read_4b_chk` — logic
Complement-checked 32-bit read (error path identical, event 0x221). 4 callers. [O]

#### 0x0006BE60 `boot_ram_march_test` — logic
Boot RAM test driver: brackets two FUN_0006bf12 range tests with PFC checkpoint 0x00072AE0(0xa1d5b0). Ranges covered: DAT_fff94e8c..fff95013 and DAT_fff94d04..fff94e8b.
[O] shape, [I] that 0xa1d5b0 is a service key.

#### 0x0006BE90 `lib_fill_complement_thunk` — stub
Thunk copy of FUN_0006beb0: dst[i] = ~src[i] over [param_1..param_2]. Reached only from
FUN_0006bc90. [O]

#### 0x0006BEB0 `lib_fill_complement` — library
Complement-fill loop dst[i] = ~src[i] for param_1 <= param_2; used to build the mirror image of a
RAM block (callers=-, pointer/shared with the thunk). [O]

#### 0x0006BEB6 `eeprom_reset_errhist_idx` — logic
If DAT_fff80d06 != 0x1d, sets ring index DAT_fff80d13 = 0xff (0-entry marker) — resets the diverse-
RAM error history (arrays filled by FUN_0006becc) when the module id differs. Called from
FUN_0006bc90. [O]

#### 0x0006BECC `lib_log_diverse_error` — logic
Logs one diverse-RAM failure into a 4-entry ring: index DAT_fff80d13 (wraps 4->0), fields into
arrays DAT_fff80c98/fff80c84/fff80cc0/fff80cac (+type byte DAT_fff80d14). Called by all
lib_diverse_read_*_chk variants and FUN_0006bf12. [O]

#### 0x0006BF12 `boot_ram_march_range` — logic
March-tests a RAM range in 0x80-byte steps via PTR_FUN_0006bf94(block,0x10) (special-cases the top
block at DAT_fff9ff80); on failure logs type 0x20, stores address in DAT_fff90998, raises 0x221.
Called from FUN_0006be60. [O]

#### 0x0006BF9C `park_pwm_begin_decode` — logic
Starts a PWM-coded value exchange: decodes initial value via PTR_FUN_0006c234(&DAT_fff94fe7,
&DAT_fff94e5f,param_3); classifies result 0x1e/0x19/0x07 against period thresholds
(DAT_fff9383e/40/42/44 over DAT_fffff220-DAT_fff9240c) adjusting candidate DAT_fff93ba2 by +/-1/2
(clamped 0..7, flags DAT_fff93ba5 0/1/4, status DAT_fff93ba7 |=1/2), then tail-shares FUN_0006c0ba
to program the timer. Error family 0x221/0x226. Belongs to the lever/range PWM protocol (spec 14).
[O] mechanics, [I] protocol role.

#### 0x0006C0BA `park_pwm_program_timer` — logic
Common tail of the PWM protocol: sets decode marker DAT_fff93b9b=7, deadline DAT_fff92410 =
DAT_fff93846 + measured value, programs output-compare DAT_fffff5b0 = delay<<8 (min 5), sets
DAT_fffff585 bit 0, latches DAT_fff9092a=1 / DAT_fffff58c/e, stores delay in DAT_fff90864, and
locks/unlocks around it (PTR_FUN_0009125c/00091260). [O]

#### 0x0006C12C `park_pwm_init` — logic
Init: period base DAT_fff93848=0x3138, code masks DAT_fff94fe8=0x3f/DAT_fff94e60=0xc0 (+zeroed
complements), state vars DAT_fff93ba2/DAT_fff93ba1=7, time base DAT_fff9240c=DAT_fffff220,
diversity pair DAT_fff93b48/49 = ~DAT_fff9054d/c, computes thresholds (FUN_0006c594) and dead time
DAT_fff93846 = max(ptr,ptr)+0xbb. Caller 0x00087732. [O]

#### 0x0006C1BA `park_pwm_service` — logic
Periodic service: PTR_FUN_0006c244 tick; if idle marker DAT_fff93b9b==0 forwards DAT_fff92414 via
PTR_FUN_0006c248, else decodes current pair via PTR_FUN_0006c234(&DAT_fff94fe8,&DAT_fff94e60) and
passes result to PTR_FUN_0006c24c. Callers=- (task table). [O]

#### 0x0006C1EA `park_monitor_feedback` — logic
253-instr monitor of the PWM feedback pair DAT_fff9054d/c and raw decode DAT_fff90550: tracks
candidate position DAT_fff93ba2 vs decoded DAT_fff93ba1 (0..7, nibble flags DAT_fff90551&5),
debounce counters DAT_fff9383c/DAT_fff93ba0/DAT_fff93b9e/DAT_fff93b9f, sets event bits in
DAT_fff94f84 / clears DAT_fff94dfc / marks DAT_fff9384c; verifies echoed code via PTR_FUN_0006c4e8
(mismatch -> DAT_fff93ba7 |=0x20/0x40); final diversity check raises event 0x226 at site 0x975f0.
Called from FUN_0006c63e. [O]

#### 0x0006C48C `park_pwm_learn_value` — logic
Learn/program path (idle only): rejects period param_1 >= 0x3139 with 2; computes 6-bit code
(param_1*0x148>>16), stores code+complement via PTR_FUN_0006c4f4 or manually into
DAT_fff94fe9/DAT_fff94e61, sets substate DAT_fff93b9c (0x19/0x07), kicks FUN_0006c0ba; verifies
code*200 against param_2 (mismatch -> event 0x226, site 0x975ee). Callers 0x000873F0/0x0008799C. [O]

#### 0x0006C542 `park_pwm_period_tick` — logic
Time tick: when DAT_fffff220-DAT_fff9240c >= DAT_fff93848+0x960, advances the base by one period,
increments candidate position DAT_fff93ba1/DAT_fff93ba2 (max 7) and steps the diversity counters
DAT_fff94fe6++/DAT_fff94e5e--. Called from FUN_0006c63e. [O]

#### 0x0006C594 `park_pwm_calc_thresholds` — logic
Fixed-point threshold derivation from period base DAT_fff93848: DAT_fff9383e = base+0x47a,
DAT_fff93840/42/44 via 0x79b8/0x8688/0x7ed8/0x8168 multiplier chains. Called from init and echo
confirm. [O]

#### 0x0006C612 `park_store_request_state` — logic
Conditional request store: if param==0 calls PTR_FUN_0006c784, then gates on PTR_FUN_0006c788()
before storing DAT_fff9094a = (param==0) and DAT_fff90953 = param. Callers 0x000873F0/0x00072898.
[O] shape, [I] on meaning (request flags).

#### 0x0006C63E `park_pwm_task` — logic
Main periodic task (caller 0x000873F0): state DAT_fff93ba3 — 7: sample via FUN_0006c1ea (schedule
next by DAT_fff92410); 0x19: tick + monitor; else tick only; then re-arms through FUN_0006c742.
Tail: when decoded position DAT_fff93ba1 >= 6 and DAT_fff90954 set, asserts DAT_fff800e8=1, marks
DAT_fff9384c |=0x2000 and (after 2 cycles) DAT_fff94f84 |=0x2000. [O]

#### 0x0006C720 `park_pwm_req_restart` — logic
Returns active flag DAT_fff93ba3 != 0; when idle, restarts the exchange: state 7, marker
DAT_fff93ba4=0x19, FUN_0006c0ba. Callers 0x0008D020/0x0008CEDC. [O]

#### 0x0006C742 `park_pwm_confirm_echo` — logic
Echo handshake: in substate 0x19 verifies stored code (DAT_fff94fe8/0x94e60) against echo
DAT_fff9054f with retry counter DAT_fff93b9d (mismatch -> DAT_fff93ba7 |=4), updates period base
DAT_fff93848 = code*200 and recomputes thresholds; flips substate via PTR_FUN_0006c830; diversity
check -> event 0x226 at 0x975ef. Returns substate==0x19. [O]

#### 0x0006C802 `park_pwm_req_start` — logic
Request entry: returns active flag; when idle stores value/complement (DAT_fff94fe7=~param_2,
DAT_fff94e5f=param_2... note stored pair) and payload DAT_fff92414=param_1, then starts via
FUN_0006bf9c. Callers 0x0008D020/0x0008CEDC. [O]

#### 0x0006C840 `diag_cmd_step_selector` — logic
Callers=- (task table): polls flag via thunk 0x00073F6A (-> getter FUN_0006c878), runs one of
FUN_00075f72/FUN_00075f86 by that flag, and on zero result invokes cleanup/report
PTR_FUN_0007603c(0) + FUN_00076004. Part of the diag-scheduler module at 0x00073xxx-0x00075xxx.
[O] shape, [I] module role.

#### 0x0006C868 `diag_get_3b4b` — data-accessor
Returns DAT_fff93b4b. Caller 0x000732F8 (diag scheduler loop). [O]

#### 0x0006C870 `diag_set_3b4c` — data-accessor
Stores param into DAT_fff93b4c (selector also read back via FUN_0006c878/thunks). [O]

#### 0x0006C878 `diag_get_3b4c` — data-accessor
Returns DAT_fff93b4c. Callers 0x00073F6A/0x000734FE/0x000732F8. [O]

#### 0x0006C880 `diag_get_911a4` — data-accessor
Returns 32-bit DAT_fff911a4. Caller 0x000732F8. [O]

#### 0x0006C888 `diag_set_active_bit` — logic
Encodes selection: DAT_fff911a8 = 1<<(param&0x1f) (0 if param==0) and stores the raw selector in
DAT_fff93b4a; pair-tested by FUN_0006c8a4. Callers 0x00073708/0x000734AE. [O]

#### 0x0006C89C `diag_get_3b4a` — data-accessor
Returns DAT_fff93b4a (last selector). Callers 0x000734FE/0x00073F7C. [O]

#### 0x0006C8A4 `diag_test_active_bit` — data-accessor
Returns (DAT_fff911a8 & param_1) != 0 — bit-test companion of FUN_0006c888. Caller 0x00073F88. [O]

#### 0x0006C8B0 `diag_set_3b4d` — data-accessor
Stores param into DAT_fff93b4d. Callers 0x0007FFB4/0x000735E4/0x0007310E. [O]

#### 0x0006C8B8 `diag_get_3b4d` — data-accessor
Returns DAT_fff93b4d. Caller 0x000734FE. [O]

#### 0x0006C8C0 `diag_set_3b4e` — data-accessor
Stores param into DAT_fff93b4e. Callers 0x0007FFB4/0x000735E4/0x0007310E. [O]

#### 0x0006C8C8 `diag_get_3b4e` — data-accessor
Returns DAT_fff93b4e. Callers 0x00073E92/0x00073E6C/0x000734FE/0x000732F8. [O]

#### 0x0006C8D0 `diag_apply_sel_800ab` — logic
Applies stored config: FUN_0006c870(DAT_fff800ab) and clears DAT_fff911a4=0. Caller 0x000734AE. [O]

#### 0x0006C8E6 `diag_get_911c0` — data-accessor
Returns 32-bit DAT_fff911c0. Caller 0x00073FBE. [O]

#### 0x0006CA44 `diag_install_hooks` — logic
Registration shim (callers=-, init table): installs a callback (PTR_LAB_0006cac0) and an
argument/func pair (PTR_PTR_0006cac8) via three function pointers, then kicks
PTR_FUN_0006cad0. [O] shape only.

#### 0x0006CA58 `diag_read_sel_byte_to_buf` — logic
Value readout for the scheduler: picks byte at DAT_fff92484+7/8/9 by selector param_1 (1/3/other),
writes it into response struct param_2 (1 or 2 bytes; 2-byte text mode when flag byte at +0xb ==
0x80 and short at +6 != 0), sets length at param_2[2] and type 2 at +10. Caller 0x000732F8. [O]

#### 0x0006CAAE `diag_set_3b50` — data-accessor
Stores param into DAT_fff93b50. Caller 0x000732F8. [O]

#### 0x0006CAB6 `diag_get_3b50` — data-accessor
Returns DAT_fff93b50. Caller 0x0007627C. [O]

#### 0x0006CAD4 `diag_register_handlers` — logic
Init registration (callers=-): chains 8 pointer calls; resolved callees 0x00075DB0, 0x00073F9A,
0x000741B6, 0x000760B8, 0x000740D4, 0x00074AA8, 0x00075BDC, 0x00074A88 — installs the diag/
service handlers of the 0x73xxx-0x75xxx module. [O] shape, [I] purpose.

#### 0x0006CAF8 `diag_register_handlers_b` — logic
Smaller init registration (callers=-): 3 calls resolving to 0x00074A04/0x00074C20/0x00074B0C. [O]

#### 0x0006CBDA `meas_read_entry_to_buf` — logic
Measurement/DID readout: from request struct param_1 (index *param_1, mode byte at +0x1c), reads a
u32 descriptor row via PTR_PTR_0006cd2c; fetches the value through PTR_FUN_0006cd38 (result 0 ->
range-lookup in the 0xc-byte-stride table from FUN_0006cda6 and per-entry conversion via
PTR_PTR_0006cd3c table; 1 -> raw copy via thunk 0x0007D1B4 into staging &DAT_fff84906), then
bounds-checked copy of (row+1) lo/hi length bytes from staging into the response buffer with
running length at param_1+0x12 vs max at +0x14. Statuses 0/1/3/5/9. Called from 0x00132142 (app
measurement protocol, spec 10). [O]

#### 0x0006CDA6 `meas_get_table` — data-accessor
Returns the active measurement-descriptor table pointer from PTR_PTR_0006cdbc (0 if null) and its
entry count at +0x14 into param_1. Callers 0x00074532/0x0006CBDA. [O]

#### 0x0006CF44 `diag_send_padded_resp` — logic
Pads a response to 8 bytes (fills param_2+param_3..8 with param_6 when param_4 set), then hands it
to (*DAT_0006cfc8)(param_1,param_5,len,ptr) — transmit/send wrapper. Callers 0x00075788/0x00075918.
[O] shape, [I] direction.

#### 0x0006CF7E `proto_match_3e80_header` — logic
Returns 1 when param_2==2, param_3 starts with '>'(0x3E) 0x80 and the entry selected by
DAT_fff800b0 from PTR_PTR_0006cfcc matches param_1; then invokes PTR_FUN_0006cfd0. Callers=-.
[O] shape; protocol meaning [I]/unknown — plausibly a bench/diag response matcher.

#### 0x0006CFD4 `sol_set_group_values` — logic
Fills the 3x8 output-value matrix DAT_fff92520[r][i] = param_2 for every row r selected by
param_1 bits and every bit i set in param_3, then sets state DAT_fff93bd3=3 and triggers the
apply/commit FUN_0006d094. Matches the solenoid/SPI-driver channel staging (spec 12: 8 channels
per chip, 3 chips). Callers 0x000821A4/0x000821BE. [O] matrix mechanics, [I] solenoid linkage.

#### 0x0006D01E `sol_set_group_mask` — logic
Like FUN_0006cfd4 but targets the 3x8 mask matrix DAT_fff92508[r][i] (rows by param_1 bits, cells
by param_3 bits -> param_2), then commits via FUN_0006d094 and afterwards scans DAT_fff92508 for
any bit0 set -> DAT_fff93bd2=1. Caller FUN_0006d184. [O] matrix mechanics, [I] solenoid linkage.

#### 0x0006D094 `sol_commit_outputs` — logic
Commit path for the output matrices: under lock (FUN_0009a428) computes
DAT_fff92508|DAT_fff92520 per cell, compares against last-sent DAT_fff92538, and pushes changes
via 0x00094BDC/0x0007C410; gated by state DAT_fff93bd3 (==2 blocks) and a frame/index check via
PTR_PTR_0006d1a0/1a4[DAT_fff90ab8]. Callers 0x0006CFD4/0x0006D01E. Spec 12 solenoid chain. [O]
shape, [I] peripheral target.

#### 0x0006D184 `sol_set_all_group_masks` — logic
Wrapper: FUN_0006d01e(0xff,param_1,param_2) — applies mask value to all 3 groups, returns 1.
Callers 0x00081A18/0x00081A20. [O]

#### 0x0006D1B0 `spi_monitor_pair_3b52` — logic
Two-channel monitor: refreshes fault evals (FUN_0006d3a4/FUN_0006d3c4), polls registers via
PTR_FUN_0006d3e4 with codes 0x6f/0x00/0x70/0x89 (accept only 1/2 results), sets channel-active
flags DAT_fff93bd6/DAT_fff93bd7 from status bytes DAT_fff93b53/DAT_fff93b52 bits 8+0x10, and on
success writes config 0x7f through PTR_FUN_0006d3ec (0x7f,en,flag,1) and complement form via
PTR_FUN_0006d3f0 (0x7f,v,~v,0xfe). Called from FUN_0006e83c (init). [O] flow, [I] SPI-device
identity (spec 03/05 supervisor question).

#### 0x0006D312 `spi_check_cfg_mirror` — logic
Builds complement mirror DAT_fff90f4c[i..7] = ~DAT_fff90f44[i..7], then compares copies
DAT_fff90f59 vs 49 and high nibble 58 vs 48; sets DAT_fff93b53 bit 0x80 on match and bit 0x40
when DAT_fff90f5e != DAT_fff90f4e — consistency flags for an 0x90f44 config block. Callers=-
(task table). [O]

#### 0x0006D3A4 `spi_eval_fault_3b52` — logic
If DAT_fff93b52 has bit 0x20 and gate DAT_fff84434==0, sets its bit 0x10 (channel fault latch).
[O]

#### 0x0006D3C4 `spi_eval_fault_3b53` — logic
Same latch for DAT_fff93b53 with gate DAT_fff8448c. [O]

#### 0x0006D3F4 `spi_monitor_pair_3b54` — logic
Debounced monitor of status pair DAT_fff93b54/DAT_fff93b55: refresh FUN_0006d6a0/FUN_0006d6c0,
validity via PTR_FUN_0006d688 (0x6f/0x88) + PTR_FUN_0006d68c()==2; when both-fault (bits 8&0x10)
increments saturating counters DAT_fff93bd9 (sel DAT_fff90ecc>>5==1) / DAT_fff93bda else
decrements; thresholds from PTR_PTR_0006d690/694[DAT_fff90ab8] set DAT_fff93bd8; then writes
0x78 register pair PTR_FUN_0006d698/69c (0x78,1,~flag,0xfe). Caller FUN_0006e83c. [O]

#### 0x0006D6A0 `spi_eval_fault_3b54` — logic
Fault latch (bit 0x20 -> 0x10) for DAT_fff93b54, gate DAT_fff843e4. [O]

#### 0x0006D6C0 `spi_eval_fault_3b55` — logic
Fault latch for DAT_fff93b55, gate DAT_fff84404. [O]

#### 0x0006D6E0 `spi_two_call_shim` — stub
Two indirect calls only (PTR_FUN_0006d6fc -> 0x00068EA8 and thunk -> 0x0007624A). Callers=-
(pointer table). [O] shape only.

#### 0x0006DAA8 `crc8_calc_skip_idx` — library
Standard table CRC-8 (table at 0x43FD4, init 0xFF, indexed xor, final complement) over param_2
bytes at param_1, skipping index param_3. Caller 0x0006900C. [O]

#### 0x0006DADA `crc8_calc_inv_skip_idx` — library
CRC-8 variant over ones-complemented input bytes ((0x3ff-b)&0xff indexing), init 0xFF, skip one
index, complemented result. Callers=- (pointer table). [O]

#### 0x0006DF08 `tbl_get_byte_e234` — data-accessor
Returns **(byte**)PTR_PTR_0006e234[DAT_fff90ab8] — first byte of a variant-selected record.
Callers 0x0007703C/0x0007707A/0x00076FF0/0x000770C8. [O]

#### 0x0006E74A `tbl_get_byte_e7e4` — data-accessor
Same pattern through PTR_PTR_0006e7e4[DAT_fff90ab8]. Callers 0x000777xx. [O]

#### 0x0006E804 `init_chain_e804` — logic
Init shim (callers=-): 8 chained calls, including two registrations
PTR_FUN_0006e8dc(PTR_DAT_0006e8d8,&DAT_fff9099c,val) and (...e8e4,&DAT_fff90a3c,val) binding
buffers at 0xfff9099c/0xfff90a3c. Resolved callees 0x00094CE0/0x000782FC/0x00094062/0x000782C8/
0x000782F4/0x0006796A/0x0006791A. [O] shape only.

#### 0x0006E83C `spi_module_init` — logic
Module init (callers=-, pointer table): PFC checkpoint 0x00072AE0(0x1312d01), readiness check
(PTR_FUN_0006e8f4()==1 else event 0x260), four setup calls, clears DAT_fff848fc, then runs the
monitors FUN_0006d1b0/FUN_0006d3f4 and re-checks the PFC checkpoint. [O] flow, [I] purpose.

#### 0x0006E882 `spi_bus_setup` — logic
Bus setup bracketed by flow-control checkpoints 0x00072AE0(20000000, param_2,...)/(20000000)
(the constants are PFC checkpoint ids, not clock values): init chain PTR_FUN_0006e910..e918; if
magic byte DAT_fff848fc != 'M' runs the extra first-time path (PTR_FUN_0006e91c/0006e920/0006e8e8/
0006e924/0006e928). Callers=-. [O] shape, [I] purpose.

#### 0x0006E998 `adc_pack_nibble_e7a` — logic
Scheduled job: reads 4-bit value via PTR_FUN_0006ebd8 (-> 0x00151F48 app converter) and packs it
into bits 4-7 of DAT_fff90e7a. Multiplexed analog input bookkeeping (spec 04). [O]

#### 0x0006E9BC `adc_pack_nibble_e89` — logic
Same pack pattern into DAT_fff90e89 via PTR_FUN_0006ebdc (-> 0x00151F1E). [O]

#### 0x0006E9E0 `adc_pack_nibble_eaa` — logic
Packs converter result into DAT_fff90eaa bits 4-7 via PTR_FUN_0006ebe0 (-> 0x00151F2C). [O]

#### 0x0006EA04 `adc_pack_nibble_ed2` — logic
Packs into DAT_fff90ed2 bits 4-7 via PTR_FUN_0006ebe4 (-> 0x00151F64). Caller 0x00078368. [O]

#### 0x0006EA28 `adc_pack_nibble_ee3` — logic
Packs (b&0xf)<<2 into DAT_fff90ee3 (mask 0xc3) via PTR_FUN_0006ebe8 (-> 0x00151F56). [O]

#### 0x0006EA4A `adc_pack_nibble_f02` — logic
Packs into DAT_fff90f02 bits 4-7 via PTR_FUN_0006ebec (-> 0x00151F10). [O]

#### 0x0006EA6E `adc_pack_nibble_f58` — logic
Packs into DAT_fff90f58 bits 4-7 via PTR_FUN_0006ebf0 (-> 0x00151F3A). Caller 0x00078368. [O]

#### 0x0006EA92 `adc_store_result_ead` — logic
Job over a channel descriptor (param_1): builds the complement of the 0xc..count config buffer
(+0x48 -> +0x4c), reads 16-bit sample via PTR_FUN_0006ebf4 and 3-bit status via PTR_FUN_0006ebf8
(-> app converters 0x0015247A/0x00152494), packing value into DAT_fff90ead (+13 bits in
DAT_fff90eac) and status into DAT_fff90eac bits 5-7. Callers=- (job table, spec 03/04). [O]

#### 0x0006EAF0 `adc_store_result_ed5` — logic
Same pack pattern into DAT_fff90ed5/DAT_fff90ed4 via PTR_FUN_0006ebfc/0006ec00 (-> 0x001524A4/
0x00152486). Caller 0x00078368. [O]

#### 0x0006EB4E `adc_store_word_f06` — logic
Complements the channel config buffer and stores the 16-bit read from PTR_FUN_0006ec04
(-> 0x0015207C) into DAT_fff90f06. Callers=- (job table). [O]

#### 0x0006EB80 `adc_store_word_f5e` — logic
Same as FUN_0006eb4e but stores into DAT_fff90f5e. Caller 0x00078368. [O]

#### 0x0006EC08 `can_rx_poll_bus0` — logic
Polls CAN bus 0: PTR_FUN_0006ee2c(0) then FUN_0006ec38 over the 0x28-stride object table
(PTR_DAT_0006ee34, count *PTR_DAT_0006ee30); result stored in DAT_fff93b58. Caller FUN_0006e83c.
Spec 07. [O]

#### 0x0006EC26 `can_rx_poll_bus1` — logic
Bus-1 variant: PTR_FUN_0006ee2c(1), table at PTR_DAT_0006ee3c with count from PTR_DAT_0006ee38,
inline of the FUN_0006ec38 loop. Caller FUN_0006e83c. [O]

#### 0x0006EC38 `can_rx_process_table` — logic
Iterates param_2 message objects (0x28 stride): resolves each id via PTR_FUN_0006ee44, runs
FUN_0006ec80 with the bus selector param_3 and the object's "check enabled" byte (+0xf);
ORs any-activity into the return. Bracketed by PTR_FUN_0006ee40/0006ee48. [O]

#### 0x0006EC80 `can_rx_process_object` — logic
Core RX object processor (237 instrs): per-object status/config at *(p+8), status mirror
*(p+0xa); skips when disabled or expired (counter at cfg+4 vs p[2]/p[4]); on new data copies up
to max(p[6], status+6) payload bytes from *(p+0x22) into staging DAT_fff9131c, sets new-data flag,
runs the object's callbacks at +0x16/+0x18, then FUN_0006eec0 health result is merged and the
payload is complemented and stored to the second buffer *(p+0x26) — value/complement dual storage
per message (spec 07 protection model). [O]

#### 0x0006EEC0 `can_rx_check_object_health` — logic
Per-object health evaluation (150 instrs): for the status block selected by stage
(DAT_fff93b5a: +0x10+8 or +0x14), runs up to five callback checks at p+0x18/+0x1c/+0x20/+0x24/
+0x28 (gated by param_2 bits 8/0x10/0x20/4/0x40) with limit compares from p+0x38/+0x3c/+0x40;
increments per-check counters at +3/+4/+5 and returns accumulated failure bits. Matches the
CRC-8/counter/timing checks of spec 07. [O] structure, [I] which callback is which check.

#### 0x0006F000 `can_tx_submit_object` — logic
Transmit submit for one 0x28-stride TX object: (*DAT_0006f060)(id, cfg, cfg+6, data) then sets
cfg+7 bit 1. 18 callers (the 0x00069xxx packer family — spec 08). [O]

#### 0x0006F0B4 `sel5_sync_3b34` — logic
Latches DAT_fff93b5b = DAT_fff93b33 and, when PTR_FUN_0006f0f8 returns a changed value in 1..5,
updates DAT_fff93b34. DAT_fff93b33 is used as a 0..5 table index by FUN_00067b16 — a small
selector (program/position), exact meaning unresolved. Callers=- (pointer table). [O]

#### 0x0006F0FC `sel5_check_3b5d` — logic
Validity word for the same selector: 0 when state 0 and PTR_FUN_0006f178()!=0; 1 when active
state 0; 3 for states outside 0..4; 5 when PTR_FUN_0006f17c(param) fires; stored in DAT_fff93b5d.
Consumed by FUN_000784b4. Caller 0x000784B4. [O]

#### 0x0006F144 `sel5_cond_3b40` — logic
Condition flags: PTR_FUN_0006f180()==1 and DAT_fff93b40<2 -> DAT_fff93b40=1; PTR_FUN_0006f184()
==1 clears DAT_fff848e6. Callers=- (pointer table). [O]

#### 0x0006F188 `can_tx_register_msg_1328` — logic
Registers the TX message object at &DAT_fff91328 on bus A: PTR_FUN_0006f200 (-> 0x0006A45C) +
PTR_FUN_0006f204 (-> 0x0006A3F0). Cluster member 0x0006F19C is the bus-B variant (via
0x0006A46E/0x0006A41C). Callers=- (init tables). [O]

#### 0x0006F1B0 `can_tx_register_msg_133c` — logic
Same registration pair for the object at &DAT_fff9133c (bus A; member 0x0006F1C4 = bus B). [O]

#### 0x0006F1D8 `can_tx_register_msg_1350` — logic
Same registration pair for the object at &DAT_fff91350 (bus A; member 0x0006F1EC = bus B). [O]

#### 0x0006F26C `can_rx_init_timeouts` — logic
Initialises the 0xc-stride timeout table at DAT_fff94d02 to 6000 for *PTR_DAT_0006f3fc entries;
runs one-time init (PTR_FUN_0006f400) when magic DAT_fff9372e != 0x1324, then DAT_0006f404 and
sets the magic. Callers=-. [O] mechanics, [I] that the table is CAN RX timeouts.

#### 0x0006F434 `dtc_report_on_mask` — logic
Fault-event trigger: returns early unless param_1 & mask, then reports event via the
fault-reporting API 0x0006F5C0(id,0x10,0). 12 byte-identical copies differing only in mask/event:
0x0006F434 (0xc000, 0x44c), 0x000794E4 (0xf000, 0xbe0), 0x0007ADBC (0xfc00, 0x9d8), 0x0007AE62
(0xfff8, 0x9f1), 0x0007BFF0 (0xfe03, 0x988), 0x0007C086 (0xff9d, 0x98a), 0x0007C0E8 (0xfe9f,
0x98b), 0x0007C662 (0xffe3, 0x9ce), 0x00082EB4 (0xfffc, 0xbc2), 0x0008877C (0xfff3, 0x9cf),
0x000887A8 (0xfcf3, 0x9f6), 0x0008D550 (0xfffd, 0x96a). Spec 13 event table. [O]

#### 0x0006F44C `dtc_report_44c_set_385a` — logic
Reports event 0x44c (PTR_FUN_0006f4bc(0x44c,0,p1,p2)) and ORs p1&p2&0xe3f into status word
DAT_fff9385a, bracketed by lock/unlock PTR_FUN_0006f4b8/0006f4c0. Callers 0x0006F630/0x0006F642.
[O]

#### 0x0006F47E `dtc_report_25_debounced` — logic
Debounced event: after first pass (DAT_fff93b63>1) reports PTR_FUN_0006f4c4(0x25,1,
DAT_fff9385a!=0), then increments the counter. Caller 0x0007AB9C. [O]

#### 0x0006F4C8 `dtc_report_546_27_pair` — logic
Reports 0x546 via PTR_FUN_0006f584, merges masked bits into DAT_fff9385c (set p1&p2, clear ~p1),
raises 0x27 (PTR_FUN_0006f588(0x27,1)) while DAT_fff9385c!=0, and clears it (PTR_FUN_0006f58c)
after debounce DAT_fff93b64 settles. Callers=- (pointer table). [O]

#### 0x0006F526 `dtc_report_546_on_not_1f` — logic
Unless param_1 == 0x1f, reports PTR_FUN_0006f590(0x546,0x10,1). Callers=- (pointer table). [O]

#### 0x0006F53A `tbl_get_u32_94864` — data-accessor
Indexed (0..4) u32 getter from DAT_fff94864/68/6c/70/74, 0 otherwise. Caller 0x000A9F72. [O]

#### 0x0006F594 `diag_update_cond_94038` — logic
Caches a computed condition: DAT_fff94038 = PTR_FUN_0006f5b8() (-> 0x000A0776), mirrored into
DAT_fff94039, forced to 1 when *PTR_DAT_0006f5bc != 0. 11 callers in r08_app. [O]

#### 0x0006F5C0 `dtc_queue_event` — logic
Core fault-event queue (spec 13 reporting API): while count DAT_fff94049 < 5 stores (id -> 
DAT_fff9386a[], flags -> DAT_fff93860[], arg -> DAT_fff92dcc[], in_pr -> DAT_fff92db8[]) and
bumps overflow byte DAT_fff94048; beyond 5 saturates at 0xff. 27 callers across the image. [O]

#### 0x0006F630 `eeprom_set_status_bits` — logic
DAT_fff9380c |= param_1 then reports through PTR_FUN_0006f658 (-> FUN_0006f44c, event 0x44c).
Callers: both EEPROM state machines. [O]

#### 0x0006F642 `eeprom_clear_status_bits` — logic
DAT_fff9380c &= ~param_1 then the same 0x44c report. Caller FUN_0006ab8e. [O]

#### 0x0006F66C `eeprom_scan_select_block` — logic
738-instr block scan/select engine (state DAT_fff848ff 0..7) used by both EEPROM state machines:
initialises the 8 x 0x2c record matrix at DAT_fff920cc; per block reads header fields via
FUN_0006f65c(idx,cfg), double-reads 0x54-byte headers (FUN_000702ca x2) and compares them,
requires marker 0x4001 and version nibble 1, extracts length/address (FUN_0006fcfc/0x0006fd92),
searches candidates (FUN_0006fcd4), validates tail (FUN_0007032a) and finally loads payload
(FUN_000706c4); returns 0/1/3/4/5, sets session fields param_1+0x18/0x1a/0x24/0x34/0x38.
Power-loss-safe double-read matches spec 06/18 record model. [O]

#### 0x0006FD92 `eeprom_is_rb_plant_marker` — logic
Compares 20 bytes at param_1 against the literal "RB Plant Data Prog..." string (0x443A0 area) —
record-name check for the RB plant-data block. Caller FUN_0006f66c. [O]

#### 0x0006FDB8 `eeprom_select_erase_block` — logic
498-instr selection engine (state DAT_fff84900 0..5, matrix DAT_fff92290..98): per block erase
checks (FUN_00070452 with 0xa5), +0x80 read via PTR_FUN_000700ec, double 0x54-header read compare
(FUN_000702ca), mode classification from param_1+0x39 nibbles (1/2/4/0 -> flags at 0x92296/95/96/
97) and a five-pass priority search (valid -> any -> mode-specific -> oldest via counter at
0x92298) picking the next block into param_1+0x19. Returns 1 found / 2 none. Caller
FUN_0006ab8e. [O]

#### 0x00070200 `eeprom_scale_byte` — library
Fixed-point scale: ((param&0xff) * *(int*)(*(param_2+0x28)+4)) >> 13 & 0xff. Callers
0x000720C8/0x0007237A/0x00071594. [O]

#### 0x00070210 `flash_crc16_range` — library
Accumulating CRC/checksum over [param_1..param_2] in chunks: full 0x37-block fast path, then 8-
byte steps; for aligned addresses whose (addr&0xff)>>3 is 7/8/10 consults PTR_FUN_0007036c
(erased-region probe) and substitutes len*0xff, otherwise folds via PTR_FUN_00070368(start,end,
sum). Returns 0xffff on failure. Callers 0x00070998/0x000702CA/0x00071594. [O]

#### 0x000702CA `flash_crc16_read` — logic
Selects flash read mode (PTR_FUN_00070370 1/2/0) then computes FUN_00070210 over
[param_1[0] .. +param_1[1]-1] with seed from param_1+10; stores crc into param_1[3] with ok flag
at param_1[4] (0xffff -> 0/0). Callers 0x00070998/0x000706C4/0x0006F66C/0x0006FDB8. [O]

#### 0x0007032A `flash_read_verify_chunked` — logic
Chunked (0x800) read/verify state machine (DAT_fff84901 0..3): moves addr/len through
DAT_fff922fc/0x92300, optionally re-reads (param_2==1 -> PTR_FUN_00070558(2)), and finishes by
comparing DAT_fff92304 against DAT_fff92308 into param_1[3] + ok flag. Callers
0x00070998/0x0006F66C/0x00071594. [O]

#### 0x00070452 `flash_check_erased_8b` — logic
Erase check: busy -> 2; else under mode switches scans 8 bytes at param_1 (twice) for any byte !=
param_2 (0xa5). Callers 0x0006F66C/0x0006FDB8. [O]

#### 0x00070518 `eeprom_max_block_metric` — logic
Returns max of the 8 u32s at DAT_fff92de0 (block metrics collected by FUN_0006f66c). Caller
0x000797FE. [O]

#### 0x0007053A `eeprom_find_write_entry` — logic
Scans the 0x18-stride work table from DAT_fff84880 up to param_1+0x30 for an entry with flags
(byte[2]&2), not 0x80, len!=0 and (+7&1)==0: sets param_1+0x60 = entry and state param_1[100]=3.
Caller FUN_0006ab8e. [O]

#### 0x0007061C `eeprom_wait_session_state` — logic
Poll with timeout: on first entry arms DAT_fff93b72 = -1 and DAT_fff94625 |= 4 unless session
state (PTR_FUN_000707a0, DAT_fff94624) == 0x1e; decrements each call; returns 1 busy / 0 ok /
0x80 timeout, cleaning up on completion. Caller FUN_00070998. [O]

#### 0x000706B6 `eeprom_clear_busy_flag` — logic
Clears DAT_fff94625 bit 2 and polls the session state via PTR_FUN_000707a0. Caller FUN_00070998.
[O]

#### 0x000706C4 `eeprom_load_record_data` — logic
Loads a record payload: resolves block (PTR_FUN_000707b0), prefers the +0x60 mirrored header when
its double 0x18-byte CRC read (FUN_000702ca x2) matches, else the +0x28 copy; copies up to
param_4 (<=8) bytes into param_2 and the u16 fields into param_3, summing them into param_1+0x54.
Caller FUN_0006f66c. [O]

#### 0x000707B8 `eeprom_pick_rewrite_block` — logic
Erase/rewrite target scan: gates on param_1+0x5c flags &0x10, mode nibble at +0x42 and +0x58 bit0;
scans blocks for mode-2 candidates (and lowest +0x39>>4 rank) returning a block index or 0xff.
Callers FUN_0007258A. [O]

#### 0x0007088E `eeprom_tally_block_modes` — logic
Clears counters at param_1+0x41/0x42 and tallies each block's mode nibble (param_1+0x39+i &0xf):
mode 1 -> +0x41 hi nibble, mode 2 -> +0x41 lo, mode 4 -> +0x42 hi, else +0x42 lo. Callers
0x0006A86C/0x0006AB8E/0x000707B8/0x00071594. [O]

#### 0x00070998 `flash_scan_data_blocks` — logic
720-instr boot-time data-flash scan (state DAT_fff84903 0..0xd): walks 0x1000-stride blocks from
&DAT_80100000 up to PTR_DAT_00070c30 validating marker 0x4001 with double 0x54-header reads,
classifies types 1/2/3 into base offsets DAT_fff844e4 (0x2000/0x4000/0x8000), chains 0x8000-flag
segments, records segment pointers in DAT_fff844e8/ec (max 16) and per-segment checksum tables in
DAT_fff8458c/0x8459c; session abort (DAT_fff94624==6) -> state 0xd; result in DAT_fff844e0
(0 ok / 2 none). Caller FUN_0006ab8e. Spec 06 block headers. [O]

#### 0x00071026 `eeprom_get_scan_result` — data-accessor
Returns DAT_fff844e0 (data-flash scan outcome set by FUN_00070998). Callers
0x0006AB8E/0x0007D3CC/0x0006B408. [O]

#### 0x00071040 `eeprom_write_records_step` — logic
313-instr sub-state engine (DAT_fff91f3c 0..5, flags DAT_fff91f3e) walking the entry table
(param_2+0x2c/+0x60) via FUN_000712fc: sizes records (DAT_fff91f44/0x91f48), pre/post callbacks
(entry[2]/entry[4]), and the final copy entry[3](param_3+8, len<=0x300,...,1,...,&DAT_fff91f44)
writing length into target+1..+2 and setting status bits. Callers 0x0006A86C/0x0006AB8E. [O]

#### 0x000712FC `eeprom_find_entry` — logic
Record-entry search from block+0x80 within param_4+0x34 bytes: mode 1 matches entry id
(param_2[1] vs entry&0xfff) and type byte; modes 2/3 match flag masks built from the literal
bytes at s__RB_Plant_Data_Prog__000443a0[0x14] / DAT_000443b5 against entry field bits
(&0x7000). Follows u16-stride chains. Callers 0x00071550/0x00071040/0x00071594. [O]

#### 0x00071434 `eeprom_expand_entries` — logic
Walks 8-byte entries (count param_1[0]); for type 0/5 repeats the current source byte per
entry length (PTR_FUN_00071584), otherwise consumes *(short*)(entry+4); fills the destination
stream via DAT_fff92334/0x92338 cursors, tracking DAT_fff9380e; returns repeat count. Caller
0x00086C68 (r08_app). [O]

#### 0x00071594 `eeprom_write_page_engine` — logic
1253-instr page-writer core (state DAT_fff91f54, values 0x14/'('/')'/'*'/'+'/', '/'-'/negative):
erases target (FUN_000720c8), builds records into the 0x80-byte staging DAT_fff92000 (headers,
odd-length padding to 8, running offsets DAT_fff91f64/0x91f68), chains entries via
PTR_FUN_00072000/FUN_000712fc, and programs pages with PTR_FUN_00071854(block,offset,buf,flag),
crossing 0x2000-byte boundaries and blocks (DAT_fff91f57/0x91f5e); CRC/verify via
FUN_00070210/FUN_0007032A/0x0007D53E. Caller FUN_0006ab8e. Power-loss-safe write order (spec 06/
18). [O]

#### 0x000720C8 `eeprom_erase_block` — logic
Erase sequence (state DAT_fff84904 0..7): picks block (PTR_FUN_00072240), checks header
(+0x38) erased, marks with 0xa5 pattern (PTR_FUN_00072250 8/0x38 and 0x40 variants), waits
through PTR_FUN_00072258/0007225c status (errors 2/3/4), and on success sets
param_2+0x1b+param_1 = 1. Returns 0 ok / 1 busy / 2-4 errors. Callers 0x0006AB8E/0x00071594. [O]

#### 0x0007221E `eeprom_pack_bits_entries` — logic
Bit-level packer over 8-byte entries (PTR_FUN_000724b4 length, 000724b8 source): type 0/1/5
copies bytes, type 2 stores the source-pointer low byte, types 3/4 extract with shift
((1|3) - DAT_fff93814)*-8 — reconstructing packed bitfields into the destination stream via
DAT_fff92348/0x9234c. Caller 0x00086D70 (r08_app). [O]

#### 0x0007237A `eeprom_write_status_words` — logic
Writes the per-block status words (state DAT_fff84905): for indices 0xc/0xd/0xe packages 8 bytes
from param_1+0x39/0x44/0x4c (with 0-fill thunk 0x0007D548) and programs at DAT_fff93818 (0x60+),
advancing 8 bytes per step up to 0xf; verifies via checksum PTR_FUN_000726a4 over block+0x60..
+0x77. Returns 0 ok / 1 busy / 2-6 errors. Caller FUN_0006ab8e. [O]

#### 0x0007258A `eeprom_advance_write_index` — logic
Progress step: gated by param_1+0x5c bit 0x40 and a valid pick (PTR_FUN_000726a8 + FUN_000725d4(0)
== 1), increments write counter DAT_fff9381c and stores the next block into param_1+0x19.
Caller FUN_0006ab8e. [O]

#### 0x000725D4 `eeprom_queue_alloc_slot` — logic
Allocates the first free slot of the 10x4-byte request queue at DAT_fff920a0 (byte: 0x80 busy +
7-bit id, u16 value): stores param id/value and marks busy; returns 1 on success. Callers
0x0007258A/0x0006AB10 (the retry writer). [O]

#### 0x00072618 `eeprom_slot0_status` — logic
Returns slot-0 state: 0 free, 1 busy-with-id-0, 2 busy. Caller 0x0007A3DC. [O]

#### 0x00072634 `eeprom_queue_shift` — logic
Compacts the queue: moves entries +4..+9 down one slot (merging busy bit) and clears the tail
(DAT_fff920c4/0x920c6). Caller FUN_00071594 (page writer consumes entries). [O]

#### 0x0007267E `eeprom_queue_clear` — logic
Resets all 10 queue slots (busy bit cleared, values zeroed). Callers 0x0006AB8E/0x00071594. [O]

#### 0x00072708 `crc16_calc` — library
Table CRC-16 (u16 table PTR_LAB_00072734): crc = (crc<<8) ^ tab[((crc>>8)^byte)&0xff] over the
range. Callers=- (pointer table). [O]

#### 0x00072738 `crc16_stream_step` — library
Streaming CRC-16 (u32 table at PTR_LAB_000727c4) over a {cur,end,crc,max,state} descriptor:
states 1/2 feed bytes up to the limit; at end stores ~crc with state 4, at limit raw crc with
state 2. Callers=- (pointer table; used by the write/verify chain). [O]

#### 0x000727C8 `boot_march_diverse_pairs` — logic
March test over param_2 u32 pairs: writes (0,0xffffffff), then (pattern,~pattern) and the
inverted pair with read-back checks, restoring originals; on the first failing pair sets
DAT_ffff0842 |= 0x6800 and returns its address. Called from FUN_0006bf12 (boot RAM march). [O]

#### 0x0007284C `boot_handover_step` — logic
Handover/shutdown state DAT_fff94063: when variant flags (*PTR_PTR_0007299c[DAT_fff90ab8])&2 and
state 2 re-evaluates FUN_0007291a; state 3 with DAT_fff90954==0 -> state 4 and FUN_00072964.
Callers=- (task table). [O]

#### 0x00072898 `boot_handover_abort_eeprom` — logic
Handover prep: in state 2 (unless PTR_FUN_0007291a says 2 and variant flag 4) calls
PTR_FUN_000729a0(0) (-> FUN_0006c612 watchdog/request) and PTR_FUN_000729a4 (-> FUN_0006b470
eeprom abort) and advances to state 3; returns 0 busy / 2 armed. Caller thunk 0x0006BC38. [O]

#### 0x000728F6 `boot_handover_get_state` — logic
Returns handover state DAT_fff94063 (recomputing it via FUN_0007291a while < 3, then 2).
Caller thunk 0x0006BC2C. [O]

#### 0x0007291A `boot_handover_eval` — logic
Readiness evaluation: enabled when DAT_fff84a46!=0 and counter DAT_fff9095b>14; returns 1 when
PTR_FUN_000729a8()==nonzero and variant flag &1, else 2; 0 when disabled. Callers
0x00072898/0x000728F6/0x0007284C. [O]

#### 0x00072964 `boot_handover_reset` — logic
Reset/hand-over sequence: three prep calls, then invokes the boot vector *(PTR_PTR_00006010+0x34)
(boot-data region 0x6000 block descriptor area, spec 06) with DAT_fff80ce4 set before/after, and
reports event 0x2f2 via PTR_FUN_000729c4. Caller FUN_0007284c. Spec 15 hand-over. [O] shape,
[I] that the vector enters the boot manager.

#### 0x000729C8 `eeprom_session_step` — logic
Top-level EEPROM session scheduler (callers=-, task): with DAT_fff93b8e==0 and state
DAT_fff93b90!=2: state 0 checks three readiness pointers (PTR_FUN_00072ac0/ac4==1) then arms
PTR_FUN_00072ac8, else runs the driver FUN_00072a22; when flag 0x93b8e or state 2 -> marks
DAT_fff93b8f=1. [O]

#### 0x00072A22 `eeprom_session_driver` — logic
State step (DAT_fff93b91): 0 gates on DAT_fff94808 and arms; 1-3 checks request bits (uVar1&1)
via PTR_FUN_00072ad4/ad8; 0x10 waits on PTR_FUN_00072ac0; 0xff finalises (event via
PTR_FUN_00072adc, sets DAT_fff93b8f=1). Drives the record-write engine through the variant table
PTR_PTR_00072acc[DAT_fff90ab8]. Caller FUN_000729c8. [O]

#### 0x00072AE0 `pfc_checkpoint` — logic
Program-flow-control checkpoint (spec 05 safety companion): when the cursor value
*DAT_fff847e8 equals param_1 (call-site id like 0xa1d5b0/0x1312d01/20000000) advances the
cursor; at list end sets DAT_fff9406c/0x9406b bit 2; on mismatch marks the node (+0x29), saves
context (FUN_00072de2/FUN_00072e4c), notifies PTR_FUN_00072d9c(node id) and raises event 0x228.
27 call sites across the image. [O]

#### 0x00072B48 `pfc_periodic_check` — logic
Periodic PFC evaluation (callers=-, task): gated by PTR_FUN_00072da4; from states
DAT_fff94069/0x94068/0x9406a/DAT_fff9387a sets/clears ok bit 1 of DAT_fff9406c/0x9406b and
reports via PTR_FUN_00072dac/00072db0. [O]

#### 0x00072BD4 `pfc_verify_shadow` — logic
Compares/refreshes the double-buffered PFC shadow: with DAT_fff80cff==0 verifies the saved pair
(DAT_fff94880/0x94888, flags DAT_fff94974/76) against the current buffers and returns early on
match; otherwise publishes the new pair and toggles the slot (DAT_fff94998). Caller 0x00086F5C.
[O]

#### 0x00072CCE `pfc_walk_list` — logic
Walks the PFC node list (DAT_fff92f44, next via +0x24, cursor via +0x2c): re-arms mismatches
(+0x29), event 0x228, tracks pending flags (+0x2a -> DAT_fff94069/0x94068), and folds a running
check via PTR_FUN_00072db4(node, cursor^node+flags) before advancing head/cursor. Caller
0x0007E500. [O]

#### 0x00072DB8 `pfc_push_node` — logic
Pushes a new PFC node: saves the current cursor into old->+0x2c, links param_1 (+0x24 prev),
resets its +0x29 flag, installs its cursor from +0x30 and kicks PTR_FUN_00072ea8. 14 callers
(0x0007Exxx init/registration). [O]

#### 0x00072DE2 `pfc_save_context` — logic
Saves the checkpoint context into the toggle buffers: id (or 0/-1 special) into DAT_fff80ce8
[slot], cursor value into DAT_fff80cf0[slot], node id into DAT_fff80d00[slot], marks
DAT_fff80cfe[slot]=1 and toggles DAT_fff80d1d. Callers 0x00072AE0/0x00072CCE. [O]

#### 0x00072E4C `pfc_copy_to_shadow` — logic
Copies the inactive toggle slot (DAT_fff80ce8/0x80cf0/0x80d00) into the shadow arrays
DAT_fff94880/0x94888/0x94976 and marks it valid (DAT_fff94974=1), toggling DAT_fff94998.
Callers 0x00072AE0/0x00072CCE. [O]

#### 0x00072EB6 `wdt_answer_compute` — logic
Question/answer computation under IPL lock: with guard DAT_fff80d19==-0x5b resets the counter
pair DAT_fff80cd8/0x80cd4, else shifts one count from cd4 to cd8; publishes tag DAT_00073018 into
DAT_fff80cdc and the combined answer (cd8&0xffff + cd4<<16) ^ tag into DAT_fff80ce0, mirroring
cd4 into DAT_fff92f48. Called from FUN_0006c612. [O] mechanics, [I] watchdog Q&A role (spec 05).

#### 0x0007309A `boot_range_check_5words` — logic
Plausibility check of 4 u32 window bounds against fixed limits (DAT_fff92f68/6c/70/74) plus
ordering; on pass copies 5 u32s into the staging block DAT_fff92418-0x92428 (consumed by
FUN_0007310e). Callers=- (pointer table). [O]

#### 0x00073100 `mem_request_switch_3ba9` — data-accessor
Stores the selector into DAT_fff93ba9 and sets request bit 8 in DAT_fff93ba8 for the deferred
applier FUN_0007310e. Callers=- (pointer table). [O]

#### 0x0007310E `mem_apply_deferred_config` — logic
Deferred config applier (callers=-, task): for each set bit of DAT_fff93ba8 — bit 1 copies the
staged window/pointers DAT_fff9241c-0x92428 into DAT_fff80088/8c/90/94 and DAT_fff911b8; bit 2
starts two services (PTR_FUN_0007320c/00073210); bit 8 swaps DAT_fff8008c between DAT_fff92440
and DAT_fff911bc/DAT_fff80090 by DAT_fff93ba9; bit 4 runs and clears callback DAT_fff9243c; bit
0x10 invokes (*(DAT_fff9247c+0xc))(sel,DAT_fff92434) into DAT_fff80098. [O] mechanics, [I]
purpose (RAM/stack window switch).

#### 0x000731E6 `timer_latch_92430` — data-accessor
Stores the free-running timer DAT_fffff220 into DAT_fff92430. Caller FUN_0006cf7e. [O]

#### 0x000731F4 `diag_tbl_get_92480_0c` — data-accessor
Returns *(u32*)(DAT_fff92480 + DAT_fff800b0*0x30 + 0xc) — field from the 0x30-stride channel
table selected by DAT_fff800b0. Callers=- (pointer table). [O]

#### 0x00073214 `diag_install_table_92480` — logic
Installs the channel-descriptor table: DAT_fff92480 = *(param_1+4), DAT_fff9247c = param_1,
entry count DAT_fff93baa = *param_1. Caller FUN_00073228. [O]

#### 0x00073228 `diag_register_channels` — logic
Channel registration (callers=-, init table): calls FUN_00073214 then, for each 0x30-stride
entry, PTR_FUN_000732b8(id, base+0x18+off, 8, 0, DAT_fff93b4f) with 8-byte stride; finally fills
the callback slots DAT_fff92444..0x92478 (PTR_LAB/PTR_FUN tables) used by the dispatcher.
Callee 0x000806F4. [O] shape.

#### 0x000732F8 `diag_sched_services` — logic
192-instr scheduler over the 0x14-stride request table (*DAT_fff92484, count +1): builds
session/type masks from channel bits (PTR_FUN_00073548/4c/50), matches each entry (type char,
session char, mask at +2, +4) and invokes its handler (+8/+0x10 with &DAT_fff92490), tracking
match quality in DAT_fff9249a and calling PTR_FUN_00073554/00073558 on completion. Callers
0x00073D1E/0x000739EC. [O]

#### 0x000734AE `diag_protocol_enable` — logic
Enable path: PTR_FUN_00073560, applies selector DAT_fff800ac via DAT_00073564, FUN_000734ee,
then invokes each entry point at +0xc of the 0x14-stride table. Callers=- (pointer table). [O]

#### 0x000734FE `diag_sample_params_800a4` — logic
Snapshots protocol parameters into DAT_fff800a4 (0x77), a5/a6 (PTR_FUN_0007355c/00073568), ab/ac
(PTR_FUN_00073550/0007356c), ae = DAT_fff924d4, then runs hook *(DAT_fff924a0+0x1c). Caller
0x00073FB0. [O]

#### 0x00073570 `diag_protocol_init` — logic
Protocol init: marks DAT_fff93bc4=1, sets NRC constants DAT_fff93bc5=0x78 /
DAT_fff93bc6=0x21, response buffer base DAT_fff924a8 = *(DAT_fff924a0+0x14)+4, clears state
(DAT_fff93bbb/bc1/924a4/bc3), runs PTR_FUN_0007362c + FUN_000735e4 and readiness poll
(PTR_FUN_00073630) into DAT_fff93bbf. Caller 0x0006CA44. [O]

#### 0x000735E4 `diag_clear_rsp_counters` — logic
Clears response counters DAT_fff9248c/0x92488 and calls PTR_FUN_00073638(0)/0007363c(0).
Caller FUN_00073570. [O]

#### 0x00073602 `diag_set_channel_desc` — data-accessor
Stores the channel descriptor: DAT_fff924bc = param_1, mode DAT_fff93b4f = *param_1, enable
DAT_fff924d1 = 1, config DAT_fff924c4 = *(param_1+0xc). Caller 0x0006CA44. [O]

#### 0x00073622 `diag_set_table_924a0` — data-accessor
Stores the module/service table pointer DAT_fff924a0 = param_1. Caller 0x0006CA44. [O]

#### 0x00073640 `diag_poll_service_handlers` — dispatch
Iterates the service-object table (DAT_fff924a0[1] entries): calls each object's method (+0)
returning 1 (started -> DAT_fff93bae=1) or 2 (pending -> DAT_fff93bae=2 + module hook
DAT_fff924a0[9]); records active object/context into DAT_fff92488/0x93bad/0x9248c and stops at
objects flagged +0x34. param_1==1 skips the current object. Caller FUN_00073708. [O]

#### 0x000736CE `diag_cancel_other_services` — logic
For every service object except the active one (DAT_fff9248c) invokes its cancel method (+4).
Caller FUN_00073708. [O]

#### 0x00073708 `diag_service_arbiter` — logic
Session arbiter (callers=-, task): with an active service (DAT_fff92488) either continues it
(method +0 poll, 2 -> pending + hook DAT_fff924a0[9]) or aborts via its +0x2c(2) method, then
FUN_000736ce, module hooks (DAT_fff924a0+0x18, DAT_fff924a4+0x30) and reset calls
PTR_FUN_000737ec/000737f0. [O]

#### 0x000737F4 `diag_send_pos_response` — logic
Builds a 1-byte positive response (SID + 0x40 '@') at DAT_fff92490-1 and transmits via the
channel send primitive (*DAT_fff924a4)(buf, len+1, dest, destLen, flag 2-if-pending).
Caller FUN_000739ec. [O]

#### 0x00073826 `diag_send_rsp_7f_sid` — logic
Builds the 2-byte prefix {0x7f, SID} before DAT_fff92490 and sends len+2 (flag 2 when
param_2==0). Caller FUN_000739ec cases 2/5. [O] (negative-response header shape).

#### 0x0007385A `diag_build_neg_response` — logic
Builds {0x7f, SID(param_1), NRC} at DAT_fff93bb4..b7; special case param_1==0x80 with
DAT_fff92496!=0 appends the first request byte and returns len 4, else 3. Callers
0x00073890/0x000738C4/0x000738F4. [O]

#### 0x00073890 `diag_send_nrc78_pending` — logic
Sends NRC 0x78 (responsePending): FUN_0007385a(DAT_fff9249b, DAT_fff93bc5=0x78) then transmit
with flag 3. Callers=- (pointer table). [O]

#### 0x000738C4 `diag_send_nrc10` — logic
Sends NRC 0x10 (generalReject) via FUN_0007385a(DAT_fff93bbc, 0x10) to the response buffer
DAT_fff924a8 with the table max length. Callers=- (pointer table). [O]

#### 0x000738F4 `diag_send_nrc21` — logic
Sends NRC 0x21 (busyRepeatRequest) via FUN_0007385a(sid by DAT_fff93bbb, DAT_fff93bc6=0x21)
to &DAT_fff93bb0 with len 9. Callers=- (pointer table). [O]

#### 0x00073938 `diag_echo_request_rsp` — logic
Copies the raw request bytes (DAT_fff800b6 count from DAT_fff800b7) into DAT_fff924a8+4 and
retransmits them (flag 3 when >2 bytes, first byte 0x7f and SID byte matches DAT_fff93bc5).
Callers=- (pointer table). [O]

#### 0x000739A0 `diag_reset_rsp_state` — logic
Resets response state: DAT_fff924ac = *DAT_00073a30, DAT_fff924b0 = 0, DAT_fff93bbd/bbe = 0,
DAT_fff93baf = 0, DAT_fff93bc2 = 0. Callers FUN_00073570/0x000739C0. [O]

#### 0x000739C0 `diag_send_pending_rsp` — logic
Resets state (FUN_000739a0), transmits the buffered response
(*DAT_fff924a4)(0,0,DAT_fff924a8,maxlen,0) and stores the completion token param_1 in
DAT_fff924b8. Callers FUN_000739EC/0x00073D1E. [O]

#### 0x000739EC `diag_dispatch_request` — dispatch
10-case jump-table dispatcher on the received request code DAT_fff9249a (cases 1..0xb): cases
1/2/4/5 build responses via FUN_000737f4/0x00073826 (dest DAT_fff924a8 + maxlen), case 3/6 send
direct, cases 7/8/0xa/0xb set pending flags (DAT_fff93bbd/0x93bbe/0x93bc2) and completion
objects from PTR_PTR_00073ca4/8 tables; invalid codes -> FUN_000739c0(2). Updates
DAT_fff924ac per case. Caller FUN_00073c32. Spec 15 service table. [O]

#### 0x00073C32 `diag_protocol_step` — logic
Main protocol poll (callers=-, task): when a new request is flagged (DAT_fff924ce) loads
SID/subcode/pointer/length (DAT_fff9249b/9c/0x92490/0x92496), decodes via PTR_FUN_00073cc4/
00073cc8 and dispatches FUN_000739ec; otherwise manages the pending gate (DAT_fff924d1/0d0/
DAT_fff93bc1). Caller FUN_00073cda. [O]

#### 0x00073CDA `diag_periodic_task` — logic
When the channel is enabled (DAT_fff924d1), installs the periodic callback from
PTR_PTR_00073e54 into DAT_fff924ac and invokes it. Callers=- (task table). [O]

#### 0x00073D1E `diag_check_pending_rsp` — logic
Pending-response variant of the request poll: reads the code via PTR_FUN_00073e58 into
DAT_fff9249a; codes 8/9/0xa/0xb/7 gate on the pending flags DAT_fff93bbd/0x93bbe (result 0/1),
others reset state (DAT_00073e5c) — finally sends the buffered response via FUN_000739c0(1/3/4).
Callers=- (task table). [O]

#### 0x00073E6C `diag_chk_buffer_window` — logic
Returns 1 when mode (PTR_FUN_00073f60)==2 and DAT_fff8008c < DAT_fff911b8 — response buffer
window check tied to the deferred pointer config (FUN_0007310e). Caller FUN_00073c32. [O]

#### 0x00073E92 `diag_clamp_rsp_len` — logic
Clamps the response length: to 6 when mode==2 and DAT_fff924d2 is 2/3, or to 0xfe when mode==2.
Caller FUN_00073c32. [O]

#### 0x00073EC8 `diag_select_session` — logic
Session/mode selection over the 2-byte-stride table DAT_fff924c0 (count DAT_fff93bc8): matches
param_1 directly or against the active mode (FUN_00073f50, current *DAT_fff924bc+2); on match
sets mode DAT_fff924d2 (from table+1), DAT_fff924d3 = param_1, DAT_fff924d4 = param_2. UDS
DiagnosticSessionControl shape. Callers=- (service object). [O]

#### 0x00073F42 `diag_set_session_table` — data-accessor
Installs the session table: DAT_fff924c0 = param_1, count DAT_fff93bc8 = param_2. Callers
0x000800AA/0x00080108. [O]

#### 0x00073F50 `diag_is_mode_active` — data-accessor
Returns DAT_fff93b4f == param_1 (current channel-mode compare). Caller FUN_00073ec8. [O]

#### 0x000732F0 `diag_set_reqtable` — data-accessor
Stores the request/condition table pointer DAT_fff92484 = param_1 (the 0x14-stride table walked
by FUN_000732f8). Callers 0x0007FFB4/0x00073F9A. [O]

#### 0x00073F9A `diag_set_reqtable_thunk` — stub
Tail-jump thunk to FUN_000732f0 (re-installs the request table) through DAT_000740b8. Caller
0x0006CAD4. [O]

#### 0x00073FA0 `diag_get_mode_924d2` — data-accessor
Returns the active session/mode byte DAT_fff924d2. Callers 0x00082178/0x00075Exxx. [O]

#### 0x00073FA8 `diag_set_911bc` — data-accessor
Stores param_1 into DAT_fff911bc (fallback pointer consulted by FUN_0007310e). Caller
0x00076004. [O]

#### 0x00073FB0 `diag_sample_params_thunk` — stub
Tail-jump thunk to FUN_000734fe through DAT_000740bc. Callers 0x00075E2A/0x00075E9C. [O]

#### 0x00073FB6 `diag_set_911b0` — data-accessor
Stores the 16-bit param into DAT_fff911b0. Callers 0x00087CB8/0x00088068. [O]

#### 0x0007406A `diag_chk_state3_77` — data-accessor
Returns 1 when DAT_fff80085 == 3 and DAT_fff800a4 == 'w' (0x77). Callers 0x00073570/0x000782C8.
[O]

#### 0x00074084 `diag_reset_link_state` — logic
Clears the link state block: DAT_fff80080/82/c4/a4 = 0, DAT_fff80084/85 = 0xff. Callers=-.
[O]

#### 0x000740D4 `diag_seed_rng` — logic
Seeds the pseudo-random stream: byte index DAT_fff93bc9 = 4, value/seed DAT_fff924dc/0x924d8 =
FUN_00074138(). Caller 0x0006CAD4 — SecurityAccess seed generation (spec 15/17). [O]

#### 0x000740EC `diag_rng_fill_bytes` — logic
Emits param_1 bytes into param_2 from the shift register DAT_fff924dc (>>8 per byte),
refetching a fresh value via FUN_00074138 every 4 bytes. Callers=- (pointer table). [O]

#### 0x00074138 `diag_rng_step` — library
PRNG step: DAT_fff924d8 = (DAT_fff924d8 ^ DAT_fffff220) * 9 — timer-entangled LCG. Callers
0x000740D4/0x000740EC. [O]

## Unit summary

Region `r04_boot` (0x0006A7F6–0x00074138 plus cross-region cluster members). 220 clusters,
268 functions named. Kind counts: **logic 174, stub 43, data-accessor 41, library 8,
dispatch 2, unknown 0**. No function is classified kind `unknown`; the inference level of each
name is carried by the [O]/[I] markers in the entries above — the most inference-heavy names are
`sel5_*` (unknown 0..5 selector), `mem_apply_deferred_config`/`mem_request_switch_3ba9` (deferred
pointer reconfiguration), the `spi_monitor_*`/`spi_eval_fault_*` pair (device identity unproven),
`sol_*` (output-matrix semantics vs spec 12 unproven on bench) and `wdt_answer_compute`.

Identified modules in this unit:

- **EEPROM / data-flash record engine** (`eeprom_*`, ~45 functions): two scheduler-driven state
  machines (0x0006A86C on DAT_fff91b9e, 0x0006AB8E on DAT_fff91b9f), block scan of data flash at
  0x80100000 with marker 0x4001 and double-read headers (0x00070998), erase/select/page-write
  chain (0x0006FDB8/0x00071594/0x000720C8/0x0007237A), CRC verification (0x00070210/0x000702CA),
  request queue at DAT_fff920a0. Ties directly to spec 06 block model, spec 18 loader and
  `1034420067_eeprom_records.csv`; the literal "RB Plant Data Prog" record name (0x443A0) is a
  concrete anchor.
- **Diverse (value/complement) safe-RAM library** (`lib_diverse_*`, `lib_fill_complement`,
  `boot_march_*`, ring-buffer error log 0x0006BECC, event 0x221).
- **CAN driver** (`can_rx_*`/`can_tx_*`): 0x28-stride message objects, per-object health checks
  (CRC-8/counter/timing, spec 07), dual value/complement payload storage, two buses (poll 0/1),
  TX submit used by the 0x00069xxx packer family (spec 08).
- **UDS diagnostic server** (`diag_*`, ~50 functions): service objects + arbiter, SID+0x40
  positive responses, NRC 0x78/0x10/0x21 negatives, session table, timer-entangled PRNG for
  SecurityAccess (0x00074138: (seed ^ DAT_fffff220) * 9), hand-over to boot software
  (0x00072964 calling the boot-data vector at *(0x6010+0x34), event 0x2f2) — spec 15/17.
- **Program flow control** (`pfc_*`): checkpoint walker with event 0x228, shadow double-buffer
  at DAT_fff94880/0x94888 — companion to the watchdog (spec 05).
- **PWM-coded lever/range protocol** (`park_pwm_*`): period-encoded 6-bit values, echo verify,
  0..7 position hysteresis, diversity flags, event 0x226 — spec 14 context.
- **Solenoid/output staging** (`sol_*`): 3x8 value/mask/sent matrices at DAT_fff92508/2520/2538
  with locked commit — spec 12 shape.
- **Boot self-tests** (`boot_*`): RAM complement checks, +0x480000 mirror compare, march tests.

Unknown-kind entries: none (see note above on [I]-marked names).

Cross-region leads:

- The `dtc_report_on_mask` cluster has members in r08_app (0x00082EB4, 0x0008877C, 0x000887A8,
  0x0008D550) reporting events 0xbc2/0x9cf/0x9f6/0x96a; fault-catalog units should cross-check
  these ids against the DTC list (spec 13).
- `lib_ptr_thunk` cluster has 21 members in other regions (0x000C88FC, 0x000EAD9A,
  0x000F82C8–0x000F865C, 0x001277B6–0x00128C48) — trivial veneers; those units can skip
  re-analysis.
- `meas_read_entry_to_buf` (0x0006CBDA) is called by 0x00132142 (r10_app) — measurement-protocol
  units (spec 10) should cross-check the descriptor tables PTR_PTR_0006cd2c/0006cdbc.
- `can_tx_submit_object` (0x0006F000) is called by the 18 packers at 0x00069xxx (same region,
  different unit) and by registration pairs 0x0006F188-F1EC.
- The `spi_monitor_*` family resolves its primitives to 0x000A105E/0x000A107C/0x000A0FC4
  (r08_app) — SPI-device units (spec 03) should identify the register set 0x6f/0x70/0x78/0x88/
  0x89/0x7f.
- `boot_handover_reset` dereferences a vector at 0x6010+0x34 in the boot-data block (spec 06);
  boot-region units should confirm the target.
