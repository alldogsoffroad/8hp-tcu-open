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
#include <sys/mman.h>
#include <time.h>

/* The supervisor shadow cells (sup_cells.h) live at the stock's RAM addresses
 * (0xFFF80000..0xFFF96000, design §9/§10). The sim maps that window so the
 * cells are ordinary writable memory; the target pins them with the linker's
 * .sup_* sections instead. */
static void sim_cells_map(void) __attribute__((constructor));
static void sim_cells_map(void)
{
    const uintptr_t base = 0xFFF80000u;
    const size_t len = 0x16000u;        /* 0xFFF80000..0xFFF96000 */
    void *p = mmap((void *)base, len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS
#ifdef MAP_FIXED_NOREPLACE
                   | MAP_FIXED_NOREPLACE
#else
                   | MAP_FIXED
#endif
                   , -1, 0);

    if (p == MAP_FAILED || (uintptr_t)p != base) {
        perror("sim: cell window mmap failed");
        exit(1);
    }
    memset(p, 0, len);
}

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

/* r01e fallback stubs: the host model is synchronous — the polls never
 * fire (the host tick rides plat_tick_poll above; RSPI replies complete
 * inside the stub's launch call) */
bool plat_tick_poll_hw(void) { return false; }
static bool s_walk_shot;
static uint32_t s_walk_fire_us;
extern void sup_walk_isr(void);
static uint32_t host_us(void);
bool plat_walktmr_poll(void)
{
    /* the one-shot model: fire once the armed time passes (no re-arm) */
    if (!s_walk_shot || (int32_t)(host_us() - s_walk_fire_us) < 0) {
        return false;
    }
    s_walk_shot = false;
    sup_walk_isr();
    return true;
}
bool plat_rspi_poll(uint8_t bus) { (void)bus; return false; }
/* no host ATU ISR: 0 keeps main.c's F2 gate on the CMT fallback, which is
 * the host's tick source */
uint32_t plat_tick_isr_count(void) { return 0u; }

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

/* ---- Rebuild platform model (design §5) -------------------------------- *
 * The target's tick/walk/RSPI engines are ISR-driven; the host has no
 * interrupts, so the 1 ms poll substitutes for the tick ISR and the RSPI
 * engine completes its transaction synchronously inside plat_rspi_pump(). */
#include "bsw/sched/sched.h"

void plat_tick_start(void)
{
    /* poll-driven on the host; the counter lives in plat_tick_poll */
}

uint32_t plat_tick_count(void)
{
    return s_ticks;
}

/* monotonic microsecond timestamp (the FRT model) */
static uint32_t host_us(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint32_t)((uint64_t)now.tv_sec * 1000000u + (uint64_t)now.tv_nsec / 1000u);
}

uint32_t plat_frt_us(void)
{
    return host_us();
}

/* ---- INTC / SR: no interrupt controller on the host ---- */
void plat_intc_setup(const plat_ipr_map_t *m)
{
    (void)m;
}

void plat_sr_open(void)
{
}

void plat_sr_close(void)
{
}

uint32_t plat_sr_i(void)
{
    return 0xEu;
}

void plat_sr_set(uint32_t level)
{
    (void)level;   /* host model: single-threaded, nothing to mask */
}

/* ---- Walk timer model: the walk's own clock (design §5.4) ---- */
static bool s_walk_run;

void plat_walktmr_start(void)
{
    s_walk_run = true;
}

void plat_walktmr_stop(void)
{
    s_walk_run = false;
    s_walk_shot = false;
}

/* B2 model: the counter's TRUE rate (q8 µs/tick; 0 → 1 µs/tick, the
 * reading the clock tree supports) and the target's arm law — calibrated
 * exactly at boot, (20 µs + 1/32) short. The transcript runs both 1 µs and 28 µs. */
uint32_t plat_walk_true_q8;
void plat_walktmr_calibrate(void)
{
}

void plat_walktmr_arm_us(uint32_t us)
{
    uint32_t q8 = plat_walk_true_q8 ? plat_walk_true_q8 : 256u;
    uint32_t aim = us > 20u + us / 32u ? us - 20u - us / 32u : 1u;
    uint32_t ticks = (aim * 256u) / q8;     /* the target's arm law */

    if (ticks == 0u) {
        ticks = 1u;
    }
    s_walk_run = true;
    s_walk_shot = true;
    s_walk_fire_us = host_us() + (uint32_t)(((uint64_t)ticks * q8) / 256u);
}

bool plat_walktmr_running(void)
{
    return s_walk_run;
}

uint32_t plat_walktmr_count(void)
{
    return host_us();           /* advances while "running" */
}

/* ---- WDT model ---- */
static bool s_wdt_interval;

void plat_wdt_kick(void)
{
}

static uint32_t s_boot_idx = 1u;
void plat_wdt_early_capture(void)
{
}
void plat_app_init(void)
{
}
void plat_adcmux_dma_global_init(void) {}
void plat_adcmux_timer_g_init(void) {}
void plat_adcmux_init(void) {}
void plat_adcmux_finish_5ms(void) {}
void plat_adcmux_status_5ms(void) {}
void plat_dei5_isr(void) {}
void plat_dei4_isr(void) {}
uint32_t plat_boot_index(void)
{
    return s_boot_idx;
}
uint32_t plat_boot_tcnta(void)
{
    return 0u;
}
void plat_alive(uint32_t code)
{
    (void)code;
}

void plat_wdt_capture_reset_cause(void)
{
}

uint8_t plat_wdt_wovf_count(void)
{
    return 0u;
}

bool plat_wdt_is_interval(void)
{
    return s_wdt_interval;
}

void plat_wdt_quick_reset(void)
{
    printf("[%6u ms] WDT QUICK RESET\n", s_ticks);
    exit(0);
}

/* ---- RSPI job engine model (design §5.5): completes inside pump() ---- */
typedef struct {
    bool     armed;
    uint8_t  frames;
    uint8_t  n_sel;
    uint8_t  job_id;
    const uint16_t *tx;
    uint16_t *rx;
    rspi_done_fn done;
} host_rspi_t;

static host_rspi_t s_hc[2];
static uint32_t s_hc_isr[2];

void plat_rspi_init(void)
{
    memset(s_hc, 0, sizeof s_hc);
}

/* Simulated reply per frame: bus B by select parity (SSLB1 = 0xA8 class even
 * frames of a two-command table, SSLB0 = 0xAA, SSLB2 = 0xB4); bus A 0xA2. */
static uint16_t host_reply(uint8_t bus, uint16_t tx, uint8_t i, uint8_t n_sel)
{
    uint16_t cls;
    if (bus == 0u) {
        cls = 0xA200u;
    } else if (n_sel == 1u && tx == 0x00FFu) {
        cls = 0xB400u;
    } else if (n_sel == 1u) {
        cls = 0xA800u;
    } else {
        cls = (i & 1u) == 0u ? 0xA800u : 0xAA00u;
    }
    return (uint16_t)(cls | (tx & 0x00FFu));
}

bool plat_rspi_launch(uint8_t bus, uint8_t job_id, uint16_t *tx, uint16_t *rx,
                      uint8_t frames, const uint16_t *sel_cfg, uint8_t n_sel,
                      rspi_done_fn done)
{
    host_rspi_t *c = &s_hc[bus];
    (void)sel_cfg;
    if (c->armed || frames == 0u) {
        return false;
    }
    c->tx = tx;
    c->rx = rx;
    c->frames = frames;
    c->n_sel = n_sel;
    c->job_id = job_id;
    c->done = done;
    c->armed = true;
    return true;
}

void plat_rspi_pump(uint8_t bus)
{
    host_rspi_t *c = &s_hc[bus];

    if (!c->armed) {
        return;
    }
    c->armed = false;
    s_hc_isr[bus] += c->frames;
    for (uint8_t i = 0u; i < c->frames; i++) {
        c->rx[i] = host_reply(bus, c->tx[i], i, c->n_sel);
    }
    if (c->done != NULL) {
        c->done(bus, c->job_id, c->rx, c->frames);
    }
}

bool plat_rspi_busy(uint8_t bus)
{
    return s_hc[bus].armed;
}

bool plat_rspi_owned(uint8_t bus)
{
    return plat_rspi_busy(bus);
}

bool plat_rspi_reap(uint8_t bus, uint32_t max_us)
{
    (void)bus;
    (void)max_us;
    return false;
}

uint32_t plat_rspi_err_count(uint8_t bus)
{
    (void)bus;
    return 0u;
}

uint32_t plat_rspi_isr_count(uint8_t bus)
{
    return s_hc_isr[bus];
}

bool plat_rspi_lab(uint8_t bus, const uint16_t *tx, uint16_t *rx,
                   uint8_t frames, const uint16_t *sel_cfg, uint8_t n_sel,
                   uint32_t timeout_ms)
{
    (void)bus;
    (void)timeout_ms;
    for (uint8_t i = 0u; i < frames; i++) {
        rx[i] = host_reply(1u, tx[i], i, n_sel);
    }
    return true;
}

uint8_t plat_drv_spbr_get(void)
{
    return 5u;
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

void plat_can_rx_commit(void)
{
    /* no mailbox layer on the host replay */
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

bool plat_sup_xfer5(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    /* five-select rotation: select 2 = the supervisor (0xA8-class), the rest 0xAA */
    static const uint16_t cls[5] = { 0xAA00u, 0xAA00u, 0xA800u, 0xAA00u, 0xAA00u };
    for (uint8_t i = 0u; i < n; i++) {
        rx[i] = (uint16_t)(cls[i % 5u] | (tx[i] & 0x00FFu));
    }
    return true;
}

bool plat_sup_xfer4(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    for (uint8_t i = 0u; i < n; i++) {
        rx[i] = (uint16_t)(0xAA00u | (tx[i] & 0x00FFu));
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

bool plat_sup_burst0(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    for (uint8_t i = 0u; i < n; i++) {
        rx[i] = (uint16_t)(0xAA00u | (tx[i] & 0x00FFu));   /* SSLB0 AA-class mirror */
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

/* ---- Mechatronic counterpart boot mailbox (spec 03 section 6.2): no hardware on host ---- */
void plat_mb_capture(void)
{
}

void plat_mb_tick(void)
{
}

void plat_stock_init_mirror(void)
{
}

bool plat_mb_poll_init(void)
{
    return false;
}

void plat_mb_tick_1ms(void)
{
}

const uint16_t *plat_mb_hist(uint16_t *n)
{
    if (n != NULL) {
        *n = 0u;
    }
    return NULL;
}

const plat_mb_status_t *plat_mb_status(void)
{
    static const plat_mb_status_t s_mb_zero;

    return &s_mb_zero;
}

void plat_drv_spbr_set(uint8_t spbr) { (void)spbr; }  /* host stub */

bool plat_drv_xfer_irq(const uint16_t *tx, uint16_t *rx, uint8_t n) { (void)tx; (void)rx; (void)n; return true; }
void plat_drv_irq_arm(bool on) { (void)on; }

void plat_walktmr_wait_until(uint32_t t)
{
    while ((int32_t)(t - host_us()) > 0) {
    }
}
