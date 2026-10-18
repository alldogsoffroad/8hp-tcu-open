/* Unit test: sensor inputs (ADC scaling, speeds, fluid temperature, supply, park contacts). */
#include "io/sensors/sensors.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <stdlib.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define NEAR(a, b, tol) (abs((int)(a) - (int)(b)) <= (tol))

static const sensors_cfg_t cfg = { 0u, 1u, { 60u, 60u, 60u } };

/* raw ADC value for a voltage at the connector of a channel with divider ratio x10 */
static void set_mv(uint8_t an, uint32_t mv, uint32_t ratio10)
{
    stub.adc[an] = (uint16_t)((uint64_t)mv * 10u * 65536u / 5000u / ratio10);
    stub.adc_on[an] = true;
}

static void run_ms(unsigned ms)
{
    for (unsigned i = 0u; i < ms; i++) {
        sensors_tick_1ms();
        if (i % 10u == 9u) {
            sensors_update_10ms();
        }
    }
}

/* fluid temperature circuit with pull-up/sense ratio 10.41: AN1 = (5000 - AN8) / 10.41 */
static void set_tft(uint32_t an8)
{
    set_mv(8u, an8, 10u);
    set_mv(1u, (5000u - an8) * 100u / 1041u, 10u);
}

int main(void)
{
    const sensors_in_t *s = sensors_in();
    degc10_t t;

    sensors_init(&cfg);

    /* ---- supply, divider ratio 5 ---- */
    set_mv(21u, 13500u, 50u);
    run_ms(10u);
    /* bench calibration: 13 500 mV stock-scale reading -> 1.0553 x 13 500 - 89 = 14 157 mV */
    CHECK(s->q_battery == Q_OK && NEAR(s->battery, 14157, 6));

    /* ---- wake inputs with hysteresis ---- */
    set_mv(19u, 3500u, 50u);
    run_ms(10u);
    CHECK(!s->wake1);
    set_mv(19u, 4100u, 50u);
    run_ms(10u);
    CHECK(s->wake1);
    set_mv(19u, 3500u, 50u);
    run_ms(10u);
    CHECK(s->wake1);
    set_mv(19u, 2900u, 50u);
    run_ms(10u);
    CHECK(!s->wake1);

    /* ---- fluid temperature ---- */
    CHECK(tft_convert(3241u, 169u, &t) == 0u && NEAR(t, 200, 15));
    CHECK(tft_convert(1334u, 352u, &t) == 0u && NEAR(t, 800, 40));
    CHECK(tft_convert(4494u, 49u, &t) == 0u && NEAR(t, -400, 40));
    CHECK(tft_convert(4950u, 5u, &t) == (TFT_F_SENSE_LOW | TFT_F_NODE_HIGH));   /* sensor open */
    CHECK(tft_convert(200u, 460u, &t) == TFT_F_NODE_LOW);                       /* sensor shorted */
    CHECK(tft_convert(3241u, 300u, &t) == TFT_F_RATIO);                         /* circuit fault */
    set_tft(2151u);
    run_ms(100u);
    CHECK(s->q_fluid_temp == Q_OK && NEAR(s->fluid_temp, 500, 40) && s->fluid_temp_faults == 0u);
    set_tft(4950u);                                   /* sensor opens: hold, then invalid */
    stub.adc[1] = 0u;
    run_ms(100u);
    CHECK(s->q_fluid_temp == Q_SUBSTITUTED && NEAR(s->fluid_temp, 500, 40));
    run_ms(1000u);
    CHECK(s->q_fluid_temp == Q_INVALID && (s->fluid_temp_faults & TFT_F_NODE_HIGH) != 0u);

    /* ---- park contacts on a 12 V supply ---- */
    set_mv(21u, 12000u, 50u);
    set_mv(5u, 1200u, 25u);                           /* 10 % -> band 1 */
    set_mv(6u, 7200u, 25u);                           /* 60 % -> band 3 */
    run_ms(10u);
    CHECK(s->park_band[0] == 1u && s->park_band[1] == 3u);
    CHECK(s->park == PARK_IN_P && s->q_park == Q_OK);
    set_mv(5u, 7200u, 25u);
    set_mv(6u, 1200u, 25u);
    run_ms(10u);
    CHECK(s->park == PARK_NOT_P && s->q_park == Q_OK);
    set_mv(5u, 1200u, 25u);                           /* both closed: invalid combination */
    run_ms(10u);
    CHECK(s->park == PARK_UNKNOWN && s->q_park == Q_INVALID);
    set_mv(6u, 11900u, 25u);                          /* 99 %: short to supply */
    run_ms(10u);
    CHECK(s->park_band[1] == 5u && s->q_park == Q_INVALID);
    CHECK(park_band(1000u, 5000u) == 0xFFu);          /* supply too low */

    /* ---- speed: 1000 rpm at 60 pulses/rev = 10 edges and 40000 ticks per 10 ms ---- */
    stub.speed_on[0] = stub.speed_on[1] = true;
    stub.speed[0].t_edge = SPEED_TIME_MASK - 100000u;  /* wraps during the test */
    for (int i = 0; i < 30; i++) {
        stub.speed[0].edges = (uint16_t)(stub.speed[0].edges + 10u);
        stub.speed[0].t_edge = (stub.speed[0].t_edge + 40000u) & SPEED_TIME_MASK;
        stub.speed[0].age = 1000u;
        run_ms(10u);
    }
    CHECK(s->q_ch_speed[0] == Q_OK && NEAR(s->ch_speed[0], 1000, 1));
    CHECK(s->turbine_speed == s->ch_speed[0] && s->q_turbine_speed == Q_OK);
    CHECK(s->output_speed == 0 && s->q_output_speed == Q_OK);   /* channel 1: no edges */

    /* pulses stop: speed decays (bound 4e6 x 1.1 / age), then reads 0 after 1 s */
    stub.speed[0].age = 400000u;                      /* 100 ms since the last edge */
    run_ms(10u);
    CHECK(s->ch_speed[0] > 0 && NEAR(s->ch_speed[0], 11, 1));
    for (int i = 0; i < 100; i++) {
        stub.speed[0].age += 40000u;
        run_ms(10u);
    }
    CHECK(s->ch_speed[0] == 0 && s->q_ch_speed[0] == Q_OK);

    /* restart: the first edge after a stop gives no speed yet, the next interval does */
    stub.speed[0].edges = (uint16_t)(stub.speed[0].edges + 1u);
    stub.speed[0].t_edge = (stub.speed[0].t_edge + 4000000u) & SPEED_TIME_MASK;
    run_ms(10u);
    CHECK(s->ch_speed[0] == 0);
    stub.speed[0].edges = (uint16_t)(stub.speed[0].edges + 2u);
    stub.speed[0].t_edge = (stub.speed[0].t_edge + 40000u) & SPEED_TIME_MASK;
    run_ms(10u);
    CHECK(NEAR(s->ch_speed[0], 200, 1));               /* 2 edges in 10 ms = 200 rpm */

    /* glitch: 500 edges in 10 ms (50000 rpm) is rejected, last value kept */
    stub.speed[0].edges = (uint16_t)(stub.speed[0].edges + 500u);
    stub.speed[0].t_edge = (stub.speed[0].t_edge + 40000u) & SPEED_TIME_MASK;
    run_ms(10u);
    CHECK(s->q_ch_speed[0] == Q_SUBSTITUTED && NEAR(s->ch_speed[0], 200, 1));

    /* missing channel */
    stub.speed_on[1] = false;
    run_ms(10u);
    CHECK(s->q_output_speed == Q_INVALID);

    if (fails == 0) {
        printf("test_sensors: ok\n");
    }
    return fails != 0;
}
