#include "app/bench_out.h"
#include "bsw/diag/uds.h"
#include "bsw/sup/sup.h"
#include "io/actuators/sol_drv.h"
#include "io/vehicle/vehicle.h"

#define NRC_IMLOIF 0x13
#define NRC_CNC    0x22
#define NRC_ROOR   0x31
#define SES_EXT    0x03u
#define ENGINE_RUNNING_RPM 50

static bool s_active;
static uint8_t s_ch;
static uint16_t s_ma;
static uint16_t s_left_10ms;
static uint8_t s_reason;

static bool engine_running(void)
{
    const vehicle_in_t *v = vehicle_in();
    return v->q_engine_speed == Q_OK && v->engine_speed >= ENGINE_RUNNING_RPM;
}

static bool drivers_healthy(void)
{
    const sol_status_t *s = sol_status();
    return s->ready && s->faults == 0u;
}

static bool supervisor_healthy(void)
{
    const sup_status_t *s = sup_status();
    return (s->last_rx & SUP_REPLY_MASK) == SUP_REPLY_OK;
}

static void stop(uint8_t reason)
{
    if (s_active) {
        s_reason = reason;
    }
    s_active = false;
    s_left_10ms = 0u;
    sol_all_off();
    sup_set_output_enable(false);
}

void bench_init(void)
{
    s_active = false;
    s_reason = BENCH_END_NONE;
    stop(BENCH_END_NONE);
}

bool bench_active(void)
{
    return s_active;
}

void bench_tick_10ms(void)
{
    if (!s_active) {
        sol_all_off();
        sup_set_output_enable(false);
        return;
    }
    if (uds_session() != SES_EXT) {
        stop(BENCH_END_SESSION);
    } else if (engine_running()) {
        stop(BENCH_END_ENGINE);
    } else if (!drivers_healthy()) {
        stop(BENCH_END_DRIVER);
    } else if (!supervisor_healthy()) {
        stop(BENCH_END_SUPERVISOR);
    } else if (s_left_10ms == 0u || --s_left_10ms == 0u) {
        stop(BENCH_END_TIMEOUT);
    } else {
        sol_all_off();
        sol_set(s_ch, SOL_MODE_CURRENT, (ma10_t)(s_ma * 10u));
        sup_set_output_enable(true);
    }
}

int bench_routine(uint8_t type, uint16_t rid, const uint8_t *in, uint16_t n, uint8_t *out,
                  uint16_t max)
{
    if (rid != BENCH_RID) {
        return -NRC_ROOR;
    }
    if (type == 1u) {                               /* start */
        uint16_t ma;
        if (n != 3u) {
            return -NRC_IMLOIF;
        }
        ma = (uint16_t)(in[1] * 10u);
        if (in[0] > BENCH_MAX_CHANNEL || ma == 0u || ma > BENCH_MAX_MA ||
            in[2] == 0u || in[2] > BENCH_MAX_S) {
            return -NRC_ROOR;
        }
        if (s_active || engine_running() || !drivers_healthy() || !supervisor_healthy()) {
            return -NRC_CNC;
        }
        s_ch = in[0];
        s_ma = ma;
        s_left_10ms = (uint16_t)(in[2] * 100u);
        s_reason = BENCH_END_NONE;
        s_active = true;
        return 0;
    }
    if (type == 2u) {                               /* stop */
        stop(BENCH_END_STOPPED);
        return 0;
    }
    if (max < 7u) {                                 /* results */
        return -NRC_IMLOIF;
    }
    out[0] = s_active ? 1u : 0u;
    out[1] = s_ch;
    out[2] = (uint8_t)(s_ma >> 8);
    out[3] = (uint8_t)s_ma;
    out[4] = (uint8_t)(s_left_10ms / 10u > 255u ? 255u : s_left_10ms / 10u);
    out[5] = s_reason;
    out[6] = sol_status()->faults;
    return 7;
}
