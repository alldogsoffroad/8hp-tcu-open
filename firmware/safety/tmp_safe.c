/* tmp_safe.c — the safety layer's temperature objects 0xFFF8E998 / 0xFFF8ED20
 * (tmp_safe.h), the integer channel only, ported from the bytes
 * (sh-elf-objdump of the pristine image; the Ghidra decompiles as aids):
 *   0x151DD8  step-table lookup              0x14ACF0  slew limiter
 *   0x14ADB2  jump flag                      0x14B34A  invalid flags
 *   0x14A4F0  ED20 (re)initialisation        0x14A6E8  ED20 cycle (subset)
 *   0x1401C2  E998 initialisation (subset)   0x140416  E998 direct conditioning
 *   0x1405FA  E998 cyclic conditioning       0x1402F0  its caller (subset)
 *   0x151C9C  the boot gather + conditioning 0x1517E0 / 0x132DCE gathers
 * ~/tcu_sweep/temp_byte_live_port.md.
 */
#include "safety/tmp_safe.h"
#include "bsw/drv/drv_cells.h"

#if !defined(TCU_RAM_APP)   /* not in the RAM application (app/tmp_chain.c stubs) */

#define E8(off)     TS_U8(TS_E998, (off))
#define D8(off)     TS_U8(TS_ED20, (off))

/* 0x191E44 [O image]: n = 6; x = 40 50 70 105 140 160 (°C + 40);
 * y = 3 3 6 6 6 3 (byte × 100 per 100 ms pass) */
const uint8_t TS_STEP_TBL[16] = {
    0x00u, 0x00u, 0x00u, 0x06u,
    0x28u, 0x32u, 0x46u, 0x69u, 0x8Cu, 0xA0u,
    0x03u, 0x03u, 0x06u, 0x06u, 0x06u, 0x03u,
};

/* ── 0x151DD8(tbl, x): u32 n, x[n], y[n]; the linear interpolation on the
 * u8 grid (0 below x[0], y[n-1] from x[n-1] on) ─────────────────────── */
uint8_t tmp_safe_151dd8(const uint8_t *tbl, uint8_t x)
{
    const uint32_t n = ((uint32_t)tbl[0] << 24) | ((uint32_t)tbl[1] << 16) |
                       ((uint32_t)tbl[2] << 8) | (uint32_t)tbl[3];
    const uint8_t *xs = &tbl[4];
    int32_t idx = 0;                                    /* r6 */
    int32_t frac = 0;                                   /* r7 */
    int32_t span = 0;                                   /* r4 */
    uint8_t y0;

    if ((int32_t)x > (int32_t)xs[0]) {                  /* cmp/gt r6,r13 */
        if ((int32_t)x >= (int32_t)xs[n - 1u]) {        /* 0x151E00 cmp/ge */
            span = 1;
            idx = (int32_t)n - 2;
            frac = 1;
        } else {
            uint32_t j = 0u;
            while ((int32_t)x > (int32_t)xs[j]) {       /* 0x151E0E */
                j++;
            }
            span = (int32_t)xs[j] - (int32_t)xs[j - 1u];
            frac = (int32_t)x - (int32_t)xs[j - 1u];
            idx = (int32_t)j - 1;
        }
    }
    y0 = xs[(int32_t)n + (int16_t)idx];                 /* 0x151E2A..0x151E34 */
    if ((uint8_t)frac == 0u) {
        return y0;
    }
    {
        const uint8_t y1 = xs[(int32_t)n + (int16_t)idx + 1];
        const int32_t f = (int32_t)(uint8_t)frac;       /* r13 */
        const int32_t s = (int32_t)(uint8_t)span;       /* r5 */
        if ((int32_t)y1 >= (int32_t)y0) {               /* 0x151E40 cmp/ge */
            const int32_t d = (int32_t)(uint16_t)(y1 - y0);
            return (uint8_t)((d * f) / s + (int32_t)y0);
        } else {
            const int32_t d = (int32_t)(uint16_t)(y0 - y1);
            return (uint8_t)(y0 - (uint8_t)((d * f) / s));
        }
    }
}

/* ── 0x14ADB2: the fluid byte's jump flag +0xC8 against the last accepted
 * byte +0xC6 (set above 20, cleared at or below 5); +0xC6 follows while
 * clear ─────────────────────────────────────────────────────────────── */
static void d_14adb2(void)
{
    uint8_t j = D8(TS_D_JUMP);                          /* r14 */
    const int32_t in = (int32_t)D8(TS_D_BC);            /* r6 */
    const int32_t last = (int32_t)D8(TS_D_C6);          /* r1 */
    const int32_t d = last > in ? last - in : in - last;

    if (j == 0u) {
        if (d > (int32_t)TS_CAL_JUMP_SET) {             /* cmp/gt r7,r1 */
            D8(TS_D_JUMP) = 1u;
            j = D8(TS_D_JUMP);
        }
    } else {
        if (!(d > (int32_t)TS_CAL_JUMP_CLR)) {          /* cmp/gt r5,r1; bt keep */
            D8(TS_D_JUMP) = 0u;
            j = D8(TS_D_JUMP);
        }
    }
    if (j == 0u) {
        D8(TS_D_C6) = D8(TS_D_BC);
    }
}

/* ── 0x14ACF0: the slew limiter. +0xC0 = byte × 100 moves toward the input
 * by at most the table step per pass; reset to the input on the flag's
 * release (+0xBE set, +0xBD clear) or on the jump flag's release; +0xC2 =
 * +0xC0 / 100 (truncated) ─────────────────────────────────────────── */
static void d_14acf0(void)
{
    volatile uint16_t *st = &TS_U16(TS_ED20, TS_D_STATE);
    int32_t d;
    int32_t step;
    int32_t c0;
    uint16_t out;

    if ((D8(TS_D_BE) != 0u && D8(TS_D_BD) == 0u) ||
        (D8(TS_D_JUMP) == 0u && D8(TS_D_JUMP_P) != 0u)) {
        *st = (uint16_t)((uint32_t)D8(TS_D_BC) * 100u);  /* 0x14AD1C mulr, mov.w */
    }
    D8(TS_D_JUMP_P) = D8(TS_D_JUMP);                     /* 0x14AD2E..0x14AD32 */
    D8(TS_D_BE) = D8(TS_D_BD);                           /* 0x14AD36..0x14AD3A */
    d = (int16_t)(uint16_t)((uint32_t)D8(TS_D_BC) * 100u - (uint32_t)*st);  /* mov.w @(4,r15) */
    step = (int32_t)tmp_safe_151dd8(TS_STEP_TBL, D8(TS_D_BC));
    c0 = (int16_t)*st;                                   /* mov.w @(192,r13): sign-extended */
    out = (uint16_t)c0;
    if (d > 0) {                                         /* cmp/pl */
        c0 = d >= step ? c0 + step : c0 + d;
        *st = (uint16_t)c0;
        out = *st;
    } else if (d < 0) {                                  /* cmp/pz → 0x14AD9A on d == 0 */
        const int32_t nd = -d;
        const int32_t c0u = (int32_t)(uint16_t)c0;       /* extu.w r14 */
        if (nd > step) {
            if (!(step >= c0u)) {
                *st = (uint16_t)(c0 - step);
                out = *st;
            }
        } else {
            if (!(nd >= c0u)) {
                *st = (uint16_t)(c0 + d);
                out = *st;
            }
        }
    }
    D8(TS_D_LIM) = (uint8_t)(out / 100u);                /* extu.w, divs #100 */
}

/* ── 0x14B34A: the invalid flags ─────────────────────────────────────── */
static void d_14b34a(void)
{
    D8(TS_D_D5) = (uint8_t)(((D8(TS_D_CC) == 1u && D8(TS_D_CD) == 1u) ||
                             D8(TS_D_JUMP) == 1u || D8(TS_D_BD) != 0u) ? 1u : 0u);
    D8(TS_D_D2) = (uint8_t)(((D8(TS_D_CC) == 1u && D8(TS_D_CE) == 1u) ||
                             D8(TS_D_B9) != 0u) ? 1u : 0u);
    D8(TS_D_D3) = (uint8_t)(((D8(TS_D_CD) == 1u && D8(TS_D_CE) == 1u) ||
                             D8(TS_D_BB) != 0u) ? 1u : 0u);
    D8(TS_D_D7) = (uint8_t)(((D8(TS_D_D2) == 1u && D8(TS_D_D3) == 1u) ||
                             (D8(TS_D_D2) == 1u && D8(TS_D_D5) == 1u) ||
                             (D8(TS_D_D3) == 1u && D8(TS_D_D5) == 1u)) ? 1u : 0u);
}

/* ── the gathers (0x1517E0 @0x151A32..0x151B82 / 0x132DCE @0x134210..
 * 0x13424C: ED20; 0x1517E0 @0x151B8C.. / 0x151C9C: E998) ──────────────── */
static void d_gather(void)
{
    D8(TS_D_B8) = CELL_TMP_PUB_E0;                       /* 0x7BF50 */
    D8(TS_D_B9) = CELL_TMP_PUB_E2;                       /* 0x7BF58 */
    D8(TS_D_BA) = CELL_TMP_PUB_E3;                       /* 0x7BF60 */
    D8(TS_D_BB) = CELL_TMP_PUB_E5;                       /* 0x7BF68 */
    D8(TS_D_BC) = CELL_TMP_PUB_E6;                       /* 0x7BF40 */
    D8(TS_D_BD) = CELL_TMP_PUB_E8;                       /* 0x7BF48 */
}

static void e_gather(void)
{
    E8(TS_E_IN2) = CELL_TMP_PUB_E3;                      /* 0x4C5A8 → 0x7BF60 */
    E8(TS_E_IN) = CELL_TMP_PUB_E6;                       /* 0x4C5AC → 0x7BF40 */
    E8(TS_E_INF) = CELL_TMP_PUB_E8;                      /* 0x4C5B0 → 0x7BF48 */
    E8(TS_E_IN2F) = CELL_TMP_PUB_E5;                     /* 0x4C5B4 → 0x7BF68 */
}

/* ── 0x14A4F0: ED20 (re)initialisation (the integer fields) ─────────── */
static void d_14a4f0(void)
{
    D8(TS_D_JUMP) = 0u;
    D8(TS_D_JUMP_P) = 0u;
    D8(TS_D_CB) = 0u;
    /* +0xCC/CD/CE/D2/D3/D5/D7 from 0xFFF82EF4 + 0x76/78/7A/7E/7C/80/82:
     * STAND-IN 0 (the driving dump's values; not our RAM) */
    D8(TS_D_CC) = 0u;
    D8(TS_D_CD) = 0u;
    D8(TS_D_CE) = 0u;
    D8(TS_D_D2) = 0u;
    D8(TS_D_D3) = 0u;
    D8(TS_D_D5) = 0u;
    D8(TS_D_D7) = 0u;
    D8(TS_D_BE) = D8(TS_D_BD);
    D8(TS_D_C4) = D8(TS_D_BA);
    D8(TS_D_C6) = D8(TS_D_BC);
    TS_U16(TS_ED20, TS_D_STATE) = (uint16_t)((uint32_t)D8(TS_D_BC) * 100u);
    /* +0x98..+0xB4 the unported monitors' counters: not ported */
    D8(TS_D_BE + 1u) = (uint8_t)~D8(TS_D_BE);
    D8(TS_D_C4 + 1u) = (uint8_t)~D8(TS_D_C4);
    D8(TS_D_C6 + 1u) = (uint8_t)~D8(TS_D_C6);
    D8(TS_D_D0 + 1u) = (uint8_t)~D8(TS_D_D0);
    D8(TS_D_JUMP_P + 1u) = (uint8_t)~D8(TS_D_JUMP_P);
    D8(TS_D_D3 + 1u) = (uint8_t)~D8(TS_D_D3);
    D8(TS_D_D5 + 1u) = (uint8_t)~D8(TS_D_D5);
    D8(TS_D_D7 + 1u) = (uint8_t)~D8(TS_D_D7);
    /* +0xD9..+0xDC (the halt flags) and 0x149BD0 (the float channel): not ported */
}

/* ── 0x14A6E8: the ED20 cycle, integer channel ───────────────────────── */
static void d_14a6e8(void)
{
    /* the complement / float lockstep checks (fatal on a mismatch): not ported */
    d_14adb2();
    d_14acf0();
    /* 0x14AE38 / 0x14AF70 / 0x14B27A: not ported (+0xCB..+0xCE, +0xD0 hold) */
    D8(TS_D_BE + 1u) = (uint8_t)~D8(TS_D_BE);
    D8(TS_D_C4 + 1u) = (uint8_t)~D8(TS_D_C4);
    D8(TS_D_C6 + 1u) = (uint8_t)~D8(TS_D_C6);
    D8(TS_D_D0 + 1u) = (uint8_t)~D8(TS_D_D0);
    D8(TS_D_JUMP_P + 1u) = (uint8_t)~D8(TS_D_JUMP_P);
    d_14b34a();
    /* 0x149D24 (the float shadow) and its comparison: not ported; the
     * complements the stock writes once it agrees */
    D8(TS_D_D3 + 1u) = (uint8_t)~D8(TS_D_D3);
    D8(TS_D_D5 + 1u) = (uint8_t)~D8(TS_D_D5);
    D8(TS_D_D7 + 1u) = (uint8_t)~D8(TS_D_D7);
    D8(TS_D_LIM + 1u) = (uint8_t)~D8(TS_D_LIM);
}

/* ── 0x1401C2: E998 initialisation (the bytes +0x30..+0x35) ──────────── */
static void e_1401c2(void)
{
    E8(TS_E_ST) = 2u;
    E8(TS_E_ST2) = 2u;
    E8(TS_E_OUT_C) = (uint8_t)~E8(TS_E_OUT);
    E8(TS_E_ST_C) = (uint8_t)~E8(TS_E_ST);
    /* +0x1D, +0x24/+0x28 (cal mirror), +0x36..+0x3A: not ported */
}

/* ── 0x140416: E998 direct conditioning (boot) ───────────────────────── */
static void e_140416(void)
{
    if (E8(TS_E_IN2F) == 0u) {                           /* +0x2F */
        E8(TS_E_OUT2) = E8(TS_E_IN2);
        E8(TS_E_ST2) = 0u;
    } else {
        uint16_t v = (uint16_t)(TS_CAL_DEFAULT + TS_CAL_SUB2);     /* 0x154956(v, 0, 0xFF) */
        E8(TS_E_OUT2) = (uint8_t)(v > 0xFFu ? 0xFFu : v);
        E8(TS_E_ST2) = 2u;
    }
    if (E8(TS_E_INF) == 0u) {                            /* +0x2E */
        E8(TS_E_OUT) = E8(TS_E_IN);
        E8(TS_E_ST) = 0u;
    } else {
        E8(TS_E_OUT) = TS_CAL_DEFAULT;
        E8(TS_E_ST) = 2u;
    }
    E8(TS_E_OUT_C) = (uint8_t)~E8(TS_E_OUT);
    E8(TS_E_ST_C) = (uint8_t)~E8(TS_E_ST);
    /* the RAM tests 0x148ACC: not ported */
}

/* ── 0x1405FA: E998 cyclic conditioning from ED20 ────────────────────── */
static void e_1405fa(void)
{
    const uint8_t d3 = D8(TS_D_D3);
    const uint8_t d5 = D8(TS_D_D5);
    const uint8_t lim = D8(TS_D_LIM);
    uint8_t v;
    uint32_t sum;

    if (d5 == 0u && d3 == 0u) {
        E8(TS_E_ST) = 0u;
        E8(TS_E_OUT) = lim;
        E8(TS_E_ST2) = 0u;
        E8(TS_E_OUT2) = E8(TS_E_IN2);
        return;
    }
    if (d5 == 0u) {                                      /* d3 != 0 */
        E8(TS_E_ST) = 0u;
        E8(TS_E_OUT) = lim;
        E8(TS_E_ST2) = 1u;
        sum = (uint32_t)lim + TS_CAL_SUB2;
        v = lim;
    } else if (d3 == 0u) {                               /* d5 != 0 */
        E8(TS_E_ST) = 1u;
        v = E8(TS_E_IN2) > TS_CAL_SUB2 ? (uint8_t)(E8(TS_E_IN2) - TS_CAL_SUB2) : 0u;
        E8(TS_E_OUT) = v;
        E8(TS_E_ST2) = 0u;
        E8(TS_E_OUT2) = E8(TS_E_IN2);
        return;
    } else {
        E8(TS_E_ST) = 2u;
        E8(TS_E_OUT) = TS_CAL_DEFAULT;
        E8(TS_E_ST2) = 2u;
        sum = (uint32_t)TS_CAL_DEFAULT + TS_CAL_SUB2;
        v = TS_CAL_DEFAULT;
    }
    E8(TS_E_OUT2) = sum > 0xFFu ? 0xFFu : (uint8_t)(v + TS_CAL_SUB2);
}

/* ── interface ────────────────────────────────────────────────────────── */
void tmp_safe_init(void)
{
    for (uint32_t a = TS_E998 + TS_E_IN2; a <= TS_E998 + TS_E_ST2; a++) {
        *(volatile uint8_t *)(uintptr_t)a = 0u;
    }
    for (uint32_t a = TS_ED20 + TS_D_B8; a <= TS_ED20 + TS_D_D7 + 1u; a++) {
        *(volatile uint8_t *)(uintptr_t)a = 0u;
    }
    /* the startup table 0x46948 → 0x15A67C → 0x151C9C, then the mode-0
     * init list → 0x132980 → 0x14A4F0(ED20) and 0x1401C2(E998) */
    tmp_safe_boot_pass();
    d_14a4f0();
    e_1401c2();
}

void tmp_safe_mode0_10ms(void)
{
    d_gather();                                          /* 0x134210..0x13424C */
    d_14a4f0();                                          /* 0x134258 */
}

void tmp_safe_boot_pass(void)
{
    e_1401c2();                                          /* 0x151C9E */
    e_gather();                                          /* 0x151CA4..0x151CCE */
    e_140416();                                          /* jmp 0x140416 */
}

void tmp_safe_100ms(void)
{
    d_gather();                                          /* 0x1517E0 */
    e_gather();
    d_14a6e8();                                          /* 0x14F7EA: 0x14A6E8(ED20) */
    e_1405fa();                                          /* 0x1402F0(E998) → 0x1405FA */
    E8(TS_E_OUT_C) = (uint8_t)~E8(TS_E_OUT);             /* 0x140366..0x140376 */
    E8(TS_E_ST_C) = (uint8_t)~E8(TS_E_ST);
}

uint8_t tmp_safe_byte(void)
{
    return E8(TS_E_OUT);                                 /* 0x140050 */
}
#endif /* !TCU_RAM_APP */
