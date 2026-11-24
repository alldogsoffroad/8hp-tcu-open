/* nissan_lp.c - THE NISSAN LINE-PRESSURE LIVE SEAM (app/nissan_lp.h; Nissan build only).
 * NOT an output path and not a shadow: it READS the healthy line-pressure shadow's result
 * (app/lp_shadow.h), the fault-reaction shadow's reaction 50 / 0x89CF5 (app/fr_shadow.h) and
 * the vehicle's actual-torque quality (io/vehicle/vehicle.h) through their read getters, and
 * returns one value BY VALUE. It writes nothing - no RAM of its own (stateless), no live cell,
 * no transmit path. The monitor's seam (app/gs_bridge.c drv_gsbr_take, channel 6) is its one
 * caller; the request law's guard (bsw/drv/drv_reqlaw.c) bounds what reaches the wire. */
#include "app/nissan_lp.h"
#include "app/lp_shadow.h"
#include "app/fr_shadow.h"
#include "io/vehicle/vehicle.h"

#if !defined(__sh__)
static uint16_t s_host_force;
void nissan_lp_host_force(uint16_t v) { s_host_force = v; }
#endif

uint8_t nissan_lp_sys_gate(void)
{
    const volatile lp_t *lp = lp_shadow_state();
    const volatile fr_t *fr = fr_shadow_state();
    const vehicle_in_t *v = vehicle_in();
    uint8_t g = 0u;
    const uint8_t m = lp->in.os_mode;
    if (lp->w.pass_n == 0u || m < 1u || m > 3u || lp->cf5 != 0u) {
        g = (uint8_t)(g | NLP_G_SHADOW);
    }
    if (v->q_torque_actual != Q_OK) {
        g = (uint8_t)(g | NLP_G_TORQUE);
    }
    if (fr->w.mon_passes == 0u) {
        g = (uint8_t)(g | NLP_G_FR);
    }
    if (fr_fm_react((uint8_t)NLP_R50) != 0u || fr->cd.cf5 != 0u) {
        g = (uint8_t)(g | NLP_G_R50);
    }
    return g;
}

uint16_t nissan_lp_sys_request(void)
{
    uint16_t i;
#if !defined(__sh__)
    if (s_host_force != 0u) {
        return s_host_force;                     /* host chain test: the plumbing, gates bypassed */
    }
#endif
    if (nissan_lp_sys_gate() != 0u) {
        return NLP_SYS_SAFE;
    }
    i = lp_shadow_state()->sys_i;
    if (i < NLP_SYS_MIN) {
        i = NLP_SYS_MIN;
    } else if (i > NLP_SYS_MAX) {
        i = NLP_SYS_MAX;
    }
    return i;
}
