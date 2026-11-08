# Unit 43 (r10_app)

Region 0x0013A350–0x0013FFFA. This slice is almost entirely the **safety layer** (redundant
monitoring/computation layer, spec 14 §2 note: "the safety-layer sources (`0x13xxxx`–`0x14xxxx`)
are themselves computed by that layer"). Recurring patterns in this unit:

- **Signal conditioners**: raw CAN/gathered value + status byte → validated value with
  substitute value from a calibration object, status 0/1/2 (spec 07 §7 pattern, cited at
  `0x13BB78`), often `+8`/clamp scaling.
- **Getters**: 2–3 instruction readers of one RAM global through a ROM pointer table entry
  (`PTR_DAT_0013xxxx`); consumed by gatherers `0xF7298`, `0xA7C9C` (park input, spec 14 §2).
- **Checkpoint reports**: `(monitor_obj, id)` indirect calls — program-flow checkpoints
  (spec 14 §5a "checkpoint 0x37" of `0x13CD80`); `(group, data, failed, tested)` 4-arg calls
  are fault-memory reports (spec 13 §2, `0xA0E70`).
- **Integrity**: state kept as `(value, ~value)` pairs; mismatch → fatal handler + infinite
  loop (spec 13 §2 `0x6865C` family).
- Speeds are recomputed as `x * 3000 / (teeth << 2)` from tooth-count denominators, i.e. the
  layer's own speed/ratio derivation from pulse counts.

#### 0x0013A350 `saf_debounce_timer_step` — logic
Debounce/settling timer for the safety-layer speed/ratio object: while an enable byte
(+0x12) is set, counts a 2000-tick timer down (stored at +0x00 with inverted copy at +0x04 as
-2001) unless state byte +0x0b==2 forces a reload; sets done-flag +0x0e. Validates the
inverted pair via 0x13A1AC (fatal handler + halt on mismatch). Called only by 0x13A20C.
Cluster of 1. [O]
#### 0x0013A3F0 `saf_get_u8_a3f0` — data-accessor
Returns one byte global through ROM pointer-table entry `0x13a6bc` (safety-layer state
object). Called by the park-module input gatherer `0xA7C9C` (spec 14 §2). Cluster of 1. [O]
#### 0x0013A3F8 `saf_speed_ratio_obj_init` — logic
Initializes the safety-layer speed/ratio object: recomputes 6 speed words as
`value * 3000 / (teeth << 2)` from tooth-count fields (substituting direct pass-through when
a per-channel calibration byte ≠ 1), resets debounce counters to 5, clears flags, and ORs
element/gear bits `1 << (code & 0x1f)` from 8 code bytes into words +0x00/+0x08 (codes from
two pointer-chain tables, one set offset by 0x20). Called once from task entry 0x132D8E.
Cluster of 1. [O]
#### 0x0013A60A `saf_chkpt_0x21_pass1` — stub
Single indirect call `(monitor_obj@0x13a6e8, 1, 0x21)` — program-flow checkpoint 0x21
reported with argument 1 (same checkpoint family as `0x13CD80`'s 0x37, spec 14 §5a).
Called by safety-layer cycle 0x148DBA. Cluster of 1. [I]
#### 0x0013A616 `saf_speed_ratio_cycle` — logic
Periodic update of the safety-layer speed/ratio object (companion of 0x13A3F8): refreshes the
six `*3000/(teeth<<2)` speeds every 0x65 calls, runs two polled condition evaluators through
function-pointer tables, steps debounce counters (+0x10/+0x14 words and byte flags), and when
both gear-mask words clear resets the whole state block; ends with checkpoint 0x21 report.
Called by the big safety-layer evaluation cycle 0x14F34C. Cluster of 1. [O]
#### 0x0013A9D4 `saf_situation_eval` — logic
1405-instruction situation evaluator for the safety layer (analogue of the application's
situation selector 0x10C402, spec 09): polls ~10 condition functors, then per active
gear/lever arm checks speed-vs-calibration threshold pairs (`param_1[3]+0x20..+0x7c`, objects
via `PTR_DAT_0013abd8…0x13b864`), latching per-arm enable flags and emitting situation codes
4, 9, 0x17, 0x1b–0x28 into +0x4a (+0x1b mirror). Reloads all debounce state when the mask
words change. Called by 0x13236E (from 0xB90B2) and passes param_2 as a situation override.
Cluster of 1. [O]
#### 0x0013B74E `saf_gear_lever_match_cycle` — logic
Compares the object's two gear/element mask words (+0x00/+0x08) byte-by-byte through a
256-entry table, and against the lever code at +0x19 (codes 1–8, bit positions from
`PTR_PTR_0013ba74…0x13ba98`); runs a 3-state matcher (+0x16 = 1/2/3) that only advances when
the code's bit is set in the mask, remembering +0x19 from +0x18. Finishes with a fault-report
call `(group, data, failed, tested)` (spec 13 §2 API) when any plausibility condition fired.
Called by safety-layer cycle 0x14F34C. Cluster of 1. [O]
#### 0x0013BA5C `saf_lever_code_2_5` — logic
Returns 1 when the object's lever/request code at +0x19 is in 2..5 (R/N/D band of the lever
decode, spec 14 §5a), else 0. Used by the situation evaluator 0x13A9D4 and 0x13A616.
Cluster of 1. [O]
#### 0x0013BAA8 `saf_get_engine_temp` — data-accessor
Getter for the conditioned engine (coolant) temperature produced by 0x13BABA (spec 07 §7:
→ `0xFFF8BAA8E`-chain, app `+0x104`). Readers 0xF7298, 0xB9646. Cluster of 1. [O]
#### 0x0013BAB0 `saf_get_engine_temp_st` — data-accessor
Returns the companion status byte (pointer table `0x13bb58`) of the engine-temperature signal
conditioned by 0x13BABA. Reader 0xF7298. Cluster of 1. [I]
#### 0x0013BABA `saf_cond_engine_temp` — logic
Engine-temperature conditioner (spec 07 §7): status byte at +5 selects raw path; raw 0xFF is
invalid → status 2 with substitute from the object at `PTR_DAT_0013bb5c+0x30` (byte+8 scaling,
clamp <0xF8) — per spec the substitute is the transmission fluid temperature; valid path
stores raw+8 with status 0. Output at +6, status at +7. Called by 0x14F7AA. Cluster of 1. [O]
#### 0x0013BB60 `saf_get_s16_bb60` — data-accessor
Returns a signed 16-bit global via ROM pointer-table entry `0x13bc10` (safety-layer signal
adjacent to the engine-temperature block). Reader 0xF7298. Cluster of 1. [I]
#### 0x0013BB66 `saf_get_u8_bb66` — data-accessor
Returns one byte global via ROM pointer-table entry `0x13bc14` (companion status/flag of the
`0x13bb60` signal). Reader 0xF7298. Cluster of 1. [I]
#### 0x0013BB6E `saf_copy_u16_ind` — data-accessor
Copies a u16 field out of an object held in `*param_1` (indirect `+4`) into `param_1+8` —
field-copy helper used by gatherer 0x132980. Cluster of 1. [O]
#### 0x0013BB78 `saf_sig_cond_u16` — logic
The spec-cited "typical pattern" validated-value conditioner (spec 07 §7): if status (+6)
is 0 and raw u16 (+4) ≠ -1, pass through with status 0; otherwise mark status 1 (raw
invalid) or substitute from calibration object `PTR_DAT_0013bc18` (+10, status 2 when its
+0xC flag set), and clamp the result to 0x9EC (2540). Called by safety-layer cycle 0x14F34C.
Cluster of 1. [O]
#### 0x0013BBCC `saf_mon_report_1cond` — logic
Single-condition fault monitor step: computes `tested` = (signal status OK and raw value <
threshold `*PTR_DAT_0013bc24`) and `failed` = (invalid flag), then issues the fault-memory
report `(group, data, failed, tested)` through the pointer pair at `0x13bc2c/0x13bc28`
(spec 13 §2 API; resolved callee 0xA0E70). Called by safety-layer sub-cycle 0x14F7CC.
Cluster of 1. [O]
#### 0x0013BC32 `saf_cond_u8_add8` — logic
Byte conditioner of the engine-temperature family: status 0 + raw ≠ 0xFF → clamp raw to
0xF7 and store raw+8 with status 0; 0xFF or bad status → flag +4 and status 2 with output 0.
Called by 0x14F7AA. Cluster of 1. [O]
#### 0x0013BC68 `saf_get_bcb4_val` — stub
Wrapper: calls helper 0x13BC84 on the object at ROM pointer `0x13bcb4` (value path); used by
park gatherer 0xA7C9C and 0xB912C. Member 0x13BC76 is the byte-identical variant calling
0x13BC8A (status path). Cluster of 2. [I]
#### 0x0013BC92 `saf_cond_bool` — logic
Boolean conditioner: status byte +1 selects — status 0 → result = (raw == 1), validity 0;
bad status → result 0 and validity 2 (the layer's standard 0/2 validity encoding). Member
0x001485CA identical (r14_app copy). Called from safety-layer cycles 0x132D8E/0x14F34C.
Cluster of 2. [O]
#### 0x0013BCBA `saf_mon_report_4cond` — logic
Multi-condition fault-monitor step: from an input object and the calibration objects at
`PTR_DAT_0013bee4…0x13bf20`, builds four `failed/tested` pairs (speed-below-limit conditions
with status==2 gating, bit-qualified enable flags) and issues four fault-report calls
(spec 13 §2 API); resolved callee 0xA0FC4/0xA0E70 are fault-status readers/reporters.
Called by 0x14F7EA. Cluster of 1. [O]
#### 0x0013BF24 `saf_get_eng_speed` — data-accessor
Getter for the conditioned **engine speed** (spec 07 §7: `0xFFF8E908` → conditioned
`0xFFF8E90E`, feeds app `+0x2C` chain). Readers 0xF7298, 0x134DB4, 0x1534FC, 0xADE6E,
0xB912C. Cluster of 1. [O]
#### 0x0013BF2A `saf_get_eng_speed_st` — data-accessor
Returns the status byte (pointer table `0x13c0f0`) accompanying the conditioned engine speed
of 0x13BF24. Readers 0xF7298, 0x1534FC, 0x135184, 0xB912C. Cluster of 1. [I]
#### 0x0013BF32 `saf_get_s16_bf32` — data-accessor
Returns an s16 global via ROM pointer `0x13c0f4` (safety-layer measured value; consumed by
park gatherer 0xA7C9C and 0xF471C/0x155AE4). Cluster of 1. [I]
#### 0x0013BF38 `saf_get_s16_bf38` — data-accessor
Returns an s16 global via ROM pointer `0x13c0f8` (companion of the `0x13bf32` signal).
Readers 0xF471C, 0x155AE4. Cluster of 1. [I]
#### 0x0013BF3E `saf_get_u8_bf3e` — data-accessor
Returns one byte global via ROM pointer `0x13c0fc` (status/flag of the `0x13bf32/38` pair).
Readers 0xA7C9C, 0x1534FC, 0x155AE4. Cluster of 1. [I]
#### 0x0013BF46 `saf_get_u8_bf46` — data-accessor
Returns one byte global via ROM pointer `0x13c100` (status companion of the 0x13bf3e signal
group). Readers 0x1534FC, 0x155AE4. Cluster of 1. [I]
#### 0x0013BF4E `saf_obj_init_raw_invalid` — stub
Initializes a gatherer-object record to the raw-invalid defaults (+0x10 = 0xFFFF, +0x13 =
0xFF) — the layer's "no value yet" encoding. Called by gatherer 0x132980. Cluster of 1. [O]
#### 0x0013BF5A `saf_chkpt_0x29_pass1` — stub
Program-flow checkpoint report `(monitor_obj@0x13c104, 1, 0x29)`; companion of the 0x29
checkpoint in 0x13BF66. Called by safety-layer cycle 0x148DBA. Cluster of 1. [I]
#### 0x0013BF66 `saf_sig_cond_u16_invcopy` — logic
Validated-value conditioner of the 0x13BB78 family with inverted-copy maintenance: status
(+8) 0 and raw u16 (+4) ≠ -1 → pass-through status 0, else substitute from calibration
object `PTR_DAT_0013c110+0x6a` (status 2); then rewrites value/status through setter
functors (`PTR_FUN_0013c114/0x13c118`), re-checks every (value, ~value) pair (incl. the
substitute object) and forces 0/status-2 on mismatch; checkpoint 0x29 at entry and exit.
Called by 0x14F34C and 0x133C4E. Cluster of 1. [O]
#### 0x0013C06A `saf_mon_report_thr_c06a` — logic
Fault-monitor step: `tested` = (enable flag `*PTR_DAT_0013c128`==1, validity 0, two polled
condition functors both false, raw value < threshold `*PTR_DAT_0013c12c`), `failed` =
(status byte == 1); single report `(group, data, failed, tested)` at the end (spec 13 §2;
callees 0xA0FC4/0xA0E70 are the fault-status API). Called by 0x14F7CC. Cluster of 1. [O]
#### 0x0013C144 `saf_get_c160_val` — stub
Wrapper: u16 value getter via 0x13C160 on the object at ROM pointer `0x13c1a0`. Reader
0xF7298 (safety-layer → application gatherer). Cluster of 1. [O]
#### 0x0013C152 `saf_get_c164_val` — stub
Wrapper: byte getter via 0x13C164 on the object at ROM pointer `0x13c1a0` (status path of
the 0x13C144 signal). Reader: park gatherer 0xA7C9C. Cluster of 1. [I]
#### 0x0013C16C `saf_cond_u8_x50` — logic
Byte conditioner with ×50 scaling: when calibration byte `*PTR_DAT_0013c1a4` is 0 it forces
the substitute 0x1027 with status 0; otherwise raw (0xFF left as-is) is scaled to
u16 = raw*0x32 with status 1. Called by 0x132D8E and 0x14F34C. Cluster of 1. [O]
#### 0x0013C1AA `saf_range_code_lookup` — logic
Maps a range-request code 5–8 (D/N/R/P of the lever decode, spec 14 §5a) to index 0–3
(other codes → index 4) and reads a per-range entry through table functor
`PTR_FUN_0x13c318`; additionally gates a pass-through byte (+4) on a low-value condition
(raw>>1 < `*PTR_DAT_0013c31c`, mode byte 3, flag +5==0). Called by 0x133476 and 0x14F7CC.
Cluster of 1. [I]
#### 0x0013C21E `saf_mon_report_2arm` — logic
Two-arm fault-monitor step: both arms test (status==2 AND raw u16 < limit +6 AND two polled
functors false); arm selection by flag +9 (pass/fail polarity), then one report
`(group, data, failed, tested)` via `PTR_PTR_0x13c340` (callees 0xA0FC4/0xA0E70). Called by
the safety-layer cycle 0x14F34C. Cluster of 1. [O]
#### 0x0013C344 `saf_get_c352_val` — stub
Wrapper: returns the byte at +6 of the object at ROM pointer `0x13c380` via 0x13C352.
Reader 0xF7298. Cluster of 1. [O]
#### 0x0013C352 `saf_obj_get_byte6` — stub
Two-instruction field getter: `return *(u8*)(obj+6)`. Member 0x0014B442 is the same pattern
used by 0x14B412 in r14_app (duplicated helper). Cluster of 2. [O]
#### 0x0013C35E `saf_cond_byte_gate` — logic
Gated byte conditioner: passes the raw byte at +4 through to +6 only when the source status
(`**obj`) is 0, an inhibit flag at +5 is 0, and raw ≠ -1; otherwise emits 0. Called by
0x132D8E and 0x14F34C. Cluster of 1. [O]
#### 0x0013C384 `saf_get_eng_torque_t3` — data-accessor
Getter for the conditioned **engine torque T3** (spec 07 §7: `0xFFF8EB70` → 0x13C51A →
`0xFFF8EB80`, app `+0xFE` chain). Readers 0xF7298, 0x1534FC, 0x134DE0, 0xB912C.
Cluster of 1. [O]
#### 0x13C38A `saf_get_s16_c38a` — data-accessor
Returns an s16 global via ROM pointer `0x13c5dc` — companion torque-family signal of
0x13C384 (adjacent table slot). Reader 0xF471C. Cluster of 1. [I]
#### 0x13C390 `saf_get_u8_c390` — data-accessor
Returns the status/flag byte (table `0x13c5e0`) of the engine-torque signal group.
Readers 0x1534FC, 0x135184, 0xB912C. Cluster of 1. [I]
#### 0x13C398 `saf_get_u8_c398` — data-accessor
Returns one byte global via ROM pointer `0x13c5e4`. Reader 0x1534FC. Cluster of 1. [I]
#### 0x13C3A0 `saf_get_s16_c3a0` — data-accessor
Returns an s16 global via ROM pointer `0x13c5e8`. Reader 0xF471C. Cluster of 1. [I]
#### 0x13C3A6 `saf_float_init_neg1` — stub
Initializes the float work field at obj+0x18 to -1.0 (0xBF800000) — invalid marker of the
float-mirrored torque object. Called by 0x13C4FE. Cluster of 1. [O]
#### 0x13C3B6 `saf_tq3_float_eval` — logic
Float-domain evaluation step of the engine-torque object (called by 0x13C51A): range-checks
the float input (+8) and looks up per-range entries through functors on
`PTR_DAT_0013c5f8+0x10/+0x14` (out-of-range → index 4), latches pass-through (+0xD when
lookup==1), and runs a small state machine on (+0,+1) that scales `(x-2000)/4`, producing
value/status floats (+3..+7) and their inverted copies. Cluster of 1. [O]
#### 0x13C4FE `saf_tq3_obj_init` — stub
Torque-object field initialization: +0x54 = 0xFFFF, +0x51 = ~+0x50, then float init via
0x13C3A6. Called by gatherer 0x132980. Cluster of 1. [O]
#### 0x13C50E `saf_chkpt_0x2a_pass1` — stub
Program-flow checkpoint report `(monitor_obj@0x13c60c, 1, 0x2a)` — companion of checkpoint
0x2A in 0x13C51A. Called by 0x148DBA. Cluster of 1. [I]
#### 0x13C51A `saf_cond_eng_torque_t3` — logic
Safety-layer **engine torque T3 conditioner** (spec 07 §7): maps the range code (+0x4A,
5–8 → idx 0–3, else 4) through two table functors (+0x5B/+0x5C), runs helper gates
0x13C89C/0x13C864(+0x13C880), converts the 13-bit raw (+0x3C, 0x1FFF = invalid) with
`(x-2000)/4` into +0x56 (status 0) or forces 0 (status 2), selects direct vs. gated output
(+0x4C/+0x4E), then mirrors everything into floats via 0x13C3B6 — validating every
(value, ~value) pair and the float/int agreement, else fatal handler + halt; checkpoint
0x2A on entry/exit. Called by 0x14F34C. Cluster of 1. [O]
#### 0x13C864 `saf_tq3_range_flag_a` — logic
Helper gate of the torque conditioner 0x13C51A: when qualifier +0x47 is 0, output +0x59 =
(table result +0x5C) if enable +0x46==1, else 0. Member 0x13C880 is the pattern-identical
second gate (+0x49/+0x5C/+0x48 → +0x5A). Cluster of 2. [O]
#### 0x13C89C `saf_tq3_gate_flag` — logic
Helper gate of 0x13C51A: when +0x45 is 0, sets +0x58 = 1 iff qualifier +0x44==0 and range
code +0x48 != 3. Cluster of 1. [O]
#### 0x0013C8F4 `saf_get_u8_c8f4` — data-accessor
Returns one byte global via ROM pointer `0x13c990` (pedal-block neighborhood). Reader 0xF7298.
Cluster of 1. [I]
#### 0x0013C8FE `saf_pedal_mode_gate` — logic
Condition evaluator: with validity +5==0, gates on a mode byte from sub-object `*(+8)`
(0 → pass raw byte, 1 with raw==1 or second signal below threshold `PTR_DAT_0013c994[6]`
→ 1), writing result +6 and validity +7 (2 when invalid); runs the identical sub-check
0x13C956 into +8 first. Called by 0x14F34C. Pedal-related by table adjacency [I].
Cluster of 1. [I]
#### 0x0013C956 `saf_pedal_mode_gate_sub` — logic
Sub-evaluator of 0x13C8FE: same mode-byte/threshold chain against `*PTR_DAT_0013c998`,
result into +8 of the record. Cluster of 1. [I]
#### 0x0013C99C `saf_get_pedal` — data-accessor
Getter for the conditioned **accelerator pedal** value (spec 07 §7: `0x13CA22` →
`0xFFF9545E`, app `+0x100` chain). Readers 0xF7298, 0xADEA0, 0x134D52, 0x1534FC, 0xAE5A0,
0xB912C. Cluster of 1. [O]
#### 0x0013C9A4 `saf_get_pedal_st` — data-accessor
Status byte getter (table `0x13ca68`) of the accelerator-pedal signal. Readers 0x135184,
0xAE5A0. Cluster of 1. [I]
#### 0x0013C9AE `saf_chkpt_0x30_pass1` — stub
Program-flow checkpoint report `(monitor_obj@0x13ca6c, 1, 0x30)` — companion of checkpoint
0x30 in 0x13C9BA. Called by 0x148DBA. Cluster of 1. [I]
#### 0x0013C9BA `saf_pedal_cycle` — logic
Pedal-object cycle: checkpoint 0x30, run the pedal conditioner 0x13CA22, run the fault
report 0x13C9E8, checkpoint 0x30 again when a re-run flag is set. Called by the
safety-layer cycle 0x14F34C. Cluster of 1. [O]
#### 0x0013C9E8 `saf_mon_report_pedal` — logic
Pedal fault-monitor step: `failed` = (invalid flag at +8 == 1, data = group byte from
`PTR_PTR_0013ca84`), `tested` = (validity +5 == 0 AND enable `*PTR_DAT_0013ca80` != 0);
single report `(group, data, failed, tested)` (callee 0xA0E70). Cluster of 1. [O]
#### 0x0013CA22 `saf_cond_pedal` — logic
Accelerator-pedal conditioner (spec 07 §7): valid raw (≠0xFF, status 0) is rescaled
`raw*127/125` (0–250 → 0–254, clamp <0xFB else 0xFE) with status 0; 0xFF or bad status →
status 2 with substitute from the object at `*param_1`. Output +6, status +7.
Cluster of 1. [O]
#### 0x0013CA90 `saf_get_u8_ca90` — data-accessor
Returns one byte global via ROM pointer `0x13cad0`. Reader 0xF7298. Cluster of 1. [I]
#### 0x0013CA9A `saf_cond_code_45` — logic
State-code conditioner: with status +1==0 and code ≠7 → flag +2 = (code==4 or code==5),
validity +3=0; code 7 or bad status → flag 0, validity 2. Called by 0x14F34C.
Cluster of 1. [I]
#### 0x0013CAD4 `saf_copy_u8_widen` — data-accessor
Copies one byte from `*(obj+4)` (widened) into the record's u16 at +4 — gatherer field
copy. Called by 0x132980. Cluster of 1. [O]
#### 0x0013CAE4 `saf_mon_report_bit_thr` — logic
Fault-monitor step in one call: `failed` = ((byte from `*(+8)` & mask `*PTR_DAT_0013cc10`)
!= 0), `tested` = (u16 from `*(+0xC)` < threshold `*PTR_DAT_0013cc14`); report
`(group, data, failed, tested)` (callee 0xA0E70). Called by 0x14F7EA. Cluster of 1. [O]
#### 0x0013CB24 `saf_mon_report_debounce` — logic
Debouncing fault-monitor step: a counter at +4 (seeded from `*(+4)`, decremented while
calibration byte `PTR_DAT_0013cc24[0x6E]`==1) qualifies `tested` (status==2 AND raw <
`*PTR_DAT_0013cc14` AND arm byte ≠1 AND two polled functors false, mode-selected by
`PTR_DAT_0013cc24[0x6D]`), `failed` = counter==0; one report call (callees 0xA0FC4/0xA0E70).
Called by 0x13CAE0. Cluster of 1. [O]
#### 0x0013CC44 `saf_shf_get_src_status` — data-accessor
Getter for the shifter-evaluator **source status** `0xFFF8E95D` (spec 14 §5a: 0 preferred /
1 fallback / 2 no valid source; object `0xFFF8E91C+0x41`). Readers: park gatherer 0xA7C9C
(D61A+0x59), 0x155AE4, 0x135230. Cluster of 1. [O]
#### 0x0013CC4C `saf_shf_get_ce80` — data-accessor
Byte getter via ROM pointer slot `0x13ce80` — adjacent field of the shifter-evaluator
object (readers 0x155AE4). Cluster of 1. [I]
#### 0x0013CC54 `saf_shf_get_ce84` — data-accessor
Byte getter via ROM pointer slot `0x13ce84` of the shifter-evaluator object; read by the
park gatherer 0xA7C9C. Cluster of 1. [I]
#### 0x0013CC5C `saf_shf_get_ce88` — data-accessor
Byte getter via ROM pointer slot `0x13ce88`; read by the park gatherer 0xA7C9C.
Cluster of 1. [I]
#### 0x0013CC64 `saf_shf_get_range_req` — data-accessor
Getter for the shifter-evaluator **range request** `0xFFF8E964` (spec 14 §5a, values 5–8 =
D/N/R/P; object `0xFFF8E91C+0x48`). Readers 0x132D8E, park gatherer 0xA7C9C (D61A+0x58),
0x14FC5C, 0x155AE4. Cluster of 1. [O]
#### 0x0013CC6C `saf_shf_get_ce90` — data-accessor
Byte getter via ROM pointer slot `0x13ce90` (shifter-evaluator object field). Reader
0x155AE4. Cluster of 1. [I]
#### 0x0013CC74 `saf_shf_get_ce94` — data-accessor
Byte getter via ROM pointer slot `0x13ce94`; read by the park gatherer 0xA7C9C.
Cluster of 1. [I]
#### 0x0013CC7C `saf_shf_get_ce98` — data-accessor
Byte getter via ROM pointer slot `0x13ce98`; read by the park gatherer 0xA7C9C.
Cluster of 1. [I]
#### 0x0013CC84 `saf_shf_get_ce9c` — data-accessor
Byte getter via ROM pointer slot `0x13ce9c`. Readers 0x144108 and 0x77B90 (outside this
unit). Cluster of 1. [I]
#### 0x0013CC94 `saf_shf_get_cea4` — data-accessor
Byte getter via ROM pointer slot `0x13cea4`. Readers 0x144108, 0x77B90. Cluster of 1. [I]
#### 0x0013CCA4 `saf_shf_get_ceac` — data-accessor
Byte getter via ROM pointer slot `0x13ceac`. Readers 0x144108, 0x77B90. Cluster of 1. [I]
#### 0x0013CCB4 `saf_shf_get_ceb4` — data-accessor
Byte getter via ROM pointer slot `0x13ceb4` (shifter-evaluator object field). Readers
0x144108, 0x77B90. Cluster of 1. [I]
#### 0x0013CCC4 `saf_shf_get_cebc` — data-accessor
Byte getter via ROM pointer slot `0x13cebc`. Readers 0x144108, 0x77D90. Cluster of 1. [I]
#### 0x0013CCD4 `saf_shf_get_cec4` — data-accessor
Byte getter via ROM pointer slot `0x13cec4`. Readers 0x144108, 0x77D90. Cluster of 1. [I]
#### 0x0013CCE4 `saf_shf_get_cecc` — data-accessor
Byte getter via ROM pointer slot `0x13cecc`. Readers 0x144108, 0x77D90. Cluster of 1. [I]
#### 0x0013CCF4 `saf_shf_get_ced4` — data-accessor
Byte getter via ROM pointer slot `0x13ced4`. Readers 0x144108, 0x77D90. Cluster of 1. [I]
#### 0x0013CD04 `saf_shf_get_cedc` — data-accessor
Byte getter via ROM pointer slot `0x13cedc`; read by 0xA81D8, the second park-input
gatherer (spec 14 §1 fills D61A +0x7C…+0x81). Cluster of 1. [I]
#### 0x0013CD0C `saf_shf_get_cee0` — data-accessor
Byte getter via ROM pointer slot `0x13cee0`. Readers 0x144108, 0x77D90, 0x77B90.
Cluster of 1. [I]
#### 0x0013CD14 `saf_shf_obj_init_ff` — stub
Initializes 26 byte fields of the shifter-evaluator object (the code/inverted-code pairs at
+0x06…+0x7A) to 0xFF — the "no value" marker. Called by gatherer 0x132980. Cluster of 1. [O]
#### 0x0013CD74 `saf_chkpt_0x37_pass1` — stub
Program-flow checkpoint report `(monitor_obj@0x13cee4, 1, 0x37)` — companion of checkpoint
0x37 in 0x13CD80. Called by 0x148DBA. Cluster of 1. [I]
#### 0x0013CD80 `saf_shifter_eval` — logic
The safety layer's **shifter evaluator** (spec 14 §5a, 759 instrs, object `0xFFF8E91C`):
checkpoint 0x37; verifies ~20 (value, ~value) byte pairs; classifies the shifter-type
variant byte (`*PTR_DAT_0013cef0`: 9/10/12/13 → type 0, 6/7/11 → 1, 14 → 2) into +0x68;
per channel A/B runs the decode-table validity check 0x13D534 and the fault-report steps
0x13D632/0x13D6AE, the type-2 flag decode 0x13D5DC and channel-error flags 0x13D580;
source selection 0x13DB34 then decode-table lookup 0x13D9EC produce the **range request
+0x48 (E964)** and source status +0x41 (E95D); selects the per-type output code set
(+0x70..+0x73/+0x4D..+0x55/+0x66) and aggregates a bit-flag byte +0x4C; rewrites all fields
through the setter functor `PTR_FUN_0013d474` and re-validates; sets the +0x75 run flag;
checkpoint 0x37 again. Called by 0x14F34C and 0x133C4E. Cluster of 1. [O]
#### 0x0013D534 `saf_shf_code_tbl_check` — logic
Lever-code validity check against the decode table `0x646D8` (spec 14 §5a: 7-byte entries,
`type*0x70 + code*7`): only when the channel's code/inverted-code pair is consistent
(param_3 == ~param_4, param_2==0), marks the channel invalid when entry byte 0 == 0xFF or
entry byte 5 == 0. Helper of 0x13CD80. Cluster of 1. [O]
#### 0x0013D580 `saf_shf_chan_err_flags` — logic
Builds the 2-bit channel-error byte +0x4A of the shifter object: bit0 set when channel A
reports an error/invalid (incl. type-2 extra flags +0x77/+0x3E), bit1 for channel B
(+0x78/+0x40). Helper of 0x13CD80. Cluster of 1. [O]
#### 0x0013D5DC `saf_shf_type2_flags` — logic
Type-2 shifter extra decode: from the second code pair (+0x3D/+0x3F, values 0/2/3 =
valid) computes the auxiliary flags +0x77/+0x78; for other types both are cleared.
Helper of 0x13CD80. Cluster of 1. [O]
#### 0x0013D632 `saf_shf_mon_report_a` — logic
Fault-report step for shifter channel A: `failed` set from the polled invalid flags
(mode byte must be 0/1/2; code bit 0x40 marks the error arm), `tested` = no unexpected
code bits and raw speed ≥ threshold `*PTR_DAT_0013d6e4`; report `(group, data, failed,
tested)` (callee 0xA0E70). Helper of 0x13CD80. Cluster of 1. [O]
#### 0x0013D6AE `saf_shf_mon_report_b` — logic
Channel-B twin of 0x13D632 (mode byte must be 0/1/3, threshold `*PTR_DAT_0013d9b0`).
Helper of 0x13CD80. Cluster of 1. [O]
#### 0x0013D75A `saf_shf_gather_inputs` — logic
Gathers nine shifter-related inputs by calling getter functors (`PTR_PTR_0013d9bc…0x13d9dc`,
resolved getters 0x9B6E0-family) into object bytes +0x22…+0x29 and +0x3A. Called by
0x151C3E (outside this unit). Cluster of 1. [O]
#### 0x0013D7BC `saf_shf_output_latch` — logic
Output latch of the shifter evaluator: verifies the +0x75/+0x76 run pair (fatal handler
otherwise); for type 1 copies +0x3A into +0x67; for type 0 copies the eight channel bytes
(+0x22…+0x29) into +0x56…+0x63 with inverted copies (+0x57…+0x65), else clears them; then
re-checks every source and destination (value, ~value) pair and rewrites all fields
through the setter functor `PTR_FUN_0013d9e8`, fatal on mismatch. Called by 0x14F784 and
0x133C4E. Cluster of 1. [O]
#### 0x0013D9EC `saf_shf_decode_tbl_lookup` — logic
Decode-table application (spec 14 §5a): from the selected lever code (+0x79) and its
complement nibble (+0x7A & 0xF) with type (+0x68), reads table `0x646D8` entries —
+0x48 = entry[code][0] (**range request E964**), +0x49 = entry[nib][1], +0x44 = [code][2],
+0x45 = [nib][3], +0x46 = [code][4] (only for code ≠ 0), +0x47 = [code][6]; keeps the
previous +0x48/+0x49 for "no change" codes (0, 0xC/type 0, 6/other) and the previous
+0x44/+0x45 for code 0. Helper of 0x13CD80. Cluster of 1. [O]
#### 0x0013DB34 `saf_shf_source_select` — logic
**Source selection** between the two redundant shifter channels (spec 14 §5a: mode from the
calibration object via functor `PTR_FUN_0013dc60`; 0 = prefer B/fallback A, 1 = prefer
A/fallback B (stock), 2 = A only, 3 = B only): picks the A or B code pair (+0x79/+0x7A)
and status +0x69, writes source status +0x41 = 0 preferred / 1 fallback / 2 none and the
error mirror +0x42 (0xFF/0xFE/0xFD), plus channel selector +0x6D. Helper of 0x13CD80.
Cluster of 1. [O]
#### 0x0013DC6A `saf_bit_pos_decode` — logic
Decodes the lowest set bit of a position byte from `*(obj+0x10)` into a one-hot code
1/2/4/8/0x10/0x20/0x40 (0 when zero), after running the state machine 0x13DCE4; output at
+4 of the record. Called by safety-layer cycle 0x14F34C. [I] position/direction decode.
Cluster of 1. [I]
#### 0x0013DCE4 `saf_pos_state_machine` — logic
Small state machine behind 0x13DC6A: builds two mask words through functors
(`PTR_PTR_0013dd88/0x13dd8c`), latches a change flag pair, walks states 0→1→2→3 qualified
by two speed comparisons against `*PTR_DAT_0013dd94`, writing state byte +5; clears when
an external flag (`*PTR_DAT_0013dd98`) demands. Callees 0xA102C/0xA1026 are fault-status
readers. Cluster of 1. [I]
#### 0x0013DD9C `saf_invcopy_check_de9c` — logic
Shared integrity helper of the float-domain E-block objects: if `*p != ~p[1]` calls the
fatal handler (two indirect calls) and halts in an infinite loop. Seven callers inside
this unit (0x13DFDC, 0x13E254, 0x13E5FC, 0x13F484, 0x13F7AC, 0x13FB6C, 0x13FDE6).
Cluster of 1. [O]
#### 0x0013DDC4 `saf_eobj_init` — logic
Initializes the float-domain arbitration object of the E-block: stores two table labels,
copies two bytes from the sub-object (+0x1C/+0x28 of `*(+0x18)`) with inverted copies,
seeds ~15 float work/ mirror slots (+0x40…+0xA8, +0x88/+0xC8), reads the two mode bytes at
`0x13deac+0x18/9` as floats and derives three boolean floats (+0xAC/+0xB0/+0xB4) from
bits 4/2/1; finishes with 0x13E362. Called by 0x13E4FC. Cluster of 1. [O]
#### 0x0013DE86 `saf_eobj_copy_byte_28` — stub
Refreshes the (value, ~value) pair at +0x24/+0x28 from the sub-object byte at
`*(+0x18)+0x28`. Called by 0x13F470. Cluster of 1. [O]
#### 0x0013DEB0 `saf_eobj_pair_verify` — logic
Verifies sixteen float pairs of the E-block object satisfy `f == K - g` (complementary
weights, constants `DAT_0013dfd4/0x13dfd8`), builds a 1.0/0.0 result float and passes it
to functor `PTR_FUN_0013dfd0`. Called by 0x13E5FC. Cluster of 1. [O]
#### 0x0013DFDC `saf_ratio_match_tbl` — logic
Float-domain element-ratio matching of the E-block sub-object: reads the expected-state byte
and measured u16 from the calibration sub-object, runs the per-element check 0x13E434 for
five elements (expected ratios `DAT_0013e28c`, `DAT_0013e290`, 2.0, 1.0, 0.0), validates the
complementary float pairs, and when all elements matched for a debounce period (int
counter pair +0x1C/+0x20 of the parent) stores the u16 from table `0x6489E` indexed by the
match mask into +0x54; reloads the counter via functor otherwise; rewrites all fields
through the setter functor. Called by 0x13E5FC. Cluster of 1. [O]
#### 0x0013E19A `saf_ratio_dev_chk_a` — logic
Float ratio-deviation check: when both qualifier floats are 0, computes
`x * DAT_0013e2b4 / y`, compares |computed − measured(+0x74)| against the u16 thresholds at
sub-object +8/+0xC and latches boolean float +0xB0 (1.0 = deviation). Called by 0x13E5FC.
Cluster of 1. [O]
#### 0x0013E254 `saf_ratio_dev_chk_b` — logic
Second ratio-deviation check of the E-block (scale `DAT_0013e4ec`, measured +0x78,
thresholds at sub-object +0x10/+0x14, extra inverted-pair gate +0x24) latching boolean
float +0xB4. Called by 0x13E5FC. Cluster of 1. [O]
#### 0x0013E362 `saf_vote3_float` — logic
Three-input voting on the boolean floats +0xAC/+0xB0/+0xB4: pairwise ANDs into +0xCC/+0xD0/
+0xD4 and the all-three AND into +0xD8, then rewrites through the setter functor. Called by
0x13E5FC and at init (0x13DDC4). Cluster of 1. [O]
#### 0x0013E434 `saf_elem_match_chk_f` — logic
Per-element float plausibility check: given a measured value, element index → bit
(`1 << (idx & 0x1F)`) and expected-state byte, tests the value against the bands
`0 ± tolerance` / `DAT_0013e4f8 ± tolerance` (tolerances `DAT_0013e4f4/0x13e4f8`), adding the
bit weight to the match/mismatch accumulators of the caller. Helper of 0x13DFDC.
Cluster of 1. [O]
#### 0x0013E4AC `saf_smooth_filter_f` — library
Generic float smoothing step: weighted average `(a*new + b*old[+b]) / (a+b)` with
gain `a` from the calibration byte (0 → 1), returning the clamped u16-domain result —
first-order low-pass used on the E-block speed channels. Called by 0x13E5FC and 0x13F484.
Cluster of 1. [O]
#### 0x0013E4FC `saf_eblock_init` — logic
Initializes the E-block (ratio-plausibility) object: code bytes to 0xFF, two sub-object
bytes with inverted copies, the u16 raw-invalid defaults at +0x102…+0x134, the three enable
booleans (+0x135/136/137) derived from bits 4/2/1 of the mode byte at `0x13e6cc+0x18`, and
the float sub-object via 0x13DDC4, finishing with inverted copies. Called by gatherer
0x132980. Cluster of 1. [O]
#### 0x0013E5FC `saf_eblock_cycle` — logic
Main E-block cycle (1428 instrs, checkpoint 0xC1; called by 0x14F34C): refreshes status/
speed u16s + float mirrors from calibration objects (speed clamp 16000), smooths three
speeds via 0x13E4AC (int twin via `PTR_FUN_0013ea3c`), steps two debounce counter pairs,
runs the element-match table lookup 0x13FB6C, the integer ratio checks 0x13FD3A/0x13FDE6
(→ booleans +0x136/+0x137) and the direction gate +0x135, packs the three booleans into the
3-bit code +0x139 (change-detect latches reported through monitor functors), votes them via
0x13FEE0, then performs a massive final verification — float/int agreement of all mirrored
fields plus every (value, ~value) pair and the two 0x13FA3A/0x13DEB0 checks — ending in the
fatal handler + halt on any mismatch; checkpoint 0xC1 again. Cluster of 1. [O]
#### 0x0013F470 `saf_eblock_refresh2` — stub
Refreshes the (value, ~value) byte pair at +0x14/+0x18 from the sub-object byte at
`*(+8)+0x28` and invokes functor `DAT_0013f598` on the +0x1C sub-block. Called by 0x140E52.
Cluster of 1. [O]
#### 0x0013F484 `saf_eblock_gather_f` — logic
Alternate gather front-half of the E-block (called by 0x133C4E): first runs the integer
gather 0x13F7AC, then loads the float mirrors (+0x74…+0xE4) from the same calibration
objects (with the complementary-pair check and clamp of +0x7C), smooths the three speeds
through 0x13E4AC and steps the +0x40 debounce pair, rewriting all floats via the setter
functor. Cluster of 1. [O]
#### 0x0013F79E `saf_chkpt_0xc1_pass1` — stub
Program-flow checkpoint report `(monitor_obj@0x13f87c, 1, 0xc1)` — companion of checkpoint
0xC1 in 0x13E5FC. Called by 0x148DBA. Cluster of 1. [I]
#### 0x0013F7AC `saf_eblock_gather_i` — logic
Integer-domain gather of the E-block: status/speed u16s from the calibration objects with
inverted copies, speed clamp at 16000, three speed smoothings through the integer filter
0x13FFFA (gains from sub-object bytes +0x2C/+0x30/+0x34/+0x38/+0x3C/+0x40), mode bytes
+0x13B/+0x13C, debounce counter pair +0x14, full setter-functor rewrite. Called by
0x13F484. Cluster of 1. [O]
#### 0x0013FA3A `saf_eblock_pairs_chk` — logic
Integrity sweep of ~13 (value, ~value) pairs of the E-block object (speeds, debounce
counters, mode bytes, result code +0x139/+0x13A), returning a boolean through the setter
functor; helper of 0x13E5FC's final verification. Cluster of 1. [O]
#### 0x0013FB6C `saf_elem_match5_int` — logic
Integer twin of 0x13DFDC: for the five element ratios at +0xF8…+0x100 runs the per-element
check 0x13FF94 (indices 4…0, expected-state byte and measured u16 from the sub-object);
when all five match (mask 0x1F) for the +0x0C debounce period, stores the u16 from table
`0x6489E` indexed by the match mask into +0x10C (the plausibility reference), else reloads
the counter and zeroes +0x10C. Called by 0x13E5FC. Cluster of 1. [O]
#### 0x0013FD3A `saf_ratio_dev_chk_c` — logic
Integer ratio check: requires mode bytes clear, non-zero +0x10C reference and speed above
the u16 at sub-object +0x24, computes `(+0x11A * 1000) / +0x10C` and compares the deviation
from +0x11C against the thresholds at +8/+0xC, latching boolean +0x136. Called by
0x13E5FC. Cluster of 1. [O]
#### 0x0013FDE6 `saf_ratio_dev_chk_d` — logic
Twin of 0x13FD3A against +0x11E with thresholds at +0x10/+0x14 and a +0x14 debounce-pair
gate, latching boolean +0x137. Called by 0x13E5FC. Cluster of 1. [O]
#### 0x0013FEE0 `saf_vote3_byte` — logic
Byte-domain twin of 0x13E362: pairwise AND of booleans +0x135/+0x136/+0x137 into
+0x13D/+0x13E/+0x13F and the all-three AND into +0x140, with setter-functor rewrite.
Called by 0x13E5FC and at init 0x13E4FC. Cluster of 1. [O]
#### 0x0013FF94 `saf_elem_match_chk_i` — logic
Integer per-element plausibility check (twin of 0x13E434): bands `0x2134 ± tol` and
`0 ± 500` around the measured u16; element index → bit (`1 << (idx & 0x1F)`); adds the bit
weight to the caller's match/total accumulators only when the expected-state bit agrees.
Helper of 0x13FB6C. Cluster of 1. [O]
#### 0x0013FFFA `saf_smooth_filter_i` — library
Integer weighted-average smoothing step `(a*new + b*old[+b]) / (a+b)` (gain `a` = 0 → 1) —
the integer twin of 0x13E4AC used by 0x13E5FC (`PTR_FUN_0013ea3c`) and 0x13F7AC for the
speed channels. Cluster of 1. [O]

## Unit summary

119 clusters, 123 named functions (4 two-member clusters: 0x13BC68/0x13BC76,
0x13BC92/0x1485CA, 0x13C352/0x14B442, 0x13C864/0x13C880).

Counts by kind:
- `logic` 62 — conditioners, monitors, shifter evaluator, E-block ratio plausibility
- `data-accessor` 40 — getters via ROM pointer tables, field copies
- `stub` 19 — checkpoint reports, small wrappers, field inits
- `library` 2 — float/int smoothing filters (0x13E4AC, 0x13FFFA)
- `unknown` 0 (no entry resisted classification; 30 entries carry [I] markers where the
  signal identity is inferred from table adjacency/callers rather than observed semantics —
  notably the `saf_get_s16_*`/`saf_get_u8_*`/`saf_shf_get_ceXX` getter families)

Unknown entries: none by kind; honest-[I] clusters worth bench confirmation:
0x13C8F4/0x13C8FE/0x13C956 (pedal-block adjacency), 0x13CA9A (code 4/5 conditioner),
0x13DC6A/0x13DCE4 (position/direction decode), 0x13BB60/0x13BB66/0x13BF32/0x13BF38/
0x13BF3E/0x13BF46/0x13C38A-0x13C3A0/0x13CA90 (unnamed signal getters).

Module map established for this slice:
- 0x13A350–0x13BA5C: safety-layer speed/ratio object (init 0x13A3F8, cycle 0x13A616,
  situation eval 0x13A9D4, gear/lever matcher 0x13B74E); speeds `x*3000/(teeth<<2)`.
- 0x13BA5C–0x13CD14: per-signal conditioners + getters + one-shot fault-report monitors
  (engine temp/pedal/engine-speed/engine-torque T3 per spec 07 §7; torque 0x13C51A).
- 0x13CC44–0x13D9EC: **shifter evaluator** (spec 14 §5a): 0x13CD80 core, decode table
  0x646D8, source select 0x13DB34, output latch 0x13D7BC, getters → E95D/E964.
- 0x13DC6A–0x13DEB0: position/direction decode + E-block object init/verify.
- 0x13DFDC–0x13FFFA: **E-block ratio-plausibility cycle** (0x13E5FC, checkpoint 0xC1):
  dual float/int smoothing, 5-element ratio match vs table 0x6489E, ratio-deviation checks,
  3-input voting, full inverted-copy/float-int verification.

Cross-region leads:
- r14_app units: members 0x001485CA (`saf_cond_bool__2`) and 0x0014B442
  (`saf_obj_get_byte6__2`) live there; also the safety-layer cycles 0x14F34C, 0x14F7AA/CC/EA,
  0x148DBA and gatherers 0x132980/0x132D8E/0x151C3E that drive this slice.
- 0x77B90/0x77D90/0x144108 (outside this unit) consume the shifter-evaluator getter block
  0x13CC84–0x13CD0C — worth naming there with the `saf_shf_*` context.
- Tables: lever decode table `0x646D8` (spec 14) and element-match table `0x6489E` are in
  the calibration area; cal-catalog cross-check recommended.
- Spec cross-refs used: 07 §7 (signal conditioning), 13 §2 (fault reporting API),
  14 §2/§5a (park inputs, shifter evaluator).
