/* Range and Park control (milestone M4a): park target, actuator sequencing, independent
 * monitor and the output gate. docs/firmware/01-m4-range-park.md.
 *
 * M4a computes and publishes the park release / park hold requests but drives nothing: the
 * gate in park.c has no output path, and building with TCU_M4B_OUTPUTS stops the build.
 */
#ifndef TCU_PARK_H
#define TCU_PARK_H

#include "app/park_ctl.h"
#include "safety/park_mon.h"

#define PARK_ENGINE_RUNNING_RPM 400     /* [I] line pressure available */
#define PARK_SUPPLY_MIN_MV      9000u

typedef struct {
    park_target_t  target;
    range_reason_t reason;
    park_state_t   state;
    park_out_t     req;                 /* requested outputs after the gate's enable check */
    uint8_t        ctl_faults;          /* PC_F_* */
    uint8_t        mon_faults;          /* MON_F_* */
} park_status_t;

void park_init(void);
/* Call every 10 ms after vehicle_update_10ms() and sensors_update_10ms(). */
void park_tick_10ms(void);
const park_status_t *park_status(void);

#endif
