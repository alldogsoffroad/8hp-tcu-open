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

static void test_tx_stub(void)
{
    const vehicle_out_t o = { { 2000u, 2000u, 2000u }, true };

    stub.n_tx = 0u;
    vehicle_tx_init();
    vehicle_tx_set(&o);
    CHECK(!vehicle_tx_enable_torque(true));          /* nothing exists to enable */
    CHECK(!vehicle_tx_enable_torque(false));
    for (unsigned i = 0u; i < 100u; i++) {
        vehicle_tx_tick_1ms();
    }
    CHECKV(stub.n_tx, 0);                            /* 0x177 / 0x421 are TODOs: no frame, ever */
    CHECKV(nissan_tx_sent_count(), 0);
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
    test_tx_stub();
    test_range_holds_p();
    if (fails == 0) {
        printf("profile_nissan: all checks passed\n");
    }
    return fails != 0;
}
