/* The neutral transmit seam (vehicle_tx.h), FCA implementation: forwards to the FCA transmit
 * set (fca_tx.c) and the FCA receive registration (com.c). Included by vehicle_tx.h only.
 *
 * static inline on purpose: the forwarders compile to exactly the calls main.c made to
 * fca_tx_* / com_register_hw before the seam existed (the RAM application is at its size
 * limit and every SH image stays byte-identical). A fork replaces this header's include
 * in vehicle_tx.h with its own. */
#ifndef TCU_VEHICLE_TX_FCA_H
#define TCU_VEHICLE_TX_FCA_H

#include "io/vehicle/vehicle_tx.h"
#include "io/vehicle/fca_tx.h"
#include "bsw/com/com.h"

#include <stddef.h>

/* TCU_VEHICLE switch (CMakeLists.txt, ~/tcu_sweep/fork_seams_port.md) */
#if !defined(TCU_VEHICLE_FCA) || TCU_VEHICLE_FCA != 1
#error "vehicle_tx_fca.h: the FCA vehicle transmit seam - built only with TCU_VEHICLE=FCA (defines TCU_VEHICLE_FCA=1)"
#endif

static inline void vehicle_tx_init(void)
{
    fca_tx_init();
}

static inline void vehicle_tx_tick_1ms(void)
{
    fca_tx_tick_1ms();
}

static inline void vehicle_tx_set(const vehicle_out_t *o)
{
    if (o != NULL && o->torque_request_valid) {
        fca_tx_set(FCA_F_037_B0_4, o->torque_request_raw[0]);
        fca_tx_set(FCA_F_037_B2_4, o->torque_request_raw[1]);
        fca_tx_set(FCA_F_037_B4_4, o->torque_request_raw[2]);
        fca_tx_set(FCA_F_2F9_B0_4, o->torque_request_raw[0]);
        fca_tx_set(FCA_F_2F9_B2_4, o->torque_request_raw[1]);
        fca_tx_set(FCA_F_2F9_B4_4, o->torque_request_raw[2]);
    }
}

static inline bool vehicle_tx_enable_torque(bool on)
{
    const bool a = fca_tx_enable(FCA_M_037, on);
    const bool b = fca_tx_enable(FCA_M_2F9, on);

    return a && b;
}

static inline bool vehicle_hw_register(void)
{
    return com_register_hw();
}

#endif
