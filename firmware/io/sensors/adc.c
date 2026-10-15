/* Analog averaging: samples every 1 ms, average every 10 ms, scaled to millivolts at the module
 * connector with the external divider ratio of each channel (spec 04 sections 2-3):
 *     mV = mean(raw) x 5000 / 65536 x ratio
 * The divider ratios describe the board's resistor networks (hardware), not calibration. */
#include "io/sensors/sensors.h"

#include <string.h>

#define AN_SLOTS 35u                    /* AN0..AN27 -> 0..27, AN40..AN46 -> 28..34 */

/* divider ratio x 10 (spec 04 section 3) */
static const uint8_t s_ratio10[AN_SLOTS] = {
    /* AN0..AN9   */ 10, 10, 20, 25, 25, 25, 25, 40, 10, 25,
    /* AN10..AN19 */ 25, 10, 10, 10, 10, 10, 10, 10, 10, 50,
    /* AN20..AN27 */ 50, 50, 48, 20, 10, 10, 10, 10,
    /* AN40..AN46 */ 10, 20, 10, 10, 10, 10, 10,
};

static uint32_t s_sum[AN_SLOTS];
static uint16_t s_n[AN_SLOTS];
static mv_t s_mv[AN_SLOTS];
static bool s_valid[AN_SLOTS];

static int slot(uint8_t an)
{
    if (an <= 27u) {
        return an;
    }
    if (an >= 40u && an <= 46u) {
        return 28 + (an - 40);
    }
    return -1;
}

static uint8_t an_of(uint8_t i)
{
    return i <= 27u ? i : (uint8_t)(40u + (i - 28u));
}

void adc_reset(void)
{
    memset(s_sum, 0, sizeof s_sum);
    memset(s_n, 0, sizeof s_n);
    memset(s_mv, 0, sizeof s_mv);
    memset(s_valid, 0, sizeof s_valid);
}

void adc_sample(void)
{
    for (uint8_t i = 0u; i < AN_SLOTS; i++) {
        uint16_t raw;
        if (plat_adc_raw(an_of(i), &raw)) {
            s_sum[i] += raw;
            s_n[i]++;
        }
    }
}

void adc_average(void)
{
    for (uint8_t i = 0u; i < AN_SLOTS; i++) {
        if (s_n[i] != 0u) {
            /* sum/n x 5000/65536 x ratio10/10, rounded; 5000/65536 = 625/8192 keeps the
             * product below 2^32 (65535 x 50 x 625) */
            const uint32_t mean = s_sum[i] / s_n[i];
            const uint32_t mv = (mean * s_ratio10[i] * 625u / 8192u + 5u) / 10u;
            s_mv[i] = (mv_t)(mv > 0xFFFFu ? 0xFFFFu : mv);
            s_valid[i] = true;
        } else {
            s_valid[i] = false;
        }
        s_sum[i] = 0u;
        s_n[i] = 0u;
    }
}

bool adc_mv(uint8_t an, mv_t *mv)
{
    const int i = slot(an);

    if (i < 0 || !s_valid[i]) {
        return false;
    }
    *mv = s_mv[i];
    return true;
}
