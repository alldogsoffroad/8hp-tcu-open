#include "sched.h"

static const task_t *s_tasks;
static uint8_t s_count;
static sched_status_t s_status;

void sched_init(const task_t *tasks, uint8_t count)
{
    s_tasks = tasks;
    s_count = count;
    s_status.now_ms = 0u;
    s_status.missed_ticks = 0u;
}

void sched_tick(void)
{
    for (uint8_t i = 0u; i < s_count; i++) {
        const task_t *t = &s_tasks[i];
        if (t->period_ms != 0u &&
            (s_status.now_ms % t->period_ms) == (uint32_t)(t->offset_ms % t->period_ms)) {
            t->fn();
        }
    }
    s_status.now_ms++;
}

void sched_note_missed(uint32_t n)
{
    s_status.missed_ticks += n;
}

const sched_status_t *sched_status(void)
{
    return &s_status;
}
