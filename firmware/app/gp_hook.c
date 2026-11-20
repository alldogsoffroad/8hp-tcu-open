/* gp_hook.c — the gear-pair shadow's live reads (app/gp_shadow.h). NOT an
 * output path: it reads the live inputs BY VALUE into gp_in_t and hands them
 * to the read-only shadow; it writes nothing live.
 *
 *   gp_hook_init   main.c, in the shadow init block (after sm_hook_init)
 *   gp_hook_10ms   main.c's task_10ms, right after sm_hook_10ms (the stock's
 *                  slots 30..35 follow the engine and the monitor)
 *   gp_hook_feed   the part after the gather (also the host harness's entry)
 *
 * Reads (const volatile, pinned live cells): the gear chain's inputs exactly
 * as sm_hook_10ms gathers them (E69B / E6EE / E6EF / E28E, 0x89C3D, the OS
 * mode, 0x89CD2, the live E4A7 / E69F; our output / turbine / engine
 * speeds), the live shift object 0xFFF8BD15 / BD16 / BD17 / BD41 (+0x2D),
 * 0xFFF8D151 (0xEEB04) and 0xFFF8D0E0; the r04w chain's E4A7 / E69F
 * (sm_gear_state, a read-only getter). It never reads a cell the shadow
 * models as its own.
 */
#include "app/gp_shadow.h"

#include <stdint.h>

#if !defined(TCU_RAM_APP)

#define LIVE_U8(a)   (*(const volatile uint8_t *)(uintptr_t)(a))

void gp_hook_init(void)
{
    gp_shadow_init();
    gp_gear_init();
}

#if !defined(GP_HOOK_NO_GATHER)
#include "io/sensors/sensors.h"
#include "io/vehicle/vehicle.h"
#include "platform/platform.h"

void gp_hook_10ms(void)
{
    const sensors_in_t *sn = sensors_in();
    const vehicle_in_t *v = vehicle_in();
    const volatile gf_t *r = sm_gear_state();
    gp_in_t in;
    in.gf.e69b = LIVE_U8(0xFFF8E69Bu);
    in.gf.e6ee = LIVE_U8(0xFFF8E6EEu);
    in.gf.e6ef = LIVE_U8(0xFFF8E6EFu);
    in.gf.e28e = LIVE_U8(0xFFF8E28Eu);
    in.gf.c3d = LIVE_U8(0xFFF89C3Du);
    in.gf.os_mode = LIVE_U8(0xFFF93B33u);
    in.gf.cd2 = LIVE_U8(0xFFF89CD2u);
    in.gf.live_e4a7 = (int8_t)LIVE_U8(0xFFF8E4A7u);
    in.gf.live_e69f = (int8_t)LIVE_U8(0xFFF8E69Fu);
    in.gf.pad9 = 0u;
    in.gf.cb8 = (sn->q_output_speed == Q_OK && sn->output_speed > 0) ? (uint16_t)sn->output_speed : 0u;
    in.gf.cba = (sn->q_turbine_speed == Q_OK && sn->turbine_speed > 0) ? (uint16_t)sn->turbine_speed : 0u;
    in.gf.cd0 = (v->q_engine_speed == Q_OK) ? (uint16_t)v->engine_speed : 0u;
    in.lbd15 = LIVE_U8(0xFFF8BD15u);
    in.lbd16 = LIVE_U8(0xFFF8BD16u);
    in.lbd17 = LIVE_U8(0xFFF8BD17u);
    in.lbd41 = LIVE_U8(0xFFF8BD41u);
    in.ld151 = LIVE_U8(0xFFF8D151u);
    in.ld0e0 = LIVE_U8(0xFFF8D0E0u);
    in.r_e4a7 = r->e4a7;
    in.r_e69f = r->e69f;
    const uint32_t t0 = plat_frt_us();
    gp_hook_feed(&in);
    const uint32_t dt = plat_frt_us() - t0;
    gp_hook_time(dt);
}
#endif

void gp_hook_feed(const gp_in_t *in)
{
    gp_shadow_pass(in);
}
#endif /* !TCU_RAM_APP */
