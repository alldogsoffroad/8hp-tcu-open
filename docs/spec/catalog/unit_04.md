# Unit 04 (r00_boot)

#### 0x00026A78 `diag_tester_present_3e80` — logic
Matches a 2-byte message whose payload is `3E 80` (TesterPresent with suppress-POS bit): if len==2 and bytes match, compares param_1 against a 0x30-stride table entry selected by DAT_fff800b0 (PTR_PTR_00026ac0) and invokes handler PTR_FUN_00026ac4; returns 1=handled, 0=no. Boot-side diagnostic session keep-alive check. Called by 0x000238E8. Single-member cluster. [I] (spec 15/17 use 3E TesterPresent)

#### 0x00026AC8 `boot_flash_xfer_start_m2` — logic
Flash-transfer initiator (mode 2): requires idle flag *PTR_DAT_00026d14==0 and start<end from the caller's range descriptor (params +4/+8/+0xc), copies the range into the control block at PTR_DAT_00026d18, sets mode byte +0x14=2, then drives the downloaded flash driver: start via PTR_FUN_00026d20 / poll via PTR_FUN_00026d24 (context PTR_PTR_00026d1c). Returns 0 ok, 3 driver-timeout, 4 busy. Called by 0x00024B70 and the flash sequencer 0x00027180. [I]

#### 0x00026B32 `boot_flash_xfer_stop` — logic
Aborts the mode-2 flash transfer: clears driver flag +0xf (via PTR_PTR_00026d1c), calls close/reset pointer PTR_FUN_00026d28, returns 0 on success / 3 otherwise, and clears the busy flag *PTR_DAT_00026d14. callers='-' so it is reached via a function-pointer table. [I]

#### 0x00026B54 `boot_flash_xfer_start_m1` — logic
Same skeleton as 0x00026AC8 but sets mode byte +0x14=1, zeroes padding counter _DAT_fff84d9a and additionally calls PTR_FUN_00026d2c before starting the driver transfer (mode-1 variant, e.g. erase vs program path). Range-checked identically; called from 0x00024B70, 0x00027180 and the fill writer 0x00026D34. [I]

#### 0x00026BC6 `boot_flash_xfer_prog_chunk` — logic
Programs the next chunk of the active transfer: caps the piece at 0x800 bytes and at the 0x2000-byte driver window budget (DAT_fff88bec), passes (dest, len) to driver function PTR_FUN_00026d30, and on success advances the transfer address and the pending-remainder _DAT_fff84d9a. Handles states 1/4/5, 8 aborts. Returns 1=more, 0=done, 3=driver fail, 4=idle. [I]

#### 0x00026C68 `boot_flash_xfer_finish` — logic
Transfer finaliser: when addr-1==end, state 5 pads the last 0x100-byte block with 0xFF (unless DAT_fff88bec says padded), state 7 clears the flag and returns 0 (ok), state 8 returns 3 (error); states 1/4/6 return 1 (busy). Works on the PTR_DAT_00026d18 control block and driver context PTR_PTR_00026d1c. [I]

#### 0x00026CEC `boot_flash_xfer_abort_clr` — logic
Hard abort of the driver transfer: clears driver flags +0xb and the +8 transfer counter via PTR_PTR_00026d1c, calls PTR_FUN_00026d28 (close), returns 0/3, clears *PTR_DAT_00026d14. callers='-' (pointer-table entry). [I]

#### 0x00026D34 `boot_flash_write_fill_pattern` — logic
Three-state machine (DAT_fff84e0f 0/1/2) that programs a repeated 256-byte fill pattern (param_2, 0x00 or 0xFF) over the range in param_1: state 0 starts via 0x00026B54, state 1 fills a 256-byte buffer with param_2 and programs it via 0x00026BC6, state 2 finalises via 0x00026C68. This is how the 256-byte validity markers (0x00 and 0xFF, spec 06 §1) get written. Called by the sequencer 0x00027180. [I]

#### 0x00026DBE `boot_flash_read_poll_state` — logic
Polls the read/upload state machine DAT_fff88bff: 7→clear and return 0 (done), 1/2/3/6→2 (busy), 8→clear and return 3 (error), else 4. Called by 0x00024B70 and 0x00027180. [I]

#### 0x00026DF8 `boot_flash_read_seq_step` — dispatch
Step function for the read/verify side state machine DAT_fff88bff (cases 1..8): starts via PTR_FUN_00026fec on DAT_fff84359, builds/starts segment list (0x00026F7A, 0x00026FD0), steps the driver (PTR_FUN_00026ffc/00027000), moves 0x400-window chunks with the DAT_fff88bec byte budget, and parks in 7/8. Companion of the program-side sequencer 0x00027180. [I]

#### 0x00026F68 `boot_flash_read_abort` — logic
Aborts a read/verify transfer: invokes driver pointer PTR_FUN_00027004 and clears the state DAT_fff88bff=0. Called by 0x00024B70. [I]

#### 0x00026F7A `boot_flash_read_prep_seglist_crc` — logic
Prepares a readback/verify job: resets the driver context (PTR_FUN_00027008), adds the base range (param_1+8/+0xc) and then walks the linked segment list at param_1+0x10 (next via [3], optional duplicate when flag [2]==1) calling PTR_FUN_0002700c per segment, then computes a check value via PTR_FUN_00027010 with DAT_fff8435c and stores it at driver ctx +0x11, arming +0xf=1. Called by 0x00026DF8. [I]

#### 0x00026FD0 `boot_flash_read_set_count` — logic
Arms the read transfer count: stores DAT_fff8435c as the first word of the driver context (*PTR_PTR_00026ff0) and sets its +0xb flag. Called by 0x00026DF8 cases 4. [I]

#### 0x00026FE4 `boot_flash_read_drv_close` — stub
4-instruction tail-call of driver pointer PTR_FUN_00027014 on the read context PTR_PTR_00026ff0 (close/flush of the read driver). [I]

#### 0x00027018 `boot_mem_march_test` — logic
Word-wise march-style memory test over param_1[0..param_2>>2]: pass 0x00000000, 0xFFFFFFFF, then a pattern fetched from the literal pool (_LAB_000270b4/b8), returning the first failing address or 0. Decompiler output is degraded at the tail (literal-pool reads typed as code), but the march structure is clear. callers='-': reached via pointer table; plausibly the FCU-RAM check before the downloaded flash driver runs (cf. spec 16 §2 march test at 0x10C9C). [I]

#### 0x000270BC `boot_ptr_set_free_run_cnt` — data-accessor
Stores the current free-running counter DAT_fffff220 (the ms timebase also used by 0x00027100) into **PTR_PTR_000270cc — snapshots "now" into a deadline pointer. Called by 0x00024974. [I]

#### 0x00027100 `boot_timebase_update_tick` — logic
Swaps in a new time base: saves old DAT_fff84230, installs param_1[1], derives DAT_fff84d94=(t+500)/1000 (ms→s rounding), advances the deadline in *param_1 by the base and bumps its tick counter +4, then calls pointer hooks PTR_FUN_00027178/0002717c with DAT_fff84dfc before restoring the old base — a time-slice wrapper around a periodic hook. Called by 0x000270D0; calls 0x0002493E/0x000249C8. [I]

#### 0x00027180 `boot_flash_seq_engine` — dispatch
The 16-state flash-programming sequencer (state DAT_fff84238, sub-state DAT_fff8423c): selects a block from the 0x44-stride logical block table (PTR_LAB_00027450/000279a0 — spec 06 §1 table at 0x60B4), reports progress as a bit mask (2,4,8,...,0x10000) via PTR_FUN_0002744c/0002799c, drives erase (case 3/4), program (5/6), verify (8/9), marker fill 0x00 (0xb via 0x00026FD0-family and 0x00027EBC range lookup 0x00027F02), and error exit 0xf. 812 instructions; called by 0x0002F3F0/0x0002F5A4. Cross-ref spec 16 §2 (downloaded flash driver), spec 06 §1. [I]

#### 0x000278BE `boot_flash_req_dispatch` — dispatch
Router for flash-service requests: cmd 1/3 stash the range param_2 at +0x14..+0x24 and start driver op 1/4 via PTR_FUN_00027c1c + handshake 0x00027F32 (0x55), cmd 4 computes a XOR-validated block mask over the 6-entry 0x44 table (PTR_LAB_00027c24) then validates ranges, cmd 5/6/7/0xb delegate to 0x00027CD2/0x00027D3E/0x00027D7E/0x00027E2E, default 0x00027E94. Called by 0x0002F2B0/0x0002F354/0x0002F3F0/0x0002F5A4 (the session-level wrappers). [I]

#### 0x000278C2 `boot_block_desc_lookup` — data-accessor
Returns a pointer to a 0x44-byte logical-block descriptor: with param_2==-2 uses the currently selected block DAT_fff84e11 (unless state 0xf), otherwise takes the bitmask from PTR_FUN_000279b8, finds its lowest set bit via 0x00027F02 (must be <7) and indexes PTR_LAB_000279a0. This is the boot-side block table accessor (spec 06 §1, table at 0x60B4). [I]

#### 0x0002790A `boot_flash_get_status` — data-accessor
Status of the flash-service context *PTR_DAT_000279b4: 0=idle, 1 when state 0, 0xe=finished(0), 0xf=error code DAT_000279bc, else 2 (busy). Called by the session wrappers 0x0002F258/0x0002F354/0x0002F3F0/0x0002F5A4 and by 0x00027984's target. [I]

#### 0x0002792E `boot_flash_ctx_full_clear` — logic
Zeroes the whole flash-service control block at PTR_DAT_000279b4 (offsets 0..0x6c) and the response flag block at PTR_DAT_000279c0. Invoked as thunk from 13 sites (cluster 0x00027984). [I]

#### 0x0002795A `boot_flash_ctx_clear` — logic
Lighter reset of the same control block (0..0x30) plus globals DAT_fff84e10, DAT_fff84290/94 (current block/aux), DAT_fff84e13. Called by 0x0002F3F0 between operations. [I]

#### 0x00027984 `boot_flash_ctx_full_clear_thunk` — stub
2-instruction thunk to 0x0002792E. 13 members spread across the image (0x00027984, 0x0002F2AC, 0x00030FA8, 0x00068AD6, 0x000760CC, 0x00091FF8, 0x000A736A, 0x000C2754, 0x000DC086, 0x000EB344, 0x0013CAE0, 0x0014EE9E, 0x00154FDA) — compiler-duplicated veneers, all named the same. [O]

#### 0x00027988 `boot_flash_ctx_set_flag1` — stub
4-instruction setter: *PTR_DAT_000279c0 = 1 — sets the first byte of the response-flag block zeroed by 0x0002792E (flash-service enable/active flag). callers='-'. [I]

#### 0x00027990 `boot_flash_ctx_flag_get` — data-accessor
3-instruction getter returning (char)*PTR_DAT_000279c0 — the enable/active flag set by 0x00027988 and cleared by 0x0002792E. Called by 0x0002A35C. [O]

#### 0x000279C4 `boot_flash_access_rule_check` — logic
Protection-matrix check: iterates the 0x10-byte rule table at PTR_LAB_00027c08 (*PTR_DAT_00027c0c entries); a rule fires when (param_1 & rule[0]) && (rule[4] & mode-from-0x00025C4C-thunk) && (rule[8] & 1<<sel-from-0x00025C08-thunk) && (rule[0xc] & param_2). Returns 1=allowed. Guards every flash-service command; matches the protection record semantics of spec 06 (record `bhlc0505` at 0x3D12C). [I]

#### 0x00027A42 `boot_flash_req_dispatch_v2` — dispatch
Register-state variant of 0x000278BE (same commands 1/3/4/5/6/7/0xb, same 0x44-stride block tables PTR_LAB_00027c20/00027c24) with an extra param_3 gate that sets ctx state 1 or 4 instead of returning early; helpers 0x00027CD2/0x00027D3E/0x00027D7E/0x00027E2E rely on r10/r13/r14/gbr left live by this function. callers='-': function-pointer entry (flash-service API table). [I]

#### 0x00027CC4 `boot_flash_frag_set_state4` — stub
Compiler-split fragment of the 0x00027A42 dispatcher: if live register r10==1 writes state 4 into *unaff_gbr (the ctx). Only callable from 0x00027A42 with those registers live. [O]

#### 0x00027CD2 `boot_flash_cmd5_data_prep` — logic
Command-5 body (from 0x00027A42): verifies protection via 0x000279C4(blockdesc,4) for block<=5, copies the range words from unaff_r14 into ctx +0x28..+0x2c, anchors DAT_fff84260 as the data source, and sets state 5 when r10==1 — i.e. arms a data (program/erase-data) operation. [I]

#### 0x00027D3E `boot_flash_cmd6_arm` — logic
Command-6 body: protection check 0x000279C4(blockdesc,8), then sets ctx state 6 when r10==1 and returns unaff_r13. Small arm/verify step of the flash-service API. [I]

#### 0x00027D7E `boot_flash_cmd7_key_start` — logic
Command-7 body: copies a 10-word parameter record from unaff_r14 into ctx +0x34..+0x58, invokes driver op 7 via PTR_FUN_00027eb0, runs the 0xAA-keyed selection/validation 0x00027F32 (stack params), clamps the range to the selected 0x44 block descriptor (base at +0x3c) and sets state 8. Looks like "start erase/program with key". [I]

#### 0x00027E2E `boot_flash_cmdB_key_start` — logic
Command-0xb body: stores the parameter pointer at ctx +0x5c, driver op 0xb via PTR_FUN_00027eb0, 0xAA-keyed validation via 0x00027F32, sets state 0xb. Companion start for a second operation class (read/verify side). [I]

#### 0x00027E94 `boot_flash_cmd_default_err` — stub
6-instruction default arm of the 0x00027A42 dispatcher: returns constant DAT_00027eb8 (unsupported-command error code). [O]

#### 0x00027EBC `boot_flash_next_block_pick` — logic
Sequencer helper (called from 0x00027180 case 1): picks the lowest set block bit from *param_2 via 0x00027F02; no bits left → selects DAT_fff84e10 and returns 3; otherwise stores the index into *param_1, clears that bit (mask -0x20000<<n | 0xffff, i.e. blocks below 0x20000 stay) and returns 1. [I]

#### 0x00027F02 `lib_find_first_set_bit_6` — library
Scans bits 0..5 of the argument by successive single-bit shifts and returns the index of the lowest set bit, or 0xff when all clear. Used by block-mask handling throughout the flash services. [O]

#### 0x00027F32 `boot_flash_block_select_validate` — logic
Block selection + protection gate keyed by param_3 (0x55 or 0xAA): picks lowest set bit of *param_2 (0x00027F02), consults one-shot flag DAT_fff84e12, validates each candidate block against the protection rules (0x000279C4 with descriptors from PTR_LAB_00028018, 0x44 stride) and clears failing bits from the mask. 0xAA path additionally walks remaining bits. [I]

#### 0x000280B8 `diag_set_nrc_12_22` — logic
After a state check (sub-state *PTR_DAT_0002814c==1, probe via PTR_FUN_00028150 on DAT_fff842d8, result DAT_fff842e6==4), sets response byte DAT_fff84d8c[2] to 0x12, upgraded to 0x22 when DAT_fff842e0 equals the big-endian dword assembled from DAT_fff84164 — NRC-style rejection (0x12 unsupported / 0x22 conditionsNotCorrect), then advances sub-state to 2. callers='-': pointer-table hook. [I]

#### 0x00028158 `boot_msg_buf_register` — data-accessor
Registers a message/request buffer: DAT_fff84320=param_1, DAT_fff84324=param_1[4], DAT_fff84e16=*param_1 (type byte). callers='-': pointer-table entry; the 0xfff84320 block is the boot-side comm buffer area. [I]

#### 0x00028232 `unknown_boot_hook_2f0` — unknown
Calls thunk 0x00025C08, passes its result to PTR_FUN_00028268(x,0), polls PTR_FUN_0002826c; on zero calls DAT_00024254(0x2f0). The 0x2f0 constant (752) and guard meaning are unclear; likely a periodic/housekeeping hook of the boot session (timer re-arm value?). callers='-'. [I]

#### 0x00028274 `boot_fwd_5arg_if_en` — logic
Guarded forwarder: unless param_1 has bit 7 set ("disabled"), tail-calls the handler slot *DAT_00028344 with all five arguments. Part of the boot-side dispatch veneers around 0x00028300-0x0002833C. [O]

#### 0x000282AA `boot_fwd_2arg_if_en` — stub
Same enable-bit-7 guard as 0x00028274 but 2-arg: calls PTR_FUN_0002834c only when !(param_1 & 0x80). Called by 0x00025A58. [O]

#### 0x00028300 `boot_fwd_2arg` — stub
5-instruction pure forwarder to handler slot PTR_LAB_00028364(param_1,param_2). callers='-': entry-table veneer. [O]

#### 0x00028308 `boot_fwd_3arg` — stub
Pure 3-argument forwarder through PTR_FUN_00028368, returns 0. Two identical copies: 0x00028308 and 0x00080798 (duplicated veneer). [O]

#### 0x00028318 `boot_fwd_2arg_jt` — stub
Forwarder calling the RAM-resident handler pointer DAT_fff8432c(param_1,param_2) (Ghidra could not recover the jump table at 0x00028320 — indirect call). [O]

#### 0x0002832C `boot_fwd_0arg_if_en` — stub
Enable-guarded 0-arg forwarder: calls PTR_FUN_00028370 only when !(param_1 & 0x80). Two copies: 0x0002832C and 0x000807BC. [O]

#### 0x0002833C `boot_fwd_1arg` — stub
3-instruction 1-arg forwarder through DAT_00028374. [O]

#### 0x00028378 `boot_init_session_state` — init
Initialises a boot-session state block: DAT_fff84df4=1, DAT_fff84df5=0x78, DAT_fff84df6=0x21, DAT_fff84144=*(_DAT_fff8413c+0x14)+4, clears DAT_fff84140/fff84df1/fff84df3, calls reset hooks PTR_FUN_00028428 and 0x000283E2, latches PTR_FUN_0002842c's result into DAT_fff84def and stops (or runs PTR_FUN_00028430). Called from 0x00026120. [I]

#### 0x000283E2 `boot_clear_stat_counters` — logic
Clears DAT_fff84204/DAT_fff84200 and calls PTR_FUN_00028434(0)/PTR_FUN_00028438(0) — zeroing the session statistics/counters after 0x00028378's init. [I]

#### 0x00028400 `boot_set_active_data_block` — data-accessor
Stores an active data-block pointer: DAT_fff84208=param_1, DAT_fff84e07=type byte, DAT_fff8421d=1 (valid), DAT_fff84210=param_1[0xc] (length/limit). Called by 0x00026120 during session init. [I]

#### 0x00028420 `boot_set_cfg_base_ptr` — data-accessor
3-instruction setter: _DAT_fff8413c = param_1 — stores the configuration/base pointer read back by 0x00028378 (+0x14). Called from init 0x00026120. [O]

#### 0x000286FA `diag_did_dispatch_22` — dispatch
ReadDataByIdentifier-style DID dispatcher: fetches the DID (u16) from a list at param_1[4] indexed by DAT_fff84e1a; DIDs 0xF2xx/0xF3xx are resolved through callback param_1[2] (0xff/0xfe = not found), all others go through a binary search of the sorted 0xC-stride DID table at *param_1. On a hit (param_3==0) it writes the DID big-endian into the response buffer at cursor DAT_fff84d94, fills the ctx at param_1[1], and calls the handler table PTR_PTR_00028884[entry.type]. Fits the F1xx/F2xx identifier scheme of spec 17. [I]

#### 0x00028DE0 `boot_run_deferred_cb` — logic
Runs a deferred callback: when DAT_fff8b040 bit 0 is set, calls PTR_FUN_00028e18(*PTR_DAT_00028e14), then always clears DAT_fff84e23. Invoked from the init chain 0x000294E0. [I]

#### 0x000294E0 `boot_init_hooks3` — logic
8-instruction init aggregator: calls three pointer hooks in order (PTR_FUN_00029534/00029538/0002953c), one of which resolves to 0x0002AD08, and also reaches 0x00028DE0/0x00023514. 7 byte-identical copies across the image (0x000294E0, 0x0006D6EC, 0x0007CC56/66/76, 0x0007E500, 0x000832F0) — compiler/linker-duplicated init sequences. [I]

#### 0x000294F0 `boot_init_hooks2` — logic
Same pattern with two pointer hooks (PTR_FUN_00029540/00029544, resolving to 0x0002ADD4/0x0002AE60). 5 copies (0x000294F0, 0x0002AC78, 0x0006A104, 0x0007CB90, 0x0008EDF6). [I]

#### 0x00029680 `boot_get_dat_da0` — data-accessor
Returns the 16-bit value DAT_fff84da0. Callers 0x0002FE18/0x0002FE6A. Purpose of the counter is not visible here. [I]

#### 0x000298A4 `boot_update_status_e2c` — logic
Recomputes the status byte DAT_fff84e2c[2]: base value DAT_fff84460, OR 0x80 when flag bit 1 of *(DAT_fff84484+1) is set. callers='-': pointer-table hook (status publisher for the boot session). [I]

#### 0x000298BC `boot_get_status_e2c` — data-accessor
Getter for the status byte computed by 0x000298A4. Called by 0x000300A0. [O]

#### 0x000298C4 `boot_tx_pump_n` — logic
Pumps up to min(param_1,3) (+1 when *DAT_fff84484 bit 0x20) items by repeatedly calling PTR_FUN_00029a48(param_2,param_3); counts successes, and if any, advances the pending counter at PTR_DAT_fff84db0 and sets bit 0 of *DAT_fff84484. Queue-drain helper for the boot-side transmitter. [I]

#### 0x0002A116 `boot_reg_handler_2a128` — stub
4-instruction registration forwarder: PTR_FUN_0002a15c(param_1, PTR_DAT_0002a128) — installs a callback with context PTR_DAT_0002a128. Called by 0x000300A0/0x000301D4. [I]

#### 0x0002A164 `boot_drv_ring_write` — logic
Writes into the flash driver's 0x2000-byte ring buffer at DAT_fff88860 (base *PTR_DAT_0002a26c, fill counter *PTR_DAT_0002a268): splits the write across the wrap via two memcpy-like calls through PTR_FUN_0002a270 (resolves to 0x000243FC), refuses when it would exceed 0x2000 pending. This is how 0x00026BC6 feeds program data to the downloaded FCU driver. [I]

#### 0x0002A1E0 `boot_drv_ring_reset` — logic
Resets the driver ring: base and both cursors (DAT_fff88860/_DAT_fff88864) to buffer &DAT_fff8b180, pending count 0. Called at transfer start (0x00026B54/0x00026F68 paths). [O]

#### 0x0002A1FC `boot_drv_ring_read` — logic
Consumes from the same ring at the read cursor _DAT_fff88864 (wrap-split via PTR_FUN_0002a270 into param_1) and decrements the pending count — the readback/verify direction used by 0x00026DF8. [I]

#### 0x0002A274 `lib_crc16_table_update` — library
Straightforward table-driven CRC-16 (256x16 table at PTR_LAB_0002a2a0): crc = (crc<<8) ^ table[((crc>>8)^byte)&0xff] over param_1..param_2 with seed param_3. callers='-': library entry used via pointer. [O]

#### 0x0002A2A4 `lib_crc16_ctx_update` — library
Incremental CRC-16 over a context {ptr,end,crc,len,mode}: iterates table lookups (256x32-aligned table at PTR_LAB_0002a32c, entries used as u16), updates ctx->ptr; on end-of-data stores ~crc and returns 4, on length cap returns 2 (partial). This is the probe PTR_FUN_00028150 used by 0x000280B8 to compute the CRC compared at DAT_fff842e0. [O]

#### 0x0002A35C `boot_addr_to_block_mask` — logic
Maps an address pair (param_2[2]/param_2[3]) to a logical-block bitmask for ops 1, 4, 7 and 0xb, walking the six 0x44-byte block descriptors (PTR_DAT_0002a568/PTR_LAB_0002a56c): each op uses its own offset pair relative to base +0x1c and honors the entry's flag bits (exact vs range match; op 7 additionally gates on PTR_FUN_0002a570 and flags 0x80/0x40/0x20/0x10). Companion of the block-descriptor accessors (spec 06 §1 block table). [I]

#### 0x0002A574 `boot_id_to_block_mask` — logic
Maps an id (param_2) to a block bitmask by comparing against the first word of six 0x44-stride descriptors at PTR_DAT_0002a5c8. Used by 0x0002A35C's non-address path. [I]

#### 0x0002A598 `boot_addr_range_valid` — logic
Range validator for the sequencer 0x00027180: accepts code-flash ranges below 0x200000 or the 0xFFF80000-0xFFF9FFFF window (data-flash address window), rejecting end<=start and out-of-window cases (returns 0xf). The first branch's `return param_2 <= param_1` looks like a decompiler artifact of the same T-bit comparison. [I]

#### 0x0002A5CC `boot_blk_value_get` — data-accessor
Returns the u32 at PTR_DAT_0002a60c[param_1] for param_1<7, substituting the current block index *PTR_DAT_0002a608 when param_1==0xff. Per-block scratch/value array of the flash sequencer. [I]

#### 0x0002A5F0 `boot_blk_value_set` — data-accessor
Setter counterpart: stores param_2 into PTR_DAT_0002a60c[param_1] (index<6) and records param_1 as the current block index. [I]

#### 0x0002A610 `boot_seq_state_map` — logic
Translates the sequencer sub-state *(short*)PTR_DAT_0002a6c4: 0→1, 1→2, 2→0, anything else→3. Called by 0x00027180. [O]

#### 0x0002A63C `boot_diag_reset_resp_state` — logic
Resets the diag response state: sub-state 0, NRC byte DAT_fff84d8c[2]=0x11 (serviceNotSupported default), active handler index *PTR_DAT_0002a6cc = -1. [I]

#### 0x0002A650 `diag_nrc_is_22` — logic
Returns 1 when the latched NRC DAT_fff84d8c[2] equals 0x22 (conditionsNotCorrect, the value 0x000280B8 can set). [O]

#### 0x0002A66A `diag_call_active_handler` — dispatch
If a handler index is active (*PTR_DAT_0002a6cc != -1) and its slot in table PTR_PTR_0002a6d0 is non-null, tail-calls that handler (indirect jump; Ghidra could not recover the target). [O]

#### 0x0002A682 `diag_service_invoke` — dispatch
Service invocation entry from the sequencer 0x00027180: sets NRC default 0x11, stores request params (DAT_fff84164=param_3, DAT_fff84d90=param_4 — the same globals 0x000280B8 compares) and calls the 5-arg handler from table PTR_PTR_0002a6d4[idx]. [I]

#### 0x0002A6BC `diag_set_active_handler_idx` — stub
4-instruction setter: *PTR_DAT_0002a6cc = param_1 — installs the active handler-table index consumed by 0x0002A66A/0x0002A682. Called by the sequencer 0x00027180. [O]

#### 0x0002AB0E `boot_issue_xfer_req` — logic
Sets DAT_fff84e18=1, copies *PTR_DAT_0002ab5c into *PTR_DAT_0002ab94, and issues a request through the guarded 5-arg forwarder 0x00028274 (PTR_FUN_0002ab84) with channel *PTR_DAT_fff84e00, address (DAT_fff800b0 + descriptor+0x18) or DAT_fff843f4, and length 8/param. callers='-': pointer-table entry of the transfer layer. [I]

#### 0x0002AD08 `rng_init` — logic
Seeds the PRNG: byte counter *PTR_DAT_0002ad80=4 (force refill) and generates a fresh word via 0x0002AD66 into _DAT_fff843fc (also mirrored to DAT_fff843f8). Part of the boot init hooks (0x000294E0). [O]

#### 0x0002AD1E `rng_read_bytes` — logic
Extracts param_1 bytes little-endian from the 32-bit PRNG word _DAT_fff843fc, refilling through 0x0002AD66 every 4 bytes (counter at *PTR_DAT_0002ad80). Presumably builds the SecurityAccess seed (spec 17 §4). [I]

#### 0x0002AD66 `rng_step_lcg` — library
LCG step: DAT_fff843f8 = (DAT_fff843f8 ^ DAT_fffff220) * 9 — mixes the free-running counter into the state each step and the result becomes the current random word. [O]

#### 0x0002AD84 `boot_timer_arm_seconds` — logic
Arms the one-shot timer block at *PTR_DAT_0002ae18: duration = param_1*1000000 (us-scale, matching the DAT_fffff220 free-run counter) and start = now (or 1 if the counter is 0). Called from the deferred-cb path 0x00028DE0. [I]

#### 0x0002ADAA `boot_timer_remaining` — logic
Returns remaining us (duration - (now - start)) while armed; when expired or disarmed clears the start mark and returns 0. Companion of 0x0002AD84. [O]

#### 0x0002ADD4 `boot_timer_expiry_poll` — logic
Init-hook poll (from 0x000294F0): when the timer is armed and 0x0002ADAA says expired, increments DAT_fff8b040 (if bit0 set) and calls PTR_FUN_0002ae1c; anything but 1 disarms the timer. [I]

#### 0x0002AE20 `plant_set_state` — stub
3-instruction setter of the plant-protocol state byte *PTR_DAT_0002af50 (values 1..3 seen). [O]

#### 0x0002AF32 `plant_state_in_progress` — logic
Returns 1 when the plant status byte *DAT_0002af4c is one of 0x22 ('"'), 0x33 ('3'), 0x44 ('D') — the in-progress codes of the protocol state machine in 0x0002AE60. [O]

#### 0x0002AF8C `plant_handshake_poll` — logic
Two-phase handshake on DAT_fff88c01: phase 0 sets protocol state 1 (0x0002AE20) and triggers PTR_FUN_0002afdc, latching phase 1 on success; phase 1 fetches the status byte via 0x0002AE4A and returns 2 only when it equals 'f' (0x66), otherwise resets to phase 0. Called from 0x0002ADD4's timer chain. [I]

#### 0x0002AE60 `plant_state_step` — logic
Steps the plant-protocol status byte *DAT_0002af4c: 0x33 runs the active sub-handler (by *PTR_DAT_0002af50: 1/2/3 via PTR_FUN_0002af60/64+68/6c) and ends in 0x44 (ok) or 0x88 (fail); 0x44 compares the free-run counter against DAT_fff84408 and yields 0x66/0x77; 0x66/0x77/0x88 clear the state. Drives 0x000304C4-family helpers. [I]

#### 0x0002AFE0 `boot_log_evt_if_err_bits` — logic
Gate for the event logger: if param_1 has bits 0xC000 set, records event 0x44c with counter 0x10 through 0x0002AFF8. callers='-': pointer-table hook. [I]

#### 0x0002AFF8 `boot_evt_ring_push` — logic
Appends one record to the 5-entry event ring at DAT_fff84dc0/DAT_fff84db8/DAT_fff84ccc/DAT_fff84cb8 (event id, count byte at DAT_fff84e50, and two data words, the 4th taken straight from the incoming argument register); past 5 entries it only counts up to the 0xff cap. [O]

#### 0x0002B068 `boot_evt_flags_set` — stub
OR param_1 into the event-flag word DAT_fff84da0[2:3] (read out by 0x00029680). [O]

#### 0x0002B076 `boot_evt_flags_clear` — stub
Clears param_1 bits from the event-flag word DAT_fff84da0[2:3]. [O]

#### 0x0002B098 `plant_dataprog_seq` — dispatch
8-state machine (DAT_fff88c04) implementing the "RB Plant Data Prog" scan/verify: state 0 clears eight 0x2c-byte per-block records (DAT_fff8496c) and 0xc-byte summaries (PTR_DAT_0002b310); state 1 probes each block record (+0x38/+0x40 with key 0xA5 via 0x0002BDDC); states 2/3 run checksums (0x0002BC7A op 1/2); state 4 fetches the 0x4001-typed record, checks it and the 20-byte "RB Plant Data Prog" marker (0x0002B774); states 5/6 pick a block and verify it (0x0002BCBE readback, 0x0002B6DE field diff, 0x0002C03A finish); state 7 classifies the outcome (return 1/3/4/5). param_1 is the protocol context (table ptr at +0x28). 719 instructions. Cross-ref spec 17 (plant session). [I]

#### 0x0002B6DE `plant_fields_diff_mask` — logic
Compares two 17-byte plant records field-by-field and returns a diff bitmask: bits for bytes 0-5, 6-9, 10, 11-12, 13-16. Two identical copies (0x0002B6DE, 0x0006FCFC). [O]

#### 0x0002B774 `plant_is_rb_marker` — logic
20-byte comparison against the literal "RB Plant Data Prog" (PTR_s__RB_Plant_Data_Prog__0002b814) — the marker that identifies a plant data-programming record. Strong anchor tying this whole area to the plant protocol (spec 17). [O]

#### 0x0002B79A `plant_block_select_seq` — dispatch
Second 8-block state machine (DAT_fff88c05, records at DAT_fff84b30/fff84b34/fff84b38): state 1 probes +0x38/+0x40 (key 0xA5) and reads +0x80; states 2/3 checksum; state 4 validates the +0x54 marker word and classifies each of the 8 blocks by its param_1+0x39 nibble (4/2/1); state 5 runs a four-tier preference scan (flags 4&5&6, then 4|5|7, then 0xb, then 9, then smallest +0xc counter) to pick a target block, returning 1 with the index at param_1+0x19, or 2 when none qualifies. Companion of 0x0002B098. [I]

#### 0x0002BBCA `plant_checksum_range_blank_skip` — logic
Checksums the flash range [param_1, param_2] with seed param_3: the first 0x38 bytes always via PTR_FUN_0002bcfc, then in 8-byte steps; at 8-aligned addresses whose page offset ((addr&0xff)>>3) is 7/8/10 it asks PTR_FUN_0002bd00 whether the region is blank and, if so, folds in len*0xFF without reading. 0xffff result maps to 0xffffffff (error). Callers 0x0002BC7A, 0x0002C30C, 0x0002CED4. [I]

#### 0x0002BC7A `plant_checksum_ctx_run` — logic
Runs a checksum for the context {start,len,seed,result,ok} at param_1: selects mode 1/2/0 on PTR_FUN_0002bd04 (param_2 selects), computes via 0x0002BBCA over [start, start+len-1], stores the result at param_1[3] and the !=0xffff verdict at param_1[4]. Used by both plant state machines. [O]

#### 0x0002BCBE `plant_checksum_dual_pass` — logic
Two-pass checksum/compare: state 0 latches the ctx {start,len,seed} (param_2==1 enables the second pass); states 1/2 walk 0x800-byte chunks through PTR_FUN_0002bed8 accumulating DAT_fff84ba4 / _DAT_fff84ba8; when done, pass 1 optionally restarts as mode 2 (PTR_FUN_0002bedc), and state 3 reports ok only when both accumulators match and no error flag (DAT_fff84e37) was latched. Called from 0x0002B098 state 6, 0x0002C30C and 0x0002CED4. [I]

#### 0x0002BDDC `plant_blank_check_16` — logic
Blank/erase check with fill key param_2: if PTR_FUN_0002bee0 says the unit is mid-operation returns 2 (busy); otherwise switches mode via PTR_FUN_0002bedc (1/2/0) and verifies 8+8 bytes both equal the key. 0=blank, 1=data differs. Used by both plant state machines. [I]

#### 0x0002BE9E `plant_block_counters_max` — logic
Returns the maximum of the eight block counters at DAT_fff84ce0 (the values stored by the record writer at +0x28). Called from the plant TX path 0x0002FEAC. [O]

#### 0x0002BFA0 `plant_cmd30_poll` — logic
Sub-protocol poll around command result 0x1e: state 0 sets flag bit 2 on *PTR_DAT_0002c140 and issues PTR_FUN_0002c144 (retry path via 0002c148/14c/150 with DAT_fff84e38=-1 retries), state 1 waits for the 0x1e result, yielding states 2/3 (done, return 0) or -0x80 (busy). Called from the EEPROM scanner 0x0002C30C. [I]

#### 0x0002C02E `plant_cmd30_flags_clear` — stub
Clears flag bit 2 on *PTR_DAT_0002c140 and re-issues PTR_FUN_0002c144 — teardown of 0x0002BFA0's setup. [O]

#### 0x0002C03A `plant_record_extract_8` — logic
Extracts 8 data bytes plus 8 u16 values from the plant block record: normally from +0x28/+0x48, but from +0x68/+0x58 when both 0x18-byte checksum passes (0x0002BC7A op 1/2) match the stored +0x78 word; while trusted it also sums the u16s into ctx+0x54. Called by 0x0002B098 state 6. [I]

#### 0x0002C2BC `boot_count_flagged_entries` — logic
Counts entries of the 0x18-stride pointer list at param_1+0x2c (count at +0x30) whose *(u16*)(*p+2) has bit 0x80 set. Two copies (0x0002C2BC, 0x0007094A). [O]

#### 0x0002C2E4 `boot_propagate_flag_bit1` — logic
For each entry of the same list, sets/clears bit 1 of *(byte*)(p[1]+2) according to bit 1 of *(u16*)(*p+2). Two copies (0x0002C2E4, 0x00070972). [O]

#### 0x0002C30C `eeprom_ring_scan` — dispatch
13-state scanner (DAT_fff88c08) of the data-flash EEPROM record ring at 0x80100000 (spec 18 §3: 4x32KB slots, header `40 01 00 7C`): walks 0x1000-stride records checking the 0x4001 magic, dual-checksums 0x54 bytes (0x0002BC7A modes 1/2) against the +0x54 word, tracks the highest sequence (+0x4) in DAT_fff84bb0/_DAT_fff84bac, then (states 9-0xb) walks the per-record segment table at +0x40 (0x8000-flagged entries) accumulating byte and range sums (PTR_FUN_0002c980, 0x0002BBCA) into DAT_fff88914/DAT_fff88924. Finishes with PTR_FUN_0002c984 and result 0/2 at *PTR_DAT_0002c988. [I] (spec 18 §3 cross-ref)

#### 0x0002C990 `eeprom_scan_active_get` — data-accessor
Returns DAT_fff88868, the flag set to 1 when 0x0002C30C starts a scan (with the mode byte in DAT_fff88869). [O]

#### 0x0002C998 `plant_channel_seq` — dispatch
315-instruction state machine over a channel/entry list (ctx at PTR_DAT_0002cc10): state 0 counts entries (from param_2+0x2c/+0x30, or single at +0x60 when flag 8), state 2 iterates the 0x18-stride list matching on flags, resolving each with 0x0002CC4C (mode 1) and latching pointers at +0x14; then runs per-entry handler pointers (entry+8/+0xc/+0x10) with a 0x300-capped buffer from param_3, maintaining status bits in DAT_fff847de and ctx[2]. [I]

#### 0x0002CC4C `plant_table_find_entry` — logic
Generic search over a plant record table (entries: u16 id/type, u16 len, payload) anchored at block+0x80 (start/limit from ctx+0x34): mode 1 matches id&0xfff plus a type byte, modes 2/3 match on computed flag masks built from param_2 bits and the entry's 0x7000 type field (constants sourced from the data blob at 0x20fe8, i.e. the "RB Plant Data Prog" area). Returns the matched entry or 0. [I]

#### 0x0002CD86 `tbl_expand_stream` — logic
Walks 8-byte stride descriptors: per entry gets a repeat count (0x0002BF7C) and source (0x0002BF92), sums counts into param_1[2]/[3], then replays the stream copying source bytes into a destination pointer pair (PTR_DAT_0002ce94/0002ce98) — expanding a packed descriptor stream into a flat buffer. Called from the plant TX scheduler 0x00030AEC. [I]

#### 0x0002CE9C `plant_table_lookup_id` — logic
Wraps a lookup: hook PTR_FUN_0002cecc, one 0x0002CC4C mode-1 search with the id key {param_1} against ctx param_2, then hook PTR_FUN_0002ced0. Two copies (0x0002CE9C, 0x00071550); registered as handler via 0x0002A116. [I]

#### 0x0002CED4 `plant_record_write_seq` — dispatch
1230-instruction writer state machine (DAT_fff847f4, states '('.. 'I' with error codes -1..-6) that programs a new plant/EEPROM record into the data flash: '<' builds the 0x4001/0x7c header at DAT_fff84820 (seq from +0x22/ctx+0x24, 20 bytes from +0xc, status 1/0xff/0xffff — the `40 01 00 7C` header of spec 18 §3), '='/'>' program 8-byte chunks (PTR_FUN_0002d5e4, skipping offsets 7/8), ',' buffers 0x80-byte pages and issues block writes via PTR_FUN_0002d348(bank, offset, buf, flag) with CRC via PTR_FUN_0002d344, 'F'/'G'/'H' verify (checksum + readback of the 0x40..0x53 status area) and classify per-block status nibbles at param_2+0x39, ending with counters in DAT_fff84920 and queue updates (0x0002DED0/0x0002DF44). Driven with the erase prep 0x0002D9A4 and status writer 0x0002DC3C. [I] (spec 18 §3 cross-ref)

#### 0x0002D9A4 `plant_block_erase_seq` — logic
Erase-prep state machine (DAT_fff88c09) for one data block: state 0 resolves it (PTR_FUN_0002da68), state 1 blank-checks +0x38 with 0xA5 fill (PTR_FUN_0002da7c), state 3 issues the erase (PTR_FUN_0002da80), state 5 verifies +0x40, polling driver status via PTR_FUN_0002dcb4/dcb8 between steps; returns 0 ok (latching param_2+0x1b+idx=1), 1 busy, 2/3/4 error classes. Called by 0x0002CED4 state 0x14. [I]

#### 0x0002DB12 `tbl_unpack_bits` — logic
Second packed-stream decoder (mirror of 0x0002CD86 using PTR_FUN_0002dcc4/0002dcc8): per 8-byte entry, type 0/1/5 copies the current source byte, type 2 repeats the latched value, types 3/4 extract bits from a 32-bit window with computed shifts — a bit-level unpacker feeding DAT_fff84bec. Called from 0x00030BF8. [I]

#### 0x0002DC3C `plant_record_status_write` — logic
Writes and verifies the record status area: steps DAT_fff84e3c[2] through offsets 0x60/0x68/0x70/0x78 (index 0xc..0xf) of the plant record, filling 8-byte buffers from ctx+0x39/+0x44/+0x4c or zeros, programming via PTR_FUN_0002df28(idx, offset, buf, 1), polling via PTR_FUN_0002df30, and finally comparing the 0x60..0x77 range checksum (PTR_FUN_0002df38). Result codes 1 busy, 2..6 error classes. [I]

#### 0x0002DE38 `plant_queue_block_update` — logic
When ctx flag 0x40 is clear and a block index is resolvable (PTR_FUN_0002df3c != -1) and 0x0002DE7A accepts it, increments the pending counter DAT_fff84db0[2:2] and stores the block index at ctx+0x19. callers='-': pointer-driven. [I]

#### 0x0002DE7A `plant_queue_push` — logic
Pushes (id, aux) into the first free slot of the 10-entry queue at PTR_DAT_0002df40 (4-byte entries, bit7=used, 7-bit id, u16 aux at +2); returns 1 on success. Also used by the TX pump 0x000298C4. [O]

#### 0x0002DEBC `plant_queue_head_valid` — logic
Checks the queue head at *PTR_DAT_0002df40: returns false when the slot is free or id==0, 2 otherwise (decompiler bool artifact of a two-result check). Called from 0x0003032C. [I]

#### 0x0002DED0 `plant_queue_shift` — logic
Compacts the 10-entry queue: moves every entry down one slot (keeping used flags and aux words) and clears the last slot. Called by the record writer on completion/error. [O]

#### 0x0002DF44 `plant_queue_clear` — logic
Clears all ten queue entries at DAT_fff84940 (ids and aux words). Called by 0x0002CED4 on the success path. [O]

#### 0x0002DF68 `plant_sec_key_lfsr` — logic
Key computation for the plant SecurityAccess: reads the seed length (ctx+0x10) bytes big-endian (0x0002E0A8), derives a tap index from four mask/weight fields (ctx+0x54..0x60) and an iteration count from seven more (ctx+0x68..0x7c) plus base ctx+0x80, then runs the LFSR: shift left, XOR the tap word at ctx+0x14[4*index] when the shifted-out bit is set. Stores the key at ctx+4. Fits spec 17's boot-side SecurityAccess. [I]

#### 0x0002E06C `plant_sec_key_verify` — logic
Recomputes the big-endian value of ctx+0x10 bytes and compares it against the stored key (ctx+4) or the -1 tap entry — accept/reject of a tester-supplied key. [I]

#### 0x0002E0A8 `lib_be_bytes_to_u32` — library
Assembles param_3 bytes big-endian into a u32 (v = v*256 + byte). SH-2A big-endian helper used by the SecurityAccess key code. [O]

#### 0x0002F0A0 `diag_parse_two_u32` — logic
Parses a request payload: needs length>3 (at msg+6); splits payload byte 3 into two nibbles, then reads the two u32 fields at offsets 4 and 4+nibble1 (via PTR_FUN_0002f178), routing the first to *param_4 (<0x100) or *param_2, and computes field2+field1-1 into *param_3. 0=ok, 6=malformed. [I]

#### 0x0002F17C `diag_parse_req_ctx` — logic
Builds a transfer-request context at param_2: clears +0x10/+0x14 (tying DAT_fff84c84 as payload buffer), parses the two u32s via 0x0002F0A0, then extracts a trailing 1-4 byte field via PTR_FUN_0002f250 and checks the total length consistency (offset+count == message length); 0=ok, 6/9=malformed. Fits the RequestDownload-style (address, size) parser of the boot flash services. [I]

#### 0x0002F254 `diag_async_poll_33` — logic
Polls the busy marker DAT_fff84e40[2]=='3': calls PTR_FUN_0002f2a0; result 1/0 clears the marker (0=done), DAT_0002f2a4 triggers PTR_FUN_0002f2a8 then 0xd, 2 returns 1 (busy), anything else clears and returns 0xd. When not busy returns 5. Two byte-identical members (0x0002F254, 0x0002F258). [I]

#### 0x0002F2B0 `diag_req5_start` — logic
Starts the data-transfer command of the flash services: when idle (DAT_fff84e40[2]==0) and the requested size *param_2 fits *(DAT_fff84354+4), issues dispatch cmd 5 via PTR_FUN_0002f308 (-> 0x000278BE) and latches busy 0x33; size overflow returns 3, wrong-handle 4/0xd. [I]

#### 0x0002F310 `diag_poll_and_clear_flag` — logic
Runs the sub-service poll PTR_FUN_0002f350 (-> 0x0002F354); on completion clears one of DAT_fff88802/DAT_fff8880e/DAT_fff8881a selected by DAT_fff84e44[2] (0/1/2). [O]

#### 0x0002F354 `diag_req6_erase_start` — logic
Erase/finish command of the flash services: when idle and the pending size is zero, issues dispatch cmd 6 (PTR_FUN_0002f3dc) and latches '3'; then polls PTR_FUN_0002f3e4 — 1/0 = done (clears marker and *(param_1+8)), 2 = busy(1), special handle via PTR_FUN_0002f3ec, else 0xd. Size still pending returns 0xb. [I]

#### 0x0002F3F0 `diag_flash_service_step` — logic
187-instruction service orchestrator: phase 0 gates the request — a buffer at exactly 0xFFF8E000-0xFFF8E0FF maps to codes 0x78/0x22, otherwise the session flag selector DAT_fff84e44[2] (DAT_fff88802/0e/1a) yields 0x70; then issues dispatch cmd 4 (PTR_FUN_0002f588) and steps the engine (PTR_FUN_0002f594/0002f590), with phase '3' polling the sequencer (0x00027180) to completion. Return codes 0 ok, 3/4/5/9 error, 0xf with DAT_fff84136=7. Calls the whole flash-service family (0x0002792E, 0x000278BE, 0x0002790A, 0x0002795A). [I]

#### 0x0002F5A4 `diag_flash_service_step_alt` — logic
Simplified sibling of 0x0002F3F0 without the address-window gate: clears busy, polls PTR_FUN_0002f600, then dispatch cmd 4 and completion via PTR_FUN_0002f60c/0002f608; 0 ok, 3 error, 4/9 handle errors. Also wired to 0x000278BE/0x00027180/0x0002790A/0x0002792E. [I]

#### 0x0002F6A8 `diag_verify_key_gate` — logic
When sub-function param_3==4, verifies the supplied key via 0x0002F6F8 with the latched session value DAT_fff84c8c and two ROM constants (DAT_0002f748/0002f744); returns 0 on match, 0x22 (conditionsNotCorrect) otherwise. Boot-side SecurityAccess check. [I]

#### 0x0002F6F8 `plant_sec_algo_check` — library
Key algorithm: byte-swaps param_1, then checks param_2 == (bswap*0x800 + bswap>>0x16) ^ param_3 ^ (param_4 & param_1). Returns 0=match, 0x22=fail. Two identical copies (0x0002F6F8, 0x0008D262). [O]

#### 0x0002FD50 `diag_resp_append_2` — logic
Appends two bytes from the channel table entry +0x14/+0x15 (channel = DAT_fff84e00[1], entries 0x10-stride at PTR_PTR_0002fe14) to the response buffer of ctx param_1 (buffer at +0xc, cursor +0x12, limit +0x14). 0=ok, 2/3/5=overflow/wrong-state. [I]

#### 0x0002FDB8 `diag_resp_append_10` — logic
Appends the 10 constant bytes at DAT_0000ec0c (likely a block id/serial string) to the response buffer; same cursor/limit protocol as 0x0002FD50. [I]

#### 0x0002FE18 `plant_evt_200_and_idle` — logic
Returns true when the event flags (via 0x00029680) have bit 0x200 set and both DAT_fff88c0f/DAT_fff88c10 are clear — the "pending event, nothing running" gate of the plant state step 0x0002AE60. [O]

#### 0x0002FE40 `plant_trig_by_index` — logic
For index param_1<0x10 with a non-null entry in table PTR_PTR_0002feb8, invokes PTR_FUN_0002febc(1,1,**entry) — per-channel trigger dispatch. Returns 1 when fired. Called from the plant TX helpers 0x000304CC/0x000304C4. [I]

#### 0x0002FE6A `plant_evt_any_20_40_100` — logic
Returns true when the event flags (0x00029680) contain any of bits 0x20, 0x40, 0x100 — readiness gate polled by 0x0002AE60. [O]

#### 0x0002FE8E `plant_flag_by_index_set` — logic
Sets or clears bit 0x80 of the byte at table entry PTR_LAB_0002fec0[idx] (idx<0x10) according to param_2 — per-channel enable flag manipulation. Called from 0x000304D6/0x000304D0/0x000304DC. [O]

#### 0x0002FEAC `plant_max_counters_thunk` — stub
3-instruction thunk through DAT_0002fec4, resolving to the block-counter maximum 0x0002BE9E. [O]

#### 0x0002FED2 `boot_send_buf_ch0` — logic
TX wrapper for channel 0: calls the sender 0x000301D4(0, *(short*)PTR_DAT_00030110, DAT_fff889f4, len(PTR_DAT_00030114-00030118), ...) forwarding the 6 request parameters. callers='-': pointer-table entry. [I]

#### 0x0002FF0E `boot_recv_buf_ch0` — logic
RX wrapper for channel 0: calls 0x0003032C(0, DAT_fff889f4, param_1, param_2, len) — queue a receive for the channel-0 buffer. [I]

#### 0x0002FF38 `boot_send_buf_ch3` — logic
TX wrapper for channel 3, same shape as 0x0002FED2 with DAT_fff889f8/PTR_DAT_0003011c..00030124. [I]

#### 0x0002FF74 `boot_recv_buf_ch3` — logic
RX wrapper for channel 3 (0x0003032C with DAT_fff889f8). [I]

#### 0x0002FF9E `boot_send_buf_ch8` — logic
TX wrapper for channel 8 (DAT_fff889fc, PTR_DAT_00030128..00030130). [I]

#### 0x0002FFDA `boot_recv_buf_ch8` — logic
RX wrapper for channel 8 (0x0003032C with DAT_fff889fc). [I]

#### 0x00030004 `boot_send_buf_ch12` — logic
TX wrapper for channel 0xc (DAT_fff88a00, PTR_DAT_00030134..0003013c). [I]

#### 0x00030040 `boot_recv_buf_ch12` — logic
RX wrapper for channel 0xc (0x0003032C with DAT_fff88a00). [I]

#### 0x00030060 `plant_chan_probe` — logic
Probes one channel via 0x000303FC: result 1 stores ready(1) and returns 0 (or 2 when param_2 forces a second step), 0 stores 2 and returns 3, anything else returns 1 unsupported. Records the status in PTR_DAT_00030140[chan]. [I]

#### 0x000300A0 `plant_chan_complete_check` — logic
3-state completion check (DAT_fff88c12): state 1 probes the channel (0x00030060), state 2 evaluates readiness — channel-table flag bit 0 (PTR_PTR_00030148), event-flag mask 0xcfff, and status bits 2/0x80/0x10 from *DAT_00030310, folding error bits 0x20/2/4 into DAT_fff88c11 — and state 3 runs the channel error handler 0x00030460 with DAT_fff88c0f=1. Returns 1 while not settled. [I]

#### 0x000301D4 `plant_tx_pump` — logic
Transmit pump behind the channel send wrappers (0x0002FED2 family): state 0 initializes (capacity via 0x00030492, enable bit in DAT_fff889e0, chunk decision), state 1 copies min(capacity, remaining) bytes from param_5 into the outgoing buffer at param_3, state 2 zero-fills the tail (or reports 3 when param_9 requests more), state 3 runs the completion check 0x000300A0. Progress in *PTR_DAT_0003031c. [I]

#### 0x0003032C `plant_rx_pump` — logic
Receive pump for the channel receive wrappers (0x0002FF0E family): serialises one channel at a time (DAT_fff88c14/15), on channel change re-arms via PTR_FUN_000303f8 and 0x0003042E and clamps the chunk size (DAT_fff84db4[2:2] = min(param_5, capacity)), then copies up to count bytes from param_2 to param_3, returning &DAT_fff889f0 (2=partial, 0=complete, length in DAT_fff889f2). Uses 0x0002DEBC for queue-state. [I]

#### 0x000303FC `plant_chan_probe_dispatch` — logic
Dispatches a channel probe by id: 3 -> PTR_FUN_000304e4, 8 -> PTR_FUN_000304e8, 0xc -> DAT_000304ec; other ids return 1. Callees resolve into the 0x00030924 family. [O]

#### 0x0003042E `plant_chan_start_dispatch` — logic
Dispatches a channel start/capability query by id: 3 -> PTR_FUN_000304f0, 8 -> PTR_FUN_000304f4, 0xc -> PTR_FUN_000304f8 (resolving to the 0x00030930 family). Byte-identical second copy at 0x00030492 (the per-channel u16 getter used by the TX/RX pumps). [O]

#### 0x00030460 `plant_chan_error_dispatch` — logic
Dispatches the post-error handler by channel: 3 -> 0x0003093C, 8 -> 0x00030956, 0xc -> 0x00030970, passing the accumulated error class from 0x000300A0. [O]

#### 0x000304C4 `plant_trig_ch3` — stub
2-instruction trigger: 0x0002FE40(3) — fire the channel-3 handler entry. Called from the plant state step 0x0002AE60. [O]

#### 0x000304C8 `plant_trig_ch8` — stub
Direct variant for channel 8: reads the table slot PTR_PTR_0002feb8+0x20 and fires PTR_FUN_0002febc(1,1,*entry) when present, returning whether it fired. [O]

#### 0x000304CC `plant_trig_ch12` — stub
2-instruction trigger: 0x0002FE40(0xc) — fire the channel-12 handler entry. [O]

#### 0x000304D0 `plant_flag_ch3_set` — stub
3-instruction wrapper: 0x0002FE8E(3, param_1) — set/clear the channel-3 enable bit. [O]

#### 0x000304D6 `plant_flag_ch8_set` — stub
Same for channel 8: 0x0002FE8E(8, param_1). [O]

#### 0x000304DC `plant_flag_ch12_set` — stub
Same for channel 0xc: 0x0002FE8E(0xc, param_1), called from 0x0002AE60. [O]

#### 0x0003052C `plant_tx_build_t4` — logic
Builds a 0x1c-byte TX request {flags, id u16, subtype 4, len 0, ctx PTR_PTR_00030648, callback param_6} and submits it via 0x00030AEC. callers='-': handler-table entry. [I]

#### 0x0003055E `plant_tx_build_t1` — logic
Same builder with subtype 1 and ctx PTR_PTR_0003064c. [I]

#### 0x00030590 `plant_tx_build_t2` — logic
Same builder with subtype 2 and ctx PTR_PTR_00030650. [I]

#### 0x000305C2 `plant_rx_build_t4` — logic
Receive-path counterpart calling 0x00030BF8 with subtype 4, ctx PTR_PTR_00030648 and the status word DAT_fff84e55. [I]

#### 0x000305EA `plant_rx_build_t1` — logic
Receive counterpart with subtype 1, ctx PTR_PTR_0003064c, status pointer &DAT_fff84e58. [I]

#### 0x00030612 `plant_rx_build_t2` — logic
Receive counterpart with subtype 2, ctx PTR_PTR_00030650, status pointer &DAT_fff84e5b. [I]

#### 0x0003066C `plant_tx_build_200` — logic
TX builder with subtype 1, fixed length 200, ctx PTR_PTR_000306dc and data pointer PTR_DAT_000306d4, submitted via 0x00030AEC. [I]

#### 0x000306A2 `plant_rx_build_200` — logic
RX counterpart of 0x0003066C (length 200, status word DAT_fff84e61) via 0x00030BF8. [I]

#### 0x000306F6 `plant_tx_build_60` — logic
TX builder with subtype 1, fixed length 0x60, ctx PTR_PTR_00030760, data PTR_DAT_00030758. [I]

#### 0x0003072A `plant_rx_build_60` — logic
RX counterpart of 0x000306F6 (length 0x60, status &DAT_fff84e64). [I]

#### 0x0003077A `plant_tx_build_48` — logic
TX builder with subtype 1, fixed length 0x48, ctx PTR_PTR_000307e4, data PTR_DAT_000307dc. [I]

#### 0x000307AE `plant_rx_build_48` — logic
RX counterpart of 0x0003077A (length 0x48, status word DAT_fff84e67). [I]

#### 0x000307FE `plant_tx_build_38` — logic
TX request builder, subtype 1, fixed length 0x38, ctx PTR_PTR_00030868 and data PTR_DAT_00030860, submitted via 0x00030AEC. callers='-': handler-table entry. [I]

#### 0x00030832 `plant_rx_build_38` — logic
RX counterpart of 0x000307FE (length 0x38, status word DAT_fff84e6a) via 0x00030BF8. [I]

#### 0x00030914 `boot_call_2f0_timer` — stub
4-instruction wrapper calling PTR_FUN_00030920 (resolves to 0x00024144) with constant 0x2f0 — the same 0x2f0 re-arm idiom as 0x00028232, a timer/kick constant of the boot session loop. Three copies (0x00030914, 0x000670FA, 0x00086B3C); reached from the init chain 0x0002AC78. [I]

#### 0x0003093C `plant_chan3_err_clear` — logic
Channel-3 error arm of 0x00030460: when the accumulated error class has bit 0x20, zeroes the channel buffer range PTR_DAT_000309c8..000309cc via memset thunk 0x000246D6. [O]

#### 0x00030956 `plant_chan8_err_clear` — logic
Channel-8 counterpart: zeroes PTR_DAT_000309d4..000309d8 on error bit 0x20. [O]

#### 0x00030970 `plant_chan12_err_clear` — logic
Channel-0xc error handler: extra hook PTR_FUN_000309dc, zeroes PTR_DAT_000309e0..000309e4 on bit 0x20, copies 10 bytes from 0xFFF8B0FB to a stack buffer and sets DAT_fff8b10e=0x40. [O]

#### 0x000309AE `plant_chan3_size_get` — data-accessor
Returns PTR_DAT_000309cc - PTR_DAT_000309c8 — the channel-3 buffer size, used by the channel-capability dispatch 0x0003042E/0x00030492. [O]

#### 0x000309B6 `plant_chan8_size_get` — data-accessor
Returns PTR_DAT_000309d8 - PTR_DAT_000309d4 (channel-8 buffer size). [O]

#### 0x000309BE `plant_chan12_size_get` — data-accessor
Returns PTR_DAT_000309e4 - PTR_DAT_000309e0 (channel-0xc buffer size). [O]

#### 0x000309EC `plant_mode_code_pick` — logic
Tiny classifier: returns 3 when param_1==0 or param_2 has bits 0x60, else 2 — picks an internal mode/code used by the plant transfer layer. Exact semantics unverified. Two copies (0x000309EC, 0x00086B58). [I]

#### 0x00030A00 `plant_stream_fill_tx` — logic
Fills the outgoing stream buffer DAT_fff84d80 from the packed descriptor list at DAT_fff84d7c: per entry consumes the entry's repeat count (PTR_FUN_00030bec), copying bytes (writing param_1[1] when a type-0/5 entry crosses the watermark at param_1+2), capping at 0x300 bytes; returns 1 when more descriptors remain. Companion of 0x0002CD86. [I]

#### 0x00030AEC `plant_tx_submit` — logic
4-state submit engine (DAT_fff88c17) behind every plant_tx_build_* wrapper: state 1 arms the transfer ctx at PTR_DAT_00030bf4 (id, watermark chunk, capacity checks), state 2 straight-copies the fixed-length payload, state 3 runs the packed-stream filler 0x00030A00, state 4 completes and clears DAT_fff88bcc. Returns 1 while busy. [I]

#### 0x00030BF8 `plant_rx_poll` — logic
Receive counterpart for the plant_rx_build_* wrappers: when the ctx flag (param_1+0x10) lacks bit 0x40, fills the DAT_fff88bd8/be0 block and fetches a chunk via PTR_FUN_00030c68, reporting 2=partial / 0=complete at *PTR_DAT_00030c6c (length at +2); bit 0x40 selects a plain length-copy. Drives the bit-unpacker 0x0002DB12. [I]

#### 0x00030CB4 `init_hw_modules` — init
Hardware/module bring-up called from 0x00024A26/0x00022224: sets bit 0 of DAT_ffffd000/DAT_ffffd800/DAT_ffffe000 (module-stop releases), then runs 0x0003193C, four pointer hooks (0x00030D30, 0x00030F54, 0x0003105E, 0x00030FA8 among them) and thunk 0x00031C4C. [I]

#### 0x00030D30 `init_periph6_setup` — logic
Configures six peripheral channels: sets pin/config bits in DAT_ffffc82c/DAT_ffffc82e (0x3f/0x0c/0xf0 patterns), and per channel (ids from PTR_DAT_00030e34) toggles its bit in DAT_fffffa00 (stop during setup, re-enable after) while writing a 0x20-stride register block (offsets -0x568..-0x578: zeroed counters, 1 at -0x576). Ends with 0x00031CEC(0x40) and timestamps DAT_fff84e74. Likely analog/capture channel setup; exact peripheral unconfirmed. [I]

#### 0x00030DE4 `periph6_read_results` — logic
Busy-waits about 0x10e us (time from DAT_fff84e74), then reads the six per-channel result registers (ch*0x20-0x568, >>8, capped 0xffff) into the u16 array at DAT_fff84e78 — the result-capture companion of 0x00030D30. Called from 0x00030CF8. [I]

#### 0x00030E40 `boot_periodic_supervise` — logic
5 ms periodic task (gate on DAT_fff84e88[0:2] vs the free-run counter): manages an uptime counter at *PTR_DAT_00030f44 capped at 120000 (us) that then parks the outputs (DAT_fffe3902 bits) and, per state byte DAT_fff84e70[1] (6 / 0xE8 / 0x19 / 0x31), writes result codes 0x40/4/0/0x10(+0x80) to *PTR_DAT_fff84e88[2] and to register DAT_ffffb804 (=code+0x8e00), finally servicing DAT_fffe0002 with 0x5ab7/0x5adb (watchdog-style magic, selected by DAT_ffff1000 bit 2 — the hardware-variant flag). Called from the 0x00024A26/0x00024B70 loops. [I]

#### 0x00030F30 `boot_periodic_kick` — logic
Back-dates the 5 ms gate by 3000 (DAT_fff84e88[0:2] = now-3000) so the next 0x00030E40 run fires immediately, then calls 0x0003180C. Called from 0x00030CEA/0x00024B70. [I]

#### 0x00030F54 `init_pins_fffe3902` — logic
Delay loop (300 or 0x96 iterations, variant-dependent via DAT_fff852c8) then sets _DAT_fffe3902 = 0x3000-pattern and DAT_fffe3906 |= 0xb0 — pin/output config after settle, part of the 0x00030CB4 init chain. [I]

#### 0x00030FAC `boot_timing_probe` — logic
Timing calibration: timestamps around PTR_FUN_00031094 with a 7/13-iteration delay loop, latches DAT_fff84e8c = (elapsed > 2) and, when fast, calls PTR_FUN_0003109c(0x230). Result feeds the variant handling of 0x00031022. [I]

#### 0x00031022 `init_hw_variant_setup` — init
Variant-aware init: DAT_fff852c8 = (DAT_ffff1000 & 4) != 0 (hardware variant flag read by 0x00030E40/0x00030F54 too), then four pointer hooks, a 700-constant call (PTR_FUN_000310b0), DAT_ffffff46=4000, PTR_FUN_000310a0(0x103), and the timing probe 0x00030FAC. Called from 0x00030C70. [I]

#### 0x00031100 `boot_trap_hang` — logic
Sets DAT_fff800ed=0x39, calls 0x000310B8 and then deliberately spins in `do {} while (true)` — a terminal halt reached from 0x00030E40's supervision and 0x0002230A. Two copies (0x00031100, 0x00092148). [O]

#### 0x00031144 `tbl_get_entry_14` — data-accessor
Returns PTR_LAB_00031150 + idx*0x14 — entry getter for the 0x14-stride table (buffer/mailbox descriptors used by 0x00031154/0x00031350). [O]

#### 0x00031154 `can_reg_mailbox` — logic
Registers a comm buffer: computes flags (1/2, |0x80 when param_2[3]!=0), calls PTR_FUN_00031328(id, buffer ptr from param_2[2], param_2[4], len param_2[0xd], flags) and on success records the id<->buffer mapping (DAT_fff84ea0[id]=*param_2, DAT_fff84f68[buf]=id) and applies param_2[0xf] via PTR_FUN_0003132c. Boot-side CAN-style mailbox registration. [I]

#### 0x00031350 `can_tx_by_id` — logic
Transmit by identifier: resolves the buffer id via DAT_fff84f68[id], takes defaults from the 0x14-stride descriptor table (PTR_LAB_000313b4: length at +4, dlc at +0xd), clears the busy flag at DAT_fff84f6c+id+1 and, when the buffer is valid (<0x60) and idle, sends via PTR_FUN_000313b8(buf, id, dlc, data). [I]

#### 0x0002AE26 `plant_mark_busy_33` — logic
Arm helper of the plant handshake: when the status is not one of the in-progress codes (0x0002AF32) and a protocol state is latched (*PTR_DAT_0002af50 != 0), writes 0x33 into the status byte *DAT_0002af4c and returns 1; otherwise 0. Called from 0x0002AF8C. [O]

#### 0x0002AE4A `plant_status_fetch` — logic
Fetches the current plant status byte *DAT_0002af4c into *param_1 and returns 1 when the protocol is idle (0x0002AF32 == 0). Used by the handshake poller 0x0002AF8C. [O]

#### 0x0002BE3E `plant_find_flagged_entry` — logic
Scans the 0x18-stride entry list (param_1+0x2c, count +0x30) for the first entry whose flag byte (entry[1][2]) has bit 0x10, selecting it into ctx+0x60 with retry count 3 at ctx+100. Two copies (0x0002BE3E, 0x000704B8). [O]

#### 0x0002BEC0 `plant_resume_scan_entry` — logic
Resumable scan (cursor DAT_fff88bee) over the same entry list: picks the next entry with flag bit 2, no 0x80, non-zero id and data bit 0 clear, selecting it into ctx+0x60 with retry 3; when nothing qualifies it clears the selection and resets the cursor. callers='-': pointer-driven. [O]

#### 0x0002F67A `diag_op_status_map` — logic
Maps the op result byte *PTR_DAT_0002f6a0 to a service status: 0→1 (idle), bit0→2 (busy), 0xfe→0 (done), anything else→3 (error). callers='-': polled via pointer. [I]

#### 0x000313BC `can_rx_indicate` — dispatch
RX indication: resolves the buffer id for the identifier via DAT_fff84ea0, validates the 0x14-stride descriptor (id match, type != 2, callback flag +0xf, handler at +8 non-null) and tail-calls the handler(id, param_2) (indirect). Called from 0x00031DF8/0x00032E38. [O]

#### 0x000313FC `can_rx_recv_dispatch` — dispatch
Variant of 0x000313BC that also sets the buffer busy flag (bit 0 of DAT_fff84f6c+id+1) and passes three stack buffers to the handler — queue a receive with fresh buffers. Called from 0x00031EA8. [O]

#### 0x00031454 `can_tx_confirm` — logic
Transmit confirmation: gets the mapped channel status via 0x0003148C and, when bit 0 is set, calls the per-channel handler from the 0x10-stride table PTR_DAT_00031488+ch*0x10+8 with (ch, status word). Called from 0x00032E14. [I]

#### 0x0003148C `can_ch_status_map` — logic
Translates the raw channel status (PTR_FUN_000314d0 -> 0x00032DB4) into flag bits (raw&0x19→1, &0x22→2, &0x100→0x10, &0x20→0x800), merges with the shadow word at DAT_fff84f60+ch*2+2 (preserving bits 0x600) and returns the new flags. [I]

#### 0x00031530 `plant_cmd_P_O_handle` — logic
Handles plant commands 0x50 ('P') and 0x4F ('O'): clamps the supplied value to <0x200000, stores it into the PTR_SUB_00031670 context (+0xc for 'P', +8 for 'O', validating +4/0 against 0xfff80001) and shifts the corresponding status bit pair (+0x12 / +0x10) left, setting the low bit on activity. Uses PTR_FUN_00031668/6c/74 hooks. [I]

#### 0x000315C0 `plant_flash_addr_window` — logic
Computes a clamped flash window value from DAT_fff84108/fff84110 (cap 0x1fff7f, -0x80/-0x100 adjustments, 0x5e00/0x5dde marker bounds), writes DAT_ffffac02 and the DAT_ffffac08 bit selected by DAT_fff852c7, burns a 0x40 delay loop and returns DAT_fff852c7. Companion of 0x00031530. [I]

#### 0x0003165E `plant_cmd_po_window_step` — logic
5-instruction aggregate: runs 0x00031530 then 0x000315C0 — one step of the 'P'/'O' command + flash-window path, called from the 0x00022224 init loop. Two copies (0x0003165E, 0x0009524A). [O]

#### 0x00031678 `boot_read_eeprom_config` — logic
Reads the boot configuration from the data-flash EEPROM record at 0x80100000: DAT_fff852c5 = (byte at +2 == 'E'), then matches the config id at 0x80100006 against a 5- or 8-entry table (PTR_DAT_00031734/00031738) to latch the value byte DAT_fff852c6 and flag bits, deriving DAT_fff852c7 (speed grade, 1<<((fl&0xc)>>2)) and DAT_fff852c8 (the DAT_ffff1000 bit-2 hardware variant). Finally writes DAT_ffffa840 |= 0x2d00 and DAT_ffffab00 = 0xb300. Called from the boot inits 0x00024A26/0x00022224. Cross-ref spec 18 §3 (record ring at 0x80100000). [I]

#### 0x00031744 `init_can_regs` — init
Register init burst for the boot-side CAN/controller: DAT_fffe0420|=0x40, DAT_fffe0430=0xe0, DAT_ffff0812=0x7810 or 0x7821 (variant via DAT_ffff1000 bits 0-1), DAT_ffff0802=0x69ff, DAT_ffff0842=0x68ff, DAT_fffe0802=0x5555, zero blocks at DAT_fffe0810/0828/0c00, then DAT_ffffa810=0, DAT_ffffa811=5, DAT_ffffac32=0xc500, DAT_fffc1400=1, DAT_fffc1408=0xf5, DAT_fffe0400|=0x3c10, DAT_fffe0806=0, DAT_fffe080c=0xfffe. [O]

#### 0x00031A48 `boot_cfg_word_pick` — data-accessor
Selects one of two config tables (PTR_LAB_00021b7a vs PTR_LAB_00031c44) by DAT_fff852c7 bit 0 and copies the word at entry+0x10 into DAT_fff84f7c. Called by the plant cmd pollers 0x0002BFA0/0x00024B70. [O]

#### 0x00031A62 `plant_fcu_seq` — dispatch
171-instruction FCU (data-flash P/E) sequencer around the ctx at PTR_DAT_00031c48 (states 3/0x15/0x19/0x1e/0x11/6): writes DAT_ffff1002=0xc300/0xc301, DAT_ffffa904=0x5501, on state 3/0 arms DAT_ffffa854=0xc401 (FCU-RAM enable, cf. spec 16 §2 FCURAME=0xC401) and starts the transfer (0x00032090 with the config-table code buffer), tracking progress in DAT_fffe1038/103c and the ctx count; finishes into 0x15/6. Called by every plant flash operation (0x0002BFA0, 0x0002C02E, 0x0002CED4, 0x0002D9A4, 0x0002DC3C, 0x00024B70). [I]

#### 0x00031BF0 `plant_fcu_reset_state` — logic
Forces the FCU ctx state to 3 and clears its [1] and [3] sub-states — restart the FCU sequence from the beginning. Called from 0x00032E84. [O]

#### 0x00031BFE `plant_fcu_restart` — logic
Lighter restart: sets the FCU ctx state to 3 and clears only sub-state [3]. Called from five points in the flash-driver layer (0x00032196/0x000321FE/0x000331C0/0x000332C4/0x00032F5C). [O]

#### 0x00031C4C `init_regs_fffec0` — logic
Clears *PTR_DAT_00031c9c and the DAT_fffec000/010/020/011/021 registers, then sets DAT_fffe0c04 |= 0x1200 — peripheral channel clear/enable step in the 0x00030CB4 init chain. [O]

#### 0x00031CA0 `init_periph_f100_variant` — init
Variant-dependent prescaler setup: DAT_fffff100=0x13/0x27, DAT_fffff102=4/9, DAT_fffff104=6/0xd, DAT_fffff106=0/1 (chosen by DAT_ffff1000 bit 2), enables the unit (DAT_fffff000 |= 1, DAT_fffff003=0x9f) and latches the control word into *PTR_DAT_00031d28. Called from 0x00031022/0x0003105E. [O]

#### 0x00031CEC `periph_reg_set_bits` — logic
IRQ-protected register update: disables interrupts (0x000319E0), ORs param_1 into DAT_fffff000 via the shadow at *PTR_DAT_00031d28, and restores (0x00031A04). Used by 0x00030D30 and the init chain. [O]

#### 0x00031D08 `periph_reg_clr_bits` — logic
Clear-mask counterpart of 0x00031CEC: under the same IRQ guard, clears param_1 bits from DAT_fffff000 through the shadow word. [O]

## Unit summary

Unit 04 = region `r00_boot`, 220 clusters / 257 functions documented. The slice is the
**Bosch boot manager / flash-services block** (spec 06 §1: `0x020000-0x03FFFF`, block ID
`bjix2201`), not application code. Four big subsystems:

1. **Flash services for the diagnostic session** (0x00026A78-0x0002A682): a request
   dispatcher (0x000278BE/0x00027A42, cmds 1/3/4/5/6/7/0xb), a 16-state program/erase/verify
   sequencer (0x00027180) with 0x44-stride logical-block descriptors (spec 06 §1 table at
   0x60B4), a protection-rule gate (0x000279C4), and the 0x2000-byte driver ring
   (0x0002A164/0x0002A1E0/0x0002A1FC) feeding the tester-downloaded FCU driver (spec 16 §2).
2. **Plant data-programming protocol** (0x0002AB0E-0x00030832): state machines around the
   literal marker **"RB Plant Data Prog"** (0x0002B774) — block scan/select
   (0x0002B098/0x0002B79A), record writer building `40 01 00 7C` headers (0x0002CED4,
   spec 18 §3), erase/checksum machinery, the data-flash EEPROM ring scanner at 0x80100000
   (0x0002C30C), SecurityAccess LFSR key (0x0002DF68) and boot-side key equation
   (0x0002F6F8), the PRNG seed source (0x0002AD66), and the plant channel TX/RX pumps
   (channels 3/8/0xc: 0x000301D4/0x0003032C + their builders).
3. **FCU / hardware init tail** (0x00030CB4-0x00031D08): module-stop release, register
   bursts (0x00031744 CAN-like, 0x00031CA0 variant prescalers), the FCU P/E sequencer with
   FCURAME=0xC401 (0x00031A62, spec 16 §2), boot config read from the EEPROM record at
   0x80100000 (0x00031678), and the boot-side CAN buffer registry (0x00031154/0x00031350).
4. **Assorted helpers**: timers on the DAT_fffff220 free-run counter, event ring
   (0x0002AFF8), march test (0x00027018), CRC16 (0x0002A274/0x0002A2A4), packed-table
   expanders (0x0002CD86/0x0002DB12), and many compiler-duplicated veneers/thunks
   (e.g. 13 copies of the ctx-clear thunk at 0x00027984).

Kind counts (257 names): logic 171, stub 41, data-accessor 17, dispatch 15, library 7,
init 5, unknown 1.

Unknown entries: 0x00028232 (`unknown_boot_hook_2f0` — polls two hooks and on failure calls
DAT_00024254(0x2f0); the 0x2f0 constant recurs at 0x00030914 but its meaning is unresolved).

Cross-region leads:
- 0x0002A6BC/0x0002A66A/0x0002A682 index handler tables PTR_PTR_0002a6d0/2a6d4 — entries
  likely live in the 0x2F000-0x31000 diag-service tables; the unit owning 0x0002F3F0/0x0002F5A4
  callers (0x0002F2AC thunks) should cross-check.
- The plant channel layer calls into 0x0002BF7C/0x0002BF92 (descriptor count/source, outside
  this unit) and 0x00032196/0x000321D4/0x00033284 (driver send/poll, outside) — those units
  should cross-check names against `plant_*`/`boot_send_buf_*` here.
- 0x000294E0/0x000294F0 init-hook veneers have copies at 0x0006D6EC-0x0008EDF6 etc.; units
  covering those addresses should reuse the same names (already emitted in this unit's CSV).
- Spec cross-refs used: 06 §1 (block layout/tables), 15 (diag/NRC), 16 §2 (downloaded flash
  driver, FCURAME), 17 (plant session/SecurityAccess), 18 §2-3 (data flash, record ring).
