/* test_diag_realfb.c — HOST DRY-RUN: the output diagnosis fed by the REAL
 * feedback chain instead of the stand-in (2026-11-18,
 * ~/tcu_sweep/diag_real_feedback_dryrun.md). TEST ONLY: no firmware source
 * changes; the switch is made here, at the 0x838B0 slot.
 *
 * The real supervisor (test_ladder.c's chip / switched-output plant, L3b —
 * the target configuration) + the real bus-A chain (drv_jobs / drv_reqlaw /
 * drv_odiag) in main.c's schedule (test_fullon.c's loop: 2.5 ms monitor,
 * 5 ms batch at ms 2, 1 ms diag poll, bus A one frame per 12 µs), plus the
 * feedback shadow (app/fb_shadow.c) on the Timer F / Timer D model
 * (tests/unit/fb_model.c). app_out_head_hook is defined HERE (fb_hook.c is
 * not linked): it gathers the live cells by value exactly as fb_hook.c does,
 * runs the shadow, and — when s_real is set — copies the shadow's outputs
 * into the live cells the stand-in just wrote (0x93A2E class, 0x9340C duty,
 * 0x93420 filtered duty, 0x93394 estimate, 0x933A8 resistance), so the law
 * (0x89F5C / 0x8E8E4 / 0x8E7B4 / 0x84644 / 0x897AC / 0x8A788) runs on them.
 *
 * THE BENCH FEEDBACK WAVEFORMS (r04a captures, r04d recorder), a function
 * of what each driver chip last received on the wire (its 0x88 mode
 * register and its E8 / F0 setpoints, as the chip model stores them):
 *   - mode code 1 ('2', the pre-'U' off content 0x15) → pin held LOW
 *     (r04d: class 0x20 at 1000 ‰ on all eight populated channels);
 *   - code 3 ('C', the phase-0x10 full-on tick) → LOW;
 *   - code 0 (disabled, after 'U') → HIGH (0x21);
 *   - code 2 ('!') → 1 kHz PWM, low time ‰ = 46 + 0.04775·I (0.1 mA) on the
 *     ≈ 7 Ω channels (500 → 70 ‰, 8500 → 452 ‰: r04a A 446–454, B/WK/Sys
 *     69–71), 46 + 0.0882·I on the release (10000 → 928 ‰, r04a); I = 0 →
 *     HIGH; optional turn-on saturation: pin LOW for SAT ms after a channel
 *     goes from not-'!' to '!' or its setpoint rises (an L/R current rise
 *     at full duty — not measured on the bench; run at 0 and 30 / 120 ms);
 *   - the empty fifth select (ch 8/9) → HIGH.
 *
 * Runs (each: cold boot → ladder incl. the phase-0x10 full-on tick → 'U'
 * → the gear-1 P set → the park release cycled as the gs-check lever
 * sequence D, P, R, P — 1 s on, 0.5 s off, 1 s on, 1 s off):
 *   stand-in vs real; mode-1 cadence (the stock switch) and mode 0 held
 *   (test-only: the pending mode request cleared every frame); SAT 0 / 30 /
 *   120 ms. Per law pass the harness records every change of the fault word
 *   0x93004, latched bits 0x94930, report class 0x93994, channel class
 *   0x93A42, the off-test state 0x93A38 / letter 0x93A4C, the law's mode
 *   0x94FD8, the DSM group counters (fail_n / pend_n), the 0x84644 witness
 *   (site 6) and every other would-be reset; and prints them, pass by pass.
 *
 * Checks (the dry-run's verdict, SAT 0, both cadences): the real-feedback
 * run ends with no fault word, no switched-off channel, no failed DSM
 * report and no would-be reset, like the stand-in run. */
#define main test_ladder_main
#include "test_ladder.c"
#undef main

#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_odiag.h"
#include "app/fb_shadow.h"
#include "app/fb_cal.h"
#include "tests/unit/fb_model.h"

#include <stdlib.h>

#if defined(DIAG_TEMPS)
/* STAGE 1b — THE RISK GATE (2026-11-18, ~/tcu_sweep/diag_temp_shadow_port.md):
 * built -DDIAG_TEMPS (test_diag_temps) the same dry run runs with the
 * diagnosis's temperature gate OPEN — 0x940FC forced valid (8 / 6, or
 * following the ECU-temperature shadow's own status) and the shadow's
 * selected temperature 0x9393A fed to the feedback chain in place of its
 * 0x0215 stand-in (fb_shadow.c compiled -DFBS_9393A_HOST_SRC) — over the
 * full sequence: cold boot → ladder (the phase-0x10 full-on tick) → 'U' with
 * A's fill → the gear-1 P set → the park release D/P/R/P → the garage-shift
 * shadow's element trajectories (P→D: C third, B first; P→R: D third). Per
 * law pass it records the bounds 0x93510 / 0x93524 the check used (the
 * previous pass's — 0x8A788 runs after 0x89F5C), the estimate 0x93394 and
 * the tested bits 0x94944, so the margin of every TESTED pass, transients
 * included, is reported per channel. Verdict: zero 0x100 / 0x200 anywhere
 * (fault word, latched 0x94930), no switch-off, no DSM report, no would-be
 * reset. Negative: D's estimate forced to 7000 (< 7300) must set 0x100 on
 * ch 1 while D's request, law mode and chip setpoint stay on. */
#include "app/tsel_shadow.h"
volatile int16_t g_fbs_9393a_host = 0x0215;
#endif

/* ── the bench's four healthy driver chips (test_fullon.c's model) ─────── */
static uint8_t  f_reg8[5][256];
static uint16_t f_e8[5], f_f0[5];

static uint16_t drv_chip(uint16_t tx, uint8_t c)
{
    const uint8_t top5 = (uint8_t)(tx >> 11);
    const uint8_t cmd = (uint8_t)(tx >> 8);
    const uint8_t d = (uint8_t)tx;
    if (c >= 4u) {
        return 0x0000u;
    }
    switch (top5) {
    case 0x1Du: f_e8[c] = (uint16_t)(tx & 0x7FFu); return 0xA000u;
    case 0x1Eu: f_f0[c] = (uint16_t)(tx & 0x7FFu); return 0xA000u;
    case 0x18u: return (uint16_t)(0xA000u | f_e8[c]);
    case 0x1Bu: return (uint16_t)(0xA000u | f_f0[c]);
    default: break;
    }
    switch (cmd) {
    case 0x88u: case 0x8Eu: case 0x90u: case 0x96u: case 0xA0u: case 0xA6u:
        f_reg8[c][cmd] = d;
        return 0xA200u;
    case 0x28u: return (uint16_t)(0xA200u | 0x80u | f_reg8[c][0x88]);
    case 0x30u: return (uint16_t)(0xA200u | f_reg8[c][0x90]);
    case 0x2Eu: return (uint16_t)(0xA200u | f_reg8[c][0x8E]);
    case 0x36u: return (uint16_t)(0xA200u | f_reg8[c][0x96]);
    case 0x60u: return (uint16_t)(0xA200u | f_reg8[c][0xA0]);
    case 0x66u: return (uint16_t)(0xA200u | f_reg8[c][0xA6]);
    case 0x18u: case 0x1Eu: return 0xA200u;
    case 0x06u: return 0xA212u;
    case 0x00u: return 0xA2BDu;
    default: return 0xA200u;
    }
}

static void drain_b(void)
{
    unsigned guard = 512u;
    while (stub_rspi_busy(1u) && guard-- != 0u) {
        unsigned before = stub_log_n;
        stub_rspi_isr(1u);
        if (stub_log_n != before && stub_log[before].bus == 1u &&
            stub_log[before].job == JOB_DUTY &&
            (stub_log[before].word & 0xFF00u) == 0x8800u &&
            (before == 0u || stub_log[before - 1u].job != JOB_DUTY ||
             (stub_log[before - 1u].word & 0xFF00u) != 0x8800u)) {
            if (!chip.ignore_tokens && chip.level != 0u) {
                chip.level--;
            }
            chip.sel = (uint8_t)((chip.sel + 1u) & 0x0Fu);
        }
    }
}

static bool s_a_was_busy;
static uint32_t s_a_due;
static void bus_a_step(void)
{
    const bool busy = stub_rspi_busy(0u);
    if (busy && !s_a_was_busy) {
        s_a_due = stub.frt_us + 12u;
    }
    s_a_was_busy = busy;
    if (busy && (int32_t)(stub.frt_us - s_a_due) >= 0) {
        stub_rspi_isr(0u);
        s_a_due += 12u;
        s_a_was_busy = stub_rspi_busy(0u);
        if (s_a_was_busy && !busy) {
            s_a_due = stub.frt_us + 12u;
        }
    }
}

/* ── the feedback waveform model ───────────────────────────────────────── */
static unsigned s_sat_ms;
static uint8_t  s_code[10];         /* last mode code per channel */
static uint16_t s_sp[10];           /* last setpoint ×6 (0.1 mA) */
static uint32_t s_sat_until[10];    /* µs */
static uint32_t s_wave[10];         /* 0 high, 0xFFFFFFFF low, else low counts */
static bool     s_c1_low_always;    /* '2' (code 1) LOW even with the rail closed */

static uint8_t tf_of(uint8_t ch) { return FB_PINS[3u * (ch >> 1) + (ch & 1u)]; }

static void fb_wave_step(void)
{
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        const uint8_t c = (uint8_t)(ch >> 1);
        uint8_t code = 0u;
        uint16_t sp = 0u;
        uint32_t w;
        if (c < 4u) {
            code = (uint8_t)((f_reg8[c][0x88] >> (((ch & 1u) != 0u) ? 2u : 0u)) & 3u);
            sp = (uint16_t)(6u * (((ch & 1u) != 0u) ? f_f0[c] : f_e8[c]));
        }
        if (code == 2u && sp != 0u && (s_code[ch] != 2u || sp > s_sp[ch] + 300u)) {
            s_sat_until[ch] = stub.frt_us + 1000u * s_sat_ms;
        }
        s_code[ch] = code;
        s_sp[ch] = sp;
        if (c >= 4u || code == 0u) {
            w = 0u;
        } else if (code == 3u) {
            w = 0xFFFFFFFFu;
        } else if (code == 1u) {
            /* '2': LOW with the rail open (r04d, pre-'U'); with the rail
             * closed [I] HIGH — the stock's own off test (0x8E8E4 state 4)
             * passes only on 0x21 / duty 0 after commanding '2' */
            w = (s_c1_low_always || CELL_MUX_MV[0] < 8000u) ? 0xFFFFFFFFu : 0u;
        } else if (sp == 0u) {
            w = 0u;
        } else if (s_sat_ms != 0u && (int32_t)(stub.frt_us - s_sat_until[ch]) < 0) {
            w = 0xFFFFFFFFu;
        } else {
            const float k = (ch == 7u) ? 0.0882f : 0.04775f;
            uint32_t pm = (uint32_t)(46.0f + k * (float)sp);
            if (pm > 990u) {
                pm = 990u;
            }
            w = 4u * pm;
        }
        if (w != s_wave[ch]) {
            s_wave[ch] = w;
            if (w == 0u) {
                fbm_set_static(tf_of(ch), true);
            } else if (w == 0xFFFFFFFFu) {
                fbm_set_static(tf_of(ch), false);
            } else {
                fbm_set_pwm(tf_of(ch), 4000u, w, 400u * c);    /* the chips' stagger */
            }
        }
    }
    fbm_advance_to(stub.frt_us);
}

/* ── the switch at the 0x838B0 slot (test-only) ────────────────────────── */
static bool s_real;
static bool s_temp_valid;
#if defined(DIAG_TEMPS)
static uint8_t  s_fc_mode;          /* 8 / 6 forced, 1 = the shadow's status */
static int16_t  s_brd_raw;          /* the board sensor's word (1/128 °C) */
static uint16_t s_die_n;            /* the J3 average */
static bool     s_neg_d;            /* force D's estimate to 7000 from U+100 */
static unsigned s_frame, s_uframe;
static unsigned s_neg_set;
/* the hub slot (sup_batch.c, after drv_temp_tick): the shadow on the bench's
 * inputs, by value — no J3 / J4 reply in the first frame, the board's
 * not-ready word in the second, then the scenario's values */
void app_temp_hub_hook(void);
void app_temp_hub_hook(void)
{
    tsel_in_t in;
    memset(&in, 0, sizeof in);
    in.t_us = stub.frt_us;
    in.n908ca = (s_frame <= 1u) ? 0u : s_die_n;
    in.r908cc = (s_frame <= 1u) ? 0 : (s_frame == 2u) ? (int16_t)0xFF03 : s_brd_raw;
    in.t94f98 = (int16_t)(s_temp_valid ? 574 : 0);
    in.st94fee = (uint8_t)(s_temp_valid ? 8u : 0u);
    in.n5 = (uint8_t)((CELL_OS_MODE == 0u) ? 1u : 2u);
    in.fm_ready = 1u;
    in.os_mode = CELL_OS_MODE;
    tsel_shadow_pass(&in);
}
#endif
void app_out_head_hook(void);
void app_out_head_hook(void)
{
    fbs_in_t in;
    const volatile fbs_t *s;
#if defined(DIAG_TEMPS)
    /* STAGE 2a, modelled: the selection's cells live — 0x9393A / 0x940FC
     * (forced valid, or the shadow's own status) and the feedback chain's
     * a-curve / R_drv temperature from 0x9393A */
    {
        const volatile tsel_t *t = tsel_shadow_state();
        g_fbs_9393a_host = t->s93a;
        *(volatile int16_t *)(uintptr_t)0xFFF9393Au = t->s93a;
        *(volatile uint8_t *)(uintptr_t)0xFFF940FCu = (s_fc_mode == 1u) ? t->s0fc : s_fc_mode;
    }
#endif
    /* the fluid temperature: invalid until the ADC chain (r02l: 500 ms) */
    *(volatile int16_t *)(uintptr_t)0xFFF94F9Cu = (int16_t)(s_temp_valid ? 574 : 0);
    *(volatile uint8_t *)(uintptr_t)0xFFF94FF0u = (uint8_t)(s_temp_valid ? 8u : 0u);
    /* fb_hook.c's gather, verbatim */
    in.n5 = (uint8_t)((CELL_OS_MODE == 0u) ? 1u : 2u);
    in.rail_mv = *(volatile uint16_t *)(uintptr_t)0xFFF94ED0u;
    in.t_fluid = *(volatile int16_t *)(uintptr_t)0xFFF94F9Cu;
    in.t_st = *(volatile uint8_t *)(uintptr_t)0xFFF94FF0u;
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        in.i_req[ch] = *(volatile int16_t *)(uintptr_t)(0xFFF94ED4u + 2u * ch);
        in.si_cls[ch] = *(volatile uint8_t *)(uintptr_t)(0xFFF93A2Eu + ch);
        in.si_duty[ch] = *(volatile uint16_t *)(uintptr_t)(0xFFF9340Cu + 2u * ch);
        in.si_est[ch] = *(volatile uint16_t *)(uintptr_t)(0xFFF93394u + 2u * ch);
        in.si_res[ch] = *(volatile uint16_t *)(uintptr_t)(0xFFF933A8u + 2u * ch);
        in.mode[ch] = *(volatile uint8_t *)(uintptr_t)(0xFFF94FD8u + ch);
    }
    in.seq2 = CELL_SEQ2_STATE[0];
    in.os_mode = CELL_OS_MODE;
    in.phase = CELL_LAD_PHASE;
    in.f114 = CELL_LAD_94114;
    in.f115 = CELL_DIAG_94115;
    fb_shadow_tick(&in);
    if (!s_real) {
        return;
    }
    /* THE SWITCH (the recommended firmware change, modelled): the real
     * chain's outputs replace the stand-in's in the consumers' cells */
    s = fb_shadow_state();
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        *(volatile uint8_t  *)(uintptr_t)(0xFFF93A2Eu + ch) = s->cls[ch];
        *(volatile uint16_t *)(uintptr_t)(0xFFF9340Cu + 2u * ch) = s->duty[ch];
        *(volatile uint16_t *)(uintptr_t)(0xFFF93420u + 2u * ch) = s->dutyf[ch];
        *(volatile uint16_t *)(uintptr_t)(0xFFF93394u + 2u * ch) = s->estm[ch];
        *(volatile uint16_t *)(uintptr_t)(0xFFF933A8u + 2u * ch) = s->res[ch];
    }
#if defined(DIAG_TEMPS)
    if (s_neg_d && s_uframe != 0u && s_frame >= s_uframe + 100u && s_frame < s_uframe + 400u) {
        *(volatile uint16_t *)(uintptr_t)(0xFFF93394u + 2u) = 7000u;   /* D (ch 1) below 7300 */
        s_neg_set++;
    }
#endif
}

/* ── the observed state per law pass ───────────────────────────────────── */
#define U8C_(a)  (*(volatile uint8_t *)(uintptr_t)(a))
#define U16C_(a) (*(volatile uint16_t *)(uintptr_t)(a))
typedef struct {
    uint16_t fw[10], latched[10];
    uint8_t  rcls[10], ccls[10], ost[10], olet[10], mode[10], state[10], fb[10];
    uint32_t gfail[21], gpend[21];
    uint32_t fatal_site[12];
} obs_t;

static void observe(obs_t *o)
{
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        o->fw[ch] = U16C_(0xFFF93004u + 2u * ch);
        o->latched[ch] = U16C_(0xFFF94930u + 2u * ch);
        o->rcls[ch] = U8C_(0xFFF93994u + ch);
        o->ccls[ch] = U8C_(0xFFF93A42u + ch);
        o->ost[ch] = U8C_(0xFFF93A38u + ch);
        o->olet[ch] = U8C_(0xFFF93A4Cu + ch);
        o->mode[ch] = U8C_(0xFFF94FD8u + ch);
        o->state[ch] = U8C_(0xFFF93B0Cu + ch);
        o->fb[ch] = U8C_(0xFFF93A2Eu + ch);
    }
    for (uint8_t g = 1u; g <= 20u; g++) {
        o->gfail[g] = drv_od_group(g)->fail_n;
        o->gpend[g] = drv_od_group(g)->pend_n;
    }
    for (unsigned k = 0u; k < 12u; k++) {
        o->fatal_site[k] = drv_od_fatal()->site_n[k];
    }
}

typedef struct {
    const char *name;
    bool real;
    bool mode0_held;
    unsigned sat_ms;
    unsigned parity;
    bool verbose;
    bool fill;              /* ch 0 (A) runs the r03p boot fill after 'U' */
    bool c1low;             /* '2' LOW even with the rail closed */
    uint8_t fc_mode;        /* DIAG_TEMPS: 0x940FC 8 / 6 forced, 1 = the shadow's status */
    bool gs;                /* DIAG_TEMPS: the garage-shift shadow's element trajectories */
    bool neg_d;             /* DIAG_TEMPS: D's estimate forced to 7000 */
    int16_t brd_raw;        /* DIAG_TEMPS: the board word (0 → 0x164B, r04f) */
    bool info;              /* DIAG_TEMPS: informational run (not part of the verdict) */
    uint16_t die_n;         /* DIAG_TEMPS: the J3 average (0 → 433, r04f) */
    /* results */
    unsigned u_frame, sw_frame, passes, events;
    unsigned fw_set_events, gfail_tot, gpend_tot, fatal_tot, f84644, switched_off;
    unsigned fb_hist[10][6];        /* consumer-seen class counts after 'U': 00 10 20 21 31 other */
    uint16_t fw_or[10];             /* OR of every fault word seen */
    uint8_t  ccls_max[10];
    /* DIAG_TEMPS: the bound check's own inputs per tested pass */
    uint16_t ub_lo[10], ub_hi[10], lb_lo[10], lb_hi[10];   /* nonzero bounds seen (min / max) */
    int32_t  mup[10], mlo[10];          /* min margin, tested passes (UB − est / est − LB) */
    unsigned mup_f[10], mlo_f[10];      /* the frame (U+) of that minimum */
    uint16_t mup_est[10], mlo_est[10], mup_b[10], mlo_b[10];
    unsigned tup_n[10], tlo_n[10];      /* tested passes */
    int32_t  aup[10], alo[10];          /* min margin over ALL passes after 'U' with that bound != 0 */
    uint16_t bits_or[10];               /* OR of (fault word | 0x94930) & 0x300 */
    uint16_t ub_prev[10], lb_prev[10];
    unsigned neg_ch1_100;               /* passes with 0x100 in ch 1's fault word / 0x94930 */
    uint8_t  end_mode1, end_code1;      /* D's law mode / chip code at the end */
    uint16_t end_req1, end_sp1;
    uint8_t  end_sel_st, end_sel_src;
    int16_t  end_sel_t;
    uint16_t est_ss[10], ub_ss[10], lb_ss[10];  /* the steady P set at U+38 (after A's fill, before the release) */
    bool ss_done;
} run_r_t;

static unsigned cls_idx(uint8_t c)
{
    switch (c) {
    case 0x00u: return 0u;
    case 0x10u: return 1u;
    case 0x20u: return 2u;
    case 0x21u: return 3u;
    case 0x31u: return 4u;
    default: return 5u;
    }
}

/* the gs-check lever sequence's effect on the live path: the park release
 * (ch 7) — D on 1 s, P off 0.5 s, R on 1 s, P off 1 s (k = frames after 'U') */
static uint16_t rel_at(unsigned k)
{
    if (k >= 40u && k < 240u) {
        return 10000u;
    }
    if (k >= 340u && k < 540u) {
        return 10000u;
    }
    return 0u;
}

/* the r03p gear-1 fill on A (recorder s_m5_rec): boost 8500 one pass,
 * 5305 for 9 passes, blend 4609 one pass, then 8500 — at the mode-1 10 ms
 * producer cadence (frames of 5 ms after 'U'; starting at U+4) */
static uint16_t fill_at(unsigned k)
{
    if (k < 8u) {
        return 500u;            /* the r03f value, before the producer's first pass */
    }
    if (k < 10u) {
        return 8500u;           /* pass 0: the boost */
    }
    if (k < 28u) {
        return 5305u;           /* passes 1..9 */
    }
    if (k < 30u) {
        return 4609u;           /* pass 10: the blend */
    }
    return 8500u;               /* pass 11 on: applied */
}

#if defined(DIAG_TEMPS)
/* the garage-shift shadow's element trajectories on the live requests
 * (0.1 mA; ch 0 A, 1 D, 2 B, 3 E, 4 C), from the gs_shadow host trace
 * (CLAUDE r03x: commit 150 ms after the range change; C boost 500 / fill
 * 5932 / applied 6850; B boost 8500 / fill 5709 / phase 4 3630 / ramp 19
 * passes; end P→D A D B E 8500, C 500; P→R D boost 500 / fill 4710 /
 * applied 6560, end D 500, B 8500) [D, scripted at the 10 ms producer
 * cadence = 2 frames per pass; k = frames after 'U'; the release lever
 * steps at k 40 (D), 240 (P), 340 (R), 540 (P), as rel_at] */
static uint16_t gs_at(uint8_t ch, unsigned k)
{
    static const uint16_t PSET[5] = { 8500u, 8500u, 500u, 8500u, 8500u };
    unsigned j;
    bool rev;
    if (k >= 70u && k < 240u) {
        j = (k - 70u) / 2u;
        rev = false;
    } else if (k >= 370u && k < 540u) {
        j = (k - 370u) / 2u;
        rev = true;
    } else {
        return PSET[ch];
    }
    if (ch == 2u) {                         /* B, first element */
        if (j < 11u) { return 500u; }
        if (j == 11u) { return 8500u; }
        if (j <= 21u) { return 5709u; }
        if (j <= 31u) { return 3630u; }
        if (j <= 50u) { return (uint16_t)(3630u + (8500u - 3630u) * (j - 31u) / 19u); }
        return 8500u;
    }
    if (!rev && ch == 4u) {                 /* C, third element of 1st */
        if (j == 0u) { return 500u; }
        if (j <= 10u) { return 5932u; }
        if (j <= 50u) { return 6850u; }
        return 500u;
    }
    if (rev && ch == 1u) {                  /* D, third element of R */
        if (j == 0u) { return 500u; }
        if (j <= 10u) { return 4710u; }
        if (j <= 50u) { return 6560u; }
        return 500u;
    }
    return PSET[ch];
}
#endif

static void rr_reset(const run_r_t *r)
{
    static const uint16_t P[10] = { 8500u, 8500u, 500u, 8500u, 8500u, 500u, 500u, 0u, 0u, 0u };
    reset_all();
    plant.enabled = true;
    plant.lag = 0u;
    plant.never_on = false;
    plant.ignore_level = false;
    memset(f_reg8, 0, sizeof f_reg8);
    memset(f_e8, 0, sizeof f_e8);
    memset(f_f0, 0, sizeof f_f0);
    stub.drv_reply_fn = drv_chip;
    drv_od_host_seed_select(false);
    drv_jobs_init();
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        drv_rq_request_set(ch, P[ch]);
    }
    if (r->fill) {
        drv_rq_request_set(0u, 500u);
    }
    s_a_was_busy = false;
    stub_log_n = 0u;
    /* the feedback hardware: F1's configuration, pins high at power-on */
    fbm_reset();
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        fbm_config(tf_of(ch), (uint8_t)(((ch & 1u) == 0u) ? 1u : 2u));
        fbm_set_static(tf_of(ch), true);
        s_code[ch] = 0u;
        s_sp[ch] = 0u;
        s_wave[ch] = 0u;
        s_sat_until[ch] = 0u;
    }
    fbm_advance_to(stub.frt_us);
    fb_shadow_init();
    fb_shadow_start();
    s_real = r->real;
    s_sat_ms = r->sat_ms;
    s_temp_valid = false;
    s_c1_low_always = r->c1low;
#if defined(DIAG_TEMPS)
    tsel_shadow_init();
    g_fbs_9393a_host = 0x0780;              /* the init value until the first hub pass */
    *(volatile int16_t *)(uintptr_t)0xFFF9393Au = 0x0780;
    *(volatile uint8_t *)(uintptr_t)0xFFF940FCu = 0u;
    s_fc_mode = r->fc_mode;
    s_brd_raw = (r->brd_raw != 0) ? r->brd_raw : 0x164B;
    s_die_n = (r->die_n != 0u) ? r->die_n : 433u;
    s_neg_d = r->neg_d;
    s_frame = 0u;
    s_uframe = 0u;
    s_neg_set = 0u;
#endif
}

static void diff_print(run_r_t *r, unsigned p, const obs_t *a, const obs_t *b)
{
    char buf[1024];
    int n = 0;
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        if (a->fw[ch] != b->fw[ch]) {
            n += snprintf(buf + n, sizeof buf - (size_t)n, " ch%u fw %04X>%04X", ch, a->fw[ch], b->fw[ch]);
            if (b->fw[ch] != 0u) {
                r->fw_set_events++;
            }
        }
        if (a->latched[ch] != b->latched[ch]) {
            n += snprintf(buf + n, sizeof buf - (size_t)n, " ch%u 94930 %04X>%04X", ch, a->latched[ch], b->latched[ch]);
        }
        if (a->rcls[ch] != b->rcls[ch]) {
            n += snprintf(buf + n, sizeof buf - (size_t)n, " ch%u rcls %02X>%02X", ch, a->rcls[ch], b->rcls[ch]);
        }
        if (a->ccls[ch] != b->ccls[ch]) {
            n += snprintf(buf + n, sizeof buf - (size_t)n, " ch%u ccls %u>%u", ch, a->ccls[ch], b->ccls[ch]);
            if (b->ccls[ch] == 4u) {
                r->switched_off++;
            }
        }
        if (a->ost[ch] != b->ost[ch] || a->olet[ch] != b->olet[ch]) {
            n += snprintf(buf + n, sizeof buf - (size_t)n, " ch%u ost %u/%02X>%u/%02X", ch,
                          a->ost[ch], a->olet[ch], b->ost[ch], b->olet[ch]);
        }
        if (n > 900) {
            break;
        }
    }
    for (uint8_t g = 1u; g <= 20u && n < 900; g++) {
        if (a->gfail[g] != b->gfail[g]) {
            n += snprintf(buf + n, sizeof buf - (size_t)n, " g%u FAIL+%u(mon %u)", g,
                          (unsigned)(b->gfail[g] - a->gfail[g]), drv_od_group(g)->monitor);
            r->gfail_tot += b->gfail[g] - a->gfail[g];
        }
        if (a->gpend[g] != b->gpend[g]) {
            n += snprintf(buf + n, sizeof buf - (size_t)n, " g%u pend+%u", g,
                          (unsigned)(b->gpend[g] - a->gpend[g]));
            r->gpend_tot += b->gpend[g] - a->gpend[g];
        }
    }
    for (unsigned k = 0u; k < 12u && n < 900; k++) {
        if (a->fatal_site[k] != b->fatal_site[k]) {
            n += snprintf(buf + n, sizeof buf - (size_t)n, " WOULD-RESET site %u +%u (ch %u)", k,
                          (unsigned)(b->fatal_site[k] - a->fatal_site[k]), drv_od_fatal()->ch);
            r->fatal_tot += b->fatal_site[k] - a->fatal_site[k];
            if (k == 6u) {
                r->f84644 += b->fatal_site[k] - a->fatal_site[k];
            }
        }
    }
    if (n > 0) {
        r->events++;
        if (r->verbose && r->events <= 60u) {
            printf("    F%u (law pass %u, seq2 %02X mode %u, U+%d):%s\n", p, r->passes,
                   CELL_SEQ2_STATE[0], CELL_OS_MODE, r->u_frame ? (int)(p - r->u_frame) : -1, buf);
        }
    }
}

#if defined(DIAG_TEMPS)
/* after a law pass: the check 0x89F5C used this pass's estimate and the
 * PREVIOUS pass's bounds (0x8A788 runs after it in the law loop) */
static void bounds_track(run_r_t *r, unsigned p)
{
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        const uint16_t est = U16C_(0xFFF93394u + 2u * ch);
        const uint16_t tested = U16C_(0xFFF94944u + 2u * ch);
        const uint16_t ub = U16C_(0xFFF93510u + 2u * ch), lb = U16C_(0xFFF93524u + 2u * ch);
        const uint16_t fw = (uint16_t)(U16C_(0xFFF93004u + 2u * ch) | U16C_(0xFFF94930u + 2u * ch));
        const unsigned k = (r->u_frame != 0u) ? p - r->u_frame : 0u;
        r->bits_or[ch] = (uint16_t)(r->bits_or[ch] | (fw & 0x300u));
        if (ch == 1u && (fw & 0x100u) != 0u) {
            r->neg_ch1_100++;
        }
        if ((tested & 0x200u) != 0u && r->ub_prev[ch] != 0u) {
            const int32_t m = (int32_t)(int16_t)r->ub_prev[ch] - (int32_t)(int16_t)est;
            r->tup_n[ch]++;
            if (m < r->mup[ch]) {
                r->mup[ch] = m; r->mup_f[ch] = k; r->mup_est[ch] = est; r->mup_b[ch] = r->ub_prev[ch];
            }
        }
        if ((tested & 0x100u) != 0u && r->lb_prev[ch] != 0u) {
            const int32_t m = (int32_t)(int16_t)est - (int32_t)(int16_t)r->lb_prev[ch];
            r->tlo_n[ch]++;
            if (m < r->mlo[ch]) {
                r->mlo[ch] = m; r->mlo_f[ch] = k; r->mlo_est[ch] = est; r->mlo_b[ch] = r->lb_prev[ch];
            }
        }
        if (r->u_frame != 0u) {
            if (r->ub_prev[ch] != 0u) {
                const int32_t m = (int32_t)(int16_t)r->ub_prev[ch] - (int32_t)(int16_t)est;
                if (m < r->aup[ch]) { r->aup[ch] = m; }
            }
            if (r->lb_prev[ch] != 0u) {
                const int32_t m = (int32_t)(int16_t)est - (int32_t)(int16_t)r->lb_prev[ch];
                if (m < r->alo[ch]) { r->alo[ch] = m; }
            }
        }
        if (ub != 0u) {
            if (ub < r->ub_lo[ch]) { r->ub_lo[ch] = ub; }
            if (ub > r->ub_hi[ch]) { r->ub_hi[ch] = ub; }
        }
        if (lb != 0u) {
            if (lb < r->lb_lo[ch]) { r->lb_lo[ch] = lb; }
            if (lb > r->lb_hi[ch]) { r->lb_hi[ch] = lb; }
        }
        if (k >= 38u && r->u_frame != 0u && !r->ss_done) {
            r->est_ss[ch] = est;
            r->ub_ss[ch] = r->ub_prev[ch];
            r->lb_ss[ch] = r->lb_prev[ch];
            if (ch == 9u) {
                r->ss_done = true;
            }
        }
        r->ub_prev[ch] = ub;
        r->lb_prev[ch] = lb;
    }
}

static void bounds_summary(const run_r_t *r)
{
    const volatile tsel_t *t = tsel_shadow_state();
    printf("    temperature: 0x940FC %s, selection at the end %d (%.1f C) status %u source %u; "
           "first status 8 at %u us; bound bits seen:",
           r->fc_mode == 1u ? "= the shadow's status" : (r->fc_mode == 6u ? "forced 6" :
                                                 (r->fc_mode == 0u ? "0 (gate closed)" : "forced 8")),
           t->s93a, (double)t->s93a / 16.0, t->s0fc, t->s170, (unsigned)t->first_sel8_us);
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        printf(" %04X", r->bits_or[ch]);
    }
    printf("\n    steady P set (U+38) est / UB / LB:");
    for (uint8_t ch = 0u; ch < 5u; ch++) {
        printf("  ch%u %u/%u/%u", ch, r->est_ss[ch], r->ub_ss[ch], r->lb_ss[ch]);
    }
    printf("\n    ch  UB seen (min..max) tested min-margin (U+, est/UB)   | LB seen (min..max) tested min-margin "
           "(U+, est/LB)   | all-pass min UB/LB margin\n");
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        char up[64], lo[64], ua[16], la[16];
        if (r->tup_n[ch] != 0u) {
            snprintf(up, sizeof up, "%5d (U+%u, %u/%u) n=%u", (int)r->mup[ch], r->mup_f[ch], r->mup_est[ch],
                     r->mup_b[ch], r->tup_n[ch]);
        } else {
            snprintf(up, sizeof up, "untested");
        }
        if (r->tlo_n[ch] != 0u) {
            snprintf(lo, sizeof lo, "%5d (U+%u, %u/%u) n=%u", (int)r->mlo[ch], r->mlo_f[ch], r->mlo_est[ch],
                     r->mlo_b[ch], r->tlo_n[ch]);
        } else {
            snprintf(lo, sizeof lo, "untested");
        }
        if (r->aup[ch] != 0x7FFFFFFF) { snprintf(ua, sizeof ua, "%d", (int)r->aup[ch]); } else { snprintf(ua, sizeof ua, "-"); }
        if (r->alo[ch] != 0x7FFFFFFF) { snprintf(la, sizeof la, "%d", (int)r->alo[ch]); } else { snprintf(la, sizeof la, "-"); }
        printf("    %u   %5u..%-5u %-36s | %5u..%-5u %-36s | %s / %s\n", ch,
               r->ub_lo[ch] == 0xFFFFu ? 0u : r->ub_lo[ch], r->ub_hi[ch], up,
               r->lb_lo[ch] == 0xFFFFu ? 0u : r->lb_lo[ch], r->lb_hi[ch], lo, ua, la);
    }
}
#endif

static void run_one(run_r_t *r, unsigned frames)
{
    obs_t prev, now;
    uint32_t law0;
    rr_reset(r);
    observe(&prev);
    law0 = drv_out_wit()->law_n;
#if defined(DIAG_TEMPS)
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        r->mup[ch] = r->mlo[ch] = r->aup[ch] = r->alo[ch] = 0x7FFFFFFF;
        r->ub_lo[ch] = r->lb_lo[ch] = 0xFFFFu;
    }
#endif
    for (unsigned p = 1u; p <= frames; p++) {
#if defined(DIAG_TEMPS)
        s_frame = p;
        s_uframe = r->u_frame;
#endif
        if (p == 100u + r->parity) {
            stub.adc_ready = true;
            s_temp_valid = true;
        }
        if (r->u_frame != 0u) {
            drv_rq_request_set(7u, rel_at(p - r->u_frame));
            if (r->fill) {
                drv_rq_request_set(0u, fill_at(p - r->u_frame));
            }
#if defined(DIAG_TEMPS)
            if (r->gs) {
                for (uint8_t ch = 1u; ch < 5u; ch++) {
                    drv_rq_request_set(ch, gs_at(ch, p - r->u_frame));
                }
            }
#endif
        }
        inputs();
        for (unsigned step = 0u; step < 500u; step++) {
            const unsigned us = step * 10u;
            if (us % 1000u == 0u) {
                drv_od_diag_poll(stub.frt_us);
            }
            if (us == 0u || us == 3000u) {
                sup_monitor_poll();
                drain_b();
            }
            if (us == 2000u) {
                sup_batch_5ms();
                drain_b();
                if (CELL_SEQ2_STATE[0] == (uint8_t)'U' && r->u_frame == 0u) {
                    r->u_frame = p;
                }
                if (CELL_OS_MODE == 1u && r->sw_frame == 0u) {
                    r->sw_frame = p;
                }
                if (r->mode0_held && CELL_OS_MODE != 0u) {
                    /* test-only: the switch was applied (0x848CF = 1, so
                     * 0x66E06 never re-applies); the scheduler mode cell
                     * goes back to 0 → the mode-0 cadence (law every 5 ms,
                     * 5 ms pass budgets) after 'U' */
                    CELL_OS_MODE = 0u;
                }
                if (drv_out_wit()->law_n != law0) {
                    law0 = drv_out_wit()->law_n;
                    r->passes++;
                    observe(&now);
                    diff_print(r, p, &prev, &now);
                    if (r->verbose && ((r->u_frame != 0u && p <= r->u_frame + 8u) ||
                                       (r->u_frame != 0u && (rel_at(p - r->u_frame) != rel_at(p - r->u_frame - 1u) ||
                                                              rel_at(p - r->u_frame - 2u) != rel_at(p - r->u_frame - 3u))))) {
                        printf("    F%u U+%u mode %u cls:", p, p - r->u_frame, CELL_OS_MODE);
                        for (uint8_t ch = 0u; ch < 10u; ch++) {
                            printf(" %02X/%02X", now.fb[ch], now.state[ch]);
                        }
                        printf("\n");
                    }
#if defined(DIAG_TEMPS)
                    bounds_track(r, p);
#endif
                    if (r->u_frame != 0u) {
                        for (uint8_t ch = 0u; ch < 10u; ch++) {
                            r->fb_hist[ch][cls_idx(now.fb[ch])]++;
                            r->fw_or[ch] = (uint16_t)(r->fw_or[ch] | now.fw[ch]);
                            if (now.ccls[ch] > r->ccls_max[ch]) {
                                r->ccls_max[ch] = now.ccls[ch];
                            }
                        }
                    }
                    prev = now;
                }
            }
            if (plat_walktmr_running() &&
                (int32_t)(stub.frt_us - stub.walk_fire_us) >= 0) {
                plat_walktmr_stop();
                sup_walk_isr();
                drain_b();
            }
            sup_mode_background();
            bus_a_step();
            stub.frt_us += 10u;
            fb_wave_step();
        }
        stub_log_n = 0u;
    }
#if defined(DIAG_TEMPS)
    r->end_mode1 = U8C_(0xFFF94FD8u + 1u);
    r->end_code1 = s_code[1];
    r->end_req1 = U16C_(0xFFF94ED4u + 2u);
    r->end_sp1 = s_sp[1];
    r->end_sel_st = tsel_shadow_state()->s0fc;
    r->end_sel_src = tsel_shadow_state()->s170;
    r->end_sel_t = tsel_shadow_state()->s93a;
#endif
}

static void summary(const run_r_t *r)
{
    printf("  %-34s 'U' F%u, mode-1 F%u, law passes %u, change events %u | fault-word sets %u, "
           "ch class-4 (switch-off) %u, DSM fail %u pend %u, would-reset %u (84644 %u)\n",
           r->name, r->u_frame, r->sw_frame, r->passes, r->events, r->fw_set_events,
           r->switched_off, r->gfail_tot, r->gpend_tot, r->fatal_tot, r->f84644);
    printf("    consumer-seen class after 'U' (00/10/20/21/31/other) and OR of fault words:\n");
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        printf("      ch%u %5u %5u %5u %5u %5u %5u  fw|=%04X ccls max %u\n", ch,
               r->fb_hist[ch][0], r->fb_hist[ch][1], r->fb_hist[ch][2], r->fb_hist[ch][3],
               r->fb_hist[ch][4], r->fb_hist[ch][5], r->fw_or[ch], r->ccls_max[ch]);
    }
}

static int fails;
#define DCHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

#if defined(DIAG_TEMPS)
int main(int argc, char **argv)
{
    /* the full sequence = fill + release + gs in one run */
    static run_r_t runs[] = {
        { .name = "T8 REAL mode-1 full",            .real = true, .fc_mode = 8u, .fill = true, .gs = true, .verbose = true },
        { .name = "T8 REAL mode-0 full",            .real = true, .fc_mode = 8u, .fill = true, .gs = true, .mode0_held = true },
        { .name = "T8 REAL mode-1 fill+release (stage-2a exposure)", .real = true, .fc_mode = 8u, .fill = true },
        { .name = "T8 REAL mode-0 fill+release (stage-2a exposure)", .real = true, .fc_mode = 8u, .fill = true,
          .mode0_held = true },
        { .name = "T8 REAL mode-1 full par1",       .real = true, .fc_mode = 8u, .fill = true, .gs = true, .parity = 1u },
        { .name = "T6 REAL mode-1 full",            .real = true, .fc_mode = 6u, .fill = true, .gs = true },
        { .name = "TS REAL mode-1 full (shadow st)", .real = true, .fc_mode = 1u, .fill = true, .gs = true },
        { .name = "T8 REAL mode-1 full board 25C",  .real = true, .fc_mode = 8u, .fill = true, .gs = true,
          .brd_raw = 0x0C87, .die_n = 340u },
        { .name = "T8 REAL mode-1 full board 120C", .real = true, .fc_mode = 8u, .fill = true, .gs = true,
          .brd_raw = 0x3C07, .die_n = 794u },
        { .name = "T8 REAL mode-0 full board 120C", .real = true, .fc_mode = 8u, .fill = true, .gs = true,
          .mode0_held = true, .brd_raw = 0x3C07, .die_n = 794u },
        { .name = "T8 stand-in mode-1 full",        .real = false, .fc_mode = 8u, .fill = true, .gs = true },
        { .name = "T8 REAL mode-1 full SAT10 (info)", .real = true, .fc_mode = 8u, .fill = true, .gs = true, .sat_ms = 10u,
          .info = true },
        { .name = "T0 REAL mode-1 full SAT10 (gate closed, info)", .real = true, .fc_mode = 0u, .fill = true, .gs = true,
          .sat_ms = 10u, .info = true },
        { .name = "T8 REAL mode-1 full NEG D 7000", .real = true, .fc_mode = 8u, .fill = true, .gs = true, .neg_d = true,
          .verbose = true, .info = true },
    };
    const unsigned n_runs = sizeof runs / sizeof runs[0];
    const unsigned frames = 900u;
    (void)argc;
    (void)argv;
    for (unsigned k = 0u; k < n_runs; k++) {
        run_r_t *r = &runs[k];
        printf("== %s ==\n", r->name);
        run_one(r, frames);
        summary(r);
        bounds_summary(r);
    }
    /* the verdict: every run but the informational SAT10 and the negative
     * — no bound bit, no fault word, no switch-off, no DSM report, no
     * would-be reset; the bounds were actually tested */
    for (unsigned k = 0u; k < n_runs; k++) {
        const run_r_t *r = &runs[k];
        unsigned tested = 0u;
        if (r->info) {
            continue;
        }
        DCHECK(r->u_frame != 0u, "%s: no 'U'", r->name);
        for (uint8_t ch = 0u; ch < 10u; ch++) {
            DCHECK(r->bits_or[ch] == 0u, "%s: ch %u bound bits %04X", r->name, ch, r->bits_or[ch]);
            tested += r->tup_n[ch] + r->tlo_n[ch];
        }
        DCHECK(tested > 1000u, "%s: the bounds were tested only %u times", r->name, tested);
        DCHECK(r->tlo_n[1] != 0u && r->tlo_n[3] != 0u && r->tlo_n[4] != 0u && r->tup_n[2] != 0u,
               "%s: D/E/C lower and B upper not all tested", r->name);
        for (uint8_t ch = 0u; ch < 10u; ch++) {
            DCHECK((r->tup_n[ch] == 0u || r->mup[ch] > 0) && (r->tlo_n[ch] == 0u || r->mlo[ch] > 0),
                   "%s: ch %u margin %d / %d", r->name, ch, (int)r->mup[ch], (int)r->mlo[ch]);
        }
        DCHECK(r->fw_set_events == 0u && r->switched_off == 0u && r->gfail_tot == 0u &&
               r->gpend_tot == 0u && r->fatal_tot == 0u,
               "%s: fault %u switch-off %u fail %u pend %u fatal %u", r->name, r->fw_set_events,
               r->switched_off, r->gfail_tot, r->gpend_tot, r->fatal_tot);
    }
    DCHECK(runs[6].end_sel_st == 8u && runs[6].end_sel_src == 4u && runs[6].end_sel_t == 713,
           "the shadow's selection at the end: %d / %u / %u", runs[6].end_sel_t, runs[6].end_sel_st,
           runs[6].end_sel_src);
    {
        /* the negative: 0x100 on D, and D still driven */
        const run_r_t *r = &runs[n_runs - 1u];
        printf("NEG: D's 0x100 passes %u (estimate forced on %u head passes); D at the end: law mode %u, "
               "request %u, chip code %u, setpoint %u, class max %u\n",
               r->neg_ch1_100, s_neg_set, r->end_mode1, r->end_req1, r->end_code1, r->end_sp1, r->ccls_max[1]);
        DCHECK((r->bits_or[1] & 0x100u) != 0u && r->neg_ch1_100 > 0u, "NEG: 0x100 not set on D");
        DCHECK((r->bits_or[1] & 0x200u) == 0u, "NEG: 0x200 on D");
        for (uint8_t ch = 0u; ch < 10u; ch++) {
            if (ch != 1u) {
                DCHECK(r->bits_or[ch] == 0u, "NEG: ch %u bound bits %04X", ch, r->bits_or[ch]);
            }
        }
        DCHECK(r->end_mode1 == 3u && r->end_code1 == 2u && r->end_req1 == 8500u && r->ccls_max[1] < 4u,
               "NEG: D not kept on (mode %u code %u req %u class %u)", r->end_mode1, r->end_code1,
               r->end_req1, r->ccls_max[1]);
        DCHECK(r->fatal_tot == 0u, "NEG: would-be reset");
    }
    if (fails == 0) {
        printf("test_diag_temps: ok\n");
    }
    return fails != 0;
}
#else
int main(int argc, char **argv)
{
    static run_r_t runs[] = {
        /* name                                 real   m0    SAT  par  verbose fill */
        { .name = "stand-in mode-1 SAT0",        .real = false, .mode0_held = false, .sat_ms = 0u,  .parity = 0u, .verbose = true,  .fill = false },
        { .name = "REAL     mode-1 SAT0",        .real = true,  .mode0_held = false, .sat_ms = 0u,  .parity = 0u, .verbose = true,  .fill = false },
        { .name = "REAL     mode-1 SAT0 par1",   .real = true,  .mode0_held = false, .sat_ms = 0u,  .parity = 1u, .verbose = false, .fill = false },
        { .name = "stand-in mode-0 SAT0",        .real = false, .mode0_held = true,  .sat_ms = 0u,  .parity = 0u, .verbose = true,  .fill = false },
        { .name = "REAL     mode-0 SAT0",        .real = true,  .mode0_held = true,  .sat_ms = 0u,  .parity = 0u, .verbose = true,  .fill = false },
        { .name = "stand-in mode-1 SAT0 fill",   .real = false, .mode0_held = false, .sat_ms = 0u,  .parity = 0u, .verbose = true,  .fill = true },
        { .name = "REAL     mode-1 SAT0 fill",   .real = true,  .mode0_held = false, .sat_ms = 0u,  .parity = 0u, .verbose = true,  .fill = true },
        { .name = "REAL     mode-0 SAT0 fill",   .real = true,  .mode0_held = true,  .sat_ms = 0u,  .parity = 0u, .verbose = true,  .fill = true },
        { .name = "REAL     mode-1 SAT10 fill",  .real = true,  .mode0_held = false, .sat_ms = 10u, .parity = 0u, .verbose = true,  .fill = true },
        { .name = "REAL     mode-0 SAT10 fill",  .real = true,  .mode0_held = true,  .sat_ms = 10u, .parity = 0u, .verbose = true,  .fill = true },
        { .name = "REAL     mode-1 SAT30 fill",  .real = true,  .mode0_held = false, .sat_ms = 30u, .parity = 0u, .verbose = true,  .fill = true },
        { .name = "REAL     mode-0 SAT30 fill",  .real = true,  .mode0_held = true,  .sat_ms = 30u, .parity = 0u, .verbose = true,  .fill = true },
        { .name = "REAL     mode-1 SAT60 fill",  .real = true,  .mode0_held = false, .sat_ms = 60u, .parity = 0u, .verbose = true,  .fill = true },
        { .name = "REAL     mode-1 SAT30 fill c1low", .real = true, .mode0_held = false, .sat_ms = 30u, .parity = 0u, .verbose = true, .fill = true, .c1low = true },
        { .name = "REAL     mode-0 SAT30 fill c1low", .real = true, .mode0_held = true, .sat_ms = 30u, .parity = 0u, .verbose = false, .fill = true, .c1low = true },
        { .name = "REAL     mode-1 SAT60 fill c1low", .real = true, .mode0_held = false, .sat_ms = 60u, .parity = 0u, .verbose = false, .fill = true, .c1low = true },
        { .name = "REAL     mode-1 SAT120",      .real = true,  .mode0_held = false, .sat_ms = 120u,.parity = 0u, .verbose = false, .fill = false },
    };
    const unsigned frames = 900u;
    if (argc > 1 && strcmp(argv[1], "--sweep") == 0) {
        /* the turn-on / step saturation threshold, fill on A, both cadences */
        for (unsigned m0 = 0u; m0 < 2u; m0++) {
            for (unsigned sat = 0u; sat <= 30u; sat += 2u) {
                run_r_t r;
                memset(&r, 0, sizeof r);
                r.name = m0 ? "sweep mode-0" : "sweep mode-1";
                r.real = true;
                r.mode0_held = m0 != 0u;
                r.sat_ms = sat;
                r.fill = true;
                run_one(&r, 500u);
                printf("%s SAT %2u ms: fault-word sets %u, switch-off %u, DSM fail %u, would-reset %u\n",
                       r.name, sat, r.fw_set_events, r.switched_off, r.gfail_tot, r.fatal_tot);
            }
        }
        return 0;
    }
    for (unsigned k = 0u; k < sizeof runs / sizeof runs[0]; k++) {
        run_r_t *r = &runs[k];
        printf("== %s ==\n", r->name);
        run_one(r, frames);
        summary(r);
    }
    /* the verdict: SAT 0, both cadences, both parities — the real chain
     * changes nothing the diagnosis reports */
    for (unsigned k = 0u; k < 8u; k++) {
        const run_r_t *r = &runs[k];
        DCHECK(r->u_frame != 0u, "%s: no 'U'", r->name);
        DCHECK(r->fw_set_events == 0u && r->switched_off == 0u && r->gfail_tot == 0u &&
               r->gpend_tot == 0u && r->fatal_tot == 0u,
               "%s: fault %u switch-off %u fail %u pend %u fatal %u", r->name, r->fw_set_events,
               r->switched_off, r->gfail_tot, r->gpend_tot, r->fatal_tot);
    }
    DCHECK(runs[1].sw_frame != 0u && runs[1].passes < runs[4].passes, "cadence variants");
    if (fails == 0) {
        printf("test_diag_realfb: ok\n");
    }
    return fails != 0;
}
#endif
