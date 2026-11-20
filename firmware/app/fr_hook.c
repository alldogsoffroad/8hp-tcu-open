/* fr_hook.c — the fault-reaction shadow's live reads (app/fr_shadow.h; F1,
 * 2026-11-19). NOT an output path: it reads the live inputs BY VALUE into
 * fr_in_t and hands them, and the raw CAN frames, to the read-only shadow;
 * it writes nothing live and has no RAM of its own.
 *
 *   fr_hook_init   main.c, in the shadow init block
 *   fr_hook_rx     main.c's 1 ms CAN drain: every received frame, BEFORE our
 *                  com layer judges it (vehicle_rx) — D-FR-8
 *   fr_hook_10ms   main.c's task_10ms, after the engine and the monitor and
 *                  BEFORE the gear feedback chain (sm_hook_10ms): the stock's
 *                  slot 27 (the code) precedes slot 30 (the hinge)
 *   fr_hook_feed   the part after the gather (also the host harness's entry)
 *
 * Inputs: the OS mode 0x93B33; kl15's E9FE / E9FF; the live range D6D6
 * (app/park_cells.h's read-only PCELL_TGT); the engine's E69B / E6EE / E6EF and
 * 0x89C3D (pinned); 0x89CD2; the supply 0x94F86; our output / turbine
 * sensors and the 0x108 engine speed (with their validity); rng_chain's
 * group-109 / -98 report counters and arguments and its U1267 / P0850
 * verdicts (rng_chain_model, the comparator); park_pair's last group-98
 * report; the healthy gear chain's state (sm_gear_state) and the shift-map
 * shadow's 0x89CE8 (sm_shadow_state) for the fault-fed hinge; for groups
 * 27 / 28 (the TOE32 test, 2026-11-20) the AN0 mux ring's slots 35 / 40 in
 * both halves (0xFFF885E8 / 0xFFF88688), TOE32's applied duty 0xFFF90874[3],
 * the sequencer's 0xFFF95005 and 0x849D4's 0xFFF94110. It never reads
 * a cell the shadow models (the 0x89C41..44 / 0x89CED / 0x89CF5 / AE2D cells,
 * the comm object 0xFFF95750.., the DSM arrays 0xFFF8D714 / D89A / DBA6 /
 * DD2C, the masks 0xFFF897D0..E7, the RX states 0xFFF84020..447F, the TOE32
 * test's 0x94828 / 0x9482D / 0x94148 / 0x93A7A / 0x938F6 / 0x93900 / 0x940D4).
 */
#include "app/fr_shadow.h"

#include <stdint.h>

#if !defined(TCU_RAM_APP)

#define LIVE_U8(a)   (*(const volatile uint8_t *)(uintptr_t)(a))
#define LIVE_U16(a)  (*(const volatile uint16_t *)(uintptr_t)(a))

void fr_hook_init(void)
{
    fr_shadow_init();
}

void fr_hook_rx(uint8_t bus, uint16_t id, uint8_t dlc, const uint8_t *data)
{
    fr_shadow_rx(bus, id, dlc, data);
}

#if !defined(FR_HOOK_NO_GATHER)
#include "app/park_cells.h"
#include "app/rng_chain.h"
#include "app/sm_shadow.h"
#include "io/sensors/sensors.h"
#include "io/vehicle/vehicle.h"
#include "platform/platform.h"
#include "safety/kl15.h"
#include "safety/park_pair.h"

void fr_hook_10ms(void)
{
    const sensors_in_t *sn = sensors_in();
    const vehicle_in_t *v = vehicle_in();
    const rng_chain_model_t *rm = rng_chain_model();
    const volatile gf_t *gf = sm_gear_state();
    park_pair_wit_t pw;
    fr_in_t in;
    park_pair_witness(&pw);
    in.t_us = plat_frt_us();
    in.os_mode = LIVE_U8(0xFFF93B33u);
    in.e9fe = kl15_e9fe();
    in.e9ff = kl15_e9ff();
    in.d6d6 = PCELL_TGT;                    /* the live range D6D6 (read-only, the writer guard) */
    in.d6da = 0u;                           /* obj+0x1A: read by nothing ported */
    in.e69b = LIVE_U8(0xFFF8E69Bu);
    in.e6ee = LIVE_U8(0xFFF8E6EEu);
    in.e6ef = LIVE_U8(0xFFF8E6EFu);
    in.c3d = LIVE_U8(0xFFF89C3Du);
    in.u_failed = rm->u_failed;
    in.u_tested = rm->u_tested;
    in.rng_g109 = rm->g109;
    in.rng_g98 = rm->g98;
    in.eng_st = (v->q_engine_speed == Q_OK) ? 0u : 2u;
    in.out_st = (sn->q_output_speed == Q_OK) ? 0u : 2u;
    in.gf_e4a5 = gf->e4a5;
    in.gf_e69c1 = gf->e69c[1];
    in.gf_e69f = gf->e69f;
    in.gf_e343 = gf->e343;
    in.gf_cce4 = gf->cce4[0];
    in.gf_c25 = gf->c25[0];
    in.gf_c28 = gf->c28[0];
    in.gf_c2b = gf->c2b[0];
    in.gf_e6a1 = gf->e6a1;
    in.ce8 = sm_shadow_state()->ce8;
    in.cd2 = LIVE_U8(0xFFF89CD2u);
    in.g98_failed = pw.failed;
    in.g98_tested = pw.tested;
    in.pad[0] = 0u;
    in.pad[1] = 0u;
    in.pad[2] = 0u;
    in.rep109_n = rm->u_rep_n;
    in.rep98_n = rm->fm98.rep_n;
    in.sup_mv = LIVE_U16(0xFFF94F86u);
    in.d646 = (sn->q_output_speed == Q_OK && sn->output_speed > 0) ? (uint16_t)sn->output_speed : 0u;
    in.eng = (v->q_engine_speed == Q_OK) ? (uint16_t)v->engine_speed : 0u;
    in.cb8 = in.d646;
    in.cba = (sn->q_turbine_speed == Q_OK && sn->turbine_speed > 0) ? (uint16_t)sn->turbine_speed : 0u;
    in.cd0 = in.eng;
    {
        /* groups 27 / 28 (the TOE32 test): the AN0 mux ring's slots 35 / 40
         * (mux input 4 = PL7) in both halves (.adc_ring 0xFFF885E8, 2 x 80
         * words; the A-DMAC writes them, we only read), TOE32's applied duty
         * 0xFFF90874[3] (drv_groupa's 0x91500), the sequencer's pre-'L'
         * window 0xFFF95005, 0x849D4's first-pass flag 0xFFF94110 */
        fr_t3_in_t t3;
        t3.r35[0] = LIVE_U16(0xFFF885E8u + 0x46u);
        t3.r35[1] = LIVE_U16(0xFFF88688u + 0x46u);
        t3.r40[0] = LIVE_U16(0xFFF885E8u + 0x50u);
        t3.r40[1] = LIVE_U16(0xFFF88688u + 0x50u);
        t3.duty = LIVE_U16(0xFFF90874u + 6u);
        t3.x95005 = LIVE_U8(0xFFF95005u);
        t3.x94110 = LIVE_U8(0xFFF94110u);
        fr_hook_feed_t3(&t3);
    }
    fr_hook_feed(&in);
    fr_shadow_time(plat_frt_us() - in.t_us);
}
#endif

void fr_hook_feed(const fr_in_t *in)
{
    fr_shadow_tick_10ms(in);
}

void fr_hook_feed_t3(const fr_t3_in_t *in)
{
    fr_shadow_t3_in(in);
}

#endif /* !TCU_RAM_APP */
