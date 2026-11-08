# Unit 02 (r00_boot)

#### 0x00013BE6 `boot_diag_eval_cond_table` — dispatch
Scans a table of 0x14-byte entries (base from `*PTR_DAT_00013df0`, count at +4) matching each entry's class byte, condition byte (-1 = wildcard), and two bit-masks built from indirect getters (`PTR_FUN_00013df4/8/c`, counts at table+5/+6) against the request; on match invokes the entry's handler fnptr (+8) with its argument (+0x10) unless a gate callback (`*(DAT_fff84190+0x28)`) vetoes (then status 3 at `0xFFF84188`). No-match sets status 8/9; tail block demotes state `0xFFF84188.2` to 3 on terminal states. Called by service routers 0x14720/0x14334. [I]
#### 0x00013D6E `boot_init_run_table` — dispatch
Boot initializer with no direct callers (pointer-invoked): calls init fnptr `PTR_FUN_00013e10`, passes `DAT_fff800ac` to `*DAT_00013e14`, clears the two pending-request bytes via 0x13DAC, then walks the handler table at `*PTR_DAT_00013df0` (count at +1) invoking every non-NULL init callback at entry+0xc. [O]
#### 0x00013DAC `boot_clear_pending_req` — stub
Trivial: zeroes the two pending-request/state bytes `DAT_fff800ab` and `DAT_fff800ac`. Cluster of 3 (0x13DAC, 0x22DE2, 0x734EE in r04_boot) — duplicated helper copies. [O]
#### 0x00013DBC `boot_snapshot_link_status` — logic
Writes 0x77 to `DAT_fff800a4` then stores four getter results (`PTR_FUN_00013e0c/18/fc/1c`) into `DAT_fff800a5`, `a6`, `ab`, `ac`, and finally invokes the callback at `DAT_fff84190+0x1c` if present. Snapshots link/session status bytes into a communication block. [I]
#### 0x00013E20 `boot_diag_session_init` — logic
Resets the diag-session globals (`DAT_fff84ea0` bytes, `DAT_fff84194/98/9c/a4`, `DAT_fff84118`, `DAT_fff84ea0.1=0x78`, `.2=0x21`), sets `DAT_fff84198` = `*(DAT_fff84190+0x14)+4`, aborts a pending response (0x1422E), resets counters (0x13E94), and if probe `PTR_FUN_00013edc` fails clears pending bytes (0x13DAC). Called by 0x11DD0. [O]
#### 0x00013E94 `boot_diag_reset_counters` — stub
Clears `DAT_fff8417c` and `DAT_fff84178` and calls the two reset fnptrs `PTR_FUN_00013ee0/ee4` with 0. [O]
#### 0x00013EB2 `boot_diag_set_request_ctx` — data-accessor
Stores the request-context pointer into `DAT_fff841ac`, copies byte 0 to `DAT_fff84e84.1`, sets active flag `DAT_fff841b8.1=1`, and saves ctx+0xc into `DAT_fff841c4`. [O]
#### 0x00013ED2 `boot_diag_set_config_ptr` — data-accessor
Single assignment: `DAT_fff84190 = param_1` (diag config/vtable pointer used all over this module). [O]
#### 0x00013EE8 `boot_statemach_poll_transitions` — logic
Walks the state list at `*PTR_DAT_00014078` (count at +4) calling each state's probe fnptr (+0); result 1 records a new current state, result 2 additionally fires the exit hook at list+0x24; tracks current state/args in `PTR_DAT_0001407c` block; a byte at state+0x34 stops the scan. [O]
#### 0x00013F70 `boot_statemach_exit_others` — logic
If a current state is recorded (`*PTR_DAT_0001407c != 0`), calls the exit/tick fnptr (+4) of every state in the `PTR_DAT_00014078` list except the current one. [O]
#### 0x00013FA4 `boot_statemach_run` — logic
Main state-machine step (caller 0x11C08): runs the current state's enter fnptr (+0) or transition callback (+0x2c), on result 0 runs exit hooks (`0x13F70`, +0x18, `DAT_fff84194+0x30`, two PTR_FUN cleanups) and clears current, otherwise polls transitions (0x13EE8); returns the current-state cookie at `PTR_DAT_0001407c`. [O]
#### 0x0001408C `boot_diag_send_with_hdr` — logic
Response sender for the channel at `PTR_DAT_00014318`: in state 2 sends payload as-is (length at +8); otherwise prepends the target-address byte (byte +0xb, plus '@' in the non-2 branch) and passes buf/len/data/tag to the channel send fnptr at +0x14. [O]
#### 0x000140C8 `boot_diag_send_negresp_hdr` — logic
Builds a two-byte header (target byte from `PTR_DAT_00014318[0xb]`, then 0x7f) in front of the response buffer and calls the channel send fnptr (+0x14) with length+2 — K-line framed negative-response send. [I]
#### 0x000140F6 `boot_diag_build_negresp` — logic
Formats a diagnostic negative response at `PTR_DAT_fff84e90_2_0001431c`: `[0]=0x7f, [1]=SID, [2]=NRC`, appending the sub-function byte when SID==0x80 and `*(PTR_DAT_00014318+6)!=0`; returns length 3 or 4. Classic 0x7F/SID/NRC layout. [O]
#### 0x00014128 `boot_diag_send_negresp_last_sid` — logic
Sends a negative response built from current SID `DAT_fff84188` with NRC `DAT_fff84ea0.2` through the send fnptr at `*DAT_fff84194`. No direct callers (state callback). [O]
#### 0x0001415A `boot_diag_send_negresp_0x10` — logic
Sends negative response NRC 0x10 (general reject) using SID `DAT_fff84e98.1` via the `PTR_DAT_00014320` channel fnptr (+4) with its stored destination args. [I]
#### 0x00014186 `boot_diag_send_negresp_active` — logic
Chooses SID from `DAT_fff84e98.0` (pending vs `DAT_fff84188`), builds the 0x7F response (0x140F6) and sends it via `*DAT_fff84194` with destination `0xFFF84E92`, length tag 9. [O]
#### 0x000141CA `boot_diag_echo_rx_buffer` — logic
Copies `DAT_fff800b6` bytes from `0xFFF800B7` (RX buffer) behind a 4-byte header, sets tag 3 when the frame starts 0x7f and matches SID `DAT_fff84ea0.2`, then re-sends it via the `PTR_DAT_00014320` channel fnptr — echoes the received request frame back. [I]
#### 0x0001422E `boot_diag_abort_pending_resp` — logic
Cancels a pending response: clears `DAT_fff84e98.3/9c.3/.2` flags, resets channel length (`*(PTR_DAT_00014320+0x10)=0`, cookie from `PTR_PTR_00014324`), and if a response was active (`DAT_fff84ea4.0`) clears it and sends an empty frame via the channel fnptr. [O]
#### 0x00014276 `boot_diag_abort_set_reason` — stub
Calls 0x1422E (abort pending response) then records the reason code in `DAT_fff841a8`. [O]
#### 0x00014286 `boot_diag_send_first_fragment` — logic
Starts a multi-frame response: when idle and channel mode != 2, takes half the pending length (`*(PTR_DAT_00014318+4)>>1`) into `0xFFF84114`, moves the data pointer, adjusts remaining length at +8, records fragment info (`0xFFF8411A`, `PTR_DAT_00014328`), and sends the first chunk via 0x1408C with block timing from `*(PTR_DAT_00014318+0x18)`. [I]
#### 0x00014334 `boot_diag_dispatch_service` — dispatch
Central service dispatcher: indexes the switchdata table by `DAT_fff84188.2` (values 1..0xb); cases route to send helpers 0x1408C/0x140C8, raw sends via `*DAT_fff84194`, fragment start 0x14286, session teardown via 0x14276, and set next-state cookies `PTR_PTR_00014588..98` into `DAT_fff8419c`; contains named `caseD_8` path. Called by 0x145B4. [O]
#### 0x000145B4 `boot_diag_process_request` — logic
Request pump: when idle detects the session-activate edge on `DAT_fff841b8.1/.0` (records into `DAT_fff84e9c.2`); when a request is pending copies it from `DAT_fff841b0` into the channel struct at `PTR_DAT_000147c0` (len-1, checksum byte via 0x14888, timing via 0x148AE) and runs the service dispatcher 0x14334. [O]
#### 0x00014620 `boot_diag_poll_request` — stub
If session-active flag `DAT_fff841b8.1` is set, invokes the request callback `*PTR_PTR_000147c4` (recorded in `DAT_fff8419c`). No direct callers — callback-invoked poll. [O]
#### 0x00014720 `boot_diag_route_service` — logic
Clears status, runs the condition table (0x13BE6), then classifies the resulting index: 8/10 gated on `DAT_fff84e98.0` -> reason 4; 9/0xb gated on `.2` -> 3; 7 checks config `*(DAT_fff84190+0xc)`; everything else stores teardown cookie `PTR_PTR_000147cc`; calls 0x14276 to finish. Companion router to 0x14334. [I]

#### 0x00014888 `boot_diag_check_p2_timer` — logic
Returns 1 when the mode getter `PTR_FUN_00014990` reports 2 and `DAT_fff8008c < DAT_fff84120` (timer compare), else 0. Used by request pump 0x145B4 as the checksum/echo byte. [I]
#### 0x000148AE `boot_diag_clamp_block_time` — logic
Constrains a block time value: in mode 2 with state 2/3 forces 6; in mode 2 clamps to 0xfe; otherwise passes through. Feeds the staging struct in 0x145B4. [I]
#### 0x000148E4 `boot_diag_scan_state_table` — logic
Walks up to `*PTR_DAT_fff84ea4_2_00014994` entries of the transition table at `DAT_fff841c0` (2-byte src/dst pairs) matching the requested source `param_1` or the active source `*(DAT_fff841ac+2)`; on match records new state into `DAT_fff841b8.2/.3`, stores `param_2` in `DAT_fff841bc`, and decrements the retry counter. [I]
#### 0x00014972 `boot_diag_set_state_table` — stub
Installs the transition table pointer into `DAT_fff841c0` and the entry count into `*PTR_DAT_fff84ea4_2_00014994`. Called by 0x19D5A/0x19DAC. [O]
#### 0x0001497E `boot_diag_is_same_source` — stub
Returns `DAT_fff84e84.1 == param_1` (source-address compare). [O]
#### 0x00014998 `boot_thunk_cb_149cc` — stub
Three-instruction thunk: tail-calls `PTR_FUN_000149cc(param_1)`. Cluster of 8 identical thunks (0x14998, 0x149A4, 0x234C0, 0x282F4, 0x282FA, 0x28326, 0x73F64, 0x73F76) — generic pointer-dispatch shims, each likely bound to a different callback slot. [O]
#### 0x000149B8 `thunk_FUN_00013be0` — stub
Sets the condition-table pointer: `*PTR_DAT_00013df0 = param_1`. [O]
#### 0x000149BC `boot_install_default_cond_table` — stub
Installs the default condition table `PTR_PTR_000149e0` via thunk 0x149B8; called from 0x11EF8. [O]
#### 0x000149C2 `boot_set_dat_fff84124` — data-accessor
Single store of param into `DAT_fff84124`; purpose not identifiable from this unit. [I]
#### 0x000149E4 `boot_call_init_pair` — stub
Calls `PTR_FUN_000149f4()` then `PTR_FUN_000149f8(0x400)` — pair of subsystem init hooks with a 0x400 argument. [I]
#### 0x00014A00 `boot_log_event_ring` — logic
Appends an event record (id `param_1`, tag `param_2`, dword `param_3`, ctx) into a 5-entry ring at `0xFFF84E5A/0xFFF84E64/0xFFF84C34/0xFFF84C20` with counters `DAT_fff856e8/0FFF856E7`; after overflow just counts events in `DAT_fff856e8` up to 0xff. Called by 0x161AC and 0x168D0 — a boot event trace log. [I]
#### 0x00014A70 `boot_or_event_flags` — data-accessor
ORs bits into `DAT_fff84e34`. [O]
#### 0x00014A7E `boot_clear_event_flags` — data-accessor
Clears bits of `param_1` from `DAT_fff84e34`. [O]
#### 0x00014A90 `boot_obj_init_from_template` — logic
Initializes an object at `param_1` from a constant template: dword `DAT_00014d34`→+0x28, ptr `PTR_PTR_00014d38`→+0x2c, word `PTR_DAT_00014d3c`→+0x30, plus two copied bytes (+0xa/+0xb). No direct callers (callback invoked). [O]
#### 0x00014ABC `boot_tbl_entry_ptr` — library
Address arithmetic helper: returns `0x80100000 + idx * *(table+4)` — indexes a record array based in external/record memory at 0x80100000 described by the descriptor at `*(param+0x28)`. Cluster of 3 (0x14ABC, 0x2B088, 0x6F65C). [O]
#### 0x00014ACC `boot_blk_select_main` — logic
Six-phase state machine (`DAT_fff8601a` 0..5) over eight 0x2c-byte candidate blocks from the table at 0x80100000: initializes slots, checks two status bytes per block, validates each with magic 0x4001 + double-checksum (0x15502) and pattern checks (0x15652), picks the highest priority (+8) via 0x14FB2, verifies content diff (0x14FE0) and name template (0x15060 vs `PTR_DAT_000151f8`), then installs the selection via 0x158DE and reports 1/3/4/5. Looks like boot-variant/config block selection. [I]
#### 0x00014FB2 `boot_blk_pick_max_priority` — library
Argmax over the +8 word of 0x2c-stride records; returns the winning index or 0xff. Cluster of 3 (0x14FB2, 0x2B6B8, 0x6FCD4). [O]
#### 0x00014FE0 `boot_blk_diff_bitmask` — library
Byte-compares 0x11 bytes of two arrays and folds mismatches into a bitmask by position group (bits 0/1/2/3/4 for ranges 0-5,6-9,10,11,12-16). [O]
#### 0x00015060 `boot_blk_name_matches` — library
Compares 0x14 bytes at `param_1` against the constant template `PTR_DAT_000151f8`, returning true only on full match. [O]
#### 0x00015086 `boot_blk_scan_lite` — logic
Companion selector (`DAT_fff8601b` 0..3) over 0xc-byte per-block records at `0xFFF84750..`: per-block status/checksum via 0x15502, tiered scans (status, status+verified, name-miss, class-2, class-1) honoring a mask at `*(param_1+0x58)`, falling back to lowest +0 dword at `0xFFF84758`; stores the chosen slot at `param_1+0x19` and returns 1 (found) or 2. [I]
#### 0x00015484 `boot_blk_index_to_offset` — library
Returns `param_1 * *(table+4) >> 13` — scales a block index into an offset using the record stride. Cluster of 2 (0x15484, 0x2BBBC). [I]
#### 0x00015492 `boot_checksum_range` — library
Rolling checksum over the byte range [param_1..param_2] with seed `param_3`, processing 8 bytes per step through `PTR_FUN_00015670`; at 8-byte rows whose row-index is 7/8/10 it first consults `PTR_FUN_0001566c` and substitutes `len*0xff` accumulation. Two area hooks make this a mode-aware ROM checksum. [O]
#### 0x00015502 `boot_verify_block_checksum` — library
Descriptor verifier (param_1 = [addr,len,seed,out,ok]): sets `DAT_ffffaa00=0xb400`, computes 0x15492 twice in read-modes 1 and 2 (`PTR_FUN_00015674`), requires both sums equal, stores the value and verdict; restores `DAT_ffffaa00=0xb401`. [O]
#### 0x0001555E `boot_checksum_stream_800` — logic
Chunked two-pass checksum state machine (`DAT_fff8601c`): digests a region from `PTR_DAT_00015678` in 0x800-byte slices per call through `PTR_FUN_0001567c`, runs pass 2 when `param_2==1` (with `DAT_ffffaa00` mode toggling), and on completion compares the two sums into the descriptor out/ok fields. Returns 1 while busy. [O]
#### 0x00015652 `boot_blk_pattern_differs` — logic
Checks 8 bytes at `param_1` against pattern `param_2` (e.g. 0xa5) twice with bus-mode switching (`PTR_FUN_00015894` 1/2/0); returns 2 when a mode getter says so, else 1 if any byte differs from the pattern (0 = all bytes equal). Erased/marker-pattern probe for block slots. [I]

#### 0x000156E0 `boot_tbl_find_tiered` — logic
Scans 0x18-stride entries (list at `param_1+0x2c`, count +0x30) in three passes for the first entry whose word[0]!=0 and whose flag byte lacks 0x40 (pass1) / 0x20 (pass2) / 0x10 (pass3); records the entry at `param_1+0x60` and the tier at +100. No direct callers (callback). [I]
#### 0x00015756 `boot_tbl_find_incremental` — logic
Incremental cursor scan (`DAT_fff85fec`) of 0x18-stride entries for one with (attr&2), !(byte&0x80), word[0]!=0 — selects it (entry at +0x60, tier 3); resets cursor when none. [I]
#### 0x000157C4 `boot_tbl_contains_id` — library
Linear search: returns 1 when any 0x18-stride entry has word[0] == param_1 (nonzero). Cluster of 3 (0x157C4, 0x2BF40, 0x705BC). [O]
#### 0x00015800 `boot_desc_get_length` — data-accessor
Returns a descriptor's length: for type byte +6==5 the buffer span `[1]-[0]`, else the word at +4. Cluster of 3 (0x15800, 0x2BF7C, 0x705F8). [O]
#### 0x00015816 `boot_desc_get_data_ptr` — data-accessor
Returns the descriptor data pointer (`*param_1`, double deref when type==5). Cluster of 3 (0x15816, 0x2BF92, 0x7060E). [O]
#### 0x00015824 `boot_hw_wait_ready` — logic
Two-phase startup (`DAT_fff8601d`): sets bit 2 of `DAT_fff8578d`, polls `PTR_FUN_00015898` until `DAT_fff8578c != 0x1e`, then runs init callbacks 0x1589c/0x158a0/0x158a4 and counts retries in `DAT_fff84eb0.2`; returns 1 when ready, 0x80 busy/failed. [I]
#### 0x000158D0 `boot_hw_release` — stub
Clears bit 2 of `DAT_fff8578d` and calls `PTR_FUN_00015b1c` — counterpart to 0x15824. [O]
#### 0x000158DE `boot_blk_install_config` — logic
Installs the selected block: reads slot+0x60 (validated by `PTR_FUN_00015b20` + 0x15502 over 0x18 bytes) or falls back to +0x28/+0x48, then copies up to 8 (id,value) pairs into the param_2 bytes / param_3 words and accumulates the value sum into `param_1+0x54`. [I]
#### 0x00015988 `boot_blk_pick_candidate` — logic
Runs 0x15A5E to tally slot classes, then selects the next candidate index: preferred class-0 unused slot, else lowest high-nibble class-2 slot, honoring the exclusion mask at `param_1+0x58`; returns 0xff when none. [I]
#### 0x00015A5E `boot_blk_tally_classes` — library
Tallies slot classes from bytes at `param_1+0x39` into nibble counters at +0x41 (classes 1/2) and +0x42 (class 4/other). Cluster of 2 (0x15A5E, 0x2C1FE). [O]
#### 0x00015B24 `boot_flash_catalog_scan` — logic
9-phase catalog builder (`DAT_fff8601e`): clears the RAM catalog (`0xFFF84540..0xFFF845E6`), walks the flash table from 0x80100000 in 0x1000 steps validating magic 0x4001 + checksums (0x15502), maps block types 1/2/3 to sizes 0x2000/0x4000/0x8000, collects sub-blocks (headers with bit 0x8000, ranges into `DAT_fff84540/0xFFF84542`), and accumulates per-block checksums into `0xFFF845C4/0xFFF845D4`; exits with `DAT_fff845e6` = 0 (ok) / 2 (fail). Matches the block/marker catalog of spec 06/16. [I]
#### 0x000161AC `boot_read_mem_virtualized` — logic
Copies param_3 bytes from address param_1 into param_2, but substitutes 0xff for addresses inside catalogued marker/checksum ranges and splices in the computed checksum bytes from `0xFFF845C4/0xFFF845D4` at block offsets +0x50..+0x56 — a virtualized flash read used by upper layers (e.g. diagnostic read-by-address). Called by 0x17EA0. [I]
#### 0x0001637C `boot_catalog_get_status` — data-accessor
Returns 4 when `DAT_fff845e5`==0 else `DAT_fff845e6` (catalog result code). [O]
#### 0x00016390 `boot_catalog_clear_flag` — stub
Clears `DAT_fff845e5`. [O]
#### 0x000163A4 `boot_data_engine_step` — logic
Five-state engine over `PTR_DAT_00016618`: sizes an entry list (via 0x1666E marker walks summing length words), sets transfer flags in `DAT_fff85d9a`, invokes per-entry callbacks (+8/+0xc/+0x10) and moves up to 0x300 bytes via `PTR_FUN_0001661c`; states 3/4/5 gate on flags 4/2/1. [I]
#### 0x0001666E `boot_marker_find` — logic
Walks the linked record list at block+0x80 (`param_4` context): each record has a tag word (type 0x7000 / id 0xfff) and next-offset at +2; modes 1 (id+attr match), 2/3 (type match) with masks from `DAT_000048e0/0x000048E1`; returns the record pointer or 0. This is the block marker-directory search of spec 06. [O]
#### 0x000167A8 `boot_desc_stream_step` — logic
Serializes out a descriptor list: computes total length via 0x15800, then copies byte-by-byte through the pointer pair `PTR_DAT_000168a0`/`PTR_DAT_00016898` advancing `DAT_fff847cc/0xFFF847D0`, returning bytes transferred. Called by 0x1C480. [I]
#### 0x000168D0 `boot_flash_program_block` — logic
Multi-phase flash programmer (`DAT_fff8601f`): locates the target record (param+0x60), validates entries with a rolling checksum (`PTR_FUN_00016da8`), builds an 0x3c-byte payload (padding to 8, address via `+0x7ff00000`/13-bit page mapping), programs 8-byte units through `PTR_FUN_00016b6c` tracking 0x2000 page wraps, verifies, retries once (`DAT_fff86020`), and returns codes 1..6. [I]
#### 0x00016C3C `boot_blk_read_record` — logic
Three-phase reader (`DAT_fff86021`): locates a checksum-valid record (id at param+0x60) in the block at +0x34/+0x80, copies up to 0x3c bytes to `param_2+4`, then invokes the +0xc callback with mode 2/1; result 2 on failure. [I]
#### 0x00016DC0 `boot_flash_write_engine` — logic
The main flash-write state machine (states 0x14..0x49 / 0xfa-0xff on `*PTR_DAT_00017000`), 1183 instrs: erase check via 0xa5 patterns, 0x80-byte chunk programming with `0x1fff` page boundaries (`DAT_fff85dbc/0xFFF85E3A/0xFFF85E3E`), writes 0x4001-magic block headers (0x7c size at +0xae) with version `param_2+0x24`, marker arrays to `0xFFF85E68/0xFFF85EA4`, and verify states 'F'/'G' comparing checksums into `DAT_fff84e40`. This is the reflash/programming engine behind the boot manager (spec 16). [I]
#### 0x000178C0 `boot_flash_mark_block` — logic
Erase/mark state machine (`DAT_fff86022`): resolves the block, verifies erased-0xa5 state (0x15652), programs 8-byte 0xa5 patterns at +0x38/+0x40 through `PTR_FUN_00017990`, commits via `PTR_FUN_00017994`, and sets the per-slot flag at `param_2+0x1b+idx` on success. [I]
#### 0x00017A26 `boot_desc_unpack_bits` — logic
Bit-stream expander: walks 8-byte descriptors of types 0-5 (type 2 = byte, 3/4 = bit-packed with shift extraction from `local_24`), expanding each field to a byte written through `PTR_DAT_00017be0`; mirrors 0x167A8 in reverse. Called by 0x1C590. [I]
#### 0x00017B50 `boot_flash_write_params` — logic
Programs the three 8-byte parameter areas `param_1+0x39/+0x44/+0x4c` plus a zeroed 6-byte trailer via `PTR_FUN_00017d84`, tracking a rolling checksum (`PTR_FUN_00017bf0` into `DAT_fff84e44`) and offsets in `PTR_DAT_00017d7c`; verifies against `PTR_FUN_00017d94` at the end. [I]
#### 0x00017D46 `boot_blk_next_candidate` — logic
Unless bit 0x40 of `*(param_1+0x5c)` blocks it, asks 0x15988 for the next slot, stores it at `param_1+0x19` and bumps the sequence `DAT_fff84e4c`. [I]
#### 0x00017D9C `stub_ret0` — stub
`return 0` — empty placeholder callback. Huge cluster of 36 identical copies spread across all regions (0x17D9C, 0x1C2C6, 0x1CDCC, 0x22372/6, 0x23EE2/6, 0x2A160, 0x2F6A4, 0x30924-0x3092C, 0x75F72/6, 0x762CC-D8, 0x794C4, 0x7C6AE, 0x9B6E0-0x9B6FE, 0x9CCAA, 0xA4652, 0xC918C, 0xCDD00/4, 0xD0C90, 0xD0D66, 0xDCFC4, 0xE58CC, 0x13237E, 0x1520B4/8) — null-substitute slots in vtables/handler arrays. [O]
#### 0x00017DA0 `boot_ringbuf_write_6000` — logic
Appends param_2 bytes at `param_1` into the linear capture buffer at `DAT_fff85ee8` (base `*PTR_DAT_00017e9c`), wrapping at offset 0x6000 via two 0x18032 copies; updates fill count at `PTR_DAT_00017e98`; rejects when full at 0x6000. [O]

#### 0x00017E16 `boot_ringbuf_reset` — stub
Resets the 0x6000-byte capture ring at `0xFFF88400`: base pointer `*PTR_DAT_00017e9c`, write `DAT_fff85ee8` and read `DAT_fff85eec` to base, count 0. Companion of 0x17DA0/0x17E32. [O]
#### 0x00017E32 `boot_ringbuf_read` — logic
Consumes up to param_2 bytes from the ring (read ptr `DAT_fff85eec`, wrapping at 0x6000 via two 0x18032 copies) and decrements the fill count at `PTR_DAT_00017e98`. [O]
#### 0x00017EA0 `boot_mem_copy_checked` — logic
Copies param_3 bytes param_1→param_2 only after validating the source (mode 2) and destination (mode 4) with cached results via 0x1822C; plain 0x18032 copy except when the source lies in the 0x80100000 table area, where it routes through `PTR_FUN_000180f4`. Called by 0x11F8E. [O]
#### 0x00017FEE `boot_chunk_size_800` — library
Returns 0x800 for chunk types 1/2/4/8/0x10/0x20, else 0. Cluster of 3 (0x17FEE, 0x243B8, 0x7D184). [O]
#### 0x00018022 `lib_word_copy_alt` — library
Word (4-byte) copy loop, appears as a thunk-twin of 0x1802C. Cluster of 4 (0x18022, 0x243EC, 0x3E170, 0x7D1BA). [O]
#### 0x0001802C `lib_word_copy` — library
Word-stride copy loop (4 bytes per step). Cluster of 4 (0x1802C, 0x243F6, 0x3E17A, 0x7D1C6). [O]
#### 0x00018032 `lib_memcpy` — library
General memcpy: aligns head/tail via byte thunk 0x1801c and copies the aligned middle via the word copier; heavily used across the boot region. [O]
#### 0x000180B2 `boot_prot_check_range` — logic
Validates an address range against a protection-window table (0xc-stride entries: start,end,attr,flag): mode 1 (read windows) yields 0 ok / 1 outside / 2 mixed owners / 3 attr-denied / 4 chunk > 0x800 (via 0x17FEE); mode 2 (write windows) honors the owner byte at +9. Implements the read/write protection records of spec 06/16. [I]
#### 0x0001822C `boot_mem_access_check` — logic
Full access check: read-window table `PTR_LAB_00018384` then write table `PTR_PTR_0001838c` via 0x180B2, plus special-case regions 0x600000-0x680000 (mode 4, gated on `DAT_fff86031`) and 0x80100000+ (mode 2, bounded by `PTR_DAT_00018390/98`, gated on `DAT_ffffab00` bit 0); caches the last verdict in the `DAT_fff85f58` slot pair for 0x17EA0. [I]
#### 0x0001836E `lib_memset_alt` — library
Byte-fill loop (`memset`), thunk-twin of 0x18378. Cluster of 3 (0x1836E, 0x246CC, 0x7D53E). [O]
#### 0x00018378 `lib_memset` — library
Byte-fill loop: fills param_2 bytes with param_3. Cluster of 3 (0x18378, 0x246D6, 0x7D548). [O]
#### 0x0001839C `boot_upload_pump` — logic
Upload state pump (`DAT_fff85ff8`): state 1 moves 0x400-byte chunks from cursor `DAT_fff84078` through access checks (0x1822C); state 2 runs the range op `PTR_FUN_000184a8` staging into `0xFFF85CBC` and gates on `PTR_FUN_000184ac`/`DAT_fff85cca`; advances states 2/4/5. Called by 0x13780. [I]
#### 0x00018436 `boot_upload_start` — logic
Starts an upload: for a page-aligned (addr&0xff==0, len>0xff) range stores it in `DAT_fff84070/74/78` and enters state 1; also drains states 3 (result into *param_3), 4, 5; returns 0xc for invalid args. [I]
#### 0x000184B0 `boot_state_pump_fff85ff2` — logic
Two-state pump on `DAT_fff85ff2`: state 1 runs `PTR_FUN_00018534` then `PTR_FUN_00018538` (0/2 → state 2), state 2 runs `PTR_FUN_0001853c(2,0)` (0/2 → state 4); other results fail to state 8. Part of the boot session close sequence (calls into 0x1B628/0x1B8F2/0x1B8FA cluster family). [I]
#### 0x00018504 `boot_state_abort_fff85ff2` — stub
Companion of 0x184B0: clears state 4, converts state 8 to return 5, otherwise re-arms state 1. [O]
#### 0x00018540 `boot_addr_range_lookup` — logic
Maps an address range onto the flash block catalog: uses descriptor `PTR_PTR_00006010` (boot data at 0x6000-0x7FFF, header block 0x20000) with lookup callbacks (+0x44/+0x48/+0x58) filling `0xFFF84084..0xFFF840A0`; sets window bounds at `PTR_DAT_00018764`, classifies into `DAT_fff86013` (1 outside, 2/3 inside special windows), and computes a block bitmask at `PTR_DAT_00018760`. [I]
#### 0x00018774 `boot_flash_erase_walk` — logic
Erase state machine over catalog blocks (`DAT_fff86025/26`): walks block indices from `DAT_fff84e74-1` down using mask `_DAT_fff840cc`, resolves each range via the +0x48 lookup, checks erase status via 0x19078, and programs through `*DAT_0001896c`; returns 1 busy / 2 done. [I]
#### 0x0001886E `boot_flash_prog_range` — logic
Programs the range from `PTR_DAT_00018964` (start/len): checks with 0x19078 (state `DAT_fff86027`), then programs via `*DAT_0001896c`; returns 2/3 on failure. [I]
#### 0x000188C6 `boot_flash_range_status` — logic
Probes two ranges of `PTR_DAT_00018964` with 0x19078 (modes 0xffffffff and 0): returns 1 when the first is busy, 2 when both are clear, 0 otherwise. [I]
#### 0x00018902 `boot_flash_write_gate` — logic
Gate on flash status flags `DAT_fff840c8`: maps mode getter values (via `PTR_FUN_00018970`) and bits 0x400/0x10/8 plus `DAT_fff86013` into verdict 0/2/4/8/0x2a; final mask check against `PTR_thunk_FUN_00011c30_00018974`. Used before erase/program. [I]
#### 0x000189BC `boot_copy_status_8bytes` — stub
Copies 8 bytes from `0xFFF85A7D` to `0xFFF85CD4`. [O]
#### 0x000189D0 `boot_map_mode_letter` — stub
Runs `PTR_FUN_00018a90` then maps `PTR_FUN_00018a94` result 1/2/3/other to letter 'M'/'D'/'R'/'F' stored at `*PTR_DAT_00018a98`. [I]
#### 0x000189D8 `boot_map_mode_letter_2` — stub
Same M/D/R/F letter mapping from `PTR_FUN_00018a94` (without the 0x18a90 preamble). [I]
#### 0x00018A0A `boot_build_version_string` — logic
Builds the ASCII version string "TCU-<a>-<b>.<c> <d><e>.<f>" at `0xFFF86004` from `DAT_fff85f29/2a/2b/2c` (BCD nibbles to '0'-based chars). [O]
#### 0x00018A9C `boot_state_pump_fff85ff6` — logic
State pump on `DAT_fff85ff6` (erase/program session of 0x18AEE): state 1 → `PTR_FUN_00018b70` then `PTR_FUN_00018b78(PTR_DAT_00018b74)` (0 → state 2), state 2 → `PTR_FUN_00018b7c` (0 → state 4, 2 busy), else fail 8. [I]
#### 0x00018AEE `boot_erase_start` — logic
Entry for an erase/program request: stores the range at `PTR_DAT_00018b74`, classifies with 0x18540; inside catalog windows → gate 0x18902 then state 1; below 0x20000 rejected with 5; otherwise runs `PTR_FUN_00018b78` and enters state 2. Pairs with 0x18A9C. [I]

#### 0x00018B80 `boot_erase_check_pump` — logic
Erase-verify pump (`DAT_fff85ff9`): reads 0x800 chunks from `_DAT_fff8407c` through access check 0x1822C in read-mode 0x40 (bus-mode toggling via `PTR_FUN_00018c50`), scanning for any byte != 0xff; blank → state 2, non-blank → state 4, denied → state 8. [I]
#### 0x00018C08 `boot_erase_check_start` — stub
Starts the erase check (range into `_DAT_fff8407c/0xFFF84080`, state 1) and drains terminal states 2/4/8 returning 0xfe/0xff/0xc. [O]
#### 0x00018C54 `boot_memop_pump` — stub
Pumps the memory-op session at `PTR_DAT_00018d38` via `PTR_FUN_00018d3c`: result 0 → state 2 (done), 1 → busy, other → state 4. [O]
#### 0x00018C7C `boot_memop_start` — logic
Starts a range memory op: classifies the range (0x18540), gates with 0x18902 (NRC 0x31 out-of-range, 0x33 security-denied), stores start/end at ctx+4/+8 and kicks `PTR_FUN_00018d40`; returns 0x70 while accepted. Called by 0x1C714. [I]
#### 0x00018CC8 `boot_memop_feed_data` — logic
Feeds (data,len) into the memory-op ctx (+0xc/+0x10) and runs it via `PTR_FUN_00018d3c`: busy → 0x78, accepted state 1, failure → 0x72. Called by 0x1CAA0 (transfer-data service). [I]
#### 0x00018D18 `boot_memop_status` — stub
Polls `PTR_FUN_00018d44`: 1 → busy 0x78, 0 → done 0, other → 0x13. Called by 0x1CAC6. [O]
#### 0x00018D48 `boot_find_marker_walk` — logic
Marker-search walk (`DAT_fff85ff0`): iterates catalog blocks (+0x48 lookup, skipping entries without bits 1/8), and per bank 0/1 searches 0x800 chunks for the 8-byte marker patterns at `PTR_LAB_00018f6c` using Boyer-Moore 0x18ED2; records found offsets in `PTR_DAT_00018f64` and finishes via 0x1BF44/0x1BEC4 cleanup. Implements the marker scan behind spec 16's EB23 marker block. [I]
#### 0x00018ED2 `lib_bm_search` — library
Full Boyer-Moore substring search: builds a 256-entry bad-character table and good-suffix table (via 0x1901A) for the param_4-byte pattern at param_3, then scans param_2 bytes at param_1; returns match offset or 0. [O]
#### 0x0001901A `lib_bm_is_suffix` — library
Boyer-Moore helper: tests whether pattern[i..] is a suffix of pattern[..j] (case analysis over k). [I]
#### 0x00019078 `lib_range_all_equal` — library
Returns 1 when all param_3 bytes at param_2 equal param_1 (erased-flash probe: 0xff / 0x00 patterns). [O]
#### 0x000190A0 `lib_memeq_255` — library
Byte equality compare of (param_3 & 0xff) bytes between two buffers; 1 = equal. [O]
#### 0x000190CC `boot_bank_seq_state` — logic
Two-bank sequence driver (`DAT_fff85ff4` states 1/2/4/8/0x10): bank 0 = `0x6000`/0x2000, bank 1 = header block `0x20000`; per-bank nibble states 0-7 plus 0x20/0x80 flags in `PTR_DAT_000192ec`, catalog lookups (+0x48), range ops via `PTR_FUN_000192f4`, erase/prog helpers 0x188C6/0x1886E/0x18540, retries bounded by `DAT_fff84e74.2` (max 0x32) with retry count `DAT_fff85ff5`. The boot-data bank erase/rewrite sequencer. [I]
#### 0x00019360 `boot_bank_seq_start` — stub
Resets the 0x190CC sequencer: retry budget 2, 0x32-byte state array at `0xFFF84DF2` set to 7, state 1. [O]
#### 0x000193B4 `boot_select_range_table_pair` — logic
Scans four 0x14-stride groups of six slots: nonzero values at `0xFFF85700` are validated against (min,max) pairs at +0/+4 and (+8,+0xc); picks the first in-range slot and a fallback, stores selection/limits into `PTR_DAT_000194f4` and `PTR_DAT_fff84e28_2_000194f0`, sets `DAT_fff86016` and `DAT_fff86015=9`. Looks like boot-time operating-point/table band selection. [I]
#### 0x00019500 `boot_match_platform_id` — logic
Matches the platform: iterates four 0xb-byte entries of the table at `PTR_LAB_000195d8`, comparing `DAT_fff80029` bytes at `0xFFF80021` with entry+8 (key byte at +0x10); on match stores `DAT_fff85d04/DAT_fff84e7c` (=`DAT_fff80020`), `DAT_fff86016` (variant byte), `DAT_fff840d8/+0xdd`, sets `DAT_fff86014=1`, `DAT_fff85cf4=2`, `DAT_fff86015=0xb`. Boot variant identification (gated on `_DAT_fff80010==1`). [I]
#### 0x000195E0 `boot_check_erased_8` — stub
Returns 1 when the 8 bytes at `*DAT_00000210` are all 0xff. [O]
#### 0x0001960C `boot_call_link_send` — stub
Invokes the link vtable send at `DAT_00000210+0x1c` with (ctx, len-0x85, len-0x84, buffer `0xFFF86800`, config+0xb4). [I]
#### 0x0001963E `boot_call_link_recv` — stub
Invokes link vtable +0x20 (prepare `0xFFF86800`) then +0x24 (receive into it with param_1). [I]
#### 0x00019856 `boot_map_counter_state` — stub
Maps `*PTR_DAT_00019898` to 1 (zero) / 2 (bit0 set) / 0 (0xfe) / 3 (other). [O]
#### 0x0001987A `boot_bump_counter_19898` — stub
Increments `*PTR_DAT_00019898` unless its state (0x19856) is 2 or 0. [O]
#### 0x000198FC `boot_range_validate_store` — logic
Validates a 4-word (start/end, start2/end2) range against bounds `DAT_fff84bc8/0xFFF84BCC/0xFFF84BD0/0xFFF84BD4` and ordering, then copies 5 words into `0xFFF84B84`; returns 1 invalid / 3 unordered / 0 ok. [O]
#### 0x0001995C `boot_sched_request` — stub
Sets sub-state `PTR_DAT_00019a44[1]` and ORs 8 into the scheduler flag byte. [O]
#### 0x00019968 `boot_sched_pump` — logic
Scheduler flag pump on `*PTR_DAT_00019a44`: bit 2 loads timing words `0xFFF84B84..94` into ctx 0x19A40 and `DAT_fff84120`; bit 1 arms `DAT_fff84ba4` and calls `PTR_FUN_00019a48/4c`; bit 8 recomputes the deadline (`DAT_fff84124`/ctx+0x10); bit 4 runs and clears the pending callback `_DAT_fff84ba8`; bit 0x10 invokes a callback through the `DAT_fff84170` session vtable. [I]
#### 0x00019C90 `boot_diag_activate_session` — logic
Activates comm session param_1 from the 0x30-stride table at `DAT_fff84170`: copies state bytes to `0xFFF800A9/AA/B0`, runs setup 0x19DD2 + `PTR_FUN_00019e04/08`, loads timing `0xFFF84BB4..0xFFF84BC4` (incl. `DAT_fff84120`), invokes the entry fnptr +0x18, and when the entry is exclusive (`DAT_fff84168`) tears down all other sessions via 0x1A3EE. [I]
#### 0x00019D5A `boot_diag_channel_setup` — logic
Per-channel bring-up: reads the 0x30-stride entry (stores `DAT_fff856d1`, fn-table into `DAT_fff84bf4/8`), calls `PTR_FUN_00019e10`, resolves (source,target) via 0x1A44C/0x1A438, then runs an inlined copy of the 0x148E4 state-table scan to switch `DAT_fff841b8.2/.3`/`DAT_fff841bc` for the new source. Installs the table via 0x14972. [I]

#### 0x00019DAC `boot_diag_channel_init` — stub
Slim channel bring-up: stores channel byte into `DAT_fff856d1`, fn-table pointer into `DAT_fff84bf4/8`, and calls `PTR_FUN_00019e10` with entry+0x10/+0x14. Subset of 0x19D5A. [O]
#### 0x00019DD2 `boot_diag_load_timing` — stub
Copies the two timing triplets from entry+0x1c/+0x20 into `0xFFF84BB4..0xFFF84BD8`, zeroing `DAT_fff84bbc/0xFFF84BD0` and presetting `DAT_fff84bc4/0xFFF84BD8` to 5000. [O]
#### 0x0001A278 `boot_link_tx_trigger` — logic
Marks tx busy (`DAT_fff84e88.3=1`), snapshots peripheral `DAT_fffff220` into `DAT_fff84b9c`, computes channel/length (8 default, or `DAT_fff84bf0`), and invokes `PTR_FUN_0001a2d0(5 args)` — comm transmit trigger. [I]
#### 0x0001A3B8 `boot_link_gate_call_1a4d4` — stub
Calls `*DAT_0001a4d4(param…)` only when (param_1 & 0x80)==0 — gated link-layer dispatcher. [O]
#### 0x0001A3D8 `boot_link_gate_call_1a4d8` — stub
Calls `PTR_FUN_0001a4d8(param_1,param_2)` when bit 7 clear, else returns 0. Cluster of 2 (0x1A3D8, 0x28294). [O]
#### 0x0001A3EE `boot_link_channel_state_set` — logic
For a channel id (bit 7 clear): patches the 0x28-stride channel entry at `*PTR_DAT_000121e0+0xc` — low nibble of +0x18 = param_2, zeroes +0x19/+0x1a — wrapped by `PTR_FUN_000121e4/000121e8` lock callbacks. Used by 0x19C90 teardown. [I]
#### 0x0001A3FC `boot_link_gate_call` — stub
`if ((param & 0x80)==0) call PTR_FUN_…()` — generic gated thunk; cluster of 10 near-identical copies (0x1A3FC, 0x1A410, 0x1A424, 0x1A438, 0x1A44C, 0x1A490, 0x282B8, 0x282CC, 0x282E0, 0x80760) each bound to a different callback pointer. [O]
#### 0x0001A460 `stub_gate_nop` — stub
Tests (param & 0x80) and returns — empty gated placeholder. [O]
#### 0x0001A46E `boot_link_gate_call_1a4f8` — stub
Calls `PTR_FUN_0001a4f8(param_1,param_2,param_3)` when bit 7 clear; returns 0. [O]
#### 0x0001A482 `boot_link_thunk_bf8` — stub
Tail-jump through `*DAT_fff84bf8(param_1,param_2)` (channel fn-table slot). [O]
#### 0x0001A4A4 `boot_link_gate_call_1a500` — stub
Calls `*DAT_0001a500()` when bit 7 clear. [O]
#### 0x0001A4B2 `boot_module_init_sequence` — stub
Runs seven init callbacks (`PTR_FUN_0001a504…0001a51c`, including the default-condition-table install via thunk of 0x149BC). Called from the top-level boot paths 0x11DD0/0x11EF8/0x1380C etc. [O]
#### 0x0001A520 `boot_diag_reset_rx_state` — stub
Clears `DAT_fff856d6` and refreshes `DAT_fff856d7` from a getter thunk. [O]
#### 0x0001A75C `boot_diag_did_read_dispatch` — logic
RDBI-style DID handler: for the selected row (`DAT_fff856d9`) of the table at param_1[4], copies the u16 value big-endian into the response buffer (cursor `DAT_fff84e54`), updates length fields in param_1[1], and dispatches per-class handlers from `PTR_PTR_0001a854`; special-cases DID pages 0xF2xx/0xF3xx (dynamic callback param_1[3]) and 0xF1F3 (gated on flag `DAT_fff84ce8=='Z'`). [I]
#### 0x0001B496 `boot_link_filter_select` — logic
Chooses between filter routines 0x1C2C6 / 0x1C2D6 / 0x1C2CA based on params 3/4 and the current `DAT_fff856d7`. [I]
#### 0x0001B628 `boot_apply_config_engine` — logic
Multi-phase supervisor (`DAT_fff8602c` 0..9) over the block-selection engine: installs a context at `0xFFF85D2C`, chains itself via +0x28, waits on hardware (0x15824), then drives 0x14ACC/0x15086/0x158DE to select and install the boot configuration block, with error states 6/8/9. Called by 0x184B0. [I]
#### 0x0001B8F2 `boot_get_op_status` — stub
Returns (char)`*PTR_DAT_0001ba50` — current op status byte. [O]
#### 0x0001B8FA `boot_reprogram_engine` — logic
Companion engine (`DAT_fff8602e` 0..0xc): re-installs the context at `0xFFF85D1C`, copies 0x14 bytes to `0xFFF84C70`, gates on `*PTR_DAT_0001ba50`, then drives the write engine 0x16DC0 with ranges from `DAT_fff841cc`, plus 0x16390/0x15824/0x158DE; error/finish states 6-0xc. Called by 0x184B0. [I]
#### 0x0001BBA0 `lib_cb_then_ret0` — stub
Calls `PTR_FUN_0001bc1c()` then returns 0 — tiny callback wrapper. Cluster of 21 copies (0x1BBA0, 0x1C60C, 0x240D6, 0x24130, 0x30658, 0x306E4, 0x30768, 0x307EC, 0x6E92C-0x6E98C series, 0x7CF24, 0x7CFC0, 0x7D058, 0x7D0F0), each bound to a different callback pointer. [O]
#### 0x0001BBB2 `boot_link_send_request` — stub
Builds a 6-field request descriptor on the stack (dest `PTR_DAT_0001bc18`, 0x60 bytes, mode 1) and submits it to 0x1C480. [O]
#### 0x0001BBEA `boot_link_send_simple` — stub
Builds a shorter descriptor (dest `&DAT_fff856eb`, 0x60 bytes) and submits it to 0x1C590. [O]
#### 0x0001BC3C `lib_checksum_bytes` — library
Byte-wise sum over a u16 range: adds (lo+hi) of each word to the seed, with `DAT_ffffaa00=0xb401/0xFFFFAA02=0` bus setup. Cluster of 3 (0x1BC3C, 0x30884, 0x7D568). [O]
#### 0x0001BC88 `lib_checksum_words` — library
Word-wise sum over the range (adds each u16 to the seed). Cluster of 3 (0x1BC88, 0x308D0, 0x726C4). [O]
#### 0x0001BCA0 `lib_checksum_words_checked` — logic
Wrapper for 0x1BC88 with bus-register setup; returns 0xffffffff if `DAT_ffffaa02` changed during the sum (bus error). Cluster of 3 (0x1BCA0, 0x308E8, 0x726DC). [O]
#### 0x0001BCCC `lib_crc32_stream` — library
Streaming CRC-32 over a descriptor (addr/len/max/state at +0xe): table-driven (`PTR_LAB_0001bd54`), xor-indexed, final complement; states 1/2 continue, 4 done with `~crc`. [O]

#### 0x0001BD2E `lib_crc32_init` — library
Initializes the CRC-32 descriptor (see 0x1BCCC): stores start/end, clamps length to 0x400, seeds `~param_3`, state 1. Cluster of 3 (0x1BD2E, 0x2A306, 0x7279E). [O]
#### 0x0001BD58 `boot_read_sw_info` — logic
Six-state exchange (`DAT_fff85ff1`): requests via the link vtable (`DAT_00000210+0x1c`) into `0xFFF86800` with config offsets +0xc0/+0xcc, receives via +0x20/+0x24 into `DAT_fff84e78`, retries, and at state 5 selects the info-string pointer — flash `0x6000` path picks `PTR_LAB_0001bebc` when byte `0x6003` is '0'/'1' — then calls `PTR_FUN_0001bec0` to parse (SW info block). [I]
#### 0x0001BEC4 `boot_swinfo_copy8` — logic
When the SW-info gate passes (`PTR_PTR_00006010[0x2f]` bit 0x80, +0x5c callback), copies 8 bytes from `0xFFF840A4` to `0xFFF84DE8+2`; otherwise writes the literal "no SW I" tail into `PTR_DAT_0001c180+0x102`. [O]
#### 0x0001BF44 `boot_build_id_block` — logic
Assembles the identification block at `0xFFF84000..0xFFF84050`: BCD version fields from `DAT_fff85ef8-0xFFF85F03`, the "TCU-a-b.c" string (mirrors 0x18A0A), date digits from `DAT_fff85f29-0xFFF85F2C`, plus a 0xc-byte tail from `0xFFF85FAC` and checksum words at +0x4c/+0x50. [O]
#### 0x0001C0C2 `boot_fill_swinfo_string` — logic
Fills the SW-info area at `PTR_DAT_0001c180`: default literal "ZFSWINFO string not found." (0x66 spaces padded) or copies the caller string; second branch mirrors 0x80 bytes. [O]
#### 0x0001C184 `boot_fill_adinfo_string` — logic
Same pattern at `0xFFF84D68` with default "ZFADINFO string not found." and NUL/0xff→space conversion; sets flag `DAT_fff84de8.1 = '#'`. [O]
#### 0x0001C25C `lib_read_be24` — library
Returns param[0]<<16 | param[1]<<8 | param[2] — big-endian 24-bit read. [O]
#### 0x0001C272 `stub_ret_m1` — stub
`return 0xffffffff` — placeholder. Cluster of 11 copies (0x1C272, 0x9B6E4, 0x9B6EE, 0x9B6F8, 0x9B702, 0xA4092, 0xA41FA, 0xA4656, 0xBC72A, 0x127D36, 0x132386). [O]
#### 0x0001C278 `boot_timer_start` — stub
Arms a timer: duration `DAT_fff84ca4`, start tick `DAT_fff84ca8` (from peripheral `DAT_fffff220`, min 1). [O]
#### 0x0001C290 `boot_timer_running` — stub
Returns 1 while `DAT_fffff220 - DAT_fff84ca8 < DAT_fff84ca4`; clears the timer when elapsed. [O]
#### 0x0001C2BC `boot_link_call_1c38c` — stub
Calls `PTR_FUN_0001c38c(DAT_fff800ac)` — single gated dispatch on the pending byte. [O]
#### 0x0001C2CA `boot_link_idle_cb` — stub
Calls 0x1C2BC then returns 0 — idle callback used by the filter selector 0x1B496. Cluster of 14 copies (0x1C2CA, 0x1C2D6, 0x23EEA, 0x23EF6, 0x75F7A, 0xA4B44, 0xA4B50, 0xA4B5C, 0xA4B68, 0xE5696, 0xE56A2, 0xE56AE, 0xE56BA, 0xE56C6). [O]
#### 0x0001C2E2 `boot_diag_put_scaled_pair` — logic
Writes two big-endian u16 values into the response at param[0]+1..+4 — `*DAT_fff84c1c/1000` and `[1]/10000`, saturated at 0xffff — and sets length 5. Looks like voltage/current reporting. [I]
#### 0x0001C394 `boot_scatter_fill` — logic
Scatter-copies a descriptor list: for each 8-byte descriptor gets the destination buffer (`PTR_FUN_0001c57c`) and length (`PTR_FUN_0001c584`), copying bytes into `DAT_fff84cb0` until the per-request max at param+6 or 0x300, advancing descriptor/position counters (`DAT_fff84e6c/0xFFF84E70/0xFFF84CAC`). [I]
#### 0x0001C480 `boot_link_tx_engine` — logic
Transmit state machine (`DAT_fff86033` 1..4): stage 1 fills the channel ctx `PTR_DAT_0001c58c` (src/len/tag) and validates against the mode table at param_1[3] (retry via `PTR_FUN_0001c588`); stage 2 memcpy; stage 3 runs the scatter 0x1C394; returns 1 busy / 0 done. [I]
#### 0x0001C590 `boot_link_rx_engine` — logic
Receive-side builder: fills the response record at `PTR_DAT_0001c604` (status, length via `PTR_FUN_0001c600` on ctx `0xFFF85F98`), or when bit 0x40 is set copies param_1[4] bytes into *param_1. Companion of 0x17A26. [I]
#### 0x0001C61E `boot_link_send_request_38` — stub
Descriptor builder like 0x1BBB2 but with 0x38-byte destination `PTR_DAT_0001c684`; submits to 0x1C480. [O]
#### 0x0001C656 `boot_link_send_simple_38` — stub
Descriptor builder like 0x1BBEA with 0x38-byte dest and `&DAT_fff856ef`; submits to 0x1C590. [O]
#### 0x0001C714 `boot_diag_erase_kick` — stub
Calls `*DAT_0001c948(len, addr)` from ctx `DAT_fff84c18+8/+4` — kicks the erase/program op (0x18C7C family). [I]
#### 0x0001C7DA `boot_diag_erase_report` — logic
On status 0/5 from `PTR_FUN_0001c950`, appends the erase-status array at `0xFFF84DF2` (count `DAT_fff84e74.2`) into the response and adjusts length; returns the status. [O]
#### 0x0001CA58 `boot_diag_erase_service` — logic
Erase service handler: mode 1 delegates to `PTR_FUN_0001cb14`; mode 2 maps status (0→0, 4→NRC 0x33, 0xb→0x22, 9→0x31) from `PTR_FUN_0001cb18`; default NRC 0x70. [I]
#### 0x0001CAC6 `boot_diag_transfer_status` — stub
Polls transfer status via mode-dependent fnptrs (`PTR_FUN_0001cb20` / `*DAT_0001cb24`); returns 0x13 for unknown mode. [O]
#### 0x0001CAEA `boot_diag_poll_service` — stub
Polls via mode-dependent fnptrs (`PTR_FUN_0001cb28` / `PTR_FUN_0001cb2c` → 0x1CB94); returns 5 for unknown mode. [O]
#### 0x0001CB30 `boot_diag_writemem_kick` — stub
When `DAT_fff86029==1`, calls `*DAT_0001cbd8(data, ctx+8, len)` to start a memory write; else 5. [I]
#### 0x0001CB94 `boot_diag_bulkread_service` — logic
Bulk-read service (`DAT_fff84ebc.3` states 0..3): walks the linked descriptor list from `PTR_PTR_0001cbe4` (next at +8), accumulating into the response (count `PTR_DAT_0001cd9c`), sends via link 0x1960C/0x1963E; on completion zeroes RAM windows `0xFFF88400` (0x7c00) and `0xFFF80100` (0x1f00) via `PTR_FUN_0001cdb0` and finishes with NRC 0xf/0. [I]

#### 0x0001CD08 `boot_diag_clear_writemem_state` — stub
Clears `DAT_fff86029` (write-memory armed flag used by 0x1CB30). [O]
#### 0x0001CD12 `boot_diag_writemem_check` — logic
Validates a write-memory request: gated on mask `PTR_thunk_FUN_00011c30_0001cdbc` bit 5, checks the address range against two window pairs (`PTR_PTR_0001cdc0/c4` bounds), optionally runs `PTR_FUN_0001cdc8` (NRC 0xb), then arms `DAT_fff86029=1`; returns NRC 4/9/0xb/0. [I]
#### 0x0001CFA0 `boot_state_pump_fff85ff7` — logic
Pump on `DAT_fff85ff7`: idle runs `PTR_FUN_0001cfe4` (skip if 2) then init callbacks `PTR_FUN_0001cfe8/0x0001CFEC(1)` entering state 1; state 1 waits for `PTR_FUN_0001cfe0` then clears. Uses 0x19856/0x189D0/0x12F88/0x125FA helpers. [I]
#### 0x0001CFF0 `boot_standby_enter` — logic
Standby entry: clears bit 0 of `DAT_fffe1200`, zeroes `DAT_fffe6000` (plus a callback through `PTR_FUN_0001d08c`), and sets bit 1 in `DAT_ffffd000/0xFFFFD800/0xFFFFE000` (module-stop/clock gates). Called from 0x10644. [I]
#### 0x0001D034 `boot_standby_exit` — logic
Wake path: sets bit 0 in `DAT_ffffd000/0xFFFFD800/0xFFFFE000`, runs four init callbacks (`PTR_FUN_0001d090..9c`) and the IRQ-group init 0x1D2D4. Called from 0x10644. [I]
#### 0x0001D068 `boot_pwr_mode_change` — logic
Runs `PTR_FUN_0001d0a0/0x0001d0a4` then 0x1D1A4 (rebase periodic base). Cluster of 2 (0x1D068, 0x30CEA). [I]
#### 0x0001D076 `boot_pwr_resume_check` — logic
Runs `PTR_FUN_0001d0a8` then 0x1D224(param_1,param_2,0xffffff0f) — post-wake pin/IRQ check. Cluster of 2 (0x1D076, 0x30CF8). [I]
#### 0x0001D0AC `boot_reset_periodic_counter` — stub
Zeroes the periodic counter at `*PTR_DAT_0001d1b8`. [O]
#### 0x0001D0B4 `boot_periodic_service_5000` — logic
Runs every 5000 ticks of `DAT_fffff220`: advances the counter (cap 120000 → clears `DAT_fff856f6/0xFFF856F7` and rewrites `DAT_fffe3902` bits), computes output bits (0x40/4/0x10/0x80 states incl. `DAT_fff800ec` check) into `*PTR_DAT_0001d1c4` and `DAT_ffffb804=0x8e00+bits/0xFFFFB803=0`, finally writes watchdog magic `DAT_fffe0002` = 0x5ab7 / 0x5adb (per `DAT_ffff1000` bit 2). [I]
#### 0x0001D1A4 `boot_shift_periodic_base` — stub
Sets `DAT_fff8570c = DAT_fffff220 - 3000` (re-arms the 5000-tick window) and calls 0x1DBC8. [O]
#### 0x0001D1C8 `boot_clock_switch_delay` — logic
Time-stamps around `PTR_FUN_0001d2ac/0001D2B0` with a 13/7-iteration tick delay (mode `DAT_fff85a76`), records tick-advance flag `DAT_fff85710`, and issues `PTR_FUN_0001d2b8(0x230)` when idle and the check fnptr != 1. [I]
#### 0x0001D224 `boot_wakeup_pin_check` — stub
If `DAT_fffe0420` bit 0x80 (wake pin), issues `PTR_FUN_0001d2b8(0x48)`; then runs 0x1E194(0x143). [I]
#### 0x0001D23E `boot_pwr_init_sequence` — stub
Power/clock init: latches `DAT_fff85a76` from `DAT_ffff1000` bit 2, runs callbacks `PTR_FUN_0001d2c0..cc(700)`, sets `DAT_ffffff46=4000`, `PTR_FUN_0001d2bc(0x103)`, and the clock-switch delay 0x1D1C8. [I]
#### 0x0001D27A `boot_pwr_resume_sequence` — stub
Wake-path twin of 0x1D23E: callbacks `PTR_FUN_0001d2c0/c4/d0/c8`, `d2cc(700)`, `DAT_ffffff46=4000`, `d2bc(0x103)`, 0x1D1C8. Cluster of 2 (0x1D27A, 0x3105E). [I]
#### 0x0001D2D4 `boot_irq_group_init` — logic
Initializes six interrupt groups: configures `DAT_ffffc82c/0xFFFFC82E` port bits, then for each channel byte in `PTR_DAT_0001d3d8` sets the mask bit in `DAT_fffffa00` and initializes the 0x20-stride register block (base ~-0x580 offsets: mode bits 0x10|2, zeroed priority/count words, +1). Finishes with 0x1E194(0x40) and stamps `DAT_fff856fc`. [I]
#### 0x0001D388 `boot_wait_and_sample_groups` — logic
Busy-waits until 0x10e ticks elapse (bounded by 0x439 iterations), then reads six group result words ((dword at -0x568)>>8, saturated 0xffff) into `DAT_fff85700[0..5]` — the values later consumed by 0x193B4/0x1D4AC. [I]
#### 0x0001D3DC `boot_pwr_reg_seq_3902` — stub
Delay (300/150 loops by mode), then `DAT_fffe3902 = (x&0x7fff)|0x3000` and `DAT_fffe3906 |= 0xb0`. [O]
#### 0x0001D430 `boot_spi_reg_init` — logic
Initializes the register block at `0xFFFFB800-0xFFFFB810` (cmd 0x48, magic 0xaf11 at +0x10, mode 5/0xb by `DAT_ffff1000` bit 2) plus zeroed control words at 0xFFFE6074/0xFFFE6076 — serial/SPI peripheral setup. [I]
#### 0x0001D478 `boot_spi_start_wait` — logic
Runs 0x1D430, sets `DAT_ffffb804=0x8e01`, then loops up to 11 times waiting for status bit 0x20 in `DAT_ffffb803`, clearing it and arming 0x8401 each time. [O]
#### 0x0001D4AC `boot_irq_find_source` — logic
Searches the five-entry source table `PTR_PTR_0001d4f4` for param_2, records the hit into `DAT_fff85718[param_1]` and configures it via 0x1DC40; returns 0 when not found. [I]
#### 0x0001D4F8 `boot_irq_channel_register` — logic
Registers one IRQ channel: builds flags from the 0x10-stride descriptor (type byte +6==2 → mode 2, +3 → 0x80), calls the register op 0x1DD44(id, ptr at +2, len +4, byte +0xd, flags), then records the channel map `DAT_fff8571c[idx]` / `0xFFF85760[chan]` and calls 0x1DE70 with byte +0xf. [I]
#### 0x0001D552 `boot_irq_table_init` — logic
Full IRQ subsystem init: clears the vector map `0xFFF8571C` (0x21 words) and `0xFFF85760/0xFFF85763` state, walks the 0x10-stride group table binding group descriptors (fallback 0xffffffff slots via 0x1DE9C), runs the post-init callback list at `DAT_0001d6d4+4`, then walks per-group channel lists (from `0xFFF85D18`, max 0x20 each) registering channels via 0x1D4F8, and finally invokes `PTR_FUN_0001d6dc` per entry. [I]
#### 0x0001D6E0 `boot_irq_enable` — logic
Enables/configures one interrupt: resolves its group via `0xFFF85760`, defaulting priority/trigger from the 0x14-stride table `PTR_DAT_0001d754`, takes the lock (0x1E120/0x1E138), clears the busy bit at `0xFFF85763`, and calls 0x1E2B8(group, prio, trigger, arg). [I]
#### 0x0001D758 `boot_irq_dispatch` — dispatch
Common interrupt entry: maps vector param_1 → group via `0xFFF8571C`, validates against the 0x14-stride table `PTR_DAT_0001d7ec` (id, type != 2, flag +0xf, handler at +8), then tail-calls the handler fnptr(vector, arg). Reached from vector table — the generic IRQ dispatcher. [O]
#### 0x0001D798 `boot_irq_dispatch_ctx` — dispatch
Variant of 0x1D758 from 0x1E448: sets the in-service bit at `0xFFF85763` and calls the same handler signature with a small stack context. [O]

#### 0x0001D7F0 `boot_irq_ack_channel` — logic
Clears the 0x400 pending bit of channel param_1 (`0xFFF8575e`), sets the group flag bit 3 at `0xFFF85763` for groups whose 0x14-stride table entry (`PTR_DAT_0001d844`) matches, and returns the `PTR_FUN_0001d840` result. [I]
#### 0x0001D848 `boot_irq_eoi_call` — stub
Samples channel flags via 0x1D880 and, when bit 0 is set, calls the handler fnptr +8 of the 0x10-stride table `PTR_DAT_0001d87c` with (channel, flags) — EOI callback path from 0x1E742. [I]
#### 0x0001D880 `boot_irq_sample_flags` — logic
Maps the raw status from 0x1E6E8 (bits 0x19/0x22/0x100/0x20 → 1/2/0x10/0x800) into the channel flags word at `0xFFF8575e`, preserving bits 0x600. [O]
#### 0x0001D8C0 `boot_link_mode_sync` — logic
Per-channel sync: when the state byte in `PTR_DAT_0001d9bc` has 0x80 set, compares the descriptor word+2 bit 0x10 against the wanted state and updates the cached byte at `PTR_DAT_0001d9c4`, then strips the 0x80 flag. [I]
#### 0x0001D9AA `boot_link_mode_update0` — logic
Mode controller for channel 0: runs 0x1D8C0 then, when the raw state differs from `DAT_fff85a7a`, either transitions to mode 3 (code 0x83, setting word0 bits 0x100 and clearing bit 1) or mode 2 (0x82), or copies +0x10→+0x14 and sets word0 bit 2 with state 0x80. K-line/comm mode switching. [I]
#### 0x0001D9AC `boot_link_mode_update` — logic
Indexed twin of 0x1D9AA operating on any channel (tables `PTR_DAT_0001d9bc/c0/c4` at param_1). [I]
#### 0x0001D9C8 `boot_periph_reg_init` — logic
Writes the peripheral init constants: 0x8600/0x8700/0x8800/0x8900 into `0xFFFF0820/0xFFFF0822/0xFFFF0860/0xFFFF0862`, 0x7600 into `0xFFFF0804`, 0x4B01/`0xFFFFAC00`, 0xB401/`0xFFFFAA00`, 0xA600/`0xFFFFA8E2`. Cluster of 3 (0x1D9C8, 0x314D4, 0x950A0). [O]
#### 0x0001DA24 `boot_detect_hw_variant` — logic
Reads the variant bytes at `0x80100001/2` (checks 'E'), matches `DAT_80100006` against a 4- or 10-entry table to select `DAT_fff85a74` (default 0x97), computes the flag nibble into `DAT_fff85a73` and bit mask `DAT_fff85a75`, latches `DAT_fff85a76` from `DAT_ffff1000` bit 2, and writes `0xFFFFA840 |= 0x2d00`, `0xFFFFAB00 = 0xb300`. Boot hardware-variant detection. [I]
#### 0x0001DAFC `boot_periph_init_full` — logic
Large peripheral bring-up: wake/pin regs (`0xFFFE0420`|=0x40, `0xFFFE0430`=0xe0), mode words `0xFFFF0812`=0x7810/0x7821, drive words 0x69ff/0x68ff, `0xFFFE0802`=0x5555, zeroed tables at `0xFFFE0810/0xFFFE0828/0xFFFE0C00`, `0xFFFFAC32`=0xc500, `0xFFFC1400`=1/0xFFFC1408=0xf5, `0xFFFE0400`|=0x3c10. Cluster of 2 (0x1DAFC, 0x9533C). [O]
#### 0x0001DBC8 `boot_wdt_program` — logic
Programs the watchdog/clock block `0xFFFE0000`: `0xFFFE0C04`|=0xe, `0xFFFE0002`=0x5ab7/0x5a6e, `0xFFFE0006`=0x5a80, `0xFFFE0004`|=0xa500, `0xFFFE0000`=0xa567. Cluster of 3 (0x1DBC8, 0x3180C, 0x3E930). [I]
#### 0x0001DC2C `boot_wdt_rearm` — stub
`DAT_fffe0000 = (x & 0xffdf) | 0xa500` — re-arm the watchdog counter. Cluster of 3 (0x1DC2C, 0x31870, 0x95458). [I]
#### 0x0001DC40 `boot_irq_group_config` — logic
Configures one IRQ group descriptor (32 sub-vectors at +0x100, 0x20-stride): sets mode words 0x2700, source pointer/length from param_2 (+4/+6, doubled in the bit-2 mode), copies/restores the 32-word table, and updates the state bytes `0xFFF85A77-0xFFF85A79` (mode 2 sets word bit 0x100). [I]
#### 0x0001DCF8 `boot_irq_subsystem_reset` — stub
Ungates clocks (`0xFFFFD000/D800/E000/F0000` bit 0), clears `DAT_fffe0c32` low bits, and resets channel-state bytes `0xFFF85A77-0xFFF85A7A` and the group mask `DAT_fff85774`. [O]
#### 0x0001DD44 `boot_irq_vector_program` — logic
Core INTC programming: splits vector into group (>>5) and bit (&0x1f), sets mask/polarity/trigger words at group block +0x30..+0x58 and mode word 0x2700 at the sub-register, writes the handler source address/length (direct, 19-bit shifted, or long mode per flags 0x80/0xc0) and the priority nibble, updating software masks `0xFFF8576C/0xFFF85770/0xFFF85774`. [I]
#### 0x0001DE70 `boot_irq_mask_set` — stub
Sets or clears one group bit in the enable mask `DAT_fff85774` per param_2. [O]
#### 0x0001DE9C `boot_irq_group_bringup` — logic
Brings up an IRQ group: sets word0 bit 0, resets state (`0xFFF85A79/7A`), programs source len/pair (+4/+6), stamps 0x2700 into all 32 sub-words at +0x110, zeroes the software masks `0xFFF85768..0xFFF85774`, and selects mode word 0x8041/0x8141 by param_3. [O]
#### 0x0001DF70 `boot_irq_group_latch` — stub
Latches the group mode into `0xFFF85A79/0xFFF85A7A` (both = `0xFFF85A78`) and clears bit 1 of the group word0. [O]

#### 0x0001CAA0 `boot_diag_writemem_service` — logic
Write-memory service entry: mode 1 delegates to `PTR_FUN_0001cb1c`; mode 2 validates/feeds data via 0x1CB30 (which kicks `*DAT_0001cbd8`, armed by 0x1CD12); returns NRC 0x72 for unknown mode. Caller of the 0x18CC8 data-feed path. [I]

## Unit summary

220 clusters, 364 named functions (every cluster member named). Counts by kind:
- logic: 126
- stub: 168 (many are byte-identical callback/thunk copies: 36x `stub_ret0`, 11x `stub_ret_m1`, 21x `lib_cb_then_ret0`, 14x `boot_link_idle_cb`, 10x `boot_link_gate_call`, 8x `boot_thunk_cb_149cc`)
- library: 53 (memcpy/memset/word-copy family, byte/word checksums, CRC-32, Boyer-Moore search, BE24 read)
- data-accessor: 12
- dispatch: 5 (incl. the common IRQ dispatcher 0x1D758/0x1D798 and service dispatchers 0x14334, 0x13BE6)
- irq: 0 (the handlers are dispatched via 0x1D758, itself classified dispatch)
- unknown: 0 — every cluster got at least a structural name; entries whose semantics rest on inference carry [I] markers (notably the boot_diag_* family, the flash catalog/write engine, and the flash-protection checks).

Content picture: this slice is the bootloader + boot-manager support core — (1) a K-line-style diagnostic server (state machine at 0x13EE8-0x13FA4, service router 0x14334, 0x7F/SID/NRC negative responses 0x140F6-0x14186, upload/download/erase/writemem services 0x1839C-0x1CB94 with NRC 0x31/0x33/0x70/0x72/0x78); (2) a flash block-catalog engine over the record table at 0x80100000 (magic 0x4001 blocks, 0x2000/0x4000/0x8000 sizes, marker records at block+0x80 with type bits 0x7000 — matches spec 06 boot data & spec 16 reflash EB blocks), plus the multi-phase reflash write engine 0x16DC0 and erase/verify helpers; (3) read/write protection window checks (0x180B2/0x1822C — spec 06/16 protection records); (4) clock/power/standby control with watchdog magic 0xa5xx/0x5axx at 0xFFFE0000 (0x1DBC8/0x1DC2C/0x1D0B4); (5) the INTC/IRQ subsystem (init 0x1D552, vector programming 0x1DD44, dispatcher 0x1D758).

Cross-region leads for other units:
- Callers below this unit that should cross-check names here: 0x11C08/0x11C1E/0x11DD0/0x11EF8/0x11F1A/0x11F8E (boot init/pumps), 0x10644 (standby/periph init caller of 0x1CFF0/0x1D034/0x1D9C8/0x1DA24/0x1DAFC/0x1DBC8), 0x12384, 0x13086/0x130F8/0x1319A, 0x13650-0x13A74 pumps, 0x13780 (main service pump calling 0x18A9C/0x184B0/0x18B80/0x18C54/0x18D48/0x190CC/0x1839C), 0x1380C/0x1389A/0x1389A/0x13A74.
- Functions in 0x1E4xx-0x1F4xx called heavily by this unit (0x1E120 lock pair, 0x1E194/0x1E194 cmd sender, 0x1E8xx flash IO, 0x1F0xx/0x1F1xx/0x1F4xx helpers) belong to the adjacent r00_boot slice — their unit should reuse the `boot_` prefixes established here.
- Duplicated member copies land in other units' address ranges and share names: 0x73F64/0x73F76 (r04_boot), 0x80760 (r08_app), 0x6E92C-0x6E98C/0x7CF24-0x7D1C6 (r0c/r10_app), 0x9B6E0-0x13238E (r10-r14), 0x1520B4 (r14_app) — those units will see the matching cluster hashes.
- The 0x80100000 record table and the RAM catalog at 0xFFF84540-0xFFF845E6/0xFFF84750 are the shared state between this unit's flash engine and any application-side reflash code (spec 16).
