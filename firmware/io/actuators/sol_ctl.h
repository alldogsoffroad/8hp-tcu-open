/* sol_ctl.h — solenoid current control on the bus-A engine (design §4). */
#ifndef TCU_SOL_CTL_H
#define TCU_SOL_CTL_H

#include <stdint.h>
#include <stdbool.h>

#define SOL_CHANNELS 8
#define SOL_MODE_OFF   0u
#define SOL_MODE_CURRENT 1u
#define SOL_CH_PARK_RELEASE 7u   /* MV Pos: park release valve, on/off (spec 12 section 1) */

typedef struct {
    uint8_t mode;         /* SOL_MODE_* */
    uint16_t setpoint;    /* 11-bit mA */
} sol_chan_t;

void sol_init(void);
void sol_set(uint8_t ch, uint8_t mode, uint16_t setpoint);
void sol_tick_10ms(void);           /* builds the cyclic, queues DRV_CYC */
void sol_engage_config(uint8_t level); /* level-change config sync */

const sol_chan_t *sol_channel(uint8_t ch);

#endif
