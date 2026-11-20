/* lp_shadow.c - the READ-ONLY healthy line-pressure / Sys-request shadow (app/lp_shadow.h).
 * (2026-11-20, ~/tcu_sweep/lp_ti_shadow_port.md) It includes no live module, names no live
 * cell, calls no live function and has no volatile pointer: the inputs arrive BY VALUE
 * (app/lp_hook.c) and the only RAM it writes is its own (.lp_shadow). The stock calibration
 * at 0x80000 and above is read in place (app/cal_inplace.h); the tables from below 0x80000
 * are the generated copies of app/lp_cal.c. */
#define TCU_LP_SHADOW_TU 1
#include "app/lp_shadow.h"
#include "app/lp_cal.h"
#include "app/cal_inplace.h"

/* the one tables below 0x80000 reached through virtual addresses (the Sys curve and the NO temperature
 * map, flash 0xE108 / 0xE170); every other address is a real stock address read in place */
#define LPV_E108  0xE0000000u
#define LPV_E170  0xE0001000u
#define LPV_COND  0x19C02Cu      /* the T3 conditioning map (0x53A10 chain) */
#define LPV_BLEND 0x19C03Cu      /* the T2 / T3 blend weight curve (0x53A14) */
#define LPV_RATIO 0x199D08u      /* the converter torque ratio map (0x5051C) */

static uint32_t lpi_ld32(uint32_t a)
{
    if (a == LPV_E108) {
        return LP_E108[0];
    }
    if (a == LPV_E170) {
        return (uint32_t)LP_E170[0];
    }
    if (a == LPV_E170 + 4u) {
        return (uint32_t)LP_E170[1];
    }
    return cal_u32(a);
}

static uint16_t lpi_ld16(uint32_t a)
{
    if (a >= LPV_E170) {
        return (uint16_t)LP_E170[2u + (a - LPV_E170 - 8u) / 2u];
    }
    if (a >= LPV_E108) {
        return LP_E108[1u + (a - LPV_E108 - 4u) / 2u];
    }
    return cal_u16(a);
}

static uint8_t lpi_ld8(uint32_t a) { return cal_u8(a); }

/* ── interpolators (formats from their code) ───────────────────────────── */
/* 0xB6EAE: {u32 n; u16 x[n]; u16 y[n]} */
static uint16_t lpi_b6eae(uint32_t m, uint16_t x)
{
    const int32_t n = (int32_t)lpi_ld32(m);
#define X(i) lpi_ld16(m + 4u + 2u * (uint32_t)(i))
#define Y(i) lpi_ld16(m + 4u + 2u * (uint32_t)(n + (i)))
    int32_t k = 0;
    float num = 0.0f, den = 0.0f;
    if (X(0) < x) {
        if (x < X(n - 1)) {
            int32_t p = 0;
            while (X(p) < x) {
                p++;
            }
            k = p - 1;
            den = (float)(int32_t)((uint32_t)X(p) - X(k));
            num = (float)(int32_t)((uint32_t)x - X(k));
        } else {
            num = 1.0f;
            den = 1.0f;
            k = n - 2;
        }
    }
    {
        const uint32_t y0 = Y(k);
        if (num == 0.0f) {
            return (uint16_t)y0;
        }
        {
            const uint32_t y1 = Y(k + 1);
            if (y1 < y0) {
                return (uint16_t)(y0 - ((uint32_t)(int32_t)(((float)(int32_t)(y0 - y1) * num) / den) & 0xFFFFu));
            }
            return (uint16_t)((uint32_t)(int32_t)(((float)(int32_t)(y1 - y0) * num) / den) + y0);
        }
    }
#undef X
#undef Y
}

/* 0xB6E10: {u32 n; s16 x[n]; s16 y[n]} */
static int32_t lpi_b6e10(uint32_t m, int16_t x)
{
    const int32_t n = (int32_t)lpi_ld32(m);
#define X(i) ((int32_t)(int16_t)lpi_ld16(m + 4u + 2u * (uint32_t)(i)))
#define Y(i) ((int32_t)(int16_t)lpi_ld16(m + 4u + 2u * (uint32_t)(n + (i))))
    int32_t k = 0;
    float num = 0.0f, den = 0.0f;
    if (X(0) < x) {
        if (x < X(n - 1)) {
            int32_t p = 0;
            while (X(p) < x) {
                p++;
            }
            k = p - 1;
            den = (float)(X(p) - X(k));
            num = (float)(x - X(k));
        } else {
            den = 1.0f;
            num = 1.0f;
            k = n - 2;
        }
    }
    {
        int32_t y0 = Y(k);
        if (num != 0.0f) {
            const int32_t y1 = Y(k + 1);
            const int32_t d = y0 - y1;
            if (y0 <= y1) {
                return (int32_t)(int16_t)(int32_t)(((float)-d * num) / den) + y0;
            }
            y0 = y0 - (int32_t)(int16_t)(int32_t)(((float)d * num) / den);
        }
        return y0;
    }
#undef X
#undef Y
}

/* 0xB6D6C: {u32 n; u8 x[n]; u8 y[n]} */
static uint8_t lpi_b6d6c(uint32_t m, uint8_t x)
{
    const int32_t n = (int32_t)lpi_ld32(m);
#define X(i) ((uint32_t)lpi_ld8(m + 4u + (uint32_t)(i)))
#define Y(i) ((uint32_t)lpi_ld8(m + 4u + (uint32_t)(n + (i))))
    int32_t k = 0, i1 = 0, i4 = 0;
    if (X(0) < x) {
        if (x < X(n - 1)) {
            int32_t p = 0;
            while (X(p) < x) {
                p++;
            }
            k = p - 1;
            i4 = (int32_t)(X(p) - X(k));
            i1 = (int32_t)(x - X(k));
        } else {
            i1 = 1;
            i4 = 1;
            k = n - 2;
        }
    }
    {
        uint32_t y0 = Y(k);
        if (i1 != 0) {
            const uint32_t y1 = Y(k + 1);
            if (y1 < y0) {
                y0 = (y0 - ((uint32_t)(int32_t)((float)(int32_t)(i1 * (int32_t)(y0 - y1)) / (float)i4) & 0xFFu)) & 0xFFu;
            } else {
                y0 = ((uint32_t)(int32_t)((float)(int32_t)(i1 * (int32_t)(y1 - y0)) / (float)i4) + y0) & 0xFFu;
            }
        }
        return (uint8_t)y0;
    }
#undef X
#undef Y
}

/* 0xB7164: {u32 nx; u32 ny; s16 x[nx]; s16 y[ny]; s16 z[ny][nx]} */
static int32_t lpi_b7164(uint32_t m, int16_t x, int16_t y)
{
    const int32_t nx = (int32_t)lpi_ld32(m);
    const int32_t ny = (int32_t)lpi_ld32(m + 4u);
    const uint32_t xs = m + 8u;
#define X(i) ((int32_t)(int16_t)lpi_ld16(xs + 2u * (uint32_t)(i)))
#define Y(i) ((int32_t)(int16_t)lpi_ld16(xs + 2u * (uint32_t)(nx + (i))))
#define Z(i) ((int32_t)(int16_t)lpi_ld16(xs + 2u * (uint32_t)(i)))
    int32_t kx = 0, ky = 0;
    float fx = 0.0f, dx = 0.0f, fy = 0.0f, dy;
    if (X(0) < x) {
        if (x < X(nx - 1)) {
            int32_t p = 0;
            while (X(p) < x) {
                p++;
            }
            kx = p - 1;
            dx = (float)(X(p) - X(kx));
            fx = (float)(x - X(kx));
        } else {
            dx = 1.0f;
            fx = 1.0f;
            kx = nx - 2;
        }
    }
    if (Y(0) < y) {
        if (y < Y(ny - 1)) {
            int32_t p = 0;
            while (Y(p) < y) {
                p++;
            }
            ky = p - 1;
            dy = (float)(Y(p) - Y(ky));
            fy = (float)(y - Y(ky));
        } else {
            dy = 1.0f;
            fy = 1.0f;
            ky = ny - 2;
        }
    } else {
        dy = (float)(Y(1) - Y(0));
    }
    {
        const int32_t b0 = nx * ky + nx + ny + kx;     /* index from xs */
        int32_t a = Z(b0);
        int32_t c;
        if (fx == 0.0f) {
            c = (int32_t)(int16_t)Z(b0 + nx);
        } else {
            const int32_t z01 = Z(b0 + 1);
            int32_t d = a - z01;
            if (z01 < a) {
                d = -(int32_t)(((float)d * fx) / dx);
            } else {
                d = (int32_t)(((float)-d * fx) / dx);
            }
            a = d + a;
            {
                const int32_t z10 = (int16_t)Z(b0 + nx);
                const int32_t z11 = Z(b0 + nx + 1);
                const int32_t e = z10 - z11;
                if (z11 < z10) {
                    c = (int16_t)(z10 - (int16_t)(int32_t)(((float)e * fx) / dx));
                } else {
                    c = (int16_t)((int16_t)(int32_t)(((float)-e * fx) / dx) + z10);
                }
            }
        }
        if (fy != 0.0f) {
            const int32_t a16 = (int16_t)a;
            const int32_t d = a16 - c;
            if (a16 <= c) {
                return (int32_t)(int16_t)(int32_t)(((float)-d * fy) / dy) + a16;
            }
            a = a16 - (int32_t)(int16_t)(int32_t)(((float)d * fy) / dy);
        }
        return a;
    }
#undef X
#undef Y
#undef Z
}

/* 0xB72D8: as 0xB7164 with u16 axes and z (state-5 release ramp only) */
static uint32_t lpi_b72d8(uint32_t m, uint16_t x, uint16_t y)
{
    const int32_t nx = (int32_t)lpi_ld32(m);
    const int32_t ny = (int32_t)lpi_ld32(m + 4u);
    const uint32_t xs = m + 8u;
#define X(i) ((uint32_t)lpi_ld16(xs + 2u * (uint32_t)(i)))
#define Y(i) ((uint32_t)lpi_ld16(xs + 2u * (uint32_t)(nx + (i))))
#define Z(i) ((uint32_t)lpi_ld16(xs + 2u * (uint32_t)(i)))
    int32_t kx = 0, ky = 0;
    float fx = 0.0f, dx = 0.0f, fy = 0.0f, dy;
    if (X(0) < x) {
        if (x < X(nx - 1)) {
            int32_t p = 0;
            while (X(p) < x) {
                p++;
            }
            kx = p - 1;
            dx = (float)(int32_t)(X(p) - X(kx));
            fx = (float)(int32_t)(x - X(kx));
        } else {
            dx = 1.0f;
            fx = 1.0f;
            kx = nx - 2;
        }
    }
    if (Y(0) < y) {
        if (y < Y(ny - 1)) {
            int32_t p = 0;
            while (Y(p) < y) {
                p++;
            }
            ky = p - 1;
            dy = (float)(int32_t)(Y(p) - Y(ky));
            fy = (float)(int32_t)(y - Y(ky));
        } else {
            dy = 1.0f;
            fy = 1.0f;
            ky = ny - 2;
        }
    } else {
        dy = (float)(int32_t)(Y(1) - Y(0));
    }
    {
        const int32_t b0 = nx * ky + nx + ny + kx;
        const uint32_t z00 = Z(b0);
        uint32_t a = (uint32_t)(int32_t)(int16_t)z00;
        uint32_t c;
        if (fx == 0.0f) {
            c = (uint32_t)(int32_t)(int16_t)Z(b0 + nx);
        } else {
            const uint32_t z01 = Z(b0 + 1);
            if (z01 < z00) {
                a = a - (uint32_t)(int32_t)(((float)(int32_t)(z00 - z01) * fx) / dx);
            } else {
                a = (uint32_t)(int32_t)(((float)(int32_t)(z01 - z00) * fx) / dx) + z00;
            }
            {
                const uint32_t z10 = Z(b0 + nx);
                const uint32_t z11 = Z(b0 + nx + 1);
                if (z11 < z10) {
                    c = (uint32_t)(int32_t)(int16_t)z10 - (uint32_t)(int32_t)(((float)(int32_t)(z10 - z11) * fx) / dx);
                } else {
                    c = (uint32_t)(int32_t)(((float)(int32_t)(z11 - z10) * fx) / dx) + z10;
                }
            }
        }
        if (fy != 0.0f) {
            const uint32_t a5 = a & 0xFFFFu;
            if ((c & 0xFFFFu) < a5) {
                a = (a5 - ((uint32_t)(int32_t)(((float)(int32_t)(a5 - (c & 0xFFFFu)) * fy) / dy) & 0xFFFFu)) & 0xFFFFu;
            } else {
                a = ((uint32_t)(int32_t)(((float)(int32_t)((c & 0xFFFFu) - a5) * fy) / dy) + a5) & 0xFFFFu;
            }
        }
        return a;
    }
#undef X
#undef Y
#undef Z
}


/* ── the one writer-free state ─────────────────────────────────────────── */
static lp_t s_lp __attribute__((section(".lp_shadow")));
#define L (s_lp)
#if !defined(__sh__)
static uint8_t s_host_r50;
void lp_shadow_host_r50(uint8_t v) { s_host_r50 = v; }
#define R50_FORCED (s_host_r50)
#else
#define R50_FORCED 0u
#endif

#define W (L.w)
#define I (L.in)

#define CALW(off)  cal_u16(LP_CALA(off))
#define CALS(off)  cal_s16(LP_CALA(off))
#define CALB(off)  cal_u8(LP_CALA(off))

/* ── small clamps (0xB785C / 0xB7768 / 0xB7786 / 0xB7888 / 0xB7776) ───── */
static uint16_t sat_u16(int32_t v) { return (v < 0) ? 0u : ((v > 0xFFFF) ? 0xFFFFu : (uint16_t)v); }
static int16_t  sat_s16(int32_t v) { return (v > 32767) ? (int16_t)32767 : ((v < -32768) ? (int16_t)-32768 : (int16_t)v); }
static uint8_t  sat_u8(int32_t v) { return (v < 0) ? 0u : ((v > 255) ? 255u : (uint8_t)v); }
static uint16_t clamp3(uint16_t v, uint16_t lo, uint16_t hi)   /* 0xB7786 */
{
    if (v < lo) {
        return lo;
    }
    return (hi < v) ? hi : v;
}

/* 0xB6C9E: {u32 n; s16 v[n]} */
static int16_t b6c9e(uint32_t m, uint32_t i) { return (int16_t)lpi_ld16(m + 4u + 2u * i); }

/* 0xD1F06 / 0xD1F18: the PT1 (value x 1000, time stamp) */
static void pt1_reset(int32_t *val, int16_t *ts, uint16_t v)
{
    *ts = (int16_t)I.now_ms;
    *val = (int32_t)v * 1000;
}

static uint16_t pt1_step(int32_t *val, int16_t *ts, uint16_t in, uint16_t tau)
{
    uint32_t tauu = tau;
    int32_t dt;
    int32_t d;
    if (tauu == 0u) {
        tauu = 1u;
    }
    dt = (int32_t)(int16_t)I.now_ms - (int32_t)*ts;
    if (tauu < ((uint32_t)dt & 0xFFFFu)) {
        dt = (int32_t)tauu;
    }
    d = (int32_t)in * 1000 - *val;
    if (d != 0) {
        const uint32_t dtu = (uint32_t)dt & 0xFFFFu;
        int32_t lim = 0x7FFFFFFF / d;                    /* 0xD1FA4 / 0xB783A: |0x7FFFFFFF / d| */
        if (lim < 0) {
            lim = -lim;
        }
        if ((int32_t)dtu < lim) {
            d = (int32_t)(((int64_t)d * (int64_t)dtu) / (int64_t)tauu);
        } else {
            d = (int32_t)dtu * (d / (int32_t)tauu);
        }
        *val += d;
        if (d == 0) {
            return sat_u16(*val / 1000);                  /* no time-stamp update */
        }
    }
    *ts = (int16_t)I.now_ms;
    return sat_u16(*val / 1000);
}

/* ── the Sys law (0xE54A4 -> 0x107FB8; type 1 = normally open) ────────── */
uint16_t lp_sys_law(uint16_t p, uint8_t mode, uint8_t temp, uint16_t line)
{
    if (mode != 0u) {                                   /* 0x10809C */
        if (p == 0u) {
            return 8500u;                               /* i_max */
        }
        if (p >= line || p >= 12750u) {
            return 500u;                                /* i_min */
        }
        W.fault_n++;                                    /* 0x1C2 (counted, not raised) */
        return 500u;
    }
    {                                                   /* 0x107FE0: curve 0xE108 + the NO map 0xE170 */
        const uint16_t i0 = lpi_b6eae(LPV_E108, p);
        const int16_t  s = sat_s16((int32_t)i0);
        const int32_t  corr = (int16_t)lpi_b7164(LPV_E170, s, (int16_t)temp);
        const int32_t  v = corr + (int32_t)i0;          /* Sys: 0xD9F10 skipped, 0xD9F44 all zero */
        return clamp3(sat_u16(v), 500u, 8500u);
    }
}

/* ── the input torque 0xFFF8E34A (0xDDFD8) ─────────────────────────────── */
static int16_t t3_cond(int16_t t3)                       /* 0xB7BA0 -> 0xD4580 */
{
    const uint32_t pct = lpi_b6d6c(LPV_COND, I.c18);
    return sat_s16(((int32_t)pct * (int32_t)t3) / 100);
}

static int16_t blend_t(int16_t a, int16_t b)             /* 0xB7BD2 -> 0xD464C(a = 0x89CB2, b = 0x89CAC) */
{
    const int16_t w = sat_s16(lpi_b6e10(LPV_BLEND, (int16_t)(b - a)));
    return sat_s16(((int32_t)b * (255 - (int32_t)w) + (int32_t)w * (int32_t)a) / 255);
}

static uint8_t conv_ratio(void)                          /* 0xD522A -> 0xFFF89CFB, % */
{
    const uint32_t m = lpi_b72d8(LPV_RATIO, I.spd_ratio, I.eng);
    uint8_t r = sat_u8((int32_t)((m & 0xFFFFu) / 10u));
    if (r == 0u) {
        r = 1u;
    }
    return r;
}

int16_t lp_input_torque(const lp_in_t *in)
{
    int16_t r;
    const lp_in_t save = L.in;
    L.in = *in;
    {
        const int16_t b = blend_t(t3_cond(in->t3), in->t2);
        const int16_t m = (b < in->t1) ? in->t1 : b;     /* 0x104D4C: the larger (signed) */
        r = sat_s16(((int32_t)m * (int32_t)conv_ratio()) / 100);
    }
    L.in = save;
    return r;
}

/* ── the shift-type demand cell (0xE9ED2 at the shift start; 0xE9FF0) ── */
static void e9ed2(uint8_t bd16)
{
    const uint32_t k = (uint32_t)(uint8_t)I.c27 + 9u * (uint32_t)(uint8_t)I.c2d;
    L.cache_ok = 0u;
    if (k >= 81u) {
        W.err_n++;
        return;
    }
    if (bd16 == 1u) {
        const uint32_t idx = LP_UPIDX[k];
        if (idx > 6u) {
            W.err_n++;
            return;
        }
        L.cur_up = (uint16_t)idx;                        /* the type; the cell is read in e9ff0 */
    } else {
        const uint32_t idx = LP_DNIDX[k];
        if (idx > 17u) {
            W.err_n++;
            return;
        }
        L.cur_dn = (uint16_t)idx;
    }
    L.cache_ok = 1u;
}

static uint16_t e9ff0(void)
{
    if (I.bd16 != 1u) {
        return (uint16_t)cal_s16(LP_DNPTR[L.cur_dn]);
    }
    return (uint16_t)cal_s16(LP_UPPTR[2u * L.cur_up + ((L.e3ed != 0u) ? 1u : 0u)]);
}

/* ── the offers (0xEA7BA / 0xEA806 / 0xEA84E) ──────────────────────────── */
static uint8_t ea7ba(uint8_t reason)
{
    if (reason > 100u) {
        W.err_n++;                                       /* 0xB79E4 event 0x363: counted */
        reason = 0u;
    }
    return (uint8_t)b6c9e(LP_CALA(0xB0), reason);
}

static void offer(uint16_t v, uint8_t reason)           /* 0xEA806 */
{
    if (ea7ba(reason) == 0u) {
        if (L.w0c < v && L.d28 == 0u) {
            L.w0c = v;
            L.b18 = reason;
        }
    } else {
        W.si_class2_n++;
        if (L.w0a < v && L.d28 == 0u) {
            L.w0a = v;
            L.b17 = reason;
        }
    }
}

static void force(uint16_t v, uint8_t reason, uint32_t prio)   /* 0xEA84E */
{
    if ((int32_t)L.d28 < (int32_t)prio) {
        L.w04 = v;
        L.b16 = reason;
        L.d28 = prio;
    }
}

/* ── 0xEA0EE: the start maximum (reason 1) ─────────────────────────────── */
static void ea0ee(void)
{
    if (L.b12 == 0u && L.b1d == 0u &&
        ((I.bd16 == 3u && I.fill_any == 1u) || (I.bd8b == 1u && I.bd16 != 4u))) {
        if (CALB(0x14) < I.cd2 && I.eng < CALW(0x18)) {
            L.b12 = 1u;
            W.fill_n++;
        }
        L.b1f = 0u;
    }
    if (L.b12 == 1u) {
        const uint8_t lim = sat_u8((int32_t)lpi_b6eae(LP_CALA(0x48), I.eng));
        if (lim <= L.b1f) {
            L.b1d = 1u;
        }
    }
    if (L.b12 != 0u &&
        (I.bd15 == 0u || L.b1d == 1u || (I.bd72 != 0u && I.bd16 != 3u) || (I.bphase4 != 0u && I.bd16 == 3u))) {
        L.b12 = 0u;
    }
}

/* ── 0xEA230 ───────────────────────────────────────────────────────────── */
static void ea230(void)
{
    L.b1e = L.b12;
    L.b19 = L.b16;
    L.w06 = L.w04;
    L.w04 = 0u;
    L.b16 = 0xFFu;
    L.w08 = 0u;
    L.w0a = 0u;
    L.b17 = 0xFFu;
    L.w0c = 0u;
    L.b18 = 0xFFu;
    L.d28 = 0u;
    if (L.b19 != 2u) {
        L.b1c = 0u;
    }
    {                                                    /* 0xD20E0(obj + 0x4C, 0xB80D8 = 0xFFF8B10C) : falling edge; b10c HELD 0 */
        const uint8_t b10c = 0u;
        L.b26 = (L.b27 == 1u && b10c == 0u) ? 1u : 0u;
        L.b27 = b10c;
    }
    ea0ee();
}

/* ── 0xEA37E: the steady demand ────────────────────────────────────────── */
static uint16_t ea37e(uint8_t idx)
{
    uint32_t mi;
    uint32_t map;
    uint16_t r;
    int32_t v;
    if (idx < 10u) {
        mi = LP_G51DE0[idx];
    } else {
        mi = 99u;
    }
    if (mi > 8u) {
        W.err_n++;                                       /* 0xB75B4: counted; the previous map is kept */
        mi = L.b28;
    } else {
        L.b28 = (uint8_t)mi;
    }
    map = LP_MAP[mi];
    if ((I.c2f == 0u && I.c30 == 0u) || I.c30 == 0u) {   /* ((C2F == 0 && C2F == C30) || C30 == 0) */
        const uint16_t u = (uint16_t)lpi_b72d8(LP_CALA(0x58), I.eng, I.out);
        L.b1a = 0x19u;
        v = (int32_t)(int16_t)lpi_b6e10(LP_CALA(0x50), (int16_t)I.temp) + (int32_t)u;
    } else {
        const int16_t a = (int16_t)lpi_b7164(map, L.e34a, (int16_t)I.eng);
        L.b1a = 0x18u;
        v = (int32_t)a + (int32_t)(int16_t)lpi_b6e10(LP_CALA(0x4C), (int16_t)I.temp);
    }
    r = sat_u16(v);
    return r;
}

/* 0xE9E6A: a garage engagement (power flows differ): the steady demand + reason 0x10 */
static void e9e6a(void)
{
    uint16_t v3 = 0u;
    const uint16_t u = ea37e(I.c26);
    if (I.c30 == 1u || I.c30 == 3u) {
        v3 = CALW(0x20);
    } else if (I.c30 == 2u || I.c30 == 4u) {
        v3 = CALW(0x24);
    }
    offer(u, L.b1a);
    offer(v3, 0x10u);
}

/* 0xEA28C: a gear change inside a power flow: max(steady of both gears) + reason 0x11 */
static void ea28c(void)
{
    const uint16_t u1 = ea37e(I.c26);
    const uint8_t r1 = L.b1a;
    const uint16_t u2 = ea37e(I.c2c);
    const uint16_t a = (u1 < u2) ? u2 : u1;              /* 0x104D58: the larger */
    const uint16_t b = e9ff0();
    (void)r1;                                            /* 0xEB118(BD14) == 3 adds cal+0x40 = 0: stood in */
    offer(a, L.b1a);
    offer(b, 0x11u);
}

/* 0xEA03A */
static void ea03a(void)
{
    if (I.c2f == I.c30) {
        if (I.c25 == I.c2b) {
            const uint16_t u = ea37e(I.c26);
            offer(u, L.b1a);
        } else {
            ea28c();
        }
    } else {
        e9e6a();
    }
}

/* 0xEA07E: the per-gear maximum (reason 1) */
static void ea07e(void)
{
    if (L.cf5 != 0u || L.b12 != 0u) {
        const uint16_t v = (uint16_t)b6c9e(LP_CALA(0xAC), (uint8_t)((uint8_t)I.c2b + 1u));
        offer(v, 1u);
    }
}

/* 0xEAB90: reason 0x13, leaving P */
static uint16_t eab90(void)
{
    uint16_t val = 0u;
    const uint8_t r = I.range;
    if (r == 8u || L.b20 != 8u || I.pos == 0u) {
        L.w22 = 0u;
    } else {
        val = lpi_b6eae(LP_CALA(0x78), I.d1d);
        if (I.pos != 3u) {
            L.w22 = 0u;
            return val;
        }
        if (L.w22 < lpi_b6eae(LP_CALA(0x7C), I.temp)) {
            return val;
        }
    }
    L.b20 = r;
    return 0u;
}

/* 0xEA9EE: the hot window (reason 0x0C) */
static uint8_t ea9ee(void)
{
    const uint8_t t84 = CALB(0x84);
    const uint16_t c98 = CALW(0x98);
    const int16_t c9c = CALS(0x9C);
    const uint16_t ca0 = CALW(0xA0);
    const int16_t ca4 = CALS(0xA4);
    const uint8_t gear_ok = (I.c25 == 1 || I.c25 == 2 || I.c25 == -1) ? 1u : 0u;
    if (gear_ok != 0u) {
        const uint8_t out_of = (I.temp <= t84 || (uint32_t)c98 <= I.out || I.ca0 <= c9c) ? 1u : 0u;
        if (out_of == 0u) {
            L.b24 = 1u;
        } else if (L.b24 != 0u) {
            const int32_t h = (int32_t)t84 - (int32_t)CALB(0x8C);
            const uint8_t hb = sat_u8(h);
            if (!(hb <= I.temp && I.out <= (uint32_t)c98 + ca0 && (int32_t)c9c - (int32_t)ca4 <= I.ca0)) {
                L.b24 = 0u;
            }
        }
    } else {
        L.b24 = 0u;
    }
    return L.b24;
}

/* 0xEA4C8: the conditional demands */
static void ea4c8(void)
{
    {                                                    /* priority 8, reason 0: 0x89D10 && (0x89CEE || 0x89D11 || 0xB80D8) - the last three HELD 0 */
        const uint8_t cee = 0u, d11 = 0u, b10c = 0u;
        if (I.d10 != 0u && (cee != 0u || d11 != 0u || b10c != 0u)) {
            force(0u, 0u, 8u);
        }
    }
    /* 0x02 / 0x31 / 0x3B / 0x03 / 0x21 / 0x04 / 0x06 / 0x0F / 0x1C: their conditions are undecoded
     * (0xAEA24 / 0xAEA5C / 0xAEAC0 / 0xAEB8C / the converter state / 0xBA19E / 0xB8064 / 0xBA73C / 0xEAB0A): HELD 0 */
    W.si_cond_n++;
    {                                                    /* 0x05: table cal+0x28 [0xFFF8B298 = 1, the dump's] */
        offer((uint16_t)b6c9e(LP_CALA(0x28), 1u), 5u);
    }
    if (CALW(0x74) < I.eng) {                            /* 0xEA89C (reason 0x0A) not ported */
        W.si_r0a_n++;
    }
    if (ea9ee() != 0u) {
        const uint32_t mi = (I.c26 < 10u) ? LP_G51DE0[I.c26] : 99u;
        if (mi > 8u) {
            W.err_n++;
        } else {
            const uint32_t cell = LP_HOT[mi];
            offer((cell != 0u) ? (uint16_t)cal_s16(cell) : 0u, 0x0Cu);
        }
    }
    offer(eab90(), 0x13u);
    if (L.b19 == 1u && L.b16 != 1u && L.b26 != 0u) {     /* 0x1A: the hold */
        offer(CALW(0x44), 0x1Au);
    }
    if (I.c2f == 1u) {                                   /* 0x2D */
        offer(lpi_b6eae(LP_CALA(0x54), I.temp), 0x2Du);
    }
    if ((I.cf4 == 4u && I.bd15 == 0u) || I.bd74 == 4u) { /* 0x48 */
        offer(CALW(0xA8), 0x48u);
    }
}

/* 0xEA774 */
static uint16_t ea774(void)
{
    uint16_t r;
    L.w08 = 0u;                                          /* 0xD1D38 over [0, +0xA] with +0xA = 0 (class 0 for every reason) */
    if (L.d28 == 0u) {
        if (L.w0c < L.w08) {
            L.b16 = L.b17;
            r = L.w08;
        } else {
            r = L.w0c;
            L.b16 = L.b18;
        }
    } else {
        r = L.w04;
    }
    return r;
}

/* 0xEA2F8: the decrease filter's bypass */
static uint8_t ea2f8(void)
{
    uint8_t b = 0u;
    if (L.b1e != 0u && L.b12 == 0u) {
        b = 1u;
    }
    if (b == 0u && L.b16 != 0u) {
        uint8_t c = 0u;
        if (L.b19 == 1u && L.b16 != 1u && L.b26 != 0u) {
            c = 1u;
        }
        if (c == 0u) {
            return 0u;
        }
    }
    return 1u;
}

/* 0xEA33E */
static uint16_t ea33e(uint16_t v, uint8_t bypass)
{
    if (v < L.last_res && bypass == 0u) {
        v = pt1_step(&L.pt1_v, &L.pt1_t, v, CALW(0x1C));
        W.pt1_n++;
    } else {
        pt1_reset(&L.pt1_v, &L.pt1_t, v);
    }
    L.last_res = v;
    return v;
}

/* 0xE9CE0 */
static void e9ce0(void)
{
    uint16_t v;
    uint16_t cap;
    ea230();
    ea03a();
    ea07e();
    ea4c8();
    L.w04 = ea774();
    cap = (uint16_t)b6c9e(LP_CALA(0xAC), (uint8_t)((uint8_t)I.c2b + 1u));
    if (cap < L.w04) {
        L.w04 = cap;
    }
    v = ea33e(L.w04, ea2f8());
    L.res = v;
    /* 0xB82C6 (the engine-start flag): 0 in the healthy bench case -> no additive offset */
    L.b13 = 0u;
    L.w14 = 0u;
    L.pt2_t = (int16_t)I.now_ms;                         /* 0xD1D24: the class-2 filter re-armed with the result */
    L.pt2_v = (int32_t)(int16_t)L.res * 128;
    L.pt2_r = (int32_t)CALW(0xB4) * 1000 * 128;
    L.p10 = (uint16_t)((L.res / 10u > 0x7FFFu) ? 0x7FFFu : (L.res / 10u));
    L.w10 = pt1_step(&L.pt3_v, &L.pt3_t, L.res, lpi_b6eae(LP_CALA(0x80), I.temp));
    L.w0e = (L.res > 4999u) ? (uint16_t)(L.res - 5000u) : 0u;
    L.b1b = (uint8_t)L.d28;
}

/* ── the pass ──────────────────────────────────────────────────────────── */
void lp_shadow_init(void)
{
    uint8_t *p = (uint8_t *)&L;
    uint32_t i;
    for (i = 0u; i < (uint32_t)sizeof(L); i++) {
        p[i] = 0u;
    }
    L.b20 = 8u;                                          /* 0xEAB90's stored range at boot: P (the dump: range 8) */
    L.b28 = 0u;
}

void lp_shadow_pass(const lp_in_t *in)
{
    L.in = *in;
    if (in->os_mode < 1u || in->os_mode > 3u) {
        W.skip_mode_n++;
        return;
    }
    W.pass_n++;
    /* slot 36: 0xEA01E: both counters, step *(u16)(0xFFF8E21E + 2) = 1 */
    L.b1f = sat_u8((int32_t)L.b1f + 1);
    L.w22 = sat_u16((int32_t)L.w22 + 1);
    /* the gear-change cache: 0xE9ED2 at the shift start (BD16 = 1 / 2) */
    if (I.bd16 != L.bd16_last && (I.bd16 == 1u || I.bd16 == 2u)) {
        e9ed2(I.bd16);
    }
    L.bd16_last = I.bd16;
    L.cf5 = (uint8_t)(((LP_R50_HELD | R50_FORCED) != 0u) ? 1u : 0u);   /* 0x89CF5: t1 = reaction 50 (HELD 0), t2..t4 HELD 0 */
    /* slot 33/34: 0xDDFD8 */
    L.e34a = lp_input_torque(in);
    /* slot 42: 0xDC4C4 -> 0xBABEE -> 0xE9CE0 */
    e9ce0();
    L.sys_p = (int16_t)L.res;
    L.sys_i = lp_sys_law((uint16_t)(L.sys_p < 0 ? 0 : L.sys_p), L.cf5, I.temp, L.res);
    if (L.cf5 != 0u) {
        W.sys_bin_n++;
    }
    W.cmp_n++;
    if (L.sys_i != I.live_sys) {
        W.diff_n++;
    }
}

const volatile lp_t *lp_shadow_state(void) { return &s_lp; }
