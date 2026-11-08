# Unit 17 (r08_app) — selector/park-range application module (0x000A46CA–0x000A79D6)

Context established up front (applies to most entries): this slice is one coherent
**park/range + PRND-indication module** matching **spec 14** (park target arbitration) plus
its display/indication layer. Lever codes are **5=D, 6=N, 7=R, 8=P** [O, spec 14 §4.1].
Persistent values use (value, ~value) inverted-copy pairs. Scheduler entries: init
`0x9B228`, cyclic `0x9B306/0x9B322/0x9B34A/0x9B39E` (plus `0x9B63E…0x9B672` for the ring
buffer), all in r08_app. Tiny getters are consumed across regions (r0c/r0e/r10/r14).

#### 0x000A46CA `iface_get_u32_473c` — data-accessor
Returns the 32-bit word pointed to by ROM pointer slot `0x000A473C`. Part of a 9-slot
pointer table (0x000A473C–0x000A475C) read by measurement consumers `0x7B6D8` (r04_boot),
`0xF4E54` (r0c), `0x133C4E/0x134460` (r10, which stores results as floats at obj+0x2C…+0x50)
and `0x14FA0C/0x14FC5C` (r14). Singleton cluster. [I]
Cross-ref: spec 07 §7 / spec 09 input-interface style getter.

#### 0x000A46D0 `iface_get_u32_4740` — data-accessor
Same generated-getter pattern, slot `0x000A4740`; same cross-region callers. Singleton. [O]

#### 0x000A46D6 `iface_get_u32_4744` — data-accessor
Same pattern, slot `0x000A4744`; callers `0x7B6D8`, `0x14FC5C`, `0xF4E54`. Singleton. [O]

#### 0x000A46DC `iface_get_u32_4748` — data-accessor
Same pattern, slot `0x000A4748`. Singleton. [O]

#### 0x000A46E2 `iface_get_u32_474c` — data-accessor
Same pattern, slot `0x000A474C`. Singleton. [O]

#### 0x000A46E8 `iface_get_u32_4750` — data-accessor
Same pattern, slot `0x000A4750`; callers `0x7B6D8`, `0xF4E54`. Singleton. [O]

#### 0x000A46EE `iface_get_u32_4754` — data-accessor
Same pattern, slot `0x000A4754`. Singleton. [O]

#### 0x000A46F4 `iface_get_u32_4758` — data-accessor
Same pattern, slot `0x000A4758`. Singleton. [O]

#### 0x000A46FA `iface_get_u32_475c` — data-accessor
Same pattern, slot `0x000A475C` (last of the 9). Singleton. [O]

#### 0x000A4764 `iface_get_u32_4980` — data-accessor
Returns u32 through pointer slot `0x000A4980`; second getter bank begins here
(0x000A4980–0x000A49BC). Callers `0xA81D8`, `0xB912C` (the ~120-input interface copy that
fills the converter/park block `0xFFF89C3C…`, spec 09 §3.2). Singleton. [O]

#### 0x000A476A `iface_get_u8_4984` — data-accessor
Byte getter through slot `0x000A4984`; callers `0x1534FC`, `0xB912C`. Singleton. [O]

#### 0x000A4772 `iface_get_u8_4988` — data-accessor
Byte getter through slot `0x000A4988`; caller `0xB912C`. Singleton. [O]

#### 0x000A477A `iface_get_s8_498c` — data-accessor
Sign-extended byte through slot `0x000A498C`; caller `0xA81D8` (fills park input struct
fields +0x7C…+0x81, spec 14 §1). Singleton. [O]

#### 0x000A4780 `iface_get_u8_4990` — data-accessor
Byte getter through slot `0x000A4990`; caller `0xA7C9C` (park input-struct copy, spec 14 §2).
Singleton. [O]

#### 0x000A4788 `iface_get_s16_4994` — data-accessor
Signed 16-bit through slot `0x000A4994`; callers `0xA7C9C`, `0xA81D8`, `0xB912C`. Singleton. [O]

#### 0x000A478E `iface_get_u8_4998` — data-accessor
Byte getter through slot `0x000A4998`; callers `0xA7C9C`, `0xB912C`. Singleton. [O]

#### 0x000A4796 `iface_get_u8_499c` — data-accessor
Byte getter through slot `0x000A499C`; caller `0xA7C9C`. Singleton. [O]

#### 0x000A479E `vcall_49a4_bool` — stub
Calls function-pointer slot `DAT_000A49A4` with context pointer `PTR_DAT_000A49A0` and
returns the byte result (resolved target `0xC24D2`, which bumps a per-slot counter in the
struct at `PTR_DAT_000C262C` and returns a comparison flag). Caller `0x139EF2` (r10) builds
5-slot status arrays — a supervision hook wrapper. Singleton. [I]

#### 0x000A47AC `vcall_49a8_bool` — stub
Same vtable-call wrapper, slot `PTR_FUN_000A49A8` (resolved `0xC24EC`), ctx
`0x000A49A0`; caller `0x139FB4` (r10). Singleton. [I]

#### 0x000A47BA `vcall_49ac_u16` — stub
Same wrapper, slot `PTR_FUN_000A49AC` (resolved `0xC250C`, computes a 16-bit delta from
`DAT_000C2634/PTR_DAT_000C2638 + 0x88`); caller `0x13A09E` (r10). Singleton. [I]

#### 0x000A47C8 `vcall_49b0_1arg` — stub
Same wrapper but forwards one byte argument, slot `DAT_000A49B0` (resolved `0xC2530`,
increments counter [3] and calls `0xC263C` when arg bits 1/2 set); caller `0x13A022` (r10).
Singleton. [I]

#### 0x000A47D8 `iface_get_u8_49b4` — data-accessor
Byte getter through slot `0x000A49B4`; caller `0xB912C`. Singleton. [O]

#### 0x000A47E0 `iface_get_u8_49b8` — data-accessor
Byte getter through slot `0x000A49B8`; caller `0xB912C`. Singleton. [O]

#### 0x000A47E8 `iface_get_u32_49bc` — data-accessor
u32 getter through slot `0x000A49BC` (last of bank 2); caller `0xA7C9C`. Singleton. [O]

#### 0x000A4806 `vcall_49d0_chk_flag` — stub
Calls slot `PTR_FUN_000A49D0` with ctx object `PTR_DAT_000A49C0`, then tests ctx[10]
(Ghidra reduced the branch); resolved target `0xC20BA` is a bounded retry/backoff stepper on
the ctx object (counters at +8/+0xA/+0xC, flags +8/+0xA/+0xB). No direct callers — reached
via a task pointer table. Singleton. [I]

#### 0x000A48F0 `vcall_4a10_if_flag` — stub
If ctx object `PTR_DAT_000A49C0[10]` is set, calls `PTR_FUN_000A4A0C()` first; then always
calls `PTR_FUN_000A4A10(ctx)` (resolved `0xC211A`). Two-service stepper shell, no direct
callers (task-table reached). Singleton. [I]

#### 0x000A4906 `vcall_4a14_chk_flag` — stub
Calls `PTR_FUN_000A4A14(ctx 0x000A49C0)` (resolved `0xC213E`) then tests ctx[9] (branch
reduced by Ghidra). Singleton. [I]

#### 0x000A4A88 `vcall_4ad0_if_flag` — stub
Second object instance of 0x000A48F0's pattern: flag at `PTR_DAT_000A4AAC[9]`, optional call
`PTR_FUN_000A4ACC`, then `PTR_FUN_000A4AD0(ctx)` (resolved `0xC2166`). Singleton. [I]

#### 0x000A4AA4 `iface_get_u8_4ad4` — data-accessor
Byte getter through slot `0x000A4AD4`; caller `0x144108` (r14). Singleton. [O]

#### 0x000A4B74 `lib_hang_loop` — stub
Empty infinite loop (`do {} while(true)`), the shared trap for callers `0xA4B44/0xA4B50/
0xA4B5C/0xA4B68` — error/else branches of nearby logic. Singleton. [O]

#### 0x000A4B80 `zero3_thru_4bec` — library
Writes 0 to three bytes at `**(undefined**)PTR_DAT_000A4BEC + 0..2` (zeroes the head of the
object referenced by the slot pair). Called by orchestrator `0xA4DEA`. Singleton. [O]

#### 0x000A4BF4 `class_reg_ctor_4ea8` — logic
Constructor/registration for a small object framework: optionally allocates 0xC bytes via
`0x9AA34(0xC)`, installs vtable `PTR_LAB_000A4EA8`, zeroes the object and appends it to the
registry array at `DAT_000A4EAC` (max 0x28/40 entries, count at registry+0xA2). No direct
callers — framework objects are created at init through pointer tables. Singleton. [O]

#### 0x000A4C3C `class_dispatch_cb` — logic
Framework dispatcher: selects a slot at `*param_2 + 0xC` or `+ 0x18` by `param_3`, calls the
object's vtable+0x20 method with ctx offset +0x18 to get a byte, then invokes the function
pointer stored at slot+8 with (param_2 + slot_value, result). Indirect jump not recovered by
Ghidra. Called by wrapper `0xA4D3E`. Singleton. [O]

#### 0x000A4C6C `class_update_result` — logic
Calls the object's vtable+0x50 method (ctx offset +0x48), stores the u16 result at obj+2,
clears state bytes obj+0 and obj+4. Invoked for every registered object by the iterator
`0xA4DEA`. Singleton. [O]

#### 0x000A4C88 `class_call_m74` — stub
Single vtable call: method at `*(obj+8) + 0x74` with ctx `*(obj+8) + 0x6C + obj` (Ghidra
could not recover the jump table). Called by `0xA4E66` for each registry object. Singleton. [O]

#### 0x000A4C96 `class_sm_step` — logic
Per-object state machine step: reads input method (vtable+0x14) and busy flag (vtable+0x2C);
when idle, refreshes the u16 at obj+2 via vtable+0x50 and clears obj+4. When active and
timer `PTR_FUN_000A4EB0()` < the vtable+0x38 limit, counts the short at obj+2 down from its
loaded value, driving obj+0 through 0→1→2 and setting done-flag obj+4. Called by `0xA7734`
consumers via pointer tables (callers list shows `0xA7734` only through the getter table).
Singleton. [O]

#### 0x000A4D3E `class_dispatch_cb_1` — stub
4-instr wrapper: invokes `0xA4C3C(param_1, param_1+0xC, param_2)` — dispatch on the object's
first slot pair. No direct callers (pointer-table reached). Singleton. [O]

#### 0x000A4D46 `tbl_lut_bool_cfg18` — logic
Gets an index byte via the object's vtable+0x20 method (ctx offset +0x18) and returns
`config[index] == 1` from the byte table at `*(PTR_DAT_000A4EB4 + 0x18)`. Lookup consumer of
the framework's decoded index. No direct callers. Singleton. [I]

#### 0x000A4D6A `tbl_lut_s16_cfg4` — logic
Same index fetch; returns the signed word at `*(PTR_DAT_000A4EB4 + 4)[index*2]`. Singleton. [I]

#### 0x000A4D8A `tbl_lut_u8_cfg14` — logic
Same index fetch; returns byte at `*(PTR_DAT_000A4EB4 + 0x14)[index]`. Cluster of 2 — member
`0x000A4DCA` differs only in the table pointer (config+0x10). No direct callers. [I]

#### 0x000A4DAA `tbl_lut_s16_cfg0` — logic
Same index fetch; returns signed word at `*(PTR_DAT_000A4EB4)[index*2]`. Singleton. [I]

#### 0x000A4DEA `class_iter_update_all` — logic
Service entry: calls `PTR_FUN_000A4EB8()` and `PTR_FUN_000A4EBC()` once, zeroes the registry
cursor (DAT_000A4EAC+0x28), then for every registered object (count at +0xA2, index cursor
capped at 0x28) calls `0xA4C6C` to refresh its result field. Called by init `0x9B228`;
singleton. [O]

#### 0x000A4E52 `class_service_all` — logic
Periodic service: `PTR_FUN_000A4EC0()`, then `0xA4E66(param_1)` (iterate + state-2 callback),
then tail thunk `0xC2CBE` (r0c). Called by cyclic `0x9B374`. Singleton. [O]

#### 0x000A4E66 `class_iter_call74` — logic
Iterates the registry like `0xA4DEA`: calls `0xA4C88(obj)` (vtable+0x74 method) per object
and, when obj state == 2, invokes callback `PTR_FUN_000A4F04(param_1, obj)`. Called by
`0xA4E52`. Singleton. [O]

#### 0x000A4F0C `park_req_conditions_ok` — logic
Condition gate: sets `*param_1` = 1 only when a cascade of pointer-table predicates holds —
lever-request object `+0x00` value 1 (via `**PTR_DAT_000A4F80`), an OR over slot `0x000A4F84`
and object byte [1] + `PTR_FUN_000A4F88`, a `PTR_FUN_000A4F8C` == 8 check (P position),
`PTR_FUN_000A4F90` == 0 and `PTR_FUN_000A49A4-family` == 1, plus struct flags
`PTR_DAT_000A4F98[0x34]`/`[0x17]` clear; finally defers to `PTR_FUN_000A4F9C`. Called by
cyclic `0x9B374` (park/range module task). [I] Spec 14 context: P-release condition bundle.

#### 0x000A4FA0 `prnd_out_init` — logic
Initialises the 5-byte output record of the PRND evaluation module: `[0]=7, [1]=0xF,
[2]=0xF, [4]=0xF` (0xF = invalid range code, 7 = undefined internal mode per `0xA50C8`'s
else-branch). Called by init `0x9B228`. Singleton. [I]

#### 0x000A4FAE `prnd_eval_stages` — stub
Sequential driver of the PRND evaluation: `0xA4FC4` (map lever to target pair), `0xA50C8`
(lever→mode), `0xA5116` (lever-change flag), `0xA512C` (range validation). Called by cyclic
`0x9B39E`. Cluster of 5 — copies at `0xC5550`, `0xC5886`, `0xE0760`, `0x100C42` are the same
4-stage driver pattern in other modules. [O]

#### 0x000A4FC4 `prnd_map_lever_range` — logic
Maps lever position (`PTR_FUN_000A51A0`, values 5-8) with validity (`PTR_FUN_000A51A8`) and
signal states `PTR_DAT_000A519C[0x7C/0x7D/0x7F]` to the two output bytes param+1/param+2:
lever 8(P)→0xD, 7(R)→0xB, 6(N)→0, 5(D)→gear clamped 1..8 via `0xA50A0/0xA50B4` (with the
`PTR_FUN_000A51AC` manual-mode qualifier), else 0xF invalid. Overrides from table
`PTR_DAT_000A51B0` slots +8/+0xC when set. Called by `0xA4FAE`. [I] Spec 14 §4.1 codes.

#### 0x000A50A0 `prnd_clamp_gear` — library
Clamps a char to 1..8, else returns 0xF (invalid). Cluster of 2 (0xA50B4 identical twin).
Called by `0xA4FC4` twice (current and target). [O]

#### 0x000A50C8 `prnd_lever_to_mode` — logic
Maps lever position to an internal mode byte stored at param+0: 8(P)→0, 7(R)→1, 6(N)→2,
5(D)→4, else 7 (undefined). Second source `PTR_FUN_000A51B8()==2` substitutes the lever
getter. Called by `0xA4FAE`. [I]

#### 0x000A5116 `prnd_detect_lever_change` — logic
Sets param+3 = (`PTR_DAT_000A519C[0x7C] != [0x7D]`) — the raw lever signal pair differs,
i.e. a selection change is in progress. Called by `0xA4FAE`. Singleton. [I]

#### 0x000A512C `prnd_range_validate` — logic
Range-code validation cascade for param+4: uses state `*PTR_DAT_000A51BC` (0, 2/5 → check
manual qualifier `PTR_FUN_000A51AC` then clamp target/gear 1..8 via `0x7C/0x7F` states, on
out-of-range return the raw value), 4/6 → mode-7 special (returns 0xC), 1/3 or
change-in-progress → 0xF invalid. Called by `0xA4FAE`. [I] Spec 14 §4.1 code space.

#### 0x000A51E4 `park_lever_get_range` — data-accessor
Checked getter of lever-request object `0xFFF8304C` field +0x00: returns the value only if
`*p == ~p[1]`, else 0 (5=D,6=N,7=R,8=P, spec 14 §4.3). Heavily used by the arbiter family
and duplicated in the safety layer. Cluster of 2 — `0x158AE4` is the safety-layer copy
(spec 14 §6). [O] Spec 14 §4.3.

#### 0x000A51F4 `park_lever_get_req2` — data-accessor
Checked getter, object field +0x08 (second request value), inv-pair at +8/+0xC. Cluster of 2
— `0x158AF4` safety-layer copy. [O] Spec 14 §4.3.

#### 0x000A5204 `park_lever_get_dir_ok` — data-accessor
Checked getter, object field +0x10 (0/1/2: request plausible with direction of travel).
Cluster of 4: `0xA5224` is the same pattern on field +0x20 (speed flag), `0x158B04/0x158B24`
are safety-layer copies on their own object. [O] Spec 14 §4.3/§6.

#### 0x000A5214 `park_lever_get_mismatch` — data-accessor
Checked getter, object field +0x18 (direction-mismatch flag, default 2). Cluster of 2 —
`0x158B14` safety-layer copy. [O] Spec 14 §4.3.

#### 0x000A5234 `park_lever_clear_status` — library
Clears the inv-pair status bytes at param+0x28/+0x29. Cluster of 4 — members `0xADEB8`,
`0xC3A76`, `0xD933C` are the same clear-inv-pair-bytes helper on other objects/regions.
[O] Spec 14 §4.3 (+0x28 status pair).

#### 0x000A5240 `park_lever_get_status` — data-accessor
Checked byte getter for the +0x28 status pair (returns 0 on complement mismatch). Cluster
of 2 — `0x158B34` safety-layer copy. [O] Spec 14 §4.3.

#### 0x000A525A `park_lever_cycle` — logic
Cycle entry for the lever object: resets +0x28/+0x29 via `0xA5234`; when
`*PTR_DAT_000A541C` is set, refreshes +0x10 from `0xA5204` and +0x18 from `0xA5214` (masked
to byte + complement) and clears the trigger. Then runs the four evaluators `0xA52A2`
(direction-plausibility +0x10), `0xA5352` (mismatch +0x18), `0xA53D4`, `0xA5448` (+0x20).
Called by cyclic `0x9B322` on the object at `PTR_DAT_000A5418`. [O] Spec 14 §4.3.

#### 0x000A52A2 `park_lever_calc_dir_ok` — logic
Writes object +0x10/+0x14: keeps an existing non-zero plausible value; else when lever
signal pair (`PTR_DAT_000A542C[0x2F]`, [0x30]) and the request table slot
(`*(PTR_DAT_000A5424 + 0x10)`) disagree with the lever `*PTR_DAT_000A5430` (5→1 with
override 0xFE, 7→2 with 0xFD) sets 1 or 2 (D-side / R-side); invalid when the message tick
`PTR_FUN_000A5420` is below the threshold at `*(PTR_DAT_000A5424+4)` or status != 2. Spec 14
cites this as one of the P1B13-relevant direction checks. [O] Spec 14 §4.1/§4.3.

#### 0x000A5352 `park_lever_calc_mismatch` — logic
Writes object +0x18/+0x1C (default 2): 2 when the request direction contradicts the lever
signal pair [0x2F]/[0x30] against the table slot +0x10, 1 when the lever (state [0x2F]=1
with lever 7 / state 2 with lever 5) matches R-side, 0 otherwise; gated on the same tick
threshold as `0xA52A2`. [O] Spec 14 §4.3.

#### 0x000A53D4 `park_lever_notify_req` — logic
Forwards the object state: `param_1==1` → call `PTR_PTR_000A543C(ctx, ctx, 1)` else
`PTR_PTR_000A5440(ctx, ctx, 0, 1)`; resets `*param_1` when `PTR_FUN_000A5444()` reports 1
(acknowledge). Consumers `0xA105E/0xA0E70` (r08). [I]

#### 0x000A5448 `park_lever_calc_speedflag` — logic
Writes object +0x20/+0x24 = 1 when `PTR_FUN_000A5684()` is 1 and tick
`PTR_FUN_000A567C()` exceeds the threshold at `*(PTR_DAT_000A5680 + 0xC)`, else 0 (with
complement). Spec 14: +0x20 = "D5F9=1 and D650 > 320" flag. [O] Spec 14 §4.3.

#### 0x000A548C `park_arbiter_store_target` — logic
Arbitration step 3 (spec 14 §1): runs the arbiter `0xA557C`, then writes the resulting park
target to the inv-pair at `PTR_DAT_000A568C[2]/[3]` (target `0xFFF8D6D6/7`) and calls
`PTR_FUN_000A5690()` (output notification). Called by init-cycle `0x9B228` path. [O]
Spec 14 §4.

#### 0x000A54A4 `park_arbiter_special_cases` — logic
Special cases of the arbiter (spec 14 §4.2 step 1-3 region): with calibration enables from
`PTR_DAT_000a5680` slots (+0x1C/+0x24/+0x28) and direction flags from the checked getters
(`0xA51F4`==5, `0xA5204`, `0xA5240`==1, `0xA5224`), sets param+1 = 5/6 with reason code
param+2 = 1/2/3; returns 1 when a special case fired, 0 otherwise (0 when speeds above the
`PTR_DAT_000A5694` threshold or `PTR_FUN_000A5698` set). [O] Spec 14 §4.2.

#### 0x000A557C `park_arbiter_compute` — logic
The park-target arbiter (spec 14 §4.2): tries `0xA54A4` special cases first; then forced
release 6 (reason 5) / forced release 6 (reason 6) / drive-related target 5 (reason 8) from
calibration enables +0x30/+0x34/+0x20 with the speed threshold `PTR_DAT_000A5694`; otherwise
dispatches on calibration mode `*(PTR_DAT_000a5680 + 0x2C)`: 0→`0xA56A4`, 1→`0xA56D8`,
2→`0xA56FA`, 3→`0xA5778`, else→`0xA57DA` (stock 4). Writes reason to param+2. [O] Spec 14 §4.2.

#### 0x000A56A4 `park_arbiter_mode0` — logic
Mode-0 target rule: output speed (`**(ushort**)PTR_DAT_000a5854`) below `*PTR_DAT_000a5850`
and lever request (`0xA51E4`) != 8(P) → target 6 / reason 0xA; else target 8 / reason 9.
[O] Spec 14 §4.2 step 5.

#### 0x000A56D8 `park_arbiter_mode1` — logic
Mode-1 rule using only `D5F8` (`PTR_FUN_000a585c`): valid park request → target 8 / reason
0xB; else target 6 / reason 0xC. [O] Spec 14 §4.2 step 5.

#### 0x000A56FA `park_arbiter_mode2` — logic
Mode-2 rule: `D5F8` → 8/0xD; else lever 7, 6 or 5 (R/N/D) → 6/0xE; else moving
(speed < threshold or state pair `PTR_DAT_000a5860[0x2E]/[0x38]` with lever != 8) → 6/0x10;
else 8/0xF. [O] Spec 14 §4.2 step 5.

#### 0x000A5778 `park_arbiter_mode3` — logic
Mode-3 rule: moving → 6/0x14; `D5F8` → 8/0x11; lever 7/6/5 → 6/0x12; else 8/0x13. [O]
Spec 14 §4.2 step 5.

#### 0x000A57DA `park_arbiter_mode_def` — logic
Default mode (stock, calibration +0x2C = 4): lever 8(P) or (stationary and valid `D5F8`) →
target 8 / reason 0x15; moving or no valid request → lever 7/6/5 or moving → 6/0x16, else
stationary without valid request → 8/0x17. [O] Spec 14 §4.2 step 5 (§4.2 listing).

#### 0x000A586C `park_store_variant_9c` — stub
Stores `0xA5884()` (variant predicate) into `*param_1`. Called by init `0x9B228`. Cluster of
2 (0xA5878 twin). [I]

#### 0x000A5884 `park_is_variant_9c` — logic
Returns 1 when `PTR_DAT_000a58b4 + 0x60` (u16) == 0x9C and byte [0x5D] ∈ {11, 6, 7} — an
equipment/market-variant test on the module's parameter object (shifter-type variants 6/7/11
appear in spec 14 §5a). Singleton. [I] Spec 14 §5a variant bytes.

#### 0x000A58B8 `task_call1_58b8` — stub
Single registered-callback invoker: `PTR_FUN_000A58CC(DAT_000A58C8)` (resolved `0xA82E0`);
part of the cyclic task `0x9B39E`. Cluster of 2 (0xA58C0 calls a different target). [O]

#### 0x000A58D4 `task_call4_58d4` — stub
Invokes four registered callbacks (`PTR_FUN_000A58FC/0x5904/0x590C/0x5914` with their ctx
slots; resolved targets 0xC64D4, 0xC29A0, 0xC8556, 0xC7D64 in r0c). Called by cyclic
`0x9B34A`. Singleton. [O]

#### 0x000A58F0 `task_call1_58f0` — stub
Single-callback invoker `PTR_FUN_000A5918(PTR_DAT_000A5908)` (resolved `0xC8528`); called
from init `0x9B228`. Singleton. [O]

#### 0x000A591C `task_call1_591c` — stub
`PTR_FUN_000A5938(DAT_000A5934)` (resolved `0xA740E`, ring-buffer refresh); called by cyclic
`0x9B322`. Cluster of 2 (0xA5924 twin via `0xA736A` thunk). [O]

#### 0x000A592C `task_call1_592c` — stub
`PTR_thunk_FUN_000a75c6_000a5940(DAT_000A5934)` — clears the ring-buffer entries via the
thunk at `0xA736A`; called by cyclic `0x9B322`. Singleton. [O]

#### 0x000A5944 `task_call4_5944` — stub
Four registered callbacks (resolved 0xC6E38, `0xA8EFE` derived-input evaluator on
`0xFFF8D5E6`, 0xC36FC, `0xA6A2C`); called by cyclic `0x9B322`. [O]

#### 0x000A5960 `task_call1_5960` — stub
`PTR_thunk_FUN_000a96fa_000a5998(PTR_DAT_000A5988)` (resolved `0xA8F34`); cyclic
`0x9B39E`. Singleton. [O]

#### 0x000A5968 `task_call2_5968` — stub
Two registered callbacks (resolved 0xC6E30, `0xA8EDC`); called from init `0x9B228`.
Singleton. [O]

#### 0x000A59A4 `task_call1_59a4` — stub
Single callback (resolved `0xC8038`); cyclic `0x9B34A`. Cluster of 3 (0x59C8, 0x59D0 twins
with own slots, one resolved `0xC5886`). [O]

#### 0x000A59C0 `task_call1_59c0` — stub
Single callback (resolved `0xC5886`); cyclic `0x9B34A`. See cluster above (shared entry with
0x59A4 twins). [O]

#### 0x000A59E8 `task_call5_59e8` — stub
Five registered callbacks (resolved 0xC7A14, 0xC360A, 0xC2754, 0xC3A7E, 0xC70CE); cyclic
`0x9B34A`. Singleton. [O]

#### 0x000A5A0A `task_call2_5a0a` — stub
Two registered callbacks (resolved 0xC3A76, 0xC3604); called from init `0x9B228`.
Singleton. [O]

#### 0x000A5A4C `task_call2_5a4c` — stub
Two registered callbacks (resolved 0xC5550, 0xC4F90); cyclic `0x9B34A`. Singleton. [O]

#### 0x000A5A70 `prnd_out_init2` — logic
Initialises the PRND output record's char pair: +0x10 = 0xFF (invalid display char), +0x11
complement 0, +0x13 = 0xFF; called by init-cycle `0x9B530`. Singleton. [O]

#### 0x000A5A82 `prnd_eval_main` — logic
Orchestrator of the PRND indication module (cyclic `0x9B546`): runs `0xA5AD8` (status
flags), `0xA5D50` (lever transitions), `0xA5EF0` (range state machine), `0xA6126`,
`0xA5BA2` (display char), `0xA5CC2` (status code), `0xA6166` (shift direction), then the
overall-validity flag to +0x22 and finally `0xA6384`, `0xA63C0`, `0xA63CA`, `0xA632E`.
[O]

#### 0x000A5AD8 `prnd_calc_status_flags` — logic
Clears flags +0x14..+0x17/+0x18, then from mode `PTR_FUN_000A5C78` (==5), state bits from
`*(PTR_DAT_000A5C7C + 0x50)` (bits 4/8/4/1), key state `*PTR_DAT_000A5C80` ∈ {6,0xC,0xE,0xA}
and `PTR_FUN_000A5C84` sets +0x18; +0x17 follows +0x18 when bit 1 clear. Early-outs when the
three gate predicates (0xC90-family) are not satisfied. [I]

#### 0x000A5BA2 `prnd_calc_display_char` — logic
Computes the ASCII range character into the inv-pair +0x10/+0x11: lever 8→'P'(0x50),
7→'R'(0x52), 6→'N'(0x4E), 5→'D'(0x44) with variants 'S'(0x53) when param obj [0x5D]==6 and
digit override (cVar5, '0'+n), 'L'(0x4C) when [0x5D]==7 with [0x55]==1, gear digit
('0'+(n&0xF)) paths; 0 → ' '(0x20), invalid → 'F'(0x46). Uses `PTR_FUN_000A5CA0` for the
lever and struct `PTR_DAT_000A5CA4[0x5D]/[0x55]`. [O] — matches spec 14 §4.1 P/R/N/D coding.

#### 0x000A5CC2 `prnd_calc_status_code` — logic
Priority decode of a 13-bit status word `PTR_FUN_000A5EAC()` (bits 0x1000, 0x100, 8, 1,
0x200, 0x40/0x20, 4) plus predicates `PTR_FUN_000A5EB0/0x5EB4` and `*PTR_DAT_000A5EB8`
into a single status byte at +0x13 (values 1..7, 0xB, 0xC, 0x40, 0xFB, 0xFC, 0xFE).
[I] — status/reason enumeration for the indication module.

#### 0x000A5D50 `prnd_calc_lever_transitions` — logic
One-shot latch (`DAT_fff893d0`, snapshot `DAT_fff8e516`): when the request-source object
`PTR_DAT_000A5EC0 + 0x2C` slot is 1, computes +0x21 (stable-position agreement of
`PTR_FUN_000A5EC4/0x5EC8` (==8) with fallback `0x5ECC` and slot +0x24), calls `0xA5E74`
(countdown), sets/clears the +0x1E engage flag from slots +0x28/& state [0x5E]/[0x81], and
computes +0x20 = rising/falling edge of `PTR_FUN_000A5EBC()` masked by slot +0x30 bits.
[O]

#### 0x000A5E74 `prnd_timer_step` — logic
Countdown on the short at param_1+0: states 4/5/6 (at param+0x1B) reload from
`*(short**)(*(PTR_DAT_000A5EC0 + 0x1C))` when the state changed; counts down and sets the
done bool at param+2; states 0..3 clear both. Called by `0xA5D50`. [O]

#### 0x000A5EF0 `prnd_sm_step` — logic
The module's main range state machine (state byte +0x1B, 0..6; previous at +0x1C): manages
countdown shorts +0xC/+8/+4 (done flags +0xE/+0xA/+6), transitions on the engage flag
+0x1E, safety-state `PTR_DAT_000A60EC[0x81]` (0..3), and position `*PTR_DAT_000A60F0`
(5/4/2/1 mirror of modes); sub-calls `0xA63F4`/`0xA645A` for the threshold timer. Resets
everything when the request-source slot +0x2C is 0. [O]

#### 0x000A6126 `prnd_calc_move_allowed` — logic
Sets +0x1A: allowed when the previous state byte +0x1D (0..3) matches the bit (1/2/4/8) in
the request mask `*(PTR_DAT_000a62f0 + 0x4C)`; also handles state `[0x59]==2` override.
Feeds `0xA6166`. [I]

#### 0x000A6166 `prnd_calc_shift_dir` — logic
Computes the shift-direction/reason byte +0x19 (0..7) from current lever (`PTR_FUN_000A62F4`,
values 5-8), previous lever (`PTR_FUN_000A62F8`) and manual-mode flags (struct
`PTR_DAT_000a6300[0x5D]/[0x55]`, `PTR_FUN_000A6304`, mask slot `0x6308+0x48` and
`PTR_DAT_000a630c`): P→R/N/D transitions map to 1/2/3, D-side up/down to 4/5/6, keep- gear
combinations to 0/7. [I] — "which way did the driver move the lever".

#### 0x000A632E `prnd_apply_display_override` — logic
When the request object `PTR_DAT_000a648c[1]` is 1: overrides the display char pair
+0x10/+0x11 from the external request bytes `*(PTR_DAT_000a6488 + 0x40/+0x44)` (0xFF = no
change) and +0x12 from slot +0x48 — bus-sourced display values take precedence over locally
computed ones. [O]

#### 0x000A6384 `prnd_calc_flag23` — logic
Sets +0x23 = 1 when `*PTR_DAT_000a6490` ∈ {2,3} and (`*PTR_DAT_000a6498` != 0xE or
`PTR_FUN_000a6494()` == 5). [I]

#### 0x000A63C0 `prnd_copy_flag_649c` — data-accessor
Copies `*PTR_DAT_000a649c` into +0x24. Singleton, 4 instrs. [O]

#### 0x000A63CA `prnd_calc_flag25` — logic
Sets +0x25: 2 when `*PTR_DAT_000a64a0` == 1, else 1 when `PTR_FUN_000a64a4()` has bit 0x200
set, else 0. [I]

#### 0x000A63F4 `prnd_load_threshold` — logic
Loads the countdown short at param+4 from one of three threshold slots of the request
object (`*(int*)PTR_DAT_000a6488`): index +0x8/+0xC (+0x10/+0x18 when the variant
predicate `PTR_FUN_000a64a8` set), selected by lever `PTR_FUN_000a64ac()` (8→first, 5/7→
second, 6→third); clears the done flag at +6. Called by `0xA5EF0`/`0xA645A`. [O]

#### 0x000A645A `prnd_threshold_timer` — logic
Counts down the short at param+4 (done flag at +6, -1 = disabled); reloads via `0xA63F4`
while the retrigger flag +0x20 (from `0xA5D50`) is set. Called by `0xA5EF0`. [O]

#### 0x000A64B0 `prnd_out2_init` — stub
Init default: sets param+6 = 0x7F (the "invalid/none" lever code used by `0xA6D6E`).
Called from init `0x9B228`. Cluster of 2 — `0x13C358` is the same init stub in r10. [O]

#### 0x000A64B6 `callpair_64b6` — stub
Two-in-sequence invoker: `0xA64C4` (switch-position decode) then `0xA6704` (target-2
state); called by cyclic `0x9B34A`. Cluster of 12 byte-identical 2-call wrappers whose
embedded call targets differ per copy: `0xA6A2C` (calls `0xA6A3E`+`0xA6B06` in this module),
the rest (`0xC589C`, `0xC8556`, `0xC9298`, `0xE94EE`, `0xEFD0C`, `0x106AB6`, `0x10A914`,
`0x10A922`, `0x134752`, `0x1394B6`) are the same shell in r0c/r0e/r10 modules. [O]

#### 0x000A64C4 `shiftpos_decode_lut` — logic
Switch-position decoder: index from `0xA6672` (lever 5-8 → 0-3, invalid codes 9/10/11 keep
the old), validity gates `PTR_FUN_000a66d8/0x66dc` (param[3]), table entry from
`*(PTR_DAT_000a66e0 + 0x18)[index*2]` drives param[1] (non-zero when flag byte set),
`0xA66A8` fills param[4], and param[0]/param[2] = (param[1] or param[4]) xor table byte 1.
[O]

#### 0x000A6544 `shiftpos_calc_plaus` — logic
Plausibility for the decoder: bit0/bit1 of slot `*(PTR_DAT_000a66e0 + 8)` combined with
lever `PTR_FUN_000a66e4` (==6/==8), the auxiliary condition `0xA65DA` (param+10), plus
freshness predicates `PTR_FUN_000a66e8/0x66ec/0x66f0` and lever comparisons — sets param+5.
[O]

#### 0x000A65DA `shiftpos_calc_cond_10` — logic
Sets param+10 = 1 when struct `PTR_DAT_000a66f8[0x17]` and `[0x34]` are both non-zero.
[I]

#### 0x000A6602 `shiftpos_calc_p_avail` — logic
Returns 1 (available) when the strict P-position bundle holds: both lever sources `PTR_FUN_
000a66f0/0x66e4` == 8 (P), source-valid `PTR_FUN_000a66e8` with `*PTR_DAT_000a66fc` == 1 and
slot `*(PTR_DAT_000a66e0 + 0x14)` == 1 — else falls back to the triple predicate. [I]

#### 0x000A6672 `shiftpos_get_index` — logic
Maps the lever to the decoder index: reads via `0xA66B4` (validity = code ∈ {9,10,11});
valid → keep previous (param+6), else index = code-5 clamped to 4 when > 8. [O]

#### 0x000A66A8 `shiftpos_calc_avail_copy` — stub
Stores `0xA65F6()` (availability predicate just below this unit's range) into param+4.
[O]

#### 0x000A66B4 `shiftpos_read_lever_valid` — logic
Reads lever code `PTR_FUN_000a66e4()` and flags validity 1 when the code is 9, 10 or 11
(the shifter's no-change/invalid codes, cf. spec 14 §5a decode table 0/6/error rows) —
returns both through the two out-bytes. [I] Spec 14 §5a.

#### 0x000A6704 `shiftpos_calc_state2` — logic
Under the request-source slot (selected by `*PTR_DAT_000a6794` into `PTR_DAT_000a6798`
+0xC threshold) and idle condition param+10: when the position table's first byte is 1 and
the lever (via `PTR_FUN_000a679c` slot call) is 6/5 with `PTR_FUN_000a67a0` != 2 and lever
`PTR_FUN_000a67a4` not 8/7, sets the u16 at param+8 = 2. [I]

#### 0x000A67A8 `didreg_sel_batch_a` — logic
Registers 54 data-identifier entries with the boot-side registry service `0x7A6E0`
(`(group, rom_table, 1)` triples): 25 pairs in group 1 (tables `0xC68450…/0xCE2570…`),
15 in group 2 (`0xC6AB60…/0xCE4C80…`), 2 in group 3 (`0xC6D270/0xCE7390`). Called by init
`0x9B2E8`. [O] Spec 13/15 DID machinery.

#### 0x000A6A04 `didreg_sel_batch_b` — logic
Registers two more group-2 DID entries (`0xC6AF48`, `0xCE5068`) via `0x7A6E0`; called by
init `0x9B530`. Singleton. [O]

#### 0x000A6A3A `thunk_6ab0` — stub
2-instruction tail-jump thunk to `0xA6AB0` (identical body); reached from the cyclic task
via `PTR_thunk_FUN_000a6ab0_0009b504`. Singleton. [O]

#### 0x000A6A3E `timer2_step_6b7c` — logic
Two independent up-counters (short +0/+1 flag, short +2/+3 flag, -1 = disabled): gate 1
`PTR_DAT_000a6b7c[0xC]`==1 with enable `*PTR_DAT_000a6b84`; result flag `*PTR_DAT_000a6b80`;
counter 2 runs only while [0xC]==1. Called via `0xA6A2C` in this module. [O]

#### 0x000A6AB0 `lever_cond_report` — logic
Reports a lever/vehicle-state tuple to a registered consumer `PTR_PTR_000a6b9c(ctx_a,
ctx_b, lever==2, moving)`: moving = request tick `**(ushort**)PTR_DAT_000a6b88` above
`*PTR_DAT_000a6b90` threshold; lever from `PTR_FUN_000a6b8c` on the same object. Body twin
of thunk target `0xA6A3A`. No direct callers (task-table reached). [I]

#### 0x000A6B06 `shiftlock_bit_control` — logic
Lever-transition-driven bit control on an output port accessed through `PTR_FUN_000a6cd4`
(read) / `0xA6CD8` (write): edge-detected states (snapshots `DAT_fff8e517/518` from
`PTR_FUN_000a6ba0` and `PTR_DAT_000a6ba8[8]`) set/clear bits 1 and 2 (e.g. on transition to
lever 8 = P, and while a state pair holds), full clear to 0 under the release states, and
forwards the engage condition to a callback pair (`PTR_PTR_000a6cec`) with timers at +8
reloaded from `*(PTR_DAT_000a6b88 + 4)`; also honours `PTR_DAT_000a6bac[0x22]/[0x43]`.
[I] — plausibly the P-display/interlock (shift-lock) output logic; exact port identity open.

#### 0x000A6CF0 `task_call1_6cf0` — stub
Single registered callback (resolved `0xC623E`); init `0x9B228`. Singleton. [O]

#### 0x000A6CF8 `task_call3_6cf8` — stub
Calls `0xA6D12` (shifter-mode calc) plus two registered callbacks (resolved 0xC60CE,
0xC6116); called from init `0x9B228` and cyclic `0x9B34A`. Singleton. [O]

#### 0x000A6D0C `shifter_mode_init3` — stub
Init default: `*param = 3` (the "invalid" shifter mode used by `0xA6D12`). No direct
callers (init table). Singleton. [O]

#### 0x000A6D12 `shifter_mode_calc` — logic
Maps the fused lever `0xA6DF4`/`0xA6D6E` result to the mode byte at *param: 8(P)→2, 6(N)→1,
0x7F(invalid)→3, else 0; overridden to 0 while slot `*(PTR_DAT_000a6e14 + 0x10)`==1 and
`*PTR_DAT_000a6e20` != 0. Also services two pointer-table calls first. [I]

#### 0x000A6D6E `shifter_lever_vote` — logic
Combines four lever sources — main `PTR_FUN_000a6e24`, qualifier `PTR_FUN_000a6e28`,
state `*PTR_DAT_000a6e2c`, selector `PTR_FUN_000a6e30` — into one code: when the selector
is 8 or 6, output 8/6 only if the qualifier/state/main checks pass (else 0); selector 7 → 7,
5 → 5; anything else 0x7F. Feeds `0xA6D12` (via `0xA6DF4`). [I]

#### 0x000A6E38 `prnd_digit_counters_step` — logic
When the main lever (`PTR_FUN_000a6eb8`) is 5 (D/manual): three BCD-digit counters
(param[0], [1], [2], each 0-9 with wrap) selected by `PTR_FUN_000a6ebc() & 3`; on wrap each
calls its overflow callback (`PTR_FUN_000a6ec0/0x6ec4/0x6ec8`). [I] — manual-gear digit
(e.g. tiptronic +/−) counting for the display.

#### 0x000A6ECC `did_read_sel_4306` — dispatch
DID read dispatcher for selector DIDs `0x4306`/`0x4325` (pointer+1 byte) and
`0x4308`/`0x4309`/`0x430A` (getter call, 4 bytes); unknown DID → 0 with zeroed out params.
Called by the UDS read path `0x9B672`. [O] Spec 15 DID service family.

#### 0x000A6F40 `did_read_sel_1389` — dispatch
DID read dispatcher for the module's local identifiers `0x1389…0x13A4` (contiguous): byte
results staged into `DAT_fff893dc` (byte/word packed by width, length 1/2/4 out-param),
sources are getters or RAM slots (`PTR_DAT_000a7078/0x707C/0x7084…`), `0x13A4` echoes
param_1. Called by `0x9B6AE`. Spec 13 §4.1 cites the P1B14 failure-byte DIDs `0x138F`
(`0xFFF8D648`) and `0x1390` (`0xFFF8D620`). [O] Spec 13 §4.1.

#### 0x000A70AA `initcall36_70aa` — dispatch
Init chain: invokes 36 registered callbacks in sequence (slots `PTR_FUN_000a71d4…0x7260`;
resolved targets all in r0c `0xC2738…0xC8914`). Called by `0x9B2AC`. Cluster of 2 —
`0xA713E` is a 37-call chain on slots `0x7264…0x72F0`. [O]

#### 0x000A72F4 `flagpair_set_1fe` — library
Writes the two-byte "changed" flag {1, 0xFE} (value + complement) — the standard
dirty-marker set used by the ring-buffer and display-pair writers. Cluster of 14 identical
copies: `0xA7660` (this module), `0xC23CA`, `0x125272`, `0x13852E`, `0x138D3C`, `0x13948A`,
`0x14003A`, `0x14115A`, `0x1418A8`, `0x1440D6`, `0x149B7A`, `0x14B3EE`, `0x14EB64` across
r0c/r12/r13/r14. [O]

#### 0x000A7326 `ring_buf_clear` — logic
Full reset of the checksum-guarded 8-entry ring buffer (see `0xA7430`): zeroes entries
region copy +0x10/+0x11, counters +0x16/+0x18 (inv pair), the four inv-pair dwords
+0x1C…+0x30, and marks the changed flag (`*PTR_DAT_000a7468 = {1,0xFE}`) — only while the
control byte `PTR_DAT_000a746c[0x18]` is 0. [O]

#### 0x000A736E `ring_buf_verify` — logic
Returns 1 only when both integrity checks pass: `0xA751A` (all inverted pairs) and
`0xA7598` (rolling sum of the 8 two-byte entries == stored +0x14). Called by cyclic
`0x9B63E`. Singleton. [O]

#### 0x000A738E `ring_buf_reset_if_set` — logic
If the buffer's changed-flag pair at `PTR_DAT_000a7468` is not {0,0xFF}, re-marks it
{1,0xFE} via `0xA72F4` and returns 1 (i.e. "had been modified"); else returns 0. Called by
cyclic `0x9B64C`. Singleton. [O]

#### 0x000A73B4 `ring_calc_age_44` — logic
Returns `((DAT_000A7470 - *PTR_DAT_000a7474) & 0xFFFF) + 0x44` — an age/ticks delta plus
constant 0x44. Called by cyclic `0x9B65A`. Meaning of the delta source is open. [I]

#### 0x000A73C0 `ring_evt_handle6` — logic
When event mask param_2 has bits 1/2 set: `0xA756C` (mark +0x34/+0x35 invalid) then
`0xA75C6` (clear all entries). Called by cyclic `0x9B668`. Cluster of 2 (`0x14EC64` copy in
r14). [O]

#### 0x000A73D6 `ring_buf_tick` — logic
Periodic maintenance (cyclic via thunk `0xA5924`/`0x9B322`): re-verify integrity
(`0xA751A`, `0xA7598`), repairing via `0xA756C`/`0xA75C6` when a check fails; while the
gating flag `*PTR_DAT_000a7464` is 0, increments the tick counter +0x16 with complement and
marks changed via `0xA72F4`. [O]

#### 0x000A740E `ring_buf_refresh` — logic
Integrity refresh only (same verify/repair pair as `0xA73D6`, without the tick counter).
Reached through `PTR_FUN_000a5938` from task `0x9B322`. Cluster of 3 (`0x14EC7A`,
`0x14EC9C` copies in r14). [O]

#### 0x000A7430 `ring_entry_update` — logic
Updates the ring slot at index `+0x12` (capped 8) with the (param_2, param_3) byte pair,
maintaining the rolling sum at +0x14 (subtract old pair, add new) and the entry counter;
marks the changed flag at `PTR_DAT_000a7634` {1,0xFE}; no-op while control byte
`PTR_DAT_000a746c[0x18]` is set or index 0xFF. Caller `0xA7B36` (this region). [O]

#### 0x000A751A `ring_check_inv_pairs` — logic
Verifies every inverted-copy pair of the ring object: u16 +0x16/+0x18, dwords +0x1C/+0x28,
+0x20/+0x2C, +0x24/+0x30, bytes +0x10/+0x11 and +0x34/+0x35; returns 0 on any mismatch.
[O]

#### 0x000A756C `ring_mark_invalid` — logic
After `0xA7326`-style state check: sets +0x34 = 0xFF (+ complement) — the "content invalid"
marker — and marks the changed flag `PTR_DAT_000a7634`. Gated by `*PTR_DAT_000a7638`. [O]

#### 0x000A7598 `ring_check_sum8` — logic
Recomputes the sum of the eight 2-byte entries and compares with the stored sum at +0x14.
[O]

#### 0x000A75C6 `ring_clear_entries` — logic
Zeroes the eight entries, index +0x12 and sum +0x14 (loop re-reads control byte
`PTR_DAT_000a763c[0x18]` per slot) and marks changed. Reached via thunk `0xA736A`. [O]

#### 0x000A766A `ret_identity` — stub
1-instruction identity: returns its argument. Used as a default no-op in a pointer table
(consumer `0xA7B12`). Singleton. [O]

#### 0x000A766C `park_lever_reset_if_d632` — logic
When `DAT_fff8d632` == 0: writes {0,0xFF} into the two out-bytes and sets the changed-flag
pair `DAT_fff83044/45` = {1,0xFE} — reset of a lever-related output pair. Called by
`0xA7B2E`. [I]

#### 0x000A768C `park_lever_set_range` — logic
Writer of the lever-request object field +0x00 (spec 14 §4.3): stores param_2 with
complement into *param_1; when `DAT_fff8d61b` == 1 (calibration override enable) and the
override slot `**(*DAT_000a79b8 + 0x18)` is not 0xFF, that value replaces the request; the
final value is mirrored to `DAT_fff8e571`. Called via `0xA7C00`. [O] Spec 14 §4.3.

#### 0x000A76C4 `ram_get_fff893ea` — data-accessor
Byte getter for RAM `0xFFF893EA` (snapshot-block area filled by `0xB912C`); consumers in
r0c (0xC823E, 0xC842E). Singleton. [O] Spec 09 §3.2.

#### 0x000A76CE `ram_get_fff893e8` — data-accessor
Byte getter `0xFFF893E8` — cited by spec 14 §3 as the auxiliary-park-request state check
(`D5F3 == 2 with 0xFFF893E8 != 0`) and spec 14 §4.4 gating; 33 callers across r0c/r08.
Singleton. [O] Spec 14 §3/§4.4.

#### 0x000A76D8 `ram_get_fff893e9` — data-accessor
Byte getter `0xFFF893E9`; five r0c callers. Singleton. [O]

#### 0x000A76E2 `ram_get_fff893f7` — data-accessor
Byte getter `0xFFF893F7`; callers include this module's `0xA4FC4`/`0xA512C` and r0c
consumers. Singleton. [O]

#### 0x000A76EC `ram_get_s8_fff893ee` — data-accessor
Sign-extended byte getter `0xFFF893EE`; callers 0xC823E/0xC842E (r0c). Singleton. [O]

#### 0x000A76F4 `ram_neq0_fff8d5ee` — data-accessor
Returns `DAT_fff8d5ee != 0` (derived-input block `0xFFF8D5E6`, spec 14 §3). Singleton. [O]

### Tiny RAM getters (0x000A7702–0x000A79D6)

Uniform generated getters of the park/range module's state and snapshot RAM; each returns
one byte (signed where noted). Callers listed are the resolved ones from `edges.csv`;
most are consumed by the r0c monitors/evaluators (`0xC2xxx–0xC8xxx`) and by this module's
own evaluators. Grouped entries follow for the plain byte getters.

#### 0x000A7702 `ram_get_fff8d609` — data-accessor
Byte getter `0xFFF8D609`. Singleton. [O]

#### 0x000A770C `ram_get_fff8d606` — data-accessor
Byte getter `0xFFF8D606`. Singleton. [O]

#### 0x000A7716 `ram_get_fff8d5f8` — data-accessor
Byte getter `0xFFF8D5F8` — spec 14 §3: "= 1 when D61A[0x2E]=1 and [0x38]=0", the **valid
park request from the safety layer** used by every arbiter mode. 18 callers incl.
`0xA56D8/0xA56FA/0xA5778/0xA57DA`. [O] Spec 14 §3.

#### 0x000A7720 `ram_get_fff8d5f9` — data-accessor
Byte getter `0xFFF8D5F9` — spec 14 §3 (`D654 != 0`); cited in the P1B14 tested-condition
(spec 13 §4.1). 26 callers. [O] Spec 13 §4.1, spec 14 §3.

#### 0x000A772A `ram_get_fff8d5f2` — data-accessor
Byte getter `0xFFF8D5F2` — spec 14 §3 auxiliary-park-request-active flag; 14 callers incl.
`0xA5D50`, `0xA6544`, `0xA6602`. [O] Spec 14 §3.

#### 0x000A7734 `ram_get_s8_fff8d5f6` — data-accessor
Signed byte getter `0xFFF8D5F6` — copy of speed `D650` (spec 14 §3); used by `0xA4C96`,
`0xA52A2`, `0xA5352`, `0xA5448`. [O] Spec 14 §3.

#### 0x000A773C `ram_get_fff8d607` — data-accessor
Byte getter `0xFFF8D607`; callers `0xA4F0C`, `0xA6704` and r0c. Singleton. [O]

#### 0x000A7746 `ram_get_fff8d5fa` — data-accessor
Byte getter `0xFFF8D5FA` (derived-input block); caller `0xA63F4`. [O] Spec 14 §3.

#### 0x000A7750 `ram_get_fff8d608` — data-accessor
Byte getter `0xFFF8D608`. Singleton. [O]

#### 0x000A775A `ram_get_fff8d603` — data-accessor
Byte getter `0xFFF8D603`; 16 callers; spec 14 §4.4 reverse-condition input. [O]

#### 0x000A7764 `ram_get_fff8d605` — data-accessor
Byte getter `0xFFF8D605`; spec 14 §4.4 D-side condition input. [O]

#### 0x000A776E `ram_get_fff8d604` — data-accessor
Byte getter `0xFFF8D604`; spec 14 §4.4 reverse-condition input. [O]

#### 0x000A7778 `ram_get_fff8d5fb` — data-accessor
Byte getter `0xFFF8D5FB`. Singleton. [O]

#### 0x000A7782 `ram_get_fff8d5f5` — data-accessor
Byte getter `0xFFF8D5F5` — spec 14 §3 falling-edge flag. [O]

#### 0x000A778C `ram_get_fff8d5f4` — data-accessor
Byte getter `0xFFF8D5F4` — spec 14 §3 rising-edge flag; caller `0xA53D4`. [O]

#### 0x000A7796 `ram_get_fff8d5f3` — data-accessor
Byte getter `0xFFF8D5F3` — spec 14 §3 request state 0/1/2. [O]

#### 0x000A77A0 `cfg_get_s16_off38` — data-accessor
Returns the signed word at `**(short **)(PTR_DAT_000A79BC + 0x38)` — a calibration/config
pointer deref, no direct callers (table-reached). Singleton. [I]

#### 0x000A77AA `ram_get_fff8d60a` — data-accessor
Byte getter `0xFFF8D60A`; callers `0xA6B06`, r0c. Singleton. [O]

#### 0x000A77B4 `ram_get_fff8e51f` — data-accessor
Byte getter `0xFFF8E51F` (module history area); one r0c caller `0xC7EF8`. [O]

#### 0x000A77BE `ram_get_fff8e51d` — data-accessor
Byte getter `0xFFF8E51D`; caller `0xC7EF8`. [O]

#### 0x000A77C8 `ram_get_fff8e523` — data-accessor
Byte getter `0xFFF8E523`; caller `0xC7EF8`. [O]

#### 0x000A77D2 `ram_get_fff8e521` — data-accessor
Byte getter `0xFFF8E521`; caller `0xC7EF8`. [O]

#### 0x000A77DC `ram_get_fff8d67f` — data-accessor
Byte getter `0xFFF8D67F`; callers `0xA8496`, `0xA8636`, r0c. [O]

#### 0x000A77E6 `ram_get_fff8d67e` — data-accessor
Byte getter `0xFFF8D67E`; callers `0xA8496`, r0c. [O]

#### 0x000A77F0 `ram_get_fff8e524` — data-accessor
Byte getter `0xFFF8E524`; caller `0xC7EF8`. [O]

#### 0x000A77FA `ram_get_fff8e525` — data-accessor
Byte getter `0xFFF8E525`; caller `0xC7EF8`. [O]

#### 0x000A7804 `ram_get_fff8d5fe` — data-accessor
Byte getter `0xFFF8D5FE`; caller `0x9B738` (task level). [O]

#### 0x000A780E `ram_get_fff8e530` — data-accessor
Byte getter `0xFFF8E530`; caller `0xC8300`. [O]

#### 0x000A7818 `ram_get_s8_fff8d624` — data-accessor
Signed byte getter `0xFFF8D624`; callers `0xA6E38` (digit counters), r0c. [O]

#### 0x000A7820 `ram_get_fff8d672` — data-accessor
Byte getter `0xFFF8D672` — spec 14 §2 `+0x58` **range request (5-8) computed in the safety
layer**; 14 callers incl. `0xA50C8`, `0xA6166`, `0xA6544`, `0xA6D6E`. [O] Spec 14 §2.

#### 0x000A782A `ram_get_fff8e531` — data-accessor
Byte getter `0xFFF8E531`; callers r0c. [O]

#### 0x000A7834 `ram_get_fff8d67d` — data-accessor
Byte getter `0xFFF8D67D`; callers `0xA8496`, `0xA8636`, r0c. [O]

#### 0x000A783E `ram_get_fff8d67c` — data-accessor
Byte getter `0xFFF8D67C`; callers `0xA8496`, r0c. [O]

#### 0x000A7848 `ram_get_fff8d673` — data-accessor
Byte getter `0xFFF8D673` — spec 14 §2 `+0x59` second request value; callers `0xA50C8`,
`0xA6704`, `0xA6D6E`, r0c. [O] Spec 14 §2.

#### 0x000A7852 `ram_get_fff893fc` — data-accessor
Byte getter `0xFFF893FC` (snapshot block); 11 callers. [O] Spec 09 §3.2.

#### 0x000A785C `ram_get_fff893ff` — data-accessor
Byte getter `0xFFF893FF`; caller `0xC4FC8`. [O]

#### 0x000A7866 `ram_get_fff893fe` — data-accessor
Byte getter `0xFFF893FE`; caller `0xC4FD8`. [O]

#### 0x000A7870 `ram_get_fff89404` — data-accessor
Byte getter `0xFFF89404`; caller `0xC5256`. [O]

#### 0x000A787A `ram_get_fff89405` — data-accessor
Byte getter `0xFFF89405`; caller `0xC5266`. [O]

#### 0x000A7884 `ram_get_fff8940a` — data-accessor
Byte getter `0xFFF8940A`; caller `0xC5276`. [O]

#### 0x000A788E `ram_get_fff8940b` — data-accessor
Byte getter `0xFFF8940B`; caller `0xC5286`. [O]

#### 0x000A7898 `ram_get_fff8940c` — data-accessor
Byte getter `0xFFF8940C`; caller `0xC5296`. [O]

#### 0x000A78A2 `ram_get_fff8940d` — data-accessor
Byte getter `0xFFF8940D`; caller `0xC52A6`. [O]

#### 0x000A78AC `ram_get_fff89411` — data-accessor
Byte getter `0xFFF89411`; caller `0xC5398`. [O]

#### 0x000A78B6 `ram_get_fff89413` — data-accessor
Byte getter `0xFFF89413`; caller `0xC5398`. [O]

#### 0x000A78C0 `ram_get_fff89415` — data-accessor
Byte getter `0xFFF89415`; callers `0xC54EE`, `0xA8E04`. [O]

#### 0x000A78CA `ram_get_fff89416` — data-accessor
Byte getter `0xFFF89416`; caller `0xC54FE`. [O]

#### 0x000A78D4 `ram_get_fff894c8` — data-accessor
Byte getter `0xFFF894C8`; 11 callers incl. `0xA6166`, `0xA6384`, `0xA5BA2` and the r15
safety layer. [O]

#### 0x000A78DE `ram_get_fff894c9` — data-accessor
Byte getter `0xFFF894C9`; callers r15/r08. [O]

#### 0x000A78E8 `ram_get_fff894ca` — data-accessor
Byte getter `0xFFF894CA`; callers `0xA5BA2`, r15, r0c. [O]

#### 0x000A78F2 `task_call1_78f2` — stub
Single registered callback with ctx `&DAT_fff894c8` (resolved `0xC4BA8`); caller `0x15643E`
(r15 safety layer). Singleton. [O]

#### 0x000A78FC `ram_get_s8_fff8d6e4` — data-accessor
Signed byte getter `0xFFF8D6E4`; callers `0xA5BA2`, `0xA5CC2`, `0xA63CA`. [O]

#### 0x000A7904 `ram_get_s8_fff8d6e0` — data-accessor
Signed byte getter `0xFFF8D6E0`; caller `0xA5AD8`. [O]

#### 0x000A790C `task_call1_790c` — stub
Single registered callback with ctx `&DAT_fff8d6e0` (resolved `0xC4AAC`); caller `0x15643E`
(r15). Singleton. [O]

#### 0x000A7916 `ram_get_fff8e559` — data-accessor
Byte getter `0xFFF8E559`; caller `0xA5BA2`. [O]

#### 0x000A792A `ram_get_fff8e565` — data-accessor
Byte getter `0xFFF8E565`; caller `0xC5688`. [O]

#### 0x000A7934 `ram_get_fff8e55a` — data-accessor
Byte getter `0xFFF8E55A`; 12 callers incl. `0xA6166`, `0xA5CC2`, r0c. [O]

#### 0x000A793E `ram_get_fff8e55b` — data-accessor
Byte getter `0xFFF8E55B`; same caller set as 0xA7934. [O]

#### 0x000A7948 `ram_get_fff8e55f` — data-accessor
Byte getter `0xFFF8E55F`; no resolved callers (table-reached). [O]

#### 0x000A7952 `ram_get_fff8e55c` — data-accessor
Byte getter `0xFFF8E55C`; 9 callers incl. `0xA8496`, r0c. [O]

#### 0x000A795C `ram_get_fff8e55e` — data-accessor
Byte getter `0xFFF8E55E`; caller `0xC531C`. [O]

#### 0x000A7966 `ram_get_fff8d6ce` — data-accessor
Byte getter `0xFFF8D6CE` — the range value feeding the third arbiter `0xC7C0C` (spec 14
§4.4); 8 callers. [O] Spec 14 §4.4.

#### 0x000A7970 `ram_get_fff8d6cf` — data-accessor
Byte getter `0xFFF8D6CF`; caller `0xC4D3C`. [O]

#### 0x000A797A `ram_get_fff8d6d2` — data-accessor
Byte getter `0xFFF8D6D2`; caller `0x9B72C` (task level). [O]

#### 0x000A7984 `ram_get_fff8941b` — data-accessor
Byte getter `0xFFF8941B`; caller `0xA6544`. [O]

#### 0x000A798E `ram_get_fff8941a` — data-accessor
Byte getter `0xFFF8941A`; callers `0xC3CEA`, `0x158044`. [O]

#### 0x000A7998 `ram_get_fff8e541` — data-accessor
Byte getter `0xFFF8E541`; caller `0xC2812`. [O]

#### 0x000A79A2 `ram_get_fff8e53f` — data-accessor
Byte getter `0xFFF8E53F`; callers `0xC530C`, `0xC51AA`. [O]

#### 0x000A79AC `ram_get_fff8e533` — data-accessor
Byte getter `0xFFF8E533`; callers `0xA8496`, r0c. [O]

#### 0x000A79CC `ram_get_fff8e532` — data-accessor
Byte getter `0xFFF8E532`; callers `0xA8496`, r0c. [O]

#### 0x000A79D6 `ram_get_fff8e535` — data-accessor
Byte getter `0xFFF8E535`; callers `0xA8496`, r0c. [O]

## Unit summary

**Clusters: 220. Names written: 271** (reps + 51 cluster members; 40 of those are
cross-region duplicate copies).

Counts by kind (per function, including members):
`data-accessor` 109, `logic` 79, `stub` 58, `library` 21, `dispatch` 4, `irq` 0,
`entry` 0, **`unknown` 0** — every function's mechanics are observable; purpose-level
uncertainty is carried by [I] markers instead of unknown names.

No `unknown_*` entries. Lowest-confidence names worth a second look:
`shiftlock_bit_control` (0x000A6B06 — bit set/clear on a pointer-table port, exact output
open), `shifter_lever_vote` / `shifter_mode_calc` (0x000A6D6E/0x000A6D12), the
`class_*` framework family (0x000A4BF4–0x000A4E66, generic vtable-object/timer framework;
its concrete objects live behind pointer tables), and `ring_*` (0x000A7326–0x000A75C6,
checksum-guarded 8x2-byte ring buffer maintained by tasks 0x9B63E–0x9B668; what it records
is open).

Module identity: this slice is the **selector/park-range application module** of spec 14,
plus its PRND-indication layer (ASCII P/R/N/D/S/L/digit display char at 0x000A5BA2), the
lever-request object (0xFFF8304C) checked getters/writers, the park-target arbiter family
(0x000A548C/0x000A557C/0x000A54A4/0x000A56A4–0x000A57DA), a switch-position decoder
(0x000A64C4 family), DID registration/read handlers (0x000A67A8, 0x000A6A04, 0x000A6ECC,
0x000A6F40), and ~100 generated RAM/interface getters consumed by the r0c monitor bank.

Cross-region leads for other units:
- Members of my clusters living in other units (same code, other modules):
  `flagpair_set_1fe` ×14 (0xC23CA, 0x125272, 0x13852E, 0x138D3C, 0x13948A, 0x14003A,
  0x14115A, 0x1418A8, 0x1440D6, 0x149B7A, 0x14B3EE, 0x14EB64), `callpair_64b6` ×12
  (0xC589C, 0xC8556, 0xC9298, 0xE94EE, 0xEFD0C, 0x106AB6, 0x10A914, 0x10A922, 0x134752,
  0x1394B6), `prnd_eval_stages` copies (0xC5550, 0xC5886, 0xE0760, 0x100C42), ring/lever
  copies in r14 (0x14EC64, 0x14EC7A, 0x14EC9C, 0x13C358), and the **safety-layer lever
  getters** 0x158AE4/0x158AF4/0x158B04/0x158B14/0x158B24/0x158B34 (spec 14 §6 redundant
  check) — the r15/r14 units should treat these as spec-14-anchored names.
- Resolved call targets into r0c (0xC20BA…0xC8914 monitors/evaluators) and the boot DID
  registry service 0x7A6E0 (r04) — those units own the counter side of these wrappers.
- 0x000B912C (not in this unit) consumes the `iface_get_*` getter bank to fill snapshot
  block 0xFFF89C3C (spec 09 §3.2); its unit can reuse the `iface_get_*` names.

Spec cross-refs: spec 14 (park/range module — arbiter, lever object, D5Fx/D6xx flags),
spec 13 §4.1 (P1B14 failure bytes exposed via DIDs 0x138F/0x1390 at 0x000A6F40), spec 09
§3.2 and spec 07 §7 (0xB912C interface copy), spec 15 (DID read services 0x4306-0x4325 /
0x1389-0x13A4).

Open items worth bench follow-up: identity of the output port behind `shiftlock_bit_control`
(candidates: P-display blink vs. shift-lock solenoid, spec 12 §1.2); the meaning of the
range codes 0xB/0xD written by `prnd_map_lever_range` (park-module internal codes vs.
display protocol); the 0x9C variant constant at `park_is_variant_9c`; what the ring buffer
records.
