# Unit 05 (r00_boot)

Boot-manager slice 0x00031D48–0x00033506 plus 0x0003D6E6–0x0003EFC2 (ROM 0x020000–0x03FFFF,
"boot manager / flash services" per spec 06). Three subsystems appear here: the boot manager's
own CAN driver (3 controllers, C_CAN/D_CAN-style arbitration with 32 message objects each), a
multi-channel serial/comm channel-driver family (0x3c-byte control blocks, clock gates at
0xFFFE6030/31), and the flash-download/program state machine used during reflash sessions.

#### 0x00031D48 `init_pin_tbl_and_pfc` — logic
Init helper (called from boot-init chain 0x0003105E): builds a 20-entry table at 0xFFFFFA80
(stride 0x20, fields +0/+1/+3 cleared, +0 seeded from ROM table PTR_DAT_00031dcc) and a 20-byte
table at 0xFFFFFA11 from PTR_DAT_00031dd0; sets three RAM globals 0xFFFFFA51/53/55 = 100 and two
0xFFFFF masks at 0xFFFFFA04/0xFFFFFA00; muxes pins via PFC 0xFFE0C1A/1C/1E/20 = 0x7777 (nibble 7
on four registers). Pattern suggests pin/switch matrix setup with ~100-unit debounce counters.
Cluster of 1. [I]

#### 0x00031DF8 `can_rx_dispatch_ctrl` — dispatch
Scans one CAN controller's message-object pending mask (struct+0x40, ANDed with per-controller
enable mask at 0xFFF850FC), skipping object 0 and starting at object 16 when the low half is
zero. For each pending object it drives the message-RAM interface handshake (struct+0x58
busy-poll, +0x40 ack), decodes arbitration (bit 31 = extended, else ID>>18 & 0x7ff), DLC
(desc+4)&0xf and two data words, then calls 0x000313BC which dispatches to the registered
per-object handler through slot map 0xFFF84EA0 and handler records at PTR_LAB_00031450.
Cluster of 1. [O]

#### 0x00031EA8 `can_txrdy_dispatch_ctrl` — dispatch
Companion of 0x00031DF8 for the second pending set (struct+0x30): scans set bits (object 0/1
special-cased), acknowledges each in struct+0x30, and calls 0x000313FC per object — which arms
bit 0 in 0xFFF84F6C and invokes the object's callback with zeroed arguments (no payload), i.e.
TX-ready/status-complete notification. struct+0x38 set to 0xffffffff on entry. Cluster of 1. [O]

#### 0x00031EFE `can_service_all_ctrls` — dispatch
Loops the 3 CAN controller structs at PTR_PTR_00031F40 calling 0x00031EA8 (tx-ready service),
0x00031DF8 (rx service) and 0x00032E14 (error/status check) for each — the polled CAN driver
tick, invoked from the flash-transfer state machine 0x000321FE between programming steps.
Cluster of 1. [O]

#### 0x00031F22 `can_txrdy_service_all_ctrls` — dispatch
Same loop over the 3 controllers but only calls 0x00031EA8 (tx-ready/status callbacks), no rx
and no error check. Used by 0x000321FE on the program/verify path where rx stays idle.
Cluster of 1. [O]

#### 0x00031F44 `can_ctrl_mode_sync` — logic
Resolves a pending controller-mode transition: if per-controller byte 0xFFF852D1 has 0x80 set,
compares the target (0x80 vs other) with hardware sleep bit (reg+2 bit 12 via PTR_PTR_00032088),
sets the per-controller enable byte at PTR_DAT_0003208C to 1/0, then folds the transition flag
away leaving the 2-bit mode in 0xFFF852D1. Wrapped by lock/unlock 0x000319EC/0x00031A04.
Cluster of 1. [O]

#### 0x00031F98 `can_ctrl_mode_apply` — logic
Executes the controller mode transition decided by 0x00031F44 using target-mode array
0xFFF852D4 (0=stop, 2/3=run variants). Stop path: sets 0xFFF852D1=0x82/0x83, 0xFFF852CE=2/3,
clears enable bits in PTR_DAT_0003208C, sets sleep bit 8 in reg+2, clears reg bit 1. Run path:
copies reg+0x10 to +0x14 (restart value), sets reg bit 1, enable=1, 0xFFF852D1=0x80 (transition
pending). Cluster of 1. [O]

#### 0x00032046 `can_ctrl_request_mode` — logic
API entry to change a controller mode: mode-sync (0x00031F44), store the requested mode in
0xFFF852D4, then apply (0x00031F98). Called from 0x00032D86. Cluster of 1. [O]

#### 0x00032078 `can_ctrl_mode_poll` — stub
Thin wrapper: mode-sync (0x00031F44) then re-apply current target (0x00031F98); called from
0x00032DB4. 8 instructions. Cluster of 1. [O]

#### 0x00032090 `boot_xfer_ch_program` — logic
Programs a channel of the 0xFFE10000-area transfer engine from a config descriptor: disables
bits in 0xFFE100C[ch] and 0xFFE108C[ch], writes a 0x110-stride register block via PTR_PTR_00032180
(+0/+4/+8 = src/dst/count from cfg+4..0xC, count masked by 0x00032184; +0x100..+0x108 from
cfg+0x10..0x18; for channels >3 also +0x10C/+0x10E/+0x8E), plus a nibble write into the
0xFFE1300 table. Invoked from 0x00031A62 right after FCURAME=0xC401, i.e. part of the FCU-RAM
march-test/flash-setup sequence; engine identity (DMA-like) inferred. Cluster of 1. [I]

#### 0x00032196 `flash_xfer_abort` — logic
Aborts the flash-transfer engine: clears the RAM array spanning PTR_SUB_0003237C..PTR_DAT_00032380,
forces engine state byte (PTR_DAT_00032388) = 0x14, calls phase callback PTR_FUN_0003238C if the
session phase (PTR_DAT_00032384[0]) is 0 or 6, and zeroes the state-machine struct fields
+0xa/+0xb/+0xe/+0xf (download/program enable+state). Called from diag-session handlers in
0x00026xxx. Returns 1. Cluster of 1. [O]

#### 0x000321D4 `flash_xfer_stop` — logic
Stops the flash transfer: clears state byte (PTR_DAT_00032388)=0 and the enable bit in
PTR_DAT_00032384[1]; if all enable flags are now clear and phase > 0x11, forces phase 0x11 with
timer PTR_DAT_00032384[2] = 0x1e. Returns 1. Called from 0x00026B32/0x00026CEC/0x00026DF8.
Cluster of 1. [O]

#### 0x000321FE `flash_xfer_state_machine` — logic
Main flash-download/program engine (362 instrs), driven from 0x00026FE4 (diag session). Two
outer phases (0x14 run / 0x16 hold); the transfer struct holds per-side state at +0xa (data
download: 1=in progress, 2, 3=error, 4=done) and +0xe (program/verify), enables at +0xb/+0xf,
error codes at +0xd/+0x12. Download path pulls 0x100-byte chunks via 0x00032540 and shifts a
256-byte staging buffer; program path runs callback chains PTR_FUN_00032390/98/9C/94 around
0x00031EFE + 0x00031F22 (keeps servicing the 3 CAN controllers while flash is busy); second
chain PTR_FUN_000325C4..D4 for the +0xe side; erase-block scheduling visible via bit 0x10 flags
in the param_1[2] byte table. Boot-manager counterpart of the application reflash engine
(spec 16 describes the application one). Cluster of 1. [O]

#### 0x000325D8 `boot_crit_enter_nest` — library
Nested critical-section enter: raises interrupt mask via 0x000319EC(0x50), increments nest
counter 0xFFF85098 and saves the previous mask in 0xFFF85094 on first entry. Used by all
0x00026xxx diag-session handlers. Cluster of 1. [O]

#### 0x000325F6 `boot_crit_exit_nest` — library
Nested critical-section exit: decrements 0xFFF85098, restores the saved mask via 0x00031A04
when the nest empties. Cluster of 1. [O]

#### 0x000326C0 `boot_sio_ch_init_bitrate` — logic
Initializes one channel of the serial/comm channel-driver family: 0x3c-byte control block at
PTR_DAT_00032748, rate-class byte from ROM table PTR_LAB_00021D56_1_0003274C written into the
per-channel register (PTR_PTR_00032744) bits 3..6 (& 0x87), derives scaler 8/9/+1 from table
bits 0x20/0x40, zeroes block state fields (+0xb..+0x28) and stamps DAT_fffff220 time into +0x30,
scaler into +0x36. Callers='-' — reached via table. Cluster of 1. [I]

#### 0x00032770 `boot_sio_irq_status_poll` — irq
Per-channel handler (callers='-', acks hardware): reads channel status reg (+0x10 via
PTR_PTR_000329F0); on bits 0x38 stamps DAT_fffff220 into +0x30, invokes the block callback
+0x18, and for bit 0x10 computes the next due time +0x24 = now − (presc[+0x36]·10⁶)/div[+0x2c]
and arms flag +0x0a bit 0; finally acks by writing 0xC7 to the status register. Cluster of 1. [O]

#### 0x000327D6 `boot_sio_irq_gate_release` — irq
Per-channel handler acknowledging the "run" clock gate: sets hw +0x10=0xBF; if the channel
enable flag (+0xb bit 0) and the channel's even gate bit in 0xFFFE6030 (ch 0: bit 6) /
0xFFFE6031 (ch ≥ 2: bit 2·(ch−1)) are set, it clears hw +8 bit 6, writes the gate register to
drop the bit (e.g. 0xFFFE6030 = 0xFFFFFFBF), stamps times +0x20/+0x30, clears the enable flag
bit and invokes callback +0x10. Cluster of 1. [O]

#### 0x0003286A `boot_sio_irq_gate_hold` — irq
Stop-side twin of 0x000327D6: acknowledges the "hold" gate bit (ch 0: bit 7 of 0xFFFE6030,
written 0x7F; ch ≥ 2: odd bits 2·(ch−1)+1 of 0xFFFE6031), clears hw +8 bits with 0x7B, clears
block flag bit 1 at +0xb and invokes callback +0xc. Callers='-' — vector/table entry.
Cluster of 1. [O]

#### 0x00032968 `boot_sio_ch_rate_set` — logic
Computes the nearest achievable channel rate from a 2.5 MHz base (2500000/n, request doubled
first if 0xFFFF1000 bit 2 clear, clamped to ROM bounds), returning the achieved value; when
commit is requested stores it in the 0x3c-stride RAM shadow at 0xFFF850C8, writes n−1 to hw
reg +4 and clears rate bits 0..1 of hw reg+0. Cluster of 1. [O]

#### 0x000329F8 `init_can_ctrl_state` — logic
Releases clocks for the 3 CAN controllers (0xFFFFD000/D800/E000 |= 1) and zeroes the driver
state arrays for 3 controllers: modes 0xFFF852D1/0xFFF852D4 = 0, 0xFFF852CE = 3, 0xFFF852CB = 1,
interrupt-enable masks 0xFFF850FC = 0; also clears PFC 0xFFE0C32. Cluster of 1. [O]

#### 0x00032A54 `can_mo_config` — logic
Configures one CAN message object: slot = ctrl<<5|object; writes object descriptor at
ctrl+0x100+obj·0x20 (control word 0x2700 base; arbitration = std ID<<18 with mask in word 1, or
extended with IDE bit 31 per flags&0xC0; DLC = param&0xf | 0x100/0x300 | 0x2000), updates
per-controller masks 0xFFF850F0 (valid), 0xFFF850E4 (rx when flags&3==2), 0xFFF850D8
(configured), clears pending in 0xFFF850FC, and mirrors mask bits into hw +0x30/+0x38/+0x40/
+0x58 (and +0x50 inverse). Reached via registration API 0x00031154. Cluster of 1. [O]

#### 0x00032B80 `can_mo_int_set_enable` — data-accessor
Sets or clears one message-object bit in the per-controller interrupt-enable mask 0xFFF850FC
(slot encoding ctrl<<5|object). Called from 0x00031154 right after 0x00032A54. Cluster of 1. [O]

#### 0x00032BAC `can_tx_send` — logic
Queues a CAN transmission on slot ctrl<<5|object: requires controller mode byte 0xFFF852D1
== 3 (running), object configured (0xFFF850D8 bit) and not already pending (hw+0x20 bit);
clamps DLC (0xFF = keep configured DLC), copies up to 8 data bytes into descriptor words
+8/+0xC, optionally rewrites the arbitration (std<<18 or extended+IDE), then sets the object
bit in hw+0x20 (TX request). Returns 1 when queued. Called via API 0x00031350. Cluster of 1. [O]

#### 0x00032C8C `can_ctrl_init` — logic
Controller bring-up for channel 0/1/2 (register blocks via PTR_PTR_00032DB0): sets INIT bit 0
in control reg, resets mode arrays 0xFFF852D1/D4, selects the controller's CAN pins in PFC
0xFFE0C32 (ch1 clears nibbles 0x0F00, ch2 0x00F0), computes the bitrate divider from cfg+4
(doubled when 0xFFFF1000 bit 2 set) into reg+6 with cfg+6 into reg+4, invalidates all 32
message-object descriptors (+0x110+i·0x20 = 0x2700), clears masks 0xFFF850D8/E4/F0/FC, then
sets mode (0xFFF852CE=3 or 2) and control word 0x8041 (normal) / 0x8141 (param_3==1 variant,
bit 8 = listen-only/loopback class). Cluster of 1. [O]

#### 0x00032D86 `can_ctrl_start` — logic
Starts a CAN controller: clears its "ready" byte 0xFFF852CB bit 0, clears INIT bit 0 in the
control register (via PTR_PTR_00032DB0) leaving reset mode, then requests the stored run mode
(0x00032046 with 0xFFF852CE). Callers='-' — API entry via pointer table. Cluster of 1. [O]

#### 0x00032DB4 `can_ctrl_status_get` — data-accessor
Polls the controller mode (0x00032078) and returns packed status: hw status reg+2 & 0x3f plus
driver flags — 0x100 if 0xFFF852CB&8, 0x200 if 0xFFF852CB&1 (started), 0x400 if mode
0xFFF852CE==2, 0x8000 while a mode transition (0xFFF852D1&0x80) is pending. Called from the
error check 0x00032E14 and API 0x0003148C. Cluster of 1. [O]

#### 0x00032E14 `can_ctrl_err_check` — logic
Per-controller error/status hook called by 0x00031EFE: reads hw reg+8; if bits 0x61 are set
(bus-off/error/overflow class) fetches extended status via 0x00032DB4 and invokes the error
callback PTR_FUN_00032E34(ctrl, status). Cluster of 1. [O]

#### 0x00032E38 `can_txq_flush` — logic
Drains a queued-request array at 0xFFF85108 (8-byte records: fields at +0, +4, +6, +8-ish),
one per count byte 0xFFF852D7, passing each record's four fields to handler
PTR_FUN_00032E80; resets the count to 0 afterwards. Called from the flash-transfer state
machine 0x000321FE — queued CAN transmits are flushed around flash busy phases. Cluster of 1. [I]

#### 0x00032E84 `flash_engine_start` — logic
Enables the boot-manager flash engine: control struct (PTR_PTR_00032F4C) +1/+2 = 0, +0 = 1;
if session flag 0xFFF84F74 is 0 fires callback PTR_FUN_00032F50; unlocks flash protection via
0x00032F18; zeroes the relock delay counter (PTR_DAT_00032F54). Called from 0x0002BFA0 at
session setup. Cluster of 1. [O]

#### 0x00032EB6 `flash_engine_stop` — stub
Minimal counterpart of 0x00032E84: sets the engine control struct bytes +1/+2 = 0 and +0 = 0
(disabled). Cluster of 1. [O]

#### 0x00032EC8 `flash_relock_after_delay` — logic
Counts the delay counter (PTR_DAT_00032F54) down each call; at zero re-locks flash protection
(writes 0xFFFFA842=0xD200, 0xFFFFA840=0x2D00, 0xFFFFA852=0xE100, 0xFFFFA850=0x1E00 — all
protection bits closed), runs the status callback PTR_FUN_00032F58 and sets 0xFFFFA811 |= 7
(ok) or |= 5. Called from 0x0002CE9C/0x0002C30C on session end. Cluster of 1. [O]

#### 0x00032F18 `flash_protect_unlock` — logic
Writes the protection unlock pair 0xFFFFA842=0xD2FF, 0xFFFFA840=0x2DFF (all bits open) and
increments the relock delay counter (capped 0xFE), i.e. arms the "relock later" timer used by
0x00032EC8. Cluster of 1. [O]

#### 0x00032F5C `flash_fcu_seq_state_machine` — logic
FCU erase/program sequencer polled from the session handlers (0x0002DC3C/0x0002D9A4/0x0002CED4).
Control struct (PTR_PTR_0003315C): [0] main state (0 idle, 2 erase loop, 3 program, 4 done),
[1] timeout/step (0x30 command timeout), [2] request substate (1 erase, 2 program, 3 finish),
[4]/[5] block/remaining, +0xC staging pointer. Erase path decrements remaining blocks via
FUN_00032F38; program path calls PTR_FUN_00033168, writes FCU commands 0xE8, word count and
0xD0 to the staging window and copies words from PTR_PTR_00033164; every step is validated by
0x000332C4 and failures set session error state (DAT_fff84f74 = 6). Cluster of 1. [O]

#### 0x000330F6 `flash_erase_cmd_start` — logic
Queues an erase request: only when idle (state < 4, substate 0); stores substate 1, last block
index = param_1+param_2−1 into [4] and block count into [5], resolves the staging pointer via
FUN_00032F38 into +0xC. Returns 1 when accepted. Cluster of 1. [O]

#### 0x00033130 `flash_program_cmd_start` — logic
Queues a program request: block index param_1 with ushort offset param_2 (wrapped in 0x2000
units), size byte 0x80 (or 8 when param_4==1) into [5], staging pointer via FUN_00032F38 into
+0xC, then copies (size>>2) 32-bit words from param_3 into the staging buffer at
PTR_PTR_00033164. Returns 1 when accepted. 0x80-byte pages match the 256-byte FCU program
granularity (spec 16 §2). Cluster of 1. [O]

#### 0x000331C0 `flash_lockbit_prog_cmd` — logic
Issues the FCU lock-bit program command (writes 0x71 then 0xD0 to the translated P/E address),
after selecting the block (0x00033372) and setting 0xFFFFA91A = addr & 0x1FF8; waits for
FSTATR0 (0xFFFFA900) bit 7 with a 0x4B0 spin, checks bits 0x40 and 0xFFFFA91F bit 0 for
errors, clears FENTRYR-frozen bit in 0xFFFFA802 and finally writes FENTRYR = 0xAA00 (ROM read
mode). Failure path sets session error state DAT_fff84f74 = 6. When 0xFFF852C5 bit 5 is set it
instead blank-checks 4 words == 0xFFFF. Called from many 0x2B–0x2D session handlers.
Cluster of 1. [O]

#### 0x000332C4 `flash_fcu_status_read` — logic
Reads/packs FCU status: FSTATR0 (0xFFFFA900) bits 0x21/0x2/0x10/ready plus FSTATR1-class
0xFFFFA810 & 0x90; if 0xFFFFA810 bit 4 sticks it retries the FCU clear-status command (writes
0x50 into the P/E window) up to 0xFF times, with the protection-pair write 0xE1FF/0x1EFF first;
a fully stuck status triggers reset callback PTR_FUN_000333D4 and session error state
(DAT_fff84f74 = 6). Optionally exits P/E mode (FENTRYR = 0xAA00) when param_1 is set and an
error bit is present. Cluster of 1. [O]

#### 0x00033372 `flash_erase_block_select` — logic
Selects the erase block: from the current block index (struct+4) computes a single block bit and
ORs it into 0xFFFFA850 (blocks 0–7, base 0x1E00) or 0xFFFFA852 (blocks ≥8, base 0xE100) —
per-block protection enable; then enters the erase P/E window with FENTRYR = 0xAA80, validates
status via 0x000332C4(0) and writes 0xFFFFA904 = 0x5501. Cluster of 1. [O]

#### 0x00033418 `boot_run_init_table` — dispatch
Walks a registration table between PTR_PTR_00033438 and PTR_PTR_0003343C (stride 0xC) and calls
each non-NULL function pointer at entry+4 — boot-manager constructor/init list. Called from
0x00024A26 and 0x00022224. Cluster of 1. [O]

#### 0x000334B4 `flash_fcu_status_clear` — logic
Clears FCU status at a caller-supplied window address: combines param_2 with 0xFFFFA810 & 0x90;
if bit 4 is set it clears the status register (write 0) and issues FCU clear-status 0x50 up to
0xFF times, falling back to the FCU reset thunk func_0x0003347C; finally writes FENTRYR =
0xAA00 and returns the packed status. Callers='-' — API table entry. Cluster of 1. [O]

#### 0x00033506 `flash_set_pe_window` — logic
Translates a ROM address into the FCU P/E window: computes the FENTRYR key — MAT-1 variant
(0xFFF852C7 == 1) gives 0xAA0C or 0xAA03 below 0x100000; otherwise FENTRYR = 0xAA00 |
(1 << (addr>>20)) while addr < MAT size ((0xFFF852C6 & 0x30)·0x14000 + 0x140000), else the
256 KB-granularity bits (8 << ((addr&0xFFFFF)>>18)). Then writes 0xFFFFA904 = 0x5501 and
replaces the caller's pointer with addr + 0x80800000 (the documented P/E mirror, spec 16 §2).
Cluster of 1. [O]

#### 0x0003D6E6 `boot_handover_final` — logic
End-of-boot-manager decision point (callers='-', ends in a non-returning tail after PTR_FUN_0003D7F8; decompiler hits bad instruction data at the final jump). First validates the two flash images via callbacks PTR_FUN_0003D7F0 over PTR_DAT_0003D7EC/7F4; if both are valid it honours a RAM-downloaded helper — magic 0x5AA5 / 0xA55A / 'H' / 02 / 02 at 0xFFF80080, entry resolved as *(code**)(*(u32)PTR_DAT_0003D7EC_struct+0x10's +0x34) when the pointer lands in range — and in all valid-image branches clears the marker pages 0x187F00–0x187FFF and 0x1BFF00–0x1BFFFF via 0x0003D8F6. Boot-selection behaviour that spec 06 lists as open. Cluster of 1. [I]

#### 0x0003D782 `flash_range_prog_sm` — logic
Eight-state range-program worker: states 0/1 install range A into the 0xFFF84000 descriptor (start/end/count) and start op PTR_FUN_0003D938; state 2 polls PTR_FUN_0003D93C; state 3 installs range B into 0xFFF84010; state 4 slices the source buffer (*param_2 advances) into ≤0x800-byte chunks (descriptor 0xFFF8401C/0xFFF84020); state 5 polls each chunk via PTR_FUN_0003D944; state 6 polls PTR_FUN_0003D948; 7/8 = success. Returns 0 while busy, 1 when complete. Two range ops followed by chunked writes reads as erase-then-program. Cluster of 1. [I]

#### 0x0003D8F6 `flash_markers_fill_clear` — logic
Fills a constant (param_2, called with 0) over a ROM range using 0x0003DC00, looping the fill engine until it stops returning busy. Called from 0x0003D6E6 for 0x187F00–0x187FFF and 0x1BFF00–0x1BFFFF — the marker/flag pages at the top of each writable flash region (compare spec 06 §3 markers). Programming 0x00 into 0xFF marker bytes invalidates them. Cluster of 1. [O]

#### 0x0003DA56 `flash_fill_cmd_start` — logic
Start of the fill engine: copies the request descriptor (state, flags, start at +4, end at +8) into the control block PTR_DAT_0003DBF0, sets block+0x14 active, zeroes the pending-count DAT_fff84040, resets the 0x2000 staging ring (0x0003E0D6) and kicks PTR_FUN_0003DBF8/PTR_DAT_0003DBF4; updates the engine state on completion. Returns 0 busy / 3 done / 4 error. Cluster of 1. [O]

#### 0x0003DAC6 `flash_fill_data_pump` — logic
Streams fill data into the staging ring: for engine states 1/4/5 it takes min(remaining DAT_fff84040, 0x800, 0x2000 − ring fill) bytes and copies them via 0x0003E060 from the source window, advancing block write pointer (+8) and remaining count; returns whether more pumping is needed; state 8 aborts. Cluster of 1. [O]

#### 0x0003DB66 `flash_fill_pad_finish` — logic
Finish step of the fill engine: requires write pointer +8 == end +0xC; in state 5, if the ring is empty, pads 0xFF bytes until the write offset is 0x100-aligned; states 1/4/6 report busy(1), state 7 resets to 0 (done), state 8 resets to error 3. Cluster of 1. [O]

#### 0x0003DC00 `flash_fill_range_engine` — logic
Top-level fill-over-range engine (states in DAT_fff84042): 0 → start (0x0003DA56); 1 → build a 0x100-byte buffer of the fill value and pump it (0x0003DAC6); 2 → pad/finish (0x0003DB66); any non-busy result resets the state. Used by 0x0003D8F6 with fill value 0. Cluster of 1. [O]

#### 0x0003DE46 `crc_ranges_compute` — logic
Feeds a linked list of range descriptors (nodes linked at +0xC: {addr, len, repeat-flag}) plus one initial range (param_1+8/+0xC) through the checksum object at PTR_DAT_0003DEB8 (method at PTR_PTR_0003DED4, called twice per node when the node flag is 1), finalizes with FUN_0003ee7a over seed DAT_fff8402c, stores the digest at object +0x11 and marks +0xF = 1. Callees in the 0x1Bxxxx block. Cluster of 1. [I]

#### 0x0003DE9A `crc_engine_seed` — stub
Two-step stub on the checksum object at PTR_DAT_0003DEB8: stores the 32-bit seed DAT_fff8402c into the object and sets byte +0xB = 1 (armed). Cluster of 1. [I]

#### 0x0003DED8 `boot_ram_stride4_test` — logic
RAM self-test step: saves the words at 0xFFF80000/0xFFF81000/0xFFF82000/0xFFF83000, writes four pattern words from table PTR_PTR_0003DFC4 into those locations, verifies each against the pattern (mismatch records the address in DAT_fff84024 and fires error callback PTR_FUN_0003DFC8(0x211)), restores the saved words, then unlocks (0x0003E84C). Callees in the 0x1Bxxxx block. Cluster of 1. [I]

#### 0x0003DF28 `boot_ram_pattern_write4` — library
Small register-based helper (decompiler artifacts for r1/r11/SPR): copies 4 words from a source pointer into a destination with a stride, then restores the interrupt state via 0x0003E84C — the pattern-install counterpart of the test/restore in 0x0003DED8. Purpose beyond that not recoverable. Cluster of 1. [I]

#### 0x0003E060 `flash_ringbuf_copy` — library
Copies len bytes from src into the 0x2000-byte staging ring at 0xFFF845A8: wraps at the buffer end (second 0x0003E180 segment), advances the write pointer DAT_fff8883C and the fill counter *DAT_0003E158; returns 1 if the copy fit (fill+len ≤ 0x2001), 0 if the ring is full. Cluster of 1. [O]

#### 0x0003E0D6 `flash_ringbuf_reset` — stub
Resets the staging ring: base pointer (PTR_DAT_0003E15C), write pointer DAT_fff8883C and second pointer DAT_fff88840 all to 0xFFF845A8, fill counter to 0. Cluster of 1. [O]

#### 0x0003E180 `lib_memcpy_align` — library
Alignment-aware memcpy splitter: breaks a copy into a leading unaligned part, a 4-byte-aligned middle and a trailing part (computing lengths from src/dst mod 4 when that pays off), delegating the segments to byte- and word-copy thunks 0x0003E16A/0x0003E17A. Cluster of 1. [O]

#### 0x0003E9E0 `boot_xfer_ch_program` — logic
Second binding of the transfer-channel programmer (byte-pattern twin of 0x00032090): disables 0xFFE100C[ch]/0xFFE108C[ch], writes the 0x110-stride channel register block via PTR_DAT_0003EADC (src/dst/count masked by DAT_0003EAE0, extra words +0x100..+0x108, channel >3 extras +0x10C/+0x10E/+0x8E) and the nibble table at 0xFFE1300. This copy belongs to the flash-services block bound to the 0x1Bxxxx code area. Cluster of 1. [O]

#### 0x0003EB5A `flash_xfer_state_machine__2` — logic
Byte-pattern twin of 0x000321FE (same 349-instruction download/program state machine: phases 0x14/0x16, struct states at +0xA/+0xE, 0x100-byte chunk staging via FUN_0003EE7A, program/verify callback chains including the local lock/unlock pair) but bound to the flash-services tables (_LAB_0003ECD4..0x3EF00) with callbacks in RAM (0xFFF8898A/0xFFF88AC4) and code in the 0x1Bxxxx block. Callers='-' — reached via table. Cluster of 1. [O]

#### 0x0003EFC2 `flash_set_pe_window__2` — logic
Byte-pattern twin of 0x00033506 using this block's config byte DAT_fff841A0: computes the FENTRYR key (0xAA0C / 0xAA03 below 0x100000 in the MAT-1 variant; 0xAA00 | 1<<(addr>>20) within the MAT; 0x200 KB-granularity bits 8<<(…>>18) above), writes 0xFFFFA904 = 0x5501 and rewrites the caller's address to +0x80800000. Callers='-' — API table entry. Cluster of 1. [O]

## Unit summary

59 clusters, 59 functions (every cluster is a singleton — no duplicated byte-patterns within
this unit, though three functions have byte-pattern twins elsewhere, see leads).

Counts by kind:
- logic: 40
- library: 5 (0x000325D8, 0x000325F6, 0x0003DF28, 0x0003E060, 0x0003E180)
- dispatch: 5 (0x00031DF8, 0x00031EA8, 0x00031EFE, 0x00031F22, 0x00033418)
- irq: 3 (0x00032770, 0x000327D6, 0x0003286A — vector/table-reached channel handlers)
- stub: 4 (0x00032078, 0x00032EB6, 0x0003DE9A, 0x0003E0D6)
- data-accessor: 2 (0x00032B80, 0x00032DB4)
- unknown: 0

No `unknown` entries; the honest-inference ones (marked [I] in the entries) are:
0x00031D48 (init_pin_tbl_and_pfc), 0x00032090/0x0003E9E0 (transfer-engine channel programmer,
peripheral identity unconfirmed), 0x00032E38 (tx-queue flush — handler pointer is indirect),
0x000326C0/0x00032770/0x000327D6/0x0003286A/0x00032968 (multi-channel "sio" driver family —
exact peripheral not identified: 0x3c control blocks, 0xFFFE6030/31 clock-gate pairs, 2.5 MHz
base rate), 0x0003D6E6 (handover semantics), 0x0003D782 (erase-vs-program reading),
0x0003DE46/0x0003DE9A (checksum assumed CRC-class), 0x0003DED8/0x0003DF28 (RAM self-test pair).

Subsystem map of this unit:
- Boot-manager CAN driver: 3 controllers, 32 C_CAN/D_CAN-style message objects each
  (0x00031DF8, 0x00031EA8, 0x00031EFE, 0x00031F22, 0x00031F44, 0x00031F98, 0x00032046,
  0x00032078, 0x000329F8, 0x00032A54, 0x00032B80, 0x00032BAC, 0x00032C8C, 0x00032D86,
  0x00032DB4, 0x00032E14, 0x00032E38). Slot→handler map 0xFFF84EA0, handler records at
  PTR_LAB_00031450, registration API 0x00031154, transmit API 0x00031350.
- Flash session engine (CAN-driven): 0x00032196, 0x000321D4, 0x000321FE, 0x00032E84,
  0x00032EB6, 0x00032EC8, 0x00032F18 plus the FCU command layer 0x00032F5C, 0x000330F6,
  0x00033130, 0x000331C0, 0x000332C4, 0x00033372, 0x000334B4, 0x00033506.
- Boot handover / self-test / second engine binding: 0x0003D6E6–0x0003EFC2.

Cross-region leads:
- The 0x0003D6E6–0x0003EFC2 block calls code at 0x001BD4F8, 0x001BDCC4, 0x001BDEAE,
  0x001BDFD4, 0x001BE2BC, 0x001BE57A, 0x001BE708, 0x001BE828, 0x001BE930, 0x001BEAE4,
  0x001BEAF2, 0x001BEED6 — inside the 0x180000–0x1FFFFF window. Whoever owns that region
  should treat it as the flash-services helper block (its second binding of the transfer
  state machine lives there, with RAM callbacks 0xFFF8898A/0xFFF88AC4).
- Byte-pattern twins of unit-05 functions exist elsewhere: 0x00032090≈0x0003E9E0,
  0x000321FE≈0x0003EB5A, 0x00033506≈0x0003EFC2. Other units finding similar bodies should
  reuse the same names (the pairs differ only in bound tables/globals).
- Flash register map observed here extends spec 16's: FENTRYR 0xFFFFA902, 0xFFFFA904 (0x5501
  strobe), FSTATR-class 0xFFFFA900/0xFFFFA810/0xFFFFA91F, block-address 0xFFFFA91A,
  protection pairs 0xFFFFA840/0xFFFFA842 (unlock 0x2DFF/0xD2FF, relock 0x2D00/0xD200) and
  0xFFFFA850/0xFFFFA852 (per-block erase select 0x1E00/0xE100 bases), FCURAME 0xFFFFA854,
  MAT control 0xFFFFA820, state bytes 0xFFFFA802/0xFFFFA811. P/E window = +0x80800000.
- 0x000321FE is called only from 0x00026FE4 and the session services in 0x0002B–0x0002E —
  those UDS/transport handlers belong to another unit and can anchor on this engine.
- The application-side CAN driver (specs 07/08, 0x40000+) is a different code base from this
  boot-manager CAN driver; don't merge them.
