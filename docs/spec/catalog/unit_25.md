# Unit 25 (r0c_app)

Region 0x000C4A98–0x000C8D18: park/range module monitor & arbitration block (spec 14 §1:
"monitors in 0xC2xxx–0xC8xxx"; fault-group monitors per spec 13 §2a and
1034420267_fault_groups.csv: groups 113 @0xC5E78, 114 @0xC5E02, 115 @0xC58AA, 153 @0xC78BE,
168 @0xC5C7C, 192 @0xC5912). Naming prefix `park_` throughout. Literal-pool pointers were
resolved against the ELF (helper /tmp/tcu_lit/lit.py) to identify RAM globals and getters.

#### 0x000C4A98 `park_cal_slot_abc_set_flash` — stub
Writes data-flash calibration descriptor pointer 0x001811B8 into park-module calibration
pointer slot 0xFFF84ABC (the group at 0xFFF84AB0 = park arbiter calibration, spec 14 §4.2;
descriptor's first entry points at value 0x191FF8). Called from init caller 0x000A713E.
Single cluster member. [O] spec 14 §4.2.

#### 0x000C4AAC `park_obj_clear_u16_at4` — stub
Zeroes the u16 at param+4 of a park-module object (callers 0x000A790C). 3 instructions.
[O]

#### 0x000C4AB2 `park_range_mask_hold_update` — logic
Maintains a u16 output (param[2]) qualified by a hold flag: when 0xFFF8E550 == 1 the previous
value is kept; otherwise FUN_000c4b12 picks a source (calibration-selected) and FUN_000c4b46
forces 0 while 0xFFF894C8 == 0. Called from the park module driver 0x000A82E0.
[O] spec 14 §3 (derived-input pass).

#### 0x000C4ADC `park_range_mask_calc` — logic
Computes u16 at param+2 = u16@0xFFF8D6C0 AND calibration value read through slot 0xFFF84ABC
(descriptor 0x1811B8 → 0x191FF8), then sets bit 1 when the descriptor's +8 flag byte is 1 and
park input 0xFFF8D623 (D61A[+0x09], lever plausibility) is 1. [I]

#### 0x000C4B08 `park_flag_e550_is_set` — data-accessor
Returns *(0xFFF8E550) == 1 — the "hold last value" flag shared by the range-output rules.
[O]

#### 0x000C4B12 `park_range_mask_source_sel` — dispatch
Selects the mask source via the calibration byte at descriptor+4 of slot 0xFFF84ABC:
value 1 → compute via FUN_000c4adc and return param[1], value 2 → call getter 0x000A7818
(u8@0xFFF8D624), anything else → 0. Writes the selector result to param[0]. [O]

#### 0x000C4B46 `park_flag_d94c8_is_zero` — data-accessor
Returns *(0xFFF894C8) == 0 (used to blank the range mask). [O]

#### 0x000C4B68 `park_cal_slot_ac0_set_rom` — stub
Installs ROM calibration struct 0x0004EDD4 (pointers into 0x196020…) into slot 0xFFF84AC0
of the park calibration group. Called from init variant 0x000A70AA. [O] spec 14 §4.2.

#### 0x000C4B70 `park_cal_slot_ac0_set_flash` — stub
Same slot 0xFFF84AC0 but installs the data-flash-side descriptor 0x001811C4 (whose entries
point at the same values 0x196020…); selected by init variant 0x000A713E. ROM/data-flash
descriptor pair with 0x000C4B68. [O]

#### 0x000C4B84 `park_cal_slot_ac4_set_rom` — stub
Installs ROM struct 0x0004EE28 (values 0x195E20…) into calibration slot 0xFFF84AC4 — the
override/test descriptor read by the range-output chain (FUN_000c4ce6/0x000c4eac/0x000c4ef0).
[O]

#### 0x000C4B8C `park_cal_slot_ac4_set_flash` — stub
Slot 0xFFF84AC4 := data-flash descriptor 0x00181218 (same value targets 0x195E20…); the
flash-side alternative to 0x000C4B84. [O]

#### 0x000C4BA0 `park_res_invcopy_set` — library
Sets result[1] = ~result[0] — refreshes the inverted copy of a value/invcopy pair (park
module convention, spec 14 §1). Called from 0x000A78F2 (safety-layer reset path 0x0015643E).
[O]

#### 0x000C4BA8 `park_res3_clear` — library
Clears a 3-byte result triple to {0, 0xff, 0} (value / inverted copy / qualifier) — the
default-initialized park result object layout. [O]

#### 0x000C4BB6 `park_range_out_arbiter` — dispatch
Priority chain over a 4-byte result {value, ~value, qualifier, reason}: runs rules
FUN_000c4c2c, 4c9e, 4cc8, 4ce4, 4d3c, 4d98, 4dce, 4e5e, 4e24, 4e66 in order (first rule that
fires wins), falls back to FUN_000c4e96 (value 0, reason 160), then applies the test/calibration
override (FUN_000c4eac value, FUN_000c4ef0 qualifier) when 0xFFF8D61B = 1. Produces range
codes 5/6/7/8 (D/N/R/P, spec 14 §4.1) from the park target 0xFFF8D6D6 (getter 0x000A7B78).
Called from park driver 0x000A82E0. [O] spec 14 §4.

#### 0x000C4C2C `park_range_rule_inactive` — logic
Rule (reason 10): when module flags are idle (0xFFF894CC[2]==0, 0xFFF8E54C[2]==1,
[3]==1, override byte calib+8 == 0) → result 0 (no range); the same output is also taken in
the active branch when 0xFFF8D655==1, D94CC[2]==0, getter 0x000A7B6E (u8@0xFFF8D710) == -1
and calib+0 byte == 0. [I]

#### 0x000C4C9E `park_range_rule_state_byte` — logic
Rule (reason 20): when 0xFFF894CC[2] != 0 the range output passes that state byte through
(value, inverted copy, qualifier 1). [I]

#### 0x000C4CC8 `park_range_rule_hold` — logic
Rule (reason 30): when hold flag 0xFFF8E550 == 1 keep the current value and just refresh the
inverted copy. [O]

#### 0x000C4CE4 `park_range_rule_p_no_req` — logic
Rule (reason 90): park target (D6D6) == 8 and valid-park-request flag D5F8 (getter
0x000A7716) == 0 → output 0 or 6 (N) selected by calibration descriptor byte +4
(slot 0xFFF84AC4). [O] spec 14 §3 D5F8.

#### 0x000C4CE6 `park_range_rule_p_no_req_fp` — logic
Byte-identical rule to 0x000C4CE4 but with callers='-' — reached through a function pointer
(a second installed copy of the P-without-request rule). [O]

#### 0x000C4D3C `park_range_rule_p_req_override` — logic
Rule (reason 170): park target == 8, range byte 0xFFF8D6CF is 7 or 5, D5F9 (getter
0x000A7720) == 0 and calibration byte +0xC == 1 → output that requested range (7=R or 5=D).
[I]

#### 0x000C4D98 `park_range_rule_p_valid_req` — logic
Rule (reason 100): park target == 8 and D5F8 == 1 (valid safety-layer park request) →
output 8 (P). [O] spec 14 §3/§4.2.

#### 0x000C4DCE `park_range_rule_target_r` — logic
Rule (reason 110): park target 0xFFF8D6D6 == 7 → output 7 (R). The main arbiter only writes
8/6/5 (spec 14 §4.1) so this covers targets from other writers. [I]

#### 0x000C4E24 `park_range_rule_target_n` — logic
Rule (reason 140): park target == 6 and D5F9 == 0 → output 6 (N). [O]

#### 0x000C4E5E `park_range_rule_none` — stub
Placeholder rule in the chain: clears the reason byte and always returns 0 (never fires).
[O]

#### 0x000C4E66 `park_range_rule_target_d` — logic
Rule (reason 150): park target == 5 → output 5 (D, drive-related released state).
[O] spec 14 §4.1.

#### 0x000C4E96 `park_range_rule_default` — logic
Chain default (reason 160): writes {0, 0xff, 0, 0xa0} when no earlier rule fired —
"no range output". [O]

#### 0x000C4EAC `park_range_test_value_override` — logic
Test/calibration override (reason 200): when 0xFFF8D61B = 1 (calibration-override enable,
spec 14 §2) the result value is replaced from the override descriptor at slot 0xFFF84AC4:
byte +0x10 (unless 0xff) with its inverted copy, and byte +0x18 can force the inverted-copy
byte directly. [O]

#### 0x000C4EF0 `park_range_test_qual_override` — logic
Override tail: with 0xFFF8D61B = 1 and override descriptor byte +0x14 != -1 the qualifier
byte of the result is replaced from that descriptor. [O]

#### 0x000C4F20 `park_cal_slot_ac8_set_rom` — stub
Calibration pointer install: slot 0xFFF84AC8 := ROM struct 0x0004EE44 (init variant
0x000A70AA). [O]

#### 0x000C4F28 `park_cal_slot_ac8_set_flash` — stub
Slot 0xFFF84AC8 := data-flash descriptor 0x00181234 (init variant 0x000A713E). [O]

#### 0x000C4F3C `park_cal_slot_acc_set_rom` — stub
Slot 0xFFF84ACC := ROM struct 0x0004EE4C (init variant 0x000A70AA). [O]

#### 0x000C4F44 `park_cal_slot_acc_set_flash` — stub
Slot 0xFFF84ACC := data-flash descriptor 0x0018123C (init variant 0x000A713E). [O]

#### 0x000C4F58 `park_cal_slot_5350_set_rom` — stub
Slot 0xFFF85350 (safety-layer park-arbiter calibration group, spec 14 §6) := ROM struct
0x0004EE54 (init variant 0x000A70AA). [O] spec 14 §6.

#### 0x000C4F60 `park_cal_slot_5350_set_flash` — stub
Slot 0xFFF85350 := data-flash descriptor 0x00181244 (init variant 0x000A713E). [O]

#### 0x000C4F74 `park_cal_slot_ad0_set_rom` — stub
Slot 0xFFF84AD0 := ROM struct 0x0004EF2C (init variant 0x000A70AA). [O]

#### 0x000C4F7C `park_cal_slot_ad0_set_flash` — stub
Slot 0xFFF84AD0 := data-flash descriptor 0x0018131C (init variant 0x000A713E). [O]

#### 0x000C4F90 `park_range_req_copy_out` — data-accessor
Copies the range request byte 0xFFF8D672 (lever request, spec 14 §2) into *param.
Called from 0x000A5A4C. [O] spec 14 §2.

#### 0x000C4F9C `park_cal_slot_ad4_set_rom` — stub
Slot 0xFFF84AD4 (the override/test descriptor slot read by the range-output chain) :=
ROM struct 0x0004EF48 (init variant 0x000A70AA). [O]

#### 0x000C4FA4 `park_cal_slot_ad4_set_flash` — stub
Slot 0xFFF84AD4 := data-flash descriptor 0x00181338 (init variant 0x000A713E). [O]

#### 0x000C4FB8 `park_flag_d5f5_is_one` — data-accessor
Representative of a 10-member cluster of generated predicates `flag == 1`, each member
calling a different byte getter: 0x000C4FB8→0xFFF8D5F5 (falling edge aux park request,
spec 14 §3), 0x000C4FC8→0xFFF893FF, 0x000C5256→0xFFF89404, 0x000C5266→0xFFF89405,
0x000C5276→0xFFF8940A, 0x000C5286→0xFFF8940B, 0x000C5296→0xFFF8940C,
0x000C530C→0xFFF8E53F, 0x000C54EE→0xFFF89415, 0x000C54FE→0xFFF89416. All called from
park output step 0x000A87CC. [O]

#### 0x000C4FD8 `park_cond_out_enabled` — logic
Compound condition: 0xFFF893FE == 1 AND (0xFFF8E55B == 0 OR override byte calib+4 == 1)
AND (0xFFF8E55A == 0 OR calib+8 == 1) AND 0xFFF8E533 == 0 AND 0xFFF8E532 == 0 → 1.
Override bytes come from descriptor slot 0xFFF84AD4. Part of the 0x000A87CC output-step
condition bank. [I]

#### 0x000C503A `park_cond_e533_released` — logic
Condition: 0xFFF8E533 != 0 AND 0xFFF893E8 != 0 (module active, spec 14 §4.4) AND
0xFFF8E55B == 0 AND 0xFFF8E55A == 0 AND 0xFFF8E55C == 0 AND park target 0xFFF8D6D6 not in
{8,7,6} → 1 (park released while request flag set). Member 0x000C5092 is identical except
it tests 0xFFF8E532 != 0. [I]

#### 0x000C50EA `park_cond_e535_released` — logic
Like 0x000C503A but gated by 0xFFF8E535 != 0 and 0xFFF8D680 == 0. Member 0x000C514A tests
0xFFF8E534 != 0 instead. [I]

#### 0x000C51AA `park_cond_d6bc_bit1` — logic
Condition: 0xFFF8D680 == 1 AND (0xFFF8D6BC & 2) != 0 AND 0xFFF8E53F == 0 AND
0xFFF893FC == 0 → 1. [I]

#### 0x000C5220 `park_range_value_is_d` — logic
Returns range value 0xFFF8D6CE (getter 0x000A7966, spec 14 §4.4) == 5 (D). [O]

#### 0x000C5230 `park_range_value_is_prn` — logic
Returns 1 when the range value 0xFFF8D6CE is 8, 7 or 6 (P/R/N) — i.e. not released-to-D.
[O]

#### 0x000C52A6 `park_cond_d940d_d670` — logic
Condition: 0xFFF8940D == 1 AND 0xFFF8D670 == 0 → 1. [I]

#### 0x000C52C4 `park_cond_d6b6_released` — logic
Condition: 0xFFF8D6B6 == 1 AND (override byte calib+0 (slot 0xFFF84AD4) == 0 OR
0xFFF893FC == 0) AND park target not in {8,7,6} → 1. [I]

#### 0x000C531C `park_cond_no_flag_rise` — logic
Edge-tracking predicate with one-shot latches (0xFFF8ABB4/0xFFF8ABB8) and history bytes
0xFFF8E645/0xFFF8E646: returns 1 when 0xFFF8E55B and 0xFFF8E55A show no 0→1 transition and
0xFFF8E55E == 0 and 0xFFF8E55C != 0. [I]

#### 0x000C5398 `park_cond_d9411_or_d9413` — logic
Condition: (0xFFF89411 == 1 OR 0xFFF89413 == 1) AND 0xFFF893FC == 1 → 1. [I]

#### 0x000C53C2 `park_cond_release_e535` — logic
Output-step condition (caller 0x000A87CC): 0xFFF8E535 != 0 AND input D61A[+0x55] (0xFFF8D66F)
== 1 AND D61A[+0x66] (0xFFF8D680) == 0 AND module active (0xFFF893E8) AND 0xFFF8E55B/E55A/E55C
all 0 AND park target 0xFFF8D6D6 not in {8,7,6} → 1. [I]

#### 0x000C5484 `park_cond_release_e534` — logic
Same shape as 0x000C53C2 but gated by 0xFFF8E534 != 0 (via getter 0x000A79E0). [I]

#### 0x000C5534 `park_cal_slot_ad8_set_rom` — stub
Calibration pointer install: slot 0xFFF84AD8 := ROM struct 0x0004EFB8 (init 0x000A70AA).
This slot's descriptors feed the debounce monitors 0x000C556E/0x000C562C/0x000C5688. [O]

#### 0x000C553C `park_cal_slot_ad8_set_flash` — stub
Slot 0xFFF84AD8 := data-flash descriptor 0x001813A8 (init 0x000A713E). [O]

#### 0x000C5566 `park_mon_load_d66e` — data-accessor
Copies input byte 0xFFF8D66E into monitor-struct field +4 (caller 0x000C5550). [O]

#### 0x000C556E `park_mon_qualify_timer` — logic
Debounce cell: when mode byte param[2] changes, reloads timer param[0] from calibration slot
0xFFF84AD8 (+0/+4), arms state flag 0xFFF8517F; aborts (clears timer and flag) when
0xFFF8E533/E532 both 0 or 0xFFF893E8 == 0; writes timer-expired bool param[1], qualified
flag param[3] and its complement param[7], and history param[5]. [I]

#### 0x000C562C `park_mon_d670_edge` — logic
Edge tracker for input 0xFFF8D670 == 1: param[+8] prev / param[+9] now / param[+10] edge
(rising normally, falling only while calibration +0x20 byte (slot 0xFFF84AD8) is 1).
[O]

#### 0x000C5688 `park_mon_track_transitions` — logic
Fills monitor struct +0x0B…+0x1F with edges/transitions of park inputs: D61A[+0x55]/[+0x56]
(D66F/D670) rising/falling, u16 at D61A[+0x70] (bit 3 toggles) with mask checks against
calibration 0xFFF84AD8 pointers +8/+0xC/+0x10/+0x14/+0x18/+0x1C, a change flag on D61A[+0x42]
(D65C), and rising/falling edges of 0xFFF8E565, 0xFFF8E55B and 0xFFF8E55A (getters
0x000A792A/0x000A793E/0x000A7934). [O] spec 14 §2.

#### 0x000C5848 `park_cal_slot_adc_set_rom` — stub
Slot 0xFFF84ADC := ROM struct 0x0004EFDC — the monitor calibration used by groups 115/192
(timers at +0x1C/+0x20, limits at +0x10/+0x14, +0x18). [O]

#### 0x000C5850 `park_cal_slot_adc_set_flash` — stub
Slot 0xFFF84ADC := data-flash descriptor 0x001813CC (init variant 0x000A713E). [O]

#### 0x000C5864 `park_mon_g192_reload_timers` — logic
Reloads monitor-struct debounce timers: u16 at +0x10 from calibration 0xFFF84ADC +0x20 and
u16 at +0xC from +0x1C (same fields the group-192 monitor 0x000C5912 counts down). Called
from 0x000A59D0. [O]

#### 0x000C58AA `park_mon_g115_report` — logic
Fault monitor, group **115** (identity slot 0x4B91C → 0x63254 = 0x631E1+0x73), data 0x40,
report entry 0x000A0E70 via slot 0x4B888. Fails when park target (0xFFF8D6D6) == 8 AND
D5F8 (valid park request) == 1 AND speed 0xFFF8D5F6 (getter 0x000A7734) exceeds the u16
limit at calibration 0xFFF84ADC +0x14 — i.e. **Park engaged while the vehicle is moving**.
Also sets monitor flag param[+0x19]. Caller 0x000C5886. [O] spec 13 §2/§2a.

#### 0x000C5912 `park_mon_g192_check` — logic
Fault monitor, group **192** (identity slot 0x4B8E8 → 0x632A1), report3 0x000A105E via slot
0x4B88C, data 0x40. When enabled (param[+0xC] == 1) and target == 8: fails after both debounce
timers (+0xC/+0x10) expire plus 0x000C5C54's qualifier, or on an invalid transition of the
safety-layer park-request byte D61A[+0x2E] (0xFFF8D648: 1→0, 1→2, 2→0). Sub-timers:
0x000C5AC4 (param[0], armed when D648==1 qualified by +0x38), 0x000C5B4C (param+4),
0x000C5BC4 (param+8, armed from D893E8/D893E9 states). Healing report when sub-flags
E648/E649 clear the stored fails. [O/I] spec 13 §2a.

#### 0x000C5AC4 `park_mon_g192_timer_req` — logic
Sub-timer of the group-192 monitor: arms param[0] from calibration +0xC when target==8,
D684==0, D686==0, D68A(D61A[+0x6C])==0 and the park request D648==1 with qualifier
D61A[+0x38]==0 (setting *param_3=1); counts down while target stays 8 and D686==0.
[O] spec 14 §2 (+0x2E/+0x38).

#### 0x000C5B4C `park_mon_g192_timer_trans` — logic
Sub-timer (param+4) armed from calibration +0x4 when the previous park-request byte
(param_2, history in 0xFFF8E647) was 1 and it is now 2 or 0 (dropped/invalidated); counts
down while target == 8, result bool at param+6. [O]

#### 0x000C5BC4 `park_mon_g192_timer_qual` — logic
Sub-timer (param+8) armed from calibration +0x8 when module-state 0xFFF893E8 == 1 with
0xFFF893E9 != 1, or 0xFFF893E8 == 3 with E9 == 0; sets *param_2=1 when D648==1 and
D61A[+0x38]==0; counts down while target == 8. [I]

#### 0x000C5C54 `park_mon_g192_time_up` — logic
Qualifier used by the group-192 monitor: param+4 timer running AND 0xFFF893E8 == 1 AND
0xFFF893E9 != 1. [O]

#### 0x000C5C7C `park_mon_g168_fsm` — logic
Fault monitor FSM, group **168 = P07E4-00** (identity slot 0x4B8EC → 0x63289), report3 via
0x4B88C, data 0x41. Watches the park target: state 0→1 on transition from 7/6/5 to 8 (P);
state 2 while target==8: if park request D61A[+0x2E] == 1 loads timer param+0x14 from
calibration +0 (slot 0xFFF84ADC) → state 3, else fails immediately when D61A[+0x06]
(0xFFF8D620) == 4; state 3: on timer expiry with target still 8, fails when 0xFFF8D648 != 0
else passes when the freeze-frame byte from 0x000A7B12 (object 0xFFF81C48) is >= the
calibration +0x18 minimum. Uses 0x000A7B2E/0x000A7B24 to clear/mark the event object.
[O] spec 13 §2a; spec 14 §7.

#### 0x000C5E02 `park_mon_g114_report` — logic
Fault report, group **114 = P1B13-00** (slot 0x4B8E4 → 0x63253), data 0x41, report
0x000A0E70. Fails when D61A[+0x06] (0xFFF8D620) == 4 AND park request D61A[+0x2E] != 1;
tested flag = enable byte param[+0x18] (set by 0x000C5E40). Caller 0x000C589C. [O]
spec 13 §2a; spec 14 §7.

#### 0x000C5E40 `park_mon_g113_enable` — logic
Monitor enable: sets param[+0x18] = 1 when qualifier D61A[+0x38] < 2 AND the u16 at
calibration 0xFFF84ADC +0x10 is below input D61A[+0x3E] (0xFFF8D658, [I] supply voltage)
AND D5F9 (0xFFF8D5F9) == 0. Gates the tested flag of the group-113/114 reports. [O]
spec 12 §4.1; spec 13 §4.1.

#### 0x000C5E78 `park_mon_g113_report` — logic
Fault report, group **113 = P1B14-00** (slot 0x4B8DC → 0x63252), data 0x40, report
0x000A0E70. Fails exactly when D61A[+0x2E] (0xFFF8D648, safety-layer park request) == 1 AND
D61A[+0x06] (0xFFF8D620) == 1 — matching spec 13 §4.1; tested = enable byte param[+0x18].
Caller 0x000C589C. [O] spec 13 §4.1.

#### 0x000C5EF8 `park_cal_slot_ae0_set_rom` — stub
Slot 0xFFF84AE0 := ROM struct 0x0004F000 (init 0x000A70AA). [O]

#### 0x000C5F00 `park_cal_slot_ae0_set_flash` — stub
Slot 0xFFF84AE0 := data-flash descriptor 0x001813F0 (init 0x000A713E). [O]

#### 0x000C5F14 `lib_debounce_timer_run` — library
Config-driven debounce timer cell: param[0] timer, param[+4] points to a config object with
function pointers (+0x14 start, +0x20 condition) and args; when idle calls start, otherwise
evaluates the condition and counts down; publishes param[1] expired and param[2] running.
Called from 0x000C829E. [I]

#### 0x000C6090 `park_cal_slot_ae4_set_rom` — stub
Slot 0xFFF84AE4 := ROM struct 0x0004F010 (init 0x000A70AA); flash-side counterpart is
0x000C6098. [O]

#### 0x000C6098 `park_cal_slot_ae4_set_flash` — stub
Installs data-flash calibration descriptor 0x00181400 into park calibration slot 0xFFF84AE4
(ROM counterpart 0x000C6090 installs 0x0004F010). Init caller 0x000A713E. [O]

#### 0x000C60AC `park_cal_slot_ae8_set_rom` — stub
Slot 0xFFF84AE8 := ROM struct 0x0004F024 (init 0x000A70AA). This slot feeds the countdown
reload logic 0x000C62F6/0x000C6394 (its +8/+0xC/+0x10 u16 curve tables). [O]

#### 0x000C60B4 `park_cal_slot_ae8_set_flash` — stub
Slot 0xFFF84AE8 := data-flash descriptor 0x00181414 (init 0x000A713E). [O]

#### 0x000C60CE `park_e567_mode_state_calc` — logic
Classifies a request into states 0..3 for byte-pair object param_1: when param_1[1]==1 the
state is 0; otherwise the mode byte 0xFFF8E567 selects: 2 → state 2 when the valid-park-request
flag D5F8 (getter 0x000A7716) is 1; 1 → state 1 when D5F9 (0x000A7720) is 0; 3 → state 3;
other → pass-through. Returns the raw flag/mode value. Caller 0x000A6CF8 (task 0x9B228).
[I] spec 14 §3 (D5F8/D5F9 meanings).

#### 0x000C6116 `park_flag_d60a_cond_eval` — logic
Sets param_1[+1] = 1 when D60A (0xFFF8D60A via 0x000A77AA) is 1 AND (byte 0xFFF81C6C via
0x000A7B08 nonzero OR D5F9==1 OR D5F8==0) OR the u16 0xFFF8D65A (D61A[+0x40]) is below the
first u16 of calibration slot 0xFFF84AE8's target table. Caller 0x000A6CF8. [I]

#### 0x000C617C `park_cal_slot_aec_set_rom` — stub
Slot 0xFFF84AEC := ROM struct 0x0004F028 (init 0x000A70AA). [O]

#### 0x000C6184 `park_cal_slot_aec_set_flash` — stub
Slot 0xFFF84AEC := data-flash descriptor 0x00181418 (init 0x000A713E). [O]

#### 0x000C6198 `park_prot4_checksum` — library
Returns (sum of 4 bytes at param) XOR 0xAAAAAAAA — the integrity checksum used by the
protected state cell at 0xFFF83078. Only caller 0x000C623E. [O]

#### 0x000C61B2 `park_prot32_delta_addsub` — library
Updates the obfuscated u32 cell *param_1: v = ((v ^ 0xAAAAAAAA) + b0 - c0 + b1 - c1) ^
0xAAAAAAAA using two bytes each from param_2/param_3 (passed on the stack). Used by the
countdown logic 0x000C62F6/0x000C627A. [O]

#### 0x000C61F8 `park_prot32_delta_add2` — library
Simpler delta update: v = ((v ^ 0xAAAAAAAA) + a - b) ^ 0xAAAAAAAA. Caller 0x000C627A. [O]

#### 0x000C6228 `lib_pair_byte0_get` — library
Two-instruction getter: returns *param_1 — the first byte of an inverted-copy pair (checked
getter idiom). In this unit it reads the validity pair at 0xFFF83046 (caller 0x000C62F6).
Cluster of 3: 0x000C6228, 0x0010CD20, 0x00149252 (identical duplicated helpers in the
safety-layer regions). [O]

#### 0x000C622E `lib_invpair_mismatch` — library
Returns *p != (byte)~p[1] — inverted-copy consistency check. Caller 0x000C623E. [O]

#### 0x000C623E `park_3078_prot_state_reset` — logic
Validates the protected state at 0xFFF83078: if checksum(0x000C6198) of the first 4 bytes
differs from the stored copy at 0xFFF8307C, zeroes bytes 0..3, recomputes and re-stores the
checksum. Also resets the inverted pair 0xFFF83046 to 0/0xFF when 0x000C622E reports
corruption. Called via pointer from task 0x9B228 (0x000A6CF0). [O]

#### 0x000C627A `park_3078_countdown_run` — logic
Runs the protected countdown at 0xFFF83078 (short at +0, expiry flag byte at +2): decrements
it each pass through 0x000C61B2/0x000C61F8 (delta updates through the associated protected
cell *(0xFFF89758)+4), sets the expired flag when it reaches 1/0, and resets the pair
0xFFF83046 when 0xFFF893E8 (getter 0x000A76CE) is nonzero. Caller 0x000A6D12. [O]

#### 0x000C62F6 `park_3078_countdown_reload` — logic
Reloads the 0xFFF83078 countdown: gated by D5F9, 0xFFF893E8/0xFFF893E9, the last output value
at 0xFFF85181 (∉ {7,5}) and the validity pair 0xFFF83046; when the enable byte at
cal-slot 0xFFF84AEC->+0x10 is 1, computes a new timeout via 0x000C6394 and delta-commits it.
Caller 0x000A6D12. [I]

#### 0x000C6394 `park_3078_timeout_value_calc` — logic
Computes the countdown value: evaluates two 8-point curves (interp helper 0x0012529C) with
input byte D61A[+0x19] over the +8/+0xC tables of cal slot 0xFFF84AEC; D61A[+0x1A]==2 selects
the max-of-8 variant 0x000C6462; sets the 0xFFF83046 pair to 1/0xFE on the D5F9 path; when
D61A[+0x05]==1 scales the result by a percent u16 from the slot's pointer table (v*ref/100).
Caller 0x000C62F6. [I]

#### 0x000C6462 `park_curve8_max_over_cal` — logic
Loops 8 iterations over u16 entries of the two curve tables of cal slot 0xFFF84AEC (source
selected per-pass by D5F9, getter 0x000A7720), evaluating each through the 8-point
interpolator 0x0012529C and returning the maximum. Caller 0x000C6394. [I]

#### 0x000C64B8 `park_cal_slot_af0_set_rom` — stub
Slot 0xFFF84AF0 := ROM struct 0x0004F03C (init 0x000A70AA). This slot holds the lever-request
arbiter's calibration (target pointers +0/+4/+8/+0x10, status word +0x14, override +0x1C).
[O]

#### 0x000C64C0 `park_cal_slot_af0_set_flash` — stub
Slot 0xFFF84AF0 := data-flash descriptor 0x00142C00+0x722C → value 0x0018142C (init
0x000A713E). [O]

#### 0x000C64D4 `park_lever_req_load_and_run` — logic
Entry of the lever-request output stage: copies the current range request byte 0xFFF894B6
(codes 5–8) into object byte +4, runs the candidate arbiter 0x000C6512, and then applies the
calibration override from 0xFFF84AF0->+0x1C when the override enable D61B (0xFFF8D61B) is 1
(spec 14 §4.3 override mechanism). Caller 0x000A58D4 (task 0x9B34A). [O] spec 14 §4.3.

#### 0x000C6502 `park_flag_d661_copy` — data-accessor
Sets object byte +9 = (0xFFF8D661 == 1) — the enable/precondition flag consumed by the
candidate handlers 0x000C6578/0x000C66F8. Caller 0x000C6512. [O]

#### 0x000C6512 `park_tgt_arbiter_run` — dispatch
Runs the range-target candidate chain over the lever object: after 0x000C6502, tries
0x000C6578, 0x000C6664, 0x000C67F6, 0x000C6822, 0x000C687E, 0x000C68CC, 0x000C695E,
0x000C66F8 in order; the first to set done-flag +3 stops the chain, 0x000C69FC is the
no-candidate default, and 0x000C6A46 always post-processes into confirmed output byte +8
(codes 5/6/7/8 = D/N/R/P). [I] spec 14 §4.1.

#### 0x000C6578 `park_tgt_cand_lever_57` — logic
Candidate for lever codes 7 (R) and 5 (D): requires flag +9 == 0, D5F2 (aux park request)
== 1 and D608 == 0; reads the target short from cal slot 0xFFF84AF0 — indexed pointer table
+8[*0xFFF8E530] when mode struct 0xFFF89434[+0x82]==7, dereferenced pointer +0 when ==5 —
and commits it (state +2 = 6, source +5 = lever, subtype +7 = 3) when 0x000C6BA8 agrees; the
mode-0 path uses pointer +0x10 with qualifiers 0x000C6D86/0x000C6DD8 (subtype 4). [I]

#### 0x000C6664 `park_tgt_cand_lever_8` — logic
Candidate for lever code 8 (P): requires the D603 check (0x000A775A) to return 1 (or 2 with
cal byte 0xFFF84AF0->+0x20 == 1), the park target D6D6 (0x000A7B78) != 8, and the target
short behind pointer +4 nonzero; commits target with state 6, source 8, subtype 2. [I]
spec 14 §4.1.

#### 0x000C66F8 `park_tgt_cand_hold` — logic
Last candidate — holds/refreshes the committed target: for source 8 with 0xFFF8E531 == 8 and
D603 nonzero it takes the +4 pointer target (when cal +0x24 byte == 1); otherwise requires
0xFFF8E531 == source +5, D608 == 0, flag +9 == 0, D5F2 == 1, 0xFFF893E8 ∈ {1,2}, cal +0x38
byte == 1, then for source 7 takes the indexed +8 table target (or falls to source-5 path
with +0 pointer and D605 == 1) and re-commits it with state 0. [I]

#### 0x000C67F6 `park_tgt_cand_d608_zero` — logic
Candidate handler: when D608 (0xFFF8D608 via 0x000A7750) is 1 and the current source byte +5
is 7, 6 or 5, commits a zeroed target with state 0 via 0x000C6B8E. Returns the touched flag
at +6. Caller 0x000C6512. [I]

#### 0x000C6822 `park_tgt_cand_req_diff` — logic
Candidate handler: compares the request byte 0xFFF8941A against the active source +5 — for
source 8 it acts when 0xFFF8941A is 5/6/7, for source 5/7 when 0xFFF8941A is 8/6 or 5/7 and
differs from the source — and commits a zeroed target with state = the current lever request
0xFFF894B6 (0x000C6B8E). [I]

#### 0x000C687E `park_tgt_cand_flag9_release` — logic
Candidate handler: for sources 5 and 7, when the enable/precondition flag at object +9 is 1,
releases the target via the commit-clear 0x000C6B7A (target 0, source kept as state). [I]

#### 0x000C68CC `park_tgt_cand_e3e8_e3e9` — logic
Candidate handler for sources 5/7 combining the two status bytes 0xFFF893E8 (0x000A76CE) and
0xFFF893E9 (0x000A76D8) with the lever request 0xFFF894B6 and the D5F2 aux-park-request flag
(0x000A772A): the ==1/==0 path commits state 0 via 0x000C6B8E, the ==2 paths and the plain
1/0 case fall through to 0x000C6B7A. [I]

#### 0x000C695E `park_tgt_cand_dir_checks` — logic
Candidate handler: source 8 requires D603 (0x000A775A) 0/2 and (for 2) the cal byte behind
0xFFF84AF0->+0x20 to be 0; sources 5/7 require 0xFFF893E8 1/2 plus the direction bytes D604
(0x000A776E, source 7) or D605 (0x000A7764, source 5), the direction-plausibility getter
0x000A7BD2 (lever object 0xFFF8304C+0x10, spec 14 §4.3) and the cal +0x28 byte; on match
releases via 0x000C6B7A. [I] spec 14 §4.3.

#### 0x000C69FC `park_tgt_default_countdown` — logic
Default when no candidate fired: decrements the target short at object +0 unless it is -1,
sets the expired flag (+2 as bool) when it reaches 0, and on expiry releases via 0x000C6B7A
when the source is 7/5 with lever request 0xFFF894B6 == 0, or source 8. [I]

#### 0x000C6A46 `park_tgt_confirm_status_bits` — logic
Post-processing: clears the confirmed output byte +8, then for each source (6/8/7/5) and
subtype at +7 checks the matching bit (0, 1, 2, 3, 4, 5, 6, 7 or 8) of the u16 status word
behind calibration slot 0xFFF84AF0->+0x14 and copies the source code into +8 only when the
bit is set. This is the arbiter's qualified output (codes 5–8). [I]

#### 0x000C6B7A `park_tgt_commit_clear` — logic
Commit helper: target = 0, state (+4) = current source (+5), source = 0, subtype = 0,
done flag (+3→+6 usage) = 1. Shared by the candidate handlers 0x000C687E/0x000C68CC/
0x000C695E and the default 0x000C69FC. [O]

#### 0x000C6B8E `park_tgt_commit_state` — logic
Commit helper with explicit state argument: target = 0, state (+4) = param_2, source = 0,
subtype = 0, done = 1. Callers 0x000C67F6, 0x000C6822, 0x000C68CC. [O]

#### 0x000C6BA8 `park_tgt_transition_allowed` — logic
225-instruction transition-permission matrix between the park target D6D6 (0x000A7B78) and
the lever request 0xFFF894B6: permits when (D6D6=7, lever=5) or (5,7) with D605/D604
qualifiers, or (D6D6=6, lever=7/5) with the cal bytes behind 0xFFF84AF0 +0x28/+0x2C/+0x30/
+0x34, the direction-plausibility getter 0x000A7BD2 and the source matching the lever.
Caller 0x000C6578 (the D/R candidate). [I] spec 14 §4.1.

#### 0x000C6D86 `park_tgt_release_dir_ok` — logic
Returns 1 when (D604 == 0 and lever 0xFFF894B6 == 7) or (D605 == 0 and lever == 5) — the
mode-0 path qualifier of 0x000C6578. [I]

#### 0x000C6DD8 `park_tgt_released_lever57_req` — logic
Returns 1 when the park target D6D6 is 8 or 6 and the lever request 0xFFF894B6 is 7 or 5.
Qualifier of the 0x000C6578 mode-0 commit. [I]

#### 0x000C6E14 `park_cal_slot_af4_set_rom` — stub
Slot 0xFFF84AF4 := ROM struct 0x0004F078 (init 0x000A70AA). Slot feeds the speed
qualification block 0x000C6E38–0x000C7050 (curve pointers +0/+4, mode bytes +8/+0xC). [O]

#### 0x000C6E1C `park_cal_slot_af4_set_flash` — stub
Slot 0xFFF84AF4 := data-flash descriptor 0x00181468 (init 0x000A713E). [O]

#### 0x000C6E30 `park_obj_flags_8_9_set` — stub
Initializes the speed-qualification object: bytes +8 and +9 := 1. Caller 0x000A5968. [O]

#### 0x000C6E38 `park_speed_qual_run` — logic
Runs the speed-qualification stage on its object: 0x000C6E56 (input change detection),
0x000C6EC2 (validity), 0x000C6F36 (change-flag clear), 0x000C6FC6 and 0x000C6F44 (the two
speed threshold evaluations), 0x000C7050 (combine). Caller 0x000A5944 (task 0x9B34A). [O]

#### 0x000C6E56 `park_qual_edge_detect` — logic
Records changed-flags for four inputs into the qualification object: D67D (0x000A7834) →
+2/+3, D67C (0x000A783E) → +0/+1, and the two struct bytes 0xFFF8D61A[+0x64] → +4/+5 and
[+0x65] → +6/+7. [O]

#### 0x000C6EC2 `park_qual_valid_flags` — logic
When all four inputs are 0 sets valid flags +8 = +9 = 1; otherwise +9 follows +8 only if the
+0/+1 input changed, and a change of the +4/+5 group clears +8. [I]

#### 0x000C6F36 `park_qual_clear_changed_flags` — logic
When the combined flag +0xE is 1, clears the change flags +7 and +5. Caller 0x000C6E38. [O]

#### 0x000C6FC6 `park_speed_abs_qualify` — logic
Evaluates |s16 0xFFF8D61A[+0x12]| against a 5-point curve (interp 0x0012529C) behind
cal slot 0xFFF84AF4->+4: threshold flag +0xA; the mode byte at the slot's +0xC pointer (0/1/2)
selects how +0xC is produced (curve only, or curve with status bit 1 of 0xFFF8D6BC and
byte 0xFFF8D61A[+9]). [I]

#### 0x000C6F44 `park_speed_out_qualify` — logic
Same pattern for the output speed D61A[+0x2C] (0xFFF8D646, spec 14 §2) against a 5-point
curve behind cal slot 0xFFF84AF4->**+0: threshold flag +0xB, mode byte at slot +8, result
flag +0xD. [I] spec 14 §2.

#### 0x000C7050 `park_qual_combine_flags` — logic
Combined qualification flag +0xE = (+0xC or +0xD) — OR of the two speed-threshold results.
Caller 0x000C6E38. [O]

#### 0x000C7080 `park_cal_slot_af8_set_rom` — stub
Slot 0xFFF84AF8 := ROM struct 0x0004F088 (init 0x000A70AA). [O]

#### 0x000C7088 `park_cal_slot_af8_set_flash` — stub
Slot 0xFFF84AF8 := data-flash descriptor 0x00181478 (init 0x000A713E). [O]

#### 0x000C709C `park_req_pair_override_cal_af8` — logic
When the calibration-override enable D61B (0xFFF8D61B) is 1, writes an inverted-copy pair at
param+2/+3 from the override byte behind cal slot 0xFFF84AF8->+0x2C (kept when 0xFF) with the
char at ->+0x30 replacing the inverted byte when nonzero. Called by the park arbiter driver
0x000A548C and by 0x000C70CE — the spec 14 §4.3 override mechanism applied to a
requested-target pair. [O] spec 14 §4.3.

#### 0x000C70CE `park_range_out_stage_run` — logic
Driver of the range-output stage: keeps the previous target at object+4, runs the change
detectors 0x000C7110/0x000C71BE, the delay state 0x000C7258, the target candidate chain
0x000C7390, the calibration override 0x000C709C, publishes through 0x000C79B8 into the lever
request object 0xFFF8304C, and — when D5F9 (0x000A7720) is 0 — writes the target pair into
object +8/+0xC inverted. Caller 0x000A59E8. [O] spec 14 §4.3/§6.

#### 0x000C7110 `park_chg_flag_d68e_detect` — logic
Edge detector for byte 0xFFF8D68E (kept in 0xFFF85182): when the previous output (+4) and the
cal getter 0x000A79FE (byte 0xFFF8E53C) are both != 6 and the byte changed to match one of
the calibration bytes behind slot 0xFFF84AF8 +0x10/+0x0C, sets change flag object+7. [I]

#### 0x000C7178 `park_cand_chgflag7_lever` — logic
Candidate: when change flag +7 is 1 and the current output (0xFFF8D6DE) is 7 or 5, requests
target 8 reason 0x50 if the previous target was 8, else target 6 reason 0x51 (via
0x000C792E). Caller 0x000C7390. [I]

#### 0x000C71BE `park_chg_flag_d68f_detect` — logic
Second edge detector, for byte 0xFFF8D68F (kept in 0xFFF85183), against the calibration bytes
behind slot 0xFFF84AF8 +0x08/+0x04; sets change flag object+8. [I]

#### 0x000C7226 `park_cand_chgflag8_lever` — logic
Candidate: change flag +8 is 1 and 0xFFF8D6DE is 7 or 5 → target 8 reason 0x52 (previous 8)
or target 6 reason 0x53. Caller 0x000C7390. [I]

#### 0x000C7258 `park_out_delay_countdown` — logic
Maintains the output-delay state (bytes +0 countdown, +1 expired, +9 active): starts only
when 0xFFF893E9 (0x000A76D8) is 1 and 0xFFF893E8 (0x000A76CE) is 0 and the previous target
!= 8, loading the countdown byte from cal slot 0xFFF84AF8->+0x14; while the safety-layer park
request D61A[+0x2E]=0 with qualifier [+0x38]=0 holds, counts down. Caller 0x000C70CE. [I]

#### 0x000C733A `park_cand_safety_park_req` — logic
Candidate: delay state +9 = 1, D61A[+0x2E] (P1B14 byte 1, valid park request) nonzero with
qualifier D61A[+0x38] = 0, D603 = 0, D62F = 0 and the cal byte at 0xFFF84AF8->+0x24 = 1 →
target 8, reason 0x59. [I] spec 14 §2/§3.

#### 0x000C7390 `park_range_tgt_chain_run` — dispatch
Seeds the current output from 0xFFF8D6DE (0x000C7926) into object +2, then — only when the
enable byte behind cal slot 0xFFF84AF8->+0x18 is 1 — runs the candidate chain 0x000C77FE,
0x000C7444, 0x000C747C, 0x000C767C, 0x000C7756, 0x000C7178, 0x000C7226, 0x000C733A,
0x000C7630, 0x000C75FA, 0x000C76D8, 0x000C77A0, 0x000C785C, 0x000C78BE, with 0x000C74F4 as
fallback; finishes by writing the inverted copy object+3 = ~object+2. [O]

#### 0x000C7444 `park_cand_d5f9_prev8_back8` — logic
Candidate: D5F9 (0x000A7720) = 1, previous target (+4) = 8 and current output != 8 → target
8, reason 0x0A ("return to Park"). [I]

#### 0x000C747C `park_cand_release_cond` — logic
Candidate: current != 8 and D5F9 = 1: with D5F8 (0x000A7716) = 1 and D603 = 0 and D62F = 0 →
target 8 reason 0x14; otherwise (current != 6) target 6 reason 0x15. [I]

#### 0x000C74F4 `park_cand_fallback_release` — logic
Fallback when nothing fired: current = 5 and D5F9 = 0, output speed-class u16 D61A[+0x24]
below the u16 behind cal 0xFFF84AF8->**0, flags D61A[+0x22] != 0, [+0x26] = 0, and the
byte D61A[+0x39] selects (via values 4, 0x17, 0x1B–0x1E, 0x23, 0x25, 0x27, 0x29) one of ten
status bytes behind the +0x34 pointer — all matching → target 6, reason 0x1E. [I]

#### 0x000C75FA `park_cand_hold8_cond` — logic
Candidate: D609 (0x000A7702) = 1, current 0xFFF8D6DE != 8, and the cal byte at
0xFFF84AF8->+0x1C = 1 → target 8, reason 0x20. [I]

#### 0x000C7630 `park_cand_d65f_force8` — logic
Candidate: current != 8, D61A[+0x45] = 1, D61A[+0x15] (D62F, range-request block) = 0, D5F8 =
1 and D603 = 0 → target 8, reason 0x5A. [I] spec 14 §2 (D62F).

#### 0x000C767C `park_cand_leaver8_dir8` — logic
Candidate: previous target = 8, D5F9 = 0, 0xFFF893E8 != 0 and current 7 or 5 with the
direction bytes 0xFFF8D666 (7) / 0xFFF8D665 (5) = 1 → target 8, reason 0x33. [I]

#### 0x000C76D8 `park_cand_e3e8_release6` — logic
Candidate: 0xFFF893E8 != 0: current 5 with 0xFFF8D665 = 1 → target 6 reason 0x6E; current 7
with 0xFFF8D666 = 1 → target 6 reason 0x6F. [I]

#### 0x000C7756 `park_cand_release_hold8` — logic
Candidate: previous target = 8, current != 8, D5F9 = 0, byte 0xFFF8D664 = 1 and
0xFFF893E8 != 0 → target 8, reason 0x3C. [I]

#### 0x000C77A0 `park_cand_idle_force8` — logic
Candidate: current = 6, D61A[+0x45] = 0 and [+0x15] = 0, D5F9 = 0, 0xFFF893E8 = 0, D603 = 0
and D61A[+0x46] = 1 → target 8, reason 0x78. [I]

#### 0x000C77FE `park_cand_first_release6` — logic
First candidate: cal byte behind 0xFFF84AF8->+0x20 nonzero, 0xFFF8D62F = 1 and current 7/5
with that byte 2, or current 7 with byte 1 → target 6, reason 0x28. [I]

#### 0x000C785C `park_cand_fault_cond` — logic
Candidate: current != 8, D5F9 = 0, byte 0xFFF8E548 = 1: with D603 = 0 and D62F = 0 → target
8 reason 0x82, otherwise (current != 6) target 6 reason 0x83. [I]

#### 0x000C78BE `park_cand_g153_report` — logic
Candidate: cal byte behind 0xFFF84AF8->+0x28 = 1 and (D61C = 1 with current 7, or D61C = 3
with current 5) → target 6, reason 0x8C. Always invokes the interface call through the
pointer at 0x0004B888 (slots 0x4B8C8/0x4B928) with the pass/fail result — the **fault group
153** report (unit header; report callee 0x000A0E70 family). [O] spec 13 §2a.

#### 0x000C7926 `park_range_out_current_get` — data-accessor
Returns *0xFFF8D6DE — the current range-output byte shared by the whole 0xC7xxx candidate
chain. [O]

#### 0x000C792E `park_range_out_set_with_reason` — logic
Commits a candidate result: writes target to object+2 and reason to object+5; when the
current output is 8 and the new target is neither 8 nor 6 (or current 6 and new ∉ {8,6}) it
instead reports event 7/1 through the 8-arg reporter 0x000A7B36 (object 0xFFF81C38). [O]

#### 0x000C79B8 `park_lever_obj_publish` — logic
Publishes the range output into the lever request object 0xFFF8304C: writes the u32 pair
+0/+4 (value, inverted), mirrors the value to 0xFFF8E64B, and when the override enable D61B
is 1 replaces it with the byte behind cal slot 0xFFF84AB0->+0x18 (unless 0xFF) — answering
spec 14 §4.3's open question of a second writer of the lever object's +0x00. Caller
0x000C70CE. [O] spec 14 §4.3.

#### 0x000C79F8 `park_cal_slot_afc_set_rom` — stub
Slot 0xFFF84AFC := ROM struct 0x0004F0C0 (init 0x000A70AA). [O]

#### 0x000C7A00 `park_cal_slot_afc_set_flash` — stub
Slot 0xFFF84AFC := data-flash descriptor 0x001814B0 (init 0x000A713E). [O]

#### 0x000C7A14 `park_range_req_classify` — logic
Range-request classification driver: seeds its result object from the park target
0xFFF8D6D6 and dispatches 8 → 0x000C7A94, 5/7 → 0x000C7BAA, 6 → 0x000C7C9C, finishing with
the calibration override 0x000C7CCA. Caller 0x000A59E8 (same task stage as 0x000C70CE). [O]

#### 0x000C7A56 `park_req_7_downgrade_6` — logic
For range request 7 from 0xFFF8D6CE: when 0xFFF893E8 is 1 or 2 and D604 (0x000A776E) is
nonzero the result becomes 6 with flag +1 = 1, otherwise the 7 passes through. [O]

#### 0x000C7A94 `park_req_from_target8` — logic
Park target 8 path: when 0xFFF8D6CE is 7/6/5 delegates to 0x000C7A56, otherwise the result
is 8. Caller 0x000C7A14. [O]

#### 0x000C7AB8 `park_req_from_d6ce8` — logic
For 0xFFF8D6CE = 8: with D603 = 0 the result is 8, or 6 when D62F = 1; with D603 nonzero the
result is 6 and flag +0 = 1. Caller 0x000C7C9C. [O]

#### 0x000C7AE2 `park_req_set_6` — stub
Three-instruction helper: result byte +2 := 6. Caller 0x000C7BAA. [O]

#### 0x000C7AE8 `park_req_from_d6ce8_alt` — logic
For 0xFFF8D6CE = 8 with D603 1/2 the result is 6 with flag +0 = 1; otherwise 8, or the park
target 0xFFF8D6D6 when D62F = 1. Caller 0x000C7BAA. [O]

#### 0x000C7B18 `park_req_dir_swap_57` — logic
For requests 5/7 with 0xFFF8D6CE and 0xFFF8D6D6 on opposite sides (5↔7): with D62F = 1 and
current 7 keeps the request, otherwise requires 0xFFF893E8 1/2 and the direction bytes
D604/D605 to pass 6 with flag +1 = 1, or falls back to 6. [I] spec 14 §4.1.

#### 0x000C7BAA `park_req_class_57` — dispatch
Dispatch on 0xFFF8D6CE for the 5/7 target path: 6 → 0x000C7AE2, 8 → 0x000C7AE8, 5/7 →
0x000C7B18, other → pass through the park target 0xFFF8D6D6. Caller 0x000C7A14. [O]

#### 0x000C7C0C `park_mon_range_third_arbiter` — logic
The spec's "third arbiter" for the monitors: active only when 0xFFF893E8 is 1/2 and D62F ≠ 1;
input 0xFFF8D6CE (getter 0x000A7966) — value 7 → 7, or 6 when the reverse conditions fail
(D604 nonzero and (D604 ≠ 1 or lever-object direction 0xFFF8304C+0x10 via 0x000A5204 ≠ 2));
value 5 → 5 when D605 = 0 or the lever direction is 1; otherwise 6. [O] spec 14 §4.4.

#### 0x000C7C9C `park_req_class_6` — dispatch
Park target 6 path: 0xFFF8D6CE = 8 → 0x000C7AB8, 5/7 → the third arbiter 0x000C7C0C,
otherwise result 6. Caller 0x000C7A14. [O]

#### 0x000C7CCA `park_req_cal_override` — logic
When the override enable D61B is 1 and the byte behind cal slot 0xFFF84AFC->+0 is not -1,
replaces the classified result at +2 with that byte. Caller 0x000C7A14. [O] spec 14 §4.3.

#### 0x000C7D10 `park_cal_slot_b00_set_rom` — stub
Slot 0xFFF84B00 := ROM struct 0x0004F0C4 (init 0x000A70AA). [O]

#### 0x000C7D18 `park_cal_slot_b00_set_flash` — stub
Slot 0xFFF84B00 := data-flash descriptor 0x001814B4 (init 0x000A713E). [O]

#### 0x000C7D2C `park_cal_slot_b04_set_rom` — stub
Slot 0xFFF84B04 := ROM struct 0x0004F0C8 (init 0x000A70AA). [O]

#### 0x000C7D34 `park_cal_slot_b04_set_flash` — stub
Slot 0xFFF84B04 := data-flash descriptor 0x001814B8 (init 0x000A713E). [O]

#### 0x000C7D48 `park_cal_slot_b08_set_rom` — stub
Slot 0xFFF84B08 := ROM struct 0x0004F0EC (init 0x000A70AA). [O]

#### 0x000C7D50 `park_cal_slot_b08_set_flash` — stub
Slot 0xFFF84B08 := data-flash descriptor 0x001814DC (init 0x000A713E). [O]

#### 0x000C7D82 `park_edge_e531_detect` — logic
Records the changed-flag +9 for the byte 0xFFF8E531 (getter 0x000A782A) into the multi-request
object. Called from the 0x000C7D64 stage runner. [O]

#### 0x000C7DA4 `park_edge_d671_rise` — logic
Rising-edge flag +0xC for the byte 0xFFF8D671 (value 1 after 0). [O]

#### 0x000C7DC6 `park_edge_d681_detect` — logic
Changed-flag +0xF for the byte 0xFFF8D681. [O]

#### 0x000C7DE2 `park_out_clear_cond` — logic
When 0xFFF8D5FB (0x000A7778) is 1 and the classified request at +9 is 6, 7 or 5, clears the
output byte +0xA and reports handled. Caller 0x000C7E0E. [I]

#### 0x000C7E0E `park_out_select_run` — logic
Fills the output byte +0xA: first 0x000C7DE2 (clear condition), then 0x000C7E30 (pass the
request through), else 0x000C7E52 (release codes). Caller 0x000C7D64. [O]

#### 0x000C7E30 `park_out_pass_source` — logic
Passes the request at +9 through to +0xA for values 8, 7, 5 and 6; other values leave +0xA
untouched (returns 0). [O]

#### 0x000C7E52 `park_out_release_codes` — logic
Maps intermediate codes at +9 (9, 10, 11) to a released output 6 when the last valid range
request D5FA (0x000A7746) or the park target D6D6 (0x000A7B78) is 5, 7 or 8 in the matching
combination. [I] spec 14 §3 (D5FA).

#### 0x000C7EF8 `park_multi_req_combine` — logic
Combines the four request bytes from 0xFFF8E52x (getters 0x000A77B4/0x000A77BE/0x000A77C8/
0x000A77D2) as two pairs into the object +0..+3: keeps the previous values, ORs the new pair
into +4..+7, and copies a pair over when exactly one side is 0 and the gate byte
0xFFF8E524/0xFFF8E525 (0x000A77F0/0x000A77FA) is 1; validity counter at +0x10 increments per
nonzero input, staged from the +0xC rising flag. Caller 0x000C7D64. [I]

#### 0x000C7FDA `park_out_cal_override` — logic
When the override enable D61B is 1 and the byte behind cal slot 0xFFF84B08->+0 is not -1,
replaces the multi-request output byte at +0xA. Called from the 0x000C7D64 stage runner. [O]

#### 0x000C801C `park_cal_slot_b0c_set_rom` — stub
Slot 0xFFF84B0C := ROM struct 0x0004F0F0 (init 0x000A70AA). Holds the per-lever timeout
pointers used by 0x000C805A. [O]

#### 0x000C8024 `park_cal_slot_b0c_set_flash` — stub
Slot 0xFFF84B0C := data-flash descriptor 0x001814E0 (init 0x000A713E). [O]

#### 0x000C8038 `park_timer_decrement_run` — logic
Decrements the countdown short of the timer object (expired flag at +2 as bool) and then runs
the reload logic 0x000C805A. Caller 0x000A59A4 (task stage). [O]

#### 0x000C8050 `park_timer_obj_clear` — stub
Zeroes the timer object's countdown short, state byte +4 and done byte +5. Caller 0x000A59AC.
[O]

#### 0x000C805A `park_timer_reload_by_lever` — logic
Reloads the lever-condition timer: inactive (object cleared) while 0xFFF8D655 == 1, D5F2
(0x000A772A) != 0 or 0xFFF8942C (0x000A7BAA) != 0; otherwise classifies the request byte
0xFFF894CA (0x000A78D4) — 8, or 8 with qualifier 0xFFF894C8 (0x000A78E8) = 1 treated as 6,
6, 5/7, other — loads the timeout short from the matching pointer (+8/+4/+0xC/+0) of cal
slot 0xFFF84B0C, tags the state byte (0x6C/0x6A/0xAF/0x68) and sets done +5 when the shared
countdown 0xFFF83078 has reached 0. [I]

#### 0x000C8150 `park_cal_slot_b10_set_rom` — stub
Slot 0xFFF84B10 := ROM struct 0x0004F100 (init 0x000A70AA). The monitor-condition tables of
the 0xC81xx–0xC85xx block index this slot by the byte 0xFFF8E530. [O]

#### 0x000C8158 `park_cal_slot_b10_set_flash` — stub
Slot 0xFFF84B10 := data-flash descriptor 0x001814F0 (init 0x000A713E). [O]

#### 0x000C816C `park_4x4_code_table_lookup` — logic
Two-dimensional byte lookup over range codes: maps the two codes through 0x000C81A6 (8/7/6/5
→ 0..3) and returns table[row*4 + col], or 0xFF when either code is invalid. Caller
0x000C820A. [O]

#### 0x000C81A6 `park_range_code_to_index` — library
Maps the range codes 8, 7, 6, 5 to indices 0..3; any other value returns -1. [O]

#### 0x000C81D0 `park_vtable_mask_selfcheck` — logic
When the object's flag +4 is 1, reads twice through the vtable reader at struct+0x14
(param+0xC offset) and returns 1 when the first read equals (mask-from-slot-0xFFF84B10[0]
AND second read) — a consistency/bit check on the monitored field. Caller 0x000A7A3C. [I]

#### 0x000C820A `park_4x4_lookup_apply` — logic
When the object's enable +4 is set, looks up the byte pair (request 0xFFF8941A, park target
D6D6 via 0x000A7B78) in the table behind *param_1 (0x000C816C) and writes the result byte to
param_2. Caller 0x000C85D6. [I]

#### 0x000C823E `park_mon_flag_e94b8_speed` — logic
Monitor condition: disabled while 0xFFF894B8 != 0; otherwise requires the u16 0xFFF893EA
(0x000A76EC) above the slot-0xFFF84B10 +0x10 threshold, the index byte 0xFFF8E530 (via
0x000A76C4) != 2, and the vtable-read masked with the slot's +4-index table word to be 0 —
then sets the status flag at object +4. Caller 0x000C8650. [I]

#### 0x000C8296 `park_mon_obj_ptr_e64c_set` — stub
Installs the monitor-object pointer 0xFFF8E64C into *param_1. Cluster of 4 identical copies
(0x000C8296, 0x000C8426, 0x000C8486, 0x000C84DE — same constant), all called by 0x000C8528.
[O]

#### 0x000C829E `park_mon_debounce_and_flag` — logic
Runs the per-lever indexed short from cal slot 0xFFF84AE0 into the debounce timer cell
0x000C5F14 at object +0xC; sets flag +4 = (D607 (0x000A773C) == 0 and +0x10 == 0); finishes
with the latch update 0x000C831C. Caller 0x000C8650. [I]

#### 0x000C82EE `park_mon_force_active` — logic
Selects the object pointer via 0x000C8300 and force-sets the status flag +4 and the latch
+0x31 to 1. Caller 0x000C8528. [I]

#### 0x000C8300 `park_mon_obj_ptr_select` — logic
Object pointer := 0xFFF8E64C, or 0xFFF8E66E when the index getter 0x000A780E (0xFFF8E530)
returns 1. Callers 0x000C82EE/0x000C8650. [O]

#### 0x000C831C `park_mon_latch_update` — logic
Clears latch +0x30, refreshes +0x31 from the check 0x000C86CA over object fields +0x1C/+0x18,
and when flag +4 = 1 and +0x18 != 0 demotes +4 to 0 while setting +0x30 = 1 (fault latched).
Caller 0x000C829E. [I]

#### 0x000C8352 `park_mon_flag_d606_band` — logic
Monitor condition: vtable-read masked with the slot-0xFFF84B10 +4-index word is 0, the u16
D606 (0x000A7734) is at or below the +0x18-index threshold, and D607 = 0 → sets flag +4.
Caller 0x000C8650. [I]

#### 0x000C83B0 `park_mon_obj_ptr_e65d_set` — stub
Installs the monitor-object pointer 0xFFF8E65D. Caller 0x000C8528. [O]

#### 0x000C83E8 `park_mon_flag_d60a_cond` — logic
Monitor condition: masked vtable-read nonzero and D60A (0x000A77AA) = 1 → flag +4. Caller
0x000C8650. [I]

#### 0x000C842E `park_mon_flag_d675_speed` — logic
Monitor condition, gated by 0xFFF8D675 = 1: u16 0xFFF893EA above the slot-0xFFF84B10 +0x0C
threshold, index byte 0xFFF8E530 != 2, masked vtable-read 0 → flag +4. Caller 0x000C8650.
[I]

#### 0x000C848E `park_mon_flag_d5f2_fault` — logic
Monitor condition: masked vtable-read nonzero, D5F2 (aux park request) = 1, and the struct
bytes 0xFFF8D61A[+0x72] = 1, [+0x73] = 0 → flag +4. Caller 0x000C8650. [I]

#### 0x000C84E6 `park_mon_flag_d703_cond` — logic
Monitor condition: masked vtable-read 0 and byte 0xFFF8D703 = 1 → flag +4. Caller 0x000C8650.
[I]

#### 0x000C8520 `park_mon_obj_ptr_e67f_set` — stub
Installs the monitor-object pointer 0xFFF8E67F. Caller 0x000C8528. [O]

#### 0x000C8528 `park_mon_ptrs_install` — logic
Installs the monitor-object pointers for the park monitor group: 0xFFF8E64C at sub-object
offsets +0x58/+0x40/+0x4C/+0x34 and +0, 0xFFF8E65D at +0x70, 0xFFF8E67F at +0x64, and runs
the forced-active path 0x000C82EE on the base object. Caller 0x000A58F0. [O]

#### 0x000C8564 `lib_byte_field_get` — library
Two-instruction getter: returns the byte field at a fixed offset of an object (the offset is
the only embedded constant). Giant cluster of 74 duplicated copies across the whole firmware
(0x000C8564 reads +0x30 — the monitor latch byte — while members elsewhere read +0x0E, +0x46,
+0x12, +0x1EC, +0x220 …); caller here is 0x000A7A94. [O]

#### 0x000C856A `park_mon_hyst_flag_copy` — logic
Copies the hysteresis evaluation of 0x000C8578 into the monitor object byte +0x84. Caller
0x000C8650. [O]

#### 0x000C8578 `park_mon_hysteresis_eval` — logic
Hysteresis over the u16 D606 (0x000A7734) scaled by 10 against the +0x20-index threshold of
cal slot 0xFFF84B10 (scaled by 10) and the +0x14 reference: sets +0x84 = 1 when above the
threshold, clears it at or below (threshold − reference). [I]

#### 0x000C85D6 `park_mon_4x4_apply_all` — logic
Copies the request byte 0xFFF8941A into +0x82 and applies the 4x4 code-table lookup
0x000C820A to all seven monitor sub-objects (+0, +0x34, +0x40, +0x4C, +0x58, +0x70, +0x64).
Caller 0x000C8556. [O]

#### 0x000C8650 `park_mon_conditions_run` — logic
Runs the monitor-condition set: pointer select 0x000C8300, hysteresis 0x000C856A, then the
seven condition handlers 0x000C848E, 0x000C83E8, 0x000C842E, 0x000C823E, 0x000C829E,
0x000C8352, 0x000C84E6 on the sub-objects, finally copying the resulting status bytes
(+4/+0x38/+0x44/+0x50/+0x5C/+0x68/+0x74) into the report block +0x7C..+0x83. Caller
0x000C8556. [O]

#### 0x000C86CA `lib_vtable_field_read_next` — library
Reads the current field through the object's vtable reader (+0x14, offset +0xC) and advances
the field pointer with 0x000C86F0; returns the read byte. Duplicated as 0x0015A0F6 in
r14_app (safety layer). [O]

#### 0x000C86F0 `lib_ptr_advance_by_code` — library
Advances an object pointer by 0..3 words depending on a code byte (0→+1, 1→+2, 2→+3,
3→+4, other → unchanged). Duplicated as 0x0015A11C in r14_app. [O]

#### 0x000C8914 `park_cal_slot_b14_set_rom` — stub
Slot 0xFFF84B14 := ROM struct 0x0004F134 (init 0x000A70AA). This is the park-module
calibration group used by the hydraulic-readiness state machine (spec 14 §3). [O]
spec 14 §3.

#### 0x000C891C `park_cal_slot_b14_set_flash` — stub
Slot 0xFFF84B14 := data-flash descriptor 0x00181524 (init 0x000A713E). [O]

#### 0x000C8930 `eeprom_req_queue_process` — logic
Drains the deferred-request queue at 0xFFF8ABC8 (10 records of 12 bytes: u16 id at
0xFFF8ABC8, u32 arg at 0xFFF8ABCC, type byte at 0xFFF8ABD0; count at 0xFFF85184): sets the
busy flag, runs the type-1 handler 0x000A9FF8 or type-2 handler 0x000AA45E per record, and
clears the count. Caller 0x000AE868. [I]

#### 0x000C89A4 `eeprom_req_queue_push` — logic
Appends one request record (type byte, u16 id, u32 arg) to the queue at 0xFFF8ABC8 while the
count at 0xFFF85184 is below 10. Callers 0x000AA45E/0x000A9FF8. [O]

#### 0x000C89D6 `eeprom_record_read_masked` — logic
Reads an EEPROM/data-flash record by 16-bit id: takes the type byte from the table
0xFFF823F7[id]; when the per-entry flags word 0xFFF8DBA6[id-1] has bit 0x10 set, performs the
0x0009F838 check (flash pattern/validity gate) comparing id and status word (bit 0x2000) and
masks the result through 0x000C8A40, finally ANDing with the mode byte 0xFFF8E573
(0x000A9FF0). Caller 0x000C8C4A. [I]

#### 0x000C8A40 `eeprom_result_mask_invalid` — logic
When the entry flags word 0xFFF8DBA6[id-1] has bit 0x10 set and the validity check failed,
masks the result byte with 0x50. Callers 0x000AA09A/0x000C89D6. [O]

#### 0x000C8A60 `eeprom_addr_unaligned_bad` — logic
Returns 1 (bad) when the address is 0 or not 32-byte aligned — used by the data-flash block
helpers 0x000AA358/0x000AA3DA/0x000AA41E/0x000AA39C. [I]

#### 0x000C8A70 `eeprom_record_find` — logic
Searches the 400-entry database (enumeration 0x000AA662, cursor 0x000AA604, validation
0x000A09FC) for the entry matching param_2: verifies the 0x000C8D18 lock, and — for
0x10-flagged entries or param_1 = 2 — the 0x0009F838 gate plus the reference word at
param_4 (bit 0x2000) before returning success (0), the candidate index, or 0xFFFF/0xFFFE
errors. Callers 0x000AA09A/0x000C8DDC. [I]

#### 0x000C8B94 `eeprom_write_req_validate` — logic
Parameter validation for a write/control request: requires (flags & param_1) masked by the
mode byte 0xFFF8E573 (0x000A9FF0), param_2 ∈ {0,1}, param_3 ∈ {0,1,2}, param_4 ∈ {0,1} and
param_5 ∈ {0..3} with the 4→param_5=0 restriction. Caller 0x000AA058. [I]

#### 0x000C8C06 `eeprom_record_op_check` — logic
Resolves a record id through the cursor 0x000AA604 and — only for mode 1 — runs the gated
write check 0x000C8D58, returning 1 (not found), 2 (wrong mode) or 3 (check failed). Caller
0x000C8DDC. [I]

#### 0x000C8C4A `eeprom_read_gated` — logic
Full gated read of a record: cursor lookup, 0x000A09FC id validation, the 0x000C8D18 lock
check, the thresholds 0x000C8D2A/0x000C8D58, the entry flags through 0x000AA76E and
0x0009AA00 (result masks 0xFFF8AC4C/0xFFF8AC50/0xFFF8AC40), and the masked value read
0x000C89D6 into param_3. Callers 0x000AA176/0x000AA10C. [I]

#### 0x000C8D18 `eeprom_entry_locked` — logic
Returns true when the entry's status byte +0x21 bit 0 is clear (entry not writeable/locked);
0x000C8A70 and 0x000C8C4A abort the operation in that case. [I]

## Unit summary
Continuation agent finished the remaining 145 clusters (0x000C6098–0x000C8D18) after the
first 75 (0x000C4A98–0x000C6090). Unit totals: 220 clusters, 312 named functions.

Counts by kind (whole unit): logic 143, library 89, stub 58, data-accessor 16, dispatch 6,
irq 0, entry 0, **unknown 0** (the monitor/arbiter families were all classifiable from code
+ spec 14 anchors; the eeprom_* block is inferred from its callers, marked [I] throughout).

Sub-blocks of the continuation range:
- 0xC6098–0xC64C0: calibration slot installers (ROM 0x4F0xx / flash 0x1814xx pairs).
- 0xC60CE–0xC6462: checksum-protected countdown at 0xFFF83078 (XOR 0xAAAAAAAA deltas) for the
  lever-condition timer.
- 0xC64D4–0xC6A46: lever-request output arbiter (codes 5/6/7/8 = D/N/R/P) over cal slot
  0xFFF84AF0.
- 0xC6E38–0xC7050 (+ member 0xC7D64 with its own stage set 0xC7D82–0xC7EF8): speed
  qualification and multi-request combination stages.
- 0xC70CE–0xC79B8: range-output stage — 14-candidate target chain with reason codes
  0x0A–0x8C, publishes into lever object 0xFFF8304C+0x00 (resolves spec 14 §4.3's open
  "second writer" question) and mirrors to 0xFFF8E64B.
- 0xC7A14–0xC7CCA: range-request classification incl. the spec 14 §4.4 third arbiter
  0x000C7C0C.
- 0xC8038–0xC831C: lever timer reload/debounce.
- 0xC8352–0xC8650: monitor-condition family over cal slot 0xFFF84B10 with report bytes
  +0x7C..+0x83 (fault-group wiring not identified — see leads).
- 0xC8930–0xC8D18: EEPROM/data-flash record service for the 0xAAxxx block (r08_app).

Cross-region leads:
- 0x000C79B8 is a second writer of lever object 0xFFF8304C+0x00 with calibration override
  via 0xFFF84AB0+0x18 — spec 14 §4.3/§6 should be updated.
- The 74-member cluster 0x000C8564 `lib_byte_field_get` spans r0c/r10/r14 — units 26–30
  and the safety-layer units will see the same members; same for 0x0010CD20/0x00149252
  (0x000C6228) and 0x0015A0F6/0x0015A11C (0x000C86CA/0x000C86F0).
- 0xC8930–0xC8D18 eeprom_* serve callers 0x000AA058–0x000AA76E, 0x0009F838, 0x0009AA00,
  0x000C8DDC/0x000C8D58 (r08_app and the unit after this one) — those units should
  cross-check the record/queue layout (0xFFF8ABC8 queue, 0xFFF8DBA6 flags, 0xFFF823F7
  types, 400-entry database).
- Curve evaluations run through the shared interpolator 0x0012529C (5- and 8-point) —
  other units using it can borrow the 0x000C6F44/0x000C6462 reading pattern.
