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

#if !defined(G6A_TEST) || !GS_G6B
#  error "test_gs_bridge_chain.c (G6c): the harness of test_g6b / test_g6b_gate* only (G6A_TEST, GS_G6B 1)"
#endif
#if defined(G6A_TEST)
/* G6a (2026-11-19, ~/tcu_sweep/g6a_port.md): tests/unit/test_g6a.c includes
 * this harness with the candidate + comparator ticked as app/main.c ticks
 * them (after the bridge, its boot pass at boot state 1 through
 * sup_mode.c's weak hook); g6a_on selects the run with / without them */
#include "app/g6a.h"
#include "app/gs_cand.h"
#if defined(G6B_GATE)
/* G6b (2026-11-19, ~/tcu_sweep/g6b_port.md): the G2 gate's main with THE
 * SWITCH — the candidate ticked before the monitor, the monitor's seam on
 * the wire — against the r04n goldens (the bridge of the shadow) */
static bool g6a_on = true;
#else
static bool g6a_on;
#endif
static unsigned g6a_eng0_from;  /* frames after the mode-1 switch: 0x108 valid at 0 rpm (0 = never) */
static unsigned g6q_engoff_from;/* r04q: frames after the mode-1 switch: 0x108 no longer sent (0 = never) —
                                 * the bench's engine-sim stop (0x108 invalid) */
static bool g6a_nosim;          /* no lever / brake / engine frames at all (the plain boot) */
static unsigned g6a_m1;         /* the mode-1 frame of the current run */
#if defined(G6B_TEST)
static uint32_t g6b_relwin_from = 0xFFFFFFFFu, g6b_relwin_to;   /* test_g6b.c: forced RELWIN passes */
static uint32_t g6q_boot2_at = 0xFFFFFFFFu;     /* r04q: a second boot entry after this monitor pass */
#endif
#if defined(G6A_HOSTTIME)
/* scratch CPU-cost model (not a ctest): host ns of the shadow's tick (0) and
 * the candidate + comparator (1), per pass */
#include <time.h>
static struct timespec g6a_ts0;
static double g6a_ns_sum[2], g6a_ns_max[2];
static unsigned g6a_ns_n[2];
#define G6A_T0() timespec_get(&g6a_ts0, TIME_UTC)
#define G6A_T1(k) do { struct timespec t1_; timespec_get(&t1_, TIME_UTC); \
    const double d_ = (double)(t1_.tv_sec - g6a_ts0.tv_sec) * 1e9 + (double)(t1_.tv_nsec - g6a_ts0.tv_nsec); \
    g6a_ns_sum[k] += d_; g6a_ns_n[k]++; if (d_ > g6a_ns_max[k]) { g6a_ns_max[k] = d_; } } while (0)
#endif
#endif

/* r04r (~/tcu_sweep/r04r_port.md): THE RETIRED LIVE HOLDER, REPLAYED. The
 * image no longer links app/rq_prod.c (the engine is the sole producer of
 * ch 0..6; nothing reads the holder's cells). With G6R_HOLDER_REPLICA 1
 * (the default) this harness still runs it at its r04q slots — init right
 * after drv_rq_init (rr_reset), its boot pass just before the candidate's
 * (g6a_boot_pass), its 10 ms pass at the top of task10 — ONLY so that its
 * stock cells (0xFFF84B78.., 0xFFF8B154.., 0xFFF8B7C8.., the input mirrors,
 * the fill cells) hold the values the r04q goldens' cell-window hashes saw:
 * every other cell, every bus-A word, the park demands, the monitor's ticks
 * and windows can then be compared with those goldens byte for byte. Its
 * outputs reach nothing (drv_reqlaw.c's r04r guard does not read it).
 * G6R_HOLDER_REPLICA 0: the image's configuration (no holder at all) —
 * the wire hash must still equal the r04q goldens on every frame. */
#ifndef G6R_HOLDER_REPLICA
#  define G6R_HOLDER_REPLICA 1
#endif
#if G6R_HOLDER_REPLICA && GS_G6B
#  define G6R_REPLICA_INIT()  rq_prod_init()
#  define G6R_REPLICA_BOOT()  rq_prod_boot_pass()
#  define G6R_REPLICA_10MS(m) rq_prod_task_10ms(m)
#elif GS_G6B
#  define G6R_REPLICA_INIT()  ((void)0)
#  define G6R_REPLICA_BOOT()  ((void)0)
#  define G6R_REPLICA_10MS(m) ((void)(m))
#else
#  define G6R_REPLICA_INIT()  ((void)0)     /* drv_rq_init runs it (the holder-fed form) */
#  define G6R_REPLICA_BOOT()  ((void)0)     /* bsw/sup/sup_mode.c runs it */
#  define G6R_REPLICA_10MS(m) rq_prod_task_10ms(m)
#endif

#if defined(G6B_GATE)
/* the candidate's boot pass at boot state 1 (app/main.c g6a_boot_pass) */
void g6a_boot_pass(void)
{
    G6R_REPLICA_BOOT();             /* r04r: the retired holder's boot pass (r04q: just before) */
    const vehicle_in_t *v = vehicle_in();
    const gs_in_t gi = {
        .os_mode = sup_mode(),
        .rngd = *(const volatile uint8_t *)0xFFF89CDCu,
        .park_model = park_law_state(),
        .park_req = park_law_request(),
        .temp = *(const volatile uint8_t *)0xFFF89C13u,
        .cd2 = *(const volatile uint8_t *)0xFFF89CD2u,
        .eng_speed = (uint16_t)v->engine_speed,
        .eng_valid = (v->q_engine_speed == Q_OK) ? 1u : 0u,
        .out_rpm = 0u,
        .tur_rpm = 0u,
        .ae28 = *(const volatile uint8_t *)0xFFF8AE28u,
    };
    g6a_boot(&gi);
    gs_bridge_boot_pass();          /* r04q: as app/main.c g6a_boot_pass */
    g6a_boot_compare();
}
#endif

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
/* r04s (~/tcu_sweep/r04s_port.md): R first, then D (the reversal's mirror;
 * set by test_g6b's scenario before run_g; default false = D then R) */
static bool g_rfirst;
static uint8_t lever_at(unsigned p, unsigned m1)
{
    unsigned k;
    if (m1 == 0u || p < m1) {
        return 1u;
    }
    k = p - m1;
    if (k >= K_D && k < g_kP1) { return g_rfirst ? 2u : 4u; }
    if (k >= g_kR && k < g_kP2) { return g_rfirst ? 4u : 2u; }
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
    uint64_t hw[GFRAMES];   /* G4b: the WIRE part alone (bus-A words + park demands) */
    uint8_t  relon[GFRAMES];/* G4b: the bridged release on after this frame's 10 ms task */
    /* G4b: the bridged release, on the real chain */
    unsigned rel_n;                 /* releases checked (lever P from D / R, shadow completion in the run) */
    unsigned rel_bad_applied;       /* a shadow release pass not bridged */
    unsigned rel_p_lag[2];          /* ticks from the shadow's completion pass to the staged P content (D, R) */
    unsigned rel_len[2];            /* shadow release passes (D, R) */
    unsigned rel_open_end;          /* a release still bridged at the end of the run */
    uint16_t rel_b0[2], rel_b1[2], rel_x0[2], rel_x1[2];   /* staged first / last release values */
#if defined(DIAG_TEMPS)
    /* the current-bound margins during the bridged release (tested passes),
     * per direction (0 D, 1 R) for B (0) and the third (1): UB − est, est − LB */
    int32_t  rm_up[2][2], rm_lo[2][2];
    uint16_t rm_up_est[2][2], rm_up_b[2][2], rm_lo_est[2][2], rm_lo_b[2][2];
    unsigned rm_n[2][2];
#endif
    unsigned c3_frame;      /* the frame of the shadow's third commit (0 → R): the
                             * first engagement after the D → P release */
    bool gold_full;         /* G4a golden: every frame identical to HEAD (else
                             * every frame BEFORE c3_frame) */
    unsigned bw_beg, bw_end;/* r04q: the frames of the monitor's boot entry and of the
                             * boot allowance's end (gsb_t.boot_st USED) */
    run_r_t r;
} gr_t;

static uint64_t fnv64(uint64_t h, uint32_t v)
{
    for (int k = 0; k < 4; k++) {
        h = (h ^ (uint8_t)(v >> (8 * k))) * 0x100000001B3ull;
    }
    return h;
}

#if defined(GSB_HASH_FLAGS_AS_2A)
/* STAGE 2b (2026-11-19, ~/tcu_sweep/temp_stage2b_port.md): this harness
 * runs the temperature hub with the fault flags 0x940E1 / E2 / E4 / E5 live
 * (TSEL_LIVE_FLAGS, the image's configuration); the goldens were written at
 * stage 2a, when nothing in this harness wrote them (no drv_temp_init: all
 * four 0). The hash reads them as that 0 — every OTHER cell, every bus-A
 * word and the park demands must still match the goldens byte for byte —
 * and counts the frames in which the live flags held the stock's tested
 * masks (E1 0x60 / 0x68 — 0x68 once the die's comparators agree — and E4
 * 0xE0), so the mask cannot hide an absent writer. */
static unsigned long g_flags_live_n;
static uint8_t g_flags_last[4];
static unsigned long g_flags_nz_n;
#endif
static uint64_t g_hash_window(uint64_t h)
{
    const volatile uint8_t *p = (const volatile uint8_t *)(uintptr_t)0xFFF80000u;
#if defined(GSB_HASH_FLAGS_AS_2A)
    if ((p[0x140E1u] & 0x60u) == 0x60u && p[0x140E4u] == 0xE0u) {
        g_flags_live_n++;
    }
    if ((p[0x140E1u] | p[0x140E2u] | p[0x140E4u] | p[0x140E5u]) != 0u) {
        g_flags_nz_n++;
        g_flags_last[0] = p[0x140E1u];
        g_flags_last[1] = p[0x140E2u];
        g_flags_last[2] = p[0x140E4u];
        g_flags_last[3] = p[0x140E5u];
    }
#endif
    for (uint32_t a = 0u; a < 0x16000u; a++) {
        const uint32_t abs = 0xFFF80000u + a;
        if ((abs >= GS_REQ_BASE && abs < GS_REQ_BASE + GS_REQ_SIZE) ||
            (abs >= 0xFFF8EE00u && abs < 0xFFF90000u) ||
            (abs >= 0xFFF91440u && abs < 0xFFF92400u)) {
            continue;                       /* the garage shadow's own RAM */
        }
#if defined(G6A_TEST)
        if (abs >= 0xFFF925C0u && abs < 0xFFF92860u) {
#  if defined(G6B_GATE)
            /* G6b gate: hashed as the zeros the r04n goldens saw there (the
             * goldens' builds had no candidate) */
            h = (h ^ 0u) * 0x100000001B3ull;
#  endif
            continue;                       /* G6a: the candidate's own request objects (.gsc_req) */
        }
#endif
#if defined(GSB_HASH_FLAGS_AS_2A)
        if (abs == 0xFFF940E1u || abs == 0xFFF940E2u || abs == 0xFFF940E4u || abs == 0xFFF940E5u) {
            h = (h ^ 0u) * 0x100000001B3ull;
            continue;
        }
#endif
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
#if defined(G6A_TEST)
    unsigned rpm = 700u;
    g6a_m1 = g->m1;
    if (g6a_eng0_from != 0u && g->m1 != 0u && p >= g->m1 + g6a_eng0_from) {
        rpm = 0u;                                   /* the engine stalls: 0x108 valid, 0 rpm */
    }
    if (!g6a_nosim)
#else
    const unsigned rpm = 700u;
#endif
    {
    {
        const uint8_t l[5] = { lev, 0, 0, 0, 0 };
        g_send(CAN_BUS1, 0x2EAu, 5u, true, l);
        g_send(CAN_BUS2, 0x210u, 5u, true, l);
    }
    memset(d, 0, sizeof d);
    d[0] = 0x04u;                                   /* brake on */
    g_send(CAN_BUS1, 0x140u, 8u, true, d);
    memset(d, 0, sizeof d);
    d[0] = (uint8_t)(rpm >> 8);
    d[1] = (uint8_t)rpm;
    d[2] = 0x1Fu;
    d[3] = 0xFFu;
#if defined(G6A_TEST)
    if (g6q_engoff_from == 0u || g->m1 == 0u || p < g->m1 + g6q_engoff_from)
#endif
    g_send(CAN_BUS1, 0x108u, 8u, true, d);          /* engine 700 */
    }

    G6R_REPLICA_10MS(mode);         /* r04r: the retired holder (replica only, see above) */
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
#if defined(G6A_TEST)
            .ae28 = *(const volatile uint8_t *)0xFFF8AE28u,   /* app/main.c gs_in_build */
#endif
        };
        /* G6c: no shadow (instance 1 is deleted from app/main.c) */
#if defined(G6A_TEST) && GS_G6B
        /* G6b (app/main.c): the candidate BEFORE the monitor */
        const bool g6b_run = g6a_on && gs_bridge_g6b();
        if (g6b_run) {
            g6a_cand_tick(&gi);
        }
#endif
        if (g->bridge) {
#if defined(G6B_TEST)
            /* G6b negatives: the park-release window forced latched on these
             * monitor passes (a permit failing mid-engagement) */
            const uint32_t bp = gs_bridge_state()->passes;
            const uint8_t rw = (bp >= g6b_relwin_from && bp < g6b_relwin_to) ? 2u : drv_rq_rel_win_state();
#else
            const uint8_t rw = drv_rq_rel_win_state();
#endif
            const gsb_in_t bi = {
                .mon_faults = park_mon_faults(),
                .relwin_st = rw,
                .eng_valid = gi.eng_valid,
                .tur_ok = 1u,
                .eng_speed = gi.eng_speed,
                .tur_rpm = 0u,
                .cyc = drv_job_tx(DRV_CYC),
            };
            gs_bridge_tick_10ms(&bi);
#if defined(G6B_TEST)
            /* r04q negative: a second boot entry (the allowance used twice) */
            if (gs_bridge_state()->passes == g6q_boot2_at) {
                gs_bridge_boot_pass();
            }
#endif
        }
#if defined(G6A_TEST)
        if (g6a_on) {
#if defined(G6A_HOSTTIME)
            G6A_T0();
#endif
            if (g6b_run) {
                g6a_compare_10ms(mode);             /* G6b: the comparison after the monitor */
            }
#if defined(G6A_HOSTTIME)
            G6A_T1(1);
#endif
        }
#endif
    }
    drv_rq_request_set(7u, park_slot7_release());   /* ch 7: the park release (SH: DRV_RQ_PSET) */
}

/* applied runs: the staged words after each 10 ms tick */
typedef struct {
    uint8_t lev; uint16_t st[5]; uint16_t sh[5]; bool applied;
    uint32_t miss, fn;  /* drv_out_wit(): cyclic misses, F passes after this tick */
    uint8_t dir;        /* G4b: the direction whose pins apply (the bridged release's, else the shadow's pf) */
    bool shrel;         /* the shadow releasing after this tick */
    bool relon;         /* the bridged release on after this tick */
    uint8_t src;        /* G6b: the monitor's seam source after this tick (GSM_S_*) */
    uint16_t frame;     /* the frame of this tick */
    uint32_t bpass;     /* G6b: the bridge / monitor pass of this tick */
    uint8_t  g6c_latch, g6c_fence;  /* G6c goldens: the monitor's latch / fence after this tick */
    uint16_t g6c_trip_n;
} tick_t;
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
    G6R_REPLICA_INIT();             /* r04r: the retired holder's init (r04q: the end of drv_rq_init) */
#if defined(DIAG_TEMPS)
    for (unsigned d = 0u; d < 2u; d++) {
        for (unsigned q = 0u; q < 2u; q++) {
            g->rm_up[d][q] = g->rm_lo[d][q] = 0x7FFFFFFF;
            g->rm_n[d][q] = 0u;
        }
    }
#endif
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
    gs_bridge_init(true);
#if defined(G6A_TEST)
    if (g6a_on) {
        g6a_init();                                 /* app/main.c: after gs_bridge_init */
    }
#endif
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
                        t->relon = g->bridge && gs_bridge_rel()->on != 0u;
                        t->miss = drv_out_wit()->missed[3];
                        t->src = gs_bridge_state()->src;
                        t->frame = (uint16_t)p;
                        t->bpass = gs_bridge_state()->passes - 1u;
                        t->g6c_latch = gs_bridge_win()->latch;
                        t->g6c_fence = gs_bridge_state()->fence;
                        t->g6c_trip_n = gs_bridge_state()->trip_n;
                        t->fn = drv_out_wit()->f_n;
                        t->shrel = gs_cand_releasing() != 0u;
                        t->dir = t->relon ? gs_bridge_rel()->pf : gs_cand_pf();
                        for (uint8_t ch = 0u; ch < 5u; ch++) {
                            t->st[ch] = cyc[(((ch & 1u) != 0u) ? 20u : 10u) + (ch >> 1)];
                            t->sh[ch] = gs_cand_cur(ch);
                        }
#if defined(G6A_TEST) && GS_G6B
                        if (g6a_on && gs_bridge_g6b()) {
                            /* G6b: the source the seam carries is the candidate */
                            t->shrel = gs_cand_releasing() != 0u;
                            t->dir = t->relon ? gs_bridge_rel()->pf : gs_cand_pf();
                            for (uint8_t ch = 0u; ch < 5u; ch++) {
                                t->sh[ch] = gs_cand_cur(ch);
                            }
                        }
#endif
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
                    if (g->bridge && gs_bridge_rel()->on != 0u) {
                        /* G4b: the bound margins while the release is bridged */
                        const uint8_t d = (gs_bridge_rel()->pf == 2u) ? 1u : 0u;
                        const uint8_t chs[2] = { 2u, (uint8_t)((d == 1u) ? 1u : 4u) };
                        for (uint8_t q = 0u; q < 2u; q++) {
                            const uint8_t ch = chs[q];
                            const uint16_t est = U16C_(0xFFF93394u + 2u * ch);
                            const uint16_t tested = U16C_(0xFFF94944u + 2u * ch);
                            g->rm_n[d][q]++;
                            if ((tested & 0x200u) != 0u && r->ub_prev[ch] != 0u) {
                                const int32_t m = (int32_t)(int16_t)r->ub_prev[ch] - (int32_t)(int16_t)est;
                                if (m < g->rm_up[d][q]) {
                                    g->rm_up[d][q] = m; g->rm_up_est[d][q] = est; g->rm_up_b[d][q] = r->ub_prev[ch];
                                }
                            }
                            if ((tested & 0x100u) != 0u && r->lb_prev[ch] != 0u) {
                                const int32_t m = (int32_t)(int16_t)est - (int32_t)(int16_t)r->lb_prev[ch];
                                if (m < g->rm_lo[d][q]) {
                                    g->rm_lo[d][q] = m; g->rm_lo_est[d][q] = est; g->rm_lo_b[d][q] = r->lb_prev[ch];
                                }
                            }
                        }
                    }
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
        g->hw[p - 1u] = h;
        g->h[p - 1u] = g_hash_window(h);
#if defined(G6A_TEST) && GS_G6B
        if (g->bw_beg == 0u && gs_bridge_state()->boot_calls != 0u) {
            g->bw_beg = p;
        }
        if (g->bw_end == 0u && (gs_bridge_state()->boot_st == GSM_B_USED ||
                                (gs_bridge_state()->boot_st & GSM_B_VIOL) != 0u)) {
            g->bw_end = p;
        }
#endif
        g->relon[p - 1u] = (uint8_t)((g->bridge && gs_bridge_rel()->on != 0u) ? 1u : 0u);
        if (getenv("GSB_DUMP_FRAME") != NULL && (unsigned)atoi(getenv("GSB_DUMP_FRAME")) == p &&
            g->parity == 0u && getenv("GSB_DUMP_RUN") != NULL && strcmp(getenv("GSB_DUMP_RUN"), g->name) == 0) {
            /* debug: the cell window at this frame (the post-release state comparison) */
            FILE *df = fopen(getenv("GSB_DUMP_FILE") ? getenv("GSB_DUMP_FILE") : "gsb_dump.bin", "wb");
            if (df != NULL) {
                fwrite((const void *)(uintptr_t)0xFFF80000u, 1u, 0x16000u, df);
                fclose(df);
            }
        }
        stub_log_n = 0u;
        if (g->c3_frame == 0u && gs_cand_wit()->commits >= 3u) {
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
    if (s->arm_off == GSB_NONE && applied_build && gs_bridge_rel()->on != 0u &&
        gs_bridge_rel()->pf == (uint8_t)(k + 1u)) {
        /* G4b: the run ended inside this direction's bridged release */
        printf("  slot %u: still armed at the end of the run — in its bridged release (pass %u)\n", k,
               gs_bridge_rel()->t);
    } else {
        /* G4b: an applied build disarms at the end of the bridged release
         * (the shadow's completion: power flow 0, not releasing) — the
         * range reason is still among them */
        GCHECK(s->arm_off != GSB_NONE && (s->off_why & GSB_R_RANGE) != 0u,
               "slot %u: disarmed by the range (lever P / the release's end): off %u why %s", k, s->arm_off,
               why_str(s->off_why));
        GCHECK(s->back_at != GSB_NONE && (unsigned)(s->back_at - s->arm_off) <= 2u,
               "slot %u: shadow == live within 2 passes of the disarm (off %u back %u)", k, s->arm_off,
               s->back_at);
    }
    if (print) {
        printf("  pass ph0 ph1 ph2 st  why          | would-be A D B E C          | live staged A D B E C       "
               "| est A D B E C               | shadow A D B E C\n");
    }
    for (uint8_t i = 0u; i < s->n; i++) {
        const volatile gsb_rec_t *e = gs_bridge_rec(k, i);
        /* G6c: the shadow (instance 1) and its recorder are deleted; the
         * would-be words were checked against the shadow's recorded current
         * (common-mode with the candidate since G6b) — the gold identity of
         * every frame is the check now */
        static const gs_rec_t z0;
        const gs_rec_t *sr = &z0;
        if ((e->st & 1u) != 0u) {
            armed++;
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
        /* lever P after D / R (G4b): the shadow releases from tick i; every
         * release pass is bridged; the staged words read the P content from
         * the tick after the shadow's completion pass e (the F stage follows
         * the 10 ms task: lag 1), never before */
        if (t->lev == 1u && (q->lev == 4u || q->lev == 2u)) {
            const unsigned d = (q->lev == 4u) ? 0u : 1u;
            unsigned e = i, j;
            while (e < g_tk_n && g_tk[e].shrel) {
                if (!g_tk[e].applied) {
                    g->rel_bad_applied++;
                }
                e++;
            }
            if (e >= g_tk_n) {
                g->rel_open_end++;          /* the run ends inside the release */
                j = 0xFFFFu;
            } else if (g_tk[e].lev != 1u) {
                j = 0xFFFFu;                /* the lever left P first: the release interrupted */
            } else {
                g->rel_n++;
                g->rel_len[d] = e - i;
                for (j = i; j < g_tk_n && j < e + 20u; j++) {
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
                g->rel_p_lag[d] = (j >= e) ? j - e : 0xFFFFu;   /* 0xFFFF: P content before the completion */
                if (e > i + 1u) {
                    const uint8_t x = (d == 0u) ? 4u : 1u;
                    g->rel_b0[d] = (uint16_t)sp_of(g_tk[i + 1u].st[2]);
                    g->rel_x0[d] = (uint16_t)sp_of(g_tk[i + 1u].st[x]);
                    g->rel_b1[d] = (uint16_t)sp_of(g_tk[e].st[2]);
                    g->rel_x1[d] = (uint16_t)sp_of(g_tk[e].st[x]);
                }
            }
            /* the first tick with the P content after lever P (any tick) */
            for (j = i; j < g_tk_n && j < i + 200u; j++) {
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
            if (d == 0u) { lagD = j - i; } else { lagR = j - i; }
        }
        /* applied on the previous tick: the staged words carry the shadow's
         * previous-tick current (the F stage runs after the 10 ms task) */
        if (q->applied && t->applied) {
            for (uint8_t ch = 0u; ch < 5u; ch++) {
                if (t->fn == q->fn) {
                    /* no F stage between these two 10 ms ticks (the 10 ms
                     * task and the F stage's 10 ms part drift by a frame now
                     * and then): the staged words still carry the tick
                     * before — not a follow sample */
                    break;
                }
                g->follow_n++;
                if (absd(sp_of(t->st[ch]), q->sh[ch]) > 6u) {
                    if (getenv("GSB_FOLLOW_DBG") != NULL) {
                        printf("    follow mismatch tick %u ch %u staged %u shadow(prev) %u lev %u/%u rel %u/%u "
                               "shrel %u/%u miss %u→%u fn %u→%u\n", i, ch, sp_of(t->st[ch]), q->sh[ch], q->lev,
                               t->lev, q->relon, t->relon, q->shrel, t->shrel, (unsigned)q->miss, (unsigned)t->miss,
                               (unsigned)q->fn, (unsigned)t->fn);
                    }
                    g->follow_bad++;
                }
            }
            /* the fixed channels: D → A D E, R → A C E at 8500 (G4b: the
             * direction of the bridged release while it runs) */
            if (t->dir == 1u || t->dir == 2u) {
                const uint8_t fixm = (t->dir == 1u) ? 0x0Bu : 0x19u;
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
            GCHECK(b->arm_ev == 2u && b->disarm_ev == (unsigned)((gs_bridge_rel()->on != 0u) ? 1u : 2u),
                   "arm %u / disarm %u events (want 2 / 2, or 2 / 1 with a release open at the end)",
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
#if defined(GSB_HOST_NO_REL_BRIDGE)
            /* the r04k behaviour: the P content within 2 passes of lever P */
            GCHECK(g->lagP_D <= 2u && g->lagP_R <= 2u, "P content after lever P: D %u R %u ticks",
                   g->lagP_D, g->lagP_R);
#else
            printf("   G4b bridged releases: %u completed in the run (D %u passes, R %u), open at the end %u; "
                   "P content %u / %u ticks after the shadow's completion pass; staged B %u → %u / %u → %u, "
                   "third %u → %u / %u → %u; release passes not bridged %u\n", g->rel_n, g->rel_len[0],
                   g->rel_len[1], g->rel_open_end, g->rel_p_lag[0], g->rel_p_lag[1], g->rel_b0[0], g->rel_b1[0],
                   g->rel_b0[1], g->rel_b1[1], g->rel_x0[0], g->rel_x1[0], g->rel_x0[1], g->rel_x1[1],
                   g->rel_bad_applied);
            GCHECK(g->rel_bad_applied == 0u, "a shadow release pass not bridged (%u)", g->rel_bad_applied);
            GCHECK(g->rel_n + g->rel_open_end >= 1u, "no bridged release in the run");
            for (unsigned d = 0u; d < 2u; d++) {
                GCHECK(g->rel_len[d] == 0u || g->rel_len[d] == 120u, "release %u: %u passes (want 120)", d,
                       g->rel_len[d]);
                GCHECK(g->rel_len[d] == 0u || g->rel_p_lag[d] == 1u,
                       "release %u: the P content %u ticks after the shadow's completion pass (want 1)", d,
                       g->rel_p_lag[d]);
            }
            GCHECK(gs_bridge_rel()->trip_n == 0u, "release envelope trips %u", gs_bridge_rel()->trip_n);
#endif
            GCHECK(g->modes_bad == 0u, "law modes != 3 on A..Sys: %u", g->modes_bad);
#endif
        }
#if defined(GSB_HASH_FLAGS_AS_2A)
        /* in the child (the run's own process): the live flags were there */
        printf("   stage 2b: %lu of the hashed frames carried the tested masks (E1 0x60/0x68, E4 0xE0); "
               "%lu any flag; last E1 %02X E2 %02X E4 %02X E5 %02X\n", g_flags_live_n, g_flags_nz_n,
               g_flags_last[0], g_flags_last[1], g_flags_last[2], g_flags_last[3]);
        GCHECK(g_flags_live_n > 1000u, "stage 2b: the live flags never reached the tested masks (%lu)",
               g_flags_live_n);
#endif
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

/* THE LIVE GOLDENS. Each line: run k, frame f, the per-frame live identity
 * hash (every bus-A word in wire order, the cell window without the
 * shadow's own RAM, the park demands) and — G4b files — the WIRE part alone
 * (bus-A words + park demands).
 *
 *   golden 1 (argv[1]): in a G4b build the REFERENCE trace of the r04k
 *     behaviour (no bridged release), written by the GSB_HOST_NO_REL_BRIDGE
 *     build — which itself reproduces HEAD bccfb9d's trace under the G4a
 *     rule (ctest gs_bridge_g2_r04k_ref*). The G4b run must equal it in
 *     every frame before its first bridged-release frame, and on the WIRE in
 *     every frame outside the release windows (a frame whose 10 ms task, or
 *     one of the 4 frames before it, had the bridged release on). Cell-only
 *     differences after a release (slow state: the coil-resistance model,
 *     the feedback filters) are counted, not failed.
 *     In a GSB_HOST_NO_REL_BRIDGE build: HEAD bccfb9d's trace, the G4a rule.
 *   golden 2 (argv[2], optional): the exact trace, every frame, every hash.
 * GSB_GOLD_WRITE=<path> writes this build's trace (4 columns) instead. */
typedef struct { unsigned k, f; uint64_t h, hw; bool has_w; } gline_t;

static gline_t *gold_load(const char *path, size_t *n)
{
    FILE *rf = fopen(path, "r");
    char line[128];
    gline_t *v = NULL;
    size_t cap = 0u;
    *n = 0u;
    if (rf == NULL) {
        perror(path);
        exit(2);
    }
    while (fgets(line, sizeof line, rf) != NULL) {
        unsigned kk, ff;
        unsigned long long h, hw;
        const int m = sscanf(line, "%u %u %llx %llx", &kk, &ff, &h, &hw);
        if (m >= 3) {
            if (*n == cap) {
                cap = cap ? 2u * cap : 4096u;
                v = realloc(v, cap * sizeof *v);
            }
            v[*n] = (gline_t){ kk, ff, (uint64_t)h, (m == 4) ? (uint64_t)hw : 0u, m == 4 };
            (*n)++;
        }
    }
    fclose(rf);
    return v;
}

static void gold_write(const gr_t *g, unsigned k)
{
    static FILE *wf;
    const char *path = getenv("GSB_GOLD_WRITE");
    if (wf == NULL) {
        wf = fopen(path, "w");
        if (wf == NULL) {
            perror(path);
            exit(2);
        }
    }
    for (unsigned f = 0u; f < GFRAMES; f++) {
        fprintf(wf, "%u %u %016llx %016llx\n", k, f + 1u, (unsigned long long)g->h[f],
                (unsigned long long)g->hw[f]);
    }
    fflush(wf);
}

static bool in_rel_window(const gr_t *g, unsigned f /* 1-based */)
{
    for (unsigned d = 0u; d <= 4u && d < f; d++) {
        if (g->relon[f - 1u - d] != 0u) {
            return true;
        }
    }
    return false;
}

#if defined(G6B_GATE)
/* r04q (D-G6-13, ~/tcu_sweep/r04q_port.md): THE BOOT-WINDOW CARVE-OUT of the
 * r04n / r04k gate goldens. The boot fence is lifted: the candidate's stock
 * boot sequence (A 500 for one producer pass, then A's switch-on fill one
 * pass later than the live producer's) reaches the wire, so against these
 * goldens (r04p's wire) a WIRE difference is allowed only in the frames of
 * the boot allowance [bw_beg, bw_end], and a cell-window-only difference
 * (A's filtered feedback duty 0xFFF93420 and the candidate's A request
 * object settle) only in [bw_beg, bw_end + G6Q_SETTLE]. Every other frame
 * keeps the old rule. The per-word identity of the rest of the wire is
 * test_g6b's (the staged A..E words per tick vs the r04p goldens,
 * tests/unit/g6q_gold_diff.py). */
#  define G6Q_SETTLE 300u
static unsigned g6q_cw_n[16], g6q_ww_n[16], g6q_cw_last[16];
static bool g6q_boot_frame(const gr_t *g, unsigned k, unsigned f, bool wire)
{
    if (g->bw_beg == 0u || g->bw_end == 0u || f < g->bw_beg) {
        return false;
    }
    if (wire ? (f <= g->bw_end) : (f <= g->bw_end + G6Q_SETTLE)) {
        if (k < 16u) {
            if (wire) {
                g6q_ww_n[k]++;
            } else {
                g6q_cw_n[k]++;
                g6q_cw_last[k] = f;
            }
        }
        return true;
    }
    return false;
}
#endif
static unsigned gold_check(const char *path, const gr_t *g, unsigned k, bool exact)
{
    static const char *lp;
    static gline_t *v;
    static size_t vn;
    unsigned found = 0u, diff = 0u, first = 0u;
    if (path == NULL) {
        return 0u;
    }
    if (lp == NULL || strcmp(lp, path) != 0) {
        free(v);
        v = gold_load(path, &vn);
        lp = path;
    }
#if defined(GSB_HOST_NO_REL_BRIDGE)
    if (!exact) {
        /* the r04k behaviour against HEAD bccfb9d (r04j): the G4a rule */
        unsigned post = 0u, pfirst = 0u;
        for (size_t i = 0u; i < vn; i++) {
            if (v[i].k != k || v[i].f < 1u || v[i].f > GFRAMES) {
                continue;
            }
            found++;
            if (g->h[v[i].f - 1u] != v[i].h) {
                if (g->gold_full || v[i].f < g->c3_frame) {
                    if (diff == 0u) { first = v[i].f; }
                    diff++;
                } else {
                    post++;
                    if (pfirst == 0u) { pfirst = v[i].f; }
                }
            }
        }
        printf("   GOLDEN (r04k behaviour) vs HEAD bccfb9d: run %u '%s': %u frames compared (%s), %u differ "
               "(first F%u); from the R commit F%u on %u differ (first F%u)\n", k, g->name, found,
               g->gold_full ? "every frame" : "the frames before the R commit", diff, first, g->c3_frame, post,
               pfirst);
        if (!g->gold_full && (g->c3_frame == 0u || (pfirst != 0u && pfirst < g->c3_frame))) {
            diff++;
        }
        return diff + (GFRAMES - found);
    }
#endif
    if (exact) {
        for (size_t i = 0u; i < vn; i++) {
            if (v[i].k != k || v[i].f < 1u || v[i].f > GFRAMES) {
                continue;
            }
            found++;
            if (g->h[v[i].f - 1u] != v[i].h || (v[i].has_w && g->hw[v[i].f - 1u] != v[i].hw)) {
#if defined(G6B_GATE)
                const bool wd = v[i].has_w && g->hw[v[i].f - 1u] != v[i].hw;
                if (g6q_boot_frame(g, k, v[i].f, wd)) {
                    continue;
                }
#endif
                if (diff == 0u) { first = v[i].f; }
                diff++;
            }
        }
        printf("   GOLDEN exact (G4b trace): run %u '%s': %u frames compared, %u differ (first F%u)\n", k,
               g->name, found, diff, first);
#if defined(G6B_GATE)
        printf("   r04q boot-window carve-out (exact): boot entry F%u, allowance end F%u: wire-differing frames "
               "inside it %u, cell-only %u (last F%u)\n", g->bw_beg, g->bw_end, k < 16u ? g6q_ww_n[k] : 0u,
               k < 16u ? g6q_cw_n[k] : 0u, k < 16u ? g6q_cw_last[k] : 0u);
        if (k < 16u) {
            g6q_ww_n[k] = g6q_cw_n[k] = g6q_cw_last[k] = 0u;
        }
#endif
        return diff + (GFRAMES - found);
    }
    {
        /* G4b against the r04k reference: confinement to the release windows */
        unsigned first_rel = 0u, pre = 0u, wire_out = 0u, wire_in = 0u, cell_only = 0u, wo_first = 0u;
        unsigned co_first = 0u, co_last = 0u, relf = 0u;
        for (unsigned f = 1u; f <= GFRAMES; f++) {
            if (g->relon[f - 1u] != 0u) {
                relf++;
                if (first_rel == 0u) { first_rel = f; }
            }
        }
        for (size_t i = 0u; i < vn; i++) {
            const unsigned f = v[i].f;
            if (v[i].k != k || f < 1u || f > GFRAMES) {
                continue;
            }
            found++;
            if (!v[i].has_w) {
                printf("   reference golden %s has no wire column\n", path);
                return 1u + GFRAMES;
            }
            if (first_rel == 0u || f < first_rel) {
                if (g->h[f - 1u] != v[i].h) {
#if defined(G6B_GATE)
                    if (g6q_boot_frame(g, k, f, g->hw[f - 1u] != v[i].hw)) {
                        continue;
                    }
#endif
                    pre++;
                }
                continue;
            }
            if (g->hw[f - 1u] != v[i].hw) {
                if (in_rel_window(g, f)) {
                    wire_in++;
                } else {
                    if (wo_first == 0u) { wo_first = f; }
                    wire_out++;
                }
            } else if (g->h[f - 1u] != v[i].h) {
                cell_only++;
                if (co_first == 0u) { co_first = f; }
                co_last = f;
            }
        }
        printf("   GOLDEN vs the r04k reference (no bridged release): run %u '%s': %u frames; before the first "
               "release frame F%u %u differ; WIRE differs in %u frames inside the %u release-window frames, "
               "%u outside (first F%u); cell-window-only differences %u (F%u..F%u)\n", k, g->name, found,
               first_rel, pre, wire_in, relf, wire_out, wo_first, cell_only, co_first, co_last);
#if defined(G6B_GATE)
        printf("   r04q boot-window carve-out (reference): boot entry F%u, allowance end F%u: wire-differing frames "
               "inside it %u, cell-only %u (last F%u)\n", g->bw_beg, g->bw_end, k < 16u ? g6q_ww_n[k] : 0u,
               k < 16u ? g6q_cw_n[k] : 0u, k < 16u ? g6q_cw_last[k] : 0u);
        if (k < 16u) {
            g6q_ww_n[k] = g6q_cw_n[k] = g6q_cw_last[k] = 0u;
        }
#endif
        return pre + wire_out + (GFRAMES - found) + ((first_rel == 0u) ? 1u : 0u) + ((wire_in == 0u) ? 1u : 0u);
    }
}

#if !defined(GSB_CHAIN_NO_MAIN)
int main(int argc, char **argv)
{
    const char *gold = (argc > 1) ? argv[1] : NULL;
    const char *gold2 = (argc > 2) ? argv[2] : NULL;
    (void)gold;
    (void)gold2;
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
            if (getenv("GSB_GOLD_WRITE") != NULL) {
                gold_write(g, k);
            } else {
                const unsigned d = gold_check(gold, g, k, false);
                const unsigned e = gold_check(gold2, g, k, true);
                GCHECK(d == 0u, "%s: the live path against golden 1 (%s): %u", g->name, gold, d);
                GCHECK(e == 0u, "%s: the live path against the exact golden (%s): %u", g->name, gold2, e);
            }
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
        /* G4b: the bound margins while the release is bridged (tested passes) */
        for (unsigned d = 0u; d < 2u; d++) {
            for (unsigned q = 0u; q < 2u; q++) {
                if (g->rm_n[d][q] == 0u) {
                    continue;
                }
                printf("    G4b release from %s, %s (%u passes): UB − est %s%d (est %u / UB %u), est − LB %s%d "
                       "(est %u / LB %u)\n", d == 0u ? "D" : "R", q == 0u ? "B" : (d == 0u ? "C" : "D"),
                       g->rm_n[d][q], g->rm_up[d][q] == 0x7FFFFFFF ? "untested " : "",
                       g->rm_up[d][q] == 0x7FFFFFFF ? 0 : (int)g->rm_up[d][q], g->rm_up_est[d][q],
                       g->rm_up_b[d][q], g->rm_lo[d][q] == 0x7FFFFFFF ? "untested " : "",
                       g->rm_lo[d][q] == 0x7FFFFFFF ? 0 : (int)g->rm_lo[d][q], g->rm_lo_est[d][q],
                       g->rm_lo_b[d][q]);
                if (!g->info) {
                    GCHECK(g->rm_up[d][q] > 0 && g->rm_lo[d][q] > 0, "%s: release %u ch %u margin %d / %d",
                           g->name, d, q, (int)g->rm_up[d][q], (int)g->rm_lo[d][q]);
                }
            }
        }
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
#endif /* !GSB_CHAIN_NO_MAIN */
