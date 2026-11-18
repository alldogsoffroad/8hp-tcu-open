/* drv_jobs.h — bus-A driver job service (design §8, spec 22 §2.2).
 *
 * 2026-11-18 (~/tcu_sweep/output_cadence_decode.md, output_cadence_port.md):
 * THE STOCK'S OUTPUT CADENCE. One law pass and one F stage per output tick,
 * in the stock's list slots (sup_batch.c): mode 0 in the 5 ms task, modes
 * 1..3 in the 10 ms task's group-3 part; nothing of this on the odd 5 ms
 * pass of mode 1. The F stage (0x901A0) composes and stages + queues the
 * cyclic (0x924B0) AND the config job (0x92344) on EVERY pass, no change
 * detection. Bus A has the stock's queue semantics (0x937F8 / 0x93902 /
 * 0x93A08 / 0x93B8E): a 10-entry FIFO; a producer checks only its own job;
 * the burst end launches the next queued job BEFORE the finished job's
 * handler runs; nothing is dropped and a queued job's staging is never
 * overwritten.
 */
#ifndef TCU_DRV_JOBS_H
#define TCU_DRV_JOBS_H

/* THE RANGE CHAIN IS ISOLATED (app/rng_chain.c, ours): its translation
 * unit defines TCU_RNG_CHAIN_TU and may not include an output-side header —
 * its only way to the outputs is the range D6D6, read by app/park.c */
#if defined(TCU_RNG_CHAIN_TU)
#error "the range chain is isolated: it may not include an output-side header (drv_jobs.h)"
#endif

#include <stdint.h>
#include <stdbool.h>

#define DRV_ID     16
#define DRV_DIAG   17
#define DRV_CFG    18
#define DRV_CYC    19

/* the bus-A normal ring (stock 0x937F8: bus object +0x3C, 10 entries) */
#define DRV_RING_N 10u

/* host-only negative-test switch: the old no-queue launch (a busy channel
 * refuses the job). Never on the target. */
#if defined(__sh__) && defined(DRV_RING_DISABLE)
#error "drv_jobs: DRV_RING_DISABLE is a host negative-test switch only"
#endif

/* ── the job service ─────────────────────────────────────────────────── */
/* 0x937F8: stage-free job → ring (dispatch at once if bus A is idle). False
 * when the job is not free or the ring is full (counted as dropped). */
bool drv_job_queue(uint8_t id);
/* status == 0 (free), no side effect (bench / tests) */
bool drv_job_check_free(uint8_t id);
/* 0x93B8E as the producers call it: free → true; still pending → the job's
 * handler error branch runs (record error bits, no release — ours, see
 * drv_jobs.c), the miss is counted, false (skip staging this pass) */
bool drv_job_check_free_pass(uint8_t id);
void drv_jobs_init(void);

/* ── the output tick (stock list slots; sup_batch.c calls them) ──────── */
void drv_out_tick_head(void);   /* 0x8EE12 (0x904FA harvest) + 0x7CBA8 (0x838B0 feedback) */
void drv_out_law(void);         /* 0x7CC1E: 0x83DB0 with its monitors — no send */
void drv_f_stage(void);         /* 0x8EE30 → 0x901A0: compose, stage + queue cyclic, config */
void drv_out_report(void);      /* 0x7ABC8: the DSM report of groups 1–20 */
void drv_id_kick(void);         /* 0x8EE82 → 0x90ACC → 0x921A0: queue the ID job */
bool drv_diag_kick(void);       /* 0x9225C: queue the diag job (drv_odiag.c times it) */

/* law + compose + staging of the cyclic and config rows, NO queue — a test /
 * bench convenience (the firmware's tick is the four functions above) */
void drv_cyclic_build(const uint16_t *setpoints, uint8_t n_ch);

/* reply buffer of a finished bus-A job (bench FE12 results; NULL outside) */
const uint16_t *drv_job_rx(uint8_t id);
const uint16_t *drv_job_tx(uint8_t id);

/* the law / compose passes report themselves (drv_reqlaw.c) */
void drv_out_note_law(void);
void drv_out_note_compose(void);

/* ── bench witnesses (ours; .out_wit at 0xFFF87E00 on the flash app) ──── */
typedef struct {
    uint32_t queued[4];         /* [job - DRV_ID]: accepted into the ring */
    uint32_t launched[4];       /* launched from the ring (cyc = [3], cfg = [2]) */
    uint32_t done[4];           /* completions with frames */
    uint32_t err[4];            /* error completions (retry exhausted / reaped) */
    uint32_t missed[4];         /* 0x93B8E busy branch: own previous instance pending */
    uint32_t dropped[4];        /* queue refused (ring full / not free) — must stay 0 */
    uint32_t miss2_n;           /* a second consecutive miss (the stock's event 0xA51) */
    uint32_t launch_refused;    /* the ring head could not launch (channel owned elsewhere) */
    uint32_t mismatch;          /* a completion that was not the ring head's */
    uint32_t law_n;             /* law passes (0x83DB0, any caller) */
    uint32_t compose_n;         /* compose passes (0x901A0's record part, any caller) */
    uint32_t f_n;               /* F-stage passes */
    uint32_t law_f_bad;         /* F passes with law passes since the last F != 1 */
    uint32_t compose_f_bad;     /* F passes with compose passes != 1 */
    uint32_t t_law_us;          /* FRT at the last law pass */
    uint32_t t_f_us;            /* FRT at the last F stage */
    uint8_t  ring_hw;           /* FIFO high-water mark */
    uint8_t  ring_n;            /* entries now (incl. the launched head) */
    uint8_t  rsv[2];
} drv_out_wit_t;

const drv_out_wit_t *drv_out_wit(void);

#endif
