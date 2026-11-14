/* Independent park-target cross-check (D-M4-6: kept, trimmed to the stock's
 * rule). The stock's safety layer recomputes the park target with its own
 * code and calibration copy (0x158C3E → 0x158C60 / 0x158D86, spec 14 §6)
 * and compares it with the application's 0xFFF8D6D6 (getter 0xA7B78). This
 * module does the same: its own copy of the limits and its own code for
 * the arbiter's rules (special rows, forced releases, mode 4), from the raw
 * inputs, compared with the published target every 10 ms.
 *
 * Ours (not stock): a disagreement for more than MON_TARGET_CYCLES
 * consecutive cycles, or a broken complement in the park cells, LATCHES a
 * fault until power-off and removes the output enable — app/park.c then
 * forces both park demands to 0. (The stock reports the mismatch; its
 * reaction beyond the report is not decoded.) The old release / hold /
 * contact rules (M4a) were project inventions and are removed.
 */
#ifndef TCU_PARK_MON_H
#define TCU_PARK_MON_H

#include <stdint.h>
#include <stdbool.h>

enum {
    MON_F_TARGET    = 0x01u,        /* own target differs for more than 2 cycles      */
    MON_F_INTEGRITY = 0x10u         /* corrupt copy in the application or the monitor */
};

typedef struct {
    /* raw inputs, the monitor's own copies */
    uint8_t  lever;     /* lever request 5..8 (0 none) */
    uint8_t  lever2, dir, fast, status;
    uint16_t out_speed; /* rpm, unsigned (as the arbiter) */
    uint16_t speed2;    /* D650 */
    uint8_t  park_req;  /* D5F8 */
    /* the application's result */
    uint8_t  target;    /* 0xFFF8D6D6 as published */
    bool     app_integrity_ok;
} park_mon_in_t;

void park_mon_init(void);
void park_mon_step(const park_mon_in_t *in);
uint8_t park_mon_target(const park_mon_in_t *in);   /* the monitor's own recompute */
bool park_mon_enable(void);
uint8_t park_mon_faults(void);

#endif
