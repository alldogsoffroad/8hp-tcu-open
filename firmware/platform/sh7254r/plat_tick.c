/* plat_tick.c — ATU-D1 CMID10 1 ms tick ISR (design §5.3, spec 24 §1.2).
 *
 * The tick is the scheduler's heartbeat: vector 228. THE TRUE REGISTER MAP
 * (manual §13.19, corrected 2026-11-09 after the r01/r01b bench showed the
 * timers never ran — the first-flash addresses were design guesses):
 *   TSTRD  0xFFFFF500 bit1 = STRD1 (NOT 0xF540 — that is TCNT1D2)
 *   ATUENR 0xFFFFF004 bit4 = TDE   (live bench: already 0x9F — set anyway)
 *   TCRD1  0xFFFFF53C bits 8-6 = CKSEL1D1: 3 = clock bus 3 = PSCR3 = 0 → Pφ
 *   TSRD1  0xFFFFF5CC bit4 = CMFAD10 (compare-A flag, write-0-after-read)
 *   TIERD1 0xFFFFF5CE bit4 = CMEAD10 (compare-A interrupt enable)
 *   OCRD10 0xFFFFF5D0, TCNT1D1 0xFFFFF530 (32-bit, counts at 20 MHz)
 * Pφ = 20 MHz bench-confirmed (spec 05 §1.1): 1 ms = 20 000 raw counts
 * (no <<8 fraction — the stock's fraction convention assumed a 32 MHz
 * counter; ours runs the raw Pφ bus).
 */
#include "platform.h"
#include "regs.h"
#include "bsw/sched/sched.h"

/* ATU-D1 registers (manual §13.19, the true map) */
#define TCNT1D1   REG32(0xFFFFF530u)
#define OCRD10    REG32(0xFFFFF5D0u)   /* compare-match A, channel 0 */
#define TSRD1     REG16(0xFFFFF5CCu)   /* bit4 = CMFAD10 */
#define TIERD1    REG16(0xFFFFF5CEu)   /* bit4 = CMEAD10 */
#define TCRD1     REG16(0xFFFFF53Cu)   /* bits 8-6 = CKSEL1D1 */
#define TSTRD     REG8(0xFFFFF500u)    /* bit1 = STRD1 */

#define TICK_COUNTS 20000u             /* 1 ms at Pφ = 20 MHz */
#define CMFA10     0x0010u

static volatile uint32_t s_tick_count;

uint32_t plat_tick_count(void)
{
    return s_tick_count;
}

void plat_tick_start(void)
{
    ATUENR |= ATUENR_TDE;                       /* timer D enable (defensive) */
    TCRD1 = (uint16_t)((TCRD1 & ~0x01C0u) | (3u << 6));  /* TCNT1D1 ← bus 3 = Pφ */
    /* arm the first compare, clear any pending flag, enable, start */
    OCRD10 = TCNT1D1 + TICK_COUNTS;
    TSRD1 = (uint16_t)(TSRD1 & ~CMFA10);     /* write-0-after-read clears */
    TIERD1 = CMFA10;
    TSTRD |= 0x02u;
}

/* the ISR body — called from the vector-table entry at slot 228 */
void plat_tick_isr(void)
{
    /* ack the compare-match: write-0-after-read, only bit 4 */
    TSRD1 = (uint16_t)(TSRD1 & ~CMFA10);
    /* re-arm: next compare = current + 1 ms */
    OCRD10 = TCNT1D1 + TICK_COUNTS;
    s_tick_count++;
    sched_tick_isr();                      /* bounded: timestamp + due scan */
}
