# Unit 18 (r08_app)

Clusters from `units/unit_18_r08_app.csv` in unit order. Region is the 0x000A79E0-0x000ADEA0
band of r08_app: a large application-support layer of small accessors, slot/value wrappers and
vtable-style dispatchers on objects in RAM (DAT_fff81c38 area) consumed by the r0c_app state
machines (0x000C2xxx-0x000C8xxx) and r14_app engine (0x0015xxxx).

#### 0x000A79E0 `get_fff8e534` — data-accessor
Returns global byte DAT_fff8e534. Called from r0c_app 0x000C5484, 0x000C514A and r08_app
0x000A8496. Singleton cluster. [O]

#### 0x000A79EA `get_fff8e537` — data-accessor
Returns global byte DAT_fff8e537. Same caller trio as the fff8e53x neighbors
(0x000C4384/0x000C48DE/0x000C4190), a status-poll set in the r0c dispatcher. Singleton. [O]

#### 0x000A79F4 `get_fff8e536` — data-accessor
Returns global byte DAT_fff8e536; called by 0x000C4384/0x000C48DE/0x000C4190. Singleton. [O]

#### 0x000A79FE `get_fff8e53c` — data-accessor
Returns global byte DAT_fff8e53c; called by 0x000C7110 and 0x000C71BE. Singleton. [O]

#### 0x000A7A08 `get_fff8e538` — data-accessor
Returns global byte DAT_fff8e538; caller trio 0x000C4384/0x000C48DE/0x000C4190. Singleton. [O]

#### 0x000A7A12 `get_fff8e539` — data-accessor
Returns global byte DAT_fff8e539; caller trio 0x000C4384/0x000C48DE/0x000C4190. Singleton. [O]

#### 0x000A7A1C `tbl_lookup_fff8e530` — data-accessor
Table read: `*(*(PTR_DAT_000a7c28 + DAT_fff8e530*4 + 0x20))` — indexes a pointer table at
0x000a7c28 by global byte DAT_fff8e530 and double-dereferences. No resolved callers
(via pointer table). Singleton. [I]

#### 0x000A7A32 `get_fff89438` — data-accessor
Returns global byte DAT_fff89438; called by 0x000A5A82. Singleton. [O]

#### 0x000A7A3C `any_flag_pred_7slots` — logic
Sequentially applies predicate pointer PTR_FUN_000a7c2c to seven flag records
(DAT_fff8948c/74/80/68/34/a4/98), returning the first non-zero result — an "any condition
of the set is active" check. Called by 0x000A64C4; also tail-calls 0x000C81D0. Singleton. [I]

#### 0x000A7A94 `pred_flag_fff89434` — logic
Single application of predicate pointer PTR_FUN_000a7c30 to record DAT_fff89434; tail-jumps
into 0x000C8564. Callers 0x000A5A82, 0x000A64C4. Singleton. [I]

#### 0x000A7AA4 `ptr_fff81c54` — data-accessor
Returns &DAT_fff81c54, one of three adjacent slot addresses (fff81c54/58/5c) handed to
0x000A6ECC. Singleton. [O]

#### 0x000A7AAA `ptr_fff81c58` — data-accessor
Returns &DAT_fff81c58; caller 0x000A6ECC. Singleton. [O]

#### 0x000A7AB0 `ptr_fff81c5c` — data-accessor
Returns &DAT_fff81c5c; caller 0x000A6ECC. Singleton. [O]

#### 0x000A7AB6 `vo_set_slot_fff81c5c` — data-accessor
Virtual call PTR_LAB_000a7c34(&DAT_fff81c38, &DAT_fff81c5c, 0xfff81c68): binds slot
fff81c5c of the object at fff81c38 to link cell 0xfff81c68. Caller 0x000A6E38. Singleton. [I]

#### 0x000A7AC8 `vo_set_slot_fff81c54` — data-accessor
Same pattern as 0x000A7AB6 for slot &DAT_fff81c54 with link cell 0xfff81c60. Singleton. [I]

#### 0x000A7ADA `vo_set_slot_fff81c58` — data-accessor
Same pattern for slot &DAT_fff81c58 with link cell 0xfff81c64. Singleton. [I]

#### 0x000A7AEC `set_fff81c6c_and_notify` — data-accessor
If DAT_fff8d632==0, stores param plus its complement into DAT_fff81c6c/fff81c6d, then marks
update-pending by setting DAT_fff83044=1, DAT_fff83045=0xfe (via FUN_000a7660). Caller
0x000A6B06. Singleton. [O]

#### 0x000A7B08 `get_fff81c6c` — data-accessor
Returns DAT_fff81c6c (the value written by 0x000A7AEC). Callers 0x000A6B06, 0x000C6116. [O]

#### 0x000A7B12 `get_slot_val_fff81c48` — data-accessor
Dereferences the identity helper FUN_000a766a(&DAT_fff81c48) — i.e. returns the byte at slot
fff81c48 through the module's accessor discipline. Caller 0x000C5C7C. Singleton. [I]

#### 0x000A7B24 `vo_call_slot_fff81c48` — data-accessor
Tail virtual call PTR_LAB_000a7c38(&DAT_fff81c48) on the fff81c38 object's slot. Caller
0x000C5C7C. Singleton. [I]

#### 0x000A7B2E `clear_slot_fff81c48` — logic
FUN_000a766c(&DAT_fff81c48): when DAT_fff8d632==0 zeroes slot (0/0xff pattern) and raises
update flag DAT_fff83044. Caller 0x000C5C7C. Singleton. [O]

#### 0x000A7B36 `vo_dispatch_8arg_fff81c38` — data-accessor
Pure tail-jump: invokes method pointer PTR_FUN_000a7c3c on object &DAT_fff81c38 forwarding
8 arguments (resolved target 0x000A7430, an index-clamped char-pair store). Heavily used
(32 callers across r0c_app and r14_app 0x0015xxxx). Singleton. [I]

#### 0x000A7B6E `get_fff8d710` — data-accessor
Returns global byte DAT_fff8d710; caller 0x000C4C2C. Singleton. [O]

#### 0x000A7B78 `get_fff8d6d6` — data-accessor
Returns global byte DAT_fff8d6d6 — the most widely used status byte in this band (72 callers
across r08/r0c/r14/r18). Singleton. [O]

#### 0x000A7B82 `get_fff8d6d7` — data-accessor
Returns DAT_fff8d6d7; callers 0x0009B594, 0x001556D4, 0x00155890. Singleton. [O]

#### 0x000A7B8C `get_fff8d6da` — data-accessor
Returns DAT_fff8d6da; callers 0x000A6D6E, 0x0009B5A0. Singleton. [O]

#### 0x000A7B96 `get_fff8d6d9` — data-accessor
Returns DAT_fff8d6d9; caller 0x000A902E. Singleton. [O]

#### 0x000A7BA0 `get_fff8d6df` — data-accessor
Returns DAT_fff8d6df; caller 0x000A902E. Singleton. [O]

#### 0x000A7BAA `get_fff8942c` — data-accessor
Returns DAT_fff8942c; caller 0x000C805A. Singleton. [O]

#### 0x000A7BB4 `read_valid_fff8304c` — data-accessor
Virtual read of a field from the value+complement record at DAT_fff8304c via validator
PTR_FUN_000a7c40 (resolved 0x000A5224, which checks word at +0x20 against ~+0x24 and returns 0
on mismatch). Cluster of 5: members 0x000A7BD2/0x000A7BF0/0x000A7C0A/0x000A7C50 validate
different fields of the same record through validators 0x000A5204/0x000A51E4/0x000A51F4/
0x000A5214. Callers 0x00158B9C/0x00158B4E (r14_app) and r0c 0x000C695E/0x000C6BA8. [O]

#### 0x000A7BC4 `set_fff8306c_pair` — data-accessor
Stores param&0xff into DAT_fff8306c and its complement into DAT_fff83070. Caller 0x00158B9C.
Singleton. [O]

#### 0x000A7BE2 `set_fff8305c_pair` — data-accessor
Stores param&0xff into DAT_fff8305c and complement into DAT_fff83060. Caller 0x00158B9C. [O]

#### 0x000A7C00 `set_fff8304c_override` — logic
Wrapper of FUN_000a768c(&DAT_fff8304c, param): writes value+complement, but when
DAT_fff8d61b==1 substitutes a byte read via *DAT_000a79b8+0x18 (unless 0xff). Caller
0x00158C30. Singleton. [O]

#### 0x000A7C1A `set_fff83054_pair` — data-accessor
Stores param&0xff into DAT_fff83054 and complement into DAT_fff83058. Caller 0x00158B9C. [O]

#### 0x000A7C60 `set_fff83064_pair` — data-accessor
Stores param&0xff into DAT_fff83064 and complement into DAT_fff83068. Caller 0x00158B9C. [O]

#### 0x000A7C6E `get_fff8e54a` — data-accessor
Returns DAT_fff8e54a; caller 0x000C3832. Singleton. [O]

#### 0x000A7C78 `get_fff8e545` — data-accessor
Returns DAT_fff8e545; callers 0x000A96DE, 0x00157478. Singleton. [O]

#### 0x000A7C82 `get_fff8e543` — data-accessor
Returns DAT_fff8e543; no direct callers (pointer-table use). Singleton. [O]

#### 0x000A7C8C `get_fff8e544` — data-accessor
Returns DAT_fff8e544; no direct callers (pointer-table use). Singleton. [O]

#### 0x000A7C9C `snap_pack_124_getters` — dispatch
442-instruction snapshot builder: invokes ~124 getter routines indirectly through consecutive
PTR_PTR tables at 0x000a7f20-0x000a82c0 and packs byte/word results into a 0x82-byte struct at
param_1 (offsets 0x00-0x81, words at +0x0a/0x0e/.../+0x78). Called from one-shot init
dispatcher 0x0009B228 and periodic 0x0009B306. Singleton. [O]

#### 0x000A81D8 `snap_pack_tail_fields` — dispatch
Same snapshot idiom for the tail: 8 indirect getters (PTR_PTR_000a82c4-0x000a82dc) fill struct
offsets 0x7c-0x81 plus a word at +0x0a. Caller 0x0009B39E (cyclic dispatcher in r08_app).
Singleton. [O]

#### 0x000A82E0 `periodic_chain_tick_a` — logic
One scheduler tick: FUN_000a8382 countdown, three virtual calls on PTR_DAT_000a8430,
FUN_000a8336/FUN_000a836a flag updates, then three more virtual calls (PTR_FUN_000a8440/48/50).
Caller 0x000A58B8 (4-instr periodic stub). Also reaches r0c 0x000C3EA0/0x000C4BB6/0x000C4AB2/
0x000C3F66. Singleton. [O]

#### 0x000A8310 `state_flags_init_123` — data-accessor
Writes param[2]=1, param[4]=0, param[3]=1 then virtual call PTR_FUN_000a8454(PTR_DAT_000a843c).
Caller 0x000A58C0 (same 4-instr periodic stub shape as 0x000A58B8). Singleton. [O]

#### 0x000A8326 `snap_seed_word0` — data-accessor
Reads `**(undefined1**)*DAT_000a8458` into param[0] and clears param[1] — seeds the first field
of the snapshot record. Caller 0x0009B228. Singleton. [O]

#### 0x000A8336 `flag4_when_mode8` — logic
Sets param[4]=1 only when (PTR_FUN_000a845c()==8 && PTR_FUN_000a8460()==0 && *PTR_DAT_000a843c
!= 8 && *PTR_DAT_000a8464==0), else 0. Called by 0x000A82E0. Singleton. [I]

#### 0x000A836A `clear_flag3_mode23` — logic
If PTR_DAT_000a8468[0x16] is 2 or 3, clears param[3]. Caller 0x000A82E0. Singleton. [O]

#### 0x000A8382 `countdown_decrement_pair` — logic
When PTR_DAT_000a8468[0x27] in 1..3: decrements *param twice (flags into param[1] via the
double-decrement pattern), and a second guarded branch clears param[2]/*param. Caller
0x000A82E0. Singleton. [I]

#### 0x000A8474 `periodic_chain_tick_b` — logic
Orchestrates seven state sub-updates on the shared record: FUN_000a8cd6, 0x000a86ac,
0x000a8496, 0x000a8636, 0x000a8d28, 0x000a8e04, 0x000a8e40. Caller 0x0009B34A. Singleton. [O]

#### 0x000A8496 `state_update_timers_min` — logic
186 instrs: decrements the byte-timer pair at param+0x1c/0x1e (expiry flags at +0x1d/+0x1f),
takes min() of two getter pairs (PTR_FUN_000a8688/8c, 0x000a8690/94) into locals, and on a
cleared-condition path stores a selected value into param+0x26. Uses getters 0x000A79E0 etc.
Caller 0x000A8474. Singleton. [I]

#### 0x000A8636 `timer_word18_update` — logic
Reloads the word timer at param+0x18 from a table (**PTR_DAT_000a86a8) when both predicates
clear, otherwise decrements it and stores the expiry flag at param+0x1a. Caller 0x000A8474. [O]

#### 0x000A86AC `state20_track_with_masks` — logic
Tracks state byte param+0x20 against global *PTR_DAT_000a87b8: on transition 2<->3 keeps
param+0x2b, on value 4 resets and issues the 8-arg dispatch (0x000A7B36 target) with
(3,1,...)/(3,2,...); applies AND-masks from the table at *PTR_DAT_000a87c4 (+4/+8/+0xc)
guarded by flags at +0x49/+0x4d/+0x4e, and finally latches param+0x20 into the global.
Callers 0x000A8474; calls 0x000A87CC and 0x000A7B36. Singleton. [I]

#### 0x000A87CC `state_map_reason_chain` — dispatch
572-instr priority chain: ~24 predicates (PTR_FUN_000a8aac..0x000a8d1c) tested in order; the
first that fires maps param+0x20 through a per-condition translation table hung off
PTR_DAT_000a8ab0/0x000a8cf8 (+0x10..+0x6c) and records a distinct reason code (0x01,0x02,0x09,
0x16-0x18,0x20,0x21,0x23,0x28-0x2f,0x32-0x35,0x3b-0x3d) at param+0x2c, returning 1. Calls two
dozen r0c_app condition evaluators (0x000C4FB8-0x000C54FE). Caller 0x000A86AC. Singleton. [I]

#### 0x000A8CD6 `word22_masked_from_table` — logic
Computes `*(PTR_DAT_000a8d24+0x70) & table[ (*PTR_DAT_000a8d24)[0x42] ] & 0xfffc` (table hung
off *PTR_DAT_000a8d20+0x10, word-indexed) into param+0x22 — a quantized copy of a source word.
Caller 0x000A8474. Singleton. [I]

#### 0x000A8D28 `word24_build_masked` — logic
Builds param+0x24 = (param+0x20 & 3) | param+0x22, then clears bit 0x200 under flag conditions
(PTR_DAT_000a8ec0[0x42]/[0x56]), clears bit 8 of +0x25 when flag +0x6e is set, ANDs against
masks from *PTR_DAT_000a8ec4 (+4/+8/+0xc gated by +0x49/+0x4d/+0x4e) and finally ORs 0x2000
when PTR_FUN_000a8ecc() is true. Caller 0x000A8474. Singleton. [I]

#### 0x000A8E04 `flag2d_two_preds` — logic
Sets param+0x2d=1 when *PTR_DAT_000a8ed0==1 && PTR_FUN_000a8ed4()==1 && a linked byte==1;
clears it when *PTR_DAT_000a8ed0==0. Caller 0x000A8474. Singleton. [O]

#### 0x000A8E40 `latch_flags_28_26_27` — logic
Edge-latch pair: param+0x29/+0x2a follow +0x26/+0x27 only while table cells at
(*PTR_DAT_000a8ec4)+0x20/+0x1c (indexed by +0x20 and +0x28) are non-zero and the previous
state differs; stores back into PTR_DAT_000a8ed8[0]/[1] and copies +0x20 into +0x28. Caller
0x000A8474. Singleton. [I]

#### 0x000A8EDC `state14_15_update3` — logic
Copies PTR_FUN_000a90d8() into param+0x14, runs FUN_000a908e/0x000a9126/0x000a9108 and finally
sets param+0x15=1. Caller 0x000A5968. Singleton. [O]

#### 0x000A8EFE `state_update_tick_c` — logic
Big orchestrator: twelve sub-updates on the shared record — FUN_000a9144, 0x000a9086, 0x000a908e,
0x000a90a0, 0x000a9126, 0x000a8f38, 0x000a9526, 0x000a9668, 0x000a9190, 0x000a96de, 0x000a902e,
0x000a9748. Caller 0x000A5944 (scheduler stub). Singleton. [O]

#### 0x000A8F34 `timer8_reload_mode8` — logic
When PTR_FUN_000a975c()==8: double-decrement of the word timer at param+8 with expiry flag at
param+10; otherwise reload the timer from (*PTR_DAT_000a976c)+0x74. Ghidra labels this a thunk
of FUN_000a96fa (byte-identical copy pattern). Caller 0x000A5960. Singleton. [O]

#### 0x000A8F38 `latch_state14_mode5678` — logic
Copies PTR_FUN_000a90d8() into param+0x14 when it is one of 5/6/7/8. Caller 0x000A8EFE. [O]

#### 0x000A8F5C `edge_flags_cd_ef` — logic
90 instrs: computes a 0/1/2 band at param+0xd from PTR_DAT_000a90dc[0x27]/[0x28] and a linked
byte, a mode flag (1-3) at param+0xc, and rising/falling edge flags at param+0xe/+0xf against
the latched DAT_fff8e572 (updated at the end). First call latches DAT_fff8e572 from param+0xc
(DAT_fff8975c one-shot guard). Caller 0x0009B306. Singleton. [I]

#### 0x000A902E `flag23_mode8_entry` — logic
Sets param+0x23=1 when PTR_FUN_000a90e8()==8 && PTR_FUN_000a90d8()!=8 && two more predicates &&
*PTR_DAT_000a90f4==1; clears when both return 8. Uses getters 0x000A7B78/0x000A7B96/0x000A7BA0.
Caller 0x000A8EFE. Singleton. [I]

#### 0x000A9086 `copy_word10_from_ptr` — data-accessor
Copies the word at *PTR_DAT_000a90f8 into param+0x10 (the value the threshold band code reads).
Caller 0x000A8EFE. Singleton. [O]

#### 0x000A908E `flag13_from_ptr` — data-accessor
param+0x13 = (*PTR_DAT_000a90fc != 0). Callers 0x000A8EDC, 0x000A8EFE. Singleton. [O]

#### 0x000A90A0 `flag21_threshold_cmp` — logic
Sets param+0x21=1 when (PTR_DAT_000a90dc[0x31]==1 && [0x17]==0) or (**(*PTR_DAT_000a90e0+0x10) <
word at PTR_DAT_000a90dc+0x32 while [0x34]==0 && [0x17]!=0). Caller 0x000A8EFE. Singleton. [I]

#### 0x000A9108 `timer_pair_reload` — data-accessor
Reloads the word timers at param[0] and param[2] from (*PTR_DAT_000a9248)+0x18/+0x14 and clears
their expiry flags. Caller 0x000A8EDC. Singleton. [O]

#### 0x000A9126 `flag12_from_flags` — logic
param+0x12 = (PTR_DAT_000a924c[0x2e]==1 && [0x38]==0). Callers 0x000A8EDC, 0x000A8EFE. [O]

#### 0x000A9144 `flag15_set_clear_2` — logic
Sets param+0x15=1 when PTR_FUN_000a9258()==2; clears it when the mode leaves 2 and the values
from PTR_FUN_000a9250()/0x000a9254() are equal. Caller 0x000A8EFE. Singleton. [O]

#### 0x000A9190 `band_eval_priority_encoder` — logic
366 instrs: mode-2-gated timers at param[0]/param[2] (reload from table cells +6/+0x14), four
threshold comparisons against table words (PTR_DAT_000a9248 offsets +0..+3, +0x16-+0x1c) writing
band flags at +0x19/+0xd/+0x1b/+0xe, then a nested priority encoder that updates the two
interlocks at +0xb/+0x17 from those flags. Tail-calls FUN_000a9504 into +0xc. Caller 0x000A8EFE.
Singleton. [I]

#### 0x000A9504 `map_state16_to_bits` — data-accessor
Maps param+0x16: 1->1, 2->2, 3->4, else 0 — an enum-to-bitmask translation. Caller 0x000A9190.
Singleton. [O]

#### 0x000A9526 `band_thresholds_tick` — logic
Groups the three threshold-band evaluators: FUN_000a9538, 0x000a9594, 0x000a9616. Cluster of 2
— member 0x001260AE is the byte-identical copy in r10_app. Callers 0x000A8EFE (r08) and the
r10 caller of the twin. [O]

#### 0x000A9538 `band1d_two_thresholds` — logic
Threshold classify of param+0x10 into 0/1/2 stored at param+0x1d; threshold pair chosen by
PTR_DAT_000a9658[0x42] (0: table+0x38/+0x3c, 1: +0x30/+0x34, else 2). Caller 0x000A9526. [I]

#### 0x000A9594 `band1e_table_pair` — logic
Selects a short threshold pair from (*PTR_DAT_000a965c)+0x48/+0x50 (indexed by
*PTR_DAT_000a9660) or the flat pair at +0x40/+0x44 when [0x42] differs from 1, then calls
FUN_000a95f4; writes 2 directly when disabled. Caller 0x000A9526. Singleton. [I]

#### 0x000A95F4 `band_write_1e` — logic
Plain two-threshold classify of param+0x10 into param+0x1e (0: <=lo, 1: <=hi, 2: above).
Caller 0x000A9594. Singleton. [O]

#### 0x000A9616 `band1f_switched_pair` — logic
Two-threshold classify into param+0x1f using pair +0x28/+0x2c, swapped to +0x20/+0x24 when
*PTR_DAT_000a9664==1. Caller 0x000A9526. Singleton. [O]

#### 0x000A9668 `flag20_gear6_latch` — logic
Sets param+0x20=1 when PTR_FUN_000a975c()==6 && param+0xf==1 && param+0xd!=2 &&
PTR_DAT_000a9760[0x29]==1 && *PTR_DAT_000a9764==1 && [0x2a]==1; otherwise complex reset path
when the mode is not 8. Caller 0x000A8EFE. Singleton. [I]

#### 0x000A96DE `flag22_change_detect` — logic
param+0x22 = (PTR_FUN_000a975c() != PTR_FUN_000a9768()) — change detector between two mode
getters. Caller 0x000A8EFE. Singleton. [O]

#### 0x000A96FA `timer8_reload_mode8_vt` — logic
Byte-identical twin of 0x000A8F34 (mode-8 gated timer decrement/reload at param+8 with expiry
flag at +10); this copy has no direct callers — entered via a pointer table. Uses getter
0x000A7B78. Singleton. [O]

#### 0x000A9748 `flag24_eq_1` — data-accessor
param+0x24 = (*PTR_DAT_000a9770 == 1). Caller 0x000A8EFE. Singleton. [O]

#### 0x000A9774 `obd_mode1_dispatch` — logic
Entry for OBD mode 0x01 (current powertrain data): calls FUN_000a97ca(1, param, 1, 6) — length
window 1..6. Caller 0x00148050 sits in the OBD-mode service chain (spec 15 §2, modes
0x01–0x0A). Singleton. [O]

#### 0x000A9792 `obd_mode4_dispatch` — logic
Entry for OBD mode 0x04 (clear emission DTCs): calls FUN_000a97ca(4, param, 0). Caller
0x0008086C is also the UDS 0x14 ClearDiagnosticInformation handler (spec 15 §2). Singleton. [O]

#### 0x000A97A6 `obd_mode9_dispatch` — logic
Entry for OBD mode 0x09 (vehicle information): calls FUN_000a97ca(9, param, 1, 6). Caller
0x00148050 (OBD service chain). Singleton. [O]

#### 0x000A97BA `obd_req_store` — data-accessor
Stores request buffer/length into the OBD service object PTR_DAT_000a999c via FUN_000a9d44 and
copies byte[2] to byte[1] (sub-function echo). No direct callers — pointer-table entry.
Singleton. [O]

#### 0x000A97C4 `obd_state_clear` — data-accessor
Zeroes state bytes 1..3 of the OBD service object via FUN_000a9d50(PTR_DAT_000a999c). No
direct callers — pointer-table entry. Singleton. [O]

#### 0x000A97CA `obd_mode_request_dispatch` — dispatch
The OBD-mode request dispatcher: gates on session predicate and a linked byte, validates the
length window, then routes mode 1/2/3/4/7/9/0x0A to handlers 0x000A98BC/0x000A99C0/0x000A9AFA/
0x000A9B00/0x000A9B32/0x000A9B38/0x000A9BF4. Status codes 0x11/0x12/0x13/0x14/0x22/0x78 are
UDS NRCs (incl. responseTooLong 0x14, conditionsNotCorrect 0x22, pending 0x78); special path
when the request byte 0x10 has bit 7 sets bit 6. Callers 0x000A9774/0x000A9792/0x000A97A6.
Singleton. [O] (spec 15 §2)

#### 0x000A98BC `obd1_encode_pid_list` — logic
Mode 01 handler: copies the requested PID byte list to the scratch at obj+0xc, verifies the
list contains both zero and non-zero low-5-bit entries, pre-computes encoded sizes via codec
PTR_FUN_000a99b4 (measure-mode then write-mode), checks the response capacity at req+0x14
(NRC 0x14) and encodes each PID; returns 0 or NRC 0x12/0x13. Calls 0x000AE0C8. [I]

#### 0x000A99C0 `obd2_encode_pid_pairs` — logic
Mode 02 (freeze frame) handler: accepts request lengths 2/4/6 only (else NRC 0x13), de-
interleaves (pid, frame) byte pairs into arrays at obj+0xc/+0x12, size-checks and encodes two
bytes per entry via codec PTR_FUN_000a9c3c. Returns 0 or NRC 0x12/0x13/0x14. Calls 0x000AE2AA.
[I]

#### 0x000A9AFA `obd3_list_dtcs` — logic
Mode 03 (stored DTCs): tail-call FUN_000a9c18(obj, req, 0x200). Cluster of 2 — the member
0x000A9B32 is the mode 07 (pending DTCs) twin passing mask 0x100; both go through the shared
DTC reporter. [O] (spec 13)

#### 0x000A9B00 `obd4_clear_dtcs` — logic
Mode 04 handler: if the DTC clearing counter at PTR_DAT_000a9c38+0x16 is 0xFFFF or below the
table minimum at (*PTR_DAT_000a9c38)+0x44, invokes PTR_FUN_000a9c40(1,0) (clear trigger) and
zeroes the response length; else returns NRC 0x22 (conditionsNotCorrect). Calls 0x0009FA5E.
[O]

#### 0x000A9B38 `obd9_encode_info_chunks` — logic
Mode 09 (VIN/CAL-ID style info): stores the request at obj+0x18, pre-computes encoded sizes
per entry (codec PTR_FUN_000a9c44), rejects >1 non-zero entry (NRC 0x13), and on capacity
overflow sets obj bytes 2/3 (9, 0) and defers via FUN_000a9de6; NRC 0x12 when nothing encodes.
Calls 0x000AE472. [I]

#### 0x000A9BF4 `obdA_permanent_dtcs` — logic
Mode 0x0A (permanent DTCs): fetches gate object via PTR_FUN_000a9c48 into PTR_DAT_000a9c4c;
if its first byte is non-zero delegates to FUN_000a9c18(obj, req, 0x400), else NRC 0x12. Also
calls 0x000AA82E. [I]

#### 0x000A9C18 `obd_dtc_report_build` — logic
Shared DTC reporter for modes 03/07/0A: iterates the DTC list via PTR_FUN_000a9e84/0x000a9e88,
counts records whose status (uStack_60) intersects the mask (0x100/0x200/0x400), then writes
[count][DTC hi][DTC lo]... big-endian into the response via PTR_FUN_000a9e8c; modes 03/07
also advance the DTC cursor (PTR_FUN_000a9e94, *DAT_000a9e98). NRC 0x14 when the response
buffer is too small. Callers 0x000A9AFA/0x000A9B32/0x000A9BF4. [O] (spec 13)

#### 0x000A9D44 `obd_req_store_fields` — data-accessor
Writes request pointer (+4) and length (+8) into the service object and copies byte[2]->byte[1].
Caller 0x000A97BA. Singleton. [O]

#### 0x000A9D50 `obd_clear_state_123` — data-accessor
Zeroes object bytes 1, 2, 3. Caller 0x000A97C4. Singleton. [O]

#### 0x000A9D5A `obd_clear_state_0_3` — data-accessor
Zeroes object bytes 0..3. Caller 0x000A1170. Singleton. [O]

#### 0x000A9D66 `obd_multi_frame_tick` — logic
Continuation tick for chunked mode-09 responses: when obj[0]==1 and the gate passes, encodes
entry 0x06 (0x78 pending when the response has only 1 byte left) and finishes via
FUN_000a9de6; when obj[1]==9 runs FUN_000a9de6 and reports NRC 0x22 upward. Caller 0x000A123C.
[I]

#### 0x000A9DE6 `obd_encode_remaining` — logic
Encodes remaining request entries into the response in chunks: per entry calls codec
PTR_FUN_000a9ea8, stores the info-code byte plus payload, tracks the running total against the
response capacity at obj+8; returns 0x78 when entry 0x06 needs more space. Callers 0x000A9B38,
0x000A9D66. [I]

#### 0x000A9EB8 `obd_clear_state_01` — data-accessor
Zeroes the first two words of the object (bytes 0 and 4 in the u32 view). Callers 0x000AE890,
0x000A1170. Singleton. [O]

#### 0x000A9EC0 `id16_cache_clear` — data-accessor
Zeroes *PTR_DAT_000a9fd0 and the 16-byte cache at PTR_DAT_000a9fd4. Caller 0x000ACA68.
Singleton. [O]

#### 0x000A9EDE `id16_get_with_change` — logic
Fetches a 16-byte block via PTR_PTR_000a9fdc into a local, compares it against the cached
PTR_DAT_000a9fd4; on first difference refreshes the cache (after PTR_FUN_000a9fe4/PTR_FUN_
000a9fe8 side effects) and returns the refreshed pointer, else returns the cache. Caller
0x000AE868; also calls 0x000764A0/0x0009A978/0x000ACA68/0x0009E74C. [I]

#### 0x000A9F72 `xor_combine_two_vals` — logic
One-shot (state byte at +4): XORs the results of PTR_PTR_000a9fec(0) and (1), stores the
result into *param_1 and *PTR_DAT_000a9fd0 and advances state to 9. Caller 0x000A123C, calls
0x0006F53A. Singleton. [I]

#### 0x000A9FC2 `ret_deref_arg` — stub
Two-instruction `return *param_1`. Cluster of 21 — copies at 0x000BC816, 0x000BCD84,
0x000BD530, 0x000BE096, 0x000BE596, 0x000BEA88, 0x000BEF86, 0x000BF474, 0x000BF754,
0x000BFA7E, 0x000BFD02, 0x000C005E, 0x000C0398, 0x000C064E, 0x000C080E, 0x000C0AF0,
0x000C0CB6, 0x000C0DF2, 0x000C1152, 0x000C1EF6 (r0c_app) — first-field getters on small
service objects. Caller 0x000AE472 for the rep. [O]

#### 0x000A9FC6 `arg1_nonzero` — stub
Returns (*param_1 != 0). Callers 0x000A9D66, 0x000AE472. Singleton. [O]

#### 0x000A9FF0 `get_fff8e573` — data-accessor
Returns byte DAT_fff8e573 — a global gate/mask AND-ed into data reads by 0x000AA09A and
0x000AA176. Eight callers incl. 0x00074C20, 0x00087CB8, 0x00088068, 0x0008848C, 0x000C8B94.
Singleton. [O]

#### 0x000A9FF8 `chan_set_state_by_idx` — logic
Bounds-checks index param_1 against *PTR_DAT_000aa1dc and param_2 in {0,1}; direct call
PTR_FUN_000aa1e4(1,idx,state) when *PTR_DAT_000aa1e0==0, else resolves a slot via
PTR_FUN_000aa1e8 and calls PTR_FUN_000aa1f4 through *DAT_000aa1f0. Caller 0x000C8930. [I]

#### 0x000AA058 `dyndef_begin` — logic
Starts a dynamically-defined read: zeroes counters at obj+0x14/+0x16/+0x18, validates the
opcode via PTR_FUN_000aa200, and on success stores opcode + four caller parameters into the
request object at PTR_DAT_000aa1fc. Callers 0x00087CB8, 0x00088068, 0x0008848C (diagnostic
service handler area, spec 15 §2). [I]

#### 0x000AA09A `chan_read_gated` — logic
Reads a channel state via PTR_FUN_000aa204 and combines two indirect transforms
(*DAT_000aa1f8, *DAT_000aa208) AND the global DAT_fff8e573 into *param_3; returns 1/2 error
codes when the object is inactive or param_2 != 1. Callers 0x0008D2B4, 0x00074C20. [I]

#### 0x000AA10C `dyndef_count_entries` — logic
Iterates the definition table: for each of up to 400 positions queries
PTR_FUN_000aa214 until it returns exhausted, counting completed entries into obj+0x18 (with
retry counter at +0x14 capped at 399). Returns the entry count. Callers 0x00087CB8/0x00088068/
0x0008848C. [I]

#### 0x000AA176 `dyndef_read_values` — logic
Reads the defined identifier list: loops positions obj+0x14 < 400, honors the entry-count limit
at **PTR_DAT_000aa218 (error 3), decodes each entry via PTR_FUN_000aa2e0/0x000aa2e4, ANDs the
result byte with the DAT_fff8e573 gate (0x000A9FF0), and for mode 1 optionally re-encodes via
PTR_FUN_000aa2e8/0x000aa2ec/0x000aa2f0/0x000aa2f8. Returns 0..3 status. Callers 0x00087CB8,
0x00088068. [I]

#### 0x000AA2FC `dyndef_status_remap` — logic
Calls the 7-arg worker PTR_FUN_000aa4b8 and remaps internal status {0,2,3,4,5} -> {0,3,2,4,5}
(default 1). Caller 0x00087F6C. Singleton. [O]

#### 0x000AA358 `bits_clear_mask_4c4` — logic
Clears param_1 bits in the mask byte *PTR_DAT_000aa4c4 (0 clears all) and applies the
complement (~a|~b)&0xff via PTR_FUN_000aa4cc; error 1 when inactive. Callers 0x00087F6C,
0x00075BE6. [I]

#### 0x000AA39C `bits_set_mask_4c4` — logic
Sets param_1 bits in *PTR_DAT_000aa4c4 (0 sets all) and applies (mask&value) via
PTR_FUN_000aa4d0. Callers 0x000C8930, 0x00087F6C, 0x00075C56, 0x00075BE6. [I]

#### 0x000AA3DA `bits_clear_mask_4c8` — logic
Clears param_1 bits in the value byte *PTR_DAT_000aa4c8 and applies (~mask|~value)&0xff via
PTR_FUN_000aa4cc; returns 1 on success, 0 when inactive. Callers 0x000C8930, 0x00087F6C. [I]

#### 0x000AA41E `bits_set_mask_4c8` — logic
Sets param_1 bits in *PTR_DAT_000aa4c8 and applies (mask&value) via PTR_FUN_000aa4d0; 0 when
inactive. Callers 0x000C8930, 0x00087F6C. [I]

#### 0x000AA45E `chan4_set_notify` — logic
For channel param_1 < 4 and state param_2 in {0,1}: direct call PTR_FUN_000aa4d8(2,ch,state)
when the object is inactive, else updates the channel byte at PTR_DAT_000aa4d4+ch and fans out
a notify loop 1..0xc3 through PTR_FUN_000aa4dc (off) or PTR_FUN_000aa500 (on). Caller
0x000C8930. [I]

#### 0x000AA504 `clear_3bytes_51c` — data-accessor
Zeroes the first 3 bytes at PTR_DAT_000aa51c. Caller 0x000ACA68. Singleton. [O]

#### 0x000AA522 `meas_snapshot_read13` — logic
Refreshes limit flags via FUN_000aa590, reads two words via PTR_FUN_000aa5f4(0)/(1) into
param+4/+8, calls PTR_FUN_000aa5f8(param+0xc) and copies 13 words from PTR_DAT_000aa5fc into
param+0xe. Caller 0x000A123C. Singleton. [I]

#### 0x000AA560 `meas_limit_check_a` — logic
Compares the word at PTR_DAT_000aa600+10 against **(obj+0x344) setting flag obj+10, and byte
at +0xc against **(obj+0x348) setting bit 1 of obj+0x29. Caller 0x000A1296. Singleton. [I]

#### 0x000AA590 `meas_limit_check_b` — logic
Five threshold tests: word at PTR_DAT_000aa600+6 against **(obj+0x334)/(+0x338)/(+0x33c),
word at +8 against **(+0x340), and byte[4]==1 — setting bits 2/4/8/0x10 of obj+0x2b and a
flag at obj+0x2a. Caller 0x000AA522. Singleton. [I]

#### 0x000AA604 `meas_get_u16_by_idx` — data-accessor
Index read (param_1 < 400) of a u16 through the table accessor PTR_FUN_000aa748 after
PTR_FUN_000aa744; returns 0xff when out of range — the denied-read marker. Cluster of 2 —
member 0x000AA632 is the byte-identical twin (callers 0x000C8A70, 0x000AA176, 0x000C8C4A).
Rep callers 0x000C8C06, 0x000C8A70, 0x000C8C4A. [I] (spec 10 §5 index table)

#### 0x000AA662 `meas_get_u32_by_idx` — data-accessor
32-bit variant: PTR_FUN_000aa74c after PTR_FUN_000aa744, 0xff when idx >= 400. Callers
0x000C8A70, 0x000AA176. Singleton. [I]

#### 0x000AA692 `meas_find_get_u16` — logic
Finds a table record by (param_1, param_2) pair via FUN_000aa6f6 (stride 0xc scan), then reads
a u16 field via PTR_FUN_000aa748; 0xff when not found. Callers 0x000A9C18 (OBD DTC reporter)
and 0x0009E8CC/0x000ADB34/0x000AE2AA/0x000ADC1E/0x0009E74C. [I]

#### 0x000AA6C4 `meas_find_get_u32` — logic
Same lookup, returns the u32 field via PTR_FUN_000aa74c. Callers 0x000A0DB4, 0x000AD606.
Singleton. [I]

#### 0x000AA6F6 `tbl_find_stride12` — logic
Linear scan of records (stride 0xc, up to 400) comparing the first two fields against
(param_1, param_2) via PTR_FUN_000aa748; stores the record base or 0 into *param_3. Callers
0x000AA692, 0x000AA6C4. Singleton. [O]

#### 0x000AA750 `tbl_word_at_0x30c` — data-accessor
Returns the u32 at (*PTR_DAT_000aa86c)+0x30c. Cluster of 10 — byte-identical fixed-offset
readers of the same table: 0x000AA75A +0x310, 0x000AA764 +0x314, 0x000AA77C +0x350,
0x000AA786 +0x324, 0x000AA790 +0x330, 0x000AA79A +0x318, 0x000AA7A4 +0x31c, 0x000AA7B8
+0x328, 0x000AA7C2 +0x32c. Rep callers 0x000AAFE8, 0x000AD430, 0x0009F7AA, 0x000AD072,
0x000ACEA4. [O]

#### 0x000AA76E `tbl_word_by_idx1` — data-accessor
Returns the u32 at (*PTR_DAT_000aa86c)+(idx-1)*4 — the most-used table lookup of the unit (39
callers across the 0x9B-0x9E DTC band, 0xAB and 0xAC code). Singleton. [O]

#### 0x000AA7AE `tbl_word_at_800` — data-accessor
Returns the u32 at (*PTR_DAT_000aa86c)+800. Callers 0x000AD430, 0x000AD072, 0x000AABBC,
0x000ACEA4. Singleton. [O]

#### 0x000AA7CC `tbl870_word_at_off` — data-accessor
Family of 16 identical `return *(u32*)(*DAT_000aa870 + off)` getters reading successive fields
(off 0x0c..0x54) of the record table at *DAT_000aa870 — companion of the 0x000AA76E index
lookup. Members and offsets: 0x000AA7CC +0x20, 0x000AA7D4 +0x10, 0x000AA7DC +0x14, 0x000AA7E4
+0x18, 0x000AA7EC +0x1c, 0x000AA7F4 +0x24, 0x000AA7FC +0x28, 0x000AA804 +0x2c, 0x000AA80C
+0x50, 0x000AA816 +0x30, 0x000AA81E +0x34, 0x000AA826 +0x38, 0x000AA82E +0x54, 0x000AA838
+0x3c, 0x000AA840 +0x40, 0x000AA862 +0xc. Callers across the 0x9B-0x9E DTC band. [O]

#### 0x000AA84A `tbl870_word_0` — data-accessor
Returns *(u32*)*DAT_000aa870 (offset 0). Caller 0x0009F23A. Singleton. [O]

#### 0x000AA852 `tbl870_word_4` — data-accessor
Returns *(u32*)(*DAT_000aa870+4). Caller 0x0009F23A. Singleton. [O]

#### 0x000AA85A `tbl870_word_8` — data-accessor
Returns *(u32*)(*DAT_000aa870+8). Caller 0x0009F23A. Singleton. [O]

#### 0x000AA874 `dtc_request_flag_set` — dispatch
Sets one of the 20 DTC request flags DAT_fff8e57e..DAT_fff8e591 by index 1..0x14; case 0x11
additionally latches word DAT_fff8e20c (guarded against double-set), case 0x13 ORs param_2
into DAT_fff8e590. Returns 0xffff for unknown indices. Callers 0x000A0074, 0x000A0AA6,
0x0009F944, 0x000A086A, 0x0009FDC4. Singleton. [O]

#### 0x000AA948 `dtc_request_flags_tick` — logic
293 instrs: for each set request flag queries its fulfillment via PTR_FUN_000aaba0(1,i) and
clears it when done; index 0x11 uses PTR_FUN_000aaba4(1,DAT_fff8e20c); the 0xff-condition
clears the whole flag block. Caller 0x000AAFA8 (master tick). Singleton. [O]

#### 0x000AABBC `dtc_gate_snapshot` — logic
When DAT_fff8e57d==0xA5 and DAT_fff8e575==0: one-shot pass running FUN_000aac0e +
FUN_000aae6c (guarded by DAT_fff8e575), then captures three pointers via PTR_FUN_000aae1c/
0x000aae24/0x000aae2c into PTR_DAT_000aae20/28/30 with first bytes into DAT_fff8e57b,
DAT_fff89810/14. Caller 0x000AAFA8. [I]

#### 0x000AAC0E `dtc_records_scan_mode2` — logic
Scans DTC records (stride 0x38) at *PTR_PTR_000aae48 up to the count from PTR_FUN_000aae34:
per record tests status bits (0x10/0x08/0x40), validates via PTR_FUN_000aae50(h,rec,2),
accumulates condition masks through PTR_FUN_000aae54 into *PTR_DAT_000aae38/3c, clears
status bit 0x20/0x40 cells on failure and latches DAT_fff8e577 (change flag) when anything
changed. Callers 0x000ABF98, 0x0009CF88, 0x000AABBC. [I] (spec 13)

#### 0x000AACE4 `dtc_records_scan_reset` — logic
Mode-1 variant of the record scan; at completion clears the 0xc3-entry word array at
PTR_DAT_000aae5c, two 0x20-byte areas DAT_fff89818/DAT_fff89858, the cells at
PTR_DAT_000aae60/64, and AND-masks another 0xc3 word array with 0xdbbb. Returns 1 if any
record matched. Caller 0x000A0856. [I]

#### 0x000AAE6C `dtc_records_scan_debounced` — logic
Runs only while the debounce counter DAT_fff8e57a-- > 0: record scan combining membership in
the 20-entry recent list (FUN_000aaf7e), status bits and two masked predicates
(PTR_FUN_000aafcc/0x000aafd0), OR-ing result masks into *PTR_DAT_000aafdc/PTR_DAT_000aafd8
and clearing bit 0x20 cells otherwise. Sets DAT_fff8e577 on any change. Caller 0x000AABBC. [I]

#### 0x000AAF7E `dtc_recent_list_lookup` — logic
Linear scan of the 20-entry short list at DAT_fff897e8 (0-terminated) for param_1; returns
1 on hit. Caller 0x000AAE6C. Singleton. [O]

#### 0x000AAFA8 `dtc_master_tick` — logic
Periodic orchestrator: FUN_000aabbc, 0x000aa948, 0x000ab23a, 0x000ab2d4 plus one indirect
call PTR_FUN_000aafe4. Caller 0x000A1296 (scheduler tick family). Singleton. [O]

#### 0x000AAFE8 `dtc_module_reset` — logic
Full reset of the fault module: clears the 0xc3 word arrays at PTR_DAT_000ab25c/260, runs
FUN_000abada + FUN_000ab31c, zeroes the pointer cells PTR_DAT_000ab264..278, change flags
DAT_fff8e576..e579, the 13-byte DAT_fff8dee6, the two 0x20-byte counter areas, all 20 request
flags, the 20-entry recent list, and re-latches state DAT_fff8e57d='Z' when the watched byte
is 'Z' or 0. Callers 0x000AE868, 0x000A086A. [O]

#### 0x000AB102 `dtc_occurrence_inc` — logic
For DTC index param_1 whose status word (PTR_DAT_000ab25c array) lacks bit 0x40: calls two
predicates, sets bit 0x40 in the DAT_000ab298 mirror, ORs masks into *PTR_DAT_000ab274/278
and increments the matching counters in the two 0x20-entry arrays DAT_fff89818/fff89858
(bit-position counted). Caller 0x0009C516. [I]

#### 0x000AB1A6 `dtc_occurrence_dec` — logic
Inverse of 0x000AB102: clears bit 0x40, then decrements the 0x20-entry counters and clears
the corresponding mask bits in *PTR_DAT_000ab274/278 when a counter reaches zero. Callers
0x0009C89A, 0x0009B918, 0x0009C81A, 0x0009BC38, 0x0009C58C, 0x0009C98A, 0x0009C6A8,
0x0009C342. [I]

#### 0x000AB23A `dtc_reset_trigger_watch` — logic
Edge watcher on the state byte from PTR_FUN_000ab29c: on a change to 0x55/0xAA (and armed
only when the previous gate byte is 0) latches DAT_fff8e592 and runs a reset cascade over the
record table (writes status 0x50 into slots via DAT_0009fd98, decrements counts through
PTR_FUN_0009fd94, guarded by the 0xA5/0xAA/0x55 state bytes at PTR_DAT_0009fb9c). Caller
0x000AAFA8. Body partly collapsed by Ghidra dead-code removal. [I]

#### 0x000AB2D4 `dtc_reset_watch_b` — logic
Smaller twin of 0x000AB23A: watches the byte from PTR_FUN_000ab514; on change to 0x55/0xAA
(when armed) latches DAT_fff8e593 and fires PTR_FUN_000ab520. Caller 0x000AAFA8. Singleton.
[I]

#### 0x000AB31C `dtc_status_words_init` — logic
ANDs 0xedff over the 0xc3 status words at PTR_DAT_000ab524 and zeroes the parallel arrays at
DAT_fff8df00 and DAT_fff8e086 (0xc3 entries). Callers 0x000AAFE8, 0x000A0856. Singleton. [O]

#### 0x000AB356 `dtc_clear_group_aging` — dispatch
Group clear over the 0xc3 DTC slots: selector param_2 0/0xff = all, 1..2 by status bit 8,
3..6 by predicate PTR_FUN_000ab528/0x000ab7c8, 7..8 by status bit 0x80, 9..0xb by
PTR_FUN_000ab7cc — each pass zeroes the DAT_fff8df00/DAT_fff8e086 pair cells and strips bits
0x1200 from the status word. param_1!=0 path clears slots whose record byte at +0x20 matches
a param_2 mask. Callers 0x0009FA5E, 0x0009FDC4. [I] (spec 13)

#### 0x000AB784 `dtc_slots_set_0x50` — data-accessor
Writes 0x50 into all 0xc3 bytes at PTR_DAT_000ab7d4 (DTC slot status bytes). Caller
0x000A086A. Singleton. [O]

#### 0x000AB79C `dtc_slot_set_0x50_idx` — data-accessor
Writes 0x50 into slot param_1 of PTR_DAT_fff823f4_3_000ab7d8. Callers 0x000A0074, 0x0009ED10.
Singleton. [O]

#### 0x000AB7B6 `dtc_slot_mask_0x77_idx` — data-accessor
ANDs slot param_1 with 0x77. Callers 0x000ABF98, 0x0009B918, 0x0009C98A, 0x0009C6A8.
Singleton. [O]

#### 0x000AB7DC `dtc_clear_group_status50` — dispatch
Structural twin of 0x000AB356 operating on the PTR_DAT_000aba24/28 slot group: the same 12
selectors, but the action is writing status byte 0x50 (via PTR_FUN_000aba2c/0x000abc7c/0x000abc80
predicates; param_1!=0 path masks against record byte +0x20). Callers 0x0009FA5E,
0x0009FDC4. [I]

#### 0x000ABADA `dtc_words_clear_c3` — data-accessor
Zeroes the 0xc3-entry word array at PTR_DAT_000abc88. Caller 0x000AAFE8. Singleton. [O]

#### 0x000ABAF2 `dtc_clear_group_words` — dispatch
Third twin of the group-clear family: zeroes the 0xc3 word arrays selected by the same 12
selectors over the PTR_DAT_000abc78/88 and PTR_DAT_000abeec/f0 groups (predicates
PTR_FUN_000abc7c/0x000abc80/0x000abef4/0x000abef8, record mask via PTR_FUN_000abefc).
Callers 0x0009FA5E, 0x0009FDC4. [I]

#### 0x000ABDFA `dtc_record_validate` — logic
Validates a DTC record against its condition handle (param_1+0x1c via PTR_FUN_000abf00):
selects a nibble of the status word by mode param_3 (0: bits 0-3, 1: bits 4-7, else bits
8-11), then disqualifies on record byte+5 bit 3, a threshold compare, or a masked-pair
mismatch; the mode-0 failure path appends the code to the 20-entry recent list DAT_fff897e8
and reloads the debounce counter DAT_fff8e57a from *PTR_FUN_000abf08. Callers 0x000AACE4,
0x000A016A, 0x000AAC0E. Singleton. [I]

#### 0x000ABF28 `dtc_list_count_set` — data-accessor
Sets the used-count word for DTC list 1 (*PTR_DAT_000ac0e4, capacity 0x14) or list 2
(*PTR_DAT_000ac0e0, capacity 10). 13 callers across the fault band. Singleton. [O]

#### 0x000ABF40 `dtc_list_count_refresh` — logic
Recounts non-zero entries of the chosen list (stride 0x38 records) and stores the count via
FUN_000abf28. Caller 0x0009F7AA. Singleton. [O]

#### 0x000ABF7C `dtc_list_count_get` — data-accessor
Returns the used count of list 1 or 2 (0xffff otherwise) — the hottest accessor of the fault
band (25 callers). Singleton. [O]

#### 0x000ABF98 `dtc_record_insert` — logic
Inserts a DTC event: while list 1 has room, FUN_000ac324 stores it and bumps the count; when
full, selects a victim via FUN_000ac208/0x000ac214, retires it (clears bit 0x10 cell, runs
PTR_FUN_000ac0fc/100/108, decrements count), and reuses the slot. Sets *PTR_DAT_000ac110 on
failure 0xffff. Callers 0x0009B918, 0x0009C98A, 0x0009C6A8. [O]

#### 0x000AC07A `dtc_list_find_code` — logic
Linear search of list 1 or 2 (stride 0x38, bounded by the count words) for DTC code param_1;
returns index or 0xffff. 12 callers. Singleton. [O]

#### 0x000AC114 `dtc_record_delete_at` — logic
Deletes record index param_1: clears its bit in five parallel word arrays (PTR_DAT_
000ac2e0..f0), masks the status word cell (0xed47), zeroes counters at PTR_DAT_000ac2fc..308,
then compacts the remaining records (per-move callback *DAT_000ac310) and zeroes the vacated
0x38-byte tail. Callers 0x000A0074, 0x0009FA5E, 0x0009ED10, 0x0009EC40, 0x0009F944,
0x0009FDC4. [O]

#### 0x000AC208 `dtc_victim_select_thunk` — stub
Tail-jump into FUN_000ac214. Caller 0x000ABF98. Singleton. [O]

#### 0x000AC214 `dtc_victim_select` — logic
Chooses which of the 20 records to evict: filters with PTR_FUN_000ac31c/0x000ac320, scores by
priority nibble (status & 0x70) with a two-tier preference (records with status bits 8/0x10
clear win, then lowest priority), returns the index or 0xffff. Caller 0x000AC208. [I]

#### 0x000AC324 `dtc_record_store` — logic
Builds a 0x38-byte DTC record at slot param_3: code param_1, sub param_2, status 0x4f,
occurrence id from FUN_000ac4c8, env-source byte via FUN_000ac508, condition bits 0x10/0x80/
0x100 from PTR_FUN_000ac52c/530/544, then maintains the 10-entry circular history ring
(pointer cell *(PTR_DAT_000ac554+0x69c), bounds PTR_DAT_000ac558..+0x230) bumping list-2
counts. Caller 0x000ABF98 (and 15 callee contacts). [I]

#### 0x000AC48E `env_block_fetch_0x13` — logic
Fetches a 0x13-byte environment snapshot via PTR_FUN_000ac568 and, when the keyed fetch
(PTR_FUN_000ac56c with key 0x13) fails, zeroes the 19-byte block. Callers 0x0009CE52,
0x000C919E. Singleton. [I]

#### 0x000AC4C8 `dtc_next_occurrence_id` — logic
Scans the 20 records' byte+9 occurrence counters and returns max+1 (0xff when saturated).
Caller 0x000AC324. Singleton. [O]

#### 0x000AC508 `dtc_env_source_pick` — logic
Picks record byte +7 from the table entry (param_2+7) when param_3==0 and **PTR_DAT_000ac538
!= 1, else from **PTR_DAT_000ac540. Callers 0x000AC324, 0x0009CE52. Singleton. [I]

#### 0x000AC570 `dtc_consistency_orchestrate` — logic
One-shot (gate *PTR_DAT_000ac79c) aggregation of ten integrity checks — FUN_000ac5c4/5ea/646/
692/6d6/718/752/7d0/814/82a — OR-ed into *PTR_DAT_000ac7a0; fires PTR_FUN_000ac7a4 when any
bit was set. Caller 0x0009F7AA. Singleton. [O]

#### 0x000AC5C4 `dtc_chk_slot_overflow` — logic
Returns bit 1 when any of the 20 event records carries a first-field value > 0xc3 (invalid
slot number). Caller 0x000AC570. Singleton. [O]

#### 0x000AC5EA `dtc_chk_duplicate_code` — logic
Returns bit 2 when two of the 20 records share the same code while the filter predicate
PTR_FUN_000ac7ac says the first is inactive. Caller 0x000AC570. Singleton. [O]

#### 0x000AC646 `dtc_chk_record_garbage` — logic
Returns bit 4 when a record beyond the used count or any zero-code record still has non-zero
bytes anywhere in its 0x38-byte body. Caller 0x000AC570. Singleton. [O]

#### 0x000AC692 `dtc_chk_flag_2000_cond` — logic
Returns bit 8 when some record has filter-clear state, zero word at +6 and no status bit
0x2000. Caller 0x000AC570. Singleton. [I]

#### 0x000AC6D6 `dtc_chk_flag_20_cond` — logic
Returns bit 0x10 when some record is filter-clear, has byte+6 >= 2 and no bit 0x20 in byte+5.
Caller 0x000AC570. Singleton. [I]

#### 0x000AC718 `dtc_chk_occurrence_cond` — logic
Returns bit 0x20 when some record is filter-clear with a non-zero occurrence counter at +9.
Caller 0x000AC570. Singleton. [I]

#### 0x000AC752 `dtc_chk_env_limit` — logic
Returns bit 0x40 when some filter-clear record's byte+7 exceeds both the per-record table
limit (record+7 of PTR_FUN_000ac7b8 lookup) and the global **PTR_DAT_000ac7b4 limit. Caller
0x000AC570. Singleton. [I]

#### 0x000AC7D0 `dtc_chk_ring_overflow` — logic
Returns bit 0x2000 when a history-ring record (10 entries at *PTR_PTR_000ac9ac) has slot > 0xc3
or the ring pointer PTR_DAT_000ac9b0 is outside its buffer PTR_DAT_000ac9b4..+0x230.
Caller 0x000AC570. Singleton. [O]

#### 0x000AC814 `dtc_chk_first_slot` — logic
Returns bit 0x4000 when the first record of a second list (*PTR_PTR_000ac9b8) has slot > 0xc3.
Caller 0x000AC570. Singleton. [O]

#### 0x000AC82A `dtc_chk_ff_bytes` — logic
Returns bit 0x8000 when any of the 0xc3 bytes at PTR_DAT_000ac9bc is 0xff (uninitialized
flash markers). Caller 0x000AC570. Singleton. [O]

#### 0x000AC84E `ring_bump_occurrence` — logic
Walks the 10-entry history ring backwards (stride 0x38, wrap to PTR_DAT_000ac9b4+0x1f8) for
code param_1 and increments its byte +6 (unless 0xff/-1). Caller 0x0009CE52. Singleton. [O]

#### 0x000AC890 `ring_clear_and_reset` — logic
Zeroes all ten 0x38-byte history records, resets the ring pointer PTR_DAT_000ac9b0 and the
list-2 count (FUN_000abf28(2,0), PTR_FUN_000ac9c0(2,0)). Callers 0x000A0796, 0x000A086A.
Singleton. [O]

#### 0x000AC8CA `dtc_list1_clear` — logic
Zeroes all twenty 0x38-byte records at *PTR_PTR_000ac9c4 and resets the list-1 count
(FUN_000abf28(1,0) + PTR_FUN_000ac9c0(1,0)). Caller 0x000A086A. Singleton. [O]

#### 0x000AC8FE `dtc_aging_tick` — logic
One-shot (gate *PTR_DAT_000ac9c8) pass over the used list-1 records: for records matching
filter polarity param_1 without status bit 0x2000, increments the two aging bytes at +0xa and
+0xc (saturating at 0xff). Callers 0x0009F7AA, 0x0009D158. Singleton. [O]

#### 0x000AC97E `dtc_aging_bump_cond` — logic
Bumps the aging byte at record+0xb when the slot status cell (DAT_000ac9d0 array) has bit 0x40
set, bit 2 clear, no status bit 0x2000 and the byte is not saturated. Callers 0x0009CF88,
0x0009C43C, 0x000A0BD6. Singleton. [O]

#### 0x000AC9D4 `sig_recheck_tick` — logic
If the record's counter (*param_1) is non-zero and the signature verify FUN_000acb2e fails,
re-captures the reference block via FUN_000aca68 and fires PTR_FUN_000acb9c. Cluster of 2 —
0x000AC9F2 is the byte-identical twin called from the other scheduler stub (0x000A1296 vs
0x000A1170). [O]

#### 0x000ACA10 `sig_recheck_flags6` — logic
Virtual preamble call then, when param_2 has bits 1/2 set, recapture via FUN_000aca68 +
PTR_FUN_000acb9c; always increments the word at param+4. Caller 0x000A10B6. Singleton. [O]

#### 0x000ACA36 `sig_capture_snapshot` — logic
Stores the signature-verify result (low byte) at param_1[1] and increments the word at
param_1[0]. Caller 0x000A108E. Singleton. [O]

#### 0x000ACA4A `sig_valid_and_count` — logic
Returns 1 only when the word at param+4 is non-zero and FUN_000acb2e() == 1. Caller
0x000A109C. Singleton. [O]

#### 0x000ACA68 `sig_recapture_block` — logic
Refreshes the reference signature: five virtual preamble calls, copies 9 bytes
PTR_DAT_000acbcc->000acbc8 and 12 bytes PTR_DAT_000acbd0->000acbd4, writes marker 0x26 to
*DAT_000acbd8 and bumps the word at param+0xa. Callers 0x000AC9D4/0x000AC9F2/0x000ACA10/
0x000A9EDE. [I]

#### 0x000ACAC4 `dtc_rec_code_get` — data-accessor
Bounds-checked (list1 idx<=0x13 / list2 idx<=9) read of the record's first short (DTC code)
from PTR_DAT_000acbe0/0x000acbdc. Caller 0x000A10C6. Singleton. [O]

#### 0x000ACAF8 `dtc_rec_field2_get` — data-accessor
Same bounds check, returns the short at record+2. Caller 0x000A10D8. Singleton. [O]

#### 0x000ACB2E `sig_verify` — logic
Compares the working block (PTR_DAT_000acbc8/000acbd4) against the reference
(PTR_DAT_000acbcc/000acbd0) byte-for-byte (9 + 12 bytes) plus marker *DAT_000acbd8 == 0x26,
storing the pass/fail flag at param+8 and bumping param+6. Callers 0x000AC9D4/0x000AC9F2/
0x000ACA36/0x000ACA4A. [I]

#### 0x000ACBE4 `mon_tbl25_find` — logic
Scans the 25-entry table via PTR_FUN_000ace4c (fed from *(param_1+0x24)+4) for short param_2;
returns 1 on hit. Caller 0x000ACEA4. Singleton. [O]

#### 0x000ACC12 `mon_tbl25_match_or_masks` — logic
Finds the entry (25 max) by key param_2, then requires both condition words (*DAT_000ace54 at
idx, arg 0 and 1) to intersect PTR_FUN_000ace50(0)/(1); on success ORs them into param_1[0]/
[1]. Caller 0x000ACEA4. Singleton. [I]

#### 0x000ACC94 `mon_any_condition_active` — logic
Iterates the 25 entries, fetches each condition word via PTR_FUN_000ace5c and returns 1 when
any intersects the gate byte from PTR_FUN_000ace58. Caller 0x000AD072. Singleton. [I]

#### 0x000ACCF2 `mon_mask_reconcile` — logic
Masks param_1[0]/[1] with PTR_FUN_000ace50(0)/(1), then walks the two pseudo-indices taken
from PTR_s__0123456789 (0x30) and *PTR_DAT_000ace64: for each bit set in the pre-mask values
calls FUN_000acdcc to clear the corresponding debounce cell. Caller 0x000AD072. [I]

#### 0x000ACD92 `mon_bitpair_test` — logic
Maps byte param_2 (0..0x3f) to a single bit in word0 (idx<0x20) or word1 (0x20..0x3f) and
returns whether it is set in the monitor's mask pair. Callers 0x000AD072, 0x000AD430. [O]

#### 0x000ACDCC `mon_debounce_clear` — logic
Clears debounce cell 0 (at param+0x1e), 1 (+0x1f) or 2 (+0x20) by index. Caller 0x000ACCF2.
Singleton. [O]

#### 0x000ACDEC `mon_debounce_update` — logic
Three-state debounce per channel param_2: cell at +0x1e..0x20 walks 0->1->2 on mismatches
against the expected byte at +0x1b..0x1d, reset to 0/kept on match; reaching state 3 jumps to
the fatal path (PTR_PTR_000ad0d0 report + infinite loop). Caller 0x000AD430. [O]

#### 0x000ACEA4 `mon_validate_event` — logic
144 instrs: guards on four state bytes (0xA5/0x56/'Z'), then validates event param_2 — the
0xfe path requires status bit 8 (not 0x8000) plus FUN_000acbe4 membership and
FUN_000acc12 mask pair; other paths check param_3 -1/-2 variants; every violation jumps to
the fatal report+spin path. Callers 0x000A107C, 0x000AD072. [I]

#### 0x000ACFD0 `mon_flags_init` — data-accessor
Initializes the monitor's word pattern: +0x10/+0x14/+0x16 = 0xffff, +0xa/+0xc = 0xa5. Caller
0x000A1170. Singleton. [O]

#### 0x000ACFE6 `mon_state_init` — logic
Zeroes the monitor object (mask words, counters) with 0xffff sentinels at +0x10/+0x16 and
clears the three expected/debounce cell triples. Caller 0x000AE890. Singleton. [O]

#### 0x000AD034 `mon_register_channels` — logic
Registers four monitor entries via *DAT_000ad0f8: (1,18000000,1), (1,0x112a881,1),
(3,0x112a882,1), (3,0x112a883,1). Caller 0x000ADE60. Singleton. [I]

#### 0x000AD072 `mon_task_a` — logic
372-instr periodic monitor (no direct callers — thread/pointer table): optional 18000000-timeout
arm, FUN_000acc94 condition gate driving the 0xfe event path through FUN_000acea4, then three
debounce groups (counters at +8/+0xc/+0xe with complement words at +0x10/+0x14/+0x16 checked
with ~), latched from *DAT_000ad318..358 tables, followed by FUN_000accf2 mask reconcile and
a program-identity compare that jumps to the fatal spin when it fails. [I]

#### 0x000AD430 `mon_task_b` — logic
Second monitor task (no direct callers): for the two condition sources PTR_DAT_000ad5e8 /
PTR_s__0123456789__ it reads the current values into +0x18..0x1a and runs FUN_000acdec
debounce per channel, or clears all three expected/debounce triples when neither condition is
armed; ends with the same program-identity check + fatal spin. [I]

#### 0x000AD600 `clear_byte_arg4` — stub
`param_1[4] = 0`. Cluster of 3 — twins at 0x001084CE and 0x001369C0 in later regions.
Callers 0x000AE890, 0x000A1170. [O]

#### 0x000AD606 `ident_records_ensure` — logic
Rebuild path for the identification records: gated on PTR_FUN_000ad874()&8, looks up the
record (PTR_FUN_000ad87c), checks staleness against the captured timestamp (delta < 0x64b) and
falls back to copying a fallback slot via FUN_000ada54 before rebuilding through
FUN_000ad976. Caller 0x000A0DB4. Singleton. [I]

#### 0x000AD69A `ident_buffers_clear` — data-accessor
Zeroes the three 100-byte identification buffers at DAT_000ad888 and the three dwords at
PTR_DAT_000ad884. Callers 0x000A0E04, 0x000ACA68. Singleton. [O]

#### 0x000AD6D4 `ident_lru_cache_put` — logic
Maintains the 8-entry cache (PTR_DAT_000ad89c keys, 0x000ad8a0 timestamps): inserts
(param_2, time>>4), returns on hit, evicts the oldest when full. Callers 0x000A9C18 (OBD DTC
reporter), 0x000AA176. Singleton. [I]

#### 0x000AD80C `ident_stamp_capture` — logic
When armed: captures the timestamp>>4 into PTR_DAT_000ad8a4+0x10a4 and increments the aging
byte at +0x10a6 (saturating at 0xfe). Callers 0x000A9C18, 0x000AA176, 0x000AA10C. [O]

#### 0x000AD85C `ident_age_check` — logic
If the free-running time is < 0x51, invalidates the identification cache via FUN_000ad8a8.
Caller 0x000A0E0C. Singleton. [I]

#### 0x000AD8A8 `ident_invalidate` — logic
Marks the stamp word/byte 0xffff/0xff, clears the 8-entry LRU arrays (PTR_DAT_000adb18/1c)
and clears the armed flag at param+4. Callers 0x000AD85C, 0x000ACA68. Singleton. [O]

#### 0x000AD8DA `did_read_dispatch` — dispatch
Routes DID-style reads by code: 0x269/0x26a/0x26b -> FUN_000ad93a, 0x2e2 -> FUN_000ada8c,
0x210 -> FUN_000adb34, 0x20e -> FUN_000adc1e; unknown codes return 0. Caller 0x000A0DF0.
Singleton. [O]

#### 0x000AD93A `did_ptr3_x100` — data-accessor
Returns the pointer to one of the three 100-byte identification records
(PTR_DAT_000adb20/24/28+0x49) and length 100 for codes 0x269/0x26a/0x26b. Caller 0x000AD8DA.
Singleton. [O]

#### 0x000AD976 `ident_block_build` — logic
Builds a 100-byte identification record: walks the 0x46-entry short table at *param_1,
resolves each entry to a string/record via PTR_FUN_000adb30, copies its bytes (or a single 0
when missing) up to the 100/0x62-byte budget, and stores the source time at
PTR_DAT_000adb2c+idx*4. Caller 0x000AD606. Singleton. [I]

#### 0x000ADA54 `ident_record_copy` — data-accessor
Copies the 100-byte record and its timestamp between cache slots param_2 -> param_3.
Caller 0x000AD606. Singleton. [O]

#### 0x000ADA8C `did_2e2_read` — logic
Packs the 0x23-byte DID 0x2e2 payload into DAT_fff8e594: stamp byte + stamp word + the eight
LRU (key,timestamp) pairs. Caller 0x000AD8DA. Singleton. [O]

#### 0x000ADB34 `did_210_read` — logic
Builds the 11-byte DID 0x210 payload: two bytes from a keyed fetch (PTR_FUN_000addf8), a
status nibble derived from bits 0x200/0x100/0x800 of a status word, and eight bytes copied
from the table at PTR_DAT_000ade00 (offsets 0-0x10). Caller 0x000AD8DA. Singleton. [I]

#### 0x000ADC1E `did_20e_read` — logic
222 instrs building the 0x21-byte DID 0x20e payload at DAT_fff8e594: up to four 4-byte
entries selected by status bit 0x400 (DTC-like records), zero-padded to 0x14, then two 3-byte
values, a flag byte assembled from five predicates (bits 0-4), and three trailing words.
Caller 0x000AD8DA. Singleton. [I]

#### 0x000ADE40 `snap_time_capture` — data-accessor
Stores the free-running time (*PTR_PTR_000adf78) into *param_1, latches
*DAT_000adf80 = *PTR_DAT_000adf7c and runs FUN_000adeb8 + FUN_000adec2. Caller 0x000A0E20.
Singleton. [O]

#### 0x000ADE60 `init_pair_call` — stub
FUN_000adec2() followed by the virtual call PTR_FUN_000adf88(PTR_DAT_000adf84). No direct
callers — pointer-table entry. Singleton. [O]

#### 0x000ADE6E `snap_time_ext_capture` — data-accessor
Extended time snapshot: FUN_000adec2, time into *param_1, latch *DAT_000adf80, plus a byte
into +4, a word into +6 and a word into +2 from three indirect getters. Callers 0x000A0E44,
0x000A0E2E. Singleton. [O]

#### 0x000ADEA0 `snap_word_capture` — data-accessor
Stores a word from *PTR_PTR_000adf98 into param+0xa and a byte from *PTR_PTR_000adf9c into
param+0xc. Caller 0x000A0E52. Singleton. [O]

## Unit summary

220 clusters, 274 named functions (reps + members; 54 members beyond the reps, in 9
multi-member clusters). Kind counts by name rows:
- logic: 134
- data-accessor: 104
- stub: 27
- dispatch: 9
- library / irq / entry: 0
- unknown: 0 (no cluster was left unclassifiable; the least-certain families are marked [I]
  instead of guessed)

Confidence markers across the 220 catalog entries: 137 [O], 86 [I] (some entries carry both
an [O] pattern statement and an [I] interpretation). Honest-unknown note: no cluster required
`unknown` — the hardest cases (the 0x000A8CD6-0x000A96DE band/threshold state machine, the
0x000AC5C4-0x000AC82A consistency checks, the 0x000AD072/0x000AD430 monitor tasks with their
fatal-spin paths, and the 0x000AA058 dynamic-definition family) got pattern-descriptive names
with [I] markers rather than fabricated semantics.

Cross-region leads:
- The OBD-mode family (0x000A97CA..0x000A9F72) is entered from r0c_app 0x00148050 and
  0x0008086C — the latter is the shared UDS 0x14/OBD-mode-4 handler listed in spec 15 §2;
  units covering 0x80xxx/0x148050 should cross-check.
- `dtc_*` functions here are called from the 0x0009B000-0x0009F000 fault band (other r08
  units) and call into 0x000C4xxx-0x000C5xxx (r0c_app condition evaluators); units owning
  those ranges should cross-link.
- 0x000AA76E/0x000AA7CC-family table accessors sit on *PTR_DAT_000aa86c / *DAT_000aa870 —
  the 400-entry / 0x38-stride table shared with the DTC engine; spec 13 should be updated
  with these access points.
- Cluster members found in other regions (should keep names in sync): 0x001260AE (r10_app,
  band_thresholds_tick), 0x001084CE / 0x001369C0 (clear_byte_arg4), and twenty
  `ret_deref_arg` copies across r0c_app 0x000BC816-0x000C1EF6.
