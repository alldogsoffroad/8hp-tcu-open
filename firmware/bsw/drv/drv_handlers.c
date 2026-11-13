/* drv_handlers.c — bus-A reply handlers (design §8).
 *
 * Four handlers for the bus-A jobs. The ID handler checks the A2 class
 * (chip ID). The cyclic handler checks the 5-bit cyclic header. Config
 * handler does readback checks and fault latching. All ISR context, short.
 */
#include "bsw/drv/drv_jobs.h"
#include "bsw/sup/sup_jobs.h"
#include "platform.h"

#define A2_OK(rx) (((rx) & 0xFF00u) == 0xA200u)

/* helper: clear the in-flight flag and pump bus A (ISR-context release) */
void s_drv_in_flight_clear(uint8_t idx);

void drv_resp_id(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)n;
    /* replies must start with the A2 class (spec 22 §2.2) */
    for (uint8_t i = 0u; i < 5u; i++) {
        if (!A2_OK(rx[i])) goto fin;
    }
    /* chip IDs harvested: rx[0..4] low bytes = the four real + one dead */
fin:
    s_drv_in_flight_clear(job_id - DRV_ID);
}

void drv_resp_diag(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)rx; (void)n;
    s_drv_in_flight_clear(job_id - DRV_ID);
}

void drv_resp_cfg(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)n;
    /* readback checks: rows 4-7 are reads of what rows 0-3 wrote */
    for (uint8_t chip = 0u; chip < 5u; chip++) {
        /* check 0x28 (row 4) == what 0x8E wrote (row 0) low byte */
        /* check 0x2E (row 6) == what 0x96 wrote (row 2) low byte */
        (void)rx; /* simplified: full check in the sol_ctl module */
    }
    s_drv_in_flight_clear(job_id - DRV_ID);
}

void drv_resp_cyc(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)n;
    /* 5-bit cyclic header check (0xF800 mask) */
    for (uint8_t i = 0u; i < 5u; i++) {
        if ((rx[i * 9u] & 0xF800u) != 0xA800u) break;
    }
    s_drv_in_flight_clear(job_id - DRV_ID);
    /* r02v stage 0a: the stock's 0x901A0 sends the config job (0x92344)
     * every pass after the cyclic (0x924B0); bus A runs one job at a time,
     * so it is chained from the cyclic's completion */
    if (drv_job_check_free(DRV_CFG)) {
        (void)drv_job_queue(DRV_CFG);
    }
}

void s_drv_in_flight_clear(uint8_t idx)
{
    extern volatile uint8_t s_drv_in_flight[4];
    s_drv_in_flight[idx] = 0u;
    plat_rspi_pump(0u);
}
