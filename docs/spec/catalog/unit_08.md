# Unit 08 (r04_boot)

#### 0x00074154 `diag_2c_set_did_table` — data-accessor
Stores the dynamic-DID definition table pointer into RAM global `DAT_fff924e0` (3 instr setter).
Only caller is the UDS 0x2C service handler 0x000741F4; the table has a byte count at +3 and
8-byte entries (u16 DID at +0, type byte at +4, handler ptr at +4 within entry). Single-member
cluster. [O] Spec 15 service table (SID 0x2C handler 0x741F4, config 0x45728).

#### 0x000741B6 `diag_2c_clear_all_dids` — logic
Clears diag state bytes `DAT_fff93bcb/bcc`, then walks the dynamic-DID table from
`DAT_fff924e0` (count at +3) nulling each entry's handler pointer (+4 in 8-byte entries).
Called from init frame 0x0006CAD4 — resets all dynamically defined DIDs at start-up.
Single-member cluster. [O]

#### 0x000741F4 `diag_svc_2c_dyn_define_did` — dispatch
UDS service 0x2C (DynamicallyDefineDataIdentifier) handler: sub-function = req[0]&0x7f
(1 defineByID, 2 defineByAddress, 3 clear), bit 0x80 latched in `DAT_fff93bcd`; length checks
and DID match (0xF3xx) against the table via 0x00074154; mode 3/len 1 clears every entry.
NRCs 0x12/0x13/0x31 written into the request buffer, response codes 1/2/3. Calls the per-DID
handler slot param_1[1]/param_1[2]. Single-member cluster. [O] Spec 15 §2 row SID 0x2C.

#### 0x00074532 `tbl_range_lookup_by_variant` — logic
Range-scan: fetches a descriptor table (count u16 at +0x14) via 0x0006cda6, validates the u16
input through 0x00132142 (error to `DAT_fff93bce`; error 1 also copies `DAT_fff84882` to
`DAT_fff84906`), then scans 0xC-byte entries {u16 lo, u16 hi, type byte, handler ptr} and calls
`PTR_PTR_000745fc[type]` with the entry payload, returning u16. Callers unresolved (pointer
table). Looks like variant-conditioned value formatting for the diag stack. Single-member
cluster. [I]

#### 0x000748E6 `blk_assemble_next_segment` — logic
Block assembler step: from job struct param_1 (segment index at +0x1C, offset +0x12, dest base
param_1[3], max len u16 +0xA) and a descriptor table at `PTR_PTR_00074960`, takes the next
{len,src} pair and memcpy's it (0x0007d1cc) under the 0x0009a428/0x0009a3fc critical section
if it fits; advances offset; returns 3 when the block is full. Callers via pointer table.
Single-member cluster. [O] behavior, [I] purpose (diag write / data-block build).

#### 0x00074970 `diag_timeout_start_secs` — logic
Sets diag timeout budget `DAT_fff924e4` = seconds*1e6 (µs) and latches the free-running timer
`DAT_fffff220` into `DAT_fff924e8` (increments once if a condition via `PTR_FUN_00074a78`
holds). Called by 0x00081490 (UDS 0x27 SecurityAccess handler, spec 15) and 0x00074a88.
Single-member cluster. [O]

#### 0x000749AE `diag_timeout_remaining_us` — logic
Returns remaining diag-timeout µs: elapsed = `DAT_fffff220 - DAT_fff924e8` vs budget
`DAT_fff924e4`; re-latches the timer via `PTR_FUN_00074a7c` when expired or a condition via
`PTR_FUN_00074a78` is active. Sibling of 0x00074970 (same callers). Single-member cluster. [O]

#### 0x00074A04 `diag_timeout_poll` — logic
Periodic poll from init frame 0x0006caf8: when activity flags (`PTR_FUN_00074a80`,
`PTR_FUN_00074a78`) are set, consumes the timeout (0x000749ae), bumps comm-state counter
`DAT_fff82e54` while bit 0 set, and refreshes the budget from a mode byte via
`PTR_FUN_00074a84` unless mode == 1. Single-member cluster. [I]

#### 0x00074A88 `diag_comm_stop` — logic
If comm-state `DAT_fff82e54` bit 0 set, invokes `PTR_FUN_00074ac0(PTR_DAT_00074abc)` (disable
path), then clears diag state `DAT_fff93bcf`. Called from init frame 0x0006cad4 alongside
0x000741b6/0x00074aa8 — diag-session teardown at start-up. Single-member cluster. [I]

#### 0x00074AA8 `diag_flag_fff93bcf_clear` — logic
8-instr state clear: if `DAT_fff93bcf != -1` set it to 0 (0xFF marks "leave alone").
Called from init frame 0x0006cad4. Single-member cluster. [O]

#### 0x00074AC4 `diag_write_mode_set` — data-accessor
Setter for mode byte `DAT_fff84a05` (values 1/2/3 select the operation of the write state
machine 0x00074b0c). Called by 0x00081a98 = UDS 0x2E WriteDataByIdentifier handler (spec 15).
Single-member cluster. [O]

#### 0x00074ACC `diag_write_sm_start` — logic
Starts the write state machine: if not busy (0x00074be8: state in {0x22,0x33,0x44}) and mode
`DAT_fff84a05 != 0`, sets state `DAT_fff93bd0 = 0x33` and returns 1, else 0. Callers
0x00081a98 (UDS 0x2E) and 0x00082944. Single-member cluster. [O]

#### 0x00074AF4 `diag_write_sm_get_state` — data-accessor
Copies state byte `DAT_fff93bd0` to *param_1 and returns TRUE when the write state machine is
idle (0x00074be8 == 0). Callers 0x00081a98, 0x00082944 (0x2E write path). Single-member
cluster. [O]

#### 0x00074B0C `diag_write_sm_step` — dispatch
Write/program state machine driven from init frame 0x0006caf8. States 0x33 → 0x44 → 0x88 with
0x66/0x77 failure terminals; mode `DAT_fff84a05` 1/2/3 picks the operation thunks
(`0x00132316/0x001321fe/0x001321ee`, `0x00079764/0x000797fe/0x000797bc`, and 0x00070518 via
thunk for the retry limit `DAT_fff924ec`). Implements the asynchronous data write behind UDS
0x2E (erase/program/verify phases inferred). Single-member cluster. [O] structure, [I] phases.

#### 0x00074BE8 `diag_write_sm_busy` — logic
Returns 1 when state `DAT_fff93bd0` is 0x22, 0x33 ('3') or 0x44 ('D') — write in progress.
Called by 0x00074acc/0x00074af4. Single-member cluster. [O]

#### 0x00074C20 `diag_status_frame_periodic` — logic
200-tick supervisor (gated on `DAT_fff90628 - DAT_fff924f8`): when two condition functions and
`DAT_fff82e56 == 1` and a per-bus enable byte (`PTR_PTR_00074cd0[DAT_fff90ab8]`) allow, builds
an 8-byte frame at `DAT_fff924f0` = {07, 0x59, 0x0E, val, 24-bit LE value} with a CRC byte via
`PTR_FUN_00074cdc` and transmits it on channel 0x4F (`PTR_FUN_00074ce0/0x00074ce4`).
Periodic diagnostic status/heartbeat frame. Called from init frame 0x0006caf8. Single-member
cluster. [I]

#### 0x00074CE8 `stream_chan_init_all` — logic
Stores the stream-module descriptor in `DAT_fff924fc` (count at +1, schedule table +4,
config table +8, state array +0xC) and initialises every 0x28-byte channel state: clears
+0x19..+0x1D/+0x23 and sets +0x18 = 1. Callers via pointer table. Single-member cluster. [O]

#### 0x00074D28 `stream_chan_set_value` — logic
For channel idx: sets data ptr (+0xC) and length (+0x10), optional tag bytes (+0x1C/+0x1D),
then arms state +0x18 = 2 (len 0) or 0x82 under the 0x00098e8c/0x00098eac critical section.
Caller 0x000806f4. Single-member cluster. [O]

#### 0x00074D90 `stream_chan_clear_flags` — logic
Clears mask bits (param_2) of channel +0x18 inside the critical section and returns the old
byte. Caller 0x00080714. Single-member cluster. [O]

#### 0x00074DC4 `stream_chan_set_mode` — logic
Sets the low nibble of channel state +0x18 to param_2 and clears +0x19/+0x1A under the
critical section. Callers 0x00075788, 0x0008072a, 0x00074f5a. Single-member cluster. [O]

#### 0x00074DFE `stream_chan_take_flag` — data-accessor
Reads channel +0x18, clears bit 0x40, returns the previous bit 0x40 (consume-style flag).
Caller 0x00080738. Single-member cluster. [O]

#### 0x00074E32 `stream_chan_get_len` — data-accessor
Returns u16 at channel state +0x14. Caller 0x0008074c. Single-member cluster. [O]

#### 0x00074E48 `stream_chan_get_buf` — data-accessor
Returns the channel data pointer at state +0xC. Caller 0x00080760. Single-member cluster. [O]

#### 0x00074E5E `stream_chan_get_tag_a` — data-accessor
Returns tag byte at channel state +0x1C (set by 0x00074d28 param_4). Cluster of 2 —
0x00074E76 is the identical +0x1D accessor (same pattern, different offset). Callers
0x00080774 / 0x00080782. [O]

#### 0x00074E90 `stream_chan_commit` — logic
Commits a new value: memcpy of old/new data via thunk 0x00074f18, updates ptr/len, and when
+0x18 bit 3 was set clears it, timestamps +4/+8 with free-running `DAT_fffff220`, sets
+0x19 = 0x40, +0x1A = 0x40 under the critical section. Caller 0x00080798. Single-member
cluster. [O]

#### 0x00074F20 `stream_chan_get_state0` — data-accessor
Returns the u32 at stream-channel state +0x00 (first dword of the 0x28-byte state; +4 holds
the due-time, so this is likely a schedule/status word). Caller 0x000807b6 (API wrapper).
Single-member cluster. [O] access, [I] meaning.

#### 0x00074F34 `stream_chan_mark_1b` — logic
Sets stream-channel state byte +0x1B = 2 under the 0x00098e8c/0x00098eac critical section.
Caller 0x000807bc (gated on param & 0x80). Single-member cluster. [O]

#### 0x00074F5A `stream_chan_stop` — logic
Stops a stream channel: clears +0x19/+0x1A/+0x1B, bit 0x20 of +0x18 and +0x23 under the
critical section, then calls 0x00074dc4(idx,2) to set mode nibble 2. Caller 0x000807cc.
Single-member cluster. [O]

#### 0x00075788 `stream_chan_send_cmd` — logic
Builds and sends a stream control frame for channel idx: param_2 0/1/2 selects first byte
0x30/0x31/0x32 (start/next/stop); copies up to 3 config bytes, sets due-time +4 = cfg period
(`*(int*)cfg + DAT_fffff220`), decrements repeat count +0x21, and transmits via 0x0006cf44 →
`PTR_FUN_00075b44` (0x600+idx variant when cfg+8==1). Marks +0x19 = 0x40/1/0x80 on success.
Callers via pointer table. Single-member cluster. [O] behavior.

#### 0x00075918 `stream_periodic_send_chunk` — logic
Periodic chunk sender for channel idx: first byte = (+0x1e counter)|0x20, flushes the
leftover buffer `DAT_fff92500`/`DAT_fff92507` first, then up to ~6 bytes from data ptr
+0xC at offset +0x12; advances offset/total (+0x12/+0x16), computes the next due-time +8
from period byte +0x20 (<0x80: ×1000 µs, 0xF1–0xF9: ×100, else 0x1F018), handles repeat
+0x1F/+0x21, sets done flag +0x23, transmits via `PTR_FUN_00075b44`. Rolls everything back
on TX failure. Callers via pointer table. Single-member cluster. [O]

#### 0x00075BDC `diag_dtc_setting_state_clear` — stub
4-instr init-frame stub: `DAT_fff93bd4 = 0` (ControlDTCSetting state). Called from
0x0006cad4. Single-member cluster. [O]

#### 0x00075BE6 `diag_dtc_setting_control` — logic
Back-end of UDS SID 0x85 (handler 0x000809c4, spec 15): param_1 1/2 = on/off via
`PTR_FUN_00075c7c`/`PTR_FUN_00075c80` storing state in `DAT_fff93bd4`; refuses (NRC 0x22)
while the active-condition mask (`PTR_FUN_00075c74 & PTR_PTR_00075c78[DAT_fff90ab8]`) is
set; NRC 0x12 for bad sub-function, 0x13 for bad length. Single-member cluster. [O] Spec 15
SID 0x85.

#### 0x00075C56 `diag_dtc_setting_restore` — logic
If `DAT_fff93bd4 == 1` (setting off), re-enables DTC setting via `PTR_FUN_00075c7c(0)` and
clears the state. Called from 0x00075f86 during diagnostic session hand-over preparation.
Single-member cluster. [O]

#### 0x00075C84 `comm_src_cache_add` — logic
Maintains a 6-entry cache at `DAT_fff92550` (8 bytes/entry: age byte +0, 24-bit id at +4)
in `DAT_fff93bd5`-counted slots: finds a matching id, else takes the free/marker
(0xFF000000) or wraps ages (+7 each when an age hits 0xFF) and stores the new id with the
highest age+1. Caller 0x000a0db4. Purpose (recently-seen source ids) inferred. Single-member
cluster. [I]

#### 0x00075D20 `comm_src_cache_count` — data-accessor
Returns cached-entry count `DAT_fff93bd5`. Caller 0x00074c20 (status frame). Single-member
cluster. [O]

#### 0x00075D28 `comm_src_cache_pop_oldest` — logic
Pops the lowest-age cached id: writes the 24-bit id to param_1[0..2], replaces the slot
from the template at `PTR_DAT_00075ddc`, decrements the count. Caller 0x00074c20 (feeds the
status frame payload). Single-member cluster. [O]

#### 0x00075DB0 `comm_src_cache_init` — logic
Fills all 6 cache slots from the template at `PTR_DAT_00075ddc` and zeroes the count.
Called from init frame 0x0006cad4. Single-member cluster. [O]

#### 0x00075E2A `diag_rec_store_response` — logic
Stores a complete positive response into the boot request record at `0xFFF800B5`
(record +0x35 = 1): first encodes session timing P2/P2* via 0x00075f9e, then
+0x36 = len+1, +0x37 = SID|0x40, +0x38 = subfn&0x7f, payload copied from the request
buffer, then `(*DAT_00076040)()`. Called by 0x00075de0 on hand-over return code 0x17.
Single-member cluster. [O] Spec 06 §5a step 3.

#### 0x00075E9C `diag_rec_store_request` — logic
Stores only the request into the boot request record (`DAT_fff800b5` = +0x35 = 0):
+0x36 = len+1, +0x37 = request SID (param+0xB), +0x38 = sub-function (param+0xC), then
`(*DAT_00076040)()`. Called by 0x00080a10 (0x10 confirmation path) after the hand-over
state machine returns 0x18. Single-member cluster. [O] Spec 06 §5a.

#### 0x00075EC6 `diag_session_0x61_handler` — logic
DiagnosticSessionControl handler for session 0x61: requires session-state != 4, condition
mask (`PTR_FUN_00076048 & 0x36`) == 0 and `DAT_fff800b0` (record +0x30) == 0, then runs the
hand-over state machine 0x00076022; otherwise resets `DAT_fff84a06` and returns 5 (NRC
0x22 upstream). Callers via the session handler table 0x4660c. Single-member cluster. [O]
Spec 15 §3 row 2; spec 06 §5a.

#### 0x00075EFC `diag_session_0x02_handler` — logic
Programming-session (0x02) handler: same shape as 0x00075ec6 but the condition mask comes
from a variant-indexed table (`PTR_PTR_0007604c[DAT_fff90ab8]`). Hand-over target = boot
manager (spec 06 §5a step 4). Single-member cluster. [O] Spec 15 §3 row 4.

#### 0x00075F38 `diag_session_0x6b_handler` — logic
Session 0x6B handler: condition mask 0x39 variant, then the same 0x00076022 hand-over.
Hand-over target 1 or 2 depending on a configuration flag (spec 06 §5a step 4). Single-member
cluster. [O] Spec 15 §3 row 3.

#### 0x00075F86 `diag_session_prep_outputs` — logic
Hand-over preparation sequence called from 0x00075de0: 0x00075f6c, then output-preparation
calls through `PTR_FUN_00076054` (→ 0x14863a), `PTR_FUN_00076058` and `PTR_FUN_0007605c`,
including DTC-setting restore 0x00075c56. Matches "state 2 → prepare outputs" of spec 06
§5a step 3. Single-member cluster. [O] callees, [I] role.

#### 0x00075F9E `diag_session_timing_encode` — logic
Encodes the session timing pair from `DAT_fff92f9c` (P2 µs /1000 → ms, P2* µs /10000 →
10 ms) into response bytes +1..+4 of the request buffer, clamped to 0xFFFF; sets response
len (+8) = 5. Used by 0x00075e2a/0x00080a10. Single-member cluster. [O] Spec 06 §5a
(response data record).

#### 0x00076004 `diag_rec_store_timing` — logic
Copies the active session timing into the boot request record: `DAT_fff8008c` (record
+0x0C) = P2 and +0x10 via `PTR_FUN_00076060` = P2*, from `DAT_fff92f9c`; returns early
when no session is active. Caller 0x00075de0. Single-member cluster. [O] Spec 06 §5a
(+0x08–0x18 timing values).

#### 0x00076022 `diag_session_handover_sm` — logic
Hand-over state machine on `DAT_fff84a06`: state 0 polls readiness (`PTR_FUN_000760a8`;
1 → 0x13 response-pending, 2 → state 2), state 2 runs the prep calls
(`PTR_FUN_000760ac`, `PTR_FUN_000760b0(0)` → 0x73f76) → state 3, returns 0x18; state 3
finishes via `PTR_FUN_000760b4` and returns 0x10. Called by the three session handlers.
Single-member cluster. [O] Spec 06 §5a step 3.

#### 0x000760B8 `diag_mem_read_state_init` — stub
Init-frame stub clearing the memory-read job state (`DAT_fff92580`, `DAT_fff9384e`,
`DAT_fff92584`, `DAT_fff92588`). Called from 0x0006cad4. Single-member cluster. [O]

#### 0x000760D0 `diag_mem_read_start` — logic
Arms a block memory-read job (address `DAT_fff92584`, dest ptr `DAT_fff92588`, remaining
length u16 `DAT_fff9384e`, state `DAT_fff92580` = 5) when idle (state 0/1), then runs the
first chunk step via 0x000760f8. Caller 0x000812d4 = UDS SID 0x23 ReadMemoryByAddress
(spec 15). Single-member cluster. [O]

#### 0x000760F8 `diag_mem_read_step` — logic
Read-job step: caps the chunk at 0x400 bytes, permission-checks the address range via
`PTR_FUN_00076168` (→ 0x0007d3cc, the region-table check shared with the measurement
protocol, spec 10 §2), memcpy's via 0x0007d1cc, advances address/dest/remaining; returns
5 busy, 1 done (state → 1), 2 denied (state → 0). Callers 0x000760d0/0x000760cc.
Single-member cluster. [O]

#### 0x00076170 `diag_handover_flag_get` — data-accessor
3-instr getter for `DAT_fff84a07` (byte next to the hand-over state `DAT_fff84a06`).
Callers 0x0014fc5c and 0x00133c4e (application). Likely the "hand-over active / reset
pending" flag. Single-member cluster. [O] access, [I] meaning.

#### 0x00076178 `cond_latch_fff84a07` — logic
Reads channel-status function 0x00094ea0(3) and latches `DAT_fff84a07 = (res & 1) == 1`
(the byte returned by 0x00076170). Called by 0x0006e882 (init area). Single-member cluster. [O]
access, [I] meaning.

#### 0x00076190 `cond_get_fff84a08` — data-accessor
3-instr getter for condition byte `DAT_fff84a08`. Callers 0x0014fc5c, 0x00133c4e
(application). Single-member cluster. [O]

#### 0x00076198 `cond_latch_fff84a08` — logic
Same shape as 0x00076178 but channel 0x43 of 0x00094ea0, latching `DAT_fff84a08`.
Called by 0x0006e882. Single-member cluster. [O]

#### 0x000761B0 `cond_poll_fff94097` — data-accessor
Refreshes cached byte `DAT_fff94097` from `(*DAT_00076254)()` and returns it. Called by the
status-frame sender 0x00074c20 (feeds its payload byte) and by application 0x0014fc5c /
0x00133c4e. Single-member cluster. [O]

#### 0x000761C8 `vec_call_DAT_0007625c` — stub
3-instr tail-call thunk: invokes the RAM function pointer `DAT_0007625c` (→ 0x000821a4).
Callers 0x0014222e, 0x00142112. Single-member cluster. [O]

#### 0x000761CE `input_pair_status_encode` — logic
Reads two u16 values (channels 0/1 via `PTR_FUN_00076260`) and two status chars
(`PTR_FUN_00076264`); selector param 1/2 swaps to the second channel; maps to codes
0 / 1 (status==1) / 4 (value bit 1) / 0x80 (bad selector). Looks like a two-sensor position
encoder for the measurement stack (caller 0x001513f8). Single-member cluster. [O] behavior,
[I] purpose.

#### 0x00076234 `tbl10_get_ptr` — data-accessor
Bounds-checked (idx < 10) pointer fetch from the 8-byte-entry table at `PTR_PTR_00076268`.
Caller 0x0015531a — the measurement-protocol buffer setup (spec 10 §1). Single-member
cluster. [O] Spec 10 cross-ref.

#### 0x0007624A `thunk_to_0x000761b0` — stub
2-instr duplicate of 0x000761b0 (refresh + return `DAT_fff94097`). Caller 0x0006d6e0.
Single-member cluster. [O]

#### 0x0007626C `flag_state_eq_2_get` — data-accessor
Returns TRUE when `PTR_FUN_000762bc()` (thunk to 0x00073f7c) == 2. Callers 0x00133476,
0x0014fc5c. Semantics of the polled state unknown. Single-member cluster. [I]

#### 0x0007627C `tbl_lookup_by_counter` — logic
Gets a counter from `PTR_FUN_000762c4`; when it is below the table max at
`PTR_PTR_000762c0 + 0x10` and (counter != 0 or `DAT_fff924ce` or `PTR_FUN_000762c8` set),
returns the byte at `*(u8*)(*table + counter)`, else 0 — value-by-running-index lookup.
Caller 0x0014fc5c. Single-member cluster. [I]

#### 0x000762EC `boot_ram_clear_block_a` — logic
Init-hook: when flag param_1 & 0x20, memsets 0 over the RAM block defined by the ROM
pointer pair (`PTR_DAT_00076414`, length `*0x76418 - *0x76414`), then returns 1. Caller
0x00139ce4 (module init). Single-member cluster. [O]

#### 0x00076308 `boot_cond_call_clear_ids` — logic
Init-hook: when flag param_1 & 0x20, calls the thunk to 0x00076588 (clears the identity
buffers); returns 1. Caller 0x00139ce4. Single-member cluster. [O]

#### 0x0007631A `boot_diag_active_latch` — logic
Init-hook: flag 0x20 → memset the RAM block (`PTR_DAT_00076424` pair); flags 0x20/2/4 →
set diag-active flag `DAT_fff82e56` = 1 (the gate of the 0x4F status frame, 0x00074c20).
Caller 0x00139ce4. Single-member cluster. [O]

#### 0x0007634E `boot_ident_variant_init` — logic
Init-hook: like 0x0007631a (memset `PTR_DAT_0007642c` block), then copies the 10-char part
number string `"68345917AC"` (at 0x00076434) to `DAT_fff82fb7`, loads nine variant bytes
from double-pointer tables (`PTR_PTR_0007643c/40/44/48/4c/50/54/58/5c` indexed by
`DAT_fff90ab8`) into `DAT_fff82fc1…fc9`, and calls `PTR_FUN_00076460(0)`. Caller
0x00139ce4. Single-member cluster. [O]

#### 0x000763F4 `boot_ram_block_a_size` — data-accessor
4-instr size getter: `*0x76418 - *0x76414` (RAM block A length). Caller 0x00139df4 — the
workspace-size query of the module init. Single-member cluster. [O]

#### 0x000763FC `boot_ram_block_b_size` — data-accessor
Same pattern with the pointer pair at 0x76468/0x76464 (note reversed order, returns
`*0x76464 - *0x76468`). Caller 0x00139df4. Single-member cluster. [O]

#### 0x00076404 `boot_ram_block_c_size` — data-accessor
`*0x76428 - *0x76424` — size of the block cleared by 0x0007631a. Caller 0x00139df4.
Single-member cluster. [O]

#### 0x0007640C `boot_ram_block_d_size` — data-accessor
`*0x76430 - *0x7642c` — size of the block cleared by 0x0007634e. Caller 0x00139df4.
Single-member cluster. [O]

#### 0x00076480 `tbl10_get_ptr_and_flag` — data-accessor
Bounds-checked (idx < 10) fetch from the 8-byte-entry table at `PTR_LAB_00076500`:
returns the pointer and writes the entry's flag byte to *param_2 (0/null when out of
range). Caller 0x00134a28. Companion of 0x00076234. Single-member cluster. [O]

#### 0x000764A0 `ident_get_part_or_id` — logic
Selector 0: copies the 10-byte part number from `DAT_fff82fb7` to `DAT_fff9258c`, zeroes
6 trailing bytes, returns buffer ptr with len 0xF; selector 1: returns `&DAT_fff93bdc`
with len 0x11 (the 17-byte identity buffer); other: null/0. Caller 0x000a9ede. Single-member
cluster. [O]

#### 0x0007650E `ident_field_select` — dispatch
Maps selector 1…5 onto identity shadow buffers: 1 → `DAT_fff93bed` (len 0x11), 2 →
`DAT_fff93bdc` (len 0x11), 3 → `DAT_fff93bfe` (len 3), 4 → `DAT_fff93bdb` (len 1),
5 → `DAT_fff93c02` (len 1); writes {ptr, len} into param_1 fields, returns 1; unknown
selector clears them and returns 2. Callers via pointer table. Single-member cluster. [O]

#### 0x00076568 `ident_module_reset` — logic
Reset hook: calls `PTR_FUN_00076580()`, `PTR_FUN_00076584()` then clears `DAT_fff93c03`.
Callers via pointer table (companion of 0x00076588/0x000765de). Single-member cluster. [I]

#### 0x00076588 `ident_buffers_clear` — logic
Clears the identity working area: `DAT_fff82e14 = 1`, 0xFF-fills the two 17-byte buffers
`DAT_fff82e15` and `DAT_fff82e26` (VIN-sized), zeroes `DAT_fff82e37/38…3a`, 0x18 bytes at
`DAT_fff82e3b…` and `DAT_fff82e53`, then `PTR_FUN_000766c0()`. Callers 0x00076568, 0x00076508.
Single-member cluster. [O]

#### 0x000765DE `ident_commit_shadow` — logic
Under the 0x0009a428/0x0009a3fc critical section (via `PTR_FUN_000766c4/c8`): copies the
two 17-byte buffers `DAT_fff82e15/26` into shadows `DAT_fff93bed/93bdc`, `DAT_fff82e37`
into `DAT_fff93c02`, the 3 bytes `DAT_fff82e38…` into `DAT_fff93bfe…` and packs them into
24-bit `DAT_fff9259c`, clears `DAT_fff93bdb`. Caller 0x00076568. Single-member cluster. [O]

#### 0x0007665E `ident_restore_from_shadow` — logic
Inverse of 0x000765de under the same critical section: shadow buffers
`DAT_fff93bed/93bdc/93c02/93bfe…` back into the working area `DAT_fff82e15/26/37/38…`,
sets `DAT_fff82e14 = 1`. Callers via pointer table. Single-member cluster. [O]

#### 0x000766CC `meas_flag_abe_bit2_get` — data-accessor
Returns `(DAT_fff90abe & 2) != 0`. Part of the snapshot-getter bank consumed by the giant
measurement provider 0x00144108 (5322 instr) and 0x0014fc5c. Single-member cluster. [O]
Spec 10 context (measurement index providers).

#### 0x000766D8 `meas_flag_abe_bit4_get` — data-accessor
Returns `(DAT_fff90abe & 4) != 0`. Same getter bank as 0x000766cc; caller 0x00144108.
Single-member cluster. [O]

#### 0x000766E4 `meas_flag_abe_bit8_get` — data-accessor
Returns `(DAT_fff90abe & 8) != 0`. Same getter bank; caller 0x00144108. Single-member
cluster. [O]

#### 0x000766F0 `meas_flag_abe_bit10_get` — data-accessor
Returns `(DAT_fff90abe & 0x10) != 0`. Cluster of 2 — member 0x000766FC tests bit 0x80 of
the same snapshot byte (constant difference). Callers 0x00144108 / 0x00144108+0x00132d8e.
[O]

#### 0x00076708 `meas_get_via_0x000696b0` — stub
6-instr wrapper: calls a provider through a ROM function pointer and returns its u16.
Rep target 0x000696b0 = `(DAT_fff90abf & 0xf) << 8 | DAT_fff90ac0` (12-bit composed
signal). Cluster of 35 — every member is the same wrapper around a different provider
(0x000696xx–0x00069cxx bank plus cross-region copies at 0x000f69c6/0x000f6cc8/
0x000f8618/0x000f8624 → r0c targets and 0x00128bd4/0x00128be0 → r10 targets); per-member
names encode the target. All feed the measurement provider 0x00144108. [O] Spec 10 §5
(index of 470 measurement providers).

#### 0x00076714 `meas_snap_acc_low3_get` — data-accessor
Returns `DAT_fff90acc & 7`. Callers 0x00144108, 0x00132d8e, 0x0014fc5c, 0x001356b4,
0x00133c4e. Single-member cluster. [O]

#### 0x0007671E `meas_snap_adc_low3_get` — data-accessor
Returns `(DAT_fff90adc & 7) | 0xf8` (low 3 bits, upper bits forced to 1). Callers
0x0014fc5c, 0x00133c4e. Single-member cluster. [O]

#### 0x0007672A `meas_snap_acc_bit6_get` — data-accessor
Returns `(DAT_fff90acc & 0x40) != 0`. Callers 0x00144108, 0x00132d8e, 0x0014fc5c.
Single-member cluster. [O]

#### 0x00076736 `meas_snap_acd_bit2_get` — data-accessor
Returns `(DAT_fff90acd & 4) != 0`. Callers 0x00144108, 0x00132d8e, 0x0014fc5c. Single-member
cluster. [O]

#### 0x00076742 `meas_get_via_0x000696dc` — stub
Same 6-instr provider wrapper as 0x00076708 (separate cluster, target `DAT_00076a74` →
0x000696dc). Callers 0x00144108, 0x00133740, 0x001517e0, 0x00135362. Single-member
cluster. [O]

#### 0x0007674E `meas_snap_aea_get` — data-accessor
Returns byte `DAT_fff90aea`. Caller 0x00144108. Single-member cluster. [O]

#### 0x00076758 `meas_snap_afb_bits3_5_get` — data-accessor
Returns `(DAT_fff90afb >> 3) & 7`. Callers 0x00144108, 0x00132d8e, 0x0014fc5c. Single-member
cluster. [O]

#### 0x00076768 `meas_snap_afb_top2_get` — data-accessor
Returns `DAT_fff90afb >> 6` (top 2 bits). Callers 0x00144108, 0x00132d8e, 0x0014fc5c.
Single-member cluster. [O]

#### 0x00076776 `meas_snap_b08_low5_get` — data-accessor
Returns `DAT_fff90b08 & 0x1f`. Caller 0x00144108. Single-member cluster. [O]

#### 0x00076780 `meas_snap_b15_get` — data-accessor
Returns byte `DAT_fff90b15`. Callers 0x00144108, 0x00132d8e, 0x0014fc5c. Single-member
cluster. [O]

#### 0x0007678A `meas_snap_b18_bits4_6_get` — data-accessor
Returns `(DAT_fff90b18 >> 4) & 7`. Caller 0x00144108. Single-member cluster. [O]

#### 0x0007679A `meas_snap_b1b_get` — data-accessor
Returns byte `DAT_fff90b1b`. Callers 0x00144108, 0x00132d8e, 0x0014fc5c. Single-member
cluster. [O]

#### 0x000767A4 `meas_snap_b27_get` — data-accessor
Returns byte `DAT_fff90b27`. Callers 0x00144108, 0x00132d8e, 0x0014fc5c. Single-member
cluster. [O]

#### 0x000767AE `meas_snap_b34_bits1_3_get` — data-accessor
Returns `(DAT_fff90b34 >> 1) & 7`. Callers 0x00144108, 0x00132d8e, 0x0014fc5c. Single-member
cluster. [O]

#### 0x000767BC `meas_snap_b48_bits2_5_get` — data-accessor
Returns `(DAT_fff90b48 >> 2) & 0xf`. Callers 0x00144108, 0x00132d8e, 0x0014fc5c. Single-member
cluster. [O]

#### 0x000767D6 `meas_snap_b5a_low4_get` — data-accessor
Returns `DAT_fff90b5a & 0xf`. Caller 0x00144108. Single-member cluster. [O]

#### 0x000767E0 `meas_snap_b5a_high4_get` — data-accessor
Returns `DAT_fff90b5a >> 4`. Caller 0x00144108. Single-member cluster. [O]

#### 0x000767EE `meas_snap_b5b_get` — data-accessor
Returns byte `DAT_fff90b5b`. Caller 0x00144108. Single-member cluster. [O]

#### 0x0007681C `meas_get_or_0xe000` — logic
Calls the provider at `DAT_00076a88` (→ 0x000697b8) and returns `u16 | 0xe000` — marks the
top 3 bits as a "no-value/invalid" tag for the measurement protocol. Caller 0x0014fc5c.
Single-member cluster. [O] tag meaning [I].

#### 0x0007682E `meas_snap_b68_top2_get` — data-accessor
Returns `DAT_fff90b68 >> 6`. Callers 0x00144108, 0x0014fab4, 0x0014fc5c. Single-member
cluster. [O]

#### 0x00076848 `meas_snap_b6a_high4_get` — data-accessor
Returns `DAT_fff90b6a >> 4`. Measurement-provider getter bank (0x00144108). Single-member
cluster. [O]

#### 0x00076856 `meas_snap_b6b_get` — data-accessor
Returns byte `DAT_fff90b6b`. Getter bank. Single-member cluster. [O]

#### 0x00076860 `meas_snap_b7c_bit6_get` — data-accessor
Returns `(DAT_fff90b7c & 0x40) != 0`. Cluster of 2 — member 0x0007686c tests bit 0x80 of
the same byte. Getter bank. [O]

#### 0x00076884 `meas_snap_b7e_get` — data-accessor
Returns byte `DAT_fff90b7e`. Getter bank. Single-member cluster. [O]

#### 0x0007688E `meas_snap_b7f_bit5_get` — data-accessor
Returns `(DAT_fff90b7f & 0x20) != 0`. Cluster of 3 — members 0x0007689a/0x000768a6 test
bits 0x40/0x80 of the same byte. Getter bank. [O]

#### 0x000768BE `meas_snap_b81_bit1_get` — data-accessor
Returns `(DAT_fff90b81 & 2) != 0`. Getter bank. Single-member cluster. [O]

#### 0x000768CA `meas_snap_b81_bit2_get` — data-accessor
Returns `(DAT_fff90b81 & 4) != 0`. Getter bank. Single-member cluster. [O]

#### 0x000768D6 `meas_snap_b81_bit3_get` — data-accessor
Returns `(DAT_fff90b81 & 8) != 0`. Getter bank. Single-member cluster. [O]

#### 0x000768E2 `meas_snap_b81_high4_get` — data-accessor
Returns `DAT_fff90b81 >> 4`. Getter bank. Single-member cluster. [O]

#### 0x000768F0 `meas_snap_b82_get` — data-accessor
Returns byte `DAT_fff90b82`. Getter bank. Single-member cluster. [O]

#### 0x000768FA `meas_snap_b8c_bit7_get` — data-accessor
Returns `(DAT_fff90b8c & 0x80) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076906 `meas_snap_b9c_bit7_get` — data-accessor
Returns `((DAT_fff90b9c & 0x80) != 0) | 0xfe` — bit 7 folded into a 0xfe/0xff byte.
Getter bank. Single-member cluster. [O]

#### 0x00076920 `meas_snap_b8e_top3_get` — data-accessor
Returns `DAT_fff90b8e >> 5`. Getter bank. Single-member cluster. [O]

#### 0x0007693A `meas_get_or_0xe000_via_698d4` — stub
Provider wrapper: u16 from `PTR_FUN_00076aa0` (→ 0x000698d4), ORed with 0xe000 (no-value
tag). Cluster of 2 — member 0x00076cfc calls 0x00069a08 and ORs 0xf800. Getter bank. [O]

#### 0x0007694C `meas_snap_b90_bit4_get` — data-accessor
Returns `(DAT_fff90b90 & 0x10) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076958 `meas_snap_b91_get` — data-accessor
Returns byte `DAT_fff90b91`. Getter bank. Single-member cluster. [O]

#### 0x00076962 `meas_snap_b92_low4_get` — data-accessor
Returns `DAT_fff90b92 & 0xf`. Getter bank. Single-member cluster. [O]

#### 0x0007696C `meas_snap_ba2_low4_get` — data-accessor
Returns `(DAT_fff90ba2 & 0xf) | 0xf0`. Getter bank. Single-member cluster. [O]

#### 0x00076978 `meas_snap_b92_high4_get` — data-accessor
Returns `DAT_fff90b92 >> 4`. Getter bank. Single-member cluster. [O]

#### 0x00076986 `meas_snap_b93_get` — data-accessor
Returns byte `DAT_fff90b93`. Getter bank. Single-member cluster. [O]

#### 0x00076990 `meas_snap_ba6_bit4_get` — data-accessor
Returns `(DAT_fff90ba6 & 0x10) != 0`. Getter bank. Single-member cluster. [O]

#### 0x0007699C `meas_snap_ba8_get` — data-accessor
Returns byte `DAT_fff90ba8`. Getter bank. Single-member cluster. [O]

#### 0x000769A6 `meas_snap_baa_high4_get` — data-accessor
Returns `DAT_fff90baa >> 4`. Getter bank. Single-member cluster. [O]

#### 0x000769B4 `meas_snap_bab_get` — data-accessor
Returns byte `DAT_fff90bab`. Getter bank. Single-member cluster. [O]

#### 0x000769BE `meas_snap_bb5_low3_get` — data-accessor
Returns `DAT_fff90bb5 & 7`. Getter bank. Single-member cluster. [O]

#### 0x000769C8 `meas_snap_bb8_bits4_5_get` — data-accessor
Returns `(DAT_fff90bb8 >> 4) & 3`. Getter bank (see 0x000766cc). Single-member cluster. [O]

#### 0x000769D8 `meas_snap_bc4_get` — data-accessor
Returns byte `DAT_fff90bc4`. Getter bank. Single-member cluster. [O]

#### 0x000769E2 `meas_snap_bc5_get` — data-accessor
Returns byte `DAT_fff90bc5`. Getter bank. Single-member cluster. [O]

#### 0x000769EC `meas_snap_bc6_get` — data-accessor
Returns byte `DAT_fff90bc6`. Getter bank. Single-member cluster. [O]

#### 0x000769F6 `meas_snap_bc9_bit7_get` — data-accessor
Returns `(DAT_fff90bc9 & 0x80) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076A02 `meas_snap_bdb_bits2_4_get` — data-accessor
Returns `(DAT_fff90bdb >> 2) & 7`. Getter bank. Single-member cluster. [O]

#### 0x00076A10 `meas_snap_be4_bit5_get` — data-accessor
Returns `(DAT_fff90be4 & 0x20) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076A1C `meas_snap_be4_bit7_get` — data-accessor
Returns `(DAT_fff90be4 & 0x80) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076A28 `meas_snap_be5_get` — data-accessor
Returns byte `DAT_fff90be5`. Getter bank. Single-member cluster. [O]

#### 0x00076A32 `meas_snap_bf7_bit7_get` — data-accessor
Returns `(DAT_fff90bf7 & 0x80) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076A3E `meas_snap_c04_bit5_get` — data-accessor
Returns `(DAT_fff90c04 & 0x20) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076A56 `meas_snap_c06_bit5_get` — data-accessor
Returns `(DAT_fff90c06 & 0x20) != 0`. Cluster of 2 — member 0x00076a62 tests bit 0x80 of
the same byte. Getter bank. [O]

#### 0x00076AA8 `meas_snap_c14_bit3_get` — data-accessor
Returns `(DAT_fff90c14 & 8) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076AB4 `meas_snap_c14_bit5_get` — data-accessor
Returns `(DAT_fff90c14 & 0x20) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076AC0 `meas_snap_c15_get` — data-accessor
Returns byte `DAT_fff90c15`. Getter bank. Single-member cluster. [O]

#### 0x00076ACA `meas_snap_c16_low3_get` — data-accessor
Returns `DAT_fff90c16 & 7`. Getter bank. Single-member cluster. [O]

#### 0x00076AD4 `meas_snap_c16_bits3_5_get` — data-accessor
Returns `(DAT_fff90c16 >> 3) & 7`. Getter bank. Single-member cluster. [O]

#### 0x00076AE4 `meas_snap_c17_high4_get` — data-accessor
Returns `DAT_fff90c17 >> 4`. Getter bank. Single-member cluster. [O]

#### 0x00076AFE `meas_snap_c24_get` — data-accessor
Returns byte `DAT_fff90c24` (sign-extended cast). Getter bank. Single-member cluster. [O]

#### 0x00076B08 `meas_snap_c28_low3_get` — data-accessor
Returns `DAT_fff90c28 & 7`. Getter bank. Single-member cluster. [O]

#### 0x00076B12 `meas_snap_c34_bit0_get` — data-accessor
Returns `(DAT_fff90c34 & 1) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076B1E `meas_snap_c34_bit1_get` — data-accessor
Returns `(DAT_fff90c34 & 2) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076B2A `meas_snap_c34_bit2_get` — data-accessor
Returns `(DAT_fff90c34 & 4) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076B36 `meas_snap_c34_bit3_get` — data-accessor
Returns `(DAT_fff90c34 & 8) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076B42 `meas_snap_c34_bit5_get` — data-accessor
Returns `(DAT_fff90c34 & 0x20) != 0`. Cluster of 2 — member 0x00076b4e tests bit 0x40 of
the same byte. Getter bank. [O]

#### 0x00076B5A `meas_snap_c36_get` — data-accessor
Returns byte `DAT_fff90c36`. Getter bank. Single-member cluster. [O]

#### 0x00076B64 `meas_snap_c37_get` — data-accessor
Returns byte `DAT_fff90c37`. Getter bank. Single-member cluster. [O]

#### 0x00076B6E `meas_snap_c44_low2_get` — data-accessor
Returns `DAT_fff90c44 & 3`. Getter bank. Single-member cluster. [O]

#### 0x00076B78 `meas_get_via_DAT_00076e4c` — stub
6-instr provider wrapper returning u16 from RAM function pointer `DAT_00076e4c`.
Getter bank. Single-member cluster. [O]

#### 0x00076B84 `meas_snap_c58_low5_get` — data-accessor
Returns `DAT_fff90c58 & 0x1f`. Getter bank. Single-member cluster. [O]

#### 0x00076B9A `meas_snap_c66_low3_get` — data-accessor
Returns `DAT_fff90c66 & 7`. Getter bank. Single-member cluster. [O]

#### 0x00076BA4 `meas_snap_c75_bits1_3_get` — data-accessor
Returns `(DAT_fff90c75 >> 1) & 7`. Getter bank. Single-member cluster. [O]

#### 0x00076BB2 `meas_snap_c75_high4_get` — data-accessor
Returns `DAT_fff90c75 >> 4`. Getter bank. Single-member cluster. [O]

#### 0x00076BC0 `meas_snap_c76_get` — data-accessor
Returns byte `DAT_fff90c76`. Getter bank. Single-member cluster. [O]

#### 0x00076BCA `meas_snap_c84_low4_get` — data-accessor
Returns `DAT_fff90c84 & 0xf`. Getter bank. Single-member cluster. [O]

#### 0x00076BD4 `meas_snap_c84_bit6_get` — data-accessor
Returns `(DAT_fff90c84 & 0x40) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076BE0 `meas_snap_c94_bits2_3_get` — data-accessor
Returns `(DAT_fff90c94 >> 2) & 3`. Getter bank. Single-member cluster. [O]

#### 0x00076BEE `meas_snap_ca4_bits2_3_get` — data-accessor
Returns `((DAT_fff90ca4 >> 2) & 3) | 0xfc`. Getter bank. Single-member cluster. [O]

#### 0x00076BFE `meas_snap_c94_bit5_get` — data-accessor
Returns `(DAT_fff90c94 & 0x20) != 0`. Cluster of 3 — members 0x00076c0a/0x00076c16 test
bits 0x40/0x80 of the same byte. Getter bank. [O]

#### 0x00076C22 `meas_snap_c95_bits2_3_get` — data-accessor
Returns `(DAT_fff90c95 >> 2) & 3`. Getter bank. Single-member cluster. [O]

#### 0x00076C30 `meas_snap_ca5_bits2_3_get` — data-accessor
Returns `((DAT_fff90ca5 >> 2) & 3) | 0xfc`. Getter bank. Single-member cluster. [O]

#### 0x00076C40 `meas_snap_c95_bits4_5_get` — data-accessor
Returns `(DAT_fff90c95 >> 4) & 3`. Getter bank. Single-member cluster. [O]

#### 0x00076C50 `meas_snap_c95_bit6_get` — data-accessor
Returns `(DAT_fff90c95 & 0x40) != 0`. Cluster of 2 — member 0x00076c5c tests bit 0x80.
Getter bank. [O]

#### 0x00076C68 `meas_snap_c96_low3_get` — data-accessor
Returns `DAT_fff90c96 & 7`. Getter bank. Single-member cluster. [O]

#### 0x00076C72 `meas_snap_c96_bits3_5_get` — data-accessor
Returns `(DAT_fff90c96 >> 3) & 7`. Getter bank. Single-member cluster. [O]

#### 0x00076C82 `meas_snap_c96_bit6_get` — data-accessor
Returns `(DAT_fff90c96 & 0x40) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076C8E `meas_snap_c97_bits2_5_get` — data-accessor
Returns `(DAT_fff90c97 >> 2) & 0xf`. Getter bank. Single-member cluster. [O]

#### 0x00076CA8 `meas_snap_c9a_high4_get` — data-accessor
Returns `DAT_fff90c9a >> 4`. Getter bank. Single-member cluster. [O]

#### 0x00076CB6 `meas_snap_c9b_get` — data-accessor
Returns byte `DAT_fff90c9b`. Getter bank. Single-member cluster. [O]

#### 0x00076CC0 `meas_snap_cac_bit6_get` — data-accessor
Returns `(DAT_fff90cac & 0x40) != 0`. Cluster of 2 — member 0x00076ccc tests bit 0x80 of
the same byte. Getter bank. [O]

#### 0x00076CD8 `meas_snap_cb2_high4_get` — data-accessor
Returns `DAT_fff90cb2 >> 4`. Getter bank. Single-member cluster. [O]

#### 0x00076CE6 `meas_snap_cb3_get` — data-accessor
Returns byte `DAT_fff90cb3`. Getter bank. Single-member cluster. [O]

#### 0x00076D0E `meas_snap_cc2_high4_get` — data-accessor
Returns `DAT_fff90cc2 >> 4`. Getter bank. Single-member cluster. [O]

#### 0x00076D1C `meas_snap_cc3_get` — data-accessor
Returns byte `DAT_fff90cc3`. Getter bank. Single-member cluster. [O]

#### 0x00076D26 `meas_snap_cd7_get` — data-accessor
Returns byte `DAT_fff90cd7`. Getter bank. Single-member cluster. [O]

#### 0x00076D30 `meas_snap_cd8_get` — data-accessor
Returns byte `DAT_fff90cd8`. Getter bank. Single-member cluster. [O]

#### 0x00076D3A `meas_snap_cd9_get` — data-accessor
Returns byte `DAT_fff90cd9`. Getter bank. Single-member cluster. [O]

#### 0x00076D44 `meas_snap_cdb_get` — data-accessor
Returns byte `DAT_fff90cdb`. Getter bank. Single-member cluster. [O]

#### 0x00076D4E `meas_snap_ce4_get` — data-accessor
Returns byte `DAT_fff90ce4`. Getter bank. Single-member cluster. [O]

#### 0x00076D58 `meas_snap_ce5_get` — data-accessor
Returns byte `DAT_fff90ce5`. Getter bank. Single-member cluster. [O]

#### 0x00076D62 `meas_snap_cf4_top2_get` — data-accessor
Returns `DAT_fff90cf4 >> 6`. Getter bank. Single-member cluster. [O]

#### 0x00076D7C `meas_snap_cf6_top2_get` — data-accessor
Returns `DAT_fff90cf6 >> 6`. Getter bank. Single-member cluster. [O]

#### 0x00076D96 `meas_snap_cf8_top2_get` — data-accessor
Returns `DAT_fff90cf8 >> 6`. Getter bank. Single-member cluster. [O]

#### 0x00076DB0 `meas_snap_cfa_top2_get` — data-accessor
Returns `DAT_fff90cfa >> 6`. Getter bank. Single-member cluster. [O]

#### 0x00076DE2 `meas_snap_d0a_high4_get` — data-accessor
Returns `DAT_fff90d0a >> 4`. Getter bank. Single-member cluster. [O]

#### 0x00076DF0 `meas_snap_d0b_get` — data-accessor
Returns byte `DAT_fff90d0b`. Getter bank. Single-member cluster. [O]

#### 0x00076DFA `meas_snap_d22_bits4_5_get` — data-accessor
Returns `(DAT_fff90d22 >> 4) & 3`. Getter bank. Single-member cluster. [O]

#### 0x00076E0A `meas_snap_d31_top2_get` — data-accessor
Returns `DAT_fff90d31 >> 6`. Getter bank. Single-member cluster. [O]

#### 0x00076E18 `meas_snap_d32_top3_get` — data-accessor
Returns `DAT_fff90d32 >> 5`. Getter bank. Single-member cluster. [O]

#### 0x00076E26 `meas_snap_d3f_low3_get` — data-accessor
Returns `DAT_fff90d3f & 7`. Getter bank. Single-member cluster. [O]

#### 0x00076E30 `meas_snap_d4c_bit6_get` — data-accessor
Returns `(DAT_fff90d4c & 0x40) != 0`. Cluster of 2 — member 0x00076e3c tests bit 0x80 of
the same byte. Getter bank. [O]

#### 0x00076E90 `meas_snap_d52_high4_get` — data-accessor
Returns `DAT_fff90d52 >> 4`. Getter bank. Single-member cluster. [O]

#### 0x00076E9E `meas_snap_d53_get` — data-accessor
Returns byte `DAT_fff90d53`. Getter bank. Single-member cluster. [O]

#### 0x00076EA8 `meas_snap_d5d_low2_get` — data-accessor
Returns `DAT_fff90d5d & 3`. Getter bank. Single-member cluster. [O]

#### 0x00076EB2 `meas_snap_d62_low2_get` — data-accessor
Returns `DAT_fff90d62 & 3`. Getter bank. Single-member cluster. [O]

#### 0x00076EBC `meas_snap_d71_low4_get` — data-accessor
Returns `DAT_fff90d71 & 0xf`. Getter bank. Single-member cluster. [O]

#### 0x00076EC6 `meas_snap_d72_top2_get` — data-accessor
Returns `DAT_fff90d72 >> 6`. Getter bank. Single-member cluster. [O]

#### 0x00076ED4 `meas_snap_d7c_bit0_get` — data-accessor
Returns `(DAT_fff90d7c & 1) != 0`. Cluster of 2 — member 0x00076ee0 tests bit 1 of the
same byte. Getter bank. [O]

#### 0x00076EEC `meas_get_via_0x00069b9c` — stub
3-instr tail-call thunk invoking provider 0x00069b9c through `DAT_000771d0`. Getter bank.
Single-member cluster. [O]

#### 0x00076EF2 `meas_snap_d9c_low2_get` — data-accessor
Returns `DAT_fff90d9c & 3`. Getter bank. Single-member cluster. [O]

#### 0x00076EFC `meas_snap_d9c_bit7_get` — data-accessor
Returns `(DAT_fff90d9c & 0x80) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076F08 `meas_snap_d9d_bit0_get` — data-accessor
Returns `(DAT_fff90d9d & 1) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076F14 `meas_snap_d9d_bit1_get` — data-accessor
Returns `(DAT_fff90d9d & 2) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076F20 `meas_snap_d9d_bit3_get` — data-accessor
Returns `(DAT_fff90d9d & 8) != 0`. Cluster of 2 — member 0x00076f2c tests bit 0x10 of the
same byte. Getter bank. [O]

#### 0x00076F38 `meas_snap_d9d_bit7_get` — data-accessor
Returns `(DAT_fff90d9d & 0x80) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076F44 `meas_snap_d9e_bit0_get` — data-accessor
Returns `(DAT_fff90d9e & 1) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076F50 `meas_snap_d9f_bit1_get` — data-accessor
Returns `(DAT_fff90d9f & 2) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076F5C `meas_snap_dae_low2_get` — data-accessor
Returns `DAT_fff90dae & 3`. Getter bank. Single-member cluster. [O]

#### 0x00076F66 `meas_snap_dbc_low3_get` — data-accessor
Returns `DAT_fff90dbc & 7`. Getter bank. Single-member cluster. [O]

#### 0x00076F70 `meas_snap_dcc_low3_get` — data-accessor
Returns `(DAT_fff90dcc & 7) | 0xf8`. Getter bank. Single-member cluster. [O]

#### 0x00076F7C `meas_snap_dbc_bits3_5_get` — data-accessor
Returns `(DAT_fff90dbc >> 3) & 7`. Getter bank. Single-member cluster. [O]

#### 0x00076F8C `meas_snap_dbc_bit6_get` — data-accessor
Returns `(DAT_fff90dbc & 0x40) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076F98 `meas_snap_dbd_bit0_get` — data-accessor
Returns `(DAT_fff90dbd & 1) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076FA4 `meas_snap_dbd_bit1_get` — data-accessor
Returns `(DAT_fff90dbd & 2) != 0`. Getter bank. Single-member cluster. [O]

#### 0x00076FB0 `meas_snap_dbd_bit2_get` — data-accessor
Returns `(DAT_fff90dbd & 4) != 0`. Getter bank. Single-member cluster. [O]

## Unit summary

**Clusters documented:** 220 (269 functions named, rep + members). No pre-named `caseD_*`
functions appeared in this unit.

**Counts by kind** (per names CSV rows):
| kind | count |
|---|---|
| data-accessor | 172 |
| logic | 50 |
| stub | 44 |
| dispatch | 3 |
| unknown | 0 |

**Unknown entries:** none fully unknown; the weakest identifications are 0x0007626c
(`flag_state_eq_2_get`), 0x0007627c (`tbl_lookup_by_counter`) and 0x00076568
(`ident_module_reset`), all [I]-marked with observable behavior described.

**What this slice is:** the region 0x74154–0x76FB0 in r04_boot is (a) the UDS diagnostic
session/record/hand-over block of spec 06 §5a + spec 15 §2/§3 (0x74154–0x76170), and
(b) a bank of ~200 tiny measurement-signal getters (`meas_snap_*`, `meas_get_via_*`)
over the RAM snapshot 0xFFF90ABE–0xFFF90DBD, consumed by the giant measurement provider
0x00144108 (r14_app) — the 470-entry measurement index of spec 10 §5.

**Cross-region leads:**
- The 35-member cluster 0x00076708 has copies in **r0c_app** (0x000F69C6, 0x000F6CC8,
  0x000F8618, 0x000F8624 → providers 0x0011C7C8/0x0011C742/0x0012134C/0x0012135E) and
  **r10_app** (0x00128BD4, 0x00128BE0 → 0x0012ECAC/0x0012ECBE). Those units should mirror
  the `meas_get_via_*` names.
- The UDS 0x2E write state machine (0x00074B0C) thunks into r10_app 0x001321EE–0x00132316
  and r04_boot 0x00079764/0x000797BC/0x000797FE (flash/EEPROM engine, another unit) plus
  0x00070518 — cross-check with the unit owning 0x797xx.
- 0x00074C20 transmits its status frame on "channel 0x4F" through r08_app helpers
  (0x00094EEC etc.); the stream-channel module (0x00074CE8–0x00075918) transmits via
  0x0006CF44 → RAM pointer DAT_0006cfc8.
- 0x00074532 validates through r10_app 0x00132142 (OS event call); 0x000760F8 permission-
  checks addresses with 0x0007D3CC (region tables 0x435C0/0x43620, spec 10 §2).
- Identity/VIN-style buffers live at 0xFFF82E14–0xFFF82E53 with shadows at
  0xFFF93BDB–0xFFF93C03; no spec doc covers them yet (candidate addition to spec 15).
