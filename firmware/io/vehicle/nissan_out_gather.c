/* Nissan application only: gather the TCU state for nissan_out_build() (see nissan_out.h).
 * Reads the read-only shadows' state and the sensor / vehicle inputs BY VALUE; writes nothing
 * but the transmit set's input. Stateless. */
#include "io/vehicle/nissan_out.h"
#include "io/vehicle/nissan_tx.h"
#include "io/vehicle/vehicle.h"
#include "io/sensors/sensors.h"
#include "app/sm_shadow.h"
#include "app/ti_shadow.h"
#include "app/wk_shadow.h"

static uint16_t rpm_or_0(rpm_t v, quality_t q)
{
    return (q == Q_OK && v > 0) ? (uint16_t)v : 0u;
}

void nissan_out_tick_1ms(void)
{
    const volatile gf_t *g = sm_gear_state();
    const volatile ti_t *ti = ti_shadow_state();
    const volatile wk_t *wk = wk_shadow_state();
    const sensors_in_t *sn = sensors_in();
    const vehicle_in_t *v = vehicle_in();
    nissan_out_src_t s;
    vehicle_out_t o;

    s.engaged = (int8_t)(g->c25[1] - 1);
    s.target = g->c25[2];
    s.output_rpm = rpm_or_0(sn->output_speed, sn->q_output_speed);
    s.turbine_rpm = rpm_or_0(sn->turbine_speed, sn->q_turbine_speed);
    s.lockup = wk->closed != 0u;
    s.ti_cls = ti->cls;
    s.ti_type = ti->type;
    s.ti_armed = ti->armed != 0u;
    s.ti_delta_nm = ti->delta;
    s.actual_nm10 = v->torque_actual;
    s.actual_ok = v->q_torque_actual == Q_OK;
    nissan_out_build(&s, &o);
    vehicle_tx_set(&o);
    if (nissan_tx_sent_count() == 0u) {
        (void)nissan_tx_enable(true);              /* until the first frame has gone out (stateless) */
    }
}
