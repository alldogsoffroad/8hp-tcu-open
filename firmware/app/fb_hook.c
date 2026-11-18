/* fb_hook.c — the feedback shadow's one live read (app/fb_shadow.h). NOT an
 * output path: it reads the live cells BY VALUE into fbs_in_t and hands them
 * to the read-only shadow; it writes nothing live.
 *
 * Called at the stock's 0x7CBA8 slot, right after drv_out_tick_head (the
 * harvest 0x904FA status part + the stand-in) and before the law
 * (sup_batch.c: app_out_head_hook — 5 ms in mode 0, the 10 ms part in modes
 * 1..3), so 0x94ED4 is the previous law pass's current, as the stock's
 * 0x838B0 reads it, and the stand-in's cells are this pass's. */
#include "app/fb_shadow.h"

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
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        in.i_req[ch] = LIVE_S16(0xFFF94ED4u + 2u * ch);         /* 0x94ED4 */
        in.si_cls[ch] = LIVE_U8(0xFFF93A2Eu + ch);              /* the stand-in's */
        in.si_duty[ch] = LIVE_U16(0xFFF9340Cu + 2u * ch);
        in.si_est[ch] = LIVE_U16(0xFFF93394u + 2u * ch);
        in.si_res[ch] = LIVE_U16(0xFFF933A8u + 2u * ch);
    }
    fb_shadow_tick(&in);
}

/* the weak hook of bsw/sup/sup_batch.c (after drv_out_tick_head) */
void app_out_head_hook(void);
void app_out_head_hook(void)
{
    fb_shadow_gather_tick();
}
#endif
