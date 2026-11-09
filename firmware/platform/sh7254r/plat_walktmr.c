/* plat_walktmr.c — ATU-D0 compare ISR for the walk timer (design §5.4, A2).
 *
 * The walk's own timer, independent of the 5 ms pass (the stock's v224
 * architecture). Rate = SUP_WALK_TICK_MS (default 1 ms, a named calibration
 * constant). The ISR feeds the WDT first (stock law 0x6C1BA), then calls
 * sup_walk_isr() which builds and queues the current walk job.
 */
#include "platform.h"
#include "regs.h"
#include "bsw/sup/sup_cal.h"

#ifndef SUP_WALK_TICK_MS
#define SUP_WALK_TICK_MS 1u
#endif

/* ATU-D0 registers (spec 22 §1: the stock's walk timer peripheral family).
 * Control set is observed; the counter address is [I] (design §5.4),
 * sanity-probed at bring-up. */
#define TIER_D0     REG8(0xFFFFF585u)   /* interrupt enable, bit 0 = CMID00 */
#define TSR_D0      REG8(0xFFFFF58Cu)   /* status, bit 0 = compare flag */
#define TSTR_D0     REG8(0xFFFFF58Eu)   /* start, bit 0 = D0 counting */
#define OCR_D0      REG32(0xFFFFF5B0u)  /* compare value (fraction bits << 8) */
#define TCNT_D0     REG32(0xFFFFF520u)  /* counter [I] — P8 step 0 probes */

/* forward: the walk service entry (sup_walk.c) */
extern void sup_walk_isr(void);

static bool s_walk_tmr_running;

bool plat_walktmr_running(void)
{
    return s_walk_tmr_running;
}

void plat_walktmr_start(void)
{
    if (s_walk_tmr_running) {
        return;
    }
    /* arm the first compare: current + tick_ms * 4000 (4 MHz / 1000 = per ms) */
    uint32_t now = TCNT_D0 >> 8;
    OCR_D0 = (now + (uint32_t)SUP_WALK_TICK_MS * 4000u) << 8;
    TIER_D0 = 0x01u;                   /* enable CMID00 */
    TSR_D0 = 0x00u;                    /* clear pending */
    TSTR_D0 |= 0x01u;                  /* start D0 */
    s_walk_tmr_running = true;
}

void plat_walktmr_stop(void)
{
    TSTR_D0 &= (uint8_t)~0x01u;
    TIER_D0 = 0x00u;
    s_walk_tmr_running = false;
}

uint32_t plat_walktmr_count(void)
{
    return TCNT_D0 >> 8;                /* P8 step 0: must advance when running */
}

/* the ISR body — called from the vector-table entry at slot 216 (CMID00) */
void plat_walktmr_isr(void)
{
    /* ack the compare-match */
    TSR_D0 = 0x00u;
    /* re-arm */
    uint32_t now = TCNT_D0 >> 8;
    OCR_D0 = (now + (uint32_t)SUP_WALK_TICK_MS * 4000u) << 8;
    /* WDT feed first (stock law: the walk service 0x6C1BA feeds before
     * dispatching — the walk's liveness IS the WDT's liveness) */
    plat_wdt_kick();
    /* dispatch the walk job */
    sup_walk_isr();
}
