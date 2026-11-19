/* drv_temp.c — the hold-law temperature 0xFFF94F9C (drv_temp.h). Ported
 * from the bytes of 0x88A50..0x88ABC, 0x8D57C..0x8D5B8, 0x887D8..0x88A1E,
 * 0x8D5BA..0x8D88C, 0x8D4B4..0x8D548, 0x8ED20..0x8EDDC (sh-elf-objdump of
 * the pristine image; the decompiles checked against them); calibration
 * through the 0x66xxx pointer tables at index 0x90AB8 = 0.
 * ~/tcu_sweep/groupA_park_port.md §3.
 *
 * Driving dump reproduction (test_groupa): AN8 = 3031 / 3033 mV, AN1 = 164
 * mV → diff 0x0B35, ratio 0x32C1, flags 0x0880, 0x93AA0 = 410 / 411 →
 * 0x94F9C = 0x019B (25.7 °C), status 8.
 *
 * NOT PORTED (named stand-ins / deviations):
 *   0x72AE0 profiling; the 0x6BDFE / 0x6BDCA pair checks (fault 0x221 on a
 *   torn complement) whose values 0x887D8 discards; the DTC report
 *   0x7CA24 inside 0x7BF8C and the registration 0x7BFF0(0x1FC) (the
 *   fault-code tail and 0x7BF8C's flags ARE ported, 2026-11-17; 0x6F594
 *   fault memory ready = stand-in 1);
 *   0xA0FC4(0x65) (fault path 0x65 active → 0x94F9C from the substitute) —
 *   stand-in "not active" (the dump's 0x94F9C == 0x94F98);
 *   0x9393A / 0x940FC (the substitute temperature and its status) are not
 *   produced here (0): the substitution branch can never be taken;
 *   0xFFF84796 (the coding byte copied to 0x94157) — stand-in 0 (dump);
 *   the curve tables of variants 0 and 1 are identical in the image, as are
 *   the plausibility limits — one table each here.
 */
#include "bsw/drv/drv_temp.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_tsel.h"       /* DRV_TSEL_FLAGS (stage 2b) */
#include "bsw/sup/sup_cells.h"
#include "platform.h"

#if defined(TCU_RAM_APP)
/* not in the RAM application (see drv_groupa.c): no AN table there either */
void drv_temp_init(void) {}
#else

/* ── calibration [O bytes] ───────────────────────────────────────────── */
#define CAL_TMP_VAR_DEF     0u          /* 0x1919FC (0x66A7C) default variant */
static const uint8_t CAL_TMP_VAR_TAB[5][2] = {  /* 0x1916E6 (0x66A78): {code, variant} */
    { 0u, 0u }, { 1u, 0u }, { 2u, 0u }, { 3u, 0u }, { 4u, 0u },
};
#define CAL_TMP_F94155      0u          /* 0x1919FD (0x66A80) */
#define CAL_TMP_AN_SENS     8u          /* 0x1914A2[0] (pool 0x8D818) */
#define CAL_TMP_AN_REF      1u          /* 0x1914A2[1] */
#define CAL_TMP_REF_MIN     0x0005u     /* 0x191384 (0x66AA8) */
#define CAL_TMP_REF_MAX     0x01A7u     /* 0x191380 (0x66AA4) */
#define CAL_TMP_SENS_MIN    0x01FCu     /* 0x19137C (0x66AA0) */
#define CAL_TMP_SENS_MAX    0x125Cu     /* 0x191378 (0x66A9C) */
#define CAL_TMP_DIFF_MIN    0x0087u     /* 0x19138C (0x66AB0) */
#define CAL_TMP_DIFF_MAX    0x11A0u     /* 0x191388 (0x66AAC) */
#define CAL_TMP_RATIO_MIN   0x2710u     /* 0x1912EC (0x66A74) */
#define CAL_TMP_RATIO_MAX   0x3C8Cu     /* 0x1912E8 (0x66A70) */
#define CAL_TMP_DDIFF_MAX   0x0064u     /* 0x191374 (0x66A98) */
#define CAL_TMP_HOLD        0x0014u     /* 0x1916F2 (0x66A90): 20 x 5 ms */
#define CAL_TMP_DEFAULT     0x0780      /* 0x1916F0 (0x66A8C): 1920 = 120 °C */
#define CAL_TMP_START       0x02u       /* 0x1919FE (0x66A94) */
#define CAL_TMP_SRCSEL      0x04u       /* 0x191A01 (0x66B58) */
#define CAL_TMP_SUB_B1      0x0000      /* 0x191736 (0x66B5C) */
#define CAL_TMP_SUB_B2      0x0000      /* 0x191738 (0x66B60) */
#define TMP_CODE_STANDIN    0u          /* 0xFFF84796 (driving dump 00) */
/* the sensor curve (0x66A84 + 4·variant → 0x1912F0, = 0x191332): x = AN8
 * mV, y = 1/16 °C + 0x8000 (170 °C at 538 mV .. -40 °C at 4494 mV) */
static const uint16_t CURVE_TFT[33] = {
    16u,
    538u, 568u, 653u, 747u, 880u, 1069u, 1334u, 1692u,
    2151u, 2687u, 3241u, 3735u, 4110u, 4355u, 4457u, 4494u,
    35488u, 35328u, 35008u, 34768u, 34528u, 34288u, 34048u, 33808u,
    33568u, 33328u, 33088u, 32848u, 32608u, 32368u, 32208u, 32128u,
};

/* ── 0x8ED60 via 0x8ED20: the unsigned curve ─────────────────────────── */
uint16_t drv_temp_curve_u16(const uint16_t *tbl, uint16_t x)
{
    const uint16_t n = tbl[0];
    const uint16_t *xs = &tbl[1];
    const uint16_t *ys = &tbl[1u + n];
    float f;

    if ((int32_t)x <= (int32_t)xs[0]) {
        f = (float)ys[0];
    } else if ((int32_t)x >= (int32_t)xs[n - 1u]) {
        f = (float)ys[n - 1u];
    } else {
        uint32_t j = 1u;
        while ((int32_t)x >= (int32_t)xs[j]) {
            j++;
        }
        {
            const int32_t dx = (int32_t)x - (int32_t)xs[j - 1u];
            const int32_t range = (int32_t)xs[j] - (int32_t)xs[j - 1u];
            if (dx <= 0) {
                f = (float)ys[j - 1u];
            } else {
                const float y0 = (float)ys[j - 1u];
                f = (((float)ys[j] - y0) * (float)dx) / (float)range + y0;
            }
        }
    }
    return (uint16_t)(int32_t)(f + 0.5f);               /* 0x8EDE0, ftrc, extu.w */
}

static void pair16(volatile int16_t *v, volatile uint16_t *c, int16_t x)   /* 0x6BD6E */
{
    *v = x;
    *c = (uint16_t)~(uint16_t)x;
}

static void pair8(volatile uint8_t *v, volatile uint8_t *c, uint8_t x)      /* 0x6BD68 */
{
    *v = x;
    *c = (uint8_t)~x;
}

/* ── 0x8D5BA: the fluid-temperature acquisition ──────────────────────── */
static void temp_8d5ba(void)
{
    uint16_t fl = 0x0400u;                              /* r13 */
    uint8_t var = CAL_TMP_VAR_DEF;                      /* r5 */
    const uint8_t code = CELL_TMP_CODE;                 /* r10 */
    uint16_t sens, ref, diff, ref10, d16, ratio;
    int32_t delta, r2;

    CELL_TMP_VAR_RAW = var;                             /* 0x94158 */
    for (uint32_t i = 0u; i < 5u; i++) {
        if (CAL_TMP_VAR_TAB[i][0] == code) {
            var = CAL_TMP_VAR_TAB[i][1];
            fl = (uint16_t)(fl & 0xFBFFu);
            CELL_TMP_VAR_RAW = var;
        }
    }
    if (var < 2u) {                                     /* cmp/ge #2 */
        CELL_TMP_VARIANT = var;
    } else {
        fl = (uint16_t)(fl | 0x0400u);
        var = CAL_TMP_VAR_DEF;
        CELL_TMP_VARIANT = var;
    }
    (void)var;                                          /* variants 0/1: identical tables */
    /* 0x94155 = cal != 0 && (flag 0x400 || code == 0xFF) */
    CELL_TMP_94155 = (CAL_TMP_F94155 != 0u &&
                      ((fl & 0x0400u) != 0u || code == 0xFFu)) ? 1u : 0u;
    ref = CELL_AN_MV(CAL_TMP_AN_REF);
    CELL_TMP_REF = ref;                                 /* 0x93AA6 */
    sens = CELL_AN_MV(CAL_TMP_AN_SENS);
    CELL_TMP_SENS = sens;                               /* 0x93AA4 */
    diff = (sens > ref) ? (uint16_t)(sens - ref) : 0u;  /* cmp/gt on extu values */
    CELL_TMP_DIFF = diff;                               /* 0x93AA8 */
    d16 = (uint16_t)(diff - CELL_TMP_DIFF_PREV);
    CELL_TMP_DDIFF = d16;                               /* 0x93AA2 */
    delta = (int16_t)d16;
    if (delta < 1) {                                    /* cmp/pl */
        delta = -delta;
    }
    CELL_TMP_DIFF_PREV = diff;                          /* 0x93AAA (delay slot: always) */
    ref10 = ref < 10u ? 10u : ref;
    r2 = 5000 - (int32_t)(int16_t)diff;
    r2 = (int16_t)r2;                                   /* exts.w */
    if (r2 < 1) {
        r2 = 0;
    }
    {
        /* float(r2) x 1000.0f / float(ref10), ftrc, cap 40000 (cmp/hs) */
        uint32_t q = (uint32_t)(int32_t)(((float)r2 * 1000.0f) / (float)ref10);
        if (q >= 40000u) {
            q = 40000u;
        }
        ratio = (uint16_t)q;
        CELL_TMP_RATIO = ratio;                         /* 0x93A9C */
    }
    if (ref < CAL_TMP_REF_MIN)   fl = (uint16_t)(fl | 0x01u);
    if (ref > CAL_TMP_REF_MAX)   fl = (uint16_t)(fl | 0x02u);
    if (sens < CAL_TMP_SENS_MIN) fl = (uint16_t)(fl | 0x04u);
    if (sens > CAL_TMP_SENS_MAX) fl = (uint16_t)(fl | 0x08u);
    if ((int32_t)(int16_t)diff < (int32_t)CAL_TMP_DIFF_MIN) fl = (uint16_t)(fl | 0x10u);
    if ((int32_t)(int16_t)diff > (int32_t)CAL_TMP_DIFF_MAX) fl = (uint16_t)(fl | 0x20u);
    if (ratio < CAL_TMP_RATIO_MIN ||
        (ratio > CAL_TMP_RATIO_MAX && CAL_TMP_RATIO_MAX != 0u)) {
        fl = (uint16_t)(fl | 0x40u);
    }
    if ((fl & 0x7Fu) == 0u) {
        fl = (uint16_t)(fl | 0x80u);                    /* plausible */
        if ((uint16_t)delta > CAL_TMP_DDIFF_MAX) {
            fl = (uint16_t)(fl | 0x100u);               /* ... but jumping */
        }
    }
    if (CELL_CNT_TMP_START == 0u) {
        fl = (uint16_t)(fl | 0x800u);
    }
    if ((fl & 0x180u) == 0x80u) {
        CELL_TMP_ACQ = (int16_t)(drv_temp_curve_u16(CURVE_TFT, sens) - 0x8000u);
        CELL_TMP_ACQ_ST = 8u;
        CELL_CNT_TMP_HOLD = CAL_TMP_HOLD;
    } else if ((fl & 0x800u) != 0u) {
        if (CELL_CNT_TMP_HOLD != 0u) {
            CELL_TMP_ACQ_ST = 7u;                       /* hold the last value */
        } else {
            CELL_TMP_ACQ_ST = 3u;
            fl = (uint16_t)(fl | 0x200u);
            CELL_TMP_ACQ = CAL_TMP_DEFAULT;             /* 120 °C */
        }
    }
    CELL_TMP_FLAGS = fl;                                /* 0x93A9E */
}

/* ── 0x8D4B4 ─────────────────────────────────────────────────────────── */
static void temp_8d4b4(void)
{
    /* 0xA0FC4(0x65) stand-in: fault path 0x65 not active */
    CELL_TMP = CELL_TMP_SEL;
    CELL_TMP_C = CELL_TMP_SEL_C;
    CELL_TMP_ST = CELL_TMP_SEL_ST;
    CELL_TMP_ST_C = CELL_TMP_SEL_ST_C;
}

static uint8_t temp_byte(int16_t t)                     /* (t + 8) >> 4 + 40, 0..255 */
{
    const int32_t b = (int16_t)((((int32_t)t + 8) >> 4) + 40);
    if (b <= 0) {
        return 0u;
    }
    return b >= 255 ? 255u : (uint8_t)b;
}

/* ── 0x7BF70: the fault word's bits → the published flag byte ─────────── */
static uint8_t temp_7bf70(uint16_t w)
{
    uint8_t r = (uint8_t)((w >> 3) & 1u);               /* bld #3; movt */
    if ((w & 0x0004u) != 0u) {                          /* bld #2 → bset #1 */
        r = (uint8_t)(r | 0x02u);
    }
    if ((w & 0x0010u) != 0u) {                          /* bld #4 → bset #2 */
        r = (uint8_t)(r | 0x04u);
    }
    if ((w & 0x01E0u) != 0u) {                          /* #120 << 2 → bset #7 */
        r = (uint8_t)(r | 0x80u);
    }
    return r;
}

/* ── 0x7BF8C(mask, value): the fluid sensor's fault word 0x93914 and the
 * flags 0x940E7 (tested) / 0x940E8 (failed). The DTC report 0x7CA24(2440,
 * 0, mask, value) at its head is NOT ported (no fault memory). ───────── */
static void temp_7bf8c(uint16_t mask, uint16_t value)
{
    uint16_t w = (uint16_t)((CELL_TMP_FWORD & (uint16_t)~mask) | (value & mask));
    uint8_t mm;
    uint8_t old;

    CELL_TMP_FWORD = w;                                 /* 0x7BFBA (delay slot) */
    mm = temp_7bf70(mask);
    old = CELL_TMP_PUB_E8;
    CELL_TMP_PUB_E7 = mm;                               /* 0x7BFC8 */
    old = (uint8_t)(old & (uint8_t)~mm);
    if ((mask & w) == 0u) {                             /* 0x7BFCC tst r12,r4 */
        CELL_TMP_PUB_E8 = old;                          /* 0x7BFE8 */
    } else {
        CELL_TMP_PUB_E8 = (uint8_t)((temp_7bf70(w) & mm) | old);
    }
}

/* ── 0x887D8's tail @0x8896A..0x88A10: the fault code from the 0x8D5BA
 * plausibility bits, then (fault memory ready, 0x6F594 — stand-in 1, as
 * sup_ladder.c S1) 0x7BF8C(0x1FC, code) ──────────────────────────────── */
#define STANDIN_FAULTMEM_READY  1u                      /* 0x6F594 */
/* the fault code / mask (0x887D8's own; inside .pks_eval 0x93AC4..0x93B03,
 * cleared by drv_pksens_init, rewritten every pass) */
#define CELL_TMP_FCODE      (*(volatile uint16_t *)0xFFF93AE8u)
#define CELL_TMP_FMASK      (*(volatile uint16_t *)0xFFF93AEAu)
static void temp_fault_tail(void)
{
    const uint16_t fl = CELL_TMP_FLAGS;                 /* 0x93A9E */
    uint16_t code = 0u;                                 /* r12 = 0 (0x887E4) */
    uint16_t m;

    m = (uint16_t)(fl & 0x0809u);
    if (m == 0x0808u) {
        code = 0x04u;
    } else if (m == 0x0801u) {
        code = 0x08u;
    } else if (m == 0x0809u) {
        code = 0x10u;
    } else if ((uint16_t)(fl & 0x0828u) == 0x0820u) {
        code = 0x10u;
    } else if ((uint16_t)(fl & 0x087Fu) == 0x0840u) {
        code = 0x20u;
    } else if ((uint16_t)(fl & 0x087Fu) == 0x0820u) {  /* unreachable (0x828 test above) — as the bytes */
        code = 0x40u;
    } else if ((fl & 0x0800u) != 0u && (fl & 0x007Fu) != 0u) {
        code = 0x80u;
    }
    if (CELL_TMP_94155 != 0u) {                         /* 0x889D8 */
        code = (uint16_t)(code | 0x0100u);
    }
    CELL_TMP_FMASK = 0x01FCu;                           /* #127 << 2 */
    CELL_TMP_FCODE = code;
    if (STANDIN_FAULTMEM_READY != 0u) {
        temp_7bf8c(CELL_TMP_FMASK, CELL_TMP_FCODE);
    }
}

/* ── 0x887D8 ─────────────────────────────────────────────────────────── */
void drv_temp_tick(void)
{
    uint8_t f;
    int16_t v;
    uint8_t st;
    uint8_t src;

    temp_8d5ba();
    /* the pair reads of 0x94F9A / 0x94FEF / 0x94F98 / 0x94FEE / 0x94F9C /
     * 0x94FF0 (0x6BDFE / 0x6BDCA): values discarded, checks not ported */
    pair16(&CELL_TMP_RAW, &CELL_TMP_RAW_C, CELL_TMP_ACQ);
    pair8(&CELL_TMP_RAW_ST, &CELL_TMP_RAW_ST_C, CELL_TMP_ACQ_ST);
    f = (uint8_t)(CELL_TMP_ACQ_ST >= 7u ? 1u : 0u);     /* cmp/hs, movt */
    if (CELL_TMP_940FC >= 7u) {
        f = (uint8_t)(f | 4u);
    }
    CELL_TMP_9416E = f;
    v = CELL_TMP_ACQ;
    st = CELL_TMP_ACQ_ST;
    src = 1u;
    if ((f & 1u) == 0u) {
        if ((f & 2u) != 0u && (CAL_TMP_SRCSEL & 2u) != 0u) {      /* bit 1: never set */
            v = CAL_TMP_SUB_B1;
            st = 6u;
            src = 2u;
        } else if ((f & 4u) != 0u && (CAL_TMP_SRCSEL & 4u) != 0u) {
            v = (int16_t)(CAL_TMP_SUB_B2 + CELL_TMP_9393A);
            st = 6u;
            src = 4u;
        }
    }
    pair16(&CELL_TMP_SEL, &CELL_TMP_SEL_C, v);
    pair8(&CELL_TMP_SEL_ST, &CELL_TMP_SEL_ST_C, st);
    CELL_TMP_9416F = src;
    temp_8d4b4();
    CELL_TMP_BYTE_SEL = temp_byte(CELL_TMP_SEL);
    CELL_TMP_BYTE = temp_byte(CELL_TMP);
    temp_fault_tail();
}

/* ── 0x7C100 (the 0x7AB48 poll group, after 0x7B150): the published bytes
 * the safety layer gathers (0x7BF40 / 0x7BF50 / 0x7BF60) ──────────────── */
void drv_temp_pub_tick(void)
{
    CELL_TMP_PUB_E6 = (CELL_TMP_PUB_E8 != 0u) ? 0xFFu : CELL_TMP_BYTE_SEL;   /* 0x7C100..0x7C11E */
    CELL_TMP_PUB_E0 = (CELL_TMP_PUB_E2 != 0u) ? 0xFFu : CELL_TMP_940FE;      /* 0x7C120..0x7C13E */
    CELL_TMP_PUB_E3 = (CELL_TMP_PUB_E5 != 0u) ? 0xFFu : CELL_TMP_94101;      /* 0x7C140..0x7C158 */
}

/* ── 0x8F70C's ADC_A part (reply proc head) ──────────────────────────── */
void drv_temp_an_tick(void)
{
    plat_adcmux_an_convert();
}

/* ── 0x88A50 → 0x8D57C ───────────────────────────────────────────────── */
void drv_temp_init(void)
{
    /* .drv_tmp1 / .drv_tmp2 (ours to clear; the rest is sup_cells_init's) */
    for (uint32_t a = 0xFFF93A9Cu; a < 0xFFF93AACu; a++) {
        *(volatile uint8_t *)(uintptr_t)a = 0u;
    }
    CELL_TMP_9416E = 0u;
    CELL_TMP_9416F = 0u;
    /* 0x7BFF0(0x1FC): the DTC registration, not ported. The fault word and
     * the published bytes start 0 (the stock's .bss) */
    CELL_TMP_FWORD = 0u;
    for (uint32_t a = 0xFFF940E0u; a <= 0xFFF940E8u; a++) {
        *(volatile uint8_t *)(uintptr_t)a = 0u;
    }
#if !DRV_TSEL_FLAGS
    /* STAND-IN (ours, stage 2a and every build without TSEL_LIVE_FLAGS):
     * the fault flags 0x940E5 (board, 0x88DA8 → 0x7C0B6) and 0x940E2 (die,
     * 0x88BD0 → 0x7C01E) are held nonzero ("faulted"), so 0x7C100 publishes
     * 0xFF for both bytes and the safety layer's conditioning takes its own
     * both-invalid substitute (cal 0x195DB8 = 0x82, 90 °C) if the fluid
     * sensor fails — never a byte from a source the build does not run */
    CELL_TMP_PUB_E2 = TMP_STANDIN_SRC_FAULTED;
    CELL_TMP_PUB_E5 = TMP_STANDIN_SRC_FAULTED;
#else
    /* STAGE 2b (TSEL_LIVE_FLAGS, ~/tcu_sweep/temp_stage2b_port.md): the
     * flags are the shadow's 0x7C01E / 0x7C0B6 results, written by the
     * temperature hub (bsw/drv/drv_tsel.c); here they start 0, the stock's
     * cleared RAM (no initialised-data entry, and the init hub's
     * 0x7C086(0x62) / 0x7C0E8(0x160) never report) — the loop above */
#endif
    CELL_CNT_TMP_HOLD = CAL_TMP_HOLD;                   /* 0x8D57C */
    CELL_CNT_TMP_START = CAL_TMP_START;
    CELL_TMP_CODE = TMP_CODE_STANDIN;
    CELL_TMP_ACQ = CAL_TMP_DEFAULT;
    pair16(&CELL_TMP_RAW, &CELL_TMP_RAW_C, 0);
    pair8(&CELL_TMP_RAW_ST, &CELL_TMP_RAW_ST_C, 0u);
    pair16(&CELL_TMP_SEL, &CELL_TMP_SEL_C, 0);
    pair8(&CELL_TMP_SEL_ST, &CELL_TMP_SEL_ST_C, 0u);
    temp_8d4b4();
}
#endif /* !TCU_RAM_APP */
