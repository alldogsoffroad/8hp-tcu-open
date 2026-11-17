/* Unit test: M5-1, the stock element fill (app/rq_fill.c) behind the
 * request producer (app/rq_prod.c) and the request law (drv_reqlaw.c),
 * built as the target runs it (DRV_RQ_USE_PRODUCER) — for the stage the
 * build selects (RQ_M5_STAGE: default 1a, test_m5fill_1b = 1b).
 * ~/tcu_sweep/m5_1_fill_port.md.
 *
 *   1. the pass-by-pass trace from boot (the boot pass at boot state 1,
 *      then the 10 ms task in mode 1): the applied element's FSM, +0x46,
 *      its current and the bus-A words; printed for the report;
 *      1a: B fills (boost 8500, fill, blend), then 8500 — every pass after
 *          the fill byte-identical to the r03f / r03n constant wire, and
 *          during the fill only B's setpoint word (cyclic word 11) differs;
 *      1b: A fills, B is released (500) from the first pass; the steady
 *          state is the decode's block-0 gear-1 content;
 *   2. the widened guard on both sides of each bound (fill states 1/2 only,
 *      [500, 8500], channels 0..4 only, complement); m5fill_mut builds this
 *      file with the lower bound mutated to 499 and must FAIL;
 *   3. no fault / unported counter moves in the traces; nothing exceeds 8500;
 *   4. (--stock) the cal copies against the image byte for byte; the fill
 *      cells against the driving dump (A +0x36 = 0x6B, B +0x36 = 0x9F, ...).
 */
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_rqprod.h"
#include "bsw/sup/sup_cells.h"
#include "app/rq_prod.h"
#include "app/rq_fill.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

#define U8(a)   (*(volatile uint8_t  *)(uintptr_t)(a))
#define U16(a)  (*(volatile uint16_t *)(uintptr_t)(a))

static const uint16_t PSET[7] = { 500u, 8500u, 8500u, 8500u, 8500u, 500u, 500u };

#if RQ_M5_STAGE == RQ_M5_STAGE_1B
#define STAGE_NAME  "M5-1b"
#define FILL_OBJ    RQ_OBJ_A
#define FILL_CH     0u          /* A */
#define FILL_WORD   10u         /* cyclic E8 row, chip 0 */
static const uint16_t STEADY[7] = { 8500u, 8500u, 500u, 8500u, 8500u, 500u, 500u };
#else
#define STAGE_NAME  "M5-1a"
#define FILL_OBJ    RQ_OBJ_B
#define FILL_CH     2u          /* B */
#define FILL_WORD   11u         /* cyclic E8 row, chip 1 */
static const uint16_t *const STEADY = PSET;
#endif

static void reset(void)
{
    drv_jobs_init();                    /* → drv_rq_init → rq_prod_init */
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    CELL_LAD_94114 = 0u;
    CELL_DIAG_94115 = 0u;
    CELL_J2_STAGED = 0x9020u;
    CELL_LAD_REQ[0] = 0u;
    CELL_LAD_REQ[1] = 0u;
    CELL_OUT_MV[0] = 12940u;
}

#define NP 40u
typedef struct {
    uint8_t st; uint16_t frac; uint16_t p2; uint8_t mode;
    uint8_t b_st; uint16_t b_frac; uint16_t rec_b;
    uint16_t cur[7]; uint16_t li[7]; uint16_t w[85];
} tr_t;

/* prod: the producer at the stock cadence; !prod: the r03f constants */
static void run(bool prod, tr_t out[NP])
{
    reset();
    drv_rq_prod_host_select(prod);
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        drv_rq_request_set(ch, (!prod && ch < 7u) ? PSET[ch] : 0u);
    }
    for (unsigned k = 0u; k < NP; k++) {
        if (prod) {
            if (k == 0u) {
                rq_prod_boot_pass();            /* boot state 1 */
            } else {
                rq_prod_task_10ms(1u);          /* the 10 ms task, mode 1 */
            }
        }
        drv_cyclic_build(0, 8u);
        memcpy(out[k].w, drv_job_tx(DRV_CYC), 45u * sizeof(uint16_t));
        memcpy(&out[k].w[45], drv_job_tx(DRV_CFG), 40u * sizeof(uint16_t));
        out[k].st = U8(FILL_OBJ + RO_ST);
        out[k].frac = U16(FILL_OBJ + RO_FRAC);
        out[k].p2 = U16(FILL_OBJ + RO_RES);
        out[k].mode = U8(FILL_OBJ + RO_MODE);
        out[k].b_st = U8(RQ_OBJ_B + RO_ST);
        out[k].b_frac = U16(RQ_OBJ_B + RO_FRAC);
        out[k].rec_b = rqf_rec_frac(1u);
        for (uint8_t ch = 0u; ch < 7u; ch++) {
            out[k].cur[ch] = U16(0xFFF84B78u + 2u * ch);
            out[k].li[ch] = CELL_RQ_I[ch];
        }
    }
}

static void print_trace(const tr_t *t, const char *name)
{
    printf("%s trace (%s): pass | FSM +0x46 +2 mode | 0xFFF84B78 A D B E C WK Sys | word%u | B FSM +0x46 rec[B]\n",
           STAGE_NAME, name, FILL_WORD);
    for (unsigned k = 0u; k < NP; k++) {
        printf("  %2u | %u %5u %5u %u | %4u %4u %4u %4u %4u %4u %4u | %04X | %u %5u %04X\n", k,
               t[k].st, t[k].frac, t[k].p2, t[k].mode,
               t[k].cur[0], t[k].cur[1], t[k].cur[2], t[k].cur[3], t[k].cur[4],
               t[k].cur[5], t[k].cur[6], t[k].w[FILL_WORD], t[k].b_st, t[k].b_frac, t[k].rec_b);
    }
}

static int stock_checks(void);

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--stock") == 0) {
        return stock_checks();
    }

    /* ── 1. the trace ─────────────────────────────────────────────────── */
    {
        static tr_t a[NP], c[NP];
        unsigned fill_end = NP;
#if RQ_M5_STAGE != RQ_M5_STAGE_1B
        unsigned diff_words = 0u, diff_other = 0u;
#endif
        static rq_rec_t rec[RQ_REC_N];
        uint32_t rec_n, pass_n;
        run(true, a);
        rec_n = rq_prod_rec_n();
        pass_n = rq_prod_pass_n();
        for (uint32_t k = 0u; k < RQ_REC_N; k++) {
            memcpy(&rec[k], (const void *)rq_prod_rec(k), sizeof rec[k]);
        }
        run(false, c);
        print_trace(a, "producer");
#if RQ_M5_STAGE == RQ_M5_STAGE_1B
        /* the boot pass: A's boost, mode 0, 20000 → curve → 8500 */
        CHECK(a[0].st == RQF_ST_RAMP && a[0].frac == 0u && a[0].mode == 0u && a[0].p2 == 20000u);
#else
        /* the boot pass: record 0x1FD ready, B's stored 0x2710 → 0xE497A →
         * state 3 in the same pass; mode 0 (0xE40DA ran before 0xE3836) with
         * +2 = 20000 (0xE4374) → curve → 8500; mode 1 from pass 1 */
        CHECK(a[0].st == RQF_ST_APPLIED && a[0].frac == 10000u && a[0].mode == 0u && a[0].p2 == 20000u);
#endif
        CHECK(a[0].cur[FILL_CH] == 8500u);
        for (unsigned k = 0u; k < NP; k++) {
            if (a[k].st == RQF_ST_APPLIED && fill_end == NP) {
                fill_end = k;
            }
        }
#if RQ_M5_STAGE == RQ_M5_STAGE_1B
        CHECK(fill_end == 11u);                 /* fill time 107 ms: +0x2C 110 ≥ 107 */
#else
        CHECK(fill_end == 0u);                  /* no fill: B restored at the boot pass */
#endif
        for (unsigned k = 0u; k < NP; k++) {
            /* nothing above i_max, nothing below i_min, every pass */
            for (uint8_t ch = 0u; ch < 7u; ch++) {
                CHECK(a[k].cur[ch] >= 500u && a[k].cur[ch] <= 8500u);
                CHECK(a[k].li[ch] == a[k].cur[ch]);     /* the guard passed every value */
            }
            if (k >= 1u && k < fill_end) {
                CHECK(a[k].st == RQF_ST_RAMP && a[k].mode == 0u);
                CHECK(a[k].cur[FILL_CH] > 500u && a[k].cur[FILL_CH] < 8500u);
            }
            if (k >= fill_end) {
                CHECK(a[k].st == RQF_ST_APPLIED && a[k].frac == 10000u && (k == 0u || a[k].mode == 1u));
                for (uint8_t ch = 0u; ch < 7u; ch++) {
                    CHECK(a[k].cur[ch] == STEADY[ch]);
                }
            }
            for (uint8_t ch = 0u; ch < 7u; ch++) {
                if (ch != FILL_CH) {
                    CHECK(a[k].cur[ch] == STEADY[ch]);
                }
            }
#if RQ_M5_STAGE != RQ_M5_STAGE_1B
            /* 1a: the wire vs the r03f constants */
            for (unsigned w = 0u; w < 85u; w++) {
                if (a[k].w[w] != c[k].w[w]) {
                    if (w == FILL_WORD && k < fill_end) {
                        diff_words++;
                    } else {
                        diff_other++;
                        printf("FAIL pass %u word %u: %04X vs r03f %04X\n", k, w, a[k].w[w], c[k].w[w]);
                    }
                }
            }
#endif
        }
#if RQ_M5_STAGE != RQ_M5_STAGE_1B
        CHECK(diff_other == 0u);
        printf("M5-1a: %u passes with B's word differing, %u other differences\n",
               diff_words, diff_other);
        CHECK(diff_words == 0u);                /* wire-identical to r03n from pass 0 */
        CHECK(a[NP - 1u].rec_b == 0x2710u);     /* the write-back keeps B = 0x2710 */
        CHECK(a[NP - 1u].w[0] == 0x881Au && a[NP - 1u].w[1] == 0x881Au &&
              a[NP - 1u].w[2] == 0x881Au && a[NP - 1u].w[3] == 0x8812u && a[NP - 1u].w[4] == 0x8815u);
#else
        /* 1b steady state: E8 row ED88/89 E853/54 ED88/89 E853/54 E800,
         * mode words unchanged */
        CHECK(a[NP - 1u].w[10] == 0xED88u || a[NP - 1u].w[10] == 0xED89u);
        CHECK(a[NP - 1u].w[11] == 0xE853u || a[NP - 1u].w[11] == 0xE854u);
        CHECK(a[NP - 1u].w[12] == 0xED88u || a[NP - 1u].w[12] == 0xED89u);
        CHECK(a[NP - 1u].w[13] == 0xE853u || a[NP - 1u].w[13] == 0xE854u);
        CHECK(a[NP - 1u].w[14] == 0xE800u);
        CHECK(a[NP - 1u].w[0] == 0x881Au && a[NP - 1u].w[1] == 0x881Au &&
              a[NP - 1u].w[2] == 0x881Au && a[NP - 1u].w[3] == 0x8812u && a[NP - 1u].w[4] == 0x8815u);
#endif
        run(true, a);                           /* the producer run again for its counters */
#if RQ_M5_STAGE == RQ_M5_STAGE_1B
        /* B: never requested, but record 0x1FD restores its 0x2710 at the boot
         * pass (0xE497A → state 3), then 3 → 4 → 5 (release ramp, current 500
         * throughout, mode 1) → 0; the write-back 0xBCC00 follows +0x46 to 0 */
        for (unsigned k = 0u; k < 200u; k++) {
            const uint8_t st0 = U8(RQ_OBJ_B + RO_ST);
            rq_prod_task_10ms(1u);
            CHECK(rq_prod_ch(2u) == 500u);
            if (st0 != RQF_ST_OFF && U8(RQ_OBJ_B + RO_ST) == RQF_ST_OFF) {
                printf("M5-1b: B's release ramp ends (state 0) at pass %u, +0x3E %u, +0x48 %u\n",
                       NP + k, U16(RQ_OBJ_B + RO_TREL), U16(RQ_OBJ_B + RO_FRAC0));
            }
        }
        CHECK(rqf_rec_frac(1u) == 0u && U16(RQ_OBJ_B + RO_FRAC) == 0u);
        if (U8(RQ_OBJ_B + RO_ST) != RQF_ST_OFF) {
            printf("B: st %u act %u p %u mode %u\n", U8(RQ_OBJ_B + RO_ST), U8(RQ_OBJ_B + RO_ACTIVE),
                   U16(RQ_OBJ_B + RO_P), U8(RQ_OBJ_B + RO_MODE));
        }
        CHECK(U8(RQ_OBJ_B + RO_ST) == RQF_ST_OFF);
#endif
        CHECK(rq_prod_unp_n() == 0u && rq_prod_f19f_n() == 0u && rq_prod_f19a_n() == 0u &&
              rq_prod_f1c4_n() == 0u && drv_rq_m5_guard_n() == 0u);
        /* the recorder holds the transient: 40 passes, pass numbers 0.. */
        CHECK(rec_n == NP && pass_n == NP);
        for (unsigned k = 0u; k < NP; k++) {
            const rq_rec_t *r = &rec[k];
            CHECK(r->pass == k && r->cur[FILL_CH] == a[k].cur[FILL_CH] &&
                  r->st[(FILL_OBJ - RQ_OBJ_A) / 0x60u] == a[k].st &&
                  r->frac[(FILL_OBJ - RQ_OBJ_A) / 0x60u] == a[k].frac);
        }
        /* the fill bits: set during the fill, cleared once applied (0xE5030) */
        CHECK(RQF_CELL_851A2 == 0u);
#if RQ_M5_STAGE == RQ_M5_STAGE_1B
        CHECK(RQF_CELL_E384 == U8(FILL_OBJ + RO_ELEM));
#else
        CHECK(RQF_CELL_E384 == 0u);             /* 0xE4214 never ran: no fill */
#endif
    }

    /* ── before the boot pass and outside mode 1..3: no producer pass ──── */
    reset();
    drv_rq_prod_host_select(true);
    rq_prod_task_10ms(1u);                      /* not started: nothing */
    CHECK(!rq_prod_started() && rq_prod_pass_n() == 0u);
    drv_rq_law();
    for (uint8_t ch = 0u; ch < 7u; ch++) {
        CHECK(CELL_RQ_I[ch] == PSET[ch]);       /* r03n's values, uncounted */
    }
    CHECK(drv_rq_m5_guard_n() == 0u);
    rq_prod_boot_pass();
    rq_prod_task_10ms(0u);                      /* mode 0: no list holds it */
    rq_prod_task_10ms(4u);
    CHECK(rq_prod_started() && rq_prod_pass_n() == 1u);

    /* ── 2. the guard, both sides of each bound ───────────────────────── */
    {
        struct { uint8_t ch; uint8_t st; uint16_t v; bool pass; } G[] = {
            { 2u, 2u,  499u, false }, { 2u, 2u,  500u, true  }, { 2u, 2u, 5704u, true  },
            { 2u, 2u, 8500u, true  }, { 2u, 2u, 8501u, false }, { 2u, 1u, 5000u, true  },
            { 2u, 3u, 5000u, false }, { 2u, 0u, 5000u, false }, { 2u, 4u, 5000u, false },
            { 2u, 5u, 5000u, false }, { 2u, 6u, 5000u, false }, { 0u, 2u, 5293u, true  },
            { 4u, 2u, 4720u, true  }, { 5u, 2u, 5000u, false }, { 6u, 2u, 5000u, false },
            { 0u, 2u,  499u, false }, { 0u, 1u, 8501u, false },
        };
        for (size_t k = 0u; k < sizeof G / sizeof G[0]; k++) {
            CHECK(drv_rq_fill_ok(G[k].ch, G[k].st, G[k].v) == G[k].pass);
        }
        /* through the law: B mid-fill with a fill value passes, the same value
         * with B applied is replaced by 8500 and counted; a stale complement
         * in a fill state is refused; WK never takes a fill value */
        reset();
        drv_rq_prod_host_select(true);
        rq_prod_boot_pass();
        rq_prod_host_freeze(true);
        U8(RQ_OBJ_B + RO_ST) = RQF_ST_RAMP;
        U16(0xFFF84B78u + 2u * 2u) = 5704u;
        U16(0xFFF84B90u + 2u * 2u) = (uint16_t)~5704u;
        drv_rq_law();
        CHECK(CELL_RQ_I[2] == 5704u && drv_rq_m5_guard_n() == 0u);
        U16(0xFFF84B78u + 2u * 2u) = 8501u;
        U16(0xFFF84B90u + 2u * 2u) = (uint16_t)~8501u;
        drv_rq_law();
        CHECK(CELL_RQ_I[2] == 8500u && drv_rq_m5_guard_n() == 1u);
        U16(0xFFF84B78u + 2u * 2u) = 499u;
        U16(0xFFF84B90u + 2u * 2u) = (uint16_t)~499u;
        drv_rq_law();
        CHECK(CELL_RQ_I[2] == 8500u && drv_rq_m5_guard_n() == 2u);
        U16(0xFFF84B78u + 2u * 2u) = 500u;      /* the lower bound itself passes */
        U16(0xFFF84B90u + 2u * 2u) = (uint16_t)~500u;
        drv_rq_law();
        CHECK(CELL_RQ_I[2] == 500u && drv_rq_m5_guard_n() == 2u);
        U16(0xFFF84B78u + 2u * 2u) = 5704u;     /* stale complement */
        drv_rq_law();
        CHECK(CELL_RQ_I[2] == 8500u && drv_rq_m5_guard_n() == 3u);
        U16(0xFFF84B90u + 2u * 2u) = (uint16_t)~5704u;
        U8(RQ_OBJ_B + RO_ST) = RQF_ST_APPLIED;  /* applied: a fill value is refused */
        drv_rq_law();
        CHECK(CELL_RQ_I[2] == 8500u && drv_rq_m5_guard_n() == 4u);
        U16(0xFFF84B78u + 2u * 5u) = 5000u;     /* WK: the strict 500/8500 rule */
        U16(0xFFF84B90u + 2u * 5u) = (uint16_t)~5000u;
        drv_rq_law();
        CHECK(CELL_RQ_I[5] == 500u && drv_rq_m5_guard_n() == 6u);   /* B again + WK */
        rq_prod_host_freeze(false);
    }

    /* ── HOST ONLY: record 0x1FD NOT ready (0xBC82C = 0) — B runs the fill
     * (the earlier stand-in): boost, 14 × fill, blend, applied at pass 16 ── */
#if RQ_M5_STAGE != RQ_M5_STAGE_1B
    {
        rqf_in_t f = {
            .temp = RQ_M5_TEMP, .c1c = RQ_M5_C1C, .c0a = RQ_M5_C0A, .eng_stop = 0, .c3d = 1,
            .cd2 = RQ_M5_CD2, .e6a3 = RQ_M5_E6A3, .rec1fd = 0, .lp = 20000, .lp2 = 20000,
            .adapt38 = { 4, 16, 0, 0, 0 }, .adapt42 = { 600, 482, 993, 838, 611 },
            .ba7dc = { 1, 1, 0, 0, 0 }, .k4b = { 3, 4, 6, 8, 9 },
            .rec_frac = { 0, 0x2710, 0, 0, 0 },
        };
        static tr_t t[NP];
        unsigned fe = NP;
        rq_prod_host_fill_in(&f);
        run(true, t);
        print_trace(t, "host only: record 0x1FD not ready");
        for (unsigned k = 0u; k < NP; k++) {
            if (t[k].st == RQF_ST_APPLIED && fe == NP) {
                fe = k;
            }
        }
        CHECK(t[0].st == RQF_ST_RAMP && t[0].cur[2] == 8500u && fe == 16u);
        CHECK(t[1].cur[2] == 5725u && t[14].cur[2] == 5725u && t[15].cur[2] == 5642u);
        rq_prod_host_fill_in(0);
    }
#endif

    /* ── 3. a later release (host input path): no transient (decode Q2) ─ */
#if RQ_M5_STAGE != RQ_M5_STAGE_1B
    {
        rq_prod_in_t in = {
            .gear = 3, .pflow = 0, .gear_eng = 3, .c2f = 0, .e28e = 0, .e298 = 0,
            .park_req = 1, .e6c3 = 0, .b52f = 0, .sys_mode = 1, .line_p = 20000,
            .shift_src = 0, .coding = 0,
        };
        reset();
        drv_rq_prod_host_select(true);
        rq_prod_boot_pass();
        for (unsigned k = 0u; k < 30u; k++) {
            rq_prod_step_in(&in);
        }
        CHECK(rq_prod_ch(2u) == 8500u && rq_prod_ch(0u) == 500u);
        in.gear = 1;                            /* 3 → 1: A on, B off in one pass */
        rq_prod_step_in(&in);
        CHECK(rq_prod_ch(2u) == 500u);          /* B: 500 from the first pass */
        CHECK(rq_prod_ch(0u) == 8500u);         /* A: the boost */
        CHECK(U8(RQ_OBJ_B + RO_ST) == RQF_ST_RELWAIT && U8(RQ_OBJ_B + RO_MODE) == 1u);
        rq_prod_step_in(&in);
        CHECK(rq_prod_ch(0u) > 500u && rq_prod_ch(0u) < 8500u && rq_prod_ch(2u) == 500u);
        CHECK(rq_prod_unp_n() == 0u && rq_prod_f19f_n() == 0u);
    }
#endif

    if (fails == 0) {
        printf("m5fill (%s): all checks passed\n", STAGE_NAME);
    }
    return fails == 0 ? 0 : 1;
}

/* ── 4. the image and the dump ────────────────────────────────────────── */
#ifndef TCU_STOCK_IMAGE
#define TCU_STOCK_IMAGE ""
#endif
#ifndef TCU_STOCK_DUMP
#define TCU_STOCK_DUMP ""
#endif

static size_t load(const char *path, uint8_t *buf, size_t max)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    if (f == NULL) {
        return 0u;
    }
    n = fread(buf, 1u, max, f);
    fclose(f);
    return n;
}

static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

static int stock_checks(void)
{
    static uint8_t img[0x200000], ram[0x20000];
    const size_t ni = load(TCU_STOCK_IMAGE, img, sizeof img);
    const size_t nr = load(TCU_STOCK_DUMP, ram, sizeof ram);
    if (ni != sizeof img || nr != sizeof ram) {
        printf("m5fill --stock: image (%zu) or dump (%zu) not available — skipped\n", ni, nr);
        return 77;
    }
    /* every cal copy, byte for byte */
    for (uint32_t i = 0u; i < RQF_NBLOBS; i++) {
        if (memcmp(RQF_BLOBS[i].data, &img[RQF_BLOBS[i].addr], RQF_BLOBS[i].len) != 0) {
            printf("FAIL cal blob 0x%06X differs from the image\n", RQF_BLOBS[i].addr);
            fails++;
        }
    }
    /* the RAM cal pointer cells the port resolves as constants = the dump */
    for (uint32_t c = 0xFFF84D14u; c <= 0xFFF84D30u; c += 4u) {
        CHECK(be32(&ram[c - 0xFFF80000u]) == rqf_cell_ptr(c));
    }
    /* the request objects' cal cells and the per-unit / correction tables */
    for (uint32_t e = 0u; e < 5u; e++) {
        CHECK(be32(&ram[0xB7C8u + 0x60u * e + 0x10u]) == 0xFFF84D20u + 4u * e);
        CHECK(be32(&ram[0xB7C8u + 0x60u * e + 0x0Cu]) == 0x0004A008u);
    }
    {
        static const uint32_t U[14] = { 0xE004, 1, 0xE038, 1, 0xE06C, 1, 0xE0A0, 1, 0xE0D4, 1,
                                        0xE108, 1, 0xE13C, 1 };
        static const uint32_t C[4] = { 0xE170, 1, 0xE214, 1 };
        for (uint32_t k = 0u; k < 14u; k++) {
            CHECK(be32(&ram[0xCDFCu + 4u * k]) == U[k]);
        }
        for (uint32_t k = 0u; k < 4u; k++) {
            CHECK(be32(&ram[0xCDECu + 4u * k]) == C[k]);
        }
    }
    /* the inputs */
    CHECK(ram[0x9C13] == RQ_M5_TEMP && ram[0x9C1C] == RQ_M5_C1C && ram[0x9C0A] == RQ_M5_C0A &&
          ram[0x9D10] == RQ_M5_ENGSTOP && ram[0x9C3D] == RQ_M5_C3D && ram[0x9CD2] == RQ_M5_CD2 &&
          ram[0xB119] == RQ_M5_B119 && ram[0xB0FE] == RQ_M5_B0FE && ram[0xE3ED] == RQ_M5_E3ED &&
          ram[0xE6A3] == RQ_M5_E6A3 && be16(&ram[0xBCC0]) == RQ_M5_LP2);
    CHECK(be16(&ram[0xE222]) == 10u && be16(&ram[0xE220]) == 1u);
    /* the kiss pressures / +0x4B the port takes as constants */
    {
        static const uint16_t K42[5] = { RQ_M5_AD42_A, RQ_M5_AD42_B, RQ_M5_AD42_C, RQ_M5_AD42_D, RQ_M5_AD42_E };
        static const uint8_t K4B[5] = { 3u, 4u, 6u, 8u, 9u };
        for (uint32_t e = 0u; e < 5u; e++) {
            CHECK(be16(&ram[0xB7C8u + 0x60u * e + 0x42u]) == K42[e]);
            CHECK(ram[0xB7C8u + 0x60u * e + 0x4Bu] == K4B[e]);
        }
        CHECK(be16(&ram[0xB7C8u + 0x38u]) == RQ_M5_AD38_A && be16(&ram[0xB828u + 0x38u]) == RQ_M5_AD38_B);
    }
    /* code constants of the fill */
    {
        static const struct { uint32_t a; uint8_t n; uint8_t b[4]; } K[] = {
            { 0xE40EC, 2, { 0x80, 0x47 } },               /* 0xE40DA: mov.b r0,@(7,r4): mode = 0 */
            { 0xE4306, 4, { 0x06, 0x00, 0x4E, 0x20 } },   /* 0xE4214: boost movi20 #20000 */
            { 0xE3E86, 4, { 0x07, 0x00, 0x27, 0x10 } },   /* 0xE3E28: state 3 → 10000 */
            { 0xE4DBE, 4, { 0x06, 0x00, 0x27, 0x10 } },   /* 0xE4DBA: == 10000 */
            { 0xE3B3E, 4, { 0x06, 0x00, 0x27, 0x10 } },   /* 0xE39DC: state-3 entry 10000 */
            { 0xE42BE, 2, { 0xE5, 0x0A } },               /* 0xE4214: blend window 10 */
            { 0xE42E8, 2, { 0xE0, 0x0A } },               /* 0xE4214: / 10 */
            { 0xE44FA, 2, { 0xE0, 0x0A } },               /* 0xE44B6: × 10 */
            { 0xE37A2, 2, { 0x81, 0x31 } },               /* 0xE3710: +0x36 = 0 */
            { 0xE3796, 2, { 0xEE, 0x06 } },               /* 0xE3710: state 6 */
        };
        for (size_t k = 0u; k < sizeof K / sizeof K[0]; k++) {
            if (memcmp(&img[K[k].a], K[k].b, K[k].n) != 0) {
                printf("FAIL code constant at 0x%X\n", K[k].a);
                fails++;
            }
        }
        /* the boost table: 5 × 5 of 1 at *(G + 0x20) */
        CHECK(be32(&img[0x54C80u + 0x20u]) == 0x1A877Cu);
        CHECK(be32(&img[0x1A877C]) == 5u && be32(&img[0x1A8780]) == 5u);
        for (uint32_t k = 0u; k < 25u; k++) {
            CHECK(img[0x1A8784u + k] == 1u);
        }
    }
    /* the fill times against the dump: A (host input, gear 1) and B */
    {
        rq_prod_in_t in = {
            .gear = 1, .pflow = 0, .gear_eng = 3, .c2f = 0, .e28e = 0, .e298 = 0,
            .park_req = 1, .e6c3 = 0, .b52f = 0, .sys_mode = 1, .line_p = 20000,
            .shift_src = 0, .coding = 0,
        };
        reset();
        drv_rq_prod_host_select(true);
        rq_prod_step_in(&in);
        printf("A: +0x36 %u (dump %u), +0x38 %u (dump %u), +0x3A %u, +0x3C %u (dump %u), +0x42 %u, fill p %u\n",
               U16(RQ_OBJ_A + RO_TFILL), be16(&ram[0xB7C8u + 0x36u]), U16(RQ_OBJ_A + RO_ADAPT),
               be16(&ram[0xB7C8u + 0x38u]), U16(RQ_OBJ_A + RO_TQF), U16(RQ_OBJ_A + RO_TRAMP),
               be16(&ram[0xB7C8u + 0x3Cu]), U16(RQ_OBJ_A + RO_PKISS), U16(RQ_OBJ_A + RO_RES));
        CHECK(U16(RQ_OBJ_A + RO_TFILL) == be16(&ram[0xB7C8u + 0x36u]));
        CHECK(U16(RQ_OBJ_A + RO_ADAPT) == be16(&ram[0xB7C8u + 0x38u]));
        CHECK(U16(RQ_OBJ_A + RO_TQF) == be16(&ram[0xB7C8u + 0x3Au]));
        CHECK(U16(RQ_OBJ_A + RO_TRAMP) == be16(&ram[0xB7C8u + 0x3Cu]));
        in.gear = 3;
        /* the dump's B object is the product of a FILL in that power cycle
         * (B +0x58 = 0, +0x36 = 0x9F, 0xFFF8E384 = 2 — 0xE4214 is its only
         * writer; A also filled: +0x36 = 0x6B): compare it with our B after
         * a fill, i.e. on the host-only "record not ready" path */
        CHECK(ram[0xE384] == 2u && ram[0xB828u + 0x58u] == 0u && be16(&ram[0xB828u + 0x36u]) == 0x9Fu &&
              ram[0xB7C8u + 0x58u] == 1u && be16(&ram[0xB7C8u + 0x36u]) == 0x6Bu);
        {
            static rqf_in_t f;
            f = (rqf_in_t){ .temp = RQ_M5_TEMP, .c1c = RQ_M5_C1C, .c0a = RQ_M5_C0A, .c3d = 1,
                            .cd2 = RQ_M5_CD2, .e6a3 = RQ_M5_E6A3, .rec1fd = 0, .lp = 20000,
                            .lp2 = 20000, .adapt38 = { 4, 16, 0, 0, 0 },
                            .adapt42 = { 600, 482, 993, 838, 611 }, .ba7dc = { 1, 1, 0, 0, 0 },
                            .k4b = { 3, 4, 6, 8, 9 }, .rec_frac = { 0, 0x2710, 0, 0, 0 } };
            rq_prod_host_fill_in(&f);
        }
        reset();
        drv_rq_prod_host_select(true);
        for (unsigned k = 0u; k < 400u; k++) {     /* +0x40: the filter settles (time base: ours) */
            rq_prod_step_in(&in);
        }
        printf("B: +0x36 %u (dump %u), +0x38 %u (dump %u), +0x3C %u (dump %u), +0x40 %u (dump %u)\n",
               U16(RQ_OBJ_B + RO_TFILL), be16(&ram[0xB828u + 0x36u]), U16(RQ_OBJ_B + RO_ADAPT),
               be16(&ram[0xB828u + 0x38u]), U16(RQ_OBJ_B + RO_TRAMP), be16(&ram[0xB828u + 0x3Cu]),
               U16(RQ_OBJ_B + RO_PMOD), be16(&ram[0xB828u + 0x40u]));
        /* B, applied, against the dump's B object */
        {
            static const struct { uint8_t off; uint8_t w; } F[] = {
                { 0x00, 2 }, { 0x02, 2 }, { 0x04, 2 }, { 0x07, 1 }, { 0x08, 1 }, { 0x09, 1 },
                { 0x32, 1 }, { 0x33, 1 }, { 0x34, 1 }, { 0x36, 2 }, { 0x38, 2 }, { 0x3A, 2 },
                { 0x3C, 2 }, { 0x40, 2 }, { 0x42, 2 }, { 0x46, 2 }, { 0x48, 2 }, { 0x4B, 1 },
                { 0x4C, 1 }, { 0x4D, 1 }, { 0x4E, 1 }, { 0x50, 1 }, { 0x52, 1 }, { 0x53, 1 },
                { 0x54, 1 }, { 0x55, 1 }, { 0x57, 1 }, { 0x58, 1 }, { 0x59, 1 }, { 0x5A, 1 },
                { 0x5B, 1 },
            };
            for (size_t k = 0u; k < sizeof F / sizeof F[0]; k++) {
                const uint32_t o = 0xB828u + F[k].off;
                const uint32_t d = F[k].w == 1u ? ram[o] : be16(&ram[o]);
                const uint32_t g = F[k].w == 1u ? U8(RQ_OBJ_B + F[k].off) : U16(RQ_OBJ_B + F[k].off);
                if (d != g) {
                    printf("FAIL B +0x%02X: ours 0x%X, dump 0x%X\n", F[k].off, g, d);
                    fails++;
                }
            }
        }
        /* 0x851A2 (the fill bits) and 0xFFF8E384 */
        CHECK(ram[0x51A2] == RQF_CELL_851A2 && ram[0x51A5] == RQF_CELL_851A5);
        CHECK(ram[0xE384] == RQF_CELL_E384);
        rq_prod_host_fill_in(0);
        /* the record 0x1FD constants = the dump */
        CHECK(be32(&ram[0x9EDC]) == RQ_M5_REC_STATE && ram[0x5174] == RQ_M5_REC_BUSY &&
              be16(&ram[0x0EE8]) == RQ_M5_REC_A && be16(&ram[0x0EEA]) == RQ_M5_REC_B &&
              be16(&ram[0x0EEC]) == RQ_M5_REC_C && be16(&ram[0x0EEE]) == RQ_M5_REC_D &&
              be16(&ram[0x0EF0]) == RQ_M5_REC_E);
        /* the restore code: 0xBC82C reads the long at 0xFFF89EDC == 0x80 and
         * the byte 0xFFF85174 == 0; 0xBCBD4 reads 0xFFF80E28 + 0xC0 + 2·idx */
        CHECK(be32(&img[0xBC9AC]) == 0xFFF85174u && be32(&img[0xBCE64]) == 0xFFF80E28u &&
              be32(&img[0xE3988]) == 0xFFF89EDCu && be32(&img[0xE399C]) == 0xFFF89D10u);
    }
    if (fails == 0) {
        printf("m5fill --stock: image and dump checks passed\n");
    }
    return fails == 0 ? 0 : 1;
}
