/* test_fb_shadow.c — the READ-ONLY PWM-feedback shadow (app/fb_shadow.c;
 * ~/tcu_sweep/feedback_shadow_port.md), on the Timer F / Timer D model
 * (tests/unit/fb_model.c).
 *
 *   (1) the stock driving dump (2026-11-04) reproduced:
 *       (1a) end to end — PWM waveforms with the dump's low times through
 *            Timer F, the CMID00 sampler, 0x904FA, 0x89A8C: classes 10 ×7 /
 *            21 ×3, duties within ±1 ‰ of the dump's 0x9340C;
 *       (1b) the classifier on the dump's own snapshot 0x9026C / 0x90294;
 *       (1c) the estimate 0x8390C with the dump's d / V / T / R: 0x930E4
 *            exact for ch 0 / 5 / 6, within 0.3 % for the 850 mA channels;
 *       (1d) the resistance model 0x83ABC from the dump's state: 0x933A8
 *            within ±3 mΩ;
 *   (2) the paths: the timeout (no edges → 0x40 → the pin level: high 0x21,
 *       low 0x20); in-window saturation (A ≥ P − 10 → 0x20, A ≤ 10 → 0x21);
 *       the 950 ‰ rule outside the window (96 % → 0x20, 94 % → 0x31); the
 *       5-pass mixed hold → 0x31 and the decided-pass countdown; the R-model
 *       cadence (one channel per pass, a round every 2 s, mode 0 and mode 1);
 *       the sampler rate (4 CMID00 per 10 240 µs, two windows);
 *   (3) the negative controls (built -DFBS_NEG_POLARITY / -DFBS_NEG_PINS,
 *       ctest WILL_FAIL): `test_fb_shadow --neg` runs (1a) only, which a
 *       swapped polarity or a swapped pin pair must fail.
 * It also prints the ISR's peripheral-access count (the model counts every
 * Timer F / D / port / FRT access of the longest ISR). */
#include "app/fb_shadow.h"
#include "app/fb_cal.h"
#include "tests/unit/fb_model.h"
#include "bsw/drv/drv_temp.h"
#include "bsw/drv/drv_cells.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_fail;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); s_fail++; } } while (0)

/* the stock driving dump stockram_driving_20261104.bin (values only) */
static const uint16_t D_SNAP_P[20] = { 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
                                       1000, 1000, 1000, 1000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
static const uint16_t D_SNAP_A[20] = { 0x43, 0x45, 0x1C9, 0x1C8, 0x1C1, 0x1C1, 0x1CA, 0x1CA, 0x1C6, 0x1C6,
                                       0x45, 0x45, 0x48, 0x47, 0, 0, 0, 0, 0, 0 };   /* 0x90294 */
static const uint8_t  D_CLS[10]  = { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x21, 0x21, 0x21 };
static const uint16_t D_DUTY[10] = { 0x44, 0x1C7, 0x1C0, 0x1C9, 0x1C5, 0x44, 0x47, 0, 0, 0 };   /* 0x9340C */
static const uint16_t D_EST[10]  = { 0x17D, 0x2409, 0x237F, 0x2437, 0x23F1, 0x17D, 0x1C7, 0, 0, 0 }; /* 0x930E4 */
static const uint16_t D_RES[10]  = { 0x1653, 0x17FA, 0x17FA, 0x17FA, 0x17FA, 0x1653, 0x1653, 0x2E01, 0x16B7, 0x16B7 };
static const uint16_t D_FAST[10] = { 0, 0x937, 0x937, 0x937, 0x937, 0, 0, 0, 0, 0 };         /* 0x933D0 */
static const uint16_t D_SLOW[10] = { 0, 0x189, 0x189, 0x189, 0x189, 0, 0, 0, 0, 0 };         /* 0x933E4 */
static const int16_t  D_I[10]    = { 500, 8500, 8500, 8500, 8500, 500, 500, 0, 0, 0 };       /* 0x94ED4 */
#define D_V      12908u          /* 0x94ED0[0] */
#define D_T      0x019B          /* 0x94F9C: 25.7 °C */
#define D_AMB    0x0C74          /* 0x94FD4 */

/* the drive used for the end-to-end checks: low time (counts at 4 MHz,
 * the dump's two windows averaged), period 4000 */
static const uint32_t D_LOW[10] = { 272u, 1826u, 1796u, 1832u, 1816u, 276u, 286u, 0u, 0u, 0u };

static uint8_t tf_of(uint8_t ch) { return FB_PINS[3u * (ch >> 1) + (ch & 1u)]; }

static void boot(void)
{
    fbm_reset();
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        fbm_config(tf_of(ch), (uint8_t)(((ch & 1u) == 0u) ? 1u : 2u));   /* F1: sub 0 rising */
        fbm_set_static(tf_of(ch), true);
    }
    fb_shadow_init();
    fb_shadow_start();
}

static fbs_in_t s_in;
static uint32_t s_t;            /* µs */

static void run_ms(uint32_t ms, uint32_t step_ms)
{
    const uint32_t end = s_t + ms * 1000u;
    s_in.n5 = (uint8_t)(step_ms / 5u);
    while (s_t + step_ms * 1000u <= end) {
        s_t += step_ms * 1000u;
        fbm_advance_to(s_t);
        fb_shadow_tick(&s_in);
    }
}

static void drive_dump(void)
{
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        if (D_LOW[ch] != 0u) {
            fbm_set_pwm(tf_of(ch), 4000u, D_LOW[ch], 37u * ch);    /* the chips' stagger */
        } else {
            fbm_set_static(tf_of(ch), true);
        }
    }
}

static void in_dump(void)
{
    memset(&s_in, 0, sizeof s_in);
    s_in.rail_mv = (uint16_t)D_V;
    s_in.t_fluid = D_T;
    s_in.t_st = 8u;                     /* a plausible acquisition (0x94FF0) */
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        s_in.i_req[ch] = D_I[ch];
    }
}

/* (1a) */
static int test_1a(void)
{
    const volatile fbs_t *s;
    int bad = 0;
    s_t = 0u;
    boot();
    in_dump();
    drive_dump();
    run_ms(400u, 5u);
    s = fb_shadow_state();
    printf("(1a) end to end, 400 ms at 5 ms: ch  cls duty dutyf  est   R   (dump cls duty)\n");
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        const int dd = (int)s->duty[ch] - (int)D_DUTY[ch];
        printf("     %u  %02X  %4u %4u %5u %5u  (%02X %4u)\n", ch, s->cls[ch], s->duty[ch],
               s->dutyf[ch], s->est[ch], s->res[ch], D_CLS[ch], D_DUTY[ch]);
        if (s->cls[ch] != D_CLS[ch] || dd > 1 || dd < -1) {
            bad++;
        }
    }
    CHECK(bad == 0, "(1a) %d channels differ from the dump", bad);
    return bad;
}

/* (1b) */
static void test_1b(void)
{
    volatile fbs_t *s = fb_shadow_host_state();
    s_t = 0u;
    boot();
    for (unsigned k = 0u; k < 20u; k++) {
        s->snap_p[k] = D_SNAP_P[k];
        s->snap_a[k] = D_SNAP_A[k];
    }
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        int dd;
        fb_shadow_host_classify(ch);
        dd = (int)s->duty[ch] - (int)D_DUTY[ch];
        CHECK(s->cls[ch] == D_CLS[ch], "(1b) ch %u class %02X, dump %02X", ch, s->cls[ch], D_CLS[ch]);
        CHECK(dd <= 1 && dd >= -1, "(1b) ch %u duty %u, dump %u", ch, s->duty[ch], D_DUTY[ch]);
        CHECK(s->dutyf[ch] == s->duty[ch], "(1b) ch %u: the filter's first pass copies", ch);
    }
    printf("(1b) the dump's snapshot: classes and duties (±1) reproduced\n");
}

/* (1c) */
static void test_1c(void)
{
    volatile fbs_t *s = fb_shadow_host_state();
    s_t = 0u;
    boot();
    s->tdrv = 0x0215;
    s->tfl = D_T;
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        s->res[ch] = D_RES[ch];
        fb_shadow_host_est(ch, (uint16_t)D_V, D_I[ch], D_DUTY[ch]);
        printf("(1c) ch %u estimate %5u, dump %5u\n", ch, s->est[ch], D_EST[ch]);
        if (ch == 0u || ch == 5u || ch == 6u || ch >= 7u) {
            CHECK(s->est[ch] == D_EST[ch], "(1c) ch %u estimate %u, dump %u", ch, s->est[ch], D_EST[ch]);
        } else {
            const int d = (int)s->est[ch] - (int)D_EST[ch];
            CHECK(d * 1000 <= 3 * (int)D_EST[ch] && -d * 1000 <= 3 * (int)D_EST[ch],
                  "(1c) ch %u estimate %u, dump %u (> 0.3 %%)", ch, s->est[ch], D_EST[ch]);
        }
    }
}

/* (1d) */
static void test_1d(void)
{
    volatile fbs_t *s = fb_shadow_host_state();
    s_t = 0u;
    boot();
    s->amb = D_AMB;
    s->ambc = (uint16_t)~(uint16_t)D_AMB;
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        s->rfast[ch] = D_FAST[ch];
        s->rslow[ch] = D_SLOW[ch];
        s->rfirst[ch] = 1u;
        s->rfirst2[ch] = 1u;
    }
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        int d;
        fb_shadow_host_rupd(ch, D_I[ch], 0u, D_T);
        d = (int)s->res[ch] - (int)D_RES[ch];
        printf("(1d) ch %u R %5u mOhm, dump %5u (rise %u)\n", ch, s->res[ch], D_RES[ch], s->rsum[ch]);
        CHECK(d <= 3 && d >= -3, "(1d) ch %u R %u, dump %u", ch, s->res[ch], D_RES[ch]);
    }
    CHECK(s->pair_bad_n == 0u, "(1d) the 0x6BDFE twin disagreed %u times", (unsigned)s->pair_bad_n);
}

/* (2) */
static void test_2_paths(void)
{
    const volatile fbs_t *s;
    volatile fbs_t *w = fb_shadow_host_state();
    s_t = 0u;
    boot();
    in_dump();
    drive_dump();
    run_ms(200u, 5u);
    /* ch 0 (A, PF4, sub 0) static high, ch 1 (D, PF5, sub 1) static low,
     * ch 2 (B) in-window saturated, ch 3 (E) in-window floor, ch 4 (C)
     * 2.5 ms at 96 %, ch 5 (WK) 2.5 ms at 94 % */
    fbm_set_static(tf_of(0u), true);
    fbm_set_static(tf_of(1u), false);
    fbm_set_pwm(tf_of(2u), 4000u, 3990u, 0u);
    fbm_set_pwm(tf_of(3u), 4000u, 30u, 0u);
    fbm_set_pwm(tf_of(4u), 10000u, 9600u, 0u);
    fbm_set_pwm(tf_of(5u), 10000u, 9400u, 0u);
    run_ms(300u, 5u);
    s = fb_shadow_state();
    CHECK(s->st[0].fl == 0x40u && s->cls[0] == 0x21u, "(2) no edges, pin high: fl %02X class %02X",
          s->st[0].fl, s->cls[0]);
    CHECK(s->st[1].fl == 0x40u && s->cls[1] == 0x20u && s->duty[1] == 1000u,
          "(2) no edges, pin low: fl %02X class %02X duty %u", s->st[1].fl, s->cls[1], s->duty[1]);
    CHECK(s->cls[2] == 0x20u && s->duty[2] == 1000u, "(2) A >= P - m: class %02X duty %u", s->cls[2], s->duty[2]);
    CHECK(s->cls[3] == 0x21u && s->duty[3] == 0u, "(2) A <= m: class %02X duty %u", s->cls[3], s->duty[3]);
    CHECK(s->cls[4] == 0x20u, "(2) 2.5 ms at 96 %%: class %02X (the 950 rule)", s->cls[4]);
    CHECK(s->cls[5] == 0x31u, "(2) 2.5 ms at 94 %%: class %02X", s->cls[5]);
    printf("(2) timeout high/low %02X/%02X, saturated %02X, floor %02X, 96 %% %02X, 94 %% %02X\n",
           s->cls[0], s->cls[1], s->cls[2], s->cls[3], s->cls[4], s->cls[5]);

    /* the mixed hold: window 0 = 0x20 (no edges, active), window 1 = 0x21 */
    w->cls[6] = 0x10u;
    w->hold[6] = 0u;
    w->duty[6] = 77u;
    for (unsigned pass = 1u; pass <= 6u; pass++) {
        w->snap_p[12] = 0xFFFFu;
        w->snap_a[12] = 0xFFFFu;
        w->snap_p[13] = 0xFFFFu;
        w->snap_a[13] = 0u;
        fb_shadow_host_classify(6u);
        if (pass <= 4u) {
            CHECK(w->cls[6] == 0x10u && w->hold[6] == pass && w->duty[6] == 77u,
                  "(2) mixed pass %u: class %02X hold %u duty %u", pass, w->cls[6], w->hold[6], w->duty[6]);
        } else {
            CHECK(w->cls[6] == 0x31u && w->hold[6] == 4u, "(2) mixed pass %u: class %02X hold %u",
                  pass, w->cls[6], w->hold[6]);
        }
    }
    w->snap_a[12] = 0u;                         /* both 0x21: decided */
    fb_shadow_host_classify(6u);
    CHECK(w->cls[6] == 0x21u && w->hold[6] == 3u, "(2) decided after the hold: class %02X hold %u",
          w->cls[6], w->hold[6]);
    printf("(2) the mixed hold: 4 passes kept, 0x31 on the 5th, a decided pass counts down\n");
}

static void test_2_cadence(uint32_t step_ms)
{
    const volatile fbs_t *s;
    uint32_t prev = 0u, rounds = 0u, first_round_tick = 0u, ticks = 0u;
    int bad = 0;
    s_t = 0u;
    boot();
    in_dump();
    drive_dump();
    s = fb_shadow_state();
    for (uint32_t k = 1u; k <= (8000u / step_ms); k++) {
        run_ms(step_ms, step_ms);
        ticks++;
        if (k == 1u) {
            CHECK(s->r_upd_n == 0u, "(2) cadence: first pass counts only");
        } else if (k == 2u) {
            CHECK(s->r_upd_n == 10u, "(2) cadence: second pass updates all ten (%u)", (unsigned)s->r_upd_n);
        } else if (s->r_upd_n != prev) {
            if (s->r_upd_n != prev + 1u) {
                bad++;
            }
            if (s->c9410d == 1u) {
                rounds++;
                if (first_round_tick == 0u) {
                    first_round_tick = k;
                }
            }
        }
        prev = s->r_upd_n;
    }
    CHECK(bad == 0, "(2) cadence: more than one channel in a pass");
    CHECK(first_round_tick == 2u + 2000u / step_ms, "(2) cadence: first round at pass %u (step %u ms)",
          first_round_tick, step_ms);
    CHECK(rounds == 3u, "(2) cadence: %u rounds in 8 s (step %u ms)", rounds, step_ms);
    CHECK(s->r_upd_n == 10u + 30u, "(2) cadence: %u updates", (unsigned)s->r_upd_n);
    printf("(2) R model at %u ms passes: all ten on pass 2, then one channel per pass, rounds at pass %u + 2 s\n",
           step_ms, first_round_tick);
    (void)ticks;
}

static void test_2_rate(void)
{
    const volatile fbs_t *s;
    s_t = 0u;
    boot();
    in_dump();
    drive_dump();
    run_ms(1024u, 5u);
    s = fb_shadow_state();
    printf("(2) sampler over 1024 ms: ISR %u (model %u), windows %u / %u, state-1 %u, loops %u, IPR10 %04X; "
           "peripheral accesses of the longest ISR %u\n",
           (unsigned)s->isr_n, fbm_isr_n(), (unsigned)s->win_n[0], (unsigned)s->win_n[1],
           (unsigned)s->st1_n, (unsigned)s->isr_loops, (unsigned)fbm_ipr10(), fbm_isr_acc_max());
    CHECK(s->isr_n >= 398u && s->isr_n <= 401u, "(2) %u ISRs in 1024 ms (4 per 10.24 ms)", (unsigned)s->isr_n);
    CHECK(s->win_n[0] >= 99u && s->win_n[0] <= 101u && s->win_n[1] >= 99u && s->win_n[1] <= 101u,
          "(2) windows %u / %u", (unsigned)s->win_n[0], (unsigned)s->win_n[1]);
    CHECK(s->isr_loops == 0u, "(2) catch-up loops %u", (unsigned)s->isr_loops);
    CHECK(s->iv[0] == 2560u && s->iv[1] == 5120u && s->iv[2] == 2060u,
          "(2) intervals %u %u %u (dump 0A00 1400 080C)", s->iv[0], s->iv[1], s->iv[2]);
    CHECK(fbm_ipr10() == 0x6000u, "(2) IPR10 bits 15-12 = 6");
}

/* (4) THE COLD BOOT (2026-11-18, the r04b bench defect): zeroed shadow RAM,
 * the fluid temperature as OUR drv_temp produces it at boot (0x94F9C = 0 at
 * init, then the 0x0780 default with status 0 / 7 / 3 until our Timer G
 * starts at 500 ms; then a plausible acquisition, status 8, 35.9 °C as the
 * r04b read), the rail at 'U' (+620 ms) with the P set (A D E C 850 mA,
 * B WK Sys 50 mA) at the r04b duties. After 60 s: the ambient state within
 * 1 °C of the fluid, R on the 850 mA channels 6.0-6.4 Ω, the estimate in the
 * stock dump's band for 850 mA (its own 0x930E4: 9087-9271). The negative
 * build -DFBS_NEG_COLDSEED (the r04b schedule, no wait) must FAIL here and
 * show the bench's ≈ 94 °C at 75.6 s. */
static void cnt_5ms(void)            /* the 0x7C7E4 countdown service (sup_ladder's) */
{
    for (uint32_t a = 0xFFF94734u; a < 0xFFF94786u; a += 2u) {
        volatile uint16_t *w = (volatile uint16_t *)(uintptr_t)a;
        if (*w != 0u) {
            *w = (uint16_t)(*w - 1u);
        }
    }
    for (uint32_t a = 0xFFF94788u; a < 0xFFF94835u; a++) {
        volatile uint8_t *b = (volatile uint8_t *)(uintptr_t)a;
        if (*b != 0u) {
            *b = (uint8_t)(*b - 1u);
        }
    }
}

static int test_cold(void)
{
    static const uint32_t LOW_U[7] = { 1776u, 1804u, 276u, 1836u, 1812u, 272u, 276u };   /* r04b ‰ × 4 */
    static const int16_t I_P[7] = { 8500, 8500, 500, 8500, 8500, 500, 500 };   /* the gear-1 P set */
    const volatile fbs_t *s;
    int bad = 0;
    bool up = false;
    uint32_t ms = 0u;
    memset((void *)(uintptr_t)0xFFF80000u, 0, 0x16000u);
    s_t = 0u;
    boot();
    drv_temp_init();
    memset(&s_in, 0, sizeof s_in);
    s_in.rail_mv = 12940u;
    s = fb_shadow_state();
    while (ms < 75600u) {
        const uint32_t step = (ms < 670u) ? 5u : 10u;      /* mode 0, then mode 1 */
        ms += step;
        for (uint32_t k = 0u; k < step / 5u; k++) {
            cnt_5ms();
        }
        if (ms >= 500u) {                                  /* Timer G started: AN8 / AN1 plausible */
            CELL_AN_MV(8) = 2656u;
            CELL_AN_MV(1) = 170u;
        }
        drv_temp_tick();                                   /* 0x887D8 (entry 7, before 0x7CBA8) */
        if (!up && ms >= 620u) {                           /* 'U': the P set */
            up = true;
            for (uint8_t ch = 0u; ch < 7u; ch++) {
                s_in.i_req[ch] = I_P[ch];
                fbm_set_pwm(tf_of(ch), 4000u, LOW_U[ch], 37u * ch);
            }
        }
        s_in.n5 = (uint8_t)(step / 5u);
        s_in.t_fluid = CELL_TMP;
        s_in.t_st = CELL_TMP_ST;
        s_t = ms * 1000u;
        fbm_advance_to(s_t);
        fb_shadow_tick(&s_in);
        if (ms == 60670u) {
            const int amb_err = (int)s->amb - (int)s->tfl * 8;
            printf("(4) cold boot, 60 s: fluid %d (%.1f C, status %u), amb %d (%.1f C), seed waited %u passes\n",
                   s->tfl, s->tfl / 16.0, CELL_TMP_ST, s->amb, s->amb / 128.0, s->r_wait);
            if (amb_err > 128 || amb_err < -128) {
                bad++;
            }
            for (uint8_t ch = 0u; ch < 7u; ch++) {
                printf("    ch %u: duty %4u R %5u mOhm est %5u (I %u)\n", ch, s->duty[ch], s->res[ch],
                       s->est[ch], (unsigned)I_P[ch]);
                if (I_P[ch] == 8500 && (s->res[ch] < 6000u || s->res[ch] > 6400u ||
                                        s->est[ch] < 8500u || s->est[ch] > 9400u)) {
                    bad++;
                }
            }
        }
    }
    printf("(4) cold boot, 75.6 s: amb %d (%.1f C) — r04b read 12001 (93.8 C)\n", s->amb, s->amb / 128.0);
    CHECK(bad == 0, "(4) cold boot: %d checks out of band", bad);
    return bad;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--cold") == 0) {
        const int bad = test_cold();
        printf("%s\n", bad == 0 ? "PASS" : "FAIL");
        return bad == 0 ? 0 : 1;
    }
    if (argc > 1 && strcmp(argv[1], "--neg") == 0) {
        const int bad = test_1a();
        printf("%s (%d channels differ)\n", bad == 0 ? "PASS" : "FAIL", bad);
        return bad == 0 ? 0 : 1;
    }
    test_1a();
    test_1b();
    test_1c();
    test_1d();
    test_2_paths();
    test_2_cadence(5u);
    test_2_cadence(10u);
    test_2_rate();
    test_cold();
    printf("%s (%d failures)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
    return s_fail == 0 ? 0 : 1;
}
