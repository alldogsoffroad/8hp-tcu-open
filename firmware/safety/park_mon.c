#include "safety/park_mon.h"

/* Own copies of the limits (not taken from the application headers on
 * purpose; the stock's safety layer reads its own calibration copy at
 * 0xFFF85350). Values = the stock calibration (0x195FD4..). */
#define MON_STILL_RPM       80u
#define MON_FAST_SPEED      320u
#define MON_TARGET_CYCLES   2u

static uint8_t s_faults, s_faults_inv;
static uint8_t s_target_cnt;

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
}

/* The recompute (written independently of app/range.c, same rule set as
 * the stock's 0x158C60 / 0x158D86 at the stock calibration: special row 1
 * disabled, rows 2/3/5/6/8 enabled, mode 4). */
uint8_t park_mon_target(const park_mon_in_t *in)
{
    const bool lever_drive = in->lever >= 5u && in->lever <= 7u;
    const bool moving = in->out_speed > MON_STILL_RPM;

    /* drive special cases at speed (speed2 >= / > 320) */
    if (in->speed2 >= MON_FAST_SPEED && in->park_req == 0u && in->lever2 == 5u &&
        in->status == 1u) {
        return in->dir != 2u ? 5u : 6u;
    }
    if (in->speed2 > MON_FAST_SPEED) {
        if (in->fast == 1u || (in->lever == 0u && in->dir == 0u)) {
            return 6u;
        }
        if (in->lever == 5u && in->dir == 1u) {
            return 5u;
        }
    }
    /* mode 4 */
    if (in->lever == 8u) {
        return 8u;
    }
    if (!moving && in->park_req == 1u) {
        return 8u;
    }
    return (lever_drive || moving) ? 6u : 8u;
}

static bool count(uint8_t *c, bool cond, uint8_t limit)
{
    *c = cond ? (uint8_t)(*c < 0xFFu ? *c + 1u : *c) : 0u;
    return *c > limit;
}

void park_mon_step(const park_mon_in_t *in)
{
    if (s_faults != (uint8_t)~s_faults_inv || !in->app_integrity_ok) {
        latch(MON_F_INTEGRITY);
    }
    if (s_faults != 0u) {
        return;                     /* latched: outputs are off, keep the first cause */
    }
    if (count(&s_target_cnt, in->target != park_mon_target(in), MON_TARGET_CYCLES)) {
        latch(MON_F_TARGET);
    }
}

bool park_mon_enable(void)
{
    return s_faults == 0u && s_faults_inv == 0xFFu;
}

uint8_t park_mon_faults(void)
{
    return s_faults;
}
