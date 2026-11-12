/* sup_batch.c — the 5 ms batch (design §7.5; stock 0x8EE82 pool order, 1:1).
 *
 * Stock call order (artifact2 §2.3):
 *   timing → 0x91C86 (compose 0x8E) → 0x91DAA (= 0x93126 C9 summarize +
 *   0x92B82 queue J6 + 0x92D38 arm J1) → 0x91E12 (J2/F staging, the
 *   0x91804/0x91810 law; the letters/selftest chain runs in the same pass,
 *   the stock's mode-sequencer 0x85160 slot) → 0x92A84 (queue J5) →
 *   0x90ACC (echo-derived condense) → 0x921A0 (bus-A kick, queues the
 *   0x40A98 ID job) → 0x9305A/0x93198 (J3/J4 cyclic starters). J7 is
 *   never queued — the stock has no J7 queuer (see sup_batch_5ms).
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

static uint32_t s_stamp;            /* the batch timing journal (FRT us) */

void drv_id_kick(void);              /* bsw/drv P4: queue the bus-A ID job */
__attribute__((weak)) void drv_id_kick(void)
{
    /* bus-A service not yet built (P4) — no-op until the strong symbol lands */
}

/* ── the queuers (S7, baseline review): every stock queuer (0x92D38 J1,
 *    0x92B82 J6, 0x92A84 J5, 0x9305A J3, 0x93198 J4) calls check-free
 *    0x93B8E FIRST and returns if busy; only then copies the template, ORs
 *    in the data, CLEARS the last reply word and queues. Staging before the
 *    check could tear an in-flight burst (the ISR feeds tx[idx] live). ── */

static bool q_begin(uint8_t id)
{
    return sup_job_check_free(id);
}

static void q_commit(uint8_t id)
{
    sup_job_table[id].rx[sup_job_table[id].frames - 1u] = 0u;
    (void)sup_job_queue(id);
}

static void queue_stat(void)                    /* J6, 0x92B82 */
{
    if (!q_begin(JOB_STAT)) return;
    uint16_t *st = sup_job_table[JOB_STAT].tx_stage;
    st[0] = 0x1800u;
    st[1] = 0x0C00u;
    st[2] = 0x1E00u;
    st[3] = 0x0600u;
    st[4] = 0x1400u;
    q_commit(JOB_STAT);
}

static void queue_ctrl(void)                    /* J1, 0x92D38 */
{
    if (!q_begin(JOB_CTRL)) return;
    uint16_t *st = sup_job_table[JOB_CTRL].tx_stage;
    st[0] = (uint16_t)(0x8E00u | CELL_CTRL_STAGED);
    st[1] = (uint16_t)(0xC900u | CELL_C9_STAGED);
    st[2] = 0x2200u;
    st[3] = 0x0900u;
    CELL_J1_STAGED = st[0];                     /* bench mirror; on the
                                                 * target st IS 0xFFF88424 */
    q_commit(JOB_CTRL);
}

static void queue_id(void)                      /* J5, 0x92A84 */
{
    if (!q_begin(JOB_ID)) return;
    uint16_t *st = sup_job_table[JOB_ID].tx_stage;
    st[0] = 0x0600u;
    st[1] = 0x0300u;
    st[2] = 0x0000u;
    st[3] = 0x0000u;
    q_commit(JOB_ID);
}

static void queue_pos(void)                     /* J3, 0x9305A */
{
    if (!q_begin(JOB_POS)) return;
    uint16_t *st = sup_job_table[JOB_POS].tx_stage;
    st[0] = 0x2800u;
    st[1] = 0x2E00u;
    q_commit(JOB_POS);
}

static void queue_sslb2(void)                   /* J4, 0x93198 */
{
    if (!q_begin(JOB_SSLB2)) return;
    sup_job_table[JOB_SSLB2].tx_stage[0] = 0x00FFu;
    q_commit(JOB_SSLB2);
}

/* ── init (stock 0x91DD2 serial_rx_init law) ────────────────────────────── */

void sup_batch_init(void)
{
    s_stamp = 0u;
    sup_enable_request(false);          /* request 0, outputs-disabled 1 */
    CELL_SHUTDOWN_ST = 1u;              /* stock serial_rx_init 0x91DD2: shutdown-selftest
                                         * flag SET at init (C9 bit 4 stays clear; live
                                         * stock 0xFFF9094D = 01, first500_reads 2026-11-11) */
    CELL_SSLB2_RAW = 0x8000u;           /* the sentinel */
    /* the first bus-B job: J5 ID staged and queued */
    queue_id();
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
     * frames). The monitor has no lead-in either (r02d: 0x873F0 runs
     * every 2.5 ms from the first tick — see sup_monitor_poll).
     *
     * DECODED, NOT YET PORTED (post-certification, so outside the gate
     * window): once the OS leaves mode 0 the stock moves this batch, the
     * reply proc 0x8EE52 and the F stage 0x8EE30 to the 10 ms task (table
     * 0x43430 row 6, entries 3/75/76) while the J2 stage 0x8EDF6 — which
     * QUEUES J2 via 0x92E80 — stays at 5 ms. The switch (0x6F0B4 ->
     * 0x66E06) waits on startup module 2 (0xFFF93B81) <- module 1 <-
     * modules 7 and 11; module 11 (0xFFF93B85) is set by the mode
     * sequencer 0x85160 when the selftest sub-state reads 0x40 (ladder
     * complete) or its retry countdown 0xFFF9414D expires [length not
     * decoded]. On a gate-closed chip we therefore stay at 5 ms. */

    /* THE POOL ORDER, 1:1 for real (first500ms_diff §1 row 3; the transcript
     * tool caught the old order staging J6/J1 first with J3/J4 at the tail —
     * the reverse of the stock's wire sequence on every pass):
     *   1. reply proc 0x91ACA — queues J3 (0x9305A), then J4 (0x93198)
     *   2. stage J2 0x91804 (→0x91758+0x92E80), stage F 0x91810
     *   3. compose 0x91C86 → queue J6 0x92B82 + arm J1 0x92D38
     *   4. 0x91E12 letters chain
     *   5. queue J5 0x92A84
     *   6. 0x90ACC condense, 7. kick 0x921A0
     * J7 IS NEVER QUEUED — the stock never sends it (2026-11-11 decode,
     * three independent [O] lines): (1) no queuer: the descriptor address
     * 0x00040BC0 occurs nowhere in the flash image (every other bus-B
     * descriptor has exactly one literal in its queuer's pool; spec22
     * validation's caller-list absence agrees); (2) the stock driving RAM
     * dump holds J7's TX/RX staging 0xFFF885E0..E3 = 0000 0000 while J4's
     * neighbouring staging is live (00FF / 119F) — no reply ever landed;
     * (3) the stock's own handler-completeness supervisor 0x93126 ->
     * 0x931F8(&0x90973, ..., mask 7) expects only bits 0-2 — J7's handler
     * 0x92CC8 owns bit 3, outside the expected set (dump: 0x90973 = 07).
     * The NIB cell it would write (0x90979/0x90958) is fed every pass by
     * the J6 handler 0x92BBE, as ours does (sup_handlers.c). r01..r02d put
     * a word on the wire every 2nd pass that the chip never hears from the
     * stock. The descriptor and handler stay (dead, as in the stock). */
    s_stamp = plat_frt_us();
    plat_alive(0x05u);                  /* S11 breadcrumb: the 5 ms batch */

    /* 1. the reply proc 0x91ACA: condense the previous pass, then the
     *    starters — J3 FIRST, then J4 (S4, baseline review: 0x91C12 calls
     *    0x9305A via pool 0x91CE8, then 0x91C16 calls 0x93198 via 0x91CEC;
     *    first500ms_diff.md §1 row 3 and r02a..r02f carried J4 first) */
    sup_reply_condense();
    queue_pos();
    queue_sslb2();

    /* 2. stage J2 + F (the enables pair; the letters chain folds in below
     *    per the 0x91E12 law — the stock runs it after the J1 arm) */
    sup_enables_stage();

    /* 3. compose 0x91C86, then 0x91DAA: the 0x93126 summarizer, queue J6
     *    (0x92B82), arm J1 (0x92D38) */
    sup_ctrl_compose();
    sup_c9_compose();
    sup_reply_summarize();
    queue_stat();
    queue_ctrl();

    /* 4. the letters chain (0x91E12: the selftest runs in the batch pass —
     *    the stock's mode-sequencer slot, AFTER the J1 arm) */
    sup_selftest_step();

    /* 5. queue JOB_ID (0x92A84) */
    queue_id();

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
