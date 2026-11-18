/* fb_shadow.c — the READ-ONLY shadow of the stock's PWM-feedback chain
 * (app/fb_shadow.h; ~/tcu_sweep/feedback_chain_decode.md, the port
 * ~/tcu_sweep/feedback_shadow_port.md). Every routine below is ported from
 * the stock bytes (addresses at each); every value lands in the shadow's own
 * struct (fbs_t, section .fb_shadow) — no stock cell, no output, no job.
 *
 * Floating point: single precision in the stock's operation order (the SH-2A
 * FPU, FPSCR.PR = 0); the host test builds with -ffp-contract=off (the C11
 * default) so a*b+c is not fused where the stock used fmul + fadd. The
 * stock's fmac sites (0x839A4, 0x83BF0, 0x83C06, 0x83C2A, 0x83C74, 0x83C7E,
 * 0x83D56) are written as a*b + c: at the inputs that reach them the products
 * are exact in single precision or the difference is below the ftrc step [D].
 */
#define TCU_FB_SHADOW_TU
#include "app/fb_shadow.h"
#include "app/fb_cal.h"
#include "platform/plat_fbhw.h"

#include <stdbool.h>

#if defined(TCU_RAM_APP)
/* the RAM application carries no shadow (its 64 KB limit) */
void fb_shadow_init(void) {}
void fb_shadow_start(void) {}
void fb_shadow_isr(void) {}
void fb_shadow_tick(const fbs_in_t *in) { (void)in; }
const volatile fbs_t *fb_shadow_state(void) { return 0; }
const volatile fbr_t *fb_shadow_rec(void) { return 0; }
#else

#if defined(__sh__)
#define FBS_RAM __attribute__((section(".fb_shadow")))
#else
#define FBS_RAM
#endif

static fbs_t s FBS_RAM;

/* the disagreement recorder (ours, r04d): its own section .fb_rec (linker.ld,
 * 0xFFF85400; ASSERT-bounded), so no .fb_shadow / .bss symbol moves */
#if defined(__sh__)
#define FBR_RAM __attribute__((section(".fb_rec")))
#else
#define FBR_RAM
#endif
static fbr_t s_rec FBR_RAM;

/* ── stand-ins [S] (the dump's values; writers not ported) ─────────────── */
/* 0x9393A, the selected ECU temperature (the a-curve / R_drv model input,
 * 0x93A10 = 0x9393A). STAGE 2a (2026-11-18): FBS_T_ECU_LIVE (the SH
 * application by default, as the selection's cells go live there — bsw/drv/
 * drv_tsel.c) takes it from the live cell, gathered by value (fbs_in_t
 * t_ecu, app/fb_hook.c); a host build may set -DFBS_T_ECU_LIVE=1 for the
 * live semantics. Otherwise the dump's 0x0215 stand-in. */
#if !defined(FBS_T_ECU_LIVE)
#if defined(__sh__) && !defined(TCU_RAM_APP) && !defined(TCU_LOADER)
#define FBS_T_ECU_LIVE 1
#else
#define FBS_T_ECU_LIVE 0
#endif
#endif
#define FBS_9393A_SI    ((int16_t)0x0215)   /* the stand-in: 33.3 °C [S] */
#if defined(FBS_9393A_HOST_SRC) && !defined(__sh__)
/* HOST DRY-RUN ONLY (test_diag_temps, ~/tcu_sweep/diag_temp_shadow_port.md):
 * the selected ECU temperature from the temperature-selection shadow in
 * place of the stand-in; never defined for the target (identical SH code) */
extern volatile int16_t FBS_9393A_HOST_SRC;
#define FBS_TECU(in)    ((int16_t)FBS_9393A_HOST_SRC)
#elif FBS_T_ECU_LIVE
#define FBS_TECU(in)    ((in)->t_ecu)
#else
#define FBS_TECU(in)    FBS_9393A_SI
#endif
#define FBS_9090B       0x20u           /* 0x9090B chip frequency code (dump) */
#define FBS_9334C       0u              /* 0x9334C dither amplitude (0x8347C not ported) */
/* the group-0 table 0x901DC.. (dump: 05 | 00..09 | 00..04) */
#define FBS_NCHIP       5u
static const uint8_t FBS_PAIR[10] = { 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u };   /* 0x901DD */
static const uint8_t FBS_CHIP[5]  = { 0u, 1u, 2u, 3u, 4u };                        /* 0x901E7 */

/* the capture configs 0x40954 as 0x97648 reads them */
typedef struct {
    uint32_t lim;       /* +0 */
    uint32_t pmax;      /* +4 */
    uint16_t grbf;      /* +8 */
    uint8_t  pol;       /* +10 */
} fbs_cfg_t;

static inline fbs_cfg_t cfg_of(uint8_t sub)
{
    fbs_cfg_t c;
    if (sub == 0u) {
        c.lim = FB_CFG0_LIM[0]; c.pmax = FB_CFG0_LIM[1]; c.grbf = FB_CFG0_GRBF[0]; c.pol = FB_CFG0_POL[0];
    } else {
        c.lim = FB_CFG1_LIM[0]; c.pmax = FB_CFG1_LIM[1]; c.grbf = FB_CFG1_GRBF[0]; c.pol = FB_CFG1_POL[0];
    }
    return c;
}

#if defined(FBS_NEG_POLARITY)
/* negative control (host only): both subs' polarity swapped */
#define POL_OF(c) ((uint8_t)((c).pol == 0u ? 1u : 0u))
#else
#define POL_OF(c) ((c).pol)
#endif

static inline uint8_t pin_of(uint8_t chip, uint8_t sub)    /* 0x40D96 */
{
#if defined(FBS_NEG_PINS)
    /* negative control (host only): chip 1's two channels swapped (ch 2 <-> ch 3) */
    if (chip == 1u) {
        sub = (uint8_t)(sub ^ 1u);
    }
#endif
    return FB_PINS[3u * chip + sub];
}

/* ════════════════════════════════════════════════════════════════════════
 * 0x97648 — the capture service (n = Timer F number, st = 0x90334 + 12 ch)
 * [O bytes 0x97648–0x9773E]; called from the ISR (0x9076E) and the task
 * part (0x904FA, under SR.I = 6).
 * ════════════════════════════════════════════════════════════════════════ */
static void cap_97648(uint8_t n, const fbs_cfg_t *c, fbs_st_t *st)
{
    uint8_t fl = (uint8_t)(st->fl & 0xF0u);
    bool got = false;
    uint32_t grc, gra, ecc;
    do {                                        /* 0x97666..0x9767C */
        if ((fbhw_tsrf(n) & 1u) != 0u) {
            fbhw_icff_clear(n);
            got = true;
        }
        grc = fbhw_grcf(n);
        gra = fbhw_graf(n);
        ecc = fbhw_ecntcf(n);
    } while ((fbhw_tsrf(n) & 1u) != 0u);
    ecc >>= 8;
    if (fl == 0u || fl == 0x40u) {              /* 0x976C2 */
        if (got) {
            fl = 0x10u;
            fbhw_grbf_set(n, c->grbf);
            fbhw_icff_clear(n);
            fbhw_icff_clear(n);
            fbhw_icff_clear(n);
        } else if (fl != 0x40u && ecc > c->pmax) {
            fl = 0x40u;
        }
    } else {
        if (got) {                              /* 0x9768E */
            grc >>= 8;
            gra >>= 8;
            st->p = grc;
            st->on = grc - gra;
            fl = (grc > c->pmax) ? 0x31u : 0x21u;
        }
        if (ecc > c->pmax && ecc > c->lim) {    /* 0x976A4..0x976B4 */
            fl = 0x40u;
            if (c->grbf != 0u) {
                fbhw_grbf_set(n, 1u);
                fbhw_ecntbf_set(n, 0u);
                fbhw_icff_clear(n);
                fbhw_icff_clear(n);
            }
        }
    }
    if (fl == 0x40u) {                          /* 0x976EA: the pin level */
        bool high;
        st->p = 0xFFFFFFFFu;
        if (n >= 6u) {
            high = ((uint32_t)fbhw_pfpr() & (1u << (n - 6u))) != 0u;   /* PFPR bit n − 6 */
        } else {
            high = ((uint32_t)fbhw_phpr() & (1u << n)) != 0u;          /* PHPR bit n */
        }
        if (high) {
            st->on = (POL_OF(*c) != 0u) ? 0xFFFFFFFFu : 0u;
        } else {
            st->on = (POL_OF(*c) == 0u) ? 0xFFFFFFFFu : 0u;
        }
    }
    st->fl = fl;
}

/* 0x9076E(w, g = 0) — one window snapshot of every channel of group 0 */
static void snap_9076e(uint8_t w)
{
    for (uint8_t k = 0u; k < FBS_NCHIP; k++) {
        const uint8_t chip = FBS_CHIP[k];
        for (uint8_t sub = 0u; sub < 2u; sub++) {
            const uint8_t ch = FBS_PAIR[2u * k + sub];
            fbs_cfg_t c;
            fbs_st_t *st;
            if (ch == 0xFFu) {
                continue;
            }
            c = cfg_of(sub);
            st = &s.st[ch];
            cap_97648(pin_of(chip, sub), &c, st);
            s.win_p[2u * ch + w] = (uint16_t)(st->p >> 2);
            s.win_a[2u * ch + w] = (uint16_t)((sub == 0u) ? ((st->p - st->on) >> 2) : (st->on >> 2));
        }
    }
    s.win_n[w]++;
}

/* 0x90844(g = 0) — the sequencer; returns "the next compare already passed" */
static bool seq_90844(void)
{
    uint32_t t = s.time;
    s.m9091a = (uint8_t)(s.m9091a - 1u);
    switch (s.state) {
    case 2u:
        t += s.iv[1];
        fbhw_ocrd00_set(t << 8);
        snap_9076e(0u);
        s.state = 3u;
        break;
    case 3u:
        t += s.iv[2];
        fbhw_ocrd00_set(t << 8);
        snap_9076e(1u);
        s.state = 0u;
        break;
    case 1u:
        /* the record bytes 0x91119 / 0x91121, the 0x90830 handshake and the
         * readback job 0x92812: NOT ported (counted) */
        s.st1_n++;
        s.m94434 = 0u;
        if (s.reload != 0u) {
            const uint16_t *row = ((s.m9090b & 0x20u) != 0u) ? FB_IV_190210 : FB_IV_1902D0;
            /* row index (0x9090B & 0x1F) = 0 for the stand-in 0x20 */
            s.reload = 0u;
            s.iv[1] = row[1];
            s.iv[2] = (uint16_t)(row[2] - 500u);
            s.iv[0] = row[0];
        }
        t += s.iv[0];
        fbhw_ocrd00_set(t << 8);
        s.state = 2u;
        break;
    default:
        t += 500u;
        s.m94434 = 0x17u;
        fbhw_ocrd00_set(t << 8);
        s.state = 1u;
        break;
    }
    t &= 0x00FFFFFFu;
    s.time = t;
    return ((t - (fbhw_tcnt1d0() >> 8)) & 0x00FFFFFFu) >= 0x10000u;
}

/* vector 216 → 0x90A72: clear CMFAD00, run 0x90844 until it is not behind */
void fb_shadow_isr(void)
{
    const uint32_t t0 = fbhw_frt_us();
    bool again;
    uint32_t loops = 0u;
    s.isr_n++;
    do {
        fbhw_cmfad00_clear();
        again = seq_90844();
        if (again) {
            loops++;
        }
    } while (again && loops < 64u);     /* ours: a bound (each iteration adds >= 500 µs) */
    s.isr_loops += loops;
    {
        const uint32_t d = fbhw_frt_us() - t0;
        s.isr_last_us = d;
        if (d > s.isr_max_us) {
            s.isr_max_us = d;
        }
    }
}

/* ── curves 0x886AC → 0x8D42C (s16, round half away from zero) ─────────── */
static int16_t curve_s16(const int16_t *tbl, int16_t x)
{
    const int16_t n = tbl[0];
    const int16_t *xs = &tbl[1];
    const int16_t *ys = &tbl[1 + n];
    float f;
    if ((int32_t)x <= (int32_t)xs[0]) {
        f = (float)ys[0];
    } else if ((int32_t)x >= (int32_t)xs[n - 1]) {
        f = (float)ys[n - 1];
    } else {
        int32_t j = 1;
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
    return (int16_t)(int32_t)(f < 0.0f ? f - 0.5f : f + 0.5f);
}

/* 0x886C8: saturating u16 add */
static uint16_t sat_add(uint16_t a, uint16_t b)
{
    const uint32_t r = (uint32_t)a + b;
    return (uint16_t)(r > 0xFFFFu ? 0xFFFFu : r);
}

/* 0x89EDA: the first-order filter state ×10000 (gain coef / 10000 per pass) */
static int16_t filt_89eda(int32_t *st, int16_t duty, uint16_t coef)
{
    const float k = 0.0001f;                    /* 0x89F50 = 0x38D1B717 */
    int32_t x = *st;
    int32_t y = (int32_t)((float)x * k + (x >= 0 ? 0.5f : -0.5f));
    const int32_t diff = (int32_t)duty - (int32_t)(int16_t)y;
    if (diff >= 0) {
        x += (int32_t)((uint32_t)(uint16_t)diff * coef);
    } else {
        x -= (int32_t)((uint32_t)(uint16_t)(-diff) * coef);
    }
    *st = x;
    return (int16_t)(int32_t)((float)x * k + (x >= 0 ? 0.5f : -0.5f));
}

/* one window of 0x89A8C (0x89B1A..0x89BB2 / 0x89BB4..0x89C4A) */
static uint8_t win_class(uint16_t P, uint16_t A, uint16_t pexp, uint16_t m, uint16_t *dout)
{
    int32_t d;
    const uint32_t lo = (uint32_t)pexp - 20u;
    if (P == 0xFFFFu) {
        if (A == 0xFFFFu) {
            *dout = 1000u;
            return 0x20u;
        }
        *dout = 0u;
        return (A == 0u) ? 0x21u : 0x31u;
    }
    if (P == 0u || (int32_t)A >= (int32_t)P) {
        d = 1000;
    } else {
        d = (int32_t)(((float)A * 1000.0f) / (float)P);     /* 0x89CEC = 1000.0f */
    }
    if ((uint32_t)P >= lo && !((uint32_t)P > (uint32_t)pexp + 20u)) {
        if (!((int32_t)A > (int32_t)m)) {
            *dout = 0u;
            return 0x21u;
        }
        if ((int32_t)A >= (int32_t)P - (int32_t)m) {
            *dout = 1000u;
            return 0x20u;
        }
        *dout = (uint16_t)d;
        return 0x10u;
    }
    if (A >= 100u || P > 12000u) {
        if ((uint16_t)d >= 950u && (uint32_t)P > lo * 2u) {      /* the 950 ‰ saturation rule */
            *dout = 1000u;
            return 0x20u;
        }
        *dout = 0u;
        return 0x31u;
    }
    *dout = 0u;
    return 0x21u;
}

static uint16_t ratio(uint16_t a, uint16_t den)    /* sat'd a ≥ den → 1000, else a·1000/den */
{
    if ((int32_t)a >= (int32_t)den) {
        return 1000u;
    }
    return (uint16_t)(int32_t)(((float)a * 1000.0f) / (float)den);
}

/* 0x89A8C(ch) — the classifier, then the duty filter (0x89E58 → 0x89EDA) */
static void cls_89a8c(uint8_t ch)
{
    const uint16_t P0 = s.snap_p[2u * ch], P1 = s.snap_p[2u * ch + 1u];
    const uint16_t A0 = s.snap_a[2u * ch], A1 = s.snap_a[2u * ch + 1u];
    const uint16_t pexp = FB_PEXP[0];           /* 0x9083C (0x8FC5E: cal 0x191490) */
    const uint16_t m = (uint16_t)(int32_t)((float)FB_M[0] * (float)pexp * 0.001f);   /* 0x89CE4 */
    const bool dith = (FBS_9334C != 0u);
    uint16_t duty = s.duty[ch];
    uint8_t cls = s.cls[ch];
    uint8_t cnt = s.hold[ch];
    uint16_t d1, d0;
    const uint8_t c1 = win_class(P1, A1, pexp, m, &d1);
    const uint8_t c0 = win_class(P0, A0, pexp, m, &d0);
    bool decided = true;

    if (c1 == 0x21u && c0 == 0x21u) {
        duty = 0u;
        cls = 0x21u;
    } else if (c1 == 0x20u && c0 == 0x20u) {
        duty = 1000u;
        cls = 0x20u;
    } else if (c1 == 0x10u && c0 == 0x10u) {
        const uint16_t sum = (uint16_t)(P1 + P0);
        duty = (sum == 0u) ? 0u : ratio(sat_add(A1, A0), sum);
        cls = 0x10u;
    } else if (c1 == 0x10u && (c0 == 0x21u || c0 == 0x20u)) {
        const uint16_t s2 = (uint16_t)(P1 + P1);
        if (s2 != 0u) {
            if (dith) {
                duty = ratio(sat_add(A1, (c0 == 0x20u) ? P1 : 0u), s2);
                cls = 0x19u;
            } else {
                duty = d1;
                cls = 0x10u;
            }
        }
    } else if (c0 == 0x10u && (c1 == 0x21u || c1 == 0x20u)) {
        const uint16_t s2 = (uint16_t)(P0 + P0);
        if (s2 != 0u) {
            if (dith) {
                duty = ratio(sat_add(A0, (c1 == 0x20u) ? P0 : 0u), s2);
                cls = 0x18u;
            } else {
                duty = d0;
                cls = 0x10u;
            }
        }
    } else if (c1 == 0x10u && c0 == 0x31u) {
        duty = d1;
        cls = dith ? 0x19u : 0x10u;
    } else if (c0 == 0x10u && c1 == 0x31u) {
        duty = d0;
        cls = dith ? 0x18u : 0x10u;
    } else {
        /* a mixed pair: hold the previous class, 0x31 on the cal-th pass */
        const uint8_t hc = FB_HOLD[0];
        decided = false;
        if ((int32_t)cnt + 1 >= (int32_t)hc && hc > 0u) {
            cls = 0x31u;
        } else if (cnt < 0xFEu) {
            cnt++;
        }
    }
    if (decided && cnt != 0u) {
        cnt--;
    }
    s.d1[ch] = d1;
    s.duty[ch] = duty;
    s.d0[ch] = d0;
    s.cls[ch] = cls;
    s.hold[ch] = cnt;
    /* the filter 0x89E58: cal 0x191294[ch] > 0 */
    if (FB_COEF[ch] > 0) {
        const uint16_t bit = (uint16_t)(1u << ch);
        int16_t out;
        if ((s.finit & bit) == 0u) {
            s.fstate[ch] = (int32_t)(int16_t)duty * 10000;  /* 0x88748(duty, 10000) */
            s.finit = (uint16_t)(s.finit | bit);
            out = (int16_t)duty;
        } else {
            out = filt_89eda(&s.fstate[ch], (int16_t)duty, (uint16_t)FB_COEF[ch]);
        }
        s.dutyf[ch] = (uint16_t)out;
    }
}

/* 0x8390C(ch, 0, I, d) — the current estimate (0.1 mA) [O bytes 0x8390C–0x839EA] */
static void est_8390c(uint8_t ch, uint16_t v, int16_t i, uint16_t d)
{
    float f9 = 0.0f;
    if ((uint16_t)i == 0u) {
        s.est[ch] = 0u;
    } else if (s.res[ch] != 0u) {
        const float a = (float)curve_s16(FB_C_A[ch], s.tdrv);    /* 0x66974: x = 0x93A10 */
        const float b = (float)curve_s16(FB_C_B[ch], s.tfl);     /* 0x6694C: x = 0x94FD6 */
        const float fd = (float)d;
        float f8;
        f9 = 1000.0f - fd;
        f9 = f9 * -a;
        f9 = (float)v * fd + f9;                /* fmac fr0, fr8, fr9 */
        f8 = (float)s.res[ch];
        f9 = f9 * b;
        f8 = f8 * 100.0f;
        f9 = f9 / f8;
        if (0.0f > f9) {
            f9 = 0.0f;
        }
        if (f9 > 32000.0f) {
            f9 = 32000.0f;
        }
    }
    {
        const uint16_t old = s.est[ch];
        const uint16_t e = (uint16_t)(int32_t)f9;
        s.est[ch] = e;
        s.estm[ch] = (uint16_t)(((uint32_t)old + e) >> 1);
    }
}

/* 0x83CA8(ch, flag) — the complement twin of the ambient filter, then the
 * value / complement checks 0x6BDFE (stock: fatal 0x221 on a mismatch) */
static void twin_83ca8(uint8_t ch, uint8_t flag, uint16_t tc)
{
    const float f2 = (float)(int16_t)(uint16_t)~tc * 8.0f;
    float f1;
    s.tflc = tc;                                /* 0x94E4E = 0x94E14 */
    if (s.rfirst2[ch] == 0u) {
        s.rfirst2[ch] = 1u;
        f1 = f2;
    } else {
        f1 = (float)(int16_t)(uint16_t)~s.ambc;
        if (flag != 0u) {
            const float ka = (float)FB_KA[0] * 0.0001f;
            float t = ka * f1;
            t = (1.0f - ka) * f2 + t;           /* fmac fr0, fr7, fr8 */
            f1 = t;
        }
    }
    s.ambc = (uint16_t)~(uint16_t)(int32_t)f1;
    if ((uint16_t)s.tfl != (uint16_t)~s.tflc) {
        s.pair_bad_n++;
    }
    if ((uint16_t)s.amb != (uint16_t)~s.ambc) {
        s.pair_bad_n++;
    }
}

/* 0x83ABC(ch, I, flag) — the coil-resistance thermal model [O bytes 0x83ABC–0x83CA6] */
static void r_83abc(uint8_t ch, int16_t i, uint8_t flag, int16_t t_fluid, int16_t t_ecu)
{
    const float k = 0.0001f;                    /* 0x83CFC */
    float f13, rfix, r20, f7, f5, f1, fi, f10, f8, f4, f15, f14;
    s.r_upd_n++;
    s.tfl = t_fluid;                            /* 0x94FD6 = 0x94F9C */
    s.tdrv = t_ecu;                             /* 0x93A10 = 0x9393A (FBS_TECU) */
    f13 = (float)curve_s16(FB_C_RDRV[ch], s.tdrv);     /* 0x669B8 */
    rfix = (float)FB_RFIX[ch];                  /* @r15 */
    r20 = (float)FB_R20[ch];                    /* @(4,r15) */
    f7 = (float)curve_s16(FB_C_HEAT[ch], s.tfl);       /* 0x669E0 */
    f5 = (float)FB_SPLIT[ch];
    f1 = (float)s.tfl * 8.0f;
    fi = (float)(uint16_t)i;
    f5 = f5 * k;                                /* split s */
    f10 = 1.0f - f5;
    f7 = f7 * fi;
    f7 = f7 * fi;
    f8 = (float)FB_INOM[0];
    f8 = f8 * f8;
    f7 = f7 / f8;                               /* the steady rise at I */
    f4 = f7 * f10;                              /* slow target */
    f15 = f7;
    f7 = f7 * f5;                               /* fast target */
    if (s.rfirst[ch] == 0u) {
        s.rfirst[ch] = 1u;
        f14 = f1;
    } else {
        const float kf = (float)FB_KF[ch] * k;
        const float ks = (float)FB_KS[ch] * k;
        float t;
        f15 = 1.0f - kf;
        f15 = f15 * f7;
        f15 = (float)s.rfast[ch] * kf + f15;    /* fmac fr0, fr8, fr15 */
        f7 = f15;
        t = 1.0f - ks;
        t = t * f4;
        t = (float)s.rslow[ch] * ks + t;        /* fmac fr0, fr6, fr8 */
        f14 = (float)s.amb;
        f4 = t;
        f15 = f15 + t;
        if (flag != 0u) {
            const float ka = (float)FB_KA[0] * k;
            float u = ka * f14;
            u = (1.0f - ka) * f1 + u;           /* fmac fr0, fr1, fr8 */
            f14 = u;
        }
    }
    if (f15 > 27520.0f) {                       /* 0x83D10: 215 °C */
        f5 = f5 * 27520.0f;
        f15 = 27520.0f;
        f10 = f10 * 27520.0f;
        f7 = f5;
        f4 = f10;
    }
    s.rfast[ch] = (uint16_t)(int32_t)f7;
    s.rslow[ch] = (uint16_t)(int32_t)f4;
    s.rsum[ch] = (uint16_t)(int32_t)f15;
    s.amb = (int16_t)(int32_t)f14;
    f14 = f14 + f15;
    twin_83ca8(ch, flag, (uint16_t)~(uint16_t)t_fluid);   /* 0x94E14 = ~0x94F9C */
    {
        float f8b = f14 * 0.3046875f + 9220.0f; /* fmac fr0, fr9, fr8 (0x83D18, 0x83D14) */
        float r;
        f8b = f8b * r20;
        r = f8b * k + f13;                      /* fmac fr0, fr12, fr13 */
        r = r + rfix;
        if (r > 32000.0f) {
            r = 32000.0f;
        }
        s.res[ch] = (uint16_t)(int32_t)r;
    }
}

/* 0x83A1C — the R model's schedule (0x9410D channel, 0x9410E call count,
 * 0x9476E the 2 s countdown, 0x84A21 the ambient flag) */
static void sched_83a1c(const fbs_in_t *in)
{
#if !defined(FBS_NEG_COLDSEED)
    /* OURS (r04c; 2026-11-18 r04b bench): the R model's start waits for a
     * valid fluid temperature. Call 2 seeds the ambient state 0x94FD4 (and
     * its twin 0x94E4C) at 0x94F9C · 8 and from then on follows it only at
     * 1 % per 2 s (cal 0x1916B6 = 9900: τ ≈ 200 s). The stock's 0x94F9C is
     * a real reading at that pass (its ADC chain runs from the init list, so
     * 0x887D8 has a plausible acquisition within its first 5 ms passes);
     * ours is not: our Timer G starts at app 500 ms (r02l's deferred start),
     * so 0x94F9C reads 0 (0x8D4B4's init) and then the 120 °C default
     * 0x0780 (cal 0x1916F0, status 0 / 7 / 3) for ≈ 0.5 s — the r04b seed
     * (amb 15360 → 12001 after 37 rounds at 35.9 °C fluid: 93.8 °C).
     * Holding calls 1-2 until status 8 gives the seed the stock's input; the
     * schedule after it is the stock's, unchanged. */
    if (s.c9410e < 2u && in->t_st != 8u) {
        if (s.r_wait != 0xFFFFu) {
            s.r_wait++;
        }
        return;
    }
#endif
    if (s.c9410e == 0u) {
        s.c9410e = 1u;
        return;
    }
    if (s.c9410e == 1u) {
        for (uint8_t ch = 0u; ch < 10u; ch++) {
            r_83abc(ch, in->i_req[ch], s.c84a21, in->t_fluid, FBS_TECU(in));
            s.c84a21 = 0u;
        }
        s.c9410d = 10u;
        s.cd476e = 400u;
        s.c9410e = 2u;
        return;
    }
    if (s.cd476e == 0u) {
        s.c84a21 = 1u;
        s.cd476e = 400u;
        s.c9410d = 0u;
    }
    if (s.c9410d < 10u) {
        const uint8_t ch = s.c9410d;
        r_83abc(ch, in->i_req[ch], s.c84a21, in->t_fluid, FBS_TECU(in));
        s.c84a21 = 0u;
        s.c9410d++;
    }
}

/* 0x838B0 — the classifier and the estimate per channel */
static bool chain_838b0(const fbs_in_t *in)
{
    if (s.c9410c == 0u) {
        s.c9410c = 1u;
        return false;
    }
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        cls_89a8c(ch);
        est_8390c(ch, in->rail_mv, in->i_req[ch], s.duty[ch]);
    }
    return true;
}

/* 0x904FA's capture part (under 0x9665C(0x60)) */
static void cap_904fa(void)
{
    const uint32_t lk = fbhw_lock6();
    /* the window measured INSIDE the lock (r04c): r04b read the end after
     * the unlock, so the level-6 ISRs held off by the lock (the sampler,
     * ≈ 30 µs; the walk) ran inside the measurement — its 79 µs */
    const uint32_t t0 = fbhw_frt_us();
    for (uint8_t chip = 0u; chip < 5u; chip++) {
        for (uint8_t sub = 0u; sub < 2u; sub++) {
            const uint8_t ch = (uint8_t)(2u * chip + sub);
            const uint8_t n = pin_of(chip, sub);
            const fbs_cfg_t c = cfg_of(sub);
            fbs_st_t *st = &s.st[ch];
            if (n == 0xFFu) {
                continue;
            }
            cap_97648(n, &c, st);
            if ((st->fl & 0xF0u) == 0x40u) {
                const uint16_t lv = (uint16_t)st->on;
                s.win_p[2u * ch] = 0xFFFFu;
                s.win_p[2u * ch + 1u] = 0xFFFFu;
                s.win_a[2u * ch] = (sub == 0u) ? (uint16_t)~lv : lv;
                s.win_a[2u * ch + 1u] = (sub == 0u) ? (uint16_t)~lv : lv;
            }
        }
    }
    s.m9091a = (uint8_t)(s.m9091a + 3u);
    for (uint8_t k = 0u; k < 20u; k++) {
        s.snap_p[k] = s.win_p[k];
        s.snap_a[k] = s.win_a[k];
    }
    {
        const uint32_t d = fbhw_frt_us() - t0;
        if (d > s.lock_max_us) {
            s.lock_max_us = d;
        }
    }
    fbhw_unlock(lk);
}

static uint16_t absdiff(uint16_t a, uint16_t b) { return (uint16_t)(a > b ? a - b : b - a); }

/* the recorder (ours, witness only): one disagreeing (pass, channel) */
static uint8_t fbr_ridx(uint8_t c)
{
    switch (c) {
    case 0x00u: return 0u;
    case 0x10u: return 1u;
    case 0x20u: return 2u;
    case 0x21u: return 3u;
    case 0x31u: return 4u;
    default:    return 5u;
    }
}

static uint8_t fbr_sidx(uint8_t c)
{
    return (c == 0x10u) ? 0u : ((c == 0x21u) ? 1u : 2u);
}

static void fbr_record(uint8_t ch, const fbs_in_t *in)
{
    uint16_t *h = &s_rec.hist[ch][fbr_ridx(s.cls[ch])][fbr_sidx(in->si_cls[ch])];
    if (*h != 0xFFFFu) {
        (*h)++;
    }
    if (s_rec.n_dis != 0xFFFFFFFFu) {
        s_rec.n_dis++;
    }
    {
        const uint32_t t = fbhw_frt_us();
        if (s.cmp[ch].dis_n == 1u) {            /* (dis_n is already counted) */
            s_rec.span[ch].first_us = t;
            s_rec.span[ch].first_pass = (uint16_t)s.tick_n;
        }
        s_rec.span[ch].last_us = t;
        s_rec.span[ch].last_pass = (uint16_t)s.tick_n;
    }
    if (s_rec.n_rec < FBR_N) {
        fbr_ent_t *e = &s_rec.e[s_rec.n_rec];
        e->t_us = fbhw_frt_us();
        e->pass = (uint16_t)s.tick_n;
        e->ch = ch;
        e->real = s.cls[ch];
        e->si = in->si_cls[ch];
        e->mode = in->mode[ch];
        e->duty = s.duty[ch];
        e->i_req = in->i_req[ch];
        e->phase = in->phase;
        e->seq2 = in->seq2;
        e->f114 = in->f114;
        e->f115 = in->f115;
        e->os_mode = in->os_mode;
        s_rec.n_rec++;
    }
}

void fb_shadow_tick(const fbs_in_t *in)
{
    s.tick_n++;
    /* our copy of the 5 ms countdown 0x7C7E4 for the one cell 0x9476E */
    s.cd476e = (uint16_t)((s.cd476e > in->n5) ? (uint32_t)(s.cd476e - in->n5) : 0u);
    cap_904fa();                                /* 0x8EE12 → 0x904FA */
    sched_83a1c(in);                            /* 0x7CBA8 → 0x83A1C */
    if (!chain_838b0(in)) {                     /*         → 0x838B0 */
        return;
    }
    for (uint8_t ch = 0u; ch < 10u; ch++) {     /* ours: the comparison */
        fbs_cmp_t *c = &s.cmp[ch];
        c->si_cls = in->si_cls[ch];
        c->si_duty = in->si_duty[ch];
        c->si_est = in->si_est[ch];
        c->si_res = in->si_res[ch];
        if (s.cls[ch] == in->si_cls[ch]) {
            const uint16_t dd = absdiff(s.duty[ch], in->si_duty[ch]);
            c->agree_n++;
            if (dd > c->dduty_max) {
                c->dduty_max = dd;
            }
        } else {
            c->dis_n++;
            c->dis_real = s.cls[ch];
            c->dis_si = in->si_cls[ch];
            fbr_record(ch, in);
        }
    }
}

void fb_shadow_init(void)
{
    uint8_t *p = (uint8_t *)(void *)&s;
    for (uint32_t k = 0u; k < sizeof s; k++) {
        p[k] = 0u;
    }
    p = (uint8_t *)(void *)&s_rec;
    for (uint32_t k = 0u; k < sizeof s_rec; k++) {
        p[k] = 0u;
    }
    s_rec.cap = (uint16_t)FBR_N;
    s.reload = 1u;                              /* the first state 1 loads the intervals */
    s.m9090b = FBS_9090B;
    s.c84a21 = FB_DTBL_84A21[0];                /* the initialised-data image: 01 */
}

/* 0x90010's arming (bytes 0x900AA–0x900F8): time = now + 3000 (the stock
 * arms 3000 absolute at its init; ours arms relative to the running
 * counter), OCRD00, then CMFAD00 clear and CMEAD00 enable (plat_fbsamp_start) */
void fb_shadow_start(void)
{
    const uint32_t t = ((fbhw_tcnt1d0() >> 8) + 3000u) & 0x00FFFFFFu;
    s.time = t;
    s.state = 0u;
    s.armed = 1u;
    plat_fbsamp_start(t);
}

const volatile fbs_t *fb_shadow_state(void) { return &s; }
const volatile fbr_t *fb_shadow_rec(void) { return &s_rec; }

#if !defined(__sh__)
volatile fbs_t *fb_shadow_host_state(void) { return &s; }
void fb_shadow_host_classify(uint8_t ch) { cls_89a8c(ch); }
void fb_shadow_host_est(uint8_t ch, uint16_t v, int16_t i, uint16_t d) { est_8390c(ch, v, i, d); }
void fb_shadow_host_rupd(uint8_t ch, int16_t i, uint8_t flag, int16_t t_fluid)
{
    r_83abc(ch, i, flag, t_fluid, FBS_9393A_SI);
}
#endif

#endif /* !TCU_RAM_APP */
