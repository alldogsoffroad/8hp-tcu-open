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
    volatile bool handoff;          /* burst-end window guard (the stock's
                                     * '@' state, 0x93A08: set before the
                                     * snapshot handoff, cleared before the
                                     * done handler runs — see the burst-end
                                     * branch below) */
    uint32_t isr_count;             /* ISR entries (P8 diagnostic) */
    bool err;                       /* S2: OVRF/MODF/PERF seen this burst */
    bool retried;                   /* S2: the one retry (stock 0x93A74) used */
    uint32_t launch_us;             /* S3: TCNTA at launch (the reaper) */
    uint32_t err_count;             /* S2/S3: error completions (witness) */
    uint32_t retry_count;           /* S2: retries launched (witness) */
    volatile bool in_body;          /* r02g review S-1: an RX body is running
                                     * (a walk ISR at IPR 6 may preempt it) —
                                     * the reaper must not complete under it */
} rspi_chan_t;

/* SPSR error flag: OVRF (bit 0) only — the stock's burst-end ISR 0x965E4
 * tests `tst #1` (OVRF) plus the DMA flags; MODF/PERF are unreachable with
 * SPCR = 0xC8 and no parity (r02g review N2) */
#define SPSR_ERRS 0x01u

/* SPTEF wait, TIME-bounded (r02g review S-2): a frame is ~10 µs at
 * Pφ/12; 200 µs of no SPTEF is a dead transmitter. The old 20000-poll
 * guard spun for an unbounded-in-time stretch and, chained through the
 * error completions, could drain every queued job in one masked cascade
 * (~100 ms against a ~30 ms WDT). */
#define SPTEF_WAIT_US 200u
static bool sptef_wait(volatile uint8_t *sr)
{
    uint32_t t0 = plat_frt_us();

    while ((*sr & SPSR_SPTEF) == 0u) {
        if ((plat_frt_us() - t0) >= SPTEF_WAIT_US) {
            return false;
        }
    }
    return true;
}

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
    c->err = false;
    c->retried = false;
    c->launch_us = plat_frt_us();
    c->state = CH_ARMED;                        /* the commit point */
    plat_sr_set(sr_i);
    return true;
}

static void burst_end(rspi_chan_t *c, bool err);

/* ── pump: start the highest-priority staged job (task or init context) ── */

void plat_rspi_pump(uint8_t bus)
{
    rspi_chan_t *c = &s_chan[bus];
    if (c->state != CH_ARMED) {
        return;                                 /* nothing to start */
    }
    if (bus == 1u) {
        plat_adcmux_mux_hold();                 /* r02p: no PG12-15 edge in a bus-B burst */
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
    (void)*spsr(bus);  /* r02z: manual 8.10.1 dummy read of the cleared flag */

    /* arm the interrupt: SPCR = 0xC8 (SPRIE + SPE + MSTR) */
    *spcr(bus) = 0xC8u;

    /* feed frame 0 in the E60-proven order: WAIT for SPTEF first — an
     * SPDR write while the TX buffer is full is silently dropped and the
     * burst never clocks (old plat_sh7254r.c:694) */
    volatile uint8_t *sr = spsr(bus);
    volatile uint16_t *dr = spdr(bus);
    if (!sptef_wait(sr)) {
        /* S3 + r02g review S-2: a dead transmitter. Disarm and leave the
         * channel ARMED with its launch stamp: the reaper (sup_job_check_
         * free, 1 ms, task or walk context, no recursion) error-completes
         * it WITHOUT a retry. Completing here recursed through every
         * queued job's handler pump in one cascade. */
        *spcr(bus) = 0x08u;
        c->err = true;
        c->retried = true;
        return;
    }
    (void)*sr;                                  /* SPTEF read */
    *dr = c->tx[0];                             /* SPDR write */
    (void)*sr;                                  /* SPSR read */
    *sr = 0u;                                   /* SPSR clear */
    (void)*sr;  /* r02z: manual 8.10.1 dummy read of the cleared flag */

    c->idx = 1u;
    c->state = CH_FEEDING;
}

bool plat_rspi_busy(uint8_t bus)
{
    /* B1 FIX (baseline review 2026-11-11): the launch gate is the channel
     * state ONLY. r01s..r02f also returned busy while `handoff` was set, so
     * the sup_job_engine_pump() at the end of every reply handler — which
     * runs INSIDE the handoff window — launched nothing, and the trailing
     * plat_rspi_pump() below only starts an already-ARMED channel: queued
     * jobs never chained and waited for an unrelated pump (batch, monitor,
     * walk). J1, 6th in the per-pass queue, essentially never reached the
     * wire. The stock's 0x93A08 launches the next job (0x93902) BEFORE it
     * calls the release function. */
    return s_chan[bus].state != CH_IDLE;
}

bool plat_rspi_owned(uint8_t bus)
{
    /* the '@' window (stock 0x93A08 sets it before the release-fn harvest;
     * 0x93B8E exempts it from the busy-find cancel): the finishing job is
     * still owned by its completion until in_flight clears, so the check-
     * free recovery must not "recover" it as an orphan. */
    return s_chan[bus].state != CH_IDLE || s_chan[bus].handoff;
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
/* r02y witness (busA_errors_decode.md): per bus — [0] re-entries refused by
 * the SPRF/OVRF guard, [1] OVRF at entry, [2] OVRF seen inside the feed,
 * [3] min us between FEEDING entries, [4] max us between them */
volatile uint32_t s_rspi_w[2][5];
static uint32_t s_rspi_last_entry[2];
/* r02y: the last reap's snapshot taken BEFORE burst_end clears anything:
 * SPSR, idx, frames, job, age us (per bus) */
volatile uint32_t s_rspi_reap_snap[2][5];

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
    {   /* r02y: entry-gap witness (stamped before the guard) */
        const uint32_t now = plat_frt_us();
        const uint32_t gap = now - s_rspi_last_entry[bus];
        s_rspi_last_entry[bus] = now;
        if (c->idx > 1u) {                      /* a gap inside one burst */
            if (s_rspi_w[bus][3] == 0u || gap < s_rspi_w[bus][3]) {
                s_rspi_w[bus][3] = gap;
            }
            if (gap > s_rspi_w[bus][4]) {
                s_rspi_w[bus][4] = gap;
            }
        }
    }
    /* r02y THE RE-ENTRY GUARD: the feed clears SPSR and returns with no
     * dummy read of the flag, so the RX request can be accepted again after
     * the clear (manual 8.10.1: "Perform dummy read the interrupt source
     * flag after clearing it to ensure that the interrupt request that
     * should have been cleared is not received again erroneously"). On such
     * a re-entry neither SPRF nor OVRF is set: harvesting would store a
     * stale word and feed a second frame while one is still shifting (two
     * in flight → OVRF, stalls, slips). Refuse it, and count it. The dummy
     * read itself is deliberately NOT added yet (one variable: this flash
     * measures how often it happens). */
    if ((*spsr(bus) & (0x80u | SPSR_ERRS)) == 0u) {
        s_rspi_w[bus][0]++;
        return;
    }
    if ((*spsr(bus) & SPSR_ERRS) != 0u) {
        s_rspi_w[bus][1]++;
    }
    c->in_body = true;

    /* collect the reply of the frame that just completed (off-by-one law:
     * the reply to frame k is available at the k-1th interrupt — the
     * E60 lesson; first frame's reply available after feeding frame 1) */
    /* S2: the stock's burst-end ISR 0x965E4 reads SPSR and routes any OVRF
     * burst to the error completion 0x93A74 — never harvest corrupt data.
     * Read the flags BEFORE the SPDR read (which clears SPRF). */
    if ((*spsr(bus) & SPSR_ERRS) != 0u) {
        c->err = true;
    }
    c->rx[c->idx - 1u] = *spdr(bus);

    if (c->idx < c->frames) {
        /* feed the next frame. THE SPTEF WAIT IS MANDATORY on EVERY feed
         * (not just frame 0): an SPDR write while the TX buffer is full is
         * silently dropped, the sequence desyncs one word, and the burst
         * completes with overrun garbage — the r01g reply-misalignment
         * class (SPSR=0xA0, ECHO_22 lost, REG_14 the lone survivor). */
        volatile uint8_t *sr = spsr(bus);
        volatile uint16_t *dr = spdr(bus);
        if (sptef_wait(sr)) {
            (void)*sr;
            *dr = c->tx[c->idx];
            if ((*sr & SPSR_ERRS) != 0u) {
                s_rspi_w[bus][2]++;             /* r02y: OVRF inside the feed */
            }
            *sr = 0u;
            (void)*sr;  /* r02z: manual 8.10.1 dummy read of the cleared flag */
            c->idx++;
        } else {
            /* S3 + S-2: a dead transmitter mid-burst. The reply was read
             * and SPRF cleared, so no further interrupt will come: disarm,
             * mark, and let the reaper error-complete it (no retry, no
             * recursion from inside the ISR). */
            *spcr(bus) = 0x08u;
            c->err = true;
            c->retried = true;
        }
    } else {
        c->in_body = false;                     /* the handoff window owns it now */
        burst_end(c, c->err);
        return;
    }
    c->in_body = false;
}

/* burst end (success or error): disarm, CLEAN the SPSR (trailing flag/OVRN
 * kill — see the pump note), hand the replies to the completion handler.
 * F5 (protocol_audit §1): snapshot the registration BEFORE clearing the
 * state — the walk ISR runs at IPR 6 over this engine's 4 and can preempt
 * the window between state=IDLE and the field reads; its pump then launches
 * a queued walk job, overwriting done/job_id/rx and misdirecting THIS
 * burst's completion.
 *
 * S2 (stock 0x93A74): an errored burst is relaunched ONCE with the same
 * registration; a second failure completes with frames = 0, the handlers'
 * error path (sup_jobs.c job_done) — no corrupt word is ever harvested. */
static void burst_end(rspi_chan_t *c, bool err)
{
    uint8_t bus = c->bus;
    rspi_done_fn done = c->done;
    uint8_t job_id = c->job_id;
    uint16_t *rx = c->rx;
    uint8_t frames = c->frames;

    *spcr(bus) = 0x08u;                         /* idle = MSTR only */
    (void)*spsr(bus);
    *spsr(bus) = 0x20u;
    (void)*spsr(bus);  /* r02z: manual 8.10.1 dummy read of the cleared flag */
    if (bus == 1u) {
        plat_adcmux_mux_release();              /* r02p: SSL negated — stepping resumes */
    }
    if (err && !c->retried) {
        c->retried = true;
        c->err = false;
        c->retry_count++;
        c->idx = 0u;
        c->launch_us = plat_frt_us();
        c->state = CH_ARMED;
        plat_rspi_pump(bus);                    /* may recurse once into
                                                 * burst_end on a dead SPTEF;
                                                 * retried is set, so the
                                                 * recursion ends there */
        return;
    }
    if (err) {
        c->err_count++;
        frames = 0u;                            /* the error completion */
    }
    c->handoff = true;                          /* '@' open: the job stays
                                                 * owned until its done
                                                 * handler's release lands —
                                                 * the guard the stock runs in
                                                 * 0x93B8E (the armed state 0x40
                                                 * is exempt from the busy-find
                                                 * cancel) — read by
                                                 * plat_rspi_owned only. The
                                                 * handler's engine pump sees
                                                 * the channel IDLE and chains
                                                 * the next queued job. */
    c->state = CH_IDLE;
    c->idx = 0u;
    if (done) {
        done(bus, job_id, rx, frames);
    }
    c->handoff = false;                         /* '@' closed */
    plat_rspi_pump(bus);                        /* starts an ARMED channel
                                                 * (a walk-ISR launch that
                                                 * landed in the window) */
}

/* S3 reaper: a channel ARMED/FEEDING longer than max_us is dead (the
 * longest burst, W1's 7 frames, is ~80 µs at Pφ/12) — error-complete it.
 * Called from the job engine's check-free under the engine lock (SR.I ≥ 6),
 * which stops a NEW RX ISR — but check-free also runs in the walk ISR
 * (IPR 6), which can preempt an RX body already running (r02g review S-1):
 * never reap under a live body, nor the lab path (job 0xFF, its own
 * timeout; an FE13 burst may run past 1 ms). Returns true if it reaped. */
bool plat_rspi_reap(uint8_t bus, uint32_t max_us)
{
    if (bus >= 2u) {
        return false;
    }
    rspi_chan_t *c = &s_chan[bus];

    if (c->state == CH_IDLE || c->in_body || c->handoff || c->job_id == 0xFFu ||
        (plat_frt_us() - c->launch_us) < max_us) {
        return false;
    }
    s_rspi_reap_snap[bus][0] = *spsr(bus);      /* r02y: before anything clears it */
    s_rspi_reap_snap[bus][1] = c->idx;
    s_rspi_reap_snap[bus][2] = c->frames;
    s_rspi_reap_snap[bus][3] = c->job_id;
    s_rspi_reap_snap[bus][4] = plat_frt_us() - c->launch_us;
    c->retried = true;                          /* no retry for a dead channel */
    burst_end(c, true);
    return true;
}

uint32_t plat_rspi_err_count(uint8_t bus)
{
    return bus < 2u ? s_chan[bus].err_count : 0u;
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
    rspi_chan_t *c = &s_chan[bus];
    uint32_t e0 = c->err_count;
    plat_rspi_pump(bus);
    /* busy-wait with a timeout (diagnostic context only) */
    uint32_t t0 = plat_tick_count();
    while (c->state != CH_IDLE) {
        if (plat_tick_count() - t0 > timeout_ms) {
            /* abort: disarm */
            *spcr(bus) = 0x08u;
            if (bus == 1u) {
                plat_adcmux_mux_release();      /* r02p */
            }
            c->state = CH_IDLE;
            return false;
        }
        if (c->state == CH_ARMED && c->retried && c->err) {
            burst_end(c, true);                 /* dead SPTEF: no reaper here */
        }
    }
    return c->err_count == e0;                  /* an error completion is a failure */
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
