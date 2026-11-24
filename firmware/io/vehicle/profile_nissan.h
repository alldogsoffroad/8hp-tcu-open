/* Nissan Frontier / Xterra vehicle profile: the Nissan-only decoded signals (2026-11-20).
 *
 * vehicle_in_t (vehicle.h) has no field for vehicle speed, wheel speeds or throttle position,
 * and a new field would move the FCA build's .bss. The confirmed Nissan signals that have no
 * vehicle_in_t home are therefore kept here, inside io/vehicle/, for the day a consumer needs
 * them (a vspeed_* field then joins vehicle_in_t with the FCA profile setting it, audit §4.1).
 * Nothing outside io/vehicle/ reads this header today.
 */
#ifndef TCU_PROFILE_NISSAN_H
#define TCU_PROFILE_NISSAN_H

#include "platform/platform.h"

#if !defined(TCU_VEHICLE_NISSAN) || TCU_VEHICLE_NISSAN != 1
#error "profile_nissan.h: the Nissan vehicle profile - built only with TCU_VEHICLE=NISSAN (defines TCU_VEHICLE_NISSAN=1)"
#endif

/* Field order is chosen to avoid padding: the Nissan SH image's data ends exactly at the 4 KB
 * stack limit (2026-11-23), so this struct and the profile's statics may not grow. The DBC's
 * 8-bit 0x160 byte-3 "throttle" was removed when the same signal became vehicle_in_t.pedal
 * (10-bit with its complement check). */
typedef struct {
    uint16_t  speed_mph100;       /* 0x280 bytes 4-5 BE, 0.01 mph               [CONFIRMED] */
    uint16_t  wheel_kmh100[4];    /* 0x284 b0-1 / b2-3, 0x285 b0-1 / b2-3, 0.01 km/h [CONFIRMED] */
    nm10_t    torque_demand;      /* 0x180 B3[3:0]:B4, raw x 5 - 4000 = 0.1 Nm: driver-demand torque,
                                     unchanged during a 0x177 B4 cut [O logs 2026-11-23] */
    quality_t q_speed;
    quality_t q_wheels;           /* valid only when BOTH 0x284 and 0x285 are fresh */
    quality_t q_torque_demand;    /* fresh 0x180, counter +1, raw != 0xFFF */
} nissan_in_t;

const nissan_in_t *nissan_in(void);

#endif
