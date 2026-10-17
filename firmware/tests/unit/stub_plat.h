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
    bool drv_fail;              /* driver bus timeout */
    bool drv_bad_header;        /* chips reply with a wrong header */
    bool drv_bad_readback;      /* chips return wrong read-back values */
    uint8_t drv_reg[5][256];    /* simulated chip registers, by write command */
    uint16_t drv_last_tx[45];
    unsigned drv_jobs;
    bool userboot_ok;
} stub_plat_t;

extern stub_plat_t stub;

#endif
