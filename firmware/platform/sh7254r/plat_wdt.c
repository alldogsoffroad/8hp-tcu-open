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

/* ── S11 RESET-CAUSE WITNESS (baseline review 2026-11-11) ─────────────────
 * s_wovf_count alone cannot prove an external reset: a RES-pin reset clears
 * WTSR (manual §14), and the boot MAT may clear WOVF. WRCR is the second
 * discriminator — cleared only by a RES-pin / power-on reset, it SURVIVES a
 * WDT reset; wdt_init rewrites it (0x5A80 → reads 0x80), so it must be read
 * BEFORE wdt_init. Captured raw at entry, plus the previous boot's last-alive
 * breadcrumb (TCNTA + a code naming what the firmware was doing). Readings:
 *   wrcr & 0x80 set   → no RES since a firmware wdt_init: a WDT reset (or
 *                       the boot side left it set)
 *   wrcr & 0x80 clear → a RES-pin / power-on reset [M: manual §14.3.4]
 * The record is magic-guarded: power-on garbage resets it (boots = 1). */
#define RST_MAGIC 0x52535431u               /* 'RST1' */
plat_rst_t s_rst __attribute__((section(".noinit")));
volatile uint32_t s_alive_t __attribute__((section(".noinit")));
volatile uint32_t s_alive_code __attribute__((section(".noinit")));
/* r02m: per-boot interrupt entry counts (0 DEI4, 1 DEI5, 2 RSPI any, 3 walk)
 * and the previous boot's final counts — a storm in the dying boot shows as
 * a huge prev count, a stuck handler as an entry breadcrumb with no exit. */
volatile uint32_t s_isr_n[4] __attribute__((section(".noinit")));
volatile uint32_t s_isr_prev[4] __attribute__((section(".noinit")));
/* r02n: two 32-entry rolling rings of bus-B events; s_trc_cur names the
 * ring of THIS boot (toggled at entry), the other holds the previous boot's
 * last 32 events up to its reset. s_trc_head[r] = total events written. */
typedef struct {
    uint32_t t;          /* raw TCNTA (us) */
    uint8_t  kind;
    uint8_t  job;
    uint8_t  n;
    uint8_t  sr;         /* SR.I at the call */
    uint16_t tx0;
    uint16_t rx[3];
} trc_rec_t;             /* 16 bytes (stack guard: 2 x 32 x 16 = 1 KB) */
#define TRC_N 32u
trc_rec_t s_trc[2][TRC_N] __attribute__((section(".noinit")));
volatile uint32_t s_trc_head[2] __attribute__((section(".noinit")));
volatile uint32_t s_trc_cur __attribute__((section(".noinit")));

void plat_wdt_early_capture(void)
{
    uint32_t prev_t = s_alive_t, prev_code = s_alive_code;

    const bool warm = (s_rst.magic == RST_MAGIC);
    if (!warm) {
        s_rst.magic = RST_MAGIC;
        s_rst.boots = 0u;
        prev_t = 0u;
        prev_code = 0u;
    }
    for (uint32_t i = 0u; i < 4u; i++) {
        s_isr_prev[i] = warm ? s_isr_n[i] : 0u;
        s_isr_n[i] = 0u;
    }
    s_trc_cur = warm ? ((s_trc_cur & 1u) ^ 1u) : 0u;
    if (!warm) {
        s_trc_head[1] = 0u;
    }
    s_trc_head[s_trc_cur] = 0u;
    s_rst.boots++;
    s_rst.wtcr = REG16(WTCR_ADDR);
    s_rst.wtcnt = REG16(WTCNT_ADDR);
    s_rst.wtsr = REG16(WTSR_ADDR);
    s_rst.wrcr = REG16(WRCR_ADDR);
    s_rst.tcnta = FRT_TCNTA;
    s_rst.prev_alive_t = prev_t;
    s_rst.prev_alive_code = prev_code;
    s_alive_t = 0u;
    s_alive_code = 0u;
}

uint32_t plat_boot_index(void)
{
    return s_rst.boots;
}

uint32_t plat_boot_tcnta(void)
{
    return s_rst.tcnta;
}

void plat_trace(uint8_t kind, uint8_t job, uint8_t n, uint16_t tx0, const uint16_t *rx, uint8_t nrx)
{
    const uint32_t lk = plat_sr_i();
    plat_sr_set(15u);
    const uint32_t r = s_trc_cur & 1u;
    const uint32_t h = s_trc_head[r];
    trc_rec_t *e = &s_trc[r][h % TRC_N];
    e->t = FRT_TCNTA;
    e->kind = kind;
    e->job = job;
    e->n = n;
    e->sr = (uint8_t)lk;
    e->tx0 = tx0;
    for (uint8_t i = 0u; i < 3u; i++) {
        e->rx[i] = (rx != (const uint16_t *)0 && i < nrx) ? rx[i] : 0u;
    }
    s_trc_head[r] = h + 1u;
    plat_sr_set(lk);
}

void plat_alive(uint32_t code)
{
    s_alive_t = FRT_TCNTA;
    s_alive_code = code;
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
