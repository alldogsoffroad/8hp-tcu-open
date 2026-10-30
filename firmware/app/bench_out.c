#include "app/bench_out.h"
#include "bsw/diag/uds.h"
#include "bsw/sup/sup.h"
#include "io/actuators/sol_drv.h"
#include "io/vehicle/vehicle.h"

#if defined(TCU_M4B_OUTPUTS)
#include "platform/platform.h"          /* plat_toe_out_* for the FE11 park hold test */
#endif

#define NRC_IMLOIF 0x13
#define NRC_CNC    0x22
#define NRC_ROOR   0x31
#define NRC_SFNS   0x12
#define SES_EXT    0x03u
#define ENGINE_RUNNING_RPM 50

typedef enum { BK_SOL = 0, BK_PARK_OUT = 1 } bench_kind_t;

static bool s_active;
static bench_kind_t s_kind;
static uint8_t s_ch;                    /* FE10 channel or FE11 output selector */
static uint16_t s_ma;
static uint16_t s_duty;
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

/* Channels 0-6 are the bench channels: forced off whenever no test drives them.
 * Channel 7 and TOE31 belong to the park module (M4b builds); they are cleared here
 * only when a park commissioning test ends. */
static void bench_channels_off(void)
{
    for (uint8_t ch = 0u; ch <= BENCH_MAX_CHANNEL; ch++) {
        sol_set(ch, SOL_MODE_OFF, 0u);
    }
}

static void stop(uint8_t reason)
{
    if (s_active) {
        s_reason = reason;
#if defined(TCU_M4B_OUTPUTS)
        if (s_kind == BK_PARK_OUT) {
            sol_set(SOL_CH_PARK_RELEASE, SOL_MODE_OFF, 0u);
            plat_toe_out_set(0u);
        }
#endif
    }
    s_active = false;
    s_left_10ms = 0u;
    bench_channels_off();
    sup_enable_clear(SUP_EN_BENCH);
}

void bench_init(void)
{
    s_active = false;
    s_kind = BK_SOL;
    s_reason = BENCH_END_NONE;
    s_left_10ms = 0u;
    bench_channels_off();
    sup_enable_clear(SUP_EN_BENCH);
}

bool bench_active(void)
{
    return s_active;
}

void bench_tick_10ms(void)
{
    if (!s_active) {
        bench_channels_off();
        sup_enable_clear(SUP_EN_BENCH);
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
    } else if (s_kind == BK_SOL) {
        bench_channels_off();
        sol_set(s_ch, SOL_MODE_CURRENT, (ma10_t)(s_ma * 10u));
        sup_enable_set(SUP_EN_BENCH);
#if defined(TCU_M4B_OUTPUTS)
    } else {                             /* BK_PARK_OUT */
        bench_channels_off();
        if (s_ch == 0u) {
            sol_set(SOL_CH_PARK_RELEASE, SOL_MODE_FULL_ON, 0u);
            plat_toe_out_set(0u);
        } else {
            plat_toe_out_set(s_duty);
        }
        sup_enable_set(SUP_EN_BENCH);
#endif
    }
}

static int sol_start(const uint8_t *in, uint16_t n)
{
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
    s_kind = BK_SOL;
    s_ch = in[0];
    s_ma = ma;
    s_left_10ms = (uint16_t)(in[2] * 100u);
    s_reason = BENCH_END_NONE;
    s_active = true;
    return 0;
}

#if defined(TCU_M4B_OUTPUTS)
static int park_out_start(const uint8_t *in, uint16_t n)
{
    if (n != 2u && n != 3u) {
        return -NRC_IMLOIF;
    }
    s_duty = 1000u;
    if (n == 3u) {
        if (in[2] == 0u || in[2] > 100u) {
            return -NRC_ROOR;
        }
        s_duty = (uint16_t)(in[2] * 10u);           /* percent -> permille */
    }
    if (in[0] > 1u || in[1] == 0u || in[1] > BENCH_MAX_S) {
        return -NRC_ROOR;
    }
    if (s_active || engine_running() || !drivers_healthy() || !supervisor_healthy()) {
        return -NRC_CNC;
    }
    s_kind = BK_PARK_OUT;
    s_ch = in[0];
    s_left_10ms = (uint16_t)(in[1] * 100u);
    s_reason = BENCH_END_NONE;
    s_active = true;
    return 0;
}
#endif

int bench_routine(uint8_t type, uint16_t rid, const uint8_t *in, uint16_t n, uint8_t *out,
                  uint16_t max)
{
    const bool sol_rid = rid == BENCH_RID;
#if defined(TCU_M4B_OUTPUTS)
    const bool park_rid = rid == BENCH_PARK_RID;
#else
    const bool park_rid = false;
#endif

    if (!sol_rid && !park_rid) {
        return -NRC_ROOR;
    }
    if (type == 1u) {                               /* start */
        if (sol_rid) {
            return sol_start(in, n);
        }
#if defined(TCU_M4B_OUTPUTS)
        return park_out_start(in, n);
#else
        (void)in;
        (void)n;
        return -NRC_SFNS;                           /* park outputs not built in */
#endif
    }
    if (type == 2u) {                               /* stop */
        stop(BENCH_END_STOPPED);
        return 0;
    }
    if (max < 7u) {                                 /* results */
        return -NRC_IMLOIF;
    }
    out[0] = s_active ? 1u : 0u;
    if (park_rid) {
        out[1] = s_kind == BK_PARK_OUT ? s_ch : 0xFFu;
        out[2] = (uint8_t)(s_duty >> 8);
        out[3] = (uint8_t)s_duty;
    } else {
        out[1] = s_ch;
        out[2] = (uint8_t)(s_ma >> 8);
        out[3] = (uint8_t)s_ma;
    }
    out[4] = (uint8_t)(s_left_10ms / 10u > 255u ? 255u : s_left_10ms / 10u);
    out[5] = s_reason;
    out[6] = sol_status()->faults;
    return 7;
}
