/* Unit test: scheduler periods, phase offsets and ring drain (design §6). */
#include "bsw/sched/sched.h"

#include <stdio.h>

static unsigned n1, n5a, n5b, n10, n100;
static void t1(void) { n1++; }
static void t5a(void) { n5a++; }
static void t5b(void) { n5b++; }
static void t10(void) { n10++; }
static void t100(void) { n100++; }

/* the application's shape (app/main.c): per-type rings, phase offsets */
static const sched_ent_t tasks[] = {
    {    1u,  0u, t1,   RT_1MS   },
    {    5u,  2u, t5a,  RT_5MS   },
    {    5u,  3u, t5b,  RT_5MS   },
    {   10u,  1u, t10,  RT_10MS  },
    {  100u,  5u, t100, RT_100MS },
};

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void)
{
    sched_init(tasks, (uint8_t)(sizeof tasks / sizeof tasks[0]));
    for (unsigned i = 0u; i < 1000u; i++) {
        sched_tick_isr();               /* the 1 ms tick ISR fills the rings */
        sched_drain();                  /* the superloop drains them */
    }
    CHECK(n1 == 1000u);
    CHECK(n5a == 200u);                 /* phase 2: first fire at tick 2 */
    CHECK(n5b == 200u);                 /* phase 3: first fire at tick 3 */
    CHECK(n10 == 100u);                 /* phase 1: first fire at tick 1 */
    CHECK(n100 == 10u);                 /* phase 5: first fire at tick 5 */
    CHECK(sched_now_ms() == 1000u);
    CHECK(sched_status()->tick_count == 1000u);
    CHECK(sched_status()->missed_ticks == 0u);

    /* ring overrun: fill a ring beyond RING_SLOTS without draining — counted,
     * not crashed */
    sched_init(tasks, (uint8_t)(sizeof tasks / sizeof tasks[0]));
    for (unsigned i = 0u; i < 12u; i++) {
        sched_tick_isr();
    }
    CHECK(sched_status()->missed_ticks == 4u);   /* 9..12 overflow the 1 ms ring */
    sched_drain();

    if (fails == 0) {
        printf("test_sched: ok\n");
    }
    return fails != 0;
}
