/* drv_jobs.c — bus-A driver job service (design §8, spec 22 §2.2).
 *
 * Four descriptors on the five-select table (i mod 5), bus 0, cyclic queue.
 * The 45-frame cyclic is the solenoid setpoint carrier (design §8).
 * The engine is shared with bus-B (sup_jobs.c); these descriptors extend
 * the same table and queue through the same one-in-flight path.
 */
#include "bsw/drv/drv_jobs.h"
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

/* THE CYCLIC (stock 0x924B0, template 0x40D0C) — r02v stage 0a.
 * COMMAND-MAJOR: the engine sends frame i to SSLA[SEL_A5[i mod 5]], so the
 * 45 frames are 9 commands x 5 chips: 88|mode x5, A0|dith0 x5, E8|sp0 x5,
 * A6|dith1 x5, F0|sp1 x5, then the reads 60, C0, 66, D8 x5 (driving dump
 * 0xFFF88518: 881A 881A 881A 8812 8815 | A000x5 | E853 ED89 ED8A E853 E800
 * | A600x5 | F589 F589 F053 F000 F000 | ...). Until the letter law
 * 0x83DB0 / 0x89540 / 0x901A0 is ported, every channel carries the stock's
 * pre-engagement content — letter '2', mode 0x15, setpoints and dither 0,
 * written EXPLICITLY (E800 / F000) — whatever the callers ask: no path
 * can put a bus-A channel in current control. (Up to r02u the layout was
 * chip-major: chips 0/3 got mode 0x1A, chip 2 0x12, and the setpoint rows
 * went out as 0x0000 reads — solenoid_chain_decode.md §F.1.) */
void drv_cyclic_build(const uint16_t *setpoints, uint8_t n_ch)
{
    static const uint16_t CMD[9] = {
        0x8815u, 0xA000u, 0xE800u, 0xA600u, 0xF000u,
        0x6000u, 0xC000u, 0x6600u, 0xD800u,
    };
    (void)setpoints;
    (void)n_ch;
    for (uint8_t r = 0u; r < 9u; r++) {
        for (uint8_t chip = 0u; chip < 5u; chip++) {
            stage_drv_cyc[r * 5u + chip] = CMD[r];
        }
    }
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
