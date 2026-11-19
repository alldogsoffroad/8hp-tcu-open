/* sm_hook.c — the shift-map shadow's live reads (app/sm_shadow.h). NOT an
 * output path: it reads the live inputs BY VALUE into sm_in_t and hands them
 * to the read-only shadow; it writes nothing live.
 *
 *   sm_hook_init   main.c, in the shadow init block
 *   sm_hook_30ms   main.c's tmp_chain_hook_30ms (app/tmp_chain.c's 30 ms slot,
 *                  modes 1..3, before 0xB9646 = entry 19 — the shift target's
 *                  entries 5 / 14 / 15 / 16 precede it in the list 0x4D940)
 *   sm_hook_feed   the part after the gather (also the host harness's entry)
 *
 * Inputs (stand-ins stated in sm_shadow.h):
 *   D646   our output speed (TIF12, 40 ppr, sensors_in) when Q_OK and > 0,
 *          else 0 — the stock's D646 decay / substitute rules not ported;
 *   pedal  0x120 byte 2 (vehicle_in: permil = raw × 4): the stock's
 *          conditioning raw × 127 / 125, ≤ 254 [I: spec 07 §7.1, truncating];
 *          no valid 0x120 → the dump's 23;
 *   gear   0x89C28 ← our engine's compile-time engaged gear (GS_C25 = 3, the
 *          dump's 03).
 */
#include "app/sm_shadow.h"

#include <stdint.h>

#if !defined(TCU_RAM_APP)

#define LIVE_U8(a)   (*(const volatile uint8_t *)(uintptr_t)(a))

void sm_hook_init(void)
{
    sm_shadow_init();
}

#if !defined(SM_HOOK_NO_GATHER)
#include "io/sensors/sensors.h"
#include "io/vehicle/vehicle.h"
#include "platform/platform.h"

void sm_hook_30ms(void)
{
    const sensors_in_t *sn = sensors_in();
    const vehicle_in_t *v = vehicle_in();
    const uint8_t qs = (sn->q_output_speed == Q_OK && sn->output_speed > 0) ? 1u : 0u;
    sm_hook_feed(qs ? (uint16_t)sn->output_speed : 0u, qs,
                 (uint16_t)v->pedal, (v->q_pedal == Q_OK) ? 1u : 0u,
                 LIVE_U8(0xFFF93B33u));                         /* 0x93B33 */
}
#endif

#if defined(SM_HOOK_NO_GATHER)
static uint32_t frt_us(void) { return 0u; }
#else
static uint32_t frt_us(void) { return plat_frt_us(); }
#endif

void sm_hook_feed(uint16_t rpm, uint8_t q_speed, uint16_t pedal_permil, uint8_t q_pedal,
                  uint8_t os_mode)
{
    sm_in_t in;
    uint32_t ped = SM_SI_PEDAL;
    if (q_pedal) {
        ped = ((uint32_t)(pedal_permil / 4u) * 127u) / 125u;
        if (ped > 254u) {
            ped = 254u;
        }
    }
    in.t_us = frt_us();
    in.d646 = rpm;
    in.pedal = (uint8_t)ped;
    in.gear_in = (int8_t)SM_SI_GEAR;
    in.os_mode = os_mode;
    in.q_speed = q_speed;
    in.q_pedal = q_pedal;
    in.pad = 0u;
    sm_shadow_pass(&in);
}
#endif /* !TCU_RAM_APP */
