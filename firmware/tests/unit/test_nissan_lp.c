/* test_nissan_lp.c - THE NISSAN LINE-PRESSURE LIVE SEAM'S GATES (2026-11-23, app/nissan_lp.h;
 * the Nissan configure only: NISSAN_LP_LIVE=1, TCU_VEHICLE_NISSAN=1).
 *
 * The real pieces, on a simulated Nissan bus: the Nissan profile (io/vehicle/profile_nissan.c:
 * the actual torque T1 from 0x180 with its rolling-counter check), the fault-reaction shadow fed
 * the same raw frames (app/fr_cal_nissan.c: option C), the healthy line-pressure shadow (fed its
 * inputs BY VALUE as app/lp_hook.c would, with t1 from vehicle_in()). Timeline as
 * test_fr_nissan: 10 ms ticks, OS mode 1 from +650 ms. Checks on nissan_lp_sys_request():
 *   (1) before OS mode 1 (the shadow skips its law): 500, gate SHADOW
 *   (2) every frame present, torque valid (P idle): the value = the shadow's sys_i = the stock
 *       curve law of its own target (lp_sys_law(res, 0, temp)) = 7265 +-1 (test_lp_shadow's
 *       decode value), gate 0
 *   (3) D idle at 60 Nm through 0x180 (raw 920): the law's 7227 +-12, = sys_i
 *   (4) torque invalid (0x180 raw 0xFFF; then a frozen counter): 500, gate TORQUE only,
 *       while the shadow's own would-be value is unchanged (not 500)
 *   (5) 0x1F9 stops at 5 s: U0100 (group 139) confirms -> fault reaction 50 active in the
 *       fault manager's masks and the shadow's 0x89CF5 = 1: 500, gate R50 (before: live)
 *   (6) OS mode back to 0 (the shadow keeps a stale sys_i): 500, gate SHADOW
 *   (7) the guard's range is the stock Sys clamp [500, 8500] and its floor is the P content */
#include "app/nissan_lp.h"
#include "app/lp_shadow.h"
#include "app/fr_shadow.h"
#include "io/vehicle/vehicle.h"
#include "bsw/drv/drv_gsbr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !DRV_RQ_SYS_LIVE
#error "test_nissan_lp.c: built with NISSAN_LP_LIVE=1"
#endif

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { g_fail++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); \
    printf("\n"); } } while (0)

#define M1_MS 650u

typedef struct { uint16_t id; uint8_t dlc; uint8_t per10; } nmsg_t;
static const nmsg_t k_msgs[] = {
    { 0x1F9u, 8u, 1u }, { 0x180u, 8u, 1u }, { 0x182u, 8u, 1u }, { 0x160u, 7u, 1u }, { 0x551u, 8u, 10u },
    { 0x280u, 8u, 2u }, { 0x284u, 8u, 2u }, { 0x285u, 8u, 2u }, { 0x354u, 8u, 4u }, { 0x35Du, 8u, 10u },
};
#define NMSG (sizeof k_msgs / sizeof k_msgs[0])

/* the scenario state */
static unsigned g_ms;
static uint32_t g_rep109, g_rep98;
static unsigned g_mpass;
static uint8_t g_ctr;
static uint16_t g_t1_raw = 800u;     /* 0x180 actual torque, 12-bit: raw x 5 - 4000 (0.1 Nm); 800 = 0 Nm */
static int g_ctr_frozen;
static uint16_t g_stop_id;
static unsigned g_stop_ms;
static int g_mode_on = 1;
static lp_in_t g_li;

static lp_in_t lp_base(void)
{
    lp_in_t in;
    memset(&in, 0, sizeof in);
    in.c25 = 3; in.c26 = 4; in.c27 = 3; in.c2b = 3; in.c2c = 4; in.c2d = 3;
    in.cf4 = 1u; in.temp = 0x4F; in.cd2 = 0x4F; in.range = 8u; in.pos = 3u; in.d1d = 0x9E; in.c18 = 0x4B;
    in.eng = 700u; in.live_sys = 500u; in.spd_ratio = 1000u;
    return in;
}

static void lp_gear(lp_in_t *in, int g)
{
    in->c25 = (int8_t)g; in->c26 = (uint8_t)(g + 1); in->c27 = (int8_t)g;
    in->c2b = (int8_t)g; in->c2c = (uint8_t)(g + 1); in->c2d = (int8_t)g;
}

static void tick(void)
{
    fr_in_t in;
    const vehicle_in_t *v;
    memset(&in, 0, sizeof in);
    g_ms += 10u;
    const unsigned ms = g_ms, t = ms / 10u;
    for (unsigned i = 0u; i < NMSG; i++) {
        const nmsg_t *m = &k_msgs[i];
        can_frame_t f;
        if (m->id == g_stop_id && ms >= g_stop_ms) {
            continue;
        }
        if (t % m->per10 != 0u) {
            continue;
        }
        memset(&f, 0, sizeof f);
        f.id = m->id;
        f.dlc = m->dlc;
        if (m->id == 0x1F9u) {
            f.data[2] = (uint8_t)((700u * 8u) >> 8);      /* 700 rpm, 0.125 rpm/bit */
            f.data[3] = (uint8_t)(700u * 8u);
        }
        if (m->id == 0x180u) {
            f.data[0] = (uint8_t)((700u * 8u) >> 8);      /* the engine-speed mirror */
            f.data[1] = (uint8_t)(700u * 8u);
            f.data[2] = (uint8_t)(g_t1_raw >> 4);
            f.data[3] = (uint8_t)((g_t1_raw & 0x0Fu) << 4);
            f.data[4] = 0x20u;                            /* the driver demand, any valid value */
        }
        f.data[6] = (uint8_t)((f.data[6] & 0xF0u) | ((g_ctr_frozen ? 3u : g_ctr) & 0x0Fu));
        vehicle_rx(CAN_BUS1, &f);
        fr_hook_rx(0u, f.id, f.dlc, f.data);
    }
    g_ctr++;
    vehicle_update_10ms();
    /* the fault-reaction shadow (app/main.c: fr_hook_10ms before lp_hook_10ms) */
    in.os_mode = (ms >= M1_MS && g_mode_on) ? 1u : 0u;
    in.e9fe = (uint8_t)(ms >= 20u);
    in.e9ff = (uint8_t)((ms == 20u) ? 1u : 2u);
    in.d6d6 = 8u;
    in.c3d = (uint8_t)(in.os_mode == 1u);
    if (in.os_mode >= 1u) {
        if (g_mpass % 2u == 0u) {
            in.u_failed = 0u;
            in.u_tested = (uint8_t)(g_mpass > 100u);
            g_rep109++;
        }
        if (g_mpass % 10u == 0u) {
            g_rep98++;
        }
        g_mpass++;
    }
    in.g98_failed = 0u;
    in.g98_tested = 1u;
    in.rep109_n = g_rep109;
    in.rep98_n = g_rep98;
    in.eng = 700u;
    in.gf_e69f = 1; in.gf_c25 = 1; in.gf_c28 = 1; in.gf_c2b = 1; in.gf_cce4 = 1; in.gf_e343 = 1;
    in.gf_e6a1 = 1;
    in.ce8 = 1u;
    in.cd2 = 0x4Fu;
    in.sup_mv = 12950u;
    in.cd0 = in.eng;
    fr_hook_feed(&in);
    /* the line-pressure shadow, its inputs by value (app/lp_hook.c's t1: nm_of(torque_actual)) */
    v = vehicle_in();
    g_li.os_mode = in.os_mode;
    g_li.now_ms = (uint16_t)ms;
    g_li.t1 = (v->q_torque_actual == Q_OK) ? (int16_t)(v->torque_actual / 10) : (int16_t)0;
    g_li.t2 = g_li.t3 = g_li.t1;
    g_li.ca0 = g_li.t1;
    lp_hook_feed(&g_li);
}

static void run_to(unsigned until_ms)
{
    while (g_ms < until_ms) {
        tick();
    }
}

static void reset(void)
{
    vehicle_init();
    fr_hook_init();
    lp_hook_init();
    g_ms = 0u;
    g_rep109 = g_rep98 = 0u;
    g_mpass = 0u;
    g_ctr = 0u;
    g_t1_raw = 800u;
    g_ctr_frozen = 0;
    g_stop_id = 0u;
    g_stop_ms = 0u;
    g_mode_on = 1;
    g_li = lp_base();
}

static int near_u16(uint16_t v, uint16_t want, uint16_t tol)
{
    return v + tol >= want && v <= want + tol;
}

static void show(const char *tag)
{
    const volatile lp_t *lp = lp_shadow_state();
    printf("%-44s t %5u ms: gate %02X request %4u | shadow sys_i %4u res %5u | torque q %u %d | "
           "r50 %u cf5 %u\n", tag, g_ms, nissan_lp_sys_gate(), nissan_lp_sys_request(), (unsigned)lp->sys_i,
           (unsigned)lp->res, (unsigned)vehicle_in()->q_torque_actual, (int)vehicle_in()->torque_actual,
           (unsigned)fr_fm_react(NLP_R50), (unsigned)fr_shadow_state()->cd.cf5);
}

int main(void)
{
    const volatile lp_t *lp = lp_shadow_state();

    /* (1) before OS mode 1 */
    reset();
    run_to(600u);
    show("(1) before OS mode 1");
    CHECK((nissan_lp_sys_gate() & NLP_G_SHADOW) != 0u && nissan_lp_sys_request() == 500u,
          "(1) the shadow skipped its law: gate %02X request %u", nissan_lp_sys_gate(), nissan_lp_sys_request());

    /* (2) healthy, P idle */
    run_to(3000u);
    show("(2) healthy, P idle, 0 Nm");
    CHECK(nissan_lp_sys_gate() == 0u, "(2) gate %02X", nissan_lp_sys_gate());
    CHECK(vehicle_in()->q_torque_actual == Q_OK, "(2) 0x180 torque valid");
    CHECK(nissan_lp_sys_request() == lp->sys_i, "(2) request %u = sys_i %u", nissan_lp_sys_request(),
          (unsigned)lp->sys_i);
    CHECK(lp->sys_i == lp_sys_law(lp->res, 0u, 0x4Fu, lp->res), "(2) sys_i %u = the curve law of res %u",
          (unsigned)lp->sys_i, (unsigned)lp->res);
    CHECK(near_u16(nissan_lp_sys_request(), 7265u, 1u) && nissan_lp_sys_request() != 500u,
          "(2) P idle 6000 mbar -> 7265: %u", nissan_lp_sys_request());

    /* (3) D idle at 60 Nm (0x180 raw 920 = 600 x 0.1 Nm) */
    g_t1_raw = 920u;
    g_li.c30 = g_li.c2f = 1u;
    lp_gear(&g_li, 1);
    g_li.range = 5u;
    g_li.pos = 0u;
    run_to(3000u + 15000u);
    show("(3) healthy, D idle, 60 Nm");
    CHECK(vehicle_in()->q_torque_actual == Q_OK && vehicle_in()->torque_actual == 600, "(3) T1 60.0 Nm (%d)",
          (int)vehicle_in()->torque_actual);
    CHECK(nissan_lp_sys_gate() == 0u && nissan_lp_sys_request() == lp->sys_i, "(3) live: gate %02X %u / %u",
          nissan_lp_sys_gate(), nissan_lp_sys_request(), (unsigned)lp->sys_i);
    CHECK(near_u16(nissan_lp_sys_request(), 7227u, 12u), "(3) D idle 60 Nm -> 7227 +-12: %u",
          nissan_lp_sys_request());
    {
        /* (4) torque invalid: 0x180's T1 = 0xFFF, then a frozen counter */
        const uint16_t live = nissan_lp_sys_request();
        g_t1_raw = 0xFFFu;
        run_to(g_ms + 200u);
        show("(4a) torque 0xFFF");
        CHECK(nissan_lp_sys_gate() == NLP_G_TORQUE && nissan_lp_sys_request() == 500u,
              "(4a) torque invalid: gate %02X request %u", nissan_lp_sys_gate(), nissan_lp_sys_request());
        CHECK(lp->sys_i != 500u, "(4a) the shadow's would-be value stays %u (only the gate fell)",
              (unsigned)lp->sys_i);
        g_t1_raw = 920u;
        run_to(g_ms + 1500u);
        CHECK(nissan_lp_sys_gate() == 0u && near_u16(nissan_lp_sys_request(), live, 12u),
              "(4) torque back: gate %02X request %u", nissan_lp_sys_gate(), nissan_lp_sys_request());
        g_ctr_frozen = 1;
        run_to(g_ms + 100u);
        show("(4b) 0x180 counter frozen");
        CHECK(nissan_lp_sys_gate() == NLP_G_TORQUE && nissan_lp_sys_request() == 500u,
              "(4b) frozen counter: gate %02X request %u", nissan_lp_sys_gate(), nissan_lp_sys_request());
        g_ctr_frozen = 0;
    }

    /* (5) reaction 50: 0x1F9 stops (U0100, group 139 -> reactions 13 and 50) */
    reset();
    g_stop_id = 0x1F9u;
    g_stop_ms = 5000u;
    run_to(5000u);
    show("(5) before 0x1F9 stops");
    CHECK(nissan_lp_sys_gate() == 0u && nissan_lp_sys_request() != 500u, "(5) live before: gate %02X %u",
          nissan_lp_sys_gate(), nissan_lp_sys_request());
    run_to(6500u);
    show("(5) 0x1F9 stopped at 5 s (U0100)");
    CHECK(fr_fm_confirmed(139u) != 0u, "(5) U0100 (group 139) confirmed");
    CHECK(fr_fm_react(NLP_R50) != 0u, "(5) fault reaction 50 active");
    CHECK((nissan_lp_sys_gate() & NLP_G_R50) != 0u && nissan_lp_sys_request() == 500u,
          "(5) reaction 50: gate %02X request %u", nissan_lp_sys_gate(), nissan_lp_sys_request());
    CHECK((nissan_lp_sys_gate() & NLP_G_TORQUE) == 0u, "(5) the torque is still valid (0x180 present)");

    /* (6) OS mode back to 0: the shadow skips, sys_i stale */
    reset();
    run_to(3000u);
    CHECK(nissan_lp_sys_gate() == 0u, "(6) live first");
    g_mode_on = 0;
    run_to(3100u);
    show("(6) OS mode 0 again");
    CHECK(nissan_lp_sys_gate() == NLP_G_SHADOW && nissan_lp_sys_request() == 500u && lp->sys_i != 500u,
          "(6) stale shadow: gate %02X request %u sys_i %u", nissan_lp_sys_gate(), nissan_lp_sys_request(),
          (unsigned)lp->sys_i);

    /* (7) the guard's range */
    CHECK(DRV_RQ_SYS_LIVE_MIN == 500u && DRV_RQ_SYS_LIVE_MAX == 8500u && DRV_RQ_SYS_LIVE_MIN == DRV_RQ_PCONT_SYS &&
          NLP_SYS_MIN == DRV_RQ_SYS_LIVE_MIN && NLP_SYS_MAX == DRV_RQ_SYS_LIVE_MAX && NLP_SYS_SAFE == DRV_RQ_PCONT_SYS,
          "(7) the Sys range");

    if (g_fail == 0) {
        printf("test_nissan_lp: ok\n");
    }
    return g_fail != 0;
}
