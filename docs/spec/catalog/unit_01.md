# Unit 01 (r00_boot)

Region 0x000000-0x03FFFF as far as this unit reaches: clusters 0x000004A0-0x0000631A sit in
**block 0** (0x0000-0x3FFF, Bosch software-authentication module `bhie0101`, spec 06 §6.0a:
service table 0x25C-0x280, SHA-1 at 0x345A, name/keys stored bit-inverted at 0x288) plus the
**boot data block** descriptor entries (0x624C/0x625A/0x62F6/0x631A, spec 06 §5a); clusters
0x000105A6-0x00013BE0 are the start of the **bootloader** (0x10000-0x1FFFF, spec 06 §1).
Cross-refs to spec 06 use section numbers from `docs/spec/06-boot-application-interface.md`.

#### 0x000004A0 `auth_stub_call_0598` — stub
Three-instruction stub that tail-jumps to 0x598 with argument 0; 0x598 is not itself a
decompiled function (interior label). No resolved callers — reached indirectly. [O]

#### 0x0000052A `auth_load_entry_ptrs` — data-accessor
Copies the current 8-byte table entry pointed to by RAM global 0xFFF84E78 into globals
0xFFF843C8 and 0xFFF84938 (two words). The entry pointer is computed by 0x63A as
`*(r10+0x18) + index*8`. Called only by 0x63A. [O]

#### 0x0000063A `auth_tbl_state_step` — logic
Table-driven state-step executor: runs an optional callback (param_2), stores its 0/1 result
into the flag at PTR_DAT_00000750 (anything else -> code 0x22); then selects one of three
indirect targets by index DAT_fff850b9 and flag *_LAB_00000754, and on a zero result advances
DAT_fff850b9, recomputes DAT_fff84e78 = table_base(r10+0x18) + index*8 and calls 0x52A.
Calls RAM-resident code at 0xFFF8200E/12/1E/2A (inside the RAM window later validated by
0x1A66). Error path builds an 0x22 response through a callback at r10+0x14 and sets state 2;
suppress-bit handling sets state 3. No resolved callers (pointer-invoked). [I]

#### 0x00000774 `diag_dispatch_service` — dispatch
UDS-style service dispatcher: requires length >= 3 (else NRC 0x13), looks the service byte up
in a table of (id16, handler) entries with count at param_1+5, runs a validity check via
0x8AA-area pointer (fail -> 0x33), then picks the handler from one of three session tables
(param_1[2..4], selected by request bits 1/2/3). Maps handler results {6->0x13, 4->0x33,
0xD/0xE->0x72, 9->0x31, 0xB->0x24, 0xF->pass 0x0F, else->0x22}; 0 = pending (state 1,
suppress-bit -> 3), 1 = suppressed (state 7). This is the Bosch diagnostic-session library
shared with the application and the boot manager (spec 06 §5a). [O]

#### 0x000007AA `diag_dispatch_service_reg` — dispatch
Register-context twin of 0x774: searches (param_2<<8|r0) in the id table at unaff_r13 with
count at +0x14, dispatches through session tables at +8/+0xC/+0x10, same NRC mapping. Calls
0x1E02. Entered with context in r9-r14 instead of memory params. [O]

#### 0x00000AB4 `diag_validate_pad_bytes` — logic
Padding validator: when enabled (SR bit) or the first trailing byte is 0xFF, scans the request
from +2 while bytes equal the expected pad value; uniform pad remainder ends in NRC 0x22
stored via PTR_DAT_00000b48, otherwise copies the first request byte and sets length/DLC.
Suppress-bit at +3 vs +0xE handled like the dispatcher. [I]

#### 0x00000B14 `diag_nrc_13_response` — stub
Minimal negative-response builder: clears PTR_DAT_00000b48, writes NRC 0x13
(incorrectMessageLengthOrInvalidFormat) into the response buffer, sets state 2 and
length+1. Cluster of 2 (0x0B14, 0x0B18 — adjacent copies). [O]

#### 0x00000B88 `auth_store_ctrl_ptrs` — data-accessor
Stores param_1 into 0xFFF84E80 and copies system-register value DAT_fffff220 into 0xFFF84E84,
substituting 1 when it reads 0. Likely records a control block plus a derived default. [O]

#### 0x00000D2C `diag_pack_addr_size_resp` — logic
Bit-packs two length-prefixed request fields (address: unaff_r11 bytes, size: unaff_r9 bytes,
shift-accumulated big-endian into the context at PTR_DAT_00000e14 +0/+4). If the request SID
at +0xB is 0x34, runs a validation callback (r8+4) and maps {6->0x13, 4->0x33, 3->0x70,
9->0x31, 0xF->none, else->0x22}. Success writes response byte 0x20 + big-endian 16-bit length
and bumps the request cursor. Called by 0x16F0 (upload path); NRC 0x70 = uploadDownloadNotAccepted. [I]

#### 0x00000DA0 `diag_build_resp_0x20` — logic
Twin of 0xD2C without the bit-packing: SR-flag set -> NRC 0x31, r0 != 0xF -> 0x22; otherwise
the same 0x20 + length positive-response build and cursor bump. Shares the 0xE14 context. [I]

#### 0x00001034 `auth_deinvert_name_copy` — logic
Fills a 0x119-byte buffer with a complemented fill byte, then complement-copies 0x1A bytes from
the response buffer and 12 bytes from config+0x40 into RAM 0xFFF84954, finishing with an
indirect call. The complement copies de-obfuscate block 0's bit-inverted name/key strings
(spec 06 §6.0a: `Bosch.common.Auth_TSW.TCU2.V001` stored inverted at 0x288). [I]

#### 0x0000104C `auth_deinvert_short_copy` — logic
Shorter variant of 0x1034 (fill length from in_r1): complement-fills, complement-copies the
data, copies the same 12-byte config+0x40 blob to 0xFFF84954, indirect call. Called by the
upload range check 0x1A66. [I]

#### 0x00001090 `auth_stub_check8` — stub
Two-instruction wrapper calling 0x10B0 with count 8. [O]

#### 0x0000109E `auth_bytes_equal_check` — library
Compares in_r2 bytes at param_2 against pattern in_r1, then calls through PTR_SUB_00001258 and
additionally accepts when DAT_fff84e74[1] == 1 or a fetched word equals DAT_00001260. Returns
boolean. Looks like a fill/erase verification helper with an escape hatch. [I]

#### 0x000010B0 `auth_fill_verify_loop` — library
Recursive byte-compare loop (count in param_1, pattern in_r1) sharing the 0x1258/0x125C/0x1260
post-check sequence with 0x109E. Self-recursive and called by 0x1090(8). [I]

#### 0x000010DC `auth_get_ptr_1264` — data-accessor
Two-instruction constant accessor returning the address of _LAB_00001264. [O]

#### 0x00001364 `lib_memcpy` — library
Thunk into 0x136E (byte-forward copy loop). Cluster of 5: copies at 0x1364, 0x18012, 0x243DC,
0x3E160, 0x7D1A8 — the standard memcpy duplicated across boot blocks (and one copy at
0x7D1A8 inside the application area). [O]

#### 0x0000136E `lib_memcpy_core` — library
Byte-wise copy loop (dst, src, count). Cluster of 5: 0x136E, 0x1801C, 0x243E6, 0x3E16A,
0x7D1B4 — the loop bodies matching the 0x1364 thunks. [O]

#### 0x00001374 `lib_sum_bytes` — library
Adds every byte in [param_1 .. param_2] (inclusive, count = param_2-param_1+1) into param_3
and returns the accumulator — additive checksum over a range. Cluster of 4: 0x1374, 0x1BC24,
0x3086C, 0x726AC. [O]

#### 0x000013C4 `auth_call_service_locked` — logic
Invokes a block-0 service through the service object at DAT_00000210: takes a lock via
PTR_SUB_000013f0(1), calls the +0x20 preparation and +0x24 main entry (resolved callees 0x243E
and 0x24C8 — two of the ten documented services, spec 06 §6.0a), then releases the lock. [O]

#### 0x00001404 `diag_read_did_multi` — logic
Multi-DID read handler (UDS 0x22-style): parses an even-length DID list (else NRC 0x13),
caps the count at param_1+0x16 (else 0x31), resolves each DID either through the special
0xF2xx/0xF3xx callback param_1[2] or a range search over the 12-byte DID table at *param_1,
collects matches at param_1[4], then loops 0x1624 per DID with flow control: result 3 -> NRC
0x14 (responseTooLong), 5 -> 0x22, 8 -> 0x7F, 6 -> 0x13, 4 -> 0x33, else 0x31; finishing sets
state 1 (suppress-bit -> 3). Same Bosch DID library the application uses. [O]

#### 0x00001624 `diag_did_read_one` — logic
Per-DID reader: fetches the (DID, index) pair from the param_1[4] collection, reads the
12-byte table entry, writes the DID into the response and snapshots the request context, then
dispatches the encoder through table DAT_000016ec[entry[4]] or, for 0xF2xx/0xF3xx DIDs, the
special handler param_1[3] (absent -> code 5). Called by 0x1404. [O]

#### 0x000016F0 `diag_upload_next` — logic
Upload pump: only when the transfer-active flag DAT_fff8525f is 1 does it run the worker at
0x192A-area with the transfer config at DAT_fff84e98+8; otherwise returns 5 (mapped to NRC
0x22 by callers). Calls 0xD2C to build the response. [I]

#### 0x00001A5C `auth_upload_flag_clear` — data-accessor
Clears the upload-active flag DAT_fff8525f. Complements 0x1A66/0x16F0. [O]

#### 0x00001A66 `diag_upload_range_check` — logic
RAM-upload gate documented in spec 06 §6.0a (as 0x1A80): checks that the buffer
[*(DAT_fff84e98+8), +length at DAT_fff84e98+4) lies fully inside 0xFFF80100-0xFFF81FFF or
0xFFF88400-0xFFF8FFFF (else code 9 -> NRC 0x31); on a set request flag calls the callback at
PTR_FUN_00001b0c (0x1DFA) and 0x104C, then sets DAT_fff8525f = 1 to start the upload. [O]

#### 0x00001DC8 `lib_tramp_undecoded` — stub
Single-instruction function whose bytes Ghidra could not decode (halt_baddata); called as a
callee from 0x63A, so it behaves as a trampoline/return. Cluster of 73 identical copies spread
across all regions including the vector-tail area 0xFFFFF2FA-0xFFFFFF56 — probably RTS/NOP
padding promoted to functions. Honest classification. [I]

#### 0x00001DD0 `auth_call_svc_mat_aware` — logic
Checks the ROMMAT register 0xFFFFA820 bit 0 (user boot MAT mapped, spec 06 §8): when set it
first calls the hook at PTR_SUB_00001ef0(1), then invokes the block-0 service through the
service object at DAT_00000210+8 with context PTR_DAT_00001ef4. Tail runs into undecoded data. [O]

#### 0x00001DFA `auth_call_svc_direct` — logic
Four-instruction service invocation: calls the entry at DAT_00000210+8 with PTR_DAT_00001ef4.
Called by the upload range check 0x1A66. [O]

#### 0x00001E02 `auth_call_svc_ctx` — logic
Register-context variant of the service invocation (context in r1); called by the dispatcher
0x7AA and reaching the service init 0x1F18. [O]

#### 0x00001F18 `auth_svc_init` — logic
Block-0 service (entry #2 of the documented table, spec 06 §6.0a): zeroes a 0x30-byte context
via PTR_FUN_00002160 and sets state (+4) = 1, flag +0xD = 0. [O]

#### 0x00001F30 `auth_verify_state_machine` — logic
Auth challenge/response engine: outer states at +4 (2 -> 3, 3 -> 4 with sub-states 0..3 at +8);
sub-state 3 runs 0x26AE (hash stream) plus 0x26A4/0x268C and, on tag 10/0x75, copies a 0x14-byte
(20 = SHA-1 size) digest; state 4 compares two 0x14-byte buffers and returns 4 on match, 5 on
mismatch. Strongly matches the TPROT verification services of spec 06 §6.1. [O]

#### 0x00002060 `auth_svc_hash_start` — logic
Block-0 service: from state 1, with length param_1 != 0/4 and < 0x81 and param_4 % 3 == 0,
records the hash range (+0xC0/+0xF0), calls the hash setup pointers 0x2174/0x2170 and moves to
state 2; anything else parks in state 7. Callees 0x3D3A and 0x314E. [O]

#### 0x000020E4 `auth_svc_hash_update` — logic
Block-0 service: from state 2, hashes param_2 (< 0x81, nonzero) bytes from param_1 through
PTR_FUN_00002170 into the +0x10 buffer, sets +0xD and count +6, and an optional result word. [O]

#### 0x00002144 `auth_svc_status_map` — data-accessor
Maps the service state at +4 to a status code: 0 -> 1, 5/6 -> 2, 7 -> 3, others 0. (Not the
user-MAT dispatcher of the same number in spec 17 §5n — different memory.) [O]

#### 0x00002290 `rspi_b_regs_init` — logic
Resets RSPI-B (0xFFFFB800, spec 03 §1): disables its DMA requests (0xFFFE6074/0xFFFE6076 = 0),
clears SPSR/SPPCR, sets 0xFFFFB810 = 0xAF11 and SPCR (0xFFFFB800) = 0x48 — the same register
recipe the bootloader uses for direct RSPI-B access (spec 03 line 141). Cluster of 2
(0x2290 and a copy at 0x9210C in the application area). [O]

#### 0x000022CC `rspi_b_init_xmit_word` — logic
Runs the RSPI-B reset (0x2290), spins a 1001-iteration SPSR-clear delay, writes the data word
into SPDR (0xFFFFB804), and on the (always-taken) timeout path re-inits and returns a pointer
to the 0x5A5A pattern at 0x5A5A. Decompilation of the polling loop is suspect; register writes
are certain. [O]

#### 0x00002320 `eeprom_stage_unique_bytes` — logic
Copies 16 device-unique bytes out of the EEPROM/data-flash mirror — 8 from 0x8010003A, 4 from
0x80100059, 4 from 0x80100032 — into the caller buffer, then enables the data-flash clocks
(0xFFFFA840 |= 0x2D00, 0xFFFFAB00 = 0xB300). Same source addresses as the user-MAT staging in
spec 17 §5n. [O]

#### 0x00002380 `boot_adc0_init` — logic
Initialises ADC0 (0xFFFFE800, spec 04 §1): stops the scan, waits for the busy bit
(0xFFFFE802 & 0x40) to clear, sets 0xFFFFE830 = 0x4E01, installs the interrupt vector
0x23EC at 0xFFFFE8E0, and writes the control word 0xC0. [O]

#### 0x000023D8 `boot_adc0_flag_consume` — data-accessor
Test-and-clear of ADC0 status bit 0 at 0xFFFFE802. [O]

#### 0x000023F0 `auth_verify_range_init` — logic
Verifies-job setup service (spec 06 §6.1 range hashing): clears a 0x4E0-byte context, stores
range start +0xC0 and remaining length +0xC4 (param_2-param_1+1), copies the 0x80-byte
reference blob to +0x450, stores the tag param_5 at +0x4DC and starts the hash stream (0x268C).
Called by the bootloader verification state machine 0x1BD58 and 0x1960C. [O]

#### 0x0000243E `auth_verify_hash_step` — logic
Advance service for 0x23F0: while +0xC4 remains, hashes min(0x10, remaining) bytes per call
through PTR_FUN_00002538 (SHA-1 update) advancing +0xC0; on exhaustion finalises the digest
into +0x43C via PTR_FUN_0000253C; with an empty range it runs the compare stream 0x26AE
directly. Called by 0x1BD58, 0x13C4 and 0x1963E. [O]

#### 0x000024C8 `auth_verify_result_get` — logic
Result service: when the stream reports done (0x26A4), compares the computed 0x14-byte digest
at +0x43C against the reference at +0x4D8, requiring tag state (+200 == 1, +0x4D0 == 10,
+0x4D4 == 0x75) for success. [O]

#### 0x00002540 `auth_stage_eeprom_keys` — logic
Boot service (spec 17 §5n documents the identical user-MAT twin): clears 0xFFF80066, stages
the 16 EEPROM-unique bytes via 0x2320, copies them to the hand-over area 0xFFF80054 (16 bytes)
and runs the pointer at 0x256C. [O]

#### 0x0000268C `auth_hashstream_init` — logic
Clears the 0x370-byte hash-stream context and sets its state (+0x154) to 4 (start). [O]

#### 0x000026A4 `auth_hashstream_done` — data-accessor
Returns whether the hash-stream state (+0x154) reached 0 (finished). [O]

#### 0x000026AE `auth_hashstream_step` — logic
Incremental range-hash state machine (4 -> 3 -> 2 -> 1): computes the byte length from the
bit count, streams data through 0x281A into the +0x268 buffer, trims trailing 0xFF padding
(erased flash) before finalising, and exports the digest pointer/state. This is the engine
behind "hashes address ranges" in spec 06 §6.1. [O]

#### 0x0000280C `auth_hashstream_start` — stub
Two stores that kick the hash-stream sub-state machine: +0x15C = 7, +0x158 = 2. [O]

#### 0x0000281A `auth_rsa_verify_state` — logic
RSA verify engine (spec 17 §5: RSA-1024, e=3, PKCS#1 v1.5): states 7 -> 6 -> 5 -> 4 -> 3 -> 2
-> 1 import the modulus and the 0x80-byte signature as bignums (0x29B4), take the exponent
(+0x268 = 3 when cfg+8 == 3), run the modular exponentiation 0x2E7A, and export the result
bytes via 0x2A3C. Callees 0x29B4, 0x2A92, 0x2A3C, 0x303A, 0x2E7A, 0x306C, 0x3D2C. [O]

#### 0x000029B4 `lib_bignum_from_be` — library
Converts a big-endian byte string (walking backwards) into a 0x21-word little-endian bignum
at param_1, zero-filling the high words and clearing state when done. Input to the RSA engine. [O]

#### 0x00002A3C `lib_bignum_to_be` — library
Exports a 0x21-word bignum to a big-endian byte string (walking down from the end), zero
padding the remainder. Output stage of the RSA verify. [O]

#### 0x00002A92 `lib_bignum_zero` — library
Zeroes param_2 words at param_1. Used by the RSA engine (0x281A/0x2E7A/0x2B5C). [O]

#### 0x00002AA4 `lib_bignum_sub` — library
Multi-precision subtract: param_1 = param_2 - param_3 (word-wise, borrow out as return). Used
by the division engine 0x2B5C. [O]

#### 0x00002ADE `lib_bignum_shl` — library
Shifts a bignum left by param_3 (< 32) bits across words, carrying bits between words. [O]

#### 0x00002B1A `lib_bignum_shr` — library
In-place right shift of a bignum by param_3 (< 32) bits, walking from the top word down. [O]

#### 0x00002B5C `lib_bignum_divmod_step` — logic
Knuth-D long division/modulo state machine (states 7 -> 6 -> 5 -> 4 -> 3 -> 2 -> 1): zeroes the
2x workspace, multiply-accumulates rows with 32x32->64 products (0x30FC), normalizes and shifts
the divisor (0x2ADE/0x30EA), estimates quotient digits with the two-word correction loop,
multiplies-subtracts (0x3088/0x2AA4) and does the final result shift (0x2B1A). Engine behind the
RSA modexp (0x2E7A). [O]

#### 0x00002E7A `auth_modexp_engine` — logic
Modular exponentiation controller for the RSA verify: sub-states at +0x53 chain squaring,
multiply and divmod (0x2B5C) passes over the 0x79/+0xBB workspaces, with a 2-bit window loop
(>> 0x1E) over the exponent and normalisation via 0x306C/0x2A92. Called by the RSA state
machine 0x281A. Matches spec 17 §5 (RSA-1024, e=3). [O]

#### 0x0000303A `lib_bignum_cmp` — library
Unsigned bignum compare over param_3 words: returns 1 / 0 / -1. [O]

#### 0x0000306C `lib_bignum_norm` — library
Returns the significant word length (strips leading zero words). [O]

#### 0x00003088 `lib_bignum_mulsub` — library
param_1 = param_2 - param_3 * param_4 (word-wise multiply with 0x30FC products and borrow
propagation); the inner loop of the division engine. [O]

#### 0x000030EA `lib_word_bitlen` — library
Counts the bit length of a 32-bit word by successive halving. [O]

#### 0x000030FC `lib_mulu_32x32` — library
32x32 -> 64 unsigned multiply returning high and low halves by pointer. [O]

#### 0x00003118 `auth_stage_handover_words` — logic
Builds the low hand-over area: fetches two 4-byte values via 0x3D84 (RSPI/ADC-derived), copies
them to 0xFFF80040 and 0xFFF80044, then copies 12 bytes from 0xFFF80054 to 0xFFF80048 — i.e.
assembles the 0x14-byte hand-over block next to the 16 EEPROM-unique bytes. Called by 0x2540. [O]

#### 0x0000314E `auth_rng_stream_crypt` — logic
Stream-crypt loop over param_2 (param_3 bytes) in 0x14-byte blocks: for each block it seeds
from the MMIO value at 0xFFFFF220, runs 0x3C9A (init), 0x3CD2 (absorb 0x14 bytes + the 4-byte
seed), 0x3CF6 (finalise) and copies 0x14 bytes out with 0x3D3A. Block size 20 = the RIPEMD-160
digest size; 0xFFFFF220 looks like a hardware RNG/data register. [I]

#### 0x000031D0 `lib_ripemd_round_step` — logic
One RIPEMD-160 round step: a = ROTL(a + f(b,c,d) + X[k] + K) + e with c rotated by 10 (the
RIPEMD-160 signature), f selected through the passed function pointer. Called 80x per
compression from 0x3218. [O]

#### 0x00003218 `lib_ripemd160_compress` — logic
RIPEMD-160 compression state machine (states 0-5): copies the 5-word chaining value into left
(+0x24) and right (+0x29) lines, runs 5 rounds x 16 steps per line through 0x31D0 with the
permutation/rotate/constant tables at PTR_DAT_00003478 (0x3478 area), then combines both lines
into the new chaining value and advances the 0x40-byte block cursor. Twin-line structure proves
RIPEMD-160 (spec 17 §5), not SHA-1 as spec 06 §6.0a assumed. [O]

#### 0x0000345A `lib_ripemd160_update` — logic
Hash absorber documented as "SHA-1 at 0x345A" in spec 06 §6.0a but driving the RIPEMD-160 core
(0x3218): states 0-5 manage init, bit counting (+0x14/+0x18), 0x40-byte block assembly
(big-endian word loads), compression and padding. Called by 0x3CD2; dispatches the shared
state tail helpers 0x39D8 etc. [O]

#### 0x000034AE `lib_hash_count_add` — logic
Adds the absorbed byte count x 8 into the 64-bit bit counter (+0x14/+0x18) and chains to the
next state via 0x39D8 — the tail of 0x345A case 1. [O]

#### 0x00003582 `lib_hash_fill_words` — logic
Fills the 16-word block buffer from the input (big-endian, partial first word supplied) and
advances the absorb state — tail of 0x345A case 3. [O]

#### 0x0000359E `lib_hash_fill_tail` — logic
Register-context word-fill loop for the hash block buffer (continuation of the absorb path),
closing with the block-count adjustment and state set. [O]

#### 0x00003926 `lib_hash_pad_tail` — logic
Fills remaining block words from input, rewinds the partial-word bytes (little-endian style
byte assembly from the buffer end) and sets state 0xC — the odd-length padding tail. [O]

#### 0x000039D6 `caseD_6` — stub
Pre-named switch case of switchD_00003472: sets the hash state (+0x60) to 8. Part of the
0x345A absorb/finalise dispatch; name kept as found. [O]

#### 0x000039D8 `lib_hash_set_state` — stub
Three-instruction state setter: stores in_r2 into the hash context state (+0x60). Shared tail
of several 0x345A cases. [O]

#### 0x000039E0 `lib_hash_pad_init` — logic
Prepares the hash padding buffer: 0x80 followed by three zero bytes at +0x74, points +0x78 at
it and clears the phase counter (+100). Called by 0x3C9A/0x3CF8. [O]

#### 0x00003A04 `lib_ripemd160_final` — logic
Finalisation driver (phases at +100): 1 zero-fill partial words, 2 compress (0x3218), 3 append
the 64-bit bit count as words 14/15 of the last block, 4 -> 0x3B5C, 5 -> 0x3B7A. Called by
0x3CF8. [O]

#### 0x00003B48 `lib_hash_len_append` — stub
Writes the low length word into block word 14 and the high bit-count word into word 15 (+0x3C),
then sets phase 4 — length-append tail of the hash padding. [O]

#### 0x00003B5C `lib_hash_final_compress` — logic
Runs the compression (0x3218) and, when the core reaches its done state, advances the
finalisation phase (+100) to 5. Called by 0x3A04. [O]

#### 0x00003B74 `lib_hash_set_phase5` — stub
Single store: finalisation phase (+100) = 5. [O]

#### 0x00003B7A `lib_hash_export_digest20` — logic
Exports the 5-word (160-bit) chaining value as 20 big-endian bytes and marks the hash context
done (state 6). Final stage of the RIPEMD-160 pipeline. [O]

#### 0x00003BA0 `lib_hash_digest_out_v1` — logic
Digest-output variant: writes a parameter byte plus big-endian words at +4/+8/+0xC/+0x10
(16 bytes) and finishes the context. Register-held context (r14). [O]

#### 0x00003BF4 `lib_hash_digest_out_v2` — logic
Digest-output variant: 4 big-endian bytes of param_3 followed by the words at +0xC/+0x10
(8 bytes); finishes the context. [O]

#### 0x00003C04 `lib_hash_digest_out_v3` — logic
Digest-output variant: two high bytes of param_3 followed by the words at +0xC/+0x10;
finishes the context. These three variants feed different service response formats. [O]

#### 0x00003C9A `lib_ripemd160_init` — logic
Hash context init: clears 0xC0 bytes, loads the 5-word initial value from the constants at
0x00003D04-0x00003D14 (the "initial value at 0x3D04" of spec 06 §6.1), zeroes counters and
prepares padding (0x39E0). Called by 0x1F30, 0x314E and 0x23F0. [O]

#### 0x00003CD2 `lib_hash_absorb_loop` — logic
Pumps the absorber 0x345A until the context state reaches 0xC. The one-call "update" wrapper
used by the auth services. [O]

#### 0x00003CF6 `lib_hash_final_loop` — logic
Cluster of 2 (0x3CF6 = 1-instruction tail-jump, 0x3CF8 = body): re-initialises padding
(0x39E0) and pumps the finaliser 0x3A04 until phase 6. Called by 0x243E, 0x1F30, 0x314E. [O]

#### 0x00003D1C `lib_hash_final_loop_ctx` — logic
Finalise loop variant with the context held in r14 instead of a parameter. [O]

#### 0x00003D2C `lib_memset` — library
Byte-wise memory fill (dst, value, count). Used throughout the auth services. [O]

#### 0x00003D3A `lib_memcpy_alt` — library
Byte-wise forward copy (dst, src, count) — same semantics as 0x136E but a separate code
variant; the most-called helper in block 0 (8+ resolved callers). [O]

#### 0x00003D48 `lib_crc32_bitwise` — logic
Bitwise CRC-32: init 0xFFFFFFFF, reflected algorithm, polynomial from the constant at
0x3D80, final complement — the same CRC-32 as the block headers (spec 06 §2). [O]

#### 0x00003D84 `boot_collect_entropy` — logic
Entropy collector for the challenge: configures clock-gate registers 0xFFFFC83x, samples the
free-running value at 0xFFFFF220 and ADC0 status bits (0xFFFFE800+0x3E/+0x40) across 2x16
rounds, packing the sampled bits into two 32-bit words; uses the ADC init 0x2380/0x23D8 and
the RSPI-B path 0x22CC. Its 8 bytes are the challenge staged by 0x3118. [I]

#### 0x00003EFE `stub_zero_ctx_undecoded` — stub
One-instruction entry that zeroes a register-held context (+0, +0x10 and two globals) before
hitting undecodable bytes. Purpose not recoverable. [I]

#### 0x00004098 `bootdata_blob_4098` — unknown
Data-area fragment (boot data block 0x4000-0xFFFF: CAN profiles, marker list, key material,
spec 06 §1) decoded as a 1-instruction "function" that hits bad instruction data. Not real
code; kept for inventory completeness. [O]

#### 0x000041B4 `bootdata_blob_41b4` — unknown
Same as 0x4098: data fragment near the bit-inverted key identifiers at 0x41D4 (spec 06
§6.0a), truncated by bad instruction data. [O]

#### 0x00004220 `lib_hash_fill_3byte` — logic
Hash block fill for 3-byte groups: ORs a GBR-relative byte with 0xCE then assembles block
words from (lo16, byte, byte) triples; tail shares the standard absorb bookkeeping and 0x39D8
state set. Part of the RIPEMD-160 absorb machinery. [I]

#### 0x0000437E `bootdata_blob_437e` — unknown
Data-area fragment whose "call" to 0x4120 is an artifact of decoding the key tables; body
hits bad instruction data. Not real code. [O]

#### 0x00004734 `bootdata_blob_4734` — unknown
Two-instruction data fragment (single store) in the marker/CAN-profile area; truncated. [O]

#### 0x000048A0 `bootdata_blob_48a0` — unknown
Data fragment (single byte store) in the boot data block; truncated by bad data. [O]

#### 0x00004A54 `bootdata_blob_4a54` — unknown
Data fragment (single byte store) inside the CAN-profile/key area; truncated. [O]

#### 0x00004B90 `bootdata_blob_4b90` — unknown
Data fragment writing 0xFF relative to 0x437FC — constant table bytes decoded as code;
truncated. [O]

#### 0x00004C4A `bootdata_blob_4c4a` — unknown
Data fragment: a handful of 0xFF/0xFC stores and an indirect call via TBR+0xFF0 (vector
references) before bad data — almost certainly table bytes, not executable code. [O]

#### 0x00004C60 `bootdata_blob_4c60` — unknown
Data fragment with an indirect TBR+0xFF0 call and immediate bad data — table bytes decoded as
code. [O]

#### 0x00004CC2 `bootdata_undecoded_fn` — unknown
Cluster of 13 copies (0x4CC2, 0x4D04, 0x4E2A, 0x5512, 0x201A0, 0x20B4C, 0x20B5C, 0x2144C,
0x4A04A, 0x4A0EA, 0x4A242, 0x4A39E, 0x4A3A2) of a 17-"instruction" body that hits bad
instruction data immediately — table/config bytes promoted to functions, duplicated across the
boot data block, boot manager and application areas. Not analysable further. [O]

#### 0x00004D68 `bootdata_frag_tbr_call` — unknown
Cluster of 3 (0x4D68, 0x20888, 0x44878): data fragment with an indirect call through TBR+0xD30
(vector area) and one store before bad data. Decode artifact. [O]

#### 0x00004D7C `bootdata_frag_store4` — unknown
Cluster of 3 (0x4D7C, 0x2089C, 0x4488C): single-store data fragment; bad instruction data. [O]

#### 0x00004E02 `bootdata_frag_flash_4e02` — unknown
Cluster of 2 (0x4E02, 0x2091C): fragment storing a word GBR+0xC2 and a context word before
calling the FCU buffer writer 0x5AD8 and hitting bad data — flash-driver-adjacent bytes. [O]

#### 0x00004E72 `bootdata_frag_flash_4e72` — unknown
Register-flag variant that calls 0x5012 then the FCU writer 0x5AD8 before bad data; same
flash-adjacent table/fragment family. [O]

#### 0x00004F4E `bootdata_frag_4f4e` — unknown
Fragment writing 0x70 and repeated 0xF8 bytes (fill pattern) before bad data. [O]

#### 0x00004FFE `bootdata_frag_4ffe` — unknown
Fragment (FPU register stores, GBR |= 0x31) calling the erase-list builder 0x5BA4 and the
vector area via TBR+0xFF0 before bad data. [O]

#### 0x00005012 `bootdata_frag_5012` — unknown
Cluster of 3 (0x5012, 0x5682, 0x5794): 0x70/0xF8 fill-store fragment; bad instruction data. [O]

#### 0x00005642 `bootdata_frag_5642` — unknown
Fill-store fragment referencing 0x55F8 and a unique global; bad instruction data. [O]

#### 0x0000580C `bootdata_frag_580c` — unknown
Two-store fragment (0xF8 fill); bad instruction data. [O]

#### 0x00005830 `flash_fcu_reset` — logic
Flash-controller (FCU) error reset: 1000-iteration delay, then 0xFFFFA810 = 0,
0xFFFFA902 = 0xAA00, 0xFFFFA906 = 0xCC00 — the FCU command-register block based at
0xFFFFA800. Cluster of 2 (0x5830, 0x3EF38). [O]

#### 0x0000584A `flash_fcu_cmd_param` — logic
Delay loop then FCU register writes through a 0xFFFFA800-based context: status clear (+0x10),
command word param_1 to +0x102 and 0xCC00 to +0x106. [O]

#### 0x00005868 `flash_fcu_wait_ready` — logic
FCU ready handshake: merges the status byte at 0xFFFFA810, and while bit 0x10 (busy) is set
writes 0x50 (status clear) up to 0xFF times; on timeout runs the FCU reset 0x5830; finishes
with 0xFFFFA902 = 0xAA00. Cluster of 2 (0x5868, 0x3EF70). [O]

#### 0x00005884 `flash_fcu_wait_ready_ctx` — logic
Register-context twin of 0x5868: polls the busy bit at ctx+0x10, writes command bytes, times
out into 0x5830 and sets the AA00 command word. [O]

#### 0x000058BA `flash_erase_block_cmd` — logic
Builds the FCU erase-block select word from a target address: group bits by 1 MB region
(1 << (addr >> 20)) or the (addr & 0xFFFFF) >> 0x12 shift table, gated by the erase-mode flag
0xFFF85A75 and window 0xFFF85A74; writes the AA00-series select to 0xFFFFA902 and the
erase command 0x5501 to 0xFFFFA904. [O]

#### 0x0000592C `flash_prog_select_cmd` — logic
Computes the FCU program/select word (bit position = param_2 >> 2) ORed with param_4 into
+0x102, writes 0x5501 to +0x104 and adjusts a program-range word — the program-side twin of
0x58BA. [O]

#### 0x00005AD8 `flash_fcu_write_buf` — logic
FCU buffer program: issues command 0x80, streams a 128-word buffer into the data register,
terminates with 0xD0, then loops a 0x32-round verify/status sequence (via 0x5BA4) and closes
with the ready wait 0x5868. Writes through a RAM function pointer at 0xFFF863C4. [O]

#### 0x00005BA4 `flash_erase_list_build` — logic
Walks the enabled-erase-group mask (config word *(ctx+0x40) & 0xFFF85774), computes each
block's FCU address select (11-bit field >> 0x12), fills the group table at 0xFFF858EC and
advances the group counter 0xFFF85AAC. Called by 0x4FFE and the FCU writer. [O]

#### 0x000061D2 `bootdata_frag_sleep` — unknown
Single Sleep_Standby call followed by bad instruction data — data-area fragment near the block
descriptor at 0x6000. [O]

#### 0x0000624C `bootblk_get_count` — data-accessor
Boot-data block-descriptor service (spec 06 §1/§5a table at 0x60B4): zeroes the status word
and returns the block count from PTR_DAT_00006354. Called by the bootloader flash jobs 0x190CC,
0x18540 and 0x18D48. [O]

#### 0x0000625A `bootblk_get_descriptor` — logic
Returns block descriptor idx: range-check against the count byte (error 0xFFF1), then copies
the 0x44-byte table entry fields — full range start (+0x18)/end (+0x1C), checked length,
marker addresses (+0x14/+0x16) — from the table pointer at +0x14 of the descriptor root.
Matches the logical block table documented in spec 06 §1. Callers 0x190CC, 0x18540, 0x18774,
0x18D48. [O]

#### 0x000062F6 `bootblk_write_marker8` — logic
Runs a precondition callback (PTR_FUN_0000635C) and, when it passes, writes 8 bytes through
the service at *(PTR_PTR_00006360 + 0x24) — the marker-write entry used by the bootloader's
0x1BEC4. Callees 0x25334/0x243FC live in the boot manager area. [O]

#### 0x0000631A `bootblk_get_group_bits` — logic
For block index idx (range-checked like 0x625A) returns the FCU erase-group bits: the
descriptor word >> 0x10 OR (idx ? 1 << idx : 0). Used by the bootloader's address-range check
0x18540 before erase/download. [O]

#### 0x000105A6 `bootmgr_handover_route` — logic
Boot-side handover router: reads the boot request record target byte at 0xFFF800AB (record at
0xFFF80080 + 0x2B, spec 06 §5a) — target 3 selects state 3 (or 0 when flag bit 1 of the
0x6010-rooted config is set), target 2 selects 0/5 (flag bit 0) — stores the choice into the
state at PTR_DAT_00010638, sets the phase word and jumps into the bootloader entry 0x10644.
Called by the boot manager's 0x25374 (the 0x24B38 entry family). [O]

#### 0x00010644 `bootloader_entry_main` — entry
Bootloader startup and main loop: RAM march-test over 0xFFF88000 (pointer 0x106CC), CMT clock
gate 0xFFFE1200 bit clear, DMAC stop 0xFFFE6000 = 0, 0xFFFE080E = 0xE000, then a chain of
module inits through the pointer table 0x106D0-0x10704 (resolved: 0x108DC, 0x1096C, 0x10986,
0x109F4, 0x10AB0 and the 0x1CFF0/0x1D0xx/0x1DAxx/0x1F394 diag/flash services) and finally an
infinite scheduler loop (never returns). [O]

#### 0x00010870 `bootbl_status_msg_build` — logic
Builds the bootloader status message: stores 0xA5 plus 0x90 (state 4) or 0x91 (state 5) at the
buffer behind PTR_DAT_000108d8, registers the payload with 0x10B0C(0x42, buf) and transmits it
via 0x10B3A. [I]

#### 0x000108A4 `bootbl_status_buf_fill` — logic
Copies six argument bytes into the status buffer at PTR_DAT_000108d8 (same buffer 0x10B0C
serialises). [O]

#### 0x000108DC `lib_empty_hook` — stub
Empty body (single RTS). Cluster of 131 copies across every region — default hook / unused
slot filler, called once by the bootloader init chain 0x10644. [O]

#### 0x00010970 `boot_ram_readback_init` — logic
RAM error-check initialisation: reads every word in a range and writes it back (spec 06 §5a
documents the boot manager's copy at 0x2242C over 0xFFF80000-0xFFF800FF). Cluster of 4
(0x10970 bootloader, 0x2242C boot manager, 0x3DF42, 0x6768C application area). [O]

#### 0x00010986 `boot_scatter_load` — logic
Bootloader startup loader: readback-init 0xFFF80000-0xFFF800FF (0x10970), zero-fills
0xFFF84000+0x3400 (0x10DF4), then walks the 0xC-byte (src/end/dst) scatter table at
PTR_PTR_000109EC copying initialised data into RAM until a null/empty entry. Called from the
entry chain 0x10644. [O]

#### 0x000109F4 `boot_selftest_crc_check` — logic
Boot self-test: runs the peripheral RAM tests 0x10D28 over 0xFFFFD000 / 0xFFFFD800 /
0xFFFFE000, ORs the failures into bits 1/2/4 at DAT_fff84e78[1] (error code 0x219), then
configures 0xFFFFA854 = 0xC401 and integrity-checks the 0x1800/0x2000-byte region behind
0x10A80 through PTR_FUN_00010a84 (error code 0x218 when it fails). [O]

#### 0x00010AB0 `boot_session_state_clear` — logic
Clears the three session-state globals (0xFFF84E7C byte 3, 0xFFF86017, and the pointer target
at 0x10B08) and starts the scheduler loop 0x10E58. [O]

#### 0x00010AD0 `boot_session_change_notify` — logic
On a session-index change at DAT_fff84e7c (skipping states 0 and 3), latches it into
0xFFF86017 and the pointer at 0x10B08, then reports it through 0x10E36 and 0x10E98(x, state, 2). [O]

#### 0x00010B0C `bootbl_status_msg_set` — logic
Serialises a status payload: stores the length byte at 0xFFF840DC, sets the frame length at
DAT_fff84e7c[1] (2 without data, 8 with) and copies up to 6 data bytes to 0xFFF840DE-0xFFF840E3. [O]

#### 0x00010B3A `bootbl_status_msg_send` — logic
Transmits the status frame through PTR_FUN_00010b6c with the CAN id looked up at
0xFFF85FEA + state*2, the buffer 0xFFF840D8 and the payload at 0xFFF840DC (reaching the
bootloader CAN layer 0x1D6E0). [O]

#### 0x00010B70 `boot_call_via_10c80` — logic
Indirect call through the service pointer at 0x10C80 with resolved callees 0x1DC2C, 0x1D478,
0x1E284 and the flag check 0x10C8C; called from 0x109F4, 0x1D224, 0x149E4 and 0x1D1C8. Likely
the CAN/diag pump entry of the bootloader. [I]

#### 0x00010C8C `boot_flag_ffff1001_40` — data-accessor
Returns whether bit 0x40 of the register at 0xFFFF1001 is set. [O]

#### 0x00010C9C `boot_ram_march_words` — logic
Word-wise march test over param_2 bytes: zero-fill, 0xFFFFFFFF fill, readback verify from both
ends, writing the DAT_00010d24 background pattern — the integrity check used by 0x109F4 over
the FCU/data window behind 0x10A80. [O]

#### 0x00010D28 `boot_periph_ram_test` — logic
March test of an on-chip peripheral RAM block: sets the start marker, runs 32 rounds of
16-halfword write/readback (zeros then 0xFFFF) over param+0x100.., returning failure position.
Used on 0xFFFFD000/0xD800/0xE000. [O]

#### 0x00010DF4 `lib_zero16_stride` — library
Unrolled zero fill (4 words per iteration, 16-byte stride). Cluster of 4: 0x10DF4, 0x248FC,
0x3E200, 0x68E88 — the .bss clearer duplicated per region. [O]

#### 0x00010E0C `boot_tbl_task_ptr_get` — data-accessor
Returns the task-group pointer from the 4x2 table behind PTR_PTR_00010e50 (default 0x10E4C). [O]

#### 0x00010E36 `boot_tbl_tasklist_get` — data-accessor
Returns task-list pointer param_1 (< 4) from the table at PTR_PTR_00010e54. [O]

#### 0x00010E58 `boot_sched_loop` — logic
Bootloader cooperative scheduler: runs task-list 0 via 0x10E98(...,2), the periodic hook at
0x10E90, then loops forever — tick counter 0xFFF84E2C++, fetch the state's task group
(0x10E0C) and run it (0x10E98(...,0)) with the 0x10E94 hook. Entered from 0x10AB0. [O]

#### 0x00010E98 `boot_tasklist_run` — logic
Runs one task list: picks the entry from the table at PTR_PTR_00010ed0 (+0x10 per class),
takes the resource via 0x11B44, then calls every function pointer until a null terminator,
re-locking around each. [O]

#### 0x0001168E `diagbl_session_control` — logic
DiagnosticSessionControl handler of the bootloader: session structures at DAT_fff84128
(0x28-byte entries, timing table at +4, sizes at +8); sub-function 2 clears the pending/timer
bytes and bit 0x20 then calls 0x12088; sub-function 1 starts a session change (NRC 0x0E
stored when disallowed), building the 0x32/0x600+sub response record with the P2 timing. [I]

#### 0x000117E8 `diagbl_session_enter` — logic
Second session service: searches the 10-byte session list at DAT_fff84128+4 for the requested
index, requires state 2, and assembles the timing parameter record (0x20-flagged response
bytes, P2/P2* from the entry) using 0x12196/0x1231E. [I]

#### 0x00011B30 `boot_timestamp_snap` — stub
Stores the free-running timer value at 0xFFFFF220 into the word behind PTR_PTR_00011b40. [O]

#### 0x00011B44 `boot_deadline_check` — logic
Walks a deadline list; for every entry whose time (0xFFFFF220 - stored) has expired fires
0x11B74. Cluster of 2 (0x11B44, 0x270D0 in the boot manager). [O]

#### 0x00011B74 `boot_timer_task_fire` — logic
Timer-task dispatch: swaps the current period into 0xFFF84104, updates the ms display counter
0xFFF84E2C[2..3], reschedules (*entry += period, tick+1), runs the task via 0x10E0C/0x10E98
and restores the period. [O]

#### 0x00011BE8 `boot_stub_call_11bfc` — stub
Four-instruction wrapper calling (PTR_FUN_00011bfc)(PTR_LAB_00011bf8); called from 0x1A4B2,
reaching 0x13A74. [O]

#### 0x00011C00 `diagbl_get_flag_e801` — data-accessor
Returns session-state byte 0xFFF84E80+1. Part of the bootloader's session getter/setter API
around the block written by 0xB88. [O]

#### 0x00011C08 `diagbl_set_flag_e802` — data-accessor
Sets session-state byte 0xFFF84E80+2. [O]

#### 0x00011C10 `diagbl_get_flag_e802` — data-accessor
Returns session-state byte 0xFFF84E80+2. [O]

#### 0x00011C18 `diagbl_get_state_word` — data-accessor
Returns the word behind PTR_DAT_00011da8 (session flag word). [O]

#### 0x00011C1E `diagbl_set_session_index` — data-accessor
Stores the session index into 0xFFF84E80+0 and the mask 1 << index into the word at
PTR_DAT_00011dac. [O]

#### 0x00011C30 `diagbl_get_session_index` — data-accessor
Returns session index byte 0xFFF84E80+0. [O]

#### 0x00011C38 `diagbl_session_bit_test` — data-accessor
Tests a bit of the session mask word at PTR_DAT_00011dac ("is session N active"). [O]

#### 0x00011C42 `diagbl_set_byte_e803` — data-accessor
Sets session-state byte 0xFFF84E80+3. [O]

#### 0x00011C4A `diagbl_get_byte_e80_signed` — data-accessor
Returns (signed char)0xFFF84E80. [O]

#### 0x00011C52 `diagbl_set_byte_e84` — data-accessor
Sets the state byte at 0xFFF84E84. [O]

#### 0x00011C5A `diagbl_get_byte_e84` — data-accessor
Returns the state byte at 0xFFF84E84. [O]

#### 0x00011C62 `diagbl_set_target_from_record` — data-accessor
Sets 0xFFF84E80+2 from the boot request record target byte 0xFFF800AB and clears the flag word
at 0x11DA8 — hooking the record (spec 06 §5a) into the session state. [O]

#### 0x00011DD0 `boot_call_init_ptrs` — logic
Runs three indirect init hooks (PTR_FUN 0x11EDC/0x11EE4/0x11EE8; resolved callees 0x13ED2,
0x13EB2, 0x13E20). Part of the bootloader startup chain; called from 0x1A4B2. [O]

#### 0x00011DE4 `diagbl_did_session_status` — logic
DID handler: reads one of three status bytes from the config at *DAT_00011eec (+7 for mode 2,
+8 for 1, +9 for 3) into the response (suppress-bit 0x80 at +0xB and nonzero length select the
1- or 2-byte form), sets state 2. [O]

#### 0x00011E34 `diagbl_record_write_source3` — logic
Boot request record writer, exactly as documented in spec 06 §5a: refreshes the timing value
at +0x18 via 0x11EF4 when the flag at +0x25 is 1, sets header 0xA55A, length 0x48, source 3,
clears +0x18, picks target 1 (bootloader) when the requested target at +0x2B is 2 or 3 else 2,
and writes the 0x5AA5 trailer at +0x44. Calls 0x13BC8. [O]

#### 0x00011E7A `diagbl_record_store_response` — logic
Record/response update: swaps the source/target bytes at 0xFFF80084/0xFFF80085, sets +0x35 = 1
(complete positive response stored), +0x36 = 3, +0x37 = 0x7F (TesterPresent-style SID), keeps
the old +0x38, and writes 0x22/3 at +0x39/+0x36-area bytes (spec 06 §5a record layout). [O]

#### 0x00011EA6 `diagbl_record_is_pending` — logic
Documented check (spec 06 §5a): returns 1 when the record target byte at +5 is 1 (bootloader)
and +0x24 == 'w' — i.e. answer the stored request and stay in the bootloader. [O]

#### 0x00011EBE `diagbl_record_clear` — logic
Record clear (spec 06 §5a): header 0, length 0, source/target 0xFF, +0x44 = 0, +0x24 = 0. [O]

#### 0x00011EF8 `lib_thunk_indirect` — stub
Three-instruction indirect thunk through PTR_FUN_00011fac into 0x149BC. Cluster of 29 copies
across the bootloader and other regions (vtable glue). [O]

#### 0x00011EFE `diagbl_tbl_dispatch1` — dispatch
Calls (**(DAT_fff84194 + 0x24)) with the idx-th entry of the table at *(param_1+8) — single
entry dispatch through the diag configuration at 0xFFF84194. [O]

#### 0x00011F1A `flashbl_range_check_wrap` — logic
Flash address-range gate used by the download path: mode 0 checks via PTR_FUN_00011fb0
(0x18540) with the 0x20 flag through 0x11FB8 (0x1822C), honoring the 0x2A permission bits;
mode 1 checks with the 0x20 flag; returns 0/1/2/3. Callees 0x18540, 0x18902, 0x1822C. [O]

#### 0x00011F8E `flashbl_op_0x40_wrap` — logic
Thin wrapper calling PTR_FUN_00011fbc (0x17EA0) with flag 0x40; maps success to 1, failure to
2. [O]

#### 0x00011FA8 `lib_return_true` — stub
Two-instruction constant-true function. Cluster of 16 copies — default "permitted" slot
filler. [O]

#### 0x00011FC0 `diagbl_session_table_init` — logic
Installs the session configuration: DAT_fff84128 = param_1, session count from *(param_1+1);
for each 0x28-byte session entry clears the timer/status bytes (+0x19..+0x1D, +0x23) and sets
+0x18 = 1. Called by 0x12318. [O]

#### 0x00012000 `diagbl_session_timing_set` — logic
Writes the timing pointer and value into session entry +0xC/+0x10 (optional parameters at
+0x1C/+0x1D), then toggles the +0x18 mode bits (2 or 0x82|&0x20) around the 0x121E4/0x121E8
callbacks. Called from 0x1A3B8. [O]

#### 0x0001205A `diagbl_sess_flags_clear` — logic
Clears the given bits (param_2) from session entry +0x18 and returns the previous value;
bracketed by the lock/unlock callbacks 0x121E4/0x121E8 (0x1EDDC/0x1EDFA). Called by 0x1A3D8. [O]

#### 0x00012088 `diagbl_sess_substate_set` — logic
Sets the low nibble of session entry +0x18 to param_2 (keeping the high nibble) and clears the
+0x19/+0x1A timer bytes. Called by the session handler 0x1168E. [O]

#### 0x000120B8 `diagbl_sess_bit40_consume` — logic
Test-and-clear of bit 0x40 in session entry +0x18 (returns the old bit). Called by 0x1A3FC. [O]

#### 0x000120E2 `diagbl_sess_get_short14` — data-accessor
Returns the 16-bit field at session entry +0x14. Called by 0x1A410. [O]

#### 0x000120F6 `diagbl_sess_get_ptr0c` — data-accessor
Returns the timing/data pointer at session entry +0xC. Called by 0x1A424. [O]

#### 0x0001210A `diagbl_sess_get_byte1c` — data-accessor
Returns the byte at session entry +0x1C. Cluster of 2 (0x1210A, 0x1211E). Called by 0x1A438. [O]

#### 0x00012134 `diagbl_sess_timing_swap` — logic
Copies the old timing record out (byte copy 0x12196), installs the new buffer/length at entry
+0xC/+0x10, clears bit 8 of +0x18 and — when the size-table flag at +0x17 bit 0 is set — arms
the timer (+0x19 = 0x40, +4 = 0xFFFFF220). Called by 0x1A46E. [O]

#### 0x00012196 `lib_memcpy_thunk2` — library
Thunk into the byte-copy 0x121A0. Cluster of 3 (0x12196, 0x26396, 0x74F0C). [O]

#### 0x000121A0 `lib_memcpy3` — library
Byte-wise copy loop (dst, src, count). Cluster of 3 (0x121A0, 0x263A0, 0x74F18). [O]

#### 0x000121A8 `diagbl_sess_get_word0` — data-accessor
Returns the first word of the 0x28-byte session entry. Called by 0x1A490. [O]

#### 0x000121BA `diagbl_sess_set_state1b2` — data-accessor
Sets session entry +0x1B = 2 inside the lock pair. Called by 0x1A4A4. [O]

#### 0x00012318 `diagbl_session_config_install` — stub
Installs the bootloader's session configuration table (pointer at 0x1236C) via 0x11FC0. [O]

#### 0x0001231E `diagbl_send_response` — logic
Sends a diagnostic response: optionally pads the frame to 8 bytes with 0xAA, then hands
(length, buffer) to the CAN transmit at PTR_FUN_00012370 (0x1D6E0). [O]

#### 0x00012350 `diagbl_is_testerpresent_3e80` — logic
Returns 1 when the request is SID 0x3E with sub-function 0x80 (TesterPresent, no response) —
the S3-keepalive detector. [O]

#### 0x00012384 `diagbl_session_engine` — logic
Bootloader session/timing engine: 15000-unit timeout against the 0xFFFFFF48 timer, states at
ctx+0x16 (1 idle/listen with 0x1E check, 2, 3/5 response phases via PTR_FUN_000125CC with
0xFFF841D8, 6, 8), chaining into 0x125E0, 0x15A5E, 0x163A4, 0x14ACC, 0x14A70 and the CAN layer
(0x1E4FE, 0x1E69A, 0x1E8A8, 0x1E8F8). The S3-response-pending machinery. [I]

#### 0x000125E0 `diagbl_resp_flag_update` — logic
Copies the state word 0xFFF85D64 into 0xFFF84EA8, setting bit 0x80 when the flag at
*(0xFFF85D88)+1 bit 1 is set — response status propagation for the CAN layer. [O]

#### 0x000125FA `diagbl_counter_bump` — logic
Adds min(param,3) (+1 when *0xFFF85D88 has bit 0x20) to the counter at 0x1277C and sets bit 0
of the state byte — retry/sequence counter. Called by 0x1CFA0. [O]

#### 0x00012F88 `diagbl_calc_total_length` — logic
Computes the total frame count/length: (0xFFF84E4C - 0xFFF84E48) plus protocol flags, plus
per-message extra bytes (bits 0x40/0x20/0x10 of each 0xC-stride entry at 0xFFF85D58 table of
0xFFF85D5C entries) — multi-frame response sizing. [O]

#### 0x0001301C `flashbl_job2_start` — logic
Job start (job type 2, the upload/read direction): copies (type, tag, start +4, end +8, extra
+0xC) into the job block at 0x13224, sets state +0x14 = 2, runs the start callback at
0x1322C and evaluates the result (1 -> ok, else poll via 0x13230). Called by 0x18AEE/0x18A9C. [O]

#### 0x00013086 `flashbl_job1_start` — logic
Job start (type 1, the download/program direction): same job-block layout, state +0x14 = 1,
clears the byte counter 0xFFF84E4C[2..3] and calls the 0x13234 setup first. Callers
0x1323C and 0x18C7C. [O]

#### 0x000130F8 `flashbl_chunk_move` — logic
Moves one chunk of an active job (states 1/4/5): chunk = min(0x800, remaining), further capped
by the 0x6000 - 0xFFF85FEE buffer watermark, data moved via PTR_FUN_00013238 (0x17DA0),
pointers advanced and remaining decremented; state 8 aborts. Called by 0x18C54, 0x1323C,
0x18CC8. [O]

#### 0x0001319A `flashbl_job_finish_check` — logic
Job completion check: requires current+0xC == end-1; on job type 5 pads the last 0x100-byte
block with 0xFF; types 1/4/6 continue (1), type 7 finishes clean (0), type 8 fails (3). [O]

#### 0x0001323C `flashbl_job_engine` — logic
Job state machine driven from the flash-session services 0x18774/0x1886E: phase 0 starts job
type 1 (0x13086), phase 1 fills the 256-byte staging buffer with the fill byte and pumps
0x130F8, phase 2 continues — coordinating the whole erase/program/verify transfer. [O]

#### 0x000132C6 `flashbl_job_status_map` — logic
Maps the job state at 0xFFF86024: 7 -> 0 (done, cleared), 1/2/3/6 -> 2 (busy), 8 -> 3 (failed,
cleared), else 4. Called by 0x18A9C. [O]

#### 0x00013470 `flashbl_job_cancel` — stub
Runs the cancel callback at 0x1350C (0x17E16) and clears the job state 0xFFF86024. [O]

#### 0x00013482 `bootbl_hash_segment_list` — logic
Hash-job runner: initialises the context via 0x13510, hashes (start, len) for the primary range
(param+8/+0xC) and then walks the 0x10-byte segment list at param+0x10 (next at +3, hashed
twice when the flag byte +2 is 1) through 0x13514, finalises with 0x13518 and stores the digest
byte at ctx+0x11, +0xF = 1. Callees 0x1ECF4/0x1ED50/0x1ED20 (RAM crypto services). [O]

#### 0x000134D8 `bootbl_hash_seed_set` — stub
Stores the value 0xFFF84914 into the hash context word 0 and marks +0xB = 1 (seeded). [O]

#### 0x00013650 `bootbl_mat_flag40_gate` — logic
MAT-bank-aware gate: computes the mirror offset (*(int*)0x1376C * (1 - idx/2)) against the
0x6010-rooted descriptor, and when its flag byte at +0x2F has bit 0x40 set calls the handler at
0x13770 (0x195E0). The offset trick makes the same code work with the user boot MAT mapped
(ROMMAT, spec 06 §8). [I]

#### 0x0001367E `bootbl_markers_validate` — logic
Fetches the two marker ranges via 0x1370C and validates them through PTR_FUN_00013774 first
with 0xFFFFFFFF then 0 — the marker-area check of spec 17 §5n (0x5E00/0x7E00 pairs). [O]

#### 0x000136B2 `bootbl_marker_tbl_resolve` — logic
Resolves the marker list for MAT bank idx: even banks use the fixed 0x6034/0x604C area
(default 0x13778), odd banks read the boot-manager header root (PTR_hdr_block_0x20000 at
0x6008) +0x34/+0x35 and the pointer at +0x4C, dereferencing +0x14 — then hands (list, count)
to PTR_FUN_0001377c. [O]

#### 0x0001370C `bootbl_marker_ranges_get` — logic
Returns the marker range pair: even index -> fixed 0x7E00 (len 0x100) and 0x7F00 (len 0x100)
(matching the marker areas of specs 06 §1 and 17 §5n); odd index -> the two ranges from the
0x6010-rooted block descriptor, MAT-offset-adjusted. [O]

#### 0x00013780 `bootbl_bg_job_dispatch` — logic
The background job dispatcher documented in spec 06 §6.1: priority-ordered — when
0xFFF85FF3 == 1 runs *0x13910 and re-evaluates; otherwise one of the seven handlers at
0x138F8-0x1390C by the flags 0xFFF85FFA/F6/F4/F8/F9/F2. Called by the flash-session jobs
0x190CC, 0x184B0, 0x18C54, 0x1839C, 0x18A9C, 0x18B80 and 0x18D48. [O]

#### 0x0001380C `bootbl_jobs_shutdown` — logic
Shutdown chain: acts on the record field at 0xFFF80034 (values 1/3/4/5 — the record's
restore-session area, spec 06 §5a), runs the 0x13914/0x13918/0x1391C callbacks, timestamps
0xFFF8405C from 0xFFFFF220 and finally clears all seven job flags 0xFFF85FF2-0xFFF85FFA.
Calles 0x19856, 0x1EE14, 0x189D8, 0x11E7A, 0x18A0A, 0x1D7F0, 0x189BC. [O]

#### 0x0001389A `bootbl_session_watchdog` — logic
Communication watchdog: if a session timestamp 0xFFF8405C is set and 500000 ticks of
0xFFFFF220 elapsed, fires the timeout handler 0x13938; otherwise toggles the connect state
0xFFF84E74[1] with codes 0x31/0x19 through 0x13934 (only when 0xFFF84178 permits). [O]

#### 0x0001393C `bootbl_rx_state_get` — logic
Reads the CAN receive state: types 1/3 copy length DAT_fff85d0d and the payload bytes from
0xFFF85D05 plus the fields at 0xFFF85D00/0xFFF85D04; type 4 returns the pair 0xFFF85CF8/
0xFFF85CFC; otherwise clears. [O]

#### 0x000139F0 `bootbl_vt_call_c` — dispatch
Single vtable call: (**(*(PTR_PTR_00013a54 + 0x10) + 0xC))(PTR_DAT_00013a58) — jumptable not
recovered by Ghidra. Called by 0x1CD12. [O]

#### 0x000139FE `bootbl_vt_call8_cond` — logic
Sets 0xFFF856F6 from the 0xFFF84946 flag, runs PTR_FUN_00013a5c (0x1D0B4), and when
0xFFF86028 == 1 dispatches the vtable +8 entry with (0xFFF84A44, 0x13A58). [O]

#### 0x00013A34 `bootbl_vt_call_4` — dispatch
Vtable +4 call through the 0x13A54 service object. Called by 0x1CB94. [O]

#### 0x00013A42 `bootbl_vt_call_0` — dispatch
Vtable +0 call with (0xFFF84A44, PTR_DAT_00013a58). [O]

#### 0x00013A60 `bootbl_jobtbl_install` — data-accessor
Registers a job table: 0xFFF84170 = *(param+4) (entries), 0xFFF8416C = param,
0xFFF84E88[1] = *param (count). [O]

#### 0x00013A74 `bootbl_diag_jobs_install` — logic
Installs the bootloader's diagnostic job infrastructure: 0x13A60, then registers each 0x30-byte
job entry through 0x13AF8 (using the +0x18 timing table) and fills the service function block
0xFFF84134-0xFFF84168 with pointers to 0x13AFC-0x13B2C. Called from the startup chain via
0x11BE8 (0x1A4B2). [O]

#### 0x00013B30 `bootbl_addr_to_buffer_map` — logic
Maps a code address to its verification buffer: 0x77C04-0x7C63C -> 0x7A120,
0xEF808-0xF8C78 -> 0xF4240, 0x17F35-0x18E0B -> 0x186A0, 0x3BE03-0x3E31D -> 0x3D090,
0x1DF02-0x1F18E -> 0x1E848; out-of-range addresses return 1 with no buffer. Stores the result
in 0xFFF84BA0 — the address->reference-data lookup used by the verification jobs. [O]

#### 0x00013BBE `bootbl_flag_set_856cf` — stub
Sets bit 0x10 of the state byte 0xFFF856CF. [O]

#### 0x00013BC8 `bootbl_timing_from_record` — data-accessor
Returns the 0xC-offset word of the 0x30-byte job entry selected by the record's session byte
0xFFF800B0 (record +0x30, spec 06 §5a). Called by the record writer 0x11E34. [O]

#### 0x00013BE0 `bootbl_set_ptr_13df0` — stub
Three-instruction store of param_1 into PTR_DAT_00013df0. Called by 0x19C90. [O]

## Unit summary

**220 clusters -> 512 named functions** (reps + members; identical duplicated helpers share one
name). Kinds by name row: stub 266 (dominated by the 131-copy `lib_empty_hook` and 73-copy
`lib_tramp_undecoded` clusters), logic 130, library 39, data-accessor 31, dispatch 6, entry 1,
unknown 39.

**Region map established (all anchored to spec 06):**
- 0x0000-0x3FFF block 0 = Bosch Auth_TSW module `bhie0101` (spec 06 §6.0a): UDS-style dispatch
  (0x774/0x7AA), DID read (0x1404/0x1624), RAM upload gate (0x1A66/0x16F0/0xD2C), the ten
  service-table entries (0x1F18...0x2540), the TPROT verify engine (0x1F30/0x23F0/0x243E/0x24C8),
  a full RIPEMD-160 (0x345A core — spec 06 called it SHA-1; the twin-line compression at 0x3218
  and spec 17 §5 prove RIPEMD-160), RSA-1024 e=3 bignum verify (0x281A/0x2E7A/0x2B5C + lib
  primitives), CRC-32 (0x3D48), entropy/challenge collection (0x3D84), EEPROM key staging
  (0x2320/0x2540/0x3118), bit-inverted name handling (0x1034/0x104C).
- 0x4000-0xFFFF boot data block: FCU flash driver services (0x5830-0x5BA4) and the logical
  block table API (0x624C/0x625A/0x62F6/0x631A). Everything else in 0x4C00-0x6200 that Ghidra
  listed as functions is data (CAN profiles, keys, marker lists) — 39 honest `unknown` rows.
- 0x10000-0x13BE0 bootloader start: entry+scatter load (0x10644/0x10986), self-test (0x109F4),
  cooperative scheduler (0x10E58), session layer (0x1168E/0x117E8/0x12384 + accessors
  0x11C00-0x11C62), boot request record writer/reader/clear (0x11E34/0x11EA6/0x11EBE — exactly
  as spec 06 §5a documents), flash job engine (0x1301C-0x13470), background job dispatcher
  (0x13780 — spec 06 §6.1), MAT-bank-aware marker logic (0x136B2/0x1370C).

**Unknown entries (39):** all data-area fragments decoded as code, in the boot data block and
its duplicated copies elsewhere: 0x00004098, 0x000041B4, 0x0000437E, 0x00004734, 0x000048A0,
0x00004A54, 0x00004B90, 0x00004C4A, 0x00004C60, 0x00004CC2, 0x00004D04, 0x00004E2A,
0x00005512, 0x000201A0, 0x00020B4C, 0x00020B5C, 0x0002144C, 0x0004A04A, 0x0004A0EA,
0x0004A242, 0x0004A39E, 0x0004A3A2 (undecoded 17-instr pattern, 13 copies),
0x00004D68, 0x00020888, 0x00044878, 0x00004D7C, 0x0002089C, 0x0004488C (TBR/store fragments),
0x00004E02, 0x0002091C, 0x00004E72, 0x00004F4E, 0x00004FFE, 0x00005012, 0x00005682,
0x00005794, 0x00005642, 0x0000580C (fill/flash-adjacent fragments), 0x000061D2.

**Cross-region leads for other units:**
- Bootloader functions **not in this unit** (this unit's clusters stop at 0x13BE0): the flash
  session services 0x18540 (address-range check), 0x190CC (post-download check), 0x18774,
  0x18A9C-0x18D48, 0x1BEC4, 0x1BD58 (verification state machine), 0x1CFF0-0x1EE14 (CAN/diag
  layer). They call heavily into this unit's 0x624C-0x631A block-table API and 0x13780
  dispatcher — whoever catalogs them should reuse the `bootblk_*`/`flashbl_*`/`bootbl_*` names.
- Boot manager (0x20000+): 0x2242C and 0x248FC are copies of this unit's 0x10970/0x10DF4
  (same clusters); 0x25374 calls 0x105A6 (handover router). 0x25680/0x2558A run the same Bosch
  diag library documented here.
- RAM functions 0xFFF8200E-0xFFF8202A (called by 0x63A) and the RAM window 0xFFF80100-0xFFF81FFF
  are the encrypted-session code area (specs 06 §6.0a, 17) — worth a `r_ram` unit cross-check.
- Application-area members found in this unit's clusters (memcpy 0x7D1A8/0x7D1B4,
  byte-sum 0x726AC, readback 0x6768C, RSPI init 0x9210C, empty hooks etc.) are duplicated
  library copies — the application units should keep the identical names.
