#ifndef TCU_STUB_PLAT_H
#define TCU_STUB_PLAT_H

#include "platform/platform.h"

#define STUB_TX_MAX 64u

typedef struct {
    can_frame_t tx[STUB_TX_MAX];
    can_bus_t tx_bus[STUB_TX_MAX];
    unsigned n_tx;
    unsigned resets, handovers;
    uint8_t rec[0x48];
    uint8_t ram[0x200];         /* simulated target memory at 0xFFF80000 */
    uint16_t adc[47];           /* left-aligned raw result per AN number */
    bool adc_on[47];
    speed_raw_t speed[SPEED_CHANNELS];
    bool speed_on[SPEED_CHANNELS];
    uint16_t sup_tx[64];
    unsigned sup_n;
    bool sup_busy;              /* transmitter not free */
    uint16_t sup_reply;         /* reply returned for every frame after the first */
    uint16_t sup_xfer_rx;       /* plat_sup_xfer: reply returned for every frame */
    uint16_t (*sup_reply_fn)(uint16_t tx);  /* per-word reply generator (fake supervisor) */
    bool sup_xfer_fail;         /* plat_sup_xfer: bus timeout */
    bool drv_fail;              /* driver bus timeout */
    bool drv_bad_header;        /* chips reply with a wrong header */
    bool drv_bad_readback;      /* chips return wrong read-back values */
    uint8_t drv_reg[5][256];    /* simulated chip registers, by write command */
    uint16_t drv_last_tx[45];
    unsigned drv_jobs;
    unsigned drv5_jobs;
    uint16_t toe_duty;          /* park hold PWM duty (permil), TOE31 / PL6 */
    bool wdt_interval;          /* last plat_wdt_mode() state */
    bool userboot_ok;
    bool fwe;                   /* FPMON.FWE */
    unsigned flasher_starts;
    bool flash_enable;
    bool fwe_follows_enable;

    /* ---- rebuild platform model (design §5) ---- */
    unsigned ms;                /* plat_tick_count() returns this (the test advances it) */
    uint32_t frt_us;            /* plat_frt_us() model value */
    bool rspi_launch_fail;      /* plat_rspi_launch: bus busy (all buses) */
    bool lab_fail;              /* plat_rspi_lab: bus timeout */
    unsigned walktmr_starts;    /* plat_walktmr_start() calls */
} stub_plat_t;

extern stub_plat_t stub;

/* The fake RSPI job engine arms a transaction on plat_rspi_launch(); the TEST
 * plays the interrupt with stub_rspi_isr() — one call per frame fed, the last
 * call runs the completion handler (ISR context) and may queue the next job.
 * Scripted replies: sup_reply_fn(tx) (default sup_xfer_rx), framed in the
 * 0xA8/0xAA class by select parity on bus 1, 0xA2 on bus 0. */
void stub_rspi_isr(uint8_t bus);
bool stub_rspi_busy(uint8_t bus);

#endif
