/* Sensor input aggregation: speeds, fluid temperature, supply and park contacts (spec 02, 04). */
#include "io/sensors/sensors.h"

#include <string.h>

#define AN_TFT_NODE   8u        /* fluid temperature sensor node        [I] spec 04 4.1 */
#define AN_TFT_SENSE  1u        /* fluid temperature sense return       [I] */
#define AN_BATTERY    21u       /* supply voltage                       [I] spec 04 4 */
#define AN_WAKE1      19u       /* switched supply 1                    [I] */
#define AN_WAKE2      20u       /* switched supply 2                    [I] */
static const uint8_t AN_PARK[2] = { 5u, 6u };   /* park contacts        [I] spec 04 5.0 */

#define WAKE_ON_MV    4000u     /* project hysteresis for the switched supplies */
#define WAKE_OFF_MV   3000u
#define BATTERY_MIN_MV 6000u    /* below this the resistor-coded inputs are not evaluated */
#define TFT_HOLD_MS   1000u     /* hold the last valid temperature this long, then invalid */

/* Resistor-coded switch bands, ratio input/supply in 0.01 % (spec 04 4.4). Valid levels are
 * bands 1 (contact closed) and 3 (open). PROVISIONAL: band edges as observed in the stock
 * software; re-derive from the measured contact levels on the bench. */
static const uint16_t s_band_edge[5] = { 100u, 1900u, 5000u, 8000u, 9500u };

static const sensors_cfg_t *s_cfg;
static sensors_in_t s_in;
static speed_state_t s_speed[SPEED_CHANNELS];
static uint8_t s_div100;
static uint16_t s_tft_bad_ms;
static bool s_tft_seen;

uint8_t park_band(mv_t input, mv_t supply)
{
    uint32_t ratio;
    uint8_t band = 0u;

    if (supply < BATTERY_MIN_MV) {
        return 0xFFu;
    }
    ratio = (uint32_t)input * 10000u / supply;
    while (band < 5u && ratio >= s_band_edge[band]) {
        band++;
    }
    return band;
}

void sensors_init(const sensors_cfg_t *cfg)
{
    s_cfg = cfg;
    memset(&s_in, 0, sizeof s_in);
    memset(s_speed, 0, sizeof s_speed);
    for (uint8_t i = 0u; i < SPEED_CHANNELS; i++) {
        s_in.q_ch_speed[i] = Q_INVALID;
    }
    s_in.q_turbine_speed = s_in.q_output_speed = Q_INVALID;
    s_in.q_fluid_temp = s_in.q_battery = s_in.q_park = Q_INVALID;
    s_div100 = 0u;
    s_tft_bad_ms = 0u;
    s_tft_seen = false;
    adc_reset();
}

void sensors_tick_1ms(void)
{
    adc_sample();
}

static void update_speeds(void)
{
    for (uint8_t ch = 0u; ch < SPEED_CHANNELS; ch++) {
        speed_raw_t r;
        bool ok;
        uint32_t q;

        if (!plat_speed_read(ch, &r)) {
            s_in.q_ch_speed[ch] = Q_INVALID;
            continue;
        }
        q = speed_update(&s_speed[ch], &r, s_cfg->pulses_per_rev[ch], &ok);
        s_in.ch_speed[ch] = (rpm_t)((q + 2u) / 4u);
        s_in.q_ch_speed[ch] = ok ? Q_OK : Q_SUBSTITUTED;
    }
    s_in.turbine_speed = s_in.ch_speed[s_cfg->turbine_ch];
    s_in.q_turbine_speed = s_in.q_ch_speed[s_cfg->turbine_ch];
    s_in.output_speed = s_in.ch_speed[s_cfg->output_ch];
    s_in.q_output_speed = s_in.q_ch_speed[s_cfg->output_ch];
}

static void update_fluid_temp(void)
{
    mv_t an8, an1;
    degc10_t t = 0;
    uint8_t f;

    if (!adc_mv(AN_TFT_NODE, &an8) || !adc_mv(AN_TFT_SENSE, &an1)) {
        f = TFT_F_NO_ADC;
    } else {
        f = tft_convert(an8, an1, &t);
    }
    s_in.fluid_temp_faults = f;
    if (f == 0u) {
        s_in.fluid_temp = t;
        s_in.q_fluid_temp = Q_OK;
        s_tft_bad_ms = 0u;
        s_tft_seen = true;
        return;
    }
    /* invalid: hold the last good value for a while, then report invalid */
    if (s_tft_bad_ms < TFT_HOLD_MS) {
        s_tft_bad_ms = (uint16_t)(s_tft_bad_ms + 100u);
    }
    s_in.q_fluid_temp = s_tft_seen && s_tft_bad_ms < TFT_HOLD_MS ? Q_SUBSTITUTED : Q_INVALID;
}

static void update_supply_and_park(void)
{
    mv_t v;
    uint8_t band[2];

    s_in.q_battery = adc_mv(AN_BATTERY, &s_in.battery) ? Q_OK : Q_INVALID;
    if (adc_mv(AN_WAKE1, &v)) {
        s_in.wake1 = v >= WAKE_ON_MV ? true : (v < WAKE_OFF_MV ? false : s_in.wake1);
    }
    if (adc_mv(AN_WAKE2, &v)) {
        s_in.wake2 = v >= WAKE_ON_MV ? true : (v < WAKE_OFF_MV ? false : s_in.wake2);
    }

    for (uint8_t i = 0u; i < 2u; i++) {
        band[i] = s_in.q_battery == Q_OK && adc_mv(AN_PARK[i], &v) ? park_band(v, s_in.battery) : 0xFFu;
        s_in.park_band[i] = band[i];
    }
    /* [I] contact 0 closed (band 1) and contact 1 open (band 3) = in Park, and the reverse =
     * not in Park; equal states or invalid bands = unknown. Which contact is which: bench. */
    if (band[0] == 1u && band[1] == 3u) {
        s_in.park = PARK_IN_P;
        s_in.q_park = Q_OK;
    } else if (band[0] == 3u && band[1] == 1u) {
        s_in.park = PARK_NOT_P;
        s_in.q_park = Q_OK;
    } else {
        s_in.park = PARK_UNKNOWN;
        s_in.q_park = Q_INVALID;
    }
}

void sensors_update_10ms(void)
{
    adc_average();
    update_speeds();
    update_supply_and_park();
    if (++s_div100 >= 10u) {
        s_div100 = 0u;
        update_fluid_temp();
    }
}

const sensors_in_t *sensors_in(void)
{
    return &s_in;
}
