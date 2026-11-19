/* Unit test: STAGE 2b — the die / board fault flags 0x940E1 / E2 / E4 / E5
 * LIVE (2026-11-19, ~/tcu_sweep/temp_stage2b_port.md).
 *
 * The chain under test, at our firmware's slots: the temperature hub hook
 * (app/tsel_hook.c → the shadow's 0x88DA8 / 0x88BD0 with their reports
 * 0x7C0B6 / 0x7C01E → app/tsel_live.c → bsw/drv/drv_tsel.c) → 0x7C100
 * (drv_temp_pub_tick: 0x940E0 = E2 ? FF : die byte, 0x940E3 = E5 ? FF :
 * board byte) → the safety layer (safety/tmp_safe.c: ED20's gather / D2 D3
 * D7 0x14B34A, E998's 0x140416 / 0x1405FA) → 0xB9646 → 0xB986C → 0x89C13.
 *
 *   1. the driving dump: from the dump's inputs (J3 N = 379, J4 0x10AB, no
 *      comparator flag, variant 0, the fluid) the live flags are the dump's
 *      E1 0x68 / E2 0 / E4 0xE0 / E5 0; 0x7C100 publishes the die / board
 *      bytes (the dump's 0x940E0 = 0x940E3 = 0x49); after a 100 ms slice
 *      ED20 D2 = D3 = D7 = 0 and E998's second output is the board byte with
 *      status 0 — the dump's own state (ED20 +0xD2/D3/D7 = 0, E998 +0x34 =
 *      +0x2C, +0x35 = 0); with TCU_STOCK_DUMP those cells are compared;
 *   2. THE FLUID-SENSOR FAILURE (0x940E8 set, mode 1): the stock rule
 *      0x1405FA [O bytes 0x1405FA..0x1406D2] — D5 = 1 and D3 = 0 → E9C8 =
 *      max(E998+0x2C − cal 0x195DB7, 0) = the BOARD byte 0x940E3 (cal 0),
 *      status 1; the die never substitutes for E9C8. So 0x89C13 = the board
 *      byte, not 0x82; at boot state 1 (0x140416: no board path) it is 0x82
 *      until the first 100 ms slice; the board failed too → 0x82; the board
 *      never ready (0xFF03 forever: no failed flag, the init byte 0xA0 =
 *      120 °C published) → 0xA0; an implausible fluid jump latched → the
 *      board byte;
 *   3. fault injection with the stock's timing: the board word 0x8001 after
 *      valid (0x20), the low bits missing (0x80), out of range (0x40) —
 *      each after the 600 ms hold (60 mode-1 passes of 10 ms); 0xFF03
 *      forever (no flag); the die below range (0x20), above range (0x40),
 *      a comparator mismatch (0x08) — each after the 40 ms hold; then the
 *      published byte goes 0xFF and D3 / D2 follow at the next 100 ms slice.
 *
 * The STAND-IN negative (ctest tsel2b_neg_standin, TSEL_LIVE_FLAGS=0) must
 * FAIL: with the flags held at 0x80 none of 1 / 2 / 3 holds.
 */
#include "app/tsel_shadow.h"
#include "bsw/drv/drv_tsel.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_temp.h"
#include "bsw/sup/sup_cells.h"
#include "app/tmp_chain.h"
#include "safety/tmp_safe.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <stdlib.h>

#if !TSEL_LIVE
#error "test_tsel2b runs the live path: build it with TSEL_LIVE=1"
#endif

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, _a, _b); fails++; } } while (0)

#define U8(a)   (*(volatile uint8_t  *)(uintptr_t)(a))
#define U16(a)  (*(volatile uint16_t *)(uintptr_t)(a))
#define S16(a)  (*(volatile int16_t  *)(uintptr_t)(a))
#define E8(off) U8(TS_E998 + (off))
#define D8(off) U8(TS_ED20 + (off))

#define F_E1 0xFFF940E1u
#define F_E2 0xFFF940E2u
#define F_E4 0xFFF940E4u
#define F_E5 0xFFF940E5u

void app_temp_hub_hook(void);

/* the inputs the hub reads (tsel_hook.c), held by the test */
static uint16_t in_n = 379u;            /* J3 average (dump: 381 / 379, the torn pair) */
static uint16_t in_j4 = 0x10ABu;        /* J4 word (dump 0x93ABC) */
static uint8_t  in_cmp = 0u;            /* 0x9095E */
static uint8_t  in_fbyte = 0x42u;       /* 0x94102 (dump 0x940E6 = 0x42) */
static uint8_t  in_fflags = 0u;         /* 0x940E8 (the fluid's failed flags) */
static uint8_t  in_mode = 0u;           /* 0x93B33 */
static uint32_t s_ms;

/* the cleared RAM a cold boot gives (sup_cells_init's job on the target;
 * the ranges this chain touches) */
static void cells_clear(void)
{
    for (uint32_t a = 0xFFF89C00u; a < 0xFFF89D40u; a++) {
        U8(a) = 0u;
    }
    for (uint32_t a = 0xFFF90900u; a < 0xFFF90A00u; a++) {
        U8(a) = 0u;
    }
    for (uint32_t a = 0xFFF93900u; a < 0xFFF94200u; a++) {
        U8(a) = 0u;
    }
    for (uint32_t a = 0xFFF94F80u; a < 0xFFF95000u; a++) {
        U8(a) = 0u;
    }
}

static void env_boot(void)
{
    cells_clear();
    drv_temp_init();                    /* main.c order */
    tmp_chain_init();
    S16(0xFFF94F98u) = 411;             /* the fluid 25.7 °C (dump), status 8 */
    U8(0xFFF94FEEu) = 8u;
    tsel_hook_init();
    s_ms = 0u;
}

/* one hub pass: the hub hook (0x88DA8 / 0x88BD0 + the live writes), then
 * 0x7C100 (sup_batch.c: the same supervisor tick) */
static void hub(void)
{
    U16(0xFFF908CAu) = in_n;
    S16(0xFFF908CCu) = (int16_t)in_j4;
    U8(0xFFF9095Eu) = in_cmp;
    U8(0xFFF90966u) = 0u;
    U8(0xFFF93B33u) = in_mode;
    app_temp_hub_hook();
    CELL_TMP_BYTE_SEL = in_fbyte;
    CELL_TMP_PUB_E8 = in_fflags;
    drv_temp_pub_tick();
}

/* 10 ms of main.c: mode 0 — two hub passes (5 ms); modes 1..3 — one (the
 * 10 ms part); then task_10ms's temperature byte (10 / 30 / 100 ms) */
static void ms10(void)
{
    hub();
    if (in_mode == 0u) {
        hub();
    }
    tmp_chain_task_10ms(in_mode);
    tmp_chain_task_slow(in_mode);
    s_ms += 10u;
}

/* a normal boot to mode 1: 600 ms of mode 0, boot state 1, then mode 1 */
static void boot_to_mode1(unsigned mode1_ms)
{
    env_boot();
    in_mode = 0u;
    for (unsigned k = 0u; k < 60u; k++) {
        ms10();
    }
    tmp_chain_boot_pass();              /* sup_mode.c: boot state 1 */
    in_mode = 1u;
    for (unsigned k = 0u; k < mode1_ms / 10u; k++) {
        ms10();
    }
}

/* ── 1. the driving dump ──────────────────────────────────────────────── */
static void test_dump(void)
{
    in_n = 379u;
    in_j4 = 0x10ABu;
    in_cmp = 0u;
    in_fbyte = 0x42u;
    in_fflags = 0u;
    boot_to_mode1(300u);
    printf("dump: E1 %02X E2 %02X E4 %02X E5 %02X  E0 %02X E3 %02X  D2 %u D3 %u D7 %u  "
           "E998 +2C %02X +2F %02X +34 %02X +35 %u  E9C8 %02X st %u  89C13 %02X\n",
           U8(F_E1), U8(F_E2), U8(F_E4), U8(F_E5), CELL_TMP_PUB_E0, CELL_TMP_PUB_E3,
           D8(TS_D_D2), D8(TS_D_D3), D8(TS_D_D7), E8(TS_E_IN2), E8(TS_E_IN2F),
           E8(TS_E_OUT2), E8(TS_E_ST2), E8(TS_E_OUT), E8(TS_E_ST), U8(0xFFF89C13u));
    CHECKV(U8(F_E1), 0x68u);
    CHECKV(U8(F_E2), 0x00u);
    CHECKV(U8(F_E4), 0xE0u);
    CHECKV(U8(F_E5), 0x00u);
    CHECKV(CELL_TMP_PUB_E0, 0x49u);                 /* die 0x940FE (N 379) */
    CHECKV(CELL_TMP_PUB_E3, 0x49u);                 /* board 0x94101 (0x10AB) */
    CHECKV(D8(TS_D_B9), 0u);
    CHECKV(D8(TS_D_BB), 0u);
    CHECKV(D8(TS_D_D2), 0u);
    CHECKV(D8(TS_D_D3), 0u);
    CHECKV(D8(TS_D_D7), 0u);
    CHECKV(E8(TS_E_IN2F), 0u);
    CHECKV(E8(TS_E_OUT2), E8(TS_E_IN2));            /* the second output = the board byte */
    CHECKV(E8(TS_E_ST2), 0u);
    CHECKV(E8(TS_E_ST), 0u);                        /* the fluid path, unchanged */
    {
        const tsel_t *t = (const tsel_t *)(uintptr_t)tsel_shadow_state();
        CHECKV(U8(F_E1), t->e1);                    /* live = the shadow */
        CHECKV(U8(F_E2), t->e2);
        CHECKV(U8(F_E4), t->e4);
        CHECKV(U8(F_E5), t->e5);
    }
#if defined(TCU_STOCK_DUMP)
    {
        static uint8_t d[0x20000];
        FILE *f = fopen(TCU_STOCK_DUMP, "rb");
        if (f != NULL && fread(d, 1u, sizeof d, f) == sizeof d) {
#define DB(a) ((unsigned)d[(a) - 0xFFF80000u])
            CHECKV(U8(F_E1), DB(F_E1));
            CHECKV(U8(F_E2), DB(F_E2));
            CHECKV(U8(F_E4), DB(F_E4));
            CHECKV(U8(F_E5), DB(F_E5));
            CHECKV(CELL_TMP_PUB_E0, DB(0xFFF940E0u));
            CHECKV(CELL_TMP_PUB_E3, DB(0xFFF940E3u));
            CHECKV(D8(TS_D_B9), DB(TS_ED20 + TS_D_B9));
            CHECKV(D8(TS_D_BB), DB(TS_ED20 + TS_D_BB));
            CHECKV(D8(TS_D_D2), DB(TS_ED20 + TS_D_D2));
            CHECKV(D8(TS_D_D3), DB(TS_ED20 + TS_D_D3));
            CHECKV(D8(TS_D_D7), DB(TS_ED20 + TS_D_D7));
            CHECKV(E8(TS_E_IN2F), DB(TS_E998 + TS_E_IN2F));
            CHECKV(E8(TS_E_ST2), DB(TS_E998 + TS_E_ST2));
            /* the dump's own second output equals its own gathered board
             * byte (0x4C / 0x4C — a later read than 0x940E3's 0x49) */
            CHECK(DB(TS_E998 + TS_E_OUT2) == DB(TS_E998 + TS_E_IN2));
            printf("dump: the 13 flag / consumer cells compared against %s\n", TCU_STOCK_DUMP);
#undef DB
        } else {
            printf("dump: %s not readable — the file comparison skipped\n", TCU_STOCK_DUMP);
        }
        if (f != NULL) {
            fclose(f);
        }
    }
#endif
}

/* ── 2. the fluid-sensor failure ──────────────────────────────────────── */
static unsigned ticks_to(volatile uint8_t *cell, uint8_t want, unsigned max10)
{
    unsigned k;
    for (k = 0u; k < max10 && *cell != want; k++) {
        ms10();
    }
    return k;
}

static void test_fluid_failure(void)
{
    unsigned k;
    const uint8_t brd = 0x55u;          /* 0x164B → 713 (44.6 °C) → byte 0x55 */

    /* (a) healthy boot to mode 1, then the fluid sensor fails */
    in_n = 433u;
    in_j4 = 0x164Bu;
    in_cmp = 0u;
    in_fbyte = 0x4Cu;
    in_fflags = 0u;
    boot_to_mode1(500u);
    CHECKV(U8(0xFFF89C13u), 0x4Cu);
    in_fflags = 0x80u;                  /* 0x7BF8C: the fluid's failed flag */
    k = ticks_to((volatile uint8_t *)(uintptr_t)0xFFF89C13u, brd, 30u);
    printf("fluid failure (board %02X): 0x89C13 = %02X after %u ticks of 10 ms, E9C8 status %u, "
           "D5 %u D3 %u\n", brd, U8(0xFFF89C13u), k, E8(TS_E_ST), D8(TS_D_D5), D8(TS_D_D3));
    CHECKV(U8(0xFFF89C13u), brd);
    CHECK(k <= 14u);                    /* 100 + 30 + 10 ms */
    CHECKV(E8(TS_E_ST), 1u);
    CHECKV(E8(TS_E_OUT), brd);
    for (k = 0u; k < 50u; k++) {
        ms10();
        CHECK(U8(0xFFF89C13u) == brd);
    }
    /* (b) the board fails too (0x8001 after valid → 0x20 after 600 ms) →
     * D3 → 0x82 */
    in_j4 = 0x8001u;
    k = ticks_to((volatile uint8_t *)(uintptr_t)0xFFF89C13u, 0x82u, 100u);
    printf("…and the board lost: 0x89C13 = %02X after %u ticks, E5 %02X, D3 %u\n",
           U8(0xFFF89C13u), k, U8(F_E5), D8(TS_D_D3));
    CHECKV(U8(0xFFF89C13u), 0x82u);
    CHECKV(U8(F_E5), 0x20u);
    CHECKV(E8(TS_E_ST), 2u);

    /* (c) the failure present at boot: 0x140416 has no board path → 0x82
     * at boot state 1, the board byte after the first mode-1 slices */
    in_n = 433u;
    in_j4 = 0x164Bu;
    in_fbyte = 0x4Cu;
    in_fflags = 0x80u;
    env_boot();
    in_mode = 0u;
    for (k = 0u; k < 60u; k++) {
        ms10();
    }
    tmp_chain_boot_pass();
    CHECKV(U8(0xFFF89C13u), 0x82u);
    CHECKV(E8(TS_E_ST), 2u);
    in_mode = 1u;
    k = ticks_to((volatile uint8_t *)(uintptr_t)0xFFF89C13u, brd, 30u);
    printf("fluid failed at boot: 0x82 at boot state 1, then 0x89C13 = %02X after %u ticks of mode 1\n",
           U8(0xFFF89C13u), k);
    CHECKV(U8(0xFFF89C13u), brd);

    /* (d) the board never ready (0xFF03 for ever): no failed flag (code 0),
     * the init byte 0xA0 (0x9393E = 0x0780, 120 °C) published — the stock's
     * own fallback then reads 120 °C */
    in_j4 = 0xFF03u;
    in_fflags = 0u;
    boot_to_mode1(300u);
    CHECKV(U8(F_E5), 0u);
    CHECKV(U8(F_E4), 0xE0u);
    CHECKV(CELL_TMP_PUB_E3, 0xA0u);
    CHECKV(U8(0xFFF940FCu), 6u);                    /* the die selected */
    in_fflags = 0x80u;
    k = ticks_to((volatile uint8_t *)(uintptr_t)0xFFF89C13u, 0xA0u, 30u);
    printf("board never ready + fluid failure: 0x89C13 = %02X (the board's init byte) after %u ticks\n",
           U8(0xFFF89C13u), k);
    CHECKV(U8(0xFFF89C13u), 0xA0u);

    /* (e) an implausible fluid jump, latched (r03u: |new − last| > 5 keeps
     * +0xC8 until power-off) → the board byte instead of 0x82 */
    in_j4 = 0x164Bu;
    in_fbyte = 0x4Eu;
    in_fflags = 0u;
    boot_to_mode1(300u);
    in_fflags = 0x80u;
    for (k = 0u; k < 20u; k++) {
        ms10();
    }
    in_fflags = 0u;
    in_fbyte = 0x5Au;
    for (k = 0u; k < 100u; k++) {
        ms10();
    }
    printf("fluid jump latched: jump %u, 0x89C13 = %02X (board %02X)\n", D8(TS_D_JUMP), U8(0xFFF89C13u), brd);
    CHECKV(D8(TS_D_JUMP), 1u);
    CHECKV(U8(0xFFF89C13u), brd);
}

/* ── 3. fault injection, the stock's timing ───────────────────────────── */
/* mode-1 ticks (10 ms, one hub pass each) from the injection until the
 * flag reads `want` (k = 60 → the flag was written by the 60th pass after
 * the injection: 600 ms) */
static unsigned flag_after(uint32_t cell, uint8_t want, unsigned max10)
{
    unsigned k;
    for (k = 0u; k < max10 && U8(cell) != want; k++) {
        ms10();
    }
    return k;
}

static void inj_board(const char *what, uint16_t word, uint8_t want)
{
    unsigned k;
    in_n = 433u;
    in_j4 = 0x164Bu;
    in_cmp = 0u;
    in_fbyte = 0x4Cu;
    in_fflags = 0u;
    boot_to_mode1(300u);
    CHECKV(U8(F_E5), 0u);
    in_j4 = word;
    k = flag_after(F_E5, want, 100u);
    /* the slice after: 0x7C100's byte and ED20's D3 (the 100 ms part) */
    for (unsigned j = 0u; j < 11u; j++) {
        ms10();
    }
    printf("board %-22s word %04X: E5 = %02X after %u ms (status %u), E3 %02X, D3 %u, D7 %u, 0x89C13 %02X\n",
           what, word, U8(F_E5), 10u * k, U8(0xFFF94100u), CELL_TMP_PUB_E3, D8(TS_D_D3),
           D8(TS_D_D7), U8(0xFFF89C13u));
    CHECKV(U8(F_E5), want);
    CHECK(k >= 59u && k <= 61u);                    /* the 600 ms hold (120 × 5 ms) */
    CHECKV(CELL_TMP_PUB_E3, 0xFFu);
    CHECKV(D8(TS_D_D3), 1u);
    CHECKV(D8(TS_D_D7), 0u);                        /* one source only */
    CHECKV(U8(0xFFF89C13u), 0x4Cu);                 /* the fluid byte holds */
}

static void inj_die(const char *what, uint16_t n, uint8_t cmp, uint8_t want)
{
    unsigned k;
    in_n = 433u;
    in_j4 = 0x164Bu;
    in_cmp = 0u;
    in_fbyte = 0x4Cu;
    in_fflags = 0u;
    boot_to_mode1(300u);
    CHECKV(U8(F_E2), 0u);
    in_n = n;
    in_cmp = cmp;
    k = flag_after(F_E2, want, 30u);
    for (unsigned j = 0u; j < 11u; j++) {
        ms10();
    }
    printf("die   %-22s N %4u cmp %u: E2 = %02X after %u ms (status %u), E0 %02X, D2 %u, 0x89C13 %02X\n",
           what, n, cmp, U8(F_E2), 10u * k, U8(0xFFF9415Eu), CELL_TMP_PUB_E0, D8(TS_D_D2),
           U8(0xFFF89C13u));
    CHECKV(U8(F_E2), want);
    CHECK(k >= 3u && k <= 5u);                      /* the 40 ms hold (8 × 5 ms) */
    CHECKV(CELL_TMP_PUB_E0, 0xFFu);
    CHECKV(D8(TS_D_D2), 1u);
    CHECKV(U8(0xFFF89C13u), 0x4Cu);
}

static void test_injection(void)
{
    unsigned k;
    inj_board("0x8001 after valid", 0x8001u, 0x20u);
    inj_board("low 1-bits missing", 0x1648u, 0x80u);
    inj_board("above 168 C", 0x5407u, 0x40u);
    inj_die("below range (N<5)", 3u, 0u, 0x20u);
    inj_die("above range (N>1022)", 1023u, 0u, 0x40u);
    /* the comparator mismatch: T 44.6 °C (< 128 °C) but the supervisor's
     * 128 °C comparator bit 0 set → 0x200 → 0x30 → status 7 → 4, code 2 */
    inj_die("comparator mismatch", 433u, 1u, 0x08u);
    /* the board not ready for ever: no failed flag, ever */
    in_n = 433u;
    in_j4 = 0xFF03u;
    in_cmp = 0u;
    in_fflags = 0u;
    boot_to_mode1(2000u);
    printf("board 0xFF03 for ever: E4 %02X E5 %02X, board status %u, selection %u / source %u\n",
           U8(F_E4), U8(F_E5), U8(0xFFF94100u), U8(0xFFF940FCu), U8(0xFFF94170u));
    CHECKV(U8(F_E5), 0u);
    CHECKV(U8(F_E4), 0xE0u);
    /* recovery: a good word clears the failed flag at once (the code is
     * rebuilt every pass) */
    in_j4 = 0x164Bu;
    in_n = 433u;
    boot_to_mode1(300u);
    in_j4 = 0x8001u;
    (void)flag_after(F_E5, 0x20u, 100u);
    CHECKV(U8(F_E5), 0x20u);
    in_j4 = 0x164Bu;
    k = flag_after(F_E5, 0u, 10u);
    printf("board recovery: E5 = %02X after %u ms\n", U8(F_E5), 10u * k);
    CHECKV(U8(F_E5), 0u);
    CHECK(k == 1u);                     /* the next pass */
}

int main(void)
{
    test_dump();
    test_fluid_failure();
    test_injection();
    printf("%s (%d failures)\n", fails == 0 ? "PASS" : "FAIL", fails);
    return fails == 0 ? 0 : 1;
}
