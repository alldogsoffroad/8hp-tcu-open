/* wk_shadow.c - the READ-ONLY torque-converter lock-up (WK) shadow (app/wk_shadow.h).
 * (2026-11-20, ~/tcu_sweep/wk_shadow_port.md) It includes no live module, names no live cell,
 * calls no live function and has no volatile pointer: the inputs arrive BY VALUE (app/wk_hook.c)
 * and the only RAM it writes is its own (.wk_shadow). The stock calibration at 0x80000 and above
 * is read in place (app/cal_inplace.h); the two tables from below 0x80000 (the WK curve 0xE13C and
 * the NC temperature map 0xE214) are the generated copies of app/wk_cal.c. */
#define TCU_WK_SHADOW_TU 1
#include "app/wk_shadow.h"
#include "app/wk_cal.h"
#include "app/cal_inplace.h"

static wk_t K __attribute__((section(".wk_shadow")));   /* linker.ld: the lock-up object + the result */
#define I  K.in
#define W  K.w

/* ── small helpers (0xB785C / 0xB7888 / 0xB7786 / 0xB7910 / 0xB77A0 / 0xB797C) ─────────── */
static uint16_t sat_u16(int32_t v) { return (v < 0) ? 0u : ((v > 0xFFFF) ? 0xFFFFu : (uint16_t)v); }
static int16_t  sat_s16(int32_t v) { return (v > 32767) ? (int16_t)32767 : ((v < -32768) ? (int16_t)-32768 : (int16_t)v); }
static int16_t  f2s16(float f)     /* 0xB7910 */
{
    if (!(f > 32767.0f) && !(-32768.0f > f)) {
        return (int16_t)(int32_t)f;
    }
    return (f > 32767.0f) ? (int16_t)0x7FFF : (int16_t)-0x8000;
}
static int16_t abs16(int16_t v) { return (v < 0) ? (int16_t)-v : v; }     /* 0xB77A0 */
static float clampf(float v, float lo, float hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); }  /* 0xB797C */

/* 0xB6EAE over the array form of 0xE13C: {n, x[n], y[n]} */
static uint16_t curve_u16(const uint16_t *a, uint16_t x)
{
    const int32_t n = (int32_t)a[0];
#define X(i) a[1 + (i)]
#define Y(i) a[1 + n + (i)]
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

/* 0xB7164 over the array form of 0xE214: {nx, ny, x[nx], y[ny], z[ny][nx]} (s16) */
static int32_t map_s16(const int16_t *a, int16_t x, int16_t y)
{
    const int32_t nx = (int32_t)a[0];
    const int32_t ny = (int32_t)a[1];
#define X(i) ((int32_t)a[2 + (i)])
#define Y(i) ((int32_t)a[2 + nx + (i)])
#define Z(i) ((int32_t)a[2 + (i)])
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
        const int32_t b0 = nx * ky + nx + ny + kx;
        int32_t a0 = Z(b0);
        int32_t c;
        if (fx == 0.0f) {
            c = (int32_t)(int16_t)Z(b0 + nx);
        } else {
            const int32_t z01 = Z(b0 + 1);
            int32_t d = a0 - z01;
            if (z01 < a0) {
                d = -(int32_t)(((float)d * fx) / dx);
            } else {
                d = (int32_t)(((float)-d * fx) / dx);
            }
            a0 = d + a0;
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
            const int32_t a16 = (int16_t)a0;
            const int32_t d = a16 - c;
            if (a16 <= c) {
                return (int32_t)(int16_t)(int32_t)(((float)-d * fy) / dy) + a16;
            }
            a0 = a16 - (int32_t)(int16_t)(int32_t)(((float)d * fy) / dy);
        }
        return a0;
    }
#undef X
#undef Y
#undef Z
}

/* 0xB7164 on a map IN PLACE: {u32 nx, u32 ny, s16 x[nx], s16 y[ny], s16 z[ny][nx]} at the stock address m */
static int32_t map_s16c(uint32_t m, int16_t x, int16_t y)
{
    const int32_t nx = (int32_t)cal_u32(m);
    const int32_t ny = (int32_t)cal_u32(m + 4u);
#define X(i) ((int32_t)cal_s16(m + 8u + 2u * (uint32_t)(i)))
#define Y(i) ((int32_t)cal_s16(m + 8u + 2u * (uint32_t)(nx + (i))))
#define Z(i) ((int32_t)cal_s16(m + 8u + 2u * (uint32_t)(i)))
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
        const int32_t b0 = nx * ky + nx + ny + kx;
        int32_t a0 = Z(b0);
        int32_t c;
        if (fx == 0.0f) {
            c = (int32_t)(int16_t)Z(b0 + nx);
        } else {
            const int32_t z01 = Z(b0 + 1);
            int32_t d = a0 - z01;
            if (z01 < a0) {
                d = -(int32_t)(((float)d * fx) / dx);
            } else {
                d = (int32_t)(((float)-d * fx) / dx);
            }
            a0 = d + a0;
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
            const int32_t a16 = (int16_t)a0;
            const int32_t d = a16 - c;
            if (a16 <= c) {
                return (int32_t)(int16_t)(int32_t)(((float)-d * fy) / dy) + a16;
            }
            a0 = a16 - (int32_t)(int16_t)(int32_t)(((float)d * fy) / dy);
        }
        return a0;
    }
#undef X
#undef Y
#undef Z
}

/* 0xB6E10 on a 1-D s16 curve IN PLACE: {u32 n, s16 x[n], s16 y[n]} */
static int16_t curve_s16c(uint32_t m, int16_t x)
{
    const int32_t n = (int32_t)cal_u32(m);
#define CX(i) ((int32_t)cal_s16(m + 4u + 2u * (uint32_t)(i)))
#define CY(i) ((int32_t)cal_s16(m + 4u + 2u * (uint32_t)(n + (i))))
    if (x <= CX(0)) {
        return (int16_t)CY(0);
    }
    if (x >= CX(n - 1)) {
        return (int16_t)CY(n - 1);
    }
    {
        int32_t j = 1;
        while (j < n - 1 && x > CX(j)) {
            j++;
        }
        {
            const int32_t i = j - 1;
            const float num = (float)((int32_t)x - CX(i));
            const float den = (float)(CX(j) - CX(i));
            if (CY(j) >= CY(i)) {
                return (int16_t)(CY(i) + (int32_t)(((float)(CY(j) - CY(i)) * num) / den));
            }
            return (int16_t)(CY(i) - (int32_t)(((float)(CY(i) - CY(j)) * num) / den));
        }
    }
#undef CX
#undef CY
}

/* 0xB6F5C: float curve {u32 n; f32 x[n]; f32 y[n]} in the stock calibration */
static float curve_f(uint32_t m, float x)
{
    const int32_t n = (int32_t)cal_u32(m);
#define FX(i) cal_f32(m + 4u + 4u * (uint32_t)(i))
#define FY(i) cal_f32(m + 4u + 4u * (uint32_t)(n + (i)))
    int32_t k = 0;
    float fr = 0.0f;
    if (!(x > FX(0))) {
        return FY(0);
    }
    if (!(FX(n - 1) > x)) {
        return FY(n - 1);
    }
    while (FX(k + 1) < x) {
        k++;
    }
    fr = (x - FX(k)) / (FX(k + 1) - FX(k));
    return FY(k) + (FY(k + 1) - FY(k)) * fr;
#undef FX
#undef FY
}

/* 0xB7472: float 2-D map {u32 nx; u32 ny; f32 x[nx]; f32 y[ny]; f32 z[ny][nx]} */
static float map_f(float px, float py, uint32_t m)
{
    const int32_t nx = (int32_t)cal_u32(m);
    const int32_t ny = (int32_t)cal_u32(m + 4u);
    const uint32_t xs = m + 8u;
#define F(i) cal_f32(xs + 4u * (uint32_t)(i))
    int32_t kx = 0, ky = 0;
    float dx, dy;
    if (F(0) < px) {
        if (px < F(nx - 1)) {
            int32_t p = 0;
            while (F(p) < px) {
                p++;
            }
            px = px - F(p - 1);
            dx = F(p) - F(p - 1);
            kx = p - 1;
        } else {
            dx = 1.0f;
            px = 1.0f;
            kx = nx - 2;
        }
    } else {
        dx = 0.0f;
        kx = 0;
        px = 0.0f;
    }
    if (F(nx) < py) {
        if (py < F(nx + ny - 1)) {
            int32_t p = nx;
            while (F(p) < py) {
                p++;
            }
            py = py - F(p - 1);
            dy = F(p) - F(p - 1);
            ky = p - 1 - nx;
        } else {
            dy = 1.0f;
            py = 1.0f;
            ky = ny - 2;
        }
    } else {
        ky = 0;
        py = 0.0f;
        dy = F(nx + 1) - F(nx);
    }
    {
        const int32_t b0 = nx * ky + nx + ny + kx;
        float a = F(b0);
        float c;
        if (px == 0.0f) {
            c = F(b0 + nx);
        } else {
            float d = a - F(b0 + 1);
            if (F(b0 + 1) < a) {
                a = a - (d * px) / dx;
            } else {
                a = (-d * px) / dx + a;
            }
            c = F(b0 + nx);
            {
                const float c1 = F(b0 + nx + 1);
                const float e = c - c1;
                if (c1 < c) {
                    c = c - (e * px) / dx;
                } else {
                    c = (-e * px) / dx + c;
                }
            }
        }
        if (py != 0.0f) {
            if (a <= c) {
                return (-(a - c) * py) / dy + a;
            }
            a = a - ((a - c) * py) / dy;
        }
        return a;
    }
#undef F
}

/* ── the WK current law (0xE54A4 -> 0x107FB8; id 5, type 0 = normally closed) ─────────── */
uint16_t wk_law(uint16_t p, uint8_t mode, uint8_t temp_byte, uint16_t line, int16_t offset)
{
    if (mode != 0u) {                                   /* 0x10809C: the binary law, the NC roles */
        if (p == 0u) {
            return 500u;                                /* i_min (0xD9CBC) */
        }
        if (p >= line || p >= 12750u) {
            return 8500u;                               /* i_max (0xD9CEE) */
        }
        W.fault_n++;                                    /* 0x1C2 (counted, not raised) */
        return 500u;
    }
    {                                                   /* 0x107FE0: curve 0xE13C + the converter offset (0xBA310) */
        const int32_t r14 = (int32_t)curve_u16(WK_E13C, p) + (int32_t)offset;
        const int16_t s = sat_s16(r14);
        const int32_t corr = (int16_t)map_s16(WK_E214, s, (int16_t)temp_byte);   /* 0xD9F96 (type 0 = NC) */
        /* 0xD9F10 (type 0): the map CAL_WK_ZMAP, z all 0 (verified by the test), adds 0 */
        const int32_t v = corr + r14;
        const uint16_t u = sat_u16(v);
        return (u < 500u) ? 500u : ((u > 8500u) ? 8500u : u);   /* 0x10810A: [i_min, i_max] */
    }
}

/* ── the producer gate 0xDCE40 / 0xDCD38 (1:1) ─────────────────────────────────────────── */
bool wk_gate(const wk_in_t *in)
{
    const bool fwd = (in->c30 == 1u && in->c2f == in->c30);
    const bool dcd38 = (in->e61c_busy == 0u && in->c3d == 0u);          /* 0xBB13E(E61C, 5) == 0 && 0x89C3D == 0 */
    return (fwd || in->e612_a == 1u) && dcd38 && in->cf6 == 0u && in->e612_b == 1u;
}

/* ── the request level 0xDFD2C: the 1-D table cal 0x19F410 = [0, 1, 2, 3] indexed by 0xFFF8A0A1 ── */
static uint8_t request_level(void)
{
    const uint32_t n = cal_u32(CAL_WK_REQTAB);
    uint8_t lvl = 0u;
    W.si_req_n++;                                       /* the index (0xFFF8A0A1) is a by-value input: the shift-map shadow's lock-up producer */
    if ((uint32_t)I.req_idx < n) {
        lvl = cal_u8(CAL_WK_REQTAB + 4u + I.req_idx);
    }
    if (I.req_force0 != 0u) {
        lvl = 0u;                                       /* E352+1 / +2 */
    }
    return lvl;
}

/* the traction flag (0xDF922 / 0xDFAAE): the turbine torque against +-100 Nm, in between the slip against +-5 */
static void traction(void)
{
    const int16_t hi = cal_s16(CAL_WK_TRAC_HI);
    const int16_t lo = cal_s16(CAL_WK_TRAC_LO);
    const int16_t sl = (int16_t)cal_u8(CAL_WK_TRAC_SLIP);
    if (I.tq >= hi) {
        K.trac = 1u;
    } else if (I.tq <= lo) {
        K.trac = 0u;
    } else if (K.slip >= sl) {
        K.trac = 1u;
    } else if (K.slip <= (int16_t)-sl) {
        K.trac = 0u;
    }
}

/* ── the predicates of 0xDEA5E (each by its stock address; by-value terms counted) ─────── */
static bool abort_flag(void) { return I.e353 != 0u; }                     /* 0xDF58C */

static bool range_change(void)                                           /* the 0xDFB7C / traction-changed term of 0xDF248 / 0xDEA5E case 4 */
{
    W.si_flag_n++;
    return I.shift_busy != 0u || K.trac != K.trac_last;
}

static bool entry_ok(void)                                               /* 0xDF3E4 */
{
    const float thr = cal_f32(CAL_WK_FLUID0);
    const float hys = cal_f32(CAL_WK_FLUID_HYS);
    const float t = (float)I.temp_c;
    const bool warm = (K.fl_ok != 0u) ? (t > thr - hys) : (t > thr);     /* fluid > 0 C, 5 C hysteresis */
    const bool overheat = t > cal_f32(CAL_WK_OVERHEAT) || t > cal_f32(CAL_WK_OVERHEAT2);
    const bool slip_open = (float)K.slip > -100.0f;                       /* B5E0 set: slip > -100 (0x1A612C) */
    bool ok;
    K.fl_ok = warm ? 1u : 0u;
    W.si_flag_n++;                                                        /* 0xB6AC / E61A / E359 / B71A / the +0x254/6 timers */
    ok = K.req != 0u && warm && I.shift_busy == 0u && I.range_ok != 0u &&
         I.eng > cal_u16(CAL_WK_ENG) && I.turb > cal_u16(CAL_WK_TURB);
    return ok && (overheat || slip_open);
}

static bool close_req(void)                                              /* 0xDF61C / 0xDF63A: E353 or no request or inhibit */
{
    return abort_flag() || K.req == 0u || K.inhibit != 0u;
}

static bool lvl_ramp_done(void)                                          /* 0xDF1FE */
{
    return !(K.t_pre != 0u);                                             /* (+0x220 == 0, B6DA != 1 stand-in) */
}

static bool df2b8(void)                                                  /* 5/6/7/8 -> 9: request 3, |B6CA| <= 10, |slip| <= 10 */
{
    const int16_t thr = (int16_t)cal_u8(CAL_WK_SLIP_A);
    W.si_filt_n++;                                                       /* B6CA (the slip gradient) = 0 */
    return K.req == 3u && abs16(0) <= thr && abs16(K.slip) <= thr;
}

static bool df248(void)                                                  /* -> 7: the range test */
{
    const uint8_t st = K.state;
    W.si_flag_n++;                                                       /* 0xB6AD / 0xB6AC (the cfg D10+0x40 = 0 tests B6AD == 0): stand-in 0 */
    return range_change() && (st == 5u || st == 6u || st == 10u) && cal_u8(CAL_WK_CFG40) <= 2u;
}

static bool df742(void)                                                  /* -> 6 */
{
    const int16_t thr = (int16_t)cal_u8(CAL_WK_SLIP_D);
    return K.req == 1u && ((thr < K.slip && K.trac != 0u) || (K.slip < (int16_t)-thr && K.trac == 0u) || K.state == 5u);
}

static bool df7a6(void)                                                  /* -> 8 */
{
    const int16_t thr = (int16_t)cal_u8(CAL_WK_SLIP_C);
    return (K.req == 2u || K.req == 3u) && (abs16(K.slip) <= thr || K.state == 5u);
}

static bool df30e(void)                                                  /* 10 -> 6 */
{
    const int16_t thr = (int16_t)cal_u8(CAL_WK_SLIP_B);
    return ((thr < K.slip && K.trac != 0u) || (K.slip < (int16_t)-thr && K.trac == 0u) || K.t_s10 == 0u) && K.req < 3u;
}

static bool df3a4(void)                                                  /* 10 -> 9 */
{
    const int16_t thr = (int16_t)cal_u8(CAL_WK_SLIP_A);
    return ((K.slip <= thr && K.trac != 0u) || ((int16_t)-thr <= K.slip && K.trac == 0u)) && K.req == 3u;
}

static bool df21c(void)                                                  /* 7 -> 6: the range test (0xDFC5E) ended */
{
    W.si_flag_n++;
    return I.shift_busy == 0u && (cal_u8(CAL_WK_CFG3C) != 0u);           /* (or B6AD == 0: stand-in 0) */
}

static bool release_done(void)                                           /* 0xDF594 (state 0xB -> 1) */
{
    if (abort_flag()) {
        return true;
    }
    if (cal_u8(CAL_WK_CFG278) != 0u) {
        return false;
    }
    return !(K.p_mbar > (uint16_t)f2s16(cal_f32(CAL_WK_REL_BAR) * 1000.0f));
}

/* ── the pressure ceiling and the slip controller (0x10A3A0; states 9 / 10) ───────────── */
static float ceiling_bar(void)
{
    const float lbar = (float)I.line / 1000.0f;
    const float c = curve_f(CAL_WK_CEIL, (float)I.line);                  /* D10+0x34 = 12.5 bar everywhere */
    return (lbar > c) ? lbar : c;
}

static void slip_entry(void)
{
    K.slip_run = 0u;
    K.integ = K.p_bar;                                                    /* the ramp-in starts at the pressure it takes over */
}

static void slip_control(void)
{
    const float target = cal_f32(CAL_WK_TARGET);
    const float ki = cal_f32(CAL_WK_KI) * 0.01f;                          /* 0.05 x 0.01 = 0.0005 bar / rpm / pass */
    K.ff = map_f((float)I.tq, (float)I.temp_c, CAL_WK_FF_MAP);            /* feed-forward 1.1 .. 6.5 bar */
    if (K.slip_run == 0u) {                                               /* ramp-in [I direction: down toward FF] */
        const float rate = 0.01f * curve_f(CAL_WK_RAMP, (float)I.tq);     /* 0.02 .. 0.05 bar per pass */
        K.integ -= rate;
        if (K.integ <= K.ff || (float)K.slip >= target) {                 /* at FF, or as soon as the slip appears [I] */
            K.integ = K.ff;
            K.slip_run = 1u;
        }
        K.p_bar = K.integ;
    } else {                                                              /* run: the I-only PID toward 40 rpm */
        K.p_bar += ki * ((float)K.slip - target);
    }
    K.p_bar = clampf(K.p_bar, 0.0f, K.ceil);
}

/* ── the output stage 0xDF004 ──────────────────────────────────────────────────────────── */
static void output_stage(void)
{
    K.p_mbar = (uint16_t)sat_u16((int32_t)f2s16(K.p_bar * 1000.0f));
    if (K.b4ac == 0u) {
        const float m = map_f(K.p_bar, (float)I.line, CAL_WK_OUTMAP);     /* D10+0x48: ~ identity */
        const float c = curve_f(CAL_WK_CEIL, (float)I.line);
        K.b2aa = (uint16_t)f2s16(clampf(m, -2.0f, c) * 1000.0f);
        K.b2ac = (int16_t)(f2s16(clampf(m, -2.0f, c) * 1000.0f) - sat_s16((int32_t)cal_s16(CAL_WK_B2AC) * 100));
    } else {
        K.b2aa = 0u;
        K.b2ac = 0;
    }
}

/* ── the state machine 0xDEA5E ─────────────────────────────────────────────────────────── */
static void go(uint8_t s)
{
    K.state = s;
    if (s <= 12u) {
        W.seen = (uint16_t)(W.seen | (1u << s));
    }
    W.trans_n++;
}

static void enter_open(void)                     /* case 1's action 0x10B248 / the resets of the abort path */
{
    K.closed = 0u;
    K.b4ac = 1u;
    K.p_bar = 0.0f;
    K.t_pre = 0u;
    K.t_fill = 0u;
    K.t_s10 = 0u;
    K.slip_run = 0u;
    K.integ = 0.0f;
    K.fill_len = 0u;
    K.t_hold = 0u;
    K.ol24 = 0.0f;
    K.ol28 = 0.0f;
}

/* ── the open-loop ramps (gaps3_decode.md section 2; the calibration in place) ───────────────────────────────── */
/* 0x10BB42 (state 1 -> 2): OL byte 0, B2A8 0, B294+0 0; OL+0x40 = the prefill time (D10+0x1F0 over B284 x B60C, both stood in 0:
 * the map is all 0), OL+0x28 = OL+0x40 / 100, OL+0x24 = 0 */
static void ramp_prefill_entry(void)
{
    const int32_t v = map_s16c(CAL_WK_PRE_MAP, 0, 0);
    K.t_pre = (v < 0) ? 0u : sat_u16(v);
    K.ol28 = (float)K.t_pre / 100.0f;
    K.ol24 = 0.0f;
    K.p_bar = 0.0f;
    W.si_flag_n++;                                                        /* B284 / B60C by value 0 */
}

/* 0x10BB8A (state 2): OL+0x24 += 0.01 (up to OL+0x28), OL+0x40--, p = the curve D10+0x1F4 of the fraction (0 .. 100) */
static void ramp_prefill_pass(void)
{
    float c24 = K.ol24 + 0.01f;
    float den = K.ol28;
    float fr;
    if (c24 > K.ol28) {
        c24 = K.ol28;
    }
    K.ol24 = c24;
    if (K.t_pre != 0u) {
        K.t_pre--;
    }
    if (den < 0.01f) {
        den = 0.01f;
    }
    fr = clampf(c24 / den, 0.0f, 100.0f);
    K.p_bar = curve_f(CAL_WK_PRE_CRV, fr);
}

/* 0x10BC24 (state 2 -> 3): the fill length OL+0x38 = sat(map D10+0x1E8 (slip x engine) + curve D10+0x1EC (temperature byte)
 * + [B294+0x2D == 0: map D10+0x200 (temperature byte x E5C0, stood in 0)]); OL+0x34 = the adaptation read 0xAEE0E (held
 * 0: the EEPROM-backed store, never ported); OL+0x36 = +0x38 + +0x34; OL+0x42 = 0 then ++ (1); p = D098 + the B532 curve */
static void ramp_fill_entry(void)
{
    int32_t len = map_s16c(CAL_WK_FILL_LEN, K.slip, (int16_t)I.eng);
    len += (int32_t)curve_s16c(CAL_WK_FILL_TMP, (int16_t)I.temp_byte);
    if (K.fl_ok == 0u) {
        len += map_s16c(CAL_WK_FILL_M2, (int16_t)I.temp_byte, 0);
    }
    K.fill_maps = sat_u16(len);
    K.fill_len = sat_u16((int32_t)K.fill_maps + 0);                       /* + OL+0x34: 0xAEE0E held 0 */
    K.t_fill = 1u;
    K.closed = 0u;
    K.p_bar = WK_SI_D098_BAR + curve_f(CAL_WK_FILL_P, 0.0f);
    W.si_ramp_n++;                                                        /* D098 (0x108652) is the stand-in */
}

/* 0x10B534 (state 3 -> 4): OL+0x40 = the engage time (D10+0x1F8, or +0x1FC while B2BE (0): temperature byte x slip),
 * OL+0x28 = / 100, OL+0x24 = 0, OL+0x10 = 0x10B79E (the target: stand-in 0), OL+0x30 = its slope (stand-in: to the ceiling
 * over OL+0x28), p = D098 + OL+0x10, OL+0x46 = the hold timer (D10+0x168: turbine x slip) */
static void ramp_engage_entry(void)
{
    const int32_t v = map_s16c(CAL_WK_ENG_T0, (int16_t)I.temp_byte, K.slip);
    K.t_pre = (v < 0) ? 0u : sat_u16(v);
    K.ol28 = (float)K.t_pre / 100.0f;
    K.ol24 = 0.0f;
    K.tgt = 0.0f;
    K.slope = (K.ol28 > 0.0f && K.ceil > WK_SI_D098_BAR) ? (K.ceil - WK_SI_D098_BAR) / K.ol28 : 0.0f;
    K.p_bar = WK_SI_D098_BAR + K.tgt;
    K.closed = 0u;
    {
        const int32_t h = map_s16c(CAL_WK_ENG_HOLD, (int16_t)I.turb, K.slip);
        K.t_hold = (h < 0) ? 0u : sat_u16(h);
    }
    W.si_ramp_n++;                                                        /* D098 / 0x10B79E: stand-ins */
}

/* 0x10B5F8 (state 4, D10+0x234 == 0, the linear form): OL+0x40--, OL+0x46--, p = D098 + target + slope x OL+0x24,
 * OL+0x24 = min(OL+0x24 + 0.01, OL+0x28) */
static void ramp_engage_pass(void)
{
    if (K.t_pre != 0u) {
        K.t_pre--;
    }
    if (K.t_hold != 0u) {
        K.t_hold--;
    }
    if (cal_u8(CAL_WK_ENG_FORM) == 0u) {
        K.p_bar = WK_SI_D098_BAR + K.tgt + K.slope * K.ol24;
    } else {
        W.si_ramp_n++;                                                    /* D10+0x234 == 1: the integrating form is not ported */
    }
    if (K.p_bar > K.ceil) {
        K.p_bar = K.ceil;
    }
    K.ol24 = (K.ol24 + 0.01f >= K.ol28) ? K.ol28 : K.ol24 + 0.01f;
    W.si_ramp_n++;
}

/* 0x10C7E0 (state 7): p = PT1(0, D10+0x248 = 0.25) of the state's own float (reset to 0 at the entry 0x10C7AC) + D098 */
static void ramp_st7(void)
{
    K.p_bar = WK_SI_D098_BAR;
    (void)CAL_WK_ST7;
    W.si_ramp_n++;
}

static void wk_machine(void)
{
    const uint8_t st = K.state;
    K.prev = st;
    switch (st) {
    case 1:
        enter_open();
        if (entry_ok()) {
            go(2);
            K.b4ac = 0u;
            ramp_prefill_entry();
        }
        break;
    case 2:
        if (K.t_pre == 0u) {                                              /* 0xDF738: OL+0x40 == 0 */
            go(3);
            ramp_fill_entry();
        } else if (close_req()) {                                         /* 0xDF61C */
            go(1);
            enter_open();
        } else {
            ramp_prefill_pass();                                          /* 0x10BB8A */
        }
        break;
    case 3:
        if (K.fill_len <= K.t_fill) {                                     /* 0xDF300: OL+0x36 <= OL+0x42 */
            go(4);                                                        /* (0xAEE26: the learner, KEPT OUT) */
            ramp_engage_entry();
        } else if (close_req()) {                                         /* 0xDF63A */
            go(1);
            enter_open();
        } else {
            if (K.t_fill < 0xFFFFu) {
                K.t_fill++;                                               /* 0x10BD7E: OL+0x42++ (the pressure stays) */
            }
        }
        break;
    case 4:
        if (abort_flag()) {                                               /* 0xDF58C */
            go(1);
            enter_open();
        } else if (K.req == 0u || K.inhibit != 0u) {                      /* 0xDF6B4 */
            go(0xB);
            K.closed = 0u;
        } else if (lvl_ramp_done()) {                                     /* 0xDF1FE */
            if (!range_change()) {
                go(5);
                K.closed = 1u;
                K.p_bar = K.ceil;
            } else {
                go(7);
                ramp_st7();
            }
        } else {
            ramp_engage_pass();                                           /* 0x10B5F8 */
        }
        break;
    case 5:
    case 6:
    case 8:
        if (abort_flag()) {
            go(1);
            enter_open();
        } else if (K.req == 0u) {                                         /* 0xDF6F6 (the pressure test: stand-in) */
            go(0xB);
            K.closed = 0u;
            W.si_flag_n++;
        } else if (df2b8()) {
            go(9);
            slip_entry();
        } else if (df248()) {
            go(7);
        } else {
            if (df742()) {
                go(6);
                K.closed = 1u;
            } else if (df7a6()) {
                go(8);
                K.closed = 2u;
            }
            K.p_bar = K.ceil;                                             /* 0x10C8A6 / 0x10C880: the closed pressure (stand-in) */
            W.si_ramp_n++;
        }
        break;
    case 7:
        if (abort_flag()) {
            go(1);
            enter_open();
        } else if (K.req == 0u) {                                         /* 0xDF6EC */
            go(0xB);
            K.closed = 0u;
        } else if (df21c()) {
            go(6);
            K.closed = 1u;
        } else if (df2b8()) {
            go(9);
            slip_entry();
        } else {
            ramp_st7();                                                   /* 0x10C7E0: p = 0.25 PT1 of 0 (state 0) + D098 */
        }
        break;
    case 9:
        if (abort_flag()) {
            go(1);
            enter_open();
        } else if (K.req <= 2u) {                                         /* 0xDF398 */
            go(10);
            K.t_s10 = (uint16_t)WK_SI_S10_N;
        } else {
            slip_control();
        }
        break;
    case 10:
        if (abort_flag()) {
            go(1);
            enter_open();
        } else if (K.req == 0u) {                                         /* 0xDF6CA */
            go(0xB);
            K.closed = 0u;
        } else if (df30e()) {
            go(6);
            K.closed = 1u;
        } else if (df3a4()) {
            go(9);
            slip_entry();
        } else if (df248()) {
            go(7);
        } else {
            if (K.t_s10 != 0u) {
                K.t_s10--;
            }
            K.p_bar += WK_SI_BAR_STEP;                                    /* the ramp-out toward the closed pressure (stand-in) */
            if (K.p_bar > K.ceil) {
                K.p_bar = K.ceil;
            }
            W.si_ramp_n++;
        }
        break;
    case 0xB:
        if (release_done()) {
            go(1);
            enter_open();
        } else {
            /* 0x10B2EA: p += 0.01 x rate x curve(D10+0x224, pedal): the curve is 1.0 everywhere (evaluated in place, pedal stood in 0);
             * the rate (a PT1 of B61C over a map chosen by 0x89CF4) is the stand-in WK_SI_REL_RATE */
            K.p_bar += 0.01f * (-WK_SI_REL_RATE) * curve_f(CAL_WK_REL_CRV, 0.0f);
            if (K.p_bar < 0.0f) {
                K.p_bar = 0.0f;
            }
            W.si_ramp_n++;
        }
        break;
    case 0xC:
        go(1);                                                            /* 0xDF658 (never entered; unreachable) */
        enter_open();
        break;
    default:
        W.err_n++;                                                        /* the stock logs event 1099 (0xB79E4) and resets to 1 */
        go(1);
        enter_open();
        break;
    }
}

/* ── the lock-up torque raise (0xE14EC -> 0xFFF8B5E1 / 0xFFF8B5E2) ───────────────────── */
static void torque_raise(void)
{
    if (cal_u8(CAL_WK_B5E_EN) == 0u || K.state == 1u) {
        K.tq_hold = I.tq;
        K.b5e1 = 0u;
        K.b5e2 = 0u;
        return;
    }
    if (I.tq > K.tq_hold) {
        K.tq_hold = I.tq;
    }
    {
        /* [I units] B542 is taken as 0.1 Nm in the stock: the max-hold minus the torque, in 0.1 Nm, clamped
         * 0 .. 2500, / 10 (the byte, Nm) */
        const int32_t raw = ((int32_t)K.tq_hold - (int32_t)I.tq) * 10;
        const int32_t c = (raw < 0) ? 0 : ((raw > 2500) ? 2500 : raw);
        K.b5e1 = (uint8_t)(c / 10);
    }
    K.b5e2 = (K.f2b != 0u || K.f2c != 0u) ? 1u : 0u;
}

/* ── the 10 ms pass ────────────────────────────────────────────────────────────────────── */
void wk_shadow_pass(const wk_in_t *in)
{
    uint8_t old_closed;
    I = *in;
    W.pass_n++;
    K.slip = (int16_t)((int32_t)I.eng - (int32_t)I.turb);                 /* B5FC+8 (the filtered values = the raw ones) */
    W.si_filt_n++;
    K.gate = wk_gate(&I) ? 1u : 0u;
    if (K.gate != 0u) {
        W.gate_n++;
    }
    if (I.c3d != 0u) {                                                    /* OS mode 1: the reset list (0xE0FE4 -> 0xDE614) */
        W.reset_n++;
        K.state = 1u;
        K.prev = 1u;
        K.closed = 0u;
        K.b4ac = 1u;
        K.b2aa = 0u;
        K.b2ac = 0;
        K.p_bar = 0.0f;
        K.p_mbar = 0u;
        K.req = 0u;
        K.t_pre = 0u;
        K.t_fill = 0u;
        K.t_s10 = 0u;
        K.slip_run = 0u;
        K.integ = 0.0f;
        K.tq_hold = I.tq;
        K.b5e1 = 0u;
        K.b5e2 = 0u;
        K.f2b = 0u;
        K.f2c = 0u;
    } else {                                                              /* the active list: slots 37 .. 39 */
        float t;
        K.req = request_level();
        traction();
        t = (float)I.temp_c;
        K.inhibit = (t > cal_f32(CAL_WK_OVERHEAT) || I.inhibit_x != 0u) ? 1u : 0u;   /* 0xDE688's +0x33 (the other terms: stand-in) */
        K.ceil = ceiling_bar();
        old_closed = K.closed;
        if (abort_flag()) {
            W.abort_n++;
        }
        wk_machine();
        K.f2b = (K.state == 3u) ? 1u : 0u;                                /* DE688: +0x2B / +0x2C */
        K.f2c = (K.state == 4u) ? 1u : 0u;
        if (K.closed != 0u && old_closed == 0u && K.b4ac == 0u) {
            W.eng_cnt_n++;                                                /* the 0 -> 1 edge: 0xBF8E8 would be called (KEPT OUT) */
        }
        if (K.state != 1u) {
            W.perm_n++;
        }
        output_stage();
        torque_raise();
    }
    K.trac_last = K.trac;
    /* the producer's offer (0xDCE40): max(0, B2AA) and B4AC when the gate is open, else 0 and mode 1 */
    K.req_mbar = (K.gate != 0u && (int16_t)K.b2aa > 0) ? K.b2aa : 0u;
    K.mode_out = (K.gate != 0u) ? K.b4ac : 1u;
    K.wk_i = wk_law(K.req_mbar, K.mode_out, I.temp_byte, I.line, 0);
    W.cmp_n++;
    if (K.wk_i != I.live_wk) {
        W.diff_n++;
    }
}

void wk_shadow_init(void)
{
    uint32_t i;
    uint8_t *p = (uint8_t *)&K;                                 /* its own RAM only */
    for (i = 0u; i < (uint32_t)sizeof K; i++) {
        p[i] = 0u;
    }
    K.state = 1u;
    K.prev = 1u;
    K.b4ac = 1u;
    K.mode_out = 1u;
    K.wk_i = 500u;
    W.seen = 2u;
}

const volatile wk_t *wk_shadow_state(void) { return &K; }
uint8_t wk_b5e1(void) { return K.b5e1; }
uint8_t wk_b5e2(void) { return K.b5e2; }
void wk_shadow_note_us(uint32_t us)
{
    W.us_last = us;
    if (us > W.us_max) {
        W.us_max = us;
    }
}
