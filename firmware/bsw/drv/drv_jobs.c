/* drv_jobs.c — bus-A driver job service (design §8, spec 22 §2.2).
 *
 * Four descriptors on the five-select table (i mod 5), bus 0, cyclic queue.
 * The 45-frame cyclic is the solenoid setpoint carrier (design §8).
 * The engine is shared with bus-B (sup_jobs.c); these descriptors extend
 * the same table and queue through the same one-in-flight path.
 */
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_reqlaw.h"
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
static const uint16_t TMPL_DRV_DIAG[15] = {
    0x1800u, 0x1E00u, 0x2800u,   /* per chip ×5 */
    0x1800u, 0x1E00u, 0x2800u,
    0x1800u, 0x1E00u, 0x2800u,
    0x1800u, 0x1E00u, 0x2800u,
    0x1800u, 0x1E00u, 0x2800u,
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

/* staging buffers */
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

/* bus-A descriptors (extending the shared table) */
static const struct {
    uint8_t frames;
    uint16_t *tx_stage;
    uint16_t *rx;
    rspi_done_fn done;
    volatile uint8_t *in_flight;
} s_drv_table[4] = {
    { 10, stage_drv_id,   rx_drv_id,   drv_resp_id,   NULL },
    { 15, stage_drv_diag, rx_drv_diag, drv_resp_diag, NULL },
    { 40, stage_drv_cfg,  rx_drv_cfg,  drv_resp_cfg,  NULL },
    { 45, stage_drv_cyc,  rx_drv_cyc,  drv_resp_cyc,  NULL },
};

/* per-job in-flight flags (drv_handlers.c clears them: ISR-context release) */
volatile uint8_t s_drv_in_flight[4];

/* THE CYCLIC (stock 0x924B0, template 0x40D0C) and THE CONFIG JOB (stock
 * 0x92344, template 0x40AD8), staged from the per-chip driver records
 * 0xFFF91108 + 0x1C·c that 0x901A0 composes (drv_reqlaw.c).
 * COMMAND-MAJOR: the engine sends frame i to SSLA[SEL_A5[i mod 5]], so the
 * 45 frames are 9 commands x 5 chips: 88|rec+7 x5, A0|rec+0x10 x5,
 * E8|rec+0xC x5, A6|rec+0x18 x5, F0|rec+0x14 x5, then the reads 60, C0,
 * 66, D8 x5 (driving dump 0xFFF88518: 881A 881A 881A 8812 8815 | A000x5 |
 * E853 ED89 ED8A E853 E800 | A600x5 | F589 F589 F053 F000 F000 | ...);
 * the config rows 8E|rec+9, 90|rec+0xA, 96|rec+0xB, 88|rec+7 x5, reads
 * 28, 30, 2E, 36 x5.
 *
 * r03b: THE REQUEST LAW (0x83DB0 → 0x89540 → 0x901A0 → 0x924B0/0x92344),
 * ported 1:1 in drv_reqlaw.c, replaces r02w's zero-request letter shortcut
 * (drv_letter_code). On SH every request is a compile-time 0 (the safety
 * frame, drv_reqlaw.h), so the staged content is the stock's at zero
 * requests: pre-'U' (seq2 0x11/0x16/'3') 8815, after 'U' 8810 on chips
 * 0-3 (chip 4: 8815), the rail-off full-on tests 881F / EFFF / F7FF under
 * our interlock (full-on only while the staged J2 word is 0x9000 and both
 * request cells are 0). The callers' setpoints (sol_set / FE10) are
 * ignored, as before. Deviation: our cyclic + config run at 10 ms plus the
 * same-tick refresh (stock F stage: 5 ms in mode 0, 10 ms in mode 1). */
static void drv_stage_from_records(void)
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
        /* 0x92344: the config rows from the same record */
        stage_drv_cfg[0u + chip]  = (uint16_t)(0x8E00u | CELL_REC_U8(chip, REC_CFG8E));
        stage_drv_cfg[5u + chip]  = (uint16_t)(0x9000u | CELL_REC_U8(chip, REC_CFG90));
        stage_drv_cfg[10u + chip] = (uint16_t)(0x9600u | CELL_REC_U8(chip, REC_CFG96));
        stage_drv_cfg[15u + chip] = (uint16_t)(0x8800u | CELL_REC_U8(chip, REC_MODE));
    }
}

/* the law (0x83DB0, idempotent on its inputs) and the composition
 * (0x901A0), then the staging; the setpoint arguments are ignored */
void drv_cyclic_build(const uint16_t *setpoints, uint8_t n_ch)
{
    (void)setpoints;
    (void)n_ch;
    drv_rq_law();
    drv_rq_compose();
    drv_stage_from_records();
}

/* r02w: the same-tick refresh. Called from the 5 ms supervisor tick right
 * after seq2 (sup_batch.c, weak hook) — the stock's 0x83DB0 slot (5 ms
 * entry 0x7CC1E, right after seq2 0x7CC56). r03b: it runs the request law
 * and, when the letters or currents differ from the last cyclic sent,
 * sends a new cyclic NOW (or on the next tick if bus A is busy), so the
 * chips follow seq2 within one tick as the stock's 0x83DB0 → 0x901A0 →
 * 0x924B0 do. Snapshot in .drv_wit (not .bss: the stack guard). */
#if defined(__sh__)
#define DRV_WIT __attribute__((section(".drv_wit")))
#else
#define DRV_WIT
#endif
static uint8_t s_drv_sent_let[DRV_RQ_NCH] DRV_WIT;
static uint16_t s_drv_sent_cur[DRV_RQ_NCH] DRV_WIT;
static uint8_t s_drv_refresh_pending;
volatile uint32_t s_drv_refresh_n;      /* witness: event-driven cyclics sent */

/* r02x: THE BUS-A REAPER. r02w's live read: bus A sat FEEDING the config
 * job at frame 38/40 from +0.623 s on, its RX interrupt count frozen —
 * a lost interrupt with no timeout, so bus A was dead from then on (r02v
 * showed the same stuck in-flight flag). The bus-B engine reaps from its
 * check-free; bus A had nothing. A burst older than 5 ms is error-completed
 * (frames = 0) through its handler, which releases the job. Under SR.I = 15
 * so no RX body can run beneath it. Root cause of the lost interrupt: open. */
volatile uint32_t s_drv_reap_n;         /* witness */

static void drv_reap(void)
{
    const uint32_t lk = plat_sr_i();
    plat_sr_set(15u);
    if (plat_rspi_reap(0u, 5000u)) {
        s_drv_reap_n++;
    }
    plat_sr_set(lk);
}

static bool drv_cyclic_send(void)
{
    drv_reap();
    if (!drv_job_check_free(DRV_CYC) || !drv_job_check_free(DRV_CFG)) {
        return false;
    }
    drv_cyclic_build(0, 8u);
    if (!drv_job_queue(DRV_CYC)) {
        return false;
    }
    for (uint8_t ch = 0u; ch < DRV_RQ_NCH; ch++) {
        s_drv_sent_let[ch] = CELL_RQ_LETTER[ch];
        s_drv_sent_cur[ch] = CELL_RQ_CUR[ch];
    }
    return true;
}

void drv_letter_refresh(void)
{
    bool differ = false;
    drv_rq_law();                       /* 0x83DB0 (+ 0x89540) this tick */
    for (uint8_t ch = 0u; ch < DRV_RQ_NCH; ch++) {
        if (CELL_RQ_LETTER[ch] != s_drv_sent_let[ch] ||
            CELL_RQ_CUR[ch] != s_drv_sent_cur[ch]) {
            differ = true;
        }
    }
    if (differ || s_drv_refresh_pending != 0u) {
        if (drv_cyclic_send()) {
            s_drv_refresh_pending = 0u;
            s_drv_refresh_n++;
        } else {
            s_drv_refresh_pending = 1u;
        }
    }
}

/* the 10 ms periodic send (sol_tick_10ms) */
void drv_cyclic_periodic(void)
{
    (void)drv_cyclic_send();
}

bool drv_job_check_free(uint8_t id)
{
    uint8_t idx = id - DRV_ID;
    return idx < 4u && s_drv_in_flight[idx] == 0u;
}

bool drv_job_queue(uint8_t id)
{
    uint8_t idx = id - DRV_ID;
    if (idx >= 4u || s_drv_in_flight[idx]) {
        return false;
    }
    const rspi_done_fn done = s_drv_handlers[idx];
    if (!plat_rspi_launch(0u /* bus A */, id,
                          s_drv_table[idx].tx_stage, s_drv_table[idx].rx,
                          s_drv_table[idx].frames, SEL_A5, 5u, done)) {
        return false;
    }
    s_drv_in_flight[idx] = 1u;
    plat_rspi_pump(0u);
    return true;
}

void drv_id_kick(void)
{
    if (drv_job_check_free(DRV_ID)) {
        memcpy(stage_drv_id, TMPL_DRV_ID, sizeof TMPL_DRV_ID);
        drv_job_queue(DRV_ID);
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
    memset(s_drv_in_flight, 0, sizeof s_drv_in_flight);
    memset(stage_drv_cyc, 0, sizeof stage_drv_cyc);
    memcpy(stage_drv_cfg, TMPL_DRV_CFG, sizeof TMPL_DRV_CFG);   /* r02v: sent every pass */
    memset(s_drv_sent_let, 0, sizeof s_drv_sent_let);          /* NOLOAD on SH */
    memset(s_drv_sent_cur, 0, sizeof s_drv_sent_cur);
    drv_rq_init();                      /* 0x90010's part: letters '2', 0x9090A, cfg bytes */
}
