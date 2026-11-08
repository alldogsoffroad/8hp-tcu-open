# Unit 03 (r00_boot)

Slice of the Bosch 8HP70 TCU firmware: bootloader tail (0x1DF92–0x1FFFF) and boot-manager /
flash-services helpers (0x20000–0x26A46 in this unit), plus their duplicated copies in the boot
manager (0x31xxx–0x3Exxx) and application (0x44xxx–0xA1xxx). Semantics anchored on specs 06
(boot/application interface), 15 (diagnostic services), 17 (userboot plant protocol) and the
CAN driver family around 0x1D552/0x1D758. RAM globals 0xFFF8xxxx–0xFFF9xxxx; peripherals
0xFFFC0000–0xFFFFFFFF (INTC 0xFFFE0Cxx, A-DMAC 0xFFFE6xxx, FCU 0xFFFFA8xx/A9xx per spec 03/16).

#### 0x0001DF92 `can_ctl_deactivate` — logic
Per-CAN-controller deactivate: zeroes state byte 0xFFF85A77[i], clears bit 0 of the controller
flag word reached through pointer table 0x1DFB4, then helper 0x1DF70 copies requested-state
0xFFF85A78[i] to current/shadow 0xFFF85A79[i]/0x7A[i] and clears bit 1. Called only by CAN
driver init 0x1D552. Cluster of 1. [I] (module attribution from the 0x1D552 CAN-init family;
register behavior [O]).

#### 0x0001DFB8 `boot_dma_regs_reset` — logic
Boot init (called from 0x1D034 early-init chain): zeroes eight 0xFFFE10xx registers at 0x10
stride (0xFFFE100C…0xFFFE107C) and the 0xFFFE6030–0xFFFE608E A-DMAC/clock-gate block (spec 03
§RSPI: request-disable regs 0xFFFE6074/76), clears flag 0xFFFE1204, then sets 0xFFFE1200=0x0101
and re-enables 0xFFFE6000/0xFFFE6002. Cluster of 3 (copies at 0x31884 boot-manager,
0x96010 application — identical constants). [I]

#### 0x0001E070 `boot_periph_c800_init` — logic
Resets the 0xFFFFC800 peripheral bank: clears 0xFFFFC830–C844 and C860–C892 state, loads
compare-like constants 0x125/0x24A (bank C86x) and 0x61B/0x824 (bank C88x) plus 0xFFFF values
at C830/C83C, and clears bit 0 of 0xFFFFC921 (output enable). Values look like timer
period/duty pairs. Cluster of 2 (+0x3193C). [I]

#### 0x0001E0D2 `boot_periph_c800_setbits` — logic
Complement of the 0xC800 init: ORs 0xAAAA/0x00AA into 0xFFFFC840/C842 and 0x14AA/0x0005 into
0xFFFFC870/C872, 0x0AAA/0x00A8 into 0xFFFFC890/C892 — setting alternating bit patterns
(test/enable masks) across the three channel banks. Cluster of 2 (+0x3199E). [I]

#### 0x0001E138 `irq_mask_set_15` — library
Raises the SH-2A SR interrupt mask to level 15 (reads SR, ORs 0xF0, returns) — all maskable
interrupts off; the classic boot/application entry step (spec 06 §4 step 1). Cluster of 4
(+0x31A04, 0x3E84C, 0x96674: boot-manager and application copies). [O]

#### 0x0001E148 `boot_periph_f100_init` — logic
Configures the 0xFFFFF100 peripheral with one of two constant sets (0x13/4/6 vs 0x27/9/0xD plus
0xFFFFF106=0/1) selected by bit 2 of the mode register 0xFFFF1000 (MD_CLKP, spec 05 §1) —
i.e. rate setup dependant on the clock source. Sets 0xFFFFF003=0x9F, ORs bit 0 into 0xFFFFF000
and mirrors it through PTR_DAT_0001e1b8. Cluster of 1. [I]

#### 0x0001E194 `boot_f000_reg_or` — logic
ORs the given bits into the control word at PTR_DAT_0001e1b8 and mirror 0xFFFFF000 (peripheral
0xFFFFF000 bank). Sibling of 0x1E1A4 (AND ~bits). Cluster of 1. [O]

#### 0x0001E1A4 `boot_f000_reg_and` — logic
Clears the given bits in the control word at PTR_DAT_0001e1b8 and mirror 0xFFFFF000. Inverse of
0x1E194. Cluster of 1. [O]

#### 0x0001E1BC `boot_frc_irq_disable` — logic
Stops the 0xFFFFF200 timer (FRC, spec 02/16: free-running counter at 0xFFFFF220): zeroes
0xFFFFF202/204/208/209 and clears its INTC priority nibble 0xFFFE0C06. Cluster of 3
(+0x31D2C, 0x96F7C). [I]

#### 0x0001E1D8 `boot_irq_priority_bulk_set` — logic
Fills the 0xFFFFFA80+i*0x20 peripheral table (20 entries) from a 20-byte "dddd…" boot-data
string (ptr 0x1E258/0x1E25C), sets 0xFFFFFA51/53/55=100, 0xFFFFFA00/0xFFFFFA04=0x000FFFFF,
then sets interrupt priorities en masse: 0xFFFE0C1A=0x9999, 0xFFFE0C1E=0x9999 and
0xFFFE0C1C/0C20 |= 0x9990 (priority 9 across two INTC banks). Cluster of 1. [I]

#### 0x0001E260 `boot_ffffff40_irq_enable` — logic
Writes 0x10/0x00 to 0xFFFFFF40/41 and sets the matching INTC nibble 0xFFFE0C22 (low byte |= 0x20)
to priority 2 — unmasks one interrupt of the 0xFFFFFF40 peripheral (WDT-class; exact module
open). Cluster of 3 (+0x31DD4, 0x977CC). [I]

#### 0x0001E284 `boot_cpg_protected_write` — logic
Key-protected CPG write (magic 0x5A80 to 0xFFFE0006, param|0x5A00 to 0xFFFE0002, 0xA563 to
0xFFFE0000): parameterised clock-generator setting — likely divider/rate selection. Called from
bootloader 0x10B70. Cluster of 3 (+0x31A14, 0x966B8). [I]

#### 0x0001E2B8 `can_tx_load_object` — logic
Loads one RCAN message object for transmit: splits param_1 into controller (bits 5-12) and
message bit (0-1F), requires controller state 0xFFF85A79[ctl]==3 (running) and the enable mask
at 0xFFF85768+ctl*4, then fills slot ctl_base+0x100+msg*0x20: DLC nibble (caps at 8; 0xFF keeps
old), up to 8 data bytes, and the identifier (standard (id&0x7FF)<<18, extended id&0x1FFFFFFF |
0x80000000); finally sets TX-request bit at ctl_base+0x20 and returns 1. Cluster of 1. [O]

#### 0x0001E398 `can_rx_dispatch_ctrl` — logic
Receive pump for one CAN controller: scans the received mask at ctl+0x40, and for every message
object reads slot ctl+0x100+msg*0x20 (ID word, DLC nibble, data), decodes standard
(>>0x12 &0x7FF) vs extended (mask 0x1E4DC) identifiers and calls 0x1D758(global_msg_index, id,
len, data), which dispatches through the handler table at 0xFFF8571C → descriptors (PTR at
0x1D7EC, 0x14 bytes each with callback at +0x10). Cluster of 1. [O]

#### 0x0001E448 `can_tx_confirm_ctrl` — logic
Transmit-confirmation pump: scans ctl+0x30 completed mask, and for each object calls 0x1D798
(global index = msg + ctl*0x20), which sets the "sent" bit in 0xFFF85763[h] and invokes the
message handler callback. Cluster of 1. [O]

#### 0x0001E49E `can_irq_handler` — irq
CAN interrupt body (callers='-', reached via vector): for the one controller in pointer table
0x1E4E0 runs tx-confirm (0x1E448), rx-dispatch (0x1E398) and the error/status check 0x1E742.
Cluster of 1. [I]

#### 0x0001E4C2 `can_irq_tx_confirm` — irq
Reduced CAN interrupt body (callers='-'): only runs the tx-confirmation 0x1E448 over the
controller list at 0x1E4E0 — the TX-complete vector counterpart of 0x1E49E. Cluster of 1. [I]

#### 0x0001E4E4 `can_profile_select` — logic
Chooses the active CAN profile: when flag 0xFFF85A75 bit 0 is set uses the alternate profile
block at 0x1E6E0 instead of boot-data table 0x5632 ("CAN profiles for the bootloader", spec 06
§1) and loads the driver-config pointer (profile+0x10) into 0xFFF85794. Called by session
managers 0x15824 and 0x1A4B2. Cluster of 1. [I]

#### 0x0001E4FE `boot_can_setup_engine` — logic
Central bootloader state machine over the driver struct at PTR 0x1E6E4: states 0x11/0x15/0x19/
0x1E gate transitions (auto-fall back to 0x15 on 0x19/0x1E conditions); state 3 arm phase
exits FCU entry mode (0x1E6A6), writes 0xFFFFA854=0xC401, programs the RCAN mailbox via 0x1E760
from the selected profile (ptr at profile+0x14), arms the 0xFFFE103C bit and waits the
0xFFFE1038 counter; completion issues command 0xFFFE10BC=0xEF, 0xFFFFA854=0xC400 and moves to
sub-state 4/6. Also toggles 0xFFFF1002=0xC300/0xC301. Called by seven bootloader session
functions (0x12384, 0x15824, 0x158D0, 0x168D0, 0x16DC0, 0x178C0, 0x17B50). Cluster of 1. [I]

#### 0x0001E68C `can_setup_trigger_full` — logic
Requests a full CAN setup restart: sets driver state (PTR 0x1E6E4) to 3, sub-state byte[3]=0 and
clears byte[1]. Called by 0x1E864. Cluster of 1. [I]

#### 0x0001E69A `can_setup_trigger` — logic
Requests CAN (re)setup but keeps byte[1]: sets driver state 3, sub-state 0. Called by 0x1E94A,
0x12384, 0x1F13C, 0x1F240, 0x1EEE4. Cluster of 1. [I]

#### 0x0001E6A6 `flash_mode_exit_rom` — logic
After a 20-iteration delay writes 0xFFFFA810=0, FENTRYR (0xFFFFA902) = 0xAA00 and
0xFFFFA906=0xCC00 — returns the flash controller to normal ROM-read mode (FENTRYR key 0xAA00,
spec 16 §FCU). Cluster of 3 (+0x31C0A, 0x3E714). [O]

#### 0x0001E6E8 `can_ctl_status_flags` — logic
Builds a 16-bit per-controller status word: profile value (ctl+2) & 0x3F, plus bit 8 if
0xFFF85A77[i]&8, bit 9 if &1, bit 10 if state 0xFFF85A79[i]≠3, bit 15 if &0x80; refreshes via
0x1D9AC first. Cluster of 1. [I]

#### 0x0001E742 `can_ctl_error_report` — logic
Reads the controller status word (ctl+8); if error/bus bits 0x61 are set, collects detailed
flags via 0x1E6E8 and reports through 0x1D848. Called per interrupt from 0x1E49E. Cluster of 1.
[I]

#### 0x0001E760 `can_mailbox_program` — logic
Programs one RCAN message object from a config struct: clears enable bit 0xFFFE100C[ch*4].0,
issues the 0xFE→0xEF command strobe at 0xFFFE108C+ch*0x10, copies ID/mask/config words (16B +
0x1C) into the per-channel object RAM via pointer table 0x1E85C, and stores the config nibble
at 0xFFFE1300+(ch>>1)*4. For ch≥4 also writes +0x10C/+0x10E/+0x8E (0x5B00-base word). Cluster
of 1. [O]

#### 0x0001E864 `boot_engine_state_reset` — logic
Resets the bootloader engine struct behind PTR_PTR_0001e92c (state=1, bytes 1/2=0), calls a
hook (0x1E930) when 0xFFF8578C==0, re-arms the FCU window via 0x1E8F8 and clears the retry
counter (PTR 0x1E934). Called by session manager 0x15824. Cluster of 1. [I]

#### 0x0001E896 `boot_engine_state_off` — logic
Same struct as 0x1E864: clears bytes 1/2 and sets state 0 — parks the engine. Cluster of 1.
[I]

#### 0x0001E8A8 `boot_fcu_retry_tick` — logic
Decrements the retry counter (PTR 0x1E934); on expiry writes 0xFFFFA840=0x2D00,
0xFFFFA842=0xD200, 0xFFFFA850=0x1E00, 0xFFFFA852=0xE100 (FCU window reset), calls hook
0x1E938 and sets result bits 5/7 into 0xFFFFA811. Called periodically by five session
functions (0x12384, 0x15B24, 0x161AC, 0x1B628, 0x1B8FA). Cluster of 1. [I]

#### 0x0001E8F8 `boot_fcu_window_arm` — logic
Writes 0xFFFFA840=0x2DFF and 0xFFFFA842=0xD2FF (FCU window select pair) and increments the
retry counter at PTR 0x1E934 (capped at 0xFE). Cluster of 1. [I]

#### 0x0001E918 `eeprom_block_addr` — library
Returns 0x80100000 + block*0x2000 + offset — data-flash (EEPROM) address calculator for the
128 KB array in 8 KB blocks (spec 01 §memory map). Cluster of 3 (+0x32F38, 0x97D7A). [O]

#### 0x0001E93C `can_msg_state_clear` — logic
Zeroes bytes 10, 11, 0xE, 0xF of the struct pointed to by *param_1 — clears four message
state bytes (tail of 0x1E94A does the same). Cluster of 2 (+0x32188). [I]

#### 0x0001E94A `can_engine_restart` — logic
Copies the config table (ptrs 0x1EB2C–0x1EB34, byte loop) into RAM, sets rate byte 0x1EB3C=0x14,
fires hook 0x1EB40 when engine state (0x1EB38) is 0 or 6, requests CAN setup via 0x1E69A and
clears the four state bytes like 0x1E93C. Returns 1. Cluster of 1. [I]

#### 0x0001E988 `can_engine_clamp_state` — logic
On struct 0x1EB38: clears counter 0x1EB3C and byte1 bit 0; if byte1&0xFE==0 and state byte0
exceeds 0x11, clamps state to 0x11 with sub-state 0x1E. Cluster of 1. [I]

#### 0x0001ECF4 `tbl_range_lookup` — library
Interval lookup: walks the 8-byte-entry table at *(p+4) from index (*(p+0x18)-1) downward and
returns the index whose [start, start+len) contains param_2, else -1. Cluster of 3 (+0x32540,
0x3EE7A). [O]

#### 0x0001ED20 `tbl_range_mark` — logic
Marks a bit-0x10 range: looks up the start and end indices (0x1ECF4) for [param_2, param_3] and
ORs 0x10 into the flag bytes at p[2]+i; sets dirty flag byte at *p+0xF = 1. Cluster of 2
(+0x3256C). [O]

#### 0x0001ED50 `tbl_range_unmark_all` — logic
Clears bit 0x10 from all flag bytes p[2]+0..count(*(p+6)) and resets the dirty flag (*p+0xF=0).
Cluster of 2 (+0x3259C). [O]

#### 0x0001ED8C `boot_hang_forever` — logic
Empty infinite loop — deliberate hang (fatal-error trap; the body between the branches is
unreachable). Cluster of 4 (+0x1F200, 0x33284, 0x98C4C: three more hangs). [O]

#### 0x0001EDDC `irq_lock_nest` — logic
Nested critical-section enter: masks interrupts to level 15 via 0x1E138 (returns old SR),
increments 0xFFF858DC and stores the old SR at 0xFFF858D8 on first entry. Called by the six
command handlers 0x12000/0x1205A/0x12088/0x120B8/0x12134/0x121BA. Cluster of 1. [O]

#### 0x0001EDFA `irq_unlock_nest` — logic
Critical-section leave: decrements 0xFFF858DC and, at zero, passes the saved SR mask
(0xFFF858D8) back to 0x1E138. Pair of 0x1EDDC; same six callers. Cluster of 1. [O]

#### 0x0001EE14 `eeprom_tables_load` — logic
Copies six configuration blocks (sizes from table PTR 0x1EE64, src/dst pairs at 0x1EE68) into
RAM, then writes 0xFFFFA840 |= 0x2D00 and 0xFFFFAB00=0xB300 (FCU/data-flash window enable).
Called by 0x1380C. Cluster of 1. [I]

#### 0x0001EE6C `can_tx_restart_all` — logic
For a CAN controller in state 3 (0xFFF85A79[i] & 0x13 == 3): sets state bit 0x10 and copies the
TX-request word at ctl+0x20 to ctl+0x28 — reissues all pending transmissions. Cluster of 2
(+0x32188). [I]

#### 0x0001EE98 `can_rx_queue_flush` — logic
Flushes the queued-frame table at 0xFFF858E0 (count 0xFFF85AAC): per entry calls the dispatch
callback (resolved: 0x1D758 CAN rx handler) with index/id/data pointers, then zeroes the
count. callers='-' (invoked via pointer). Cluster of 1. [I]

#### 0x0001EEE4 `eeprom_program_engine` — logic
Data-flash erase/program state machine over struct PTR 0x1F0DC: sub-state 1 erase (FCU 0x20 …
0xD0 per block via 0x1F2EE), sub-state 2 program (0xE8 + length + data words from mirror
PTR 0x1F0E4 + 0xD0), walking block addresses backwards with 0x1E918; verification compares the
block against the mirror. Runs with IRQ mask 15 (0x1E138). Called by the four session
functions 0x168D0/0x16DC0/0x178C0/0x17B50. Cluster of 1. [O]

#### 0x0001F07A `eeprom_erase_request` — logic
Queues an erase: if engine idle (state<4, sub 0), sets sub-state 1, last-block = param_1+
param_2-1 at struct+4/5 and target address = 0x1E918(block,0) at +0xC. Cluster of 1. [I]

#### 0x0001F0B4 `eeprom_write_request` — logic
Queues a program: sub-state 2; splits the offset into 8 KB block/remainder (param_2 += 0xE000
while > 0x1FFF), length 0x80 (or 8 when param_4==1) at +5, target 0x1E918(block,offset) at
+0xC, and copies len/4 words from param_3 into the buffer at *(PTR+4). Cluster of 1. [O]

#### 0x0001F13C `flash_blank_check` — logic
Blank-checks a code-flash address (param_1 & ~7): issues FCU commands 0x71, 0xD0 at the
window address, polls 0xFFFFA900 bit 7 (0x4B0 retries), passes if bits 0x40/0x1F clear; or,
when flag 0xFFF85A73.5 is set, compares 4 words against 0xFFFF directly. On timeout clears
FENTRYR and sets error state 0xFFF85792=1, 0xFFF8578C=6. Called by eleven session functions.
Cluster of 1. [I]

#### 0x0001F240 `flash_status_check` — logic
FCU status/error handler: reads 0xFFFFA900/0xFFFFA810; on the 0xFFFFA810 error latch retries
the status-clear sequence (A852=0xE1FF, A850=0x1EFF, A810=0, window cmd 0x50) up to 0xFF
times, else reports failure (0xFFF85792=1, 0xFFF8578C=6); optionally exits P/E mode
(FENTRYR=0xAA00). Returns the packed status bits. Cluster of 1. [I]

#### 0x0001F2EE `flash_pe_mode_enter` — logic
Selects the FCU window for the current block (struct+4 → bit at 0xFFFFA850/A852 with
0x1E00/0xE100 masks), enters P/E mode (FENTRYR 0xFFFFA902 = 0xAA80), runs the status check
0x1F240 and writes 0xFFFFA904=0x5501. Cluster of 1. [I]

#### 0x0001F354 `lib_swap16_bytes` — library
Byte-swaps each 16-bit halfword in the buffer (n/2 iterations, swaps pairs). Cluster of 3
(+0x333D8, 0x9A3BC). [O]

#### 0x0001F36C `lib_swap32_bytes` — library
Byte-reverses each 32-bit word in the buffer (n/4 iterations, swaps 0↔3, 1↔2). Cluster of 3
(+0x333F0, 0x9A3D6). [O]

#### 0x0001F394 `boot_init_table_run` — dispatch
Walks a 0x0C-byte init-record table (start PTR 0x1F3B4, end PTR 0x1F3B8), calling the function
pointer at record+4 until NULL or end — the bootloader's static-constructor/init-list runner.
Called by early init 0x10644. Cluster of 1. [O]

#### 0x0001F3CC `tbl_call_indexed` — dispatch
Two-instruction thunk: tail-calls the entry PTR_LAB_0001f3d8[index] (jump table indexed by the
incoming r0). Called by session functions 0x14ACC and 0x15086. Cluster of 1. [O]

#### 0x0001F464 `lib_memcpy4` — library
Optimised byte copy (len in r0, dst r1, src r2): copies up to 4 bytes per iteration with early
exit at the exact end. Used by the session/flash functions (0x16DC0, 0x1BBA0, 0x1C60C).
Cluster of 3 (+0x33444, 0x9A978). [O]

#### 0x00020554 `unknown_badinstr_20554` — unknown
Ghidra decompile truncated on bad instruction data; callers='-', no callees. Part of the
desynced 0x20500–0x21600 zone inside the boot-manager block (likely a data blob or
mis-disassembled stream). Cluster of 1. [O] (of the failure)

#### 0x00020780 `unknown_badinstr_20780` — unknown
Bad-instruction truncation; stores a register to *param_1 before halting. The application copy
0x44770 (r04_boot) decompiles identically broken — the same bytes exist in both blocks.
Cluster of 2 (+0x44770). [O]

#### 0x0002087E `unknown_badinstr_2087e` — unknown
Bad-instruction truncation; visible head calls through the vector base (TBR+0xD30) and then
0x2132C. Real purpose unrecoverable from this decompile. Cluster of 1. [O]

#### 0x00020ACC `unknown_badinstr_20acc` — unknown
Bad-instruction truncation; visible fragment writes param to a register-relative address and
conditionally sets bit 0x20 of a byte. Desync zone. Cluster of 1. [O]

#### 0x00020AF8 `unknown_badinstr_20af8` — unknown
Truncated on an unimplemented instruction; visible head computes a flag from SR, calls
func_0x2011a and stores two results. Desync zone. Cluster of 1. [O]

#### 0x00020E34 `unknown_badinstr_20e34` — unknown
Bad-instruction truncation; float-register noise (fr0/fr1/fr3 arithmetic) in the visible head —
the region may contain FP code fragments or mis-decoded immediates. Cluster of 1. [O]

#### 0x00020E56 `unknown_badinstr_20e56` — unknown
Same shape as 0x20E34 (float registers, register-relative stores) with bad-instruction
truncation. Cluster of 1. [O]

#### 0x00020E7A `unknown_badinstr_20e7a` — unknown
Bad-instruction truncation; head saves r11/r13 into param_5 fields and does float compares.
Cluster of 1. [O]

#### 0x00021006 `unknown_badinstr_21006` — unknown
Bad-instruction truncation; visible indirect call via table PTR_LAB_000217be_2+index.
Cluster of 1. [O]

#### 0x00021178 `unknown_badinstr_21178` — unknown
Two-instruction fragment (GBR-relative OR 0xCE) then bad data. Likely mid-instruction entry.
Cluster of 1. [O]

#### 0x000211A2 `unknown_badinstr_211a2` — unknown
Three-instruction fragment (GBR-relative AND 0xD1, OR 0xCE) then bad data. Cluster of 1. [O]

#### 0x000212D6 `unknown_badinstr_212d6` — unknown
Bad-instruction truncation; visible head byteswaps param_4 and calls func_0x21078.
Cluster of 1. [O]

#### 0x000212DC `unknown_badinstr_212dc` — unknown
Same pattern as 0x212D6 (byteswap then func_0x21078) truncated on bad data. Cluster of 1. [O]

#### 0x0002132C `unknown_badinstr_2132c` — unknown
Bad-instruction truncation; visible head stores a halfword, DAT_ffffff93 and tail-calls
code at 0x21E2F. Called (nominally) by 0x2087E. Cluster of 1. [O]

#### 0x0002136A `unknown_badinstr_2136a` — unknown
Truncated on unimplemented instructions; head writes GBR+0x200 and r14-relative byte, calls
0x220B4/0x20930. Cluster of 1. [O]

#### 0x0002137C `unknown_badinstr_2137c` — unknown
Bad-instruction truncation; head stores a halfword register and calls 0x204F6. Member
0x21384 lies 8 bytes behind — overlapping fragments of one desynced stream, not two real
functions. Cluster of 2 (+0x21384). [O]

#### 0x000213A0 `unknown_badinstr_213a0` — unknown
Two-instruction fragment (GBR+0x280 store, XOR 0xED) then bad data; member 0x213A4 overlaps it
by 4 bytes. Cluster of 2 (+0x213A4). [O]

#### 0x000213BE `unknown_badinstr_213be` — unknown
Partially decoded: GBR-relative OR 0x56, register saves, a callback with argument 0x62/0xB0
(cmp against 0x0000A70E), a call through PTR_FUN_0002238c, then an infinite loop — the
decompiler never resynchronises. Cluster of 1. [O]

#### 0x000213E6 `unknown_badinstr_213e6` — unknown
Bad-instruction truncation; head writes a byte, stores param_3, calls 0x20B4C with
LAB_0x21440. Cluster of 1. [O]

#### 0x00021574 `unknown_badinstr_21574` — unknown
Bad-instruction truncation; two register-relative stores visible. Cluster of 1. [O]

#### 0x00021596 `unknown_badinstr_21596` — unknown
Bad-instruction truncation; visible self-relative indirect call (FUN_00021596+index+4).
Cluster of 1. [O]

#### 0x000215F8 `unknown_badinstr_215f8` — unknown
Bad-instruction truncation; head stores params, clears 0x37600 and tail-calls the literal
address 0x3CAB0. Called (nominally) by 0x2091C. Cluster of 1. [O]

#### 0x000216C4 `unknown_badinstr_216c4` — unknown
Two-instruction fragment (register save) then bad data; desync zone. Cluster of 1. [O]

#### 0x000216CE `unknown_badinstr_216ce` — unknown
Two-instruction fragment (register save) then bad data; sibling of 0x216C4. Cluster of 1. [O]

#### 0x000219AC `unknown_badinstr_219ac` — unknown
Bad-instruction truncation; visible head branches on SR bit 0 only. Cluster of 1. [O]

#### 0x00021AE8 `unknown_badinstr_21ae8` — unknown
Bad-instruction truncation; visible call via table at 0x21AEC+index. Cluster of 1. [O]

#### 0x00021B26 `unknown_badinstr_21b26` — unknown
Bad-instruction truncation; noisy register saves (references LAB_00042000, DAT_000080FF) —
likely a literal pool decoded as code. Cluster of 1. [O]

#### 0x00021BA2 `unknown_badinstr_21ba2` — unknown
Bad-instruction truncation; repeated byte-store noise then stack-frame teardown. Cluster of 1.
[O]

#### 0x00021BFA `unknown_badinstr_21bfa` — unknown
Bad-instruction truncation; register saves + 0xFF store then halt. Member 0x21C1A is the same
pattern 0x20 bytes later — overlapping fragments. Cluster of 2 (+0x21C1A). [O]

#### 0x00021EA6 `unknown_badinstr_21ea6` — unknown
Bad-instruction truncation; visible head calls through the vector table (TBR+0xA70,
TBR+0xE70) and stores results — real call pattern, body unrecoverable. Cluster of 1. [O]

#### 0x00022224 `bootmgr_init_chain_run` — dispatch
Boot-manager init driver: clears 0xFFFE1200 bit 0 and 0xFFFE6000, sets 0xFFFE080E=0xE000 and
0xFFFE0400 |= 0x3C00, then invokes ~20 init hooks from the pointer table at 0x222C4–0x22304
(including 0x22444 RAM init, 0x2256C, 0x2264A handover consume), passing 0xFFF8F3C0 to the
first; ends in an infinite loop. Called by 0x24A0C/0x24A26/0x24B70. Cluster of 1. [O]

#### 0x0002230A `bootmgr_clock_mode_init` — logic
Clock-mode-dependent setup: on hook result 0 selects constant 0xB0 or 0x62 from MD_CLKP
(0xFFFF1000 bit 2, spec 05 §1) and calls the setup hooks at 0x22388/0x2238C, else the alternate
hook 0x22390; never returns (infinite loop). Called by 0x24144. Cluster of 1. [I]

#### 0x00022358 `boot_reset_reason_get` — data-accessor
Checks the RAM boot marker at 0xFFF800E0 against magic 0xA835 and returns the reason byte at
0xFFF800E6 (0 if no match); always clears *param_1. Called by 0x31530 (boot-manager copy
region). Cluster of 1. [I]

#### 0x000223C0 `bootmgr_ram_test` — logic
Power-on RAM test: writes a pattern from PTR 0x224BC into the first word of the four RAM banks
(0xFFF80000 + 0x1000·n), verifies readback, restores; a mismatch stores the failing address at
0xFFF8411C and calls the error reporter with code 0x211. Cluster of 1. [O]

#### 0x00022444 `bootmgr_ram_init` — logic
Startup RAM preparation: clears 0xFFF80000–0xFFF800FF and the 0xFFF84000 handover page via
0x2242C, copies the template from ROM 0x0000A700 to 0xFFF84100, runs the RAM test (0x223C0)
and then copies the word-block init list (0x0C-byte entries: src/dst/end from table PTR
0x224C8) — BSS/data-section init. Called from the 0x22224 chain. Cluster of 1. [O]

#### 0x0002246A `bootmgr_ram_init_warm` — logic
Variant of 0x22444's tail (template copy 0xA700 → RAM, RAM test, init-list copy loop) with the
block table held in callee-saved registers (decompiler shows unaff_r11/r12/r14) and an extra
callee 0x248FC; callers='-' (pointer-invoked). Cluster of 1. [I]

#### 0x000224D4 `bootmgr_startup_checks` — logic
Early self-checks: probes module-start registers 0xFFFFD000/0xD800/0xE000 through helper
0x2255C, stores the error mask in 0xFFF84DDC byte 0 (report 0x219 on failure), then maps the
flash window (0xFFFFA854=0xC401) and reads the descriptor via helper 0x22568 (length 0x2000,
or 0x1800 when flag 0xFFF852C7.0 set) into 0xFFF84120 (report 0x218 on failure; result flag in
0xFFF84DDC byte 1). Cluster of 1. [I]

#### 0x0002256C `bootmgr_clear_boot_flags` — logic
Clears 0xFFF84DFC byte 1, 0xFFF88BF6 and the byte behind PTR 0x225CC, then invokes hook
0x225D0 — resets the mode-notification state consumed by 0x2258E. Cluster of 1. [I]

#### 0x0002258E `bootmgr_mode_change_notify` — logic
When the mode byte 0xFFF84DFC.1 differs from 0xFFF88BF6 (and is not 0/3): copies it to
0xFFF88BF6 and *0x225CC and calls hooks 0x225D4(mode,2) / 0x225D8(result, mode, 2) —
propagates a boot/power mode change with event class 2. Called by 0x24974. Cluster of 1. [I]

#### 0x000225DC `bootmgr_handover_build` — logic
Builds the RAM handover record at PTR 0x227FC: copies up to 10 identifier bytes from
0xFFF8412C into 0xFFF800B7[1..] (and 0xFFF84137 to 0xFFF800B7[0]), sets count at +0x36, calls
hooks 0x22800 and (conditionally, 0xFFF84158) 0x22804, writes flags 2/2 at +4/+5 and the magic
trailer 0x5AA5@+0x44, 0xA55A@+0, size 0x48@+2. Called by 0x27180. Cluster of 1. [I]

#### 0x0002264A `bootmgr_handover_consume` — logic
Validates the handover record (magic 0x5AA5 at +0x44, 0xA55A at +0, size 0x48, versions 2/3
and 2 at +4/+5) and clears those magic/size words — consume-once handshake with 0x225DC.
Cluster of 1. [O]

#### 0x00022860 `bootmgr_diag_object_init` — logic
Initialises one 9-byte diagnostic object (slots 0xFFF88803/0F/1B): on hook success sets [0]=0,
[1]=0x26; for the 0xFFF8881B variant copies a 3-byte ID from ROM 0x7D00, otherwise up to three
bytes from the application-header pointer chain at PTR 0x40010+0x20 (+0x10/+0x14/+0x18) or 0xFF
on failure. Cluster of 1. [I]

#### 0x000228E2 `bootmgr_diag_objects_init` — logic
Runs 0x22860 over the three diagnostic slots and, when 0xFFF8B108-0x8B10D are all zero, fills
all six bytes from the same application-header chain (0x40010+0x20, +0x10..+0x24).
callers='-' (pointer-invoked). Cluster of 1. [I]

#### 0x0002296A `flash_header_check_wrap` — logic
Wrapper: calls the flash header validator (pointer 0x22A94; resolved callee 0x24FD0 — the
block-header acceptance check of spec 06 §2) on the block descriptor PTR 0x22A90 and returns
(result == 0). Cluster of 1. [I]

#### 0x0002297C `bootmgr_ram_marker_check` — logic
Returns (sum of the eight bytes 0xFFF8E0FC–0xFFF8E0FF and 0xFFF8E000–0xFFF8E003) == 0x324 — a
signature check over the head/tail of the 0xFFF8E000 buffer. Cluster of 1. [O]

#### 0x000229C0 `bootmgr_config_invalid` — logic
Combines the two validity checks: returns true when the 0xFFF8E000 signature (0x2297C) or the
structure/CRC validation 0x229D4 fails. Called by 0x2F3F0. Cluster of 1. [O]

#### 0x000229D4 `bootmgr_config_crc_validate` — logic
Deep validation of the 0xFFF8E000–0xFFF8E2FF structure: every pointer taken from the
descriptor chain (param_1 = {base, offset}, entries at +0x10) must lie inside the buffer,
lengths ≤ 0x500, then two CRC verifications through hook 0x22C08 over the ranges behind +0x0C
and +0x04. Returns 0 on any violation. [I] (buffer role — likely the protection record
loaded from flash, spec 06 §1 — is inferred). Cluster of 1.

#### 0x00022C10 `diag_set_service_table` — data-accessor
Stores the diagnostic service-table pointer into *PTR_DAT_00022e30 (consumed by the dispatcher
0x22C1C and the tick loop 0x22DA4). Cluster of 1. [O]

#### 0x00022C1C `diag_dispatch_service` — dispatch
UDS service dispatcher of the boot manager: computes SID/sub-function/session masks from hooks
0x22E34/0x22E38/0x22E3C, walks the 0x14-byte handler entries of the installed table (match at
+0/+1, session mask +2, condition mask +4), gates on the permission callback
(*0xFFF8413C)+0x28, then calls the handler at +8 with its argument at +0x10. Unmatched requests
go to hook 0x22E48; session/state transitions use bytes 0xFFF8421E and *PTR 0x22E44 (states
1/2/3, session codes 0x11/0x12/'1'). Spec 15 cross-ref. Cluster of 1. [I]

#### 0x00022DA4 `diag_handlers_tick` — logic
Background tick: hook 0x22E50, hook 0x22E54(0xFFF800AC), 0x22DE2, then calls the +0x0C
callback of every service-table entry (count at table+1, 0x14 stride) when non-NULL.
callers='-' (invoked from a scheduler). Cluster of 1. [I]

#### 0x00022DF2 `diag_status_block_update` — logic
Fills the tester-visible status block at 0xFFF800A4: 0x77 ('w'), session state, and hook
results (0x22E58, 0x22E3C, 0x22E5C) plus 0xFFF84220, then invokes the session hook at
(*0xFFF8413C)+0x1C. Called by the handover builder 0x225DC, 0x2353E and 0x25F90. Matches the
RAM window named in spec README item 20. Cluster of 1. [I]

#### 0x00022E60 `diag_send_positive_response` — logic
Sends one byte (code + 0x40) ahead of the response buffer (len+1) through the TX callback at
*(PTR 0x23080)+0x14 — classic UDS positive-response convention. Cluster of 1. [O]

#### 0x00022E8E `diag_send_negative_response` — logic
Sends 0x7F + code (two bytes ahead of the buffer) through the same TX callback — UDS negative
response. Cluster of 1. [O]

#### 0x00022EBC `diag_negresp_build` — logic
Builds the 0x7F frame at PTR 0x23084: [0]=0x7F, [1]=code, [2]=sub; 4-byte form with an extra
copy of **(PTR 0x23080) when code==0x80 and (PTR 0x23080)+6 != 0. Returns the length 3/4.
Cluster of 1. [O]

#### 0x00022EEE `diag_negresp_send_session` — logic
Builds (0x22EBC) a negative response from 0xFFF84137/0xFFF84DF5 and sends it via
*DAT_fff84140(buf, len, 0, 0, 3). callers='-'. Cluster of 1. [O]

#### 0x00022F20 `diag_negresp_send_svc` — logic
Builds a negative response (code 0xFFF84DEC, sub 0x10) and sends it through the callback chain
at PTR 0x23088 (+4 call, +8 arg, +0x10 len). callers='-'. Cluster of 1. [O]

#### 0x00022F4C `diag_negresp_send_pending` — logic
Sends the pending negative response: code from 0xFFF84137 or 0xFFF84DEC (selected by
0xFFF84DE8), sub 0xFFF84DF6, via *DAT_fff84140 with the 0xFFF84DE0/DE4 argument pair and class
9. callers='-'. Cluster of 1. [O]

#### 0x00022F90 `diag_send_buffered_resp` — logic
Copies the buffered response bytes at 0xFFF800B7 (count 0xFFF800B6; filled by the handover
builder 0x225DC) into the TX buffer and sends them; flags class 3 when the buffer starts with
0x7F and the sub-function matches 0xFFF84DF5. callers='-'. Cluster of 1. [I]

#### 0x00022FF4 `diag_state_reset` — logic
Resets the diagnostic engine: handler pointer 0xFFF84148 from table PTR 0x2308C, zeroes
0xFFF8414C, 0xFFF84DED, 0xFFF84DEE, 0xFFF84E0A, 0xFFF84DF2. Cluster of 1. [O]

#### 0x00023014 `diag_error_state_enter` — logic
Resets diag state (0x22FF4), notifies the TX callback with zero payload and stores param_1 at
0xFFF84154 ([I] an error/timer code: called with 1/2/3/4). Cluster of 1. [I]

#### 0x0002303E `diag_response_dispatch` — logic
Response-completion dispatcher: switches on the status byte 0xFFF84136 (set by hook 0x23090;
cases 1-11) and routes to positive (0x22E60), negative (0x22E8E) or raw TX callback, updating
the next-handler pointer at 0xFFF84148 from the tables at 0x232C4-0x232D4; cases 10/0xB set
0xFFF84DF2/0xFFF84DEE. Jump data at 0x23094. Called by 0x2327E. Cluster of 1. [I]

#### 0x0002327E `diag_request_pump` — logic
Session pump: edge-detects a new request (0xFFF8421D with 0xFFF8421C/0xFFF84DF1), else copies
SID/sub from the request buffer 0xFFF84214 into 0xFFF84137/138, sets the payload pointer
0xFFF8412C, lengths via hooks, and runs the response dispatcher 0x2303E. Called by 0x23324.
Cluster of 1. [O]

#### 0x00023324 `diag_handler_tick` — logic
When a request is pending (0xFFF8421D != 0), installs the handler from PTR 0x23498 into
0xFFF84148 and calls it. callers='-'. Cluster of 1. [I]

#### 0x00023368 `diag_completion_pump` — logic
Post-handler pump: hook 0x234A0, then switch on the result code (0, 7, 8, 9, 10, 0xB): errors
with 0xFFF84DED/0xFFF84DEE set enter the error state 0x23014 (codes 3/4); other codes update
0xFFF84148 from table 0x234A4. callers='-'. Cluster of 1. [I]

#### 0x000234D8 `cb_thunk_via_23678` — stub
Tail-call through the data pointer DAT_00023678 (resolved callee 0x25C32). Called by 0x23EDC.
Cluster of 1. [O]

#### 0x00023514 `diag_service_table_install` — stub
Installs the service table at DAT_00023698 via thunk to 0x22C10. Called by 0x294E0. Cluster of
1. [O]

#### 0x0002352E `diag_session_get` — data-accessor
Returns the diagnostic session byte 0xFFF8421E. callers='-'. Cluster of 1. [O]

#### 0x00023536 `diag_state_arg_set` — data-accessor
Stores param_1 into 0xFFF841F8 (diagnostic context slot). Called by 0x23F56. Cluster of 1. [O]

#### 0x0002360A `bootmgr_dispatch_event` — logic
Event pump: thunk 0x25C08, hook 0x236B4(value, 0), early-out hook 0x236B8, then maps the value
through 0x25334/0x2534A to a function pointer (jumptable at 0x2536C unrecovered) and calls it.
callers='-'. Cluster of 1. [I]

#### 0x0002362C `diag_session_write_active` — data-accessor
Returns 1 when the session/handover struct at PTR 0x236A8 has flags +5 == 2 and the marker
+0x24 == 'w' (0x77, same marker written by 0x22DF2) — [I] "flash/write session established".
Called by 0x28378. Cluster of 1. [I]

#### 0x00023644 `diag_session_invalidate` — logic
Clears the same struct: words +0/+2 and +0x44 to 0, bytes +4/+5 to 0xFF, marker +0x24 to 0 —
invalidates the session record. callers='-'. Cluster of 1. [O]

#### 0x000238E8 `diag_isotp_frame_process` — logic
ISO-15765-2 (ISO-TP) frame engine, 633 instructions: parses the PCI high nibble (0=Single
Frame, 1=First Frame with 12-bit length, 2=Consecutive Frame with 0x0F-wrapped sequence check
against +0x1E), validates against the per-message config tables behind 0xFFF84224 (0x0C-stride
descriptor entries, 0x28-byte state records, 0x10-byte handler triples), copies payloads
(thunk 0x263A0), stamps timers from the FRC at 0xFFFFF220 (spec 02 §2) and reports NRC-style
codes 4/5/6/7/8/9/0xB/0xC/0xD/0xF through the +0x0C callbacks. callers='-'. Cluster of 1. [O]

#### 0x00023E98 `diag_comm_state_check` — logic
Gated variant dispatch: (param_1, param_2) selects one of four communication-state checks
(0x23EE2/0x23EE6/0x23EF6/0x23EEA); on zero result (failure) calls hook 0x240E4(param_2) and the
link-status update 0x23F56. Called by 0x28232 and 0x2360A. Cluster of 2 (+0x75DE0 app copy).
[I]

#### 0x00023EDC `cb_thunk_via_240e8` — stub
Three-instruction thunk calling (*PTR_FUN_000240e8)(0). Called by 0x23EF6/0x23EEA. Cluster of 3
(+0x75F6C, 0xA1026 copies). [O]

#### 0x00023F02 `diag_build_timing_params` — logic
Writes the UDS timing-parameters response: **PTR 0x240EC / 1000 (clamped 0xFFFF) as two bytes
and *(PTR+4)/10000 as two more — P2/P2* server timing in ms; sets response length 5. Called by
0x23FA4. Cluster of 1. [I]

#### 0x00023F56 `diag_link_status_update` — logic
Gathers the link counter block (via (*0xFFF84140)+0x20(1) and PTR 0x240EC), runs the check
callback at (*0xFFF84140)+0x18, and on failure the update hook (+0x1C) plus DAT_000240F0 with
the second counter. Cluster of 1. [I]

#### 0x00023FA4 `diag_build_pending_response` — logic
Builds the pending response in the shared buffer 0xFFF800B5: length = table length+1,
SID|0x40 at 0xFFF800B7, subfunction at 0xFFF800B8 and up to 8 payload bytes from *param_1+1;
gated on bit 7 of the +0xC/+0xE bytes; finishes via 0x2353E. callers='-'. Cluster of 1. [I]

#### 0x00024016 `diag_build_short_response` — logic
Short variant: length from +6, SID at +0xB and sub at +0xC into 0xFFF800B5/B6/B7/B8, then the
0x2353E update. callers='-'. Cluster of 1. [I]

#### 0x0002410C `diag_link_state_3_check` — logic
Returns 5 when the link state thunk 0x25C08 reads 3, else 0. callers='-'. Cluster of 1. [O]

#### 0x00024144 `bootmgr_clock_init_wrap` — stub
Wrapper invoking the clock-mode init 0x2230A through DAT_00024254. Called from six init paths
(0x223C0, 0x224D4, 0x24A26, 0x30914, 0x30FAC, 0x31008). Cluster of 1. [O]

#### 0x00024260 `boot_mode_pin_40_check` — logic
One-shot read of mode-register bit 0xFFFF1001.6 (hardware strap): returns the bit state and
latches 0xFFF84DF9 so it is sampled only once. Called by 0x2230A and two boot-manager copies.
Cluster of 1. [O]

#### 0x000243FC `lib_memcpy_aligned` — library
Alignment-aware copy: splits the transfer into byte head/tail (thunks 0x243E6/0x243F6) around
an aligned word middle. The boot manager's general memcpy, used from 13 sites. Cluster of 1.
[O]

#### 0x0002447C `flash_protect_range_check` — logic
Checks a flash address range [param_4, param_4+param_5) against a 0x0C-byte-entry protection
table (start/end/mask/flag): mode 1 requires full coverage with consistent flags (mixed → 2,
mask mismatch → 3, size overflow → 4); mode 2 tests any overlap with flag bits. 0 = allowed.
Cluster of 1. [I]

#### 0x000245EE `flash_access_check_log` — logic
Top-level flash access gate: runs 0x2447C over the primary protection table (PTR 0x246F0) and,
if present, the secondary table (PTR 0x246F8); special-cases ranges inside 0x5FFFFF–0x680000
(boot-data marker area) allowed only when param_4 == 0xFFF88BFC; records the request (type,
addr, length, verdict, DAT_ffffab00 bit) into the two-slot log at 0xFFF88840 and returns the
status. callers='-' (service-invoked). Spec 06 §1 protection record cross-ref. Cluster of 1.
[I]

#### 0x000247A4 `bootmgr_rambus_test` — logic
Classic RAM bus test over the block [param_1, +param_2): all-zero, all-ones, a walking data
pattern (constant DAT_0002482C written byte-wise and verified) and an address-aliasing check;
returns the failing address or 0. Called by the startup checks 0x224D4. Cluster of 1. [O]

#### 0x00024830 `bootmgr_ram_pattern_test` — logic
Pattern test over the 32-entry × 0x10 table at param_1+0x100: fill 0xFFFF, verify, fill
DAT_000248F8, verify, clear; busy flags at param_1+0x300/+0x301; returns the failing cell or 0.
Called by 0x224D4. Cluster of 1. [O]

#### 0x0002493E `bootmgr_mode_cb_get` — data-accessor
Returns the callback pointer (first word) from the 4×3 mode/class table at PTR 0x24970 when
param_1<4 and param_2<3 and the entry is non-NULL, else 0. Cluster of 1. [O]

#### 0x00024974 `bootmgr_main_loop` — entry
Boot-manager scheduler loop (never returns): initial hooks (0x249B8/BC/C0), then forever:
increment the loop counter at 0xFFF84D92, read the mode byte 0xFFF84DFC.0, invoke the
mode/class callbacks (hooks 0x249B8/BC/C4 — resolving through 0x2493E/0x2258E/0x249C8) and
0x270BC. Reached from the init chain via 0x2256C's hook. Cluster of 1. [O]

#### 0x000249C8 `bootmgr_mode_cbs_run` — logic
Fetches the value for (mode param_2, class param_3) from table PTR 0x24A04 via hook 0x24A08,
then calls every function pointer in the param_1 array (0-terminated), refreshing the value
between calls. Cluster of 1. [I]

#### 0x00024A0C `bootmgr_restart` — logic
Boot-manager restart: copies the 0x34-byte restart record from param_1 to PTR 0x24B2C
(aligned memcpy), passes it to hook 0x24B30, clears the handover flag byte 0xFFF8403E and
runs the init chain 0x22224 — exactly the restart behaviour of spec 06 §5. callers='-'
(pointer-invoked). Cluster of 1. [O]

#### 0x00024A26 `bootmgr_handover_entry` — logic
Application→boot-manager handover entry (called from the application at 0x72964): if the RAM
session record 0xFFF80080 is valid (magic 0xA55A/0x5AA5, size 0x48), type 0x85==2 → optionally
0x25416(0x24B2C, 0x100, 0) when 0x84==3, sets 0xFFF8403E=1, 0x25404 and the init chain; type
1 → hook 0x24B38. If instead the record at 0xFFF80000 is valid (size 0x14): hook 0x24B40
(0xFFF8F400, ROM 0x5AA4, 0xFFF800C4), A-DMAC off, ten-hook init, then the flash-write engine
0x24B70 with (0xFFF80004, 0xFFF80008, 0xFFF8000C) and an endless poll. Fallback: clock wrapper
0x24144(0x2F0). Specs 06 §5 / 15 cross-ref. Cluster of 1. [I]

#### 0x00024B10 `chk_min_version` — logic
Writes 0 or 0xFFF1 into *param_1 depending on whether the ROM byte behind PTR 0x24B6C is below
param_2 — a minimum-version check with error code 0xFFF1. Called by bootloader 0x190CC.
Cluster of 1. [I]

#### 0x00024B26 `chk_result_zero` — stub
Stores 0 into *param_1 (success-assignment). Called by 0x190CC. Cluster of 1. [O]

#### 0x00024B70 `flash_write_engine` — logic
Code-flash programming engine (209 instr): validates dest < 0x200000 and range fit, then a
state machine 0→8 — start, erase (0x24D64), clear-check (0x24D68), program in 0x800-byte
chunks (0x24D74) with progress ((done*8)/(total+1)) published to 0xFFF88BE8 and mirrored to
peripheral 0xFFFC0D04, final verify (0x24D78); status flags in the record at PTR 0x24D60.
Invoked by 0x24A26 with the RAM request triple. Spec 16/17 cross-ref. Cluster of 1. [O]

#### 0x00024D7C `flash_blockdesc_validate` — logic
Validates the boot's logical block descriptor (like 0x229D4 but for the flash map): every
chained pointer must lie in 0x6000–0x1FFFFF 4-aligned, sizes ≤ 0x500, then two CRC verifications
via 0x250CC over the ranges behind +0x0C and +0x04. Called by 0x25140/0x2526C/0x252A4. Spec 06
§1 cross-ref. Cluster of 1. [I]

#### 0x00024FD0 `flash_header_validate` — logic
Block-header acceptance check (spec 06 §2, exact): word+0 must match 0x876543xx (mask constant
at 0x25138), the end pointer at +0x0C must lie in 0x6000–0x1FFFFF and be 4-aligned, and the
word at that end address must equal DAT_0002513C (0x12345678). Called by 0x2296A and four
0x251xx/0x252xx validators. Cluster of 1. [O]

#### 0x00025036 `flash_marker_check` — logic
Cross-block marker check: validates both descriptor pointers (0x6000–0x1FFFFF, aligned) and
compares the 4 bytes at descriptor+0x0C-relative addresses between the two block views via
0x25102. Called by 0x2519A/0x251AE/0x2527E/0x252B6. Cluster of 1. [I]

#### 0x000250CC `flash_fill_check` — library
Verifies that param_4 bytes at param_3 all equal param_2 (0xFF = erased / 0x00 = cleared
checks), writing the matched length to *param_5; 1 = uniform. Used by the block validators.
Cluster of 1. [O]

#### 0x00025102 `flash_range_compare` — library
Verifies that param_4 bytes at param_2 equal the bytes at param_3, writing the matched length
to *param_5. Cluster of 1. [O]

#### 0x00025140 `flash_markers_state_check` — logic
Checks the marker pair of the block behind PTR 0x2524C+4: +0x7F00 must be uniformly 0xFF and
+0x7E00 uniformly 0x00 (256 B each — the marker convention of spec 06 §3), then runs hook
0x25254 over PTR 0x25250; returns (hook result == 0). Called by 0x25212. Cluster of 1. [I]

#### 0x0002519A `flash_marker_check_a` — logic
Compares marker bytes between the descriptor pairs (PTR 0x25258, PTR 0x2524C) via hook 0x2525C
(resolved 0x25036); returns (result == 0). Called by 0x251C2. Cluster of 1. [I]

#### 0x000251AE `flash_marker_check_b` — logic
Same check for the pair (PTR 0x2524C, PTR 0x25250). Called by 0x251C2. Cluster of 1. [I]

#### 0x000251D8 `flash_header_bad_desc1` — logic
Runs the header validator (hook 0x25260 → 0x24FD0) on descriptor PTR 0x2524C and returns
(result == 0) — true when the header is NOT acceptable. Called by 0x251FC. Cluster of 1. [O]

#### 0x000251EA `flash_header_bad_desc2` — logic
Same for descriptor PTR 0x25250. Called by 0x251FC. Cluster of 1. [O]

#### 0x00025212 `flash_blocks_health_check` — logic
Cascaded block health check: markers (0x25140) → headers (0x251FC) → marker cross-check
(0x251C2); returns the first non-zero verdict. Called by 0x27180. Cluster of 1. [O]

#### 0x00025232 `flash_block_field_4c_get` — data-accessor
Returns the +0x4C word of the block record behind (PTR 0x25268 + *PTR 0x25264 + 0x10) when
param_1 == 0, else 0. Called by 0x27180. Cluster of 1. [O]

#### 0x0002526C `flash_blockdesc_bad_a` — logic
Runs the block-descriptor validator (hook 0x254A8 → 0x24D7C) on PTR 0x254A4; returns
(result == 0). Called by 0x252F2. Cluster of 2 (+0x25292: same wrapper over hook 0x254B4 →
0x24FD0-style check). [O]

#### 0x0002527E `flash_marker_check_c` — logic
Marker check (hook 0x254B0 → 0x25036) for the pair (PTR 0x254AC, PTR 0x254A4). Called by
0x252DC. Cluster of 1. [O]

#### 0x000252A4 `flash_blockdesc_bad_b` — logic
Block-descriptor validation (0x24D7C) on PTR 0x254B8; returns (result == 0). Called by 0x2531E.
Cluster of 2 (+0x252CA: same wrapper over hook 0x254B4). [O]

#### 0x000252B6 `flash_marker_check_d` — logic
Marker check for the pair (PTR 0x254A4, PTR 0x254B8). Called by 0x25308. Cluster of 1. [O]

#### 0x0002534A `flash_block_field_28_get` — data-accessor
Returns the +0x28 word of the block record behind (*PTR 0x254A4 + 0x10) when param_1 == 0.
Called by 0x2535C (the dispatcher used by 0x2360A). Cluster of 1. [O]

#### 0x00025374 `bootmgr_app_hook_call` — logic
Invokes the application-side callback at PTR_PTR_00004010+0x14 (resolved 0x105A6 — application
code) when non-NULL; part of the handover path from 0x24A26. Cluster of 1. [I]

#### 0x00025386 `lib_copy_tail_bounded` — library
Bounds-checked copy of param_6 bytes from the tail of one buffer into the tail of another
(offsets param_5/param_4); returns 1 on success. Called by 0x2F0A0/0x2F17C. Cluster of 1. [O]

#### 0x000253BE `bootmgr_request_record_build` — logic
Builds the RAM request record at 0xFFF80014: 0x25404() first, then param_1[0..7] → 0xFFF80018,
param_1[8..15] → 0xFFF80020, and the magic words 0xA55A@0xFFF80014, 0x5AA5@0xFFF80030, size
0x1E@0xFFF80016. Called by the restart path 0x24A0C. Cluster of 1. [O]

#### 0x00025404 `bootmgr_blockid_copy` — logic
Copies the 8-byte block-ID string "bjix2201" (ROM 0x601A) into 0xFFF80028 — the boot-manager
block ID of spec 06 §1. Cluster of 1. [O]

#### 0x00025416 `lib_memset_aligned` — library
Alignment-aware memset: byte head/tail around replicated-word stores of param_3
(param_3*0x10101 … ×0x1000000 fill). Used by 0x24A26 and 0x27180. Cluster of 1. [O]

#### 0x00025520 `bootmgr_request_validate` — logic
Validates a 4-word address/length request against the window bounds 0xFFF841B8–0xFFF841C4 and
the start<end ordering; on success copies 5 words into PTR 0x2567C. Returns 1 out-of-range,
3 bad-order, 0 ok. callers='-'. Cluster of 1. [I]

#### 0x0002557E `bootmgr_request_set_8` — logic
Posts a request: stores param_1 into the mailbox byte at PTR_0x25678+1 and ORs 8 into the
mailbox flag byte — consumed by 0x2558A bit 8. callers='-'. Cluster of 1. [O]

#### 0x0002558A `bootmgr_request_pump` — logic
Mailbox processor over PTR 0x25678 flags: bit 1 copies the 5-word request (record 0x2567C →
struct 0x25674 +8/+0xC/+0x10/+0x14 and 0xFFF841F4); bit 2 sets 0xFFF84194 and calls
0x25C5C/0x25C6C(0); bit 8 selects the active address (0xFFF8419C / 0xFFF841F8 selector);
bit 4 invokes and clears the one-shot callback at record+0x24; bit 0x10 calls
(*(0xFFF84320+0xC)) and publishes record+0x1C to 0xFFF80098. callers='-'. Cluster of 1. [O]

#### 0x0002564E `timer_stamp_fff8418c` — data-accessor
Stores the FRC timebase 0xFFFFF220 (spec 02 §2) into 0xFFF8418C. Called by 0x26A78. Cluster of
1. [O]

#### 0x0002565C `diag_ctx_table_get` — data-accessor
Returns the +0x0C word of the 0x30-stride entry of the session table at 0xFFF84324 selected by
PTR_0x25674[0x30]. callers='-'. Cluster of 1. [O]

#### 0x00025A58 `diag_session_activate` — logic
Activates diagnostic session/service param_1 from the 0x30-stride table at 0xFFF84324:
publishes session ids to 0xFFF800A9/AA/B0, calls 0x25C5C(entry+8)/0x25C6C(1), loads the
address window via 0x25BAE, mirrors 0xFFF841A4-B4 into the tester block 0xFFF80088-98,
optionally fires hook 0x25BE0 (entry+0x2C), invokes the entry+0x18 handler, clears
0xFFF84DFC.3 and notifies all other sessions (count 0xFFF84E16) through 0x25BE4. callers='-'.
Cluster of 1. [I]

#### 0x00025B38 `diag_session_attach` — logic
Publishes the session entry (param_1): id to 0xFFF84E01, the two words of entry+0x24's table to
0xFFF84328/32C, installs the session table via 0x260FC, then hooks 0x25BE8/0x25BEC feed
0x26082 (session lookup) and 0xFFF800AE. callers='-'. Cluster of 1. [I]

#### 0x00025B8A `diag_session_publish` — logic
Head of 0x25B38 without the hook calls: publishes id, 0xFFF84328/32C and 0x260FC. callers='-'.
Cluster of 1. [I]

#### 0x00025BAE `diag_window_load` — logic
Loads the address window: *entry+0x1C → 0xFFF841A4/A8/B0 and *entry+0x20 → 0xFFF841B8/BC/C4,
resetting 0xFFF841AC/0xFFF841C0 and the 5000 limits at 0xFFF841B4/0xFFF841C8. Called by
0x25A58. Cluster of 1. [O]

#### 0x00025BF8 `diag_ctl0_get` — data-accessor
Returns the session byte 0xFFF84E00. Called by 0x22C1C. Cluster of 1. [O]

#### 0x00025C00 `diag_ctl4_set` — data-accessor
Stores param_1 into diag control byte 0xFFF84E04. Cluster of 1. [O]

#### 0x00025C08 `diag_ctl4_get` — data-accessor
Returns diag control byte 0xFFF84E04. Called by 0x22C1C, 0x22DF2, 0x234C6, 0x2410C. Cluster of
1. [O]

#### 0x00025C2C `diag_ptr_25dcc_get` — data-accessor
Returns *PTR_DAT_00025dcc. Called by 0x22C1C. Cluster of 1. [O]

#### 0x00025C44 `diag_ctl1_get` — data-accessor
Returns diag control byte 0xFFF84E01. Called by 0x22DF2, 0x234DE. Cluster of 1. [O]

#### 0x00025C5C `diag_ctl5_set` — data-accessor
Stores param_1 into 0xFFF84E05. Called by 0x25A58, 0x2558A, 0x283E2. Cluster of 1. [O]

#### 0x00025C64 `diag_ctl5_get` — data-accessor
Returns 0xFFF84E05. Called by 0x22DF2. Cluster of 1. [O]

#### 0x00025C6C `diag_ctl6_set` — data-accessor
Stores param_1 into 0xFFF84E06. Called by 0x25A58, 0x2558A, 0x283E2. Cluster of 1. [O]

#### 0x00025C74 `diag_ctl6_get` — data-accessor
Returns 0xFFF84E06. Called by 0x22C1C, 0x22DF2, 0x26028, 0x2604E. Cluster of 1. [O]

#### 0x00025DF4 `diag_comm_modules_scan` — logic
Scans the communication-module list (PTR 0x25F84: {list, count}): polls each module's entry
callback; result 1 marks busy, result 2 marks error and fires the module+0x24 hook; tracks the
current module at PTR 0x25F88. Called by 0x25EB0. Cluster of 1. [I]

#### 0x00025E7C `diag_comm_modules_stop_all` — logic
Calls the +4 ("stop/abort") callback of every communication module in the list except the
current one (PTR 0x25F88+4). Called by 0x25EB0. Cluster of 1. [I]

#### 0x00025EB0 `diag_comm_pump` — logic
Communication state pump (callers='-'): state 1 polls the current module; state 2 verifies
completion via module+0x2C, then stops all modules (0x25E7C), fires the module+0x18 done hook
and the (*0xFFF84140+0x30) hook and resets 0x25C32/0x25C00; otherwise scans (0x25DF4).
Cluster of 1. [I]

#### 0x00025F90 `diag_pending_response_flush` — logic
Flushes a pending response: on hook 0x2601C()==0 with state 0xFFF8414C matching, copies 10
bytes from 0xFFF8412C-1 to 0xFFF800B7, sets lengths from 0xFFF84132 and triggers the TX
callback *0xFFF84140(0,0,0,0,0); state-1 path publishes only the status. Called by 0x22DF2,
0x23524. Cluster of 1. [I]

#### 0x00026028 `diag_rx_pending_check` — logic
Returns 1 when hook 0x2611C reads 2 and the received counter 0xFFF8008C is below the window
end 0xFFF841F4 — more data expected. Called by 0x2327E. Cluster of 1. [I]

#### 0x0002604E `diag_maxlen_clamp` — logic
Clamps param_1 to 6 (session 2/3) or 0xFE when the channel 0x25C74() reads 2 — maximum
response length selection. Called by 0x2327E. Cluster of 1. [I]

#### 0x00026082 `diag_session_lookup_set` — logic
Matches param_1 against the session-type table at 0xFFF8420C (2-byte entries: type, session;
count 0xFFF84E0B, with override byte 0xFFF84E07 via 0x2610A) and on match sets the session
0xFFF8421E, type 0xFFF8421F and 0xFFF84220. Called by 0x25B38. Cluster of 1. [O]

#### 0x000260FC `diag_session_table_set` — data-accessor
Installs the session table: 0xFFF8420C = pointer, 0xFFF84E0B = count. Called by 0x25B38/0x25B8A.
Cluster of 1. [O]

#### 0x0002610A `diag_override_type_match` — data-accessor
Returns (0xFFF84E07 == param_1) — session-override byte compare. Called by 0x26082. Cluster of
1. [O]

#### 0x00026120 `diag_comm_hooks_init` — logic
Fires three comm hooks (0x26188 with PTR 0x26184, 0x26190, 0x26194). Called by
0x28378/0x28400/0x28420. Cluster of 1. [I]

#### 0x00026134 `diag_build_status_response` — logic
Builds a 1-2 byte response with a code byte selected from the table behind PTR 0x26198 (+7 for
default, +8 for param_1==1, +9 for param_1==3); uses the two-byte form when sub-code 0x80 is
pending; marks class 2. Called by the service dispatcher 0x22C1C. Cluster of 1. [I]

#### 0x0002619C `diag_flags_ec_ed_set` — logic
Sets 0xFFF84E0C = 0 and 0xFFF84E0D = the state byte from 0x25C08. callers='-'. Cluster of 1.
[O]

#### 0x000261B8 `diag_isotp_config_install` — logic
Installs the ISO-TP configuration: 0xFFF84224 = table pointer, count from param_1+1, and
resets every 0x28-byte channel state record (bytes +0x18=1, +0x19/0x1A/0x1B/0x1C/0x1D/+0x23=0)
— the config consumed by 0x238E8. callers='-'. Cluster of 1. [O]

#### 0x000261F8 `diag_isotp_buf_set` — logic
Arms an ISO-TP channel buffer (0x28-byte records behind PTR 0x26418+0xC): stores pointer at
+0xC, length at +0x10 and the two address bytes at +0x1C/+0x1D; sets +0x18 flags 0x02 (or
0x82 | 0x20) by length, wrapped in the lock/unlock hooks 0x2641C/0x26420. Called by 0x28274.
Cluster of 1. [O]

#### 0x00026252 `diag_isotp_flags_clear` — logic
Clears the given bits from the channel's +0x18 flag byte and returns the previous value.
Called by 0x28294. Cluster of 1. [O]

#### 0x00026280 `diag_isotp_state_set` — logic
Sets the channel state nibble (+0x18 low nibble = param_2) and clears +0x19/+0x1A. Called by
0x263DE, 0x2661E, 0x238E8, 0x282AA. Cluster of 1. [O]

#### 0x000262B0 `diag_isotp_bit40_take` — logic
Reads the channel flag byte +0x18, clears bit 0x40 and returns whether it was set
(test-and-clear). Called by 0x282B8. Cluster of 1. [O]

#### 0x000262DA `diag_isotp_count14_get` — data-accessor
Returns the halfword at channel+0x14. Called by 0x282CC. Cluster of 1. [O]

#### 0x000262EE `diag_isotp_bufptr_get` — data-accessor
Returns the buffer pointer at channel+0xC. Called by 0x282E0. Cluster of 1. [O]

#### 0x00026302 `diag_isotp_addr1c_get` — data-accessor
Returns the address byte at channel+0x1C. Cluster of 2 (+0x26316 returning +0x1D — adjacent
getter of the second address byte). [O]

#### 0x0002632C `diag_isotp_buf_swap` — logic
Swaps the TX buffer: copies the old buffer out (thunk 0x263A0), stores the new pointer/length
(+0xC/+0x10), clears bit 3 and — when the config entry (+0x17 bit 1) requires it — re-arms the
0x40 state and the FRC timer (0xFFFFF220) at +4/+8. Called by 0x28308. Cluster of 1. [O]

#### 0x000263A8 `diag_isotp_word0_get` — data-accessor
Returns the first word (deadline timer) of the channel record. Called by 0x28326. Cluster of
1. [O]

#### 0x000263BA `diag_isotp_flag1b_set2` — logic
Sets channel+0x1B = 2 inside the lock hooks. Called by 0x2832C. Cluster of 1. [O]

#### 0x000263DE `diag_isotp_channel_reset` — logic
Resets the channel: clears +0x1B/+0x19/+0x1A, flag bit 0x20 and +0x23, then sets state 2 via
0x26280. Called by 0x2833C. Cluster of 1. [O]

#### 0x0002661E `diag_isotp_flowctrl_send` — logic
Sends the ISO-15765 Flow Control frame: FS byte 0x30 (CTS), 0x31 (wait, with STmin countdown
at +0x21) or 0x32 (overflow) selected by param_2, BS/STmin copied from the config entry
(+0x14/+0x15), deadline = config word + FRC 0xFFFFF220, transmitted (padded) through 0x26A46;
on success sets +0x19 state 0x80/1/0x40. Called by the RX engine 0x238E8. Cluster of 1. [O]

#### 0x0002678C `diag_isotp_cf_send` — logic
Consecutive-frame pump (249 instr): PCI = 0x20|SN with the sequence number at +0x1E incremented
mod 0x10; STmin from +0x20 (×1000 for <0x80, ×100 for 0xF0-0xF9, else 0x1F018 — the ISO-TP
STmin encoding); stashes overflow bytes in the 0xFFF84228/0xFFF8422F spare buffer; timers from
0xFFFFF220; sends padded via 0x26A46 and rolls back the state on failure. callers='-'. Cluster
of 1. [O]

#### 0x00026A46 `diag_isotp_frame_send_pad` — logic
Pads the frame to 8 bytes with the filler byte and passes it to the TX callback PTR 0x26ABC.
Used by both the FC and CF senders. Cluster of 1. [O]

#### 0x000251C2 `flash_check_pair_or` — logic
Two-step validity combine: runs the first check and, when it returns 0, the second; returns the
combined verdict. Cluster of 7 byte-equivalent copies distinguished only by their callee pairs:
0x251C2 (marker checks 0x2519A/0x251AE), 0x251FC (header checks 0x251D8/0x251EA), 0x252DC,
0x252F2, 0x25308, 0x2531E and 0x25334 (same OR-shape over their own check pairs). [O]

#### 0x0002535C `bootmgr_dispatch_event_2` — logic
Second event dispatcher: maps the value from 0x25334 through 0x2534A to a function pointer and
tail-calls it (jumptable at 0x2536C unrecovered); returns when the mapping yields 0. Companion
of 0x2360A. callers='-'. [I]

#### 0x00025C32 `diag_ctl_bits_set` — logic
Sets the diag control bit-mask: *PTR 0x25DD0 = 1<<param_1 (0 when param_1==0) and stores
param_1 into 0xFFF84E01. Called by 0x22DA4, 0x234D8, 0x25EB0. [O]

#### 0x00025C4C `diag_ctl_bits_get` — data-accessor
Returns the diag control bit-mask *PTR 0x25DD0. Called by 0x234E4. [O]

#### 0x00025C52 `diag_ctl_bits_test` — data-accessor
Returns whether the diag control bit-mask (*PTR 0x25DD0) contains param_1. Called by 0x234EA.
[O]

#### 0x00025C7C `diag_state_clear_full` — logic
Resets the diagnostic state: propagates 0xFFF800AB through 0x25C00 and clears the pointer
*PTR 0x25DCC. Called by 0x22DA4. [O]

## Unit summary

220 clusters, 267 functions named (rep + all members). Counts by kind (per function row):

| Kind | Count |
|---|---|
| logic | 167 |
| data-accessor | 28 |
| unknown | 34 |
| library | 24 |
| stub | 7 |
| dispatch | 4 |
| irq | 2 |
| entry | 1 |

**Composition of the slice.** 0x1DF92–0x1FFFF is the bootloader's tail: the boot-side CAN
driver family (RCAN mailbox load/program, rx dispatch through the handler table at 0xFFF8571C
with descriptors at PTR 0x1D7EC, tx confirm, profile selection between boot-data table 0x5632
and alternate 0x1E6E0) plus the data-flash (EEPROM) erase/program engine (0x1EEE4, FCU command
flow 0x20/0xE8/0xD0, block addressing 0x1E918 = 0x80100000 + block*0x2000) and code-flash
helpers (blank check, P/E mode entry, status/clear retry). 0x20000+ is the boot manager:
init chain (0x22224), RAM tests (0x223C0/0x247A4/0x24830), handover-record build/consume with
magics 0xA55A/0x5AA5/0x48 (0x225DC/0x2264A), the application→boot handover entry 0x24A26,
the scheduler loop 0x24974, the flash-write engine 0x24B70 (progress mirrored to 0xFFFC0D04),
the block/header/marker validation suite matching spec 06 §2 exactly (0x24FD0 checks
0x876543xx / end-pointer / 0x12345678), the flash protection gate 0x245EE, and a full UDS/
ISO-TP diagnostic stack (service dispatcher 0x22C1C, request/response pumps, ISO-15765-2
SF/FF/CF/FC engine 0x238E8/0x2661E/0x2678C with STmin timing from the FRC at 0xFFFFF220).

**Unknown entries (34 functions, 30 clusters):** all in the desynced 0x20554–0x21EA6 zone
where Ghidra hit bad/unimplemented instruction data (`unknown_badinstr_*`):
0x20554, 0x20780(+0x44770), 0x2087E, 0x20ACC, 0x20AF8, 0x20E34, 0x20E56, 0x20E7A, 0x21006,
0x21178, 0x211A2, 0x212D6, 0x212DC, 0x2132C, 0x2136A, 0x2137C(+0x21384), 0x213A0(+0x213A4),
0x213BE, 0x213E6, 0x21574, 0x21596, 0x215F8, 0x216C4, 0x216CE, 0x219AC, 0x21AE8, 0x21B26,
0x21BA2, 0x21BFA(+0x21C1A), 0x21EA6. Several show real call heads (VBR-relative calls in
0x2087E/0x21EA6, tables at 0x2100C/0x21AEC) — the region is likely a data blob or compressed/
obfuscated stream inside the boot-manager block, not dead code. Real code resumes at 0x22224.

**Cross-region leads:**
- Duplicated helper copies live in the boot-manager copies region (0x313xx–0x3EE7A) and the
  application (0x44770, 0x75DE0–0x75F6C, 0x96010–0x9A978, 0xA1026); units r04_boot/r08_app
  should cross-check these names (`__2`/`__3` suffixed here).
- 0x24A26 (handover entry) is called from **0x00072964** in the application — the app-side
  units should document that caller as the flash-request/hand-over trigger (spec 06 §5, 15).
- The diag engine hooks into 0x270BC–0x2F3F0 (0x26396, 0x26A78 thunks; 0x27180/0x27100
  session managers; 0x28xxx transport callbacks; 0x2F3F0 uses 0x229C0) — the unit owning
  0x27000+ should cross-reference `diag_*` names here.
- 0x22DF2/0x25A58 publish the tester-visible RAM window 0xFFF80080–0xFFF800C7 — the same
  window named in spec README item 20 (measurement protocol read-back during sessions).
- Spec cross-refs used: 06 §1/§2/§3/§5 (blocks, headers, markers, handover flag 0xFFF8403E,
  block ID "bjix2201"), 15 (UDS session/service), 17 (flash-write path), 02 §2 (FRC
  0xFFFFF220), 16 (FENTRYR 0xFFFFA902, FCU P/E flow), 01 (memory map, data flash 0x80100000).
