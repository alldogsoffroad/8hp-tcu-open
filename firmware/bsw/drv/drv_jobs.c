/* drv_jobs.c — bus-A driver job service (design §8, spec 22 §2.2).
 *
 * Four descriptors on the five-select table (i mod 5), bus 0, cyclic queue.
 * The 45-frame cyclic is the solenoid setpoint carrier (design §8).
 * The engine is shared with bus-B (sup_jobs.c); these descriptors extend
 * the same table and queue through the same one-in-flight path.
 */
#include "bsw/drv/drv_jobs.h"
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

/* THE CYCLIC (stock 0x924B0, template 0x40D0C).
 * COMMAND-MAJOR: the engine sends frame i to SSLA[SEL_A5[i mod 5]], so the
 * 45 frames are 9 commands x 5 chips: 88|mode x5, A0|dith0 x5, E8|sp0 x5,
 * A6|dith1 x5, F0|sp1 x5, then the reads 60, C0, 66, D8 x5 (driving dump
 * 0xFFF88518: 881A 881A 881A 8812 8815 | A000x5 | E853 ED89 ED8A E853 E800
 * | A600x5 | F589 F589 F053 F000 F000 | ...).
 *
 * r02w: THE ZERO-REQUEST LETTER LAW (0x83DB0 → 0x89540 → 0x901A0), every
 * request 0 (the application's demands are not ported; callers ignored):
 *   ch 0..7 (chip ch/2; cal 0x19154C = 0x00FF), chip 4 (ch 8/9) → '2'.
 *   seq2[0] in {0x11, 0x16, '3'}: mode 4 (FULL ON) when 0x94115 & 1, or
 *     0x94114 & 1 with seq2[0] in {0x11, 0x16} (masks 0x191550 / 0x19154E =
 *     0x3FF) — the stock's rail-off full-on tests (ladder phase 0x10, the
 *     '3' diagnosis) — else mode 0;
 *   any other seq2 state ('U' included): I = 0, no channel fault → mode 1.
 *   0x89540: mode 1 → letter 0, 4 → 'C', other → '2'.
 *   0x901A0 +7: 0x10 | code(ch0) | code(ch1) << 2, '2' → 1, 'C' → 3,
 *     '!' → 2, 0 → 0; setpoint 'C' → (0x2FFA + 0x9090A) / 6 = 0x7FF [I:
 *     0x9090A taken as 0], else 0.
 * So: pre-'U' 8815 (stage 0a), the tests 881F with E/F 7FF, after 'U'
 * 8810 on chips 0-3 (8815 on the empty chip 4). Deviation: our cyclic runs
 * at 10 ms (stock 5 ms), so a 5 ms test window may be missed. No channel
 * fault model (0x84040 / 0x93A42 taken as 0). */
static uint8_t drv_letter_code(uint8_t ch)
{
    const uint8_t s = CELL_SEQ2_STATE[0];

    if (ch >= 8u) {
        return 1u;                                   /* '2' */
    }
    if (s == 0x11u || s == 0x16u || s == (uint8_t)'3') {
        const bool full = ((CELL_DIAG_94115 & 1u) != 0u) ||
                          ((CELL_LAD_94114 & 1u) != 0u && (s == 0x11u || s == 0x16u));
        /* SAFETY INTERLOCK (ours, not stock): full-on only while no rail
         * request is up and the staged 0x90 word is 0x9000 — the stock
         * gets this from same-tick ordering; our cyclic is not on that tick */
        const bool rail_quiet = CELL_J2_STAGED == 0x9000u &&
                                CELL_LAD_REQ[0] == 0u && CELL_LAD_REQ[1] == 0u;
        return (full && rail_quiet) ? 3u : 1u;       /* 'C' : '2' */
    }
    return 0u;                                       /* mode 1 → letter 0 */
}

void drv_cyclic_build(const uint16_t *setpoints, uint8_t n_ch)
{
    (void)setpoints;
    (void)n_ch;
    for (uint8_t chip = 0u; chip < 5u; chip++) {
        const uint8_t c0 = drv_letter_code((uint8_t)(chip * 2u));
        const uint8_t c1 = drv_letter_code((uint8_t)(chip * 2u + 1u));
        const uint16_t sp0 = (c0 == 3u) ? 0x7FFu : 0u;
        const uint16_t sp1 = (c1 == 3u) ? 0x7FFu : 0u;
        stage_drv_cyc[0u + chip]  = (uint16_t)(0x8800u | 0x10u | c0 | (uint16_t)(c1 << 2));
        stage_drv_cyc[5u + chip]  = 0xA000u;
        stage_drv_cyc[10u + chip] = (uint16_t)(0xE800u | sp0);
        stage_drv_cyc[15u + chip] = 0xA600u;
        stage_drv_cyc[20u + chip] = (uint16_t)(0xF000u | sp1);
        stage_drv_cyc[25u + chip] = 0x6000u;
        stage_drv_cyc[30u + chip] = 0xC000u;
        stage_drv_cyc[35u + chip] = 0x6600u;
        stage_drv_cyc[40u + chip] = 0xD800u;
    }
    /* the config job's 0x88 row carries the same mode bytes (0x92344) */
    for (uint8_t chip = 0u; chip < 5u; chip++) {
        stage_drv_cfg[15u + chip] = stage_drv_cyc[chip];
    }
}

/* r02w: the same-tick refresh. Called from the 5 ms supervisor tick right
 * after seq2 (sup_batch.c, weak hook): when the letter law's mode bytes
 * differ from the last cyclic sent, build and send it NOW (or on the next
 * tick if bus A is busy), so the chips follow seq2 within one tick as the
 * stock's 0x83DB0 → 0x901A0 → 0x924B0 do — not up to 10 ms later. */
static uint16_t s_drv_sent_modes[5];
static uint8_t s_drv_refresh_pending;
volatile uint32_t s_drv_refresh_n;      /* witness: event-driven cyclics sent */

static bool drv_cyclic_send(void)
{
    if (!drv_job_check_free(DRV_CYC) || !drv_job_check_free(DRV_CFG)) {
        return false;
    }
    drv_cyclic_build(0, 8u);
    if (!drv_job_queue(DRV_CYC)) {
        return false;
    }
    for (uint8_t c = 0u; c < 5u; c++) {
        s_drv_sent_modes[c] = stage_drv_cyc[c];
    }
    return true;
}

void drv_letter_refresh(void)
{
    bool differ = false;
    for (uint8_t c = 0u; c < 5u; c++) {
        const uint16_t m = (uint16_t)(0x8810u | drv_letter_code((uint8_t)(c * 2u)) |
                                      (uint16_t)(drv_letter_code((uint8_t)(c * 2u + 1u)) << 2));
        if (m != s_drv_sent_modes[c]) {
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
}
