/* fb_hook.c — the feedback shadow's one live read (app/fb_shadow.h). NOT an
 * output path: it reads the live cells BY VALUE into fbs_in_t and hands them
 * to the read-only shadow. r04f: with DRV_OD_FB_REAL it also hands the
 * shadow's outputs to drv_od_fb_real (the diagnosis's five input cells).
 *
 * Called at the stock's 0x7CBA8 slot, right after drv_out_tick_head (the
 * harvest 0x904FA status part + the stand-in) and before the law
 * (sup_batch.c: app_out_head_hook — 5 ms in mode 0, the 10 ms part in modes
 * 1..3), so 0x94ED4 is the previous law pass's current, as the stock's
 * 0x838B0 reads it, and the stand-in's cells are this pass's. */
#include "app/fb_shadow.h"
#include "bsw/drv/drv_odiag.h"     /* r04f: drv_od_fb_real (the real chain feeds the diagnosis) */

#include <stdint.h>

#if !defined(TCU_RAM_APP)
#define LIVE_U8(a)   (*(const volatile uint8_t  *)(uintptr_t)(a))
#define LIVE_U16(a)  (*(const volatile uint16_t *)(uintptr_t)(a))
#define LIVE_S16(a)  (*(const volatile int16_t  *)(uintptr_t)(a))

void fb_shadow_gather_tick(void);

void fb_shadow_gather_tick(void)
{
    fbs_in_t in;
    in.n5 = (uint8_t)((LIVE_U8(0xFFF93B33u) == 0u) ? 1u : 2u);  /* OS mode 0x93B33 */
    in.rail_mv = LIVE_U16(0xFFF94ED0u);                         /* 0x94ED0[0] */
    in.t_fluid = LIVE_S16(0xFFF94F9Cu);                         /* 0x94F9C */
    in.t_st = LIVE_U8(0xFFF94FF0u);                             /* 0x94FF0 */
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        in.i_req[ch] = LIVE_S16(0xFFF94ED4u + 2u * ch);         /* 0x94ED4 */
        in.si_cls[ch] = LIVE_U8(0xFFF93A2Eu + ch);              /* the stand-in's */
        in.si_duty[ch] = LIVE_U16(0xFFF9340Cu + 2u * ch);
        in.si_est[ch] = LIVE_U16(0xFFF93394u + 2u * ch);
        in.si_res[ch] = LIVE_U16(0xFFF933A8u + 2u * ch);
        in.mode[ch] = LIVE_U8(0xFFF94FD8u + ch);               /* 0x94FD8 (recorder) */
    }
    /* the disagreement recorder's context (witness only) */
    in.seq2 = LIVE_U8(0xFFF94FD0u);                             /* 0x94FD0[0] */
    in.os_mode = LIVE_U8(0xFFF93B33u);                          /* 0x93B33 */
    in.phase = LIVE_U16(0xFFF94FE2u);                           /* 0x94FE2 */
    in.f114 = LIVE_U8(0xFFF94114u);                             /* 0x94114 */
    in.f115 = LIVE_U8(0xFFF94115u);                             /* 0x94115 */
    fb_shadow_tick(&in);
#if DRV_OD_FB_REAL
    {
        /* r04f: once the sampler is armed and the chain has run two task
         * passes, its outputs replace the stand-in's in the live cells
         * (~/tcu_sweep/diag_real_feedback_dryrun.md); before that the
         * stand-in stays (the pre-'U' window the diagnosis skips anyway). */
        const volatile fbs_t *s = fb_shadow_state();
        if (s->armed != 0u && s->tick_n >= 2u) {
            drv_od_fb_t fb;
            for (uint8_t ch = 0u; ch < 10u; ch++) {
                fb.cls[ch] = s->cls[ch];
                fb.duty[ch] = s->duty[ch];
                fb.dutyf[ch] = s->dutyf[ch];
                fb.estm[ch] = s->estm[ch];
                fb.res[ch] = s->res[ch];
            }
            drv_od_fb_real(&fb);
        }
    }
#endif
}

/* the weak hook of bsw/sup/sup_batch.c (after drv_out_tick_head) */
void app_out_head_hook(void);
void app_out_head_hook(void)
{
    fb_shadow_gather_tick();
}
#endif
