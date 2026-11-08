# Unit 15 (r08_app)

Clusters 0x0009B144-0x000A0D82: application C-runtime (heap, init), then a long run of app logic.

#### 0x0009B144 `lib_heap_alloc` — library
Rounds the requested size up to the alignment kept in `PTR_DAT_0009b190`, then calls the
allocator hook `PTR_FUN_0009b194` (resolved to 0x000A4B50) with (size, DAT_0009b184); on success
stores `size-4` into the first word of the block (allocator header) and returns it, else 0.
Classic newlib-style `malloc` front end for the app heap; called only by 0x0009B0C2 (heap setup).
Cluster of 1. [O]

#### 0x0009B198 `lib_heap_free` — library
Inserts a freed block back into the singly-linked free list rooted at `PTR_DAT_0009b224`,
coalescing with the physical successor (header at ptr-1, block length `hdr[0]+4`) and with the
previous free block when adjacent. Only caller is 0x0009B0C2, pairing with `lib_heap_alloc`
above — this is the `free()` of the pair. Cluster of 1. [O]

#### 0x0009B228 `lib_run_global_ctors` — library
One-shot constructor runner guarded by the flag at `PTR_DAT_0009b3fc`: first call invokes 21
init thunks (PTR_FUN_0009b400..0x0009b480, each with its object/DSO-handle argument), then sets
the flag; second call returns immediately. This is the GCC `_GLOBAL__sub_I` equivalent that
brings up the app's static C++ objects (streams etc.). Callees resolve into 0x000A5xxx-0x000A8xxx
(C++ runtime bodies) and 0x0015xxxx. Cluster of 1. [O]

#### 0x0009B2AC `lib_dt_state_dispatch` — dispatch
Destructor-path helper used by two dtor runners (0x0009B2E8, 0x0009B322): clears a state byte
in the object at `PTR_DAT_0009b488` and compares byte `*PTR_DAT_0009b404` against the object's
second byte; depending on the outcome tail-calls callback `PTR_FUN_0009b48c` or `PTR_FUN_0009b490`
(resolved 0x000A713E / 0x000A70AA). Looks like a conditional teardown (e.g. flush vs. close) for
one static object. Cluster of 1. [I]

#### 0x0009B2E8 `lib_run_dtors_2e8` — library
At-exit runner: calls `PTR_FUN_0009b494` (unregister?), invokes `lib_dt_state_dispatch`
(0x0009B2AC) when the ctor flag at 0x0009b3fc is clear, then `(*DAT_0009b49c)(PTR_DAT_0009b498)`.
Same GCC `_GLOBAL__sub_D` shape as its siblings; callees 0x000A67A8/0x000A70A8. Cluster of 1. [I]

#### 0x0009B306 `lib_run_dtors_306` — library
Straight-line dtor runner invoking four registered destructors of the form
`dtor(obj)` / `dtor(obj, &dtor)` via PTR_FUN/PTR_DAT pairs 0x0009b404..0x0009b4ac
(resolved 0x000A7C9C, 0x000A8F5C, 0x000A5878, 0x00155AC0). Cluster of 1. [O]

#### 0x0009B322 `lib_run_dtors_322` — library
Dtor runner: first `lib_dt_state_dispatch` (0x0009B2AC), then a guarded callback when flag
`PTR_DAT_0009b4b0` is set (clearing it), then three more dtors via PTR_FUN 0x0009b4b8..0x0009b4c0
(resolved 0x000A5944, 0x000A592C, 0x000A525A, 0x000A591C). Cluster of 1. [O]

#### 0x0009B34A `lib_run_dtors_34a` — library
Largest dtor runner of the group: nine registered destructors via PTR_FUN 0x0009b4c4..0x0009b4e0
(resolved into 0x000A5xxx-0x000A8xxx). Same `_GLOBAL__sub_D` pattern. Cluster of 1. [O]

#### 0x0009B374 `lib_run_dtors_374` — library
Two registered destructors: `PTR_FUN_0009b4e4/PTR_FUN_0009b4e8` then
`PTR_DAT_0009b44c/PTR_FUN_0009b4ec` (resolved 0x000A4F0C, 0x000A4E52). Cluster of 1. [O]

#### 0x0009B384 `lib_run_dtors_384` — library
Two destructors (PTR_FUN 0x0009b4f0/0x0009b4f4, resolved 0x0015627A/0x001574AA) followed by an
increment of the phase counter byte at `PTR_DAT_0009b4f8` — the counter 0x9B39E resets.
Cluster of 1. [O]

#### 0x0009B39E `lib_run_dtors_39e` — library
Dtor-chain head/terminator: zeroes the counter at `PTR_DAT_0009b4f8`, runs eight registered
dtors (incl. thunk `PTR_thunk_FUN_000a6ab0_0009b504` and a call with literal 0xc6ab65 —
likely freeing a string constant), bumps the counter. Callees span 0x000A5xxx-0x000A8xxx,
0x001555E4/0x00156332 and 0x0007A520. Cluster of 1. [I]

#### 0x0009B530 `lib_run_dtors_530` — library
Two registered destructors via PTR_FUN 0x0009b568/0x0009b570 (resolved 0x000A5A70, 0x000A6A04).
Cluster of 1. [O]

#### 0x0009B546 `lib_run_dtors_546` — library
Three registered destructors; the middle one is called through a double indirection with literal
0xc6af48 (string free, cf. 0x9B39E); resolved callees 0x000A5A82, 0x00155E02, 0x0007A520.
Cluster of 1. [O]

#### 0x0009B588 `lib_vcall_9b74c` — stub
Six-instruction thunk: tail-calls the function pointer stored at DAT_0009b74c and returns its
byte result. Called from 16 sites across 0x000AD072-0x00159194, i.e. shared by the C++ stdlib
regions — a pluggable hook/getter (e.g. a `__retarget`-style lock or locale predicate).
Cluster of 1. [I]

#### 0x0009B5A0 `lib_vcall_9b754` — stub
Same shape as 0x9B588 but through DAT_0009b754; six callers (0x00132980-0x00147CBA).
Cluster of 1. [I]

#### 0x0009B5AC `lib_get_9b758` — data-accessor
Three-instruction getter: returns `(int)*DAT_0009b758` (value loaded through a code/data
pointer-word at 0x9b758). Callers 0x000B912C, 0x000F7298, 0x00144108. Cluster of 1. [O]

#### 0x0009B5B2 `lib_get_9b75c` — data-accessor
Returns byte `*PTR_DAT_0009b75c`. Callers 0x000F7242/0x000F7298. Cluster of 1. [O]

#### 0x0009B5BA `lib_get_9b760` — data-accessor
Returns byte `*PTR_DAT_0009b760`. Callers 0x000F7234/0x000F7298. Cluster of 1. [O]

#### 0x0009B5C2 `lib_get_9b764` — data-accessor
Returns byte `*PTR_DAT_0009b764`. Caller 0x000F7298. Cluster of 1. [O]

#### 0x0009B5CA `lib_get_9b768` — data-accessor
Returns byte `*PTR_DAT_0009b768`. Callers 0x00132D8E-0x00156068 (six). Cluster of 1. [O]

#### 0x0009B5D2 `lib_get_9b76c` — data-accessor
Returns byte `*PTR_DAT_0009b76c`. Callers 0x001534FC, 0x00156068. Cluster of 1. [O]

#### 0x0009B5DA `lib_get_9b770` — data-accessor
Returns byte `*DAT_0009b770`. Callers 0x00132D8E, 0x0014FC5C, 0x00152102. Cluster of 1. [O]

#### 0x0009B5E2 `lib_get_9b774` — data-accessor
Returns byte `*PTR_DAT_0009b774`. Caller 0x00152148. Cluster of 1. [O]

#### 0x0009B5EA `lib_get_9b778` — data-accessor
Returns byte `*PTR_DAT_0009b778`. Caller 0x00152156. Cluster of 1. [O]

#### 0x0009B5F2 `lib_get_9b77c` — data-accessor
Returns byte `*PTR_DAT_0009b77c`. Caller 0x0015212C. The 0x9B5AC-0x9B5F2 family are one-per-global
flag getters used by the 0x0013/0x0015 stdlib code (stream/locale state bits). Cluster of 1. [I]

#### 0x0009B5FA `lib_get_9b780` — data-accessor
Returns byte `*PTR_DAT_0009b780`; caller 0x001520E6 only. Part of the 0x9B5AC-0x9B744 thunk
family servicing the 0x0013/0x0015 stdlib code. Cluster of 1. [O]

#### 0x0009B602 `lib_get_9b784` — data-accessor
Returns byte `*PTR_DAT_0009b784`; caller 0x001520F4. Cluster of 1. [O]

#### 0x0009B60A `lib_get_9b788` — data-accessor
Returns byte `*DAT_0009b788`; callers 0x00152110/0x0015211E. Cluster of 1. [O]

#### 0x0009B612 `lib_get_9b78c` — data-accessor
Returns byte `*DAT_0009b78c`; callers 0x0013D75A, 0x0014FC5C, 0x00133C4E. Cluster of 1. [O]

#### 0x0009B61A `lib_get_9b790` — data-accessor
Returns byte `*PTR_DAT_0009b790`; caller 0x00155F8A. Cluster of 1. [O]

#### 0x0009B622 `lib_get_9b794` — data-accessor
Returns byte `*PTR_DAT_0009b794`; caller 0x000F7298. Cluster of 1. [O]

#### 0x0009B62A `lib_get_deref_9b798` — data-accessor
Returns `**PTR_DAT_0009b798` (double dereference); caller 0x001520D8. Cluster of 1. [O]

#### 0x0009B634 `lib_get_9b798_p1` — data-accessor
Returns byte at offset +1 of the object behind `PTR_DAT_0009b798` (same object as 0x9B62A);
caller 0x0015213A. Cluster of 1. [O]

#### 0x0009B63E `lib_vcall_9b7a0` — stub
Calls `(*DAT_0009b7a0)(PTR_DAT_0009b79c)` and returns the byte result — virtual one-arg getter
thunk over the object at 0x9b79c; resolved target 0x000A736E, caller 0x00139EF2. Cluster of 1. [O]

#### 0x0009B64C `lib_vcall_9b7a4` — stub
Same virtual-call shape via `PTR_FUN_0009b7a4` over the same object `PTR_DAT_0009b79c`;
resolved 0x000A738E, caller 0x00139FB4. Cluster of 1. [O]

#### 0x0009B65A `lib_vcall_9b7a8_w` — stub
Virtual call via `PTR_FUN_0009b7a8` over `PTR_DAT_0009b79c`, returning a 16-bit value
(read-two-bytes variant of the pair above); resolved 0x000A73B4, caller 0x0013A09E. Cluster of 1. [O]

#### 0x0009B668 `lib_vcall_9b7ac_set` — stub
Virtual two-arg call via `PTR_FUN_0009b7ac` forwarding (object `PTR_DAT_0009b79c`, param);
a one-byte "put" counterpart to the getters above; resolved 0x000A73C0, caller 0x0013A022. [O]
Cluster of 1.

#### 0x0009B672 `lib_vcall_9b7b4_3arg` — stub
Forwards three arguments to the virtual call `PTR_FUN_0009b7b4(PTR_DAT_0009b7b0, ...)` and
returns the 16-bit result; resolved 0x000A6ECC, single caller 0x00144108. Cluster of 1. [O]

#### 0x0009B686 `lib_get_9b7b8` — data-accessor
Returns byte `*PTR_DAT_0009b7b8`; caller 0x001531B8. Cluster of 1. [O]

#### 0x0009B68E `lib_get_9b7bc` — data-accessor
Returns byte `*PTR_DAT_0009b7bc`; callers 0x00144108, 0x001531E2, 0x001534FC, 0x00155890.
Cluster of 1. [O]

#### 0x0009B696 `lib_get_9b7c0` — data-accessor
Returns byte `*PTR_DAT_0009b7c0`; callers 0x001531C6-0x001556D4. Cluster of 1. [O]

#### 0x0009B69E `lib_get_9b7c4` — data-accessor
Returns byte `*PTR_DAT_0009b7c4`; callers 0x0014FC5C, 0x00133C4E. Cluster of 1. [O]

#### 0x0009B6A6 `lib_get_9b7c8` — data-accessor
Returns byte `*DAT_0009b7c8`; callers 0x00144108, 0x001534FC, 0x0007ACF2. Cluster of 1. [O]

#### 0x0009B6AE `lib_vcall_9b7cc_2arg` — stub
Forwards two arguments through `DAT_0009b7cc(PTR_DAT_0009b7b0, ...)` (same object as 0x9B672);
resolved 0x000A6F40, caller 0x00134A28. Cluster of 1. [O]

#### 0x0009B6BA `lib_get_9b7d0` — data-accessor
Returns byte `*PTR_DAT_0009b7d0`; caller 0x001534FC. Cluster of 1. [O]

#### 0x0009B6C2 `lib_get_s16_9b7d4` — data-accessor
Returns the 16-bit value `*(short *)PTR_DAT_0009b7d4`; caller 0x001534FC. Cluster of 1. [O]

#### 0x0009B6C8 `lib_set_shadow_28` — stub
Six-instruction setter: stores the argument byte at `PTR_DAT_0009b7d8[0x28]` AND at
`*(DAT_0009b7dc + 0x28)` — a primary-plus-shadow write (redundant copy, likely main and backup
structure). Twin member 0x9B6D4 does offset 0x29. Both called only by 0x001359B4. Cluster of 2
(members 0x0009B6C8, 0x0009B6D4 `lib_set_shadow_29`). [O]

#### 0x0009B720 `lib_vcall_9b7e8` — stub
Tail-call through `DAT_0009b7e8`, returning a byte; resolved target 0x000A78E8, single caller
0x00157A90. Cluster of 1. [O]

#### 0x0009B744 `lib_get_9b7f4` — data-accessor
Returns byte `*PTR_DAT_0009b7f4`; caller 0x00152A94. Cluster of 1. [O]

#### 0x0009B7F8 `lib_cmp_ret_5a_a5` — logic
Compares the int at `*param_1` with the result of `PTR_FUN_0009b818(PTR_LAB_0009b814, param_2)`
(resolved 0x00151DCA): equal → returns 0x5A, mismatch → 0xA5 (complementary byte pair typical
of validity/status flags). Single caller 0x00136A86. Cluster of 1. [O]

#### 0x0009B81C `lib_cmp_5a_a5_b83c` — logic
Sibling of 0x9B7F8: compares `*param_1` against the result of the virtual call
`PTR_FUN_0009b83c(PTR_LAB_0009b838, param_2)` (resolved 0x00151DCA) and returns 0x5A on match,
0xA5 otherwise. Single caller 0x0013C51A. Cluster of 1. [O]

#### 0x0009B840 `lib_cmp_5a_a5_b860` — logic
Same compare-return-5A/A5 pattern via PTR_FUN_0009b860; caller 0x0013E5FC. Cluster of 1. [O]

#### 0x0009B864 `lib_cmp_5a_a5_b884` — logic
Same pattern via PTR_FUN_0009b884; caller 0x001405FA. Cluster of 1. [O]

#### 0x0009B888 `lib_cmp_5a_a5_b8a8` — logic
Same pattern via PTR_FUN_0009b8a8; caller 0x0014A6E8. Cluster of 1. [O]

#### 0x0009B8AC `lib_cmp_5a_a5_b8cc` — logic
Same pattern via PTR_FUN_0009b8cc; caller 0x0014C23E. Cluster of 1. [O]

#### 0x0009B8D0 `lib_cmp_5a_a5_b8f0` — logic
Same pattern via PTR_FUN_0009b8f0; caller 0x0014C818. Cluster of 1. [O]

#### 0x0009B8F4 `lib_cmp_5a_a5_b914` — logic
Same pattern via PTR_FUN_0009b914; caller 0x0014D79E. The 0x9B7F8-0x9B8F4 family of eight are
per-site validation stubs used by the 0x0013-0x0015 monitor code. Cluster of 1. [O]

#### 0x0009B918 `dtc_fail_now` — logic
Spec 13 §2a "fail_now(group, data)": rejects when module state byte `*PTR_DAT_0009bbb4` is 0,
group out of 1..0xC3, or state 'Z' (0x5A) → returns 0x8000. Otherwise toggles the access bit
0x800 in the group status array `PTR_DAT_0009bbb8`, checks the exclusion mask table
`PTR_DAT_0009bbc0` (+ object byte 0x20 mask at `PTR_DAT_0009bbc4`) and the per-group object
(byte 0x21 bit0), then on the confirm path sets status bits 0x1603 and 0x2088/0x2098 (+0x20 when
`FUN_0009d0ca` returns 1, likely warning-indicator), calls freeze-frame capture `FUN_0009d232`
family and finish helpers, finally `FUN_0009c110` (timer stop) + PTR_FUN_0009bc08/0c/10. Callers:
0x000A105E, 0x000A0E94, 0x0007AC44, 0x0007B222, 0x0007B4EC, 0x0007BBE0, 0x0006F4C8, 0x00082DD4,
0x00082F84. Cluster of 1. [O] (spec 13 §2a)

#### 0x0009BC38 `dtc_fail_now_nocycle` — logic
Variant of `dtc_fail_now` over the same arrays (`PTR_DAT_0009bebc` status, mask at 0x0009bec4,
exclusion table 0x0009bec8): shortcut `PTR_FUN_0009bef4(group)` when the word is already
0x1201-mature; otherwise clears bits 0x100/0x8/0x2 (`&0xfeff|0x1200`, `&0xfff7`, `&0xfffd|1`),
runs the one-time guarded block via `PTR_FUN_0009bee4` and stop logic `FUN_0009c110`, then
`FUN_0009ccae`/`FUN_0009ccf6`. Sets testFailed (bit0) while clearing the cycle bit — the
"failed, no operation-cycle bookkeeping" sibling. Callers 0x000A105E, 0x000A0EA4, 0x0006F4C8.
Cluster of 1. [I] (spec 13 §2a context)

#### 0x0009BE50 `dtc_report` — logic
Spec 13 §2 "report(group, data, failed, tested)" core (reached from 0xA0E70): validates group
1..0xC3 against state byte `*PTR_DAT_0009beb8`, toggles access bit 0x800 in status array
`PTR_DAT_0009bebc`, applies the exclusion masks; permission masks (`PTR_FUN_0009c170/174`) can
cancel the tested flag. tested==0 → clears accumulators 0x0009c178/0x0009c17c and takes
`dtc_not_tested_clear` (0x9C342); tested!=0 → `dtc_deb_tick` (0x9C1C6) then fail finalize
(0x9C516, sets bit1|0x400) or pass finalize (0x9C960, clears bit1). Ends with
`FUN_0009ccae`/`FUN_0009ccf6` and a bit-3 change notification via PTR_FUN_0009c180. Caller
0x000A0E70. Cluster of 1. [O] (spec 13 §2)

#### 0x0009C094 `dtc_timer_active` — data-accessor
Returns `group-1` if the group's debounce counter at `PTR_DAT_0009c184` is non-zero, else 0xFFFF
— the "timer running?" query used by the finalize dispatchers (0x9C960/0x9C516/0x9C342).
Cluster of 1. [O]

#### 0x0009C0B2 `dtc_timer_arm` — logic
Arms the group's timer: param_2==0 → counter `PTR_DAT_0009c184[i]` = current tick from
`PTR_FUN_0009c174`; param_2!=0 → counter = 1 and if the previous value was 0 also sets
`PTR_DAT_0009c188[i]` = 1. Sets bit 2 (0x4) in word array `PTR_DAT_0009c18c` and snapshots the
tick into `PTR_DAT_0009c190[i]`. Callers 0x9C43C, 0x9C6A8, 0x9C89A. Cluster of 1. [O]

#### 0x0009C110 `dtc_timer_stop` — logic
Disarms the timer: zeroes the counter at `PTR_DAT_0009c184[i]` and clears bit 2 (mask 0xfffb) in
the word array `PTR_DAT_0009c198`. Called by the report/fail paths (0x9B918, 0x9BC38, 0x9C342,
0x9C58C, 0x9C98A). Cluster of 1. [O]

#### 0x0009C124 `dtc_timer_tick` — logic
Accumulates time: reads the previous tick snapshot `PTR_DAT_0009c190[i]`, gets the current tick
from `PTR_FUN_0009c194`, adds the delta to counter `PTR_DAT_0009c184[i]` saturating at 0xFFFF,
clamps to param_2, and tracks the running maximum in `PTR_DAT_0009c400[i]`. Single caller
0x9C98A (the periodic update). Cluster of 1. [O]

#### 0x0009C1C6 `dtc_deb_tick` — logic
When neither maturity bit is finished, reads the previous timestamp `PTR_DAT_0009c408[i]`, gets
the current tick via `PTR_FUN_0009c40c`, and feeds the elapsed delta into both debounce
accumulators: `dtc_deb_fail_accum` (0x9C21E) and `dtc_deb_heal_accum` (0x9C2B0). Called only by
`dtc_report` (0x9BE50). Cluster of 1. [O]

#### 0x0009C21E `dtc_deb_fail_accum` — logic
Fail-debounce accumulator: while bit 0x200 clear, adds `delta*rate` to the saturating 16-bit
counter `PTR_DAT_0009c414[i]` (first increment seeds 1); when the counter reaches the threshold
from `PTR_FUN_0009c418`, sets status bit 0x200 in the group word `PTR_DAT_0009c404`. Called only
from 0x9C1C6. Cluster of 1. [O]

#### 0x0009C2B0 `dtc_deb_heal_accum` — logic
Twin of 0x9C21E operating on counter `PTR_DAT_0009c41c` and setting status bit 0x1000 at the
threshold — the heal/pass debounce direction. Called only from 0x9C1C6. Cluster of 1. [O]

#### 0x0009C342 `dtc_not_tested_clear` — logic
Not-tested/clear path of `dtc_report`: records a timestamp into `PTR_DAT_0009c420[i]`; one-time
guarded init looks the event record up (`PTR_FUN_0009c42c(group,1)`, record = idx*0x38) and
clears bit 0 of record byte +5 (stored flag); if the status/timer conditions hold, stops the
timer (`FUN_0009c110`) and calls PTR_FUN_0009c438. 'Z' state → 0x8000. Returns the group status
word. Caller 0x9BE50. Cluster of 1. [I]

#### 0x0009C43C `dtc_pass_event_update` — logic
Pass-side event-record update (timer inactive path): guarded by state 'Z'/re-entry flag
`PTR_DAT_0009c678`; resolves the event record via `PTR_FUN_0009c680(group,1)` (0xFFFF → just
clears bit 3 in the status array `PTR_DAT_0009c684`); in the event record sets word+4
`&0xfffd|1` (testFailed recorded) or, when word+4 bit 3 is set, branches on `PTR_FUN_0009c694`:
arm timer (0x9C0B2) or run `FUN_0009cf88(record)` and clear bits 0xC in the group word. Ends with
`FUN_0009d01a(group)`. Caller 0x9C960. Cluster of 1. [I]

#### 0x0009C516 `dtc_fail_finalize` — logic
Fail finalize of `dtc_report`: dispatches on `dtc_timer_active` — inactive → immediate confirm
worker `FUN_0009c6a8(group,data)`, active → timer path `FUN_0009c98a`; then, when the occurrence
counter `PTR_DAT_0009c698[i]` reached the limit from `PTR_FUN_0009c694` and bit 0x20/8/0x2000
conditions hold, invokes `PTR_FUN_0009c69c(group)` (warning-indicator latch, [I]). Returns the
inner result. Caller 0x9BE50. Cluster of 1. [O]

#### 0x0009C58C `dtc_pass_timer_heal` — logic
Pass finalize when a timer is active: 'Z'/locked → stop timer (0x9C110), call PTR_FUN_0009c6a0
and clear bit 3, return 0x8000; otherwise resolves the event record and either (per
`FUN_0009cc54`) runs the full clear — `FUN_0009cf88(record)`, stop timer, clear bit 3 in the
status array `PTR_DAT_0009c684`, `FUN_0009d01a(group)` — or just refreshes event word+4 bits 0/1.
Caller 0x9C960. Cluster of 1. [I]

#### 0x0009C6A8 `dtc_fail_confirm_work` — logic
Immediate-fail confirmation worker (180 instrs): event lookup `PTR_FUN_0009c8d8(group,1)`;
no-record branch arms the timer (0x9C0B2) or, on confirmation, calls the freeze-frame capture
`FUN_0009ce52`/`FUN_0009d232`, `FUN_0009cf9e`, sets status bits 0x2088/0x2098 (+0x20 via
`FUN_0009d0ca`) and `FUN_0009cff0(group, stored, snapshot)`; with a record present it marks
event word+4 |= 3, re-checks the bit-8/0x2000 latch, and when session mode 2 and record byte+5
bit 3 allow, copies param_2 into record+2 (fault data). Callers 0x9C516 (and tail block 0x9C81A).
Cluster of 1. [O]

#### 0x0009C81A `dtc_fail_confirm_tail` — logic
Continuation block of 0x9C6A8 (entered with unassigned r10/r12/r13 carried in registers — a
Ghidra split, callers='-'): repeats the record-finish sequence — 0x9CE52, 0x9CF9E, 0x2098|0x20
status bits, `FUN_0009cff0`, and the session-mode-2 data store into record+2 — then clears the
re-entry flag at `PTR_DAT_0009c8d4`. Cluster of 1. [I]

#### 0x0009C89A `dtc_fail_locked_exit` — logic
Shared exit of 0x9C6A8 for the locked/'Z' case: when status bits 8 and 0x2000 are not both set,
arms the timer (0x9C0B2) or runs `FUN_0009cfc4` + PTR_FUN_0009cb2c when the masks at
`PTR_DAT_0009cb24/28` reject, latching status bit 0x2088 (+0x20 via 0x9D0CA); returns 0x8000.
Caller 0x9C6A8. Cluster of 1. [I]

#### 0x0009C960 `dtc_pass_finalize` — logic
Pass finalize of `dtc_report`: `dtc_timer_active` (0x9C094) == -1 → `FUN_0009c43c(group)`
(event-record update), else → `FUN_0009c58c` (timer heal). Caller 0x9BE50. Cluster of 1. [O]

#### 0x0009C98A `dtc_fail_timer_path` — logic
Timer-path fail finalize (308 instrs), called from 0x9C516: first ticks the timer
(`FUN_0009c124`), then splits on event-record presence — no record + confirm-fail → 0x9CFC4 +
0x2088 latch, or 0x9CF9E/0x9C110 stop + 0x2098 latch; record present → full freeze-frame
`FUN_0009ce52`, `FUN_0009cf9e`, stop timer, 0x2098 (+0x20 via `FUN_0009d0ca`) and
`FUN_0009cff0(group, stored, snapshot)`; also the session-mode-2 data store into record+2.
'Z'/locked branch mirrors 0x9C89A. Cluster of 1. [O]

#### 0x0009CC54 `dtc_countdown_update` — logic
Per-group countdown at `PTR_DAT_0009cd90[i]`: takes the minimum with param_2, otherwise
decrements by the elapsed ticks (timestamp array `PTR_DAT_0009cd94[i]`, tick from
`PTR_FUN_0009cd98`); returns the remaining count. Used by `dtc_pass_timer_heal` (0x9C58C).
Cluster of 1. [O]

#### 0x0009CCAE `dtc_bmap_bit1_sync` — logic
When the group object byte 0x21 bit 7 is clear, sets/clears the group's bit in the 13-word
bitmap `PTR_DAT_0009cda0` (bit index = group-1) according to status bit 1 of the word array
`PTR_DAT_0009cd74` — keeps a "failed this operation cycle" bitmap in sync. Callers 0x9B918,
0x9BC38, 0x9BE50, 0x000A016A. Cluster of 1. [O]

#### 0x0009CCF6 `dtc_bmap_bit3_sync` — logic
Twin of 0x9CCAE syncing status bit 3 (confirmed) into bitmap `PTR_DAT_0009cda4`. Callers 0x9B918,
0x9BC38, 0x9BE50, 0x000AC114, 0x000A016A. Cluster of 1. [O]

#### 0x0009CD3E `dtc_bmap_bit1_clear` — logic
Zeroes the 13 words (208 bits) of bitmap `PTR_DAT_0009cda0`. Caller 0x000AAFE8 (clear-all).
Cluster of 1. [O]

#### 0x0009CD54 `dtc_bmap_bit3_clear` — logic
Zeroes the 13 words of bitmap `PTR_DAT_0009cda4`. Caller 0x000AAFE8. Cluster of 1. [O]

#### 0x0009CDA8 `dtc_related_groups_check` — logic
Walks the up-to-3 related group numbers stored at `(group-1)*6 + base(PTR_FUN_0009d02c)`; for
each relative that is not the group itself, reads its status word from `PTR_DAT_0009d034` and —
depending on the session flag `PTR_FUN_0009d03c & 0x100` — tests bit 0/1 or bit 3; if any
relative is failed, sets bit 0x100 in this group's word (else clears). Fault-group interlock.
Callers 0x9B918, 0x9BC38, 0x9BE50. Cluster of 1. [O]

#### 0x0009CE52 `dtc_freeze_frame_capture` — logic
Fills the 0x38-byte event record (param_3): increments the occurrence byte record+3 (sat. 0xFF)
with callback `PTR_FUN_0009d040`, snapshots two clock values into `PTR_DAT_0009d048/050`, sets
record word+2 bits (0x100 when param_5, final |= 0x4F), runs the capture callbacks
`PTR_FUN_0009d05c/060(record, param_4, mode)`, stores param_2 (fault data) into record+2 outside
session 2, and manages the record byte+5 bit 0x20/0x10 flags and the stored counters record+0xb,
record+0xd, decrementing `PTR_DAT_0009d06c` for bit-0x2000 records. Callers 0x9B918, 0x9C81A,
0x9C98A. Cluster of 1. [O]

#### 0x0009CF88 `dtc_event_soft_clear` — logic
Light record reset: event word+4 = `&0xfff5 | 1` (clears bits 1,3; sets bit 0) plus two callbacks
`PTR_FUN_0009d070/074`. Callers 0x9BC38, 0x9C43C, 0x9C58C. Cluster of 1. [O]

#### 0x0009CF9E `dtc_store_pending_or` — logic
ORs the result of `PTR_FUN_0009d078(param_1+8)` into the global flag words at `PTR_DAT_0009d07c`
and again (no-arg call) into `PTR_DAT_0009d080` — accumulates "record changed / storage pending"
state after a capture. Callers 0x9B918, 0x9C6A8, 0x9C81A, 0x9C98A. Cluster of 1. [I]

#### 0x0009CFC4 `dtc_store_pending_or2` — logic
Variant of 0x9CF9E ORing into `PTR_DAT_0009d084`/`PTR_DAT_0009d08c` and additionally storing a
byte (extraout_r2) at `PTR_DAT_0009d088`. Callers 0x9B918, 0x9C6A8, 0x9C89A, 0x9C98A, 0xABF98.
Cluster of 1. [I]

#### 0x0009CFF0 `dtc_group_flagbyte_set` — logic
Updates the per-group flag byte at `DAT_0009d054[group]`: `&0xaf|0x23`; when param_3==0 also
sets bit 0x4 and, if param_2!=0, bit 0x8. Called at the end of every confirm path (0x9B918,
0x9C6A8, 0x9C81A, 0x9C98A). Cluster of 1. [O]

#### 0x0009D01A `dtc_group_flagbyte_clear` — logic
Clears the per-group flag byte: `DAT_0009d054[group] & 0xae`. Counterpart of 0x9CFF0; callers
0x9BC38, 0x9BE50, 0x9C43C, 0x9C58C. Cluster of 1. [O]

#### 0x0009D090 `dtc_event_data_fwd` — logic
Looks the event record up (`PTR_FUN_0009d0f0(group,1)`); if found, reads the stored fault data
(record+2) and forwards (group, data) to `PTR_FUN_0009d0f8`; else forwards the passed param_2.
Callers 0x9B918, 0x9BC38, 0x9BE50. Cluster of 1. [O]

#### 0x0009D0CA `dtc_warn_indicator_check` — logic
Returns 1 when either `PTR_FUN_0009d0fc(param_1+8)` or the no-arg second call is non-zero — the
condition that latches status bit 0x20 (warning-indicator requested, [I]) in the confirm paths.
Callers 0x9B918, 0x9C6A8, 0x9C81A, 0x9C89A, 0x9C98A, 0x9CE52, 0xAC324. Cluster of 1. [I]

#### 0x0009D100 `dtc_cycle_process` — logic
Operation-cycle master (caller 0x000AAFA8): when the module state byte `*PTR_DAT_0009d26c` is
0xA5 and the once-per-cycle flag at `PTR_DAT_0009d270` is set, runs `dtc_cycle_start` (0x9D158)
then the cycle task chain — 0x9D9C2, 0x9DA70, 0x9DAB6, 0x9DBF8, 0x9D932, 0x9DE18, 0x9DE8A,
0x9DF60, 0x9DFDC, 0x9DB8A plus PTR_FUN bookends — then always calls PTR_FUN_0009d280.
Cluster of 1. [O]

#### 0x0009D158 `dtc_cycle_start` — logic
Cycle-start reset: arms `PTR_FUN_0009d284(1/2)`, snapshots odometers/timestamps via
`FUN_0009e168/0x0009e162` into struct 0x9D290+0x81C/0x826 and RAM DAT_fff8d5c8-fff8d5d2, clears
DAT_fff8e4f5-4fa; when the double condition on `PTR_FUN_0009d294` and struct[0x7e2]==1 holds,
performs the full reset sequence (7 callbacks + PTR_FUN_0009d2b8(1)) and then iterates all 0xC3
groups, setting per-group byte `&0xfd|0x40` where `PTR_FUN_0009d2c0(group)` returns 1 —
fault-memory aging at ignition-cycle start. Callers 0x9D100, 0x9F7AA. Cluster of 1. [I]

#### 0x0009D232 `dtc_failed_word_ptr` — logic
Returns a pointer to the group's 16-bit status word (array base from `PTR_FUN_0009d2c4`,
index group-1) when its bit 0 (testFailed) is set, else NULL. Widely used filter by the report
core and cycle tasks (10 callers). Cluster of 1. [O]

#### 0x0009D252 `dtc_rec_pending_bits` — logic
Returns 1 when the event record's word+4 has bit 0x800 or 0x2000 set ([I] "record needs
re-store / historical"), else 0. Callers 0x9F944, 0x9FA5E, 0x9FDC4, 0xAC214. Cluster of 1. [I]

#### 0x0009D2C8 `dtc_aging_step` — logic
Large per-record aging machine (347 instrs): manages the occurrence/heal counter record[8]
(0x00→0x01 seeds, 0xFE→0xFD decay path with the `PTR_FUN_0009d588/590` callback at 0xFD), moves
the group bit between the working bitmaps `PTR_DAT_0009d574/0x9d578`, gates on bitmaps
`0x9d57c/0x9d580` and status bit 0x1000 in `PTR_DAT_0009d584`, sets the RAM flags
DAT_fff8e4fb/DAT_fff8e4fc and the dirty flag `PTR_DAT_0009d594`, and clears record word+2 bit
0x100. Called only by 0x9DBF8 (cycle task). Cluster of 1. [I]

#### 0x0009D5D2 `dtc_snapshot_odometer_get` — logic
One-shot reader for the environment snapshot: when the module state byte (0xA5/0xAA/0x55) and
the not-busy flag allow, copies the two shorts behind `PTR_PTR_0009d7b8` (odometer/timestamp at
fault, [I]) into *param_1/*param_2 and returns 0; otherwise 0xFF. Callers 0xADB34, 0xAE2AA,
0xAE5A0 (diagnostic read services). Cluster of 1. [I]

#### 0x0009D626 `dtc_any_fault_present` — logic
With state 0xA5: iterates the 0xC3 group status words; returns 0 as soon as a group has
testFailed (bit 0) while not excluded by the selected bitmap (`param_1==0` → `PTR_DAT_0009d7cc`
else `0x9d7d0`); returns 1 when none/short-circuited ([I] "no relevant fault present" /
readiness). Callers 0xAE0C8, 0xAE5A0. Cluster of 1. [I]

#### 0x0009D6BA `dtc_clear_all` — logic
Clears the fault-memory state block: zeroes struct fields at 0x818-0x828 and 0x7e2/0x7e3 of the
block behind `PTR_DAT_0009d7d4`, re-snapshots the odometers (0x9E168/0x9E162) into 0x81C and
DAT_fff8d5cc-fff8d5d2, clears DAT_fff8e4f5/4f6/4fd, and invokes reset callbacks
PTR_FUN_0009d7d8/7dc/7e0 + DAT_0009d7e4. Internal of ClearDiagnosticInformation. Callers 0x9FA5E,
0xA086A, 0x9FDC4. Cluster of 1. [I]

#### 0x0009D73E `dtc_get_fff8d5ca` — data-accessor
Returns the u16 at DAT_fff8d5ca (fault-memory timestamp slot). Callers 0xADC1E, 0xAE5A0,
0x000A0C98. Cluster of 1. [O]

#### 0x0009D746 `dtc_get_d7e8` — data-accessor
Returns the u16 behind `PTR_DAT_0009d7e8`. Caller 0xAE5A0. Cluster of 1. [O]

#### 0x0009D74C `dtc_get_d7ec` — data-accessor
Returns u16 behind `PTR_DAT_0009d7ec`; callers 0xAE0C8, 0xAE5A0. Cluster of 1. [O]

#### 0x0009D752 `dtc_get_d7f0` — data-accessor
Returns s8 behind `PTR_DAT_0009d7f0`; callers 0xAE0C8, 0xAE5A0. Cluster of 1. [O]

#### 0x0009D758 `dtc_get_d7f4` — data-accessor
Returns s8 behind `DAT_0009d7f4`; callers 0xAE0C8, 0xAE5A0. Cluster of 1. [O]

#### 0x0009D75E `dtc_get_d7f8` — data-accessor
Returns u16 behind `PTR_DAT_0009d7f8`; caller 0xAE5A0. Cluster of 1. [O]

#### 0x0009D764 `dtc_get_d7fc` — data-accessor
Returns s8 behind `PTR_DAT_0009d7fc`; caller 0xAE5A0. Cluster of 1. [O]

#### 0x0009D76A `dtc_get_time_d5d0` — data-accessor
Returns u16 DAT_fff8d5d0 (condition-gated time accumulator, see 0x9D88A). Caller 0xADC1E.
Cluster of 1. [O]

#### 0x0009D772 `dtc_get_time_d5cc` — data-accessor
Returns u16 DAT_fff8d5cc (warm-up accumulator, see 0x9D800). Caller 0xADC1E. Cluster of 1. [O]

#### 0x0009D77A `dtc_time_d5d0_gt29` — logic
Returns `0x1d < DAT_fff8d5d0`. Caller 0xADC1E. Cluster of 1. [O]

#### 0x0009D788 `dtc_time_d5cc_gt299` — logic
Returns `299 < DAT_fff8d5cc`. Caller 0xADC1E. Cluster of 1. [O]

#### 0x0009D79A `dtc_time_d5ca_gt599` — logic
Returns `599 < DAT_fff8d5ca`. Caller 0xADC1E. Cluster of 1. [O]

#### 0x0009D800 `dtc_accum_d5cc` — logic
Accumulates the time base `FUN_0009e162` into DAT_fff8d5cc (saturating 0xFFFF, wrap correction
-0x76C9) while the gate `FUN_0009e0ca` is true and the byte from `PTR_FUN_0009da48` >= 0x29
([I] temperature above warm-up threshold); otherwise latches the paused flag DAT_fff8e4ff and
DAT_fff8d5d4. Called from 0x9DB8A. Cluster of 1. [O]

#### 0x0009D88A `dtc_accum_d5d0` — logic
Second gated accumulator into DAT_fff8d5d0 (0xFFFF saturation, wrap +0x8937 correction): runs
while below 0x1E, gate `PTR_FUN_0009da4c`==0, byte `PTR_FUN_0009da48` < 2, and
`FUN_0009e0ca` true; pause latch DAT_fff8e500/DAT_fff8d5d6. Called from 0x9DB8A. Cluster of 1. [O]

#### 0x0009D91C `dtc_cycle_healed_flag` — data-accessor
Returns DAT_fff8e4f7 when the state byte at DAT_0009da50 is 0xA5, else 0xFF — the "cycle
completed / aged-out" flag computed by 0x9D932. Callers 0x001534FC, 0xAE0C8, 0xAE5A0. Cluster of 1. [O]

#### 0x0009D932 `dtc_aged_records_scan` — logic
One-time-per-cycle scan over all event records (count `PTR_FUN_0009da58(1)`, stride 0x38): for
records with word+2 bit 0x200 whose group word passes `dtc_failed_word_ptr` with bit 1, sets
bit7 in the per-group byte at DAT_0009da60 when the aging counter record[8] matches the stored
level or is > 0xFD; records the outcome in DAT_fff8e4f7. Callers 0x9D100, 0x9D158. Cluster of 1. [O]

#### 0x0009D9C2 `dtc_cycle_bitmap_refresh` — logic
For each of the 195 groups not already marked in bitmap `PTR_DAT_0009da68`: when the status word
(array `PTR_DAT_0009da64`) lacks bit 0x200, or the failed-word filter has bit 2, calls
`dtc_bitmap_mark_group` (0x9DA1A). Cycle task from 0x9D100. Cluster of 1. [O]

#### 0x0009DA1A `dtc_bitmap_mark_group` — logic
Sets the group's bit in both bitmaps `PTR_DAT_0009da68` and `PTR_DAT_0009da6c`. Callers 0x9B918,
0x9BC38, 0x000A0BD6, 0x9D9C2. Cluster of 1. [O]

#### 0x0009DA70 `dtc_cycle_cond_flag` — logic
Cycle task guarded by flag `PTR_DAT_0009dc28`: selects a mode struct via `PTR_FUN_0009dc2c`;
mode 1 delegates to `PTR_FUN_0009dc58(buf)` (abort on non-zero), otherwise samples
`FUN_0009e0ca`; when the local condition becomes 1 the flag is latched. Caller 0x9D100. Cluster of 1. [I]

#### 0x0009DAB6 `dtc_warmup_cycle_detect` — logic
Warm-up cycle detector: gated by struct[0x7e3] (PTR_DAT_0009dc38); mode struct via
`PTR_FUN_0009dc3c` (mode 1 → `PTR_FUN_0009dc58`), else latches the first sample into
DAT_fff8e4f5/DAT_fff8e4f4 and compares later samples (with `PTR_FUN_0009dc48/0x9dc50` bounds)
for the rise; on completion sets struct[0x7e3]=1 → DAT_fff8e4f9=1 and runs 0x9E088/0x9E09C.
Cycle task from 0x9D100. Cluster of 1. [I]

#### 0x0009DB78 `dtc_warmup_value_get` — data-accessor
Returns DAT_fff8e4f4 when flag DAT_fff8e4f5==1, else 0xFF (warm-up snapshot value). Caller
0xAE5A0. Cluster of 1. [O]

#### 0x0009DB8A `dtc_drive_cycle_detect` — logic
Drive-cycle detector: short-circuits once DAT_fff8e4fd is set; mode struct via `PTR_FUN_0009dc5c`
(abort via `PTR_FUN_0009dc64`); otherwise runs the two accumulators (0x9D800, 0x9D88A) and
requires the thresholds DAT_fff8d5ca >= 600, DAT_fff8d5cc >= 300, DAT_fff8d5d0 >= 0x1E before
latching DAT_fff8e4fd = DAT_fff8e4fa = 1. Cycle task from 0x9D100. Cluster of 1. [O]

#### 0x0009DBF8 `dtc_cycle_records_update` — logic
Main per-cycle record pass: for every event record (stride 0x1C words) whose group word passes
`dtc_failed_word_ptr`, runs the aging step (0x9D2C8); on the DAT_fff8e4fc path sets record
word+2 |= 0x200, marks the group byte at DAT_0009deb0 |= 8, optionally `FUN_0009e5c8(rec,0x800)`
and copies the byte from `PTR_FUN_0009dec0` into record+7; then 0x9E69C, PTR_FUN_0009dec8,
0x9E9D6 and the countdown 0x9DD1C, setting the dirty flag at PTR_DAT_0009dc78. Cycle task from
0x9D100. Cluster of 1. [O]

#### 0x0009DD1C `dtc_record_store_countdown` — logic
Per-record storage countdown: when the mode/flag gates (`PTR_FUN_0009decc`, state at
PTR_DAT_0009ded4, status bit 0x1000 in array 0x0009ded8, bitmap `PTR_DAT_0009dedc`) allow and
the record is in the healable state, marks the record's bit and decrements record byte+7; at
zero fires `PTR_FUN_0009dee0(group)`, re-checks `PTR_FUN_0009dee4 & 0x80` and
`FUN_0009e406(index)`. Returns 1 when it acted. Caller 0x9DBF8. Cluster of 1. [I]

#### 0x0009DE18 `dtc_consec_cycles_count` — logic
Counts consecutive equal-step samples of an int source `PTR_FUN_0009dee8(&v)`: while the healed
flag DAT_fff8e4f7 stays set, increments struct+0x820 (sat -1) whenever the source advanced by
exactly 1 since struct+0x818, otherwise just re-bases; resets via DAT_fff8e501 when the flag
drops. Cycle task from 0x9D100. [I] Cluster of 1.

#### 0x0009DE8A `dtc_cond_accum_81e` — logic
Gated accumulator for struct+0x81E of the block behind `PTR_DAT_0009e170` using the time base
`FUN_0009e168` (wrap +0x249) and the gate `FUN_0009e0ca`; saturates at 0xFFFF only while the
gate is true; returns the gate. Cycle task from 0x9D100. Cluster of 1. [O]

#### 0x0009DF60 `dtc_cond_accum_d5ca` — logic
Gated accumulator into DAT_fff8d5ca from `FUN_0009e162` (wrap +0x8937) while
`FUN_0009e0ca` is true, with pause latch DAT_fff8e4fe/DAT_fff8d5da. Cycle task from 0x9D100.
Cluster of 1. [O]

#### 0x0009DFDC `dtc_accum_828` — logic
Unconditional wrap-aware accumulator into struct+0x828 (block behind `PTR_DAT_0009e170`) using
time base `FUN_0009e168` (wrap +0x249), saturating at 0xFFFF; returns the time base. Cycle task
from 0x9D100. Cluster of 1. [O]

#### 0x0009E02E `dtc_consec_count_824` — logic
Consecutive-step counter into struct+0x824 (sat -1) driven by int source `PTR_FUN_0009e174`,
first-sample latch DAT_fff893c8/DAT_fff8e4f6. Cycle task from 0x9D100. Cluster of 1. [O]

#### 0x0009E088 `dtc_counter_822_inc` — logic
Increments struct[0x822] (behind `PTR_DAT_0009e170`) unless it is 0xFF. Called on warm-up-cycle
completion by 0x9DAB6. Cluster of 1. [O]

#### 0x0009E09C `dtc_counter_823_step` — logic
struct[0x823]: increments (unless 0xFF) while the healed flag DAT_fff8e4f7 is clear; resets to 0
when it is set. Called by 0x9DAB6. Cluster of 1. [O]

#### 0x0009E0CA `dtc_cond_active_check` — logic
The shared gate used by all cycle accumulators: takes the ms base `PTR_FUN_0009e178`, a
measurement `PTR_FUN_0009e17c(buf)`, a threshold `PTR_FUN_0009e180` and a duration
`PTR_FUN_0009e18c`; once the measurement is below threshold it waits out the duration from
DAT_fff893cc before latching the byte at `PTR_DAT_0009e188` to 1 ([I] "engine running /
driving active" gate). Callers 0x9D800/0x9D88A/0x9DAB6/0x9DA70/0x9DE8A/0x9DF60. Cluster of 1. [I]

#### 0x0009E13C `dtc_timebases_update` — logic
Divides the ms base `PTR_FUN_0009e178` by 1000 into the seconds base `PTR_DAT_0009e194`, keeps
the low byte at `PTR_DAT_0009e198` and minutes at +2 — maintains the fault-memory time bases.
Callers 0x9D100, 0x9D158. Cluster of 1. [O]

#### 0x0009E162 `dtc_timebase_get_e194` — data-accessor
Returns u16 `*PTR_DAT_0009e194` (seconds base maintained by 0x9E13C). Cluster of 1. [O]

#### 0x0009E168 `dtc_timebase_get_e19c` — data-accessor
Returns u16 `*PTR_DAT_0009e19c` (second time base, kept elsewhere). Cluster of 1. [O]

#### 0x0009E1A0 `dtc_fault_count_set` — data-accessor
Selector setter: 1 → `*PTR_DAT_0009e374 = param_2`; 2 → `*PTR_DAT_0009e370 = param_2`. Callers
0x9E1D4, 0x9E74C and the storage layer (0x9FA5E etc.). Cluster of 1. [O]

#### 0x0009E1B8 `dtc_fault_count_get` — data-accessor
Selector getter for the same pair; returns 0xFFFF for other selectors. Callers across the
storage layer (0x9F944-0xAC8CA). Cluster of 1. [O]

#### 0x0009E1D4 `dtc_count_present_faults` — logic
Walks record list 1 (up to 20 records, `PTR_PTR_0009e37c`) or list 2 (10, `PTR_PTR_0009e378`)
and counts groups whose status passes `FUN_0009eebe` (spec 13 §2 "fault currently present");
stores the count via `dtc_fault_count_set`. Callers 0x9D158, 0x9F7AA. Cluster of 1. [O] (spec 13 §2)

#### 0x0009E22E `dtc_count_stored_records` — logic
Counts records (max 20) whose word+2 has bit 0x2000 into `*PTR_DAT_0009e380`. Caller 0x9F7AA.
Cluster of 1. [O]

#### 0x0009E25A `dtc_warmup_done_get` — data-accessor
Returns DAT_fff8e4f9 (warm-up cycle completed) when state byte 0xA5, else 0xFF. Caller 0xAE5A0.
Cluster of 1. [O]

#### 0x0009E270 `dtc_flag_e4f8_get` — data-accessor
State-0xA5-gated getter: param_1==0 → DAT_fff8e4f8, else the byte behind `PTR_DAT_0009e388`;
0xFF otherwise. Caller 0xAE5A0. Cluster of 1. [O]

#### 0x0009E292 `dtc_drivecycle_done_get` — data-accessor
Returns DAT_fff8e4fa (drive cycle completed) when state 0xA5, else 0xFF. Caller 0xADC1E.
Cluster of 1. [O]

#### 0x0009E2A8 `dtc_mirror_reset_swap` — logic
When param_1 matches the group of the 0x18-byte mirror record at `PTR_PTR_0009e38c`: clears all
0x18 bytes and `PTR_DAT_0009e390[0x878]`; in mode 3 (via `PTR_FUN_0009e394`) locates a
replacement record with `FUN_0009e312` and copies it in via `PTR_FUN_0009e39c(dst, src)` with
the mirror-busy flag set. Callers 0x9E406/0x9E408, 0x9E9D6, 0xAC114. Cluster of 1. [O]

#### 0x0009E312 `dtc_best_record_idx` — logic
Scans all event records; for records whose group differs from param_1 and passes the validity
check `PTR_FUN_0009e3a4`, ranks them with `FUN_0009e3a8` and returns the index of the
highest-priority one (0xFFFF if none). Callers 0x9E2A8, 0x9E9D6. Cluster of 1. [O]

#### 0x0009E3A8 `dtc_record_priority` — logic
Maps record word+4 bits to a 0-3 priority: 1 = bit 0x100 without 0x200, 2 = bit 0x200 without
0x800, 3 = bits 0x200 and 0x800 (param_2 variant only counts the 0x200+0x800 case). Caller
0x9E312. Cluster of 1. [O]

#### 0x0009E3E8 `dtc_mirror_clear` — logic
Zeroes the 0x18-byte buffer behind `PTR_PTR_0009e60c` and clears `*PTR_DAT_0009e610`. Caller
0x000A086A. Cluster of 1. [O]

#### 0x0009E406 `dtc_record_erase` — logic
Erases one event-record slot (0x38 bytes at `PTR_PTR_0009e614` + idx*0x38): saves group/word+2/
bytes +8/+9, zeroes the record, restores them, sets word+2 = `&0x3c03 | 0x2000`, byte+7 = 4,
clears the group's bit from the bitmaps at 0x0009e61c-0x0009e634, runs `dtc_mirror_reset_swap`
(0x9E2A8), scrubs the status word (`&0xed57`) in array 0x0009e634, zeroes the 0x9e638/0x9e640/
0x9e644 entries, writes 0x50 into the per-group byte at DAT_0009e63c and re-syncs bitmap
0x0009cda4 (bit-3) with the group status. 0x9E406 is the 1-instruction tail-jump entry to the
full body at 0x9E408. Callers 0x9DD1C, 0x9F944, 0x9FA5E, 0x9FDC4. Cluster of 2 (members
0x0009E406 `dtc_record_erase`, 0x0009E408 `dtc_record_erase__2`). [O]

#### 0x0009E524 `dtc_record_soft_reset` — logic
Partial record reset: clears word+2 bits 0x1C00, zeroes record bytes +8, +7, +0xf, calls
`FUN_0009e91c(group)`; when byte+8 was non-zero, decrements that counter in every record and at
the value 4 fires `FUN_0009e5c8(record, 0x800)`. Callers 0x9D2C8, 0xAC114. Cluster of 1. [I]

#### 0x0009E58A `dtc_next_slot_num` — logic
Scans all records for the maximum byte+8 slot number and returns max+1 (capped 0xFF) — the
next free freeze-frame slot allocator. Caller 0x9E5C8. Cluster of 1. [O]

#### 0x0009E5C8 `dtc_record_slot_update` — logic
Slot bookkeeping on record word+4: param_2==0x800 assigns byte+8 (via 0x9E58A when unset) and,
for slot < 5, sets bit 0x800 (or 0xC00 in the mode at `PTR_DAT_0009e890`) and runs
`FUN_0009e8cc(record)`; param_2==0x1000 sets bit 0x1000 (or 0x1400) when 0x800 present. Ends by
setting the dirty flag `*DAT_0009e894 = 1`. Callers 0x9DBF8, 0x9E524, 0x9EE6E. Cluster of 1. [O]

#### 0x0009E69C `dtc_bitmap_8a8_update` — logic
Per-record bitmap maintenance: in mode 0 (via `PTR_FUN_0009e898`) clears the group bit from
bitmap 0x0009e8a8; otherwise, for records with word+2 bits 0x800 without 0x200 that pass the
mask bitmap 0x0009e8a0, status bit 0x1000 (array 0x0009e8a4) and the drive-cycle flag
DAT_fff8e4fd, sets the bit in 0x0009e8a8. Caller 0x9DBF8. Cluster of 1. [I]

#### 0x0009E74C `dtc_records_rebuild` — logic
Rebuilds the 20-record array at `PTR_PTR_0009e8ac`: clears it, then (in the other mode) walks
the 4-entry source list at `PTR_PTR_0009e8b0`, validates each group
(`PTR_FUN_0009e8b4/0x0009e8b8` + equality of word 2), checks "fault present" via `FUN_0009eebe`
and object byte 0x21 bit 0, and copies up to 4 entries into fresh records with word+2 =
0x3C00/0x3C80 and slot bytes +8/+9 = sequence number; ends with `PTR_FUN_0009e8c8(1)` and
`dtc_fault_count_set(1, n)`. Callers 0xA9EDE, 0xAC9D4, 0xAC9F2, 0xACA10. Cluster of 1. [I]

#### 0x0009E8CC `dtc_shortlist_entry_write` — logic
Writes (group, data, mapped-value) into one of the four 6-byte entries of the list at
`PTR_PTR_0009eb04` (free slot chosen by 0x9E9A0; the third word comes from
`PTR_FUN_0009eb08(group,data,buf)`). Callers 0x9E5C8, 0x9E91C. Cluster of 1. [O]

#### 0x0009E91C `dtc_shortlist_remove_rebuild` — logic
Removes the group's entry from the 4-entry shortlist (compact via `PTR_FUN_0009eb0c`, zero the
tail entry), then re-adds every other record that has word+4 bit 0x800 via 0x9E8CC. Caller
0x9E524. Cluster of 1. [O]

#### 0x0009E9A0 `dtc_shortlist_free_idx` — logic
Returns the first empty index of the 4-entry shortlist, 0xFFFE when the group is already listed,
0xFFFF when full. Caller 0x9E8CC. Cluster of 1. [O]

#### 0x0009E9D6 `dtc_record_replace_eval` — logic
Record-displacement decision (144 instrs) used by the cycle record pass: same-group case clears
the mirror (modes 2/3 without bits 0x100/0x200) or enforces the slot-count limit from
`PTR_FUN_0009eb34`, selecting a replacement record via `dtc_best_record_idx` (0x9E312) and
swapping via `PTR_FUN_0009eb30`; different-group case is gated by the DAT_fff8e4fc/4fb flags,
modes 2/3 and the latch `PTR_DAT_0009eb28`. Caller 0x9DBF8. Cluster of 1. [I]

#### 0x0009EB38 `dtc_bitmaps_rebalance` — logic
For all 195 groups with their bit set in bitmap `PTR_DAT_0009ed44`: sets/clears the same bit in
`PTR_DAT_0009ed4c` (per 0x0009ed48) and in `PTR_DAT_0009ed48` (per `PTR_DAT_0009ed50`), then
clears it from `PTR_DAT_0009ed50` — cycle-start rebalance of the group-class bitmaps. Caller
0x9D158. Cluster of 1. [I]

#### 0x0009EBB8 `dtc_bitmaps_clear_2` — logic
Zeroes the 13-word bitmaps `PTR_DAT_0009ed54` and `PTR_DAT_0009ed44`. Callers 0x9D158, 0x9D6BA.
Cluster of 1. [O]

#### 0x0009EBDE `dtc_bitmap_clear_58` — logic
Zeroes the 13 words at `PTR_DAT_0009ed58`. Callers 0x9D158, 0x9D6BA. Cluster of 1. [O]

#### 0x0009EBF4 `dtc_bitmap_clear_5c` — logic
Zeroes the 13 words at `DAT_0009ed5c`. Caller 0x9D6BA. Cluster of 1. [O]

#### 0x0009EC0A `dtc_bitmaps_clear_3` — logic
Zeroes the 13-word bitmaps `PTR_DAT_0009ed4c`, `0x0009ed48` and `0x0009ed50`. Caller 0x9D6BA.
Cluster of 1. [O]

#### 0x0009EC40 `dtc_records_purge_marked` — logic
One-time pass over all records (lock at `PTR_DAT_0009ed40`): for records with word+2 bit 0x800
whose group bit is set in bitmap `PTR_DAT_0009ed6c`, clears the bit, decrements the stored count
at `PTR_DAT_0009ed68` for 0x2000 records, writes 0x50 into the per-group byte at
`DAT_0009ed70`, calls the erase callback `PTR_FUN_0009ed74(idx)`, re-indexes via
`PTR_FUN_0009ed78(1,count)` and decrements the fault count (0x9E1B8/0x9E1A0). Caller 0x9D158.
Cluster of 1. [O]

#### 0x0009ED0E `dtc_records_compact` — logic
Tail-jump entry (1 instr) to the full body at 0x9ED10; called by 0x9D158. Cluster of 1 (see
0x9ED10 for the body). [O]

#### 0x0009ED10 `dtc_records_compact__2` — logic
Full compaction body (135 instrs): removes list entries whose byte+7 is 0, permission bit 0x80
(`PTR_FUN_0009ef38`) is set, group maps (`PTR_FUN_0009ef3c`) or mode 1, and word+2 lacks 0x2000 —
decrementing counts, fixing the list via `PTR_FUN_0009ef48/4c/50`, reporting through
`PTR_FUN_0009ef54(group,data)`; when no entry with bit 0x200 and byte+7 remains, clears the
struct+0x820/0x81e counters. Callers: none direct (reached via 0x9ED0E). Cluster of 1. [I]

#### 0x0009EE6E `dtc_slots_mark_1000_all` — logic
Under the lock at `PTR_DAT_0009ef68`, calls `dtc_record_slot_update(rec, 0x1000)` for every
record in the list at `PTR_PTR_0009ef5c`. Caller 0x9D158. Cluster of 1. [O]

#### 0x0009EEA8 `dtc_bitmap_clear_ef6c` — logic
Zeroes the 13 words at `PTR_DAT_0009ef6c`. Caller 0x000A086A. Cluster of 1. [O]

#### 0x0009EEBE `dtc_fault_present` — data-accessor
Spec 13 §2: for groups 1..0xC3 returns bit 0 (testFailed) of the status word (array base via
`DAT_0009ef70` call), else 0xFF — the "fault currently present" query; 22 callers across the
app, monitor and park/range regions. Cluster of 1. [O] (spec 13 §2)

#### 0x0009EEE8 `dtc_mode_get_ef40` — data-accessor
Copies the mode byte from `PTR_FUN_0009ef40` (cached at `PTR_DAT_0009ef44`) into *param_1;
returns 0. Callers 0xC901E, 0xC8FB6 (park/range region). Cluster of 1. [O]

#### 0x0009EEFE `dtc_mode_get_ef74` — data-accessor
Same shape via `PTR_FUN_0009ef74`/`PTR_DAT_0009ef78`. Callers 0xC901E, 0xC8FB6. Cluster of 1. [O]

#### 0x0009EF14 `dtc_rec_byte_f_inc` — logic
For records with word+2 bit 0x800 whose group bit is in bitmap `PTR_DAT_0009f0bc` but not in
`PTR_DAT_0009f0c0` and byte+0xf != -1: increments byte+0xf. Caller 0x9D158. Cluster of 1. [O]

#### 0x0009EFD6 `dtc_rec_byte7_update` — logic
Per record with bit 0x800: when the group bit is in 0x9f0bc — not in 0x9f0c0 → record byte+7 |= 1,
otherwise clears byte+7 and byte+0xf; for 0x2000 records byte+7 |= 4 (|= 6 when the drive cycle
flag DAT_fff8e4fd is set). Caller 0x9DBF8. Cluster of 1. [O]

#### 0x9F06E with address 0x0009F06E `dtc_freq_stats_update` — logic
Large cycle task (208 instrs) maintaining fault-occurrence statistics over all 195 groups: the
two header counters at struct+0x87A/0x87C (increment with -1 wrap reset), per-group saturating
counters `PTR_DAT_0009f3a0` (halved at 0x7FFF) with the group-population word at
`PTR_DAT_0009f38c`, seven gating bitmaps (0x0009f390-0x0009f3ac), plus the per-group helpers
`FUN_0009f3d8` and `FUN_0009f23a`. Caller 0x9D100. Cluster of 1. [I]

#### 0x0009F23A `dtc_group_timeouts_tick` — logic
Two per-group elapsed-time counters (arrays `PTR_DAT_0009f3b0/0x9f3b4` and
`0x9f3c0/0x9f3c4/0x9f3cc/0x9f3d0`) advanced by the delta from `FUN_0009f670`; when a counter
reaches its threshold (via `PTR_FUN_0009f3b8/0x9f3d0`) the group bit is cleared from bitmaps
`PTR_DAT_0009f3ac/0x9f3a4`, the second path also bumping the escalation counter
`PTR_DAT_0009f3cc` and resetting to 0xFF. Caller 0x9F06E. Cluster of 1. [I]

#### 0x0009F32C `dtc_freq_stats_clear` — logic
Clears the occurrence-statistics block: struct+0x87A/0x87C, the 195-word counter arrays
`PTR_DAT_0009f3a0`/`0x9f38c`, the 13-word bitmaps `0x9f398`/`0x9f39c`, then `FUN_0009f5a4`.
Callers 0x9F676, 0x000A086A. Cluster of 1. [O]

#### 0x0009F3D8 `dtc_related_bitmap_update` — logic
Walks the up-to-3 related groups (base + (group-1)*6 from `PTR_FUN_0009f580`); when a relative
with byte-flag bit 2 (at `DAT_0009f584`) is found, sets this group's bit in bitmap
`PTR_DAT_0009f58c`, otherwise clears it. Caller 0x9F06E. Cluster of 1. [O]

#### 0x0009F4A2 `dtc_monitor_window_update` — logic
State-0xA5-gated helper for monitor maturation windows: maps the group (0x9F576), reads the
status word's bits 0x30; param_2==0 clears the per-group window arrays
`PTR_DAT_0009f598/0x9f59c`; otherwise, when the bit is in bitmap `0x9f5a0`, stamps
`FUN_0009f670` into `0x9f598` and moves the bit between `0x9f5a0` and `0x9f590` (0x10 variant
clears). Callers 0x000D790A, 0x000D78B8, 0x0011D2E8 — the shift-element monitor region (spec 13
§2b). Cluster of 1. [I] (spec 13 §2b cross-ref)

#### 0x0009F576 `dtc_group_to_idx` — stub
Returns `param_1 - 1` (group number to zero-based index). Callers 0x9F3D8, 0x9F4A2. Cluster of 1. [O]

#### 0x0009F5A4 `dtc_freq_windows_reset` — logic
Resets the occurrence-window block: flags struct[0x87E/0x87F], three 13-word bitmaps
(`PTR_DAT_0009f6e0/e4/e8`), four 195-byte arrays (`0x9f6ec/f0/f4/f8`), then rebuilds bitmap
`DAT_0009f700` from the group status words (bit 8 gate, bits 0x30 set). Callers 0x9D158,
0x9F32C. Cluster of 1. [O]

#### 0x0009F670 `dtc_get_tick_f704` — data-accessor
Returns the tick byte/word at `DAT_0009f704` — the time source for the group timeout counters.
Callers 0x9F23A, 0x9F4A2. Cluster of 1. [O]

#### 0x0009F676 `dtc_on_state_reset_stats` — logic
On module states 0xA5/0xAA/0x55 with a one-shot flag: reads the mode via `PTR_FUN_0009f70c`;
when the mode changed to 'U' (0x55) or 0xAA, clears the occurrence statistics
(`FUN_0009f32c`). Cycle task from 0x9D100. Cluster of 1. [I]

#### 0x0009F718 `dtc_changed_mask_or` — data-accessor
Returns the OR of the "groups changed" mask words: selector 0 → `PTR_DAT_0009f8e0/e4/e8/ec`,
selector 1 → `0x9f8f0/f4/f8/fc`; state 'Z' ORs three masks, 0xA5 only the last, otherwise two.
Callers 0xA1026-0xACCF2 (the change-notification API). Cluster of 1. [O]

#### 0x0009F7AA `dtc_module_init` — logic
Fault-memory module bring-up: state byte 0x55 (`*PTR_DAT_0009f900`) is forced to 0xA5; states
0/0x5A run the init callback chain (`PTR_FUN_0009f908..0x0009f91c`), classify the new state
(0xA5/0xAA/0x5A), then `FUN_000a016a` (status mirror rebuild) and `FUN_0009fa5e(1,0xff)`.
Caller 0xAE868. Cluster of 1. [O]

#### 0x0009F838 `dtc_record_select_read` — logic
Gated (state 0xA5/0xAA/0x55, not busy) indexed access: maps param_3 via `PTR_FUN_0009f934`;
selector 1 → record list at `PTR_PTR_0009f938`, selector 2 → `0x0009f93c` (record = idx*0x38);
on hit calls `PTR_FUN_0009f940` and returns 0, else -1. Callers 0xAA176, 0xADB34, 0xC89D6,
0xC8A70, 0xC8D58 (snapshot readout / park-range). Cluster of 1. [I]

#### 0x0009F944 `dtc_clear_group` — logic
Single-group clear (136 instrs): state/busy gated; looks the record up (param_2), runs the
list-removal callbacks (`PTR_FUN_0009fba8-0x0009fbd0`) with count fixups, sets the dirty flag
`PTR_DAT_0009fbd4` and writes 0x50 into the per-group byte at `DAT_0009fbd8`; returns 0 on
success, 1 for the session-mode fallback. Caller 0x000AA948. Cluster of 1. [O]

#### 0x0009FDC4 `dtc_clear_by_mask` — logic
ClearDiagnosticInformation worker (212 instrs): iterates all records; those whose group byte-0x20
mask matches param_2 (and pass the stored/validity gates) are erased — list fixups via
`PTR_FUN_000a002c-0x000a0058`, per-group byte 0x50, count updates, completion flags at
`PTR_DAT_000a0044/0x000a0048`, dirty flag `0x000a005c`. Caller 0x000AA948. Cluster of 1. [O]

#### 0x0009FF86 `dtc_record_by_index_read` — logic
Gated by-index record access: selector 1 → index < 20 (list `PTR_PTR_000a0014`), selector 2 →
index < 10 (`PTR_PTR_000a0064`); computes record = idx*0x38 and calls `PTR_FUN_000a0068`;
returns 0xFFFF when refused. Callers 0xA9C18, 0xADC1E (UDS readouts). Cluster of 1. [O]

#### 0x000A0074 `dtc_post_clear_fixup` — logic
Post-clear list maintenance (120 instrs): under lock `PTR_DAT_000a01f8`, walks the records
decaying byte+7; when it reaches 0 with the permission bit (`PTR_FUN_000a0218 & 0x80`) the
entry is removed (callbacks `PTR_FUN_000a021c-0x000a0228`, count fixups), dirty flag
`DAT_000a022c`; mode-0x12 fallback path. Callers 0xAE868, 0x000AA948. Cluster of 1. [I]

#### 0x000A016A `dtc_status_mirror_rebuild` — logic
Rebuild (171 instrs): for every record runs `FUN_000a0bd6`, clears record byte+5 bits per the
word+2 conditions, then OR-maps word+2 bits (4→0x10, 8→8, 0x10→0x20, 0x40→0x80, 1→1, 2→2) into
the group status mirror at `PTR_DAT_000a0230`, accumulating change masks into
`PTR_DAT_000a0420/0x000a0424` with dirty flag `0x000a0414`. Caller 0x9F7AA. Cluster of 1. [O]

#### 0x000A031C `dtc_group_changed_test` — data-accessor
The change-notification query with 43 callers across every region: bit 0-0x3F of param_1 selects
one of the 32-bit change masks (`PTR_DAT_000a0420-0x000a0440`, state-dependent OR combination);
returns 1 when the bit is set. This is how monitors notice a group's state changed. Cluster of 1. [O]

#### 0x000A03D6 `dtc_group_status_get` — data-accessor
Spec-13-listed status reader: returns the group's 16-bit status word from the array behind
`PTR_DAT_000a0448` for valid groups (1-195) in states 0xAA/0xA5/0x55, else 0x8000. Callers
0xA0EB2, 0xA0ED6, 0xA0FA0, 0xA0FC4, 0xA0FE8, 0xAD606, 0xACEA4, 0xF7298. [I] second copy of the
working array (EEPROM mirror). Cluster of 1. [O] (spec 13 §2a status_readers)

#### 0x000A044C `dtc_count_by_filter` — logic
Large statistics helper (370 instrs): selector param_1 picks record list 1/2; param_2 (0-0x0E,
0x9B) picks one of fourteen predicates over record byte+5 / word+2 bits (total, stored, aged,
warned, matured, related…) and returns the matching record count — the counting back end of the
UDS ReadDTCInformation statistics. Callers 0xA9C18, 0xA104E, 0xA123C, 0xADC1E, 0xAE0C8.
Cluster of 1. [O]

#### 0x000A0776 `dtc_state_class_get` — data-accessor
Maps the state byte at `PTR_DAT_000a0898`: 'U'/0xAA → 2, 0xA5 → 1, else 0. Callers 0x6F594,
0x794D8, 0x82EA8. Cluster of 1. [O]

#### 0x000A0796 `dtc_run_gated_cb` — logic
Under the lock at `PTR_DAT_000a089c` and states 0xA5/0xAA/0x55, runs the callback
`PTR_FUN_000a08a0` (resolved 0xAC890). Caller 0xAB2D4. Cluster of 1. [O]

#### 0x000A07CC `dtc_flags_or` — stub
ORs param_1 into the flag byte at `PTR_DAT_000a08a4`. Callers 0xAA358, 0xAA3DA. Cluster of 1. [O]

#### 0x000A07D6 `dtc_flags_clear` — stub
Clears param_1 bits in the flag byte at `PTR_DAT_000a08a4`. Callers 0xAA39C, 0xAA41E. Cluster of 1. [O]

#### 0x000A07E2 `dtc_group_byte_get` — data-accessor
State-gated per-group byte read at `DAT_000a08a8[group]` after the validity check
`FUN_000a09fc`; 0xFF when refused. Callers 0xAA09A, 0xADC1E. Cluster of 1. [O]

#### 0x000A0816 `dtc_unmapped_group_mark` — logic
For all 195 groups that fail the mapping check `PTR_FUN_000a0894(group)`: clears bit 2 in the
per-group byte at `DAT_000a08ac` (when bits 2/0x40 clear) then forces `&0xfd|0x40`. Called by
the module init 0x9F7AA. Cluster of 1. [O]

#### 0x000A0856 `dtc_module_shutdown` — logic
Runs two callbacks (`PTR_FUN_000a08b0/b4`) and latches the flag at `PTR_DAT_000a08b8` — module
teardown/flush counterpart of `dtc_module_init`. Caller 0xAE890. [I] Cluster of 1.

#### 0x000A086A `dtc_clear_all_run` — logic
Full-clear orchestrator under lock `PTR_DAT_000a089c`: runs eleven callbacks (including
0xAAFE8 bitmap clears, 0x9EEA8, 0x9F32C stats clear, 0x9D6BA clear-all, 0x9E3E8 mirror clear),
latches `PTR_DAT_000a0a38` and resets `DAT_000a0a44`; busy path defers via mode-0x14. Callers
0xAA948, 0xAC570, 0xACA68 — the ClearDiagnosticInformation core. Cluster of 1. [O]

#### 0x000A08F8 `dtc_group_mask20_get` — data-accessor
Via `FUN_000a0bae(group,&obj)`: copies object byte+0x20 (mask byte) into *param_2; 0/0xFF.
Callers 0xACC94, 0xACEA4. Cluster of 1. [O]

#### 0x000A0920 `dtc_group_obj_bit0_get` — data-accessor
Via `FUN_000a0b7e(group,buf)`: returns bit 0 of the returned byte. Caller 0xC8DDC (park/range).
Cluster of 1. [O]

#### 0x000A0942 `dtc_group_byte7_get` — data-accessor
Via `FUN_000a0bae(group,&obj)`: copies object byte+7 into *param_2. Caller 0xC8FB6. Cluster of 1. [O]

#### 0x000A0968 `dtc_group_byte6_get` — data-accessor
Via `FUN_000a0bae(group,&obj)`: copies object byte+6 into *param_2; 0/0xFF. Caller 0xAD6D4.
Cluster of 1. [O]

#### 0x000A098E `dtc_group_stored_test` — data-accessor
Via `FUN_000a0b7e(group,buf)`: returns whether the returned word has bit 0x2000 (record stored,
[I]); -1 when refused. Callers 0xA0DB4, 0xAB356, 0xAB7DC, 0xABAF2, 0xAD606, 0xC8DDC. Cluster of 1. [O]

#### 0x000A09B6 `dtc_env_table_read` — data-accessor
Bounds-checked 2-D read: param_1 < 0x18 (rows), param_2 < 0x13 (19 u16 per row, stride 0x26)
from the table pointer behind `PTR_FUN_000a0a48` — [I] a 24x19 environment/freeze-frame value
table. Caller 0xAD6D4. Cluster of 1. [I]

#### 0x000A09FC `dtc_group_valid` — stub
Range check: returns 0 when 1 <= group <= 0xC3, else 0xFF. Used by fifteen callers as the
standard group validator. Cluster of 1. [O]

#### 0x000A0A14 `dtc_state_get` — data-accessor
Copies the module state byte at `PTR_DAT_000a0a50` into *param_1, returns 0. Callers 0xAA522,
0xACC94, 0xACEA4. Cluster of 1. [O]

#### 0x000A0A54 `dtc_filter_sel_valid` — stub
Validates the statistics selector: 0-0x0E or 0xFF → 0, else 0xFFFF. Caller 0x9FA5E. Cluster of 1. [O]

#### 0x000A0AA6 `dtc_filter_count_fetch` — library
Maps selector 0-0x0E/0xFF to 1-0x10 and fetches the corresponding u16 via
`PTR_FUN_000a0c68(n,0)` — the count dispatch behind `FUN_0009fa5e`. Caller 0x9FA5E. Cluster of 1. [O]

#### 0x000A0B7E `dtc_group_obj_fetch` — data-accessor
Gated fetch: `FUN_000a0bae` object pointer, then copies the word from `PTR_FUN_000a0c6c` and
object byte+0x21 into the two-word out param. Callers 0xA0920, 0xA098E. Cluster of 1. [O]

#### 0x000A0BAE `dtc_group_obj_ptr` — data-accessor
Validates the group (0xA09FC) and returns the object pointer from `(*DAT_000a0c70)(group)` in
*param_2; 0/0xFF. Callers 0xA08F8, 0xA0942, 0xA0968, 0xA0B7E. Cluster of 1. [O]

#### 0x000A0BD6 `dtc_stored_flag_clear` — logic
Stored-flag clear used by the status-mirror rebuild: for a valid group with session bit 0x400
and record byte+5 bit 3, calls `PTR_FUN_000a0c78(record)`, clears bits 0/4/6 in the per-group
byte array at `DAT_000a0c7c`, sets record word+2 `&0xfff5|1`, ORs 0x1000 into the mirror array
`PTR_DAT_000a0c80` and fires `PTR_FUN_000a0c84/0x000a0c88`. Caller 0xA016A. Cluster of 1. [O]

#### 0x000A0C8C `dtc_get_u32_efc` — data-accessor
Returns the u32 at `PTR_DAT_000a0efc` (timer callbacks context). Callers 0x9C0B2, 0x9C124,
0x9C1C6, 0x9CC54, 0x9C342. Cluster of 1. [O]

#### 0x000A0C92 `dtc_get_u32_f00` — data-accessor
Returns the u32 at `PTR_DAT_000a0f00`. Callers 0x9E0CA, 0x9E13C. Cluster of 1. [O]

#### 0x000A0C98 `dtc_record_env_fill` — logic
Fills a record's environment block: copies the group/data words and then bytes from the struct
behind `PTR_DAT_000a0f04` (offsets 0x11, 0x1b, 0xe, 0x16, 0x26, 0x2a plus a word from
`DAT_000a0f08`) into record bytes 4-0xF — the freeze-frame environment snapshot template.
Callers 0x9E2A8, 0x9E9D6. Cluster of 1. [O]

#### 0x000A0CF4 `dtc_warmup_source_get` — data-accessor
Reads a byte through the double-indirect `PTR_PTR_000a0f0c` (resolved 0x1417A8), caching it at
`PTR_DAT_000a0f10`; copies to out when < 2. Caller 0x9DAB6 (warm-up cycle detect). Cluster of 1. [O]

#### 0x000A0D1A `dtc_cond_source_get` — data-accessor
Same shape via `PTR_PTR_000a0f14` (resolved 0x141798); caller 0x9DA70 (cycle condition flag).
Cluster of 1. [O]

#### 0x000A0D40 `dtc_drivecycle_src_reset` — stub
Zeroes `*PTR_DAT_000a0f1c` and the out byte; used by the drive-cycle detector (0x9DB8A).
Cluster of 1. [O]

#### 0x000A0D4A `dtc_meas_get_adffc` — stub
Eight-instruction virtual getter over the env struct `PTR_DAT_000a0f04` via
`PTR_FUN_000a0f20` (resolved 0x000ADFFC); caller 0x9DAB6. Cluster of 3 — the members differ
only in the function-pointer slot: 0x000A0D5A `dtc_meas_get_adfe8` (resolved 0x000ADFE8, caller
0x9E0CA) and 0x000A0D6A `dtc_meas_get_ae012` (resolved 0x000AE012, callers 0x9DE18/0x9E02E).
These three feed the warm-up/condition/counter accumulators. [O]

#### 0x000A0D7A `dtc_get_f2c` — data-accessor
Returns `*DAT_000a0f2c`. Callers 0x9D800, 0x9D88A. Cluster of 1. [O]

#### 0x000A0D82 `dtc_get_f30` — data-accessor
Returns `*DAT_000a0f30`. Caller 0x9D88A. Cluster of 1. [O]

#### 0x0009FA5E `dtc_clear_by_filter` — logic
The filtered-clear engine (351 instrs): validates the filter selector (`FUN_000a0a54`), then —
state/busy gated like 0x9FDC4 — walks all records applying the param_2 predicate (selectors
0-0x0E/0xFF over record word+2 bits 8/0x80/0x40/0x100/0x200/0x400 and the mapped-group helpers
`PTR_FUN_0009fbb8/0x0009fbbc`); matching records are erased (per-group byte 0x50 at
`DAT_0009fd98`, list fixups `PTR_FUN_0009fd90-0x0009fdb8`, count updates, completion flags at
`PTR_DAT_0009fda4/0x0009fda8`, dirty flag `0x0009fdbc`); other states fall back to the count
fetch via `FUN_000a0aa6`. Callers 0xA9B00, 0xA0E84, 0x000AA948, 0x9F7AA. Cluster of 1. [O]

## Unit summary

Unit 15 covers 0x0009B144-0x000A0D82 (r08_app): 220 clusters, 224 functions named.
The slice is two blocks: (1) 0x9B144-0x9B918 — application C/C++ runtime support (newlib-style
heap alloc/free pair, GCC `_GLOBAL__sub_I/D` ctor/dtor runners, per-object vcall/getter thunks
used by the 0x0013-0x0015 stdlib code); (2) 0x9B918-0xA0D82 — the **fault-memory (DTC) engine**
already framed by spec 13: report core (`dtc_report` 0x9BE50 from 0xA0E70, `dtc_fail_now`
0x9B918), debounce/heal accumulators, per-group timers, the 0x38-byte event records with
freeze-frame capture and slot bookkeeping, the operation-cycle task chain (0x9D100) with
warm-up/drive-cycle detection and aging, and the clear paths (`dtc_clear_all_run` 0xA086A,
`dtc_clear_by_filter` 0x9FA5E, `dtc_clear_group` 0x9F944).

Counts by kind: logic 121, data-accessor 69, stub 20, library 13, dispatch 1, unknown 0.
(Many `data-accessor`/`stub` rows are tiny thunks; their summaries carry the pointer identity.)

Unknown entries: none — every function was classifiable from code plus spec 13 anchors.

Module state byte (multiple copies, e.g. `PTR_DAT_0009bbb4`, `0x0009beb8`, `0x000a0678`):
0xA5 active, 0x5A 'Z' inactive, 0xAA transitional, 0x55 'U' reset-pending — gates nearly every
entry point. Group range everywhere 1..0xC3 (195 fault groups, spec 13 §1); status-word bits
used as: bit0 testFailed, bit1 testFailedThisCycle, bit2 timer-armed, bit3 confirmed, 0x100
relative-failed, 0x200/0x1000 debounce matured, 0x800 access-toggled, 0x2000 stored.

Cross-region leads:
- The 0x9B2xx dtor runners resolve into 0x000A4-0x000A8 and 0x0015xxxx (C++ runtime bodies) —
  whichever unit holds those should cross-check the ctor/dtor pairing from here.
- 0x9EEBE `dtc_fault_present` (22 callers) and 0xA031C `dtc_group_changed_test` (43 callers) are
  the two big query APIs consumed by the monitor regions 0x000D-0x0015 (spec 13 §2a/§2b) and
  park/range 0xC8-0xC9 (spec 14).
- Clear dispatch: 0xAA948 → 0x9F944/0x9FDC4/0x9FA5E/0xA0074/0xA086A; UDS readouts 0xA9C18,
  0xADC1E, 0xAE0C8, 0xAE5A0 consume the getters/counters in 0x9D5D2-0xA09B6.
- 0x9F4A2 is called from the shift-element monitors 0xD790A/0xD78B8/0x11D2E8 (spec 13 §2b);
  0x9EEE8/0x9EEFE/0xA0920/0xA0942 serve the park/range module 0xC8DDC/0xC8FB6 (spec 14).
- The measurement/environment providers behind 0xA0CF4/0xA0D1A/0xA0D4A-cluster resolve to
  0x141798, 0x1417A8, 0xADFFC-0xAE012 — next unit should confirm their signal identities
  (spec 10 measurement index).
