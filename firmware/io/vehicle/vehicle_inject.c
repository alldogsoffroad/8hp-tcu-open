/* TEST-ONLY vehicle input injector: see vehicle_inject.h. Host unit tests only. */
#include "io/vehicle/vehicle_inject.h"

#include <string.h>

static vehicle_in_t s_in;
static uint32_t s_rx_n;

void vehicle_inject_reset(void)
{
    memset(&s_in, 0, sizeof s_in);
    s_in.q_engine_speed = Q_INVALID;
    s_in.q_torque_actual = Q_INVALID;
    s_in.q_torque_t2 = Q_INVALID;
    s_in.q_torque_t3 = Q_INVALID;
    s_in.q_pedal = Q_INVALID;
    s_in.q_engine_temp = Q_INVALID;
    s_in.brake = BRAKE_NA;
    s_in.q_brake = Q_INVALID;
    s_in.q_engine_mode = Q_INVALID;
    s_in.q_park_speed = Q_INVALID;
    s_in.q_soak = Q_INVALID;
    s_in.q_ign_code = Q_INVALID;
    s_in.lever = RANGE_NONE;
    s_in.q_lever = Q_INVALID;
    s_in.lever_src = LEVER_SRC_NONE;
}

void vehicle_inject(const vehicle_in_t *v)
{
    s_in = *v;
}

uint32_t vehicle_inject_rx_count(void)
{
    return s_rx_n;
}

/* ── the profile interface (io/vehicle/vehicle.h) ─────────────────────── */
void vehicle_init(void)
{
    s_rx_n = 0u;
    vehicle_inject_reset();
}

void vehicle_rx(can_bus_t bus, const can_frame_t *f)
{
    (void)bus;
    (void)f;
    s_rx_n++;
}

void vehicle_update_10ms(void)
{
}

const vehicle_in_t *vehicle_in(void)
{
    return &s_in;
}
