/* READ-ONLY park position sensor (2026-11-16): this translation unit is
 * an output path — the sensor headers refuse to compile here (and the
 * ctest pksens_readonly_scan fails if this file names a sensor cell) */
#define TCU_OUTPUT_PATH 1
#include "safety/park_mon.h"

/* Independent plausibility check of the chain's range (park_mon.h has the
 * rules and their evidence grades). Written from the raw inputs; it shares
 * no code and no cell with app/rng_chain.c. */

static uint8_t s_faults, s_faults_inv;
static uint8_t s_prev;          /* the range on the previous pass */
static uint8_t s_eng_on;        /* own engine state (430 / 380 / 25 passes) */
static uint8_t s_eng_low;       /* consecutive passes at or below 380 rpm (or invalid) */

static void latch(uint8_t f)
{
    s_faults = (uint8_t)(s_faults | f);
    s_faults_inv = (uint8_t)~s_faults;
}

void park_mon_init(void)
{
    s_faults = 0u;
    s_faults_inv = 0xFFu;
    s_prev = 8u;                /* the boot range is P (the init arbiter's 8) */
    s_eng_on = 0u;
    s_eng_low = 0u;
}

/* own engine hysteresis on the raw 0x108 speed (invalid = 0 rpm) */
static void engine_step(const park_mon_in_t *in)
{
    const uint16_t rpm = in->engine_ok ? in->engine_rpm : 0u;

    if (rpm > MON_ENG_ON_RPM) {
        s_eng_on = 1u;
        s_eng_low = 0u;
    } else if (rpm > MON_ENG_OFF_RPM) {
        s_eng_low = 0u;             /* between the limits: the state holds */
    } else if (s_eng_on != 0u) {
        if (s_eng_low < 0xFFu) {
            s_eng_low++;
        }
        if (s_eng_low >= MON_ENG_OFF_PASSES) {
            s_eng_on = 0u;
        }
    }
}

/* the brake interlock from the raw signals (the D607 law, re-written) */
static bool brake_holds(const park_mon_in_t *in)
{
    if (in->brake_ok) {
        return in->brake_on;
    }
    return in->speed14b_ok && (uint16_t)(in->speed14b_raw / 5u) > MON_BRAKE_SPEED;
}

static bool range_valid(uint8_t t, uint8_t tc)
{
    return (t == 5u || t == 6u || t == 7u || t == 8u) && tc == (uint8_t)~t;
}

void park_mon_step(const park_mon_in_t *in)
{
    engine_step(in);
    if (s_faults != (uint8_t)~s_faults_inv || !in->law_integrity_ok) {
        latch(MON_F_INTEGRITY);
    }
    if (!range_valid(in->target, in->target_c)) {
        latch(MON_F_RANGE);
    } else if (s_prev == 8u && in->target != 8u) {
        /* the transition out of P */
        if (!in->lever_ok || in->lever != in->target) {
            latch(MON_F_LEVER);
        }
        if (!brake_holds(in)) {
            latch(MON_F_BRAKE);
        }
        if (s_eng_on == 0u) {
            latch(MON_F_ENGINE);
        }
    }
    s_prev = in->target;
}

bool park_mon_enable(void)
{
    return s_faults == 0u && s_faults_inv == 0xFFu;
}

uint8_t park_mon_faults(void)
{
    return s_faults;
}

bool park_mon_engine_running(void)
{
    return s_eng_on != 0u;
}
