#include "app/range.h"

static uint8_t s_target, s_target_inv;
static uint8_t s_reason, s_reason_inv;

bool range_stationary(rpm_t output_speed, bool speed_ok)
{
    return speed_ok && output_speed <= RANGE_STATIONARY_RPM && output_speed >= -RANGE_STATIONARY_RPM;
}

park_target_t range_decide(const range_in_t *in, range_reason_t *why)
{
    if (in->lever_ok && in->lever == RANGE_P) {
        *why = R_LEVER_P;
        return PARK_ENGAGE;
    }
    if (!range_stationary(in->output_speed, in->speed_ok)) {
        *why = R_MOVING;
        return PARK_RELEASE;
    }
    if (in->park_request) {
        *why = R_PARK_REQ;
        return PARK_ENGAGE;
    }
    if (in->lever_ok && (in->lever == RANGE_R || in->lever == RANGE_N || in->lever == RANGE_D)) {
        *why = R_LEVER_RND;
        return PARK_RELEASE;
    }
    *why = R_AUTO_P;
    return PARK_ENGAGE;
}

static void store(park_target_t t, range_reason_t why)
{
    s_target = (uint8_t)t;
    s_target_inv = (uint8_t)~s_target;
    s_reason = (uint8_t)why;
    s_reason_inv = (uint8_t)~s_reason;
}

void range_init(void)
{
    store(PARK_ENGAGE, R_NONE);
}

void range_update(const range_in_t *in)
{
    range_reason_t why;
    const park_target_t t = range_decide(in, &why);

    store(t, why);
}

bool range_integrity_ok(void)
{
    return s_target == (uint8_t)~s_target_inv && s_reason == (uint8_t)~s_reason_inv &&
           (s_target == (uint8_t)PARK_ENGAGE || s_target == (uint8_t)PARK_RELEASE);
}

park_target_t range_target(void)
{
    return range_integrity_ok() ? (park_target_t)s_target : PARK_ENGAGE;
}

range_reason_t range_reason(void)
{
    return range_integrity_ok() ? (range_reason_t)s_reason : R_NONE;
}

#ifdef PARK_TEST_HOOKS
void range_test_store(park_target_t t)
{
    store(t, R_NONE);
}

void range_test_corrupt(void)
{
    s_target_inv ^= 0x01u;
}
#endif
