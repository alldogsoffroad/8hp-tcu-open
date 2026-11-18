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
 *   (e) dry-bench completion: the dump's progress cells (normal completion
 *       through B's final ramp) vs progress never satisfied (the timeout).
 *
 *   test_gs_shadow            run all (prints the traces)
 */
#define _POSIX_C_SOURCE 200809L
#include "app/gs_shadow.h"
#include "app/gs_fill.h"
#include "app/park.h"
#include "app/park_law.h"
#include "app/rq_prod.h"
#include "app/rng_chain.h"
#include "bsw/com/crc8.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_park.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_rqprod.h"
#include "bsw/sup/sup_cells.h"
#include "io/sensors/sensors.h"
#include "io/vehicle/vehicle.h"
#include "safety/kl15.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static sensors_in_t s_z;
const sensors_in_t *sensors_in(void) { return &s_z; }

static int s_fail;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); s_fail++; } } while (0)

static uint8_t s_ctr[0x800];
static void send(can_bus_t bus, uint16_t id, uint8_t dlc, bool prot, const uint8_t *data)
{
    can_frame_t f = { .id = id, .dlc = dlc };
    memcpy(f.data, data, dlc);
    if (prot) {
        s_ctr[id] = (uint8_t)((s_ctr[id] + 1u) & 0x0Fu);
        f.data[dlc - 2u] = (uint8_t)((f.data[dlc - 2u] & 0x0Fu) | (uint8_t)(s_ctr[id] << 4));
        f.data[dlc - 1u] = crc8_j1850(f.data, (uint8_t)(dlc - 1u));
    }
    vehicle_rx(bus, &f);
}

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
    };
    gs_shadow_tick_10ms(&gi);
}

typedef void (*tick_cb_t)(uint32_t t, uint8_t lever);

/* run one scenario; returns per-tick hashes into hs (n ticks) */
static void run(const scen_t *sc, bool shadow, uint64_t *hs, tick_cb_t cb)
{
    memset(s_ctr, 0, sizeof s_ctr);
    memset((void *)(uintptr_t)0xFFF80000u, 0, 0x16000u);
    memset(&s_z, 0, sizeof s_z);
    s_z.q_output_speed = Q_OK;
    vehicle_init();
    park_init();
    rng_chain_init();
    kl15_init();
    *(volatile uint16_t *)(uintptr_t)KL15_SRC_CELL = 13206u;
    drv_jobs_init();
    *(volatile uint8_t *)0xFFF89C13u = 0x4Fu;   /* the bench's 38.9 °C (r03u) */
    *(volatile uint8_t *)0xFFF89CD2u = 0x4Fu;
    drv_rq_prod_host_select(true);
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        drv_rq_request_set(ch, 0u);
    }
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    CELL_LAD_94114 = 0u;
    CELL_DIAG_94115 = 0u;
    CELL_J2_STAGED = 0x9020u;
    CELL_LAD_REQ[0] = 0u;
    CELL_LAD_REQ[1] = 0u;
    CELL_OUT_MV[0] = 12940u;
    rq_prod_boot_pass();
    if (shadow) {
        gs_shadow_init();
    }
    for (uint32_t t = 0u; t < sc->nseg * SEG; t++) {
        const uint8_t code = sc->sched[t / SEG];
        uint8_t d[8];
        uint64_t h = 0xCBF29CE484222325ull;

        if (code != 0u && t >= sc->lever_from &&
            !(t >= sc->gap_from && t < sc->gap_from + sc->gap_len)) {
            const uint8_t l[5] = { code, 0, 0, 0, 0 };
            send(CAN_BUS1, 0x2EAu, 5u, true, l);
            send(CAN_BUS2, 0x210u, 5u, true, l);
        }
        if (sc->brake) {
            memset(d, 0, sizeof d);
            d[0] = 0x04u;
            send(CAN_BUS1, 0x140u, 8u, true, d);
        }
        if (sc->engine != 0u) {
            memset(d, 0, sizeof d);
            d[0] = (uint8_t)(sc->engine >> 8);
            d[1] = (uint8_t)sc->engine;
            d[2] = 0x1Fu;
            d[3] = 0xFFu;
            send(CAN_BUS1, 0x108u, 8u, true, d);
        }
        /* main.c's task_10ms order */
        rq_prod_task_10ms(1u);
        vehicle_update_10ms();
        kl15_tick_10ms();
        rng_chain_tick_10ms(1u, 1u, 1u);
        park_tick_10ms();
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
        _exit(0);
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
        waitpid(pid, NULL, 0);
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

/* ── (b) (c) (e): one P → X engagement, printed ─────────────────────── */
static void dump_rec(uint8_t slot, unsigned upto)
{
    const volatile gs_wit_t *w = gs_shadow_wit();
    printf("  rec slot %u: %u entries\n", slot, w->rec_n[slot]);
    printf("  pass pf fl  ph0 ph1 ph2 md  set gr |    A    D    B    E    C   WK  Sys\n");
    for (unsigned i = 0u; i < w->rec_n[slot] && i < upto; i++) {
        const volatile gs_rec_t *r = gs_shadow_rec(slot, (uint8_t)i);
        printf("  %4u %2u %02X  %3X %3X %3X %02X %02X %3d | %4u %4u %4u %4u %4u %4u %4u\n",
               r->pass, r->pf, r->flags, r->ph[0], r->ph[1], r->ph[2], r->modes, r->reqset,
               r->gear, r->cur[0], r->cur[1], r->cur[2], r->cur[3], r->cur[4], r->cur[5], r->cur[6]);
    }
}

typedef struct {
    int c_first, b_start, b_ph4, done_tick, commit_tick;
    uint16_t b_ph4_cur, c_fill_cur, b_fill_cur;
    uint8_t phmax_b;
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
    if (s_obs.commit_tick >= 0 && s_obs.done_tick < 0 && gs_shadow_pending() == 0u &&
        gs_shadow_pf() != 0u) {
        s_obs.done_tick = (int)t;
    }
    if (gs_shadow_phase(0) == 4u && s_obs.b_ph4 < 0) {
        int16_t v[6];
        s_obs.b_ph4 = (int)t;
        s_obs.b_ph4_cur = gs_shadow_cur(2);
        gs_shadow_host_sub(0u, v);
        printf("  B phase 4 entry: +0xC %d, +0x24 %d, +0x54 %d, +0x30 %d; B result +2 %d mbar\n",
               v[0], v[1], v[2], v[3], v[5]);
        gs_shadow_host_sub(2u, v);
        printf("  third element then: phase %X +0xC %d, +0x24 %d, +0x54 %d, result +2 %d mbar\n",
               gs_shadow_phase(2), v[0], v[1], v[2], v[5]);
    }
    if (gs_shadow_phase(0) == 2u && s_obs.b_start < 0) {
        s_obs.b_start = (int)t;
    }
    if (gs_shadow_phase(2) == 2u && s_obs.c_first < 0) {
        s_obs.c_first = (int)t;
        s_obs.c_fill_cur = 0u;
    }
}

static void engagement(const char *name, uint8_t lever, int16_t bd76, uint8_t bd30,
                       const uint16_t end[7], bool print)
{
    static const scen_t sp = { "P then X", { 1, 9, 9, 9, 9, 9 }, 6u, true, 700u, 0u, 0u, 0u };
    scen_t sc = sp;
    for (unsigned i = 1u; i < 6u; i++) {
        sc.sched[i] = lever;
    }
    memset(&s_obs, 0xFF, sizeof s_obs);
    s_obs.c_fill_cur = s_obs.b_ph4_cur = s_obs.b_fill_cur = 0u;
    s_target_lever = lever;
    gs_shadow_host_progress(bd76, bd30);
    run(&sc, true, NULL, obs_cb);
    {
        const volatile gs_wit_t *w = gs_shadow_wit();
        printf("%s: commits %u (last %u → %u at pass %u), done cause %u at pass %u, "
               "unp %u, e896 %u, lim %u, rel %u, seq %u, loop cap %u; fill unp %u f19f %u shift %u\n",
               name, w->commits, w->commit_old, w->commit_new, w->commit_pass, w->done_cause,
               w->done_pass, w->unp_n, w->e896_n, w->lim_n, w->rel_n, w->seq_n, w->loop_cap_n,
               gsf_unp_n(), gsf_f19f_n(), gsf_shift_n());
        printf("  observed: commit tick %d, C phase 2 at %d, B phase 2 at %d, B phase 4 at %d "
               "(B %u), completion at %d (%.2f s after the commit)\n",
               s_obs.commit_tick, s_obs.c_first, s_obs.b_start, s_obs.b_ph4, s_obs.b_ph4_cur,
               s_obs.done_tick,
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

int main(void)
{
    /* the end states: the holder's block-1 row, binary currents (0.1 mA) */
    static const uint16_t END_D[7] = { 8500u, 8500u, 8500u, 8500u, 500u, 500u, 500u };
    static const uint16_t END_R[7] = { 8500u, 500u, 8500u, 8500u, 8500u, 500u, 500u };

    printf("garage shadow, GS_FORCED = %d\n", GS_FORCED);
    test_identity();
    engagement("(b) P->D, dump progress cells", 4u, 0x7FFF, 0xFFu, END_D, true);
    engagement("(c) P->R, dump progress cells", 2u, 0x7FFF, 0xFFu, END_R, true);
#if GS_FORCED
    engagement("(e) P->D, progress never satisfied (BD76 -32768, BD30 0)", 4u, -32768, 0u,
               END_D, false);
    {
        const volatile gs_wit_t *w = gs_shadow_wit();
        CHECK(w->done_cause == 2u, "(e) the dry-bench completion must be the timeout (cause %u)",
              w->done_cause);
    }
    engagement("(e) P->R, progress never satisfied", 2u, -32768, 0u, END_R, false);
#endif
    gs_shadow_host_progress(0x7FFF, 0xFFu);
#if GS_FORCED
    {   /* (f) the bench tool's gs-check sequence: P D P R P, 2 s each */
        const volatile gs_wit_t *w;
        run(&k_scen[5], true, NULL, NULL);
        w = gs_shadow_wit();
        printf("(f) gs-check sequence P-D-P-R-P: commits %u, last %u -> %u, recorder slots %u / %u, "
               "release stand-in %u, 0x104A28 stand-in %u; end pf %u gear %d\n",
               w->commits, w->commit_old, w->commit_new, w->rec_n[0], w->rec_n[1], w->rel_n,
               w->seq_n, gs_shadow_pf(), gs_shadow_gear());
        dump_rec(1u, 50u);
        CHECK(w->commits == 4u && w->rec_n[0] == GS_REC_N && w->rec_n[1] == GS_REC_N,
              "(f) expected 4 commits (0-1, 1-0, 0-2, 2-0) and both slots full");
        CHECK(gs_shadow_rec(1u, 60u)->cur[1] == 500u && gs_shadow_rec(1u, 60u)->pf == 2u,
              "(f) slot 1 must hold the completed R engagement (D 500, pf 2)");
        CHECK(gs_shadow_pf() == 0u, "(f) back in P: pf 0");
    }
#endif
    printf("%s (%d failures)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
    return s_fail == 0 ? 0 : 1;
}
