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

/* helper: release the job (status 0; ISR context). 2026-11-18: no pump —
 * drv_burst_done (drv_jobs.c, the stock's 0x93A08 order) has already
 * launched the next ring head before this handler runs */
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
    /* 2026-11-18: the config job is no longer chained from here — the F
     * stage queues it right behind the cyclic every pass (0x901A0 →
     * 0x924B0, 0x92344; drv_jobs.c) */
}

/* the 0x93B8E busy branch (drv_jobs.c): the job's own previous instance is
 * still pending at its next pass — the handler's ERROR branch (the record
 * error bits, as with frames = 0), WITHOUT the release (ours: the burst's
 * completion or the reaper releases it). Called under SR.I = 15. */
void drv_resp_miss(uint8_t idx)
{
#if defined(TCU_RAM_APP)
    (void)idx;                  /* no diagnosis in the RAM application: no record bits */
    return;
#endif
    switch (idx) {
    case 0u: drv_od_rx_id(drv_job_rx(DRV_ID), false); break;                       /* 0x921E0 */
    case 1u: drv_od_rx_diag(drv_job_rx(DRV_DIAG), false); break;                   /* 0x92298 */
    case 2u: drv_od_rx_cfg(drv_job_tx(DRV_CFG), drv_job_rx(DRV_CFG), false); break; /* 0x923BC */
    case 3u: drv_od_rx_cyc(drv_job_tx(DRV_CYC), drv_job_rx(DRV_CYC), false); break; /* 0x92530 */
    default: break;
    }
}

void s_drv_in_flight_clear(uint8_t idx)
{
    extern volatile uint8_t s_drv_in_flight[4];
    s_drv_in_flight[idx] = 0u;
}
