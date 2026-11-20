/* test_g6b.c — G6b, THE SWITCH ON THE REAL CHAIN (2026-11-19,
 * ~/tcu_sweep/g6b_port.md; plan ~/tcu_sweep/g6_migration_plan.md §3 G6b).
 *
 * The harness of test_gs_bridge_chain.c (included; GS_LIVE_DIR 3, GS_G6B 1,
 * the candidate in its SH form GSC_G6A) with the candidate ticked as
 * app/main.c ticks it in G6b: its boot pass at boot state 1, its 10 ms pass
 * BEFORE the monitor (app/gs_bridge.c mon_tick), the comparison after.
 *
 * (1) THE WIRE vs r04n — every scenario runs twice in one binary: the r04n
 *     form (gs_bridge_host_g6b(false): the bridge of the shadow, the
 *     candidate read-only after it — r04n's main.c order) and the G6b form
 *     (the candidate is the producer of the live A..E currents through the
 *     monitor). Every frame: every bus-A word in wire order + the park
 *     demands (the wire), and the cell window 0xFFF80000..0xFFF96000; at the
 *     end the whole window byte for byte. Expected: identical everywhere.
 * (2) THE G6b COMPARATOR (app/g6a.c compare_g6b) — UNEXP 0, the boot window
 *     closed within G6A_BOOT_MAX passes, the law cross-check, the widened
 *     guard never fell back, candidate == shadow on every candidate pass
 *     after the boot window; the monitor never tripped; the output
 *     diagnosis on the real feedback silent.
 * (3) THE E28E FENCE — the boot transient fenced (the live producer's boot
 *     fill on the wire) and, in the engine-stall run, the all-release fenced:
 *     the candidate's A at 500 while the staged A word stays at 8500.
 * (4) THE MONITOR NEGATIVES (G6b form only; each against the base run): an
 *     envelope / bound / window / permit violation injected into what the
 *     monitor reads of the candidate (gs_bridge_host_inject) or its inputs —
 *     each must trip in its pass, put the P content on the wire from the next
 *     F stage to the end of the run (latched: never back, even when the
 *     permit returns), and leave every frame before the trip identical.
 *     Build test_g6b_bypass (GSM_HOST_BYPASS: the checks skipped) must FAIL.
 * Scenarios: the gs-check P D P R P with the brake + engine 700 sims (both
 * 10 ms parities), the bench's long-P shape, P held 0.3 s (R interrupts the
 * release), lever P 50 ms after D (mid-fill), the engine stalling in D
 * (0x108 valid at 0 rpm), the plain boot (no frames). test_g6b_englim
 * (GSB_ENG_PASSES 50): the 3.5 s limit trips mid-engagement in both forms.
 */
/* G6c (2026-11-19, ~/tcu_sweep/g6c_port.md): THE SHADOW AND THE r04n BRIDGE
 * FORM ARE DELETED (D-G6-7). (1) is now THE WIRE vs r04o: every run is
 * compared, frame by frame and tick by tick, with the goldens the G6b form of
 * r04o (HEAD fff32bd, tests/unit/g6c_gold.h; G6C_GOLD_IN) wrote — every bus-A
 * word, the park demands, the cell window, the monitor's source / latch /
 * fence / trips per tick, gsb_t and the .noinit windows at the end, the end-
 * of-run window byte for byte. (2) keeps the comparator's wire self-checks
 * (law cross-check, guard replica, UNEXP 0). (4) the monitor negatives: 12 of
 * r04o's 13 trip in the same pass with the same latch, identical to the
 * goldens; the 13th (candidate != shadow) had its check deleted with the
 * shadow — it must NOT trip now (the injected in-bounds value reaches the
 * wire, as any in-bounds candidate value does), and every frame before the
 * injection must equal the golden. */
#define GSB_CHAIN_NO_MAIN 1
#define G6A_TEST 1
#define G6B_TEST 1
#include "test_gs_bridge_chain.c"

#if !GS_G6B
#  error "test_g6b: build with GS_G6B=1"
#endif

#define WIN_N   0x16000u

void g6a_boot_pass(void)
{
    G6R_REPLICA_BOOT();             /* r04r: the retired holder's boot pass (r04q: just before) */
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
        .c3d = *(const volatile uint8_t *)(uintptr_t)0xFFF89C3Du,   /* G5c: app/main.c gs_in_build */
#endif
    };
    g6a_boot(&gi);
    gs_bridge_boot_pass();          /* r04q: as app/main.c g6a_boot_pass */
    g6a_boot_compare();
}

/* a negative: what is injected, where (bridge passes), what must trip */
typedef struct {
    const char *name;
    uint8_t  kind;          /* N_* */
    uint8_t  ch;            /* inject: the channel */
    uint16_t v;             /* inject: the value */
    int32_t  from_off;      /* relative to the anchor (bridge pass) */
    uint32_t len;           /* passes */
    uint8_t  anchor;        /* A_D_COMMIT, A_R_COMMIT, A_D_REL */
    uint8_t  latch;         /* the expected latch bit */
    int32_t  trip_off;      /* the expected trip pass − (anchor + from_off); −1: not exact */
    uint8_t  injb;          /* r04q N_INJB: also inject the E28E / A mode the monitor reads */
    uint8_t  injb_e28e;     /* 0xFF unchanged */
    uint8_t  injb_am;       /* 0xFF unchanged */
} neg_t;
#define N_INJ     1u
#define N_RELWIN  2u
#define N_ENG     3u
#define N_REL     4u
#define N_WIN     5u
#define N_COMMITS 6u
#define N_BOOT2   7u        /* r04q: a second boot entry after monitor pass anchor + from_off */
#define N_BLEN    8u        /* r04q: the boot allowance's bound lowered to len passes */
#define A_D_COMMIT 0u
#define A_R_COMMIT 1u
#define A_D_REL    2u
#define A_BOOT     3u       /* r04q: the monitor pass of the boot entry (gsb_t.boot_p0) */

typedef struct {
    const char *name;
    unsigned kR, kP1, kP2, parity;
    unsigned eng0;
    unsigned engoff;        /* r04q: 0x108 frames stop (the bench's engine-sim stop) */
    bool nosim;
    bool info;              /* informational: printed, the r04n identity not required */
    bool rfirst;            /* r04s: R first, then D (lever_at) */
#if defined(G5B_SCEN)
    unsigned engon;         /* G5b: 0x108 only from m1 + engon (the sim started after boot) */
    unsigned engone;        /* G5b: ONE 0x108 frame at m1 + engone (a stray frame) */
    unsigned keyoff, keyon; /* G5b: 0x112 off from m1 + keyoff, on again from m1 + keyon */
    bool holdm1;            /* G5c: the hand-off 0x68EFE suppressed (mode 2 never granted) */
    unsigned c3d_from, c3d_to;  /* G5c: the engine's 0x89C3D forced 1 on [m1 + from, m1 + to) (host injection) */
    uint8_t g5c;            /* G5c: a case that CHANGES behaviour by design (1 a, 2 b, 3 c, 4 d) */
#endif
    const neg_t *neg;
    uint32_t anchors[4];    /* negatives: from the base run */
} scen_t;

typedef struct {
    g6a_t c;
    gsb_t b;
    gsb_win_t w;
    uint32_t rel_t0[2];
    uint16_t rel_trip_n;
    uint8_t  rel_trip_code;
    uint8_t  pad;
    uint16_t relev, relref;
    uint32_t cand_commits, sh_commits, ae28_x, cand_unp;
    uint32_t m5_guard;      /* r04r: drv_reqlaw.c's guard fallbacks (drv_rq_m5_guard_n) */
    run_r_t  r;
    unsigned m1, u, tk_n;
#if defined(GS_IN_HAS_C3D)
    gs_host_g5c_t g5c;      /* G5c: the engine's 0x89C3D witnesses (host only) */
#endif
    unsigned g5b_m2, g5b_back1, g5b_again2;
} res_t;

static uint8_t g_win[2][WIN_N];
static tick_t  g_tks[2][GFRAMES];

static void run_child(const scen_t *sc, bool g6b, gr_t *g, res_t *res, uint8_t *win, tick_t *tks)
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
        g6a_on = true;
        (void)g6b;                                  /* G6c: the G6b form only */
        g6a_eng0_from = sc->eng0;
        g6q_engoff_from = sc->engoff;
        g6a_nosim = sc->nosim;
        g_rfirst = sc->rfirst;                      /* r04s */
#if defined(G5B_SCEN)
        g5b_engon_from = sc->engon;
        g5b_eng_one = sc->engone;
        g5b_keyoff_from = sc->keyoff;
        g5b_keyon_from = sc->keyon;
        g5c_hold_m1 = sc->holdm1;
        g5c_c3d_from = sc->c3d_from;
        g5c_c3d_to = sc->c3d_to;
#endif
        if (sc->neg != NULL) {
            const neg_t *n = sc->neg;
            const uint32_t at = (uint32_t)((int32_t)sc->anchors[n->anchor] + n->from_off);
            if (n->injb != 0u) {
                gs_bridge_host_inject_boot(n->injb_e28e, n->injb_am, at, at + n->len);
            }
            switch (n->kind) {
            case N_INJ:     if (n->ch != 0xFFu) { gs_bridge_host_inject(n->ch, n->v, at, at + n->len); } break;
            case N_BOOT2:   g6q_boot2_at = at; break;
            case N_BLEN:    gs_bridge_host_boot_limit((uint16_t)n->len); break;
            case N_RELWIN:  g6b_relwin_from = at; g6b_relwin_to = at + n->len; break;
            case N_ENG:     gs_bridge_host_limits(50u, GSB_REL_PASSES, GSB_WIN_MS, GSB_COMMITS_MAX); break;
            case N_REL:     gs_bridge_host_limits(GSB_ENG_PASSES, 50u, GSB_WIN_MS, GSB_COMMITS_MAX); break;
            case N_WIN:     gs_bridge_host_limits(GSB_ENG_PASSES, GSB_REL_PASSES, 3000u, GSB_COMMITS_MAX); break;
            case N_COMMITS: gs_bridge_host_limits(GSB_ENG_PASSES, GSB_REL_PASSES, GSB_WIN_MS, 1u); break;
            default: break;
            }
        }
        run_g(g);
#if defined(SM_TEST)
        {
            const volatile sm_t *sm = sm_shadow_state();
            fprintf(stderr, "SM run '%s': passes %u walks %u lookups %u err %u a697 %u a0aa %u ce8 %u "
                    "skip_mode %u arb_skip %u corr %u conf %u\n", sc->name, (unsigned)sm->pass_n,
                    (unsigned)sm->walk_n, (unsigned)sm->lookup_n, (unsigned)sm->err_n, (unsigned)sm->a697,
                    (unsigned)sm->a0aa, (unsigned)sm->ce8, (unsigned)sm->skip_mode_n,
                    (unsigned)sm->arb_skip_n, (unsigned)sm->corr_n, (unsigned)sm->conf_n);
            const volatile gf_t *gf = sm_gear_state();
            fprintf(stderr, "GF run '%s': passes %u e4a7 %d e69f %d c28 %d e6a1 %d strobes %u done %u "
                    "agree %u dis %u unp %u err %u neg9 %u gear_in %d\n", sc->name, (unsigned)gf->pass_n,
                    (int)gf->e4a7, (int)gf->e69f, (int)gf->c28[0], (int)gf->e6a1, (unsigned)gf->strobe_n,
                    (unsigned)gf->done_n, (unsigned)gf->agree_n, (unsigned)gf->dis_n, (unsigned)gf->unp_n,
                    (unsigned)gf->err_n, (unsigned)sm->neg9_n, (int)sm->in.gear_in);
        }
#endif
        if (getenv("G6C_DUMP_PINNED") != NULL &&
            (strcmp(getenv("G6C_DUMP_PINNED"), sc->name) == 0 || strcmp(getenv("G6C_DUMP_PINNED"), "*") == 0)) {
            printf("PINRUN %s\n", sc->name);
            /* G6c: the engine's pinned stock objects at the end of the run
             * (the report's dump comparison) — host_state blocks 11..22,
             * r04s: + the sub-objects, their cells, E6C1 / E6C2 (23..27) */
            for (uint8_t k = 11u; k <= 27u; k++) {
                uint32_t n = 0u;
                const volatile uint8_t *b = (const volatile uint8_t *)gs_cand_host_state(k, &n);
                printf("PINNED %u ", k);
                for (uint32_t i = 0u; b != NULL && i < n; i++) {
                    printf("%02X", b[i]);
                }
                printf("\n");
            }
            {   /* r04s: every start's reversal inputs (the last 8) */
                uint8_t v[12];
                const uint8_t n = gs_cand_host_r04s(0u, v);
                for (uint8_t k = 0u; k < (n < 8u ? n : 8u); k++) {
                    (void)gs_cand_host_r04s(k, v);
                    printf("START pass %u pf %u flag %u E6C1 %u E6C2 %d E34E %u BD44 %u BD1C/1D %u/%u "
                           "D0DC+1E %u D1B4+1E %u D1B4+3A %d\n", (unsigned)(v[10] | (v[11] << 8)), v[0], v[1], v[2],
                           (int)(int8_t)v[3], v[4], v[5], v[6] & 1u, v[6] >> 1, v[7], v[8], (int)(int8_t)v[9]);
                }
            }
        }
        memset(res, 0, sizeof *res);
        res->c = *(const g6a_t *)(const void *)g6a_state();
        res->b = *(const gsb_t *)(const void *)gs_bridge_state();
        res->w = *(const gsb_win_t *)(const void *)gs_bridge_win();
        res->rel_t0[0] = gs_bridge_rel()->slot[0].t0;
        res->rel_t0[1] = gs_bridge_rel()->slot[1].t0;
        res->rel_trip_n = gs_bridge_rel()->trip_n;
        res->rel_trip_code = gs_bridge_rel()->trip_code;
        res->relev = gs_bridge_rel()->ev;
        res->relref = gs_bridge_rel()->refused;
        res->cand_commits = gs_cand_wit()->commits;
        res->sh_commits = gs_cand_wit()->commits;   /* G6c: no shadow */
        res->ae28_x = gs_cand_ae28_x();
        res->cand_unp = gs_cand_wit()->unp_n;
        res->m5_guard = drv_rq_m5_guard_n();
        res->r = g->r;
        res->m1 = g->m1;
        res->u = g->r.u_frame;
        res->tk_n = g_tk_n;
#if defined(GS_IN_HAS_C3D)
        gs_cand_host_g5c(&res->g5c);
#endif
#if defined(G5B_SEL)
        res->g5b_m2 = g->g5b_m2;
        res->g5b_back1 = g->g5b_back1;
        res->g5b_again2 = g->g5b_again2;
#endif
        memcpy(tks, g_tk, sizeof g_tk);
        memcpy(win, (const void *)(uintptr_t)0xFFF80000u, WIN_N);
        {
            const void *src[4] = { g, res, win, tks };
            const size_t n[4] = { sizeof *g, sizeof *res, WIN_N, sizeof g_tk };
            for (unsigned k = 0u; k < 4u; k++) {
                size_t off = 0u;
                while (off < n[k]) {
                    const ssize_t w = write(fd[1], (const char *)src[k] + off, n[k] - off);
                    if (w <= 0) {
                        _exit(3);
                    }
                    off += (size_t)w;
                }
            }
        }
        fflush(stdout);
        _exit(0);
    }
    close(fd[1]);
    {
        uint8_t *dst[4] = { (uint8_t *)g, (uint8_t *)res, win, (uint8_t *)tks };
        const size_t n[4] = { sizeof *g, sizeof *res, WIN_N, sizeof g_tk };
        for (unsigned k = 0u; k < 4u; k++) {
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

#include "g6c_gold.h"
#if defined(G6A_ENGLIM)
#  define G6C_PFX "englim "
#else
#  define G6C_PFX ""
#endif
static unsigned g6c_cmp(const char *name, const gr_t *g, const res_t *r, const uint8_t *win, const tick_t *tks)
{
    char n[200];
    snprintf(n, sizeof n, "%s%s", G6C_PFX, name);
    if (getenv("G6C_GOLD_OUT") != NULL) {
        g6c_gold_put(n, g, r, win, tks, r->tk_n);
        return 0u;
    }
    if (getenv("G6C_GOLD_IN") == NULL) {
        printf("FAIL %s: G6C_GOLD_IN (the r04o goldens) not set\n", n);
        return 1u;
    }
    return g6c_gold_cmp(n, g, r, win, tks, r->tk_n, 0u, 0u);
}
static gr_t g_ga, g_gb;
static res_t g_ra, g_rb;
static res_t g_base;        /* the base gs-check run in G6b form (anchors, the negatives' reference) */
static gr_t g_gbase;

static void monitor_line(const char *tag, const res_t *r)
{
    const gsb_t *b = &r->b;
    printf("   %s monitor: passes %u | candidate %u pre-boot %u P content %u | stall-fence entries %u exits %u "
           "(first exit pass %u) | trips %u (first: pass %d why %04X ch %d v %u) latch %02X | armed %u acc %u ms "
           "commits %u | shadow-diff passes %u | release trips %u | boot %02X (%u eval.)\n", tag, (unsigned)b->passes,
           (unsigned)b->cand_n, (unsigned)b->live_n, (unsigned)b->pc_n, b->fence_ev, b->unfence_n,
           (unsigned)b->unfence_pass, b->trip_n, (b->trip_pass == 0xFFFFFFFFu) ? -1 : (int)b->trip_pass,
           b->trip_why, (b->trip_ch == 0xFFu) ? -1 : (int)b->trip_ch, b->trip_v, r->w.latch,
           (unsigned)b->armed_n, (unsigned)r->w.acc_ms, r->w.commits, (unsigned)b->shd_n, r->rel_trip_n,
           b->boot_st, b->boot_len);
}

#if defined(G5B_SEL)
/* G5b (~/tcu_sweep/g5b_port.md): THE LIVE OS MODE in every run — the mode
 * cells and the selector at the end of the run (the end window), the entry
 * frame, the latch, the slot-26 flags and the monitor's cross-check (gsb_t
 * m2: zero disagreeing passes). The wire / cell identity with r04t is the
 * golden comparison (the G5b-masked HEAD goldens). */
static unsigned g5b_runs, g5b_m2_runs;
#define W8(w, a) ((w)[(uint32_t)(a) - 0xFFF80000u])
static void g5b_check(const char *name, bool sims, unsigned engon, unsigned engone, unsigned keyoff,
                      unsigned keyon, const gr_t *g, const res_t *r, const uint8_t *w)
{
    const uint8_t mode = W8(w, 0xFFF93B33u), req = W8(w, OS_SEL_OBJ + 7u), app = W8(w, OS_SEL_OBJ + 8u);
    const uint8_t r5e = W8(w, 0xFFF93B5Eu), echo = W8(w, OS_SEL_ECHO_LAST);
    const uint8_t f3c = W8(w, OS_FLG_C3C), f3d = W8(w, OS_FLG_C3D), f3e = W8(w, OS_FLG_C3E);
    const uint8_t f53 = W8(w, OS_FLG_C53), f54 = W8(w, OS_FLG_C54), f55 = W8(w, OS_FLG_C55);
    uint32_t n, n2;
    memcpy(&n, &w[OS_SEL_OBJ + 0x10u - 0xFFF80000u], 4u);
    memcpy(&n2, &w[OS_SEL_OBJ + 0x14u - 0xFFF80000u], 4u);
    const bool want2 = sims;
    /* the mode-2 entry: the 5 ms batch 1 ms after the first mode-1 10 ms task
     * that saw a valid 0x108 >= 150 rpm (the harness's frames are 5 ms, the
     * 10 ms task runs on every second one) */
    const unsigned at = (g->g5b_e1 > g->g5b_t1) ? g->g5b_e1 : g->g5b_t1;
    (void)engon;
    (void)engone;
    bool ok;
    g5b_runs++;
    if (want2) {
        /* key-off: E9FF 2 → 3 → 4 over two 10 ms tasks, the selector reads
         * it one task later, the batch applies: mode 1 within 3 tasks (6
         * frames); key-on: E9FF 4 → 1 → 2, then the selector: 2 within 6 */
        const bool ko = keyoff == 0u ||
                        (g->g5b_back1 > g->m1 + keyoff && g->g5b_back1 <= g->m1 + keyoff + 6u &&
                         g->g5b_again2 > g->m1 + keyon && g->g5b_again2 <= g->m1 + keyon + 6u);
        ok = mode == 2u && req == 2u && app == 2u && r5e == 2u && echo == 2u && at != 0u && g->g5b_h2 == at &&
             g->g5b_b2 != 0u && g->g5b_m2 == g->g5b_b2 && ko &&
             (keyoff != 0u || g->g5b_drop == 0u) && g->g5b_max == 2u && f3e == 1u && f53 == 1u && f3c == 0u && f3d == 0u &&
             f54 == 0u && f55 == 0u;
        if (ok) {
            g5b_m2_runs++;
        }
    } else {
        ok = mode == 1u && req == 1u && app == 1u && r5e == 1u && echo == 1u && g->g5b_m2 == 0u &&
             g->g5b_max == 1u && n2 == 0u && f3d == 1u && f55 == 1u && f3c == 0u && f3e == 0u && f53 == 0u &&
             f54 == 0u;
    }
    ok = ok && r->b.m2.pad == 0u && r->b.m2.n != 0u && n != 0u;
    printf("   G5b [%s]: mode 1 F%u (first mode-1 task F%u, first 0x108 in it F%u), 2 handed over by the task of "
           "F%u (want %s F%u), applied by the batch of F%u, mode 2 F%u, drops %u, max %u | end: mode %u req %u echo +8 %u "
           "0x93B5E %u last %u flags 3C/3D/3E %u%u%u 53/54/55 %u%u%u | selector passes %u (2: %u) | m2 "
           "cross-check passes %u disagreeing %u => %s\n", name, g->m1, g->g5b_t1, g->g5b_e1, g->g5b_h2,
           want2 ? "" : "never, not", want2 ? at : 0u, g->g5b_b2, g->g5b_m2, g->g5b_drop, g->g5b_max, mode, req,
           app, r5e, echo, f3c, f3d, f3e, f53, f54, f55,
           (unsigned)n, (unsigned)n2, (unsigned)r->b.m2.n, (unsigned)r->b.m2.pad, ok ? "AS REQUIRED" : "WRONG");
    if (keyoff != 0u) {
        printf("   G5b [%s]: key-off at F%u → mode 1 at F%u; key-on at F%u → mode 2 at F%u\n", name,
               g->m1 + keyoff, g->g5b_back1, g->m1 + keyon, g->g5b_again2);
    }
    GCHECK(ok, "G5b %s: the live OS mode", name);
}
#endif

#if defined(G5C_SCEN)
/* G5c (~/tcu_sweep/g5c_port.md): the 0x89C3D cases. The engine's witnesses
 * (gs_cand_host_g5c, host only), the ticks (the staged A..E words) and the
 * monitor; gd = the differences from the r04u engine in the same scenario
 * (the HEAD goldens) — expected nonzero where the stock's gate acts. */
static unsigned g5c_runs_ok;
static unsigned tick_bpass_at_frame(unsigned frame)
{
    for (unsigned i = 0u; i < g_rb.tk_n; i++) {
        if (g_tks[1][i].frame >= frame) {
            return (unsigned)g_tks[1][i].bpass;
        }
    }
    return 0xFFFFu;
}
/* the staged words equal up to the setpoint rounding phase ((I + 0/4/1) / 6:
 * one count) and the same command bits */
static bool g5c_same_content(const uint16_t *a, const uint16_t *b)
{
    for (unsigned c = 0u; c < 5u; c++) {
        if ((a[c] & 0xF800u) != (b[c] & 0xF800u) || absd(a[c] & 0x7FFu, b[c] & 0x7FFu) > 1u) {
            return false;
        }
    }
    return true;
}
static void g5c_check(const scen_t *sc, unsigned gd)
{
#if !defined(GS_IN_HAS_C3D)
    /* a HEAD (r04u) build writing the goldens: no G5c witnesses */
    printf("   G5c [%s]: (r04u engine: golden only) OS mode 2 F%u back to 1 F%u 2 again F%u, D commit monitor pass "
           "%u\n", sc->name, g_rb.g5b_m2, g_rb.g5b_back1, g_rb.g5b_again2, (unsigned)g_rb.b.slot[0].t0);
    (void)gd;
    (void)tick_bpass_at_frame;
    (void)g5c_same_content;
    return;
#else
    const gs_host_g5c_t *h = &g_rb.g5c;
    const gsb_t *b = &g_rb.b;
    const g6a_t *c = &g_rb.c;
    bool ok = b->trip_n == 0u && g_rb.w.latch == 0u && c->cls_n[G6A_C_UNEXP] == 0u && c->law_x == 0u &&
              c->guard_x == 0u && g_rb.r.fatal_tot == 0u && g_rb.r.switched_off == 0u &&
              g_rb.r.fw_set_events == 0u;
    printf("   G5c [%s]: engine passes %u, 0x89C3D passes %u, first 0x89C3D-0 pass %u | nonzero commits %u "
           "(first at pass %u: E6C5 %u c3d %u E4AC+5 %u; last at pass %u: E6C5 %u c3d %u E4AC+5 %u) | 0xDC37E refusals: c3d %u park %u settle %u | 0xD15A6 "
           "returns %u | now E6C5 %u E4AC+5 %u c3d %u | OS mode 2 F%u, back to 1 F%u, 2 again F%u | vs r04u: %u "
           "differences | monitor trips %u latch %02X UNEXP %u\n", sc->name, (unsigned)h->passes,
           (unsigned)h->c3d_passes, (unsigned)h->m2_first, (unsigned)h->nz_commits, (unsigned)h->first_pass,
           h->first_e6c5, h->first_c3d, h->first_e4ac5, (unsigned)h->commit_pass,
           h->commit_e6c5, h->commit_c3d, h->commit_e4ac5, (unsigned)h->ref_c3d, (unsigned)h->ref_park,
           (unsigned)h->ref_settle, (unsigned)h->d15a6_ret, h->e6c5, h->e4ac5, h->c3d, g_rb.g5b_m2,
           g_rb.g5b_back1, g_rb.g5b_again2, gd, b->trip_n, g_rb.w.latch, (unsigned)c->cls_n[G6A_C_UNEXP]);
    switch (sc->g5c) {
    case 1u: {
        /* (a) the stock's gate: no nonzero commit while 0x89C3D = 1; the
         * staged A..E words stay the P content on every tick after the boot
         * window (= the last tick's) */
        unsigned off = 0u;
        const tick_t *last = &g_tks[1][g_rb.tk_n - 1u];
        unsigned i0 = 0u;
        while (i0 < g_rb.tk_n && g_tks[1][i0].bpass < (uint32_t)b->boot_p0 + b->boot_len) {
            i0++;               /* the first tick after the monitor's boot allowance (A's boot fill) */
        }
        printf("   G5c (a): the P content from tick %u (F%u)\n", i0, i0 < g_rb.tk_n ? g_tks[1][i0].frame : 0u);
        for (unsigned i = i0; i < g_rb.tk_n; i++) {
            if (!g5c_same_content(g_tks[1][i].st, last->st)) {
                if (off < 6u || getenv("G5C_DUMP") != NULL) {
                    printf("      off P: tick %u F%u pass %u lev %u src %u words %04X %04X %04X %04X %04X\n", i,
                           g_tks[1][i].frame, (unsigned)g_tks[1][i].bpass, g_tks[1][i].lev, g_tks[1][i].src,
                           g_tks[1][i].st[0], g_tks[1][i].st[1], g_tks[1][i].st[2], g_tks[1][i].st[3],
                           g_tks[1][i].st[4]);
                }
                off++;
            }
        }
        printf("   G5c (a): ticks off the P content after the boot window %u (P content %04X %04X %04X %04X %04X) "
               "| OS mode at the end %u\n", off, last->st[0], last->st[1], last->st[2], last->st[3], last->st[4],
               g_win[1][0x13B33u]);
        ok = ok && h->nz_commits == 0u && h->ref_c3d > 0u && h->ref_settle == 0u && off == 0u &&
             g_win[1][0x13B33u] == 1u && h->c3d == 1u && h->e6c5 == 0u;
        break;
    }
    case 2u: {
        /* (b) the settle counter: the commit waits until the 24th mode-2 pass */
        const unsigned m2p = tick_bpass_at_frame(g_rb.g5b_m2);
        printf("   G5c (b): mode 2 at F%u (monitor pass %u); D commit at monitor pass %u; engine: first mode-2 "
               "pass %u, commit pass %u = the %u-th mode-2 pass, refused for the settle counter on %u passes\n",
               g_rb.g5b_m2, m2p, (unsigned)b->slot[0].t0, (unsigned)h->m2_first, (unsigned)h->first_pass,
               (unsigned)(h->first_pass - h->m2_first + 1u), (unsigned)h->ref_settle);
        ok = ok && h->nz_commits >= 1u && h->ref_settle > 0u && h->first_e6c5 == 24u && h->first_c3d == 0u &&
             (h->first_pass - h->m2_first + 1u) == 24u && gd != 0u;
        break;
    }
    case 5u:
        /* (e) the mode-1 return taken on the injected passes with D engaged,
         * the wire identical to r04u's */
        ok = ok && h->d15a6_ret > 0u && h->ref_settle == 0u && gd == 0u;
        break;
    case 3u:
    case 4u:
        /* (c)/(d) the key-off cycle: no nonzero commit while the OS mode
         * is 1; (d): the R engagement after the cycle starts with E4AC+5
         * re-armed by the mode-1 passes */
        ok = ok && g_rb.g5b_back1 != 0u && g_rb.g5b_again2 != 0u && h->ref_settle == 0u &&
             (sc->g5c != 4u || h->commit_e4ac5 == 1u);
        break;
    default:
        ok = false;
        break;
    }
    if (ok) {
        g5c_runs_ok++;
    }
    GCHECK(ok, "G5c %s: as the stock's rule", sc->name);
#endif
}
#endif

static void scenario(const scen_t *sc)
{
    const g6a_t *c = &g_rb.c;
    unsigned gd;

    (void)g_ga;
    (void)g_ra;
    run_child(sc, true, &g_gb, &g_rb, g_win[1], g_tks[1]);
    gd = g6c_cmp(sc->name, &g_gb, &g_rb, g_win[1], g_tks[1]);
#if defined(G5B_HASH_MASK)
    printf("== %s: mode 1 F%u 'U' F%u | (1) vs the golden (r04t, G5b-masked: g5b_r04t_test_g6b.tgz): %u "
           "differences\n", sc->name, g_rb.m1, g_rb.u, gd);
#else
    printf("== %s: mode 1 F%u 'U' F%u | (1) vs the golden (r04s: g6s_r04s_test_g6b.tgz): %u differences\n", sc->name, g_rb.m1, g_rb.u,
           gd);
#endif
    printf("   (2) wire self-check: passes %u | AGREE %u BOOT %u FENCED %u FSMTAIL %u UNEXP %u | boot window closed at "
           "pass %u (%u disagreeing) | law checks %u mismatches %u | guard fallbacks %u | ae28_x %u | engine unported %u\n",
           (unsigned)c->passes, (unsigned)c->cls_n[0], (unsigned)c->cls_n[1], (unsigned)c->cls_n[2],
           (unsigned)c->cls_n[3], (unsigned)c->cls_n[4], c->boot_close, c->boot_dis, (unsigned)c->law_n,
           (unsigned)c->law_x, (unsigned)c->guard_x, (unsigned)g_rb.ae28_x, (unsigned)g_rb.cand_unp);
    monitor_line("(2)", &g_rb);
    printf("       commits %u / candidate %u; diag: fault events %u switch-off %u DSM fail %u pend %u "
           "would-reset %u\n", (unsigned)g_rb.sh_commits, (unsigned)g_rb.cand_commits, g_rb.r.fw_set_events,
           g_rb.r.switched_off, g_rb.r.gfail_tot, g_rb.r.gpend_tot, g_rb.r.fatal_tot);
    for (unsigned i = 0u; i < c->rec_n && i < 8u; i++) {
        print_rec("rec ", &c->rec[i]);
    }
    for (unsigned i = 0u; i < c->urec_n; i++) {
        print_rec("UNEX", &c->urec[i]);
    }
#if defined(G5C_SCEN)
    if (sc->g5c != 0u) {
        g5c_check(sc, gd);          /* G5c: a behaviour change by design (not the golden's identity) */
        return;
    }
#endif
    GCHECK(gd == 0u, "%s: differs from the golden (%u)", sc->name, gd);
#if defined(G5B_SEL)
    g5b_check(sc->name, !sc->nosim,
#  if defined(G5B_SCEN)
              sc->engon, sc->engone, sc->keyoff, sc->keyon,
#  else
              0u, 0u, 0u, 0u,
#  endif
              &g_gb, &g_rb, g_win[1]);
#endif
    if (sc->info) {
        return;             /* D -> R direct: the golden identity is the whole check */
    }
    GCHECK(c->magic == G6A_MAGIC && c->booted == 1u, "%s: comparator magic / booted", sc->name);
    GCHECK(c->cls_n[G6A_C_UNEXP] == 0u, "%s: %u UNEXPECTED", sc->name, (unsigned)c->cls_n[G6A_C_UNEXP]);
    GCHECK(c->boot_close != 0xFFFFu && c->boot_close <= G6A_BOOT_MAX, "%s: boot window closed at %u", sc->name,
           c->boot_close);
    GCHECK(c->law_x == 0u && c->law_n + 10u >= c->passes, "%s: law cross-check %u of %u", sc->name,
           (unsigned)c->law_x, (unsigned)c->law_n);
    GCHECK(c->guard_x == 0u, "%s: the widened guard fell back %u", sc->name, (unsigned)c->guard_x);
    GCHECK(g_rb.ae28_x == 0u && g_rb.cand_unp == 0u, "%s: ae28_x %u unported %u", sc->name,
           (unsigned)g_rb.ae28_x, (unsigned)g_rb.cand_unp);
#if defined(G6A_ENGLIM)
    /* the englim build: the 3.5 s limit (50 passes) trips mid-engagement —
     * the r04n bridge disarms latched, the monitor latches the P content */
    GCHECK((g_rb.w.latch & GSB_L_ENGT) != 0u, "%s: the engagement limit did not latch (%02X)", sc->name,
           g_rb.w.latch);
#else
    GCHECK(g_rb.b.trip_n == 0u && g_rb.w.latch == 0u, "%s: the monitor tripped (%u, latch %02X)", sc->name,
           g_rb.b.trip_n, g_rb.w.latch);
#endif
    /* r04q (D-G6-13): no boot fence — the boot allowance carried the boot
     * transient once, ended on the P content within GSB_BOOT_PASSES; the
     * stall fence only in the stall run (D-G6-14) */
    printf("   (2) boot allowance: state %02X entries %u evaluations %u (from monitor pass %u) | stall fence entries %u "
           "exits %u passes %u | pre-boot passes %u\n", g_rb.b.boot_st, g_rb.b.boot_calls, g_rb.b.boot_len,
           (unsigned)g_rb.b.boot_p0, g_rb.b.fence_ev, g_rb.b.unfence_n, (unsigned)g_rb.b.fence_n,
           (unsigned)g_rb.b.live_n);
    GCHECK(g_rb.b.boot_st == GSM_B_USED && g_rb.b.boot_calls == 1u && g_rb.b.boot_len >= 2u &&
           g_rb.b.boot_len <= GSB_BOOT_PASSES, "%s: the boot allowance (state %02X entries %u evaluations %u)",
           sc->name, g_rb.b.boot_st, g_rb.b.boot_calls, g_rb.b.boot_len);
    GCHECK(sc->eng0 != 0u || (g_rb.b.fence_ev == 0u && g_rb.b.fence_n == 0u),
           "%s: a stall fence without a stall (entries %u)", sc->name, g_rb.b.fence_ev);
    GCHECK(g_rb.r.fw_set_events == 0u && g_rb.r.switched_off == 0u && g_rb.r.gfail_tot == 0u &&
           g_rb.r.gpend_tot == 0u && g_rb.r.fatal_tot == 0u, "%s: diagnosis not silent", sc->name);
    if (sc->eng0 != 0u) {
        /* (3) the engine-stop all-release fenced: the candidate's A at 500,
         * the staged A word at the live producer's 8500 (D-G6-12) */
        unsigned n = 0u;
        for (unsigned i = 1u; i < g_rb.tk_n; i++) {
            const tick_t *t = &g_tks[1][i], *q = &g_tks[1][i - 1u];
            if (q->src == GSM_S_FENCE && q->sh[0] == 500u && absd(sp_of(t->st[0]), 8500u) <= 6u &&
                t->frame > g_rb.m1 + sc->eng0) {
                n++;
            }
        }
        printf("   (3) engine stall: fenced passes with the candidate's A at 500 and the staged A at 8500: %u; "
               "fence entries %u\n", n, g_rb.b.fence_ev);
        GCHECK(n > 100u && g_rb.b.fence_ev >= 1u, "%s: the engine-stop all-release not fenced (%u, entries %u)",
               sc->name, n, g_rb.b.fence_ev);
    }
}

/* the negatives: against the base run */
static unsigned g_neg_ok;
static void negative(const neg_t *n)
{
    scen_t sc = { .name = n->name, .neg = n };
    gr_t g;
    res_t r;
    unsigned tripk = 0xFFFFu, pre_bad = 0u, notp = 0u, after = 0u, src_bad = 0u, trip_frame = 0u;
    static const uint16_t PC[5] = { 8500u, 8500u, 500u, 8500u, 8500u };
    bool ok;
    static uint8_t win[WIN_N];
    static tick_t tks[GFRAMES];

    sc.anchors[A_D_COMMIT] = g_base.b.slot[0].t0;
    sc.anchors[A_R_COMMIT] = g_base.b.slot[1].t0;
    sc.anchors[A_D_REL] = g_base.rel_t0[0];
    sc.anchors[A_BOOT] = g_base.b.boot_p0;
    run_child(&sc, true, &g, &r, win, tks);
    if (n->latch == GSB_L_SHADOW) {
        /* G6c: the candidate != shadow check went with the shadow: the
         * injection must NOT trip; every frame before the injected pass is
         * the r04o golden's */
        const uint32_t at0 = (uint32_t)((int32_t)sc.anchors[n->anchor] + n->from_off);
        unsigned lim = GFRAMES, pre = 0u;
        for (unsigned i = 0u; i < r.tk_n; i++) {
            if (tks[i].bpass == at0 && tks[i].frame < lim) {
                lim = tks[i].frame;
            }
        }
        for (unsigned f = 0u; f + 1u < lim && f < GFRAMES; f++) {
            if (g.hw[f] != g_gbase.hw[f]) {
                pre++;
            }
        }
        ok = r.b.trip_n == 0u && r.w.latch == 0u && pre == 0u && lim != GFRAMES;
        printf("== NEG %s: G6c — the check is DELETED with the shadow (D-G6-7): injection at pass %u (F%u), trips %u "
               "latch %02X, frames before the injection differing from the base %u => %s\n", n->name, (unsigned)at0,
               lim, r.b.trip_n, r.w.latch, pre, ok ? "NOT TRIPPED, as required" : "UNEXPECTED");
        monitor_line("   ", &r);
        (void)g6c_cmp(n->name, &g, &r, win, tks);     /* printed: differs from the injection on, by design */
        if (ok) {
            g_neg_ok++;
        }
        GCHECK(ok, "NEG %s: the deleted shadow check", n->name);
        return;
    }
    {
        const unsigned gd = g6c_cmp(n->name, &g, &r, win, tks);
        GCHECK(gd == 0u, "NEG %s: differs from the r04o golden (%u)", n->name, gd);
    }
    for (unsigned i = 0u; i < r.tk_n; i++) {
        if (r.b.trip_pass != 0xFFFFFFFFu && tks[i].bpass == r.b.trip_pass) {
            tripk = i;
            trip_frame = tks[i].frame;
        }
    }
    if (tripk != 0xFFFFu) {
        for (unsigned i = tripk; i < r.tk_n; i++) {
            if (tks[i].src != GSM_S_PCONT) {
                src_bad++;
            }
        }
        for (unsigned i = tripk + 1u; i < r.tk_n; i++) {
            after++;
            for (uint8_t ch = 0u; ch < 5u; ch++) {
                if (absd(sp_of(tks[i].st[ch]), PC[ch]) > 6u) {
                    notp++;
                    break;
                }
            }
        }
        /* the frames before the first injected pass (an injection the
         * monitor lets through by design — the shadow check's 2-pass debounce
         * — reaches the wire for its first pass) or before the trip */
        unsigned lim = trip_frame;
        if (n->kind == N_INJ || n->kind == N_RELWIN) {
            const uint32_t at0 = (uint32_t)((int32_t)sc.anchors[n->anchor] + n->from_off);
            for (unsigned i = 0u; i < r.tk_n; i++) {
                if (tks[i].bpass == at0 && tks[i].frame < lim) {
                    lim = tks[i].frame;
                }
            }
        }
        for (unsigned f = 0u; f + 1u < lim && f < GFRAMES; f++) {
            if (g.hw[f] != g_gbase.hw[f]) {
                pre_bad++;
            }
        }
    }
    {
        const uint32_t at = (uint32_t)((int32_t)sc.anchors[n->anchor] + n->from_off);
        const bool exact_ok = n->trip_off < 0 || r.b.trip_pass == at + (uint32_t)n->trip_off;
        ok = r.b.trip_n >= 1u && (r.w.latch & n->latch) != 0u && tripk != 0xFFFFu && exact_ok && src_bad == 0u &&
             notp == 0u && after > 20u && pre_bad == 0u;
        printf("== NEG %s: inject/anchor pass %u | trip pass %d (want %s%u) latch %02X (want bit %02X) | tick of the "
               "trip %u (F%u): P content on the wire in %u of %u later ticks, seam source not P content in %u ticks; "
               "frames before the trip differing from the base %u => %s\n", n->name, (unsigned)at,
               (r.b.trip_pass == 0xFFFFFFFFu) ? -1 : (int)r.b.trip_pass, n->trip_off < 0 ? ">= " : "",
               (unsigned)(at + (uint32_t)(n->trip_off < 0 ? 0 : n->trip_off)), r.w.latch, n->latch, tripk,
               trip_frame, after - notp, after, src_bad, pre_bad, ok ? "CAUGHT" : "NOT CAUGHT");
        monitor_line("   ", &r);
    }
    if (ok) {
        g_neg_ok++;
    }
    GCHECK(ok, "NEG %s not caught as required", n->name);
}

/* r04r (~/tcu_sweep/r04r_port.md): THE WK / Sys GUARD. The engine supplies
 * ch 5 / 6 through the seam since r04r; drv_reqlaw.c passes only the
 * compile-time 500 (D-G6-10). The engine's Sys (ch 6) is injected at 8500
 * and WK (ch 5) at 0 at the seam (gs_bridge_host_inject: drv_gsbr_take's
 * candidate value) for 20 passes each, in D: the wire must stay the base
 * run's on EVERY frame (the guard holds 500), the guard must count every
 * substituted pass, and the monitor must not trip (WK / Sys are not its
 * channels). */
static unsigned g_supply_ok;
static void supply_guard(uint8_t ch, uint16_t v, int32_t off)
{
    const neg_t n = { "supply", N_INJ, ch, v, off, 20u, A_D_COMMIT, 0u, 0, 0u, 0xFFu, 0xFFu };
    scen_t sc = { .name = (ch == 6u) ? "r04r: Sys 8500 at the seam for 20 passes in D"
                                     : "r04r: WK 0 at the seam for 20 passes in D", .neg = &n };
    gr_t g;
    res_t r;
    static uint8_t win[WIN_N];
    static tick_t tks[GFRAMES];
    unsigned hw_bad = 0u;
    sc.anchors[A_D_COMMIT] = g_base.b.slot[0].t0;
    run_child(&sc, true, &g, &r, win, tks);
    for (unsigned f = 0u; f < GFRAMES; f++) {
        if (g.hw[f] != g_gbase.hw[f]) {
            hw_bad++;
        }
    }
    {
        const bool ok = hw_bad == 0u && r.m5_guard >= 20u && r.m5_guard <= 40u && g_base.m5_guard == 0u &&
                        r.b.trip_n == 0u && r.w.latch == 0u;
        printf("== %s: wire frames differing from the base %u | guard fallbacks %u (base %u) | monitor trips %u "
               "latch %02X => %s\n", sc.name, hw_bad, (unsigned)r.m5_guard, (unsigned)g_base.m5_guard, r.b.trip_n,
               r.w.latch, ok ? "HELD AT 500" : "NOT HELD");
        if (ok) {
            g_supply_ok++;
        }
        GCHECK(ok, "%s: the WK / Sys guard", sc.name);
    }
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
        { .name = "engine frames stop in D (0x108 invalid)", .engoff = 300u, .info = true },
        { .name = "plain boot (no frames)", .nosim = true },
        { .name = "D -> R direct (no P between)", .kR = K_P1, .info = true },
        /* r04s: the reversal's mirror — R, then D with no P between: after
         * a completed R, E6C1 / E6C2 = 2 / −1; the D start's flag, E33C+0x12
         * <= 10 and the R release's third element (D, BD44) still applied
         * → +0x1E, BD1C (0x10E5EE's offset is 0 for D, 0x10F084 0 for E6C2 −1) */
        { .name = "R -> D direct (no P between)", .kR = K_P1, .info = true, .rfirst = true },
#if defined(G5B_SCEN)
        /* G5b (~/tcu_sweep/g5b_port.md): the 0x108 shapes of the bench —
         * the engine sim started 1 s after the mode-1 switch, and a single
         * stray frame 0.5 s after it (the rest of the gs-check unchanged) */
        { .name = "G5b: engine sim starts 1 s after mode 1", .info = true, .engon = 100u },
#  if defined(G5C_SCEN)
        /* G5c: mode 2 enters 50 ms before lever D — the commit now waits for
         * the settle counter (0xDC37E, E6C5 >= 24): a behaviour change by
         * design, checked as case (b) */
        { .name = "G5b: one stray 0x108 frame 0.5 s after mode 1", .info = true, .engone = 50u, .g5c = 2u },
#  else
        { .name = "G5b: one stray 0x108 frame 0.5 s after mode 1", .info = true, .engone = 50u },
#  endif
        /* key-off in D (0x112 code 4 → 0 at m1 + 3 s, after the D engagement
         * completed), key-on 1.5 s later: mode 2 → 1 → 2 (the engine sim
         * still running); the wire is r04t's (key-off auto-P included) */
        { .name = "G5b: key-off in D, key-on 1.5 s later", .info = true, .keyoff = 300u, .keyon = 450u },
#  if defined(G5C_SCEN)
        /* G5c (~/tcu_sweep/g5c_port.md): the cases that CHANGE behaviour by
         * design — 0x89C3D honest. Each is compared with what the r04u
         * engine does in the same scenario (the HEAD goldens) and checked
         * against the stock's rule (g5c_check). */
        { .name = "G5c (a): mode 2 not granted (hand-off held), lever D in mode 1", .info = true,
          .holdm1 = true, .g5c = 1u },
        { .name = "G5c (b): engine sim from m1+250 ms, lever D 50 ms later", .info = true, .engon = 50u,
          .g5c = 2u },
        { .name = "G5c (c): key-off mid-engagement in D, key-on 1.7 s later", .info = true, .keyoff = 110u,
          .keyon = 450u, .g5c = 3u },
        { .name = "G5c (d): key-off after D, key-on after the release, then R", .info = true, .keyoff = 300u,
          .keyon = 600u, .g5c = 4u },
        /* (e) the 0xD15A6 mode-1 return and the slot-40 clear exercised: the
         * engine's 0x89C3D forced 1 for 0.5 s while D is engaged (OS mode
         * 2; host injection) — E6A1 = E69F (1 = the target: no wire change),
         * E6C5 cleared, no commit; the r04u golden ran the same scenario
         * without the input (no .c3d) — the wire must be identical */
        { .name = "G5c (e): 0x89C3D injected for 0.5 s in D (0xD15A6 return)", .info = true, .c3d_from = 300u,
          .c3d_to = 400u, .g5c = 5u },
#  endif
#endif
    };
    /* the negatives (G6b form), anchored on the base run's D commit, R commit
     * and D release start (bridge passes) */
    static const neg_t N[] = {
        /* the compile-time element bounds (engage) */
        { "fixed channel A off its pin in D",       N_INJ, 0u, 8000u, 20, 3, A_D_COMMIT, GSB_L_GUARD, 0 },
        { "moving B above 8500 in D",               N_INJ, 2u, 9000u, 30, 3, A_D_COMMIT, GSB_L_GUARD, 0 },
        { "B mode 1 at 4000 (hold)",                N_INJ, 2u, 4000u, 150, 3, A_D_COMMIT, GSB_L_GUARD, 0 },
        { "fixed channel C off its pin in R",       N_INJ, 4u, 8000u, 40, 3, A_R_COMMIT, GSB_L_GUARD, 0 },
        /* the release envelope */
        { "release: B above its first value",       N_INJ, 2u, 8000u, 10, 3, A_D_REL, GSB_L_GUARD, 0 },
        { "release: C below its first value",       N_INJ, 4u, 600u, 10, 3, A_D_REL, GSB_L_GUARD, 0 },
        { "release: 125-pass limit at 50",          N_REL, 0u, 0u, 0, 0, A_D_REL, GSB_L_GUARD, 50 },
        /* the windows */
        { "3.5 s engagement limit at 50 passes",    N_ENG, 0u, 0u, 0, 0, A_D_COMMIT, GSB_L_ENGT, 50 },
        { "30 s armed budget at 3 s",               N_WIN, 0u, 0u, 0, 0, A_D_COMMIT, GSB_L_WIN, -1 },
        { "16-commit cap at 1",                     N_COMMITS, 0u, 0u, 0, 0, A_R_COMMIT, GSB_L_COMMITS, 0 },
        /* the permits, the idle content, the shadow */
        { "park-release window latched mid-D",      N_RELWIN, 0u, 0u, 40, 5, A_D_COMMIT, GSB_L_PERMIT, 0 },
        { "non-P content while idle (A 500 in P)",  N_INJ, 0u, 500u, -10, 1, A_D_COMMIT, GSB_L_IDLE, 0 },
        { "candidate != shadow 2 passes (C fill)",  N_INJ, 4u, 5000u, 3, 4, A_D_COMMIT, GSB_L_SHADOW, 1 },
        /* r04q (D-G6-13): the boot allowance cannot be abused */
        { "boot: a second boot entry (allowance twice)", N_BOOT2, 0u, 0u, 30, 0, A_BOOT, GSB_L_IDLE, 0 },
        { "boot: D off the P content in the window",    N_INJ, 1u, 5000u, 3, 2, A_BOOT, GSB_L_IDLE, 0 },
        { "boot: the entry with E28E off its boot states", N_INJ, 0xFFu, 0u, 0, 1, A_BOOT, GSB_L_IDLE, 0, 1u, 5u, 0xFFu },
        { "boot: E28E off its states during the fill",  N_INJ, 0xFFu, 0u, 1, 1, A_BOOT, GSB_L_IDLE, 0, 1u, 5u, 0xFFu },
        { "boot: the all-release row after the fill began", N_INJ, 0u, 500u, 6, 1, A_BOOT, GSB_L_IDLE, 0, 1u, 2u, 1u },
        { "boot: the allowance bound lowered to 5",     N_BLEN, 0u, 0u, 0, 5, A_BOOT, GSB_L_IDLE, 4 },
        { "a non-boot all-release (A 500 after the window)", N_INJ, 0u, 500u, 20, 1, A_BOOT, GSB_L_IDLE, 0 },
    };
#if defined(G6B_BYPASS)
    /* the monitor bypassed: the bounds negative must NOT be caught (ctest WILL_FAIL) */
    const unsigned nN = 1u;
#else
    const unsigned nN = (unsigned)(sizeof N / sizeof N[0]);
#endif
#if !defined(G6B_BYPASS)
    {
        const unsigned n = (unsigned)(sizeof S / sizeof S[0]);
#  if defined(G6A_ENGLIM)
        const unsigned nrun = 1u;   /* the englim build: the gs-check only (the 3.5 s limit in both forms) */
#  else
        const unsigned nrun = n;
#  endif
        for (unsigned k = 0u; k < nrun; k++) {
            scenario(&S[k]);
        }
    }
#endif
#if !defined(G6A_ENGLIM)
    {
        static uint8_t win[WIN_N];
        static tick_t tks[GFRAMES];
        const scen_t base = { .name = "base gs-check (G6b, the negatives' reference)" };
        run_child(&base, true, &g_gbase, &g_base, win, tks);
        GCHECK(g6c_cmp(base.name, &g_gbase, &g_base, win, tks) == 0u, "base: differs from the r04o golden");
#if defined(G5B_SEL)
        g5b_check(base.name, true, 0u, 0u, 0u, 0u, &g_gbase, &g_base, win);
#endif
        printf("== base: D commit pass %u, R commit pass %u, D release start pass %u\n",
               (unsigned)g_base.b.slot[0].t0, (unsigned)g_base.b.slot[1].t0, (unsigned)g_base.rel_t0[0]);
        for (unsigned k = 0u; k < nN; k++) {
            negative(&N[k]);
        }
        printf("negatives as required: %u of %u (12 caught as r04o + the deleted shadow check not tripping + 7 r04q "
               "boot-allowance negatives caught)\n", g_neg_ok, nN);
#if !defined(G6B_BYPASS)
        supply_guard(6u, 8500u, 30);
        supply_guard(5u, 0u, 60);
        printf("r04r WK / Sys guard runs as required: %u of 2\n", g_supply_ok);
#endif
    }
#endif
#if defined(G5B_SEL)
    printf("G5b: %u runs checked, %u entered mode 2 as required\n", g5b_runs, g5b_m2_runs);
#endif
#if defined(G5C_SCEN)
    printf("G5c: %u of 6 G5c cases as the stock's rule\n", g5c_runs_ok);
#endif
    if (g_fails == 0) {
        printf("test_g6b: ok\n");
    }
    return g_fails != 0;
}
