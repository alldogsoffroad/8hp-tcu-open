# Unit 19 (r08_app)

Region 0x000ADEC2–0x000B4046 of the application flash (r08_app). This band is dominated by the
UDS diagnostic data layer (DTC snapshot/status records, DID read handlers, 0xC3-entry DTC index
tables) plus generic calibration-table accessors and small object/virtual-call helpers used by
other regions.

---

#### 0x000ADEC2 `init_mirror_pair_read_trap_check` — logic
Reads two bytes via indirect getters into struct +0xd/+0xe (value and complement), latches the
value at +0x10, and on first run forces 0/0xFF defaults. When the value changes it reloads five
table rows via `PTR_FUN_000adfac(label, idx)` into globals at `PTR_DAT_000adfb0..fd0` plus a call
through `PTR_FUN_000adfd8`. If `+0xd != ~+0xe` it calls the fault reporter via `PTR_PTR_000adfe0`
with argument `'d'` and spins forever (trap). Called from init wrappers 0x000ADE40/0x000ADE60/
0x000ADE6E/0x000ACA10. Single-member cluster. [I]

#### 0x000ADFE8 `dtc_snap_read_status_u16` — data-accessor
Returns 0xFF when DTC-snapshot struct +0x14 (invalid flag) is set, else 0; copies the u16 at
+0x16 to the caller. Reached from DID-read wrapper 0x000A0D5A (UDS 0x22 path, callers in
0x9D/0x9E service handlers). [I]

#### 0x000ADFFC `dtc_snap_read_flag_u8` — data-accessor
Same pattern as 0x000ADFE8 for the byte at +0x11 gated by flag +0x10; returns 0xFF when the
flag is set. Called from DID wrapper 0x000A0D4A. [I]

#### 0x000AE012 `dtc_snap_read_u32_field40` — data-accessor
Copies the u32 at +0x40 of the snapshot record to *param_2 and always returns 0 (no invalid
flag). Called from DID wrapper 0x000A0D6A (UDS ReadDataByIdentifier path). [I]

#### 0x000AE01C `dtc_get_global_ctr_a` — data-accessor
Returns `*DAT_000ae0b4` (a global byte/word promoted to int). Called from wrapper 0x000A1104
whose caller 0x00144108 lives in another region (r14/r15 diag app). [I]

#### 0x000AE022 `dtc_get_global_ctr_b` — data-accessor
Identical pattern to 0x000AE01C reading `*DAT_000ae0b8`. [I]

#### 0x000AE028 `dtc_scan_flag_table_next` — logic
Starting at DTC index `param_2`, scans a 2-byte-per-entry flag table (base from 0x000AA80C,
which returns `*DAT_000aa870 + 0x50`) for the next entry with bit 3 (0x08) set, bounded by
0xC3 entries; returns the index or 0xFFFF. Called from 0x000A1120 (UDS ReadDTCInformation
support). Complements the s16 table lookups 0x000AE084/0x000AE09C. [I]

#### 0x000AE084 `dtc_table_s16_lookup_a` — data-accessor
Bounds-checks index < 0xC3 and returns the s16 entry from table `PTR_DAT_000ae0c0` (index*2),
else 0xFFFF. DTC-number-keyed table read. [I]

#### 0x000AE09C `dtc_table_s16_lookup_b` — data-accessor
Same as 0x000AE084 against table `PTR_DAT_000ae0c4`. [I]

#### 0x000AE0C8 `diag_did_read_dtc_status` — dispatch
DID-read handler (UDS 0x22 style): dispatches on DID byte in {0x00,0x01,0x04,0x05,0x0C,0x0D,
0x20,0x21,0x30,0x31,0x40,0x42,0x49}, returns a 1/2/4-byte payload and its length from the DTC
snapshot struct (+0x11,+0x16,+0x1b,+0x2a, table getter results). Case 0x42('B') counts records
with flag 0x200 set from the 0x38-byte-stride record array via `PTR_PTR_000ae324` — a
"number of active DTCs" value. Called from 0x000A98BC. [I] (spec 15 diagnostic services)

#### 0x000AE2AA `diag_did_read_dtc_record` — dispatch
Companion DID-read handler dispatching on {0x00,0x02,0x04,0x05,0x0C,0x0D,0x20,0x40,0x42,0x49};
reads bytes/words at offsets +4..+0x14 of a record obtained via helpers `PTR_FUN_000ae580/584`
and `PTR_DAT_000ae588`. Case 0x02 fetches a u16 through two indirect calls (min/limit pair).
Called from 0x000A99C0. [I]

#### 0x000AE472 `diag_did_read_ids_block` — dispatch
DID-read handler with three families: case 0x04 returns 17 bytes (0x11) — 1 + 16 copied from
`PTR_DAT_000ae58c` (VIN-like fixed ID); case 0x06 returns 5 bytes derived from an indirect
lookup returning a u32; case 0x0A returns 21 bytes (0x15) copied from `*(PTR_DAT_000ae59c)+0x4C`
(calibration/software-number-like block). Case 0x00 returns constant 0x14,0x40,0,0. Called from
0x000A9D66/0x000A9DE6/0x000A9B38. [I] (spec 15)

#### 0x000AE564 `dtc_snap_init_defaults` — logic
Writes 0xFF/0xFFFF "invalid" defaults to snapshot fields +0xe, +0x16, +0x11, +0x1b, +0x2a.
Called from 0x000A1170 (module init). [O]

#### 0x000AE5A0 `dtc_snap_collect_all` — logic
236-instr snapshot builder: fills a ~0x46-byte record from ~25 indirect getters (PTR_FUN/PTR_PTR
at 0x000ae7b4..0x000ae82c), each value paired with an invalid flag that forces 0xFF/0xFFFF/0.
Includes scaling steps (`<<2` clamped to 0xFFFF, `>>4` clamped to 0xFF) and two <2-validity
checks written to +0xa/+0x6. Called from 0x000A123C (periodic task). [I]

#### 0x000AE830 `tbl_s32_2d_lookup` — library
Two-dimensional word lookup: `tbl[(row * tbl[0]) + col + 2]` where the first word of the blob
is the row width. No bounds checks. Called from 0x000ACC12. [O]

#### 0x000AE844 `tbl_s16_get` — library
Returns s16 at `ptr + 4 + idx*2` — a 4-byte-header 16-bit table accessor. 4-member cluster:
0x000AE844, 0x000B6C94, 0x000B6C9E (r08_app copies) and 0x00151DB6 (r16 copy); identical
pattern, duplicated helper copies. [O]

#### 0x000AE84E `tbl_s32_get` — library
Returns u32 at `ptr + 4 + idx*4`. 3-member cluster: 0x000AE84E, 0x00151DC0, 0x001552DA. [O]

#### 0x000AE858 `tbl_s32_get_rev` — library
Returns u32 at `ptr + 4 + (~idx & 0xFFFF)*4` — same shape as 0x000AE84E but indexes the table
backwards (complemented index). 2-member cluster with 0x00151DCA; callers 0x000AD430,
0x000AD072. [O]

#### 0x000AE868 `dtc_module_init` — init
Zeros a state byte, calls 0x000AE906 (mirror/alive counter init), then registers the module
through five indirect calls (`PTR_FUN_000ae970..0x000ae984` — likely service-table/method
registration), setting state byte to 1 at the end. Called from 0x000A1170. [I]

#### 0x000AE890 `dtc_session_state_machine` — logic
Three-state machine (states 1/2/3) gated by mirror RAM state `*PTR_DAT_000ae988`: state 3 runs
a shutdown sequence (0x000AE906 + four indirect un registrations) and returns a status from
`PTR_FUN_000ae9a0`; state 2 calls 0x000AE8EE and advances to 3. Returns early with the mirror
state whenever it disagrees. Called from 0x000A123C. [I]

#### 0x000AE8EE `dtc_session_arm_restart` — logic
Sets state field +1 = 1 and clears the mirrored globals DAT_fff830ac/DAT_fff830b0 (arms a fresh
mirror pair). Called only from 0x000AE890 state 2. [O]

#### 0x000AE8FE `dtc_session_is_active` — data-accessor
Returns `*(param+1) == 1`. Called from 0x000A0DA6 whose caller chain reaches the UDS handler
0x0009D158 — plausibly "is a DTC/diag session in progress". [I]

#### 0x000AE906 `dtc_mirror_counter_tick` — logic
Verifies mirrored RAM pair DAT_fff830ac == ~DAT_fff830b0; on mismatch restores from constants
DAT_000ae9a4/9a8, resets the complementary alive counter DAT_fff830f0/fff830f1 (val/~val) and
clears +2; otherwise increments the counter and publishes it to +2. Classic mirrored-memory
alive-counter safeguard. Called from 0x000AE868 and 0x000AE890. [O]

#### 0x000AE9AC `vcall_obj_read_flag_d4` — data-accessor
Fetches an object via `PTR_FUN_000aebf0(PTR_DAT_000aebec)` and invokes virtual method at vtable
+0xd4 on it, returning a byte. Called from r0c_app 0x000E4548 (warning/text module). Purpose of
the flag not observable here. [I]

#### 0x000AE9C8 `get_global_flag_aebf4` — data-accessor
Three-instr getter returning `*_LAB_000aebf4` (a global byte). Called from r0c_app 0x000E4548. [O]

---
#### 0x000AE9D0 `fwd_iface_get_s16` — stub
Forwarding stub: calls the method registered at `PTR_FUN_000aebfc(PTR_DAT_000aebf8)` and returns
the s16 result (implementation 0x000CD4FC reads vtable+0x38 of the RAM object at DAT_fff8add8).
Called from r0b/r0c 0x000DC4C4. 2-member cluster (0x000AE9D0, 0x000AEA08). [O]

#### 0x000AE9DE `fwd_iface_get_u8` — stub
Same forwarding pattern returning u8 (via 0x000CD516). 7 identical copies in this cluster
(0x000AE9DE, 0x000AE9EC, 0x000AEA16, 0x000AEA24, 0x000AEA5C, 0x000AEAC0, 0x000AEAD4) — a bank
of per-use duplicate stubs. [O]

#### 0x000AE9FA `fwd_iface_get_u16` — stub
Forwarding stub returning u16 via 0x000CD54A (vtable+0x5c of the DAT_fff8add8 object). Called
from r0c warning/text module 0x000E4548 and 0x000E4214. [O]

#### 0x000AEA32 `get_glob_u8_aec18` — data-accessor
Three-instr getter for `*PTR_DAT_000aec18`. Called from r0c 0x000DFD2C. [O]

#### 0x000AEA3A `get_glob_eq2_aec1c` — data-accessor
Returns `*PTR_DAT_000aec1c == 2` (state == 2 check). Called from 0x000D1990. [O]

#### 0x000AEA44 `set_glob_1_aec20` — data-accessor
Writes 1 to `*PTR_DAT_000aec20`. Called from 0x000D1990. [O]

#### 0x000AEA4C `fwd_iface_call_aec28` — stub
Void forwarding call to `PTR_FUN_000aec28(PTR_DAT_000aec24)` (implementation 0x000B48F8 in this
region). 2-member cluster (0x000AEA4C, 0x000AEA54); callers 0x000D140C, 0x000CFD1A. [O]

#### 0x000AEA6A `get_glob_u8_aec34` — data-accessor
Getter for `*DAT_000aec34`; caller 0x000A3B94. [O]

#### 0x000AEA72 `get_glob_u8_aec38` — data-accessor
Getter for `*DAT_000aec38`; caller 0x000A3BA2. [O]

#### 0x000AEA7A `fwd_iface_get_u16_aec40` — stub
Forwarding stub returning u16 via `(*DAT_000aec40)(PTR_DAT_000aec3c)` (impl 0x000CB348).
Callers 0x000A3BBE and 0x000BA050. [O]

#### 0x000AEA88 `get_glob_u8_aec44` — data-accessor
Getter for `*DAT_000aec44`; caller 0x000A3BB0. [O]

#### 0x000AEA90 `get_glob_s8_aec48` — data-accessor
Returns sign-extended `*PTR_DAT_000aec48`; callers 0x000D0DFA, 0x000D1214. [O]

#### 0x000AEA96 `glob_range_1_8_invalid` — logic
Returns 1 when `*DAT_000aec4c` is 0 or > 8 — a 1..8 range validity check on a mode/position
global. Caller 0x000D0DFA. [I]

#### 0x000AEAAC `get_glob_u8_aec50` — data-accessor
Getter promoting `*DAT_000aec50` to int; caller 0x000A32E4. [O]

#### 0x000AEAB2 `get_glob_s16_aec54` — data-accessor
s16 getter for `*PTR_DAT_000aec54`; caller 0x000D37FE. [O]

#### 0x000AEAB8 `get_glob_u8_aec58` — data-accessor
u8 getter for `*PTR_DAT_000aec58`; caller 0x000D3830. [O]

#### 0x000AEACE `get_glob_s16_aec60` — data-accessor
s16 getter for `*PTR_DAT_000aec60`; caller r0c 0x000EA4C8. [O]

#### 0x000AEAE2 `fwd_obj_idx_s16_aec6c` — stub
Forwards `(PTR_DAT_000aec68, idx)` to `PTR_FUN_000aec6c`, returns s16 (impl 0x000A1FA4: reads
two config/list entries for idx-1, validates idx 1..5 and emits event 0x248 on violation).
2-member cluster (0x000AEAE2, 0x000AEB0A); callers 0x000E5236, 0x000E5706. [O]

#### 0x000AEAF2 `fwd_obj_idx_call_aec70` — stub
Void forwarding of `(PTR_DAT_000aec68, idx)` to `(*_LAB_000aec70)` (impl 0x000A1454). Caller
0x000E39DC. [O]

#### 0x000AEAFA `fwd_obj_idx_s16_aec74` — stub
Same forwarding family returning s16 via `(*DAT_000aec74)` (impl 0x000A1664). Callers
0x000DD61A, 0x000E5706, 0x000E3F84, 0x0010CC78. [O]

#### 0x000AEB1A `fwd_obj_idx_u8_aec7c` — stub
Forwarding stub returning u8 via `PTR_FUN_000aec7c` (impl 0x000A1B7A). 2-member cluster
(0x000AEB1A, 0x000AEB3A); caller 0x000DD61A. [O]

#### 0x000AEB2A `fwd_obj_idx_s8_aec80` — stub
Forwarding stub returning s8 via `PTR_FUN_000aec80` (impl 0x000A13CE). 2-member cluster
(0x000AEB2A, 0x000AEB4A); caller 0x000A3B22. [O]

#### 0x000AEB5A `fwd_obj_idx_u8_aec8c` — stub
Forwarding stub returning u8 via `(*DAT_000aec8c)` (impl 0x000A1362); caller 0x000A3B52. [O]

#### 0x000AEB6A `fwd_obj_idx_set4_aec90` — stub
Forwards `(PTR_DAT_000aec68, idx, a, b)` — a 4-argument setter — to `PTR_FUN_000aec90`
(impl 0x000A28A8, 141 instr, compares byte at +4 and s16 at +2 of the target record).
Caller 0x000DBF12. [O]

#### 0x000AEB76 `get_glob_s16_aec94` — data-accessor
s16 getter for `*PTR_DAT_000aec94`; caller 0x000DD65C. [O]

---
#### 0x000AEB7C `get_glob_u8_aec98` — data-accessor
Getter for `*PTR_DAT_000aec98`; callers 0x000D9448, 0x000D9582, 0x000D6E48. [O]

#### 0x000AEB84 `fwd_obj_aec94_call` — stub
Void forwarding call `PTR_LAB_000aec9c(PTR_DAT_000aec94)` — object method invoke. Caller
0x000DD65C. [O]

#### 0x000AEB8C `fwd_obj_aec94_get_u8` — stub
Forwarding stub: `PTR_FUN_000aeca0(PTR_DAT_000aec94)` returning u8 (impl 0x000B5B0C in this
region). 5-member cluster (0x000AEB8C, 0x000AEBAC, 0x000AEBBA, 0x000AEBC8, 0x000AEBD6);
callers 0x000EA4C8, 0x000D64D2, 0x000DD59E. [O]

#### 0x000AEB9A `fwd_obj_aec94_set2` — stub
Forwards `(PTR_DAT_000aec94, a, b)` to `PTR_FUN_000aeca4` (impl 0x000B59F8: case-gated writes
at +2/+5/+6/+0xC of the record). Caller 0x000DD400. [O]

#### 0x000AEBA4 `fwd_obj_aec94_call2` — stub
Void forwarding call `(*DAT_000aeca8)(PTR_DAT_000aec94)` (impl 0x000B5A38). Caller 0x000DD280. [O]

#### 0x000AEBE4 `get_glob_u8_aecbc` — data-accessor
Getter for `*DAT_000aecbc`; callers 0x000A41A0, 0x000A3F80. [O]

#### 0x000AECC0 `fwd_obj_aeeb4_get_u16_idx` — stub
Forwards `(PTR_DAT_000aeeb4, p2, u16 p3)` to `*DAT_000aeeb8` returning u8 (impl 0x000CA162,
r0c_app). Caller 0x000A3D1C. [O]

#### 0x000AECD8 `fwd_obj_aeeb4_set2_s16` — stub
Forwards `(PTR_DAT_000aeeb4, s16, s16, u8, u8)` to `PTR_FUN_000aeec0` (impl 0x000CA56A).
Callers 0x000EDC5A, 0x000EBDBC. [O]

#### 0x000AECF4 `fwd_obj_aeeb4_get_u8` — stub
Forwarding getter `PTR_FUN_000aeec4(PTR_DAT_000aeeb4)` (impl 0x000CA148). Callers 0x000E0C6C,
0x000E0A7A. [O]

#### 0x000AED02 `fwd_obj_aeeb4_set2c` — stub
Forwards `(PTR_DAT_000aeeb4, c, c, u8)` to `PTR_FUN_000aeec8` (impl 0x000C9FD8). Caller
0x000EDC5A. [O]

#### 0x000AED0E `fwd_obj_aeecc_get_u8_3a` — stub
Different object (`PTR_DAT_000aeecc`): forwards `(obj, c, c, u8)` to `PTR_FUN_000aeed0`
returning u8 (impl 0x000B1C3C). Caller 0x001180BE (r10_app). [O]

#### 0x000AED22 `fwd_obj_aeeb4_set3` — stub
Forwards `(PTR_DAT_000aeeb4, u8, u8, u8)` to `PTR_FUN_000aeed4` (impl 0x000CA5A2). Caller
0x000EBB6C. [O]

#### 0x000AED2E `fwd_obj_aeeb4_set_c_u8u8` — stub
Forwards `(PTR_DAT_000aeeb4, c, u8, u8)` to `PTR_FUN_000aeed8` (impl 0x000CA59A). Caller
0x000EDC5A. [O]

#### 0x000AED3A `fwd_obj_aeeb4_set5` — stub
Forwards six args `(PTR_DAT_000aeeb4, c, c, u8, u8, u8)` to `PTR_FUN_000aeedc` (impl
0x000CA07A). Caller 0x000EBC60. [O]

#### 0x000AED5C `fwd_obj_aeee0_get_s16_11` — stub
Forwards `(PTR_DAT_000aeee0, c, c, 1, u8, 1)` to `PTR_FUN_000aeee4` returning s16 (impl
0x000B108C, 184-instr conversion routine with two positive indices). Caller 0x001132F4. [O]

#### 0x000AED7A `fwd_obj_aeee0_get_s16_00` — stub
Same forwarding family with flag args (0, u8, 0). Caller 0x0010FFBE (r10_app). [O]

#### 0x000AED98 `fwd_obj_aeee0_get_s16_01` — stub
Same forwarding family with flag args (0, u8, 1). Caller 0x0010FFBE. [O]

#### 0x000AEDB6 `fwd_obj_aeee0_get_u8_c` — stub
Forwards `(PTR_DAT_000aeee0, c, u8)` to `PTR_FUN_000aeee8` (impl 0x000B00A6). Caller
0x000EC956. [O]

#### 0x000AEDC8 `fwd_obj_aeee0_get_u8_2c` — stub
Forwards `(PTR_DAT_000aeee0, c, c, u8)` to `PTR_FUN_000aeeec` (impl 0x000B019C). Caller
0x001180BE. [O]

#### 0x000AEDDC `fwd_obj_aeee0_get_s8_2c` — stub
Forwards `(PTR_DAT_000aeee0, c, c, u8)` to `(*DAT_000aeef0)` returning s8 (impl 0x000B0294).
Caller 0x000D1990. [O]

#### 0x000AEDF0 `fwd_obj_aeef4_get_s16` — stub
Forwarding getter returning s16 via `PTR_FUN_000aeef8(PTR_DAT_000aeef4)` (impl 0x000CF0A4).
Caller 0x001064C6 (r10_app shift module area). [O]

#### 0x000AEDFE `fwd_obj_aeefc_set_u8` — stub
Forwards `(PTR_DAT_000aeefc, u8)` to `PTR_FUN_000aef00` (impl 0x000CFA3A). Caller 0x001132F4. [O]

#### 0x000AEE06 `fwd_obj_aeefc_call` — stub
Void forwarding call `(*DAT_000aef04)(PTR_DAT_000aeefc)` (impl 0x000CFA86). Callers 0x00113F16,
0x000DDE78. [O]

#### 0x000AEE0E `fwd_obj_aef08_get_s16` — stub
Forwarding getter returning s16 via `PTR_FUN_000aef0c(PTR_DAT_000aef08)` (impl 0x000CEDC6).
Callers 0x0010BC24, 0x0010BD7E. [O]

#### 0x000AEE1C `fwd_obj_aef08_set_u16u8` — stub
Forwards `(PTR_DAT_000aef08, u16, u8)` to `PTR_FUN_000aef10` (impl 0x000CE48C). Caller
0x0010BC24. [O]

---
#### 0x000AEE26 `fwd_obj_aef08_set_u8` — stub
Forwards `(PTR_DAT_000aef08, u8)` to `PTR_FUN_000aef14` (impl 0x000CEE94). Callers 0x000DEA5E,
0x0010B490. [O]

#### 0x000AEE2E `fwd_obj_aef18_call` — stub
Void forwarding call `PTR_FUN_000aef1c(PTR_DAT_000aef18)` (impl 0x000CE00C). Caller 0x0010BC24. [O]

#### 0x000AEE36 `get_glob_u8_aef20` — data-accessor
Getter for `*DAT_000aef20`; caller 0x000A32E4. [O]

#### 0x000AEE3C `get_glob_u8_aef24` — data-accessor
Getter for `*DAT_000aef24`; caller 0x000A3BCC. [O]

#### 0x000AEE44 `get_glob_u8_aef28` — data-accessor
Getter for `*PTR_DAT_000aef28`; caller 0x000A3BDA. [O]

#### 0x000AEE4C `get_glob_u8_aef2c` — data-accessor
Getter for `*DAT_000aef2c`; caller 0x000A3BE8. [O]

#### 0x000AEE54 `fwd_obj_aef28_get_u16` — stub
Forwarding getter returning u16 via `(*DAT_000aef30)(PTR_DAT_000aef28)` (impl 0x000AF370).
Caller 0x000A3BF6. [O]

#### 0x000AEE62 `fwd_obj_aef34_call` — stub
Void forwarding call `PTR_FUN_000aef38(PTR_DAT_000aef34)` (impl 0x000C92A6). 2-member cluster
(0x000AEE62, 0x000AEEAA); callers 0x00103B36, 0x00103DE2 (r10_app). [O]

#### 0x000AEE6A `fwd_obj_aef34_call2` — stub
Void forwarding call `(*DAT_000aef3c)(PTR_DAT_000aef34)` (impl 0x000C92B6). Callers 0x000D140C,
0x000CFD1A. [O]

#### 0x000AEE72 `fwd_obj_aeeb4_call` — stub
Void forwarding call `PTR_FUN_000aef40(PTR_DAT_000aeeb4)` (impl 0x000CA176); no direct callers
— reached via registration/pointer table. 3-member cluster (0x000AEE72, 0x000AEE7A, 0x000AEE82). [O]

#### 0x000AEE8A `fwd_obj_aef4c_call` — stub
Void forwarding call `PTR_FUN_000aef50(PTR_DAT_000aef4c)` (impl 0x000A2A60, this region).
Caller 0x000BAEB4. [O]

#### 0x000AEE92 `fwd_obj_aeefc_call2` — stub
Void forwarding call `PTR_FUN_000aef54(PTR_DAT_000aeefc)` (impl 0x000CFA94); no direct callers.
2-member cluster (0x000AEE92, 0x000AEE9A). [O]

#### 0x000AEEA2 `fwd_obj_aef5c_call` — stub
Void forwarding call `PTR_FUN_000aef60(PTR_DAT_000aef5c)` (impl 0x000CF6D8). Caller 0x000BAEB4. [O]

#### 0x000AEF68 `get_glob_u8_af154` — data-accessor
Getter for `*PTR_DAT_000af154`; callers 0x000AF69E, 0x000AF43E. [O]

#### 0x000AEF70 `get_glob_u8_af158` — data-accessor
Getter for `*PTR_DAT_000af158`; callers 0x000AF69E, 0x000AF43E. [O]

#### 0x000AEF78 `request_state_machine` — logic
271-instr state machine over a status struct (state at [0], last result at [1], flags at
[2..0xE], u16 counters at +0x10/+0x1a). State 0 verifies two condition flags then starts a
operation through `PTR_FUN_000af17c(obj,3)` and polls result codes via PTR_FUN calls; state 1/2
complete or abort (codes 0xFD/0xFF/0xFB stored at [1]); states 3/4 tear down via 0x000AF7B6.
Calls helpers 0x000AF624/0x000AF43E/0x000AF57A/0x000AF69E/0x000AF75A/0x000AF78E. Driven from
r0c_app 0x000CD720 (callers=- there → task/registered). Purpose (what request) not identifiable
from this code alone. [I]

#### 0x000AF1F4 `request_timers_tick` — logic
Decrements the three nonzero u16 timers at +0x1a/+0x16/+0x18 of the request struct, then
forwards a call `(*DAT_000af3f4)(PTR_DAT_000af3f0)` (impl 0x000C9DC6). Called from 0x000A2A60. [O]

#### 0x000AF22C `request_result_u8_f74` — logic
If the request struct state == 2 and the operation object (via `PTR_FUN_000af3f8/400`) exists
and is ready, returns the u8 from operation vtable +0x74; else 0. 3-member cluster differing
only in vtable offset: 0x000AF22C (+0x74), 0x000AF2F8 (+0x8c), 0x000AF3A8 (+0xb0) — three
different result fields of the same operation. Callers 0x000CD798/0x000CD7D0/0x000CD8A0-ish. [I]

#### 0x000AF266 `request_get_status_flag` — logic
Returns the operation status char (via `PTR_FUN_000af404(PTR_DAT_000af3f0)`) when the request
state is 1 or 2, else 0. Caller 0x000CC5C6. [O]

#### 0x000AF282 `request_result_s16_f80` — logic
Same state==2 gating as 0x000AF22C but returns the s16 from operation vtable +0x80. 2-member
cluster: 0x000AF282 (+0x80), 0x000AF2BC (+0xa4). Callers 0x000CD7B4-family. [I]

#### 0x000AF334 `request_result_u16_f98` — logic
Same pattern returning u16 from operation vtable +0x98. Caller 0x000CD7DE. [I]

#### 0x000AF370 `cond_get_u16_state1` — logic
Calls `PTR_FUN_000af40c(PTR_DAT_000af408)`; only when that returns 1 returns the u16 from
`PTR_FUN_000af414(PTR_DAT_000af410)` (impls 0x000C0B06/0x000C0B7E), else 0. A state-gated
paired get/set of a u16 value. [O]

#### 0x000AF38C `cond_set_u16_state1` — logic
Setter counterpart of 0x000AF370 using `PTR_FUN_000af418` (impl 0x000C0BA0), gated by the same
state-1 check. [O]

#### 0x000AF41C `request_state_init` — logic
Returns param_1 (or allocates a 0x1C-byte block via `PTR_FUN_000af608(0x1c)` when NULL), zeroes
state [0] and sets [1] = 0xFF ("no result"). Callee 0x0009AA34 is the allocation path. No
direct callers (registered entry). [O]

#### 0x000AF43E `request_eval_reason_codes` — logic
150-instr precondition evaluator: when the operation object state is 1 and both gate globals
(0x000AEF68 == 1, 0x000AEF70 == -2) agree, walks a decision tree over the status object at
`PTR_DAT_000af614` (fields [7], [0x96], [0x9c], [0xa0]==5, [0x141]) and the operand object,
writing an enumerated reason code 0x00..0x12 into *param_2 (e.g. 0x03/0x04 from byte
difference/absolute comparisons at +8/+0xC/+0x10; 0x12 when a returned flag has bit 0). Called
from 0x000AEF78. Domain of the conditions (speeds/temperatures?) not observable here. [I]

---
#### 0x000AF57A `req_cond_tree_a_reasons` — logic
Precondition evaluator used by 0x000AEF78: compares calibration byte `PTR_DAT_000af614[0x96]`
against operand-object bytes at +8/+0xC/+0x14 and flag pair at `PTR_DAT_000af620[0x2B/0x2C/0x21/0x27]`,
writing reason codes 3/4/0xB/0xE; returns 1 only when `af620[0x21] == af620[0x27]`. Same family
as 0x000AF43E. [I]

#### 0x000AF624 `req_cond_tree_b_gear_split` — logic
Second precondition tree for 0x000AEF78: requires `*(short**)obj+0x1C < af7dc+0xC4`, flag
`af7dc[0xC2]==1`, then compares u16 `af7dc+0x7C` against *param_1-dependent thresholds —
different s16 targets at obj+0x20 for `*param_1 < 2` vs obj+0x24 for `> 1` (a gear-1-vs-rest
split); writes reason 5 or 8. [I]

#### 0x000AF69E `req_update_gather_fields` — logic
86-instr gather step for 0x000AEF78: copies the u16 at operand+0x2C into struct +0xC when
`af7dc+0x94 <= *(short**)obj+0x28`; clears +0xC/+0xD for end states 0/3/4; reloads timer +0x16
from operand+0x38 when +0x94 falls outside obj+0x30/+0x34; latches +0x12/+0xB ("timer active")
and raises +0x18/+0x19 = -0xC on expiry. [I]

#### 0x000AF75A `req_abort_state4_count` — logic
Abort path of 0x000AEF78: sets state(+0)=4, phase(+6)=2, result(+1)=0xFB, reason(+5)=0xC,
increments the u16 attempt counter via the state-gated pair 0x000AF370/0x000AF38C (+1, stored
at +8), then issues three unregister-style calls (PTR_FUN_000cc580/584/588). [O]

#### 0x000AF78E `req_exit_state3_count100` — logic
Success/exit path of 0x000AEF78: state(+0)=3, phase(+6)=1, result(+1)=0xFC, attempt counter
+= 100 via 0x000AF370/0x000AF38C, then the same three unregister calls. [O]

#### 0x000AF7B6 `req_struct_clear_all` — logic
Clears the whole request status struct (state=0, result=0xFF, flags and u16 fields at +0x2..+0x14
to 0). Called from 0x000AEF78 teardown. [O]

#### 0x000AF7F4 `tblset_s16_get_f48` — library
Reads s16 table entry: `PTR_FUN_000afad4(table_ptr, idx)` where table_ptr = `*(PTR_DAT_000afad0
+ 0x48)` (impl 0x000B6C94, the duplicated s16 accessor). 4-member cluster differing only in the
object field: 0x000AF7F4 (+0x48), 0x000AF80A (+0x50), 0x000AF820 (+0x58), 0x000AF836 (+0x60) —
four table slots in one table-set object; no direct callers (registered). [O]

#### 0x000AF84C `tblset_u16_get_f4c` — library
Same pattern returning u16 (impl 0x000B6C9E) from `PTR_DAT_000afad0 + 0x4C`. 3-member cluster:
0x000AF84C (+0x4C), 0x000AF862 (+0x54), 0x000AF878 (+0x5C). Interleaves with the s16 slots
above — eight consecutive table pointers at +0x48..+0x64 of one object. [O]

#### 0x000AF88E `tblset_u16_get_f64` — library
Same u16 table lookup from `PTR_DAT_000afad0 + 0x64` (eighth slot). Own cluster because of a
constant-only difference from 0x000AF84C's family. [O]

#### 0x000AF8A4 `tblset_apply_active_rows` — logic
Computes an index from the 9-wide nibble table `PTR_DAT_000afadc[byte(p+5) + byte(p1[1])*9]`
(low nibble −1), scales it by byte counts at obj+0x1C/+0x1D, then loops calling vtable+0x20
("set active row") on the four sub-objects at +0xC/+0x10/+0x14/+0x18 plus the two global table
objects `PTR_DAT_000afae4`/`DAT_000afae8` with index `byte(p+6)-1`. Selects active rows across
a whole set of curve/table objects. Called from 0x000B13B8 (gear-pair planner). [I]

#### 0x000AF98A `gearpair_tbl_read_f10_f18` — logic
Selects sub-object at +0x10 (selector 1) or +0x18 (selector 2) of the referenced object and
reads vtable+0x14 value at index `byte(p+7)-1`. Called from 0x000B12EC/0x000B1228. [I]

#### 0x000AFA4E `iter_init_pair_0xc` — logic
Allocates (or reuses) a 0xC-byte iterator: stores callback `PTR_LAB_000afaf8` at [1], `this =
obj+0x1C` at [0], and first element `obj+0xC` or `obj+0x14` by flag param_3. Allocator is
0x0009AA34 (via PTR_FUN_000afaf4). Called from 0x000B0860. [O]

#### 0x000AFA86 `obj18_vcall14_char` — data-accessor
Calls vtable+0x14 on the object at `*(param+0x18)` with param_2, returns char. No direct
callers (registered). [O]

#### 0x000AFA9E `iter_init_pair_0x1c` — logic
0x1C-byte variant: callback `PTR_LAB_000afafc` at +0x14, element pointer `obj+0xC`/`obj+0x14`
by flag at +0x18. Called from 0x000B0E44. [O]

#### 0x000AFB00 `req30_init_defaults` — logic
Allocates 0x30 bytes (or reuses), sets +6 = 400, +0x1E = 0x7FFE, +0x27 = 1 (state 1 with a 400
default at the result-code slot). Called from 0x0015B0A4 (r18_tail region). [O]

#### 0x000AFB2C `gearpair_get_result_byte` — logic
Composes the gear-pair lookups: 0x000B03A4 (path code) then 0x000B0424 (element code) then
0x000B0530 (map object) and returns the byte at obj+6. Called from 0x000A28A8/0x000A280A. [I]

#### 0x000AFB66 `gearpair_eval_advance_state2` — logic
99-intr gate for the planner: requires active object (**obj+4 != 0), computes 0x000B03A4 → +0xF,
checks two flag getters == 0 and status flags afd68[0x2B/0x2C]==1, computes 0x000B0424 → +0x13,
extra getter → +0x14, 0x000B04B8 → +0x11, resolves the pair object 0x000B0530 → +0x2C, requires
its vtable+0x44 status == 1 and two bracket reads via `(*DAT_000afd74)(obj+0x38/+0x3C, code,
code)` around `*PTR_DAT_000afd70`; then advances +0x27 = 2. Called from r0c 0x000C9FD8. [I]

#### 0x000AFC54 `shiftseq_state_machine` — logic
326-instr multi-step shift-sequence executor over state byte +0x27 (1..7): state 1 one-shot
flag +0x16 → result 400; state 2 selects +0x17 from operand+0x68/+0x6C, evaluates two checks
(PTR_FUN_000afd90 with 1/2) storing pass flags at +0x25/+0x26; states 3/4/5 run 0x000B0708,
accumulate timer +0x18 from tables, increment counter +0x1C, and complete with u16 result
codes 0x19A/0x19F/0x1A4/0x1A9/0x1C2/0x1D6 stored at +6; invalid state logs event 0x294 (via
PTR_FUN_000affa0, args 0xF,0,3,6,0x90) and resets to state 1. Called from r0c 0x000CA3A6. [I]
(spec 11 shift-execution: gear-pair/shift-type selection)

#### 0x000AFFA4 `shiftseq_finish_checks` — logic
Completion gate: requires state == 7, +0x1E == +4 with +4 != 0, two flag getters (PTR_FUN_000b017c/184)
== 0, +2 != 1, param_2 == 0 and param_3 != 0; assigns result codes 0x1D6/0x1E0/0x1EA per failed
check; on success runs 0x000B0860, then sets +0x16 = 1 and calls 0x000B05D4. Called from r0c
0x000CA07A. [I]

#### 0x000B0030 `gearpair_map_read_value` — logic
Remaps both gear bytes via `PTR_FUN_000b0188`, resolves path (0x000B03A4) and element
(0x000B0424) codes, gets the pair object (0x000B0530), selects sub-object +0x10 (flag) or
+0x18, and reads vtable+0x14 at index path-1. Called from 0x000A28A8. [I]

#### 0x000B00A6 `gearpair_check_flag_2to8` — logic
For gear param_2 in 2..8 with flags b018c[0x2B/0x2C]==1 and calibration window checks
(b0194[0x96] within obj+0x2C/+0x28, b0194[0x9C] <= obj+0x20): resolves pair object via
0x000B03A4(p, p-1)/0x000B0530 and dispatches on operand mode (*(char**)obj+8) 1 → 0x000B04B8
or 3 → 0x000B043C, then `PTR_FUN_000b0198`; sets +0x22 = 1 on success. Called from 0x000AEDB6. [I]

#### 0x000B019C `gearpair_find_attempt_match` — logic
115-instr: validates flags/gear window like 0x000B00A6 (mode must be 2 or 3), requires
element code 0x000B0424 ∈ {0,1}, resolves pair object, gets code via 0x000B043C and validates
with 0x000B1228; on miss scans param_4 (<5) candidates via 0x000B04B8 for a matching code.
Writes match flag to +0x23 and returns the (possibly updated) index. Called from 0x000AEDC8. [I]

#### 0x000B0294 `gearpair_walk_down_find_next` — logic
Walks gear param_2 down to param_3+1 (`uVar8-1` each step): resolves pair codes (0x000B0424,
0x000B03A4), logs event 0x294 (args 0xF,0,8,0xC,0x5B) on a bad pair, gets pair object and code
(0x000B043C), validates via 0x000B12EC; on success stores the found gear at +0x21 (mirroring
the lower gear via PTR_FUN_000b03e0) and stops. Called from 0x000AEDDC. This is the
"next gear in a multi-step sequence" search. [I]

#### 0x000B03A4 `shift_type_code_lookup` — library
Returns low nibble of `PTR_DAT_000b03ec[to + from*9]` for from/to < 9 (9×9 from-to gear table
— the shift-type/path code; compare spec 11: four 9×9 (from,to) tables keyed kind 0-3); on
out-of-range logs event 0x294 (args 0xF,0,6,7,0x16). Called by the whole gear-pair family. [O]
(spec 11 shift-execution §shift type)

#### 0x000B0424 `shift_elem_code_lookup` — library
Returns the high nibble (`&0xF0 >> 4`) of the same 9×9 table entry — the second code (shift
element combination) of the from/to pair. [O] (spec 11)

---

#### 0x000B043C `gearpair_code_if_12` — library
Pair-code lookup for the shift family: bounds-checks gears (<5), calls vtable+0x80 check on
table object at *PTR_DAT_000b069c+0x80 and returns the +0x7c result only when the check is 1
or 2; else logs event 0x294 (args 0xF,0,7,0x4D) and returns 0. Callers: 0x000B108C,
0x000B00A6, 0x000B0294, 0x000B019C. [O] (spec 11 shift-execution)

#### 0x000B04B8 `gearpair_code_if_2` — library
Same lookup as 0x000B043C but accepts the +0x7c pair code only when the +0x80 check returns
exactly 2 (subset used for candidate scanning); error event 0x294 code 0x71. Callers:
0x000B00A6, 0x000AFB66, 0x000B019C. [O]

#### 0x000B0530 `gearpair_obj_by_gear_sel` — data-accessor
Resolves the gear-pair object pointer: gear 0 or unknown selector logs event 0x294 (0x96/0xB5)
and returns default PTR_PTR_000b06a4; otherwise indexes one of four pointer tables
(PTR_LAB_000b06a8/ac/b0/b4) by selector 0/1/3/4 and gear. Core resolver for the whole
gear-pair family (8 callers incl. 0x000B108C, 0x000B0030). [O]

#### 0x000B05D4 `shiftseq_reset_defaults` — logic
Re-initializes the shift-sequence struct: clears state/result/counters, sets limit +0x1E=0x7FFE
and flag +0x27=1 (compare 0x000AFB00/0x000AF7B6 defaults). Called from 0x000AFFA4 and
0x000AFB66 on completion/restart. [O]

#### 0x000B0612 `shift_ramp_cell_index` — logic
Two row-scans over the pair object's ramp tables (+8 rows bounded by +0x1d, +4 rows by +0x1c)
comparing rpm params against vtable+0x14 row values; if both advanced, builds a linear cell
index (nLow+1)*(shift_type-1)*(nHigh+1) + ... via 0x000B03A4 and returns the u16 from
PTR_FUN_000b08d8(index). Feeds the torque-phase profile cell. Called from 0x000B108C. [I]

#### 0x000B0708 `shiftseq_precheck_result_1d6` — logic
Shift-sequence precondition gate: event 0x294 (0x9b) if pair object (+0x2c) null; checks mode
1/2 torque/rpm conditions (PTR_FUN_000b08ec..091c, cal 0x9c window on +0x20) — on pass returns
0, else zeroes result +4 and returns 0x1D6. Called from 0x000AFC54 state machine. [I]

#### 0x000B0860 `shiftseq_apply_torque_phase` — logic
371-instr torque-phase executor, run from 0x000AFFA4 after the completion gates pass: computes
phase duration (+0x18/+0x1C scaled /10), torque-reduction base via 0x000B0C40 and 2-D profile
tables (PTR_LAB_000b0b78/94 indexed from/to gear x0x24, rows 6/0x11), clamps (+4000 via
PTR_FUN_000b0e00), reads 0x000B1012, issues vtable+0x44/+0x50 commands on the pair object,
then 0x000B0FAE advances the element table. [I] (spec 11 phase engine)

#### 0x000B0C40 `shift_torque_reduce_base` — library
Up/down torque-reduction base percent: from/to gears at param_2+4/+5 pick PTR_LAB_000b0e18
(up) or 0x000b0e34 (down) index; mode 1/2 selects struct +8/+0xC, then 2-D interpolation
PTR_FUN_000b0e40(tbl, param_3, param_4). Returns 100 (no reduction) for unknown modes or
same gear. Called only from 0x000B0860. [I]

#### 0x000B0E44 `shift_phase_time_calc` — logic
Builds two iterators (PTR_FUN_000b105c/60/64) over the pair object, reads boundary rpm rows
(vtable+0x14) for the high/low elements, then returns clamp( sStack_20 - sStack_1c ) between
two x10-scaled limits (PTR_FUN_000b107c/80) — a phase duration/timing value. Called from
0x000B108C. [I]

#### 0x000B0FAE `shift_elem_advance_row` — logic
Post-command step: shift-type via 0x000B03A4 (-1), picks element table +0x10 (mode 1) / +0x18
(mode 2), reads current row count (vtable+0x14), then calls vtable+0x20 with row+1 (advance
active row); increments global counter PTR_DAT_000b1084. Called from 0x000B0860. [O]

#### 0x000B1012 `shift_elem_row_s16` — data-accessor
Returns s16 from the element table (+0x10 mode 1 / +0x18 mode 2) at index shift_type-1 via
vtable+0x14; 0 when shift type is 0. Called from 0x000B0860. [O]

#### 0x000B108C `shift_torque_phase_evaluate` — logic
Main torque-phase evaluator behind the fwd stubs at 0x000AED5C/7A/98: normalizes gears
(PTR_FUN_000b12c4), shift type 0x000B03A4 → +8, selector check (0x000b12cc), element code
0x000B0424 → +0x15, pair code 0x000B043C → +9, object via 0x000B0530, gate vtable+0x44;
computes cell 0x000B0612 then timing 0x000B0E44 → writes s16 result to *param_1. Special
7-gear/PTR_DAT_000b12dc path can latch +0x14/+0x15/+2 or force rc +0x27=7. [I] (spec 11)

#### 0x000B1228 `gearpair_check_thresholds` — logic
Pair validation: u16 via PTR_FUN_000b12e8 vs mode-specific limits from global PTR_DAT_000b12c0
(+0xC/+0x10 up, +0x1C down); upshift case also runs 0x000B1352 combine. Returns ok-flag;
callers 0x000B00A6, 0x000B019C, 0x000AFC54. [I]

#### 0x000B12EC `gearpair_check_dn_thresholds` — logic
Downshift-only variant of 0x000B1228 using global PTR_DAT_000b1428 (+0x14/+0x18 limits,
PTR_FUN_000b1424 reader) then 0x000B1352; returns 1 by default. Called from 0x000B0294
sequence walk. [I]

#### 0x000B1352 `gearpair_mode_combine` — logic
Combines a table flag and the incoming ok-flag per mode byte at *PTR_DAT_000b1428+0x24:
0=AND, 1=pass-through, 2=table-only, 3=OR, other=1; table value from pair gears via
PTR_FUN_000b1430/DAT_000b1438. Shared tail of both threshold checks. [O]

#### 0x000B13B8 `shiftseq_on_gear_change` — logic
Gear-change hook from 0x000AFC54: latches current gear pair (PTR_DAT_000b143c vs obj
+0x74/+0x78); on change, if PTR_FUN_000b1440(param) ok, resolves pair object (0x000B0424 +
0x000B0530) and invokes PTR_FUN_000b1444 on it. [O]

#### 0x000B1448 `req3c_set_rc_10` — stub
Takes param (or gets object via PTR_FUN_000b14c8(0x3C)) and writes result code 10 to +0x2F
of the 0x30-byte request struct; callers='-' (pointer-table entry). Companion setter to
0x000B18E0 which writes rc 0xB. [I]

#### 0x000B1468 `req_state_valid_mode1` — logic
Returns 1 iff struct state byte is not 0 and not a "whitespace" code (0x09-0x0D/0x20 — i.e.
states 9-13/32) and operand-mode byte [8]==1. Used by 0x000B108C to gate evaluation on an
active mode-1 request. [O]

#### 0x000B148E `shph2_state_machine` — logic
450-instr phase-2 state machine (states 1..0xE in struct[0]) called from 0x000CA3A6:
prologue 0x000B1E7C/0x000B1E54/0x000B1FEE; state 1 arms (mode latch, [0x23]=1)→2; 2 waits
flag then →3/5; 3/4 (mode 1) and 5/0xE (mode 2) track rpm values +0xC/+0x1E/+0x1C with
-9999 (0xD8F1) invalid markers through gear-pair tables; 6 latches delta +0xE; 7 logs event
600 (0x77) and sets 0xD; abort via 0x000B2028 → 0xB. [I] (spec 11 phase engine)

#### 0x000B18E0 `shph2_start_request` — logic
Entry-condition evaluator behind 0x000C9FD8 (fwd 0x000AED02 family): shift-in-progress
global 0x1a14 ([0x1C]<6), 10x10 gear table cVar1, gate flags, then mode-1/2 paths set +0x30
(vtable), +0x31 (via 0x000B1E12) and check rpm/percent windows (cal +0x14/+0x10 vs 0x7E/0x96
limits); on pass latches [8]=mode, [9]=code, state=1, rc(+0x2F)=0xB — starts 0x000B148E. [I]

#### 0x000B1BB2 `shph2_stop_set_state` — logic
Stop/abort transition (from 0x000CA07A): mode 1 or flag==0 → reset via 0x000B2336 then state
0xF/0xA; prior state 6 → run 0x000B2378 and set 8/9; else state 0xC. Latches [0x23]→[1] and
clears [0x2E]. [O]

#### 0x000B1C3C `shph2_upshift_entry_check` — logic
Impl behind fwd 0x000AED0E: validates an upshift phase-2 request (param_2<param_3 gears,
shift code <6 from PTR_FUN_000b1ed8, 10x10 table code cVar13, flags +0x68/2B/2C), then
cal-table window checks (vs +0x96/+0x7E/+0x64 limits); on pass sets +0x2E=1 and returns
param_4, else 0. [I]

#### 0x000B1E06 `fwd_obj_plus_c_call` — stub
6-instr wrapper: indirect call PTR_FUN_000b1f24(obj PTR_DAT_000b1f20, param_1+0xC); no
callers resolved (pointer-table entry). [O]

#### 0x000B1E12 `shph2_validate_bool` — logic
Maps a byte to 0/1; any other value logs event 600 (args 0x14,3,0x5A) and returns 0. Used by
0x000B18E0/0x000B1C3C to validate vtable-derived flags. [O]

#### 0x000B1E54 `shph2_init_target_field4` — logic
Prologue step of 0x000B148E: reads signal via PTR_FUN_000b1f18, 2-D table lookup
PTR_FUN_000b1f34 at param+0x34 against cal ref +0x20, writes u16 target to +4. [I]

#### 0x000B1E7C `shph2_rc_accumulate_avg` — logic
Result-code dispatcher for the phase-2 machine: rc 10 keeps; 11 zeroes counters; 12 polls two
flags → 0xE; 13 accumulates +0x28 += global time and +0x2C++ then computes average +0x18 =
sum/count → rc 0xE/10; 14 keeps; else event 600 (0x61). [I]

#### 0x000B1FEE `shph2_calc_delta_field12` — logic
If global PTR_DAT_000b2180[8] (gear) nonzero: +0x12 = u16(PTR_DAT_000b2184 value minus table
lookup(PTR_FUN_000b2188 gear, cal 0x218c)) — rpm/speed delta used as phase-2 reference. [I]

#### 0x000B2028 `shph2_abort_check` — logic
216-instr abort predicate used by every working state of 0x000B148E: master flags
(PTR_FUN_000b219c/1a0/1ac, global [199]); mode-1 path checks gear-pair table window (+8/+0xC
vs +0x64), 0x000B2294 driver-type gate and +0x30 table vs gear window; mode-2 path checks
+0x58/+0x5C window on +0x94, +0x48/+0x4C tables indexed [9]-10, 0x000B2294, +0x9C/+0xC4
and flags [0xEB]/[0xC6]. Returns 1 = abort. [I]

#### 0x000B2294 `shph2_driver_type_valid` — logic
Validates the driver-type combination on global PTR_DAT_000b243c: type [0x20] 1-4 each
requires flags [0x1D]/[0x1F] and a shift-active check into PTR_DAT_000b242c; returns 1 when
the combination is permitted. Part of 0x000B2028. [I]

#### 0x000B2336 `shph2_reset_struct` — logic
Full reset of the phase-2 struct: states/counters 0, +0x24=0x7B (no-value), +0x1E/+0x1C =
0xD8F1 (-9999), rc(+0x2F)=10, [10]=1, +0x30/+0x31=0. Called from 0x000B18E0 and 0x000B1BB2. [O]

#### 0x000B2378 `shph2_compute_final_value` — logic
Finalization path (state-6 stop): validates [9], [0x24]==[2], gear 1..5 (event 600 0x92/0x99/
0x9F on failure); mode 1 interpolates tables with +0xE/+0x1A, mode 2 uses +0x10/+0x18 x10;
result clamped via PTR_FUN_000b2610/14/18/1c chain into +6; if rc +0x30 in 1..3 writes it out
via 0x2620/0x2624 and returns 1. [I]

#### 0x000B2400 `evt600_9f_stub` — stub
1-instr event-log stub: event 600 (args 0x11,4,0x9F), return 0; no resolved callers
(pointer-table entry, likely error branch target). [O]

#### 0x000B2402 `evt600_9f_stub__2` — stub
8-instr duplicate of the 0x000B2400 event 600 (0x11,4,0x9F) log stub. [O]

#### 0x000B2412 `evt600_log_11_4` — stub
Event-log wrapper taking extra params: logs event 600 with (0x11,4,param_3) and returns 0. [O]

#### 0x000B268E `geareng_pos_code_5` — logic
When actuator state byte +0x94==5, computes a code from counters (+0xa0%10 plus +0x8C*10,
-10 adjustment) via PTR_FUN_000b27c8; otherwise logs event 0x27B (0xD7) and returns 0.
Called from 0x000CD6C0. [I]

#### 0x000B26CA `evt27b_d7_stub` — stub
Logs event 0x27B (args 1,0xB,0xD7) and returns stale stack — error-path stub with no resolved
callers (pointer-table entry). [O]

#### 0x000B26FE `geareng_reset_output` — logic
Forces +0x94=8, ticks sub-object vtable+0x74, then sets output global PTR_DAT_000b27d4 to 0
(when +0xac in {0,1}) or 4 (+0xac:=0xE); chains PTR_FUN_000b27dc/27e0. Callers: 0x000CB080,
0x000CB188, 0x000B2E68, 0x000B3610. [I]

#### 0x000B2754 `geareng_state_machine` — dispatch
17-state (0x00-0x11 at +0xac) gear-engagement sequence, called from 0x000CD676: 0-3 arm
(magic 0xAA check at +0x13b, 0x000B301E, 0x000B312E clear, 0x000B3160/0x000B31D8 checks);
4 reads result object (0x000B2FD8, vtable +0x2C/+0x38/+0x14, latch +0xAB/+0x9E/+0x95);
5 gates via 0x000B33E8/0x000B33A6 → 6; 6 issues the engage command (0x000B2F52 selects
+0x2C/+0x38/+0x50 sub-object, vtable +0x2C with +0x9E, +0x8C) → 7; 7 waits +0x94 in {7,8}
then resets (vtable +0x80/+0xA4) → 8; 8/9/10 timed ramp: 5-entry table by +0x9E, u16 target
+0x8C, timeout +0x90 = byte*100, progress via 0x000B35B8/0x000B35EA, fail → 0xB; 0xB-0xC
confirm/teardown (0x000B26FE, restart at 9 or 4); 0xD confirm logs result 3/4; 0xE full reset
(0x000B3394); 0xF waits 0x000B33BE → 0x10 (0x000B33D6) → 0x11 idle, abort → 0. Every state
first runs abort checks 0x000B3332/0x000B31D8/0x000B33E8; tail always 0x000B37DC tick. [I]
(cross-region: impls at 0xCB0xx-0xCD6xx live in another unit)

#### 0x000B2C72 `geareng_frag_call80` — stub
10-instr register-fragment (unaff r0/r12/r13): vtable+0x80 call, store r12 to +0xa6, tick
0x000B37DC, return state(+0xac)!=1; likely a jump-thunk variant of the 0xE teardown path. [I]

#### 0x000B2E68 `geareng_default_case_evt` — logic
Default-case helper of 0x000B2754: logs event 0x27B (0xF), runs 0x000B26FE reset and the
0x000B37DC tick, returns state(+0xac)!=1. [O]

#### 0x000B2EA6 `geareng_cmd_state_machine` — logic
Split-body machine (18 instr at 0xB2EA6 flowing into code at 0xCAB9C+; called from
0x000CA3A6): sub-state at +0xF — 0 clears, 1 evaluates selector flags (+7/+0x1D/+0x1E → 0/1/2)
and gear gates (+0x95/+0x42/+0x34), 2 executes: 3-byte-per-gear table (PTR_LAB_000cabd4)
indexed by +0x96 (1-4) and direction, starts timer via 0x000CB188, then per mode 1/2 issues
two different indirect command chains (PTR_FUN_000cae4c..70 vs e74..8c, param/10 timeouts)
on gear 1-5 and vtable+0x14(...,2) → 3; failure logs event 0x25D with 13 snapshot args and
polls PTR_FUN_000caeb0. [I]

#### 0x000B2ECA `geareng_io_update` — logic
Refresh engagement I/O: PTR_FUN_000b2fc4 on value field +0x8C, sub-object (0x000B2EFC)
vtable+0x44, PTR_FUN_000b2fc8(param), PTR_FUN_000b2fcc on timeout +0x90. Called from
0x000A2A60. [I]

#### 0x000B2EFC `geareng_get_subobj_84` — data-accessor
Returns +0x84 sub-object pointer, validating it is +0x2C/+0x38/+0x50 of the module struct;
else logs event 0x27B (0xBC) and defaults to +0x2C. 9 callers across the module (incl.
0x000AE9AC r0c-warning read). [O]

#### 0x000B2F52 `geareng_set_subobj_84` — logic
Setter counterpart of 0x000B2EFC: stores one of +0x2C/+0x38/+0x50 into +0x84, event 0x27B
(0xD8) on any other pointer. [O]

#### 0x000B2FD8 `geareng_get_obj_88` — data-accessor
Returns +0x88 object when it equals global default PTR_DAT_000b321c, else event 0x27B (0xF4)
and the default; used by 0x000B37DC tick. [O]

#### 0x000B301E `geareng_set_obj_88` — logic
Stores +0x88 = PTR_DAT_000b321c (default) or the supplied pointer; non-default logs event
0x27B (0x11). No resolved callers (invoked from the 0xB2754 state 1 path). [O]

#### 0x000B304C `evt27b_log2` — stub
6-instr event-log stub: event 0x27B with two caller-supplied args. [O]

#### 0x000B3060 `geareng_get_ctr_bank` — data-accessor
Returns per-gear attempt counter for gear 1-5: bank 1 at +0xAE+g (mode 1) or bank 2 at
+0xB3+g (mode 2); 0 for out-of-range. Callers incl. 0x000B37DC, 0x000CB7EC, 0x000CBD0E. [O]

#### 0x000B309C `geareng_ctr_incr` — logic
Increments the bank-1 (+0xAE+g, saturates at 0xFF) or bank-2 (+0xB3+g, saturates at 0xFE)
per-gear counter for gear 1-5; no-op otherwise. [O]

#### 0x000B30E4 `geareng_ctr_clear_bank` — logic
Zeroes the five counters of bank 1 (+0xAF..) or bank 2 (+0xB4..) for gears 1-5. [O]

#### 0x000B312E `geareng_clear_check_bytes` — logic
Clears the three check-result bytes +0xC9/+0xCA/+0xCB. Cluster of 3 (dup helpers at
0x0014802A and 0x0014889E in later regions). [O]

#### 0x000B313E `geareng_check_code13` — logic
Indirect validation (_LAB_000b322c(obj, gear)); on failure stores reason code 0x13 in
*_LAB_000b3230. Used by the 0xB2754 ramp-progress path. [I]

#### 0x000B3160 `geareng_check_code_3_4` — logic
Engagement-progress check (armed +0xAD==1): compares gear +0x96 against object windows
(+0x10/+0x14/-+0x1C) producing reason 3/4, 0xD (flag mismatch) or 0xE; result latched in
+0xCB. Callee impl at 0x000CC52E (cross-region). [I]

#### 0x000B31D8 `geareng_check_main` — logic
Main status check (armed): 0x000B32DA speed-window, then bit test (_LAB_000b3444) → 0xB/0x1A,
hold-flag → 0xF, +0xA0==5 path compares +0x9C/+0x96 against +0x14 windows → 3/4/7, else 6;
reason 0x10 on the first indirect fail; result in +0xCA, reason via PTR_DAT_000b3454. [I]

#### 0x000B32DA `geareng_check_speed_win` — logic
Speed-window precondition (called by 0x000B31D8): compares object values (+0xC4, flag
[0xC2], window +0x7C) producing reason 5 or 8; result latched in +0xC9. [I]

#### 0x000B32EE `geareng_check_speed_win_frag` — logic
Register-argument fragment (in_r1/in_r7) implementing the same speed-window check as
0x000B32DA with reason 5/8 into +0xC9 — alternate entry kept by the linker. [I]

#### 0x000B331C `geareng_magic_ok` — logic
Returns armed(+0xAD==1) AND *PTR_DAT_000b345c == 0xAA (magic/pattern handshake byte). [O]

#### 0x000B3332 `geareng_abort_latch` — logic
Master abort check of the engagement machine: if 0x000B331C fails or already latched (+0xAE),
stores reason 0x12 and sets latch +0xAE=1; returns the latch. Called from nearly every state
of 0x000B2754. [O]

#### 0x000B335A `geareng_broadcast_armed` — logic
When armed (+0xAD==1), copies the armed value to the four output globals PTR_DAT_000cc550/
554/558/55c and returns 1; else returns +0xAD. Companion of the 0x000B3382/94/D6 fan-outs. [I]

#### 0x000B3370 `geareng_armed_call_3468` — stub
8-instr armed gate: if +0xAD==1 call PTR_FUN_000b3468(PTR_DAT_000b3460) (impl 0x000CC40C,
cross-region). [O]

#### 0x000B3382 `geareng_armed_call4` — stub
Armed gate firing four indirect calls on globals cc554/cc550/cc558/cc55c (PTR_FUN_000cc570-
57c) — part of the state-4 completion path of 0x000B2754. [I]

#### 0x000B3394 `geareng_armed_call3` — stub
Armed gate firing three indirect calls (PTR_FUN_000cc580/584/588) — teardown of the four
output globals, run at state 0xE. [I]

#### 0x000B33A6 `geareng_armed_poll_3474` — stub
Armed gate returning PTR_FUN_000b3474(PTR_DAT_000b3460) (impl 0x000CC45E); gate condition
inside state 5 of 0x000B2754. [O]

#### 0x000B33BE `geareng_armed_poll_3478` — logic
Armed gate returning PTR_FUN_000b3478(PTR_DAT_000b3460) (impl 0x000CC4A2), default 1 — the
state-0xF wait condition. Cluster of 2 with 0x000B33E8 (same pattern). [O]

#### 0x000B33D6 `geareng_armed_call4_off` — stub
Armed gate firing four indirect calls (PTR_FUN_000cc58c-598) on the output globals — state
0x10 release path. [I]

#### 0x000B3400 `geareng_start_ramp` — logic
State-8 entry of 0x000B2754: resets +0x96=1/+0x97/+0x98/+0xAA, selects sub-object by mode
+0x95 (1→+0x2C, 2→+0x38, else event 0x27B/0xD3), issues vtable+0x2C with gear +0x9E, reads
u16 bounds +0x9A/+0x9C and vtable+0x98; clears RAM DAT_fff89b81-8A and republishes the
7-channel snapshot via PTR_FUN_000b3d50-80 (gear-1 indexed); FUN_000b455a when +0x88>3
(cross-region). [I]

#### 0x000B3506 `geareng_progress_cond` — logic
Progress predicate: global gate clear, sub-object vtable+0xB0 and +0xBC both zero, then
compares an interpolated cal value (obj +0x30) against a scaled short (PTR_DAT_000b3664);
returns true when below. Used by 0x000B35EA (reason 0x11). [I]

#### 0x000B3574 `geareng_abort_code19` — logic
Abort predicate (armed +0xAD): when global gate clear and scaled difference
(PTR_DAT_000b3674 - +0xa6) exceeds the cal threshold (+0xC), sets reason 0x19 and returns 1. [I]

#### 0x000B35B8 `geareng_pos_window_code9` — logic
Position-window abort: armed and +0x94 outside the inclusive window formed by object values
at +0x20/+0x24 → reason 9, return 1. Used in the 0x000B2754 ramp states. [I]

#### 0x000B35EA `geareng_abort_code11` — logic
Abort wrapper: armed and 0x000B3506 true → reason 0x11, return 1; default 0. [O]

#### 0x000B3610 `geareng_substate_machine` — logic
Sub-state machine on +0x94 (0-8): 0/7/8 zero the timer +0x8C; 1→2 when timer 0; 3→4; 4
computes ramp targets +0xA0/+0xA2/+0x8C from vtable+0x50 plus gear table (PTR_FUN_000b38ec/
f0, /10 scaling) and calls vtable+0xC8 → 5; 5 reads vtable+0x14 → +0x8C → 6; 6→7; default
event 0x27B (0x85) + DAT_000b38fc recovery. Latches busy flag +0xA9 and +0xA8 =
(DAT_fff89ac0==5). Called from 0x000B2EA6. [I]

#### 0x000B37B0 `geareng_latch_field_a4` — data-accessor
When RAM global DAT_fff89ac0==6, stores sub-object (0x000B2EFC) vtable+0x20 result into
+0xA4, else 0. Prologue helper of 0x000B2EA6. [O]

#### 0x000B37DC `geareng_tick_snapshot` — logic
End-of-tick snapshot: for gear +0x9E in 1..5 copies the working set (+0x9E/+0x9A/+0x9C/
+0x95..98) into the mirror block +0xB9..+0xBD, reads two u16s by gear (PTR_FUN_000b3904/08),
the +0x88-object flag via vtable+0x20 (0x000B2FD8) and both bank counters (0x000B3060) into
+0xC2..+0xC8; out-of-range gear clears the whole mirror. [O]

#### 0x000B389E `geareng_ctr_scaled_calc` — logic
Reads a per-gear attempt counter (0x000B3060), picks cal table +0x54/+0x50 by param_3<1 and
runs an interpolation chain (PTR_FUN_000b3910/14 over (param_2+param_3, param_2) and a base
value +0x4C) returning s16. Callers 0x000CB7EC/0x000CBD0E (cross-region). [I]

#### 0x000B3918 `meas_state_machine` — logic
Small 0-8 state machine at +1 (state mirrored to global PTR_DAT_000b3a78): 1→2; 2 gated by
0x000B39B8 → 3 and latches via 0x000B3A24; 3 holds until gate drops (→8) or 0x000B3A1C
timer-zero → 4; 4→5; 6 runs 0x000B3A6C → 7; invalid → clear → 8. Called from 0x000CC3EA. [I]

#### 0x000B39B8 `meas_gate_check` — logic
Gate for 0x000B3918: mode byte *param_1 in {1,3} and two chained indirect checks
(PTR_FUN_000b3a80 then a84 with the prior result; impls 0xCC6DA/0xCC6F0 cross-region) → 1. [I]

#### 0x000B39F0 `meas_state_done` — logic
Returns 1 when the meas state (+1) is 5 or 7 (completed/finished). Callers 0xCC4A2/0xCC45E. [O]

#### 0x000B3A02 `meas_state_failed` — stub
4-instr predicate: state byte (+1) == 8 (failed/aborted). Called from 0xCC4DA. [O]

#### 0x000B3A0A `meas_set_state0` — stub
3-instr state setter: +1 = 0 (reset). Cluster of 3 with duplicated copies at 0x00103ACA and
0x00108470. Called from 0xCC442. [O]

#### 0x000B3A10 `meas_set_state1` — stub
3-instr state setter: +1 = 1 (start). Cluster of 2 with 0x001039EA. Called from 0xCC410. [O]

#### 0x000B3A16 `meas_set_state6` — stub
3-instr state setter: +1 = 6 (clear path). Called from 0xCC42C. [O]

#### 0x000B3A1C `meas_timer_zero` — stub
4-instr predicate: u16 at +8 == 0 (measurement window elapsed). [O]

#### 0x000B3A24 `meas_latch_values` — logic
Latch step (state 2→3): when mode==1 reads u16 +0x20; if below cal threshold
PTR_DAT_000b3a8c, latches +2 = obj+0x28 value, +4 = obj+0x2C s16 (flag +6 when nonzero),
+8 = obj+0x3C u16. [I]

#### 0x000B3A6C `meas_clear_fields` — stub
6-instr clear of the four result fields (+2/+4/+6/+8). [O]

#### 0x000B3A90 `init_obj_bc_subobjs` — init
Initializer: resolves the object (param or module registry id 0xBC via PTR_FUN_000b3d14) and
runs four indirect sub-object constructors on +0x34/+0x40/+0x70/+0x78; no resolved callers
(vtable entry, impls at 0xCC87E/0xCD238/0xCD808/0xCDCA0). [I]

#### 0x000B3ADE `geareng_scaled_calc_b` — logic
Second scaled calc (parallel to 0x000B389E) on the same struct: value from PTR_FUN_000b3d2c
(when +0x89==1) or by +0x94 via b3d30; cal +0x38/+0x34 by param_3<1; interpolation chain
3d38/3d3c/3d40 over (param_2+param_3, param_2). Callers 0xCDBB4/0xCDF42. [I]

#### 0x000B3B4C `geareng_report_gear_snapshot` — logic
Publishes the per-gear snapshot: clears RAM DAT_fff89b81-8A then, behind gate
PTR_FUN_000b3d48, writes the 7 values through PTR_FUN_000b3d50-80 indexed gear-1; tail calls
FUN_000b455a when +0x88>3. Same body as the 0x000B3400 tail; callers 0xCF700/0xCFAEC. [O]

#### 0x000B3BF4 `geareng_pos_code_6` — logic
When +0x88==6 computes a code from counters (+0x96%10, +0x84*10, -10 adjust) via
PTR_FUN_000b3d84; else logs event 0x254 with a 6-field RAM snapshot (DAT_fff89b6c..fff89b8c)
and returns 0. Mirror of 0x000B268E for state 6. Called from 0xCD712. [I]

#### 0x000B3C80 `geareng_arbiter6_select` — logic
231-instr arbiter over six entries (u16 keys +0xE.., u16 scores +0x1C..): stores three param
pairs via PTR_FUN_000b3d90/94 (indices 0-2), pairwise adds +5 score bias by key comparison,
picks the max at +0xAB; zero max → [0x9C]=1; otherwise winner byte checked against table
_LAB_000b3fc0 — parity mismatch clears [0xAA], else [0xAA]=winner and 0x000B4B56 flags may
reset the selection. Returns the selected byte. Called from 0x000B49FC (cross-region). [I]

#### 0x000B3F02 `geareng_request_gate` — logic
Pre-request gate for the 0x000B49FC path: selected gear +0x94==param_2 nonzero, +0x51!=3,
+0x88 not 9/10/11, 0x000B4B56 flag; then (mode window, incl. +0x61/+0xa9/+0x84 conditions)
DAT_000b3fd0 clear, gear gates 0x000B3FA2 and delta checks 0x000B4002/0x000B4022 all pass →
returns 0 (proceed), else 1. [I]

#### 0x000B3FA2 `geareng_gear_gate` — logic
Gear 1-5 gate: global flag (PTR_FUN_000b4288) and per-gear check (PTR_FUN_000b4290) both
zero → 1. Callers 0x000B3F02, 0x000B5086, 0x000B54E4. [O]

#### 0x000B4002 `geareng_delta_a2_check` — logic
Scaled-delta predicate: (s16 +0xA2 - *PTR_DAT_000b4298) through PTR_FUN_000b429c compared
against obj(+0x10) threshold; true = exceeds. [I]

#### 0x000B4022 `geareng_delta_a4_check` — logic
Byte variant of 0x000B4002: (+0xA4 - *PTR_DAT_000b42a0) scaled vs obj(+0xC) threshold. [I]

#### 0x000B4046 `geareng_setup_gear_request` — logic
272-instr gear-request setup from 0x000B49FC: validates gear 1-5 equals the arbiter
selection (else event 0x254/0x53 + 0x000B455a); latches +0x94, +0xA4/+0xA2 from cal; loads
seven per-gear config bytes/u16s (+0x89..+0x8E, +0x90/+0x92) when +0x2A[sel]==0; odd/even
selector parity sets mode +0x89=1/2, compares cal +0x2C row → flag +0x9F (+0xBA extra flag
mode 2); ticks sub-objects +0x70/+0x78/+0x40; computes +0x90/+0x92 offsets per mode from cal
tables; sets +0x88=4, +0x74/+0x7E by mode, +0x51=1; returns mode. [I] (spec 11 cross-ref)

#### 0x000B2F92 `evt27b_log3` — stub
7-instr event-log stub: forwards (obj, arg) to the event logger PTR_FUN_000b2fbc with fixed
event 0x27B; no resolved callers (pointer-table entry). [O]

## Unit summary

Unit 19 (r08_app, 0x000ADEC2-0x000B4046): 220 clusters, 259 functions named.

Counts by kind: logic 107, stub 86, data-accessor 37, library 23, dispatch 4, init 2.
`unknown` entries: none by kind; however the geareng_*/meas_*/shph2_* families describe
structure (state machines, gates, reason-code writers), not externally confirmed function —
all such entries are marked [I] in their bodies.

Module map of this unit:
- DTC snapshot/record accessors + UDS DID readers (0x000ADEC2-0x000AE564).
- Table accessor libraries (2-D word lookup, s16/s32 row getters + far copies in other regions).
- DTC mirror/session module (0x000AE868-0x000AE906) and forwarding-stub belt (0x000AE9AC-
  0x000AEF78) wiring remote objects at 0xAExxxx into impls across 0xA0xxx-0xD1xxx.
- Request engine (0x000AEF78-0x000AF8A4) and gear-pair / shift torque-phase family
  (0x000AF98A-0x000B13B8): 9x9 from/to tables (0x000B03A4/0x000B0424, spec 11).
- Shift-sequence executor (0x000AFC54 + 0x000B05D4-0x000B13B8) with torque-phase apply
  0x000B0860 and evaluator 0x000B108C (spec 11 phase engine).
- Phase-2 rpm/delta state machine (0x000B1448-0x000B2412, event 600/0x258 family).
- Gear-engagement module (0x000B268E-0x000B3B4C, 0x000B3C80-0x000B4046): 17-state machine
  0x000B2754, sub-state machine 0x000B3610, command machine 0x000B2EA6, per-gear attempt
  counters (banks +0xAE/+0xB3), reason codes 3-0x1A, events 0x27B/0x254/0x25D.
- Small measurement latch module (0x000B3918-0x000B3A6C) with impls at 0xCC4xx.

Cross-region leads:
- Heavy dependence on impls in the 0xCB0xx-0xD1C28 band (e.g. 0xCB080, 0xCB7EC, 0xCC3EA,
  0xCC40C-0xCC6F0, 0xCD676/0xCD6C0, 0xCD914, 0xCDBB4, 0xCF700) — the units owning that band
  should cross-check against these geareng_/meas_ stubs.
- Cluster members duplicated far away: 0x00151DB6/0x00151DC0/0x00151DCA/0x001552DA (table
  accessors), 0x0014802A/0x0014889E (check-byte clear), 0x001039EA/0x00103ACA/0x00108470
  (meas state setters) — same bodies, keep names in sync.
- geareng module continues past this unit's boundary: 0x000B455A, 0x000B49FC (arbiter
  caller), 0x000B4B56, 0x000B5086, 0x000B54E4, 0x000B5996 are callees of 0x000B3C80/
  0x000B3F02/0x000B4046 and belong to the next unit.
- Caution for next units: split-body functions exist (0x000B2EA6's code continues at
  0xCAB9C+); entry-address file headers there can hide large bodies.
- Event IDs seen (for the diag/event unit): 0x294 (shift family), 0x27B (engagement),
  0x254/0x25D (engagement reports), 600 decimal=0x258 logged as "600" (phase-2).
