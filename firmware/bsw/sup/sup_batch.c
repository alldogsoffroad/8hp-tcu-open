/* sup_batch.c — the 5 ms batch (design §7.5; stock 0x8EE82 pool order, 1:1).
 *
 * Stock call order (artifact2 §2.3):
 *   timing → 0x91C86 (compose 0x8E) → 0x91DAA (= 0x93126 C9 summarize +
 *   0x92B82 queue J6 + 0x92D38 arm J1) → 0x91E12 (J2/F staging, the
 *   0x91804/0x91810 law; the letters/selftest chain runs in the same pass,
 *   the stock's mode-sequencer 0x85160 slot) → 0x92A84 (queue J5) →
 *   0x90ACC (echo-derived condense) → 0x921A0 (bus-A kick, queues the
 *   0x40A98 ID job) → 0x9305A/0x93198 (J3/J4 cyclic starters) and J7 on
 *   every 2nd pass (design §12 V4: "every rotation" not fully decoded).
 *
 * Staging model (design §7.1): the stagers write the descriptor's tx_stage
 * directly and the engine sends tx_stage as-is — the constant frames are
 * re-staged here each pass exactly as the stock's template queuers do.
 *
 * The bus-A ID kick (drv_id_kick, stock 0x921A0) belongs to bsw/drv (P4);
 * a weak no-op keeps the link green until that module lands and its
 * strong definition replaces this one.
 */
#include "bsw/sup/sup_batch.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/sup/sup_ctrl.h"
#include "bsw/sup/sup_letters.h"
#include "bsw/sup/sup_walk.h"
#include "platform.h"

static uint16_t s_pass;              /* the 2-pass divider for J7 */
static uint32_t s_stamp;            /* the batch timing journal (FRT us) */

void drv_id_kick(void);              /* bsw/drv P4: queue the bus-A ID job */
__attribute__((weak)) void drv_id_kick(void)
{
    /* bus-A service not yet built (P4) — no-op until the strong symbol lands */
}

/* ── the constant-frame stagers ─────────────────────────────────────────── */

static void batch_stage_stat(void)
{
    uint16_t *st = sup_job_table[JOB_STAT].tx_stage;
    st[0] = 0x1800u;
    st[1] = 0x0C00u;
    st[2] = 0x1E00u;
    st[3] = 0x0600u;
    st[4] = 0x1400u;
}

static void batch_stage_ctrl_tail(void)
{
    /* frames 2/3: the 0x22/0x09 read commands (frames 0/1 are the
     * composers' — sup_ctrl_compose / sup_c9_compose) */
    uint16_t *st = sup_job_table[JOB_CTRL].tx_stage;
    st[2] = 0x2200u;
    st[3] = 0x0900u;
}

static void batch_stage_id(void)
{
    uint16_t *st = sup_job_table[JOB_ID].tx_stage;
    st[0] = 0x0600u;
    st[1] = 0x0300u;
    st[2] = 0x0000u;
    st[3] = 0x0000u;
}

static void batch_stage_pos(void)
{
    uint16_t *st = sup_job_table[JOB_POS].tx_stage;
    st[0] = 0x2800u;
    st[1] = 0x2E00u;
}

static void batch_stage_sslb2(void)
{
    sup_job_table[JOB_SSLB2].tx_stage[0] = 0x00FFu;
}

static void batch_stage_nib(void)
{
    sup_job_table[JOB_NIB].tx_stage[0] = 0x0000u;
}

/* ── init (stock 0x91DD2 serial_rx_init law) ────────────────────────────── */

void sup_batch_init(void)
{
    s_pass = 0u;
    s_stamp = 0u;
    sup_enable_request(false);          /* request 0, outputs-disabled 1 */
    CELL_SHUTDOWN_ST = 1u;              /* stock serial_rx_init 0x91DD2: shutdown-selftest
                                         * flag SET at init (C9 bit 4 stays clear; live
                                         * stock 0xFFF9094D = 01, first500_reads 2026-11-11) */
    CELL_SSLB2_RAW = 0x8000u;           /* the sentinel */
    /* the first bus-B job: J5 ID staged and queued */
    batch_stage_id();
    (void)sup_job_queue(JOB_ID);
}

/* ── the 5 ms batch (stock 0x8EE82 pool order, 1:1) ─────────────────────── */

void sup_batch_5ms(void)
{
    /* NO SILENCE WINDOW (r02c, decode 2026-11-11): the stock's batch runs
     * from the FIRST OS tick. 0x8EE82 is entry 26 of the 5 ms task's mode-0
     * list (row 7 col 0, list 0x42CD8; task object 0x41378 enabled in mode
     * 0 only; 0x66DA4 sets mode 0 at app entry). The OS tick (CMT, 2.5 ms,
     * 0x91A30(0x186A,0x19)) starts on the 2nd main-loop pass: 0x7DAA4
     * starts module 3 (0xFFF93B87, no deps in table 0x467D4) -> 0xFFF93B8C
     * = 1 -> 0x6F144 sets 0xFFF93B40 = 1 -> 0x67A7C starts the tick. The
     * state-3 gate 0x67B16 holds ONLY the background list (row 0: 0x6F144,
     * 0x7DAA4) — r02a's gate here was a misattribution, and the bench
     * answered it with a ~196 ms restart loop (app life ~106 ms, restart
     * within ~10 ms of the release; WOVF counter flat, no exception
     * frames). The MONITOR lead-in (sup_monitor_poll) is unchanged: its
     * start time is still [I]. */

    /* THE POOL ORDER, 1:1 for real (first500ms_diff §1 row 3; the transcript
     * tool caught the old order staging J6/J1 first with J3/J4 at the tail —
     * the reverse of the stock's wire sequence on every pass):
     *   1. reply proc 0x91ACA — queues J4 (0x93198), J3 (0x9305A)
     *   2. stage J2 0x91804 (→0x91758+0x92E80), stage F 0x91810
     *   3. compose 0x91C86 → queue J6 0x92B82 + arm J1 0x92D38
     *   4. 0x91E12 letters chain
     *   5. queue J5 0x92A84
     *   6. 0x90ACC condense, 7. kick 0x921A0
     * J7 rides the starter block (every 2nd pass; position [I] — the pool
     * order above lists seven jobs, J7 unplaced in the decode). */
    s_stamp = plat_frt_us();

    /* 1. the reply proc: condense the previous pass, then the starters */
    sup_reply_condense();
    batch_stage_sslb2();
    (void)sup_job_queue(JOB_SSLB2);
    batch_stage_pos();
    (void)sup_job_queue(JOB_POS);
    s_pass++;
    if ((s_pass & 1u) == 0u) {
        batch_stage_nib();
        (void)sup_job_queue(JOB_NIB);
    }

    /* 2. stage J2 + F (the enables pair; the letters chain folds in below
     *    per the 0x91E12 law — the stock runs it after the J1 arm) */
    sup_enables_stage();

    /* 3. the composers (0x91C86 + 0x93126): queue J6, arm J1 */
    sup_ctrl_compose();
    sup_c9_compose();
    batch_stage_stat();
    (void)sup_job_queue(JOB_STAT);
    batch_stage_ctrl_tail();
    (void)sup_job_queue(JOB_CTRL);

    /* 4. the letters chain (0x91E12: the selftest runs in the batch pass —
     *    the stock's mode-sequencer slot, AFTER the J1 arm) */
    sup_selftest_step();

    /* 5. queue JOB_ID (0x92A84) */
    batch_stage_id();
    (void)sup_job_queue(JOB_ID);

    /* 6/7. bus-A kick (0x921A0: queue the ID job + engine kick) */
    drv_id_kick();

    /* the builder law (0x91C86 tail): force the request cell while outputs
     * are not disabled — the composer does it with bit 0; the batch makes
     * the pass invariant explicit. */
    if (CELL_OUTPUTS_DIS != 1u) {
        CELL_REQUEST = 1u;
    }

    /* pump the engine: launch whatever's queued and the bus is free */
    sup_job_engine_pump();
}
