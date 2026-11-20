/* test_sm_shadow.c — THE SHIFT-MAP SHADOW (app/sm_shadow.c; 2026-11-19,
 * ~/tcu_sweep/sm_shadow_port.md; decode ~/tcu_sweep/shift_map_decode.md).
 *
 *   (no argument)  (1) the dump cells: gear_in 3, speed 0, pedal 23 → the
 *                      walk's down cells, candidate 1, A697 / A0AA / 0x89CE8 = 1
 *                      (and, from the dump file when present, the same cells
 *                      read back); the up cells (D8 / D0 / D4 / CE) by the
 *                      walk at gear_in 1 — the dump's are the confirm stage's
 *                      evaluation of candidate 1, which runs the same
 *                      functions (0x128E94 / 0x128D9C) [O structure, stood in]
 *                  (2) standstill: every gear_in 1..8 at speed 0 → 1 (8 → 2
 *                      → 1: the down limit 6)
 *                  (3) the decode's §7 sweeps at pedal 23 / 51 / 89 — the walk
 *                      alone exactly; the full pass (the AB30 filter, the
 *                      confirm stand-in, 0xFC5DE) the same gear sequence, each
 *                      change at the decode's speed or (rising, AB30 settling
 *                      at v - 1) one 50-rpm step later
 *                  (4) the step limits (0xFC358): programs 1 / 2 / 15, the
 *                      column-47 cap
 *                  (5) invariants over random inputs
 *   --cal          every calibration copy against the stock image, and the
 *                  indirections (exit 77: the image absent)
 *   --time         host ns per pass (the CPU estimate)
 * Build test_sm_shadow_neg_MAP (SM_NEG_MAP: one map cell 125 → 126 in
 * app/sm_cal.c) must FAIL. */
#include "app/sm_shadow.h"
#include "app/sm_cal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int g_fail;
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); g_fail++; } } while (0)
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

#define S (sm_shadow_state())

static void pass(uint16_t speed, uint8_t pedal, int8_t gear)
{
    const sm_in_t in = { 0u, speed, pedal, gear, 1u, 1u, 1u, 0u };
    sm_shadow_pass(&in);
}

/* ── (1) the dump ─────────────────────────────────────────────────────── */
static void test_dump(void)
{
    sm_shadow_init();
    pass(0u, 23u, 3);
    CHECKV(S->s_c2, 3);
    CHECKV(S->s_e2, 1);
    CHECKV(S->s_da, 160);
    CHECKV(S->s_d2, 160);
    CHECKV(S->s_d6, 750);
    CHECKV(S->s_dc, 0);
    CHECKV(S->s_de, 0);
    CHECKV(S->s_b6, 0);
    CHECKV(S->s_af, 102);
    CHECKV(S->s_b0, 249);
    CHECKV(S->s_e6, 10000);
    CHECKV(S->s_e8, 0);
    CHECKV(S->s_ea, 10000);
    CHECKV(S->s_ec, 0);
    {
        static const uint8_t ba[6] = { 5, 6, 5, 6, 0, 0 };
        for (unsigned i = 0u; i < 6u; i++) {
            CHECKV(S->s_ba[i], ba[i]);
        }
    }
    CHECKV(S->a697, 1);
    CHECKV(S->a0aa, 1);
    CHECKV(S->ce8, 1);
    CHECKV(S->m17, 1);
    CHECKV(S->m18, 3);
    CHECKV(S->blk[0], 1);
    CHECKV(S->blk[1], 1);
    CHECKV(S->blk[2], 3);
    CHECKV(S->blk[6], 1);
    CHECKV(S->blk[7], 0x17);
    CHECKV(S->blk[8], 1);
    CHECKV(S->blk[9], 1);
    CHECKV(S->arb_skip_n, 4);           /* 0x1B, 0x0B, 0x12, 0x08 before program 1 */
    CHECKV(S->err_n, 0);
    CHECKV(S->corr_n, 0);
    /* the confirm stage (2026-11-19): 3 -> 1 down, masks 0 → E0 0xFFFF > AB30 0
     * accepted; its full-load lookup leaves S+0xE3 0xFF / S+0xE4 1 (gear 1's up
     * line), S+0xC0 0xFF, S+0xDF = S+0xC2 3, S+0xAD / S+0x84 0 */
    const unsigned dump_e3 = S->s_e3, dump_e4 = S->s_e4, dump_e0 = S->s_e0, dump_c0 = S->s_c0,
                   dump_df = S->s_df, dump_ad = S->s_ad, dump_84 = S->s_84;
    CHECKV(dump_e3, 0xFF);
    CHECKV(dump_e4, 1);
    CHECKV(dump_e0, 0xFFFF);
    CHECKV(dump_c0, 0xFF);
    CHECKV(dump_df, 3);
    CHECKV(dump_ad, 0);
    CHECKV(dump_84, 0);
    (void)dump_e3; (void)dump_e4; (void)dump_e0; (void)dump_c0; (void)dump_df; (void)dump_ad; (void)dump_84;
    const uint16_t da = S->s_da, d2 = S->s_d2, d6 = S->s_d6;
    /* the up cells: program 1's walk at gear_in 1 */
    uint8_t cand = 0u;
    CHECKV(sm_shadow_walk(1u, 0u, 23u, &cand), 0);
    CHECKV(cand, 1);
    CHECKV(S->s_d8, 240);
    CHECKV(S->s_d0, 240);
    CHECKV(S->s_d4, 1028);
    CHECKV(S->s_ce, 1040);
#if defined(TCU_STOCK_DUMP)
    {
        static unsigned char d[0x20000];
        FILE *f = fopen(TCU_STOCK_DUMP, "rb");
        if (f == NULL || fread(d, 1u, sizeof d, f) != sizeof d) {
            printf("(1) dump file absent — cell comparison skipped\n");
        } else {
#define DW(a) ((unsigned)((d[(a) - 0xFFF80000u] << 8) | d[(a) - 0xFFF80000u + 1u]))
#define DB(a) ((unsigned)d[(a) - 0xFFF80000u])
            const unsigned SB = 0xFFF8A4E4u;
            CHECKV(DW(SB + 0xDAu), da);
            CHECKV(DW(SB + 0xD2u), d2);
            CHECKV(DW(SB + 0xD6u), d6);
            CHECKV(DW(SB + 0xD8u), S->s_d8);
            CHECKV(DW(SB + 0xD0u), S->s_d0);
            CHECKV(DW(SB + 0xD4u), S->s_d4);
            CHECKV(DW(SB + 0xCEu), S->s_ce);
            CHECKV(DW(SB + 0xDCu), 0u);
            CHECKV(DB(SB + 0xC2u), 3u);
            CHECKV(DB(SB + 0xE2u), 1u);
            CHECKV(DB(SB + 0xACu), 1u);
            CHECKV(DB(SB + 0xAEu), SM_SI_AE);
            CHECKV(DB(SB + 0xAFu), SM_SI_AF);
            CHECKV(DB(SB + 0xB0u), SM_SI_B0);
            CHECKV(DW(SB + 0xB2u), (unsigned)SM_SI_B2);
            CHECKV(DB(SB + 0xB6u), 0u);
            CHECKV(DB(SB + 0xC3u), SM_SI_C3);
            CHECKV(DB(SB + 0xDEu), 0u);
            /* the confirm stage's cells (0x1284CC with the masks 0) */
            CHECKV(DB(SB + 0xE3u), dump_e3);
            CHECKV(DB(SB + 0xE4u), dump_e4);
            CHECKV(DW(SB + 0xE0u), dump_e0);
            CHECKV(DW(SB + 0xC0u), dump_c0);
            CHECKV(DB(SB + 0xDFu), dump_df);
            CHECKV(DB(SB + 0xADu), dump_ad);
            CHECKV(DB(SB + 0x84u), dump_84);
            CHECKV(DW(SB + 0x8Cu), 0u);                 /* the masks held 0 */
            CHECKV(DW(SB + 0x94u), 0u);
            for (unsigned i = 0u; i < 6u; i++) {
                CHECKV(DB(SB + 0xBAu + i), S->s_ba[i]);
            }
            CHECKV(DW(SB + 0xE6u), SM_SI_LIM_HI);
            CHECKV(DW(SB + 0xE8u), SM_SI_LIM_LO);
            CHECKV(DW(SB + 0xEAu), SM_SI_LIM_HI);
            CHECKV(DW(SB + 0xECu), SM_SI_LIM_LO);
            CHECKV(DB(0xFFF8A697u), 1u);
            CHECKV(DB(0xFFF8A0AAu), 1u);
            CHECKV(DB(0xFFF89CE8u), 1u);
            CHECKV(DB(0xFFF8A694u), 3u);
            CHECKV(DB(0xFFF8A67Cu + 0x17u), 1u);       /* min gear at 0 rpm */
            CHECKV(DB(0xFFF8A67Cu + 0x2Bu), SM_SI_WIN_HI);
            CHECKV(DB(0xFFF8A67Cu + 0x2Du), SM_SI_WIN_LO);
            CHECKV(DB(0xFFF8A67Cu + 0x24u), 0u);
            CHECKV(DB(0xFFF8A67Cu + 0x04u), 0u);
            CHECKV(DB(0xFFF8A67Cu + 0x07u), 0u);
            CHECKV(DW(0xFFF8AB30u), 0u);
            CHECKV(DB(0xFFF8A786u), SM_SI_PEDAL);
            CHECKV(DB(0xFFF8AB34u), SM_SI_AB34);
            CHECKV(DB(0xFFF85157u), (unsigned)SM_SI_S3F);
            CHECKV(DB(0xFFF89C28u), 3u);                 /* the dump's gear_in (the chain: test_gear (6))*/
            CHECKV(DB(0xFFF8A706u), SM_SI_A706);
            CHECKV(DB(0xFFF8A1F4u), SM_SI_BASE_B);
            CHECKV(DB(0xFFF8A1F5u), SM_SI_BASE_A);
            CHECKV(DB(0xFFF8A1F7u), SM_SI_BASE_W);
            CHECKV(DB(0xFFF8A2C4u), SM_SI_LVL_LO);
            CHECKV(DB(0xFFF8A2C5u), SM_SI_LVL_HI);
            CHECKV(DB(0xFFF8A2C6u), SM_SI_LVL_W);
            {
                static const unsigned bo[] = { 0, 1, 2, 6, 7, 8, 9 };
                for (unsigned i = 0u; i < sizeof bo / sizeof bo[0]; i++) {
                    CHECKV(DB(0xFFF8A5D4u + bo[i]), S->blk[bo[i]]);
                }
            }
            printf("(1) dump cells compared\n");
        }
        if (f) {
            fclose(f);
        }
    }
#endif
}

/* ── (2) standstill ───────────────────────────────────────────────────── */
static void test_standstill(void)
{
    for (int g = 1; g <= 8; g++) {
        sm_shadow_init();
        pass(0u, 23u, (int8_t)g);
        CHECKV(S->a697, g == 8 ? 2 : 1);
        pass(0u, 23u, (int8_t)S->a697);
        CHECKV(S->a697, 1);
        CHECKV(S->err_n, 0);
    }
}

/* ── (3) the sweeps ───────────────────────────────────────────────────── */
static const char *const k_rise[3] = {
    "3->1@0 1->2@250 2->3@550 3->4@700 4->5@950 5->6@1200 6->7@1500 7->8@1850",
    "3->1@0 1->2@300 2->3@550 3->4@750 4->5@1050 5->6@1400 6->7@1650 7->8@2000",
    "3->1@0 1->2@450 2->3@850 3->4@1250 4->5@1650 5->6@2100 6->7@2800 7->8@3550",
};
static const char *const k_fall[3] = {
    "8->7@1600 7->6@1300 6->5@1050 5->4@800 4->3@400 3->2@300 2->1@150",
    "8->7@1650 7->6@1350 6->5@1100 5->4@850 4->3@500 3->2@350 2->1@150",
    "8->7@1850 7->6@1550 6->5@1300 5->4@1050 4->3@750 3->2@450 2->1@200",
};
static const uint8_t k_ped[3] = { 23u, 51u, 89u };

static void app(char *b, size_t n, unsigned g0, unsigned g1, unsigned v)
{
    size_t l = strlen(b);
    snprintf(b + l, n - l, "%s%u->%u@%u", l ? " " : "", g0, g1, v);
}

/* the walk alone (no filter, no limits): the decode's sim */
static void sweep_walk(unsigned k, char *rise, char *fall, size_t n)
{
    uint8_t g = 3u, c;
    rise[0] = fall[0] = '\0';
    for (unsigned v = 0u; v <= 7000u; v += 50u) {
        CHECKV(sm_shadow_walk(g, (uint16_t)v, k_ped[k], &c), 0);
        if (c != g) {
            app(rise, n, g, c, v);
            g = c;
        }
    }
    for (int v = 7000; v >= 0; v -= 50) {
        CHECKV(sm_shadow_walk(g, (uint16_t)v, k_ped[k], &c), 0);
        if (c != g) {
            app(fall, n, g, c, (unsigned)v);
            g = c;
        }
    }
}

/* the full pass: each speed held 12 passes, gear_in = the last A697 */
static void sweep_pass(unsigned k, char *rise, char *fall, size_t n)
{
    sm_shadow_init();
    uint8_t g = 3u;
    rise[0] = fall[0] = '\0';
    for (unsigned v = 0u; v <= 7000u; v += 50u) {
        for (unsigned i = 0u; i < 12u; i++) {
            pass((uint16_t)v, k_ped[k], (int8_t)g);
            if (S->a697 != g) {
                CHECK(S->a697 <= g + 1u);
                app(rise, n, g, S->a697, v);
                g = S->a697;
            }
        }
    }
    for (int v = 7000; v >= 0; v -= 50) {
        for (unsigned i = 0u; i < 12u; i++) {
            pass((uint16_t)v, k_ped[k], (int8_t)g);
            if (S->a697 != g) {
                app(fall, n, g, S->a697, (unsigned)v);
                g = S->a697;
            }
        }
    }
}

/* the same transitions, each at the expected speed or (rising) 50 rpm later */
static int same_or_later(const char *exp, const char *got)
{
    char e[512], g[512];
    snprintf(e, sizeof e, "%s", exp);
    snprintf(g, sizeof g, "%s", got);
    char *se, *sg, *te = strtok_r(e, " ", &se), *tg = strtok_r(g, " ", &sg);
    while (te && tg) {
        unsigned a0, a1, av, b0, b1, bv;
        if (sscanf(te, "%u->%u@%u", &a0, &a1, &av) != 3 || sscanf(tg, "%u->%u@%u", &b0, &b1, &bv) != 3) {
            return 0;
        }
        if (a0 != b0 || a1 != b1 || (bv != av && bv != av + 50u)) {
            return 0;
        }
        te = strtok_r(NULL, " ", &se);
        tg = strtok_r(NULL, " ", &sg);
    }
    return te == NULL && tg == NULL;
}

static void test_sweeps(void)
{
    char r[512], f[512];
    for (unsigned k = 0u; k < 3u; k++) {
        sm_shadow_init();
        sweep_walk(k, r, f, sizeof r);
        if (strcmp(r, k_rise[k]) != 0 || strcmp(f, k_fall[k]) != 0) {
            printf("FAIL walk sweep pedal %u\n  rise %s\n  exp  %s\n  fall %s\n  exp  %s\n",
                   k_ped[k], r, k_rise[k], f, k_fall[k]);
            g_fail++;
        } else {
            printf("(3) walk  pedal %3u rise: %s\n                 fall: %s\n", k_ped[k], r, f);
        }
        sweep_pass(k, r, f, sizeof r);
        if (!same_or_later(k_rise[k], r) || strcmp(f, k_fall[k]) != 0) {
            printf("FAIL pass sweep pedal %u\n  rise %s\n  exp  %s\n  fall %s\n  exp  %s\n",
                   k_ped[k], r, k_rise[k], f, k_fall[k]);
            g_fail++;
        } else {
            printf("(3) pass  pedal %3u rise: %s\n                 fall: %s\n", k_ped[k], r, f);
        }
        CHECKV(S->err_n, 0);
    }
}

/* ── (4) the step limits ──────────────────────────────────────────────── */
static void test_steps(void)
{
    CHECKV(sm_shadow_step_limit(1u, 0u, 0u), 6);
    CHECKV(sm_shadow_step_limit(1u, 1u, 0u), 1);
    CHECKV(sm_shadow_step_limit(2u, 0u, 0u), 6);
    CHECKV(sm_shadow_step_limit(2u, 1u, 0u), 1);
    CHECKV(sm_shadow_step_limit(15u, 0u, 0u), 2);
    CHECKV(sm_shadow_step_limit(15u, 1u, 0u), 6);
    for (unsigned d = 0u; d < 2u; d++) {
        for (unsigned p = 1u; p <= 63u; p++) {
            const uint8_t a = sm_cal_step[(p - 1u) + d * 63u], c = sm_cal_step[47u + d * 63u];
            CHECKV(sm_shadow_step_limit((uint8_t)p, (uint8_t)d, 1u), a < c ? a : c);
        }
    }
    /* program 0 → column 255: 0x1214B4's range error → t[0] */
    CHECKV(sm_shadow_step_limit(0u, 1u, 0u), sm_cal_step[0]);
}

/* ── (5) invariants ───────────────────────────────────────────────────── */
static void test_invariants(void)
{
    srand(12345);
    sm_shadow_init();
    for (unsigned i = 0u; i < 50000u; i++) {
        const uint16_t v = (uint16_t)(rand() % 9000);
        const uint8_t ped = (uint8_t)(rand() % 256);
        const int8_t g = (int8_t)(1 + rand() % 8);
        pass(v, ped, g);
        const unsigned a = S->a697;
        CHECK(a >= 1u && a <= 8u);
        CHECK(a <= (unsigned)g + 1u);                       /* up ≤ 1 per pass (program 1) */
        if (a < (unsigned)g) {
            CHECK(a >= (S->m17 < (unsigned)g ? S->m17 : (unsigned)g));   /* the min-gear rule */
            CHECK((unsigned)g - a <= 6u);                   /* down ≤ 6 per pass */
        }
        if (S->s_e2 > (unsigned)g) {
            CHECK(S->s_e2 == (unsigned)g + 1u);             /* no up-after-down, one up */
        }
        if (g_fail > 20) {
            break;
        }
    }
    CHECKV(S->err_n, 0);
    printf("(5) invariants: %u passes, corr (unported, pedal >= 102) %u, min-gear %u, clamps %u\n",
           (unsigned)S->pass_n, (unsigned)S->corr_n, (unsigned)S->mingear_n, (unsigned)S->clamp_n);
}


/* ── (6) THE GEAR FEEDBACK CHAIN (app/sm_gear.c) ───────────────────────── */
#define G (sm_gear_state())
static unsigned g_tick;     /* 10 ms ticks; the shift-target pass on every 3rd */
typedef struct {
    uint8_t e69b, e6ee, e6ef, e28e, c3d, mode;
    uint16_t out;           /* output rpm */
    uint8_t pedal;
} gx_t;
static const uint16_t k_ratio[9] = { 0, 4714, 3143, 2106, 1667, 1285, 1000, 839, 667 };

/* one 10 ms tick: the gear pass (slots 30..33), then on every 3rd tick the
 * shift-target pass with gear_in = the shadow's 0x89C28 (main.c's order) */
static void gtick(const gx_t *x)
{
    const int8_t g = G->e69f;
    const uint16_t tur = (g >= 1 && g <= 8) ? (uint16_t)((uint32_t)x->out * k_ratio[g] / 1000u) : 0u;
    const gf_in_t in = { x->e69b, x->e6ee, x->e6ef, x->e28e, x->c3d, x->mode, 80u, 0, 0, 0u,
                         x->out, tur, tur };
    sm_gear_pass(&in);
    if (g_tick % 3u == 0u) {
        const sm_in_t si = { 0u, x->out, x->pedal, G->c28[0], x->mode, 1u, 1u, 0u };
        sm_shadow_pass(&si);
    }
    g_tick++;
}

static void ginit(void)
{
    sm_shadow_init();
    sm_gear_init();
    sm_gear_host_si(0u, 0);
    g_tick = 0u;
}

static void test_gear(void)
{
    /* (6a) boot in P (power flow 0, mode 1 → c3d 1): the stock init E4A7 =
     * E69F = CCE4[0] = 6; the first target pass from gear_in 6 at 0 rpm →
     * A697 1; the chain follows: E4A4 1 → 0xD1B10 → E6A1 1 → E4A7 1 → the
     * strobe → E69F 1 → 0x89C25 1 (same pass) */
    ginit();
    gx_t x = { 0u, 0u, 0u, 0u, 1u, 1u, 0u, 23u };
    CHECKV(G->e4a7, 6);
    CHECKV(G->c28[0], 6);
    gtick(&x);                              /* tick 0: the gear pass sees 0x89CE8 0 → stays 6; the target pass → 1 */
    CHECKV(G->e4a7, 6);
    CHECKV(S->m18, 6);
    CHECKV(S->a697, 1);
    gtick(&x);
    CHECKV(G->e4a4, 1);
    CHECKV(G->e6a1, 1);
    CHECKV(G->e4a7, 1);
    CHECKV(G->c28[0], 1);
    CHECKV(G->c28[1], 2);
    CHECKV(G->e69f, 1);
    CHECKV(G->c2b[0], 1);
    CHECKV(G->c2b[3], (int8_t)~1);
    CHECKV(G->cce4[0], 1);
    CHECKV(G->c25[0], 1);
    CHECKV(G->e69c[0], 0);
    CHECKV(G->strobe_n, 1);
    CHECKV(G->done_n, 1);
    for (unsigned i = 0u; i < 60u; i++) {
        gtick(&x);
    }
    CHECKV(G->e4a7, 1);
    CHECKV(S->m18, 1);
    CHECKV(S->a697, 1);
    CHECKV(G->e343, 1);                     /* the coordinator followed 0x89C2B */
    CHECKV(G->x519f, 1);
    /* (6b) lever D, mode 2 (c3d 0): the commit (E69B 1) pending ~95 passes
     * (E6EE 0), then complete — standstill D: current gear 1, target 1 */
    x.c3d = 0u; x.mode = 2u; x.e6ef = 1u; x.e69b = 1u; x.e6ee = 0u;
    for (unsigned i = 0u; i < 95u; i++) {
        gtick(&x);
        CHECKV(G->e4a7, 1);
    }
    x.e6ee = 1u;
    for (unsigned i = 0u; i < 60u; i++) {
        gtick(&x);
    }
    CHECKV(G->e4a7, 1);
    CHECKV(G->e69f, 1);
    CHECKV(G->c28[0], 1);
    CHECKV(S->a697, 1);
    CHECKV(G->d1990_n > 0u, 1);
    CHECKV(G->strobe_n, 1);
    /* (6c) the mode-1 return of 0xD15A6: c3d 1 with E69B 1 → E6A1 = E69F */
    x.c3d = 1u;
    gtick(&x);
    CHECKV(G->e6a1, 1);
    CHECKV(G->ret1_n, 1);
    x.c3d = 0u;
    /* (6d) R: E6EF 2, commit E69B 2, pending: E6A1 −1 at once, E4A7 held at
     * E69F (0x104A28's pending path) until the completion, then −1 → the
     * strobe (a reversal: the checks skipped) → E69F −1, 0x89C28 0xFF →
     * the target pass's A694 "negative → 9" → 0xFC5DE(A697) = 1 */
    x.e6ef = 2u; x.e69b = 2u; x.e6ee = 1u;
    for (unsigned i = 0u; i < 95u; i++) {
        gtick(&x);
        CHECKV(G->e6a1, -1);
        CHECKV(G->e4a7, 1);
    }
    x.e6ee = 2u;
    gtick(&x);
    CHECKV(G->e4a7, -1);
    CHECKV(G->e69f, -1);
    CHECKV((uint8_t)G->c28[0], 0xFF);
    CHECKV(G->c28[1], 0);
    CHECKV(G->c28[2], 0);
    CHECKV(G->strobe_n, 2);
    const unsigned n9 = S->neg9_n;
    for (unsigned i = 0u; i < 30u; i++) {
        gtick(&x);
    }
    CHECKV(S->m1a, 1);
    CHECKV(S->neg9_n, n9 + 10u);
    CHECKV(S->m18, 1);
    CHECKV(S->a697, 1);
    CHECKV(S->err_n, 0);
    /* (6e) back to P: the release pending (E69B 0, E6EE 2): E4A7 held −1,
     * then the completion → 0xD1B10(1) → 1 → the strobe → E69F 1 */
    x.e6ef = 0u; x.e69b = 0u; x.e6ee = 2u;
    for (unsigned i = 0u; i < 120u; i++) {
        gtick(&x);
        CHECKV(G->e4a7, -1);
    }
    x.e6ee = 0u;
    gtick(&x);
    CHECKV(G->e4a7, 1);
    CHECKV(G->e69f, 1);
    CHECKV(G->c28[0], 1);
    CHECKV(G->strobe_n, 3);
    CHECKV(G->err_n, 0);
    CHECKV(G->unp_n, 0);
    printf("(6) gear chain: boot P 6 -> 1, D 1, R -1 (neg9 %u), P 1; strobes %u done %u passes %u\n",
           (unsigned)S->neg9_n, (unsigned)G->strobe_n, (unsigned)G->done_n, (unsigned)G->pass_n);
}

/* (6f) the drive ramp: D committed, the output speed up 0 → 3000 rpm and back
 * (10 rpm per 10 ms tick), pedal 23. The target walks up through the up lines
 * one gear per 30 ms pass at most; the commanded gear 0x89C28 = E4A7 follows
 * it in the next 10 ms gear pass, and E69F the same pass */
static void test_ramp(void)
{
    ginit();
    gx_t x = { 1u, 1u, 1u, 0u, 0u, 2u, 0u, 23u };
    for (unsigned i = 0u; i < 30u; i++) {
        gtick(&x);                          /* settle to 1 */
    }
    CHECKV(G->e4a7, 1);
    char up[256] = "", dn[256] = "";
    int8_t last = G->e4a7;
    uint8_t lasta = S->a697;
    unsigned lag_max = 0u, chg_at = 0u, a_chg = 0u;
    int mono_up = 1, mono_dn = 1;
    for (int dir = 0; dir < 2; dir++) {
        for (unsigned k = 0u; k <= 300u; k++) {
            x.out = (uint16_t)(dir == 0 ? k * 10u : 3000u - k * 10u);
            gtick(&x);
            if (S->a697 != lasta) {
                if (dir == 0 && S->a697 != lasta + 1u) {
                    mono_up = 0;
                }
                lasta = S->a697;
                a_chg = g_tick;
            }
            if (G->e4a7 != last) {
                if (dir == 0 && G->e4a7 != last + 1) {
                    mono_up = 0;
                }
                if (dir == 1 && G->e4a7 >= last) {
                    mono_dn = 0;
                }
                app(dir == 0 ? up : dn, sizeof up, (unsigned)last, (unsigned)G->e4a7, x.out);
                last = G->e4a7;
                chg_at = g_tick;
                if (chg_at - a_chg > lag_max) {
                    lag_max = chg_at - a_chg;
                }
            }
            if (g_tick % 3u == 2u) {
                /* the gear pass of the tick after a target pass: commanded = target */
                CHECKV(G->e4a7, (int8_t)S->a697);
                CHECKV(G->e69f, G->e4a7);
                CHECKV(G->c25[0], G->e4a7);
            }
        }
    }
    printf("(6f) ramp up:   %s\n     ramp down: %s\n     commanded-after-target lag max %u tick(s)\n",
           up, dn, lag_max);
    /* each change within [line, line + 70] rising / [line - 50, line] falling
     * (the decode's §7 pedal-23 lines; the AB30 filter (v + prev) / 2 lags the
     * 10 rpm-per-tick ramp, sampled every 30 ms) */
    {
        static const unsigned lu[7] = { 250, 550, 700, 950, 1200, 1500, 1850 };
        static const unsigned ld[7] = { 1600, 1300, 1050, 800, 400, 300, 150 };
        unsigned n = 0u, a0, a1, v;
        const char *q = up;
        int adv;
        while (sscanf(q, "%u->%u@%u%n", &a0, &a1, &v, &adv) == 3 && n < 7u) {
            CHECKV(a0, n + 1u);
            CHECKV(a1, n + 2u);
            CHECK(v >= lu[n] && v <= lu[n] + 70u);
            q += adv;
            n++;
        }
        CHECKV(n, 7);
        n = 0u;
        q = dn;
        while (sscanf(q, "%u->%u@%u%n", &a0, &a1, &v, &adv) == 3 && n < 7u) {
            CHECKV(a0, 8u - n);
            CHECKV(a1, 7u - n);
            CHECK(v + 50u >= ld[n] && v <= ld[n]);
            q += adv;
            n++;
        }
        CHECKV(n, 7);
    }
    CHECK(mono_up == 1);
    CHECK(mono_dn == 1);
    CHECKV(lag_max <= 1u, 1);
    CHECKV(G->e4a7, 1);
    CHECKV(G->err_n, 0);
    CHECKV(S->err_n, 0);
    CHECKV(G->strobe_n >= 14u, 1);
    CHECKV(G->strobe_n, G->done_n);
}

/* (6g) the dump: the fault-reaction inputs of THIS bench (AE2D 1, 0x89C43 3:
 * code 0x20) in P, mode 1 → E4A4 / E6A1 / E4A7 / E69F / CCE4 / 0x89C25..2E /
 * E33C+7 / 0x8519F as the driving dump */
static void test_gear_dump(void)
{
    ginit();
    sm_gear_host_si(1u, 3);
    gx_t x = { 0u, 0u, 0u, 0u, 1u, 1u, 0u, 23u };
    for (unsigned i = 0u; i < 30u; i++) {
        gtick(&x);
    }
    CHECKV(G->e4a4, 3);
    CHECKV(G->e6a1, 3);
    CHECKV(G->e4a7, 3);
    CHECKV(G->e69f, 3);
    CHECKV(G->cce4[0], 3);
    CHECKV(G->e343, 3);
#if defined(TCU_STOCK_DUMP)
    {
        static unsigned char d[0x20000];
        FILE *f = fopen(TCU_STOCK_DUMP, "rb");
        if (f == NULL || fread(d, 1u, sizeof d, f) != sizeof d) {
            printf("(6g) dump file absent — chain cells skipped\n");
        } else {
            const volatile gf_t *g = G;
            const struct { unsigned a; int v; const char *n; } k[] = {
                { 0xFFF8E4A4u, g->e4a4, "E4A4" }, { 0xFFF8E4A5u, g->e4a5, "E4A5" },
                { 0xFFF8E4A6u, g->e4a6, "E4A6" }, { 0xFFF8E4A7u, g->e4a7, "E4A7" },
                { 0xFFF8E6A1u, g->e6a1, "E6A1" },
                { 0xFFF8E69Cu, g->e69c[0], "E69C" }, { 0xFFF8E69Du, g->e69c[1], "E69D" },
                { 0xFFF8E69Eu, g->e69c[2], "E69E" }, { 0xFFF8E69Fu, g->e69f, "E69F" },
                { 0xFFF8E6A0u, (int8_t)g->e6a0, "E6A0" },
                { 0xFFF8CCE4u, g->cce4[0], "CCE4" }, { 0xFFF8CCE5u, g->cce4[1], "CCE5" },
                { 0xFFF8CCE6u, g->cce4[2], "CCE6" }, { 0xFFF8CCE7u, g->cce4[3], "CCE7" },
                { 0xFFF89C25u, g->c25[0], "89C25" }, { 0xFFF89C26u, g->c25[1], "89C26" },
                { 0xFFF89C27u, g->c25[2], "89C27" }, { 0xFFF89C28u, g->c28[0], "89C28" },
                { 0xFFF89C29u, g->c28[1], "89C29" }, { 0xFFF89C2Au, g->c28[2], "89C2A" },
                { 0xFFF89C2Bu, g->c2b[0], "89C2B" }, { 0xFFF89C2Cu, g->c2b[1], "89C2C" },
                { 0xFFF89C2Du, g->c2b[2], "89C2D" }, { 0xFFF89C2Eu, g->c2b[3], "89C2E" },
                { 0xFFF8E343u, g->e343, "E33C+7" }, { 0xFFF8E344u, g->e344, "E33C+8" },
                { 0xFFF8519Fu, g->x519f, "8519F" }, { 0xFFF851A0u, g->x51a0, "851A0" },
                { 0xFFF8AE2Eu, g->ae2e, "AE2E" }, { 0xFFF8AE2Fu, g->ae2f, "AE2F" },
            };
            for (unsigned i = 0u; i < sizeof k / sizeof k[0]; i++) {
                const int dv = (int8_t)d[k[i].a - 0xFFF80000u];
                if (dv != (int8_t)k[i].v) {
                    printf("FAIL (6g) %s dump %d shadow %d\n", k[i].n, dv, (int)(int8_t)k[i].v);
                    g_fail++;
                }
            }
            printf("(6g) the chain's %u dump cells compared\n", (unsigned)(sizeof k / sizeof k[0]));
        }
        if (f) {
            fclose(f);
        }
    }
#endif
    sm_gear_host_si(0u, 0);
}

/* ── --cal ───────────────────────────────────────────────────────────── */
static unsigned char g_img[0x200000];
static unsigned U32(unsigned a) { return ((unsigned)g_img[a] << 24) | ((unsigned)g_img[a + 1] << 16) | ((unsigned)g_img[a + 2] << 8) | g_img[a + 3]; }
static unsigned U16(unsigned a) { return ((unsigned)g_img[a] << 8) | g_img[a + 1]; }

static int test_cal(void)
{
#if !defined(TCU_STOCK_IMAGE)
    return 77;
#else
    FILE *f = fopen(TCU_STOCK_IMAGE, "rb");
    if (f == NULL || fread(g_img, 1u, sizeof g_img, f) != sizeof g_img) {
        printf("SKIP no stock image %s\n", TCU_STOCK_IMAGE);
        return 77;
    }
    fclose(f);
    /* the indirections (the initialised-data pointers, the dump's values) */
    CHECKV(U32(0x62278u + 0x1A4u), 0x1F3B7Cu);
    CHECKV(U32(0x62424u), 0x1F3BE4u);          /* *0xFFF84EB4 = 0x62424 */
    CHECKV(U32(0x61D34u + 0x0Cu), 0x1E5E44u);  /* *0xFFF84E60 = 0x61D34 */
    CHECKV(U32(0x61D34u), 0x1E5E30u);
    CHECKV(U32(0x61A80u + 0x0Cu), 0x1FEC8Cu);  /* *0xFFF84E44 = 0x61A80 */
    CHECKV(U32(0x61A80u + 0x10u), 0x1FEC8Du);
    /* 0x1290A8's cases for the ids the remap yields: id 5k loads @(4k) of
     * 0x62278 (mov.l @r2,r4 = 0x6422; mov.l @(4k,r2),r4 = 0x5420 | k) */
    for (unsigned k = 0u; k < 5u; k++) {
        const unsigned id = 5u * k;
        const int16_t off = (int16_t)U16(0x1290D0u + 2u * id);
        const unsigned body = (unsigned)(0x1290CCu + (unsigned)(int)off);
        const unsigned want = (k == 0u) ? 0x6422u : (0x5420u | k);
        int seen = 0;
        for (unsigned a = body; a < body + 14u; a += 2u) {
            if (U16(a) == want) {
                seen = 1;
            }
        }
        CHECK(seen);
        CHECKV(sm_cal_map[k].addr, U32(0x62278u + 4u * k));
    }
    /* the remap yields only 0/5/10/15/20 for ids < 25 */
    for (unsigned k = 0u; k < 25u; k++) {
        CHECK(sm_cal_remap[k] % 5u == 0u && sm_cal_remap[k] <= 20u);
    }
    for (unsigned k = 0u; k < SM_CAL_NMAP; k++) {
        const sm_map_t *m = &sm_cal_map[k];
        const unsigned p = m->addr;
        CHECKV(m->nx, U32(p));
        CHECKV(m->ny, U32(p + 4u));
        for (unsigned i = 0u; i < SM_CAL_NX; i++) {
            CHECKV(m->xa[i], U16(p + 8u + 2u * i));
        }
        for (unsigned i = 0u; i < SM_CAL_NY; i++) {
            CHECKV(m->ya[i], U16(p + 8u + 2u * SM_CAL_NX + 2u * i));
        }
        for (unsigned i = 0u; i < SM_CAL_NX * SM_CAL_NY; i++) {
            CHECKV(m->z[i], U16(p + 8u + 2u * (SM_CAL_NX + SM_CAL_NY) + 2u * i));
        }
    }
    CHECKV(U32(0x1F3B7Cu), 100u);
    for (unsigned i = 0u; i < 100u; i++) {
        CHECKV(sm_cal_remap[i], g_img[0x1F3B80u + i]);
    }
    CHECKV(U32(0x1F3BE4u), 18u);
    CHECKV(U32(0x1F3BE8u), 14u);
    for (unsigned i = 0u; i < 18u * 14u; i++) {
        CHECKV(sm_cal_prio[i], g_img[0x1F3BECu + i]);
    }
    CHECKV(U32(0x1E5E44u), 63u);
    CHECKV(U32(0x1E5E48u), 2u);
    for (unsigned i = 0u; i < 126u; i++) {
        CHECKV(sm_cal_step[i], g_img[0x1E5E4Cu + i]);
    }
    CHECKV(U32(0x1E5E30u), 8u);
    for (unsigned i = 0u; i < 8u; i++) {
        CHECKV(sm_cal_mingear[i], U16(0x1E5E34u + 2u * i));
    }
    CHECKV(sm_cal_ab30w[0], g_img[0x1FEC8Cu]);
    CHECKV(sm_cal_ab30w[1], g_img[0x1FEC8Du]);
    /* column 1 of the priority table: 0x1B, 0x0B, 0x12, 0x08, 0x01 */
    CHECKV(sm_cal_prio[18], 0x1B);
    CHECKV(sm_cal_prio[22], 0x01);
    printf("(cal) %d mismatches\n", g_fail);
    return g_fail ? 1 : 0;
#endif
}

/* ── --time ──────────────────────────────────────────────────────────── */
static double ns_per(uint16_t v, uint8_t ped, int8_t g, unsigned n)
{
    struct timespec a, b;
    sm_shadow_init();
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (unsigned i = 0u; i < n; i++) {
        pass(v, ped, g);
    }
    clock_gettime(CLOCK_MONOTONIC, &b);
    return ((double)(b.tv_sec - a.tv_sec) * 1e9 + (double)(b.tv_nsec - a.tv_nsec)) / n;
}

static int test_time(void)
{
    const unsigned n = 200000u;
    const double t_bench = ns_per(0u, 23u, 3, n), t_hold = ns_per(1200u, 51u, 5, n),
                 t_worst = ns_per(0u, 23u, 8, n);
    sm_shadow_init();
    pass(0u, 23u, 3);
    const unsigned lk = (unsigned)S->lookup_n;
    printf("host ns/pass: bench (gear 3, 0 rpm) %.0f [%u lookups], cruising hold %.0f, from 8th at 0 rpm %.0f\n",
           t_bench, lk, t_hold, t_worst);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--cal") == 0) {
        return test_cal();
    }
    if (argc > 1 && strcmp(argv[1], "--time") == 0) {
        return test_time();
    }
    test_dump();
    test_standstill();
    test_sweeps();
    test_steps();
    test_invariants();
    test_gear();
    test_ramp();
    test_gear_dump();
    printf("%s: %d failures\n", g_fail ? "FAIL" : "PASS", g_fail);
    return g_fail ? 1 : 0;
}
