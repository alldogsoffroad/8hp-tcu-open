/* The neutral transmit seam (vehicle_tx.h), Nissan implementation: nissan_tx.c (0x174 / 0x176 /
 * 0x177). Included by vehicle_tx.h only. Every frame starts disabled. */
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

void nissan_out_tick_1ms(void);     /* io/vehicle/nissan_out_gather.c (the code-flash application) */

static inline void vehicle_tx_tick_1ms(void)
{
#if defined(__sh__) && !defined(TCU_RAM_APP)
    nissan_out_tick_1ms();          /* the application: the TCU state -> the frames (D-N5) */
#endif
    nissan_tx_tick_1ms();
}

static inline void vehicle_tx_set(const vehicle_out_t *o)
{
    nissan_tx_set(o);
}

static inline bool vehicle_tx_enable_torque(bool on)
{
    return nissan_tx_enable(on);     /* refused until drivetrain data was set */
}

static inline bool vehicle_hw_register(void)
{
    return com_register_hw();
}

#endif
