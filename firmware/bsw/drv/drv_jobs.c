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
    /* row 3: 0x88 writes (live 88←{1A,1A,1A,12,15}) */
    0x881Au, 0x881Au, 0x881Au, 0x8812u, 0x8815u,
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

/* the cyclic row structure (design §8):
 * 9 words per chip × 5 chips = 45 frames
 * rows: 88|mode, A0|flags, E8|sp0, A6, F0|sp1, 60, C0, 66, D8 */
void drv_cyclic_build(const uint16_t *setpoints, uint8_t n_ch)
{
    (void)n_ch;  /* always 8 channels (4 chips × 2) */
    static const uint16_t MODE[5] = { 0x881Au, 0x881Au, 0x881Au, 0x8812u, 0x8815u };
    for (uint8_t chip = 0u; chip < 5u; chip++) {
        uint16_t *row = &stage_drv_cyc[chip * 9u];
        row[0] = MODE[chip];                 /* 88|mode */
        row[1] = 0xA000u;                    /* A0|dither-flags */
        row[2] = setpoints[chip * 2u];       /* E8|sp_lo (11-bit) */
        row[3] = 0xA600u;                    /* A6 */
        row[4] = setpoints[chip * 2u + 1u];  /* F0|sp_hi (11-bit) */
        row[5] = 0x6000u;                    /* 60 */
        row[6] = 0xC000u;                    /* C0 */
        row[7] = 0x6600u;                    /* 66 */
        row[8] = 0xD800u;                    /* D8 */
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

void drv_jobs_init(void)
{
    memset(s_drv_in_flight, 0, sizeof s_drv_in_flight);
    memset(stage_drv_cyc, 0, sizeof stage_drv_cyc);
}
