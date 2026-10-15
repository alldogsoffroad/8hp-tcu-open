/* Shaft speed from the timer F rotation-speed captures (spec 02 section 4.1).
 *
 * Every 10 ms: if new edges arrived since the reference capture, speed = edges / time between
 * the reference edge and the latest edge. Without new edges the speed cannot exceed one edge
 * per "time since the last edge", so it decays toward zero (stock behaviour, margin 1.1);
 * after SPEED_STOP_MS without edges the shaft is reported stopped.
 * Result in 1/4 rpm:  speed_q = edges x 60 x 4 MHz x 4 / (ppr x ticks).
 */
#include "io/sensors/sensors.h"

#define SPEED_STOP_MS   1000u           /* no edge for 1 s: below 1 rpm at 60 pulses/rev */
#define SPEED_MAX_RPM   12000u          /* above this a sample is implausible */
#define TICKS_PER_MS    (SPEED_TICK_HZ / 1000u)

uint32_t speed_update(speed_state_t *s, const speed_raw_t *r, uint16_t ppr, bool *ok)
{
    /* 1/4 rpm for one edge per tick: 60 s x 4 MHz x 4 / ppr */
    const uint64_t k = (uint64_t)60u * SPEED_TICK_HZ * 4u / (ppr != 0u ? ppr : 1u);
    const uint16_t de = (uint16_t)(r->edges - s->ref_edges);
    uint32_t age;

    *ok = true;
    if (!s->started) {
        s->started = true;
        s->ref_edges = r->edges;
        s->ref_time = r->t_edge;
        s->speed_q = 0u;
        s->ms_since_edge = SPEED_STOP_MS;   /* unknown until two edges are seen */
        return 0u;
    }

    if (de != 0u) {
        const uint32_t dt = (r->t_edge - s->ref_time) & SPEED_TIME_MASK;
        const bool first_after_stop = s->ms_since_edge >= SPEED_STOP_MS;

        s->ref_edges = r->edges;
        s->ref_time = r->t_edge;
        s->ms_since_edge = 0u;
        if (first_after_stop || dt == 0u) {
            return s->speed_q;              /* no valid interval yet */
        }
        {
            const uint64_t q = ((uint64_t)de * k + dt / 2u) / dt;
            if (q > (uint64_t)SPEED_MAX_RPM * 4u) {
                *ok = false;                /* glitch / implausible: keep last value */
                return s->speed_q;
            }
            s->speed_q = (uint32_t)q;
        }
        return s->speed_q;
    }

    /* no new edge */
    s->ms_since_edge = s->ms_since_edge + 10u > SPEED_STOP_MS ? SPEED_STOP_MS : s->ms_since_edge + 10u;
    if (s->ms_since_edge >= SPEED_STOP_MS) {
        s->speed_q = 0u;
        return 0u;
    }
    /* time since the last edge: hardware age, but never less than our own count (the hardware
     * counter can wrap) */
    age = r->age;
    if (age < s->ms_since_edge * TICKS_PER_MS) {
        age = s->ms_since_edge * TICKS_PER_MS;
    }
    if (age != 0u) {
        const uint64_t bound = k * 11u / 10u / age;
        if (bound < s->speed_q) {
            s->speed_q = (uint32_t)bound;
        }
    }
    return s->speed_q;
}
