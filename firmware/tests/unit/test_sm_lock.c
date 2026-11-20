/* test_sm_lock.c - the lock-up REQUEST PRODUCER (app/sm_shadow.c lu_pass = 0x124570 and its callees), judged on the host
 * (2026-11-20, ~/tcu_sweep/gaps3_port.md; decode gaps3_decode.md section 1):
 *   - the calibration facts the port assumes (the group pointer struct 0x62A6C, the objects it points at, the dead gates)
 *   - a cruise in 6th .. 8th at a light pedal: request 1 at output ~ 1050 / 1225 / 1475 rpm, 2 above 2300 / 2500 / 2700,
 *     with the decode's hysteresis (1 -> 0 below ~915 .. 980 / 1170 / 1395 .. 1445, 2 -> 1 below 40 under the engage point)
 *   - fault reaction 49 (inhibit bit 2) forced -> request 0 (the bench's dump state); gear 0 / not in the mask -> inhibit bit 1
 *   - the cold-start latch: the temperature byte below 57 caps the request at level 1
 *   - the request reaches the WK shadow (in place of its stand-in): with request 1 the WK shadow goes prefill (1 pass) ->
 *     fill (13 .. 36 passes) -> engage (80 .. 150 passes) -> closed, and releases when the request drops
 * The mutated-cal negative (LU_NEG_MAP: a moved stock byte of the 6th-gear threshold map) must FAIL this test. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app/sm_shadow.h"
#include "app/wk_shadow.h"
#include "app/cal_inplace.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void feed(uint16_t rpm, uint8_t pedal, int8_t gear, unsigned passes)
{
    unsigned k;
    for (k = 0u; k < passes; k++) {
        sm_in_t in;
        memset(&in, 0, sizeof in);
        in.d646 = rpm;
        in.pedal = pedal;
        in.gear_in = gear;
        in.os_mode = 2u;
        in.q_speed = 1u;
        in.q_pedal = 1u;
        sm_shadow_pass(&in);
    }
}

static void fresh(void)
{
    sml_in_t li;
    sm_shadow_init();
    sm_lock_default_inputs(&li);
    sm_lock_set_inputs(&li);
}

static void test_cal(void)
{
    /* the group pointer struct *0xFFF84F5C = 0x62A6C: the entries this port writes out as constants */
    static const struct { uint32_t off, addr; } g[] = {
        { 0x00u, CAL_LU_REMAP }, { 0x04u, CAL_LU_ALLOW }, { 0x08u, CAL_LU_GMASK }, { 0x0Cu, CAL_LU_HYS }, { 0x10u, CAL_LU_HYS + 2u },
        { 0x14u, CAL_LU_OVR }, { 0x18u, CAL_LU_OVR + 1u }, { 0x1Cu, CAL_LU_OVR + 2u }, { 0x20u, CAL_LU_CAPLVL },
        { 0x48u, CAL_LU_OFS }, { 0x4Cu, CAL_LU_OFS + CAL_LU_OFS_STEP }, { 0x50u, CAL_LU_OFS + 2u * CAL_LU_OFS_STEP },
        { 0x54u, CAL_LU_EN + 3u }, { 0x5Cu, CAL_LU_EN + 4u }, { 0x68u, CAL_LU_G4TAB }, { 0x74u, CAL_LU_EN + 7u }, { 0x84u, CAL_LU_EN + 8u },
    };
    unsigned i;
    for (i = 0u; i < sizeof g / sizeof g[0]; i++) {
        CHECK(cal_u32(0x62A6Cu + g[i].off) == g[i].addr, "0x62A6C+%X = %X (%X)", g[i].off, cal_u32(0x62A6Cu + g[i].off), g[i].addr);
    }
    for (i = 0u; i < CAL_LU_NMAP; i++) {
        CHECK(cal_u32(0x62A6Cu + 0x24u + 4u * i) == CAL_LU_MAP0 + CAL_LU_MAP_LEN * i, "map %u pointer", i);
        CHECK(cal_u32(CAL_LU_MAP0 + CAL_LU_MAP_LEN * i) == 40u && cal_u32(CAL_LU_MAP0 + CAL_LU_MAP_LEN * i + 4u) == 10u, "map %u header 40 x 10", i);
    }
    /* the second group (*0xFFF84F60 = 0x62B1C) and the latch (*0xFFF84F64 = 0x62B24) and the A1F8 getters (*0xFFF84E30 = 0x61864) */
    CHECK(cal_u32(0x62B1Cu) == CAL_LU_CAPSPD && cal_u32(0x62B1Cu + 4u) == CAL_LU_CAPFLG && cal_u32(0x62B24u) == CAL_LU_LATCH, "cap / latch pointers");
    for (i = 0u; i < 6u; i++) {
        CHECK(cal_u32(0x61864u + 4u * i) == CAL_LU_FLAGS + i, "A1F8 flag pointer %u", i);
    }
    /* the decode's values: mask 0x01FF, hysteresis 34 / 64, overrides 05 FF FF, cap table 0 1 1 1, offsets 20 / 1500 / 20, enables 0 */
    CHECK(cal_u16(CAL_LU_GMASK) == 0x01FFu && cal_u16(CAL_LU_HYS) == 34u && cal_u16(CAL_LU_HYS + 2u) == 64u, "mask / hysteresis");
    CHECK(cal_u8(CAL_LU_OVR) == 5u && cal_u8(CAL_LU_OVR + 1u) == 0xFFu && cal_u8(CAL_LU_OVR + 2u) == 0xFFu, "override families");
    CHECK(cal_u8(CAL_LU_CAPLVL + 4u) == 0u && cal_u8(CAL_LU_CAPLVL + 5u) == 1u && cal_u8(CAL_LU_CAPLVL + 7u) == 1u, "level cap table");
    CHECK(cal_s16(CAL_LU_OFS + 4u) == 20 && cal_s16(CAL_LU_OFS + CAL_LU_OFS_STEP + 4u) == 1500 && cal_s16(CAL_LU_OFS + 2u * CAL_LU_OFS_STEP + 4u) == 20, "offsets");
    CHECK(cal_u8(CAL_LU_EN + 3u) == 0u && cal_u8(CAL_LU_EN + 4u) == 0u && cal_u8(CAL_LU_EN + 8u) == 0u, "the correction / bit-3 / bit-5 enables are 0");
    for (i = 0u; i < 100u; i++) {
        CHECK(cal_u8(CAL_LU_G4TAB + 4u + i) == 0u && cal_u8(CAL_LU_CAPFLG + 4u + i) == 0u, "the bit-4 gate table / the cap context flags are all 0 (%u)", i);
    }
    /* the allow table: family 5 -> p = gear, 6 -> 10 + gear, 50 -> 40 + gear; remap 0 / 15 / 20 / 30 / 31 -> 6, 50 .. 54 -> 50, else 5 */
    for (i = 0u; i < 8u; i++) {
        CHECK(cal_u8(CAL_LU_ALLOW + 8u + i + 5u * 8u) == i + 1u && cal_u8(CAL_LU_ALLOW + 8u + i + 6u * 8u) == 11u + i &&
                  cal_u8(CAL_LU_ALLOW + 8u + i + 50u * 8u) == 41u + i, "allow table gear %u", i + 1u);
    }
    CHECK(cal_u8(CAL_LU_REMAP + 4u + 0u) == 6u && cal_u8(CAL_LU_REMAP + 4u + 5u) == 5u && cal_u8(CAL_LU_REMAP + 4u + 52u) == 50u && cal_u8(CAL_LU_REMAP + 4u + 15u) == 6u,
          "program remap");
}

/* ramp the output speed through one gear's lock-up levels and record the engage / release points */
typedef struct { int up1, up2, dn2, dn1; } lvl_t;
static lvl_t run_gear(int gear, int pedal)
{
    lvl_t r = { -1, -1, -1, -1 };
    int rpm, prev = 0;
    fresh();
    for (rpm = 0; rpm <= 3200; rpm += 5) {
        int q;
        feed((uint16_t)rpm, (uint8_t)pedal, (int8_t)gear, 6u);
        q = sm_lock_request();
        if (q == 1 && prev == 0 && r.up1 < 0) {
            r.up1 = rpm;
        }
        if (q == 2 && prev != 2 && r.up2 < 0) {
            r.up2 = rpm;
        }
        prev = q;
    }
    for (rpm = 3200; rpm >= 0; rpm -= 5) {
        int q;
        feed((uint16_t)rpm, (uint8_t)pedal, (int8_t)gear, 6u);
        q = sm_lock_request();
        if (q == 1 && prev == 2 && r.dn2 < 0) {
            r.dn2 = rpm;
        }
        if (q == 0 && prev != 0 && r.dn1 < 0) {
            r.dn1 = rpm;
        }
        prev = q;
    }
    return r;
}

static void test_cruise(void)
{
    /* the decode's section 1.5 numbers (the filter AB30 = (D646 + AB30) / 2 truncates, so a ramp meets a threshold up to 5 rpm
     * late; the down points sit on the threshold) */
    static const struct { int gear, e1, e2, d2, d1_lo, d1_hi; } want[3] = {
        { 6, 1050, 2300, 2266, 915, 980 }, { 7, 1225, 2500, 2466, 1175, 1175 }, { 8, 1475, 2700, 2666, 1400, 1450 },
    };
    unsigned i;
    for (i = 0u; i < 3u; i++) {
        const lvl_t lo = run_gear(want[i].gear, 20), mid = run_gear(want[i].gear, 45);
        printf("gear %d pedal 20: 0->1 at %d, 1->2 at %d, 2->1 below %d, 1->0 below %d | pedal 45: %d %d %d %d\n", want[i].gear, lo.up1, lo.up2,
               lo.dn2, lo.dn1, mid.up1, mid.up2, mid.dn2, mid.dn1);
        CHECK(lo.up1 >= want[i].e1 && lo.up1 <= want[i].e1 + 8, "gear %d: engage at %d (%d)", want[i].gear, lo.up1, want[i].e1);
        CHECK(lo.up2 >= want[i].e2 && lo.up2 <= want[i].e2 + 8, "gear %d: level 2 at %d (%d)", want[i].gear, lo.up2, want[i].e2);
        CHECK(lo.dn2 >= want[i].d2 - 8 && lo.dn2 <= want[i].d2 + 8, "gear %d: 2 -> 1 below %d (%d)", want[i].gear, lo.dn2, want[i].d2);
        CHECK(lo.dn1 >= want[i].d1_lo - 8 && lo.dn1 <= want[i].d1_lo + 8, "gear %d: 1 -> 0 below %d (%d) at pedal 20", want[i].gear, lo.dn1, want[i].d1_lo);
        CHECK(mid.dn1 >= want[i].d1_hi - 8 && mid.dn1 <= want[i].d1_hi + 8, "gear %d: 1 -> 0 below %d (%d) at pedal 45", want[i].gear, mid.dn1, want[i].d1_hi);
        /* hysteresis: the release is below the engage point, the 2 -> 1 point below the 1 -> 2 point */
        CHECK(lo.dn1 < lo.up1 && lo.dn2 < lo.up2 && lo.dn2 > lo.up1, "gear %d: hysteresis", want[i].gear);
        CHECK(lo.up1 > 0 && mid.up1 > 0, "gear %d: request 1 reached", want[i].gear);
    }
    /* a heavy pedal (76): the 1 -> 2 point collapses to the level-1 point (the decode: right after level 1 at pedal >= 76) */
    {
        const lvl_t hv = run_gear(6, 76);
        CHECK(hv.up2 > 0 && hv.up2 <= hv.up1 + 20, "pedal 76: level 2 right after level 1 (%d / %d)", hv.up1, hv.up2);
    }
    /* never 3 (column t3 = 10000 everywhere) */
    {
        int rpm, mx = 0;
        fresh();
        for (rpm = 0; rpm <= 6000; rpm += 25) {
            feed((uint16_t)rpm, 30u, 8, 4u);
            mx = (sm_lock_request() > mx) ? sm_lock_request() : mx;
        }
        CHECK(mx == 2, "the request never reaches 3 (max %d)", mx);
    }
}

static void test_inhibits(void)
{
    sml_in_t li;
    const volatile sml_t *l = sm_lock_state();
    /* reaction 49 (A004+0x61): the bench's dump state: inhibit byte 0x04, request 0 at any speed */
    fresh();
    sm_lock_default_inputs(&li);
    li.react49 = 1u;
    sm_lock_set_inputs(&li);
    feed(1800u, 30u, 6, 20u);
    CHECK(sm_lock_request() == 0u && l->inh == 0x04u && l->req == 0u && l->lvl == 0u, "reaction 49: request %u inhibit %02X (dump L+0x32 = 04)", sm_lock_request(), l->inh);
    li.react49 = 0u;
    sm_lock_set_inputs(&li);
    feed(1800u, 30u, 6, 20u);
    CHECK(sm_lock_request() == 1u && l->inh == 0u, "reaction 49 gone: request %u inhibit %02X", sm_lock_request(), l->inh);
    /* the gear mask 0x01FF allows every gear 1 .. 8 (a gear outside 1 .. 8 never reaches the producer: 0xFC112 replaces it) */
    {
        int g;
        for (g = 1; g <= 8; g++) {
            feed(100u, 30u, (int8_t)g, 6u);
            CHECK((l->inh & 0x02u) == 0u && l->allow[0] == (uint8_t)g, "gear %d: allowed (p = %u)", g, l->allow[0]);
        }
    }
    /* special-mode inhibit bit 7 (A3BC+0x55) */
    li.a3bc_55 = 1u;
    sm_lock_set_inputs(&li);
    feed(1800u, 30u, 6, 3u);
    CHECK(sm_lock_request() == 0u && (l->inh & 0x80u) != 0u, "bit 7: inhibit %02X", l->inh);
    li.a3bc_55 = 0u;
    sm_lock_set_inputs(&li);
    feed(1800u, 30u, 6, 3u);
    CHECK(sm_lock_request() == 1u, "released again");
    CHECK(l->unp_n == 0u && l->err_n == 0u, "no unported / error counts (%u / %u)", (unsigned)l->unp_n, (unsigned)l->err_n);
    /* the cold-start latch (0x1251BC): below 57 the request is capped at level 1 until the temperature passes it */
    fresh();
    sm_lock_default_inputs(&li);
    li.temp_byte = 50u;
    sm_lock_set_inputs(&li);
    feed(2600u, 30u, 6, 30u);
    CHECK(sm_lock_request() == 1u && l->l3 == 1u && l->cap_n > 0u, "cold (temperature byte 50): request %u capped at 1 (latch %u)", sm_lock_request(), l->l3);
    li.temp_byte = 58u;
    sm_lock_set_inputs(&li);
    feed(2600u, 30u, 6, 30u);
    CHECK(sm_lock_request() == 2u && l->l3 == 2u, "warm (58): the cap is released for good (latch %u, request %u)", l->l3, sm_lock_request());
    li.temp_byte = 40u;
    sm_lock_set_inputs(&li);
    feed(2600u, 30u, 6, 10u);
    CHECK(sm_lock_request() == 2u, "the latch stays released (state 2) when the fluid cools again");
    /* A0A1 = AB94 */
    CHECK(l->a0a1 == l->ab94 && l->ab94 == l->req, "A0A1 / AB94 / L+0x21");
}

/* the producer feeds the WK shadow in place of its request stand-in */
static void test_into_wk(void)
{
    wk_in_t w;
    unsigned k, n[8] = { 0u }, ms = 0u;
    uint16_t seen_fill = 0u;
    fresh();
    wk_shadow_init();
    memset(&w, 0, sizeof w);
    w.os_mode = 2u; w.c2f = 1u; w.c30 = 1u; w.e612_b = 1u; w.range_ok = 1u;
    w.temp_byte = 120u; w.temp_c = 80;
    w.eng = 1800u; w.turb = 1750u; w.tq = 100; w.line = 7000u;
    for (k = 0u; k < 500u; k++) {
        /* the strategy pass (30 ms) then the WK pass (10 ms, three per strategy pass) */
        if (k % 3u == 0u) {
            feed(1800u, 23u, 6, 1u);
        }
        w.req_idx = sm_lock_request();
        ms += 10u;
        w.now_ms = (uint16_t)ms;
        wk_shadow_pass(&w);
        if (wk_shadow_state()->state < 8u) {
            n[wk_shadow_state()->state]++;
        }
        if (wk_shadow_state()->state == 3u) {
            seen_fill = 1u;
        }
        if (wk_shadow_state()->state == 6u) {
            break;
        }
    }
    CHECK(sm_lock_request() == 1u, "the cruise gives request 1 (%u)", sm_lock_request());
    CHECK(seen_fill != 0u && n[2] == 1u && n[3] >= 13u && n[3] <= 36u && n[4] >= 80u && n[4] <= 151u,
          "prefill %u pass(es), fill %u, engage %u (1 / 13..36 / 80..150)", n[2], n[3], n[4]);
    CHECK(wk_shadow_state()->state == 6u && wk_shadow_state()->closed == 1u, "closed (state %u)", wk_shadow_state()->state);
    /* the request drops (the speed falls under the release point): the WK machine releases */
    for (k = 0u; k < 600u && wk_shadow_state()->state != 1u; k++) {
        if (k % 3u == 0u) {
            feed(600u, 23u, 6, 1u);
        }
        w.req_idx = sm_lock_request();
        ms += 10u;
        w.now_ms = (uint16_t)ms;
        wk_shadow_pass(&w);
    }
    CHECK(sm_lock_request() == 0u && wk_shadow_state()->state == 1u, "request dropped: released to open (request %u, state %u)",
          sm_lock_request(), wk_shadow_state()->state);
}

/* the host cost of one strategy pass with the producer (informational; the SH2A runs this ~50-100x slower) */
static void test_cost(void)
{
    struct timespec t0, t1;
    unsigned k;
    double us;
    fresh();
    feed(1500u, 30u, 6, 20u);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (k = 0u; k < 20000u; k++) {
        feed((uint16_t)(1000u + (k % 1500u)), 30u, 6, 1u);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    us = ((double)(t1.tv_sec - t0.tv_sec) * 1e6 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e3) / 20000.0;
    printf("host cost of one sm_shadow_pass (strategy + lock-up producer): %.2f us (native x86; the SH2A runs this ~50-100x slower)\n", us);
}

int main(void)
{
    test_cal();
    test_cost();
    test_cruise();
    test_inhibits();
    test_into_wk();
    if (fails != 0) {
        printf("test_sm_lock: %d FAILED\n", fails);
        return 1;
    }
    printf("test_sm_lock: ok\n");
    return 0;
}
