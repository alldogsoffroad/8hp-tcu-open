/* Range and Park (milestone M4): the stock's park-target arbiter (app/range.c),
 * its output law with the lock decision, interlock and timer model
 * (app/park_law.c), and the independent cross-check (safety/park_mon.c).
 * ~/tcu_sweep/range_logic_port.md. D-M4-6 (2026-11-14): the invented M4a
 * sequencing (park_ctl.c: contact feedback, timeouts, engine precondition)
 * is retired.
 *
 * The two demands leave this module ONLY through the slot getters of
 * bsw/drv/drv_park.h (park_slot7_release → request law ch 7, park_slot11_hold
 * → group-A slot 11), which the drivers bound to {0, 10000} and gate with
 * DRV_GA_STAGE (default of tcu_app.elf: 1 = follow this logic; 0 = hold both
 * at 0). There is no setter.
 */
#ifndef TCU_PARK_H
#define TCU_PARK_H

/* READ-ONLY range shadow (app/rng_shadow.c): the shadow translation unit
 * defines TCU_RNG_SHADOW_TU and may not name a live cell */
#if defined(TCU_RNG_SHADOW_TU)
#error "the range shadow is read-only: it may not include a live cell header (park.h)"
#endif

#include "app/range.h"
#include "app/park_law.h"
#include "safety/park_mon.h"

typedef struct {
    uint8_t  lever;                     /* the lever request the arbiter saw */
    park_target_t target;               /* 0xFFF8D6D6 */
    uint8_t  reason;                    /* stock reason code */
    uint8_t  state;                     /* timer model 0xFFF8BC70[0] */
    uint8_t  request;                   /* 0xFFF8AE29 park request */
    uint8_t  lock;                      /* 0xFFF8BC70[1] */
    bool     release, hold;             /* the demands as published (after the monitor) */
    uint8_t  mon_faults;                /* MON_F_* */
} park_status_t;

void park_init(void);
/* Call every 10 ms after vehicle_update_10ms() and sensors_update_10ms(). */
void park_tick_10ms(void);
const park_status_t *park_status(void);

#endif
