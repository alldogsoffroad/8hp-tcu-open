/* plat_wdt.c — WDT service for the rebuild (design §5.6, spec 22 §2.3, §3).
 *
 * Carries the bench-proven kick/mode/quick-reset law, plus:
 * - WTSR.WOVF capture once at init (reset-cause occurrence counter,
 *   spec 24 §4.3 — the stock's only reset discriminator)
 * - The A-law mode switch: interval mode ONLY on an echo-confirmed bit-0 edge
 */
#include "platform.h"
#include "regs.h"

static bool s_interval;
static uint8_t s_wovf_count;      /* .noinit — survives resets, not power-off */

uint8_t plat_wdt_wovf_count(void)
{
    return s_wovf_count;
}

void plat_wdt_capture_reset_cause(void)
{
    /* called ONCE at init, before the first feed (spec 24 §4.3) */
    uint8_t wtsr = REG8(WTSR_ADDR);
    if (wtsr & 0x80u) {                 /* WOVF bit 7 — WDT reset occurred */
        s_wovf_count++;
        REG8(WTSR_ADDR) = 0x00u;        /* clear by 0-keyed write */
    }
}

void plat_wdt_kick(void)
{
    /* WTCNT ← 0x5Axx (write-protect key), WTCR ← 0xA5xx */
    REG8(WTCNT_ADDR) = 0xB7u;           /* ~30.3 ms margin (design §3) */
    REG8(WTCR_ADDR) = 0x67u;            /* watchdog mode, /5120 — bench-proven */
}

void plat_wdt_mode(bool interval)
{
    if (interval == s_interval) {
        return;
    }
    s_interval = interval;
    if (interval) {
        /* interval mode: WTCR bit 6 (TME) = 1 — ITI interrupt on overflow */
        REG8(WTCR_ADDR) = 0x67u | 0x40u;
    } else {
        /* reset mode: TME = 0 — WDTOVF pin resets the MCU (stock law) */
        REG8(WTCR_ADDR) = 0x67u;
    }
}

void plat_wdt_quick_reset(void)
{
    /* fatal path: fast clock, WRCR reset-enable, halt — the stock 0x966B8 */
    REG8(WTCR_ADDR) = 0x63u;            /* fast clock (short timeout) */
    REG8(WRCR_ADDR) = 0x80u;            /* WDTOVF resets the MCU */
    for (;;) {
        /* halt — the WDT overflow does the reset */
    }
}

bool plat_wdt_is_interval(void)
{
    return s_interval;
}
