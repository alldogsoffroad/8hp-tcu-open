/* sup_batch.h — the 5 ms batch (design §7.5, stock 0x8EE82 pool order). */
#ifndef TCU_SUP_BATCH_H
#define TCU_SUP_BATCH_H

#include <stdint.h>
#include <stdbool.h>

/* the serial-rx init law (stock 0x91DD2): request cell 0, outputs-disabled
 * 1, SSLB2 sentinel 0x8000, and the first bus-B job (J5 ID) staged and
 * queued. Call once from the init order, before the first pass. */
void sup_batch_init(void);

/* the 5 ms batch — the stock's call order, 1:1:
 *   1. timing stamp (FRT sample for the batch journal)
 *   2. sup_ctrl_compose() + sup_c9_compose()
 *   3. queue JOB_STAT, queue JOB_CTRL
 *   4. sup_selftest_step() + sup_enables_stage()   (J2/F staging)
 *   5. queue JOB_ID
 *   6. sup_reply_condense()
 *   7. drv_id_kick()                               (bus-A ID job)
 *   8. JOB_POS / JOB_SSLB2 cyclic, JOB_NIB every 2nd pass
 *   then: builder-law request-cell force + sup_job_engine_pump()
 */
void sup_batch_5ms(void);

#endif /* TCU_SUP_BATCH_H */
