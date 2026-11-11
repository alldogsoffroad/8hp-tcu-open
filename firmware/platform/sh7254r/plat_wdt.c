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
/* .noinit FOR REAL (r01r duty session): the plain static landed in .bss, which
 * start.S zeroes on EVERY boot — the reset discriminator counted nothing
 * across the r01r reset loop (bench: s_wovf_count read 0 after 4+ reboot
 * boundaries). In .noinit it survives resets (cleared only by power-off) and
 * accumulates one count per WDT reset per power session. */
static uint8_t s_wovf_count __attribute__((section(".noinit")));

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
    /* F6 (platform_audit #6/P0): WTCNT only — the stock's periodic superloop
     * feed (0x920F6) writes WTCNT=0x5ADB alone; WTCR belongs to the init and
     * mode-switch sites (0x95456 / the 16-bit RMW helpers 0x97850-0x9789E).
     * The old WTCR rewrite here re-armed watchdog RESET mode on every 1 ms
     * pass, silently undoing plat_wdt_mode(true) within one tick — interval
     * mode (and its ITI ISR) never survived past the switch. */
    REG16(WTCNT_ADDR) = 0x5AB7u;        /* ~30.3 ms margin (design §3) */
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
