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
} stub_plat_t;

extern stub_plat_t stub;

#endif
