/* Vehicle input interface (architecture: io layer).
 *
 * Every vehicle profile (FCA stock bus, later Nissan) fills the same structure in project
 * units. Application code reads only this structure, never CAN data.
 * Rows marked [I] in spec 07 section 7.1 are interpretations, to be confirmed on a vehicle.
 */
#ifndef TCU_VEHICLE_H
#define TCU_VEHICLE_H

#include "platform/platform.h"

/* Range request codes, same numbering as the stock software (spec 14 section 5a). */
typedef enum {
    RANGE_NONE = 0,
    RANGE_D = 5,
    RANGE_N = 6,
    RANGE_R = 7,
    RANGE_P = 8
} range_req_t;

typedef enum { LEVER_SRC_PRIMARY = 0, LEVER_SRC_FALLBACK, LEVER_SRC_NONE } lever_src_t;

typedef enum { BRAKE_OFF = 0, BRAKE_ON, BRAKE_ERROR, BRAKE_NA } brake_t;

typedef struct {
    rpm_t     engine_speed;     quality_t q_engine_speed;
    nm10_t    torque_actual;    quality_t q_torque_actual;   /* T1, [I] actual engine torque */
    nm10_t    torque_t2;        quality_t q_torque_t2;       /* role open (spec 07 7.1)      */
    nm10_t    torque_t3;        quality_t q_torque_t3;       /* role open                    */
    permil_t  pedal;            quality_t q_pedal;           /* [I] accelerator pedal        */
    degc10_t  engine_temp;      quality_t q_engine_temp;     /* [I] coolant                  */
    brake_t   brake;            quality_t q_brake;           /* [I]                          */
    uint8_t   engine_mode;      quality_t q_engine_mode;     /* [I] raw code 0..14           */
    uint16_t  park_speed_raw;   quality_t q_park_speed;      /* 0x14B, unit open             */
    uint32_t  soak_s;           quality_t q_soak;            /* [I] engine-off time          */
    uint8_t   park_request;     quality_t q_park_request;    /* [I] raw code; 1..3 = active  */

    range_req_t lever;          quality_t q_lever;
    bool        lever_flag;     /* lever code 5: D with extra flag ([I] manual range)        */
    lever_src_t lever_src;
} vehicle_in_t;

/* Profile interface. */
void vehicle_init(void);
/* Feed a received frame (all buses). */
void vehicle_rx(can_bus_t bus, const can_frame_t *f);
/* Age messages and decode signals; call every 10 ms. */
void vehicle_update_10ms(void);
const vehicle_in_t *vehicle_in(void);

#endif
