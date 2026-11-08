# Unit 24 (r0c_app)

Region 0x000C0000-0x000C4A90 plus cross-region members. This band is the **EEPROM/NVM shadow-record
access layer** (spec 06 area): RAM record images with trailing 16-bit additive checksums, marker-ID
validation, per-record dirty/busy flags and state bytes at 0xFFF89F7C-0xFFF89FA4 (state 0x80 = valid).
Checksum helpers live at 0x000F71D0 (sum), 0x000F7214 (verify), 0x000F71EC (delta-adjust); they are
reached through function-pointer descriptor tables embedded after each function group. Recurring roles
per record: cks_ok / cks_update / marker_ok / idle / avail / clear / get / set. Record identity below
uses the marker/ID constant checked by the family (e.g. rec122). [I] on the EEPROM attribution.

#### 0x000C0000 `eeprom_rec122_cks_ok` — logic
Verifies the additive checksum of record "0x122" (shadow image at *0xC0128, cks@+0xd60, data@+0xd64, len 4)
by tail-calling the shared verifier through fnptr *0xC0140 (-> 0x000F7214). No resolved callers (called via
pointer/descriptor). Cluster of 1. [O] (behavior), [I] (record naming)
#### 0x000C001C `eeprom_rec122_cks_update` — logic
Recomputes the additive checksum of record 0x122 (data@+0xd64 -> cks@+0xd60, len 4) via fnptr *0xC0144
(-> 0x000F71D0) and sets the write-request flag *0xC0130 = 1. Cluster of 1. [O]
#### 0x000C004E `eeprom_rec122_marker_ok` — data-accessor
Returns *(short*)*0xC0148 == 0x122: the record's stored ID/marker must equal 0x122. Used by clear/set paths.
Cluster of 1. [O]
#### 0x000C0074 `eeprom_rec122_avail` — logic
Record usable when state byte DAT_fff89f84 == 0x80 (read through 2-instr getter 0x000C005E) AND no write
pending (*0xC014C == 0). Central guard for all rec122 accessors. Cluster of 1. [O]
#### 0x000C0098 `eeprom_rec122_idle` — data-accessor
Returns *0xC014C == 0 (no EEPROM write job pending for record 0x122). Cluster of 1. [O]
#### 0x000C00B8 `eeprom_rec122_b1_clear` — logic
If idle: if marker_ok, zero byte +0xd64 of the record image through the delta-checksum write helper
(*0xC012C -> 0x000F71EC, len 1), force cached value *0xC0150 = 0 and set write-request *0xC0130 = 1.
Called by 0x000C00A2 (group reset). Cluster of 1. [O]
#### 0x000C0102 `eeprom_rec122_b1_get` — data-accessor
Returns cached byte *0xC0150 when rec122_avail(); 0 otherwise. Caller 0x000CE3A0. Cluster of 1. [O]
#### 0x000C0154 `eeprom_rec122_b1_set` — logic
If avail: delta-checksum write of the new byte into +0xd64 (cks@+0xd60), sets write-request *0xC03E4 = 1
and caches the value at *0xC03E8. Called from the 0x28b-coded state machine 0x000CE038. Cluster of 1. [O]
#### 0x000C0194 `eeprom_rec122_b2_clear` — logic
Same clear-defaults pattern as 0x000C00B8 for byte +0xd65 (image via *0xC03DC, delta fn *0xC03E0),
zeroes *0xC03EC, sets *0xC03E4 = 1. Cluster of 1. [O]
#### 0x000C01DE `eeprom_rec122_b2_get` — data-accessor
Returns cached byte *0xC03EC when rec122_avail(); 0 otherwise. Cluster of 1. [O]
#### 0x000C0200 `eeprom_rec122_b2_set` — logic
Set byte +0xd65 variant of 0x000C0154 (delta-cks via *0xC03E0, cks@+0xd60, caches at *0xC03EC, dirty *0xC03E4).
Called by 0x000CE038. Cluster of 1. [O]
#### 0x000C0240 `eeprom_rec122_w_clear` — logic
Clear-defaults for the 16-bit element +0xd66 (local_8 is 2 bytes, len 2), zeroes *0xC03F0, sets dirty.
Cluster of 1. [O]
#### 0x000C028A `eeprom_rec122_w_get` — data-accessor
Returns cached word *0xC03F0 (as int) when rec122_avail(); 0 otherwise. Cluster of 1. [O]
#### 0x000C02A6 `eeprom_rec122_w_set` — logic
Set word +0xd66 (big-endian halves via &local+2, len 2), caches at *0xC03F0, sets dirty *0xC03E4 = 1.
Called by 0x000CE038. Cluster of 1. [O]
#### 0x000C033A `eeprom_rec128_cks_ok` — logic
Checksum verify for the next record in the same image: cks@+0xd68, data@+0xd6c, len 0x14 via fnptr *0xC03F8
(-> 0x000F7214). Marker for this family is 0x128. Cluster of 1. [O], [I] naming
#### 0x000C0356 `eeprom_rec128_cks_update` — logic
Recompute cks@+0xd68 over the 0x14-byte array at +0xd6c via *0xC03FC (-> 0x000F71D0); sets *0xC03E4 = 1.
Cluster of 1. [O]
#### 0x000C0388 `eeprom_rec128_marker_ok` — data-accessor
Returns *(short*)*0xC0400 == 0x128. Cluster of 1. [O]
#### 0x000C03AE `eeprom_rec128_avail` — logic
state DAT_fff89f8c == 0x80 (getter 0x000C0398) AND *0xC0404 == 0. Guard for the array accessors. Cluster of 1. [O]
#### 0x000C03D2 `eeprom_rec128_idle` — data-accessor
Returns *0xC0404 == 0. Cluster of 1. [O]
#### 0x000C0408 `eeprom_rec128_arr_clear` — logic
If idle: calls fnptr *0xC0694, then loops 5 times zeroing the 5x16-bit array at image+0xd6c (delta-cks writes,
len 2 each, via *0xC069C) when marker_ok; sets *0xC06A0 = 1. Caller 0x000BD778 (group reset in 0xBD band).
Member 0x000C04D6 identical (called by 0x000BDCE8). Cluster of 2. [O]
#### 0x000C048A `eeprom_rec128_arr_set` — logic
If avail: index write — computes image + idx*2 + 0xd6c, delta-cks write (len 2) of param_3, sets *0xC06A0 = 1,
caches into *0xC0698-based shadow. Members 0x000C048A/0x000C0558 are parallel copies. Cluster of 2. [O]
#### 0x000C05F0 `eeprom_rec1d3_cks_ok` — logic
Checksum verify cks@+0xd80 / data@+0xd84 (len 2) via *0xC06A8 (-> 0x000F7214); family marker 0x1d3.
Cluster of 1. [O], [I] naming
#### 0x000C060C `eeprom_rec1d3_cks_update` — logic
Recompute cks@+0xd80 via *0xC06AC (-> 0x000F71D0), len 2; sets *0xC06A0 = 1. Cluster of 1. [O]
#### 0x000C063E `eeprom_rec1d3_marker_ok` — data-accessor
Returns *(short*)*0xC06B0 == 0x1d3. Cluster of 1. [O]
#### 0x000C0664 `eeprom_rec1d3_avail` — logic
state DAT_fff89f94 == 0x80 (getter 0x000C064E) AND *0xC06B4 == 0. Cluster of 1. [O]
#### 0x000C0688 `eeprom_rec1d3_idle` — data-accessor
Returns *0xC06B4 == 0 (no pending write for the 0x1d3 word record). Cluster of 1. [O]
#### 0x000C06B8 `eeprom_rec1d3_w_clear` — logic
Clear-defaults for word +0xd84 (delta-cks len 2 via *0xC0944 when marker_ok), zeroes *0xC0948, sets
*0xC094C = 1. Called by 0x000CA176. Cluster of 1. [O]
#### 0x000C0702 `eeprom_rec1d3_w_get` — data-accessor
Returns cached word *0xC0948 when rec1d3_avail(); 0 otherwise. Callers 0x000CB348/0x000CB2F6. Cluster of 1. [O]
#### 0x000C0724 `eeprom_rec1d3_w_set` — logic
Set word +0xd84 (delta-cks len 2, cks@+0xd80), caches at *0xC0948, sets *0xC094C = 1. Caller 0x000CB2F6. Cluster of 1. [O]
#### 0x000C0764 `eeprom_rec182_grp_reset` — logic
Object/vtable-driven record-group reset: if slot +0xa4 (validate) returns true on the group object at
param_1, invokes slots +0x68, +0x50, +0x80 with the group's embedded arg offsets, clears *0xC0950, then
calls 0x000C0852 which clears both 5-byte arrays of record 0x182. Callers 0x000BD778/0x000BDCE8. Cluster of 1. [O]
#### 0x000C07B0 `eeprom_rec182_cks_ok` — logic
Checksum verify cks@+0xd86 / data@+0xd8a (len 10) via *0xC0954 (-> 0x000F7214). Family marker 0x182.
Cluster of 1. [O], [I] naming
#### 0x000C07CC `eeprom_rec182_cks_update` — logic
Recompute cks@+0xd86 (len 10) via *0xC0958 (-> 0x000F71D0); sets *0xC094C = 1. Cluster of 1. [O]
#### 0x000C07FE `eeprom_rec182_marker_ok` — data-accessor
Returns *(short*)*0xC095C == 0x182. Cluster of 1. [O]
#### 0x000C0824 `eeprom_rec182_avail` — logic
state DAT_fff89f9c == 0x80 (getter 0x000C080E) AND *0xC0960 == 0. Cluster of 1. [O]
#### 0x000C0848 `eeprom_rec182_idle` — data-accessor
Returns *0xC0960 == 0. Cluster of 1. [O]
#### 0x000C0862 `eeprom_rec182_arrA_clear` — logic
If idle: loop 5x zeroing byte image+0xd8a+i (delta-cks len 1, cks@+0xd86, when marker_ok); sets *0xC094C = 1.
Called via 0x000C0852. Cluster of 1. [O]
#### 0x000C08CC `eeprom_rec182_arrA_get` — data-accessor
Indexed get of array A byte via shadow LUT _LAB_000c0964[idx] when avail(); 0 otherwise. Cluster of 1. [O]
#### 0x000C08F2 `eeprom_rec182_arrA_set` — logic
Indexed set: delta-cks write of byte at image+idx+0xd8a (len 1, cks@+0xd86), caches into LUT _LAB_000c0964,
sets *0xC094C = 1. Cluster of 1. [O]
#### 0x000C0968 `eeprom_rec182_arrB_clear` — logic
Second 5-byte array (image+0xd8f+i, same cks@+0xd86): same clear loop as 0x000C0862, dirty flag *0xC0C34.
Image via *0xC0C2C. Cluster of 1. [O]
#### 0x000C09D2 `eeprom_rec182_arrB_get` — data-accessor
Indexed get of array B byte via shadow LUT *0xC0C38[idx] when avail(). Cluster of 1. [O]
#### 0x000C09F8 `eeprom_rec182_arrB_set` — logic
Indexed set: delta-cks write at image+idx+0xd8f (len 1, cks@+0xd86 via *0xC0C30), caches into *0xC0C38[idx],
sets *0xC0C34 = 1. Cluster of 1. [O]
#### 0x000C0A92 `eeprom_rec1d3b_cks_ok` — logic
Checksum verify for a second 0x1d3-marked record: cks@+0xd94 / data@+0xd98 (len 2) via *0xC0C40.
Cluster of 1. [O], [I] naming
#### 0x000C0AAE `eeprom_rec1d3b_cks_update` — logic
Recompute cks@+0xd94 (len 2) via *0xC0C44; sets *0xC0C34 = 1. Cluster of 1. [O]
#### 0x000C0AE0 `eeprom_rec1d3b_marker_ok` — data-accessor
Returns *(short*)*0xC0C48 == 0x1d3 (second storage with same ID). Cluster of 1. [O]
#### 0x000C0B06 `eeprom_rec1d3b_avail` — logic
state DAT_fff89fa4 == 0x80 (getter 0x000C0AF0) AND *0xC0C4C == 0. Callers 0x000AF38C/0x000AF370 (0xAF band). Cluster of 1. [O]
#### 0x000C0B2A `eeprom_rec1d3b_idle` — data-accessor
Returns *0xC0C4C == 0. Cluster of 1. [O]
#### 0x000C0B34 `eeprom_rec1d3b_w_clear` — logic
Clear-defaults word +0xd98 (delta-cks len 2 via *0xC0C30 when marker_ok), zeroes *0xC0C50, sets *0xC0C34 = 1. Cluster of 1. [O]
#### 0x000C0B7E `eeprom_rec1d3b_w_get` — data-accessor
Returns cached word *0xC0C50 when avail(); 0 otherwise. Caller 0x000AF370. Cluster of 1. [O]
#### 0x000C0BA0 `eeprom_rec1d3b_w_set` — logic
Set word +0xd98 (delta-cks len 2, cks@+0xd94), caches at *0xC0C50, sets *0xC0C34 = 1. Caller 0x000AF38C. Cluster of 1. [O]
#### 0x000C0C58 `eeprom_recc8d3_cks_ok` — logic
Checksum verify cks@+0xd9a / data@+0xd9e (len 2) via *0xC0E9C (-> 0x000F7214). This family's ID check
compares against -0x372d (0xC8D3). Cluster of 1. [O]
#### 0x000C0C74 `eeprom_recc8d3_cks_update` — logic
Recompute cks@+0xd9a (len 2) via *0xC0EA0; sets *0xC0EA4 = 1. Cluster of 1. [O]
#### 0x000C0CA6 `eeprom_recc8d3_marker_ok` — data-accessor
Returns *(short*)*0xC0EA8 == 0xC8D3 (decompiled as -0x372d). Cluster of 1. [O]
#### 0x000C0CF0 `eeprom_recc8d3_idle` — data-accessor
Returns *0xC0EAC == 0. Cluster of 1. [O]
#### 0x000C0CFA `eeprom_recc8d3_w_clear` — logic
Reset-defaults: writes **0xD431** (not 0) into word +0xd9e via delta-cks (len 2, cks@+0xd9a) when
marker_ok and idle; caches 0xD431 at *0xC0EB4, sets *0xC0EA4 = 1. Cluster of 1. [O]
#### 0x000C0DA4 `eeprom_rec1fe_s_cks_ok` — logic
Checksum verify for a small standalone record: image *0xC0EB8 with cks@+0, data@+4, len 6 via *0xC0E9C.
Family marker 0x1fe. Cluster of 1. [O], [I] naming
#### 0x000C0DB8 `eeprom_rec1fe_s_cks_update` — logic
Recompute cks@+0 over 6 data bytes via *0xC0EA0; sets *0xC0EBC = 1. Cluster of 1. [O]
#### 0x000C0DE2 `eeprom_rec1fe_s_marker_ok` — data-accessor
Returns *(short*)*0xC0EC0 == 0x1fe. Cluster of 1. [O]
#### 0x000C0E06 `eeprom_rec1fe_s_avail` — logic
state DAT_fff89fb4 == 0x80 (getter 0x000C0DF2) AND *0xC0EC4 == 0. Callers spread across 0xC/0xD/0xE bands. Cluster of 1. [O]
#### 0x000C0E2A `eeprom_rec1fe_s_idle` — data-accessor
Returns *0xC0EC4 == 0. Cluster of 1. [O]
#### 0x000C0E34 `eeprom_rec1fe_s_w1_clear` — logic
Clear word +4 to 0 (delta-cks len 2, image *0xC0EB8), zeroes *0xC0EC8, sets *0xC0EBC = 1. Cluster of 1. [O]
#### 0x000C0E76 `eeprom_rec1fe_s_w1_get` — data-accessor
Returns cached word *0xC0EC8 when avail(); caller 0x000D3D12. Cluster of 1. [O]
#### 0x000C0ECC `eeprom_rec1fe_s_w1_set` — logic
Set word +4 (image view *0xC116C, delta fn *0xC1170), caches at *0xC1178, sets *0xC1174 = 1.
Caller 0x000D3D12. Cluster of 1. [O]
#### 0x000C0F04 `eeprom_rec1fe_s_w2_clear` — logic
Clear word +6 to 0 (image *0xC116C), zeroes *0xC117C, sets *0xC1174 = 1. Cluster of 1. [O]
#### 0x000C0F46 `eeprom_rec1fe_s_w2_get` — data-accessor
Returns cached word *0xC117C when avail(); caller 0x000E8058. Cluster of 1. [O]
#### 0x000C0F68 `eeprom_rec1fe_s_w2_set` — logic
Set word +6 (image *0xC116C), caches at *0xC117C, dirty *0xC1174. Caller 0x000E8058. Cluster of 1. [O]
#### 0x000C0FA0 `eeprom_rec1fe_s_w3_clear` — logic
Clear word +8 to 0, zeroes *0xC1180, sets *0xC1174 = 1. Cluster of 1. [O]
#### 0x000C0FE2 `eeprom_rec1fe_s_w3_get` — data-accessor
Returns cached word *0xC1180 when avail(); caller 0x000D48AE. Cluster of 1. [O]
#### 0x000C1004 `eeprom_rec1fe_s_w3_set` — logic
Set word +8, caches at *0xC1180, dirty *0xC1174. Caller 0x000D48AE. Cluster of 1. [O]
#### 0x000C103C `eeprom_rec1fe_big_grp_reset` — logic
Master reset of the large 0x1fe adaptation record: when the group object's vtable slot +0xa4 validates,
runs slot +0x68, clears *0xC1184, then calls the 14 sub-array clear routines 0x000C11C2-0x000C1D76,
then slots +0x50/+0x80. Caller 0x000E8058 (0xE8 band). Cross-region lead: 0xE8058/0xE835E use this family. Cluster of 1. [O]
#### 0x000C10F0 `eeprom_rec1fe_big_cks_ok` — logic
Checksum verify over the large record: cks@+0x102, data@+0x106, len **0x786** via *0xC1188. Cluster of 1. [O]
#### 0x000C110E `eeprom_rec1fe_big_cks_update` — logic
Recompute cks@+0x102 over 0x786 bytes via *0xC118C; sets *0xC1174 = 1. Cluster of 1. [O]
#### 0x000C1142 `eeprom_rec1fe_big_marker_ok` — data-accessor
Returns *(short*)*0xC1190 == 0x1fe. Cluster of 1. [O]
#### 0x000C1194 `eeprom_rec1fe_big_avail` — logic
state DAT_fff89fbc == 0x80 (getter 0x000C1152) AND *0xC1460 == 0. Caller 0x000E835E. Cluster of 1. [O]
#### 0x000C11B8 `eeprom_rec1fe_big_idle` — data-accessor
Returns *0xC1460 == 0. Cluster of 1. [O]
#### 0x000C11C2 `eeprom_rec1fe_arr_clear` — logic
Sub-array clear of the big 0x1fe record: zeroes 0x510 bytes at image+0x106 (rep) / 0xC0 bytes at +0x616
(member 0x000C12A2), per-byte delta-cks (cks@+0x102) when marker_ok and idle; sets *0xC146C = 1.
Called by 0x000C103C. Cluster of 2. [O]
#### 0x000C1382 `eeprom_rec1fe_arr_clear__3` — logic
Sub-array clear: 0x18 bytes at image+0x6d6. Same pattern as 0x000C11C2. Cluster of 1. [O]
#### 0x000C147C `eeprom_rec1fe_arr_clear__4` — logic
Sub-array clears of the big 0x1fe record, per-byte delta-cks: rep +0x6ee (0x18 B), members 0x000C155A
+0x706 (0x18 B) and 0x000C1638 +0x71e (0xC B); dirty *0xC1720. Cluster of 3. [O]
#### 0x000C1730 `eeprom_rec1fe_arr_clear__7` — logic
Sub-array clears: rep +0x72a (0x18 B), members 0x000C180E +0x742 (0x18 B), 0x000C18EC +0x75a (0x18 B);
dirty *0xC19D4. Cluster of 3. [O]
#### 0x000C19E4 `eeprom_rec1fe_arr_clear__10` — logic
Sub-array clears: rep +0x772 (0x78 B), members 0x000C1AC2 +0x7ea (0x5D B), 0x000C1BA0 +0x847 (0x33 B);
dirty *0xC1C88. Cluster of 3. [O]
#### 0x000C1C98 `eeprom_rec1fe_arr_clear__13` — logic
Sub-array clear: 3 bytes at image+0x87a; dirty *0xC1F40. Cluster of 1. [O]
#### 0x000C1D76 `eeprom_rec1fe_arr_clear__14` — logic
Sub-array clear: 0xF bytes at image+0x87d; dirty *0xC1F40. Last of the 14 sub-clears from 0x000C103C. Cluster of 1. [O]
#### 0x000C1EA8 `eeprom_rec185_cks_ok` — logic
Checksum verify for standalone record: image *0xC1F4C, cks@+0, data@+4, len 0xE via *0xC1F50.
Family marker 0x185. Cluster of 1. [O], [I] naming
#### 0x000C1EBC `eeprom_rec185_cks_update` — logic
Recompute cks@+0 over 0xE bytes via *0xC1F54; sets *0xC1F58 = 1. Cluster of 1. [O]
#### 0x000C1EE6 `eeprom_rec185_marker_ok` — data-accessor
Returns *(short*)*0xC1F5C == 0x185. Cluster of 1. [O]
#### 0x000C1F0A `eeprom_rec185_avail` — logic
state DAT_fff89ffc == 0x80 (getter 0x000C1EF6) AND *0xC1F60 == 0. Cluster of 1. [O]
#### 0x000C1F2E `eeprom_rec185_idle` — data-accessor
Returns *0xC1F60 == 0. Cluster of 1. [O]
#### 0x000C1F64 `eeprom_rec185_w_clear` — logic
Clear word +4 to 0 (image view *0xC20A0, delta fn *0xC20A4), zeroes *0xC20A8, sets *0xC20AC = 1. Cluster of 1. [O]
#### 0x000C1FA6 `eeprom_rec185_w_set` — logic
Set word +4; caches at *0xC20A8; dirty *0xC20AC. Caller 0x000F3502 (0xF3 band). Cluster of 1. [O]
#### 0x000C1FDE `eeprom_rec185_arr_clear` — logic
Clears the 12-byte array at image+6 to 0 per-byte delta-cks (cks@+0); sets *0xC20AC = 1. Cluster of 1. [O]
#### 0x000C2038 `eeprom_rec185_arr_get` — data-accessor
Indexed get of the 12-byte array via shadow LUT *0xC20B0[idx] when avail(); callers 0x000CF1EC/0x000CF55E. Cluster of 1. [O]
#### 0x000C205E `eeprom_rec185_arr_set` — logic
Indexed set: delta-cks write at image+idx+6 (len 1), caches into *0xC20B0[idx], dirty *0xC20AC.
Callers 0x000CEEB4/0x000CF1EC (0xCE/CF band). Cluster of 1. [O]
#### 0x000C20BA `dtc_mon_debounce` — logic
Debounced event latch on a monitor struct: increments counter [+0xe] (cap 0xFF) until it reaches 10 AND
the condition fnptr (*0xC22F0 -> 0x00100F4C) is true AND [+8] not yet active; then latches [+8]=+0xa=+0xb=1
and bumps the 16-bit occurrence counter [+0xc] (cap 0xFFFE). Caller 0x000A4806 (0xA4 monitor band).
Spec 13 fault-event style. Cluster of 1. [O] behavior, [I] attribution
#### 0x000C211A `dtc_mon_release_a` — logic
Release of the [+0xa] latch: if [+8] was inactive, pushes event 0x33 into the 10-deep event history via
*0xC22F8 (-> 0x000F7EC8); clears [+8] and [+0xa]. Caller 0x000A48F0. Cluster of 1. [O]
#### 0x000C213E `dtc_mon_trigger` — logic
Arms the monitor: if [+0xb] and [+8] both clear, sets [+8]=+9=1; clears the one-shot [+0xb]. Caller 0x000A4906. Cluster of 1. [O]
#### 0x000C2166 `dtc_mon_release_b` — logic
Release of the [+9] latch, identical to 0x000C211A but for flag [+9]; pushes event 0x33 when ending.
Caller 0x000A4A88. Cluster of 1. [O]
#### 0x000C218C `vec_dispatch_thunk` — stub
3-instr thunk: tail-call through *0xC22FC (resolved to 0x000F7950, the large phase-dispatch engine with
150+ snippet callees in the 0x11D-0x11F band). No direct callers (vector/pointer table entry). Cluster of 2. [O]
#### 0x000C2192 `seq_poll5_update_obj` — logic
Fixed call sequence via fnptr table 0xC2300-0xC2324: two module calls, then polls one getter 5 times
storing results into object *0xC2310 slots [0x11],[0x17],[0x18],[0x19],[0x1b], then two more calls.
No resolved callers (table entry). Cluster of 1. [O] structure, [I] purpose
#### 0x000C21E4 `seq_reset_snapshot_cfg` — logic
Sequence: pushes event 0xFF (history clear) via *0xC22F8, refreshes modules via *0xC232C/0xC2334/0xC2338/
0xC2304/0xC2340, copies config bytes +0x5f..+0x64 from *0xC2344 into slots 5-10 of another struct, then
two final module calls. No resolved callers (table entry). Cluster of 1. [O] structure, [I] purpose
#### 0x000C2254 `seq_refresh_2mods` — logic
Three fnptr calls: *0xC235C(*0xC2308), registers *0xC2364, *0xC2368(*0xC2330). No resolved callers. Cluster of 1. [I]
#### 0x000C226A `seq_call_c2370` — logic
Single indirect call *0xC2370(DAT_000c236c). No resolved callers (table entry). Cluster of 1. [I]
#### 0x000C2272 `seq_guarded_call_1` — logic
Calls module *0xC2378(*0xC2328) unless mode byte *0xC2374 == 2. Member 0x000C22A6 identical with
fnptr *0xC2388 (-> 0x000F90E8). No resolved callers (table entries). Cluster of 2. [O]
#### 0x000C2284 `seq_guarded_call_3` — logic
Unless *0xC2374 == 2: three fnptr calls *0xC237C/0xC2380/0xC2384 (-> 0x000F9062/0x000F9088/0x000F90C4)
on object *0xC2328. Cluster of 1. [O]
#### 0x000C22B8 `seq_call_c2390` — logic
Single indirect call *0xC2390(DAT_000c238c) (-> 0x000F92A0). Cluster of 1. [I]
#### 0x000C22C2 `seq_call_c2398` — logic
Three parallel 4-instr thunks calling *0xC2398 / *0xC239C / *0xC23A0 (-> 0x000F8356/0x000F83A0/0x000F83E4)
with object *0xC2394. Cluster of 3 (0x000C22C2/0x000C22CA/0x000C22D2). [O]
#### 0x000C22DA `seq_call_23a4_2324` — logic
Two indirect calls: *0xC23A4(*0xC2394) (-> 0x000F83F2) then *0xC2324(*0xC2320). Also reached from
0x000F7864 (callee list). Cluster of 1. [O]
#### 0x000C23A8 `safe_ram_needs_init` — logic
Returns 0 if the redundant block's first pair is still the default {0,0xFF}; otherwise calls
0x000C23CA (mark modified) and returns 1. Called by 0x000C24EC. Start of the complementary-pair RAM
block module (pairs value/~value at +0..+0x15, counter +0x16/+0x18). Cluster of 1. [O]
#### 0x000C23D4 `safe_ram_blk_defaults` — logic
When gate (**PTR_PTR_000c2624)() == 0: resets the five byte-pairs of the redundant block to {0,0xFF}
and writes {1,0xFE} ("modified") at *0xC2628. Callers 0x000C263C/0x00122570. Cluster of 1. [O]
#### 0x000C241A `safe_ram_set_pair1` — logic
Stores byte param_2 with complement into block +0/+1 via 0x000C2696. Callers 0x00122570.
Cluster of 5 (0x000C241A pair1, 0x000C2424 pair2 (+2/+3), 0x000C2430 pair3 (+4/+5), 0x000C243C pair4 (+6/+7), 0x000C2448 pair5 (+8/+9)). [O]
#### 0x000C2454 `safe_ram_set_word_a` — logic
Stores 16-bit param_2 with complement at +0xa/+0xc via 0x000C26FA. Caller 0x000F9570. Cluster of 1. [O]
#### 0x000C2460 `safe_ram_set_word_b` — logic
Stores 16-bit param_2 with complement at +0xe/+0x10 via 0x000C26C8. Caller 0x000FA780. Cluster of 1. [O]
#### 0x000C2484 `safe_ram_counter_incr` — logic
Gated by (**PTR_PTR_000c2624)(): increments the 16-bit counter at +0x16 and refreshes its complement
at +0x18; marks the block modified. Called by 0x000C254A. Cluster of 1. [O]
#### 0x000C24AC `safe_ram_counter_clear` — logic
Gated: zeroes +0x16, sets +0x18 = 0xFFFF, marks modified. Called by 0x000C263C. Cluster of 1. [O]
#### 0x000C24D2 `safe_ram_verify_wrap` — logic
Bumps call-counter [0] at *0xC262C, runs the full complement check 0x000C2582, caches the boolean
result at *0xC2630. Caller 0x000A479E (0xA4 monitor band). Cluster of 1. [O]
#### 0x000C24EC `safe_ram_init_check_wrap` — logic
Bumps call-counter [1], runs 0x000C23A8 (needs-init), stores result flag at *0xC262C[0x19].
Caller 0x000A47AC. Cluster of 1. [O]
#### 0x000C250C `safe_ram_upd_delta136` — logic
Bumps call-counter [2]; computes (DAT_000c2634 - *0xC2638) + 0x88, clamps below 1 to 0, stores the
32-bit result at block+0x14 and returns the low 16 bits. Callers 0x000A47BA/0x000C254A. Cluster of 1. [O]
#### 0x000C2530 `safe_ram_reset_cond` — logic
Bumps call-counter [3]; if (param_2 & 6) != 0 performs the block reset 0x000C263C; always returns 1.
Caller 0x000A47C8. Cluster of 1. [O]
#### 0x000C254A `safe_ram_refresh` — logic
Bumps call-counter [6]; verifies the block (0x000C2582) and resets it (0x000C263C) on failure, then
increments the counter (0x000C2484) and updates the +0x14 delta (0x000C250C). Caller 0x000C2192. Cluster of 1. [O]
#### 0x000C256E `safe_ram_verify_or_reset` — logic
Verify (0x000C2582); if inconsistent, reset block (0x000C263C). Caller 0x000C21E4. Cluster of 1. [O]
#### 0x000C2582 `safe_ram_verify` — logic
Full integrity check: every pair (+0/+1..+8/+9, words +0xa/+0xc and +0xe/+0x10, bytes +0x12/+0x13,
+0x14/+0x15, counter +0x16/+0x18) must satisfy value == ~complement; result cached at *0xC262C[0x18].
Bumps call-counter [4]. Cluster of 1. [O]
#### 0x000C263C `safe_ram_blk_reset` — logic
Bumps call-counter [5]; clears the counter (0x000C24AC); if gate (**PTR_PTR_000c2730)() == 0 resets
byte-pairs via 0x000C23D4 and zeroes words +0xa..+0x15 (with 0xFFFF complements), marks modified;
else bumps call-counter [7]. Callers 0x000C2530/0x000C256E/0x000C254A. Cluster of 1. [O]
#### 0x000C2696 `safe_ram_set_pair` — logic
Primitive: gated by (**PTR_PTR_000c2730)(), stores param_2 and ~param_2 at the given addresses and
marks the block modified {1,0xFE} at *0xC2734; else bumps call-counter [9]. Cluster of 1. [O]
#### 0x000C26C8 `safe_ram_set_word_prim_b` — logic
Word variant of 0x000C2696 for block +0xe/+0x10: gated store of value + ~value, mark modified {1,0xFE};
else bump call-counter [10]. Caller 0x000C2460. Cluster of 1. [O]
#### 0x000C26FA `safe_ram_set_word_prim_a` — logic
Word-pair primitive used by 0x000C2454 (block +0xa/+0xc); bumps call-counter [0x10] when gated off. Cluster of 1. [O]
#### 0x000C2738 `park_set_const_2748` — stub
Writes 32-bit constant DAT_000c2748 into *0xC274C. Caller 0x000A70AA. Cluster of 1. [O]
#### 0x000C2740 `park_set_const_2750` — stub
Writes DAT_000c2750 into *0xC274C. Caller 0x000A713E. Cluster of 1. [O]
#### 0x000C2758 `park_req_cond_1e` — logic
Condition gate: if 0x000C2776 returns 1, records request (state 6, reason 0x1e) via 0x000C290E.
Called in the 0x000C28DC chain. Cluster of 1. [O] structure, [I] park attribution (spec 14)
#### 0x000C2776 `park_req_cond_1e_ok` — logic
Compound condition over mode *0xC2958 (5 or 7), struct flags at (*0xC2960)+9, and three fnptr predicates
(*0xC295C/0xC2964/0xC2968 -> 0xA7-band providers); returns 1 when the 0x1e-request applies. Cluster of 1. [O]
#### 0x000C27C4 `park_req_cond_23` — logic
If flag (*0xC2960)+10 set and mode 5 or 7 and fnptr *0xC2968 == 0 and *0xC295C != 0: record (6, 0x23). Cluster of 1. [O]
#### 0x000C2812 `park_req_cond_28` — logic
If flag +0xb set, mode 5/6/7, *0xC296C == 1, *0xC295C == 0 and *0xC2970 == 0: record (8, 0x28). Cluster of 1. [O]
#### 0x000C286C `park_req_cond_50` — logic
If flag +0xe == 1, *0xC2968 == 0, *0xC2974 == 0, mode 5/6/7, *0xC2978 == 0, *0xC295C == 0:
record (8, 0x50). Lowest priority of the four. Cluster of 1. [O]
#### 0x000C28DC `park_req_arbiter` — logic
Copies mode *0xC2958 into the request object, clears the reason byte, then evaluates the four request
conditions in priority order 0x2812 -> 0x2758 -> 0x27C4 -> 0x286C; first hit wins. Called by 0x000C2754. Cluster of 1. [O]
#### 0x000C290E `park_req_set_result` — logic
Records (state, reason) into the request object; if the pending state *0xC297C[2] is 8 or 6 and the new
state differs, fires event 0x1f via *0xC2980 (-> 0x000A7B36) instead. Cluster of 1. [O]
#### 0x000C2984 `park_set_const_2994` — stub
Writes DAT_000c2994 into *0xC2998. Caller 0x000A70AA (same caller as 0x000C2738 — paired stores). Cluster of 1. [O]
#### 0x000C298C `park_set_const_299c` — stub
Writes DAT_000c299c into *0xC2998. Caller 0x000A713E (paired with 0x000C2740). Cluster of 1. [O]
#### 0x000C29A0 `park_lever_refresh` — logic
Refresh of the lever-request object: copies mode *0xC2BC4 into +2, then runs 0x000C29C0 (input validity),
0x000C29E4 (arbitration), 0x000C2B86 (consistency) and 0x000C2BE4/0x000C2C04. Caller 0x000A58D4. Cluster of 1. [O], spec 14
#### 0x000C29C0 `park_lever_inputs_ok` — logic
Sets +7 = (fnptr *0xC2BC8() == 1 || fnptr *0xC2BCC() == 2) — lever input plausibility. Cluster of 1. [O]
#### 0x000C29E4 `park_lever_arbitrate` — logic
Main arbitration of the lever object: defaults (+4=1, +5=0), priority chain 0x2A28 -> 0x2AB4 -> 0x2ADC ->
0x2B06 -> 0x2B26, then countdown via 0x2B42 when +4 remains 1. Cluster of 1. [O]
#### 0x000C2A28 `park_lever_try_engage` — logic
If mode *0xC2BC4 == 7, delegates to 0x000C2A40 (park engage request). Returns the changed flag +5. Cluster of 1. [O]
#### 0x000C2A40 `park_lever_engage_req` — logic
When input struct flag (*0xC2BD0)[0] set, fnptr states 5/6, inputs not OK (+7==0) and *0xC2BD8 == 1:
latches target 6, phase 4, +5=1, +4=0 into the object. Cluster of 1. [O]
#### 0x000C2A9E `park_lever_set_target7` — logic
Result helper: clears object, sets +2 = 7, +5 = 1 (target 7 request recorded). Cluster of 1. [O]
#### 0x000C2AB4 `park_lever_modes5678` — logic
If mode is 8/7/6/5: clears target and phase, sets +5 = 1. Cluster of 1. [O]
#### 0x000C2ADC `park_lever_cond_release` — logic
If phase +3 != 0 and fnptr *0xC2BDC == 1: clears target/phase and flags +5 = 1. Cluster of 1. [O]
#### 0x000C2B06 `park_lever_abort4` — logic
If *0xC2BD8 == 0 and phase == 4: adopt target 7 via 0x000C2A9E. Cluster of 1. [O]
#### 0x000C2B26 `park_lever_abort4b` — logic
If phase == 4 and inputs OK (+7 == 1): adopt target 7 via 0x000C2A9E. Cluster of 1. [O]
#### 0x000C2B42 `park_lever_countdown` — logic
Decrements the object timer [0]; on expiry sets +1 = 1 and either adopts target 7 (phase 4, via 0x2A9E)
or sets target 0 with +5 = 1; clears timer and phase. Cluster of 1. [O]
#### 0x000C2B86 `park_lever_consistency` — logic
Timer/phase consistency guard: timer without phase (or phase without timer) fires event 0x1c via
*0xC2BE0 (-> 0x000A7B36) and clears both; else returns the active timer/phase value. Cluster of 1. [O]
#### 0x000C2BE4 `park_lever_copy_mode` — logic
Copies the lever mode (only values 5-8 accepted) from *0xC2C24[10] into lever object +6. Called by
0x000C29A0. Cluster of 1. [O]
#### 0x000C2C04 `park_lever_copy_target` — logic
If enabled (*0xC2C2C == 1) and the indirect pointer from *0xC2C28 is valid (!= -1), copies the target
byte into object +2. Cluster of 1. [O]
#### 0x000C2C30 `park_tgt_pick_lower` — logic
Park-target arbitration between two source objects: compares method +0x5c of each; picks param_2 when
it has no rival, the current slot is empty, or its key is lower; stores the object and its method +0x20
result. Vtable-driven. Cluster of 1. [O], spec 14 arbiter
#### 0x000C2C86 `park_reqobj_clear` — logic
Zeroes a request-object struct (words at +4/+8 and bytes at +0xc/+0xd set to 0xFF pattern). Caller 0x000A4DEA. Cluster of 1. [O]
#### 0x000C2C9A `park_reqobj_reset` — logic
Clears the secondary slot (+8) and its key, then runs the arbitration 0x000C2C30 when a primary object
exists. Caller 0x000A4E52. Cluster of 1. [O]
#### 0x000C2CBA `park_reqobj_arb8` — stub
2-instr tail call: re-arbitrate the secondary slot via 0x000C2C30(param+8). Caller 0x000A4E66. Cluster of 1. [O]
#### 0x000C2CBE `park_tgt_sched_tick` — logic
86-instr request-source scheduler: on a new pending source (param[2] != current) it stops the current
(method +0x68 with 0), activates the new one (with 1), reads a duration byte via method +0x44 scaled
x100, and counts it down; returns the expiry tick. Caller 0x000A4EFC. Cluster of 1. [O]
#### 0x000C2D70 `park_set_const_2d80` — stub
Writes DAT_000c2d80 into *0xC2D84. Caller 0x000A70AA. Cluster of 1. [O]
#### 0x000C2D78 `park_set_const_2d88` — stub
Writes DAT_000c2d88 into *0xC2D84. Caller 0x000A713E. Cluster of 1. [O]
#### 0x000C2D8C `park_set_const_2d9c` — stub
Writes DAT_000c2d9c into *0xC2DA0. Caller 0x000A70AA. Cluster of 1. [O]
#### 0x000C2D94 `park_set_const_2da4` — stub
Writes DAT_000c2da4 into *0xC2DA0. Caller 0x000A713E. Cluster of 1. [O]
#### 0x000C2DA8 `park_tbl_flag_lookup` — data-accessor
Gets an index via vtable +0x20 of the object, then returns table[(*0xC2FEC)+0x18][idx] == 1. No resolved
callers (table entry). Cluster of 1. [O]
#### 0x000C2DCC `park_tbl_word_lookup2` — data-accessor
Returns *(short*)(*0xC2FEC+4 + idx*2) using the vtable +0x20 index. Cluster of 1. [O]
#### 0x000C2DEC `park_tbl_byte_lookup` — data-accessor
Returns byte table[(*0xC2FEC)+0x14 + idx] using the vtable +0x20 index. Cluster of 1. [O]
#### 0x000C2E0C `park_tbl_word_lookup0` — data-accessor
Returns *(short*)(*0xC2FEC+0 + idx*2) using the vtable +0x20 index. Cluster of 1. [O]
#### 0x000C35E8 `park_set_const_35f8` — stub
Writes DAT_000c35f8 into *0xC35FC. Caller 0x000A70AA. Cluster of 1. [O]
#### 0x000C35F0 `park_set_const_3600` — stub
Writes DAT_000c3600 into *0xC35FC. Caller 0x000A713E. Cluster of 1. [O]
#### 0x000C3604 `lib_clear_byte` — stub
Trivial 3-instr helper: sets *param_1 = 0. Duplicated six times across the image (0x000C3604,
0x000D78B0 [caller 0x000BB1CE], 0x00104008, 0x0011839A, 0x0012BCA4, 0x00130B74) — cross-region
duplicated helper copies. Cluster of 6. [O]
#### 0x000C360A `park_lever_timer_upd` — logic
Updates the lever timer object: decrements [0] with expiry flag [1]; shortens/cancels on request-state
8 or lever-state 1; restarts when mode 8 pending with state 2; derives request-active flag [2]; remembers
the previous request in *0xC36C0. Caller 0x000A59E8. Cluster of 1. [O] structure, [I] naming
#### 0x000C36C4 `park_set_const_36d4` — stub
Writes DAT_000c36d4 into *0xC36D8. Caller 0x000A70AA. Cluster of 1. [O]
#### 0x000C36CC `park_set_const_36dc` — stub
Writes DAT_000c36dc into *0xC36D8. Caller 0x000A713E. Cluster of 1. [O]
#### 0x000C36E0 `park_set_const_36f0` — stub
Writes DAT_000c36f0 into *0xC36F4. Caller 0x000A70AA. Cluster of 1. [O]
#### 0x000C36E8 `park_set_const_36f8` — stub
Writes DAT_000c36f8 into *0xC36F4. Caller 0x000A713E. Cluster of 1. [O]
#### 0x000C36FC `park_grp_refresh5` — logic
Runs the five sub-refreshes of the park group: 0x000C3716, 0x000C39D4, 0x000C37BE, 0x000C3832, 0x000C3900.
Caller 0x000A5944. Cluster of 1. [O]
#### 0x000C3716 `park_speed_timer_upd` — logic
Computes a target word [6] from struct *0xC3920 (fields +0x20/+0x24/+0x22/+0x26 and fnptr *0xC3924
returning 8/6), tags its source [5] (0/1/2), decrements timer [0]; when the target exceeds the limits at
*0xC392C(+8/+0xc) while armed, latches [7]=[6] and reloads the timer from +0x10. Caller 0x000C36FC.
Cluster of 1. [O] structure, [I] naming
#### 0x000C37BE `park_speed2_timer_upd` — logic
Second target/speed watchdog of the park group (sub-refresh of 0x000C36FC, twin of 0x000C3716 but
driven by limits at *0xC392C +4/+8/+0x10): decrements timer [4] with expiry flag [6], latches target
[0x10] and reloads the timer when target [0xc] exceeds the limits, sets active flag [0x12] only when
mode [0xa] != 2. Caller 0x000C36FC. Cluster of 1. [O] structure, [I] naming
#### 0x000C3832 `park_req_flags_upd` — logic
Sub-refresh of 0x000C36FC: latches request-state flags on the park request object — [0x13] active
(mode [0xa]!=2 and target [0xe]!=0, or mode 2 with PTR_DAT_000c3930[0xc]/[0xd] gating and speed limit
via *PTR_DAT_000c3934+0x1c), [0x14] previous state, [0x15] edge/hold, [0x16] release-allowed
(fnptr *0xC3938 != 5 plus gate byte via *0xC392C+0x14 == 1). Caller 0x000C36FC. Cluster of 1.
[O] structure, [I] naming
#### 0x000C3900 `park_tgt_mode_fsm` — logic
Park-target mode FSM (sub-refresh of 0x000C36FC): cycles mode byte [8] through states 0-3 (copies [8]
to [9] each pass), consulting limits via *PTR_DAT_000c39fc (+0/+0xc), mode [0xa], latches [0x12]/[0x13]
and the override flag [0x17] produced by 0x000C39D4; returns the new mode. Caller 0x000C36FC.
Cluster of 1. [O] structure, [I] naming
#### 0x000C39D4 `park_tgt_override_flag` — logic
Sub-refresh of 0x000C36FC: sets override byte [0x17]=1 only when PTR_DAT_000c3a00[3]==1, [4]==0 and
PTR_DAT_000c3a04[0xc]==1 with [0xd]!=2 (latch pattern consumed by the 0x000C3900 mode FSM).
Caller 0x000C36FC. Cluster of 1. [O] structure, [I] naming
#### 0x000C3A08 `park_set_const_3a18` — stub
Writes DAT_000c3a18 into *0xC3A1C. Caller 0x000A70AA. Cluster of 1. [O]
#### 0x000C3A10 `park_set_const_3a20` — stub
Writes DAT_000c3a20 into *0xC3A1C. Caller 0x000A713E. Cluster of 1. [O]
#### 0x000C3A24 `park_set_const_3a34` — stub
Writes DAT_000c3a34 into *0xC3A38. Caller 0x000A70AA. Cluster of 1. [O]
#### 0x000C3A2C `park_set_const_3a3c` — stub
Writes DAT_000c3a3c into *0xC3A38. Caller 0x000A713E. Cluster of 1. [O]
#### 0x000C3A40 `park_timer_decr_speed_scaled` — logic
Speed-scaled timer decrement: amount = (*0xC3C6C * *(*0xC3C68+0x18))/1000 + *(*0xC3C68+0x14) is stored
to param[2] and subtracted from timer param[0] (floored at 0), clearing the expiry byte param[1].
No resolved callers (vector/pointer-table entry). Cluster of 1. [O] structure, [I] naming
#### 0x000C3A7E `park_lock_grp_refresh` — logic
Park-lock group refresh called from 0x000A59E8 (same park scheduler as 0x000C360A): runs speed-window
check 0x000C3D8A, P-button request eval 0x000C3CEA, the speed-scaled timer tick (thunk 0x000C3A94 of
0x000C3A40), then the engage state machine 0x000C3AE6. Cluster of 1. [O]
#### 0x000C3A94 `park_timer_decr_thunk` — stub
Byte-identical duplicate/thunk of 0x000C3A40 (speed-scaled timer decrement) invoked by 0x000C3A7E.
Cluster of 1. [O]
#### 0x000C3A98 `park_lock_cond_eval` — logic
Evaluates the enable byte [0xe] for the park-lock FSM: lever mode *0xC3C70==6 with fnptr checks
*0xC3C74/*0xC3C78 (results 1/2), or flags *0xC3C7C==0 / *0xC3C80==1. Callees 0x000A772A/0x000A7796.
Caller 0x000C3AE6. Cluster of 1. [O] structure, [I] naming
#### 0x000C3AE6 `park_lock_fsm` — logic
Park-lock engage FSM (221 instrs): state [0xd] 0=idle (long gate chain — modes 6 via
*0xC3C84/*0xC3C70, speed fnptrs *0xC3C88/*0xC3C8C, flags PTR_DAT_000c3c90[0x29]/[0x4f]/[0x15]/[0x46]/
[0x2a] — before arming timer param[0] and phase [3]=1), 1=counting (phase 2/3 from limit *0xC3C68+8 vs
timer; aborts to state 2 via fnptr *0xC3C98 or *PTR_DAT_000c3c9c==2), 2=engaged (restart path clamps
the timer to the minimum at *PTR_DAT_000c3dbc+4 and re-arms). Uses the enable byte from 0x000C3A98.
Caller 0x000C3A7E. Cluster of 1. [O] structure, [I] naming
#### 0x000C3CEA `park_lock_button_req` — logic
Sets button-request byte [0xf]=1 when both mode getters *0xC3DC0/*PTR_DAT_000c3dc4 return 6 and the
button fnptr *0xC3DC8 (values 0/1/2/3) matches its enable bitmask at *(*0xC3DBC+0x24); clears it when
any mode is 8/7/5 (also PTR_DAT_000c3dcc[2]). Caller 0x000C3A7E. Cluster of 1. [O] structure,
[I] naming
#### 0x000C3D8A `park_lock_speed_window` — logic
Sets speed-window byte [0x10]: 1 when *(*0xC3DBC+0xc) <= *(ushort*)(*0xC3DD0+0x78) and the value is
>= *(*0xC3DBC+0x10) — a [lo,hi] plausibility window for the park-lock group. Caller 0x000C3A7E.
Cluster of 1. [O] structure, [I] naming
#### 0x000C3DD4 `park_set_const_3de4` — stub
Writes DAT_000c3de4 into *0xC3DE8. Caller 0x000A70AA. Cluster of 1. [O]
#### 0x000C3DDC `park_set_const_3dec` — stub
Writes DAT_000c3dec into *0xC3DE8. Caller 0x000A713E. Cluster of 1. [O]
#### 0x000C3DF0 `park_set_const_3e00` — stub
Writes DAT_000c3e00 into *0xC3E04. Caller 0x000A70AA. Cluster of 1. [O]
#### 0x000C3DF8 `park_set_const_3e08` — stub
Writes DAT_000c3e08 into *0xC3E04. Caller 0x000A713E. Cluster of 1. [O]
#### 0x000C3E0C `park_req56_eval` — logic
Request evaluation called from 0x000A82E0: clears request [2]/done [3]; when the gate fnptr *0xC3F2C
returns 0, requests 6 via 0x000C3F1C if mode (*0xC3F28)==8 with conditions *0xC3F34==1/*0xC3F38!=0,
then runs post-check 0x000C3EEC; when the gate is 1, requests 5 (or 6 when the *0xC3F30/*0xC3F34
bypass conditions hold). Cluster of 1. [O] structure, [I] naming
#### 0x000C3EA0 `park_req56_timer` — logic
Two-stage countdown for the 0x000C3E0C requester: when mode (*0xC3F28)==5 it reloads byte [0] from
**PTR_DAT_000c3f3c, otherwise decrements [0] twice per pass setting the expiry flag [1] on the second.
Caller 0x000A82E0. Cluster of 1. [O] structure, [I] naming
#### 0x000C3EEC `park_req56_post` — logic
Post-check of the requester: when mode (*0xC3F28)==8, condition *0xC3F34==0 and fnptr *0xC3F40==0,
requests 6 via 0x000C3F1C and marks done [3]=1. Caller 0x000C3E0C. Cluster of 1. [O]
#### 0x000C3F1C `park_req56_latch` — logic
Latches the request byte at param+2 (writes param_2 only if currently 0 — first-wins request merge
used by 0x000C3E0C/0x000C3EEC). Cluster of 1. [O]
#### 0x000C3F44 `park_set_const_3f54` — stub
Writes DAT_000c3f54 into *0xC3F58. Caller 0x000A70AA. Cluster of 1. [O]
#### 0x000C3F4C `park_set_const_3f5c` — stub
Writes DAT_000c3f5c into *0xC3F58. Caller 0x000A713E. Cluster of 1. [O]
#### 0x000C3F60 `park_res8_clear` — stub
Trivial 3-instr: zeroes the arbitration-result byte at param+8 (reset half of the 0x000C3F66 group's
result slot). No resolved callers here (vector entry); duplicate 0x000CE302 is called by 0x000CE48C.
Cluster of 2. [O]
#### 0x000C3F66 `park_res8_arbiter` — logic
Result-arbiter refresh called from 0x000A82E0: runs the four producers 0x000C4190 (stage counter A),
0x000C4384 (stage counter B), 0x000C48DE (stage control) and 0x000C4A68 (report), then the abort chain
0x000C3FAE -> 0x000C3FBC -> 0x000C3FE2 -> 0x000C4074 (overrides from [0xb]/[9]/[0xa]); only if none
aborts, default-set via 0x000C40A2. Result byte at +8. Cluster of 1. [O]
#### 0x000C3FAE `park_res8_abort1` — data-accessor
Returns *PTR_DAT_000c4164 == 1 — first abort condition of the 0x000C3F66 arbiter chain. Cluster of 1. [O]
#### 0x000C3FBC `park_res8_abort2` — logic
Second abort: if *0xC4168==0 or !=5, or PTR_DAT_000c416c[2]==1 with [3]==1, clears result [8] and
returns 1 (abort). Caller 0x000C3F66. Cluster of 1. [O]
#### 0x000C3FE2 `park_res8_abort3` — logic
Third abort: selects a word from the array *(*0xC4174+0x20) by bits 0-2 of the mask at
*(ushort*)(*0xC4170+4) (with per-bit fallbacks), aborts when (mask & 0xfffc & word) != 0 — an
inhibit/fault bitmask matrix — clearing result [8]. Caller 0x000C3F66. Cluster of 1. [O]
#### 0x000C4074 `park_res8_override` — logic
Override cascade: result [8] takes [0xb] if nonzero, then [9], then [0xa] (last-wins priority); returns
whether any override was present. Caller 0x000C3F66. Cluster of 1. [O]
#### 0x000C40A2 `park_res8_default_set` — logic
Default path of the arbiter: stores the table selection from 0x000C40C2 into result [8]. Member
0x000DD79E is the same pattern calling its own local twin 0x000DD2C4 from caller 0x000DD2F8.
Cluster of 2. [O]
#### 0x000C40AE `park_mode_1_8_valid` — logic
Clamp helper: returns *(char*)PTR_DAT_000c417c when it is in 1..8, else 0 — validates a selector/mode
value for the 0x000C40C2 table lookup. Caller 0x000C40C2. Cluster of 1. [O]
#### 0x000C40C2 `park_res8_table_select` — logic
Table-driven default selection: picks a byte-array pointer from *0xC4174 (+0x68/+0x6c or +0x70/+0x74
depending on flag *0xC4184 and fnptr *0xC4188), indexes it with the selector *(short*)*0xC4180 & 3;
value 1 -> PTR_DAT_000c418c[0x7c], 2 -> [0x7d], 3 -> 0x000C40AE validation, else 0. Caller 0x000C40A2.
Cluster of 1. [O]
#### 0x000C4190 `park_lock_stage_cnt_a` — logic
Stage counter A (producer for the 0x000C3F66 arbiter, result slot [9]): steps 0 -> 1 when
*PTR_DAT_000c4348==1 with gate *(*0xC434C+0x60)==1; increments (bounded by *(*0xC434C+0x38)) on mode
*0xC4350==5 with fnptr conditions *0xC4354/*0xC4358 or *0xC435C; decrements on *0xC4360/*0xC4364;
resets to 1 on *0xC4368!=5 with gate +100; full reset to 0 under a 10-bit abort matrix (bits of
*(*0xC434C+0x1c), each gated by speed flags/fnptrs *0xC436C..*0xC4380). Caller 0x000C3F66. Cluster
of 1. [O] structure, [I] naming
#### 0x000C4384 `park_lock_stage_cnt_b` — logic
Stage counter B (result slot [0xa]), structural twin of 0x000C4190 on tables *0xC4598/*0xC459C and
fnptrs *0xC45A8..*0xC45D4: mode gate *PTR_DAT_000c4594==6, preloads from result [8] when a +2 flag is
set, bounded by *(*0xC4598+0x34), abort matrix *(*0xC4598+0x18). Caller 0x000C3F66. Cluster of 1.
[O] structure, [I] naming
#### 0x000C45D8 `park_lock_stage_ramp` — logic
Ramps the stage counter [0xb] up (bounded by *(*0xC47F4+0x30), while fnptr *0xC47F8==0) and down when
[0xf]==1; on reaching *PTR_DAT_000c47fc with gate *(*0xC47F4+0x54)==0 it sets the completion byte
[0xe]=1. Caller 0x000C48DE. Cluster of 1. [O] structure, [I] naming
#### 0x000C4642 `park_lock_stage_tmr_a` — logic
Timer A [2] with expiry [3]: decrements per pass; on expiry reloads counter [0xb] from
PTR_DAT_000c4800[0x53] (clamped >= 0) when [0xb]!=0; restarts from *(*0xC47F4+0x48) when disabled
[4]==0 and ([0x10]==1 with [2]==0, or [0x12]==1 with gate +0x4c); forces [2]=0 when [0x12]==0.
Caller 0x000C48DE. Cluster of 1. [O] structure, [I] naming
#### 0x000C46D8 `park_lock_stage_tmr_b` — logic
Timer B [4] with expiry [5]: mirror of 0x000C4642 on the other condition pair — reloads counter [0xb]
from *(*0xC47F4+0x30) on expiry, restarts from *(*0xC47F4+0x44) when [0xf]==1 and timers [2]/[4] are
done; forces [4]=0 when [0x11]==0. Caller 0x000C48DE. Cluster of 1. [O] structure, [I] naming
#### 0x000C4742 `park_lock_stage_timeout` — logic
Timeout gate on the stage ramp: starts countdown [6]/[7] from *(*0xC47F4+0x3c) while counter [0xb] is
below [0xd] and PTR_DAT_000c4800[0x53] (or below [0x7e] with [6]==0); on expiry forces [0xb]=
PTR_DAT_000c4800[0x7e] and sets completion [0xe]=1 when armed [0xc]==1; clears [6] otherwise.
Caller 0x000C48DE. Cluster of 1. [O] structure, [I] naming
#### 0x000C4804 `park_lock_stage_start` — logic
Starts the stage counter: only when mode fnptr *0xC49AC==5, PTR_DAT_000c49b0[0x52]==1, [0x66]==0,
counter [0xb]==0 and [9]==0; picks the initial-value profile pointer from *0xC49B8 (+0x28 when
[0x10]==1 and bit0 of +0x50, +0x2c when [0xf]==1 and bit1, else +0x24 when fnptr *0xC49BC==1 and
[0x13]==0 with bit2) and applies it via 0x000C48A6. Caller 0x000C48DE. Cluster of 1. [O] structure,
[I] naming
#### 0x000C48A6 `park_lock_stage_setval` — logic
Applies a start profile to counter [0xb]: profile 0 -> *PTR_DAT_000c49c0, 1 -> PTR_DAT_000c49b0[0x7e]-1
(when > 1), 2 -> *PTR_DAT_000c49c0+1; other values pass through. Caller 0x000C4804. Cluster of 1. [O]
#### 0x000C48DE `park_lock_stage_ctrl` — logic
Stage-control master (producer for 0x000C3F66, result [0xb]): latches four condition-pair bytes
[0xf]-[0x12] from fnptr pairs *0xC49C4..*0xC49E0, snapshots [0xd]=[0xb], runs the ramp/timers/start
chain (0x000C45D8, 0x000C4642, 0x000C46D8, 0x000C4742, 0x000C4804), maintains hold timer [0]/[1]
(reload from *(*0xC49B8+0x40) while [0xf]==1), and clears [0xb] when the hold expires, the level is
at/below *(*0xC4A7C+0x30), or the inhibit mask *(ushort*)*0xC4A88 / *PTR_DAT_000c4a8c fires; records
the inhibit bit to [0x13]. Caller 0x000C3F66. Cluster of 1. [O] structure, [I] naming
#### 0x000C4A68 `park_lock_stage_report` — data-accessor
Copies the stage counter to the report slot: [0x14] = [0xb] when nonzero (fourth producer of the
0x000C3F66 arbiter). Caller 0x000C3F66. Cluster of 1. [O]
#### 0x000C4A90 `park_set_const_4aa0` — stub
Writes DAT_000c4aa0 into *0xC4AA4. Caller 0x000A70AA. Cluster of 1. [O]

## Unit summary
- 220 clusters, 240 names total (rep + every member). Continued session: this pass documented the
  remaining 45 clusters 0x000C37BE-0x000C4A90 (park-group logic + const-setter stubs) on top of the
  175 clusters 0x000C0000-0x000C3716 (EEPROM shadow-record layer, DTC monitor latches, sequence
  helpers, safe-RAM redundant block, park request/lever arbitration).
- Kinds (names rows): logic 168, stub 37, data-accessor 35, unknown 0.
- `unknown` entries: none — every function got at least a structurally-observable name; the
  park-lock/staging semantics ([I] naming) rest on the caller context (0x000A59E8/0x000A82E0 park
  scheduler, 0x000A70AA/0x000A713E const-swap setters) rather than a spec doc.
- Cross-region leads: duplicated park-arbiter helpers live outside this unit — 0x000CE302 (caller
  0x000CE48C) and 0x000DD79E (caller 0x000DD2F8, local table-twin 0x000DD2C4); the units covering
  0x000CE000/0x000DD000 should cross-check their parallel instances. The `lib_clear_byte` cluster
  also spans 0x000D78B0/0x00104008/0x0011839A/0x0012BCA4/0x00130B74 (six copies). All park_ logic
  hangs off the 0x000A59xx/0x000A82xx park scheduler and swaps constants through *0xC27xx-0xC4AAx
  config pointers — the unit owning 0x000A7xxx holds the diag setter pair that drives them.
