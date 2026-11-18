/* Unit test: M5-0, the stock request PRODUCER (app/rq_prod.c) in front of
 * the request law (drv_reqlaw.c), built with -DDRV_RQ_USE_PRODUCER=1 — the
 * flashed application's configuration. ~/tcu_sweep/m5_0_port.md.
 *
 *   1. one pass with the M5-0 inputs: channel 0..6 currents = the r03f P
 *      set (A 500, D/B/E/C 8500, WK 500, Sys 500), the producer's cells as
 *      the stock driving dump holds them, every witness counter 0;
 *   2. WIRE IDENTITY: the full bus-A cyclic (45) + config (40) words, the
 *      law's cells, pass by pass through every seq2 state, the full-on
 *      tests, 'U', both park-release values and the rounding phases —
 *      producer vs. the r03f constant requests: byte-identical;
 *   3. the guard: a value outside {500, 8500} or a complement mismatch is
 *      replaced by the r03f value and counted;
 *   4. the table law on other inputs (host-only rq_prod_step_in): the
 *      decode's per-gear currents, the overrides, neutral idle, reaction 37;
 *   5. (--stock) the const data and code constants against the pristine
 *      image, and the producer's cells against the driving dump; exit 77
 *      (skipped) when the files are absent.
 */
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_rqprod.h"
#include "bsw/sup/sup_cells.h"
#include "app/rq_prod.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

#define U8(a)   (*(volatile uint8_t  *)(uintptr_t)(a))
#define U16(a)  (*(volatile uint16_t *)(uintptr_t)(a))
#define U32(a)  (*(volatile uint32_t *)(uintptr_t)(a))

static const uint16_t PSET[7] = { 500u, 8500u, 8500u, 8500u, 8500u, 500u, 500u };

static void reset(void)
{
    drv_jobs_init();                    /* → drv_rq_init → rq_prod_init */
    /* the temperature byte is LIVE since 2026-11-17 (app/tmp_chain.c owns
     * 0x89C13 / 0x89CD2; the producer only reads them): set the dump's */
    U8(0xFFF89C13u) = RQ_M5_TEMP_DUMP;
    U8(0xFFF89CD2u) = RQ_M5_TEMP_DUMP;
    CELL_SEQ2_STATE[0] = 0x11u;
    CELL_LAD_94114 = 0u;
    CELL_DIAG_94115 = 0u;
    CELL_J2_STAGED = 0x9000u;
    CELL_LAD_REQ[0] = 0u;
    CELL_LAD_REQ[1] = 0u;
    CELL_OUT_MV[0] = 13360u;
}

/* M5-1: the producer runs at the stock cadence (boot pass, then the 10 ms
 * task in mode 1), not from the law; settle() takes it past the boot fill
 * of B to the steady state M5-0 asserted */
static void settle(void)
{
    rq_prod_boot_pass();
    for (unsigned k = 0u; k < 30u; k++) {
        rq_prod_task_10ms(1u);
    }
}

/* a 10 ms pass with a host input set, n times */
static void step_n(const rq_prod_in_t *in, unsigned n)
{
    for (unsigned k = 0u; k < n; k++) {
        rq_prod_step_in(in);
    }
}

/* ── the producer's cells: the dump's values for the fields the port owns ─ */
typedef struct { uint32_t a; uint8_t w; uint32_t v; } cellv_t;
static const cellv_t DUMP_CELLS[] = {
    /* request objects +0 p, +2 res, +4 lim, +6 set, +7 mode, +8 src, +9 prio, +0x53 */
    { 0xFFF8B7C8u, 2, 0x0000 }, { 0xFFF8B7CAu, 2, 0x0000 }, { 0xFFF8B7CCu, 2, 0x0000 },
    { 0xFFF8B7CEu, 1, 0 }, { 0xFFF8B7CFu, 1, 1 }, { 0xFFF8B7D0u, 1, 0 }, { 0xFFF8B7D1u, 1, 4 },
    { 0xFFF8B81Bu, 1, 0 },
    { 0xFFF8B828u, 2, 0x4E20 }, { 0xFFF8B82Au, 2, 0x4E20 }, { 0xFFF8B82Cu, 2, 0x4E20 },
    { 0xFFF8B82Eu, 1, 0 }, { 0xFFF8B82Fu, 1, 1 }, { 0xFFF8B830u, 1, 0 }, { 0xFFF8B831u, 1, 4 },
    { 0xFFF8B87Bu, 1, 1 },
    { 0xFFF8B888u, 2, 0 }, { 0xFFF8B88Au, 2, 0 }, { 0xFFF8B88Cu, 2, 0 },
    { 0xFFF8B88Eu, 1, 0 }, { 0xFFF8B88Fu, 1, 1 }, { 0xFFF8B890u, 1, 0 }, { 0xFFF8B891u, 1, 4 },
    { 0xFFF8B8DBu, 1, 0 },
    { 0xFFF8B8E8u, 2, 0 }, { 0xFFF8B8EAu, 2, 0 }, { 0xFFF8B8ECu, 2, 0 },
    { 0xFFF8B8EEu, 1, 0 }, { 0xFFF8B8EFu, 1, 1 }, { 0xFFF8B8F0u, 1, 0 }, { 0xFFF8B8F1u, 1, 4 },
    { 0xFFF8B93Bu, 1, 0 },
    { 0xFFF8B948u, 2, 0 }, { 0xFFF8B94Au, 2, 0 }, { 0xFFF8B94Cu, 2, 0 },
    { 0xFFF8B94Eu, 1, 0 }, { 0xFFF8B94Fu, 1, 1 }, { 0xFFF8B950u, 1, 0 }, { 0xFFF8B951u, 1, 4 },
    { 0xFFF8B99Bu, 1, 0 },
    /* Sys 0xFFF8B9A8, WK 0xFFF8B9B8 */
    { 0xFFF8B9A8u, 2, 0x4E20 }, { 0xFFF8B9AAu, 2, 0x4E20 }, { 0xFFF8B9ACu, 2, 0x4E20 },
    { 0xFFF8B9AEu, 1, 0 }, { 0xFFF8B9AFu, 1, 1 }, { 0xFFF8B9B0u, 1, 0x15 }, { 0xFFF8B9B1u, 1, 4 },
    { 0xFFF8B9B8u, 2, 0 }, { 0xFFF8B9BAu, 2, 0 }, { 0xFFF8B9BCu, 2, 0 },
    { 0xFFF8B9BEu, 1, 0 }, { 0xFFF8B9BFu, 1, 1 }, { 0xFFF8B9C0u, 1, 0x16 }, { 0xFFF8B9C1u, 1, 4 },
    /* actuator objects +0 id, +1 type, +2 I, +4 ~I, +0xC request, +0x10 fill */
    { 0xFFF8B154u, 1, 0 }, { 0xFFF8B155u, 1, 0 }, { 0xFFF8B156u, 2, 0x01F4 }, { 0xFFF8B158u, 2, 0xFE0B },
    { 0xFFF8B160u, 4, 0xFFF8B7C8u }, { 0xFFF8B164u, 1, 0 },
    { 0xFFF8B17Cu, 1, 2 }, { 0xFFF8B17Du, 1, 0 }, { 0xFFF8B17Eu, 2, 0x2134 }, { 0xFFF8B180u, 2, 0xDECB },
    { 0xFFF8B188u, 4, 0xFFF8B828u }, { 0xFFF8B18Cu, 1, 0 },
    { 0xFFF8B1A4u, 1, 4 }, { 0xFFF8B1A5u, 1, 1 }, { 0xFFF8B1A6u, 2, 0x2134 }, { 0xFFF8B1A8u, 2, 0xDECB },
    { 0xFFF8B1B0u, 4, 0xFFF8B888u }, { 0xFFF8B1B4u, 1, 0 },
    { 0xFFF8B1CCu, 1, 1 }, { 0xFFF8B1CDu, 1, 1 }, { 0xFFF8B1CEu, 2, 0x2134 }, { 0xFFF8B1D0u, 2, 0xDECB },
    { 0xFFF8B1D8u, 4, 0xFFF8B8E8u }, { 0xFFF8B1DCu, 1, 0 },
    { 0xFFF8B1F4u, 1, 3 }, { 0xFFF8B1F5u, 1, 1 }, { 0xFFF8B1F6u, 2, 0x2134 }, { 0xFFF8B1F8u, 2, 0xDECB },
    { 0xFFF8B200u, 4, 0xFFF8B948u }, { 0xFFF8B204u, 1, 0 },
    { 0xFFF8B21Cu, 1, 5 }, { 0xFFF8B21Du, 1, 0 }, { 0xFFF8B21Eu, 2, 0x01F4 }, { 0xFFF8B220u, 2, 0xFE0B },
    { 0xFFF8B228u, 4, 0xFFF8B9B8u },
    { 0xFFF8B22Cu, 1, 6 }, { 0xFFF8B22Du, 1, 1 }, { 0xFFF8B22Eu, 2, 0x01F4 }, { 0xFFF8B230u, 2, 0xFE0B },
    { 0xFFF8B238u, 4, 0xFFF8B9A8u },
};

static uint32_t cell_get(const cellv_t *c)
{
    return c->w == 1u ? U8(c->a) : (c->w == 2u ? U16(c->a) : U32(c->a));
}

static void check_dump_cells(void)
{
    static const uint16_t CUR[12] = { 0x01F4, 0x2134, 0x2134, 0x2134, 0x2134, 0x01F4, 0x01F4, 0, 0, 0, 0, 0 };
    for (size_t i = 0u; i < sizeof DUMP_CELLS / sizeof DUMP_CELLS[0]; i++) {
        const uint32_t got = cell_get(&DUMP_CELLS[i]);
        if (got != DUMP_CELLS[i].v) {
            printf("FAIL cell 0x%08X: 0x%X, dump 0x%X\n", DUMP_CELLS[i].a, got, DUMP_CELLS[i].v);
            fails++;
        }
    }
    for (uint32_t k = 0u; k < 12u; k++) {
        CHECK(U16(0xFFF84B78u + 2u * k) == CUR[k]);
        CHECK(U16(0xFFF84B90u + 2u * k) == (uint16_t)~CUR[k]);
    }
}

/* ── 2. one scenario's wire, pass by pass ─────────────────────────────── */
#define NPASS 40u
typedef struct { uint16_t w[85]; uint16_t i[10]; uint8_t mode[10]; uint8_t let[10]; } pass_t;

static void scenario(bool prod, uint16_t rel, pass_t out[NPASS])
{
    /* seq2 states incl. the rail-off full-on tests, then 'U' (rounding
     * 0 → 4 → 1 ↔ 4), a dropout ('4') and back */
    static const uint8_t S[NPASS] = {
        0x00, 0x11, 0x11, 0x16, 0x11, 0x11, '3', '3', '3', 0x22,
        0x33, 'U', 'U', 'U', 'U', 'U', 'U', 'U', '4', '4',
        'U', 'U', 'U', 'U', 'U', 'U', 'U', 'U', 'U', 'U',
        '8', 'U', 'U', 'U', 'U', 'U', 'U', 'U', 'U', 'U',
    };
    reset();
    drv_rq_prod_host_select(prod);
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        /* producer: the cell holds 0 for ch 0..6 — the requests can only
         * come from the producer */
        drv_rq_request_set(ch, ch < 7u ? (prod ? 0u : PSET[ch]) : (ch == 7u ? rel : 0u));
    }
    if (prod) {
        settle();
    }
    for (unsigned k = 0u; k < NPASS; k++) {
        CELL_SEQ2_STATE[0] = S[k];
        CELL_LAD_94114 = (uint8_t)((k == 2u || k == 3u) ? 1u : 0u);     /* phase 0x10 test */
        CELL_DIAG_94115 = (uint8_t)((k == 7u) ? 1u : 0u);              /* '3' test */
        CELL_J2_STAGED = (k >= 11u) ? 0x9020u : 0x9000u;
        CELL_OUT_MV[0] = (k >= 11u) ? 12940u : 2940u;
        drv_cyclic_build(0, 8u);
        memcpy(out[k].w, drv_job_tx(DRV_CYC), 45u * sizeof(uint16_t));
        memcpy(&out[k].w[45], drv_job_tx(DRV_CFG), 40u * sizeof(uint16_t));
        for (uint8_t ch = 0u; ch < 10u; ch++) {
            out[k].i[ch] = CELL_RQ_I[ch];
            out[k].mode[ch] = CELL_RQ_MODE[ch];
            out[k].let[ch] = CELL_RQ_LETTER[ch];
        }
    }
}

static int stock_checks(void);

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--stock") == 0) {
        return stock_checks();
    }

    /* ── 1. one pass, M5-0 inputs ────────────────────────────────────── */
    reset();
    drv_rq_prod_host_select(true);
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    settle();
    drv_rq_law();
    for (uint8_t ch = 0u; ch < 7u; ch++) {
        CHECK(rq_prod_ch(ch) == PSET[ch]);
        CHECK(CELL_RQ_I[ch] == PSET[ch]);
    }
    CHECK(rq_prod_ch(7u) == RQ_PROD_INVALID);
    check_dump_cells();
    /* the write-only input mirrors (0x89C13: the live cell, set by reset()) */
    CHECK(*(volatile int8_t *)0xFFF89C2Bu == RQ_M5_GEAR && U8(0xFFF89C13u) == 0x43u && U8(0xFFF89D10u) == 0u && U8(0xFFF89C30u) == 0u &&
          U8(0xFFF89CF5u) == 1u && U16(0xFFF8BCB0u) == 20000u);
    CHECK(rq_prod_f19a_n() == 0u && rq_prod_f19f_n() == 0u && rq_prod_f1c4_n() == 0u &&
          rq_prod_unp_n() == 0u && drv_rq_m5_guard_n() == 0u);
    /* a second pass: the same (steady state) */
    rq_prod_task_10ms(1u);
    drv_rq_law();
    check_dump_cells();
    CHECK(drv_rq_m5_guard_n() == 0u);

    /* ── 2. wire identity vs the r03f constant requests ───────────────── */
    {
        static pass_t a[NPASS], b[NPASS];
        static const uint16_t RELS[2] = { 0u, 10000u };
        for (unsigned r = 0u; r < 2u; r++) {
            unsigned diff = 0u;
            scenario(false, RELS[r], a);
            scenario(true, RELS[r], b);
            for (unsigned k = 0u; k < NPASS; k++) {
                if (memcmp(&a[k], &b[k], sizeof a[k]) != 0) {
                    diff++;
                    printf("FAIL wire differs at pass %u (release %u)\n", k, RELS[r]);
                }
            }
            CHECK(diff == 0u);
            CHECK(drv_rq_m5_guard_n() == 0u && rq_prod_unp_n() == 0u);
            /* and the content is the r03f / r03k one at 'U' */
            CHECK(b[NPASS - 1u].w[0] == 0x881Au && b[NPASS - 1u].w[1] == 0x881Au &&
                  b[NPASS - 1u].w[2] == 0x881Au && b[NPASS - 1u].w[4] == 0x8815u);
            CHECK(b[NPASS - 1u].w[3] == (RELS[r] != 0u ? 0x881Au : 0x8812u));
            CHECK(b[2].w[0] == 0x881Fu);        /* the phase-0x10 full-on test, unchanged */
            CHECK(b[NPASS - 1u].i[1] == 8500u && b[NPASS - 1u].i[0] == 500u);
        }
    }

    /* ── 3. the guard ─────────────────────────────────────────────────── */
    reset();
    drv_rq_prod_host_select(true);
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    settle();
    drv_rq_law();
    rq_prod_host_freeze(true);
    U16(0xFFF84B78u + 2u * 2u) = 9000u;                 /* B: not a mode-1 current */
    U16(0xFFF84B90u + 2u * 2u) = (uint16_t)~9000u;
    U16(0xFFF84B78u + 2u * 0u) = 8500u;                 /* A: complement left stale */
    U16(0xFFF84B78u + 2u * 6u) = 0u;                    /* Sys: 0 */
    U16(0xFFF84B90u + 2u * 6u) = 0xFFFFu;
    drv_rq_law();
    CHECK(CELL_RQ_I[2] == 8500u && CELL_RQ_I[0] == 500u && CELL_RQ_I[6] == 500u);
    CHECK(drv_rq_m5_guard_n() == 3u);
    /* a 500 ↔ 8500 swap with a good complement passes (the guard's stated limit) */
    U16(0xFFF84B78u + 2u * 0u) = 8500u;
    U16(0xFFF84B90u + 2u * 0u) = (uint16_t)~8500u;
    drv_rq_law();
    CHECK(CELL_RQ_I[0] == 8500u && drv_rq_m5_guard_n() == 5u);   /* B and Sys again */
    rq_prod_host_freeze(false);
    rq_prod_task_10ms(1u);
    drv_rq_law();
    CHECK(CELL_RQ_I[0] == 500u && CELL_RQ_I[2] == 8500u && drv_rq_m5_guard_n() == 5u);

    /* ── 4. the table law on other inputs (host only) ─────────────────── */
    {
        rq_prod_in_t in = {
            .gear = 3, .pflow = 0, .gear_eng = 3, .c2f = 0, .e28e = 0, .e298 = 0,
            .park_req = 1, .e6c3 = 0, .b52f = 0, .sys_mode = 1, .line_p = 20000,
            .shift_src = 0, .r37 = 0,
        };
        /* ch order A D B E C; applied NC (A, B) → 8500, applied NO (C D E) → 500 */
        struct { int8_t g; uint8_t pf; uint16_t a, d, b, e, c; } G[] = {
            { -1, 1, 8500,  500, 8500, 8500, 8500 },   /* R  (block 1: A B D) */
            {  0, 1,  500, 8500,  500, 8500, 8500 },   /* N */
            {  1, 1, 8500, 8500, 8500, 8500,  500 },   /* 1st (A B C) */
            {  2, 1, 8500, 8500, 8500,  500, 8500 },   /* 2nd (A B E) */
            {  3, 1,  500, 8500, 8500,  500,  500 },   /* 3rd (B C E) */
            {  6, 1,  500,  500,  500,  500,  500 },   /* 6th (C D E) */
            {  8, 1, 8500,  500,  500,  500, 8500 },   /* 8th (A D E) */
            {  1, 0, 8500, 8500,  500, 8500, 8500 },   /* block 0, gear 1: A */
            {  3, 0,  500, 8500, 8500, 8500, 8500 },   /* block 0, gear 3: B — the P set */
            {  6, 0,  500, 8500,  500, 8500, 8500 },   /* block 0, gear 6: none */
        };
        reset();
        for (size_t k = 0u; k < sizeof G / sizeof G[0]; k++) {
            in.gear = G[k].g;
            in.pflow = G[k].pf;
            step_n(&in, 30u);
            CHECK(rq_prod_ch(0u) == G[k].a && rq_prod_ch(1u) == G[k].d && rq_prod_ch(2u) == G[k].b &&
                  rq_prod_ch(3u) == G[k].e && rq_prod_ch(4u) == G[k].c);
            CHECK(rq_prod_ch(5u) == 500u && rq_prod_ch(6u) == 500u);
        }
        /* neutral idle in 1st (block 1, 0xE298 = 3 → 0xD11EC 1): B (0x6312F[1] = 2) released */
        in.gear = 1; in.pflow = 1; in.gear_eng = 1; in.e298 = 3;
        step_n(&in, 30u);
        CHECK(rq_prod_ch(0u) == 8500u && rq_prod_ch(2u) == 500u && rq_prod_ch(4u) == 500u);
        in.e298 = 2;                                        /* 0xD11EC 0: the table again */
        step_n(&in, 30u);
        CHECK(rq_prod_ch(2u) == 8500u);
        /* the engagement overrides (0xD0C94): states 1..6 → all off, 8..10 → A only */
        in.e298 = 0; in.e28e = 3;
        step_n(&in, 30u);
        CHECK(rq_prod_ch(0u) == 500u && rq_prod_ch(2u) == 500u && rq_prod_ch(4u) == 8500u &&
              rq_prod_ch(1u) == 8500u && rq_prod_ch(3u) == 8500u);
        in.e28e = 9;
        step_n(&in, 30u);
        CHECK(rq_prod_ch(0u) == 8500u && rq_prod_ch(2u) == 500u && rq_prod_ch(4u) == 8500u);
        in.e28e = 7;                                        /* 0xD0C94 → 0: the table */
        step_n(&in, 30u);
        CHECK(rq_prod_ch(4u) == 500u);
        /* park request + 0xE6C3 → 0x63128: all off (block 0) */
        in.e28e = 0; in.pflow = 0; in.gear = 3; in.e6c3 = 1;
        step_n(&in, 30u);
        CHECK(rq_prod_ch(2u) == 500u && rq_prod_ch(0u) == 500u);
        in.e6c3 = 0;
        /* reaction 37 (0xD9CBC): i_min 0 for ch 0..4, not for WK / Sys (0xD9CD4..0xD9CE0) */
        in.r37 = 1;
        step_n(&in, 30u);
        CHECK(rq_prod_ch(0u) == 0u && rq_prod_ch(5u) == 500u && rq_prod_ch(6u) == 500u &&
              rq_prod_ch(2u) == 8500u);
        in.r37 = 0;
        /* line pressure below 20000: B's mode-1 law 0x107CFC still 8500 (p >= line) */
        in.line_p = 9000;
        step_n(&in, 30u);
        CHECK(rq_prod_ch(2u) == 8500u && rq_prod_ch(6u) == 500u);
        CHECK(U16(0xFFF8B9AAu) == 9000u && U16(0xFFF8B82Cu) == 9000u);   /* +4 = min(+2, Sys) */
        CHECK(rq_prod_f1c4_n() == 0u && rq_prod_f19f_n() == 0u);
        in.line_p = 20000;
        /* the unported branches count and yield 0 */
        in.sys_mode = 0;
        {
            const uint32_t n0 = rq_prod_unp_n();
            rq_prod_step_in(&in);
            CHECK(rq_prod_ch(6u) == 0u && rq_prod_unp_n() == n0 + 1u);   /* 0x107FE0 curve */
        }
        in.sys_mode = 1; in.b52f = 1;
        {
            const uint32_t n0 = rq_prod_unp_n();
            rq_prod_step_in(&in);
            CHECK(rq_prod_unp_n() == n0 + 1u && rq_prod_ch(5u) == 500u);  /* 0xDCD38.. */
        }
    }

    if (fails == 0) {
        printf("m5prod: all checks passed\n");
    }
    return fails == 0 ? 0 : 1;
}

/* ── 5. the image and the dump ────────────────────────────────────────── */
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
        printf("m5prod --stock: image (%zu) or dump (%zu) not available — skipped\n", ni, nr);
        return 77;
    }
    /* the tables */
    CHECK(memcmp(RQ_T505A4, &img[0x505A4], 120u) == 0);
    CHECK(memcmp(RQ_T63122, &img[0x63122], 6u) == 0);
    CHECK(memcmp(RQ_T63128, &img[0x63128], 6u) == 0);
    CHECK(memcmp(RQ_T6314C, &img[0x6314C], 6u) == 0);
    CHECK(memcmp(RQ_T6312E, &img[0x6312E], 10u) == 0);
    /* the table pointers in the code's literal pools */
    CHECK(be32(&img[0xDC410]) == 0x505A4u && be32(&img[0xDC408]) == 0x63122u &&
          be32(&img[0xDC404]) == 0x63128u && be32(&img[0xDC40C]) == 0x6314Cu &&
          be32(&img[0xDC3E4]) == 0x6312Fu);
    /* the calibration: group 0x537F8 → 0x1FEF6C / 0x1FEF6E = 500 / 8500 */
    CHECK(be32(&img[0x537F8]) == 0x1FEF6Cu && be32(&img[0x537FC]) == 0x1FEF6Eu);
    CHECK(be16(&img[0x1FEF6C]) == RQ_CAL_IMIN && be16(&img[0x1FEF6E]) == RQ_CAL_IMAX);
    /* code constants */
    {
        static const struct { uint32_t a; uint8_t n; uint8_t b[4]; } K[] = {
            { 0xDCC3A, 4, { 0x0D, 0x00, 0x4E, 0x20 } },   /* holder: movi20 #20000,r13 */
            { 0xDCC48, 2, { 0xE7, 0x04 } },               /* holder: prio 4 */
            { 0xDCC4A, 2, { 0xE6, 0x01 } },               /* holder: mode 1 */
            { 0xDCC42, 2, { 0xE2, 0x00 } },               /* holder: source 0 */
            { 0xDC4DE, 2, { 0xE6, 0x15 } },               /* Sys: source 0x15 */
            { 0xDC4F0, 2, { 0xE7, 0x04 } },               /* Sys: prio 4 */
            { 0xDC530, 2, { 0xE4, 0x16 } },               /* WK: source 0x16 */
            { 0xDC53E, 2, { 0xE7, 0x04 } },               /* WK: prio 4 */
            { 0xDC4F8, 2, { 0xE6, 0x01 } },               /* WK: mode 1 when not lockup */
            { 0xDC262, 2, { 0xE0, 0x3C } },               /* 0xDC1E2: block × 0x3C */
            { 0xDC26A, 2, { 0xE0, 0x06 } },               /* 0xDC1E2: row × 6 */
            { 0xE4E7E, 4, { 0x05, 0x00, 0x4E, 0x20 } },   /* 0xE4E5C: 20000 */
            { 0xE54C8, 4, { 0x05, 0x00, 0x31, 0xCE } },   /* 0xE54A4: 12750 */
            { 0x107D1C, 4, { 0x02, 0x00, 0x31, 0xCE } },  /* 0x107CFC: 12750 */
            { 0x1080BC, 4, { 0x02, 0x00, 0x31, 0xCE } },  /* 0x10809C: 12750 */
            /* 0x15BF2C: the actuator construction (id r5, type r6) */
            { 0x15BF2E, 2, { 0xE5, 0x00 } }, { 0x15BF3C, 2, { 0x66, 0x53 } },     /* A 0/0 */
            { 0x15BF3E, 2, { 0xE6, 0x00 } }, { 0x15BF48, 2, { 0xE5, 0x02 } },     /* B 2/0 */
            { 0x15BF4A, 2, { 0xE5, 0x04 } }, { 0x15BF54, 2, { 0x66, 0xD3 } },     /* C 4/1 */
            { 0x15BF5C, 2, { 0x66, 0xD3 } }, { 0x15BF60, 2, { 0x65, 0xD3 } },     /* D 1/1 */
            { 0x15BF62, 2, { 0xE5, 0x03 } }, { 0x15BF6C, 2, { 0x66, 0xD3 } },     /* E 3/1 */
            { 0x15BF6E, 2, { 0xE6, 0x00 } }, { 0x15BF7A, 2, { 0xE5, 0x05 } },     /* WK 5/0 */
            { 0x15BF7C, 2, { 0xE5, 0x06 } }, { 0x15BF88, 2, { 0x66, 0xD3 } },     /* Sys 6/1 */
            { 0x15BF30, 2, { 0xED, 0x01 } },                                      /* r13 = 1 */
        };
        for (size_t k = 0u; k < sizeof K / sizeof K[0]; k++) {
            if (memcmp(&img[K[k].a], K[k].b, K[k].n) != 0) {
                printf("FAIL code constant at 0x%X\n", K[k].a);
                fails++;
            }
        }
    }
    /* the construction's object / request literals and the vtables */
    CHECK(be32(&img[0x15BFB0]) == 0xFFF8B7C8u && be32(&img[0x15BFB8]) == 0xFFF8B828u &&
          be32(&img[0x15BFBC]) == 0xFFF8B888u && be32(&img[0x15BFC0]) == 0xFFF8B8E8u &&
          be32(&img[0x15BFC4]) == 0xFFF8B948u && be32(&img[0x15BFC8]) == 0xFFF8B9B8u &&
          be32(&img[0x15BFD0]) == 0xFFF8B9A8u);
    CHECK(be32(&img[0x507D8 + 4]) == 0xFFF8B7C8u && be32(&img[0x507D8 + 36]) == 0xFFF8B9A8u &&
          be32(&img[0x507D8 + 40]) == 0xFFF8B9B8u);
    CHECK(be32(&img[0x4A008 + 0x14]) == 0xE4E5Cu && be32(&img[0x4A0A4 + 0x14]) == 0xE54A4u &&
          be32(&img[0x4AED8 + 0x14]) == 0x107BB8u && be32(&img[0x4AF20 + 0x14]) == 0x107FA6u);

    /* the producer's cells against the dump itself (big-endian) */
    reset();
    drv_rq_prod_host_select(true);
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    settle();
    drv_rq_law();
    for (size_t i = 0u; i < sizeof DUMP_CELLS / sizeof DUMP_CELLS[0]; i++) {
        const uint32_t o = DUMP_CELLS[i].a - 0xFFF80000u;
        const uint32_t d = DUMP_CELLS[i].w == 1u ? ram[o] :
                           (DUMP_CELLS[i].w == 2u ? be16(&ram[o]) : be32(&ram[o]));
        if (d != DUMP_CELLS[i].v || cell_get(&DUMP_CELLS[i]) != d) {
            printf("FAIL dump cell 0x%08X: dump 0x%X, table 0x%X, ours 0x%X\n",
                   DUMP_CELLS[i].a, d, DUMP_CELLS[i].v, cell_get(&DUMP_CELLS[i]));
            fails++;
        }
    }
    for (uint32_t k = 0u; k < 12u; k++) {
        CHECK(be16(&ram[0x4B78u + 2u * k]) == U16(0xFFF84B78u + 2u * k));
        CHECK(be16(&ram[0x4B90u + 2u * k]) == U16(0xFFF84B90u + 2u * k));
        /* and the hand-over the request law reads (stages 3..5 not ported:
         * identity in the dump) */
        if (k < 7u) {
            CHECK(be16(&ram[0x14EE8u + 2u * k]) == U16(0xFFF84B78u + 2u * k));
        }
    }
    /* the inputs */
    CHECK(ram[0x9C2B] == 3u && ram[0x9C30] == 0u && ram[0x9C25] == 3u && ram[0x9C2F] == 0u &&
          ram[0xE28E] == 0u && ram[0xE298] == 0u && ram[0xAE29] == 1u && ram[0xE6C3] == 0u &&
          ram[0xB52F] == 0u && ram[0x9CF5] == 1u && be16(&ram[0xBCB0]) == 20000u);
    if (fails == 0) {
        printf("m5prod --stock: image and dump checks passed\n");
    }
    return fails == 0 ? 0 : 1;
}
