/* drv_handlers.c — bus-A reply handlers (design §8).
 *
 * Four handlers for the bus-A jobs. The ID handler checks the A2 class
 * (chip ID). The cyclic handler checks the 5-bit cyclic header. Config
 * handler does readback checks and fault latching. All ISR context, short.
 */
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_odiag.h"
#include "bsw/sup/sup_jobs.h"
#include "platform.h"
#include <stdbool.h>

#define A2_OK(rx) (((rx) & 0xFF00u) == 0xA200u)

/* helper: clear the in-flight flag and pump bus A (ISR-context release) */
void s_drv_in_flight_clear(uint8_t idx);

/* 2026-11-17: every handler harvests into the per-chip records as the
 * stock's do (0x921E0 / 0x92298 / 0x923BC / 0x92530, drv_odiag.c); n = 0
 * (an error completion) is the stock's not-'@' branch */
void drv_resp_id(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus;
    drv_od_rx_id(rx, n != 0u);                  /* 0x921E0 */
    s_drv_in_flight_clear(job_id - DRV_ID);
}

void drv_resp_diag(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus;
    drv_od_rx_diag(rx, n != 0u);                /* 0x92298 */
    s_drv_in_flight_clear(job_id - DRV_ID);
}

void drv_resp_cfg(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus;
    drv_od_rx_cfg(drv_job_tx(DRV_CFG), rx, n != 0u);    /* 0x923BC */
    s_drv_in_flight_clear(job_id - DRV_ID);
}

/* r02y witness: cyclic replies that slipped — a 0000 at a populated select
 * (i mod 5 != 4) or a nonzero word at the empty SSLA5 (i mod 5 == 4) */
volatile uint32_t s_drv_slip_n;
volatile uint32_t s_drv_cyc_ok_n;

void drv_resp_cyc(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus;
    if (n != 0u) {
        bool slip = false;
        for (uint8_t i = 0u; i < 45u; i++) {
            const bool empty = (i % 5u) == 4u;
            if (empty ? (rx[i] != 0u) : (rx[i] == 0u)) {
                slip = true;
            }
        }
        if (slip) {
            s_drv_slip_n++;
        } else {
            s_drv_cyc_ok_n++;
        }
    }
    drv_od_rx_cyc(drv_job_tx(DRV_CYC), rx, n != 0u);   /* 0x92530 */
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
