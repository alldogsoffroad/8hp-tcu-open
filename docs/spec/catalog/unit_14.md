# Unit 14 (r08_app, 0x00093740–0x0009B0C2)

Cluster catalog in unit-file order. Module map established while working: the 0x937CC–0x93C42 run is
the SPI job/frame-transaction queue (spec 03 §6.2m/6.2ap pins 0x937F8=queue, 0x93B8E=check-free,
0x93C42=buffer pump, 0x93A08/0x93A74=frame workers); 0x93C64–0x93FB0 is the timer-F rotation-speed
measurement family (spec 02 §4: speed = 0.5 + scale·Δedges/Δt, decay ×1.1 — constants DAT_93df0=0.5f,
DAT_93df4=1.1f); 0x93FE0 onward is a CAN message-object/slot manager + priority TX scheduler
(spec 08 §1: two priority queues, 32-buffer driver); 0x954xx–0x95Axx are the RSPI vector-table ISR
family (spec 03 §6.2am: vectors 424–476); 0x96xxx–0x9B0C2 continues the driver layer (0x96468 job
launcher, 0x980E0 DMAC configurator — spec 03 §6.2aw).

#### 0x00093740 `lib_moving_sum_push` — library
Pushes a u16 sample into a ring buffer (ring ptr at param_1[3], capacity at state+5, write idx at
state+1) while keeping a running u32 sum at *param_1: on wrap the evicted sample is subtracted, so
*param_1 is always the sum of the last N samples. Single caller 0x0008F058. [O]
#### 0x000937CC `spi_jobq_slot_ctrs_init` — init
Queue-module init called from 0x00091DCC: runs a per-module init fn (pool 0x93a50) then, for job
indices 0 and 1 of both queue banks, frees the slot (pool 0x93a58) and writes 0xFF to the inverted
count bytes at 0xFFF941C6/C7 (bank A) and 0xFFF941CA/CB (bank B) — i.e. count=0/~count=0xFF.
Matches the "clear letters[0..1]" init pattern the spec records next door at 0x093298. [I]
#### 0x000937F8 `spi_job_queue` — logic
Queue request for a 0x48-byte SPI job descriptor (spec 03 §6.2m). Validates the count/~count pair
at 0xFFF941C4/C8 banks (capacities 10 and 5, pointer arrays at 0xFFF2834C/0xFFF28378), checks the
frame state byte (8 = ready) and slot vacancy, stores the descriptor, bumps the count with wrap,
and logs violations as event 0x253 with codes 0x11–0x16 in 0xFFF90998 plus a per-job error counter
at 0xFFF94218+2·job. 14 callers = the job starters (0x92A84, 0x92E80, 0x921A0 …). [O]
#### 0x00093902 `spi_job_dispatch_head` — logic
Pops the head job of a queue bank: sets the active flag at 0xFFF941CC+0x48·idx, validates count and
the magic at desc+0x1c against pool constant 0x00093a68, stores the current-job pointer at
0xFFF94180+4·idx, then calls the descriptor's completion handler at desc+0x14 and rewrites the frame
state byte to 0x18/0x19 (idle path) or 0x22 (calls 0x93AD8 to free and advance). Error class 0x253
codes 0x15/0x16/0x11. [O]
#### 0x00093A08 `spi_frame_abort_worker` — irq
RSPI frame worker dispatched from the vector-449 ISR path (spec 03 §6.2am): checks the job pointer
against 0xFFF94180+4·idx (mismatch → event 0x253/0x16), forces the frame state byte to 0x40, frees
the queue slot via 0x93AD8 and dispatches the next job via 0x93902, then jumps to the descriptor
handler at +0x14. callers='-' (reached from the ISR/pointer table). [O]
#### 0x00093A74 `spi_frame_done_worker` — irq
Second frame worker (ISR-dispatched): same job-pointer check, but branches on frame state 0x18 →
restart (state 0x09, clear active flag) vs other → finish (state 0x20, free slot 0x93AD8, run
descriptor handler) then dispatch next job. Spec 03 §6.2ap reads these state values as the SPCR
cycle 0x09/0x20. [O]
#### 0x00093AD8 `spi_job_slot_free` — logic
Frees a queue slot: clears the pointer-array entry (0xFFF2834C/0xFFF28378 bank by desc[1]), bumps
the count with wrap (10/5), rewrites count/~count at 0xFFF941C5/C9 (+3 inverted) and clears the
active flag at 0xFFF941CC. Inverted-pair mismatch → event 0x253/0x15. [O]
#### 0x00093B68 `spi_job_status_reset` — data-accessor
Spec 03 §6.2m pins this: with flag==0 clears the status cell (desc+0x10 state+1), else bumps the
use counter at 0xFFF94218+2·desc[0]; always zeroes the frame state byte at desc+0x10. [O]
#### 0x00093B8E `spi_job_check_free` — logic
Check-free test (spec 03 §6.2m): returns 1 when the state byte is 0; otherwise runs the descriptor
handler if the state is not 0x40 ('@'), saturates the retry counter at state+1, and bumps the use
counter at 0xFFF94218+2·idx (pool call event 0xa51 if it was 0). Same 14 starter callers as
0x937F8. [O]
#### 0x00093C24 `spi_frame_slots_fill` — library
Fills a frame-slot array with a repeated 6-byte pattern {u32, u16} — the 8-byte-per-frame RAM slots
{32-bit SPCR/pad, 16-bit SPDR} the buffer pump feeds to the RSPI/DMA engine (spec 03 §6.2ap).
Count comes from the descriptor byte at +8. [O]
#### 0x00093C42 `spi_frame_buf_pump` — logic
The buffer pump (spec 03 §6.2ap): calls 0x93C24 to fill the frame slots from the descriptor's
pattern, then zeroes the job's progress cells (+8, +0xc, +0xe, +0x14, +0x15) and latches the first
SPCR word at +4. Called per bus from the frame processor 0x00090DA2. [O]
#### 0x00093C64 `meas_speed_from_ring` — logic
Rotation-speed compute (spec 02 §4): from the new (time,value) sample and the (time,edges) ring,
computes speed = DAT_00093df0(0.5f) + scale·Δvalue/Δt with the undersample decay bound
scale/Δt·DAT_00093df4(1.1f), saturates to 0xFFFF, and runs the sample-class state machine
('0',' ',0x10,'!',0x30). Caller 0x00090E14 = per-channel speed processing. [O]
#### 0x00093DBE `meas_speed_sample_read` — data-accessor
Reads the captured sample: copies the edge-count u16 at desc+0x10 and maps the classification
nibble at desc+0x14 (0x10/0x20→8, 0x30→6, 0x40→5, else 0) to a status byte; returns the raw nibble.
Spec 03 §6.2ap reads the same mapping as SPCR values — the speed-pipeline reading fits the caller
context better. [I]
#### 0x00093DF8 `meas_speed_delta_accum` — logic
Interval accumulator for the speed family: by classification nibble (0x20/0x30 real Δedges/Δcount,
0x10 single edge, 0x40 → −1 = timeout) computes (Δcount&0xFFFFFF)·scale into out[1] and the delta
count into out+10, latching the last sample. Caller 0x00090E14. [O]
#### 0x00093E6C `meas_speed_acc_clear` — stub
9 instructions: zeroes the 14-byte speed accumulator/status struct (state, totals, last value,
counters). Called from the frame processor 0x00090DA2 and 0x93E7E. [O]
#### 0x00093E7E `meas_speed_track_update` — logic
The speed track/totalize state machine ('!'=tracking, ' '=hold, else reset via 0x93E6C): appends
(time,count) pairs to the ring, applies min-time/max-Δ plausibility gates (cfg at param_3[3],
param_3+4), maintains totalized (time,count), then publishes speed = total_count·scale/total_time
saturated to 16 bits (spec 02 §4 formula). Caller 0x00090E14. [O]
#### 0x00093FE0 `can_slot_setup` — logic
Programs one message slot: calls the driver setup (pool 0x9423c → the 0x94000/0x980E0 DMAC chain,
spec 03 §6.2ap) with (id, buf, len, flags|0x80), on success registers slot↔message in
0xFFF94220[idx] / 0xFFF94436[msg] and arms it (pool 0x94240); on failure logs event 0xa73/1.
Called from the slot-map init 0x94062 and the tx queue 0x9458E/0x949BC. [O]
#### 0x00094000 `can_slot_setup_body` — logic
Twin of the 0x93FE0 body reached with corrupted prologue registers (unaff r13/r14 — decompiler
artifact): same driver-call/register/0xa73-error sequence, and its resolved callees are 0x980E0
(the A-DMAC channel configurator, spec 03 §6.2aw) and 0x9820C. Likely the literal target of pool
PTR_FUN_0009423c. [I]
#### 0x00094062 `can_msg_slots_init` — init
Boot-time (caller 0x0006E804) init of the message-slot engine: clears 0x70 ushorts at 0xFFF94220
(=0xFFFF), 0x66 bytes at 0xFFF94436 (=0xFF) and 0xFFF9449C, the 3-word table 0xFFF94300 (=-1),
runs pool init callbacks, then for 3 groups × 5 descriptor lists (ptrs at 0xFFF844C0) walks the
0x14-byte ROM descriptors (base DAT_000944b4) and assigns each enabled message a 0x20-slot-group
index via 0x93FE0; RX-ish types 4/5 registered via pool 0x94270/0x944ac. Failures logged as event
0xa73 codes 0x2a40–0x2a45. [O]
#### 0x00094558 `can_tx_queue_clear` — init
Clears the 3 per-controller queue bookkeeping structs at 0xFFF9440C..0x00094417 ({base,count,
active,cur} ×3) and the 16 dynamic TX slots (0xFFF94318[i]=0xFFFF, state 0xFFF9431B=0). Called
from 0x94062 at init. [O]
#### 0x0009458E `can_tx_queue_insert` — logic
Priority-queue insert for dynamic TX messages: if the message already holds a static slot <0x66
inserts sorted by the descriptor's u32 at +2 (period/budget) into the 0x10-deep per-group array at
0xFFF9430C/0xFFF94318/0xFFF9431A (shifting entries and fixing 0xFFF94436 maps), assigns dynamic
slot 0x60+n in 0xFFF94220, else calls the full handler (pool 0x94818) and rebases the group.
Returns 1 on success. [O]
#### 0x00094730 `can_tx_group_pump` — logic
Cyclic group pump (caller 0x00093426 in the timer-dispatch region): completes the current entry
(verify id, set 0xFFF9449C bit0, state 0xFFF9431B=2, run the descriptor's completion fn at +0x10),
then scans the group's entries for the next enabled one (+0xF==1) and submits it via pool
0x94820 → driver 0x9829C, setting the group-active flag 0xFFF9440E. Skipped while the 0x400 DMA
burst bit is set in 0xFFF94502. [O]
#### 0x00094824 `can_tx_msg_send` — logic
Send one message by id: resolves its slot via 0xFFF94436 (must be ≥0x60), copies the DLC payload
into the entry buffer at 0xFFF94310+16·slot, sets priority/state bytes, then if the group is idle
(0xFFF9440E==0 and driver-ready) scans for the enabled entry and submits via pool 0x94948.
Callers 0x000934AC (the software-IRQ dispatcher, spec 02 §7) and 0x0009355A. Direct callee
0x979F0 = the DMA-bit test. [O]
#### 0x000948F4 `can_tx_submit_thunk` — stub
24-instruction unaff-register thunk (r1/r12/r13/r14 preserved by the caller): tail of the
0x94824 submit path — calls pool 0x94948 with (slot|1, entry, dlc&0xf, buf) and sets the group
active byte on success. Reached only via the jump from 0x94824. [O]
#### 0x00094932 `can_tx_group_idle_clear` — stub
6 instructions: clears the group-active byte 0xFFF9440E+4·group. Called from 0x94F7C and
0x95024 (tx completion paths). [O]

#### 0x0009494C `can_rx_group_irq_frag` — unknown
Entered from 0x000967C4 (the RX dispatcher): gates on a pool status bit and a non-zero ptr at
PTR_DAT_00094990+16·grp+8, then switches on the group state 0xFFF94502[grp] (cases 3 and 4) —
decompile is truncated by bad instruction data (jumptable at the end), so the case bodies are
unreadable; callee 0x94D6C suggests an RX/error worker updating the same state word. [I]
#### 0x00094994 `can_cyclic_group_init` — init
Init-time (from 0x94062) setup of one cyclic-TX group: copies the 8-byte template
{enabled,count,cur,end,desc-array ptr} from ROM pool 0x94bc8 into 0xFFF94418+8·grp, overriding
the array pointer with desc+0xc when the descriptor is enabled. [O]
#### 0x000949BC `can_cyclic_slot_register` — logic
Registers a message's static slot in a cyclic group (from 0x94062): if the group is enabled and
has no slot bound yet (0xFFF9441B), latches it, calls pool 0x94bcc (= 0x93FE0 slot setup) and
stores the message in 0xFFF94220[slot]; always writes the reverse map 0xFFF94436[msg]=slot.
[O]
#### 0x00094A04 `can_cyclic_group_pump` — logic
Cyclic-TX pump from the timer dispatcher 0x00093426: completes the current entry of the group's
0x10-stride descriptor array (verify id, set 0xFFF9449C bit0, run the completion fn at desc+0x10),
advances the cursor (wrap at 0xFFF94418 count), and unless a 0x400 DMA burst is pending submits
the next entry via pool 0x94bd4 → driver 0x9829C. [O]
#### 0x00094AE6 `can_cyclic_msg_send` — logic
Fill-in send for a cyclic message: resolves the group from the ROM descriptor (DAT_00094bd0,
0x14 stride), writes {id, word, dlc|flags, data} into the next free 0x10-stride array entry at
0xFFF94419, and submits immediately via pool 0x94bd4 when it was idle. Callers 0x0009355A and
the dispatcher 0x000934AC; also tests the DMA mask via 0x979F0. [O]
#### 0x00094BB8 `can_cyclic_cursor_sync` — stub
7 instructions: copies the group's current-entry cursor 0xFFF9441A into the fill cursor
0xFFF94419 (drop pending fills). Called from the completion paths 0x94F7C and 0x95024. [O]
#### 0x00094BDC `can_msg_mask_walk` — logic
Applies a group bitmask (param_1 over 3 groups → bytes 0xFFF9450A) and a walk mask to all five
descriptor lists at 0xFFF844C0: for every message whose group bit is set, compares a ROM table
(PTR_LAB_00040e02 indexed by desc+0xc) against param_2 and calls the enable pair (pool
0x94cd0→0x94E38 set mask bit + 0x94cd4→0x94E48 buffer-off) or the disable pair (0x94cd8→0x94E6C
clear bit + 0x94cdc→0x94E7C buffer-on), gated by desc+0xf. Caller 0x0006D094 (init/shutdown
path). Returns whether any list matched. [O]
#### 0x00094CE0 `can_slot_map_checksum` — logic
Sums the 0x70-word slot table 0xFFF94220 into 0xFFF94430 and the 0x66-byte reverse map
0xFFF94436 into 0xFFF94508 — baseline for the verification at 0x94D12. Called from boot init
0x0006E804 right after the slot init 0x94062. [I]
#### 0x00094D12 `can_slot_map_verify` — logic
Recomputes both slot-map sums and compares against the baselines 0xFFF94430/0xFFF94508; returns
true only if both match (init-time RAM consistency check). Caller 0x0006E83C. [O]
#### 0x00094D58 `can_hw_setup_wrap` — stub
8-instruction wrapper: calls pool 0x94d68 with (param_1,param_2) — resolved callee 0x00098ED0
(driver-layer setup). Called from the slot init 0x94062. [O]
#### 0x00094D6C `can_group_status_update` — logic
Reads the hardware group status (pool 0x94db8) and folds error bits into the group state
0xFFF94502[grp]: 0x19→bit0, 0x22→bit1, 0x100→bit4, 0x20→bit11, preserving the 0x600 burst
bits. Called from 0x000780D0 and the RX worker 0x9494C; callee 0x96768. [O]
#### 0x00094DBC `can_rx_msg_read` — logic
Reads one received message by id: resolves the slot from 0xFFF94436 (else group·0x20+1 from the
ROM descriptor), reads the 0x20-byte mailbox at group_base+0x100+slot·0x20 and unpacks
{11-bit id<<18 (raw if bit31 set), DLC (+4 low nibble), 8 big-endian data bytes} into the
caller's outputs. Caller 0x0006F000 — mirror of the tx writer 0x9829C. [O]
#### 0x00094DF4 `can_msg_desc_ptr` — data-accessor
6 instructions: returns DAT_00094e00 + msg·0x14 — a pointer into the message ROM descriptor
table. Caller 0x0006EC38. [O]
#### 0x00094E04 `can_msg_recv_try` — logic
If the message's slot is static (<0x60) and the mask bit2 is clear, delegates to pool 0x94e34
(slot, r2, r3); otherwise returns 4 (rejected). Caller 0x0006EC80. Bit2 is set/cleared by
0x94E38/0x94E6C (mask walk). [I]
#### 0x00094E38 `can_msg_mask_set` — stub
7 instructions: sets bit2 of the message flag byte 0xFFF9449c[msg] (mask message off). Called
from the mask walk 0x94BDC. [O]
#### 0x00094E48 `can_buf_disable` — logic
If the message holds a static slot, calls pool 0x94e68 → 0x0009820C(slot,0) — buffer
disable/invalid. Called from the mask walk 0x94BDC. [O]
#### 0x00094E6C `can_msg_mask_clear` — stub
7 instructions: clears bit2 of 0xFFF9449c[msg]. Called from the mask walk 0x94BDC. [O]
#### 0x00094E7C `can_buf_enable` — logic
If the message holds a static slot, calls pool 0x94e9c → 0x0009820C(slot,1) — buffer
enable/valid. Called from the mask walk 0x94BDC. [O]
#### 0x00094EA0 `can_msg_flags_update` — logic
For message types <4 (ROM type byte at DAT_00094ee4 table), reads the slot status via pool
0x94ee8 and ORs bits 1→bit0, 2→bit1 into the flag byte 0xFFF9449c[msg]; returns the flags.
Callers 0x00076198/0x00076178; callee 0x9A2CC. [O]
#### 0x00094EEC `can_msg_period_get` — data-accessor
8 instructions: dereferences the u32 pointer at message descriptor +4 (the period/budget word)
and returns it. Caller 0x00074C20. [O]
#### 0x00094F00 `can_group_err_check` — logic
Group health poll (caller 0x00078038): reads global + per-group hardware status (pools
0x94f70/0x94f74); with errors (status & 0x19) non-zero clears the state to just the 0x400 bit
and returns 1 (healthy reset), else escalates via pool 0x94f78 and sets the 0x200 error bit.
Callees 0x96768, 0x9A324, 0x9A354. [I]
#### 0x00094F7C `can_group_tx_abort` — logic
Force-abort a group's current transaction (caller 0x00078038): sets the 0x200 error bit in
0xFFF94502[g], runs pool 0x94fb8/0x94fbc and syncs the cursors (0x94BB8) and group-active flag
(0x94932); callee 0x9A354. Returns the pool verdict. [O]
#### 0x00094FC4 `can_burst_end_clear` — logic
End-of-DMA-burst cleanup (caller 0x0007807E): runs pool 0x9501c, and if the 0x400 burst bit is
set in 0xFFF94502[g], clears bit3 (0x8) of 0xFFF9449c[msg] for all 0x66 messages of that group
(desc group byte at DAT_00095020+2), then drops the 0x400 bit. Callee 0x9A374. [O]

#### 0x00095024 `can_burst_begin` — logic
Start-of-DMA-burst (caller 0x0007807E): runs pool 0x95090/94/98, and if the 0x400 burst bit is
clear sets bit3 (0x8) of 0xFFF9449c[msg] for all 0x66 messages of the group and arms the 0x400
bit in 0xFFF94502[g]. Mirror of the end-clear 0x94FC4; callees 0x94BB8, 0x94932, 0x9A388. [O]
#### 0x000950FC `ident_pair_capture` — logic
Called from 0x0009524A: reads two pool values, compares against ASCII 'P'(0x50)/'O'(0x4f), and
if matched stores the value (<0x200000) into 0xFFF80E1C/0xFFF80E18 with a link pointer from
0xFFF80E14/0xFFF80E10, shifting a one-bit validity history into 0xFFF80E22/0xFFF80E20.
Part of the chip-identification sequence at 0x95260. [I]
#### 0x0009519A `ident_value_plausibilize` — logic
Post-processing of the identified value (0xFFF80E14/1C chain): clamps to <0x1FFFFF and
0x1FFF7F, kills the dead zones 0x7F7F–0x10080 and <0x5E00 cases with 0x20/0x40 adjustments,
then runs a 64-iteration delay loop; sets 0xFFFFAC08 bits by DAT_fff94713 and returns it.
Meaning of the clamps is open. [I]
#### 0x00095260 `bus_chip_detect` — logic
Probes the controller at 0x80100001/2: −1→code 0xA1, ASCII 'E'→1, else 0 (and clears
0xFFF94711), then matches the ID word 0x80100006 against a pool table and records type nibbles
into 0xFFF94711/12/13 (bits 2-3 → 1<<n) and 0xFFF94714 (0xFFFF1000 bit2), finally writing
0xFFFFA840 |= 0x2D00 and 0xFFFFAB00 = 0xB300. These cells gate the CAN group driver
(0x9829C checks fff9471b==3). callers='-'. [I]
#### 0x00095408 `wdt_kick` — logic
Watchdog service from the 5 ms engine (caller 0x000920F8 ← 0x0008EEDC): writes the WDT
interrupt priority nibble 0xE into 0xFFFE0C04 (IPR05), feeds WTCNT 0xFFFE0002 = 0x5AB7,
WRCR 0xFFFE0006 = 0x5A80, and sets WTCR 0xFFFE0000 = 0xA567 (CKS=7, counter enabled) —
exactly the values spec 05 §/spec 03 §6.2 record. [O] (spec 05 watchdog, spec 03 line 156)
#### 0x0009546C `can_err_isr_a` — irq
Vector-table ISR (callers='-'), one of three byte-identical copies: takes the pool 0x956f8
spinlock on 0xFFF90F64, then a flag-wait loop the decompiler cannot recover (unreachable
blocks at 0x954ae-f4); tail calls 0x933E0. Cluster of 3: 0x9546C and 0x9567E (both pool
0x956f8) and 0x958B4 (pool 0x959a0) — the error/status interrupt entries for the CAN/RSPI
units. [I]
#### 0x000954E6 `can_rx_isr_a` — irq
CAN channel-A receive ISR (callers='-'): reads the RX flag word 0xFFFFD040 masked by the
enabled-mailbox mask 0xFFF94588, for each set bit reads the 0x20-byte message buffer at
0xFFFFD100+32·idx ({id<<18 (raw if bit31), 8 data bytes, DLC nibble at +0x10}), acks the flag,
and hands (idx, id, dlc, data) to pool 0x95700. Buffer layout matches the tx writer 0x9829C.
[O] (spec 07 §1 receive path)
#### 0x000955E2 `can_txend_isr_a` — irq
CAN channel-A tx-complete ISR (callers='-'): reads 0xFFFFD030 & 0xFFF94588, writes
0xFFFFFFFF to 0xFFFFD038, and for each completed mailbox acks the flag and calls pool
0x95708 → the tx pump dispatcher 0x00093426. [O]
#### 0x00095714 `can_rx_isr_b` — irq
Channel-B receive ISR, same body as 0x954E6 against base 0xFFFFD800 (flags at
0xFFFFD840 & mask 0xFFF9458C, buffers at 0xFFFFD900), using the pool 0x959a0 lock; hands
frames to pool 0x959b8. Channel B = extra receive capacity on bus 1 (spec 01 §2.5). [O]
#### 0x00095814 `can_txend_isr_b` — irq
Channel-B tx-complete ISR: reads 0xFFFFD830 & 0xFFF9458C, clears 0xFFFFD838 = −1, acks and
calls pool 0x959b0 with idx+0x20 (channel-B slot offset). [O]
#### 0x0009592E `can_rx_isr_c` — irq
Channel-C receive ISR (bus 2, the shifter/diagnostic bus — spec 01 §2.5): same body against
base 0xFFFFE000 (flags 0xFFFFE040 & mask 0xFFF94590, buffers 0xFFFFE100), pool 0x959a0 lock.
[O]
#### 0x00095A4C `can_txend_isr_c` — irq
Channel-C tx-complete ISR: reads 0xFFFFE030 & 0xFFF94590, clears 0xFFFFE038 = −1, acks and
calls pool 0x95b34 with idx+0x40 (channel-C slot offset). [O]
#### 0x00095BF4 `rspi_unit_config` — logic
Configures one RSPI unit (callers='-'; reached from an init table): picks the mode byte from
ROM table 0x40E66 by param_2, merges it into the unit register via per-unit pointer table
(pool 0x95c78), computes a mode code (8/9/+1 by bits 0x20/0x40), zeroes the unit's 0x3C-byte
state block at 0xFFF9451B+0x3C·unit, and latches the timer snapshot 0xFFFFF220 into
0xFFF94540. [I]
#### 0x00095CB6 `rspi_status_check` — logic
Polls a unit's status byte (unit_ptr+0x10): on error bits 0x38 latches the time, runs the
error callback at 0xFFF94528+0x3C·unit; on done-bit 0x10 computes the idle deadline
now − mode·1000000/bitrate_cell (0xFFF9453C) into 0xFFF94534 and sets bit0 of 0xFFF9451A;
acks by writing 199 (0xC7). [O]
#### 0x00095D26 `rspi_clock_gate_arm` — logic
Clock-gate arm per unit (spec 03 §6.2ao names this block 0xFFFE6030): writes 0xBF to
unit+0x10, checks the unit's gate bit (unit 0 = 0xFFFE6030 bit 6, others 2-bit fields of
0xFFFE6031), and when set clears unit+8 bit, acknowledges the gate (0xFFFE6030 = 0xBF /
0xFFFE6031 = ~mask), latches times into 0xFFF94530/40, clears the armed bit and runs the
completion callback 0xFFF94520+0x3C·unit. [O]
#### 0x00095DBC `rspi_clock_gate_disarm` — logic
Twin of 0x95D26 in the disable direction: same gate-bit checks, clears unit+8 bit (mask
0x7B), acknowledges the clock gate, clears bit1 of 0xFFF9451B+0x3C·unit and runs the
callback at 0xFFF9451C+0x3C·unit. [O]
#### 0x00095EDC `rspi_bitrate_set` — logic
Bit-rate programming for one unit: converts the requested rate to a divider by reciprocal
rounding against the constant 0x1312D0, and when apply=1 stores the rate into
0xFFF9453C+0x3C·unit, writes divider−1 to unit+4 and pulses unit+0 bits 0-1 to latch.
Returns the actual rate. [O]
#### 0x00095F68 `dmac_start_a` — stub
8 instructions: calls 0x95FA8 (request-enable 0xFFFE105C |= 4) then starts the DMA channel
with 0xFFFFE800 |= 2. Called from 0x0008F9EC. [O]
#### 0x00095F7A `dmac_start_b` — stub
Twin of 0x95F68: calls 0x95FB6 (0xFFFE104C |= 4) then 0xFFFFEC00 |= 2. Called from
0x0008F9EC. [O]
#### 0x00095F8C `spi_stage_buf_pick` — data-accessor
11 instructions: returns the even/odd SPI staging cell (0xFFF884BA vs 0xFFF884CA — the
staging area spec 03 §6.2m records) chosen by whether fewer than 8 of 16 remain in the
hardware count 0xFFFE1048 (DMA burst progress). Caller 0x0008F058. [O]
#### 0x00095FA8 `dmac_req_en_a` — stub
6 instructions: sets bit2 (request enable) of 0xFFFE105C. Called from 0x95F68. [O]
#### 0x00095FB6 `dmac_req_en_b` — stub
6 instructions: sets bit2 of 0xFFFE104C. Called from 0x95F7A. [O]

#### 0x0009617A `unknown_ffff0810_mode_set` — unknown
15 instructions: clears 0xFFFF0810 and 0xFFF94554; when the arg is non-zero sets 0xFFFF0810 = 3
and clears 0xFFF94550 / 0xFFFF0806. The register's meaning (same block the 0x96AF8 pair writes)
is unresolved. Caller 0x00068DD8. [I]
#### 0x000961F0 `unknown_hw_c8_block_clear` — logic
Zero-fill of the 16-bit register block 0xFFFE3802–391C and 0xFFFFC800–C8AE, leaving only
chosen non-zero values (C808=0x10, C828=0x8000, C830/C83C=0xFFFF, C860/C86C=0x125, C868=0x24A,
C880/C88C=0x61B, C888=0x824, C8A8=0x1FC) and setting C921 bit0. Looks like a timer/trigger
sequencer reset; block not identified in the specs. Caller 0x0008EEDC (5 ms task). [I]
#### 0x000962E2 `unknown_hw_c8_block_arm` — logic
ORs enable patterns into the same 0xFFFFC8xx block: 0xFDD5/0xFD, 0x5000/0x555, 0xFFF,
0xAAAA/0xAA (alternating channel masks), 0x14AF|5, 0x5550|1 — the arm counterpart of the
0x961F0 clear. Caller 0x0008EF1A. [I]
#### 0x0009637C `spi_bus_busy_clear` — stub
9 instructions: clears the per-bus busy byte 0xFFF94558+2·bus (bus<2). Called from the job
dispatcher 0x93902. [O]
#### 0x00096390 `spi_bus_shadow_clear` — stub
9 instructions: clears the per-bus shadow byte 0xFFF94559+2·bus. Called from the engine kick
0x000921A0. [O]
#### 0x000963A4 `rspi_all_units_idle` — stub
9 instructions: writes SPCR=8 (master-armed idle, spec 03 §6.2ak) to the three RSPI units at
0xFFFFB000/0xFFFFB800/0xFFFFC000 and masks the RSPI interrupt priorities (0xFFFE0C30 &=
0xF000). Called from the queue init 0x937CC. [O] (spec 03)
#### 0x000963CC `rspi_bus_start` — logic
Starts one of two buses (this is pool PTR_FUN_00093a58 called by 0x937CC): SPCR=8, arms the
clock-gate nibble at 0xFFFE6017 (bus1: bit3|4; bus0: bit1), zeroes the DMA handshake pair
0xFFFE6070/72 or 74/76, sets the IPR nibble 0x40/0x400 in 0xFFFE0C30, copies cfg byte to
unit+1, clears the busy/desc cells 0xFFF94558/5C. Returns 1 for bus≥2. [O]
#### 0x00096468 `spi_job_dmac_launch` — logic
The job launcher (spec 03 §6.2aw): for a free bus latches the job at 0xFFF9455C, programs the
RSPI unit from the 0x18-off descriptor (frame count−1 at unit+8, frame words copied from
desc+0xc into the unit+0x10 SPDR area, SPCMD/SSLC at unit+2..+0xe), then arms the DMA channel
(count at 0xFFFE6070/74, TX/RX addresses 0xFFFE60A0-A6 from desc+8/+4 masked 0x7FFF), pulses
the clock gate 0xFFFE6030 = 0xFC/0xF3 and finally sets SPCR = 0xF8 — the DMA-driven mode the
spec records. Returns 1 if busy, 2 for bad bus. [O] (spec 03 §6.2aw)
#### 0x00096684 `unknown_96684_dispatch` — unknown
Register-passing trampoline (callers='-'): calls pool 0x966b4 with the incoming stack arg and
returns {r0,r1} per verdict 1/2/other; tail callee 0x00096D6A. Purpose unresolved — likely a
vector-table entry that re-dispatches to a handler pair. [I]
#### 0x000966F8 `can_ctrls_reset_init` — init
CAN init from the slot-map init 0x94062: sets bit0 (reset) on four controller bases 0xFFFF0000,
0xFFFFD000, 0xFFFFD800, 0xFFFFE000, clears IPR 0xFFFE0C32, and for the 3 message groups
initializes 0xFFF9471B (state)=0, 0xFFF9471E=0, 0xFFF94718=3, 0xFFF94715=1 and the
enabled-mailbox masks 0xFFF94588/8C/90=0. [O]
#### 0x00096768 `can_group_status_read` — logic
Reads a controller's error field (unit+2 & 0x3F) after a pool read/ack (0x967ec), and composes
the group status: flags 0xFFF94715 bit3→0x100 / bit0→0x200, state≠3→0x400, 0xFFF9471B bit7→
0x8000. Consumers: 0x94D6C, 0x94F00, 0x967C4; callee 0x97B92. [O]
#### 0x000967C4 `can_err_dispatch` — irq
18-instruction error path (callers='-'): reads the status word at obj+8 and, if error bits
0x61 are set, calls 0x96768 (composite status) and passes it to pool 0x967f0 → the RX worker
0x9494C. Likely the vector-table error/other interrupt entry. [I]
#### 0x000967F4 `dmac_init` — init
Two-channel DMAC init (caller 0x0008F9EC): resets channels 0xFFFFE800/0xFFFFEC00 (mode 0x10),
waits for !0x40 busy, writes channel configs 0x4F01/0x4F80, points descriptor registers
0xFFFFE8E0/ECE0 at the ROM table DAT_000968f0, sets masks 0xFFFE/0xFFF/0x7F, clears the
channel registers, registers the two DMA-end callbacks via pool 0x968f8, enables request bits
0xFFFE105C/104C |= 1 and sets IPR02 (0xFFFE0C02 |= 0xE400 — priorities E and 4). [O]
#### 0x00096900 `spi_engine_hw_init` — init
Companion of 0x967F4 (same caller): loads the staging template (0x9696A), enables
0xFFFFE80C bit0, programs the RSPI/DMA handshake cells 0xFFFE6090/92=0x44C, 0xFFFE6060/62=0x50,
0xFFFE6094/96=0x5E8, 0xFFFE6064/66=0xA0, toggles 0xFFFFFEA0 bit0 and enables
0xFFFFE804 bit0. [O]
#### 0x0009696A `spi_stage_template_load` — stub
9 instructions: copies the 0x50-byte ROM template (pool 0x969b8) into the SPI staging area at
0xFFF8844C. Called from 0x96900. [O]
#### 0x0009697E `spi_stage_template_verify` — logic
15 instructions: byte-compares the 0x50-byte staging area 0xFFF8844C against the ROM template;
returns 1 on exact match. Caller 0x0008F70C (post-init RAM check). [O]
#### 0x0009699E `spi_duty_buf_pick` — data-accessor
9 instructions: returns the duty staging buffer 0xFFF885E8, or 0xFFF88688 when the DMA count
0xFFFE6064 exceeds 0x50 — the even/odd live duty array pick (spec 03 §6.2m notes the 0xFFF885E8
duty array). Callers 0x00083280, 0x0008F058. [O] (spec 03)
#### 0x000969BC `ram_win_template_load` — stub
17 instructions: copies 8 words (32 bytes) from a ROM template (pool 0x969e4) to 0xFFF7FFFC —
a descriptor block at the top of the 0xFFF80000 RAM window. Caller 0x00090B98. [I]
#### 0x000969E8 `dmac_channel_program` — logic
Programs one DMAC channel from a descriptor: disables the channel (0xFFFE100C+4·ch), toggles
the reset bytes 0xFE/0xEF at 0xFFFE108C+0x10·ch, writes 7 words {src, dst, ctrl, ...} into the
channel table (pool 0x96ae4), merges count bits at +0x1c (mask DAT_00096ae8, preserving 0x40005),
adds 0x5B00-based priority for channels >3, and packs a half-word at 0xFFFE1300. Callers
0x967F4 (init), 0x97DB6, 0x90B98. [O]
#### 0x00096AEC `unknown_fffc1c00_trigger` — stub
5 instructions: writes 1 to 0xFFFC1C00. Part of the 0x90AEC init chain around the 0xFFFC04xx
window controller; register meaning unresolved. [I]
#### 0x00096AF8 `unknown_ffff0812_mode0` — data-accessor
12 instructions: sets 0xFFFF0812 = 0x7800, or 0x7821 when the mode register 0xFFFF1000 bits
0-1 are clear; returns the value. Cluster of 2 — member 0x96B16 writes 0x7810 instead (bit 4
differs). Caller 0x00090AEC. [O]
#### 0x00096B34 `hw_variant_probe` — logic
Probes the hardware variant via pool 0x96d94: when the verdict is not 1, zeroes the 4-channel
register set 0xFFFC0400-0434/04A0-04C0 and sets 0xFFFC04C0 = 0x30000; latches the verdict as
boolean into 0xFFF94721. Callers 0x00072964 and 0x00090AEC. [O]
#### 0x00096B80 `hw_variant_win_program` — logic
Programs the 4 window channels by detected variant (0xFFF94721): variant 2 uses fixed windows
(ch0: 0xFFF80000/0x37FFF ctrl 0x6AC, ch1: 0xFFF81000/0x3EFFF, ch2: 0xFFF82000/0x3DFFE, ch3:
0xFFF9E000/0x3F ctrl 0x768); variant 3 reads them from the 0x14-per-channel table at
0xFFF94594 with ctrl |= 0x68. Clears the per-channel enable bits (0x8800 masks) in 0xFFFC04C0.
Callers 0x96D1C, 0x90AEC. [O]

#### 0x00096CA8 `hw_watch_channel_stop` — logic
Stops window/watch channel n (variant 2/3 only): zeroes the address/count pair
0xFFFC0400+16·n, writes ctrl 0x2000 to the channel's +0xA0/A4/B0/B4 slot, and clears the
channel's enable bit (0x8800 mask) in 0xFFFC04C0. Called from 0x96D2E. [O]
#### 0x00096D1C `hw_watch_ch3_program` — stub
7 instructions: when the variant cell 0xFFF94721 == 2, calls 0x96B80(3) — programs window
channel 3 only. Caller 0x00090B98. [O]
#### 0x00096D2E `hw_variant_set` — logic
Selects the hardware variant: for arg<4 stops all four 0xFFFC04xx channels (0x96CA8 loop)
when the current variant is not 1, then latches the variant into 0xFFF94721 (arg≥4 → 0).
Caller 0x00090AEC. [O]
#### 0x00096D6A `hw_watch_poll` — logic
The 0xFFFC04xx block decoded as a watch/compare unit: for variant 3 walks 4 channels; for
each enabled one reads the watched address (u8/u16/u32 by type byte 0xFFF945B5), range-checks
value&mask (0xFFF945A4/A8/AC) and time (0xFFF94598/9C) with a debounce counter
(0xFFF945B4: −1/−2 special codes), and on trigger latches {time→0xFFF945A0,
value→0xFFF945B0}; acks 0xFFFC04C0 &= 0xFFFF00FF. Reached from the dispatch 0x96684
(callers='-' there) — the pair is the watch interrupt. Returns the variant. [O]
#### 0x00096E80 `hw_fffec000_ctrl_clear` — init
Clears the mask cell 0xFFF94722 and the 0xFFFEC000/0010/0020 register set, then clears the
high nibble of IPR05 (0xFFFE0C04 &= 0xFF | 0x1200). Caller 0x00091A80. [I]
#### 0x00096ED2 `hw_fffec000_mask_set` — logic
Under the 0x9665C/0x96674 lock: ORs the bit mask into 0xFFF94722 and mirrors it into
0xFFFEC000. Caller 0x00091A30. [O]
#### 0x00096EF8 `hw_fffec000_mask_clear` — logic
Lock-protected clear of mask bits from 0xFFF94722/0xFFFEC000. Caller 0x00091A30. [O]
#### 0x00096F28 `hw_fffff000_init` — init
Writes the 0xFFFFF1xx config block (F100=0x13, F102=4, F104=6, F106=0), sets 0xFFFFF000 |= 1,
0xFFFFF003 = 0x9F and latches the shadow 0xFFF9470E. Callers 0x00091F68/0x00091FA4 (the
supervisor register-job layer). [I]
#### 0x00096F56 `hw_fffff000_bits_set` — data-accessor
8 instructions: ORs bits into the shadow 0xFFF9470E and writes it to 0xFFFFF000. Callers
0x00091F68/0x00091F3E/0x00091FA4. [O]
#### 0x00096F68 `hw_fffff000_bits_clear` — data-accessor
8 instructions: clears bits from the shadow 0xFFF9470E / 0xFFFFF000. Callers
0x00091F68/0x00091FA4. [O]
#### 0x00096F98 `hw_fffff202_busy_get` — stub
6 instructions: returns 0xFFFFF202 != 0 — a busy/active flag poll (callers='-', reached via
pointer). [I]
#### 0x00096FA4 `hw_fffff3xx_setup` — init
Writes 0xFFFFF304=3, F305/F306/F307=0 and clears the high nibble of IPR08 (0xFFFE0C08).
Caller 0x00091FA4. [I]
#### 0x00096FC8 `timer_f440_units_init` — init
Initializes 5 units of 0x20 stride at 0xFFFFF440+ from ROM (pool 0x970f8), sets the 0x32
('2') status bytes at 0xFFFFF414-417 and 0xF at 0xFFFFF402, arms IPR08 |= 0x77, clears
IPR0A/0C and sets 0xFFFFF400 = 0x1F. Caller 0x00091FA4. [I]
#### 0x0009703C `timer_f4xx_ready_check` — logic
Readiness poll (callers='-'): false-ready when 0xFFFFF440 low bits set, 0xFFFFF402 low nibble
≠0xF, the four 0x32 status bytes ≠'2' — extended to the F41C-41F/F424-427 columns when the
detect cell 0xFFF94713 ≠ 1. Complements 0x96FC8's init values. [O]
#### 0x000970FC `timer_f580_units_init` — init
Initializes 6 units of 0x40 stride at 0xFFFFF580 (reset/count regs to 0/0xFF, compare words
from ROM pool 0x97194), then arms the interrupt priorities IPR0E=0x6665, IPR10=0x5666,
IPR12=0x6666, IPR14 |= 0x6600 and sets 0xFFFFF500 = 0xF. Caller 0x00091FA4. [I]
#### 0x00097198 `timer_f800_units_init` — init
Initializes 6 units of 0x40 stride at 0xFFFFF800 (enable/polarity/mode bytes and per-unit
ROM bytes), sets the per-group 0xF enables at 0xFFFFF80C..0xFFFFF94C and 0xFFFFF700 = 0x3F,
IPR16 |= 0x88, IPR18 = 0x8488. Caller 0x00091FA4. [I] (spec 02 ATU/PWM groups)
#### 0x00097214 `pwm_channel_setup` — logic
Sets up one output channel of a 0x40-stride timer unit (0xFFFFF800+0x40·unit): channel bit
mask from the 1-8 bit index, polarity/enable bytes at unit+0..+3, and the compare-value pair
written twice (unit+0x10+2·ch and unit+0x20+2·ch) plus a zeroed second pair. Callers
0x000919A0, 0x0009136E, 0x0008FF66 — the supervisor duty/level job machinery (spec 03 §6.2m).
[O]
#### 0x00097292 `timer_f8xx_ready_check` — logic
Readiness poll (callers='-'): not-ready when any of the six 0xFFFFF8xx unit bytes (&7)≠1, any
group enable (&0xF)≠0xF, or 0xFFFFF700 (&0x3F)≠0x3F — the mirror of 0x97198's init state.
[O]
#### 0x0009733C `timf_capture_init` — init
Initializes 20 capture units of 0x20 stride at 0xFFFFFA80 (zero state, mode byte from ROM
pool 0x9743c, config byte from pool 0x97440 at 0xFFFFFA11+2·i), sets 0xFFFFFA51/53/55 = 0x32
and the 0xFFFFF masks 0xFFFFFA00/FA04 = 0xFFFFF, and arms IPR1A/1C/1E/20 to priority 7.
Caller 0x00091FA4. [I] (spec 02 §4 timer F)
#### 0x000973BA `timf_capture_verify` — logic
Verifies the 20 capture units against the ROM templates (mode high bits 0xE0 and config
bytes) plus the 0x32/0xFFFFF status words — readiness check of 0x9733C's init (callers='-').
[O]
#### 0x00097444 `timf_capture_setup` — logic
Configures one timer-F capture channel (spec 03 §6.2ap cites it in the 0x90DA2 chain): clears
the channel's enable bit in 0xFFFFFA00, sets mode bits 0x14|type (desc+0xA &3 → 1/2/3), zeros
the capture/counters, writes the source select from desc+8 and re-arms the enable. Under the
0x96674/0x96650 lock. Caller 0x00090DA2 (speed-channel setup). [O] (spec 02 §4)
#### 0x000974C2 `timf_capture_read` — logic
The consistent capture read (spec 02 §4 step 1-2): re-reads the six channel counters
(0xFFFFFA80+0x20·ch − offsets) until stable, shifts out the 8 fraction bits, classifies the
update (0x10 first edge, 0x21/0x31 implausible bands via the param_2 thresholds, 0x40
timeout, bit0/1 direction), computes the saturated Δticks>>8 into out+12 and latches
{time, count}. Caller 0x00090E14 (speed processing). [O] (spec 02 §4)
#### 0x000975EC `timf_capture_stop` — logic
Disables one capture channel: zeroes the pending byte, clears the enable bit in 0xFFFFFA00
and rewrites the mode byte (|2 when desc+0xA == 0). Callers 0x00090010, 0x00091884. [O]

#### 0x00097648 `timf_capture_poll` — logic
Polled capture read of one timer-F channel (0xFFFFFA80+0x20·ch): waits for the pending bit,
classifies the sample (0x10 first edge, 0x21/0x31 by the min/max thresholds in param_2, 0x40
timeout with the fff94627-style deadbands via 0xFFFFC852/0xFFFFC822 masks), latches
{time, Δticks>>8} into the out struct. Callers 0x0009076E/0x000904FA/0x000918F6 (speed
processing family). [O] (spec 02 §4)
#### 0x00097744 `timer_fe80_units_init` — init
Initializes 6 units of 0x10 stride at 0xFFFFFE80 (mode byte from ROM pool 0x977c8, zeroed
counters, 0xFFFF reload), arms IPR22 |= 0x7700 and sets 0xFFFFFE01 = 0x3F. Caller 0x00091FA4.
[I]
#### 0x0009778A `timer_fe80_ready_check` — logic
Verifies the six 0xFFFFFE80 units against the ROM mode bytes (high bits 0x70) and the 0x3F
enable mask at 0xFFFFFE01 — readiness twin of 0x97744 (callers='-'). [O]
#### 0x000977F0 `timer_ff80_units_init` — init
Initializes the 0xFFFFFF80–AA unit pair (ctrl 0x10, zeroed counters, 0xFFFF reloads, 0x32
status bytes), arms IPR24 |= 0x3300 and writes 0xFFFFFF80 = 3. Caller 0x00091FA4. [I]
#### 0x00097850 `wdt_mode_start` — logic
Watchdog control sequencing from the engine (caller 0x000920A4): when WTCR bits 0x40/0x20 are
clear arms 0xFFFE0000 |= 0xA540 (or 0xA560 under the lock pair 0x96674/0x96650), and always
writes WRCR 0xFFFE0006 = 0x5A80. Write-enable key pattern matches the wdt_kick 0x95408.
[O] (spec 05)
#### 0x000978A4 `wdt_mode_stop` — logic
Companion of 0x97850 in the stop direction: when WTCR bit 0x40 is set, clears 0x40/0x20 with
the 0xA500/0xA520 key writes under the lock, and re-arms the WDT interrupt priority nibble
0xE in 0xFFFE0C04. Caller 0x000920A4. [O] (spec 05)
#### 0x0009790C `can_tx_buf_write` — logic
Rich CAN transmit-buffer write: like 0x9829C but with DLC 0xFF = keep-existing, extended-ID
handling via the mask table 0xFFF9457C (mask const DAT_00097a28), and — when the buffer is
also RX-armed (0xFFF94588 bit) — skipping the tx-complete trigger write at +0x30; arms the
buffer valid bit at +0x20. Callers 0x0009355A and the dispatcher 0x000934AC. [O] (spec 08 §1)
#### 0x000979F0 `can_dma_bit_test` — data-accessor
23 instructions: returns 1 when the group state byte 0xFFF9471B[grp]==3 AND the message
slot's bit is set in the DMA-enable table 0xFFF94564+4·grp — the "is this buffer
DMA-launched" test gating the tx pumps. Callers 0x94824, 0x94AE6. [O]
#### 0x00097A2C `can_mode_sync` — logic
Under the lock pair: reads a controller's control word bit 0x10 and resolves the pending
0x80 transition in the group state 0xFFF9471B (updating the error flag 0xFFF94715 and
folding the state to its low 2 bits). Callers 0x97B92/0x97B42. [O]
#### 0x00097A86 `can_mode_transition` — logic
Executes a CAN group mode change: from state 0 to target 2 (sleep) or 3 (run) it sets
0xFFF9471B = 0x82/0x83, latches mode cell 0xFFF94718, clears flags and writes the
controller sleep bit (+2 bit8); from a running state it restarts the controller (copy
+0x10→+0x14, clear +0x30, ctrl |= 2, state 0x80). Callers 0x97B92/0x97B42. [O]
#### 0x00097B42 `can_mode_set` — logic
Requests a target mode for a group: syncs (0x97A2C), clears the 0x10 interim bit, latches
the target into 0xFFF9471E and runs the transition (0x97A86). Callers 0x984A4, 0x9A324,
0x9A354. [O]
#### 0x00097B7C `can_all_groups_sync` — init
11 instructions: runs 0x97B92 for groups 0..2 — the boot-time mode sync of all three CAN
groups. Caller 0x0006E882. [O]
#### 0x00097B92 `can_group_sync` — logic
Per-group sync: 0x97A2C then, if the group is running and the controller start bit (+2 bit1)
is set, clears the target cell 0xFFF9471E; finally 0x97A86. Callers 0x96768 (status read)
and 0x97B7C. [O]
#### 0x00097CC0 `eeprom_start` — logic
Starts the data-flash/EEPROM service (caller 0x0007061C): resets the state cells of the unit
struct (pool 0x97d90), sets state=1, optional pool init, calls the rearm 0x97D58 and zeroes
the countdown 0xFFF94723. Module identity inferred from the 0x97DB6 verifier family. [I]
#### 0x00097CF4 `eeprom_stop` — stub
9 instructions: clears the state cells and writes state=0 into the unit struct. Caller
0x0007061C. [O]
#### 0x00097D06 `eeprom_service` — logic
Countdown service (callers 0x6A86C/0x6AB8E/0x70998/0x71550): decrements 0xFFF94723; at zero
writes the key patterns 0xFFFFA842=0xD200, 0xFFFFA840=0x2D00, 0xFFFFA852=0xE100,
0xFFFFA850=0x1E00, then sets 0xFFFFA811 |= 7 or 5 by a pool verdict. [I]
#### 0x00097D58 `eeprom_rearm` — stub
13 instructions: writes 0xD2FF/0x2DFF to 0xFFFFA842/A840 and increments the countdown
0xFFF94723 (saturating at 0xFE). Callers 0x97CC0 and the 0x6A/0x70/0x71 service sites. [O]
#### 0x00097D9C `eeprom_blk_size_load` — stub
10 instructions: picks the ROM table (pool 0x97f80/0x97f84) by the variant cell 0xFFF94713
bit0 and loads its +0x10 word into 0xFFF94638. Caller 0x0007061C. [O]
#### 0x00097DB6 `eeprom_verify_engine` — logic
State machine over 0xFFF94624 (3 = verify block, 0x15/0x11/0x19/0x1e phases, 6 = done/error):
selects the ROM block table by the variant, parses a 2-digit decimal from the table, drives a
DMA read (enable 0xFFFE103C, count deadline 0xFFF94628 = len+7 vs 0xFFFE1038) set up by
0x980A0/0x97F8C, and writes the key patterns 0xFFFF1002=0xC300/0xC301 and
0xFFFFA904=0x5501/0xFFFFA854=0xC401/0xC400. Callers 0x6A86C, 0x6AB8E, 0x720C8, 0x7237A,
0x7061C, 0x71594, 0x706B6. [I] (spec 06 block validation)
#### 0x00097F62 `eeprom_verify_restart` — stub
7 instructions: resets the verifier state (0xFFF94624=3, step 0xFFF94627=0, flags
0xFFF94625=0). Caller 0x97CC0. [O]
#### 0x00097F72 `eeprom_verify_restart2` — stub
6 instructions: 0xFFF94624=3, 0xFFF94627=0 — the short restart. Callers 0x988E0, 0x6A86C,
0x6AB8E, 0x9A030, 0x98B7C. [O]
#### 0x00097F8C `eeprom_blk_sum_check` — logic
Chunked block checksum: on first pass sizes the block from the table word (&0x1FFF)+1 and
reads a decimal flag byte; then sums u16 words from the block source (max 0x600 per pass,
cursor 0xFFF94630) into 0xFFF9462C, and at completion compares against the table's expected
sum — match → state 4, mismatch → state 0 with error cells 0xFFF94636/0xFFF9462A=0x10/
0xFFF94624=6. Writes the 0xFFFFA854=0xC400 key. Caller 0x97DB6. [O]

#### 0x000980A0 `eeprom_keys_strobe` — stub
19 instructions: a 10-iteration delay, then writes 0xFFFFA810 = 0 and the key patterns
0xFFFFA902 = 0xAA00 / 0xFFFFA906 = 0xCC00. Part of the 0x97DB6 EEPROM/verify engine. [O]
#### 0x000980E0 `can_slot_dmac_config` — logic
The A-DMAC channel configurator (spec 03 §6.2aw pins this address): for message slot
(group·0x20+idx) initializes the mailbox DLC word 0x2700, arms the DMA-enable bit in the
mask table 0xFFF94564, sets the completion masks (+0x30/38/40/58), programs the ID (std<<18
or extended via mask DAT_00098208 and the table 0xFFF9457C), selects RX-buffer mode by
param_5 (0xFFF94570 bit + DLC word 0x100/0x300), and always sets 0x2000 in the DLC word.
Caller 0x94000. [O] (spec 03 §6.2ap/aw)
#### 0x0009820C `can_buf_rx_arm` — logic
Buffer enable/disable: with off, sets the tx-only bit (+0x50/52) and clears the RX-armed bit
in 0xFFF94588; with on, pulses the RX-complete flags (+0x32/42) when newly armed and sets
the RX-armed bit. Callers 0x94E48/0x94E7C (mask walk) and 0x94000. [O]
#### 0x0009838C `can_group_hw_init` — logic
Full CAN group hardware init (from 0x94062): controller reset (ctrl|=1), zeroes the state
cells 0xFFF9471B/1E, sets the group's IPR nibble 5 in 0xFFFE0C32, loads the bitrate pair
from the descriptor into +4/+6, resets all 32 mailbox DLC words to 0x2700, zeroes the DMA
mask tables 0xFFF94564/70/7C/88, sets mode 3/2 (param_3) with ctrl word 0xC041/0xC141,
error counters 0xFFFF and flags 0xFFF94715=1. [O]
#### 0x000984A4 `can_group_stop` — logic
21 instructions: clears the group flag 0xFFF94715, drops the controller's start bit
(ctrl &= ~1) and hands the group to the mode-set 0x97B42 with its current mode. Caller
0x94062 (init teardown path). [O]
#### 0x000984DC `unknown_hw_c8_level_get` — unknown
Readiness/level decode of the undocumented 0xFFFFC8xx block (callers='-'): by mode bits in
0xFFFFC872 (0x1000/0xC/0xC0) and 0xFFFFCA86 selects level bits in 0xFFFFC860/0xFFFFC820/
0xFFFFC880; returns 1 for the undetermined path. Same block 0x961F0/0x962E2 program. [I]
#### 0x00098590 `diag_selftest_run` — logic
Built-in self-test battery (callers='-'; reached from a pointer/boot check): initializes
pass/fail counters 0xFFF9464C/0FFF94650 and the config cell 0xFFF94648, then runs a series of
check functions (pools 0x987b8/…) with constants, comparing each result and tallying.
Direct callees are the 0x99xxxx check family: 0x999CC, 0x991EC/EE, 0x9987C, 0x994B0,
0x99624, 0x990A4, 0x99914. [O]
#### 0x000988E0 `eeprom_rw_engine` — logic
State machine over the EEPROM unit struct {state, substate, phase, len, count, buffer@+0xc}:
states 0-4 with the 0x1E special case, commands 0x20 (read), 0xE8/0xD0 (write) and 0x30
written to the data window, write-verify comparing the buffer against the RAM source word by
word, and pool calls for address mapping. Callers 0x000720C8/0x0007237A/0x00071594. [I]
#### 0x00098A9E `eeprom_read_start` — logic
Starts a read of param_2 bytes at offset param_1: requires the unit idle (phase 0), sets
phase=1, latches {offset, len} and maps the data pointer via pool 0x98b1c. Caller
0x000720C8. [O]
#### 0x00098AE2 `eeprom_write_start` — logic
Starts a write: phase=2, wraps the offset through the 0x2000-byte window (+0xE000 loop),
latches {offset, len (0x80 or 8)}, maps the buffer via pool 0x98b78 and copies (len>>2)
words from the source. Callers 0x000720C8/0x0007237A/0x00071594. [O]
#### 0x00098B7C `eeprom_erase_row` — logic
Erases/programs one row: computes the row address in the 0x3FF80000-style window, sets
0xFFFFA91A to the masked offset, writes the command bytes 0x71/0xD0 to the data window and
waits (≤600 iterations) for the ready bit 0xFFFFA900.0x80; failure → error cells
0xFFF9462A=1, 0xFFF94624=6 and the 0xFFFFA802 bit4 clear. The (0xFFF94711 & 0x20) variant
just verifies 4 words read 0xFFFFFFFF (erased). Callers: 0x70998, 0x70210, 0x706C4,
0x70452, 0x720C8, 0x6F66C, 0x7237A, 0x6FDB8, 0x71594. [O]
#### 0x00098C90 `ident_cells_guard_check` — logic
Maintains/validates inverted copies of the chip-detect cells: writes {~0xFFF94711→0xFFF94724,
~0xFFF94712→0xFFF94726, ~0xFFF94714→0xFFF94725, ~0xFFF94713→0xFFF84A47} when the marker
0xFFF84A47==0x37, otherwise verifies all four and reports event 0x252 via 0x6865C.
Callers='-' (periodic/pointer-called RAM guard). [O]
#### 0x00098D32 `hw_health_bits_check` — logic
22 instructions: returns true (unhealthy) when 0xFFFFAC00 != 1, 0xFFFF0804 != 0, or either
DMA channel config (0xFFFFE830/0xFFFFEC30) still has bit 0x8000 set — an init-complete /
quiescent check. Callers='-'. [O]

#### 0x00098E8C `lib_lock_enter` — library
15 instructions: under the 0x9665C lock primitive takes the recursive counter 0xFFF946EC to 1
and saves the outer state in 0xFFF946E8. Callers: the 0x74xxx measurement/diag region and
0x79054 (8 sites). [O]
#### 0x00098EAC `lib_lock_exit` — library
12 instructions: decrements 0xFFF946EC and restores the saved state at zero via 0x96674.
Same 8 callers. [O]
#### 0x00098ED0 `can_irq_mask_set` — logic
Clears interrupt-enable bits (bit8 by arg&1, bit1 by arg&2, bits 0x61 by arg&4) from the
group shadow 0xFFF94706+2·grp and writes it to the controller +10. Caller 0x94D58 (the hw
setup wrapper). [O]
#### 0x00098FA8 `can_rx_slot_read` — logic
Receive read by slot (pool target of 0x94E04): requires the DMA bit in 0xFFF94564, checks
the new-data flags (+0x40/+0x58, up to 5 overwrite retries), unpacks {id<<18/extended,
DLC, 8 big-endian data bytes} from the 0x20 mailbox and acks. [O] (spec 07 §1)
#### 0x00099084 `can_group_mode_get` — data-accessor
14 instructions: maps the group state 0xFFF9471B low bits to 3 (running) / 2 (sleep) / 0.
Caller 0x0007807E. [O]
#### 0x000990A4 `diag_check_cpu_float` — logic
Obfuscated FPU check from the self-test runner 0x98590: a constant ladder on floats
117.0/31356.0 with ABS/SQRT identities against ROM cells DAT_0009915c-64, writing the magic
words 0x46F4F800/0x42EA0000 to a test cell; returns the computed word or 0 on any mismatch.
[O]
#### 0x000991EC `diag_check_cpu_alu` — logic
Obfuscated ALU self-test (cluster of 2 — 0x991EC and 0x991EE are the same check): a long
chain of shifted/saturated arithmetic finishing in bitwise identity verification against
ROM constants DAT_0009939c/a0/a4; returns the signature or 0. Called from 0x98590. [O]
#### 0x000993A8 `diag_check_obf_sig` — logic
Another obfuscated constant/identity check vs DAT_000994a8/ac (byte-swap transforms,
0x69C marker); on failure reports event 0x250 via 0x6865C. Callers='-'. [O]
#### 0x000994B0 `diag_check_obf_chain` — logic
171-instruction obfuscated check chain from 0x98590: calls the helper 0x99610 and pool
0x99618, runs longlong/GBR-relative arithmetic and compares results. [I]
#### 0x00099610 `diag_check_helper` — library
4-instruction obfuscated transform helper ((x<<1)+0x62, byteswap) used by 0x994B0. [O]
#### 0x00099624 `diag_check_ram_shadow` — logic
RAM shadow check from 0x98590: writes param to a cell via PTR_DAT_00099734, reads it back
and walks the ROM byte list LAB_00099738 (decompiler reports overlapping instructions —
region is obfuscated/misaligned). [I]
#### 0x0009987C `diag_check_muldiv` — logic
Multiply/divide identity check from 0x98590 (x·0x100/x, ÷0x2F93, constants
DAT_0009990c/0e/10); returns a magic offset or 0. [O]
#### 0x00099914 `diag_check_table_walk` — logic
87-instruction check from 0x98590: builds ~param locally and walks a pointer table
PTR_DAT_000999c4 (length byte capped at 6) — another obfuscated verify. [I]
#### 0x000999CC `diag_check_obf_hash` — logic
49-instruction obfuscated arithmetic/hash check from 0x98590 (instruction-overlap warning
in the listing). [I]
#### 0x0009A030 `eeprom_cmd_wait` — logic
EEPROM command/status wait (callers 0x988E0, 0x9A0E2): folds the 0xFFFFA900 status bits,
and when the busy bit 0xFFFFA810.4 stays set retries (≤0xFF) the key patterns + command
0x50, on timeout setting the error cells 0xFFF9462A=1 / 0xFFF94624=6; also restarts the
verifier via 0x97F72. [O]
#### 0x0009A0E2 `eeprom_row_key_arm` — logic
Writes the row-address bit | key 0x1E00 into 0xFFFFA850 selected by the length byte of the
EEPROM unit struct. Callers 0x988E0, 0x98B7C. [O]

#### 0x0009A148 `hw_c8_config_check` — logic
30 instructions: returns not-configured when 0xFFFFC842 ≠ 0xAAAA, 0xFFFFC840 ≠ 0xAA,
0xFFFFC82E ≠ 0xFF, (0xFFFFC82C & 0xFFF) ≠ 0xFFF or 0xFFFFC951 bit0 — the config-check
complement of 0x962E2's arm patterns. Callers='-'. [O]
#### 0x0009A190 `lib_lock2_enter` — library
Lock-enter pair with 0x9A1B0 (counter 0xFFF946F4, saved state 0xFFF946F0, primitives
0x9665C/0x96674). Callers 0x0008316C. [O]
#### 0x0009A1B0 `lib_lock2_exit` — library
Lock-exit half: decrements 0xFFF946F4, restores at zero. [O]
#### 0x0009A1D4 `hw_config_verify` — logic
Composite hardware-init verify (callers='-'): 0xFFFE080E/080C/0800 patterns, watch unit
0xFFFC1400 bit0 and 0xFFFC1408 == 0xF5, 0xFFFF1000 bits, the 0xFFFF0812 mode byte (='!' or
<0x10), plus 0xFFFE0430 == 0xE0 when the variant 0xFFF94713 ≠ 1. Returns "any mismatch".
[O]
#### 0x0009A278 `hw_fffff000_verify` — logic
28 instructions: returns mismatch when the shadow 0xFFF9470E (&0x3FF) differs from
0xFFFFF000, or 0xFFFFF100/F102/F106/F003 are not the values 0x96F28 wrote. Callers='-'. [O]
#### 0x0009A2BC `wdt_bits_check` — stub
6 instructions: returns (WTCR 0xFFFE0000 & 7) != 7 — watchdog mode check. Callers='-'. [O]
#### 0x0009A2CC `can_slot_flags_read` — logic
Slot status read (caller 0x94EA0): folds 4 (slot not DMA-armed, table 0xFFF94564), 2
(overwrite pending, +0x58) and 1 (tx-complete +0x30 or rx-complete +0x40) into the return.
[O]
#### 0x0009A324 `can_mode_restore` — logic
18 instructions: re-requests the group's configured mode (0xFFF94718) via 0x97B42 and
returns whether the state byte 0xFFF9471B reached it. Caller 0x94F00. [O]
#### 0x0009A354 `can_mode_stop` — logic
13 instructions: forces mode 0 via 0x97B42 and returns whether the group stopped.
Callers 0x94F00/0x94F7C. [O]
#### 0x0009A374 `can_group_run_check` — logic
9 instructions: clears the 0x10 interim bit of the group state and returns whether the
result is 3 (running). Caller 0x94FC4. [O]
#### 0x0009A388 `can_tx_req_rearm` — logic
22 instructions: for a running group (state & 0x13 == 3) sets the 0x10 request bit, copies
+0x20→+0x28 (tx request register) and clears +0x30 = −1. Caller 0x95024 (burst begin). [O]
#### 0x0009A3FC `lib_counter_dec` — library
16 instructions: decrements the counter cell behind PTR_DAT_0009a420 — a ticket/timeout
decrement API used by 53 call sites across the 0x6A-0x8B application region. [I]
#### 0x0009A428 `lib_irq_lock_enter` — library
Increments the nesting counter behind PTR_DAT_0009a464 and, on 0→1, saves SR & 0xF0 (the
interrupt-mask bits) into a cell — the interrupt-disable enter of the standard critical
section; 58 callers across the application region. [O]
#### 0x0009A558 `kern_state_peek` — unknown
8 instructions: reads the cell behind PTR_DAT_0009a580 (decompiler lost the return value);
part of the event kernel at 0x9A590. Callers='-'. [I]
#### 0x0009A56A `kern_drain_if_due` — logic
10 instructions: when the pending counter (*DAT_0009a584) is below the limit cell, runs the
event drain 0x9A590. Callers='-'. [O]
#### 0x0009A590 `kern_event_drain` — logic
The kernel event-drain loop: repeatedly fetches an index via pool 0x9a600, picks the
handler from the table PTR_PTR_0009a5fc, invokes it through the object's +4 vector, and
XORs a guard word until it falls below the saved limit; restores the saved cursor/limit
after. Callers 0x9A46C, 0x9A56A, 0x9A60C, 0x9A674; helper 0x9A72C. [O]
#### 0x0009A60C `kern_prio_mask_update` — logic
Computes priority mask (highest-cell·2−1) into PTR_DAT_0009a66c, triggers pool 0x9a670 when
the pending count exceeds it, then reloads the mask from the table. Caller 0x00067BE4 (the
boot/kernel init region). [I]
#### 0x0009A674 `kern_event_set` — logic
Sets an event mask (OR into the cell PTR_DAT_0009a6e4), triggering pool 0x9a6f0 when at
IPL 0 and the mask grew; when already set, waits via pool 0x9a6e8 (obj,0,0,4,4) and returns
4. Caller 0x00067A68. This is the set-flag half of the mini event kernel (compare spec 02
§7's dispatcher). [O]
#### 0x0009A6F4 `kern_event_set_simple` — logic
22 instructions: ORs the event mask into PTR_DAT_0009a720 without the trigger path; the
already-set branch waits via pool 0x9a724. Caller 0x00067DF0. [O]
#### 0x0009A728 `kern_nop` — stub
1 instruction: returns — placeholder in the kernel init table (callee of 0x9A46C). [O]
#### 0x0009A72C `lib_clz32` — library
Count-leading-zeros of the cell *DAT_0009a77c using a nibble lookup table (PTR_LAB_0009a780)
plus 16/8/4 shift tests — the kernel priority-selection helper. Caller 0x9A590. [O]
#### 0x0009A784 `crt_term_chain_run` — logic
Runs a termination chain: pool enter (0x9a798), then (obj->vt[2])(obj, fn) on the object at
PTR_DAT_0009a79c, then pool exit (0x9a7a0) — atexit/destructor-style walk. Callers='-'. [I]
#### 0x0009A7A4 `kern_once_wrap` — logic
16 instructions: when the guard cell (PTR_DAT_0009a7c4) is 0, sets it, calls the pool
handler and clears it — one-shot serialization wrapper. Callers 0x9A674, 0x9A6F4; callee
0x00078508. [O]
#### 0x0009A7CC `rt_fpu_regs_save` — logic
Pushes an 18-word FPU context frame (FR0–FR15, FPUL, FPSCR plus 4 args) onto the frame stack
behind PTR_DAT_0009a7d8, advancing it by 0x48 — context save of the frame-switch machinery.
Caller 0x9A784. [O]
#### 0x0009A802 `rt_fpu_regs_restore` — logic
Pops the 0x48-byte FPU frame: reloads the frame pointer (PTR_DAT_0009a810 −0x48 →
PTR_DAT_0009a840) and returns the saved word at −0xC. Caller 0x9A784. [O]

---

## Continuation (clusters resumed after 0x0009A802)

The 29 clusters below were left undocumented when the first pass stopped; appended in unit-file
order (0x0009829C belongs between 0x0009820C and 0x0009838C above; the rest follow 0x0009A802).
This tail is dominated by the SHcompact runtime: C-runtime init, .init-array runner, the
operator-new/malloc heap chain, and the compiler's 64-bit ABI helper family (u64 add/sub/eq/ne,
s64 le/gt, s32→s64, __divmoddi4/__moddi3 with two shift-add multiply twins), plus an obfuscated
RAM/bus-guard diag battery (events 0x251, spec 13 family) around the 0xffff08xx/0xffffac00
blocks.

#### 0x0009829C `can_tx_mailbox_submit` — logic
Driver-level transmit submit used by all four tx pumps (callers 0x00094730, 0x000948F4,
0x00094A04, 0x00094AE6). Requires group state fff9471b[g]==3 (running) and the slot's bit set in
the DMA-enable table fff94564+4g with the controller pending bit (ctrl+0x20, base via
PTR_PTR_00098384) clear; clamps DLC to 8, writes the DLC field at mailbox+0x10 and the payload
words, sets the ID word (standard id<<18, or extended via DAT_00098388 mask | 0x80000000) while
updating the slot bit in fff9457c+4g, then strobes ctrl+0x30/ctrl+0x20 to request. Returns 1 on
submit, else 0. Cluster of 1. [O] spec 08 §1 tx driver layer.
#### 0x00099B34 `hw_fffe60_cfg_mismatch` — logic
Returns true when any handshake/config cell deviates from its programmed value: fffe1200!=0x101,
fffe6002!=1, fffe6040 has bits 2-7 set, fffe6041..45!=0, (fffe6047&5)!=0, or (fffe6000&1)!=1 —
the quiescent pattern of the 0xfffe60xx rspi/DMAC handshake cells programmed by 0x96900
(spi_engine_hw_init). Companion of 0x98D32 hw_health_bits_check. Callers='-' (diag pointer
table). Cluster of 1. [O]
#### 0x00099BA8 `diag_ram_rw_probe` — logic
Obfuscated RAM probe: picks a region from addr bits 14-15 (skips regions 2 and 7; region 0 also
needs fff94710==0xd4), sequences the 0xffff0820/0xffff0822 bus-window keys (0x868a/0x87aa then
0x8600/0x8700), writes old^param_2 to *param_1, reads back, restores, and reports through result
cell param_3 (seeded 0xf3, pass flag param_3[1]); VBR+0x13c must equal param_4. Lock pair
0x9665C/0x96674. Returns 1 pass / 0 fail / 2 not-run. Cluster of 1. [I]
#### 0x00099CC6 `diag_ram_probe_step` — logic
17-instr wrapper: clears DAT_fff94550, runs the 0x99BA8 probe on (&DAT_fff94704, signature
PTR_LAB_00099cf8), then folds the result into nibble accumulator DAT_fff94727
(((old+0x10)&0xf0)+r) — one step of the obfuscated diag battery. Callers='-'. Cluster of 1. [I]
#### 0x00099CFC `diag_ram_cell_rw_test` — logic
RAM cell torture test behind the 0xffff080x guard: saves DAT_ffff0810, writes param_3 then
param_2 to *param_1 comparing read-back, then ~original and original again; any mismatch (or
DAT_ffff0806 set / DAT_ffff080c!=addr) fails, records the faulting address into fff90998 and
raises event 0x251 via PTR_FUN_00099e30 (spec 13 event family). Restores 0xffff0804=0x7600 and
the saved mode. Same region/skip rules as 0x99BA8. Callers='-'. Cluster of 1. [O]
#### 0x00099E38 `diag_bustype_tbl_match` — logic
Compares 4 shorts at PTR_DAT_0009a020 against the expected copy — ROM 0x5DE0 when bus-chip
type cell fff94713==1 (latched by bus_chip_detect 0x95260), else 0xE0. Returns 1 on full match.
Called only by 0x99EFA. Cluster of 1. [I]
#### 0x00099E68 `diag_ac08_state_write` — logic
Writes 2 to 0xffffac08 when bus type fff94713!=1 and returns the constant 5; with 67 instrs vs
the 3 lines recovered, the decompiler plainly lost control flow, so treat the return as
unreliable. Called only by 0x99EFA. Cluster of 1. [I]
#### 0x00099EFA `diag_hw_monitor_check` — logic
Obfuscated monitor check around the 0xffffac00 block: gates on fffc1408/VBR+0x140 vs label
0x9a028 (with fff94729 debounce), runs table match 0x99E38, then the 0xffffac08 sequence via
0x99E68 compared against fff9470d (seeded 0) — bit 0x40 of 0xffff1001 forces "not run" (2). ORs
1|2 into ffffac08 by bus type, folds the result nibble into fff94728, restores fffc1408.
Callers='-'. Cluster of 1. [I]
#### 0x0009A46C `crt_ram_init` — init
C-runtime RAM init called from boot 0x66DA4: copies the initialized-data block
(PTR_DAT_0009a4ec..0009a4e8 from source 0009a4f0) and zero-fills the bss range backwards
(PTR_DAT_0009a4f8 down to 0009a4f4). Then stores param_1 into 0009a500, saves a cell, invokes
PTR_FUN_0009a508 (resolves to kern_nop 0x9A728) and, when the counter at 0009a50c is below
0009a510, PTR_FUN_0009a514 (kern_event_drain 0x9A590) — kernel bring-up hook. Cluster of 1. [O]
#### 0x0009A89C `crt_init_array_run` — dispatch
Walks a static-initializer table: for each 0xc-byte record from PTR_PTR_0009a8bc to
PTR_PTR_0009a8c0, calls the function at record+4 while non-NULL (one resolved callee: 0x15A458).
Classic .init-array/constructor runner; callers='-'. Cluster of 1. [O]
#### 0x0009A8D4 `lib_call_vec_thunk` — stub
6-instr thunk: calls (**PTR_LAB_0009a8ec + in_r0)() — indirect call through a table at 0x9a8ec
indexed by an incoming register; the jump table could not be recovered. Member 0x9A8E0 is the
byte-identical twin (callers 0x15C94C/0x15C968 vs 0x6CBDA). Cluster of 2. [I]
#### 0x0009A9B0 `lib_u64_eq` — library
SHcompact ABI helper: returns whether the two 64-bit stack operands are equal; called from the
0x74970-0x74A04 library run. Cluster of 1. [O]
#### 0x0009A9C8 `lib_u64_ne` — library
Complement of 0x9A9B0: returns 1 when the two u64 stack operands differ (equality result ^1).
Caller 0x74A04. Cluster of 1. [O]
#### 0x0009A9E8 `lib_sp4_word_get` — library
2-instr helper returning the word at sp+4 — tail of a 64-bit result/argument fetch in the
SHcompact ABI. Callers 0x74A04/0x749AE. Cluster of 1. [O]
#### 0x0009A9EC `lib_struct2_store` — library
Stores the 2-word result {0, r0} through the hidden result pointer at sp+0 — SHcompact
small-struct return helper. Callers 0x74970/0x749AE/0x74A04. Cluster of 1. [O]
#### 0x0009AA00 `lib_ld_u16` — library
Interwork load thunk: returns the u16 at *r1. Extremely hot — 35 callers across the
0x9C0000-0xAC000 library region. Cluster of 1. [O]
#### 0x0009AA14 `lib_ld_u32` — library
Interwork load thunk: returns the u32 at *r1; 13 callers in the 0x9C-0xAB library region.
Cluster of 1. [O]
#### 0x0009AA34 `lib_operator_new` — library
C++ operator new: size 0 is promoted to 1; loops calling the allocator PTR_FUN_0009aa70
(=0x9B07C lib_malloc_locked) and, on NULL, runs the installed new-handler (PTR_DAT_0009aa7c) or
the default (PTR_FUN_0009aa78) and gives up with 0 once the default runs. 137 callers across
0xA0000-0x12E000. Cluster of 1. [O]
#### 0x0009AA80 `lib_call6_tramp` — stub
3-instr trampoline calling the code pointer at PTR_LAB_0009aa88 with constant argument 6;
callers='-' (vector/pointer-table path). The meaning of the constant is not observable here.
Cluster of 1. [I]
#### 0x0009AA8C `lib_u64_add` — library
SHcompact helper: 64-bit add of the two u64 stack operands with carry, stored through the
hidden result pointer at sp+0. Cluster of 1. [O]
#### 0x0009AAAC `lib_s64_le` — library
Signed 64-bit a<=b compare of stack operands (signed high words; unsigned low on tie).
Cluster of 1. [O]
#### 0x0009AACC `lib_s64_gt` — library
Signed 64-bit a>b compare of stack operands (b.hi<a.hi signed; on tie b.lo<a.lo unsigned).
Cluster of 1. [O]
#### 0x0009AAEC `lib_s32_sext64` — library
Sign-extends r0 into 64 bits ({r0, bit31 ? 0xffffffff : 0}) stored through the hidden result
pointer at sp+0. Cluster of 1. [O]
#### 0x0009AB04 `lib_s64_divmod` — library
Signed 64/64 division in the GCC __divmoddi4 shape: zero divisor stores 0 and exits; otherwise
normalizes both operand signs, uses narrow-divisor fast paths (16-bit and 8-bit via shift
tricks), multiplies through 0x9AC30, and finishes with a restoring subtract loop. Quotient
returns in r0/r1; remainder written via *param_5. Cluster of 1. [O]
#### 0x0009AC30 `lib_u64_mul_shift_add` — library
u64 multiply step of the division algorithm — a shift-add multiply whose result flows through
the extraout registers, so the decompile degenerates to a no-op test. 0x9AC30 serves 0x9AB04;
twin member 0x9AED6 serves 0x9ADB4. Cluster of 2. [I]
#### 0x0009ADB4 `lib_s64_moddiv` — library
Twin of 0x9AB04 with the result registers swapped: signed 64/64 remainder — the restoring-
subtract loop leaves the remainder in r0/r1 (final sign re-applied) while the quotient is
written to *param_5. Helpers 0x9AED6/0x9AF36/0x9B04C. Callers='-'. Cluster of 1. [O]
#### 0x0009B05C `lib_u64_sub` — library
SHcompact helper: 64-bit subtract of stack operands with borrow, stored through the hidden
result pointer. Cluster of 1. [O]
#### 0x0009B07C `lib_malloc_locked` — library
Locked malloc entry (the allocator invoked by operator new 0x9AA34): lock(1) via PTR_FUN_0009b178,
allocate via 0x9B0C2, unlock(1) via 0009b17c; on lock failure or non-zero unlock it stores error
code 0x834/0x835 into the cell returned by 0009b180 (errno-like) and returns 0. Also reaches
0xA4B44/0xA4B5C/0xA4B68. Cluster of 1. [O]
#### 0x0009B0C2 `lib_freelist_alloc` — library
First-fit free-list allocator: rejects 0/oversized (limit DAT_0009b184), rounds the request to
4 bytes plus a 4-byte header, walks the list at PTR_DAT_0009b188 taking exact fits or splitting
blocks with >=0xC slack; on exhaustion grows a new chunk via 0x9B144 (PTR_FUN_0009b18c recycles
a mismatched block). Returns the pointer past the header. Cluster of 1. [O]

## Unit summary

220/220 clusters documented (191 in the first pass + 29 in this continuation; the catalog also
carries one extra entry, 0x0009A728 `kern_nop`, documented out-of-scope as a callee of
0x9A46C). 227 names rows = 226 unit functions (rep + members) + that extra callee.

Counts by kind (names rows): logic 121, library 28, stub 30, init 18, irq 12,
data-accessor 12, dispatch 1, unknown 5.

Unknown entries (5):
- 0x0009494C `can_rx_group_irq_frag` — rx worker switching on fff94502; decompile broken
- 0x0009617A `unknown_ffff0810_mode_set` — mode-3 set/clear of 0xffff0810 + fff94554/50
- 0x00096684 `unknown_96684_dispatch` — trampoline registration on pool verdict; tail 0x96d6a
- 0x000984DC `unknown_hw_c8_level_get` — level decode of the 0xffffc8xx block
- 0x0009A558 `kern_state_peek` — kernel cell read via ptr 0x9a580

Cross-region leads:
- 0x9AA34 `lib_operator_new` has 137 callers in 0xA0000–0x12E000 (C++ library region) and its
  heap chain 0x9B07C -> 0x9B0C2 also reaches 0xA4B44/0xA4B5C/0xA4B68 — the unit owning
  0xA4Bxx should cross-check the chunk-grow/recycle side (0x9B144/0x9B198).
- 0x9AA00/0x9AA14 (`lib_ld_u16/u32`) are the interwork load thunks used across 0x9C0000–
  0xAC000; any unit cataloging that region will see them constantly.
- 0x9829C `can_tx_mailbox_submit` is the driver-level submit beneath this unit's tx scheduler
  (spec 08 §1); controller bases come from PTR_PTR_00098384.
- 0x9A46C `crt_ram_init` is called from boot 0x66DA4; the kernel bring-up hooks and the
  VBR-relative cells (in_vbr+0x13c/+0x140) belong to the boot/VBR units.
- 0x99CFC raises event 0x251 (fault addr into fff90998) — matches the 0x250/0x251/0x252
  obfuscated diag battery; cross-check spec 13's event table.
- 0x9A8D4/0x9A8E0 thunks index an unrecovered table at 0x9a8ec; their callers live at
  0x15C94C/0x15C968 and 0x6CBDA (other units).
