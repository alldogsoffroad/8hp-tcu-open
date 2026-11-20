/* sm_gear.c — THE GEAR FEEDBACK CHAIN of the READ-ONLY shift-map shadow
 * (app/sm_shadow.h "THE GEAR FEEDBACK CHAIN"; 2026-11-19, ~/tcu_sweep/
 * gear_feedback_shadow_port.md; decode m6_shiftmap_gaps_decode.md §3,
 * b2_slots_subobj_decode.md, gear_code_decode.md).
 *
 * The strategy's "current gear" 0x89C28 is the COMMANDED gear E4A7 (written
 * by 0x104A28), and the target A697 → A0AA → 0x89CE8 feeds it back through
 * 0x10485E → 0xD15A6 → 0x104A28: a loop. This TU ports that chain from the
 * bytes (sh-elf-objdump of the pristine image, literal pools resolved) on its
 * OWN copies of the stock cells (gf_t, section .sm_gear). The live engine's
 * E69B / E6EE / E6EF / E28E, 0x89C3D, 0x89CD2 and the speeds arrive BY VALUE
 * in gf_in_t; nothing live is written. The shadow's 0x89C28 is the gear_in of
 * the shift-target pass (app/sm_hook.c).
 *
 * Every function names the stock routine it ports. Stand-ins (healthy
 * vehicle, the frame of the live engine's GS_TARGET_1ST / D-G5-5 (a)):
 *   AE2D = 0x89C43 = 0x89C42 = 0     the fault-free gear code (the stock on
 *                                    THIS bench: code 0x20 → 0x89C43 = 3,
 *                                    AE2D 1 — reaction 13 of U11E4; host seam
 *                                    sm_gear_host_si reproduces the dump)
 *   AE2B = AE2C = AE56 = AE71 = 0    dump; AE70[1] = 0 (dump idle); E298 = 0
 *                                    (0xD11D0 → 0, dump); E4A4[1] = 0
 *                                    (0x1048C8 not ported, dump 0)
 *   the shift object 0xFFF8BD14      IDLE: +1 = 0 (0xEB7BC → 1), +2 = 0
 *                                    (0xEB1F0 → 1), BD15 = 0, +0x2C = 0 (dump
 *                                    00 00 00 04 / BD15 00) — the gear-pair
 *                                    engine (0xEB9EC block B) is not ported:
 *                                    a strobed gear completes in the same pass
 *   0x89BC0 = 0, 0xFFF8AFD4 = 0      dump (0xDD29E → 1 for a gear ≠ 0)
 *   0x89B36 = 0                      dump (0xAEA3A → 0)
 *   0x89CA0 = 0                      dump (0xD1854's second test)
 *   0xD0496 = 0, 0xD0304 not run     the AE30 sequencer object (undecoded)
 */
#define TCU_SM_SHADOW_TU 1
#include "app/sm_shadow.h"
#include "app/sm_cal.h"

#include <stdbool.h>

/* (2026-11-20, ~/tcu_sweep/gp_shadow_port.md) THE COUPLED INSTANCE: app/
 * gp_gear.c compiles this file a second time with SMG_COUPLED = 1 (its own
 * state on its own RAM .gp_gear, its own function names) and binds the shift
 * object 0xFFF8BD14's stand-ins below to the READ-ONLY gear-pair shadow
 * (app/gp_shadow.c): BD15 / BD16 / +0x2C, 0xEB7BC, 0xDDA54's BD15 term,
 * 0xEB1F0 → 0xEDA48, 0xBAAC8 → 0xDDDE8 (the commit), 0xBAAE4 → 0xDDE78 (the
 * end), the full 0xDD954 / 0xDE190 (the pre-select). Without SMG_COUPLED
 * (this TU, app/sm_gear.c, the r04w chain) every hook is the idle stand-in
 * it always was — the object code is unchanged. */
#if !defined(SMG_COUPLED)
#define SMG_COUPLED 0
#endif
#if !defined(SMG_SECTION)
#define SMG_SECTION ".sm_gear"
#endif

#if defined(__sh__)
#define GFSEC __attribute__((section(SMG_SECTION)))
#else
#define GFSEC
#endif

static gf_t s_gf GFSEC;

/* ── the stand-ins ─────────────────────────────────────────────────────── */
#define GF_SI_AE2B   0u     /* 0xFFF8AE2B */
#define GF_SI_AE2C   0u     /* 0xFFF8AE2C */
#define GF_SI_AE56   0u     /* 0xFFF8AE56 (s16) */
#define GF_SI_AE70   0u     /* 0xFFF8AE70[1] */
#define GF_SI_AE71   0u     /* 0xFFF8AE71 */
#define GF_SI_E298   0u     /* 0xFFF8E298 state (0xD11D0) */
#if SMG_COUPLED == 0
#define GF_SI_BD14_1 0u     /* 0xFFF8BD15 = BD14+1 (0xEB7BC: 0 → 1) */
#define GF_SI_BD14_2 0u     /* BD14+2 (0xEB1F0) */
#define GF_SI_BD15   0u     /* 0xFFF8BD15 (0xBABD8) */
#define GF_SI_BD40   0u     /* BD14+0x2C (0xEADA6) */
#endif
#define GF_SI_9BC0   0u     /* 0xFFF89BC0 (0xB5B2E: == 0 → 1) */
#define GF_SI_AFD4   0u     /* 0xFFF8AFD4 */
#define GF_SI_9B36   0u     /* 0xFFF89B36 (0xAEA3A: == 2) */
#define GF_SI_9CA0   0      /* 0xFFF89CA0 (s16) */
_Static_assert(GF_SI_AE2B == 0u && GF_SI_AE2C == 0u && GF_SI_AE56 == 0u && GF_SI_AE70 == 0u &&
               GF_SI_AE71 == 0u && GF_SI_E298 == 0u &&
               GF_SI_9BC0 == 0u && GF_SI_AFD4 == 0u && GF_SI_9B36 == 0u &&
               GF_SI_9CA0 == 0,
               "sm_gear: every stand-in equals the stock driving dump (stockram_driving_20261104)");
#if SMG_COUPLED == 0
_Static_assert(GF_SI_BD14_1 == 0u && GF_SI_BD14_2 == 0u && GF_SI_BD15 == 0u,
               "sm_gear: the idle shift object (the dump's BD15 00 / BD16 00)");
/* the idle shift object's hooks (the r04w chain) */
#define GF_EB7BC(c7, t)          (GF_SI_BD14_1 == 0u)
#define GF_DDA54(base)           (base)
#define GF_EB1F0(in)             (GF_SI_BD14_2 == 0u)
#define GF_UNP_EB1F0()           (s_gf.unp_n++)
#define GF_UNP_AFC()             (g->unp_n++)
#define GF_DDE78(g0, sk, ps)     ((void)0)
#define GF_DDDE8(o, n, sk, ps)   ((void)0)
#else
#include "app/gp_shadow.h"
#define GF_SI_BD15               gp_cpl_bd15()          /* 0xBABD8 */
#define GF_SI_BD40               gp_cpl_bd40()          /* 0xBAB50 → 0xEADA6: BD14+0x2C */
#define GF_EB7BC(c7, t)          gp_cpl_eb7bc((c7), (t))
#define GF_DDA54(base)           gp_cpl_dda54(base)
#define GF_EB1F0(in)             gp_cpl_eb1f0((in)->e69b, s_gf.e344)
#define GF_UNP_EB1F0()           ((void)0)              /* ported in the coupled instance */
#define GF_UNP_AFC()             ((void)0)              /* LAB_104AFC: reachable and ported */
#define GF_DDE78(g0, sk, ps)     gp_cpl_dde78((g0), (sk), (ps))
#define GF_DDDE8(o, n, sk, ps)   gp_cpl_ddde8((o), (n), (sk), (ps), s_gf.e69c[1])
#endif

/* the fault-reaction inputs: compile-time 0 (healthy) on SH; host seam */
#if defined(__sh__)
#define GF_AE2D  0u         /* 0xFFF8AE2D = (0x89C43 != 0) */
#define GF_C43   0          /* 0xFFF89C43 */
#else
static uint8_t s_ae2d;
static int8_t  s_c43;
#define GF_AE2D  s_ae2d
#define GF_C43   s_c43
void sm_gear_host_si(uint8_t ae2d, int8_t c43)
{
    s_ae2d = ae2d;
    s_c43 = c43;
}
#endif

/* ── the gear-pair tables ──────────────────────────────────────────────── */
/* t[(to+1) + (from+1)*10] (0xD4F86 / 0xD4FA0: both +1 masked to a byte) */
static int8_t gtab(const int8_t *t, int8_t from, int8_t to)
{
    const unsigned c = (unsigned)(uint8_t)(to + 1), r = (unsigned)(uint8_t)(from + 1);
    if (c >= 10u || r >= 10u) {
        s_gf.err_n++;
        return 0;
    }
    return t[c + r * 10u];
}
#define B7C02(f, t) gtab(sm_cal_gtype, (f), (t))     /* E5C0 → 0xD4F86 */
#define B7C14(f, t) gtab(sm_cal_gkind, (f), (t))     /* E5C0 → 0xD4FA0 */

/* 0xB6D14 / 0xB6CE8 (s16) and 0xB6CBC (u8): {u32 nx, u32 ny, data[y*nx + x]} */
static int16_t tspd(unsigned k, uint8_t x, uint8_t y)
{
    if (x < SM_CAL_GN && y < SM_CAL_GN) {
        return sm_cal_gspd[k][y * SM_CAL_GN + x];
    }
    return 0;
}
static uint8_t ttemp(uint8_t x, uint8_t y)
{
    if (x < SM_CAL_GN && y < SM_CAL_GN) {
        return sm_cal_gtemp[y * SM_CAL_GN + x];
    }
    return 0u;
}

/* the 0x89C04 block's triplets: g, g + 1, max(g, 0) */
static void trip(int8_t *c, int8_t g)
{
    c[0] = g;
    c[1] = (int8_t)(g + 1);
    c[2] = (g < 0) ? 0 : g;
}

/* ── 0xD1922 (from 0xD1B10): raise a low target while the output speed
 *    already exceeds the next gear's limit (tab [4]) ──────────────────── */
static int8_t d1922(int8_t cur, int8_t t, const gf_in_t *in)
{
    bool more;
    do {
        more = false;
        if (t < cur) {
            const uint16_t lim = (uint16_t)tspd(4u, (uint8_t)(t + 2), (uint8_t)(t + 1));
            if (lim <= in->cb8) {
                more = true;
                t = (int8_t)(t + 1);
                s_gf.lim_n++;
            }
        }
    } while (more && t < cur);
    return t;
}

/* ── 0xD1B10: power flow 0 ─────────────────────────────────────────────── */
static int8_t d1b10(int8_t t, const gf_in_t *in)
{
    s_gf.d1b10_n++;
    /* 0xBB0C2(E61C) = 0xDBCA8: 1 unless 0x89C43 != 0 and (0x6344E[0x89C42] != 1
     * or 0x89CED != 0 or 0xFFF8C310 != 0) — with the fault-free 0x89C43 = 0: 1;
     * the host's dump case (0x89C43 3, code 0x20) is decided by the result
     * only through 0xD1922, which leaves t at cur == t either way */
    const bool bb0c2 = (GF_C43 == 0);
    if (bb0c2 && GF_SI_AE2B == 0u) {
        t = d1922(s_gf.e69f, t, in);
    }
    return t;
}

/* ── 0xD16BA: a downshift whose lower gear the speeds forbid is replaced
 *    from 0x4F708 (loop while the type stays 2) ──────────────────────── */
static int8_t d16ba(int8_t cur, int8_t t, const gf_in_t *in)
{
    const uint8_t x = (uint8_t)(cur + 1);
    bool again;
    int8_t c7;
    do {
        again = false;
        c7 = B7C02(cur, t);
        if (c7 == 0) {
            t = cur;
        } else if (c7 == 2) {
            const int8_t c8 = B7C14(cur, t);
            const uint8_t y = (uint8_t)(t + 1);
            bool hot = false;
            if (c8 == 4 && in->cd2 < ttemp(x, y)) {
                hot = true;
            }
            if ((uint16_t)tspd(2u, x, y) <= in->cba || (uint16_t)tspd(3u, x, y) <= in->cd0 ||
                (uint16_t)tspd(4u, x, y) <= in->cb8 || hot) {
                t = sm_cal_gskip[(unsigned)y * 10u + x];
                again = true;
                s_gf.lim_n++;
                if (t < -1 || t > 8 || t == 0) {
                    s_gf.err_n++;                    /* the stock reports 0x97 and subtracts 100 */
                    t = (int8_t)(t - 100);
                }
            }
        }
    } while (again && c7 == 2);
    return t;
}

/* ── 0xD1854: an upshift below its minimum output speed (tab [0]) with
 *    0x89CA0 >= tab [1] is replaced from 0x4F708 ─────────────────────── */
static int8_t d1854(int8_t cur, int8_t t, const gf_in_t *in)
{
    const uint8_t x = (uint8_t)(cur + 1);
    bool again;
    int8_t c;
    do {
        again = false;
        c = B7C02(cur, t);
        if (c == 0) {
            t = cur;
        } else {
            const uint8_t y = (uint8_t)(t + 1);
            if (in->cb8 < (uint16_t)tspd(0u, x, y) && tspd(1u, x, y) <= (int16_t)GF_SI_9CA0) {
                t = sm_cal_gskip[(unsigned)y * 10u + x];
                again = true;
                s_gf.lim_n++;
                if (t < -1 || t > 8 || t == 0) {
                    s_gf.err_n++;
                    t = (int8_t)(t - 100);
                }
            }
        }
    } while (again && c != 0);
    return t;
}

/* ── 0xD1990: the next commanded gear in D ─────────────────────────────── */
static int8_t d1990(int8_t t, const gf_in_t *in)
{
    gf_t *g = &s_gf;
    const int8_t cur = g->e69f;
    g->d1990_n++;
    if (t < cur) {
        /* 0xBABAA (the shift object's query, only an argument) → 0xAEDDC =
         * 0xB0294: its gate needs cal 0x1FE672 ∈ {2, 3} — the cal reads 0:
         * the target passes unchanged [O] */
    }
    int8_t l = gtab(sm_cal_gnext, cur, t);
    if (g->e69c[1] == 2u && B7C14(cur, l) == 4 && g->e343 == (int8_t)(cur - 1)) {
        l = g->e343;                                 /* 0xBA848 = E33C+7 */
    }
    if (g->e69c[1] == 1u && B7C14(cur, l) == 2 && g->e343 == (int8_t)(cur + 1)) {
        l = g->e343;
    }
    if (B7C14(cur, l) == 4) {
        g->unp_n++;                                  /* 0xD0496(AE30, cur, l): stand-in 0 */
    }
    if (B7C14(cur, l) == 2) {
        g->unp_n++;
    }
    if (cur != l) {
        /* 0xBB0C2 / 0xBB0D0 (E61C → 0xDBCA8 / 0xDBCF8): 1 with 0x89C43 = 0 */
        const bool bb0c2 = (GF_C43 == 0), bb0d0 = (GF_C43 == 0);
        if (bb0c2) {
            l = d16ba(cur, l, in);
        }
        if (bb0d0) {
            l = d1854(cur, l, in);
        }
        if (GF_AE2D == 0u && GF_SI_9B36 == 2u && B7C02(cur, l) == 1) {
            l = cur;                                 /* 0xAEA44: 0x89B37 = 1 */
        }
    }
    return l;
}

/* ── 0x10485E: the target E4A4[0] ──────────────────────────────────────── */
static void r485e(const volatile sm_t *sm)
{
    gf_t *g = &s_gf;
    int8_t v = (int8_t)sm->ce8;                      /* *0x89CE8 */
    if (GF_SI_E298 != 0u && GF_SI_E298 != 1u && GF_SI_E298 != 11u && GF_SI_E298 != 12u) {
        g->unp_n++;                                  /* 0xD11D0 → E29D: unreachable (E298 0) */
    } else if (GF_SI_AE2C == 1u) {
        g->unp_n++;                                  /* 0xB8396(E5C4) */
    } else if (GF_SI_AE70 == 1u || GF_SI_AE70 == 2u) {
        g->unp_n++;                                  /* AE70[7] */
    } else if (GF_AE2D != 1u) {
        if (g->e4a5 == 1u) {
            v = g->cce4[0];
        }
    } else {
        v = (int8_t)GF_C43;
    }
    g->e4a4 = v;
}

/* ── 0xD15A6: the gear target E6A1 ─────────────────────────────────────── */
static void d15a6(const gf_in_t *in)
{
    gf_t *g = &s_gf;
    int8_t l = g->e4a4;
    if (in->c3d != 0u && in->e69b == 1u && GF_SI_AE2C == 0u) {
        g->e6a1 = g->e69f;                           /* the mode-1 return (0xD15AA..0xD15D0) */
        g->ret1_n++;
        return;
    }
    if (l < 1 || l > 8) {
        l = g->e69f;
        if (l == -1) {
            l = 1;
        }
        if (l == 0) {
            l = 1;
        }
    }
    if (in->e6ef == 2u || in->e69b == 2u) {
        l = -1;
    } else if (GF_SI_AE2C == 0u) {
        if (in->e69b == 0u) {
            l = d1b10(l, in);
        } else if (GF_SI_AE2B == 0u) {
            l = d1990(l, in);
        }
    }
    /* E69C[0] == 0 && AE56 != 0 && E6A1 != l → 0xD05A0(AE30): AE56 = 0 */
    g->e6a1 = l;
}

/* 0xD0D1C: E28E ∈ {1, 2, 4, 5, 7, 8, 9, 12, 13} */
static bool d0d1c(uint8_t s)
{
    return s == 1u || s == 2u || s == 4u || s == 5u || s == 7u || s == 8u || s == 9u ||
           s == 12u || s == 13u;
}

/* 0xD0232: g2 strictly between g1 and g3 (none −1) */
static bool d0232(int8_t a, int8_t b, int8_t c)
{
    if (a == -1 || b == -1 || c == -1) {
        return false;
    }
    if ((b <= a || b <= c) && (a <= b || c <= b)) {
        return false;
    }
    return true;
}

/* E4A7 and the 0x89C28 triplet (0x104A28's stores), 0xBA1F4 not run */
static void set_e4a7(int8_t v)
{
    if (s_gf.e4a7 != v) {
        s_gf.e4a7 = v;
        trip(s_gf.c28, v);
        s_gf.e4a7_chg_n++;
    }
}

/* ── 0x104A28: the commanded gear E4A7 ─────────────────────────────────── */
static void r4a28(const gf_in_t *in)
{
    gf_t *g = &s_gf;
    const bool pend = in->e6ee != in->e69b;          /* 0xCFE62(E69B) */
    g->e4a6 = 0u;
    if (pend || (d0d1c(in->e28e) && GF_SI_AE2C == 0u)) {
        set_e4a7(g->e69f);                           /* LAB_104B86 */
        return;
    }
    if (g->e4a7 == g->e6a1) {
        return;
    }
    if (GF_SI_AE2C != 0u || GF_SI_AE2B != 0u || g->e6a1 == -1) {
        set_e4a7(g->e6a1);
        return;
    }
    /* 0xBAA96 → 0xDD940 → 0xEB7BC(BD14, E33C+7, E6A1): BD14+1 == 0 → 1 */
    const bool baa96 = GF_EB7BC(g->e343, g->e6a1);
    if (baa96) {
        /* 0xD0304(AE30, CCE4[0], E6A1): the AE30 sequencer object, not run */
        if (in->e69b != 1u || GF_AE2D != 0u) {
            set_e4a7(g->e6a1);
            return;
        }
        if (GF_SI_AE56 == 0u) {
            set_e4a7(g->e6a1);
            return;
        }
    }
    /* LAB_104AFC (unreachable with the idle shift object; ported) */
    GF_UNP_AFC();
    if (!d0232(g->cce4[0], g->e69f, g->e6a1)) {
        return;
    }
    if (GF_SI_BD40 == 0u && g->e4a5 != 1u) {          /* 0xBAB50 → 0xEADA6 */
        return;
    }
    set_e4a7(g->cce4[0]);
    g->e4a6 = 1u;
}

/* ── 0x103CEC: the strobe CCE4[3] ──────────────────────────────────────── */
static void r3cec(const gf_in_t *in)
{
    gf_t *g = &s_gf;
    const bool pend = in->e6ee != in->e69b;
    if (g->e4a7 == g->e69f || pend) {
        return;
    }
    if (g->e69c[0] != 0u) {
        if (d0232(g->cce4[0], g->e69f, g->e4a7) && g->e4a6 != 0u) {
            g->cce4[1] = 1;
        }
        if (g->cce4[1] == 0) {
            /* 0xBA84E → 0xDDA54(E33C, E4A7): E33C+7 == g and 0x89C2F == 0x89C30 == 1
             * (BD14+1 == 0: the notification branch skipped) */
            g->cce4[2] = (int8_t)GF_DDA54((g->e343 == g->e4a7 && in->e6ee == in->e69b && in->e69b == 1u) ? 1 : 0);
        }
    }
    if (GF_SI_AE2C == 0u && GF_SI_AE2B == 0u && g->cce4[1] == 0 &&
        (g->e4a7 != -1 || g->e69f == -1) && (g->e4a7 == -1 || g->e69f != -1)) {
        if (GF_SI_BD15 != 0u && g->cce4[2] == 0) {   /* 0xBABD8 */
            return;
        }
        /* 0xBA202 → 0xDD29E(E32A, E4A7): g != 0, 0xAEBC8 (0x89BC0 == 0), AFD4 == 0 */
        const bool dd29e = g->e4a7 != 0 && GF_SI_9BC0 == 0u && GF_SI_AFD4 == 0u;
        if (!dd29e) {
            return;
        }
    }
    g->cce4[3] = 1;
    g->strobe_n++;
}

/* 0x103C2A: the completion test */
static bool r3c2a(const gf_in_t *in)
{
    if (GF_EB1F0(in)) {                              /* 0xBAB5E → 0xEB1F0: BD14+2 == 0 → 1 */
        return true;
    }
    GF_UNP_EB1F0();
    const bool pend = in->e6ee != in->e69b;
    if (!pend && in->e69b == 1u && GF_SI_AE2B == 0u && s_gf.ae2e != 1u && !d0d1c(in->e28e) &&
        GF_SI_AE71 != 2u && s_gf.e4a5 != 1u) {
        return false;
    }
    return true;
}

/* ── 0x103B36(CCE4, p2) ────────────────────────────────────────────────── */
static void r3b36(const gf_in_t *in, bool p2)
{
    gf_t *g = &s_gf;
    g->cce4[1] = 0;                                  /* 0x103C20 */
    g->cce4[2] = 0;
    g->cce4[3] = 0;
    if (p2) {
        r3cec(in);
    }
    if (g->e69c[0] == 1u) {
        const bool pend = in->e6ee != in->e69b;
        if ((in->e6ef != in->e69b || pend) && in->e6ef == 0u) {
            g->ae2e = 1u;
            g->ae2f = g->cce4[0];
        }
    }
    if (g->e69c[0] != 0u && (g->cce4[3] != 0 || r3c2a(in))) {
        g->cce4[0] = g->e69f;
        trip(g->c25, g->e69f);
        g->e69c[0] = 0u;
        g->e69c[1] = 0u;
        g->e69c[2] = 0u;
        g->done_n++;
        /* 0xBAAE4(E61A, CCE4[0], CCE4[1] && CCE4[3], CCE4[2] && CCE4[3]) →
         * 0xDDE78 → 0xEBC60: the gear-pair END (the coupled instance only) */
        GF_DDE78(g->cce4[0], (uint8_t)((g->cce4[1] != 0 && g->cce4[3] != 0) ? 1u : 0u),
                 (uint8_t)((g->cce4[2] != 0 && g->cce4[3] != 0) ? 1u : 0u));
        /* 0xBB0E6 / 0xAEE62 / 0xAEA54 / 0xD0574: notifications, not run */
        if (g->e4a5 == 2u) {
            g->e4a5 = 0u;                            /* 0x104986(E4A4) */
        }
    }
}

/* ── 0xD140C(E69C): on the strobe, E69F = E4A7 ─────────────────────────── */
static bool d140c(void)
{
    gf_t *g = &s_gf;
    if (g->cce4[3] == 0) {
        return false;
    }
    const int8_t old = g->e69f;
    g->e69f = g->e4a7;
    trip(g->c2b, g->e69f);
    g->c2b[3] = (int8_t)~g->e69f;
    g->e6a0 = (uint8_t)~(uint8_t)g->e69f;
    g->e69c[0] = 1u;
    g->e69c[1] = (uint8_t)B7C02(old, g->e69f);
    g->e69c[2] = (uint8_t)B7C14(old, g->e69f);
    if (old != g->cce4[0]) {
        g->unp_n++;                                  /* 0xB79E4 event 0x9A: counted, not raised */
    }
    /* 0xD052E, 0xBB0DE: not run; 0xBAAC8(E61A, old, new, CCE4[1] && CCE4[3],
     * CCE4[2] && CCE4[3]) → 0xDDDE8 → 0xEBA6E: the gear-pair start (the
     * coupled instance only; pool 0xD1564, args 0xD14C0..0xD14EC); 0xB7B5C,
     * 0xB83CA, 0xAEE6A / 0xAEA4C: not run */
    GF_DDDE8(old, g->e69f, (uint8_t)((g->cce4[1] != 0 && g->cce4[3] != 0) ? 1u : 0u),
             (uint8_t)((g->cce4[2] != 0 && g->cce4[3] != 0) ? 1u : 0u));
    return true;
}

#if SMG_COUPLED == 0
/* ── 0xDE190: the coordinator's gear ───────────────────────────────────── */
static int8_t de190(uint8_t pf, int8_t c25, int8_t c2b, int8_t c28, int8_t cur7, int8_t prev)
{
    gf_t *g = &s_gf;
    const int8_t k = B7C14(c25, c2b);
    if (c2b != prev) {
        g->x51a0 = 0u;
    }
    if (k == 0 || pf != 1u || c2b != prev) {
        return c2b;
    }
    if (c2b == cur7) {
        if (g->x51a0 != 0u) {
            return cur7;
        }
        g->x51a0 = 1u;
        g->unp_n++;                                  /* the early next gear (0xBB044, 0xDE38C, 0xDE3A0): not ported */
        return c2b;
    }
    if (cur7 == c28) {
        return cur7;
    }
    g->unp_n++;                                      /* 0xDE3A4 / 0xBB044: not ported */
    return c2b;
}

/* ── 0xDD954: the gear-pair coordinator (its gear only) ────────────────── */
static void dd954(const gf_in_t *in)
{
    gf_t *g = &s_gf;
    const int8_t c3 = de190(in->e69b, g->c25[0], g->c2b[0], g->c28[0], g->e343, g->x519f);
    if (c3 != g->e343) {
        g->e343 = c3;
        g->gp_n++;                                   /* 0xDDBB4 + 0xEB9EC: the gear-pair start, not ported */
    }
    g->x519f = g->c2b[0];
    g->e344 = (uint8_t)((g->e343 != g->c2b[0] && g->e343 == g->c28[0]) ? 1u : 0u);
}
#else /* SMG_COUPLED */
/* ── 0xDE190 (bytes 0xDE190..0xDE2AA): the coordinator's gear, WITH the
 *    early next gear of the coupled instance. Args as 0xDD954 passes them:
 *    (pf 0x89C30, c25 0x89C25, c2b 0x89C2B, c28 0x89C28, cur7 E33C+7,
 *    prev 0x8519F) ─────────────────────────────────────────────────────── */
static int8_t de190(uint8_t pf, int8_t c25, int8_t c2b, int8_t c28, int8_t cur7, int8_t prev,
                    const gf_in_t *in)
{
    gf_t *g = &s_gf;
    const int8_t k = B7C14(c25, c2b);                /* 0xDE1A8: 0xB7C14(E5C0, c25, c2b) */
    if (c2b != prev) {
        g->x51a0 = 0u;                               /* 0xDE1BC: 0x851A0 = 0 */
    }
    if (k == 0 || pf != 1u || c2b != prev) {
        return c2b;                                  /* 0xDE2A4 */
    }
    if (c2b == cur7) {
        int8_t nx = cur7;
        if (g->x51a0 != 0u) {
            return cur7;                             /* 0xDE1D8 -> 0xDE2A0 */
        }
        g->x51a0 = 1u;                               /* 0xDE1E2 */
        if (c2b < c25) {
            nx = (int8_t)(c2b - 1);                  /* 0xDE1E4: continue down */
        } else if (c25 < c2b) {
            nx = (int8_t)(c2b + 1);                  /* 0xDE1EE: continue up */
        }
        if (nx < 1 || nx > 8) {
            return c2b;                              /* 0xDE1F6..0xDE1FE -> 0xDE29E */
        }
        /* 0xBB044(E61C) -> 0xDB9FA: 0x89C43 != 0 (a substitute gear) */
        if (GF_C43 != 0) {
            return c2b;
        }
        /* 0xDE210: 0xEB5A6(BD14, c25, c2b, next) — the chained-shift
         * permission (0xEC956's cal / speed / mode gates, undecoded): the
         * gear-pair shadow's stand-in, counted there.
         * 0xDE21E..0xDE264: the engine (0x89CD0) and turbine (0x89CBA)
         * speeds the stock hands 0xB6BBC, scaled by ratio(c2b) / ratio(c25)
         * (0xD7C8C, divs, 0xB785C) */
        const uint32_t r25 = (uint32_t)gp_cpl_ratio(c25);
        const uint32_t r2b = (uint32_t)gp_cpl_ratio(c2b);
        uint32_t ne = 0u, nt = 0u;
        if (r25 != 0u) {
            ne = ((uint32_t)in->cd0 * r2b) / r25;
            nt = ((uint32_t)in->cba * r2b) / r25;
        }
        if (ne > 0xFFFFu) {
            ne = 0xFFFFu;                            /* 0xB785C */
        }
        if (nt > 0xFFFFu) {
            nt = 0xFFFFu;
        }
        /* 0xB6BBC(E5BD, c2b, next, ne, nt) -> 0xD165A (0xD16BA / 0xD1854 on
         * those speeds, 0xD17CC): == next -> next (stand-in in the shadow) */
        if (gp_cpl_early_ok(c25, c2b, nx, (uint16_t)ne, (uint16_t)nt)) {
            return nx;
        }
        return c2b;
    }
    if (cur7 == c28) {
        return cur7;                                 /* 0xDE280..0xDE284 */
    }
    /* 0xDE286: 0xEB73E(BD14) == 0 && 0xBB044 == 0 -> hold cur7; 0xEB73E is
     * stood in by the gear-pair shadow (counted) */
    if (gp_cpl_eb73e() == 0u && GF_C43 == 0) {
        return cur7;
    }
    return c2b;
}

/* ── 0xDD954 (bytes 0xDD954..0xDDA52): the gear-pair coordinator ──────── */
static void dd954(const gf_in_t *in)
{
    gf_t *g = &s_gf;
    const int8_t c3 = de190(in->e69b, g->c25[0], g->c2b[0], g->c28[0], g->e343, g->x519f, in);
    if (c3 != g->e343) {
        g->gp_n++;
        /* the +0x14 pre-select-pending block, E33C+7 = c3, then 0xDDBB4(E33C,
         * c2b, c3, 1) and 0xEB9EC(BD14, c2b, c3, +5, +6) — on the shadow's
         * own E33C copy */
        g->e343 = c3;
        gp_cpl_dd954_change(c3, g->c2b[0], g->c25[0], g->x519f);
    }
    g->x519f = g->c2b[0];                            /* 0xDDA3E: 0x8519F = 0x89C2B */
    g->e344 = (uint8_t)((g->e343 != g->c2b[0] && g->e343 == g->c28[0]) ? 1u : 0u);
}
#endif /* SMG_COUPLED */

static void witness(const gf_in_t *in)
{
    gf_t *g = &s_gf;
    if (g->e4a7 == in->live_e4a7 && g->e69f == in->live_e69f) {
        g->agree_n++;
    } else {
        g->dis_n++;
    }
}

/* ── the 10 ms pass: slots 30..33 ──────────────────────────────────────── */
void sm_gear_pass(const gf_in_t *in)
{
    gf_t *g = &s_gf;
    g->in = *in;
    if (in->os_mode < 1u || in->os_mode > 3u) {
        g->skip_mode_n++;
        return;
    }
    g->pass_n++;
    const int8_t e4a7_0 = g->e4a7, e69f_0 = g->e69f;
    /* slot 30: 0xCFFDE → 0x1048C8 (not ported), 0x10485E, 0xD15A6 */
    r485e(sm_shadow_state());
    d15a6(in);
    /* slot 31: 0xD01C4 → 0x104A28 */
    r4a28(in);
    /* slot 32: 0xD01E6 → 0x103B36(CCE4, 1); 0xD140C; 0x103B36(CCE4, 0) */
    r3b36(in, true);
    if (d140c()) {
        r3b36(in, false);
    }
    /* slot 33: 0xDD954 */
    dd954(in);
    witness(in);
    if (g->e4a7 != e4a7_0 || g->e69f != e69f_0 || g->pass_n == 1u) {
        gf_tr_t *t = &g->tr[g->tr_head % GF_TR_N];
        t->pass = (uint16_t)g->pass_n;
        t->e4a4 = g->e4a4;
        t->e6a1 = g->e6a1;
        t->e4a7 = g->e4a7;
        t->e69f = g->e69f;
        t->live_e4a7 = in->live_e4a7;
        t->e69b = in->e69b;
        g->tr_head = (uint8_t)((g->tr_head + 1u) % GF_TR_N);
    }
}

/* the stock's init (0xCFFDE(1)'s init calls): 0x104A0E E4A7 = 6 (0x89C28..2A
 * = 6 7 6), 0xD13C6 E69F = E4A7 (0x89C2B..2E, E69C[0..2] = 0), 0x103B0E
 * CCE4[0] = E69F (0x89C25..27); 0x8519F / 0x851A0 / E33C+7 start 0 (DTBL 0
 * / NOLOAD RAM) */
void sm_gear_init(void)
{
    uint8_t *p = (uint8_t *)&s_gf;
    for (size_t i = 0u; i < sizeof s_gf; i++) {
        p[i] = 0u;
    }
    s_gf.e4a7 = 6;
    trip(s_gf.c28, 6);
    s_gf.e69f = 6;
    trip(s_gf.c2b, 6);
    s_gf.c2b[3] = (int8_t)~6;
    s_gf.e6a0 = (uint8_t)~6u;
    s_gf.cce4[0] = 6;
    trip(s_gf.c25, 6);
    s_gf.init_n = 1u;
}

const volatile gf_t *sm_gear_state(void)
{
    return &s_gf;
}
