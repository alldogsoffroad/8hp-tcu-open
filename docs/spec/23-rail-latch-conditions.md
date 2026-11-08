# 23 — Rail latch condition set (top-down artifact 3)

> Every condition for supervisor output 0 to conduct; the real chain (self-test → letters → 0x90-enables) and the ranked missing-condition candidates. Generated 2026-11-08.

# ARTIFACT 3 — The full output-latch condition set (supervisor output 0 = solenoid power rail)

Bosch 1034420267, SH7254R. Question: every condition the STOCK firmware satisfies for the
supervisor's (SSLB1, RSPI-B) switch output 0 to CLOSE/CONDUCT, from exhaustive slices of
`~/tcu_sweep/decomp` (all greps are complete over the exported tree; pointer slots resolved
from the ELF `1034420267_sh7254r.elf` literal pools, method of reconciliation.md).

**Headline finding 1 — the two "known" conditions are a telltale, not the latch.** The
`0x0006C63E:35` gate `(DAT_fff93ba1 >= 6) && (DAT_fff90954 != 0)` writes only
`DAT_fff800e8` (the boot fault-record "certified output" byte, consumed solely by
`0x00087AAC fault_boot_record_read` → diag cells `0xFFF9384A/B`; cleared by `0x00087B08`)
plus diag bits (`0xFFF9384C |= 0x2000`, the two-pass counter `0xFFF93B9A` at level 7 →
`0xFFF94F84 |= 0x2000`). Nothing in the output path reads any of it. The grant cell
`0xFFF90954` has exactly three sites: writer `0x00091ACA:24` (`= 0x22-echo 0xFFF907D5 & 1`),
readers `0x0006C63E` (telltale) and `0x0007284C:9` (boot-handover state 3→4 advance when
the grant DROPS). Neither known condition gates the rail.

**Headline finding 2 — the rail's real producer chain is the LETTERS → 0x90-enables path,
and its classifier is the POWER-ON SELF-TEST, which hard-fails on the exact status bit our
bench lacks (0x14 bit 1).**

```
selftest_seq_dispatch 0x0008B7A8 (from mode sequencer 0x00085160)
  → phase handlers 0x0008B974/8BB34/8BB8A/8BC54/8BD40/8BDD6/8BEC0/8BFB8/8C0F2/8C194
  → seq_group_code_send 0x00085600 (codes 0x11/0x16/0x22/0x33, calib channel-enable bits)
  → seq2 command state 0xFFF94FD0[ch] (seq2_state_set 0x00085E00)
  → seq2_chan_fsm 0x00085828 (2 ch; speed-plaus class 0x0008C6D4 → 0xFFF93A92[ch];
     chip fault bools 0xFFF90947[ch] from 0x14 bits 5/6/7; hold counters 0xFFF947AC/B0/B2)
  → mon2_chan_enable 0x0008C7A8(ch, enable): 0xFFF93A90[ch] = enable;
     letters[ch] = 0xFFF90944[ch] = 'c'(99) if enable else 0
  → switch_status_build 0x00091758: enables byte 0xFFF907D6 =
       (0xFFF90963 & 0x1D [ROM 0x1918EF = 0x00 → base 0])
     | 0x20 if letters[tbl[0]]=='c' | 0x40 if letters[tbl[1]]=='c' | 0x80 if letters[tbl[2]]=='c'
       (tbl @0x1918E8 = {0,1,2}; | (0x1918EB=0)<<5)
     & ~(0x60) if 0xFFF90969 != 0        ← one-way clear latch, see C2
     | 0x08 if 0xFFF90952 == 1           ← cell never written in the image → bit3 always 0
  → staging 0x00092E80 (job 0x40C0C, every scheduler batch 0x0008EDF6 → 0x00091804; gate =
     job-idle 0x00093B8E only — NOT letter-change): TX word 0xFFF88434 =
       template(0x40C08) | (0xFFF907D6 & ~(0xFFF907D1 & 0xE0))
  → job 0x40C0C = {90|enables, 24} both frames to SSLB1 → completion 0x00092ED4:
     0x24 reply data → 0xFFF907D7 (write-only shadow, NO consumer); compares only bits
     0/2/4 (& 0x15) of reply vs staged word → OK bit 0xFFF907C8 |= 0x10 (bit5 not checked)
  → the CHIP latches 0x90 bit5 internally → 0x24 answers 0xAA20 (stock) vs 0xAA00 (us)
```
Twin job 0x40C30 (stager 0x00092F7C, handler 0x00092FD0, same content) runs from batch
`0x0008EE30 → 0x00091810` AND re-fires from `0x00092BBE:56` whenever the 0x14 status
reports any output faulted (`(0xFFF907D1 & 0xE0) != 0`).

---

## 1. THE CONDITION TABLE

Stock-side conditions for rail conduction. "Bench" = our E60l steady state (spec 03 §6.2be:
grant yes, level 7, rail primed (0x22 echo bit5 during the window), 0x24 AA00, 0x28 AA45,
0x2E AA3A/3C, 0x14 AAD5).

### A. Wire conditions to SSLB1 (the actual latch producers)

| # | Condition | Code citation | Consumer | Our bench | Verdict |
|---|---|---|---|---|---|
| A1 | **0x90 register bit5 written in cyclic job 0x40C0C** — requires letters[0]='c' | 0x00091758:8-10 (letters→bit5/6/7, tbl 0x1918E8={0,1,2}) | chip 0x90 reg; readback 0x24 | we hard-code 0x20 and DO transmit 0x9020 | PARTIAL (byte present, provenance absent) |
| A1a | **letters[0]='c': 0xFFF93A90[0] set by the seq2/selftest classifier** — needs: calibration channel-enable bit (bank byte @PTR_PTR_00085AA0, bit0), command ladder 0x11/0x16 (uVar11=0xFFF939B6[ch], written 0x00085E64 by selftest phases 1/4/8) or '3'/'U' ladder with previous-state ≠ 0 | 0x00085828 (542-instr FSM); enable call PTR_FUN_00085C94→0x0008C7A8(ch, uVar11≠0) | letters cell 0xFFF90944[0] | not present (no classifier in our firmware) | N/A for wire; see C4 |
| A1b | **self-test phase 2 passes: 0x14 bit1 SET** (`DAT_fff90959 != 0`), else error latch 0xFFF93A8C and restart | 0x0008BB34:13 `if (DAT_fff90959 == 0) {DAT_fff93a8c |= DAT_fff94fe2; substate 0x55}`; 0x14→0xFFF90959 @0x00092BBE:48 | selftest_seq → letters | **our 0x14=AAD5 has bit1 CLEAR (stock C7 has it SET)** | **FAIL — the discriminator** |
| A1c | chip reports output-0 unfaulted: 0x14 bit5 clear (else bit5 stripped from TX) | 0x00092E80:29 mask `~(0xFFF907D1 & 0xE0)`; fault bool 0xFFF90947[0] @0x0009181C:11 | staged 0x90 word; seq2 'U' class @0x00085828:171/213 | 0xD5 bit5 clear | PASS |
| A1d | scheduler batch running (cyclic; gate = job-idle only) | 0x0008EDF6→0x00091804→0x00092E80; idle check 0x00093B8E | job queue 0x000937F8 | our cyclic runs | PASS |
| A2 | **0x8E control word bit0 (output-enable master) = 0xFFF907D4 bit0 ← 0xFFF90953**, set by the one-shot `0x000873F0:44 → 0x0006C612(1)` (requires a harvested level < 7 at a monitor pass AND 0x68BE0()==0 i.e. 0xFFFF1001 bit6 clear); held unless handover-disable 0x00072898→0x0006C612(0) | 0x00091C86:18-31 (`d4|=1` / `d4&=0xFE`; echo-gated WDT switch 0x000920A4) | job 0x40BE8 frame 0 (`8E\|d4`) | our self-ask (own code) obtains bit0/grant | PASS (equiv) |
| A3 | **chip latches the certification** (0x24 data 0x20) | — chip-internal; firmware only reads it (0x00092ED4→0xFFF907D7, no consumer) | — | 0x24 = AA00 | **FAIL (the symptom)** |
| A4 | walk completes: 84-value chain 0x3F (init 0x0006C12C) → 0x06 (state 0, 0x000873F0) → 0x1F (state 0x23), cyclic job 0x40A14 {84\|v, 0A, 12, 0C}, per-pass 0x0A-echo + (0x12&5)==phase-mask checks | 0x0006C48C/0x0006C1EA:66-103; staged value 0xFFF9054E (writer=reader=0x00091190 only) | level cell 0xFFF93BA1; commit status 0xFFF90551 | our E60l sends the stock handshake; E60n's 0x06-era attempt reset-looped | PARTIAL / see cand. 2 |

### B. The two "known" conditions — necessary only for telltales, not the rail

| # | Condition | Code citation | Consumer | Our bench | Verdict |
|---|---|---|---|---|---|
| B1 | level ≥ 6 (0xFFF93BA1 from 0x0C nibble `0xFFF90550>>4&7`, harvester 0x0006C1EA:62) | 0x0006C63E:35 | DAT_fff800e8 telltale only (readers 0x00087AAC/0x00087B08); walk/FSM sequencing elsewhere | level 7 | PASS (and shown non-load-bearing for the rail) |
| B2 | grant: 0xFFF90954 = 0x22-echo bit0 (0xFFF907D5 & 1, 0x00091ACA:24) | 0x0006C63E:35; 0x0007284C:9; 0x00091C86:29 | telltale; boot-handover; echo-gated watchdog switch | grant yes | PASS (non-load-bearing for the rail) |

### C. Inhibitors (conditions that must NOT hold)

| # | Condition | Code citation | Effect | Our bench | Verdict |
|---|---|---|---|---|---|
| C1 | mode sequencer in terminal path: system-mode cell 0xFFF9500C == 'L' (entered from mode '.' when FSM 0xFFF95010==0x45 AND 0x14-bit1 — 0x00085160:126-131), or mode 0 with countdown 0xFFF9414D expired (0x00085160:76-86) | both call `PTR_FUN_000853E8(1)` → 0x00092094 → **0xFFF90969 := 1, never cleared in the image (one-way until reboot)** | 0x00091758:17-19 strips bits 5 AND 6 (mask 0x1918EC=0x03<<5) from the enables byte | n/a (our firmware has no mode sequencer); stock: mode 'L' entry needs 0x14-bit1 — see A1b | noted |
| C2 | 0xFFF90952 == 1 would set bit3 of the enables byte (never written anywhere → always 0; stock steady 0x9020 consistent) | 0x00091758:22 | cosmetic | — | PASS |
| C3 | 0x14 bits 6/7 (outputs 1/2 faulted) do NOT strip bit5 (mask only touches the faulted bits) | 0x00092E80:29 | — | bits 6/7 set both stock and us | PASS |

### D. Output path (solenoid demand) — cross-check result: NO supervisor gate exists there

Exhaustive readers of 0xFFF90954 / 0xFFF800e8 / 0xFFF90944 / 0xFFF907D6: none in the
actuator chain. Spec 12 §2 chain (objects 0xFFF8B154–0xFFF8B25C → 0xD9F70 → 0xFFF84B78 →
0xA3646 → 0xBC0F4 → 0x7B6D8 → 0x83DB0 (mode 0xFFF94FD8: 1=off, 2/3=current, 4=full-on) →
0x89540 → 0xFFF90230/0xFFF90812 → 0x901A0 (mode letters 0x21/0x43/0x32 at 0xFFF9081C) →
0x924B0 (45-frame cyclic 0x40B48, gate = job-idle only)) never consults the supervisor.
The drivers cannot conduct without the rail, but the firmware drives them regardless.
The on/off actuator slots 10–14 (0xFFF94FAA, switch-output manager 0x849D4, calib
0x1919B4=0x0B) go to MCU timer outputs TOE30–TOE22, not to the rail.

### E. Symptom bytes — producers and whether the firmware uses them

| Byte | Producer (stock) | Firmware consumers | Verdict |
|---|---|---|---|
| 0x24 data | 0x00092ED4/0x00092FD0 → 0xFFF907D7 | **none** (write-only shadow; compare uses only bits 0/2/4 of the word) | pure symptom of chip state |
| 0x14 data | 0x00092BBE:49 → 0xFFF907D1 | 0x0009181C (bits 5/6/7 → fault bools 0xFFF90947[tbl]; bit1 → 0xFFF90959); 0x00091ACA:23 (bit0 → 0xFFF84A46); 0x00092BBE:56 (fault re-trigger); 0x00092E80/0x00092F7C (TX mask) | **0x14 bit1 is the load-bearing one** (selftest phase 2 + mode-'L' entry + speed interlock at 0x00085160:23,131); bit0 feeds boot-handover readiness 0x0007291A:12 |
| 0x28/0x2E | 0x00093096 → 10-bit value 0xFFF907CC + flag 0xFFF907CE | none (0xFFF907CC unread; 0xFFF907CE → 0xFFF9095D diag) | pure telemetry (matches spec 03 6.2n) |
| 0x22 echo | 0x00092D94 → 0xFFF907D5; compare vs staged 8E word masks bit5 (0xDF) | 0xFFF90954 (grant), 0x91C86 bit0/bit2 branches | bit5 (rail-up) deliberately not checked by the echo compare |
| 0x12 commit | 0x000911D8 → 0xFFF90551; checked `(0x12&5)==phase-mask` in 0x0006C1EA:66-78 (mismatch → walk restart bits 0xFFF93BA7) | walk progress | hand-fed 0x1200 → AAC2 (bit1) on our bench; stock 0x14 mirrors it as bit1 |

---

## 2. RANKED MISSING-CONDITION CANDIDATES

**#1 — the chip's 0x14 bit1 ("commit accepted / lock" status) and the self-test gate it feeds.**
Our 0x14 = AAD5 lacks bit1; the stock's C7 has it. Statically this bit is the single
hardest firmware-side gate on the certification producer chain: with bit1 clear the stock's
own `selftest_phase2` (0x0008BB34:13) latches the error state and restarts forever —
letters[0] could never become 'c', the 0x90 word would never carry bit5, and the stock
itself would sit at 0x24=AA00 exactly like us. The stock gets bit1 from the chip (our
hand-fed commit 0x1200 produces AAC2 on the 0x12 read — bit1 in the commit-status — but our
0x14 still reads D5: the chip answers the commit yet does not enter the state whose status
bit1 reports). Evidence: 0x0008BB34:13; 0x00092BBE:48-49; 0x00085160:23,126-131; spec 6.2az
(AAC2 note). The missing condition is therefore upstream of bit1: whatever makes the chip
actually accept/complete commissioning.

**#2 — the 0x06-era walk (value 0x3F→0x06→0x1F with per-pass 0x0A-echo and (0x12&5)
commit-mask checks).** E60n proved our replica cannot yet run the 0x06 era (reset loop,
the E16 class); E60l runs only the handshake + steady 0x1F-class conversation. The stock
sends the level job cyclically with the commit word inside the job ({84|v, 0A, 12, 0C}) and
restarts the level on any echo/mask mismatch (0x0006C1EA). A chip that never traversed the
0x06 era plausibly refuses both the certification latch (0x24 stays AA00) and the committed
status (0x14 bit1 stays clear). Evidence: 0x0006C1EA:66-103; 0x0006C48C; spec 6.2bd/be;
the E60n regression.

**#3 — the cyclic job CONTEXT of the 0x90 write.** In the stock the enables job is never
sent alone: every batch round runs the full SSLB1 job set (0x40BA0 status incl. the 0x14
harvest, 0x40BE8 8E/C9/22/09 control+echo, 0x40C0C/0x40C30 enables, 0x40C54 28/2E) as one
zero-gap DMA-fed stream (bus-B five-select, DMA tables 0xFFF90630-class; spec 6.2ah/6.2ap).
Our writes are single jobs / hand-fed probes with inter-frame gaps. If the chip's register
latch (or its 0x14-status state machine) requires the write to arrive inside the intact
cyclic sequence (or within the primed window — our 0x22 echo bit5 drops at the window
close), the same bytes will not stick. Evidence: job table spec 03 §4.3; 6.2ah surviving
wire-timing hypothesis; 6.2ay primed-window law.

**#4 — the letters machinery itself (stock-internal, currently bypassed by our hard-coded
0x20).** If #1/#2 are fixed and we ever mirror the stock composer, its own conditions
become binding: calibration channel-enable bits (bank byte at PTR_PTR_00085AA0; decompiler
ambiguity on the channel-0 mask constant — see §3), the selftest phase run (speed
0xFFF94F96 inside the calibration window, calibration byte ≠ 0xA5, mode sequencer not in a
terminal mode), the seq2 'U' classification (previous-state ≠ 0, hold counters, chip fault
bit clear), and the one-way clear-latch 0xFFF90969 never having fired (mode 'L' / mode-0
terminal paths). None of these can fail on the bench while we hard-code 0x20 — but the
stock satisfies all of them within ~250 ms (spec 6.2aw "letters 0→0x21" at the leap).

**#5 — demoted: the two "known" conditions.** Level ≥ 6 and the grant are necessary only
for the boot-record telltale and bookkeeping (and the grant for the handover/watchdog);
they cannot be the missing latch because nothing downstream of them touches the wire.

---

## 3. What cannot be determined statically (chip-internal / unresolved)

1. **The SSLB1's internal latch law for 0x90 bit5** — what makes 0x24 answer 0x20 vs 0x00
   (granted-context? window? commit? walk history?) is inside the chip; the firmware only
   writes the register and reads the echo.
2. **What sets 0x14 bits 0–4 internally** (bit1 "committed/lock" inferred from the AAC2
   experiment; bit4 set for us, clear for stock — meaning open; bit0 "ready/output-on"
   set for both).
3. **The chip's window/lease law** (0x22-echo bit5 priming and its ~44-pass close) and any
   inter-frame-gap sensitivity of register writes.
4. **The exact 84-value ↔ level-nibble mapping and era-2 dynamics on a boot-warm chip**
   (only the 0x0C/0x0A/0x12 replies reveal it; planned 6.2be observation run).
5. **Decompiler ambiguity:** the channel-mask constant for seq2 channel 0 in 0x00085828:34-42
   renders as `uVar6 = 0` (which would dead-end the classified path); the raw SH-2A shows
   `mov #1,r9; shll r9; extu.b; tst` — a `1<<ch` mask is the only reading consistent with
   the stock's own steady 0x9020, so the calibration-bit-0 condition is real, but the
   disassembly was not fully machine-verified.
6. Whether the chip self-persists the certification across full power-off (bench chip
   powers up at 0x1F "engaged-ready", spec 6.2j — suggests yes while powered).

## 4. Verification breadcrumbs (for the parent's synthesis)

- Every 0xFFF90954 site: `grep -ri fff90954` → 3 sites (writer 0x00091ACA:24; readers
  0x0006C63E:35, 0x0007284C:9). Every 0xFFF9054E site: writer=reader=0x00091190 (staging
  only). 0xFFF907D7/0xFFF907CC: write-only.
- Pointer resolutions from the ELF pool: 0x92F68→0x00093B8E (idle-reap), 0x92F70→0x000937F8
  (submit), 0x92F74→0x40C0C, 0x93178→0x40C30, 0x9186C→0x1918E8 {00,01,02}, 0x91870→0x1918EB
  (0x00), 0x91878→0x1918EC (0x03), 0x91CF8/0x91E18→0x000920A4, 0x85C94/0x85E9C→0x0008C7A8,
  0x85AA4→0x0008C6D4, 0x85AAC/0x8C804→0x0008F50C, 0x85AB0→0x00085FC4, 0x85AB4→0x0007BBB4,
  0x85EA0→0x0008C3BC, 0x85EA4→0x0008C670, 0x853E8→0x00092094, 0x6C784→0x00072EB6,
  0x6C788→0x00068BE0, 0x92720→0x00093B8E, 0x9271C→0x40B48, 0x92724→0x40D0C.
- The enables byte is recomposed every batch and transmitted cyclically — the spec 6.2ax
  wording "queues on every letter change" is imprecise: staging is gated only by job-idle.
