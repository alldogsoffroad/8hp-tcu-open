/* test_ti_amounts.c - host judgement of the torque-intervention AMOUNTS (app/ti_shadow.c),
 * 2026-11-20, ~/tcu_sweep/ti_amounts_wk_decode.md Task A + ~/tcu_sweep/ti_amounts_port.md.
 *   test_ti_amounts          the decode's trace: 1->2 at 3000 rpm / 300 Nm warm = -90 Nm (class 0) / -132 Nm
 *                            (class 1); -54 Nm at 2000, -144 Nm at 5000 (class 0); the temperature factor 35 %
 *                            at 0 C and 100 % from 50 C; the accelerating ramp-in (-1 -2 -5 -8 -15 -23 -33 -46 -64 -88);
 *                            the fade-out against the sync progress; the 10-pass ramp-out after the end; the coast
 *                            downshift blip within +40..+150 Nm; the garage = 0 Nm; the 13-bit field = (T3 + delta) x 4 + 2000
 *   test_ti_amounts --cal    ti_cal.c re-derived from the stock image
 * Exit 77 (a loud SKIP from app/cal_inplace.c) when the stock image file is missing.
 * The mutated-map negative (TI_NEG_AMT: one stock byte of the 1->2 torque map moved) must FAIL this test. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/ti_shadow.h"
#include "app/ti_cal.h"
#include "app/lp_cal.h"
#include "app/cal_inplace.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint16_t s_ms;

/* a 1 -> 2 up-shift at turbine `turb`, torque `tq`, fluid temperature byte `temp` (C + 40), kind `k7` (2 = class 0, 4 = class 1) */
static ti_in_t up_in(int16_t turb, int16_t tq, uint8_t temp, uint8_t k7)
{
    ti_in_t in;
    memset(&in, 0, sizeof in);
    in.os_mode = 1u;
    in.bd16 = 1u;
    in.bd22 = temp;
    in.pf = 1u;
    in.t3 = tq;
    in.tq = tq;
    in.turb = turb;
    in.eng = turb;
    in.bd76 = -40;
    in.c2d = 1u; in.c27 = 2u; in.c26 = 2u;
    in.k7 = k7;
    in.ktype = 1u;
    in.temp = temp;
    return in;
}

static void run1(ti_in_t *in)
{
    s_ms = (uint16_t)(s_ms + 10u);
    in->now_ms = s_ms;
    ti_shadow_pass(in);
}

/* drive an up-shift: 8 -> 9 (the flare limiter) -> 0xA (the ramp-in) ; returns the amounts of the 0xA passes */
static int run_up(ti_in_t *in, int16_t *amts, unsigned n_amts, int16_t *final_amt, unsigned *a_passes)
{
    unsigned i;
    unsigned n = 0u;
    ti_shadow_init();
    s_ms = 0u;
    for (i = 0u; i < 50u; i++) {                           /* the arm (0 -> 8 -> 9), phase 9 (the on-coming object leaves its fill after 3 passes), phase 0xA */
        if (i == 3u) { in->on_left_fill = 1u; }
        run1(in);
        if (ti_shadow_state()->phase == 0xAu) {
            if (n < n_amts) { amts[n] = ti_shadow_state()->out; }
            n++;
            *final_amt = ti_shadow_state()->out;
        } else if (n > 0u) {
            break;
        }
    }
    *a_passes = n;
    return (ti_shadow_state()->phase >= 0xAu) ? 0 : 1;
}

static void test_amounts(void)
{
    int16_t amts[64];
    int16_t fin = 0;
    unsigned n = 0u;
    ti_in_t in;
    unsigned i;

    /* the decode's trace: 1 -> 2 at 3000 rpm / 300 Nm, fluid 80 C (byte 120: the factor map is 255 from byte 98) */
    in = up_in(3008, 300, 120u, 2u);
    (void)run_up(&in, amts, 64u, &fin, &n);
    CHECK(ti_shadow_state()->cls == 1u, "an up-shift is class 1 (TI+4)");
    CHECK(fin >= -96 && fin <= -84, "class 0, 3000 rpm / 300 Nm: %d Nm, decode -90", fin);
    CHECK(amts[0] == 0, "the first pass of phase 0xA is the blend's zero (the stamp = now): %d", amts[0]);
    CHECK(amts[1] >= -3 && amts[1] <= -1, "the ramp-in starts at about -2: %d", amts[1]);
    CHECK(n >= 10u, "the ramp-in is 10 passes before the hold: %u passes in 0xA", n);
    for (i = 1u; i < 10u && i < n; i++) {
        CHECK(amts[i] <= amts[i - 1u], "the ramp-in accelerates monotonically (pass %u: %d after %d)", i, amts[i], amts[i - 1u]);
    }
    CHECK(amts[4] >= -18 && amts[4] <= -12, "the 4th ramp-in value: %d, decode -15", amts[4]);
    CHECK(amts[8] >= -69 && amts[8] <= -58, "the 8th ramp-in value: %d, decode -64", amts[8]);
    CHECK(amts[9] >= -91 && amts[9] <= -84, "the 9th ramp-in value: %d, decode -88", amts[9]);
    /* class 1 (the in-window kind 4): -132 */
    in = up_in(3008, 300, 120u, 4u);
    (void)run_up(&in, amts, 64u, &fin, &n);
    CHECK(fin >= -138 && fin <= -126, "class 1, 3000 rpm / 300 Nm: %d Nm, decode -132", fin);
    /* against the turbine speed (class 0, 300 Nm): 2000 -> -54, 5000 -> -144 */
    in = up_in(2000, 300, 120u, 2u);
    (void)run_up(&in, amts, 64u, &fin, &n);
    CHECK(fin >= -60 && fin <= -48, "2000 rpm: %d Nm, decode -54", fin);
    in = up_in(5000, 300, 120u, 2u);
    (void)run_up(&in, amts, 64u, &fin, &n);
    CHECK(fin >= -150 && fin <= -138, "5000 rpm: %d Nm, decode -144", fin);
    /* the temperature factor: 0 C (byte 40) = 35 %, 15 C (55) = 40 %, 50 C (90) = 100 % */
    in = up_in(3008, 300, 40u, 2u);
    (void)run_up(&in, amts, 64u, &fin, &n);
    CHECK(fin >= -34 && fin <= -29, "0 C: %d Nm, decode -90 x 35 %% = -31", fin);
    in = up_in(3008, 300, 90u, 2u);
    (void)run_up(&in, amts, 64u, &fin, &n);
    CHECK(fin >= -96 && fin <= -84, "50 C: %d Nm (100 %%)", fin);
    /* the field: (T3 + delta) x 4 + 2000 */
    {
        const volatile ti_t *t = ti_shadow_state();
        CHECK(t->type == 2u && t->mode == 2u, "a reduction: type %u mode %u", t->type, t->mode);
        CHECK(t->fld_req == (uint16_t)((300 + t->out) * 4 + 2000), "the field %04x, want (T3 + %d) x 4 + 2000", t->fld_req, t->out);
        CHECK(t->fld_req < 0x1FFEu, "the request field drops below 1FFE during the up-shift: %04x", t->fld_req);
    }
}

static void test_fade_and_end(void)
{
    ti_in_t in = up_in(2000, 300, 120u, 2u);
    int16_t amts[64];
    int16_t fin = 0;
    unsigned n = 0u;
    int16_t seq[12];
    unsigned k;
    const int16_t bd76s[] = { -9, -8, -6, -3, 2 };
    (void)run_up(&in, amts, 64u, &fin, &n);
    /* the sync progress rises: phase 0xA ends at BD76 >= -10 (class 0 1->2: -10 .. -12) */
    for (k = 0u; k < 60u && ti_shadow_state()->phase == 0xAu; k++) {
        in.bd76 = (int16_t)(in.bd76 + 1);
        run1(&in);
    }
    CHECK(ti_shadow_state()->phase == 0xBu, "phase 0xA ends on the sync progress: phase %x at BD76 %d", ti_shadow_state()->phase, in.bd76);
    CHECK(in.bd76 >= -13 && in.bd76 <= -9, "the exit threshold %d, decode about -10..-12", in.bd76);
    /* the fade: 100 / 45 / 30 / 20 / 10 / 0 % of -90 over BD76 -10, -9, -8, -6, -3, +2 */
    seq[0] = ti_shadow_state()->out;
    for (k = 0u; k < 5u; k++) {
        in.bd76 = bd76s[k];
        run1(&in);
        seq[k + 1u] = ti_shadow_state()->out;
    }
    CHECK(seq[0] >= -60 && seq[0] <= -48, "the fade starts from the hold (2000 rpm): %d", seq[0]);
    CHECK(seq[1] == (int16_t)(seq[0] * 45 / 100), "BD76 -9 (45 %%): %d of %d", seq[1], seq[0]);
    CHECK(seq[2] == (int16_t)(seq[0] * 30 / 100), "BD76 -8 (30 %%): %d of %d", seq[2], seq[0]);
    CHECK(seq[3] == (int16_t)(seq[0] * 20 / 100), "BD76 -6 (20 %%): %d of %d", seq[3], seq[0]);
    CHECK(seq[4] == (int16_t)(seq[0] * 10 / 100), "BD76 -3 (10 %%): %d of %d", seq[4], seq[0]);
    CHECK(seq[5] == 0, "BD76 +2 (0 %%): %d", seq[5]);
    for (k = 0u; k < 5u; k++) {
        CHECK(seq[k + 1u] >= seq[k], "the fade never deepens (%d after %d)", seq[k + 1u], seq[k]);
    }
    /* the y >= 3000 map of the fade (100 / 60 / 40 / 25 / 15 / 0 %) */
    {
        ti_in_t hi = up_in(3500, 300, 120u, 2u);
        (void)run_up(&hi, amts, 64u, &fin, &n);
        for (k = 0u; k < 60u && ti_shadow_state()->phase == 0xAu; k++) {
            hi.bd76 = (int16_t)(hi.bd76 + 1);
            run1(&hi);
        }
        seq[0] = ti_shadow_state()->out;
        hi.bd76 = -9;
        run1(&hi);
        CHECK((int)ti_shadow_state()->out <= (int)(seq[0] * 60 / 100) + 1 && ti_shadow_state()->out >= (int16_t)(seq[0] * 60 / 100 - 2),
              "turbine 3500: BD76 -9 gives 60 %%: %d of %d", ti_shadow_state()->out, seq[0]);
    }
    /* the end: the 10-pass linear ramp-out of the last (negative) output */
    {
        ti_in_t e = up_in(3008, 300, 120u, 2u);
        int16_t last;
        (void)run_up(&e, amts, 64u, &fin, &n);
        last = ti_shadow_state()->out;
        CHECK(last < -60, "a reduction is running at the end: %d", last);
        e.bd16 = 0u;
        run1(&e);
        CHECK(ti_shadow_state()->armed == 0u && ti_shadow_state()->w.end_n == 1u, "the end disarms");
        CHECK(ti_shadow_state()->out == last, "the ramp-out starts at the last value: %d / %d", ti_shadow_state()->out, last);
        for (k = 0u; k < 5u; k++) {
            run1(&e);
        }
        CHECK(ti_shadow_state()->out >= last / 2 - 4 && ti_shadow_state()->out <= last / 2 + 4, "half way after 5 passes: %d of %d", ti_shadow_state()->out, last);
        for (k = 0u; k < 6u; k++) {
            run1(&e);
        }
        CHECK(ti_shadow_state()->out == 0, "the ramp-out is over after 10 passes: %d", ti_shadow_state()->out);
        CHECK(ti_shadow_state()->w.ramp_n >= 8u, "ramp passes %u", ti_shadow_state()->w.ramp_n);
        CHECK(ti_shadow_state()->fld_req == 0x1FFEu || ti_shadow_state()->type == 0u, "no request after the ramp-out");
    }
}

static void test_down_and_garage(void)
{
    /* the coast down-shift 2 -> 1: torque <= 0, high sync speed, warm: a blip of +40 .. +150 Nm (decode: +42 .. +102 at 2500 rpm) */
    ti_in_t d;
    unsigned k;
    int16_t peak = 0;
    memset(&d, 0, sizeof d);
    d.os_mode = 1u; d.bd16 = 2u; d.pf = 1u;
    d.bd22 = 120u; d.temp = 120u;
    d.tq = -20; d.t3 = -20;
    d.turb = 2800; d.eng = 2800; d.sync_to = 2800;
    d.c2d = 2u; d.c27 = 1u; d.c26 = 2u; d.k7 = 4u; d.ktype = 1u;
    d.bd76 = -40;
    d.bd80 = 200;                                        /* the gate: a flare beyond the threshold */
    ti_shadow_init();
    s_ms = 0u;
    for (k = 0u; k < 60u; k++) {
        run1(&d);
        if (ti_shadow_state()->out > peak) {
            peak = ti_shadow_state()->out;
        }
        if (k == 40u) {
            d.bd76 = 0;
        }
    }
    CHECK(ti_shadow_state()->cls == 0u, "a down-shift is class 0");
    CHECK(ti_shadow_state()->w.inc_n > 0u, "the coast down-shift produced a positive amount");
    CHECK(peak >= 40 && peak <= 150, "the coast blip peak %d Nm, decode +40 .. +150", peak);
    /* under power the same shift is a reduction or zero, never a blip */
    {
        ti_in_t p = d;
        int16_t mx = 0;
        p.tq = 250; p.t3 = 250;
        ti_shadow_init();
        s_ms = 0u;
        for (k = 0u; k < 60u; k++) {
            run1(&p);
            if (ti_shadow_state()->out > mx) {
                mx = ti_shadow_state()->out;
            }
            if (k == 40u) { p.bd76 = 0; }
        }
        CHECK(mx <= 0, "under power a down-shift never adds torque: %d", mx);
    }
    /* the garage: 0 Nm on this unit (every base map is flat 145), the phases 8 -> 0xA -> 0xB -> 0xC */
    {
        ti_in_t g;
        int16_t mn = 0;
        int16_t mx = 0;
        memset(&g, 0, sizeof g);
        g.os_mode = 1u; g.bd16 = 3u; g.pf = 1u; g.bd22 = 120u; g.temp = 120u;
        g.tq = 80; g.t3 = 80; g.turb = 800; g.eng = 800;
        g.c2d = 0u; g.c27 = 1u; g.c26 = 2u; g.k7 = 9u; g.ktype = 1u;
        g.bd76 = -30;
        ti_shadow_init();
        s_ms = 0u;
        for (k = 0u; k < 40u; k++) {
            run1(&g);
            if (ti_shadow_state()->out < mn) { mn = ti_shadow_state()->out; }
            if (ti_shadow_state()->out > mx) { mx = ti_shadow_state()->out; }
            g.bd76 = (int16_t)(g.bd76 + 2);
        }
        CHECK(ti_shadow_state()->cls == 2u && ti_shadow_state()->armed == 1u, "garage arms class 2");
        CHECK(mn == 0 && mx == 0, "garage: 0 Nm (%d .. %d)", mn, mx);
        CHECK(ti_shadow_state()->fld_req == 0x1FFEu && ti_shadow_state()->type == 0u, "garage: no request, field %04x", ti_shadow_state()->fld_req);
        CHECK(ti_shadow_state()->phase == 0xCu, "garage reaches phase 0xC (%x)", ti_shadow_state()->phase);
    }
}

static void test_calibration(void)
{
    /* the facts the decode quotes, read in place */
    CHECK(cal_u8(0x1FE8CBu) == 0u, "E3DE is disabled by its enable byte 0x1FE8CB");
    CHECK(cal_u8(0x1FE840u) == 1u && cal_u8(0x1FE841u) == 0u, "the increase enables: down 1, up 0");
    CHECK(cal_u16(0x1FF126u) == 200u && cal_u16(0x1FF128u) == 200u, "the phase timeouts are 200 passes");
    CHECK(cal_u8(0x1FE83Eu) == 40u, "the temperature gate 40");
    CHECK(cal_u8(0x1FE854u) == 10u && cal_u8(0x1FE844u) == 10u, "the ramp-out lengths are 10 passes");
    CHECK(cal_s16(TI_B_5814[0][6]) == -30 && cal_s16(TI_B_5814[0][8]) == -80, "1->2 class 0: tau %d end %d, decode -30 / -80",
          cal_s16(TI_B_5814[0][6]), cal_s16(TI_B_5814[0][8]));
    CHECK(cal_u8(0x1FE8F5u) == 0u, "BD7A is the turbine speed (flag byte 0x1FE8F5 = 0)");
}

/* ti_cal.c re-derived from the stock image */
static int cal_bytes(void)
{
    unsigned i, j;
    int bad = 0;
    for (i = 0u; i < 10u; i++) {
        const int32_t v = (int32_t)cal_u32(0x50804u + 4u * i);
        if (TI_IDX_GEAR[i] != ((v < 0 || v >= 9) ? 0xFFu : (uint8_t)v)) { bad++; }
    }
    for (i = 0u; i < 81u; i++) {
        const int32_t u = (int32_t)cal_u32(0x5082Cu + 4u * i), d4 = (int32_t)cal_u32(0x50970u + 4u * i), d = (int32_t)cal_u32(0x50AB4u + 4u * i);
        if (TI_IDX_UP[i] != ((u < 0 || u >= 7) ? 0xFFu : (uint8_t)u)) { bad++; }
        if (TI_IDX_DN4[i] != ((d4 < 0 || d4 >= 7) ? 0xFFu : (uint8_t)d4)) { bad++; }
        if (TI_IDX_DN[i] != ((d < 0 || d >= 18) ? 0xFFu : (uint8_t)d)) { bad++; }
    }
    {
        static const uint32_t t584[5] = { 0x50BF8u, 0x50D3Cu, 0x50E80u, 0x50FC4u, 0x51108u };
        static const uint32_t t5814[4] = { 0x5124Cu, 0x51390u, 0x514D4u, 0x51618u };
        static const uint32_t t5892[4] = { 0x5175Cu, 0x518A0u, 0x519E4u, 0x51B28u };
        for (j = 0u; j < 5u; j++) { for (i = 0u; i < 81u; i++) { const int32_t v = (int32_t)cal_u32(t584[j] + 4u * i); if (TI_IDX_584[j][i] != ((v < 0 || v >= 35) ? 0xFFu : (uint8_t)v)) { bad++; } } }
        for (j = 0u; j < 4u; j++) { for (i = 0u; i < 81u; i++) { const int32_t v = (int32_t)cal_u32(t5814[j] + 4u * i); if (TI_IDX_5814[j][i] != ((v < 0 || v >= 28) ? 0xFFu : (uint8_t)v)) { bad++; } } }
        for (j = 0u; j < 4u; j++) { for (i = 0u; i < 81u; i++) { const int32_t v = (int32_t)cal_u32(t5892[j] + 4u * i); if (TI_IDX_5892[j][i] != ((v < 0 || v >= 61) ? 0xFFu : (uint8_t)v)) { bad++; } } }
    }
#define BLK(arr, tab, n, nw) \
    for (i = 0u; i < (n); i++) { \
        const uint32_t b = cal_u32((tab) + 8u * i); \
        for (j = 0u; j < (nw); j++) { \
            uint32_t v = cal_u32(b + 4u * j); \
            if (v == b) { v = 0u; } \
            if ((arr)[i][j] != v) { bad++; } \
        } \
    }
    BLK(TI_B_GEAR, 0x55868u, 9u, 13u)
    BLK(TI_B_UP, 0x55D6Cu, 7u, 13u)
    BLK(TI_B_584, 0x55AE4u, 35u, 4u)
    BLK(TI_B_5814, 0x55440u, 28u, 10u)
    BLK(TI_B_DN, 0x56E0Cu, 18u, 4u)
    BLK(TI_B_DN4, 0x55658u, 7u, 9u)
    BLK(TI_B_5892, 0x56B00u, 61u, 14u)
#undef BLK
    /* every non-zero block word is at or above the in-place limit (never read from our own flash) */
    for (i = 0u; i < 13u; i++) { for (j = 0u; j < 9u; j++) { if (TI_B_GEAR[j][i] != 0u && TI_B_GEAR[j][i] < CAL_FLASH_CAL_BASE) { bad++; } } }
    return bad;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--cal") == 0) {
        const int b = cal_bytes();
        printf(b ? "ti_cal_bytes: %d FAILED\n" : "ti_cal_bytes: ok\n", b);
        return b ? 1 : 0;
    }
    test_calibration();
    test_amounts();
    test_fade_and_end();
    test_down_and_garage();
    if (fails) {
        printf("test_ti_amounts: %d FAILED\n", fails);
        return 1;
    }
    printf("test_ti_amounts: ok\n");
    return 0;
}
