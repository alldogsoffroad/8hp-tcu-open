# Unit 44 (r14_app)

Upper half of the safety/monitoring layer (0x140000–0x14B4xx), per spec 13 §2
(`0x135xxx–0x14Exxx`) and spec 14 §5–§6. Conventions used throughout this unit:
- **report()** calls go through interface slot tables in flash (0x4B8xx–0x4B9xx → e.g.
  0x4B980→`0xA0E70` report(group,data,failed,tested), spec 13 §2). Group numbers come from
  identity byte tables (0x631E1+N==N, second table 0x632A6, third 0x632E1), resolved here by
  reading the ELF.
- **Program-flow monitor (PFM)** checkpoints: `0x148E92` (check-in, id+11000000) and
  `0x148EB6` (check-out, 11999999−id), gated by slot 0x4C720 (spec 13 §3).
- **RAM cell self-tests** 0x148ACC/0x148B6C/0x148C78 (0xAA/0x55 write-read-back, restore,
  fatal on failure) are sprinkled over every local at function end.
- **Fatal reaction**: 0x794C8 (via slot 0x4B9C0) + 0x7A914 (slot 0x4B9BC) + infinite loop.
- Safety objects keep **value+complement** copies of nearly every byte.

#### 0x00140050 `shifter_eval_get_flag_ac` — data-accessor
Returns the status byte at 0xFFF8E9C8 (object 0xFFF8E91C + 0xAC) — the safety-layer shifter
evaluator object of spec 14 §5a. Called from app/park modules 0xA7C9C (park DID handler),
0xF471C, 0xB9646, 0xF7298, 0x133740, 0x1534FC, 0x151C8C. Single-member cluster. [I]

#### 0x00140058 `shifter_eval_get_flag_ad` — data-accessor
Returns the status byte at 0xFFF8E9C9 (shifter evaluator +0xAD); sole caller 0xF471C.
Single-member cluster. [I]

#### 0x00140060 `shifter_eval_get_flag_ae` — data-accessor
Returns the status byte at 0xFFF8E9CA (shifter evaluator +0xAE); callers 0xA7C9C, 0x1350DC,
0x1534FC, 0xB9646. Single-member cluster. [I]

#### 0x00140068 `shifter_eval_get_flag_b0` — data-accessor
Returns the status byte at 0xFFF8E9CC (shifter evaluator +0xB0); callers 0xF7298, 0xA7C9C,
0xB9646. Single-member cluster. [I]

#### 0x00140070 `shifter_eval_get_flag_b2` — data-accessor
Returns the status byte at 0xFFF8E9CE (shifter evaluator +0xB2); callers 0x144108 (this unit,
diag string builder) and 0x77C54. Single-member cluster. [I]

#### 0x00140078 `safety_check_complement32` — library
Verifies the u32 complement pair at p (+0 value, +4 inverted copy); on mismatch calls the
fatal pair 0x794C8(code via slot 0x4B940→byte 1) and 0x7A914 then spins forever. Used by
0x1404C8 on the monitor's timeout counter pair (obj +0x24/+0x28). Single-member cluster. [O]

#### 0x001400A0 `mon_g152_float_chan_calc` — logic
First (float) channel of the dual-channel value computation: fills object floats +0xc (mode
0/1/2), +0x10 (value), +0x14 (sum, clamped to the float at DAT_001402d0), +0x18 (mode) from
calibration flags 0xFFF8ED20[+0xd6]/[+0xd4], default cal [+0xc3] and the two redundant input
bytes (input struct +4/+8, reached via *(obj+0)). Ends with RAM tests (0x148ACC) on all
locals. Called by the module step 0x1402F0. [I] (spec 13 §2 dual-channel safety style)

#### 0x001401C2 `mon_obj_init_defaults` — logic
Initialises monitor-object bytes +0x30..+0x3A to defaults (request=2, mode=2 with inverted
copies, flow counters +0x37/38 and +0x39/3a cleared to 0/0xff), copies the u16 at input+0x10
into the complement pair +0x24/+0x28, and clears +0x1d. Called by init 0x151C9C and by
0x132980 (safety-layer reset writer, spec 14 §6 style). [O]

#### 0x00140212 `mon_flow_checkin_35` — logic
Cyclic program-flow check-in: while flow counter (obj+0x37, complement +0x38) < 0x19, reports
PFM checkpoint 0x35 through 0x148E92 (obj 0xFFF95FE8) and advances the counter; a second,
one-shot check-in of +0x39 is gated by RAM flag 0xFFF96059. RAM tests on the four counter
bytes. Called by init scheduler 0x14F34C. [O] (spec 13 §3)

#### 0x0014029E `mon_flow_checkpoint_35_pair` — stub
Single call 0x148D84(0xFFF95FE8, 1, 0x35): reports checkpoint 0x35 as an immediate
in/out pair (ids 11000053 / 11999964). Called from init dispatcher 0x148DBA. [O]

#### 0x001402F0 `mon_g152_step` — logic
Main cyclic step (from task scheduler 0x14F7EA): verifies calibration complements at
0xFFF8ED20[+0xc2..0xd6] (fatal otherwise), runs the float channel 0x1400A0 and byte channel
0x1405FA, cross-verifies byte results +0x30/+0x32 against the float-channel values (fatal on
mismatch — diverse-redundant computation), runs timeout supervision 0x1404C8, restarts the
+0x39 flow counter, and emits +0x36 = qualified value + 10 (0xfe when −1). Returns 2 when both
mode bytes are 2. [O]

#### 0x00140416 `mon_g152_init_qualify` — logic
Init-time qualification (from 0x151C9C): reads the two input bytes; when flag +0x2f==0 keeps
manual defaults (+0x2c/+0x2d), else derives +0x34 (clamped sum via 0x154956, mode 2) and
+0x30 = second input; writes complements +0x31/+0x33 and modes +0x35/+0x32. RAM tests on all
touched locals. [I]

#### 0x001404C8 `mon_g152_timeout_report` — logic
Timeout/enable supervision: on trigger (+0x1c) reloads the complemented countdown +0x24/+0x28
from input u16 (input+0x10); decrements it (via 0x140078) while the enable condition — timer
value input+0xc below thresholds from cal structs 0xFFF8E904(+4,+8) / 0xFFF8E7A0(+0x48,+0x4c)
and flag 0xFFF85336 — holds; then runs the four fault reporters 0x14072C (group 152),
0x1407BE (132), 0x140868 (133), 0x140948 (101). [O]

#### 0x001405FA `mon_g152_byte_chan_calc` — logic
Second (byte) channel: computes +0x30 (value), +0x32 (source 0/1/2), +0x34 (sum saturated to
0xff), +0x35 (0/1/2) from cal flags 0xFFF8ED20[+0xd5]/[+0xd3], the two input bytes and the
manual default +0x2c. Validates the object through 0x9B864(obj+0x20, *0xFFF8EF57|0xfffe) and
fatals unless it returns 'Z'. RAM tests on locals. [O]

#### 0x0014072C `mon_g152_report` — logic
report(152, data, failed, tested=1) through slot 0x4B9D4→0x4B980→0xA0E70. Group 152 =
P0710/P0712/P0713/P0714 (ATF temperature sensor circuit, 1034420267_fault_groups.csv). Data
byte defaults to 13 (identity table 0x632F3) and is replaced by the mode mux
0x153AC6(0xFFF95FE9, obj+0x2e) for modes 1/2/4; failed = (mode != 0); mode −0x80 keeps the
default. RAM tests on locals. [O]

#### 0x001407BE `mon_g132_report` — logic
report(132, 0, failed, 1) via slot 0x4B9DC (group 132 = P1DA9-00): failed = (byte at
0xFFF8EDE8 == 1) with qualifier (obj+0x2e == 0); the second argument pair comes from slot
0x4B9D4 (152). RAM tests on locals. [O]

#### 0x00140868 `mon_g133_report` — logic
report(133, stat, cond, 1) via slot 0x4B9E0 (group 133 = P077E-00): enable = cal
0xFFF8ED20[0xcc]==[0xcd]==1; secondary condition requires timer value (input+0xc) below the
thresholds from 0xFFF8E904(+4>>2)/0xFFF8E7A0(+0x48), obj state +0x1d==1, cal[200]==0 and
obj+0x2e==0. RAM tests on locals. [O]

#### 0x00140948 `mon_g101_report` — logic
report(101, 0, failed, 1) via slot 0x4B9D8 (group 101, no DTC rows in fault_groups.csv —
aggregate/functional monitor): queries 0xA0FC4 (slot 0x4B994) for groups 152 (slot 0x4B9D4),
122 (0x4B9EC) and 133 (0x4B9E0); if any query returns 0 the failed byte is set. RAM tests on
locals. [O]

#### 0x001409F4 `get_flag_f95408_5` — stub
Returns *(0xFFF95408 + 5) through the +5 getter 0x140A02. Called by 0x144108 (this unit) and
0x77C54. Single-member cluster. [I]

#### 0x00140A0A `tmr_age_window_check` — logic
Wrap-around tick-age check on the timer-supervision object {ptr,threshold@+4,flag@+5}: with
threshold != 0xff, sets flag=1 when now(*ptr)+0x28 < threshold, clears it when
(now − timestamp(*(ptr+4)) + 0x28) > threshold, holds in between. Callers 0x133740, 0x14F848.
[I]

#### 0x00140A4C `if_get_flag_f953c4_7` — stub
Interface entry exposed through slot 0x4B898: returns byte +7 of the object at 0xFFF953C4 via
getter 0x140A5A. Single caller 0xA7C9C (park DID handler). [O]

#### 0x00140A5A `get_u8_off7` — data-accessor
Two-instruction getter: returns *(u8*)(param+7). Cluster of 3: 0x140A5A (behind interface
slot 0x4B898), 0x148FF6 (behind slot 0x4B894, wrapper 0x148FE8, object 0xFFF953D0),
0x14B448 (called by 0x14B420). Duplicated helper copies, same name. [O]

#### 0x00140A60 `tmr_reload_from_src` — stub
Reloads the countdown byte at obj+4 from the source byte *(*(obj)+4). Called by 0x132980.
[I]

#### 0x00140A6A `tmr_step_mode` — logic
Cyclic step of the timer-supervision object: when running flag +6 is set, counts byte +4 down
(0xff = hold) and on reaching 0 sets +7=0 with result 2; when not running, reloads +4 from the
source and samples bit 0 or bit 1 of the source byte (mode +5 = 1 or 2; 0/3 = no test),
writing result 6 to +8 when the bit is set. Called by 0x132D8E and init 0x14F34C. [I]

#### 0x00140AE0 `get_flag_f8e9fe` — stub
Returns the status byte at 0xFFF8E9FE via pool pointer 0x140D18. Callers 0x1534FC, 0xADE6E,
0xA97CA. Single-member cluster. [I]

#### 0x00140AE8 `shifter_eval_get_flag_e3` — data-accessor
Returns the status byte at 0xFFF8E9FF (shifter evaluator object 0xFFF8E91C + 0xE3). Widely
used: callers 0x6D3F4, 0x6D1B0, 0x77E92, 0x7ACF2, 0xA7C9C, 0xB912C, 0xF7298, 0x134F24,
0x1534FC, 0x55AE4. Single-member cluster. [I]

#### 0x00140AF0 `shifter_eval_get_flag_e5` — data-accessor
Returns the byte at 0xFFF8EA01 (shifter evaluator +0xE5); callers 0xA7C9C, 0x55AE4. [I]

#### 0x00140AF8 `shifter_eval_get_flag_e7` — data-accessor
Returns the byte at 0xFFF8EA03 (shifter evaluator +0xE7); callers 0xA7C9C, 0x55AE4. [I]

#### 0x00140B00 `shifter_eval_get_flag_e8` — data-accessor
Returns the byte at 0xFFF8EA04 (shifter evaluator +0xE8); caller 0x55AE4. [I]

#### 0x00140B08 `safety_check_complement32__2` — library
Second compiled copy of the u32 complement check (pool 0x140D2C–34): fatal handlers
0x794C8/0x7A914 + spin unless *(u32)p == ~p[1]. Used on the timeout pairs of the 0x140B80
module. [O]

#### 0x00140B30 `mon2_init_defaults` — logic
Initialises the second monitor object's complement pairs: +0x26/+0x27, +0x28/+0x29,
+0x2b/+0x2c, +0x2d/+0x2e, +0x1c/+0x1d, +0x23 self-complement, defaults 0xff at +0x2c/+0x30,
and +0x21 = byte at 0xFFF82F78. Called by the safety-layer reset writer 0x132980. [O]

#### 0x00140B74 `mon2_flow_checkpoint_3e_pair` — stub
Reports PFM checkpoint pair id 0x3e via 0x148D84(0xFFF95FE8, 1, 0x3e). Called from init
dispatcher 0x148DBA. [O]

#### 0x00140B80 `mon2_step_3e` — logic
Main step of the second monitor module (PFM checkpoint 0x3e; called by cyclic 0x14F34C and
0x133C4E): verifies complement pairs (+0x1c/+0x2b region), reloads/decrements the timeout
counter pair [1]/[2] from input byte (input+0xc), qualifies the input code via table lookup
0x151DAC(*(input+4), +0x26) into +0x1f (7 = invalid), runs qualification 0x140F18 and the
state machine 0x140FCC plus the qualifier transition 0x140EB0 on +0x2b (states 1–4), then for
state 1 fans out through 0x140E52 / 0x140E86, for state 3 through 0x140E94; result +0x2a =
state in {1,2,3}. Checkpoint check-out gated by 0xFFF9604F. RAM tests on ~17 locals. [O]

#### 0x00140E52 `mon2_notify_consumers` — logic
Fan-out after a state-1 qualification: calls (object, function) pairs — (0xFFF8303C,
0x135BA8), (0xFFF8E7A0, 0x1434FC), (0xFFF8ECE0, 0x1424BA), (0xFFF952D0, 0x137B2E),
(0xFFF9536C, 0x13936C), (0xFFF8EB94, 0x13F470), (0xFFF95750, 0x136952) — i.e. notifies seven
downstream monitor objects. [O]

#### 0x00140E86 `mon2_mark_changed_objs` — logic
Sets byte +0x1c = 1 on the two RAM objects 0xFFF8E998 and 0xFFF95858 (changed/trigger
markers for downstream consumers). [O]

#### 0x00140E94 `mon2_notify_139374` — stub
Single indirect call 0x139374(0xFFF9536C) — notify one consumer object on state 3. [O]

#### 0x00140E9C `mon2_reload_timeout18` — logic
Reloads the u32 complement pair [3]/[4] from the input byte at *(input+0x10). Called by
0x1420C8. [O]

#### 0x00140EAC `mon_g151_report_thunk` — stub
Tail-thunk of 0x141126 (same body): report(group 151 = P1DDE-00, data 1, tested, failed).
Entered from the cyclic scheduler 0x14F7AA. [O]

#### 0x00140EB0 `mon_qual_state_step` — logic
Generic qualifier state transition on a complemented state byte: {2: flag? keep : 3},
{4: flag==1 ? 1 : keep}, {1: 2}, {3: 4}, {0: flag==1 ? 1 : 4}; returns the new state. Applied
by 0x140B80 to both the value and its inverted copy. RAM tests on the two locals. [O]

#### 0x00140F18 `mon2_qualify_code` — logic
Valid flag (+0x2f, complement +0xc) = 0x151DAC(*(input+4), code +0x26) when gate +0x28==0 and
code != 7 (invalid marker), else 0. Called from 0x140B80. [I]

#### 0x00140FCC `mon2_state_machine` — logic
Two-phase state machine on +0x22 (0 = qualified, 1 = waiting, 2 = running with timeout pair
[5]/[6] reloaded from input+0x18): uses qualification +0x1f and the +0x1e flag; publishes the
result byte +0x21 to the global complement pair 0xFFF82EF4[0x84/0x85] and, on change, calls
0x14115A(0xFFF83042); ends with 0x14EBA6(0xFFF82EF4) and an inline RAM test on +0x23.
[O]

#### 0x00141126 `mon_g151_report` — logic
report(151, data 1, tested = (obj+0x21 == 2), failed = (obj+0x28 != 0 or obj+0x21 != 2))
through slot 0x4C6E8→0x4B980→0xA0E70. Group 151 = P1DDE-00 (fault_groups.csv). No direct
callers (reached via vector); the thunk 0x140EAC is the scheduler entry. [O]

#### 0x0014118C `get_flag_f9570b` — data-accessor
Returns the flag byte at 0xFFF9570B; caller 0xB912C. [I]

#### 0x00141194 `get_flag_f9570c` — data-accessor
Returns the flag byte at 0xFFF9570C; caller 0xB912C. [I]

#### 0x0014119C `get_u16_f9570e` — data-accessor
Returns the u16 at 0xFFF9570E; callers 0xA7C9C (park DID handler), 0x55AE4. [I]

#### 0x001411A2 `obj_reload_from_ptr` — logic
Stores *(byte*)(obj+4) = byte pointed to by *(u32*)obj — generic reload helper used by the
safety-layer reset writer 0x132980. Cluster of 2 (0x1411A2, 0x148FFC — identical copy). [O]

#### 0x001411AC `tbl_decode_code_lookup` — logic
Decode-table step: for code byte +5 (!= −1) looks up u16 = 0x151E70(0x64984, code) into +0xa
and byte = 0x151DD8(0x64964, code) into +0x7, clearing +0xd/+0xc; for an invalid code sets
+0xd=1, +0xc=2, +0x7=0xff; disabled (+6 with +1) path sets +0xa=0xffff. Also counts byte +1
down when running flag +6 is set. Tables 0x64984/0x64964 sit next to the lever decode table
0x646D8 (spec 14 §5a). Called by cyclic 0x14F34C. [I]

#### 0x00141252 `qual_simple_gate` — logic
Small qualifier: when enable byte +1 == 0 passes the input byte to +2 and clears +3/+4;
otherwise +3 = 2 and +2 = 0. Called by cyclic 0x14F34C. [I]

#### 0x0014126C `get_flag_f8ea14` — data-accessor
Returns the flag byte at 0xFFF8EA14; caller 0xB912C. [I]

#### 0x00141274 `get_flag_f8ea15` — data-accessor
Returns the flag byte at 0xFFF8EA15; caller 0xB912C. [I]

#### 0x0014127C `get_flag_f8ea17` — data-accessor
Returns the flag byte at 0xFFF8EA17; caller 0xB912C. [I]

#### 0x00141284 `get_flag_f8ea18` — data-accessor
Returns the flag byte at 0xFFF8EA18; caller 0xB912C. [I]

#### 0x0014128C `get_flag_f8ea1a` — data-accessor
Returns the flag byte at 0xFFF8EA1A; callers 0xA7C9C, 0xB912C. [I]

#### 0x00141294 `mon3_init_defaults` — logic
Initialises a third monitor object's complement bytes (defaults 0xff at +9/+0xb/+0xf/+0x13/
+0x15/+0x19/+0x1b/+0x1d, pairs {2,0xfd} at +0xc, {1,0xfd} at +0x11, {1,0xfe} at +0x17) and
finishes with interface calls 0x7B1FE(1) / 0x7B206(0xfe) (slots 0x4C70C/0x4C710). Called by
the reset writer 0x132980. [O]

#### 0x001412F4 `mon3_flow_checkpoint_47_pair` — stub
Reports PFM checkpoint pair id 0x47 via 0x148D84(0xFFF95FE8, 1, 0x47); from init
dispatcher 0x148DBA. [O]

#### 0x00141300 `mon3_step_47` — logic
Third module step (PFM checkpoint 0x47; from cyclic 0x14F34C and 0x133C4E): runs enable
calculation 0x1415DA, state qualification 0x141346 and flag calculation 0x141680, then
reports the two result bytes through 0x7B1FE(+0x17) / 0x7B206(+0x18); check-out gated by
0xFFF9605B. [O]

#### 0x00141346 `mon3_state_qualify` — logic
279-instr state qualifier: fatals unless the complement pairs +0xd/+0xe, +0x1b/+0x1c,
+0x10/+0x11 hold; with mode gate *(0xFFF8E9FF)==2 derives state +0x1b in {0..7} from the
enable +0x12, gates +6/+0xa, and the code pair (+8, +4) (code 0x0f → 1, (3,1)→3, (4,1)/(1,1)
→4, (2/9,1)→5, else 6; any gate set → 7; disabled → 2); then maps the state to the triple
(+0xf, +0xc, +0xd) and refreshes complements. Verifies the object's own complement pairs
(+4/+5, +8/+9, +6/+7, +0xa/+0xb) and the global pair 0xFFF8E9D4[0x2b/0x2c]; 17 RAM tests.
[O]

#### 0x001415DA `mon3_enable_calc` — logic
Enable calculation: +0x1d = (byte at input2 + offset-from-slot-chain 0x4C714 == 0xAA);
+0x17 (complement +0x1c? no, param_1[6]) is 1 when the first input byte is 1 or the 0xAA
match holds (with +0x12 = 1), else 0 with +0x12 = 0. Verifies pairs (+4/+5, +6/+7). RAM
tests on four bytes. [I]

#### 0x00141680 `mon3_flags_calc` — logic
Derives two result flags: +0x19 = (code +8 == 2 and +0xa == 0 and +0x12 == 1), +0x1a = (code
+8 == 1 and +0xa == 0 and +0x12 == 1). [O]

#### 0x001416D8 `get_flag_f96040` — data-accessor
Returns the flag byte at 0xFFF96040; callers 0x144108 (this unit), 0x77C54. [I]

#### 0x001416E0 `get_flag_f96041` — data-accessor
Returns the flag byte at 0xFFF96041; callers 0x144108, 0x77C54. [I]

#### 0x0014171A `mon_g117_report_qual` — logic
Group-117 report (U0401-00, fault_groups.csv): gated by 0xFFF8EA1A == 1; qualifier call
0xA031C(sub-code 36, slots 0x4BE6C/0x4B9A4) must not return 1; sets the object's +4/+5
qualification bytes and finishes report(117, data 1, tested = (+3 & 1), failed =
(*0xFFF8E9FE == 1)) via slot 0x4B980. Called from 0x14F784. [O]

#### 0x00141798 `get_flag_f95473` — data-accessor
Returns the flag byte at 0xFFF95473; callers 0xA0D1A, 0xAE5A0. [I]

#### 0x001417A0 `get_flag_f95474` — data-accessor
Returns the flag byte at 0xFFF95474; caller 0xA0D8A. [I]

#### 0x001417A8 `get_flag_f95476` — data-accessor
Returns the flag byte at 0xFFF95476; callers 0xAE5A0, 0xA0CF4. [I]

#### 0x001417B0 `get_flag_f95477` — data-accessor
Returns the flag byte at 0xFFF95477; callers 0xA0D98, 0xADC1E. [I]

#### 0x001417B8 `mon3_set_init_15` — stub
Setter used at reset: stores the byte at 0xFFF82F56 into object byte +0xf. Called by
0x132980. [O]

#### 0x001417C0 `qual_pass_or_invalid` — logic
Two-way hand-off: when gate +4 == 0 passes the qualified inputs +5..+8 to +0xb..+0xe
(+0xc forced to 0xff when +9 == 1) and clears +0x10/+0xa; otherwise marks all four outputs
invalid (2 / 0xff). Then runs the latch check 0x141826. Called from cyclic 0x14F7EA. [O]

#### 0x001417FC `safety_latch_event` — logic
Sets object byte +0xf = 1 and, unless already latched (0xFFF82EF4[0x62] == 1 or flag
0xFFF96068), latches the global safety-status struct 0xFFF82EF4 (bytes [0x62] = 1, [99] =
0xfe) and notifies consumer 0x1418A8(0xFFF83042). Called from 0x14870A. [O]

#### 0x00141826 `safety_latch_threshold` — logic
Latch check used by 0x1417C0: sets +0xf = 1 and latches 0xFFF82EF4[0x62]/[0x1f...] with the
same notify when the input value *(*(obj)) is below the u32 threshold at 0xFFF956FC (gate
0xFFF8EE76 == 0) or below *(u32*)(0xFFF956F0+8) (gate +0x10 == 0). [I] — reads like a
"value below limit → set one-shot event" supervision.

#### 0x001418E2 `mon4_select_result` — logic
Selects result byte +7 (1/2/3): mode +8 == 1 requires 0xFFF8E9FF == 2 and then checks speed
inputs against the u16 at 0xFFF96066 and thresholds from 0xFFF8E7A0(+0x48/+0x4c) and flag
0xFFF9584A; mode 2 maps through 0xFFF8E9D4[0x2b] (4→1); other modes give 1/3. Steps object
0xFFF95838 first via 0x14DC70. Called from cyclic 0x14F34C. [I]

#### 0x001419B4 `if_wrap_f95410_a` — stub
Interface wrapper exposed through slot 0x4B880: returns 0x1419D0(0xFFF95410). Cluster of 2
— the twin 0x1419C2 is the same wrapper for slot 0x4B884 calling 0x1419D6 (second field of
the same object). Callers 0xA7C9C. [O]

#### 0x001419DE `qual_gate_5` — logic
Small qualifier: when enable +1 == 0 passes +2 = (input == 5) with +3 = 0, else +2 = 0 with
+3 = 2 (invalid). Callers 0x133476, cyclic 0x14F34C. [O]

#### 0x00141A08 `get_flag_f8e788_9` — stub
Returns 0x141A16(0xFFF8E788) — a field getter on the object at 0xFFF8E788; caller 0xB912C.
[I]

#### 0x00141A1E `mon5_flow_checkpoint_fff0_pair` — stub
Reports the PFM checkpoint pair with the unusual id 0xFFF0 via 0x148D84(0xFFF95FE8, 1,
0xFFF0); from init dispatcher 0x148DBA. [O]

#### 0x00141A2C `park_req_safety_step` — logic
Safety-layer park-request producer (spec 14 §5): PFM checkpoint 0xFFF0; from object
0xFFF8EA38 writes the park request byte to +8 = 0xFFF8EA40 (D61A[0x2E], the P1B14 failure
byte) and its status to +9 = 0xFFF8EA41 (2 = invalid). Source select by the gate *(*(obj)):
value 1 in +4 → request 1, 7 → request 0 with status 2; otherwise, when +7 == 0, the request
is taken from +6. RAM tests on the touched bytes. Callers 0x132D8E, cyclic 0x14F34C. [O]
(spec 14 §5, §7)

#### 0x00141AD4 `park_req_get` — data-accessor
Returns the safety-layer park request byte at 0xFFF8EA40 (= park object D61A[0x2E], spec 14
§5). Callers 0xA7C9C, 0xF471C, 0x55AE4, 0xB912C. [O]

#### 0x00141ADC `park_req_status_get` — data-accessor
Returns the park-request status byte at 0xFFF8EA41 (2 = input invalid, spec 14 §5). Callers
0xF471C, 0x55AE4. [O]

#### 0x00141AE4 `get_flag_f8ea42` — data-accessor
Returns the flag byte at 0xFFF8EA42 (next byte of the park-request object). Callers 0xA7C9C,
0x1350DC, 0xF471C, 0x55AE4, 0xB912C. [I]

#### 0x00141AEC `get_flag_f8ea43` — data-accessor
Returns the flag byte at 0xFFF8EA43. Callers 0xF471C, 0x55AE4. [I]

#### 0x00141AF4 `safety_check_complement32__3` — library
Third compiled copy of the u32 complement check (pool 0x141CC0–C8) with the standard fatal
reaction. Used on the timeout pair of the 0x141BA2 module. [O]

#### 0x00141B1C `mon5_init_ff` — logic
Reset-time initialisation: sets eight object bytes (odd offsets +0xd..+0x1d) to 0xff.
Called by 0x132980. [O]

#### 0x00141B34 `mon5_flow_checkpoint_89_pair` — stub
Reports PFM checkpoint pair id 0x89 via 0x148D84(0xFFF95FE8, 1, 0x89); from 0x148DBA. [O]

#### 0x00141B42 `mon5_step_89` — logic
Module step (PFM checkpoint 0x89; callers cyclic 0x14F34C, 0x133C4E): runs the status
qualifier 0x141BA2, then ORs bits 4/8 into +0x1e when the pairs +0xc/+0x1f and +0x10/+0x20
disagree (with +0xe / +0x12 == 0). Check-out gated by 0xFFF96050. [O]

#### 0x00141BA2 `mon5_status_bits_qualify` — logic
247-instr qualifier: fatals on broken complement pairs (+0x18/+0x19 region); assembles a
status byte +0x1c bit-by-bit (bit0 from +0xe, bit1 from +0x12, bit2 = +0xc&+0x10 both zero,
bit3 from +0x14, bit0x10 from the timeout pair [1]/[2] counting down from *(*(in+4)));
result +0x18 in 0..3 and +0x1a in {0,2} from the gate pair (+0xc,+0x10); refreshes five
complements, 17 RAM tests, and verifies the six complement pairs (+0xc/+0xd,
+0x10/+0x11, +0xe/+0xf, +0x12/+0x13, +0x14/+0x15). [O]

#### 0x00141E06 `mon_g98_source_check` — logic
Fault-group 98 monitor (P0850-00 / P07B2-00 / P07B8-00, fault_groups.csv): reads a status
word via 0xA0FE8 (slot 0x4B990) into a local, picks a source byte from the identity slots
(**0x4B96C = 12, **0x4B970 = 13, **0x4B9B8 = 0) according to bits of the status byte +0x1c,
verifies mode-dependent change flags (+0x1e bits 4/8/0xc) and finishes report(98, source,
failed, tested) via slot 0x4B980→0xA0E70. When +0xe == 0 copies +0xc/+0x10 to +0x1c/+0x20.
Called from cyclic 0x14F7EA. [O]

#### 0x00141FCC `get_f8ef44_via_1fe8` — stub
Wrapper: returns 0x141FE8(0xFFF8EF44). Cluster of 2 — twin 0x141FDA calls 0x141FEE on the
same object. Callers 0xA7C9C/0x55AE4 and 0xB912C. [I]

#### 0x00141FF6 `mon6_flow_checkpoint_fff9_pair` — stub
Reports PFM checkpoint pair id 0xFFF9 via 0x148D84(0xFFF95FE8, 1, 0xFFF9); from 0x148DBA.
[O]

#### 0x00142004 `mon6_step_fff9` — logic
Module step (PFM 0xFFF9; callers 0x132D8E, cyclic 0x14F34C): qualification flag +4 =
(*(0xFFF953BE) == 1 and input byte == 0x0c); +5 = 0; special case (+3 == 0 and +2 == 1)
sets both to 1. RAM tests on six bytes. [O]

#### 0x001420A0 `get_flag_f9582c` — data-accessor
Returns the flag byte at 0xFFF9582C; caller 0xA7C9C. [I]

#### 0x001420A8 `get_flag_f9582d` — data-accessor
Returns the flag byte at 0xFFF9582D; caller 0x7ACF2. [I]

#### 0x001420B0 `get_flag_f9582e` — data-accessor
Returns the flag byte at 0xFFF9582E; caller 0xA7C9C. [I]

#### 0x001420B8 `mon6_req_wrapper` — stub
Argument-passing wrapper: 0x1420C8(0xFFF95820, param). No direct callers in edges.csv —
reached through a pointer/vector. [O]

#### 0x001420C8 `mon6_req_decode` — logic
Request decode on object 0xFFF95820: request byte 1 → +0xf = 1, +6 = 10, return 2; request 2
or 0x82 → mask from 0x13854A(0xFFF958C4) and, when (*(input+8) & mask) == 0, reloads the
timeout pair of the 0xFFF8E9D4 object via 0x140E9C and returns 2; otherwise 0. [I]

#### 0x0014210E `mon7_notify_once_thunk` — stub
Thunk of 0x14222E (same body): one-shot event notify. Called by the reset writer
0x132980. [O]

#### 0x00142112 `mon7_seq_step` — logic
Sequence supervisor step (callers 0x132D8E, cyclic 0x14F34C): copies enable 0xFFF8E9D4[0x2a]
to +0x12, arms +0x10; state +0x13: state 0 waits for +0x12 (else latches 0xFFF85337 = 1);
state 1 runs the phase machine 0x142246, tracks +0xb edge history in globals 0xFFF952AC /
0xFFF95FC8, clears +0xc/+0xd/+0x11 on success, fires the one-shot notify 0x761C8 (slot
0x4BE90) and calls 0x13A102(0xFFF95904) when 0xFFF85337 was set; also decrements timer +6
under gate 0x79764 (slot 0x4BA60). [O]

#### 0x0014222E `mon7_notify_once` — logic
One-shot notify: when +0xe == 0 calls the event interface 0x761C8 (slot 0x4BE90) and sets
+0xe = 1. No direct callers (pointer-invoked; also thunked at 0x14210E). [O]

#### 0x00142246 `mon7_phase_seq_sm` — logic
Eight-state two-phase timed sequence: state 0 loads u16 timer [1] from *(input+4); state 1
counts down, holding while **(input) <= cal *(u16*)(0xFFF8EE00+0x6e) and cal[0x76] != 2 and
gate 0xFFF8E9F2 == 0; states 2/3 wait for +0xa and release the notify latch (+0xe=0); state
4 waits for +0xb == 0; state 5 loads u16 timer [2] from *(input+0xc); state 6 counts down
and writes +0xd = 1 or 2 (from +4); state 7 holds. [I]

#### 0x0014236C `safety_check_complement32__4` — library
Fourth compiled copy of the u32 complement check (pool 0x14260C–14), fatal reaction
identical. Used by 0x143344, 0x142A20, 0x1425C4, 0x142FBA. [O]

#### 0x00142394 `mon7_init_defaults` — logic
Reset-time init (caller 0x132D8E): 0xff/0xa5 defaults, five u32 complement pairs [1..10]
loaded from input byte pointers (input+0, +4, +0x14, +0x24, +0x28) and two u16 copies from
calibration 0xFFF8EEA0(+0x7c/+0x80). [O]

#### 0x0014240C `mon8_step_8a` — logic
Module step (PFM checkpoint 0x8a; from cyclic 0x14F34C): selects outputs +0x33/+0x34 from
+0x31/+0x32 with the fallback +0x2c/+0x30 when the source holds sentinel 0xA5 (-0x5b), then
runs seven sub-steps 0x142FBA (g92), 0x1424EE (g94), 0x1425C4 (g48), 0x142A20 (g164),
0x143344 (g40), 0x142ED0 (g97), 0x142F76 (g96); RAM tests on six bytes. [O]

#### 0x001424AC `mon8_flow_checkpoint_8a_pair` — stub
Reports PFM checkpoint pair id 0x8a via 0x148D84(0xFFF95FE8, 1, 0x8a); from 0x148DBA. [O]

#### 0x001424BA `mon8_init_defaults` — logic
Init for the g92..g164 module (called by the fan-out 0x140E52 and reset writer 0x132980):
clears +0x35 (complement 0xff), timeout pair [15] = 0/0xff, and loads pairs [5]/[6],
[7]/[8] from the first two input bytes. [O]

#### 0x001424EE `mon_g94_report_mux` — logic
report(94, data, failed, tested) via slots 0x4C764/0x4B980: data byte from the mode mux
0x153AC6(0xFFF95FE9, *0xFFF8EE82); tested = (mode == 0 and data equals **0x4B954 (6) or
**0x4B944 (3)); failed = (result +0x33 == 0). [O]

#### 0x00142544 `mon_g106_range_report` — logic
report(106, 0, tested, failed) (group 106 = U1412-00): tested = cal 0xFFF95588[0x31] == 1;
failed = (cal[0x18] == 0 and the window holds: **(in+0xc) < obj+0x2e < **(in+0x10)); also
RAM-tests the u16 at obj+0x2e (slot 0x14265C → 0x148B6C). Called from 0x14F7CC. [O]

#### 0x001425C4 `mon_g48_report_qual` — logic
417-instr qualified reporter (group 48): builds three enable paths from cal objects
(PTR_DAT_00142660[0x13f/0x140], 0x142668[0x12]) and a speed condition
(**(in+0x2c) <= cal+0xe, cal[0x12] < 2, *0x142700 == 2); decrements the timeout pair [1]/[2]
(reloaded from **(in+0x28)) only while enabled; issues up to three report3+debounce pairs
(0xA105E via 0x4B9B0, 0xA107C via 0x4B9B4) with data bytes **0x429AC/0x429BC/0x429C0-slot
chains; finishes with a cascade of ~12 complement-pair verifications (including cal objects
0x1429C4/C8/CC/D0) and 8 RAM tests. Fatal on any mismatch. [O]

#### 0x00142A20 `mon_g164_report_qual` — logic
Bigger twin of 0x1425C4 for group 164: adds change detection (*0x142B98 vs *0x142B9C),
enable gates 0x142BA8/B8/BB0/B4, the same speed condition via cal 0x142BBC, plus upper-bound
speed checks against 0x142BC0 and 0x142BC4(+0x14/+0x16) and state *0x142BC8 != 4; timeout
pair [3]/[4] from **(in+0x24); report3+debounce pairs via slots 0x4C770/0x4B9B0/0x4B9B4;
complement cascade (0x142E4C..0x142E58) and 12 RAM tests. [O]

#### 0x00142ED0 `mon_g97_report_cond` — logic
report(97, data, failed, tested) via 0x4C778/0x4B980: failed path needs cal 0xFFF8EB94
[0x13e] == 1 and *0x1430D8 == 0; tested requires speed **(in+0x18) < obj+0x2e, result +0x33
== 0, cal[0x140] == 0, 0x1430E0 gates clear, mode 0x1430E4[0x37] != 4 with [0x35] == 1, and
**(in+0x1c) below thresholds 0x1430DC(+0x80) / 0x1430E8(+0x14/+0x16). [O]

#### 0x00142F76 `mon_g96_report_cond` — stub-like logic
report(96, data, failed, tested) via 0x4C77C/0x4B980: failed = cal 0xFFF8EB94[0x13d] == 1;
tested = (*0x1430F4 == 0 and +0x33 == 0 and cal[0x140] == 0). [O]

#### 0x00142FBA `mon_g92_timeout_qual` — logic
346-instr qualifier for group 92: fatals unless the object pairs (+0xf/+0x3d, +0xe/+0x3a,
+0x33/+0xd) and cal pairs (0x1430FC+0x7c/0x7e, 0x1430D0+0x10c/0x13f) hold; timeout pair
[7]/[8] (reloaded from **(in+4)) drives latch +0xf; second pair [5]/[6] (reloaded from
**(in)) gates the plausibility of the value read through 0x153AC6 (0x43320/0x43324 slot
chains); report3+debounce (0x4B9B0/0x4B9B4) with tested = (+0x33 == 0); copies the cal u16
into +0xe/+0x3a; 15 RAM tests. [O]

#### 0x00143344 `mon_g40_timeout_qual` — logic
Smaller qualifier for group 40: fatals on cal pairs 0x1434D4[0x13f..0x144] and
0x1434D8[0x7e/0x82] and object pairs (+0x33/+0xd, +0x35/+0x36); timeout pair [9]/[10]
reloaded from **(in+0x14) while cal[0x13f] == 1, *0x1434DC == 0 and the cal u16 (+0x10c) is
nonzero; report3+debounce with tested = all gates clear; 3 RAM tests. [O]

#### 0x001434FC `spd_eval_init_arm` — logic
Arms the two-channel speed-evaluation module: on first entry (+0x1c == 0) initialises the
two output pairs (+0x26/+0x27, +0x4e/+0x4f) and flags; then writes the channel parameter
blocks (+2 and +0xc) via 0x14366E with the input pointers and u16s. Callers 0x132980 and
0x140E52 (fan-out). [O]

#### 0x0014356A `spd_eval_step_5e` — logic
Speed-evaluation module step (PFM 0x5e; callers 0x133946, cyclic 0x14F34C): per channel
(param+2, param+0xc) runs 0x143686 (speed calculation) and 0x1438A2, reads nine input values
(bytes/u16 via the input struct), then runs the plausibility window check 0x1438BC on both
channels and the verdict selectors 0x14393A / 0x1439F8. RAM tests on three words. [O]

#### 0x00143662 `spd_flow_checkpoint_5e_pair` — stub
Reports PFM checkpoint pair id 0x5e via 0x148D84(0xFFF95FE8, 1, 0x5e); from 0x148DBA. [O]

#### 0x0014366E `spd_chan_set_params` — logic
Writes a channel parameter block: byte +0, u16s at +2/+4/+6 (numerator, denominator, min),
offset byte at +0x1d. Used by 0x1434FC for both channels. [O]

#### 0x00143686 `spd_chan_calc_rpm` — logic
Speed calculation from a pulse stream: with count +0xc < 2 holds the last value (mode 0); 2
.. 0x47 computes (60000000 / pulses_per_rev (+0)) / timespan (+8) scaled by u16 +4 / u16 +2;
>= 0x48 uses the 30 ms-window formula ((count*30000000 + 0xfe363c80)/ (timespan * pulses))
etc., latches +0x1e/+0x1f and on a zero timespan records the overflow into the safety-status
struct via 0x14EEA2(0xFFF82EF4, 0x5e, 1, count). Result +0x1a, clamped to 0 below the u16
minimum +6, offset by byte +0x1d, stored at +0x18. 22 RAM tests over the whole channel
struct. [O] — this is a classic frequency→RPM evaluator.

#### 0x001438AC `spd_eval_snapshot` — logic
Copies the channel results: byte +0x1c → +0x26 and u16 +0x18 → +0x24 (snapshot of verdict +
speed). Callers 0x133946, 0x14F850. [O]

#### 0x001438BC `spd_plaus_windows` — logic
Plausibility check of the computed speed (+0x16) against two (value, tolerance) pairs
(param_2/3 and 4/5): computes the event frequency +0x22 = (count−1)*1000000/timespan (Hz),
and when it is <= param_6 (or the value is 0) classifies +0x20 = 1 (inside window 1), 2
(inside window 2) or 0 (outside both). [O]

#### 0x0014393A `spd_verdict_gated` — logic
Verdict selector +0x21 in 0..5 with gear/range gating: 3 = no speed; 4 = outside both
windows; otherwise passes +0x20 through only when consistent with the gear state
0xFFF8E7A0[4] (5/6/7), vehicle-speed compare (param_7 vs +0x18) and the qualifier +0x1c —
else 5. [I]

#### 0x001439F8 `spd_verdict_simple` — logic
Simpler verdict selector +0x21 in 0..5 (no gear gating): 3 = no speed, 4 = outside both
windows, 5 = inconsistent, else copies +0x20 (0/1/2). Returns the previous verdict. [O]

#### 0x00143A80 `get_f8e74c_via_ad2` — data-accessor
Wrapper returning 0x143AD2(0xFFF8E74C). Cluster of 3 — twins 0x143A8E (0x143AD8) and 0x143A9C
(0x143ADE) read further fields of the same object. Callers 0xF7298, 0xA7C9C, 0xDA388,
0x55AE4, 0xB912C. [I]

#### 0x00143AAA `safety_check_complement32__5` — library
Fifth compiled copy of the u32 complement check (pool 0x143CC0–C8). Used by 0x143B42 and
neighbours. [O]

#### 0x00143AE4 `mon9_init_defaults` — logic
Init for the 0x143B42 module: defaults 0xff/2 at +0x2e/+0x2f/+0x32/+0x36, +0x37 = byte at
0xFFF82F08, and complement pairs [1]/[2] (byte **(in)), [3]/[4] (u16 **(in+0x14)), [5]/[6]
(u32 **(in+0x20)), [7]/[8] (byte **(in+0x28)). Called by 0x132980. [O]

#### 0x00143B34 `mon9_flow_checkpoint_fff6_pair` — stub
Reports PFM checkpoint pair id 0xFFF6 via 0x148D84(0xFFF95FE8, 1, 0xFFF6); from 0x148DBA.
[O]

#### 0x00143B42 `mon9_step_fff6` — logic
296-instr module step (PFM 0xFFF6; callers 0x132D8E, cyclic 0x14F34C): timeout pair [1]/[2]
from **(in) gated by 0xFFF8ECD2; with enable **(in+4) == 1 runs a state machine on byte [9]
(0..7): state 0 checks the 6/6 pair (+0x29, +0xa) and sets +0x3b; state 1 latches
0xFFF82EF4[0x12] = 1 (complement 0xfe) with notify 0x1440D6(0xFFF83042); states 2..7 set
+0x37 (1/2/4/7) and release the latch; results +0x2d (0/1) and +0x2f (0/2) with complements.
Then runs 0x143E1A, 0x143FA4, 0x143F08 and closes with 25 RAM tests. [O]

#### 0x00143E1A `mon9_qualify_result` — logic
Sub-step of 0x143B42: timeout pair [7]/[8] (reload from **(in+0x28) unless state +0x29 == 6);
with the override +0x25 clear and state [9] < 8 looks the result up via
0x151DD8(*(in+0x18), state) into +0xe (else +0x37 = 1/2 invalid); copies to +0x33 and, when
flag 0xFFF96068 is clear, publishes +0x37 to the safety-status pair 0xFFF82EF4[0x14/0x15]
with change notify 0x1440D6(0xFFF83042). [O]

#### 0x00143F08 `mon9_qualify_hold` — logic
Sub-step: countdown pair [5]/[6] (u32 from **(in+0x20)); sets hold flag +0x39 while state
+0x37 == 3 with the 6/6 combination (+0x29, +0xa) and +0x3b == 1; otherwise reloads and
clears +0x39/+0x3b. [O]

#### 0x00143FA4 `mon9_qualify_window` — logic
Sub-step: countdown pair [3]/[4] from u16 **(in+0x14); sets +0x35 = 1 while state +0x37 == 3,
gated by (+0xb != 0 or **(in+0x24) == 0) and the countdown below the reload value. [O]

#### 0x00144012 `mon_g165_report_speed` — logic
report(165, data, failed, tested) via 0x4CBD4/0x4B980: tested = (+0x31 == 1 and state [10]
in {5,7}); failed = (cal 0xFFF8E9D4[0x2b] == 2 and speed **(in+0x10) < +0x26). Called from
cyclic 0x14F7EA. [O]

#### 0x001440F0 `get_flag_f956de` — data-accessor
Returns the flag byte at 0xFFF956DE; callers 0xAD6D4, 0xAD80C. [I]

#### 0x001440F8 `get_flag_f956df` — data-accessor
Returns the flag byte at 0xFFF956DF; caller 0xF7298. [I]

#### 0x00144100 `get_flag_f956e0` — data-accessor
Returns the flag byte at 0xFFF956E0; caller 0xF7298. [I]

#### 0x00144108 `diag_did_safety_handler` — dispatch
The safety layer's ReadDataByIdentifier data handler (spec 07 §7): 5322 instructions of
per-identifier response building. Calls every getter of this unit (0x140070, 0x1409F4,
0x1416D8/0x1416E0, 0x14140F0-F8 …), the measurement helpers 0x9B5AC..0x9B6A6 (spec 10), the
report family 0xA0DF0..0xA46xx, its own packers 0x147CBA/0x147D28/0x147DC4/0x148050/
0x148102/0x148248 and byte/u16 emitters in 0x766CC..0x777D2. Documented scalings: DID 0x51F
= engine speed x4 (0.25 rpm steps, saturating 16383 rpm), DID 0x515 = lever code 1-4 →
range codes 8-5. Entered via 0x132142. [O] (spec 07 §7, spec 10)

#### 0x00147BEC `diag_wdid_dispatch_27a_27b` — dispatch
WriteDataByIdentifier dispatcher for the safety layer: DID 0x27A → 0x147C22, DID 0x27B →
0x147C7A; anything else returns 0 (not handled). Called from 0x132156. [O]

#### 0x00147C22 `diag_wdid_27a_handler` — logic
UDS write handler for DID 0x27A with negative response codes: session/state +0xbc != 1 →
0x33 (security access); mode +0xbd != 3 → 0x7f; value 9..0x0e → 0x31 (out of range);
+0xbf != 0 → 0x22 (conditionsNotCorrect); else stores the value to +0xc0 and returns 1.
[O]

#### 0x00147C7A `diag_wdid_27b_handler` — logic
UDS write handler for DID 0x27B: same session/mode gates (0x33/0x7f); accepts values < 2
into +0xbf (bit flag), else 0x31. [O]

#### 0x00147CBA `did_status_bits_a` — logic
Builds a DID status byte from six sources: RAM flags 0xFFF8EDEB (bit 6), 0xFFF8EF08 (bit 1),
0xFFF8EE76 (bit 0) and bool interface calls through slots 0x4C42C (bit 5), 0x4C430 (bit 4),
0x4C434 (bit 2). Used by the DID handler 0x144108. [O]

#### 0x00147D28 `did_status_bits_b` — logic
Builds a DID status byte with two 2-bit fields: field A (0x20/0x30/(x&3)<<4) from slots
0x4C438/0x4C43C, field B (0x80/0xC0/x<<6) from slots 0x4C440/0x4C444. Used by 0x144108. [O]

#### 0x00147DC4 `diag_vin_string_build` — logic
Assembles the 17(15)-byte identification string for the DID handler: 'T' + "01" + a
2-byte model/plant code (via 0x151DAC-style lookup, default from flash 0xEC0A when both
reads return -1) + a 4-digit build-date code (from 0x147F12, default 'FFFF') + marker byte
+ 5 tail bytes ("70201" region of the constant at 0xE01B). Returns length 14. [I] — VIN /
plant identification builder.

#### 0x00147F12 `diag_builddate_parse` — library
Parses the 8-digit ASCII build date in flash (0xE000 + 0x925/0x935): validates all digits
(0x1481F8), year 2000..3000, month 1..12, day < 32; converts to (month-1)*30 + day with the
month-length offsets (+1 Feb/Jun/Jul, +2 Aug, +3 Sep/Oct, +4 Nov/Dec) and a leap-year
February +1 (year%400 or year%4 and not year%100). Returns 0xFFFF when invalid. [O]

#### 0x0014803A `mon_clear_flag_bf` — logic
Clears object byte +0xbf when the session byte +0xbc != 1 (diag write permission removal).
Called from cyclic 0x14F34C. [O]

#### 0x00148050 `did_loop_test_writer` — logic
DID helper (from 0x144108): for param_4 iterations writes the pattern (param_3 − i) into RAM
0xFFF85339, sets up the transfer block (+0xa0..+0xb4, length 1, mode 0x50) and triggers
0xA9774 (mode 1) or 0xA97A6 (mode 9); collects per-iteration success bits (struct +0xac
result, +0xb0 bit 6) into a 4-byte result. [I] — channel-wise routine/test trigger.

#### 0x00148102 `did_ratio_table_pack` — logic
DID helper: iterates entries (list via slot 0x4C3FC, value/max via 0x4C400/0x4C404) up to 10
records; each 6-byte record packs the entry id, the u16 value and value/limit scaled by
8192.0 with a cap of 8.0 (floats 0x41000000/0x46000000). Writes back pointer and total
length (count*6). [O]

#### 0x001481F8 `lib_isdigits` — library
Checks that all param_3 bytes at param_2 are ASCII digits '0'..'9'; returns 1/0. [O]

#### 0x00148248 `did_dualchan_compare` — logic
DID helper for the redundant-channel comparison: for channel pairs (0..4) x (0/1) compares
0x151D98(obj+0x14 field, i, j) against the getters via slots 0x4C424/0x4C41C (objects
0x4C4D8/0x4C4DC/0x4C4E0/0x4C4E4/0x4C4E8); combines the match flag with the overall value
(< 100, via slot 0x4C418) into a result 0..3. [I]

#### 0x00148404 `get_s16_f8e794_4` — stub
Returns *(s16*)(0xFFF8E794 + 4) via the +4 getter 0x148412; caller 0xF7298. [I]

#### 0x00148412 `get_s16_off4` — data-accessor
Two-instruction getter *(s16*)(p+4). Cluster of 2: 0x148412 (behind 0x148404) and 0x14E282
(called by 0x14E274). [O]

#### 0x00148416 `spd_chan_init_300` — logic
Writes the default channel parameters: u16 +4 = 300, u16 +6 = 300, u32 +8 = 100 (counts/
timescale defaults). Called by the reset writer 0x132980. [O]

#### 0x00148424 `mon10_flow_checkpoint_ffef_pair` — stub
Reports PFM checkpoint pair id 0xFFEF via 0x148D84(0xFFF95FE8, 1, 0xFFEF); from 0x148DBA.
[O]

#### 0x00148432 `mon10_step_ffef` — logic
Module step (PFM 0xFFEF; callers 0x132D8E, cyclic 0x14F34C): computes a percentage [2] =
u16(0xFFF8EE6E)*100 / u32(0xFFF955B4); a threshold u16 +6 from cal 0xFFF952D0(+0x6c/+0x6e)
via 0x1548D8/0x154992 comparing [2]*10000 against **(in+4)*10000; then builds the output
u16 [1] from 0x138242/0x13824C products (/10000, /10) with mode gates 0xFFF8E74C[0x2d/0x2f/
0x31] and 0xFFF8EE76/0xFFF955B8 pass-through; RAM tests on the three words. [I]

#### 0x001485B4 `get_f953d8_via_5c2` — stub
Wrapper returning 0x1485C2(0xFFF953D8); caller 0xB912C (app safety-value gatherer, spec 07
§7). [I]

#### 0x001485F0 `uds_obj_get_via_640` — stub
Status getter wrappers on the safety-layer diagnostic service object 0xFFF95420. Cluster of
3 identical wrappers: 0x1485F0 (0x148640, caller 0xB912C), 0x1485FE (0x148646, callers
0x144108, 0xA7C9C, 0x14FC5C, 0x55AE4), 0x14860C (0x14864C, caller 0xB912C). [I]

#### 0x0014861A `uds_service_entry_wrap` — stub
Argument-forwarding entry into 0x148696 (request pointer/length + mode): called from
0x81D46 — the boot/diag interface entry into the safety-layer service handler. [O]

#### 0x0014863A `uds_state_reset_wrap` — stub
Calls 0x14889E(0xFFF95420): clears the service-object state bytes +0x22/+0x23/+0x25 (drops
the 0xAA authorisation marker). Callers 0x76022, 0x75F86. [O]

#### 0x00148654 `uds_step_and_channel_set` — logic
Cyclic step of the service object (callers 0x133476, 0x14F34C): runs the service state
machine 0x1488DC, then when the pending control +0x2d == 1 and mode [10] is 3/4 finds the
target channel index via 0x148A96 (table at *(in+8)) and writes it through
0x151D98(*(in+8), idx, 1), storing the result at +0x2e. [O]

#### 0x00148696 `uds_service_dispatch` — dispatch
Service dispatcher on the object at 0xFFF95420: stores request pointer (+0x14) and length
(+0x18), copies the service id to +0x10; mode 1 (control) → 0x14870A, mode 2 (status) →
0x148844, mode 3 (read/build response) → 0x14896E; other → NRC 0x12. Null request returns
clean. [O]

#### 0x0014870A `uds_ctrl_dispatch` — dispatch
Control half of the dispatcher, by service id at +0x10: 0x236 (len 3, value 1 → interface
0x4CC04, else 0x31); 0x311 (len 3, value 1/2 → sets the authorisation marker +0x25 = 0xAA
and +0x22 = 0xC0); 0x312 → 0x148A02 (control bit +0x2d); 0x3013 (len 2 → slot 0x4B9E4);
0xFF02 (len 2; when *(0xFFF956F8) < 0x51 first slot 0x4CC00) which also triggers the safety
latch 0x1417FC(0xFFF95468); 0x9E0A (len 4; 0xFFFE → +0x2c = 1, 0xFFFF → +0x2c = 0, else NRC
0x22). Unknown → 0. [O]

#### 0x00148844 `uds_status_dispatch` — dispatch
Status half: 0x311 (len 2 → +0x23 = 1), 0x312 → 0x148A1E (clears +0x2d), 0x236/0x3013/
0xFF02/0x9E0A → NRC 0x12, unknown → 0. [O]

#### 0x001488AC `uds_find_channel_tbl4` — library
Linear search (max 32 entries) for the index whose entry (read via 0x151D98(*(in+4), i, 0))
equals param_2; returns 0x1f when not found. [O]

#### 0x001488DC `uds_service_state_step` — logic
Service state machine on +0x22/+0x23: with no active marker sets +0x25 = 0xAA when the
first request byte is 1; state +0x23 handles the acknowledged 0x311 and clears on completion;
mode byte [1] == 4 performs the channel write via 0x1488AC + 0x151D98(*(in+4), idx, 1);
mode 2 sets +0x22 = 0xC0 and returns 2. [O]

#### 0x0014896E `uds_read_dispatch` — dispatch
Response-building half: 0x311 (len 2) packs six bytes (+0x1a..+0x1f) from the request
(including +0x24 for code 4); 0x312 → 0x148A30 (eight bytes incl. the u16 at +0xe and +0x2e
for code 4); 0x236/0x3013/0xFF02/0x9E0A → NRC 0x12. Writes record pointer/length out. [O]

#### 0x00148A02 `uds_312_ctrl_set` — logic
Service 0x312 control (len 2): stores +0x2d = (request byte +0xb == 1); else NRC 0x31. [O]

#### 0x00148A1E `uds_312_status_clear` — logic
Service 0x312 status (len 2): clears +0x2d; else NRC 0x31. [O]

#### 0x00148A30 `uds_312_read_build` — logic
Service 0x312 response: packs eight bytes (+0x1a..+0x21) from +0xa/+0xc/+8/+0xe, with +0x2e
appended for code 4. [O]

#### 0x00148A96 `uds_find_channel_tbl8` — library
Linear channel search like 0x1488AC but over the table at *(in+8) (max 32, miss = 0x1f). [O]

#### 0x00148ACC `ramtest_byte_aa55` — library
RAM-cell self-test used on stack locals at function exit: writes 0xAA, verifies, writes
0x55, verifies, restores the original byte; any failure calls the fatal pair
0x794C8/0x7A914 (slots 0x4B9C0/0x4B9BC via pool 0x148C64) and spins. 67 callers across the
safety layer. Cluster of 2 (0x148B1E is an identical second compiled copy, pool 0x148D70).
[O]

#### 0x00148B6C `ramtest_word_55aa` — library
16-bit variant of the RAM cell test (pattern 0xAA55/0x5555), same fatal reaction (pool
0x148C64 for the first copy, 0x148D74 for the twin 0x148BC2). 34 callers. [O]

#### 0x00148C14 `ramtest_dword_2` — library
Third variant of the dword RAM test with its own pattern pool 0x148C70/74 (0xAAAAAAAA /
0x55555555); fatal reaction identical. 7 callers (0x143686, 0x148432, …). [O]

#### 0x00148C78 `ramtest_dword_aa55` — library
Dword RAM-cell test with patterns from pool 0x148D7C/80 (0xAAAAAAAA/0x55555555); same fatal
reaction. 18 callers. [O]

#### 0x00148D18 `ramtest_word_55aa__2` — library
Isolated 16-bit RAM-test copy with its own pool (0x148D70/74); only caller 0x14EBC2. [O]

#### 0x00148D84 `pfm_report_checkpoint_pair` — library
Reports one program-flow checkpoint as an in/out pair through slot 0x4C71C→0x7A6E0 with
ids (param_2, param_3 + 11000000) and (param_2, 11999999 − param_3, param_4). Used by the
checkpoint-pair stubs of every module in this unit. 32 callers. [O] (spec 13 §3)

#### 0x00148DBA `safety_init_dispatch` — logic
Init dispatcher (caller 0x14F32C): invokes ~35 object functions via (DAT, FUN) pool pairs
(0x148EE0..0x148FD8) — the module init/arm entries, including this unit's checkpoint-pair
stubs and init steps. [O]

#### 0x00148E92 `pfm_checkpoint_in` — library
Program-flow check-in: if the monitor is armed (slot 0x4C720 → 0x68EA0), reports id +
11000000 through slot 0x4C718 → 0x7A520 with the monitor object 0xFFF95FE8. 32 callers;
spec 13 §3. [O]

#### 0x00148EB6 `pfm_checkpoint_out` — library
Program-flow check-out: armed-gated report of 11999999 − id. 34 callers; spec 13 §3. [O]

#### 0x00148FE8 `if_get_flag_f953d0_7` — stub
Interface wrapper (slot 0x4B894): returns byte +7 of the object at 0xFFF953D0 via 0x148FF6.
Caller 0xA81D8. [O]

#### 0x00149006 `tmr_reload_or_count` — logic
Timer step: when not running (+6 == 0) reloads byte +4 from *(*(obj)) and sets +7 = (+5 == 1);
otherwise counts +4 down (−1 = hold) with +7 = 0. Callers 0x132D8E, cyclic 0x14F34C. [I]

#### 0x0014904C `safety_check_complement32__6` — library
Sixth compiled copy of the u32 complement check (pool 0x14920C–14). Used by 0x1490C6. [O]

#### 0x00149074 `mon_reload_pair_refresh` — logic
Reloads the complement pair [1]/[2] from the input byte *(*(obj)) and refreshes the
complement of [3] into +0xd. Called by 0x132D8E. [O]

#### 0x0014908A `mon11_step_e0` — logic
Module step (PFM 0xe0; from cyclic 0x14F34C): runs 0x1490C6; check-out gated by 0xFFF9605D.
[O]

#### 0x001490B8 `mon11_flow_checkpoint_e0_pair` — stub
Reports PFM checkpoint pair id 0xe0 via 0x148D84(0xFFF95FE8, 1, 0xe0); from 0x148DBA. [O]

#### 0x001490C6 `mon_g95_timeout_flags` — logic
150-instr sub-step for group 95: timeout pair [1]/[2] from *(*(obj)); mode flags from byte
[3] bits 4/0x20/0x40; when the timeout has expired and a flag is set, reports report3 +
debounce (0xA105E via 0x4B9B0 / 0xA107C via 0x4B9B4, group slot 0x4C7B8) with data byte
**(0x4B95C) or **(0x4B958); 7 RAM tests; verifies the [3]/+0xd complement pair. [O]

#### 0x00149244 `get_f9601a_via_49252` — stub
Wrapper returning 0x149252(0xFFF9601A); caller 0xA7C9C. [I]

#### 0x0014925A `mon_copy_flag_e785` — logic
Copies the global byte at 0xFFF8E785 into the object byte +0 (reset writer helper; callers
0x132D8E, 0x14F34C). [O]

#### 0x0014926C `get_flag_f8ea70` — data-accessor
Returns the flag byte at 0xFFF8EA70; callers 0xA7C9C, 0xF471C, 0x55AE4. [I]

#### 0x00149274 `get_flag_f8ea71` — data-accessor
Returns the flag byte at 0xFFF8EA71; callers 0xF471C, 0x55AE4. [I]

#### 0x0014927C `safety_check_complement32__7` — library
Seventh compiled copy of the u32 complement check (pool 0x1494D0–D8). Used throughout the
0x149324 module. [O]

#### 0x001492A4 `mon12_reload_all` — logic
Reloads the four complement pairs [1]/[2] (byte **(in+0x10)), [3]/[4] (byte **(in+0x14)),
[5]/[6] (u16 **(in+0x18)), [7]/[8] (byte **(in+0x1c)) and sets +0x25 = 0xff. Called by the
reset writer 0x132980. [O]

#### 0x001492E4 `mon12_flow_checkpoint_b1_pair` — stub
Reports PFM checkpoint pair id 0xb1 via 0x148D84(0xFFF95FE8, 1, 0xb1); from 0x148DBA. [O]

#### 0x001492F2 `mon12_step_b1` — logic
Module step (PFM 0xb1; from cyclic 0x14F34C): runs the big supervision 0x149324 and the
group-53 report 0x1496CC. [O]

#### 0x00149324 `mon12_window_supervise` — logic
381-instr multi-timer supervision: fatals unless the calibration complement pair
0xFFF8EE00(+0x6e/+0x70) and 0xFFF8E890[6/7] hold; reads flags 0xFFF8E898[0x11/0x12] and
0xFFF8E9FF and four input u16s; full reload when 0xFFF8E9FF == 1; three countdown pairs
([1]/[2], [7]/[8], [5]/[6]) armed by windows of the reference value (cal +0x6e) around the
tracked value [10] and the inputs; latches the result byte [9] and its history +0x2a,
re-arms pair [7] on the armed condition (flags + **(in+0x20) + input windows) and releases
on the disarm condition; 15 RAM tests. [I] — pattern of a value-window / gradient
supervisor with four redundant timers.

#### 0x001496CC `mon_g53_report_latch` — logic
report(53, 1, failed, tested=1) via slot 0x4C7BC/0x4B980: failed = (result +0x24 == 1 and
complement +0x25 == 0xFE) — i.e. the latched changed-pair of module 12. [O]

#### 0x0014971C `get_u16_f953dc_via_754` — stub
Field getter wrapper on the object at 0xFFF953DC (u16 via 0x149754); callers 0xF7298,
0xA7C9C. [I]

#### 0x0014972A `get_f953dc_via_758` — data-accessor
Field getter wrapper on 0xFFF953DC (byte via 0x149758). Cluster of 2 — twin 0x149746 reads
another field (0x149762); callers 0xF7298/0xA7C9C and 0xA7C9C. [I]

#### 0x00149738 `get_s16_f953dc_via_75e` — stub
Field getter wrapper on 0xFFF953DC (s16 via 0x14975E); caller 0xA7C9C. [I]

#### 0x0014976A `meas_ratio_blend_calc` — logic
Ratio/percentage calculation with adaptive blending (callers 0x132D8E, 0x14F34C): when gate
+6 == 0 and the raw u16 (+4) is inside 0x1000..0x3FFF, maps (raw−0x1000)×0xA0/0xE through
curve 0x1548CE / scale 0x154992 capped at 0x7FFF into +8; derives a delta ratio (×7/0xA0
×100) passed through 0x1549A0(val, −999, 0x9FB) and blends it with the previous value (+0xe)
using the two input bytes as weights ((new×A + old×B)/(A+B)); invalid raw → +8 = 0 with
qualifiers +0xa/+0xb = 2/1; disabled → 2/2. [I] — reads like a redundant-channel analog
input conditioner.

#### 0x00149858 `mon13_init_defaults` — logic
Reset defaults: pairs +7/+8 and +9/+10 cleared to {0,0xff} and +0xb = byte at 0xFFF82F04.
Called by 0x132980. [O]

#### 0x00149872 `mon13_step` — logic
Cyclic step (from 0x14F7EA): clears the +7/+8 pair, runs the min-tracker 0x149928 and the
sub-steps 0x149964, 0x1499AA, 0x149A38, 0x149AEA, then clears the +9/+10 pair. [O]

#### 0x0014989C `mon13_flow_checkin_68` — logic
Flow check-in for checkpoint 0x68: advances the complemented counters +7 and +9 (max 0x19)
reporting through 0x148E92/0x148EB6 (gate 0xFFF9605A); RAM tests on the four bytes. [O]

#### 0x0014991C `mon13_flow_checkpoint_68_pair` — stub
Reports PFM checkpoint pair id 0x68 via 0x148D84(0xFFF95FE8, 1, 0x68); from 0x148DBA. [O]

#### 0x00149928 `mon13_min_tracker` — logic
Minimum tracker: when +6 == 0 and [0xb] < [5], adopts [5] into [0xb] and publishes the new
minimum to the safety-status pair 0xFFF82EF4[0x10/0x11] with change notify 0x149B7A
(0xFFF83042), gated by 0xFFF96068. [O]

#### 0x00148CC6 `ramtest_byte_aa55__3` — library
Third isolated copy of the byte RAM-cell test (pool 0x148D70–78); callers 0x14EB6E /
0x14EBA6. [O]

#### 0x00149964 `mon_g100_report_mux` — logic
report(100, data, failed, tested=1) via slot 0x4B9C4/0x4B980: data byte selected by the code
at +4 — 8 → **(0x4B96C) = 12, 0x20 → **(0x4B95C) = 8, 0x40 or default → **(0x4B958) = 7
(identity tables); failed = (code != 0). [O]

#### 0x001499AA `mon_g135_report_cond` — logic
report(135, data, cond, 1) via 0x4B9C8/0x4B980: enable = cal 0xFFF8ED20[0xcc] == [0xce] ==
1; tested requires code +4 == 0, speed (input+0xc of object 0xFFF8E998) below the thresholds
from 0xFFF8E904(+4>>2)/0xFFF8E7A0(+0x48), object state +0x1d == 1 and 0xFFF8E9FE == 1.
Twin of the group-133 monitor 0x140868. [O]

#### 0x00149A38 `mon_g131_report_mux` — logic
report(131, data, failed, 1) via 0x4B9CC/0x4B980: data by code +6 — 0x20 → **(0x4B95C) =
8, 0x40 → **(0x4B958) = 7, 0x80 → **(0x4B96C) = 12, default **(0x4B9B8) = 0; failed =
(code != 0). [O]

#### 0x00149AEA `mon_g134_report_cond` — logic
report(134, data, cond, 1) via 0x4B9D0/0x4B980: enable = cal 0xFFF8ED20[0xcd] == [0xce] ==
1; tested requires code +6 == 0 plus the same speed-threshold and object-state gates as
0x1499AA (object 0xFFF8E998, flags 0xFFF8E9FE). [O]

#### 0x00149BA8 `safety_check_complement32__8` — library
Eighth compiled copy of the u32 complement check (pool 0x149E40–48). Used by the 0x149D24
window-tracker family. [O]

#### 0x00149BD0 `mon14_init_floats` — logic
Float-path init of module 14: loads seven configuration bytes (as floats) from the
safety-status struct 0xFFF82EF4[0x76..0x82] into +0x1b..+0x24, copies the byte-path values
(+0xe/+9/+0xc) into the float mirror fields with a ×100.0 scale (0x42C80000), loads three
complemented u16 pairs from input pointers (select between *(in+0x1c)/(in+0x20)/(in+0x28)/
(in+0x2c) by the config bytes) and precomputes the inverted float copies. Called by
0x14A4F0. [O]

#### 0x00149D24 `mon14_step_orchestrate` — logic
Runs the float-channel sub-steps in order — hysteresis 0x149F36, slew limiter 0x149E58,
window tracker 0x149FB2, dual window tracker 0x14A0C4, window tracker 0x14A36E — then
refreshes the five inverted-float complement fields, runs the plausibility combiner
0x14A43A and finishes with 21 dword RAM tests (0x148C78). Called by 0x14A6E8. [O]

#### 0x00149E58 `mon14_slew_limiter` — logic
Rate limiter on the float path: target +0x11 tracks +0xc (raw) with the per-step maximum
taken from the table lookup 0x151DD8(*(in), value); reloads to +0xc×100.0 when the edge
flags (+0xf/+0xe/+0x17/+0x18) fire; output +0x12 = int(result/100.0) & 0xff. [O]

#### 0x00149F36 `mon14_hysteresis_a` — logic
Hysteresis latch on the float path: |+0x15 − +0xc| above the byte at *(in+4) sets +0x17 =
1.0; back below *(in+8) clears it; when clear +0x15 follows +0xc. [O]

#### 0x00149FB2 `mon14_window_tracker_a` — logic
Complemented-u16 window tracker: when |[9] − [0x13]| exceeds the byte at *(in+0x24) the
value snaps to **(in+0x14) (fast path) or, after the [1]/[2] countdown expires, to
**(in+0x18); latch flag +0x1a selects the branch; complements refreshed. [I]

#### 0x0014A0C4 `mon14_window_tracker_b` — logic
Two-window tracker with hysteresis for pairs [3]/[4] and [5]/[6]: inside the small window
(*(in+0x10)) the value reloads immediately; outside the large window (*(in+0xc)) the
countdown runs and finally selects the alternate source (*(in+0x20) vs *(in+0x1c)); state
flags +0x1b/+0x1c. 306 instrs. [I]

#### 0x0014A36E `mon14_window_tracker_c` — logic
Third tracker for pair [7]/[8]: deviation |[9] − [+0xd]| above the byte at *(in+0x30) sets
+0x1e = 1.0; on state change reloads from *(in+0x28) or *(in+0x2c); when the countdown ends
copies +0x1e into +0x1d. [I]

#### 0x0014A43A `mon14_plaus_or` — logic
Plausibility combiner: sets +0x84 (+0x6c & +0x74 both 1.0, or +0x28 ≠ 0), +0x88 (+0x70 &
+0x74, or +0x2c), +0x8c (+0x6c & +0x70, or +0x38) and the majority/any combination +0x90.
[O]

#### 0x0014A4F0 `mon14_init` — logic
Module-14 init (callers 0x132980, 0x133C4E): clears the flow/latch bytes, copies the seven
config bytes from 0xFFF82EF4[0x76..0x82] into the byte mirror fields (+0xcc..+0xd7),
initialises the byte-path history (+0xbe/+0xc4/+0xc6, percent +0xc0 = +0xbc×100), loads the
three complemented u16 pairs from the input struct at +0x94 and finishes with 0x149BD0
(float mirror). [O]

#### 0x0014A64A `mon14_flow_checkin_c0` — logic
Flow check-in for checkpoint 0xc0: advances the complemented counters +0xd9/+0xdb (max
0x19) through 0x148E92/0x148EB6 (gate 0xFFF960xx via pool 0x14A6DC). Called from cyclic
0x14F34C. [O]

#### 0x0014A6C6 `mon14_flow_checkpoint_c0_pair` — stub
Reports PFM checkpoint pair id 0xc0 via 0x148D84(0xFFF95FE8, 1, 0xc0); from 0x148DBA. [O]

#### 0x0014A6E8 `mon14_step` — logic
654-instr main step (from cyclic 0x14F7EA): fatals unless ~12 byte pairs and five float
complement pairs hold; runs 0x14ADB2 (hysteresis on the byte path), 0x14ACF0 (percent slew
tracker), 0x14AE38, 0x14AF70, 0x14B27A, 0x14B34A and the float orchestrator 0x149D24; then
cross-verifies EVERY byte-path result (+0xc8, +0xc9, +0xcc..0xd7, +0xc2) against the
int(float) mirror values — the diverse dual-channel check, fatal on mismatch; publishes
changed config bytes back into 0xFFF82EF4[0x76..0x83] with change notify 0x14B3EE; 29 RAM
tests; validates the input object via 0x14ACE0(obj+0x94, *|0xfffe) == 'Z'. [O]

#### 0x0014ACF0 `mon14_pct_slew_track` — logic
Byte-path percent tracker: +0xc0 tracks +0xbc×100 with the maximum step from
0x151DD8(*(in+0x94), +0xbc); reloads when (+0xbe && !+0xbd) or (!+0xc8 && +0xc9); result
+0xc2 = value/100. Word RAM test at end. [O]

#### 0x0014ADB2 `mon14_hysteresis_b` — logic
Byte-path hysteresis: |+0xbc − +0xc6| above *(in+4) sets +0xc8; back within *(in+8) clears
it; when clear +0xc6 follows +0xbc. RAM tests on the two threshold locals. [O]

## Unit summary

**Unit 44 = r14_app, 0x140050–0x14ADB2: the upper half of the safety/monitoring layer**
(spec 13 §2 "0x135xxx–0x14Exxx"), plus its diagnostic (DID/UDS) interface. 220 clusters,
233 functions named. Every cluster could be classified; **0 `unknown` entries**.

Counts by kind: logic 120, data-accessor 45, stub 39, library 23, dispatch 6, irq 0, entry 0.

Module map established in this unit (each = init + cyclic step + reporters, bracketed by
program-flow checkpoints; spec 13 §3):

- **Program-flow monitor / RAM-test library (0x148ACC–0x148FE8):** 0xAA/0x55 RAM-cell
  self-tests (byte/word/dword, 9 compiled copies), 8 copies of the u32 complement fatal
  check, PFM check-in/out 0x148E92/0x148EB6 and the checkpoint-pair reporter 0x148D84
  (ids +11000000 / 11999999−id), init dispatcher 0x148DBA.
- **Monitor modules** with their PFM checkpoint id: 0x35 (groups 152/132/133/101,
  dual-channel float+byte value arbiter 0x1402F0), 0x3e (0x140B80, fan-out 0x140E52),
  0x47 (0x141300), 0x5e (speed evaluation 0x14356A: pulse→RPM 0x143686 with 60e6/30e6
  windows, plausibility windows, verdicts), 0x68 (0x149872 min-tracker), 0x89
  (0x141B42), 0x8a (0x14240C: groups 92/94/96/97/40/48/106/164), 0xb1 (0x149324 window
  supervisor + group 53), 0xc0 (0x14A6E8: byte/float dual-channel cross-verified
  tracker, config published to 0xFFF82EF4), 0xe0 (group 95 P0642/P0643), and the
  large-id init-phase checkpoints 0xFFF0 (park request 0x141A2C → 0xFFF8EA40/41,
  spec 14 §5), 0xFFF6 (0x143B42), 0xFFF9 (0x142004), 0xFFEF (0x148432).
- **Safety-layer park/lever outputs:** getters for the shifter evaluator object
  0xFFF8E91C and the park request 0xFFF8EA40..43 (spec 14 §5, §7).
- **Diagnostic interface:** ReadDataByIdentifier handler 0x144108 (spec 07 §7: DID 0x51F
  engine speed ×4, 0x515 lever→range), its packers (VIN/built-date 0x147DC4/0x147F12,
  status bitfields, ratio tables), and a UDS service dispatcher (0x148696/0x14870A/
  0x148844/0x14896E) for services 0x236/0x311/0x312/0x3013/0xFF02/0x9E0A incl. NRCs
  0x12/0x22/0x31/0x33/0x7f and the 0xAA authorisation marker.

`unknown` entries: none.

Cross-region leads:
- **Report interface slots live in flash 0x4B880–0x4C7xx** (→ 0xA0E70 report(group,data,
  failed,tested), 0xA105E report3, 0xA107C debounce, group numbers via identity tables
  0x631E1/0x632A6/0x632E1). Units covering 0xA0xxx–0xA4xx and 0x79xxx–0x7Bxxx should
  cross-check the group numbers cited here (98, 100–101, 117, 131–135, 151–152, 165 and
  the report3/debounce pairs for 40/48/53/92/94–97/106/164) against
  1034420267_fault_groups.csv.
- **Speed evaluation (0x14356A family)** feeds verdicts 0–5; its consumers in r10
  (0x133946, 0x132D8E) and the input objects (0xFFF952D0/0xFFF956FC/0xFFF8EE00
  calibration) should be cross-checked by the r10 unit.
- The cyclic schedulers **0x14F34C / 0x14F7EA / 0x14F784 / 0x14F7AA / 0x14F7CC / 0x14F850**
  and reset writer **0x132980** (r10/r14 boundary) drive most entries here; the r10 unit
  covering 0x132980/0x132D8E/0x133xxx owns the other half of those module pairs.
- **0xFFF8E91C (shifter evaluator), 0xFFF82EF4 (safety-status struct), 0xFFF95FE8 (PFM
  object)** are shared with spec 14 §5a/§6 structures — updates to those specs can cite
  this unit's getters (0x140050–0x140070, 0x140AE8–0x140B00, 0x141AD4–0x141AEC).
