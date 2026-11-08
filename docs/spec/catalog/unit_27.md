# Unit 27 (r0c_app)

Region 0x000D0918–0x000D7D62 of r0c_app. Interface-driven (ROM pointer-table) subsystem
band: two multi-state engine objects (`sm12` handshake at 0xD09xx, `sm13` level-runner at
0xD0DFA with 500-tick abort), a redundant mode+~mode latch manager (0xD139C/0xD13C6/0xD140C,
inverted-copy integrity per spec 14 §5a pattern), and a long tail of state predicates and
interface fan-out helpers. Callers are mostly r08_app/r10_app ROM-slot facades (0xB6Axx,
0x103xxx) — the counter side of unit 22's forwarding stubs.

---

#### 0x000D0918 `sm12_ctor` — logic
Constructs the 10-byte state object used by the 0xD093A machine: state[0]=0, flags[2..3]=0,
sub[4]=0; if handed NULL allocates 10 bytes via heap helper 0x0009AA34. callers=- (reached
through a pointer table). [O]

#### 0x000D093A `sm12_step` — logic
12-state handshake machine (switch table at 0xD095C): state 0 waits on subsystem poll
0x000D0D6A and three globals, exiting to 7 (flag 0x20) / 4 (flags 0x80 or 8) / 1; states 2,5,7,
9,0xb consume a result byte into [6..7] from an interface object (+0x24 call) and OR error
bits into the u16 flag word at [2..3]. Illegal state >0xb reports a 14-arg interface event
(0x89,0xf,0,1,3,0x8c) and resets state to 0. Caller 0x000CFFDE. [O]

#### 0x000D0BAA `sm12_cancel` — logic
Abort/cancel path of the 0xD093A machine: states 1→2, 4→5, 8→9 each via an interface call
(PTR_FUN_000d0c08/cb0); states 3/6/10 hard-reset the object ([0]=0,[2..4]=0) plus finalizer
call. Caller 0x000D01E6. [O]

#### 0x000D0C5A `sm12_force_run` — stub
Trivial force-start: state[0]=1, flags[2..3] |= 4. Caller 0x000CFFDE. [O]

#### 0x000D0C66 `sm12_decr_timeouts` — logic
Saturating decrement of the two u16 countdown fields at object +6 and +8 (the +8 field is
armed with 500 by abort path 0x000D1262). Caller 0x000CFEFE (periodic tick). Member 0x000D1240
is a byte-identical copy for the same tick path. Cluster of 2. [O]

#### 0x000D0C88 `sm12_is_state0` — stub
Predicate: object state byte == 0. Called through r08_app facade 0x000B6B8C (ROM slot).
Member 0x000D1238 byte-identical (facade 0x000B6B6C). Cluster of 2. [O]

#### 0x000D0C94 `sm12_phase_of_state` — logic
Maps the state byte of a related machine (values up to 0x11) to a phase class: 1..6→2,
8,9,10,0xf..0x11→5, 0xc..0xe→4, 0,7,0xb→0; any other value fires the 14-arg interface event
(0x89,0xf,0,2,5,8). Caller facade 0x000B6B9A. [O]

#### 0x000D0D1C `sm13_is_active` — logic
Predicate over the 0xD0DFA machine: true for states {1,2,4,5,7,8,9,12,13}. Callers
0x00103C2A, 0x00104A28 (r10_app facades). [O]

#### 0x000D0D4A `sm13_is_state7` — stub
Predicate: state byte == 7. Caller 0x00103F56. [O]

#### 0x000D0D52 `sm13_is_running` — logic
Predicate: state not 0 and not 7. Callers 0x00103F56, 0x00103FD6, 0x00103DE2 (r10_app). [O]

#### 0x000D0D6A `sm12_poll_subsys` — logic
Readiness probe feeding the 0xD093A machine: when global PTR_DAT_000d0dc0==1, queries two
interface flags (bit 0x10 vs bit 1 into flags[2..3]) and pulls a u8 status from the interface
object (+0x24) into result[6..7]; returns 1 if the subsystem was polled, else 0. Caller
0x000D093A. [O]

#### 0x000D0DDC `sm13_ctor` — logic
Constructor for the 0xD0DFA machine's object (10 bytes): NULL→heap(10) via 0x0009AA34; only
state[0]=0. callers=- (pointer table). [O]

#### 0x000D0DFA `sm13_step` — logic
13-state (0..12) level-runner engine: state 0 selects 1/2 from a mode global; 2→3 starts an
interface op (PTR_FUN_000d10e4 on object 0xD10e0); 3 waits for a level in 1..8 (PTR_FUN_
000d10e8) into [5]; 4 latches a u16 result ([6..7], direct or via +0x18 interface); 5 waits
result==0; 6 waits global level == latched; 7 waits op-done; 8 watches for a NEW level and
restarts at 4; error path 0x000D1262 sets state 9 with result and 500-tick timeout [8..9];
9/10 drain result+timeout; 11/12 finish to 0. Illegal state reports (0x86,0xf,0,1,2,0x4d).
Tail selects one of two done/busy interface callbacks (PTR_FUN_000d12cc/d0). Caller
0x000CFFDE. Level domain 1..8 plus the mode manager below suggest a request/position runner
[spec 14 §5a range codes — I]. [I]

#### 0x000D11A6 `sm13_is_midrun` — logic
Predicate: state in {2,3,4,5,6,9,10,11}. Caller 0x00103F56. [O]

#### 0x000D11D0 `sm13_is_working` — logic
Predicate: true unless state in {0,1,11,12}. Caller 0x0010485E (r10_app). [O]

#### 0x000D11EC `sm13_is_advanced` — logic
Predicate: true unless state in {0,1,2,11,12} (i.e. past initial acquisition). Caller facade
0x000B6A98. [O]

#### 0x000D120C `sm13_is_state3` — stub
Predicate: state byte == 3. Caller facade 0x000B6B50. [O]

#### 0x000D1214 `sm13_is_state8_lvl_held` — logic
True when state==8 and the current level read through interface 0xD12a8/0xD12d4 still equals
the latched level [5] (i.e. the level-change retrigger has not fired). Caller facade
0x000B6B5E. [O]

#### 0x000D1262 `sm13_abort_st9` — logic
Error entry for the 0xD0DFA machine: state=9, queries the interface, stores u16 result into
[6..7] (direct +0x1c or interface +0x18), arms [8..9]=500 (tick countdown via 0x000D0C66
family), sets flags [1],[2],[4]=1. Caller 0x000D0DFA. [O]

#### 0x000D12E8 `flags_unacked_report` — logic
Two-byte flag/ack watchdog: bits set in [0] that are not mirrored in [1] fire the 14-arg
interface event (0x7f,0xf,0,1,0,0x9d); afterwards both bytes are cleared. Caller 0x000CFF0E
(periodic). [O]

#### 0x000D132C `flagpair_or` — library
Helper: [0] |= mask; [1] |= mask (set flag + mark-changed pair). Caller 0x000D0DFA. [O]

#### 0x000D133A `flagpair_clear` — library
Helper: [0] &= ~mask; [1] |= mask (clear flag + mark-changed pair). Caller 0x000D0DFA. [O]

#### 0x000D134A `sm13_preconditions_ok` — logic
Returns true only when three interface queries (PTR_FUN_000d1388/1390/1398) return 0,1,1 —
the enable precondition set consulted by the 0xD0DFA engine. [O]

#### 0x000D139C `mode5_ctor` — logic
Constructor of the 5-byte mode object: heap(5) if NULL; [0..2]=0, [3]=6, [4]=0xf9 — i.e.
default mode 6 with its one's-complement (inverted-copy integrity, spec 14 §5a pattern).
callers=- (pointer table). [O]

#### 0x000D13C6 `mode_latch_mirror_init` — logic
Initialization half of the mode manager: copies the current mode byte (PTR_DAT_000d152c[1])
into object [3] and its complement into [4], and mirrors mode, mode+1, max(mode,0), ~mode
into the consumer object *PTR_DAT_000d1530 fields +0x27..+0x2a; zeroes [0..2]. Caller
0x000CFFDE. [O]

#### 0x000D140C `mode_change_apply_notify` — logic
Mode-manager apply path: if the consumer object (PTR_DAT_000d1534[3]) is enabled, latches the
new mode + complement exactly like 0x000D13C6, sets state=1, reads two (old,new) interface
bytes into [1]/[2], fires event (0x9a,0xf,0,1,1,0x1f) when the mode actually changed, then
fans the change out to six subscriber interfaces (PTR_FUN_000d1554/155c/1564/1578, two status
bools from fields [1]&[3], [2]&[3]). Caller 0x000D01E6. [O]

#### 0x000D1588 `mode1_ctor6` — logic
Constructor of a 1-byte mode holder: heap(1) via 0x0009AA34 if NULL; *p=6 (the same default
mode 6 as the 5-byte object of 0x000D139C). callers=- (pointer table). [O]

#### 0x000D15A6 `shift_gear_target_calc` — logic
Top gear-decision entry: fast path copies PTR_DAT_000d17a4 when (g1794!=0 && g179c==1 &&
g17a0==0); otherwise takes the level at g1798 (clamped to 1..8, else mapped -1/0→1); forces
-1 when either source flag == 2; otherwise runs upshift ladder 0x000D1B10 or full map select
0x000D1990. On change (and g17b0==0, counter g17b4!=0) notifies via PTR_FUN_000d17bc.
Caller 0x000CFFDE. Level domain 1..8 = gear/range codes (spec 09 gear decision, spec 14
range codes) [I]. [I]

#### 0x000D165A `shift_chain_select` — logic
Feature-gated chain wrapper: if interface flag 0xD17c4 is set runs the downshift chain
0x000D16BA(cur,req,lim1,lim2); if flag 0xD17c8 set runs the upshift chain 0x000D1854; a final
arbiter (PTR_FUN_000d17cc) may force the result back to the current gear. Callers facade
0x000B6BBC, 0x001048C8. [O]

#### 0x000D16BA `shift_downshift_chain` — logic
Multi-step downshift feasibility loop: while the transition interface returns 2, checks a
mode-dependent limit (table +0x14), two speed/torque window queries (tables +8/+0xc against
param_5/param_4) or an alternate limit (table +0x10 vs global 0xD17f0), then takes the next
gear from the 10-byte-row map at ROM 0x4F6A2-family pointer 0xD17d0 [(next)*10 + cur+1].
Out-of-range result (-1..8, not 0) fires event (0x97,0xf,0,1,3,0xe3) and returns gear-100 as
error code. Callers 0x000D165A, 0x000D1990, 0x001048C8. [O] (spec 09 shift map)

#### 0x000D1854 `shift_upshift_chain` — logic
Upshift counterpart of 0x000D16BA: while allowed, requires speed value (table +0, gear+1)
above global 0xD197c AND torque estimate (table +4) below global 0xD1988, then reads the next
gear from the same 10-byte-row map (0xD1970). Range violation fires (0x97,0xf,0,2,4,0x60).
Callers 0x000D165A, 0x000D1990, 0x001048C8. [O] (spec 09)

#### 0x000D1922 `shift_step_up_while_ok` — logic
Simple ladder: increments the gear while below the limit at PTR_DAT_000d1b90 and the window
query (table +0x10 at (cur+2,cur+1)) stays below global 0xD197c. Caller 0x000D1B10. [O]

#### 0x000D1990 `shift_map_select_gear` — logic
Map-based gear selection: takes the request below current object gear (PTR_DAT_000d1b40[3])
through a limiter interface pair, indexes the 10-byte-row map at ROM 0x4F6A2
([req+1 + (cur+1)*10]), then honors forced -1/+1 steps when the transition and guard
interfaces agree (return 2/4 + flag 1). On a real change runs downshift chain 0x000D16BA
(limits from object +0x94/+0x7e) and upshift chain 0x000D1854, and a final veto pair can
revert to current. Caller 0x000D15A6. [O] (spec 09 shift maps)

#### 0x000D1B10 `shift_upshift_ladder_wrap` — logic
Wrapper: if interface flag 0xD1B70 set and global 0xD1B8C==0, replaces the request with the
ladder result of 0x000D1922 up to limit 0xD1B90. Caller 0x000D15A6. [O]

#### 0x000D1B94 `lerp_u8_conv` — library
Linear interpolation toward a target: y = x*(p4-p3)/p2 + p3 while x<p2 (u8 x), result passed
through converter interface 0xD1DB4 (returns s16). Seven callers across r0c/r10/r14 (e.g.
0x000EE516, 0x00110CDC). [O]

#### 0x000D1BBA `lerp_u16_conv` — library
u16-x variant of 0x000D1B94, same converter interface 0xD1DB4. Callers 0x000F1020,
0x0010DE46. [O]

#### 0x000D1BE0 `lerp_u16_conv2` — library
u16-x interpolation variant feeding converter interface 0xD1DB8 (returns u16). Caller
0x000E3E28. [O]

#### 0x000D1C04 `blend_w2` — library
Two-input weighted average: (w3*x + w4*y)/(w3+w4) with w3 zero-guarded to 1. Cluster of 3
with byte-identical copies at 0x00121BCC (r10_app) and 0x001548E6 (r14_app) — a shared helper
instantiated per module; likely the driver-type blending primitive (spec 09) [I]. [O]

#### 0x000D1C28 `blend_w2_s16` — library
s16 flavor of 0x000D1C04: (w*p2 + n*p1)/(w+n), weight zero-guarded. Callers 0x000B3ADE,
0x000B389E, 0x000C9632. [O]

#### 0x000D1C70 `rampt_f_step` — library
Time-based float ramp: advances *p by rate param_2/DAT_000d1dc4 × (tick_now − last_tick),
where tick_now is the system tick at object 0xD1DC0+0xBC; clamps to [p[2],p[3]]; refreshes
last_tick; converts via interface 0xD1DC8 (s16 out). Caller 0x000D1D96. [O] (spec 02 tick)

#### 0x000D1CB8 `rampt_f_init` — logic
Initializes the float-ramp state: value=p2, min=p3, max=p4, last_tick=current tick (global
0xD1DBC). Callers 0x00107D90, 0x000E9CE0. [O]

#### 0x000D1CDC `rampt_f_step2` — library
Float ramp identical to 0x000D1C70 but converting through interface 0xD1DCC (u16 out).
Callers 0x000EAA9A, 0x00107BD2. [O]

#### 0x000D1D24 `rampt_q7_init` — logic
Initializes a Q7 (×128) fixed-point ramp state: value=p2<<7, last_tick=global 0xD1DBC,
target=p3<<7 at [4]. Caller 0x000E9CE0. [O]

#### 0x000D1D38 `rampt_q7_step` — library
Q7 ramp step: new target p3<<7; advances *p by rate[4]×(tick delta)/1000, clamps to
[[2],[3]], refreshes tick, converts via interface 0xD1DB8 (value>>7, u16 out). Caller
0x000EA774. [O]

#### 0x000D1D96 `rampt_f_step_pct` — stub
Thin wrapper: rampt 0x000D1C70(p, rate×100). callers=- (pointer table). [O]

#### 0x000D1DA4 `interp_q7u8_init` — logic
Progress-interpolator init (Q7 ×128, u8 progress): value=p3<<7, progress=p2 at byte [1].
Callers 0x000D3D12, 0x000D326C, 0x001132F4, 0x0010FFBE. [O]

#### 0x000D1DD0 `interp_q7u8_step` — logic
Interpolates from the stored Q7 value toward target p4×0x128 by progress fraction
(p2−last)/(p3−last) while p2<p3 and last<=p2; stores value+progress; returns value>>7.
Callers 0x00110CDC, 0x00113F16, 0x000D3D12, 0x000D326C, 0x000EE38C. [O]

#### 0x000D1E18 `interp_q7u16_init` — logic
Q7 interpolator init with u16 progress field. callers=- (pointer table). [O]

#### 0x000D1E26 `interp_q7u16_step` — logic
u16-progress variant of 0x000D1DD0. callers=- (pointer table). [O]

#### 0x000D1E6E `rampt_milli_init` — logic
Ramp init with ×1000 (milli) scale and u16 tick source 0xD1F9C: value=p2×1000, last=tick.
Callers 0x000D4CC4, 0x000EEDFC, 0x000EEEA8, 0x000EECEC, 0x00115F6E. [O]

#### 0x000D1E80 `slew_milli_step` — library
Milli-scale (×1000) slew toward target p2×1000: elapsed ticks capped to window p3, step
shaped by helper interface 0xD1FA8 (time constant DAT_000d1fa4 / delta), partial-step
arithmetic, tick refresh from object 0xD1FA0+0xBC, output value/1000 via converter 0xD1FAC.
Callers 0x000D4CC4, 0x000EF448, 0x000EF4B2, 0x000EF138, 0x000EF2BA, 0x00115F6E. [O]

#### 0x000D1F06 `slew_milli_init_b` — logic
Slew-state init with tick source 0xD1F9C: value=p2×1000, last=tick (second instance of the
0x000D1E6E pattern). Caller 0x000EA33E. [O]

#### 0x000D1F18 `slew_milli_step2` — library
Variant of 0x000D1E80 with u16 target and converter 0xD1FB0 (u16 out). Callers 0x000EA33E,
0x000E9CE0. [O]

#### 0x000D1FB4 `fade_pct_step` — library
Timed 0..1000 fader: advances the progress word at [2..3] by tick delta scaled by
DAT_000d20bc/remaining (tick source 0xD20B8), saturates at 1000 and clamps at 0 (resetting
direction flags [2]/[3]); returns base[p0] + (p2−base)×progress/1000 through converter
0xD20C4. Caller 0x000E65F4. [O]

#### 0x000D2038 `fade_pct_init` — logic
Fader init: base=p2, progress=0, tick=now (0xD20B8). Callers 0x000E65F4, 0x000E6210. [O]

#### 0x000D2046 `slew_milli_init_c` — logic
Slew init on the 0xD20B8 tick source: value=p2×1000. Caller 0x001179D8. [O]

#### 0x000D2058 `slew_lin_step` — library
General linear-rate slew: step = (target×1000 − value)/window through converter 0xD20C4,
value += step×elapsed (tick object 0xD20C8+0xBC), tick refresh. Ten callers including
0x000D2154, 0x000D2212, 0x000B1E54, 0x001179D8. [O]

#### 0x000D20CC `edge_rise_store` — library
Rising-edge detector with storage: returns 1 only when old==0 and new==1, then latches p2.
Callers 0x00116A20, 0x0011577C. [O]

#### 0x000D20E0 `edge_fall_store` — library
Falling-edge counterpart of 0x000D20CC (old==1, new==0). Callers 0x000EA230, 0x00116A20. [O]

#### 0x000D20F4 `feq_within_tol` — library
Float equality within tolerance: |p2−p1| < max(p3, 1.0). Caller 0x00109816. [O]

#### 0x000D2114 `seq_within_tol` — library
s16 equality within tolerance (deadband), tolerance floored at 1; equal values always match.
Callers 0x000E2204, 0x000E22D8. [O]

#### 0x000D2140 `slot_fwd_d2150` — stub
Pure ROM-slot forwarder: tail-calls PTR_FUN_000d2150(p2). Callers 0x000B81C0, 0x0010CBB8. [O]

#### 0x000D2154 `ramp_tbl_ae78` — logic
Ramps the RAM state at 0xFFF8AE78 (slew family 0x000D2058) toward the calibration-table
value at object 0xD2278 slot +0x10 indexed by p2. Caller facade 0x000B7AE0. [O]

#### 0x000D2170 `pct_of` — library
Percentage helper: (p2×1000)/p3 through converter 0xD2280; 1000 when p3<=0. Caller facade
0x000B7B2E. [O]

#### 0x000D2192 `blend_runin_w01` — logic
Blend with run-in flag: first invocation passes p2 through and sets the flag; afterwards
blends p2 with p3 using weights from object 0xD2278 bytes +0/+1 (via blend 0x000D1C04
family). Caller facade 0x000B7AF0. [O]

#### 0x000D21C2 `blend_runin_w8c` — logic
Variant of 0x000D2192 with weights from object 0xD2278 slots +8/+0xC (u16). Caller facade
0x000B7B02. [O]

#### 0x000D21F2 `tbl_scale_pth` — library
Table lookup (interface 0xD2288) scaled by p3/1000 through converter 0xD228C. Callers
0x000D2570, 0x000D22E0, facade 0x000B7B1C. [O]

#### 0x000D2212 `ramp_tbl_ae80` — logic
Ramp of RAM state 0xFFF8AE80 toward table slot +0x14 of object 0xD2278 (pre-converted by
0xD2290). Caller facade 0x000B7AD0. [O]

#### 0x000D2234 `ramp_tbl_ae88` — logic
Same pattern as 0x000D2212 on RAM state 0xFFF8AE88, table slot +0x18. Caller facade
0x000B7AC0. [O]

#### 0x000D2256 `ramp_tbl_ae90` — logic
Same pattern on RAM state 0xFFF8AE90, table slot +0x1C. Caller facade 0x000B7AB0. [O]

#### 0x000D2294 `set_flag_e6a4` — stub
Sets global DAT_fff8e6a4 = 1. Caller facade 0x000B7B14. [O]

#### 0x000D229E `flag_e6a4_gated_call` — logic
If object (0xD22D0) byte +1 is 0 and the flag DAT_fff8e6a4 set, forwards byte +0x96 of that
object to interface 0xD22D8 — a one-shot, armed-when-idle notification. Caller facade
0x000B7E66. [O]

#### 0x000D22BE `read_byte_via_slot` — data-accessor
Reads a byte through interface 0xD22DC into *p. Caller facade 0x000B7E5E. [O]

#### 0x000D22E0 `blend_transition_convert` — logic
Two-source transition blender on object 0xD2528: takes the raw value (slot 0xD252C) or
converts it via 0x000D2492(mode 0x28) + factor scale 0x000D244C(mode 0x27); while a mode
change is in flight (fields 0x21/0x27 or 0x2B/0x2C differ, or latch flag 0xD2530), computes
new value p[2] and old-mode value p[1] (converter 0xD253C with per-mode factor +0x7C/+0x7E)
and blends them by elapsed fraction, holding the blend anchor at 0xD2540. Final clamp to a
max byte via object 0xD254C. Caller 0x000D280C. Cross-shift value blending (spec 09/11
driver-type blending context) [I]. [I]

#### 0x000D244C `scale_by_modefactor` — logic
Applies a per-mode percentage: for modes 1/2 multiplies p2 by lookup(0xD2550, mode)/1000
through converter 0xD2548; other modes pass through. Caller 0x000D22E0. [O]

#### 0x000D2492 `tbl_gear_convert` — logic
Gear-dependent conversion: maps mode byte p2 through ROM index table 0xD2554 (validated 0..8,
else a 6-arg range-violation report with the module id word at 0xD2558 and fallback row),
picks the row from the 2-D table at 0xD255C (object 0xD2528 byte 0 selects the set), applies
two curve interfaces (fields 0x18/0xA and table 0xD254C+4 with field 0xF), multiplies /100
and adds the base value at slot 0xD252C. Caller 0x000D22E0. [O]

#### 0x000D2570 `blend_transition_convert2` — logic
Second instance of the 0x000D22E0 transition blender on objects 0xD27C0/0xD27C4, with the
gear-table conversion inlined (index table 0xD27C8, 2-D 0xD27D0): converts old-mode (0x21)
and new-mode (0x27) values through the same tables, blends by elapsed fraction with anchor
0xD27F0, applies boost curve 0x000D2774, clamps to the byte max at object 0xD27FC. Caller
0x000D280C. [O]

#### 0x000D2774 `scale_boost_curve` — logic
Boost scaling above a base: when p2 > p3, looks a factor through table (0xD27FC+4 / slot
0xD2800) and returns ((factor×p2/100 − p3)×100)/global 0xD2808 + p3. Caller 0x000D2570. [O]

#### 0x000D280C `calc_request_value` — logic
Triggers a background computation through two interfaces (compute + completion check) when
the state flag 0xD2830 != 1, then copies the u16 result slot 0xD2834. Caller facade
0x000B7EAE. [O]

#### 0x000D2844 `cond_speed_window_check` — logic
Two-group feasibility check on four speed-style inputs (object 0xD2AC8 slots +0x3C/+0x40/
+0x44/+0x48) against thresholds (0xD2ACC fields 0x96/0x98 plus hysteresis margins +0x4C/
+0x50): mode p2==2 forces "blocked" (1), p2==0 forces evaluate; each group compares the
live value (interface 0xD2AD4) against window tables +0x20/+0x1C with a slip-corrected
bound (0xD2AD8). Returns 0 (window ok) / 1 (blocked) / 2 (second group blocked). Caller
facade 0x000B7CAA. [O]

#### 0x000D2982 `cond3_speed_window_check` — logic
Three-window variant of 0x000D2844: copies candidate values into p[2]/p[4]/p[6] (slots
+0x54/+0x28/+0x30), checks windows at +0x14/+0x18 and object 0xD2D20 +0x34/+0x38 with
hysteresis; combines into a 0..3 code from the p2 bit pattern. Caller facade 0x000B7CBA. [O]

#### 0x000D2B8A `limit_by_table_clamp` — logic
Computes a protective limit: validity flag on fields +0x44/+0x48 (hysteresis vs 0x96/0x98)
into 0xD2D38; when valid, limit = gear-table lookup (row byte 0x22, table stack 0xD2D3C/
0xD2D44/0xD2D48) of max(0x96,0x98); else 0x7FFF (no limit). The requested p2 is then
clamped between lower bound 0xD2D58 and the limit with margins from object fields +8/+0xC
(slew-converted via 0xD2D5C). Caller facade 0x000B7CCA. [O]

#### 0x000D2CF0 `limit_by_table_clamp2` — logic
Second instance of the 0x000D2B8A limiter on object 0xD2D20 with window +0x1C, table slot
+4 and base margins from object fields [0]/[1]; limit 0x7FFF when the window is not met;
clamps through converter 0xD2E84. Caller facade 0x000B7CDA. [O]

#### 0x000D2E88 `scale_pair_lookup` — library
Multiplies p[0] by table lookup (0xD3058+4, key p[1]) / 10000 through converter 0xD3060.
Caller facade 0x000B7E40. [O]

#### 0x000D2EB0 `calc_ratio_sq_term` — logic
Stores ratio (p3×10000)/p2 (floored at 1) into p[1]; computes a squared term: lookup at
object+0xC / object header, scaled by (p2²·25/10000)/10000 into p[0] — a speed² (centrifugal)
contribution. Caller facade 0x000B7E2A. [I]

#### 0x000D2F14 `calc_slip_curve_filter` — logic
Float rational-curve computation: x = table(+0x94, obj+0xC)/const, y = table(+0x94, obj+8);
denominator x⁴ + x²·(c1 + c2·y²) + 1; numerator 2·x·y·(x²+1) scaled by DAT_000d3084 — a
slip/capacity characteristic; feeds a 10-tick limiter (0xD308C/0xD3090), a second-order
filter (0xD3098 with pole DAT_000d3094) into p[2], and the speed² term into p[0]. Caller
facade 0x000B7E4E. [I]

#### 0x000D30A4 `obj6_ctor_inverted` — logic
Constructor of a 6-byte redundant pair object (value+inverted copy): u16@0=0, u16@2=0xFFFF,
byte@4=0, byte@5=0xFF; heap(6) via 0x0009AA34 when NULL. callers=- (pointer table). [O]

#### 0x000D30CE `obj6_update_inverted` — logic
Refreshes the redundant pair: copies the u16 at 0xD3100, queries two interfaces into the
byte@4 status, then writes complements p[1]=~p[0], byte@5=~byte@4 (inverted-copy integrity
check pattern, spec 14 §5a). Callers facade 0x000B7EB6. [O]

#### 0x000D3110 `obj40_ctor_defaults` — logic
Constructor of a 0x40-byte parameter object: pressure-style defaults 0x514 (1300) at +0xA/
+0xC/+0xE/+0x1C/+0x20/+0x22, 0xFF bytes at +4/+8/+0x14, 0xFFFF at +0x12, zeros at +0x38/
+0x3A; heap(0x40). callers=- (pointer table). [O]

#### 0x000D315A `reg_fields_to_obj` — logic
Registers fourteen fields of p1 (offsets +4,+8,+0x12,+0x16,+0x18,+0x2C,+0x2E,+0x30,+0x32,
+0x34,+0x36,+0x3E) into the target object 0xD3370 through its u16/byte setter interfaces —
parameter-record publication (measurement protocol registration, spec 10) [I]. Caller
facade 0x000B7E6E. [I]

#### 0x000D31D0 `calc_base_target` — logic
Base-target computation: reads a byte and a word from interface 0xD337C, runs the windowed
target calc 0x000D38B4, clamps/resets via 0x000D385C, subtracts the offset word at 0xD3388
into p[1]. Caller facade 0x000B7E8E. [O]

#### 0x000D3208 `dir_vs_threshold` — logic
Direction decision against the threshold at object 0xD338C+0x6C: above → 1 (only when mode
flag interface 0xD3394 agrees), below → 2; a veto callback pair (PTR_PTR_000d3398/39C) can
cancel the "above" result to 0. Caller 0x000D30CE. [O]

#### 0x000D326C `shift_limit_cascade` — logic
525-instruction shift torque/pressure cascade. Lazy RAM init (DAT_fff8e6a5/e6a6 first-run
latches); when a shift is pending (obj 0xD33A4 mode [0x2C] in {1,2} differing from [0x2B])
it re-evaluates preconditions (interfaces 0xD33B4/0xD33C0), sets phase p[0x10] to 1/2/4/5,
computes the phase setpoint p[6] from gear-table lookups (row byte 2, index byte 0x22 via
0xD3650/0xD3654, validated 0..8) with event report (0x114,0xf,0,5,0xE,0x11) for phase 3, and
scales p[0xE]=p[0xC] by byte(+0x18)/0xFF; otherwise (no shift) it recomputes the base p[0xA]
from table candidates (0xD363C/0xD38D4 +0x28 rows, min 0x514 or the 0xD38D8 table), slews
p[0xE] via the RAM interpolator DAT_fff8af10. Both paths then cascade 0xD3D12 → 0xD4030
(min with limit 0xD38F8) → 0xD3A6A (slip allowance) → final min with table +0x20; failure
flag 0xD3CA2 forces 0x1E. Caller facade 0x000B7B6C. Spec 11 pressure-parameter family
context [I]. [I]

#### 0x000D37BC `flag_cond_setclr` — logic
Sets the flag byte *DAT_000d3904 when object field +0x25 or interface 0xD3900 says 1,
clears it only when both are 0. Caller facade 0x000B7B86. [O]

#### 0x000D37FE `limits_combine3` — logic
Combines three limits through interface 0xD390C (min-style): f(0, a), then f(a+b, c) — a
three-way limit merge over interfaces 0xD3908/0xD390C/0xD3918. Caller facade 0x000B7C58. [O]

#### 0x000D3830 `cond_two_queries` — logic
Returns 1 unless both interface queries (0xD391C over 0xD38E0, and 0xD3920 over 0xD3914)
return 0. Caller facade 0x000B7C4A. [O]

#### 0x000D385C `clamp_base_reset` — logic
Post-clamp resets: forces p2 to the fixed word 0xD3910 when the object 0xD38D4 header byte
is set and its +1 word <= p[0x30]; clears byte[0x32] below the +0x6C threshold; resets
p[0x30]=0 when byte(+8) <= p[0x32] or u16(+0x7C) is exceeded. Caller 0x000D31D0. [O]

#### 0x000D38B4 `calc_target_windows` — logic
Windowed target computation: base = (+0x6C threshold of the selected 0xD3924/0xD3B5C
object) + p3 when mode p2==1, overridden by a chain of interface conditions (0xD3B64/68/70)
and a negative-side correction (0xD3B7C), capped at +0x6C under the veto callback
(PTR_PTR_000d3B80/84), converted by 0xD3B88. Caller 0x000D31D0. [O]

#### 0x000D3A0A `calc_slip_allowance` — logic
Allowance from a speed-like word (+0x8A of 0xD3B5C): v = lookup(x) × u16(+0x80); large
values rescaled by ×0x417/10⁷; clamped to the byte limit at +0x7C and converted by 0xD3B94.
Caller 0x000D3A6A. [I]

#### 0x000D3A6A `add_slip_allowance` — logic
Adds the slip allowance (0x000D3A0A) to the running value only when fully settled: target
mode [0x2B]==[0x2C], flag 0xD3B9C clear, object bytes +0x1A/+0x1E clear and check
0xD3CA2 == 0; stores the allowance at +0x3C and marks overflow (*0xD3BA0=6) on wrap.
Caller 0x000D326C. [O]

#### 0x000D3AC8 `phase45_limits_min` — logic
Phase-dependent limit pair for phases 4/5 of the 0xD326C cascade: phase 5 takes the minimum
of 0x000D44D8(gear row, p[6]) and 0x000D4534(gear row, p2, p[0x2E]); phase 4 of
0x000D44A4(...) and 0x000D4558(..., p[0x2C]); other phases yield 0. Gear rows come from the
index stacks 0xD3BA4/0xD3E04 (validated 0..8 with the 6-arg range report). Caller
0x000D326C. [O]

#### 0x000D3CA2 `chg_detect_counter_ok` — logic
Change watchdog: queries interface 0xD3E14, compares against the stored prev pair at
0xD3E18 (vs 0xD3E1C), clears the object counter byte +4 on any change, refreshes the pair,
and returns counter < 0x65 (100). Callers 0x000D326C, 0x000D3A6A. [O]

#### 0x000D3CE6 `scale_byte_ratio` — library
Scales p2 by byte(object 0xD3E20 +0x18)×p3×100/(0xFF×p3) through converter 0xD3E24.
Callers 0x000D3D12, 0x000D326C. [O]

#### 0x000D3D12 `clutch_fill_phase_sm` — logic
Three-state engagement engine on p[0x37] (0 idle / 1 filling / 2 engaged): computes the
target delta p[0x28] = u16(+0x7E) − u16(+0x7C)×lookup(0xD3E1C)/1000; resets p[0x3E] on the
fill-reset condition 0x000D3F78 or speed windows (+0x40/+0x4C); idle→fill captures
p[0x3A]=u16(+0x64), zeroes the RAM timer 0xD4070, steps counter p[0x26], starts timer
interface 0xD407C and evaluates the fill table (+0x38); fill→engaged on condition
0x000D3FF6 (+window +0x44) with the RAM interpolator DAT_fff8af18 re-armed; engaged→idle
when byte(+0x48) <= p[0x36]. Output p[0x38] via scale 0x000D3CE6; marks *0xD4088 = 3 when
below request. Caller 0x000D326C. Clutch-fill phase engine, spec 11 [I]. [I]

#### 0x000D3F78 `clutch_fill_reset_cond` — logic
Returns 1 (reset the fill state) unless every condition holds: mode pair [0x2C]/[0x2B] both
1, gear [0x21]==[0x27], table(+0x3C) lookup above p[0x28], interface 0xD40A0 not 1, flags
0xCD/0xCC clear and p[0x24] != 1. Caller 0x000D3D12. [O]

#### 0x000D3FF6 `cond_not_settled1` — logic
Returns 1 unless the object 0xD408C is settled in mode 1 with equal gears and flag 0xD40A8
clear — the "still moving" half of the fill→engaged gate. Caller 0x000D3D12. [O]

#### 0x000D4030 `limits_min_stage2` — logic
Second min stage of the cascade: returns min(0x000D42CC(), 0x000D40AC(p,p2,p3)) and keeps
p[0x20] = min(p[0x1C], p[0x22]). Caller 0x000D326C. [O]

#### 0x000D40AC `lockup_engage_sm` — logic
225-instruction engagement state machine on p[0x1A] (0/1/2): arm condition combines settled
mode {1,2}, speed byte (+0x54 vs 0x9C), gear-row table (+0x64 < +0x94), window (+0x7E <
+0x6C) and enable flags 0xC2/0xC6(+0x24); 0→1 on the p[0x12]/p[0x16] windows (+0x70/+0x74),
1→2 on the release condition, 2→0 when request <= floor. State 1 scales p2 by table
(+0x58 row byte 2, key via 0xD42C0)/0xFF into p[0x1C] (resetting integrator p[0x12]);
state 2 returns min(table(+0x60)+p3, p2); idle passes p2 with default 0x514 bookkeeping.
Marks *0xD42C8 = 4 at completion. Caller 0x000D4030. Converter-lockup engagement [I]. [I]

#### 0x000D42CC `lockup_engage_sm2` — logic
Parallel twin of 0x000D40AC on state byte p[0x1E] with its own counter window (+0x84),
tables +0x88 (+scale /0xBF byte) and +0x60 floor, default 0x514, completion flag
*0xD4520 = 5. Caller 0x000D4030. [O]

#### 0x000D4482 `cond_flag_lt3` — logic
Returns 1 when interface 0xD4528 is non-zero and the byte at 0xD452C is below 3. Caller
0x000D326C. [O]

#### 0x000D44A4 `tbl_eval_chain8` — logic
Table-object dispatcher: when the table descriptor at +8 is not the self sentinel, evaluates
it with the fixed key word (object 0xD450C +0xC); otherwise evaluates the 2-D table at
param_1 (row = object byte 2) with p2. Caller 0x000D3AC8. [O]

#### 0x000D44D8 `tbl_eval_chainC` — logic
Same chained-table dispatch as 0x000D44A4 using the descriptor at +0xC. Caller 0x000D3AC8.
[O]

#### 0x000D4534 `tbl_eval_chain10` — logic
Chained-table dispatch through the descriptor at +0x10 (converter 0xD457C), self-recursive
on the sentinel path. Cluster of 2 with the byte-identical copy 0x000D4558 (descriptor
+0x14); both called by 0x000D3AC8 for the phase-4/5 limits. [O]

#### 0x000D4580 `scale_lookup_pct` — library
p2 × lookup(header table 0xD4848, byte 0xD484C)/100 through converter 0xD4854. Caller
facade 0x000B7BA0. [O]

#### 0x000D45A8 `scale_byte_pct` — library
p2 × byte global 0xD4858 / 100 through converter 0xD4854. Caller facade 0x000B7BB0. [O]

#### 0x000D45C2 `tbl_blend_delta` — logic
Gear-indexed delta blend: stores p[0]=p3−p2, looks the delta up in the gear row (index
stack 0xD4860/0xD4868, validated 0..8), converts to a 0..0xFF weight (0xD4878) and returns
(p3×(0xFF−w) + p2×w)/0xFF. Caller facade 0x000B7BC0. [O]

#### 0x000D464C `tbl_blend_delta2` — logic
Direct-table delta blend using the s16 weight from table 0xD4848+4: (p3×(0xFF−w) +
w×p2)/0xFF. Caller facade 0x000B7BD2. [O]

#### 0x000D4684 `slew_tbl_af24` — logic
Slews the RAM state 0xFFF8AF24 toward the table (+0x14 of object 0xD4848) value for p2 —
the 0x000D2058 slew family. Caller facade 0x000B7BE4. [O]

#### 0x000D46A0 `calc_power_torque_diff` — logic
Power-balance torque computation: for the two speed words +0x88/+0x8A it evaluates ω×M
terms — constants 0x188B (2π×1000) over 60000 with a ×10 branch above 5215 — times table
lookups (+0x10/+8 with the 0xBF percent byte), then a difference scaled by the gear lookup
(mode 0x27) and +0x6C×0xBF/100 into p[2]; bypass value from table +0xC when flags
0xCA/0xCD/0xCE are 2. Caller facade 0x000B7E96. The 2π/60 identity makes this a
torque-from-power calculation [I]. [I]

#### 0x000D4890 `obj18_ctor` — logic
Constructor of a 0x18-byte object clearing only the dword at +0x14; heap(0x18) via
0x0009AA34. callers=- (pointer table). [O]

#### 0x000D48AE `tc_state_sm` — logic
Three-state torque-converter machine on p[1] (0/1/2) gated by mode pair [0x2B]/[0x2C]==2
and equal gears: idle clears outputs to 0x514/0; state 1 (prep) runs a ramp into p[5]
(×10 into *p), takes the +0xC speed, delta against base 0xD4AAC, gear-table lookup
(row 0xD4AB8) into p[6]; state 2 does the same on +8, requires interface 0xD4A98==1, then
blends p[0xE] = (p[0xE]×1000 + *p)/1000 through 0xD4AC0 and re-queries status. Caller
facade 0x000B7B7E. [I]

#### 0x000D4AC8 `calc_sum_clamp_conv` — logic
Unless inhibit flag *0xD4B34: refreshes two interface values, sums them, clamps to
[−1200, 1200] via 0xD4B4C, and converts into p[0] when the enable byte (object 0xD4B50) is
clear. Caller facade 0x000B7E9E. [O]

#### 0x000D4B18 `cond_refresh_conv` — logic
When the enable byte (object 0xD4B50) is 1, refreshes p[0] from the stored value at p+2
through converter 0xD4B54. Caller facade 0x000B7EA6. [O]

#### 0x000D4B58 `calc_div300_refresh` — logic
Selects the +0xAE/+0x94 word (0x000D4B8A), runs the external computation 0x000D4BA8,
converts (0xD4BEC) and stores through interface 0xD4BF4 with the byte at +8; output p[0] =
value/300. Caller 0x000D4AC8. [O]

#### 0x000D4B8A `sel_word_ae94` — logic
Selects the u16 at object-0xD4BF8 offset +0xAE or +0x94 depending on the byte at +4, into
p[2]. Caller 0x000D4B58. [O]

#### 0x000D4BA8 `calc_scaled_product` — logic
External computation used by 0x000D4B58: takes the u16 at p+2 and an interface value
(PTR_FUN_000d4c00), combines both through an interface call on obj+0xc (PTR_FUN_000d4c04),
then scales by an s16 interface factor (PTR_FUN_000d4c0c) with /1000. Pure interface
arithmetic, no globals written. Caller 0x000D4B58. [O]

#### 0x000D4C12 `validity_and_scaled_value` — logic
Computes a validity byte p[0] (1 unless status interface clear, flag 0x000d4c98 clear, and
condition 0x000D4C52 false) and, when the enable byte at 0x00105008 is clear, a chained
four-lookup product (each /100) of interface values from the 0x00104ffC object block,
converted through 0x00105034 into the u16 at 0x000d4c9c. Caller 0x000D4AC8. [I]

#### 0x000D4C52 `cond_val_le_idle` — logic
Predicate: 1 iff the u16 behind the interface object (0x000d4ca4 deref) is <= the limit at
0x000d4ca8 and the interface query (0x000d4cb0) returns 0; result latched to 0x000d4cb4.
Caller 0x000D4C12. [O]

#### 0x000D4CB8 `fwd_set_arg2` — stub
Pure forwarder: single interface call (0x000d4d8c) with param+2. Caller facade 0x000B7E76. [O]

#### 0x000D4CC4 `slew_position_hysteresis` — logic
Hysteresis latch on a u16 pair from object 0x000d4d90 (sets global byte 0x000d4d98 low/high).
When latched low: selects target word (obj slot +0x10 or +0x14) by byte p[1], clamps rate
byte between obj slots +0x18/+0x1c, and slews RAM DAT_fff8af58 toward the target via
0x000D1E80/0x000D1E6E; output p[0] and return byte are ±0x80 sign-adjusted. Caller facade
0x000B7C8A. [I]

#### 0x000D4DA4 `obj5_ctor` — logic
Ctor of a 5-byte object clearing only byte +4; heap(5) via interface (PTR_FUN_000d4f40,
the 0x9AA34 facade). callers=- (pointer table). [O]

#### 0x000D4DC2 `quad_cond_state_sm` — logic
Four-condition coordinator on bytes p[0..3]: evaluates 0x000D4E9E→p[0], 0x000D4EC0→p[1],
0x000D4F02→p[2], 0x000D4F24→p[3], then a 0..3 state machine on p[4]: 0→1/2/3 by conditions;
1→2 when p[0]/p[2] set; 2→1 when all clear, →3 when p[1]/p[3] set; 3 holds while any of
p[0]/p[2]/p[3] set. Illegal state resets to 3 with event (0x11a,0xf,…,0xd3). Caller facade
0x000B7C66; predicates 0x000D4E8E/0x000D4E96 read states 1/3. [O]

#### 0x000D4E8E `quadcond_is_state1` — stub
Predicate: quad-cond coordinator state byte (+4) == 1. Caller facade 0x000B7C6E. [O]

#### 0x000D4E96 `quadcond_is_state3` — stub
Predicate: quad-cond coordinator state byte (+4) == 3. Caller facade 0x000B7C7C. [O]

#### 0x000D4E9E `cond_lt_limit_94` — logic
Condition p[0] for 0x000D4DC2: 1 when the interface u16 (field +4 of obj 0x000d4f4c) is
below the table limit at +0x94 of obj 0x000d4f50, or override byte +0xca is set. [O]

#### 0x000D4EC0 `cond_sum_lt_limit_94` — logic
Condition p[1] for 0x000D4DC2: 1 when (s16 at field +8 + u16 at field +4) < limit +0x94
(or override +0xca), or override set and mode byte +0xa0 is 5 or 7. [O]

#### 0x000D4F02 `cond_lt_limit_7e` — logic
Condition p[2] for 0x000D4DC2: 1 when the interface u16 (field +0xc of obj 0x000d4f4c) is
below the limit at +0x7e of obj 0x000d4f50, or override byte +0xcd is set. [O]

#### 0x000D4F24 `cond_iface_eq1` — stub
Condition p[3] for 0x000D4DC2: stores (interface query 0x000d4f58 == 1) into p+3. [O]

#### 0x000D4F5C `obj3_ctor_ff` — logic
Ctor of a 3-byte object setting byte +2 = 0xFF; heap(3) via interface (PTR_FUN_000d51c4).
callers=- (pointer table). [O]

#### 0x000D4F7A `fwd_set_arg2__2` — stub
Same forwarding pattern as 0x000D4CB8 on a different interface pair (0x000d51cc/0x000d51c8).
Caller facade 0x000B7E7E. [O]

#### 0x000D4F86 `tbl_gearpair_lookup1` — data-accessor
Returns a byte from a 10x10 ROM matrix (via 0x000d51d0) indexed by two -1..8 gear-style
arguments. Caller facade 0x000B7C02. [O]

#### 0x000D4FA0 `tbl_gearpair_lookup2` — data-accessor
Second gear-pair matrix lookup, identical shape via 0x000d51d4. Caller facade 0x000B7C14. [O]

#### 0x000D4FBA `shift_pair_torque_check` — logic
Gear-pair shift check: validates both -1..8 args (else event 0x118/0xd), reads shift-type
code from 10x10 matrix 0x000d51d8; for codes 2/3 loads a u32 from the pair matrix
(stride 0x24) into the RAM pair object (0x000d51e0), bounds-checks 0..0x11 (event via
0x000d51f0, else pointer table 0x000d51e8 stride 8). Sums one or two interface torque
values (slots +0x10/+0x14/+0x1c/+0x20 of obj 0x000d51fc), applies ±s16 at +0x18 by
param_4 direction (0 add / 1 sub / 2 none, else event), returns sum <= global 0x000d5204.
Caller facade 0x000B7C26. [I]

#### 0x000D5174 `gear_transition_window` — logic
Compares the latched gear byte at 0x000d5208 with the target at 0x000d51e4[+0x27]: equal or
either -1 returns 0 (forced when target == byte[+0x21] and pass-through byte p+2 > 9);
during a transition inside the window (between target and limit byte 0x000d520c, either
direction) returns 1 and resets byte p+2; finally latches the global to the target.
Caller facade 0x000B7C9A. [I]

#### 0x000D522A `tbl_check_default1` — logic
Converts param_2 through an interface (0x000d53e8), combines it with the field at obj
0x000d53e4+0x24 and the u16 at 0x000d53ec via interface 0x000d53f0, /10, then feeds the
result to the checker at 0x000d53f4; returns 1 when the checker returns 0 (default-allow).
Caller facade 0x000B7C3A. [O]

#### 0x000D525C `mode_code_lookup_ovr` — logic
Picks a byte from the 24-entry table at 0x000d5408 indexed by (cond1==0) + param_3*2 +
param_2*6, then overrides to 1 when interface status (0x000d5404) is 1 and to 3 when it
is 2. Returns a 0..3 mode/level code. Caller facade 0x000B7CEA. [O]

#### 0x000D52CA `speed_window_flags` — logic
Sets flag pair p[0]/p[1] from the shared speed object 0x000d53e4: p[0]=1 when enabled and
(speed u16 at +0x28 <= param_2 or byte at +0x34 clear) or byte at +0x30 set with param_4;
p[0]=0 when disabled or speed <= (speed - u16 at +0x2c) and byte +0x34 set; p[1] mirrors the
same conditions. Classic hysteresis band flag pair. Caller facade 0x000B7CFC. [O]

#### 0x000D5368 `cond_tbl_lt_obj0` — logic
Predicate: u16 at 0x000d540c < the first u16 behind the shared object 0x000d53e4.
Caller facade 0x000B7BF4. [O]

#### 0x000D537A `get_s16_fld4` — data-accessor
Returns the s16 behind field +4 of the shared interface object 0x000d53e4. Caller facade
0x000B7D1E. [O]

#### 0x000D5384 `get_s16_fld8` — data-accessor
Same getter on field +8 of object 0x000d53e4. Caller facade 0x000B7E00. [O]

#### 0x000D538E `get_status_bitmask5` — data-accessor
Queries interface (0x000d5410/0x000d5414) with selectors 1..5 and ORs the ==1 results into
a bitmask (bits 0,1,2,3,4). Caller facade 0x000B7D10. [O]

#### 0x000D5418 `reg_8_fields` — logic
Calls interface 0x000d54c4 once bare, then with the eight field addresses param+6, +0xc,
+0x12, +0x18, +0x1e, +0x24, +0x2a, +0x30 — registering eight sub-fields of a param object.
Caller facade 0x000B7EBE. [O]

#### 0x000D54D0 `ringf_ctor` — library
Ctor of a 16-byte float ring-buffer head {full=0, idx=param_1, size=param_1, buf=param_3};
heap(0x10) via interface (0x9AA34 facade). Used by the r10_app trace module (0x00105184). [O]

#### 0x000D5500 `ringf_reset` — library
Float ring-buffer reset: clears the full flag and sets write index back to size (empty).
Caller 0x001051BE. [O]

#### 0x000D550E `ringf_push` — library
Float ring-buffer write: advances the (float-typed) write index, wraps at size, sets the
full flag at size-1, stores the value at buf[idx & 0xff] (max 256 entries). Callers
0x001051BE, 0x000BC092. [O]

#### 0x000D5548 `ringf_get_history` — library
Reads the element param_1 positions behind the write index (wrapping), only when the full
flag is set or range predicate 0x000D559C passes. Caller 0x001051BE. [O]

#### 0x000D559C `ringf_in_range` — library
Range predicate for the float ring buffer: 1.0 when full or 0 <= size - offset (read
position valid). Callers 0x000D5548, 0x001051BE. [O]

#### 0x000D55C4 `ringu16_ctor` — library
Ctor of the 8-byte u16 ring-buffer head {full=0, idx=param_3, size=param_3, buf=param_2};
heap(8) via interface. Callers 0x001050C4, 0x000D56A8. [O]

#### 0x000D55EE `ringu16_reset` — library
u16 ring-buffer reset: full flag 0, write index = size (empty). Caller 0x001050F8. [O]

#### 0x000D55F8 `ringu16_push` — library
u16 ring-buffer write: byte-indexed advance, wrap at size byte, full flag at size-1, store
u16 at buf[idx]*2. Callers 0x000BC092, 0x001050F8. [O]

#### 0x000D562C `ringu16_get_history` — library
Reads the u16 element param_2 positions back (wrap, byte index), gated by 0x000D567C.
Caller 0x001050F8. [O]

#### 0x000D567C `ringu16_in_range` — library
Range predicate for the u16 ring buffer (full or 0 <= idx - offset). Callers 0x000D562C,
0x001050F8. [O]

#### 0x000D56A8 `ringu16_alloc_ctor` — logic
Allocates 8 bytes via interface (0x9AA34 facade) when handed NULL, then runs the ring
ctor 0x000D55C4. Caller 0x000BC060. [O]

#### 0x000D56D8 `ringu16_validate` — logic
Chain of interface validations: global check (0x000d5720 bare), object fields +1/+2, and
the buffer end address byte(p+1)*2 + *(int*)(p+4) via 0x000d5728; returns 1 only if all
pass. Caller 0x000BC092. [O]

#### 0x000D572C `diag_trace_u8` — library
Diagnostic value-trace hook (sink object 0x000d59d4): skips when the float equals the
previous byte; clamps to [0, DAT_000d59d0] (else 0xff), then appends a 14-arg event
(id, tag, prev, clamped byte) via 0x000d59d8 and commits via 0x000d59dc. Called from 30
sites across r10_app/r14_app. [O]

#### 0x000D5784 `diag_trace_s8` — library
Same trace pattern with signed-byte clamp ([DAT_000d59e4, DAT_000d59e0], floor -128, cap
127) on sink 0x000d59d4. Callers 0x000BB758, 0x0011CE44, 0x000F471C, 0x000F5B96, 0x000F4E54. [O]

#### 0x000D57E0 `diag_trace_u16` — library
Trace with u16 previous value; emits prev hi/lo plus clamped hi/lo bytes (two data bytes).
Callers span r10_app/r14_app/r08_app (18 sites). [O]

#### 0x000D5860 `diag_trace_s16` — library
Trace with s16 previous value and s16 clamp ([-32768 floor via DAT_000d59ec, 32767 cap]).
Callers 0x000BB758, 0x000F471C, 0x000F4E54. [O]

#### 0x000D58F8 `diag_trace_u32` — library
Trace with u32 previous value; converts the float with the 2.1474836e9 wrap so values above
INT_MAX map to their unsigned bit pattern; emits 4+4 bytes. Callers 0x000F6A22, 0x000F6D02,
0x0011D0F0. [O]

#### 0x000D59AC `diag_trace2_s32` — library
Second trace sink family (object 0x000d5c18, commit 0x000d5c20): s32 clamp with INT_MIN
floor and DAT_000d59fc cap, 4+4 bytes. Caller 0x000F6A22. [O]

#### 0x000D5A7C `diag_trace2_u8` — library
u8 variant on the second trace sink; the busiest hook in the family (31 call sites across
r10_app/r14_app, plus 0x000D7C36 and 0x000D7DFA here). [O]

#### 0x000D5AD8 `diag_trace2_s8` — library
s8 variant (clamp -128..127 via DAT_000d5c2c/0x000d5c28) on the second trace sink.
Callers 0x000F5778, 0x0011CACC, 0x000F471C, 0x000BB494, 0x000F4E54. [O]

#### 0x000D5B32 `diag_trace2_u16` — library
u16 variant (two data bytes) on the second trace sink. 18 call sites. [O]

#### 0x000D5BBC `diag_trace3_s16` — library
Third trace sink (object 0x000d5dc8, commit 0x000d5dd0): s16 clamp variant. Callers
0x000F471C, 0x000BB494, 0x000F4E54. [O]

#### 0x000D5C7C `diag_trace3_u32` — library
u32 variant with the 2.1474836e9 wrap on the third trace sink. Callers 0x000F6C3E,
0x000F6896, 0x0011CF14. [O]

#### 0x000D5D2A `diag_trace3_s32` — library
s32 clamp variant on the third trace sink. Caller 0x000F6896. [O]

#### 0x000D5DE4 `diag_report_evt2` — logic
Minimal 2-value event report: appends (param_2, param_1, 0…) to sink 0x000d5e20 via the
14-arg interface 0x000d5e24; no change-check. Callers 0x000D7D0E, 0x000D7C8C, 0x000D7E86,
0x000D7E20, 0x000BC092, 0x000D7D62, 0x001273B0. [O]

#### 0x000D5E0C `fatal_err69_reset_loop` — logic
Fatal-error sink for the diag/event subsystem: stores error code 0x69 via 0x000794C8 (RAM
DAT_fff80d1e/d, the boot-region fatal-code setter), then loops forever calling 0x0007A914
(system reset request 0x300). 36 callers — every diag_trace function and their users route
here on the failure path. [O]

#### 0x000D5E30 `shaft_speed_pair_calc` — logic
Computes a speed pair into param_1: filter factor via 0x000d6138; two dual-path channels
(direct interface chain 0x000d6144/48/4c/50, or gear-aware 0x000D635A/0x000D6338/0x000D6164
when the byte at obj 0x000d6130+8 is set); p[0]/p[6] raw, p[2]/p[8] float /DAT_000d6154,
p[0x1a]/p[0x1e] the ×300 scaled ints, p[0xc] the s16-clamped delta, all slewed through
0x000d6158, and p[4] a low-pass blend of p[2] with the s16 at 0x000d6160. Caller
0x000D70DC. [I]

#### 0x000D5FBA `shaft_speed_pair_calc2` — logic
Twin of 0x000D5E30 without the ×300 integer fields — same two channels, filter, delta
clamp and blend. Caller 0x000DD2F8 (outside this unit). [I]

#### 0x000D6164 `gear_ratio_interp_scaled` — logic
Gear-aware value computation: param_4 selects gear 1..4 (boundary bytes via interface
0x000d6390); looks up the pair (s16 ratio via 0x000D62A4, u16 value via 0x000D62EE) for
gear n and n+1 and interpolates by the boundary percentage (via 0x000d6398) when the
global byte 0x000d6394 sits between; paths 0/other use fixed slots (+0x24/+0x34 tables,
+0x10/+0x20 values). Final result = converter 0x000d63a0 applied to (value ×
((speed×10/7)² /1000 × ratio /1000) /10000). Callers 0x000D5E30, 0x000D5FBA. [I]

#### 0x000D62A4 `tbl_gear_ratio_lookup` — data-accessor
Switch on gear 1..4/else → table object slot (+0x24/+0x28/+0x2c/+0x30/+0x34 of obj
0x000d6388), s16 lookup by param_3 via 0x000d638c. Caller 0x000D6164. [O]

#### 0x000D62EE `tbl_gear_value_lookup` — data-accessor
Same gear switch on the paired slots (+0x10/+0x14/+0x18/+0x1c/+0x20), u16 lookup via
0x000d639c. Caller 0x000D6164. [O]

#### 0x000D6338 `calc_ratio_10000` — library
(param_3 × 10000) / max(param_2, 1) through converter 0x000d63a0 — a zero-guarded ratio.
Callers 0x000D5E30, 0x000D5FBA. [O]

#### 0x000D635A `gear_index_from_boundaries` — logic
Scans gear boundary bytes (obj 0x000d6388+0xc via 0x000d6390) and returns the first index
0..4 where the global byte 0x000d6394 is below the boundary, else 5. Callers 0x000D5E30,
0x000D5FBA. [O]

#### 0x000D63A4 `speed_window_flags2` — logic
Second hysteresis flag-pair setter (twin of 0x000D52CA shape) on object 0x000d6580: sets
p+3/p+4 from window comparisons (bands +8/+0xc and +0x18/+0x1c, enable bytes +0x10/+0x14),
keeping set state when p+9 is set. Caller 0x000D64D2. [O]

#### 0x000D645A `tbl_pair_feed_70a6` — logic
Reads the s16 behind obj 0x000d6580+0xac, then two table lookups (0x000d6588 on slots
+0xb0/+0xb4 with byte 0x000d6584[0xf]) each fed through 0x000D70A6 into p+0x1d/p+0x1e.
Caller 0x000D64D2. [O]

#### 0x000D64A0 `timers_reset_64a0` — logic
Resets the timer/counter block of the permission object: p+0x32=60000, p+0x2a/p+0x2e =
the u16s behind obj 0x000d6580 slots +0xc4/+0xc8, counters p+0x28/p+0x2c/p+0x30/p+0x16
and p+5 zeroed. Caller 0x000D70DC. [O]

#### 0x000D64D2 `shift_permission_eval` — logic
The unit's monster (956 instrs): updates a ~0x39-byte permission/flag object — standby
timeout init (0xEA60), window flags via 0x000D63A4, then ~18 hysteresis window checks
(p[0x1c], p[0x13], p[0xb], p[0x21], p[0x22], p[0xf], p[0x20], p[0xc], p[0x19], p[0x1f],
p[0x18], …) against objects 0x000d658c, 0x000d696c, 0x000d6aac, 0x000d6d0c, 0x000d6f40,
hold-timer counting on p+0x28/+0x2c/+0x30, master gates p[7]/p[9]/p[0xd]/p[0], latch p+0x38,
and tail calls 0x000D645A, 0x000D708C, 0x000D6FC8. Caller 0x000DD2F8. [I]

#### 0x000D6E48 `engage_permission_eval` — logic
Evaluates the p[0xe] engage-permission flag: requires mode byte (obj 0x000d6f40) == 1,
standby gear equality (0x6f54[0x2b]/[0x2c]), interface queries (0x6f64, 0x6f70 with
0x6f54[0x21]), timer p+0x24 vs a chained interface value (0x6f78/0x6f7c), a table lookup
(0x6f84 on obj+0xb8) and the p+0x38 hold timer. Caller 0x000D724C. [O]

#### 0x000D6F8C `cond_deny_speed_mode` — logic
Returns 0 (deny) only when: mode byte 0x000d70c4[0x34] != 6 AND s16 at +0x2c below the
speed at obj 0x000d70c8+0x28, with flag 0x000d70cc set, byte at obj+4 set, and mode nonzero.
Caller 0x000D725A. [O]

#### 0x000D6FC8 `mode_flags_decode` — logic
Decodes status flags by mode (0x000d70c4[0x34]): p[0x10] = (mode==5); p[0x11] = 1 when
(not mode-5 and (p[0] or p[1]) set, or mode 4) and interface 0x000d70d4 query returns 0;
p[0x12] = 1 when not mode-5, byte at obj 0x000d70c8+0xa8 clear or p[0x11] clear, and mode
not 0/1/6. Caller 0x000D64D2. [O]

#### 0x000D7058 `state_code_from_flags` — logic
Only in modes 1/2: p[0x27] = 3 when p[0] set, else 2 when p[9] set, else 0 when p[7] set
(unchanged when p[7] clear). Caller 0x000D7102. [O]

#### 0x000D708C `timer16_inc_708c` — logic
Increments the u16 at p+0x16 while it stays below the s16 limit at obj 0x000d70c8+0xd4.
Callers 0x000D64D2, 0x000D7454. [O]

#### 0x000D70A6 `flag_above_limit_hold` — logic
Sets *p4 = 1 when param_2 >= the s16 at 0x000d70d8+0xc; clears to 0 when below; keeps the
previous value inside the [param_2, param_2+param_3] hold window. Caller 0x000D645A. [O]

#### 0x000D70DC `speed_module_step` — logic
Periodic driver of the shaft-speed module: refreshes four interfaces (0x000d72cc/0x72d4/
0x72dc/0x72e4), clears p+0x34, and per edges.csv runs the chain 0x000D64A0 (timer reset),
0x00105F9C, 0x000D7774 and the speed calc 0x000D5E30. Caller facade 0x000BA230. [I]

#### 0x000D7102 `ramp4_channels_step` — logic
Step of a four-channel float ramp block: tick via 0x000D76CE, increments the u16 timer at
+0x42, p[0xc] = timer × DAT_000d72e8, runs the sequencer 0x000D7454 and an interface call
(0x72ec) plus 0x000D76BC; each of the four floats (p[0], p[4], p[5], p[3]) is filtered
through 0x000d72f8 (min/max DAT_000d72f0/0x000d72f4) and converted (× DAT_000d72fc via
0x000d7300) into u16 outputs p[2], p[0x20], p[0xc as u16], p+0x3e. Caller 0x000DD65C. [I]

#### 0x000D71B2 `two_state_selector` — logic
Latches p+0x35 = p+0x34, then compares an interface value (0x000d7310 on obj 0x000d7308+4)
against float 0x000d730c: below → state 2 with interfaces on p+0x58 (twice) and p+0x74;
above → state 1 with one interface on p+0x58; returns the value from the chosen sub-object.
Caller 0x000DD3C2. [O]

#### 0x000D7224 `module_reset_7224` — logic
Teardown: interface call on p+100, latches p+0x35 = p+0x34, clears RAM global DAT_fff8afd4
= 0, interface call on p+0xe4. Caller 0x000DD3C2. [O]

#### 0x000D724C `get_engage_permission` — stub (2-member cluster)
ROM-slot getter: returns interface (0x000d7334 on 0x000d72d8) which resolves to
0x000D6E48 (engage_permission_eval). Member 0x000D725A is byte-identical but its slot
resolves to 0x000D6F8C (cond_deny_speed_mode) — named separately below for that reason.
Callers 0x000DD4F8 / 0x000DD524. [O]

#### 0x000D725A `get_deny_speed_mode` — stub
Second member of the 0x000D724C pattern; its pointer slot (0x000d7338) resolves to
0x000D6F8C, so it exposes the deny predicate rather than engage permission. [O]

#### 0x000D7268 `cond_flags_f0_and_1` — logic
Predicate on the flag object at 0x000d72d8: 1 iff byte[0xf] == 0 and byte[1] != 0.
Caller 0x000D7454. [O]

#### 0x000D727C `cond_ready_speed_time` — logic
Returns 0 while: interface value (0x000d7310 on obj 0x000d7308+0xc) < float 0x000d730c AND
elapsed time (u16 p+0x42 × DAT_000d72e8) < the float at obj 0x000d7308+0x2c AND flag
0x000d7340 clear; else 1 (ready). Caller 0x000D7454. [O]

#### 0x000D7344 `cond_flag_matrix` — logic
Large AND/OR predicate over flag bytes 0x000d754c[1,3,4,5,7,8,9,10] and the float compare
(obj 0x000d7550+0x28 vs 0x000d7554); returns 0 when any abort combination holds.
Caller 0x000D7454. [O]

#### 0x000D73D0 `cond_all_flags_clear` — logic
Returns 1 only when flag bytes 0x000d754c[0,1,7,8,9,10] are all zero (all-clear).
Caller 0x000D7454. [O]

#### 0x000D73FC `get_byte_7558` — data-accessor
Returns the byte behind pointer cell 0x000d7558. Caller 0x000D7454. [O]

#### 0x000D7404 `cond_time_le_limit` — logic
Converts the elapsed timer (u16 p+0x42 × DAT_000d755c) through interface 0x000d7564 on
obj 0x000d7550+4 and compares <= float 0x000d7560. Caller 0x000D7454. [O]

#### 0x000D742E `cond_not_abort_flags` — logic
Returns 0 when any of flag bytes 0x000d754c[0,9,7] is set while [5] and [6] are clear
(abort in progress). Caller 0x000D7454. [O]

#### 0x000D7454 `engage_seq_state_machine` — logic
Seven-state sequencer on p+0x34 (with p+0x35 latch) driving sub-objects p+0x58, p+0x74,
p+100, p+0xe4, p+0xe8: state 0 idles on p+0xe4; 1 waits on 0x000D742E/0x000D7404 then
moves to 2; 2 branches on 0x000D73FC/0x000D7268/0x000D7344/0x000D73D0 to 3 or 4; 3/4 wait
on 0x000D727C then finish at 6; 6 settles on p+0xe4. Illegal state clears DAT_fff8afd4
and reports event (0x30b,0xf,0,1,3,0xf3). Caller 0x000D7102. [I]

#### 0x000D76BC `sel_value_if_flag` — logic
*p = (byte at p+0xd != 0) ? *(0x000d7754) : 0. Caller 0x000D7102. [O]

#### 0x000D76CE `filter3_init_from_mode` — logic
Re-initializes three 2-dword filter/ramp heads (pointer pairs 0x000d7758/0x7760,
0x000d7764/0x7768, 0x000d776c/0x000d7770): clears each value and loads the mode-indexed
entry (byte 0x000d775c) from each pointer table. Callers 0x000D7102, 0x000D71B2. [O]

#### 0x000D7774 `copy_word_7860` — data-accessor
Copies the dword behind pointer cell 0x000d7860 into p+0xc. Caller 0x000D70DC. [O]

#### 0x000D777E `calc_cascade_777e` — logic
Five-stage interface cascade into p[0..3]: combine (0x000d7870 on slots 0x786c/0x7868+0x10
and obj 0x000d7864+4), scale (0x000d787c with prev state), second combine (0x000d7888),
fold (0x000d788c), float convert (0x000d7898) plus offset DAT_000d789c into the u16 at
p[4]. Caller 0x000DD2F8. [O]

#### 0x000D78B8 `apply_map_on_change` — logic
Runs the map rebuild 0x000D790A, then edge-detects an interface status (0x000d7b18): on
drop with p+4 set, issues (obj, 0) via 0x000d7b20 and clears p+4; on rise with p+4 clear,
issues (obj, 1) and sets p+4. Caller 0x000BB1BC. [O]

#### 0x000D790A `gear_map7_apply` — logic
Rebuilds the 7-entry engagement map at RAM DAT_fff8e6b6: when the gear pair 0x000d7b24
[0x2b]==[0x2c] and flag 0x000d7b28 is 1/2 with [0x21]==[0x27], releases all seven channels
(interface 0x000d7c1c values off), computes three gear-indexed selections (0x000d7b4c/
0x000d7b50/0x000d7b54 by byte [0x27]) and fills the 7 array cells with their 1/2/3-input
table combinations (0x000d7b5c, 0x000d7c10), then enables all seven. Also manages single
channel acquire/release (speed-gated by table +0x7c) and latches p+2/p+3 from the current
gear bytes. Caller 0x000D78B8. [I]

#### 0x000D7C36 `diag_report_evt22` — logic
Two-shot event report: (0x000d7c80)(sink 0x000d7c7c, 0x22) then (0x000d7c84)(0,2,0x62,0);
edges.csv also routes it through trace hook 0x000D5A7C. Caller facade 0x000B7F4C. [O]

#### 0x000D7C60 `diag_report_evt23` — logic
Twin of 0x000D7C36 in the other order with code 0x23 and (0,0xc,0x62,0); also traces via
0x000D572C. Caller facade 0x000B7F4C. [O]

#### 0x000D7C8C `tbl_gear_s16_lookup` — data-accessor
Validates gear arg -1..8 (else fatal report (1,0x66) funneling to 0x000D5E0C), gets the
active row via 0x000D7DDE, and returns the s16 at table[row] + (gear+1)*2 from pointer
table 0x000d7ed4 (NULL row also fatal). 21 callers across this unit (0x000D21F2,
0x000D244C, 0x000D326C, 0x000D46A0, …) and r10_app/r14_app. [O]

#### 0x000D7CE8 `tbl_gear_s16_signed` — data-accessor
Wraps 0x000D7C8C: clamps the u16 result to 0x7fff and negates it when gear == -1.
Callers 0x00105040, 0x000BB290. [O]

#### 0x000D7D0E `tbl_gear5_s16_lookup` — data-accessor
Only when param_1 == 5 and gear param_2 in (-2..8): row via 0x000D7DDE, s16 at
table[row] + (param_2+1)*2 from 0x000d7ee0 (NULL fatal (1,0x66)); default 1000.
Caller 0x0010CBA8. [O]

#### 0x000D7D62 `tbl_row_gear2d_lookup` — data-accessor
2-D variant: for row param_1 (1..5) and gear param_2 (-1..8, nonzero) returns the s16 at
table[row] + (param_2+1)*10 + (param_1-1 clamped)*2 from 0x000d7ee4; else 0. Caller
0x000DCC70. [O]

## Unit summary

220 clusters, 226 named functions (6 clusters are multi-member: byte-identical twins named
with `__2`-style or semantic suffixes). Counts by kind:

- `logic`: 148
- `library`: 49 (ring-buffer float/u16 families, lerp/slew helpers, diag-trace hooks)
- `stub`: 15
- `data-accessor`: 14
- `unknown`: 0

No `unknown` entries; the least-certain identifications are marked [I] in the entries
(notably `shift_permission_eval` 0x000D64D2, `shaft_speed_pair_calc` 0x000D5E30/0x000D5FBA,
`gear_map7_apply` 0x000D790A, `engage_seq_state_machine` 0x000D7454).

Thematic blocks in the 0x4Bxx–0x7Dxx continuation: (1) condition predicates and the
quad-condition coordinator (0x000D4C52–0x000D4F24); (2) gear-pair matrix lookups and the
shift-pair torque feasibility check (0x000D4F86–0x000D5174, ROM matrices behind
0x000d51d0/0x000d51d4/0x000d51d8); (3) two ring-buffer libraries — float (0x000D54D0 family)
and u16 (0x000D55C4 family) — owned by the r10_app 0x001050xx/0x001051xx facade cluster;
(4) the diag value-trace family on three sinks (0x000D572C–0x000D5D2A) plus the shared
fatal sink 0x000D5E0C (error 0x69 → 0x000794C8 store + endless 0x0007A914 reset loop);
(5) the shaft-speed/gear-ratio calc chain (0x000D5E30–0x000D635A); (6) the shift-permission
arbiter 0x000D64D2 and its predicate swarm (0x000D63A4–0x000D742E); (7) the engage
sequencer 0x000D7454 with its 7-state machine and the 7-entry engagement map at RAM
DAT_fff8e6b6 (0x000D790A); (8) gear-indexed s16 table getters with fatal-arg checking
(0x000D7C8C–0x000D7D62, tables 0x000d7ed4/0x000d7ee0/0x000d7ee4).

Cross-region leads:
- The diag-trace hooks (0x000D572C–0x000D5D2A) are called from ~150 sites in r10_app
  (0x000F4xxx–0x00106xxx), r14_app (0x0011xxxx–0x0012xxxx), and r08_app (0x000BBxxx–
  0x000BCxxx) — those units should cross-check their measurement sites against these sinks
  and against fatal 0x000D5E0C / event 0x66.
- tbl_gear_s16_lookup 0x000D7C8C is shared by this unit (0x000D21F2, 0x000D244C,
  0x000D326C, 0x000D46A0) and by 0x000E97C6/0x00117E98/0x0011C468/0x00115E52 etc. — the
  owning units of those callers should reuse this name.
- Callers just outside this unit's range: 0x000DCC70, 0x000DD2F8, 0x000DD3C2, 0x000DD4F8,
  0x000DD524, 0x000DD65C, 0x000D7DFA, 0x000D7E20, 0x000D7E86, 0x000D7F4E (r0c_app tail) —
  the unit covering 0xD7D63+ owns the drivers of shift_permission_eval,
  shaft_speed_pair_calc2, ramp4_channels_step and the module getters.
