/* tsel_shadow.c — the READ-ONLY shadow of the stock's ECU-side temperatures
 * and their selection (app/tsel_shadow.h; ~/tcu_sweep/
 * diag_temp_inputs_decode.md, the port ~/tcu_sweep/diag_temp_shadow_port.md).
 * Every routine below is ported from the stock bytes (addresses at each);
 * every value lands in the shadow's own struct (tsel_t, section
 * .tsel_shadow) — no stock cell. Its inputs arrive by value
 * (app/tsel_hook.c).
 */
#define TCU_TSEL_SHADOW_TU
#include "app/tsel_shadow.h"

#if defined(TCU_RAM_APP)
/* the RAM application carries no shadow (its 64 KB limit: zero margin) —
 * nothing here is referenced there, and the hook file is empty too */
#else

#if defined(__sh__)
#define TSEL_RAM __attribute__((section(".tsel_shadow")))
#else
#define TSEL_RAM
#endif
static tsel_t s_tsel TSEL_RAM;

static const int16_t  CAL_OFF[2]   = { TSEL_CAL_OFF0, TSEL_CAL_OFF1 };      /* 0x191394 */
static const uint16_t CAL_SCALE[2] = { TSEL_CAL_SCALE0, TSEL_CAL_SCALE1 };  /* 0x191390 */
static const int16_t  CAL_C1LO[2]  = { TSEL_CAL_C1LO0, TSEL_CAL_C1LO1 };    /* 0x1913A0 */
static const int16_t  CAL_C1HI[2]  = { TSEL_CAL_C1HI0, TSEL_CAL_C1HI1 };    /* 0x1913A4 */
static const int16_t  CAL_C2LO[2]  = { TSEL_CAL_C2LO0, TSEL_CAL_C2LO1 };    /* 0x191398 */
static const int16_t  CAL_C2HI[2]  = { TSEL_CAL_C2HI0, TSEL_CAL_C2HI1 };    /* 0x19139C */

/* the 0x7C7E4 countdown (5 ms, every mode, the 5 ms list's first worker,
 * before this hub): the shadow's own copies, n5 steps per hub pass */
static uint16_t cd16(uint16_t v, uint8_t n)
{
    return (v > n) ? (uint16_t)(v - n) : 0u;
}

static uint8_t cd8(uint8_t v, uint8_t n)
{
    return (v > n) ? (uint8_t)(v - n) : 0u;
}

/* the byte law (0x88BD0 @0x88C94..0x88CC6, 0x88DA8 @0x88DB8..0x88DE0):
 * clamp(((t + 8) >> 4) + 40, 0, 255) — shad −4 is arithmetic */
static uint8_t tbyte(int16_t t)
{
    const int32_t v = (((int32_t)t + 8) >> 4) + 40;
    if (v >= 255) {
        return 255u;
    }
    return (v > 0) ? (uint8_t)v : 0u;
}

/* 0x7C008: the die report's flag map (0x20 → 0x20, 0x40 → 0x40, 0x02 → 0x08) */
static uint8_t map_7c008(uint16_t v)
{
    uint8_t r = 0u;
    if ((v & 0x20u) != 0u) { r = 0x20u; }
    if ((v & 0x40u) != 0u) { r = (uint8_t)(r | 0x40u); }
    if ((v & 0x02u) != 0u) { r = (uint8_t)(r | 0x08u); }
    return r;
}

/* 0x7C09C: the board report's flag map (0x20, 0x40, 0x100 → 0x80) */
static uint8_t map_7c09c(uint16_t v)
{
    uint8_t r = 0u;
    if ((v & 0x20u) != 0u) { r = 0x20u; }
    if ((v & 0x40u) != 0u) { r = (uint8_t)(r | 0x40u); }
    if ((v & 0x100u) != 0u) { r = (uint8_t)(r | 0x80u); }
    return r;
}

/* 0x7C01E(mask, code): the die fault word 0x93916 and tested / failed
 * 0x940E1 / 0x940E2 (bytes 0x7C01E..0x7C084; the 0x7CA24(2442) report is
 * not ported — counted) */
static void rep_7c01e(uint16_t mask, uint16_t code)
{
    uint8_t m, e2;
    s_tsel.rep_die_n++;
    s_tsel.w916 = (uint16_t)((s_tsel.w916 & (uint16_t)~mask) | (code & mask));
    m = map_7c008(mask);
    e2 = (uint8_t)(s_tsel.e2 & (uint8_t)~m);
    s_tsel.e1 = m;
    if ((mask & s_tsel.w916) != 0u) {
        s_tsel.e2 = (uint8_t)((map_7c008(s_tsel.w916) & m) | e2);
    } else {
        s_tsel.e2 = e2;
    }
}

/* 0x7C0B6(mask, code): tested / failed 0x940E4 / 0x940E5 (0x7CA24(2443) not
 * ported — counted) */
static void rep_7c0b6(uint16_t mask, uint16_t code)
{
    s_tsel.rep_brd_n++;
    s_tsel.e4 = map_7c09c(mask);
    s_tsel.e5 = map_7c09c((uint16_t)(code & mask));
}

/* ── 0x8DD7C: the board sensor (bytes 0x8DD7C..0x8DEC8) ───────────────── */
static void brd_8dd7c(const tsel_in_t *in)
{
    const uint16_t old = s_tsel.ac0;            /* r6 */
    const int16_t prev = s_tsel.abc;            /* r13 */
    const int16_t r = in->r908cc;               /* r7 (mov.w: sign-extended) */
    uint16_t f = (uint16_t)(old & 1u);          /* r4 */
    s_tsel.ac2 = old;
    s_tsel.abe = prev;
    s_tsel.abc = r;
    if (r == 0) {
        f = (uint16_t)(f | 0x400u);
    }
    if ((uint16_t)r == 0xFFFFu) {
        f = (uint16_t)(f | 4u | (old & 0x80u));
        if ((f & 0x80u) == 0u) {
            f = (uint16_t)(f | 0x100u);
        }
    } else if (r >= TSEL_CAL_BNEAR_LO && r <= TSEL_CAL_BNEAR_HI) {
        f = (uint16_t)(f | 0x80u);
    }
    if ((uint16_t)r == TSEL_CAL_BNOTRDY) {
        f = (uint16_t)(f | 8u);
    }
    if (((uint16_t)r & TSEL_CAL_BONES) == 0u) {
        f = (uint16_t)(f | 0x10u);
    }
    if (r > TSEL_CAL_BMAX) {
        f = (uint16_t)(f | 0x40u);
    }
    if (r < TSEL_CAL_BMIN) {
        f = (uint16_t)(f | 0x20u);
    }
    if ((f & 1u) == 0u && s_tsel.w730 == 0u && TSEL_CAL_BNEVER != 0u) {
        f = (uint16_t)(f | 2u);
    }
    if (r != prev) {
        f = (uint16_t)(f | 0x200u);
    }
    if ((f & 1u) == 0u) {
        /* never valid: the hold is reloaded every pass (0x8DE74, delay slot) */
        s_tsel.w77c = TSEL_CAL_BHOLD;
        if ((f & 0x7Cu) != 0u) {
            if ((f & 2u) != 0u) {
                s_tsel.s100 = 2u;
            }
            s_tsel.ac0 = f;
            return;
        }
        s_tsel.w730 = 0u;
        f = (uint16_t)(f | 1u);
    } else {
        if ((f & 0x170u) != 0u) {
            if (s_tsel.w77c == 0u) {
                f = (uint16_t)(f | 0x1000u);
                s_tsel.s100 = 4u;
            } else {
                s_tsel.s100 = 7u;
            }
            s_tsel.ac0 = f;
            return;
        }
        s_tsel.w77c = TSEL_CAL_BHOLD;
    }
    s_tsel.s93e = (int16_t)(r >> 3);            /* shad −3 (1/128 → 1/16 °C) */
    s_tsel.s100 = 8u;
    s_tsel.ac0 = f;
}

/* ── 0x88DA8: the board's byte + report (bytes 0x88DA8..0x88E60) ─────── */
static void brd_88da8(const tsel_in_t *in)
{
    uint16_t f, code = 0u;
    brd_8dd7c(in);
    s_tsel.s101 = tbyte(s_tsel.s93e);
    s_tsel.af6 = 0x160u;
    f = s_tsel.ac0;
    if ((f & 0x1041u) == 0x1041u) {
        code = 0x40u;
    }
    if ((f & 0x1021u) == 0x1021u) {
        code = (uint16_t)(code | 0x20u);
    }
    if ((f & 2u) != 0u || (f & 0x1101u) == 0x1101u || (f & 0x1011u) == 0x1011u) {
        code = (uint16_t)(code | 0x100u);
    }
    s_tsel.af4 = code;
    if (in->fm_ready != 0u) {                   /* 0x6F594 */
        rep_7c0b6(s_tsel.af6, s_tsel.af4);
    }
}

/* ── 0x8DB5A: the supervisor die temperature (bytes 0x8DB5A..0x8DD0C) ── */
static void die_8db5a(const tsel_in_t *in)
{
    const uint16_t prev_n = s_tsel.ab2;         /* r11 */
    const uint16_t n = in->n908ca;              /* r12 (extu.w) */
    const unsigned v = (in->v90966 == 0u) ? 0u : 1u;
    uint16_t f;
    int16_t t;
    float x;
    s_tsel.ab4 = prev_n;
    s_tsel.ab2 = n;
    s_tsel.s15c = in->f9095e;
    s_tsel.s15d = in->v90966;
    s_tsel.ab8 = s_tsel.ab6;
    f = (uint16_t)(s_tsel.ab6 & 0x11u);
    /* fr9 = off[v]; fr0 = scale[v] / 10000.0f; fr9 += fr0 · (float)N (fmac);
     * ftrc; exts.w — single precision, as the stock (fused or not gives the
     * same truncation for every N of 0..1023 and both variants [D]) */
    x = (float)CAL_OFF[v] + ((float)CAL_SCALE[v] / TSEL_CAL_DIV) * (float)n;
    t = (int16_t)(uint16_t)(uint32_t)(int32_t)x;
    if (n > TSEL_CAL_NHI) {
        f = (uint16_t)(f | 4u);
    }
    if (n < TSEL_CAL_NLO) {
        f = (uint16_t)(f | 2u);
    }
    if (s_tsel.b823 == 0u) {
        f = (uint16_t)(f | 0x40u);
    }
    if ((f & 6u) == 0u) {
        unsigned a;
        if (t < CAL_C1LO[v]) {
            f = (uint16_t)(f | (((in->f9095e & 1u) != 0u) ? 0x200u : 0x100u));
        }
        if (t > CAL_C1HI[v]) {
            f = (uint16_t)(f | (((in->f9095e & 1u) != 0u) ? 0x100u : 0x200u));
        }
        if (t < CAL_C2LO[v]) {
            f = (uint16_t)(f | (((in->f9095e & 2u) != 0u) ? 0x1000u : 0x800u));
        }
        if (t > CAL_C2HI[v]) {
            f = (uint16_t)(f | (((in->f9095e & 2u) != 0u) ? 0x800u : 0x1000u));
        }
        a = (unsigned)(f >> 8) & 3u;
        if (a == 1u && (((unsigned)f >> 11) & 3u) == 1u) {
            /* consistent with both comparators */
            f = (uint16_t)((f | 0x20u) & (uint16_t)~0x10u);
            a = (unsigned)(f >> 8) & 3u;
        }
        if (a == 2u || (((unsigned)f >> 11) & 3u) == 2u) {
            f = (uint16_t)(f | 0x30u);          /* a mismatch: 0x10 survives the per-pass clear */
        }
    }
    if (n != prev_n) {
        f = (uint16_t)(f | 8u);
    }
    if ((f & 0x16u) == 0u) {
        s_tsel.w477a = TSEL_CAL_DHOLD;
        s_tsel.aba = t;
        f = (uint16_t)(f | 1u);
        s_tsel.s15e = 8u;
    } else if ((f & 1u) == 0u) {
        if (s_tsel.w477a == 0u) {
            f = (uint16_t)(f | 0x80u);
            s_tsel.s15e = 2u;
        }                                       /* else: no status write (0x8DD08) */
    } else {
        if (s_tsel.w477a != 0u) {
            s_tsel.s15e = 7u;
        } else {
            f = (uint16_t)(f | 0x80u);
            s_tsel.s15e = 4u;
        }
    }
    s_tsel.ab6 = f;
    s_tsel.t_conv = t;
}

/* ── 0x88BD0: the die, the SELECTION, the bytes, the report
 *    (bytes 0x88BD0..0x88D60) ──────────────────────────────────────────── */
static void sel_88bd0(const tsel_in_t *in)
{
    const uint8_t sel = TSEL_CAL_SEL;           /* r4 (movu.b) */
    int16_t r10, t;
    uint8_t r11, st, src;
    uint16_t f, code = 0u;
    die_8db5a(in);
    r10 = s_tsel.aba;
    r11 = s_tsel.s15e;
    s_tsel.s93c = r10;
    s_tsel.s0fd = r11;
    t = (int16_t)TSEL_CAL_TINIT;                /* r6 */
    st = 3u;                                    /* r1 */
    src = 0u;                                   /* r7 */
    if ((sel & 1u) != 0u && in->st94fee >= 3u) {
        src = 1u;
        st = in->st94fee;
        t = (int16_t)(TSEL_CAL_FOFF + in->t94f98);
        if (sel > 1u && in->st94fee >= 6u) {
            st = 6u;
        }
    }
    if ((sel & 2u) != 0u && !(st > r11)) {
        t = r10;
        st = r11;
        src = 2u;
        if (sel > 3u && r11 >= 6u) {
            st = 6u;
        }
    }
    if ((sel & 4u) != 0u && !(st > s_tsel.s100)) {
        src = 4u;
        st = s_tsel.s100;                       /* not capped */
#if defined(TSEL_NEG_SEL)
        /* NEGATIVE BUILD (ctest tsel_shadow_neg_SEL): the board capped at 6
         * like the die — test_tsel_shadow must FAIL */
        if (st >= 6u) {
            st = 6u;
        }
#endif
        t = (int16_t)(TSEL_CAL_BOFF + s_tsel.s93e);
    }
    s_tsel.s93a = t;
    s_tsel.s0fc = st;
    s_tsel.s170 = src;
    s_tsel.s0ff = tbyte(t);
    s_tsel.s0fe = tbyte(r10);
    f = s_tsel.ab6;
    s_tsel.af2 = ((f & 0x20u) != 0u) ? 0x62u : 0x60u;
    if ((f & 0xC4u) == 0xC4u) {
        code = 0x40u;
    }
    if ((f & 0xC2u) == 0xC2u) {
        code = (uint16_t)(code | 0x20u);
    }
    if ((f & 0xD0u) == 0xD0u && (s_tsel.af2 & 2u) != 0u) {
        code = (uint16_t)(code | 2u);
    }
    s_tsel.af0 = code;
    if (in->fm_ready != 0u) {                   /* 0x6F594 */
        rep_7c01e(s_tsel.af2, s_tsel.af0);
    }
}

/* ── the init hub 0x832AC's two entries ────────────────────────────────── */
void tsel_shadow_init(void)
{
    uint8_t *p = (uint8_t *)&s_tsel;
    for (unsigned k = 0u; k < sizeof s_tsel; k++) {
        p[k] = 0u;
    }
    /* 0x88BAC → 0x8DB28 */
    s_tsel.aba = (int16_t)TSEL_CAL_TINIT;
    s_tsel.b823 = TSEL_CAL_DSTART;
    s_tsel.w477a = TSEL_CAL_DHOLD;
    /* 0x7C086(0x62): reports only when 0x62 & 0xFF9D != 0 — never */
    if ((0x62u & 0xFF9Du) != 0u) {
        s_tsel.initrep_n++;
    }
    s_tsel.s93a = (int16_t)TSEL_CAL_TINIT;
    /* 0x88D98 → 0x8DD40 */
    s_tsel.s93e = (int16_t)TSEL_CAL_BINIT;
    s_tsel.w730 = TSEL_CAL_BNEVER;
    s_tsel.w77c = TSEL_CAL_BHOLD;
    s_tsel.ac0 = 0x80u;
    /* 0x7C0E8(0x160): reports only when 0x160 & 0xFE9F != 0 — never */
    if ((0x160u & 0xFE9Fu) != 0u) {
        s_tsel.initrep_n++;
    }
    s_tsel.sel_min = 0x7FFF;
    s_tsel.sel_max = (int16_t)-0x8000;
    s_tsel.init_n++;
}

/* ── the hub 0x83300's two entries (0x88DA8 before the fluid 0x887D8,
 *    0x88BD0 after it; the board part reads nothing the fluid writes, so
 *    running both after drv_temp_tick is the same computation) ─────────── */
void tsel_shadow_pass(const tsel_in_t *in)
{
    const uint8_t p0fc = s_tsel.s0fc, p170 = s_tsel.s170, p15e = s_tsel.s15e, p100 = s_tsel.s100;
    /* the countdown 0x7C7E4 ran n5 times since the last hub pass */
    s_tsel.w477a = cd16(s_tsel.w477a, in->n5);
    s_tsel.w77c = cd16(s_tsel.w77c, in->n5);
    s_tsel.b823 = cd8(s_tsel.b823, in->n5);
    s_tsel.in = *in;
    s_tsel.pass_n++;
    if ((uint16_t)in->r908cc == TSEL_CAL_BNOTRDY) {
        s_tsel.ff03_n++;
    }
    if ((uint16_t)in->r908cc == 0x8001u) {
        s_tsel.r8001_n++;
    }
    brd_88da8(in);                              /* 0x88DA8 */
    sel_88bd0(in);                              /* 0x88BD0 */
    if ((s_tsel.ac0 & 0x10u) != 0u) {
        s_tsel.lowbit_n++;
    }
    if (s_tsel.s15e == 8u) {
        s_tsel.die_ok_n++;
    }
    if (s_tsel.s100 == 8u) {
        s_tsel.brd_ok_n++;
        if (s_tsel.first_brd8_us == 0u) {
            s_tsel.first_brd8_us = (in->t_us != 0u) ? in->t_us : 1u;
        }
    }
    if (s_tsel.s0fc == 8u) {
        s_tsel.sel8_n++;
        if (s_tsel.first_sel8_us == 0u) {
            s_tsel.first_sel8_us = (in->t_us != 0u) ? in->t_us : 1u;
        }
    }
    if (s_tsel.s0fc >= 6u) {
        if (s_tsel.s93a < s_tsel.sel_min) {
            s_tsel.sel_min = s_tsel.s93a;
        }
        if (s_tsel.s93a > s_tsel.sel_max) {
            s_tsel.sel_max = s_tsel.s93a;
        }
    }
    if (s_tsel.s0fc != p0fc || s_tsel.s170 != p170 || s_tsel.s15e != p15e || s_tsel.s100 != p100) {
        tsel_tr_t *e = &s_tsel.tr[s_tsel.tr_head];
        e->t_us = in->t_us;
        e->pass = (uint16_t)s_tsel.pass_n;
        e->t_sel = s_tsel.s93a;
        e->r908cc = in->r908cc;
        e->st_sel = s_tsel.s0fc;
        e->src = s_tsel.s170;
        e->st_die = s_tsel.s15e;
        e->st_brd = s_tsel.s100;
        e->n908ca = in->n908ca;
        s_tsel.tr_head = (uint8_t)((s_tsel.tr_head + 1u) % TSEL_TR_N);
        s_tsel.tr_n++;
    }
}

const volatile tsel_t *tsel_shadow_state(void)
{
    return &s_tsel;
}

#endif /* !TCU_RAM_APP */
