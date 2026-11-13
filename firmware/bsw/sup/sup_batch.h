/* sup_batch.h — the 5 ms batch (design §7.5, stock 0x8EE82 pool order). */
#ifndef TCU_SUP_BATCH_H
#define TCU_SUP_BATCH_H

#include <stdint.h>
#include <stdbool.h>

/* the serial-rx init law (stock 0x91DD2): request cell 0, outputs-disabled
 * 1, SSLB2 sentinel 0x8000, and the first bus-B job (J5 ID) staged and
 * queued. Call once from the init order, before the first pass. */
void sup_batch_init(void);

/* the 5 ms batch — the stock's 5 ms task (and, on every second pass, its
 * 10 ms task, which shares the CMT tick and runs right after it), per OS
 * mode (sup_mode.c; ~/tcu_sweep/mode1_port.md):
 *   mode 0, every pass (5 ms list 0x42CD8, flash 0x44CCC..):
 *     countdown 0x7C7E4 + 0x9181C · reply proc (condense, J3, J4) ·
 *     publishers · mode request 0x6F0B4 · ladder 0x8570E · seq2 · pin cut ·
 *     J2 stage · F stage · batch (compose, summarize, J6, J1, J5, bus-A
 *     kick) · pump; on a 10 ms pass then 0x66E06 (the mode apply)
 *   mode 1, every pass (0x42F80): countdown + 0x9181C · publishers ·
 *     0x6F0B4 · sequencer 0x85160 · seq2 · J2 stage · pump · 0x66E06;
 *     on a 10 ms pass then (0x42F44): reply proc · pin cut · F stage ·
 *     batch · pump
 * The 10 ms pass is the first pass and every second one after (the stock's
 * 5 ms / 10 ms countdowns 0x9355A / 0x9353C both start at 0). */
void sup_batch_5ms(void);

#endif /* TCU_SUP_BATCH_H */
