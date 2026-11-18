/* Range and Park (milestone M4): the range D6D6 comes from the stock's
 * cyclic chain (app/rng_chain.c, the live owner since 2026-11-17; the init
 * arbiter app/range.c runs once inside it); this module runs the stock's
 * output law from D6D6 to the two park demands (app/park_law.c: 0xB912C /
 * 0xB97F8 → 0xCFFDE → 0xE88FA → 0xE8888) and the independent plausibility
 * check of the range (safety/park_mon.c, ours).
 * ~/tcu_sweep/range_logic_port.md, range_chain_live_port.md. D-M4-6
 * (2026-11-14): the invented M4a sequencing (park_ctl.c) is retired.
 *
 * The two demands leave this module ONLY through the slot getters of
 * bsw/drv/drv_park.h (park_slot7_release → request law ch 7, park_slot11_hold
 * → group-A slot 11), which the drivers bound to {0, 10000} and gate with
 * DRV_GA_STAGE (default of tcu_app.elf: 1 = follow this logic; 0 = hold both
 * at 0). There is no setter.
 */
#ifndef TCU_PARK_H
#define TCU_PARK_H

/* THE GARAGE SHADOW IS READ-ONLY (app/gs_shadow.c defines TCU_GS_SHADOW_TU):
 * it may not include a live output header */
#if defined(TCU_GS_SHADOW_TU)
#error "the garage shadow is read-only: it may not include a live output header (park.h)"
#endif

#include "app/range.h"
#include "app/park_law.h"
#include "safety/park_mon.h"

typedef struct {
    uint8_t  lever;                     /* the lever request (5..8, 0 none) */
    park_target_t target;               /* 0xFFF8D6D6 (8 when torn / out of range) */
    uint8_t  reason;                    /* 0xFFF8D6D9, the chain's reason code */
    uint8_t  state;                     /* timer model 0xFFF8BC70[0] */
    uint8_t  request;                   /* 0xFFF8AE29 park request */
    uint8_t  lock;                      /* 0xFFF8BC70[1] */
    bool     release, hold;             /* the demands as published (after the monitor) */
    uint8_t  mon_faults;                /* MON_F_* */
} park_status_t;

void park_init(void);
/* Call every 10 ms after vehicle_update_10ms() and rng_chain_tick_10ms(). */
void park_tick_10ms(void);
/* the OS mode (sup_mode) for the fault-reaction park request 0x89CF2
 * (its input 0x89C3C = the mode-3 flag); 1 after park_init — call before
 * park_tick_10ms */
void park_set_os_mode(uint8_t mode);
const park_status_t *park_status(void);

#endif
