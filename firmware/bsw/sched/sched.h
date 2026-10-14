/* Cooperative scheduler on the 1 ms tick (architecture §6). */
#ifndef TCU_SCHED_H
#define TCU_SCHED_H

#include "tcu/units.h"

typedef void (*task_fn)(void);

typedef struct {
    task_fn  fn;
    uint16_t period_ms;   /* 1, 10, 100 ...                                */
    uint16_t offset_ms;   /* phase within the period, spreads the load     */
} task_t;

typedef struct {
    ms_t     now_ms;          /* ticks since start                          */
    uint32_t missed_ticks;    /* ticks that arrived while a cycle was busy  */
} sched_status_t;

void sched_init(const task_t *tasks, uint8_t count);

/* Run all tasks due at the current tick. Call once per tick. */
void sched_tick(void);

/* Note that a tick was missed (detected by the platform). */
void sched_note_missed(uint32_t n);

const sched_status_t *sched_status(void);

#endif
