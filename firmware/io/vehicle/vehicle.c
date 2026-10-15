/* Profile-independent helpers of the vehicle input interface. */
#include "io/vehicle/vehicle.h"

uint16_t vehicle_quality_bits(const vehicle_in_t *v)
{
    const quality_t q[12] = {
        v->q_engine_speed, v->q_torque_actual, v->q_torque_t2, v->q_torque_t3, v->q_pedal,
        v->q_engine_temp, v->q_brake, v->q_engine_mode, v->q_park_speed, v->q_soak,
        v->q_park_request, v->q_lever,
    };
    uint16_t bits = 0u;

    for (uint8_t i = 0u; i < 12u; i++) {
        bits = (uint16_t)(bits | (q[i] == Q_OK ? 1u << i : 0u));
    }
    return bits;
}
