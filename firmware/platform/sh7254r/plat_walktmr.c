/* plat_walktmr.c — ATU-D0 CMID00 compare ISR for the walk timer (design §5.4).
 *
 * The walk's own timer, independent of the 5 ms pass (the stock's v224
 * architecture), vector 216. THE TRUE REGISTER MAP (manual §13.19, corrected
 * 2026-11-09 — the first-flash addresses were design guesses and the timers
 * never ran; notably 0xFFF58E is TIERD0, NOT a start register, and 0xFFF5B0
 * is a DOWN-counter, not the compare):
 *   TSTRD  0xFFFFF500 bit0 = STRD0
 *   TCRD0  0xFFFFF52C bits 8-6 = CKSEL1D0: 3 = bus 3 = Pφ (20 MHz)
 *   TSRD0  0xFFFFF58C bit4 = CMFAD00 (this one the first flash had right)
 *   TIERD0 0xFFFFF58E bit4 = CMEAD00
 *   OCRD00 0xFFFFF590, TCNT1D0 0xFFFFF520 (32-bit, counts at Pφ)
 * Rate = SUP_WALK_TICK_MS (default 1 ms = 20 000 counts, spec 05 §1.1).
 * The ISR feeds the WDT first (stock law 0x6C1BA), then calls
 * sup_walk_isr() which builds and queues the current walk job.
 */
#include "platform.h"
#include "regs.h"
#include "bsw/sup/sup_cal.h"

#ifndef SUP_WALK_TICK_MS
#define SUP_WALK_TICK_MS 1u
#endif

/* ATU-D0 registers (manual §13.19, the true map) */
#define TCNT1D0   REG32(0xFFFFF520u)
#define OCRD00    REG32(0xFFFFF590u)   /* compare-match A, channel 0 */
#define TSRD0     REG16(0xFFFFF58Cu)   /* bit4 = CMFAD00 */
#define TIERD0    REG16(0xFFFFF58Eu)   /* bit4 = CMEAD00 */
#define TCRD0     REG16(0xFFFFF52Cu)   /* bits 8-6 = CKSEL1D0 */
#define TSTRD     REG8(0xFFFFF500u)    /* bit0 = STRD0 */

#define MS_COUNTS 20000u                /* 1 ms at Pφ = 20 MHz */
#define CMFA00    0x0010u

/* forward: the walk service entry (sup_walk.c) */
extern void sup_walk_isr(void);

static bool s_walk_tmr_running;

static uint32_t next_ocr(uint32_t now)
{
    return now + (uint32_t)SUP_WALK_TICK_MS * MS_COUNTS;
}

bool plat_walktmr_running(void)
{
    return s_walk_tmr_running;
}

void plat_walktmr_start(void)
{
    if (s_walk_tmr_running) {
        return;
    }
    ATUENR |= ATUENR_TDE;                       /* timer D enable (defensive) */
    TCRD0 = (uint16_t)((TCRD0 & ~0x01C0u) | (3u << 6));  /* TCNT1D0 ← bus 3 = Pφ */
    OCRD00 = next_ocr(TCNT1D0);
    TSRD0 = (uint16_t)(TSRD0 & ~CMFA00);     /* write-0-after-read clears */
    TIERD0 = CMFA00;
    TSTRD |= 0x01u;                          /* start D0 */
    s_walk_tmr_running = true;
}

void plat_walktmr_stop(void)
{
    TSTRD &= (uint8_t)~0x01u;
    TIERD0 = 0x0000u;
    s_walk_tmr_running = false;
}

uint32_t plat_walktmr_count(void)
{
    return TCNT1D0;                          /* P8 step 0: must advance when running */
}

/* superloop fallback (r01e): harvest the walk compare flag directly.
 * Mirror of the ISR body (WDT feed first — the stock law). */
bool plat_walktmr_poll(void)
{
    if ((TSRD0 & CMFA00) == 0u) {
        return false;
    }
    TSRD0 = (uint16_t)(TSRD0 & ~CMFA00);
    OCRD00 = next_ocr(TCNT1D0);
    plat_wdt_kick();
    sup_walk_isr();
    return true;
}

/* the ISR body — called from the vector-table entry at slot 216 (CMID00) */
void plat_walktmr_isr(void)
{
    /* ack the compare-match: write-0-after-read, only bit 4 */
    TSRD0 = (uint16_t)(TSRD0 & ~CMFA00);
    /* re-arm */
    OCRD00 = next_ocr(TCNT1D0);
    /* WDT feed first (stock law: the walk service 0x6C1BA feeds before
     * dispatching — the walk's liveness IS the WDT's liveness) */
    plat_wdt_kick();
    /* dispatch the walk job */
    sup_walk_isr();
}
