/* plat_walktmr.c — ATU-D0 down-counter DCNTD00 underflow ISR for the walk
 * timer (spec 22 §1, spec 24 §2.1 vector 224 = "UDID00" — U = UNDERFLOW
 * interrupt D00, NOT a "U-compare": manual section 8 INTC table, vector
 * 224 row "UDID00" / IPR10 bits 7-4; ISR = stock 0x9106C).
 *
 * WALK-TIMER MAP CORRECTED AGAIN 2026-11-11 (r01s bench live reads + the
 * stock's own executed bytes; the 2026-11-10 "U-compare" map below had the
 * right ADDRESSES but the wrong REGISTER MODEL — every address sits in the
 * ATU-D0 DOWN-COUNTER register group, manual section 31 list verbatim):
 *
 *   DCNTD00  0xFFFFF5B0 32-bit — TIMER DOWN COUNTER D00 (manual §13.19.17,
 *            §31 list "Timer down counter D00 DCNTD00 H'FFFFF5B0"). 24-bit
 *            counter, bits 7:0 read 0. Counts DOWN on the TCRD0.DCSELDn0
 *            clock (TCRD0 = 0x4000 → bus 0 = 1 MHz → 1 count = 1 µs).
 *            HOLDS H'000000 on underflow and STOPS there (one-shot).
 *   DSTRD0   0xFFFFF585  8-bit — down counter STARTING register (§13.19.5).
 *            Bit 0 = DSTDn0 "write 1 → DCNTD00 starts". STROBE: "writing 0
 *            is ignored; this bit is always read as 0 even if 1 is written"
 *            — this is why arm @585 reads 0x00 forever on the bench (the
 *            write LANDS and starts the counter; the bit just never reads
 *            back). "When DCNTDn = H'000000, writing 1 to these bits has no
 *            effect" → the LOAD must precede the TRIGGER.
 *   TSRD0    0xFFFFF58C 16-bit — bit 8 = UDFDn0, the DCNTD00 UNDERFLOW flag
 *            (§13.19.8: bits 11:8 = UDFDn3..0; set on decrement-at-0, clear
 *            by writing 0). Stock ISR 0x9106C: bclr.b #0,@(12,r4) with
 *            r4 = 0xFFFFF580 → byte 0xF58C bit 0 = register bit 8. THIS IS
 *            the walk flag byte (0x9106C is vector 224's own ISR).
 *   TIERD0   0xFFFFF58E 16-bit — bit 8 = UDEDn0, DCNTD00 underflow interrupt
 *            enable (§13.19.9: bits 11:8 = UDEDn3..0). Stock arm 0x91004:
 *            bset.b #0,@(14,r5); stock stop 0x91058: bclr.b #0,@(14,r1).
 *   DSRD0    0xFFFFF587  8-bit — down counter STATUS: bit 0 = DSFDn0
 *            "counter enabled" (set on start, cleared on underflow-stop) —
 *            the bench discriminator for "is the countdown alive".
 *   DCRD0    0xFFFFF588 16-bit — TRGSELDn0[2:0] start/stop trigger select;
 *            stock init 0x97138 writes ch (=0) → compare-match triggers
 *            OFF, the counter is manual-start only (DSTRD0). Never a
 *            runtime write in the stock.
 *   IPR10    0xFFFE0C0E word, bits 7-4 = priority 6 (stock init 0x970FC
 *            writes IPR10 = 0x6665; 0x97198 raises to 8); vector 224
 *            (stock table 0x40070 + 4*224 → 0x9106C).
 *
 * THE 2026-11-10 REGRESSION, DECODED (why r01s launched each walk job
 * ~once per boot): the OLD code treated DCNTD00 as a COMPARE register and
 * re-armed it with next_ocr() = ((TCNT1D0>>8) + quanta)<<8 — the absolute
 * compare idiom. Written into a DOWN COUNTER that value means "count down
 * (TCNT_quanta mod 2^24)+4 quanta" — the counter became a TCNT SHADOW
 * (r01s bench: DCNTD00 tracks TCNT1D0 at offset −0.6..−2.3 s and RISES,
 * because the task-context restart law re-programs it every ~1-2 s with a
 * fresh absolute value). It can only underflow when TCNT_quanta is still
 * tiny — i.e. the FIRST arm of a boot → exactly one tick → USE[DUTY] and
 * USE[LEVEL] freeze at 1. The ISR's re-arm made it worse twice over: a
 * DCNTD00 write alone does NOT restart a terminated counter (manual: "not
 * decremented until the counter stopping source is activated"; DSRD0.DSFDn0
 * cleared by the underflow), and even while running a re-trigger is a
 * documented no-op — the stock NEVER relies on the ISR to re-arm; its
 * programmer (0x91004, the tail of 0x6C0BA) reloads DCNTD00 AND sets DSTDn0
 * every call:
 *   0x9102E  mov.l r2,@(48,r5)   DCNTD00 @0xFFFFF5B0 ← remaining << 8
 *   0x91030  bset.b #0,@(5,r5)   DSTRD0  @0xFFFFF585 |= 0x01 (START)
 *   0x91042  bclr.b #0,@(12,r5)   TSRD0   bit 8 clear (ack, first-time)
 *   0x91046  bset.b #0,@(14,r5)   TIERD0  bit 8 set   (enable, first-time)
 * (r01r-vs-r01s is therefore NOT a register-behavior diff: byte-identical
 * plat_walktmr.c, same first-tick-only law. r01r's ~35 s reset windows
 * spanned the first 3-4 terms of the doubling sequence tick→re-arm→tick at
 * ~2T, so its windows showed "repeated" launches; r01s's ~16 s windows cut
 * the sequence after the first term. 601029d is exonerated: it touched the
 * RSPI completion handoff, not the timer.)
 *
 * THE PORT LAW: the stock sustains the tick by re-programming from its 5 ms
 * pass; our walk FSM is tick-driven (no every-pass programmer), so the ISR
 * and the poll repeat the stock's own load+trigger pair after every
 * underflow — DCNTD00 = WALK_COUNTS, then DSTRD0 |= 1 — a self-sustaining
 * SUP_WALK_TICK_MS stream with the stock's exact idiom and order.
 */
#include "platform.h"
#include "regs.h"
#include "bsw/sup/sup_cal.h"

#ifndef SUP_WALK_TICK_MS
#define SUP_WALK_TICK_MS 1u
#endif

/* ATU-D0 subblock 0: the down-counter control set (stock walk tick) */
#define TCNT1D0    REG32(0xFFFFF520u)   /* subblock counter, 1 MHz (bus 0)  */
#define DCNTD00    REG32(0xFFFFF5B0u)   /* down counter D00 (stock 0x91004) */
#define DSTRD0     REG8(0xFFFFF585u)    /* START strobe bit 0 (0x91004; reads 0) */
#define DSRD0      REG8(0xFFFFF587u)    /* status: bit 0 = DSFDn0 "enabled" */
#define TSRD0_ACK  REG8(0xFFFFF58Cu)    /* TSRD0 bit 8 = UDFDn0 ack (0x9106C) */
#define TIERD0_EN  REG8(0xFFFFF58Eu)    /* TIERD0 bit 8 = UDEDn0 (0x91004/0x91058) */
#define TSTRD      REG8(0xFFFFF500u)    /* counter starts, stock init 0x9718C = 0x0F */

/* TCRD0 0xFFFFF52C stays 0x4000 (stock boot table word, written once at
 * 0x97152; OBREDn | CKSEL1Dn=0 and DCSELDn=000 → counter clocks = bus 0,
 * 1 MHz: TCNT1D0 and DCNTD00 both tick at 1 count/us). Re-written here
 * every arm (idempotent — the r01o dead-channel revival law keeps each arm
 * a full channel restore; the stock's one-time init 0x97138 leaves
 * DCRD0 = 0x0000 = compare triggers off, manual-start only). */
#define TCRD0_INIT 0x4000u
#define TCRD0      REG16(0xFFFFF52Cu)

/* Countdown, BENCH-CALIBRATED on r01t (2026-11-11): DCNTD00 drained the
 * r01t load (1024 counts) in ~112 µs — the down-counter's effective clock
 * is ~9.1 MHz observed (Pφ/2 = 10 MHz offered, one-in-flight gating at the
 * ~112 µs burst time delivered 8,890 launches/s with metronome deltas) —
 * NOT the 1 MHz the TCNT quanta law assumed. DCNTD00's own DCSELDn0 field
 * (TCRD0 bits?) is NOT yet decoded [D: TODO]; the load below is calibrated
 * to the STOCK'S WALK-PASS CADENCE (the stock's 5 ms programmer reload,
 * ~200 walk services/s): 0xB000 = 176 quanta = 45,056 counts ≈ 4.5 ms at
 * 10 MHz / 4.95 ms at 9.1 MHz — stock class either way, which un-starves
 * the cyclic batch (r01t's 9 k/s duty flood held NIB at 0 — the 84-class
 * silence law is the risk that cadence carries). Bits 7:0 don't exist
 * (24-bit law, manual §13.19.17); 0xB000 is <<8-clean. */
#define WALK_QUANTA  4u
#define UCOMP_DSTD   0x01u
#define WALK_COUNTS  0xB000u

/* forward: the walk service entry (sup_walk.c) */
extern void sup_walk_isr(void);

static bool s_walk_tmr_running;

/* the stock's load+trigger pair (0x9102E/0x91030): a nonzero DCNTD00 load
 * MUST precede the DSTDn0 trigger — "when DCNTDn = H'000000, writing 1 to
 * these bits has no effect" (manual §13.19.5). While running, the trigger
 * is a documented no-op and the load resets the remaining count (the
 * stock's steady-state reload law), so both orders of re-arm are safe. */
static void walktmr_arm_count(void)
{
    DCNTD00 = WALK_COUNTS;
    DSTRD0 |= UCOMP_DSTD;
}

bool plat_walktmr_running(void)
{
    return s_walk_tmr_running;
}

void plat_walktmr_start(void)
{
    /* NO running-guard: the stock's programmers re-write the whole channel
     * on EVERY call (0x91004 reloads + re-triggers every 5 ms pass; the
     * bench r01o proved a guarded start() could never revive a dead
     * channel). Order: config → load+trigger → ack → enable → counters. */
    TCRD0 = (uint16_t)TCRD0_INIT;            /* word store, stock 0x97152 */
    walktmr_arm_count();                     /* DCNTD00 load + DSTDn0 (stock 0x9102E/0x91030) */
    TSRD0_ACK &= (uint8_t)~UCOMP_DSTD;       /* UDFDn0 ack (stock 0x91042) */
    TIERD0_EN |= UCOMP_DSTD;                 /* UDEDn0 enable (stock 0x91046) */
    TSTRD = 0x0Fu;                           /* counter starts (stock init 0x9718C) */
    ATUENR |= ATUENR_TDE;                    /* TDE last (boot leaves it clear) */
    s_walk_tmr_running = true;
}

void plat_walktmr_stop(void)
{
    TIERD0_EN &= (uint8_t)~UCOMP_DSTD;       /* UDEDn0 disable — stock 0x91058 verbatim */
    s_walk_tmr_running = false;
}

uint32_t plat_walktmr_count(void)
{
    return TCNT1D0;                          /* P8 step 0: must advance when running */
}

/* superloop fallback (r01e): harvest the underflow flag directly.
 * Mirror of the ISR body (WDT feed first — the stock law). */
bool plat_walktmr_poll(void)
{
    if ((TSRD0_ACK & UCOMP_DSTD) == 0u) {
        return false;
    }
    TSRD0_ACK &= (uint8_t)~UCOMP_DSTD;       /* UDFDn0 ack (stock 0x9106C idiom) */
    walktmr_arm_count();                     /* reload + re-trigger */
    plat_wdt_kick();
    sup_walk_isr();
    return true;
}

/* the ISR body — called from the vector-table entry at index 220 (VECTOR
 * 224, ATU-D0 UDID00 = DCNTD00 underflow — the stock's walk tick vector;
 * the stock table holds 0x9106C at 0x40070 + 4*224) */
void plat_walktmr_isr(void)
{
    /* ack the underflow (stock byte idiom, 0x9106C: bclr bit0 @0xFFFFF58C
     * = TSRD0 bit 8 = UDFDn0) */
    TSRD0_ACK &= (uint8_t)~UCOMP_DSTD;
    /* re-arm: the stock never re-arms from the ISR — its 5 ms pass
     * programmer (0x91004) does the load+trigger; our tick-driven FSM has
     * no every-pass programmer, so the ISR repeats the stock's own pair
     * (walktmr_arm_count) to sustain SUP_WALK_TICK_MS. */
    walktmr_arm_count();
    /* WDT feed first (stock law: the walk service 0x6C1BA feeds before
     * dispatching — the walk's liveness IS the WDT's liveness) */
    plat_wdt_kick();
    /* dispatch the walk job */
    sup_walk_isr();
}
