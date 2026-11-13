/* sup_mode.h — the stock OS operating mode: the mode-0 → mode-1 switch
 * after engagement, ported from the September image's bytes
 * (~/tcu_sweep/mode1_port.md).
 *
 * The stock's task table 0x43430 (10 rows × 6 modes) gives every OS task a
 * per-mode function list. Mode 0 is the boot / certification mode; the
 * switch to mode 1 follows the startup modules:
 *   seq2 (0x85828) module 7 = 0x0F, sequencer (0x85160) module 11 = 0x0F
 *   → module manager 0x7DAA4 (background, boot state 0): module 1 = 0x0F,
 *     0x93B8B = 1 → 0x6F144: 0x848E6 = 0 → 0x67A7C: state 0 → 1
 *   → 0x67AD4 state 1: the row-1 list once — its last entry 0x7DB70 sets
 *     module 2 = 0x0F → state 2
 *   → 5 ms list entry 0x6F0B4 (→ 0x784B4 → 0x6F0FC → 0x6BC44): module 2
 *     == 0x0F → pending 0x93B34 = 1
 *   → 0x66E06 at the END of the 10 ms task in mode 0 (0x67A42; in modes
 *     >= 1 at the end of the 5 ms task, 0x67A34): 0x848CF = 0x93B33 = 1,
 *     the mode-1 row-9 init list → state 2 → 3 (0x67AD4).
 * In mode 1 the batch 0x8EE82, the reply proc 0x8EE52, the F stage
 * 0x8EE30 and the pin cut 0x7CC32 run in the 10 ms list; the countdown,
 * 0x8EDEA, the publishers, the sequencer (0x85160 direct), seq2 and the J2
 * stage 0x8EDF6 stay at 5 ms (sup_batch.c). */
#ifndef TCU_SUP_MODE_H
#define TCU_SUP_MODE_H

#include <stdint.h>
#include <stdbool.h>
#include "bsw/sup/sup_ladder.h"

/* H5, the mode-1 apply (2026-11-13): HELD by default while H4 is held.
 * With H4 held (L3a / L3a-E) the NOMINAL path never reaches module 7 (seq2
 * stays 0x11), but every ladder FAILURE path does — retry exhaustion (seq2
 * 0x22) and the supply abort (seq2 0x16) resolve the channel, seq2 writes
 * module 7 = 0x0F, module 11 is 0x0F, and the stock goes to mode 1 there
 * too. Held, 0x66E06 records the request it would apply (sup_mode_held())
 * and leaves the mode 0: an L3a / L3a-E image keeps r02t / r02u's wire on
 * every path. L3b (H4 released) releases H5 with it; -DSUP_MODE_HOLD=0
 * gives the stock's behaviour on the failure paths of L3a / L3a-E too. */
#ifndef SUP_MODE_HOLD
#define SUP_MODE_HOLD SUP_L3_HOLD_SEQ_L
#endif

/* 0x66DA4's mode cells (0x848CF = 0x93B34 = 0x93B33 = 0) and the boot
 * state machine's start (state 0, 0x848E6 != 0); modules 1/2 = 0. Called
 * from sup_batch_init, before the mode-0 init list's entries. */
void sup_mode_init(void);

/* the background (stock: the OS background task 0x6A104, activated every
 * main-loop pass): 0x67A7C in boot state 0 (0x6F144 + the module manager
 * 0x7DAA4, modules 1 and 2 only) and 0x67AD4 (state 1: 0x7DB70 of the
 * row-1 list; state 2 → 3 when 0x848CF != 0). Call every superloop pass. */
void sup_mode_background(void);

/* 0x6F0B4 (5 ms list entry 14 in mode 0, 6 in mode 1): compute the mode
 * request (0x784B4 / 0x6F0FC / 0x6BC44 / 0x6BC60) into 0x93B34 */
void sup_mode_request(void);

/* 0x66E06: apply a pending mode (0x67A42 at the end of the 10 ms task in
 * mode 0; 0x67A34 at the end of the 5 ms task in modes >= 1). */
void sup_mode_apply(void);

/* the current mode 0x93B33 */
uint8_t sup_mode(void);

/* witnesses: the mode H5 refused to apply (0 = none); TCNTA at the
 * applied switch (0 = none). The 5 ms / 10 ms run counters are the stock
 * cells 0xFFF9355C / 0xFFF9353E (sup_cells.h). */
uint8_t sup_mode_held(void);
uint32_t sup_mode_switch_frt(void);

#endif /* TCU_SUP_MODE_H */
