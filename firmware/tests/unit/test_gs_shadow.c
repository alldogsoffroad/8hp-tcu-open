/* test_gs_shadow.c — the READ-ONLY garage-engagement shadow (app/gs_shadow.c,
 * app/gs_fill.c; ~/tcu_sweep/garage_shadow_port.md).
 *
 *   (a) LIVE BIT-IDENTITY: main.c's 10 ms output path (vehicle → kl15 → the
 *       range chain → park_tick → [the shadow] → the producer + fill → the
 *       request law → bus-A words) over the r03t / r03w scenarios, run twice —
 *       without and with the shadow — and the WHOLE stock cell window
 *       0xFFF80000..0xFFF96000 hashed every tick (the shadow's own .gs_req
 *       range excluded), plus every bus-A word and the producer currents:
 *       every tick identical.
 *   (b) P → D with brake + engine 700 (forced commit): the shadow trace
 *       against the decode's predicted N→D shape and end state.
 *   (c) P → R the same.
 *   (d) [GS_FORCED = 0 build] the stock-faithful variant: no commit, ever.
 *   (e) the synchronisation progress (2026-11-18, garage_progress_decode.md):
 *       on the dry bench (turbine 0, output 0) the context syncs on the pass
 *       after B's phase-4 entry, B holds phase 0x0B for 49 passes (BD30 →
 *       50), then the 0x0C ramp → a NORMAL completion (cause 1) ≈ 0.94–0.96 s
 *       after the commit, P→D and P→R, at 38.9 °C and ≈ 21 °C; the garage
 *       fill-time term 0xE469C fills C / D 2 passes longer;
 *   (g) a turbine that never synchronises (700 rpm injected): the 250-pass
 *       timeout from B's phase-9 entry → 0x0E → completion cause 2;
 *   (n) negative: the OLD stand-ins (dump progress cells; 0xE469C = 0) must
 *       FAIL the (e) checks; reaction 37 (host-forced) must zero the
 *       released elements' i_min and leave WK / Sys at 500.
 *
 *   test_gs_shadow            run all (prints the traces)
 */
#define _POSIX_C_SOURCE 200809L
#include "app/gs_shadow.h"
#include "app/gs_fill.h"
#include "app/gs_bridge.h"
#include "safety/park_mon.h"
#include "app/park.h"
#include "app/park_law.h"
#include "app/rq_prod.h"
#include "app/rng_chain.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_park.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_rqprod.h"
#include "bsw/sup/sup_cells.h"
#include "io/sensors/sensors.h"
#include "io/vehicle/vehicle.h"
#include "io/vehicle/vehicle_inject.h"
#include "safety/kl15.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(GS_DUAL)
/* G6-0 (~/tcu_sweep/g6_0_port.md): instance 2 of the engine template (the
 * candidate) driven beside the shadow, byte-identical every pass */
#include "app/gs_cand.h"
#include "tests/unit/gs_dual.h"
#  define HOST_STANDINS(m, a, b)  gs_dual_standins((m), (a), (b))
static int s_fail;
static uint8_t host_ceca(uint8_t cls, uint8_t eng, uint8_t rel, uint8_t f7, int16_t calc_p, uint8_t old)
{
    const uint8_t a = gs_shadow_host_ceca(cls, eng, rel, f7, calc_p, old);
    const uint8_t b = gs_cand_host_ceca(cls, eng, rel, f7, calc_p, old);
    if (a != b) {
        printf("FAIL gs_dual: 0x10CECA shadow %u, candidate %u\n", a, b);
        s_fail++;
    }
    return a;
}
#  define HOST_CECA host_ceca
#else
#  define HOST_CECA gs_shadow_host_ceca
#  define HOST_STANDINS(m, a, b)  gs_shadow_host_standins((m), (a), (b))
#endif

static sensors_in_t s_z;
static uint16_t s_tur_inject;           /* (g): the shadow's 0x89CBA */
static uint32_t s_tur_from;             /* (r7): the injection starts at this tick */
static uint32_t s_t;                    /* the current tick */
static uint8_t (*s_lever_fn)(uint32_t t);   /* (r): a per-tick lever, overriding sched */
static uint8_t s_temp_byte = 0x4Fu;     /* 0x89C13 / 0x89CD2 (0x4F: 38.9 °C, r03u) */
const sensors_in_t *sensors_in(void) { return &s_z; }

#if !defined(GS_DUAL)
static int s_fail;
#endif
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); s_fail++; } } while (0)

typedef struct {
    const char *name;
    uint8_t  sched[8];      /* lever code per segment: 1 P 2 R 3 N 4 D, 0 none */
    uint8_t  nseg;
    bool     brake;
    uint16_t engine;
    uint32_t lever_from, gap_from, gap_len;
} scen_t;

#define SEG 100u
static const scen_t k_scen[] = {      /* the r03t / r03w scenarios (test_rng_wire) */
    { "rest: no sims",          { 1, 2, 3, 4, 1, 0 },    6u, false, 0u,   0u,   0u,   0u },
    { "brake + engine",         { 1, 2, 3, 4, 1, 2 },    6u, true,  700u, 0u,   0u,   0u },
    { "engine, no brake",       { 1, 2, 3, 4, 1 },       5u, false, 700u, 0u,   0u,   0u },
    { "late lever 1.1 s",       { 1, 1, 2, 3, 4, 1 },    6u, true,  700u, 110u, 0u,   0u },
    { "2 s gap with R held",    { 1, 2, 2, 2, 2, 2, 1 }, 7u, true,  700u, 0u,   140u, 200u },
    { "P-D-P-R-P (gs-check)",   { 1, 4, 4, 1, 1, 2, 2, 1 }, 8u, true, 700u, 0u,  0u,   0u },
};

static uint64_t fnv(uint64_t h, uint32_t v)
{
    for (int k = 0; k < 4; k++) {
        h = (h ^ (uint8_t)(v >> (8 * k))) * 0x100000001B3ull;
    }
    return h;
}

static uint64_t hash_window(uint64_t h)
{
    const volatile uint8_t *p = (const volatile uint8_t *)(uintptr_t)0xFFF80000u;
    for (uint32_t a = 0u; a < 0x16000u; a++) {
        const uint32_t abs = 0xFFF80000u + a;
        if ((abs >= GS_REQ_BASE && abs < GS_REQ_BASE + GS_REQ_SIZE) ||
            (abs >= 0xFFF8EE00u && abs < 0xFFF90000u) ||
            (abs >= 0xFFF91440u && abs < 0xFFF92400u)) {
            continue;                       /* the shadow's own RAM */
        }
        h = (h ^ p[a]) * 0x100000001B3ull;
    }
    return h;
}

/* the gs_in_t main.c builds */
static void shadow_tick(void)
{
    const vehicle_in_t *v = vehicle_in();
    const gs_in_t gi = {
        .os_mode = 1u,
        .rngd = *(const volatile uint8_t *)0xFFF89CDCu,
        .park_model = park_law_state(),
        .park_req = park_law_request(),
        .temp = *(const volatile uint8_t *)0xFFF89C13u,
        .cd2 = *(const volatile uint8_t *)0xFFF89CD2u,
        .eng_speed = (uint16_t)v->engine_speed,
        .eng_valid = (v->q_engine_speed == Q_OK) ? 1u : 0u,
        .out_rpm = (s_z.q_output_speed == Q_OK && s_z.output_speed > 0) ? (uint16_t)s_z.output_speed : 0u,
        .tur_rpm = (s_t >= s_tur_from) ? s_tur_inject : 0u,
    };
    gs_shadow_tick_10ms(&gi);
#if defined(GS_DUAL)
    gs_dual_tick(&gi);
#endif
    {
        /* (2026-11-18) THE GARAGE BRIDGE, G1 (app/gs_bridge.c): ticked with
         * the shadow, so (a) proves the live path identical with both */
        const gsb_in_t bi = {
            .mon_faults = park_mon_faults(),
            .relwin_st = 0u,
            .eng_valid = gi.eng_valid,
            .tur_ok = 1u,
            .eng_speed = gi.eng_speed,
            .tur_rpm = gi.tur_rpm,
            .cyc = drv_job_tx(DRV_CYC),
        };
        gs_bridge_tick_10ms(&bi);
    }
}

typedef void (*tick_cb_t)(uint32_t t, uint8_t lever);

/* run one scenario; returns per-tick hashes into hs (n ticks) */
static void run(const scen_t *sc, bool shadow, uint64_t *hs, tick_cb_t cb)
{
    memset((void *)(uintptr_t)0xFFF80000u, 0, 0x16000u);
    memset(&s_z, 0, sizeof s_z);
    s_z.q_output_speed = Q_OK;
    vehicle_init();
    park_init();
    rng_chain_init();
    kl15_init();
    *(volatile uint16_t *)(uintptr_t)KL15_SRC_CELL = 13206u;
    drv_jobs_init();
    *(volatile uint8_t *)0xFFF89C13u = s_temp_byte;   /* the bench's 38.9 °C (r03u) */
    *(volatile uint8_t *)0xFFF89CD2u = s_temp_byte;
    drv_rq_prod_host_select(true);
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        drv_rq_request_set(ch, 0u);
    }
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    CELL_LETTERS[0] = (uint8_t)'c';     /* the engaged rail (the bridge's arm condition) */
    CELL_OS_MODE = 1u;
    CELL_LAD_94114 = 0u;
    CELL_DIAG_94115 = 0u;
    CELL_J2_STAGED = 0x9020u;
    CELL_LAD_REQ[0] = 0u;
    CELL_LAD_REQ[1] = 0u;
    CELL_OUT_MV[0] = 12940u;
    rq_prod_boot_pass();
    if (shadow) {
        gs_shadow_init();
#if defined(GS_DUAL)
        gs_dual_init();
#endif
        gs_bridge_init(true);
    }
    for (uint32_t t = 0u; t < sc->nseg * SEG; t++) {
        const uint8_t code = (s_lever_fn != NULL) ? s_lever_fn(t) : sc->sched[t / SEG];
        uint8_t d[8];
        uint64_t h = 0xCBF29CE484222325ull;

        if (code != 0u && t >= sc->lever_from &&
            !(t >= sc->gap_from && t < sc->gap_from + sc->gap_len)) {
            const uint8_t l[5] = { code, 0, 0, 0, 0 };
            vehicle_inject_msg_lever(LEVER_SRC_PRIMARY, l[0]);
            vehicle_inject_msg_lever(LEVER_SRC_FALLBACK, l[0]);
        }
        if (sc->brake) {
            vehicle_inject_msg_brake(BRAKE_ON);
        }
        if (sc->engine != 0u) {
            vehicle_inject_msg_engine(sc->engine, 0x1FFFu);
        }
        /* main.c's task_10ms order */
        rq_prod_task_10ms(1u);
        vehicle_update_10ms();
        kl15_tick_10ms();
        rng_chain_tick_10ms(1u, 1u, 1u);
        park_tick_10ms();
        s_t = t;
        if (shadow) {
            shadow_tick();
        }
        drv_rq_request_set(7u, park_slot7_release());
        drv_cyclic_build(0, 8u);
        {
            const uint16_t *cyc = drv_job_tx(DRV_CYC);
            const uint16_t *cfg = drv_job_tx(DRV_CFG);
            for (unsigned i = 0u; i < 45u; i++) {
                h = fnv(h, cyc[i]);
            }
            for (unsigned i = 0u; i < 40u; i++) {
                h = fnv(h, cfg[i]);
            }
        }
        h = fnv(h, park_slot7_release());
        h = fnv(h, park_slot11_hold());
        h = hash_window(h);
        if (hs != NULL) {
            hs[t] = h;
        }
        if (cb != NULL) {
            cb(t, code);
        }
    }
}

/* ── (a) ─────────────────────────────────────────────────────────────── */
static void run_fresh(const scen_t *sc, bool shadow, uint64_t *hs, unsigned n)
{
    int fd[2];
    pid_t pid;
    if (pipe(fd) != 0) {
        perror("pipe");
        exit(2);
    }
    pid = fork();
    if (pid == 0) {
        static uint64_t h[1000];
        close(fd[0]);
        run(sc, shadow, h, NULL);
        if (write(fd[1], h, n * sizeof h[0]) != (ssize_t)(n * sizeof h[0])) {
            _exit(3);
        }
#if defined(GS_DUAL)
        if (shadow) {
            gs_dual_report(sc->name);
            fflush(stdout);
        }
        _exit(gs_dual_bad() != 0u ? 5 : 0);
#else
        _exit(0);
#endif
    }
    close(fd[1]);
    {
        size_t got = 0u;
        while (got < n * sizeof hs[0]) {
            const ssize_t r = read(fd[0], (char *)hs + got, n * sizeof hs[0] - got);
            if (r <= 0) {
                break;
            }
            got += (size_t)r;
        }
        close(fd[0]);
#if defined(GS_DUAL)
        {
            int st = 0;
            waitpid(pid, &st, 0);
            if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
                printf("FAIL gs_dual: the candidate differs from the shadow in the (a) child (%s)\n",
                       sc->name);
                s_fail++;
            }
        }
#else
        waitpid(pid, NULL, 0);
#endif
        if (got != n * sizeof hs[0]) {
            printf("FAIL (a) child run incomplete\n");
            s_fail++;
        }
    }
}

static void test_identity(void)
{
    static uint64_t a[1000], b[1000];
    unsigned n_scen = (unsigned)(sizeof k_scen / sizeof k_scen[0]), diff = 0u, ticks = 0u;
    for (unsigned k = 0u; k < n_scen; k++) {
        const unsigned n = k_scen[k].nseg * SEG;
        /* each run in a fresh process: the live modules keep static state
         * between runs that their init functions do not reset */
        run_fresh(&k_scen[k], false, a, n);
        run_fresh(&k_scen[k], true, b, n);
        for (unsigned t = 0u; t < n; t++) {
            ticks++;
            if (a[t] != b[t]) {
                if (diff < 5u) {
                    printf("FAIL (a) %s tick %u: live state differs with the shadow\n",
                           k_scen[k].name, t);
                }
                diff++;
            }
        }
    }
    CHECK(diff == 0u, "(a) %u of %u ticks differ", diff, ticks);
    printf("(a) live bit-identity: %u ticks over %u scenarios, %u differ\n", ticks, n_scen, diff);
}

/* ── (b) (c) (e) (g) (n): one P → X engagement ───────────────────────── */
static void dump_rec(uint8_t slot, unsigned upto)
{
    const volatile gs_wit_t *w = gs_shadow_wit();
    printf("  rec slot %u: %u entries\n", slot, w->rec_n[slot]);
    printf("  pass pf fl  ph0 ph1 ph2 md  set gr |    A    D    B    E    C   WK  Sys | bd30 sy   bd76\n");
    for (unsigned i = 0u; i < w->rec_n[slot] && i < upto; i++) {
        const volatile gs_rec_t *r = gs_shadow_rec(slot, (uint8_t)i);
        printf("  %4u %2u %02X  %3X %3X %3X %02X %02X %3d | %4u %4u %4u %4u %4u %4u %4u | %4u %2X %6d\n",
               r->pass, r->pf, r->flags, r->ph[0], r->ph[1], r->ph[2], r->modes, r->reqset,
               r->gear, r->cur[0], r->cur[1], r->cur[2], r->cur[3], r->cur[4], r->cur[5], r->cur[6],
               r->bd30, r->sync, r->bd76);
    }
}

/* tick-level observations of one engagement */
typedef struct {
    int commit_tick, done_tick, b_ph4, b_ph9, b_ph0a, b_ph0b, b_ph0e, sync_tick, c_first, b_start;
    uint16_t b_ph4_cur;
} eng_obs_t;
static eng_obs_t s_obs;
static uint8_t s_target_lever;
static void obs_cb(uint32_t t, uint8_t lever)
{
    const volatile gs_wit_t *w = gs_shadow_wit();
    if (lever != s_target_lever) {
        return;
    }
    if (s_obs.commit_tick < 0 && w->commits > 0u && w->commit_new != 0u) {
        s_obs.commit_tick = (int)t;
    }
    if (s_obs.commit_tick < 0) {
        return;
    }
    if (s_obs.done_tick < 0 && gs_shadow_pending() == 0u && gs_shadow_pf() != 0u) {
        s_obs.done_tick = (int)t;
    }
    if (gs_shadow_phase(0) == 4u && s_obs.b_ph4 < 0) {
        s_obs.b_ph4 = (int)t;
        s_obs.b_ph4_cur = gs_shadow_cur(2);
    }
    if (gs_shadow_phase(0) == 9u && s_obs.b_ph9 < 0) {
        s_obs.b_ph9 = (int)t;
    }
    if (gs_shadow_phase(0) == 0x0Au && s_obs.b_ph0a < 0) {
        s_obs.b_ph0a = (int)t;
    }
    if (gs_shadow_phase(0) == 0x0Bu && s_obs.b_ph0b < 0) {
        s_obs.b_ph0b = (int)t;
    }
    if (gs_shadow_phase(0) == 0x0Eu && s_obs.b_ph0e < 0) {
        s_obs.b_ph0e = (int)t;
    }
    if (s_obs.sync_tick < 0 && s_obs.b_ph4 >= 0 && gs_shadow_sync() != 0u) {
        s_obs.sync_tick = (int)t;
    }
    if (gs_shadow_phase(0) == 2u && s_obs.b_start < 0) {
        s_obs.b_start = (int)t;
    }
    if (gs_shadow_phase(2) == 2u && s_obs.c_first < 0) {
        s_obs.c_first = (int)t;
    }
}

/* the recorder's view of slot 0 */
typedef struct {
    unsigned n;             /* entries */
    int ph4, sync, b0b_first, b0b_n, b0c_first, done;   /* passes (−1 none) */
    unsigned fill3_n;       /* passes the third element sits in phase 2 */
    uint16_t b0b_first_cur, b0b_last_cur;
    uint8_t done_cause;
    double done_s;          /* completion − commit, s (tick level) */
} shape_t;

#if GS_FORCED
static shape_t shape_of(uint8_t slot)
{
    shape_t r = { 0u, -1, -1, -1, 0, -1, -1, 0u, 0u, 0u, 0u, -1.0 };
    const volatile gs_wit_t *w = gs_shadow_wit();
    r.n = w->rec_n[slot];
    for (unsigned i = 0u; i < r.n; i++) {
        const volatile gs_rec_t *e = gs_shadow_rec(slot, (uint8_t)i);
        if (e->ph[2] == 2u) {
            r.fill3_n++;
        }
        if (r.ph4 < 0 && e->ph[0] == 4u) {
            r.ph4 = (int)e->pass;
        }
        if (r.ph4 >= 0 && r.sync < 0 && (e->sync & 1u) != 0u) {
            r.sync = (int)e->pass;
        }
        if (e->ph[0] == 0x0Bu) {
            if (r.b0b_first < 0) {
                r.b0b_first = (int)e->pass;
                r.b0b_first_cur = e->cur[2];
            }
            r.b0b_n++;
            r.b0b_last_cur = e->cur[2];
        }
        if (r.b0c_first < 0 && e->ph[0] == 0x0Cu) {
            r.b0c_first = (int)e->pass;
        }
        if (r.done < 0 && (e->flags & 0x20u) != 0u) {
            r.done = (int)e->pass;
        }
    }
    r.done_cause = w->done_cause;
    if (s_obs.done_tick >= 0 && s_obs.commit_tick >= 0) {
        r.done_s = (double)(s_obs.done_tick - s_obs.commit_tick) / 100.0;
    }
    return r;
}

/* the stock's dry-bench shape (garage_progress_decode §4/§5) */
static int shape_bad(const shape_t *r, unsigned fill_expect, bool print, const char *name)
{
    int bad = 0;
#define SHAPE(c, ...) do { if (!(c)) { bad++; if (print) { printf("FAIL %s: ", name); \
    printf(__VA_ARGS__); printf("\n"); } } } while (0)
    SHAPE(r->ph4 >= 0, "B never reached phase 4");
    SHAPE(r->sync == r->ph4 + 1, "sync at pass %d, expected the pass after B's phase 4 (%d)", r->sync, r->ph4 + 1);
    SHAPE(r->b0b_first == r->ph4 + 1, "B enters 0x0B at pass %d, expected %d (9 → 0x0A → 0x0B in one pass)",
          r->b0b_first, r->ph4 + 1);
    SHAPE(r->b0b_n == 49u, "B holds 0x0B for %d passes, expected 49 (BD30 1 → 50)", r->b0b_n);
    SHAPE(r->b0c_first == r->b0b_first + 49, "the 0x0C ramp starts at pass %d, expected %d",
          r->b0c_first, r->b0b_first + 49);
    SHAPE(r->done_cause == 1u, "completion cause %u, expected 1 (normal)", r->done_cause);
    SHAPE(r->done_s >= 0.94 && r->done_s <= 0.99, "completion %.2f s after the commit, expected 0.94..0.99",
          r->done_s);
    SHAPE(r->done >= 0 && (unsigned)r->done + 10u < GS_REC_N,
          "the recorder must hold the completion + 10 passes (done at %d of %u)", r->done, GS_REC_N);
    SHAPE(r->fill3_n == fill_expect, "the third element sits %u passes in phase 2, expected %u "
          "(the holder's fill + 2: 0xE469C +20 ms)", r->fill3_n, fill_expect);
#undef SHAPE
    return bad;
}
#endif

static void engagement(const char *name, uint8_t lever, uint8_t mask, const uint16_t end[7],
                       bool print)
{
    static const scen_t sp = { "P then X", { 1, 9, 9, 9, 9, 9 }, 6u, true, 700u, 0u, 0u, 0u };
    scen_t sc = sp;
    for (unsigned i = 1u; i < 6u; i++) {
        sc.sched[i] = lever;
    }
    memset(&s_obs, 0xFF, sizeof s_obs);
    s_obs.b_ph4_cur = 0u;
    s_target_lever = lever;
    HOST_STANDINS(mask, 0, 0u);
    run(&sc, true, NULL, obs_cb);
    {
        const volatile gs_wit_t *w = gs_shadow_wit();
        printf("%s: commits %u (last %u → %u at pass %u), done cause %u at pass %u, "
               "unp %u, e896 %u, lim %u, rel %u, seq %u, loop cap %u; fill unp %u f19f %u shift %u "
               "e469c %u\n",
               name, w->commits, w->commit_old, w->commit_new, w->commit_pass, w->done_cause,
               w->done_pass, w->unp_n, w->e896_n, w->lim_n, w->rel_n, w->seq_n, w->loop_cap_n,
               gsf_unp_n(), gsf_f19f_n(), gsf_shift_n(), gsf_e469c_n());
        printf("  observed (ticks): commit %d, C/D phase 2 %d, B phase 2 %d, B phase 4 %d (B %u), "
               "sync %d, B phase 9 %d, B 0x0E %d, completion %d (%.2f s after the commit)\n",
               s_obs.commit_tick, s_obs.c_first, s_obs.b_start, s_obs.b_ph4, s_obs.b_ph4_cur,
               s_obs.sync_tick, s_obs.b_ph9, s_obs.b_ph0e, s_obs.done_tick,
               s_obs.done_tick >= 0 ? (double)(s_obs.done_tick - s_obs.commit_tick) / 100.0 : -1.0);
        if (print) {
            dump_rec(0u, GS_REC_N);
        }
        printf("  end: pf %u gear %d currents A %u D %u B %u E %u C %u WK %u Sys %u\n",
               gs_shadow_pf(), gs_shadow_gear(), gs_shadow_cur(0), gs_shadow_cur(1),
               gs_shadow_cur(2), gs_shadow_cur(3), gs_shadow_cur(4), gs_shadow_cur(5),
               gs_shadow_cur(6));
#if GS_FORCED
        CHECK(w->commits >= 1u && w->commit_new != 0u, "%s: no nonzero commit", name);
        CHECK(s_obs.done_tick >= 0, "%s: the engagement never completed", name);
        CHECK(w->loop_cap_n == 0u, "%s: 0x10D070 loop cap reached", name);
        CHECK(gsf_f19f_n() == 0u, "%s: fill fault 0x19F reported", name);
        for (uint8_t c = 0u; c < 7u; c++) {
            CHECK(gs_shadow_cur(c) == end[c], "%s: end current ch %u = %u, expected %u",
                  name, c, gs_shadow_cur(c), end[c]);
        }
        /* the shape: the third element fills first, B waits in 0x0F */
        CHECK(s_obs.c_first >= 0 && s_obs.b_start > s_obs.c_first,
              "%s: B's fill must follow the third element's (C %d, B %d)", name, s_obs.c_first,
              s_obs.b_start);
#else
        (void)end;
        CHECK(w->commits == 0u, "%s: the stock-faithful variant committed (%u)", name, w->commits);
        CHECK(gs_shadow_pf() == 0u, "%s: pf %u", name, gs_shadow_pf());
#endif
    }
}

/* ── (r) THE RELEASE (G4a, 2026-11-19; ~/tcu_sweep/g4a_release_port.md,
 * decode garage_release_decode.md §5) ─────────────────────────────────── */
static uint32_t s_rel_lp;           /* the tick of lever P (or of the second lever) */
static uint8_t  s_rel_x, s_rel_x2;  /* the first lever (4 D / 2 R), the second (1 P / 2 R) */
static uint8_t lever_rel(uint32_t t)
{
    if (t < 100u) {
        return 1u;
    }
    return (t < s_rel_lp) ? s_rel_x : s_rel_x2;
}

typedef struct {
    int      x_commit;          /* tick of the first (nonzero) commit */
    int      r_commit;          /* tick of the release commit */
    int      c3;                /* tick of a third commit (D → R direct) */
    unsigned bad_out;           /* release passes with a sub-object +0xA > its +0x26 */
    unsigned bad_mono;          /* release passes where a moving channel moved the wrong way */
    uint16_t prev[5];
} rel_obs_t;
static rel_obs_t s_ro;
static void rel_cb(uint32_t t, uint8_t lever)
{
    const volatile gs_wit_t *w = gs_shadow_wit();
    (void)lever;
    if (s_ro.x_commit < 0 && w->commits >= 1u) {
        s_ro.x_commit = (int)t;
    }
    if (s_ro.r_commit < 0 && w->commits >= 2u) {
        s_ro.r_commit = (int)t;
    }
    if (s_ro.c3 < 0 && w->commits >= 3u) {
        s_ro.c3 = (int)t;
    }
    if (gs_shadow_releasing() != 0u) {
        for (uint8_t k = 0u; k < 3u; k++) {
            int16_t v[4];
            gs_shadow_host_rel(k, v);
            if (v[1] > v[0]) {
                s_ro.bad_out++;
            }
        }
    }
}

typedef struct {
    const gs_rslot_t *h;
    const gs_rrec_t *r;
} rel_view_t;

static rel_view_t rel_slot(uint8_t k)
{
    const volatile gs_rel_t *g = gs_shadow_rel();
    rel_view_t v = { (const gs_rslot_t *)&g->slot[k], (const gs_rrec_t *)&g->rec[k][0] };
    return v;
}

static void rel_dump(uint8_t k, unsigned every)
{
    const rel_view_t v = rel_slot(k);
    printf("  release slot %u: start pass %u, len %u, cause %u, BD8E at the end %u, lim12 %u, n %u, gear %d, "
           "elements %u %u %u, +0x26 %d %d %d\n", k, (unsigned)v.h->start_pass, v.h->len, v.h->cause,
           v.h->bd8e_end, v.h->lim12, v.h->n, v.h->gear, v.h->el[0], v.h->el[1], v.h->el[2], v.h->p26[0],
           v.h->p26[1], v.h->p26[2]);
    for (unsigned i = 0u; i < v.h->n; i++) {
        const gs_rrec_t *e = &v.r[i];
        if (i < 3u || (i % every) == 0u || (i + 3u >= v.h->n) || (e->fl & 0x02u) != 0u ||
            (i > 0u && (v.r[i - 1u].fl & 0x02u) != 0u)) {
            printf("  %4u ph %02X fl %02X | A %4u D %4u B %4u E %4u C %4u\n", i, e->ph, e->fl, e->cur[0],
                   e->cur[1], e->cur[2], e->cur[3], e->cur[4]);
        }
    }
}

/* one release: lever x for `hold` ticks after its commit, then x2; temp byte */
static rel_view_t release(const char *name, uint8_t x, int hold, uint8_t x2, uint8_t temp, uint8_t mask,
                          bool print)
{
    static const scen_t sc0 = { "release", { 0 }, 7u, true, 700u, 0u, 0u, 0u };
    int xc;
    s_temp_byte = temp;
    HOST_STANDINS(mask, 0, 0u);
    s_lever_fn = lever_rel;
    s_rel_x = x;
    s_rel_x2 = 1u;
    /* the probe: where does x commit? */
    s_rel_lp = 100000u;
    memset(&s_ro, 0xFF, sizeof s_ro);
    run(&sc0, true, NULL, rel_cb);
    xc = s_ro.x_commit;
    s_rel_lp = (uint32_t)(xc + hold);
    s_rel_x2 = x2;
    memset(&s_ro, 0, sizeof s_ro);
    s_ro.x_commit = s_ro.r_commit = s_ro.c3 = -1;
    run(&sc0, true, NULL, rel_cb);
    s_lever_fn = NULL;
    HOST_STANDINS(0u, 0, 0u);
    {
        const volatile gs_rel_t *g = gs_shadow_rel();
        const uint8_t k = (x == 2u) ? 1u : 0u;
        printf("%s: %s commit at tick %d, lever %u at tick %u, release commit at tick %d (%d passes after "
               "the %s commit), third commit %d; rel passes %u starts %u unp %u c3 %u e896 %u idle %u; "
               "+0xA > +0x26: %u\n", name, x == 2u ? "R" : "D", s_ro.x_commit, x2, s_rel_lp, s_ro.r_commit,
               s_ro.r_commit - s_ro.x_commit, x == 2u ? "R" : "D", s_ro.c3, (unsigned)g->rel_passes,
               (unsigned)g->starts, (unsigned)g->unp_n, (unsigned)g->c3_n, (unsigned)g->e896_n,
               (unsigned)g->idle_n, s_ro.bad_out);
        if (print) {
            rel_dump(k, 10u);
        }
        return rel_slot(k);
    }
}

/* the decode's §5 values: pass 0 and passes 80..119 of the moving pair, the
 * P content at pass 120 (cause 6, the BD8E timer); returns the failures */
static int rel_shape_bad(const rel_view_t *v, uint8_t mov3, uint16_t b0, uint16_t m0, uint16_t b80,
                         uint16_t m80, bool print, const char *nm)
{
    static const uint16_t PC[5] = { 8500u, 8500u, 500u, 8500u, 8500u };
    int bad = 0;
#define RS(c, ...) do { if (!(c)) { bad++; if (print) { printf("FAIL %s: ", nm);     printf(__VA_ARGS__); printf("\n"); } } } while (0)
    RS(v->h->len == 120u && v->h->cause == 6u && v->h->bd8e_end == 120u && v->h->lim12 == 120u,
       "release length %u cause %u BD8E %u lim12 %u, expected 120 / 6 (the BD8E timer) / 120 / 120",
       v->h->len, v->h->cause, v->h->bd8e_end, v->h->lim12);
    RS(v->h->n == GS_RREC_N, "the recorder holds %u entries (%u expected)", v->h->n, GS_RREC_N);
    if (v->h->n == GS_RREC_N) {
        RS(v->r[0].cur[2] == b0 && v->r[0].cur[mov3] == m0, "pass 0: B %u / third %u, expected %u / %u",
           v->r[0].cur[2], v->r[0].cur[mov3], b0, m0);
        for (unsigned i = 0u; i < 120u; i++) {
            const gs_rrec_t *e = &v->r[i];
            RS((e->fl & 0x01u) != 0u && (e->ph & 0x03u) == 1u && ((e->ph >> 4) & 3u) == 1u &&
               ((e->ph >> 2) & 3u) == 0u,
               "pass %u: releasing with D0DC / D1B4 in 0x13 and D148 (A) in 0x12 (ph %02X fl %02X)", i, e->ph, e->fl);
            for (unsigned c = 0u; c < 5u; c++) {
                if (c != 2u && c != mov3) {
                    RS(e->cur[c] == 8500u, "pass %u: fixed channel %u = %u (8500)", i, c, e->cur[c]);
                }
            }
            if (i > 0u) {   /* B (normally closed) never rises, the third never falls */
                RS(e->cur[2] <= v->r[i - 1u].cur[2] && e->cur[mov3] >= v->r[i - 1u].cur[mov3],
                   "pass %u: B %u -> %u, third %u -> %u (the release envelope)", i, v->r[i - 1u].cur[2],
                   e->cur[2], v->r[i - 1u].cur[mov3], e->cur[mov3]);
            }
            if (i >= 80u) {
                RS(e->cur[2] == b80 && e->cur[mov3] == m80, "pass %u: B %u third %u, expected %u / %u", i,
                   e->cur[2], e->cur[mov3], b80, m80);
            }
        }
        for (unsigned i = 120u; i < GS_RREC_N; i++) {
            for (unsigned c = 0u; c < 5u; c++) {
                RS(v->r[i].cur[c] == PC[c], "pass %u: P content ch %u = %u (%u)", i, c, v->r[i].cur[c], PC[c]);
            }
            RS((v->r[i].fl & 0x01u) == 0u, "pass %u: still releasing", i);
        }
        RS((v->r[120].fl & 0x02u) != 0u, "the completion is not recorded at pass 120 (fl %02X)", v->r[120].fl);
    }
#undef RS
    return bad;
}

static void test_release(void)
{
    static const struct { uint8_t t; const char *n; uint16_t db0, dc0, db80, dc80, rb0, rd0, rb80, rd80; } T[3] = {
        { 0x4Fu, "38.9 C", 4120u, 6441u, 3484u, 6669u, 3600u, 6151u, 3484u, 6504u },
        { 0x52u, "42 C",   4119u, 6439u, 3482u, 6665u, 3598u, 6149u, 3482u, 6499u },
        { 0x3Du, "21 C",   4128u, 6452u, 3494u, 6710u, 3610u, 6164u, 3494u, 6536u },
    };
    char nm[96];
    rel_view_t v;
    for (unsigned k = 0u; k < 3u; k++) {
        /* (r1)/(r3) D → P and (r2)/(r3) R → P, lever P 2 s after the commit */
        snprintf(nm, sizeof nm, "(r1) D->P release, %s", T[k].n);
        v = release(nm, 4u, 200, 1u, T[k].t, 0u, k == 0u);
        s_fail += rel_shape_bad(&v, 4u, T[k].db0, T[k].dc0, T[k].db80, T[k].dc80, true, nm);
        CHECK(s_ro.bad_out == 0u, "%s: a sub-object's +0xA above its +0x26 (%u passes)", nm, s_ro.bad_out);
        CHECK(gs_shadow_rel()->unp_n == 0u && gs_shadow_rel()->c3_n == 0u && gs_shadow_rel()->e896_n > 0u,
              "%s: unported %u, C case %u, the D252 stand-in counted %u", nm, (unsigned)gs_shadow_rel()->unp_n,
              (unsigned)gs_shadow_rel()->c3_n, (unsigned)gs_shadow_rel()->e896_n);
        CHECK(v.h->el[0] == 2u && v.h->el[1] == 1u && v.h->el[2] == 3u, "%s: released B, A, C (%u %u %u)", nm,
              v.h->el[0], v.h->el[1], v.h->el[2]);
        snprintf(nm, sizeof nm, "(r2) R->P release, %s", T[k].n);
        v = release(nm, 2u, 200, 1u, T[k].t, 0u, k == 0u);
        s_fail += rel_shape_bad(&v, 1u, T[k].rb0, T[k].rd0, T[k].rb80, T[k].rd80, true, nm);
        CHECK(s_ro.bad_out == 0u, "%s: a sub-object's +0xA above its +0x26 (%u passes)", nm, s_ro.bad_out);
        CHECK(v.h->el[0] == 2u && v.h->el[1] == 1u && v.h->el[2] == 4u, "%s: released B, A, D (%u %u %u)", nm,
              v.h->el[0], v.h->el[1], v.h->el[2]);
    }
    /* (r4) lever P while B is still filling: done in the commit pass (cause 5) */
    {
        static const int H[2] = { 5, 20 };
        for (unsigned i = 0u; i < 2u; i++) {
            snprintf(nm, sizeof nm, "(r4) lever P %d ticks after the D commit (B filling)", H[i]);
            v = release(nm, 4u, H[i], 1u, 0x4Fu, 0u, false);
            CHECK(v.h->len == 0u && v.h->cause == 5u && v.h->n >= 1u && (v.r[0].fl & 0x02u) != 0u &&
                  v.r[0].cur[2] == 500u && v.r[0].cur[4] == 8500u,
                  "%s: len %u cause %u (expected 0 / 5, P content in the commit pass: B %u C %u)", nm,
                  v.h->len, v.h->cause, v.r[0].cur[2], v.r[0].cur[4]);
        }
    }
    /* (r5) lever P during B's 0x0B / 0x0C: the full 120 passes */
    {
        static const int H[2] = { 40, 60 };
        for (unsigned i = 0u; i < 2u; i++) {
            snprintf(nm, sizeof nm, "(r5) lever P %d ticks after the D commit (B applied)", H[i]);
            v = release(nm, 4u, H[i], 1u, 0x4Fu, 0u, false);
            CHECK(v.h->len == 120u && v.h->cause == 6u, "%s: len %u cause %u (expected 120 / 6)", nm, v.h->len,
                  v.h->cause);
        }
    }
    /* (r6) D → R direct: one release pass, one N pass, the R commit */
    v = release("(r6) D->R direct (lever R 2 s after the D commit)", 4u, 200, 2u, 0x4Fu, 0u, true);
    CHECK(v.h->len == 1u && v.h->cause == 4u, "(r6) the release lasts %u passes, cause %u (expected 1 / 4)",
          v.h->len, v.h->cause);
    CHECK(s_ro.c3 >= 0 && s_ro.c3 - s_ro.r_commit == 2, "(r6) the R commit %d ticks after the release commit "
          "(expected 2: one release pass, one N pass)", s_ro.c3 - s_ro.r_commit);
    CHECK(v.h->n >= 2u && v.r[1].cur[2] == 500u && v.r[1].cur[4] == 8500u && (v.r[1].fl & 0x01u) == 0u,
          "(r6) pass 1 must be the N content (B %u C %u fl %02X)", v.r[1].cur[2], v.r[1].cur[4], v.r[1].fl);
    /* (r7) the speed exit: a 2100 rpm turbine from release pass 5 */
    {
        int rc;
        s_tur_inject = 2100u;
        s_tur_from = 100000u;
        (void)release("(r7) probe", 4u, 200, 1u, 0x4Fu, 0u, false);
        rc = s_ro.r_commit;
        s_tur_from = (uint32_t)(rc + 5);
        v = release("(r7) D->P, turbine 2100 rpm from release pass 5", 4u, 200, 1u, 0x4Fu, 0u, true);
        s_tur_inject = 0u;
        s_tur_from = 0u;
        CHECK(s_ro.r_commit == rc && v.h->len == 6u && v.h->cause == 5u,
              "(r7) the release ends %u passes after its commit, cause %u (expected 6 / 5: 0x14 at pass 5)",
              v.h->len, v.h->cause);
        CHECK(v.h->n > 6u && (v.r[5].ph & 0x33u) == 0x22u && (v.r[5].fl & 0x20u) != 0u,
              "(r7) pass 5: D0DC / D1B4 must be in 0x14 with the speed exit (ph %02X fl %02X)", v.r[5].ph,
              v.r[5].fl);
    }
    /* (n4) the OLD one-pass stand-in, (n5) the timer limit forced 0: must FAIL (r1) */
    v = release("(n4) D->P, the OLD one-pass release stand-in", 4u, 200, 1u, 0x4Fu, GS_HOST_OLD_RELEASE, false);
    CHECK(rel_shape_bad(&v, 4u, 4120u, 6441u, 3484u, 6669u, false, "n4") > 0 && v.h->len == 0u && v.h->cause == 3u,
          "(n4) the old stand-in must fail the release shape (len %u cause %u)", v.h->len, v.h->cause);
    v = release("(n5) D->P, D0DC +0x12 forced 0", 4u, 200, 1u, 0x4Fu, GS_HOST_LIM12_0, false);
    CHECK(rel_shape_bad(&v, 4u, 4120u, 6441u, 3484u, 6669u, false, "n5") > 0 && v.h->len == 0u && v.h->cause == 6u,
          "(n5) a zero timer limit must fail the release shape (len %u cause %u)", v.h->len, v.h->cause);
    /* (n6) 0x10CECA: the element test vs the OLD class test. They differ only
     * when sub0's element is not B (or B sits in another sub-object) — never
     * in a 1st / R release (B, A, C / B, A, D): the synthetic sub-objects
     * show the difference, the D → P / R → P traces must be identical */
    CHECK(HOST_CECA(2u, 0u, 3u, 1u, 0, 0u) == 1u && HOST_CECA(2u, 0u, 3u, 1u, 0, 1u) == 0u,
          "(n6) class 2 releasing C: element test mode 1 (f7, out 0), class test mode 0");
    CHECK(HOST_CECA(4u, 0u, 2u, 1u, 0, 0u) == 0u && HOST_CECA(4u, 0u, 2u, 1u, 0, 1u) == 1u,
          "(n6) class 4 releasing B: element test mode 0, class test mode 1");
    {
        rel_view_t a, b;
        static gs_rrec_t ra[GS_RREC_N];
        a = release("(n6) D->P, element test", 4u, 200, 1u, 0x4Fu, 0u, false);
        memcpy(ra, a.r, sizeof ra);
        b = release("(n6) D->P, OLD class test", 4u, 200, 1u, 0x4Fu, GS_HOST_CECA_CLASS, false);
        CHECK(memcmp(ra, b.r, sizeof ra) == 0, "(n6) a 1st release: the class and element tests must agree");
    }
    s_temp_byte = 0x4Fu;
}

int main(void)
{
    /* the end states: the holder's block-1 row, binary currents (0.1 mA) */
    static const uint16_t END_D[7] = { 8500u, 8500u, 8500u, 8500u, 500u, 500u, 500u };
    static const uint16_t END_R[7] = { 8500u, 500u, 8500u, 8500u, 8500u, 500u, 500u };

    printf("garage shadow, GS_FORCED = %d\n", GS_FORCED);
    test_identity();
#if GS_FORCED
    {   /* (e) the dry bench, the ported progress producer + 0xE469C, P→D and
         * P→R at 38.9 °C (0x4F) and ≈ 21 °C (0x3D). The third element's
         * fill length is judged against the same run with the term at 0
         * (+2 passes = +20 ms), and must be 11 at 38.9 °C (the decode) */
        static const struct { uint8_t temp; const char *t; } T[2] = {
            { 0x4Fu, "38.9 C" }, { 0x3Du, "21 C" } };
        static const struct { uint8_t lever; const char *x; const uint16_t *end; } L[2] = {
            { 4u, "P->D", END_D }, { 2u, "P->R", END_R } };
        for (unsigned k = 0u; k < 2u; k++) {
            for (unsigned l = 0u; l < 2u; l++) {
                char nm[96];
                shape_t r;
                unsigned base;
                s_temp_byte = T[k].temp;
                snprintf(nm, sizeof nm, "(e0) %s, %s, the term at 0 (reference)", L[l].x, T[k].t);
                engagement(nm, L[l].lever, GS_HOST_OLD_E469C, L[l].end, false);
                base = shape_of(0u).fill3_n;
                snprintf(nm, sizeof nm, "(e) %s dry bench, %s", L[l].x, T[k].t);
                engagement(nm, L[l].lever, 0u, L[l].end, true);
                r = shape_of(0u);
                printf("  shape: B ph4 pass %d, sync %d, 0x0B %d..+%u (B %u -> %u), 0x0C %d, done pass %d "
                       "cause %u (%.2f s), third element phase 2 for %u passes (term 0: %u)\n",
                       r.ph4, r.sync, r.b0b_first, r.b0b_n, r.b0b_first_cur, r.b0b_last_cur, r.b0c_first,
                       r.done, r.done_cause, r.done_s, r.fill3_n, base);
                s_fail += shape_bad(&r, base + 2u, true, nm);
                if (k == 0u) {
                    CHECK(r.fill3_n == 11u, "%s: the third element's fill must be 10 passes (11 in phase 2), "
                          "got %u", nm, r.fill3_n);
                    CHECK(r.done == (l == 0u ? 96 : 95), "%s: completion at pass %d, the decode's host model "
                          "%d", nm, r.done, l == 0u ? 96 : 95);
                }
                CHECK(gsf_e469c_n() > 0u, "%s: the garage fill-time term was never evaluated", nm);
            }
        }
        s_temp_byte = 0x4Fu;
    }
    {   /* (n) negative: each OLD stand-in must fail the (e) shape */
        shape_t r;
        engagement("(n1) P->D, OLD progress stand-ins (dump idle cells)", 4u, GS_HOST_OLD_PROGRESS,
                   END_D, false);
        r = shape_of(0u);
        printf("  shape: done %.2f s cause %u, 0x0B passes %u\n", r.done_s, r.done_cause, r.b0b_n);
        CHECK(shape_bad(&r, 11u, false, "n1") > 0, "(n1) the old progress stand-ins pass the new shape");
        CHECK(r.done_s < 0.6, "(n1) expected the old 0.45-0.47 s completion, got %.2f", r.done_s);
        engagement("(n2) P->D, OLD 0xE469C stand-in (0)", 4u, GS_HOST_OLD_E469C, END_D, false);
        r = shape_of(0u);
        printf("  shape: third element phase 2 for %u passes, done %.2f s\n", r.fill3_n, r.done_s);
        CHECK(shape_bad(&r, 11u, false, "n2") > 0, "(n2) the old 0xE469C stand-in passes the new shape");
        CHECK(r.fill3_n == 9u, "(n2) expected the old 8-pass fill (9 phase-2 passes), got %u", r.fill3_n);
        engagement("(n2) P->R, OLD 0xE469C stand-in (0)", 2u, GS_HOST_OLD_E469C, END_R, false);
        r = shape_of(0u);
        CHECK(shape_bad(&r, 11u, false, "n2R") > 0 && r.fill3_n == 9u,
              "(n2) P->R with the old 0xE469C stand-in: D fill %u passes", r.fill3_n);
    }
    {   /* (g) the timeout: a turbine that never synchronises (700 rpm) */
        static const char nm[] = "(g) P->D, turbine 700 rpm (never synchronises)";
        shape_t r;
        s_tur_inject = 700u;
        engagement(nm, 4u, 0u, END_D, false);
        r = shape_of(0u);
        s_tur_inject = 0u;
        printf("  slip %d, B phase 9 at tick %d, 0x0A at %d, 0x0B at %d, 0x0E at %d (%d passes after "
               "phase 9), cause %u, %.2f s\n", gs_shadow_slip(), s_obs.b_ph9, s_obs.b_ph0a, s_obs.b_ph0b,
               s_obs.b_ph0e,
               s_obs.b_ph0e - s_obs.b_ph9, r.done_cause, r.done_s);
        CHECK(r.sync < 0, "(g) the context synchronised (pass %d) with a 700 rpm turbine", r.sync);
        CHECK(r.done_cause == 2u, "(g) completion cause %u, expected 2 (the timeout)", r.done_cause);
        CHECK(s_obs.b_ph9 >= 0 && s_obs.b_ph0e - s_obs.b_ph9 >= 250 && s_obs.b_ph0e - s_obs.b_ph9 <= 252,
              "(g) the 0x0E ramp %d passes after B's phase 9, expected the 250-pass timeout",
              s_obs.b_ph0e - s_obs.b_ph9);
        /* 9 ends on its 30-pass map (0x10E7D8, cal blk58+0x94 = 30), 0x0A has
         * no duration: B waits there on BD76 >= -2 (sync) until the timeout */
        CHECK(s_obs.b_ph0a == s_obs.b_ph9 + 30 && s_obs.b_ph0b < 0,
              "(g) B must leave 9 after 30 passes and wait in 0x0A (0x0A at tick %d, 0x0B at %d)",
              s_obs.b_ph0a, s_obs.b_ph0b);
        CHECK(gs_shadow_bd76() == -500 || gs_shadow_sync() != 0u,
              "(g) BD76 %d while not synchronised (the floored -500 expected)", gs_shadow_bd76());
    }
    {   /* (n3) reaction 37 (host-forced): released elements at i_min 0, WK / Sys
         * stay 500; the shadow only (the live law's 0xD9CBC stand-in is
         * test_m5prod's) */
        engagement("(n3) P->D with reaction 37 active", 4u, GS_HOST_R37,
                   (const uint16_t[7]){ 8500u, 8500u, 8500u, 8500u, 0u, 500u, 500u }, false);
        CHECK(gs_shadow_cur(4) == 0u && gs_shadow_cur(5) == 500u && gs_shadow_cur(6) == 500u,
              "(n3) reaction 37: C %u (expected 0), WK %u, Sys %u (expected 500)",
              gs_shadow_cur(4), gs_shadow_cur(5), gs_shadow_cur(6));
    }
    HOST_STANDINS(0u, 0, 0u);
    test_release();
    {   /* (f) the bench tool's gs-check sequence: P D P R P, 2 s each */
        const volatile gs_wit_t *w;
        run(&k_scen[5], true, NULL, NULL);
        w = gs_shadow_wit();
        printf("(f) gs-check sequence P-D-P-R-P: commits %u, last %u -> %u, recorder slots %u / %u, "
               "release passes %u, 0x104A28 stand-in %u; end pf %u gear %d\n",
               w->commits, w->commit_old, w->commit_new, w->rec_n[0], w->rec_n[1], w->rel_n,
               w->seq_n, gs_shadow_pf(), gs_shadow_gear());
        dump_rec(1u, 112u);
        CHECK(w->commits == 4u && w->rec_n[0] == GS_REC_N && w->rec_n[1] == GS_REC_N,
              "(f) expected 4 commits (0-1, 1-0, 0-2, 2-0) and both slots full");
        CHECK(gs_shadow_rec(1u, 105u)->cur[1] == 500u && gs_shadow_rec(1u, 105u)->pf == 2u,
              "(f) slot 1 must hold the completed R engagement (D 500, pf 2)");
        {
            const volatile gs_rec_t *last = gs_shadow_rec(1u, GS_REC_N - 1u);
            CHECK((last->sync & 1u) != 0u && last->ph[0] == 0x1Eu,
                  "(f) slot 1 must reach past the completion (sync, B stopped)");
        }
        CHECK(gs_shadow_pf() == 0u, "(f) back in P: pf 0");
    }
#else
    engagement("(b) P->D, stock-faithful", 4u, 0u, END_D, false);
    engagement("(c) P->R, stock-faithful", 2u, 0u, END_R, false);
#endif
#if defined(GS_DUAL)
    gs_dual_report("test_gs_shadow, in-process scenarios");
    CHECK(gs_dual_passes() > 1000u, "gs_dual: only %u passes compared", gs_dual_passes());
    CHECK(gs_dual_bad() == 0u, "gs_dual: %u passes where the candidate differs from the shadow",
          gs_dual_bad());
#endif
    printf("%s (%d failures)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
    return s_fail == 0 ? 0 : 1;
}
