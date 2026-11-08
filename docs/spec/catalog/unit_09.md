# Unit 09 (r04_boot)

Cluster documentation for region r04_boot slice 0x00076FBC–0x00079C08.

## Batch 1 — bitfield getters over the RAM status blocks (0xFFF843xx / 0xFFF90Dxx–0xFFF90Exx)

Context established for the whole unit: hundreds of 3–6-instruction getters in this range are
called by the giant application snapshot builders 0x00144108 (5322 instrs), 0x0014FC5C (1811),
0x00133C4E (576) and the pair 0x00133476/0x001517E0 — these collect state signals into packed
records (measurement/telemetry style). A recurring safety pattern reads DAT_fff84304 whose low
nibble holds a value and high nibble its one's-complement; a callback via PTR_FUN_000771d4 gates
whether a broken redundancy latches "last good value".

#### 0x00076FBC `st_get_90dbd_b3` — data-accessor
Returns bit 3 of RAM byte 0xFFF90DBD. One of the plain bit getters consumed by the snapshot
builders 0x00144108/0x0014FC5C/0x00133C4E. Cluster of 1. [O]

#### 0x00076FC8 `st_get_90dbd_b4_5` — data-accessor
Returns bits 4–5 (2-bit field) of 0xFFF90DBD. Same getter family as above. Cluster of 1. [O]

#### 0x00076FD8 `st_get_90dbf_hi` — data-accessor
Returns the high nibble of 0xFFF90DBF. Called only by 0x00144108. Cluster of 1. [O]

#### 0x00076FE6 `st_get_90dc0` — data-accessor
Returns byte 0xFFF90DC0 verbatim. Called only by 0x00144108. Cluster of 1. [O]

#### 0x00076FF0 `st_get_84304_lo_latched` — data-accessor
Redundancy-checked read of the value nibble pair at 0xFFF84304 (low nibble = value, high nibble
expected to be its complement). If the pair is consistent, or callback PTR_FUN_000771d4 does not
report bit 0x40, the low nibble is re-latched into 0xFFF93C04; otherwise the last good value is
held. Returns the latched byte as signed char. Calls 0x0006DF08. Cluster of 1. [O]

#### 0x0007703C `st_get_84304_lo_sext_inv` — data-accessor
Same complement-checked nibble pair at 0xFFF84304, but returns the one's-complement of the value
sign-extended (caches 0xF0|~value into 0xFFF84A09). Consumers of 0x00076FF0 and this pair get
opposite polarity encodings of the same safe value. Calls 0x0006DF08. Cluster of 1. [O]

#### 0x0007707A `st_get_84314_b6_invalid` — data-accessor
Reads char at 0xFFF84314 and merges a validity bit: bit 6 of the result is set only when the
redundancy pair at 0xFFF84304 is broken AND callback PTR_FUN_000771d4 reports bit 0x40. Calls
0x0006DF08. Cluster of 1. [O]

#### 0x000770C8 `st_get_8486e_b6_valid` — data-accessor
Mirror of 0x0007707A over byte 0xFFF8486E with inverted bit-6 polarity (set when the value is
considered good). Calls 0x0006DF08. Cluster of 1. [O]

#### 0x00077116 `st_get_84305_b0` — data-accessor
Returns bit 0 of 0xFFF84305. Cluster of 1. [O]

#### 0x00077122 `st_get_84305_b1` — data-accessor
Returns bit 1 of 0xFFF84305. Cluster of 1. [O]

#### 0x0007712E `st_get_84305_b2_3` — data-accessor
Returns the 2-bit field at bits 2–3 of 0xFFF84305. Cluster of 1. [O]

#### 0x0007713C `st_get_84305_b4` — data-accessor
Returns bit 4 of 0xFFF84305. Cluster of 2; member 0x00077148 returns bit 5 of the same byte
(same pattern, different mask — duplicated getter family). [O]

#### 0x00077154 `st_get_90ddd_b5_inv` — data-accessor
Returns 0xFE|bit5 of 0xFFF90DDD, i.e. 0xFF when bit 5 set, else 0xFE (inverted-style boolean
encoding). Cluster of 1. [O]

#### 0x00077162 `st_get_84305_hi` — data-accessor
Returns the high 2 bits of 0xFFF84305. Called only by 0x00144108. Cluster of 1. [O]

#### 0x00077170 `st_get_84306` — data-accessor
Returns byte 0xFFF84306 verbatim. Cluster of 1. [O]

#### 0x0007717A `st_get_90dde` — data-accessor
Returns byte 0xFFF90DDE verbatim. Cluster of 1. [O]

#### 0x00077184 `st_get_84307_hi` — data-accessor
Returns the high nibble of 0xFFF84307. Cluster of 1. [O]

#### 0x00077192 `st_get_90ddf_hi_sext` — data-accessor
Returns high nibble of 0xFFF90DDF OR 0xF0 (sign-extended negative form). Cluster of 1. [O]

#### 0x000771A2 `st_get_84308` — data-accessor
Returns byte 0xFFF84308 verbatim. Called only by 0x00144108. Cluster of 1. [O]

#### 0x000771B8 `st_get_90de8_b2_4` — data-accessor
Returns the 3-bit field at bits 2–4 of 0xFFF90DE8. Cluster of 1. [O]

#### 0x000771DC `st_get_90de8_b5_7` — data-accessor
Returns the top 3 bits of 0xFFF90DE8. Cluster of 1. [O]

#### 0x000771EA `st_get_90df4_b0_1` — data-accessor
Returns the 2-bit field at bits 0–1 of 0xFFF90DF4. Consumers 0x00133476/0x001517E0. Cluster of 1. [O]

#### 0x000771F4 `st_get_90df4_b2_7` — data-accessor
Returns the top 6 bits of 0xFFF90DF4. Cluster of 1. [O]

#### 0x00077200 `st_get_90df5` — data-accessor
Returns byte 0xFFF90DF5 verbatim. Cluster of 1. [O]

#### 0x0007720A `st_get_90df6_b0_4` — data-accessor
Returns the low 5 bits of 0xFFF90DF6. Cluster of 1. [O]

#### 0x00077214 `st_get_90df6_b7` — data-accessor
Returns bit 7 of 0xFFF90DF6. Cluster of 1. [O]

#### 0x00077220 `st_get_90df7_b0_1` — data-accessor
Returns the 2-bit field at bits 0–1 of 0xFFF90DF7. Cluster of 1. [O]

#### 0x0007722A `st_get_90df7_b2` — data-accessor
Returns bit 2 of 0xFFF90DF7. Cluster of 1. [O]

#### 0x00077236 `st_get_90df7_hi` — data-accessor
Returns the high nibble of 0xFFF90DF7. Cluster of 1. [O]

#### 0x00077244 `st_get_90df9_lo` — data-accessor
Returns the low nibble of 0xFFF90DF9. Cluster of 1. [O]

#### 0x0007724E `st_get_90dfb_b6` — data-accessor
Returns bit 6 of 0xFFF90DFB. Cluster of 2; member 0x0007725A returns bit 7 of the same byte
(same pattern, adjacent flag). [O]

#### 0x00077266 `st_get_90e04_b0_1` — data-accessor
Returns the 2-bit field at bits 0–1 of 0xFFF90E04. Cluster of 1. [O]

#### 0x00077270 `st_get_90e05_b4` — data-accessor
Returns bit 4 of 0xFFF90E05. Cluster of 1. [O]

#### 0x0007727C `st_get_90e07_b4` — data-accessor
Returns bit 4 of 0xFFF90E07; called only by 0x00144108. Cluster of 1. [O]

#### 0x00077288 `st_get_90e14_b0_1` — data-accessor
Returns the 2-bit field at bits 0–1 of 0xFFF90E14. Cluster of 1. [O]

#### 0x00077292 `st_get_90e17_b6` — data-accessor
Returns bit 6 of 0xFFF90E17. Cluster of 1. [O]

#### 0x0007729E `st_get_90e1b_b3` — data-accessor
Returns bit 3 of 0xFFF90E1B. Cluster of 1. [O]

#### 0x000772AA `st_get_90e1b_b4` — data-accessor
Returns bit 4 of 0xFFF90E1B. Cluster of 2; member 0x000772B6 returns bit 7 of the same byte. [O]

#### 0x000772C2 `st_get_90e24_b6_7` — data-accessor
Returns the top 2 bits of 0xFFF90E24. Cluster of 1. [O]

#### 0x00077300 `st_get_90e34_b0_1` — data-accessor
Returns the 2-bit field at bits 0–1 of 0xFFF90E34; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x0007730A `st_get_90e34_b4_6` — data-accessor
Returns the 3-bit field at bits 4–6 of 0xFFF90E34; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x00077326 `st_get_90e44_b0_1` — data-accessor
Returns the 2-bit field at bits 0–1 of 0xFFF90E44. Cluster of 1. [O]

#### 0x00077330 `st_get_90e44_b3` — data-accessor
Returns bit 3 of 0xFFF90E44. Cluster of 1. [O]

#### 0x0007733C `st_get_90e44_b4` — data-accessor
Returns bit 4 of 0xFFF90E44. Cluster of 1. [O]

#### 0x00077348 `st_get_90e45_b6_7` — data-accessor
Returns the top 2 bits of 0xFFF90E45. Cluster of 1. [O]

#### 0x00077356 `st_get_90e46_b3_4` — data-accessor
Returns the 2-bit field at bits 3–4 of 0xFFF90E46. Cluster of 1. [O]

#### 0x00077366 `st_get_90e46_b7` — data-accessor
Returns bit 7 of 0xFFF90E46; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x00077372 `st_get_90e54_b0_1` — data-accessor
Returns the 2-bit field at bits 0–1 of 0xFFF90E54. Cluster of 1. [O]

#### 0x0007737C `st_get_90e55_b0` — data-accessor
Returns bit 0 of 0xFFF90E55. Cluster of 1. [O]

#### 0x00077388 `st_get_90e64_b0_1` — data-accessor
Returns the 2-bit field at bits 0–1 of 0xFFF90E64; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x00077392 `st_get_90e65` — data-accessor
Returns byte 0xFFF90E65; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x0007739C `st_get_90e66` — data-accessor
Returns byte 0xFFF90E66; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x000773A6 `st_get_90e67` — data-accessor
Returns byte 0xFFF90E67; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x000773B0 `st_get_90e68` — data-accessor
Returns byte 0xFFF90E68; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x000773BA `st_get_90e69` — data-accessor
Returns byte 0xFFF90E69; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x000773C4 `st_get_90e6a` — data-accessor
Returns byte 0xFFF90E6A; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x000773CE `st_get_90e6b` — data-accessor
Returns byte 0xFFF90E6B; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x000773D8 `st_get_8402c` — data-accessor
Returns byte 0xFFF8402C as int. First of a getter series reading byte 0 of records at 16-byte
stride in the 0xFFF840xx block (0xFFF8402C…0xFFF8410C) — a per-component state array. Consumers
0x00132D8E/0x001513F8/0x0014FC5C. Cluster of 1. [O]

#### 0x000773E0 `st_get_8403c` — data-accessor
Returns byte 0xFFF8403C (record +0x10 in the same stride-16 array). Widely used (6 callers
including 0x0013854A, 0x00133C4E). Cluster of 1. [O]

#### 0x000773E8 `st_get_84844` — data-accessor
Returns byte 0xFFF84844. Cluster of 1. [O]

#### 0x000773F0 `st_get_8404c` — data-accessor
Returns byte 0xFFF8404C (record +0x20 of the stride-16 array). Cluster of 1. [O]

#### 0x000773F8 `st_get_8405c` — data-accessor
Returns byte 0xFFF8405C (record +0x30). Cluster of 1. [O]

#### 0x00077400 `st_get_8407c` — data-accessor
Returns byte 0xFFF8407C (record +0x50). Cluster of 1. [O]

#### 0x00077408 `st_get_8408c` — data-accessor
Returns byte 0xFFF8408C (record +0x60). Cluster of 1. [O]

#### 0x00077410 `st_get_8409c` — data-accessor
Returns byte 0xFFF8409C (record +0x70). Cluster of 1. [O]

#### 0x00077418 `st_get_840ac` — data-accessor
Returns byte 0xFFF840AC (record +0x80). Cluster of 1. [O]

#### 0x00077420 `st_get_840bc` — data-accessor
Returns byte 0xFFF840BC (record +0x90); only 0x001513F8 consumes it. Cluster of 1. [O]

#### 0x00077428 `st_get_840cc` — data-accessor
Returns byte 0xFFF840CC (record +0xA0). Notably also called from 0x000AE5A0 (outside the usual
collector set). Cluster of 1. [O]

#### 0x00077430 `st_get_8484a` — data-accessor
Returns byte 0xFFF8484A. Cluster of 1. [O]

#### 0x00077438 `st_get_840dc` — data-accessor
Returns byte 0xFFF840DC (record +0xB0). Consumers include 0x00151784. Cluster of 1. [O]

#### 0x00077440 `st_get_840ec` — data-accessor
Returns byte 0xFFF840EC (record +0xC0). Cluster of 1. [O]

#### 0x00077448 `st_get_84850` — data-accessor
Returns byte 0xFFF84850. Cluster of 1. [O]

#### 0x00077450 `st_get_840fc` — data-accessor
Returns byte 0xFFF840FC (record +0xD0). Cluster of 1. [O]

#### 0x00077458 `st_get_8410c` — data-accessor
Returns byte 0xFFF8410C (record +0xE0); only 0x001513F8 consumes it. Cluster of 1. [O]

#### 0x00077460 `st_get_8411c` — data-accessor
Returns byte 0xFFF8411C as int — record +0xF0 of the stride-16 array; also called by 0x000AE5A0 and 0x00151754. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x00077468 `st_get_8412c` — data-accessor
Returns byte 0xFFF8412C as int — record +0x100; only 0x001513F8 consumes it. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x00077470 `st_get_8413c` — data-accessor
Returns byte 0xFFF8413C as int — record +0x110. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x00077478 `st_get_8414c` — data-accessor
Returns byte 0xFFF8414C as int — record +0x120. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x00077480 `st_get_8415c` — data-accessor
Returns byte 0xFFF8415C as int — record +0x130. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x00077488 `st_get_8416c` — data-accessor
Returns byte 0xFFF8416C as int — record +0x140. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x00077490 `st_get_8417c` — data-accessor
Returns byte 0xFFF8417C as int — record +0x150; consumers 0x001513F8/0x00151754. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x00077498 `st_get_8418c` — data-accessor
Returns byte 0xFFF8418C as int — record +0x160; also called by 0x000AE5A0. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x000774A0 `st_get_8419c` — data-accessor
Returns byte 0xFFF8419C as int — record +0x170. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x000774A8 `st_get_841bc` — data-accessor
Returns byte 0xFFF841BC as int — record +0x190. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x000774B0 `st_get_841cc` — data-accessor
Returns byte 0xFFF841CC as int — record +0x1A0; only 0x001513F8 consumes it. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x000774B8 `st_get_841ec` — data-accessor
Returns byte 0xFFF841EC as int — record +0x1C0. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x000774C0 `st_get_84856` — data-accessor
Returns byte 0xFFF84856 as int — byte on the 0xFFF848xx block. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x000774C8 `st_get_841fc` — data-accessor
Returns byte 0xFFF841FC as int — record +0x1D0. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x000774D0 `st_get_8420c` — data-accessor
Returns byte 0xFFF8420C as int — record +0x1E0. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x000774D8 `st_get_8485c` — data-accessor
Returns byte 0xFFF8485C as int — byte on the 0xFFF848xx block; only 0x0014FC5C consumes it. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x000774E0 `st_get_8421c` — data-accessor
Returns byte 0xFFF8421C as int — record +0x1F0. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x000774E8 `st_get_8423c` — data-accessor
Returns byte 0xFFF8423C as int — record +0x210; also called by 0x00133946. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x000774F0 `st_get_8424c` — data-accessor
Returns byte 0xFFF8424C as int — record +0x220; also called by 0x00133946 and 0x0013854A. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x000774F8 `st_get_8425c` — data-accessor
Returns byte 0xFFF8425C as int — record +0x230. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x00077500 `st_get_8426c` — data-accessor
Returns byte 0xFFF8426C as int — record +0x240. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x00077508 `st_get_8428c` — data-accessor
Returns byte 0xFFF8428C as int — record +0x260; only 0x001513F8 consumes it. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x00077510 `st_get_8429c` — data-accessor
Returns byte 0xFFF8429C as int — record +0x270. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x00077518 `st_get_842ac` — data-accessor
Returns byte 0xFFF842AC as int — record +0x280. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x00077520 `st_get_842bc` — data-accessor
Returns byte 0xFFF842BC as int — record +0x290. Part of the
stride-16 record-array getter series over 0xFFF8402C–0xFFF842BC consumed by the snapshot
collectors. Cluster of 1. [O]

#### 0x00077528 `st_get_842cc` — data-accessor
Returns byte 0xFFF842CC (record +0x2A0 of the stride-16 array). Cluster of 1. [O]

#### 0x00077530 `st_get_842dc` — data-accessor
Returns byte 0xFFF842DC (record +0x2B0). Cluster of 1. [O]

#### 0x00077538 `st_get_842ec` — data-accessor
Returns byte 0xFFF842EC (record +0x2C0); only 0x001513F8 consumes it. Cluster of 1. [O]

#### 0x00077540 `st_get_842fc` — data-accessor
Returns byte 0xFFF842FC (record +0x2D0). Cluster of 1. [O]

#### 0x00077548 `st_get_84868` — data-accessor
Returns byte 0xFFF84868. Cluster of 1. [O]

#### 0x00077550 `st_get_84314_raw` — data-accessor
Returns byte 0xFFF84314 raw — the same cell 0x0007707A decorates with the validity bit. Cluster
of 1. [O]

#### 0x00077558 `st_get_84324` — data-accessor
Returns byte 0xFFF84324 (record +0x2F0). Cluster of 1. [O]

#### 0x00077560 `st_get_84334` — data-accessor
Returns byte 0xFFF84334 (record +0x300). Cluster of 1. [O]

#### 0x00077568 `st_get_84344` — data-accessor
Returns byte 0xFFF84344 (record +0x310). Cluster of 1. [O]

#### 0x00077570 `st_get_84354` — data-accessor
Returns byte 0xFFF84354 (record +0x320). Cluster of 1. [O]

#### 0x00077578 `st_get_84364` — data-accessor
Returns byte 0xFFF84364 (record +0x330). Cluster of 1. [O]

#### 0x00077580 `st_get_84374` — data-accessor
Returns byte 0xFFF84374 (record +0x340); only 0x001513F8 consumes it. Cluster of 1. [O]

#### 0x00077588 `st_get_84384` — data-accessor
Returns byte 0xFFF84384 (record +0x350). Cluster of 1. [O]

#### 0x00077590 `st_get_84394` — data-accessor
Returns byte 0xFFF84394 (record +0x360). Cluster of 1. [O]

#### 0x00077598 `st_get_843a4` — data-accessor
Returns byte 0xFFF843A4 (record +0x370); only 0x001513F8 consumes it. Cluster of 1. [O]

#### 0x000775B4 `st_get_90f14_b0_2` — data-accessor
Returns the 3-bit field at bits 0–2 of 0xFFF90F14. Cluster of 1. [O]

#### 0x000775BE `st_get_90f24_b0_2_inv` — data-accessor
Returns (0xFFF90F24 & 7) | 0xF8 — the 3-bit field in inverted/sign-extended byte form. Cluster
of 1. [O]

#### 0x000775CA `st_get_90f14_b3_5` — data-accessor
Returns the 3-bit field at bits 3–5 of 0xFFF90F14. Cluster of 1. [O]

#### 0x000775DA `st_get_90f14_b6` — data-accessor
Returns bit 6 of 0xFFF90F14; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x000775E6 `st_get_90f15_b0` — data-accessor
Returns bit 0 of 0xFFF90F15. Cluster of 1. [O]

#### 0x000775F2 `st_get_90f15_b1` — data-accessor
Returns bit 1 of 0xFFF90F15. Cluster of 1. [O]

#### 0x000775FE `st_get_90f15_b2` — data-accessor
Returns bit 2 of 0xFFF90F15. Cluster of 1. [O]

#### 0x0007760A `st_get_90f15_b3` — data-accessor
Returns bit 3 of 0xFFF90F15. Cluster of 1. [O]

#### 0x00077616 `st_get_90f15_b4_5` — data-accessor
Returns the 2-bit field at bits 4–5 of 0xFFF90F15. Cluster of 1. [O]

#### 0x00077626 `st_get_90f17_hi` — data-accessor
Returns the high nibble of 0xFFF90F17; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x00077634 `st_get_90f18` — data-accessor
Returns byte 0xFFF90F18; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x0007763E `st_get_8446c_lo_latched` — data-accessor
Redundancy-checked read of the complement nibble pair at 0xFFF8446C (same pattern as
0x00076FF0 but the second safe-value group): consistent pair or callback PTR_FUN_00077814
without bit 0x40 re-latches the low nibble into 0xFFF93C05, else last-good value is held.
Calls 0x0006E74A. Cluster of 1. [O]

#### 0x0007768A `st_get_8446c_lo_sext_inv` — data-accessor
Complement/sign-extended form of the 0xFFF8446C safe pair (cache 0xFFF84A0A), the analogue of
0x0007703C for the second group. Calls 0x0006E74A. Cluster of 1. [O]

#### 0x000776C8 `st_get_8447c_b6_invalid` — data-accessor
Returns 0xFFF8447C with bit 6 set when the 0xFFF8446C redundancy is broken AND callback
PTR_FUN_00077814 reports bit 0x40 (analogue of 0x0007707A). Calls 0x0006E74A. Cluster of 1. [O]

#### 0x00077716 `st_get_84878_b6_valid` — data-accessor
Analogue of 0x000770C8 for the second safe-value group: byte 0xFFF8487A with bit 6 in
good-value polarity. Calls 0x0006E74A. Cluster of 1. [O]

#### 0x00077764 `st_get_8446d_b0` — data-accessor
Returns bit 0 of 0xFFF8446D. Cluster of 1. [O]

#### 0x00077770 `st_get_8446d_b1` — data-accessor
Returns bit 1 of 0xFFF8446D. Cluster of 1. [O]

#### 0x0007777C `st_get_8446d_b2_3` — data-accessor
Returns the 2-bit field at bits 2–3 of 0xFFF8446D. Cluster of 1. [O]

#### 0x0007778A `st_get_8446d_b4` — data-accessor
Returns bit 4 of 0xFFF8446D. Cluster of 2; member 0x00077796 returns bit 5 of the same byte. [O]

#### 0x000777A2 `st_get_90f35_b5_inv` — data-accessor
Returns 0xFE|bit5 of 0xFFF90F35 (0xFF when set). Cluster of 1. [O]

#### 0x000777B0 `st_get_8446d_hi` — data-accessor
Returns the top 2 bits of 0xFFF8446D; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x000777BE `st_get_8446e` — data-accessor
Returns byte 0xFFF8446E. Cluster of 1. [O]

#### 0x000777C8 `st_get_90f36` — data-accessor
Returns byte 0xFFF90F36. Cluster of 1. [O]

#### 0x000777D2 `st_get_8446f_hi` — data-accessor
Returns the high nibble of 0xFFF8446F. Cluster of 1. [O]

#### 0x000777E0 `st_get_90f37_hi_sext` — data-accessor
Returns high nibble of 0xFFF90F37 OR 0xF0. Cluster of 1. [O]

#### 0x000777F0 `st_get_84470` — data-accessor
Returns byte 0xFFF84470; only 0x00144108 consumes it. Cluster of 1. [O]

#### 0x000777FA `st_get_84464` — data-accessor
Returns byte 0xFFF84464. Cluster of 1. [O]

#### 0x00077802 `st_get_84874` — data-accessor
Returns byte 0xFFF84874. Cluster of 1. [O]

#### 0x0007780A `st_get_8447c_raw` — data-accessor
Returns byte 0xFFF8447C raw (the cell 0x000776C8 decorates with the validity bit); only
0x001513F8 consumes it. Cluster of 1. [O]

#### 0x00077818 `st_snap_df4_to_845c4` — logic
When byte 0xFFF84334 (record +0x300 of the 16-stride array) is zero, copies 4 bytes from
0xFFF90DF4 to 0xFFF845C4 and sets flag 0xFFF84A0B = 1; otherwise clears the flag to 0. Called
from scheduler task 0x0006E83C. Cluster of 1. [O]

#### 0x0007784C `st_pack_90e74_7a` — logic
Signal-pool packer: calls a run of app callbacks via PTR_FUN_00077A28…0x00077A58 (targets in
the 0x00152xxx/0x00153xxx area) and merges their results bit-field by bit-field into
0xFFF90E74…0xFFF90E7A — the exact cells this unit's getters expose. Invoked from 0x0006E92C.
This pack/getter pairing proves the 0xFFF90Exx block is a scheduled signal pool. Cluster of 1. [O]

#### 0x0007791C `st_set_90e7c` — logic
Stores the byte returned by callback PTR_FUN_00077A5C into 0xFFF90E7C. Called from 0x0006E938.
Cluster of 1. [O]

#### 0x0007792C `st_pack_90e86_89` — logic
Signal-pool packer over callbacks PTR_FUN_00077A60…0x00077A80 writing bit fields and booleans
into 0xFFF90E86/0xFFF90E87/0xFFF90E89. Called from 0x0006E944. Cluster of 1. [O]

#### 0x00077978 `st_pack_90ea4_a6` — logic
Signal-pool packer over callbacks PTR_FUN_00077A84…0x00077AA0 writing shifted bit fields into
0xFFF90EA4–0xFFF90EA6. Called from 0x0006E950. Cluster of 1. [O]

#### 0x000779D0 `st_pack_90ecc_ce` — logic
Signal-pool packer over callbacks PTR_FUN_00077AA4…0x00077AC0 writing into
0xFFF90ECC–0xFFF90ECE — same shape as 0x00077978. Called from 0x0006E95C. Cluster of 1. [O]

#### 0x00077AC4 `st_pack_90edc_ee3` — logic
Signal-pool packer over callbacks PTR_FUN_00077CE4…0x00077D28 writing booleans and nibble
fields into 0xFFF90EDC/0xFFF90EDD/0xFFF90EDE/0xFFF90EE3. Called from scheduler wrapper
0x0006E968. Cluster of 1. [O]

#### 0x00077B90 `st_pack_90efc_f02` — logic
Signal-pool packer over callbacks PTR_FUN_00077D2C…0x00077D64 writing
0xFFF90EFC–0xFFF90F02. Called from 0x0006E974. Cluster of 1. [O]

#### 0x00077C54 `st_pack_90f0c_0e` — logic
Signal-pool packer over callbacks PTR_FUN_00077D68…0x00077D8C writing
0xFFF90F0C/0xFFF90F0D/0xFFF90F0E. Called from 0x0006E980. Cluster of 1. [O]

#### 0x00077D90 `st_pack_90f54_58` — logic
Signal-pool packer over callbacks PTR_FUN_00077E18…0x00077E3C writing
0xFFF90F54–0xFFF90F58 — the cells this unit's getters 0x000777xx read. Called from 0x0006E98C. Cluster of 1. [O]

#### 0x00077E40 `st_copy_8b_to_slot` — logic
Called with a non-positive index into a table at _LAB_0007800E_2 (12-byte entries: flag-byte
pointer, dest pointer, counter pointer); copies exactly 8 bytes from the caller's buffer to the
destination, sets the flag byte to 1 and increments the counter. Callers=- → reached via a
pointer table; the 8-byte size suggests a CAN-frame-shaped mailbox put [I]. Cluster of 1. [O]

#### 0x00077E70 `can_bus_latch_match` — logic
Latches the first bus value 1 or 2 into 0xFFF80D1A when it is still 0, then returns whether the
requested bus equals the latched one. This is the stock "first bus used is latched" behaviour
the project measurement implementation mirrors (spec 10 §6a; bus 1 = pins 5/6, bus 2 = pins 3/4,
spec 01 §2.8). Cluster of 1. [I]

#### 0x00077E92 `can_session_bus_step` — logic
One step of a 3-state machine on 0xFFF84A0E driven by callback PTR_FUN_00078014 (current bus):
on transition 1→2 it loads 0xFFF94809 from a table via PTR_PTR_0007801C + 0xFFF90AB8, and when
that byte clears with bus==2 it resets the latch 0xFFF80D1A. Keeps 0xFFF80D1B = last bus value.
Called by 0x00077F0A. Cluster of 1. [I]

#### 0x00077F0A `can_task_poll_dispatch` — logic
Periodic task body called from 0x0006E882: resets 0xFFF84A0D/0xFFF84A0C, steps 0x00077E92,
allocates 0x50 bytes via PTR_FUN_00078024(0x50,0,0), then scans a 4-entry table at
PTR_PTR_0007802C (0x14-byte records) round-robin and runs the first enabled handler via
PTR_FUN_00078030(id, arg); releases the buffer afterwards. A cooperative one-job-per-tick
dispatcher. Cluster of 1. [O]

#### 0x00078038 `can_grp3_cond_check` — logic
For one of the two channel groups (index param_2 into the u16 mask table PTR_DAT_00078298),
tests the 3 low bits; for each set bit calls PTR_FUN_000782A0 (param_1==0) or PTR_FUN_0007829C
and ANDs the results. Helper of the bus-pair state machine 0x000780D0. Cluster of 1. [I]

#### 0x0007807E `can_grp3_cond_check2` — logic
Variant of 0x00078038 using callbacks PTR_FUN_000782A4 (state!=2 pre-check), 0x000782A8/0x000782AC.
Helper of 0x000780D0. Cluster of 1. [I]

#### 0x000780D0 `can_grp_state_machine` — logic
State machine for the two channel groups: per group reads a status word via PTR_FUN_000782B0,
stores it to 0xFFF92FB8[i], and walks states in 0xFFF93890[i]/0xFFF93892[i] (0=off, 1=arming,
2=running, 3=timed hold) with timeout words in 0xFFF94734[i]/0xFFF94738[i] loaded from
variant-indexed tables (PTR_PTR_000782B4…0x000782C4 + 0xFFF90AB8). Calls 0x00078038/0x0007807E
for the 3-bit condition tests. Called from task 0x0006E882. Likely the two CAN buses' enable
sequencing [I]. Cluster of 1. [O]

#### 0x00078280 `st_get_92fb8_idx` — data-accessor
Returns (signed) 0xFFF92FB8[param] — the per-group status word written by 0x000780D0. Caller
0x000761CE. Cluster of 1. [O]

#### 0x0007828C `st_get_9388c_idx` — data-accessor
Returns 0xFFF9388C[param] — the per-group run flags written by 0x000780D0. Callers
0x0006EC08/0x0006EC26/0x000761CE. Cluster of 1. [O]

#### 0x000782C8 `st_reload_94764_variant` — logic
If callback PTR_FUN_000782EC returns 1, reloads the word 0xFFF94764 from a table selected by
the variant index 0xFFF90AB8 (PTR_PTR_000782F0). Called from 0x0006E804. Cluster of 1. [O]

#### 0x000782F4 `st_dualrail_refresh_a` — logic
Walks a table (count DAT_0007834C, base PTR_DAT_00078350, 0x50-byte records): sets bit 7 at
*(rec+0x10)+8, clears bit 7 of *(rec+0x14), and writes the one's-complement copy of
rec+0x48's bytes to rec+0x4C — refreshing the value/inverse dual-rail pattern used by the safe
getters at the top of this unit. Called from 0x0006E804. Cluster of 1. [O]

#### 0x000782FC `st_dualrail_refresh_b` — logic
Byte-identical role to 0x000782F4 over the second table (count DAT_00078354, base
PTR_DAT_00078358). Called from 0x0006E804. Cluster of 1. [O]

#### 0x0007835C `timer_sched_run_list_a` — logic
Cyclic scheduler for the timer list at PTR_DAT_00078494: per 0x50-byte record, when the enable
bit (state+7 bit0) is clear it increments the u16 counter at +4 and, on reaching the thresholds
at +2/+6, fires up to four callbacks (+0x18/+0x1C/+0x2C/+0x30) and marks the channel event via
0x0007844E(&DAT_FFF9099C,...); a set bit1 triggers the stop path with PTR_FUN_000784A4.
Entered from task 0x0006E882; wrapped by enter/leave via PTR_FUN_000784A0/0x000784A8.
Cross-ref spec 02 (cyclic 1 ms/timer engine). Cluster of 1. [O]

#### 0x00078368 `timer_sched_run_list_b` — logic
Second instance of the same scheduler engine over PTR_DAT_0007849C with event area
0xFFF90A3C; its callback tables reach the packer wrappers 0x0006E98C/0x0006E95C, linking the
timer lists to the signal-pool packers. Also called from 0x0006E882. Cluster of 1. [O]

#### 0x0007844E `timer_event_flag_set` — logic
Under an enter/leave pair (PTR_FUN_000784AC(0x60) — priority mask 6, PTR_FUN_000784B0), scans
the 0x14-byte event records at param_1 (count param_2) for the channel id param_3 at +2 and
sets its flag byte at +0 to 1. Used by both schedulers to mark "channel fired this cycle". Cluster of 1. [O]

#### 0x000784B4 `st_pos_decode_93b5b` — logic
Reads a position byte via callback PTR_FUN_00078504 into 0xFFF93B5B, copies to 0xFFF93B5C and
validates it against 5/0xFF with fallback through 0xFFF93B5D/0xFFF93B5E (<6); returns the
selected value. Values 0–5 plus 0xFF-invalid resemble a lever/range selector decode [I].
Called from 0x0006F0B4. Cross-ref spec 14 (park/range inputs) as a lead only. Cluster of 1. [I]

#### 0x00078508 `st_clr_9409e` — stub
Clears byte 0xFFF9409E. Caller 0x0009A7A4. Cluster of 1. [O]

#### 0x00078514 `pool_init_dual_index` — library
Initialises six pool entries: copies the next-index bytes from PTR_DAT_00078798 into BOTH
0xFFF917F7 and 0xFFF917FD (redundant dual copies) and clears the six flag bytes at 0xFFF93850.
Callers 0x0006F26C and 0x000788E4. Cluster of 1. [O]

#### 0x000785E2 `pool_get_active_block` — library
Returns &DAT_fff91698 + idx*8 only when the pool state byte 0xFFF917D0[idx] equals 3 (active);
else NULL. Callers=- (pointer-table reach). Cluster of 1. [O]

#### 0x00078606 `pool_release_advance` — library
If the current pool block's state (0xFFF917D0[0xFFF917F7[param]]) is 3, clears it and advances
the pool's current index to PTR_LAB_000787B0[param] when it equals the limit, else +1. The
release/rotate side of the block pool. Callers=- (pointer-table reach). Cluster of 1. [O]

#### 0x0007863C `pool_alloc_block` — library
Allocates from the pool: when the state byte is 0 or 1 it marks it 1 and returns the 8-byte
block at 0xFFF91698 + idx*8; returns NULL otherwise. Callers=- (pointer-table reach). Cluster of 1. [O]

#### 0x000786D2 `pool_queue_next` — logic
Marks the pool's current block state 2 (queued) in 0xFFF917D0 and advances the current index
(with wrap via PTR_LAB_000787B0); if the dual-copy index block is also queued, allocates 0x50
bytes and — guarded by the 0xFFF93850 flag — calls PTR_FUN_000787B8 once, then runs the
dispatcher 0x0007873C. Callers=- (pointer-table reach). Cluster of 1. [O]

#### 0x0007873C `can_tx_flush_pools` — logic
For pools 0–2: if the redundant index's block is queued, transmits the 8-byte block via
PTR_FUN_000787C0(id=0xFFF844D4[pool], 0xFFFFFFFF, 8, block) and frees it. For pools 3–5 (when
0xFFF845C8 allows) it first selects the highest-priority pending pool using priority bytes from
PTR_PTR_000787BC's table, sends via PTR_FUN_00078914 and requeues on failure. The 8-byte
payload + bus channel adapters below mark this as the CAN transmit queue flush [I] (spec 08). Cluster of 1. [O]

#### 0x000788E4 `can_tx_reset_pools` — logic
Re-initialises the pool machinery: calls 0x00078514, sets 0xFFF845C8 = 1 and clears the 39
state bytes at 0xFFF917D0. Called from 0x000792AC. Cluster of 1. [O]

#### 0x00078918 `stub_write_fe20_ret3` — stub
Six-instruction fragment reached with R7 pointing at a buffer: writes 0xFE, 0x20 and returns
3 (looks like "emit two reply bytes, length 3"). Callers=-; likely a table-entry tail. Cluster
of 1. [O]

#### 0x00078E14 `meas_did_ctrl_reply` — logic
Handler for a control request (id at param_3+2): if the id is within the descriptor's limit
(param_1+0x28) and its status byte (array at param_1+0x24) lacks bit 0x40, it ORs the request's
flag byte, stores a value byte and an extra byte and replies {0xFF} (3 bytes total); otherwise
replies {0xFE, 0x11} (id out of range → 0x22). Reads like the measurement-protocol
enable/disable-value reply shape of spec 10 [I]. Callers=-. Cluster of 1. [O]

#### 0x00079054 `timer_deadline_extend` — logic
Under enter/leave callbacks, keeps the deadline word 0xFFF925D8 = max(0xFFFFF220 +
0xFFF925D4, 0xFFF925D8) and returns it. 0xFFFFF220 is the free-running hardware timer
(absolute time source, spec 02). Used by the frame builder 0x000792AC as its send deadline. Cluster of 1. [O]

#### 0x000792AC `can_tx_build_frames` — logic
For a message id gated by the 0x40 enable bit: for up to 3 frames, acquires an 8-byte pool
block via the descriptor's callback (param_2+4), writes header byte (base+seq), appends the
rolling-counter byte on the first frame (via PTR_FUN_000793EC) when the descriptor says so,
then packs param_2+0x2A signal bytes through per-signal function pointers with lengths from
(param_2+0x1C), releasing each block via (param_2+0xC). On pool starvation it counts
0xFFF93C07 and — after 31 misses inside 5000 timer units — escalates via PTR_FUN_000793E8.
Also calls 0x000788E4 and 0x00079054. The counter+header+signal-getter layout matches the
periodic CAN/measurement frame builders of spec 08/10 [I]. Callers=0x000793F4. Cluster of 1. [O]

#### 0x000793F4 `can_tx_build_msg_modules` — logic
Walks the module table (PTR_LAB_00079448, count PTR_DAT_0007944C, 0x34-byte records); for each
module walks its message array (at +0x24, count at +0x28) and for the entry whose id byte
matches param_1 invokes the module's build callback (+0x14) with a per-module 0x18-byte state
block at 0xFFF94CEC+. Stops before a module whose first word is 2. Callers=- (timer-table
reach). Cluster of 1. [O]

#### 0x00079450 `can_tx_disable_all` — logic
Same table walk, but resets every module state (0xFFFF at state+0xE) and clears each message
record's id/enable byte — disables all periodic transmission. Caller 0x0006F26C. Cluster of 1. [O]

#### 0x000794C8 `st_set_d1e_request` — data-accessor
Stores param into 0xFFF80D1E and sets flag 0xFFF80D1F = 1. Called from 75 sites all over the
application — a global "post a request/value" primitive consumed by 0x00079708. Cluster of 1. [O]

#### 0x000794FC `st_handshake_5a_a5` — logic
Maintains the 0x5A/0xA5 alternating byte at 0xFFF80D02 from callback PTR_FUN_000796F4 and, in
state 0xFFF84A0F==1, resolves it into the boolean 0xFFF940A6 (0xA5 → 1, 0x5A → 0, anything
else keeps state). A ping/pong handshake keep-alive. Callers=-. Cluster of 1. [O]

#### 0x00079570 `st_classify_report_0x23` — dispatch
Big bit-selector: masks a status word (param_1) by one active bit of param_2 (bits 0x1…0x800
each mapping to a small code 0/1/9/0xC/0xD/0xE/0xF) and reports the outcome via
PTR_FUN_00079704(0x23, code, enabled, result), then works 0xFFF940A5. Gates on session facts
(0xFFF94065==3 && 0xFFF80D06==0x1D). Callers=-. Likely event/fault-class reporting [I]. Cluster of 1. [O]

#### 0x00079708 `st_req_consume_d1e` — logic
When 0xFFF84A0A==1: if a request is pending (0xFFF80D1F==1) it takes 0xFFF80D1E (values >99
wrap by +0x9C) into 0xFFF940A5, else 0xFF; clears the request and returns
0xFFF940A5 != 0xFF. The consume side of the 0x000794C8 setter. Callers=-. Cluster of 1. [O]

#### 0x00079764 `st_chk_b200_idle` — logic
Returns 1 only when the status callback PTR_FUN_00079858 reports bit 0x200 AND both
0xFFF84A11 and 0xFFF84A12 are clear. 16 callers across the comms/diag code. Cluster of 1. [O]

#### 0x00079792 `st_call_chan_table` — logic
For an index <16 whose entry in the pointer-of-shorts table PTR_PTR_0007985C is non-null,
calls PTR_FUN_00079860(1, 1, *entry) and returns 1; else returns 0. Callers 0x00139E98/
0x0013A138. Cluster of 1. [O]

#### 0x000797BC `st_chk_b20_40_100` — logic
Returns 1 when the shared status callback PTR_FUN_00079858 reports any of bits 0x20, 0x40 or
0x100. Callers 0x0013A102/0x0014FC5C/0x00074B0C. Cluster of 1. [O]

#### 0x000797E0 `st_set_chan_b7` — logic
For index <16 with a non-null entry in pointer table PTR_PTR_00079864, sets or clears bit 7 of
the target byte per param_2. Caller 0x00139EC4. Cluster of 1. [O]

#### 0x000797FE `boot_thunk_70518` — stub
Three-instruction tail-jump through the pointer at 0x00079868 to 0x00070518. Callers
0x0013A102/0x00139B58/0x00074B0C. Cluster of 1. [O]

#### 0x0007986C `chan0_call_a0ea` — stub
Channel-0 adapter: forwards to 0x0007A0EA with fixed channel 0. Callers=- (pointer-table reach). Cluster of 1. [O]

#### 0x0007987A `chan0_call_a130` — stub
Channel-0 adapter forwarding to 0x0007A130(0). Cluster of 1. [O]

#### 0x00079886 `chan0_call_a26e` — stub
Channel-0 adapter forwarding six caller args to 0x0007A26E with channel 0, the channel-0 word at
PTR_DAT_00079B84, state block 0xFFF845F0 and the table size
(PTR_DAT_00079B7C − PTR_DAT_00079B80). Cluster of 1. [O]

#### 0x000798CA `chan0_call_a3dc` — stub
Channel-0 adapter forwarding (id, value) to 0x0007A3DC with state block 0xFFF845F0 and the same
table size. Cluster of 1. [O]

#### 0x000798EA `chan1_call_a0ea` — stub
Channel-1 twin of 0x0007986C (channel 1 into 0x0007A0EA). The 0/1 adapter pair is the first of seven such channel adapter sets (see 0x79948…0x79C08). Cluster of 1. [O]

#### 0x000798F8 `chan1_call_a130` — stub
Channel-1 twin of 0x0007987A. Cluster of 1. [O]

#### 0x00079904 `chan1_call_a26e` — stub
Channel-1 twin of 0x00079886 using word PTR_DAT_00079B90, state block 0xFFF845F4 and size from
PTR_DAT_00079B88/0x00079B8C. Cluster of 1. [O]

#### 0x00079948 `chan1_call_a3dc` — stub
Channel-1 adapter forwarding (id, value) to 0x0007A3DC with state block 0xFFF845F4 and the
channel-1 table size (PTR_DAT_00079B88 − 0x00079B8C). Cluster of 1. [O]

#### 0x00079968 `chan2_call_a0ea` — stub
Channel-2 adapter into 0x0007A0EA. Cluster of 1. [O]

#### 0x00079976 `chan2_call_a130` — stub
Channel-2 adapter into 0x0007A130. Cluster of 1. [O]

#### 0x00079982 `chan2_call_a26e` — stub
Channel-2 adapter into 0x0007A26E with word PTR_DAT_00079B9C, state block 0xFFF845F8 and size
from PTR_DAT_00079B94/0x00079B98. Cluster of 1. [O]

#### 0x000799C6 `chan2_call_a3dc` — stub
Channel-2 adapter into 0x0007A3DC (state block 0xFFF845F8). Cluster of 1. [O]

#### 0x000799E6 `chan3_call_a0ea` — stub
Channel-3 adapter into 0x0007A0EA. Cluster of 1. [O]

#### 0x000799F4 `chan3_call_a130` — stub
Channel-3 adapter into 0x0007A130. Cluster of 1. [O]

#### 0x00079A00 `chan3_call_a26e` — stub
Channel-3 adapter into 0x0007A26E with word PTR_DAT_00079BA8, state block 0xFFF845FC and size
from PTR_DAT_00079BA0/0x00079BA4. Cluster of 1. [O]

#### 0x00079A44 `chan3_call_a3dc` — stub
Channel-3 adapter into 0x0007A3DC (state block 0xFFF845FC). Cluster of 1. [O]

#### 0x00079A64 `chan4_call_a0ea` — stub
Channel-4 adapter into 0x0007A0EA. Cluster of 1. [O]

#### 0x00079A72 `chan4_call_a130` — stub
Channel-4 adapter into 0x0007A130. Cluster of 1. [O]

#### 0x00079A7E `chan4_call_a26e` — stub
Channel-4 adapter into 0x0007A26E with word PTR_DAT_00079BB4, state block 0xFFF84600 and size
from PTR_DAT_00079BAC/0x00079BB0. Cluster of 1. [O]

#### 0x00079AC2 `chan4_call_a3dc` — stub
Channel-4 adapter into 0x0007A3DC (state block 0xFFF84600). Cluster of 1. [O]

#### 0x00079AE2 `chan5_call_a0ea` — stub
Channel-5 adapter into 0x0007A0EA. Cluster of 1. [O]

#### 0x00079AF0 `chan5_call_a130` — stub
Channel-5 adapter into 0x0007A130. Cluster of 1. [O]

#### 0x00079AFC `chan5_call_a26e` — stub
Channel-5 adapter into 0x0007A26E with word PTR_DAT_00079BC0, state block 0xFFF84604 and size
from PTR_DAT_00079BB8/0x00079BBC. Cluster of 1. [O]

#### 0x00079B40 `chan5_call_a3dc` — stub
Channel-5 adapter into 0x0007A3DC (state block 0xFFF84604). Cluster of 1. [O]

#### 0x00079B60 `chan6_call_a0ea` — stub
Channel-6 adapter into 0x0007A0EA. Cluster of 1. [O]

#### 0x00079B6E `chan6_call_a130` — stub
Channel-6 adapter into 0x0007A130. Cluster of 1. [O]

#### 0x00079BC4 `chan6_call_a26e` — stub
Channel-6 adapter into 0x0007A26E with word PTR_DAT_00079F04, state block 0xFFF84608 and size
from PTR_DAT_00079EFC/0x00079F00 — the state blocks march 0xFFF845F0+4*channel. Cluster of 1. [O]

#### 0x00079C08 `chan6_call_a3dc` — stub
Channel-6 adapter into 0x0007A3DC (state block 0xFFF84608). Closes a seven-channel adapter
family (channels 0–6) over the four common primitives 0x0007A0EA/0x0007A130/0x0007A26E/
0x0007A3DC, all reached via pointer tables. Cluster of 1. [O]

### Adapter-family note
The `chanN_call_*` stubs (0x0007986C–0x00079C08, 27 functions across 27 clusters) are a
uniform per-channel veneer over four shared primitives in the 0x0007A0xx–0x0007A3xx area
(outside this unit). Seven channels exist; per-channel state blocks live at
0xFFF845F0 + 4*ch. Which physical buses these map to (2 CAN + others) is unresolved —
cross-check with the unit owning 0x0007A0EA…0x0007A3DC. [I]

## Unit summary

**Slice:** r04_boot 0x00076FBC–0x00079C08 — 220 clusters, 224 functions (4 two-member
clusters: 0x0007713C/48, 0x0007724E/5A, 0x000772AA/B6, 0x0007778A/96).

**What this slice is:** the TCU's scheduled *signal-pool* layer plus the communication-channel
plumbing that surrounds it. Three cooperating parts:

1. **Signal pool + getters (0x76FBC–0x777FF, ~150 functions).** Two packer families write
   bitfields into the RAM pool 0xFFF90Dxx–0xFFF90Fxx from app callbacks; ~150 tiny getters
   expose single bits/fields of that pool (and of the 0xFFF840xx record array at 16-byte
   stride) to the big app snapshot builders 0x00144108 / 0x0014FC5C / 0x00133C4E /
   0x001513F8 / 0x00133476+0x001517E0. Two complement-nibble "safe value" channels exist
   (0xFFF84304 and 0xFFF8446C) with last-good-value latching gated by callbacks
   PTR_FUN_000771D4/0x00077814.
2. **Schedulers and bus machinery (0x77818–0x784B4).** Two cyclic timer-list engines
   (0x7835C/0x78368, spec 02 territory), the signal-pool packers they trigger, a two-group
   channel state machine (0x780D0) with the bus 1/2 latch (0x77E70, spec 10 §6a), dual-rail
   refresh passes (0x782F4/0x782FC), and an 8-byte block pool with priority flush
   (0x78514/0x785E2/0x78606/0x7863C/0x786D2/0x7873C).
3. **Periodic frame building + channel adapters (0x78918–0x79C08).** A DID-gated periodic
   frame builder with rolling counter and per-signal getter callbacks (0x792AC/0x793F4/
   0x79450, 0x78E14), timer-deadline helper reading HW timer 0xFFFFF220 (0x79054), and a
   7-channel × 4-primitive adapter family (0x7986C–0x79C08) over shared code at
   0x0007A0EA/0x0007A130/0x0007A26E/0x0007A3DC.

**Counts by kind** (224 functions): data-accessor 150, logic 38, stub 31, library 4,
dispatch 1. Entries: 0 unknown.

No function needed kind `unknown` — every getter/stub's behavior is fully observable — but the
*semantic meaning* of most signal-pool bits (which physical quantity each 0xFFF90Dxx field
represents) remains open; the getter names are deliberately address/field-based, not
interpretive.

**Cross-region leads:**
- The packer callbacks point into **0x00152xxx–0x00153xxx** (r14/r15_app) — those app
  functions are the true source of every signal-pool bit; units covering them should
  cross-check against this pool map.
- The snapshot builders **0x00144108** (5322 instrs), **0x0014FC5C** (1811), 0x001513F8 and
  0x00133476/0x001517E0 consume nearly every getter here — naming them would label hundreds
  of pool bits at once (measurement/DID table candidates, spec 10 §5).
- The frame builder's module/message tables (PTR_LAB_00079448) and the shared channel
  primitives **0x0007A0EA–0x0007A3DC** are outside this unit; whoever owns them holds the
  channel-0..6 identities (2 CAN buses + 5 more, likely SPI/shifter/diag virtual channels).
- 0xFFF90AB8 is a global variant/table selector used by the state machine and pool reloads —
  worth naming in whichever unit writes it.
