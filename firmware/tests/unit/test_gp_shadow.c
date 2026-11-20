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
 *   (2026-11-20, the stand-in retirements, ~/tcu_sweep/gp_standins_port.md)
 *                  (8) the kind / class machine in the speed ramp: in the torque /
 *                      temperature window the START call 0xED906 (kind 0 after the
 *                      progress init 0x117970) takes kind 4, class 1 for an up- AND a
 *                      downshift [bytes; the decode §1.4 assumed kind 1 and had the
 *                      upshift at kind 2 / class 0]; out of the window kind 2, class 0;
 *                      the 2->1 / 3->2 downshifts then run the class-1 rows (phase 1 / 0x0A
 *                      skipped, the 0x0D lock 50 / 40 passes, 0x0B 20 / 10, 0x0F 7);
 *                      the 0xEC182 / 0xEC1F0 class table against an independent list; the
 *                      0x117B4A override (host seam AE31);
 *                  (9) BD1B 0 in D, 1 at power flow 0, latched until the next commit,
 *                      0 while BD17 == 4;
 *                  (10) the slot-28 counters: +1 per pass, saturating, the resets at the
 *                      documented entries, ordered before the phase steps (a phase of D
 *                      passes lasts exactly D observed passes);
 *                  (11) the 0xECB14 swap exchanges every listed field, the two ramp objects
 *                      and the pair ids, re-pins the context pointers, sets A+3 = 1 / B+3 = 0;
 *                      the ramp class init / step.
 *   --cal          every table copy against the stock image (exit 77: absent)
 * Build test_gp_shadow_neg_TABLE (GP_NEG_TABLE: the 1→2 on-coming element E →
 * D in app/gp_cal.c) must FAIL; so must test_gp_shadow_neg_CLASS (GP_NEG_CLASS: the
 * down-shift f8 array t[1] 1 -> 0 — every kind stays 2, class 0) and
 * test_gp_shadow_neg_SWAP (GP_NEG_SWAP: no ramp exchange, no re-pin). */
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
static int16_t s_tq;            /* the input torque by value (0x89CA0) */
static uint8_t s_ped = 23u;     /* the pedal (0x89CD8) */
static uint8_t s_pf_now = 1u;
static uint8_t s_prev_tin, s_prev_ph = 0xFFu;
static int s_expect_ov = -1;                    /* ctx+0x30 (-1: not judged) */
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

/* the class-1 downshifts 2->1 / 3->2 (decode §6): the on-coming phase 1 and 0x0A, the off-going phase 1
 * last 0 passes — a phase that lasts 0 passes is left in the entry pass and never observed */
static const uint8_t k_on_dn_c1[]  = { 2, 0x13, 5, 9, 0x0B, 0x0C, 0x0D, 0x12 };
static const uint8_t k_off_dn_c1[] = { 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x11 };
static int s_cur_c1;            /* the block's class at the commit (1: the class-1 rows apply) */
static unsigned s_len_0d, s_len_off_0b, s_len_off_0f, s_len_on_0a;    /* observed passes per phase */

/* the decode's class-1 off-going durations (0x0B: 2->1 axis 17/42/67/101 -> 20/20/16/10, 3->2 -> 10 everywhere;
 * 0x0F: 2->1 axis 34/51/67/151 -> 7/20/20/10, 3->2 axis 42/51/84/151 -> 7/10/20/20), linear between the
 * breakpoints (truncating), the torque class clamp_u8(tq / 6 + 34) */
static unsigned dec_interp(const int *xs, const int *ys, int x)
{
    if (x <= xs[0]) {
        return (unsigned)ys[0];
    }
    if (x >= xs[3]) {
        return (unsigned)ys[3];
    }
    int j = 1;
    while (x > xs[j]) {
        j++;
    }
    const int dy = ys[j] - ys[j - 1], dx = xs[j] - xs[j - 1];
    return (unsigned)(ys[j - 1] + (dy * (x - xs[j - 1])) / dx);
}
static unsigned dec_off(int from, int which, int cls)
{
    static const int ax0b[4] = { 17, 42, 67, 101 };
    static const int y0b21[4] = { 20, 20, 16, 10 }, y0b32[4] = { 10, 10, 10, 10 };
    static const int ax0f21[4] = { 34, 51, 67, 151 }, y0f21[4] = { 7, 20, 20, 10 };
    static const int ax0f32[4] = { 42, 51, 84, 151 }, y0f32[4] = { 7, 10, 20, 20 };
    if (which == 0) {
        return dec_interp(ax0b, from == 2 ? y0b21 : y0b32, cls);
    }
    return from == 2 ? dec_interp(ax0f21, y0f21, cls) : dec_interp(ax0f32, y0f32, cls);
}

static void seq_check(int8_t from, int8_t to)
{
    const int up = to > from;
    const int c1 = (!up) && s_cur_c1 && (from == 2 || from == 3);
    const uint8_t *eon = up ? k_on_up : (c1 ? k_on_dn_c1 : k_on_dn);
    const uint8_t *eoff = up ? k_off_up : (c1 ? k_off_dn_c1 : k_off_dn);
    const unsigned non = up ? sizeof k_on_up : (c1 ? sizeof k_on_dn_c1 : sizeof k_on_dn);
    const unsigned noff = up ? sizeof k_off_up : (c1 ? sizeof k_off_dn_c1 : sizeof k_off_dn);
    if (c1) {
        /* the decoded durations: lock 50 / 40 passes, 0x0B 20 / 10, 0x0F 7 (class 1, torque class 34) */
        CHECK(s_len_0d == (from == 2 ? 50u : 40u), "class-1 %d->%d: the 0x0D lock lasted %u passes", from, to, s_len_0d);
        int cls = (int)s_tq / 6 + 34;
        cls = cls < 0 ? 0 : (cls > 255 ? 255 : cls);
        CHECK(s_len_off_0b == dec_off(from, 0, cls), "class-1 %d->%d (torque class %d): off 0x0B lasted %u passes (%u)",
              from, to, cls, s_len_off_0b, dec_off(from, 0, cls));
        CHECK(s_len_off_0f == dec_off(from, 1, cls), "class-1 %d->%d (torque class %d): off 0x0F lasted %u passes (%u)",
              from, to, cls, s_len_off_0f, dec_off(from, 1, cls));
    } else if (up) {
        CHECK(s_len_0d == GP_SI_T_SHORT && s_len_on_0a == GP_SI_T_SHORT,
              "up %d->%d: the stand-in phases 0x0D / 0x0A lasted %u / %u passes (%u)", from, to, s_len_0d, s_len_on_0a,
              (unsigned)GP_SI_T_SHORT);
    }
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
    s_pf_now = d->pf;
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
    gp_in2_t in2;
    memset(&in2, 0, sizeof in2);
    in2.tq = s_tq;
    in2.pedal = s_ped;
    in2.now_ms = (uint16_t)(s_tick * 10u);
    gp_shadow_set_in2(&in2);
    gp_hook_feed(&gi);
    if (s_tick % 3u == 0u) {
        sm_hook_feed(d->out, d->out > 0u ? 1u : 0u, 0u, 0u, 2u);
    }
    s_tick++;
    /* the slot-28 counters against the phase objects (0xEC0E0 / 0x11295C): a counter reset at its phase
     * entry counts the same passes as the phase's +0x1A while that phase runs */
    {
        const volatile gp_x_t *gx = gp_shadow_x();
        const uint8_t ph = gp->a.on.phase, tin = gp->a.on.tin;
        if (gp->a.on.elem != 0u && tin < 255u) {
            CHECK(ph != 9u || gx->blk[0].on_ca == tin, "phase 9: +0xA %u vs +0x1A %u", gx->blk[0].on_ca, tin);
            CHECK(ph != 0x0Au || gx->blk[0].on_cc == tin, "phase 0x0A: +0xC %u vs +0x1A %u", gx->blk[0].on_cc, tin);
            CHECK((ph != 4u && ph != 5u) || gx->blk[0].on_c10 == tin, "phase 4/5: +0x10 %u vs +0x1A %u",
                  gx->blk[0].on_c10, tin);
            CHECK(ph != 4u || gx->blk[0].on_c3e == tin, "phase 4: +0x3E %u vs +0x1A %u", gx->blk[0].on_c3e, tin);
            if (s_prev_ph == 9u && ph == 0x0Au) {        /* the old +0x1A -> +0x34 at the step */
                CHECK(gx->blk[0].on_c34 == (uint16_t)(s_prev_tin + 1u), "9 -> 0x0A: +0x34 %u (the last +0x1A %u + 1)",
                      gx->blk[0].on_c34, s_prev_tin);
            }
        }
        s_prev_ph = ph;
        s_prev_tin = tin;
    }
    /* observed passes per phase of block A (reset at the commit by judge_tick) */
    if (gp->a.on.elem != 0u) {
        s_len_0d += (gp->a.on.phase == 0x0Du);
        s_len_on_0a += (gp->a.on.phase == 0x0Au);
    }
    if (gp->a.off.elem != 0u) {
        s_len_off_0b += (gp->a.off.phase == 0x0Bu);
        s_len_off_0f += (gp->a.off.phase == 0x0Fu);
    }
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
static int s_expect_kind, s_expect_cls;      /* 0: not judged */
static unsigned s_commits_judged;

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
        s_len_0d = s_len_off_0b = s_len_off_0f = s_len_on_0a = 0u;
        seq_reset();
        s_cur_c1 = (gp_shadow_x()->blk[0].cls[0] == 1u);
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
        /* the kind / class machine (bytes): the progress init zeroes the kind, so the start call sees kind 0 */
        if (s_expect_kind != 0) {
            const volatile gp_x_t *x = gp_shadow_x();
            CHECK(x->blk[0].k7 == s_expect_kind && x->blk[0].cls[0] == s_expect_cls,
                  "commit %d->%d (tq %d): kind %u class %u (expected %d / %d)", f, t, (int)s_tq, x->blk[0].k7,
                  x->blk[0].cls[0], s_expect_kind, s_expect_cls);
            CHECK(x->blk[0].f8 == 1u, "commit %d->%d: f8 %u (1)", f, t, x->blk[0].f8);
            CHECK(s_expect_ov < 0 || x->blk[0].ov == s_expect_ov, "commit %d->%d: override flag %u (%d)", f, t,
                  x->blk[0].ov, s_expect_ov);
        }
        s_commits_judged++;
    }
    /* BD1B (0xEC6A6): 0 in D at any speed on this calibration (thresholds 0; power flow 1) */
    if (gp->bd15 != 0u && gp->bd17 != 4u && s_pf_now == 1u) {
        CHECK(gp->bd1b == 0u, "BD1B %u in D", gp->bd1b);
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
    s_tq = 0;
    s_ped = 23u;
    s_expect_kind = s_expect_cls = 0;
    s_expect_ov = -1;
    s_commits_judged = 0u;
    s_prev_tin = 0u;
    s_prev_ph = 0xFFu;
    s_cur_c1 = 0;
    gp_shadow_host_ae31(0u);
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

static void test_ramp_tq(int16_t tq, int ekind, int ecls)
{
    reset_all();
    gp_shadow_host_si(0u, 0u);
    settle();
    s_tq = tq;
    s_expect_kind = ekind;
    s_expect_cls = ecls;
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
    CHECK(s_commits_judged >= 14u, "kind / class judged on %u commits", s_commits_judged);
    printf("ramp (tq %d, kind %d class %d judged on %u commits, class-1 durations %u): commits %u (ends %u, "
           "completions %u, would-be NV %u), phase sequences checked %d, max gear %u, "
           "pre-selects %u (type 0 %u), interlock passes %u, chain disagreement %u\n",
           (int)tq, ekind, ecls, s_commits_judged, (unsigned)gp_shadow_x()->w.c1_n, (unsigned)gp->w.commit_n, (unsigned)gp->w.end_n, (unsigned)gp->w.done_n, (unsigned)gp->w.nv_abort_n,
           s_seq_checked, maxg, (unsigned)gp->w.presel_n, (unsigned)gp->w.presel_t0_n, (unsigned)gp->w.block_n,
           (unsigned)gp->w.dis_n);
}

/* (8) the class table 0xEC182 / 0xEC1F0 against an independent list (decode §1.6) */
static void test_class_fn(void)
{
    /* kinds 3..9: flag 0 / flag 1 (type != 4) and flag 0 with type 4 */
    static const uint8_t f0[7]  = { 0, 1, 0, 2, 1, 2, 2 };
    static const uint8_t f1[7]  = { 1, 1, 2, 2, 2, 3, 2 };
    static const uint8_t t4f0[7] = { 1, 1, 1, 2, 1, 2, 2 };
    unsigned n = 0u;
    for (unsigned kind = 0u; kind < 14u; kind++) {
        for (unsigned flag = 0u; flag < 2u; flag++) {
            for (unsigned type = 0u; type < 6u; type++) {
                uint8_t e;
                if (kind < 3u || kind > 9u) {
                    e = (type == 4u) ? 1u : 0u;
                } else if (flag != 0u) {
                    e = f1[kind - 3u];
                } else {
                    e = (type == 4u) ? t4f0[kind - 3u] : f0[kind - 3u];
                }
                CHECK(gp_class_ec182((uint8_t)kind, (uint8_t)flag, (uint8_t)type) == e,
                      "0xEC182(kind %u, flag %u, type %u) = %u (%u)", kind, flag, type,
                      gp_class_ec182((uint8_t)kind, (uint8_t)flag, (uint8_t)type), e);
                const uint8_t e1f0 = (kind == 9u) ? 4u : e;
                CHECK(gp_class_ec1f0((uint8_t)kind, (uint8_t)flag, (uint8_t)type) == e1f0, "0xEC1F0(%u, %u, %u)", kind,
                      flag, type);
                n++;
            }
        }
    }
    printf("class: %u (kind, flag, type) combinations of 0xEC182 / 0xEC1F0 against the decode's list\n", n);
}

/* (8b) the 0x117B4A override: AE31 == 3, tq 1500 (outside the window), pedal 100: the override's f8 array
 * (1 -> kind 4) takes block A at the start call of the up- AND the downshifts, with ctx+0x30 set */
static void test_override(void)
{
    gp_shadow_host_ae31(1u);
    reset_all();
    gp_shadow_host_ae31(1u);
    gp_shadow_host_si(0u, 0u);
    settle();
    s_tq = 1500;
    s_ped = 100u;
    s_expect_kind = 4;
    s_expect_cls = 1;
    s_expect_ov = 1;
    drive_t d = { 0u, 1u, 0u, 800u };
    for (unsigned i = 0u; i < 14000u; i++) {
        d.out = (uint16_t)(i < 6000u ? (2600u * i) / 6000u : (i < 8000u ? 2600u : (2600u * (14000u - i)) / 6000u));
        tick(&d);
        judge_tick(1);
    }
    CHECK(s_commits_judged >= 14u, "override: judged on %u commits", s_commits_judged);
    CHECK(gp_shadow_x()->w.k_ovr >= 14u, "override taken %u times", (unsigned)gp_shadow_x()->w.k_ovr);
    printf("override (AE31 seam): kind 4 / ov 1 on %u commits, override taken %u times\n", s_commits_judged,
           (unsigned)gp_shadow_x()->w.k_ovr);
    /* the pedal below its limit leaves a downshift in the window-failed kind 2 (up: the torque map still takes it) */
    gp_shadow_host_ae31(0u);
}

/* (9) BD1B 0xEC6A6: 0 in D (the speed thresholds are 0 on this cal); set by power flow 0 / 3 / 4 and latched;
 * 0 while BD17 == 4; cleared at the commit (0xED9B2: every commit of the ramp reads 0 in D, judge_tick) */
static void test_bd1b(void)
{
    reset_all();
    settle();
    gp_t *m = gp_shadow_host_mut();
    const volatile gp_x_t *gx = gp_shadow_x();
    drive_t d = { 0u, 1u, 0u, 800u };
    /* a shift is owned (BD15 = 1, up, BD17 1): in D BD1B stays 0 */
    m->bd15 = 1u;
    m->bd16 = 1u;
    m->bd17 = 1u;
    m->bd1b = 0u;
    tick(&d);
    CHECK(m->bd1b == 0u, "BD1B %u in D with a shift owned", m->bd1b);
    for (unsigned pf = 0u; pf < 5u; pf++) {
        m->bd15 = 1u;
        m->bd16 = 1u;
        m->bd17 = 1u;
        m->bd1b = 0u;
        d.pf = (uint8_t)pf;
        tick(&d);
        const uint8_t want = (pf == 0u || pf == 3u || pf == 4u) ? 1u : 0u;
        CHECK(m->bd1b == want, "BD1B %u at power flow %u (%u)", m->bd1b, pf, want);
    }
    /* latched: power flow back to 1 keeps it */
    m->bd15 = 1u;
    m->bd17 = 1u;
    m->bd1b = 0u;
    d.pf = 0u;
    tick(&d);
    CHECK(m->bd1b == 1u, "BD1B not set at power flow 0");
    d.pf = 1u;
    m->bd15 = 1u;
    tick(&d);
    CHECK(m->bd1b == 1u, "BD1B %u not latched", m->bd1b);
    /* BD17 == 4 (the garage owns the object) forces 0 */
    m->bd15 = 1u;
    m->bd17 = 4u;
    tick(&d);
    CHECK(m->bd1b == 0u, "BD1B %u with BD17 == 4", m->bd1b);
    CHECK(gx->w.bd1b_n == 4u, "BD1B set %u times (4)", (unsigned)gx->w.bd1b_n);
    printf("bd1b: 0 in D, 1 at power flow 0 / 3 / 4 (latched), 0 with BD17 == 4, 0 at every commit; set %u times\n",
           (unsigned)gx->w.bd1b_n);
}

/* (10) the slot-28 counters in a non-converging shift: +1 per pass, saturating, reset at the commit */
static void test_counters(void)
{
    reset_all();
    settle();
    const volatile gp_t *gp = gp_shadow_state();
    const volatile gp_x_t *gx = gp_shadow_x();
    drive_t d = { 600u, 1u, 1u, 900u };
    unsigned c0 = 0u, commit_seen = 0u, ph9_seen = 0u, ph9_at = 0u;
    for (unsigned i = 0u; i < 1500u; i++) {
        d.out = (uint16_t)(i < 500u ? 600u : 1200u);
        tick(&d);
        judge_tick(0);
        if (!commit_seen && gp->w.commit_n >= 2u) {
            commit_seen = 1u;
            c0 = i;
        }
        if (gp->a.on.phase == 9u && !ph9_seen) {
            ph9_seen = 1u;
            ph9_at = i;
        }
    }
    const unsigned since_commit = 1499u - c0, since_ph9 = 1499u - ph9_at;
    CHECK(gp->bd15 == 1u && gp->a.on.phase == 9u, "counters: BD15 %u phase %02X", gp->bd15, gp->a.on.phase);
    CHECK(gp->a.on.tin == 255u, "+0x1A (u8) %u saturates at 255", gp->a.on.tin);
    CHECK(gx->blk[0].on_ca == since_ph9, "+0xA (u16) %u after %u passes in phase 9", gx->blk[0].on_ca, since_ph9);
    CHECK(gx->blk[0].c22 == since_commit, "ctx+0x22 (u16) %u after %u passes since the commit", gx->blk[0].c22,
          since_commit);
    CHECK(gp->bd68 == 255u && gx->bd5f == 255u && gx->bd51 == 255u && gp->bd67 == 255u,
          "BD14 u8 counters %u %u %u %u saturate at 255", gp->bd68, gx->bd5f, gx->bd51, gp->bd67);
    CHECK(gx->bd2e == 0u && gx->bd30 == 0u, "BD2E %u / BD30 %u not cleared while unsynchronised", gx->bd2e, gx->bd30);
    CHECK(gx->w.c_pass >= 1500u, "slot-28 passes %u", (unsigned)gx->w.c_pass);
    /* a synchronised, converged shift: BD30 / BD2E count the locked passes (cleared only while unsynchronised) */
    printf("counters: +0x1A %u (sat), +0xA %u, ctx+0x22 %u, BD68 %u, BD2E %u, BD30 %u\n", gp->a.on.tin,
           gx->blk[0].on_ca, gx->blk[0].c22, gp->bd68, gx->bd2e, gx->bd30);
}

/* (11) the 0xECB14 swap: every listed field, both phase objects, the ramp objects, the pair ids; the re-pin */
static void fill_bytes(void *p, size_t n, uint8_t seed)
{
    for (size_t i = 0u; i < n; i++) {
        ((uint8_t *)p)[i] = (uint8_t)(seed + 7u * i);
    }
}
static void test_swap(void)
{
    reset_all();
    gp_t *g = gp_shadow_host_mut();
    gp_x_t *x = gp_shadow_host_xmut();
    gp_ramp_t (*r)[2] = gp_shadow_host_rmut();
    fill_bytes(&g->a, sizeof g->a, 0x11u);
    fill_bytes(&g->b, sizeof g->b, 0x80u);
    fill_bytes(&x->blk[0], sizeof x->blk[0], 0x21u);
    fill_bytes(&x->blk[1], sizeof x->blk[1], 0x90u);
    for (unsigned k = 0u; k < 2u; k++) {                 /* every object points at its own context */
        x->blk[k].xon.ctxp = (uint8_t)k;
        x->blk[k].xoff.ctxp = (uint8_t)k;
    }
    gp_ramp_init(&r[0][0], 10, -20, 300, 1000u);
    gp_ramp_init(&r[0][1], 20, -30, 400, 1010u);
    gp_ramp_init(&r[1][0], 30, -40, 500, 1020u);
    gp_ramp_init(&r[1][1], 40, -50, 600, 1030u);
    const gp_blk_t oa = g->a, ob = g->b;
    const gp_xblk_t xa = x->blk[0], xb = x->blk[1];
    const gp_ramp_t ra0 = r[0][0], ra1 = r[0][1], rb0 = r[1][0], rb1 = r[1][1];
    const uint16_t repin0 = x->w.repin_n;
    gp_shadow_host_ecb14();
    gp_blk_t ea = ob, eb = oa;                           /* B -> A, A -> B; the flags are set by the swap */
    ea.act = 1u;
    eb.act = 0u;
    CHECK(memcmp(&g->a, &ea, sizeof ea) == 0, "swap: block A is not B's every field (+3 = 1)");
    CHECK(memcmp(&g->b, &eb, sizeof eb) == 0, "swap: block B is not A's every field (+3 = 0)");
    gp_xblk_t exa = xb, exb = xa;                        /* the extension travels; the re-pin points each object home */
    exa.xon.ctxp = exa.xoff.ctxp = 0u;
    exb.xon.ctxp = exb.xoff.ctxp = 1u;
    CHECK(memcmp(&x->blk[0], &exa, sizeof exa) == 0, "swap: block A's extension (counters, pair ids, re-pin)");
    CHECK(memcmp(&x->blk[1], &exb, sizeof exb) == 0, "swap: block B's extension (counters, pair ids, re-pin)");
    CHECK(memcmp(&r[0][0], &rb0, sizeof rb0) == 0 && memcmp(&r[0][1], &rb1, sizeof rb1) == 0 &&
              memcmp(&r[1][0], &ra0, sizeof ra0) == 0 && memcmp(&r[1][1], &ra1, sizeof ra1) == 0,
          "swap: the ramp objects are not exchanged");
    CHECK((uint16_t)(x->w.repin_n - repin0) == 4u, "swap: %u phase-object pointers re-pinned (4)",
          (unsigned)(uint16_t)(x->w.repin_n - repin0));
    /* the pair ids travelled with the objects */
    CHECK(memcmp(x->blk[0].xon.pair, xb.xon.pair, 3u) == 0 && memcmp(x->blk[1].xoff.pair, xa.xoff.pair, 3u) == 0,
          "swap: the pair-cell ids did not travel");
    printf("swap: blocks A <-> B (%zu + %zu bytes), extensions, 4 ramp objects, re-pin %u pointers, A+3 = %u B+3 = %u\n",
           sizeof g->a, sizeof x->blk[0], (unsigned)(uint16_t)(x->w.repin_n - repin0), g->a.act, g->b.act);
}

/* (11b) the 16-byte ramp class: init 0xD1D8E / step 0xD1D96 */
static void test_ramp_obj(void)
{
    reset_all();
    gp_ramp_t r;
    memset(&r, 0, sizeof r);
    gp_ramp_init(&r, 100, 0, 1000, 5000u);
    CHECK(r.v == 100.0f && r.mn == 0.0f && r.mx == 1000.0f && r.ts == 5000u, "ramp init");
    CHECK(gp_ramp_step(&r, 5, 5010u) == 105, "ramp: +5 per 10 ms at rate 5");
    CHECK(gp_ramp_step(&r, -100, 5020u) == 5, "ramp: rate -100 over 10 ms");
    CHECK(gp_ramp_step(&r, -100, 5030u) == 0 && r.v == 0.0f, "ramp: clamped at the minimum");
    CHECK(gp_ramp_step(&r, 32767, 5040u) == 1000, "ramp: clamped at the maximum");
    gp_ramp_init(&r, 0, 0, 32767, 0xFFF0u);
    CHECK(gp_ramp_step(&r, 100, 4u) == 200, "ramp: the u16 ms clock wraps (dt 20 ms: +100 per 10 ms at rate 100)");
    gp_ramp_init(&r, 0, -32768, 32767, 0u);
    CHECK(gp_ramp_step(&r, 32767, 100u) == 32767, "ramp: s16 saturation");
    printf("ramp class: init / step / clamp / wrap / saturation as 0xD1D8E / 0xD1D96\n");
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
    CHECK(gp_shadow_x()->blk[0].k7 == 1u && gp_shadow_x()->blk[1].k7 == 1u && gp_shadow_x()->blk[0].cls[0] == 0u,
          "bench: the idle kind %u / %u class %u (1 / 1 / 0)", gp_shadow_x()->blk[0].k7, gp_shadow_x()->blk[1].k7,
          gp_shadow_x()->blk[0].cls[0]);
    CHECK(gp->bd1e == 0u && gp->bd68 == 0u && gp->bd1b == 0u, "bench: idle BD1E %u BD68 %u BD1B %u", gp->bd1e, gp->bd68,
          gp->bd1b);
    {
        const volatile gp_x_t *gx = gp_shadow_x();
        CHECK(gx->bd5f == 255u && gx->bd51 == 255u && gp->bd67 == 255u && gx->bd2e > 3000u && gx->bd30 == 255u,
              "bench counters: BD5F %u BD51 %u BD67 %u BD2E %u BD30 %u (255 255 255 >3000 255: block A is synchronised since the settle commit)", gx->bd5f, gx->bd51, gp->bd67,
              gx->bd2e, gx->bd30);
        CHECK(gx->w.c_pass == gp->w.pass_n && gx->w.k_cyc >= 2u * 3000u && gx->w.k_start == 0u && gx->w.k_chg <= 2u &&
                  gx->w.k_ovr == 0u && gx->w.repin_n == 0u && gx->w.bd1b_n == 0u && gx->w.c1_n == 0u &&
                  gx->w.k_a1032 == 0u && gx->w.k_adapt == 0u,
              "bench witnesses: passes %u/%u k_cyc %u k_start %u chg %u ovr %u repin %u bd1b %u c1 %u a1032 %u adapt %u",
              (unsigned)gx->w.c_pass, (unsigned)gp->w.pass_n, (unsigned)gx->w.k_cyc, (unsigned)gx->w.k_start,
              (unsigned)gx->w.k_chg, (unsigned)gx->w.k_ovr, (unsigned)gx->w.repin_n, (unsigned)gx->w.bd1b_n,
              (unsigned)gx->w.c1_n, (unsigned)gx->w.k_a1032, (unsigned)gx->w.k_adapt);
    }
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
    unsigned n2 = 0u, bad2 = 0u;
    /* 2026-11-20: the kind / class machine's tables, through the stock's own pointers */
#define B_(o) U32(0x5713Cu + (o))
    CHECK(B_(0x2C) == 0x1B607Cu && B_(0x34) == 0x1B6094u && U32(B_(0x2C)) == 5u && U32(B_(0x34)) == 5u &&
              U32(0x19B03Cu) == 5u, "the f8 array headers");
    for (unsigned i = 0u; i < 5u; i++) {
        bad2 += (gp_cal_f8_dn[i] != img[B_(0x2C) + 4u + i]) + (gp_cal_f8_up[i] != img[B_(0x34) + 4u + i]) +
                (gp_cal_f8_ov[i] != img[0x19B03Cu + 4u + i]);
        n2 += 3u;
    }
    static const uint8_t wo[4] = { 0xF0, 0xF4, 0xF8, 0xFC }, wl[4] = { 0xE0, 0xE4, 0xE8, 0xEC },
                         wh[4] = { 0xD0, 0xD4, 0xD8, 0xDC };
    for (unsigned f = 0u; f < 4u; f++) {
        CHECK(U32(B_(wo[f])) == 10u && U32(B_(wo[f]) + 4u) == 10u && U32(B_(wl[f])) == 10u &&
                  U32(B_(wh[f])) == 10u, "window %u table headers (10 x 10)", f);
        for (unsigned i = 0u; i < 100u; i++) {
            bad2 += (gp_cal_win_out[f][i] != U16(B_(wo[f]) + 8u + 2u * i)) +
                    (gp_cal_win_lo[f][i] != (int16_t)U16(B_(wl[f]) + 8u + 2u * i)) +
                    (gp_cal_win_hi[f][i] != (int16_t)U16(B_(wh[f]) + 8u + 2u * i));
            n2 += 3u;
        }
    }
    static const uint8_t to_[10] = { 0x80, 0x84, 0x88, 0x8C, 0x90, 0x94, 0x98, 0x9C, 0xA0, 0xA4 };
    for (unsigned i = 0u; i < 10u; i++) {
        bad2 += gp_cal_tmp[i] != img[B_(to_[i])];
        n2++;
    }
    bad2 += (gp_cal_k7c != img[B_(0x7C)]) + (gp_cal_ka8 != img[B_(0xA8)]) + (gp_cal_ov_ped != img[B_(0x154)]);
    n2 += 3u;
    static const uint8_t tho[5] = { 0xB8, 0xBC, 0xC0, 0xC4, 0xC8 };
    for (unsigned i = 0u; i < 5u; i++) {
        bad2 += gp_cal_thr[i] != (int16_t)U16(B_(tho[i]));
        n2++;
    }
    CHECK(B_(0x158) == 0x1B78A0u && U32(0x1B78A0u) == 4u, "the override map header");
    for (unsigned i = 0u; i < 4u; i++) {
        bad2 += (gp_cal_ov_x[i] != (int16_t)U16(0x1B78A0u + 4u + 2u * i)) +
                (gp_cal_ov_y[i] != (int16_t)U16(0x1B78A0u + 12u + 2u * i));
        n2 += 2u;
    }
    /* the class-1 rows: the pointer chains and the values */
    for (unsigned f = 0u; f < 2u; f++) {
        const uint32_t from = 2u + f;
        const uint32_t blk = U32(0x5CD5Cu + 8u * U32(0x5F014u + 4u * from));
        CHECK(blk == (f == 0u ? 0x5C84Cu : 0x5C894u), "class-1 on block %u", f);
        bad2 += (gp_cal_c1_on[f][0] != img[U32(blk + 4u)]) + (gp_cal_c1_on[f][1] != img[U32(blk + 0x14u)]) +
                (gp_cal_c1_on[f][2] != img[U32(blk)]);
        n2 += 3u;
        const uint32_t boff = U32(0x59E34u + 8u * U32(0x5E984u + 4u * from));
        CHECK(boff == (f == 0u ? 0x596E4u : 0x59718u), "class-1 off block %u", f);
        for (unsigned w = 0u; w < 2u; w++) {
            const uint32_t m = U32(boff + (w == 0u ? 0xCu : 0x28u));
            CHECK(U32(m) == 4u, "class-1 map header");
            for (unsigned i = 0u; i < 8u; i++) {
                bad2 += gp_cal_c1_off[f][w][i] != img[m + 4u + i];
                n2++;
            }
        }
    }
    CHECK(bad2 == 0u, "%u kind / class table values differ from the image", bad2);
    printf("gp_cal: %u values checked against the image, %u differ; the kind / class tables: %u checked, %u differ\n",
           2u * 100u + 9u + GP_CAL_ONSEQ_N + GP_CAL_OFFSEQ_N + 10u + 1u, bad, n2, bad2);
    return g_fail != 0 ? 1 : 0;
#endif
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--cal") == 0) {
        return cal_check();
    }
    test_class_fn();
    test_ramp_tq(0, 4, 1);               /* in the window: kind 4 / class 1 (up and down, bytes) */
    test_ramp_tq(1500, 2, 0);            /* above the upper torque limit: kind 2 / class 0 */
    test_override();
    test_bd1b();
    test_counters();
    test_swap();
    test_ramp_obj();
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
