/* The neutral transmit seam (vehicle_tx.h), Nissan implementation: the stub (nissan_tx.c).
 * Included by vehicle_tx.h only. Nothing is transmitted. */
#ifndef TCU_VEHICLE_TX_NISSAN_H
#define TCU_VEHICLE_TX_NISSAN_H

#include "io/vehicle/vehicle_tx.h"
#include "io/vehicle/nissan_tx.h"
#include "bsw/com/com.h"

#if !defined(TCU_VEHICLE_NISSAN) || TCU_VEHICLE_NISSAN != 1
#error "vehicle_tx_nissan.h: the Nissan vehicle transmit seam - built only with TCU_VEHICLE=NISSAN (defines TCU_VEHICLE_NISSAN=1)"
#endif

static inline void vehicle_tx_init(void)
{
    nissan_tx_init();
}

static inline void vehicle_tx_tick_1ms(void)
{
    nissan_tx_tick_1ms();
}

static inline void vehicle_tx_set(const vehicle_out_t *o)
{
    (void)o;                 /* TODO(capture): no Nissan message is built yet */
}

static inline bool vehicle_tx_enable_torque(bool on)
{
    return nissan_tx_enable(on);     /* always false */
}

static inline bool vehicle_hw_register(void)
{
    return com_register_hw();
}

#endif
