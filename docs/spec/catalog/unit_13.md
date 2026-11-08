# Unit 13 (r08_app)

Region 0x00089518–0x000936FC (r08_app, 220 clusters). This slice is dominated by one large
subsystem: a per-channel condition-monitoring engine. Channels are indexed (up to ~16; one
family loops 10), every family has per-channel state bytes (DAT_fff93b0c[] with states
0x10–0x61, substates DAT_fff93a2e[]/DAT_fff93a4c[]), debounce counters, ratio/utilization
values scaled 0–1000 (per-mille), and a shared epilogue that builds a condition bitmask,
ANDs it with calibration enable tables selected through PTR_PTR_xxx + DAT_fff90ab8 (active
calibration-bank index), latches it into a per-channel word, classifies (0/1/2/0x81/0x82)
and calls a report function pointer. Companion block 0x0008B7A8–0x0008C2A4 is a guarded
10-phase sequencer over value+complement RAM pairs (0x55/0xAA start, 0x30–0x36 phases,
0xF0–0xF6 error codes) that start/check/stop two instances of something with mode 3/5
transitions (0x0008D108/0x0008D136/0x0008D1BA/0x0008D020). All semantics beyond structure
are [I]; no known_map anchors exist in this range.

#### 0x00089518 `mon_subsys_init` — init
Module init from the 0x0008332C init chain: calls function pointer PTR_FUN_00089538 twice
with ROM id &DAT_0003b150 and PTR_FUN_0008953c once (register/callback style). 10 instrs,
no other callers. [I]

#### 0x00089540 `mon_chan_thresh_set` — logic
Per-channel setup: maps channel type (table via PTR_PTR_000896f8+DAT_fff90ab8) and state
DAT_fff94fd8[ch] (1–4; 3 loads extra offset DAT_fff94f58[2i]) to complement tag pairs
0x21/0xde, 0x32/0xcd, 0x43/0xbc; calls 0x00089648, then PTR_FUN_000896fc → DAT_fff93a12;
clamps value+offset to 0x2FFA and stores tag DAT_fff90812[ch] plus value DAT_fff90230[ch]
and DAT_fff94f58[2i]. Verifies value/complement integrity and reports error 0x226 via
PTR_FUN_00089700 with info address in DAT_fff90998. Caller: per-channel driver 0x00083DB0.
[O] structure, [I] meaning

#### 0x00089648 `mon_input_acquire` — logic
Input acquisition helper of 0x00089540: for channel 0 polls three sub-slots (query
PTR_FUN_00089704/00089710, apply PTR_FUN_00089708/0008970c; slot 0 one-shot DAT_fff94140);
for type-1 channels stores PTR_FUN_00089714(ch,x) into DAT_fff90320[ch] when
DAT_fff90826[ch] != 0. [I]

#### 0x00089718 `mon_chan_cnt_step` — logic
Complement-stored counter step (caller 0x00084392): keeps DAT_fff94dd0[2i] unchanged for
substates -2/-1/-3/-5 (state -4 reloads it as uVar2), type-2 channels untouched; otherwise
increments the previous value (recovered via ~uVar2) by global DAT_fff93a12, clamps at
0x2FFA and stores the complement back (0xD005 = ~0x2FFA clamp sentinel). [I]

#### 0x000897AC `mon_state_update` — logic
Per-channel state machine writing DAT_fff93b0c[ch] (0x10,0x11,0x18,0x19,0x20,0x21,0x30,
0x31,0x32,0x40,0x41,0x50,0x51,0x60,0x61) and DAT_fff947ea[ch]. First loops 10 channels
building "in-window" bitmask DAT_fff93a14 from DAT_fff94ee8[i] vs hysteresis windows around
DAT_fff933f8[i] (offset from table), tracking max DAT_fff94826 and flag DAT_fff94141;
DAT_fff84a2d holds last channel. Type-2 channels map straight to 0x50/0x51; substates
' ', '1', '2' handled with latch bitmask DAT_fff93a16. [I]

#### 0x00089A8C `mon_ratio_eval` — logic
Per-channel per-mille ratio evaluation: ratio1 = DAT_fff90296[2i]*1000/DAT_fff9026e[2i],
ratio2 = DAT_fff90294[2i]*1000/DAT_fff9026c[2i], gated by denominator sentinels 0xFFFF/0,
speed window DAT_fff9083c±0x14 and thresholds 99/12000/0x3B6 → statuses 0x10 valid, 0x20
clamp-1000, 0x21 zero; combined cases (sum, single-side*2 via divide fn-ptrs
PTR_FUN_00089cf0/00089f3c) give 0x18/0x19. Result → DAT_fff9340c[ch], status
DAT_fff93a2e[ch], debounce DAT_fff93a24[ch]; then first-order filtered (PTR_FUN_00089f4c,
then 0x00089EDA) into DAT_fff93420[2i] with filter state DAT_fff93434[4i] and armed bitmask
DAT_fff93a22. [I]

#### 0x00089EDA `lib_pt1_filter_i` — library
Integer PT1 (first-order lag): state += rate16 * (x - f(state)) with sign handling, returns
f(state) = state*k + r (k/r from ROM floats DAT_00089f50/f54/f58). Used by the ratio monitor
0x00089A8C. [O]

#### 0x00089F5C `mon_cond_eval_report` — logic
Condition-evaluation epilogue of the state/ratio monitor family (caller 0x00083DB0):
refreshes mode via 0x0008A6D8 and PTR_FUN_0008a1c8, copies DAT_fff93394[2i]→DAT_fff93104[2i],
builds condition bitmask (bits 1,4,8,0x10,0x20,0x80,0x100,0x200,0x400,0x800,0x1000,0x2000)
from state/substate/type/mode and thresholds DAT_fff93510/fff93524[2i]; ANDs with enable
table (PTR_PTR_0008a760), latches DAT_fff94930[2i] (bits kept while enabled), stores raw
mask DAT_fff94944[2i]; classifies 0/1/2/0x81/0x82 into DAT_fff93994[ch] via two more mask
tables and reports via PTR_FUN_0008a770(0,ch,0,class,cond). [I]

#### 0x0008A6D8 `mon_mode_classify` — logic
Per-channel mode into DAT_fff9399e[ch]: 1 default; if environment fn-ptr state
(PTR_FUN_0008a774(0)) == 'U' and DAT_fff939b8 == 4 → 3 or 4 by two speed windows on
DAT_fff94ed0; otherwise 2. Feeds 0x00089F5C. [I]

#### 0x0008A788 `mon_adapt_thresholds` — logic
Adaptive thresholds (caller 0x00083DB0) for states 0x20–0x32: works on the step of
DAT_fff94ed4[ch] vs DAT_fff93484[2i]; computes two ramped limits using curve fn-ptrs
(PTR_FUN_0008a978 / PTR_FUN_0008ac28 on calibration handles) plus adaptation offset from
0x0008ACFE and scale floats DAT_0008a980/0008ac34; stores limits DAT_fff93498[2i] /
DAT_fff934ac[2i] with ramp-active bitmasks DAT_fff93a62/fff93a64; final enable windows
(PTR_PTR_0008ac38..40) zero the outputs; results DAT_fff93510[2i]/DAT_fff93524[2i]. [I]

#### 0x0008ACFE `mon_adapt_offset` — logic
Sums condition-dependent adaptation offsets: base term latched while DAT_fff940fa/DAT_fff94827
set, extra if counters DAT_fff940fc or DAT_fff94ff0 < 6, extra for state 0x31, plus per-channel
term when bit set in DAT_fff93378 (latched via DAT_fff947f4[ch]). Returns offset for
0x0008A788. [I]

#### 0x0008ADF4 `mon_utilization_calc` — logic
Per-channel utilization 0–1000: mode 0 → 1000; mode 1 → curve value (PTR_FUN_0008b004) over
snapshot DAT_fff94fbe divided by speed from 0x0008AFC0; mode 2 → direct DAT_fff94fc0[2i];
mode 3 → weighted formula with DAT_fff933a8[2i] and DAT_fff93a10; clamped into min/max
windows (PTR_DAT_0008b018/0x20) with enable-table exceptions, ≥0x3E9 → 1000. Outputs
DAT_fff93a66[2i], DAT_fff9086a[ch], DAT_fff94fb4[2i]. Caller 0x000849D4. [I]

#### 0x0008AFC0 `mon_speed_select` — data-accessor
Reference-speed selector: table byte 0xFF → DAT_fff94f9e, 0xFE → DAT_fff94f86, else speed
channel DAT_fff94ed0[idx]. [O]

#### 0x0008B02C `mon_utilization_calc_c` — logic
Complement-stored variant of 0x0008ADF4 (caller 0x00084DA0): same 0–1000 modes via speed
select 0x0008B21A and curve fn-ptr PTR_FUN_0008b260; stores ~value into DAT_fff94e2c[2i] and
DAT_fff93a70[2i] (0xFC17 = ~1000 sentinel); one-shot init DAT_fff94142[ch]. [I]

#### 0x0008B1C6 `mon_snapshot_copy` — library
Rotates per-channel snapshot fields through byte+complement copy helpers 0x0006BDCA/
0x0006BDFE: DAT_fff95000[i]←DAT_fff94e78[i], DAT_fff94fbe←DAT_fff94e36,
DAT_fff94fc0[2i]←DAT_fff94e38[2i], DAT_fff94fb4[2i]←DAT_fff94e2c[2i]. Caller 0x000850A4. [O]

#### 0x0008B21A `mon_speed_select_c` — data-accessor
Complement-variant speed select: 0xFF → DAT_fff94e16, 0xFE → ~DAT_fff94f86, else
DAT_fff94d48[2*idx]. [O]

#### 0x0008B284 `mon_cond_eval_report2` — logic
Condition-evaluation epilogue of the utilization monitor family (caller 0x000849D4):
gated by 0x0008B5DE; window checks on utilization DAT_fff90874[ch] (0, below-min band,
mid band, >1000) and speed pair DAT_fff908a6/fff9089c[2i] against calibration min/max
tables → condition bits 0x4/0x8/0x10/0x18/0x20/0x24 (plus enable-table remap 8|0x10→0x20);
enable combine, raw mask DAT_fff94988[2i], latch DAT_fff9497e[2i], class into
DAT_fff94175[ch] (0/1/2) via mask tables, report PTR_FUN_0008b6f4(1,ch,type,class,cond);
immediate PTR_FUN_0008b6e0() on rising edge. [I]

#### 0x0008B5DE `mon_release_gate` — logic
Per-channel release gate for 0x0008B284: mode/speed from table (0xFF→DAT_fff94f9e,
0xFE→DAT_fff94f86, else DAT_fff94ed0[idx]); enables when fn-ptr env check 'U' with
DAT_fff939b8[mode]==4, one-shot DAT_fff94148[ch], utilization transition tracking
(DAT_fff90874 vs prev DAT_fff93a7a[2i]) and speed window tables PTR_PTR_0008b7a0/a4 →
returns 1. [I]

#### 0x0008B7A8 `selftest_seq_dispatch` — dispatch
Do/while sequencer over value+complement pairs (state DAT_fff94fe2/fff94e5a, sub-state
DAT_fff9500d/fff94e85): re-inits when speed DAT_fff94f96 leaves the calibration window
(latching DAT_fff94150/4f/4e), dispatches state bits 1,2,4,8,0x10,0x20,0x40,0x80,0x100,
0x200 to phase handlers 0x0008B974/BB34/BB8A/BC54/BD40/BDD6/BEC0/BFB8/C0F2/C194; writes
sub-state codes (0x55/0xAA start, 0x30–0x36 phases with complements, 0xF0–0xF6 error
codes) via PTR_FUN_0008ba24/0008ba28; loops while sub-state == 'U' and calibration byte
!= 0xA5. Caller 0x00085160. [O] structure, [I] purpose

#### 0x0008B974 `selftest_phase1` — logic
Phase 1: reads lock/context fn-ptrs (PTR_FUN_0008ba30(5), PTR_FUN_0008ba20) and current
sub-state; on 'U' start clears per-instance results (PTR_FUN_0008ba44(i,0), i<2), resets
DAT_fff94114, ORs error words DAT_fff94f70/94de8 and DAT_fff94f6e/94de6 into
DAT_fff94f72/94dea; zero → ack 0x33 (PTR_FUN_0008bcbc) and next sub-state 0x55/0xAA or
0x31/0xCE (with DAT_fff9481f from table); error paths 0xF1/0x0E, 0x30/0xCF, 0xF0/0x0F. [I]

#### 0x0008BB34 `selftest_phase2` — logic
Phase 2: if DAT_fff90959 == 0 → error latch (DAT_fff93a8c |= state) and sub-state 0x55/0xAA;
else with DAT_fff9481f == 0 → sub-state 0xF4/0x0B and per-instance (i<2, enable table
PTR_PTR_0008566c) acknowledge errors via PTR_FUN_00085660(i,0x33) from word
DAT_fff94f6c/94de4; else 0x33/0xCC. [I]

#### 0x0008BB8A `selftest_phase4` — logic
Phase 4: 'U' → masks instances flagged in DAT_fff93a8a[i] out of the error word
DAT_fff94f6c/94de4 into DAT_fff94114, sub-state 0x30/0xCF; otherwise runs
0x0008C250(0x22,1)+0x0008C2A4(0) — on clear check latches DAT_fff93a8c and writes
0x55/0xAA, result codes 0x34/0xCB or 0xF2/0x0D; final ack 0x33 via PTR_FUN_0008bcbc. [I]

#### 0x0008BC54 `selftest_phase8` — logic
Phase 8: 'U' → per-instance (i<2) error-bit acknowledge PTR_FUN_0008bccc(i,1) from word
DAT_fff94f72/94dea, sub-state 0x34/0xCB; else 0x0008C250(0x22,1)+0x0008C2A4(0): on error
latch + 0x55/0xAA, else per-instance disable PTR_FUN_0008bf60(i,0); sub-states
0xF2/0x0D, 0x34/0xCB. [I]

#### 0x0008BD40 `selftest_phase16` — logic
Phase 0x10: 'U' → snapshot error word DAT_fff94f72/94dea into DAT_fff94114, sub-state
0x34/0xCB; else 0x0008C250(0x22,1)+0x0008C2A4(0): error → latch + 0x55/0xAA; clear →
DAT_fff94114=0, per-instance stop via PTR_FUN_0008bf68()/PTR_FUN_0008bf6c(3) returning
PTR_FUN_0008bf70(); error sub-state 0xF3/0x0C. [I]

#### 0x0008BDD6 `selftest_phase32` — logic
Phase 0x20: 'U'/'1' → with lock fn-ptr PTR_FUN_0008bf74(3) free and follow-up <2 →
0xF1/0x0E; with DAT_fff90959 set and fn-ptr check → success 0x33, DAT_fff94154=0xAA,
DAT_fff9481f from table; state '3' → 0xF4/0x0B or 0x55/0xAA error; default 0xF0/0x0F.
Uses mode helpers 0x0008D108/0x0008D1BA. [I]

#### 0x0008BEC0 `selftest_phase64` — logic
Phase 0x40: 'U' → per-instance (i<2) error-bit acknowledge PTR_FUN_0008bf60(i,1) from word
DAT_fff94f72/94dea, DAT_fff94821 from table, sub-state 0x34/0xCB; state '4' →
0x0008C250(0x33,1)+0x0008C2A4(1): clear → error latch + 0x55/0xAA, result 0xF2/0x0D or
0x34/0xCB; other → 0xF0/0x0F. [I]

#### 0x0008BFB8 `selftest_phase128` — logic
Phase 0x80: 'U' → start instances (PTR_FUN_0008c224/0008c228(5)) and stop again
(PTR_FUN_0008c228(0)), sub-state 0x31/0xCE; '1' → with lock PTR_FUN_0008c230(5) free,
second check >1 returns, else 0xF1/0x0E or DAT_fff94821 from table; '4' →
0x0008C250(0x66,0)+0x0008C2A4(1): clear → latch + 0x55/0xAA, per-instance disable
PTR_FUN_0008c240(i,0), DAT_fff94821 from table, start(0) cycle; 0xF2/0x0D, 0x34/0xCB. [I]

#### 0x0008C0F2 `selftest_phase256` — logic
Phase 0x100 (also error handler of the dispatcher): sub-state < 0xF0 → 0x0008C250(0x22,1)
+ 0x0008C2A4(1); clear with DAT_fff93a8c == 0xFF → copy to DAT_fff93a86 and sub-state
0x55/0xAA (restart); 0x0F → 0xF2/0x0D; else 0x30/0xCF; sub-state ≥ 0xF0 → doubles
DAT_fff93a84 |= a8c+1 and DAT_fff93a86 = a8c*2+1 (error escalation). [I]

#### 0x0008C194 `selftest_phase512` — logic
Phase 0x200 (init/rest state): restores DAT_fff94820/94821 from DAT_fff9414f/4150; if speed
DAT_fff94f96 is inside the calibration window, re-arms the sequencer state
(PTR_FUN_0008c24c writes DAT_fff93a88 + complement) and sub-state from DAT_fff9414e pair;
else sub-state 0x40/0xBF (wait). [I]

#### 0x0008C250 `selftest_inst_status` — logic
For the two instances (DAT_fff93a8a[i]): reads status PTR_FUN_0008c340(i); mode-1 compare
marks instance active when status == param (0x33 also accepts 0x66); mode-0 marks inactive
when equal. Returns matched status. Called by phases 4–0x100. [I]

#### 0x0008C2A4 `selftest_inst_ack` — logic
Consumes the instance bitmask: for each active DAT_fff93a8a[i] with bit set in the error
word DAT_fff94f72/94dea (read+complement), clears the bit, records the status into
DAT_fff939b4[i], ORs the sequencer state into DAT_fff93a84; param 1 with DAT_fff94821 set
returns 10 early; empty word → 0x0F. Returns 0 (all cleared) or 10. [I]

#### 0x0008C34C `mon2_vote_update` — logic
Small two-input arbiter on DAT_fff94151 bits and request bits DAT_fff90956&3: clears
DAT_fff93b22, sets DAT_fff93b24 to 2 or 4 depending on bit 2, and shuffles bits 0/1/2/4 of
DAT_fff94151 (set-2 → clear-4 style hysteresis); ends with fn-ptr PTR_FUN_0008c3b8.
Caller 0x00085788. [I]

#### 0x0008C3BC `mon2_errcode_update` — logic
Per-channel (0/1) error-code derivation: gates on enable table PTR_PTR_0008c614; combines
plausibility fn-ptr PTR_FUN_0008c618(ch), previous codes DAT_fff94e48[ch] (state machine
over negative codes -0x12,-0x17,-0x23,-0x34,-0x35,-0x39,-0x40,-0x45,-0x56 with
DAT_fff939be[ch] -1/-2 progression), latch DAT_fff947ae[ch] (raised while ~DAT_fff94e0e
below table), status DAT_fff93a92[ch] ('11'/'D' checks), DAT_fff90947[ch], and enable
bitmask DAT_fff94117; writes updated code DAT_fff94e48[ch], status DAT_fff94e46[ch]
(0xFF/0xFE/0xDD sentinels), latch and bitmask. Caller 0x00085828. [I]

#### 0x0008C670 `mon2_snapshot_copy` — library
Rotates five per-channel value+complement snapshot pairs through copy helper (PTR_FUN_0008c6d0
→ 0x0006BDCA): DAT_fff94fca[i]←DAT_fff94e42[i], DAT_fff94fd0[i]←DAT_fff94e48[i],
DAT_fff94fcc[i]←DAT_fff94e44[i], DAT_fff94fce[i]←DAT_fff94e46[i], DAT_fff95005←DAT_fff94e7d.
Caller 0x00085828. [O]

#### 0x0008C6D4 `mon2_speed_plaus_class` — logic
Per-channel speed plausibility classification into DAT_fff93a92[ch]: 0x11 below the low
window table, 0x22/0x44 by low/high thresholds, else compares channel speed DAT_fff94ed0[ch]
against reference DAT_fff94f96 via difference fn-ptr PTR_FUN_0008c7f4 and a scaled window
(table * fVar7 / DAT_0008c800) → 0x33 (in tolerance) / 0x55 (out) / 0x66 (diff below
threshold). Callers 0x00085DAC/0x00085828/0x00086838. [I]

#### 0x0008C7A8 `mon2_chan_enable` — data-accessor
Stores per-channel enable DAT_fff93a90[ch] = arg; on change triggers fn-ptr
PTR_FUN_0008c804(ch) and updates DAT_fff93a8e[ch]; sets DAT_fff90944[ch] = 99 when enabled
else 0. [O]

#### 0x0008C808 `mon2_flag_set` — stub
Stores its byte argument into DAT_fff9094d (3 instrs). Caller 0x00086916. [O]

#### 0x0008C810 `mon3_cond_eval` — logic
Single-instance condition evaluation: early-returns DAT_fff9411e when it holds 2,5,6,7,8,9;
otherwise state machine DAT_fff9417a (0→1→3→2→4 transitions) driven by gates
(DAT_fff94f86 speed windows, DAT_fff94fed < 8, DAT_fff940fa, DAT_fff939f2/DAT_fff93942 vs
threshold tables), building condition word 0/4/8/0xC/0x10 with latch DAT_fff94992 (raw mask
DAT_fff94994) and latched bytes DAT_fff94832/94833 from tables; report via
PTR_FUN_0008cab0(mask, value). Caller 0x00086A80. [I]

#### 0x0008CAB4 `mon3_cond_eval2` — logic
Condition eval gated by 0x0008CB2A (reference speed in window): reads value via fn-ptr
PTR_FUN_0008cb60(*PTR_DAT_0008cb5c) and classifies against two thresholds →
DAT_fff93b26 = 0/4/8; masks with enable table PTR_PTR_0008cb6c into DAT_fff93b28 and
reports PTR_FUN_0008cb70(mask, value). Caller 0x00086AF8. [I]

#### 0x0008CB2A `mon3_speed_in_window` — data-accessor
Returns 1 when reference speed DAT_fff94f86 is within the calibration window
[PTR_PTR_0008cb74, PTR_PTR_0008cb78). [O]

#### 0x0008CB7C `mon3_signal_stuck_chk` — logic
Reads (&DAT_fff9018c)[*PTR_DAT_0008ccbc] into DAT_fff93a94; deviation vs previous
DAT_fff93b2a ± tolerance (PTR_PTR_0008ccc0) with bit 3 of DAT_fff9417c toggles bits 4/2 and
freezes/refreshes DAT_fff9417b (from DAT_fff90958) — signal-stuck detection with alternating
confirmation; then bit 0 set when DAT_fff9417b is 2 or 4 and table gates allow; bit-1/bit-0
interplay selects DAT_fff94784 (word from table) or DAT_fff94834 (byte from table).
Caller 0x00086B0C. [I]

#### 0x0008CC76 `mon3_speed_pick_a` — data-accessor
Returns DAT_fff94f9e or DAT_fff94f86 depending on a calibration byte == -1. [O]

#### 0x0008CC98 `mon3_speed_pick_b` — data-accessor
Returns DAT_fff94ff5 or DAT_fff94fec depending on the same calibration byte == -1. [O]

#### 0x0008CCD4 `mon3_cond_eval3` — logic
Gated by 0x0008CDDC: direction thresholds on DAT_fff93b2a vs PTR_PTR_0008ce28/2c tables →
DAT_fff93b2c = 4/8 (uVar3 conditions 4/8); when table enables and DAT_fff90958 bits qualify,
extends conditions with 0x100/0x200 using counters DAT_fff94768/fff94766, DAT_fff94120 and
DAT_fff9417c bits 0x10/0x20; final mask DAT_fff93b2e = cond & enable-table, report
PTR_FUN_0008ce38(mask, value). Caller 0x00086B0C. [I]

#### 0x0008CDDC `mon3_release_gate` — logic
Release gate for 0x0008CCD4: true when fn-ptr state PTR_FUN_0008ce40() == 8, fn-ptr value
PTR_FUN_0008ce3c() above table threshold, and both DAT_fff94834 == 0 and DAT_fff94784 == 0. [I]

#### 0x0008CE48 `mon3_state_cnt_update` — logic
State/counter updater for the 0x00086B0C monitor: DAT_fff94122 (0/1/2) with DAT_fff90958
bit 1 selects DAT_fff94120 (0/1), DAT_fff9095a, and loads counter DAT_fff94768 or
DAT_fff94766 from tables; calibration switch 0 disables; ends with report fn-ptr
PTR_FUN_0008ced8 (→ 0x00090CF6). [I]

#### 0x0008CEDC `mode_seq_step` — logic
Transition executor for the value+complement mode pair DAT_fff95011/fff94e89 (shadow
DAT_fff93b97): states 0, 7, 0x19, 0x2A with fn-ptr helpers PTR_FUN_0008d00c (read request),
PTR_FUN_0008d010 (pair read), PTR_FUN_0008d014(req, code) compare/apply, PTR_FUN_0008d018
for 0x2A; special DAT_fff93ba4 == 7 → state 0x2A; sets bits 0x40 in DAT_fff9384c/DAT_fff94f84
and clears DAT_fff94dfc bit; complement-mismatch reports error 0x226 with info 0x975E1.
Callers 0x000873F0/0x0008799C. [I]

#### 0x0008CFF0 `mode_seq_reset` — stub
Resets the mode sequencer: DAT_fff95011 = 0x1E, complement DAT_fff94e89 = 0xE1, shadow
DAT_fff93b97 = 0x1E. Caller 0x00087732. [O]

#### 0x0008D020 `mode_transition_supervise` — logic
Supervises mode transitions of the DAT_fff95012/94e8a pair: countdown DAT_fff9417d (set up
by 0x0008D136) decrements while 0x0008D108(target) says not reached, setting bits 8 in
DAT_fff9384c/DAT_fff94f84 and clearing DAT_fff94dfc bit; DAT_fff93ba4 == 7 → DAT_fff9417e = 0;
target 0 or below actual DAT_fff93ba1 → request 7 via PTR_FUN_0008d1e8/0008d1ec(…,0x1E) else
0x19 via PTR_FUN_0008d1e4; complement mismatch → error 0x226 (info 0x975E2). Callers
0x000873F0/0x0008799C. [I]

#### 0x0008D108 `mode_target_reached` — logic
Returns 1 when actual mode DAT_fff93ba1 matches the requested mode, with grouping: request 5
accepts actual ≥ 5, request 3 accepts actual ≤ 4; anything else 0. Callers
0x000873F0/0x0008D020/0x0008B974/0x0008BDD6/0x0008BFB8. [O]

#### 0x0008D136 `mode_req_set` — logic
Requests a mode transition on the value+complement pair DAT_fff95012/fff94e8a (0/0xFF,
5/0xFA, 3/0xFC), computes timeout DAT_fff9417d from calibration base + |delta to actual
DAT_fff93ba1| + 2, clears flag DAT_fff93b96. Callers 0x000873F0/0x0008B974/0x0008BD40/
0x0008BFB8. [O]

#### 0x0008D1BA `mode_timeout_check` — logic
Returns countdown DAT_fff9417d and latches DAT_fff93b96 = 1 when it reaches 1. Callers
0x000873F0/0x0008B974/0x0008BDD6/0x0008BFB8. [O]

#### 0x0008D1D0 `mode_req5_set` — stub
Stores mode 5 with complement into DAT_fff95012/fff94e8a. Called from the shutdown/reset
chain 0x00087732. [O]

#### 0x0008D1F8 `cmd_tag_arm` — stub
Copies ROM word DAT_0008d20c into DAT_fff93320 and arms tag DAT_fff9412c = 0xAA (matched
against a struct byte by 0x00087060). Purpose of the tag not resolved. [O] code, [I] purpose

#### 0x0008D210 `state_integrity_chk` — library
When the passed state byte matches param_1, verifies the 4-byte RAM state DAT_fff93344..47
against checksum helper 0x0008D262 (byteswap + x*2048 + x>>22 xor constants DAT_0008d2b0/
d2ac): returns 0 on match, 0x22 on failure, 0xFFFFFFFF when the state does not apply.
No direct callers (fn-ptr/table-dispatched). [O]

#### 0x0008D2B4 `lib_be24_validate` — library
Extracts a big-endian 24-bit value from the record at param_2 and passes it with mode 1 to
validator 0x000AA09A (decode/check library near 0xA0xxx–0xC8xxx); success writes the result
byte at +4, failures return codes 0x31/0x22. Caller 0x00087F6C. [I]

#### 0x0008D308 `tbl_interp2_i16` — library
Bilinear interpolation of a 2-D s16 map (values ptr, x-axis ptr, y-axis ptr, sizes, x, y):
index search, edge clamping, two linear interpolations in float, ±0.5 rounding
(DAT_0008d424/428). Caller 0x00088538. [O]

#### 0x0008D42C `tbl_interp1_i16` — library
1-D s16 curve interpolation with axis array, edge clamp, linear interpolation and ±0.5
rounding (DAT_0008d4ac/4b0). Caller 0x000886AC. [O]

#### 0x0008D4B4 `meas_val_pair_read` — logic
Returns a validated value as word+byte complement pairs: with plausibility fn-ptr
PTR_FUN_0008d548(0x65) == 0 the stored pairs DAT_fff94f98/94e10, DAT_fff94fee/94e66 are
copied; otherwise computes table value + DAT_fff9393a and min(6, DAT_fff940fc) with
complements. Callers 0x00088A50/0x000887D8. [I]

#### 0x0008D526 `meas_byte_pairs_copy` — data-accessor
Copies four bytes: DAT_fff94ff1/94e69 and DAT_fff94ff3/94e6b (two value+complement byte
pairs) to the caller. Callers 0x0008E58C/0x0008E5E8. [O]

#### 0x0008D57C `meas_ratio_defaults` — data-accessor
Loads pipeline defaults: DAT_fff94770 and DAT_fff94822 from calibration tables,
DAT_fff94157 = DAT_fff84796, DAT_fff93aa0 via DAT_0008d808+DAT_fff90ab8. Caller 0x00088A50. [O]

#### 0x0008D5BA `meas_ratio_process` — logic
Two-value ratio/delta pipeline: 5-entry state-transition table on DAT_fff94157/94156;
reads two channels of table DAT_fff9018c (via PTR_DAT_0008d818) into DAT_fff93aa6/aaa4,
delta DAT_fff93aa8 with one-cycle delay DAT_fff93aaa, deviation DAT_fff93aa2 and derived
value DAT_fff93a9c = (5000−delta)*k/min(aa6,10) clamped to 0x9C40; builds condition
bitmask (bits 0–0x40, 0x80/0x180, 0x800/0x200) against state-indexed window tables;
valid → curve value PTR_FUN_0008d848 − 0x8000 into DAT_fff93aa0, state DAT_fff94159=8;
timeout path state 3. Result mask DAT_fff93a9e. Caller 0x000887D8. [I]

#### 0x0008D894 `meas_spd1_defaults` — logic
Signal-pipeline reset for the DAT_fff94f86 family: loads timeouts DAT_fff94774/94772 from
tables and writes (0,0xFFFF) to pair DAT_fff94f86/94dfe and (0,0xFF) to byte pair
DAT_fff94fec/94e64 via fn-ptrs. Caller 0x00088B74. [O]

#### 0x0008D8DE `meas_spd1_process` — logic
Validates raw speed DAT_fff907e2 against min/max tables with timeout latches DAT_fff94774/
94776/94778; deviation check vs offset table (bits 1/3); raises quality flags DAT_fff940fa
(raw DAT_fff907f6 over table, clamped 0x7FFF into DAT_fff93938) and DAT_fff9415b; substitutes
DAT_fff93ab0 = DAT_fff907e4 snapshot; writes filtered value (clamped 0x7FFF, complement) to
DAT_fff94f86/94dfe and status code 8/6/7 (complements 0xF7/0xF9/0xF8) to DAT_fff94fec/94e64;
condition mask DAT_fff93aac. Caller 0x00088AE0. [I]

#### 0x0008DB28 `meas_spd2_defaults` — data-accessor
Loads defaults DAT_fff93aba, DAT_fff94823, DAT_fff9477a from calibration tables. Caller
0x00088BAC. [O]

#### 0x0008DB5A `meas_spd2_process` — logic
Second signal pipeline on raw DAT_fff908ca with mode index DAT_fff90966 and direction bits
DAT_fff9095e: computed value sVar1 = axis table + raw * gain table/1000; out-of-range bits
2/4, disable 0x40; window comparisons (bits 0x100/0x200 by direction bit 0, 0x800/0x1000 by
bit 1) with 0x20/0x30 consolidation into condition mask DAT_fff93ab6; change bit 8; accepted
value DAT_fff93aba with states DAT_fff9415e = 8/2/4/7 and timeout DAT_fff9477a; accepted
inputs latched to DAT_fff93ab2/9415c/9415d. Caller 0x00088BD0. [I]

#### 0x0008DD40 `meas_sig3_defaults` — data-accessor
Loads defaults DAT_fff9393e, DAT_fff94730, DAT_fff9477c and marks DAT_fff93ac0 = 0x80.
Caller 0x00088D98. [O]

#### 0x0008DD7C `meas_sig3_process` — logic
Third signal pipeline on raw DAT_fff908cc: zero bit 0x400, sentinel -1 bits 4/0x100, in-range
bit 0x80, exact-match bit 8, mask bit 0x10, high/low bits 0x40/0x20, timeout bits 2/0x1000;
accepted result DAT_fff9393e = raw >> 3 with states DAT_fff94100 = 8/2/4/7 and timeouts
DAT_fff94730/9477c; change bit 0x200. Caller 0x00088DA8. [I]

#### 0x0008DEF0 `meas_ch6_init` — logic
Clears the six value+complement byte pairs DAT_fff94f88[i] = 0 / DAT_fff94e00[i] = 0xFF,
loads DAT_fff94160 and timeout DAT_fff9477e from tables. Caller 0x00088EB0. [O]

#### 0x0008DF2E `meas_ch6_process` — logic
Six-channel classification pipeline: per channel enabled by variant tables
(PTR_DAT_0008e174/178 indexed by DAT_fff9094b), reads the value+complement pair via
PTR_FUN_0008e17c, computes ratio PTR_FUN_0008e184(DAT_fff9018c[chan], 10000, DAT_fff93942),
compares to calibration window, and classifies 0–5 by walking a 10-byte-stride threshold
table (DAT_fff94161); direction-gated copies DAT_fff93ad0[i] from DAT_fff90958 bits;
results DAT_fff934c4[i], flags DAT_fff93ac4/fff93aca/fff93ad6[i], pair rewrite
DAT_fff94f88[i]/DAT_fff94e00[i] with complement verification; summary bytes DAT_fff94165/
94166/94164/94168/94163; guard states DAT_fff94162 = 0/2/7/8. Caller 0x00088EF0. [I]

#### 0x0008E284 `meas_sig5_defaults` — data-accessor
Loads timeout default DAT_fff94780 from table. Caller 0x000893CA. [O]

#### 0x0008E29A `meas_sig5_process` — logic
Single-channel pipeline on table-selected input (PTR_DAT_0008e3f0): raw value via fn-ptr
PTR_FUN_0008e3f4 → DAT_fff93adc; state DAT_fff90924[idx] (>4→8) and mask bits
DAT_fff90921[idx]; conditions DAT_fff93ade bits 2/4 (speed DAT_fff93942, DAT_fff9411d),
0x100/0x200 below threshold by mask bits, 0x10 state<4, debounce DAT_fff9416a → bit 0x20,
timeout DAT_fff94780 → 0x40, extra 0x400; result selector DAT_fff9416b = 0/1/2; trigger
PTR_FUN_0008e404(idx) when 0x316 & 0x20 set. Uses helpers 0x00090DA2/0x00090D5C. Caller
0x00089330. [I]

#### 0x0008E408 `meas_sig6_defaults` — data-accessor
Loads timeout default DAT_fff94782 from table. Caller 0x000894B6. [O]

#### 0x0008E41E `meas_sig6_process` — logic
Structural twin of 0x0008E29A for a second input (PTR_DAT_0008e574, fn-ptr
PTR_FUN_0008e578): conditions DAT_fff93ae4, raw DAT_fff93ae2, debounce DAT_fff9416c,
timeout DAT_fff94782, result DAT_fff9416d, trigger PTR_FUN_0008e588. Caller 0x0008941C. [I]

#### 0x0008E58C `meas_speed_outputs_reset` — logic
Resets the shared speed output pairs to (0, complement) defaults via write fn-ptrs:
DAT_fff94f9e/94e16 (0,0xFFFF), DAT_fff94ff5/94e6d, DAT_fff94ff1/94e69, DAT_fff94ff3/94e6b
(0,0xFF) and the pair-copy PTR_FUN_0008e798 for DAT_fff94ff2/94e6a/fff94ff4/94e6c; also
reads back via 0x0008D526. Caller 0x00089508 (thunk from 0x000832AC init). [O]

#### 0x0008E5E8 `meas_speed_publish` — logic
Publishes the primary speed value with status: re-reads the pairs via fn-ptrs, writes
DAT_fff907ee (+complement) into DAT_fff94f9e/94e16, DAT_fff907f0 into DAT_fff93944, sets
validity byte pair DAT_fff94ff1/94e69 (1/0xFE) by speed threshold or DAT_fff84a44 passthrough,
status byte DAT_fff94ff5/94e6d = 8/0xF7 or 7/0xF8 by speed, extra state 8 when
DAT_fff9095b > 0xE, and states DAT_fff94108/94106/94107 (1/7/8) from DAT_fff84a45/4a46.
Called from 0x00089518 (mon_subsys_init chain). [I]

#### 0x0008E7B4 `mon_util_calc2` — logic
Per-channel utilization companion of the state/ratio monitor (called by 0x00089F5C):
target uVar1 from table; zero target → value 0 status 0; disabled when channel speed
DAT_fff94ed0 below table or DAT_fff94ed4[ch] == 0 → status 1; value = DAT_fff94ed4[ch] *
DAT_fff933a8[2i] / speed; validity uses tolerance around DAT_fff93100, filtered value
DAT_fff93420[2i] vs table, state checks ('0'/'1') → status 2/3/4 by threshold bands;
result DAT_fff934e8[2i], status DAT_fff93b16[ch]. [I]

#### 0x0008E8E4 `mon_substate_update` — logic
Per-channel substate machine (called by 0x00089F5C) for type-2 channels: phase counter
DAT_fff93a38[ch] 0–7 advancing with result codes DAT_fff93a4c[ch] ('\0','\x01','\x10','!',
' ','"','$','%','0') and selector DAT_fff93a42[ch] 0–4; phase logic gates on environment
fn-ptr 'U', speed DAT_fff94ed0, gear-state byte DAT_fff9030c[ch] (values 0–6 with 0x10 flag),
monitor states '@'/'A', ratio status DAT_fff93a2e and DAT_fff9340c; per-phase timeouts
DAT_fff947cc/fff947d6/fff947e0 loaded from tables via PTR_FUN_0008eb6c; forces
DAT_fff93a56[ch] result codes. [I]

#### 0x0008ECF0 `mon_substate_force` — data-accessor
Forces substate DAT_fff93a4c[ch] = arg and, for '\x10', loads hold time DAT_fff947d6[ch]
from table. Called by 0x00089F5C. [O]

#### 0x0008ED20 `tbl_curve_eval` — library
Evaluates a counted curve (ptr[0] = length, axis at ptr+1, values after): forwards to the
u16 interpolation 0x0008ED60. Callers 0x0008E8E4/0x0008D5BA. [O]

#### 0x0008ED40 `lib_ratio_scaled` — library
Returns (param_2 * param_1) / param_3, saturating at 0xFFFF; 0 when divisor 0. Used by the
6-channel pipeline 0x0008DF2E for its (value, 10000, base) ratio. [O]

#### 0x0008ED60 `tbl_interp1_u16` — library
1-D u16 curve interpolation: counted axis, index walk, edge clamp, linear interpolation,
rounding via float constant DAT_0008ede0. Caller 0x0008ED20. [O]

#### 0x0008EDEA `sched_batch_a` — dispatch
Table-dispatched job batch (no direct callers): invokes PTR_FUN_0008ef90 then
DAT_0008ef94 (resolved: 0x0008F62C, 0x0009181C). Same family as 0x0008EE02–0x0008EF1A. [I]

#### 0x0008EE02 `sched_batch_b` — dispatch
Interrupt-bracketed job batch: lock via PTR_FUN_0008efa0(0x10) (nop 0x0009665C), run
PTR_FUN_0008efa4 = interrupt-state restore (0x00096674, returns SR|0xF0). No other jobs
beyond the bracket pair. [I]

#### 0x0008EE12 `sched_batch_c` — dispatch
Interrupt-bracketed job batch: bracket pair plus PTR_FUN_0008efa8 and PTR_FUN_0008efac
(resolved: 0x0009140A, 0x000904FA). [I]

#### 0x0008EE30 `sched_batch_d` — dispatch
Interrupt-bracketed job batch: bracket pair plus three jobs PTR_FUN_0008efb0/b4/b8
(resolved: 0x000901A0, 0x00091810, 0x00091500). [I]

#### 0x0008EE52 `sched_batch_e` — dispatch
Interrupt-bracketed job batch: bracket pair plus six jobs PTR_FUN_0008efbc..0008efd0
(resolved: 0x00090F66, 0x0008FBDE, 0x00090D5A, 0x0008F70C, 0x000918F6, 0x00091ACA). [I]

#### 0x0008EE82 `sched_batch_f` — dispatch
Interrupt-bracketed job batch: bracket pair plus seven jobs PTR_FUN_0008efd4..0008efec
(resolved: 0x00091C86, 0x00090B98, 0x00090ACC, 0x00090D58, 0x00091DAA, 0x00092A84,
0x000921A0). [I]

#### 0x0008EEB4 `bus_enable2_job` — dispatch
Table-dispatched job: one fn-ptr call (PTR_FUN_0008eff0, resolved 0x00091F68) then sets
bit 1 in the three enable registers DAT_ffffd000/ffffd800/ffffe000. [O]

#### 0x0008EEDC `sched_batch_g` — dispatch
Job batch with bus/channel enables: ORs bit 0 into DAT_ffffd000, DAT_ffffd800, DAT_ffffe000
(likely peripheral/port enables) then runs seven fn-ptr jobs (resolved: 0x0008FB96,
0x00096010, 0x00091FA4, 0x000920F8, 0x00091A80, 0x000961F0, 0x00091DCC). [I]

#### 0x0008EF1A `sched_batch_h` — dispatch
Job batch (no interrupt bracket): nine fn-ptr calls (resolved: 0x000919A0, 0x00090D12,
0x000962E2, 0x0009136E, 0x00090F7A, 0x0008F9EC, 0x00090010, 0x00091ED4, 0x00091884, then
0x00090ACC, 0x00091E30, 0x00090AEC, 0x00091DD2, 0x00091852 in the tail). [I]

#### 0x0008EF56 `sched_one_91f3e` — dispatch
Single-job wrapper: calls PTR_FUN_0008f044 (resolved 0x00091F3E). Cluster of 3 —
byte-equivalent copies 0x0015C94C and 0x0015C968 live in r14_app and forward to their own
PTR_FUN slots (both resolved 0x0009A8D4). [O]

#### 0x0008EF6A `sched_retry_call` — logic
While DAT_fff90968 != 0, repeatedly calls PTR_FUN_0008f048(args) (resolved 0x0008F04C)
until it returns nonzero — a spin-wait on flag DAT_fff91100. [O]

#### 0x0008F04C `flag_91100_poll` — stub
Returns DAT_fff91100 != 0. [O]

#### 0x0008F058 `meas_stats_task` — logic
Periodic statistics/aggregation over the measurement buffer (base via PTR_FUN_0008f304):
every 20 ticks computes tick delta DAT_fff910fc = DAT_fffff220 − DAT_fff91100; mean and
mean-absolute-deviation over 16 ten-byte records (written back through
PTR_FUN_0008f30c), min tracking DAT_fff907fa; averages over four eight-record blocks
(DAT_fff90184/90186 etc., DAT_fff9002f gate at 0x3333); accumulators DAT_fff900b0..900dc
with counts; paired-channel min/max stats with 0x1168 threshold (DAT_fff90590..5a6).
Callers='-' (task table). Matches the measurement-protocol data path — spec 10. [I]

#### 0x0008F4FE `flag_90978_lt16` — stub
Returns DAT_fff90978 < 0x10. [O]

#### 0x0008F50C `mon2_reqflag_clear` — data-accessor
Clears byte [7] of DAT_fff90018 (param 1) or DAT_fff90010. Callers 0x0008C3BC/0x00085828/
0x0008C7A8. [O]

#### 0x0008F524 `mon3_cnt900b6_clear` — stub
Stores 0 into DAT_fff900b6. Caller 0x00086916. [O]

#### 0x0008F52E `meas_index_read` — data-accessor
Measurement-index lookup: 0–99 → DAT_fff9018c[i], 100–199 → DAT_fff901c4[i−100], 200–249 →
DAT_fff901d4[i−200], 0xFD → 3300, 0xFF → 5000, else 0. Callers 0x0008305C/0x0008CAB4.
Matches the RAM-read-by-index measurement protocol — spec 10 §5. [O]

#### 0x0008F5A0 `meas_task_timing_chk` — logic
Ramp-counts four init/state counters (DAT_fff900b6, 9002f, 90017, 9001f to 2) and
supervises the stats-task cadence: OK when DAT_fff91100 == 0, or the delta DAT_fff910fc is
within ROM window 0x4A38–0x5208 and DAT_fffff220 − DAT_fff91100 < 0x59D9, or an fn-ptr
check returns 1; otherwise reports error 0x231 via PTR_FUN_0008f6d8. Caller 0x0008EDF6. [I]

#### 0x0008F62C `meas_slew_publish` — logic
Measurement job (from 0x0008EDEA): bracket(0x40), publish fn-ptrs, then slew-limits
DAT_fff907f6 toward the delta of DAT_fff907e4 vs DAT_fff907e6 (clamp table PTR_DAT_0008f6ec,
fall rate PTR_DAT_0008f6f0) and converts the min tracker DAT_fff907fa (reset 0xFFFF) into
DAT_fff907fc scaled by float DAT_0008f6f4. [I]

#### 0x0008F70C `meas_avg_task` — logic
Periodic aggregation job (from 0x0008EE52): scales DAT_fff907f2 down; divides the 28-entry
accumulator DAT_fff900e8 by DAT_fff907de with per-entry float gains into measurement array
DAT_fff9018c, the 4-entry DAT_fff90174 into DAT_fff901d4 and 7-entry DAT_fff90158 into
DAT_fff901c4 (measurement-protocol slots, spec 10); re-arms RAM-integrity markers
DAT_fff88728/fff8816a/fff884da = 0x5B97 with counter DAT_fff907fe; swaps marker bytes
DAT_fff908f7/9, f8/a; error 0x232 when fn-ptr checks fail or a marker was wrong; then
averages the 4-phase duty accumulators DAT_fff900e0/900e4 over 8 samples
(DAT_fff908f5/908f6, cyclic 0–3) into DAT_fff90800/fff90806 and updates the
DAT_ffffe830/ffffec30 PWM-group select bits. [I]

#### 0x0008F9EC `meas_task_init` — logic
Measurement-subsystem reset job (from 0x0008EF1A): re-arms ramps DAT_fff90007/9000f/90037/
9002f = 2, integrity markers 0x5B97, DAT_fff907fe/908f6 = 0, mux select registers
DAT_ffffe830/ffffec30, timing seeds DAT_fff910fc = 0x4E20 and DAT_fff91104 = 0x14, plus
peripheral defaults DAT_fffffe96/94, ffffea6/a4, ffffec6/c4, ffffed6/d4 (0x32/0xFFD8/
4000/0xFFB0/0xF64). [I]

#### 0x0008FB1C `eeprom_wait_flag40` — logic
Strobes EEPROM control DAT_fffe3902 (& 0xDF | 0x10), delays 0x32 loop iterations, returns
(DAT_fffe391e & 0x40) != 0. Caller 0x00072A22 (EEPROM session driver, unit 02/04 area). [O]

#### 0x0008FB42 `eeprom_ctl_set30` — stub
Sets bits 0x30 in EEPROM control DAT_fffe3902. Caller 0x000729C8 (eeprom_session_step). [O]

#### 0x0008FB50 `eeprom_flag40_clear` — stub
Returns (DAT_fffe391e & 0x40) == 0. Caller 0x0007A7E8. [O]

#### 0x0008FB5C `eeprom_flag40_ready` — stub
Returns enable (*DAT_0008fbe0 & 1) && (DAT_fffe391e & 0x40) == 0. Cluster of 2: member
0x0008FB7E tests bit 0x20 instead of 0x40 (different EEPROM status flag). [O]

#### 0x0008FB74 `eeprom_enabled` — stub
Returns (*DAT_0008fbe0 & 1) != 0. Callers 0x00072A22/0x0007A7E8/0x000729C8. [O]

#### 0x0008FB96 `eeprom_ctrl_init_seq` — logic
Delays 0x96 iterations, then DAT_fffe3902 = (x & 0x77) | 0x30 and DAT_fffe3906 |= 0xB8 —
EEPROM control init strobe. Caller 0x0008EEDC (sched_batch_g). [O]

#### 0x0008FBE4 `pwm_duty_classify` — logic
Classifies a PWM-input duty word into a 0–0xFF code DAT_fff9090b[ch]: scales by float
DAT_0008fe74, clamps ≥ 0x100 → 0xFF, > 0x7D → 0, < 0x20 keeps bit 0x20 flag, mid-band
((x+2)>>2); mode DAT_fff9091b == 1 with gate byte 0 forces 0; for channels < 3 stores the
code and marks change flag DAT_fff90212[12*ch] = 1. Callers 0x00090010/0x0008FDB6/0x00089648. [I]

#### 0x0008FC5E `pwm_period_set` — logic
On period change for channel: stores DAT_fff9083c[ch] = period, DAT_fff9083a[ch] = period<<1;
computes prescaler uVar8 = DAT_0008fe7c/period − 1 (mode-1 scaled/clamped, else bit7 from
ROM); below the stored prescaler DAT_fff90900 → latch DAT_fff90919; otherwise walks the
group list DAT_fff9080d[6*ch] writing the prescaler and programming the timer pattern
buffers (duty 100, period<<1, enable bits via (idx,bits) encoding at the 0xFFFE83xx-area
registers). Callers 0x00090010/0x00089648. [I]

#### 0x0008FD94 `pwm_pair_enabled` — data-accessor
Returns 1 when pair index (ch>>1 < 5) has its bit in the ROM enable byte *PTR_DAT_0008fe8c.
Callers 0x00084644/0x00084392/0x000901A0/0x00083DB0. [O]

#### 0x0008FDB6 `pwm_duty_rescale` — logic
Rescales the five duty/prescaler bytes (DAT_fff90900 &0x7F, DAT_fff908fb &0x3F with bit6,
DAT_fff90905 &0x3F) by floats at PTR_DAT_0008fe80 when mode DAT_fff9091b == 1; with the
extra gate byte 0 then zeroes the three duty outputs via 0x0008FBE4(i,0). Callers
0x00090010/0x000901A0. [O]

#### 0x0008FE98 `pwm_chan_map_init` — logic
Builds the channel maps from three ROM mask words (PTR_DAT_0009014c): per group decodes ten
2-bit codes into prev/next channel lists DAT_fff901dd/fff901de, position DAT_fff901e7,
count DAT_fff901dc, valid DAT_fff90212[12*g], channel→group DAT_fff90826/27; then builds
job descriptors at DAT_fff90630 (stride 0x88): counts, sample buffer pointers, callback
PTR_LAB_00092738/3c, RAM buffer addresses (0xFFF882xx/0xFFF908xx), and the per-channel
pattern words (0xB800/0xC000/0xD800/0x7800/0xBE00/0xD800/0x7E00) by the 2-bit code. [I]

#### 0x0008FF66 `pwm_group_setup` — logic
Configures one PWM group: for each member channel (mask param_2) records it in
DAT_fff9080d[6*grp] / DAT_fff9090f, calls timer init PTR_FUN_00090158(idx, bits, 1,
period*2, 100) and writes compare values (period*2 − 0x28 − table offsets) into the
channel register (via the (idx,bits) encoding). Caller 0x00090010. [I]

#### 0x00090010 `pwm_subsys_init` — logic
PWM/TIF subsystem init job (from 0x0008EF1A): seeds the three base periods via
0x0008FBE4, builds channel maps (0x0008FE98), group 0 setup (0x0008FF66) and prescaler
(0x0008FC5E), rescale (0x0008FDB6); then 5×2 peripheral channel setups
PTR_FUN_00090170(chan, RAM, buf) with control writes at −0x580 offset registers;
initializes the 10 state bytes DAT_fff90812/fff9081c = 0x32; arms the three group
sequencers DAT_fff90214[3i] = 3000/3100/3200 (+ register writes DAT_fffff58d/f) and
DAT_fff9090a = table byte. [I]

#### 0x00090118 `pwm_duty_to_value` — logic
Converts duty state to a scaled value: tag 0x21 → DAT_fff90230[ch] (clamped 0x2FFA), tag
0x43 → 0x2FFA, else 0; stores the clamp DAT_fff90244[ch] and returns (value +
DAT_fff9090a) * float DAT_000903cc. Caller 0x000901A0. [I]

#### 0x000901A0 `pwm_input_task` — logic
PWM-input (TIF) periodic task (from 0x0008EE30): (1) per 10 channels compares the raw
period read (PTR_FUN_000903d0) against 6× value on DAT_fff90244 → error 0x238 on drift
> 0xF; (2) mode byte DAT_fff9091b from DAT_fff9110c == -0x43 and DAT_fff9110d bands
(1/2/0), on change rescales (0x0008FDB6); (3) per 10 channels state sync DAT_fff9081c
(with 0x0008FD94 gate → '2') and duty-band flags into uStack_34; (4) per 5 pairs publishes
states/status words into the DAT_fff91110-block (stride 0x1C: 0x11C/0x11F/0x120 words),
combines the complement tags 0x21/0x32/0x43 into status bits, computes values
(0x00090118) and duty outputs DAT_fff91118/120 from DAT_fff90320/322 * DAT_000903e4 with
latch states DAT_fff90830/31; (5) DAT_fff90919 state machine re-applies group periods.
Spec 02 TIF/PWM inputs. [I]

#### 0x000904FA `sol_feedback_task` — logic
Solenoid-driver feedback job (from 0x0008EE12): reads the 10 driver channels via
PTR_FUN_0009067c(chan, out12, DAT_fff90334+12c) (5 pairs × 2); status nibble 0x4 → values
into DAT_fff902bc/2be (raw) and DAT_fff902e4/2e6 (zero-adjusted, complemented on even
index); rotates both pairs into DAT_fff9026c/2b and DAT_fff90294/9b — the ratio-monitor
inputs of 0x00089A8C; computes per-pair current scale DAT_fff84a30 = DAT_0009068c/(class+1)
from duty-class bits; assembles status words DAT_fff9030c[i] (bits 0x1000/0x2000/0x4000
from the DAT_fff91110 block) and signed currents DAT_fff90258[i] = value11 * scale.
Spec 03 (SPI devices) / spec 12 (solenoid channels). [I]

#### 0x0009076E `meas_mux_sample` — logic
One AN-mux sample pass for a group: walks the prev/next channel lists (DAT_fff901dd) and
reads both channels through PTR_FUN_00090954 into DAT_fff90334+12*ch, storing raw>>2 into
DAT_fff902bc[param + 2*ch] and zero-adjusted (DAT_fff90338[3*ch]) into DAT_fff902e4.
Caller 0x00090844. [I]

#### 0x00090844 `meas_mux_seq_step` — logic
AN-multiplexer group sequencer (table-dispatched): state byte DAT_fff90213[12*g]: 0 →
+500 (marker DAT_fff94434 = 0x17) → 1; 1 → push duty codes DAT_fff9090b[g] into the
channel state words DAT_fff91119/21 (with DAT_fff90830 latch), load phase durations from
ROM table (indexed by DAT_fff9090b & 0x1F) into DAT_fff9020c/0e/10 → 2; 2 → sample A
(0x0009076E(0,g)) → 3; 3 → sample B (0x0009076E(1,g), duration −500) → 0; each step adds
the duration to the group counter DAT_fff90214[3g] (written *0x100 to the −0xA70
registers) and returns overrun vs the −0xAE0 timer. Spec 04 multiplexed AN0. [I]

#### 0x00090AEC `unknown_dev_init_90aec` — unknown
Init job (from 0x0008EF1A) for an unidentified peripheral: verifies register state at
0xFFFF1000 against a mask pair (error 0x233 via PTR_FUN_00090c58); selects a variant by
calibration byte 1/2 and DAT_fff94713/fff94711 (PTR_FUN_00090c60 vs 00090c64); loads mode
byte DAT_fff9091d from table; runs a fn-ptr chain (0x96xxx: 00090c6c, 00090c70(2),
00090c74(0/1/2)); clears DAT_fff9091e. Callees live outside this unit. [I]

#### 0x00090B98 `unknown_service_90b98` — unknown
Service job (from 0x0008EE82): one-shot '.', reads identity via PTR_FUN_00090c80(ROM id
DAT_00090c7c) into DAT_fff903ac with mismatch latch DAT_fff9091c; manages a periodic
hardware timer at 0xFFFE1078/0xFFFE107C (reload 0xFFFFFF − count, callback
PTR_FUN_00090c90, countdown DAT_fff90848); re-reads the mode byte DAT_fff9091d from the
calibration table and reports error 0x23F when it reads odd. [I]

#### 0x00090C94 `periph_3802_flag_set` — data-accessor
Stores DAT_fff9091f = (param == 1) and sets/clears bit 2 of DAT_fffe3802, strobing bit 2
of DAT_fffe3806. No direct callers (fn-ptr table). [O]

#### 0x00090CBA `eeprom_ctl_bit3` — stub
Sets (param 0) or clears (param nonzero) bit 3 of EEPROM control DAT_fffe3902. Caller
0x00090D12. [O]

#### 0x00090CC8 `eeprom_ctl_bit2` — logic
EEPROM control bit 2: param != 1 clears bits 0–1 of DAT_fffe3902; param 1 delays 0x4B
iterations then sets bit 2 and clears bit 0. Caller 0x00090D12. [O]

#### 0x00090CF6 `periph_3802_bit7_set` — data-accessor
Stores DAT_fff90920 = param and sets/clears bit 7 of DAT_fffe3802. Callers 0x00090D12/
0x0008CE48. [O]

#### 0x00090D12 `periph_init_seq` — init
Init job (from 0x0008EF1A): strobes control bits across DAT_fffe3802/3806, DAT_fffe3906,
DAT_ffffc860/ffffc86c and DAT_fffe3902/3906, calling 0x00090CBA(1), 0x00090CC8(1),
0x00090CF6(0). [O]

#### 0x00090D5C `meas_scale_read` — library
Returns (K[ch] * DAT_fff90856[2ch]) / (gain_table[ch] * DAT_00090fc0) — scaled physical
value from a raw input; K and gain come from ROM tables. Callers 0x0008E29A/0x0008E41E. [I]

#### 0x00090DA2 `spi_dev_xfer` — logic
Per-device transfer: PTR_FUN_00090fcc(dev_id, descriptor, RAM work area) then
PTR_FUN_00090fd4(work, &DAT_fff904f8+0x18*dev, config ptr) and PTR_FUN_00090fd8 — device
read/write cycle on one of three devices. Caller 0x00090F7A. [I]

#### 0x00090DF4 `data_90448_read32` — data-accessor
Returns the u32 at DAT_fff90448 + ch*0xC. Callers 0x0007C3AA/0x0007C29E (outside unit). [O]

#### 0x00090E04 `data_9044e_read16` — data-accessor
Returns the s16 at DAT_fff9044E + ch*0xC. Callers 0x0007C3AA/0x0007C29E. [O]

#### 0x00090E14 `spi_dev_init` — logic
Initializes one device (id PTR_DAT_00090fc4[dev], −1 absent): computes scale float
DAT_fff904f8+0x18*dev (gain table / K), seeds config word, runs the fn-ptr chain
PTR_FUN_00090fe4/0e8/0ec/0f4/0fc (configure + verify), reads back counts into
DAT_fff9084a/90850/9085c and DAT_fff90927[dev], and derives the mask byte DAT_fff90921[dev]
(2-bit lanes of DAT_fff9097a or 0x80/4). Caller 0x00090F66. [I]

#### 0x00090F66 `spi_devs_init` — dispatch
Runs 0x00090E14 over the three devices (from 0x0008EE52). [O]

#### 0x00090F7A `spi_devs_start` — logic
Job (from 0x0008EF1A): for each present device sets control bits (dev*0x20 − 0x580
registers, pattern 0x14) and starts the transfer 0x00090DA2. [I]

#### 0x00091058 `port_f58e_bit0_clear` — stub
Clears bit 0 of DAT_fffff58e and zeroes DAT_fff9092a. Caller 0x0006C1EA (outside unit). [O]

#### 0x00091086 `err_bits_set` — logic
Bit-set API into a global record: gated by fn-ptr check on buffer PTR_LAB_00091270, copies
7 template words to DAT_fff8849e, ORs the four param bytes into DAT_fff884a4/fff884a2/
fff884a0/fff8849e, clears DAT_fff884b8, then publishes via PTR_FUN_0009127c. Caller
0x0006C1BA (outside unit). [I]

#### 0x000910EE `serial_resp_handler7` — logic
Table-dispatched response callback (context struct: id at [0], buffer at +8, state at +0x10):
timestamps DAT_fff90544 = DAT_fffff220−0x29, counts DAT_fff9054d; when the state byte is
'@' and all 7 words match (x & 0xF900) == 0xA800, extracts three payload bytes into
DAT_fff90550/51/4f, clamps DAT_fff800ed (&0x3F) and sets DAT_fff90540 bit 0; failure sets
DAT_fff90541 bit 0 and bumps the per-channel error counter DAT_fff94218[2*id]. [I]

#### 0x00091190 `err_bits_set2` — logic
Second bank of the bit-set API (twin of 0x00091086): copies 2 template dwords to
DAT_fff88120, ORs DAT_fff9054e into it, clears DAT_fff8812e, publishes. Caller 0x0006C1BA. [O]

#### 0x000911D8 `serial_resp_handler4` — logic
Four-word twin of 0x000910EE: validates (x & 0xF900) == 0xA800 on 4 words, extracts
puVar2[1..3] into DAT_fff9054f/50/51, flags DAT_fff90540/41 bit 1, error counter
DAT_fff94218[2*id]. [I]

#### 0x0009128C `pwm_duty_ratio` — library
Returns a 0–1000 ratio from the two counters DAT_fff9093f[ch] and DAT_fff9093a[ch]:
1000*(2a−1)/((2a+2b)−2), 0 when a == 0, 1000 when b == 0. Caller 0x0007B358 (outside unit). [O]

#### 0x000912C6 `pwm_write_duty_period` — logic
Writes period and duty for one PWM channel: DAT_fff90888[ch] = period, duty zeroed when
utilization DAT_fff9087e[ch] == 0, programmed into the (idx,bits)-encoded registers
(−0x7E0 period, −0x7D8 duty). Caller 0x0006A680 (outside unit). [O]

#### 0x00091318 `pwm_write_duty` — logic
Writes only the duty word for one PWM channel (same encoding, zeroed when utilization 0).
Caller 0x0006A680. [O]

#### 0x00091350 `pwm_ch_enabled` — data-accessor
Returns 1 when channel (< 5) has its bit in the ROM enable mask *DAT_000914e0. Callers
0x00084DA0/0x000849D4. [O]

#### 0x0009136E `pwm_outputs_init` — logic
Init job (from 0x0008EF1A): for the five PWM output channels calls timer setup
PTR_FUN_000914e4(idx, bits, cfg byte, cfg word, 0), writes the compare word, loads targets
DAT_fff90888[i] (table) and DAT_fff90892[i] (config), and clears/sets the enable bits
(−0x7FC/−0x7FE) for the first two lanes. [I]

#### 0x0009140A `pwm_capture_enable_job` — logic
Job (from 0x0008EE12): for the five PWM-input channels — disabled lanes get
PTR_FUN_000914f8 + flag DAT_fff9092b[i] = 1; enabled lanes copy the counters
DAT_fff9055c/5e into DAT_fff9093a/3f[i], call PTR_FUN_000914f4 and sync duty
DAT_fff90874[2i] = DAT_fff908b0[2i] with flags DAT_fff90930. Then latches the discrete
port states DAT_fffe381f bits 3/4/5/6 into DAT_fff90935..939 (DAT_fff90939 = 2). [I]

#### 0x00091500 `pwm_duty_out_task` — logic
Solenoid/PWM output job (from 0x0008EE30): copies the five targets; per channel masked by
PTR_DAT_00091748 bit 1: utilization DAT_fff9087e[i] = (gate & 1) ? DAT_fff9086a[i] : 0
(the value computed by 0x0008ADF4); standalone channels verify curve
PTR_FUN_0009174c(util, word) against the readback (error 0x238) and write the duty register;
pair channels write duty from the curve with period target DAT_fff90892/90888 and toggle
the enable bit. Closes the loop utilization → timer duty. [I]

#### 0x00091758 `switch_status_build` — logic
Builds the switch/status byte DAT_fff907d6 = DAT_fff90963 & 0x1D | 0x20/0x40/0x80 for each
of three table-selected channels whose state DAT_fff90944[ch] == 'c', | config<<5 with
conditional clears (fn-ptr gated), bit 3 by DAT_fff90952; second fn-ptr check selects
DAT_fff90965 = (d6 & 0xFE) or 1. Callers 0x00091804/0x00091810. [I]

#### 0x00091804 `switch_task_a` — logic
Job wrapper (from 0x0008EDF6, outside unit): runs 0x00091758 then PTR_FUN_0009187c
(→ 0x00092E80). Cluster of 2 with 0x00091810 — identical except the trailing job. [O]

#### 0x0009181C `switch_states_copy` — logic
Job (from 0x0008EDEA): expands bits 0x20/0x40/0x80 of DAT_fff907d1 into the three
per-channel booleans DAT_fff90947[tbl[0..2]] and DAT_fff90959 = bit 1 — companion of
0x00091758. [O]

#### 0x00091852 `periph_380x_strobe` — stub
Clears the low nibble of DAT_fffe3812 and bit 0 of DAT_fffe3802, sets bit 0 of
DAT_fffe3806. Job from 0x0008EF1A. [O]

#### 0x00091884 `spi6_devs_init` — logic
Initializes six devices (from 0x0008EF1A): per device sets control bits (dev*0x20 − 0x580,
pattern 0x14), runs PTR_FUN_00091998(dev, RAM, &DAT_fff905d8+12*i), clears
DAT_fff908c0[i] and arms DAT_fff905a8[i] = 1. Spec 03 (SPI device schedule). [I]

#### 0x000918F6 `spi6_devs_poll` — logic
Polls the six devices (from 0x0008EE52): PTR_FUN_0009199c(dev, RAM, result); status byte
DAT_fff905e0[i] bit 0 → ready flag DAT_fff908ba[i] = 1; high nibble 0x20/0x30 → class 8,
0x40 → class 4 into DAT_fff908c0[i]; copies the result dwords to DAT_fff905c0+4*i and
DAT_fff905a8[i]. [I]

#### 0x000919A0 `pwm_out_init` — logic
Single PWM-output channel init: copies the config word into DAT_fff908c6 and calls timer
setup PTR_FUN_00091a28(id, cfg, mode, word, 0). Job from 0x0008EF1A. [I]

#### 0x000919D6 `pwm_out_duty_set` — data-accessor
For param 0: clamps duty to DAT_fff908c6, stores DAT_fff908c8 and writes it to the
duty register ((id*0x40 + bits*2) − 0x7D8). No direct callers (table-dispatched). [O]

#### 0x00091A30 `timer_cmp_set_ec` — logic
Programs the second timer group: DAT_fffec012 = value − offset (min 2), DAT_fffec014 =
value − 1, clears bit 0 of DAT_fffec011, sets bit 0x40 of DAT_fffec010. Caller 0x00067A7C
(outside unit). [O]

#### 0x00091A80 `thunk_91a98` — stub
Single indirect call through DAT_00091a98 (resolved target outside unit). Caller
0x0008EEDC. [O]

#### 0x00091A9C `env_mode_gate` — logic
Returns 0 when DAT_fff907cf == −0x44 and DAT_fff907d0 < 0x10, else 1 — environment gate
used by the self-test sequencer (0x0008BDD6). [O]

#### 0x00091AB8 `cfg_bytes_load` — data-accessor
Loads the two config bytes DAT_fff90962/93963 from ROM tables. Callers 0x00091C86/
0x00091DD2. [O]

#### 0x00091ACA `serial_rx_process` — logic
Processes received frame data (job from 0x0008EE52): when armed (DAT_fff9095b != 0) —
latches switch states DAT_fff84a44/4a45/4a46 and DAT_fff90954 from DAT_fff90979/fff907d1;
rolling 4-slot average of raw DAT_fff907cc into DAT_fff908ca (reset paths), direction
byte DAT_fff9095e from DAT_fff907d3 bits 4/5, counter swap DAT_fff908cc/908ee (0x8001);
first-frame path copies DAT_fff9097a/fff907d1/fff907d3 into DAT_fff9095f/960/961 and
builds DAT_fff800ed from DAT_fff907d2; later frames validate DAT_fff90971/972
((&0x87)==0x81) → DAT_fff9094f/50/4e (0x2D/0x2E); finally mode decode DAT_fff907cf ==
−0x44 with DAT_fff907d0 bands → mode DAT_fff90966, variant DAT_fff9094b (1)/DAT_fff9094c
(6) — the variant index consumed by 0x0008DF2E. [I]

#### 0x00091C86 `power_state_update` — logic
Job (from 0x0008EE82): builds DAT_fff9097b from DAT_fff90957/962/94d/95a; selects
PTR_FUN_00091e18 vs PTR_FUN_00091cf8(1) by DAT_fff90953/DAT_fff907d5 bit 0; latches
DAT_fff800ec = 1 when DAT_fff84a46 == 1 and the free-run counter DAT_fffff220 passed
199999, or DAT_fff90951 == 6 (with DAT_fff907d4 bit 6); reshuffles DAT_fff907d4 bits
0/1/2/3/7 from DAT_fff90951/fff907d5/fff90964. Looks like ignition/power-state handling. [I]

#### 0x00091DAA `serial_rx_arm` — logic
Job (from 0x0008EE82): three fn-ptr calls then arms the serial processing
(DAT_fff9095b = 1 if 0). [O]

#### 0x00091DCC `thunk_937cc` — stub
Single indirect call through DAT_00091e28 (resolved 0x000937CC, outside unit). Caller
0x0008EEDC (sched_batch_g). [O]

#### 0x00091DD2 `serial_rx_init` — logic
Init job (from 0x0008EF1A): resets the receive state (DAT_fff90953/94d/94a, counters
DAT_fff908ee/908cc = 0x8000), loads config bytes (0x00091AB8), DAT_fff90957 =
DAT_fff90962 & 0xF, DAT_fff9095b/94e = 0; on fn-ptr check copies the 2-dword template
into DAT_fff88414 (error-bit bank) and publishes. [I]

#### 0x00091E34 `sfr_f460_set2` — stub
DAT_fffff460 = (x & 0xF8) | 2. Caller 0x000901A0. [O]

#### 0x00091E42 `sfr_f488_set200` — stub
DAT_fffff488 = 0x200. Caller 0x00091ACA. [O]

#### 0x00091E4E `periph_c80_enable` — stub
Sets bits 1:0 of DAT_ffffc80e and DAT_ffffc80c, DAT_fff90967 = 1. Caller 0x00085788
(outside unit). [O]

#### 0x00091E6C `periph_c80_disable` — stub
Clears those bits, DAT_fff90967 = 0. Caller 0x00085788. [O]

#### 0x00091E86 `time_delta_check` — logic
Timing probe: brackets with fn-ptrs, executes a 7-iteration delay loop and sets
DAT_fff90968 = (elapsed DAT_fffff220 delta > 1); if the delta is short and an fn-ptr check
returns != 1, reports error 0x230. Callers 0x00091F68/0x00091FA4/0x00091FF8. [I]

#### 0x00091ED4 `sfr_f4xx_pair_init` — init
Initializes a symmetric peripheral pair at 0xFFFFF46x/0xFFFFF48x: descriptor 0xFFE0C100
and 0x100 into DAT_fffff478/fff468 and DAT_fffff498/fff488, mode |3 on f462/f482,
DAT_fffff400 = (x & 0xF9) | 6, DAT_fff90967 = 1. No direct callers (job table). [O]

#### 0x00091F3E `clk_ready_check` — logic
Reports error 0x48 when bit 7 of DAT_fffe0420 is clear (clock/PLL ready flag), copies
DAT_ffffff48 → DAT_fff90628 and calls PTR_FUN_0009200c(0x3FF). Reached via the single-job
wrapper 0x0008EF56. [I]

#### 0x00091F68 `clk_init_variant_job` — init
Init job (from 0x0008EEB4): latches the hardware-variant flag DAT_fff94714 =
(DAT_ffff1000 & 4), then the fn-ptr init chain with the same 700/4000/0x103 signature and
the timing probe 0x00091E86 — variant-aware twin of 0x00091FA4. [I]

#### 0x00091FA4 `clk_pwr_init_job` — init
Init job (from 0x0008EEDC): eleven fn-ptr init calls, PTR_FUN_0009201c(700),
DAT_ffffff46 = 4000, DAT_fff90628 = DAT_ffffff48, PTR_FUN_0009200c(0x103), then the
timing probe 0x00091E86. Same 700/4000/0x103 signature as the clock/power init documented
in unit 02 (0x0001D2xx area) — cross-region twin. [I]

#### 0x00091FF8 `thunk_time_delta_check` — stub
Tail-jump to 0x00091E86. Caller 0x0008EE82. [O]

#### 0x0009203C `lib_scale_under1000` — library
Returns value * ratio * float DAT_0009208c when ratio < 1000, else value unchanged.
Caller 0x00091500. [O]

#### 0x00092060 `timer_reload_f5b8` — logic
Programs the timer at 0xFFFFF58x: DAT_fffff5b8 = value << 8, DAT_fffff585 |= 4,
DAT_fffff58c &= ~4, DAT_fffff58e |= 4. Caller 0x00092530. [O]

#### 0x00092094 `flag_90969_set` — stub
Stores its byte into DAT_fff90969. Caller 0x00085160 (outside unit). [O]

#### 0x0009209C `flag_90969_read` — stub
Returns DAT_fff90969. Caller 0x00091758. [O]

#### 0x000920A4 `state_9096a_update` — logic
On change of DAT_fff9096a: value 0 calls PTR_FUN_00092104, nonzero calls
PTR_FUN_00092100 (enable/disable pair outside unit). Caller 0x00091C86. [O]

#### 0x000920C6 `wdt_service_magic` — logic
Watchdog service: reads the magic register DAT_fffe0002, tracks the minimum of
0xFF − value in DAT_fff94996, then writes the service magic 0x5ADB — matches the
watchdog magic documented in units 02/04 (0x5AB7/0x5ADB selected by the 0xFFFF1000
variant bit). Callers 0x00072964/0x0006C1BA (outside unit). [O]

#### 0x000920F8 `thunk_92108` — stub
Single indirect call through DAT_00092108 (target outside unit). Caller 0x0008EEDC. [O]

#### 0x0009218C `soldiag_errflag_set5` — logic
ORs the given byte into the error-flag slot (offset +1) of all five per-channel
diagnosis records at DAT_fff91108+0x1C*i. Callers 0x000921E0/0x00092298/0x000923BC/
0x00092530. [O]

#### 0x000921A0 `errbits_bank_init3` — logic
Error-bit-bank initializer (twin of the 0x00091086 family): on fn-ptr gate copies a
5-dword template into DAT_fff8816c, clears DAT_fff88192, publishes via
PTR_FUN_00092424/00092428. Caller 0x00090ACC (outside unit). [O]

#### 0x000921E0 `solresp_handler_2w` — logic
Table-dispatched SPI response handler (context struct, 5 channels × 2-word frames with
(x & 0xFF00) == 0xA200): stores bytes into DAT_fff9110d/fff9110c[0x1C*i], sets OK bit 0
in DAT_fff91108[i] or error bit 0 in DAT_fff91109[i]; wrong state ('@' expected) →
0x0009218C(1); ends with the indirect completion call PTR_FUN_0009242c. [I]

#### 0x0009225C `errbits_bank_init4` — logic
Copies a 15-word template into DAT_fff884dc, clears DAT_fff88516, publishes via
PTR_FUN_00092428 — another error-bit bank. No direct callers (job table). [O]

#### 0x00092298 `solresp_handler_3w` — logic
Response handler (5 channels × 3-word 0xA200 frames): timestamps DAT_fff9062c, clears
DAT_fff9096b; stores status bytes into DAT_fff9111a/fff91122/fff91110[0x1C*i], updates
bits 0/1/2 of DAT_fff9110e[i] (keep-when-set semantics) and ORs 0x42 into the OK/error
flags; wrong state → 0x0009218C(2). [I]

#### 0x00092344 `solstat_publish` — logic
Publish job (from 0x000901A0): copies a 20-dword template into DAT_fff88194 then ORs the
five per-channel status bytes (DAT_fff91108+7/9/10/11, +0x1C stride) into the 5×5-word
matrix, clears DAT_fff88232, publishes via PTR_FUN_00092428. [O]

#### 0x000923BC `solresp_handler_8w` — logic
Response handler (5 channels × 8-word 0xA200 frames): compares the frame fields
(6-bit word, three chars at +0x33/+0x3D/+0x47) against the expected buffer (context +4);
mismatch → bit 4 in DAT_fff9110a[i], match → bit 4 in DAT_fff91108[i]; wrong state →
0x0009218C(4). [I]

#### 0x000924B0 `solstat_publish2` — logic
Publish job (from 0x000901A0): copies a 45-word template into DAT_fff88518, ORs
per-channel words DAT_fff91108+0xC/0x14 and bytes +7/+0x10/+0x18 into the 5-slot matrix,
clears DAT_fff885ca, publishes via DAT_00092728. [O]

#### 0x00092530 `solresp_handler_9w` — logic
Response handler (5 channels × 9-word 0xA000 frames): first arms PTR_FUN_0009272c(0xED8);
counter DAT_fff9096b ≥ 2 → error 0x40 flags; per channel compares two 11-bit words
(&0x7FF) and chars at +0x33/+0x47 against the expected buffer, setting presence bits
2/8 in DAT_fff9110e[i] by value != 0 and OK/mismatch bit 0x10; wrong state →
0x0009218C(0x10) or (0x40). [I]

#### 0x00092812 `pwm_job_result_pub` — logic
Publishes a PWM-group job result (descriptors DAT_fff90634+0x88*i from 0x0008FE98): on
fn-ptr completion copies the sample words from DAT_fff9065e+0x44*i into the output
buffer, ORs the per-channel status bytes DAT_fff91119/21[0x1C*ch] into the words by the
2-bit lane codes (table DAT_fff90654), clears the count word and re-arms via
PTR_FUN_000929fc. Caller 0x00090844 (AN-mux sequencer). [I]

#### 0x00092A04 `solcmd_build` — logic
Builds one driver-chip command: PTR_FUN_00092a80(&DAT_fff91108+0x1C*i, buf, 0x77) — a
read/command request from the per-channel record; when the mode code DAT_fff9110c[i] is
−0x43 sets bit 7 of DAT_fff9096c[i]. Caller 0x00092A48. [I]

#### 0x00092A48 `solcmd_build5` — dispatch
Runs 0x00092A04 over the five channels. Caller 0x000904FA (sol_feedback_task). [O]

#### 0x00092A60 `data_8852c_read` — data-accessor
Returns the 11-bit word (x & 0x7FF) from the interleaved matrices DAT_fff8852c /
DAT_fff88540 by index parity. Caller 0x000901A0. [O]

#### 0x00092A84 `errbits_bank_init5` — logic
Error-bit-bank initializer for the DAT_fff88414 bank (2 dwords, clears DAT_fff88422,
publish) — same shape as 0x00091086/0x000921A0. Caller 0x00091E12 (outside unit). [O]

#### 0x00092AC0 `shifter_resp_4w` — logic
Serial-response handler for the shifter/auxiliary link: 4 words — odd pair validated
(x & 0xFE00) == 0xAA00 → stores DAT_fff90978 (the <0x10 qualifier checked by 0x0008F4FE)
and DAT_fff90977, OK bit 0 in DAT_fff90973; even pair (x & 0xF900) == 0xA800 → stores
DAT_fff907d0 and the mode code DAT_fff907cf (the −0x44 marker), OK bit 0 in
DAT_fff907c8; failures set DAT_fff90974/fff907c9, error counter DAT_fff94218[2*id]. [I]

#### 0x00092B82 `errbits_bank_init6` — logic
Copies a 5-word template into DAT_fff885cc, clears DAT_fff885de, publishes. Caller
0x00091DAA (serial_rx_arm job chain). [O]

#### 0x00092BBE `shifter_resp_5w` — logic
Serial-response handler (5 words): odd 0xAA00 pair → lever/enable bytes DAT_fff90979 and
DAT_fff9097a with derived gate word DAT_fff90958 = (DAT_fff90979 & 0xF) ^ 0xF, bit 4 in
DAT_fff90973; even 0xA800 pair → DAT_fff907d2/DAT_fff907d3 (direction source for
0x0008DB5A) plus word 4 → lever state DAT_fff907d1, DAT_fff90959, DAT_fff90956; when
(DAT_fff907d1 & 0xE0) != 0 calls 0x00092F7C. [I]

#### 0x00092CC8 `shifter_resp_1w` — logic
One-word serial handler: 0xAA00 pattern → refreshes DAT_fff90979 and the derived gate
word DAT_fff90958, bit 8 in DAT_fff90973; failure → DAT_fff90974 bit 8, error counter. [I]

#### 0x00092D38 `shifter_echo_template_init` — logic
Initializes the echo-compare template bank at DAT_fff88424 (2 dwords, clears
DAT_fff88432) and ORs the live output state DAT_fff907d4 and DAT_fff9097b into it, then
publishes — arms the echo check used by 0x00092D94. Caller 0x00091DAA. [O]

#### 0x00092D94 `shifter_resp_echo` — logic
Serial echo handler: odd 0xAA00 pair plus echo byte compare against the template
DAT_fff88426 → OK bit 2 in DAT_fff90973 / mismatch DAT_fff90975; even 0xA800 pair →
stores DAT_fff907d5 and compares (x ^ DAT_fff88424) & 0xDF → OK bit 2 in DAT_fff907c8 /
mismatch DAT_fff907ca. Closes the loop with 0x00092D38's template. [I]

#### 0x00092E80 `shifter_echo_tmpl_init2` — logic
Echo-template bank initializer, twin of 0x00092D38 for the bank at DAT_fff88434: gated by
0x00093B8E, copies one template dword from ROM (PTR_DAT_00092f78), clears the halfword at
DAT_fff88438+2, merges switch status DAT_fff907d6 OR ~lever bits (DAT_fff907d1 & 0xE0) into
byte 0, then publishes via 0x000937F8. Caller 0x00091804 (switch_task_a); consumed by the
echo check 0x00092ED4. [O]

#### 0x00092ED4 `shifter_resp_echo2` — logic
Serial echo handler, bit-0x10 variant of 0x00092D94: verifies two payload words are 0xA800
frames, stores DAT_fff907d7, compares word 1 against the template DAT_fff88434 — match sets
OK bit 0x10 in DAT_fff907c8, mismatch sets bit 0x10 in DAT_fff907ca, pattern failure sets
bit 0x10 in DAT_fff907c9 and bumps the per-id error counter DAT_fff94218[id]; frame is then
consumed. Reached via handler table (callers=-). [O]

#### 0x00092F7C `shifter_echo_tmpl_init3` — logic
Third echo-template bank initializer, identical pattern to 0x00092E80 for the bank at
DAT_fff8843c (template PTR_DAT_00093180, merge of DAT_fff907d6 and ~DAT_fff907d1&0xE0,
publish via 0x000937F8). Callers 0x00091810 (switch_task_a__2) and 0x00092BBE
(shifter_resp_5w, on lever-state high bits); consumed by 0x00092FD0. [O]

#### 0x00092FD0 `shifter_resp_echo3` — logic
Echo handler, bit-0x20 variant of 0x00092ED4: two 0xA800 words, DAT_fff907d7 store, compare
against template DAT_fff8843c → OK/mismatch/fail bits 0x20 in DAT_fff907c8/ca/c9 and error
counter DAT_fff94218[id]. Handler-table reached (callers=-). [O]

#### 0x0009305A `bank_tmpl_init_88444` — logic
Bank template initializer without live merge: gate via 0x00093B8E, copy one template dword
from ROM into DAT_fff88444, clear the halfword at DAT_fff88448+2, publish via 0x000937F8 —
unlike 0x00092E80/0x00092F7C it does NOT OR in switch/lever state. Caller 0x00091ACA
(serial_rx_process). [O]

#### 0x00093096 `shifter_resp_pos_2w` — logic
Two-word 0xA800 handler on the shifter/even bank: on valid pattern sets DAT_fff907ce =
(word0 & 0x40) != 0 and builds the 10-bit position value ((w0 & 0xF) << 6) | (w1 & 0x3F)
into DAT_fff907cc with OK bit 0x40 in DAT_fff907c8; pattern failure stores 2 in
DAT_fff907ce and fail bit 0x40 in DAT_fff907c9. Finishes through the shared frame helper
0x00093B68 (retry counter DAT_fff94218[id] or clear). Handler-table reached. [I]

#### 0x00093126 `serial_status_summarize` — logic
Consolidates the two serial-status triples using 0x000931F8: (DAT_fff90973/74/75) → summary
byte DAT_fff90971 and (DAT_fff907c8/c9/ca) → DAT_fff90972, then ORs 0x80 into each when the
sentinel markers DAT_fff90977 == 0xBB / DAT_fff907cf == 0xBC are present. Caller 0x00091DAA
(serial_rx_arm); produces the aggregate health bytes for both handler banks. [O]

#### 0x00093198 `bank_tmpl_init_885e4` — logic
Smallest bank initializer of the family: gate via 0x00093B8E, copy one ROM u16 template
(PTR_DAT_000931ec) into DAT_fff885e4, clear DAT_fff885e6, publish via 0x000937F8. Caller
0x00091ACA (serial_rx_process); part of the DAT_fff88xxx status-bank publish chain. [O]

#### 0x000931D4 `serial_resp_store_908ee` — logic
Minimal one-word serial-response handler: stores the first payload word into DAT_fff908ee
then calls the common finish callback PTR_FUN_000931f4. Handler-table reached (callers=-). [O]

#### 0x000931F8 `serial_status3_consume` — library
Reduces an (ok, fail, mismatch) byte triple into a summary code: fail≠0 → bit 1,
mismatch-without-fail → bit 2, ok-only → bit 0; if the requested mask is fully covered by
the union the triple is cleared (consumed), otherwise the prior summary & 7 is merged.
Shared by 0x00093126 (bank summaries) and 0x00092A04 (solcmd_build). [O]

#### 0x0009336C `svc_tbl_chain_walk` — dispatch
Walks the 0x14-byte entry table at *DAT_000934a0 from index+1 up to 0x65, chaining entries
whose state byte DAT_fff94437[i] equals DAT_fff94436[base] and whose *(entry+4) word matches
*param_2; stops at a state byte ≥ 0x71, and finally invokes the terminal entry's callback
(entry+0x10, gated by entry+0xF) with (idx, args). Default result 0x14. Called only by
0x000933E0. [I]

#### 0x000933E0 `svc_tbl_query_dispatch` — dispatch
Table dispatch: id → u16 entry index via DAT_fff94220[id], entry = *DAT_000934a0 + idx*0x14;
entry type (+0xE) 2 delegates to the chain walk 0x0009336C, otherwise invokes the entry's
callback (enabled byte +0xF, fn-ptr +0x10) with (idx, arg), returning 0x14 when unmatched.
Callers 0x0009546C/0x000954E6/0x0009567E/0x00095714/0x000958B4/0x0009592E — the 0x954xx+
block outside this unit; possible relation to the spec 15 service table (unverified). [I]

#### 0x00093426 `svc_tbl_collect_dispatch` — dispatch
Query/collect variant over the same table: entry type 4 or 5 calls accessor fn-ptrs
PTR_FUN_000934a4 / PTR_FUN_000934a8 with the entry's sub byte (+2); otherwise sets bit 0 of
the per-entry flags DAT_fff9449c[idx] and invokes the entry callback with three stack
out-buffers (result collect). Callers 0x000955E2, 0x00095A4C, 0x00095814 (outside unit). [I]

#### 0x000934AC `svc_tbl_request_dispatch` — dispatch
Request (set/write-style) API over the second entry table *DAT_000935f4: resolves value
(param or *(entry+4)) and sub-code (param or entry+0xD), brackets the call with the
PTR_FUN_000935f8(0x50)/PTR_FUN_00093608 pair (interrupt-mask save/restore), clears bit 0 of
flags DAT_fff9449c[id], then runs a state ladder on DAT_fff94436[id]: <0x60 with type 5 →
PTR_FUN_000935fc, <0x60 otherwise (flags clear) → PTR_FUN_00093600, 0x60–0x6F →
PTR_FUN_00093604, ≥0x70 → no-op. Callers live in r04_boot: 0x000677F8, 0x0006CF44,
0x00074C20, 0x00078368, 0x0007873C. [I]

#### 0x0009355A `svc_tbl_auto_dispatch` — dispatch
Twin of 0x000934AC that takes value and sub-code from the entry itself (**(entry+4) and
entry+0xD) instead of caller arguments; same 0x50-bracket, flags clear, and state ladder on
DAT_fff94436[id] with the PTR_FUN_000935fc/00093600/00093604 handlers. Caller 0x00077F0A
(r04_boot). [I]

#### 0x0009360C `meas_avg_finalize` — logic
Finalizes one measurement accumulator: (accum * weight) / count + float const DAT_00093654,
clamped to 0xFFFF, stored as s16 at *param_1[2]; accumulator and count are reset and the
phase byte (+6) steps 1→2 — one stage of the 4-phase duty in 0x0008F70C (meas_avg_task,
spec 10 measurement path). [O]

#### 0x00093658 `pwm_cap_avg_pair` — library
Computes two float-weighted averages from the (accum, count) pairs at record +0/+8 and
+4/+10 into outs *param_1[2] / *param_1[3], each floored to 1 and clamped to 0xFFFF; the
primary output falls back to the secondary when it computes to zero. Used by the PWM input
capture job 0x0009140A and its reset wrapper 0x000936C8 (period/duty averaging). [I]

#### 0x000936C8 `pwm_cap_avg_pair_reset` — logic
Calls 0x00093658 to publish the averaged pair, then zeroes both accumulator/count pairs
(+0/+4 accumulators, +8/+10 counts) so the next capture window starts clean. Caller
0x0009140A (pwm_capture_enable_job). [O]

#### 0x000936DC `meas_state_seed_reset` — logic
Resets a small measurement state record: clears the dword at +0, bytes at +4 and +6, and
seeds byte +5 with min(config limit byte at param_1+5, *(byte *)param_1[4]) — a
clamp-to-live-value reseed. Caller 0x0008F9EC (meas_task_init). [I]

#### 0x000936FC `meas_avg_finalize8` — logic
Byte-count variant of 0x0009360C: (weight * accum) / count8 + float const DAT_000937c8,
clamped to 0xFFFF into the s16 at *param_1[2]; only fires when count ≠ 0, keeps the
accumulators, and steps the phase byte (+7) 1→2. Caller 0x0008F62C
(meas_slew_publish). [O]

## Unit summary
220 clusters documented (225 names; 3 multi-member clusters: 0x0008EF56 ×3, 0x0008FB5C ×2,
0x00091804 ×2, plus one out-of-cluster helper 0x0008EEB4 recorded by an earlier pass).
Counts by kind (per names row): logic 134, stub 25, data-accessor 24, dispatch 20,
library 15, init 5, unknown 2.

Unknown entries: 0x00090AEC `unknown_dev_init_90aec` (init job: verify 0xFFFF1000 state,
variant, fn-ptr chain), 0x00090B98 `unknown_service_90b98` (service job: id check, timer
0xFFFE1078 + callback).

Cross-region leads:
- svc_tbl_* dispatch family (0x0009336C–0x0009355A) sits on ROM entry tables *DAT_000934a0
  and *DAT_000935f4 (0x14-byte entries) with RAM index map DAT_fff94220, state bytes
  DAT_fff94436, flags DAT_fff9449c. Its callers are in r04_boot (0x000677F8, 0x0006CF44,
  0x00074C20, 0x00077F0A, 0x00078368, 0x0007873C) and in the r08_app 0x954xx–0x95Axx block
  (outside this unit). Those units should cross-check against these tables; possible but
  unverified relation to the spec 15 diagnostic service table.
- The status-bank publish pair 0x000937F8/0x00093B8E (used by 14+ publishers in this unit)
  and the frame-finish helper 0x00093B68/0x00093B8E resolve into leaf helpers 0x0009665C,
  0x00096674, 0x0006865C, 0x0006F5C0 — the low-level publish primitives live outside this
  unit's range and in r04_boot.
- Echo-template banks DAT_fff88424/34/3c/44 and DAT_fff885e4 pair with the 0xA800-frame
  handlers documented here; the dedicated shifter bus is spec open-question 15 (TCM pins
  3/4), so the shifter_resp_* / echo semantics stay candidates until that capture exists.
- meas_avg_finalize helpers (0x0009360C/0x000936FC) and pwm_cap_avg_pair (0x00093658/
  0x000936C8) feed the spec 10 measurement path and the PWM-input capture lanes
  (0x000901A0 / 0x0009140A); measurement-index owners should confirm which RAM slots the
  s16 outputs land in.
