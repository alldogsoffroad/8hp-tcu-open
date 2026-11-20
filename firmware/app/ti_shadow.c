/* ti_shadow.c - the READ-ONLY torque-intervention shadow (app/ti_shadow.h).
 * (2026-11-20, ~/tcu_sweep/lp_ti_shadow_port.md + ti_amounts_port.md) Includes no live module,
 * names no live cell, calls no live function, has no volatile pointer; the only RAM it writes is
 * its own (.ti_shadow). It NEVER transmits: the would-be field values stay in the witness.
 * The calibration it reads lies at 0xA0000 and above and is read IN PLACE (app/cal_inplace.h);
 * the index tables and the variant-0 block pointers of 0x50804 .. 0x56E0C are copies (app/ti_cal.c). */
#define TCU_TI_SHADOW_TU 1
#include "app/ti_shadow.h"
#include "app/lp_cal.h"
#include "app/ti_cal.h"
#include "app/cal_inplace.h"

static ti_t s_ti __attribute__((section(".ti_shadow")));
#define T (s_ti)
#define W (T.w)
#define I (T.in)

/* the common calibration block *0xFFF84D34 = 0x55524 (its words are stock addresses in the calibration) */
#define TI_C_TEMP_THR   0x1FE83Eu   /* +4  u8 40: BD22 > this (0 C) */
#define TI_C_TMO_A      0x1FF126u   /* +0xC  u16 200: the phase 0xA / 0xB timeout */
#define TI_C_TMO_D      0x1FF128u   /* +0x10 u16 200: the phases 3..6 timeout */
#define TI_C_GAR14      0x1ABEE0u   /* +0x14 garage curve (power flow not 2 / 4) */
#define TI_C_GAR18      0x1ABEECu   /* +0x18 garage curve (power flow 2 / 4) */
#define TI_C_FINAL      0x1ABEF8u   /* +0x1C the final scale map (flat 1000 per mille) */
#define TI_C_INC_DN     0x1FE840u   /* +0x20 u8 1: the down "increase" enable */
#define TI_C_INC_UP     0x1FE841u   /* +0x24 u8 0: the up "increase" enable */
#define TI_C_BLIP_THR   0x1FF12Au   /* +0x28 u16: the phase-3 gate flare threshold */
#define TI_C_T3CURVE    0x1ABF60u   /* +0x2C the high-torque T3 curve (0xE7C12) */
#define TI_C_T3MARGIN   0x1FF12Cu   /* +0x30 s16 */
#define TI_E3DE_EN      0x1FE8CBu   /* E3DE's enable byte (0xE971C): 0 = disabled */
#define TI_VAR2 0u                  /* 0x89C06 (the map variant inside a block) = 0 [counted stand-in] */

uint16_t ti_field13(int32_t q)
{
    const int32_t f = q + 2000;
    return (uint16_t)((f < 0) ? 0 : ((f > 0x1FFD) ? 0x1FFD : f));
}

static int16_t sat16(int32_t v)
{
    return (v > 32767) ? (int16_t)32767 : ((v < -32768) ? (int16_t)-32768 : (int16_t)v);
}
static uint8_t clamp_u8(int32_t v)
{
    return (uint8_t)((v < 0) ? 0 : ((v > 255) ? 255 : v));
}

/* the stock interpolators on a stock map (copies of the verified readers of app/gp_shadow.c) */
static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static int32_t s16at(const uint8_t *p, int32_t i)
{
    return (int32_t)(int16_t)(uint16_t)(((uint32_t)p[2 * i] << 8) | p[2 * i + 1]);
}

/* 0xB6D6C: {u32 n; u8 x[n]; u8 y[n]} */
static uint8_t m1u8(const uint8_t *m, uint8_t x)
{
    const int32_t n = (int32_t)be32(m);
    const uint8_t *xs = m + 4, *ys = m + 4 + n;
    int32_t k = 0, i1 = 0, i4 = 0;
    if (xs[0] < x) {
        if (x < xs[n - 1]) {
            int32_t p = 0;
            while (xs[p] < x) {
                p++;
            }
            k = p - 1;
            i4 = (int32_t)(xs[p] - xs[k]);
            i1 = (int32_t)(x - xs[k]);
        } else {
            i1 = 1;
            i4 = 1;
            k = n - 2;
        }
    }
    {
        uint32_t y0 = ys[k];
        if (i1 != 0) {
            const uint32_t y1 = ys[k + 1];
            if (y1 < y0) {
                y0 = (y0 - ((uint32_t)(int32_t)((float)(int32_t)(i1 * (int32_t)(y0 - y1)) / (float)i4) & 0xFFu)) & 0xFFu;
            } else {
                y0 = ((uint32_t)(int32_t)((float)(int32_t)(i1 * (int32_t)(y1 - y0)) / (float)i4) + y0) & 0xFFu;
            }
        }
        return (uint8_t)y0;
    }
}

/* 0xB7164: {u32 nx; u32 ny; s16 x[nx]; s16 y[ny]; s16 z[ny][nx]} */
static int32_t m2s16(const uint8_t *m, int16_t x, int16_t y)
{
    const int32_t nx = (int32_t)be32(m), ny = (int32_t)be32(m + 4);
    const uint8_t *xs = m + 8;
#define PX(i) s16at(xs, (i))
#define PY(i) s16at(xs, nx + (i))
#define PZ(i) s16at(xs, (i))
    int32_t kx = 0, ky = 0;
    float fx = 0.0f, dx = 0.0f, fy = 0.0f, dy;
    if (PX(0) < x) {
        if (x < PX(nx - 1)) {
            int32_t p = 0;
            while (PX(p) < x) {
                p++;
            }
            kx = p - 1;
            dx = (float)(PX(p) - PX(kx));
            fx = (float)(x - PX(kx));
        } else {
            dx = 1.0f;
            fx = 1.0f;
            kx = nx - 2;
        }
    }
    if (PY(0) < y) {
        if (y < PY(ny - 1)) {
            int32_t p = 0;
            while (PY(p) < y) {
                p++;
            }
            ky = p - 1;
            dy = (float)(PY(p) - PY(ky));
            fy = (float)(y - PY(ky));
        } else {
            dy = 1.0f;
            fy = 1.0f;
            ky = ny - 2;
        }
    } else {
        dy = (float)(PY(1) - PY(0));
    }
    {
        const int32_t b0 = nx * ky + nx + ny + kx;
        int32_t a = PZ(b0);
        int32_t c;
        if (fx == 0.0f) {
            c = (int32_t)(int16_t)PZ(b0 + nx);
        } else {
            const int32_t z01 = PZ(b0 + 1);
            int32_t d = a - z01;
            if (z01 < a) {
                d = -(int32_t)(((float)d * fx) / dx);
            } else {
                d = (int32_t)(((float)-d * fx) / dx);
            }
            a = d + a;
            {
                const int32_t z10 = (int16_t)PZ(b0 + nx);
                const int32_t z11 = PZ(b0 + nx + 1);
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
#undef PX
#undef PY
#undef PZ
}

/* 0xB6FD6: {u32 nx; u32 ny; u8 x[nx]; u8 y[ny]; u8 z[ny][nx]} */
static uint8_t m2u8(const uint8_t *m, uint8_t px, uint8_t py)
{
    const uint32_t nx = be32(m), ny = be32(m + 4);
    const uint8_t *ax = m + 8, *ay = ax + nx;
    uint32_t sx = 0u, sy = 0u;
    int32_t fx = 0, dx = 0, fy = 0, dy = 0;
    uint32_t u6, u7, u3;
    if (ax[0] < px) {
        if (px < ax[nx - 1u]) {
            uint32_t i = 0u;
            while (ax[i] < px) {
                i++;
            }
            sx = i - 1u;
            fx = (int32_t)px - (int32_t)ax[sx];
            dx = (int32_t)ax[i] - (int32_t)ax[sx];
        } else {
            fx = 1;
            dx = 1;
            sx = nx - 2u;
        }
    }
    if (ay[0] < py) {
        if (py < ay[ny - 1u]) {
            uint32_t i = 0u;
            while (ay[i] < py) {
                i++;
            }
            sy = i - 1u;
            fy = (int32_t)py - (int32_t)ay[sy];
            dy = (int32_t)ay[i] - (int32_t)ay[sy];
        } else {
            fy = 1;
            dy = 1;
            sy = ny - 2u;
        }
    } else {
        dy = (int32_t)ay[1] - (int32_t)ay[0];
    }
    {
        const uint8_t *z = ax + nx * sy + nx + ny + sx;
        u6 = z[0];
        if (fx == 0) {
            u7 = (uint32_t)(int32_t)(int8_t)z[nx];
        } else {
            const float f = (float)dx;
            int32_t d;
            u7 = z[1];
            if (u7 < u6) {
                d = -(int32_t)((float)(fx * (int32_t)(u6 - u7)) / f);
            } else {
                d = (int32_t)((float)(fx * (int32_t)(u7 - u6)) / f);
            }
            u6 = (uint32_t)((int32_t)u6 + d);
            u7 = z[nx];
            u3 = z[nx + 1u];
            if (u3 < u7) {
                u7 = u7 - (uint32_t)(int32_t)((float)(fx * (int32_t)(u7 - u3)) / f);
            } else {
                u7 = (uint32_t)(int32_t)((float)(fx * (int32_t)(u3 - u7)) / f) + u7;
            }
        }
        if (fy != 0) {
            u3 = u6 & 0xFFu;
            if ((u7 & 0xFFu) < u3) {
                u6 = (u3 - ((uint32_t)(int32_t)((float)(fy * (int32_t)(u3 - (u7 & 0xFFu))) / (float)dy) & 0xFFu)) & 0xFFu;
            } else {
                u6 = ((uint32_t)(int32_t)((float)(fy * (int32_t)((u7 & 0xFFu) - (u6 & 0xFFu))) / (float)dy) + u3) & 0xFFu;
            }
        }
    }
    return (uint8_t)u6;
}

/* ── map access on the stock calibration (in place) ───────────────────────── */
static uint8_t  mp2u(uint32_t a, uint8_t x, uint8_t y) { return (a != 0u) ? m2u8(cal_p8(a), x, y) : 0u; }
static int32_t  mp2s(uint32_t a, int16_t x, int16_t y) { return (a != 0u) ? m2s16(cal_p8(a), x, y) : 0; }
static uint8_t  mp1u(uint32_t a, uint8_t x) { return (a != 0u) ? m1u8(cal_p8(a), x) : 0u; }
static int16_t  ps16(uint32_t a) { return (a != 0u) ? cal_s16(a) : (int16_t)0; }      /* 0xE7F4E / 0xE7F76: 0 for the self-pointer */
static int16_t  tq_amt(uint8_t z) { return (int16_t)((int32_t)z * 6 - 870); }          /* z * 6 - 0x366 (145 = 0 Nm) */

/* 0xEC182 / 0xEC1F0: the shift class from the kind, the call-site flag and the pair type [O bytes 0xEC182..0xEC208] */
static uint8_t cls182(uint8_t kind, uint8_t flag, uint8_t type)
{
    uint8_t r = 0u;
    if (kind < 3u || kind > 9u) {
        return (type == 4u) ? 1u : 0u;
    }
    if (flag != 0u || kind == 4u || type == 4u) {
        r = 1u;
    }
    if ((flag != 0u && kind >= 5u) || kind == 6u || (kind == 8u && flag == 0u) || kind == 9u) {
        r = 2u;
    }
    if (kind == 7u && flag == 0u) {
        r = 1u;
    }
    if (kind == 8u && flag != 0u) {
        r = 3u;
    }
    return r;
}
static uint8_t cls1f0(uint8_t kind, uint8_t flag, uint8_t type)
{
    return (kind == 9u) ? 4u : cls182(kind, flag, type);
}

/* the per-pass block lookups (0xE584E / 0xE5814 / 0xE5892 then the table of the variant-0 blocks); NULL = out of range */
static const uint32_t *blk_584(uint8_t cls)
{
    const uint32_t i = (uint32_t)I.c2d * 9u + I.c27;
    uint8_t idx;
    if (i >= 81u) { W.err_n++; return 0; }
    idx = TI_IDX_584[(cls >= 1u && cls <= 4u) ? cls : 0u][i];
    if (idx >= 35u) { W.err_n++; return 0; }
    return TI_B_584[idx];
}
static const uint32_t *blk_5814(uint8_t cls)
{
    const uint32_t i = (uint32_t)I.c2d * 9u + I.c27;
    uint8_t idx;
    if (i >= 81u) { W.err_n++; return 0; }
    idx = TI_IDX_5814[(cls >= 1u && cls <= 3u) ? cls : 0u][i];
    if (idx >= 28u) { W.err_n++; return 0; }
    return TI_B_5814[idx];
}
static const uint32_t *blk_5892(uint8_t cls)
{
    const uint32_t i = (uint32_t)I.c2d * 9u + I.c27;
    uint8_t idx;
    uint8_t t;
    if (i >= 81u) { W.err_n++; return 0; }
    t = (cls == 1u) ? 1u : ((cls == 2u) ? 2u : ((cls == 4u) ? 3u : 0u));
    idx = TI_IDX_5892[t][i];
    if (idx >= 61u) { W.err_n++; return 0; }
    return TI_B_5892[idx];
}
static const uint32_t *blk_up(void)   { return (T.ui < 7u) ? TI_B_UP[T.ui] : 0; }
static const uint32_t *blk_gear(void) { return (T.gi < 9u) ? TI_B_GEAR[T.gi] : 0; }
static const uint32_t *blk_dn(void)   { return (T.di < 18u) ? TI_B_DN[T.di] : 0; }

/* ── 0xD1FB4 / 0xD2038: the accelerating blend (a fraction in per mille toward `end`, tau in ms) ── */
static void blend_init(int16_t start)
{
    T.bl_start = start;
    T.bl_frac = 0;
    T.bl_ts = (int16_t)I.now_ms;
}
static int16_t blend_step(int16_t target, int16_t tau, int16_t end)
{
    int32_t t = tau;
    int32_t d = (int32_t)end - T.bl_frac;
    int32_t step = 0;
    if (t == 0) {
        t = 1;
    }
    if (d != 0) {
        const int32_t dt = (int32_t)(uint16_t)(I.now_ms - (uint16_t)T.bl_ts);
        int32_t lim = (int32_t)(0x7FFFFFFF / d);
        if (lim < 0) {
            lim = -lim;
        }
        if (dt < lim) {
            step = d * dt / t;
        } else {
            step = dt * (d / t);
        }
        T.bl_frac += step;
        if (T.bl_frac > 1000) {
            T.bl_frac = 1000;
        } else if (T.bl_frac < 0) {
            T.bl_frac = 0;
        }
    }
    if (d == 0 || step != 0) {
        T.bl_ts = (int16_t)I.now_ms;
    }
    return sat16((((int32_t)target - T.bl_start) * T.bl_frac) / 1000 + T.bl_start);
}

/* 0xD1B94(t, T, a, b): t < T ? a + t (b - a) / T : b */
static int16_t d1b94(uint16_t t, uint16_t big, int16_t a, int16_t b)
{
    int32_t v = b;
    if ((uint32_t)t < (uint32_t)big) {
        v = ((int32_t)t * ((int32_t)b - (int32_t)a)) / (int32_t)big + (int32_t)a;
    }
    return sat16(v);
}

/* ── 0xE7782: the temperature (and the garage speed) factor of an amount ── */
static int16_t e7782(int16_t p)
{
    uint8_t u;
    const uint32_t *ub = blk_up();
    const uint32_t *db = blk_dn();
    if (p < 1) {
        if (T.cls != 2u) {
            const uint32_t m = (T.cls == 1u) ? (ub ? ub[0x20u / 4u] : 0u) : (db ? db[0] : 0u);
            u = mp2u(m, T.c18, T.c1c);
        } else {
            u = mp1u(((I.pf == 2u || I.pf == 4u) ? TI_C_GAR18 : TI_C_GAR14), T.c18);
        }
    } else {
        const uint32_t m = (T.cls == 1u) ? (ub ? ub[0x1Cu / 4u] : 0u) : (db ? db[1] : 0u);
        u = mp1u(m, T.c18);
    }
    return (int16_t)(((int32_t)p * (int32_t)u) / 255);
}

/* 0xE7126: the down-shift reduction under power (class 0) */
static int16_t e7126(void)
{
    const uint32_t *b;
    if (I.bd18 == 0u) {
        b = blk_5892(cls182(I.k7, 0u, I.ktype));
        if (b == 0) {
            return 0;
        }
        return tq_amt(mp2u(b[(0x10u / 4u) + TI_VAR2], T.bd78, T.c1c));
    }
    /* BD18 != 0: the BD17 == 4 block (cell 0x55690): +0x10 / +0x18 by the class, the 0x10FB52 predicate stood in false */
    {
        const uint8_t c = cls182(I.k7, 0u, I.ktype);
        const uint32_t *d4 = (T.d4i < 7u) ? TI_B_DN4[T.d4i] : 0;
        uint32_t m;
        W.si_misc_n++;                                   /* 0x10FB52(0xFFF8BDA0): false */
        if (d4 == 0) {
            return 0;
        }
        m = (c == 1u) ? d4[0x10u / 4u] : ((c == 2u) ? d4[0x18u / 4u] : d4[0u + TI_VAR2]);
        return (int16_t)(((int32_t)mp2u(m, T.bd78, T.c1c) - 0x91) * 6);
    }
}

/* 0xE76DE: the coast-blip target of a down shift (0 for the other classes) */
static int16_t e76de(void)
{
    int16_t v = 0;
    if (T.cls == 0u) {
        const uint32_t *b = blk_5892(cls1f0(I.k7, 0u, I.ktype));
        if (b != 0) {
            v = tq_amt(mp2u(b[0x1Cu / 4u + TI_VAR2], T.bd78, T.c1c));
        }
    }
    return e7782(v);
}

/* 0xE6F5E: the up-shift base (E584E set, x = BD78 the turbine peak / 32) */
static int16_t e6f5e(void)
{
    const uint32_t *b = blk_584(cls1f0(I.k7, 1u, I.ktype));
    int32_t v;
    if (b == 0) {
        return 0;
    }
    v = tq_amt(mp2u(b[1u + TI_VAR2], T.bd78, T.c1c));
    if (T.hd != 0u) {
        const uint32_t *ub = blk_up();
        if (ub != 0 && ub[0x2Cu / 4u] != 0u && ps16(ub[0x2Cu / 4u]) <= I.bd76) {
            v = (int16_t)v + tq_amt(mp2u(ub[0x30u / 4u], T.bd78, T.c1c));
        }
    }
    return sat16(v);
}

/* 0xE69F4: the base of an amount */
static int16_t e69f4(void)
{
    int32_t v;
    if (T.cls == 0u) {
        v = e7126();
    } else if (T.cls == 1u) {
        v = e6f5e();
    } else {
        const uint32_t *g = blk_gear();
        v = (g != 0) ? tq_amt(mp2u(g[0x18u / 4u + TI_VAR2], T.bd78, T.c1c)) : 0;      /* TI+0x34 = 0 (stood in) */
    }
    v = e7782(sat16(v));
    if (T.cls != 2u && v < 0) {
        /* the final scale map (0x1ABEF8, flat 1000 per mille): x = the cell 0x89CC8 (unknown, 0), y = T3 */
        const int32_t sc = mp2s(TI_C_FINAL, 0, I.t3);
        W.si_amount_n++;
        v = (v * sc) / 1000;
    }
    return sat16(v);
}

/* 0xE7D5E: the flare limiter of the up-shift phase 9 (and the clip of phase 0xA); p = the amount offered */
static int16_t e7d5e(int16_t p)
{
    const uint32_t *ub = blk_up();
    int16_t lim = 0;
    if (ub != 0) {
        const int32_t v = mp2s(ub[0u + TI_VAR2], I.bd80, I.tq);       /* x = BD80, y = 0x89C1A (the torque, Nm) */
        lim = sat16((v - 0x91) * 6);
        W.si_amount_n++;                                              /* 0x89C1A = the input torque in Nm (stand-in) */
    }
    T.v12 = lim;
    T.flare = lim;
    return (lim < p && lim < 0) ? lim : p;
}

/* 0xE7A72 / 0xE7844 / 0xE7814: the increase mode (TI+0x28) */
static uint8_t e7844(void)
{
    if (T.cls == 0u) {
        if (cal_u8(TI_C_INC_DN) == 0u) {
            return 0u;
        }
        if (e76de() > 0 && cal_u8(TI_C_TEMP_THR) < I.bd22) {
            return 1u;
        }
    }
    if (T.cls == 1u && cal_u8(TI_C_INC_UP) != 0u && e69f4() > 0 && cal_u8(TI_C_TEMP_THR) < I.bd22) {
        return 1u;
    }
    return 0u;
}
static uint8_t e7814(void)
{
    if (T.cls == 0u) {
        return e7844();
    }
    if (T.cls != 1u || e7844() != 1u) {
        return 0u;
    }
    W.si_misc_n++;                                       /* 0xE7A72 (the up increase: disabled by calibration here) */
    return 1u;
}

/* 0xE7C12: the high-torque variant TI+0xD (the T3 curve over the engaged gear + 1) */
static void e7c12(void)
{
    const int16_t crv = cal_s16(TI_C_T3CURVE + 4u + 2u * (uint32_t)((I.c26 < 10u) ? I.c26 : 9u));   /* 0xB6C94: an indexed s16 array */
    if (crv < I.t3) {
        T.hl = 1u;
    }
    if (T.cls == 1u) {
        if (I.t3 <= crv && T.hl != 0u) {
            T.hd = 1u;
            return;
        }
        if (T.hl != 0u && (int32_t)I.t3 <= (int32_t)ps16(TI_C_T3MARGIN) + (int32_t)crv) {
            return;                                      /* TI+0xD keeps its value */
        }
    }
    T.hd = 0u;
}

/* 0xE73AC: the down-shift gate of the phase 3 (class 0) */
static uint8_t e73ac(void)
{
    if (T.cls != 0u) {
        return 1u;
    }
    if (T.inc == 0u) {
        return 0u;                                       /* BD1F (0xFFF8BD1F) stood in 0 */
    }
    {
        const uint32_t *ba = blk_5892(cls1f0(I.k7, 0u, I.ktype));
        const uint32_t *bb = blk_5892(cls182(I.k7, 1u, I.ktype));
        int32_t a;
        int32_t b;
        if (ba == 0 || bb == 0) {
            return 1u;
        }
        a = mp2u(ba[0x24u / 4u], T.c1c, T.c12);
        b = (int8_t)(uint8_t)(mp1u(bb[0x28u / 4u], I.temp) - 0x80u);
        W.si_misc_n++;                                   /* 0x89C12 = engine / 32 */
        if ((int32_t)T.t1a <= b + a) {
            const int32_t fl = (I.bd80 < 0) ? -(int32_t)I.bd80 : (int32_t)I.bd80;
            if (fl < (int32_t)cal_u16(TI_C_BLIP_THR)) {
                return 0u;
            }
        }
        return 1u;
    }
}

/* 0xE795A: the phase-3 coast blip (a blend of the phase-start value toward 0xE76DE) */
static int16_t e795a(void)
{
    const int16_t s = e76de();
    int32_t pct = 0;
    int32_t r;
    const uint32_t *b;
    if (T.v8 < s) {
        T.v8 = s;
    }
    b = blk_5892(cls1f0(I.k7, 0u, I.ktype));
    if (b != 0) {
        pct = mp2u(b[0x18u / 4u], (uint8_t)((T.pc > 255u) ? 255u : T.pc), T.c1c);
    }
    r = (int32_t)sat16(((int32_t)T.v8 - (int32_t)T.v6) * pct / 100) + T.v6;
    if (s < r) {
        r = s;
    }
    if (T.inc == 0u && r > 0) {
        r = 0;
    }
    return sat16(r);
}

/* 0xE78EE (+ 0xE6D94): the down phases 4 / 5 in the increase mode */
static int16_t e78ee(void)
{
    const uint32_t *d = blk_dn();
    int32_t lim = 0x7FFFFFFF;
    if (d != 0 && d[2] != 0u) {
        lim = (int32_t)(int8_t)(uint8_t)(cal_u8(d[2]) - 0x80u);
    }
    if ((int32_t)I.bd76 < lim && T.inc_prev != 0u) {
        const int16_t s = e76de();
        const uint32_t *b;
        int32_t pct = 0;
        if (s < T.v6) {
            T.v6 = s;
        }
        b = blk_5892(cls182(I.k7, 1u, I.ktype));
        if (b != 0 && b[0x30u / 4u] != 0u) {
            pct = mp2s(b[0x30u / 4u], (int16_t)T.pa, T.v8);
        }
        return sat16(sat16(pct) * (int32_t)T.v30 / 100);
    }
    T.v6 = 0;
    T.v30 = 0;
    return 0;
}

/* 0xE6CA0 (the up phase B percent) / the garage phase B percent */
static int16_t fade_pct(void)
{
    int16_t y = I.turb;                                   /* BD7A = the turbine speed (flag byte 0x1FE8F5 = 0) */
    y = (int16_t)((uint16_t)y > 32767u ? 32767 : y);
    if (T.cls == 1u) {
        const uint32_t *b = blk_5814(cls182(I.k7, 0u, I.ktype));
        return (b != 0) ? sat16(mp2s(b[2u + TI_VAR2], I.bd76, y)) : (int16_t)0;
    }
    {
        const uint32_t *g = blk_gear();
        return (g != 0) ? sat16(mp2s(g[0x20u / 4u], I.bd76, y)) : (int16_t)0;
    }
}

/* 0xE728A: the garage phase 0xA entry gate (the on-coming / E33C readers: stood in true) */
static uint8_t e728a(void)
{
    W.si_misc_n++;
    return 1u;
}

/* 0xE65F4: the amount of the phase, Nm */
static int16_t amount(void)
{
    int32_t a = 0;
    switch (T.phase) {
    case 2u:
        T.tmr = 0u;
        return 0;
    case 3u:
        if (e73ac() != 0u) {
            return e795a();
        }
        T.pc = 0u;
        T.tmr = 0u;
        return 0;
    case 4u:
        if (T.inc == 0u) {
            a = e76de();
            return (int16_t)((a < 1) ? a : 0);
        }
        return e78ee();
    case 5u:
        if (T.inc == 0u || e7126() < 0) {
            a = e69f4();
        } else {
            a = e78ee();
        }
        T.p5seen = 1u;
        return sat16(a);
    case 6u:
        if (T.inc == 0u || e7126() < 0) {
            const uint32_t *b = blk_5892(cls182(I.k7, 1u, I.ktype));
            const int32_t pct = (b != 0) ? (int32_t)mp1u(b[1], (uint8_t)((T.pc > 255u) ? 255u : T.pc)) : 0;
            a = sat16((int32_t)T.v6 * pct / 100);
            if (T.a22 >= 1) {
                return 0;
            }
            return (int16_t)((a < 1) ? a : 0);
        }
        return e78ee();
    case 8u:
        return 0;
    case 9u: {
        const int16_t r = e7d5e(0);
        T.tmr = 0u;
        return r;
    }
    case 0xAu:
        if (T.inc == 0u) {
            if (T.cls == 2u) {
                const uint32_t *g = blk_gear();
                if (e728a() == 0u) {
                    T.tmr = 0u;
                    return 0;
                }
                if (g != 0) {
                    int32_t base = e69f4();
                    int32_t bl = blend_step(sat16(base), ps16(g[2]), ps16(g[3u + TI_VAR2]));
                    const int32_t gate = 255;            /* gear block +0x14 (slip gate map, x = 0x89C1D): stood in */
                    W.si_amount_n++;
                    bl = (bl * gate) / 255;
                    return (int16_t)((bl > 0) ? 0 : bl);
                }
                return 0;
            } else {
                const uint32_t *b = blk_5814(cls182(I.k7, 0u, I.ktype));
                int16_t base = e69f4();
                int16_t tau = 0;
                int16_t end = 0;
                int16_t o;
                if (b != 0) {
                    tau = ps16(b[0x18u / 4u + TI_VAR2]);
                    end = ps16(b[0x20u / 4u + TI_VAR2]);
                }
                o = blend_step(base, tau, end);
                if (o > 0) {
                    o = 0;
                }
                return e7d5e(o);
            }
        }
        W.si_misc_n++;                                   /* the up / garage increase branch (disabled by calibration) */
        return 0;
    case 0xBu:
        if (T.inc == 0u) {
            const int32_t pct = fade_pct();
            a = sat16((int32_t)pct * (int32_t)T.v2a / 100);
            if (a < T.a22) {
                a = T.a22;
            }
            return (int16_t)((a < 1) ? a : 0);
        }
        W.si_misc_n++;
        return 0;
    default:
        return 0;                                        /* 0xC: 0 */
    }
}

/* ── the phase exits: 0xE727A (the stock timeout) / 0xE74E2 / 0xE630C ── */
static uint8_t timeout_d(void) { return (T.tmr > cal_u16(TI_C_TMO_D)) ? 1u : 0u; }

/* 0xE6E20: the sync-progress threshold of the phase 0xA (curve - 128) */
static int32_t e6e20(void)
{
    if (T.cls == 2u) {
        const uint32_t *g = blk_gear();
        return (g != 0) ? (int32_t)mp1u(g[0x24u / 4u], T.c1c) - 128 : 0;
    }
    if (T.cls == 1u) {
        const uint32_t *b = (T.ci < 28u) ? TI_B_5814[T.ci] : 0;     /* TI+0x54: the block chosen at the arm (class flag 1) */
        return (b != 0) ? (int32_t)mp1u(b[0u + TI_VAR2], T.c1c) - 128 : 0;
    }
    return 0;
}

/* 0xE74E2(TI, inc): the exit tests */
static uint8_t e74e2(uint8_t inc)
{
    if (T.cls == 0u) {
        const uint32_t *b = blk_5892(cls182(I.k7, 1u, I.ktype));
        if (b == 0) {
            return 1u;
        }
        if (inc == 0u) {
            W.si_misc_n++;                               /* 0x89C12 = engine / 32 */
            return ((int32_t)(int8_t)(uint8_t)(mp2u(b[0xCu / 4u], T.c12, T.c1c) - 0x80u) <= (int32_t)I.bd76) ? 1u : 0u;
        }
        return ((int32_t)(int8_t)(uint8_t)(mp2u(b[0x2Cu / 4u], T.bd78, T.c1c) - 0x80u) <= (int32_t)I.bd76) ? 1u : 0u;
    }
    if (I.bd1b != 0u) {
        return I.sync_a;                                 /* ctx+0 the sync flag */
    }
    if ((int32_t)I.bd76 < e6e20()) {
        return 0u;
    }
    return 1u;                                           /* TI+0x19 == 0 (0xE7C8E's nominal limit byte: stood in) */
}

/* 0xE630C: does the phase end? */
static uint8_t phase_exit(void)
{
    switch (T.phase) {
    case 3u:
        if (timeout_d() != 0u) {
            return 1u;
        }
        W.si_misc_n++;                                   /* 0xDDBA8(E33C): stood in false */
        return e74e2(T.inc);
    case 4u:
        if (timeout_d() != 0u) {
            return 1u;
        }
        if (T.inc != 0u) {
            const uint32_t *b = blk_5892(cls182(I.k7, 1u, I.ktype));
            if (b != 0) {
                const int32_t c9 = (int8_t)(uint8_t)(mp2u(b[0x2Cu / 4u], T.bd78, T.c1c) - 0x80u);
                const int32_t c7 = (int8_t)(uint8_t)(mp2u(b[0xCu / 4u], T.c12, T.c1c) - 0x80u);
                if (c7 < c9) {
                    return 1u;
                }
            }
            return e74e2(0u);
        }
        return e74e2(0u);
    case 5u:
        if (timeout_d() != 0u) {
            return 1u;
        }
        if (T.p5seen == 0u) {
            return 0u;
        }
        {
            const uint32_t *b = blk_5892(cls182(I.k7, 1u, I.ktype));
            return (b != 0 && b[2] != 0u &&
                    (int32_t)(int8_t)(uint8_t)(cal_u8(b[2]) - 0x80u) <= (int32_t)I.bd76) ? 1u : 0u;
        }
    case 6u:
        if (T.a22 == 0) {
            return 1u;
        }
        return timeout_d();
    case 8u:
        return 1u;                                       /* 0xE6AE8, then true (decoded) */
    case 9u:
        /* 0xE72E6 (the on-coming object's counter against TI+0x26, BD1F, torque > the up block's limit): stood in */
        {
            const uint32_t *ub = blk_up();
            if (ub != 0 && ub[0x28u / 4u] != 0u && (int32_t)I.tq > (int32_t)ps16(ub[0x28u / 4u])) {
                return 1u;
            }
        }
        W.si_dwell_n++;
        return (T.pc >= TI_SI_MIN9 && I.on_left_fill != 0u) ? 1u : 0u;
    case 0xAu:
        if (T.cls == 2u) {
            W.si_misc_n++;                               /* 0xE6AE8 in the garage 0xA: the TI+0x26 hold (unused here) */
        }
        if (e74e2(T.inc) != 0u) {
            return 1u;
        }
        return (T.tmr > cal_u16(TI_C_TMO_A)) ? 1u : 0u;
    case 0xBu:
        if (T.a22 == 0) {
            return 1u;
        }
        return (T.tmr > cal_u16(TI_C_TMO_A)) ? 1u : 0u;
    case 0xCu:
        return 0u;                                       /* the terminal phase holds (decoded) */
    default:
        return 1u;                                       /* the switch's default (phases 0, 2, ...) */
    }
}

/* 0xE6210: the phase entry */
static void phase_enter(void)
{
    switch (T.phase) {
    case 2u:
        if (!(I.pf != 0u && T.out > 0)) {
            blend_init(T.a22);                           /* 0xD2038(TI+0x38, TI+0x22) */
        }
        break;
    case 3u: {
        const int16_t v = (T.inc == 0u || (I.pf != 0u && T.out > 0)) ? T.out : T.a22;
        T.v6 = v;
        T.v8 = v;
        break;
    }
    case 4u:
        T.v6 = T.a22;
        T.v8 = T.a22;
        T.v32 = e76de();
        T.v30 = T.out;
        T.pa = 0u;
        break;
    case 6u:
        if (!(T.out > 0 && T.inc != 0u)) {
            T.v6 = T.a22;
        }
        break;
    case 9u:
    case 0xAu:
        blend_init(T.a22);
        break;
    case 0xBu:
        T.v2a = T.a22;
        break;
    default:
        break;
    }
}

/* 0xE6B6E (+ 0xE7E08 / 0xE7A1E): the ramp-out after the end, and the shaping of a running amount */
static int16_t shape(uint8_t armed, int16_t amt)
{
    if (armed == 0u) {
        if (T.v20 < 0 && I.pf == 1u && I.bd1a != 0x0Au) {
            W.ramp_n++;
            T.a22 = 0;
            return d1b94(T.rc, T.rlen, T.v20, 0);       /* 0xD1B94(TI+0x1E, TI+0x1F, TI+0x20, 0) */
        }
        T.a22 = 0;
        return amt;
    }
    if (T.v20 < 0) {
        const int16_t r = d1b94(T.rc, T.rlen, T.v20, 0);
        if (T.inc == 0u) {
            if (r < amt) {
                amt = r;                                 /* a new shift armed during the ramp-out: the larger reduction */
            }
        } else if (amt == 0) {
            amt = r;
        }
    }
    return amt;                                          /* 0xE7E08: the increase part is not used here (TI+0x20 <= 0) */
}

/* 0xE5EAA */
static void run(void)
{
    int16_t amt;
    if (T.armed == 0u || I.bd1a == 0x0Au) {
        amt = shape(T.armed, 0);
        T.hd = 0u;
        T.out = amt;
        return;
    }
    {
        uint32_t guard;
        e7c12();                                         /* TI+0xD */
        T.inc_prev = T.inc;
        T.inc = e7814();
        for (guard = 0u; guard < 8u && phase_exit() != 0u; guard++) {
            const uint32_t row = ((uint32_t)T.step + 1u) * 3u + T.cls;
            uint8_t nxt;
            if (row >= 24u) {
                W.err_n++;                               /* 0xB75B4 (counted) */
                break;
            }
            nxt = LP_TI_PHASE[row];
#if defined(TI_NEG_TABLE)
            if (row == 8u) {
                nxt = 0xBu;                              /* the negative: the garage's second phase 0xA -> 0xB */
            }
#endif
            if (nxt == 0xFFu) {
                break;
            }
            T.step++;
            T.phase = nxt;
            T.pc = 0u;
            W.phase_n++;
            phase_enter();
        }
    }
    amt = amount();
    T.last_amt = amt;
    T.a22 = amt;
    amt = shape(1u, amt);
    T.out = amt;
}

/* 0xE5A20 (arm) + 0xE5E48 (start) */
static void arm(uint8_t bd16)
{
    const uint32_t i = (uint32_t)I.c2d * 9u + I.c27;
    T.armed = 1u;                                        /* TI+0x17 */
    T.cls = LP_TI_CLASS[bd16 & 7u];                      /* 0x634D6[BD16] */
    T.step = 0u;
    T.phase = 0u;
    T.tmr = 0u;
    T.pc = 0u;
    T.t1a = 0u;
    T.out = 0;
    T.a22 = 0;
    T.v6 = 0;
    T.v8 = 0;
    T.v2a = 0;
    T.v30 = 0;
    T.v32 = 0;
    T.v12 = 0;
    T.inc = 0u;
    T.inc_prev = 0u;
    T.p5seen = 0u;
    T.hd = 0u;
    T.hl = 0u;
    T.pa = 0u;
    T.bl_frac = 0;
    T.bl_start = 0;
    T.bl_ts = (int16_t)I.now_ms;
    /* the cache selections of 0xE5A20: the gear block always, then the class's blocks (index 0xFF / out of range = 0xB75B4 counted) */
    T.gi = (I.c26 < 10u) ? TI_IDX_GEAR[I.c26] : 0xFFu;
    T.ui = 0xFFu;
    T.di = 0xFFu;
    T.d4i = 0xFFu;
    T.ci = 0xFFu;
    if (T.gi >= 9u) {
        W.err_n++;
    }
    if (T.cls == 1u) {
        T.ui = (i < 81u) ? TI_IDX_UP[i] : 0xFFu;
        T.ci = (i < 81u) ? TI_IDX_5814[(cls182(I.k7, 1u, I.ktype) >= 1u && cls182(I.k7, 1u, I.ktype) <= 3u) ? cls182(I.k7, 1u, I.ktype) : 0u][i] : 0xFFu;
        if (T.ui >= 7u || T.ci >= 28u) {
            W.err_n++;
        }
    }
    if (T.cls == 0u) {
        T.di = (i < 81u) ? TI_IDX_DN[i] : 0xFFu;
        if (I.bd17 == 4u) {
            const uint32_t j = (uint32_t)((I.c27 > 1u) ? (I.c27 - 1u) : 0u) * 9u + I.c27;
            T.d4i = (j < 81u) ? TI_IDX_DN4[j] : 0xFFu;
        }
        if (T.di >= 18u) {
            W.err_n++;
        }
    }
    /* 0x117E98 at the start: BD78 = the turbine / 32 byte (upshift / garage) or the new gear's sync speed / 32 */
    T.bd78 = clamp_u8((int32_t)I.turb >> 5);
    W.arm_n[bd16 & 3u]++;
}

/* 0xE5FF6: the end of the shift (the ramp-out of a negative output) */
static void disarm(void)
{
    uint8_t len = 0u;
    if (T.cls == 2u) {
        const uint32_t *g = blk_gear();
        len = (g != 0 && g[4u / 4u] != 0u) ? (uint8_t)cal_u8(g[4u / 4u]) : 0u;
    } else if (T.cls == 1u) {
        const uint32_t *ub = blk_up();
        len = (ub != 0 && ub[0x24u / 4u] != 0u) ? (uint8_t)cal_u8(ub[0x24u / 4u]) : 0u;
    } else {
        const uint32_t *b = blk_5892(cls182(I.k7, 1u, I.ktype));
        len = (b != 0 && b[1] != 0u) ? cal_u8(b[1]) : 0u;               /* 0xE7EB0 [I: the down-block ramp-out length] */
        W.si_misc_n++;
    }
    T.rlen = len;
    T.armed = 0u;
    if (T.out < 0) {
        T.v20 = T.out;                                   /* TI+0x20 = the last negative output */
    } else {
        T.v20 = 0;
        if (T.out > 0) {
            W.si_misc_n++;                               /* a positive end value: the increase ramp-out is not ported */
        }
    }
    if (I.pf != 1u) {
        T.v20 = 0;                                       /* the power flow is not 1: no ramp (0xE5FF6) */
    }
    T.rc = 0u;
    if (I.pf != 1u || T.out < 1) {
        T.a22 = 0;
    }
    T.phase = 0u;
    W.end_n++;
}

void ti_shadow_init(void)
{
    uint8_t *p = (uint8_t *)&T;
    uint32_t i;
    for (i = 0u; i < (uint32_t)sizeof(T); i++) {
        p[i] = 0u;
    }
    T.fld_req = 0x1FFEu;
    T.fld_lim = ti_field13((int32_t)(TI_SI_LIM_CONV / 10) * 4);
    T.e3de_en = cal_u8(TI_E3DE_EN);
}

void ti_shadow_pass(const ti_in_t *in)
{
    int32_t t3q;
    int32_t dq;
    int32_t req;
    int16_t e3de = 0;
    I = *in;
    if (in->os_mode < 1u || in->os_mode > 3u) {
        W.skip_mode_n++;
        return;
    }
    W.pass_n++;
    /* the conditioned inputs of the tables: 0x89C1C = clamp_u8(torque / 6 + 34), 0x89C18 = fluid byte + 8, 0x89C12 = engine / 32 [I] */
    T.c1c = clamp_u8((int32_t)I.tq / 6 + 34);
    T.c18 = clamp_u8((int32_t)I.temp + 8);
    T.c12 = clamp_u8((int32_t)I.eng >> 5);
    T.c0e = clamp_u8((int32_t)I.turb >> 5);
    /* the shift start / end: BD16 = 1 up / 2 down / 3 garage arms (0xED92E -> 0xE5A20, 0xEDC5A -> 0xE5E48);
     * its return (0xEBB6C / 0xEBC60 -> 0xE5FF6) ends */
    if (I.bd16 != T.bd16_last) {
        if (I.bd16 >= 1u && I.bd16 <= 3u) {
            arm(I.bd16);
        } else if (T.armed != 0u) {
            disarm();
        }
        T.bd16_last = I.bd16;
    }
    /* BD78 (ctx+0xC, 0x117E98): the turbine peak / 32 of an up-shift or garage (a running maximum), the new gear's sync speed / 32 of a down-shift */
    if (T.armed != 0u) {
        if (I.bd16 == 2u) {
            T.bd78 = clamp_u8((int32_t)I.sync_to >> 5);
        } else if (T.c0e > T.bd78) {
            T.bd78 = T.c0e;
        }
    }
    run();                                               /* slot 36: 0xDE0D8 -> 0xE5EAA */
    /* slot 40 (0xE6144): the per-pass counters */
    if (T.pc < 0xFFFFu) { T.pc++; }
    if (T.tmr < 0xFFFFu) { T.tmr++; }
    if (T.t1a < 0xFFFFu) { T.t1a++; }
    if (T.rc < 0xFFu) { T.rc++; }
    if (T.pa < 0xFFu) { T.pa++; }
    if (T.out < 0) { W.red_n++; }
    if (T.out > 0) { W.inc_n++; }
    /* 0xDDF84 / 0xDDF98: combine with E3DE (disabled by the calibration enable byte 0x1FE8CB on this unit: 0) */
    if (T.e3de_en != 0u) {
        W.si_misc_n++;                                   /* E3DE enabled but not ported (0xE9508 / 0xE95A2) */
    }
    T.delta = T.out;
    if (T.out >= 0 && T.out < e3de) {
        T.delta = e3de;
    }
    if (T.out == 0) {
        T.mode = (uint8_t)((e3de == 0) ? 0u : 1u);
    } else if (T.out < 0) {
        T.mode = 2u;
    } else {
        T.mode = 1u;
    }
    /* 0xD31D0: 0xD38B4 (the request), 0xD385C (pass-through), 0xD3208 (the type) */
    t3q = (int32_t)I.t3 * 4;
    dq = (int32_t)T.delta * 4;
    req = t3q;
    if (T.mode == 1u && T.delta > 0) {
        req = t3q + dq;
    }
    if (T.mode == 2u && dq < req - t3q) {
        req = t3q + dq;
    }
    if (I.b5e2 == 1u && I.b5e1 != 0u) {
        req = ((int32_t)I.t3 + (int32_t)I.b5e1) * 4;     /* 0xD38B4: B5E2 == 1 and B5E1 != 0 -> T3 + B5E1 [units I] */
    }
    if (I.e330 < 0 && I.t3 > 0) {
        req = (-(int32_t)I.e330 * 4 >= req) ? 0 : req + (int32_t)I.e330 * 4;   /* the E330 term (0xD38B4) */
    }
    if (t3q < req && TI_R23_HELD != 0u) {
        req = t3q;                                       /* an increase is cancelled while reaction 23 is active */
        W.r23_n++;
    }
    T.req_q = sat16(req);
    T.dreq_q = sat16(req - t3q);
    if (req > t3q) {
        T.type = (uint8_t)((TI_R23_HELD == 0u) ? 1u : 0u);
    } else if (req < t3q) {
        T.type = 2u;
    } else {
        T.type = 0u;
    }
    if (T.type != 0u) {
        W.req_n++;
    }
    /* 0xA32E4 + the safety packer: the limit (the converter's) and the 13-bit fields */
    T.lim_nm = (int16_t)(TI_SI_LIM_CONV / 10);
    T.fld_req = (T.type == 0u) ? 0x1FFEu : ti_field13(T.req_q);
    T.fld_lim = ti_field13((int32_t)T.lim_nm * 4);
}

const volatile ti_t *ti_shadow_state(void) { return &s_ti; }
