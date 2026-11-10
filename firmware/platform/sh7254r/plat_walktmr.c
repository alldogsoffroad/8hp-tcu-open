/* plat_walktmr.c — ATU-D0 CMID00 compare ISR for the walk timer (design §5.4).
 *
 * The walk's own timer, independent of the 5 ms pass (the stock's v224
 * architecture), vector 216. Register map (manual §13.19 + bench
 * corrections; the flag/enable ops use the STOCK BYTE IDIOM — the 16-bit
 * register writes land mirrored on this bus, see plat_tick.c's r01g note):
 *   TSTRD  0xFFFFF500 bit0 = STRD0
 *   TCRD0  0xFFFFF52C bits 8-6 = CKSEL1D0: 3 = bus 3 = Pφ (20 MHz)
 *   TSRD0  0xFFFFF58C bit4 = CMFAD00 — flag/ack via BYTE 0xFFFFF58D (bit 4)
 *   TIERD0 0xFFFFF58E bit4 = CMEAD00 — enable via BYTE 0xFFFFF58F |= 0x10
 *   OCRD00 0xFFFFF590, TCNT1D0 0xFFFFF520 (32-bit)
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

/* ATU-D0 registers */
#define TCNT1D0   REG32(0xFFFFF520u)
#define OCRD00    REG32(0xFFFFF590u)   /* compare-match A, channel 0 */
#define TSRD0_FLG REG8(0xFFFFF58Du)    /* CMFAD00 = bit 4 of this byte */
#define TSRD0_ACK REG8(0xFFFFF58Du)    /* ack: &= 0xEF (stock byte idiom) */
#define TIERD0_EN REG8(0xFFFFF58Fu)    /* CMEAD00 enable: |= 0x10 */
#define TCRD0     REG16(0xFFFFF52Cu)   /* bits 8-6 = CKSEL1D0 */
#define TSTRD     REG8(0xFFFFF500u)    /* bit0 = STRD0 */

#define MS_COUNTS 20000u                /* 1 ms at Pφ = 20 MHz */
#define CMFA00_BYTE 0x10u

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
    ATUENR |= ATUENR_TDE;                    /* timer D enable (boot leaves it clear) */
    TCRD0 = (uint16_t)((TCRD0 & ~0x01C0u) | (3u << 6));  /* TCNT1D0 ← bus 3 = Pφ */
    OCRD00 = next_ocr(TCNT1D0);
    TSRD0_ACK &= 0xEFu;                      /* CMFAD00 ack (stock byte idiom) */
    TIERD0_EN |= 0x10u;                      /* CMEAD00 enable (stock byte idiom) */
    TSTRD |= 0x01u;                          /* start D0 */
    s_walk_tmr_running = true;
}

void plat_walktmr_stop(void)
{
    TSTRD &= (uint8_t)~0x01u;
    TIERD0_EN &= 0xEFu;                      /* disable (byte idiom) */
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
    if ((TSRD0_FLG & CMFA00_BYTE) == 0u) {
        return false;
    }
    TSRD0_ACK &= 0xEFu;
    OCRD00 = next_ocr(TCNT1D0);
    plat_wdt_kick();
    sup_walk_isr();
    return true;
}

/* the ISR body — called from the vector-table entry at slot 216 (CMID00) */
void plat_walktmr_isr(void)
{
    /* ack the compare-match (stock byte idiom) */
    TSRD0_ACK &= 0xEFu;
    /* re-arm */
    OCRD00 = next_ocr(TCNT1D0);
    /* WDT feed first (stock law: the walk service 0x6C1BA feeds before
     * dispatching — the walk's liveness IS the WDT's liveness) */
    plat_wdt_kick();
    /* dispatch the walk job */
    sup_walk_isr();
}
