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
 *   gear   0x89C28 ← the shadow's OWN gear feedback chain (app/sm_gear.c,
 *          2026-11-19): the commanded gear E4A7 it computes from the
 *          shadow's 0x89CE8 and the live engine's power-flow cells.
 *
 * (2026-11-19, ~/tcu_sweep/gear_feedback_shadow_port.md) the 10 ms gather
 * sm_hook_10ms (main.c's task_10ms, after the engine and the monitor = the
 * stock slots 30..33 follow the commit) reads BY VALUE, through const
 * volatile, the live engine's pinned stock cells E69B / E6EE / E6EF / E28E
 * (the power flow and the exception machine), 0x89C3D, 0x89CD2, the OS mode,
 * and — for the agreement witness only — the live E4A7 / E69F; the speeds
 * as the engine takes them (output / turbine rpm when valid, else 0; the
 * 0x108 engine speed when valid). It never reads a cell the shadow models
 * (CCE4, E4A4..E4A6, E69C..E69E, E6A0 / E6A1, 0x89C25 / 0x89C28 / 0x89C2B,
 * E33C+7, 0x8519F / 0x851A0, AE2A..AE2F).
 */
#include "app/sm_shadow.h"

#include <stdint.h>

#if !defined(TCU_RAM_APP)

#define LIVE_U8(a)   (*(const volatile uint8_t *)(uintptr_t)(a))

void sm_hook_init(void)
{
    sm_shadow_init();
    sm_gear_init();
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

void sm_hook_10ms(void)
{
    const sensors_in_t *sn = sensors_in();
    const vehicle_in_t *v = vehicle_in();
    gf_in_t in;
    in.e69b = LIVE_U8(0xFFF8E69Bu);                     /* the engine's E69B (pinned) */
    in.e6ee = LIVE_U8(0xFFF8E6EEu);                     /* E6EE */
    in.e6ef = LIVE_U8(0xFFF8E6EFu);                     /* E6EF */
    in.e28e = LIVE_U8(0xFFF8E28Eu);                     /* E28E +0 the state */
    in.c3d = LIVE_U8(0xFFF89C3Du);                      /* slot 26's mode-1 flag */
    in.os_mode = LIVE_U8(0xFFF93B33u);
    in.cd2 = LIVE_U8(0xFFF89CD2u);
    in.live_e4a7 = (int8_t)LIVE_U8(0xFFF8E4A7u);        /* witness only */
    in.live_e69f = (int8_t)LIVE_U8(0xFFF8E69Fu);        /* witness only */
    in.pad9 = 0u;
    in.cb8 = (sn->q_output_speed == Q_OK && sn->output_speed > 0) ? (uint16_t)sn->output_speed : 0u;
    in.cba = (sn->q_turbine_speed == Q_OK && sn->turbine_speed > 0) ? (uint16_t)sn->turbine_speed : 0u;
    in.cd0 = (v->q_engine_speed == Q_OK) ? (uint16_t)v->engine_speed : 0u;
    sm_hook_gear_feed(&in);
}
#endif

void sm_hook_gear_feed(const gf_in_t *in)
{
    sm_gear_pass(in);
}

#if defined(SM_HOOK_NO_GATHER)
static uint32_t frt_us(void) { return 0u; }
#else
static uint32_t frt_us(void) { return plat_frt_us(); }
#endif

void sm_hook_feed(uint16_t rpm, uint8_t q_speed, uint16_t pedal_permil, uint8_t q_pedal,
                  uint8_t os_mode)
{
    sm_in_t in;
    /* 0x13CA22: invalid → 23; raw 0xFF (not available) → 23; raw >= 0xFB →
     * 0xFE; else raw · 127 / 125 truncated [O] */
    uint32_t ped = SM_SI_PEDAL;
    if (q_pedal) {
        const uint32_t raw = (uint32_t)(pedal_permil / 4u);
        if (raw == 0xFFu) {
            ped = SM_SI_PEDAL;
        } else if (raw >= 0xFBu) {
            ped = 0xFEu;
        } else {
            ped = (raw * 127u) / 125u;
        }
    }
    in.t_us = frt_us();
    in.d646 = rpm;
    in.pedal = (uint8_t)ped;
    in.gear_in = sm_gear_state()->c28[0];           /* the shadow's 0x89C28 = E4A7 */
    in.os_mode = os_mode;
    in.q_speed = q_speed;
    in.q_pedal = q_pedal;
    in.pad = 0u;
    sm_shadow_pass(&in);
}
#endif /* !TCU_RAM_APP */
