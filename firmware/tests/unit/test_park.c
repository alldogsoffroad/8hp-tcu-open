/* Unit and closed-loop test: range / park control (milestone M4a,
 * docs/firmware/01-m4-range-park.md sections 3-6).
 *
 * The loop: CAN frames (0x108 engine speed, 0x2EA / 0x210 lever, real CRC and counter) ->
 * FCA profile -> park_tick_10ms() -> park mechanism model -> park contacts. sensors_in() is
 * provided here (speeds, supply and contacts come from the scenario and the model).
 */
#include "app/park.h"
#include "bsw/com/crc8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

/* ---------------------------------------------------------------- sensors stand-in */

static sensors_in_t s_z;

const sensors_in_t *sensors_in(void)
{
    return &s_z;
}

/* ---------------------------------------------------------------- park mechanism model */

#define PRESSURE_RPM 300        /* line pressure for the release piston (model)          */
#define RATCHET_RPM  200        /* above this output speed the pawl ratchets, no engage  */

static struct {
    int  pawl;                  /* 0 = engaged (in P) ... 100 = fully released           */
    bool latched;               /* hold magnet holds the release piston                  */
} m;

static void model_step(const park_out_t *o, rpm_t engine, rpm_t out_speed)
{
    if (!o->hold) {
        m.latched = false;
    }
    if (o->hold && m.pawl == 100) {
        m.latched = true;
    }
    if (m.latched) {
        m.pawl = 100;
    } else if (o->release && engine >= PRESSURE_RPM) {
        m.pawl = m.pawl + 25 > 100 ? 100 : m.pawl + 25;
    } else if (abs(out_speed) <= RATCHET_RPM) {
        m.pawl = m.pawl - 25 < 0 ? 0 : m.pawl - 25;
    } else {
        m.pawl = m.pawl - 25 < 50 ? 50 : m.pawl - 25;
    }
}

/* ---------------------------------------------------------------- scenario driver */

static struct {
    uint8_t lever_code;         /* 1 P, 2 R, 3 N, 4 D */
    bool    send_2ea, send_210, bad_crc;
    rpm_t   engine, out_speed;
    mv_t    battery;
    int     stuck;              /* -1 = contacts follow the model, else park_pos_t */
} sc;

static uint8_t s_ctr[0x800];
static bool s_prev_release;
static unsigned s_violations;

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

static void reset_all(void)
{
    memset(&sc, 0, sizeof sc);
    sc.lever_code = 1u;
    sc.send_2ea = sc.send_210 = true;
    sc.battery = 13500u;
    sc.stuck = -1;
    m.pawl = 0;
    m.latched = false;
    s_prev_release = false;
    s_violations = 0u;
    vehicle_init();
    park_init();
}

static void step(void)
{
    const uint8_t e[8] = { (uint8_t)((uint16_t)sc.engine >> 8), (uint8_t)sc.engine, 0x09, 0x60, 0, 0, 0, 0 };
    const uint8_t l[5] = { sc.lever_code, 0, 0, 0, 0 };
    const park_status_t *ps = park_status();
    const bool still = abs(sc.out_speed) <= RANGE_STATIONARY_RPM;
    const int pawl_before = m.pawl;

    send(CAN_BUS1, 0x108u, 8u, true, e);
    if (sc.send_2ea) {
        send(CAN_BUS1, 0x2EAu, 5u, true, l);
    }
    if (sc.send_210) {
        send(CAN_BUS2, 0x210u, 5u, false, l);
    }
    vehicle_update_10ms();

    s_z.output_speed = sc.out_speed;
    s_z.q_output_speed = Q_OK;
    s_z.battery = sc.battery;
    s_z.q_battery = Q_OK;
    s_z.park = sc.stuck >= 0 ? (park_pos_t)sc.stuck
             : m.pawl == 0 ? PARK_IN_P : m.pawl == 100 ? PARK_NOT_P : PARK_UNKNOWN;
    s_z.q_park = Q_OK;

    park_tick_10ms();

    /* pass criteria (section 6): no release out of P unless stationary with the engine running;
     * the pawl never starts to drop while moving except in SAFE */
    if (ps->req.release && !ps->req.hold && !s_prev_release && (!still || sc.engine < 400)) {
        s_violations++;
    }
    s_prev_release = ps->req.release;
    model_step(&ps->req, sc.engine, sc.out_speed);
    if (pawl_before == 100 && m.pawl < 100 && !still && ps->state != PC_SAFE) {
        s_violations++;
    }
}

static void run(unsigned n)
{
    for (unsigned i = 0u; i < n; i++) {
        step();
    }
}

/* ---------------------------------------------------------------- tests */

static void test_decide(void)
{
    static const range_req_t levers[5] = { RANGE_NONE, RANGE_P, RANGE_R, RANGE_N, RANGE_D };
    static const rpm_t speeds[5] = { 0, 80, -80, 81, 1500 };
    range_reason_t why;

    for (int l = 0; l < 5; l++) {
        for (int ok = 0; ok < 2; ok++) {
            for (int s = 0; s < 5; s++) {
                for (int sok = 0; sok < 2; sok++) {
                    for (int pr = 0; pr < 2; pr++) {
                        const range_in_t in = { levers[l], ok != 0, speeds[s], sok != 0, pr != 0 };
                        const bool still = sok && speeds[s] <= 80 && speeds[s] >= -80;
                        const bool lever_valid = ok && levers[l] != RANGE_NONE;
                        const park_target_t t = range_decide(&in, &why);

                        if (lever_valid && levers[l] == RANGE_P) {
                            CHECK(t == PARK_ENGAGE && why == R_LEVER_P);
                        } else if (!still) {
                            CHECK(t == PARK_RELEASE && why == R_MOVING);
                        } else if (pr) {
                            CHECK(t == PARK_ENGAGE && why == R_PARK_REQ);
                        } else if (lever_valid) {
                            CHECK(t == PARK_RELEASE && why == R_LEVER_RND);
                        } else {
                            CHECK(t == PARK_ENGAGE && why == R_AUTO_P);
                        }
                    }
                }
            }
        }
    }
    range_init();
    CHECK(range_target() == PARK_ENGAGE && range_integrity_ok());
}

/* P -> R -> N -> D -> P at standstill with the engine running. */
static void test_standstill_cycle(void)
{
    const park_status_t *ps = park_status();

    reset_all();
    run(5u);
    CHECK(ps->state == PC_IN_P && !ps->req.release && !ps->req.hold);

    sc.lever_code = 2u;                 /* R, engine off: no release */
    run(20u);
    CHECK(ps->target == PARK_RELEASE && ps->state == PC_IN_P && m.pawl == 0);

    sc.engine = 800;                    /* engine starts: release, then hold */
    run(20u);
    CHECK(ps->state == PC_HOLDING && ps->req.hold && ps->req.release && m.pawl == 100);
    sc.lever_code = 3u;
    run(10u);
    sc.lever_code = 4u;
    run(10u);
    CHECK(ps->state == PC_HOLDING && ps->reason == R_LEVER_RND);

    sc.lever_code = 1u;                 /* P: engage */
    run(20u);
    CHECK(ps->state == PC_IN_P && !ps->req.hold && !ps->req.release && m.pawl == 0);
    CHECK(ps->ctl_faults == 0u && ps->mon_faults == 0u && s_violations == 0u);
}

/* N with the engine stopped: the hold stays on (car wash); P then engages. */
static void test_car_wash(void)
{
    const park_status_t *ps = park_status();

    reset_all();
    sc.engine = 800;
    sc.lever_code = 3u;
    run(30u);
    CHECK(ps->state == PC_HOLDING);
    sc.engine = 0;
    run(100u);
    CHECK(ps->state == PC_HOLDING && ps->req.hold && m.pawl == 100);
    sc.lever_code = 1u;
    run(20u);
    CHECK(ps->state == PC_IN_P && m.pawl == 0);
    CHECK(ps->mon_faults == 0u && s_violations == 0u);
}

/* P selected at speed: the hold stays on until below 80 rpm, then Park engages. */
static void test_p_at_speed(void)
{
    const park_status_t *ps = park_status();

    reset_all();
    sc.engine = 800;
    sc.lever_code = 4u;
    run(30u);
    sc.out_speed = 1500;
    run(10u);
    sc.lever_code = 1u;
    run(100u);
    CHECK(ps->target == PARK_ENGAGE && ps->state == PC_HOLDING && ps->req.hold && m.pawl == 100);
    sc.out_speed = 81;
    run(10u);
    CHECK(ps->state == PC_HOLDING);
    sc.out_speed = 60;
    run(20u);
    CHECK(ps->state == PC_IN_P && m.pawl == 0);
    CHECK(ps->mon_faults == 0u && s_violations == 0u);
}

/* Lever lost while moving: released (moving); once stopped, Auto P. */
static void test_lever_lost_moving(void)
{
    const park_status_t *ps = park_status();

    reset_all();
    sc.engine = 800;
    sc.lever_code = 4u;
    run(30u);
    sc.out_speed = 1500;
    run(10u);
    sc.send_2ea = sc.send_210 = false;
    run(100u);
    CHECK(vehicle_in()->q_lever == Q_INVALID);
    CHECK(ps->target == PARK_RELEASE && ps->reason == R_MOVING && ps->req.hold);
    sc.out_speed = 0;
    run(20u);
    CHECK(ps->reason == R_AUTO_P && ps->state == PC_IN_P && m.pawl == 0);
    CHECK(ps->mon_faults == 0u && s_violations == 0u);
}

/* Lever lost while stationary in D: Auto P after the message timeout. */
static void test_lever_lost_still(void)
{
    const park_status_t *ps = park_status();

    reset_all();
    sc.engine = 800;
    sc.lever_code = 4u;
    run(30u);
    sc.send_2ea = sc.send_210 = false;
    run(30u);
    CHECK(ps->reason == R_AUTO_P && ps->state == PC_IN_P);
    CHECK(ps->mon_faults == 0u && s_violations == 0u);
}

/* Bus 1 lost or 0x2EA corrupt: the shifter bus copy (0x210) is used. */
static void test_fallback(void)
{
    const park_status_t *ps = park_status();

    reset_all();
    sc.engine = 800;
    sc.lever_code = 2u;
    sc.send_2ea = false;
    run(30u);
    CHECK(vehicle_in()->lever_src == LEVER_SRC_FALLBACK && ps->state == PC_HOLDING);

    sc.send_2ea = true;
    sc.bad_crc = true;
    run(20u);
    CHECK(vehicle_in()->lever_src == LEVER_SRC_FALLBACK && ps->state == PC_HOLDING);

    sc.send_210 = false;                /* both sources unusable -> Auto P */
    run(30u);
    CHECK(vehicle_in()->q_lever == Q_INVALID && ps->state == PC_IN_P);
    CHECK(ps->mon_faults == 0u && s_violations == 0u);
}

/* In P with a moving (or not valid) output speed: no release until stationary. */
static void test_no_release_moving(void)
{
    const park_status_t *ps = park_status();

    reset_all();
    sc.engine = 800;
    sc.out_speed = 300;                 /* e.g. a speed signal glitch while in P */
    sc.lever_code = 4u;
    run(30u);
    CHECK(ps->target == PARK_RELEASE && ps->state == PC_IN_P && !ps->req.release);
    sc.out_speed = 0;
    run(20u);
    CHECK(ps->state == PC_HOLDING && ps->mon_faults == 0u && s_violations == 0u);
}

/* Low supply: no release. */
static void test_supply_low(void)
{
    const park_status_t *ps = park_status();

    reset_all();
    sc.engine = 800;
    sc.battery = 8500u;
    sc.lever_code = 4u;
    run(30u);
    CHECK(ps->target == PARK_RELEASE && ps->state == PC_IN_P && !ps->req.release);
    sc.battery = 12000u;
    run(20u);
    CHECK(ps->state == PC_HOLDING);
}

/* Contacts stuck in P: release times out, no retry until P is selected again. */
static void test_contacts_stuck_in_p(void)
{
    const park_status_t *ps = park_status();
    unsigned releasing = 0u;

    reset_all();
    sc.stuck = PARK_IN_P;
    sc.engine = 800;
    sc.lever_code = 4u;
    for (unsigned i = 0u; i < 200u; i++) {
        step();
        releasing += ps->req.release ? 1u : 0u;
    }
    CHECK((ps->ctl_faults & PC_F_RELEASE_TIMEOUT) != 0u && ps->state == PC_IN_P);
    CHECK(releasing >= 45u && releasing <= 51u);        /* one 500 ms attempt only */
    sc.lever_code = 1u;
    run(5u);
    sc.lever_code = 4u;
    run(5u);
    CHECK(ps->state == PC_RELEASING);                    /* P selected again: retry allowed */
}

/* Contacts say "not in P" while the sequence is in P: monitor fault -> SAFE, latched. */
static void test_contacts_disagree(void)
{
    const park_status_t *ps = park_status();

    reset_all();
    run(5u);
    CHECK(ps->state == PC_IN_P);
    sc.stuck = PARK_NOT_P;
    run(15u);
    CHECK(ps->mon_faults == 0u);
    run(10u);
    CHECK(ps->mon_faults == MON_F_CONTACTS && ps->state == PC_SAFE);
    sc.stuck = -1;
    sc.engine = 800;
    sc.lever_code = 4u;
    run(50u);
    CHECK(ps->state == PC_SAFE && !ps->req.release && !ps->req.hold);
}

/* Engage times out (contacts never reach P): SAFE. */
static void test_engage_timeout(void)
{
    const park_status_t *ps = park_status();

    reset_all();
    sc.engine = 800;
    sc.lever_code = 4u;
    run(30u);
    CHECK(ps->state == PC_HOLDING);
    sc.stuck = PARK_UNKNOWN;
    sc.lever_code = 1u;
    run(110u);
    CHECK((ps->ctl_faults & PC_F_ENGAGE_TIMEOUT) != 0u && ps->state == PC_SAFE);
}

/* Reset while moving with the pawl out: the hold is re-latched without a monitor fault. */
static void test_reset_moving(void)
{
    const park_status_t *ps = park_status();

    reset_all();
    m.pawl = 100;
    sc.engine = 1500;
    sc.out_speed = 2000;
    sc.lever_code = 4u;
    run(10u);
    CHECK(ps->state == PC_HOLDING && ps->req.hold && m.pawl == 100);
    CHECK(ps->mon_faults == 0u && s_violations == 0u);
}

/* Monitor on its own: target mismatch, release / hold-drop conditions, integrity. */
static park_mon_in_t mon_in(void)
{
    park_mon_in_t mi;

    memset(&mi, 0, sizeof mi);
    mi.lever = RANGE_P;
    mi.lever_ok = mi.speed_ok = mi.engine_ok = mi.contacts_ok = mi.app_integrity_ok = true;
    mi.contacts = PARK_IN_P;
    mi.target = PARK_ENGAGE;
    mi.state = PC_IN_P;
    return mi;
}

static void test_monitor(void)
{
    park_mon_in_t mi;

    park_mon_init();                    /* wrong target: tolerated 2 cycles, fault on the 3rd */
    mi = mon_in();
    mi.target = PARK_RELEASE;
    park_mon_step(&mi);
    park_mon_step(&mi);
    CHECK(park_mon_enable());
    park_mon_step(&mi);
    CHECK(!park_mon_enable() && park_mon_faults() == MON_F_TARGET);

    park_mon_init();                    /* release out of P with the engine off */
    mi = mon_in();
    mi.lever = RANGE_D;
    mi.target = PARK_RELEASE;
    mi.out.release = true;
    park_mon_step(&mi);
    CHECK(park_mon_faults() == MON_F_RELEASE);

    park_mon_init();                    /* release out of P while moving */
    mi = mon_in();
    mi.lever = RANGE_D;
    mi.target = PARK_RELEASE;
    mi.engine_speed = 800;
    mi.output_speed = 300;
    mi.out.release = true;
    park_mon_step(&mi);
    CHECK(park_mon_faults() == MON_F_RELEASE);

    park_mon_init();                    /* hold dropped while moving */
    mi = mon_in();
    mi.lever = RANGE_D;
    mi.target = PARK_RELEASE;
    mi.state = PC_HOLDING;
    mi.contacts = PARK_NOT_P;
    mi.output_speed = 1500;
    mi.out.hold = true;
    park_mon_step(&mi);
    CHECK(park_mon_enable());
    mi.out.hold = false;
    park_mon_step(&mi);
    CHECK(park_mon_faults() == MON_F_HOLD_DROP);

    park_mon_init();                    /* corrupt application copy */
    mi = mon_in();
    mi.app_integrity_ok = false;
    park_mon_step(&mi);
    CHECK(park_mon_faults() == MON_F_INTEGRITY);

    park_mon_init();                    /* speed not valid counts as moving: P stays P */
    mi = mon_in();
    mi.speed_ok = false;
    park_mon_step(&mi);
    mi.lever = RANGE_NONE;
    mi.lever_ok = false;
    mi.target = PARK_RELEASE;
    park_mon_step(&mi);
    park_mon_step(&mi);
    park_mon_step(&mi);
    CHECK(park_mon_enable());
}

/* Inverted copies: a corrupt copy reads as the safe value and is reported. */
static void test_integrity(void)
{
    reset_all();
    run(5u);
    range_test_corrupt();
    CHECK(!range_integrity_ok() && range_target() == PARK_ENGAGE);
    park_ctl_test_corrupt();
    CHECK(!park_ctl_integrity_ok() && park_ctl_state() == PC_SAFE);
    run(1u);                            /* park_ctl repairs to SAFE; range copy rewritten */
    CHECK(park_ctl_state() == PC_SAFE && (park_ctl_faults() & PC_F_INTEGRITY) != 0u);
}

int main(void)
{
    test_decide();
    test_standstill_cycle();
    test_car_wash();
    test_p_at_speed();
    test_lever_lost_moving();
    test_lever_lost_still();
    test_fallback();
    test_no_release_moving();
    test_supply_low();
    test_contacts_stuck_in_p();
    test_contacts_disagree();
    test_engage_timeout();
    test_reset_moving();
    test_monitor();
    test_integrity();
    printf("%s (%d failures)\n", fails ? "FAILED" : "passed", fails);
    return fails ? 1 : 0;
}
