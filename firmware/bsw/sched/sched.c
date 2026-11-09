/* sched.c — the cooperative superloop scheduler (design §6, spec 24 §1.2/1.4).
 *
 * Mirrors the stock's architecture: the 1 ms tick ISR fills per-type rings;
 * the superloop drains them. No RTOS, flat tables, overrun instrumentation.
 */
#include "bsw/sched/sched.h"
#include <string.h>

typedef struct {
    uint8_t buf[RING_SLOTS];
    uint8_t head, tail, count, overflow;
} ring_t;

static ring_t s_rings[RT_COUNT];
static const sched_ent_t *s_tbl;
static uint8_t s_n;
static volatile uint32_t s_now_ms;
static sched_status_t s_status;

void sched_init(const sched_ent_t *tbl, uint8_t n)
{
    memset(s_rings, 0, sizeof s_rings);
    memset(&s_status, 0, sizeof s_status);
    s_tbl = tbl;
    s_n = n;
    s_now_ms = 0;
}

void sched_tick_isr(void)
{
    s_now_ms++;
    s_status.tick_count++;
    for (uint8_t i = 0u; i < s_n; i++) {
        const sched_ent_t *e = &s_tbl[i];
        if (e->period_ms == 0u) {
            continue;
        }
        /* wrap-free phase compare: (now - phase) % period fires spuriously in
         * the first period when period divides 2^32-1 (e.g. 5 ms) */
        if (s_now_ms % e->period_ms == (uint32_t)e->phase_ms % e->period_ms) {
            ring_t *r = &s_rings[e->ring];
            if (r->count < RING_SLOTS) {
                r->buf[r->head] = i;
                r->head = (uint8_t)((r->head + 1u) % RING_SLOTS);
                r->count++;
            } else {
                r->overflow++;
                s_status.missed_ticks++;
            }
        }
    }
}

void sched_drain(void)
{
    for (uint8_t rt = 0u; rt < (uint8_t)RT_COUNT; rt++) {
        ring_t *r = &s_rings[rt];
        while (r->count > 0u) {
            uint8_t idx = r->buf[r->tail];
            r->tail = (uint8_t)((r->tail + 1u) % RING_SLOTS);
            r->count--;
            if (idx < s_n) {
                s_tbl[idx].fn();
            }
        }
    }
}

const sched_status_t *sched_status(void)
{
    return &s_status;
}

uint32_t sched_now_ms(void)
{
    return s_now_ms;
}
