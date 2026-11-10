/* plat_walktmr.c — ATU-D0 UDID00 U-compare ISR for the walk timer (spec 22 §1,
 * spec 24 §2.1 vector 224, stock 0x9106C "the park/supervisor PWM-servo compare ISR").
 *
 * WALK-TIMER REGISTER MAP CORRECTED 2026-11-10 (bench r01n live reads + the
 * stock's own executed bytes — the old map below sat on the wrong compare
 * family and the walk never ticked: USE[DUTY]=USE[LEVEL]=0 after 16 min,
 * OCR 0xFFFFF590 never re-armed, TCNT1D0 1.6M×256 counts past it, flag
 * byte 0xFFFFF58D stuck 0x00 while byte 0xFFFFF58C held 0x30):
 *
 *   OLD (wrong — ATU-D0 C-compare channel 0, "CMID00", vector 216):
 *     OCR 0xFFFFF590, flag/ack byte 0xFFFFF58D bit 4, enable byte
 *     0xFFFFF58F bit 4, TSTRD 0xFFFFF500 bit 0. The stock's own
 *     multi-channel arming loop (0x90010) arms ch0's OCR but touches NO
 *     flag/enable bit for it (bVar6=0 for ch0 while ch1/ch2 get 0x20/0x40)
 *     — C-channel 0 does not deliver an interrupt on this silicon, and
 *     neither our ISR (vector 216) nor the poll ever saw a flag.
 *
 *   NEW (stock verbatim — ATU-D0 U-compare channel 0, "UDID00", vector 224):
 *     OCR      0xFFFFF5B0 word = remaining << 8          (programmer 0x6C0BA)
 *     TIER     0xFFFFF585 byte |= 0x01 (enable bit 0)    (0x6C0BA)
 *     TSR      0xFFFFF58C byte &= 0xFE (ack bit 0)       (ISR 0x9106C idiom;
 *                                                          servo twin 0x92060 bit 2)
 *     TSTR     0xFFFFF58E byte |= 0x01 (start bit 0)     (0x6C0BA)
 *              0xFFFFF58E byte &= 0xFE (stop bit 0)      (0x91058 walk stop)
 *     IPR10    0xFFFE0C0E nibble bits 7-4 = 6            (stock init 0x970FC
 *                                                          arms 6; 0x97198 → 8)
 *     vector   224 (stock table: 0x40070 + 4*224 -> 0x9106C)
 *
 * The compared counter is the subblock counter TCNT1D0 0xFFFFF520 (32-bit
 * register, only bits 31:8 compare — OCR writes are longwords with bits 7:0
 * = 0, the <<8 quanta law; 24-bit µs counter, TCNTA 0xFFFFF220 is its
 * low-half view). Rate = SUP_WALK_TICK_MS steps of WALK_QUANTA x 256 µs
 * (bus 0: 1024 µs per walk step). The ISR feeds the WDT first (stock law
 * 0x6C1BA), then sup_walk_isr() builds and queues the current walk job.
 */
#include "platform.h"
#include "regs.h"
#include "bsw/sup/sup_cal.h"

#ifndef SUP_WALK_TICK_MS
#define SUP_WALK_TICK_MS 1u
#endif

/* ATU-D0 subblock: counter + the U-compare control set (stock walk tick) */
#define TCNT1D0    REG32(0xFFFFF520u)
#define OCRDU00    REG32(0xFFFFF5B0u)   /* U-compare compare value, ch 0 (stock 0x6C0BA) */
#define TSRC_ACK   REG8(0xFFFFF58Cu)    /* TSR: U-compare status/ack, bit 0 (stock 0x9106C) */
#define TIERU_EN   REG8(0xFFFFF585u)    /* TIER: U-compare enable, bit 0 (stock 0x6C0BA) */
#define TSTRE_U    REG8(0xFFFFF58Eu)    /* TSTR: U-compare start/stop bit 0 (0x6C0BA / 0x91058) */
#define TSTRD      REG8(0xFFFFF500u)    /* counter starts, stock init 0x9718C = 0x0F */

/* TCRD0 0xFFFFF52C stays 0x4000 (stock boot table word, written once at
 * 0x97152; clock-bus 0). The counter runs from plat_tick_start's
 * TSTRD = 0x0F; this module only re-asserts it when arming. */
#define TCRD0_INIT 0x4000u
#define TCRD0      REG16(0xFFFFF52Cu)

/* OCR step in 256-count quanta (same <<8 hardware law as the tick timer):
 * SUP_WALK_TICK_MS x 4 quanta x 256 µs = 1024 µs per walk step on bus 0. */
#define WALK_QUANTA 4u
#define UCOMP_BIT   0x01u

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
    /* NO running-guard: the stock's programmers (0x6C0BA level / 0x6BF9C
     * duty twin) re-write ALL FOUR registers on EVERY call, and the stock's
     * steady state calls them every 5 ms pass (0x8799C → 0x8D020/0x8CEDC →
     * 0x6C802 → 0x6BF9C). Bench r01o proved why the guard is fatal: the
     * U-compare channel died mid-boot once (OCR@5B0 frozen at its last
     * re-arm, TIER@585 readback 0x00, TSR@58C bit0 never asserts again,
     * TCNT1D0 running 24-bit wraps past the stale compare forever) and the
     * guarded start() could never revive it — the walk sat silent for the
     * rest of the boot (USE[DUTY]=USE-LEVEL frozen, W1 never dispatched,
     * LVL_REPORTED stuck 7, the one-shot never fired). A compare event can
     * be lost for good (missed while TCNT is already past the OCR — the
     * next match would be a full 24-bit wrap away), so every arm must
     * restore the complete channel state, exactly like the stock. */
    /* stock order (timer_audit §5: config → arm → enable → start; the
     * programmer 0x6C0BA writes OCR@5B0, TIER@585|=1, then on the staged
     * transition TSR@58C&=0xFE + TSTR@58E|=1) */
    TCRD0 = (uint16_t)TCRD0_INIT;            /* word store, stock 0x97152 */
    OCRDU00 = next_ocr(TCNT1D0);
    TSRC_ACK &= (uint8_t)~UCOMP_BIT;         /* U-compare ack (stock 0x9106C idiom) */
    TIERU_EN |= UCOMP_BIT;                   /* U-compare enable (stock 0x6C0BA) */
    TSTRD = 0x0Fu;                           /* counter starts (stock init 0x9718C) */
    TSTRE_U |= UCOMP_BIT;                    /* U-compare start bit 0 (stock 0x6C0BA) */
    ATUENR |= ATUENR_TDE;                    /* TDE last (boot leaves it clear) */
    s_walk_tmr_running = true;
}

void plat_walktmr_stop(void)
{
    TSTRE_U &= (uint8_t)~UCOMP_BIT;          /* U-compare stop bit 0 (stock 0x91058) */
    s_walk_tmr_running = false;
}

uint32_t plat_walktmr_count(void)
{
    return TCNT1D0;                          /* P8 step 0: must advance when running */
}

/* superloop fallback (r01e): harvest the U-compare flag directly.
 * Mirror of the ISR body (WDT feed first — the stock law). */
bool plat_walktmr_poll(void)
{
    if ((TSRC_ACK & UCOMP_BIT) == 0u) {
        return false;
    }
    TSRC_ACK &= (uint8_t)~UCOMP_BIT;
    OCRDU00 = next_ocr(TCNT1D0);
    plat_wdt_kick();
    sup_walk_isr();
    return true;
}

/* the ISR body — called from the vector-table entry at slot 220 (VECTOR 224,
 * ATU-D0 UDID00 — the stock's walk tick vector; the stock table holds
 * 0x9106C at 0x40070 + 4*224) */
void plat_walktmr_isr(void)
{
    /* ack the compare-match (stock byte idiom, 0x9106C: bclr bit0 @0xFFFFF58C) */
    TSRC_ACK &= (uint8_t)~UCOMP_BIT;
    /* re-arm */
    OCRDU00 = next_ocr(TCNT1D0);
    /* WDT feed first (stock law: the walk service 0x6C1BA feeds before
     * dispatching — the walk's liveness IS the WDT's liveness) */
    plat_wdt_kick();
    /* dispatch the walk job */
    sup_walk_isr();
}
