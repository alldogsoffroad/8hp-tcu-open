/* gp_shadow.c — THE READ-ONLY GEAR-PAIR SHIFT-ENGINE SHADOW (app/gp_shadow.h;
 * 2026-11-20, ~/tcu_sweep/gp_shadow_port.md; decode
 * ~/tcu_sweep/shift_engine_gearpair_decode.md).
 *
 * The stock's shift object 0xFFF8BD14 (header BD14, block A 0xBD6C, block B
 * 0xBEB4, each with an on-coming phase object at ctx+0xB0 and an off-going
 * one at ctx+0x34) and the gear-pair coordinator's E33C bytes, on this TU's
 * OWN RAM (gp_t, .gp_shadow). Every function names the stock routine it
 * ports; the bytes were re-read from the pristine image (sh-elf-objdump,
 * literal pools resolved) — addresses in the comments. The phase objects step
 * their stock SEQUENCES (0x5EC24 / 0x63555); since 2026-11-20 the up-shift
 * BODIES of the image rows (gp_cal.c GP_PH_NUP, classes 0 / 1) run the decoded
 * laws (exits, pressures; section "THE PHASE BODIES" below) and the rest stays
 * on documented stand-ins (GP_SI_*, counted).
 *
 * NOTHING LIVE IS WRITTEN. The live inputs arrive by value (gp_in_t). The
 * NV calls of the ends (0xBA538 / 0xBA52C → 0xE8512) are never made: only
 * counted.
 */
#define TCU_GP_SHADOW_TU 1
#include "app/gp_shadow.h"
#include "app/gp_cal.h"
#include "app/sm_cal.h"

#if defined(__sh__)
#define GPSEC __attribute__((section(".gp_shadow")))
#else
#define GPSEC
#endif

static gp_t s_gp GPSEC;
#define G (s_gp)
#define W (s_gp.w)

/* 2026-11-20: the retired stand-ins' state — own RAM beside gp_t / gf_t (the
 * dump-zero gaps 0xFFF86794..0xFFF86800 and 0xFFF869BC..0xFFF86A00) */
#if defined(__sh__)
#define GXSEC __attribute__((section(".gp_ext")))
#define GRSEC __attribute__((section(".gp_ramp")))
#else
#define GXSEC
#define GRSEC
#endif
static gp_x_t s_gx GXSEC;
static gp_ramp_t s_gr[2][2] GRSEC;                   /* [block][0 on, 1 off] */
#define X (s_gx)
#define XW (s_gx.w)

/* 2026-11-20: the phase bodies' state (dump-zero 0xFFF87EB0..0xFFF88000, after .out_wit) */
#if defined(__sh__)
#define GPHSEC __attribute__((section(".gp_phase")))
#else
#define GPHSEC
#endif
static gp_ph_t s_gph GPHSEC;
#define PH (s_gph)

/* host seams (SH: compile-time 0 — the dump / the undecoded gates closed) */
#if defined(__sh__)
#define GP_EARLY_OK 0u
#define GP_BD26     GP_SI_BD26
#define GP_AE31     GP_SI_AE31
#else
static uint8_t s_early_ok;
static uint8_t s_bd26;
static uint8_t s_ae31;
#define GP_EARLY_OK s_early_ok
#define GP_BD26     s_bd26
#define GP_AE31     s_ae31
void gp_shadow_host_si(uint8_t early_ok, uint8_t bd26)
{
    s_early_ok = early_ok;
    s_bd26 = bd26;
}
void gp_shadow_host_ae31(uint8_t ae31)
{
    s_ae31 = ae31;
}
#endif
_Static_assert(GP_SI_BD26 == 0u && GP_SI_AE31 == 0u && GP_SI_VAR == 0u && GP_SI_CF4 == 1u && GP_SI_CFA == 0u,
               "gp_shadow: the dump's BD14+0x26 / AE31 / 0x89C06 / 0x89CF4 / 0x89CFA");

/* ── tables ────────────────────────────────────────────────────────────── */
static uint8_t gidx(int8_t g, bool *ok)
{
    const int v = (int)g + 1;
    if (v < 0 || v > 9) {
        *ok = false;
        return 0u;
    }
    return (uint8_t)v;
}

/* 0xB7C02 → 0xD4F86: t[(to+1) + (from+1)*10] (sm_cal_gtype) */
static int8_t b7c02(int8_t f, int8_t t)
{
    bool ok = true;
    const uint8_t c = gidx(t, &ok), r = gidx(f, &ok);
    if (!ok) {
        W.err_n++;
        return 0;
    }
    return sm_cal_gtype[c + r * 10u];
}
/* 0xB7C14 → 0xD4FA0 (sm_cal_gkind) */
static int8_t b7c14(int8_t f, int8_t t)
{
    bool ok = true;
    const uint8_t c = gidx(t, &ok), r = gidx(f, &ok);
    if (!ok) {
        W.err_n++;
        return 0;
    }
    return sm_cal_gkind[c + r * 10u];
}
/* 0xB852A → 0xDC198 / 0xB853C → 0xDC1AE: t[(from+1) + (to+1)*10] */
static uint8_t eltab(const uint8_t *t, int8_t f, int8_t to)
{
    bool ok = true;
    const uint8_t c = gidx(f, &ok), r = gidx(to, &ok);
    if (!ok) {
        W.err_n++;
        return 0u;
    }
    return t[c + r * 10u];
}

/* 0xD7C8C: the gear ratio x1000 (0x4FAA4) */
uint16_t gp_cpl_ratio(int8_t gear)
{
    bool ok = true;
    const uint8_t i = gidx(gear, &ok);
    if (!ok) {
        W.err_n++;                                   /* the stock reports 0x66 */
        return 0u;
    }
    return gp_cal_ratio[i];
}

static void ev(uint8_t kind, int8_t a, int8_t b, uint8_t c, uint8_t d, uint8_t e)
{
    gp_ev_t *x = &G.ev[G.ev_head % GP_EV_N];
    x->pass = (uint16_t)W.pass_n;
    x->kind = kind;
    x->a = a;
    x->b = b;
    x->c = c;
    x->d = d;
    x->e = e;
    G.ev_head = (uint8_t)((G.ev_head + 1u) % GP_EV_N);
}

/* ── 2026-11-20: the kind / class machine, BD1B, the counters, the ramp class,
 * the swap (~/tcu_sweep/shift_class_counters_decode.md) ─────────────────── */
static uint8_t sat8(uint8_t v)
{
    return (uint8_t)(v == 0xFFu ? v : (uint8_t)(v + 1u));
}
static uint16_t sat16(uint16_t v)
{
    return (uint16_t)(v == 0xFFFFu ? v : (uint16_t)(v + 1u));
}
static unsigned bidx(const gp_blk_t *c)
{
    return (c == &G.b) ? 1u : 0u;
}
static uint8_t clamp_u8(int32_t v)
{
    return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

/* 0xB7910: float -> s16 (saturating, truncating) */
static int16_t b7910(float v)
{
    if (!(v > 32767.0f) && !(-32768.0f > v)) {
        return (int16_t)(int32_t)v;
    }
    return (v > 32767.0f) ? (int16_t)0x7FFF : (int16_t)-0x8000;
}

/* 0xB6E10: the 1-D map (ascending axis): x <= x0 -> y0; x >= x[n-1] -> y[n-1];
 * else the segment ending at the first xs[j] >= x, y_i +- trunc(|dy| * (x - xs_i) / dxs)
 * (bytes 0xB6E16..0xB6EA8). The u8 class-1 maps use the same form [I: the
 * stock's byte-map routine was not read]. */
static int16_t map1(const int16_t *xs, const int16_t *ys, unsigned n, int16_t x)
{
    if (x <= xs[0]) {
        return ys[0];
    }
    if (x >= xs[n - 1u]) {
        return ys[n - 1u];
    }
    unsigned j = 1u;
    while (j < n - 1u && x > xs[j]) {
        j++;
    }
    const unsigned i = j - 1u;
    const float num = (float)((int32_t)x - (int32_t)xs[i]);
    const float den = (float)((int32_t)xs[j] - (int32_t)xs[i]);
    if (ys[j] >= ys[i]) {
        const float d = ((float)((int32_t)ys[j] - (int32_t)ys[i]) * num) / den;
        return (int16_t)((int32_t)ys[i] + (int32_t)d);
    }
    const float d = ((float)((int32_t)ys[i] - (int32_t)ys[j]) * num) / den;
    return (int16_t)((int32_t)ys[i] - (int32_t)d);
}
/* 0xEC182 / 0xEC1F0: the shift class from the kind, the call-site flag and the
 * pair type (ctx+5; type 4 = a skip downshift) [O bytes 0xEC182..0xEC208] */
uint8_t gp_class_ec182(uint8_t kind, uint8_t flag, uint8_t type)
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
uint8_t gp_class_ec1f0(uint8_t kind, uint8_t flag, uint8_t type)
{
    return (kind == 9u) ? 4u : gp_class_ec182(kind, flag, type);
}

/* 0xEC362(BD14, type): f8 = t[0x89CF4] of the type's array {u32 n; u8 v[]}
 * (types 3 / 4 down, 1 / 2 up, else 0), clamped to 4; variant 0x89C06 = 0 */
static uint8_t ec362(uint8_t type)
{
    unsigned i = GP_SI_CF4;
    if (i > 4u) {
        W.report_n++;                                /* 0xB79E4(0x36B, …): counted */
        i = 4u;
    }
    uint8_t r = 0u;
    if (type == 3u || type == 4u) {
        r = GP_CAL_F8_DN(i);
    } else if (type == 1u || type == 2u) {
        r = GP_CAL_F8_UP(i);
    }
    return (r > 4u) ? 4u : r;
}

/* the four condition windows (bytes 0xEC40E / 0xEC4D8 / 0xEC578 / 0xEC614):
 * output speed >= out[], torque > lo[], torque < hi[] (tables 10 x 10 s16 /
 * u16, index (to+1)*10 + (from+1), 0 outside — 0xB6D14 / 0xB6CE8), the
 * temperature byte window and (0xEC40E / 0xEC4D8 / 0xEC578) the BD16 rule */
static bool win_core(unsigned f, int8_t from, int8_t to)
{
    const unsigned x = (unsigned)((uint8_t)(from + 1)), y = (unsigned)((uint8_t)(to + 1));
    const bool in = (x <= 9u && y <= 9u);
    const uint16_t ov = in ? GP_CAL_WIN_OUT(f, y * 10u + x) : 0u;
    const int32_t lo = in ? (int32_t)GP_CAL_WIN_LO(f, y * 10u + x) : 0;
    const int32_t hi = in ? (int32_t)GP_CAL_WIN_HI(f, y * 10u + x) : 0;
    const int32_t tq = (int32_t)X.in2.tq;
    if ((int32_t)G.in.gf.cb8 < (int32_t)ov) {
        return false;
    }
    if (!(tq > lo)) {
        return false;
    }
    return tq < hi;
}
static bool ec40e(uint8_t t, int8_t from, int8_t to, uint8_t bd16)
{
    if (!win_core(0u, from, to) || !(t < GP_CAL_TMP(0))) {
        return false;
    }
    if (bd16 == 2u) {
        return t > GP_CAL_TMP(2);
    }
    return bd16 == 1u && t > GP_CAL_TMP(1);
}
static bool ec4d8(uint8_t t, int8_t from, int8_t to, uint8_t bd16)
{
    if (!win_core(1u, from, to) || !(t < GP_CAL_TMP(3))) {
        return false;
    }
    if (bd16 == 2u) {
        return t > GP_CAL_TMP(5);
    }
    return bd16 == 1u && t > GP_CAL_TMP(4);
}
static bool ec578(uint8_t t, int8_t from, int8_t to, uint8_t bd16)
{
    return win_core(2u, from, to) && t > GP_CAL_TMP(7) && t < GP_CAL_TMP(6) && bd16 == 1u;
}
static bool ec614(uint8_t t, int8_t from, int8_t to)
{
    return win_core(3u, from, to) && t > GP_CAL_TMP(9) && t < GP_CAL_TMP(8);
}

/* 0xEC20A(BD14, f8, BD16, kind, T, from, to): the window decision (bytes
 * 0xEC20A..0xEC360) */
static bool ec20a(uint8_t f8, uint8_t bd16, uint8_t kind, uint8_t t, int8_t from, int8_t to)
{
    bool r = true;
    if ((f8 == 1u && (kind == 3u || kind == 4u)) || (f8 == 2u && (kind == 5u || kind == 6u)) ||
        (f8 == 3u && kind == 8u) || (f8 == 4u && kind == 9u)) {
        return true;
    }
    if (kind >= 1u && kind <= 4u) {
        if (bd16 == 1u) {
            r = false;
        }
    } else if (kind != 0u) {
        return true;                                 /* kind >= 5: 0xEC35A returns 1 */
    }
    if (f8 < 1u || f8 > 4u) {
        r = false;
    }
    if (!ec40e(t, from, to, bd16)) {
        if (f8 == 4u && !ec614(t, from, to)) {
            r = false;
        }
        if (f8 == 3u && !ec578(t, from, to, bd16) && !ec4d8(t, from, to, bd16)) {
            r = false;
        }
        if (f8 == 2u && !ec4d8(t, from, to, bd16)) {
            r = false;
        }
        if (f8 == 1u) {
            r = false;
        }
    }
    {
        /* 0xA1032() (the DSM reaction banks via 0x9F718, callback 0x4CED0): stand-in 0,
         * counted; the term (0xA1032 ∧ cal +0x7C == 0) could only fire with cal +0x7C == 0 */
        const bool a1032 = false;
        XW.k_a1032 = sat16(XW.k_a1032);
        if (a1032 && GP_CAL_K7C == 0u) {
            r = false;
        }
    }
    return r;
}

/* 0x1180BE(ctx, p): the kind (ctx+7) and f8 (ctx+8) of one context (bytes
 * 0x1180BE..0x118372).
 * FINDING (bytes 0x117970..0x11798E, 0xED9B2, 0xED906, 0xEC20A..0xEC294): the start call
 * 0x117B4A(ctx, 1) of 0xED906 comes AFTER the progress init 0x117970, which stores kind
 * ctx+7 = 0 — so it evaluates with kind 0, and 0xEC20A skips its "kind 1..4 and BD16 == 1
 * -> 0" rule for kind 0. Inside the torque / temperature window an UPSHIFT therefore starts
 * at kind 4 (class 1) exactly like a downshift; only outside the window (or after kind 1 / 2
 * is entered) does BD16 == 1 hold it at kind 2 (class 0). The decode's section 1.4 table
 * (upshift kind 2 / class 0) assumed the idle kind 1 at the start call. */
static void k_1180be(gp_blk_t *c, gp_xblk_t *x, uint8_t p)
{
    const uint8_t type = c->kind;                    /* ctx+5 */
    const uint8_t kprev = x->k7;
    const int8_t from = c->from, to = c->to;
    const uint8_t f8 = ec362(type);
    x->f8 = f8;                                      /* ctx+8 */
    if (G.bd15 != 0u && (type == 1u || type == 3u)) {
        XW.k_adapt = sat16(XW.k_adapt);              /* 0xAEDC8 -> 0xB019C: exact pass-through (cal 0x1FE672 = 0) */
    }
    if (G.bd15 != 0u && type == 1u && p == 1u) {
        XW.k_adapt = sat16(XW.k_adapt);              /* 0xAED0E -> 0xB1C3C: exact pass-through (cal 0x1FE68F = 0) */
    }
    const bool b3 = ec614(G.bd22, from, to) && f8 == 4u;
    const bool b4 = ec578(G.bd22, from, to, G.bd16) && f8 == 3u;
    const bool b5 = ec4d8(G.bd22, from, to, G.bd16) && (f8 == 2u || f8 == 3u);
    if (!(G.bd15 != 0u && (type == 1u || type == 3u || type == 4u))) {
        x->k7 = 1u;
        return;
    }
    const bool cc = ec20a(f8, G.bd16, kprev, G.bd22, from, to);
    const bool act_or_e344 = (c->act != 0u) || (gp_gear_state()->e344 != 0u);
    const bool a8 = (GP_CAL_KA8 == 0u) || (G.bd16 != 2u);
    uint8_t k = kprev;
    if (kprev > 9u) {
        W.report_n++;                                /* 0xB79E4(0x369, …) */
    } else if (cc) {
        switch (kprev) {
        case 0u:
        case 1u:
            k = b3 ? 9u : (b4 ? 8u : (b5 ? 6u : 4u));
            break;
        case 2u:
            k = (act_or_e344 && a8) ? (b5 ? 5u : 3u) : (b5 ? 6u : 4u);
            break;
        case 3u:
            k = b5 ? 5u : 3u;
            break;
        case 4u:
            k = !b5 ? 4u : ((act_or_e344 && a8) ? 7u : 6u);
            break;
        default:
            break;
        }
    } else if (kprev <= 2u) {
        k = 2u;
    }
    x->k7 = k;
}

/* the override's f8 array 0xD02EC (via 0xB6B28) */
static uint8_t ov_f8(void)
{
    return GP_CAL_F8_OV(GP_SI_CF4 > 4u ? 4u : GP_SI_CF4);
}

/* 0x117B4A(ctx, p): 0x1180BE, then (committed context, AE31 == 3) the
 * override (bytes 0x117B4A..0x117C0A) */
static void b4a(gp_blk_t *c, gp_xblk_t *x, uint8_t p)
{
    const uint8_t bd16 = G.bd16;
    const uint8_t k0 = x->k7;
    k_1180be(c, x, p);
    if (c->act == 1u) {
        bool took = false;
        if (GP_AE31 != 0u) {
            bool ovr = false;
            if (p != 0u && bd16 == 1u) {
                const int16_t tur = (int16_t)G.in.gf.cba;      /* 0x89C0C = the turbine speed 0x89CBA */
                int16_t ov_x[4], ov_y[4];                       /* 0x1B78A0 {u32 4, s16 x[4], s16 y[4]}, read in place */
                for (unsigned i = 0u; i < 4u; i++) {
                    ov_x[i] = GP_CAL_OV_X(i);
                    ov_y[i] = GP_CAL_OV_Y(i);
                }
                ovr = (int32_t)X.in2.tq > (int32_t)map1(ov_x, ov_y, 4u, tur);
            }
            if (!ovr && p != 0u && bd16 == 2u && X.in2.pedal > GP_CAL_OV_PED) {
                ovr = true;
            }
            if (ovr) {
                const uint8_t f = ov_f8();
                const uint8_t k = (f == 1u) ? 4u : ((f == 2u) ? 6u : 2u);
                if (x->ov != 0u || x->k7 != k) {
                    x->f8 = f;
                    x->k7 = k;
                    x->ov = 1u;
                }
                took = true;
                XW.k_ovr = sat16(XW.k_ovr);
            }
        }
        if (!took && x->ov != 0u) {
            x->f8 = ov_f8();
        }
    }
    x->cls[0] = gp_class_ec182(x->k7, 0u, c->kind);
    x->cls[1] = gp_class_ec182(x->k7, 1u, c->kind);
    if (x->k7 != k0) {
        XW.k_chg = sat16(XW.k_chg);
    }
}

/* 0xED92E(BD14, p): the cal-cell reload 0x1176E0 and the torque modules —
 * not ported, counted */
static void reload(void)
{
    W.unp_n++;
    XW.k_reload = sat16(XW.k_reload);
}

/* 0xEC7CC (the BD1B output threshold) and 0xEC6A6 (BD1B itself) */
static uint16_t ec7cc(void)
{
    const bool one = (G.a.to == 1 || G.a.from == 1);
    const unsigned idx = one ? (G.bd16 == 1u ? 2u : 3u) : (G.bd16 == 1u ? 0u : 1u);
    return (uint16_t)GP_CAL_THR(idx);
}
static void ec6a6(void)
{
    if (G.bd17 == 4u) {
        G.bd1b = 0u;
        return;
    }
    if (G.bd1b != 0u) {
        return;                                      /* latched until the next commit (0xED9B2) */
    }
    const uint8_t pf = G.in.gf.e69b;                 /* 0x89C30 */
    bool set = false;
    if ((uint32_t)G.in.gf.cb8 < (uint32_t)ec7cc()) {
        set = true;
    } else if ((uint32_t)(int32_t)(int16_t)G.in.gf.cba < (uint32_t)(int32_t)GP_CAL_THR(4)) {
        set = true;
    } else if (pf == 0u || pf == 3u || pf == 4u) {
        set = true;
    }
    if (set) {
        G.bd1b = 1u;
        XW.bd1b_n = sat16(XW.bd1b_n);
    }
}

/* the slot-28 counters: 0xEC0E0 (BD14), 0x117538 per context (0x11295C on-coming,
 * 0x10FCA2 off-going): += *(u16)0xFFF8E220 = 1 per pass, saturating */
static void ctr_pass(void)
{
    G.bd68 = sat8(G.bd68);                           /* +0x54 */
    X.bd30 = sat8(X.bd30);                           /* +0x1C */
    X.bd5f = sat8(X.bd5f);                           /* +0x4B */
    X.bd2e = sat16(X.bd2e);                          /* +0x1A */
    G.bd67 = sat8(G.bd67);                           /* +0x53 */
    X.bd51 = sat8(X.bd51);                           /* +0x3D */
    /* 0xEEB94(E6CD, …) is not ported: one call per counter pass, counted by XW.c_pass */
    for (unsigned k = 0u; k < 2u; k++) {
        gp_blk_t *c = (k == 0u) ? &G.a : &G.b;
        gp_xblk_t *x = &X.blk[k];
        x->c22 = sat16(x->c22);
        c->on.tin = sat8(c->on.tin);                 /* +0x1A */
        x->on_c10 = sat8(x->on_c10);
        x->on_c1c = sat8(x->on_c1c);
        x->on_ca = sat16(x->on_ca);
        x->on_cc = sat16(x->on_cc);
        x->on_c3e = sat16(x->on_c3e);
        c->off.tin = sat8(c->off.tin);               /* +0x12 */
        x->off_c2c = sat8(x->off_c2c);
    }
    XW.c_pass = sat16(XW.c_pass);
}

/* the 16-byte float ramp object (vtable 0x49FC0): init 0xD1D8E / step 0xD1D96 */
void gp_ramp_init(gp_ramp_t *r, int16_t value, int16_t mn, int16_t mx, uint16_t now)
{
    r->mn = (float)mn;
    r->mx = (float)mx;
    r->v = (float)value;
    r->ts = now;
    r->pad = 0u;
    XW.ramp_n = sat16(XW.ramp_n);
}
int16_t gp_ramp_step(gp_ramp_t *r, int16_t rate, uint16_t now)
{
    const uint16_t dt = (uint16_t)(now - r->ts);
    float v = r->v + ((float)dt * (float)((int32_t)rate * 100)) / 1000.0f;
    if (v < r->mn) {
        v = r->mn;
    }
    if (v > r->mx) {
        v = r->mx;
    }
    r->v = v;
    r->ts = now;
    XW.ramp_n = sat16(XW.ramp_n);
    return b7910(v);
}

/* ── the progress (sync) part of the shift context ─────────────────────── */
/* 0x117A74: slip = ratio(to)·0x89CB8/1000 − 0x89CBA (s1 = s2 = 1: dump) */
static int16_t a74(const gp_blk_t *c)
{
    const int32_t r = (int32_t)gp_cpl_ratio(c->to);
    int32_t v = ((int32_t)G.in.gf.cb8 * r) / 1000 - (int32_t)G.in.gf.cba;
    if (v > 0x7FFF) {
        v = 0x7FFF;
    } else if (v < -0x8000) {
        v = -0x8000;                                 /* 0xB7888 */
    }
    return (int16_t)v;
}

/* 0x117F48(ctx, hold, p3): the sync flag and its direction (the BD2E / BD30
 * clears of the active block are part of the unported slot-34 cyclic) */
static void f48(gp_blk_t *c, uint8_t hold, uint8_t p3)
{
    const int32_t thr = (int32_t)GP_CAL_SYNC;
    c->slip = a74(c);
    if (hold != 0u) {
        c->sync = 0u;
        c->dir = 0xFFu;
    } else if (c->sync == 0u) {
        uint8_t d = c->dir;
        if (d == 0xFFu) {
            d = (G.bd16 == 3u) ? (c->slip >= 0 ? 3u : 1u) : c->kind;
            c->dir = d;
        }
        if (((d == 1u || d == 2u) && (int32_t)c->slip >= -thr) ||
            ((d == 3u || d == 4u) && (int32_t)c->slip <= thr) || p3 != 0u) {
            c->sync = 1u;
        }
    }
    if (c->act == 1u) {                              /* 0x11803E: the active context clears BD2E / BD30 */
        const int32_t a = (c->slip == -0x8000) ? 0x7FFF : (c->slip < 0 ? -(int32_t)c->slip : (int32_t)c->slip);
        if (c->sync == 0u) {
            X.bd2e = 0u;
        }
        if (c->sync == 0u || a > thr) {              /* 0xB77A0 */
            X.bd30 = 0u;
        }
    }
}

/* 0x117970(ctx, BD16): the progress init: ctx+0x22 = 0, +0 = 0, +7 = 0 (the
 * kind), +0x30 = 0, +0x26 = 0 (bytes 0x117970..0x1179D4; 0x117E98 / 0x117F28 /
 * 0xB8260 / 0xB827A and the +0x16 / +0x1C / +0x1D stores not ported) */
static void p970(gp_blk_t *c)
{
    gp_xblk_t *x = &X.blk[bidx(c)];
    c->sync = 0u;
    x->c22 = 0u;
    x->k7 = 0u;
    x->ov = 0u;
    W.unp_n++;
}

static void prog_reset(const gp_blk_t *c);

/* 0x11793E: the progress start (+0x31 = +0x28 = 1, +0x27 = BD1A ∈ {0x1E, 0x0A}),
 * then 0x117F48(ctx, 1, +0x27) */
static void p93e(gp_blk_t *c)
{
    c->hold = 1u;
    c->p27 = (uint8_t)((G.bd1a == 0x1Eu || G.bd1a == 0x0Au) ? 1u : 0u);
    f48(c, 1u, c->p27);
    prog_reset(c);                                   /* 0x1179D8(p, 1) -> 0xD2046 */
}

/* ── the phase objects ─────────────────────────────────────────────────── */
/* 0x11294C (on) / 0x10FBFC (off): the ends */
static void on_end(gp_po_t *o)
{
    o->out = 0u;                                     /* +0x16, +2, +0x3A, +0x22 */
}
static void off_end(gp_po_t *o)
{
    o->out = 0u;                                     /* +0xE, +2, +0x14 */
    o->phase = 0x11u;                                /* +9 = 0x11 (0x10FC04) */
}

/* 0x11299A: the on-coming start */
static void on_start(gp_po_t *o)
{
    o->type = (uint8_t)((G.bd16 == 2u) ? 2u : 1u);   /* 0x1132E2 */
    o->step = 0u;                                    /* +0x28 */
    o->phase = 0u;                                   /* +0x12 */
    o->f4 = 0u;                                      /* +4..+7 */
    o->f7 = 0u;
    o->tin = 0u;
    o->out = 0u;
    o->phases_n = 0u;
    W.unp_n++;                                       /* 0x114E26 / 0x113788 / +0x36 */
}

/* 0x10FC5C: the off-going start */
static void off_start(gp_po_t *o)
{
    uint8_t t = (uint8_t)((G.bd16 == 1u) ? 1u : 2u); /* 0x10FF9E */
    if (G.bd1a == 0x14u && t == 2u) {
        t = 0u;
    }
    o->type = t;
    o->step = 0u;                                    /* +0x18 */
    o->phase = 0u;                                   /* +9 */
    o->f4 = 0u;
    o->f7 = 0u;
    o->tin = 0u;
    o->out = 0u;
    o->phases_n = 0u;
    W.unp_n++;                                       /* +0x16 = !0xB822C(elem) */
}

/* 0x113D0A: on next = +7 ? 0x0E : 0x5EC24[step·3 + type] */
static uint8_t on_next(const gp_po_t *o)
{
    const unsigned i = (unsigned)o->step * 3u + o->type;
    if (o->f7 != 0u) {
        return 0x0Eu;
    }
    if (i >= GP_CAL_ONSEQ_N) {
        W.err_n++;
        return 0xFFu;
    }
    return gp_cal_onseq[i];
}
/* 0x110ADE: off next = 0x63555[step·3 + type] */
static uint8_t off_next(const gp_po_t *o)
{
    const unsigned i = (unsigned)o->step * 3u + o->type;
    if (i >= GP_CAL_OFFSEQ_N) {
        W.err_n++;
        return 0xFFu;
    }
    return gp_cal_offseq[i];
}

/* ── 2026-11-20: THE PHASE BODIES (decode shift_phase_bodies_decode.md §2 / §3, port gp_phases_port.md) ──
 * The up-shift laws of the on-coming (0x1132F4 entry / 0x1138FC done / 0x113F16 output) and off-going
 * (0x10FFBE / 0x1106DC / 0x110CDC) phase objects for the rows 1->2 .. GP_PH_NUP->+1 in the classes 0 / 1
 * (UA / UB / UC / O40 / O3C / O4C / ctx+0x134 of gp_cal.c, variant 0). Down-shifts, the other rows and
 * classes, and the block-B (pre-selected) phases keep the sequence-only stand-ins (counted). */

/* the cal map readers: the stock interpolators on a flash-copied map (big-endian) */
static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static int32_t s16at(const uint8_t *p, int32_t i)
{
    return (int32_t)(int16_t)(uint16_t)(((uint32_t)p[2 * i] << 8) | p[2 * i + 1]);
}

/* 0xB6E10: {u32 n; s16 x[n]; s16 y[n]} */
static int32_t m1s16(const uint8_t *m, int16_t x)
{
    const int32_t n = (int32_t)be32(m);
    const uint8_t *xs = m + 4, *ys = m + 4 + 2 * n;
    int32_t k = 0;
    float num = 0.0f, den = 0.0f;
    if (s16at(xs, 0) < x) {
        if (x < s16at(xs, n - 1)) {
            int32_t p = 0;
            while (s16at(xs, p) < x) {
                p++;
            }
            k = p - 1;
            den = (float)(s16at(xs, p) - s16at(xs, k));
            num = (float)(x - s16at(xs, k));
        } else {
            den = 1.0f;
            num = 1.0f;
            k = n - 2;
        }
    }
    {
        int32_t y0 = s16at(ys, k);
        if (num != 0.0f) {
            const int32_t y1 = s16at(ys, k + 1);
            const int32_t d = y0 - y1;
            if (y0 <= y1) {
                return (int32_t)(int16_t)(int32_t)(((float)-d * num) / den) + y0;
            }
            y0 = y0 - (int32_t)(int16_t)(int32_t)(((float)d * num) / den);
        }
        return y0;
    }
}

/* the in-place map views (app/cal_inplace.h): the row structs hold the flash ADDRESS of a stock map */
#define MP(a) cal_p8(a)
/* the first x column of a stock 2-D s16 map {nx = 8; ny = 10; x[nx]; y[ny]; z[ny][nx]} as a 1-D map {n = ny; y; z[.][0]}
 * (the x input, the stock's ctx+0xC, is a stand-in 0 below the axis: gp_cal_ph generator asserts x[0] >= 19) */
static int32_t m1s16c0(uint32_t addr, int16_t x)
{
    const uint8_t *m = cal_p8(addr);
    const uint32_t nx = be32(m), ny = be32(m + 4);
    uint8_t buf[4u + 4u * 10u];
    uint32_t i;
    if (nx != 8u || ny != 10u) {
        return 0;
    }
    buf[0] = 0u; buf[1] = 0u; buf[2] = 0u; buf[3] = (uint8_t)ny;
    for (i = 0u; i < ny; i++) {
        buf[4u + 2u * i] = m[8u + 2u * nx + 2u * i];
        buf[5u + 2u * i] = m[9u + 2u * nx + 2u * i];
        buf[4u + 2u * (ny + i)] = m[8u + 2u * (nx + ny) + 2u * i * nx];
        buf[5u + 2u * (ny + i)] = m[9u + 2u * (nx + ny) + 2u * i * nx];
    }
    return m1s16(buf, x);
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

static int16_t s16sat(int32_t v)
{
    return (int16_t)(v < -0x8000 ? -0x8000 : (v > 0x7FFF ? 0x7FFF : v));
}
static int16_t pos16(int32_t v)                      /* 0xB77BC / the clamp >= 0 */
{
    return (int16_t)(v < 0 ? 0 : (v > 0x7FFF ? 0x7FFF : v));
}
/* 0xD1B94(t, T, a, b): t < T ? a + t (b - a) / T : b */
static int16_t d1b94(uint16_t t, uint16_t big, int16_t a, int16_t b)
{
    int32_t v = b;
    if ((uint32_t)t < (uint32_t)big) {
        v = ((int32_t)t * ((int32_t)b - (int32_t)a)) / (int32_t)big + (int32_t)a;
    }
    return s16sat(v);
}
/* 0xB6EAE (u16 axes): the DT1 time constant */
static uint16_t b6eae_dt1(uint16_t x)
{
    if (x <= gp_cal_dt1x[0]) {
        return gp_cal_dt1y[0];
    }
    if (x >= gp_cal_dt1x[2]) {
        return gp_cal_dt1y[2];
    }
    const unsigned j = (x <= gp_cal_dt1x[1]) ? 1u : 2u;
    const uint32_t y0 = gp_cal_dt1y[j - 1u], y1 = gp_cal_dt1y[j];
    const float num = (float)(int32_t)((uint32_t)x - gp_cal_dt1x[j - 1u]);
    const float den = (float)(int32_t)((uint32_t)gp_cal_dt1x[j] - gp_cal_dt1x[j - 1u]);
    if (y1 < y0) {
        return (uint16_t)(y0 - ((uint32_t)(int32_t)(((float)(int32_t)(y0 - y1) * num) / den) & 0xFFFFu));
    }
    return (uint16_t)((uint32_t)(int32_t)(((float)(int32_t)(y1 - y0) * num) / den) + y0);
}

/* the sync progress BD76 of a context: 0x1179D8 -> 0xD2058 (the DT1 rate of -slip) and 0x117AEC; the
 * engagement start 0xD2046 resets the DT1 (gs_engine.c's chain, now per context) */
static void prog_reset(const gp_blk_t *c)
{
    gp_pb_t *b = &PH.blk[bidx(c)];
    b->ts = (uint16_t)X.in2.now_ms;
    b->acc = (int32_t)(int16_t)(-(int32_t)c->slip) * 1000;
}
static void prog_pass(const gp_blk_t *c)
{
    gp_pb_t *b = &PH.blk[bidx(c)];
    const int16_t x = (int16_t)(-(int32_t)c->slip);
    uint16_t t = b6eae_dt1((uint16_t)G.in.gf.cb8);
    uint16_t dt;
    int16_t y;
    float r;
    if (t == 0u) {
        t = 1u;
    }
    dt = (uint16_t)((uint16_t)X.in2.now_ms - b->ts);
    if (dt > t) {
        dt = t;
    }
    y = s16sat(((int32_t)x * 1000 - b->acc) / (int32_t)t);
    b->acc += (int32_t)y * (int32_t)dt;
    b->ts = (uint16_t)X.in2.now_ms;
    b->rate = y;
    if (c->sync != 0u) {
        r = (float)X.bd2e;
    } else if (y == 0 || (y > 0 && c->slip < 0) || (y < 0 && c->slip > 0)) {
        r = -32768.0f;
    } else {
        r = ((float)(-(int32_t)c->slip) * 100.0f) / (float)y;
    }
    if (-500.0f > r) {
        r = -500.0f;
    }
    b->bd76 = b7910(r);
}

/* ── the rows ─────────────────────────────────────────────────────────────── */
typedef struct {
    const gp_ua_t *ua;
    const gp_ub_t *ub;
    const gp_uc_t *uc;
    const gp_o40_t *o40;
    const gp_o3c_t *o3c;
    const gp_o4c_t *o4c;
    const gp_c134_t *c134;
} gp_rows_t;

/* the up-shift row of a context: a single upshift of the image rows, class 0 / 1 */
static bool rows_get(const gp_blk_t *c, gp_rows_t *r)
{
    const unsigned cls = X.blk[bidx(c)].cls[0];
    if (c->to != (int8_t)(c->from + 1) || c->from < 1 || (unsigned)c->from > GP_PH_NUP || cls > 1u) {
        return false;
    }
    const unsigned i = (unsigned)c->from - 1u;
    r->ua = &gp_ph_ua[i];
    r->ub = &gp_ph_ub[i];
    r->uc = &gp_ph_uc[cls][i];
    r->o40 = &gp_ph_o40[i];
    r->o3c = &gp_ph_o3c[i];
    r->o4c = &gp_ph_o4c[cls][i];
    r->c134 = &gp_ph_c134[i];
    return true;
}

#define BTQ     ((int16_t)X.in2.tq)                  /* 0x89CA0 the input torque */
#define BTMP    ((uint8_t)G.bd22)                    /* BD22 the temperature byte at the shift start */
#define BTURB   ((int16_t)G.in.gf.cba)               /* 0x89C0C = the turbine speed */

static int16_t ms_since(const gp_pb_t *b)            /* on +0x3C: 0x89CAC - ctx+0x16 */
{
    const uint16_t d = (uint16_t)((uint16_t)X.in2.now_ms - b->ms16);
    return (int16_t)(d > 0x7FFFu ? 0x7FFF : d);
}

/* the shift counter of an on-coming element (0xFFF8E6F4..F7 = A / C / D / E; B has none) */
static int cnt_idx(uint8_t elem)
{
    return elem == 1u ? 0 : (elem == 3u ? 1 : (elem == 4u ? 2 : (elem == 5u ? 3 : -1)));
}

/* 0x113E88 (up): the kiss level UA+0x1C(torque, temp) [+ 0x1132BC (ctx+0x1C): 0, stood in] */
static int16_t b_kiss(const gp_rows_t *r)
{
    PH.w.x_si = sat16(PH.w.x_si);                    /* 0x1132BC: ctx+0x1C == 0 */
    return (int16_t)m2s16(MP(r->ua->f1c), BTQ, BTMP);
}
/* 0x112A54: the phase-4 duration UC+4(BD22, 0x89C0E) + UA+0x48(counter, temp) */
static uint16_t b_dur4(const gp_rows_t *r, uint8_t elem)
{
    const int ci = cnt_idx(elem);
    int32_t v = (int32_t)m2u8(MP(r->uc->f04), BTMP, X.c0e);
    v += m2s16(MP(r->ua->f48), (int16_t)(ci < 0 ? 0 : PH.cnt[ci]), (int16_t)BTMP);
    return (uint16_t)(v < 0 ? 0 : v);
}
/* 0x112BFC: the phase-9 duration UC+0x18(0x89C1C, 0x89C0E) x UA+0xC(BD22) / 100 */
static uint16_t b_dur9(const gp_rows_t *r)
{
    return (uint16_t)(((uint32_t)m2u8(MP(r->uc->f18), X.cls1c, X.c0e) * (uint32_t)m1u8(MP(r->ua->f0c), BTMP)) / 100u);
}
/* 0x1144F0(x): UC+0x20(ctx+0xC, torque) + UA+0x28(x) [+ UA+0x44(BD80) while 0x89CC2 < UA+0x40: off] */
static int32_t b_1144f0(const gp_rows_t *r, int16_t x)
{
    return m1s16c0(r->uc->f20, BTQ) + m1s16(MP(r->ua->f28), x);
}

/* 0x113C82 (up): BD1B == 0 -> BD76 >= UC+0(0x89C1C) - 0x80; BD1B != 0 -> false */
static bool c82(const gp_blk_t *c, const gp_rows_t *r)
{
    const gp_pb_t *b = &PH.blk[bidx(c)];
    return G.bd1b == 0u && (int32_t)b->bd76 >= (int32_t)m1u8(MP(r->uc->f00), X.cls1c) - 128;
}

/* the on-coming phase-done (0x1138FC) for an up row of block A / the pre-selected block's stand-ins */
static bool on_done_up(gp_blk_t *c, const gp_po_t *o, const gp_rows_t *r)
{
    const gp_pb_t *b = &PH.blk[bidx(c)];
    const bool com = (c->act == 1u);
    if (!com) {
        return (o->phase == 0x00u) || (o->phase == 0x01u && o->tin >= GP_SI_T_SHORT);    /* block B: counted stand-in */
    }
    switch (o->phase) {
    case 0x00: return true;
    case 0x01: return o->f7 != 0u || o->tin >= r->uc->f08;
    case 0x02: PH.w.fill_si = sat16(PH.w.fill_si); return o->f7 != 0u || o->tin >= GP_SI_T_FILL;
    case 0x13: return true;                                              /* committed: at once */
    case 0x04: return b->v2e != 0u && (o->f7 != 0u || c82(c, r) || (G.bd1b != 0u && o->tin >= b->d32));
    case 0x09: return o->f7 != 0u || c82(c, r) || o->tin >= b->d32;
    case 0x0A: return o->f7 != 0u || c82(c, r) || (G.bd1b != 0u && o->tin >= b->d32);
    case 0x0D: return o->f7 != 0u || X.bd30 >= r->ua->f14;
    default:   return false;                                             /* 0x0E / 0x12 terminal */
    }
}

/* the on-coming entry bodies (0x1132F4) */
static void on_entry_up(gp_blk_t *c, gp_po_t *o, const gp_rows_t *r)
{
    gp_pb_t *b = &PH.blk[bidx(c)];
    gp_ramp_t *rp = &s_gr[bidx(c)][0];
    switch (o->phase) {
    case 0x13:
        b->o36 = b_kiss(r);
        break;
    case 0x04:
        b->d32 = b_dur4(r, o->elem);
        b->o36 = b_kiss(r);
        gp_ramp_init(rp, 0, -20000, 20000, (uint16_t)X.in2.now_ms);              /* [I] the phase-4 ramp starts at 0 */
        PH.w.x_si = sat16(PH.w.x_si);
        break;
    case 0x09:
        b->f44 = (uint8_t)((c->from == 1 && c->to == 2 && G.in.gf.cb8 == 0u) ? 1u : 0u);   /* 0x1138C6 (cal / 0x89C87 terms not read) */
        b->d32 = b_dur9(r);                                                      /* x the 0x84D6C map when f44: not scaled */
        b->ms16 = (uint16_t)X.in2.now_ms;
        b->o36 = (int16_t)(0 + m2s16(MP(r->uc->f38), BTQ, BTMP));                    /* +0x18 = 0xAED5C (the adaptation read): 0 */
        o->f4 = 1u;
        PH.w.x_si = sat16(PH.w.x_si);
        break;
    case 0x0A: {
        const int ci = cnt_idx(o->elem);
        b->d32 = r->ua->f10;
        gp_ramp_init(rp, 0, -20000, 20000, (uint16_t)X.in2.now_ms);
        if (ci >= 0 && PH.cnt[ci] != 0xFFu) {
            PH.cnt[ci]++;                                                        /* 0x1150FE */
        }
        break;
    }
    case 0x0D:
        b->o36 = (int16_t)((int32_t)o->out - b_1144f0(r, 0));                    /* +0x36 = +0x16 - (0x1144F0(passes) + E9464) */
        gp_ramp_init(rp, 0, -20000, 20000, (uint16_t)X.in2.now_ms);              /* [I] min / max as the 0x0A ramp */
        PH.w.x_si = sat16(PH.w.x_si);
        break;
    case 0x0E:
    case 0x12:
        b->d32 = r->c134->f04;                                                   /* 0x1173EE: 1 pass */
        if (G.bd1a != 0x0Au && G.bd1a != 0x1Eu) {
            b->o36 = b->o2;
        }
        break;
    default:
        break;
    }
}

/* the on-coming output (0x113F16): the step output +2 of an up row */
static void on_out_up(gp_blk_t *c, gp_po_t *o, const gp_rows_t *r)
{
    gp_pb_t *b = &PH.blk[bidx(c)];
    gp_ramp_t *rp = &s_gr[bidx(c)][0];
    const bool com = (c->act == 1u);
    int32_t v = 0;
    switch (o->phase) {
    case 0x13:
        v = b->o36 + (com ? m1s16(MP(r->ua->f08), ms_since(b)) : 0);
        v = pos16(v);
        b->o2c = (int16_t)v;
        b->v2e = 1u;
        break;
    case 0x04:
        v = b->o36 + m2s16(MP(r->uc->f0c), BTURB, BTQ)                                /* 0x114CBE */
            + gp_ramp_step(rp, (int16_t)m1s16(MP(r->ua->f04), BTQ), (uint16_t)X.in2.now_ms)
            + (com ? m1s16(MP(r->ua->f08), ms_since(b)) : 0);
        v = pos16(v);
        b->o2c = (int16_t)v;
        b->v2e = 1u;
        break;
    case 0x09: {
        const uint16_t big = (uint16_t)(((uint32_t)b->d32 * (uint32_t)r->uc->f28) / 100u);   /* 0xEAEFC: blend over UC+0x28 % */
        v = b->o36 + b_1144f0(r, 0) + m1s16(MP(r->ua->f30), ms_since(b));
        v = (int32_t)d1b94((uint16_t)o->tin, big, b->o2c, s16sat(v));                         /* [I] linear blend */
        v = pos16(v);
        if (v > 20000) {
            v = 20000;                                                                        /* 0x1147C8's element limit [I] */
        }
        PH.w.x_si = sat16(PH.w.x_si);
        break;
    }
    case 0x0A:
        v = gp_ramp_step(rp, (int16_t)m2s16(MP(r->uc->f30), BTQ, BTURB), (uint16_t)X.in2.now_ms)
            + b_1144f0(r, (int16_t)(X.blk[bidx(c)].on_cc > 255u ? 255u : X.blk[bidx(c)].on_cc))
            + b->o36 + m1s16(MP(r->ua->f30), ms_since(b));
        v = pos16(v);
        if (v > 20000) {
            v = 20000;
        }
        break;
    case 0x0D: {
        const int32_t rate = (m1s16(MP(r->ua->f34), BTQ) * m1s16(MP(r->ua->f3c), 0)) / 10;       /* 0x11483C: x UA+0x3C(0x89CC2 = 0) / 10 */
        v = gp_ramp_step(rp, s16sat(rate), (uint16_t)X.in2.now_ms)
            + b_1144f0(r, (int16_t)(o->tin + X.blk[bidx(c)].on_c34))
            + m1s16(MP(r->ua->f2c), (int16_t)o->tin)                                          /* 0x114B80 */
            + b->o36;
        v = pos16(v);
        if (v > 20000) {
            v = 20000;
        }
        break;
    }
    case 0x0E:
    case 0x12:
        v = d1b94((uint16_t)o->tin, b->d32, b->o36, 20000);
        break;
    default:
        v = 0;
        break;
    }
    b->o2 = (int16_t)v;
    PH.w.on_n = sat16(PH.w.on_n);
}

/* the off-going phase-3 exit time t (0x111AE2): the on-coming phase-9 duration + O4C+4(0x89C1C, BD22) - 0x80
 * + O4C+8(BD80 = 0) */
static int32_t off_t3(const gp_blk_t *c, const gp_rows_t *r)
{
    (void)c;
    return (int32_t)b_dur9(r) + (int32_t)m2u8(MP(r->o4c->f04), X.cls1c, BTMP) - 128 + m1s16(MP(r->o4c->f08), 0);
}

static bool off_done_up(gp_blk_t *c, const gp_po_t *o, const gp_rows_t *r)
{
    const gp_pb_t *b = &PH.blk[bidx(c)];
    const bool com = (c->act == 1u);
    if (!com) {
        return (o->phase == 0x00u) || (o->phase == 0x01u && o->tin >= GP_SI_T_SHORT);    /* block B: counted stand-in */
    }
    if (o->phase == 0x00u) {
        return true;
    }
    if (o->phase != 0x11u && c->sync != 0u && G.bd16 == 1u) {
        return true;                                                     /* 0x1106DC's head: (sync && BD16 == 1) */
    }
    switch (o->phase) {
    case 0x01: return o->tin >= b->p20;
    case 0x02: {
        /* 0x110430: leave |O4C+0xC(0x89C1C, BD22) - 0x80| passes after the on-coming's fill end [I: the sense] */
        const int32_t lag = (int32_t)m2u8(MP(r->o4c->f0c), X.cls1c, BTMP) - 128;
        PH.w.x_si = sat16(PH.w.x_si);
        return (int32_t)b->pfill >= -lag && b->pfill != 0u;
    }
    case 0x03: {
        const int32_t t = off_t3(c, r);
        PH.w.ph3_t = (uint16_t)(t < 0 ? 0 : (t > 0xFFFF ? 0xFFFF : t));
        PH.w.x_si = sat16(PH.w.x_si);
        if (t < 0) {
            return true;
        }
        return c->on.f4 != 0u && (int32_t)X.blk[bidx(c)].on_ca >= t;
    }
    case 0x04:
    case 0x09: return G.bd1b == 0u ? (int32_t)b->bd76 >= (int32_t)r->o40->f00 : false;
    default:   return false;
    }
}

static void off_entry_up(gp_blk_t *c, gp_po_t *o, const gp_rows_t *r)
{
    gp_pb_t *b = &PH.blk[bidx(c)];
    gp_ramp_t *rp = &s_gr[bidx(c)][1];
    switch (o->phase) {
    case 0x01:
        b->p20 = r->o4c->f00;                                            /* 0x111FE4 */
        break;
    case 0x03:
        b->p3s = b->p2;
        b->p1a = (int16_t)(m1s16(MP(r->o3c->f08), BTQ) + m1s16c0(r->o40->f04, BTQ));   /* the slip-hold level [I: O40+4 + O3C+8] */
        PH.w.x_si = sat16(PH.w.x_si);
        break;
    case 0x04:
        gp_ramp_init(rp, 0, -20000, 0, (uint16_t)X.in2.now_ms);
        PH.w.x_si = sat16(PH.w.x_si);
        break;
    default:
        break;
    }
}

static void off_out_up(gp_blk_t *c, gp_po_t *o, const gp_rows_t *r)
{
    gp_pb_t *b = &PH.blk[bidx(c)];
    gp_ramp_t *rp = &s_gr[bidx(c)][1];
    int32_t v = 0;
    switch (o->phase) {
    case 0x01:
        v = 20000;
        break;
    case 0x02:
        v = m2s16(MP(r->o40->f18), BTQ, BTMP) + d1b94((uint16_t)o->tin, r->o40->f10, r->o40->f0c, 0);
        v = pos16(v);
        b->p3s = (int16_t)v;
        break;
    case 0x03: {
        int32_t t = off_t3(c, r);
        t = t < 1 ? 1 : (t > 0x7FFF ? 0x7FFF : t);
        v = d1b94((uint16_t)o->tin, (uint16_t)t, b->p3s, b->p1a);       /* 0xD1DD0: linear to the slip-hold level */
        v = pos16(v);
        break;
    }
    case 0x04:
        v = pos16((int32_t)b->p1a + gp_ramp_step(rp, (int16_t)m1s16(MP(r->o40->f1c), BTQ), (uint16_t)X.in2.now_ms));
        break;
    default:
        v = 0;
        break;
    }
    b->p2 = (int16_t)v;
    PH.w.off_n = sat16(PH.w.off_n);
}

/* ── 2026-11-20 (phases2): THE DOWN-SHIFT BODIES, class 1, single downshifts from = 2..8 ────────────────────────
 * Decode shift_phase_bodies_decode.md §2 / §3 (the down columns) and shift_class_counters_decode.md §6 (numbers for 2->1 /
 * 3->2; the other pairs are the same blocks of the same arrays, value-unchecked). The on-coming sequence 1 -> 2 -> 0x13 -> 5 ->
 * 9 -> 0x0A -> 0x0B -> 0x0C -> 0x0D -> 0x12, the off-going 1 -> 0 -> 0x0A .. 0x0F -> 0x11. Ported from the cal: the on-coming
 * kiss level (blk58+0xC vs the temperature byte), the phase-5 term 0x114BAA (blk58+0x40 vs ms since the start), the phase-1 delay,
 * the 0x0A duration, the 0x0C exit BD76 >= blk60+0x2C - 0x80, the 0x0D lock count (BD30), the final ramp (ctx+0x138 blk+8);
 * off-going: the phase-1 threshold terms, the 0x0A slip level (O50+0 vs 0x89C1C), the 0x0B / 0x0F durations, the 0x0D level
 * (O50+0x2C x O48+0xC / 100). COUNTED stand-ins (x_si): the terms 0x114CBE (turbine), 0x114688 / 0x1147B0 / 0x11477E /
 * 0x114C50 / 0x114930 (on-coming torque / inertia terms), 0x1111FE / 0x111254 / the 0x111860 ramp (off-going base level), ctx+0xC4,
 * ctx+0x20, the 0x117D30 / 0x117DE6 remaining-to-sync timers (the phase 5 / 0x0B timers and the phase 9 exit stay the earlier
 * stand-ins), 0x0xEAEFC's shape (linear), the 0x0A term O48+0x1C (not applied). Producer ids 1..4 carry the values (recorded). */
static const gp_dn_t *dn_get(const gp_blk_t *c)
{
    const gp_xblk_t *x = &X.blk[bidx(c)];
    if (x->cls[0] != 1u || c->from < 2 || c->from > 8 || c->to != (int8_t)(c->from - 1)) {
        return NULL;
    }
    return &gp_ph_dn[(unsigned)(c->from - 2)];
}
static void dn_read(void)
{
    XW.c1_n = sat16(XW.c1_n);                        /* a phase duration / term taken from the class-1 down rows */
}
static void dn_si(void)
{
    PH.w.x_si = sat16(PH.w.x_si);                    /* an undecoded down-shift term stood in */
}

/* on-coming phase done (0x1138FC, down column) */
static bool on_done_dn(gp_blk_t *c, const gp_po_t *o, const gp_dn_t *d)
{
    const gp_pb_t *b = &PH.blk[bidx(c)];
    const bool com = (c->act == 1u);
    if (!com) {
        dn_read();
        return (o->phase == 0x00u) || (o->phase == 0x01u && o->tin >= d->on_delay);   /* block B: the stand-in as before */
    }
    switch (o->phase) {
    case 0x00: return true;
    case 0x01: dn_read(); return o->f7 != 0u || o->tin >= d->on_delay;
    case 0x02: PH.w.fill_si = sat16(PH.w.fill_si); return o->f7 != 0u || o->tin >= GP_SI_T_FILL;
    case 0x13: return true;                                              /* committed down: at once */
    case 0x05: dn_si(); return o->tin >= GP_SI_T_TORQUE;                 /* 0x117DE6 (BD1B == 0): stood in */
    case 0x09: dn_si(); return c->sync != 0u;                            /* the inertia phase ends synchronised */
    case 0x0A: dn_read(); return o->f7 != 0u || o->tin >= d->on_dur0a;
    case 0x0B: dn_si(); return o->f7 != 0u || G.bd1b != 0u || o->tin >= GP_SI_T_SHORT;   /* 0x117D30 + passes: stood in */
    case 0x0C: {
        dn_read();
        const int32_t thr = (int32_t)m1u8(MP(d->on_ex0c), BTMP) - 128;   /* 0x1154BA: blk60+0x2C(temp) - 0x80 */
        return o->f7 != 0u || G.bd1b != 0u || (int32_t)b->bd76 >= thr;
    }
    case 0x0D: dn_read(); return o->f7 != 0u || X.bd30 >= d->on_lock;
    default:   return false;                                             /* 0x0E / 0x12 terminal */
    }
}

/* the on-coming entry bodies (0x1132F4, down column) */
static void on_entry_dn(gp_blk_t *c, gp_po_t *o, const gp_dn_t *d)
{
    gp_pb_t *b = &PH.blk[bidx(c)];
    gp_ramp_t *rp = &s_gr[bidx(c)][0];
    switch (o->phase) {
    case 0x13:
        b->o36 = (int16_t)m1s16(MP(d->on_kiss), BTMP);                   /* 0x113E88: blk58+0xC vs the temperature byte */
        dn_read();
        break;
    case 0x05:
        b->d32 = GP_SI_T_TORQUE;                                         /* 0x112A54: stood in */
        b->o36 = (int16_t)m1s16(MP(d->on_kiss), BTMP);
        dn_si();
        break;
    case 0x09:
        b->d32 = GP_SI_T_SHORT;                                          /* 0x112BFC down: stood in (the exit is the sync) */
        b->ms16 = (uint16_t)X.in2.now_ms;
        /* +0x36 = +0xAED5C (0) + 0x114930 (stood in: the kiss level of phase 5 carried [I, decode 6: "held at kiss"]) */
        o->f4 = 1u;
        dn_si();
        break;
    case 0x0A:
        b->d32 = d->on_dur0a;                                            /* 0x114C9A: blk58+0x14 */
        gp_ramp_init(rp, 0, -20000, 20000, (uint16_t)X.in2.now_ms);
        dn_read();
        break;
    case 0x0B:
        b->o36 = (int16_t)(int32_t)o->out;                               /* +0x36 = +0x16 - (E9464 + 0x114688 + 0x114C50 + 0x11477E) */
        dn_si();
        break;
    case 0x0D:
        b->o36 = (int16_t)((int32_t)o->out - m2s16(MP(d->on_lvl), 0, BTQ));   /* - the 0x0B / 0x0C level (blk60+0x28), the rest stood in */
        gp_ramp_init(rp, 0, -20000, 20000, (uint16_t)X.in2.now_ms);
        dn_si();
        break;
    case 0x0E:
    case 0x12:
        b->d32 = d->fin;                                                 /* 0x1173EE (down): ctx+0x138 blk+8 */
        if (G.bd1a != 0x0Au && G.bd1a != 0x1Eu) {
            b->o36 = b->o2;
        }
        dn_read();
        break;
    default:
        break;
    }
}

/* the on-coming output (0x113F16, down column): +2 */
static void on_out_dn(gp_blk_t *c, gp_po_t *o, const gp_dn_t *d)
{
    gp_pb_t *b = &PH.blk[bidx(c)];
    const bool com = (c->act == 1u);
    int32_t v = 0;
    switch (o->phase) {
    case 0x13:
        v = pos16(b->o36);                                               /* the kiss level */
        b->o2c = (int16_t)v;
        b->v2e = 1u;
        break;
    case 0x05:                                                           /* + 0x114CBE (turbine) stood in */
        v = b->o36 + (com ? m1s16(MP(d->on_t5), ms_since(b)) : 0);       /* 0x114BAA: blk58+0x40 vs ms since the start */
        v = pos16(v);
        b->o2c = (int16_t)v;
        b->v2e = 1u;
        dn_si();
        break;
    case 0x09:                                                           /* + 0x114688 + 0x1147B0 + 0x114C50 stood in */
        v = pos16(b->o36);
        if (v > 20000) {
            v = 20000;
        }
        dn_si();
        break;
    case 0x0A:
    case 0x0C:
    case 0x0D:
        v = pos16(b->o36 + m2s16(MP(d->on_lvl), 0, BTQ));                /* the 0x0B / 0x0C level blk60+0x28 (ctx+0xC = 0) + +0x36 */
        if (v > 20000) {
            v = 20000;
        }
        dn_si();
        break;
    case 0x0B:                                                           /* the linear ramp (+0x7C) 0 -> the level, + +0x36 */
        v = b->o36 + (int32_t)d1b94((uint16_t)o->tin, GP_SI_T_SHORT, 0, (int16_t)m2s16(MP(d->on_lvl), 0, BTQ));
        v = pos16(v);
        dn_si();
        break;
    case 0x0E:
    case 0x12:
        v = d1b94((uint16_t)o->tin, b->d32, b->o36, 20000);
        break;
    default:
        v = 0;
        break;
    }
    b->o2 = (int16_t)v;
    PH.w.on_n = sat16(PH.w.on_n);
}

/* off-going phase done (0x1106DC, down column) */
static bool off_done_dn(gp_blk_t *c, const gp_po_t *o, const gp_dn_t *d)
{
    const gp_pb_t *b = &PH.blk[bidx(c)];
    const bool com = (c->act == 1u);
    if (!com) {
        return (o->phase == 0x00u) || (o->phase == 0x01u);               /* block B: the pre-selected hold (the stand-in as before) */
    }
    switch (o->phase) {
    case 0x00: return true;
    case 0x01: dn_read(); return c->sync != 0u || o->tin >= b->p20;     /* committed: sync, or passes >= +0x20 = O50+0x1C */
    case 0x02:
    case 0x03: dn_si(); return o->tin >= GP_SI_T_SHORT;
    case 0x04: dn_si(); return o->tin >= GP_SI_T_TORQUE;
    case 0x09: dn_si(); return c->sync != 0u;
    case 0x0A: dn_read(); return c->sync != 0u || o->tin >= b->p20;    /* passes >= +0x20 (+0x24 - the earlier passes) or sync */
    case 0x0B: dn_read(); return o->tin >= (unsigned)m1u8(MP(d->of_d0b), X.cls1c);   /* O50+0xC vs 0x89C1C: passes only */
    case 0x0C:
    case 0x0D:
    case 0x0E: dn_si(); return o->tin >= GP_SI_T_SHORT;
    case 0x0F: dn_read(); return o->tin >= (unsigned)m1u8(MP(d->of_d0f), X.cls1c);   /* O50+0x28 vs 0x89C1C */
    default:   return false;                                             /* 0x11 final */
    }
}

static void off_entry_dn(gp_blk_t *c, gp_po_t *o, const gp_dn_t *d)
{
    gp_pb_t *b = &PH.blk[bidx(c)];
    (void)c;
    switch (o->phase) {
    case 0x01: {
        /* committed down: +0x20 = O50+0x1C(0x89C1C, 0x89C12); 0x89C12 not an input here (0) */
        b->p20 = (uint16_t)m2u8(MP(d->of_d1), X.cls1c, 0u);
        dn_read();
        dn_si();
        break;
    }
    case 0x0A: {
        /* +0x24 = O48+0x10(temp) + ctx+0xC4 (0) + O50+0x14(0x89C0E, 0x89C1C) - 0x80; +0x20 = +0x24 - the earlier passes (0) */
        const int32_t t24 = (int32_t)m1u8(MP(d->of_thr), BTMP) + (int32_t)m2u8(MP(d->of_p1), X.c0e, X.cls1c) - 128;
        b->p20 = (uint16_t)(t24 < 0 ? 0 : t24);
        dn_read();
        dn_si();
        break;
    }
    case 0x0F:
        b->p1a = (int16_t)(int32_t)o->out;                               /* +0x1A = +0xE: the output at the entry */
        dn_read();
        break;
    case 0x0D:
        b->p1a = (int16_t)(int32_t)o->out;                               /* the linear ramp (+0x6C) starts from the output */
        dn_si();
        break;
    default:
        break;
    }
}

static void off_out_dn(gp_blk_t *c, gp_po_t *o, const gp_dn_t *d)
{
    gp_pb_t *b = &PH.blk[bidx(c)];
    int32_t v = 0;
    (void)c;
    switch (o->phase) {
    case 0x01:
        v = 20000;                                                       /* the full binary apply of the old gear's element */
        break;
    case 0x0A:                                                           /* 0x1115E0 = 0x1111FE (stood in) + O50+0 (0x89C1C) */
        v = pos16((int32_t)m1s16(MP(d->of_slip), X.cls1c));
        dn_si();
        break;
    case 0x0B: {                                                         /* 0xEAEFC: the blend 0x1115E0 -> 0x1111FE over the 0x0B duration */
        const uint16_t big = (uint16_t)m1u8(MP(d->of_d0b), X.cls1c);
        v = d1b94((uint16_t)o->tin, big, (int16_t)m1s16(MP(d->of_slip), X.cls1c), 0);
        v = pos16(v);
        dn_si();
        break;
    }
    case 0x0C:                                                           /* ramp step (0x111860) + 0x1111FE: stood in */
        v = 0;
        dn_si();
        break;
    case 0x0D: {                                                         /* the linear ramp to 0x1116D0 = O50+0x2C(0, torque) x O48+0xC(temp) / 100 */
        const int32_t lvl = (m2s16(MP(d->of_l0d), 0, BTQ) * (int32_t)m1u8(MP(d->of_pct), BTMP)) / 100;
        v = pos16(d1b94((uint16_t)o->tin, GP_SI_T_SHORT, b->p1a, s16sat(lvl)));
        dn_si();
        break;
    }
    case 0x0E:
        v = pos16((m2s16(MP(d->of_l0d), 0, BTQ) * (int32_t)m1u8(MP(d->of_pct), BTMP)) / 100);   /* 0x1116D0 + 0x111254 (stood in) */
        dn_si();
        break;
    case 0x0F:                                                           /* ctx+0x20 == 0: 0xD1B94(passes, O50+0x28, +0x1A, 0) */
        v = pos16(d1b94((uint16_t)o->tin, (uint16_t)m1u8(MP(d->of_d0f), X.cls1c), b->p1a, 0));
        break;
    default:
        v = 0;
        break;
    }
    b->p2 = (int16_t)v;
    PH.w.off_n = sat16(PH.w.off_n);
}

/* THE PHASE-DONE DISPATCH: ported up rows, else the stand-ins (counted) */
static bool on_done(gp_blk_t *c, const gp_po_t *o)
{
    gp_rows_t r;
    const gp_dn_t *dn;
    W.si_phase_n++;
    if (c->act == 0u && o->phase >= 2u) {
        return false;                                /* block B: the pre-fill holds */
    }
    if (o->type == 1u && rows_get(c, &r)) {
        return on_done_up(c, o, &r);
    }
    if (o->type != 1u && (dn = dn_get(c)) != NULL) {
        return on_done_dn(c, o, dn);
    }
    PH.w.unp_row = sat16(PH.w.unp_row);
    if (o->type != 1u) {
        PH.w.dn_si = sat16(PH.w.dn_si);
    }
    switch (o->phase) {
    case 0x00: return true;
    case 0x01: return o->tin >= GP_SI_T_SHORT;
    case 0x02: return o->tin >= GP_SI_T_FILL;
    case 0x13: return o->tin >= 1u;
    case 0x04:
    case 0x05: return o->tin >= GP_SI_T_TORQUE;
    case 0x09: return c->sync != 0u;                 /* the inertia phase ends synchronised */
    case 0x0A:
    case 0x0D:
    case 0x0B:
    case 0x0C: return o->tin >= GP_SI_T_SHORT;
    default:   return false;                         /* 0x12 final, 0x0E, 0xFF */
    }
}
static bool off_done(gp_blk_t *c, const gp_po_t *o)
{
    gp_rows_t r;
    const gp_dn_t *dn;
    W.si_phase_n++;
    if (c->act == 0u && o->phase >= 2u) {
        return false;
    }
    if (o->type == 1u && rows_get(c, &r)) {
        return off_done_up(c, o, &r);
    }
    if (o->type != 1u && (dn = dn_get(c)) != NULL) {
        return off_done_dn(c, o, dn);
    }
    PH.w.unp_row = sat16(PH.w.unp_row);
    if (o->type != 1u) {
        PH.w.dn_si = sat16(PH.w.dn_si);
    }
    switch (o->phase) {
    case 0x00: return true;
    case 0x01:
    case 0x02:
    case 0x03: return o->tin >= GP_SI_T_SHORT;
    case 0x04: return o->tin >= GP_SI_T_TORQUE;
    case 0x09: return c->sync != 0u;
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
    case 0x0E:
    case 0x0F: return o->tin >= GP_SI_T_SHORT;
    default:   return false;                         /* 0x11 final */
    }
}

/* 0x1132F4 (the on-coming entry): only its context stores are ported —
 * phase 4 / 5 (0x1133E4..0x113400): ctx+6 = 1, BD1B == 0 → ctx+0x31 = 0;
 * 0x0D (0x1135C2..0x1135D4): +4 = +5 = +6 = 1, ctx+0x31 = 0; 0x0E / 0x12
 * (0x113698..0x1136A6): ctx+0x31 = 0 */
static void on_entry(gp_blk_t *c, gp_po_t *o)
{
    if (o->phase == 4u || o->phase == 5u) {
        if (G.bd1b == 0u) {
            c->hold = 0u;
        }
    } else if (o->phase == 0x0Du) {
        o->f4 = 1u;
        c->hold = 0u;
    } else if (o->phase == 0x0Eu || o->phase == 0x12u) {
        c->hold = 0u;
    }
}

/* the output of the objects after the step loop (0x113F16 / 0x110CDC) */
static void on_body(gp_blk_t *c)
{
    gp_rows_t r;
    const gp_dn_t *dn;
    if (c->on.type == 1u && rows_get(c, &r)) {
        on_out_up(c, &c->on, &r);
    } else if (c->on.type != 1u && (dn = dn_get(c)) != NULL) {
        on_out_dn(c, &c->on, dn);
    } else {
        PH.blk[bidx(c)].o2 = 0;
    }
}
static void off_body(gp_blk_t *c)
{
    gp_rows_t r;
    const gp_dn_t *dn;
    if (c->off.type == 1u && rows_get(c, &r)) {
        off_out_up(c, &c->off, &r);
    } else if (c->off.type != 1u && (dn = dn_get(c)) != NULL) {
        off_out_dn(c, &c->off, dn);
    } else {
        PH.blk[bidx(c)].p2 = 0;
    }
}

/* 0x112888: the on-coming step */
static void on_step(gp_blk_t *c)
{
    gp_po_t *o = &c->on;
    if (o->phase > 0x13u) {
        W.report_n++;                                /* 0xB79E4(0x365, …): counted */
    }
    if (o->elem == 0u) {
        return;
    }
    for (unsigned guard = 0u; guard < 16u; guard++) {
        if (!on_done(c, o)) {
            on_body(c);                              /* 0x113F16 */
            return;
        }
        uint8_t nx;
        unsigned k = 0u;
        do {
            o->step = sat8(o->step);
            nx = on_next(o);
        } while (nx == o->phase && ++k < 16u);       /* 0x1128F2: equal entries skipped */
        o->phase = nx;
        {
            gp_xblk_t *x = &X.blk[bidx(c)];
            x->on_c34 = o->tin;                      /* 0x1128FE..0x11290A: the old +0x1A -> +0x34, then 0 */
            if (nx == 9u) {
                x->on_ca = 0u;                       /* 0x113480 */
            }
            if (nx == 0x0Au) {
                x->on_cc = 0u;                       /* 0x1134D8 */
            }
            if (nx == 4u || nx == 5u) {
                x->on_c10 = 0u;                      /* 0x113410 */
            }
            if (nx == 4u) {
                x->on_c3e = 0u;                      /* 0x1133BE */
            }
        }
        o->tin = 0u;
        o->phases_n++;
        on_entry(c, o);
        {
            gp_rows_t r;
            const gp_dn_t *dn;
            if (o->type == 1u && rows_get(c, &r)) {
                on_entry_up(c, o, &r);
            } else if (o->type != 1u && (dn = dn_get(c)) != NULL) {
                on_entry_dn(c, o, dn);
            }
        }
        if (c == &G.a) {
            ev(GP_EV_PHON, (int8_t)nx, (int8_t)o->step, o->elem, 0u, 0u);
        }
    }
    W.err_n++;
}

/* 0x10FB72: the off-going step */
static void off_step(gp_blk_t *c)
{
    gp_po_t *o = &c->off;
    if (o->phase > 0x11u) {
        W.report_n++;                                /* 0xB79E4(0x367, …): counted */
    }
    if (o->elem == 0u) {
        return;
    }
    for (unsigned guard = 0u; guard < 16u; guard++) {
        if (!off_done(c, o)) {
            off_body(c);                             /* 0x110CDC */
            return;
        }
        uint8_t nx;
        unsigned k = 0u;
        do {
            o->step = sat8(o->step);
            nx = off_next(o);
        } while (nx == o->phase && ++k < 16u);
        o->phase = nx;
        {
            gp_xblk_t *x = &X.blk[bidx(c)];
            x->off_c22 = o->tin;                     /* 0x10FBE6..0x10FBF0: the old +0x12 -> +0x22, then 0 */
            if (nx == 0x0Du) {
                x->off_c2c = 0u;                     /* 0x110378: written at the 0x0D entry [I: 0] */
            }
        }
        o->tin = 0u;
        o->phases_n++;
        {
            gp_rows_t r;
            const gp_dn_t *dn;
            if (o->type == 1u && rows_get(c, &r)) {
                off_entry_up(c, o, &r);              /* 0x10FFBE */
            } else if (o->type != 1u && (dn = dn_get(c)) != NULL) {
                off_entry_dn(c, o, dn);
            } else {
                W.unp_n++;                           /* 0x10FFBE (the entry): not ported for this row */
            }
        }
        if (c == &G.a) {
            ev(GP_EV_PHOFF, (int8_t)nx, (int8_t)o->step, o->elem, 0u, 0u);
        }
    }
    W.err_n++;
}

/* 0x112916 / 0x10FA7E: the outputs (+0x16 / +0xE: the bodies 0x1144B0 /
 * 0x110C8E not ported → 0; on +8 = phase == 1 (0x11293E..0x112946); off +6 =
 * 0x110600 not ported → 0) */
static void outputs(gp_blk_t *c)
{
    const gp_pb_t *b = &PH.blk[bidx(c)];
    if (c->on.elem != 0u) {
        c->on.out = (uint16_t)pos16(b->o2);          /* 0x1144B0: + the torque term E9464 (0), clamped >= 0 */
        c->on.mode = (uint8_t)((c->on.phase == 1u) ? 1u : 0u);
    }
    if (c->off.elem != 0u) {
        c->off.out = (uint16_t)pos16(b->p2);         /* 0x110C8E: + E945E (0) */
        c->off.mode = (uint8_t)((c->off.phase == 1u || c->off.phase == 0x11u) ? 1u : 0u);   /* 0x110600 [I] */
    }
}

/* ── 0x11755E(ctx, p1, p2, p3) ─────────────────────────────────────────── */
static void p55e(gp_blk_t *c, bool p1, bool p2, bool p3)
{
    if (c->act == 1u) {
        W.unp_n++;                                   /* 0xE9446(E6CC) */
    }
    if (p1) {
        c->hold = 0u;                                /* +0x31 */
        c->p27 = 1u;                                 /* +0x27 */
    }
    if (p2) {
        on_end(&c->on);                              /* +0x18 = 0x1132B8; 0x11294C */
        PH.blk[bidx(c)].o2 = 0;
    }
    if (p3) {
        off_end(&c->off);                            /* +0x1A = 0x10FF9A; 0x10FBFC */
        PH.blk[bidx(c)].p2 = 0;
    }
}

/* 0x1176C4(ctx, from, to) / 0x1176BA(ctx, on, off) */
static void c6c4(gp_blk_t *c, int8_t f, int8_t t)
{
    c->from = f;
    c->to = t;
    c->kind = (uint8_t)b7c14(f, t);
}
static void c6ba(gp_blk_t *c, uint8_t on, uint8_t off)
{
    c->on.elem = on;
    c->off.elem = off;
}

/* 0xEC134(BD14, to, from): the chained sub-type of a 4→3 / 3→2 behind a
 * 6 / 8 / 5 shift (bytes 0xEC134..0xEC180), else block A's +5 */
static uint8_t ec134(int8_t to, int8_t from)
{
    uint8_t r = G.a.kind;
    if (to == 3 && from == 4 && (G.a.from == 6 || G.a.from == 8)) {
        r = (G.a.from == 6) ? 6u : 5u;
    }
    if (to == 2 && from == 3 && (G.a.from == 5 || G.a.from == 6)) {
        r = (G.a.from == 6) ? 7u : 8u;
    }
    return r;
}

/* 0xED988(BD14, BD16): BD17 */
static void ed988(void)
{
    if (G.bd16 == 3u || G.bd16 == 4u) {
        G.bd17 = 4u;
        return;
    }
    const unsigned i = (unsigned)G.bd1e * 3u + G.bd16;
    if (i >= 9u) {
        W.err_n++;
        return;
    }
    G.bd17 = gp_cal_bd17[i];
}

/* 0xED9B2(BD14, p) */
static void ed9b2(uint8_t p)
{
    gp_blk_t *c = &G.a;
    if (p == 1u) {
        G.bd1e = GP_SI_BD1E;                         /* 0xEC788(BD14, BD6E, BD6D, 2, 0) */
        W.si_misc_n++;
        G.bd22 = G.in.gf.cd2;                        /* 0x89CD2 */
        G.bd1b = 0u;                                 /* +7 */
        G.bd67 = (uint8_t)((G.bd1e == 1u) ? 0xFFu : 0u);   /* +0x53 */
        G.a.f4 = 0u;
        G.b.f4 = 0u;
    } else if (p == 0u) {
        G.a.f4 = 0u;
        G.b.f4 = 0u;
        c = &G.b;
    }
    p970(c);
    ed988();
    W.unp_n++;                                       /* +5 = 0xE941E(E6CC, …) */
}

/* 0xED906(BD14, p): 0xED988, then 0x117B4A(ctx(p), 1) */
static void ed906(uint8_t p)
{
    ed988();
    XW.k_start = sat16(XW.k_start);
    if (p != 0u) {
        b4a(&G.a, &X.blk[0], 1u);
    } else {
        b4a(&G.b, &X.blk[1], 1u);
    }
}

/* the body state at a shift start: the cached terms cleared, the ms stamp (ctx+0x16) taken [I: the stock
 * stamps it at phase 9; before that the stand-in is the shift start] */
static void pb_start(gp_blk_t *c)
{
    gp_pb_t *b = &PH.blk[bidx(c)];
    b->o2 = 0;
    b->o36 = 0;
    b->o2c = 0;
    b->d32 = 0u;
    b->v2e = 0u;
    b->f44 = 0u;
    b->p2 = 0;
    b->p1a = 0;
    b->p3s = 0;
    b->p20 = 0u;
    b->pfill = 0u;
    b->ms16 = (uint16_t)X.in2.now_ms;
}

/* 0xEDC5A(BD14, p): the start dispatcher (bytes 0xEDC5A..0xEDE1A) */
static void edc5a(uint8_t p)
{
    if (G.bd16 == 0u) {
        return;
    }
    G.bd45 = 0u;                                     /* +0x31 */
    if (p == 0u || G.bd18 == 0u) {
        ed9b2(p);
    }
    ed906(p);                                        /* 0xED906 → 0xED988 + 0x117B4A(ctx(p), 1) */
    reload();                                        /* 0xED92E (0x1176E0, torque): counted */
    if (p == 0u) {
        W.unp_n++;                                   /* 0x117C48 / 0x1179D8 */
        p93e(&G.b);
    } else {
        G.bd68 = 0u;                                 /* +0x54 */
        G.bd41 = 0u;                                 /* +0x2D */
        G.bd49 = 0u;                                 /* +0x35 */
        W.unp_n++;                                   /* 0xEE2B6, 0xE9E5A (line pressure) */
        if (G.bd18 == 0u) {
            p93e(&G.a);                              /* (0x117C48), 0x11793E */
        }
        W.unp_n++;                                   /* 0x1179D8, 0xEDBA4, torque / notifications */
    }
    if (G.bd17 == 4u) {
        W.unp_n++;                                   /* 0xAED2E, 0xEEB9C(E6CD, BD17, BD16) */
    }
    if (G.bd16 != 4u && G.bd16 != 3u && (p == 0u || G.bd18 == 0u)) {
        gp_blk_t *c = (p != 0u) ? &G.a : &G.b;
        on_start(&c->on);                            /* 0x11299A */
        X.blk[bidx(c)].on_cc = 0u;                   /* 0x11299A: +0xC = 0 */
        off_start(&c->off);                          /* 0x10FC5C */
        pb_start(c);
    }
    if (p == 1u) {
        X.blk[0].on_c1c = 0u;                        /* 0x112142 (A on): +0x1C = 0 [I: the decode's 0x112176] */
        W.unp_n++;                                   /* 0x112142's rest, 0xE5E48, 0xE9404 */
    }
}

/* 0xDDBB4(E33C, from, to, p4) */
static void ddbb4(int8_t f, int8_t t, uint8_t p4)
{
    const int8_t ty = b7c02(f, t);
    if (ty == 0) {
        if (p4 != 0u) {
            G.e5 = 0u;
            G.e6 = 0u;
        } else {
            G.e0 = 0u;
            G.e1 = 0u;
        }
        return;
    }
    const uint8_t on = eltab(gp_cal_on, f, t), off = eltab(gp_cal_off, f, t);
    if (p4 != 0u) {
        G.e5 = on;
        G.e6 = off;
    } else {
        G.e0 = on;
        G.e1 = off;
    }
}

/* 0xEB9EC(BD14, gear code, cmd, on, off): the pre-select into block B */
static void eb9ec(int8_t gc, int8_t cmd, uint8_t on, uint8_t off)
{
    W.presel_n++;
    G.bd45 = 0u;                                     /* +0x31 */
    if (G.in.ld151 != 0u) {                          /* 0xEEB04: *0xFFF8D151 (live, by value) */
        W.presel_busy_n++;
        return;
    }
    c6c4(&G.b, gc, cmd);
    c6ba(&G.b, on, off);
    const int8_t ty = b7c02(gc, cmd);
    G.b.x1e = ec134(cmd, gc);                        /* BD14+0x1BE */
    if (ty == 0) {
        W.presel_t0_n++;
        p55e(&G.b, true, true, true);
    } else {
        G.bd15 = 1u;
        edc5a(0u);
    }
    ev(GP_EV_PRESEL, gc, cmd, on, off, (uint8_t)ty);
}

/* the 0xDD954 change block (0xDD98C..0xDDA2A) */
void gp_cpl_dd954_change(int8_t c3, int8_t c2b, int8_t c25, int8_t prev)
{
    if (c3 == c2b && c2b != c25 && G.e5 != 0u) {
        if (prev != c2b) {
            G.e14 = 0u;
        }
        if (G.e14 != 0u) {
            if (G.e1 == 0u) {
                G.e1 = G.e14;
                G.e14 = 0u;
            } else {
                W.report_n++;                        /* 0xB79E4(0x373, …, 0xD6) */
            }
        }
    } else {
        G.e14 = 0u;
    }
    ddbb4(c2b, c3, 1u);
    eb9ec(c2b, c3, G.e5, G.e6);
}

/* 0x117CE4(ctx): the phase objects' context pointers are re-pinned to their own
 * context (on +0x48 / off +0x38); the swap moved them with the objects, so
 * after it every object points at the OTHER context until this runs. The
 * pair-cell pointers (the ids) are not re-pinned: they travel. */
static void repin(void)
{
    for (unsigned k = 0u; k < 2u; k++) {
        gp_xblk_t *x = &X.blk[k];
        XW.repin_n = (uint16_t)(XW.repin_n + (x->xon.ctxp != k ? 1u : 0u) + (x->xoff.ctxp != k ? 1u : 0u));
#if !defined(GP_NEG_SWAP)
        x->xon.ctxp = (uint8_t)k;
        x->xoff.ctxp = (uint8_t)k;
#endif
    }
}

/* 0xECB14: the 0x148-byte contexts A (0xBD6C) and B (0xBEB4) exchanged field
 * by field through BD14+0x320 — both blocks with BOTH embedded phase objects
 * (counters, pair-pointer ids, the 16-byte ramp objects) — then 0x117CE4 on
 * both, A+3 = 1, B+3 = 0. NOT moved by the stock (padding [I], +0x140..0x147
 * undecoded): nothing is modelled there. */
static void ecb14(void)
{
    const gp_blk_t t = G.a;                          /* A -> temp */
    const gp_xblk_t xt = X.blk[0];
    const gp_ramp_t rt0 = s_gr[0][0], rt1 = s_gr[0][1];
    const gp_pb_t pt = PH.blk[0];                    /* the phase-body state travels with its context */
    G.a = G.b;                                       /* B -> A */
    X.blk[0] = X.blk[1];
#if !defined(GP_NEG_SWAP)
    PH.blk[0] = PH.blk[1];
    PH.blk[1] = pt;
#else
    (void)pt;
#endif
#if !defined(GP_NEG_SWAP)
    s_gr[0][0] = s_gr[1][0];
    s_gr[0][1] = s_gr[1][1];
#endif
    G.b = t;                                         /* temp -> B */
    X.blk[1] = xt;
#if !defined(GP_NEG_SWAP)
    s_gr[1][0] = rt0;
    s_gr[1][1] = rt1;
#else
    (void)rt0;
    (void)rt1;
#endif
    repin();
    G.a.act = 1u;
    G.b.act = 0u;
    ev(GP_EV_SWAP, G.a.from, G.a.to, G.a.on.elem, G.a.off.elem, 0u);
}

/* 0xEBA6E(BD14, old, new, on, off, skip, presel) (bytes 0xEBA6E..0xEBB6A) */
static void eba6e(int8_t old, int8_t nw, uint8_t on, uint8_t off, uint8_t skip, uint8_t presel,
                  uint8_t e69d)
{
    W.commit_n++;
    if (presel == 0u) {
        c6c4(&G.a, old, nw);
        c6ba(&G.a, on, off);
        G.bd16 = e69d;                               /* 0xB6AAE(E5BD) = *0xFFF8E69D */
        G.a.x1e = 0u;                                /* BD8A */
    } else {
        W.commit_ps_n++;
        if (!(G.b.to == nw && G.b.from == old && G.b.on.elem == on && G.b.off.elem == off)) {
            W.mismatch_n++;
            W.report_n++;                            /* 0xB79E4(0x36B, 0x13, 0x53) */
        }
        ecb14();
        G.bd5d = 0u;                                 /* +0x49 */
        c6c4(&G.a, old, nw);
        c6ba(&G.a, on, off);
        G.bd16 = e69d;
    }
    ev(GP_EV_COMMIT, old, nw, on, off, presel);
    if (G.bd16 != 0u) {
        G.bd15 = 1u;
        G.bd3c = skip;                               /* +0x28 */
        G.bd1a = (uint8_t)((skip != 0u) ? G.bd40 : 0u);
        G.bd40 = 0u;
        G.bd18 = presel;                             /* +4 */
        edc5a(1u);
    } else {
        G.bd15 = 0u;
        G.bd1a = 0u;
        G.bd40 = 0u;
    }
}

/* 0xDDDE8(E33C, old, new, skip, presel) (bytes 0xDDDE8..0xDDE76) */
void gp_cpl_ddde8(int8_t old, int8_t nw, uint8_t skip, uint8_t presel, uint8_t e69d)
{
    W.unp_n++;                                       /* 0xEAC64(E3EC), 0xDE2FA */
    ddbb4(old, nw, 0u);
    G.e10 = (int16_t)G.in.gf.cd0;                    /* +0x10 = 0x89CD0 */
    G.ea = (int16_t)G.in.gf.cba;                     /* +0xA = 0x89CBA */
    eba6e(old, nw, G.e0, G.e1, skip, presel, e69d);
    W.unp_n++;                                       /* 0xE9508(E3DE), 0xEECEC(C190) */
}

/* 0xEBC60(BD14, on, off, skip, presel): THE GEAR-PAIR END (bytes
 * 0xEBC60..0xEBD82) */
static void ebc60(uint8_t on, uint8_t off, uint8_t skip, uint8_t presel)
{
    uint8_t nv = 0u;
    if (skip != 0u && presel != 0u) {
        W.report_n++;                                /* 0xB79E4(0x36B, 7, 0x16, 0xB0) */
    }
    if (G.bd1a == 50u) {
        G.bd1a = 0u;
    }
    if (G.bd48 != 0u) {                              /* +0x34: the abort pending */
        G.bd48 = 0u;
        return;
    }
    G.bd38 = G.a.off.out;                            /* +0x24 = 0x10FF9A(A off) */
    if (G.bd16 != 0u) {
        G.bd15 = 0u;
        W.unp_n++;                                   /* 0xE5FF6(B9D8, …), 0xAED3A(E5BB, …) */
        if (G.bd41 == 1u) {
            /* 0xBA52C(E613, BD6E, BD6D, 0x89C30) → 0xE874A → 0xE82A4 → 0xE8512:
             * the NV shift counter — NOT PORTED, NOT CALLED: counted */
            W.nv_abort_n++;
            nv = 1u;
        }
    }
    W.unp_n++;                                       /* 0xEACE0 / 0xEAC30 (E3EC) */
    W.end_n++;
    ev(GP_EV_END, G.a.from, G.a.to, G.bd41, nv, 0u);
    p55e(&G.a, true, true, true);
    c6ba(&G.a, on, off);
    W.unp_n++;                                       /* 0x112142(A on, 1) */
}

/* 0xBAAE4 → 0xDDE78(E33C, g, skip, presel) (bytes 0xDDE78..0xDDED4) */
void gp_cpl_dde78(int8_t g, uint8_t skip, uint8_t presel)
{
    ddbb4(g, g, 0u);                                 /* E33C+0 / +1 of a g → g "shift" */
    ebc60(G.e0, G.e1, skip, presel);
    W.unp_n++;                                       /* 0xEEF42, 0xE958A, 0xAEE06, 0xB8574 */
    W.si_misc_n++;                                   /* 0xB6AC4(E5BD) == 0: E33C+0 not restored */
}

/* the live garage end → 0xEBB6C(BD14, from, to, 0) (bytes 0xEBB6C..0xEBC5E):
 * BD15 = 0 (0xEBBEE); BD16 != 0 → 0xE5FF6 / 0xAED22, 0x11755E(A, 1, 1, 1),
 * 0x1176BA(A, 0, 0); +0x2D == 1 ∧ BD16 != 0 ∧ from != to → 0xBA538 (NOT
 * CALLED: counted; from / to of a garage are power-flow codes the live
 * engine keeps — taken as different [I]) */
static void ebb6c(uint8_t live_bd41)
{
    W.gend_n++;
    G.bd15 = 0u;
    if (G.bd16 != 0u) {
        W.unp_n++;
        p55e(&G.a, true, true, true);
        c6ba(&G.a, 0u, 0u);
    }
    uint8_t nv = 0u;
    if (live_bd41 == 1u && G.bd16 != 0u) {
        /* 0xBA538(E613, from, to, BD6D) → 0xE8774 → 0xE82AC → 0xE8512 */
        W.nv_end_n++;
        nv = 1u;
    }
    W.unp_n++;                                       /* 0xEAC30 */
    ev(GP_EV_GEND, 0, 0, live_bd41, nv, G.bd16);
}

/* 0xEDA48(BD14): the completion (bytes 0xEDA48..0xEDBA2) */
static bool eda48(uint8_t e344)
{
    G.bd41 = 0u;                                     /* 0xEDA56 */
    if (G.bd16 == 4u || G.bd16 == 3u) {
        /* the garage branches (0xEEAA4 on the garage engine): done when the
         * live garage no longer owns the shift object [I] */
        W.si_misc_n++;
        return G.own == 0u;
    }
    if ((G.a.on.elem == 0u && G.b.on.elem == 0u) || G.bd16 == 0u || G.bd15 == 0u) {
        W.report_n++;                                /* 0xB79E4(0x36B, 4, 0x15, 0xD1) */
        return true;
    }
    if (G.bd1a == 50u) {
        return true;
    }
    /* 0xB8246(E5C4, BE2F): the on-coming element applied — stand-in [I]:
     * block A's on-coming phase object in its final phase 0x12 */
    W.si_appl_n++;
    /* a ported on-coming body: applied = its request reached the 13030 mbar of the stock's applied test (phase 0x12
     * commands 20000 one pass after its entry); the sequence-only rows keep the phase-0x12 stand-in */
    gp_rows_t rr;
    const bool ported = (G.a.on.type == 1u && rows_get(&G.a, &rr)) || (G.a.on.type != 1u && dn_get(&G.a) != NULL);
    const bool applied = (G.a.on.phase == 0x12u) && (!ported || G.a.on.out >= 13030u);
    if (applied && G.a.sync != 0u && e344 == 0u) {
        G.bd41 = 1u;                                 /* 0xEB49C */
    }
    if (G.bd41 != 0u) {
        W.done_n++;
        ev(GP_EV_DONE, G.a.from, G.a.to, 1u, 0u, 0u);
        return true;
    }
    /* BE20 (on +4) ∧ BE26 (on +0xA) >= BD90 (A+0x24): the timer branch —
     * +0xA / A+0x24 belong to the undecoded bodies: not modelled */
    if (G.a.on.f4 != 0u) {
        W.si_misc_n++;
    }
    return false;
}

/* 0xEB1F0(BD14): BD16 == 0 ∨ 0xEC74C (0x89C30 != 1) → 1, else 0xEDA48 */
bool gp_cpl_eb1f0(uint8_t pf, uint8_t e344)
{
    if (G.bd16 == 0u || pf != 1u) {
        return true;
    }
    return eda48(e344);
}

/* 0xDDA54's BD15 term (0xDDA78..0xDDAA4) and 0xEB1E8 (+0x2D = 1) */
uint8_t gp_cpl_dda54(int base)
{
    if (G.bd15 == 0u) {
        return (uint8_t)(base != 0 ? 1u : 0u);
    }
    const uint8_t r = (uint8_t)((base != 0 && (G.a.sync != 0u || G.a.f4 != 0u)) ? 1u : 0u);
    if (r != 0u) {
        G.bd41 = 1u;                                 /* 0xEB1E8 */
        W.eb1e8_n++;
    }
    return r;
}

/* 0xEB7BC(BD14, cmd7, E6A1) (bytes 0xEB7BC..0xEB80C) */
bool gp_cpl_eb7bc(int8_t cmd7, int8_t e6a1)
{
    if (G.bd15 == 0u) {
        return true;
    }
    W.block_n++;
    if (e6a1 != cmd7) {
        return false;
    }
    if (e6a1 == G.a.to) {
        return true;
    }
    if (GP_BD26 == 0u) {
        return false;
    }
    W.si_misc_n++;                                   /* 0xEC90A / 0xEC89A: stand-in 0 → 1 */
    return true;
}

uint8_t gp_cpl_bd15(void)
{
    return G.bd15;
}
uint8_t gp_cpl_bd40(void)
{
    return G.bd40;
}

uint8_t gp_cpl_eb73e(void)
{
    W.si_misc_n++;
    /* 0xEB73E undecoded below 0xEB8D4 / 0xEB8E0: 1 (drop the pre-selected
     * gear, = the r04w chain's choice); with the host's chained pre-select
     * seam 0 (hold it — its 0xEB8E0 branch returns 0 with BD14+0x26 = 0) */
    return (GP_EARLY_OK != 0u) ? 0u : GP_SI_EB73E;
}

/* 0xEB5A6(BD14, c25, c2b, next) ∧ 0xB6BBC(E5BD, c2b, next, ne, nt) == next */
bool gp_cpl_early_ok(int8_t c25, int8_t c2b, int8_t nx, uint16_t ne, uint16_t nt)
{
    (void)c25;
    (void)c2b;
    (void)nx;
    (void)ne;
    (void)nt;
    W.si_misc_n++;
    return GP_EARLY_OK != 0u;
}

/* the slot-34 cyclic 0xEBDBC's kind part (bytes 0xEBE9A..0xEBFBA) after the
 * 0x1175D6 sync pair: BD15 != 0 -> 0x117C0C (the +4 flag: not ported), BD1B
 * 0xEC6A6, BD1E 0xEC788 (stand-in) and the 0x117B4A re-evaluations with the
 * 0xED92E reloads on a kind change; BD15 == 0 -> BD68 = 0, BD1E = 0x89CFA,
 * 0x117B4A on both */
static void slot34_kind(void)
{
    const uint8_t kb = X.blk[1].k7, ka = X.blk[0].k7;
    if (G.bd15 != 0u) {
        W.unp_n++;                                   /* 0x117C0C (ctx+4) */
        ec6a6();
        W.si_misc_n++;                               /* 0xEC788 -> GP_SI_BD1E */
        if (GP_SI_BD1E != G.bd1e) {
            G.bd1e = GP_SI_BD1E;
            ed906(1u);
            reload();
        } else {
            if (G.bd17 != 4u) {
                XW.k_cyc = sat16(XW.k_cyc);
                b4a(&G.b, &X.blk[1], 0u);
                if (X.blk[1].k7 != kb) {
                    reload();
                }
            }
            XW.k_cyc = sat16(XW.k_cyc);
            b4a(&G.a, &X.blk[0], 0u);
            if (X.blk[0].k7 != ka) {
                reload();
            }
        }
        if (G.bd1e == 0u) {
            G.bd67 = 0u;                             /* +0x53 */
        }
        W.unp_n++;                                   /* 0xE941E (BD14+5), 0x117622 x2, 0xEDE1C, 0xEE9A4 */
    } else {
        G.bd68 = 0u;                                 /* +0x54 */
        G.bd1e = GP_SI_CFA;                          /* +0xA = *0xFFF89CFA */
        XW.k_cyc = sat16(XW.k_cyc);
        b4a(&G.a, &X.blk[0], 0u);
        XW.k_cyc = sat16(XW.k_cyc);
        b4a(&G.b, &X.blk[1], 0u);
        X.blk[0].on_c1c = 0u;                        /* 0x112142 (A on) [I] */
        /* 0xEACE0, 0x112DB0, 0xEB476, 0x1176B6 x2: idle, no effect on the shadow's cells (not counted) */
    }
}

/* the would-be producer requests, ids 1..4 (0xDD7B8 / 0xDD862 roles 0..3) */
static void prod_one(gp_prod_t *p, uint8_t elem, const gp_po_t *o)
{
    p->elem = elem;
    p->phase = o->phase;
    p->mode = o->mode;
    p->value = o->out;
    if (elem != 0u && p->n != 0xFFFFu) {
        p->n++;
    }
}

/* ── the 10 ms pass ────────────────────────────────────────────────────── */
void gp_shadow_pass(const gp_in_t *in)
{
    G.in = *in;
    if (in->gf.os_mode < 1u || in->gf.os_mode > 3u) {
        W.skip_mode_n++;
        return;
    }
    W.pass_n++;
    /* the live garage owns the shift object (0xEB900 .. 0xEBB6C): mirror */
    const uint8_t own = (uint8_t)((in->lbd15 != 0u && (in->lbd16 == 3u || in->lbd16 == 4u)) ? 1u : 0u);
    if (own != 0u) {
        if (G.own == 0u && G.bd15 != 0u && G.bd17 != 4u) {
            W.unp_n++;                               /* 0xEB900's abort of a gear shift (0xEBC60) */
        }
        G.bd15 = 1u;
        G.bd16 = in->lbd16;
        G.bd17 = in->lbd17;
        W.own_n++;
    } else if (G.own != 0u) {
        G.own = 0u;
        ebb6c(in->lbd41);
    }
    G.own = own;
    /* the inputs the retirements read: 0x89C1C / 0x89C0E (0xB986C's classes) */
    X.cls1c = clamp_u8((int32_t)X.in2.tq / 6 + 34);
    X.c0e = clamp_u8((int32_t)in->gf.cba / 32);
    /* slot 28: the counters (0xEC0E0, 0x117538 x2) — before the slot-35 steps */
    ctr_pass();
    /* slots 30..33: the coupled gear chain (its hooks call into this TU) */
    gp_gear_pass(&in->gf);
    /* slot 34 (0xEBDBC → 0x1175D6, the sync part only) on both blocks: the
     * hold +0x28 = BD17 == 4 ? *0xFFF8D0E0 : +0x31 */
    {
        const uint8_t ha = (G.bd17 == 4u) ? in->ld0e0 : G.a.hold;
        const uint8_t hb = (G.bd17 == 4u) ? in->ld0e0 : G.b.hold;
        f48(&G.a, ha, G.a.p27);
        f48(&G.b, hb, G.b.p27);
    }
    prog_pass(&G.a);                                 /* 0x1175D6: the DT1 rate and BD76 of both contexts */
    prog_pass(&G.b);
    slot34_kind();
    /* slot 35: 0xEB348 (while BD17 != 4): the steps on B, on A, off B, off A;
     * the outputs */
    if (G.bd17 != 4u) {
        for (unsigned k = 0u; k < 2u; k++) {
            const gp_blk_t *c = (k == 0u) ? &G.a : &G.b;
            if (c->on.elem != 0u && c->on.phase > 2u && PH.blk[k].pfill != 0xFFu) {
                PH.blk[k].pfill++;                   /* [I] passes since the on-coming left its fill */
            }
        }
        on_step(&G.b);
        on_step(&G.a);
        off_step(&G.b);
        off_step(&G.a);
        outputs(&G.b);
        outputs(&G.a);
    }
    /* the producer's would-be requests (ids 1..4) */
    prod_one(&G.prod[0], G.e0, &G.a.on);
    prod_one(&G.prod[1], G.e1, &G.a.off);
    prod_one(&G.prod[2], G.e5, &G.b.on);
    prod_one(&G.prod[3], G.e6, &G.b.off);
    /* the coupled chain against the r04w chain */
    {
        const volatile gf_t *c = gp_gear_state();
        if (c->e4a7 != in->r_e4a7 || c->e69f != in->r_e69f) {
            W.dis_n++;
        }
    }
}

void gp_hook_time(uint32_t us)
{
    W.us_last = us;
    if (us > W.us_max) {
        W.us_max = us;
    }
}

void gp_shadow_set_in2(const gp_in2_t *in2)
{
    X.in2 = *in2;
}

const volatile gp_x_t *gp_shadow_x(void)
{
    return &s_gx;
}

const volatile gp_ph_t *gp_shadow_ph(void)
{
    return &s_gph;
}

const volatile gp_ramp_t (*gp_shadow_ramps(void))[2]
{
    return (const volatile gp_ramp_t (*)[2])s_gr;
}

#if !defined(__sh__)
void gp_shadow_host_ecb14(void)
{
    ecb14();
}
int32_t gp_host_m1s16(const uint8_t *m, int16_t x) { return m1s16(m, x); }
uint8_t gp_host_m1u8(const uint8_t *m, uint8_t x) { return m1u8(m, x); }
int32_t gp_host_m2s16(const uint8_t *m, int16_t x, int16_t y) { return m2s16(m, x, y); }
uint8_t gp_host_m2u8(const uint8_t *m, uint8_t x, uint8_t y) { return m2u8(m, x, y); }
gp_t *gp_shadow_host_mut(void)
{
    return &s_gp;
}
gp_x_t *gp_shadow_host_xmut(void)
{
    return &s_gx;
}
gp_ramp_t (*gp_shadow_host_rmut(void))[2]
{
    return s_gr;
}
#endif

void gp_shadow_init(void)
{
    uint8_t *p = (uint8_t *)&s_gp;
    for (size_t i = 0u; i < sizeof s_gp; i++) {
        p[i] = 0u;
    }
    p = (uint8_t *)&s_gx;
    for (size_t i = 0u; i < sizeof s_gx; i++) {
        p[i] = 0u;
    }
    p = (uint8_t *)s_gr;
    for (size_t i = 0u; i < sizeof s_gr; i++) {
        p[i] = 0u;
    }
    p = (uint8_t *)&s_gph;
    for (size_t i = 0u; i < sizeof s_gph; i++) {
        p[i] = 0u;
    }
    /* the phase objects' pointers: each points at its own context; the three pair-cell ids per object are
     * one cell each [I: the constructor's assignment is undecoded] */
    for (unsigned k = 0u; k < 2u; k++) {
        X.blk[k].xon.ctxp = (uint8_t)k;
        X.blk[k].xoff.ctxp = (uint8_t)k;
        for (unsigned j = 0u; j < 3u; j++) {
            X.blk[k].xon.pair[j] = (uint8_t)(6u * k + j);
            X.blk[k].xoff.pair[j] = (uint8_t)(6u * k + 3u + j);
        }
    }
    X.in2.pedal = 23u;                               /* 0x13CA22: the no-frame value */
    G.a.act = 1u;                                    /* the active context (0xECB14 / boot) */
    G.a.dir = 0xFFu;
    G.b.dir = 0xFFu;
    G.bd17 = 4u;                                     /* the dump's BD17 (0xFFF8BD17 = 04) */
    W.init_n = 1u;
}

const volatile gp_t *gp_shadow_state(void)
{
    return &s_gp;
}
