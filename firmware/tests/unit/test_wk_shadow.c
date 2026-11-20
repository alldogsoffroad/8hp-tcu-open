/* test_wk_shadow.c - the READ-ONLY torque-converter lock-up (WK) shadow (app/wk_shadow.c), judged on
 * the host (2026-11-20, ~/tcu_sweep/wk_shadow_port.md):
 *
 *   test_wk_shadow           the gate 0xDCE40 (1:1 truth table), the WK current law (binary with the NC
 *                            roles, the curve 0xE13C + the NC map 0xE214), every dry-bench input = never
 *                            permitted (state 1, WK 500), a vehicle-shaped scenario (forward power flow, OS
 *                            mode 2, 1800 / 1750 rpm in 6th, request level 1 or 3) -> prefill -> fill ->
 *                            engaged ramp -> closed, or the 40 rpm controlled slip with the integral
 *                            controller converging on a plant; a shift / tip-in / abort releases it; the
 *                            engagement counter edge (0xBF8E8's call) only COUNTED; B5E1 / B5E2
 *   test_wk_shadow --cal     app/wk_cal.c re-derived from the stock image (skip 77 without it) + the facts
 *                            the shadow assumes about data it does not read (the 0xD9F10 map is all zero)
 * The mutated-map negative (the WK_NEG_MAP build) must FAIL this test. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/wk_shadow.h"
#include "app/wk_cal.h"
#include "app/cal_inplace.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint16_t s_ms;

static wk_in_t base(void)               /* the dry bench: OS mode 1, nothing turning, WK live 500 */
{
    wk_in_t in;
    memset(&in, 0, sizeof in);
    in.os_mode = 1u; in.c3d = 1u; in.e612_b = 1u; in.range_ok = 1u;
    in.temp_byte = 0x4F; in.temp_c = 39; in.line = 7000u; in.live_wk = 500u;
    return in;
}

static wk_in_t vehicle(void)            /* a healthy vehicle in 6th: forward power flow, OS mode 2, 1800 / 1750 rpm */
{
    wk_in_t in = base();
    in.os_mode = 2u; in.c3d = 0u; in.c2f = 1u; in.c30 = 1u; in.e612_b = 1u;
    in.temp_byte = 120u; in.temp_c = 80;
    in.eng = 1800u; in.turb = 1750u; in.tq = 100; in.line = 7000u;
    return in;
}

static void step(wk_in_t *in, unsigned n)
{
    while (n--) {
        s_ms = (uint16_t)(s_ms + 10u);
        in->now_ms = s_ms;
        wk_shadow_pass(in);
    }
}

#define ST (wk_shadow_state())

static void test_gate(void)
{
    wk_in_t in = vehicle();
    in.e612_a = 0u; in.e612_b = 1u;
    CHECK(wk_gate(&in), "forward flow, healthy: open");
    in.c2f = 0u;
    CHECK(!wk_gate(&in), "completed != committed flow (and no E612 flag): shut");
    in.e612_a = 1u;
    CHECK(wk_gate(&in), "the E612 flag opens it");
    in.e612_a = 0u; in.c2f = 1u; in.c30 = 2u;
    CHECK(!wk_gate(&in), "reverse flow: shut");
    in.c30 = 1u; in.c3d = 1u;
    CHECK(!wk_gate(&in), "OS mode 1 (0x89C3D): shut");
    in.c3d = 0u; in.cf6 = 1u;
    CHECK(!wk_gate(&in), "reaction 49 / 42 (0x89CF6): shut");
    in.cf6 = 0u; in.e612_b = 0u;
    CHECK(!wk_gate(&in), "the second E612 flag: shut");
    in.e612_b = 1u; in.e61c_busy = 1u;
    CHECK(!wk_gate(&in), "E61C busy: shut");
}

static void near_u16(uint16_t v, uint16_t want, uint16_t tol, const char *what)
{
    CHECK(v + tol >= want && v <= want + tol, "%s: %u, want %u +-%u", what, v, want, tol);
}

static void test_law(void)
{
    wk_shadow_init();
    /* the binary law, normally closed roles (0x10809C): p = 0 -> i_min; p >= line or >= 12750 -> i_max */
    near_u16(wk_law(0u, 1u, 0x4F, 7000u, 0), 500u, 0u, "binary p=0");
    near_u16(wk_law(7000u, 1u, 0x4F, 7000u, 0), 8500u, 0u, "binary p=line");
    near_u16(wk_law(20000u, 1u, 0x4F, 30000u, 0), 8500u, 0u, "binary p>=12750");
    near_u16(wk_law(3000u, 1u, 0x4F, 7000u, 0), 500u, 0u, "binary 0 < p < line");
    CHECK(ST->w.fault_n == 1u, "the 0x1C2 report counted once (%u)", (unsigned)ST->w.fault_n);
    /* the curve 0xE13C (decode 1.4): 500 -> 3088, 5000 -> 5143, 11785 -> 8500; the NC map moves a few counts */
    near_u16(wk_law(500u, 0u, 0x4F, 0u, 0), 3088u, 60u, "curve 500 mbar");
    near_u16(wk_law(5000u, 0u, 0x4F, 0u, 0), 5143u, 60u, "curve 5000 mbar");
    near_u16(wk_law(11785u, 0u, 0x4F, 0u, 0), 8500u, 0u, "curve end");
    near_u16(wk_law(12500u, 0u, 0x4F, 0u, 0), 8500u, 0u, "curve above the end");
    near_u16(wk_law(0u, 0u, 0x4F, 0u, 0), 500u, 0u, "curve at 0");
    /* the converter offset (0xBA310) shifts the curve before the map */
    CHECK(wk_law(5000u, 0u, 0x4F, 0u, 100) > wk_law(5000u, 0u, 0x4F, 0u, 0) + 80u, "offset +100 raises the current");
}

static void test_dry(void)
{
    wk_in_t in = base();
    unsigned i;
    wk_shadow_init();
    step(&in, 100u);                                    /* OS mode 1: the reset list */
    CHECK(ST->state == 1u && ST->b4ac == 1u && ST->b2aa == 0u, "reset list: state %u b4ac %u", ST->state, ST->b4ac);
    CHECK(ST->w.reset_n == 100u && ST->w.perm_n == 0u, "reset passes %u perm %u", (unsigned)ST->w.reset_n, (unsigned)ST->w.perm_n);
    /* OS mode 2 (the bench with the brake + engine sims), nothing turning: the entry never opens */
    in.os_mode = 2u; in.c3d = 0u;
    in.eng = 700u;
    step(&in, 200u);
    CHECK(ST->state == 1u && ST->w.perm_n == 0u, "mode 2 dry: state %u", ST->state);
    /* a turning turbine but the request level producer unlocated (0): never permitted */
    in.turb = 2000u; in.eng = 2050u; in.c30 = 1u; in.c2f = 1u;
    step(&in, 200u);
    CHECK(ST->state == 1u && ST->w.perm_n == 0u, "request 0: state %u", ST->state);
    CHECK(ST->wk_i == 500u && ST->w.diff_n == 0u, "WK stays 500 (%u), diff %u", ST->wk_i, (unsigned)ST->w.diff_n);
    CHECK(ST->w.err_n == 0u && ST->w.eng_cnt_n == 0u, "no error, no engagement");
    /* every input combination on the dry-bench side of the threshold: the gate-or-entry never lets WK off 500 */
    for (i = 0u; i < 256u; i++) {
        wk_in_t x = base();
        x.os_mode = (uint8_t)(1u + (i & 1u)); x.c3d = (uint8_t)((i >> 1) & 1u); x.c30 = (uint8_t)((i >> 2) & 1u);
        x.c2f = x.c30; x.eng = (uint16_t)(((i >> 3) & 1u) ? 700u : 0u); x.turb = (uint16_t)(((i >> 4) & 1u) ? 700u : 0u);
        x.shift_busy = (uint8_t)((i >> 5) & 1u); x.cf6 = (uint8_t)((i >> 6) & 1u); x.e612_a = (uint8_t)((i >> 7) & 1u);
        wk_shadow_init();
        step(&x, 30u);
        CHECK(ST->state == 1u && ST->wk_i == 500u, "dry combination %u left state 1 / 500 (%u / %u)", i, ST->state, ST->wk_i);
    }
}

/* the vehicle scenario: closed-loop through a plant (a converter whose slip falls with the apply pressure) */
static uint16_t plant_slip(float p_bar, float lock_bar)
{
    const float s = 800.0f - 120.0f * p_bar;
    (void)lock_bar;
    return (uint16_t)((s < 0.0f) ? 0.0f : s);
}

static void test_vehicle_closed(void)
{
    wk_in_t in = vehicle();
    unsigned i;
    uint16_t seen_pre = 0u;
    wk_shadow_init();
    wk_shadow_host_req(1u);
    in.req_idx = 1u;
    in.live_wk = 500u;
    /* 2026-11-20 (gaps3): the stock's open-loop ramps: prefill 1 pass (D10+0x1F0 all 0, p = 0), the fill 13 .. 36 passes at p = D098
     * (the maps D10+0x1E8 / 0x1EC / 0x200), the engage 80 .. 150 passes (D10+0x1F8), then closed */
    {
        unsigned n[8] = { 0u }, k;
        float pe_prev = 0.0f;
        int mono = 1;
        for (k = 0u; k < 500u && ST->state != 6u; k++) {
            step(&in, 1u);
            if (ST->state < 8u) {
                n[ST->state]++;
            }
            if (ST->state == 2u) {
                CHECK(ST->b4ac == 0u && ST->p_bar == 0.0f, "prefill: curve law, p %.3f", (double)ST->p_bar);
            }
            if (ST->state == 3u) {
                CHECK(fabsf(ST->p_bar - WK_SI_D098_BAR) < 0.01f, "fill: p = D098 (%.3f)", (double)ST->p_bar);
            }
            if (ST->state == 4u) {
                if (ST->p_bar + 0.0001f < pe_prev) {
                    mono = 0;
                }
                pe_prev = ST->p_bar;
            }
        }
        CHECK(n[2] == 1u, "prefill lasted %u pass(es) (1: the prefill time map is all 0)", n[2]);
        CHECK(n[3] >= 13u && n[3] <= 36u, "fill lasted %u passes (13 .. 36)", n[3]);
        CHECK(n[3] == (unsigned)ST->fill_len, "fill passes %u = the fill length %u", n[3], (unsigned)ST->fill_len);
        CHECK(n[4] >= 80u && n[4] <= 151u, "engage lasted %u passes (80 .. 150 + the entry pass)", n[4]);
        CHECK(n[4] == 81u, "engage at 80 C / slip 50 lasted %u passes (80 + 1: D10+0x1F8 at x 120 / y 50)", n[4]);
        CHECK(mono != 0, "the engage ramp is monotone (the linear form)");
        CHECK(ST->state == 6u, "closed (state %u)", ST->state);
        seen_pre = (uint16_t)n[2];
        CHECK(ST->f2c == 0u, "B5E2 off once closed");
    }
    CHECK(ST->state == 6u && ST->closed == 1u, "closed (state %u closed %u)", ST->state, ST->closed);
    CHECK(ST->wk_i == 8500u, "locked: WK at 8500 (%u)", ST->wk_i);
    CHECK(ST->gate == 1u && ST->mode_out == 0u && ST->req_mbar >= 11000u, "request %u mode %u", ST->req_mbar, ST->mode_out);
    CHECK(ST->w.eng_cnt_n == 1u, "one 0 -> 1 closure counted (the stock's 0xBF8E8 call: counted, never made): %u", (unsigned)ST->w.eng_cnt_n);
    CHECK(ST->w.diff_n > 0u, "the would-be WK differs from the live 500");
    CHECK(seen_pre != 0u, "seen");
    /* a tip-in / shift: the range test sends the closed state to 7, back to 6 when it ends */
    in.shift_busy = 1u;
    step(&in, 2u);
    CHECK(ST->state == 7u, "shift in progress: state %u", ST->state);
    in.shift_busy = 0u;
    step(&in, 3u);
    CHECK(ST->state == 6u, "shift over: state %u", ST->state);
    /* the request drops: release through 0xB to open, WK back to 500 */
    in.req_idx = 0u;
    wk_shadow_host_req(0u);
    step(&in, 2u);
    CHECK(ST->state == 0xBu, "request 0: release (state %u)", ST->state);
    for (i = 0u; i < 60u && ST->state != 1u; i++) {
        step(&in, 1u);
    }
    CHECK(ST->state == 1u && ST->b4ac == 1u && ST->wk_i == 500u, "released to open (state %u wk %u)", ST->state, ST->wk_i);
    /* an abort (E353) from the closed state */
    in.req_idx = 1u;
    step(&in, 300u);
    CHECK(ST->state == 6u, "re-engaged (state %u)", ST->state);
    in.e353 = 1u;
    step(&in, 1u);
    CHECK(ST->state == 1u && ST->wk_i == 500u && ST->w.abort_n >= 1u, "E353 aborts to open (state %u)", ST->state);
    in.e353 = 0u;
    /* a hot converter / fluid blocks the entry */
    in.temp_c = 140; in.temp_byte = 180u;
    step(&in, 5u);
    CHECK(ST->state <= 3u, "overheat: no closure within 5 passes (%u; the prefill is 1 pass now)", ST->state);
    CHECK(ST->w.err_n == 0u, "no table error");
}

static void test_slip_control(void)
{
    wk_in_t in = vehicle();
    unsigned i, closed_seen = 0u;
    float pmax = 0.0f;
    wk_shadow_init();
    in.req_idx = 3u;
    in.tq = 300;
    /* the plant: slip = 800 - 120 p (rpm, p in bar), the engine at 1800 */
    for (i = 0u; i < 1200u; i++) {
        in.turb = (uint16_t)(in.eng - plant_slip(ST->p_bar, 0.0f));
        step(&in, 1u);
        if (ST->state == 6u || ST->state == 5u) {
            closed_seen = 1u;
        }
        if (ST->state == 9u) {
            if (ST->p_bar > pmax) {
                pmax = ST->p_bar;
            }
        }
    }
    CHECK(closed_seen != 0u, "closed on the way");
    CHECK(ST->state == 9u, "controlled slip (state %u)", ST->state);
    CHECK(fabsf(ST->p_bar - 6.33f) < 0.4f, "the integral controller converged on the plant: %.2f bar (6.33)", (double)ST->p_bar);
    CHECK(abs((int)ST->slip - 40) <= 6, "slip %d rpm, target 40", ST->slip);
    CHECK(ST->ff > 5.2f && ST->ff < 5.4f, "the feed-forward at 300 Nm is 5.3 bar: %.3f", (double)ST->ff);
    CHECK(ST->slip_run == 1u, "run phase");
    CHECK(ST->p_bar <= ST->ceil + 0.001f, "below the ceiling %.2f", (double)ST->ceil);
    /* request 3 -> 1: ramp out to closed (10 -> 6) */
    in.req_idx = 1u;
    for (i = 0u; i < 40u; i++) {
        in.turb = (uint16_t)(in.eng - plant_slip(ST->p_bar, 0.0f));
        step(&in, 1u);
    }
    CHECK(ST->state == 6u && ST->closed == 1u, "request 1: back to closed (state %u)", ST->state);
    /* the engine slip must be driven toward the target from a free-slipping start too */
    wk_shadow_init();
    in.req_idx = 3u;
    in.tq = 100;
    for (i = 0u; i < 1500u; i++) {
        in.turb = (uint16_t)(in.eng - plant_slip(ST->p_bar, 0.0f));
        step(&in, 1u);
    }
    CHECK(ST->state == 9u && abs((int)ST->slip - 40) <= 6, "second run (tq 100): state %u slip %d", ST->state, ST->slip);
}

static void test_gate_shut_machine_runs(void)
{
    wk_in_t in = vehicle();
    wk_shadow_init();
    in.req_idx = 1u;
    in.cf6 = 1u;                                         /* reaction 49 / 42: the producer's gate is shut */
    step(&in, 120u);
    CHECK(ST->state == 6u, "the machine runs (state %u)", ST->state);
    CHECK(ST->gate == 0u && ST->req_mbar == 0u && ST->mode_out == 1u && ST->wk_i == 500u,
          "gate shut: request %u mode %u wk %u", ST->req_mbar, ST->mode_out, ST->wk_i);
    in.cf6 = 0u;
    step(&in, 1u);
    CHECK(ST->wk_i == 8500u, "gate open: 8500 (%u)", ST->wk_i);
}

static void test_b5e(void)
{
    wk_in_t in = vehicle();
    wk_shadow_init();
    in.req_idx = 1u;
    in.tq = 200;
    {
        unsigned k;
        for (k = 0u; k < 40u && ST->state != 3u; k++) {
            step(&in, 1u);
        }
        step(&in, 3u);                                   /* into the fill ramp: B5E2 = 1 */
    }
    CHECK(ST->f2b == 1u && wk_b5e2() == 1u, "fill ramp: B5E2 (%u)", wk_b5e2());
    in.tq = 150;                                         /* the torque falls while engaging: the raise is the drop */
    step(&in, 1u);
    CHECK(wk_b5e1() > 0u, "B5E1 raised by the drop (%u)", wk_b5e1());
    step(&in, 300u);
    CHECK(ST->state == 6u && wk_b5e2() == 0u, "closed: B5E2 off (%u)", wk_b5e2());
}

static void test_cal(void)
{
    uint32_t i;
    for (i = 0u; i < 25u; i++) {
        const uint16_t want = (i == 0u) ? (uint16_t)cal_u32(0xE13Cu) : cal_u16(0xE13Cu + 2u * i + 2u);
        if (WK_E13C[i] != want) { fails++; printf("FAIL E13C %u\n", i); }
    }
    for (i = 0u; i < 79u; i++) {
        const int16_t want = (i < 2u) ? (int16_t)cal_u32(0xE214u + 4u * i) : cal_s16(0xE214u + 8u + 2u * (i - 2u));
        if (WK_E214[i] != want) { fails++; printf("FAIL E214 %u\n", i); }
    }
    /* the facts the shadow assumes about data it does not read: the 0xD9F10 correction map of a type-0 (NC)
     * element (D10 cal pointer 0x54C54+0x24 -> CAL_WK_ZMAP) has z all 0, and its header is {4, 2} */
    CHECK(cal_u32(CAL_WK_ZMAP) == 4u && cal_u32(CAL_WK_ZMAP + 4u) == 2u, "zmap header");
    for (i = 0u; i < 8u; i++) {
        if (cal_u32(CAL_WK_ZMAP + 8u + 4u * (4u + 2u + i)) != 0u) { fails++; printf("FAIL zmap z %u\n", i); }
    }
    /* 2026-11-20 (gaps3): the open-loop ramps' calibration: the D10 pointer entries and the decode's numbers */
    {
        static const struct { uint32_t off, addr; } d10[] = {
            { 0x168u, CAL_WK_ENG_HOLD }, { 0x16Cu, CAL_WK_FILL_P }, { 0x1E8u, CAL_WK_FILL_LEN }, { 0x1ECu, CAL_WK_FILL_TMP },
            { 0x1F0u, CAL_WK_PRE_MAP }, { 0x1F4u, CAL_WK_PRE_CRV }, { 0x1F8u, CAL_WK_ENG_T0 }, { 0x1FCu, CAL_WK_ENG_T1 },
            { 0x200u, CAL_WK_FILL_M2 }, { 0x224u, CAL_WK_REL_CRV }, { 0x234u, CAL_WK_ENG_FORM }, { 0x248u, CAL_WK_ST7 },
        };
        for (i = 0u; i < sizeof d10 / sizeof d10[0]; i++) {
            if (cal_u32(0x54680u + d10[i].off) != d10[i].addr) { fails++; printf("FAIL D10+0x%X = %X (%X)\n", d10[i].off, cal_u32(0x54680u + d10[i].off), d10[i].addr); }
        }
        CHECK(cal_u32(CAL_WK_PRE_MAP) == 2u && cal_u32(CAL_WK_PRE_MAP + 4u) == 2u, "prefill map header");
        for (i = 0u; i < 4u; i++) {
            CHECK(cal_s16(CAL_WK_PRE_MAP + 8u + 2u * (2u + 2u + i)) == 0, "prefill time z[%u] is 0", i);
            CHECK(i >= 2u || cal_f32(CAL_WK_PRE_CRV + 4u + 8u + 4u * i) == 0.0f, "prefill pressure curve y[%u] is 0", i);
        }
        CHECK(cal_u32(CAL_WK_ENG_FORM) == 0u || cal_u8(CAL_WK_ENG_FORM) == 0u, "D10+0x234 = 0 (the linear engage form)");
        CHECK(cal_f32(CAL_WK_ST7) == 0.25f && cal_u8(CAL_WK_CFG278) == 0u, "state 7's 0.25 and D10+0x278 = 0");
        for (i = 0u; i < 4u; i++) {
            CHECK(cal_f32(CAL_WK_REL_CRV + 4u + 16u + 4u * i) == 1.0f, "release curve y[%u] = 1.0", i);
        }
        /* the engage time map (temperature byte x slip): 150 / 120 / 80 at x 120 over the slip rows, 100 everywhere in +0x1FC */
        {
            static const int ey[6] = { 150, 150, 120, 80, 100, 100 };
            for (i = 0u; i < 6u; i++) {
                CHECK(cal_s16(CAL_WK_ENG_T0 + 8u + 2u * (6u + 6u + 6u * i + 4u)) == ey[i], "engage time z[%u][4] = %d", i,
                      cal_s16(CAL_WK_ENG_T0 + 8u + 2u * (6u + 6u + 6u * i + 4u)));
                CHECK(cal_s16(CAL_WK_ENG_T1 + 8u + 2u * (6u + 6u + 6u * i + 4u)) == 100, "engage time (+0x1FC) z[%u][4]", i);
            }
        }
        /* the fill length: 17 .. 10 over slip x engine, the temperature part 15 10 5 4 2 0, the third map 4 2 0 */
        CHECK(cal_s16(CAL_WK_FILL_LEN + 8u + 2u * 12u) == 17 && cal_s16(CAL_WK_FILL_LEN + 8u + 2u * (12u + 35u)) == 10, "fill length map corners 17 / 10");
        CHECK(cal_s16(CAL_WK_FILL_TMP + 4u + 12u) == 15 && cal_s16(CAL_WK_FILL_TMP + 4u + 12u + 10u) == 0 && cal_s16(CAL_WK_FILL_TMP + 4u + 12u + 6u) == 4 &&
                  cal_s16(CAL_WK_FILL_TMP + 4u) == 40 && cal_s16(CAL_WK_FILL_TMP + 4u + 10u) == 160, "fill temperature curve 15 .. 0 over 40 .. 160");
        CHECK(cal_s16(CAL_WK_FILL_M2 + 8u + 2u * 12u) == 4 && cal_s16(CAL_WK_FILL_M2 + 8u + 2u * 13u) == 2 && cal_s16(CAL_WK_FILL_M2 + 8u + 2u * 14u) == 0,
              "fill third map 4 2 0");
        CHECK(cal_s16(CAL_WK_ENG_HOLD + 8u + 2u * 12u) == 90 && cal_s16(CAL_WK_ENG_HOLD + 8u + 2u * (12u + 35u)) == 100, "hold map corners 90 / 100");
    }
    /* the request-level table [0, 1, 2, 3] and the thresholds the decode quotes */
    CHECK(cal_u32(CAL_WK_REQTAB) == 4u && cal_u8(CAL_WK_REQTAB + 4u) == 0u && cal_u8(CAL_WK_REQTAB + 7u) == 3u, "request table");
    CHECK(cal_u16(CAL_WK_ENG) == 500u && cal_u16(CAL_WK_TURB) == 750u, "500 / 750 rpm");
    CHECK(cal_f32(CAL_WK_TARGET) == 40.0f && cal_f32(CAL_WK_KI) == 0.05f, "40 rpm / Ki 0.05");
    CHECK(cal_f32(CAL_WK_OVERHEAT) == 135.0f && cal_f32(CAL_WK_OVERHEAT2) == 150.0f, "135 / 150 C");
    CHECK(cal_u8(CAL_WK_SLIP_A) == 10u && cal_u8(CAL_WK_SLIP_D) == 5u && cal_s16(CAL_WK_B2AC) == 10, "slip limits");
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--cal") == 0) {
        test_cal();
        printf(fails ? "wk_cal_bytes: %d FAILED\n" : "wk_cal_bytes: ok\n", fails);
        return fails ? 1 : 0;
    }
    test_gate();
    test_law();
    test_dry();
    test_vehicle_closed();
    test_slip_control();
    test_gate_shut_machine_runs();
    test_b5e();
    if (fails) {
        printf("test_wk_shadow: %d FAILED\n", fails);
        return 1;
    }
    printf("test_wk_shadow: ok\n");
    return 0;
}
