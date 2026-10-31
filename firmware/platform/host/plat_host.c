/* Host (PC) platform: real-time 1 ms tick, CAN frames printed to stdout.
 * Environment:
 *   TCU_SIM_MS     run time in milliseconds before exit (default 1000)
 *   TCU_SIM_CANLOG candump -L log to replay ("(sec.usec) can0 108#0011..."); can0 = bus 1,
 *                  can1 = bus 2; timestamps are taken relative to the first line */
#define _POSIX_C_SOURCE 199309L
#include "platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static struct timespec s_next;
static uint32_t s_ticks, s_limit = 1000u;
static uint32_t s_idle;
static FILE *s_log;
static double s_log_t0 = -1.0;
static can_bus_t s_pend_bus;
static can_frame_t s_pend;
static double s_pend_t;
static bool s_pend_valid;

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
    e = getenv("TCU_SIM_CANLOG");
    if (e != NULL && (s_log = fopen(e, "r")) == NULL) {
        perror(e);
        exit(1);
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

bool plat_can_rx_register(can_bus_t bus, uint16_t id)
{
    (void)bus;
    (void)id;
    return true;            /* the replay delivers every frame; the receive layer filters */
}

/* Read the next frame of the replay log into s_pend. */
static void log_next(void)
{
    char line[256], ifname[32], payload[64];
    double t;
    unsigned id;

    s_pend_valid = false;
    while (s_log != NULL && fgets(line, sizeof line, s_log) != NULL) {
        char *hash;
        size_t n;

        if (sscanf(line, " (%lf) %31s %63s", &t, ifname, payload) != 3 ||
            (hash = strchr(payload, '#')) == NULL || sscanf(payload, "%x#", &id) != 1) {
            continue;
        }
        n = strlen(hash + 1) / 2u;
        if (n > 8u || id > 0x7FFu) {
            continue;
        }
        if (s_log_t0 < 0.0) {
            s_log_t0 = t;
        }
        s_pend_t = (t - s_log_t0) * 1000.0;
        s_pend_bus = strcmp(ifname, "can1") == 0 ? CAN_BUS2 : CAN_BUS1;
        s_pend.id = (uint16_t)id;
        s_pend.dlc = (uint8_t)n;
        for (size_t i = 0u; i < n; i++) {
            unsigned v;
            sscanf(hash + 1 + 2u * i, "%2x", &v);
            s_pend.data[i] = (uint8_t)v;
        }
        s_pend_valid = true;
        return;
    }
}

bool plat_can_recv(can_bus_t bus, can_frame_t *f)
{
    if (!s_pend_valid) {
        log_next();
    }
    if (!s_pend_valid || s_pend_bus != bus || s_pend_t > (double)s_ticks) {
        return false;
    }
    *f = s_pend;
    s_pend_valid = false;
    return true;
}

bool plat_adc_raw(uint8_t an, uint16_t *raw)
{
    (void)an;
    (void)raw;
    return false;           /* no analog inputs on the host */
}

bool plat_speed_read(uint8_t ch, speed_raw_t *r)
{
    (void)ch;
    (void)r;
    return false;
}

static uint16_t s_sup_last;
static bool s_sup_pending;

void plat_sup_init(void) { s_sup_pending = false; }

uint8_t plat_sup_exchange(uint16_t tx, uint16_t *rx)
{
    uint8_t r = 0u;
    if (s_sup_pending) {
        *rx = (uint16_t)(0xA800u | (s_sup_last & 0x00FFu));   /* simulated reply: echo */
        r |= 2u;
    }
    s_sup_last = tx;
    s_sup_pending = true;
    return (uint8_t)(r | 1u);
}

/* Stock-job transport (spec 03 section 4.4): even frames to SSLB1 (the supervisor,
 * 0xA8 reply class), odd frames to SSLB0 (0xAA class). The supervisor echoes the last
 * 0x8E control byte back through the 0x22 read, as the stock handler 0x92D94 expects. */
bool plat_sup_xfer(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    static uint16_t ctl;

    for (uint8_t i = 0u; i < n; i++) {
        if ((tx[i] & 0xFF00u) == 0x8E00u) {
            ctl = tx[i];
        }
        if ((i & 1u) == 0u) {
            rx[i] = (tx[i] & 0xFF00u) == 0x2200u ? (uint16_t)(0xA800u | (ctl & 0x00FFu))
                                                 : (uint16_t)(0xA800u | (tx[i] & 0x00FFu));
        } else {
            rx[i] = (uint16_t)(0xAA00u | (tx[i] & 0x00FFu));
        }
    }
    return true;
}

/* Burst: all frames to SSLB1, echoing like the stock handler expects (0x22 read
 * returns the last 0x8E control byte). */
bool plat_sup_burst(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    static uint16_t ctl;

    for (uint8_t i = 0u; i < n; i++) {
        if ((tx[i] & 0xFF00u) == 0x8E00u) {
            ctl = tx[i];
        }
        rx[i] = (tx[i] & 0xFF00u) == 0x2200u ? (uint16_t)(0xA800u | (ctl & 0x00FFu))
                                             : (uint16_t)(0xA800u | (tx[i] & 0x00FFu));
    }
    return true;
}

bool plat_sup_burst2(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    for (uint8_t i = 0u; i < n; i++) {
        rx[i] = (uint16_t)(0xB400u | (tx[i] & 0x00FFu));   /* SSLB2 replies 0xB4xx */
    }
    return true;
}

void plat_wdt_mode(bool interval) { (void)interval; }

void plat_drv_init(void) { }

bool plat_drv_xfer(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    (void)tx;
    (void)rx;
    (void)n;
    return false;           /* no driver chips on the host: the driver layer reports a fault */
}

static uint16_t s_toe_duty;

void plat_toe_out_init(void)
{
    s_toe_duty = 0u;
}

void plat_toe_out_set(uint16_t permil)
{
    s_toe_duty = (permil > 1000u) ? 1000u : permil;
}

bool plat_mem_read(uint32_t addr, uint8_t *out, uint16_t n)
{
    (void)addr;
    (void)out;
    (void)n;
    return false;           /* no target memory on the host */
}

bool plat_userboot_copy(uint8_t *dst, uint32_t len)
{
    (void)dst;
    (void)len;
    return false;           /* no flash on the host */
}

void plat_reset(void)
{
    printf("[%6u ms] RESET\n", s_ticks);
    exit(0);
}

bool plat_flash_fwe(void)
{
    return true;
}

uint16_t plat_entry_snapshot(uint8_t *out, uint16_t max)
{
    (void)out;
    (void)max;
    return 0u;
}

uint16_t plat_clock_probe(uint8_t *out, uint16_t max)
{
    (void)out;
    (void)max;
    return 0u;
}

bool plat_flash_enable(bool on)
{
    (void)on;
    return false;
}

void plat_flasher_start(void)
{
    printf("[%6u ms] FLASHER START (the RAM flasher is simulated by tcu_flasher_sim)\n", s_ticks);
    exit(0);
}

void plat_boot_handover(const uint8_t *rec, uint8_t len)
{
    printf("[%6u ms] BOOT HANDOVER record:", s_ticks);
    for (uint8_t i = 0u; i < len; i++) {
        printf("%s%02X", i % 16u == 0u ? "\n    " : " ", rec[i]);
    }
    printf("\n");
    exit(0);
}

void plat_idle_count(void) { s_idle++; }

uint32_t plat_idle_take(void)
{
    uint32_t v = s_idle;
    s_idle = 0u;
    return v;
}
