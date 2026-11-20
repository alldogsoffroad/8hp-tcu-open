/* G6c (2026-11-19, ~/tcu_sweep/g6c_port.md): THE MONITOR ONLY. The r04n
 * bridge (the shadow's currents armed per pass: bridge_tick, its agreement
 * entry and fresh-arm rules) is deleted with the shadow (D-G6-7); every
 * monitor check stays, unchanged in value (D-G6-6), except "candidate !=
 * shadow" (GSB_L_SHADOW), whose reference was the shadow. The recorders
 * (.gsb_rec0 / .gsb_rec1) moved off the stock cells into the deleted
 * shadow's RAM (linker.ld). GS_G6B 0 now means: no seam, no monitor.
 * The text below is the history (G1 .. G6b).
 * gs_bridge.c — THE GARAGE BRIDGE, STAGE G1: computed, recorded, NOT
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
#if GS_G6B && !defined(TCU_RAM_APP)
#include "app/gs_cand.h"          /* G6b: the candidate (instance 2), the monitored source */
#endif

#if defined(TCU_RAM_APP)

void gs_bridge_init(bool cold) { (void)cold; }
void gs_bridge_tick_10ms(const gsb_in_t *in) { (void)in; }

#else

#if (GS_LIVE_DIR != 0) && (GS_TARGET_1ST == 0)
#  error "gs bridge: a live bridge needs the forced garage shadow (GS_FORCED / GS_TARGET_1ST 1): the gear-3 variant is a host reference"
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
/* G6b: the output path's P content (bsw/drv/drv_gsbr.h, rq_prod_guarded's
 * fallback and the monitor's latched content) is this one */
_Static_assert(DRV_RQ_PCONT_A == GSB_PIN && DRV_RQ_PCONT_D == GSB_PIN && DRV_RQ_PCONT_B == GSB_IMIN_P &&
               DRV_RQ_PCONT_E == GSB_PIN && DRV_RQ_PCONT_C == GSB_PIN && DRV_RQ_PCONT_WK == 500u &&
               DRV_RQ_PCONT_SYS == 500u, "gs bridge: drv_gsbr.h's P content is the bridge's GSB_PCONT");
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

/* G6b host tests: the monitor's windows lowered at run time (the negatives);
 * on SH the plan's constants, nothing else */
#if GS_G6B && !defined(__sh__)
static uint16_t s_h_eng = GSB_ENG_PASSES, s_h_rel = GSB_REL_PASSES, s_h_commits = GSB_COMMITS_MAX;
static uint32_t s_h_win = GSB_WIN_MS;
void gs_bridge_host_limits(uint16_t eng, uint16_t rel, uint32_t win_ms, uint16_t commits)
{
    s_h_eng = eng;
    s_h_rel = rel;
    s_h_win = win_ms;
    s_h_commits = commits;
}
static uint16_t s_h_boot = GSB_BOOT_PASSES;
void gs_bridge_host_boot_limit(uint16_t passes)
{
    s_h_boot = passes;
}
#  define MON_ENG_PASSES  s_h_eng
#  define MON_REL_PASSES  s_h_rel
#  define MON_WIN_MS      s_h_win
#  define MON_COMMITS_MAX s_h_commits
#  define MON_BOOT_PASSES s_h_boot
#else
#  define MON_BOOT_PASSES GSB_BOOT_PASSES
#  define MON_ENG_PASSES  GSB_ENG_PASSES
#  define MON_REL_PASSES  GSB_REL_PASSES
#  define MON_WIN_MS      GSB_WIN_MS
#  define MON_COMMITS_MAX GSB_COMMITS_MAX
#endif
#if defined(GSM_HOST_BYPASS) && defined(__sh__)
#  error "gs bridge: the monitor bypass is a host-only negative test"
#endif

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
/* r04q host negatives: the candidate's E28E state and A's request mode /
 * fill state, as the monitor reads them, on bridge passes [from, to) */
static uint8_t  s_injb_e28e = 0xFFu, s_injb_am = 0xFFu;
static uint32_t s_injb_from, s_injb_to;
void gs_bridge_host_inject_boot(uint8_t e28e, uint8_t amode, uint32_t from, uint32_t to)
{
    s_injb_e28e = e28e;
    s_injb_am = amode;
    s_injb_from = from;
    s_injb_to = to;
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
#if GS_G6B
    /* r04q (D-G6-13): no boot fence any more — before the candidate's boot
     * pass the seam is empty (GSM_S_LIVE: drv_reqlaw.c's r03f table, D-G6-15),
     * from it the boot allowance (gs_bridge_boot_pass) */
    B.fence = 0u;
    B.fence_ev = 0u;
    B.boot_st = GSM_B_NONE;
    B.trip_pass = 0xFFFFFFFFu;
    B.trip_ch = 0xFFu;
#endif
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

#if GS_G6B
/* G6c (2026-11-19, ~/tcu_sweep/g6c_port.md): everything below is the
 * MONITOR's; GS_G6B 0 (a host build without the seam) compiles none of it */
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
/* G5b (2026-11-19, ~/tcu_sweep/g5b_port.md, D-G5-8): the live selector is
 * safety/os_sel.c; this stays an INDEPENDENT, counted cross-check of it (no
 * latch, no output). With GS_M2_XCHECK (the code-flash application and the
 * G5b host tests — the builds that link os_sel.c) it models the stock's
 * inputs as the live path sees them: +8 = the applied OS mode (0x68EA8's
 * echo of 0x848CF at the task head), E9FF = the PREVIOUS pass's (0x1418E2
 * runs before 0x140B80; this pass's kl15 already ran when the monitor runs),
 * and counts the passes whose request differs from the live 0x95483
 * (os_sel.h OS_SEL_OBJ +7, read as a const volatile literal) in m2.pad. */
#if !defined(GS_M2_XCHECK)
#  define GS_M2_XCHECK 0
#endif
#define L_OSSEL_REQ  0xFFF95F83u    /* u8 the live request 0x95483 (os_sel.h OS_SEL_OBJ + 7) */
static void m2_pass(const gsb_in_t *in, uint8_t os)
{
    volatile gsb_m2_t *m = &B.m2;
    uint8_t cur, req;
    bool spd;
#if GS_M2_XCHECK
    const uint8_t e9ff_prev = m->e9ff;

    m->e9ff = LIVE_U8(L_E9FF);      /* sampled every pass (mode 0 too): next pass's "previous" */
#endif

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
    m->f584a = 1u;                  /* 0x14DC70 stand-in: no reaction 0x38, no overvoltage start */
#if GS_M2_XCHECK
    m->app = os;                    /* +8: the applied mode (the live echo of 0x848CF) */
    cur = os;
    const uint8_t e9 = e9ff_prev;
#else
    m->e9ff = LIVE_U8(L_E9FF);
    cur = m->app;                   /* 0x1418EC: +7 = +8 first */
    const uint8_t e9 = m->e9ff;
#endif
    req = cur;
    /* engine: mov.w (sign-extended) cmp/hs 150 — u16 >= 150; turbine:
     * movu.w cmp/ge 100; E7EC / +6: tst */
    spd = ((uint32_t)(int32_t)(int16_t)m->eng >= M2_ENG_MIN && m->eng_st == 0u) ||
          ((uint32_t)m->tur >= M2_TUR_MIN && m->tur_st == 0u);
    m->spd_ok = spd ? 1u : 0u;
    if (cur == 1u) {
        if ((int8_t)e9 == 2) {
            if (m->f584a == 0u) {
                req = 3u;
            } else if (spd && m->sup > M2_SUP_MIN && m->sup < M2_SUP_MAX) {
                req = 2u;
            }
        }
    } else if (cur == 2u) {
        if ((int8_t)e9 == 4) {
            req = 1u;
        }
        if (m->f584a == 0u && (int8_t)e9 == 2) {
            req = 3u;
        }
    } else if (cur == 3u) {
        if ((int8_t)e9 == 4 || m->f584a == 1u) {
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
#if GS_M2_XCHECK
    /* the cross-check: this pass's request against the live selector's */
    if (req != LIVE_U8(L_OSSEL_REQ) && m->pad < 0xFFFFu) {
        m->pad = (uint16_t)(m->pad + 1u);
    }
#else
    /* +8 ← the request as if applied: 0x95483 → 0x68EFE → 0x93B5E → 0x6F0B4
     * → 0x66E06 → 0x68EA8 (one pass later in this model [I]) */
    m->app = req;
#endif
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
static uint8_t rel_envelope(uint8_t *tch, uint16_t *tv, uint8_t (*reqmode)(uint8_t))
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
                (reqmode(ch) == 1u && v != GSB_IMIN && v != GSB_IMAX)) {
                return GSBR_T_RANGE;
            }
        } else if (v != fix[ch]) {
            return GSBR_T_FIXED;
        }
    }
    if (R.t >= MON_REL_PASSES) {
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

/* ══ G6b: THE MONITOR (ours, not stock; ~/tcu_sweep/g6b_port.md) ══════════
 * The candidate (app/gs_cand.c, instance 2 of the garage engine template:
 * the stock engagement engine + holder in the stock 10 ms slot order, on
 * its own RAM .gsc_req / .gsc_obj) is THE PRODUCER of the live A..E
 * currents: every pass the seam (drv_gsbr_take, drv_reqlaw.c
 * rq_prod_guarded) carries its currents — unless
 *   (a) r04q: THE BOOT FENCE IS LIFTED (D-G6-13): the boot transient is
 *       carried, bounded by the once-per-boot BOOT ALLOWANCE (mon_tick 3a,
 *       gs_bridge_boot_pass); THE STALL FENCE (D-G6-14) stays, its source
 *       the constant P content (GSM_S_FENCE), no longer the live producer.
 *       The r04p text of (a), for the record:
 *       THE E28E FENCE (D-G6-4, D-G6-12): the candidate's exception machine
 *       E28E != 0 while its engine owns nothing — the stock's boot transient
 *       (E28E starts at 1, 0xD0C5A: every element released 2..3 passes, then
 *       A refills) and the all-release on an engine stop (0xD0C94 with
 *       0x89D12) — hands the seam back to the live producer (app/rq_prod.c,
 *       today's content: its own boot fill, then the P content). The fence
 *       lifts only when E28E is 0, the candidate's content IS the P content
 *       and the live producer's is too (no element filling): never onto an
 *       engaged state, never with a step on the wire. Not a latch.
 *   (b) A VIOLATION: every r04n bridge check, now a permanent monitor (D-G6-6),
 *       evaluated on every pass whose candidate content leaves the P content
 *       (non-P: a current off the P table, a request mode != 1, an element
 *       filling):
 *         - the candidate's engine owns nothing (non-P while idle) — GSB_L_IDLE;
 *         - a permit fails (rail 'U' + letter 'c', OS mode >= 1, live D6D6 =
 *           the engaged direction (a release: 5..8), live park model released
 *           (engage only), park_mon clear, no A..E diagnosis fault, the
 *           park-release window not latched, the direction known) — GSB_L_PERMIT;
 *         - the compile-time element bounds (engage) / the release envelope
 *           (release, incl. its 125-pass limit) — GSB_L_GUARD;
 *         - the windows: the 30 s armed budget, 3.5 s per engagement, 16
 *           commits — GSB_L_WIN / ENGT / COMMITS (as the bridge);
 *         - candidate != shadow (A..E currents) on 2 owned non-P passes
 *           running (plan G6b item 4; the shadow is common-mode: this
 *           catches a binding error or a foreign writer, not the logic) —
 *           GSB_L_SHADOW.
 *       Any violation → THE P CONTENT on the wire in the trip pass, latched
 *       in .noinit until power-off (a warm reset carries it; the candidate
 *       keeps running underneath, read-only from then on — the wire never
 *       returns to it).
 * r04r (~/tcu_sweep/r04r_port.md): THE LIVE HOLDER IS RETIRED — the engine
 * is the sole producer; the seam (drv_gsbr_take) also hands WK / Sys
 * (ch 5 / 6: the candidate's own current, the P content's 500 while latched /
 * fenced) and its first value (gs_bridge_boot_pass) starts the producer
 * content on the wire. Nothing here reads the holder's cells any more.
 * A non-P pass that passes every check, and every P-content pass, carries the
 * candidate. Passes whose content IS the P content need no permit: the wire
 * would be the P content either way. */

static uint8_t cand_cur_inj(uint8_t ch, uint16_t *v)
{
    *v = gs_cand_cur(ch);
#if !defined(__sh__)
    if (ch == s_inj_ch && B.passes >= s_inj_from && B.passes < s_inj_to) {
        *v = s_inj_v;
        return 1u;
    }
#endif
    return 0u;
}

static void mon_trip(uint8_t lbit, uint16_t why, uint8_t ch, uint16_t v)
{
    latch(lbit);
    if (B.trip_n == 0u) {
        B.trip_why = why;
        B.trip_ch = ch;
        B.trip_v = v;
        B.trip_pass = B.passes;
    }
    if (B.trip_n < 0xFFFFu) {
        B.trip_n = (uint16_t)(B.trip_n + 1u);
    }
}

/* r04q (D-G6-13): which allowed boot row the candidate's A..E content is
 * (gear target 1st: the target's first element is A, ch 0):
 *   GSM_B_REL  — the all-release row of 0xD0C94 code 2 (table 0x6314C): A
 *                500 (mode 1, not filling), D / B / E / C at the P content
 *                (mode 1, not filling), with E28E in 1..3 (the boot states);
 *   GSM_B_FILL — A's switch-on per the stock law: E28E 0, A in [500, 8500]
 *                in the curve law (mode 0) or a fill state (1 / 2), every
 *                other channel at the P content (mode 1, not filling);
 *   0          — neither */
#if !defined(__sh__)
static bool injb_on(void)
{
    return B.passes >= s_injb_from && B.passes < s_injb_to;
}
#endif
static uint8_t mon_e28e(const volatile gs_wit_t *w)
{
#if !defined(__sh__)
    if (injb_on() && s_injb_e28e != 0xFFu) {
        return s_injb_e28e;
    }
#endif
    return w->e28e;
}
static uint8_t boot_row(uint8_t e28e)
{
    bool others = true;
    uint16_t a;
    uint8_t am, ast;
    for (uint8_t ch = 1u; ch < 5u; ch++) {
        uint16_t v;
        (void)cand_cur_inj(ch, &v);
        const uint8_t m = gs_cand_reqmode(ch), st = gs_cand_fillst(ch);
        if (v != GSB_PCONT[ch] || m != 1u || st == 1u || st == 2u) {
            others = false;
        }
    }
    (void)cand_cur_inj(0u, &a);
    am = gs_cand_reqmode(0u);
    ast = gs_cand_fillst(0u);
#if !defined(__sh__)
    if (injb_on() && s_injb_am != 0xFFu) {
        am = s_injb_am;
        ast = 0u;
    }
#endif
    if (!others) {
        return 0u;
    }
    if (e28e >= 1u && e28e <= 3u && a == GSB_IMIN && am == 1u && ast != 1u && ast != 2u) {
        return GSM_B_REL;
    }
    if (e28e == 0u && a >= GSB_IMIN && a <= GSB_IMAX && (am == 0u || ast == 1u || ast == 2u)) {
        return GSM_B_FILL;
    }
    return 0u;
}

void gs_bridge_boot_pass(void)
{
#if GS_G6B
    static const uint16_t PC7[7] = DRV_RQ_PCONT_TABLE;
    const volatile gs_wit_t *w = gs_cand_wit();
    uint8_t v = 0u;

    B.boot_calls = (uint8_t)(B.boot_calls < 0xFFu ? B.boot_calls + 1u : 0xFFu);
    if (B.boot_st != GSM_B_NONE) {
        v = GSM_BV_TWICE;                           /* once per MCU start */
    } else if (w->passes == 0u || boot_row(mon_e28e(w)) != GSM_B_REL) {
        v = GSM_BV_ENTRY;
    }
    B.boot_p0 = B.passes;
    B.boot_len = 1u;
    if (W.latch != 0u) {
        B.src = GSM_S_PCONT;                        /* a warm reset's carried latch */
        if (B.boot_st == GSM_B_NONE) {
            B.boot_st = GSM_B_USED;
        }
    } else if (v != 0u) {
        B.boot_st = (uint8_t)(GSM_B_VIOL | v);
        mon_trip(GSB_L_IDLE, (uint16_t)(GSB_R_IDLE | GSB_G_FIXED), 0xFFu, v);
        B.src = GSM_S_PCONT;
    } else {
        B.boot_st = GSM_B_REL;
        B.src = GSM_S_CAND;
    }
    B.applied = 1u;
    for (uint8_t ch = 0u; ch < 5u; ch++) {
        uint16_t c;
        (void)cand_cur_inj(ch, &c);
        B.sh[ch] = c;
        B.app[ch] = (B.src == GSM_S_CAND) ? c : PC7[ch];
        B.cur[ch] = B.app[ch];
    }
#endif
}

static void mon_tick(const gsb_in_t *in)
{
    static const uint16_t PC7[7] = DRV_RQ_PCONT_TABLE;
    const volatile gs_wit_t *w = gs_cand_wit();
    const uint8_t os = LIVE_U8(L_OS_MODE);
    const uint8_t pf = gs_cand_pf();
    const uint8_t dirbit = (pf == 1u) ? 1u : ((pf == 2u) ? 2u : 0u);
    const uint16_t *fix = (pf == 2u) ? GSB_FIX_R : GSB_FIX_D;
    const uint8_t move = (pf == 2u) ? GSB_R_MOVE : GSB_D_MOVE;
    const uint8_t rel = gs_cand_releasing();
    const bool owns = pf != 0u || rel != 0u || gs_cand_pending() != 0u || (w->now_flags & 0x08u) != 0u;
    volatile gsb_slot_t *s = 0;
    uint16_t why = 0u;
    bool commit_now = false, done_now = false, armed, agree = true, nonp = false;
    const bool booted = w->passes != 0u;            /* r04q: the candidate's boot pass ran */
    uint8_t bootv = 0u;                             /* r04q: a boot-allowance violation (GSM_BV_*) */
    const bool prev_cand = (B.src == GSM_S_CAND) && (B.passes != 0u);
    bool relmode = false;
    uint8_t adirbit = dirbit, src;

    m2_pass(in, os);

    /* 1. the candidate's commit / completion (the bridge's rule) */
    if (w->commits != B.sh_commits) {
        B.sh_commits = w->commits;
        commit_now = true;
        if (w->commit_new != 0u) {
            if (W.commits < 0xFFFEu) {
                W.commits = (uint16_t)(W.commits + 1u);
                win_seal();
            }
            if (W.commits > MON_COMMITS_MAX) {
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
        if (B.eng_t > MON_ENG_PASSES) {
            latch(GSB_L_ENGT);          /* 3.5 s without a completion: latched */
            B.eng_on = 0u;
        }
    }
    if (B.slot_cur < 2u) {
        s = &B.slot[B.slot_cur];
    }

    /* the release (G4b's rule on the candidate): from its power-flow-0
     * commit pass, after a pass the seam carried the candidate, in the last
     * nonzero commit's direction; it ends with the candidate's release
     * (the completion pass is the P content), a nonzero commit, or the first
     * pass the seam does not carry the candidate */
    if (R.on != 0u && (rel == 0u || (commit_now && w->commit_new != 0u))) {
        rel_end(GSBR_E_DONE, 0u);
    }
    if (rel != 0u && R.rel_prev == 0u) {
        const uint8_t spf = (B.slot_cur < 2u) ? B.slot[B.slot_cur].pf : 0u;
        if (prev_cand && spf != 0u && commit_now && w->commit_new == 0u && w->commit_old == spf) {
            rel_start(spf, true);
        } else {
            R.refused = (uint16_t)(R.refused + 1u);
        }
    }
    relmode = (R.on != 0u) && (rel != 0u);
    if (relmode) {
        adirbit = (R.pf == 1u) ? 1u : 2u;
    }
    R.rel_prev = rel;

    /* the candidate's A..E and the reference content. r04r: the live holder
     * (app/rq_prod.c, 0xFFF84B78) is retired — gsb_t.lv, the agreement
     * witness's reference, is now the content the seam falls back to: the
     * constant P content once the candidate's boot pass ran, 0 before it
     * (the retired holder's cleared array read 0 there). Witness only:
     * agree / disagree / back_at; no decision reads it. */
    for (uint8_t ch = 0u; ch < 5u; ch++) {
        uint16_t v;
        (void)cand_cur_inj(ch, &v);
        B.sh[ch] = v;
        B.lv[ch] = booted ? GSB_PCONT[ch] : 0u;
        if (v != B.lv[ch]) {
            agree = false;
        }
        {
            const uint8_t m = gs_cand_reqmode(ch), st = gs_cand_fillst(ch);
            if (v != GSB_PCONT[ch] || m != 1u || st == 1u || st == 2u) {
                nonp = true;
            }
        }
    }
    B.owns = owns ? 1u : 0u;
    B.nonp = nonp ? 1u : 0u;

    /* 2. the permits (the bridge's arm conditions; the AGREE entry is gone) */
    if (!relmode && !owns) {
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
    if (W.acc_ms >= MON_WIN_MS) {
        latch(GSB_L_WIN);
    }
    if ((W.latch & GSB_L_WIN) != 0u) {
        why |= GSB_R_WIN;
    }
    if ((W.latch & GSB_L_ENGT) != 0u) {
        why |= GSB_R_ENGT;
    }
    if ((W.latch & GSB_L_COMMITS) != 0u) {
        why |= GSB_R_COMMITS;
    }
    if ((W.latch & (uint8_t)~(GSB_L_WIN | GSB_L_ENGT | GSB_L_COMMITS)) != 0u) {
        why |= GSB_G_LATCH;
    }

    /* 3a. r04q (D-G6-13): THE BOOT ALLOWANCE (ours) — opened by the boot
     * entry (gs_bridge_boot_pass); while open, non-P content is allowed only
     * as the stock's E28E boot sequence: the all-release row (E28E 1..3),
     * then the gear-target element's switch-on fill (E28E 0, A only, every
     * other channel at the P content), within MON_BOOT_PASSES evaluations
     * of the entry; it ends (for good) on the first P-content pass with
     * E28E 0. Anything else while open → a violation (GSB_L_IDLE, latched) */
    if (B.boot_st == GSM_B_REL || B.boot_st == GSM_B_FILL) {
        if (B.boot_len < 0xFFFFu) {
            B.boot_len = (uint16_t)(B.boot_len + 1u);
        }
        if (owns) {
            B.boot_st = GSM_B_USED;                 /* the engine owns content: the normal checks */
        } else if (!nonp && mon_e28e(w) == 0u) {
            B.boot_st = GSM_B_USED;                 /* ended on the P content */
        } else if (B.boot_len > MON_BOOT_PASSES) {
            bootv = GSM_BV_LEN;
        } else {
            const uint8_t row = boot_row(mon_e28e(w));
            if (row == GSM_B_REL && B.boot_st == GSM_B_REL) {
                /* the all-release row, before the fill */
            } else if (row == GSM_B_FILL) {
                B.boot_st = GSM_B_FILL;
            } else {
                bootv = (row == GSM_B_REL) ? GSM_BV_ORDER : GSM_BV_ROW;
            }
        }
    }

    /* 3b. the E28E stall fence (D-G6-12 / r04q D-G6-14): on while E28E != 0
     * with nothing owned (outside the boot allowance); it hands the seam THE
     * CONSTANT P CONTENT (no longer the live producer); off once E28E is 0
     * and the candidate's content IS the P content */
    if (B.fence != 0u) {
        if (w->e28e == 0u && !nonp) {
            B.fence = 0u;
            B.unfence_n = (uint16_t)(B.unfence_n + 1u);
            if (B.unfence_pass == 0u) {
                B.unfence_pass = B.passes;
            }
        }
    } else if (booted && w->e28e != 0u && !owns && B.boot_st != GSM_B_REL && B.boot_st != GSM_B_FILL &&
               bootv == 0u) {
        B.fence = 1u;
        B.fence_ev = (uint16_t)(B.fence_ev + 1u);
    }

    /* 4. the decision */
    if (W.latch != 0u && booted) {
        src = GSM_S_PCONT;                          /* latched until power-off */
        if (B.trip_n == 0u) {
            /* a window latch (30 s / 3.5 s / 16 commits, a torn window, a
             * warm boot's carried latch): the first one recorded. r04q:
             * before the fence (r04p: the fence first — the wire is the P
             * content either way) */
            mon_trip(0u, why, 0xFFu, W.latch);
        }
    } else if (!booted) {
        src = GSM_S_LIVE;                           /* r04q: before the candidate's boot pass (r03f) */
    } else if (bootv != 0u) {
        B.boot_st = (uint8_t)(GSM_B_VIOL | bootv);
        mon_trip(GSB_L_IDLE, (uint16_t)(why | GSB_R_IDLE | GSB_G_FIXED), 0xFFu, bootv);
        why = (uint16_t)(why | GSB_G_FIXED | GSB_G_LATCH);
        src = GSM_S_PCONT;
    } else if (B.boot_st == GSM_B_REL || B.boot_st == GSM_B_FILL) {
        src = GSM_S_CAND;                           /* r04q: the stock's E28E boot sequence */
    } else if (B.fence != 0u) {
        src = GSM_S_FENCE;                          /* r04q: the stall fence's P content */
    } else if (!nonp) {
        src = GSM_S_CAND;                           /* the P content (no permit needed) */
#if defined(GSM_HOST_BYPASS)
    } else if (true) {
        src = GSM_S_CAND;                           /* host NEGATIVE: the monitor bypassed */
#endif
    } else if (!owns) {
        mon_trip(GSB_L_IDLE, (uint16_t)(why | GSB_R_IDLE | GSB_G_FIXED), 0xFFu, 0u);
        why = (uint16_t)(why | GSB_G_FIXED | GSB_G_LATCH);
        src = GSM_S_PCONT;
    } else if ((why & (uint16_t)~GSB_R_IDLE) != 0u) {
        mon_trip(GSB_L_PERMIT, why, 0xFFu, 0u);
        why = (uint16_t)(why | GSB_G_LATCH);
        src = GSM_S_PCONT;
    } else {
        uint8_t tch = 0xFFu;
        uint16_t tv = 0u, g = 0u;
        if (relmode) {
            const uint8_t code = rel_envelope(&tch, &tv, gs_cand_reqmode);
            if (code != 0u) {
                g = (code == GSBR_T_FIXED) ? GSB_G_FIXED : GSB_G_RANGE;
                R.trip_n = (uint16_t)(R.trip_n + 1u);
                R.trip_code = code;
                R.trip_ch = tch;
                R.trip_v = tv;
                rel_end((uint8_t)(0x10u + code), (uint16_t)(why | g | GSB_G_LATCH));
            }
        } else {
            for (uint8_t ch = 0u; ch < 5u; ch++) {
                const uint16_t v = B.sh[ch];
                uint16_t t = 0u;
                if (((move >> ch) & 1u) != 0u) {
                    if (v < GSB_IMIN || v > GSB_IMAX) {
                        t = GSB_G_RANGE;
                    } else if (gs_cand_reqmode(ch) == 1u && v != GSB_IMIN && v != GSB_IMAX) {
                        t = GSB_G_RANGE;    /* a mode-1 pass is exactly 500 or 8500 */
                    }
                } else if (v != fix[ch]) {
                    t = GSB_G_FIXED;
                }
                if (t != 0u && g == 0u) {
                    tch = ch;
                    tv = v;
                }
                g = (uint16_t)(g | t);
            }
        }
        if (g != 0u) {
            B.guard_n = (uint16_t)(B.guard_n + 1u);
            B.guard_why = g;
            B.guard_ch = tch;
            B.guard_v = tv;
            if (s != 0) {
                s->guard_n = (uint8_t)(s->guard_n + 1u);
            }
            mon_trip(GSB_L_GUARD, (uint16_t)(why | g), tch, tv);
            why = (uint16_t)(why | g | GSB_G_LATCH);
            src = GSM_S_PCONT;
        } else {
            /* G6c: the r04o check "candidate != shadow on 2 owned non-P
             * passes → GSB_L_SHADOW" went with the shadow (D-G6-7: its only
             * reference was the deleted instance 1); gsb_t.shd_* stay 0 */
            src = GSM_S_CAND;
        }
    }

    /* 5. the seam's value; armed = the candidate's engine on the wire */
    /* r04q: the boot allowance's passes are not the engine's engagement —
     * not armed, not charged to the 30 s budget */
    armed = (src == GSM_S_CAND) && (owns || nonp) && B.boot_st != GSM_B_REL && B.boot_st != GSM_B_FILL;
    if (relmode && R.on != 0u && src != GSM_S_CAND) {
        rel_end(GSBR_E_WHY, why);                   /* never re-armed */
    }
    if (relmode && R.on != 0u) {
        R.n++;
    }
    if (armed) {
        B.armed_n++;
        W.acc_ms += 10u;
        win_seal();
        if (W.acc_ms >= MON_WIN_MS) {
            latch(GSB_L_WIN);                       /* the budget ends after this pass */
        }
        if (s != 0 && s->armed_n < 0xFFFFu) {
            s->armed_n = (uint16_t)(s->armed_n + 1u);
        }
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
    B.src = src;
    B.armed = armed ? 1u : 0u;
    B.applied = (src != GSM_S_LIVE) ? 1u : 0u;      /* the seam returns a value */
    B.pf = pf;
    B.why = why;
    switch (src) {
    case GSM_S_CAND:  B.cand_n++;  B.applied_n++; break;
    case GSM_S_PCONT: B.pc_n++;    B.applied_n++; break;
    case GSM_S_FENCE: B.fence_n++; B.applied_n++; break;
    default:          B.live_n++;                 break;
    }
    for (uint8_t ch = 0u; ch < 5u; ch++) {
        B.app[ch] = (src == GSM_S_CAND) ? B.sh[ch] :
                    ((src == GSM_S_PCONT || src == GSM_S_FENCE) ? PC7[ch] : 0u);
        B.cur[ch] = (src == GSM_S_LIVE) ? B.lv[ch] : B.app[ch];
    }

    /* 6. the recorders (the bridge's formats; "shadow" fields = the candidate) */
    if (s != 0 && s->n < GSB_REC_N) {
        volatile gsb_rec_t *r = rec_at(B.slot_cur, s->n);
        for (uint8_t k = 0u; k < 3u; k++) {
            r->ph[k] = gs_cand_phase(k);
        }
        r->st = (uint8_t)((armed ? 0x01u : 0u) | ((src != GSM_S_LIVE) ? 0x02u : 0u) |
                          ((W.latch != 0u) ? 0x04u : 0u) | (agree ? 0x08u : 0u) |
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
    if (R.rec < 2u) {
        volatile gsbr_slot_t *rs = &R.slot[R.rec];
        const uint8_t x = rs->xch;
        if (rs->n < GSBR_REC_N) {
            const uint8_t i = rs->n;
            const uint16_t eb = LIVE_U16(L_EST + 2u * GSB_CH_B), ex = LIVE_U16(L_EST + 2u * x);
            if (src == GSM_S_CAND) {
                rs->bm[i >> 3] = (uint8_t)(rs->bm[i >> 3] | (uint8_t)(1u << (i & 7u)));
            }
            rs->live[i][0] = (in->cyc != 0) ? in->cyc[cyc_idx(GSB_CH_B)] : 0u;
            rs->live[i][1] = (in->cyc != 0) ? in->cyc[cyc_idx(x)] : 0u;
            s_gsbr_est.est[R.rec][i][0] = (uint8_t)((eb >= 10180u) ? 255u : (eb + 20u) / 40u);
            s_gsbr_est.est[R.rec][i][1] = (uint8_t)((ex >= 10180u) ? 255u : (ex + 20u) / 40u);
            rs->n = (uint8_t)(i + 1u);
            if (R.on != 0u && src == GSM_S_CAND) {
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
#endif /* GS_G6B */
#if !GS_G6B
void gs_bridge_boot_pass(void) {}               /* r04q: no monitor, no allowance */
#endif

/* G6c: the r04n bridge form (GS_G6B 0 with a seam) is deleted; GS_G6B 1 is
 * the monitor, GS_G6B 0 a host build without the seam (no monitor at all) */
bool gs_bridge_g6b(void) { return GS_G6B != 0; }

void gs_bridge_tick_10ms(const gsb_in_t *in)
{
#if GS_G6B
    mon_tick(in);
#else
    (void)in;
    (void)latch;                        /* the monitor's, compiled out here */
    (void)GSB_PCONT;
#endif
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
/* THE SEAM (drv_gsbr.h). r04r: ch 0..6 — the live holder is retired, the
 * engine is the sole producer. A..E: the monitor's applied value (counted
 * in take_n, as r04o..r04q). WK / Sys (ch 5 / 6): the candidate's own
 * 0xDC4C4 → 0x10809C current while the seam carries the candidate, the P
 * content's 500 while it is latched / fenced (drv_reqlaw.c passes only 500
 * for these two). false = before the candidate's boot pass (the seam
 * empty: the r03f table, D-G6-15) */
bool drv_gsbr_take(uint8_t ch, uint16_t *v)
{
    if (B.applied == 0u) {
        return false;
    }
    if (ch < 5u) {
        *v = B.app[ch];
        B.take_n++;
        return true;
    }
#if GS_G6B
    if (ch == 5u || ch == 6u) {
        static const uint16_t PC7[7] = DRV_RQ_PCONT_TABLE;
        uint16_t c;
        (void)cand_cur_inj(ch, &c);
        *v = (B.src == GSM_S_CAND) ? c : PC7[ch];
        return true;
    }
#endif
    return false;
}
#endif

#endif /* !TCU_RAM_APP */
