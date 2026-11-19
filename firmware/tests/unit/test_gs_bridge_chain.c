/* test_gs_bridge_chain.c — THE GARAGE BRIDGE ON THE REAL BUS-A CHAIN
 * (2026-11-18, ~/tcu_sweep/garage_g1_port.md; plan ~/tcu_sweep/
 * garage_live_plan.md §5 items 1, 2, 5).
 *
 * The harness of test_diag_realfb.c (included): the real supervisor (the
 * ladder, 'U', the mode-1 switch), the real bus-A chain in main.c's
 * schedule (the 2.5 ms monitor, the 5 ms batch with the F stage, the 1 ms
 * diag poll, bus A one frame per 12 µs, the four driver chips), the
 * feedback shadow on the Timer F / D model at the 0x838B0 slot (REAL: the
 * r04f bench configuration) — PLUS main.c's 10 ms task at ms 1 of every
 * 10 ms: the producer (DRV_RQ_USE_PRODUCER), the vehicle / CAN receive,
 * kl15, the live range chain, park_tick, the garage shadow (GS_FORCED 1)
 * and the bridge; the gs-check sequence with the brake + engine 700 sims
 * from power-on: P (0.3 s after the mode-1 switch), D 2 s, P 1 s, R 2 s, P.
 *
 * Built twice:
 *   test_gs_bridge_chain     GS_LIVE_DIR 0 (G1, the SH configuration):
 *     (1) LIVE IDENTITY — the same run without and with the bridge ticking,
 *         every 5 ms frame: every bus-A word on the wire (in order), the
 *         whole cell window 0xFFF80000..0xFFF96000 (the shadow's own RAM
 *         excluded; the bridge's host RAM is outside it), the park demands
 *         — identical in every frame, while the bridge would-arms;
 *     (2) THE RECORDER on the real chain: both slots, the would-arm window
 *         from the commit pass to lever P, the would-be words = the shadow's
 *         current / 6 pass for pass, the live words = the P content, the
 *         estimate recorded, guard 0, agreement after the shadow's warm-up,
 *         the OS-mode-2 witness requesting 2.
 *   test_gs_bridge_g2[_temps] GS_LIVE_DIR 3 (host only — THE G2 GATE): the
 *     same runs with the bridged currents APPLIED through the seam; the
 *     output diagnosis on the real feedback must stay silent (no fault
 *     word, no switch-off, no DSM report, no would-be reset) at SAT 0, both
 *     10 ms parities; the staged words follow the shadow with <= 1 pass of
 *     lag; the fixed channels never move; the P content within 2 passes of
 *     lever P. Informational: SAT 10 (the temperature dry-run's D / C
 *     0x0010 sensitivity) and, in _temps (DIAG_TEMPS, 0x940FC forced 8),
 *     the current bounds' margins incl. B's lower bound during its fill.
 */
#define TEST_DIAG_REALFB_NO_MAIN 1
#include "test_diag_realfb.c"

#include "app/gs_shadow.h"
#include "app/gs_fill.h"
#include "app/gs_bridge.h"
#include "app/park.h"
#include "app/park_law.h"
#include "app/rng_chain.h"
#include "app/rq_prod.h"
#include "bsw/com/crc8.h"
#include "bsw/drv/drv_gsbr.h"
#include "bsw/drv/drv_park.h"
#include "bsw/drv/drv_rqprod.h"
#include "io/sensors/sensors.h"
#include "io/vehicle/vehicle.h"
#include "safety/kl15.h"
#include "safety/park_mon.h"

#include <sys/wait.h>
#include <unistd.h>

static sensors_in_t g_z;
const sensors_in_t *sensors_in(void) { return &g_z; }

/* ── the vehicle sims (the gs-check: lever + brake + engine 700) ───────── */
static uint8_t g_ctr[0x800];
static void g_send(can_bus_t bus, uint16_t id, uint8_t dlc, bool prot, const uint8_t *data)
{
    can_frame_t f = { .id = id, .dlc = dlc };
    memcpy(f.data, data, dlc);
    if (prot) {
        g_ctr[id] = (uint8_t)((g_ctr[id] + 1u) & 0x0Fu);
        f.data[dlc - 2u] = (uint8_t)((f.data[dlc - 2u] & 0x0Fu) | (uint8_t)(g_ctr[id] << 4));
        f.data[dlc - 1u] = crc8_j1850(f.data, (uint8_t)(dlc - 1u));
    }
    vehicle_rx(bus, &f);
}

#define GFRAMES 1420u
#define K_D     60u             /* frames after the mode-1 switch: lever D */
#define K_P1    460u            /* P */
#define K_R     660u            /* R */
#define K_P2    1060u           /* P */

/* lever code (1 P 2 R 4 D) at frame p, mode-1 frame m1 (0 = not yet) */
/* the R step of the current run (G4a: a short P hold interrupts the
 * shadow's 1.2 s release; default K_R) */
static unsigned g_kR = K_R, g_kP1 = K_P1, g_kP2 = K_P2;
/* the lower bound of the shadow's normal completion (check_slot). G4a: when
 * the P hold between D and R is shorter than the shadow's release (1.2 s)
 * plus B's release ramp in its fill FSM (≈ 0.8 s) — the default runs (1 s)
 * and shortP (0.3 s) — the stock's R engagement starts with B not yet
 * released and completes earlier (pass 84; HEAD's instant release: 93) */
static unsigned g_done_lo = 90u;
static uint8_t lever_at(unsigned p, unsigned m1)
{
    unsigned k;
    if (m1 == 0u || p < m1) {
        return 1u;
    }
    k = p - m1;
    if (k >= K_D && k < g_kP1) { return 4u; }
    if (k >= g_kR && k < g_kP2) { return 2u; }
    return 1u;
}

typedef struct {
    const char *name;
    bool bridge;
    bool real;
    unsigned sat_ms;
    unsigned sat_from;      /* 0: SAT from power-on; else from frame m1 + sat_from */
    unsigned parity;
    unsigned kR;            /* 0: K_R; else the R step (frames after the mode-1 switch) */
    unsigned kP1, kP2;      /* 0: K_P1 / K_P2; else the first / second P step */
    bool info;
    uint8_t fc_mode;        /* DIAG_TEMPS: 0x940FC forced */
    /* results (child → parent) */
    unsigned m1, u;
    unsigned fails;
    unsigned lagP_D, lagP_R;        /* ticks from lever P to the staged P content (applied) */
    unsigned fix_moves;             /* applied: a fixed channel's staged word left its pin */
    unsigned follow_bad, follow_n;  /* applied: staged moving words vs the shadow, 1 pass lag */
    unsigned modes_bad;             /* applied: ch 0..6 law mode != 3 after A's fill */
    uint32_t armed_n, applied_n, take_n, guard_n;
    uint64_t h[GFRAMES];
    unsigned c3_frame;      /* the frame of the shadow's third commit (0 → R): the
                             * first engagement after the D → P release */
    bool gold_full;         /* G4a golden: every frame identical to HEAD (else
                             * every frame BEFORE c3_frame) */
    run_r_t r;
} gr_t;

static uint64_t fnv64(uint64_t h, uint32_t v)
{
    for (int k = 0; k < 4; k++) {
        h = (h ^ (uint8_t)(v >> (8 * k))) * 0x100000001B3ull;
    }
    return h;
}

static uint64_t g_hash_window(uint64_t h)
{
    const volatile uint8_t *p = (const volatile uint8_t *)(uintptr_t)0xFFF80000u;
    for (uint32_t a = 0u; a < 0x16000u; a++) {
        const uint32_t abs = 0xFFF80000u + a;
        if ((abs >= GS_REQ_BASE && abs < GS_REQ_BASE + GS_REQ_SIZE) ||
            (abs >= 0xFFF8EE00u && abs < 0xFFF90000u) ||
            (abs >= 0xFFF91440u && abs < 0xFFF92400u)) {
            continue;                       /* the garage shadow's own RAM */
        }
        h = (h ^ p[a]) * 0x100000001B3ull;
    }
    return h;
}

static unsigned sp_of(uint16_t w) { return 6u * (unsigned)(w & 0x7FFu); }
static unsigned absd(unsigned a, unsigned b) { return a > b ? a - b : b - a; }

/* main.c's task_10ms (the parts this harness models), at ms 1 of 10 */
static void task10(gr_t *g, unsigned p)
{
    const uint8_t mode = sup_mode();
    const uint8_t lev = lever_at(p, g->m1);
    uint8_t d[8];
    {
        const uint8_t l[5] = { lev, 0, 0, 0, 0 };
        g_send(CAN_BUS1, 0x2EAu, 5u, true, l);
        g_send(CAN_BUS2, 0x210u, 5u, true, l);
    }
    memset(d, 0, sizeof d);
    d[0] = 0x04u;                                   /* brake on */
    g_send(CAN_BUS1, 0x140u, 8u, true, d);
    memset(d, 0, sizeof d);
    d[0] = (uint8_t)(700u >> 8);
    d[1] = (uint8_t)700u;
    d[2] = 0x1Fu;
    d[3] = 0xFFu;
    g_send(CAN_BUS1, 0x108u, 8u, true, d);          /* engine 700 */

    rq_prod_task_10ms(mode);
    vehicle_update_10ms();
    *(volatile uint16_t *)(uintptr_t)KL15_SRC_CELL = 13206u;
    kl15_tick_10ms();
    rng_chain_tick_10ms(mode, 1u, 1u);
    park_set_os_mode(mode);
    park_tick_10ms();
    {
        const vehicle_in_t *v = vehicle_in();
        const gs_in_t gi = {
            .os_mode = mode,
            .rngd = *(const volatile uint8_t *)0xFFF89CDCu,
            .park_model = park_law_state(),
            .park_req = park_law_request(),
            .temp = *(const volatile uint8_t *)0xFFF89C13u,
            .cd2 = *(const volatile uint8_t *)0xFFF89CD2u,
            .eng_speed = (uint16_t)v->engine_speed,
            .eng_valid = (v->q_engine_speed == Q_OK) ? 1u : 0u,
            .out_rpm = 0u,
            .tur_rpm = 0u,
        };
        gs_shadow_tick_10ms(&gi);
        if (g->bridge) {
            const gsb_in_t bi = {
                .mon_faults = park_mon_faults(),
                .relwin_st = drv_rq_rel_win_state(),
                .eng_valid = gi.eng_valid,
                .tur_ok = 1u,
                .eng_speed = gi.eng_speed,
                .tur_rpm = 0u,
                .cyc = drv_job_tx(DRV_CYC),
            };
            gs_bridge_tick_10ms(&bi);
        }
    }
    drv_rq_request_set(7u, park_slot7_release());   /* ch 7: the park release (SH: DRV_RQ_PSET) */
}

/* applied runs: the staged words after each 10 ms tick */
typedef struct { uint8_t lev; uint16_t st[5]; uint16_t sh[5]; bool applied; } tick_t;
static tick_t g_tk[GFRAMES];
static unsigned g_tk_n;

static void run_g(gr_t *g)
{
    obs_t prev, now;
    uint32_t law0;
    run_r_t *r = &g->r;

    memset(r, 0, sizeof *r);
    g_kR = (g->kR != 0u) ? g->kR : K_R;
    g_kP1 = (g->kP1 != 0u) ? g->kP1 : K_P1;
    g_kP2 = (g->kP2 != 0u) ? g->kP2 : K_P2;
    g_done_lo = ((g_kR - g_kP1) < 400u) ? 80u : 90u;
    r->name = g->name;
    r->real = g->real;
    r->sat_ms = (g->sat_from != 0u) ? 0u : g->sat_ms;
    r->parity = g->parity;
    r->fill = false;
#if defined(DIAG_TEMPS)
    r->fc_mode = g->fc_mode;
#endif
    rr_reset(r);
    memset(g_ctr, 0, sizeof g_ctr);
    memset(&g_z, 0, sizeof g_z);
    g_z.q_output_speed = Q_OK;
    g_z.q_turbine_speed = Q_OK;
    vehicle_init();
    park_init();
    rng_chain_init();
    kl15_init();
    *(volatile uint16_t *)(uintptr_t)KL15_SRC_CELL = 13206u;
    *(volatile uint8_t *)0xFFF89C13u = 0x4Fu;       /* the bench's 38.9 °C (r03u) */
    *(volatile uint8_t *)0xFFF89CD2u = 0x4Fu;
    drv_rq_prod_host_select(true);
    drv_rq_request_set(7u, 0u);
    gs_shadow_init();
    gs_bridge_init(true);
    g_tk_n = 0u;
    observe(&prev);
    law0 = drv_out_wit()->law_n;
#if defined(DIAG_TEMPS)
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        r->mup[ch] = r->mlo[ch] = r->aup[ch] = r->alo[ch] = 0x7FFFFFFF;
        r->ub_lo[ch] = r->lb_lo[ch] = 0xFFFFu;
    }
#endif
    for (unsigned p = 1u; p <= GFRAMES; p++) {
        uint64_t h = 0xCBF29CE484222325ull;
#if defined(DIAG_TEMPS)
        s_frame = p;
        s_uframe = r->u_frame;
#endif
        if (p == 100u + g->parity) {
            stub.adc_ready = true;
            s_temp_valid = true;
        }
        if (g->sat_from != 0u && g->m1 != 0u && p == g->m1 + g->sat_from) {
            s_sat_ms = g->sat_ms;               /* the saturation model from here on */
        }
        inputs();
        for (unsigned step = 0u; step < 500u; step++) {
            const unsigned us = step * 10u;
            if (us % 1000u == 0u) {
                drv_od_diag_poll(stub.frt_us);
                if (((stub.frt_us / 1000u) % 10u) == 1u) {
                    task10(g, p);
                    if (g_tk_n < GFRAMES) {
                        tick_t *t = &g_tk[g_tk_n++];
                        const uint16_t *cyc = drv_job_tx(DRV_CYC);
                        t->lev = lever_at(p, g->m1);
                        t->applied = g->bridge && gs_bridge_state()->applied != 0u;
                        for (uint8_t ch = 0u; ch < 5u; ch++) {
                            t->st[ch] = cyc[(((ch & 1u) != 0u) ? 20u : 10u) + (ch >> 1)];
                            t->sh[ch] = gs_shadow_cur(ch);
                        }
                    }
                }
            }
            if (us == 0u || us == 3000u) {
                sup_monitor_poll();
                drain_b();
            }
            if (us == 2000u) {
                sup_batch_5ms();
                drain_b();
                if (CELL_SEQ2_STATE[0] == (uint8_t)'U' && r->u_frame == 0u) {
                    r->u_frame = p;
                }
                if (CELL_OS_MODE == 1u && r->sw_frame == 0u) {
                    r->sw_frame = p;
                    g->m1 = p;
                }
                if (drv_out_wit()->law_n != law0) {
                    law0 = drv_out_wit()->law_n;
                    r->passes++;
                    observe(&now);
                    diff_print(r, p, &prev, &now);
#if defined(DIAG_TEMPS)
                    if (getenv("GSB_BTRACE") != NULL && r->u_frame != 0u && g->parity == 0u &&
                        g->sat_ms == 0u && g->real && p >= r->u_frame + 60u && p <= r->u_frame + 300u) {
                        /* B (ch 2) / C (ch 4) through the D engagement: the tested
                         * bits, the estimate and the bounds the check used */
                        printf("    BTRACE U+%u B t%04X est %4u LB %4u UB %4u req %4u | C t%04X est %4u LB %4u UB %4u req %4u\n",
                               p - r->u_frame, U16C_(0xFFF94944u + 4u), U16C_(0xFFF93394u + 4u), r->lb_prev[2],
                               r->ub_prev[2], U16C_(0xFFF94ED4u + 4u), U16C_(0xFFF94944u + 8u),
                               U16C_(0xFFF93394u + 8u), r->lb_prev[4], r->ub_prev[4], U16C_(0xFFF94ED4u + 8u));
                    }
                    bounds_track(r, p);
#endif
                    if (r->u_frame != 0u) {
                        for (uint8_t ch = 0u; ch < 10u; ch++) {
                            r->fb_hist[ch][cls_idx(now.fb[ch])]++;
                            r->fw_or[ch] = (uint16_t)(r->fw_or[ch] | now.fw[ch]);
                            if (now.ccls[ch] > r->ccls_max[ch]) {
                                r->ccls_max[ch] = now.ccls[ch];
                            }
                        }
                        /* A's boot fill is over by U+40: every A..Sys law mode 3 */
                        if (p > r->u_frame + 60u) {
                            for (uint8_t ch = 0u; ch < 7u; ch++) {
                                if (U8C_(0xFFF94FD8u + ch) != 3u) {
                                    g->modes_bad++;
                                }
                            }
                        }
                    }
                    prev = now;
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
            stub.frt_us += 10u;
            fb_wave_step();
        }
        /* the frame's identity hash: every bus-A word in wire order, the
         * cell window, the park demands */
        for (unsigned i = 0u; i < stub_log_n; i++) {
            if (stub_log[i].bus == 0u) {
                h = fnv64(h, ((uint32_t)stub_log[i].job << 16) | stub_log[i].word);
            }
        }
        h = fnv64(h, park_slot7_release());
        h = fnv64(h, park_slot11_hold());
        g->h[p - 1u] = g_hash_window(h);
        stub_log_n = 0u;
        if (g->c3_frame == 0u && gs_shadow_wit()->commits >= 3u) {
            g->c3_frame = p;
        }
    }
    g->u = r->u_frame;
    g->armed_n = g->bridge ? gs_bridge_state()->armed_n : 0u;
    g->applied_n = g->bridge ? gs_bridge_state()->applied_n : 0u;
    g->take_n = g->bridge ? gs_bridge_state()->take_n : 0u;
    g->guard_n = g->bridge ? gs_bridge_state()->guard_n : 0u;
}

static int g_fails;
#define GCHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); g_fails++; } } while (0)

static const char *why_str(uint16_t w)
{
    static char b[160];
    static const char *N[16] = { "IDLE", "DIR", "RAIL", "OS", "RANGE", "PARK", "MON", "DIAG",
                                 "RELWIN", "WIN", "ENGT", "COMMITS", "AGREE", "G_RANGE", "G_FIXED",
                                 "G_LATCH" };
    int n = 0;
    b[0] = 0;
    for (unsigned k = 0u; k < 16u; k++) {
        if ((w >> k) & 1u) {
            n += snprintf(b + n, sizeof b - (size_t)n, "%s%s", n ? "|" : "", N[k]);
        }
    }
    if (n == 0) {
        snprintf(b, sizeof b, "-");
    }
    return b;
}

/* the recorder against the shadow's own recorder, pass for pass */
static void check_slot(uint8_t k, bool applied_build, bool print)
{
    const volatile gsb_t *b = gs_bridge_state();
    const volatile gsb_slot_t *s = &b->slot[k];
    const uint16_t P[5] = { 8500u, 8500u, 500u, 8500u, 8500u };
    unsigned armed = 0u, bad_wb = 0u, bad_live = 0u, est_nz = 0u;

    printf("  slot %u (%s): n %u, t0 %u, arm_on %u, arm_off %u (%s), done_at %u (cause %u), back_at %u, "
           "armed_n %u, guard_n %u\n", k, k == 0u ? "D" : "R", s->n, (unsigned)s->t0, s->arm_on,
           s->arm_off, why_str(s->off_why), s->done_at, s->done_cause, s->back_at, s->armed_n, s->guard_n);
    GCHECK(s->pf == (uint8_t)(k + 1u) && s->n == GSB_REC_N, "slot %u: pf %u n %u", k, s->pf, s->n);
    GCHECK(s->arm_on == 0u, "slot %u: the would-arm window opens at the commit pass (arm_on %u)", k, s->arm_on);
    GCHECK(s->done_at >= g_done_lo && s->done_at <= 100u && s->done_cause == 1u,
           "slot %u: the shadow's normal completion at pass 90..100 (%u, cause %u)", k, s->done_at, s->done_cause);
    GCHECK(s->guard_n == 0u, "slot %u: guard trips %u", k, s->guard_n);
    GCHECK(s->arm_off != GSB_NONE && (s->off_why & GSB_R_RANGE) != 0u,
           "slot %u: disarmed by the range (lever P): off %u why %s", k, s->arm_off, why_str(s->off_why));
    GCHECK(s->back_at != GSB_NONE && (unsigned)(s->back_at - s->arm_off) <= 2u,
           "slot %u: shadow == live within 2 passes of the disarm (off %u back %u)", k, s->arm_off, s->back_at);
    if (print) {
        printf("  pass ph0 ph1 ph2 st  why          | would-be A D B E C          | live staged A D B E C       "
               "| est A D B E C               | shadow A D B E C\n");
    }
    for (uint8_t i = 0u; i < s->n; i++) {
        const volatile gsb_rec_t *e = gs_bridge_rec(k, i);
        const volatile gs_rec_t *sr = gs_shadow_rec(k, i);
        static const uint8_t SH_CH[5] = { 0u, 1u, 2u, 3u, 4u };
        if ((e->st & 1u) != 0u) {
            armed++;
            for (uint8_t ch = 0u; ch < 5u; ch++) {
                const unsigned cur = sr->cur[SH_CH[ch]];
                if (absd(sp_of(e->wb[ch]), cur) > 6u) {
                    bad_wb++;
                }
            }
        }
        if (!applied_build) {
            /* G1: nothing is applied — the live staged words stay the P content */
            for (uint8_t ch = 0u; ch < 5u; ch++) {
                if (absd(sp_of(e->live[ch]), P[ch]) > 6u) {
                    bad_live++;
                }
            }
        }
        for (uint8_t ch = 0u; ch < 5u; ch++) {
            if (e->est[ch] != 0u) {
                est_nz++;
            }
        }
        if (print && (i < 16u || (i % 8u) == 0u || (e->st & 0x80u) != 0u || i >= s->n - 2u)) {
            printf("  %4u %3X %3X %3X %02X %-12s | %04X %04X %04X %04X %04X | %04X %04X %04X %04X %04X "
                   "| %4u %4u %4u %4u %4u | %4u %4u %4u %4u %4u\n",
                   i, e->ph[0], e->ph[1], e->ph[2], e->st, why_str(e->why),
                   e->wb[0], e->wb[1], e->wb[2], e->wb[3], e->wb[4],
                   e->live[0], e->live[1], e->live[2], e->live[3], e->live[4],
                   e->est[0], e->est[1], e->est[2], e->est[3], e->est[4],
                   sr->cur[0], sr->cur[1], sr->cur[2], sr->cur[3], sr->cur[4]);
        }
    }
    GCHECK(armed == s->n, "slot %u: every recorded pass would-armed (%u of %u)", k, armed, s->n);
    GCHECK(bad_wb == 0u, "slot %u: %u would-be words != the shadow's current / 6", k, bad_wb);
    GCHECK(bad_live == 0u, "slot %u: %u live staged words left the P content (G1 applies nothing)", k, bad_live);
    GCHECK(est_nz > 0u, "slot %u: the estimate 0x93394 never recorded", k);
}

/* applied runs: the staged words vs the shadow and the P content after lever P */
static void check_applied(gr_t *g)
{
    const uint16_t P[5] = { 8500u, 8500u, 500u, 8500u, 8500u };
    unsigned lagD = 0xFFFFu, lagR = 0xFFFFu;
    for (unsigned i = 1u; i < g_tk_n; i++) {
        const tick_t *t = &g_tk[i], *q = &g_tk[i - 1u];
        /* lever P after D / R: the first tick the staged words read the P content */
        if (t->lev == 1u && (q->lev == 4u || q->lev == 2u)) {
            unsigned j = i;
            for (; j < g_tk_n && j < i + 20u; j++) {
                bool pc = true;
                for (uint8_t ch = 0u; ch < 5u; ch++) {
                    if (absd(sp_of(g_tk[j].st[ch]), P[ch]) > 6u) {
                        pc = false;
                    }
                }
                if (pc) {
                    break;
                }
            }
            if (q->lev == 4u) { lagD = j - i; } else { lagR = j - i; }
        }
        /* applied on the previous tick: the staged words carry the shadow's
         * previous-tick current (the F stage runs after the 10 ms task) */
        if (q->applied && t->applied) {
            for (uint8_t ch = 0u; ch < 5u; ch++) {
                g->follow_n++;
                if (absd(sp_of(t->st[ch]), q->sh[ch]) > 6u) {
                    g->follow_bad++;
                }
            }
            /* the fixed channels: D → A D E, R → A C E at 8500 */
            {
                const uint8_t fixm = (t->lev == 4u) ? 0x0Bu : 0x19u;
                for (uint8_t ch = 0u; ch < 5u; ch++) {
                    if (((fixm >> ch) & 1u) != 0u && absd(sp_of(t->st[ch]), 8500u) > 6u) {
                        g->fix_moves++;
                    }
                }
            }
        }
    }
    g->lagP_D = lagD;
    g->lagP_R = lagR;
}

/* one run in a fresh process; the result struct comes back by pipe */
static void run_fresh_g(gr_t *g, bool verbose)
{
    int fd[2];
    pid_t pid;
    if (pipe(fd) != 0) {
        perror("pipe");
        exit(2);
    }
    fflush(stdout);
    pid = fork();
    if (pid == 0) {
        close(fd[0]);
        run_g(g);
        if (g->bridge) {
            const volatile gsb_t *b = gs_bridge_state();
            const volatile gsb_win_t *w = gs_bridge_win();
            printf("== %s: 'U' F%u, mode 1 F%u | bridge passes %u armed %u applied %u seam reads %u | "
                   "arm %u disarm %u guard %u | agree %u disagree %u (last %u) | win acc %u ms commits %u "
                   "latch %02X\n", g->name, g->r.u_frame, g->m1, (unsigned)b->passes, (unsigned)b->armed_n,
                   (unsigned)b->applied_n, (unsigned)b->take_n, b->arm_ev, b->disarm_ev, b->guard_n,
                   (unsigned)b->agree_n, (unsigned)b->disagree_n, (unsigned)b->last_dis,
                   (unsigned)w->acc_ms, w->commits, w->latch);
            printf("   mode-2 witness: req %u app %u live os %u e9ff %u eng %u/st %u tur %u/st %u sup %u "
                   "n %u n2 %u first2 %u chg %u\n", b->m2.req, b->m2.app, b->m2.live_os, b->m2.e9ff,
                   b->m2.eng, b->m2.eng_st, b->m2.tur, b->m2.tur_st, b->m2.sup, (unsigned)b->m2.n,
                   (unsigned)b->m2.n2, (unsigned)b->m2.first2, (unsigned)b->m2.chg);
            if (g->info) {
                for (uint8_t k = 0u; k < 2u; k++) {
                    const volatile gsb_slot_t *s = &b->slot[k];
                    uint16_t wor = 0u;
                    unsigned armed = 0u;
                    for (uint8_t i = 0u; i < s->n; i++) {
                        wor = (uint16_t)(wor | gs_bridge_rec(k, i)->why);
                        armed += gs_bridge_rec(k, i)->st & 1u;
                    }
                    printf("   (info) slot %u: armed %u of %u passes, reasons seen %s, arm_off %u (%s)\n", k,
                           armed, s->n, why_str(wor), s->arm_off, why_str(s->off_why));
                }
                g->fails = 0u;
                if (write(fd[1], g, sizeof *g) != (ssize_t)sizeof *g) {
                    _exit(3);
                }
                fflush(stdout);
                _exit(0);
            }
            check_slot(0u, GS_LIVE_DIR != 0, verbose);
            check_slot(1u, GS_LIVE_DIR != 0, verbose);
            GCHECK(b->guard_n == 0u && w->latch == 0u && w->commits == 2u,
                   "guard %u latch %02X commits %u", b->guard_n, w->latch, w->commits);
            GCHECK(b->arm_ev == 2u && b->disarm_ev == 2u, "arm %u / disarm %u events (want 2 / 2)",
                   b->arm_ev, b->disarm_ev);
            GCHECK(b->m2.req == 2u && b->m2.first2 != 0u && b->m2.live_os == 1u,
                   "mode-2 witness: req %u first2 %u (engine 700 valid from power-on)", b->m2.req,
                   (unsigned)b->m2.first2);
#if GS_LIVE_DIR == 0
            GCHECK(b->applied_n == 0u && b->take_n == 0u, "G1: applied %u seam %u",
                   (unsigned)b->applied_n, (unsigned)b->take_n);
#else
            check_applied(g);
            printf("   applied: staged follows the shadow (1 pass lag) %u of %u bad; fixed-channel moves %u; "
                   "lever P → P content D %u / R %u ticks; law modes != 3 %u\n", g->follow_bad, g->follow_n,
                   g->fix_moves, g->lagP_D, g->lagP_R, g->modes_bad);
            GCHECK(b->applied_n > 0u && b->take_n > 0u, "applied %u seam %u", (unsigned)b->applied_n,
                   (unsigned)b->take_n);
            GCHECK(g->follow_n > 0u && g->follow_bad == 0u, "staged vs shadow: %u bad of %u",
                   g->follow_bad, g->follow_n);
            GCHECK(g->fix_moves == 0u, "a fixed channel moved (%u)", g->fix_moves);
            GCHECK(g->lagP_D <= 2u && g->lagP_R <= 2u, "P content after lever P: D %u R %u ticks",
                   g->lagP_D, g->lagP_R);
            GCHECK(g->modes_bad == 0u, "law modes != 3 on A..Sys: %u", g->modes_bad);
#endif
        }
        g->fails = (unsigned)g_fails;
        if (write(fd[1], g, sizeof *g) != (ssize_t)sizeof *g) {
            _exit(3);
        }
        fflush(stdout);
        _exit(0);
    }
    close(fd[1]);
    {
        size_t got = 0u;
        while (got < sizeof *g) {
            const ssize_t r = read(fd[0], (char *)g + got, sizeof *g - got);
            if (r <= 0) {
                break;
            }
            got += (size_t)r;
        }
        close(fd[0]);
        waitpid(pid, NULL, 0);
        if (got != sizeof *g) {
            printf("FAIL child run %s incomplete\n", g->name);
            g_fails++;
        }
    }
}

static gr_t g_a, g_b;

/* THE LIVE GOLDEN (G4a, 2026-11-19, ~/tcu_sweep/g4a_release_port.md): the
 * per-frame live identity hash (every bus-A word in wire order, the cell
 * window without the shadow's own RAM, the park demands) of each run,
 * against the trace of HEAD bccfb9d (r04j, the instant-release stand-in;
 * the golden file was written by this binary built at that HEAD). argv[1]
 * = the golden file; GSB_GOLD_WRITE=1 writes it instead of comparing. */
static unsigned gold_check(const char *path, const gr_t *g, unsigned k)
{
    static FILE *wf;
    static char *buf;
    static size_t blen;
    const bool wr = getenv("GSB_GOLD_WRITE") != NULL;
    unsigned diff = 0u, first = 0u, post = 0u, pfirst = 0u;
    if (path == NULL) {
        return 0u;
    }
    if (wr) {
        if (wf == NULL) {
            wf = fopen(path, "w");
            if (wf == NULL) {
                perror(path);
                exit(2);
            }
        }
        for (unsigned f = 0u; f < GFRAMES; f++) {
            fprintf(wf, "%u %u %016llx\n", k, f + 1u, (unsigned long long)g->h[f]);
        }
        fflush(wf);
        return 0u;
    }
    if (buf == NULL) {
        FILE *rf = fopen(path, "r");
        long n;
        if (rf == NULL) {
            perror(path);
            exit(2);
        }
        fseek(rf, 0, SEEK_END);
        n = ftell(rf);
        fseek(rf, 0, SEEK_SET);
        buf = malloc((size_t)n + 1u);
        blen = fread(buf, 1u, (size_t)n, rf);
        buf[blen] = 0;
        fclose(rf);
    }
    {
        unsigned found = 0u;
        char *p = buf;
        while (p != NULL && *p != 0) {
            unsigned kk, ff;
            unsigned long long h;
            if (sscanf(p, "%u %u %llx", &kk, &ff, &h) == 3 && kk == k && ff >= 1u && ff <= GFRAMES) {
                found++;
                if (g->h[ff - 1u] != (uint64_t)h) {
                    if (g->gold_full || ff < g->c3_frame) {
                        if (diff == 0u) {
                            first = ff;
                        }
                        diff++;
                    } else {
                        post++;
                        if (pfirst == 0u) {
                            pfirst = ff;
                        }
                    }
                }
            }
            p = strchr(p, '\n');
            if (p != NULL) {
                p++;
            }
        }
        printf("   GOLDEN vs HEAD bccfb9d (r04j): run %u '%s': %u of %u frames compared (%s), %u differ "
               "(first F%u); the R commit after the release at F%u, %u frames differ from it on (first F%u)\n",
               k, g->name, found, GFRAMES, g->gold_full ? "every frame" : "the frames before the R commit",
               diff, first, g->c3_frame, post, pfirst);
        if (!g->gold_full && (g->c3_frame == 0u || (pfirst != 0u && pfirst < g->c3_frame))) {
            diff++;
        }
        if (found != GFRAMES) {
            diff += GFRAMES - found;
        }
    }
    return diff;
}

int main(int argc, char **argv)
{
    const char *gold = (argc > 1) ? argv[1] : NULL;
    (void)gold;
#if GS_LIVE_DIR == 0
    /* (1) identity, (2) the recorder — real feedback (the bench since r04f) */
    g_a = (gr_t){ .name = "live, no bridge", .bridge = false, .real = true };
    g_b = (gr_t){ .name = "live + G1 bridge", .bridge = true, .real = true };
    run_fresh_g(&g_a, false);
    run_fresh_g(&g_b, true);
    g_fails += (int)g_b.fails;
    {
        unsigned diff = 0u, first = 0u;
        for (unsigned f = 0u; f < GFRAMES; f++) {
            if (g_a.h[f] != g_b.h[f]) {
                if (diff == 0u) {
                    first = f + 1u;
                }
                diff++;
            }
        }
        printf("(1) LIVE IDENTITY over the gs-check (P D P R P, brake + engine 700): %u frames, %u differ "
               "(first F%u); bridge would-armed %u passes\n", GFRAMES, diff, first, (unsigned)g_b.armed_n);
        GCHECK(diff == 0u, "(1) the live path differs with the G1 bridge in %u frames", diff);
        GCHECK(g_b.armed_n > 150u, "(1) the bridge must have would-armed (%u)", (unsigned)g_b.armed_n);
        GCHECK(g_a.u == g_b.u && g_a.m1 == g_b.m1, "(1) 'U' / mode-1 frames differ");
    }
    if (g_fails == 0) {
        printf("test_gs_bridge_chain: ok\n");
    }
    return g_fails != 0;
#else
    /* THE G2 GATE (host only): the bridged currents APPLIED */
    static gr_t runs[] = {
        { .name = "G2 APPLIED real SAT0",         .bridge = true, .real = true, .fc_mode = 8u },
        { .name = "G2 APPLIED real SAT0 par1",    .bridge = true, .real = true, .parity = 1u, .fc_mode = 8u },
        { .name = "G2 APPLIED stand-in SAT0",     .bridge = true, .real = false, .fc_mode = 8u },
        /* G4a: P held 0.3 s only (lever R 60 frames after lever P): the
         * shadow's release (120 passes) is interrupted by the R request */
        { .name = "G2 APPLIED real SAT0 shortP",  .bridge = true, .real = true, .kR = K_P1 + 60u,
          .fc_mode = 8u },
        /* G4a: P held 2.3 s (D 1.5 s, R 2 s): the shadow's release (1.2 s)
         * AND B's release ramp in its fill FSM (0.8 s) end before the R
         * commit — the bench gs-check's shape (--hold >= 2) */
        { .name = "G2 APPLIED real SAT0 longP",   .bridge = true, .real = true, .kP1 = 360u,
          .kR = 820u, .kP2 = 1220u, .fc_mode = 8u, .gold_full = true },
        { .name = "G2 APPLIED real SAT0 longP par1", .bridge = true, .real = true, .parity = 1u,
          .kP1 = 360u, .kR = 820u, .kP2 = 1220u, .fc_mode = 8u, .gold_full = true },
        /* the bench tool's default --hold 2.0: P held exactly 2 s */
        { .name = "G2 APPLIED real SAT0 P 2.0 s",  .bridge = true, .real = true, .kP1 = 360u,
          .kR = 760u, .kP2 = 1160u, .fc_mode = 8u, .gold_full = true },
        /* the 10 ms turn-on / step saturation stress (the temperature dry-run's
         * D / C 0x0010 sensitivity): from power-on (A's boot fill sees it) and
         * from the lever D step only (the engagement's own steps) */
        { .name = "G2 APPLIED real SAT10 from boot (info)", .bridge = true, .real = true, .sat_ms = 10u,
          .info = true, .fc_mode = 8u },
        { .name = "G2 APPLIED real SAT10 from lever D (info)", .bridge = true, .real = true, .sat_ms = 10u,
          .sat_from = K_D, .info = true, .fc_mode = 8u },
        { .name = "G2 APPLIED real SAT30 from lever D (info)", .bridge = true, .real = true, .sat_ms = 30u,
          .sat_from = K_D, .info = true, .fc_mode = 8u },
    };
    for (unsigned k = 0u; k < sizeof runs / sizeof runs[0]; k++) {
        gr_t *g = &runs[k];
        run_fresh_g(g, k == 0u);
        g_fails += g->info ? 0 : (int)g->fails;
        if (!g->info) {
            const unsigned d = gold_check(gold, g, k);
            GCHECK(d == 0u, "%s: the live path differs from HEAD bccfb9d in %u frames", g->name, d);
        }
        summary(&g->r);
#if defined(DIAG_TEMPS)
        printf("    bound bits OR:");
        for (uint8_t ch = 0u; ch < 10u; ch++) {
            printf(" %04X", g->r.bits_or[ch]);
        }
        printf("\n    tested min margins (0.1 mA): ");
        for (uint8_t ch = 0u; ch < 5u; ch++) {
            printf(" ch%u UB %d (U+%u est %u/UB %u, n %u) LB %d (U+%u est %u/LB %u, n %u);", ch,
                   g->r.tup_n[ch] ? (int)g->r.mup[ch] : -1, g->r.mup_f[ch], g->r.mup_est[ch], g->r.mup_b[ch],
                   g->r.tup_n[ch], g->r.tlo_n[ch] ? (int)g->r.mlo[ch] : -1, g->r.mlo_f[ch],
                   g->r.mlo_est[ch], g->r.mlo_b[ch], g->r.tlo_n[ch]);
        }
        printf("\n");
#endif
        if (!g->info) {
            const run_r_t *r = &g->r;
            GCHECK(r->u_frame != 0u, "%s: no 'U'", g->name);
            GCHECK(r->fw_set_events == 0u && r->switched_off == 0u && r->gfail_tot == 0u &&
                   r->gpend_tot == 0u && r->fatal_tot == 0u,
                   "%s: fault %u switch-off %u DSM fail %u pend %u would-reset %u", g->name,
                   r->fw_set_events, r->switched_off, r->gfail_tot, r->gpend_tot, r->fatal_tot);
#if defined(DIAG_TEMPS)
            for (uint8_t ch = 0u; ch < 10u; ch++) {
                GCHECK(r->bits_or[ch] == 0u, "%s: ch %u bound bits %04X", g->name, ch, r->bits_or[ch]);
            }
#endif
        } else {
            printf("    (informational) fault-word OR D %04X C %04X B %04X\n", g->r.fw_or[1], g->r.fw_or[4],
                   g->r.fw_or[2]);
        }
    }
    if (g_fails == 0) {
        printf("test_gs_bridge_g2: ok (THE G2 GATE PASSES on the host model)\n");
    } else {
        printf("test_gs_bridge_g2: THE G2 GATE FAILS (%d)\n", g_fails);
    }
    return g_fails != 0;
#endif
}
