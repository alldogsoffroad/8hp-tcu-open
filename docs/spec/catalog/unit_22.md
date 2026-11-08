# Unit 22 (r08_app)

Region 0x000BAD48–0x000BF928 of the application flash (r08_app). The band opens with the
system-tick deadline library (saturating add/subtract of the 16-bit tick counters
`DAT_fff8e220`/`DAT_fff8e222` against u8/u16/u32 deadlines, spec 02 1 ms base) and its
10 ms-granularity cycle fan-out, then turns into a long bank of ROM-slot forwarding
facades: tiny stubs that call a fixed function pointer with a fixed object pointer
(`PTR_DAT_000bb1e0` gear-state object, `PTR_DAT_000bb230` gear-query object,
`PTR_DAT_000bb1d8` shift-execution object, spec 11) into the r0c_app implementation.
Cross-region call targets 0x000C0000–0x0010FFFF are the counter side of these wrappers.

---

#### 0x000BAD48 `timer_u8_add_tick` — library
Arms a byte deadline against the system tick: `*p = sat8(*p + DAT_fff8e220)` via the u8
saturation helper 0x000B782C. Companion subtractor 0x000BAD64. Callers 0x0010844E,
0x000E89BA. [O] (spec 02 timers)

#### 0x000BAD64 `timer_u8_left_tick` — library
Computes remaining time for a byte deadline: if `*p - DAT_fff8e220` does not underflow,
`*p = sat8(*p - tick)` else 0. Widely used (callers include 0x000CFAB6, 0x000CAE06,
0x000E89BA). [O]

#### 0x000BAD8C `timer_u16_add_tick` — library
u16 version of 0x000BAD48 via saturator 0x000B781E. Callers 0x000B810E, 0x0010CD0E,
0x00107D90, 0x000CDC4A. [O]

#### 0x000BADA8 `timer_u16_left_tick` — library
u16 remaining-time counterpart of 0x000BAD8C. Callers 0x000B810E, 0x000A2A60, 0x000B492C,
0x000CA60A and more. [O]

#### 0x000BADD0 `timer_u32_add_tick` — library
`*p += DAT_fff8e220` saturating to 0xFFFFFFFF on wrap (inline, no helper call). Callers
0x0011866E, 0x000D48AE. [O]

#### 0x000BADE6 `timer_u32_left_tick` — library
`*p = (*p >= tick) ? *p - DAT_fff8e220 : 0` inline. Callers 0x000E2640, 0x000B2ECA. [O]

#### 0x000BAE02 `timer_u16_add_tick2` — library
u16 arm against the second tick base `DAT_fff8e222` (callers=-: reached via pointer
table). [O]

#### 0x000BAE1E `timer_u16_left_tick2` — library
u16 remaining against `DAT_fff8e222`; callers=- (pointer table). [O]

#### 0x000BAE46 `timer_u8_add_objtime` — library
`*p2 = sat8(*p2 + *(u16*)(p1+2))` — arms a byte deadline against a per-object 16-bit time
base at struct +2 (not the global tick). 15 callers across r0c/r10. [O]

#### 0x000BAE5E `timer_u16_add_objtime` — library
u16 version of 0x000BAE46 against the object time base at +2. 15 callers. [O]

#### 0x000BAE76 `timer_u32_add_objtime` — library
u32 saturating add of the object time base at +2. Callers 0x000B7A96, 0x000B5D78. [O]

#### 0x000BAE88 `timer_u8_left_objtime` — library
Byte remaining-time against the object time base at +2. Caller 0x000E6144. [O]

#### 0x000BAEB4 `timer_cycle_delta_fanout` — logic
Computes the elapsed time since the previous call into a 4-word timer struct (`[0]`=delta
clamped at 0 when negative, `[1]`=(delta+5)/10 i.e. rounded 10 ms units, `[2]`=delta
rounded back to ms, `[3]`=last timestamp from object+0xbc of `PTR_DAT_000baf78`), arms
`+8` of the struct via 0x000BAF72, then fans out to ~25 module tick/update functions
through ROM slots `PTR_FUN_000baf80…0x000baff4` each with its own object pointer (one with
arg 0x2c). Looks like the cyclic 10 ms scheduler step of this module. Callers 0x000A2EE0,
0x000A2D5E (both pointer-table dispatchers). [I] (spec 02)

#### 0x000BAF72 `timer_u8_arm_field8` — library
`*(p+8) = sat8(*(p+8) + DAT_fff8e220)` — arm the byte deadline at struct offset 8; the
companion of 0x000BAEB4. [O]

#### 0x000BAFF8 `shiftx1d8_get_93` — stub
Forwards to thunk 0x000F06F2: returns the byte at shift-execution object+0x93
(`PTR_DAT_000bb1d8`). Caller 0x000A398E. [O] (spec 11)

#### 0x000BB006 `gstat1e0_get_s8_0` — data-accessor
Returns the signed byte at object `PTR_DAT_000bb1e0` offset 0 (no call). Caller 0x000B912C
(the interface-copy task that fills snapshot block 0xFFF89C3C). [O] (spec 09 §3.2)

#### 0x000BB00C `gstat1e4_get_u8` — data-accessor
Returns `*DAT_000bb1e4` (byte through a second ROM pointer slot). Caller 0x000B912C. [O]

#### 0x000BB014 `gstat1e0_call3_req` — stub
Calls `(*DAT_000bb1e8)(obj1e0, char, u8, u8)` returning char. Its caller 0x000B90B2 stores
the three results into DAT_fff89c41..43 — the lever/request snapshot filled by 0x000B912C.
[O] (spec 09 §3.2)

#### 0x000BB028 `gstat1e0_pred_1ec` — stub
First of a 13-member bank of no-arg gear-state predicates, each forwarding
`PTR_FUN_xxx(PTR_DAT_000bb1e0)` to a distinct thunk in r0c (0x000DB99E…0x000DBCF8) that
tests the transmission state machine (state code at obj+6 with gear values 2–8 and
transient codes 0x12–0x1e). This one: 1 unless flag dbac0 clear and both readiness
sub-checks 0. Callers 0x000CA65C etc. [I] (spec 11)

Members (same forwarding shape, different thunk target each — checked individually):
0x000BB036 `gstat1e0_pred_1f0` (thunk 0x000DB9D2), 0x000BB044 `gstat1e0_get_1f4`
(0x000DB9FA init flag), 0x000BB052 `gstat1e0_pred_1f8` (0x000DBA04 gear/state-code set),
0x000BB060 `gstat1e0_pred_1fc` (0x000DBA62 transient-state set), 0x000BB06E
`gstat1e0_pred_200` (0x000DBAE8 element-version changed), 0x000BB07C `gstat1e0_pred_204`
(0x000DBBB4 state/prev pair table), 0x000BB08A `gstat1e0_get_208` (0x000DBC86 flag),
0x000BB098 `gstat1e0_get_20c` (0x000DBC8E flag), 0x000BB0A6 `gstat1e0_get_210`
(0x000DBC96 flag), 0x000BB0B4 `gstat1e0_pred_214` (0x000DBC9E flag!=0), 0x000BB0C2
`gstat1e0_pred_218` (0x000DBCA8 lookup-table on state code), 0x000BB0D0 `gstat1e0_pred_21c`
(0x000DBCF8 prev!=0 and state not 7/0x15/0x13). [I]

#### 0x000BB0DE `gstat1e0_set_req_a` — stub
Forwards `(obj1e0, char)` to slot 0x000BB220 → thunk 0x000DBD18 → 0x000F0612: when
obj[0x2c]∈{1,2} sets obj[0x66]=1 and marks obj[0x6e]=1 when the request byte differs from
obj[0x61] — request-latch semantics. Caller 0x000D140C. Member 0x000BB0E6
`gstat1e0_set_req_b` differs in target: thunk 0x000DBD20 → 0x000F0602 clears obj[0x66],
stores the byte at obj+0x63, zeroes +0xa and invokes a callback (request-confirm path);
caller 0x00103B36 (r10 shift module). [I] (spec 11)

#### 0x000BB0EE `gstat1e0_call2_req` — stub
Forwards `(obj1e0, u8, u8)` via slot 0x000BB228 → 0x000DBD28. Caller 0x000CFD1A. [O]

#### 0x000BB0F8 `gstat1e0_op_22c` — stub
Forwards `PTR_FUN_000bb22c(obj1e0)` → 0x000DBD30. Member 0x000BB1C4 `gstat1e0_vcall_284`
targets 0x000DBD38 which walks the linked object at obj+4 and dispatches through a
vtable slot +0x2c (unresolved jump table). Callers 0x00103DE2 / 0x000A2D88, 0x000A2EE0.
[O]

#### 0x000BB100 `gstat230_gear_u16` — stub
Forwards `(obj230, u8 gear-index)` to 0x000DCEDC — an indexed dispatch (cases 4/5/6 and
default over gear-state tables) returning u16. Caller 0x000D9C7C. [I] (spec 11)

#### 0x000BB110 `gstat230_idx_bool_a` — stub
Cluster of four same-shape indexed getters on `PTR_DAT_000bb230`, each member calling a
different thunk: 0x000BB110→0x000DCF7C (bool by index, gear 3/4 special-cased, checks
state[0xb2]/[0xd5]), 0x000BB120 `gstat230_ret0` →0x000DCFC4 (returns constant 0),
0x000BB13E `gstat230_idx_bool_b` →0x000DCFDA (bool by index incl. case 6), 0x000BB15C
`gstat230_idx_validate` →0x000DD118 (validates index 0/1/2/7/6, else reports fault via
`DAT_000dd200(...,0x49c,...)` and returns 1). Callers 0x000E88FA, 0x000D9C7C, 0x000B90B2.
[I] (spec 13 fault report, spec 11)

#### 0x000BB130 `gstat230_gear6_bool` — stub
Forwards obj230 to 0x000DCFCE = indexed query with index 6 (gear 6 state check). Member
0x000BB14E `gstat230_any_bool` →0x000DD1B4 = same query with index 0xFF ("any/current").
Callers 0x000B986C / 0x000F31B2. [I]

#### 0x000BB16C `gstat230_call_250` — stub
Forwards `(obj230, u8)` through slot `DAT_000bb250` → 0x000DD16C. Caller 0x000B90B2
(snapshot filler). [O]

#### 0x000BB17C `shiftx1d8_op_254` — stub
Cluster of five no-arg operations on the shift-execution object `PTR_DAT_000bb1d8`, each
member a different ROM slot/thunk target (checked individually): 0x000BB17C →0x000F038E
(179-instr state machine gated by the five element flags `PTR_DAT_000f04a0[1..5]` vs
`DAT_fff8e3f3..3f7` — element matrix A–E, spec 11), 0x000BB184 `shiftx1d8_op_258` →
0x000F0386 (one-hop forward), 0x000BB18C `shiftx1d8_op_25c` →0x000F02EA (shift phase loop
calling 0x000F1020/0x000F0D5C per element), 0x000BB194 `shiftx1d8_op_260` →0x000EFF9C,
0x000BB19C `shiftx1d8_op_264` →0x000F063A (iterates a per-element call over offsets
+0..+6 of a parameter block). Callers 0x000A31D0, 0x000A2E20, 0x000BAEB4 and pointer
tables. [O] (spec 11 shift execution)

#### 0x000BB1A4 `iface268_op_26c` — stub
Forwards obj@slot 0x000BB268 via 0x000F2AA8: computes a u16 from a calibration pair and a
value at obj+0x68, stores it at obj+8. Caller 0x000A2D88. [O]

#### 0x000BB1AC `iface268_op_270` — stub
Forwards obj@0x000BB268 through slot `DAT_000bb270` (target unresolved, callers=-).
[O]

#### 0x000BB1B4 `iface274_op_278` — stub
Forwards obj@0x000BB274 via 0x000F2AFC: when a state byte == 4 runs 0x000F2B3A else the
five-step update 0x000F3072/0x000F2EE8/0x000F2D98/0x000F2C0E/0x000F2F28 plus 0x000F2B56.
Callers=- (pointer table). [O]

#### 0x000BB1BC `iface27c_op_280` — stub
Forwards obj@0x000BB27C via 0x000D78B8: on a mode-flag transition invokes a callback with
0/1 (enable/disable pattern). Callers=-. [O]

#### 0x000BB1CE `iface288_op_28c` — stub
Forwards obj@0x000BB288 via 0x000D78B0: writes `*obj = 0`. Callers 0x000A2D5E, 0x000A2EBC.
[O]

#### 0x000BB290 `gearsel_from_elements` — logic
Gear identification from shift-element states: queries five element objects twice each
(two function slots, arg 2) yielding 10 booleans, then a nested if-tree decodes the
combination into a gear code 0x01–0x08 (0xFF=no match, 0 when a master flag is set).
Result is debounced: a u16 stability counter at +6 increments to 0x100 while the code
matches prev (+1), resets {+6,+7,+4} on change, and timestamps (+2) come from
`PTR_FUN_000bb48c`. Driven by the r0c cyclic evaluator 0x000F43A8; reported by 0x000BB758.
[I] (spec 11 element matrix)

#### 0x000BB494 `gearsel_report_551` — logic
Publishes the five fields of the ratio-gear struct (byte, byte, s16, byte, s16) with ids
1–5 and class 0x551 through value-clamping publishers 0x000D5B32/0x000D5AD8/0x000D5BBC.
Called from evaluator 0x000F43A8. [I] (spec 13/10 reporting channel)

#### 0x000BB4EA `gearsel_from_ratios` — logic
Float companion of 0x000BB290: ten float inputs (five objects × two slots, arg 2) decoded
through a >0.0 if-tree into float codes 0.0, 1.0, 2.0 or table constants at
0x000BB6CC–0x000BB6E0 (−1.0 = no match); skipped entirely (`*p=0.0`) when a mode float ==
1.0. Hysteresis: counter float +4 vs `DAT_000bb7b8`, timestamps via `PTR_FUN_000bb7b4`,
bounded by `PTR_FUN_000bb7c0(~*PTR_DAT_000bb7bc)`. [I] (spec 11)

#### 0x000BB758 `gearsel_report2_551` — logic
Publishes the five fields of the element-gear struct (from 0x000BB290) with ids
0x65–0x69, class 0x551, then triggers op 0x44 via `PTR_FUN_000bb7d0`. Called from
0x000F43A8. [I]

#### 0x000BB7D4 `gstat890_get_flag` — data-accessor
Returns `*PTR_DAT_000bb890` — the flag that the r0c thunk 0x000DB9FA (behind wrapper
0x000BB044) exposes as "initialized". Callers 0x000E190E, 0x000E1876, 0x000DB99E. [O]

#### 0x000BB7DC `gstat890_status_bits` — stub
Calls `(*DAT_000bb894)(obj890)` → 0x000BBDC8 (own unit): ORs status bits from
DAT_fff89dd0, DAT_fff8e228, DAT_fff89e00 … into a mask byte. Caller 0x000BA050. [O]

#### 0x000BB7EA `iface898_dt_a` — stub
Returns u16 from `PTR_FUN_000bb89c(obj898)` → 0x000F6E28: difference of two tick reads,
clamped. Member 0x000BB7F8 `iface898_dt_b` uses slot 0x000BB8A0 → 0x000F6E4C: word read
from object+0x54 minus DAT_fff8cc40, clamped at 0. Both called by 0x000E14EC. [I]

#### 0x000BB806 `tbl6_state1_check2` — logic
Checks two ROM pointer tables (`PTR_DAT_000bb8a4`, `PTR_DAT_000bb8a8`) of six object
pointers, index clamped to 0–5: returns 1 if `*table_a[idx] == 1`, else 1 if
`*table_b[idx] == 1`, else 0 — a per-channel (0–5 = solenoid channels) valid/engaged test
against two alternative sources. Caller 0x000D538E. [I] (spec 12 channel table)

#### 0x000BB84C `iface8ac_idx_counter` — stub
Forwards `(obj8ac, idx)` → 0x000F6036: switch on idx 1/2/3… returning the counter words
DAT_fff8c8ac / DAT_fff8c948 / … Caller 0x000BA050. [O]

#### 0x000BB85C `iface8b4_mode_flag` — stub
Forwards obj8b4 → 0x000F645E: flag selected by the mode byte at obj+0x58 (cases 0 and 1
read through different slots, one from &DAT_fff8e490 / &DAT_fff8cb40). Caller 0x000BA050.
[I]

#### 0x000BB86A `thunk_f70b4` — stub
Bare tail-jump through ROM slot `DAT_000bb8bc` to 0x000F70B4. Callers 0x000A2D5E,
0x000A2EBC (r08 teardown dispatchers). [O]

#### 0x000BB870 `thunk_bc0f4` — stub
Bare tail-jump through `DAT_000bb8c0` to 0x000BC0F4 (own unit, the monitor evaluator).
Callers=- (pointer table). [O]

#### 0x000BB876 `thunk_f471c` — stub
Bare tail-jump through `DAT_000bb8c4` to 0x000F471C. Callers 0x000A2D5E, 0x000A2EBC. [O]

#### 0x000BB87C `thunk_f4e54` — stub
Bare tail-jump through `DAT_000bb8c8` to 0x000F4E54. Callers=-. [O]

#### 0x000BB8D4 `monstate4_alloc_zero` — library
Returns param_1, or if NULL allocates a 4-byte block via the allocator chain
0x0009AA34 (`PTR_FUN_000bbae0(4)`), then zeroes {u8 state, u8 next, u16 counter} — the
monitor-state struct shape used by 0x000BB8F6. Caller 0x0015B258 (r14/r15). [O]

#### 0x000BB8F6 `mon49_debounce_sm` — logic
Monitor state machine (op id 0x49): compares two byte inputs (0x000BBA1C /
0x000BBA22) and walks states {1,2,5,10} — state 2 requires stability and a master flag,
state 10 saturates the u16 debounce counter at +2 (0xFFFF cap) until inputs match, state
5 latches 0xFFFF into `PTR_DAT_000bbaf8`. Stores the result byte through
`PTR_FUN_000bbaf0(obj, a, b, flag)`. Driven by evaluator 0x000F43A8. [I] (spec 13 monitor
pattern)

#### 0x000BB9D8 `mon5a1_report` — logic
Publishes (id 1, 2, 5, class 0x5a1) the monitor-state byte, state byte and s16 counter
via the clamping publishers, after arming `PTR_FUN_000bbb00`. Called from 0x000F43A8.
[I]

#### 0x000BBA1C `get_flag_bbb0c` — data-accessor
Returns the signed byte at `*PTR_DAT_000bbb0c` — the "a" input of monitor 0x000BB8F6.
[O]

#### 0x000BBA22 `get_flag_bbb10` — data-accessor
Returns the signed byte at `*PTR_DAT_000bbb10` — the "b" input of monitor 0x000BB8F6.
[O]

#### 0x000BBA28 `mon49_next_state` — logic
Next-state selector for monitor 0x49: when the two inputs match, picks 2/3/5 from priority
bytes obj[1]/obj[3]/obj[8] of `PTR_DAT_000bbaec`; when they differ, tries predicate slots
`PTR_FUN_000bbb14/18/1c/20` yielding 1/4/0, default 0xFF; stores the code at param+1.
Called by 0x000BB8F6. [O]

#### 0x000BBA9A `mon49_debounce_done` — logic
Returns 1 when flag `*PTR_DAT_000bbb24` is set and the monitor's u16 counter (+2) is still
below the byte threshold `**PTR_DAT_000bbaf4`. Called by 0x000BB8F6 (state-10 exit test).
[O]

#### 0x000BBAB8 `monstate12b_alloc_zero` — library
Allocates a 12-byte (3-word) monitor-state block via allocator 0x0009AA34 when param is
NULL and zeroes it — the float-monitor struct shape of 0x000BBB28. Two byte-identical
copies live in r10_app: 0x001198DC `monstate12b_alloc_zero__2`, 0x0012E4BA
`monstate12b_alloc_zero__3` (different allocator slots only). [O]

#### 0x000BBB28 `monf_sm_debounce` — logic
Float monitor state machine: compares `FUN_000bbc6c()` vs `FUN_000bbc72(param)` (two float
inputs), arms `PTR_FUN_000bbd3c(a, b, *PTR_DAT_000bbd34, obj)`, then walks float states
{1.0, 2.0, DAT_000bbd4c, DAT_000bbd58} with a float debounce counter +2 vs
`DAT_000bbd50`, latching 0xFFFF… equivalents into `PTR_DAT_000bbd5c` via 0x000BBC78.
Output `*param_1 = 2.0` while parked in the default state. Driven by 0x000F43A8. [I]
(spec 13 monitor pattern)

#### 0x000BBC22 `mon5a1_report2` — logic
Publishes the float-monitor fields with ids 0x65/0x66/0x69, class 0x5a1, then triggers op
0x50 via `PTR_FUN_000bbd70`. Called from 0x000F43A8. [I]

#### 0x000BBC6C `get_float_bbd74` — data-accessor
Returns the float at `*PTR_DAT_000bbd74` (input "a" of 0x000BBB28). [O]

#### 0x000BBC72 `get_float_bbd78` — data-accessor
Returns the float at `*PTR_DAT_000bbd78` (input "b" of 0x000BBB28). [O]

#### 0x000BBC78 `monf_result_calc` — logic
Result evaluator for 0x000BBB28: on equal inputs takes a value from `PTR_FUN_000bbd7c`
and checks extraout flags → 0x40000000/2.0/`DAT_000bbd58`; on mismatch walks slots
bbd88/8c/90/98 writing the chosen float into param_3+4 (1.0 latch on early exit).
[O]

#### 0x000BBCFC `monf_debounce_done` — logic
Returns 1.0 when slot `PTR_FUN_000bbd40(obj)` == 1.0 and the monitor's float at +8 is
below the limit from `PTR_FUN_000bbd48(~*PTR_DAT_000bbd44)`. Called by 0x000BBB28. [O]

#### 0x000BBDA0 `monstate13_alloc_zero` — library
Allocates a 0x10-byte block (via 0x0009AA34) if NULL and zeroes the {u8,s16,u8,s16,u32,u8}
layout that 0x000BBDF4 fills. Caller 0x0015B274 (r14/r15). [O]

#### 0x000BBDC8 `mon_status_bits_or` — logic
Builds a status mask byte: bit0 from DAT_fff89dd0, bit2 from DAT_fff8e228, bit4 from
DAT_fff89e00 (and further bits below). Reached through wrapper 0x000BB7DC; also feeds the
r0c ready-check chain. [O]

#### 0x000BBDF4 `mon51_gather_status` — logic
Fills the 0xD-byte snapshot at param from the fault-flag block: `[0]`=any of
DAT_fff89dd0/fff8e228/fff89e00, `[2]`=s16 DAT_fff89dd4, `[4]`=DAT_fff89dd0, `[6]`=s16
DAT_fff89e04, `[8]`=u32 DAT_fff89e10, `[0xc]`=DAT_fff89e00; `[1]`=combined verdict from
cross-checks (DAT_fff89de0&&dd0) / DAT_fff8e229 / (DAT_fff89e14&&e00). Called by
0x000BBF5A. [O] (spec 13 flag block)

#### 0x000BBE7A `mon5b7_report_a` — logic
Publishes snapshot bytes 0 and 1 with ids 1 and 3, class 0x5b7 via `PTR_FUN_000bc01c`.
Called by 0x000BBF5A. [O]

#### 0x000BBEA6 `monstate8_alloc_zero` — library
Allocates 8 bytes via 0x0009AA34 when NULL, zeroes the first word — the two-float result
struct written by 0x000BBEC4. Caller 0x0015B274. [O]

#### 0x000BBEC4 `mon51_flag_combine` — logic
Combines float flags into two result floats at param: `[0]`=1.0 if any of
DAT_fff89de4/fff89df8/fff89e20 == 1.0; `[1]`=1.0 if (DAT_fff89df4&&de4) or DAT_fff89dfc
or (DAT_fff89e34&&e20) == 1.0. Called with &DAT_fff89dbc by 0x000BBF5A. [O]

#### 0x000BBF2E `mon5b7_report_b` — logic
Publishes the two result floats (ids 0xb and 0xd, class 0x5b7) of 0x000BBEC4 via
`PTR_FUN_000bc020`. Called by 0x000BBF5A. [O]

#### 0x000BBF5A `mon51_driver` — logic
Cyclic driver for monitor 0x51/0x5e: arms op 0x51, registers the flag pairs
(DAT_fff89dc4/dc8, dd0/de4, e00/e20, e228/df8) into compare slots, then gathers the byte
snapshot into DAT_fff89dac (0x000BBDF4), computes the float verdict into DAT_fff89dbc
(0x000BBEC4) and publishes both (0x000BBE7A, 0x000BBF2E), finally arms op 0x5e. Called by
the r0c evaluator 0x000F43A8. [I] (spec 13)

#### 0x000BC060 `monstate6c_alloc_init` — library
Allocates a 0x6c-byte block (via 0x0009AA34) if param==0, then copies 0x32 bytes from the
block base to block+100 via 0x000D56A8 (memcpy) — struct self-clone init. Callers=-
(pointer table). [O]

#### 0x000BC092 `mon3b_check_report` — logic
Monitor 0x3b/0x3c: copies the s16 input (`FUN_000bc0ce`) into param+100, checks the
result through `PTR_FUN_000bc0e4`; on failure reports (1, 0x535) via `PTR_FUN_000bc0e8`
and triggers `PTR_FUN_000bc0ec`. Called by 0x000F43A8. [I] (spec 13)

#### 0x000BC0CE `get_s16_bc0f0` — data-accessor
Returns the s16 at `*PTR_DAT_000bc0f0` — input of monitor 0x000BC092. [O]

#### 0x000BC0F4 `chan12_count_monitor` — logic
619-instr channel-quality monitor (ops 0x35→0x36): for channels 0–0xb reads a per-channel
value via `PTR_FUN_000bc438(obj, ch)` and a status word via `PTR_FUN_000bc440(obj,
0xff-ch)` (ch 0 uses slots bc434/43c, 8–0xb use the bc6f0/6fc/704/70c bank), publishes
`~status` as floats with ids 1 and 2 under class 0x5eb, and stores the pairs into
DAT_fff89eac…fff89ec2 / DAT_fff89ec4…fff89eda (12 s16 + 12 u16). Entered only through
thunk 0x000BB870 (pointer table). [I] (spec 07/13 channel counters)

#### 0x000BC686 `monstate18_alloc_zero` — library
Allocates a 0x18-byte block (12 u16) via 0x0009AA34 when NULL and zeroes all 12 words.
Caller 0x0015B300 (r14/r15). [O]

#### 0x000BC6BA `monstate18_alloc_ffff` — library
Same shape as 0x000BC686 but fills all 12 u16 with 0xFFFF (invalid marker init).
Callers=-. [O]

#### 0x000BC720 `tbl_bc74c_get_u32` — data-accessor
Returns `*(u32*)(*PTR_PTR_000bc74c + idx*4)` — indexes a ROM u32/pointer table by byte.
Eight callers incl. DID handlers 0x000A4200/0x000A3E60/0x000A3D88/0x000A3F80 and
0x000E9340/0x000E916A. [O] (spec 15 DID data)

#### 0x000BC72E `tbl_bc750_get_u32` — data-accessor
Returns `*(u32*)(*PTR_PTR_000bc750 + idx*4)` — ROM table element by byte index. Callers
0x000E90E0, 0x000A41A0/0x000A4170/0x000A4098 (DID handlers), 0x000E8CF6. [O]

#### 0x000BC738 `ret_const_2` — stub
Returns constant 2. Same caller set as 0x000BC72E (count/selector for the DID tables).
[O]

#### 0x000BC73C `tbl_bc754_get_u32` — data-accessor
Sibling table getter via `*PTR_PTR_000bc754`. Callers 0x000E9048, 0x000E8CF6. [O]

#### 0x000BC7C4 `paramrec1fd_verify` — logic
Verifies the checksummed parameter record: `PTR_FUN_000bc99c(rec, rec+4, 0xcc)` → r0c
0x000F7214 compares the u16 sum over rec+4..+0xcc with the checksum at rec+0. Callers=-.
[O] (checksum pair 0x000F71D0/0x000F7214, spec 06-style record integrity)

#### 0x000BC7DA `paramrec1fd_rechecksum` — logic
Recomputes and stores the record checksum (`PTR_FUN_000bc9a0` → 0x000F71D0 over rec+4,
0xcc bytes into rec[0..1]) and sets the dirty flag `*PTR_DAT_000bc9a4 = 1`. Callers=-.
[O]

#### 0x000BC806 `paramrec1fd_id_ok` — data-accessor
Returns `*(short*)PTR_DAT_000bc9a8 == 0x1fd` — record-id gate shared by the whole
0xBC7C4–0xBCC88 accessor bank. [O]

#### 0x000BC82C `paramrec1fd_ready` — logic
Ready gate: `*(&DAT_fff89edc) == 0x80` (subsystem state word, read via 0x000BC816) AND
busy flag `*PTR_DAT_000bc9ac == 0`. Twelve callers inside the accessor bank. [O]

#### 0x000BC850 `paramrec1fd_not_busy` — data-accessor
Returns `*PTR_DAT_000bc9ac == 0`. Eight callers. [O]

#### 0x000BC85A `paramrec1fd_default_3039` — logic
When the record is idle: writes the u16 default 0x3039 (12345) into rec+4 through the
checksum-update helper 0x000F71EC, mirrors it into `*PTR_DAT_000bc9b4`, sets dirty.
Callers=-. [O]

#### 0x000BC8A0 `paramrec_reset_seq` — logic
Reset wrapper: clear the 0xb4-byte array at +6 (0x000BC8B6), zero the u16 at +0xba
(0x000BC954 with param+1) and the u16 at +0xbc (0x000BCA1C with param+2) of the record.
Six byte-identical members (checked): 0x000BD576, 0x000BD89A, 0x000BDAEE, 0x000BF798,
0x000BFD46 operate their own record sections via local op triples; 0x000C00A2
(`paramrec_reset_seq__7`) is the r0c copy. Callers 0x000CA176, 0x000BF6AA, 0x000BFC58,
0x000BFFB4 and pointer tables. [O]

#### 0x000BC8B6 `paramrec1fd_clear_array` — logic
Clears 0xb4 single bytes at record+6 one per iteration (each through checksum-update
0x000F71EC), sets dirty. Called by 0x000BC8A0. [O]

#### 0x000BC912 `paramrec1fd_write_byte6` — logic
Writes one byte (top byte of param_3) at record+6+(idx&0xffff) via 0x000F71EC and mirrors
it into the shadow array `PTR_DAT_000bc9b8[idx]`; sets dirty. Caller 0x000B767E. [O]

#### 0x000BC954 `paramrec1fd_zero_ba` — logic
Zero-initialises the u16 at record+0xba via checksum-update (len 2) and the mirror
`*PTR_DAT_000bc9bc`; sets dirty. [O]

#### 0x000BC9C0 `paramrec1fd_get_ba` — data-accessor
Returns the s16 mirror `*PTR_DAT_000bcbb8` (record section at bcbbc, field +0xba) when
the record is ready. Caller 0x000B767E. [O]

#### 0x000BC9E2 `paramrec1fd_set_ba` — logic
Writes the upper half of param_2 as u16 at bcbbc+0xba (checksum-updated) and into the
mirror; sets dirty. Caller 0x000B767E. [O]

#### 0x000BCA1C `paramrec1fd_zero_bc` — logic
Zeroes the u16 at bcbbc+0xbc (checksum-updated), mirror `*PTR_DAT_000bcbc8`, dirty.
[O]

#### 0x000BCA60 `paramrec1fd_reset_be` — logic
Reset pair: 0x000BCA70 sets byte 0x32 (50) default at bcbbc+0xbe, 0x000BCB10 zeroes
bcbbc+0xbf. Members 0x000BF4B8/0x000BFAC2 (own unit) and 0x000C0852 (r0c) are the same
two-step reset for other record sections (0x000BF5A4 clears 12 u16 at +0xd2c of record
PTR_DAT_000bf610). Callers 0x000BF3CA, 0x000C0764, pointer tables. [O]

#### 0x000BCA70 `paramrec1fd_set_be_50` — logic
Checksum-updated byte write of 0x32 at bcbbc+0xbe, mirror `*PTR_DAT_000bcbcc`, dirty.
[O]

#### 0x000BCAB4 `paramrec1fd_get_be` — data-accessor
Returns the byte mirror `*PTR_DAT_000bcbcc` when ready. Caller 0x000D22BE. [O]

#### 0x000BCAD6 `paramrec1fd_set_be` — logic
Checksum-updated byte write at bcbbc+0xbe from param_2, mirror update, dirty. Caller
0x000D229E. [O]

#### 0x000BCB10 `paramrec1fd_zero_bf` — logic
Zeroes the byte at bcbbc+0xbf (checksum-updated), dirty. [O]

#### 0x000BCB54 `paramrec1fd_clear_c0_5` — logic
Clears five u16 at record bce64+0xc0 (each checksum-updated via `PTR_FUN_000bce68` →
0x000F71EC), dirty `*PTR_DAT_000bce6c`. Callers=-. [O]

#### 0x000BCBD4 `paramrec1fd_get_c0_idx` — data-accessor
Returns the s16 at bce64+0xc0+idx*2 when the record is ready. Caller 0x000E3836. [O]

#### 0x000BCC00 `paramrec1fd_set_c0_idx` — logic
Checksum-updated u16 write at bce64+0xc0+idx*2, dirty. Caller 0x000E3836. [O]

#### 0x000BCC44 `paramrec1fd_zero_cc` — logic
Zeroes the u32 at bce64+0xcc (checksum-updated, len 4), mirror `*PTR_DAT_000bce70`,
dirty. Callers=-. [O]

#### 0x000BCC88 `paramrec108_get_cc` — data-accessor
Returns the u32 mirror `*PTR_DAT_000bce70` when record ready. [O]

#### 0x000BCCAA `paramrec108_set_cc` — logic
Checksum-updated u32 write at record bce64+0xcc, mirror, dirty. [O]

#### 0x000BCCDE `objrec_vt_reset_apply` — logic
Vtable-driven record-object operation: calls method +0xa4 (arg obj+0x9c+param); if it
returns true, method +0x68, clears `*DAT_000bce74`, runs the 8-section record reset
0x000BCDC8 on the state at DAT_fff89eec, then methods +0x50 and +0x80. Callers 0x000CA176,
0x000BDCE8, 0x000BD778, 0x000A3E60 (DID handler). [I]

#### 0x000BCD2A `paramrec108_verify` — logic
Verifies record 0x108: checksum at bce64+0xd0 over 0x2e bytes at +0xd4 (via 0x000F7214).
[O]

#### 0x000BCD44 `paramrec108_rechecksum` — logic
Recomputes the checksum into bce64+0xd0 (via 0x000F71D0) and sets dirty
`*PTR_DAT_000bce6c`. [O]

#### 0x000BCD74 `paramrec108_id_ok` — data-accessor
Returns `*(short*)PTR_DAT_000bce80 == 0x108` — id gate of the second parameter-record
subsystem (state at DAT_fff89ee4). [O]

#### 0x000BCD9A `paramrec108_ready` — logic
Ready gate: state word at DAT_fff89ee4 == 0x80 (via 0x000BCD84) and busy flag
`*PTR_DAT_000bce84 == 0`. [O]

#### 0x000BCDBE `paramrec108_not_busy` — data-accessor
Returns `*PTR_DAT_000bce84 == 0`. [O]

#### 0x000BCDC8 `paramrec108_reset_all` — logic
Eight-section reset chain for record 0x108: clears 5×u16 at +0xd4 (0x000BCDFC), 5×u16 at
bd128+0xde (0x000BCEF4), 5 bytes at +0xe8 and +0xed (0x000BCFC8/0x000BD09C on bd128),
5 bytes at +0xf2/+0xf7/+0xfc (0x000BD186/0x000BD25A/0x000BD32E on bd404). Called by
0x000BCCDE. [O]

#### 0x000BCDFC `paramrec108_zero_d4` — logic
Clears five u16 at bce64+0xd4 (each checksum-updated against base +0xd0), dirty
`*PTR_DAT_000bce6c`. [O]

#### 0x000BCE88 `paramrec108_get_arr_d4` — data-accessor
Returns the s16 at bd128+0xd4+idx*2 when ready. Cluster member 0x000BCF5C
`paramrec108_get_arr_de` reads the second array at bd128+0xde+idx*2 (same shape,
different field — checked). Callers 0x000B4046, 0x000B45E0. [O]

#### 0x000BCEAC `paramrec108_set_arr_d4` — logic
Checksum-updated u16 write at bd128+0xd4+idx*2, dirty `*PTR_DAT_000bd130`. Member
0x000BCF80 `paramrec108_set_arr_de` targets +0xde. Callers 0x000B4372, 0x000B3B4C,
0x000B5086, 0x000B54E4. [O]

#### 0x000BCEF4 `paramrec108_zero_de` — logic
Clears five u16 at bd128+0xde, checksum-updated, dirty. [O]

#### 0x000BCFC8 `paramrec108_zero_e8` — logic
Clears five bytes at bd128+0xe8; member 0x000BD09C `paramrec108_zero_ed` clears +0xed.
[O]

#### 0x000BD02C `paramrec108_get_e8` — data-accessor
Returns mirror byte `PTR_DAT_000bd134[idx]` when ready. Callers 0x000B4046, 0x000B3C80,
0x000B5086, 0x000B45E0, 0x000B54E4. [O]

#### 0x000BD052 `paramrec108_set_e8` — logic
Checksum-updated byte write at bd128+0xe8+idx, mirror, dirty. [O]

#### 0x000BD100 `paramrec108_get_ed` — data-accessor
Returns mirror byte `PTR_DAT_000bd138[idx]` when ready. [O]

#### 0x000BD13C `paramrec108_set_ed` — logic
Checksum-updated byte write at bd128+0xed+idx (slot `PTR_FUN_000bd408`), mirror, dirty.
[O]

#### 0x000BD186 `paramrec108_zero_f2` — logic
Clears five bytes at record bd404+0xf2. Cluster members (field offset differs, checked):
0x000BD25A `paramrec108_zero_f7` (+0xf7), 0x000BD32E `paramrec108_zero_fc` (+0xfc);
dirty `*PTR_DAT_000bd410`. [O]

#### 0x000BD1EA `paramrec108_get_f2` — data-accessor
Returns mirror byte `PTR_DAT_000bd414[idx]` when ready. [O]

#### 0x000BD210 `paramrec108_set_f2` — logic
Checksum-updated byte write at bd404+0xf2+idx, mirror, dirty. [O]

#### 0x000BD2BE `paramrec108_get_f7` — data-accessor
Returns mirror byte `PTR_DAT_000bd418[idx]` when ready. [O]

#### 0x000BD2E4 `paramrec108_set_f7` — logic
Checksum-updated byte write at bd404+0xf7+idx, mirror, dirty. [O]

#### 0x000BD392 `paramrec108_get_fc` — data-accessor
Returns mirror byte `PTR_DAT_000bd41c[idx]` when ready. [O]

#### 0x000BD3B8 `paramrec108_set_fc` — logic
Checksum-updated byte write at bd404+0xfc+idx, mirror, dirty. The five-entry byte/word
arrays across bd128/bd404 (consumed by the 0x000B3B4C–0x000B54E4 adaptation callers)
look like per-element/per-gear adaptation storage. [I] (spec 11 adaptation values)

#### 0x000BD420 `paramrec11c_zero_101` — logic
Zeroes the byte at record bd670+0x101 (checksum base +0xd0→+0x102 area), mirrors
`*PTR_DAT_000bd678`, dirty. Part of the reset chain 0x000BCDC8. [O]

#### 0x000BD4CE `paramrec11c_verify` — logic
Verifies record 0x11c: checksum at bd670+0x102 over 0xa6 bytes at +0x106 (0x000F7214).
[O]

#### 0x000BD4EC `paramrec11c_rechecksum` — logic
Recomputes checksum into bd670+0x102 (0x000F71D0), dirty `*PTR_DAT_000bd67c`. [O]

#### 0x000BD520 `paramrec11c_id_ok` — data-accessor
Returns `*(short*)PTR_DAT_000bd68c == 0x11c` — id gate of the third parameter-record
subsystem (state DAT_fff89ef4, buffers bd670/bd91c/bdbe8). [O]

#### 0x000BD548 `paramrec11c_ready` — logic
Ready gate: state DAT_fff89ef4 == 0x80 (via 0x000BD530) and busy
`*PTR_DAT_000bd690 == 0`. 45 callers — this is the master gate of the whole
DID-backed value bank. [O]

#### 0x000BD56C `paramrec11c_not_busy` — data-accessor
Returns `*PTR_DAT_000bd690 == 0`. [O]

#### 0x000BD58C `paramrec11c_zero_106` — logic
Clears five u16 at bd670+0x106 (checksum base +0x102), dirty. [O]

#### 0x000BD5F6 `paramrec11c_get_106` — data-accessor
Returns the s16 at bd670+0x106+idx*2 when ready. Callers 0x000A2268, 0x000A2AC4,
0x000A1572, 0x000A1454 (DID read handlers). [O]

#### 0x000BD624 `paramrec11c_set_106` — logic
Checksum-updated u16 write at bd670+0x106+idx*2, dirty. Callers 0x000A2C3A, 0x000A1454
(DID write path). [O]

#### 0x000BD694 `paramrec11c_zero_110` — logic
Clears five u16 at bd91c+0x110 (checksum base +0x102), dirty `*PTR_DAT_000bd924`. [O]

#### 0x000BD6FE `paramrec11c_get_110` — data-accessor
Returns the s16 at bd91c+0x110+idx*2 when ready. Callers 0x000A1B94, 0x000A2B76,
0x000A13E6, 0x000A2AC4, 0x000A1572, 0x000A23F4, 0x000A1C60, 0x000A1A42. [O]

#### 0x000BD72C `paramrec11c_set_110` — logic
Checksum-updated u16 write at bd91c+0x110+idx*2. Member 0x000BD84E
`paramrec11c_set_11a` targets the +0x11a array (checked). Callers 0x000A2C88,
0x00125392 / 0x000A1712, 0x00125364. [O]

#### 0x000BD778 `paramrec11c_reset_seq1` — logic
Reset sequence gated on request flag `*PTR_DAT_000bd928 == 1`: clears the flag, then
runs 0x000BCCDE (vtable record reset), 0x000BD694, 0x000BD58C, 0x000BDE0C, 0x000BDF04,
slots `PTR_FUN_000bd92c/930`, and clears five u16 at bd91c+0x11a; dirty + flag++.
Callers 0x000CA176, 0x000BD576. [O]

#### 0x000BD828 `paramrec11c_get_11a` — data-accessor
Returns the s16 at bd91c+0x11a+idx*2 when ready. Callers 0x000A1712, 0x000A15FE. [O]

#### 0x000BD8B0 `paramrec11c_zero_124` — logic
Zeroes the u16 at bd91c+0x124, mirror `*PTR_DAT_000bd934`, dirty. [O]

#### 0x000BD8FA `paramrec11c_get_124` — data-accessor
Returns the s16 mirror `*PTR_DAT_000bd934` when ready. Callers 0x000A1572, 0x000A23F4,
0x000A1454. [O]

#### 0x000BD938 `paramrec11c_set_124` — logic
Checksum-updated u16 write at bdbe8+0x124, mirror `*PTR_DAT_000bdbf4`, dirty. [O]

#### 0x000BD978 `paramrec11c_zero_126` — logic
Zeroes the u16 at bdbe8+0x126, mirror `*PTR_DAT_000bdbf8`, dirty. [O]

#### 0x000BD9C2 `paramrec11c_get_126` — data-accessor
Returns the s16 mirror `*PTR_DAT_000bdbf8` when ready. [O]

#### 0x000BD9E4 `paramrec11c_set_126` — logic
Checksum-updated u16 write at bdbe8+0x126, mirror, dirty. [O]

#### 0x000BDA24 `paramrec11c_reset_seq2` — logic
Reset sequence gated on `*PTR_DAT_000bdbfc == 1`: clears flag, 0x000BD978, 0x000BD8B0,
zeroes u16 at bdbe8+0x128, mirror `*PTR_DAT_000bdc00`, dirty, flag++. Callers 0x000CA176,
0x000BD89A. [O]

#### 0x000BDA92 `paramrec11c_get_128` — data-accessor
Returns the s16 mirror `*PTR_DAT_000bdc00` when ready. Callers 0x000A16B8, 0x000A1886. [O]

#### 0x000BDAAE `paramrec11c_set_128` — logic
Checksum-updated u16 write at bdbe8+0x128, mirror, dirty. Caller 0x000A1886. [O]

#### 0x000BDB04 `paramrec11c_zero_12a` — logic
Clears five u16 at bdbe8+0x12a, dirty. Called by 0x000BDCE8, 0x000A3E60, 0x000BDAEE. [O]

#### 0x000BDB6E `paramrec11c_get_12a` — data-accessor
Returns the s16 at bdbe8+0x12a+idx*2 when ready. Callers 0x000A22FA, 0x000A1454. The
0x000A12xx–0x000A2Cxx caller bank (unit 17's DID read/write handlers) ties records
0x1FD/0x108/0x11C to UDS DID persistence. [I] (spec 15)

#### 0x000BDB9C `paramrec11c_set_12a` — logic
Checksum-updated u16 write at bdbe8+0x12a+idx*2, dirty. Callers 0x000A2186, 0x000A1454. [O]

#### 0x000BDC04 `paramrec11c_zero_134` — logic
Clears five u16 at bde9c+0x134 (chk base +0x102), dirty `*PTR_DAT_000bdea4`. [O]

#### 0x000BDC6E `paramrec11c_get_134` — data-accessor
Returns the s16 at bde9c+0x134+idx*2 when ready. Callers 0x000A220C, 0x000A1362,
0x000A22FA, 0x000A2186. [O]

#### 0x000BDC9C `paramrec11c_set_134` — logic
Checksum-updated u16 write at bde9c+0x134+idx*2. Member 0x000BDDB0
`paramrec11c_set_13e` targets +0x13e. Callers 0x00125336, 0x000A2186, 0x000A20E8 /
0x000A2028, 0x00125308. [O]

#### 0x000BDCE8 `paramrec11c_reset_seq3` — logic
Reset sequence gated on `*PTR_DAT_000bdea8 == 1`: vtable reset via `PTR_FUN_000bdeac`
(0x000BCCDE shape), 0x000BDC04, 0x000BDB04, slots `PTR_FUN_000bdeb0/bdeb4`, then clears
25 u16 at bde9c+0x148; dirty, flag++. Callers 0x000CA176, 0x000BDAEE. [O]

#### 0x000BDD8A `paramrec11c_get_13e` — data-accessor
Returns the s16 at bde9c+0x13e+idx*2 when ready. Callers 0x000A1FA4, 0x000A1F90,
0x000A12FC. [O]

#### 0x000BDDFC `paramrec_clear_pair` — logic
Cluster of nine two-step clear wrappers, each "clear 0x1c0 bytes of record area X, then
clear array field (+param*4) of record Y": 0x000BDDFC (BDE0C→BDF04), 0x000BE0DC
`__2` (BE0EC→BE204), 0x000BE2E8 `__3` (2F8→3EE), 0x000BE5DC `__4` (5EC→6E2), 0x000BE7E8
`__5` (7F8→8EE), 0x000BEAEA `__6` (EAFA→EBF0), 0x000BECD4 `__7` (ECE4→EDEC),
0x000BEFCC `__8` (EFDC→F0F0), 0x000BF1D4 `__9` (F1E4→F2D8). All feed the vtable reset
drivers (0x000BDFE0/0x000BE4E0/…); callers 0x000CA176, 0x000BE9D2, 0x000BEED0. [O]

#### 0x000BDE0C `paramrec11c_zero_148_25` — logic
Clears 25 u16 at bde9c+0x148 (checksum-updated), dirty. Called by 0x000BDDFC, 0x000BD778.
[O]

#### 0x000BDF04 `paramrec11c_zero_17a_25` — logic
Clears 25 u16 at be178+0x17a (chk base +0x102), dirty `*PTR_DAT_000be180`. [O]

#### 0x000BDFE0 `objrec_vt_reset2` — logic
Vtable-driven record reset (same shape as 0x000BCCDE): method +0xa4 test, method +0x68,
clear `*DAT_000be184`, run clear pairs 0x000BE0DC/0x000BE2E8 (states DAT_fff89f0c/f014),
methods +0x50/+0x80. Caller 0x000CA176. [O]

#### 0x000BE034 `paramrec13a_verify` — logic
Verifies record instance A (id 0x13a): checksum at be178+0x1ac over 0x39c bytes at +0x1b0.
[O]

#### 0x000BE052 `paramrec13a_rechecksum` — logic
Recomputes checksum into be178+0x1ac, dirty `*PTR_DAT_000be180`. [O]

#### 0x000BE086 `paramrec13a_id_ok` — data-accessor
Returns `*(short*)PTR_DAT_000be190 == 0x13a`. [O]

#### 0x000BE0AE `paramrec13a_ready` — logic
Ready gate: state DAT_fff89f04 == 0x80 and busy `*PTR_DAT_000be194 == 0`. [O]

#### 0x000BE0D2 `paramrec13a_not_busy` — data-accessor
Returns `*PTR_DAT_000be194 == 0`. [O]

#### 0x000BE0EC `paramrec13a_clear_1b0` — logic
Clears 0x1c0 bytes at be178+0x1b0 (each checksum-updated vs base +0x1ac), dirty. [O]

#### 0x000BE204 `paramrec13a_zero_370` — logic
Clears seven u16 at be488+0x370 (chk base +0x1ac), dirty `*PTR_DAT_000be490`. Member
0x000BE3EE `paramrec13a_zero_53e` clears +0x53e. Callers 0x000BE0DC/0x000A3E60 and
0x000A3E60/0x000BE2E8. [O]

#### 0x000BE2F8 `paramrec13a_clear_37e` — logic
Clears 0x1c0 bytes at be488+0x37e (chk base +0x1ac), dirty. Called by 0x000BE2E8. [O]

#### 0x000BE4E0 `objrec_vt_reset3` — logic
Vtable-driven record reset (0x000BCCDE shape) with clear pair 0x000BE5DC/0x000BE7E8 and
states DAT_fff89f24/f2c. Caller 0x000CA176. [O]

#### 0x000BE534 `paramrec13a2_verify` — logic
Verifies record instance B (second id-0x13a object): checksum at be77c+0x54c over 0x39c
bytes at +0x550. [O]

#### 0x000BE552 `paramrec13a2_rechecksum` — logic
Recomputes checksum into be77c+0x54c, dirty `*PTR_DAT_000be784`. [O]

#### 0x000BE586 `paramrec13a2_id_ok` — data-accessor
Returns `*(short*)PTR_DAT_000be794 == 0x13a` (instance B id field). [O]

#### 0x000BE5AE `paramrec13a2_ready` — logic
Ready gate: state DAT_fff89f1c == 0x80 and busy `*PTR_DAT_000be798 == 0`. [O]

#### 0x000BE5D2 `paramrec13a2_not_busy` — data-accessor
Returns `*PTR_DAT_000be798 == 0`. [O]

#### 0x000BE5EC `paramrec13a2_clear_550` — logic
Clears 0x1c0 bytes at be77c+0x550 (chk base +0x54c), dirty. Called by 0x000BE5DC. [O]

#### 0x000BE6E2 `paramrec13a2_zero_710` — logic
Clears seven u16 at be77c+0x710 (chk base +0x54c), dirty. Called by 0x000BE5DC, 0x000A3E60.
[O]

#### 0x000BE7F8 `paramrec13a2_clear_71e` — logic
Clears 0x1c0 bytes at bea90+0x71e (chk base +0x54c), dirty `*PTR_DAT_000bea98`. [O]

#### 0x000BE8EE `paramrec13a2_zero_8de` — logic
Clears seven u16 at bea90+0x8de, dirty. Called by 0x000BE7E8, 0x000A3E60. [O]

#### 0x000BE9D2 `objrec_vt_reset4` — logic
Vtable-driven record reset (0x000BCCDE shape) with clear pairs 0x000BEAEA/0x000BECD4 and
states DAT_fff89f3c/f44. Caller 0x000CA176. [O]

#### 0x000BEA26 `paramrec13a3_verify` — logic
Verifies the third section of bea90: checksum at +0x8ec over 0x318 bytes at +0x8f0. [O]

#### 0x000BEA44 `paramrec13a3_rechecksum` — logic
Recomputes checksum into bea90+0x8ec, dirty. [O]

#### 0x000BEA78 `paramrec112_id_ok` — data-accessor
Returns `*(short*)PTR_DAT_000beaa8 == 0x112` — record id 0x112 (cf. EEPROM block 0x0112,
total length 0x140, in the project's record map). [I] (spec 06/EEPROM map)

#### 0x000BEABC `paramrec112_ready` — logic
Ready gate: state DAT_fff89f34 == 0x80 and busy `*PTR_DAT_000bed70 == 0`. [O]

#### 0x000BEAE0 `paramrec112_not_busy` — data-accessor
Returns `*PTR_DAT_000bed70 == 0`. [O]

#### 0x000BEAFA `paramrec112_clear_8f0` — logic
Clears 0x180 bytes at bed74+0x8f0 (chk base +0x8ec), dirty `*PTR_DAT_000bed7c`. Member
0x000BECE4 `paramrec112_clear_a7c` clears +0xa7c. [O]

#### 0x000BEBF0 `paramrec112_zero_a70` — logic
Clears six u16 at bed74+0xa70, dirty. Called by 0x000BEAEA, 0x000A3E60. [O]

#### 0x000BEDEC `paramrec112_zero_bfc` — logic
Clears six u16 at bf040+0xbfc (chk base +0x8ec), dirty `*PTR_DAT_000bf048`. Called by
0x000BECD4, 0x000A3E60. [O]

#### 0x000BEED0 `objrec_vt_reset5` — logic
Vtable-driven record reset with clear pairs 0x000BEFCC/0x000BF1D4 and states
DAT_fff89f54/f5c. Caller 0x000CA176. [O]

#### 0x000BEF24 `paramrec112_verify2` — logic
Verifies bf040: checksum at +0xc08 over 0x104 bytes at +0xc0c. [O]

#### 0x000BEF42 `paramrec112_rechecksum2` — logic
Recomputes checksum into bf040+0xc08, dirty. [O]

#### 0x000BEF76 `paramrec1d0_id_ok` — data-accessor
Returns `*(short*)PTR_DAT_000bf058 == 0x1d0` — record id 0x1d0. [O]

#### 0x000BEF9E `paramrec1d0_ready` — logic
Ready gate: state DAT_fff89f4c == 0x80 and busy `*PTR_DAT_000bf05c == 0`. [O]

#### 0x000BEFC2 `paramrec1d0_not_busy` — data-accessor
Returns `*PTR_DAT_000bf05c == 0`. [O]

#### 0x000BEFDC `paramrec1d0_clear_c0c` — logic
Clears 0x78 bytes at bf040+0xc0c (chk base +0xc08), dirty. [O]

#### 0x000BF0F0 `paramrec1d0_zero_c84` — logic
Clears five u16 at bf344+0xc84 (chk base +0xc08), dirty `*PTR_DAT_000bf34c`. Member
0x000BF2D8 `paramrec1d0_zero_d06` clears +0xd06. Called by 0x000BEFCC/0x000A3E60 and
0x000BF1D4/0x000A3E60. [O]

#### 0x000BF1E4 `paramrec1d0_clear_c8e` — logic
Clears 0x78 bytes at bf344+0xc8e, dirty. Called by 0x000BF1D4. [O]

#### 0x000BF3CA `objrec_vt_reset6` — logic
Vtable-driven record reset with single clear pair 0x000BF4B8 and state DAT_fff8e230.
Caller 0x000CA176. [O]

#### 0x000BF416 `paramrec155_verify` — logic
Verifies record bf610: checksum at +0xd10 over 0x30 bytes at +0xd14. [O]

#### 0x000BF432 `paramrec155_rechecksum` — logic
Recomputes checksum into bf610+0xd10, dirty `*PTR_DAT_000bf618`. [O]

#### 0x000BF464 `paramrec155_id_ok` — data-accessor
Returns `*(short*)PTR_DAT_000bf628 == 0x155` — record id 0x155 (state at DAT_fff89f64,
the subsystem behind 0x000BF5A4/0x000BF4B8). [O]

#### 0x000BF48A `paramrec155_ready` — logic
Ready gate for record 0x155: state DAT_fff89f64 == 0x80 (via 0x000BF474) and busy
`*PTR_DAT_000bf62c == 0`. Callers 0x000CF0A4, 0x000CF48C and the field accessors below.
[O]

#### 0x000BF4AE `paramrec155_not_busy` — data-accessor
Returns `*PTR_DAT_000bf62c == 0`. [O]

#### 0x000BF4C8 `paramrec155_zero_d14_12` — logic
Clears twelve u16 at bf610+0xd14 (chk base +0xd10), dirty. Member 0x000BF5A4
`paramrec155_zero_d2c_12` clears twelve u16 at bf8c0+0xd2c (checked). Called by
0x000BF4B8, 0x000A3E60. [O]

#### 0x000BF532 `paramrec155_get_d14` — data-accessor
Returns the s16 at bf610+0xd14+idx*2 when ready. Callers 0x000CF0A4, 0x000CF1EC,
0x000CEEB4. [O]

#### 0x000BF558 `paramrec155_set_d14` — logic
Checksum-updated u16 write at bf610+0xd14+idx*2, dirty. Caller 0x000CF1EC. [O]

#### 0x000BF630 `paramrec155_get_d2c` — data-accessor
Returns the s16 at bf8c0+0xd2c+idx*2 when ready. Caller 0x000CF1EC. [O]

#### 0x000BF65E `paramrec155_set_d2c` — logic
Checksum-updated u16 write at bf8c0+0xd2c+idx*2 (chk base +0xd10), dirty
`*PTR_DAT_000bf8c8`. Caller 0x000CF1EC. [O]

#### 0x000BF6AA `objrec_vt_reset7` — logic
Vtable-driven record reset with single clear pair 0x000BF798 and state DAT_fff8e62f.
Caller 0x000CA176. [O]

#### 0x000BF6F6 `paramrec155_verify2` — logic
Verifies bf8c0: checksum at +0xd44 over 6 bytes at +0xd48. [O]

#### 0x000BF712 `paramrec155_rechecksum2` — logic
Recomputes checksum into bf8c0+0xd44, dirty `*PTR_DAT_000bf8c8`. [O]

#### 0x000BF744 `paramrec1c7_id_ok` — data-accessor
Returns `*(short*)PTR_DAT_000bf8d8 == 0x1c7` — record id 0x1c7 (state DAT_fff89f6c,
buffers bf8c0/bfb80). [O]

#### 0x000BF76A `paramrec1c7_ready` — logic
Ready gate: state DAT_fff89f6c == 0x80 and busy `*PTR_DAT_000bf8dc == 0`. Callers incl.
0x000E0868, 0x000E0A7A, 0x000DE688. [O]

#### 0x000BF78E `paramrec1c7_not_busy` — data-accessor
Returns `*PTR_DAT_000bf8dc == 0`. [O]

#### 0x000BF7AE `paramrec1c7_zero_d48` — logic
Zeroes the u16 at bf8c0+0xd48 (chk base +0xd44), mirror `*PTR_DAT_000bf8e0`, dirty.
[O]

#### 0x000BF7F8 `paramrec1c7_get_d48` — data-accessor
Returns the s16 mirror `*PTR_DAT_000bf8e0` when ready. Callers 0x000E0D5E, 0x000E0868. [O]

#### 0x000BF814 `paramrec1c7_set_d48` — logic
Checksum-updated u16 write at bf8c0+0xd48, mirror, dirty. Caller 0x000E0868. [O]

#### 0x000BF854 `paramrec1c7_zero_d4a` — logic
Zeroes the u16 at bf8c0+0xd4a, mirror `*PTR_DAT_000bf8e4`, dirty. Called by 0x000A3E60,
0x000BF798. [O]

#### 0x000BF89E `paramrec1c7_get_d4a` — data-accessor
Returns the s16 mirror `*PTR_DAT_000bf8e4` when ready. Caller 0x000DE688. [O]

#### 0x000BF8E8 `paramrec1c7_set_d4a` — logic
Checksum-updated u16 write at bfb80+0xd4a (chk base +0xd44), mirror `*PTR_DAT_000bfb8c`,
dirty. Callers 0x000E08AC, 0x000DE688. [O]

#### 0x000BF928 `paramrec1c7_zero_d4c` — logic
Zeroes the u16 at bfb80+0xd4c, mirror `*DAT_000bfb90`, dirty. Called by 0x000CA176,
0x000A3E60, 0x000BF798. Last cluster of the unit. [O]


## Unit summary
220 clusters, 273 named functions (reps + members). Kinds: 150 logic, 53 data-accessor,
48 stub, 22 library, 0 dispatch/irq/entry counted separately (the vtable reset drivers are
filed under logic), 0 unknown.

Content: (1) timer tick helpers (0x000BAD48–0x000BAF72); (2) interface thunks into the
gear-state/shift-execution objects (0x000BAFF8–0x000BB1CE); (3) gear-selection from elements
and ratios + 0x551 reporting (0x000BB290–0x000BB758); (4) monitor state machines and
0x5a1/0x5b7/0x535/0x5eb fault publishing (0x000BB7D4–0x000BC0F4); (5) ROM table accessors for
DID data (0x000BC720–0x000BC73C); (6) the bulk of the unit: EEPROM parameter-record library
(0x000BC7C4–0x000BF928) — shadow buffers with additive byte checksums, ready/busy/default
predicates per record manager (state objects DAT_fff89edc/ee4/ef4/f0c..f9c), checksum-adjusted
byte/word writes with RAM mirrors, block zero/reset cascades, and vtable-driven reset drivers.

Unknown entries: none — every function got at least a structural name anchored on its buffer
base, offsets, and checksum pattern.

Cross-region leads:
- The EEPROM library leans on r0c_app checksum helpers 0x000F71D0 (sum), 0x000F71EC
  (adjust old→new), 0x000F7214 (verify) — the unit owning r0c_app 0xF71xx should treat those
  three as shared infrastructure, not unit-local helpers.
- Cluster members living outside r08_app: 0x001198DC and 0x0012E4BA (r10 copies of
  monstate12b_alloc_zero), 0x000C00A2 and 0x000C0852 (r0c_app copies of the record-clear
  wrappers) — those units will see duplicate patterns; names were kept family-consistent.
- Callers into this unit from other regions to cross-check: 0x000CA176 (reset orchestrator),
  0x000A3E60/0xA13xx–0xA2Cxx (DID read/write handlers), 0x000D22BE/0x000D229E, 0x000E3836,
  0x000B767E, and the 0xB3xxx–0xB5xxx adaptation-learn modules.
- The record offsets documented here (buffer bases, +0xBA..+0xD4C fields, mirrors) feed the
  project's EEPROM block map (1034420267_eeprom_records.csv, spec README) — good input for
  naming the remaining record fields.
