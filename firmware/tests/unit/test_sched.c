/* Unit test: scheduler periods and offsets. */
#include "bsw/sched/sched.h"

#include <stdio.h>

static unsigned n1, n10, n100;
static void t1(void) { n1++; }
static void t10(void) { n10++; }
static void t100(void) { n100++; }

static const task_t tasks[] = {
    { t1, 1u, 0u }, { t10, 10u, 3u }, { t100, 100u, 5u },
};

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void)
{
    sched_init(tasks, 3u);
    for (int i = 0; i < 1000; i++) {
        sched_tick();
    }
    CHECK(n1 == 1000u);
    CHECK(n10 == 100u);
    CHECK(n100 == 10u);
    CHECK(sched_status()->now_ms == 1000u);
    if (fails == 0) {
        printf("test_sched: ok\n");
    }
    return fails != 0;
}
