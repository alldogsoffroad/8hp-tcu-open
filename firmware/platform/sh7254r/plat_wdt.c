/* plat_wdt.c — WDT service for the rebuild (design §5.6, spec 22 §2.3, §3).
 *
 * Carries the bench-proven kick/mode/quick-reset law, plus:
 * - RSTCSR.WOVF capture once at init (reset-cause occurrence counter,
 *   spec 24 §4.3 — the stock's only reset discriminator)
 * - The A-law mode switch: interval mode ONLY on an echo-confirmed bit-0 edge
 *
 * EVERY WDT write is 16-BIT KEYED (platform_audit #6/P0: the stock writes
 * NO WDT register by byte anywhere in 0x50000-0x180000 — all five decoded
 * sites are mov.w with the key in the high byte: WTCNT=0x5Axx @0x95440/
 * 0x920F6, WTCR=0xA5xx @0x95456/0x97850, WRCR=0x5A80 @0x95444; a byte write
 * at the even offset hits the KEY half by address math).
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
    /* called ONCE at init, before the first feed (spec 24 §4.3).
     * WOVF lives in the RSTCSR LOW half (0xFFFE0005 bit 7) — the old
     * REG8(0xFFFE0004) read captured the KEY half, not the flag
     * (platform_audit #2). */
    if ((REG8(WTSR_ADDR + 1u) & 0x80u) != 0u) {   /* WOVF — WDT reset occurred */
        uint16_t v = REG16(WTSR_ADDR);
        s_wovf_count++;
        /* stock's 16-bit keyed clear, verbatim (0x95446-0x95450: RMW with
         * the 0xA5 key; & 0xFF77 also clears RSTCSR bit 3 like the stock) */
        REG16(WTSR_ADDR) = (uint16_t)((v | 0xA500u) & 0xFF77u);
    }
}

void plat_wdt_kick(void)
{
    /* WTCNT ← 0x5A key << 8 | count; WTCR ← 0xA5 key << 8 | config */
    REG16(WTCNT_ADDR) = 0x5AB7u;        /* ~30.3 ms margin (design §3) */
    REG16(WTCR_ADDR) = 0xA567u;         /* watchdog mode, /5120 — bench-proven */
}

void plat_wdt_mode(bool interval)
{
    if (interval == s_interval) {
        return;
    }
    s_interval = interval;
    if (interval) {
        /* interval mode: WTCR bit 6 must be CLEAR — the stock's engaged
         * live value is config 0x27 (platform_audit #1/P0: the old
         * 0x67|0x40 was a no-op, bit 6 was already set, so interval mode
         * never engaged and the ITI ISR plumbing was dead as built) */
        REG16(WTCR_ADDR) = 0xA527u;
    } else {
        /* reset mode: bit 6 set (0x67 — the bench-proven WDTOVF-reset
         * watchdog config) */
        REG16(WTCR_ADDR) = 0xA567u;
    }
}

void plat_wdt_quick_reset(void)
{
    /* fatal path: fast clock, WRCR reset-enable, halt — the stock 0x966B8 */
    REG16(WTCR_ADDR) = 0xA563u;         /* fast clock (short timeout), keyed */
    REG16(WRCR_ADDR) = 0x5A80u;         /* WDTOVF resets the MCU (stock 0x95444) */
    for (;;) {
        /* halt — the WDT overflow does the reset */
    }
}

bool plat_wdt_is_interval(void)
{
    return s_interval;
}
