/* Unit test: the park output path from the range (M4, 2026-11-17 —
 * ~/tcu_sweep/range_chain_live_port.md, range_logic_port.md §3):
 *   - app/park.c follows D6D6 (written here directly, standing in for the
 *     chain app/rng_chain.c, which test_rng_chain runs end to end) through
 *     the stock output law 0xB912C / 0xB97F8 → 0xCFFDE → 0xE88FA → 0xE89DE
 *     → 0xE8888 (app/park_law.c) to the two park demands;
 *   - the init arbiter 0xA557C (app/range.c, pure; the chain runs it once);
 *   - safety/park_mon.c's independent checks (ours), each one violated on
 *     purpose (mutations of the chain's output) — latch, both demands 0;
 *   - the request law's D-M4-7 on-time limit (drv_rq_window_step).
 */
#include "app/park.h"
#include "app/park_cells.h"
#include "bsw/com/crc8.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_park.h"
#include "bsw/drv/drv_reqlaw.h"
#include "io/vehicle/vehicle.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); fails++; } } while (0)

/* ---------------------------------------------------------------- the range, written as the chain would */
#define D6D6_W (*(volatile uint8_t *)(uintptr_t)0xFFF8D6D6u)
#define D6D7_W (*(volatile uint8_t *)(uintptr_t)0xFFF8D6D7u)

static void set_range(uint8_t v)
{
    D6D6_W = v;
    D6D7_W = (uint8_t)~v;
}

/* ---------------------------------------------------------------- scenario driver */

static struct {
    uint8_t  lever_code;        /* 1 P, 2 R, 3 N, 4 D (0 no frame) */
    bool     brake;             /* 0x140 pressed */
    uint16_t engine;            /* 0x108 rpm (0 = no frame) */
    uint16_t spd14b;            /* 0x14B raw (0 = no frame) */
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
    }
    vehicle_rx(bus, &f);
}

static void reset_all(void)
{
    memset(&sc, 0, sizeof sc);
    memset(s_ctr, 0, sizeof s_ctr);
    s_bound_violations = 0u;
    CELL_TMP_BYTE = 67u;            /* 27 °C: the dump's 0x89C13 = 0x43 */
    vehicle_init();
    park_init();
    set_range(8u);                  /* the init arbiter's Auto-P */
}

static void step(void)
{
    uint8_t d[8];

    if (sc.lever_code != 0u) {
        const uint8_t l[5] = { sc.lever_code, 0, 0, 0, 0 };
        send(CAN_BUS1, 0x2EAu, 5u, true, l);
        send(CAN_BUS2, 0x210u, 5u, false, l);
    }
    if (sc.brake) {
        memset(d, 0, sizeof d);
        d[0] = 0x04u;
        send(CAN_BUS1, 0x140u, 8u, true, d);
    }
    if (sc.engine != 0u) {
        memset(d, 0, sizeof d);
        d[0] = (uint8_t)(sc.engine >> 8);
        d[1] = (uint8_t)sc.engine;
        d[2] = 0x1Fu;
        d[3] = 0xFFu;
        send(CAN_BUS1, 0x108u, 8u, true, d);
    }
    if (sc.spd14b != 0u) {
        memset(d, 0, sizeof d);
        d[0] = (uint8_t)(sc.spd14b >> 8);
        d[1] = (uint8_t)sc.spd14b;
        send(CAN_BUS1, 0x14Bu, 8u, true, d);
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

/* the stock release preconditions as the bench sims give them */
static void sims_on(uint8_t lever_code)
{
    sc.lever_code = lever_code;
    sc.brake = true;
    sc.engine = 700u;
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

/* park.c follows D6D6: P → off, R / N / D → release + hold ON the same
 * pass (model 2 → 1 in 150 ms), back to P → both off (model 3 → 4 in
 * 200 ms at 27 °C) — the stock law, from the bytes [O 0xB91C6 / 0xB9634 /
 * 0xD0012..0xD003A / 0xE891A..0xE8986 / 0xE8888] */
static void test_follow(void)
{
    static const uint8_t codes[3] = { 2u, 3u, 4u };     /* R N D */

    reset_all();
    sims_on(1u);
    run(30u);
    CHECKV(PCELL_TGT, 8);
    CHECK(PCELL_RNG == 8u && PCELL_RNGD == 8u);
    CHECK(PCELL_AE28 == 0u && PCELL_AE29 == 1u);
    CHECK(park_law_lock() == 1u && park_law_state() == 4u);
    CHECK(REL() == 0u && HOLD() == 0u);
    for (unsigned k = 0u; k < 3u; k++) {
        const uint8_t r = (uint8_t)(9u - codes[k]);      /* R 7, N 6, D 5 */

        sc.lever_code = codes[k];
        step();                                         /* the lever valid this pass */
        set_range(r);                                   /* the chain's answer */
        step();
        CHECKV(PCELL_TGT, r);
        CHECK(PCELL_RNG == r && PCELL_RNGD == r);
        CHECKV(PCELL_AE28, r == 7u ? 2 : r == 6u ? 0 : 1);   /* table 0x63445 */
        CHECKV(PCELL_AE29, 0);
        CHECKV(park_law_lock(), 0);
        CHECK(REL() == DRV_PARK_ON_REQ && HOLD() == DRV_PARK_ON_REQ);
        CHECKV(park_law_state(), 2);                    /* releasing */
        run(15u);
        CHECKV(park_law_state(), 1);                    /* released after 150 ms */
        CHECK(REL() == DRV_PARK_ON_REQ && HOLD() == DRV_PARK_ON_REQ);
        sc.lever_code = 1u;
        set_range(8u);                                  /* back to P */
        step();
        CHECK(REL() == 0u && HOLD() == 0u);
        CHECKV(park_law_state(), 3);                    /* engaging */
        run(19u);
        CHECKV(park_law_state(), 3);
        step();
        CHECKV(park_law_state(), 4);                    /* engaged after 200 ms */
    }
    CHECKV(park_mon_faults(), 0);
    CHECKV(s_bound_violations, 0);
    {
        const park_status_t *st = park_status();
        CHECK(st->target == PARK_ENGAGE && st->lever == 8u && !st->release && !st->hold);
    }
}

/* safety/park_mon.c (ours): each check, violated on purpose by writing a
 * wrong range where the chain would write its own — every violation
 * latches until power-off (park_mon_init) and forces both demands to 0 */
static void mon_case(const char *name, uint8_t lever_code, bool brake, uint16_t engine,
                     uint16_t spd14b, uint8_t range, uint8_t range_c, uint8_t want)
{
    reset_all();
    sc.lever_code = 1u;
    sc.brake = brake;
    sc.engine = engine;
    sc.spd14b = spd14b;
    run(30u);
    CHECK(park_mon_enable());
    sc.lever_code = lever_code;
    step();
    D6D6_W = range;
    D6D7_W = range_c;
    step();
    if (park_mon_faults() != want) {
        printf("FAIL monitor case %s: faults 0x%02X, expected 0x%02X\n", name, park_mon_faults(), want);
        fails++;
    }
    if (want != 0u) {
        CHECK(!park_mon_enable() && REL() == 0u && HOLD() == 0u);
        /* latched: a valid P and a valid release later change nothing */
        sims_on(1u);
        set_range(8u);
        run(5u);
        sc.lever_code = 2u;
        step();
        set_range(7u);
        run(5u);
        CHECK(!park_mon_enable() && REL() == 0u && HOLD() == 0u);
        CHECKV(park_mon_faults() & want, want);
    } else {
        CHECK(park_mon_enable() && REL() == DRV_PARK_ON_REQ && HOLD() == DRV_PARK_ON_REQ);
    }
}

static void test_monitor(void)
{
    /* the legal release: lever R valid, brake pressed, engine 700 rpm */
    mon_case("legal R", 2u, true, 700u, 0u, 7u, 0xF8u, 0u);
    mon_case("legal D", 4u, true, 700u, 0u, 5u, 0xFAu, 0u);
    /* brake invalid but 0x14B speed raw/5 > 10 (the D607 fallback) */
    mon_case("brake via 0x14B", 2u, false, 700u, 55u, 7u, 0xF8u, 0u);
    mon_case("0x14B at the limit", 2u, false, 700u, 54u, 7u, 0xF8u, MON_F_BRAKE);
    /* mutations of the chain's output */
    mon_case("no lever frame", 0u, true, 700u, 0u, 7u, 0xF8u, MON_F_LEVER);
    mon_case("lever P, range R", 1u, true, 700u, 0u, 7u, 0xF8u, MON_F_LEVER);
    mon_case("lever R, range D", 2u, true, 700u, 0u, 5u, 0xFAu, MON_F_LEVER);
    mon_case("lever invalid code 7", 7u, true, 700u, 0u, 7u, 0xF8u, MON_F_LEVER);
    mon_case("no brake", 2u, false, 700u, 0u, 7u, 0xF8u, MON_F_BRAKE);
    mon_case("no engine", 2u, true, 0u, 0u, 7u, 0xF8u, MON_F_ENGINE);
    mon_case("engine 430 (not above)", 2u, true, 430u, 0u, 7u, 0xF8u, MON_F_ENGINE);
    mon_case("nothing at all", 0u, false, 0u, 0u, 6u, 0xF9u, MON_F_LEVER | MON_F_BRAKE | MON_F_ENGINE);
    mon_case("range 3", 2u, true, 700u, 0u, 3u, 0xFCu, MON_F_RANGE);
    mon_case("range 9", 2u, true, 700u, 0u, 9u, 0xF6u, MON_F_RANGE);
    mon_case("complement torn (in P)", 1u, true, 700u, 0u, 8u, 0xF6u, MON_F_RANGE);
    mon_case("complement torn (R)", 2u, true, 700u, 0u, 7u, 0xF7u, MON_F_RANGE);

    /* the engine hysteresis: on above 430, off after 25 passes <= 380 */
    reset_all();
    sc.lever_code = 1u;
    sc.engine = 431u;
    step();
    CHECK(park_mon_engine_running());
    sc.engine = 381u;                   /* between the limits: holds */
    run(100u);
    CHECK(park_mon_engine_running());
    sc.engine = 380u;
    run(24u);
    CHECK(park_mon_engine_running());
    step();
    CHECK(!park_mon_engine_running());  /* the 25th pass at 380 */
    sc.engine = 400u;                   /* above 380, below 430: stays off */
    run(10u);
    CHECK(!park_mon_engine_running());
    sc.engine = 700u;
    step();
    CHECK(park_mon_engine_running());
    sc.engine = 0u;                     /* the frame stops: 50 ms timeout, then invalid = 0 rpm */
    run(20u);
    CHECK(park_mon_engine_running());   /* < 25 passes since the timeout */
    run(20u);
    CHECK(!park_mon_engine_running());

    /* moves between R / N / D (no P in between) are not checked, and a
     * return to P is always allowed */
    reset_all();
    sims_on(2u);
    run(30u);
    set_range(7u);
    step();
    sc.lever_code = 3u;
    set_range(6u);
    step();
    sc.brake = false;
    sc.engine = 0u;
    sc.lever_code = 4u;
    set_range(5u);
    run(30u);
    set_range(8u);
    run(5u);
    CHECKV(park_mon_faults(), 0);

    /* the park law's own output pairs corrupted → MON_F_INTEGRITY */
    reset_all();
    sims_on(1u);
    run(5u);
    *(volatile uint8_t *)(uintptr_t)0xFFF851B1u ^= 0x01u;   /* complement of output flag 4 (not rewritten by the law) */
    park_tick_10ms();
    CHECK((park_mon_faults() & MON_F_INTEGRITY) != 0u && REL() == 0u && HOLD() == 0u);
}

/* D-M4-7: the request law's on-time limit with the release ON for 40 s */
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

    /* the closed loop: lever R (and the sims) held for 40 s, law passes every 5 ms */
    reset_all();
    sims_on(2u);
    step();
    set_range(7u);
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

int main(void)
{
    test_maps();
    test_follow();
    test_lock_interlock();
    test_arbiter_rows();
    test_monitor();
    test_window();
    printf("%s (%d failures)\n", fails ? "FAILED" : "passed", fails);
    return fails ? 1 : 0;
}
