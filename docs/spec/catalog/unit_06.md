# Unit 06 (r04_boot)

Region slice 0x0412DE–0x06A78C. Note: the 0x41xxx–0x4Axxx band decompiled as
"bad instruction data" throughout (Ghidra could not decode it); entries there lean on
edges.csv callers/callees and embedded constants only.

#### 0x000412DE `unknown_baddata_0412de` — unknown
Single-instruction bad-data body: tail-calls 0x000420A8 with `arg3-0x2F`, then stores the
r2/r3 pair at `[r2+0x34]`. No resolved callers/callees in edges.csv; semantics unrecoverable
from this dump (likely misdecoded data or an offcut entry). 1 member. [I]

#### 0x000413B6 `unknown_typ_teilenr_ref` — unknown
Bad-data decompile, but the embedded references are telling: it touches the string
`TYPTEILENRbhjd0701` at 0x001001A (the bootloader block ID, spec 06), writes `DAT_00009C40=0xF9`,
`_DAT_0001FFF2=0xFFF9` and fills a struct at an r11 base (+6, +0x104..+0x106). Plausibly builds
a part-number/identity record; decode too broken to assert more. 1 member. [I] spec 06 (bootloader block bhjd0701)

#### 0x000416BC `unknown_baddata_0416bc` — unknown
103-instruction bad-data body of self-indexed RAM stores (`*(short*)(r*2)=r` patterns); no
resolved edges. Cannot classify beyond "misdecoded". 1 member. [I]

#### 0x00041A7E `unknown_baddata_041a7e` — unknown
Bad-data body; branch on SR bit, sets `_DAT_00020004=2`, `uRamffffff4c=0xFFFFFFA6`. No edges.
1 member. [I]

#### 0x00043602 `unknown_load_043a00` — unknown
Loads a pointer from `PTR_DAT_00043A00` then hits bad data. Likely a data-pointer indirection
stub near the 0x43A00 table area. 1 member. [I]

#### 0x00043662 `unknown_load_043a5e` — unknown
Loads the word at 0x00043A5E area (`_LAB_00043a5e_2`) then bad data. Sibling indirection of
0x00043602 one page later. 1 member. [I]

#### 0x0004407E `unknown_store_04407e` — unknown
Stores argument r0 to `[r8+0x14]`, then an unimplemented instruction truncates flow. Likely a
register/struct writer whose body did not decode. 1 member. [I]

#### 0x00044190 `unknown_write_0x31` — unknown
Bad-data body repeating a store of 0x31 through r13. No edges; likely misdecoded table data.
1 member. [I]

#### 0x0004490C `unknown_gbr_c2_store` — unknown
Stores r0 to `GBR+0xC2` and r10 to `[r13+0x2C]`, then calls 0x000455E8 before bad data.
GBR-relative store suggests an on-chip register write, but the peripheral is not identifiable.
1 member. [I]

#### 0x00044B22 `stub_fwd_45262_add6a` — stub
Clean 4-instruction thunk: forwards `(param_1, param_2+0x6A)` to 0x00045262 and returns.
0x6A = 106 constant offset hints at a sub-block/channel index; target itself is bad data.
1 member. [O]

#### 0x00044B76 `unknown_baddata_044b76` — unknown
Stores r11 to `[r8+0x1C]`, then bad data. No edges. 1 member. [I]

#### 0x00044BDE `unknown_gbr_xor_ba` — unknown
Computes `r0 ^ 0xBA` into `GBR+0x228` and stores a byte at `[param_1-1]` before bad data.
The XOR constant 0xBA is unusual (not a mask); purpose unclear. 1 member. [I]

#### 0x00044E04 `unknown_zero_ptr` — unknown
Zeroes a byte at param_1, touches `[param_1+0x38]`, then bad data. Looks like a small
clear/init of a struct head. 1 member. [I]

#### 0x00045262 `unknown_call_45e0a` — unknown
Body entirely lost to an unimplemented instruction; header resolves callee 0x00045E0A and
caller 0x00044B22 (the +0x6A thunk). 1 member. [I]

#### 0x000452CC `unknown_arith_0452cc` — unknown
Bad-data arithmetic chain with carry/borrow flags into a byte result; shape resembles a
checksum or packed arithmetic helper but nothing is assertable. 1 member. [I]

#### 0x0004546C `unknown_baddata_04546c` — unknown
2-instruction bad-data body, no edges. 1 member. [I]

#### 0x00045776 `unknown_shift_045776` — unknown
Contains a full shift-by-param_4 pattern (left when ≥1, else arithmetic right by
`(~n&0x1F)+1`) — i.e. a 64-bit/rounding shift helper shape — then bad data. No callers
resolved. 1 member. [I]

#### 0x0004582C `unknown_shift_04582c` — unknown
Same shift-by-param_2 pattern as 0x00045776, storing the result byte to `(r0|0x4C)*2`.
Bad data; no edges. 1 member. [I]

#### 0x000459D0 `unknown_baddata_0459d0` — unknown
2-instruction bad-data body (byte store `*(char*)(r0*2)=r0`); cluster twin 0x0013362E lives
in the r10_app band — a duplicated constant-only copy of the same misdecoded pattern.
2 members. [I]

#### 0x000460B6 `init_frag_struct_fill` — unknown
Init-bank fragment (0x2C-stride family 0x460B6–0x47086): repeatedly stores r13 to a struct
at +8…, param_4 to +0x1C, conditionally +0x3C/+0x10/+r2/+r1, and stores an r1 pointer to
`[param_5+0x108]`. Called only by chain body 0x00046A56. Body ends in bad data. [I]

#### 0x0004628A `init_frag_04628a` — unknown
Init-bank fragment storing r10 to DAT_000000CE/_DAT_0001FFF2/_LAB_0000000E; called by chain
body 0x00046A56. Bad data beyond. [I]

#### 0x000463D0 `unknown_baddata_0463d0` — unknown
Isolated bad-data body (callers=-, unlike its 0x46xxx neighbors): stores r9/r3 into an
r1-based struct (+0x28, +0x18). No edges. 1 member. [I]

#### 0x000466DE `init_frag_0466de` — unknown
Init-bank fragment in the 0x46AC2 chain; byte-store runs plus `LAB_00000040=0x20`.
Bad data. [I]

#### 0x00046736 `init_frag_046736` — unknown
Init-bank fragment (chain 0x00046AC2): byte equality tests over r10/r12 lanes, indirect
call through `PTR_DAT_00046768`, writes +0x60 fields and `UNK_000000F0=0x78`. Bad data.
[I]

#### 0x000467BA `init_frag_0467ba` — unknown
Init-bank fragment (chain 0x00046AC2): writes param byte to +0x60, `UNK_000000F0=0x78`,
SR-flag byte store. Bad data. [I]

#### 0x00046832 `init_frag_046832` — unknown
Init-bank fragment (chain 0x00046AC2): zeroes `LAB_00000400`, indirect call through the
pointer at 0x00046A42, returns a byte. Bad data beyond. [I]

#### 0x000468CE `init_frag_0468ce` — unknown
Init-bank fragment (chain 0x00046AC2): duplicate r2 word stores; bad data beyond. [I]

#### 0x00046902 `init_frag_046902` — unknown
Init-bank fragment (chain 0x00046AC2): 8-fold byte store of r0 into param_1-relative slots,
then `[r13-1]=param_2`. Bad data beyond. [I]

#### 0x0004696A `init_chain_call_bank` — logic
Chain body of the 0x2C-stride init bank (0x460B6–0x47086): sequentially invokes ~55 fragment
entries (0x46996, 0x469DA, 0x46AC2, 0x46BE2, 0x46C66 … 0x46A56), each followed by a saved-r1
slot, then performs the shared epilogue: zero GBR-relative registers at +4,+0x14,…,+0x189,
BCLR masks (0x04,0x18,0x2C,0x40,0x50,0x60,0x70,0xA8,0xE0,0xF0,0x28,0x60,0x70,0x80,0x90,0xA0,
0xB0,0xC0,0xE8,0xF8) and BSET bits 0x08,0x28,0xF4, ending at _LAB_00047558. Reads like a
peripheral/I-O init interpreter; decode too broken to name the peripheral. 1 member. [I]

#### 0x00046996 `init_chain_046996` — logic
Widest chain body of the init bank (52 entries, 0x46AC2 → 0x46BE6) with the same GBR-register
epilogue as 0x0004696A; header callers=0x0004696A, callees=0x00046AC2/0x00046BE2. 1 member. [I]

#### 0x000469D2 `init_chain_0469d2` — logic
Chain body (callers=0x00046AC2) invoking 0x46C66…0x46A56 entries plus the shared GBR epilogue.
1 member. [I]

#### 0x00046A56 `init_chain_046a56` — logic
Chain body (callers=0x00046BE2) invoking 0x477CA, 0x460B6, 0x4628A, 0x4644A … entries plus
the shared GBR epilogue. 1 member. [I]

#### 0x00046AC2 `init_chain_046ac2` — logic
Chain body (callers=0x46996, 15 resolved callees incl. 0x46566…0x4696A) plus the shared GBR
epilogue — a nested level of the same interpreter. 1 member. [I]

#### 0x00046BE2 `init_chain_046be2` — logic
Chain body (callers=0x46996, callees 0x469FE/0x46A2A/0x46A56/0x46BBA) plus shared GBR
epilogue. 1 member. [I]

#### 0x00046C66 `init_gbr_io_regs` — logic
Register-init body (callers=0x000469D2): stores r0 to GBR+0x64, zeroes GBR offsets
0xBC,0xDC,0xEC,0xFC,0x18/19,0x38/39,0x58/59,0x78/79,0x128/29,0x148/49,0x168/69,0x188/89,
then the shared BCLR/BSET epilogue (clear 0x04..0xF8, set 0x08,0x28,0xF4). The GBR base and
offsets imply one on-chip peripheral bank; identity unknown. 1 member. [I]

#### 0x00046D86 `init_gbr_io_regs_c` — logic
Same register-init body as 0x00046C66 minus the GBR+0x64 store; called by 0x000469D2.
Cluster of 2 (0x00046D86, 0x00046F12 — identical pattern, differing constants only).
2 members. [I]

#### 0x00047086 `init_gbr_io_regs_b` — logic
Same register-init family (callers=0x000469D2) starting from the 0x70-mask clear; the only
difference from its twins is which leading stores survive the decode. 1 member. [I]

#### 0x00047656 `init_chain_047656` — logic
Chain body (callers=-) invoking 0x467BA…0x46A56 entries plus the shared GBR epilogue; the
tail-most chain segment of the init bank. 1 member. [I]

#### 0x000477CA `stub_ptr_47bac` — stub
Single observable action: returns `&LAB_00047BAC`, a constant flash pointer just past this
band. Used by chain bodies (0x46A56/0x46996/0x469D2) — likely the "next table" pointer of the
init interpreter. 1 member. [O]

#### 0x00049A3E `unknown_call_48d6e` — unknown
Bad-data body whose only decoded action is a call to offcut 0x00048D6E; called by 0x0004A44A.
1 member. [I]

#### 0x0004A412 `unknown_call_bank_04a412` — unknown
Bad-data body that serially calls ~15 fragments in 0x4A04A–0x4A44A (incl. itself via offcut
and 0x4A43E/0x4A44A). Shape matches the 0x46xxx init-bank interpreter (call + saved-r1 slot),
but nothing pins its purpose; callers=0x0004A412 (self/offcut only). 1 member. [I]

#### 0x0004A43E `unknown_frag_04a43e` — unknown
Bad-data fragment called by 0x0004A412; its decoded body re-lists the 0x4A412 call bank
(offcut overlap) so the real instructions are lost. 1 member. [I]

#### 0x0004A44A `fwd_49a3e_args_incr` — logic
Clean wrapper: calls 0x00049A3E with `(param_1+1, param_2+1)` and returns — an index-bumping
thunk into the 0x49A3E/0x48D6E fragment family (79 analyzed instrs but only this forwards).
1 member. [O]

#### 0x00062C40 `unknown_baddata_062c40` — unknown
254-instruction bad-data body; decoded head stores `PTR_LAB_00040080 = 0x80040080` and makes
an indirect call computed from `r3+0x6310E`. The 0x8004xxxx value looks like a masked/mirrored
address constant; no resolved edges. 1 member. [I]

#### 0x00063164 `dispatch_selfrel_063164` — dispatch
Bad-data body whose one decoded action is an indirect call to `FUN_00063164 + r2 + 4` — a
jump table relative to its own address (classic SH switch/trampoline). Callers/callees
unresolved. 1 member. [I]

#### 0x00066DA4 `init_tbl_run_timed` — logic
Runs a function-pointer block at 0x00066E8C–0x00066EA8 (8 slots: no-arg, `(1)`, no-arg,
`(0,*0x66E98)`, `(0,**0x66EA0,…)`), bracketing it with reads of the free-running timer
DAT_fffff220 and storing the elapsed delta (saturated to 0xFFFF) into `DAT_fff90a50`; also
clears `DAT_fff848cf`, `DAT_fff93b34/33`. Resolved callees 0x0009A46C etc. live in the app
region — cross-check there. Looks like a timed init/self-test sequence. 1 member. [I]

#### 0x00066EC0 `exc_record_clear` — logic
Initializes the fatal-event record at 0xFFF80D24: if both magics `DAT_fff80e0a` and
`DAT_fff800e0` equal 0xA835 it just bumps the occurrence counter `DAT_fff80e08` and latches
`DAT_fff800e6`; otherwise it zeroes the 10-slot ring (three dword arrays at 0xFFF80D24/D4C/D74
plus the ushort array at 0xFFF80D9C), the 16-word context buffer at 0xFFF80DC8, the housekeeping
bytes 0xFFF80DB0–0xFFF80E0E, then sets both magics to 0xA835. 1 member. [O]

#### 0x00066F82 `exc_capture_and_halt` — logic
Central fatal handler (caller 0x00068736, callees 0x00095458/0x00092148/0x000966B8/0x00068BE0/
0x00068658): captures r0, PR, SP-side registers and up to 16 stack words (bounded by RAM top
0xFFF9FFFC, remainder filled from DAT_00067104) into the 0xFFF80D24 ring; classifies the event
code in r0 against a 4-entry {min,max,class} table at PTR_DAT_00067108 (default class 0x10)
stored to `DAT_fff80e0f`; then runs handler pointers 0x0006710C–0x0006711C (one path passes
0xB0) and finishes in an infinite empty loop — deliberate halt awaiting watchdog reset.
1 member. [O]

#### 0x000670BA `exc_get_class` — data-accessor
Returns the byte `DAT_fff80e0f` (event class latched by 0x00066F82). Called by 0x00077E92.
1 member. [O]

#### 0x000670C2 `exc_get_tick_class` — data-accessor
Stores `DAT_fff80dc4` (the global latched at capture time, source `DAT_fff90998`) to *param_1
and returns the event class `DAT_fff80e0f`. Callers 0x00079570 and 0x000950FC. 1 member. [O]

#### 0x000670D2 `exc_get_flag_e0c` — data-accessor
Returns `DAT_fff80e0c` (a byte cleared by `exc_record_clear` 0x00066EC0) when the event class
`DAT_fff80e0f` is non-zero, else 0. Same accessor family as 0x000670BA/0x000670C2; callers
0x00079570 and 0x000950FC. 1 member. [O]

#### 0x000670E6 `exc_get_captured_word` — data-accessor
Returns the first captured dword `DAT_fff80db4` of the fatal-event record (saved by
0x00066F82) when the event class is set, else 0. Caller 0x000950FC. 1 member. [O]

#### 0x00067124 `timer_stamp_buf_init` — logic
Initializes the 7-slot timestamp buffer at 0xFFF80C64: slot 0 = free-run timer DAT_fffff220,
slots 1–6 cleared. No resolved callers (table-driven init). 1 member. [O]

#### 0x00067140 `timer_stamp_capture` — logic
Appends a snapshot of DAT_fffff220 into the next free slot of the 0xFFF80C64 buffer
(capacity 7, cursor `DAT_fff848ce`). Boot-phase timing probe. 1 member. [O]

#### 0x00067164 `timer_stamp_capture_once` — logic
One-shot guard around 0x00067140: captures the timestamp only while flag `DAT_fff93b30` is 0,
then latches it to 1. 1 member. [O]

#### 0x00067180 `unknown_prog_tbl_a9c` — unknown
Command-driven table programmer (caller 0x000689B4): if `param_2>>16==0x60`, fills
0xFFF90A9C[] with `((param_1>>16)+i)<<8 | low16(param_1+i)` for `param_3>>16` entries and
builds a one-bit-per-entry mask into `DAT_fff9353a`; if `==0x18` clears 4 ushorts at
0xFFF90A9C and the mask. Returns 1 if handled, 0 otherwise. Values look like channel
identifiers/tags — exact meaning unresolved. 1 member. [O] actions / [I] purpose

#### 0x00067200 `boot_handshake_start` — logic
Reads the boot/status word `DAT_fffc0d00` (0xFFFC area): 9/10 → sets handshake state
`DAT_fff93b3d=0x55`, writes `DAT_fffc0d04=0x1234`, on 10 also runs PTR_FUN_00067348(3) plus a
query; 0xD/0xE → state 0x56 + same magic; otherwise state 0 and if PTR_FUN_00067344 returns 0
sets `DAT_fffe0400 |= 0x3C04` (reset-control bits). Latches FRT into DAT_fff90aa4. Looks like
the application side of the boot hand-over / reset-cause evaluation (spec 15 hand-over).
1 member. [I] spec 15

#### 0x0006728E `boot_handshake_advance` — logic
Progresses the 0x55/0x56 handshake: bounded wait (~99 FRT ticks) for `DAT_fffc0d04==0`; path
0x55 → PTR_FUN_00067348(3), success writes magic `0x5555`, state 0xAA, `DAT_fff94710=0xD4`,
failure state 0xF1 → PTR_FUN_00067348(0) → `DAT_fffe0400 |= 0x3C04`; path 0x56 → state 0xAB,
magic `0x3737`; already-magic path → PTR_FUN_00067348(0), state 0xF0. 1 member. [I] spec 15

#### 0x00067350 `io_ffe3810_clear` — logic
Small peripheral op: clears two bytes of `DAT_fffe3810` and ORs `DAT_fffe3806 |= 0xF0`
(on-chip I/O at 0xFFFE38xx). Peripheral identity not resolved. 1 member. [O]

#### 0x00067384 `boot_handshake_wait` — logic
Only when 0x000673E4 reports state 0xAA: two bounded waits on `DAT_fff90aa8` (timeouts 4000 /
8000 on counters DAT_fff90ab0/DAT_fff90ab4 — the second until it equals the flash constant
DAT_000673fc). Waits for a peer to complete the handshake. 1 member. [I]

#### 0x000673E4 `boot_handshake_state_get` — data-accessor
Returns handshake state `DAT_fff93b3d` if it is 0x55 or 0xAA, else 0. Callers 0x0007C7BC
(r0c_app) and 0x00067384 — cross-region consumers of the boot hand-over state. 1 member. [O]

#### 0x00067400 `ram_blk_validate_repair` — logic
Validates two RAM blocks against flash templates and repairs them: block A at 0xFFF80100
(len 0x3F00, header magics: ptr 0xFFF80100, template `PTR_DAT_0006760c`, 0x3F00, 0xFFFFFFB3,
11-byte signature at 0xFFF80D07, generation `DAT_00040044`) and block B at 0xFFF99800
(len 0x1800, 7-byte signature). On mismatch runs PTR_FUN_00067610 over the block (CRC/check),
records the result in error latch `DAT_fff90998` and reports fault code 0x212 via
PTR_FUN_00067614 (fault-reporting API, spec 13), then re-copies the template. Includes busy
delays (0xFC0 / 0x600 loops). 1 member. [O] (spec 13 fault reporting)

#### 0x00067620 `ram_8word_test` — logic
RAM read/write test over 8 words at 0x1000 stride from 0xFFF80000: saves, writes expected
values from table `PTR_DAT_000677b0`, compares, restores; any mismatch records the address in
`DAT_fff90998` and reports fault 0x211 via PTR_FUN_000677b4. Called by 0x00067740.
1 member. [O]

#### 0x000676A4 `ram_ranges_test` — logic
Runs the test routine `PTR_FUN_000677bc` over five RAM ranges — 0xFFF90000, 0xFFF94000
(+0x4000), 0xFFF98000 (+0x1801), 0xFFF9B000 (+0x1000), 0xFFF9C000 (+0x4000) — reporting fault
0x211 with the failing value for each. Power-up RAM self-test (spec 05 §4 order).
1 member. [O]

#### 0x00067740 `ram_data_init` — logic
Startup RAM initialization: read/write-back pass over 0xFFF80000–0xFFF800FF via 0x0006768C
(exactly the boot-manager style init of spec 06 §5a — preserves the boot request record),
block copy from flash 0x0000B400 to 0xFFF84000 via PTR_FUN_000677c4, the 8-word RAM test
(0x00067620), then a table-driven dword-range copy loop (12-byte {src,dst,end} entries at
`PTR_PTR_000677c0`, count at 0x000677C8) — the .data-style section copy. 1 member. [O]
spec 06 §5a, spec 05 §4

#### 0x000677F8 `os_sched_run_due` — logic
Cooperative scheduler step: scans two task tables (entries of 0x14 bytes at 0xFFF9099C and
0xFFF90A3C, counts `*PTR_DAT_00067994` / `*PTR_DAT_0006799c`) for the active entry with the
smallest +4 value in each, runs its handler via PTR_FUN_00067998 (deactivating entries that
return non-zero), then re-arms the periodic timer: `LAB_fffff5d0 = (table[DAT_fff90ab8] +
DAT_fffff530>>8) * 0x100` and calls PTR_FUN_000679a0/PTR_FUN_000679a8. 1 member. [I]

#### 0x0006791A `os_timer_rearm_6791a` — logic
Timer re-arm helper sharing the scheduler's idiom (same `LAB_fffff5d0` computation and
PTR_FUN_000679a0/a8 calls) plus peripheral setup `DAT_fffe0c0e = (…&0xFFF0)|6`,
`DAT_fffff5cd &= 0xEF`, `DAT_fffff5cf |= 0x10`. Callers 0x0006E804/0x0006E882 (r0c_app).
1 member. [I]

#### 0x0006796A `tbl_copy_28_to_14` — library
Copies `param_3 & 0xFF` records from 0x28-byte source structs (param_1) into 0x14-byte
destination structs (param_2), field map (u16, u16, *dword, u8, dword at +0x24). Table
compaction helper; caller 0x0006E804. 1 member. [O]

#### 0x000679AC `tbl_6x10_lookup` — data-accessor
Bounds-checked lookup into a 6-column × 10-row dword table: returns
`*(DAT_000679fc + p1*4 + p2*0x18)` when `p1<6 && p2<10`, else 0. Callers are all in this
module (0x00066DA4, 0x00067A7C, 0x00067AD4, 0x00067B16, 0x00067E3C). 1 member. [O]

#### 0x000679D4 `unknown_cond_679d4` — unknown
Returns 1 only when PTR_FUN_00067a00() yields non-zero AND bit 0 of the byte selected by
`*(PTR_PTR_00067a04 + DAT_fff90ab8)` is set — an index-dependent enable check whose subject
is not resolvable. Caller 0x00067F50. 1 member. [I]

#### 0x00067A08 `val_353c_fanout4` — logic
Copies master value `DAT_fff9353c` into the four slots DAT_fff93542/48/54/60 (0xFFF935xx
config area). 1 member. [O]

#### 0x00067A68 `main_loop` — entry
Never-returning main scheduling loop (called by 0x00066DA4): forever { PTR_FUN_00067b54(
*PTR_PTR_00067b50); FUN_00067b16(); PTR_FUN_00067b58(); } — service callback, housekeeping
0x00067B16, service callback. Matches the entry-contract step "enter the main scheduling
loops; never return" (spec 06 §4). 1 member. [O] spec 06 §4

#### 0x00067A7C `boot_phase_init_announce` — logic
First boot phase (state `DAT_fff93b32` 0→1, guarded by `DAT_fff848e6`): runs the
measure-start/stop pair PTR_FUN_00067b60/68 with table `PTR_DAT_00067b5c`, and on
`DAT_fff93b40==1` registers a 25-byte flash blob at 0x0000186A via PTR_FUN_00067b70 plus
PTR_FUN_00067b6c — an announce/handshake with data from the boot block. Caller 0x0006A104.
1 member. [I]

#### 0x00067AD4 `boot_phase_wait_tick` — logic
Second boot phase on the same state byte: 1→2 after another measure pair (table
`PTR_DAT_00067b74`/`DAT_00067b78`), 2→3 only once the tick flag `DAT_fff848cf` is set —
i.e. waits for the first timer tick before completing hand-over. Caller 0x0006A104.
1 member. [I]

#### 0x00067B16 `os_sched_service_step` — logic
Main-loop scheduler service (called by 0x00067A68 after the first service callback): only when
the boot state machine is complete (`DAT_fff93b32==3`); runs the measure pair with index
`DAT_fff93b33` and table `PTR_DAT_00067b7c`, then PTR_FUN_00067b84(2)/00067b88/00067b8c; its
resolved callees are the task-array executor 0x00067BE4, config loader 0x0006839C, table lookup
0x000679AC, top5 clear 0x000683C8 and monitor reset 0x0006856C — the periodic scheduler
bookkeeping bundle. 1 member. [O]

#### 0x00067B90 `chans6_init_ff` — logic
Initializes six 0xE-stride entries in the 0xFFF935xx config area (0x3572/80/8E/9C/AA/B8) to
0xFF and two shorts (0x356E, 0x35A6) from `DAT_fff90628` — invalidating six channel slots at
startup. Called by 0x00067A7C. 1 member. [O]

#### 0x00067BC2 `chans4_init_from_0628` — logic
Loads `(u16)DAT_fff90628` into the four channel slots DAT_fff9357C/8A/98/B4 — the short-field
counterpart of 0x00067B90 in the same 0xE-stride channel table. 1 member. [O]

#### 0x00067BE4 `sched_run_task_arrays` — logic
Task-list executor (callers 0x00066DA4, 0x00067B16, 0x00067A7C, 0x00067AD4, 0x00067E3C):
walks NULL-terminated function-pointer arrays from param_3; execution mode from
PTR_FUN_00067dcc decides instrumentation — plain calls; or wrapped in measure start/stop
(PTR_FUN_00067dd0/dd4), with extra capture when the callback equals designated task
`DAT_fff90a88`, an overrun report via PTR_FUN_00067de4 when a runtime exceeds threshold
`DAT_fff90a8c`, and bookkeeping through PTR_FUN_00067dd8/de0/ddc/de8/dec. The heart of the
cyclic scheduler with per-task timing. 1 member. [O]

#### 0x00067DF0 `task_timer_tick` — logic
Cyclic task countdown (caller 0x00069FA4): for a task whose slot flag
`[param_2 + DAT_fff93b33 + 0x1E]` is 1, decrements the short at *param_2; at zero it reloads
from `param_2[7]`, calls the pointer at DAT_00067EA8, and on zero return marks the task done
(`param_2[9] byte = 1`) or bumps a retry counter at +4 unless it is -1. 1 member. [O]

#### 0x00067E3C `task_dispatch_wrapped` — logic
Runs one task (callers 0x0006A0A8, 0x0006A054): critical-section enter PTR_FUN_00067eac,
optional lock PTR_FUN_00067eb0, swaps the "current" pointer `DAT_fff8401c` to the task record,
bumps its run counter at (*param_1)+2, brackets execution with the measure pair
PTR_FUN_00067eb4/eb8, calls the task function `param_1[3](idx)`, then restores everything and
calls PTR_FUN_00067ec0. 1 member. [O]

#### 0x00067EC4 `load_meas_sample` — logic
CPU-load sampling helper: measure window 0xA0 via PTR_FUN_000680a8/b4, and when enabled
(`DAT_fff93b36==1`) stores raw results DAT_fff90a68/DAT_fff90a6c from
PTR_FUN_000680ac(&DAT_fff90a5c,…)/PTR_FUN_000680b0. Caller 0x0006A054. 1 member. [O]

#### 0x00067F02 `load_pct_track_max` — logic
Computes `DAT_fff935ba = load_pct_calc(0x68246)(DAT_fff90a68, DAT_fff90a6c)` and tracks the
maximum in DAT_fff935c4; state flags DAT_fff93b36 (armed) / DAT_fff93b39 (reset). The load
percentage of the window measured by 0x00067EC4. 1 member. [O]

#### 0x00067F50 `load_mon_enable_update` — logic
Sets enable flag `DAT_fff93b38 = ((*DAT_000680b8)() == 0)` — refreshes whether the cyclic
load monitor runs. Caller 0x0006A0A8. 1 member. [O]

#### 0x00067F66 `load_mon_calibrate` — logic
One-shot calibration (caller 0x00066DA4): only when the indexed table word equals 0xCA35 —
measure window 0xE0, enable monitor, run the idle-count window 0x00068088 with window 0xFF and
20000 (0x4E20) iterations referencing flash LAB_00004E20, then disable. Establishes the
idle-loop reference for load percentages. 1 member. [I]

#### 0x00067FBE `load_mon_cyclic` — logic
Cyclic load monitor invoked from the main loop (0x00067A68): when enabled + tick
(`DAT_fff848cf`) + table word 0x53AC, runs the idle-count window 0x00068088, stores load value
DAT_fff935bc (via 0x00068246) and the integrated delta DAT_fff935be (via 0x000681E4), keeps
maxima DAT_fff935C2/C0, and on flag DAT_fff93b3A resets history via 0x0006822C. 1 member. [O]

#### 0x00068088 `idle_count_window` — logic
Counts idle loop iterations: spins while `table_delta < window(param_3)` and the run is still
valid (max param_4 iterations), then converts the count to a load value with float constant
DAT_00068270 scaled by an indexed ushort table (`PTR_PTR_0006826c`), writing int+float math
into *param_1; zeroes *param_2. 1 member. [O]

#### 0x000681E4 `load_delta_integrate` — logic
10-entry history at 0xFFF90A70 (cursor DAT_fff93b37): accumulates
`DAT_fff90a84 += new - last` and returns the total scaled by float DAT_00068274 — integrated
load delta for the monitor. 1 member. [O]

#### 0x0006822C `load_hist_clear` — logic
Clears the 10-entry load history 0xFFF90A70 and the accumulator DAT_fff90a84. 1 member. [O]

#### 0x00068246 `load_pct_calc` — library
Pure math helper: returns `DAT_00068278 - (a * DAT_00068278) / b` (float constants) = k*(1-a/b),
guarded for b<1 — the load-percentage formula used by the monitors. 1 member. [O]

#### 0x0006827C `sched_mode_check` — logic
Checks the 6-byte-stride activation table `(idx*6 + sub + *(PTR_PTR_000684f0+DAT_fff90ab8))`;
if active returns 2 and calls 0x000684AA to (re)configure a trace buffer; otherwise returns
whether a designated task is set (`DAT_fff90a88 != -1`). Called by the task executor 0x67BE4.
1 member. [I]

#### 0x000682B8 `runtime_record_max` — logic
Stores clamped runtime `param_1` (≤0xFFFF) into DAT_fff935d0 and keeps the maximum in
DAT_fff935d2, bracketed by measure lock PTR_FUN_000684f4(0x10)/PTR_FUN_000684f8. Called by
0x67BE4's instrumented path. 1 member. [O]

#### 0x000682EA `runtime_top5_update` — logic
Maintains a 5-slot "top runtime" list (values DAT_fff935c6[5], task ids DAT_fff910e8[5], cursor
DAT_fff93b3b): inserts/replaces the entry for param_2 with a higher runtime param_1, finds the
minimum slot, and moves the cursor — all under the measure lock. Called by 0x67BE4. 1 member. [O]

#### 0x0006839C `sched_overrun_cfg_load` — logic
Loads the instrumentation config for the current index DAT_fff90ab8: overrun threshold
`DAT_fff90a8c` (table word, or -1 when 0xFFFF) and designated task pointer `DAT_fff90a88`.
These drive 0x67BE4's overrun report. Callers 0x00066DA4, 0x00067B16. 1 member. [O]

#### 0x000683C8 `runtime_top5_clear` — logic
Under the measure lock, clears the 5-slot top-runtime list (values + ids) and cursor when the
indexed enable byte is 1 or the one-shot flag DAT_fff93b3c was set; also clears the max
DAT_fff935d2 when no designated task is set. Caller 0x00067B16. 1 member. [O]

#### 0x00068440 `trace_buf_store16` — logic
If the trace buffer is armed (`param_2[5]==1`) and the write pointer (at *param_2) has not
passed the end (param_2[1]): stores clamped param_1, advances 2 bytes, increments the count at
param_2+0x10. Used by the instrumented task executor. 1 member. [O]

#### 0x00068468 `trace_buf_store_minmax` — logic
Trace-store variant: when armed and room, if `+0x12 < +0x10` stores +0x12 then swaps
(+0x12 ← +0x10), else stores 0xFFFF; advances the write pointer. Records the smaller member of
a min/max pair with promotion. 1 member. [O]

#### 0x00068496 `trace_buf_store_cfg16` — logic
When armed and room, stores the configured value at +0x10 into the trace buffer (no clamp/swap).
1 member. [O]

#### 0x000684AA `trace_buf_setup` — logic
Configures a trace buffer record from tables: enable byte via `PTR_PTR_00068508`, and when
enabled resets counters and loads the 16-byte buffer descriptor (start/end/etc.) from
`(param_1*0x10)+PTR_PTR_0006850c` into param_2[0..3]. Called by 0x0006827C. 1 member. [O]

#### 0x00068510 `task_budget_calc_8510` — logic
Reads a value via 0x000685CE and writes `(val+5) * float DAT_00068640` (as short) into task
field +0xC; if the +0x12 flag is set clears +0x10/+0x12. Computes a task time budget/margin.
Caller 0x00067E3C. 1 member. [I]

#### 0x0006853E `task_budget_calc_max` — logic
Same budget formula via 0x00068600 into task field +0xE, keeping the maximum in +0x10.
Caller 0x00067E3C. 1 member. [I]

#### 0x0006856C `monitors_reset_trigger` — logic
When the indexed table byte equals param_1: calls PTR_FUN_00068648, sets the load-monitor reset
flags DAT_fff93b39=1 and DAT_fff93b3A=1, calls PTR_FUN_0006864c, sets the top5-clear flag
DAT_fff93b3C=1 — a coordinated reset of all runtime monitors. Caller 0x00067B16. 1 member. [O]

#### 0x000685A6 `runtime_accumulate` — logic
Adds the timing-segment exit value from 0x00068600 into `[param_1+0xC]` and bumps the counter
at +0x12 — the per-task/per-module runtime accumulator hook used by the measurement protocol
side (callers include 0x000955E2, 0x00095A4C, 0x00069FA4, 0x00068FA2… in r08_app). 1 member. [O]

#### 0x000685BC `runtime_snap_clear` — logic
Moves the accumulated runtime to last-period fields: `+0x10 = (short)+0xC`, clear +0xC;
`+0x14 = +0x12`, clear +0x12. Called by 0x0006A160. 1 member. [O]

#### 0x000685CE `timing_seg_enter` — logic
Pushes a timing record: saves previous anchor `DAT_fff84020` into *param_1, stamps
`param_1[1] = DAT_fffff220`, installs param_1 as the new anchor, and returns the delta since
this record's previous stamp (measure lock 0xA0). Very widely used (r08_app measurement packers).
1 member. [O] — note: uses 0xFFF84020, inside the area spec 06 §5 calls boot handover; spec
should be re-checked.

#### 0x00068600 `timing_seg_exit` — logic
Pops the timing segment: restores anchor from *param_1, adds `now - param_1[1]` into the
parent anchor's +8 accumulator, and returns the excess over the budget `param_1[2]` (0 if
within). Complement of 0x000685CE. 1 member. [O]

#### 0x0006865C `fatal_assert_enter` — stub
Two-instruction trampoline into the 0x00068736/0x00066F82 fatal path. 55 resolved callers
across the whole application (0x0007F44E, 0x0008F70C, 0x0006BC90, …) — this is the
"unrecoverable error" entry point modules call to capture context and halt. Cluster of 2 with
0x000686D6 (same pattern, callers=- — likely the raw vector-table copy). 2 members. [O]

#### 0x000686A6 `fatal_tramp_686a6` — stub
Calls the function pointer at DAT_0006876C (the fatal handler pointer, target 0x00066F82) and
returns — third trampoline variant of the fatal path. 1 member. [O]

#### 0x00068736 `fatal_tramp_68736` — stub
Same one-call trampoline through DAT_0006876C; Ghidra resolved the ultimate target as
0x00066F82 (`exc_capture_and_hatten`). Callers 0x0006865C/0x000686D6. 1 member. [O]

#### 0x00068778 `diag_mode_switch` — logic
Diagnostic session/mode switcher (callers 0x00067200/0x0006728E boot-handshake, 0x0007C736):
selects entry `PTR_PTR_00068994[param_1]` into `DAT_fff848e8` and mode byte `DAT_fff93b42`
(param_1 < 6, old != 0xF); on change, when status `DAT_fff94fe4`==0x0A runs the calibration
validation 0x000689B4(mode, 0x180000, 0x180000, 0) (calibration block base!), when ==0x05 runs
0x000688F2(mode, 0, 0xFF); if leaving mode 3 with a failed query sets the reset bits
`DAT_fffe0400 |= 0x3C04` and clears DAT_fff94710; finally notifies via 0x00068830. Matches the
session layer of spec 15. 1 member. [I] spec 15

#### 0x00068830 `diag_mode_notify_if_match` — logic
If param_1 equals the active mode `DAT_fff93b42`, calls PTR_FUN_000689a0(param_2) — a
mode-match notification hook. Callers 0x00068778/0x00068A72/0x00068C04. 1 member. [O]

#### 0x0006884E `diag_sess3_validate` — logic
Mode-3 maintenance: requires mode==3 and `DAT_fff94711&1`; validates the measurement list
(&DAT_fff90a9c / mask &DAT_fff9353a) via PTR_FUN_000689a4; on failure reads status via
PTR_FUN_000689ac and posts event (5,0xFA) or (0xF,0xF0) through PTR_FUN_000689a8; on success
posts (0,0xFF) or (10,0xF5) and calls 0x00068B30. 1 member. [I]

#### 0x000688F2 `diag_sess124_check` — logic
Same event pattern for modes 1/2/4: gate check PTR_FUN_000689b0(param_2,param_3); failure →
status read + event (5,0xFA)/(0xF,0xF0); success → (0,0xFF) when param_2==0 else (5,0xFA)
plus 0x00068B30. Callers 0x00068778, 0x0007C736. 1 member. [I]

#### 0x000689B4 `diag_cal_meas_validate` — logic
Calibration-session validation: gates on mode match, `DAT_fff94711&1` and PTR_FUN_00068bb4();
runs PTR_FUN_00068bb8(param_2,param_3,param_4) (invoked with 0x180000/0x180000/0 — the
calibration block) and the measurement-list check PTR_FUN_00068bbc(&DAT_fff90a9c,
&DAT_fff9353a); posts ok/fail events like its siblings and calls 0x00068B30; also calls the
list programmer 0x00067180. Callers 0x00068778/0x00068A72/0x0007C736. 1 member. [I] spec 09/10

#### 0x00068A72 `diag_sess_shutdown` — logic
Session close (caller 0x00072964): when status is 0x0A or 0xFA, re-validates via 0x000689B4
with the 0x180000 calibration bounds, posts (0xFA,5)/(0xFF,0), notifies 0x00068830, then sets
mode `DAT_fff93b42 = 0xF` (none). 1 member. [O]

#### 0x00068ADA `bootreq_record_build` — logic
Builds the boot-request record at 0xFFF80000 (spec 06 §5a layout): writes magics 0xA55A at +0
and 0x5AA5 at +0x12, length 0x14 at +2, params into +4/+8/+0xC, state 1 at +0x10, mirrors two
words via `PTR_DAT_00068bd4 + 0x480000/4`, then calls PTR_FUN_00068bdc. Gated on
PTR_FUN_00068bb4()==1 and PTR_FUN_00068bcc()==2. This is the application asking the boot
software for a reset session. 1 member. [O] spec 06 §5a

#### 0x00068B30 `diag_sess_ok_latch` — logic
Latches session validity `DAT_fff93b45`: 1 when the two config words (via PTR_DAT_00068bd4)
equal flash constants DAT_00068bd0/DAT_00068bd8, or when the posted status is non-zero; else 0.
Called by all the diag validate paths. 1 member. [O]

#### 0x00068B58 `diag_evt_report_ad00` — logic
When `DAT_fff94711&1`: reads peripheral `DAT_ffffad00`; posts (0,0xFF) if it is 0 and status
was 0x0A, or (10,0xF5) if status was 0 and the peripheral is set, then latches via 0x00068B30.
Caller 0x0007CB2A. 1 member. [I]

#### 0x00068BE0 `hw_cond_once_1001_40` — logic
One-shot hardware condition: returns whether `DAT_ffff1001 & 0x40` is set, latching
`DAT_fff93b3e` so only the first evaluation while clear reports false. 11 callers across
regions (0x00091E86, 0x0006C612, 0x0008F5A0…). 1 member. [O]

#### 0x00068C04 `meas_list_cfg_apply` — logic
Applies the measurement-list configuration from the persistent RAM record at 0xFFF8011x (the
block validated by 0x00067400): requires the 0x11 tag at +0x14 and state 1 at +0x28, computes
mask `DAT_fff9353a = DAT_ffffad00 & DAT_fff8012c | DAT_fff80130`, copies up to 4 ushorts from
+0x1C into 0xFFF90A9C, runs PTR_FUN_00068db0(3, sel), sets state `DAT_fff8010c` 1/3; a second
selector clears the list; changes of DAT_fff80c50 trigger PTR_FUN_00068dc4(3, 0/1). The
measurement-protocol list setup (spec 10 §3). 1 member. [I] spec 10

#### 0x00068D20 `ram_blk_xor_check` — logic
Integrity check of the persistent RAM record block at 0xFFF80100: XORs
`(DAT_fff80104 - 0x10) >> 2 & 0xFF` dwords starting at 0xFFF80110 and compares against the
stored checksum `DAT_fff80108`. Called by the list-apply state machine 0x00068C04; same block
validated/repaired by 0x00067400. 1 member. [O]

#### 0x00068D48 `meas_list_cfg_reload` — stub
Copies `DAT_fff80c50` (requested selector) to `DAT_fff80c4c` and runs 0x00068C04. Caller
0x00067200 (boot handshake). 1 member. [O]

#### 0x00068D52 `bootreq_arm_from_8900` — logic
Two-step arm on the 0xFFF89000 state block: when `DAT_fff89008` matches the flash selector and
`DAT_fff89004==0x100`, `DAT_fff89000==3` and the diag enable is set, posts `DAT_fff9472c=10`
and advances the selector; when the countdown reaches 0 calls PTR_FUN_00068dd4 with the three
params at 0xFFF8900C/10/14 — the builder 0x00068ADA. Caller 0x0007CB2A. 1 member. [I]

#### 0x00068DD8 `thunk_ptr_68de0` — stub
Calls the pointer at DAT_00068DE0 with param_1 (target 0x0009617A per edges). Called by the
RAM-block validator 0x00067400. 1 member. [O]

#### 0x00068DE4 `ram_pattern_test` — logic
Classic marching RAM test over param_2 bytes: zero-fill + verify, 0xFFFFFFFF + verify, pattern
from PTR_DAT_00068e84 + verify, then a byte-wise walking pattern from DAT_00068e80 with
complemented-word checks; returns the failing address or 0. This is the routine behind
PTR_FUN_000677bc used by the power-up range tests (0x000676A4) and block validation (0x00067400).
1 member. [O]

#### 0x00068EA0 `mode_3b56_get` — data-accessor
Returns the mode byte `DAT_fff93b56`. Eight callers, several cross-region (0x00148EB6,
0x00155030, 0x000A7C9C, 0x000AD072…). 1 member. [O]

#### 0x00068EA8 `mode_tick_dispatch` — dispatch
Reads the tick counter `DAT_fff848cf`, clamps to 1–3, and when it differs from the current mode
`DAT_fff93b56` dispatches PTR_FUN_00068fac/fb0/fb4 (targets 0x001320BC/C4/CC in r08_app) for
values 1/2/3, then latches the mode. Caller 0x0006D6E0. 1 member. [O]

#### 0x00068EFE `mode_3b5e_update` — logic
Reads a value via PTR_FUN_00068fb8 (target 0x001320D4) and, when the tick counter is running
and the value is < 4, stores it into `DAT_fff93b5e`. Caller 0x0006D6EC. 1 member. [O]

#### 0x00068F34 `timing_enter_f7c` — stub
Calls 0x000685CE (`timing_seg_enter`) on the timing record at 0xFFF90F7C. Cluster twin
0x00068F3E is the accumulate variant (calls 0x000685A6 on the same record) — same instruction
pattern, different function-pointer constant, hence different names. 2 members. [O]

#### 0x00068F5C `timing_enter_fac` — stub
Same enter/accumulate pair for the timing record at 0xFFF90FAC (member 0x00068F66 =
accumulate via 0x000685A6). 2 members. [O]

#### 0x00068F70 `timing_enter_fc4` — stub
Enter/accumulate pair for the timing record at 0xFFF90FC4 (member 0x00068F7A accumulates).
2 members. [O]

#### 0x00068F98 `timing_enter_ff4` — stub
Enter/accumulate pair for the timing record at 0xFFF90FF4 (member 0x00068FA2 accumulates).
2 members. [O]

#### 0x00068FC4 `chan_open_dispatch` — logic
Selects an open path by mode `DAT_fff93b5a`: mode 1 → function pointer at `[param_1+0x10]+8`,
mode 2 → `[param_1+0x14]`; passes it to 0x00069F00 together with the high nibble of table byte
`0xFFF9131A[param_1[+0xC]]` and a mode flag. Part of the channel/device open layer. 1 member. [I]

#### 0x0006900C `chan_status_sync` — logic
Fetches a status byte via PTR_FUN_00069284(`param_1[+0x48]`, idx, idx-1) and stores it into
both `[param_1[+0x10]]+9` and `[param_1[+0x48]+idx-1]` — synchronizes a channel state between
two structures. Caller 0x00078368. 1 member. [I]

#### 0x000690DA `tbl_id50_lookup` — library
Linear search of the 0x50-byte-entry device table (base `PTR_DAT_00069290`, count
`*PTR_DAT_0006928c`) for an entry whose first ushort equals param_1; returns the index or 0xFE.
Used by all 18 channel-open functions of this band. 1 member. [O]

#### 0x00069110 `chan_open_d0_tblA` — logic
Opens channel slot `DAT_fff848d0` (0xFF = unused): resolves the id via 0x000690DA and, if found
(<0xFE), calls init pointer PTR_FUN_00069294 with `table + idx*0x50`. First of the 18-slot
channel-open family; all share the 0x50-entry table layout and slot bytes DAT_fff848d0..d4.
1 member. [O]

#### 0x0006914C `chan_open_d1_tblA` — logic
Same open pattern for slot `DAT_fff848d1` (table A, init via PTR_FUN_00069294). 1 member. [O]

#### 0x00069188 `chan_open_d2_tblA` — logic
Same open pattern for slot `DAT_fff848d2` (table A). 1 member. [O]

#### 0x000691C4 `chan_open_d3_tblA` — logic
Same open pattern for slot `DAT_fff848d3` (table A). 1 member. [O]

#### 0x00069200 `chan_open_d4_tblA_irq` — logic
Slot `DAT_fff848d4`, table A, register-variant: calls `(*DAT_0006f060)(id, cfg, cfg+6,
entry[+0x44])` and then sets bit 1 in `[cfg+7]` — looks like an IRQ/handler registration with
the entry's config block. 1 member. [I]

#### 0x0006923C `chan_open_d5_tblA_gated` — logic
Slot `DAT_fff848d5`, table A, opened only when `*(int*)PTR_DAT_00069298 == *param_2` (config
pointer match); init via PTR_FUN_00069294. 1 member. [O]

#### 0x0006929C `chan_open_d6_tblB_gated` — logic
Slot `DAT_fff848d6`, table B (`PTR_DAT_00069528`), gated on `*(int*)PTR_DAT_00069524 ==
*param_2`; init via PTR_FUN_0006952c. 1 member. [O]

#### 0x000692E4 `chan_open_d7_tblB` — logic
Slot `DAT_fff848d7`, table B, plain init via PTR_FUN_0006952c. 1 member. [O]

#### 0x00069320 `chan_open_d8_tblB` — logic
Slot `DAT_fff848d8`, table B, plain init. 1 member. [O]

#### 0x0006935C `chan_open_d9_tblB_gated` — logic
Slot `DAT_fff848d9`, table B, gated on `*DAT_00069530 == *param_2`. 1 member. [O]

#### 0x000693A4 `chan_open_da_tblB_irq` — logic
Slot `DAT_fff848da`, table B, register-variant via `(*DAT_0006f060)` with bit-1 set at cfg+7
(same shape as 0x00069200). 1 member. [I]

#### 0x000693E0 `chan_open_db_tblB_gated` — logic
Slot `DAT_fff848db`, table B, gated on `*(int*)PTR_DAT_00069534 == *param_2`. 1 member. [O]

#### 0x00069428 `chan_open_dc_tblB` — logic
Slot `DAT_fff848dc`, table B, plain init. 1 member. [O]

#### 0x00069464 `chan_open_dd_tblB_gated_irq` — logic
Slot `DAT_fff848dd`, table B, gated on `*(int*)PTR_DAT_00069538 == *param_2`, register-variant
via `(*DAT_0006f060)`. 1 member. [I]

#### 0x000694AC `chan_open_de_tblB` — logic
Slot `DAT_fff848de`, table B, plain init. 1 member. [O]

#### 0x000694E8 `chan_open_df_tblB` — logic
Slot `DAT_fff848df`, table B, plain init. 1 member. [O]

#### 0x0006953C `chan_open_e0_tblC_irq` — logic
Slot `DAT_fff848e0`, table C (`PTR_DAT_0006983c`), register-variant via `(*DAT_0006f060)`.
1 member. [I]

#### 0x00069578 `chan_open_e1_tblC` — logic
Slot `DAT_fff848e1`, table C, init via PTR_FUN_00069840. Last of the 18-slot channel-open
family (slots DAT_fff848d0–e1). 1 member. [O]

#### 0x000695B4 `chan_open_e2_tblC` — logic
Channel-open slot `DAT_fff848e2`, table C (`PTR_DAT_0006983c`), init via PTR_FUN_00069840 —
continuation of the slot family (now d0..e5, 22 openers). 1 member. [O]

#### 0x000695F0 `chan_open_e3_tblC_gated` — logic
Slot `DAT_fff848e3`, table C, gated on `*(int*)PTR_DAT_00069844 == *param_2`; init via
PTR_FUN_00069840. 1 member. [O]

#### 0x00069638 `chan_open_e4_tblC` — logic
Slot `DAT_fff848e4`, table C, plain init via PTR_FUN_00069840. 1 member. [O]

#### 0x00069674 `chan_open_e5_tblC` — logic
Slot `DAT_fff848e5`, table C, plain init via PTR_FUN_00069840 — last of the 22-slot
channel-open family (DAT_fff848d0–e5). 1 member. [O]

### Measurement raw-value getters (0x696B0–0x69A34 and on)
Shared pattern (spec 10 measurement signals): acquire lock — PTR_FUN_00069848(0x50) for the
0xFFF90ABx–0xFFF90B7x group, PTR_FUN_00069b94(0x50) for 0xFFF90B7F–0xFFF90CFx — read two
adjacent raw bytes from the RAM measurement image, release lock, return
`(hi & mask) << 8 | lo` (masked) or the full 16 bits. Callers are the measurement packers at
0x00076708–0x00076D70 (r0c_app).

#### 0x000696B0 `meas_get_0abf` — data-accessor
Raw value at 0xFFF90ABF/0AC0, mask 0xF (4-bit). Caller 0x00076708. 1 member. [O] spec 10

#### 0x000696DC `meas_get_0ae5` — data-accessor
Full 16-bit raw value at 0xFFF90AE5/AE6. Caller 0x00076742. 1 member. [O] spec 10

#### 0x00069708 `meas_get_0b56` — data-accessor
Raw value at 0xFFF90B56/B57, mask 7 (3-bit). Caller 0x000767CA. 1 member. [O] spec 10

#### 0x00069734 `meas_get_0b64` — data-accessor
Full 16-bit raw at 0xFFF90B64/B65. Caller 0x000767F8. 1 member. [O] spec 10

#### 0x00069760 `meas_get_0b74` — data-accessor
Full 16-bit raw at 0xFFF90B74/B75. Caller 0x00076804. 1 member. [O] spec 10

#### 0x0006978C `meas_get_0b66` — data-accessor
Raw at 0xFFF90B66/B67, mask 0x1F (5-bit). Caller 0x00076810. 1 member. [O] spec 10

#### 0x000697B8 `meas_get_0b76` — data-accessor
Raw at 0xFFF90B76/B77, mask 0x1F. Caller 0x0007681C. 1 member. [O] spec 10

#### 0x000697E4 `meas_get_0b68` — data-accessor
Raw at 0xFFF90B68/B69, mask 0x1F. Caller 0x0007683C. 1 member. [O] spec 10

#### 0x00069810 `meas_get_0b7c` — data-accessor
Raw at 0xFFF90B7C/B7D, mask 0x1F. Caller 0x00076878. 1 member. [O] spec 10

#### 0x00069850 `meas_get_0b7f` — data-accessor
Raw at 0xFFF90B7F/B80, mask 0x1F; first of the second lock group (PTR_FUN_00069b94).
Caller 0x000768B2. 1 member. [O] spec 10

#### 0x0006987C `meas_get_0b8c` — data-accessor
Raw at 0xFFF90B8C/B8D, mask 0x1F. Caller 0x00076914. 1 member. [O] spec 10

#### 0x000698A8 `meas_get_0b8e` — data-accessor
Raw at 0xFFF90B8E/B8F, mask 0x1F. Caller 0x0007692E. 1 member. [O] spec 10

#### 0x000698D4 `meas_get_0b9e` — data-accessor
Raw at 0xFFF90B9E/B9F, mask 0x1F. Caller 0x0007693A. 1 member. [O] spec 10

#### 0x00069900 `meas_get_0c04` — data-accessor
Raw at 0xFFF90C04/C05, mask 0x1F. Caller 0x00076A4A. 1 member. [O] spec 10

#### 0x0006992C `meas_get_0c18` — data-accessor
Full 16-bit raw at 0xFFF90C18/C19. Caller 0x00076AF2. 1 member. [O] spec 10

#### 0x00069958 `meas_get_0c48` — data-accessor
Full 16-bit raw at 0xFFF90C48/C49. Caller 0x00076B78. 1 member. [O] spec 10

#### 0x00069984 `meas_get_0c59` — data-accessor
Full 16-bit raw at 0xFFF90C59/C5A. Caller 0x00076B8E. 1 member. [O] spec 10

#### 0x000699B0 `meas_get_0c98` — data-accessor
Raw at 0xFFF90C98/C99, mask 3 (2-bit). Caller 0x00076C9C. 1 member. [O] spec 10

#### 0x000699DC `meas_get_0cbc` — data-accessor
Raw at 0xFFF90CBC/CBD, mask 7 (3-bit). Caller 0x00076CF0. 1 member. [O] spec 10

#### 0x00069A08 `meas_get_0ccc` — data-accessor
Raw at 0xFFF90CCC/CCD, mask 7. Caller 0x00076CFC. 1 member. [O] spec 10

#### 0x00069A34 `meas_get_0cf4` — data-accessor
Raw at 0xFFF90CF4/CF5, mask 0x3F (6-bit). Caller 0x00076D70. 1 member. [O] spec 10

#### 0x00069A60 `meas_get_0cf6` — data-accessor
Measurement raw getter (lock B): 0xFFF90CF6/F7, mask 0x3F. Caller 0x00076D8A. 1 member. [O] spec 10

#### 0x00069A8C `meas_get_0cf8` — data-accessor
Raw getter (lock B): 0xFFF90CF8/F9, mask 0x3F. Caller 0x00076DA4. 1 member. [O] spec 10

#### 0x00069AB8 `meas_get_0cfa` — data-accessor
Raw getter (lock B): 0xFFF90CFA/FB, mask 0x3F. Caller 0x00076DBE. 1 member. [O] spec 10

#### 0x00069AE4 `meas_get_0d04` — data-accessor
Raw getter (lock B): 0xFFF90D04/05, mask 0xF. Caller 0x00076DCA. 1 member. [O] spec 10

#### 0x00069B10 `meas_get_0d08` — data-accessor
Full 16-bit getter (lock B): 0xFFF90D08/D09. Caller 0x00076DD6. 1 member. [O] spec 10

#### 0x00069B3C `meas_get_0d4c` — data-accessor
Raw getter (lock B): 0xFFF90D4C/4D, mask 0x1F. Caller 0x00076E78. 1 member. [O] spec 10

#### 0x00069B68 `meas_get_0d4e` — data-accessor
Raw getter (lock B): 0xFFF90D4E/4F, mask 0x1F. Caller 0x00076E84. 1 member. [O] spec 10

#### 0x00069B9C `meas_get_0d90_24bit` — data-accessor
Third lock group (PTR_FUN_00069ee0/00069ee4): returns the 24-bit value
`0xFFF90D90<<16 | 0xFFF90D91<<8 | 0xFFF90D92`. Caller 0x00076EEC. 1 member. [O] spec 10

#### 0x00069BCE `meas_get_0de4` — data-accessor
Raw getter (lock C): 0xFFF90DE4/E5, mask 0x3F. Caller 0x000771AC. 1 member. [O] spec 10

#### 0x00069BFA `meas_get_0e24` — data-accessor
Raw getter (lock C): 0xFFF90E24/E25, mask 0x3F. Caller 0x000772D0. 1 member. [O] spec 10

#### 0x00069C26 `meas_get_0e26` — data-accessor
Full 16-bit getter (lock C): 0xFFF90E26/E27. Caller 0x000772DC. 1 member. [O] spec 10

#### 0x00069C52 `meas_get_0e28` — data-accessor
Full 16-bit getter (lock C): 0xFFF90E28/E29. Caller 0x000772E8. 1 member. [O] spec 10

#### 0x00069C7E `meas_get_0e2a` — data-accessor
Full 16-bit getter (lock C): 0xFFF90E2A/E2B. Caller 0x000772F4. 1 member. [O] spec 10

#### 0x00069CAA `meas_get_0e3a` — data-accessor
Full 16-bit getter (lock C): 0xFFF90E3A/E3B. Caller 0x0007731A. 1 member. [O] spec 10

#### 0x00069CD6 `meas_set_0e74` — logic
Measurement write-back (lock C): merges bits 8..11 of param_1 into the low nibble of
0xFFF90E74 (preserving 0xF0) and stores the low byte to 0xFFF90E75. Caller 0x0007784C.
1 member. [O] spec 10

#### 0x00069D06 `meas_set_0e84` — logic
Setter (lock C): 3-bit field into 0xFFF90E84 (preserve 0xF8) + low byte to 0xFFF90E85.
Caller 0x0007792C. 1 member. [O] spec 10

#### 0x00069D36 `meas_set_0e87` — logic
Setter (lock C): 5-bit into 0xFFF90E87 (preserve 0xE0) + 0xFFF90E88. Caller 0x0007792C.
1 member. [O] spec 10

#### 0x00069D68 `meas_set_0ea4` — logic
Setter (lock C): 5-bit into 0xFFF90EA4 + byte 0xFFF90EA5. Caller 0x00077978. 1 member. [O] spec 10

#### 0x00069D98 `meas_set_0ea6` — logic
Setter (lock C): 5-bit into 0xFFF90EA6 + byte 0xFFF90EA7. Caller 0x00077978. 1 member. [O] spec 10

#### 0x00069DCA `meas_set_0ea8` — logic
Setter (lock C): 5-bit into 0xFFF90EA8 + byte 0xFFF90EA9. Caller 0x00077978. 1 member. [O] spec 10

#### 0x00069DFC `meas_set_0ecc` — logic
Setter (lock C): 5-bit into 0xFFF90ECC + byte 0xFFF90ECD. Caller 0x000779D0. 1 member. [O] spec 10

#### 0x00069E2C `meas_set_0ece` — logic
Setter (lock C): 5-bit into 0xFFF90ECE + byte 0xFFF90ECF. Caller 0x000779D0. 1 member. [O] spec 10

#### 0x00069E5E `meas_set_0ed0` — logic
Setter (lock C): 5-bit into 0xFFF90ED0 + byte 0xFFF90ED1. Caller 0x000779D0. 1 member. [O] spec 10

#### 0x00069E90 `meas_set_0edf` — logic
Setter (lock C), unmasked full bytes: 0xFFF90EDF = param>>8, 0xFFF90EE0 = param. Caller
0x00077AC4. 1 member. [O] spec 10

#### 0x00069EB4 `meas_set_0ee1` — logic
Setter (lock C), unmasked: 0xFFF90EE1/EE2. Caller 0x00077AC4. 1 member. [O] spec 10

#### 0x00069F00 `chan_stage_set` — logic
Stage/step manager for a channel record (called by 0x00068FC4): if the +2 byte has bit 7 set,
validates the requested 4-bit stage `param_2` against the indexed count table
(`PTR_PTR_00069f80 + DAT_fff90ab8`), optionally inverting the delta (param_3), then stores
`param_2 & 0xF | 0x80`. Returns 0 on invalid transitions. 1 member. [O]

#### 0x00069FA4 `os_cyclic_task_batch` — logic
Runs one cyclic batch under a lock: clears bit 0 of peripheral `DAT_fffec011`, runs the task
tick 0x00067DF0 and then eight argument pairs from the tables at PTR_PTR_0006a00c…0x0006a04c
through PTR_FUN_0006a014, bracketed by PTR_FUN_0006a008/0006a050 lock calls; timing captured
via 0x000685CE/0x000685A6. 1 member. [O]

#### 0x0006A054 `load_mon_task_step` — logic
Composite periodic step (callees 0x00067EC4 load_meas_sample, 0x00067E3C task dispatch,
0x00067F02 load_pct_track_max, plus PTR_FUN_0006a068/6c/74) — sample load, run task, update
percentage. 1 member. [O]

#### 0x0006A0A8 `os_task_step_b` — logic
Second composite step: PTR_FUN_0006a0bc(0x0006a0b8), PTR_FUN_0006a0c0(), plus 0x00067E3C
(task dispatch) and 0x00067F50 (load monitor enable update). 1 member. [O]

#### 0x0006A11C `flags10_set_913xx` — logic
Sets ten enable flags to 1 (0xFFF91338, 0x34C, 0x360, 0x374, 0x388, 0x39C, 0x3B0, 0x3C4,
0x3D8, 0x3EC) — (re)enables ten cyclic consumers; called from 0x0006856C on monitor reset.
1 member. [O]

#### 0x0006A160 `runtime_snap_task` — logic
Under measure lock 0xA0, runs the snapshot/clear 0x000685BC over ten timing records
(0xFFF90F64, 0xF7C, 0xF94, 0xFAC, 0xFC4, 0xFDC, 0xFF4, 0x100C, 0x1024, 0x103C) — the
periodic close-out of the runtime statistics bank. Caller 0x00067A50. 1 member. [O]

#### 0x0006A1C4 `flags7_set_910xx` — logic
Sets seven enable flags to 1 (0xFFF91066, 0x107A, 0x108E, 0x10A2, 0x10B6, 0x10CA, 0x10DE) —
second flag bank, also from 0x0006856C. 1 member. [O]

#### 0x0006A1F4 `task_run_mark` — logic
Task start bookkeeping (called from 0x00067E3C): when slot `param_1[+0x14+idx]` is active,
checks deadline `[+6] != [+4]` (calls 0x0006A3AE on mismatch), stamps `[+8] = DAT_fff90628`
and increments the start counter [+6]. 1 member. [O]

#### 0x0006A228 `task_run_complete` — logic
Task completion counterpart: increments the end counter [+4]; if `[+6] != [+4]` (start/end
imbalance) calls 0x0006A3AE. Called from 0x00067E3C. 1 member. [O]

#### 0x0006A3AE `task_defer_dispatch` — logic
Examines the slot value: 2 → sets flag `DAT_fff93b5f` and returns 2; 3 → indirect deferred
call PTR_FUN_0006a3ec(short at +0x12+idx); otherwise passes the value through. The mismatch
handler for 0x0006A1F4/0x0006A228. 1 member. [O]

#### 0x0006A3F0 `timing_seed_from_anchor` — logic
Seeds a timing record from the current anchor `DAT_fff84020`: `*param_1 = *(anchor+8)`,
`param_1[2] = DAT_fffff220`, `param_1[1] = 0` (measure lock 0xA0). Callers 0x00067BE4 and
0x0006F1B0/188/1D8 (r0c_app). 1 member. [O]

#### 0x0006A41C `timing_overrun_accum` — logic
Accumulates timing overrun: `overrun = *(anchor+8) - *param_1`; when the raw elapsed
`now - param_1[2]` exceeds it, adds the difference into `param_1[1]`. Callers 0x00067BE4 and
0x0006F1EC/19C/1C4. 1 member. [O]

#### 0x0006A45C `timer_flag_clear_6a45c` — logic
If flag `[param_1+0x10]` is 1, clears the value at +0xE and the flag. Companion of the budget
calculators (0x00068510 family). Callers 0x0006F188/1B0/1D8. 1 member. [O]

#### 0x0006A46E `val_snap_max_6a46e` — logic
Stores `[+4]` (saturated 0xFFFF → -1) into `[+0xC]` as ushort and keeps the maximum in
[+0xE]. Callers 0x0006F19C/1C4/1EC. 1 member. [O]

#### 0x0006A494 `freq_ratio_period_calc` — logic
Float ratio/period computation on the indexed channel (idx `*PTR_DAT_0006a6e8`): takes the two
counters `0xFFF905C0[idx]>>2` and `0xFFF905A8[idx]>>2` as times, gates on state
`0xFFF908C0[idx]` and armed flag `0xFFF908BA[idx]`, computes ratio `a/b` (reciprocaled when
< 1.0), compares against float thresholds at PTR_DAT_0006a6f0/0x6a6f8, derives a period
`uVar2/uVar3 + DAT_0006a6f4` capped at 0xFF into `DAT_fff93b66`, sets error codes 0x13/0x0B/07
into `DAT_fff93b65`, and tracks min/max times into DAT_fff93802–808 gated by
`DAT_fff9472e/*DAT_0006a6fc`. Looks like a speed/frequency (period) measurement over timer
inputs (spec 02 speed inputs). Caller 0x0007AB48. 1 member. [I] spec 02

#### 0x0006A65E `timeout500_from_3b67` — logic
`DAT_fff935d8 = min(DAT_fff93b67, 0x20)`; returns 500 when the result is non-zero, else 0 —
a scaled timeout/step from the ratio byte. Caller 0x0007B6D8. 1 member. [I]

#### 0x0006A680 `wdog_service_sm` — logic
Watchdog/supervisor state machine (callers=- → timer driven): initializes
`DAT_fff935d4 = 20000` with PTR_FUN_0006a700(1,0); clamps counter DAT_fff90880 at 999; state
DAT_fff935d7/5d6/5d8 progressions call PTR_FUN_0006a788(1, &LAB_00004e20) (the 20000-iteration
flash reference also used by the load calibration 0x00067F66) and reload
`DAT_fff90880 = DAT_fff90876`; returns 1 on service/reset steps. Matches the cyclic watchdog
service of spec 05 §4-7. 1 member. [I] spec 05

#### 0x0006A78C `cfg_bank_select` — logic
Selects the module-wide configuration bank: requires `param_1 == ~param_2` (tokenized
command); bank 0 → `DAT_fff90ab8 = 0`, `DAT_fff844a4 = -1`, `DAT_fff93b3f = 0`; bank 1 →
`DAT_fff90ab8 = PTR_PTR_0006a854 - PTR_PTR_0006a858` (pointer-delta = table element count) and
`DAT_fff844a4 = ~DAT_fff90ab8`, flag 1. **DAT_fff90ab8 is the index used by every
`PTR_PTR_xxx + DAT_fff90ab8` table access in this module** — this switches all of them between
two table banks. Called by 0x000688F2 (diag session check). 1 member. [O]

## Unit summary

**Coverage:** 220 clusters, 227 named functions (7 clusters of 2 members; twin members keep the
base name with `__2`, except the four timing thunk pairs where enter/accumulate differ).

**Counts by kind:** logic 129, data-accessor 42, unknown 34, stub 16, library 3, dispatch 2,
entry 1 (main_loop).

**What this slice is:** the application's OS/runtime layer (0x66DA4–0x6A78C) plus a
bad-data 0x4xxxx band:
- 0x41xxx–0x4A4xx: Ghidra "bad instruction data" throughout — an init-bank interpreter
  (chain bodies calling 0x2C-stride fragments that end in GBR-relative register clear/set
  epilogues) plus isolated garbage; mostly `unknown_*`.
- 0x66DA4–0x673xx: fatal-error manager (context ring at 0xFFF80D24, magic 0xA835, class range
  table, halt loop; reached from 55 call sites via 0x6865C/0x686D6/0x686A6/0x68736 trampolines),
  power-up RAM tests (0x67400/0x67620/0x676A4/0x68DE4, faults 0x211/0x212), RAM data init
  (0x67740, boot-request-preserving per spec 06 §5a), boot hand-over handshake
  (0x67200/0x6728E/0x67384/0x673E4, magics 0x1234/0x5555/0x3737 at 0xFFFC0D04, reset bits
  0xFFFE0400|=0x3C04).
- 0x677F8–0x686xx: cooperative scheduler + instrumentation (task arrays 0x67BE4, timing
  segment stack anchored at DAT_fff84020, per-task budget/overrun, top-5 runtime list, trace
  buffers) and the CPU-load monitor (idle-window calibration against flash 0x4E20, float
  constants at 0x68270/74/78).
- 0x687xx–0x68Exx: diagnostic session manager (modes via PTR_PTR_00068994/DAT_fff93b42,
  events (value,priority) to 0xFFF94FE4/E5C), calibration-session validation with the 0x180000
  block bounds, boot-request record builder (0x68ADA, spec 06 §5a layout 0xA55A/0x5AA5),
  measurement-list config apply (0x68C04/0x68D20 from the persistent record at 0xFFF80100).
- 0x68Fxx–0x696xx: timing-record thunks (0xFFF90F64..0xFFF91024) and the 22-slot channel-open
  layer (slots 0xFFF848D0–E5, three 0x50-byte-entry device tables, id lookup 0x690DA; some
  slots register a handler via 0x6F060 with a cfg bit — IRQ-registration shape).
- 0x696xx–0x69Exx: measurement raw-value getter/setter bank over the RAM image
  0xFFF90ABF–0xFFF90EE2 under three lock pairs — the stock side of spec 10's index list;
  callers are the r0c_app packers 0x76708–0x77AC4.
- 0x69Fxx–0x6A7xx: cyclic task steps, task start/end bookkeeping, watchdog-shaped state
  machine (0x6A680), and 0x6A78C `cfg_bank_select` — sets DAT_fff90ab8, the index behind
  every `PTR_PTR_xxx + DAT_fff90ab8` table access in this module (two config banks).

**Unknown entries (34):** 0x000412DE 0x000413B6 0x000416BC 0x00041A7E 0x00043602 0x00043662
0x0004407E 0x00044190 0x0004490C 0x00044B76 0x00044BDE 0x00044E04 0x00045262 0x000452CC
0x0004546C 0x00045776 0x0004582C 0x000459D0 (+twin 0x0013362E) 0x000460B6 0x0004628A
0x000463D0 0x000466DE 0x00046736 0x000467BA 0x00046832 0x000468CE 0x00046902 0x00049A3E
0x0004A412 0x0004A43E 0x00062C40 0x00067180 0x000679D4 — 32 of them are the 0x4xxxx bad-data
band (plus the two 0x67xxx entries marked unknown for honesty: 0x67180 command-table
programmer, 0x679D4 index condition).

**Cross-region leads:**
- 0xFFF8401C/0xFFF84020 (task "current" pointer and timing anchor) lie inside the area spec 06
  §5 describes as boot handover (`0xFFF84000–0xFFF8403F`); spec 06 should be re-checked.
- r0c_app units: measurement packers 0x76708–0x77AC4 own the callers of the meas_get/meas_set
  bank; 0x7C736/0x7CB2A/0x72964 own the diag session calls; 0x6E804/0x6E882 use the timer
  re-arm 0x6791A; 0x6F188..0x6F1EC pair with the timing helpers.
- r08_app units: 0x955E2/0x95A4C/0x958xx (measurement protocol) use 0x685CE/0x685A6/0x685BC;
  0x1320BC/CC/D4 are the mode_tick_dispatch targets; 0x66F82's direct helpers
  0x95458/0x92148/0x966B8 and fault-report entry 0x9665C/0x96674 pair with this module.
- The 0x2C-stride init bank (0x460B6–0x47086) and its GBR epilogue decode to garbage here; a
  byte-level re-disassembly of 0x44000–0x4B000 would recover the peripheral init table.
- 0x000413B6 references the bootloader part-number string at 0x001001A (block bhjd0701) —
  cross-check with the r00_boot unit.
