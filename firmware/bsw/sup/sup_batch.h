/* sup_batch.h — the 5 ms batch (design §7.5, stock 0x8EE82 pool order). */
#ifndef TCU_SUP_BATCH_H
#define TCU_SUP_BATCH_H

#include <stdint.h>
#include <stdbool.h>

/* the serial-rx init law (stock 0x91DD2): request cell 0, outputs-disabled
 * 1, SSLB2 sentinel 0x8000, and the first bus-B job (J5 ID) staged and
 * queued. Call once from the init order, before the first pass. */
void sup_batch_init(void);

/* the 5 ms batch — the stock's 5 ms list order (flash 0x44CCC..), 1:1:
 *   0. sup_ladder_tick_head(): countdown 0x7C7E4 + 0x14 re-derive 0x9181C
 *   1. reply proc: sup_reply_condense(), queue JOB_POS then JOB_SSLB2
 *   2. sup_ladder_tick(): publishers 0x8332C, ladder 0x8570E, seq2 0x85828,
 *      pin cut 0x85788 (L1, held short of any request)
 *   3. sup_enables_stage()                         (J2/F staging)
 *   4. sup_ctrl_compose() + sup_c9_compose(), summarize, queue JOB_STAT,
 *      queue JOB_CTRL
 *   5. queue JOB_ID, drv_id_kick()                 (bus-A ID job)
 *   then: builder-law request-cell force + sup_job_engine_pump()
 */
void sup_batch_5ms(void);

#endif /* TCU_SUP_BATCH_H */
