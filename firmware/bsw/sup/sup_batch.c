/* sup_batch.c — the 5 ms batch (design §7.5; stock 0x8EE82 pool order, 1:1).
 *
 * Stock call order (artifact2 §2.3):
 *   timing → 0x91C86 (compose 0x8E) → 0x91DAA (= 0x93126 C9 summarize +
 *   0x92B82 queue J6 + 0x92D38 arm J1) → 0x91E12 (= jmp 0x92A84, queue
 *   J5; L1 corrected: the ladder is NOT in the batch — it is its own 5 ms
 *   list entry, run before the J2 stage, see sup_ladder.h) →
 *   0x90ACC (= jmp 0x921A0: the bus-A kick, queues the 0x40A98 ID job;
 *   bytes 0x90ACC) → 0x9305A/0x93198 (J3/J4 cyclic starters). J7 is
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
#include "bsw/sup/sup_ladder.h"
#include "bsw/sup/sup_walk.h"
#include "bsw/sup/sup_mode.h"
#include "platform.h"

/* THE OUTPUT TICK (2026-11-18, drv_jobs.c; ~/tcu_sweep/output_cadence_
 * decode.md §1, output_cadence_port.md): one law pass and one F stage per
 * output tick, at the stock's list slots [O lists 0x44CCC/0x44CF4 mode 0
 * 5 ms, 0x44E34/0x44E50 mode 1 10 ms]:
 *   0x8EE12 + 0x7CBA8  drv_out_tick_head  (harvest, feedback)
 *   0x7CC1E            drv_out_law        (0x83DB0 — nothing sent)
 *   0x8EE30            drv_f_stage        (0x901A0: compose; cyclic + config
 *                                          staged and queued EVERY pass)
 *   0x8EE82            drv_id_kick        (batch: the ID job, queued)
 *   0x7ABC8            drv_out_report     (DSM groups 1–20)
 * replacing r02w's refresh-on-change + the 10 ms periodic. Weak, so the
 * supervisor tests link without the bus-A driver. */
extern void drv_out_tick_head(void) __attribute__((weak));
extern void drv_out_law(void) __attribute__((weak));
extern void drv_f_stage(void) __attribute__((weak));
extern void drv_out_report(void) __attribute__((weak));
/* the READ-ONLY PWM-feedback shadow (app/fb_shadow.c, 2026-11-18,
 * ~/tcu_sweep/feedback_shadow_port.md): app/fb_hook.c's gather, right after
 * the stand-in at the 0x7CBA8 slot and before the law; weak (the supervisor
 * tests link without it), absent from the RAM application (zero margin) */
#if !defined(TCU_RAM_APP)
extern void app_out_head_hook(void) __attribute__((weak));
/* the PA7 shadow (app/pa7_hook.c, read-only) at the pin-cut entry 0x7CC32:
 * the stock's 0x86B0C is its 6th call, after 0x849D4; weak, so the
 * supervisor tests link without it (2026-11-18, r04d) */
extern void app_pin_cut_hook(void) __attribute__((weak));
#endif
/* M4 group A (drv_groupa.c / drv_temp.c; ~/tcu_sweep/groupA_park_port.md):
 * the stock's list entries, weak so the supervisor tests link without them —
 *   0x8F70C   the AN table, first call of the reply proc 0x8EE52
 *   0x7CB6E   → 0x83300 → 0x887D8, the hold-law temperature 0x94F9C
 *   0x7AB9C   → 0x7B6D8, slots 10..14 → the group-A demand 0x94FAA
 *   0x7CC32   → (0x85788 pin cut, 0x86A80, 0x86AF8) 0x849D4, the law
 *   0x8EE30   → (0x901A0) 0x91500, the group-A DRLD writer (before 0x91810)
 * Mode 0: all in the 5 ms list (entries 6/7, 17, 24, 26); mode 1: all in
 * the 10 ms list (entries 4/5, 70, 75, 76) [O task table 0x43430]. */
extern void drv_temp_an_tick(void) __attribute__((weak));
extern void drv_temp_tick(void) __attribute__((weak));
extern void drv_temp_pub_tick(void) __attribute__((weak));
extern void drv_ga_slots_tick(void) __attribute__((weak));
extern void drv_ga_law_tick(void) __attribute__((weak));
extern void drv_ga_fstage(void) __attribute__((weak));
/* the park position sensor, READ-ONLY (2026-11-16, drv_pksens.c;
 * ~/tcu_sweep/park_sensor_port.md) — hub c 0x83300 (behind 0x7CB6E):
 * entry 2 0x89320 (the reference latch) BEFORE 0x887D8, entry 9 0x88EF0
 * (monitor + evaluator 0x8DF2E) after it [O pool 0x8338C]; and 0x7B150 in
 * the poll group 0x7AB48 (mode 0: 5 ms list entry 9, right after the
 * publishers 0x7CB74; mode 1: 10 ms, right after 0x7CB6E) [O lists
 * 0x44CCC / 0x44E34]. Weak, so the supervisor tests link without them. */
extern void drv_pksens_ref_tick(void) __attribute__((weak));
extern void drv_pksens_tick(void) __attribute__((weak));
extern void drv_pksens_copy_tick(void) __attribute__((weak));

static uint32_t s_stamp;            /* the batch timing journal (FRT us) */
/* 0x67DF0 on one task object: countdown 0 → activate and reload, else
 * decrement. The cells are the stock's (sup_cells.h CELL_OS_CD5 / CD10). */
static bool os_tick(volatile uint16_t *cd, uint16_t reload)
{
    if (*cd == 0u) {
        *cd = reload;
        return true;
    }
    *cd = (uint16_t)(*cd - 1u);
    return false;
}

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
    CELL_OS_CD5 = 0u;                   /* both start at 0: the 10 ms task */
    CELL_OS_CD10 = 0u;                  /* fires with the first 5 ms pass  */
    sup_mode_init();                    /* 0x66DA4: mode 0, boot state 0 */
    sup_enable_request(false);          /* request 0, outputs-disabled 1 */
    CELL_SHUTDOWN_ST = 1u;              /* stock serial_rx_init 0x91DD2: shutdown-selftest
                                         * flag SET at init (C9 bit 4 stays clear; live
                                         * stock 0xFFF9094D = 01, first500_reads 2026-11-11) */
    CELL_SSLB2_RAW = 0x8000u;           /* the sentinel */
    /* L1: the mode-0 init list's sequencer init 0x85678 (row-9 entry
     * 0x7CBDC) — seq2 0x11, pin-cut mode 0x11, phase 1 / sub 0, retries 3 */
    sup_ladder_init();
    /* the first bus-B job: J5 ID staged and queued */
    queue_id();
}

/* ── the batch pieces (stock list entries; their per-mode placement is in
 *    sup_batch_5ms below) ─────────────────────────────────────────────── */

/* the reply proc 0x8EE52 → 0x91ACA: condense the previous pass, then the
 * starters — J3 FIRST, then J4 (S4, baseline review: 0x91C12 calls 0x9305A
 * via pool 0x91CE8, then 0x91C16 calls 0x93198 via 0x91CEC;
 * first500ms_diff.md §1 row 3 and r02a..r02f carried J4 first). Mode 0: 5 ms
 * list entry 6; mode 1: 10 ms list entry 4. */
static void reply_proc(void)
{
    if (drv_temp_an_tick) {
        drv_temp_an_tick();             /* 0x8F70C (M4: ADC_A part) */
    }
    sup_reply_condense();
    queue_pos();
    queue_sslb2();
}

/* the batch 0x8EE82 proper: compose 0x91C86, then 0x91DAA (the 0x93126
 * summarizer, queue J6 0x92B82, arm J1 0x92D38), 0x91E12 (= jmp 0x92A84,
 * queue J5), 0x90ACC (= jmp 0x921A0, the bus-A ID kick). Mode 0: 5 ms list
 * entry 27; mode 1: 10 ms list entry 77. (0x90B98, 0x91FF8, 0x90D58 at its
 * head: not ported.) */
static void batch_8ee82(void)
{
    sup_ctrl_compose();
    sup_c9_compose();
    sup_reply_summarize();
    queue_stat();
    queue_ctrl();
    queue_id();
    drv_id_kick();
    /* the builder law (0x91C86 tail): force the request cell while outputs
     * are not disabled — the composer does it with bit 0; the batch makes
     * the pass invariant explicit. */
    if (CELL_OUTPUTS_DIS != 1u) {
        CELL_REQUEST = 1u;
    }
}

/* ── the 5 ms batch task (stock 5 ms task 0x6A0F4 / 0x6A0E4 + 10 ms task
 *    0x6A078, both from the CMT0 handler 0x69FA4) ─────────────────────── */

void sup_batch_5ms(void)
{
    /* NO SILENCE WINDOW (r02c, decode 2026-11-11): the stock's batch runs
     * from the FIRST OS tick. 0x8EE82 is entry 27 of the 5 ms task's mode-0
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
     * THE MODES (2026-11-13, ~/tcu_sweep/mode1_port.md; sup_mode.c). The
     * CMT0 handler 0x69FA4 ticks the task objects in table order — 2.5 ms
     * 0x41320, 5 ms 0x4134C (modes 1..4) and 0x41378 (mode 0), 10 ms
     * 0x413D0 — each only in the modes its enable bytes (+0x1E + mode)
     * allow; the 5 ms pair shares one countdown cell (0x9355A), so the
     * mode switch keeps the phase. The OS runs the higher-priority task
     * first (0x411A0: 5 ms mask 0x100 / 0x80 above 10 ms 0x40), so on a
     * shared tick the 10 ms task runs right after the 5 ms task — here,
     * in the same call. Each task runs its row of table 0x43430 for the
     * mode read at its start (0x67E3C), then its tail hook: the 5 ms
     * 0x67A34 applies a pending mode (0x66E06) when mode != 0, the 10 ms
     * 0x67A42 when mode == 0.
     *
     * Our ported entries, per mode (flattened list positions):
     *   5 ms, mode 0 (0x42CD8): 2 countdown 0x7C7E4, 4 0x8EDEA (0x9181C;
     *     its 0x8F62C is task_adc_5ms), 6 reply proc 0x8EE52, 8 publishers
     *     0x7CB74, 14 mode request 0x6F0B4, 20 ladder 0x7CC76 (0x8570E),
     *     22 seq2 0x7CC56, 24 pin cut 0x7CC32, 25 J2 stage 0x8EDF6, 26 F
     *     stage 0x8EE30, 27 batch 0x8EE82
     *   5 ms, mode 1 (0x42F80): 2 countdown, 4 0x8EDEA, 5 publishers, 6
     *     0x6F0B4, 9 sequencer 0x7CC66 (0x85160 direct), 10 seq2, 11 J2
     *     stage 0x8EDF6 — NOTHING ELSE of ours
     *   10 ms, mode 0 (0x42D6C): none of ours (application / model work,
     *     module 3's done 0x7DB9C, the 0x67164 stamp)
     *   10 ms, mode 1 (0x42F44): 4 reply proc 0x8EE52, 75 pin cut 0x7CC32,
     *     76 F stage 0x8EE30, 77 batch 0x8EE82
     *   2.5 ms (monitor 0x7EE2E → 0x873F0): the same in both (main.c).
     * Bus B per pass, mode 0: J3, J4, J2, F, J6, J1, J5, F2. Mode 1: on a
     * 10 ms pass J2 (the 5 ms task, queued first), then J3, J4, F, J6,
     * J1, J5, F2; on the other pass J2 alone.
     *
     * THE POOL ORDER, 1:1 for real (first500ms_diff §1 row 3; the transcript
     * tool caught the old order staging J6/J1 first with J3/J4 at the tail —
     * the reverse of the stock's wire sequence on every pass).
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
    const uint8_t mode5 = sup_mode();   /* 0x67E3C: the mode at task start */
    bool due10;

    /* the CMT tick of this pass (0x69FA4 → 0x67DF0 per object): the 5 ms
     * object fires by construction (this call IS its run), the 10 ms one
     * on every second pass (reload 3 = 4 ticks); the in-between 2.5 ms-only
     * tick is taken at the end of the pass */
    (void)os_tick(&CELL_OS_CD5, 1u);
    due10 = os_tick(&CELL_OS_CD10, 3u);
    CELL_OS_RUNS5 = (uint16_t)(CELL_OS_RUNS5 + 1u);     /* 0x67E3C run count */
    s_stamp = plat_frt_us();
    plat_alive(0x05u);                  /* S11 breadcrumb: the 5 ms batch */

    /* L1: the 5 ms list's head — the countdown service 0x7C7E4 (entry 2,
     * the list's FIRST worker) and the 0x14 re-derive 0x9181C (entry 4) */
    sup_ladder_tick_head();

    if (mode5 == 0u) {
        /* ── the 5 ms task, mode 0 ── */
        reply_proc();                   /* 6: 0x8EE52 */
        if (drv_pksens_ref_tick) {
            drv_pksens_ref_tick();      /* 7: 0x7CB6E → 0x89320 (park sensor) */
        }
        if (drv_temp_tick) {
            drv_temp_tick();            /* 7: 0x7CB6E → 0x887D8 (M4) */
        }
        if (drv_pksens_tick) {
            drv_pksens_tick();          /* 7: 0x7CB6E → 0x88EF0 (park sensor) */
        }
        sup_ladder_tick_publish();      /* 8: 0x7CB74 */
        if (drv_pksens_copy_tick) {
            drv_pksens_copy_tick();     /* 9: 0x7AB48 → 0x7B150 (park sensor) */
        }
        if (drv_temp_pub_tick) {
            drv_temp_pub_tick();        /* 9: 0x7AB48 → 0x7C100 (temperature bytes) */
        }
        sup_mode_request();             /* 14: 0x6F0B4 */
        if (drv_ga_slots_tick) {
            drv_ga_slots_tick();        /* 0x44CFC: 0x7AB9C → 0x7B6D8 (M4) */
        }
        if (drv_out_tick_head) {
            /* 0x44D00 0x8EE12 (harvest) — and 0x44D0C 0x7CBA8 (the
             * feedback stand-in), which the stock runs after the ladder
             * 0x7CC76; no cell is shared across that interleave [I] */
            drv_out_tick_head();
        }
#if !defined(TCU_RAM_APP)
        if (app_out_head_hook) {
            app_out_head_hook();        /* the feedback shadow (read-only) */
        }
#endif
        /* L1: entries 20..24 BEFORE the J2 stage 0x8EDF6, so a request
         * reaches the J2 word the same tick (letters_law_decode.md §2.4) */
        sup_ladder_tick_seq(true);      /* 0x44D08: 0x7CC76 → 0x8570E */
        sup_ladder_tick_seq2();         /* 0x44D10: 0x7CC56 */
        if (drv_out_law) {
            drv_out_law();              /* 0x44D14: 0x7CC1E → 0x83DB0 */
        }
        sup_ladder_tick_cut();          /* 0x44D18: 0x7CC32 */
        if (drv_ga_law_tick) {
            drv_ga_law_tick();          /* 0x44D18: 0x7CC32 → 0x849D4 (M4) */
        }
#if !defined(TCU_RAM_APP)
        if (app_pin_cut_hook) {
            app_pin_cut_hook();         /* 0x44D18: 0x7CC32 → 0x86B0C (the PA7 shadow, read-only) */
        }
#endif
        sup_j2_stage();                 /* 0x44D1C: 0x8EDF6 → 0x91804 */
        if (drv_f_stage) {
            drv_f_stage();              /* 0x44D20: 0x8EE30 → 0x901A0 (cyclic, config) */
        }
        if (drv_ga_fstage) {
            drv_ga_fstage();            /* 0x44D20: 0x8EE30 → 0x91500 (M4) */
        }
        sup_f_stage();                  /* 0x44D20: 0x8EE30 → 0x91810 */
        batch_8ee82();                  /* 0x44D24: 0x8EE82 (… the bus-A ID job) */
        if (drv_out_report) {
            drv_out_report();           /* 0x44D28: 0x7ABC8 → 0x7B9B8 */
        }
        /* pump the engine: launch whatever's queued and the bus is free
         * (the stock's 0x937F8 dispatches at each queue, 0x93902) */
        sup_job_engine_pump();
        /* 0x67A34: no apply in mode 0 */
    } else {
        /* ── the 5 ms task, mode >= 1 (reachable: 1) ── */
        sup_ladder_tick_publish();      /* 5: 0x7CB74 */
        sup_mode_request();             /* 6: 0x6F0B4 */
        sup_ladder_tick_seq(false);     /* 9: 0x7CC66 → 0x85160 */
        sup_ladder_tick_seq2();         /* 10: 0x7CC56 */
        /* no law, no harvest, no F stage, no bus A here (0x44EC0) */
        sup_j2_stage();                 /* 11: 0x8EDF6 → 0x91804 */
        sup_job_engine_pump();          /* J2 goes out ahead of the 10 ms jobs */
        sup_mode_apply();               /* 0x67A34: mode != 0 → 0x66E06 */
    }

    if (due10) {
        const uint8_t mode10 = sup_mode();  /* the 10 ms task's own read */
        if (mode10 == 0u) {
            /* ── the 10 ms task, mode 0: nothing of ours in its list ── */
            sup_mode_apply();           /* 0x67A42: mode == 0 → 0x66E06 */
        } else {
            /* ── the 10 ms task, mode >= 1 ── */
            reply_proc();               /* 4: 0x8EE52 */
            if (drv_pksens_ref_tick) {
                drv_pksens_ref_tick();  /* 5: 0x7CB6E → 0x89320 (park sensor) */
            }
            if (drv_temp_tick) {
                drv_temp_tick();        /* 5: 0x7CB6E → 0x887D8 (M4) */
            }
            if (drv_pksens_tick) {
                drv_pksens_tick();      /* 5: 0x7CB6E → 0x88EF0 (park sensor) */
            }
            if (drv_pksens_copy_tick) {
                drv_pksens_copy_tick(); /* 6: 0x7AB48 → 0x7B150 (park sensor) */
            }
            if (drv_temp_pub_tick) {
                drv_temp_pub_tick();    /* 6: 0x7AB48 → 0x7C100 (temperature bytes) */
            }
            /* group 3 0x44E50: 0x7AB9C, 0x8EE12, (0x8EE80), 0x7CBA8,
             * 0x7CC1E, 0x7CC32, 0x8EE30, 0x8EE82, 0x7ABC8 — the model
             * group (the M5 producer 0xA30E2) ran before it: main.c's
             * task_10ms, 1 ms earlier (transcript_busa pins the order) */
            if (drv_ga_slots_tick) {
                drv_ga_slots_tick();    /* 0x44E50: 0x7AB9C → 0x7B6D8 (M4) */
            }
            if (drv_out_tick_head) {
                drv_out_tick_head();    /* 0x44E54 0x8EE12 + 0x44E5C 0x7CBA8 */
            }
#if !defined(TCU_RAM_APP)
            if (app_out_head_hook) {
                app_out_head_hook();    /* the feedback shadow (read-only) */
            }
#endif
            if (drv_out_law) {
                drv_out_law();          /* 0x44E60: 0x7CC1E → 0x83DB0 */
            }
            sup_ladder_tick_cut();      /* 0x44E64: 0x7CC32 */
            if (drv_ga_law_tick) {
                drv_ga_law_tick();      /* 0x44E64: 0x7CC32 → 0x849D4 (M4) */
            }
#if !defined(TCU_RAM_APP)
            if (app_pin_cut_hook) {
                app_pin_cut_hook();     /* 0x44E64: 0x7CC32 → 0x86B0C (the PA7 shadow, read-only) */
            }
#endif
            if (drv_f_stage) {
                drv_f_stage();          /* 0x44E68: 0x8EE30 → 0x901A0 (cyclic, config) */
            }
            if (drv_ga_fstage) {
                drv_ga_fstage();        /* 0x44E68: 0x8EE30 → 0x91500 (M4) */
            }
            sup_f_stage();              /* 0x44E68: 0x8EE30 → 0x91810 */
            batch_8ee82();              /* 0x44E6C: 0x8EE82 (… the bus-A ID job) */
            if (drv_out_report) {
                drv_out_report();       /* 0x44E70: 0x7ABC8 → 0x7B9B8 */
            }
            sup_job_engine_pump();
            /* 0x67A42: no apply in modes >= 1 */
        }
        CELL_OS_RUNS10 = (uint16_t)(CELL_OS_RUNS10 + 1u);
    }
    /* the next CMT tick (2.5 ms later): only the 2.5 ms task is due; the
     * 5 ms and 10 ms countdowns step (they can never fire here: both
     * started at 0, so the 10 ms fires only on a 5 ms tick) */
    (void)os_tick(&CELL_OS_CD5, 1u);
    (void)os_tick(&CELL_OS_CD10, 3u);
}
