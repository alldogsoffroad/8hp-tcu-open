/* pa7_hook.c — the PA7 shadow's live reads (app/pa7_shadow.h). NOT an output
 * path: it reads the live cells BY VALUE (const volatile) into
 * pa7_rng_in_t / pa7_pin_in_t and hands them to the read-only shadow; it
 * writes nothing live.
 *
 *   pa7_hook_range       main.c task_10ms, right after rng_chain_tick_10ms,
 *                        in OS modes 1..3 — the stock's 0x9B34A runs 0xA6CF8
 *                        after the D6D6 chain (0xA59C0 / 0xA59E8);
 *   pa7_hook_range_init  main.c, right after rng_chain_init — the stock's
 *                        init one-shot 0x9B228 calls 0xA6CF8 after the init
 *                        arbiter;
 *   app_pin_cut_hook     the weak hook of bsw/sup/sup_batch.c at the pin-cut
 *                        entry 0x7CC32, after 0x849D4 (drv_ga_law_tick) — the
 *                        stock's 0x86B0C is that entry's 6th call: 5 ms in
 *                        mode 0, the 10 ms part in modes 1..3. */
#include "app/pa7_shadow.h"
#include "platform/platform.h"      /* plat_frt_us (the witness timestamp) */
#include "app/park_cells.h"         /* PCELL_TGT: the range D6D6, read-only (the writer guard) */
#include "bsw/drv/drv_pa7.h"         /* r04e: PA7 live (the pin only; C9 bit 7 stays held) */

#include <stdint.h>

#if !defined(TCU_RAM_APP)
#define LIVE_U8(a)   (*(const volatile uint8_t  *)(uintptr_t)(a))
#define LIVE_S16(a)  (*(const volatile int16_t  *)(uintptr_t)(a))

static void range_gather(uint8_t os_mode)
{
    pa7_rng_in_t in;
    in.d6d6 = PCELL_TGT;                            /* 0xFFF8D6D6 (const volatile) */
    in.d6da = LIVE_U8(0xFFF8D6DAu);
    in.d672 = LIVE_U8(0xFFF8D672u);
    in.d673 = LIVE_U8(0xFFF8D673u);
    in.d65e = LIVE_U8(0xFFF8D65Eu);
    in.d5f8 = LIVE_U8(0xFFF8D5F8u);
    in.d5f9 = LIVE_U8(0xFFF8D5F9u);
    in.d60a = LIVE_U8(0xFFF8D60Au);
    in.d65a = LIVE_S16(0xFFF8D65Au);
    in.os_mode = os_mode;
    in.pad = 0u;
    pa7_shadow_range(&in);
}

void pa7_hook_range(uint8_t os_mode)
{
    /* the chain (0x9B34A) runs only in modes 1..3; so does its 0xA6CF8 */
    if (os_mode >= 1u && os_mode <= 3u) {
        range_gather(os_mode);
    }
}

void pa7_hook_range_init(void)
{
    range_gather(0u);
}

/* the weak hook of bsw/sup/sup_batch.c (the pin-cut entry, after 0x849D4) */
void app_pin_cut_hook(void);
void app_pin_cut_hook(void)
{
    pa7_pin_in_t in;
    const uint8_t mode = LIVE_U8(0xFFF93B33u);                  /* 0x93B33 */
    in.t_us = plat_frt_us();
    in.n5 = (uint8_t)((mode == 0u) ? 1u : 2u);
    in.nib = LIVE_U8(0xFFF90958u);                              /* the supervisor nibble */
    in.sup_st = LIVE_U8(0xFFF94FECu);                           /* 0x94FEC */
    in.os_mode = mode;
    in.an7 = LIVE_S16(0xFFF9018Cu + 2u * PA7_CAL_AN);           /* 0x9018C[7] */
    in.sup = LIVE_S16(0xFFF94F86u);                             /* 0x94F86 */
    pa7_shadow_pin(&in);
#if PA7_LIVE
    /* r04e: the real pin follows the shadow's 0x8CE48 result, through the
     * stock's 0x90CF6. Before the 0x9095B gate the shadow's value is the
     * init 0 (0x90D12), as the stock's pin. The C9 bit-7 request is NOT
     * made live (sup_ctrl.c's `& 0x80` test is still to be fixed). */
    drv_pa7_apply(pa7_shadow_state()->pa7);
#endif
}
#endif
