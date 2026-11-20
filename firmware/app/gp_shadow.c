/* gp_shadow.c — THE READ-ONLY GEAR-PAIR SHIFT-ENGINE SHADOW (app/gp_shadow.h;
 * 2026-11-20, ~/tcu_sweep/gp_shadow_port.md; decode
 * ~/tcu_sweep/shift_engine_gearpair_decode.md).
 *
 * The stock's shift object 0xFFF8BD14 (header BD14, block A 0xBD6C, block B
 * 0xBEB4, each with an on-coming phase object at ctx+0xB0 and an off-going
 * one at ctx+0x34) and the gear-pair coordinator's E33C bytes, on this TU's
 * OWN RAM (gp_t, .gp_shadow). Every function names the stock routine it
 * ports; the bytes were re-read from the pristine image (sh-elf-objdump,
 * literal pools resolved) — addresses in the comments. The phase BODIES are
 * not decoded: the phase objects step their stock SEQUENCES (0x5EC24 /
 * 0x63555) on documented stand-ins (GP_SI_*, counted).
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

/* host seams (SH: compile-time 0 — the dump / the undecoded gates closed) */
#if defined(__sh__)
#define GP_EARLY_OK 0u
#define GP_BD26     GP_SI_BD26
#else
static uint8_t s_early_ok;
static uint8_t s_bd26;
#define GP_EARLY_OK s_early_ok
#define GP_BD26     s_bd26
void gp_shadow_host_si(uint8_t early_ok, uint8_t bd26)
{
    s_early_ok = early_ok;
    s_bd26 = bd26;
}
#endif
_Static_assert(GP_SI_BD26 == 0u && GP_SI_BD1B == 0u, "gp_shadow: the dump's BD14+0x26 / BD1B");

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
    const int32_t thr = (int32_t)gp_cal_sync_thr;
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
}

/* 0x117970(ctx, BD16): the progress init (+0x22 / +0 / +7 / +0x30 / +0x26;
 * 0x117E98 / 0x117F28 / 0xB8260 / 0xB827A not ported) */
static void p970(gp_blk_t *c)
{
    c->sync = 0u;
    W.unp_n++;
}

/* 0x11793E: the progress start (+0x31 = +0x28 = 1, +0x27 = BD1A ∈ {0x1E, 0x0A}),
 * then 0x117F48(ctx, 1, +0x27) */
static void p93e(gp_blk_t *c)
{
    c->hold = 1u;
    c->p27 = (uint8_t)((G.bd1a == 0x1Eu || G.bd1a == 0x0Au) ? 1u : 0u);
    f48(c, 1u, c->p27);
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

/* THE PHASE-DONE STAND-INS [I] (0x1138FC / 0x1106DC undecoded) */
static bool on_done(const gp_blk_t *c, const gp_po_t *o)
{
    W.si_phase_n++;
    if (c->act == 0u && o->phase >= 2u) {
        return false;                                /* block B: the pre-fill holds */
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
    case 0x0B:
    case 0x0C:
    case 0x0D: return o->tin >= GP_SI_T_SHORT;
    default:   return false;                         /* 0x12 final, 0x0E, 0xFF */
    }
}
static bool off_done(const gp_blk_t *c, const gp_po_t *o)
{
    W.si_phase_n++;
    if (c->act == 0u && o->phase >= 2u) {
        return false;
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

static uint8_t sat8(uint8_t v)
{
    return (uint8_t)(v == 0xFFu ? v : v + 1u);
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
            o->tin = sat8(o->tin);                   /* 0x113F16 (the output): not ported */
            return;
        }
        uint8_t nx;
        unsigned k = 0u;
        do {
            o->step = sat8(o->step);
            nx = on_next(o);
        } while (nx == o->phase && ++k < 16u);       /* 0x1128F2: equal entries skipped */
        o->phase = nx;
        o->tin = 0u;
        o->phases_n++;
        on_entry(c, o);
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
            o->tin = sat8(o->tin);                   /* 0x110CDC: not ported */
            return;
        }
        uint8_t nx;
        unsigned k = 0u;
        do {
            o->step = sat8(o->step);
            nx = off_next(o);
        } while (nx == o->phase && ++k < 16u);
        o->phase = nx;
        o->tin = 0u;
        o->phases_n++;
        W.unp_n++;                                   /* 0x10FFBE (the entry): not ported */
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
    if (c->on.elem != 0u) {
        c->on.out = 0u;
        c->on.mode = (uint8_t)((c->on.phase == 1u) ? 1u : 0u);
    }
    if (c->off.elem != 0u) {
        c->off.out = 0u;
        c->off.mode = 0u;
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
    }
    if (p3) {
        off_end(&c->off);                            /* +0x1A = 0x10FF9A; 0x10FBFC */
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
    ed988();                                         /* 0xED906 → 0xED988 (+ 0x117B4A) */
    W.unp_n++;                                       /* 0x117B4A / 0xED92E (0x1176E0, torque) */
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
        off_start(&c->off);                          /* 0x10FC5C */
    }
    if (p == 1u) {
        W.unp_n++;                                   /* 0x112142, 0xE5E48, 0xE9404 */
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

/* 0xECB14: block A ↔ block B (through BD14+0x320), 0x117CE4 on both, A+3 = 1,
 * B+3 = 0 */
static void ecb14(void)
{
    const gp_blk_t t = G.a;
    G.a = G.b;
    G.b = t;
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
    const bool applied = (G.a.on.phase == 0x12u);
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
    /* slot 35: 0xEB348 (while BD17 != 4): the steps on B, on A, off B, off A;
     * the outputs */
    if (G.bd17 != 4u) {
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

void gp_shadow_init(void)
{
    uint8_t *p = (uint8_t *)&s_gp;
    for (size_t i = 0u; i < sizeof s_gp; i++) {
        p[i] = 0u;
    }
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
