/* plat_walktmr.c — ATU-D0 CMID00 compare ISR for the walk timer (design §5.4).
 *
 * The walk's own timer, independent of the 5 ms pass (the stock's v224
 * architecture), vector 216. Register map (timer_audit §5: manual §31 list
 * + manual §13.19.2 PROSE + the stock's executed bytes; the flag/enable ops
 * use the STOCK BYTE IDIOM — the "16-bit writes land mirrored" lore was a
 * misdiagnosis, see plat_tick.c / timer_audit §4):
 *   TSTRD  0xFFFFF500 byte = 0x0F (stock init 0x9718C)
 *   TCRD0  0xFFFFF52C WORD; CKSEL1D0 = bits 6:4 (NOT bits 8-6!); stock
 *          init value 0x4000 (bus 0 = 1 MHz), written once, never at runtime
 *   TSRD0  flag/ack via BYTE 0xFFFFF58D (bit 12 = CMFAD00)
 *   TIERD0 enable via BYTE 0xFFFFF58F |= 0x10 (bit 12 = CMEAD00)
 *   OCRD00 0xFFFFF590, TCNT1D0 0xFFFFF520 (32-bit; only bits 31:8 compare —
 *          the OCR step is quantized to 256 counts, timer_audit §3)
 * Rate = SUP_WALK_TICK_MS steps of WALK_QUANTA x 256 µs (bus 0: 1024 µs).
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
#define TSRD0_FLG REG8(0xFFFFF58Du)    /* CMFAD00 = bit 12 = bit 4 of this byte */
#define TSRD0_ACK REG8(0xFFFFF58Du)    /* ack: &= 0xEF (stock byte idiom) */
#define TIERD0_EN REG8(0xFFFFF58Fu)    /* CMEAD00 enable: |= 0x10 */
#define TCRD0     REG16(0xFFFFF52Cu)   /* WORD; CKSEL1D0 = bits 6:4 (timer_audit §2) */
#define TSTRD     REG8(0xFFFFF500u)    /* byte, stock init 0x9718C */

/* TCRD init: same stock-verbatim word as the tick timer (boot table @0x40F7C
 * is all-0x4000 for all six subblocks — timer_audit §1). bus 0 = 1 MHz. */
#define TCRD0_INIT 0x4000u
/* OCR step in 256-count quanta (same hardware law as the tick timer):
 * SUP_WALK_TICK_MS x 4 quanta x 256 µs = 1024 µs per walk step on bus 0. */
#define WALK_QUANTA 4u
#define CMFA00_BYTE 0x10u

/* forward: the walk service entry (sup_walk.c) */
extern void sup_walk_isr(void);

static bool s_walk_tmr_running;

static uint32_t next_ocr(uint32_t now)
{
    /* the stock's <<8 fraction convention (os_timer_rearm decode):
     * compare = (units + TCNT>>8) << 8; units are 256-count quanta
     * (OCRD bits 7:0 don't exist — timer_audit §3). */
    return (uint32_t)((((now >> 8) + (uint32_t)SUP_WALK_TICK_MS * WALK_QUANTA) & 0x00FFFFFFu) << 8);
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
    /* stock order (timer_audit §5: config → arm → enable → TSTRD → TDE
     * last, stock init 0x970FC / 0x96F28 flow) */
    TCRD0 = (uint16_t)TCRD0_INIT;            /* word store, stock 0x97152 */
    OCRD00 = next_ocr(TCNT1D0);
    TSRD0_ACK &= 0xEFu;                      /* CMFAD00 ack (stock byte idiom) */
    TIERD0_EN |= 0x10u;                      /* CMEAD00 enable (stock byte idiom) */
    TSTRD = 0x0Fu;   /* the stock starts all four D-subblocks */
    ATUENR |= ATUENR_TDE;                    /* TDE last (boot leaves it clear) */
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
