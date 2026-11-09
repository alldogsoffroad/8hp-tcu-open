/* sol_ctl.c — solenoid current control, rebuilt onto the bus-A job engine
 * (design §4). The PHASE1/PHASE2 leap logic and keep-warm cycle are
 * carried from the proven sol_drv.c; the transport is the new drv engine.
 */
#include "io/actuators/sol_ctl.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/sup/sup_cells.h"
#include <string.h>

static sol_chan_t s_chan[SOL_CHANNELS];
static uint16_t s_cyclic_sp[8];     /* the 8 setpoints fed to drv_cyclic_build */

void sol_init(void)
{
    memset(s_chan, 0, sizeof s_chan);
    memset(s_cyclic_sp, 0, sizeof s_cyclic_sp);
}

void sol_set(uint8_t ch, uint8_t mode, uint16_t setpoint)
{
    if (ch < SOL_CHANNELS) {
        s_chan[ch].mode = mode;
        s_chan[ch].setpoint = setpoint & 0x7FFu;
    }
}

const sol_chan_t *sol_channel(uint8_t ch)
{
    return ch < SOL_CHANNELS ? &s_chan[ch] : 0;
}

void sol_engage_config(uint8_t level)
{
    /* level-change config sync: queue the DRV_CFG job with the stock's
     * live rows (8E←9F, 90←21, 96←55, 88←mode) */
    if (drv_job_check_free(DRV_CFG)) {
        drv_job_queue(DRV_CFG);
    }
}

void sol_tick_10ms(void)
{
    /* build the 45-frame cyclic from the channel state */
    for (uint8_t ch = 0u; ch < SOL_CHANNELS; ch++) {
        s_cyclic_sp[ch] = s_chan[ch].mode == SOL_MODE_CURRENT
                          ? s_chan[ch].setpoint : 0u;
    }
    drv_cyclic_build(s_cyclic_sp, SOL_CHANNELS);
    if (drv_job_check_free(DRV_CYC)) {
        drv_job_queue(DRV_CYC);
    }
}
