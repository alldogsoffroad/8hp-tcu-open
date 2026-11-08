# E60p change list — our firmware vs the complete stock model (2026-11-08)

The diff of the current tree (E60n head) against top-down artifacts 1–5 (specs 20–24).
Every item cites its spec. Order = proposed implementation order. Bench-stable reference:
E60l (CRC A2CC9005). Aliases noted where the sweep's surface names and the artifacts'
behavioral names differ.

## A. The bus-B conversation (supersedes E60o's single-guess design)

1. **ADD the fault-path enables twin job (0x40C30)** — spec 22 §1: a job descriptor our
   replica has never implemented, auto-queued by the 0x14 completion handler (sweep alias
   `shifter_resp_5w` @ 0x92BBE) whenever 0x14 reports faulted switch outputs (bits 5–7).
   Our chip permanently reports bits 6/7 (unpopulated outputs 1/2) → the stock sends this
   job on our bench continuously; we never do. Highest-ranked NEW traffic.
2. **MOVE the walk onto the timer ISR** — spec 22 §1/§4, spec 20: the walk jobs are driven
   by vector 224 (handler at 0x9106C — not a Ghidra-defined function; reached via the
   vector table) every output-compare tick, NOT by the 5 ms task. Our walk rides the
   10 ms ladder with era floors ERA_06_FLOOR=100 / ERA_1F_FLOOR=200 (sup_engage.c:44-45)
   = 1 s/2 s — the stock's arithmetic is dwell = value×200+2400 timer ticks with the
   builder REJECTING any other cadence (spec 22 never-rule). E60n's reset cause.
3. **INTERLEAVE the two conversations** — spec 22 §1: 5 ms mirror + timer-tick walk share
   the one-in-flight queue (this is the "rolling preamble with live bytes between eras").
   Ours are separate burst conversations (main.c:541 plat_sup_burst w1/w2).
4. **ADD the 0x90-enables job as CYCLIC traffic** — spec 23: {90|enables,24} at 0x40C0C
   rides every batch (composer sweep-alias `switch_status_build` @ 0x91758). Ours is
   hand-fed one-shot via FE13.
5. **SPEAK the complement-token dialect** — spec 22 §4: while the chip's level == 0 the
   duty-era token context cycles 0→7→0x19 (sites `park_pwm_begin_decode` @ 0x6BF9C /
   0x8CEDC). We always send the plain token.
6. **FORCE-ON the enable request once armed** — spec 22 §3: the stock's bit-0 rides every
   5 ms pass after the first monitor pass (dialect machine @ 0x91C86); our E60i recipe
   self-asks once at pass ~21 (main.c:418).
7. **ADOPT the stock's echo masks** — spec 22 §2: 0x22 matched with 0xDF (the rail bit is
   the chip's own output — never asserted by the master), 0x24 with 0x15. Verify our
   checkers use identical masks.

## B. The output chain (E60q candidate — after A lands bit1)

8. **Mirror the latch chain instead of hard-coding** — spec 23: selftest_phase2 (0x8BB34,
   name-confirmed) gates on 0x14 bit1 → letters → cyclic 0x90. We hard-code certification
   0x20. Mirror minimally: gate our enables on 0x14 bit1; full letters machinery only if
   needed. NOTE: on the stock bench the chip already has bit1 (warm) — which is why the
   stock leaps at +250 ms mid-walk (spec 21). Our chip never sets bit1 — candidate #2's
   root cause is expected to be fixed by item 2 (the walk at stock cadence).
9. **OPTIONAL parity** — spec 24: one-shot WTSR.WOVF read before the first WDT feed
   (count watchdog resets); fatal-ring occurrence-counter semantics. No functional need.

## C. Ground truth still open (bench/observation)

- 0x14 bit1's producer is chip-internal; era-2 timing on a boot-warm chip (state-0x23
  carries a level==0 condition) — the stock observation run answers both (spec 20 §3).
- Chip-ID verdict pending (artifact 6, in flight) — a datasheet would document 0x14/0x24/0x90
  directly and may reshape item 8.

## Risk note (the fatigue law)

Items 1–7 are one coherent rewrite of the bus-B service layer — decode-backed this time,
not guesses, and the ladder stays intact (E60m's lesson). Still: build only fresh, flash
once, verdict before extending into item 8.
