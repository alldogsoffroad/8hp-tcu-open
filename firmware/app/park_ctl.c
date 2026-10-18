#include "app/park_ctl.h"

#define STEP_MS 10u

static const park_ctl_cfg_t *s_cfg;
static uint8_t s_state, s_state_inv;
static uint8_t s_faults;
static uint16_t s_t_ms;
static bool s_release_blocked;      /* after a failed release, until P is selected again */

static void set_state(park_state_t st)
{
    s_state = (uint8_t)st;
    s_state_inv = (uint8_t)~s_state;
    s_t_ms = 0u;
}

void park_ctl_init(const park_ctl_cfg_t *cfg)
{
    s_cfg = cfg;
    s_faults = 0u;
    s_release_blocked = false;
    set_state(PC_INIT);
}

bool park_ctl_integrity_ok(void)
{
    return s_state == (uint8_t)~s_state_inv && s_state <= (uint8_t)PC_SAFE;
}

park_state_t park_ctl_state(void)
{
    return park_ctl_integrity_ok() ? (park_state_t)s_state : PC_SAFE;
}

uint8_t park_ctl_faults(void)
{
    return s_faults;
}

void park_ctl_force_safe(void)
{
    s_faults |= PC_F_MONITOR;
    set_state(PC_SAFE);
}

static bool in_p(const park_ctl_in_t *in)
{
    return in->contacts_ok && in->contacts == PARK_IN_P;
}

static bool out_of_p(const park_ctl_in_t *in)
{
    return in->contacts_ok && in->contacts == PARK_NOT_P;
}

static void elapse(void)
{
    if (s_t_ms < 0xFFFFu - STEP_MS) {
        s_t_ms = (uint16_t)(s_t_ms + STEP_MS);
    }
}

void park_ctl_step(const park_ctl_in_t *in, park_out_t *out)
{
    out->release = false;
    out->hold = false;
    if (!park_ctl_integrity_ok()) {
        s_faults |= PC_F_INTEGRITY;
        set_state(PC_SAFE);
    }
    if (in->target == PARK_ENGAGE) {
        s_release_blocked = s_release_blocked && !in_p(in);
    }

    switch ((park_state_t)s_state) {
    case PC_INIT:
        if (in_p(in)) {
            set_state(PC_IN_P);
        } else if (in->engine_running || !in->stationary) {
            set_state(PC_HOLDING);      /* re-latch after a reset with the pawl out */
            out->hold = true;
            out->release = s_cfg->keep_release;
        } else {
            set_state(PC_ENGAGING);
        }
        break;

    case PC_IN_P:
        if (in->target == PARK_RELEASE && in->stationary && in->engine_running && in->supply_ok &&
            !s_release_blocked) {
            set_state(PC_RELEASING);
            out->release = true;
        }
        break;

    case PC_RELEASING:
        elapse();
        if (in->target == PARK_ENGAGE || !in->engine_running) {
            set_state(PC_ENGAGING);
        } else if (out_of_p(in)) {
            set_state(PC_HOLDING);
            out->hold = true;
            out->release = s_cfg->keep_release;
        } else if (s_t_ms >= s_cfg->release_timeout_ms) {
            s_faults |= PC_F_RELEASE_TIMEOUT;
            s_release_blocked = true;
            set_state(PC_ENGAGING);
        } else {
            out->release = true;
        }
        break;

    case PC_HOLDING:
        /* the hold stays on while moving, whatever the target */
        if (in->target == PARK_ENGAGE && in->stationary) {
            set_state(PC_ENGAGING);
        } else {
            out->hold = true;
            out->release = s_cfg->keep_release;
        }
        break;

    case PC_ENGAGING:
        elapse();
        if (in_p(in)) {
            set_state(PC_IN_P);
        } else if (s_t_ms >= s_cfg->engage_timeout_ms) {
            s_faults |= PC_F_ENGAGE_TIMEOUT;
            set_state(PC_SAFE);
        }
        break;

    case PC_SAFE:
    default:
        set_state(PC_SAFE);
        break;
    }
}

#ifdef PARK_TEST_HOOKS
void park_ctl_test_corrupt(void)
{
    s_state_inv ^= 0x01u;
}
#endif
