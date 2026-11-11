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

/* ── launch (any context: task, RX ISR at IPR 4, walk tick at IPR 6) ─────
 *
 * W1-WEDGE FIX (r01p bench): the registration is ONE indivisible trigger.
 * The stock's launcher (spi_job_dmac_launch 0x96468) arms the DMAC
 * descriptor and fires SPCR in one hardware-atomic sequence — the DMAC
 * either owns the channel or it does not. This CPU-fed port's equivalent
 * window is IDLE-check .. fields .. state=ARMED, and it must be equally
 * indivisible: launch_job runs from the RX-completion ISR (IPR 4) and the
 * batch/handler pumps (task), while the walk tick launches from IPR 6 —
 * ABOVE the RX ISR. Without the window closed, the walk tick can preempt
 * this launch AFTER the IDLE check but BEFORE the state=ARMED commit; its
 * own engine pump then reads state==IDLE, launches the next queued walk
 * job over this half-written registration, and on resume the pending
 * state=ARMED store re-arms the channel over the NEW burst. The resumed
 * launch still "succeeds": s_in_flight[id]=1 and USE[id]++ are committed
 * for a job whose channel registration was overwritten — its done handler
 * never runs, the in-flight flag never clears, and sup_job_check_free
 * refuses the builder forever. r01p ground truth: USE[JOB_DUTY]=1 frozen,
 * rx_duty untouched (the channel carried the OTHER job's rx buffer),
 * err[DUTY]=0, while LEVEL and every cyclic job kept cycling — the channel
 * is free, the one-in-flight flag is the wedge.
 *
 * The window is a dozen stores (µs); the walk tick's compare-match stays
 * pending and fires the moment SR.I is restored, so no tick is lost. The
 * pump after the registration is deliberately OUTSIDE the window: it
 * spins on SPTEF and every other context already sees state==ARMED/FEEDING
 * (busy) the instant the commit lands. */
bool plat_rspi_launch(uint8_t bus, uint8_t job_id, uint16_t *tx, uint16_t *rx,
                      uint8_t frames, const uint16_t *sel_cfg, uint8_t n_sel,
                      rspi_done_fn done)
{
    rspi_chan_t *c = &s_chan[bus];
    uint32_t sr_i = plat_sr_i();

    plat_sr_close();
    if (c->state != CH_IDLE || frames == 0u || n_sel == 0u) {
        plat_sr_set(sr_i);
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
    c->state = CH_ARMED;                        /* the commit point */
    plat_sr_set(sr_i);
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
    /* SPSCR = SPCMD sequence length - 1 (the SELECTS count), not frames-1:
     * the stock launcher 0x96468 writes SPSCR = descriptor byte 3 - 1
     * (selects) and the frame count lives in the DMA transfer — "the SPI
     * unit runs the command sequence repeatedly until the frame count is
     * reached" (spec 03 §1). With SPSCR = frames-1 a single-select multi-
     * frame burst cycled the sequence onto stale SPCMD1 (0xAF01 = SSLB0,
     * left by the two-select cyclic jobs) and virgin SPCMD2+ (0x0000 = no
     * select at all): frames 2+ of every W1/W2/EN/POS/FLT burst landed on
     * the wrong chip or nowhere. The two-select cyclic jobs (CTRL/STAT/ID,
     * frames = 2 x selects) alternated correctly only by that coincidence.
     * Stock 6.2az law: both frames of a one-select job go to its one select. */
    *spscr(bus) = (uint8_t)(c->n_sel - 1u);
    *spbr(bus) = bus ? 5u : plat_drv_spbr_get(); /* B: Pφ/12=1.667MHz, A: proven */

    /* clean any stale SPSR (the stock's ISR writes 0x20 first thing on
     * every entry — a trailing SPRF+OVRN from the last burst blocks the
     * next burst's RX interrupt requests: bus B carried SPSR=0xA0 forever
     * through r01g/r01h with zero vector deliveries while A delivered) */
    (void)*spsr(bus);
    *spsr(bus) = 0x20u;

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

    /* the stock's entry protocol (0x96590 decode): ADMATE0 acknowledge on
     * every entry, PER-BUS value (spi_job_dmac_launch 0x9655c/0x96580:
     * bus A writes 0xFC, bus B writes 0xF3 — each clears its own A-DMAC
     * channel end flags; 0xFFFE6030 IS ADMATE0, not a clock gate). */
    volatile uint8_t *const gate = (volatile uint8_t *)0xFFFE6030u;
    (void)*gate;
    *gate = bus ? 0xF3u : 0xFCu;

    if (c->state != CH_FEEDING) {
        return;                                 /* spurious */
    }

    /* collect the reply of the frame that just completed (off-by-one law:
     * the reply to frame k is available at the k-1th interrupt — the
     * E60 lesson; first frame's reply available after feeding frame 1) */
    c->rx[c->idx - 1u] = *spdr(bus);

    if (c->idx < c->frames) {
        /* feed the next frame. THE SPTEF WAIT IS MANDATORY on EVERY feed
         * (not just frame 0): an SPDR write while the TX buffer is full is
         * silently dropped, the sequence desyncs one word, and the burst
         * completes with overrun garbage — the r01g reply-misalignment
         * class (SPSR=0xA0, ECHO_22 lost, REG_14 the lone survivor). */
        volatile uint8_t *sr = spsr(bus);
        volatile uint16_t *dr = spdr(bus);
        uint32_t guard;
        for (guard = 20000u; guard != 0u && (*sr & SPSR_SPTEF) == 0u; guard--) {
        }
        if (guard != 0u) {
            (void)*sr;
            *dr = c->tx[c->idx];
            (void)*sr;
            *sr = 0u;
            c->idx++;
        }
        /* guard expiry: leave idx unchanged — the next poll/ISR retries
         * the same frame instead of desyncing (the burst stalls visibly
         * rather than corrupting silently) */
    } else {
        /* burst end: disarm, CLEAN the SPSR (trailing flag/OVRN kill —
         * see the pump note), hand the replies to the completion handler.
         * F5 (protocol_audit §1): snapshot the handoff BEFORE clearing the
         * state — the walk ISR runs at IPR 6 over this engine's 4 and can
         * preempt the window between state=IDLE and the field reads; its
         * pump then launches a queued walk job, overwriting done/job_id/rx
         * and misdirecting THIS burst's completion (snapshot works in both
         * entry paths and never delays the walk tick by raising SR). */
        rspi_done_fn done = c->done;
        uint8_t job_id = c->job_id;
        uint16_t *rx = c->rx;
        uint8_t frames = c->frames;
        *spcr(bus) = 0x08u;                     /* idle = MSTR only */
        (void)*spsr(bus);
        *spsr(bus) = 0x20u;
        c->state = CH_IDLE;
        c->idx = 0u;
        if (done) {
            done(bus, job_id, rx, frames);
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
    /* ADMATE0 (0xFFFE6030) acknowledge at init too — the per-bus values
     * clear both buses' stale A-DMAC flags once (stock per-bus on entry) */
    volatile uint8_t *const gate = (volatile uint8_t *)0xFFFE6030u;
    (void)*gate;
    *gate = 0xF3u;
    *gate = 0xFCu;
    /* both buses idle = MSTR only (stock's live idle) */
    *spcr(0) = 0x08u;
    *spcr(1) = 0x08u;
}
