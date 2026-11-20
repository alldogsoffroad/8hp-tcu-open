/* lp_hook.c - the line-pressure / torque-intervention shadows' live reads
 * (app/lp_shadow.h, app/ti_shadow.h). NOT an output path: it reads the live
 * inputs BY VALUE into lp_in_t / ti_in_t and hands them to the read-only shadows;
 * it writes nothing live and transmits nothing.
 *
 *   lp_hook_init   main.c, in the shadow init block (after gp_hook_init)
 *   lp_hook_10ms   main.c's task_10ms, right after gp_hook_10ms
 *   lp_hook_feed   the part after the gather (also the host harness's entry)
 *
 * Reads (const volatile, pinned live cells): the engine's power flow E69B / E6EE,
 * the live shift object 0xFFF8BD15 / BD16 / BD1A / BD72 / BD74 / BD8B, the garage
 * sub-objects' +0x16 / +0x17 / +0x20 (D0F2.. D1D4), the conditioned temperature bytes
 * 0x89C13 / 0x89CD2, the range chain's D6D6 (PCELL_TGT), the park position 0xFFF8EA40, the live
 * law's Sys current (witness only); the gear chain's cells come from the coupled
 * shadow chain (gp_gear_state) and the gear-pair shadow (gp_shadow_state), read-only
 * getters. It never reads a cell the shadows model as their own. */
#include "app/lp_shadow.h"
#include "app/ti_shadow.h"

#include <stdint.h>

#if !defined(TCU_RAM_APP)

#include "app/gp_shadow.h"
#include "app/park_cells.h"

#define LIVE_U8(a)   (*(const volatile uint8_t *)(uintptr_t)(a))
#define LIVE_U16(a)  (*(const volatile uint16_t *)(uintptr_t)(a))

void lp_hook_init(void)
{
    lp_shadow_init();
    ti_shadow_init();
}

#if !defined(LP_HOOK_NO_GATHER)
#include "io/sensors/sensors.h"
#include "io/vehicle/vehicle.h"
#include "bsw/sched/sched.h"

static int16_t sat16_rpm(int32_t v)
{
    return (v > 32767) ? (int16_t)32767 : ((v < -32768) ? (int16_t)-32768 : (int16_t)v);
}

static int16_t nm_of(nm10_t v, quality_t q)
{
    return (q == Q_OK) ? (int16_t)(v / 10) : (int16_t)0;
}

void lp_hook_10ms(void)
{
    const sensors_in_t *sn = sensors_in();
    const vehicle_in_t *v = vehicle_in();
    const volatile gf_t *g = gp_gear_state();
    const volatile gp_t *p = gp_shadow_state();
    lp_in_t in;
    ti_in_t ti;
    uint8_t bd16 = LIVE_U8(0xFFF8BD16u);
    uint8_t bd15 = LIVE_U8(0xFFF8BD15u);
    uint8_t bd1a = LIVE_U8(0xFFF8BD1Au);
    uint32_t eng;
    in.c25 = g->c25[0];
    in.c26 = (uint8_t)g->c25[1];
    in.c27 = g->c25[2];
    in.c2b = g->c2b[0];
    in.c2c = (uint8_t)g->c2b[1];
    in.c2d = g->c2b[2];
    in.c2f = LIVE_U8(0xFFF8E6EEu);
    in.c30 = LIVE_U8(0xFFF8E69Bu);
    if (bd16 == 0u) {                                    /* a gear-pair shift: the gear-pair shadow's object */
        bd16 = p->bd16;
        bd15 = p->bd15;
        bd1a = p->bd1a;
    }
    in.bd15 = bd15;
    in.bd16 = bd16;
    in.bd8b = LIVE_U8(0xFFF8BD8Bu);
    in.bd72 = LIVE_U8(0xFFF8BD72u);
    in.bd74 = LIVE_U8(0xFFF8BD74u);
    in.fill_any = (uint8_t)((LIVE_U8(0xFFF8D0F3u) != 0u || LIVE_U8(0xFFF8D15Fu) != 0u || LIVE_U8(0xFFF8D1CBu) != 0u) ? 1u : 0u);
    /* 0x106988: D0F2 != 0 and not (D148 / D1B4: +0x16 == 0 and +0x20 != 0) */
    in.bphase4 = (uint8_t)((LIVE_U8(0xFFF8D0F2u) == 0u ||
                            (LIVE_U8(0xFFF8D15Eu) == 0u && LIVE_U8(0xFFF8D168u) != 0u) ||
                            (LIVE_U8(0xFFF8D1CAu) == 0u && LIVE_U8(0xFFF8D1D4u) != 0u)) ? 0u : 1u);
    in.cf4 = 1u;                                         /* 0x89CF4 (0xFFF8A0A2): the dump's value, writer undecoded */
    eng = (v->q_engine_speed == Q_OK) ? (uint32_t)v->engine_speed : 0u;
    in.d10 = (uint8_t)((v->q_engine_speed == Q_OK && eng < 380u) ? 1u : 0u);   /* 0x89D10 [I]: the engine-state machine's stopped state */
    in.temp = LIVE_U8(0xFFF89C13u);
    in.cd2 = LIVE_U8(0xFFF89CD2u);
    in.range = PCELL_TGT;                                /* the live range D6D6 (read-only, the writer guard) */
    in.pos = LIVE_U8(0xFFF8EA40u);
    in.d1d = 0x9Eu;                                      /* 0x89D1D: the dump's value (the curve it feeds is flat 15000) */
    in.c18 = 0x4Bu;                                      /* 0x89C18: the dump's value (the conditioning map is flat 100 %) */
    in.os_mode = LIVE_U8(0xFFF93B33u);
    in.eng = (uint16_t)((eng > 0xFFFFu) ? 0xFFFFu : eng);
    in.out = (sn->q_output_speed == Q_OK && sn->output_speed > 0) ? (uint16_t)sn->output_speed : 0u;
    in.now_ms = (uint16_t)sched_now_ms();
    if (in.eng != 0u && sn->q_turbine_speed == Q_OK && sn->turbine_speed > 0) {
        const uint32_t r = ((uint32_t)sn->turbine_speed * 1000u) / in.eng;
        in.spd_ratio = (uint16_t)((r > 1000u) ? 1000u : r);
    } else {
        in.spd_ratio = 0u;
    }
    in.t1 = nm_of(v->torque_actual, v->q_torque_actual);
    in.t2 = nm_of(v->torque_t2, v->q_torque_t2);
    in.t3 = nm_of(v->torque_t3, v->q_torque_t3);
    in.ca0 = in.t1;                                      /* 0x89CA0 [I]: as the gear-pair hook's input torque stand-in */
    in.live_sys = LIVE_U16(0xFFF94ED4u + 12u);           /* the live law's Sys current (channel 6) */
    in.pad = 0u;
    lp_hook_feed(&in);
    ti.os_mode = in.os_mode;
    ti.bd16 = bd16;
    ti.bd17 = p->bd17;
    ti.bd18 = p->bd18;
    ti.bd1a = bd1a;
    ti.bd1b = p->bd1b;
    ti.bd22 = p->bd22;
    ti.pf = in.c30;
    ti.t3 = in.t3;
    ti.tq = gp_shadow_x()->in2.tq;                       /* 0x89CA0 / 0x89C1A: the input torque, Nm (the gear-pair hook's stand-in) */
    ti.turb = (sn->q_turbine_speed == Q_OK) ? sat16_rpm(sn->turbine_speed) : (int16_t)0;
    ti.eng = (int16_t)((in.eng > 32767u) ? 32767 : (int16_t)in.eng);
    {
        const int32_t out = (int32_t)in.out;
        const int32_t rto = (int32_t)gp_cpl_ratio((int8_t)in.c27);
        const int32_t rfr = (int32_t)gp_cpl_ratio((int8_t)in.c2d);
        ti.sync_to = sat16_rpm((out * rto) / 1000);                       /* ctx+0x2A */
        ti.bd80 = sat16_rpm((int32_t)ti.turb - (out * rfr) / 1000);       /* 0x118082: turbine - output x ratio(old gear) / 1000 */
    }
    ti.bd76 = gp_shadow_ph()->blk[0].bd76;
    ti.now_ms = in.now_ms;
    ti.c2d = (uint8_t)in.c2d;
    ti.c27 = (uint8_t)in.c27;
    ti.c26 = in.c26;
    ti.k7 = gp_shadow_x()->blk[0].k7;
    ti.ktype = p->a.kind;
    ti.temp = in.temp;
    ti.on_left_fill = (uint8_t)((p->a.on.phase >= 4u) ? 1u : 0u);        /* the phase-9 exit stand-in: the on-coming object left its fill */
    ti.b5e1 = 0u;                                        /* 0xFFF8B5E1: the lock-up torque raise - BY VALUE hook for the lock-up shadow */
    ti.b5e2 = 0u;                                        /* 0xFFF8B5E2 */
    ti.e330 = 0;                                         /* 0xFFF8E330 (0xFFF8AFA0's term: dump 0, enable +0x34 = 0) */
    ti.sync_a = p->a.sync;
    ti_hook_feed(&ti);
}
#endif

void lp_hook_feed(const lp_in_t *in)
{
    lp_shadow_pass(in);
}

void ti_hook_feed(const ti_in_t *in)
{
    ti_shadow_pass(in);
}
#endif /* !TCU_RAM_APP */
