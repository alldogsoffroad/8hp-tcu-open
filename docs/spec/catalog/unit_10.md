# Unit 10 (r04_boot)

Region 0x00079C28–0x0007EE8A. Dominant subsystem in the first half: a 16-channel
"dataset block transfer" manager (channels 0–6 wrappers live in the previous unit).
Per channel N: a RAM buffer pointer at 0xFFF8460C+4N (table base 0xFFF845F0), a ROM
start/end pointer pair (0x79F08+4N region) giving the buffer length, state byte
(&DAT_fff845cc)[N] and flag byte (&DAT_fff845dc)[N]. Four wrappers per channel forward
into shared engines 0x0007A0EA / 0x0007A130 / 0x0007A26E / 0x0007A3DC. All wrappers
have callers='-' → they are reached through function-pointer tables (dispatch), not
direct calls. [I]

#### 0x00079C28 `dset_ch7_check` — stub
Forwards (flag) to dset_check_state (0x0007A0EA) with channel index 7 bound. Part of the
16-channel dataset-transfer wrapper family; no callers in edges.csv (table-dispatched).
[O]

#### 0x00079C36 `dset_ch7_close` — stub
Forwards to dset_close_transfer (0x0007A130) with channel 7 bound — finalizes/verifies a
completed transfer for channel 7. [O]

#### 0x00079C42 `dset_ch7_write` — data-accessor
Binds channel 7 constants and calls dset_write_chunk (0x0007A26E): RAM buffer pointer
DAT_fff8460c, maxlen from ROM pair (PTR_DAT_00079f08 - PTR_DAT_00079f0c)&0xffff, id byte
from *PTR_DAT_00079f10. Caller args pass through as src/len/flags/descriptor. [O]

#### 0x00079C86 `dset_ch7_read` — data-accessor
Binds channel 7 and calls dset_read_chunk (0x0007A3DC) with RAM buffer DAT_fff8460c and
the same ROM-derived maxlen; staged read-out of the channel-7 dataset buffer into the
caller's buffer. [O]

#### 0x00079CA6 `dset_ch8_check` — stub
Channel-8 variant of dset_ch7_check (forwards to 0x0007A0EA with index 8). [O]

#### 0x00079CB4 `dset_ch8_close` — stub
Channel-8 variant of dset_ch7_close (forwards to 0x0007A130 with index 8). [O]

#### 0x00079CC0 `dset_ch8_write` — data-accessor
Channel-8 write wrapper → 0x0007A26E; buffer DAT_fff84610, maxlen from
PTR_FUN_00079f14/PTR_DAT_00079f18 pair, id from *PTR_DAT_00079f1c. [O]

#### 0x00079D04 `dset_ch8_read` — data-accessor
Channel-8 read wrapper → 0x0007A3DC with DAT_fff84610 and same maxlen. [O]

#### 0x00079D24 `dset_ch9_check` — stub
Channel-9 variant of dset_ch7_check (→ 0x0007A0EA index 9). [O]

#### 0x00079D32 `dset_ch9_close` — stub
Channel-9 variant of dset_ch7_close (→ 0x0007A130 index 9). [O]

#### 0x00079D3E `dset_ch9_write` — data-accessor
Channel-9 write wrapper → 0x0007A26E; buffer DAT_fff84614, maxlen from
PTR_DAT_00079f20/PTR_DAT_00079f24, id from *PTR_DAT_00079f28. [O]

#### 0x00079D82 `dset_ch9_read` — data-accessor
Channel-9 read wrapper → 0x0007A3DC with DAT_fff84614. [O]

#### 0x00079DA2 `dset_ch10_check` — stub
Channel-10 variant of dset_ch7_check (→ 0x0007A0EA index 10). [O]

#### 0x00079DB0 `dset_ch10_close` — stub
Channel-10 variant of dset_ch7_close (→ 0x0007A130 index 10). [O]

#### 0x00079DBC `dset_ch10_write` — data-accessor
Channel-10 write wrapper → 0x0007A26E; buffer DAT_fff84618, maxlen from
PTR_DAT_00079f2c/PTR_FUN_00079f30, id from *PTR_DAT_00079f34. [O]

#### 0x00079E00 `dset_ch10_read` — data-accessor
Channel-10 read wrapper → 0x0007A3DC with DAT_fff84618. [O]

#### 0x00079E20 `dset_ch11_check` — stub
Channel-11 variant of dset_ch7_check (→ 0x0007A0EA index 0xb). Cluster of 5: this plus
members 0x00079E9E (ch12), 0x00079F70 (ch13), 0x00079FEE (ch14), 0x0007A06C (ch15) —
byte-identical except the bound channel constant. [O]

#### 0x00079E2E `dset_ch11_close` — stub
Channel-11 close wrapper (→ 0x0007A130 index 0xb). Cluster of 5: members 0x00079EAC
(ch12), 0x00079F7E (ch13), 0x00079FFC (ch14), 0x0007A07A (ch15). [O]

#### 0x00079E3A `dset_ch11_write` — data-accessor
Channel-11 write wrapper → 0x0007A26E; buffer DAT_fff8461c, maxlen from
PTR_DAT_00079f38/PTR_DAT_00079f3c, id from *PTR_DAT_00079f40. [O]

#### 0x00079E7E `dset_ch11_read` — data-accessor
Channel-11 read wrapper → 0x0007A3DC with DAT_fff8461c. [O]

#### 0x00079EB8 `dset_ch12_write` — data-accessor
Channel-12 write wrapper → 0x0007A26E; buffer DAT_fff84620, maxlen from
PTR_DAT_00079f44/PTR_DAT_00079f48, id from *PTR_DAT_00079f4c. (Its ch12 check/close
wrappers are cluster members 0x00079E9E/0x00079EAC above.) [O]

#### 0x00079F50 `dset_ch12_read` — data-accessor
Channel-12 read wrapper → 0x0007A3DC with DAT_fff84620, maxlen from
PTR_DAT_0007a1a8/PTR_DAT_0007a1ac. [O]

#### 0x00079F8A `dset_ch13_write` — data-accessor
Channel-13 write wrapper → 0x0007A26E; buffer DAT_fff84624, maxlen from
PTR_DAT_0007a1b0/PTR_DAT_0007a1b4, id from *PTR_DAT_0007a1b8. [O]

#### 0x00079FCE `dset_ch13_read` — data-accessor
Channel-13 read wrapper → 0x0007A3DC with DAT_fff84624. [O]

#### 0x0007A008 `dset_ch14_write` — data-accessor
Channel-14 write wrapper → 0x0007A26E; buffer DAT_fff84628, maxlen from
PTR_DAT_0007a1bc/PTR_DAT_0007a1c0, id from *PTR_DAT_0007a1c4. [O]

#### 0x0007A04C `dset_ch14_read` — data-accessor
Channel-14 read wrapper → 0x0007A3DC with DAT_fff84628. [O]

#### 0x0007A086 `dset_ch15_write` — data-accessor
Channel-15 write wrapper → 0x0007A26E; buffer DAT_fff8462c, maxlen from a ROM pair at
0x0007a1c8/0x0007a1cc, id from *PTR_DAT_0007a1d0. Last of the 16-channel family. [O]

#### 0x0007A0CA `dset_ch15_read` — data-accessor
Channel-15 read wrapper → 0x0007A3DC with DAT_fff8462c. [O]

#### 0x0007A0EA `dset_check_state` — logic
Shared per-channel readiness check: calls indirect status hook (PTR_FUN_0007a1d4) with the
channel index, maps result to 0/1/2/3 and records state byte (&DAT_fff845cc)[N]=1 or 2;
returns 2 when flag set and state ok. Called by all 16 channel check wrappers plus
dset_close_transfer (0x0007A130). [O]

#### 0x0007A130 `dset_close_transfer` — logic
Three-phase close/verify state machine (DAT_fff84a14: 1→2→3): phase 1 re-runs
dset_check_state; phase 2 validates the channel's buffer pointer from table
PTR_PTR_0007a1dc, runs an indirect check (CRC/validity hook PTR_FUN_0007a1e0) setting flag
bit 0 in (&DAT_fff845dc)[N], and accumulates error bits into DAT_fff84a13 (0x20 from
DAT_fff9380c&0xcfff, 2/4 from status hook bits, 0x20 on buffer invalid); phase 3 calls
error reporter PTR_FUN_0007a3c8(DAT_fff84a13,N) under guard DAT_fff84a11 and clears the
state. Returns "busy" while DAT_fff84a14 != 0. Callees 0x0006AAE4/0x0006B4CE. [O]

#### 0x0007A26E `dset_write_chunk` — logic
Staged write engine for the dataset channels: state DAT_fff84a15 (0 idle,1 copying,2
CRC,3 verify). On start snapshots channel length via hook PTR_FUN_0007a3cc, validates the
channel buffer pointer from table PTR_PTR_0007a3d0 (flag bit0 in DAT_fff845dc[N], bit1 if
caller descriptor says data present). Copy loop moves caller bytes (src param_5) into the
channel RAM buffer (param_3) bounded by channel maxlen (param_4) and caller len
(param_6), position kept in DAT_fff84884; on completion CRCs the untouched tail via
thunk_FUN_0007d548 (0x0007D548) and, if flag param_9 set, chains dset_close_transfer
(0x0007A130) for verification. Called by all 16 dset_chN_write wrappers. [O]

#### 0x0007A3DC `dset_read_chunk` — logic
Staged read engine: copies from the channel RAM buffer (param_2) to the caller's buffer
(param_3) in chunks bounded by the ROM maxlen (param_5); per-call counter _DAT_fff84886,
bytes-moved DAT_fff845ee, status 2 in DAT_fff845ec while incomplete, 0/done resets the
session (channel id kept in DAT_fff84a16, restart detection via DAT_fff84a17). On session
start optionally triggers hook pair PTR_FUN_0007a4b4/b8 under guard DAT_fff84a12.
Returns &DAT_fff845ec as status. Called by all 16 dset_chN_read wrappers. [O]

#### 0x0007A520 `ram_hook_call_7a78c` — stub
3-instruction thunk: calls the RAM-installed function pointer at DAT_0007a78c. 35 callers
across the app state machines (0x13xxxx–0x15xxxx) — a central runtime-patched hook.
[O]

#### 0x0007A526 `trace_snap_channel` — logic
Commits a snapshot for trace channel idx 0–2: calls claim/free hooks (PTR_FUN_0007a790),
formats the channel buffer (DAT_0007a794 hook with mode 4/8/0x10 and buffer pointer
DAT_fff84630+4*idx), records the produced word count in DAT_fff938a0+2*idx and reports
error 0x227 via DAT hook if it exceeds the channel limit DAT_fff938a6+2*idx (writes
DAT_fff90998). [O]

#### 0x0007A59A `trace_ch0_snap_once` — logic
One-shot (guard DAT_fff94035): enter hook PTR_FUN_0007a798, init record area
FUN_0007a4c0(0,&DAT_fff92cdc), trace_snap_channel(0,4), exit hook PTR_FUN_0007a79c.
No direct callers — invoked through a pointer table (fault/snapshot path). [O]

#### 0x0007A5CA `trace_ch1_snap_once` — logic
One-shot (guard DAT_fff94036) for trace channel 1: FUN_0007a4c0(1,&DAT_fff92cec),
trace_snap_channel(1,8). Table-dispatched like its siblings. [O]

#### 0x0007A5F8 `trace_ch2_snap_once` — logic
One-shot (guard DAT_fff94037) for trace channel 2: FUN_0007a4c0(2,&DAT_fff92cfc),
trace_snap_channel(2,0x10). [O]

#### 0x0007A680 `trace_snap_all` — logic
Snapshots all three trace channels in one go — trace_snap_channel(0,4),(1,8),(2,0x10).
No direct callers (pointer-dispatched). [O]

#### 0x0007A696 `trace_set_capture_buf` — data-accessor
Setter: for channel arg 1–3 stores the capture buffer pointer at DAT_fff92cd0+4*(idx-1)
and the capacity (param_3&0x3fffffff)>>1 into DAT_fff938ac+2*idx. No direct callers —
exported API used through a table. [O]

#### 0x0007A6BE `trace_set_snap_buf` — data-accessor
Setter: for channel arg 1–3 stores the snapshot buffer pointer at DAT_fff84630+4*(idx-1)
and word limit into DAT_fff938a6+2*idx. Table-exported setter. [O]

#### 0x0007A6E0 `trace_log_event` — logic
Appends an 8-byte record (type byte param_3 + dword param_2) to the capture ring
DAT_fff92cd0[idx] at index DAT_fff9389a+2*idx if below capacity DAT_fff938ac; on overflow
raises error 0x227 via hook PTR_FUN_0007a788 with DAT_fff90998=idx. Real callers
0x000A67A8, 0x000AD034, 0x000A6A04, 0x000BA6B2, 0x00148D84, 0x00154FF8 — event logging
from application logic. [O]

#### 0x0007A746 `trace_mark_slot` — logic
Maps arg 0–7 to one of eight fixed 16-byte slots (DAT_fff92d0c–DAT_fff92d7c), writes
marker 0xfd, the dword arg, trailing 0xff and zeroes — marks/arms a fixed trace slot.
No direct callers (table API). [O]

#### 0x0007A7E0 `get_dset_ratio_word` — data-accessor
3-instruction getter returning DAT_fff938b4 (16-bit result produced by 0x0007A7E8).
Callers in 0x13xxxx-0x14xxxx app code. [O]

#### 0x0007A7E8 `calc_scaled_ratio` — logic
Computes the 16-bit value DAT_fff938b2: seeds from DAT_fff94f9e, evaluates hooks
(PTR_FUN_0007a8e0/e4), then if the configured reference (&DAT_fff905a8)[cfg] is a valid
magnitude range-checks it against two per-mode pointer tables and computes
(float DAT_fff905c0[cfg] / value) * DAT_0007a8fc, clamping into table bounds; sets error
DAT_fff940ae = 1/6/8 for out-of-range/invalid references. Reads DAT_fff800ef==3 check for
DAT_fff940ad. Calls 0x0008FB74/0x0008FB50. Looks like a scaled speed/ratio calculation
feeding DAT_fff938b2. [I]

#### 0x0007A90C `get_time_base` — data-accessor
3-instruction getter returning DAT_fff90628 — the same global used as time base for
timeouts in 0x000792AC (5000-unit window). [I]

#### 0x0007A914 `ram_hook_call_300` — stub
4-instruction wrapper calling RAM function pointer DAT_0007a9a8 with constant 0x300 —
a checkpoint hook shared by ~77 call sites across the app state machines. [O]

#### 0x0007A91C `check_value_valid` — logic
Validity check via hook PTR_FUN_0007a9ac(value, len, size 2-or-4, 0xff): maps hook result
to 1 (accepted, result 0 or 4), 0 (rejected, result 1 or 2), or 2 (not checkable / zero
length / bad size arg). Size selection by param_3<2. [O]

#### 0x0007A960 `check_value_valid_16` — stub
Forwards to check_value_valid (0x0007A91C) with size=2 (16-bit values). Callers
0x00081CFC, 0x001554BC. [O]

#### 0x0007A96E `check_value_valid_32` — stub
Forwards to check_value_valid with size=4 (32-bit values). Same callers as above. [O]

#### 0x0007A97C `ring_push_last4` — logic
Appends the hook value (PTR_FUN_0007a9b0) to a 4-entry ring at DAT_fff92fd4 (index
DAT_fff92d90, wrap resets read pointer DAT_fff92d8c to the array start). Called by the
poll task 0x0007AB48 — keeps the last 4 readings. [O]

#### 0x0007A9B4 `build_tcu_id_string` — logic
One-time (guard DAT_fff94043) builder of the identification string at DAT_fff92d94:
"TCU-<hi>-<lo>.<lo> <DAT_fff84739><hi>.<lo>" assembled from version bytes at
0xFFF8470C–0xFFF8473C ('T','C','U','-' literals visible), tail zeroed (17 bytes).
Lazily invoked by the ID accessors below. [O]

#### 0x0007AA86 `id_table_get_dword` — data-accessor
Returns the dword field of ID record param_1 (0–0x10, stride-8 table at PTR_LAB_0007ab38)
after ensuring the ID string is built. Called by 0x000E32FE (diagnostic read side).
[O]

#### 0x0007AAB0 `id_table_get_word` — data-accessor
Returns the word field (offset +4) of the same 17-entry stride-8 ID table; also called
from 0x000E32FE. [O]

#### 0x0007AADA `map_hook_state_code` — logic
Maps hook result (PTR_FUN_0007ab40) 0/1/2/other to 0xFF/0x00/0x43('C')/0x55('U'); calls
0x0007F70A. Likely a comms/control-unit status code translation. [I]

#### 0x0007AB08 `lookup_id_1001_1011` — data-accessor
Lookup for index 0x3E9–0x3F3 (1001–1011 decimal) into stride-8 table PTR_PTR_0007ab44:
returns the dword entry and stores the trailing byte via out-param; else returns 0 with
*param_2=0. Caller 0x00134A28. Smells like a DID/parameter-range resolver. [I]

#### 0x0007AB48 `task_poll_group_a` — dispatch
Cyclic task, bracketed by trace-enter/leave hooks PTR_FUN_0007abf0(0xa67d18): runs a fixed
sequence of ~14 subsystem update hooks (PTR_FUN_0007abf4..ac2c) plus direct calls
0x0007C16C, 0x0007A97C, 0x0007BEE0, 0x0007C29E, 0x0008305C, 0x00072AE0, 0x0007A7E8,
0x0007C618, 0x0007C3AA, 0x0007B150, 0x0007C100, 0x0007BE60, 0x00082F84 (0x0009A428/
0x0009A3FC enter/exit). No direct callers — scheduled from a task table. [O]

#### 0x0007AB9C `task_poll_group_b` — dispatch
Smaller cyclic task, trace frame 0xa67d19: three hooks (PTR_FUN_0007ac30/ac34,
DAT_0007ac38, PTR_FUN_0007ac2c) plus direct 0x0007B6D8, 0x0006F47E, 0x00072AE0 and
0x0007ACF2. Table-scheduled sibling of 0x0007AB48. [O]

#### 0x0007ABC8 `task_poll_group_c` — dispatch
Third cyclic task, trace frame 0xa67d1a: hooks PTR_FUN_0007ac3c/ac40/ac2c plus direct
0x0007AC44 (fault-flag consolidation). Table-scheduled. [O]

#### 0x0007AC44 `dtc_commit_flags_31` — logic
Consolidates the fault flag bytes: copies new-event bits DAT_fff94045 into latch
DAT_fff940b4 (with clearing), mirrors DAT_fff939ce/d0 → DAT_fff938b6/b8, and if any latched
new bits remain calls event hooks PTR_FUN_0007ae7c (bit index lookup in PTR_LAB_0007ae78,
7 entries) and PTR_FUN_0007ae84(0x31,...) — event id 0x31 — plus fast-path
PTR_FUN_0007ae80(0x31) when DAT_fff940b4&DAT_fff94046. Also promotes session state
DAT_fff940b0 0x10→1. Runs from task_poll_group_c. [I]

#### 0x0007ACF2 `dtc_eval_state_watch` — logic
State watch feeding the fault evaluator: reads two hooks into DAT_fff940b2/DAT_fff940b1,
shifts a bit history (DAT_fff84a44 != 0) into DAT_fff938ba, counts occurrences
(DAT_fff94978) on state 2, calls verdict hook PTR_FUN_0007ae90 with 0/1/2 (1 when a
per-mode mask from PTR_PTR_0007ae94 misses the history word), and resets DAT_fff940b0 when
hook state DAT_fff940af changes (report via DAT_0007aea0). Runs from task_poll_group_b.
[I]

#### 0x0007ADD4 `dtc_latch_flags_9d8` — logic
Records a fault condition via hook PTR_FUN_0007aea8(0x9d8,...) and latches bits into the
fault event bytes: mask 0x0200 → bit 1, mask 0xFDFB → bit 0, with "confirmed"
(DAT_fff94044) vs "pending" (DAT_fff94045) and immediate-report (DAT_fff94046) splits.
Caller 0x00085740. [I]

#### 0x0007AEAC `dtc_latch_flag4_9f1` — logic
Same latch pattern for bit 4: hook PTR_FUN_0007afdc(0x9f1,...); confirmed when
param_1&param_2 overlap, pending when param_1 alone. Caller 0x0008C34C. [I]

#### 0x0007AEE8 `dtc_report_evt_1838` — logic
If arg & 0xE230 nonzero, calls hook PTR_FUN_0007afe0(&LAB_00001838, 0x10, 0) — reports a
fault event keyed by ROM parameter block at 0x00001838. Caller 0x00087732. [I]

#### 0x0007AF00 `dtc_latch_flags_1838` — logic
Latches two more groups via hook PTR_FUN_0007afdc(&LAB_00001838,...): mask 0xEAFF → bit 3
(immediate-report submask 0x89), mask 0x1500 → bit 4, same confirmed/pending split into
DAT_fff94044/45. Caller 0x000877C0. [I]

#### 0x0007AF86 `dtc_report_evt_17d2` — logic
If arg & 0xFFFC nonzero, reports via hook PTR_FUN_0007afe0(&LAB_000017d2_2, 0x10, 0) —
event keyed by ROM block at 0x000017D2. Caller 0x00086F5C. [I]

#### 0x0007AF9C `dtc_latch_flag5_17d2` — logic
Latches bit 5 (mask overlap → confirmed DAT_fff94044, pending DAT_fff94045) after
hook PTR_FUN_0007afdc(&LAB_000017d2_2,...). Caller 0x00072B48. [I]

#### 0x0007AFE4 `get_status_fff847f4` — data-accessor
3-instruction getter for status byte DAT_fff847f4 (adjacent to the 4-instance status
array at 0xFFF847F8 maintained by 0x0007B07A). Caller 0x00134FA4. [O]

#### 0x0007AFEE `get_status_fff847f5` — data-accessor
Getter for status byte DAT_fff847f5. Caller 0x00134FA4. [O]

#### 0x0007AFF8 `get_status_fff847f6` — data-accessor
Getter for status byte DAT_fff847f6; callers 0x00144108, 0x00147D28, 0x00134FA4,
0x0014FC5C, 0x00133C4E. [O]

#### 0x0007B002 `get_status_fff847f7` — data-accessor
Getter for status byte DAT_fff847f7; same caller set as 0x0007AFF8. [O]

#### 0x0007B00C `get_status_fff84802` — data-accessor
Getter for status byte DAT_fff84802; callers 0x0014FC5C, 0x00133C4E. [O]

#### 0x0007B016 `get_status_fff84803` — data-accessor
Getter for status byte DAT_fff84803; callers 0x0014FC5C, 0x00133C4E. [O]

#### 0x0007B020 `get_status_fff847fa` — data-accessor
Getter for DAT_fff847fa — byte index 2 of the 4-instance status array at 0xFFF847F8.
Callers 0x00147D28, 0x0014FC5C, 0x00133C4E. [O]

#### 0x0007B02A `get_status_fff847fb` — data-accessor
Getter for DAT_fff847fb — byte index 3 of that status array. Same callers. [O]

#### 0x0007B034 `get_status_fff847fe` — data-accessor
Getter for DAT_fff847fe — byte index 2 of the inverted-mirror array at 0xFFF847FC.
Callers 0x0014FC5C, 0x00133C4E. [O]

#### 0x0007B03E `get_status_fff847ff` — data-accessor
Getter for DAT_fff847ff — mirror byte index 3. Same callers. [O]

#### 0x0007B048 `get_status_fff92fe6` — data-accessor
Getter for DAT_fff92fe6 (change-class bytes written next to DAT_fff92fe4[] by
0x0007B07A). Callers 0x0014FC5C, 0x00133C4E. [O]

#### 0x0007B052 `get_status_fff92fe7` — data-accessor
Getter for DAT_fff92fe7. Same callers. [O]

#### 0x0007B05C `map_status_bits` — library
Pure bit remap: arg bits 0x08→0, 0x04→1, 0x10→2, 0x20 or 0x02→3, packed into a byte.
Helper of the 4-instance status updater. [O]

#### 0x0007B07A `update_status_word` — logic
Per-instance (arg3 < 4) status update: reports via hook PTR_FUN_0007b174(0x992,...),
merges the masked new bits into DAT_fff92fe8+2*i, recomputes class bytes
(&DAT_fff92fe4)[i] via map_status_bits, and maintains both the live array
(&DAT_fff847f8)[i] and the changed/inverted mirror (&DAT_fff847fc)[i] with change
detection. Caller 0x00088EF0. [I]

#### 0x0007B12C `dtc_report_cond_992` — logic
Reports via hook PTR_FUN_0007b178(0x992, 0x10, uVar1): always 1 when arg2 < 3, else 0 only
if arg1 & 0xFFC1 nonzero. Part of the status/fault report family (same 0x992 id as
0x0007B07A). Caller 0x00088EB0. [I]

#### 0x0007B150 `copy_status_src_arrays` — logic
Loop of 4: copies DAT_fff94f88+i → DAT_fff847f4+i and DAT_fff94e00+i → DAT_fff84800+i —
refreshes the getter-visible status bytes from their working copies. Runs in
task_poll_group_a. [O]

#### 0x0007B17C `map_fault_bits_a` — library
Maps a family-A fault word to a priority nibble: bits 0x1C58→0, 0x1090→4, 0x24→2,
0x100→0x20, 0x200→0x40, default 0x10 when word nonzero but unmapped. Helper of
0x0007B222. [O]

#### 0x0007B1BA `map_fault_bits_b` — library
Family-B variant: 0x808→0, 0x10→4, 0x04→2, 0x20→5, default 0x10. Helper of 0x0007B358.
[O]

#### 0x0007B1E6 `get_dat_fff938c6` — data-accessor
3-instruction getter for word DAT_fff938c6. Caller 0x000A7C9C. [O]

#### 0x0007B1EE `get_dat_fff938c8` — data-accessor
Getter for word DAT_fff938c8. Caller 0x0014FC5C. [O]

#### 0x0007B1F6 `get_dat_fff940b9` — data-accessor
Getter for byte DAT_fff940b9. Caller 0x000A7C9C. [O]

#### 0x0007B1FE `set_dat_fff94fea` — data-accessor
3-instruction setter writing DAT_fff94fea — one of the override bytes checked by
0x0007B358's special case. Caller 0x00141294/0x00141300. [O]

#### 0x0007B206 `set_dat_fff94e62` — data-accessor
Setter writing DAT_fff94e62 (paired override byte). Caller 0x00141300. [O]

#### 0x0007B20E `get_inval_state_8488c` — data-accessor
Getter for DAT_fff8488c — family-C per-object invalid-state array written by 0x0007B4EC
(values 0-4, 1 for 'U'). Callers spread over app logic. [O]

#### 0x0007B218 `get_qual_code_8488e` — data-accessor
Getter for DAT_fff8488e — family-C per-object qualifier codes (0xFF/0xFE/0xFD/0xFB/0xFC)
written by 0x0007B4EC. Caller 0x0014FC5C. [O]

#### 0x0007B222 `eval_obj_status_a` — logic
Per-object evaluator for the 10-object family A: descriptor from PTR_PTR_0007b4c4
(stride 0x10: status byte, class byte, word ptr, two lamp ids). Writes value
DAT_fff93104+2i, computes class from current faults (DAT_fff93018+2i) vs confirmed
(DAT_fff93004+2i) via map_fault_bits_a, merges class into the status byte, stores
(&DAT_fff938ec)[i], and drives one/two lamp outputs through hooks
PTR_FUN_0007b4d4/PTR_FUN_0007b4d0 with a 12-entry lookup table PTR_LAB_00045c52_2.
[I]

#### 0x0007B358 `eval_obj_status_b` — logic
Family-B evaluator (5 objects, table PTR_PTR_0007b4d8): value from DAT_fff90874+2i
(index 0 via hook PTR_FUN_0007b4e0 when marker *PTR_DAT_0007b4dc == 0x6AF5), class from
DAT_fff93900/DAT_fff938f6 via map_fault_bits_b, class stored in DAT_fff940d4[i], single
pair of lamp hook calls; override hooks 0x0007b4e4 can force class 0. [I]

#### 0x0007B4EC `eval_obj_status_c` — logic
Family-C evaluator (2 objects, lamps 0x1F/0x20): merges DAT_fff9302c/93030 fault words
(with global OR-bits from DAT_fff94119/9411a masks), class into DAT_fff9390c[i], lamp
drive via hooks 0x7b6c0/c4 with 8-entry table PTR_LAB_0007b6b8; then decodes two
char-coded hook results (PTR_FUN_0007b6c8/c4) — 'U'→1, '"'/4'→2, '8'→4, else 3 — into
invalid-state array DAT_fff8488c and 0xFB-0xFF qualifier codes into DAT_fff8488e. [I]

#### 0x0007B686 `latch_word_pair_9390a` — logic
Reads two hooks into DAT_fff9390a/0b, copies to DAT_fff94fca/b and stores inverted copies
in DAT_fff94e42/43. Runs from both the sweep (0x0007B9B8) and 0x0007B6D8. [O]

#### 0x0007B6D8 `meas_acquire_words` — logic
Acquisition pipeline: reads 32 indirect channel hooks (PTR_FUN_0007b91c…0x7b990) into the
16-bit array DAT_fff84890–0xFFF848CA with inverted mirror at 0xFFF848AE+; runs a
scaling/filter hook (PTR_FUN_0007b99c) on the configured channel (*PTR_DAT_0007b994 < 10)
when its value ≥ 10000; honors override hooks (0x7b9a0 → force 0/0xFFFF) and magic marker
*PTR_DAT_0007b9a4 == 0x5AA5; snapshots 10 words into DAT_fff94ee8/DAT_fff94d60 and 5 words
into DAT_fff94faa/DAT_fff94e22, then chains meas_publish_10ch (0x0007B896) and
latch_word_pair_9390a. Runs from task_poll_group_b. Analog-input/measurement acquisition
— cf. spec 04/10. [I]

#### 0x0007B896 `meas_publish_10ch` — logic
Publishes 10 measurement channels via hook PTR_FUN_0007b9b4(i, value): value select by
per-channel state DAT_fff938e2+i (0 → 0, 1 → 0xFFFF invalid, 2 → word from 0xFFF92FF0);
also propagates session state via 0x7b9ac (DAT_fff940b5) and change detection
(DAT_fff84a18 vs DAT_fff94ff0) through hook 0x7b9b0. Matches the spec-10 measurement
channel model. [I]

#### 0x0007B9B8 `eval_status_sweep` — dispatch
Runs the full status/telltale evaluation once: eval_obj_status_a for objects 0–9,
eval_obj_status_b for 0–4, latch_word_pair_9390a, eval_obj_status_c for 0–1, then
0x0007BBE0. Called from task_poll_group_c (0x0007ABC8). [O]

#### 0x0007BA06 `dtc_report_cond_9e3` — logic
Report hook PTR_FUN_0007bbd8(0x9E3, 0x10, uVar1): 0 only when arg2 == 9 and
arg1 & 0xC000 (else 1). Caller 0x000842B0. [I]

#### 0x0007BA2A `dtc_set_faults_a` — logic
Sets family-A fault words: hook PTR_FUN_0007bbdc(0x9E3, ch, cur, new) then stores current
faults into DAT_fff93018+2*ch (mask 0xDFFF) and merges the latched confirmations into
DAT_fff93004+2*ch — the inputs consumed by eval_obj_status_a. Caller 0x00089F5C. [O]

#### 0x0007BA76 `plaus_check_value_a` — logic
Returns 1 (ok) when object i has no confirmed faults (DAT_fff93004+2i == 0) or its raw
word DAT_fff84890[i] is nonzero, else 2. Callers 0x00084392, 0x00083DB0. [I]

#### 0x0007BA96 `dtc_report_cond_9dd` — logic
Report hook (0x9DD, 0x10, uVar1): 0 only when arg2 == 4 and arg1 & 0xF7C3, else 1.
Caller 0x00084D7A. [I]

#### 0x0007BABA `dtc_set_faults_b` — logic
Family-B fault-word setter: hook (0x9DD, ch, cur, new), stores DAT_fff93900+2*ch and
merges DAT_fff938f6+2*ch — inputs to eval_obj_status_b. Caller 0x0008B284. [O]

#### 0x0007BB00 `plaus_check_value_b` — logic
Family-B plausibility: ok (1) when DAT_fff938f6+2i == 0 or raw word
DAT_fff84890[i+10] nonzero, else 2. Callers 0x00084DA0, 0x000849D4. [I]

#### 0x0007BB28 `dtc_report_cond_9db` — logic
Report hook (0x9DB, 0x10, uVar1): 1 when arg2 >= 3; when arg2 < 3 only reports 0 if
arg1 & 0xFE10. Caller 0x00085D50. [I]

#### 0x0007BB4E `dtc_set_faults_c` — logic
Family-C fault-word setter: hook PTR_FUN_0007bbdc(0x9DB, ch, cur, new), then merges into
DAT_fff9302c+2*ch with confirm latch DAT_fff93030+2*ch; first-write semantics armed by
(&DAT_fff938bc)[ch] (cleared again by eval_obj_status_c). Feeds 0x0007B4EC. Callers
0x00085FC4, 0x00085ED4. [O]

#### 0x0007BBB4 `plaus_check_value_c` — logic
Family-C plausibility: ok (1) when DAT_fff9302c+2i == 0 or raw byte DAT_fff9390a+i
nonzero, else 2. Callers 0x0008C3BC, 0x00085828. [I]

#### 0x0007BBE0 `eval_interlock_matrix` — logic
Cross-object constraint sweep over 2 groups × (10 family-A + 5 family-B) objects: loads
mode words from ROM DAT_00045d10, reads per-group enable bits (PTR_PTR_0007be30), and for
modes 2/3 queries each object's state (hook PTR_FUN_0007be34 on stride-0x10 descriptor
ids at +0xC/+0xD); when a state matches (2, or 3/6) AND the config masks
(PTR_PTR_0007be40/be48/be4c) select it, sets propagated fault bits in DAT_fff938be /
DAT_fff938c0 and pushes the new state via hook PTR_FUN_0007be44. Runs from
eval_status_sweep. [I]

#### 0x0007BE50 `get_flag_fff940de` — data-accessor
Getter for range flag DAT_fff940de (set by eval_range_classify_a). Caller 0x0014FC5C.
[O]

#### 0x0007BE58 `get_flag_fff940db` — data-accessor
Getter for range flag DAT_fff940db. Caller 0x0014FC5C. [O]

#### 0x0007BE60 `eval_range_classify_a` — logic
Two-channel range classifier run by task_poll_group_a: looks up value
DAT_fff93910/93912 from table DAT_fff9018c indexed by config bytes *DAT_0007bf20/[1],
compares against ROM thresholds (PTR_DAT_0007bf24/bf28/bf2c/bf30) and sets the
below/within/above flags DAT_fff940de/df/db/dc/dd. Looks like an operating-point window
classification (e.g. temperature). [I]

#### 0x0007BEE0 `eval_range_classify_b` — logic
Single-channel variant in the same poll group: DAT_fff9390e from DAT_fff9018c[*DAT_0007bf34],
thresholds PTR_DAT_0007bf38/bf3c, flags DAT_fff940d9/da. [I]

#### 0x0007BF40 `get_code_fff940e6` — data-accessor
Getter for status code DAT_fff940e6 (published by 0x0007C100 from DAT_fff94102 or 0xFF).
Many callers in the 0x13xxxx-0x15xxxx app region. [O]

#### 0x0007BF48 `get_class_fff940e8` — data-accessor
Getter for class byte DAT_fff940e8 (family of 0x0007BF8C). [O]

#### 0x0007BF50 `get_code_fff940e0` — data-accessor
Getter for status code DAT_fff940e0 (from DAT_fff940fe via 0x0007C100). [O]

#### 0x0007BF58 `get_class_fff940e2` — data-accessor
Getter for class byte DAT_fff940e2 (family of 0x0007C01E). [O]

#### 0x0007BF60 `get_code_fff940e3` — data-accessor
Getter for status code DAT_fff940e3 (from DAT_fff94101 via 0x0007C100). [O]

#### 0x0007BF68 `get_class_fff940e5` — data-accessor
Getter for class byte DAT_fff940e5 (family of 0x0007C0B6). [O]

#### 0x0007BF70 `map_mask_bits_e0` — library
Remap helper: 0x08→0, 0x04→1, 0x10→2, 0x1E0→0x80. Used by 0x0007BF8C. [O]

#### 0x0007BF8C `update_status_word_988` — logic
Status-word updater (hook PTR_FUN_0007c15c id 0x988): merges DAT_fff93916 masked bits and
maintains class byte DAT_fff940e7 plus confirmed-class byte DAT_fff940e8 via
map_mask_bits_e0 — same pattern as 0x0007B07A. Caller 0x000887D8. [I]

#### 0x0007C008 `map_mask_bits_e1` — library
Remap helper: 0x20→0x20, 0x40→0x40, 0x02→0x08. Used by 0x0007C01E. [O]

#### 0x0007C01E `update_status_word_98a` — logic
Status-word updater (hook id 0x98A): merges DAT_fff93916, class bytes DAT_fff940e1/e2 via
map_mask_bits_e1. Caller 0x00088BD0. [I]

#### 0x0007C09C `map_mask_bits_e4` — library
Remap helper: 0x20→0x20, 0x40→0x40, 0x100→0x80. Used by 0x0007C0B6. [O]

#### 0x0007C0B6 `update_status_word_98b` — logic
Status-word updater (hook id 0x98B): class bytes DAT_fff940e4 (current) and DAT_fff940e5
(confirmed overlap) via map_mask_bits_e4, no persistent status word. Caller 0x00088DA8.
[I]

#### 0x0007C100 `publish_status_codes` — logic
Publish step in task_poll_group_a: maps class bytes to code bytes — DAT_fff940e8==0 →
DAT_fff940e6 = DAT_fff94102 else 0xFF; DAT_fff940e2==0 → DAT_fff940e0 = DAT_fff940fe;
DAT_fff940e5==0 → DAT_fff940e3 = DAT_fff94101 (default 0xFF). These codes feed the
0x0007BF40–0x0007BF68 getters. [O]

#### 0x0007C164 `get_minutes_fff93034` — data-accessor
Getter for word DAT_fff93034 — the minute counter maintained by 0x0007C16C. Caller
0x000B912C. [O]

#### 0x0007C16C `accum_run_time` — logic
Time accumulation on the 10 ms base DAT_fff90628 (delta vs DAT_fff92db4, capped at 29 to
survive jumps): while state hook PTR_FUN_0007c1e4 != 0 accumulates into DAT_fff93040 with
360000-tick (1 h) rollover incrementing DAT_fff93038; when state == 2 also accumulates
DAT_fff9303c with 60000-tick (1 min) rollover incrementing DAT_fff93034. Operating-time /
run-hour meter. Runs from task_poll_group_a; calls 0x00068EA0. [O]

#### 0x0007C1E8 `get_dat_fff93044` — data-accessor
Getter for word DAT_fff93044. Callers 0x00133946, 0x0014FC5C. [O]

#### 0x0007C1F0 `get_dat_fff940ec` — data-accessor
Getter for byte DAT_fff940ec. Same callers. [O]

#### 0x0007C1F8 `get_dat_fff940eb` — data-accessor
Getter for byte DAT_fff940eb. Same callers. [O]

#### 0x0007C200 `get_class_fff940ea` — data-accessor
Getter for confirmed-class byte DAT_fff940ea (family of 0x0007C220). Callers
0x00133946, 0x0014FC5C, 0x0007C452. [O]

#### 0x0007C208 `get_class_fff940e9` — data-accessor
Getter for class byte DAT_fff940e9. Same caller set. [O]

#### 0x0007C210 `map_mask_bits_e9` — library
Remap helper: 0x08→5, 0x04→2. Cluster of 2 with 0x0007C31C (byte-identical copy used by
0x0007C32C). [O]

#### 0x0007C220 `update_status_word_97f` — logic
Status-word updater (hook PTR_FUN_0007c2e0 id 0x97F): merges masked bits of
DAT_fff93918, maintains class bytes DAT_fff940e9/ea via map_mask_bits_e9. Caller
0x00089330. [I]

#### 0x0007C288 `dtc_report_cond_97f` — logic
Report hook DAT_0007c2e4(0x97F, 0x10, 0) when arg & 0xFFD3. Caller 0x000893CA. [I]

#### 0x0007C29E `poll_scaled_value_a` — logic
Poll step (task_poll_group_a): reads two hooks on config ptr *PTR_DAT_0007c2e8 into
DAT_fff93044/DAT_fff940ec and scales table word DAT_fff9084a+2*cfg (clamp 0xFF) into
DAT_fff940eb. Calls 0x00090DF4/0x00090E04. [O]

#### 0x0007C2F4 `get_dat_fff93048` — data-accessor
Getter for word DAT_fff93048. Callers 0x00133740, 0x0014FC5C. [O]

#### 0x0007C2FC `get_class_fff940f0` — data-accessor
Getter for byte DAT_fff940f0. Callers 0x00133946, 0x0014FC5C. [O]

#### 0x0007C304 `get_code_fff940ef` — data-accessor
Getter for scaled code DAT_fff940ef. Same callers. [O]

#### 0x0007C30C `get_class_fff940ee` — data-accessor
Getter for confirmed-class byte DAT_fff940ee (family of 0x0007C32C). Also used by
0x0007C452. [O]

#### 0x0007C314 `get_class_fff940ed` — data-accessor
Getter for class byte DAT_fff940ed. Same callers. [O]

#### 0x0007C32C `update_status_word_97e` — logic
Status-word updater (hook PTR_FUN_0007c3ec id 0x97E): merges DAT_fff9391a, class bytes
DAT_fff940ed/ee via the cluster-duplicate mapper 0x0007C31C. Caller 0x0008941C. [I]

#### 0x0007C394 `dtc_report_cond_97e` — logic
Report hook DAT_0007c3f0(0x97E, 0x10, 0) when arg & 0xFF93. Caller 0x000894B6. [I]

#### 0x0007C3AA `poll_scaled_value_b` — logic
Second poll step, same shape as 0x0007C29E with config PTR_DAT_0007c3f4: fills
DAT_fff93048/DAT_fff940f0 and clamps DAT_fff9084a+2*cfg into DAT_fff940ef. Runs from
task_poll_group_a. [O]

#### 0x0007C400 `get_dat_fff93922` — data-accessor
Getter for word DAT_fff93922 (copied from DAT_fff94ed0 by 0x0007C618). Callers
0x00144108, 0x000A7C9C, 0x00134D2C, 0x000B912C. [O]

#### 0x0007C408 `get_dat_fff93926` — data-accessor
Getter for word DAT_fff93926 (mirror of DAT_fff93942). Caller 0x00134CD8. [O]

#### 0x0007C410 `get_dat_fff93920` — data-accessor
Getter for word DAT_fff93920 (from DAT_fff94f86) — 13 callers across app code. [O]

#### 0x0007C418 `get_flag_fff84a1f` — data-accessor
Getter for status byte DAT_fff84a1f (class byte of the 0x21-event object family).
7 callers. [O]

#### 0x0007C422 `get_flag_fff84a20` — data-accessor
Getter for confirmed-mirror byte DAT_fff84a20. 9 callers. [O]

#### 0x0007C42C `map_mask_bits_1f` — library
Remap helper: 0x08→0x20, 0x14→0x40, 0x01→4, default 8 when word nonzero. Used by
0x0007C618. [O]

#### 0x0007C452 `eval_precond_latch` — logic
Precondition latch: ORs bit-2 results of four hooks (PTR_FUN_0007c6b4…c6c0) into
DAT_fff940f2/f3, detects changes vs DAT_fff940f4, on transition fetches an event byte
from config table PTR_PTR_0007c6c4 into DAT_fff9480a, and latches DAT_fff940f1 only when
both bits set (state 3). Helper of 0x0007C618. [I]

#### 0x0007C618 `eval_obj_status_d` — logic
Evaluator for the 0x21-event object (task_poll_group_a): refreshes working words
DAT_fff93922/24/20 from DAT_fff94ed0/ed2/4f86, range lookup DAT_fff93928 from
DAT_fff9018c, runs eval_precond_latch, then merges fault words DAT_fff9391e/1c (with
forced bit 0 from the precond flags) into class bytes DAT_fff84a1f/20 and
DAT_fff940f5/f6 via map_mask_bits_1f; event/lamp drive hook PTR_FUN_0007c6d0(0x21, ...)
with 5-entry table PTR_LAB_0007c6c8. [I]

#### 0x0007C678 `dtc_set_faults_d` — logic
Fault-word setter (hook PTR_FUN_0007c6dc id 0x9CE): latch DAT_fff9391c, current
DAT_fff9391e — inputs to eval_obj_status_d. Caller 0x0008C810. [O]

#### 0x0007C6E0 `check_state_fff94fe4` — logic
Returns 1 when DAT_fff94fe4 ∈ {0x0A, 0xFA, 0x05, 0xF5} else 0 — a coded state test
(lever/gear-range style codes) used by 9 callers. [I]

#### 0x0007C708 `map_state_fff94e5c` — logic
Returns 0xFE when mirror state DAT_fff94e5c ∈ {0xF5, 0x05, 0xFA, 0x0A} else 0xFF —
code translation for the same coded states. 4 callers. [I]

#### 0x0007C736 `cal_check_block_access` — logic
Calibration-area access check (caller 0x000829C4): if protection bit DAT_fff94711&1 clear
or config byte == 0xAA, delegates to hook PTR_FUN_0007c7dc; otherwise validates the
selector value (*PTR_DAT_0007c7d0 ≤ 5) and calls PTR_FUN_0007c7d8 with the calibration
constants — ranges 0x180000 (calibration block, spec 06) / 0x600000+0x80000 variants and
&PTR_DAT_00180280 (0x180280 header area). Callees 0x00068778/0x000688F2/0x000689B4. [I]

#### 0x0007C7BC `bool_of_hook_7c7e0` — stub
6-instruction wrapper: returns hook PTR_FUN_0007c7e0() != 0 (resolved callee 0x000673E4).
No direct callers — table API. [O]

#### 0x0007C7E4 `tick_delay_timers` — logic
Under trace frame 0xE119A: walks two arrays (short and char) described by descriptor
PTR_PTR_0007c878 (start/end pairs at +0/+4 and +8/+0xC) and decrements every nonzero
element — a bank of countdown timers ticked periodically. Callees 0x00072AE0/
0x0009A428/0x0009A3FC (enter/exit). [O]

#### 0x0007CA24 `record_event_history` — logic
Shared hub called by 23 of the status/fault setters above: computes key = id*100+subcode;
when confirmed bits (arg3&arg4) finds or allocates one of 20 history slots
(DAT_fff948b8), ORs the bits into DAT_fff94908[slot] and increments occurrence count
(&DAT_fff94890)[slot]; then stores first-free-slot/4 into DAT_fff940f7 (0x14 when full).
[O]

#### 0x0007CB2A `notify_lib_group` — stub
Fan-out calling four ROM pointer-table hooks that resolve to 0x00068B58, 0x00068C04,
0x00068D52, 0x0006A7F6 (library/OS-side subscribers). No direct callers — itself
table-invoked. Cluster of 3 with 0x0007CBA8 (subscribers 0x000838B0/0x00083A1C) and
0x0007CC1E (subscribers 0x00083DB0/0x0008642C) — identical pattern, different subscriber
lists. [O]

#### 0x0007CB9C `notify_list_45ec0` — stub
Two-call fan-out: hook PTR_FUN_0007cbc4 then 0x00083A18(&LAB_00045ec0, 2) — notifies a
subscriber with ROM descriptor block 0x00045EC0. [O]

#### 0x0007CBDC `notify_84_group` — stub
Eight-hook fan-out resolving to 0x000842B0, 0x00084D7A, 0x00085678, 0x00085810,
0x00085D50, 0x00086ACE, 0x00086AFE, 0x00086B3C — the dtc_report_cond_* caller family;
this is the publish side that fans an event out to the fault reporters. [O]

#### 0x0007CC06 `register_list_3da54` — logic
Calls 0x00085158(&LAB_0003da54), then loops five sub-objects calling
0x0008515c(0x0003DA5E+i, 2) — registers/notifies a 5-element list rooted at ROM
descriptor 0x0003DA54. Resolved target 0x00086AD4. [O]

#### 0x0007CC32 `notify_86_group` — stub
Eight-hook fan-out resolving to 0x000849D4, 0x00085788, 0x0008646C, 0x00086A80,
0x00086AF8, 0x00086B0C and 0x00083DB0 — sibling of 0x0007CBDC with a different
subscriber set. [O]

#### 0x0007CCF0 `fwd_arg_code61` — stub
8-instruction forwarder: calls PTR_FUN_0007cef0(arg, 0x61) (resolved 0x00086B58).
Cluster of 3 with 0x0007CD10 (code 0x20) and 0x0007CD20 (code 0x61 again) — argument
 adapters differing only in the code constant. [O]

#### 0x0007CD00 `fwd_arg_code00` — stub
Same forwarder shape with code 0 (PTR_FUN_0007cef0(arg, 0) → 0x00086B58). [O]

#### 0x0007CD30 `query_desc_45db6` — stub
7-instruction adapter: PTR_FUN_0007cef8(PTR_LAB_00045db6_2) (resolved 0x00086B6C) —
descriptor-keyed query with ROM block 0x00045DB6. [O]

#### 0x0007CD3E `query_desc_7cefc` — stub
Same adapter with descriptor PTR_LAB_0007cefc. [O]

#### 0x0007CD4C `query_desc_7cf00` — stub
Same adapter with descriptor PTR_LAB_0007cf00. [O]

#### 0x0007CD5A `query_desc_7cf04` — stub
Same adapter with descriptor PTR_LAB_0007cf04. [O]

#### 0x0007CD68 `send_rec_type4` — stub
Builds a 0x18-byte record descriptor on the stack (value dword/word, flags, type code 4,
table PTR_PTR_0007cf08, tail param) and passes it to PTR_FUN_0007cf0c (resolved
0x00086C68) — measurement/record submit adapter. [O]

#### 0x0007CDA0 `send_rec_type1` — stub
Same adapter with type code 1 and table PTR_PTR_0007cf10. [O]

#### 0x0007CDD8 `send_rec_type2a` — stub
Same adapter with type code 2 and table PTR_PTR_0007cf14. [O]

#### 0x0007CE10 `send_rec_type2b` — stub
Same adapter with type code 2 and table PTR_PTR_0007cf18. [O]

#### 0x0007CE48 `write_rec_type4` — stub
Write-path adapter: descriptor with type code 4, table PTR_PTR_0007cf08 and state
pointer &DAT_fff9404a passed to PTR_FUN_0007cf1c (resolved 0x00086D70). [O]

#### 0x0007CE72 `write_rec_type1` — stub
Write-path adapter with type 1, table PTR_PTR_0007cf10, state &DAT_fff9404d. [O]

#### 0x0007CE9C `write_rec_type2a` — stub
Write-path adapter with type 2, table PTR_PTR_0007cf14, state &DAT_fff94050. [O]

#### 0x0007CEC6 `write_rec_type2b` — stub
Write-path adapter with type 2, table PTR_PTR_0007cf18, state &DAT_fff94053. [O]

#### 0x0007CF3A `send_rec_buf200` — stub
Submit adapter variant with type code 1, table PTR_PTR_0007cfb0 plus an explicit 200-byte
buffer &DAT_fff84640 (length 200) in the descriptor — targets 0x00086C68. [O]

#### 0x0007CF78 `write_rec_buf200` — stub
Write-path variant with the same 200-byte buffer &DAT_fff84640, type 1, state
&DAT_fff94056 — targets 0x00086D70. [O]

#### 0x0007CFD4 `send_rec_buf60` — stub
Submit adapter (type 1, table PTR_PTR_0007d048) with a 0x60-byte buffer at
&DAT_fff84708 — targets 0x00086C68. [O]

#### 0x0007D010 `write_rec_buf60` — stub
Write-path adapter for the same 0x60-byte buffer, state &DAT_fff94059 — targets
0x00086D70. [O]

#### 0x0007D06C `send_rec_buf48` — stub
Submit adapter (type 1, table PTR_PTR_0007d0e0) with a 0x48-byte buffer at
&DAT_fff84768 — targets 0x00086C68. [O]

#### 0x0007D0A8 `write_rec_buf48` — stub
Write-path adapter for the 0x48-byte buffer, state &DAT_fff9405c. [O]

#### 0x0007D104 `send_rec_buf38` — stub
Submit adapter (type 1, table PTR_PTR_0007d178) with a 0x38-byte buffer at
&DAT_fff847b0 — targets 0x00086C68. [O]

#### 0x0007D140 `write_rec_buf38` — stub
Write-path adapter for the 0x38-byte buffer, state &DAT_fff9405f. [O]

#### 0x0007D1CC `memcpy_aligned` — library
Word-aligned block copy: splits (dst,src,len) into head/body/tail by alignment mod 4
(len>3 only), wraps the copy in enter/exit hooks (PTR_FUN_0007d254/d258) and calls
byte-copy thunks 0x0007D1A8/0x0007D1BA plus aligned-copy thunk 0x0007D1C6. 11 callers
across boot/app (0x00071040…0x00082CD0). [O]

#### 0x0007D25C `check_range_table` — logic
Walks a table of 12-byte range descriptors {start,end,flags@+8,kind@+9}: mode 1 checks
the window [addr, addr+len-1] against overlapping entries (tracks flag mismatches vs
arg6, mixed kinds, consumes covered sub-ranges, final size check via FUN_0007D184);
mode 2 checks containment with kind mask arg7. Returns 0 ok, 1 not covered, 2 mixed
kinds, 3 flag mismatch, 4 size violation. Used by the memory-access validator. [O]

#### 0x0007D3CC `check_mem_access` — logic
Memory access validator for diagnostic reads/writes: selects a protection range table
(hook state == 2 + config char DAT_fff93b44 picks table PTR_DAT_0007d424/
PTR_LAB_0007d428, else PTR_DAT_0007d554/PTR_LAB_0007d550), validates via
check_range_table mode 1 plus an optional second table (PTR_PTR_0007d55c) mode 2;
special rules for window 0x600000–0x680000 (unlock byte DAT_fff848e8) and
0x800FFFFF..PTR_DAT_0007d560 (unlock hook PTR_FUN_0007d564); logs each verdict in a
2-entry ring at DAT_fff844a8 (12 bytes each, index DAT_fff844b2). Callers 0x00082C7C,
0x00082C00, 0x0007A91C, 0x00082A16, 0x00082CD0, 0x000760F8. Cf. spec 10 readable-areas
list / spec 15-16 protection. [O]

#### 0x0007D68A `consume_flag_fff94554` — data-accessor
Returns DAT_fff94554 != 0 and clears it — test-and-consume event flag. No direct
callers (table API). [O]

#### 0x0007D69E `consume_flag_fff94062` — data-accessor
Returns (char)DAT_fff94062 and clears bit 0 — test-and-consume. [O]

#### 0x0007DBB4 `check_str_all_valid` — logic
Walks the byte string arg (from offset 1); for each char derefs
PTR_PTR_0007dbd8[c]*4 and requires the target byte == 0x0F; returns 0 on the first
miss, else 1 — string-wide validity check over a per-char status table. [I]

#### 0x0007DCE8 `poll_compare_94efc` — logic
Cyclic check under trace frame 0xA1D1C8: hook PTR_FUN_0007dd30 compares DAT_fff94efc
against DAT_fff94d74; result drives hook PTR_FUN_0007dd34(2, changed). Resolved
callees 0x00086DFC, 0x0006BE34. [I]

#### 0x0007DD3C `init_event_rec_2e14` — logic
Init/report sequence for record area DAT_fff92e14: hook PTR_FUN_0007df30(0x1F), clears
DAT_fff93878, registers the record via PTR_FUN_0007df34(&rec, 1, 0xFE, 5), sets state
byte DAT_fff92e2b = 3, trailing hook DAT_0007df38. Resolved callees 0x0007F430/
0x0007F44E/0x00082F32. [I]

#### 0x0007E06C `init_channel_95007` — logic
Init/enable via hook PTR_FUN_0007e254(&DAT_fff95007, &DAT_fff94e7f, 0, 0xFF), clears
DAT_fff94067, then PTR_FUN_0007e258(1, 0). Callees 0x00086DFC/0x0006BD68. [I]

#### 0x0007E3F4 `init_channel_9500a` — logic
Sibling init with &DAT_fff9500a/&DAT_fff94e82, clears DAT_fff80d22,
PTR_FUN_0007e4e4(4, 0). [I]

#### 0x0007E510 `report_state_a1` — logic
State-change reporter, object A (state byte DAT_fff9406f, 4-byte record DAT_fff92e74):
if state != 1, publishes via hook PTR_FUN_0007e71c(DAT_0007e724, rec, 4, 0xFB), sets
state 1 and record tag DAT_fff92e9c = 9; always re-publishes via PTR_FUN_0007e720(rec).
Enter/exit hooks 0x7e710/0x7e718. [I]

#### 0x0007E550 `report_state_b1` — logic
Object B (DAT_fff94070, 0xA-byte record DAT_fff92ea8): target state 1, tag
DAT_fff92ed0 = 10, code 0xF5. Same hook pattern. [I]

#### 0x0007E590 `report_state_a2` — logic
Object A target state 2: tag DAT_fff92e9c = 0x11, code 0xFB. [I]

#### 0x0007E5D0 `report_state_b2` — logic
Object B target state 2: tag DAT_fff92ed0 = 0x12, descriptor PTR_LAB_00046292_2,
code 0xF5. [I]

#### 0x0007E610 `report_state_c2` — logic
Object C (DAT_fff94071, 0x12-byte record DAT_fff92edc): target 2, tag DAT_fff92f04 =
0x13, descriptor PTR_LAB_00046312_2, code 0xED; exit path decrements counter
*PTR_DAT_0009a420 instead of hook 0x7e718. [I]

#### 0x0007E650 `report_state_d2` — logic
Object D (DAT_fff94072, 0x1A-byte record DAT_fff92f10): target 2, tag DAT_fff92f38 =
0x14, descriptor PTR_LAB_00046322_2, code 0xE5; counter-decrement exit. [I]

#### 0x0007E690 `report_state_a3` — logic
Object A target state 3: tag DAT_fff92e9c = 0x19, descriptor PTR_LAB_0004633a_2,
code 0xFB. [I]

#### 0x0007E6D0 `report_state_b3` — logic
Object B target state 3: tag DAT_fff92ed0 = 0x1A, descriptor PTR_LAB_0004637a_2,
code 0xF5. [I]

#### 0x0007E744 `report_state_c3` — logic
Object C target state 3: tag DAT_fff92f04 = 0x1B, descriptor PTR_LAB_000463fa_2,
code 0xED; enter hook PTR_FUN_0007e8cc, counter-decrement exit. [I]

#### 0x0007E784 `report_state_d3` — logic
Object D (DAT_fff94072) → state 3: publishes record DAT_fff92f10 via hook
PTR_FUN_0007e8d4(PTR_LAB_0004640a_2, rec, 0x1A, 0xE5), tag DAT_fff92f38 = 0x1C;
counter-decrement exit. [I]

#### 0x0007E7C4 `report_state_a4` — logic
Object A (DAT_fff9406f) → state 4: tag DAT_fff92e9c = 0x21, code 0xFB, descriptor
DAT_0007e8e4, record DAT_fff92e74; exit via hook PTR_FUN_0007e8dc. [I]

#### 0x0007E804 `report_state_b4` — logic
Object B (DAT_fff94070) → state 4: tag DAT_fff92ed0 = 0x22, descriptor
PTR_LAB_00046462_2, code 0xF5, record DAT_fff92ea8. [I]

#### 0x0007E844 `report_state_c4` — logic
Object C (DAT_fff94071) → state 4: tag DAT_fff92f04 = 0x23, descriptor
PTR_LAB_000464e2_2, code 0xED, record DAT_fff92edc; counter-decrement exit. [I]

#### 0x0007E884 `report_state_d4` — logic
Object D → state 4: tag DAT_fff92f38 = 0x24, descriptor PTR_LAB_000464f2_2, code
0xE5, record DAT_fff92f10; counter-decrement exit. [I]

#### 0x0007E8F8 `remove_event_rec_2e40` — logic
Record teardown for DAT_fff92e40: unregister hook PTR_FUN_0007ea38(rec), decrement
active count DAT_fff9387a and, if the record's flag bytes (DAT_fff92e6b/6a) are set,
counters DAT_fff9406a/69. Resolved callee 0x0007F58A. [O]

#### 0x0007E938 `remove_event_rec_2e74` — logic
Same teardown for record DAT_fff92e74 (flags DAT_fff92e9f/9e). [O]

#### 0x0007E978 `remove_event_rec_2ea8` — logic
Same teardown for record DAT_fff92ea8 (flags DAT_fff92ed3/d2). [O]

#### 0x0007E9B8 `remove_event_rec_2edc` — logic
Same teardown for record DAT_fff92edc (flags DAT_fff92f07/06). [O]

#### 0x0007E9F8 `remove_event_rec_2f10` — logic
Same teardown for record DAT_fff92f10 (flags DAT_fff92f3b/3a). [O]

#### 0x0007EA48 `run_init_list` — dispatch
Recursive walker over 8-byte entries {tag byte, code ptr at +4}, 0xFF-terminated:
tag < 0xFD calls hook PTR_FUN_0007eabc(fn, tag); 0xFD recurses (nested list); 0xFE/other
calls fn directly. Callers 0x00086F26, 0x00086E90 — a startup/init table runner. [O]

#### 0x0007EA84 `set_status_bits_939d0` — logic
Encodes arg into status word DAT_fff939d0: index ((arg>>3)-1)&3 shifted left 1; if
(arg&7)==4 sets 0x31, else (arg&7)<<3 | index | 1. Signal-slot status bit setter.
Callers 0x00072AE0, 0x0008704C, 0x00072CCE. [I]

#### 0x0007EE48 `dispatch_by_type_17` — dispatch
If record arg has type byte at +0x17 in range 2..0xFE, matches it against an 8-byte
{char, fn} table at PTR_DAT_0007f120 and invokes the matched handler with the record.
Helper of the integrity sweep below. [O]

#### 0x0007EE8A `monitor_integrity_sweep` — logic
Sweeps the linked list at DAT_fff92408 of monitored objects: computes each node's check
value (hook PTR_FUN_0007f124, XOR with the 256-entry dword table at PTR_LAB_0004526a_2
when flag 0x1000000 set) and verifies the dual-rail copies at node[1]/node[8]
(0xAAAA/0x5555 sentinels), aging counters at +0x13/+0x19 vs threshold DAT_fff94128;
mismatches call dispatch_by_type_17 (0x2000000 flag) and raise error 0x226 with
position markers in DAT_fff90998; tracks the minimum-age object into DAT_fff9412b/29,
selection DAT_fff9331c / DAT_fff800e9, and returns a keyed XOR checksum
~(table[b3] ^ *DAT_fffc1c04 ^ DAT_fff93320). Callers 0x0008D020, 0x0008CEDC. Watchdog /
RAM-integrity monitor with inverted-copy checks. [I]

#### 0x0007A4C0 `trace_close_channel` — logic
Closes trace channel idx: if the capture ring (DAT_fff92cd0+4*idx) has entries
(index DAT_fff9389a+2*idx nonzero), writes a 0xFF terminator record at the write
position (bounded by capacity DAT_fff938ac+2*idx, else error 0x227 via hook
PTR_FUN_0007a788) and marks the channel's snapshot record with the 0xFD header, buffer
pointer and 0xFF tail. Called by the three one-shot snapshot wrappers (0x0007A59A/
0x0007A5CA/0x0007A5F8) before trace_snap_channel. [O]

## Unit summary

220 clusters, 233 named functions (rep + members). Kinds:
- logic: 94
- data-accessor: 70 (mostly 3-instr getters/setters around the 0xFFF845xx–0xFFF948xx status/event block)
- stub: 53 (channel wrappers, forwarders, fan-outs)
- library: 10 (bit remappers, memcpy_aligned)
- dispatch: 6 (task poll groups, init-list runner, type dispatch)
- unknown: 0 — no entry was left fully unclassifiable; the least-certain names are
  the [I]-marked evaluators (eval_obj_status_a/b/c/d, eval_interlock_matrix,
  calc_scaled_ratio, report_state_* family), which are structural matches to the
  status/fault framework rather than positively identified features.

Subsystem map of this unit:
1. 16-channel dataset block-transfer manager (0x79C28–0x7A3DC): per-channel
   check/close/write/read wrappers over four engines; RAM buffer pointer table at
   0xFFF845F0+4N, state 0xFFF845CC/0xFFF845DC arrays, CRC thunk 0x0007D548.
2. Trace/event recorder (0x7A4C0–0x7A746): 3 channels + fixed slots, error id 0x227.
3. Cyclic task poll groups a/b/c (0x7AB48/0x7AB9C/0x7ABC8) and their evaluators.
4. Measured-value acquisition + publish (0x7B6D8/0x7B896) — spec 04/10 relevance.
5. Status/fault-word framework (families A/B/C/D) with confirm/pending latches,
   class mappers, lamp/telltale drive hooks, event ids 0x9CE/0x9D8/0x9DB/0x9DD/
   0x9E3/0x97E/0x97F/0x988/0x98A/0x98B/0x992/0x21/0x31; shared history recorder
   0x0007CA24. Spec 13 (fault reporting) is the closest doc.
6. Record submit/write adapter family (0x7CD68–0x7D140, 0x7CF3A–0x7D140) into
   resolved handlers 0x00086C68/0x00086D70 — measurement-protocol shaped (spec 10).
7. Memory services: memcpy_aligned (0x7D1CC), range-table check (0x7D25C), and the
   diagnostic memory-access validator (0x7D3CC) with protection windows 0x600000–
   0x680000 and 0x800FFFFF+ — cross-ref spec 15/16 protection records.
8. State-change reporters + record teardown (0x7E510–0x7E9F8) and the dual-rail
   integrity monitor sweep (0x7EE8A, error 0x226, XOR table at 0x0004526A).

Cross-region leads:
- Channels 0–6 of the dataset manager (wrappers at 0x7986C–0x79B60) are in the
  PREVIOUS unit; it should mirror the dset_chN_* names and shares engines
  0x0007A0EA/0x0007A130/0x0007A26E/0x0007A3DC documented here.
- The notify fan-outs resolve into 0x83xxxx–0x86xxxx and 0x68xxx (other units):
  subscribers 0x838B0/0x83A1C/0x83DB0/0x842B0/0x849D4/0x84D7A/0x85678/0x85788/
  0x85810/0x85D50/0x86A80/0x86ACE/0x86AFE/0x86B0C/0x86B3C — those units should
  cross-check the dtc_report_cond_* / update_status_word_* pairing found here.
- 0x00086C68/0x00086D70/0x00086DFC/0x00086FFE consume the record adapters;
  0x0007EE8A's callers are 0x0008D020/0x0008CEDC.
- App-region getters (0x13xxxx–0x15xxxx) read the status bytes exposed here
  (0xFFF847F4–0xFFF84803, 0xFFF940D9–0xFFF940F7) — a future spec-13 pass could
  map those bytes to symptom names.
