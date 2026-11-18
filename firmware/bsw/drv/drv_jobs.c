/* drv_jobs.c — bus-A driver job service (design §8, spec 22 §2.2).
 *
 * Four descriptors on the five-select table (i mod 5), bus 0. The 45-frame
 * cyclic is the solenoid setpoint carrier (design §8).
 *
 * 2026-11-18 THE STOCK'S OUTPUT CADENCE AND BUS-A QUEUE (drv_jobs.h;
 * ~/tcu_sweep/output_cadence_decode.md §2/§4, output_cadence_port.md):
 *   - the ring = the stock's normal ring of bus 0 (0x937F8, 10 entries):
 *     queue → dispatch at once if the channel is idle (0x93902);
 *   - burst end (0x93A08): dequeue → LAUNCH THE NEXT HEAD → then the
 *     finished job's handler (harvest + release);
 *   - a producer checks only its own job (0x93B8E); a job still pending
 *     at its next pass takes its handler's error branch, staging skipped;
 *   - the F stage (0x8EE30 → 0x901A0) composes, then stages + queues the
 *     cyclic (0x924B0) and the config job (0x92344) on every pass.
 * The CPU-fed per-frame engine (plat_rspi.c) is unchanged: the ring sits
 * above it and owns every bus-A launch except the bench lab path.
 */
/* READ-ONLY park position sensor (2026-11-16): this translation unit is
 * an output path — the sensor headers refuse to compile here (and the
 * ctest pksens_readonly_scan fails if this file names a sensor cell) */
#define TCU_OUTPUT_PATH 1
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_odiag.h"
#include "bsw/sup/sup_cells.h"
#include <stdbool.h>
#include "bsw/sup/sup_jobs.h"
#include "platform.h"
#include <string.h>

/* five-select table (shared with sup_jobs.c): bus A's SPCMD values */
const uint16_t SEL_A5[] = { 0xAF31u, 0xAF21u, 0xAF11u, 0xAF01u, 0xAF51u };

/* TX templates (flash const, spec 22 §2.2) */
static const uint16_t TMPL_DRV_ID[10] = {
    0x0600u, 0x0600u, 0x0600u, 0x0600u, 0x0600u,
    0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u,
};
/* the diag job (stock 0x9225C, template 0x40CEE [O bytes]): COMMAND-MAJOR
 * like every bus-A job — 0x18 ×5 (channel 2c diagnosis), 0x1E ×5 (channel
 * 2c+1 diagnosis), 0x28 ×5 (the mode register) — queued 3.8 ms after each
 * cyclic completion (0x92530 → 0x92060 → 0x92078; drv_odiag.c) */
static const uint16_t TMPL_DRV_DIAG[15] = {
    0x1800u, 0x1800u, 0x1800u, 0x1800u, 0x1800u,
    0x1E00u, 0x1E00u, 0x1E00u, 0x1E00u, 0x1E00u,
    0x2800u, 0x2800u, 0x2800u, 0x2800u, 0x2800u,
};
static const uint16_t TMPL_DRV_CFG[40] = {
    /* 8 registers × 5 chips: writes 8E,90,96,88; reads 28,30,2E,36 */
    /* row 0: 0x8E writes (live 8E←9F) */
    0x8E9Fu, 0x8E9Fu, 0x8E9Fu, 0x8E9Fu, 0x8E9Fu,
    /* row 1: 0x90 writes (live 90←21) */
    0x9021u, 0x9021u, 0x9021u, 0x9021u, 0x9021u,
    /* row 2: 0x96 writes (live 96←55) */
    0x9655u, 0x9655u, 0x9655u, 0x9655u, 0x9655u,
    /* row 3: 0x88 writes — r02v stage 0a: the stock's PRE-ENGAGEMENT mode
     * byte on every chip, letters '2' (0x901A0 +7: cal bit4 0x10 | ch0 '2'
     * → 1 | ch1 '2' → 1 << 2 = 0x15). The engaged {1A,1A,1A,12,15} come
     * only from the letter law (0x83DB0 → 0x89540 → 0x901A0, not ported). */
    0x8815u, 0x8815u, 0x8815u, 0x8815u, 0x8815u,
    /* row 4: 0x28 reads */
    0x2800u, 0x2800u, 0x2800u, 0x2800u, 0x2800u,
    /* row 5: 0x30 reads */
    0x3000u, 0x3000u, 0x3000u, 0x3000u, 0x3000u,
    /* row 6: 0x2E reads */
    0x2E00u, 0x2E00u, 0x2E00u, 0x2E00u, 0x2E00u,
    /* row 7: 0x36 reads */
    0x3600u, 0x3600u, 0x3600u, 0x3600u, 0x3600u,
};
/* cyclic template is built at runtime from the solenoid state */

/* staging buffers — DECLARATION ORDER IS LAYOUT (GCC lays these statics
 * out in reverse order; the bench witness scripts read them by address):
 * nothing may be added or removed between here and s_drv_reap_n */
static uint16_t stage_drv_id[10];
static uint16_t stage_drv_diag[15];
static uint16_t stage_drv_cfg[40];
static uint16_t stage_drv_cyc[45];

/* reply buffers */
static uint16_t rx_drv_id[10], rx_drv_diag[15], rx_drv_cfg[40], rx_drv_cyc[45];

/* forward: handlers */
extern void drv_resp_id(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void drv_resp_diag(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void drv_resp_cfg(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void drv_resp_cyc(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);

static const rspi_done_fn s_drv_handlers[4] = {
    drv_resp_id, drv_resp_diag, drv_resp_cfg, drv_resp_cyc,
};

/* bus-A descriptors (extending the shared table; the completion of every
 * job goes through drv_burst_done, which then runs the handler) */
static const struct {
    uint8_t frames;
    uint16_t *tx_stage;
    uint16_t *rx;
} s_drv_table[4] = {
    { 10, stage_drv_id,   rx_drv_id   },
    { 15, stage_drv_diag, rx_drv_diag },
    { 40, stage_drv_cfg,  rx_drv_cfg  },
    { 45, stage_drv_cyc,  rx_drv_cyc  },
};

/* per-job STATUS (the stock's desc+0x10 byte; drv_handlers.c releases it
 * to 0 in the handler, ISR context): 0 free, 8 queued, 0x18 launched,
 * 0x40 done (the handler is running) */
volatile uint8_t s_drv_in_flight[4];
#define ST_FREE     0x00u
#define ST_QUEUED   0x08u
#define ST_LAUNCHED 0x18u
#define ST_DONE     0x40u

/* the ring's head is on the wire (was r02w's s_drv_between: same size,
 * same place — no .bss symbol moves) */
static uint8_t s_drv_ring_busy;      /* ring state: touched only under SR.I = 15 */

/* r02w's refresh snapshot, retired 2026-11-18 (no change detection: the F
 * stage stages every pass). RESERVED, so the .drv_wit witnesses after it
 * (s_rq_f238_n 0xFFF84C78 ...) keep their addresses */
#if defined(__sh__)
#define DRV_WIT __attribute__((section(".drv_wit"), used))
#else
#define DRV_WIT
#endif
static uint8_t s_drv_wit_rsv_let[10] DRV_WIT;
static uint16_t s_drv_wit_rsv_cur[10] DRV_WIT;

/* entries in the ring, the launched head included (was s_drv_refresh_pending) */
static uint8_t s_drv_ring_n;
/* F-stage passes (was s_drv_refresh_n, 0xFFF959B8 — the r03x witness
 * address now counts F passes: ≈ 200/s in mode 0, 100/s in mode 1) */
volatile uint32_t s_drv_fpass_n;

/* r02x: THE BUS-A REAPER. r02w's live read: bus A sat FEEDING the config
 * job at frame 38/40 from +0.623 s on, its RX interrupt count frozen —
 * a lost interrupt with no timeout, so bus A was dead from then on (r02v
 * showed the same stuck in-flight flag). A burst older than 5 ms is
 * error-completed (frames = 0) through drv_burst_done, which dequeues it,
 * launches the next head and runs the handler's error branch (release).
 * Under SR.I = 15 so no RX body can run beneath it. Root cause of the lost
 * interrupt (r02y/r02z): the RSPI re-entry, fixed by the §8.10.1 read-back;
 * the reaper stays as the backstop. */
volatile uint32_t s_drv_reap_n;         /* witness */
/* ── end of the fixed-layout block ─────────────────────────────────────── */

/* the ring and the witnesses (ours): .out_wit at 0xFFF87E00 on the flash
 * application — free low RAM, cleared by drv_jobs_init, no .bss symbol
 * moves; plain .bss on the host and in the RAM application */
#if defined(__sh__) && !defined(TCU_RAM_APP)
#define OUT_WIT __attribute__((section(".out_wit")))
#else
#define OUT_WIT
#endif
static volatile drv_out_wit_t s_out OUT_WIT;
/* the RAM application (its 64 KB image limit) carries the ring but not the
 * witness accounting */
#if defined(TCU_RAM_APP)
#define OW(stmt) do { } while (0)
#else
#define OW(stmt) do { stmt; } while (0)
#endif
/* (the ring and its counters are touched only under SR.I = 15, inside
 * windows bounded by plat_sr_set calls — not volatile) */
static uint8_t s_ring_q[DRV_RING_N] OUT_WIT;
static uint8_t s_ring_rd OUT_WIT;
static uint8_t s_ring_wr OUT_WIT;
static uint8_t s_miss_run[4] OUT_WIT;
static uint8_t s_law_since_f OUT_WIT;
static uint8_t s_comp_since_f OUT_WIT;

const drv_out_wit_t *drv_out_wit(void)
{
    s_out.ring_n = s_drv_ring_n;
    return (const drv_out_wit_t *)&s_out;
}

void drv_out_note_law(void)
{
#if defined(TCU_RAM_APP)
    return;
#endif
    s_out.law_n++;
    s_out.t_law_us = plat_frt_us();
    if (s_law_since_f != 0xFFu) {
        s_law_since_f++;
    }
}

void drv_out_note_compose(void)
{
#if defined(TCU_RAM_APP)
    return;
#endif
    s_out.compose_n++;
    if (s_comp_since_f != 0xFFu) {
        s_comp_since_f++;
    }
}

static uint32_t lock15(void)
{
    const uint32_t lk = plat_sr_i();
    plat_sr_set(15u);
    return lk;
}

static void drv_burst_done(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);

/* 0x93902 under the lock: launch the ring head if the channel is ours to
 * take. The registration is plat_rspi_launch's (its own SR.I window); the
 * caller pumps (frame 0) once the lock is dropped */
static bool ring_launch_head_locked(void)
{
#if defined(DRV_RING_DISABLE)
    return false;
#else
    uint8_t idx;

    if (s_drv_ring_busy != 0u || s_drv_ring_n == 0u) {
        return false;
    }
    idx = s_ring_q[s_ring_rd];
    if (!plat_rspi_launch(0u /* bus A */, (uint8_t)(DRV_ID + idx),
                          s_drv_table[idx].tx_stage, s_drv_table[idx].rx,
                          s_drv_table[idx].frames, SEL_A5, 5u, drv_burst_done)) {
        OW(s_out.launch_refused++);         /* a lab burst owns the channel */
        return false;
    }
    s_drv_ring_busy = 1u;
    s_drv_in_flight[idx] = ST_LAUNCHED;
    OW(s_out.launched[idx]++);
    return true;
#endif
}

static void ring_dispatch(void)
{
    const uint32_t lk = lock15();
    const bool go = ring_launch_head_locked();
    plat_sr_set(lk);
    if (go) {
        plat_rspi_pump(0u);
    }
}

/* 0x93A08 (+ 0x93A74's final error): the burst-end path of every bus-A job.
 * ISR context (the RSPI-A RX ISR, or the reaper under SR.I = 15). The next
 * head is launched BEFORE the finished job's handler runs; the handler
 * harvests the replies and releases the job (status 0). */
static void drv_burst_done(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    const uint8_t idx = (uint8_t)(job_id - DRV_ID);
    uint32_t lk;
    bool go;

    if (idx >= 4u) {
        return;
    }
    lk = lock15();
#if !defined(DRV_RING_DISABLE)
    if (s_drv_ring_busy != 0u && s_drv_ring_n != 0u && s_ring_q[s_ring_rd] == idx) {
        s_ring_rd = (uint8_t)((s_ring_rd + 1u) % DRV_RING_N);       /* 0x93AD8 */
        s_drv_ring_n--;
        s_drv_ring_busy = 0u;
    } else {
        OW(s_out.mismatch++);
    }
#endif
    s_drv_in_flight[idx] = ST_DONE;
    s_miss_run[idx] = 0u;
    OW(if (n != 0u) { s_out.done[idx]++; } else { s_out.err[idx]++; });
    go = ring_launch_head_locked();                                 /* 0x93902 */
    plat_sr_set(lk);
    if (go) {
        plat_rspi_pump(0u);
    }
    s_drv_handlers[idx](bus, job_id, rx, n);                    /* → handler */
}

static void drv_reap(void)
{
    const uint32_t lk = lock15();
    if (plat_rspi_reap(0u, 5000u)) {
        s_drv_reap_n++;
    }
    plat_sr_set(lk);
}

/* THE CYCLIC (stock 0x924B0, template 0x40D0C) and THE CONFIG JOB (stock
 * 0x92344, template 0x40AD8), staged from the per-chip driver records
 * 0xFFF91108 + 0x1C·c that 0x901A0 composes (drv_reqlaw.c).
 * COMMAND-MAJOR: the engine sends frame i to SSLA[SEL_A5[i mod 5]], so the
 * 45 frames are 9 commands x 5 chips: 88|rec+7 x5, A0|rec+0x10 x5,
 * E8|rec+0xC x5, A6|rec+0x18 x5, F0|rec+0x14 x5, then the reads 60, C0,
 * 66, D8 x5 (driving dump 0xFFF88518: 881A 881A 881A 8812 8815 | A000x5 |
 * E853 ED89 ED8A E853 E800 | A600x5 | F589 F589 F053 F000 F000 | ...);
 * the config rows 8E|rec+9, 90|rec+0xA, 96|rec+0xB, 88|rec+7 x5, reads
 * 28, 30, 2E, 36 x5. The callers' setpoints (sol_set / FE10) are ignored. */
static void drv_stage_cyc(void)
{
    for (uint8_t chip = 0u; chip < 5u; chip++) {
        stage_drv_cyc[0u + chip]  = (uint16_t)(0x8800u | CELL_REC_U8(chip, REC_MODE));
        stage_drv_cyc[5u + chip]  = (uint16_t)(0xA000u | CELL_REC_U8(chip, REC_DITH0));
        stage_drv_cyc[10u + chip] = (uint16_t)(0xE800u | CELL_REC_U16(chip, REC_SP0));
        stage_drv_cyc[15u + chip] = (uint16_t)(0xA600u | CELL_REC_U8(chip, REC_DITH1));
        stage_drv_cyc[20u + chip] = (uint16_t)(0xF000u | CELL_REC_U16(chip, REC_SP1));
        stage_drv_cyc[25u + chip] = 0x6000u;
        stage_drv_cyc[30u + chip] = 0xC000u;
        stage_drv_cyc[35u + chip] = 0x6600u;
        stage_drv_cyc[40u + chip] = 0xD800u;
    }
}

/* the template 0x40AD8 is copied once at init (drv_jobs_init): its read
 * rows 20..39 are never written, so re-copying it each pass (as 0x92344
 * does) would put the same words in the staging */
static void drv_stage_cfg(void)
{
    for (uint8_t chip = 0u; chip < 5u; chip++) {
        stage_drv_cfg[0u + chip]  = (uint16_t)(0x8E00u | CELL_REC_U8(chip, REC_CFG8E));
        stage_drv_cfg[5u + chip]  = (uint16_t)(0x9000u | CELL_REC_U8(chip, REC_CFG90));
        stage_drv_cfg[10u + chip] = (uint16_t)(0x9600u | CELL_REC_U8(chip, REC_CFG96));
        stage_drv_cfg[15u + chip] = (uint16_t)(0x8800u | CELL_REC_U8(chip, REC_MODE));
    }
}

/* test / bench convenience: the law (0x83DB0, without its monitors), the
 * composition (0x901A0's record part) and the staging of both jobs — NOT
 * on the firmware's path (sup_batch.c runs the four tick functions below) */
void drv_cyclic_build(const uint16_t *setpoints, uint8_t n_ch)
{
    (void)setpoints;
    (void)n_ch;
    drv_rq_law();
    drv_rq_compose();
    drv_stage_cyc();
    drv_stage_cfg();
}

/* ── the output tick (stock list slots; sup_batch.c) ────────────────────── */

/* 0x8EE12 (0x904FA: the chip status words from the records the handlers
 * filled) and 0x7CBA8 (0x838B0: the feedback chain — stand-ins) */
void drv_out_tick_head(void)
{
    drv_od_tick_head();
}

/* 0x7CC1E → 0x83DB0 with its monitors (0x89F5C, 0x897AC, 0x8A788, 0x84644,
 * 0x84392, 0x8493A): once per output tick; nothing is sent here */
void drv_out_law(void)
{
    drv_rq_law_diag();
}

/* 0x8EE30 → 0x901A0: the head check, the letters, the 0x9090A step and the
 * records (drv_rq_compose), then 0x924B0 and 0x92344 UNCONDITIONALLY
 * (0x90454 / 0x90458): each stages + queues its job when the job is free
 * (0x93B8E), else skips this pass (its handler's error branch). Before the
 * group-A writer 0x91500 and the F queuer 0x91810 (sup_batch.c). */
void drv_f_stage(void)
{
    drv_reap();
    ring_dispatch();                    /* a head a lab burst refused earlier */
    drv_rq_compose();
    s_drv_fpass_n++;
#if !defined(TCU_RAM_APP)
    s_out.f_n++;
    s_out.t_f_us = plat_frt_us();
    if (s_law_since_f != 1u) {
        s_out.law_f_bad++;
    }
    if (s_comp_since_f != 1u) {
        s_out.compose_f_bad++;
    }
    s_law_since_f = 0u;
    s_comp_since_f = 0u;
#endif
    if (drv_job_check_free_pass(DRV_CYC)) {         /* 0x924B0 */
        drv_stage_cyc();
        rx_drv_cyc[44] = 0u;                        /* the last RX word (0xFFF885CA) */
        (void)drv_job_queue(DRV_CYC);
    }
    if (drv_job_check_free_pass(DRV_CFG)) {         /* 0x92344 */
        drv_stage_cfg();
        rx_drv_cfg[39] = 0u;                        /* 0xFFF88232 */
        (void)drv_job_queue(DRV_CFG);
    }
}

/* 0x7ABC8 → 0x7B9B8: the DSM report of groups 1–20, after the batch */
void drv_out_report(void)
{
    drv_od_report();
}

/* ── the job service ───────────────────────────────────────────────────── */

bool drv_job_check_free(uint8_t id)
{
    const uint8_t idx = (uint8_t)(id - DRV_ID);
    return idx < 4u && s_drv_in_flight[idx] == ST_FREE;
}

/* 0x93B8E. DEVIATION (ours): the stock's busy branch also RELEASES the job
 * (0x93B68(desc, 1) → status 0) while its descriptor may still sit in the
 * ring or on the wire; ours runs the handler's error branch (the record
 * error bits) but does not release — the burst's own completion or the
 * 5 ms reaper does, so a queued staging is never overwritten and the ring
 * never holds a job twice. The second consecutive miss is the stock's event
 * 0xA51 (0x6F5C0, logged once per bus): counted. */
bool drv_job_check_free_pass(uint8_t id)
{
    extern void drv_resp_miss(uint8_t idx);
    const uint8_t idx = (uint8_t)(id - DRV_ID);
    uint32_t lk;
    bool free_now;

    if (idx >= 4u) {
        return false;
    }
    if (s_drv_in_flight[idx] == ST_FREE) {
        return true;
    }
#if defined(TCU_RAM_APP)
    (void)lk;
    return false;               /* the RAM application: no miss accounting (64 KB) */
#endif
    lk = lock15();
    free_now = s_drv_in_flight[idx] == ST_FREE;
    if (!free_now && s_drv_in_flight[idx] != ST_DONE) {
        OW(s_out.missed[idx]++);
        if (s_miss_run[idx] != 0u) {
            OW(s_out.miss2_n++);
        }
        if (s_miss_run[idx] != 0xFFu) {
            s_miss_run[idx]++;
        }
        drv_resp_miss(idx);
    }
    plat_sr_set(lk);
    return free_now;
}

/* 0x937F8: put a free, staged job in the ring; dispatch now if idle */
bool drv_job_queue(uint8_t id)
{
    const uint8_t idx = (uint8_t)(id - DRV_ID);
    uint32_t lk;
    bool go;

    if (idx >= 4u) {
        return false;
    }
    lk = lock15();
#if defined(DRV_RING_DISABLE)
    /* the r02v..r03y law (negative test): no queue — a busy channel refuses */
    if (s_drv_in_flight[idx] != ST_FREE ||
        !plat_rspi_launch(0u, id, s_drv_table[idx].tx_stage, s_drv_table[idx].rx,
                          s_drv_table[idx].frames, SEL_A5, 5u, drv_burst_done)) {
        OW(s_out.dropped[idx]++);
        plat_sr_set(lk);
        return false;
    }
    s_drv_in_flight[idx] = ST_LAUNCHED;
    OW(s_out.queued[idx]++);
    OW(s_out.launched[idx]++);
    go = true;
#else
    if (s_drv_in_flight[idx] != ST_FREE || s_drv_ring_n >= DRV_RING_N) {
        OW(s_out.dropped[idx]++);
        plat_sr_set(lk);
        return false;
    }
    s_ring_q[s_ring_wr] = idx;
    s_ring_wr = (uint8_t)((s_ring_wr + 1u) % DRV_RING_N);
    s_drv_ring_n++;
    OW(if (s_drv_ring_n > s_out.ring_hw) { s_out.ring_hw = s_drv_ring_n; });
    s_drv_in_flight[idx] = ST_QUEUED;
    OW(s_out.queued[idx]++);
    go = ring_launch_head_locked();
#endif
    plat_sr_set(lk);
    if (go) {
        plat_rspi_pump(0u);
    }
    return true;
}

/* 0x9225C (the 0x92078 ISR's job; ours polled, drv_odiag.c): check-free,
 * stage the 0x40CEE template, clear the last RX word (0xFFF88516), queue */
bool drv_diag_kick(void)
{
    if (!drv_job_check_free_pass(DRV_DIAG)) {
        return false;
    }
    memcpy(stage_drv_diag, TMPL_DRV_DIAG, sizeof TMPL_DRV_DIAG);
    rx_drv_diag[14] = 0u;
    return drv_job_queue(DRV_DIAG);
}

/* 0x921A0 (0x90ACC, every batch pass): check-free, stage 0x40A84, clear the
 * last RX word (0xFFF88192), queue — it waits in the ring behind the
 * cyclic and config, never dropped (0x96390(0), the launcher's SPCMD-table
 * cache reset, has no counterpart: ours reloads SPCMD at every launch) */
void drv_id_kick(void)
{
    if (drv_job_check_free_pass(DRV_ID)) {
        memcpy(stage_drv_id, TMPL_DRV_ID, sizeof TMPL_DRV_ID);
        rx_drv_id[9] = 0u;
        (void)drv_job_queue(DRV_ID);
    }
}

const uint16_t *drv_job_rx(uint8_t id)
{
    const uint8_t idx = (uint8_t)(id - DRV_ID);
    return idx < 4u ? s_drv_table[idx].rx : 0;
}

const uint16_t *drv_job_tx(uint8_t id)
{
    const uint8_t idx = (uint8_t)(id - DRV_ID);
    return idx < 4u ? s_drv_table[idx].tx_stage : 0;
}

void drv_jobs_init(void)
{
    memset((void *)s_drv_in_flight, 0, sizeof s_drv_in_flight);
    memset(stage_drv_cyc, 0, sizeof stage_drv_cyc);
    memcpy(stage_drv_cfg, TMPL_DRV_CFG, sizeof TMPL_DRV_CFG);
#if !defined(TCU_RAM_APP)
    memset(s_drv_wit_rsv_let, 0, sizeof s_drv_wit_rsv_let);    /* NOLOAD on SH */
    memset(s_drv_wit_rsv_cur, 0, sizeof s_drv_wit_rsv_cur);
#endif
    s_drv_ring_busy = 0u;
    s_drv_ring_n = 0u;
    s_drv_fpass_n = 0u;
    s_ring_rd = 0u;
    s_ring_wr = 0u;
#if !defined(TCU_RAM_APP)        /* (the RAM application's .bss starts zeroed; 64 KB) */
    memset((void *)&s_out, 0, sizeof s_out);
    memset(s_ring_q, 0, sizeof s_ring_q);
    memset(s_miss_run, 0, sizeof s_miss_run);
    s_law_since_f = 0u;
    s_comp_since_f = 0u;
#endif
    drv_rq_init();                      /* 0x90010's part: letters '2', 0x9090A, cfg bytes */
}
