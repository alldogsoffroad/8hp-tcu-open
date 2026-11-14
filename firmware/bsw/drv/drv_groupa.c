/* drv_groupa.c — the stock's group-A switch-output chain (drv_groupa.h for
 * the safety frame and the staging). Ported from the bytes of 0x7B6D8
 * (slots 10..14), 0x849D4..0x84D78, 0x8ADF4..0x8AFBE, 0x8AFC0..0x8AFF4,
 * 0x886AC / 0x8D42C..0x8D4AA, 0x91500..0x9167A, 0x9203C..0x9205E
 * (sh-elf-objdump of the pristine image; the r08_app decompiles checked
 * against them); calibration read through the 0x66xxx pointer tables at
 * index 0x90AB8 = 0. ~/tcu_sweep/groupA_park_port.md.
 *
 * NOT PORTED (named stand-ins / deviations, groupA_park_port.md §4):
 *   0x7BB00  fault class: 2 when 0x938F6[k] != 0 and the slot is 0 — the
 *            output diagnosis that fills 0x938F6 (0x84DA0 / 0x8B284) is not
 *            ported, so the class is 1 (no fault). Only ever bites with the
 *            demand at 0 (mode 0 either way); the driving dump's 0x94112 =
 *            0x09 (TOE30 / TOE32 flagged while off) therefore reads 0 here.
 *   0x84DA0, 0x8B284  per-output diagnosis; 0x6865C reports (0x226 loop
 *            check, 0x238 head check — counted in s_ga_f238_n)
 *   0x72AE0  profiling hooks
 *   the 0x84890 slot array (slots 10..14 are computed straight into
 *            0xFFF94FAA / 0x94E22 — same values, no intermediate copy)
 *   the slot getters 0xA46AC / 0x1520B4 / 0x152F68 / 0x1520B8 (slots 10,
 *            12, 13, 14): compile-time 0 (drv_groupa.h). Slot 11 (0xA46B2,
 *            the park hold) is app/park.c's park_slot11_hold() — the
 *            ported 0xE88FA → 0xE8888 chain — bounded by DRV_GA_D11
 *            (2026-11-14, ~/tcu_sweep/range_logic_port.md)
 *   0x94FEA / 0x94FEB: written by the safety layer (0x141294 init 1 / 0xFE,
 *            0x141300 at run time) — stand-in constants = its init value and
 *            the driving dump (01 / 00): TOE30 forced off, TOE32 not
 *   type 3 (0x939AA demand, the 0x66974 model curve): no output has type 3
 *            (cal 0x1919B9 = 01 01 00 00 00) — refused at compile time
 *   the TOE30/31 cycle-match ISR 0x916CC / 0x6A680: not needed — with cal
 *            0x1918E7 = 0 it only clears TSRE and copies bookkeeping cells
 *            nothing reads; TIERE stays 0 (stage 0b)
 */
#define PLAT_TE_GA_WRITER 1             /* the one TU allowed plat_te_ga_* */
#include "bsw/drv/drv_groupa.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/sup/sup_cells.h"
#include "platform.h"

#if defined(TCU_RAM_APP)
/* the RAM application (64 KB image, ~1.8 KB free before M4) carries no
 * group-A law: its park.c owns TOE31 through plat_toe_out_set, and its
 * layout does not reserve the stock cells (linker_ram.ld). The weak list
 * hooks in sup_batch.c then resolve to nothing. */
void drv_ga_init(void) {}
#else
#include <string.h>

#if defined(__sh__) && !DRV_GA_CT
#error "drv_groupa: SH builds must take the compile-time demand path"
#endif

/* ── calibration (flash 0x19xxxx through the 0x66xxx tables) [O bytes] ── */
#define CAL_GA_ENABLE   0x0Bu       /* 0x1919B4 (pool 0x84BB0 → 0x66808): TOE30, TOE31, TOE32 */
#define CAL_GA_M14      0x0Fu       /* 0x1919B5 (0x6680C): 0x94114 test / 0x95005 window mask */
#define CAL_GA_M15      0x1Fu       /* 0x1919B6 (0x66810): 0x94115 test mask */
#define CAL_GA_MAXM     0x1Fu       /* 0x1919B7 (0x66828): > max → 1000 */
#define CAL_GA_MINM     0x1Fu       /* 0x1919B8 (0x6682C): < min → min (decode §2.1 said 0x01: wrong) */
static const uint8_t  CAL_GA_TYPE[DRV_GA_N] = { 1u, 1u, 0u, 0u, 0u };            /* 0x1919B9 (0x66830) */
static const uint8_t  CAL_GA_SRC[DRV_GA_N]  = { 0x00u, 0x00u, 0x00u, 0xFEu, 0x00u }; /* 0x1919BE (0x66834) */
static const uint8_t  CAL_GA_PULL[DRV_GA_N] = { 4u, 4u, 4u, 4u, 4u };            /* 0x1919C3 (0x66838) */
static const uint16_t CAL_GA_MAX[DRV_GA_N]  = { 950u, 950u, 950u, 950u, 950u };  /* 0x1914AE (pool 0x8B020) */
static const uint16_t CAL_GA_MIN[DRV_GA_N]  = { 50u, 50u, 50u, 50u, 50u };       /* 0x1914B8 (pool 0x8B018) */
#define CAL_GA_FF_THR   6700u       /* 0x191562 (0x66818) */
#define CAL_GA_FF_HYS   300u        /* 0x191560 (0x66814) */
#define CAL_GA_FE_THR   6700u       /* 0x191566 (0x66820) */
#define CAL_GA_FE_HYS   300u        /* 0x191564 (0x6681C) */
/* the type-1 curves (pointer table 0x6683C + 4k): { n, x[n] (1/16 °C), y[n] (mV) } */
static const int16_t CURVE_K0[5] = { 2, -640, 2240, 5835, 10355 };    /* 0x191572 */
static const int16_t CURVE_K1[5] = { 2, -480, 2240, 5400, 9380 };     /* 0x19157C */
static const int16_t CURVE_KX[5] = { 2, -480, 2240, 8000, 8000 };     /* 0x191586/90/9A */
static const int16_t *const CAL_GA_CURVE[DRV_GA_N] = {
    CURVE_K0, CURVE_K1, CURVE_KX, CURVE_KX, CURVE_KX,
};
/* 0x91500 */
#define CAL_FS_EN       0x1Fu       /* 0x1918E6 (pool 0x91740; also 0x91350's mask) */
#define CAL_FS_SKIP     0x00u       /* 0x1918E7 (pool 0x91748): bit 1 = TOE31 left to 0x6A680 */
static const uint16_t CAL_FS_CYC[DRV_GA_N] = { 2000u, 2000u, 2000u, 2000u, 2000u }; /* 0x1914C2 */
static const struct { uint8_t sc; uint16_t cycle; } GA_DESC[DRV_GA_N] = {         /* 0x40CB4 */
    { 0x0Cu, 2000u },   /* E30 PL5 */
    { 0x0Du, 2000u },   /* E31 PL6 park hold */
    { 0x0Fu, 2000u },   /* E33 PL8 */
    { 0x0Eu, 2000u },   /* E32 PL7 */
    { 0x0Au, 2000u },   /* E22 PL3 */
};
#define CAL_SLOT11_ALT  0x0000u     /* 0x191500: 0x5AA5 → slot 11 from 0x6A65E (not here) */
_Static_assert(CAL_SLOT11_ALT != 0x5AA5u, "0x6A65E slot-11 source not ported");
#define GA_TYPE_3_ABSENT ((1) /* cal 0x1919B9: no output of type 3 */)
_Static_assert(GA_TYPE_3_ABSENT, "type 3 (0x939AA / 0x66974 model) not ported");

/* the stand-ins for the safety layer's force-offs (drv_ga_init) */
#define GA_OFF10_STANDIN 1u         /* 0x141294 init / driving dump: TOE30 forced off */
#define GA_OFF13_STANDIN 0u         /* driving dump */

/* ── our witnesses (.drv_wit: low RAM, not .bss) ─────────────────────── */
#if defined(__sh__)
#define WIT __attribute__((section(".drv_wit")))
#else
#define WIT
#endif
static volatile uint16_t s_ga_wit_curve WIT;    /* curve mV at 0x94F9C (TOE31) */
static volatile uint16_t s_ga_wit_pm WIT;       /* ‰ the hold would get after pull-in */
static volatile uint16_t s_ga_wit_drld WIT;     /* the DRLD that ‰ gives */
static volatile uint16_t s_ga_wit_pad WIT;
static volatile uint32_t s_ga_guard_n WIT;      /* nonzero duty refused by the stage guard */
static volatile uint32_t s_ga_thold_n WIT;      /* stock full-on tests held (stage 0/1) */
static volatile uint32_t s_ga_ilock_n WIT;      /* full-on refused by the interlock (stage 2) */
static volatile uint32_t s_ga_f238_n WIT;       /* 0x91500 head check (stock fault 0x238) */

uint16_t drv_ga_wit_curve(void)  { return s_ga_wit_curve; }
uint16_t drv_ga_wit_permil(void) { return s_ga_wit_pm; }
uint16_t drv_ga_wit_drld(void)   { return s_ga_wit_drld; }
uint32_t drv_ga_guard_n(void)    { return s_ga_guard_n; }
uint32_t drv_ga_thold_n(void)    { return s_ga_thold_n; }
uint32_t drv_ga_ilock_n(void)    { return s_ga_ilock_n; }
uint32_t drv_ga_f238_n(void)     { return s_ga_f238_n; }

/* ── the demand source and the test release ──────────────────────────── */
#if DRV_GA_CT
static inline uint16_t ga_demand(uint8_t k)
{
    static const uint16_t T[DRV_GA_N] = DRV_GA_DEMAND_TABLE;
    if (k == 1u) {
        /* slot 11, the park hold: the ported park logic's demand (stock
         * getter 0xA46B2 → *0x89EC2 ← 0xE8888(id 3)), passed through only
         * when it equals the compile-time bound exactly (0 or 10000) */
        const uint16_t v = (T[1] != 0u) ? park_slot11_hold() : 0u;
        return (T[1] != 0u && v == T[1]) ? T[1] : 0u;
    }
    return k < DRV_GA_N ? T[k] : 0u;
}
#define GA_TESTS_RELEASED() ((DRV_GA_TESTS) != 0)
#else
static uint16_t s_host_demand[DRV_GA_N];
static bool s_host_tests;
static inline uint16_t ga_demand(uint8_t k) { return k < DRV_GA_N ? s_host_demand[k] : 0u; }
#define GA_TESTS_RELEASED() (s_host_tests)
void drv_ga_demand_set(uint8_t k, uint16_t demand)
{
    if (k < DRV_GA_N) {
        s_host_demand[k] = demand;
    }
}
void drv_ga_tests_set(bool released) { s_host_tests = released; }
#endif

/* ── 0x7B6D8, slots 10..14 (5 ms entry 0x7AB9C) ──────────────────────── */
void drv_ga_slots_tick(void)
{
    uint16_t slot[DRV_GA_N], comp[DRV_GA_N];

    for (uint8_t k = 0u; k < DRV_GA_N; k++) {
        slot[k] = ga_demand(k);                         /* getters 0xA46AC.. */
        comp[k] = (uint16_t)~slot[k];                   /* complement getters */
    }
    if (CELL_GA_OFF10 == 1u) {                          /* 0x6BDCA(0x94FEA, 0x94E62) */
        slot[0] = 0u;
        comp[0] = 0xFFFFu;
    }
    if (CELL_GA_OFF13 == 1u) {                          /* 0x6BDCA(0x94FEB, 0x94E63) */
        slot[3] = 0u;
        comp[3] = 0xFFFFu;
    }
    for (uint8_t k = 0u; k < DRV_GA_N; k++) {
        CELL_GA_DEMAND[k] = slot[k];
        CELL_GA_DEMAND_C[k] = comp[k];
    }
}

/* ── 0x8D42C via 0x886AC: the signed curve ───────────────────────────── */
int16_t drv_ga_curve_s16(const int16_t *tbl, int16_t x)
{
    const int16_t n = tbl[0];
    const int16_t *xs = &tbl[1];
    const int16_t *ys = &tbl[1 + n];
    float f;
    int32_t j;

    if ((int32_t)x <= (int32_t)xs[0]) {                 /* cmp/gt: not above x[0] */
        f = (float)ys[0];
    } else if ((int32_t)x >= (int32_t)xs[n - 1]) {
        f = (float)ys[n - 1];
    } else {
        j = 1;
        while ((int32_t)x >= (int32_t)xs[j]) {
            j++;
        }
        {
            const int32_t dx = (int32_t)x - (int32_t)xs[j - 1];
            const int32_t range = (int32_t)xs[j] - (int32_t)xs[j - 1];
            if (dx <= 0) {
                f = (float)ys[j - 1];
            } else {
                const float y0 = (float)ys[j - 1];
                f = (((float)ys[j] - y0) * (float)dx) / (float)range + y0;
            }
        }
    }
    f = f + (f < 0.0f ? -0.5f : 0.5f);                 /* 0x8D4AC / 0x8D4B0 */
    return (int16_t)(int32_t)f;                         /* ftrc, exts.w */
}

/* ── 0x9203C ─────────────────────────────────────────────────────────── */
uint16_t drv_ga_9203c(uint16_t permil, uint16_t cycle)
{
    if (permil >= 1000u) {
        return cycle;
    }
    /* float(cycle) x float(‰) x 0.001f (0x3A83126F), ftrc */
    return (uint16_t)(int32_t)(((float)cycle * (float)permil) * 0.001f);
}

/* 0x8AFC0: the output's supply voltage (mV) */
static uint16_t ga_src_mv(uint8_t k)
{
    const uint8_t s = CAL_GA_SRC[k];
    if (s == 0xFFu) {
        return CELL_GA_BATT_FF;
    }
    if (s == 0xFEu) {
        return CELL_SUPPLY_RAW;                         /* 0x94F86 */
    }
    return CELL_OUT_MV[s];                              /* 0x94ED0[s]: the rail node */
}

/* 0x85DC4: seq2 state of supervisor output p (p > 1 → 0) */
static uint8_t ga_seq2(uint8_t p)
{
    return p > 1u ? 0u : CELL_SEQ2_STATE[p];
}

/* 0x7BB00 stand-in (see the header): 1 = no fault */
static uint8_t ga_fault_class(uint8_t k) { (void)k; return 1u; }

/* the type-1 law after the pull-in, with 0x8ADF4's clamps (shared by the
 * law and the stage-0 witness) */
static uint16_t ga_type1(uint8_t k, uint16_t v, int16_t temp, uint16_t *curve)
{
    const uint16_t bit = (uint16_t)(1u << k);
    int32_t d;
    uint16_t u;

    if (v == 0u) {
        d = 1000;                                       /* 0x8AEA2 → 0x8AF3E */
        *curve = 0u;
    } else {
        const int16_t c = drv_ga_curve_s16(CAL_GA_CURVE[k], temp);
        *curve = (uint16_t)c;
        d = (int32_t)(((float)c * 1000.0f) / (float)v); /* 0x447A0000, ftrc */
    }
    u = (uint16_t)d;                                    /* extu.w */
    if (u < CAL_GA_MIN[k] && (CAL_GA_MINM & bit) != 0u) {
        d = (int32_t)CAL_GA_MIN[k];
        u = CAL_GA_MIN[k];
    }
    if (u > CAL_GA_MAX[k] && (CAL_GA_MAXM & bit) != 0u) {
        d = 1000;
    }
    if ((uint16_t)d > 1000u) {
        d = 1000;
    }
    return (uint16_t)d;
}

/* ── 0x8ADF4: mode → duty ‰ ──────────────────────────────────────────── */
static void ga_8adf4(uint8_t k)
{
    int8_t cd = (int8_t)CELL_GA_PULLIN[k];              /* read before the reload */
    const uint8_t mode = CELL_GA_MODE[k];
    const uint16_t dem = CELL_GA_GATED[k];
    uint16_t v, duty;

    CELL_GA_TEMP_COPY = (uint16_t)CELL_TMP;             /* 0x94FBE = 0x94F9C */
    v = ga_src_mv(k);                                   /* 0x8AFC0 */
    if (CELL_GA_PREV[k] == 0u) {                        /* re-arm the pull-in */
        cd = (int8_t)CAL_GA_PULL[k];
        CELL_GA_PULLIN[k] = (uint8_t)cd;
    }
    if (mode == 0u) {
        duty = 0u;                                      /* 0x8AFA8 */
    } else if (cd != 0 || mode == 2u) {
        duty = 1000u;                                   /* pull-in / test: full on */
    } else {
        const uint8_t type = CAL_GA_TYPE[k];
        if (type == 1u) {
            uint16_t curve;
            duty = ga_type1(k, v, (int16_t)CELL_GA_TEMP_COPY, &curve);
        } else {
            int32_t d = (type == 0u) ? 1000 : (type == 2u ? (int32_t)(int16_t)dem : 0);
            uint16_t u = (uint16_t)d;
            const uint16_t bit = (uint16_t)(1u << k);
            if (u < CAL_GA_MIN[k] && (CAL_GA_MINM & bit) != 0u) {
                d = (int32_t)CAL_GA_MIN[k];
                u = CAL_GA_MIN[k];
            }
            if (u > CAL_GA_MAX[k] && (CAL_GA_MAXM & bit) != 0u) {
                d = 1000;
            }
            if ((uint16_t)d > 1000u) {
                d = 1000;
            }
            duty = (uint16_t)d;
        }
    }
    CELL_GA_PREV[k] = duty;                             /* 0x93A66 */
    CELL_GA_DUTY[k] = duty;                             /* 0x9086A */
    CELL_GA_DUTY2[k] = duty;                            /* 0x94FB4 */
}

/* ── 0x849D4 (5 ms entry 0x7CC32, after the pin cut 0x85788) ─────────── */
void drv_ga_law_tick(void)
{
    uint8_t full = 0u;                                  /* @(0,r15)  → 0x94113 */
    uint8_t fault = 0u;                                 /* @(4,r15)  → 0x94112 */

    for (uint8_t k = 0u; k < DRV_GA_N; k++) {
        const uint8_t bit = (uint8_t)(1u << k);
        uint8_t dem_present = 0u;                       /* r8  → 0x94FF6 */
        uint8_t mode = 0u;                              /* r10 → 0x95000 */
        uint16_t gated = 0u;                            /* @(16,r15) → 0x94FC0 */

        if ((CAL_FS_EN & bit) == 0u || (CAL_GA_ENABLE & bit) == 0u) {   /* 0x91350, cal */
            CELL_GA_MODE_C[k] = 0u;                     /* 0x84CDA */
            CELL_GA_DEMPRES[k] = 0u;
            CELL_GA_GATED[k] = 0u;
            CELL_GA_MODE[k] = 0u;
            CELL_GA_DUTY2[k] = 0u;
            CELL_GA_DUTY[k] = 0u;
            continue;                                   /* (0x84DA0: not ported) */
        }
        {
            uint8_t src = CAL_GA_SRC[k];
            uint8_t s;
            bool flt;
            bool f;

            if ((int8_t)src >= 2) {                     /* cmp/ge #2 (signed: FE/FF stay) */
                src = 0u;
            }
            s = ga_seq2(src);                           /* 0x85DC4 */
            /* 0x94110 != 0 → 0x8B284(k): not ported */
            {
                const uint8_t fc = ga_fault_class(k);   /* 0x7BB00 */
                if (fc == 1u || fc == 0u) {
                    flt = false;
                    if (CELL_GA_DEMAND[k] != 0u) {
                        dem_present = 1u;
                    }
                } else {
                    flt = true;
                    fault = (uint8_t)(fault | bit);
                }
            }
            /* the supply gate (0x94111) */
            if (src == 0xFFu || src == 0xFEu) {
                const uint16_t v = src == 0xFFu ? CELL_GA_BATT_FF : CELL_SUPPLY_RAW;
                const uint16_t thr = src == 0xFFu ? CAL_GA_FF_THR : CAL_GA_FE_THR;
                const uint16_t hys = src == 0xFFu ? CAL_GA_FF_HYS : CAL_GA_FE_HYS;
                s = 0u;
                if (v >= (uint16_t)(thr + hys)) {
                    CELL_GA_ALLOWED = (uint8_t)(CELL_GA_ALLOWED | bit);
                }
                if (v < thr) {
                    CELL_GA_ALLOWED = (uint8_t)(CELL_GA_ALLOWED & (uint8_t)~bit);
                }
            } else if (s == (uint8_t)'U') {             /* THE 'U' GATE */
                CELL_GA_ALLOWED = (uint8_t)(CELL_GA_ALLOWED | bit);
            } else {
                CELL_GA_ALLOWED = (uint8_t)(CELL_GA_ALLOWED & (uint8_t)~bit);
            }
            if ((CELL_GA_ALLOWED & bit) != 0u && !flt) {
                gated = CELL_GA_DEMAND[k];              /* type 3 (0x939AA): absent */
                if (gated != 0u) {
                    mode = 1u;
                }
            }
            /* the full-on tests: seq2 0x11 / 0x16 / '3', or the sequencer's
             * pre-'L' window 0x95005 == 1 (mask 0x1919B5) */
            f = (CELL_LAD_95005 == 1u) && (CAL_GA_M14 & bit) != 0u;
            if (s == 0x11u || s == 0x16u || s == (uint8_t)'3' || f) {
                f = false;
                if (src != 0xFFu && src != 0xFEu) {
                    const uint8_t sb = (uint8_t)(1u << src);
                    if ((CELL_DIAG_94115 & sb) != 0u && (CAL_GA_M15 & bit) != 0u) {
                        f = true;
                    } else if ((CELL_LAD_94114 & sb) != 0u && (s == 0x11u || s == 0x16u) &&
                               (CAL_GA_M14 & bit) != 0u) {
                        f = true;
                    }
                }
                if (f && !GA_TESTS_RELEASED()) {
                    f = false;                          /* OUR HOLD (stages 0/1) */
                    s_ga_thold_n++;
                }
                if (f && !drv_rq_rail_quiet()) {
                    f = false;                          /* OUR INTERLOCK (as bus A) */
                    s_ga_ilock_n++;
                }
                if (f) {
                    mode = 2u;                          /* 0x84C94: FULL ON */
                    full = (uint8_t)(full | bit);
                } else {
                    mode = 0u;
                }
            }
            CELL_GA_FAULT = fault;                      /* 0x84CA8 */
            CELL_GA_MODE[k] = mode;
            CELL_GA_FULL = full;
            CELL_GA_GATED[k] = gated;
            CELL_GA_DEMPRES[k] = dem_present;
            CELL_GA_MODE_C[k] = mode;
            ga_8adf4(k);
        }
        /* 0x84DA0(k), the 0x226 loop check: not ported */
    }
    CELL_GA_FIRST = 1u;                                 /* 0x84D58 */

    /* the stage-0 read-out (ours): what TOE31 would get after its pull-in */
    {
        uint16_t c;
        s_ga_wit_pm = ga_type1(1u, ga_src_mv(1u), (int16_t)CELL_TMP, &c);
        s_ga_wit_curve = c;
        s_ga_wit_drld = drv_ga_9203c(s_ga_wit_pm, CAL_FS_CYC[1]);
    }
}

/* the stage guard (ours): a nonzero duty only on the outputs the stage
 * allows, never above the cycle (= the stock law's 1000 ‰) */
static uint16_t ga_guard(uint8_t k, uint16_t drld, uint16_t cycle)
{
#if DRV_GA_CT
    if (drld != 0u && (DRV_GA_ALLOWED & (1u << k)) == 0u) {
        s_ga_guard_n++;
        return 0u;
    }
#else
    (void)k;
#endif
    if (drld > cycle) {
        s_ga_guard_n++;
        return cycle;
    }
    return drld;
}

/* ── 0x91500 (the F stage 0x8EE30, after 0x901A0) ─────────────────────── */
void drv_ga_fstage(void)
{
    uint8_t en = CAL_FS_EN;                             /* @r15, shifted per output */

    CELL_GA_FS_CYC[0] = CAL_FS_CYC[0];                  /* bytes: 1 << k, bld #0 → k = 0 only */
    for (uint8_t k = 0u; k < DRV_GA_N; k++) {
        const uint8_t bit = (uint8_t)(1u << k);
        const uint8_t sc = GA_DESC[k].sc;

        if (((CAL_FS_SKIP & 2u) & bit) == 0u) {
            CELL_GA_FS_DUTY[k] = (en & 1u) != 0u ? CELL_GA_DUTY[k] : 0u;
            if ((bit & 3u) != 0u) {                     /* TOE30 / TOE31 */
                const uint16_t want = CELL_GA_FS_CYC[k];
                const uint16_t now = CELL_GA_FS_CYCNOW[k];
                if (want == now) {
                    plat_te_ga_drld(sc, ga_guard(k, drv_ga_9203c(CELL_GA_FS_DUTY[k], now), now));
                } else {                                /* cycle change */
                    plat_te_ga_cycle(sc, ga_guard(k, drv_ga_9203c(CELL_GA_FS_DUTY[k], want), want),
                                     want);
                }
            } else {
                const uint16_t cyc = GA_DESC[k].cycle;
                if ((en & 1u) != 0u &&
                    drv_ga_9203c(CELL_GA_FS_PREV[k], cyc) != plat_te_ga_dtr(sc)) {
                    s_ga_f238_n++;                      /* stock 0x6865C(0x238) */
                }
                plat_te_ga_drld(sc, ga_guard(k, drv_ga_9203c(CELL_GA_FS_DUTY[k], cyc), cyc));
                CELL_GA_FS_PREV[k] = CELL_GA_FS_DUTY[k];
            }
        }
        en = (uint8_t)(en >> 1);
    }
}

/* ── init ────────────────────────────────────────────────────────────── */
void drv_ga_init(void)
{
    memset((void *)(uintptr_t)0xFFF93A66u, 0, 0x0Au);   /* .drv_ga1 */
    for (uint8_t k = 0u; k < DRV_GA_N; k++) {
        CELL_GA_FS_CYC[k] = CAL_FS_CYC[k];              /* 0x9136E: 0x90888 = cal 0x1914C2 */
        CELL_GA_FS_CYCNOW[k] = GA_DESC[k].cycle;        /*          0x90892 = 0x40CB4 cycle */
        CELL_GA_FS_PREV[k] = 0u;
        CELL_GA_FS_DUTY[k] = 0u;
        CELL_GA_DUTY[k] = 0u;
#if !DRV_GA_CT
        s_host_demand[k] = 0u;
#endif
    }
#if !DRV_GA_CT
    s_host_tests = false;
#endif
    CELL_GA_OFF10 = GA_OFF10_STANDIN;                   /* safety layer 0x141294 */
    CELL_GA_OFF10_C = (uint8_t)~GA_OFF10_STANDIN;
    CELL_GA_OFF13 = GA_OFF13_STANDIN;
    CELL_GA_OFF13_C = (uint8_t)~GA_OFF13_STANDIN;
    s_ga_wit_curve = 0u;
    s_ga_wit_pm = 0u;
    s_ga_wit_drld = 0u;
    s_ga_wit_pad = 0u;
    s_ga_guard_n = 0u;
    s_ga_thold_n = 0u;
    s_ga_ilock_n = 0u;
    s_ga_f238_n = 0u;
}
#endif /* !TCU_RAM_APP */
