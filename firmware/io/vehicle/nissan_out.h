/* Nissan: the TCU state -> the transmit set's drivetrain fields (2026-11-23; decision D-N5: the
 * ECM is reflashed for 8 gears, so the TRUE 8HP gears are reported; the 0x177 torque cut follows
 * the 8HP torque-intervention amounts, user 2026-11-23).
 *
 * nissan_out_build() is pure (host-tested); nissan_out_tick_1ms() (nissan_out_gather.c, Nissan
 * application only) reads the read-only shadows and the sensor / vehicle inputs BY VALUE, builds
 * vehicle_out_t and hands it to the neutral vehicle_tx_set(). It is the torque-intervention
 * shadow's LIVE SWITCH for the Nissan build (docs/fca_finish_line.md: "live = transmit ... Nissan
 * 0x177"); the shadows and their hooks themselves stay read-only. No static RAM: the Nissan
 * image's data ends at the 4 KB stack limit.
 */
#ifndef TCU_NISSAN_OUT_H
#define TCU_NISSAN_OUT_H

#include "platform/platform.h"
#include "io/vehicle/vehicle_tx.h"

#if !defined(TCU_VEHICLE_NISSAN) || TCU_VEHICLE_NISSAN != 1
#error "nissan_out.h: Nissan only (TCU_VEHICLE=NISSAN)"
#endif

/* the 0x174 nibbles for the non-forward ranges (ROM 0x174 handler: 9 -> class 4 [I: R],
 * 0 / 10 -> class 3 [I: P/N]) */
#define NISSAN_GEAR_R  9u
#define NISSAN_GEAR_PN 10u

/* the sources, by value */
typedef struct {
    int8_t   engaged;        /* 8HP gear code: -1 R, 0 N, 1..8 (0x89C26 - 1) */
    int8_t   target;         /* 8HP gear code of the new gear (0x89C27) */
    uint16_t output_rpm;     /* 0 when not valid */
    uint16_t turbine_rpm;    /* 0 when not valid */
    bool     lockup;         /* the converter clutch is closed (the WK shadow) */
    uint8_t  ti_cls;         /* the TI shadow's class: 0 down, 1 up, 2 garage */
    uint8_t  ti_type;        /* 0 none / 1 increase / 2 reduction */
    bool     ti_armed;
    int16_t  ti_delta_nm;    /* the TI shadow's combined delta, Nm (negative = reduction) */
    int16_t  actual_nm10;    /* the ECM's actual torque (0x180), 0.1 Nm */
    bool     actual_ok;
} nissan_out_src_t;

uint8_t nissan_gear_nibble(int8_t code);          /* 1..8 as is, R 9, N/P 10 */
uint8_t nissan_cut_q8(const nissan_out_src_t *s); /* the 0x177 B4 fraction x 256, 0 = none */
void    nissan_out_build(const nissan_out_src_t *s, vehicle_out_t *o);
void    nissan_out_tick_1ms(void);                /* the Nissan application only (nissan_out_gather.c) */

#endif
