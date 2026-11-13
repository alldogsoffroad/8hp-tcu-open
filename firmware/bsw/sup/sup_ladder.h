/* sup_ladder.h — L1..L3: the stock supervisor-chip self-test ladder, its
 * mode sequencer, the seq2 output FSM with the '3' switch-on diagnosis, the
 * TIOC pin-cut driver and the two input publishers, ported 1:1 from the
 * stock bytes, staged by compile-time holds (sup_ladder.c;
 * ~/tcu_sweep/l1_port.md, ~/tcu_sweep/l3_port.md).
 *
 * Stock 5 ms mode-0 list (flash 0x44CCC.., entries in this order):
 *   0x7C7E4 countdown · 0x7C884 · 0x8EDEA (0x8F62C finisher + 0x9181C) ·
 *   0x8EE02 · 0x8EE52 reply proc · 0x7CB6E · 0x7CB74 (0x8332C publishers) ·
 *   0x7AB48 · … · 0x7CC76 (0x8570E ladder) · 0x7CBA8 · 0x7CC56 (0x85828
 *   seq2) · 0x7CC1E · 0x7CC32 (0x85788 pin cut) · 0x8EDF6 J2 stage ·
 *   0x8EE30 F stage · 0x8EE82 batch
 * sup_batch_5ms calls sup_ladder_tick_head() first and sup_ladder_tick()
 * between the reply proc and the J2 stage, so a request the ladder writes
 * reaches the J2 word in the same 5 ms tick, as in the stock.
 */
#ifndef TCU_SUP_LADDER_H
#define TCU_SUP_LADDER_H

#include <stdint.h>
#include <stdbool.h>

/* ── THE L1 HOLDS (safety; compile-time, see sup_ladder.c) ──────────────── */
/* H1, phase 8's request: RELEASED on the target from r02s ("L2") — the
 * stock's one-frame 0x9020 shut-off test at level >= 5 (phase 8 'U' writes
 * 0x85E64(ch, 1); the output must stay off, class 0x22; the pass withdraws
 * it the next tick). r02r (H1 held) ran the ladder to the phase-0x40 hold
 * with pass mask 0x003F on the bench. The host keeps H1 held by default
 * (test_ladder: J2 9000 on every frame); test_ladder_l2 builds with it
 * released — the target's configuration, one 0x9020 frame. H2/H3 stay. */
#ifndef SUP_L1_HOLD_PHASE8_REQUEST
#if defined(__sh__)
#define SUP_L1_HOLD_PHASE8_REQUEST 0  /* L2: phase 8 'U' writes 0x85E64(ch, 1)      */
#else
#define SUP_L1_HOLD_PHASE8_REQUEST 1  /* host default: held (test_ladder)          */
#endif
#endif
/* H2, the phase-0x40 park: RELEASED on the target from L3a (2026-11-13) —
 * the ladder runs phases 0x40 (request 1: the first rail-on, class 0x33 /
 * 0x66 within 30 ms), 0x80 (target 5: the output must DROP with the request
 * still 1; then request 0, target 0) and 0x100 (class 0x22, pass mask 0xFF
 * → sub 0xAA). The host keeps H2 held by default (test_ladder: the L1 hold
 * regression); test_ladder_l3a / _l3b build with it released. */
#ifndef SUP_L1_HOLD_PHASE40
#if defined(__sh__)
#define SUP_L1_HOLD_PHASE40        0  /* L3a: phases 0x40 → 0x80 → 0x100 → 0xAA   */
#else
#define SUP_L1_HOLD_PHASE40        1  /* host default: parks before phase 0x40    */
#endif
#endif
/* H3, the 0x87766 calls: HELD on every default build (L3a). The sequencer's
 * 0x87766(0x23) after 0xAA and 0x87766(0x12 / 0x23) on retry exhaustion are
 * recorded in s_fsm_req_held and not made, so the walk FSM stays 8, the
 * sequencer waits in '.' forever (it needs FSM 'E') and the permanent
 * letter path ('L' → seq2 '3' → 'U') never starts. L3b = 0: the FSM goes
 * 0x23 → 'E', the sequencer '.' → 'L', seq2 0x33 → '3' diagnosis → 'U',
 * letter 'c', 0x9020 for good. On SH, L3b also needs SUP_L3B_ON_TARGET
 * (sup_ladder.c names what it acknowledges). */
/* r02t (2026-11-13): the TARGET default is L3a-E — H3 RELEASED, H4 held.
 * Validated against 0x873F0 (state 0x23 → 0x45 once 0x8D108(0) holds and
 * 0x6C48C(0x1838, 0xE7C7, 2) == 0): with H3 held the FSM would sit in 8 at
 * phase 0x80's target 0, a state the stock never holds. The host default
 * stays held (test_ladder / _l2 / _l3a regressions). */
#ifndef SUP_L1_HOLD_FAIL_FSM
#if defined(__sh__)
#define SUP_L1_HOLD_FAIL_FSM       0  /* L3a-E: 0x87766 calls made (FSM 0x23 → 'E') */
#else
#define SUP_L1_HOLD_FAIL_FSM       1
#endif
#endif
/* H4, the sequencer's '.' → 'L' step (L3, 2026-11-13): HELD by default. It
 * matters only with H3 released: then the FSM runs the stock's 0x23 → 'E'
 * walk (era 0x1838, the 0x8CEDC rule) but the sequencer stays in '.', so
 * seq2 never enters '3' / 'U' and the J2 word stays 0x9000 ("L3a-E": the
 * stock's post-ladder walk without the permanent letter). L3b = H3 and H4
 * both released. Why it exists: with H3 HELD (L3a as specified) the FSM
 * stays 8 at phase 0x80's target 0, so the 0x8D020 law keeps arming
 * context-0x1E tokens at walk level 0 in the 0x06 era — a wire state the
 * stock never holds (it leaves 0x23 for 'E' the pass level 0 is reached).
 * See ~/tcu_sweep/l3_port.md. */
#ifndef SUP_L3_HOLD_SEQ_L
#define SUP_L3_HOLD_SEQ_L          1
#endif

/* mode-0 init (stock 0x85678, row-9 init list entry 0x7CBDC): sequencer 0,
 * channel groups (0x8556E), every group → seq2 0x11, pin-cut mode 0x11,
 * retries 3, phase 1 / sub 0; plus the L1 stand-ins (0x94FCA[0] = 1). */
void sup_ladder_init(void);

/* the 5 ms tick head: countdown service 0x7C7E4 (the FIRST 5 ms entry)
 * and the 0x14 re-derive 0x9181C (entry 3's second half) */
void sup_ladder_tick_head(void);

/* the 5 ms tick body, in stock list order: publishers 0x8332C (0x8D8DE,
 * 0x89114), ladder 0x8570E → 0x85160 → 0x8B7A8, seq2 0x85828, pin cut
 * 0x85788. Runs before the J2/F stage. */
void sup_ladder_tick(void);

/* bench witness: the last 0x87766 target the sequencer requested (0 =
 * none) — recorded whether H3 holds the call (L3a) or makes it (L3b) */
uint8_t sup_ladder_fsm_req_held(void);

#endif /* TCU_SUP_LADDER_H */
