/* test_g6a.c — G6a, THE DUAL-RUN ON THE REAL CHAIN (2026-11-19,
 * ~/tcu_sweep/g6a_port.md; plan ~/tcu_sweep/g6_migration_plan.md §3 G6a).
 *
 * The harness of test_gs_bridge_chain.c (included, GS_LIVE_DIR 3: the
 * image's bridge) with the G6a candidate and comparator ticked exactly as
 * app/main.c ticks them: g6a_init after gs_bridge_init, the candidate's boot
 * pass at boot state 1 (bsw/sup/sup_mode.c's weak hook, after
 * rq_prod_boot_pass), g6a_tick_10ms after the bridge. The candidate is built
 * in its SH form (GSC_G6A: request objects at 0xFFF925C0 inside the hashed
 * cell window, no recorders, the live AE28, its own boot pass).
 *
 * Every scenario runs twice, WITHOUT and WITH the candidate:
 *   (1) LIVE IDENTITY — every frame: every bus-A word in wire order + the
 *       park demands (the wire), and the cell window 0xFFF80000..0xFFF96000
 *       without the shadow's and the candidate's own RAM; at the end the
 *       whole window byte for byte: the only bytes that differ lie in the
 *       candidate's own block .gsc_req (0xFFF925C0..0xFFF92860);
 *   (2) THE COMPARATOR — 0 UNEXPECTED disagreements; the boot window closed
 *       within G6A_BOOT_MAX passes; the law's 0x94ED4 equals the predicted
 *       wire value on every law pass after 'U' (law_x 0); the replicated
 *       guard never fell back; the candidate commits / completes as the
 *       shadow; the live AE28 equals the template's own lookup (ae28_x 0).
 * Scenarios: the gs-check P D P R P with the brake + engine 700 sims (both
 * 10 ms parities), the bench's long-P shape, P held 0.3 s (R interrupts the
 * release), lever P 50 ms after D (mid-fill), the engine stalling in D (0x108
 * valid at 0 rpm), the plain boot (no frames). With GSB_ENG_PASSES (build
 * test_g6a_englim) the bridge latches mid-engagement: FENCED, never UNEXP.
 * Informational: instance 1 with its 0xE469C fill-time term OFF (the r04l
 * bench recorder's signature, ~/tcu_sweep/g6a_port.md §6) — the comparator's
 * expected count on the bench.
 * Negatives (separate builds; each PASSES only if the defect is caught):
 *   G6A_NEG 1 (GSC_NEG_C3D): the candidate perturbed inside its own RAM —
 *     identity holds, the comparator must report UNEXPECTED;
 *   G6A_NEG 2 (GSC_NEG_LIVE_ALIAS): the candidate bound onto the LIVE request
 *     objects — the identity must fail.
 */
#define GSB_CHAIN_NO_MAIN 1
#define G6A_TEST 1
#include "test_gs_bridge_chain.c"

#include "app/gs_cand.h"

#ifndef G6A_NEG
#  define G6A_NEG 0
#endif

#define WIN_N   0x16000u

/* the candidate's boot pass at boot state 1 (app/main.c's g6a_boot_pass,
 * the inputs as task10 builds them) */
void g6a_boot_pass(void)
{
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
    };
    g6a_boot(&gi);
}

typedef struct {
    g6a_t c;
    uint32_t sh_commits, cand_commits, sh_passes, cand_passes;
    uint8_t  sh_cause, cand_cause, latch, pad;
    uint32_t ae28_x, applied_n, guard_n, cand_unp, cand_rel_n, sh_rel_n;
    uint32_t cand_cnt;      /* nonzero bytes in the candidate's request block */
    unsigned m1, u;
} res_t;

static uint8_t g_win[2][WIN_N];

typedef struct {
    const char *name;
    unsigned kR, kP1, kP2, parity;
    unsigned eng0;          /* engine stall: frames after mode 1 (0 never) */
    bool nosim;
    bool sh_term_off;       /* instance 1's 0xE469C term off (bench emulation, informational) */
    bool info;
} scen_t;

static void run_child(const scen_t *sc, bool on, gr_t *g, res_t *res, uint8_t *win)
{
    int fd[2];
    pid_t pid;
    memset(g, 0, sizeof *g);
    g->name = sc->name;
    g->bridge = true;
    g->real = true;
    g->parity = sc->parity;
    g->kR = sc->kR;
    g->kP1 = sc->kP1;
    g->kP2 = sc->kP2;
    if (pipe(fd) != 0) {
        perror("pipe");
        exit(2);
    }
    fflush(stdout);
    pid = fork();
    if (pid == 0) {
        close(fd[0]);
        g6a_on = on;
        g6a_eng0_from = sc->eng0;
        g6a_nosim = sc->nosim;
        if (sc->sh_term_off) {
            gsf_host_e469c_off(true);                /* instance 1 only (gcf_* is the candidate's) */
        }
        run_g(g);
        memset(res, 0, sizeof *res);
        if (on) {
            res->c = *(const g6a_t *)(const void *)g6a_state();
            res->cand_commits = gs_cand_wit()->commits;
            res->cand_passes = gs_cand_wit()->passes;
            res->cand_cause = gs_cand_wit()->done_cause;
            res->ae28_x = gs_cand_ae28_x();
            res->cand_unp = gs_cand_wit()->unp_n;
            res->cand_rel_n = gs_cand_wit()->rel_n;
        }
        res->sh_commits = gs_shadow_wit()->commits;
        res->sh_passes = gs_shadow_wit()->passes;
        res->sh_cause = gs_shadow_wit()->done_cause;
        res->sh_rel_n = gs_shadow_wit()->rel_n;
        res->applied_n = gs_bridge_state()->applied_n;
        res->guard_n = gs_bridge_state()->guard_n;
        res->latch = gs_bridge_win()->latch;
        res->m1 = g->m1;
        res->u = g->r.u_frame;
        for (uint32_t a = 0xFFF925C0u; a < 0xFFF92860u; a++) {
            res->cand_cnt += (*(const volatile uint8_t *)(uintptr_t)a != 0u) ? 1u : 0u;
        }
        memcpy(win, (const void *)(uintptr_t)0xFFF80000u, WIN_N);
        if (write(fd[1], g, sizeof *g) != (ssize_t)sizeof *g ||
            write(fd[1], res, sizeof *res) != (ssize_t)sizeof *res) {
            _exit(3);
        }
        {
            size_t off = 0u;
            while (off < WIN_N) {
                const ssize_t w = write(fd[1], win + off, WIN_N - off);
                if (w <= 0) {
                    _exit(3);
                }
                off += (size_t)w;
            }
        }
#if defined(G6A_HOSTTIME)
        if (on) {
            printf("HOSTTIME %s: shadow tick mean %.0f ns max %.0f ns (n %u) | candidate + comparator mean %.0f ns "
                   "max %.0f ns (n %u)\n", sc->name, g6a_ns_sum[0] / g6a_ns_n[0], g6a_ns_max[0], g6a_ns_n[0],
                   g6a_ns_sum[1] / g6a_ns_n[1], g6a_ns_max[1], g6a_ns_n[1]);
        }
#endif
        fflush(stdout);
        _exit(0);
    }
    close(fd[1]);
    {
        uint8_t *dst[3] = { (uint8_t *)g, (uint8_t *)res, win };
        const size_t n[3] = { sizeof *g, sizeof *res, WIN_N };
        for (unsigned k = 0u; k < 3u; k++) {
            size_t got = 0u;
            while (got < n[k]) {
                const ssize_t r = read(fd[0], dst[k] + got, n[k] - got);
                if (r <= 0) {
                    printf("FAIL %s: child run incomplete\n", sc->name);
                    g_fails++;
                    break;
                }
                got += (size_t)r;
            }
        }
        close(fd[0]);
        waitpid(pid, NULL, 0);
    }
}

static const char *CLS[G6A_NCLS] = { "AGREE", "BOOT", "FENCED", "FSMTAIL", "UNEXP" };

static void print_rec(const char *tag, const g6a_rec_t *r)
{
    printf("      %s pass %4u %-7s dcur %02X dms %04X src %02X cand E28E %u pf %u ph %02X/%02X shadow ph %02X/%02X"
           " | cand A D B E C %4u %4u %4u %4u %4u | wire %4u %4u %4u %4u %4u\n", tag, r->pass, CLS[r->cls],
           r->dcur, r->dms, r->src, r->e28e_pf & 0x0Fu, r->e28e_pf >> 4, r->cph[0], r->cph[1], r->sph[0],
           r->sph[1], r->cand[0], r->cand[1], r->cand[2], r->cand[3], r->cand[4], r->wire[0], r->wire[1],
           r->wire[2], r->wire[3], r->wire[4]);
}

static gr_t g_ga, g_gb;
static res_t g_ra, g_rb;
static unsigned g_neg_unexp, g_neg_ident;

static void scenario(const scen_t *sc)
{
    unsigned wd = 0u, hd = 0u, first = 0u, outside = 0u, inside = 0u;
    uint32_t o_lo = 0u, o_hi = 0u;
    const g6a_t *c = &g_rb.c;

    run_child(sc, false, &g_ga, &g_ra, g_win[0]);
    run_child(sc, true, &g_gb, &g_rb, g_win[1]);
    for (unsigned f = 0u; f < GFRAMES; f++) {
        if (g_ga.hw[f] != g_gb.hw[f]) {
            wd++;
            first = first ? first : f + 1u;
        }
        if (g_ga.h[f] != g_gb.h[f]) {
            hd++;
            first = first ? first : f + 1u;
        }
    }
    for (uint32_t i = 0u; i < WIN_N; i++) {
        if (g_win[0][i] != g_win[1][i]) {
            const uint32_t a = 0xFFF80000u + i;
            if (a >= 0xFFF925C0u && a < 0xFFF92860u) {
                inside++;
            } else {
                if (outside == 0u) {
                    o_lo = a;
                }
                o_hi = a;
                outside++;
            }
        }
    }
    printf("== %s: mode 1 F%u 'U' F%u | (1) wire frames differ %u, window frames differ %u (first F%u); "
           "end-of-run window bytes differing: %u inside .gsc_req, %u outside", sc->name, g_rb.m1, g_rb.u, wd, hd,
           first, inside, outside);
    if (outside != 0u) {
        printf(" (0x%08X..0x%08X)", (unsigned)o_lo, (unsigned)o_hi);
    }
    printf("\n");
    printf("   (2) comparator: passes %u | AGREE %u BOOT %u FENCED %u FSMTAIL %u UNEXP %u | boot window closed at "
           "pass %u (%u disagreeing) | cand!=shadow while applied %u | applied %u | law checks %u mismatches %u | "
           "guard fallbacks %u | ae28_x %u\n", (unsigned)c->passes, (unsigned)c->cls_n[0], (unsigned)c->cls_n[1],
           (unsigned)c->cls_n[2], (unsigned)c->cls_n[3], (unsigned)c->cls_n[4], c->boot_close, c->boot_dis,
           (unsigned)c->cvs_n, (unsigned)c->applied_n, (unsigned)c->law_n, (unsigned)c->law_x,
           (unsigned)c->guard_x, (unsigned)g_rb.ae28_x);
    printf("       per channel A D B E C WK Sys: disagreeing passes %u %u %u %u %u %u %u, unexpected %u %u %u %u %u %u %u;"
           " mode/state-only passes %u\n", (unsigned)c->dis_ch[0], (unsigned)c->dis_ch[1], (unsigned)c->dis_ch[2],
           (unsigned)c->dis_ch[3], (unsigned)c->dis_ch[4], (unsigned)c->dis_ch[5], (unsigned)c->dis_ch[6],
           (unsigned)c->unexp_ch[0], (unsigned)c->unexp_ch[1], (unsigned)c->unexp_ch[2], (unsigned)c->unexp_ch[3],
           (unsigned)c->unexp_ch[4], (unsigned)c->unexp_ch[5], (unsigned)c->unexp_ch[6], (unsigned)c->ms_dis_n);
    printf("       commits shadow %u / candidate %u, completion cause %u / %u, release passes %u / %u, "
           "passes %u / %u; bridge applied %u guard %u latch %02X; candidate booted %u, unported %u\n",
           (unsigned)g_rb.sh_commits, (unsigned)g_rb.cand_commits, g_rb.sh_cause, g_rb.cand_cause,
           (unsigned)g_rb.sh_rel_n, (unsigned)g_rb.cand_rel_n, (unsigned)g_rb.sh_passes, (unsigned)g_rb.cand_passes,
           (unsigned)g_rb.applied_n, (unsigned)g_rb.guard_n, g_rb.latch, c->booted, (unsigned)g_rb.cand_unp);
    for (unsigned i = 0u; i < c->rec_n && i < 6u; i++) {
        print_rec("rec ", &c->rec[i]);
    }
    for (unsigned i = 0u; i < c->urec_n; i++) {
        print_rec("UNEX", &c->urec[i]);
    }
    if (c->first_unexp != 0xFFFFFFFFu) {
        printf("       unexpected from pass %u to %u\n", (unsigned)c->first_unexp, (unsigned)c->last_unexp);
    }
    g_neg_unexp += c->cls_n[G6A_C_UNEXP];
    g_neg_ident += wd + hd + outside;
#if G6A_NEG == 0
    /* (1) identity — every run */
    GCHECK(wd == 0u && hd == 0u, "%s: the live path differs with the candidate (wire %u, window %u frames)",
           sc->name, wd, hd);
    GCHECK(outside == 0u, "%s: %u window bytes outside the candidate's block differ", sc->name, outside);
    GCHECK(inside > 0u && g_rb.cand_cnt > 0u, "%s: the candidate's block never changed (it did not run)", sc->name);
    GCHECK(c->magic == G6A_MAGIC && c->booted == 1u, "%s: comparator magic %08X booted %u", sc->name,
           (unsigned)c->magic, c->booted);
    if (!sc->info) {
        GCHECK(c->cls_n[G6A_C_UNEXP] == 0u, "%s: %u UNEXPECTED disagreements", sc->name,
               (unsigned)c->cls_n[G6A_C_UNEXP]);
        GCHECK(c->boot_close != 0xFFFFu && c->boot_close <= G6A_BOOT_MAX, "%s: boot window closed at %u",
               sc->name, c->boot_close);
        GCHECK(c->boot_dis > 0u, "%s: no boot-window disagreement (the E28E transient must show)", sc->name);
        GCHECK(c->law_x == 0u && c->law_n + 10u >= c->passes, "%s: law cross-check %u of %u", sc->name,
               (unsigned)c->law_x, (unsigned)c->law_n);
        GCHECK(c->guard_x == 0u, "%s: replicated guard fell back %u", sc->name, (unsigned)c->guard_x);
        GCHECK(c->cvs_n == 0u, "%s: candidate != shadow on %u bridge-applied passes", sc->name, (unsigned)c->cvs_n);
        GCHECK(g_rb.ae28_x == 0u, "%s: live AE28 vs own lookup %u", sc->name, (unsigned)g_rb.ae28_x);
        GCHECK(g_rb.cand_commits == g_rb.sh_commits, "%s: commits shadow %u candidate %u", sc->name,
               (unsigned)g_rb.sh_commits, (unsigned)g_rb.cand_commits);
        GCHECK(g_rb.cand_unp == 0u, "%s: candidate unported %u", sc->name, (unsigned)g_rb.cand_unp);
    } else {
        printf("       (informational run)\n");
    }
#endif
}

int main(void)
{
    static const scen_t S[] = {
        { .name = "gs-check P D P R P (sims)" },
        { .name = "gs-check par1", .parity = 1u },
        { .name = "long P (bench shape)", .kP1 = 360u, .kR = 820u, .kP2 = 1220u },
        { .name = "P held 0.3 s (R interrupts the release)", .kR = K_P1 + 60u },
        { .name = "lever P 50 ms after D (mid-fill)", .kP1 = K_D + 5u, .kR = K_D + 300u, .kP2 = K_D + 700u },
        { .name = "engine stall in D (0x108 valid 0 rpm)", .eng0 = 300u },
        { .name = "plain boot (no frames)", .nosim = true },
#if G6A_NEG == 0 && !defined(G6A_ENGLIM)
        { .name = "BENCH EMULATION: instance 1's 0xE469C term off", .sh_term_off = true, .info = true,
          .kP1 = 360u, .kR = 820u, .kP2 = 1220u },
#endif
    };
    const unsigned n = (G6A_NEG != 0) ? 1u : (unsigned)(sizeof S / sizeof S[0]);
    for (unsigned k = 0u; k < n; k++) {
        scenario(&S[k]);
    }
#if G6A_NEG == 1
    printf("G6A_NEG 1 (candidate perturbed in its own RAM): UNEXPECTED %u, identity defects %u\n", g_neg_unexp,
           g_neg_ident);
    return (g_neg_unexp > 0u && g_neg_ident == 0u) ? 0 : 1;
#elif G6A_NEG == 2
    printf("G6A_NEG 2 (candidate bound onto the live request objects): identity defects %u\n", g_neg_ident);
    return g_neg_ident > 0u ? 0 : 1;
#else
    if (g_fails == 0) {
        printf("test_g6a: ok\n");
    }
    return g_fails != 0;
#endif
}
