/* gs_bridge.c — THE GARAGE BRIDGE, STAGE G1: computed, recorded, NOT
 * applied (ours, not stock; app/gs_bridge.h, bsw/drv/drv_gsbr.h;
 * ~/tcu_sweep/garage_live_plan.md, ~/tcu_sweep/garage_g1_port.md).
 *
 * One pass per 10 ms task (app/main.c, after the garage shadow):
 *   1. the shadow's commit / completion → the engagement timer, the commit
 *      count and the recorder slot (slot 0 = the last D, 1 = the last R);
 *   2. the arm conditions of plan §2.4 → GSB_R_* (each one a bit);
 *   3. with every condition met: the compile-time element bounds of §2.2
 *      (the moving channels in [500, 8500], exactly 500 / 8500 in a mode-1
 *      pass; every fixed channel at its pinned value) → GSB_G_*; a trip
 *      latches the bridge off until power-off (.noinit);
 *   4. would-arm ⇔ no reason; the 30 s armed-time budget, the 3.5 s
 *      engagement limit and the 16-commit cap count in .noinit (they gate
 *      the would-arm exactly as they would gate G2's arm);
 *   5. the would-be currents (the shadow's while would-armed, else the live
 *      producer's) → the would-be E8 / F0 words, recorded per pass with the
 *      live staged words and the real-feedback estimate 0x93394;
 *   6. the OS-mode selector 0x1418E2 on our inputs (read-only witness).
 * The seam drv_gsbr_take() exists only with GS_LIVE_DIR != 0 (host dry-run
 * builds in G1); on SH it is not compiled (drv_gsbr.h: _Static_assert).
 * (G2, r04i: the SH image builds GS_LIVE_DIR 3 — the seam is live.)
 * STAGE G4b (2026-11-19, gs_bridge.h, ~/tcu_sweep/g4b_release_bridge_port.md):
 * after a bridged engagement the stock's release (the shadow's BD16 = 4)
 * stays bridged until its completion, inside the release envelope; state
 * and recorder in .gsb_rel / .gsb_rel_est.
 *
 * Live cells are READ through const volatile literals (ctest
 * gs_bridge_scan bridge), never written; the only writes are to this TU's
 * own objects (.gsb_win / .gsb_state / .gsb_rec0 / .gsb_rec1).
 */
#define TCU_GS_BRIDGE_TU 1
#include "app/gs_bridge.h"
#include "app/gs_shadow.h"
#include "bsw/drv/drv_gsbr.h"
#include "app/park_cells.h"       /* PCELL_TGT: the live range D6D6, read-only */

#if defined(TCU_RAM_APP)

void gs_bridge_init(bool cold) { (void)cold; }
void gs_bridge_tick_10ms(const gsb_in_t *in) { (void)in; }

#else

#if (GS_LIVE_DIR != 0) && (GS_FORCED == 0)
#  error "gs bridge: a live bridge needs the forced garage shadow (GS_FORCED 1): the stock-faithful shadow never commits"
#endif

/* the directions the WITNESS evaluates (both: the G1 recorder shows D and R
 * as G2 / G3 would arm them); the seam applies only GS_LIVE_DIR's */
#define GSB_EVAL_DIR    3u

/* ── the compile-time element bounds (plan §2.2) ──────────────────────────
 * channel ch: 0 A, 1 D, 2 B, 3 E, 4 C. Derived from the stock tables
 * [O bytes, host test gs_bridge_tables against the image]:
 *   role tables 0x6312F / 0x63139 / 0x63143 (first / second / third
 *   element, 1 A 2 B 3 C 4 D 5 E): 1st = B, A, C; R = B, A, D — the second
 *   element A is dropped as already applied (0xDC282: block 0 gear 1 = A);
 *   table 0x505A4 block 0 gear 1 (the live P content) = A; block 1 gear 1
 *   = A B C, block 1 R = A B D. A fixed channel must hold the SAME value in
 *   both rows: applied normally-closed A → 8500; released normally-open
 *   C / D / E → 8500 (0x107CFC, cal 500 / 8500). */
#define GSB_CH_A 0u
#define GSB_CH_D 1u
#define GSB_CH_B 2u
#define GSB_CH_E 3u
#define GSB_CH_C 4u
#define GSB_D_MOVE1     GSB_CH_B    /* first element of 1st */
#define GSB_D_MOVE3     GSB_CH_C    /* third element of 1st */
#define GSB_R_MOVE1     GSB_CH_B    /* first element of R */
#define GSB_R_MOVE3     GSB_CH_D    /* third element of R */
#ifndef GSB_SH_MUT_CH               /* SH compile negative: a bridged channel >= 5 */
#  define GSB_SH_MUT_CH GSB_D_MOVE3
#endif
_Static_assert(GSB_D_MOVE1 < 5u && GSB_SH_MUT_CH < 5u && GSB_R_MOVE1 < 5u && GSB_R_MOVE3 < 5u,
               "gs bridge: only channels 0..4 (A D B E C) may be bridged — never WK, Sys, the park release or ch 8/9");
#define GSB_D_MOVE  ((uint8_t)((1u << GSB_D_MOVE1) | (1u << GSB_SH_MUT_CH)))
#define GSB_R_MOVE  ((uint8_t)((1u << GSB_R_MOVE1) | (1u << GSB_R_MOVE3)))
_Static_assert(GSB_D_MOVE == 0x14u && GSB_R_MOVE == 0x06u,
               "gs bridge: D moves only B and C, R moves only B and D");
#define GSB_PIN     8500u           /* every fixed channel, both directions */
#define GSB_IMIN_P  500u            /* B released in the P content (cal 0x1FEF6C) */
#if defined(GSB_HOST_MUT_TABLE) && !defined(__sh__)
#  define GSB_PIN_D_E 500u          /* host negative: E pinned at 500 in D */
#else
#  define GSB_PIN_D_E GSB_PIN
#endif
static const uint16_t GSB_FIX_D[5] = { GSB_PIN, GSB_PIN, 0u, GSB_PIN_D_E, 0u };
static const uint16_t GSB_FIX_R[5] = { GSB_PIN, 0u, 0u, GSB_PIN, GSB_PIN };
/* the P content (block 0, gear code 1 or −1: A applied; B released 500,
 * D / E / C released normally-open 8500) — the live content while the
 * shadow releases (G4a) */
static const uint16_t GSB_PCONT[5] = { GSB_PIN, GSB_PIN, GSB_IMIN_P, GSB_PIN, GSB_PIN };
#define GSB_IMIN    500u            /* 0xB7786 clamp, cal 0x1FEF6C */
#define GSB_IMAX    8500u           /* cal 0x1FEF6E */

/* ── the live cells read (const volatile, never written) ──────────────── */
#define LIVE_U8(a)   (*(const volatile uint8_t  *)(uintptr_t)(a))
#define LIVE_U16(a)  (*(const volatile uint16_t *)(uintptr_t)(a))
#define L_OS_MODE    0xFFF93B33u    /* the applied OS mode */
#define L_SEQ2       0xFFF94FD0u    /* seq2 state [0] */
#define L_LETTER     0xFFF90944u    /* letter [0] ('c' = 0x63) */
#define L_PARK_MODEL 0xFFF8BC70u    /* park_law's model [0] (1 released) */
#define L_FW         0xFFF93004u    /* u16 [10] the fault word */
#define L_CCLS       0xFFF93A42u    /* u8 [10] the channel class */
#define L_OST        0xFFF93A38u    /* u8 [10] the off-test state */
#define L_PROD       0xFFF84B78u    /* u16 [12] the live producer's currents */
#define L_PROD_C     0xFFF84B90u    /* u16 [12] their complements */
#define L_EST        0xFFF93394u    /* u16 [10] the real-feedback estimate */
#define L_ROUND      0xFFF9090Au    /* u8 the 0x90118 rounding offset */
#define L_E9FF       0xFFF8E9FFu    /* u8 the terminal-15 state (kl15) */
#define L_SUPPLY     0xFFF94F86u    /* u16 the supply (0x8D8DE output) */

/* ── RAM: OUR objects (not stock cells) ───────────────────────────────── */
#if defined(__sh__)
#  define GSB_WIN __attribute__((section(".gsb_win")))
#  define GSB_ST  __attribute__((section(".gsb_state")))
#  define GSB_RC0 __attribute__((section(".gsb_rec0")))
#  define GSB_RC1 __attribute__((section(".gsb_rec1")))
#  define GSB_REL __attribute__((section(".gsb_rel")))
#  define GSB_RES __attribute__((section(".gsb_rel_est")))
#else
#  define GSB_REL
#  define GSB_RES
#  define GSB_WIN
#  define GSB_ST
#  define GSB_RC0
#  define GSB_RC1
#endif
static volatile gsb_win_t s_gsb_win GSB_WIN;    /* NOT cleared on a warm boot */
static volatile gsb_t     s_gsb     GSB_ST;
static volatile gsb_rec_t s_gsb_rec0[GSB_REC_N] GSB_RC0;   /* the last D */
static volatile gsb_rec_t s_gsb_rec1[GSB_REC_N] GSB_RC1;   /* the last R */
static volatile gsbr_t    s_gsbr    GSB_REL;     /* G4b: the bridged release */
static volatile gsbr_est_t s_gsbr_est GSB_RES;   /* G4b: its estimate record */
#define B (s_gsb)
#define W (s_gsb_win)
#define R (s_gsbr)

#if !defined(__sh__)
static uint8_t  s_inj_ch = 0xFFu;
static uint16_t s_inj_v;
static uint32_t s_inj_from, s_inj_to;
void gs_bridge_host_inject(uint8_t ch, uint16_t v, uint32_t from, uint32_t to)
{
    s_inj_ch = ch;
    s_inj_v = v;
    s_inj_from = from;
    s_inj_to = to;
}
#endif

static void zero(volatile void *p, uint32_t n)
{
    volatile uint8_t *b = (volatile uint8_t *)p;
    for (uint32_t i = 0u; i < n; i++) {
        b[i] = 0u;
    }
}

static volatile gsb_rec_t *rec_at(uint8_t slot, uint8_t i)
{
    return slot == 0u ? &s_gsb_rec0[i] : &s_gsb_rec1[i];
}

static void win_seal(void)
{
    W.magic = GSB_WIN_MAGIC;
    W.acc_inv = ~W.acc_ms;
    W.commits_inv = (uint16_t)~W.commits;
    W.latch_inv = (uint8_t)~W.latch;
}

static bool win_ok(void)
{
    return W.magic == GSB_WIN_MAGIC && W.acc_inv == ~W.acc_ms &&
           W.commits_inv == (uint16_t)~W.commits && W.latch_inv == (uint8_t)~W.latch;
}

static void latch(uint8_t bits)
{
    W.latch = (uint8_t)(W.latch | bits);
    win_seal();
}

void gs_bridge_init(bool cold)
{
    zero(&s_gsb, sizeof s_gsb);
    zero(s_gsb_rec0, sizeof s_gsb_rec0);
    zero(s_gsb_rec1, sizeof s_gsb_rec1);
    zero(&s_gsbr, sizeof s_gsbr);
    zero(&s_gsbr_est, sizeof s_gsbr_est);
    R.rec = 0xFFu;
    R.slot[0].end_t = GSB_NONE;
    R.slot[1].end_t = GSB_NONE;
    B.slot_cur = 0xFFu;
    for (unsigned k = 0u; k < 2u; k++) {
        B.slot[k].arm_on = GSB_NONE;
        B.slot[k].arm_off = GSB_NONE;
        B.slot[k].done_at = GSB_NONE;
        B.slot[k].back_at = GSB_NONE;
    }
    /* the windows: the drv_rq_win_t mechanics — power-on: a fresh budget;
     * warm reset: carried; warm reset with a broken guard: spent, latched */
    if (cold) {
        W.acc_ms = 0u;
        W.commits = 0u;
        W.latch = 0u;
        W.warm = 0u;
        W.broken = 0u;
    } else {
        if (!win_ok()) {
            W.acc_ms = GSB_WIN_MS;
            W.latch = (uint8_t)(GSB_L_WIN | GSB_L_ENGT | GSB_L_COMMITS | GSB_L_GUARD);
            W.commits = (uint16_t)(GSB_COMMITS_MAX + 1u);
            W.broken = 1u;
        }
        W.warm = (uint8_t)(W.warm + 1u);
    }
    if (W.acc_ms >= GSB_WIN_MS) {
        W.latch = (uint8_t)(W.latch | GSB_L_WIN);
    }
    win_seal();
}

/* the would-be word: the staged word's command (E8 / F0 row) with the
 * setpoint 0x901A0 composes from current cur — (I + 0x9090A) × 1/6, as
 * drv_reqlaw.c rq_setpoint (the rounding offset the last compose used) */
static uint16_t word_of(uint8_t ch, uint16_t staged, uint16_t cur)
{
    const uint16_t cmd = (staged != 0u) ? (uint16_t)(staged & 0xF800u)
                                        : (uint16_t)(((ch & 1u) != 0u) ? 0xF000u : 0xE800u);
    const uint16_t sp = (uint16_t)(int32_t)((float)((uint32_t)cur + LIVE_U8(L_ROUND)) * 0x1.555556p-3f);
    return (uint16_t)(cmd | (uint16_t)(sp & 0x7FFu));
}

static uint8_t cyc_idx(uint8_t ch)
{
    return (uint8_t)((((ch & 1u) != 0u) ? 20u : 10u) + (ch >> 1));
}

/* ── 0x1418E2, the OS-mode selector (bytes 0x1418E2..0x141996), on our
 * inputs; a witness only (never written to 0x93B5E) ────────────────────── */
#define M2_ENG_MIN  150u        /* cal 0x195F64 (table 0x649F0 [0]) */
#define M2_TUR_MIN  100u        /* cal 0x195F66 ([1]) */
#define M2_SUP_MAX  17000u      /* cal 0x195F68 ([2]) */
#define M2_SUP_MIN  6000u       /* cal 0x195F6A ([3]) */
static void m2_pass(const gsb_in_t *in, uint8_t os)
{
    volatile gsb_m2_t *m = &B.m2;
    uint8_t cur, req;
    bool spd;

    if (os < 1u || os > 3u) {
        return;                     /* 0x14F34C runs in the 10 ms lists of modes 1..3 */
    }
    m->n++;
    m->live_os = os;
    /* +4 / +6 ← E90E / E910 (0x13BF66 holds E90E while 0x108 is invalid) */
    if (in->eng_valid != 0u && in->eng_speed != 0xFFFFu) {
        m->eng = in->eng_speed;
        m->eng_st = 0u;
    } else {
        m->eng_st = 2u;
    }
    m->tur = in->tur_rpm;
    m->tur_st = (uint8_t)((in->tur_ok != 0u) ? 0u : 1u);
    m->sup = LIVE_U16(L_SUPPLY);
    m->e9ff = LIVE_U8(L_E9FF);
    m->f584a = 1u;                  /* 0x14DC70 stand-in: no reaction 0x38, no overvoltage start */
    cur = m->app;                   /* 0x1418EC: +7 = +8 first */
    req = cur;
    /* engine: mov.w (sign-extended) cmp/hs 150 — u16 >= 150; turbine:
     * movu.w cmp/ge 100; E7EC / +6: tst */
    spd = ((uint32_t)(int32_t)(int16_t)m->eng >= M2_ENG_MIN && m->eng_st == 0u) ||
          ((uint32_t)m->tur >= M2_TUR_MIN && m->tur_st == 0u);
    m->spd_ok = spd ? 1u : 0u;
    if (cur == 1u) {
        if ((int8_t)m->e9ff == 2) {
            if (m->f584a == 0u) {
                req = 3u;
            } else if (spd && m->sup > M2_SUP_MIN && m->sup < M2_SUP_MAX) {
                req = 2u;
            }
        }
    } else if (cur == 2u) {
        if ((int8_t)m->e9ff == 4) {
            req = 1u;
        }
        if (m->f584a == 0u && (int8_t)m->e9ff == 2) {
            req = 3u;
        }
    } else if (cur == 3u) {
        if ((int8_t)m->e9ff == 4 || m->f584a == 1u) {
            req = 1u;
        }
    } else {
        req = 1u;
    }
    if (req != m->req) {
        m->chg++;
    }
    m->req = req;
    if (req == 2u) {
        m->n2++;
        if (m->first2 == 0u) {
            m->first2 = B.passes;
        }
    }
    /* +8 ← the request as if applied: 0x95483 → 0x68EFE → 0x93B5E → 0x6F0B4
     * → 0x66E06 → 0x68EA8 (one pass later in this model [I]) */
    m->app = req;
}

/* ── the slot (the recorder header) ──────────────────────────────────── */
static void slot_start(uint8_t pf)
{
    const uint8_t k = (pf == 2u) ? 1u : 0u;
    volatile gsb_slot_t *s = &B.slot[k];
    s->t0 = B.passes;
    s->arm_on = GSB_NONE;
    s->arm_off = GSB_NONE;
    s->off_why = 0u;
    s->done_at = GSB_NONE;
    s->back_at = GSB_NONE;
    s->armed_n = 0u;
    s->n = 0u;
    s->pf = pf;
    s->done_cause = 0u;
    s->guard_n = 0u;
    B.slot_cur = k;
}

static uint16_t since(const volatile gsb_slot_t *s)
{
    const uint32_t d = B.passes - s->t0;
    return (uint16_t)(d > 0xFFFEu ? 0xFFFEu : d);
}

/* ── G4b: the bridged release (gs_bridge.h) ───────────────────────────── */
#define GSB_D_X     GSB_SH_MUT_CH   /* the third element of 1st: C */
#define GSB_R_X     GSB_R_MOVE3     /* the third element of R: D */

static uint8_t rel_x(uint8_t pf) { return (pf == 2u) ? GSB_R_X : GSB_D_X; }

/* the release's first pass: the slot and the references */
static void rel_start(uint8_t pf, bool prev_applied)
{
    const uint8_t k = (pf == 2u) ? 1u : 0u;
    volatile gsbr_slot_t *rs = &R.slot[k];
    const uint8_t x = rel_x(pf);
    zero(rs, sizeof *rs);
    rs->t0 = B.passes;
    rs->end_t = GSB_NONE;
    rs->pf = pf;
    rs->xch = x;
    /* the applied value in the pass before (B.app still holds it); a fresh
     * arm (host negative only) has none: the loosest references */
    rs->bprev = prev_applied ? B.app[GSB_CH_B] : (uint16_t)GSB_IMAX;
    rs->xprev = prev_applied ? B.app[x] : (uint16_t)GSB_IMIN;
    rs->b0 = rs->bprev;                 /* the first armed pass sets them */
    rs->x0 = rs->xprev;
    zero(s_gsbr_est.est[k], sizeof s_gsbr_est.est[k]);
    R.on = 1u;
    R.pf = pf;
    R.t = 0u;
    R.rec = k;
    R.rec_tail = 0u;
    R.ev = (uint16_t)(R.ev + 1u);
}

static void rel_end(uint8_t code, uint16_t why)
{
    if (R.on != 0u && R.rec < 2u) {
        volatile gsbr_slot_t *rs = &R.slot[R.rec];
        rs->end_t = R.t;
        rs->end_code = code;
        rs->end_why = why;
        R.rec_tail = 3u;                /* record the P content after it */
    }
    R.on = 0u;
}

/* the release envelope; 0 = inside, else GSBR_T_* (ch / v of the trip) */
static uint8_t rel_envelope(uint8_t *tch, uint16_t *tv)
{
    volatile gsbr_slot_t *rs = &R.slot[(R.pf == 2u) ? 1u : 0u];
    const uint16_t *fix = (R.pf == 2u) ? GSB_FIX_R : GSB_FIX_D;
    const uint8_t move = (R.pf == 2u) ? GSB_R_MOVE : GSB_D_MOVE;
    const uint8_t x = rel_x(R.pf);
    const uint16_t b = B.sh[GSB_CH_B], xv = B.sh[x];
    for (uint8_t ch = 0u; ch < 5u; ch++) {
        const uint16_t v = B.sh[ch];
        *tch = ch;
        *tv = v;
        if (((move >> ch) & 1u) != 0u) {
            if (v < GSB_IMIN || v > GSB_IMAX ||
                (gs_shadow_reqmode(ch) == 1u && v != GSB_IMIN && v != GSB_IMAX)) {
                return GSBR_T_RANGE;
            }
        } else if (v != fix[ch]) {
            return GSBR_T_FIXED;
        }
    }
    if (R.t >= GSB_REL_PASSES) {
        *tch = 0xFFu;
        *tv = R.t;
        return GSBR_T_LEN;
    }
    if (R.t == 0u) {
        *tch = GSB_CH_B;
        *tv = b;
        if (b > rs->bprev) {
            return GSBR_T_B0;
        }
        *tch = x;
        *tv = xv;
        if (xv < rs->xprev) {
            return GSBR_T_X0;
        }
        rs->b0 = b;
        rs->x0 = xv;
        return 0u;
    }
    *tch = GSB_CH_B;
    *tv = b;
    if (b > rs->b0) {
        return GSBR_T_BUP;
    }
    *tch = x;
    *tv = xv;
    if (xv < rs->x0) {
        return GSBR_T_XDN;
    }
    return 0u;
}

void gs_bridge_tick_10ms(const gsb_in_t *in)
{
    const volatile gs_wit_t *w = gs_shadow_wit();
    const uint8_t os = LIVE_U8(L_OS_MODE);
    const uint8_t pf = gs_shadow_pf();
    const uint8_t dirbit = (pf == 1u) ? 1u : ((pf == 2u) ? 2u : 0u);
    const uint16_t *fix = (pf == 2u) ? GSB_FIX_R : GSB_FIX_D;
    const uint8_t move = (pf == 2u) ? GSB_R_MOVE : GSB_D_MOVE;
    const uint8_t rel = gs_shadow_releasing();
    volatile gsb_slot_t *s = 0;
    uint16_t why = 0u;
    bool commit_now = false, done_now = false, armed, applied, agree = true;
    const bool prev_applied = (B.applied != 0u);
    bool relmode = false;
    uint8_t adirbit = dirbit;           /* the direction the arm is evaluated for */

    m2_pass(in, os);

    /* 1. the shadow's commit / completion */
    if (w->commits != B.sh_commits) {
        B.sh_commits = w->commits;
        commit_now = true;
        if (w->commit_new != 0u) {
            if (W.commits < 0xFFFEu) {
                W.commits = (uint16_t)(W.commits + 1u);
                win_seal();
            }
            if (W.commits > GSB_COMMITS_MAX) {
                latch(GSB_L_COMMITS);
            }
            B.eng_on = 1u;
            B.eng_t = 0u;
            slot_start(w->commit_new);
        } else {
            B.eng_on = 0u;
        }
    }
    if (w->done_pass != B.sh_done) {
        B.sh_done = w->done_pass;
        done_now = true;
        B.eng_on = 0u;
        if (B.slot_cur < 2u && B.slot[B.slot_cur].done_at == GSB_NONE) {
            B.slot[B.slot_cur].done_at = since(&B.slot[B.slot_cur]);
            B.slot[B.slot_cur].done_cause = w->done_cause;
        }
    }
    if (B.eng_on != 0u) {
        if (B.eng_t < 0xFFFFu) {
            B.eng_t = (uint16_t)(B.eng_t + 1u);
        }
        if (B.eng_t > GSB_ENG_PASSES) {
            latch(GSB_L_ENGT);          /* 3.5 s without a completion: abort, latched */
            B.eng_on = 0u;
        }
    }
    if (B.slot_cur < 2u) {
        s = &B.slot[B.slot_cur];
    }

    /* G4b: the bridged release. A release starts in the shadow's
     * power-flow-0 commit pass (BD16 = 4 from that pass); it is bridged only
     * when the bridge was APPLIED in the pass before (continuity) and the
     * last nonzero commit's slot names its direction (= the shadow's
     * commit_old). It ends with the shadow's release (the completion pass:
     * the P content), with a nonzero commit, or at its first unarmed pass —
     * never re-armed. */
#if defined(GSB_HOST_NO_REL_BRIDGE) && !defined(__sh__)
    (void)prev_applied;                 /* host NEGATIVE: the r04k behaviour */
    (void)rel_start;
#else
    if (R.on != 0u && (rel == 0u || (commit_now && w->commit_new != 0u))) {
        rel_end(GSBR_E_DONE, 0u);
    }
    if (rel != 0u && R.rel_prev == 0u) {
        const uint8_t spf = (B.slot_cur < 2u) ? B.slot[B.slot_cur].pf : 0u;
#  if defined(GSB_HOST_NEG_REL_FRESH) && !defined(__sh__)
        const bool cont = true;         /* host NEGATIVE: no continuity rule */
#  else
        const bool cont = prev_applied;
#  endif
        if (cont && spf != 0u && commit_now && w->commit_new == 0u && w->commit_old == spf) {
            rel_start(spf, prev_applied);
        } else {
            R.refused = (uint16_t)(R.refused + 1u);
        }
    }
    relmode = (R.on != 0u) && (rel != 0u);
    if (relmode) {
        adirbit = (R.pf == 1u) ? 1u : 2u;
    }
#endif
    R.rel_prev = rel;

    /* the shadow's and the live producer's A..E. G4a (2026-11-19,
     * ~/tcu_sweep/g4a_release_port.md): the shadow now runs the stock's
     * 1.2 s RELEASE (BD16 = 4) after a power-flow-0 commit, which this
     * stage does NOT bridge — the live content in those passes is the P
     * content. So while the shadow releases, the agreement compares the
     * live producer against the P content (what HEAD's instant-release
     * shadow showed in the same passes), keeping the arm entry, the
     * back-to-P witness and the live path exactly as before; the release
     * never arms (RANGE / IDLE: power flow 0). */
    for (uint8_t ch = 0u; ch < 5u; ch++) {
        uint16_t v = gs_shadow_cur(ch);
        const uint16_t lv = LIVE_U16(L_PROD + 2u * ch);
#if !defined(__sh__)
        if (ch == s_inj_ch && B.passes >= s_inj_from && B.passes < s_inj_to) {
            v = s_inj_v;
        }
#endif
        B.sh[ch] = v;
        B.lv[ch] = (lv == (uint16_t)~LIVE_U16(L_PROD_C + 2u * ch)) ? lv : 0xFFFFu;
        if (((rel != 0u) ? GSB_PCONT[ch] : B.sh[ch]) != B.lv[ch]) {
            agree = false;
        }
    }

    /* 2. the arm conditions (plan §2.4; G4b: in a bridged release the
     * idle / range / park conditions are the release's) */
    if (!relmode && !(pf != 0u || gs_shadow_pending() != 0u || (w->now_flags & 0x08u) != 0u)) {
        why |= GSB_R_IDLE;
    }
    if ((GSB_EVAL_DIR & adirbit) == 0u) {
        why |= GSB_R_DIR;
    }
    if (LIVE_U8(L_SEQ2) != (uint8_t)'U' || LIVE_U8(L_LETTER) != (uint8_t)'c') {
        why |= GSB_R_RAIL;
    }
    if (os < 1u) {
        why |= GSB_R_OSMODE;
    }
    if (relmode) {
        /* the release: any range 5..8 (lever P, or N / R / D on the way
         * back); the park model is waived (the park outputs follow the
         * range chain on their own and drop in the lever-P pass) */
        if (PCELL_TGT < 5u || PCELL_TGT > 8u) {
            why |= GSB_R_RANGE;
        }
    } else {
        const uint8_t want = (pf == 1u) ? 5u : ((pf == 2u) ? 7u : 0u);
        if (want == 0u || PCELL_TGT != want) {
            why |= GSB_R_RANGE;
        }
        if (LIVE_U8(L_PARK_MODEL) != 1u) {
            why |= GSB_R_PARK;
        }
    }
    if (in->mon_faults != 0u) {
        why |= GSB_R_MON;
    }
    for (uint8_t ch = 0u; ch < 5u; ch++) {
        if (LIVE_U16(L_FW + 2u * ch) != 0u || LIVE_U8(L_CCLS + ch) != 0u || LIVE_U8(L_OST + ch) >= 2u) {
            why |= GSB_R_DIAG;
        }
    }
    if (in->relwin_st == 2u) {
        why |= GSB_R_RELWIN;
    }
    if (!win_ok()) {
        latch((uint8_t)(GSB_L_WIN | GSB_L_GUARD));    /* a torn window: fail safe */
    }
    if ((W.latch & GSB_L_WIN) != 0u || W.acc_ms >= GSB_WIN_MS) {
        why |= GSB_R_WIN;
    }
    if ((W.latch & GSB_L_ENGT) != 0u) {
        why |= GSB_R_ENGT;
    }
    if ((W.latch & GSB_L_COMMITS) != 0u) {
        why |= GSB_R_COMMITS;
    }
    if ((W.latch & GSB_L_GUARD) != 0u) {
        why |= GSB_G_LATCH;
    }
    if (B.armed == 0u && B.agree_run < GSB_AGREE_N) {
        why |= GSB_R_AGREE;             /* the entry condition (first armed pass only) */
    }

    /* 3a. G4b: the release envelope (only where the bridge would arm) */
    if (why == 0u && relmode) {
        uint8_t tch = 0xFFu;
        uint16_t tv = 0u;
        const uint8_t code = rel_envelope(&tch, &tv);
        if (code != 0u) {
            const uint16_t g = (code == GSBR_T_FIXED) ? GSB_G_FIXED : GSB_G_RANGE;
            latch(GSB_L_GUARD);
            B.guard_n = (uint16_t)(B.guard_n + 1u);
            B.guard_why = g;
            B.guard_ch = tch;
            B.guard_v = tv;
            R.trip_n = (uint16_t)(R.trip_n + 1u);
            R.trip_code = code;
            R.trip_ch = tch;
            R.trip_v = tv;
            why = (uint16_t)(why | g | GSB_G_LATCH);
            rel_end((uint8_t)(0x10u + code), why);
        }
    }

    /* 3. the compile-time element bounds (only where the bridge would arm) */
    if (why == 0u && !relmode) {
        uint16_t g = 0u;
        uint8_t gch = 0xFFu;
        uint16_t gv = 0u;
        for (uint8_t ch = 0u; ch < 5u; ch++) {
            const uint16_t v = B.sh[ch];
            uint16_t t = 0u;
            if (((move >> ch) & 1u) != 0u) {
                if (v < GSB_IMIN || v > GSB_IMAX) {
                    t = GSB_G_RANGE;
                } else if (gs_shadow_reqmode(ch) == 1u && v != GSB_IMIN && v != GSB_IMAX) {
                    t = GSB_G_RANGE;    /* a mode-1 pass is exactly 500 or 8500 */
                }
            } else if (v != fix[ch]) {
                t = GSB_G_FIXED;
            }
            if (t != 0u && g == 0u) {
                gch = ch;
                gv = v;
            }
            g = (uint16_t)(g | t);
        }
        if (g != 0u) {
            latch(GSB_L_GUARD);
            B.guard_n = (uint16_t)(B.guard_n + 1u);
            B.guard_why = g;
            B.guard_ch = gch;
            B.guard_v = gv;
            if (s != 0) {
                s->guard_n = (uint8_t)(s->guard_n + 1u);
            }
            why = (uint16_t)(why | g | GSB_G_LATCH);
        }
    }

    /* 4. would-arm; the armed-time budget */
    armed = (why == 0u);
    applied = armed && (((uint8_t)GS_LIVE_DIR & adirbit) != 0u);
    if (relmode && R.on != 0u && !applied) {
#if !(defined(GSB_HOST_NEG_REL_FRESH) && !defined(__sh__))
        rel_end(GSBR_E_WHY, why);       /* an unarmed release pass: never re-armed */
#endif
    }
    if (relmode && applied) {
        R.n++;
    }
    if (armed) {
        B.armed_n++;
        W.acc_ms += 10u;
        win_seal();
        if (W.acc_ms >= GSB_WIN_MS) {
            latch(GSB_L_WIN);           /* the budget ends after this pass */
        }
        if (s != 0 && s->armed_n < 0xFFFFu) {
            s->armed_n = (uint16_t)(s->armed_n + 1u);
        }
        B.agree_run = 0u;               /* a re-arm needs 10 fresh agreeing passes */
    } else if (agree) {
        B.agree_n++;
        if (B.agree_run < 0xFFu) {
            B.agree_run = (uint8_t)(B.agree_run + 1u);
        }
    } else {
        B.disagree_n++;
        B.last_dis = B.passes;
        B.agree_run = 0u;
    }
    if (applied) {
        B.applied_n++;
    }
    if (armed && B.armed == 0u) {
        B.arm_ev = (uint16_t)(B.arm_ev + 1u);
        if (s != 0 && s->arm_on == GSB_NONE) {
            s->arm_on = since(s);
        }
    }
    if (!armed && B.armed != 0u) {
        B.disarm_ev = (uint16_t)(B.disarm_ev + 1u);
        B.why_armed_or = (uint16_t)(B.why_armed_or | why);
        if (s != 0 && s->arm_off == GSB_NONE) {
            s->arm_off = since(s);
            s->off_why = why;
        }
        B.back_pend = 1u;
    }
    if (B.back_pend != 0u && !armed && agree) {
        B.back_pend = 0u;
        if (s != 0 && s->back_at == GSB_NONE) {
            s->back_at = since(s);
        }
    }
    B.armed = armed ? 1u : 0u;
    B.applied = applied ? 1u : 0u;
    B.pf = pf;
    B.why = why;

    /* 5. the would-be currents and words; the recorder */
    for (uint8_t ch = 0u; ch < 5u; ch++) {
        B.cur[ch] = armed ? B.sh[ch] : B.lv[ch];
        B.app[ch] = applied ? B.sh[ch] : 0u;
    }
    if (s != 0 && s->n < GSB_REC_N) {
        volatile gsb_rec_t *r = rec_at(B.slot_cur, s->n);
        for (uint8_t k = 0u; k < 3u; k++) {
            r->ph[k] = gs_shadow_phase(k);
        }
        r->st = (uint8_t)((armed ? 0x01u : 0u) | (applied ? 0x02u : 0u) |
                          (((W.latch & GSB_L_GUARD) != 0u) ? 0x04u : 0u) | (agree ? 0x08u : 0u) |
                          (uint8_t)((pf & 3u) << 4) | (commit_now ? 0x40u : 0u) | (done_now ? 0x80u : 0u));
        r->why = why;
        for (uint8_t ch = 0u; ch < 5u; ch++) {
            const uint16_t lw = (in->cyc != 0) ? in->cyc[cyc_idx(ch)] : 0u;
            r->live[ch] = lw;
            r->wb[ch] = word_of(ch, lw, B.cur[ch]);
            r->est[ch] = LIVE_U16(L_EST + 2u * ch);
        }
        s->n = (uint8_t)(s->n + 1u);
    }
    /* G4b: the bridged-release recorder (entry = release pass; then up to 3
     * passes after its end, to show the P content reach the wire) */
    if (R.rec < 2u) {
        volatile gsbr_slot_t *rs = &R.slot[R.rec];
        const uint8_t x = rs->xch;
        if (rs->n < GSBR_REC_N) {
            const uint8_t i = rs->n;
            const uint16_t eb = LIVE_U16(L_EST + 2u * GSB_CH_B), ex = LIVE_U16(L_EST + 2u * x);
            if (applied) {
                rs->bm[i >> 3] = (uint8_t)(rs->bm[i >> 3] | (uint8_t)(1u << (i & 7u)));
            }
            rs->live[i][0] = (in->cyc != 0) ? in->cyc[cyc_idx(GSB_CH_B)] : 0u;
            rs->live[i][1] = (in->cyc != 0) ? in->cyc[cyc_idx(x)] : 0u;
            s_gsbr_est.est[R.rec][i][0] = (uint8_t)((eb >= 10180u) ? 255u : (eb + 20u) / 40u);
            s_gsbr_est.est[R.rec][i][1] = (uint8_t)((ex >= 10180u) ? 255u : (ex + 20u) / 40u);
            rs->n = (uint8_t)(i + 1u);
            if (R.on != 0u && applied) {
                rs->len = (uint16_t)(rs->len + 1u);
            }
        }
        if (R.on == 0u) {
            if (R.rec_tail != 0u && rs->n < GSBR_REC_N) {
                R.rec_tail = (uint8_t)(R.rec_tail - 1u);
            } else {
                R.rec = 0xFFu;
            }
        }
    }
    if (R.on != 0u && R.t < 0xFFFFu) {
        R.t = (uint16_t)(R.t + 1u);
    }
    B.passes++;
}

const volatile gsb_t     *gs_bridge_state(void) { return &s_gsb; }
const volatile gsb_win_t *gs_bridge_win(void)   { return &s_gsb_win; }
const volatile gsbr_t     *gs_bridge_rel(void)     { return &s_gsbr; }
const volatile gsbr_est_t *gs_bridge_rel_est(void) { return &s_gsbr_est; }
const volatile gsb_rec_t *gs_bridge_rec(uint8_t slot, uint8_t i)
{
    return (slot < 2u && i < GSB_REC_N) ? rec_at(slot, i) : 0;
}

bool gs_bridge_table(uint8_t pf, uint8_t *move_mask, uint16_t fixed[5])
{
    const uint16_t *f;
    if (pf == 1u) {
        *move_mask = GSB_D_MOVE;
        f = GSB_FIX_D;
    } else if (pf == 2u) {
        *move_mask = GSB_R_MOVE;
        f = GSB_FIX_R;
    } else {
        return false;
    }
    for (uint8_t ch = 0u; ch < 5u; ch++) {
        fixed[ch] = f[ch];
    }
    return true;
}

#if GS_LIVE_DIR != 0
/* THE SEAM (drv_gsbr.h; host G2 / G3 dry-run builds only in G1) */
bool drv_gsbr_take(uint8_t ch, uint16_t *v)
{
    if (ch < 5u && B.applied != 0u) {
        *v = B.app[ch];
        B.take_n++;
        return true;
    }
    return false;
}
#endif

#endif /* !TCU_RAM_APP */
