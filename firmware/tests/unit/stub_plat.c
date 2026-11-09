/* Platform stubs for unit tests: capture transmitted frames, resets and hand-overs. */
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

/* The supervisor shadow cells (sup_cells.h) live at the stock's RAM addresses
 * (0xFFF80000..0xFFF96000, design §9/§10). On the host the test maps that
 * window so the cells are ordinary writable memory; the target pins them with
 * the linker's .sup_* sections instead. */
static void stub_cells_map(void) __attribute__((constructor));
static void stub_cells_map(void)
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
        perror("stub: cell window mmap failed");
        exit(1);
    }
    memset(p, 0, len);
}

stub_plat_t stub;

bool plat_can_rx_register(can_bus_t bus, uint16_t id)
{
    (void)bus;
    (void)id;
    return true;
}

bool plat_can_send(can_bus_t bus, const can_frame_t *f)
{
    if (stub.n_tx >= STUB_TX_MAX) {
        return false;
    }
    stub.tx_bus[stub.n_tx] = bus;
    stub.tx[stub.n_tx++] = *f;
    return true;
}

void plat_wdt_service(void) { }

void plat_reset(void)
{
    stub.resets++;
}

bool plat_flash_fwe(void)
{
    return stub.fwe;
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
    stub.flash_enable = on;
    if (on && stub.fwe_follows_enable) {
        stub.fwe = true;
    }
    return false;
}

void plat_flasher_start(void)
{
    stub.flasher_starts++;
}

void plat_boot_handover(const uint8_t *rec, uint8_t len)
{
    memcpy(stub.rec, rec, len);
    stub.handovers++;
}

bool plat_mem_read(uint32_t addr, uint8_t *out, uint16_t n)
{
    if (addr < 0xFFF80000u || addr - 0xFFF80000u + n > sizeof stub.ram) {
        return false;
    }
    memcpy(out, &stub.ram[addr - 0xFFF80000u], n);
    return true;
}

bool plat_adc_raw(uint8_t an, uint16_t *raw)
{
    if (an >= 47u || !stub.adc_on[an]) {
        return false;
    }
    *raw = stub.adc[an];
    return true;
}

bool plat_speed_read(uint8_t ch, speed_raw_t *r)
{
    if (ch >= SPEED_CHANNELS || !stub.speed_on[ch]) {
        return false;
    }
    *r = stub.speed[ch];
    return true;
}

void plat_sup_init(void) { stub.sup_n = 0u; }

static uint16_t sup_reply_word(uint16_t tx)
{
    return stub.sup_reply_fn ? stub.sup_reply_fn(tx) : stub.sup_xfer_rx;
}

bool plat_sup_xfer(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    if (stub.sup_xfer_fail) {
        return false;
    }
    for (uint8_t i = 0u; i < n; i++) {
        rx[i] = sup_reply_word(tx[i]);
        if (!stub.sup_busy && stub.sup_n < 64u * 16u) {
            /* ring record: the app-pattern tick sends 8 frames per 5 ms pass */
            stub.sup_tx[stub.sup_n % 64u] = tx[i];
        }
        if (!stub.sup_busy) {
            stub.sup_n++;
        }
    }
    return true;
}

bool plat_sup_burst(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    if (stub.sup_xfer_fail) {
        return false;
    }
    for (uint8_t i = 0u; i < n; i++) {
        rx[i] = sup_reply_word(tx[i]);
        if (!stub.sup_busy) {
            stub.sup_tx[stub.sup_n % 64u] = tx[i];
            stub.sup_n++;
        }
    }
    return true;
}

bool plat_sup_xfer5(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    /* Five-select rotation: select = word index mod 5 (SSLB3/2/1/0/5). The
     * supervisor is select 2 (0xA8-class); the other chips answer 0xAA-class. */
    static const uint16_t cls[5] = { 0xAA00u, 0xAA00u, 0xA800u, 0xAA00u, 0xAA00u };
    if (stub.sup_xfer_fail) {
        return false;
    }
    for (uint8_t i = 0u; i < n; i++) {
        rx[i] = (uint16_t)(cls[i % 5u] | (stub.sup_reply_fn ? stub.sup_reply_fn(tx[i]) & 0xFFu : 0u));
        if (!stub.sup_busy) {
            stub.sup_tx[stub.sup_n % 64u] = tx[i];
            stub.sup_n++;
        }
    }
    stub.drv5_jobs++;
    return true;
}

bool plat_sup_xfer4(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    if (stub.sup_xfer_fail) {
        return false;
    }
    for (uint8_t i = 0u; i < n; i++) {
        rx[i] = (uint16_t)(0xAA00u | (stub.sup_reply_fn ? stub.sup_reply_fn(tx[i]) & 0xFFu : 0u));
        if (!stub.sup_busy) {
            stub.sup_tx[stub.sup_n % 64u] = tx[i];
            stub.sup_n++;
        }
    }
    stub.drv5_jobs++;
    return true;
}

bool plat_sup_burst2(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    if (stub.sup_xfer_fail) {
        return false;
    }
    for (uint8_t i = 0u; i < n; i++) {
        rx[i] = sup_reply_word(tx[i]);
        if (!stub.sup_busy) {
            stub.sup_tx[stub.sup_n % 64u] = tx[i];
            stub.sup_n++;
        }
    }
    return true;
}

bool plat_sup_burst0(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    if (stub.sup_xfer_fail) {
        return false;
    }
    for (uint8_t i = 0u; i < n; i++) {
        rx[i] = sup_reply_word(tx[i]);
        if (!stub.sup_busy) {
            stub.sup_tx[stub.sup_n % 64u] = tx[i];
            stub.sup_n++;
        }
    }
    return true;
}

void plat_wdt_mode(bool interval)
{
    stub.wdt_interval = interval;
}

uint8_t plat_sup_exchange(uint16_t tx, uint16_t *rx)
{
    uint8_t r = 0u;
    if (stub.sup_n != 0u) {
        *rx = stub.sup_reply;
        r |= 2u;
    }
    if (!stub.sup_busy) {
        stub.sup_tx[stub.sup_n % 64u] = tx;     /* ring: long tests keep running */
        stub.sup_n++;
        r |= 1u;
    }
    return r;
}

/* Simulated driver chips: writes (bit 7) store the data byte(s) per command; reads return the
 * register written by the paired write command, with the header the stock handlers check. */
static uint16_t drv_read(uint8_t chip, uint8_t cmd)
{
    switch (cmd) {
    case 0x28: return stub.drv_reg[chip][0x88];
    case 0x30: return stub.drv_reg[chip][0x90];
    case 0x2E: return stub.drv_reg[chip][0x8E];
    case 0x36: return stub.drv_reg[chip][0x96];
    case 0x60: return stub.drv_reg[chip][0xA0];
    case 0xC0: return (uint16_t)(stub.drv_reg[chip][0xE8] | stub.drv_reg[chip][0xE9] << 8);
    case 0x66: return stub.drv_reg[chip][0xA6];
    case 0xD8: return (uint16_t)(stub.drv_reg[chip][0xF0] | stub.drv_reg[chip][0xF1] << 8);
    case 0x06: return 0x12;
    default:   return 0;
    }
}

void plat_drv_init(void) { }
void plat_drv_spbr_set(uint8_t spbr) { (void)spbr; }

/* Park hold PWM output: the stub records the requested duty so tests can assert it. */
void plat_toe_out_init(void)
{
    stub.toe_duty = 0u;
}

void plat_toe_out_set(uint16_t permil)
{
    stub.toe_duty = (permil > 1000u) ? 1000u : permil;
}

bool plat_drv_xfer(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    if (stub.drv_fail) {
        return false;
    }
    stub.drv_jobs++;
    for (uint8_t i = 0u; i < n; i++) {
        const uint8_t chip = (uint8_t)(i % 5u), cmd = (uint8_t)(tx[i] >> 8);
        if (n == 45u && (i / 5u == 2u || i / 5u == 4u)) {      /* 11-bit setpoints */
            const uint8_t reg = i / 5u == 2u ? 0xE8u : 0xF0u;
            stub.drv_reg[chip][reg] = (uint8_t)tx[i];
            stub.drv_reg[chip][reg + 1u] = (uint8_t)((tx[i] >> 8) & 0x07u);
        } else if ((cmd & 0x80u) != 0u) {
            stub.drv_reg[chip][cmd] = (uint8_t)tx[i];
        }
        if (i < 45u) {
            stub.drv_last_tx[i] = tx[i];
        }
    }
    for (uint8_t i = 0u; i < n; i++) {
        const uint8_t chip = (uint8_t)(i % 5u), cmd = (uint8_t)(tx[i] >> 8);
        uint16_t v = drv_read(chip, cmd);
        if (stub.drv_bad_readback) {
            v ^= 0x01u;
        }
        rx[i] = n == 45u ? (uint16_t)(0xA000u | (v & 0x7FFu)) : (uint16_t)(0xA200u | (v & 0xFFu));
        if (stub.drv_bad_header) {
            rx[i] = 0x0000u;
        }
    }
    return true;
}

bool plat_userboot_copy(uint8_t *dst, uint32_t len)
{
    if (!stub.userboot_ok) {
        return false;
    }
    for (uint32_t i = 0u; i < len; i++) {
        dst[i] = (uint8_t)(i * 7u + 3u);        /* recognisable pattern */
    }
    return true;
}

/* boot-mailbox stubs (spec 03 section 6.2): zero status, no machine */
void plat_mb_capture(void) { }
void plat_mb_tick(void) { }
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
bool plat_drv_xfer_irq(const uint16_t *tx, uint16_t *rx, uint8_t n) { (void)tx; (void)rx; (void)n; return true; }
void plat_drv_irq_arm(bool on) { (void)on; }

/* ── rebuild platform model (design §5) ──────────────────────────────────── */

/* The fake RSPI job engine: launch arms, pump starts the feed, and the TEST
 * plays the RX ISR (stub_rspi_isr) frame by frame; the final call runs the
 * completion handler, which may queue and pump the next job. */

typedef struct {
    bool     armed;         /* staged, waiting for pump */
    bool     feeding;       /* on the wire, ISR collects */
    uint8_t  frames;
    uint8_t  n_sel;
    uint8_t  job_id;
    uint8_t  idx;           /* next frame to feed */
    uint16_t *tx;
    uint16_t *rx;
    rspi_done_fn done;
} stub_rspi_t;

/* the scripted reply payload (low byte) per transmitted word */
static uint16_t rspi_scripted_value(uint16_t tx)
{
    return sup_reply_word(tx);
}

static stub_rspi_t s_rs[2];
static uint32_t s_rs_isr[2];

void plat_rspi_init(void)
{
    memset(s_rs, 0, sizeof s_rs);
}

static uint16_t rspi_scripted(uint8_t bus, uint16_t tx, uint8_t i, uint8_t n_sel)
{
    uint16_t cls;

    if (bus == 0u) {
        cls = 0xA200u;                              /* driver chips: A2 class */
    } else if (n_sel == 1u && tx == 0x00FFu) {
        cls = 0xB400u;                              /* SSLB2 */
    } else if (n_sel == 1u) {
        cls = 0xA800u;                              /* single-select: supervisor */
    } else {
        cls = (i & 1u) == 0u ? 0xA800u : 0xAA00u;   /* alternating B1/B0 */
    }
    return (uint16_t)(cls | (rspi_scripted_value(tx) & 0x00FFu));
}

bool plat_rspi_launch(uint8_t bus, uint8_t job_id, uint16_t *tx, uint16_t *rx,
                      uint8_t frames, const uint16_t *sel_cfg, uint8_t n_sel,
                      rspi_done_fn done)
{
    stub_rspi_t *c = &s_rs[bus];

    (void)sel_cfg;
    if (stub.rspi_launch_fail || c->armed || c->feeding || frames == 0u) {
        return false;
    }
    c->tx = tx;
    c->rx = rx;
    c->frames = frames;
    c->n_sel = n_sel;
    c->job_id = job_id;
    c->idx = 0u;
    c->done = done;
    c->armed = true;
    return true;
}

void plat_rspi_pump(uint8_t bus)
{
    stub_rspi_t *c = &s_rs[bus];

    if (c->armed) {
        c->armed = false;
        c->feeding = true;
        c->idx = 1u;                    /* frame 0 is fed by the pump itself */
    }
}

bool plat_rspi_busy(uint8_t bus)
{
    return s_rs[bus].armed || s_rs[bus].feeding;
}

uint32_t plat_rspi_isr_count(uint8_t bus)
{
    return s_rs_isr[bus];
}

/* the RX ISR body: collect the reply of the frame that just completed, feed
 * the next one; at burst end disarm and run the completion handler */
void stub_rspi_isr(uint8_t bus)
{
    stub_rspi_t *c = &s_rs[bus];

    if (!c->feeding) {
        return;
    }
    s_rs_isr[bus]++;
    c->rx[c->idx - 1u] = rspi_scripted(bus, c->tx[c->idx - 1u], (uint8_t)(c->idx - 1u), c->n_sel);
    if (c->idx < c->frames) {
        c->idx++;
        return;
    }
    c->feeding = false;
    if (c->done != NULL) {
        c->done(bus, c->job_id, c->rx, c->frames);
    }
}

bool stub_rspi_busy(uint8_t bus)
{
    return plat_rspi_busy(bus);
}

/* blocking lab path (FE12/FE13): completes immediately from the script */
bool plat_rspi_lab(uint8_t bus, const uint16_t *tx, uint16_t *rx,
                   uint8_t frames, const uint16_t *sel_cfg, uint8_t n_sel,
                   uint32_t timeout_ms)
{
    (void)bus;
    (void)sel_cfg;
    (void)timeout_ms;
    if (stub.lab_fail) {
        return false;
    }
    for (uint8_t i = 0u; i < frames; i++) {
        rx[i] = rspi_scripted(bus, tx[i], i, n_sel);
    }
    return true;
}

uint8_t plat_drv_spbr_get(void)
{
    return 5u;
}

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

void plat_tick_start(void)
{
}

uint32_t plat_tick_count(void)
{
    return stub.ms;
}

/* ---- walk timer model ---- */
static bool s_walk_run;

void plat_walktmr_start(void)
{
    s_walk_run = true;
    stub.walktmr_starts++;
}

void plat_walktmr_stop(void)
{
    s_walk_run = false;
}

bool plat_walktmr_running(void)
{
    return s_walk_run;
}

uint32_t plat_walktmr_count(void)
{
    return stub.frt_us;             /* advances with the test's clock */
}

/* ---- WDT model ---- */
void plat_wdt_kick(void)
{
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
    return stub.wdt_interval;
}

void plat_wdt_quick_reset(void)
{
    stub.resets++;
    exit(1);
}

/* ---- FRT microsecond model (the test advances stub.frt_us) ---- */
uint32_t plat_frt_us(void)
{
    return stub.frt_us;
}
