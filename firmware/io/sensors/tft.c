/* Transmission fluid temperature (spec 04 section 4.1).
 *
 * Circuit [I]: 5 V -> pull-up -> AN8 -> sensor -> AN1 -> sense resistor -> 0 V.
 * r = (AN8 - AN1) / AN1 = R_sensor / R_sense is independent of the supply and the pull-up;
 * temperature comes from the PROVISIONAL project table tft_table.h (tools/gen_tft_table.py).
 *
 * Plausibility limits are the project's own, derived from the circuit model (pull-up/sense ratio
 * about 10.4..13 and a sensor range of -50..180 degC), with margin:
 */
#include "io/sensors/sensors.h"
#include "io/sensors/tft_table.h"

#define SENSE_MIN_MV   20u      /* -50 degC gives about 37 mV  */
#define SENSE_MAX_MV   480u     /* 180 degC gives about 440 mV */
#define NODE_MIN_MV    350u     /* 180 degC gives about 500 mV */
#define NODE_MAX_MV    4800u    /* sensor open: AN8 near 5 V   */
#define PULL_RATIO_MIN 9000u    /* (5000 - Vdiff) / AN1 x 1000 = (R_pull + R_sense) / R_sense */
#define PULL_RATIO_MAX 16000u

uint8_t tft_convert(mv_t an8, mv_t an1, degc10_t *t)
{
    uint8_t f = 0u;
    uint32_t vdiff, ratio, pull;

    if (an1 < SENSE_MIN_MV) {
        f |= TFT_F_SENSE_LOW;
    }
    if (an1 > SENSE_MAX_MV) {
        f |= TFT_F_SENSE_HIGH;
    }
    if (an8 < NODE_MIN_MV) {
        f |= TFT_F_NODE_LOW;
    }
    if (an8 > NODE_MAX_MV) {
        f |= TFT_F_NODE_HIGH;
    }
    if (f != 0u) {
        return f;
    }

    vdiff = an8 > an1 ? (uint32_t)(an8 - an1) : 0u;
    pull = (5000u - vdiff) * 1000u / an1;
    if (pull < PULL_RATIO_MIN || pull > PULL_RATIO_MAX) {
        return TFT_F_RATIO;
    }

    ratio = vdiff * 1000u / an1;            /* R_sensor / R_sense in 1/1000 */
    if (ratio > tft_table[0].ratio_milli || ratio < tft_table[TFT_TABLE_N - 1u].ratio_milli) {
        return TFT_F_RANGE;
    }
    for (uint8_t i = 1u; i < TFT_TABLE_N; i++) {
        if (ratio >= tft_table[i].ratio_milli) {
            const uint32_t r0 = tft_table[i - 1u].ratio_milli, r1 = tft_table[i].ratio_milli;
            const int32_t t0 = tft_table[i - 1u].degc10, t1 = tft_table[i].degc10;
            /* r falls as t rises: t = t0 + (t1 - t0) x (r0 - r) / (r0 - r1) */
            *t = (degc10_t)(t0 + (t1 - t0) * (int32_t)(r0 - ratio) / (int32_t)(r0 - r1));
            return 0u;
        }
    }
    return TFT_F_RANGE;
}
