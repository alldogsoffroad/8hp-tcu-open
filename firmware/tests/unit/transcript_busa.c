/* transcript_busa.c — THE BUS-A OUTPUT CADENCE VERIFICATION (2026-11-18,
 * ~/tcu_sweep/output_cadence_decode.md, output_cadence_port.md).
 *
 * In the style of transcript.c: main.c's schedule on a 10 µs step — the
 * 1 ms task (the diag poll) at every ms, the 2.5 ms monitor at 0 / 3 ms,
 * the 10 ms task (the M5 request producer) at ms 1 of every second frame,
 * the 5 ms batch at ms 2 — with the REAL supervisor (test_ladder.c's chip and
 * switched-output plant; L3b, every hold released: the target
 * configuration) and the REAL bus-A chain (drv_jobs / drv_reqlaw /
 * drv_odiag / drv_handlers, the producer app/rq_prod.c + fill). Bus B is
 * drained at once; BUS A RUNS IN TIME, one frame per 12 µs (the r02y CPU-fed
 * frame time), through the ring. The run goes from boot through the ladder,
 * 'U', the mode switch and > 2 s of OS mode 1; both "parities" (the ladder
 * start shifted by one 5 ms frame).
 *
 * JUDGED (each a FAIL line + exit 1):
 *   (a) every F pass has exactly ONE law pass and ONE compose (and no other
 *       law / compose pass runs anywhere);
 *   (b) per F pass the bus-A jobs launched are exactly CYC, CFG, ID in that
 *       order (the diag job aside), CYC launched in that pass, and nothing
 *       is lost: queued == launched == completed, no drop, no miss;
 *   (c) every diag job is launched 3800..4800 µs (+ one frame) after the
 *       last cyclic's completion (step 1: the polled delay, queued);
 *   (d) mode 0: an F pass every 5 ms; mode 1: every 10 ms, nothing on the
 *       odd 5 ms pass;
 *   (e) 0x9096B (cyclics since the last diag reply) <= 1 throughout;
 *   (f) the rounding offset 0x9090A steps once per F pass: 4, 1, 4, 1 …;
 *   (g) in mode 1 the producer ran in the same 10 ms period before the law;
 *   (h) no bus-A launch was attempted while the channel was busy, and no
 *       staged word changed while its job was queued or on the wire.
 * Then part 2: a stalled cyclic (a lost RX interrupt) — the reaper error-
 * completes it, the ring dispatches the next job, the misses are counted,
 * and the following passes are whole again.
 *
 * NEGATIVE builds (ctest WILL_FAIL): -DNEG_PERIODIC (r03x's 10 ms periodic
 * re-enabled), -DDRV_RING_DISABLE (no FIFO: a busy channel refuses),
 * -DNEG_LAW5 (a law pass on mode 1's odd 5 ms pass), -DNEG_NOFREE (the
 * cyclic re-staged without the free check while bus A carries it).
 */
#define main test_ladder_main
#include "test_ladder.c"
#undef main

#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_odiag.h"
#include "bsw/drv/drv_rqprod.h"

extern volatile uint32_t s_drv_reap_n;

#define C_DIAGCNT (*(volatile uint8_t *)(uintptr_t)0xFFF9096Bu)

/* ── the bench's four healthy driver chips (test_odiag.c's model) ──────── */
static uint8_t  f_reg8[5][256];
static uint16_t f_e8[5], f_f0[5];

static uint16_t drv_chip(uint16_t tx, uint8_t c)
{
    const uint8_t top5 = (uint8_t)(tx >> 11);
    const uint8_t cmd = (uint8_t)(tx >> 8);
    const uint8_t d = (uint8_t)tx;
    if (c >= 4u) {
        return 0x0000u;
    }
    switch (top5) {
    case 0x1Du: f_e8[c] = (uint16_t)(tx & 0x7FFu); return 0xA000u;
    case 0x1Eu: f_f0[c] = (uint16_t)(tx & 0x7FFu); return 0xA000u;
    case 0x18u: return (uint16_t)(0xA000u | f_e8[c]);
    case 0x1Bu: return (uint16_t)(0xA000u | f_f0[c]);
    default: break;
    }
    switch (cmd) {
    case 0x88u: case 0x8Eu: case 0x90u: case 0x96u: case 0xA0u: case 0xA6u:
        f_reg8[c][cmd] = d;
        return 0xA200u;
    case 0x28u: return (uint16_t)(0xA200u | 0x80u | f_reg8[c][0x88]);
    case 0x30u: return (uint16_t)(0xA200u | f_reg8[c][0x90]);
    case 0x2Eu: return (uint16_t)(0xA200u | f_reg8[c][0x8E]);
    case 0x36u: return (uint16_t)(0xA200u | f_reg8[c][0x96]);
    case 0x60u: return (uint16_t)(0xA200u | f_reg8[c][0xA0]);
    case 0x66u: return (uint16_t)(0xA200u | f_reg8[c][0xA6]);
    case 0x18u: case 0x1Eu: return 0xA200u;
    case 0x06u: return 0xA212u;
    case 0x00u: return 0xA2BDu;
    default: return 0xA200u;
    }
}

/* ── bus B drained at once (test_ladder.c's chip model rides on it) ────── */
static void drain_b(void)
{
    unsigned guard = 512u;
    while (stub_rspi_busy(1u) && guard-- != 0u) {
        unsigned before = stub_log_n;
        stub_rspi_isr(1u);
        if (stub_log_n != before && stub_log[before].bus == 1u &&
            stub_log[before].job == JOB_DUTY &&
            (stub_log[before].word & 0xFF00u) == 0x8800u &&
            (before == 0u || stub_log[before - 1u].job != JOB_DUTY ||
             (stub_log[before - 1u].word & 0xFF00u) != 0x8800u)) {
            if (!chip.ignore_tokens && chip.level != 0u) {
                chip.level--;
            }
            chip.sel = (uint8_t)((chip.sel + 1u) & 0x0Fu);
        }
    }
}

/* ── bus A in time: one frame per 12 µs ────────────────────────────────── */
#define FRAME_US 12u
static bool s_a_stall;              /* part 2: a lost RX interrupt */
static bool s_stall_arm;            /* part 2: stall the next cyclic mid-burst */
static uint32_t s_a_due;
static bool s_a_was_busy;
static void bus_a_step(void)
{
    const bool busy = stub_rspi_busy(0u);
    if (busy && !s_a_was_busy) {
        s_a_due = stub.frt_us + FRAME_US;
    }
    s_a_was_busy = busy;
    if (busy && !s_a_stall && (int32_t)(stub.frt_us - s_a_due) >= 0) {
        stub_rspi_isr(0u);
        s_a_due += FRAME_US;
        s_a_was_busy = stub_rspi_busy(0u);
    }
}

/* ── the recorder ──────────────────────────────────────────────────────── */
#define MAXB 16384u
#define MAXF 4096u
typedef struct { uint32_t t0, t1; uint8_t job; uint8_t n; } burst_t;
static burst_t s_b[MAXB];
static unsigned s_nb;
static int s_cur = -1;              /* the burst being parsed (index) */
static const uint8_t k_frames[4] = { 10u, 15u, 40u, 45u };

typedef struct {
    uint32_t t;                     /* the F stage's FRT */
    uint8_t  mode;
    uint8_t  round;
    uint32_t law_n, comp_n;
    uint32_t t_prod;                /* the last producer pass before it */
    uint32_t t_law;
    uint32_t b0;                    /* first burst index at or after it */
} fpass_t;
static fpass_t s_f[MAXF];
static unsigned s_nf;
static uint32_t s_t_prod;
static unsigned s_cnt_max;          /* (e) */

static void parse_bus_a(unsigned from)
{
    for (unsigned k = from; k < stub_log_n; k++) {
        if (stub_log[k].bus != 0u) {
            continue;
        }
        const uint8_t j = (uint8_t)(stub_log[k].job - DRV_ID);
        if (j > 3u) {
            continue;
        }
        if (s_cur < 0 || s_b[s_cur].job != j || s_b[s_cur].n == k_frames[j]) {
            if (s_nb < MAXB) {
                s_cur = (int)s_nb++;
                s_b[s_cur].job = j;
                s_b[s_cur].n = 0u;
                s_b[s_cur].t0 = stub_log[k].t_us;
            }
        }
        s_b[s_cur].n++;
        s_b[s_cur].t1 = stub_log[k].t_us;
    }
}

static int s_fail;
static uint32_t s_stall_reap0;
/* at most 3 FAIL lines per verdict site (each site is one property) */
#define VERDICT(c, ...) do { static int n_site_; if (!(c)) { if (n_site_++ < 3) { printf("FAIL: " __VA_ARGS__); printf("\n"); } s_fail++; } } while (0)

/* ── one run ───────────────────────────────────────────────────────────── */
static unsigned s_parity;
static unsigned s_mode1_first_f;    /* index of the first mode-1 F pass */

static void boot(void)
{
    reset_all();
    plant.enabled = true;
    plant.lag = 0u;
    plant.never_on = false;
    plant.ignore_level = false;
    memset(f_reg8, 0, sizeof f_reg8);
    stub.drv_reply_fn = drv_chip;
    stub.reap_enable = true;
    memset(stub.rspi_launch_busy, 0, sizeof stub.rspi_launch_busy);
    memset(stub.rspi_tx_torn, 0, sizeof stub.rspi_tx_torn);
    drv_od_host_seed_select(false);
    drv_jobs_init();
    *(volatile uint8_t *)0xFFF89C13u = 0x43u;   /* the temperature byte (dump) */
    *(volatile uint8_t *)0xFFF89CD2u = 0x43u;
    drv_rq_prod_host_select(true);
    drv_id_kick();                              /* main.c: the bus-A ID at init */
    s_a_was_busy = false;
    s_a_stall = false;
    s_nb = 0u;
    s_cur = -1;
    s_nf = 0u;
    s_t_prod = 0u;
    s_cnt_max = 0u;
    s_mode1_first_f = 0u;
    stub_log_n = 0u;
}

static void run_frames(unsigned p_from, unsigned p_to, void (*hook)(unsigned p))
{
    const drv_out_wit_t *w = drv_out_wit();
    for (unsigned p = p_from; p <= p_to; p++) {
        if (p == 100u + s_parity) {
            stub.adc_ready = true;          /* the ladder start */
        }
        if (hook != NULL) {
            hook(p);
        }
        inputs();
        for (unsigned step = 0u; step < 500u; step++) {
            const unsigned us = step * 10u;
            const unsigned log0 = stub_log_n;
            if (us % 1000u == 0u) {
                drv_od_diag_poll(stub.frt_us);  /* task_1ms */
            }
            if (us == 0u || us == 3000u) {
                sup_monitor_poll();
                drain_b();
            }
            if (us == 1000u && (p & 1u) == 1u) {    /* task_10ms (ms ≡ 1 mod 10) */
                rq_prod_task_10ms(sup_mode());
                s_t_prod = stub.frt_us;
#if defined(NEG_PERIODIC)
                /* r03x's sol_tick_10ms → drv_cyclic_periodic, re-enabled */
                if (drv_job_check_free(DRV_CYC) && drv_job_check_free(DRV_CFG)) {
                    drv_cyclic_build(0, 8u);
                    (void)drv_job_queue(DRV_CYC);
                }
#endif
            }
            if (us == 2000u) {
                const uint32_t f0 = w->f_n;
                const uint8_t mode = sup_mode();
                sup_batch_5ms();
                drain_b();
#if defined(NEG_LAW5)
                if (mode != 0u && w->f_n == f0) {
                    drv_out_law();          /* a law pass on mode 1's odd 5 ms pass */
                }
#endif
#if defined(NEG_NOFREE)
                if (stub_rspi_busy(0u)) {
                    drv_cyclic_build(0, 8u);    /* staged without the free check */
                }
#endif
                if (w->f_n != f0 && s_nf < MAXF) {
                    fpass_t *f = &s_f[s_nf++];
                    f->t = w->t_f_us;
                    f->mode = mode;
                    f->round = CELL_RQ_ROUND;
                    f->law_n = w->law_n;
                    f->comp_n = w->compose_n;
                    f->t_prod = s_t_prod;
                    f->t_law = w->t_law_us;
                    f->b0 = s_nb;
                    if (mode != 0u && s_mode1_first_f == 0u) {
                        s_mode1_first_f = s_nf - 1u;
                    }
                }
            }
            if (plat_walktmr_running() &&
                (int32_t)(stub.frt_us - stub.walk_fire_us) >= 0) {
                plat_walktmr_stop();
                sup_walk_isr();
                drain_b();
            }
            sup_mode_background();
            bus_a_step();
            parse_bus_a(log0);
            if (s_stall_arm && s_cur >= 0 && s_b[s_cur].job == 3u && s_b[s_cur].n >= 5u &&
                s_b[s_cur].n < 45u && stub_rspi_busy(0u)) {
                s_a_stall = true;           /* the RX interrupts stop mid-cyclic */
                s_stall_arm = false;
                s_stall_reap0 = s_drv_reap_n;
            }
            if (s_a_stall && s_drv_reap_n != s_stall_reap0) {
                s_a_stall = false;          /* reaped: the next bursts run normally */
                s_cur = -1;                 /* the reaped burst is partial */
            }
            if (C_DIAGCNT > s_cnt_max) {
                s_cnt_max = C_DIAGCNT;
            }
            stub.frt_us += 10u;
        }
        stub_log_n = 0u;
    }
}

static const char *JNAME[4] = { "ID", "DIAG", "CFG", "CYC" };

/* the µs timeline of the bus-A jobs around F pass k, relative to it */
static void print_tick(unsigned k, const char *what)
{
    const fpass_t *f = &s_f[k];
    const uint32_t t_end = (k + 1u < s_nf) ? s_f[k + 1u].t : f->t + 10000u;
    printf("%s: F pass #%u at t=%u us (mode %u, 0x9090A %u)\n", what, k, (unsigned)f->t, f->mode, f->round);
    if (f->mode != 0u) {
        printf("   producer (task_10ms) at F%+d us, law at F%+d us\n",
               (int)(f->t_prod - f->t), (int)(f->t_law - f->t));
    } else {
        printf("   law at F%+d us\n", (int)(f->t_law - f->t));
    }
    for (unsigned b = (f->b0 > 3u ? f->b0 - 3u : 0u); b < s_nb && s_b[b].t0 < t_end + 200u; b++) {
        if (s_b[b].t1 + 6000u < f->t) {
            continue;
        }
        printf("   %-4s frames %2u  first frame F%+6d us  last F%+6d us\n", JNAME[s_b[b].job],
               s_b[b].n, (int)(s_b[b].t0 - f->t), (int)(s_b[b].t1 - f->t));
    }
}

static void judge(const char *name)
{
    const drv_out_wit_t *w = drv_out_wit();
    unsigned n_m0 = 0u, n_m1 = 0u;

    /* (a) */
    VERDICT(w->law_f_bad == 0u && w->compose_f_bad == 0u,
            "%s (a) F passes without exactly one law / compose pass: law %u compose %u",
            name, (unsigned)w->law_f_bad, (unsigned)w->compose_f_bad);
    VERDICT(w->law_n == w->f_n && w->compose_n == w->f_n,
            "%s (a) law passes %u / compose passes %u vs F passes %u", name,
            (unsigned)w->law_n, (unsigned)w->compose_n, (unsigned)w->f_n);
    for (unsigned k = 1u; k < s_nf; k++) {
        VERDICT(s_f[k].law_n - s_f[k - 1u].law_n == 1u && s_f[k].comp_n - s_f[k - 1u].comp_n == 1u,
                "%s (a) F pass #%u: %u law / %u compose passes since the last", name, k,
                (unsigned)(s_f[k].law_n - s_f[k - 1u].law_n), (unsigned)(s_f[k].comp_n - s_f[k - 1u].comp_n));
    }
    /* (b) the jobs per F interval (the last pass may still be on the wire) */
    for (unsigned k = 0u; k + 1u < s_nf; k++) {
        static const uint8_t EXP[3] = { 3u, 2u, 0u };   /* CYC, CFG, ID */
        unsigned e = 0u;
        for (unsigned b = 0u; b < s_nb; b++) {
            if (s_b[b].t0 <= s_f[k].t || s_b[b].t0 > s_f[k + 1u].t || s_b[b].job == 1u) {
                continue;
            }
            VERDICT(e < 3u && s_b[b].job == EXP[e] && s_b[b].n == k_frames[s_b[b].job],
                    "%s (b) F pass #%u: bus-A job %u is %s (%u frames), expected %s", name, k, e,
                    JNAME[s_b[b].job], s_b[b].n, e < 3u ? JNAME[EXP[e]] : "(none)");
            e++;
        }
        VERDICT(e == 3u, "%s (b) F pass #%u carried %u bus-A jobs, expected CYC CFG ID", name, k, e);
    }
    for (unsigned j = 0u; j < 4u; j++) {
        VERDICT(w->dropped[j] == 0u && w->missed[j] == 0u && w->err[j] == 0u,
                "%s (b) %s dropped %u missed %u errors %u", name, JNAME[j],
                (unsigned)w->dropped[j], (unsigned)w->missed[j], (unsigned)w->err[j]);
        VERDICT(w->queued[j] == w->launched[j] && w->launched[j] - w->done[j] <= 1u,
                "%s (b) %s queued %u launched %u done %u (lost)", name, JNAME[j],
                (unsigned)w->queued[j], (unsigned)w->launched[j], (unsigned)w->done[j]);
    }
    VERDICT(w->mismatch == 0u && w->launch_refused == 0u, "%s (b) ring mismatch %u launch refused %u",
            name, (unsigned)w->mismatch, (unsigned)w->launch_refused);
    VERDICT(drv_od_diag_drop_n() == 0u, "%s (b) diag jobs dropped %u", name, (unsigned)drv_od_diag_drop_n());
    /* (c) diag − the previous cyclic's last frame */
    {
        uint32_t last_cyc_end = 0u;
        unsigned n_diag = 0u;
        for (unsigned b = 0u; b < s_nb; b++) {
            if (s_b[b].job == 3u) {
                last_cyc_end = s_b[b].t1;
            } else if (s_b[b].job == 1u && last_cyc_end != 0u) {
                const uint32_t d = s_b[b].t0 - last_cyc_end;
                n_diag++;
                VERDICT(d >= 3800u && d <= 4800u + 2u * FRAME_US,
                        "%s (c) diag launched %u us after the cyclic's completion", name, (unsigned)d);
            }
        }
        VERDICT(n_diag > 100u, "%s (c) only %u diag jobs", name, n_diag);
    }
    /* (d) the cadence by mode */
    for (unsigned k = 1u; k < s_nf; k++) {
        const uint32_t dt = s_f[k].t - s_f[k - 1u].t;
        /* the stub's clock also moves by the walk ISR's closed-loop spin
         * (plat_walktmr_wait_until), so the batch's FRT stamp wobbles by
         * tens of µs around the 5 / 10 ms grid */
        if (s_f[k].mode == 0u) {
            n_m0++;
            VERDICT(dt >= 4900u && dt <= 5100u, "%s (d) mode 0: F pass #%u %u us after the last", name, k, (unsigned)dt);
        } else {
            n_m1++;
            VERDICT(dt >= 9900u && dt <= 10100u, "%s (d) mode 1: F pass #%u %u us after the last", name, k, (unsigned)dt);
        }
    }
    VERDICT(n_m0 > 100u && n_m1 > 200u, "%s (d) %u mode-0 / %u mode-1 F passes", name, n_m0, n_m1);
    /* (e) */
    VERDICT(s_cnt_max <= 1u, "%s (e) 0x9096B reached %u", name, s_cnt_max);
    /* (f) 0 → 4 → 1 → 4 … per F pass */
    for (unsigned k = 0u; k < s_nf; k++) {
        const uint8_t exp = (k % 2u == 0u) ? 4u : 1u;
        VERDICT(s_f[k].round == exp, "%s (f) F pass #%u: 0x9090A %u, expected %u", name, k,
                s_f[k].round, exp);
    }
    /* (g) mode 1: producer before the law, in the same 10 ms period */
    for (unsigned k = 0u; k < s_nf; k++) {
        if (s_f[k].mode != 0u) {
            const int32_t d = (int32_t)(s_f[k].t_law - s_f[k].t_prod);
            VERDICT(d > 0 && d < 10000, "%s (g) mode-1 F pass #%u: law %d us after the producer", name, k, (int)d);
        }
    }
    /* (h) */
    VERDICT(stub.rspi_launch_busy[0] == 0u, "%s (h) %u bus-A launches attempted on a busy channel",
            name, stub.rspi_launch_busy[0]);
    VERDICT(stub.rspi_tx_torn[0] == 0u, "%s (h) %u bus-A words changed while their job was queued / on the wire",
            name, stub.rspi_tx_torn[0]);
    printf("%s: %u F passes (%u mode 0, %u mode 1; first mode-1 #%u), law %u compose %u, "
           "cyc %u cfg %u id %u diag %u launched; ring high-water %u; 0x9096B max %u; "
           "'U' %s, mode %u\n", name, s_nf, n_m0, n_m1, s_mode1_first_f, (unsigned)w->law_n,
           (unsigned)w->compose_n, (unsigned)w->launched[3], (unsigned)w->launched[2],
           (unsigned)w->launched[0], (unsigned)w->launched[1], w->ring_hw, s_cnt_max,
           CELL_SEQ2_STATE[0] == (uint8_t)'U' ? "reached" : "NOT reached", sup_mode());
}

/* ── part 2: a stalled cyclic, the reaper, the ring ────────────────────── */
/* p0: the first frame to run; mode0: the stall is in OS mode 0 (5 ms:
 * the next F pass finds the cyclic still pending → a CYC miss, the reaper
 * fires one pass later) — else mode 1 (10 ms: the reaper fires at the next
 * F pass, before its check-free; the CFG / ID queued behind the cyclic are
 * the misses) */
static void part2(unsigned p0, bool mode0)
{
    const drv_out_wit_t *w = drv_out_wit();
    const uint32_t reap0 = s_drv_reap_n;
    uint32_t miss0[4], q0[4], l0[4], e0[4], miss2_0 = w->miss2_n;
    unsigned k_from;
    for (unsigned j = 0u; j < 4u; j++) {
        miss0[j] = w->missed[j];
        q0[j] = w->queued[j];
        l0[j] = w->launched[j];
        e0[j] = w->err[j];
    }
    s_stall_arm = true;             /* the next cyclic loses its RX interrupt */
    run_frames(p0, p0 + 5u, NULL);  /* 30 ms: the reaper (5 ms age) at an F pass */
    k_from = s_nf;
    run_frames(p0 + 6u, p0 + 30u, NULL);
    printf("part 2 (%s): reaps %u; cyclic error completions %u; misses ID %u DIAG %u CFG %u CYC %u "
           "(second misses %u); then cyc %u cfg %u id %u launched\n",
           mode0 ? "mode 0" : "mode 1",
           (unsigned)(s_drv_reap_n - reap0), (unsigned)(w->err[3] - e0[3]),
           (unsigned)(w->missed[0] - miss0[0]), (unsigned)(w->missed[1] - miss0[1]),
           (unsigned)(w->missed[2] - miss0[2]), (unsigned)(w->missed[3] - miss0[3]),
           (unsigned)(w->miss2_n - miss2_0), (unsigned)(w->launched[3] - l0[3]),
           (unsigned)(w->launched[2] - l0[2]), (unsigned)(w->launched[0] - l0[0]));
    VERDICT(s_drv_reap_n - reap0 == 1u && w->err[3] - e0[3] == 1u,
            "part 2: the stalled cyclic was not reaped exactly once (reaps %u, errors %u)",
            (unsigned)(s_drv_reap_n - reap0), (unsigned)(w->err[3] - e0[3]));
    if (mode0) {
        VERDICT(w->missed[3] - miss0[3] >= 1u, "part 2: no cyclic miss counted while it was pending");
    } else {
        VERDICT(w->missed[2] - miss0[2] >= 1u && w->missed[0] - miss0[0] >= 1u,
                "part 2: no CFG / ID miss counted behind the stalled cyclic");
    }
    for (unsigned j = 0u; j < 4u; j++) {
        VERDICT(w->dropped[j] == 0u, "part 2: %s dropped", JNAME[j]);
        VERDICT(w->queued[j] - q0[j] == w->launched[j] - l0[j], "part 2: %s queued %u launched %u",
                JNAME[j], (unsigned)(w->queued[j] - q0[j]), (unsigned)(w->launched[j] - l0[j]));
    }
    /* whole again: every later F pass carries CYC, CFG, ID */
    for (unsigned k = k_from; k + 1u < s_nf; k++) {
        unsigned n = 0u;
        for (unsigned b = 0u; b < s_nb; b++) {
            if (s_b[b].t0 > s_f[k].t && s_b[b].t0 <= s_f[k + 1u].t && s_b[b].job != 1u &&
                s_b[b].n == k_frames[s_b[b].job]) {
                n++;
            }
        }
        VERDICT(n == 3u, "part 2: F pass #%u after the reap carried %u jobs", k, n);
    }
    VERDICT(stub.rspi_launch_busy[0] == 0u && stub.rspi_tx_torn[0] == 0u, "part 2: (h) launch busy %u torn %u",
            stub.rspi_launch_busy[0], stub.rspi_tx_torn[0]);
}

int main(void)
{
    for (s_parity = 0u; s_parity < 2u; s_parity++) {
        char name[32];
        snprintf(name, sizeof name, "parity %u", s_parity);
        boot();
        run_frames(1u, 560u, NULL);
        judge(name);
        if (s_parity == 0u) {
            unsigned k0 = 30u, k1 = s_mode1_first_f + 100u;
            if (k1 >= s_nf) {
                k1 = s_nf - 2u;
            }
            print_tick(k0, "MODE 0 tick");
            print_tick(k1, "MODE 1 tick");
            part2(561u, false);
            boot();
            run_frames(1u, 40u, NULL);
            VERDICT(sup_mode() == 0u, "part 2: not in mode 0 at frame 40");
            part2(41u, true);
        }
    }
    if (s_fail != 0) {
        printf("transcript_busa: FAIL (%d)\n", s_fail);
        return 1;
    }
    printf("transcript_busa: PASS\n");
    return 0;
}
