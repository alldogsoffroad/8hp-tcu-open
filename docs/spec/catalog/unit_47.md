# Unit 47 (r14_app)

Slice 0x001594A0–0x0015D5DC of the application (region r14_app, 0x14003A–0x15D946). Three
families live here:
1. **Safety-layer range/lever consistency checks** (0x1594A0–0x15A0E8): predicates and state
   updaters over range codes 5=D, 6=N, 7=R, 8=P (spec 14 §5a decode table), reading every state
   byte through RAM-self-test-validated accessors (0x1596CA byte / 0x159FBC byte / 0x0015A022
   u16 / 0x0015A094 u32 — each writes walking patterns into the cell, reads back, and checks the
   inverted copy). They sit in the same safety layer as the redundant park check 0x158C3E
   (spec 14 §6) and drive monitor wrappers at 0x155E2C–0x1572BE that report internal event
   class 0x18 (subs 0x11/0x16/1/2/7…) and reset lever-object pairs through 0x158C30.
2. **Fatal RAM-integrity handlers** 0x1597C8/0x159804: report internal event class 0x19
   (sub 1 = cell write/read failure, sub 2 = value/complement mismatch) and then park the CPU
   in an infinite loop (0x1561F0) — an application-side variant of the fatal internal-error
   path in spec 13 §2.
3. **Static initializers** (0x15A458–0x15D5DC): constructors/registration functions for global
   module objects at 0xFFF8xxxx, reached from a construction table (all have callers='-'),
   including the CAN comm-monitor group table (spec 13 §2c) and the actuator-object
   neighborhood (spec 12 §1.2).

#### 0x001594A0 `park_lever_reset_cond_or` — logic
Predicate used by monitor wrapper 0x156F90 (which reports internal event class 0x18 sub 7):
returns 1 when either sub-check 0x001594CC or 0x00159524 fires; on true it invokes the action
pointer PTR_FUN_00159584(PTR_DAT_00159580) (the lever-object pair-reset style helper, same
family as 0x158C30, spec 14 §6). No globals of its own. Cluster of 1. [I] (range-code reading
per spec 14 §5a; spec 14 §6 neighborhood)

#### 0x001594CC `park_mismatch_chk_cmd_dr` — logic
Sub-check A: true when the enable byte at object+0xD4 chain offset +9 is set, the validated
range state (0x1596CA on PTR_DAT_00159590) is 5 or 7 (D/R), flag PTR_DAT_00159594 is 0,
flag PTR_DAT_00159598 is 1, and the range-code getter (PTR_FUN_0015956C) is NOT 6 or 8 (N/P).
Feeds 0x001594A0. Cluster of 1. [I]

#### 0x00159524 `park_mismatch_chk_tgt_dr` — logic
Sub-check B: true when the enable byte at object+0xD4 chain offset +10 is set, the range-code
getter (PTR_FUN_0015956C called with the +0xD4 object and param_2) returns 5 or 7 (D/R),
flag PTR_DAT_00159598 is 0, and flag PTR_DAT_0015957C is nonzero. Feeds 0x001594A0. Cluster
of 1. [I]

#### 0x0015959C `park_interlock_req_chk` — logic
Predicate for monitor 0x15700C. True when flag PTR_DAT_001597AC is 1 and either the current
range (PTR_DAT_001597A4) is not 8 (P) while the target getter returns 7 (R) under mode byte
+0xA0==1, or 7/5 (R/D) under mode +0xA0==2, or 8 (P); or the target is 5/7 (D/R) while the
current range is 6 (N). On true, resets a lever-object pair via PTR_FUN_001597B8 (resolved
0x158C30: writes 0/0xFFFFFFFF then calls PTR_FUN_00158D4C(0)) and returns 1. Cluster of 1. [I]

#### 0x00159616 `park_chk_shift_dr_cross_req` — logic
Predicate for monitor 0x157088: condition A = in range 7 (R) with request byte
PTR_DAT_001597BC == 5 (D) while the target getter is none of 6/8/5; condition B = in range 5
(D) with request byte == 7 (R), flag PTR_DAT_001597AC == 0 and target none of 6/8/7. Either
fires the same pair-reset action (PTR_FUN_001597B8 → 0x158C30) and returns 1. Cluster of 1. [I]

#### 0x001596A2 `park_chk_target_invalid` — logic
Predicate for monitor 0x156AA0 (which reports internal event class 0x18 sub 0x11): true when
the range-code getter (PTR_FUN_001597A8) returns any value other than 5/6/7/8 (D/N/R/P) —
i.e. the selected target range is not a valid code; then resets the pair via PTR_FUN_001597B8
(→ 0x158C30) and returns 1. Cluster of 1. [O] (membership test observable; code meanings per
spec 14 §5a)

#### 0x001597C8 `fatal_ram_celltest` — logic
RAM-integrity failure handler reached from the validated-read accessors (0x1596CA, 0x159FBC,
0x0015A022, 0x0015A094) when the in-cell write/read pattern test fails: reports internal
event class 0x19 sub 1 through the report entry behind PTR_FUN_00159840 (resolved 0x000A7B36,
8-arg call `f(0x19,1,0,0,0,0,0,0)`), then looks up monitored slot 0x13 via the dual-pointer
table get 0x1564AC(PTR_DAT_00159844,0x13) (sentinel 0xDEAC) and, if found, calls
0x1561F0(slot, **PTR_DAT_0015984C) whose tail is an infinite loop — report then halt. Also
called from ~20 other safety-layer sites. Cluster of 1. [O]

#### 0x00159804 `fatal_ram_invcpl` — logic
Twin of 0x001597C8 for the value/complement mismatch branch of the validated accessors:
reports internal event class 0x19 sub 2 (`f(0x19,2,0,0,0,0,0,0)` via 0x000A7B36), fetches
monitored slot 0x13 through 0x1564AC, and halts in 0x1561F0's infinite loop when the slot is
valid. Same 21-site caller set as 0x001597C8. Cluster of 1. [O]

#### 0x00159854 `stub_set_state3c_pair` — stub
Five-instruction setter for the park-module state struct: writes flag pair
`*(param+0x3C)=1`, `*(param+0x3D)=0xFE` (TRUE with complement). Called from 0x00159160. The
same pair is maintained by 0x00159A4A/0x00159F84. Cluster of 1. [O]

#### 0x00159862 `park_tick_downcount_timers` — logic
Two identical halves (struct offsets +0x00 and +0x0C, enabled by validated flag bytes at
+0x38 and +0x3A via 0x159FBC): reads the u32 countdown counter with 0x0015A094, writes back
counter−1 with its complement, and sets the done flag pair (+0x08/+0x09, +0x14/+0x15) when
the counter reaches 0; counter already 0 clears the flag. Called by the periodic task
0x156AA0. Cluster of 1. [O]

#### 0x001598CC `stub_store_flag3e_pair` — stub
Wrapper: stores the result of hysteresis comparator 0x001598E0 as the complement pair at
param+0x3E/+0x3F. Called by 0x156AA0. Cluster of 1. [O]

#### 0x001598E0 `park_val_gt_thres_hyst` — logic
Hysteresis comparator on a validated u16 value (0x0015A022 on PTR_DAT_00159A9C, scaled ×10)
against a per-range-code threshold (u16 pointer table at object+0x78 indexed by the validated
byte PTR_DAT_00159A98, also ×10), with the hysteresis band `**(u16**)(obj+0x50)` subtracted
from the threshold for the falling edge. Sets the output state at param+0x3E (1 when value
exceeds threshold, 0 when it falls below threshold−band). [I] The range-indexed threshold
suggests a speed-versus-range interlock (park engage gating, spec 14 §4.2) but the physical
input is not identified. Cluster of 1.

#### 0x00159956 `park_range_timer_update` — logic
Timer manager for the state struct: with enable gates ((flag +0x3E clear or calib byte +0xCC
clear) and (flag PTR_DAT_00159AA4 clear or calib byte +0xC8 clear)), reloads the u16 pair at
+0x18/+0x1A from the per-range table (object+0x28 indexed by byte PTR_DAT_00159A98) when
flag PTR_DAT_00159AA8==1, flag PTR_DAT_00159AAC==6 (N) and current range (PTR_DAT_00159AA0)
is 5 or 7 (D/R); otherwise decrements the pair (via 0x0015A022) setting done flags
+0x1C/+0x1D, or zeros it and returns −1 when the range is 5/7/8 or the gates fail. Called by
0x156B80. Cluster of 1. [I]

#### 0x00159A4A `park_update_flag3c_gated` — logic
Sets/clears the flag pair at +0x3C: cleared (0/0xFF) when flag PTR_DAT_00159AA4 is 1, or the
timer pair +0x18 is nonzero, or flag +0x3E is 1; otherwise set (1/0xFE). Then runs the
cascade 0x00159F84 (state step + conditional clear of +0x3C). Called by 0x156B80. Cluster
of 1. [I]

#### 0x00159AB0 `park_acc_seed_two` — logic
Seeds two measurement channels of the state struct from object PTR_DAT_00159CCC: mode byte
PTR_DAT_00159CD4 selects between table value (object+0x38 indexed by byte PTR_DAT_00159CD0),
object+0x30 word (written to both), or object+0x34 word; writes u16 seed pairs
+0x40/+0x42 and +0x4C/+0x4E (0/0xFFFF when mode >2), then reads each seed back with
0x0015A022 and builds u32 accumulator pairs +0x44/+0x48 and +0x50/+0x54 = seed + byte
`**(obj+0x9C)` with saturating add (overflow → −1). Called by 0x156B80. Cluster of 1. [O]

#### 0x00159BC2 `park_reqpair_update_a` — logic
Updates the first u32 request-counter pair (struct +0x00/+0x04, done flags +0x08/+0x09):
computes min(table[byte PTR_DAT_00159CD0] at object+0x68, word object+0x64) and the timeout
flag bVar1 (timer at PTR_DAT_00159CD8 ≤ min); reload paths — range PTR_DAT_00159CE0==7 with
secondary flag 5/7 or sub-flag (param+0xF)==0 → reload from counter pair +0x11 via
0x0015A094; invalid head (range 6/5/8 or sub-flag not 0/7) → zero the pair; timeout-qualified
path → load `**(obj+0x9C)`. Second half sets flag pair +0x38/+0x39 from flags
PTR_DAT_00159F24 (1/2) and PTR_DAT_00159F28 vs calib byte object+0xD0. Called by 0x159EC0
(and its wrapper 0x156B80). Cluster of 1. [I]

#### 0x00159D54 `park_reqpair_update_b` — logic
Mirror of 0x00159BC2 for the second u32 request-counter pair (struct +0x0C/+0x10, done flags
+0x14/+0x15, sub-flags +0x3A/+0x3B): thresholds from object PTR_DAT_00159F2C (+0x4C/+0x48),
timer PTR_DAT_00159F30, head range PTR_DAT_00159F38==5 with secondaries 7/5 or flag +0x3C==0
→ reload from +0x50; invalid head (6/7/8 or sub-flag not 0/5) → zero; timeout path → load
`**(obj+0x9C)`. Called by 0x159F4C (wrapper 0x156C1A). Cluster of 1. [I]

#### 0x00159EC0 `park_chk_r_state_timeout` — logic
Runs 0x00159BC2 (update A), then the predicate for monitor 0x156B80 (event class 0x18 sub
0x16 side): true when range flag PTR_DAT_00159F34 == 7 (R) and either the counter (0x0015A094
on the struct) is 0 or the timer read 0x0015A022(PTR_DAT_00159F30) exceeds the per-range
threshold table (object+0x70 indexed by byte PTR_DAT_00159F40); fires the pair-reset action
PTR_FUN_00159F48 (→ 0x158C30 family) and returns 1. Cluster of 1. [I]

#### 0x00159F4C `park_chk_d_state_timeout` — logic
Runs 0x00159D54 (update B), then the predicate for monitor 0x156C1A: true when flag
PTR_DAT_0015A1A0 == 5 (D) and the second counter (0x0015A094 on struct+0x0C) is 0; fires the
pair-reset action PTR_FUN_0015A1A8 (→ 0x158C30 family) and returns 1. Cluster of 1. [I]

#### 0x00159F84 `park_flag3c_post_step_clr` — logic
Advances a sub-state machine: 0x0015A0F6(param+0x24, param+0x1E) walks an object (vtable call
`(**(obj+0x14))(obj + *(obj+0x0C))` then a follow-up via 0x0015A11C), then clears the flag
pair +0x3C/+0x3D if flag +0x3C is 1 and the sub-state byte +0x1E is nonzero. Called from
0x00159A4A. Cluster of 1. [I]

#### 0x0015A094 `ram_read32_celltest_inv` — library
Validated u32 read for safety RAM: writes walking test patterns (0, 0xFFFFFFFF, and the two
constants DAT_0015A1B4/DAT_0015A1B8) into the cell and reads each back — any mismatch calls
PTR_FUN_0015A1AC (→ fatal handler 0x001597C8, event class 0x19 sub 1); restores the value and
checks `*p == ~p[1]`, mismatch → PTR_FUN_0015A1B0 (→ 0x00159804, sub 2). Returns the value.
Companion of the byte/u16 variants 0x1596CA/0x159FBC/0x0015A022 (those belong to unit 46).
Cluster of 1. [O]

#### 0x0015A0E8 `get_validated_byte_flag` — data-accessor
Registered getter (callers='-', so reached through a function-pointer table): returns
FUN_00159FBC(PTR_DAT_0015A1BC) — a RAM-self-test-validated state byte from a global slot.
Cluster of 1. [O]

#### 0x0015A458 `canmon_group_tbl_init` — init
Builds the CAN communication monitor's per-message fault-group RAM table (3 bytes per entry,
base via PTR_DAT_0015A64C = the `0xFFF85289` table of spec 13 §2c): ~53 stride-3 records,
each byte copied from a distinct pointer chain (PTR_PTR_0015A648–0015A678). Called from the
init-table walker 0x0009A89C. Spec cross-ref: spec 13 §2c (53 monitored messages). Cluster
of 1. [O]

#### 0x0015A67C `init_safety_ram_pairs` — init
Zero-initialises the safety-layer RAM block: ~70 value/complement pairs (0 with 0xFFFFFFFF)
across 0xFFF8E71C–0xFFF8EF00 plus assorted single bytes, then registers two objects (calls
through PTR_FUN_0015A864/PTR_FUN_0015A868 with &DAT_fff8ef55 and &DAT_fff96042, resolved
0x0013238C/0x00151C9C). Reached via init/pointer table (callers='-'). This is the power-up
reset of the range/park safety layer's protected state. Cluster of 1. [O]

#### 0x0015B010 `init_monitor_area_reg20` — init
Initialises the monitoring bookkeeping area at 0xFFF95FFC (immediately after the program-flow
monitor object 0xFFF95FE8, spec 13 §3): helper(&DAT_fff95ffc), sets DAT_fff960b6=5,
DAT_fff960c0=0xFF, DAT_fff960c4=1, then registers twenty descriptor objects
(PTR_DAT_001539C8–PTR_DAT_00153A58) through per-object helper calls (PTR_FUN_00153A60…
PTR_FUN_00153AA4). Callers='-' (construction table). [I] on the area's role. Cluster of 1.

#### 0x0015B0A4 `init_obj_fff899d0_vtable` — init
Construction-table entry (callers='-'): helper(&DAT_fff899a0) (resolved 0x000AFB00), then
installs eight handler pointers (thunks PTR_LAB_0015B0F8–0015B114, immediately following this
function) into 0xFFF899D0/D4/D8/E0/DC/E8/E4/EC of the object at 0xFFF899A0 — a dispatch/
callback table fill. Cluster of 1. [O]

#### 0x0015B258 `init_obj_fff89da0` — init
Minimal static constructor (callers='-'): registers the block at 0xFFF89D9C with helper
0x000BB8D4, zeroes 0xFFF89DA0/A4/A8, returns &0xFFF89DA0. Part of the 0x15Bxxx constructor
battery. Cluster of 1. [O]

#### 0x0015B274 `init_obj_fff89e20` — init
Static constructor: registers nine sub-blocks (0xFFF89DAC, 0xFFF89DBC, 0xFFF89DC4/C8, D0, E4,
0xFFF8E228, 0xFFF89DF8, 0xFFF89E00) with helpers in the 0xF6xxx constructor support region,
zeroes the 0xFFF89E20–E38 object, installs code pointer PTR_LAB_000F6B20 at 0xFFF89E2C and
calls PTR_FUN_000F6B24(0xFFF89E38) and (0, 0xFFF89E3C); returns &0xFFF89E20. Nine resolved
callers in the 0xF6xxx region use the object. Cluster of 1. [O]

#### 0x0015B300 `init_obj_fff89ec4` — init
Static constructor: registers 0xFFF89EAC (helper 0x000BC686), then fills twelve u16 slots
0xFFF89EC4–EDA with 0xFFFF (invalid-marker reset); returns &0xFFF89EC4. Cluster of 1. [O]

#### 0x0015BD60 `init_obj_fff8ae58` — init
Static constructor: registers 0xFFF8AE38 (helper 0x001042F4), copies DAT_001042C0 into
0xFFF8AE6C, sets 0xFFF8AE58=0xFF and zeroes the remaining bytes; returns &0xFFF8AE58.
Cluster of 1. [O]

#### 0x0015BE2C `init_obj_fff8e6be` — init
Minimal static constructor: helper(&DAT_fff8e6bd) (resolved 0x000D7C24), returns
&DAT_fff8e6be — registers/returns one byte-sized object slot. Cluster of 1. [O]

#### 0x0015BFE8 `init_obj_fff8b27c` — init
Static constructor: registers 0xFFF8B274 (helper 0x000DB620), installs code pointer
PTR_LAB_0010CDAC at 0xFFF8B27C, zeroes 0xFFF8B280; returns &0xFFF8B27C. The object sits just
past the actuator-object block 0xFFF8B154–0xFFF8B25C built by the neighbouring initialiser
0x15BF2C (spec 12 §1.2) — likely an auxiliary output-path object of the same module. Cluster
of 1. [I] (module attribution)

#### 0x0015C064 `init_objs5_fff8b7c8` — init
Creates five sub-objects via helper 0x000E3710 at 0xFFF8B7C8, 0xFFF8B828, 0xFFF8B888,
0xFFF8B8E8, 0xFFF8B948 (96-byte stride), each with an id 1–5 and a parameter pointer
(PTR_DAT_0015C0A4–B8). Callers='-' (construction table). Cluster of 1. [O]

#### 0x0015C0BC `init_obj_fff8b9c8` — init
Static constructor: registers 0xFFF8B9A8 and 0xFFF8B9B8 with helper 0x000E53B8, installs code
pointer PTR_LAB_000E564C at 0xFFF8B9D4 and sets byte 0xFFF8B9CF=1; returns &0xFFF8B9C8.
Cluster of 1. [O]

#### 0x0015C13C `init_objs5_fff8c22c` — init
Creates five sub-objects via helper 0x000EFD76 at 0xFFF8C22C/248/264/280/29C (28-byte
stride), ids 1–5, parameter pointers PTR_DAT_0015C17C–190. Same shape as 0x0015C064 but a
different class. Cluster of 1. [O]

#### 0x0015C480 `init_obj_fff8c718` — init
Static constructor: registers 0xFFF8C6EC (helper 0x000F45FC); resets 0xFFF8C718–784 to
complement pairs (0xFFFF/0, 0xFF/0), one special value 0xFFF8C730=0xF9, and copies
DAT_000F4710 into 0xFFF8C734; returns &0xFFF8C718. The 0/0xFFFF and 0/0xFF pairs mirror the
safety-RAM coding used by the range/park state structs. Cluster of 1. [O]

#### 0x0015C49C `init_obj_fff8c7d4` — init
Static constructor: registers 0xFFF8E47E (helper 0x000F5620), zeroes 0xFFF8C7D4–E0 and seeds
0xFFF8C7E0–F4 with the four-word pattern DAT_000F5B08/DAT_000F5B0C; returns &0xFFF8C7D4.
Cluster of 1. [O]

#### 0x0015C6C8 `init_obj_fff8e6ea` — init
Static constructor: registers five blocks — 0xFFF8CB40 (helper 0x000F644C family),
0xFFF8CB94, 0xFFF8E490, 0xFFF8CC14, 0xFFF8E6E9 — through helpers resolved to 0x11A670/
0x11AF42/0x11AD58/0x11A9AA; returns &DAT_fff8e6ea. Cluster of 1. [O]

#### 0x0015C714 `init_obj_fff8cc7c` — init
Static constructor: registers two sub-objects (0xFFF8CC3C id 1, 0xFFF8CC68 id 2) and
initialises a float parameter slot &DAT_fff8cc50 with 0x3F800000 (1.0f); zeroes the object at
0xFFF8CC7C, installs code pointer PTR_LAB_0011D0EC and a second float 0x40000000 (2.0f) at
0xFFF8CC8C. The float constants suggest a gain/scaling object. Cluster of 1. [O]

#### 0x0015C74C `init_obj_fff8e4a0` — init
Minimal static constructor: registers 0xFFF8E49E (helper 0x000F7074), sets the two bytes
0xFFF8E4A0/A1 to 0xFF; returns &0xFFF8E4A0. Cluster of 1. [O]

#### 0x0015CA9E `unknown_pack4_words` — unknown
Seven instructions, no resolved callers or callees (pointer/vector reached): fills a 4-word
record `param_2 = {incoming r1, param_4, 0, *param_1}` — a small descriptor/queue-node build.
Purpose and owner module could not be determined; sits between the constructor batteries at
0x15C7xx and 0x15CECx. Cluster of 1. [I]

#### 0x0015CEC0 `init_objs3_fff8d0dc` — init
Creates three sub-objects via helper 0x0010D010 at 0xFFF8D0DC, 0xFFF8D148, 0xFFF8D1B4
(108-byte stride) with ids 2, 3, 4. Construction-table entry (callers='-'). Cluster of 1. [O]

#### 0x0015CF08 `init_objs_fff8d298_45_46` — init
Registers two objects with numeric ids: helper(&DAT_fff8d298, 0x45) and
helper(&DAT_fff8d29c, 0x46) (resolved 0x001189B8). The ids 0x45/0x46 (69/70) look like
module/message indices. Cluster of 1. [O]

#### 0x0015CF12 `init_obj_fff8d29c_46` — init
Single registration helper(&DAT_fff8d29c, 0x46) — exactly the second call of 0x0015CF08,
kept as a separate construction-table entry (id 0x46 only). Cluster of 1. [O]

#### 0x0015CF46 `init_objs_fff8d2dc` — init
Constructor-delegate (calls through the held constructor register unaff_r14, resolved
0x00126BF0/0x00126C9E): registers four objects ids 2–5 at 0xFFF8D2DC/2EC/2FC/30C, then five
float-parameterised objects at 0xFFF8D31C–36C with constants 0x3F800000 (1.0f), 0x40000000
(2.0f) and three pool constants — a parameterised-filter family. Cluster of 1. [O]

#### 0x0015CFF4 `init_obj_fff8d380` — init
Static constructor: registers 0xFFF8E4D8 (helper 0x001197E4), zeroes 0xFFF8D380/84/88;
returns &0xFFF8D380. Cluster of 1. [O]

#### 0x0015D0F0 `init_obj_fff8d444` — init
Static constructor: registers 0xFFF8E6F9 (helper 0x0011C540), zeroes 0xFFF8D444; returns
&0xFFF8D444. Cluster of 1. [O]

#### 0x0015D10C `init_obj_fff8d470` — init
Static constructor: registers 0xFFF8D448 and 0xFFF8D454 with helpers 0x0011C8F0/0x0011CB90,
then seeds the nine-slot table 0xFFF8D470–490 from the two words behind PTR_DAT_0015D158
(slots filled with `*(p+4)`; slots 0 and 8 with `*p`). Cluster of 1. [O]

#### 0x0015D15C `init_obj_fff8d494` — init
Static constructor: registers 0xFFF8E4E4 (helper 0x0011D340), copies the word behind
PTR_DAT_0011D440 into 0xFFF8D494 and 0xFFF8D498; returns &0xFFF8D494. Cluster of 1. [O]

#### 0x0015D1D0 `init_obj_fff8d4e4` — init
Static constructor: registers 0xFFF8E4E8 (helper 0x00125D30), zeroes 0xFFF8D4E4/E8 and
0xFFF8D4F0, copies DAT_00125F50 into 0xFFF8D4EC; returns &0xFFF8D4E4. Cluster of 1. [O]

#### 0x0015D1EC `init_obj_fff8d4f8` — init
Static constructor: registers 0xFFF8D4F4 (helper 0x0012616C), zeroes 0xFFF8D4F8/FC; returns
&0xFFF8D4F8. Cluster of 1. [O]

#### 0x0015D578 `init_obj_fff8d56c` — init
Static constructor: registers 0xFFF8D564 (helper 0x00126F34), zeroes 0xFFF8D56C–578;
returns &0xFFF8D56C. Cluster of 1. [O]

#### 0x0015D5A4 `init_obj_fff8d580` — init
Static constructor: registers 0xFFF8E718 (helper 0x001274F4), zeroes 0xFFF8D580; returns
&0xFFF8D580. Cluster of 1. [O]

#### 0x0015D5C0 `init_obj_fff8d594` — init
Static constructor: registers 0xFFF8D584 (helper 0x0012D848), then fills 0xFFF8D594–5B8 from
the constant pool 0x0012DEA8/DEAC/DEB0 (pattern of copied defaults plus zeroes); returns
&0xFFF8D594. Cluster of 1. [O]

#### 0x0015D5DC `init_obj_pair_e4ee_d5bc` — init
Construction-table tail entry: two registration calls — helper(&DAT_fff8e4ee) (resolved
0x0012E4BA) and helper(&DAT_fff8d5bc) (0x0012E350). Cluster of 1. [O]

## Unit summary

55 clusters, 55 names (every cluster single-member).

| Kind | Count |
|---|---|
| logic | 18 |
| init | 32 |
| stub | 2 |
| data-accessor | 1 |
| library | 1 |
| unknown | 1 |

**unknown entries:** 0x0015CA9E (`unknown_pack4_words`).

**Semantic anchors used:** range codes 5=D/6=N/7=R/8=P from spec 14 §5a; safety layer around
the redundant park check 0x158C3E (spec 14 §6); CAN comm-monitor group table (spec 13 §2c);
actuator-object block 0xFFF8B154–0xFFF8B25C (spec 12 §1.2); program-flow monitor object
0xFFF95FE8 (spec 13 §3).

**Cross-region leads:**
- The fatal RAM-integrity handlers 0x1597C8/0x159804 are also invoked from the byte/u16
  validated-read helpers 0x1596CA / 0x159FBC / 0x0015A022 (unit owning 0x158xxx–0x1594xx) —
  that unit should document them as its error path.
- The 8-arg internal event reports (class 0x18/0x19) resolve into 0x000A7B36, 0x000A105E,
  0x000A107C (r0c_app) — the report-entry family of spec 13 §2a; the unit owning 0xA7B36
  should confirm what "class 0x18/0x19, sub N" maps to in the fault-event space.
- Monitor wrappers 0x155E2C–0x1572BE (r14_app, earlier unit) call this unit's predicates;
  their helpers 0x1564AC (dual-table slot getter, sentinel 0xDEAC) and 0x1561F0 (report +
  infinite-loop halt) live there and deserve their own entries.
- The constructor battery's real constructors are in r08_app/r0c_app/r10_app (0xAFB00,
  0xBB8D4, 0xE3710, 0xEFD76, 0xF6xxx, 0x10D010, 0x1189B8, 0x126BF0, 0x12Exxx); 0x15BF2C
  (actuator objects, spec 12 §1.2) is the neighbouring unit's entry in the same table.
