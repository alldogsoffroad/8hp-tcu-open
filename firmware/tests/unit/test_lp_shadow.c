/* test_lp_shadow.c - host judgement of the READ-ONLY healthy line-pressure (Sys) shadow
 * (app/lp_shadow.c) and torque-intervention shadow (app/ti_shadow.c), 2026-11-20,
 * ~/tcu_sweep/lp_ti_shadow_port.md.
 *   test_lp_shadow            the decode's healthy Sys values at 38.9 C (0x4F) and 80 C (0x78), the
 *                             reaction-50 case, the input-torque chain; the torque-intervention arming,
 *                             phase sequences, request type and 13-bit fields
 *   test_lp_shadow --cal      lp_cal.c re-derived from the stock image (skip 77 without it)
 * The mutated-cal negatives (LP_NEG_MAP / TI_NEG_TABLE builds) must FAIL this test. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/lp_shadow.h"
#include "app/ti_shadow.h"
#include "app/lp_cal.h"
#include "app/cal_inplace.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint16_t s_ms;

static lp_in_t base(void)
{
    lp_in_t in;
    memset(&in, 0, sizeof in);
    in.c25 = 3; in.c26 = 4; in.c27 = 3; in.c2b = 3; in.c2c = 4; in.c2d = 3;
    in.cf4 = 1u; in.temp = 0x4F; in.cd2 = 0x4F; in.range = 8u; in.pos = 3u; in.d1d = 0x9E; in.c18 = 0x4B;
    in.os_mode = 1u; in.eng = 700u; in.live_sys = 500u;
    return in;
}

static void step(lp_in_t *in, unsigned n)
{
    while (n--) {
        s_ms = (uint16_t)(s_ms + 10u);
        in->now_ms = s_ms;
        lp_shadow_pass(in);
    }
}

static void gear(lp_in_t *in, int g)       /* a settled gear g (0 = N / P as the dump: gear code 3 held) */
{
    in->c25 = (int8_t)g; in->c26 = (uint8_t)(g + 1); in->c27 = (int8_t)g;
    in->c2b = (int8_t)g; in->c2c = (uint8_t)(g + 1); in->c2d = (int8_t)g;
}

static void near_u16(uint16_t v, uint16_t want, uint16_t tol, const char *what)
{
    CHECK(v + tol >= want && v <= want + tol, "%s: %u, want %u +-%u", what, v, want, tol);
}

static void test_law(void)
{
    /* the stock curve + NO temperature map (decode §1.5), 0x4F = 38.9 C and 0x78 = 80 C */
    near_u16(lp_sys_law(5500u, 0u, 0x4F, 0u), 8500u, 0u, "5500 @0x4F");
    near_u16(lp_sys_law(6000u, 0u, 0x4F, 0u), 7265u, 1u, "6000 @0x4F");
    near_u16(lp_sys_law(6000u, 0u, 0x78, 0u), 7208u, 1u, "6000 @0x78");
    near_u16(lp_sys_law(6707u, 0u, 0x4F, 0u), 6941u, 2u, "6707 @0x4F");
    near_u16(lp_sys_law(9000u, 0u, 0x4F, 0u), 5971u, 1u, "9000 @0x4F");
    near_u16(lp_sys_law(9000u, 0u, 0x78, 0u), 5920u, 1u, "9000 @0x78");
    near_u16(lp_sys_law(10672u, 0u, 0x4F, 0u), 5314u, 2u, "10672 @0x4F");
    near_u16(lp_sys_law(12200u, 0u, 0x4F, 0u), 4722u, 2u, "12200 @0x4F");
    near_u16(lp_sys_law(15000u, 0u, 0x4F, 0u), 3625u, 2u, "15000 @0x4F");
    near_u16(lp_sys_law(15000u, 0u, 0x78, 0u), 3569u, 2u, "15000 @0x78");
    near_u16(lp_sys_law(20000u, 0u, 0x4F, 0u), 500u, 0u, "20000 @0x4F");
    /* the binary branch (0x89CF5 != 0): the maximum request is Sys 500; p = 0 would be 8500 */
    CHECK(lp_sys_law(20000u, 1u, 0x4F, 20000u) == 500u, "binary 20000");
    CHECK(lp_sys_law(0u, 1u, 0x4F, 20000u) == 8500u, "binary 0");
}

static void test_idle(void)
{
    lp_in_t in = base();
    const volatile lp_t *s;
    lp_shadow_init();
    s_ms = 0u;
    /* P / N idle, no power flow: reason 0x19, 6000 mbar -> 7265 */
    step(&in, 5u);
    s = lp_shadow_state();
    CHECK(s->res == 6000u, "P idle res %u", s->res);
    CHECK(s->b16 == 0x19u, "P idle reason %02x", s->b16);
    near_u16(s->sys_i, 7265u, 1u, "P idle Sys");
    CHECK(s->cf5 == 0u, "healthy: 0x89CF5 held 0");
    CHECK(s->w.diff_n == s->w.cmp_n, "the would-be Sys differs from the live 500 on every pass (%u / %u)", s->w.diff_n, s->w.cmp_n);
    /* D (1st), engine idle, input torque ~0: reason 0x18, 5500 -> the minimum line pressure, Sys 8500 */
    in.c30 = in.c2f = 1u; gear(&in, 1); in.range = 5u; in.pos = 0u; in.eng = 700u;
    step(&in, 900u);
    s = lp_shadow_state();
    CHECK(s->b1a == 0x18u, "D idle steady reason %02x", s->b1a);
    CHECK(s->res <= 5631u, "D idle res %u", s->res);
    CHECK(s->sys_i == 8500u, "D idle Sys %u", s->sys_i);
    /* R idle (map 0) */
    gear(&in, -1); in.range = 7u;
    step(&in, 900u);
    CHECK(lp_shadow_state()->sys_i == 8500u, "R idle Sys %u", lp_shadow_state()->sys_i);
}

static void test_leaving_p(void)
{
    lp_in_t in = base();
    const volatile lp_t *s;
    unsigned i;
    unsigned held = 0;
    uint16_t last_i = 0;
    lp_shadow_init();
    s_ms = 0u;
    step(&in, 5u);                                       /* in P: the stored range is 8 */
    in.c30 = in.c2f = 1u; gear(&in, 1); in.range = 5u;   /* P -> D, the sensor still at 3 (this bench) */
    step(&in, 1u);
    s = lp_shadow_state();
    CHECK(s->b16 == 0x13u, "leaving P: reason %02x", s->b16);
    CHECK(s->res == 15000u, "leaving P res %u", s->res);
    near_u16(s->sys_i, 3625u, 2u, "leaving P Sys");
    for (i = 0u; i < 400u; i++) {
        step(&in, 1u);
        s = lp_shadow_state();
        if (s->b16 == 0x13u) {
            held++;
        }
        last_i = s->sys_i;
    }
    CHECK(held >= 195u && held <= 205u, "the 0x13 demand held %u passes (decode: ~200 = 2 s at position 3)", held);
    CHECK(last_i > 3625u, "Sys back above the 0x13 value after the demand ends (%u)", last_i);
    /* a working sensor (position 0) ends the demand at once */
    lp_shadow_init();
    s_ms = 0u;
    in = base();
    step(&in, 3u);
    in.c30 = in.c2f = 1u; gear(&in, 1); in.range = 5u; in.pos = 0u;
    step(&in, 2u);
    CHECK(lp_shadow_state()->b16 != 0x13u, "position 0: no 0x13");
}

static void test_garage(void)
{
    lp_in_t in = base();
    const volatile lp_t *s;
    unsigned i;
    unsigned at500 = 0;
    lp_shadow_init();
    s_ms = 0u;
    in.pos = 0u;
    step(&in, 5u);
    /* the garage commit: power flow changing (C2F 0 -> C30 1), BD16 = 3, a sub-object in its fill phase */
    in.c30 = 1u; in.bd15 = 1u; in.bd16 = 3u; in.fill_any = 1u; gear(&in, 1); in.range = 5u;
    step(&in, 1u);
    s = lp_shadow_state();
    CHECK(s->b12 == 1u, "the start maximum is armed");
    CHECK(s->b16 == 1u, "start maximum reason %02x", s->b16);
    CHECK(s->res == 20000u && s->sys_i == 500u, "start maximum: res %u Sys %u", s->res, s->sys_i);
    for (i = 0u; i < 6u; i++) {
        step(&in, 1u);
        s = lp_shadow_state();
        if (s->sys_i == 500u) {
            at500++;
        }
    }
    CHECK(at500 >= 1u && at500 <= 2u, "the maximum lasts %u more passes (curve: 2 at 700 rpm)", at500);
    CHECK(s->b16 == 0x10u && s->res == 9000u, "garage: reason %02x res %u", s->b16, s->res);
    near_u16(s->sys_i, 5971u, 1u, "garage Sys");
    /* the fill ends (the sub-object leaves phase 2): the maximum cannot re-arm in this shift */
    in.fill_any = 0u;
    step(&in, 3u);
    CHECK(lp_shadow_state()->sys_i != 500u, "no re-arm");
    /* engagement done */
    in.c2f = 1u; in.bd16 = 0u; in.bd15 = 0u;
    step(&in, 1000u);
    CHECK(lp_shadow_state()->sys_i == 8500u, "settled D: Sys %u", lp_shadow_state()->sys_i);
}

static void test_upshift(void)
{
    lp_in_t in = base();
    const volatile lp_t *s;
    lp_shadow_init();
    s_ms = 0u;
    in.c30 = in.c2f = 1u; gear(&in, 1); in.range = 5u; in.pos = 0u;
    step(&in, 900u);
    /* 1 -> 2: the strobe moves the gear code (0x89C2B / C), the committed gear follows at completion */
    in.c2b = 2; in.c2c = 3; in.c2d = 2; in.bd16 = 1u; in.bd15 = 1u;
    step(&in, 1u);
    s = lp_shadow_state();
    CHECK(s->b16 == 0x11u, "1->2 reason %02x", s->b16);
    CHECK(s->res == 9000u, "1->2 res %u", s->res);
    near_u16(s->sys_i, 5971u, 1u, "1->2 Sys");
    CHECK(s->b12 == 0u, "no start maximum in a gear-pair shift (BD8B = 0)");
}

static void test_r50(void)
{
    lp_in_t in = base();
    lp_shadow_init();
    s_ms = 0u;
    lp_shadow_host_r50(1u);
    step(&in, 3u);
    CHECK(lp_shadow_state()->cf5 == 1u, "reaction 50 forced");
    CHECK(lp_shadow_state()->res == 20000u && lp_shadow_state()->b16 == 1u, "reaction 50: res %u reason %02x",
          lp_shadow_state()->res, lp_shadow_state()->b16);
    CHECK(lp_shadow_state()->sys_i == 500u, "reaction 50: Sys %u (the stock's bench value)", lp_shadow_state()->sys_i);
    CHECK(lp_shadow_state()->w.diff_n == 0u, "with reaction 50 the would-be Sys equals the live 500");
    lp_shadow_host_r50(0u);
}

static void test_torque(void)
{
    lp_in_t in = base();
    /* engine off: the converter ratio map is 1000 -> 100 %; the T3 conditioning is 100 %, the blend weight 255 / 153 */
    in.t1 = in.t2 = in.t3 = 0; in.eng = 0u;
    CHECK(lp_input_torque(&in) == 0, "zero torque");
    in.t1 = 60; in.t2 = in.t3 = 60;
    CHECK(lp_input_torque(&in) == 60, "60 Nm at the engine-off ratio: %d", lp_input_torque(&in));
    /* a stalled turbine at idle: the torque multiplication 1.706 (the map's x = 0, y = 200 row) */
    in.eng = 700u; in.spd_ratio = 0u;
    CHECK(lp_input_torque(&in) == 102, "60 Nm x 1.706: %d", lp_input_torque(&in));
    /* coupling: ratio -> 0.994 */
    in.spd_ratio = 900u;
    CHECK(lp_input_torque(&in) == 59, "60 Nm x 0.994: %d", lp_input_torque(&in));
    /* the larger of the blended T2 / T3 and T1 */
    in.spd_ratio = 1000u; in.t1 = 10; in.t2 = in.t3 = 80;
    CHECK(lp_input_torque(&in) >= 79, "blend wins: %d", lp_input_torque(&in));
    /* D idle with an input torque of ~60 Nm: reason 0x18 map 1 -> ~7228 */
    {
        lp_in_t d = base();
        d.c30 = d.c2f = 1u; gear(&d, 1); d.range = 5u; d.pos = 0u; d.eng = 700u;
        d.t1 = d.t2 = d.t3 = 61; d.spd_ratio = 1000u;
        lp_shadow_init();
        s_ms = 0u;
        step(&d, 1500u);
        near_u16(lp_shadow_state()->sys_i, 7227u, 12u, "D idle 60 Nm");
    }
}

static void test_ti(void)
{
    ti_in_t in;
    const volatile ti_t *t;
    uint8_t seq[16];
    unsigned n;
    unsigned i;
    memset(&in, 0, sizeof in);
    in.os_mode = 1u;
    in.pf = 1u;
    ti_shadow_init();
    /* no shift: nothing armed, "no request", the converter's 150 Nm limit (the dump's fields) */
    ti_shadow_pass(&in);
    t = ti_shadow_state();
    CHECK(t->armed == 0u && t->type == 0u, "idle: armed %u type %u", t->armed, t->type);
    CHECK(t->fld_req == 0x1FFEu && t->fld_lim == 0x0A28u, "idle fields %04x / %04x (dump 1FFE / 0A28)", t->fld_req, t->fld_lim);
    /* garage: 8 -> 0xA -> 0xB -> 0xC (the sync progress BD76 rises through the exit thresholds) */
    in.bd16 = 3u; in.c2d = 0u; in.c27 = 1u; in.c26 = 2u; in.k7 = 9u; in.ktype = 1u; in.tq = 80; in.t3 = 80; in.turb = 800;
    in.bd76 = -30; in.temp = 120u; in.bd22 = 120u;
    n = 0u;
    for (i = 0u; i < 30u; i++) {
        in.now_ms = (uint16_t)(10u * (i + 1u));
        in.bd76 = (int16_t)(in.bd76 + 2);
        ti_shadow_pass(&in);
        t = ti_shadow_state();
        if (n == 0u || seq[n - 1u] != t->phase) {
            seq[n++] = t->phase;
        }
    }
    CHECK(t->cls == 2u && t->armed == 1u, "garage arms class 2");
    /* phase 0xB's exit test is "the last amount is 0" (0xE630C): the garage's 0 Nm passes 0xB inside one pass */
    CHECK(n == 2u && seq[0] == 0xAu && seq[1] == 0xCu, "garage sequence (%u: %x %x)", n, seq[0], seq[1]);
    CHECK(ti_shadow_state()->w.phase_n >= 4u, "the garage went 8 -> 0xA -> 0xB -> 0xC (%u phase entries)", ti_shadow_state()->w.phase_n);
    CHECK(t->fld_req == 0x1FFEu && t->out == 0, "garage: 0 Nm on this unit, no request");
    in.bd16 = 0u;
    ti_shadow_pass(&in);
    CHECK(ti_shadow_state()->armed == 0u && ti_shadow_state()->w.end_n == 1u, "the end disarms");
    /* up-shift: 8 -> 9 -> 0xA -> 0xB -> 0xC, and T3 + delta of the reduction */
    memset(&in, 0, sizeof in);
    in.os_mode = 1u; in.pf = 1u; in.bd16 = 1u; in.c2d = 1u; in.c27 = 2u; in.c26 = 2u; in.k7 = 2u; in.ktype = 1u;
    in.tq = 300; in.t3 = 300; in.turb = 3008; in.eng = 3008; in.temp = 120u; in.bd22 = 120u; in.bd76 = -40;
    ti_shadow_init();
    n = 0u;
    for (i = 0u; i < 70u; i++) {
        in.now_ms = (uint16_t)(10u * (i + 1u));
        in.on_left_fill = (uint8_t)((i >= 3u) ? 1u : 0u);
        if (i >= 25u) { in.bd76 = (int16_t)(in.bd76 + 1); }
        ti_shadow_pass(&in);
        t = ti_shadow_state();
        if (n == 0u || seq[n - 1u] != t->phase) {
            seq[n++] = t->phase;
        }
        if (t->phase == 0xAu && t->out <= -80) {
            CHECK(t->mode == 2u && t->type == 2u, "reduction: mode %u type %u", t->mode, t->type);
            CHECK(t->req_q == (300 + t->out) * 4 && t->dreq_q == t->out * 4, "reduction request %d / %d", t->req_q, t->dreq_q);
            CHECK(t->fld_req == (uint16_t)((300 + t->out) * 4 + 2000), "reduction field %04x", t->fld_req);
        }
    }
    CHECK(t->cls == 1u && n >= 4u && seq[0] == 9u && seq[1] == 0xAu && seq[2] == 0xBu && seq[3] == 0xCu, "up sequence (%u: %x %x %x %x)",
          n, seq[0], seq[1], seq[2], seq[3]);
    CHECK(ti_field13(0) == 2000u && ti_field13(-100000) == 0u && ti_field13(100000) == 0x1FFDu, "field packer");
    /* E3DE is disabled by its calibration enable byte, so the combine never lifts a zero */
    CHECK(t->e3de_en == 0u, "E3DE enable byte (0x1FE8CB) is 0");
    /* the lock-up raise B5E1 / B5E2 and E330 by value: T3 + B5E1 */
    in.bd16 = 0u;
    for (i = 0u; i < 14u; i++) { ti_shadow_pass(&in); }
    in.b5e2 = 1u; in.b5e1 = 12u;
    ti_shadow_pass(&in);
    CHECK(ti_shadow_state()->type == 1u && ti_shadow_state()->req_q == (300 + 12) * 4, "the lock-up raise: type %u req %d",
          ti_shadow_state()->type, ti_shadow_state()->req_q);
}

static void test_cal(void)
{
    uint32_t i;
    for (i = 0u; i < LP_CALP_N; i++) {
        if (LP_CALP[i] != cal_u32(0x5CFD8u + 4u * i)) { fails++; printf("FAIL LP_CALP[%u]\n", i); }
    }
    for (i = 0u; i < 9u; i++) {
        const uint32_t p = cal_u32(0x5CE3Cu + 8u * i);
        const uint32_t q = cal_u32(p + 4u);
        if (LP_MAP[i] != cal_u32(p) || LP_HOT[i] != ((q == p) ? 0u : q) || LP_G51DE0[i + 1u] != cal_u32(0x51DE0u + 4u * (i + 1u))) {
            fails++; printf("FAIL map %u\n", i);
        }
    }
    for (i = 0u; i < 81u; i++) {
        if (LP_UPIDX[i] != cal_u32(0x51E08u + 4u * i) || LP_DNIDX[i] != cal_u32(0x51F4Cu + 4u * i)) { fails++; printf("FAIL idx %u\n", i); }
    }
    for (i = 0u; i < 7u; i++) {
        const uint32_t c = cal_u32(0x5CEC0u + 8u * i);
        if (LP_UPPTR[2u * i] != cal_u32(c) || LP_UPPTR[2u * i + 1u] != cal_u32(c + 4u) ||
            cal_s16(LP_UPPTR[2u * i]) != 9000 || cal_s16(LP_UPPTR[2u * i + 1u]) != 9000) {
            fails++; printf("FAIL up ptr %u\n", i);
        }
    }
    for (i = 0u; i < 18u; i++) {
        const uint32_t c = cal_u32(0x5CF44u + 8u * i);
        if (LP_DNPTR[i] != cal_u32(c) || cal_s16(LP_DNPTR[i]) != 9000) { fails++; printf("FAIL dn ptr %u\n", i); }
    }
    for (i = 0u; i < 25u; i++) {
        const uint16_t want = (i == 0u) ? (uint16_t)cal_u32(0xE108u) : cal_u16(0xE108u + 2u * i + 2u);
        if (LP_E108[i] != want) { fails++; printf("FAIL E108 %u\n", i); }
    }
    for (i = 0u; i < 79u; i++) {
        const int16_t want = (i < 2u) ? (int16_t)cal_u32(0xE170u + 4u * i) : cal_s16(0xE170u + 8u + 2u * (i - 2u));
        if (LP_E170[i] != want) { fails++; printf("FAIL E170 %u\n", i); }
    }
    for (i = 0u; i < 8u; i++) {
        if (LP_TI_CLASS[i] != cal_u8(0x634D6u + i)) { fails++; printf("FAIL ti class %u\n", i); }
    }
    for (i = 0u; i < 24u; i++) {
        if (LP_TI_PHASE[i] != cal_u8(0x51C6Cu + i)) { fails++; printf("FAIL ti phase %u\n", i); }
    }
    /* the facts the shadow assumes about data it does not read: the class table (all zero), the Sys engine map (all zero),
     * the temperature curves of 0xEA37E (all zero) and the additive cal+0x40 (0) */
    for (i = 0u; i < 10u; i++) {
        if (cal_s16(0x1DE91Cu + 4u + 2u * i) != 0) { fails++; printf("FAIL class table %u\n", i); }
    }
    if (cal_s16(LP_CALA(0x40)) != 0) { fails++; printf("FAIL cal+0x40\n"); }
    {
        const uint32_t m[2] = { 0x1DE734u, 0x1DE758u };
        uint32_t k;
        for (k = 0u; k < 2u; k++) {
            const uint32_t n = cal_u32(m[k]);
            for (i = 0u; i < n; i++) {
                if (cal_s16(m[k] + 4u + 2u * (n + i)) != 0) { fails++; printf("FAIL temp curve %u/%u\n", k, i); }
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--cal") == 0) {
        test_cal();
        printf(fails ? "lp_cal_bytes: %d FAILED\n" : "lp_cal_bytes: ok\n", fails);
        return fails ? 1 : 0;
    }
    test_law();
    test_idle();
    test_leaving_p();
    test_garage();
    test_upshift();
    test_r50();
    test_torque();
    test_ti();
    if (fails) {
        printf("test_lp_shadow: %d FAILED\n", fails);
        return 1;
    }
    printf("test_lp_shadow: ok\n");
    return 0;
}
