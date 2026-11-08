# Unit 16 (r08_app)

Region 0x000A0D8A–0x000A46C4. This slice is the **fault-memory / DTC subsystem's exported API
layer plus its tail end of internals** (spec 13: event table, reporting API, fault-group
identification). Pattern: thin wrappers load a context pointer or method pointer from a flash
pointer table at 0xA0EF8–0xA1238 and jump into the fault engine at 0x0009Bxxx–0x000AExxx.
Status-word semantics used below: bit0 testFailed, bit2 pending, bit3 confirmed, bit4
testNotCompletedSinceLastClear, bit15 0x8000 = invalid marker (matches 0x000A03D6 default).
Massive caller sets from 0x0013xxxx–0x0015xxxx (r10/r14 monitor code) confirm this is the
public fault-reporting API. Spec cross-refs use `docs/spec/13-fault-reporting.md`.

#### 0x000A0D8A `dtc_flag1417a0_get` — stub
Thunk: double-indirect call through table word 0x000A0F34 into 0x001417A0, a 3-instr getter
returning `*PTR_DAT_001418b8` (byte flag in the r14-resident object area). Sole caller
0x0009F06E (fault engine). [O]

#### 0x000A0D98 `dtc_flag1417b0_get` — stub
Same thunk pattern via 0x000A0F38 into 0x001417B0 (`return *PTR_DAT_001418c0`). Sole caller
0x0009DBF8. [O]

#### 0x000A0DA6 `dtc_obj0f3c_active_get` — stub
Calls function pointer 0x000A0F40 with context PTR_DAT_000A0F3C; target 0x000AE8FE is
`return *(char*)(ctx+1) == 1` — an enabled/active check on the object at 0xA0F3C. Caller
0x0009D158 (fault engine). [O]

#### 0x000A0DB4 `diag_did_req_0f50` — logic
DID-request path on the diagnostic object at 0xA0F50: masks both params to u16, first runs a
guard fn-ptr (0xA0F44), then a lookup fn-ptr (0xA0F48 → 0x000AA6C4, key/id → index+record), and
if found invokes a handler ptr (0xA0F4C) with the record; finally always calls
DAT_000A0F54(0xA0F50, id, id2) → 0x000AD606. Callers 0x000A0074, 0x0009ED10, 0x0009D090,
0x000A0BD6 — diagnostic session side. [I]

#### 0x000A0DF0 `diag_did_call_0f50` — stub
Delegates to DAT_000A0F58(0xA0F50, did, p3, p4) → 0x000AD8DA, which switches on DID 0x269/
0x26A/0x26B, 0x2E2, 0x210 — fault-memory DID handlers. Sole caller 0x00144108. [O]

#### 0x000A0E04 `diag_did_0f50_init` — stub
Calls method ptr 0xA0F5C on context 0xA0F50 → 0x000AD69A (29-instr scrub/init over the DID
data). Cluster of 2: member 0x000A0E0C `diag_did_0f50_init__2` uses slot 0xA0F60 → 0x000AD85C
(version-gated init, calls 0x00138C36 + 0x000AD8A8). Both called from 0x0014870A. [I]

#### 0x000A0E14 `dtc_word_0f64_get` — data-accessor
Returns `*(short *)*0x000A0F64` — one-word getter behind a pointer; used by fault engine
0x000AAE6C and 0x000ABDFA. [O]

#### 0x000A0E1A `dtc_word_0f68_get` — data-accessor
Returns `*0x000A0F68` (int) — plain pointer-read getter; callers 0x0009BE50-adjacent engine
functions 0x0009B918/0x0009BC38 etc. [O]

#### 0x000A0E20 `dtc_init_pair_call` — logic
Pointer-invoked (callers=-) two-step init: calls a fn-ptr on context 0x000A0EF8 (→ 0x000ADE40
family: reads a double-ptr value, stores into ctx and 0x000ADF80, runs 0x000ADEB8/0x000ADEC2),
then an init chain on 0xA0F70: rep → 0x000A1170 (8 sequential method calls on contexts
0xA11C8/0xA11C0/0xA1214/0xA11E8/0xA1224/0xA11F4/0xA1230/0xA11F0), members 0x000A0E2E/0x000A0E44
→ 0x000A123C, member 0x000A0E52 → 0x000A1296. Cluster of 4. [I]

#### 0x000A0E70 `dtc_report_event` — library
Public fault-event report wrapper: shuffles 4 args into DAT_000A0F8C → 0x0009BE50, the event
engine (validates index 1..0xC3, sets/clears bit 0x800 in the u16 status table behind
PTR_DAT_0009bebc, honours mode marker 'U'). 52 resolved callers across r08/r0c/r10/r14 — the
monitors' main entry point (spec 13 §reporting API). [O]

#### 0x000A0E84 `dtc_call_9fa5e_10` — library
Fixed-arg wrapper: DAT_000A0F90(1,0) → 0x0009FA5E, a 351-instr fault-engine step. Sole caller
0x001554B4. [O]

#### 0x000A0E94 `dtc_call_9b918` — library
Wrapper via slot 0x000A0F94 → 0x0009B918 (336-instr event update engine). Caller 0x0013661C. [O]

#### 0x000A0EA4 `dtc_call_9bc38` — library
Wrapper via slot 0x000A0F98 → 0x0009BC38 (257-instr event engine, single-arg variant). No
resolved callers — pointer-invoked. [O]

#### 0x000A0EB2 `dtc_is_failed_or_untested` — logic
Status predicate on event id: `st = 0x000A03D6(id)` (u16 from table 0x000A0448, index 1..0xC3,
0x8000 default when the marker byte at 0xA0444 is not 0xAA/0xA5/0x55); returns
`(st & 0x8000)==0 && (st & 0x8010)!=0` — valid and (testFailed | testNotCompletedSinceLastClear).
Cluster of 4 with *meaning-changing* mask constants; members named separately:
0x000A0ED6 `dtc_is_pending_or_untested` (mask 0x8014 = pending|notCompleted),
0x000A0FA0 `dtc_is_confirmed` (mask 0x8008 = confirmedDTC), 0x000A0FC4
`dtc_is_pending_or_confirmed` (mask 0x800C = pending|confirmed). Bit mapping inferred from UDS
DTC status byte layout (spec 13). [I]

#### 0x000A0FE8 `dtc_group_field2_lookup` — logic
If gate (slot 0xA11A4 result) has bit 3: looks id up via 0x000A11A8 → index, then reads the u16
at `index*0x38 + base(0xA11AC) + 2` — a field of a 0x38-byte fault-group record (fault-group
identification, spec 13; cf. 1034420267_fault_groups.csv). Returns found flag; writes id match
guard (0xFFFF) else. Callers 0x00141E06, 0x0007BBE0. [I]

#### 0x000A102C `dtc_status_flags_arg1` — stub
Calls slot 0xA11B0 → 0x0009F718(1). 0x0009F718 summarizes state words at 0x9F8E0..0x9F8EC
depending on the marker byte ('Z' vs 0xA5). 10 callers in r10/r14 monitor code. [O]

#### 0x000A1032 `dtc_any_flag_or` — logic
Returns `0x0009F718(0) != 0 || 0x0009F718(1) != 0` — OR of the two state summaries; caller
0x000EC20A. [O]

#### 0x000A104E `dtc_call_a044c` — library
Wrapper via slot 0xA11B4 → 0x000A044C(1, p1), the 370-instr fault-engine step also used by the
init chain (0x000A123C). Callers 0x0013A102, 0x00139B58. [O]

#### 0x000A105E `dtc_update_event` — logic
Selector: if p3==1 calls 0x0009B918(p1,p2) via slot 0xA11B8, else 0x0009BC38(p1) via 0xA11BC —
two-arg vs one-arg event-update engines. 58 callers across r08–r14. [O]

#### 0x000A107C `dtc_call_acea4` — library
Calls slot 0xA11C4(0xA11C0, p1, p3) → 0x000ACEA4, a 144-instr routine that reads event status
(0x000A03D6) and fault-engine internals (0x000A0A14, 0x000AA750, 0x000ACBE4, ...). Always
returns 0. 32 callers. [O]

#### 0x000A108E `dtc_record_next_byte` — stub
Calls DAT_000A11CC(0xA11C8) → 0x000ACA36: increments `*ctx` counter, appends a byte from
0x000ACB2E into ctx+1 — record/snapshot append helper. Caller 0x00139B9C. [O]

#### 0x000A109C `dtc_call_aca4a` — stub
Calls slot 0xA11D0(0xA11C8) → 0x000ACA4A, query against the same record object. Caller
0x00139C40. [O]

#### 0x000A10AA `dtc_ptr_delta_11d4` — data-accessor
Returns `(0xA11D4 + 4 - 0xA11D8) & 0xFFFF` — u16 delta between two pointer globals (free-space /
count calc for the record object). Caller 0x00139DF4. [O]

#### 0x000A10B6 `dtc_call_aca10` — stub
Calls DAT_000A11DC(0xA11C8, p1) → 0x000ACA10: gate on method ptr 0xACBA4 then, if p2&6,
0x000ACA68+0xACB9C. Caller 0x00139CE4. [O]

#### 0x000A10C6 `dtc_call_acac4` — stub
Calls slot 0xA11E0(0xA11C8, p1, p2) → 0x000ACAC4 (indexed u8 fetch with 0x13 bound). Cluster
of 2: member 0x000A10D8 `dtc_call_acaf8` → 0x000ACAF8 via slot 0xA11E4. Caller 0x00144108. [O]

#### 0x000A10EA `dtc_event_attr_read_wrap` — library
Delegates to 0x000A1150 with context PTR_DAT_000A11E8. Caller 0x00155534. [O]

#### 0x000A10FA `dtc_lab_11ec_get` — stub
Two-instruction constant getter: returns the data label 0x000A11EC (pointer-table tail area).
Caller 0x001357A6. [O]

#### 0x000A1104 `dtc_11f4_get` — stub
Method call slot 0xA11F8 on context 0xA11F4 → 0x000AE01C (`return *DAT_000ae0b4`, a counter
getter). Cluster of 2: member 0x000A1112 `dtc_11f4_get__2` → slot 0xA11FC → 0x000AE022.
Callers 0x00144108. [O]

#### 0x000A1120 `dtc_11f4_call_arg` — stub
Method call slot 0xA1200(0xA11F4, p1) → 0x000AE028 (44-instr search/scan over a structure,
init 0xFFFF). Cluster of 3: members 0x000A1130 → 0x000AE084 (slot 0xA1204), 0x000A1140 →
0x000AE09C (slot 0xA1208). Callers 0x00148102. [I]

#### 0x000A1150 `dtc_event_attr_read` — logic
Bounds-checks p2 < 0xC4 (event count) and returns DAT_000A120C(subtable at *ctx+0x34C, p2) —
target 0x000AE844 reads `*(short*)(base + 4 + idx*2)`: a u16 attribute array indexed by event
id. Default 0xFF00. Sole caller 0x000A10EA. [I]

#### 0x000A123C `dtc_init_engine_full` — logic
Bring-up chain for the fault engine: five method calls on contexts 0xA12B8–0xA12D8 (slots
0xA12BC–0xA12DC), then six results of fn(1,{0,1,2,3,4,0xB}) stored to DAT_000A12E4[0..5] — a
config byte block. Called by init pair 0x000A0E2E/0x000A0E44. [O]

#### 0x000A1296 `dtc_init_engine_tail` — logic
Tail of the init chain: method on 0xA12C0 (slot 0xA12E8 → 0x000AC9F2), a gated optional call
through double-pointer 0xA12EC (→ 0x000AAFA8), then method on 0xA12F4 (slot 0xA12F8 →
0x000AA560). Called by 0x000A0E52. [O]

#### 0x000A12FC `adapt_read_div2` — logic
Adaptation-family reader, index 1..5 (else reports (0x248,0,1,0x3A,idx,...) via slot 0xA151C):
gate 0xA150C==1 then reads u16 table[idx-1] via slot 0xA1514 (→ 0x000BD8FA area tables),
halves it, and converts via 0xA1520. Caller 0x000AEB4A (freeze-frame capture). [I]

#### 0x000A1362 `adapt_read_1528` — logic
Same shape, detail 0x6C, raw u16 (no /2) via slot 0xA1528 → converter 0xA152C. Caller
0x000AEB5A. [I]

#### 0x000A13CE `adapt_read_div10` — logic
Calls 0x000A15FE(idx) then converts (value/10) through 0xA1520. Caller 0x000AEB2A. [I]

#### 0x000A13E6 `adapt_read_1534` — logic
Same shape, detail 0xF4, u16 via slot 0xA1534 → 0xA152C. Caller 0x000AEB3A. [I]

#### 0x000A1454 `adapt_counters_bump` — logic
Index 1..5 else report 0x248/2/0xB2: when gate 0xA150C==1, bumps two per-index counters
(slots 0xA153C/0xA1540 and 0xA1548/0xA154C, saturation-guarded at 0xFFFF), and for index 2 a
third counter (0xA1554/0xA1558). Returns gate low byte. Caller 0x000AEAF2 — adaptation
teach-in bookkeeping. [I]

#### 0x000A155C `adapt_tbl28_read` — stub
Reads element u16 via slot 0xA17FC from the table at (*0xA17F8)+0x28, index p2-1. Callers
0x000CDBB4, 0x000CC89C (diagnostic DID handlers). [O]

#### 0x000A1572 `adapt_in_range_check` — logic
Index 1..5 + gate 0xA1804: branch on 0x000A1B7A() (index-2 special-mode gate); picks current
value (0xA180C/0xA1814) or indexed table (0xA181C/0xA1824), converts via 0xA1828 and compares
against the limit table at obj+0x44 through 0xA182C — returns 1 if within limits. Caller
0x000B61C8. [I]

#### 0x000A15FE `adapt_value_1830_read` — logic
Index 1..5 else report 0x248/3/0x93; gate 0xA1804 then u16 from table 0xA1830[idx-1] via slot
0xA1834. Heavily used (12 callers incl. 0x000B4046, 0x000A17CC). [O]

#### 0x000A1664 `adapt_1830_read_gated` — logic
Reads 0x000A15FE only if one of three gate calls on DAT_000A1840 (obj+8, obj, obj+4 with
idx-1) returns true — conditional attribute fetch. Caller 0x000AEAFA (freeze-frame). [O]

#### 0x000A16B8 `adapt_1848_read_if2` — logic
Only for index 2 (else report 0x248/3/0xDF): requires 0x000A1B7A() mode gate, then reads slot
0xA1848 on 0xA1844. Caller 0x000AEB0A. [O]

#### 0x000A1712 `adapt_value_adjust_clamp` — logic
Adaptation update with saturation: index 1..5 else report 0x248/4/0x5B; reads cur from
0xA1830[idx-1], min/max from tables (*0xA17F8)+0x24/+0x28 via slot 0xA17FC; clamps
cur+param_3 via 0xA184C, stores via 0xA1850, returns the applied delta through
0xA1854(p1, idx, clamped-cur). Callers 0x000B68D0 and 0x000A17CC. [O]

#### 0x000A17CC `adapt_value_adjust_offset` — logic
Index 1..5 else report 0x248/4/0x8B: subtracts the current 0x000A15FE value from param_3,
then delegates the clamp-and-store to 0x000A1712 — absolute-to-relative adaptation write.
Callers 0x000CB7EC, 0x000CDBB4 (diagnostic write path). [I]

#### 0x000A1886 `adapt_idx2_adjust_clamp` — logic
Index-2-only twin of 0x000A1712 (else report 0x248/4/0xC4): gate 0xA1AF4 on 0xA1AF0, cur via
0xA1AFC, min/max from (*0xA1B00)+0x24/+0x28 via 0xA1B04, clamp via 0xA1B08, store via
0xA1B0C. Caller 0x000B68D0. [O]

#### 0x000A191C `adapt_param_minus_conv` — logic
For index 1..5: converts 0x000A1A42() (per-index counter) via 0xA1B10 and limit table at
(*0xA1B00)+0x20 via 0xA1B14, then returns param_3 - converted through 0xA1B08. Callers
0x000CDBB4, 0x000B68D0. [I]

#### 0x000A195C `adapt_idx2_param_minus_conv` — logic
Index-2-only variant of 0x000A191C using counter source 0x000A1B3C instead of 0x000A1A42.
Caller 0x000B68D0. [O]

#### 0x000A1998 `adapt_counter_incr_use` — logic
Index 1..5 else report 0x248/5/0x8D: reads counter 0x000A1A42(), increments unless 0xFFFF,
stores via 0xA1B18(p1, idx, c); when p3 nonzero also calls 0xA1B1C(p1, idx, 0). Callers
0x000B4F64, 0x000B68D0 — adaptation-use bookkeeping. [I]

#### 0x000A1A08 `adapt_idx2_counter_reset` — logic
Only when index==2 and gate 0xA1AF4(0xA1AF0)==1: bumps the counter at 0xA1B20 (saturation
guarded), then clears the slot 0xA1B2C value to 0. Caller 0x000B68D0. [I]

#### 0x000A1A42 `adapt_counter_1b34_read` — logic
Index 1..5 else report (0x248,0,6,0x0C,idx): u16 counter from table 0xA1B34[idx-1] via slot
0xA1B38. Twelve callers across the adapt coordinator 0xB0xxx-0xB5xxx. [O]

#### 0x000A1A8E `adapt_state_classify` — logic
Cascade classifier returning 0..4 for an index/state pair: 0 if none match; 1 if
0x000A1E1C (state match + 0xA2A88 gate); 3 if 0x000A1D14 (gated state match); 2 if
0x000A1E64 (state==p3), downgraded to 4 when p3==3 and 0x000A2AC4 fails. Callers
0x000B4046, 0x000B18E0, 0x000B5FCC, 0x000A23F4, 0x000B1C3C. [I]

#### 0x000A1B3C `adapt_idx2_read_1dd4` — logic
Index-2-only read (else report 0x248/6/0x50): u16 via slot 0xA1DD8 on table 0xA1DD4.
Callers 0x000A195C, 0x000B5B36. [O]

#### 0x000A1B7A `adapt_idx2_mode_gate` — logic
Gate: returns 1 only when index==2 and `**(obj(0xA1DE4)+0x1C) == 1` — the index-2 special
mode flag. Seven callers (0x000A1572, 0x000A16B8, 0x000B5B36, 0x000B5FCC, 0x000A23F4,
0x000AEB1A, 0x000B68D0). [O]

#### 0x000A1B94 `adapt_below_limit_curve` — logic
Index 1..5 else report 0x248/6/0x9B: gate 0xA1DEC, value u16 from 0xA1DF0[idx-1] (slot
0xA1DF4), then a mode-indexed curve object resolved through pointer table 0xA1E04 (mode 0..4,
out-of-range reports an error and falls back to *(ptr+4)); returns curve_base(*obj+8) <=
value. Callers 0x000B1352, 0x000AFC54. [I]

#### 0x000A1C60 `adapt_below_tbl54` — logic
Index 1..5 else report 0x248/6/0xC7: value 0xA1DF0[idx-1] vs converted table read
0xA1E10(obj(0xA1DE4)+0x54, idx-1); returns 1 when table low-byte < value. Caller
0x000B1C3C. [I]

#### 0x000A1CE6 `adapt_tbl3c_read` — stub
u16 via slot 0xA1E14 from table (*0xA1DE4)+0x3C [idx-1]. Cluster of 2: member 0x000A1CFC
`adapt_tbl40_read` reads the twin table at +0x40 (constant difference). Callers 0x000CDF42,
0x000CC89C (diagnostic reads). [O]

#### 0x000A1D14 `adapt_state_matches_gate` — logic
Index 1..5: requires gate 0x000A1D58 and then state slot 0xA1E10(*0xA1DE4, idx-1) == p3.
Sole caller 0x000A1A8E. [O]

#### 0x000A1D58 `adapt_gate_cascade` — logic
Compound gate for index 1..5: obj+0x10 flag==1, 0xA1E10(obj+8, idx-1)==1, u16 from obj+0xC
(slot 0xA1E18) vs 0x000A2268 margin, !0x000A2A88, and 0xA1E10(obj+0x14, idx-1) != 0.
Sole caller 0x000A1D14. [I]

#### 0x000A1E1C `adapt_state_matches_obj0` — logic
Index 1..5: state 0xA20C0(*0xA20BC, idx-1) == p3 and 0x000A2A88(p1,p2) != 0 → 1. Caller
0x000A1A8E. [O]

#### 0x000A1E64 `adapt_state_matches_obj8` — logic
Index 1..5: state from table obj(0xA20BC)+8 [idx-1] == p3. Callers 0x000B54E4, 0x000A1A8E. [O]

#### 0x000A1E96 `adapt_state_matches_obj2c` — logic
Same as 0x000A1E64 but against table obj+0x2C. Sole caller 0x000B5086. [O]

#### 0x000A1EC8 `adapt_ready_check_1` — logic
Index 1..5 readiness: state(obj+4) must be 1 or 2; if 1, per-index limit table obj+0x18
low-byte must exceed counter 0x000A1A42(); finally state(obj+8)==1. Caller 0x000B54E4. [I]

#### 0x000A1F2C `adapt_ready_check_2` — logic
Structural twin of 0x000A1EC8 using tables obj+0x30 (state), obj+0x34 (limit), counter
0x000A220C, final gate obj+0x2C. Caller 0x000B5086. [I]

#### 0x000A1F90 `adapt_20c4_read` — stub
u16 via slot 0xA20C8 from table 0xA20C4[(p2&0xFF)-1]. Nine callers, all diagnostic handlers
(0xCB3B8-0xCDF42). [O]

#### 0x000A1FA4 `adapt_20c4_read_gated` — logic
Only when state(obj+0x2C)==1 or state(obj+0x30)==1: index 1..5 (else 0x248/8/7) then the
0xA20C4 table read. Caller 0x000AEAE2 (freeze-frame). [O]

#### 0x000A2000 `adapt_report_248_g8` — stub
Unconditional error report (0x248, param_3, 8, 7) via slot 0xA20D0(0xA20CC), then returns an
uninitialized stack short — a deliberate error-path stub. No resolved callers (pointer/table
invoked). [O]

#### 0x000A2028 `adapt_20c4_write_clamp` — logic
Index 1..5 else report 0x248/8/0x81: gate slot 0xA20D8(0xA20D4,5); clamps p3 between
min/max tables obj(0xA20BC)+0x3C/+0x40 (slot 0xA20DC) via 0xA20E0, writes through
0xA20E4(0xA20C4, idx-1, clamped). Callers 0x000CDF42, 0x000CBD0E, 0x000A20E8. [O]

#### 0x000A20A2 `adapt_report_248` — stub
Minimal pointer-invoked error reporter: calls slot 0xA20D0(0xA20CC, 0x248) and returns.
[O]

#### 0x000A20E8 `adapt_reset_20c4` — logic
Resets one adaptation slot: first 0x000A2028(p1,p2,0) (clamped write of 0), then index 1..5
else report 0x248/8/0xB2; gate 0xA2368(0xA2364); zeroes counter via 0xA2370(0xA236C, idx-1,
0). Callers 0x000CF700, 0x000CFAEC (diagnostic reset). [I]

#### 0x000A2146 `adapt_param_minus_conv2` — logic
Twin of 0x000A191C on the second object set: converts counter 0x000A220C via 0xA2380 and
limit table (*0xA237C)+0x38 via 0xA2384, returns p3 - converted. Caller 0x000CDF42. [I]

#### 0x000A2186 `adapt_counter_incr_reset2` — logic
Twin of 0x000A1998: index 1..5 else report 0x248/9/0x20; gate 0xA2368; increments counter
0xA238C(0xA236C) (saturation-guarded) via 0xA2370 and clears the paired slot through
DAT_000A2394(0xA2390, idx-1, 0). Caller 0x000B4F64. [I]

#### 0x000A220C `adapt_counter_236c_read` — logic
Index 1..5 else report 0x248/9/0x4B: u16 counter from 0xA236C[idx-1] via slot 0xA238C.
Eight callers (incl. 0x000B4046, 0x000B4F64, 0x000CF700). [O]

#### 0x000A2268 `adapt_margin_calc` — logic
Index 1..5 else report 0x248/9/0xAE: returns u16 from 0xA2398[idx-1] (slot 0xA239C) minus
0x000A2B76(p1,p2) — remaining-margin calculation. Callers 0x000A22DA, 0x000A1D58. [I]

#### 0x000A22DA `adapt_margin_positive` — logic
Index 1..5: returns 1 when 0x000A2268() > -1 (margin not exhausted). Caller 0x000B54E4. [I]

#### 0x000A22FA `adapt_below_limit_curve2` — logic
Index 1..5: reads value 0xA23A0(0xA2390), counter 0xA220C(idx), converts via 0xA23A4, resolves
a mode-indexed curve object through pointer table 0xA23B0 (mode 0..4, else error-report
fallback), compares 0xA2548(curve, conv) <= value. Caller 0x000B5086. [I]

#### 0x000A23F4 `adapt_arbiter_pick_best` — logic
427-instr arbiter over three candidate indices (p2/p3/p4, each <=5 else 0x248/10/{0xED,0xF2,
0xF8}): classifies each via 0x000A1A8E(...,3), keeps candidates (0xFFFF) unless invalid, zeroes
the candidate matching special index 2 when the 0x000A1B7A mode gate is on, applies a global
window check (tables obj+0x44/+0x48 vs margin), validates each candidate through
0xA2788/0xA278C, subtracts per-index offsets (converter 0xA2790 + table 0xA2794), then selects
the winner in preference order 2 > p4 > p3 > p2, records the winner's class in 0xA279C/0xA2A28
and reports 0x248/0xB/0xDC if the winner was not one of the inputs. Callers 0x000B61C8,
0x000B5A46 — candidate-selection core of the adapt coordinator. [I]

#### 0x000A280A `adapt_pair_lookup_read` — logic
Under gate `**(obj 0xA2A34+0x4C)==1` and two nonzero params: maps the pair (p2,p3) through
0xA2A3C(0xA2A38) (else report 0x248/2/0xC/0x39), then for p4==1/2 reads u16 via the +0x14
vtable method of object 0xA2A44/0xA2A40 at mapped-1. Caller 0x000B0E44. [I]

#### 0x000A28A8 `adapt_record_apply_pair` — logic
For a record (param_1: count short at +2, index byte at +4) matching p2 in {1,3}: reads a byte
pair via 0xA2A48, maps it via 0xA2A3C (else 0x248/{2,3}/0xC/{0xBD,0xC4}), then for two phases
reads the phase u16 via 0xA2A4C(p3,p4,phase), reads current via the +0x14 vtable method of
0xA2A44/0xA2A40, computes adjusted = 0x000A2B48(record, value, current) and writes it back via
the +0x20 vtable method. Caller 0x000AEB6A (fault freeze-frame path). [I]

#### 0x000A29D4 `adapt_value_store_notify` — logic
When gate obj+0x4C and 0xA2A54(0xA2A50) are set: writes param_3 into record words [0] and [1],
sets record[4]=1, calls 0xA2A5C(0xA2A58), sets record[4]=3, calls 0xA2A5C(0xA2A58, idx), then
clears record[2] — value store with two-phase notification. Called by the clamp-store
0x000A1712. [I]

#### 0x000A2A60 `adapt_seq_2a60` — logic
Pointer-reachable (via 0x000AEE8A ← 0x000BAEB4) sequence of six method calls on contexts
0xA2CD4–0xA2CFC into the 0xAF-0xB5 coordinator area — adapt-subsystem start/phase sequence.
[I]

#### 0x000A2A88 `adapt_below_limit_14` — logic
Index 1..5: returns 1 when raw value 0xA2D04() < low byte of table obj(0xA2D08)+0x14[idx-1].
Callers 0x000A1E1C, 0x000A1D58. [O]

#### 0x000A2AC4 `adapt_window_check` — logic
Index 1..5 with gate 0xA2D14(0xA2D10,5): value 0xA2D1C(0xA2D18) converted by 0xA2D28, window
edges from tables obj(0xA2D08)+0x44/+0x48, margin 0xA2D24(0xA2D20, idx-1); returns 1 when
(max-min) <= margin. Caller 0x000A1A8E. [I]

#### 0x000A2B48 `adapt_scale_by_count` — logic
Returns converted(p3 + (table(obj 0xA2D08+0x50)[p2] * count(param_1+2)) / 100) via 0xA2D30 —
percentage adjustment of a value by a record count. Sole caller 0x000A28A8. [O]

#### 0x000A2B76 `adapt_curve_value_read` — logic
Index 1..5 else report 0x248/9/0x7F: value 0xA2D1C(0xA2D18, idx-1), converted by 0xA2D28, then
a mode-indexed curve object (pointer table 0xA2D3C, mode 0..4 else error fallback) yields
curve(+4) via 0xA2D2C. Caller 0x000A2268. [I]

#### 0x000A2C3A `adapt_margin_store` — logic
Index 1..5 else report 0x248/5/0xE4: writes 0xA2D54(0xA2D20, idx-1, val) — the margin table
read by 0x000A2AC4. Caller 0x000A1998. [O]

#### 0x000A2C88 `adapt_value_store` — logic
Index 1..5 else report 0x248/6/0x2F: writes 0xA2D58(0xA2D18, idx-1, val) — the value table
read by 0x000A2B76/0x000A2AC4. Caller 0x000A1998. [O]

#### 0x000A2D5E `seq_2d5e` — unknown
Pointer-invoked sequence of 7 method calls on table objects 0xA2F90–0xA2FB4, including thunks
to 0x000F70B4/0x000F471C, into the 0xB7–0xBB module area. No resolved callers; purpose
unresolved from this unit. [I]

#### 0x000A2D88 `seq_2d88` — unknown
Pointer-invoked sequence of 7 method calls on 0xA2F90/0xA2FA0/0xA2FCC/0xA2FD4 into the
0xB8–0xBB area. No resolved callers; purpose unresolved. [I]

#### 0x000A2DCA `seq_2dca` — unknown
Pointer-invoked sequence of 5 method calls on 0xA2FE8/0xA2FF4 plus thunks to 0x000E93C0,
0x000A360A, 0x000A34D8. No resolved callers; purpose unresolved. [I]

#### 0x000A2E20 `seq_2e20` — unknown
Pointer-invoked pair of method calls on 0xA2FE8 and 0xA2FA0 (→ 0x000B7E5E, 0x000BB194). No
resolved callers; purpose unresolved. [I]

#### 0x000A2E74 `seq_2e74` — unknown
Pointer-invoked sequence of 3 method calls on 0xA3008/0xA2FD4 (→ 0x000BAC16, 0x000B85C8).
No resolved callers; purpose unresolved. [I]

#### 0x000A2E8A `seq_2e8a` — unknown
Pointer-invoked sequence: two shared method calls (0xA2FFC, 0xA3000) plus DAT_000A3054 on
0xA3010. No resolved callers; purpose unresolved. [I]

#### 0x000A2EBC `seq_2ebc` — unknown
Pointer-invoked sequence of 6 method calls (0xA3054 on 0xA3010, then the 0xA2F98/0xA2F9C/
0xA2FA4 chain and the 0x000F70B4/0x000F471C thunks). No resolved callers; purpose unresolved.
[I]

#### 0x000A2EE0 `seq_2ee0` — unknown
Pointer-invoked sequence of 4 method calls on 0xA2FE0/0xA2FA0/0xA2FB4/0xA2FD4 including a
thunk to 0x000E566C. No resolved callers; purpose unresolved. [I]

#### 0x000A2EFC `seq_2efc` — unknown
Pointer-invoked pair of method calls on 0xA2F90 (0xA2FC0) and 0xA2FE8 (0xA3064). No resolved
callers; purpose unresolved. [I]

#### 0x000A2F0C `seq_2f0c` — unknown
Pointer-invoked sequence: thunk to 0x000E93C8 on 0xA2FF4, then 0xA3024/0xA3028 on 0xA3020
(second call passes 0). No resolved callers; purpose unresolved. [I]

#### 0x000A2F34 `seq_2f34` — unknown
Pointer-invoked sequence of 4 method calls on 0xA3008/0xA2FCC/0xA2FD4 (→ 0x000BAC0E,
0x000BAC1E, 0x000B85D0, 0x000BA238). No resolved callers; purpose unresolved. [I]

#### 0x000A2F50 `seq_2f50` — unknown
Pointer-invoked pair of method calls on 0xA3008 (0xA3070, 0xA3074). No resolved callers;
purpose unresolved. [I]

#### 0x000A2F60 `seq_2f60` — unknown
Pointer-invoked pair of method calls: 0xA3078 on 0xA3008 and 0xA307C on 0xA3010 with constant
0x30. No resolved callers; purpose unresolved. [I]

#### 0x000A2F72 `seq_2f72` — unknown
Pointer-invoked sequence of 4 method calls: 0xA3080/0xA3084 on 0xA3008, 0xA3088 on 0xA2FD4,
and 0xA307C(0xA3010, 0x31). Twin of 0x000A2F60 (0x30 vs 0x31). Purpose unresolved. [I]

#### 0x000A308C `seq_308c` — unknown
Pointer-invoked sequence of 3 method calls on 0xA3200/0xA3208/0xA3210 (→ 0x000B7E8E,
0x000BA240, 0x000BA406). Purpose unresolved. [I]

#### 0x000A30B2 `seq_30b2` — unknown
Pointer-invoked sequence: 0xA3220 on 0xA3210, 0xA3228(0xA3224,0), 0xA3230(0xA322C,0x32).
Purpose unresolved. [I]

#### 0x000A30CC `seq_30cc` — unknown
Pointer-invoked sequence of 3 method calls on 0xA3234/0xA3240 (→ 0x000AEE7A, 0x000AEEAA,
0x000BA54C). Purpose unresolved. [I]

#### 0x000A31A4 `seq_31a4` — unknown
Pointer-invoked pair: 0xA32B8 on 0xA3208, then DAT_000A32BC on 0xA3240. Purpose unresolved.
[I]

#### 0x000A31D0 `seq_31d0` — unknown
Pointer-invoked sequence of 3 method calls on 0xA3284/0xA32CC/0xA3208 (→ 0x000BA65A,
0x000B7E66, 0x000BB17C). Purpose unresolved. [I]

#### 0x000A31E6 `seq_31e6` — unknown
Pointer-invoked pair of method calls on 0xA3224 (0xA32D8, 0xA32DC → 0x000B858E/0x000B8596).
Purpose unresolved. [I]

#### 0x000A32E4 `limiter_coord_full` — logic
240-instr output-limit coordinator (no resolved callers — task-invoked): refreshes
DAT_fff8e506/0507, sets record word [3]=6, computes a desired short via slot 0xA3554 with
flags obj0xA3550[0xCA..0xCD]==2, clamps it into record+10 between record+0xC/+0xE (reason code
7 in DAT_fff8e504), then applies successive limiters — 0xA355C/10 (code 8), 0xA3564 (code 9),
0xA3568 (code from 0xA356C), gated 0xA3578 (code 0x10) — takes max of two sources
(0xA3580/0xA3584) into DAT_fff8d5e2, fills DAT_fff8e50E, zeroes counters, maps a state <9
through table 0xA35A0 into DAT_fff8e50D (else report (0x77,0xF,0,2,0x11,0x14)), then
snapshots via 0x000A3646 and closes with 0xA35A8. [I]

#### 0x000A34D8 `limiter_coord_short` — logic
Shorter twin of 0x000A32E4 called by seq_2dca/seq_2e8a: same 0xA3554 desired value with the
[0xCA..0xCD] flags, clamp into record *0xA35AC between +2/+4 (code 7), limiter 0xA37F4 (code
9), then 0x000A3646 snapshot, DAT_fff8e50E from 0xA37FC, byte to *0xA3804, final method
0xA380C on 0xA3808. [I]

#### 0x000A360A `limiter_filters_step` — logic
Steps two filters/integrators: DAT_fff8e509 = fn(0xA3814, old), DAT_fff8e508 = fn(0xA3818,
old), and the two u16s at *0xA381C via 0xA3820/0xA3824 — rate-limited state updates. Called
by seq_2dca/seq_2e8a. [I]

#### 0x000A3646 `limiter_snapshot_fill` — logic
Fills the 0x3E-byte record at *0xA3828 with indexed u16s from table object 0xA382C via two
method pointers (0xA3830 indices 0..0xB and 0xA3838 indices 0..0xB into interleaved offsets),
plus DAT_fff8e50A-050C via 0xA3834 — periodic snapshot of the subsystem's parameter set.
Callers 0x000A32E4, 0x000A34D8. [O]

#### 0x000A3776 `lim_state_383c_get` — data-accessor
Returns (char)*PTR_DAT_000A383C — state-byte getter used by seven callers (0x000F7298,
0x0013498E, 0x001534FC monitors). [O]

#### 0x000A377C `lim_state_3840_get` — data-accessor
Returns *DAT_000A3840 (word) — getter used by 0x00144108, 0x0013498E, 0x001534FC, 0x000F4E54,
0x000A81D8. [O]

#### 0x000A3782 `lim_state_383c_not` — data-accessor
Returns ~*PTR_DAT_000A383C — inverted state byte. Sole caller 0x000F4E54. [O]

#### 0x000A378C `lim_state_3844_get` — data-accessor
Returns (char)*PTR_DAT_000A3844. Sole caller 0x000F7298. [O]

#### 0x000A3792 `lim_state_3848_get` — data-accessor
Returns (char)*PTR_DAT_000A3848. Sole caller 0x000F4E54. [O]

#### 0x000A3798 `lim_flag_384c_get` — data-accessor
Calls slot 0xA3850 on context 0xA384C → 0x000BA2F2, returning a byte flag. Cluster of 3:
0x000A37A6 `lim_flag_384c_get__2` (slot 0xA3854 → 0x000BA2BA), 0x000A37B4
`lim_flag_384c_get__3` (slot 0xA3858 → 0x000BA308). Callers incl. 0x00144108, 0x000F7298,
0x001534FC. [O]

#### 0x000A37C2 `lim_word_385c_get` — data-accessor
Returns *(short*)PTR_DAT_000A385C. Caller 0x000F7298. [O]

#### 0x000A37C8 `lim_word_3860_get` — data-accessor
Returns *DAT_000A3860 (word). Callers 0x001534FC, 0x001352E4, 0x000F4E54. [O]

#### 0x000A37CE `lim_state_3864_get` — data-accessor
Returns (char)*PTR_DAT_000A3864. Callers 0x001534FC, 0x000F4E54. [O]

#### 0x000A37D4 `lim_byte_3868_get` — data-accessor
Returns *DAT_000A3868. Sole caller 0x001534FC. [O]

#### 0x000A37DC `lim_val_3810_read` — data-accessor
Calls slot 0xA386C on context 0xA3810 (the filter object from 0x000A360A) → short. Caller
0x000F7298. [O]

#### 0x000A37EA `lim_state_3870_get` — data-accessor
Returns (char)*PTR_DAT_000A3870. Sole caller 0x000F7298. [O]

#### 0x000A3874 `lim_val_3a14_read` — data-accessor
Calls DAT_000A3A18 on context 0xA3A14 → short. Caller 0x00144108. [O]

#### 0x000A3882 `lim_flag_3a14_get` — data-accessor
Calls slot 0xA3A1C on context 0xA3A14 → byte. Cluster of 2: member 0x000A3890
`lim_flag_3a14_get__2` via slot 0xA3A20 (→ 0x000BA38E). Caller 0x001534FC. [O]

#### 0x000A389E `lim_byte_e505_get` — data-accessor
Returns DAT_fff8e505 (the byte staged by limiter-coordinator 0x000A32E4 slot 0xA3530). Six
callers in r10/r14 monitor code. [O]

#### 0x000A38A6 `lim_val_3a14_read2` — data-accessor
Calls DAT_000A3A24 on context 0xA3A14 → u16. Callers 0x000F7298, 0x000A7C9C, 0x001352B4. [O]

#### 0x000A38B4 `lim_flag_3a14_get3` — data-accessor
Calls DAT_000A3A28 on context 0xA3A14 → byte. Caller 0x000A7C9C. [O]

#### 0x000A38C2 … 0x000A3940 `lim_slot0_get` … `lim_slot21_get` — data-accessor
Run of 22 clusters (0x000A38C2, C8, CE, D4, DA, E0, E6, EC, F2, F8, FE, 0x000A3904, 0A, 10,
16, 1C, 22, 28, 2E, 34, 3A, 40), each a 3-instr getter `*(short*)PTR_DAT_000a3aNN` with
sequential pointer slots 0x3A2C→0x3A80 (slot index 0..21 below in names). All consumed solely
by packer 0x000BC0F4 (619 instr, called from 0x000BB870), which reads them in pairs and emits
values with constant id 0x5EB — the limiter subsystem's value export table. Mechanics [O],
consumer semantics [I].

#### 0x000A38C8 `lim_slot1_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A38CE `lim_slot2_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A38D4 `lim_slot3_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A38DA `lim_slot4_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A38E0 `lim_slot5_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A38E6 `lim_slot6_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A38EC `lim_slot7_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A38F2 `lim_slot8_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A38F8 `lim_slot9_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A38FE `lim_slot10_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A3904 `lim_slot11_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A390A `lim_slot12_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A3910 `lim_slot13_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A3916 `lim_slot14_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A391C `lim_slot15_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A3922 `lim_slot16_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A3928 `lim_slot17_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A392E `lim_slot18_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A3934 `lim_slot19_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A393A `lim_slot20_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A3940 `lim_slot21_get` — data-accessor
See the 0x000A38C2 run entry above. [O]

#### 0x000A3946 `lim_slot22_get` — data-accessor
3-instr getter `*(short*)PTR_DAT_000a3a84` — continuation of the packer-0xBC0F4 export run
(see 0x000A38C2). [O]

#### 0x000A394C `lim_slot23_get` — data-accessor
3-instr getter `*(short*)PTR_DAT_000a3a88` — continuation of the packer-0xBC0F4 export run.
[O]

#### 0x000A3952 `lim_val_3a90_read` — data-accessor
Calls slot 0xA3A94 on context 0xA3A90 → short. Caller 0x001534FC. [O]

#### 0x000A3960 `lim_flag_3a8c_get` — data-accessor
Calls slot 0xA3A98 on DAT_000a3a8c → byte. Caller 0x000A7C9C. [O]

#### 0x000A396E `lim_word_e508_get` — data-accessor
Returns DAT_fff8e508 — the filter/integrator value stepped by 0x000A360A. Caller 0x001534FC.
[O]

#### 0x000A3976 `lim_word_3a9c_get` — data-accessor
Returns *(short*)PTR_DAT_000a3a9c. Callers 0x000F7298, 0x001534FC, 0x0014FC5C, 0x000F4E54. [O]

#### 0x000A397C `lim_word_3a9c_not` — data-accessor
Returns ~*(ushort*)PTR_DAT_000a3a9c — inverted twin of 0x000A3976. Caller 0x000F4E54. [O]

#### 0x000A3986 `lim_word_d5e2_get` — data-accessor
Returns DAT_fff8d5e2 — the max-of-two value computed in limiter coordinator 0x000A32E4.
Callers 0x00144108, 0x00133476, 0x001534FC, 0x0014FC5C. [O]

#### 0x000A398E `lim_flag_3aa0_get` — data-accessor
Calls DAT_000A3AA4 on context 0xA3AA0 → byte. Callers 0x000A7C9C, 0x0013B74E. [O]

#### 0x000A399C `lim_word_3aa8_get` — data-accessor
Returns *(short*)PTR_DAT_000a3aa8. Callers 0x001534FC, 0x000F4E54. [O]

#### 0x000A39A2 `lim_state_3aac_get` — data-accessor
Returns (char)*PTR_DAT_000a3aac. Callers 0x001534FC, 0x000F4E54. [O]

#### 0x000A39A8 `lim_word_3ab0_get` — data-accessor
Returns *(short*)PTR_DAT_000a3ab0. Caller 0x001534FC. [O]

#### 0x000A39AE `lim_word_3ab4_get` — data-accessor
Returns *(short*)PTR_DAT_000a3ab4. Caller 0x001534FC. [O]

#### 0x000A39B4 `lim_state_3ab8_get` — data-accessor
Returns (char)*PTR_DAT_000a3ab8. Caller 0x001534FC. [O]

#### 0x000A39BA `lim_word_3abc_get` — data-accessor
Returns *DAT_000a3abc (word). Caller 0x001534FC. [O]

#### 0x000A39C0 `lim_val_3ac0_read` — data-accessor
Calls slot 0xA3AC4 on context 0xA3AC0 → u16. Caller 0x000F7298. [O]

#### 0x000A39CE `lim_flag_3ac8_get` — data-accessor
Calls slot 0xA3ACC on context 0xA3AC8 → byte. Cluster of 5: members 0x000A39DC/0x000A39EA/
0x000A39F8/0x000A3A06 use slots 0xA3AD0/0xA3AD4/0xA3AD8/0xA3ADC (`__2`..`__5`). Callers
0x00151C3E etc. [O]

#### 0x000A3AE0 `lim_flag_3c34_get` — data-accessor
Calls DAT_000A3C38 on context 0xA3C34 → byte. Caller 0x00144108. [O]

#### 0x000A3AEE `lim_byte_e50e_get` — data-accessor
Returns DAT_fff8e50e (staged by both limiter coordinators). Sole caller 0x000A3AF6. [O]

#### 0x000A3AF6 `lim_byte_e50e_wrap` — stub
Tail-wrapper: return 0x000A3AEE(). Cluster of 4 duplicated wrappers: 0x000A65F6 →
0x000A6602, 0x000A6DF4 → 0x000A6D6E, and cross-region copy 0x00132334 (r10_app) → 0x00132382
— same pattern stamped per module. Callers 0x000F7298, 0x000A7C9C, 0x001534FC, 0x00155AE4,
0x000A66A8, 0x000A6D12, 0x0007ACF2. [O]

#### 0x000A3B02 `lim_word_3c3c_get` — data-accessor
Returns *DAT_000a3c3c (word). Caller 0x00155AE4. [O]

#### 0x000A3B08 `lim_word_3c40_get` — data-accessor
Returns *(short*)PTR_DAT_000a3c40. Caller 0x001534FC. [O]

#### 0x000A3B0E `lim_word_3c44_get` — data-accessor
Returns *DAT_000a3c44 (word). Caller 0x001534FC. [O]

#### 0x000A3B14 `lim_flag_3c48_get` — data-accessor
Calls slot 0xA3C4C on context 0xA3C48 → byte. Caller 0x001534FC. [O]

#### 0x000A3B22 `dtc_aeb2a_dispatch` — stub
Calls slot 0xA3C54(ctx 0xA3C50, p1) → resolved callee 0x000AEB2A — the freeze-frame
dispatcher (whose siblings 0xAEB3A/0xAEB4A/0xAEB5A wrap the adapt readers). Cluster of 2:
member 0x000A3B42 `dtc_aeb4a_dispatch` → 0x000AEB4A. Caller 0x00144108. [I]

#### 0x000A3B32 `dtc_aeb3a_dispatch` — stub
Calls slot 0xA3C58(ctx 0xA3C50, p1) → freeze-frame dispatcher 0x000AEB3A. Callers 0x00144108,
0x00148248. [I]

#### 0x000A3B62 `elemstm_call_b81c0` — stub
Calls slot 0xA3C68(ctx 0xA3C64, p1) → 0x000B81C0. Callers 0x001534FC, 0x000F7266, 0x0014FC5C.
[O]

#### 0x000A3B72 `elemstm_call_b81c0__2` — stub
Identical wrapper to 0x000A3B62 (separate cluster, different callers: 0x000B986C, 0x000B5DA6).
[O]

#### 0x000A3B82 `lim_byte_3c6c_get` — data-accessor
Returns *PTR_DAT_000a3c6c. Caller 0x000F4E54. [O]

#### 0x000A3B8A `lim_byte_3c6c_not` — data-accessor
Returns ~*PTR_DAT_000a3c6c. Caller 0x000F4E54. [O]

#### 0x000A3B94 `dtc_call_aea6a` — stub
Calls slot 0xA3C70 on ctx 0xA3C50 → 0x000AEA6A (fault-engine query). Callers 0x00144108,
0x00133476, 0x0014FC5C. [O]

#### 0x000A3BA2 `dtc_call_aea72` — stub
Calls slot 0xA3C74 on ctx 0xA3C50 → 0x000AEA72. Callers 0x00144108, 0x00133476, 0x000A7C9C,
0x0014FC5C. [O]

#### 0x000A3BB0 `dtc_call_aea88` — stub
Calls slot 0xA3C78 on ctx 0xA3C50 → 0x000AEA88. Callers 0x00144108, 0x00133476, 0x0014FC5C.
[O]

#### 0x000A3BBE `dtc_call_aea7a` — stub
Calls slot 0xA3C7C on ctx 0xA3C50 → 0x000AEA7A (u16). Cluster of 2: member 0x000A3BF6
`dtc_call_aee54` → slot 0xA3C8C → 0x000AEE54. Callers 0x00144108, 0x00148248 / 0x00133476,
0x0014FC5C. [O]

#### 0x000A3C04 `elemstm_c90_call1` — stub
Calls PTR_LAB_000a3c94(ctx 0xA3C90, p1). Caller 0x00144108. [O]

#### 0x000A3C0E `elemstm_c90_call2` — stub
Calls slot 0xA3C98(ctx 0xA3C90, p1) → 0x000BA5EC. Caller 0x00144108. [O]

#### 0x000A3C1E `elemstm_c90_call3` — stub
Calls slot 0xA3C9C on ctx 0xA3C90 → 0x000BA5DC. Caller 0x0014870A. [O]

#### 0x000A3C26 `elemstm_c90_call4` — stub
Calls slot 0xA3CA0 on ctx 0xA3C90 → 0x000BA5A2. Caller 0x00139B9C. [O]

#### 0x000A3CA4 `elemstm_d50_call1` — stub
Calls slot 0xA3D54(ctx 0xA3D50, p1) → 0x000BA5B0. Caller 0x00139CE4. [O]

#### 0x000A3CB4 `elemstm_d50_call2` — stub
Calls slot 0xA3D58 on ctx 0xA3D50 → 0x000BA5C0. Caller 0x00139C40. [O]

#### 0x000A3CC2 `elemstm_d50_call3` — stub
Calls slot 0xA3D5C on ctx 0xA3D50 → 0x000BA5CE. Cluster of 2: member 0x000A3CF2
`elemstm_d50_call3__2` → slot 0xA3D6C → 0x000BA618. Caller 0x00139DF4. [O]

#### 0x000A3CD0 `elemstm_d50_call4` — stub
Calls DAT_000A3D60 on ctx 0xA3D50 → 0x000BA5FC. Caller 0x00139B9C. [O]

#### 0x000A3CDE `elemstm_d50_chain` — stub
Two chained calls: slot 0xA3D64 on ctx 0xA3D50 (→ 0x000B7710/0x000BA60A area) then slot
0xA3D68 on the result. Caller 0x00139C40. [O]

#### 0x000A3D00 `elemstm_d50_chain_arg` — stub
Chained: slot 0xA3D70(0xA3D50, p1) then slot 0xA3D68 on the result. Caller 0x00139CE4. [O]

#### 0x000A3D16 `elemstm_out_ba050` — stub
Calls DAT_000A3D74(p1) → 0x000BA050. Caller 0x00134A28. [O]

#### 0x000A3D1C `dtc_check_pair_3d78` — logic
Returns 0 only when both checks pass: slot 0xA3D7C(0xA3D78, p1) == 0 and slot 0xA3D84(0xA3D80,
p1, p2) == 0; else 1 (default true — fails closed to "invalid"). Caller 0x001554BC. [I]

#### 0x000A3D88 `elemstm_step_set1` — logic
Coordinated element-state-machine step (object set 1: list 0x3F90/0x3F94, iterator 0x3F98/
0x3F9C): bumps tick at p1+1; when the enable flag at **0x3F90 is 1 — (re)inits every element to
state 0 via the +0x2C vtable method; then walks element *0x3F94: gates on +0x5C and +0x74
methods, sets state 1 or 2, returns 2; at list end resets the index, and returns 1 only when
every element reports 1 via +0x20. Called from method area 0x000BA5A2. [I]

#### 0x000A3E60 `elemstm_cmd_set1` — logic
Command application (bit 1 of p2): when the mode byte at obj+8 is 0, per element checks the
+0x38 method and sets state 4 + runs the +0x68 method, else state 2; when mode is nonzero sets
all to 2, and when mode==2 runs a 21-call setup chain (slots 0xA3FA4–0xA4044). Bumps tick at
p1+2. Called from 0x000BA5B0. [I]

#### 0x000A3F80 `elemstm_done_set1` — logic
Bumps tick at p1+3; scans all elements (iterator 0xA4284/0xA4280) via the +0x20 method looking
for uniform 0x80; returns 1 when the record's +6 flag is set or all elements are 0x80 and gate
0xA428C(0xA4288) passes — completion detector. Called from 0x000BA5C0. [I]

#### 0x000A4098 `elemstm_step_set2` — logic
Byte-identical twin of 0x000A3D88 on object set 2 (list 0x4290/0x4294, iterator 0x4298/
0xA429C), tick at p1+4. Called from 0x000BA5FC. [I]

#### 0x000A1170 `dtc_init_call_chain` — logic
Eight sequential method calls opening the fault subsystem: slots 0xA1210–0xA1238 on contexts
0xA11C8, 0xA11C0, 0xA1214, 0xA11E8, 0xA1224, 0xA11F4, 0xA1230, 0xA11F0 (several slots are
passed themselves as the second argument — registration-style calls). Sole caller 0x000A0E20.
[O]

#### 0x000A3B52 `dtc_aeb5a_dispatch` — stub
Member of the 0x000A3B32 cluster: slot 0xA3C60(ctx 0xA3C50, p1) → freeze-frame dispatcher
0x000AEB5A. Callers 0x00144108, 0x00148248. [I]

#### 0x000A3BCC / 0x000A3BDA / 0x000A3BE8 `dtc_call_aee3c` / `dtc_call_aee44` / `dtc_call_aee4c` — stub
Remaining members of the 0x000A3B94 cluster (6 total): slots 0xA3C80/0xA3C84/0xA3C88 on ctx
0xA3C50 → 0x000AEE3C / 0x000AEE44 / 0x000AEE4C (fault-engine queries). [O]

#### 0x000A4170 `elemstm_cmd_set2` — logic
On bit 1 of p2: iterates element set 2 (0xA4298/0xA429C) and sets every element to state 2 via
the +0x2C vtable method; always returns 1. Called from 0x000BA626. [I]

#### 0x000A41A0 `elemstm_done_set2` — logic
Twin of 0x000A3F80 on element set 2: bumps tick at p1+5, scans via +0x20 for uniform 0x80,
done when record+6 is set or all-0x80 plus gate 0xA428C(0xA4288). Called from 0x000BA60A. [I]

#### 0x000A4200 `elemstm_set0x80_set1` — logic
Gated by obj(0xA42A0)[0xD8]==0 && [0xD9]==0: for each element (set-1 iterator) whose +0x38
method returns 1, runs the +0x14 method then sets state 0x80 via +0x2C; finally sets the
record's +6 done flag. Called from 0x000BA5DC. [I]

#### 0x000A437E `elemstm_idx_to_factor` — dispatch
Switch over index 3..0x43 mapping to scale constants — 10 (3,4,8,9,0x22,0x23,0x32,0x33),
0x32 (0xA,0xB), 6 (0xF), 0x1C0 (0x10,0x11,0x14,0x15), 0x180 (0x16,0x17), 0x78 (0x18,0x19),
0xE (0x1A,0x1B,0x1E,0x1F), 0xC (0x20,0x21), 2 (0x24,0x25,0x2C..0x2F) — converted through slot
0xA4488; 28 indices fall through to the unconverted path. Called from 0x000BA5EC. [I]

#### 0x000A446E `elemstm_obj_by_idx` — dispatch
Table lookup: index (3..0x33, matching 0x000A437E's case set) → element-object pointer from
the flash table at 0x000A45C4–0x000A4638 (case 0xF/0x2E share one entry); default NULL. No
resolved callers (pointer-invoked). [O]

#### 0x000A463C `out_byte_4664_get` — data-accessor
Returns *PTR_DAT_000a4664. Caller 0x001534FC. [O]

#### 0x000A4644 `out_float_4668_inv_get` — data-accessor
Returns `~(int)*(float*)PTR_DAT_000a4668 & 0xFF` — bit-inverted low byte of a float-backed
value. Caller 0x001534FC. [O]

#### 0x000A465C `out_state_466c_get` — data-accessor
Returns (char)*PTR_DAT_000a466c. Callers 0x00132D8E, 0x0014FC5C. [O]

#### 0x000A4670 … 0x000A46C4 `out_u16_4700_get` … `out_w_4738_get` — data-accessor
Final run of 16 clusters of 3-instr getters: 0x000A4670/76/7C/82/88/8E/94/9A/A0/A6/AC return
`*(short*)PTR_DAT_000a47NN` for slots 0x4700→0x4728, and 0x000A46B2/B8/BE/C4 return
`*DAT_000a472C/30/34/38` (words). All are consumed by the output-monitor cluster at
0x0007B6D8 (same area as the 0x7B222 output-channel monitors in 1034420267_fault_groups.csv)
plus the r10/r14 readers — per-channel output-value export of this subsystem. [O]

#### 0x000A4676 `out_u16_4704_get` — data-accessor
See the 0x000A4670 run entry above. [O]

#### 0x000A467C `out_u16_4708_get` — data-accessor
See the 0x000A4670 run entry above. [O]

#### 0x000A4682 `out_u16_470c_get` — data-accessor
See the 0x000A4670 run entry above. [O]

#### 0x000A4688 `out_u16_4710_get` — data-accessor
See the 0x000A4670 run entry above. [O]

#### 0x000A468E `out_u16_4714_get` — data-accessor
See the 0x000A4670 run entry above. [O]

#### 0x000A4694 `out_u16_4718_get` — data-accessor
See the 0x000A4670 run entry above. [O]

#### 0x000A469A `out_u16_471c_get` — data-accessor
See the 0x000A4670 run entry above. [O]

#### 0x000A46A0 `out_u16_4720_get` — data-accessor
See the 0x000A4670 run entry above. [O]

#### 0x000A46A6 `out_u16_4724_get` — data-accessor
See the 0x000A4670 run entry above. [O]

#### 0x000A46AC `out_u16_4728_get` — data-accessor
See the 0x000A4670 run entry above. [O]

#### 0x000A46B2 `out_w_472c_get` — data-accessor
See the 0x000A4670 run entry above. [O]

#### 0x000A46B8 `out_w_4730_get` — data-accessor
See the 0x000A4670 run entry above. [O]

#### 0x000A46BE `out_w_4734_get` — data-accessor
See the 0x000A4670 run entry above. [O]

#### 0x000A46C4 `out_w_4738_get` — data-accessor
See the 0x000A4670 run entry above. [O]

## Unit summary

**Coverage:** 220 clusters, 252 functions named (rep + every member), 0 left unnamed.

**Counts by kind (name rows):** data-accessor 92, logic 77, stub 54, unknown 20, library 7,
dispatch 2.

**Subsystems found in 0x000A0D8A–0x000A46C4 (all in r08_app):**
1. **Fault-memory/DTC exported API** (`dtc_*`, 0xA0D8A–0xA1296 + freeze-frame dispatch
   wrappers 0xA3B22–0xA3BE8): thin wrappers over a flash pointer table (0xA0EF8–0xA1238) into
   the fault engine at 0x0009Bxxx–0x000AExxx. Status predicates decode UDS-style DTC status
   bits (bit0 testFailed, bit2 pending, bit3 confirmed, bit4 notCompletedSinceLastClear,
   bit15 invalid marker; table 0x000A0448, index 1..0xC3, marker byte 0xAA/0xA5/0x55 at
   0xA0444) — matches spec 13. DID handlers 0x269-0x26B/0x2E2/0x210 (0x000AD606/0x000AD8DA)
   tie to spec 15.
2. **Adaptation/teach-in parameter subsystem** (`adapt_*`, 0xA12FC–0xA2C88): per-index (1..5)
   value tables, counters, clamp-between-min/max updates, mode-gated curve lookups, and a
   427-instr candidate arbiter (0x000A23F4). All out-of-range indices report through the
   central (0x248, group, detail) error sink. Likely spec 11 territory but NOT asserted.
3. **Limiter/output coordinator** (`lim_*`/`seq_*`, 0xA2D5E–0xA3D1C): task-invoked method-call
   sequences plus two coordinators computing a clamped desired value with reason codes into
   DAT_fff8e504/0x505/0x50E, filters, and a 0x3E-byte snapshot record at *0xA3828.
4. **Coordinated element state machine** (`elemstm_*`, 0xA3B62–0xA446E): two element sets with
   vtable-driven states {0,1,2,4,0x80}, step/cmd/done methods, an index→scale table and an
   index→object table (0x45C4–0x4638). Shape matches a phase/element engine (spec 11?) but
   identity is [I].
5. **Value-export getters** (`lim_slot*`, `out_*`): 3-instr getters behind sequential pointer
   slots, consumed by packer 0x000BC0F4 (emits with id 0x5EB) and the 0x0007B6D8/0x0007B222
   output-channel monitors.

**`unknown` entries (20), all pointer-invoked method-call sequences with no resolved callers
(mechanics observable, purpose not):** 0x000A2D5E, 0x000A2D88, 0x000A2DCA, 0x000A2E20,
0x000A2E74, 0x000A2E8A, 0x000A2EBC, 0x000A2EE0, 0x000A2EFC, 0x000A2F0C, 0x000A2F34,
0x000A2F50, 0x000A2F60, 0x000A2F72, 0x000A308C, 0x000A30B2, 0x000A30CC, 0x000A31A4,
0x000A31D0, 0x000A31E6. They likely install/start the limiter subsystem's objects (their
contexts 0xA2F90–0xA3240 are the same ones the coordinators use) — resolving them needs the
init-table that points here (probably in r10/r14 units or the 0x000AE/0x000BA vtable areas).

**Cross-region leads for other units:**
- r10_app/r14_app monitor code (0x0013xxxx–0x0015xxxx) is the dominant caller base of
  `dtc_report_event` (0x000A0E70), `dtc_update_event` (0x000A105E), and the `dtc_is_*`
  predicates — those units should name call sites as fault-report/monitor logic.
- Fault-engine implementations live at 0x0009B918/0x0009BC38/0x0009BE50/0x0009FA5E and
  0x000AC07A–0x000AEE54 (r08_app, other units) — cross-check the event-count 0xC3/0xC4 bound
  against 1034420267_dtc_list.csv.
- 0x000BC0F4 (this region, other unit) packs the `lim_slot*` exports with constant 0x5EB —
  CAN-transmit (spec 08) or measurement (spec 10) units should identify 0x5EB.
- 0x0007B222/0x0007B6D8 output-channel monitors (fault_groups.csv) consume the `out_*`
  getters — spec 12/13 units.
- Cluster 0x000A3AF6 spans r08 and r10 (member 0x00132334): the r10 unit will see the twin
  wrapper 0x00132382.
