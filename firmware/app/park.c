#include "app/park.h"

#if defined(TCU_M4B_OUTPUTS)
#include "app/bench_out.h"
#include "bsw/sup/sup.h"
#include "io/actuators/sol_drv.h"
#endif

/* Project values, to be measured in M4b. */
static const park_ctl_cfg_t s_cfg = { 500u, 1000u, true };

/* Park hold energisation (permille). First cut: fully on — stock battery-compensates the
 * duty (spec 12 section 1.2); refine from the FE11 / bench measurements. */
#define PARK_HOLD_DUTY 1000u

static park_status_t s_st;

void park_init(void)
{
    range_init();
    park_ctl_init(&s_cfg);
    park_mon_init();
    s_st.target = PARK_ENGAGE;
    s_st.reason = R_NONE;
    s_st.state = PC_INIT;
    s_st.req.release = false;
    s_st.req.hold = false;
    s_st.ctl_faults = 0u;
    s_st.mon_faults = 0u;
#if defined(TCU_M4B_OUTPUTS)
    plat_toe_out_set(0u);
    sol_set(SOL_CH_PARK_RELEASE, SOL_MODE_OFF, 0u);
    sup_enable_clear(SUP_EN_PARK);
#endif
}

/* Output gate (M4b, docs/firmware/01-m4-range-park.md section 4/5): the requests reach
 * channel 7 (park release, full on) and TOE31 (park hold, duty in permille) only while
 * the independent monitor's enable is set and no bench test owns the outputs. park_tick
 * has already forced both requests off when the enable is missing, so this never has to
 * second-guess the requests — only the bench suspension is checked here. */
static void apply(const park_out_t *out)
{
#if defined(TCU_M4B_OUTPUTS)
    if (bench_active()) {                /* commissioning test owns the outputs */
        sol_set(SOL_CH_PARK_RELEASE, SOL_MODE_OFF, 0u);
        plat_toe_out_set(0u);
        sup_enable_clear(SUP_EN_PARK);
        return;
    }
    sol_set(SOL_CH_PARK_RELEASE, out->release ? SOL_MODE_FULL_ON : SOL_MODE_OFF, 0u);
    plat_toe_out_set(out->hold ? PARK_HOLD_DUTY : 0u);
    if (out->release || out->hold) {
        sup_enable_set(SUP_EN_PARK);
    } else {
        sup_enable_clear(SUP_EN_PARK);
    }
#else
    (void)out;                           /* M4a: requests are published only */
#endif
}

void park_tick_10ms(void)
{
    const vehicle_in_t *v = vehicle_in();
    const sensors_in_t *z = sensors_in();
    const range_in_t ri = {
        v->lever, v->q_lever == Q_OK, z->output_speed, z->q_output_speed == Q_OK, false,
    };
    park_ctl_in_t ci;
    park_mon_in_t mi;
    park_out_t out;

    range_update(&ri);

    ci.target = range_target();
    ci.stationary = range_stationary(ri.output_speed, ri.speed_ok);
    ci.engine_running = v->q_engine_speed == Q_OK && v->engine_speed >= PARK_ENGINE_RUNNING_RPM;
    ci.supply_ok = z->q_battery == Q_OK && z->battery >= PARK_SUPPLY_MIN_MV;
    ci.contacts = z->park;
    ci.contacts_ok = z->q_park == Q_OK;
    park_ctl_step(&ci, &out);

    mi.lever = ri.lever;
    mi.lever_ok = ri.lever_ok;
    mi.output_speed = ri.output_speed;
    mi.speed_ok = ri.speed_ok;
    mi.engine_speed = v->engine_speed;
    mi.engine_ok = v->q_engine_speed == Q_OK;
    mi.park_request = ri.park_request;
    mi.contacts = ci.contacts;
    mi.contacts_ok = ci.contacts_ok;
    mi.target = ci.target;
    mi.state = park_ctl_state();
    mi.out = out;
    mi.app_integrity_ok = range_integrity_ok() && park_ctl_integrity_ok();
    park_mon_step(&mi);

    if (!park_mon_enable()) {
        if (park_ctl_state() != PC_SAFE) {
            park_ctl_force_safe();
        }
        out.release = false;
        out.hold = false;
    }
    apply(&out);

    s_st.target = ci.target;
    s_st.reason = range_reason();
    s_st.state = park_ctl_state();
    s_st.req = out;
    s_st.ctl_faults = park_ctl_faults();
    s_st.mon_faults = park_mon_faults();
}

const park_status_t *park_status(void)
{
    return &s_st;
}
