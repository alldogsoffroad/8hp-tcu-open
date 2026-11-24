/* NEUTRAL TRANSMIT SEAM (2026-11-20, ~/tcu_sweep/fork_prep2_port.md; the fork-readiness
 * audit ~/tcu_sweep/fork_readiness_audit.md §4.2).
 *
 * What the TCU sends to the vehicle, as the application sees it: no CAN id, no frame layout,
 * no profile header. app/main.c (and any other code outside io/vehicle/) calls ONLY these
 * functions; io/vehicle/vehicle_tx_fca.c forwards them to the FCA transmit set (fca_tx.c).
 * A fork adds its own implementation (a Nissan one) behind the same header.
 *
 * Fields: only what the FCA transmit set already carries. The FCA field MEANINGS are still
 * open (spec 08 section 5), so the struct offers the one decoded thing the transmit layer
 * itself guards: the three 13-bit torque request fields of the torque-request messages
 * (raw = Nm*4 + 2000), and (2026-11-23) the drivetrain state the Nissan engine controller
 * expects in 0x174 / 0x176 / 0x177. The gear indicator (0x421) is not sent: its sender is open
 * (decision D-N1).
 *
 * Every message starts disabled. The torque-request messages refuse to enable until every
 * torque field has been set (an all-zero field encodes -500 Nm): that guard lives in
 * fca_tx_enable and is kept; vehicle_tx_enable_torque() reports it.
 */
#ifndef TCU_VEHICLE_TX_H
#define TCU_VEHICLE_TX_H

#include "platform/platform.h"

typedef struct vehicle_out {
    uint16_t torque_request_raw[3];   /* the three 13-bit torque request fields (Nm*4+2000) */
    bool     torque_request_valid;    /* true: the fields above are written by vehicle_tx_set */
    /* drivetrain state for a vehicle whose engine controller expects transmission frames (the
     * Nissan set, 2026-11-23; docs/reference/nissan-ecm-tcm-interface.md). Unused by FCA. */
    uint16_t output_rpm;              /* output-shaft speed, 1 rpm                            */
    uint16_t turbine_rpm;             /* turbine speed, 1 rpm                                 */
    uint8_t  gear_engaged;            /* 1..8; 0 = no gear (P / N)                            */
    uint8_t  gear_target;             /* 1..8; equal to gear_engaged when no shift is pending */
    uint8_t  torque_cut_q8;           /* torque to remove during an upshift, fraction x 256   */
    bool     lockup;                  /* converter lock-up flag [I for the Nissan bits]       */
    bool     drivetrain_valid;        /* true: the drivetrain fields above are written        */
} vehicle_out_t;

/* The interface (implemented, static inline, by the selected vehicle's header below):
 *   void vehicle_tx_init(void);        initialise the transmit set (every message off, fields 0)
 *   void vehicle_tx_tick_1ms(void);    send due messages; call every 1 ms
 *   void vehicle_tx_set(const vehicle_out_t *o);
 *                                      hand over the application's outputs (typically every 10 ms)
 *   bool vehicle_tx_enable_torque(bool on);
 *                                      enable / disable the torque-request messages; enabling
 *                                      returns false (and leaves them off) until vehicle_tx_set()
 *                                      has written valid torque fields since vehicle_tx_init()
 *   bool vehicle_hw_register(void);    register the receive mailboxes of the selected vehicle
 *                                      profile with the CAN hardware */
#if defined(TCU_VEHICLE_FCA) && TCU_VEHICLE_FCA == 1
#include "io/vehicle/vehicle_tx_fca.h"
#elif defined(TCU_VEHICLE_NISSAN) && TCU_VEHICLE_NISSAN == 1
#include "io/vehicle/vehicle_tx_nissan.h"
#else
#error "vehicle_tx.h: no vehicle selected (CMake TCU_VEHICLE defines TCU_VEHICLE_<name>=1 for the targets that link a profile)"
#endif

#endif
