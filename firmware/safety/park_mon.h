/* Independent park monitor (milestone M4, docs/firmware/01-m4-range-park.md section 5).
 *
 * Recomputes the park target with its own code and limits (as the stock safety layer does,
 * spec 14 section 6) and checks the sequencing outputs against speed, engine and the park
 * contacts. Any fault is latched until power cycle and removes the output enable.
 * Called every 10 ms, after park_ctl_step().
 */
#ifndef TCU_PARK_MON_H
#define TCU_PARK_MON_H

#include "app/park_ctl.h"

enum {
    MON_F_TARGET    = 0x01u,        /* own target differs for more than 2 cycles      */
    MON_F_RELEASE   = 0x02u,        /* release started while moving or engine off     */
    MON_F_HOLD_DROP = 0x04u,        /* park hold dropped while moving                 */
    MON_F_CONTACTS  = 0x08u,        /* contacts disagree with the sequencing state    */
    MON_F_INTEGRITY = 0x10u         /* corrupt copy in the application or the monitor */
};

typedef struct {
    /* raw inputs */
    range_req_t lever;      bool lever_ok;
    rpm_t output_speed;     bool speed_ok;
    rpm_t engine_speed;     bool engine_ok;
    bool park_request;
    park_pos_t contacts;    bool contacts_ok;
    /* application results */
    park_target_t target;
    park_state_t state;
    park_out_t out;
    bool app_integrity_ok;
} park_mon_in_t;

void park_mon_init(void);
void park_mon_step(const park_mon_in_t *in);
bool park_mon_enable(void);
uint8_t park_mon_faults(void);

#endif
