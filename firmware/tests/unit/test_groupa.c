/* Unit test: the stock's group-A switch-output chain (drv_groupa.c —
 * 0x7B6D8 slots 10..14 → 0x849D4 → 0x8ADF4 → 0x91500) and the hold-law
 * temperature producer (drv_temp.c — 0x887D8 / 0x8D5BA / 0x8D4B4).
 *
 * Built four ways (CMakeLists):
 *   test_groupa         host runtime path (demands / test release set here):
 *                       the law, the pull-in, the curves, TOE30/31/32, the
 *                       'U' gate and the 0x95005 window, the full-on test
 *                       windows + hold + interlock, the temperature producer
 *                       against the stock driving dump, the list cadence
 *   test_groupa_stage0  DRV_GA_COMPILE_TIME, DRV_GA_STAGE 0 — the TARGET
 *                       default: every DRLD written is 0 in every state
 *   test_groupa_stage1  DRV_GA_STAGE 1 — the park hold: 2000 x 4 passes,
 *                       then the compensated duty; nothing else nonzero
 *   test_groupa_stage2  DRV_GA_STAGE 2 — the tests released, demands 0
 *
 * Driving dump (stockram_driving_20261104.bin) values used: 0x94F9C =
 * 0x019B (25.7 °C), status 8, 0x94102/3 = 42 42; AN table AN1 = 164,
 * AN8 = 3031 mV; 0x93AA4 = 3033, 0x93AA6 = 164, 0x93AA8 = 0x0B35, 0x93A9C
 * = 0x32C1, 0x93A9E = 0x0880, 0x93AA0 = 0x019A; rail node 0x94ED0 =
 * 0x326C, supply 0x94F86 = 0x3293; 0x94111 = 0x0B, 0x9480B = 02 02 00 02 00.
 */
#include "bsw/drv/drv_groupa.h"
#include "bsw/drv/drv_temp.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_batch.h"
#include "bsw/sup/sup_ladder.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, _a, _b); fails++; } } while (0)

static void reset(void)
{
    sup_cells_init();                   /* clears the pinned ranges (host window) */
    drv_jobs_init();                    /* → drv_rq_init */
    drv_temp_init();
    drv_ga_init();
    memset(stub.te_drld, 0, sizeof stub.te_drld);
    memset(stub.te_dtr, 0, sizeof stub.te_dtr);
    memset(stub.te_crld, 0, sizeof stub.te_crld);
    stub.te_writes = stub.te_cycle_writes = stub.te_bad_sc = 0u;
    stub.an_converts = 0u;
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    CELL_LAD_95005 = 0u;                /* after 'L' */
    CELL_LAD_94114 = 0u;
    CELL_DIAG_94115 = 0u;
    CELL_J2_STAGED = 0x9020u;           /* engaged */
    CELL_LAD_REQ[0] = 1u;
    CELL_LAD_REQ[1] = 0u;
    CELL_OUT_MV[0] = 12940u;            /* r03f rail node under the P set */
    CELL_SUPPLY_RAW = 12973u;
    CELL_TMP = 411;                     /* dump 0x94F9C */
}

/* the countdown service 0x7C7E4 (the 5 ms list's head; sup_ladder) */
static void countdown(void) { sup_ladder_tick_head(); }

/* one law pass in list order: slots (0x7AB9C), law (0x7CC32), F (0x8EE30) */
static void law_pass(void)
{
    drv_ga_slots_tick();
    drv_ga_law_tick();
    drv_ga_fstage();
}

static void pass5_mode0(void) { countdown(); law_pass(); }
#if !DRV_GA_CT
static void pass10_mode1(void) { countdown(); countdown(); law_pass(); }
#endif

#if !DRV_GA_CT
/* ── the curves and 0x9203C, hand-computed ───────────────────────────── */
static void test_curve(void)
{
    static const int16_t K1[5] = { 2, -480, 2240, 5400, 9380 };
    static const int16_t K0[5] = { 2, -640, 2240, 5835, 10355 };
    CHECKV(drv_ga_curve_s16(K1, 411), 6704);    /* 5400 + 3980*891/2720 = 6703.74 */
    CHECKV(drv_ga_curve_s16(K1, -480), 5400);
    CHECKV(drv_ga_curve_s16(K1, -800), 5400);   /* below x[0] */
    CHECKV(drv_ga_curve_s16(K1, 2240), 9380);
    CHECKV(drv_ga_curve_s16(K1, 4000), 9380);
    CHECKV(drv_ga_curve_s16(K1, 0), 6102);      /* 6102.35 */
    CHECKV(drv_ga_curve_s16(K1, 1600), 8444);   /* 100 °C: 8443.53 */
    CHECKV(drv_ga_curve_s16(K0, 411), 7484);    /* 5835 + 4520*1051/2880 = 7484.47 */
    CHECKV(drv_ga_9203c(518u, 2000u), 1036u);
    CHECKV(drv_ga_9203c(515u, 2000u), 1030u);
    CHECKV(drv_ga_9203c(502u, 2000u), 1004u);
    CHECKV(drv_ga_9203c(999u, 2000u), 1998u);
    CHECKV(drv_ga_9203c(1000u, 2000u), 2000u);
    CHECKV(drv_ga_9203c(0u, 2000u), 0u);
}

/* ── the type-1 duty at several temperatures / rail voltages (TOE31) ──── */
static uint16_t hold_duty(int16_t t, uint16_t mv)
{
    reset();
    CELL_TMP = t;
    CELL_OUT_MV[0] = mv;
    drv_ga_demand_set(1u, 10000u);
    for (unsigned i = 0u; i < 6u; i++) {
        pass5_mode0();
    }
    return CELL_GA_DUTY[1];
}

static void test_hold_law(void)
{
    CHECKV(hold_duty(411, 12940u), 518u);        /* 6704000 / 12940 = 518.08 */
    CHECKV(stub.te_drld[1], 1036u);
    CHECKV(hold_duty(411, 13000u), 515u);        /* 515.69 */
    CHECKV(stub.te_drld[1], 1030u);
    CHECKV(hold_duty(411, 13340u), 502u);        /* 502.55 */
    CHECKV(stub.te_drld[1], 1004u);
    CHECKV(hold_duty(-480, 12940u), 417u);       /* -30 °C: 5400000/12940 = 417.3 */
    CHECKV(hold_duty(1600, 12940u), 652u);       /* 100 °C: 8444000/12940 = 652.5 */
    CHECKV(hold_duty(1920, 12940u), 688u);       /* the 120 °C default: 8912 → 688.7 */
    CHECKV(hold_duty(411, 7100u), 944u);         /* 944.2: under the 950 clamp */
    CHECKV(hold_duty(411, 7000u), 1000u);        /* 957.7 > 950 → 1000 */
    CHECKV(hold_duty(411, 0u), 1000u);           /* V_src 0 → 1000 */
    /* the stage-0 read-out follows the same law */
    (void)hold_duty(411, 12940u);
    CHECKV(drv_ga_wit_curve(), 6704u);
    CHECKV(drv_ga_wit_permil(), 518u);
    CHECKV(drv_ga_wit_drld(), 1036u);
}

/* ── the pull-in: 20 ms at 1000 ‰ in both list cadences ───────────────── */
static void test_pullin(void)
{
    reset();
    drv_ga_demand_set(1u, 10000u);
    for (unsigned i = 0u; i < 4u; i++) {
        pass5_mode0();
        CHECKV(CELL_GA_MODE[1], 1u);
        CHECKV(CELL_GA_DUTY[1], 1000u);
        CHECKV(stub.te_drld[1], 2000u);
        CHECKV(CELL_GA_PULLIN[1], 4u - i);       /* reload 4, then 3, 2, 1 */
    }
    pass5_mode0();                               /* 5th pass = 20 ms: the law */
    CHECKV(CELL_GA_DUTY[1], 518u);
    CHECKV(stub.te_drld[1], 1036u);
    CHECKV(CELL_GA_PULLIN[1], 0u);
    for (unsigned i = 0u; i < 20u; i++) {
        pass5_mode0();
    }
    CHECKV(stub.te_drld[1], 1036u);
    /* off: 0 at once; and the next on re-arms the pull-in */
    drv_ga_demand_set(1u, 0u);
    pass5_mode0();
    CHECKV(CELL_GA_MODE[1], 0u);
    CHECKV(stub.te_drld[1], 0u);
    CHECKV(CELL_GA_PULLIN[1], 0u);               /* last duty was nonzero: no reload yet */
    pass5_mode0();
    CHECKV(CELL_GA_PULLIN[1], 4u);               /* re-armed while off (dump: 02) */
    drv_ga_demand_set(1u, 10000u);
    pass5_mode0();
    CHECKV(stub.te_drld[1], 2000u);
    /* mode 1: the law at 10 ms, the countdown at 5 ms → 2 passes x 10 ms */
    reset();
    drv_ga_demand_set(1u, 10000u);
    pass10_mode1();
    CHECKV(stub.te_drld[1], 2000u);
    pass10_mode1();
    CHECKV(stub.te_drld[1], 2000u);
    CHECKV(CELL_GA_PULLIN[1], 2u);
    pass10_mode1();
    CHECKV(stub.te_drld[1], 1036u);
}

/* ── the 'U' gate and the pre-'L' window 0x95005 ─────────────────────── */
static void test_u_gate(void)
{
    static const uint8_t NOT_U[] = { 0x11u, 0x16u, 0x22u, 0x33u, (uint8_t)'3', (uint8_t)'4', 0u };
    for (unsigned j = 0u; j < sizeof NOT_U; j++) {
        reset();
        drv_ga_demand_set(1u, 10000u);
        CELL_SEQ2_STATE[0] = NOT_U[j];
        for (unsigned i = 0u; i < 8u; i++) {
            pass5_mode0();
            CHECKV(CELL_GA_MODE[1], 0u);
            CHECKV(stub.te_drld[1], 0u);
        }
        CHECKV(CELL_GA_ALLOWED & 0x02u, 0u);
    }
    /* 'U' but the sequencer before 'L' (0x95005 = 1): the test branch, no
     * test asked → mode 0 although the demand and the rail are there */
    reset();
    drv_ga_demand_set(1u, 10000u);
    CELL_LAD_95005 = 1u;
    pass5_mode0();
    CHECKV(CELL_GA_ALLOWED & 0x02u, 0x02u);
    CHECKV(CELL_GA_MODE[1], 0u);
    CHECKV(stub.te_drld[1], 0u);
    CELL_LAD_95005 = 0u;                         /* 'L' */
    pass5_mode0();
    CHECKV(CELL_GA_MODE[1], 1u);
    CHECKV(stub.te_drld[1], 2000u);
    /* 'U' lost mid-hold → off in the same pass */
    for (unsigned i = 0u; i < 6u; i++) {
        pass5_mode0();
    }
    CHECKV(stub.te_drld[1], 1036u);
    CELL_SEQ2_STATE[0] = (uint8_t)'4';
    pass5_mode0();
    CHECKV(stub.te_drld[1], 0u);
}

/* ── TOE30: the force-off, then the compensated law (curve 0x191572) ──── */
static void test_toe30(void)
{
    reset();
    drv_ga_demand_set(0u, 10000u);
    pass5_mode0();
    CHECKV(CELL_GA_DEMAND[0], 0u);               /* 0x94FEA = 1: forced 0 */
    CHECKV(CELL_GA_DEMAND_C[0], 0xFFFFu);
    CHECKV(stub.te_drld[0], 0u);
    CELL_GA_OFF10 = 0u;                          /* the safety layer releases */
    CELL_GA_OFF10_C = 0xFFu;
    for (unsigned i = 0u; i < 4u; i++) {
        pass5_mode0();
        CHECKV(stub.te_drld[0], 2000u);
    }
    pass5_mode0();
    CHECKV(CELL_GA_DUTY[0], 578u);               /* 7484000 / 12940 = 578.4 */
    CHECKV(stub.te_drld[0], 1156u);
}

/* ── TOE32: battery-supplied (0x94F86 ≥ 7000 on, < 6700 off), type 0 ──── */
static void test_toe32(void)
{
    reset();
    CELL_SEQ2_STATE[0] = 0x22u;                  /* rail off: irrelevant for TOE32 */
    drv_ga_demand_set(3u, 10000u);
    CELL_SUPPLY_RAW = 6900u;                     /* inside the hysteresis, from clear */
    pass5_mode0();
    CHECKV(CELL_GA_ALLOWED & 0x08u, 0u);
    CHECKV(stub.te_drld[3], 0u);
    CELL_SUPPLY_RAW = 7000u;
    for (unsigned i = 0u; i < 10u; i++) {
        pass5_mode0();
        CHECKV(stub.te_drld[3], 2000u);          /* pull-in, then type 0 = 1000 */
    }
    CHECKV(CELL_GA_DUTY[3], 1000u);
    CELL_SUPPLY_RAW = 6700u;                     /* still >= 6700: stays on */
    pass5_mode0();
    CHECKV(stub.te_drld[3], 2000u);
    CELL_SUPPLY_RAW = 6699u;
    pass5_mode0();
    CHECKV(CELL_GA_ALLOWED & 0x08u, 0u);
    CHECKV(stub.te_drld[3], 0u);
    /* in seq2's test states TOE32 keeps its demand (its source is 0xFE)… */
    CELL_SUPPLY_RAW = 12973u;
    CELL_SEQ2_STATE[0] = 0x11u;
    drv_ga_tests_set(true);
    CELL_DIAG_94115 = 1u;
    pass5_mode0();
    CHECKV(CELL_GA_MODE[3], 1u);
    /* …but the pre-'L' window forces it off, and it never gets a full-on */
    CELL_LAD_95005 = 1u;
    pass5_mode0();
    CHECKV(CELL_GA_MODE[3], 0u);
    CHECKV(stub.te_drld[3], 0u);
    CHECKV(CELL_GA_FULL & 0x08u, 0u);
}

/* ── the full-on test windows, the hold and the interlock ─────────────── */
static void test_fullon(void)
{
    /* held (the stage-0/1 default): mode 0, counted */
    reset();
    CELL_SEQ2_STATE[0] = 0x11u;
    CELL_LAD_94114 = 1u;
    CELL_J2_STAGED = 0x9000u;
    CELL_LAD_REQ[0] = 0u;
    pass5_mode0();
    CHECKV(CELL_GA_MODE[0], 0u);
    CHECKV(CELL_GA_MODE[1], 0u);
    CHECKV(stub.te_drld[1], 0u);
    CHECK(drv_ga_thold_n() == 2u);               /* TOE30 + TOE31 */
    /* released, rail quiet: TOE30 + TOE31 at 1000 ‰, TOE32 not */
    drv_ga_tests_set(true);
    pass5_mode0();
    CHECKV(CELL_GA_MODE[0], 2u);
    CHECKV(CELL_GA_MODE[1], 2u);
    CHECKV(CELL_GA_MODE[3], 0u);
    CHECKV(CELL_GA_FULL, 0x03u);
    CHECKV(stub.te_drld[0], 2000u);
    CHECKV(stub.te_drld[1], 2000u);
    CHECKV(stub.te_drld[3], 0u);
    /* 0x16 too; '3' only through 0x94115 */
    CELL_SEQ2_STATE[0] = 0x16u;
    pass5_mode0();
    CHECKV(CELL_GA_MODE[1], 2u);
    CELL_SEQ2_STATE[0] = (uint8_t)'3';
    pass5_mode0();
    CHECKV(CELL_GA_MODE[1], 0u);                 /* 0x94114 counts only in 0x11/0x16 */
    CELL_DIAG_94115 = 1u;
    pass5_mode0();
    CHECKV(CELL_GA_MODE[1], 2u);
    /* the pre-'L' window: 'U' + 0x95005 = 1 + 0x94115 → full on */
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    CELL_LAD_95005 = 1u;
    pass5_mode0();
    CHECKV(CELL_GA_MODE[1], 2u);
    CELL_LAD_95005 = 0u;                         /* after 'L': not a test state */
    pass5_mode0();
    CHECKV(CELL_GA_MODE[1], 0u);                 /* no demand */
    /* our interlock: a request or 0x9020 up → refused, counted */
    CELL_SEQ2_STATE[0] = 0x11u;
    CELL_J2_STAGED = 0x9020u;
    {
        const uint32_t n0 = drv_ga_ilock_n();
        pass5_mode0();
        CHECKV(CELL_GA_MODE[1], 0u);
        CHECKV(stub.te_drld[1], 0u);
        CHECK(drv_ga_ilock_n() == n0 + 2u);
    }
}

/* ── the F stage: head check, cycle-change branch, enables ────────────── */
static void test_fstage(void)
{
    reset();
    for (unsigned i = 0u; i < 5u; i++) {
        pass5_mode0();
    }
    CHECKV(drv_ga_f238_n(), 0u);                 /* DTR follows DRLD */
    stub.te_dtr[3] = 7u;                         /* a TOE32 DTR off its command */
    pass5_mode0();
    CHECKV(drv_ga_f238_n(), 1u);
    /* TOE30/31 have no head check */
    stub.te_dtr[1] = 9u;
    pass5_mode0();
    CHECKV(drv_ga_f238_n(), 1u);
    /* a cycle change (0x90888 != 0x90892) takes the CRLD branch every pass */
    CELL_GA_FS_CYCNOW[1] = 1000u;
    pass5_mode0();
    CHECKV(stub.te_cycle_writes, 1u);
    CHECKV(stub.te_crld[1], 2000u);
    /* only 0x90888[0] is refreshed from cal each pass (bytes: bld #0) */
    CELL_GA_FS_CYC[1] = 1500u;
    CELL_GA_FS_CYC[0] = 1500u;
    pass5_mode0();
    CHECKV(CELL_GA_FS_CYC[0], 2000u);
    CHECKV(CELL_GA_FS_CYC[1], 1500u);
    CHECKV(stub.te_bad_sc, 0u);
    /* the disabled outputs TOE33 / TOE22: cleared every pass, DRLD 0 */
    CELL_GA_DUTY[2] = 777u;
    pass5_mode0();
    CHECKV(CELL_GA_DUTY[2], 0u);
    CHECKV(stub.te_drld[2], 0u);
    CHECKV(stub.te_drld[4], 0u);
    CHECKV(CELL_GA_PULLIN[2], 0u);               /* never re-armed (dump: 00) */
    CHECKV(CELL_GA_PULLIN[4], 0u);
}

/* ── the dump's static state ─────────────────────────────────────────── */
static void test_dump_state(void)
{
    reset();
    CELL_OUT_MV[0] = 0x326Cu;
    CELL_SUPPLY_RAW = 0x3293u;
    pass5_mode0();
    pass5_mode0();
    CHECKV(CELL_GA_ALLOWED, 0x0Bu);              /* dump 0x94111 = 0B */
    CHECKV(CELL_GA_FULL, 0x00u);                 /* dump 0x94113 = 00 */
    CHECKV(CELL_GA_FAULT, 0x00u);                /* dump 0x94112 = 09: 0x938F6 not ported */
    for (unsigned k = 0u; k < 5u; k++) {
        CHECKV(CELL_GA_MODE[k], 0u);
        CHECKV(CELL_GA_DUTY[k], 0u);
        CHECKV(CELL_GA_DEMAND[k], 0u);
        CHECKV(CELL_GA_DEMAND_C[k], 0xFFFFu);
        CHECKV(stub.te_drld[k], 0u);
    }
    /* 0x9480B right after the law: 04 04 00 04 00; the dump's 02 02 00 02
     * 00 is the same cells two countdown steps later (mode 1) */
    CHECKV(CELL_GA_PULLIN[0], 4u);
    CHECKV(CELL_GA_PULLIN[1], 4u);
    CHECKV(CELL_GA_PULLIN[2], 0u);
    CHECKV(CELL_GA_PULLIN[3], 4u);
    CHECKV(CELL_GA_PULLIN[4], 0u);
    countdown();
    countdown();
    CHECKV(CELL_GA_PULLIN[1], 2u);
    CHECKV(CELL_GA_TEMP_COPY, 0x019Bu);          /* dump 0x94FBE */
}

/* ── the temperature producer against the driving dump ────────────────── */
static void temp_pass(void) { countdown(); drv_temp_tick(); }

static void test_temp(void)
{
    reset();
    CHECKV(CELL_TMP_ACQ, 0x0780);                /* 0x8D57C: 120 °C default */
    CHECKV(CELL_CNT_TMP_START, 2u);
    CHECKV(CELL_CNT_TMP_HOLD, 20u);
    CELL_AN_MV(1) = 164u;
    CELL_AN_MV(8) = 3033u;
    /* the first reading is a jump from the cleared 0x93AAA (delta 2869 >
     * 100 → 0x100): rejected, the 120 °C default stays (status still 0) */
    temp_pass();
    CHECKV(CELL_TMP_FLAGS, 0x0180u);
    CHECKV(CELL_TMP_ACQ, 0x0780);
    CHECKV(CELL_TMP_ACQ_ST, 0u);
    /* the second (0x94822 = 2 → 1 → 0: the start-up grace is over too) */
    temp_pass();
    CHECKV(CELL_TMP_SENS, 3033u);
    CHECKV(CELL_TMP_REF, 164u);
    CHECKV(CELL_TMP_DIFF, 0x0B35u);
    CHECKV(CELL_TMP_RATIO, 0x32C1u);             /* 12993 */
    CHECKV(CELL_TMP_ACQ, 0x019A);                /* 410 = dump 0x93AA0 */
    CHECKV(CELL_TMP_ACQ_ST, 8u);
    CHECKV(CELL_TMP_FLAGS, 0x0880u);             /* dump 0x93A9E */
    CELL_AN_MV(8) = 3031u;                       /* the dump's AN table value */
    temp_pass();
    CHECKV(CELL_TMP_ACQ, 0x019B);
    CHECKV(CELL_TMP, 0x019B);                    /* dump 0x94F9C */
    CHECKV(CELL_TMP_C, 0xFE64u);                 /* dump 0x94E14 */
    CHECKV(CELL_TMP_SEL, 0x019B);
    CHECKV(CELL_TMP_RAW, 0x019B);
    CHECKV(CELL_TMP_ST, 8u);
    CHECKV(CELL_TMP_ST_C, 0xF7u);
    CHECKV(CELL_TMP_BYTE_SEL, 0x42u);            /* dump 0x94102/3 = 42 42 */
    CHECKV(CELL_TMP_BYTE, 0x42u);
    CHECKV(CELL_TMP_9416E, 0x01u);               /* dump 05: 0x940FC (8) not produced here */
    CHECKV(CELL_TMP_9416F, 0x01u);
    CHECKV(CELL_TMP_DDIFF, 0xFFFEu);             /* 3031 - 3033 */
    CHECKV(CELL_CNT_TMP_HOLD, 20u);
    /* the curve end points */
    {
        static const uint16_t T[33] = {
            16u, 538u, 568u, 653u, 747u, 880u, 1069u, 1334u, 1692u, 2151u, 2687u, 3241u,
            3735u, 4110u, 4355u, 4457u, 4494u, 35488u, 35328u, 35008u, 34768u, 34528u,
            34288u, 34048u, 33808u, 33568u, 33328u, 33088u, 32848u, 32608u, 32368u,
            32208u, 32128u,
        };
        CHECKV(drv_temp_curve_u16(T, 538u) - 0x8000u, 2720u);    /* 170 °C */
        CHECKV(drv_temp_curve_u16(T, 100u) - 0x8000u, 2720u);
        CHECKV((int16_t)(drv_temp_curve_u16(T, 4494u) - 0x8000u), -640);   /* -40 °C */
        CHECKV(drv_temp_curve_u16(T, 3241u) - 0x8000u, 320u);    /* 20 °C */
    }
    /* an implausible sensor (AN8 open, 0 mV): hold 100 ms (status 7), then
     * the 120 °C default (status 3) */
    CELL_AN_MV(8) = 0u;
    temp_pass();
    CHECKV(CELL_TMP_ACQ_ST, 7u);
    CHECKV(CELL_TMP_ACQ, 0x019B);
    CHECK((CELL_TMP_FLAGS & 0x0804u) == 0x0804u);
    for (unsigned i = 0u; i < 18u; i++) {        /* 0x94770: 19 → 1 */
        temp_pass();
    }
    CHECKV(CELL_TMP_ACQ_ST, 7u);
    temp_pass();                                 /* 0x94770 reached 0: 100 ms */
    CHECKV(CELL_TMP_ACQ_ST, 3u);
    CHECKV(CELL_TMP_ACQ, 0x0780);
    CHECKV(CELL_TMP, 0x0780);
    CHECK((CELL_TMP_FLAGS & 0x0200u) != 0u);
    /* back to plausible: the jump pass is rejected, the next one taken */
    CELL_AN_MV(8) = 3031u;
    temp_pass();
    CHECKV(CELL_TMP_ACQ_ST, 3u);
    CHECK((CELL_TMP_FLAGS & 0x0180u) == 0x0180u);
    temp_pass();
    CHECKV(CELL_TMP, 0x019B);
    CHECKV(CELL_TMP_ST, 8u);
}

/* ── the stock list cadence through sup_batch_5ms ─────────────────────── */
static void test_cadence(void)
{
    reset();
    sup_jobs_init();
    sup_batch_init();
    CELL_OS_MODE = 0u;
    CELL_OS_MODE_PEND = 0u;
    {
        const unsigned w0 = stub.te_writes, a0 = stub.an_converts;
        for (unsigned i = 0u; i < 8u; i++) {
            sup_batch_5ms();
            CELL_OS_MODE = 0u;
        }
        CHECKV(stub.te_writes - w0, 40u);        /* mode 0: 5 outputs every 5 ms */
        CHECKV(stub.an_converts - a0, 8u);
    }
    CELL_OS_MODE = 1u;
    {
        const unsigned w0 = stub.te_writes, a0 = stub.an_converts;
        for (unsigned i = 0u; i < 8u; i++) {
            sup_batch_5ms();
            CELL_OS_MODE = 1u;
        }
        CHECKV(stub.te_writes - w0, 20u);        /* mode 1: the 10 ms list */
        CHECKV(stub.an_converts - a0, 4u);
    }
}
#endif /* !DRV_GA_CT */

#if DRV_GA_CT
/* ── the compile-time builds: drive every state, collect every DRLD ───── */
static uint16_t s_max[5];

static void sweep_pass(void)
{
    pass5_mode0();
    for (unsigned k = 0u; k < 5u; k++) {
        if (stub.te_drld[k] > s_max[k]) {
            s_max[k] = stub.te_drld[k];
        }
    }
}

static void sweep_all_states(void)
{
    static const uint8_t SEQ[] = { 0x11u, 0x16u, 0x22u, 0x33u, (uint8_t)'3', (uint8_t)'U', (uint8_t)'4', 0u };
    memset(s_max, 0, sizeof s_max);
    for (unsigned a = 0u; a < sizeof SEQ; a++) {
        for (unsigned b = 0u; b < 16u; b++) {
            CELL_SEQ2_STATE[0] = SEQ[a];
            CELL_LAD_94114 = (uint8_t)(b & 1u);
            CELL_DIAG_94115 = (uint8_t)((b >> 1) & 1u);
            CELL_LAD_95005 = (uint8_t)((b >> 2) & 1u);
            CELL_J2_STAGED = (b & 8u) != 0u ? 0x9020u : 0x9000u;
            CELL_LAD_REQ[0] = (b & 8u) != 0u ? 1u : 0u;
            CELL_GA_OFF10 = 0u;                  /* even with the force-off released */
            CELL_GA_OFF10_C = 0xFFu;
            for (unsigned i = 0u; i < 6u; i++) {
                sweep_pass();
            }
        }
    }
}
#endif

#if DRV_GA_CT
/* the slot-11 getter (app/park.c on the target): the test plays the park
 * logic's demand. Stage 1 passes it only when it equals the bound (10000);
 * stages 0 / 2 never read it. */
#include "bsw/drv/drv_park.h"
static uint16_t s_test_hold = 10000u;
static unsigned s_test_hold_reads;
uint16_t park_slot11_hold(void)
{
    s_test_hold_reads++;
    return s_test_hold;
}
uint16_t park_slot7_release(void) { return 0u; }
#endif

#if DRV_GA_CT && DRV_GA_STAGE == 0
static void test_stage0(void)
{
    reset();
    sweep_all_states();
    for (unsigned k = 0u; k < 5u; k++) {
        CHECKV(s_max[k], 0u);                    /* NO group-A duty, any state */
        CHECKV(CELL_GA_DEMAND[k], 0u);
    }
    CHECK(stub.te_writes > 0u);                  /* the law did run and write */
    CHECKV(drv_ga_guard_n(), 0u);                /* the law itself produced 0 */
    CHECK(drv_ga_thold_n() > 0u);                /* the stock's tests were asked for */
    /* the read-out: what the hold would get */
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    CELL_LAD_95005 = 0u;
    CELL_TMP = 411;
    CELL_OUT_MV[0] = 12940u;
    pass5_mode0();
    CHECKV(drv_ga_wit_permil(), 518u);
    CHECKV(drv_ga_wit_drld(), 1036u);
    CHECKV(stub.te_drld[1], 0u);
    CHECKV(CELL_GA_PULLIN[1], 4u);               /* the pull-in machinery runs */
    CHECKV(s_test_hold_reads, 0u);               /* stage 0 never asks the park logic */
}
#endif

#if DRV_GA_CT && DRV_GA_STAGE == 1
static void test_stage1(void)
{
    static const uint16_t WANT[8] = { 2000u, 2000u, 2000u, 2000u, 1036u, 1036u, 1036u, 1036u };
    reset();
    for (unsigned i = 0u; i < 8u; i++) {
        pass5_mode0();
        CHECKV(stub.te_drld[1], WANT[i]);
        CHECKV(stub.te_drld[0], 0u);
        CHECKV(stub.te_drld[3], 0u);
    }
    CHECKV(CELL_GA_DEMAND[1], 10000u);
    CHECKV(CELL_GA_DEMAND_C[1], 0xD8EFu);
    sweep_all_states();
    CHECKV(s_max[0], 0u);
    CHECKV(s_max[2], 0u);
    CHECKV(s_max[3], 0u);
    CHECKV(s_max[4], 0u);
    CHECKV(s_max[1], 2000u);                     /* the pull-in only */
    CHECKV(drv_ga_guard_n(), 0u);
    CHECK(drv_ga_thold_n() > 0u);                /* tests still held */
    CHECK(s_test_hold_reads > 0u);               /* the demand came from the getter */
    /* the bound: the logic's "off" (0) and any non-stock value give 0 */
    s_test_hold = 0u;
    pass5_mode0();
    CHECKV(CELL_GA_DEMAND[1], 0u);
    s_test_hold = 5000u;
    pass5_mode0();
    CHECKV(CELL_GA_DEMAND[1], 0u);
    s_test_hold = 0xFFFFu;
    pass5_mode0();
    CHECKV(CELL_GA_DEMAND[1], 0u);
    CHECKV(stub.te_drld[1], 0u);
    s_test_hold = 10000u;
    pass5_mode0();
    CHECKV(CELL_GA_DEMAND[1], 10000u);
}
#endif

#if DRV_GA_CT && DRV_GA_STAGE == 2
static void test_stage2(void)
{
    reset();
    for (unsigned k = 0u; k < 5u; k++) {
        CHECKV(stub.te_drld[k], 0u);
    }
    /* a stock test window with a quiet rail: TOE30 / TOE31 at 1000 ‰ */
    CELL_SEQ2_STATE[0] = 0x11u;
    CELL_LAD_94114 = 1u;
    CELL_J2_STAGED = 0x9000u;
    CELL_LAD_REQ[0] = 0u;
    pass5_mode0();
    CHECKV(stub.te_drld[0], 2000u);
    CHECKV(stub.te_drld[1], 2000u);
    CHECKV(stub.te_drld[3], 0u);
    /* out of the window: nothing (demands 0) */
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    CELL_LAD_94114 = 0u;
    pass5_mode0();
    CHECKV(stub.te_drld[0], 0u);
    CHECKV(stub.te_drld[1], 0u);
    sweep_all_states();
    CHECKV(s_max[2], 0u);
    CHECKV(s_max[3], 0u);                        /* TOE32: never in a test */
    CHECKV(s_max[4], 0u);
    CHECKV(drv_ga_guard_n(), 0u);
    CHECKV(drv_ga_thold_n(), 0u);
}
#endif

int main(void)
{
#if !DRV_GA_CT
    test_curve();
    test_hold_law();
    test_pullin();
    test_u_gate();
    test_toe30();
    test_toe32();
    test_fullon();
    test_fstage();
    test_dump_state();
    test_temp();
    test_cadence();
#elif DRV_GA_STAGE == 0
    test_stage0();
#elif DRV_GA_STAGE == 1
    test_stage1();
#else
    test_stage2();
#endif
    if (fails != 0) {
        printf("test_groupa: %d failure(s)\n", fails);
        return 1;
    }
    printf("test_groupa: ok\n");
    return 0;
}
