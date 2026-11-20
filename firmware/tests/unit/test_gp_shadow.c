/* test_gp_shadow.c — THE GEAR-PAIR SHIFT-ENGINE SHADOW (app/gp_shadow.c + the
 * coupled gear chain app/gp_gear.c; 2026-11-20, ~/tcu_sweep/gp_shadow_port.md;
 * decode ~/tcu_sweep/shift_engine_gearpair_decode.md).
 *
 *   (no argument)  a speed model in D (after the garage: power flow 1): the
 *                  output speed ramps 0 → 2600 rpm and back to 0 with the
 *                  turbine at ratio(gear) · output, moving from the old to the
 *                  new ratio while the on-coming element is in its inertia
 *                  phase 9 (the stand-in); the shift-map shadow (30 ms) picks
 *                  the target, the r04w chain and the coupled chain run every
 *                  10 ms. Judged against the DECODE (independent of the
 *                  tables the shadow reads):
 *                  (1) every commit is one gear (|new − old| == 1), the gear
 *                      code walks 1 → 8 → 1 and back, each up- and downshift
 *                      seen; block A takes (old, new) and the decode's element
 *                      pair (1→2 E/C, 2→3 C/A, 3→4 D/C, 4→5 C/E, 5→6 E/B, 6→7
 *                      A/E, 7→8 E/C; a downshift n+1 → n the pair reversed);
 *                  (2) BD16 = 1 up / 2 down, BD17 = 1 up / 3 down (0x6315C,
 *                      BD1E 1), BD15 = 1 while owned and 0 after the end
 *                      (0xEBC60), the end count == the commit count;
 *                  (3) block A's on-coming / off-going phase sequences as the
 *                      decode lists them (phase 0 passes inside a pass);
 *                  (4) every completed shift counts one would-be NV counter
 *                      call (0xBA52C) — and calls nothing;
 *                  (5) the pre-select (host seam: 0xEB5A6 / 0xB6BBC accept):
 *                      block B filled with the next gear's pair while the
 *                      current shift runs; with BD14+0x26 (seam) the strobe
 *                      takes the pre-selected gear → 0xECB14 swaps B into A,
 *                      no mismatch report;
 *                  (6) the dry bench: output 0, turbine 0, engine 700 → no
 *                      pre-select of a real pair, no commit after the boot
 *                      settle (the bench prediction);
 *                  (7) a non-converging shift (turbine held at the old
 *                      ratio): no completion (the timer branch not modelled),
 *                      BD15 stays 1, the stand-in counted.
 *   --cal          every table copy against the stock image (exit 77: absent)
 * Build test_gp_shadow_neg_TABLE (GP_NEG_TABLE: the 1→2 on-coming element E →
 * D in app/gp_cal.c) must FAIL. */
#include "app/gp_shadow.h"
#include "app/gp_cal.h"
#include "app/sm_cal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { g_fail++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

/* the decode's examples (§1.2): elements 1..5 = A..E */
static const uint8_t k_up_on[9]  = { 0, 5, 3, 4, 3, 5, 1, 5, 0 };   /* n → n+1, index n */
static const uint8_t k_up_off[9] = { 0, 3, 1, 3, 5, 2, 5, 3, 0 };
/* the decode's §1.6 sequences (phase 0 passes within the start pass) */
static const uint8_t k_on_up[]  = { 1, 2, 0x13, 4, 9, 0x0A, 0x0D, 0x12 };
static const uint8_t k_on_dn[]  = { 1, 2, 0x13, 5, 9, 0x0A, 0x0B, 0x0C, 0x0D, 0x12 };
static const uint8_t k_off_up[] = { 1, 2, 3, 4, 9, 0x11 };
static const uint8_t k_off_dn[] = { 1, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x11 };
static const uint16_t k_ratio[10] = { 3306, 0, 4714, 3143, 2106, 1667, 1285, 1000, 839, 667 };

static uint16_t ratio(int g)
{
    return (g >= -1 && g <= 8) ? k_ratio[g + 1] : 0u;
}

typedef struct {
    uint16_t out;           /* output rpm this tick */
    uint8_t  pf;            /* power flow */
    uint8_t  hold_old;      /* (7): the turbine stays at the old ratio */
    uint16_t eng;
} drive_t;

static unsigned s_tick;
static float s_frac;
static uint8_t s_on_seq[16], s_on_n, s_off_seq[16], s_off_n;
static int8_t s_last_on_ph = -1, s_last_off_ph = -1;
static uint32_t s_commits, s_last_commit_n, s_last_end_n;
static int s_up_seen[9], s_dn_seen[9];
static int s_seq_checked;

static void seq_reset(void)
{
    s_on_n = s_off_n = 0u;
    s_last_on_ph = s_last_off_ph = -1;
}

static void seq_check(int8_t from, int8_t to)
{
    const int up = to > from;
    const uint8_t *eon = up ? k_on_up : k_on_dn, *eoff = up ? k_off_up : k_off_dn;
    const unsigned non = up ? sizeof k_on_up : sizeof k_on_dn, noff = up ? sizeof k_off_up : sizeof k_off_dn;
    CHECK(s_on_n == non && memcmp(s_on_seq, eon, non) == 0, "shift %d->%d: on-coming sequence (%u phases, first %02X)",
          from, to, s_on_n, s_on_seq[0]);
    CHECK(s_off_n == noff && memcmp(s_off_seq, eoff, noff) == 0, "shift %d->%d: off-going sequence (%u phases, %02X %02X)",
          from, to, s_off_n, s_off_seq[0], s_off_n > 1 ? s_off_seq[1] : 0);
    s_seq_checked++;
}

/* one 10 ms tick: the r04w chain, the gp shadow, every 3rd the shift map */
static void tick(const drive_t *d)
{
    const volatile gp_t *gp = gp_shadow_state();
    const volatile gf_t *gc = gp_gear_state();
    /* the turbine model */
    uint16_t r = ratio(gc->e69f);
    if (gp->bd15 != 0u && gp->a.on.elem != 0u && gp->bd16 != 3u && gp->bd16 != 4u) {
        const uint16_t r0 = ratio(gp->a.from), r1 = ratio(gp->a.to);
        if (gp->a.on.phase == 9u && d->hold_old == 0u) {
            s_frac += 0.1f;
            if (s_frac > 1.0f) {
                s_frac = 1.0f;
            }
            r = (uint16_t)((float)r0 + ((float)r1 - (float)r0) * s_frac);
        } else if (gp->a.on.phase >= 0x0Au && gp->a.on.phase != 0x13u && d->hold_old == 0u) {
            r = r1;
        } else {
            r = r0;
        }
    } else {
        s_frac = 0.0f;
    }
    const uint16_t turb = (uint16_t)(((uint32_t)r * d->out) / 1000u);
    gf_in_t gin;
    memset(&gin, 0, sizeof gin);
    gin.e69b = d->pf;
    gin.e6ee = d->pf;
    gin.e6ef = d->pf;
    gin.os_mode = 2u;
    gin.cd2 = 0x4Fu;
    gin.cb8 = d->out;
    gin.cba = turb;
    gin.cd0 = d->eng;
    sm_hook_gear_feed(&gin);
    gp_in_t gi;
    memset(&gi, 0, sizeof gi);
    gi.gf = gin;
    gi.r_e4a7 = sm_gear_state()->e4a7;
    gi.r_e69f = sm_gear_state()->e69f;
    gp_hook_feed(&gi);
    if (s_tick % 3u == 0u) {
        sm_hook_feed(d->out, d->out > 0u ? 1u : 0u, 0u, 0u, 2u);
    }
    s_tick++;
    /* the phase sequences of block A */
    if (gp->a.on.elem != 0u && (int8_t)gp->a.on.phase != s_last_on_ph) {
        s_last_on_ph = (int8_t)gp->a.on.phase;
        if (s_on_n < 16u) {
            s_on_seq[s_on_n++] = gp->a.on.phase;
        }
    }
    if (gp->a.off.elem != 0u && (int8_t)gp->a.off.phase != s_last_off_ph) {
        s_last_off_ph = (int8_t)gp->a.off.phase;
        if (s_off_n < 16u) {
            s_off_seq[s_off_n++] = gp->a.off.phase;
        }
    }
}

static int8_t s_cur_from, s_cur_to;

static int b7c02_type(int8_t f, int8_t t)
{
    if (f < -1 || f > 8 || t < -1 || t > 8) {
        return 0;
    }
    return sm_cal_gtype[(t + 1) + (f + 1) * 10];
}

/* after each tick: commits and ends */
static void judge_tick(int check_seq)
{
    const volatile gp_t *gp = gp_shadow_state();
    if (gp->w.end_n != s_last_end_n) {
        s_last_end_n = gp->w.end_n;
        /* (a pre-selected commit follows its end in the same pass: BD15 = 1 again) */
        CHECK(gp->bd15 == 0u || gp->w.commit_n != s_last_commit_n, "BD15 still 1 after the end 0xEBC60");
        if (check_seq && s_cur_from != s_cur_to) {
            seq_check(s_cur_from, s_cur_to);
        }
    }
    if (gp->w.commit_n != s_last_commit_n) {
        s_last_commit_n = gp->w.commit_n;
        const int8_t f = gp->a.from, t = gp->a.to;
        s_commits++;
        s_cur_from = f;
        s_cur_to = t;
        seq_reset();
        /* the start pass already stepped: re-seed from the current phases */
        if (gp->a.on.elem != 0u) {
            s_on_seq[s_on_n++] = gp->a.on.phase;
            s_last_on_ph = (int8_t)gp->a.on.phase;
        }
        if (gp->a.off.elem != 0u) {
            s_off_seq[s_off_n++] = gp->a.off.phase;
            s_last_off_ph = (int8_t)gp->a.off.phase;
        }
        if (b7c02_type(f, t) == 0) {
            /* no direct shift (the boot settle 6 → 1): BD16 = E69D = 0, nothing owned */
            CHECK(gp->bd16 == 0u && gp->bd15 == 0u, "commit %d->%d (no direct shift): BD16 %u BD15 %u", f, t,
                  gp->bd16, gp->bd15);
            return;
        }
        CHECK(t - f == 1 || f - t == 1, "commit %d->%d is not one gear", f, t);
        if (t == f + 1) {
            s_up_seen[f]++;
            CHECK(gp->a.on.elem == k_up_on[f] && gp->a.off.elem == k_up_off[f],
                  "up %d->%d: elements on %u off %u (decode %u / %u)", f, t, gp->a.on.elem, gp->a.off.elem,
                  k_up_on[f], k_up_off[f]);
            CHECK(gp->bd16 == 1u && gp->bd17 == 1u, "up %d->%d: BD16 %u BD17 %u (1 / 1)", f, t, gp->bd16, gp->bd17);
        } else if (f == t + 1) {
            s_dn_seen[t]++;
            CHECK(gp->a.on.elem == k_up_off[t] && gp->a.off.elem == k_up_on[t],
                  "down %d->%d: elements on %u off %u (the pair of %d->%d reversed)", f, t, gp->a.on.elem,
                  gp->a.off.elem, t, f);
            CHECK(gp->bd16 == 2u && gp->bd17 == 3u, "down %d->%d: BD16 %u BD17 %u (2 / 3)", f, t, gp->bd16,
                  gp->bd17);
        }
        CHECK(gp->bd15 == 1u, "commit %d->%d: BD15 %u", f, t, gp->bd15);
    }
}

static void reset_all(void)
{
    sm_hook_init();
    gp_hook_init();
    s_tick = 0u;
    s_frac = 0.0f;
    s_commits = s_last_commit_n = s_last_end_n = 0u;
    memset(s_up_seen, 0, sizeof s_up_seen);
    memset(s_dn_seen, 0, sizeof s_dn_seen);
    seq_reset();
    s_seq_checked = 0;
}

/* the garage: power flow 0 for 50 ticks (the gear chain settles at 1st),
 * then D */
static void settle(void)
{
    drive_t d = { 0u, 0u, 0u, 700u };
    for (unsigned i = 0u; i < 50u; i++) {
        tick(&d);
        judge_tick(0);
    }
}

static void test_ramp(void)
{
    reset_all();
    gp_shadow_host_si(0u, 0u);
    settle();
    const volatile gp_t *gp = gp_shadow_state();
    const volatile gf_t *gc = gp_gear_state();
    CHECK(gc->e69f == 1, "after the settle the gear code is %d (1)", gc->e69f);
    drive_t d = { 0u, 1u, 0u, 800u };
    unsigned maxg = 0u;
    for (unsigned i = 0u; i < 14000u; i++) {
        if (i < 6000u) {
            d.out = (uint16_t)((2600u * i) / 6000u);
        } else if (i < 8000u) {
            d.out = 2600u;
        } else {
            d.out = (uint16_t)(i < 14000u ? (2600u * (14000u - i)) / 6000u : 0u);
        }
        tick(&d);
        judge_tick(1);
        if (gc->e69f > 0 && (unsigned)gc->e69f > maxg) {
            maxg = (unsigned)gc->e69f;
        }
    }
    for (unsigned i = 0u; i < 600u; i++) {
        d.out = 0u;
        tick(&d);
        judge_tick(1);
    }
    CHECK(maxg == 8u, "the ramp reached gear %u (8)", maxg);
    CHECK(gc->e69f == 1, "back at standstill in gear %d (1)", gc->e69f);
    for (int g = 1; g <= 7; g++) {
        CHECK(s_up_seen[g] > 0, "no upshift %d->%d seen", g, g + 1);
        CHECK(s_dn_seen[g] > 0, "no downshift %d->%d seen", g + 1, g);
    }
    CHECK(gp->w.end_n == gp->w.commit_n, "ends %u != commits %u", (unsigned)gp->w.end_n, (unsigned)gp->w.commit_n);
    CHECK(gp->w.nv_abort_n == gp->w.done_n && gp->w.done_n >= 14u,
          "would-be NV counter calls %u, completions %u", (unsigned)gp->w.nv_abort_n, (unsigned)gp->w.done_n);
    CHECK(gp->w.nv_end_n == 0u, "an 0xEBB6C NV call counted without a garage end");
    CHECK(gp->w.err_n == 0u && gp->w.mismatch_n == 0u, "err %u mismatch %u", (unsigned)gp->w.err_n,
          (unsigned)gp->w.mismatch_n);
    CHECK(s_seq_checked >= 14, "only %d phase sequences checked", s_seq_checked);
    CHECK(gp->w.commit_ps_n == 0u, "a pre-selected commit without the seam");
    CHECK(gp->bd15 == 0u, "BD15 %u at the end", gp->bd15);
    printf("ramp: commits %u (ends %u, completions %u, would-be NV %u), phase sequences checked %d, max gear %u, "
           "pre-selects %u (type 0 %u), interlock passes %u, chain disagreement %u\n",
           (unsigned)gp->w.commit_n, (unsigned)gp->w.end_n, (unsigned)gp->w.done_n, (unsigned)gp->w.nv_abort_n,
           s_seq_checked, maxg, (unsigned)gp->w.presel_n, (unsigned)gp->w.presel_t0_n, (unsigned)gp->w.block_n,
           (unsigned)gp->w.dis_n);
}

/* (5) the pre-select and the swap (host seams) */
static void test_presel(uint8_t bd26)
{
    reset_all();
    gp_shadow_host_si(1u, bd26);
    settle();
    const volatile gp_t *gp = gp_shadow_state();
    drive_t d = { 0u, 1u, 0u, 800u };
    unsigned b_pairs_ok = 0u, b_pairs = 0u;
    uint32_t presel_prev = gp->w.presel_n;
    for (unsigned i = 0u; i < 3000u; i++) {
        d.out = (uint16_t)((2600u * (i < 400u ? i : 400u)) / 400u);    /* a fast ramp: the target runs ahead */
        tick(&d);
        judge_tick(0);
        if (getenv("GP_DBG") != NULL && bd26 != 0u && i < 700u) {
            const volatile gf_t *g = gp_gear_state();
            printf("t%u out %u e6a1 %d e4a7 %d e69f %d c25 %d e343 %d bd15 %u Async %u Aon %02X Bto %d Bon %u x51a0 %u e344 %u\n",
                   i, d.out, g->e6a1, g->e4a7, g->e69f, g->c25[0], g->e343, gp->bd15, gp->a.sync, gp->a.on.phase,
                   gp->b.to, gp->b.on.elem, g->x51a0, g->e344);
        }
        if (gp->w.presel_n != presel_prev) {
            presel_prev = gp->w.presel_n;
            const int8_t f = gp->b.from, t = gp->b.to;
            if (t == f + 1 && f >= 1 && f <= 7) {
                b_pairs++;
                if (gp->b.on.elem == k_up_on[f] && gp->b.off.elem == k_up_off[f]) {
                    b_pairs_ok++;
                }
            }
        }
    }
    CHECK(b_pairs >= 5u && b_pairs_ok == b_pairs, "block B pre-selects %u, with the decode's pair %u", b_pairs,
          b_pairs_ok);
    if (bd26 != 0u) {
        CHECK(gp->w.commit_ps_n >= 1u, "pre-selected commits (0xECB14 swaps) %u", (unsigned)gp->w.commit_ps_n);
        CHECK(gp->w.mismatch_n == 0u, "the 0x36B block-B check failed %u times", (unsigned)gp->w.mismatch_n);
    } else {
        CHECK(gp->w.commit_ps_n == 0u, "a pre-selected commit without BD14+0x26");
    }
    CHECK(gp->w.err_n == 0u, "err %u", (unsigned)gp->w.err_n);
    printf("presel (bd26 %u): block-B pre-selects %u (decode pairs %u), swaps %u, mismatches %u, commits %u, "
           "eb1e8 %u\n", (unsigned)bd26, b_pairs, b_pairs_ok, (unsigned)gp->w.commit_ps_n,
           (unsigned)gp->w.mismatch_n, (unsigned)gp->w.commit_n, (unsigned)gp->w.eb1e8_n);
    gp_shadow_host_si(0u, 0u);
}

/* (6) the dry bench: output 0, turbine 0, engine 700, plain boot and D */
static void test_bench(void)
{
    reset_all();
    settle();
    const volatile gp_t *gp = gp_shadow_state();
    const uint32_t c0 = gp->w.commit_n;
    drive_t d = { 0u, 1u, 0u, 700u };
    for (unsigned i = 0u; i < 3000u; i++) {
        tick(&d);
        judge_tick(0);
    }
    CHECK(gp->w.commit_n == c0, "a commit at standstill (%u → %u)", (unsigned)c0, (unsigned)gp->w.commit_n);
    CHECK(gp->w.commit_ps_n == 0u && gp->b.on.elem == 0u, "a real pre-select at standstill");
    CHECK(gp->bd15 == 0u, "BD15 %u at standstill", gp->bd15);
    printf("bench: passes %u, commits %u (the boot settle 6 -> 1: BD16 %u), pre-selects %u (all type 0: %u), "
           "ends %u, bd17 %u\n", (unsigned)gp->w.pass_n, (unsigned)gp->w.commit_n, gp->bd16,
           (unsigned)gp->w.presel_n, (unsigned)gp->w.presel_t0_n, (unsigned)gp->w.end_n, gp->bd17);
}

/* (7) a non-converging shift: the turbine stays at the old ratio */
static void test_noconv(void)
{
    reset_all();
    settle();
    const volatile gp_t *gp = gp_shadow_state();
    drive_t d = { 600u, 1u, 1u, 900u };             /* above the 1→2 line */
    for (unsigned i = 0u; i < 1500u; i++) {
        d.out = (uint16_t)(i < 500u ? 600u : 1200u);  /* then above the 2→3 line: the target runs ahead */
        tick(&d);
        judge_tick(0);
    }
    CHECK(gp_gear_state()->e69f == 2 && gp->w.block_n > 0u && sm_gear_state()->e69f > 2,
          "non-converging: the BD15 interlock held the gear code at %d (2; the r04w chain %d) for %u passes",
          gp_gear_state()->e69f, sm_gear_state()->e69f, (unsigned)gp->w.block_n);
    CHECK(gp->w.commit_n >= 1u && gp->w.done_n == 0u && gp->bd15 == 1u,
          "non-converging: commits %u done %u bd15 %u", (unsigned)gp->w.commit_n, (unsigned)gp->w.done_n, gp->bd15);
    CHECK(gp->a.on.phase == 9u, "non-converging: on-coming phase %02X (9: waiting for the sync)", gp->a.on.phase);
    printf("noconv: commits %u, done %u, BD15 %u, block A on phase %02X off phase %02X, slip %d\n",
           (unsigned)gp->w.commit_n, (unsigned)gp->w.done_n, gp->bd15, gp->a.on.phase, gp->a.off.phase,
           (int)gp->a.slip);
}

static int cal_check(void)
{
#if !defined(TCU_STOCK_IMAGE)
    return 77;
#else
    FILE *f = fopen(TCU_STOCK_IMAGE, "rb");
    if (f == NULL) {
        printf("SKIP no stock image %s\n", TCU_STOCK_IMAGE);
        return 77;
    }
    static uint8_t img[0x280000];
    const size_t n = fread(img, 1, sizeof img, f);
    fclose(f);
    if (n < 0x200000u) {
        return 77;
    }
#define U32(a) (((uint32_t)img[a] << 24) | ((uint32_t)img[(a) + 1] << 16) | ((uint32_t)img[(a) + 2] << 8) | img[(a) + 3])
#define U16(a) ((uint16_t)(((uint16_t)img[a] << 8) | img[(a) + 1]))
    CHECK(U32(0xDC3DC) == GP_CAL_A_ON && U32(0xDC3E0) == GP_CAL_A_OFF, "the 0xDC198 / 0xDC1AE pools");
    CHECK(U32(0xEDB44) == GP_CAL_A_BD17, "the 0xED988 pool");
    CHECK(U32(0x113D98) == GP_CAL_A_ONSEQ && U32(0x110B84) == GP_CAL_A_OFFSEQ, "the 0x113D0A / 0x110ADE pools");
    CHECK(U32(0x5713C + 0xB0) == GP_CAL_A_THR, "the sync band pointer");
    unsigned bad = 0u;
    for (unsigned i = 0u; i < 100u; i++) {
        bad += (gp_cal_on[i] != img[GP_CAL_A_ON + i]) + (gp_cal_off[i] != img[GP_CAL_A_OFF + i]);
    }
    for (unsigned i = 0u; i < 9u; i++) {
        bad += gp_cal_bd17[i] != img[GP_CAL_A_BD17 + i];
    }
    for (unsigned i = 0u; i < GP_CAL_ONSEQ_N; i++) {
        bad += gp_cal_onseq[i] != img[GP_CAL_A_ONSEQ + i];
    }
    for (unsigned i = 0u; i < GP_CAL_OFFSEQ_N; i++) {
        bad += gp_cal_offseq[i] != img[GP_CAL_A_OFFSEQ + i];
    }
    for (unsigned i = 0u; i < 10u; i++) {
        bad += gp_cal_ratio[i] != U16(GP_CAL_A_RATIO + 2u * i);
    }
    bad += gp_cal_sync_thr != U16(GP_CAL_A_THR);
    CHECK(bad == 0u, "%u table values differ from the image", bad);
    printf("gp_cal: %u values checked against the image, %u differ\n", 2u * 100u + 9u + GP_CAL_ONSEQ_N +
           GP_CAL_OFFSEQ_N + 10u + 1u, bad);
    return g_fail != 0 ? 1 : 0;
#endif
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--cal") == 0) {
        return cal_check();
    }
    test_ramp();
    test_presel(0u);
    test_presel(1u);
    test_bench();
    test_noconv();
    if (g_fail != 0) {
        printf("test_gp_shadow: %d FAILED\n", g_fail);
        return 1;
    }
    printf("test_gp_shadow: ok\n");
    return 0;
}
