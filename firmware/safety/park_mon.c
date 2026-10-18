#include "safety/park_mon.h"

/* Own copies of the limits (not taken from the application headers on purpose). */
#define MON_STILL_RPM       80
#define MON_ENGINE_RPM      400
#define MON_TARGET_CYCLES   2u
#define MON_CONTACT_CYCLES  20u         /* 200 ms */

static uint8_t s_faults, s_faults_inv;
static uint8_t s_target_cnt, s_contact_cnt;
static bool s_prev_release, s_prev_hold;

static void latch(uint8_t f)
{
    s_faults = (uint8_t)(s_faults | f);
    s_faults_inv = (uint8_t)~s_faults;
}

void park_mon_init(void)
{
    s_faults = 0u;
    s_faults_inv = 0xFFu;
    s_target_cnt = 0u;
    s_contact_cnt = 0u;
    s_prev_release = false;
    s_prev_hold = false;
}

static bool own_moving(const park_mon_in_t *in)
{
    const int32_t v = in->output_speed < 0 ? -(int32_t)in->output_speed : (int32_t)in->output_speed;
    return !in->speed_ok || v > MON_STILL_RPM;
}

/* Engage = P selected, or stationary with a park request or without a drive range. */
static bool own_engage(const park_mon_in_t *in)
{
    const bool sel_p = in->lever_ok && in->lever == RANGE_P;
    const bool sel_drive = in->lever_ok &&
                           (in->lever == RANGE_D || in->lever == RANGE_N || in->lever == RANGE_R);

    return sel_p || (!own_moving(in) && (in->park_request || !sel_drive));
}

static bool count(uint8_t *c, bool cond, uint8_t limit)
{
    *c = cond ? (uint8_t)(*c < 0xFFu ? *c + 1u : *c) : 0u;
    return *c > limit;
}

void park_mon_step(const park_mon_in_t *in)
{
    const bool moving = own_moving(in);
    const bool engine = in->engine_ok && in->engine_speed >= MON_ENGINE_RPM;
    const bool engage = in->target == PARK_ENGAGE;
    const bool c_in_p = in->contacts_ok && in->contacts == PARK_IN_P;
    const bool c_out = in->contacts_ok && in->contacts == PARK_NOT_P;

    if (s_faults != (uint8_t)~s_faults_inv || !in->app_integrity_ok) {
        latch(MON_F_INTEGRITY);
    }
    if (s_faults != 0u) {
        return;                     /* latched: outputs are off, keep the first cause */
    }
    if (count(&s_target_cnt, engage != own_engage(in), MON_TARGET_CYCLES)) {
        latch(MON_F_TARGET);
    }
    /* a release out of P (hold off); release together with hold is the re-latch after a reset */
    if (in->out.release && !in->out.hold && !s_prev_release && (moving || !engine)) {
        latch(MON_F_RELEASE);
    }
    if (!in->out.hold && s_prev_hold && moving) {
        latch(MON_F_HOLD_DROP);
    }
    if (count(&s_contact_cnt, (in->state == PC_IN_P && c_out) || (in->state == PC_HOLDING && c_in_p),
              MON_CONTACT_CYCLES)) {
        latch(MON_F_CONTACTS);
    }
    s_prev_release = in->out.release;
    s_prev_hold = in->out.hold;
}

bool park_mon_enable(void)
{
    return s_faults == 0u && s_faults_inv == 0xFFu;
}

uint8_t park_mon_faults(void)
{
    return s_faults;
}
