/* sup_ladder.h — L1: the stock supervisor-chip self-test ladder, its mode
 * sequencer, the seq2 output FSM, the TIOC pin-cut driver and the two
 * input publishers, ported 1:1 from the stock bytes — HELD short of any
 * rail command (sup_ladder.c, ~/tcu_sweep/l1_port.md).
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
#define SUP_L1_HOLD_PHASE40        1  /* 0x8B7A8 parks before phase 0x40 runs      */
#define SUP_L1_HOLD_FAIL_FSM       1  /* 0x85160 never calls 0x87766 (FSM stays 8) */

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

/* bench witness: the 0x87766 target the sequencer WOULD have requested
 * (0 = none) — held by SUP_L1_HOLD_FAIL_FSM */
uint8_t sup_ladder_fsm_req_held(void);

#endif /* TCU_SUP_LADDER_H */
