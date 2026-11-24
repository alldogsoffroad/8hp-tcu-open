/* Unit test: THE NISSAN VEHICLE PROFILE SKELETON (io/vehicle/profile_nissan.c, nissan_tx.c;
 * 2026-11-20, ~/tcu_sweep/nissan_profile_skeleton.md).
 *
 * Frames are built from the DBC layouts (nissan_frontier_2015_candidate.dbc, CONFIRMED signals)
 * and decoded into vehicle_in_t / nissan_in_t; timeouts; "not available" for every input the
 * DBC cannot give; the transmit stub sends nothing; and, end to end, the stock range chain
 * (app/rng_chain.c) on the Nissan profile with NO lever source holds P - even with the brake
 * on and the engine running - and confirms U1267, the lost-shifter fault (decision D-N1).
 */
#include "io/vehicle/vehicle.h"
#include "io/vehicle/profile_nissan.h"
#include "io/vehicle/vehicle_tx.h"
#include "io/vehicle/nissan_out.h"
#include "app/rng_chain.h"
#include "safety/kl15.h"
#include "app/park.h"
#include "app/park_cells.h"
#include "bsw/drv/drv_park.h"
#include "bsw/drv/drv_cells.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECKV(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("FAIL %s:%d %s = %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); fails++; } } while (0)

static void send(uint16_t id, uint8_t dlc, const uint8_t *d)
{
    can_frame_t f = { .id = id, .dlc = dlc };

    memcpy(f.data, d, dlc);
    vehicle_rx(CAN_BUS1, &f);
}

static void ticks(unsigned n)
{
    while (n-- != 0u) {
        vehicle_update_10ms();
    }
}

/* DBC layouts */
static void send_rpm(uint16_t raw1f9, uint16_t raw180)
{
    uint8_t a[8] = { 0, 0, (uint8_t)(raw1f9 >> 8), (uint8_t)raw1f9, 0, 0, 0, 0 };    /* 23|16@0 */
    uint8_t b[8] = { (uint8_t)(raw180 >> 8), (uint8_t)raw180, 0, 0, 0, 0, 0, 0 };    /* 7|16@0  */

    send(0x1F9u, 8u, a);
    send(0x180u, 8u, b);
}

static void send_brake(bool sw, bool light)
{
    uint8_t a[8] = { 0, 0, 0, 0, (uint8_t)(sw ? 0x10u : 0u), 0, 0, 0 };              /* 36|1@1 */
    uint8_t b[8] = { 0, 0, 0, 0, 0, 0, (uint8_t)(light ? 0x14u : 0x04u), 0 };        /* 52|1@0 */

    send(0x35Du, 8u, a);
    send(0x354u, 8u, b);
}

static void test_defaults(void)
{
    const vehicle_in_t *v;
    const nissan_in_t *x;

    vehicle_init();
    v = vehicle_in();
    x = nissan_in();
    CHECK(v->q_engine_speed == Q_INVALID && v->q_torque_actual == Q_INVALID &&
          v->q_torque_t2 == Q_INVALID && v->q_torque_t3 == Q_INVALID && v->q_pedal == Q_INVALID &&
          v->q_engine_temp == Q_INVALID && v->q_brake == Q_INVALID && v->q_engine_mode == Q_INVALID &&
          v->q_park_speed == Q_INVALID && v->q_soak == Q_INVALID && v->q_ign_code == Q_INVALID &&
          v->q_lever == Q_INVALID);
    CHECK(v->brake == BRAKE_NA && v->lever == RANGE_NONE && v->lever_src == LEVER_SRC_NONE && !v->lever_flag);
    CHECK(x->q_speed == Q_INVALID && x->q_wheels == Q_INVALID && x->q_torque_demand == Q_INVALID);
    CHECKV(vehicle_quality_bits(v), 0);
    vehicle_update_10ms();
    CHECKV(vehicle_quality_bits(v), 0);              /* no frame: nothing OK */
}

static void test_engine(void)
{
    const vehicle_in_t *v = vehicle_in();

    vehicle_init();
    /* idle: raw 5650 = 706.25 rpm (the DBC's note); big-endian - a little-endian read would be 0x1216 */
    send_rpm(5650u, 5650u);
    vehicle_update_10ms();
    CHECKV(v->engine_speed, 706);
    CHECK(v->q_engine_speed == Q_OK);
    send_rpm(0xFFFFu, 0xFFFFu);                      /* 8191.875 rpm: the signal maximum */
    vehicle_update_10ms();
    CHECKV(v->engine_speed, 8192);
    /* primary only / mirror only */
    vehicle_init();
    {
        uint8_t a[8] = { 0, 0, 0x16, 0x12, 0, 0, 0, 0 };

        send(0x1F9u, 8u, a);
    }
    vehicle_update_10ms();
    CHECKV(v->engine_speed, 706);
    CHECK(v->q_engine_speed == Q_OK);
    vehicle_init();
    {
        uint8_t b[8] = { 0x16, 0x12, 0, 0, 0, 0, 0, 0 };

        send(0x180u, 8u, b);
    }
    vehicle_update_10ms();
    CHECKV(v->engine_speed, 706);
    CHECK(v->q_engine_speed == Q_OK);
    /* mirror disagreement: 800 raw (100 rpm) is the window; one more is invalid */
    vehicle_init();
    send_rpm(5650u, 5650u + 800u);
    vehicle_update_10ms();
    CHECK(v->q_engine_speed == Q_OK);
    vehicle_init();
    send_rpm(5650u, 5650u + 801u);
    vehicle_update_10ms();
    CHECK(v->q_engine_speed == Q_INVALID && v->engine_speed == 0);
    /* timeout 50 ms (5 x the logged 10 ms period): fresh at 50 ms, invalid after */
    vehicle_init();
    send_rpm(5650u, 5650u);
    ticks(5u);
    CHECK(v->q_engine_speed == Q_OK);
    ticks(1u);
    CHECK(v->q_engine_speed == Q_INVALID && v->engine_speed == 0);
    /* a short frame (DLC < 8) is rejected */
    vehicle_init();
    {
        uint8_t a[8] = { 0, 0, 0x16, 0x12, 0, 0, 0, 0 };

        send(0x1F9u, 7u, a);
    }
    vehicle_update_10ms();
    CHECK(v->q_engine_speed == Q_INVALID);
}

static void test_brake(void)
{
    const vehicle_in_t *v = vehicle_in();

    vehicle_init();
    send_brake(false, false);
    vehicle_update_10ms();
    CHECK(v->brake == BRAKE_OFF && v->q_brake == Q_OK);
    send_brake(true, true);
    vehicle_update_10ms();
    CHECK(v->brake == BRAKE_ON && v->q_brake == Q_OK);
    /* the two bits disagree: never ON, flagged */
    send_brake(true, false);
    vehicle_update_10ms();
    CHECK(v->brake == BRAKE_ERROR && v->q_brake == Q_INVALID);
    send_brake(false, true);
    vehicle_update_10ms();
    CHECK(v->brake == BRAKE_ERROR && v->q_brake == Q_INVALID);
    /* the light's other bits (0x04 baseline) do not read as brake; the switch's byte 4 other bits neither */
    {
        uint8_t a[8] = { 0, 0, 0, 0, 0xEFu, 0, 0, 0 };
        uint8_t b[8] = { 0, 0, 0, 0, 0, 0, 0xEFu, 0 };

        send(0x35Du, 8u, a);
        send(0x354u, 8u, b);
        vehicle_update_10ms();
        CHECK(v->brake == BRAKE_OFF);
    }
    /* only one source alive: not trusted */
    vehicle_init();
    {
        uint8_t a[8] = { 0, 0, 0, 0, 0x10, 0, 0, 0 };

        send(0x35Du, 8u, a);
    }
    vehicle_update_10ms();
    CHECK(v->brake == BRAKE_NA && v->q_brake == Q_INVALID);
    /* timeout of one source after both were fresh */
    vehicle_init();
    send_brake(true, true);
    ticks(5u);
    CHECK(v->brake == BRAKE_ON);
    {
        uint8_t a[8] = { 0, 0, 0, 0, 0x10, 0, 0, 0 };

        for (unsigned i = 0u; i < 21u; i++) {
            send(0x35Du, 8u, a);                     /* the switch keeps coming, the light stops */
            vehicle_update_10ms();
        }
    }
    CHECK(v->brake == BRAKE_NA && v->q_brake == Q_INVALID);
}

static void test_coolant_and_extras(void)
{
    const vehicle_in_t *v = vehicle_in();
    const nissan_in_t *x = nissan_in();

    vehicle_init();
    {
        uint8_t t[8] = { 90, 0, 0, 0, 0, 0, 0, 0 };         /* 0x551 byte 0: 90 - 45 = 45 degC */
        uint8_t s[8] = { 0, 0, 0, 0, 0x04, 0x43, 0, 0 };    /* 0x280 4-5 BE: 1091 = 10.91 mph */
        uint8_t w1[8] = { 0x07, 0x42, 0x07, 0x4C, 0, 0, 0, 0 };   /* 1858 / 1868 */
        uint8_t w2[8] = { 0x07, 0x4D, 0x07, 0x5E, 0, 0, 0, 0 };   /* 1869 / 1886 */
        uint8_t th[7] = { 0, 0, 0, 128, 8, 127, 0xC0 };     /* 0x160: pedal 512, complement 511 */

        send(0x551u, 8u, t);
        send(0x280u, 8u, s);
        send(0x284u, 8u, w1);
        send(0x160u, 7u, th);
        vehicle_update_10ms();
        CHECKV(v->engine_temp, 450);
        CHECK(v->q_engine_temp == Q_OK);
        CHECKV(x->speed_mph100, 1091);
        CHECK(x->q_speed == Q_OK);
        CHECK(x->q_wheels == Q_INVALID);                    /* 0x285 not yet seen */
        CHECKV(v->pedal, 502);                              /* 512 raw x 0.98 = 50.2 % */
        CHECK(v->q_pedal == Q_OK);
        send(0x285u, 8u, w2);
        vehicle_update_10ms();
        CHECK(x->q_wheels == Q_OK);
        CHECKV(x->wheel_kmh100[0], 1858);
        CHECKV(x->wheel_kmh100[1], 1868);
        CHECKV(x->wheel_kmh100[2], 1869);
        CHECKV(x->wheel_kmh100[3], 1886);
        th[3] = 255u;
        th[5] = 3u;                                         /* the complement disagrees */
        send(0x160u, 7u, th);
        vehicle_update_10ms();
        CHECK(v->q_pedal == Q_INVALID && v->pedal == 0);
        t[0] = 0u;                                          /* raw 0 = -45 degC */
        send(0x551u, 8u, t);
        vehicle_update_10ms();
        CHECKV(v->engine_temp, -450);
        send(0x160u, 6u, th);                               /* DLC 6 < 7: rejected, the old data ages */
        ticks(51u);                                         /* past the slowest timeout (0x551, 500 ms) */
        CHECK(v->q_pedal == Q_INVALID && x->q_speed == Q_INVALID && x->q_wheels == Q_INVALID);
        CHECK(v->q_engine_temp == Q_INVALID);
    }
}

/* everything the DBC cannot give stays "not available" while the confirmed signals run */
static void test_not_available(void)
{
    const vehicle_in_t *v = vehicle_in();

    vehicle_init();
    for (unsigned i = 0u; i < 5u; i++) {
        uint8_t t[8] = { 90, 0, 0, 0, 0, 0, 0, 0 };
        uint8_t th[7] = { 0, 0, 0, 200, 200, 55, 0 };       /* pedal 803 raw, complement 220: valid */
        uint8_t e[8]  = { 0, 0, 0, 0, 0xFF, 0, 0, 0 };      /* 0x182 byte 4, the torque candidate: not even received */

        send_rpm(5650u, 5650u);
        send_brake(false, false);
        send(0x551u, 8u, t);
        send(0x160u, 7u, th);
        send(0x182u, 8u, e);
        send(0x421u, 3u, (const uint8_t[3]){ 0x20, 0, 0 });  /* the TCM's own gear frame: never an input */
        vehicle_update_10ms();
    }
    CHECK(v->q_engine_speed == Q_OK && v->q_brake == Q_OK && v->q_engine_temp == Q_OK);
    CHECK(v->q_torque_actual == Q_INVALID && v->torque_actual == 0);   /* 0x180 counter frozen at 0 */
    CHECK(v->q_torque_t2 == Q_INVALID && v->q_torque_t3 == Q_INVALID);
    CHECK(v->q_pedal == Q_OK && v->pedal == 787);         /* 803 x 0.98 permil (2026-11-23) */
    CHECK(v->q_engine_mode == Q_INVALID && v->q_soak == Q_INVALID);
    CHECK(v->q_park_speed == Q_INVALID && v->park_speed_raw == 0u);
    CHECK(v->q_ign_code == Q_INVALID && v->ign_code == 0u);
    CHECK(v->q_lever == Q_INVALID && v->lever == RANGE_NONE && v->lever_src == LEVER_SRC_NONE);
    /* quality bits: engine speed (0), pedal (4), engine temp (5), brake (6) */
    CHECKV(vehicle_quality_bits(v), (1u << 0) | (1u << 4) | (1u << 5) | (1u << 6));
}

/* 0x180 torques (actual -> T1, demand -> nissan_in_t) gated by the B6[3:0] counter; 0x160 pedal
 * with its complement (2026-11-23, truck logs) */
static void send180(uint16_t ta, uint16_t td, uint8_t ctr)
{
    uint8_t b[8] = { 0x16, 0x12, (uint8_t)(ta >> 4), (uint8_t)((ta & 15u) << 4 | (td >> 8)), (uint8_t)td,
                     0, ctr, 0 };

    send(0x180u, 8u, b);
}

static void send160(uint16_t v, uint16_t c)
{
    uint8_t d[7] = { 0, 0, (uint8_t)(v >> 2), (uint8_t)(v << 6), 0, 0, 0 };

    d[3] = (uint8_t)(v >> 2);
    d[4] = (uint8_t)(v << 6);
    d[5] = (uint8_t)(c >> 2);
    d[6] = (uint8_t)(c << 6);
    send(0x160u, 7u, d);
}

static unsigned d160_byte3_pct10(uint16_t v)
{
    return ((v >> 2) * 392u + 50u) / 100u;
}

static void test_torque_pedal(void)
{
    const vehicle_in_t *v = vehicle_in();
    const nissan_in_t *x = nissan_in();

    vehicle_init();
    /* logged values: actual 0x5B4 -> 2835 x 0.1 Nm? raw 0x5B4 = 1460 -> 1460 x 5 - 4000 = 3300 */
    send180(0x5B4u, 0x5C5u, 3u);                     /* first frame: no previous counter -> invalid */
    vehicle_update_10ms();
    CHECK(v->q_torque_actual == Q_INVALID && x->q_torque_demand == Q_INVALID);
    send180(0x5B4u, 0x5C5u, 4u);                     /* counter +1 */
    vehicle_update_10ms();
    CHECK(v->q_torque_actual == Q_OK && x->q_torque_demand == Q_OK);
    CHECKV(v->torque_actual, 3300);                  /* 330.0 Nm */
    CHECKV(x->torque_demand, 3385);                  /* 0x5C5 = 1477 -> 338.5 Nm */
    CHECK((vehicle_quality_bits(v) & (1u << 1)) != 0u);
    send180(0x2DEu, 0x4C2u, 5u);                     /* the bench ECM's engine-off frame: -33.0 / 209.0 Nm */
    vehicle_update_10ms();
    CHECKV(v->torque_actual, -330);
    CHECKV(x->torque_demand, 2090);
    send180(0x000u, 0xFFFu, 6u);                     /* raw 0 = -400.0 Nm (the minimum); 0xFFF = invalid */
    vehicle_update_10ms();
    CHECK(v->q_torque_actual == Q_OK && v->torque_actual == -4000);
    CHECK(x->q_torque_demand == Q_INVALID && x->torque_demand == 0);
    send180(0x5B4u, 0x5C5u, 6u);                     /* counter frozen: both invalid */
    vehicle_update_10ms();
    CHECK(v->q_torque_actual == Q_INVALID && v->torque_actual == 0 && x->q_torque_demand == Q_INVALID);
    send180(0x5B4u, 0x5C5u, 8u);                     /* skipped one: invalid */
    vehicle_update_10ms();
    CHECK(v->q_torque_actual == Q_INVALID);
    send180(0x5B4u, 0x5C5u, 9u);                     /* back in step */
    vehicle_update_10ms();
    CHECK(v->q_torque_actual == Q_OK);
    send180(0x5B4u, 0x5C5u, 0u);                     /* wrap 15 -> 0 is +1: tested from 9 via 10..15 */
    vehicle_update_10ms();
    CHECK(v->q_torque_actual == Q_INVALID);
    for (uint8_t c = 1u; c <= 15u; c++) {
        send180(0x5B4u, 0x5C5u, c);
        vehicle_update_10ms();
    }
    send180(0x5B4u, 0x5C5u, 0u);
    vehicle_update_10ms();
    CHECK(v->q_torque_actual == Q_OK);
    ticks(4u);                                       /* 50 ms after the last frame: still fresh */
    CHECK(v->q_torque_actual == Q_OK);
    ticks(1u);
    CHECK(v->q_torque_actual == Q_INVALID && x->q_torque_demand == Q_INVALID);

    /* pedal: 800 raw (the logged full throttle) = 784 permil; complement checked */
    vehicle_init();
    send160(800u, 223u);
    vehicle_update_10ms();
    CHECK(v->q_pedal == Q_OK);
    CHECKV(v->pedal, 784);
    CHECKV(d160_byte3_pct10(800u), 784);            /* the DBC's byte-3 reading agrees: 200 x 0.392 % */
    send160(0u, 1023u);
    vehicle_update_10ms();
    CHECK(v->q_pedal == Q_OK && v->pedal == 0);
    send160(1023u, 0u);
    vehicle_update_10ms();
    CHECKV(v->pedal, 1000);                          /* 1003 clamped */
    send160(800u, 224u);                             /* complement off by one: invalid */
    vehicle_update_10ms();
    CHECK(v->q_pedal == Q_INVALID && v->pedal == 0);
    send160(800u, 223u);
    ticks(6u);                                       /* timeout 50 ms */
    CHECK(v->q_pedal == Q_INVALID);
}

/* the transmit set 0x174 / 0x176 / 0x177 (2026-11-23): refused until data, layouts as logged on
 * the truck, counters, 10 ms period, the cut only in a known upshift, 0x421 never sent */
static const can_frame_t *last_tx(uint16_t id)
{
    for (int i = (int)stub.n_tx - 1; i >= 0; i--) {
        if (stub.tx[i].id == id) {
            return &stub.tx[i];
        }
    }
    return NULL;
}

static void test_tx(void)
{
    vehicle_out_t o;
    can_frame_t f;

    memset(&o, 0, sizeof o);
    stub.n_tx = 0u;
    vehicle_tx_init();
    CHECK(!vehicle_tx_enable_torque(true));          /* no drivetrain data yet */
    vehicle_tx_set(&o);                              /* drivetrain_valid false: ignored */
    CHECK(!vehicle_tx_enable_torque(true));
    for (unsigned i = 0u; i < 50u; i++) {
        vehicle_tx_tick_1ms();
    }
    CHECKV(stub.n_tx, 0);

    /* a 3 -> 4 upshift in progress (logged: 174 B3 0x43, 177 B5 0x10 with B2 0x0A) */
    o.drivetrain_valid = true;
    o.gear_engaged = 3u;
    o.gear_target = 4u;
    o.output_rpm = 0x07C5u;
    o.turbine_rpm = 0x1252u;
    o.torque_cut_q8 = 0x4Du;
    o.lockup = true;
    vehicle_tx_set(&o);
    CHECK(vehicle_tx_enable_torque(true));
    nissan_tx_build(0u, &f);                         /* 0x174 */
    CHECK(f.id == 0x174u && f.dlc == 8u);
    CHECK(f.data[0] == 0xFFu && f.data[1] == 0xE7u && f.data[2] == 0x1Cu && f.data[3] == 0x43u);
    CHECK(f.data[4] == 0x00u && f.data[5] == 0x80u && f.data[6] == 0u && f.data[7] == 0u);
    nissan_tx_build(1u, &f);                         /* 0x176: the logged 07 C5 12 52 00 00 cc */
    CHECK(f.id == 0x176u && f.dlc == 7u);
    CHECK(f.data[0] == 0x07u && f.data[1] == 0xC5u && f.data[2] == 0x12u && f.data[3] == 0x52u &&
          f.data[4] == 0u && f.data[5] == 0u && f.data[6] == 0u);
    nissan_tx_build(2u, &f);                         /* 0x177: 00 00 0A 00 4D 10 */
    CHECK(f.id == 0x177u && f.dlc == 6u);
    CHECK(f.data[0] == 0u && f.data[1] == 0u && f.data[2] == 0x0Au && f.data[3] == 0u &&
          f.data[4] == 0x4Du && f.data[5] == 0x10u);

    /* 10 ms period, three frames each slot, counters +1 per frame in their low nibble */
    for (unsigned i = 0u; i < 100u; i++) {
        vehicle_tx_tick_1ms();
    }
    CHECKV(nissan_tx_sent_count(), 31);              /* 3 x 10 at 10 ms + 0x560 once (100 ms) */
    {
        unsigned n174 = 0u, n176 = 0u, n177 = 0u, n421 = 0u, n560 = 0u;
        uint8_t c174 = 0xFFu, c176 = 0xFFu;
        bool step_ok = true;

        for (unsigned i = 0u; i < stub.n_tx; i++) {
            const can_frame_t *t = &stub.tx[i];

            if (t->id == 0x174u) {
                if (c174 != 0xFFu && (uint8_t)((t->data[4] - c174) & 15u) != 1u) {
                    step_ok = false;
                }
                c174 = t->data[4] & 15u;
                n174++;
            } else if (t->id == 0x176u) {
                if (c176 != 0xFFu && (uint8_t)((t->data[6] - c176) & 15u) != 1u) {
                    step_ok = false;
                }
                c176 = t->data[6] & 15u;
                n176++;
            } else if (t->id == 0x177u) {
                n177++;
            } else if (t->id == 0x421u) {
                n421++;
            } else if (t->id == 0x560u) {
                CHECK(t->dlc == 4u && t->data[0] == 0x65u && t->data[1] == 0x15u && t->data[2] == '1' &&
                      t->data[3] == 0u);                 /* page 0x15: the 6th ID character of "8HP001" */
                n560++;
            }
        }
        CHECKV(n174, 10);
        CHECKV(n176, 10);
        CHECKV(n177, 10);
        CHECKV(n421, 0);                             /* the gear indicator is never sent (D-N1) */
        CHECKV(n560, 1);
        CHECK(step_ok);
    }

    /* 0x560 cycles its 12 logged pages, one per 100 ms frame */
    {
        static const uint8_t pg[13] = { 0x16, 0x17, 0x40, 0x41, 0x42, 0x43, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        static const uint8_t dt[13] = { 0x14, 0x06, 0, 0, 0, 0, '8', 'H', 'P', '0', '0', '1', 0x14 };
        bool ok = true;

        nissan_tx_host_crc = 0x1406A444u;            /* the CVN: the image CRC's top two bytes */

        for (unsigned k = 0u; k < 13u; k++) {
            stub.n_tx = 0u;
            for (unsigned i = 0u; i < 100u; i++) {
                vehicle_tx_tick_1ms();
            }
            const can_frame_t *t = last_tx(0x560u);
            if (t == NULL || t->data[1] != pg[k] || t->data[2] != dt[k]) {
                ok = false;
            }
        }
        CHECK(ok);
    }

    /* shift finished: target = engaged, no cut even if a stale level is still set */
    o.gear_engaged = 4u;
    o.gear_target = 4u;
    o.lockup = false;
    vehicle_tx_set(&o);
    nissan_tx_build(0u, &f);
    CHECK(f.data[3] == 0x44u && f.data[5] == 0x00u);
    nissan_tx_build(2u, &f);
    CHECK(f.data[2] == 0x03u && f.data[4] == 0u && f.data[5] == 0x78u);   /* code 15 = none */
    /* 1st gear: 177 B3 = 1, B2 = 0 (logged 1-2: 00 00 00 01 B4 00) */
    o.gear_engaged = 1u;
    o.gear_target = 2u;
    o.torque_cut_q8 = 0x67u;
    vehicle_tx_set(&o);
    nissan_tx_build(2u, &f);
    CHECK(f.data[2] == 0x00u && f.data[3] == 0x01u && f.data[4] == 0x67u && f.data[5] == 0x00u);
    /* downshift codes as logged; an unlogged shift sends "none" and no cut */
    CHECKV(nissan_tx_shift_code(4u, 3u), 9);
    CHECKV(nissan_tx_shift_code(5u, 4u), 13);
    CHECKV(nissan_tx_shift_code(3u, 2u), 15);
    CHECKV(nissan_tx_shift_code(4u, 5u), 3);
    o.gear_engaged = 3u;
    o.gear_target = 2u;
    vehicle_tx_set(&o);
    nissan_tx_build(2u, &f);
    CHECK(f.data[4] == 0x67u && f.data[5] == 0x78u);   /* any pending shift carries the cut (D-N5);
                                                           no 5-speed code for 3-2: 15 */
    /* 8HP gears (D-N5): 7 -> 8 */
    o.gear_engaged = 7u;
    o.gear_target = 8u;
    vehicle_tx_set(&o);
    nissan_tx_build(0u, &f);
    CHECK(f.data[3] == 0x87u);
    nissan_tx_build(2u, &f);
    CHECK(f.data[2] == 0x06u && f.data[4] == 0x67u && f.data[5] == 0x78u);
    /* disable: nothing more is sent */
    CHECK(!vehicle_tx_enable_torque(false));
    {
        const unsigned before = stub.n_tx;

        for (unsigned i = 0u; i < 50u; i++) {
            vehicle_tx_tick_1ms();
        }
        CHECKV(stub.n_tx, before);
    }
    CHECK(last_tx(0x174u) != NULL);
}

/* the TCU state -> the drivetrain fields (nissan_out_build, D-N5 + the 8HP TI amounts) */
static void test_out(void)
{
    nissan_out_src_t s;
    vehicle_out_t o;

    memset(&s, 0, sizeof s);
    CHECKV(nissan_gear_nibble(1), 1);
    CHECKV(nissan_gear_nibble(8), 8);
    CHECKV(nissan_gear_nibble(-1), NISSAN_GEAR_R);
    CHECKV(nissan_gear_nibble(0), NISSAN_GEAR_PN);
    CHECKV(nissan_gear_nibble(9), NISSAN_GEAR_PN);
    /* steady 6th, lock-up closed, speeds through */
    s.engaged = 6; s.target = 6; s.output_rpm = 1500u; s.turbine_rpm = 1505u; s.lockup = true;
    s.actual_ok = true; s.actual_nm10 = 2000;
    nissan_out_build(&s, &o);
    CHECK(o.drivetrain_valid && o.gear_engaged == 6u && o.gear_target == 6u && o.lockup);
    CHECK(o.output_rpm == 1500u && o.turbine_rpm == 1505u && o.torque_cut_q8 == 0u);
    /* a 2 -> 3 upshift, TI reduction -80 Nm of 200 Nm actual -> 0.4 x 256 = 102 */
    s.engaged = 2; s.target = 3; s.ti_armed = true; s.ti_cls = 1u; s.ti_type = 2u; s.ti_delta_nm = -80;
    nissan_out_build(&s, &o);
    CHECKV(o.gear_target, 3);
    CHECKV(o.torque_cut_q8, 102);
    s.ti_delta_nm = -500;                            /* more than the torque: clamped */
    nissan_out_build(&s, &o);
    CHECKV(o.torque_cut_q8, 255);
    s.ti_delta_nm = -80;
    s.ti_type = 1u;                                  /* an increase is not a cut */
    nissan_out_build(&s, &o);
    CHECKV(o.torque_cut_q8, 0);
    s.ti_type = 2u; s.ti_cls = 2u;                   /* garage: no gear pair */
    nissan_out_build(&s, &o);
    CHECKV(o.torque_cut_q8, 0);
    s.ti_cls = 0u;                                   /* a downshift reduction passes (the ECM's down timer) */
    s.engaged = 4; s.target = 3;
    nissan_out_build(&s, &o);
    CHECKV(o.torque_cut_q8, 102);
    s.actual_ok = false;                             /* no actual torque: no cut */
    nissan_out_build(&s, &o);
    CHECKV(o.torque_cut_q8, 0);
    s.actual_ok = true; s.actual_nm10 = 50;          /* < 10 Nm: no cut */
    nissan_out_build(&s, &o);
    CHECKV(o.torque_cut_q8, 0);
    s.actual_nm10 = 2000; s.ti_armed = false;
    nissan_out_build(&s, &o);
    CHECKV(o.torque_cut_q8, 0);
    /* no shift pending: no cut even with a stale reduction */
    s.ti_armed = true; s.engaged = 5; s.target = 5;
    nissan_out_build(&s, &o);
    CHECKV(o.torque_cut_q8, 0);
    /* into N from 3rd: no pending shift reported; R reported as 9 */
    s.engaged = 3; s.target = 0;
    nissan_out_build(&s, &o);
    CHECK(o.gear_engaged == 3u && o.gear_target == 3u && o.torque_cut_q8 == 0u);
    s.engaged = -1; s.target = -1;
    nissan_out_build(&s, &o);
    CHECK(o.gear_engaged == NISSAN_GEAR_R && o.gear_target == NISSAN_GEAR_R);
}

/* ---- end to end: the stock range chain with no lever source ---------------------------------- */
#define C8(a)   (*(volatile uint8_t *)(uintptr_t)(0xFFF80000u | (uint32_t)(a)))

static unsigned s_bad_outputs;

static void run_chain(bool brake_on, uint16_t rpm_raw, unsigned nticks, uint8_t *d6d6, uint8_t *d673,
                      uint8_t *d664)
{
    memset((void *)(uintptr_t)0xFFF80000u, 0, 0x16000u);
    CELL_TMP_BYTE = 67u;
    vehicle_init();
    park_init();
    rng_chain_init();
    kl15_init();
    *(volatile uint16_t *)(uintptr_t)KL15_SRC_CELL = 13206u;       /* the bench supply: the voltage ignition path */
    for (unsigned t = 0u; t < nticks; t++) {
        send_brake(brake_on, brake_on);
        send_rpm(rpm_raw, rpm_raw);
        vehicle_update_10ms();
        kl15_tick_10ms();
        rng_chain_tick_10ms(1u, 0u, 1u);
        park_tick_10ms();
        if (park_slot7_release() != 0u || park_slot11_hold() != 0u) {
            s_bad_outputs++;
        }
    }
    *d6d6 = C8(0xD6D6u);
    *d673 = C8(0xD673u);
    *d664 = C8(0xD664u);
}

static void test_range_holds_p(void)
{
    uint8_t d6, d673, d664;

    /* brake ON + engine ~700 rpm (the conditions under which the FCA bench's lever releases P) but NO
     * lever source: the range stays P, no park output ever comes on */
    run_chain(true, 5650u, 600u, &d6, &d673, &d664);
    CHECKV(d6, 8);
    CHECKV(d673, 2);                                 /* lever source 2 = none */
    CHECKV(d664, 1);                                 /* U1267 confirmed after 500 ms: the lost-shifter reaction */
    CHECKV(s_bad_outputs, 0);
    /* and with the brake off, engine off */
    run_chain(false, 0u, 600u, &d6, &d673, &d664);
    CHECKV(d6, 8);
    CHECKV(d673, 2);
    CHECKV(s_bad_outputs, 0);
}

int main(void)
{
    test_defaults();
    test_engine();
    test_brake();
    test_coolant_and_extras();
    test_not_available();
    test_torque_pedal();
    test_tx();
    test_out();
    test_range_holds_p();
    if (fails == 0) {
        printf("profile_nissan: all checks passed\n");
    }
    return fails != 0;
}
