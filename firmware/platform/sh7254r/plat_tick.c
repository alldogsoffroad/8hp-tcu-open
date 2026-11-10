/* plat_tick.c — ATU-D1 CMID10 tick ISR (design §5.3, spec 24 §1.2).
 *
 * The tick is the scheduler's heartbeat: vector 228. Register map
 * (timer_audit §5: manual §31 list + manual §13.19.2 PROSE + the stock's
 * own bytes; the bit-table figures are the known-smeared artifact):
 *   TSTRD  0xFFFFF500 byte = 0x0F (stock init 0x9718C starts all four D)
 *   ATUENR 0xFFFFF000 bit4 = TDE   (boot leaves 0x0163 live — TDE CLEAR; the app enables D)
 *   TCRD1  0xFFFFF53C WORD; CKSEL1D1 = bits 6:4 (NOT bits 8-6!); the stock
 *          writes 0x4000 ONCE at init (boot table @0x40F7C, all six
 *          subblocks 0x4000; store @0x97152) and NEVER touches TCRD again.
 *          0x4000 = OBREDn | CKSEL1D1=000 → clock-bus 0 = PSCR0=19 → 1 MHz.
 *          The old RMW (old&~0x01C0)|(3<<6)=0x00C0 latched CKSEL1D1=100 =
 *          bus 4 = external input — nothing connected, counters frozen
 *          (bench: TCNT stuck ~0x45, TCRD1 readback 0x40 = field 4).
 *   TSRD1  flag/ack via BYTE 0xFFFFF5CD &= 0xEF (bit 12 = CMFAD10, stock
 *          os_timer_rearm 0x6795C; the "16-bit writes land mirrored" lore
 *          was a misdiagnosis — it was big-endian word placement plus the
 *          frozen counter, timer_audit §4)
 *   TIERD1 enable via BYTE 0xFFFFF5CF |= 0x10 (bit 12 = CMEAD10, stock 0x67960)
 *   OCRD10 0xFFFFF5D0, TCNT1D1 0xFFFFF530 (32-bit; ONLY bits 31:8 compare —
 *          "upper 24 bits available", manual §13.19.15, so every OCR write
 *          is a longword with bits 7:0 = 0 and the period is quantized to
 *          256 bus counts)
 */
#include "platform.h"
#include "regs.h"
#include "bsw/sched/sched.h"

/* ATU-D1 registers */
#define TCNT1D1   REG32(0xFFFFF530u)
#define OCRD10    REG32(0xFFFFF5D0u)   /* compare-match A, channel 0 */
#define TSRD1_FLG REG8(0xFFFFF5CDu)    /* CMFAD10 = bit 12 = bit 4 of this byte
                                     * (the manual bit-table row placing CMFAD
                                     * at bits 7-4 is smeared; trust the stock's
                                     * executed bytes, timer_audit §4) */
#define TSRD1_ACK REG8(0xFFFFF5CDu)    /* ack: &= 0xEF (stock verbatim) */
#define TIERD1_EN REG8(0xFFFFF5CFu)    /* CMEAD10 enable: |= 0x10 (stock verbatim) */
#define TCRD1     REG16(0xFFFFF53Cu)   /* WORD; CKSEL1D1 = bits 6:4 (timer_audit §2) */
#define TSTRD     REG8(0xFFFFF500u)    /* byte, stock init 0x9718C */

/* TCRD init: stock-verbatim word 0x4000 (boot table @0x40F7C in two
 * independent original-stock dumps; written once at init 0x97152, never
 * again at runtime — timer_audit §1). bus 0 = PSCR0=19 → 1 MHz. */
#define TCRD1_INIT 0x4000u
/* OCR step in 256-count quanta (the <<8 idiom is hardware-mandated:
 * OCRD bits 7:0 don't exist, so tick = quanta x 256 bus clocks exactly).
 * Exact 1 ms is impossible (20 000 counts is not a multiple of 256 at any
 * integer PSCRn — timer_audit §3): 4 quanta x 256 µs = 1024 µs per tick. */
#define TICK_QUANTA 4u
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
    OCRD10 = (uint32_t)((((TCNT1D1 >> 8) + TICK_QUANTA) & 0x00FFFFFFu) << 8);
    s_tick_count++;
    return true;
}

void plat_tick_start(void)
{
    /* stock order (timer_audit §5: config → arm → enable → TSTRD → TDE
     * last, stock init 0x970FC / 0x96F28 flow) */
    TCRD1 = (uint16_t)TCRD1_INIT;               /* word store, stock 0x97152 */
    /* arm the first compare, clear any pending flag, enable, start */
    OCRD10 = (uint32_t)((((TCNT1D1 >> 8) + TICK_QUANTA) & 0x00FFFFFFu) << 8);
    TSRD1_ACK &= 0xEFu;                      /* CMFAD10 ack (stock byte idiom) */
    TIERD1_EN |= 0x10u;                      /* CMEAD10 enable (stock byte idiom) */
    TSTRD = 0x0Fu;   /* the stock starts ALL FOUR D-subblocks (init 0x9718C) */
    ATUENR |= ATUENR_TDE;                    /* TDE last (boot leaves it clear) */
}

/* the ISR body — called from the vector-table entry at slot 228 */
void plat_tick_isr(void)
{
    /* ack the compare-match (stock byte idiom) */
    TSRD1_ACK &= 0xEFu;
    /* re-arm: next compare = current + one 256-count quanta step */
    OCRD10 = (uint32_t)((((TCNT1D1 >> 8) + TICK_QUANTA) & 0x00FFFFFFu) << 8);
    s_tick_count++;
    s_tick_isr_count++;
    sched_tick_isr();                      /* bounded: timestamp + due scan */
}
