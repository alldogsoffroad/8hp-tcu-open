/* sm_shadow.c — the READ-ONLY shift-target shadow (app/sm_shadow.h).
 * ~/tcu_sweep/shift_map_decode.md; port ~/tcu_sweep/sm_shadow_port.md.
 * Every function names the stock routine it ports; the arithmetic is the
 * bytes' (u16 loads, 32-bit mulr, divu / divs as the stock picks them).
 */
#define TCU_SM_SHADOW_TU 1
#include "app/sm_shadow.h"
#include "app/sm_cal.h"

#include <stdbool.h>

#if defined(__sh__)
#define SMSEC __attribute__((section(".sm_shadow")))
#else
#define SMSEC
#endif

static sm_t s_sm SMSEC;

/* the stand-ins match the dump (stockram_driving_20261104.bin) */
_Static_assert(SM_SI_LVL_LO == 1u && SM_SI_LVL_HI == 1u && SM_SI_LVL_W == 0u,
               "dump A2C4..A2C6 = 01 01 00: b 1, c 1, d 0");
_Static_assert(SM_SI_BASE_A == 0u && SM_SI_BASE_B == 1u && SM_SI_BASE_W == 0u,
               "dump A1F4..A1F7 = 01 00 00 00");
_Static_assert(SM_SI_A706 == 1u, "dump A706 = 01");
_Static_assert(SM_SI_AE == 1u && SM_SI_AF == 102u && SM_SI_B0 == 249u, "dump S+0xAE..B0 = 01 66 F9");
_Static_assert(SM_SI_B2 == 40, "dump S+0xB2 = 0028");
_Static_assert(SM_SI_LIM_HI == 10000u && SM_SI_LIM_LO == 0u, "dump S+0xE6..EC = 2710 0000 2710 0000");
_Static_assert(SM_SI_WIN_HI == 8u && SM_SI_WIN_LO == 1u, "dump A67C+0x2B / +0x2D = 08 / 01");
_Static_assert(SM_SI_AB34 == 0u && SM_SI_S3F == 6, "dump AB34 = 00, 85157 = 06");
_Static_assert(SM_SI_PEDAL == 23u && SM_SI_GEAR == 3, "dump A786 = 0x17, 0x89C28 = 03");

#define SM_SI_A67C4 0u   /* A67C+4 (0xFC112 clears it; 0xFCA3A not ported) */
#define SM_ERR_UNPORTED 0xEEu   /* ours: a remapped id whose map case is not ported */

/* ── table getters ─────────────────────────────────────────────────────── */
/* 0x121432: {u32 n, u8 t[n]}; out of range → (error 77) index 0 */
static uint8_t remap(uint8_t k)
{
    return cal_u8(CAL_SM_REMAP + (k < 100u ? k : 0u));
}

/* 0x1214B4: {u32 n1, u32 n2, u8 t[c + r*n1]}; out of range → (error 77) 0, 0 */
static uint8_t tab2(const uint8_t *t, unsigned n1, unsigned n2, unsigned c, unsigned r)
{
    if (c >= n1 || r >= n2) {
        c = 0u;
        r = 0u;
    }
    return t[c + r * n1];
}

/* 0x121CFA: saturate to 0..0xFFFF */
static uint16_t sat16(int32_t v)
{
    return (uint16_t)(v > 0xFFFF ? 0xFFFF : (v < 0 ? 0 : v));
}

/* ── 0x121A56: the bilinear map lookup (integer, clamped) ───────────────── */
static uint16_t lut(uint32_t map, uint16_t x, uint16_t y)
{
    /* the stock map object in place: {u32 nx, u32 ny, u16 xa[nx], u16 ya[ny], u16 z[ny][nx]} */
    const unsigned nx = cal_u32(map), ny = cal_u32(map + 4u);
#define M_XA(i) cal_u16(map + 8u + 2u * (i))
#define M_YA(i) cal_u16(map + 8u + 2u * nx + 2u * (i))
#define M_Z(i)  cal_u16(map + 8u + 2u * (nx + ny) + 2u * (i))
    unsigned ix, iy;
    uint32_t dx, wx, dy, wy;
    if (!(x > M_XA(0))) {
        dx = 0u; wx = 0u; ix = 0u;
    } else if (x >= M_XA(nx - 1u)) {
        dx = 1u; wx = 1u; ix = nx - 2u;
    } else {
        unsigned k = 0u;
        while (x > M_XA(k)) {
            k++;
        }
        ix = k - 1u;
        dx = (uint32_t)(x - M_XA(k - 1u));
        wx = (uint32_t)(M_XA(k) - M_XA(k - 1u));
    }
    if (!(y > M_YA(0))) {
        dy = 0u; iy = 0u;
        wy = (uint32_t)(M_YA(1) - M_YA(0));
    } else if (y >= M_YA(ny - 1u)) {
        dy = 1u; wy = 1u; iy = ny - 2u;
    } else {
        unsigned k = 0u;
        while (y > M_YA(k)) {
            k++;
        }
        iy = k - 1u;
        dy = (uint32_t)(y - M_YA(k - 1u));
        wy = (uint32_t)(M_YA(k) - M_YA(k - 1u));
    }
    const unsigned o0 = iy * nx + ix, o1 = o0 + nx;
    uint32_t a, b;
    if (dx == 0u) {
        a = M_Z(o0);
        b = M_Z(o1);
    } else {
        const uint32_t a1 = M_Z(o0), a2 = M_Z(o0 + 1u), b1 = M_Z(o1), b2 = M_Z(o1 + 1u);
        a = (a2 >= a1) ? a1 + (a2 - a1) * dx / wx : a1 - (a1 - a2) * dx / wx;
        b = (b2 >= b1) ? b1 + (b2 - b1) * dx / wx : b1 - (b1 - b2) * dx / wx;
        a &= 0xFFFFu;
        b &= 0xFFFFu;
    }
    if (dy == 0u) {
        return (uint16_t)a;
    }
    if (b >= a) {
        return (uint16_t)(a + (b - a) * dy / wy);
    }
    return (uint16_t)(a - (uint16_t)((a - b) * dy / wy));
}
#undef M_XA
#undef M_YA
#undef M_Z

uint16_t sm_shadow_lut(uint8_t map_idx, uint16_t x, uint16_t y)
{
    return lut(SM_CAL_MAP_ADDR(map_idx < SM_CAL_NMAP ? map_idx : 0u), x, y);
}

/* ── 0x1290A8: id → map (cases 0/5/10/15/20 = @0/@4/@8/@12/@16 of 0x62278),
 *    writes S+0xE4 = line, S+0xE3 = y, then 0x121A56 ─────────────────── */
static unsigned lookup(sm_t *s, uint16_t *out, uint8_t id, uint8_t line, uint8_t y)
{
    s->s_e4 = line;
    s->s_e3 = y;
    if (id > 99u || id == 33u) {
        return 56u;
    }
    if (id % 5u != 0u || id > 20u) {
        return SM_ERR_UNPORTED;                 /* a case this shadow does not carry */
    }
    s->lookup_n++;
    *out = lut(SM_CAL_MAP_ADDR(id / 5u), line, y);
    return 0u;
}

/* ── 0x128D00: the lines of gear g (up g, down g+6, gear below g-1) ────── */
static unsigned lines(uint8_t g, int8_t *up, int8_t *dn, int8_t *below)
{
    *up = -1;
    *dn = -1;
    *below = -1;
    if (g < 1u || g > 8u) {
        return 57u;
    }
    if (g <= 7u) {
        *up = (int8_t)g;
    }
    if (g >= 2u) {
        *dn = (int8_t)(g + 6u);
        *below = (int8_t)(g - 1u);
    }
    return 0u;
}

typedef struct {
    uint8_t rp6, rp9, rp7, rp10;    /* remapped ids */
    uint8_t p6, p9, p7, p10;        /* raw ids */
    uint8_t wb, wl;                 /* base weight p11, level weight p8 */
} sm_prog_t;

/* ── 0x128D9C: the blend of four maps, two weights, at (line, y) ───────── */
static unsigned blend(sm_t *s, uint16_t *out, uint8_t y, const sm_prog_t *p, uint8_t line)
{
    uint16_t a, b, c, d;
    unsigned r = lookup(s, &a, p->rp6, line, y);
    if (r) {
        return r;
    }
    if (p->rp9 != 33u) {
        if ((r = lookup(s, &b, p->rp9, line, y)) != 0u) {
            return r;
        }
    } else {
        b = a;
    }
    if (p->rp7 != 33u) {
        if ((r = lookup(s, &c, p->rp7, line, y)) != 0u) {
            return r;
        }
    } else {
        c = a;
    }
    if (p->rp10 != 33u) {
        if ((r = lookup(s, &d, p->rp10, line, y)) != 0u) {
            return r;
        }
    } else {
        d = c;
    }
    const int32_t a2 = ((int32_t)b - (int32_t)a) * (int32_t)p->wb / 100 + (int32_t)a;
    const int32_t c2 = ((int32_t)d - (int32_t)c) * (int32_t)p->wb / 100 + (int32_t)c;
    *out = sat16((c2 - a2) * (int32_t)p->wl / 100 + a2);
    return 0u;
}

/* ── 0x1281A6: the threshold correction — ported only as its pass-through
 *    (S+0xAE == 0, or pedal < S+0xAF): the raw value ───────────────────── */
static uint16_t corr(sm_t *s, uint8_t pedal, uint16_t raw)
{
    if (s->s_ae != 0u && pedal >= s->s_af) {
        s->corr_n++;                            /* unported: kept raw */
    }
    return raw;
}

/* ── 0x128894: the special limits — stood in inactive (0x12EC34 / 0x12EC5A
 *    flags 0): dir_a 0 → E6/E8, else EA/EC ─────────────────────────────── */
static void special(sm_t *s, uint8_t dir_a)
{
    if (dir_a == 0u) {
        s->s_e6 = SM_SI_LIM_HI;
        s->s_e8 = SM_SI_LIM_LO;
    } else {
        s->s_ea = SM_SI_LIM_HI;
        s->s_ec = SM_SI_LIM_LO;
    }
}

/* ── 0x128E94: the threshold of one line (dir_a 1 / dir_b 2 = down, 0 / 1 =
 *    up): the blend at the pedal (→ *out, D0 / D2), at y = S+0xB0 (→ D4 /
 *    D6), the correction, the special limits ─────────────────────────── */
static unsigned thr(sm_t *s, uint16_t *out, uint8_t pedal, const sm_prog_t *p, uint8_t line,
                    uint8_t dir_a, uint8_t dir_b)
{
    uint16_t t0, t1;
    unsigned r = blend(s, &t0, pedal, p, line);
    if (r) {
        return r;
    }
    *out = t0;
    s->s_af = SM_SI_AF;                         /* 0xF9418 → 0xF9402 / 0xF93EC (stand-in) */
    s->s_b0 = SM_SI_B0;
    /* 0x128C04 → S+0xB2 not ported (dump 40); the up case copies it */
    if (dir_a == 0u) {
        s->s_b4 = s->s_b2;
    }
    if ((r = blend(s, &t1, s->s_b0, p, line)) != 0u) {
        return r;
    }
    if (dir_b == 1u) {
        s->s_d4 = t1;
    } else if (dir_b == 2u) {
        s->s_d6 = t1;
    }
    *out = corr(s, pedal, t0);
    special(s, dir_a);
    if (dir_b == 1u) {
        s->s_d0 = t0;
    } else if (dir_b == 2u) {
        s->s_d2 = t0;
    }
    return 0u;
}

/* ── 0xFC358: the step limit of a program, dir 0 down / 1 up ───────────── */
uint8_t sm_shadow_step_limit(uint8_t program, uint8_t dir, uint8_t a67c7)
{
    uint8_t lim = tab2(cal_p8(CAL_SM_STEP), 63u, 2u, (uint8_t)(program - 1u), dir);
    if (a67c7 != 0u) {
        const uint8_t c47 = tab2(cal_p8(CAL_SM_STEP), 63u, 2u, 47u, dir);
        if (lim >= c47) {
            lim = c47;
        }
    }
    /* the column-11 / AB30 < 0x1E66A4 cap: its condition (0xF8D86, A718+100 /
     * +121, 0xF82F8 / 0xF82EC / 0xF860C) is stood in false */
    return lim;
}

/* ── 0x128076: S+0xDC = speed (the A1F8+0x13E offset stood in 0) ───────── */
static void speed_cmp(sm_t *s, uint16_t speed)
{
    s->s_dc = speed;
}

/* ── 0x1277D8 / 0x1279C2: the walk ─────────────────────────────────────── */
static unsigned walk(sm_t *s, uint8_t *cand, uint8_t gear_in, uint16_t speed, uint8_t pedal,
                     uint8_t p6, uint8_t p7, uint8_t p8, uint8_t p9, uint8_t p10, uint8_t p11,
                     uint8_t arg12)
{
    const uint8_t dn_lim = sm_shadow_step_limit(s->s_ac, 0u, 0u);
    const uint8_t up_lim = sm_shadow_step_limit(s->s_ac, 1u, 0u);
    const uint8_t f88 = 0u, f104 = 0u, flag0 = 0u;   /* 0x12ECD0 / 0x12ECDE / 0xF8C5A (stand-in) */
    s->s_b6 = 0u;
    s->s_de = 0u;
    if (p11 > 100u || p8 > 100u) {
        return 111u;
    }
    speed_cmp(s, speed);
    if (p6 == 33u) {
        /* the previous pass forced 0x30: the ids come from S+0xC7..CC (not
         * modelled — S+0xB6 never sets with the limits inactive) */
        return SM_ERR_UNPORTED;
    }
    const sm_prog_t p = {
        .rp6 = remap(p6), .rp9 = remap(p9), .rp7 = remap(p7), .rp10 = remap(p10),
        .p6 = p6, .p9 = p9, .p7 = p7, .p10 = p10, .wb = p11, .wl = p8,
    };
    uint8_t g = gear_in, prev = gear_in;
    uint32_t it = 0u;
    for (;;) {
        int8_t upl, dnl, below;
        unsigned r = lines(g, &upl, &dnl, &below);
        if (r) {
            return r;
        }
        bool to_up = true;
        if (gear_in >= g && dnl != -1) {
            s->a699 = dn_lim;
            if ((r = thr(s, &s->s_da, pedal, &p, (uint8_t)dnl, 1u, 2u)) != 0u) {
                return r;
            }
            const bool down = (s->s_dc < s->s_da) || (speed < s->s_ec && g > 1u);
            if (down) {
                if (f88 == 1u && speed < s->s_ec) {
                    s->s_b6 = 1u;
                }
                if (speed > s->s_ea) {
                    s->s_de = 3u;
                    if (g == 8u) {
                        s->s_df = (uint8_t)(g - 1u);
                    }
                    s->s_b6 = 0u;
                } else {
                    g--;
                    prev = g;
                    /* 0x1297CC returns 1 */
                    if ((uint8_t)(gear_in - g) >= dn_lim) {
                        break;
                    }
                    speed_cmp(s, speed);
                    to_up = false;
                }
            }
        }
        if (to_up) {
            bool up = false, exit_now = false;
            if (!(gear_in > g) && upl != -1) {
                s->a699 = up_lim;
                if ((r = thr(s, &s->s_d8, pedal, &p, (uint8_t)upl, 0u, 1u)) != 0u) {
                    return r;
                }
                if ((r = blend(s, &s->s_ce, 0xFFu, &p, (uint8_t)upl)) != 0u) {
                    return r;
                }
                if (s->s_dc >= s->s_d8) {
                    up = true;
                } else if (g < 8u && speed > s->s_e6 && (s->s_d0 != 9999u || flag0 == 1u)) {
                    up = true;
                }
                if (up) {
                    if (f104 == 1u && speed > s->s_e6) {
                        s->s_de = 0u;
                        s->s_b6 = 1u;
                    } else if (speed < s->s_e8) {
                        s->s_de = 3u;
                        exit_now = true;
                    }
                }
            }
            if (exit_now) {
                break;
            }
            if (up) {
                g++;
                prev = g;
                /* 0x1297D0 returns 1 */
                if ((uint8_t)(g - gear_in) >= up_lim) {
                    break;
                }
            } else {
                /* 0x127CA8 */
                if (gear_in == g || prev == g) {
                    break;
                }
                prev = g;
            }
        }
        if (++it >= 8u) {
            break;
        }
    }
    if (it + 1u > s->iter_max) {
        s->iter_max = it + 1u;
    }
    if (s->s_b6 == 1u) {
        p6 = 33u;
    }
    s->s_ba[0] = p6;
    s->s_ba[1] = p9;
    s->s_ba[2] = p7;
    s->s_ba[3] = p10;
    s->s_ba[4] = p11;
    s->s_ba[5] = p8;
    s->s_c2 = gear_in;
    s->s_c3 = arg12;
    s->s_e2 = g;
    *cand = g;
    return 0u;
}

/* program 1's compute handler 0x12D420 (the level / base stand-ins) */
static unsigned prog1(sm_t *s, uint8_t *cand, uint8_t gear_in, uint16_t speed, uint8_t pedal)
{
    const uint8_t b = SM_SI_LVL_LO, c = SM_SI_LVL_HI;
    return walk(s, cand, gear_in, speed, pedal,
                (uint8_t)(5u * b + SM_SI_BASE_A),   /* p6 */
                (uint8_t)(5u * c + SM_SI_BASE_A),   /* p7 */
                SM_SI_LVL_W,                        /* p8 */
                (uint8_t)(5u * b + SM_SI_BASE_B),   /* p9 */
                (uint8_t)(5u * c + SM_SI_BASE_B),   /* p10 */
                SM_SI_BASE_W,                       /* p11 */
                SM_SI_C3);
}

int sm_shadow_walk(uint8_t gear_in, uint16_t speed, uint8_t pedal, uint8_t *cand)
{
    s_sm.s_ac = 1u;
    s_sm.s_ae = SM_SI_AE;
    s_sm.s_af = SM_SI_AF;
    return (int)prog1(&s_sm, cand, gear_in, speed, pedal);
}

/* ── THE CONFIRM STAGE (2026-11-19, ~/tcu_sweep/gear_feedback_shadow_port.md;
 *    decode m6_shiftmap_gaps_decode.md §1): 0x127D3E / 0x127F66 → 0x1284CC,
 *    a condition-gated second speed check. The condition masks S+0x8C (up)
 *    and S+0x94 (down) are HELD 0 (their producers — S+0x4C cal-disabled,
 *    S+0x54, S+0x7C, A1F8+0x16C — are not ported): 0x12E94C then sets no
 *    percentage and S+0x84 = 0, so 0x12E76E gives E0 = 0xFFFF (down) / 0 (up)
 *    and 0x12E7BC accepts every candidate — an exact pass-through until a
 *    mask bit sets. Ported as code: the full-load line lookup (S+0xE3 = 0xFF,
 *    S+0xE4 = the line), S+0xC0, S+0xE0, S+0xDF, S+0xAD / S+0x84, the step-7
 *    special-limit test (S+0x5A stood in inactive 10000). ───────────────── */
#define SM_SI_MASK_UP  0u       /* S+0x8C */
#define SM_SI_MASK_DN  0u       /* S+0x94 */
#define SM_SI_S5A      10000u   /* S+0x54 sub-object +0x5A (inactive, dump 10000) */
_Static_assert(SM_SI_MASK_UP == 0u && SM_SI_MASK_DN == 0u && SM_SI_S5A == 10000u,
               "dump S+0x8C / S+0x94 = 0, S+0x5A = 0x2710");

/* 0x1284CC(from, to): 1 = accepted */
static bool c1284cc(sm_t *s, uint8_t from, uint8_t to)
{
    if (to == from) {
        return true;
    }
    const uint8_t dir = (to < from) ? 1u : 0u;      /* 1 down */
    const sm_prog_t p = {
        .rp6 = remap(s->s_ba[0]), .rp9 = remap(s->s_ba[1]), .rp7 = remap(s->s_ba[2]),
        .rp10 = remap(s->s_ba[3]),
        .p6 = s->s_ba[0], .p9 = s->s_ba[1], .p7 = s->s_ba[2], .p10 = s->s_ba[3],
        .wb = s->s_ba[4], .wl = s->s_ba[5],
    };
    /* 0x128D00 for gear (to > 1 ? to - 1 : 1), its up line, 0x128D9C at
     * y = 0xFF into a stack word that starts at S+0xCE */
    {
        int8_t upl, dnl, below;
        uint16_t w = s->s_ce;
        if (lines((uint8_t)(to > 1u ? to - 1u : 1u), &upl, &dnl, &below) == 0u && upl != -1) {
            (void)blend(s, &w, 0xFFu, &p, (uint8_t)upl);
        }
        (void)sat16((int32_t)s->s_b4 + (int32_t)w);  /* uStack_24: the step-6 cap only (E0 0 up: never below) */
    }
    /* step 3: the forced paths need S+0xB6 (program 0x21) — 0 here */
    if (s->s_ad == 0u) {
        /* 0x12E94C with both masks 0: no condition, S+0x84 = 0; the
         * per-id percentages S+0x85..0x88 stay at their start (down 0xFF,
         * up 0 [I: the max / min starts]) */
        const uint8_t pc = dir ? 0xFFu : 0u;
        const int32_t a2 = pc, c2 = pc;                 /* 0x129064: four equal values */
        s->s_c0 = (uint8_t)((c2 - a2) * (int32_t)p.wl / 100 + a2);
        s->s_84 = (SM_SI_MASK_UP | SM_SI_MASK_DN) != 0u ? 1u : 0u;
        s->s_ad = 1u;
    }
    /* 0x12E76E */
    s->s_e0 = (s->s_84 == 0u) ? (dir ? 0xFFFFu : 0u) : 0u;
    /* step 7: 0x12EC5A again — up and S+0x5A < AB30 → DE = 3, E0 = S+0x5A */
    if (dir == 0u && SM_SI_S5A < s->ab30) {
        s->s_de = 3u;
        s->s_e0 = (uint16_t)SM_SI_S5A;
    }
    /* 0x12E7BC */
    return dir ? (s->s_e0 > s->ab30) : (s->s_e0 <= s->ab30);
}

/* 0x127D3E: a rejected downshift is shortened one gear, an upshift one gear
 * down (0x127F66), until accepted */
static uint8_t confirm(sm_t *s, uint8_t cand, uint8_t from)
{
    if (s->s_de != 3u) {
        s->s_df = s->s_c2;
    }
    if (cand < from) {
        while (from > cand) {
            if (c1284cc(s, from, cand)) {
                break;
            }
            cand++;
        }
    } else if (cand > from) {
        while (cand > from) {
            /* cand == 8: 0xF8DE2 (not ported; no effect on the result) */
            if (c1284cc(s, from, cand)) {
                break;
            }
            cand--;
        }
    }
    /* cand == from: 0x12E8FC / 0x12E756 (not ported; they do not change cand) */
    s->s_84 = 0u;
    s->s_ad = 0u;
    /* 0x1280F4 (S+0xC4..0xCC program ids, S+0xC3): not modelled */
    return cand;
}

/* ── 0xFCDFE: the window [A67C+0x2D, A67C+0x2B] ────────────────────────── */
static uint8_t window(sm_t *s, uint8_t r)
{
    const uint8_t hi = SM_SI_WIN_HI, lo = SM_SI_WIN_LO;
    bool clamp;
    if (r >= hi && hi != 8u) {
        clamp = true;
    } else {
        clamp = !(r > lo || lo == 1u);
    }
    if (!clamp) {
        s->m38 = 0u;
        return r;
    }
    s->m38 = 1u;
    s->win_n++;
    uint8_t v = (hi >= r) ? r : hi;
    if (lo > v) {
        v = lo;
    }
    return v;
}

/* ── 0xFC5DE: the limits → A697 ────────────────────────────────────────── */
static uint8_t limits(sm_t *s, uint8_t cand)
{
    const uint8_t prog = s->blk[8];             /* 0xF82EC = A3BC+0x220 */
    const uint8_t dn = sm_shadow_step_limit(prog, 0u, 0u);
    const uint8_t up = sm_shadow_step_limit(prog, 1u, 0u);
    const int d = (int)cand - (int)s->m18;
    uint8_t r = cand;
    if (d > 0) {
        if (d > (int)up) {
            r = (uint8_t)(s->m18 + up);
            s->m1d = up;
            s->clamp_n++;
        }
    } else if (d < 0) {
        if (d < -(int)dn) {
            r = (uint8_t)(s->m18 - dn);
            s->m1d = dn;
            s->clamp_n++;
        }
    }
    /* 0xFCA3A's eight sub-checks and 0xFCAAA..0xFCC44: not ported */
    r = window(s, r);
    if (r < s->m17 && SM_SI_A67C4 == 0u && r < s->m18) {
        r = (s->m17 <= s->m18) ? s->m17 : s->m18;
        s->mingear_n++;                         /* fault 0x4F */
    }
    if (s->m24 == 1u && r >= 2u) {
        r = 2u;
    }
    if (r > 8u || r < 1u) {
        r = s->m18;
        s->range_n++;                           /* fault 0x96 */
    }
    return r;
}

/* ── the pass: entries 5 (part), 14, 15, 16 ────────────────────────────── */
void sm_shadow_pass(const sm_in_t *in)
{
    sm_t *s = &s_sm;
    s->in = *in;
    if (in->os_mode < 1u || in->os_mode > 3u) {
        s->skip_mode_n++;
        return;
    }
    s->pass_n++;

    /* entry 5: 0xF9570 the AB30 filter (0x121BCC) */
    {
        uint32_t wa = cal_u8(CAL_SM_AB30W), wb = cal_u8(CAL_SM_AB30W + 1u);
        if (wa == 0u) {
            wa = 1u;
        }
        s->ab30 = (uint16_t)(((uint32_t)in->d646 * wa + (uint32_t)s->ab30 * wb) / (wa + wb));
    }
    /* 0xFC112: the minimum gear by speed (A004+0x0A, the raw speed), A694 */
    {
        uint8_t g = 1u;
        while (in->d646 > cal_u16(CAL_SM_MINGEAR + 2u * g)) {
            if (++g >= 8u) {
                break;
            }
        }
        s->m17 = g;
        s->m24 = 0u;                            /* 0xFC07E (stand-in) */
        /* A694 = A004+0x46 (← 0xA378C ← 0x89C28, the gear feedback chain's
         * commanded gear): negative → 9 (+0x22 = 1); 0 or > 8 → +0x1A = 1,
         * A694 = A697, then A694 = 0xFC5DE(A697) [O 0xFC112] */
        const uint8_t a694 = (in->gear_in < 0) ? 9u : (uint8_t)in->gear_in;
        if (a694 == 0u || a694 > 8u) {
            s->m1a = 1u;
            s->m18 = (s->a697 >= 1u && s->a697 <= 8u) ? s->a697 : 1u;   /* ours: A697 0 only before a first pass */
            s->m18 = limits(s, s->m18);
            s->gear_inv_n++;
            if (a694 == 9u && s->neg9_n < 0xFFu) {
                s->neg9_n++;
            }
        } else {
            s->m18 = a694;
            s->m1a = 0u;
        }
    }
    /* 0xF869C: A786 = min(pedal, 254) */
    s->a786 = (uint8_t)(in->pedal > 254u ? 254u : in->pedal);

    /* entry 14: 0xF83A0 → 0xF846E → 0x123990 */
    uint8_t cand = 0xFFu;
    unsigned no_cand = 1u;
    {
        uint8_t id = 0u;
        for (unsigned p = 0u; p < 18u; p++) {
            id = tab2(cal_p8(CAL_SM_PRIO), 18u, 14u, p, SM_SI_A706);
            if (id == 0x63u || id == 1u) {
                break;
            }
            s->arb_skip_n++;                    /* a higher-priority program, not ported */
        }
        s->blk[1] = 0u;
        if (id == 1u) {
            s->s_ac = 1u;
            s->s_ae = SM_SI_AE;
            s->walk_n++;
            const unsigned r = prog1(s, &cand, s->m18, s->ab30, s->a786);
            if (r == 0u) {
                no_cand = 0u;
                s->blk[1] = 1u;
            } else {
                s->err_n++;
                s->err_last = r;
            }
        } else {
            s->arb_none_n++;
        }
        s->blk[0] = id;
        s->blk[2] = s->m18;
        s->blk[3] = 0u;
        s->blk[4] = (uint8_t)(s->ab30 >> 8);
        s->blk[5] = (uint8_t)s->ab30;
        s->blk[6] = SM_SI_A706;
        s->blk[7] = s->a786;
    }

    /* the confirm stage 0x127D3E (program 1 → 0x12D4B8), then entry 15:
     * 0xF8490 — 0x123AA2's block bytes, the A694 fallback, 0xFC5DE */
    if (!no_cand) {
        cand = confirm(s, cand, s->m18);
        s->conf_n++;
        s->blk[8] = (s->s_b6 != 0u) ? 0x30u : s->blk[0];
        s->blk[9] = s->blk[8];                  /* 0xF8D5E(A1F8) = 0 (stand-in) */
    }
    if (no_cand || cand == 0xFFu) {
        cand = s->m18;
    }
    const uint8_t old = s->a697;
    s->a697 = limits(s, cand);

    /* entry 16: 0xF7864, then 0xB912C's 0x89CE8 */
    s->a0aa = (SM_SI_AB34 == 2u) ? (uint8_t)SM_SI_S3F : s->a697;
    s->ce8 = s->a0aa;

    if (s->a697 != old || s->pass_n == 1u) {
        s->chg_n++;
        sm_tr_t *t = &s->tr[s->tr_head % SM_TR_N];
        t->t_us = in->t_us;
        t->pass = (uint16_t)s->pass_n;
        t->ab30 = s->ab30;
        t->gear_in = s->m18;
        t->cand = cand;
        t->a697 = s->a697;
        t->pedal = s->a786;
        t->d8 = s->s_d8;
        t->da = s->s_da;
        s->tr_head = (uint8_t)((s->tr_head + 1u) % SM_TR_N);
    }
}

void sm_shadow_init(void)
{
    uint8_t *p = (uint8_t *)&s_sm;
    for (size_t i = 0u; i < sizeof s_sm; i++) {
        p[i] = 0u;
    }
    s_sm.s_ac = 1u;
    s_sm.s_ae = SM_SI_AE;
    s_sm.s_af = SM_SI_AF;
    s_sm.s_b0 = SM_SI_B0;
    s_sm.s_b2 = SM_SI_B2;
    s_sm.s_b4 = SM_SI_B2;
    s_sm.s_e6 = SM_SI_LIM_HI;
    s_sm.s_ea = SM_SI_LIM_HI;
    s_sm.blk[8] = 1u;
    s_sm.m18 = (uint8_t)SM_SI_GEAR;             /* 0xC2192 init: A004+0x3F ... stood in */
    s_sm.init_n = 1u;
}

const volatile sm_t *sm_shadow_state(void)
{
    return &s_sm;
}
