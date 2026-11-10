/* plat_tick.c — ATU-D1 CMID10 1 ms tick ISR (design §5.3, spec 24 §1.2).
 *
 * The tick is the scheduler's heartbeat: vector 228. Register map
 * (manual §13.19 + the r01-r01g bench corrections):
 *   TSTRD  0xFFFFF500 bit1 = STRD1 (NOT 0xF540 — that is TCNT1D2)
 *   ATUENR 0xFFFFF000 bit4 = TDE   (boot leaves 0x0163 live — TDE CLEAR; the app enables D)
 *   TCRD1  0xFFFFF53C bits 8-6 = CKSEL1D1: 3 = clock bus 3 = PSCR3 = 0 → Pφ
 *   TSRD1  0xFFFFF5CC bit4 = CMFAD10 — ack via BYTE 0xFFFFF5CD &= 0xEF
 *   TIERD1 0xFFFFF5CE bit4 = CMEAD10 — enable via BYTE 0xFFFFF5CF |= 0x10
 *   OCRD10 0xFFFFF5D0, TCNT1D1 0xFFFFF530 (32-bit)
 * THE BYTE IDIOM IS THE STOCK'S OWN (catalog unit_06, os_timer_rearm
 * 0x6791A decoded: DAT_fffe0c0e=(…&0xFFF0)|6, DAT_fffff5cd&=0xEF,
 * DAT_fffff5cf|=0x10). Our 16-bit register writes landed MIRRORED on this
 * bus (r01g readback: the 0x10 appeared in the HIGH byte = OVE1Dn, the
 * wrong bit — the tick interrupt never requested; s_tick_isr_count=0).
 * Pφ = 20 MHz bench-confirmed (spec 05 §1.1): 1 ms = 20 000 counts.
 */
#include "platform.h"
#include "regs.h"
#include "bsw/sched/sched.h"

/* ATU-D1 registers */
#define TCNT1D1   REG32(0xFFFFF530u)
#define OCRD10    REG32(0xFFFFF5D0u)   /* compare-match A, channel 0 */
#define TSRD1_FLG REG8(0xFFFFF5CDu)    /* CMFAD10 = bit 4 of this byte */
#define TSRD1_ACK REG8(0xFFFFF5CDu)    /* ack: &= 0xEF (stock verbatim) */
#define TIERD1_EN REG8(0xFFFFF5CFu)    /* CMEAD10 enable: |= 0x10 (stock verbatim) */
#define TCRD1     REG16(0xFFFFF53Cu)   /* bits 8-6 = CKSEL1D1 */
#define TSTRD     REG8(0xFFFFF500u)    /* bit1 = STRD1 */

#define TICK_COUNTS 20000u             /* 1 ms at Pφ = 20 MHz */
#define CMFA10_BYTE 0x10u

static volatile uint32_t s_tick_count;
volatile uint32_t s_tick_isr_count;  /* TRUE vector deliveries (diagnostic) */

uint32_t plat_tick_count(void)
{
    return s_tick_count;
}

uint32_t plat_tick_isr_count(void)
{
    return s_tick_isr_count;
}

/* superloop fallback (r01e): harvest the compare flag directly. Returns
 * true if a tick elapsed. Safe with a live interrupt: the flag clears on
 * the first harvest; a rare ISR/poll race costs at most one double tick. */
bool plat_tick_poll_hw(void)
{
    if ((TSRD1_FLG & CMFA10_BYTE) == 0u) {
        return false;
    }
    TSRD1_ACK &= 0xEFu;                      /* ack (stock byte idiom) */
    OCRD10 = TCNT1D1 + TICK_COUNTS;          /* re-arm */
    s_tick_count++;
    return true;
}

void plat_tick_start(void)
{
    ATUENR |= ATUENR_TDE;                       /* timer D enable (boot leaves it clear) */
    TCRD1 = (uint16_t)((TCRD1 & ~0x01C0u) | (3u << 6));  /* TCNT1D1 ← bus 3 = Pφ */
    /* arm the first compare, clear any pending flag, enable, start */
    OCRD10 = TCNT1D1 + TICK_COUNTS;
    TSRD1_ACK &= 0xEFu;                      /* CMFAD10 ack (stock byte idiom) */
    TIERD1_EN |= 0x10u;                      /* CMEAD10 enable (stock byte idiom) */
    TSTRD |= 0x02u;
}

/* the ISR body — called from the vector-table entry at slot 228 */
void plat_tick_isr(void)
{
    /* ack the compare-match (stock byte idiom) */
    TSRD1_ACK &= 0xEFu;
    /* re-arm: next compare = current + 1 ms */
    OCRD10 = TCNT1D1 + TICK_COUNTS;
    s_tick_count++;
    s_tick_isr_count++;
    sched_tick_isr();                      /* bounded: timestamp + due scan */
}
