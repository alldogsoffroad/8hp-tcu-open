/* wk_hook.c - the torque-converter lock-up shadow's live reads (app/wk_shadow.h). NOT an output
 * path: it reads the live inputs BY VALUE into wk_in_t and hands them to the read-only shadow;
 * it writes nothing live and transmits nothing.
 *
 *   wk_hook_init   main.c, in the shadow init block (after lp_hook_init)
 *   wk_hook_10ms   main.c's task_10ms, right after lp_hook_10ms (the stock's slots 37..39 follow the
 *                  gear-pair and line-pressure slots)
 *   wk_hook_feed   the part after the gather (also the host harness's entry)
 *
 * Reads (const volatile, pinned live cells): the OS mode 0x93B33, the mode-1 flag 0x89C3D, the engine's
 * power flow E69B / E6EE, the conditioned temperature byte 0x89C13, the live shift object's BD16 (a
 * shift in progress; the gear-pair shadow's when the live one is idle); the fault shadow's reactions
 * 49 / 42 (0x89CF6), the line-pressure shadow's target (E61A), the live WK current (witness only). The
 * speeds and the torque: our sensors and vehicle input, REPLACED while the bench's simulated-speed frame
 * is fresh (io/vehicle/shadow_sim.h; the shadows only). The strategy's lock-up request index
 * (0xFFF8A0A1) is produced by the shift-map shadow (sm_shadow.c, 0x124570; gaps3_port.md); the host harness
 * keeps a seam (wk_shadow_host_req) that overrides it. It never reads a cell the shadow models as its own. */
#include "app/wk_shadow.h"

#include <stdint.h>

#if !defined(TCU_RAM_APP)

#include "io/vehicle/shadow_sim.h"   /* BENCH-ONLY simulated speeds (feeds the shadows only) */

#define LIVE_U8(a)   (*(const volatile uint8_t *)(uintptr_t)(a))
#define LIVE_U16(a)  (*(const volatile uint16_t *)(uintptr_t)(a))

#if !defined(__sh__)
static uint8_t s_host_req;
static uint8_t s_host_req_set;                           /* the host seam overrides the producer once it has been set */
void wk_shadow_host_req(uint8_t v) { s_host_req = v; s_host_req_set = 1u; }
uint8_t wk_shadow_host_req_get(void) { return s_host_req; }
#endif

void wk_hook_init(void)
{
    wk_shadow_init();
}

#if !defined(WK_HOOK_NO_GATHER)
#include "app/fr_shadow.h"
#include "app/gp_shadow.h"
#include "app/sm_shadow.h"
#include "app/lp_shadow.h"
#include "io/sensors/sensors.h"
#include "io/vehicle/vehicle.h"
#include "platform/platform.h"
#include "bsw/sched/sched.h"

void wk_hook_10ms(void)
{
    const sensors_in_t *sn = sensors_in();
    const vehicle_in_t *v = vehicle_in();
    const volatile gp_t *p = gp_shadow_state();
    shadow_sim_in_t ss;
    wk_in_t in;
    uint8_t bd16 = LIVE_U8(0xFFF8BD16u);
    uint32_t eng;
    uint32_t turb;
    int32_t tq;
    uint32_t t0;
    if (bd16 == 0u) {                                    /* a gear-pair shift: the gear-pair shadow's object */
        bd16 = p->bd16;
    }
    in.os_mode = LIVE_U8(0xFFF93B33u);
    in.c3d = LIVE_U8(0xFFF89C3Du);
    in.c2f = LIVE_U8(0xFFF8E6EEu);
    in.c30 = LIVE_U8(0xFFF8E69Bu);
    in.cf6 = (uint8_t)((fr_fm_react(49u) != 0u || fr_fm_react(42u) != 0u) ? 1u : 0u);   /* 0x89CF6 = reaction 49 or 42 */
    in.e612_a = 0u;                                      /* 0xBA2A4(E612): stand-in */
    in.e612_b = 1u;                                      /* 0xBA2AC(E612): stand-in */
    in.e61c_busy = 0u;
    /* 0xFFF8A0A1 = the lock-up request index: the shift-map shadow's producer (sm_shadow.c lu_pass, 0x124570); the
     * host seam overrides it once set. On the bench reaction 49 holds the STOCK's request at 0; the shadow's
     * producer runs the healthy case (react 49 = 0), the WK gate (0x89CF6 = reaction 49 / 42) stays shut. */
#if !defined(__sh__)
    in.req_idx = s_host_req_set ? s_host_req : sm_lock_request();
#else
    in.req_idx = sm_lock_request();
#endif
    in.req_force0 = 0u;
    in.e353 = 0u;
    in.shift_busy = (uint8_t)((bd16 != 0u) ? 1u : 0u);
    in.range_ok = 1u;
    in.inhibit_x = 0u;
    in.temp_byte = LIVE_U8(0xFFF89C13u);
    in.pad15 = 0u;
    in.temp_c = (int16_t)((int32_t)in.temp_byte - 40);
    eng = (v->q_engine_speed == Q_OK) ? (uint32_t)v->engine_speed : 0u;
    turb = (sn->q_turbine_speed == Q_OK && sn->turbine_speed > 0) ? (uint32_t)sn->turbine_speed : 0u;
    tq = (v->q_torque_actual == Q_OK) ? (int32_t)(v->torque_actual / 10) : 0;
    shadow_sim_get(&ss);
    if (ss.valid != 0u) {                                /* the bench's simulated frame (shadows only) */
        turb = ss.turbine_rpm;
        tq = ss.torque_nm;
        if (eng == 0u) {
            eng = turb + 60u;                            /* no simulated engine speed: a 60 rpm slip [stand-in] */
        }
    }
    in.eng = (uint16_t)((eng > 0xFFFFu) ? 0xFFFFu : eng);
    in.turb = (uint16_t)((turb > 0xFFFFu) ? 0xFFFFu : turb);
    in.tq = (int16_t)tq;
    in.line = lp_shadow_state()->res;                    /* E61A: the line-pressure shadow's target */
    in.now_ms = (uint16_t)sched_now_ms();
    in.live_wk = LIVE_U16(0xFFF94ED4u + 10u);            /* the live law's WK current (channel 5) */
    in.pad30 = 0u;
    t0 = plat_frt_us();
    wk_hook_feed(&in);
    wk_shadow_note_us(plat_frt_us() - t0);
}
#endif

void wk_hook_feed(const wk_in_t *in)
{
    wk_shadow_pass(in);
}
#endif /* !TCU_RAM_APP */
