/* plat_tick.c — ATU-D1 CMID10 1 ms tick ISR (design §5.3, spec 24 §1.2).
 *
 * The tick is the scheduler's heartbeat: vector 228, count clock 4 MHz
 * with 8 fraction bits (Pφ/5 = 4 MHz from PSCRA1=4, design §3).
 * The ISR re-arms itself and calls sched_tick_isr() — bounded work (~1 µs).
 */
#include "platform.h"
#include "regs.h"
#include "bsw/sched/sched.h"

/* ATU-D1 registers (spec 24, validated: ATU-D1 CMID10 not "timer H") */
#define TCNT1D1   REG32(0xFFFFF530u)   /* 16-bit counter + 8 fraction bits */
#define OCRD10    REG32(0xFFFFF5D0u)   /* compare-match A */

static volatile uint32_t s_tick_count;

uint32_t plat_tick_count(void)
{
    return s_tick_count;
}

void plat_tick_start(void)
{
    /* ack any pending, arm the first compare, enable the interrupt, start */
    uint32_t now = TCNT1D1 >> 8;
    OCRD10 = (now + 4000u) << 8;         /* 4000 × 1 µs = 1 ms at 4 MHz */
    /* TIER for D1 CMI (bit 0 of the D1 interrupt-enable register) */
    REG8(0xFFFFF545u) = 0x01u;           /* enable CMIH-D1 (CMID10) */
    /* clear pending in TSR */
    REG8(0xFFFFF54Cu) = 0x00u;
    /* start D1 counting (TSTR bit 1) */
    REG8(0xFFFFF540u) |= 0x02u;
}

/* the ISR body — called from the vector-table entry at slot 228 */
void plat_tick_isr(void)
{
    /* ack the compare-match (TSR bit 0) */
    REG8(0xFFFFF54Cu) = 0x00u;
    /* re-arm: next compare = current + 4000 (fraction bits << 8) */
    uint32_t now = TCNT1D1 >> 8;
    OCRD10 = (now + 4000u) << 8;
    s_tick_count++;
    sched_tick_isr();                      /* bounded: timestamp + due scan */
}
