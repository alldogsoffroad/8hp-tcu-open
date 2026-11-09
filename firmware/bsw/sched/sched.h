/* sched.h — the cooperative scheduler (design §6). */
#ifndef TCU_SCHED_H
#define TCU_SCHED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ring types (per-type rings; the stock's 0xFFF943E8+8*type reduced to ours) */
enum { RT_1MS, RT_5MS, RT_10MS, RT_100MS, RT_EVENT, RT_COUNT };
#define RING_SLOTS 8

typedef void (*task_fn)(void);
typedef struct {
    uint16_t period_ms;
    uint16_t phase_ms;
    task_fn fn;
    uint8_t ring;                     /* RT_* */
} sched_ent_t;

typedef struct {
    uint32_t tick_count;
    uint32_t missed_ticks;
    uint32_t now_ms;
} sched_status_t;

void sched_init(const sched_ent_t *tbl, uint8_t n);
void sched_tick_isr(void);            /* from the 1 ms tick ISR */
void sched_drain(void);               /* from the superloop */
const sched_status_t *sched_status(void);
uint32_t sched_now_ms(void);

#endif
