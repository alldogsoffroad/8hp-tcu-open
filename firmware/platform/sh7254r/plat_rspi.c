/* plat_rspi.c — interrupt-per-frame RSPI engine for both buses (design §5.5, A1).
 *
 * Replaces the polled plat_sup_xfer/burst/plat_drv_xfer family. The mainline
 * stages a job and opens the interrupt; the RX ISR (v445 bus A / v449 bus B)
 * collects each reply and feeds the next frame until the burst ends, then
 * disarms and calls the completion handler in ISR context.
 *
 * SPCR discipline (stock-faithful): idle = 0x08 (MSTR only), armed = 0xC8
 * (SPRIE+SPE+MSTR), ISR ends every burst with SPCR = 0x08.
 * SPCMD registers are reloaded from the caller's table per transaction.
 */
#include "platform.h"
#include "regs.h"
#include <string.h>

/* ── per-bus state ─────────────────────────────────────────────────────── */

typedef enum { CH_IDLE = 0, CH_ARMED, CH_FEEDING } rspi_state_t;

typedef struct {
    uint8_t bus;                    /* 0=A, 1=B */
    volatile rspi_state_t state;
    volatile uint8_t idx;           /* next frame to feed */
    uint8_t frames;
    uint8_t n_sel;
    uint16_t *tx;                   /* staged TX (caller-owned) */
    uint16_t *rx;                   /* reply buffer (caller-owned) */
    const uint16_t *sel_cfg;        /* SPCMD values for this transaction */
    rspi_done_fn done;
    uint8_t job_id;
    uint32_t isr_count;             /* ISR entries (P8 diagnostic) */
} rspi_chan_t;

static rspi_chan_t s_chan[2];       /* [0]=bus A, [1]=bus B */

/* ── register accessors (bus-relative) ─────────────────────────────────── */

static volatile uint8_t *spcr(uint8_t bus)
{
    return (volatile uint8_t *)(bus ? 0xFFFFB800u : 0xFFFFB000u);
}
static volatile uint8_t *spsr(uint8_t bus)
{
    return (volatile uint8_t *)(bus ? 0xFFFFB803u : 0xFFFFB003u);
}
static volatile uint16_t *spdr(uint8_t bus)
{
    return (volatile uint16_t *)(bus ? 0xFFFFB804u : 0xFFFFB004u);
}
static volatile uint8_t *spscr(uint8_t bus)
{
    return (volatile uint8_t *)(bus ? 0xFFFFB808u : 0xFFFFB008u);
}
static volatile uint8_t *spbr(uint8_t bus)
{
    return (volatile uint8_t *)(bus ? 0xFFFFB80Au : 0xFFFFB00Au);
}
static volatile uint16_t *spcmd(uint8_t bus, uint8_t n)
{
    return (volatile uint16_t *)((bus ? 0xFFFFB810u : 0xFFFFB010u) + 2u * n);
}

/* ── launch (task context; caller holds SR if the model is windowed) ──── */

bool plat_rspi_launch(uint8_t bus, uint8_t job_id, uint16_t *tx, uint16_t *rx,
                      uint8_t frames, const uint16_t *sel_cfg, uint8_t n_sel,
                      rspi_done_fn done)
{
    rspi_chan_t *c = &s_chan[bus];
    if (c->state != CH_IDLE || frames == 0u || n_sel == 0u) {
        return false;                           /* busy or invalid */
    }
    c->tx = tx;
    c->rx = rx;
    c->frames = frames;
    c->n_sel = n_sel;
    c->sel_cfg = sel_cfg;
    c->done = done;
    c->job_id = job_id;
    c->idx = 0u;
    c->state = CH_ARMED;
    return true;
}

/* ── pump: start the highest-priority staged job (task or init context) ── */

void plat_rspi_pump(uint8_t bus)
{
    rspi_chan_t *c = &s_chan[bus];
    if (c->state != CH_ARMED) {
        return;                                 /* nothing to start */
    }

    /* reload SPCMD registers from the transaction's table */
    for (uint8_t i = 0u; i < c->n_sel && i < 8u; i++) {
        *spcmd(bus, i) = c->sel_cfg[i];
    }
    /* frame count, bit rate, timing */
    *spscr(bus) = (uint8_t)(c->frames - 1u);
    *spbr(bus) = bus ? 5u : plat_drv_spbr_get(); /* B: Pφ/12=1.667MHz, A: proven */

    /* arm the interrupt: SPCR = 0xC8 (SPRIE + SPE + MSTR) */
    *spcr(bus) = 0xC8u;

    /* feed frame 0 in the E60-proven order: WAIT for SPTEF first — an
     * SPDR write while the TX buffer is full is silently dropped and the
     * burst never clocks (old plat_sh7254r.c:694) */
    volatile uint8_t *sr = spsr(bus);
    volatile uint16_t *dr = spdr(bus);
    uint32_t guard;
    for (guard = 20000u; guard != 0u && (*sr & SPSR_SPTEF) == 0u; guard--) {
    }
    if (guard == 0u) {
        *spcr(bus) = 0x08u;                     /* can't start: disarm */
        return;
    }
    (void)*sr;                                  /* SPTEF read */
    *dr = c->tx[0];                             /* SPDR write */
    (void)*sr;                                  /* SPSR read */
    *sr = 0u;                                   /* SPSR clear */

    c->idx = 1u;
    c->state = CH_FEEDING;
}

bool plat_rspi_busy(uint8_t bus)
{
    return s_chan[bus].state != CH_IDLE;
}

uint32_t plat_rspi_isr_count(uint8_t bus)
{
    return s_chan[bus].isr_count;
}

/* ── the RX engine: vector entry / shared body / polled fallback ──────────
 * r01e: the INTC fabric is suspect until proven, so the engine runs three
 * ways — the true vector entry (counts s_vec_count), the shared body, and
 * a superloop poll that harvests SPRF directly. The counters tell the
 * bench which path carried the traffic. */

volatile uint32_t s_vec_count[2];   /* TRUE vector deliveries (diagnostic) */

static void plat_rspi_isr_body(uint8_t bus)
{
    rspi_chan_t *c = &s_chan[bus];
    c->isr_count++;

    /* the stock's entry protocol (0x96590 decode): clock-gate acknowledge
     * on every ISR entry — 0xFFFE6030 = 0xF3. The gate read closed on the
     * r01/r01b bench (live 0x0000); the stock never lets it stay that way. */
    volatile uint8_t *const gate = (volatile uint8_t *)0xFFFE6030u;
    (void)*gate;
    *gate = 0xF3u;

    if (c->state != CH_FEEDING) {
        return;                                 /* spurious */
    }

    /* collect the reply of the frame that just completed (off-by-one law:
     * the reply to frame k is available at the k-1th interrupt — the
     * E60 lesson; first frame's reply available after feeding frame 1) */
    c->rx[c->idx - 1u] = *spdr(bus);

    if (c->idx < c->frames) {
        /* feed the next frame (unconditional-feed order — E56 shape) */
        volatile uint8_t *sr = spsr(bus);
        volatile uint16_t *dr = spdr(bus);
        (void)*sr;
        *dr = c->tx[c->idx];
        (void)*sr;
        *sr = 0u;
        c->idx++;
    } else {
        /* burst end: disarm, hand the replies to the completion handler */
        *spcr(bus) = 0x08u;                     /* idle = MSTR only */
        c->state = CH_IDLE;
        c->idx = 0u;
        if (c->done) {
            c->done(bus, c->job_id, c->rx, c->frames);
        }
        /* the done handler may queue the next job; pump it now for
         * gapless chaining (the stock's DMA engine behaves the same) */
        plat_rspi_pump(bus);
    }
}

/* the TRUE interrupt entry (vector 445 bus A / 449 bus B) */
void plat_rspi_isr(uint8_t bus)
{
    if (bus < 2u) {
        s_vec_count[bus]++;
    }
    plat_rspi_isr_body(bus);
}

/* superloop fallback: harvest a pending reply flag directly. Returns true
 * if the body ran. Safe with a live interrupt: both paths are guarded by
 * the channel state and the flag clears on the first harvest. */
bool plat_rspi_poll(uint8_t bus)
{
    if (bus >= 2u || s_chan[bus].state != CH_FEEDING) {
        return false;
    }
    if ((*spsr(bus) & SPSR_SPRF) == 0u) {
        return false;
    }
    plat_rspi_isr_body(bus);
    return true;
}

/* ── lab/debug: single transaction with timeout (blocking, diagnostic only) ── */

bool plat_rspi_lab(uint8_t bus, const uint16_t *tx, uint16_t *rx,
                   uint8_t frames, const uint16_t *sel_cfg, uint8_t n_sel,
                   uint32_t timeout_ms)
{
    if (!plat_rspi_launch(bus, 0xFFu, (uint16_t *)tx, rx,
                          frames, sel_cfg, n_sel, NULL)) {
        return false;
    }
    plat_rspi_pump(bus);
    /* busy-wait with a timeout (diagnostic context only) */
    rspi_chan_t *c = &s_chan[bus];
    uint32_t t0 = plat_tick_count();
    while (c->state != CH_IDLE) {
        if (plat_tick_count() - t0 > timeout_ms) {
            /* abort: disarm */
            *spcr(bus) = 0x08u;
            c->state = CH_IDLE;
            return false;
        }
    }
    return true;
}

/* ── init ────────────────────────────────────────────────────────────────── */

void plat_rspi_init(void)
{
    memset(s_chan, 0, sizeof s_chan);
    s_chan[0].bus = 0u;
    s_chan[1].bus = 1u;
    /* the clock gate the stock's ISR acknowledges on every entry
     * (0x96590 decode; read closed live on the r01/r01b bench) */
    volatile uint8_t *const gate = (volatile uint8_t *)0xFFFE6030u;
    (void)*gate;
    *gate = 0xF3u;
    /* both buses idle = MSTR only (stock's live idle) */
    *spcr(0) = 0x08u;
    *spcr(1) = 0x08u;
}
