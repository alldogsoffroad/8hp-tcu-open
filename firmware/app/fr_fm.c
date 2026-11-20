/* fr_fm.c — THE GENERIC FAULT MANAGER of the read-only fault-reaction shadow
 * (app/fr_shadow.h; F1, 2026-11-19, ~/tcu_sweep/f1_fr_shadow_port.md; decode
 * ~/tcu_sweep/f0_fault_chain_decode.md §4, u1267_debounce_decode.md,
 * fault_manager_decode.md). RAM ONLY, over all 195 group configs
 * (app/fr_cal.c, 0x1923EC). This TU is the only writer of the FM cells
 * (fr_fm_t, its own pinned section .fr_fm); everything else reads them
 * through the const accessors.
 *
 * Ported (the stock routine named at each function):
 *   0x9BE50  report: the enable (cfg+0x21 b0), the suppression (0xFFF8DEE6 /
 *            0xFFF8E579 & cfg+0x20), the dependency 0x9CDA8 (cfg+0x21 b1),
 *            the operating condition (0xFFF898D6 & cfg+0x22), the untested
 *            path 0x9C342 (cfg+0 b11 / cfg+0x21 b2 stop a running timer),
 *            fail_step 0x9C516 → 0x9C6A8 / 0x9C98A / 0x9C124, pass_step
 *            0x9C960 / 0x9C58C / 0x9C43C, the pending reactions 0xAB102 /
 *            0xAB1A6 (cfg+0x12 threshold, +0x14 / +0x18 words, per-bit
 *            refcounts), confirm 0x9CA70 / 0x9C720 (0x9CF9E ORs cfg+8 / +0xC),
 *            stop 0x9C110, heal 0x9C608 (+ 0x9CF88, the mode-2 rebuild 0xAAC0E)
 *   0x9B918  the direct confirm (the node groups) — WITHOUT its record /
 *            environment calls (0xABF98, 0x9CE52, 0x9D0CA, 0x9CFF0, 0x9DA1A,
 *            0x9D232, 0x9CCAE, 0x9CCF6, 0x9D090, 0xAB7B6, 0x9CFC4)
 *   0xABDFA  the rebuild rule (modes 1 / 2; mode 0 = power-up with nothing
 *            stored = all masks 0, no call)
 *   0xAE890  the operating cycle → 0xAACE4 (terminal re-cycle: T = 0, S &=
 *            0xDBBB, the pending masks and refcounts zeroed, the mode-1 rule)
 *   0xA031C / 0x9F718 / 0xA0FC4 / 0xA0FA0  the getters (0x897D4 / DC, the
 *            record-memory-full masks, are 0: unlimited virtual records)
 *
 * The record is an in-RAM byte per group (fr_fm_t.r): it is never persisted,
 * no record / aging / UDS-status / event-log / fatal code exists here
 * (g6a_scan.py nvm checks the object).
 *
 * Stand-ins (the driving dump): 0xFFF8E57D = 0xA5 (stock-true), 0xFFF8E579 =
 * 0x10, 0xFFF8DEE6 = 0, 0xFFF898D6 = 0x17. The dependency master's rule bit
 * (0x9AA00(master) & 0x100) is taken as clear (its S b3 is tested): only
 * groups 97 / 149 / 150 carry masters and none of them is reported in this
 * shadow (counted dep_n). The 0xABDFA occurrence conditions (0x200 / 0x40 /
 * 4: the record's occurrence count against a cal not decoded) are taken as
 * "drop" (counted rule_c2_n; no reported group's rule has them).
 */
#define TCU_FR_SHADOW_TU 1
#include "app/fr_shadow.h"
#include "app/fr_cal.h"

#if !defined(TCU_RAM_APP)

#if defined(__sh__)
#define FMSEC __attribute__((section(".fr_fm")))
#else
#define FMSEC
#endif

static fr_fm_t s_fm FMSEC;
#define F s_fm

#define SI_E57D  0xA5u      /* 0xFFF8E57D: fault memory ready (0x9F7AA writes 0xA5) */
#define SI_E579  0x10u      /* 0xFFF8E579 */
#define SI_DEE6  0x00u      /* 0xFFF8DEE6 (per-group suppression bits) */
#define SI_898D6 0x17u      /* 0xFFF898D6: the operating condition */
_Static_assert(SI_E57D == 0xA5u && SI_E579 == 0x10u && SI_DEE6 == 0u && SI_898D6 == 0x17u,
               "fr_fm: every stand-in equals the stock driving dump");

/* the record byte */
#define R_TPASS  0x01u
#define R_FAILED 0x02u
#define R_ACTIVE 0x08u
#define R_MASK   0x10u
#define R_EXISTS 0x80u
/* the status word */
#define S_TESTED 0x0001u
#define S_FAILED 0x0002u
#define S_TIMER  0x0004u
#define S_CONF   0x0008u
#define S_INMASK 0x0020u
#define S_PEND   0x0040u
#define S_DEP    0x0100u
#define S_CONF13 0x2000u

#define CFG(g) (fr_cal_grp[(g) - 1u])

static void rec(uint8_t kind, uint8_t g, uint16_t v)
{
    F.rec_all++;
    if (F.rec_n < FR_REC_N) {
        fr_ev_t *e = &F.rec[F.rec_n];
        e->ms = F.now;
        e->kind = kind;
        e->g = g;
        e->v = v;
        F.rec_n++;
    }
}

void fr_fm_note(uint8_t kind, uint8_t g, uint16_t v)
{
    rec(kind, g, v);
}

/* ── the pending reactions 0xAB102 / 0xAB1A6 ───────────────────────────── */
static void pend_add(uint8_t g)
{
    const uint8_t *c = CFG(g);
    const uint32_t w[2] = { FRC_P0(c), FRC_P1(c) };
    F.s[g - 1u] = (uint16_t)(F.s[g - 1u] | S_PEND);
    for (unsigned k = 0u; k < 64u; k++) {
        if ((w[k >> 5] >> (k & 31u)) & 1u) {
            if (F.rc[k] < 0xFFu) {
                F.rc[k]++;
            }
            if (k < 32u) {
                F.p0 |= (1u << k);
            } else {
                F.p1 |= (1u << (k - 32u));
            }
        }
    }
    rec(FR_EV_PEND_ON, g, F.t[g - 1u]);
}

static void pend_remove(uint8_t g)
{
    if ((F.s[g - 1u] & S_PEND) == 0u) {
        return;
    }
    const uint8_t *c = CFG(g);
    const uint32_t w[2] = { FRC_P0(c), FRC_P1(c) };
    F.s[g - 1u] = (uint16_t)(F.s[g - 1u] & ~S_PEND);
    for (unsigned k = 0u; k < 64u; k++) {
        if ((w[k >> 5] >> (k & 31u)) & 1u) {
            if (F.rc[k] > 0u) {
                F.rc[k]--;
            }
            if (F.rc[k] == 0u) {
                if (k < 32u) {
                    F.p0 &= ~(1u << k);
                } else {
                    F.p1 &= ~(1u << (k - 32u));
                }
            }
        }
    }
    rec(FR_EV_PEND_OFF, g, F.t[g - 1u]);
}

/* ── 0x9C110 stop / c0b2 start ─────────────────────────────────────────── */
static void stop(uint8_t g)
{
    F.t[g - 1u] = 0u;
    F.s[g - 1u] = (uint16_t)(F.s[g - 1u] & ~S_TIMER);
    pend_remove(g);
}

static void start(uint8_t g, uint16_t v, uint8_t fail)
{
    F.t[g - 1u] = v;
    if (fail != 0u && F.tmax[g - 1u] < 1u) {
        F.tmax[g - 1u] = 1u;
    }
    F.s[g - 1u] = (uint16_t)(F.s[g - 1u] | S_TIMER);
    F.d[g - 1u] = (uint16_t)F.now;
    if (fail != 0u) {
        rec(FR_EV_START, g, v);
    }
}

/* ── 0xABDFA: 1 = drop the record from the masks ──────────────────────── */
static uint8_t rule_drop(uint8_t g, uint8_t mode)
{
    const uint8_t *c = CFG(g);
    const uint16_t rule = FRC_RULE(c);
    uint16_t en, c1, c2, c3;
    if (mode == 1u) {
        en = rule & 0x10u; c1 = rule & 0x20u; c2 = rule & 0x40u; c3 = rule & 0x80u;
    } else {
        en = rule & 0x100u; c1 = 1u; c2 = rule & 0x200u; c3 = rule & 0x400u;
    }
    if (en == 0u) {
        return 0u;
    }
    uint8_t drop = 1u;
    if (c1 != 0u && (F.r[g - 1u] & R_ACTIVE) != 0u) {
        drop = 0u;                                  /* the record still active */
    }
    if (c2 != 0u) {
        F.rule_c2_n++;                              /* the occurrence count: cal not decoded */
    }
    if (c3 != 0u) {
        /* [I] the operating condition not fully present keeps the record */
        const uint16_t opc = FRC_OPC(c);
        if ((uint16_t)(opc & SI_898D6) != opc) {
            drop = 0u;
        }
    }
    return drop;
}

static void masks_from_records(void)
{
    uint32_t m0 = 0u, m1 = 0u;
    for (uint8_t g = 1u; g <= FR_NGRP; g++) {
        if ((F.r[g - 1u] & (R_EXISTS | R_MASK)) == (R_EXISTS | R_MASK)) {
            m0 |= FRC_R0(CFG(g));
            m1 |= FRC_R1(CFG(g));
        }
    }
    if (m0 != F.m0 || m1 != F.m1) {
        rec(FR_EV_MASKS, 0u, (uint16_t)((m0 ^ F.m0) ^ (m1 ^ F.m1)));
    }
    F.m0 = m0;
    F.m1 = m1;
}

/* 0xAAC0E (mode 2, a heal) / 0xAACE4's rebuild (mode 1) over every record */
static void rebuild(uint8_t mode)
{
    for (uint8_t g = 1u; g <= FR_NGRP; g++) {
        if ((F.r[g - 1u] & (R_EXISTS | R_MASK)) == (R_EXISTS | R_MASK) && rule_drop(g, mode)) {
            F.r[g - 1u] = (uint8_t)(F.r[g - 1u] & ~R_MASK);
            F.s[g - 1u] = (uint16_t)(F.s[g - 1u] & ~S_INMASK);
        }
    }
    masks_from_records();
}

/* ── confirm 0x9CA70 / 0x9C720 ─────────────────────────────────────────── */
static void confirm(uint8_t g)
{
    const uint8_t *c = CFG(g);
    const uint8_t was = (uint8_t)((F.s[g - 1u] & S_CONF) != 0u);
    const uint32_t w0 = FRC_R0(c), w1 = FRC_R1(c);
    F.r[g - 1u] = (uint8_t)(F.r[g - 1u] | R_EXISTS | 0x4Fu);   /* 0x9CE52: +4 |= 0x4F */
    if ((w0 | w1) != 0u) {
        F.r[g - 1u] = (uint8_t)(F.r[g - 1u] | R_MASK);
        F.s[g - 1u] = (uint16_t)(F.s[g - 1u] | S_INMASK);
        if (((F.m0 | w0) != F.m0) || ((F.m1 | w1) != F.m1)) {
            rec(FR_EV_MASKS, g, (uint16_t)((w0 & ~F.m0) ^ (w1 & ~F.m1)));
        }
        F.m0 |= w0;                                 /* 0x9CF9E */
        F.m1 |= w1;
    }
    pend_remove(g);
    F.s[g - 1u] = (uint16_t)(F.s[g - 1u] | 0x2098u);
    stop(g);
    if (F.occ[g - 1u] < 0xFFu) {
        F.occ[g - 1u]++;
    }
    F.conf_n++;
    if (!was) {
        rec(FR_EV_CONFIRM, g, F.tmax[g - 1u]);     /* a re-confirm (the direct path every
                                                     * pass while lost) is not recorded */
    }
}

/* ── heal 0x9C608 ──────────────────────────────────────────────────────── */
static void heal(uint8_t g)
{
    const uint8_t had = (uint8_t)((F.r[g - 1u] & R_EXISTS) != 0u);
    if (had) {
        F.r[g - 1u] = (uint8_t)((F.r[g - 1u] & 0xF5u) | R_TPASS);   /* 0x9CF88 */
        rebuild(2u);                                                 /* 0xAAC0E */
    }
    stop(g);
    F.s[g - 1u] = (uint16_t)(F.s[g - 1u] & ~S_CONF);
    if (had) {
        F.heal_n++;
        rec(FR_EV_HEAL, g, 0u);
    }
}

/* ── 0x9CDA8 the dependency (cfg rows 0x1955A8) ───────────────────────── */
static uint8_t dep(uint8_t g)
{
    uint8_t hit = 0u;
    for (unsigned k = 0u; k < 3u; k++) {
        const uint16_t m = fr_cal_dep[g - 1u][k];
        if (m == 0u || m > FR_NGRP) {
            break;                                  /* 0xA09FC: no (further) master */
        }
        if (m == g) {
            continue;
        }
        F.dep_n++;
        if ((F.s[m - 1u] & S_CONF) != 0u) {         /* [stand-in: 0x9AA00's bit clear → S b3] */
            hit = 1u;
            break;
        }
    }
    if (hit) {
        F.s[g - 1u] = (uint16_t)(F.s[g - 1u] | S_DEP);
    } else {
        F.s[g - 1u] = (uint16_t)(F.s[g - 1u] & ~S_DEP);
    }
    return hit;
}

/* the common gates of 0x9BE50 / 0x9B918: 0 = the report is dropped */
static uint8_t gates(uint8_t g)
{
    const uint8_t *c;
    if (g < 1u || g > FR_NGRP) {
        F.bad_g_n++;
        return 0u;
    }
    if (SI_E57D != 0xA5u) {
        return 0u;
    }
    c = CFG(g);
    if ((FRC_E21(c) & 1u) == 0u) {
        return 0u;                                  /* group disabled (137, 144, …) */
    }
    if ((SI_DEE6 != 0u) || (SI_E579 & FRC_E20(c)) != 0u) {
        return 0u;
    }
    return 1u;
}

/* ── 0x9BE50 ───────────────────────────────────────────────────────────── */
void fr_fm_report(uint8_t g, uint8_t failed, uint8_t tested)
{
    const uint8_t *c;
    uint16_t el;
    F.rep_n++;
    if (!gates(g)) {
        return;
    }
    c = CFG(g);
    const uint16_t fail_t = FRC_FAIL(c), heal_t = FRC_HEAL(c);
    const uint8_t d = dep(g);
    if (d != 0u && (failed != 0u || (FRC_E21(c) & 2u) == 0u)) {
        tested = 0u;
    }
    if (tested != 0u && (uint16_t)(SI_898D6 & FRC_OPC(c)) != FRC_OPC(c)) {
        tested = 0u;
    }
    uint16_t *const S = &F.s[g - 1u];
    uint16_t *const T = &F.t[g - 1u];
    uint16_t *const D = &F.d[g - 1u];
    uint8_t *const R = &F.r[g - 1u];
    const uint16_t now = (uint16_t)F.now;
    if (tested == 0u) {
        /* 0x9C342: untested time never counts */
        *D = now;
        if (*T != 0u && (((*S & S_FAILED) != 0u) ? ((FRC_FLAGS(c) & 0x0800u) != 0u)
                                                 : ((FRC_E21(c) & 0x04u) != 0u))) {
            stop(g);
        }
        *S = (uint16_t)(*S & ~S_TESTED);
        return;
    }
    if (failed != 0u) {
        if (*T == 0u) {
            /* 0x9C6A8 */
            if ((*R & R_EXISTS) != 0u) {
                *R = (uint8_t)(*R | 0x03u);
            }
            if (!((*R & R_ACTIVE) != 0u && (*S & S_CONF13) != 0u)) {
                if (fail_t > 0u) {
                    start(g, 1u, 1u);
                } else {
                    confirm(g);
                }
            }
        } else {
            /* 0x9C98A → 0x9C124 */
            el = (uint16_t)(now - *D);
            *D = now;
            if ((uint32_t)*T + el > 0xFFFFu) {
                *T = 0xFFFFu;
            } else {
                *T = (uint16_t)(*T + el);
            }
            if (*T > fail_t) {
                *T = fail_t;
            }
            if (*T > F.tmax[g - 1u]) {
                F.tmax[g - 1u] = *T;
            }
            if (*T >= fail_t) {
                confirm(g);
            } else if ((*R & R_EXISTS) != 0u) {
                *R = (uint8_t)(*R | 0x03u);
            }
        }
        /* the pending reactions (0xAB102) */
        if (*T != 0u && *T >= FRC_PTHR(c) && (*S & S_PEND) == 0u &&
            !((*S & S_CONF) != 0u && (*S & S_CONF13) != 0u) && (FRC_P0(c) | FRC_P1(c)) != 0u) {
            pend_add(g);
        }
        *S = (uint16_t)(*S | S_FAILED | 0x0400u);
    } else {
        if (*T != 0u) {
            /* 0x9C58C → 0x9CC54 */
            if (*T > heal_t) {
                *T = heal_t;
            } else {
                el = (uint16_t)(now - *D);
                *D = now;
                *T = (*T > el) ? (uint16_t)(*T - el) : 0u;
            }
            if (*T == 0u) {
                heal(g);
            } else if ((*R & R_EXISTS) != 0u) {
                *R = (uint8_t)((*R | R_TPASS) & ~R_FAILED);
            }
        } else {
            /* 0x9C43C */
            if ((*R & R_EXISTS) == 0u) {
                *S = (uint16_t)(*S & ~S_CONF);
            } else {
                *R = (uint8_t)((*R | R_TPASS) & ~R_FAILED);
                if ((*R & R_ACTIVE) != 0u) {
                    if (heal_t > 0u) {
                        start(g, heal_t, 0u);
                    } else {
                        heal(g);
                    }
                }
            }
        }
        *S = (uint16_t)(*S & ~S_FAILED);
    }
    *S = (uint16_t)(*S | S_TESTED);
}

/* ── 0x9B918: the direct confirm (no record calls) ────────────────────── */
void fr_fm_direct(uint8_t g)
{
    if (!gates(g)) {
        return;
    }
    const uint8_t *c = CFG(g);
    (void)dep(g);
    if ((uint16_t)(SI_898D6 & FRC_OPC(c)) != FRC_OPC(c)) {
        return;
    }
    const uint8_t was = (uint8_t)((F.s[g - 1u] & S_CONF) != 0u);
    F.s[g - 1u] = (uint16_t)(F.s[g - 1u] | 0x1603u);
    F.direct_n++;
    if (!was) {
        rec(FR_EV_DIRECT, g, F.t[g - 1u]);
    }
    confirm(g);
}

/* ── the operating cycle 0xAE890 → 0xAACE4 ────────────────────────────── */
static void recycle(void)
{
    for (uint8_t g = 1u; g <= FR_NGRP; g++) {
        F.t[g - 1u] = 0u;
        F.s[g - 1u] = (uint16_t)(F.s[g - 1u] & 0xDBBBu);
    }
    F.p0 = 0u;
    F.p1 = 0u;
    for (unsigned k = 0u; k < 64u; k++) {
        F.rc[k] = 0u;
    }
    rebuild(1u);
    F.recycle_n++;
    rec(FR_EV_RECYCLE, 0u, 0u);
}

void fr_fm_tick(uint8_t e9fe)
{
    F.now += 10u;               /* our 0xFFF89898 (0xADE6E samples the ms counter per 10 ms task) */
    switch (F.cyc) {
    case 1u:
        if (e9fe == 1u) {
            F.cyc = 2u;
        }
        break;
    case 2u:
        if (e9fe == 0u) {
            F.cyc = 3u;
        }
        break;
    case 3u:
        if (e9fe == 1u) {
            F.cyc = 2u;
            recycle();
        }
        break;
    default:
        F.cyc = 1u;
        break;
    }
}

/* ── the getters ───────────────────────────────────────────────────────── */
uint8_t fr_fm_react(uint8_t id)
{
    if (id < 32u) {
        return (uint8_t)(((F.m0 | F.p0) >> id) & 1u);
    }
    if (id < 64u) {
        return (uint8_t)(((F.m1 | F.p1) >> (id - 32u)) & 1u);
    }
    return 0u;
}

uint32_t fr_fm_bank(uint8_t k)
{
    return (k == 0u) ? (F.m0 | F.p0) : (F.m1 | F.p1);
}

uint8_t fr_fm_failing(uint8_t g)
{
    return (uint8_t)(g >= 1u && g <= FR_NGRP && (F.s[g - 1u] & 0x8000u) == 0u &&
                     (F.s[g - 1u] & 0x000Cu) != 0u);
}

uint8_t fr_fm_confirmed(uint8_t g)
{
    return (uint8_t)(g >= 1u && g <= FR_NGRP && (F.s[g - 1u] & 0x8000u) == 0u &&
                     (F.s[g - 1u] & S_CONF) != 0u);
}

uint8_t fr_fm_in_masks(uint8_t g)
{
    return (uint8_t)(g >= 1u && g <= FR_NGRP &&
                     (F.r[g - 1u] & (R_EXISTS | R_MASK)) == (R_EXISTS | R_MASK));
}

void fr_fm_init(void)
{
    uint8_t *p = (uint8_t *)&s_fm;
    for (size_t i = 0u; i < sizeof s_fm; i++) {
        p[i] = 0u;
    }
    F.cyc = 1u;                 /* 0xAE868: the cycle state at power-up; 0xA016A with nothing
                                 * stored: every mask 0 */
}

const volatile fr_fm_t *fr_fm_state(void)
{
    return &s_fm;
}

#endif /* !TCU_RAM_APP */
