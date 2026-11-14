/* Unit and closed-loop test: the stock range / park logic (M4,
 * ~/tcu_sweep/range_logic_port.md) — the arbiter 0xA557C (app/range.c), the
 * output law 0xCFFDE / 0xE88FA / 0xE8B56 / 0xE89DE / 0xE8888
 * (app/park_law.c), the cross-check (safety/park_mon.c), and the request
 * law's D-M4-7 on-time limit (drv_rq_window_step).
 *
 * The loop: CAN frames (0x2EA on bus 1 with the real CRC-8 and counter,
 * 0x210 on bus 2) -> FCA profile -> park_tick_10ms() -> the pinned cells and
 * the slot getters. sensors_in() is provided here (the output speed comes
 * from the scenario).
 */
#include "app/park.h"
#include "app/park_cells.h"
#include "bsw/com/crc8.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_park.h"
#include "bsw/drv/drv_reqlaw.h"
#include "io/vehicle/vehicle.h"
#include "io/sensors/sensors.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); fails++; } } while (0)

/* ---------------------------------------------------------------- sensors stand-in */

static sensors_in_t s_z;

const sensors_in_t *sensors_in(void)
{
    return &s_z;
}

/* ---------------------------------------------------------------- scenario driver */

static struct {
    uint8_t lever_code;         /* 1 P, 2 R, 3 N, 4 D (0 no change, 7 invalid) */
    bool    send_2ea, send_210, bad_crc;
} sc;

static uint8_t s_ctr[0x800];
static unsigned s_bound_violations;

static void send(can_bus_t bus, uint16_t id, uint8_t dlc, bool prot, const uint8_t *data)
{
    can_frame_t f = { .id = id, .dlc = dlc };

    memcpy(f.data, data, dlc);
    if (prot) {
        s_ctr[id] = (uint8_t)((s_ctr[id] + 1u) & 0x0Fu);
        f.data[dlc - 2u] = (uint8_t)((f.data[dlc - 2u] & 0x0Fu) | (uint8_t)(s_ctr[id] << 4));
        f.data[dlc - 1u] = crc8_j1850(f.data, (uint8_t)(dlc - 1u));
        if (sc.bad_crc && id == 0x2EAu) {
            f.data[dlc - 1u] ^= 0x55u;
        }
    }
    vehicle_rx(bus, &f);
}

static void set_speed(rpm_t rpm, quality_t q)
{
    s_z.output_speed = rpm;
    s_z.q_output_speed = q;
}

static void reset_all(void)
{
    memset(&sc, 0, sizeof sc);
    memset(&s_z, 0, sizeof s_z);
    set_speed(0, Q_OK);
    s_bound_violations = 0u;
    CELL_TMP_BYTE = 67u;            /* 27 °C: the dump's 0x89C13 = 0x43 */
    vehicle_init();
    park_init();
}

static void step(void)
{
    const uint8_t l[5] = { sc.lever_code, 0, 0, 0, 0 };

    if (sc.send_2ea) {
        send(CAN_BUS1, 0x2EAu, 5u, true, l);
    }
    if (sc.send_210) {
        send(CAN_BUS2, 0x210u, 5u, false, l);
    }
    vehicle_update_10ms();
    park_tick_10ms();
    {
        const uint16_t r = park_slot7_release(), h = park_slot11_hold();
        if ((r != 0u && r != DRV_PARK_ON_REQ) || (h != 0u && h != DRV_PARK_ON_REQ)) {
            s_bound_violations++;
        }
    }
}

static void run(unsigned n)
{
    for (unsigned i = 0u; i < n; i++) {
        step();
    }
}

static void lever(uint8_t code)
{
    sc.lever_code = code;
    sc.send_2ea = sc.send_210 = true;
}

static void no_lever(void)
{
    sc.send_2ea = sc.send_210 = false;
}

#define REL() park_slot7_release()
#define HOLD() park_slot11_hold()

/* ---------------------------------------------------------------- tests */

/* the u8 map 0xB6D6C on the stock maps 0x1B5044 / 0x1B5050 */
static void test_maps(void)
{
    CHECKV(park_law_engage_ticks(0u), 100);     /* below the first x: y[0] */
    CHECKV(park_law_engage_ticks(10u), 100);    /* −30 °C */
    CHECKV(park_law_engage_ticks(25u), 50);     /* −15 °C */
    CHECKV(park_law_engage_ticks(40u), 30);     /*   0 °C */
    CHECKV(park_law_engage_ticks(67u), 20);     /*  27 °C: 30 − trunc(15·27/40) = 20 → 200 ms */
    CHECKV(park_law_engage_ticks(65u), 21);     /*  25 °C: 30 − trunc(15·25/40 = 9.375) */
    CHECKV(park_law_engage_ticks(80u), 15);     /*  40 °C */
    CHECKV(park_law_engage_ticks(200u), 15);    /* clamped */
    CHECKV(park_law_engage_ticks(17u), 77);     /* 100 − trunc(50·7/15 = 23.3) */
    CHECKV(park_law_window_ticks(20u), 0);
    CHECKV(park_law_window_ticks(58u), 0);
    CHECKV(park_law_window_ticks(59u), 0);
    CHECKV(park_law_window_ticks(60u), 20);     /* from 20 °C: 200 ms */
    CHECKV(park_law_window_ticks(67u), 20);
    CHECKV(park_law_window_ticks(255u), 20);
}

/* no lever frames: the stock's Auto-P — and the driving dump's cells */
static void test_auto_p(void)
{
    reset_all();
    no_lever();
    step();                                     /* tick 1: 0 → 3 (engaging) */
    CHECKV(PCELL_TGT, 8);
    CHECKV(PCELL_TGT_C, 0xF7);
    CHECKV(range_reason(), 0x17);
    CHECKV(PCELL_AE29, 1);
    CHECKV(park_law_lock(), 1);
    CHECKV(park_law_state(), 3);
    CHECK(REL() == 0u && HOLD() == 0u);
    run(18u);                                   /* tick 19: timer 18 */
    CHECKV(park_law_state(), 3);
    step();                                     /* tick 20: timer 19 */
    CHECKV(park_law_state(), 3);
    step();                                     /* tick 21: timer 20 >= map(27 °C) */
    CHECKV(park_law_state(), 4);
    run(300u);
    /* the stock driving dump, cell for cell */
    CHECK(PCELL_ARB[0] == 0x00u && PCELL_ARB[1] == 0x08u && PCELL_ARB[2] == 0x17u);
    CHECK(PCELL_OBJ[0] == 0x04u && PCELL_OBJ[1] == 0x01u && PCELL_OBJ[2] == 0xFFu &&
          PCELL_OBJ[3] == 0x00u);
    CHECK(PCELL_AE28 == 0x00u && PCELL_AE29 == 0x01u);
    CHECK(PCELL_RNG == 0x08u && PCELL_RNGD == 0x08u);
    for (uint8_t k = 0u; k < 5u; k++) {
        CHECK(PCELL_OUT[k] == 0x00u && PCELL_OUT_C[k] == 0xFFu);
    }
    CHECK(REL() == 0u && HOLD() == 0u);
    CHECKV(park_mon_faults(), 0);
    CHECKV(s_bound_violations, 0);
}

/* P → R → N → D → P with the stock timings */
static void test_lever_sequence(void)
{
    reset_all();
    lever(1u);                                  /* P */
    run(50u);
    CHECKV(PCELL_TGT, 8);
    CHECKV(range_reason(), 0x15);
    CHECKV(park_law_state(), 4);
    CHECK(REL() == 0u && HOLD() == 0u);

    lever(2u);                                  /* R: release + hold in the same tick */
    step();
    CHECKV(PCELL_TGT, 6);
    CHECKV(range_reason(), 0x16);
    CHECKV(PCELL_AE28, 0);                      /* table 0x63445[6] = 0 */
    CHECKV(PCELL_AE29, 0);
    CHECKV(park_law_lock(), 0);
    CHECKV(park_law_state(), 2);                /* releasing */
    CHECK(REL() == DRV_PARK_ON_REQ && HOLD() == DRV_PARK_ON_REQ);
    run(14u);                                   /* t0 + 14: timer 14 */
    CHECKV(park_law_state(), 2);
    step();                                     /* t0 + 15: 150 ms */
    CHECKV(park_law_state(), 1);                /* released */

    lever(3u);                                  /* N */
    run(5u);
    CHECKV(PCELL_TGT, 6);
    CHECKV(park_law_state(), 1);
    CHECK(REL() == DRV_PARK_ON_REQ && HOLD() == DRV_PARK_ON_REQ);

    lever(4u);                                  /* D */
    run(5u);
    CHECKV(PCELL_TGT, 6);
    CHECKV(range_reason(), 0x16);
    CHECK(REL() == DRV_PARK_ON_REQ && HOLD() == DRV_PARK_ON_REQ);

    lever(1u);                                  /* back to P: both drop in the same tick */
    step();
    CHECKV(PCELL_TGT, 8);
    CHECKV(PCELL_AE29, 1);
    CHECKV(park_law_lock(), 1);
    CHECK(REL() == 0u && HOLD() == 0u);
    CHECKV(park_law_state(), 3);                /* engaging */
    run(19u);                                   /* t1 + 19: timer 19 */
    CHECKV(park_law_state(), 3);
    step();                                     /* t1 + 20: 200 ms at 27 °C */
    CHECKV(park_law_state(), 4);

    /* R then back to P while still releasing (state 2 → 3) */
    lever(2u);
    run(5u);
    CHECKV(park_law_state(), 2);
    lever(1u);
    step();
    CHECKV(park_law_state(), 3);
    CHECK(REL() == 0u && HOLD() == 0u);
    /* and P → R while engaging (state 3 → 2) */
    lever(2u);
    step();
    CHECKV(park_law_state(), 2);
    CHECK(REL() == DRV_PARK_ON_REQ && HOLD() == DRV_PARK_ON_REQ);
    CHECKV(park_mon_faults(), 0);
    CHECKV(s_bound_violations, 0);
}

/* the shifter sources: code 0 keeps, bad CRC falls back, timeouts → Auto-P */
static void test_lever_sources(void)
{
    reset_all();
    lever(2u);                                  /* R */
    run(20u);
    CHECK(REL() == DRV_PARK_ON_REQ);
    lever(0u);                                  /* code 0 = no change */
    run(5u);
    CHECKV(PCELL_TGT, 6);
    sc.lever_code = 1u;                         /* P on 0x210 only, 0x2EA bad CRC: the */
    sc.bad_crc = true;                          /* last good 0x2EA (R) holds until its */
    run(1u);                                    /* 50 ms timeout, then the fallback */
    CHECKV(PCELL_TGT, 6);
    run(6u);
    CHECKV(vehicle_in()->lever_src, LEVER_SRC_FALLBACK);
    CHECKV(PCELL_TGT, 8);
    sc.bad_crc = false;
    lever(2u);
    run(5u);
    CHECKV(PCELL_TGT, 6);
    sc.lever_code = 7u;                         /* invalid code on both → no valid lever */
    run(1u);
    CHECKV(PCELL_TGT, 8);
    CHECKV(range_reason(), 0x17);               /* Auto-P (stationary) */
    CHECK(REL() == 0u && HOLD() == 0u);

    /* lever lost by timeout while in R, stationary: Auto-P after > 50 ms */
    lever(4u);
    run(10u);
    CHECK(REL() == DRV_PARK_ON_REQ);
    no_lever();
    {
        unsigned n = 0u;
        while (PCELL_TGT != 8u && n < 20u) {
            step();
            n++;
        }
        CHECK(n >= 5u && n <= 7u);              /* the 50 ms profile timeout (spec 07 §3) */
        printf("  lever timeout -> Auto-P after %u ticks\n", n);
    }
    CHECKV(range_reason(), 0x17);
    CHECK(REL() == 0u && HOLD() == 0u);
    CHECKV(s_bound_violations, 0);
}

/* the output speed: no validity term, unsigned > 80 = moving */
static void test_speed_rule(void)
{
    reset_all();
    no_lever();
    set_speed(0, Q_INVALID);                    /* a sensorless bench: 0, whatever the quality */
    run(3u);
    CHECKV(PCELL_TGT, 8);
    CHECKV(range_reason(), 0x17);
    set_speed(80, Q_OK);                        /* 80 is still stationary */
    run(1u);
    CHECKV(range_reason(), 0x17);
    set_speed(81, Q_OK);                        /* MOVING WITHOUT A LEVER RELEASES PARK (stock) */
    run(1u);
    CHECKV(PCELL_TGT, 6);
    CHECKV(range_reason(), 0x16);
    CHECK(REL() == DRV_PARK_ON_REQ && HOLD() == DRV_PARK_ON_REQ);
    set_speed(-500, Q_OK);                      /* magnitude */
    run(1u);
    CHECKV(PCELL_TGT, 6);
    set_speed(500, Q_INVALID);                  /* a stale nonzero value is used as is */
    run(1u);
    CHECKV(PCELL_TGT, 6);
    set_speed(3000, Q_OK);
    lever(1u);                                  /* P at speed: target 8, park request on */
    run(1u);
    CHECKV(PCELL_TGT, 8);
    CHECKV(range_reason(), 0x15);
    CHECK(REL() == 0u && HOLD() == 0u);         /* no interlock window open: lock at once */
    CHECKV(park_mon_faults(), 0);
}

/* the lock decision and the 0xE8B56 interlock, on park_law alone */
static void test_lock_interlock(void)
{
    park_law_in_t in;

    memset(&in, 0, sizeof in);
    in.temp40 = 67u;
    in.v89d49 = 2u;
    park_law_init();
    for (int i = 0; i < 30; i++) {
        park_law_step(8u, &in);
    }
    CHECK(park_law_lock() == 1u && park_law_state() == 4u);
    CHECK(park_law_demand(2u) == 0u && park_law_demand(3u) == 0u);

    /* the global inhibit 0x89C30: lock forced 0 (hold ON even in P), the
     * window reloaded from map 0x1B5050 every pass */
    in.inhibit = 1u;
    park_law_step(8u, &in);
    CHECK(park_law_lock() == 0u && PCELL_OBJ[PO_WIN] == 20u);
    CHECK(park_law_demand(2u) == 0u && park_law_demand(3u) == 10000u);

    /* inhibit gone, moving (0x89D00 > 10, 0x89CB8 < 5, valid): the new lock
     * is refused while the window runs */
    in.inhibit = 0u;
    in.vspeed = 20;
    in.v89cb8 = 0u;
    in.v89d49 = 0u;
    {
        unsigned n = 0u;
        do {
            park_law_step(8u, &in);
            n++;
        } while (park_law_lock() == 0u && n < 50u);
        CHECKV(n, 20u);                         /* 20 − 1 per pass: lock once it reads 0 */
    }
    CHECK(park_law_demand(3u) == 0u);

    /* the same with the speed at the limit (10): not "moving" → lock at once */
    in.inhibit = 1u;
    park_law_step(8u, &in);
    in.inhibit = 0u;
    in.vspeed = 10;
    park_law_step(8u, &in);
    CHECKV(park_law_lock(), 1);
    /* 0x89CB8 >= 5 → no refusal */
    in.inhibit = 1u;
    park_law_step(8u, &in);
    in.inhibit = 0u;
    in.vspeed = 20;
    in.v89cb8 = 5u;
    park_law_step(8u, &in);
    CHECKV(park_law_lock(), 1);
    /* invalid (0x89D49 != 0) → no refusal */
    in.inhibit = 1u;
    park_law_step(8u, &in);
    in.inhibit = 0u;
    in.v89cb8 = 0u;
    in.v89d49 = 2u;
    park_law_step(8u, &in);
    CHECKV(park_law_lock(), 1);
    /* the 0xE8BAA bypass (0xFFF8B272 != 0) */
    in.inhibit = 1u;
    park_law_step(8u, &in);
    in.inhibit = 0u;
    in.v89d49 = 0u;
    in.b272 = 1u;
    park_law_step(8u, &in);
    CHECKV(park_law_lock(), 1);
    in.b272 = 0u;
    /* cold (window map 0 below 19 °C): no window, lock at once */
    in.temp40 = 50u;
    in.inhibit = 1u;
    park_law_step(8u, &in);
    CHECKV(PCELL_OBJ[PO_WIN], 0);
    in.inhibit = 0u;
    park_law_step(8u, &in);
    CHECKV(park_law_lock(), 1);
    in.temp40 = 67u;

    /* a held lock is not re-checked: the window cannot drop it */
    in.vspeed = 20;
    park_law_step(8u, &in);
    CHECKV(park_law_lock(), 1);

    /* the auxiliary request 0x89CF2: lock regardless of the target, park
     * request = (no car-wash inhibit) */
    in.aux = 1u;
    park_law_step(6u, &in);
    CHECK(park_law_lock() == 1u && PCELL_AE29 == 1u);
    in.carwash = 1u;
    park_law_step(6u, &in);
    CHECK(park_law_lock() == 1u && PCELL_AE29 == 0u);
    CHECK(park_law_demand(2u) == 10000u && park_law_demand(3u) == 0u);   /* release on, hold off */
    in.aux = 0u;
    in.carwash = 0u;

    /* the diag range overrides (0xB97F8) */
    in.diag_rng6 = 1u;
    park_law_step(5u, &in);
    CHECKV(PCELL_RNGD, 6);
    park_law_step(8u, &in);
    CHECKV(PCELL_RNGD, 8);                      /* P is kept */
    in.diag_rng6 = 0u;
    in.diag_rng8 = 1u;
    park_law_step(6u, &in);
    CHECKV(PCELL_RNGD, 8);
    in.diag_rng8 = 0u;
    /* a target > 8 leaves AE28 / AE29 unchanged (fault report path) */
    park_law_step(8u, &in);
    {
        const uint8_t a = PCELL_AE29;
        park_law_step(0x20u, &in);
        CHECKV(PCELL_AE29, a);
    }
    /* the diag output override 0xBB110 / 0xBB120 (k = 3, hold forced on) */
    in.diag_out_en = 1u << 3;
    in.diag_out_val = 1u << 3;
    park_law_step(8u, &in);
    CHECKV(park_law_demand(3u), 10000);
    CHECK(park_law_integrity_ok());
    in.diag_out_en = 0u;
    in.diag_out_val = 0u;
    park_law_step(8u, &in);
    CHECKV(park_law_demand(3u), 0);
    /* id 1 / 4 / other */
    CHECK(park_law_demand(1u) == 0u && park_law_demand(4u) == 0u && park_law_demand(9u) == 0u);
}

/* the arbiter's other rows and modes (pure) */
static void test_arbiter_rows(void)
{
    range_in_t in;
    range_res_t r;

    memset(&in, 0, sizeof in);
    memset(&r, 0, sizeof r);
    in.lever = 5u;
    in.dir = 1u;
    in.d650 = 321u;                             /* row 8: D at speed, plausible direction */
    range_arbitrate(&in, &r);
    CHECK(r.target == 5u && r.reason == 8u && r.flag == 0u);
    in.d650 = 320u;                             /* not above 320 → mode 4 (D → 6 / 0x16) */
    range_arbitrate(&in, &r);
    CHECK(r.target == 6u && r.reason == 0x16u);
    in.fast = 1u;                               /* row 5 */
    in.d650 = 400u;
    range_arbitrate(&in, &r);
    CHECK(r.target == 6u && r.reason == 5u && r.flag == 1u);
    memset(&r, 0, sizeof r);
    in.fast = 0u;
    in.lever = 0u;
    in.dir = 0u;                                /* row 6 */
    range_arbitrate(&in, &r);
    CHECK(r.target == 6u && r.reason == 6u && r.flag == 1u);
    memset(&r, 0, sizeof r);
    in.lever2 = 5u;                             /* rows 2/3: D650 >= 320, D5F8 0, status 1 */
    in.status = 1u;
    in.d650 = 320u;
    in.dir = 1u;
    range_arbitrate(&in, &r);
    CHECK(r.target == 5u && r.reason == 2u);
    in.dir = 2u;
    range_arbitrate(&in, &r);
    CHECK(r.target == 6u && r.reason == 3u);
    in.d5f8 = 1u;                               /* D5F8 blocks rows 2/3; stationary + D5F8 → 8 */
    in.d650 = 0u;
    range_arbitrate(&in, &r);
    CHECK(r.target == 8u && r.reason == 0x15u);
    in.d646 = 81u;                              /* moving: D5F8 does not engage */
    range_arbitrate(&in, &r);
    CHECK(r.target == 6u && r.reason == 0x16u);
}

/* the cross-check agrees with the arbiter everywhere; a forced
 * disagreement / corruption latches and removes both demands */
static void test_monitor(void)
{
    static const uint8_t levers[] = { 0u, 1u, 4u, 5u, 6u, 7u, 8u, 9u };
    static const uint16_t speeds[] = { 0u, 80u, 81u, 3000u, 0xFFFFu };
    static const uint16_t sp2[] = { 0u, 319u, 320u, 321u, 1000u };
    unsigned n = 0u, bad = 0u;

    for (unsigned a = 0u; a < sizeof levers; a++) {
        for (unsigned b = 0u; b < 5u; b++) {
            for (unsigned c = 0u; c < 5u; c++) {
                for (unsigned m = 0u; m < 64u; m++) {
                    range_in_t in;
                    range_res_t r = { 0u, 0u, 0u };
                    park_mon_in_t mi;
                    memset(&in, 0, sizeof in);
                    in.lever = levers[a];
                    in.d646 = speeds[b];
                    in.d650 = sp2[c];
                    in.lever2 = (m & 1u) ? 5u : 0u;
                    in.status = (m & 2u) ? 1u : 0u;
                    in.dir = (uint8_t)((m >> 2) & 3u);
                    in.fast = (m & 16u) ? 1u : 0u;
                    in.d5f8 = (m & 32u) ? 1u : 0u;
                    range_arbitrate(&in, &r);
                    mi.lever = in.lever;
                    mi.lever2 = in.lever2;
                    mi.dir = in.dir;
                    mi.fast = in.fast;
                    mi.status = in.status;
                    mi.out_speed = in.d646;
                    mi.speed2 = in.d650;
                    mi.park_req = in.d5f8;
                    mi.target = r.target;
                    mi.app_integrity_ok = true;
                    bad += park_mon_target(&mi) != r.target ? 1u : 0u;
                    n++;
                }
            }
        }
    }
    CHECKV(bad, 0u);
    printf("  monitor recompute agrees with the arbiter on %u input sets\n", n);

    /* in the loop: R → release; then corrupt the target copy → latched,
     * both demands 0 at once and for good */
    reset_all();
    lever(2u);
    run(10u);
    CHECK(REL() == DRV_PARK_ON_REQ && HOLD() == DRV_PARK_ON_REQ);
    range_test_corrupt();
    park_mon_in_t mi;
    memset(&mi, 0, sizeof mi);
    mi.lever = 7u;
    mi.target = 6u;
    mi.app_integrity_ok = range_integrity_ok();
    park_mon_step(&mi);
    CHECK(!park_mon_enable() && (park_mon_faults() & MON_F_INTEGRITY) != 0u);
    run(5u);
    CHECK(REL() == 0u && HOLD() == 0u);
    lever(1u);
    run(5u);
    lever(2u);
    run(5u);
    CHECK(REL() == 0u && HOLD() == 0u);         /* latched until power-off */

    /* a persistent disagreement (> 2 cycles) latches MON_F_TARGET */
    park_mon_init();
    memset(&mi, 0, sizeof mi);
    mi.lever = 8u;
    mi.target = 6u;                             /* wrong on purpose */
    mi.app_integrity_ok = true;
    park_mon_step(&mi);
    park_mon_step(&mi);
    CHECK(park_mon_enable());                   /* 2 cycles tolerated */
    park_mon_step(&mi);
    CHECK(!park_mon_enable() && (park_mon_faults() & MON_F_TARGET) != 0u);
}

/* D-M4-7: the release on-time limit (accumulated, latched) */
static void test_window(void)
{
    drv_rq_win_t w;

    memset(&w, 0, sizeof w);
    CHECK(drv_rq_window_step(&w, 100u, false, 10000u) == 0u && w.state == 0u);   /* before 'U' */
    CHECK(drv_rq_window_step(&w, 600u, true, 0u) == 0u && w.state == 0u);        /* off */
    CHECK(drv_rq_window_step(&w, 1000u, true, 10000u) == 10000u && w.state == 1u);   /* opens */
    CHECK(drv_rq_window_step(&w, 11000u, true, 10000u) == 10000u);               /* 10 s */
    CHECK(drv_rq_window_step(&w, 11005u, true, 0u) == 0u && w.state == 0u);      /* off: 10.005 s */
    CHECKV(w.acc_ms, 10005u);
    CHECK(drv_rq_window_step(&w, 50000u, true, 0u) == 0u && w.acc_ms == 10005u); /* off time not counted */
    CHECK(drv_rq_window_step(&w, 60000u, true, 10000u) == 10000u);               /* reopens */
    CHECK(drv_rq_window_step(&w, 79990u, true, 10000u) == 10000u);               /* 29.995 s */
    CHECKV(w.acc_ms, 29995u);
    CHECK(drv_rq_window_step(&w, 79995u, true, 10000u) == 0u && w.state == 2u);  /* 30 s: latched */
    CHECK(drv_rq_window_step(&w, 90000u, true, 0u) == 0u && w.state == 2u);
    CHECK(drv_rq_window_step(&w, 99000u, true, 10000u) == 0u && w.state == 2u);  /* stays off */
    /* 'U' lost while on: off (counted up to that pass) */
    memset(&w, 0, sizeof w);
    CHECK(drv_rq_window_step(&w, 0xFFFFF000u, true, 10000u) == 10000u);          /* wrap */
    CHECK(drv_rq_window_step(&w, 0xFFFFF000u + 5000u, false, 10000u) == 0u && w.state == 0u);
    CHECKV(w.acc_ms, 5000u);

    /* the closed loop: lever R held for 40 s, law passes every 5 ms */
    reset_all();
    lever(2u);
    memset(&w, 0, sizeof w);
    {
        uint32_t now = 0u, on_ms = 0u, first_off = 0u;
        for (unsigned t = 0u; t < 4000u; t++) {             /* 10 ms park ticks */
            step();
            for (unsigned p = 0u; p < 2u; p++) {             /* two 5 ms law passes */
                const uint16_t r = drv_rq_window_step(&w, now, true, park_slot7_release());
                if (r != 0u) {
                    on_ms += 5u;
                } else if (first_off == 0u && now > 0u) {
                    first_off = now;
                }
                now += 5u;
            }
        }
        CHECK(park_slot7_release() == DRV_PARK_ON_REQ);    /* the logic still wants it */
        CHECKV(w.state, 2u);
        CHECK(first_off >= 30000u && first_off <= 30010u);
        CHECK(on_ms >= 30000u && on_ms <= 30010u);
        printf("  release on-time limit: off at %u ms after %u ms on\n",
               (unsigned)first_off, (unsigned)on_ms);
    }
}

int main(void)
{
    test_maps();
    test_auto_p();
    test_lever_sequence();
    test_lever_sources();
    test_speed_rule();
    test_lock_interlock();
    test_arbiter_rows();
    test_monitor();
    test_window();
    printf("%s (%d failures)\n", fails ? "FAILED" : "passed", fails);
    return fails ? 1 : 0;
}
