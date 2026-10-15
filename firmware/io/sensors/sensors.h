/* Transmission sensor inputs (architecture: io layer, milestone M2).
 *
 * The TCU's own measurements in project units with a quality per value. Application code reads
 * only this structure. Specs: 02 section 4.1 (speeds), 04 (analog inputs). Items marked [I]
 * are interpretations still to be confirmed on the bench.
 */
#ifndef TCU_SENSORS_H
#define TCU_SENSORS_H

#include "platform/platform.h"

typedef enum { PARK_UNKNOWN = 0, PARK_IN_P, PARK_NOT_P } park_pos_t;

/* Fluid temperature fault bits (own limits, see tft.c). */
enum {
    TFT_F_SENSE_LOW  = 0x01u,   /* AN1 below range: open circuit / short to ground */
    TFT_F_SENSE_HIGH = 0x02u,
    TFT_F_NODE_LOW   = 0x04u,   /* AN8 below range: sensor shorted */
    TFT_F_NODE_HIGH  = 0x08u,   /* AN8 above range: sensor open    */
    TFT_F_RATIO      = 0x10u,   /* pull-up/sense ratio outside the circuit's window */
    TFT_F_RANGE      = 0x20u,   /* temperature outside the table   */
    TFT_F_NO_ADC     = 0x40u
};

typedef struct {
    /* speeds: per hardware channel (TIF12, TIF13, TIF14) and assigned to shafts */
    rpm_t     ch_speed[SPEED_CHANNELS];  quality_t q_ch_speed[SPEED_CHANNELS];
    rpm_t     turbine_speed;             quality_t q_turbine_speed;
    rpm_t     output_speed;              quality_t q_output_speed;

    degc10_t  fluid_temp;                quality_t q_fluid_temp;
    uint8_t   fluid_temp_faults;         /* TFT_F_* of the last evaluation */

    mv_t      battery;                   quality_t q_battery;   /* [I] AN21 */
    bool      wake1, wake2;              /* [I] AN19 / AN20 switched supplies present */

    park_pos_t park;                     quality_t q_park;      /* [I] AN5 / AN6 contacts */
    uint8_t   park_band[2];              /* band 0..5 of each contact (spec 04 4.4) */
} sensors_in_t;

typedef struct {
    uint8_t  turbine_ch;        /* speed channel of the turbine shaft [I], bench to confirm */
    uint8_t  output_ch;         /* speed channel of the output shaft [I]                    */
    uint16_t pulses_per_rev[SPEED_CHANNELS];
} sensors_cfg_t;

void sensors_init(const sensors_cfg_t *cfg);
void sensors_tick_1ms(void);        /* ADC sampling */
void sensors_update_10ms(void);     /* averaging, speeds, supply, park; fluid temp every 100 ms */
const sensors_in_t *sensors_in(void);

/* ---- internal building blocks, exposed for unit tests ---- */

/* Averaged voltage at the module connector (divider-corrected), mV; false if not sampled. */
bool adc_mv(uint8_t an, mv_t *mv);
void adc_reset(void);
void adc_sample(void);              /* one sample of every channel (1 ms) */
void adc_average(void);             /* close the averaging window (10 ms) */

typedef struct {
    uint16_t ref_edges;
    uint32_t ref_time;
    uint32_t speed_q;               /* 1/4 rpm */
    uint32_t ms_since_edge;
    bool     started;
} speed_state_t;

/* One 10 ms update of one channel; returns speed in 1/4 rpm, sets *ok false if implausible. */
uint32_t speed_update(speed_state_t *s, const speed_raw_t *r, uint16_t ppr, bool *ok);

/* Fluid temperature from AN8 and AN1 (mV); returns fault bits (0 = valid, *t set). */
uint8_t tft_convert(mv_t an8, mv_t an1, degc10_t *t);

/* Resistor-coded switch band 0..5 from input and supply mV (spec 04 4.4); 0xFF if supply low. */
uint8_t park_band(mv_t input, mv_t supply);

#endif
