/* Unit test: THE J3 AVERAGE SEED at boot (2026-11-18, ~/tcu_sweep/temp_stage2a_port.md §1).
 *
 * The r04h bench boot (witness_temps): at the temperature shadow's pass 2 the
 * selection took the DIE at −389 (−24.3 °C) status 6, source 2, because the
 * J3 4-sample average 0x908CA was seeded with the cleared cell's 0 on the
 * reply processor's first pass (before any J3 reply) — N = 104 on pass 2 —
 * until the board went valid at pass 12.
 *
 * The stock [O bytes]: 0x91ACA reads 0x9095B once at entry and skips the
 * whole condense while it is 0 (0x91AD4..D8 `tst; bt.s 0x91B94`); the batch's
 * 0x91DAA sets it 0 → 1 at its tail (0x91DB8..C6); the next 0x91ACA sees 1,
 * RESEEDS the average from the J3 sample (0x91B1A → 0x91B5E), and sets
 * 0x9095B = 0x0F (0x91BD4..D8).
 *
 * The test drives OUR batch (sup_batch_5ms, mode 0, 5 ms) with the bus-B
 * engine drained every pass, a fake supervisor whose J3 words carry N = 421
 * (41.9 °C, the r04h die) from the first reply and whose J4 device answers
 * the not-ready word 0xFF03 for its first 10 replies and then 0x14F7 (41.9 °C,
 * the low "1" bits set), and the real tsel hook at its slot. Verdicts:
 *   (1) the stock law (the gate from 0): 0x908CA = 0 at pass 1 (the cleared
 *       cell — N < 5, invalid), 421 from pass 2; 0x9095B 0 → 1 → 0x0F; the
 *       selected temperature never leaves 41.9 °C ± 2 °C while its status is
 *       ≥ 6; the die is selected at pass 2 (status 6) and the board from its
 *       first valid pass;
 *   (2) the r02g..r04i law reproduced (0x9095B pre-set to 1 before the first
 *       pass = condense from pass 1 with the window seeded from the cleared
 *       telemetry cell's 0, as the old s_avg_seeded law did): the
 *       r04h finding — N = 105 at pass 2 and a selected −24 °C at status 6 —
 *       MUST appear (the test's own negative control).
 */
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_walk.h"
#include "bsw/sup/sup_batch.h"
#include "bsw/sup/sup_ctrl.h"
#include "bsw/sup/sup_cells.h"
#include "app/tsel_shadow.h"
#include "bsw/drv/drv_tsel.h"       /* TSEL_LIVE: j3seed_live (stage 2a) */
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <stdlib.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

#define DIE_N    421u        /* T = −738 + 3.347·421 = 671 (41.9 °C) */
#define BRD_WORD 0x14F7u     /* 5367/128 = 41.93 °C; >> 3 = 670 (1/16 °C) */
#define T_TRUE   671

static unsigned s_j4_n;

static uint16_t chip(uint16_t tx)
{
    switch (tx) {
    case 0x2800u: return (uint16_t)(0xA800u | (DIE_N >> 6));          /* J3 rx[0] */
    case 0x2E00u: return (uint16_t)(0xA800u | (DIE_N & 0x3Fu));       /* J3 rx[1] */
    case 0x00FFu:                                                      /* J4 */
        return (s_j4_n++ < 10u) ? 0xFF03u : BRD_WORD;
    default:      return 0xAA00u;
    }
}

static void drain_rspi(void)
{
    for (uint8_t b = 0u; b < 2u; b++) {
        unsigned guard = 512u;
        while (stub_rspi_busy(b) && guard-- != 0u) {
            stub_rspi_isr(b);
        }
    }
}

typedef struct {
    int implausible;         /* passes with 0x940FC >= 6 and |t − 41.9 °C| > 2 °C */
    int16_t worst;           /* the worst such selected temperature */
    uint16_t n_pass[4];      /* 0x908CA seen by the shadow at passes 1..4 */
    uint8_t gate_after[3];   /* 0x9095B after passes 1..3 */
    uint8_t src2, st2;       /* the selection at pass 2 */
    unsigned first_brd;      /* the first pass with source 4 */
} run_t;

static void boot(run_t *r, uint8_t pre_gate)
{
    *(volatile uint8_t *)(uintptr_t)0xFFF9095Bu = 0u;   /* sup_cells_init's clear */
    CELL_TELEM_AVG = 0u;
    CELL_TELEMETRY = 0u;
    CELL_SSLB2_COPY = 0u;
    s_j4_n = 0u;
    sup_jobs_init();
    sup_walk_init();
    sup_batch_init();
    tsel_hook_init();
#if TSEL_LIVE
    /* stage 2a: the init hub's values are live (0x9393A = 0x0780, status 0) */
    CHECK(*(volatile int16_t *)(uintptr_t)0xFFF9393Au == 0x0780);
    CHECK(*(volatile int16_t *)(uintptr_t)0xFFF93ABAu == 0x0780);
    CHECK(*(volatile uint8_t *)(uintptr_t)0xFFF940FCu == 0u);
#endif
    drain_rspi();                        /* the init J5 */
    if (pre_gate != 0u) {
        CELL_PROC_GATE = pre_gate;
    }
    for (unsigned p = 1u; p <= 60u; p++) {
        stub.frt_us += 5000u;
        sup_batch_5ms();                 /* the hub hook runs inside (mode 0) */
        const volatile tsel_t *t = tsel_shadow_state();
        if (p <= 4u) r->n_pass[p - 1u] = t->in.n908ca;
        if (p <= 3u) r->gate_after[p - 1u] = CELL_PROC_GATE;
        if (p == 2u) { r->src2 = t->s170; r->st2 = t->s0fc; }
        if (t->s170 == 4u && r->first_brd == 0u) r->first_brd = p;
#if TSEL_LIVE
        /* stage 2a: the LIVE cells carry the selection, every pass */
        CHECK(*(volatile int16_t *)(uintptr_t)0xFFF9393Au == t->s93a);
        CHECK(*(volatile uint8_t *)(uintptr_t)0xFFF940FCu == t->s0fc);
        CHECK(*(volatile uint8_t *)(uintptr_t)0xFFF94170u == t->s170);
        CHECK(*(volatile uint8_t *)(uintptr_t)0xFFF940FFu == t->s0ff);
#endif
        if (t->s0fc >= 6u && abs(t->s93a - T_TRUE) > 32) {
            if (r->implausible == 0) r->worst = t->s93a;
            r->implausible++;
        }
        if (p <= 14u) printf("  pass %2u: gate %02X 908CA %5u 908CC %04X | sel %5d st %u src %u | die %5d st %u | brd %5d st %u\n",
               p, CELL_PROC_GATE, t->in.n908ca, (uint16_t)t->in.r908cc, t->s93a, t->s0fc, t->s170,
               t->aba, t->s15e, t->s93e, t->s100);
        drain_rspi();                    /* the queued bus-B jobs complete within the tick */
    }
}

int main(void)
{
    run_t a = {0}, b = {0};

    stub.sup_raw_fn = chip;

    printf("(1) the stock law (0x9095B from 0):\n");
    boot(&a, 0u);
    CHECK(a.n_pass[0] == 0u);            /* the cleared cell — N < 5 */
    CHECK(a.n_pass[1] == DIE_N);         /* reseeded from the first J3 reply */
    CHECK(a.n_pass[2] == DIE_N && a.n_pass[3] == DIE_N);
    CHECK(a.gate_after[0] == 1u);        /* the batch's 0x91DAA tail */
    CHECK(a.gate_after[1] == 0x0Fu);     /* the reseeding condense */
    CHECK(a.gate_after[2] == 0x0Fu);
    CHECK(a.implausible == 0);
    CHECK(a.src2 == 2u && a.st2 == 6u);  /* the die at its true value, status 6 */
    CHECK(a.first_brd >= 11u && a.first_brd <= 14u);
    printf("    implausible passes %d, board from pass %u\n", a.implausible, a.first_brd);

    printf("(2) negative control — the r02g..r04i condense (gate pre-set to 1):\n");
    boot(&b, 1u);
    CHECK(b.n_pass[0] == 0u);
    CHECK(b.n_pass[1] >= 100u && b.n_pass[1] <= 110u);   /* (421 + 0·3 + 2) >> 2 = 105 */
    CHECK(b.implausible > 0);
    CHECK(b.worst < -300);               /* the r04h −389 (−24.3 °C) class */
    printf("    implausible passes %d, worst %d (1/16 °C)\n", b.implausible, b.worst);

    if (fails == 0) {
        printf("test_j3seed: ok\n");
    }
    return fails != 0;
}
