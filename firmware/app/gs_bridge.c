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
#if defined(GSB_HOST_MUT_TABLE) && !defined(__sh__)
#  define GSB_PIN_D_E 500u          /* host negative: E pinned at 500 in D */
#else
#  define GSB_PIN_D_E GSB_PIN
#endif
static const uint16_t GSB_FIX_D[5] = { GSB_PIN, GSB_PIN, 0u, GSB_PIN_D_E, 0u };
static const uint16_t GSB_FIX_R[5] = { GSB_PIN, 0u, 0u, GSB_PIN, GSB_PIN };
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
#else
#  define GSB_WIN
#  define GSB_ST
#  define GSB_RC0
#  define GSB_RC1
#endif
static volatile gsb_win_t s_gsb_win GSB_WIN;    /* NOT cleared on a warm boot */
static volatile gsb_t     s_gsb     GSB_ST;
static volatile gsb_rec_t s_gsb_rec0[GSB_REC_N] GSB_RC0;   /* the last D */
static volatile gsb_rec_t s_gsb_rec1[GSB_REC_N] GSB_RC1;   /* the last R */
#define B (s_gsb)
#define W (s_gsb_win)

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

void gs_bridge_tick_10ms(const gsb_in_t *in)
{
    const volatile gs_wit_t *w = gs_shadow_wit();
    const uint8_t os = LIVE_U8(L_OS_MODE);
    const uint8_t pf = gs_shadow_pf();
    const uint8_t dirbit = (pf == 1u) ? 1u : ((pf == 2u) ? 2u : 0u);
    const uint16_t *fix = (pf == 2u) ? GSB_FIX_R : GSB_FIX_D;
    const uint8_t move = (pf == 2u) ? GSB_R_MOVE : GSB_D_MOVE;
    volatile gsb_slot_t *s = 0;
    uint16_t why = 0u;
    bool commit_now = false, done_now = false, armed, applied, agree = true;

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

    /* the shadow's and the live producer's A..E */
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
        if (B.sh[ch] != B.lv[ch]) {
            agree = false;
        }
    }

    /* 2. the arm conditions (plan §2.4) */
    if (!(pf != 0u || gs_shadow_pending() != 0u || (w->now_flags & 0x08u) != 0u)) {
        why |= GSB_R_IDLE;
    }
    if ((GSB_EVAL_DIR & dirbit) == 0u) {
        why |= GSB_R_DIR;
    }
    if (LIVE_U8(L_SEQ2) != (uint8_t)'U' || LIVE_U8(L_LETTER) != (uint8_t)'c') {
        why |= GSB_R_RAIL;
    }
    if (os < 1u) {
        why |= GSB_R_OSMODE;
    }
    {
        const uint8_t want = (pf == 1u) ? 5u : ((pf == 2u) ? 7u : 0u);
        if (want == 0u || PCELL_TGT != want) {
            why |= GSB_R_RANGE;
        }
    }
    if (LIVE_U8(L_PARK_MODEL) != 1u) {
        why |= GSB_R_PARK;
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

    /* 3. the compile-time element bounds (only where the bridge would arm) */
    if (why == 0u) {
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
    applied = armed && (((uint8_t)GS_LIVE_DIR & dirbit) != 0u);
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
    B.passes++;
}

const volatile gsb_t     *gs_bridge_state(void) { return &s_gsb; }
const volatile gsb_win_t *gs_bridge_win(void)   { return &s_gsb_win; }
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
