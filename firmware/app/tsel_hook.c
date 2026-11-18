/* tsel_hook.c — the temperature-selection shadow's live reads
 * (app/tsel_shadow.h). NOT an output path: it reads the live cells BY VALUE
 * (const volatile) into tsel_in_t and hands them to the read-only shadow;
 * it writes nothing live.
 *
 *   tsel_hook_init      main.c, in the shadow init block — the stock's init
 *                       hub 0x832AC (0x88BAC, 0x88D98), once;
 *   app_temp_hub_hook   the weak hook of bsw/sup/sup_batch.c right after
 *                       drv_temp_tick — the stock's hub 0x83300 (stub
 *                       0x7CB6E): 0x88DA8 and 0x88BD0 around the fluid's
 *                       0x887D8; 5 ms in mode 0, the 10 ms part in modes 1..3.
 *
 * Inputs: the J3 average 0x908CA (sup_ctrl.c, CELL_TELEM_AVG), the J4 word
 * 0x908CC (CELL_SSLB2_COPY), the supervisor's comparator flags 0x9095E
 * (CELL_DIR_BITS — named "direction bits" in sup_cells.h, a misnomer: they
 * are the 0x907D3 bits 4/5 the die check compares against), the variant
 * 0x90966 (never written by us: 0, the bench's value), the fluid 0x94F98 /
 * 0x94FEE (drv_temp's 0x887D8 this pass: raw, or its substitute = the previous
 * pass's 0x9393A once 0x940FC >= 7 — the stock's loop too, harmless: the
 * fluid is the selection's lowest priority). */
#include "app/tsel_shadow.h"
#include "platform/platform.h"      /* plat_frt_us (the witness timestamp) */

#include <stdint.h>

#if !defined(TCU_RAM_APP)
#define LIVE_U8(a)   (*(const volatile uint8_t  *)(uintptr_t)(a))
#define LIVE_U16(a)  (*(const volatile uint16_t *)(uintptr_t)(a))
#define LIVE_S16(a)  (*(const volatile int16_t  *)(uintptr_t)(a))

void tsel_hook_init(void)
{
    tsel_shadow_init();
}

/* the weak hook of bsw/sup/sup_batch.c (after drv_temp_tick) */
void app_temp_hub_hook(void);
void app_temp_hub_hook(void)
{
    tsel_in_t in;
    const uint8_t mode = LIVE_U8(0xFFF93B33u);                  /* 0x93B33 */
    in.t_us = plat_frt_us();
    in.n908ca = LIVE_U16(0xFFF908CAu);                          /* the J3 average */
    in.r908cc = LIVE_S16(0xFFF908CCu);                          /* the J4 word */
    in.t94f98 = LIVE_S16(0xFFF94F98u);                          /* the fluid (raw or substitute) */
    in.st94fee = LIVE_U8(0xFFF94FEEu);                          /* its status */
    in.f9095e = LIVE_U8(0xFFF9095Eu);                           /* the comparator flags */
    in.v90966 = LIVE_U8(0xFFF90966u);                           /* the variant */
    in.n5 = (uint8_t)((mode == 0u) ? 1u : 2u);
    in.fm_ready = 1u;                                           /* 0x6F594: stand-in S1 */
    in.os_mode = mode;
    tsel_shadow_pass(&in);
}
#endif
