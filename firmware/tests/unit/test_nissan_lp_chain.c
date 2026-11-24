/* test_nissan_lp_chain.c - THE NISSAN LINE-PRESSURE LIVE SWITCH ON THE REAL CHAIN (2026-11-23,
 * app/nissan_lp.h; the Nissan configure only: NISSAN_LP_LIVE=1).
 *
 * The harness of test_gs_bridge_chain.c (included, as test_g6b: GS_LIVE_DIR 3, GS_G6B 1, the
 * candidate under the monitor, the supervisor + request law + output diagnosis on the real
 * feedback model) over the gs-check (P D P R P, brake + engine 700 sims). After every law pass
 * (frame callback nlp_law_pass) the law's gated Sys current 0x94ED4[6] is checked against what
 * the switch must put there: the seam's value (nissan_lp_sys_request(), inside the stock range
 * [500, 8500]) while the monitor's seam carries the candidate (gsb_t.src == GSM_S_CAND), else
 * 500 (the P content latched / fenced, or the r03f table before the boot pass). The gates are
 * test_nissan_lp's; here the value is forced (nissan_lp_host_force) so the PLUMBING is proven:
 *   (A) forced 6000: the law carries 6000 on every candidate pass in 'U', the guard never falls
 *       back, the monitor never trips (the WK / Sys channels are outside its bounds - unchanged)
 *   (B) forced 9000 (outside the stock Sys range): the guard holds 500, counted
 *   (C) forced 6000 and a monitor trip (B at 9000 in D, its compile-time bound): the GUARD latch,
 *       and from the trip on Sys is the P content's 500 on every law pass to the end of the run
 *       (latched: never back), 6000 before it
 *   (D) not forced (the gates decide; this harness feeds neither the line-pressure nor the
 *       fault-reaction shadow): 500 everywhere - the switch-on build idles at the stand-in */
#define GSB_CHAIN_NO_MAIN 1
#define G6A_TEST 1
#define G6B_TEST 1
#define NLP_CHAIN 1
#include "test_gs_bridge_chain.c"
#include "app/nissan_lp.h"

#if !GS_G6B || !DRV_RQ_SYS_LIVE
#  error "test_nissan_lp_chain: build with GS_G6B=1 and NISSAN_LP_LIVE=1"
#endif

#define LAW_SYS() (*(const volatile uint16_t *)(uintptr_t)(0xFFF94ED4u + 12u))

void g6a_boot_pass(void)
{
    G6R_REPLICA_BOOT();
    if (!g6a_on) {
        return;
    }
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
#if defined(GS_IN_HAS_C3D)
        .c3d = *(const volatile uint8_t *)(uintptr_t)0xFFF89C3Du,
#endif
    };
    g6a_boot(&gi);
    gs_bridge_boot_pass();
    g6a_boot_compare();
}

typedef struct {
    uint32_t cand_n, cand_bad, cand_live;   /* 'U' law passes with the seam on the candidate */
    uint32_t pc_n, pc_bad;                  /* 'U' law passes with the P content / before the seam */
    uint32_t after_trip_n, after_trip_bad;  /* law passes after the first trip */
    uint32_t before_trip_live;
    uint32_t trip_pass, trip_n;
    uint8_t  latch;
    uint32_t guard_n;                       /* drv_reqlaw.c's guard fallbacks */
    uint32_t d_commit;                      /* the bridge pass of the D commit */
    uint16_t first_bad_v, first_bad_want;
    unsigned first_bad_frame;
    unsigned diag;                          /* the output diagnosis: fault events + switch-offs + DSM + would-reset */
    uint32_t unexp, law_x, law_n, guard_x;  /* the G6b comparator (app/g6a.c; ch 6 out of its law check here) */
} nres_t;

static nres_t s_nr;
static uint16_t s_force;

static void nlp_law_pass(unsigned frame)
{
    const volatile gsb_t *b = gs_bridge_state();
    const uint16_t f = nissan_lp_sys_request();
    const bool tripped = b->trip_n != 0u;
    uint16_t want;
    static bool s_prev_u;
    const bool u = CELL_SEQ2_STATE[0] == (uint8_t)'U';
    const bool was_u = s_prev_u;
    s_prev_u = u;
    if (!u || !was_u) {
        return;                             /* the law's 'U' gate (read at the law's start): 0 before it */
    }
    if (b->src == GSM_S_CAND) {
        want = (f >= DRV_RQ_SYS_LIVE_MIN && f <= DRV_RQ_SYS_LIVE_MAX) ? f : DRV_RQ_PCONT_SYS;
        s_nr.cand_n++;
    } else {
        want = DRV_RQ_PCONT_SYS;
        s_nr.pc_n++;
    }
    if (LAW_SYS() != want) {
        if (b->src == GSM_S_CAND) {
            s_nr.cand_bad++;
        } else {
            s_nr.pc_bad++;
        }
        if (s_nr.first_bad_frame == 0u) {
            s_nr.first_bad_frame = frame;
            s_nr.first_bad_v = LAW_SYS();
            s_nr.first_bad_want = want;
        }
    }
    if (b->src == GSM_S_CAND && LAW_SYS() == s_force && s_force != DRV_RQ_PCONT_SYS) {
        s_nr.cand_live++;
        if (!tripped) {
            s_nr.before_trip_live++;
        }
    }
    if (tripped) {
        s_nr.after_trip_n++;
        if (LAW_SYS() != DRV_RQ_PCONT_SYS) {
            s_nr.after_trip_bad++;
        }
    }
}

static void nlp_run(uint16_t force, int inj, uint32_t inj_at, nres_t *out)
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
        gr_t g;
        close(fd[0]);
        memset(&g, 0, sizeof g);
        g.name = "nissan lp chain";
        g.bridge = true;
        g.real = true;
        g6a_on = true;
        memset(&s_nr, 0, sizeof s_nr);
        s_force = force;
        nissan_lp_host_force(force);
        if (inj) {
            gs_bridge_host_inject(2u, 9000u, inj_at, inj_at + 3u);
        }
        run_g(&g);
        s_nr.trip_n = gs_bridge_state()->trip_n;
        s_nr.trip_pass = gs_bridge_state()->trip_pass;
        s_nr.latch = gs_bridge_win()->latch;
        s_nr.guard_n = drv_rq_m5_guard_n();
        s_nr.d_commit = gs_bridge_state()->slot[0].t0;
        s_nr.diag = g.r.fw_set_events + g.r.switched_off + g.r.gfail_tot + g.r.gpend_tot + g.r.fatal_tot;
        s_nr.unexp = g6a_state()->cls_n[G6A_C_UNEXP];
        s_nr.law_x = g6a_state()->law_x;
        s_nr.law_n = g6a_state()->law_n;
        s_nr.guard_x = g6a_state()->guard_x;
        if (write(fd[1], &s_nr, sizeof s_nr) != (ssize_t)sizeof s_nr) {
            _exit(3);
        }
        _exit(0);
    }
    close(fd[1]);
    memset(out, 0, sizeof *out);
    if (read(fd[0], out, sizeof *out) != (ssize_t)sizeof *out) {
        printf("FAIL: child result\n");
        g_fails++;
    }
    close(fd[0]);
    (void)waitpid(pid, NULL, 0);
}

static void line(const char *tag, const nres_t *r)
{
    printf("== %s: cand passes %u (bad %u, at the forced value %u) | P-content passes %u (bad %u) | trips %u at "
           "pass %u latch %02X | after the trip %u (bad %u), live before it %u | guard fallbacks %u | D commit "
           "pass %u",
           tag, (unsigned)r->cand_n, (unsigned)r->cand_bad, (unsigned)r->cand_live, (unsigned)r->pc_n,
           (unsigned)r->pc_bad, (unsigned)r->trip_n, (unsigned)r->trip_pass, r->latch, (unsigned)r->after_trip_n,
           (unsigned)r->after_trip_bad, (unsigned)r->before_trip_live, (unsigned)r->guard_n,
           (unsigned)r->d_commit);
    printf(" | diag %u | g6a UNEXP %u law %u/%u guard %u", r->diag, (unsigned)r->unexp, (unsigned)r->law_x,
           (unsigned)r->law_n, (unsigned)r->guard_x);
    if (r->first_bad_frame != 0u) {
        printf(" | first bad frame %u: %u want %u", r->first_bad_frame, r->first_bad_v, r->first_bad_want);
    }
    printf("\n");
}

int main(void)
{
    nres_t a, b, c, d;

    nlp_run(6000u, 0, 0u, &a);
    line("(A) forced 6000", &a);
    GCHECK(a.cand_n > 200u && a.cand_bad == 0u && a.cand_live == a.cand_n, "(A) the law carries the seam's 6000");
    GCHECK(a.pc_bad == 0u, "(A) the P-content passes hold 500");
    GCHECK(a.guard_n == 0u && a.trip_n == 0u && a.latch == 0u, "(A) no guard fallback, no trip");
    GCHECK(a.d_commit != 0u, "(A) the D engagement committed");
    GCHECK(a.diag == 0u, "(A) the output diagnosis on the real feedback silent (%u)", a.diag);
    GCHECK(a.unexp == 0u && a.law_x == 0u && a.law_n > 200u && a.guard_x == 0u,
           "(A) the G6b comparator: UNEXP 0, law cross-check (ch 0..5) clean");

    nlp_run(9000u, 0, 0u, &b);
    line("(B) forced 9000 (out of range)", &b);
    GCHECK(b.cand_n > 200u && b.cand_bad == 0u && b.cand_live == 0u, "(B) the guard holds 500");
    GCHECK(b.guard_n >= b.cand_n, "(B) every out-of-range value counted (%u of %u)", (unsigned)b.guard_n,
           (unsigned)b.cand_n);
    GCHECK(b.trip_n == 0u, "(B) the monitor does not see Sys");

    nlp_run(6000u, 1, a.d_commit + 30u, &c);
    line("(C) forced 6000 + a monitor trip in D", &c);
    GCHECK(c.trip_n != 0u && (c.latch & GSB_L_GUARD) != 0u, "(C) the monitor latched (latch %02X)", c.latch);
    GCHECK(c.trip_pass >= a.d_commit + 30u && c.trip_pass <= a.d_commit + 31u, "(C) tripped in its pass (%u)",
           (unsigned)c.trip_pass);
    GCHECK(c.before_trip_live > 0u, "(C) Sys was live (6000) before the trip");
    GCHECK(c.after_trip_n > 100u && c.after_trip_bad == 0u, "(C) Sys 500 on every law pass after the trip");
    GCHECK(c.cand_bad == 0u && c.pc_bad == 0u, "(C) every pass as the rule");

    nlp_run(0u, 0, 0u, &d);
    line("(D) not forced (gates closed here)", &d);
    GCHECK(d.cand_n > 200u && d.cand_bad == 0u && d.cand_live == 0u && d.pc_bad == 0u, "(D) 500 everywhere");
    GCHECK(d.guard_n == 0u && d.trip_n == 0u, "(D) no fallback, no trip");
    GCHECK(d.diag == 0u && d.unexp == 0u && d.law_x == 0u, "(D) diagnosis silent, comparator clean");

    if (g_fails == 0) {
        printf("test_nissan_lp_chain: ok\n");
    }
    return g_fails != 0;
}
