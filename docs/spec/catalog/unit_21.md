# Unit 21 (r08_app)

Clusters from `units/unit_21_r08_app.csv` in unit order. Region r08_app (0x000B83CA-0x000BAD28)
is dominated by a layered interface-dispatch architecture: exported API functions call through
constant function-pointer tables with an interface object pointer (Ghidra resolves many indirect
targets into r0c_app/r10_app implementations, cited per entry). Recurring interface families:
- obj 0x000B84A4 (table 0x000B84A8..0x000B84CC): position/state-transition service (enable,
  disable+mode, request move from->to when both settle flags 0, map-derived set, bool queries).
  Possibly the park-lock actuator / range-transition service [I].
- obj 0x000B85F8 (table 0x000B85FC..0x000B8664): table/lookup service — byte table reads by
  index and 10-column 2D byte tables indexed [x+1+(y+1)*10] (range/gear-pair shaped) plus a
  pair-list validator that raises reports (0x193,...).
- objs 0x000B8628/0x000B8630/0x000B862C: small state bytes, monitor update, index table.

#### 0x000B83CA `lib_a84a4_req_move` — logic
API wrapper (obj 0x000B84A4, slot 0x000B84A8) resolved to 0x000DC3AA: requests a state/position
transition (p2->p3) but only when p4==p5==0 and obj+4==0; then 0x001081C0 pushes p2/p3 into two
sub-objects (setpoint pair). Called by lever watcher 0x000D140C. Cluster of 1. [I]

#### 0x000B83E6 `lib_a84a4_set_mode` — logic
Wrapper (slot 0x000B84AC) -> 0x000DC448: bool selects one of two interface calls (0x00108212
"both sub-objects default" vs table at 0x000DC63C) — mode/power select for the 0x84A4 service.
Caller 0x000CFD1A. Cluster of 2 (member 0x000B83EE uses slot 0x000B84B0). [I]

#### 0x000B83F6 `lib_a84a4_enable` — logic
Wrapper (slot 0x000B84B4) -> 0x000DC472: sets obj+5=1 and triggers 0x00108224 (enable/start path).
Caller 0x000D0BAA. Cluster of 1. [I]

#### 0x000B83FE `lib_a84a4_set_state` — logic
Wrapper (slot 0x000B84B8) -> 0x000DC47E: sets obj+5=0 then 0x00108234 sets a value (mode) when
gate *PTR_DAT_00108334==0. Callers 0x000D0BAA, 0x000D093A. Cluster of 1. [I]

#### 0x000B8406 `lib_a84a4_set_from_map` — logic
Wrapper (slot 0x000B84BC) -> 0x000DC48A: looks up RAM global DAT_fff8e6bf through 2D-table read
0x000DC198, forwards result + p3 to table 0x000DC65C — map-derived setpoint. Caller 0x00103DE2.
Cluster of 1. [I]

#### 0x000B8410 `lib_a84a4_query` — logic
Wrapper (slot 0x000B84C0) -> 0x000DC08A -> FUN_000dcd5a: compound bool query (range checks via
0x000DC1E2 plus sub-object getters) against the 0x84A4 service. Callers 0x000DD2F8, 0x000B5A46.
Cluster of 4 — members 0x000B8420/30/40 are the same query through table slots 0x000B84C4/C8/CC. [I]

#### 0x000B8450 `lib_signal_read_short` — library
Wrapper (slot 0x000B84D0, no obj) -> 0x000D9FEC: resolves an index through 0x000DA032 then
converts via table 0x000DA0B8, returning a short signal value. Cluster of 3; members 0x000B8558
(slot 0x000B861C) and 0x000B8566 (slot 0x000B8620) are sibling one-arg readers. [I]

#### 0x000B84D4 `lib_b85f8_resolve_conv` — library
Wrapper (obj 0x000B85F8, slot 0x000B85FC) -> 0x000DC4A8: when p3==0, resolves p2 through table
0x000DC660 then converts via 0x000DC664, returning short. Callers 0x000DDB04, 0x000DE3E0. [O]

#### 0x000B84E6 `lib_b85f8_check3` — library
Wrapper (slot 0x000B8600) -> 0x000DC282: three-arg boolean check of the 0x85F8 service.
Caller 0x000DE2AC. Cluster of 1. [I]

#### 0x000B84FA `tbl_b85f8_lookup_idx` — data-accessor
Wrapper (slot 0x000B8604) -> 0x000DC1C4: plain byte-table read `DAT_000dc3e4[idx]`. Widely used
(8 callers incl. 0x000F24B2, 0x000B5A46). Cluster of 3 — members 0x000B850A (slot 0x000B8608)
and 0x000B851A (slot 0x000B860C) are sibling indexed byte readers of the same service. [O]

#### 0x000B852A `tbl_b85f8_lookup_2d_a` — data-accessor
Wrapper (slot 0x000B8610) -> 0x000DC198: 2D byte-table read `PTR_LAB_000dc3dc[x+1+(y+1)*10]`
(10-column grid with -1 margins — pair/range shaped). Callers incl. 0x000DDBB4, 0x00104090. [O]

#### 0x000B853C `tbl_b85f8_lookup_2d_b` — data-accessor
Wrapper (slot 0x000B8614) -> 0x000DC1AE: same 10-column 2D byte lookup with base DAT_000dc3e0.
Callers incl. 0x000DDBB4, 0x000B1352, 0x000AFC54. [O]

#### 0x000B854E `tbl_b85f8_pair_list_check` — logic
Wrapper (slot 0x000B8618) -> 0x000DBF12: for index 1..5 and mode 1 or 3, walks a 2-byte-pair
list (stride 0x0E) and forwards each pair to reporter table 0x000DC174, raising fault-style
reports (0x193,...) via 0x000DC170 on malformed pairs. Caller 0x000A29D4. [O]

#### 0x000B8574 `tbl_b85f8_set_idx` — logic
Wrapper (slot 0x000B8624, impl not resolved) passing obj 0x000B85F8 + byte arg — a setter for
the 0x85F8 service. Caller 0x000DDE78. Cluster of 2 (member 0x000B85B8 same slot). [I]

#### 0x000B857C `lib_glob_8628_read` — data-accessor
Returns the byte at *PTR_DAT_000b8628 — state byte read for the 0x8628 object family.
Caller 0x000A3960. Cluster of 1. [O]

#### 0x000B8584 `tbl_glob_862c_idx_read` — data-accessor
Returns DAT_000b862c[param_2] — byte-array read indexed by caller. Caller 0x000A3646. [O]

#### 0x000B858E `lib_mon_c630_update` — logic
Wrapper (obj DAT_000b8630, slot 0x000B8634) -> 0x000E3592: periodic update of a monitor with
six 0x8-stride state blocks (resets counters unless state==2 when disabled/input 0).
Callers 0x000A2D88, 0x000A31E6. Cluster of 2 (member 0x000B8596, slot 0x000B8638). [I]

#### 0x000B859E `lib_thunk_e5602` — stub
Tail-jump thunk through DAT_000b863c to 0x000E5602; no resolved callers (pointer-table reach). [O]

#### 0x000B85A4 `lib_thunk_e566c` — stub
Tail-jump thunk through DAT_000b8640 to 0x000E566C. Caller 0x000A2EE0. [O]

#### 0x000B85B0 `lib_b85f8_call_dcca2` — stub
Wrapper (obj 0x000B85F8, thunk slot 0x000B8648) reaching 0x000DCCA2; no direct callers. [I]

#### 0x000B85C0 `lib_b85f8_update` — logic
Wrapper (slot 0x000B864C) -> 0x000DC076: calls table 0x000DC17C then DAT_000dc184(obj,0x34) —
periodic housekeeping of the 0x85F8 service. Caller 0x000A2E74. Cluster of 4 — members
0x000B85C8/0x000B85D8/0x000B85E0 use slots 0x000B8650/58/5C. [I]

#### 0x000B85D0 `lib_b85f8_read_flag` — stub
Wrapper (thunk slot 0x000B8654) -> thunk_FUN_000dcc92: reads a flag via table 0x000DCE1C into
obj+1. Caller 0x000A2F34. [O]

#### 0x000B85E8 `lib_8628_call_slot` — stub
Wrapper (obj *PTR_DAT_000b8628, slot 0x000B8660) -> 0x000E89BA. Caller 0x000BAEB4.
Cluster of 2 (member 0x000B85F0, slot 0x000B8664). [I]

#### 0x000B8668 `tbl_obj_8888_map_read0` — data-accessor
Fetches object member at (*PTR_DAT_000b8888)+0x24 and reads it through 2D-map evaluator
0x000B6D14 (bounds-checked short map: w=obj[0], h=obj[1], data+8) with flag 0. 23 callers incl.
shift modules 0x0010A576, 0x0010C402. Cluster of 1. [O]

#### 0x000B867E `tbl_obj_8888_map_read1` — data-accessor
Same 2D-map read of member +0x24 as 0x000B8668 but passes flag 1. Caller 0x00108D7C etc. [O]

#### 0x000B8694 `tbl_cal_curve_read2` — data-accessor
Row 2 of the 2D calibration map at object member (*PTR_DAT_000b8888)+0x24, evaluated at byte x
via bounds-checked map read 0x000B6D14 (w=obj[0], h=obj[1], data+8); no direct callers
(pointer/table reach). Cluster of 1. [O]

#### 0x000B86AA `tbl_cal_curve_read3` — data-accessor
Same map read as 0x000B8694 but row 3. Cluster of 1. [O]

#### 0x000B86C0 `tbl_cal_curve_read4` — data-accessor
Same map read, row 4. Callers include 0x000D1922, 0x000EC578 (from 0x000B6D14 caller list). [O]

#### 0x000B86D6 `tbl_cal_curve_read5` — data-accessor
Same map read, row 5. [O]

#### 0x000B86EC `tbl_cal_curve_read6` — data-accessor
Same map read, row 6. [O]

#### 0x000B8702 `tbl_cal_curve_read7` — data-accessor
Same map read, row 7. [O]

#### 0x000B8718 `tbl_cal_curve_read8` — data-accessor
Same map read, row 8. [O]

#### 0x000B872E `tbl_cal_curve_read9` — data-accessor
Same map read, row 9. [O]

#### 0x000B8744 `tbl_cal_curve_read10` — data-accessor
Same map read, row 10. [O]

#### 0x000B875A `tbl_cal_curve_read11` — data-accessor
Same map read, row 11. With rows 0-1 (0x000B8668/0x000B867E) this completes a 12-curve family
over one calibration map. [O]

#### 0x000B878E `lib_debounce_hold_byte` — library
Byte hold-counter: on rising edge of flag p2==1 latches p3, while flag stays 1 decrements
(count 1..0xFE), when flag !=1 clears to 0; stores flag at p1[1]. Used by all gate-writers below. [O]

#### 0x000B87C4 `lib_gate_write_val_pair` — logic
Runs the hold-counter 0x000B878E and while its count != 0 writes the same byte p4 into two
targets. Cluster of 2 — member 0x000B87E8 is the identical pattern. Caller 0x000B8A36. [O]

#### 0x000B880C `lib_gate_write_val_comp8` — logic
Hold-counter gate writing redundant byte pair: *p2 = p4 and *p3 = ~p4 while active.
Caller 0x000B8AD4. [O]

#### 0x000B8832 `lib_gate_write_val_comp16` — logic
16-bit variant: writes ushort value and its complement while the 0x000B878E counter is active.
Cluster of 2 — member 0x000B8858 identical. Caller 0x000B8AD4. [O]

#### 0x000B8898 `lib_walk_clamp_write8` — library
Walks entries via callback p6(idx) using 0x7F/0x7E('~')/0x7D('}') as end/skip/stop controls,
clamps the byte result to [p3,p4] via 0x000B7752, stores it, and advances idx. Used by 0x000B8A36. [O]

#### 0x000B8958 `lib_walk_clamp_write16` — library
16-bit walker: callback returns -1 end / -2 skip / value; result clamped via 0x000B7786 to
[p4,p5], stored with its complement; advances idx. Caller 0x000B8AD4. [O]

#### 0x000B8A36 `lib_cond_signals_grp1` — logic
Gated on *PTR_DAT_000b8ab8==1, runs three conditioners (0x000B87C4, 0x000B87E8, 0x000B8898)
over object 0x000B8ABC members +0x34..+0x48 into caller struct slots +0/+2/+4. Caller 0x000B912C. [I]

#### 0x000B8AD4 `lib_build_redundant_snapshot` — logic
573-instr builder: gated on *PTR_DAT_000b8dd0==1, reads object 0x000B8DD4 (members 0x0C-0x30,
plus curve members 0x18/0x1C/0x20 evaluated at indices 0..0xB via tables 0x000B8DF0/F4) and
writes ~24 redundant (value,~value) pairs into caller struct +7..+0x46 through 0x000B8832/
0x000B8858/0x000B880C/0x000B8958 (bounds 0..12000, count 60). Callers 0x000A32E4, 0x000A34D8. [I]

#### 0x000B90A0 `lib_sbyte_from_off128` — library
Converts (ushort&0xFF)-0x80 through table 0x000B910C and returns it as signed char —
offset-binary to signed decoding. Caller 0x000B912C. [O]

#### 0x000B90B2 `meas_chain_fff89c41` — logic
Four-stage conversion chain writing DAT_fff89c41..c44 (stage functions at PTR_PTR_000b9110 and
tables 0x000B911C/0x000B9124/0x000B9128 with object 0x000B9118). Callers 0x000DBD4E, 0x000B912C. [O]

#### 0x000B912C `meas_ram_refresh_task` — logic
520-instr periodic task (callers 0x000A2D5E, 0x000A2EBC): pulls dozens of interface getters
into RAM globals DAT_fff89cXX each with a mirror at DAT_fff89cd/dX, computes derived flags
(e.g. DAT_fff89cfd/cfe, mode select via 0x000B97F8), and calls 0x000B90B2, 0x000B90A0,
0x000B8A36 plus many 0x13xxxx-area services. This is the region's measurement/status RAM refresher. [I]

#### 0x000B9646 `meas_update_fff89c57` — logic
Small refresh: four getter reads into DAT_fff89c57/c58/c59/c9c (+mirrors) and sets
DAT_fff89c7a = 8000. Caller 0x000A2D88. [O]

#### 0x000B97E0 `lib_check_tag_match` — logic
Returns 1 exactly when p3==1 and p2 == p1+0x143 — a handle/tag match check. Caller 0x000A3D1C. [O]

#### 0x000B97F8 `lib_select_mode68` — logic
Mode select: if interface 0x000B9988 flag set and value != 8 → 6; if 0x000B9990 flag set → 8;
returns the (possibly overridden) value. Caller 0x000B912C (writes DAT_fff89cdc). [O]

#### 0x000B9824 `init_obj_state_flags` — logic
Initializes a small state struct: +0x18=1, +0=1, clears +0x17, +2, +0x19, +1. No resolved
callers — reached via pointer table. [O]

#### 0x000B983C `init_obj_state_b` — logic
Variant state initializer: +0x19=1, +1=1, clears +0x18/+0/+0x17/+2 (compare 0x000B9824).
Caller 0x000A2D5E. Cluster of 1. [O]

#### 0x000B9854 `init_obj_state_c` — logic
Third variant: +0x17=1, +2=1, clears the rest. No resolved callers. Cluster of 1. [O]

#### 0x000B986C `meas_scale_refresh_struct` — logic
473-instr periodic pipeline (callers 0x000A2D88/0x000A2EFC): converts raw struct fields
(+0x66..+0x108) through interface object 0x000B9998 (tables 0x000B999C-0x000B9C60) with >>5,
/10, /1000, *10 scalings and float constants (DAT_000b9dd8/de0/de8), computes derived validity
flags (+0xb2..+0xd7) and publishes a ~0x20-byte summary block through PTR_DAT_000b9ddc. [I]

#### 0x000B9DA4 `lib_flag2_select` — data-accessor
Two-way status read: p2==0 returns **PTR_PTR_000b9df0 else **PTR_PTR_000b9dec. Callers are all
in the 0x0EF-0x0F2 fault area plus 0x000D790A. Cluster of 1. [O]

#### 0x000B9DBE `lib_status_idx_get26` — data-accessor
Switch over p2 (1..0x19) reading status bytes through a 26-entry pointer-of-pointer table
(0x000B9FAC..0x000BA008); case 0x17 has an extra indirection via DAT_000ba00c. Callers
0x000D790A, 0x000F038E, 0x000F21BA, 0x000F068E, 0x000F1F38. [O]

#### 0x000B9EC6 `lib_status_idx_get9` — data-accessor
If-chain variant reading 9 pointer-of-pointer status bytes (0x000BA010..0x000BA030).
Callers 0x000D790A, 0x000F1F38, 0x000F13AC. [O]

#### 0x000BA050 `meas_idx_resolve_ram` — dispatch
Switch on p1 0xBBE/0xBC9..0xBD2: reads interface flags (tables 0x000BA154-0x000BA174 with
indices 1..5) into slots 0..7 of PTR_DAT_000ba158 and returns the slot pointer with a type
marker (1/2), or NULL. Shape matches a "resolve address for index" service (spec 10 style).
Caller 0x000A3D16. [I]

#### 0x000BA178 `lib_c248_enable` — stub
Family-C wrapper (obj 0x000BA248, slot 0x000BA24C) -> 0x000DD292: sets obj+5=1 (enable).
Cluster of 4 — members 0x000BA1F4 (slot 0x280), 0x000BA238 (slot 0x29C -> 0x000DD2F8),
0x000BA240 (slot 0x2A0 -> 0x000DD65C) are sibling no-arg calls on the same object. [O]

#### 0x000BA180 `lib_c248_disable` — stub
Slot 0x000BA250 -> 0x000DD298: sets obj+5=0 (disable). Caller 0x000CFD1A. [O]

#### 0x000BA188 `lib_c248_state_get8` — data-accessor
Returns byte at *PTR_DAT_000ba254 — family-C state read. Caller 0x000CF48C. [O]

#### 0x000BA190 `lib_c248_state_get16` — data-accessor
Returns short at *PTR_DAT_000ba258. Caller 0x000CEEB4. [O]

#### 0x000BA196 `lib_c248_mode_get8` — data-accessor
Returns mode byte at *PTR_DAT_000ba25C (values 0..7 used by 0x000BA212). Caller 0x000DF3E4. [O]

#### 0x000BA19E `lib_c248_mode_active` — data-accessor
Bool: *PTR_DAT_000ba25C != 0. Seven callers across fault (0x000F0C18, 0x000F0728) and shift
(0x00106AE6) modules. [O]

#### 0x000BA1A8 `lib_c248_state_get32` — data-accessor
Returns dword at *PTR_DAT_000ba260. Caller 0x000EF138. [O]

#### 0x000BA1AE `lib_c248_flag_get8` — data-accessor
Returns byte at *PTR_DAT_000ba264. Callers 0x000D134A, 0x000DC282, 0x000DC4C4. [O]

#### 0x000BA1B6 `lib_c248_check_sm` — logic
Slot 0x000BA268 -> 0x000DD59E: clears obj+0x10, and when obj+4==1 validates obj+1 state
(1 via interface 0x000DD73C; 3 via *PTR_DAT_000dd740 not in {0,6}; else raises report
0x2EE/0xF/8/3/0xCC) storing result at obj+0x10. Caller 0x000D3F78. [O]

#### 0x000BA1C4 `lib_c248_state_get16b` — data-accessor
Returns short at *PTR_DAT_000ba26C. Caller 0x000D38B4. [O]

#### 0x000BA1CA `lib_c248_flag_get8b` — data-accessor
Returns byte at DAT_000ba270. Caller 0x000A32E4. [O]

#### 0x000BA1D2 `lib_c248_state_get32b` — data-accessor
Returns int at DAT_000ba274. Callers 0x000A32E4, 0x000A34D8. [O]

#### 0x000BA1D8 `lib_c248_check_valid` — logic
Slot 0x000BA278 -> 0x000DD23E: if obj+4==0 raises report (0x2EE,0xF,0,4,2,0xA4) and returns 0,
else returns *obj. Cluster of 2 (member 0x000BA1E6 slot 0x27C -> 0x000DD2C4). Callers
0x000DC282, 0x000DC4C4. [O]

#### 0x000BA1FC `lib_c248_state_get16c` — data-accessor
Returns short at *PTR_DAT_000ba284. Caller 0x000DC4C4. [O]

#### 0x000BA202 `lib_c248_check_cond` — logic
Slot 0x000BA288 -> 0x000DD29E: returns 1 when p2!=0 AND interface 0x000DD43C flag set AND
*DAT_000dd440==0. Caller 0x00103CEC. [O]

#### 0x000BA212 `lib_c248_state_valid` — logic
Returns 1 unless mode byte *PTR_DAT_000ba25C is 0 or 6 — "position/state plausible" check.
Callers 0x000D525C, 0x000D983E. [O]

#### 0x000BA228 `lib_d28c_call` — stub
Calls slot 0x000BA290 with object *PTR_DAT_000ba28C. Caller 0x000E8058. [O]

#### 0x000BA230 `lib_e294_shutdown` — logic
Slot 0x000BA298 -> 0x000D70DC: releases four interface calls (0x000D72CC-0x000D72E4) and
clears obj+0x34 — shutdown/reset of that service. Caller 0x000A2D88. [O]

### r08 service-layer state getters (pointer block 0x000BA430-0x000BA49C)
The following one-cluster getters each return a single scalar of one module's state block
(direct `*PTR_DAT/​*DAT` read, or a one-slot interface call `if`). Callers span the fault area
(0x0CE-0x0F2), shift/solenoid area (0x00107FE0 = WK/Sys regulator, 0x0010EDB2...), and
scheduler area (0x000A3xxx), so this is the module's exported read API. Values 3/4 compared
in several predicates suggest range/gear-style state codes [I].

#### 0x000BA2A4 `lib_get8_430` — data-accessor
Returns byte at *PTR_DAT_000ba430. Caller 0x000DCE40. [O]

#### 0x000BA2AC `lib_get8_if438` — data-accessor
Interface getter (obj 0x000BA434, slot 0x000BA438 -> 0x000E0288) returning a state byte.
Caller 0x000DCE40. [O]

#### 0x000BA2BA `lib_get8_if440` — data-accessor
Interface getter (obj *PTR_DAT_000ba43C, slot 0x000BA440 -> 0x000DE94A). Caller 0x000A37A6. [O]

#### 0x000BA2C8 `lib_get16_444` — data-accessor
Returns short at *PTR_DAT_000ba444. Caller 0x000DC4C4. [O]

#### 0x000BA2CE `lib_get8_448` — data-accessor
Returns byte at *PTR_DAT_000ba448. Callers 0x000D134A, 0x000DC4C4. [O]

#### 0x000BA2D6 `lib_get32_44c` — data-accessor
Returns word at *DAT_000ba44c. Caller 0x000A3874. [O]

#### 0x000BA2DC `lib_get8_if454` — data-accessor
Interface getter (obj 0x000BA450, slot 0x000BA454 -> 0x000E2C1C). Caller 0x000CE62A. [O]

#### 0x000BA2EA `lib_get8_458` — data-accessor
Returns byte at *PTR_DAT_000ba458. Caller 0x000EA4C8. [O]

#### 0x000BA2F2 `lib_get8_if45c` — data-accessor
Interface getter (obj *PTR_DAT_000ba43C, slot 0x000BA45C -> 0x000DE930). Callers 0x000D4C12,
0x000A3798, 0x000D78B8. Cluster of 2 — member 0x000BA3F8 is the same slot pattern. [O]

#### 0x000BA300 `lib_get8_460` — data-accessor
Returns byte at *PTR_DAT_000ba460. Callers 0x000B3F02, 0x000B3C80, 0x000CD256. [O]

#### 0x000BA308 `lib_get8_464` — data-accessor
Returns byte at *DAT_000ba464. Callers 0x000CE48C, 0x000A37B4, 0x000CE62A. [O]

#### 0x000BA310 `lib_get32_468` — data-accessor
Returns word at *_LAB_000ba468. Caller 0x00107FE0 (WK/Sys current regulator, spec 12). [O]

#### 0x000BA316 `lib_get16_46c` — data-accessor
Returns short at *PTR_DAT_000ba46c. Callers 0x0010EDB2, 0x0010E7D8, 0x0010EB9E (shift area). [O]

#### 0x000BA31C `lib_get16_470` — data-accessor
Returns short at *PTR_DAT_000ba470. Caller 0x000CE48C. [O]

#### 0x000BA322 `lib_get8_474` — data-accessor
Returns byte at *PTR_DAT_000ba474. Callers 0x000D3208, 0x000D38B4. [O]

#### 0x000BA32A `lib_get8_478` — data-accessor
Returns byte at *PTR_DAT_000ba478. Caller 0x000D38B4. [O]

#### 0x000BA332 `lib_get8_47c` — data-accessor
Returns byte at *PTR_DAT_000ba47c. Caller 0x000EAB0A. [O]

#### 0x000BA33A `lib_get16_480` — data-accessor
Returns short at *PTR_DAT_000ba480; busiest getter here (7 callers incl. 0x000D2982,
0x000D2CF0, 0x000D40AC). [O]

#### 0x000BA340 `lib_get32_484` — data-accessor
Returns word at *DAT_000ba484. Caller 0x000A38A6. [O]

#### 0x000BA346 `lib_get8_488` — data-accessor
Returns byte at *DAT_000ba488. Caller 0x000A38B4. [O]

#### 0x000BA34E `lib_get8_48c` — data-accessor
Returns byte at *DAT_000ba48c. Caller 0x000A3B14. [O]

#### 0x000BA356 `lib_get8_490_lt3` — data-accessor
Predicate: *DAT_000ba490 < 3. Caller 0x000B4B56. [O]

#### 0x000BA362 `lib_get8_494_in34` — data-accessor
Predicate: PTR_DAT_000ba494[5] is 3 or 4. Caller 0x000EA4C8. [O]

#### 0x000BA376 `lib_get32_498` — data-accessor
Returns word at *DAT_000ba498. Caller 0x000A32E4. [O]

#### 0x000BA37C `lib_get8_49c_eq3` — data-accessor
Predicate: *PTR_DAT_000ba49c == 3. Caller 0x000D37BC. [O]

#### 0x000BA386 `lib_get8_4a0` — data-accessor
Returns byte at *DAT_000ba4a0. Caller 0x000A3882. [O]

#### 0x000BA38E `lib_get8_4a4` — data-accessor
Returns byte at *DAT_000ba4a4. Caller 0x000A3890. [O]

#### 0x000BA396 `lib_get8_4a8` — data-accessor
Returns byte at *PTR_DAT_000ba4a8. Caller 0x000D8C3A. [O]

#### 0x000BA39E `lib_get8_4ac` — data-accessor
Returns byte at *DAT_000ba4ac. Caller 0x000D8C3A. [O]

#### 0x000BA3A6 `lib_set_if4b0` — stub
Calls slot 0x000BA4B0 with object *PTR_DAT_000ba43C (setter/trigger of that service).
Caller 0x000E8058. [O]

#### 0x000BA3AE `lib_set_if4b4` — stub
Calls slot 0x000BA4B4 with object *PTR_DAT_000ba450. Caller 0x000E8058. [O]

#### 0x000BA3B6 `lib_set_if4b8` — stub
Calls slot 0x000BA4B8 with object *PTR_DAT_000ba450. Caller 0x000E8058. [O]

#### 0x000BA3BE `lib_set_if4bc` — stub
Calls slot 0x000BA4BC with object *PTR_DAT_000ba43C. Caller 0x000E8058. [O]

#### 0x000BA3C6 `lib_get16_4c0` — data-accessor
Returns short at *PTR_DAT_000ba4c0. Callers 0x000CE48C, 0x000CEDC6, 0x000CE62A. [O]

#### 0x000BA3CC `lib_get16_4c4` — data-accessor
Returns short at *PTR_DAT_000ba4c4. Caller 0x000CE48C. [O]

#### 0x000BA3D2 `lib_get16_4c8` — data-accessor
Returns short at *PTR_DAT_000ba4c8. Caller 0x000CEDC6. [O]

#### 0x000BA3D8 `lib_get8_4cc_nz` — data-accessor
Predicate: *PTR_DAT_000ba4cc != 0. Callers 0x000CE48C, 0x000CE62A. [O]

#### 0x000BA3E2 `lib_get8_4d0_eq1` — data-accessor
Predicate: *PTR_DAT_000ba4d0 == 1. Caller 0x000CE62A. [O]

#### 0x000BA3EC `lib_get8_4d0_lt3` — data-accessor
Predicate: (byte)*PTR_DAT_000ba4d0 < 3. Caller 0x000CE62A. [O]

#### 0x000BA406 `lib_call_if4dc` — stub
Calls slot 0x000BA4DC with DAT_000ba4d8 -> 0x000E0FE4. Cluster of 3 — members 0x000BA40E and
0x000BA416 use sibling slots of the same table. Caller 0x000A308C. [O]

#### 0x000BA41E `lib_call_if4e8` — stub
Calls slot 0x000BA4E8 with object *PTR_DAT_000ba43C -> 0x000DE964. Caller 0x000A30B2. [O]

#### 0x000BA426 `lib_call_if4f0` — stub
Calls slot 0x000BA4F0 with object *PTR_DAT_000ba4ec -> 0x000E2640. Caller 0x000BAEB4. [O]

#### 0x000BA4F4 `lib_ba55c_call_set` — stub
Calls slot 0x000BA560 with object PTR_DAT_000ba55c and a u16 arg -> 0x000E8128; no direct
callers (table reach). Cluster of 4 — members 0x000BA514/0x000BA51C/0x000BA524 are sibling
slots of the same 0x000BA55C object table. [O]

#### 0x000BA52C `lib_ba57c_set3a` — stub
Calls slot 0x000BA580 with object PTR_DAT_000ba57c and three args -> 0x000E874A.
Caller 0x000EBC60. [O]

#### 0x000BA538 `lib_ba57c_set3b` — stub
Calls slot 0x000BA584 with object PTR_DAT_000ba57c and three args -> 0x000E8774.
Caller 0x000EBB6C. [O]

#### 0x000BA544 `lib_ba55c_call2` — stub
Calls slot 0x000BA588 with object PTR_DAT_000ba55c and a byte arg -> 0x000E8348.
Caller 0x000E39DC. [O]

#### 0x000BA54C `lib_ba55c_call` — logic
Calls slot 0x000BA58C with object PTR_DAT_000ba55c -> 0x000E8050. Cluster of 2 — member
0x000BA554 uses the next slot. Caller 0x000A30CC. [O]

#### 0x000BA594 `lib_get8_if668` — data-accessor
Interface call (slot 0x000BA668, obj PTR_DAT_000ba664 -> 0x000E9030) returning a state byte.
Cluster of 3 — members 0x000BA636/0x000BA644 are sibling slots. Caller 0x000CA176. [O]

#### 0x000BA5A2 `lib_get8_if670` — data-accessor
Interface call (slot 0x000BA670, obj *PTR_DAT_000ba66C -> 0x000A3D88) returning a state byte.
Caller 0x000A3C26. [O]

#### 0x000BA5B0 `lib_get8_if674` — data-accessor
Interface call (slot 0x000BA674, obj *PTR_DAT_000ba66C, one arg -> 0x000A3E60) returning a
state byte. Caller 0x000A3CA4. [O]

#### 0x000BA5CE `lib_get16_if67c` — data-accessor
Interface getter (slot 0x000BA67C, obj *PTR_DAT_000ba66C -> 0x000A4092) returning u16.
Caller 0x000A3CC2. [O]

#### 0x000BA5DC `lib_call_if680` — stub
Interface call (slot 0x000BA680, obj 0x000BA66C -> 0x000A4200). Caller 0x000A3C1E. [O]

#### 0x000BA5EC `lib_get16_if688` — data-accessor
Interface getter with byte arg (slot 0x000BA688 -> 0x000A437E) returning u16.
Caller 0x000A3C0E. [O]

#### 0x000BA5FC `lib_get8_if68c` — data-accessor
Interface getter (slot 0x000BA68C -> 0x000A4098) returning a byte. Caller 0x000A3CD0. [O]

#### 0x000BA618 `lib_get16_if694` — data-accessor
Interface getter (slot 0x000BA694 -> 0x000A41FA) returning u16. Caller 0x000A3CF2. [O]

#### 0x000BA626 `lib_get8_if698` — data-accessor
Interface getter with byte arg (slot 0x000BA698 -> 0x000A4170). Caller 0x000A3D00. [O]

#### 0x000BA652 `lib_call_if6a4` — stub
Interface call (slot 0x000BA6A4, obj 0x000BA664 -> 0x000E8BFA). Cluster of 2 — member
0x000BA65A uses the next slot. Caller 0x000A2EE0. [O]

#### 0x000BA6AC `lib_flag_set1` — stub
Writes 1 into *param_1. Cluster of 2 — member 0x000D11A0 (r0c_app region) is the identical
two-instruction setter. Callers 0x000A2E8A, 0x000A2EBC. [O]

#### 0x000BA6B2 `lib_evt_notify_all` — logic
Walks two NULL-terminated id tables (0x000BA728, 0x000BA72C) and calls the enqueue-style API
0x0007A6E0 with (1,id,1) and (3,id,1) — registers/notifies every entry of both event tables
into queues 0 and 2 (ring buffers at DAT_fff92cd0, counts DAT_fff9389a). No direct callers
(init-time). [I]

#### 0x000BA6F4 `lib_evt_trigger_a` — logic
If gate byte *param_1 != 0, calls 0x0007A520 (-> 0x00072AE0, OS-style trigger with reports
0x228) with table 0x000BA728[p2-0x20]. Heavily used (25 callers incl. fault modules 0x000F*,
0x000B912C, 0x000BC0F4) — the "raise action/event by code" API for table A. [I]

#### 0x000BA70C `lib_evt_trigger_b` — logic
Same as 0x000BA6F4 over table 0x000BA72C. Callers 0x000F2AFC, 0x000F02EA. [I]

#### 0x000BA734 `lib_get8_76c` — data-accessor
Returns byte at *DAT_000ba76c. Caller 0x000B97F8 (mode selector). [O]

#### 0x000BA73C `lib_get8_770` — data-accessor
Returns byte at *PTR_DAT_000ba770. Caller 0x000EA4C8. [O]

#### 0x000BA744 `lib_get8_774` — data-accessor
Returns byte at *PTR_DAT_000ba774. Caller 0x000D093A. [O]

#### 0x000BA74C `lib_get8_778` — data-accessor
Returns byte at *PTR_DAT_000ba778. Callers 0x000DCFDA, 0x000DCEDC. [O]

#### 0x000BA754 `lib_thunk_e93c0` — stub
Tail-jump thunk through DAT_000ba77c to 0x000E93C0. Caller 0x000A2DCA. [O]

#### 0x000BA75A `lib_thunk_e93c8` — stub
Thunk through DAT_000ba780 to 0x000E93C8. Caller 0x000A2F0C. [O]

#### 0x000BA760 `lib_thunk_e93ca` — stub
Thunk through DAT_000ba784 to 0x000E93CA; no direct callers. [O]

#### 0x000BA78C `lib_get8_95c` — data-accessor
Returns byte at *PTR_DAT_000ba95c. Caller 0x000E4522. [O]

#### 0x000BA794 `lib_set_if964` — stub
Interface setter (slot 0x000BA964, obj 0x000BA960 -> 0x000EAD56) with byte arg.
Caller 0x000D8678. [O]

#### 0x000BA79C `lib_get8_if968` — data-accessor
Interface getter (slot 0x000BA968, obj 0x000BA960 -> 0x000EAC5E) returning a state byte;
9 callers (0x000B18E0, 0x000E476E, ...). [O]

#### 0x000BA7AA `lib_get16_96c` — data-accessor
Returns short at *PTR_DAT_000ba96c. Caller 0x000DC4C4. [O]

#### 0x000BA7B0 `lib_get8_970` — data-accessor
Returns byte at *PTR_DAT_000ba970. Caller 0x000DC4C4. [O]

#### 0x000BA7B8 `lib_get16_974` — data-accessor
Returns short at *PTR_DAT_000ba974. Caller 0x000DC4C4. [O]

#### 0x000BA7BE `lib_get8_978` — data-accessor
Returns byte at *PTR_DAT_000ba978. Caller 0x000DC4C4. [O]

#### 0x000BA7C6 `lib_get8_97c` — data-accessor
Returns byte at *PTR_DAT_000ba97c. Caller 0x000D525C. [O]

#### 0x000BA7CE `lib_get8_if984` — data-accessor
Interface getter (slot 0x000BA984, obj 0x000BA980 -> 0x000EEA96) returning a byte.
Cluster of 2 — member 0x000BA7EC uses slot 0x000BA98C. Caller 0x000DC4C4. [O]

#### 0x000BA7DC `lib_get16_if988` — data-accessor
Interface getter with byte arg (slot 0x000BA988, obj 0x000BA980 -> 0x000EEB22) returning
short. Caller 0x000E47CA. [O]

#### 0x000BA7FA `lib_get8_990` — data-accessor
Returns byte at *PTR_DAT_000ba990. Caller 0x000D3830. [O]

#### 0x000BA802 `lib_get16_994` — data-accessor
Returns short at *PTR_DAT_000ba994. Caller 0x000D37FE. [O]

#### 0x000BA808 `lib_get8_998` — data-accessor
Returns byte at *PTR_DAT_000ba998 — this pointer is also the object used by the 0x000BA84E/
0x000BA85E/0x000BA874/0x000BA8DE interface queries. Callers 0x000D07B0, 0x000DC4C4, 0x000AFC54. [O]

#### 0x000BA810 `lib_get8_99c` — data-accessor
Returns byte at *PTR_DAT_000ba99c. Callers 0x000DC4C4, 0x000AFC54. [O]

#### 0x000BA818 `lib_get8_9a0` — data-accessor
Returns byte at *PTR_DAT_000ba9a0. Caller 0x000DC4C4. [O]

#### 0x000BA820 `lib_get8_9a4` — data-accessor
Returns byte at *PTR_DAT_000ba9a4. Caller 0x000DC4C4. [O]

#### 0x000BA828 `lib_get8_9a8` — data-accessor
Returns byte at *PTR_DAT_000ba9a8. Caller 0x000DC4C4. [O]

#### 0x000BA830 `lib_get8_9ac` — data-accessor
Returns byte at *PTR_DAT_000ba9ac. Caller 0x000DC4C4. [O]

#### 0x000BA838 `lib_get8_9b0` — data-accessor
Returns byte at *PTR_DAT_000ba9b0. Caller 0x000DC4C4. [O]

#### 0x000BA840 `lib_get8_9b4` — data-accessor
Returns byte at *DAT_000ba9b4. Callers 0x000E1E00, 0x000DFE68, 0x000DC4C4, 0x000AFC54. [O]

#### 0x000BA848 `lib_gets8_9b8` — data-accessor
Returns signed byte at *PTR_DAT_000ba9b8. Callers 0x000DC4C4, 0x000D1990. [O]

#### 0x000BA84E `lib_get8_if9bc` — data-accessor
Interface query (slot 0x000BA9BC on obj *PTR_DAT_000ba998 -> 0x000DDA54) with byte arg.
Caller 0x00103CEC. [O]

#### 0x000BA85E `lib_get8_if9c0` — data-accessor
Interface query (slot 0x000BA9C0, obj *PTR_DAT_000ba998 -> 0x000DDBA8) returning a byte.
Cluster of 3 — members 0x000BA8C2 (slot 0x000BA9EC) and 0x000BA8D0 (slot 0x000BA9F0).
Caller 0x000A32E4. [O]

#### 0x000BA86C `lib_get8_9c4` — data-accessor
Returns byte at *PTR_DAT_000ba9c4. Callers 0x000D30CE, 0x000D38B4, 0x000D31D0. [O]

#### 0x000BA874 `lib_get16_if9c8` — data-accessor
Interface query (slot 0x000BA9C8, obj *PTR_DAT_000ba998 -> 0x000DDF84) returning short.
Cluster of 2 — member 0x000BA8B4 uses slot 0x000BA9E8. Caller 0x000D31D0. [O]

#### 0x000BA882 `lib_get8_9cc` — data-accessor
Returns byte at *PTR_DAT_000ba9cc. Callers 0x000E202C, 0x0010B4AE. [O]

#### 0x000BA88A `lib_get8_9d0` — data-accessor
Returns byte at *PTR_DAT_000ba9d0. Caller 0x000E4548. [O]

#### 0x000BA892 `lib_get32_9d4` — data-accessor
Returns word at *DAT_000ba9d4. Caller 0x000A32E4. [O]

#### 0x000BA898 `lib_get8_9d8` — data-accessor
Returns byte at *DAT_000ba9d8. Caller 0x000A32E4. [O]

#### 0x000BA8A0 `lib_get16_if9e0` — data-accessor
Interface query (slot 0x000BA9E0, obj *PTR_DAT_000ba9dc -> 0x000E9452) returning short.
Cluster of 2 — member 0x000BA8F4 uses slot 0x000BA9FC. No direct callers. [O]

#### 0x000BA8AE `lib_get32_9e4` — data-accessor
Returns word at *DAT_000ba9e4. Caller 0x000A39C0. [O]

#### 0x000BA8DE `lib_get8_if9f4` — data-accessor
Interface query (slot 0x000BA9F4, obj *PTR_DAT_000ba998 -> 0x000DD938) returning a byte.
Caller 0x000DC4C4. [O]

#### 0x000BA8EC `lib_get8_9f8` — data-accessor
Returns byte at *PTR_DAT_000ba9f8. Callers 0x000E4CF4, 0x000E40DA. [O]

#### 0x000BA902 `lib_get8_a00` — data-accessor
Returns byte at *PTR_DAT_000baa00. Caller 0x000CD256. [O]

#### 0x000BA90A `lib_get8_ifa04` — data-accessor
Interface query (slot 0x000BAA04, obj *PTR_DAT_000ba9d0 -> 0x000EB118) returning a byte.
Callers 0x000E1E00, 0x000E40DA, 0x000AFB66, 0x001093EC, 0x000CA7F8. [O]

#### 0x000BA918 `lib_get8_a08` — data-accessor
Returns byte at *PTR_DAT_000baa08. Callers 0x000E202C, 0x0010B4AE. [O]

#### 0x000BA920 `lib_get8_a0c` — data-accessor
Returns byte at *PTR_DAT_000baa0c. Caller 0x000F0728. [O]

#### 0x000BA928 `lib_get16_ifa10` — data-accessor
Interface query (slot 0x000BAA10, obj *PTR_DAT_000ba9d0 -> 0x000EAEC4) returning short.
Caller 0x000DC4C4. [O]

#### 0x000BA936 `lib_get8_a14` — data-accessor
Returns byte at *PTR_DAT_000baa14. Callers 0x000F0728, 0x000F0D5C. [O]

#### 0x000BA93E `lib_get8_ifa18` — data-accessor
Interface query (slot 0x000BAA18, obj *PTR_DAT_000ba9dc -> 0x000E946A) returning a byte.
Cluster of 2 — member 0x000BA94C uses slot 0x000BAA1C. Callers incl. 0x000B1E7C, 0x000F13AC. [O]

#### 0x000BAA20 `lib_get8_ifc44` — data-accessor
Interface query (slot 0x000BAC44, obj 0x000BAC40 -> 0x000E9470) returning a byte.
Cluster of 2 — member 0x000BAA2E uses slot 0x000BAC48. Callers 0x000B1E7C, 0x000AFC54. [O]

#### 0x000BAA3C `lib_get8_ifc50` — data-accessor
Interface query (slot 0x000BAC50, obj 0x000BAC4C -> 0x000EAFA0) returning a byte. Cluster of
8 — members 0x000BAB1E/3A/50/5E/6C/8E/9C use sibling slots 0x000BACAC..0x000BACD4 of the same
0x000BAC4C object. Caller 0x000AFC54. [O]

#### 0x000BAA4A `lib_get8_c54` — data-accessor
Returns byte at *DAT_000bac54. Callers 0x000B1E7C, 0x000B148E. [O]

#### 0x000BAA52 `lib_get16_c58` — data-accessor
Returns short at *PTR_DAT_000bac58. Caller 0x000D4BA8. [O]

#### 0x000BAA58 `lib_get16_c5c` — data-accessor
Returns short at *PTR_DAT_000bac5c — the busiest getter in the unit (10 callers incl.
0x00107CFC, 0x0010A3A0, 0x0010809C, 0x000DC4C4). [O]

#### 0x000BAA5E `lib_get16_c60` — data-accessor
Returns short at *PTR_DAT_000bac60. Caller 0x000D9F62. [O]

#### 0x000BAA64 `lib_get16_c64` — data-accessor
Returns short at *PTR_DAT_000bac64. Caller 0x000D0704. [O]

#### 0x000BAA6A `lib_get8_c68` — data-accessor
Returns byte at *PTR_DAT_000bac68. Caller 0x000D05DC. [O]

#### 0x000BAA72 `lib_get16_ifc70` — data-accessor
Interface query (slot 0x000BAC70, obj *PTR_DAT_000bac6C -> 0x000EEFEA) returning short.
Caller 0x000D326C. [O]

#### 0x000BAA80 `lib_call_ifc74` — stub
Interface call (slot 0x000BAC74, obj *PTR_DAT_000bac6C -> 0x000EF0A0). Cluster of 2 — member
0x000BAC06 uses slot 0x000BACF0. Caller 0x000E8058. [O]

#### 0x000BAA88 `lib_get8_ifc7c` — data-accessor
Interface query (slot 0x000BAC7C, obj *PTR_DAT_000bac78 -> 0x000DDF76) returning a byte.
Cluster of 2 — member 0x000BAB2C uses slot 0x000BACB0. Caller 0x000DA0C4. [O]

#### 0x000BAA96 `lib_get8_ifc80` — data-accessor
Interface query (slot 0x000BAC80, obj *PTR_DAT_000bac78, byte arg -> 0x000DD940).
Caller 0x00104A28. [O]

#### 0x000BAAA6 `lib_set3_ifc84` — stub
Interface setter (slot 0x000BAC84, obj *PTR_DAT_000bac78, three args -> 0x000DDC38).
Caller 0x00103DE2. [O]

#### 0x000BAAB2 `lib_set2_ifc88` — stub
Interface setter (slot 0x000BAC88, obj *PTR_DAT_000bac6C, two args -> 0x000EEEA8).
Caller 0x000DD400. [O]

#### 0x000BAAC8 `lib_set4_ifc90` — stub
Interface setter (slot 0x000BAC90, obj *PTR_DAT_000bac78, four args -> 0x000DDDE8).
Caller 0x000D140C (the lever watcher). [O]

#### 0x000BAAE4 `lib_set3_ifc94` — stub
Interface setter (slot 0x000BAC94, obj *PTR_DAT_000bac78, three args -> 0x000DDE78).
Caller 0x00103B36. [O]

#### 0x000BAAF0 `lib_call_ifc98` — stub
Interface call (slot 0x000BAC98, obj *PTR_DAT_000bac78 -> 0x000DDC30). Cluster of 5 — members
0x000BAC16/26/2E/36 are sibling no-arg slots of the 0x000BAC78/6C objects. Caller 0x000DA152. [O]

#### 0x000BAAF8 `lib_set2_ifc9c` — stub
Interface setter (slot 0x000BAC9C, obj *PTR_DAT_000bac78, two args -> 0x000DDC14).
Caller 0x000DA310. [O]

#### 0x000BAB02 `lib_set_ifca0` — stub
Interface setter (slot 0x000BACA0, obj *PTR_DAT_000bac78 -> 0x000DDC1E) with one byte arg.
Caller 0x000DA1A8. [O]

#### 0x000BAB0A `lib_set2_ifca4` — stub
Interface setter (slot 0x000BACA4, obj *PTR_DAT_000bac78 -> 0x000DDC26). Caller 0x000DA17A. [O]

#### 0x000BAB14 `lib_set2_ifca8` — stub
Interface setter (slot 0x000BACA8, obj *PTR_DAT_000bac78 -> 0x000DDED6). Caller 0x000DD2F8. [O]

#### 0x000BAB48 `lib_get8_cb8` — data-accessor
Returns byte at *DAT_000bacb8. Caller 0x000B148E. [O]

#### 0x000BAB7A `lib_get8_ifcc8` — data-accessor
Interface query (slot 0x000BACC8, obj *PTR_DAT_000bac4c -> 0x000EB216) returning a byte.
Caller 0x000DB63C. [O]

#### 0x000BAB88 `lib_gets8_ccc` — data-accessor
Returns signed byte at *PTR_DAT_000baccc. Caller 0x000E4CF4. [O]

#### 0x000BABAA `lib_get_ifcd8_pair` — data-accessor
Interface query (slot 0x000BACD8, obj *PTR_DAT_000bac4c -> 0x000EB03A) with fixed args (2,3)
plus p2 and p2-1 — an edge/transition-style query on the 0x000BAC4C service. Caller 0x000D1990. [O]

#### 0x000BABC8 `lib_get8_ifcdc` — data-accessor
Interface query (slot 0x000BACDC, obj *PTR_DAT_000bac4c -> 0x000EB21C) returning a byte.
No direct callers. [O]

#### 0x000BABD8 `lib_get8_ce0` — data-accessor
Returns byte at *PTR_DAT_000bace0. Callers 0x00103CEC, 0x000F0728, 0x000F13AC. [O]

#### 0x000BABE0 `lib_get16_ifce4` — data-accessor
Interface query (slot 0x000BACE4, obj *PTR_DAT_000bac4c -> 0x000EAE18) returning u16.
Callers 0x000F0728, 0x000F13AC. [O]

#### 0x000BABEE `lib_call_ifce8` — stub
Interface call (slot 0x000BACE8, obj *PTR_DAT_000bac5c -> 0x000E9CE0). Caller 0x000DC4C4. [O]

#### 0x000BABF6 `lib_get16_ifcec` — data-accessor
Interface query (slot 0x000BACEC, obj *PTR_DAT_000bac6C, byte arg -> 0x000EEF78) returning
short. Callers 0x00104090, 0x000D05AE, 0x000B0708, 0x000D89DE. [O]

#### 0x000BAC0E `lib_call_thunk_eac64` — stub
Interface call (thunk slot 0x000BACF8, obj DAT_000bacf4 -> 0x000EAC2C via 0x000EAC64).
Caller 0x000A2F34. [O]

#### 0x000BAC1E `lib_call_ifd00` — stub
Interface call (slot 0x000BAD00, obj *PTR_DAT_000bac78 -> 0x000DD954). Caller 0x000A2F34. [O]

#### 0x000BAD10 `lib_call_ifd34` — stub
Interface call (slot 0x000BAD34, obj 0x000BAD30 -> 0x000DE0FE). Caller 0x000BAEB4. [O]

#### 0x000BAD18 `lib_call_ifd38` — stub
Interface call (slot 0x000BAD38, obj 0x000BAD30 -> 0x000DDFBC); no direct callers. [O]

#### 0x000BAD20 `lib_call_thunk_ebdbc` — stub
Interface call (thunk slot 0x000BAD40, obj DAT_000bad3c -> 0x000EB344 via 0x000EBDBC).
Caller 0x000A2F50. [O]

#### 0x000BAD28 `lib_call_ifd44` — stub
Interface call (slot 0x000BAD44, obj DAT_000bad3c -> 0x000EB348). Caller 0x000A2F60. [O]

#### 0x000BA5C0 `lib_get8_if678` — data-accessor
Interface query (slot 0x000BA678, obj *PTR_DAT_000ba66C -> 0x000A3F80) returning a byte.
Cluster of 2 — member 0x000BA60A uses slot 0x000BA690 (-> 0x000A41A0). Callers 0x000A3CB4 /
0x000A3CDE. [O]

## Unit summary
220 clusters, 273 named functions (rep + every member), 0 `unknown` (the region is a regular
wrapper/getter layer whose behavior is observable even where the owning subsystem is not
identifiable; module identity beyond the interface objects is marked [I] where inferred).

Counts by kind (per function / per cluster rep):
- data-accessor: 157 / 136 — the bulk of the unit: state getters and one-slot interface
  queries over pointer blocks 0x000BA430-0x000BA4A0, 0x000BA95C-0x000BAA18 and
  0x000BAC40-0x000BAD44, plus the 12-row calibration-curve readers (0x000B8668-0x000B875A).
- stub: 58 / 42 — trivial setters/calls through the same interface tables.
- logic: 48 / 34 — the two big periodic pipelines (0x000B912C, 0x000B986C), the redundant
  value+complement conditioning family (0x000B878E-0x000B8AD4), family A/C service wrappers,
  and the gated event triggers.
- library: 9 / 7 — debounce/hold counter, walker+clamp pipelines, offset-128 decode,
  signal-read helpers.
- dispatch: 1 / 1 — 0x000BA050 (index -> RAM-slot resolver, spec-10 shaped).
- irq / entry: none (all functions have resolved callers or pointer-table reach; no vector
  handlers in this region).
- unknown: 0 / 0.

Recurring subsystem hypotheses (all [I], keyed to interface object pointers — worth
cross-checking from the units that own the callees):
- obj 0x000B84A4 (`lib_a84a4_*`): position/state-transition service with enable/disable,
  guarded move requests and bool reachability queries — possibly the park-lock actuator or
  range-transition service; implementations live in r0c_app 0x000DC08A-0x000DC4AA and
  r10_app 0x001081xx-0x001085xx (those units should cross-check).
- obj 0x000B85F8 (`tbl_b85f8_*`): lookup service whose 2D tables are 10-column grids indexed
  [x+1+(y+1)*10] and whose validator (0x000BA854E -> 0x000DBF12) walks 1..5-indexed pair
  lists and raises reports 0x193 — range/gear-pair shaped; r0c_app 0x000DBEE8-0x000DC1C4 and
  0x000DBF12 own the semantics.
- obj 0x000BA248 (`lib_c248_*`): 0..7 state machine with enable/disable, plausibility rules
  (valid unless state 0 or 6) and 0x2EE reports; queried by fault (0x0CE-0x0F2) and shift
  (0x0010xxxx) code — candidate range/lever-position service (spec 14 area).
- Event/queue API: 0x000BA6B2/0x000BA6F4/0x000BA70C build on r04_boot 0x0007A6E0 (3-queue
  ring buffers at DAT_fff92cd0) and 0x0007A520 -> 0x00072AE0 (OS-style trigger, reports
  0x227/0x228); boot-region units should cross-check these tables (0x000BA728/0x000BA72C).
- Redundant RAM pairs: 0x000B912C/0x000B8AD4 maintain value+mirror pairs across
  DAT_fff89cXX/DAT_fff89cd0-fff89d7f — consistent with a safety/monitoring RAM block that
  other regions (0x0CE-0x0F2 fault monitors) consume.

Cross-region leads: heavy resolved callees into r0c_app (0x000DA000-0x000EEBxx) and r10_app
(0x0010xxxx-0x001085xx), plus one cross-region cluster member 0x000D11A0 (r0c_app copy of
0x000BA6AC). No spec-doc addresses existed in this region; nearest spec hooks are spec 10
(measurement index resolution, 0x000BA050) and spec 12 (0x00107FE0 consumes 0x000BA310).
