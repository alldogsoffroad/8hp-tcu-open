# Unit 23 (r08_app)

Region 0x000BF972–0x000BFFB4 of the application flash (r08_app). Continues unit 22's
band directly: this is the per-variable accessor bank of the stored-variable EEPROM
layer (block 0x0304 family per `1034420267_eeprom_records.csv`, which places ~200
per-variable writer functions at 0xBC000–0xC0700). Every record family has a
descriptor in the code pointer table (`{image-base, checksum-update fn, dirty flag,
RAM mirror pointers, busy flag, record-id word}`), a fragment state byte in RAM
(FFF89F6C/74/7C must be 0x80 = loaded), and get/set/zero/verify/rechecksum functions
sharing the byte-sum helpers 0x000F71D0 (compute), 0x000F7214 (verify),
0x000F71EC (incremental update), all in r0c_app. Names follow unit 22's
`paramrec<id>_*` scheme; record IDs 0x1A7 and a second 0x1C7 section appear here.
No known_map.csv anchors exist in this address range.

---

#### 0x000BF972 `paramrec1c7_get_d4c` — data-accessor
Getter for the u16 stored-variable at image `*PTR_DAT_000bfb80 + 0xd4c` (record 0x1C7,
the slot zeroed by 0x000BF928 in unit 22): returns RAM mirror `*(*DAT_000bfb90)` when
the fragment is ready (0x000BF76A: state DAT_fff89f6c == 0x80 and busy flag
*PTR_DAT_000bf8dc == 0), else 0. Callers 0x000E08AC, 0x000E0D5E, 0x000E0A7A (r0c_app
parameter-object scaling code). Single-member cluster. [O] (spec: EEPROM block 0x0304
stored variables)

#### 0x000BF994 `paramrec1c7_set_d4c` — logic
Setter for the same u16 slot: passes `(param_2 >> 16)` through the checksum-update
helper 0x000F71EC as `(image+0xd4c, new, image+0xd44, 2)`, sets dirty flag
*PTR_DAT_000bfb88 = 1 and updates RAM mirror *DAT_000bfb90. Gated on 0x000BF76A
readiness. Caller 0x000E08AC only. Single-member cluster. [O]

#### 0x000BFA20 `paramrec1a7_verify` — logic
Verifies the stored checksum at `*PTR_DAT_000bfb80 + 0xd4e` against the byte sum of
the 4-byte record at `image + 0xd52` (record 0x1A7) via helper 0x000F7214. No
resolved callers — reached through a pointer table (one of PTR_FUN_000bfb98), i.e.
part of a table-driven load/validate sequence. Single-member cluster. [O]

#### 0x000BFA3C `paramrec1a7_rechecksum` — logic
Recomputes the checksum at `image_bfb80 + 0xd4e` from the 4 bytes at +0xd52
(0x000F71D0) and sets dirty flag *PTR_DAT_000bfb88 = 1 — repairs/refreshes the record
checksum so it gets rewritten. No resolved callers (pointer-table entry
PTR_FUN_000bfb9c). Single-member cluster. [O]

#### 0x000BFA6E `paramrec1a7_id_ok` — data-accessor
Record-ID check for record 0x1A7: returns `*(short *)*PTR_DAT_000bfba0 == 0x1a7`
(mirror of the record-ID word, exactly the pattern unit 22 named `paramrec1c7_id_ok`).
Called by the zero functions 0x000BFAD2/0x000BFBAC. Single-member cluster. [O]

#### 0x000BFA94 `paramrec1a7_ready` — logic
Fragment-readiness predicate for record 0x1A7: state byte DAT_fff89f74 (read through
2-instr deref stub 0x000BFA7E) == 0x80 AND busy flag *PTR_DAT_000bfba4 == 0. Used to
gate every 0x1A7 get/set (callers 0x000BFB1C/BFB3E/BFBF6/BFC18 and 0x000E2C2E).
Single-member cluster. [O]

#### 0x000BFAB8 `paramrec1a7_not_busy` — data-accessor
Returns busy flag *PTR_DAT_000bfba4 == 0 for the 0x1A7 record family. Called by
0x000BFAD2/0x000BFBAC. Single-member cluster. [O]

#### 0x000BFAD2 `paramrec1a7_zero_d52` — logic
If not busy and the record ID matches 0x1A7, writes 0 to the u16 at
`image_bfb80 + 0xd52` with incremental checksum update (0x000F71EC over cksum
+0xd4e), zeroes mirror *PTR_DAT_000bfba8 and sets dirty *PTR_DAT_000bfb88 = 1.
Called by 0x000BFAC2 (`paramrec1fd_reset_be__3` in unit 22 — a paired reset entry).
Single-member cluster. [O]

#### 0x000BFB1C `paramrec1a7_get_d52` — data-accessor
Getter: returns RAM mirror `*PTR_DAT_000bfba8` (u16) when 0x000BFA94 says the
fragment is ready, else 0. Callers 0x000E2C2E (r0c_app adaptation write path).
Single-member cluster. [O]

#### 0x000BFB3E `paramrec1a7_set_d52` — logic
Setter for the u16 at `image_bfb80 + 0xd52` (upper half of param_2): checksum-update
0x000F71EC over cksum +0xd4e, dirty *PTR_DAT_000bfb88 = 1, mirror *PTR_DAT_000bfba8
updated. Callers 0x000E2F46, 0x000E2EB2, 0x000E2C2E (adaptation/diagnostic write
path in r0c_app). Single-member cluster. [O]

#### 0x000BFBAC `paramrec1a7_zero_d54` — logic
Same zero pattern as 0x000BFAD2 but for the second 0x1A7 copy at
`image_bfe54 + 0xd54` (cksum at image_bfe54+0xd4e, mirror *PTR_DAT_000bfe5c zeroed,
dirty *PTR_DAT_000bfe60 = 1) — gated on the same 0x1A7 ID word at 0x000bfba0. Called
by 0x000BFAC2. Single-member cluster. [O]

#### 0x000BFBF6 `paramrec1a7_get_d54` — data-accessor
Getter for the second 0x1A7 copy: returns mirror `*PTR_DAT_000bfe5c` when fragment
ready (0x000BFA94, state DAT_fff89f74). Caller 0x000E2C2E. Note the paired setter
0x000BFC18 writes through mirror pointer 0x000bfba8 while this reads 0x000bfe5c —
the two mirror slots must alias the same RAM word (or the copies deliberately
diverge); worth a byte-level check. Single-member cluster. [O]

#### 0x000BFC18 `paramrec1a7_set_d54` — logic
Setter for the u16 at `image_bfe54 + 0xd54`: checksum-update 0x000F71EC (cksum
+0xd4e), dirty *PTR_DAT_000bfe60 = 1, mirror write through *PTR_DAT_000bfba8 (see
aliasing note at 0x000BFBF6). Callers 0x000E2F46, 0x000E2EB2, 0x000E2C2E.
Single-member cluster. [O]

#### 0x000BFC58 `objrec_vt_reset8` — logic
Object-method reset handler, continuing unit 22's `objrec_vt_resetN` series: if the
object's vtable predicate `(*(desc+0xa4))((*(desc+0x9c))+obj)` fires, calls vtable
+0x68 method, clears descriptor flag `*DAT_000bfe64 = 0`, runs
0x000BFD46 (`paramrec_reset_seq__6`: zeroes 0x1C7-second slots +0xd5a/+0xd5c/+0xd5e),
then calls vtable +0x50 and +0x80 methods. Invoked from the module-object iterators
0x000CA176 and 0x000A3E60 (r0c_app). Single-member cluster. [O]

#### 0x000BFCA4 `paramrec1c7b_verify` — logic
Verifies the checksum at `image_bfe54 + 0xd56` over the 6-byte record at +0xd5a
(second 0x1C7 record section; state DAT_fff89f7c) via 0x000F7214. No resolved
callers — pointer-table entry PTR_FUN_000bfe68, part of the load/validate sequence.
Single-member cluster. [O]

#### 0x000BFCC0 `paramrec1c7b_rechecksum` — logic
Recomputes the checksum at `image_bfe54 + 0xd56` from the 6 bytes at +0xd5a
(0x000F71D0) and sets dirty flag *PTR_DAT_000bfe60 = 1. No resolved callers
(pointer-table entry PTR_FUN_000bfe6c). Single-member cluster. [O]

#### 0x000BFCF2 `paramrec1c7b_id_ok` — data-accessor
Record-ID check for the second 0x1C7 section: `*(short *)*PTR_DAT_000bfe70 == 0x1c7`.
Called by the zero functions 0x000BFD5C/0x000BFE08/0x000BFEE2. Single-member
cluster. [O]

#### 0x000BFD18 `paramrec1c7b_ready` — logic
Fragment-readiness predicate: state byte DAT_fff89f7c (via 2-instr deref stub
0x000BFD02) == 0x80 AND busy flag *PTR_DAT_000bfe74 == 0. Gates all 0x1C7-second
get/set; 13 callers including 0x000BFDA6/0x000BFDC8/0x000BFE80/0x000BFEA2/
0x000BFF58/0x000BFF74 and the r0c_app diagnostic side (0x000CE00C, 0x000CE038,
0x000CE3E6, 0x000CE412, 0x000CE62A, 0x000CEDC6). Single-member cluster. [O]

#### 0x000BFD3C `paramrec1c7b_not_busy` — data-accessor
Returns busy flag *PTR_DAT_000bfe74 == 0. Gates the zero functions 0x000BFD5C/
0x000BFE08/0x000BFEE2. Single-member cluster. [O]

#### 0x000BFD5C `paramrec1c7b_zero_d5a` — logic
If not busy and record ID 0x1C7 matches, writes 0 to the u16 at `image_bfe54 +
0xd5a` with checksum update (0x000F71EC over cksum +0xd56), zeroes mirror
*PTR_DAT_000bfe78, sets dirty *PTR_DAT_000bfe60 = 1. Called by 0x000BFD46
(`paramrec_reset_seq__6`) and 0x000A3E60 (module start/stop iterator).
Single-member cluster. [O]

#### 0x000BFDA6 `paramrec1c7b_get_d5a` — data-accessor
Getter: returns RAM mirror `*PTR_DAT_000bfe78` when 0x000BFD18 says ready. Callers
0x000CE62A, 0x000CE00C (r0c_app diagnostic read path). Single-member cluster. [O]

#### 0x000BFDC8 `paramrec1c7b_set_d5a` — logic
Setter for the u16 at `image_bfe54 + 0xd5a` (upper half of param_2): checksum
update over +0xd56, dirty *PTR_DAT_000bfe60 = 1, mirror *PTR_DAT_000bfe78 updated.
Callers 0x000CE412, 0x000CE00C. Single-member cluster. [O]

#### 0x000BFE08 `paramrec1c7b_zero_d5c` — logic
Zeroes the u16 at `image_bfe54 + 0xd5c` (checksum +0xd56, mirror *PTR_DAT_000bfe7c,
dirty *PTR_DAT_000bfe60) when not busy and ID 0x1C7 matches. This is the second
image copy of the d5c variable whose get/set (0x000BFE80/0x000BFEA2) use the
`*PTR_DAT_000c0128` image — the same pattern unit 22 saw for record 0x1C7 d4a.
Callers 0x000CA176, 0x000BFD46, 0x000A3E60. Single-member cluster. [O]

#### 0x000BFE80 `paramrec1c7b_get_d5c` — data-accessor
Getter for the d5c variable via the `*PTR_DAT_000c0128` descriptor image: returns
mirror `*PTR_DAT_000c0124` when ready (0x000BFD18). Callers 0x000CE62A, 0x000CE3E6.
Single-member cluster. [O]

#### 0x000BFEA2 `paramrec1c7b_set_d5c` — logic
Setter for the u16 at `image_c0128 + 0xd5c`: checksum update over cksum +0xd56
(0x000F71EC), dirty *PTR_DAT_000c0130 = 1, mirror *PTR_DAT_000c0124 updated. Gated
on 0x000BFD18. Caller 0x000CE3E6 only. Single-member cluster. [O]

#### 0x000BFEE2 `paramrec1c7b_zero_d5e_stage` — logic
Staged one-shot reset: when descriptor stage byte `*PTR_DAT_000c0134 == 1`, clears
it, calls 0x000C0240/0x000C0194/0x000C00B8 (single-byte/u16 record resets in the
next fragment, state DAT_fff89f84, images c03dc/c0128 slots +0xd64/+0xd65/+0xd66),
then the standard zero of the u16 at `image_c0128 + 0xd5e` (cksum +0xd56, ID 0x1C7
gate, mirror *PTR_DAT_000c0138, dirty *PTR_DAT_000c0130), and finally increments the
stage byte to 2. Called by 0x000BFD46. Single-member cluster. [O]

#### 0x000BFF58 `paramrec1c7b_get_d5e` — data-accessor
Getter for the u16 d5e variable: returns mirror `*PTR_DAT_000c0138` when ready
(0x000BFD18). Callers 0x000CE3A0, 0x000CEDC6, 0x000CE038 (diagnostic read path).
Single-member cluster. [O]

#### 0x000BFF74 `paramrec1c7b_set_d5e` — logic
Setter for the u16 at `image_c0128 + 0xd5e`: checksum update over +0xd56, dirty
*PTR_DAT_000c0130 = 1, mirror *PTR_DAT_000c0138 updated. Caller 0x000CE038.
Single-member cluster. [O]

#### 0x000BFFB4 `objrec_vt_reset9` — logic
Twin of 0x000BFC58 for the next module object: vtable predicate (+0xa4/+0x9c), then
vtable +0x68 method, clears descriptor word `*PTR_DAT_000c013c = 0`, runs
0x000C00A2 (`paramrec_reset_seq__7`: resets the DAT_fff89f84-fragment records via
0x000C00B8/0x000C0194/0x000C0240), then vtable +0x50 and +0x80 methods. Callers
0x000CA176, 0x000A3E60. Single-member cluster. [O]

## Unit summary

29 clusters, 29 functions (all single-member — every accessor is byte-unique because
each embeds its own image-base/checksum/mirror constants).

By kind: logic 19, data-accessor 10, unknown 0.

Unknown entries: none — the family template (unit 22's `paramrec<id>_*`) resolves
every function mechanically; what the stored *values* mean remains unidentified, as
it does for the whole block 0x0304 bank in the EEPROM records CSV.

Cross-region leads:
- Shared byte-sum helpers 0x000F71D0 / 0x000F71EC / 0x000F7214 (r0c_app) are the
  library under every writer here; the r0c unit owning them should keep names
  `checksum_*`.
- The accessor bank continues at 0x000C00A2–0x000C0240 (r0c_app): the trio resets
  records of the fragment whose state byte is DAT_fff89f84 (ID check 0x000C004E,
  images `*PTR_DAT_000c03dc`/`c0128`, cksum +0xd60). Units covering 0xC0000+ should
  name them `paramrec<id>c_*` and cross-check against 0x000BFEE2 here.
- Callers worth anchors in later units: 0x000E2C2E/0x000E2EB2/0x000E2F46 (adaptation
  write path into record 0x1A7), 0x000CE00C/CE038/CE3E6/CE412/CE62A/CEDC6
  (diagnostic read/write of the 0x1C7-second records), 0x000E08AC/0x000E0A7A/
  0x000E0D5E (scaling of the 0x1C7 d4c value), and the module-object iterators
  0x000A3E60 / 0x000CA176 that drive `objrec_vt_reset*`.
- Open question (byte-level check needed): mirror-pointer aliasing — setter
  0x000BFC18 writes through `*PTR_DAT_000bfba8` while its getter 0x000BFBF6 reads
  `*PTR_DAT_000bfe5c`; likewise 0x000BFE08 (zero, image bfe54+0xd5c, mirror bfe7c)
  vs 0x000BFE80/0x000BFEA2 (get/set, image c0128+0xd5c, mirror c0124).
