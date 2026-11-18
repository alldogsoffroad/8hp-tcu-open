/* drv_pa7.c — stock 0x90CF6 (bytes 0x90CF6–0x90D10, re-read 2026-11-18):
 *   extu.b r4; r6 = 0xFFFE3800; 0xFFF90920 = v;
 *   v == 0 → bclr.b #7,@(3,r6) else bset.b #7,@(3,r6)
 * PA7 is an output, low, from our port of 0x90D12 (stock_gpio_init).
 * Output path (TCU_OUTPUT_PATH): the value is handed in by the PA7 hook. */
#define TCU_OUTPUT_PATH 1
#include "bsw/drv/drv_pa7.h"

#if defined(__sh__) && !defined(TCU_RAM_APP) && !defined(TCU_LOADER)
void drv_pa7_apply(uint8_t v)
{
    volatile uint8_t *const pa_lo = (volatile uint8_t *)0xFFFE3803u;   /* PADR low byte */
    *(volatile uint8_t *)0xFFF90920u = v;                              /* the stock's mirror */
    if (v == 0u) {
        *pa_lo = (uint8_t)(*pa_lo & (uint8_t)~0x80u);
    } else {
        *pa_lo = (uint8_t)(*pa_lo | 0x80u);
    }
}

#endif
