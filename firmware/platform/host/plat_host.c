/* Host (PC) platform: real-time 1 ms tick, CAN frames printed to stdout.
 * Environment: TCU_SIM_MS = run time in milliseconds before exit (default 1000). */
#define _POSIX_C_SOURCE 199309L
#include "platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static struct timespec s_next;
static uint32_t s_ticks, s_limit = 1000u;
static uint32_t s_idle;

static void add_ms(struct timespec *t)
{
    t->tv_nsec += 1000000L;
    if (t->tv_nsec >= 1000000000L) {
        t->tv_nsec -= 1000000000L;
        t->tv_sec++;
    }
}

void plat_init(void)
{
    const char *e = getenv("TCU_SIM_MS");
    if (e != NULL) {
        s_limit = (uint32_t)strtoul(e, NULL, 10);
    }
    clock_gettime(CLOCK_MONOTONIC, &s_next);
    add_ms(&s_next);
}

bool plat_tick_poll(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (now.tv_sec > s_next.tv_sec ||
        (now.tv_sec == s_next.tv_sec && now.tv_nsec >= s_next.tv_nsec)) {
        add_ms(&s_next);
        if (++s_ticks > s_limit) {
            exit(0);
        }
        return true;
    }
    return false;
}

void plat_wdt_service(void) { }

bool plat_can_send(can_bus_t bus, const can_frame_t *f)
{
    printf("[%6u ms] CAN%d %03X [%u]", s_ticks, (int)bus + 1, f->id, f->dlc);
    for (uint8_t i = 0u; i < f->dlc; i++) {
        printf(" %02X", f->data[i]);
    }
    printf("\n");
    return true;
}

void plat_idle_count(void) { s_idle++; }

uint32_t plat_idle_take(void)
{
    uint32_t v = s_idle;
    s_idle = 0u;
    return v;
}
