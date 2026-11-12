/* SH7254R platform layer: safe pin state, watchdog, 1 ms tick, CAN buses 1 and 2, ADC, speed inputs. */
#include "platform/platform.h"
#include <stddef.h>

#include "regs.h"

/* ---- Pins ----------------------------------------------------------------
 * Same two-step sequence as the stock firmware (spec 01 section 4):
 * 1. safe directions and levels (stock 0x961F0), 2. peripheral pin functions (stock 0x962E2). */
static void port_init(void)
{
    /* Ports A, B, C: general input, functions cleared */
    PACR1 = 0u; PACR2 = 0u; PACR3 = 0u; PACR4 = 0u; PAIOR = 0u; PADR = 0u; PAPSR = 0u;
    PBCR1 = 0u; PBCR2 = 0u; PBCR3 = 0u; PBCR4 = 0u; PBIOR = 0u; PBDR = 0u;
    PBIR = 0u; PBDSR = 0u; PBPSR = 0u;
    PCCR1 = 0u; PCCR2 = 0u; PCCR3 = 0u; PCCR4 = 0u; PCIOR = 0u; PCDR = 0u;
    PCPSR = 0u; PCIR = 0u; PCDSR = 0u;

    /* Ports D..L at 0xFFFFC800 (values from the stock initialisation) */
    PFC16(0x0E) = 0u;      PFC16(0x0C) = 0u;      PFC16(0x08) = 0x0010u;
    PFC16(0x00) = 0u;      PFC16(0x04) = 0u;
    PFC16(0x1E) = 0u;      PFC16(0x1C) = 0u;      PFC16(0x18) = 0u;
    PFC16(0x10) = 0u;      PFC16(0x14) = 0u;      PFC16(0x16) = 0u;
    PFC16(0x2E) = 0u;      PFC16(0x2C) = 0u;      PFC16(0x2A) = 0u;
    PFC16(0x20) = 0u;      PFC16(0x24) = 0u;      PFC16(0x26) = 0u;
    PFC16(0x28) = 0x8000u;
    PFC16(0x42) = 0u;      PFC16(0x40) = 0u;      PFC16(0x44) = 0u;
    PFC16(0x3C) = 0xFFFFu; PFC16(0x30) = 0xFFFFu; /* port G: all outputs, high (SPI CS idle) */
    PFC16(0x34) = 0u;      PFC16(0x36) = 0u;
    PFC16(0x58) = 0u;      PFC16(0x54) = 0u;      PFC16(0x50) = 0u;
    PJCR1 = 0u;            PJCR2 = 0u;            PFC16(0x74) = 0u;
    PJIOR = 0x0125u;       PJDR = 0x0125u;        /* PJ0/2/5/8 out, high: CAN TX recessive */
    PFC16(0x64) = 0u;      PFC16(0x66) = 0u;      PJPSR = 0x024Au;
    PFC16(0x92) = 0u;      PFC16(0x90) = 0u;
    PFC16(0x8C) = 0x061Bu; PFC16(0x80) = 0x061Bu; /* port K: SPI clock/MOSI idle high */
    PFC16(0x84) = 0u;      PFC16(0x86) = 0u;      PFC16(0x88) = 0x0824u;
    PFC16(0xAE) = 0u;      PFC16(0xAC) = 0u;
    PFC16(0xA8) = 0x01FCu; PFC16(0xA0) = 0u;      /* port L: PL2..PL8 out, low (PWM off) */
    PFC16(0xA4) = 0u;
    CKCR |= 0x01u;

    /* Peripheral pin functions (stock 0x962E2). Only CAN is used in M0, but the full set is
     * applied so that every pin is in the same state as under the stock firmware. */
    PFC16(0x0E) |= 0xFDD5u; PFC16(0x0C) |= 0x00FDu;
    PFC16(0x1E) |= 0x5000u; PFC16(0x1C) |= 0x0555u;
    PFC16(0x2E)  = 0xFFFFu; PFC16(0x2C) |= 0x0FFFu;
    PFC16(0x42) |= 0xAAAAu; PFC16(0x40) |= 0x00AAu;
    PFC16(0x58) |= 0x0FFFu;
    PJCR1 |= 0x14AFu;       PJCR2 |= 0x0005u;     /* CAN A/B/C, SCI A/B */
    PFC16(0x92) |= 0x0AAAu; PFC16(0x90) |= 0x00A8u;
    PFC16(0xAE) |= 0x5550u; PFC16(0xAC) |= 0x0001u;
}

/* ---- Watchdog (spec 05 section 2) ------------------------------------------ */
/* E28 (spec 03 section 6.2r): WTCR bit decode CORRECTED against the hardware
 * manual (R01UH0480EJ0400 s14.3.1) — bit 6 WT/IT: 1 = WATCHDOG (overflow =
 * internal reset via WRCR + a 64-cycle WDTOVF pin pulse), 0 = INTERVAL TIMER
 * (overflow = masked ITI interrupt only, no reset, no pin). The old comment
 * had the two modes swapped. The engaged stock's live WTCR reads 0x27 = bit 6
 * CLEAR = INTERVAL, TME=1, CKS=111 (spec 03 section 6.2p) — the stock disables
 * its own reset while engaged, and so do we now, from init. E27 proved the
 * watchdog mode's cost: the duty job + cyclic job + supervisor pass in one
 * 10 ms tick sum to ~20-25 ms against a ~15-30 ms service budget (the xfer
 * guard loops never service) — watchdog overflow, chip reset, grant lost.
 * Interval mode removes the internal reset entirely; a hang is recoverable
 * only by power cycle (storm catch works — bench build, accepted). */
static void wdt_init(void)
{
    WDT_WTCR  = 0xA547u;    /* stop (TME = 0) before changing the clock select */
    WDT_WRCR  = 0x5A80u;    /* kept: irrelevant in interval mode (no reset path) */
    WDT_WTCNT = 0x5AB7u;    /* first overflow after ~60 ms                     */
    WDT_WTCR  = 0xA567u;    /* the rebuild law (design §5.6): watchdog RESET mode,
                             * P/5120 — interval mode comes ONLY later, on the
                             * echo-confirmed enable edge (plat_wdt_mode) */
}

void plat_wdt_service(void)
{
    WDT_WTCNT = 0x5ADBu;    /* ~30 ms to overflow */
}

/* plat_wdt_mode moved to plat_wdt.c (the rebuild design §5.6 law: reset
 * mode default, interval ONLY on an echo-confirmed enable edge). */

/* ---- 1 ms tick: CMT0, P/8 = 2.5 MHz, compare 2499 --------------------------- */
static void tick_init(void)
{
    CMT_CMSTR &= (uint16_t)~0x0001u;
    CMT_CMCR0  = 0x00u;
    CMT_CMCNT0 = 0u;
    CMT_CMCOR0 = 2499u;
    CMT_CMSR0  = 0x00u;
    CMT_CMSTR |= 0x0001u;
}

bool plat_tick_poll(void)
{
    /* r01g: the CMT poll RESTORED as the fallback tick (the r01b-proven
     * path — the app works, UDS answers, the bench reads cells). The
     * main loop calls it ONLY until the ATU hardware tick proves live
     * (plat_tick_poll_hw succeeds once), then it self-retires. */
    if ((CMT_CMSR0 & 0x01u) != 0u) {
        CMT_CMSR0 = 0x00u;
        return true;
    }
    return false;
}

/* ---- CAN (RCAN-TL1, spec 05 section 1.1: 500 kbit/s) --------------------------
 * Bus 1 (CAN-C) on channel A, bus 2 (shifter / diagnostic bus) on channel C.
 * Mailboxes 1..7 transmit, 8..31 receive (one exact identifier each). Mailbox 0 unused. */
#define TX_MB_FIRST 1u
#define TX_MB_LAST  7u
#define RX_MB_FIRST 8u

static const uint32_t s_can_base[CAN_BUS_COUNT] = { RCAN_A_BASE, RCAN_C_BASE };
static uint8_t s_rx_next[CAN_BUS_COUNT] = { RX_MB_FIRST, RX_MB_FIRST };

static bool can_init(uint32_t b)
{
    uint32_t guard;

    RCAN_MCR(b) = RCAN_MCR_IDR | RCAN_MCR_RESET;
    for (guard = 100000u; guard != 0u && (RCAN_GSR(b) & RCAN_GSR_RESET) == 0u; guard--) {
    }
    RCAN_IMR(b)  = 0xFFFFu;             /* polled operation */
    RCAN_BCR1(b) = 0xE330u;             /* TSEG1 15, TSEG2 4, SJW 4 */
    RCAN_BCR0(b) = 0x0000u;             /* BRP 0 -> 500 kbit/s at 20 MHz */
    for (uint32_t n = 0u; n < RCAN_MAILBOXES; n++) {
        RCAN_MB_CTRL1(b, n) = RCAN_MBC_OFF;
    }
    RCAN_MCR(b) = RCAN_MCR_IDR;         /* leave reset: normal operation, ID reorder kept */
    for (guard = 100000u; guard != 0u && (RCAN_GSR(b) & RCAN_GSR_RESET) != 0u; guard--) {
    }
    return guard != 0u;
}

bool plat_can_send(can_bus_t bus, const can_frame_t *f)
{
    uint32_t b;

    if (bus >= CAN_BUS_COUNT || f->dlc > 8u) {
        return false;
    }
    b = s_can_base[bus];
    for (uint32_t mb = TX_MB_FIRST; mb <= TX_MB_LAST; mb++) {
        if ((RCAN_TXPR(b) & (1u << mb)) != 0u) {
            continue;                   /* still pending */
        }
        RCAN_MB_CTRL1(b, mb) = RCAN_MBC_OFF;
        RCAN_MB_CTRL0H(b, mb) = (uint16_t)((f->id & 0x7FFu) << 2);
        RCAN_MB_CTRL0L(b, mb) = 0u;
        for (uint8_t i = 0u; i < 8u; i++) {
            RCAN_MB_DATA(b, mb, i) = f->data[i];
        }
        RCAN_MB_CTRL1(b, mb) = (uint16_t)(RCAN_MBC_TX | f->dlc);
        RCAN_TXPR(b) = 1u << mb;
        return true;
    }
    return false;
}

/* RX registrations are collected and programmed in ascending identifier order by
 * plat_can_rx_commit().  The RCAN is left in ID-reorder mode (the boot's setting,
 * see can_init), and the reorder hardware expects a sorted mailbox table: an
 * unsorted table silently drops identifiers whose search path crosses an inversion
 * (bench 2026-11-03: 0x361 dead on both buses while every other registered id
 * received, with the table starting 0x7E1, 0x7DF, 0x361, 0x108, ...). */
static uint16_t s_rx_ids[CAN_BUS_COUNT][RCAN_MAILBOXES];
static uint8_t s_rx_n[CAN_BUS_COUNT];

bool plat_can_rx_register(can_bus_t bus, uint16_t id)
{
    if (bus >= CAN_BUS_COUNT || s_rx_n[bus] >= (uint8_t)(RCAN_MAILBOXES - RX_MB_FIRST)) {
        return false;
    }
    s_rx_ids[bus][s_rx_n[bus]++] = id;
    return true;
}

static void rx_mb_program(uint32_t b, uint32_t mb, uint16_t id)
{
    RCAN_MB_CTRL0H(b, mb) = (uint16_t)((id & 0x7FFu) << 2);   /* standard data frame */
    RCAN_MB_CTRL0L(b, mb) = 0u;
    RCAN_MB_LAFMH(b, mb) = 0u;                                  /* all identifier bits compared */
    RCAN_MB_LAFML(b, mb) = 0u;
    RCAN_MB_CTRL1(b, mb) = (uint16_t)(RCAN_MBC_RX | 8u);        /* NMC 0: newest frame kept */
}

void plat_can_rx_commit(void)
{
    for (uint8_t bus = 0u; bus < (uint8_t)CAN_BUS_COUNT; bus++) {
        uint32_t b = s_can_base[bus];
        uint16_t n = s_rx_n[bus];

        for (uint16_t i = 1u; i < n; i++) {                     /* insertion sort; n is small */
            uint16_t key = s_rx_ids[bus][i];
            uint16_t j = i;
            while (j > 0u && s_rx_ids[bus][j - 1u] > key) {
                s_rx_ids[bus][j] = s_rx_ids[bus][j - 1u];
                j--;
            }
            s_rx_ids[bus][j] = key;
        }
        for (uint16_t i = 0u; i < n; i++) {
            rx_mb_program(b, RX_MB_FIRST + i, s_rx_ids[bus][i]);
        }
        s_rx_next[bus] = (uint8_t)(RX_MB_FIRST + n);
    }
}

bool plat_can_recv(can_bus_t bus, can_frame_t *f)
{
    uint32_t b, pending;

    if (bus >= CAN_BUS_COUNT) {
        return false;
    }
    b = s_can_base[bus];
    pending = RCAN_RXPR(b);
    for (uint32_t mb = RX_MB_FIRST; mb < s_rx_next[bus]; mb++) {
        if ((pending & (1u << mb)) == 0u) {
            continue;
        }
        RCAN_RXPR(b) = 1u << mb;        /* clear first: a frame arriving during the copy
                                           sets it again and is read next time */
        f->id = (uint16_t)((RCAN_MB_CTRL0H(b, mb) >> 2) & 0x7FFu);
        f->dlc = (uint8_t)(RCAN_MB_CTRL1(b, mb) & 0x0Fu);
        if (f->dlc > 8u) {
            f->dlc = 8u;
        }
        for (uint8_t i = 0u; i < 8u; i++) {
            f->data[i] = RCAN_MB_DATA(b, mb, i);
        }
        return true;
    }
    return false;
}

/* ---- Analog inputs (spec 04) -----------------------------------------------------
 * Both converters run in continuous scan mode and the 1 ms task reads the latest result of
 * each channel. (The stock software uses timer-triggered scans with DMA and the external
 * multiplexer on AN0; neither is needed for the slow signals read here.) */
static void adc_init(void)
{
    ADCSR0 = 0x00u;
    ADCSR1 = 0x00u;
    ADANS0 = 0xFFFEu;                   /* AN1..AN15 (AN0 is the multiplexer input) */
    ADANS1 = 0x0FFFu;                   /* AN16..AN27 */
    ADANS3 = 0x007Fu;                   /* AN40..AN46 */
    ADCSR0 = ADCSR_ADCS | ADCSR_ADST;
    ADCSR1 = ADCSR_ADCS | ADCSR_ADST;
}

bool plat_adc_raw(uint8_t an, uint16_t *raw)
{
    if (an >= 1u && an <= 27u) {
        *raw = ADR0(an);
        return true;
    }
    if (an >= 40u && an <= 46u) {
        *raw = ADR1(an);
        return true;
    }
    return false;
}

/* ---- Speed inputs: ATU-III timer F12..F14, rotation speed mode (spec 02 4.1) -------
 * Register values as set by the stock firmware (0x96F28, 0x9733C, 0x97444): prescalers
 * 1 / 4 / 2.857 / 20 MHz, noise canceller mode 0x9F with filter 0x32, TCRF = 0x35
 * (clock bus 1 = 4 MHz, MDF 101, rising edge). Only the three speed channels are started. */
#define SPEED_FIRST_SUBBLOCK 12u

static void speed_init(void)
{
    ATU_PSCR(0) = 19u;
    ATU_PSCR(1) = 4u;
    ATU_PSCR(2) = 6u;
    ATU_PSCR(3) = 0u;
    ATU_NCMR = 0x9Fu;
    for (uint32_t i = 0u; i < SPEED_CHANNELS; i++) {
        const uint32_t n = SPEED_FIRST_SUBBLOCK + i;
        TCRF(n) = 0x35u;
        TIERF(n) = 0u;
        TSRF(n) = 0u;
        NCRFA(n) = 0x32u;
        NCCRF |= 1u << n;
        TSTRF |= 1u << n;
    }
    ATUENR |= ATUENR_PSCE | ATUENR_TFE;
}

bool plat_speed_read(uint8_t ch, speed_raw_t *r)
{
    const uint32_t n = SPEED_FIRST_SUBBLOCK + ch;

    if (ch >= SPEED_CHANNELS) {
        return false;
    }
    /* the edge count must not change across the read, so all values belong to one edge */
    for (uint8_t tries = 0u; tries < 4u; tries++) {
        const uint16_t e1 = ECNTBF(n);
        const uint32_t grd = GRDF(n);
        const uint32_t cnt = ECNTCF(n);
        if (ECNTBF(n) == e1) {
            r->edges = e1;
            r->t_edge = (grd >> 8) & SPEED_TIME_MASK;
            r->age = cnt >> 8;
            return true;
        }
    }
    return false;
}

/* ---- External supervisor (RSPI-B, SSLB1), CPU-driven as in stock 0x9210C / 0x1D430 ------
 * Bench 2026-10-27: keeping the RSPI enabled across frames wedges the CPU path after the
 * FIRST frame (tick counters sent=345 / ok=1: SPRF never sets again). The stock 5 ms
 * keep-alive task instead re-runs the FULL setup before every frame (boot ROM 0x1D430 each
 * tick from 0x1D0B4) and writes SPSR=0 after the SPDR write; the app's live-reply path is
 * A-DMAC only. So every frame here is stateless: setup -> SPDR -> poll SPRF -> read reply
 * (full duplex: the reply to THIS frame clocks in during its own transfer) -> SPSR=0. */

/* S9 (baseline review 2026-11-11): sup_pfc_mux REMOVED. Copied from
 * pf.c, it wrote 0xFFFFC840 (PGCR2, not PGCR1) |= 0xA000 — PG15/PG14 to
 * SSLA7/SSLA6 — and 0xC000 into the reserved bits 15:14 of PBCR4/PBCR1.
 * The stock keeps PG14/15 as GPIO outputs driven high (0x961F0: PGIOR =
 * PGDR = 0xFFFF) and its own pin-integrity check 0x9A148 requires PGCR2 ==
 * 0x00AA exactly (ours read 0xA0AA); the RSPI pins are the stock 0x962E2
 * set, already ported in port_init. */

static void sup_frame_setup(void)
{
    RSPIB_DMA_REQ0 = 0u;
    RSPIB_DMA_REQ1 = 0u;
    SPSRB = 0u;
    SPPCRB = 0u;
    SPBRB = 5u;                         /* P/12 */
    SPSCRB = 0u;                        /* one command (SPCMD0) */
    SPCKDB = 0u;
    SSLNDB = 0u;
    SPNDB = 1u;
    SPCMDB0 = 0xAF11u;                  /* SSLB1, 16 bit, MSB first, mode 1 */
    SPCRB = 0x48u;                      /* enable, master; SPTEF=1 after init */
}

void plat_sup_init(void)
{
    sup_frame_setup();
}

uint8_t plat_sup_exchange(uint16_t tx, uint16_t *rx)
{
    uint32_t guard;
    uint8_t r = 1u;

    sup_frame_setup();
    if ((SPSRB & SPSR_SPTEF) == 0u) {
        return 0u;                      /* not possible after a fresh setup */
    }
    SPDRB = tx;
    for (guard = 20000u; guard != 0u && (SPSRB & SPSR_SPRF) == 0u; guard--) { }
    if ((SPSRB & SPSR_SPRF) != 0u) {
        *rx = SPDRB;
        r |= 2u;
    }
    SPSRB = 0u;
    return r;
}

bool plat_sup_xfer(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    /* Stock-job transport: the same full-setup + start-trigger discipline as the driver
     * exchange (plat_drv_xfer), with the stock supervisor two-command sequence SPCMD0 =
     * 0xAF11 (SSLB1) and SPCMD1 = 0xAF01 (SSLB0) so the frames alternate between the two
     * RSPI-B devices exactly as the stock cyclic jobs 0x40BE8/0x40C0C/0x40C54 do
     * (spec 03 section 4.4). */
    /* Stock-job transport: the same full-setup + start-trigger discipline as the driver
     * exchange (plat_drv_xfer), with the stock supervisor two-command sequence SPCMD0 =
     * 0xAF11 (SSLB1) and SPCMD1 = 0xAF01 (SSLB0) so the frames alternate between the two
     * RSPI-B devices exactly as the stock cyclic jobs 0x40BE8/0x40C0C/0x40C54 do
     * (spec 03 section 4.4). */
    SPCRB = 0x00u;
    RSPIB_DMA_REQ0 = 0u;
    RSPIB_DMA_REQ1 = 0u;
    SPSRB = 0u;
    SPPCRB = 0u;
    SPBRB = 5u;                         /* P/12 as the stock cfg block 0x40D66 */
    SPCKDB = 0u;
    SSLNDB = 0u;
    SPNDB = 1u;
    SPSCRB = 1u;                        /* sequence of two commands */
    SPCMDB0 = 0xAF11u;                  /* SSLB1: the supervisor */
    SPCMDB1 = 0xAF01u;                  /* SSLB0: the second system chip */
    SPCRB = 0x48u;                      /* enable, master */
    for (uint8_t i = 0u; i < n; i++) {
        uint32_t guard;
        for (guard = 20000u; guard != 0u && (SPSRB & SPSR_SPTEF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        (void)SPSRB;                    /* read SPTEF = 1: the SPDR write prerequisite */
        SPDRB = tx[i];
        (void)SPSRB;
        SPSRB = 0u;                     /* start trigger (manual 17.4.9) */
        for (guard = 20000u; guard != 0u && (SPSRB & SPSR_SPRF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        rx[i] = SPDRB;
        SPSRB = (uint8_t)(SPSRB & SPSR_SPTEF);
    }
    return true;
}

bool plat_sup_burst2(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    /* SSLB2 variant: the third RSPI-B device uses its own select AND SPI mode 0
     * (SPCMD 0xAF20, cpol0 cpha0 - stock job 0x40C74 with cfg 0x40D84). */
    SPCRB = 0x00u;
    RSPIB_DMA_REQ0 = 0u;
    RSPIB_DMA_REQ1 = 0u;
    SPSRB = 0u;
    SPPCRB = 0u;
    SPBRB = 5u;
    SPCKDB = 0u;
    SSLNDB = 0u;
    SPNDB = 1u;
    SPSCRB = 0u;
    SPCMDB0 = 0xAF20u;                  /* SSLB2, mode 0 */
    SPCRB = 0x48u;
    for (uint8_t i = 0u; i < n; i++) {
        uint32_t guard;
        for (guard = 20000u; guard != 0u && (SPSRB & SPSR_SPTEF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        (void)SPSRB;
        SPDRB = tx[i];
        (void)SPSRB;
        SPSRB = 0u;
        for (guard = 20000u; guard != 0u && (SPSRB & SPSR_SPRF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        rx[i] = SPDRB;
        SPSRB = (uint8_t)(SPSRB & SPSR_SPTEF);
    }
    return true;
}

bool plat_sup_xfer5(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    /* Five-select rotation on RSPI-B (the stock driver jobs, cfg 0x40CE4):
     * SPCMD0..4 = SSLB3/SSLB2/SSLB1/SSLB0/SSLB5, one command per frame in
     * sequence — the supervisor's select sits IN the rotation, so it witnesses
     * every driver-register word (spec 03, job registry 2026-11-01). */
    SPCRB = 0x00u;
    RSPIB_DMA_REQ0 = 0u;
    RSPIB_DMA_REQ1 = 0u;
    SPSRB = 0u;
    SPPCRB = 0u;
    SPBRB = 5u;
    SPCKDB = 0u;
    SSLNDB = 0u;
    SPNDB = 1u;
    SPSCRB = 4u;                        /* sequence of five commands */
    SPCMDB(0) = 0xAF31u;                /* SSLB3 */
    SPCMDB(1) = 0xAF21u;                /* SSLB2 */
    SPCMDB(2) = 0xAF11u;                /* SSLB1: the supervisor */
    SPCMDB(3) = 0xAF01u;                /* SSLB0 */
    SPCMDB(4) = 0xAF51u;                /* SSLB5 */
    SPCRB = 0x48u;                      /* enable, master */
    for (uint8_t i = 0u; i < n; i++) {
        uint32_t guard;
        for (guard = 20000u; guard != 0u && (SPSRB & SPSR_SPTEF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        (void)SPSRB;
        SPDRB = tx[i];
        (void)SPSRB;
        SPSRB = 0u;
        for (guard = 20000u; guard != 0u && (SPSRB & SPSR_SPRF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        rx[i] = SPDRB;
        SPSRB = (uint8_t)(SPSRB & SPSR_SPTEF);
    }
    return true;
}

bool plat_sup_xfer4(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    /* Four-select rotation for driver WRITES (SSLB3/2/0/5 - NO supervisor):
     * a fresh setup restarts the command sequence at SPCMD0, so with the
     * supervisor in a write rotation its select would receive register data
     * shaped like control words (0x8E55 reset the TCU, bench 2026-11-01).
     * Writes go to the chips only; the supervisor stays in the read rotation. */
    SPCRB = 0x00u;
    RSPIB_DMA_REQ0 = 0u;
    RSPIB_DMA_REQ1 = 0u;
    SPSRB = 0u;
    SPPCRB = 0u;
    SPBRB = 5u;
    SPCKDB = 0u;
    SSLNDB = 0u;
    SPNDB = 1u;
    SPSCRB = 3u;                        /* sequence of four commands */
    SPCMDB(0) = 0xAF31u;                /* SSLB3 */
    SPCMDB(1) = 0xAF21u;                /* SSLB2 */
    SPCMDB(2) = 0xAF01u;                /* SSLB0 */
    SPCMDB(3) = 0xAF51u;                /* SSLB5 */
    SPCRB = 0x48u;
    for (uint8_t i = 0u; i < n; i++) {
        uint32_t guard;
        for (guard = 20000u; guard != 0u && (SPSRB & SPSR_SPTEF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        (void)SPSRB;
        SPDRB = tx[i];
        (void)SPSRB;
        SPSRB = 0u;
        for (guard = 20000u; guard != 0u && (SPSRB & SPSR_SPRF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        rx[i] = SPDRB;
        SPSRB = (uint8_t)(SPSRB & SPSR_SPTEF);
    }
    return true;
}

bool plat_sup_burst(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    /* Single-select burst (SSLB1 only, one command), otherwise the same exchange
     * discipline as plat_sup_xfer. */
    SPCRB = 0x00u;
    RSPIB_DMA_REQ0 = 0u;
    RSPIB_DMA_REQ1 = 0u;
    SPSRB = 0u;
    SPPCRB = 0u;
    SPBRB = 5u;
    SPCKDB = 0u;
    SSLNDB = 0u;
    SPNDB = 1u;
    SPSCRB = 0u;                        /* one command: every frame on SPCMD0 */
    SPCMDB0 = 0xAF11u;                  /* SSLB1: the supervisor */
    SPCRB = 0x48u;
    for (uint8_t i = 0u; i < n; i++) {
        uint32_t guard;
        for (guard = 20000u; guard != 0u && (SPSRB & SPSR_SPTEF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        (void)SPSRB;
        SPDRB = tx[i];
        (void)SPSRB;
        SPSRB = 0u;                     /* start trigger (manual 17.4.9) */
        for (guard = 20000u; guard != 0u && (SPSRB & SPSR_SPRF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        rx[i] = SPDRB;
        SPSRB = (uint8_t)(SPSRB & SPSR_SPTEF);
    }
    return true;
}

bool plat_sup_burst0(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    /* SSLB0 single-select burst: SPCMD 0xAF01 (16 bit, mode 1), the select of the odd
     * frames in the two-command jobs - the stock job 0x40BC0's cfg 0x40B76. Otherwise
     * the same exchange discipline as plat_sup_burst. */
    SPCRB = 0x00u;
    RSPIB_DMA_REQ0 = 0u;
    RSPIB_DMA_REQ1 = 0u;
    SPSRB = 0u;
    SPPCRB = 0u;
    SPBRB = 5u;
    SPCKDB = 0u;
    SSLNDB = 0u;
    SPNDB = 1u;
    SPSCRB = 0u;                        /* one command: every frame on SPCMD0 */
    SPCMDB0 = 0xAF01u;                  /* SSLB0: the second system chip */
    SPCRB = 0x48u;
    for (uint8_t i = 0u; i < n; i++) {
        uint32_t guard;
        for (guard = 20000u; guard != 0u && (SPSRB & SPSR_SPTEF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        (void)SPSRB;
        SPDRB = tx[i];
        (void)SPSRB;
        SPSRB = 0u;                     /* start trigger (manual 17.4.9) */
        for (guard = 20000u; guard != 0u && (SPSRB & SPSR_SPRF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        rx[i] = SPDRB;
        SPSRB = (uint8_t)(SPSRB & SPSR_SPTEF);
    }
    return true;
}

/* ---- Solenoid driver chips: RSPI-A, CPU-driven jobs (spec 03 sections 2-4) ----------------
 * Same bus settings as the stock job start (0x96468 with descriptor 0x40CD8): P/12, no extra
 * delays, a five-command sequence SSLA3, SSLA2, SSLA1, SSLA0, SSLA5 (16 bit, MSB first, mode 1),
 * so frame n goes to chip n mod 5. Each job restarts the sequence by toggling SPE. */
static const uint16_t s_drv_cmd[5] = { 0xAF31u, 0xAF21u, 0xAF11u, 0xAF01u, 0xAF51u };
static uint8_t s_drv_spbr = 5u;   /* declared before first use */

/* E59 (spec 03 section 6.2aq): the interrupt-driven frame feed. The buffers
 * and index are shared between plat_drv_xfer_irq and plat_drv_xfer (below;
 * sol_drv.c drives both). The mainline fills the buffer and starts the
 * burst; plat_drv_xfer_irq feeds frames on interrupt; the mainline reads
 * replies after the burst completes (count returns to 0). */
volatile uint16_t rspi_a_tx_buf[64];
volatile uint16_t rspi_a_rx_buf[64];
volatile uint8_t  rspi_a_idx;
volatile uint8_t  rspi_a_count;

/* E60b: the staged arm. E60 booted dead-on-CAN with no heartbeat at all — the
 * death is inside the first interrupt delivery (or init). The window stays
 * closed until the scheduler has proven life for a second (heartbeat flowing);
 * arming live then separates "init died" from "delivery kills the app" on the
 * bus, with the heartbeat's last frame as the timestamp. */
static volatile bool s_drv_irq_live;

void plat_drv_irq_arm(bool on)
{
    s_drv_irq_live = on;
}

bool plat_drv_xfer_irq(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    uint32_t sr;
    uint32_t guard;
    bool ok;

    if (!s_drv_irq_live || n > 64u || n == 0u) {
        return false;
    }

    __asm__ volatile ("stc sr, %0" : "=r"(sr));
    __asm__ volatile ("ldc %0, sr" : : "r"(sr | 0xF0u));   /* mask during staging */

    /* copy the frames into the shared buffer */
    for (uint8_t i = 0u; i < n; i++) {
        rspi_a_tx_buf[i] = tx[i];
        rspi_a_rx_buf[i] = 0u;
    }
    rspi_a_idx = 0u;
    rspi_a_count = n;

    /* the proven polled-path peripheral init */
    SPCRA = 0x00u;
    RSPIA_DMA_REQ0 = 0u;
    RSPIA_DMA_REQ1 = 0u;
    SPSRA = 0u;
    SPPCRA = 0u;
    SPBRA = s_drv_spbr;
    SPCKDA = 0u;
    SSLNDA = 0u;
    SPNDA = 1u;
    SPSCRA = 4u;
    for (uint32_t i = 0u; i < 5u; i++) {
        SPCMDA(i) = s_drv_cmd[i];
    }
    *(volatile uint8_t *)0xFFFE6030u = 0xFCu;              /* ADMATE0 bus-A burst arm
                                        * (platform_audit #7/P0: bus-A contexts write
                                        * 0xFC — launcher 0x96584, burst ISR 0x965B4;
                                        * 0xF3 is the bus-B value) */

    /* E60: the interrupt plumbing — RSPI_A is the ONLY enabled source
     * (plat_drv_init zeroed every IPR), priority 0xE in IPR27 bits 11-8 */
    RSPIA_IPR = (uint16_t)((RSPIA_IPR & ~0x0F00u) | 0x0E00u);

    /* arm the master with the receive interrupt (manual 17.3.1: bit7 SPRIE,
     * bit6 SPE, bit3 MSTR — the E59 value 0x09 had SPE clear and never clocked) */
    SPCRA = 0xC8u;

    /* start frame 0 exactly like the polled loop (manual 17.4.9 CPU trigger) */
    for (guard = 20000u; guard != 0u && (SPSRA & SPSR_SPTEF) == 0u; guard--) {
    }
    ok = (guard != 0u);
    if (ok) {
        (void)SPSRA;
        SPDRA = rspi_a_tx_buf[0];
        (void)SPSRA;
        SPSRA = 0u;                                        /* start trigger */
        rspi_a_idx = 1u;

        /* open the SR window: the ISR feeds frames 1..n-1 as each SPRF lands.
         * E60i: guard = 100000 (~4 ms). The E59 value (500000, ~20 ms)
         * equaled the supervisor's stall-reset threshold by itself — ONE hung
         * burst could delay the bus-B mirror past the E28 enforcement limit
         * and hard-reset the MCU (the leading mechanism for the 20:09:58
         * reset, spec 03 6.2aw). A healthy 45-frame burst completes in
         * ~300 µs; 4 ms is generous and can never approach 20 ms doubled. */
        __asm__ volatile ("ldc %0, sr" : : "r"(sr & ~0xF0u));
        for (guard = 100000u; guard != 0u && rspi_a_count != 0u; guard--) {
        }
        __asm__ volatile ("ldc %0, sr" : : "r"(sr));       /* restore the mask */
        ok = (guard != 0u);
    }

    SPCRA = 0x08u;                                         /* disarm SPRIE, keep MSTR */
    if (!ok) {
        rspi_a_count = 0u;
        return false;
    }
    /* copy the replies back */
    for (uint8_t i = 0u; i < n; i++) {
        rx[i] = rspi_a_rx_buf[i];
    }
    return true;
}

/* E54 (spec 03 section 6.2aj): the driver-bus bit rate, live-settable from the bench.
 * The stock's RSPI-A transport parameters are the writes-don't-stick suspect; the
 * stock's RSPI-A init writes SPBRA from a per-bus config object (worker 0x96468: config+0 ->
 * base+0x0A). Our 5 was chosen, never decoded for bus A — and driver chips often
 * latch writes only below a max clock. plat_drv_spbr_set() takes effect at the next
 * exchange (the full re-init per exchange re-writes it). */
void plat_drv_spbr_set(uint8_t spbr)
{
    if (spbr != 0u) {           /* 0 = leave as-is */
        s_drv_spbr = spbr;
    }
}

void plat_drv_init(void)
{
    /* E60: interrupt hygiene. The boot leaves its INTC armed when the
     * application starts (live read 2026-11-08: ATU-III at priority 7 in
     * IPR16-19, the WDT ITI at 0xE and CMT nibbles in IPR05). Any of those
     * would vector to default_handler — a hang — the first time SR.I drops.
     * Zero every IPR, then give RSPI_A its priority; xfer_irq re-asserts the
     * RSPI nibble before each burst. */
    for (uint32_t i = 3u; i <= 29u; i++) {
        INTC_IPR(i) = 0u;
    }
    RSPIA_IPR = (uint16_t)((RSPIA_IPR & ~0x0F00u) | 0x0E00u);
    /* E60e: register banks OFF — the bank-overflow killer (spec 03 6.2as).
     * E60d's exception landed with the interrupted PC in the guard loop and
     * SR.I raised to the RSPI's own priority — the bank-overflow signature:
     * the boot enables banked interrupt entry, GCC's rte never RESBANKs, and
     * the 16th interrupt (id_poll's 10 + the cyclic's first frames) exhausted
     * all 15 banks. Pure stack handling from here on. */
    INTC_IBNR = 0x0000u;
    /* E60f: arm the feed from init — the rail window (~320 ms) needs the
     * cyclic flowing from the first 10 ms tick; the staged 1 s arm was a
     * diagnostic-only safety (spec 03 6.2at) */
    s_drv_irq_live = true;

    SPCRA = 0x00u;
    RSPIA_DMA_REQ0 = 0u;
    RSPIA_DMA_REQ1 = 0u;
    SPSRA = 0u;
    SPPCRA = 0u;
    SPBRA = s_drv_spbr;
    SPCKDA = 0u;
    SSLNDA = 0u;
    SPNDA = 1u;
    SPSCRA = 4u;                        /* sequence of five commands */
    for (uint32_t i = 0u; i < 5u; i++) {
        SPCMDA(i) = s_drv_cmd[i];
    }
}

#define DRV_WAIT_LOOPS 2000u            /* far above one 16-bit frame at P/12 */

bool plat_drv_xfer(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    /* Full re-init before EVERY exchange, the discipline the RSPI needs to actually clock
     * (bench 2026-10-30, FE12 diagnostic: with only an SPCR toggle the peripheral enables
     * and accepts the write but never shifts; with the full setup each frame completes —
     * the same lesson as the RSPI-B supervisor transport, spec 03 section 4.4). */
    SPCRA = 0x00u;
    RSPIA_DMA_REQ0 = 0u;
    RSPIA_DMA_REQ1 = 0u;
    SPSRA = 0u;
    SPPCRA = 0u;
    SPBRA = s_drv_spbr;
    SPCKDA = 0u;
    SSLNDA = 0u;
    SPNDA = 1u;
    SPSCRA = 4u;
    for (uint32_t i = 0u; i < 5u; i++) {
        SPCMDA(i) = s_drv_cmd[i];
    }
    SPCRA = 0x48u;                      /* E55 verdict (6.2ak): 0x08 (the stock's live value) zeroes BOTH buses with our polled transport — bit 6 is load-bearing for polled framing; the stock's 0x08 belongs to its interrupt/DMA transaction engine. Restore until the DMA transport exists. */
    for (uint8_t i = 0u; i < n; i++) {
        uint32_t guard;
        for (guard = DRV_WAIT_LOOPS; guard != 0u && (SPSRA & SPSR_SPTEF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        (void)SPSRA;                    /* read SPTEF = 1: the SPDR write prerequisite */
        SPDRA = tx[i];
        /* Start trigger (manual 17.4.9): a CPU transfer begins when SPTEF is cleared by
         * the read-1-then-write-0 path after the SPDR write — the peripheral does not
         * auto-start from the write alone (bench 2026-10-30, FE12 variants 1 vs 2). */
        (void)SPSRA;
        SPSRA = 0u;
        for (guard = DRV_WAIT_LOOPS; guard != 0u && (SPSRA & SPSR_SPRF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        rx[i] = SPDRA;
        SPSRA = (uint8_t)(SPSRA & SPSR_SPTEF);  /* clear SPRF / error flags */
    }
    return true;
}

/* plat_drv_xfer_stock deleted (platform_audit #12/P1#7): its claimed decode
 * doesn't check out (stock 0x93C30 is a RAM buffer copy loop, no packed
 * register store), SPCR=0x09 contradicts the project's own SPE=bit6 law
 * (E60 proved 0x09 never clocks), and the 32-bit packed store clobbers
 * SPPCR (+2) as a side effect. It had no callers. */

/* ---- Park hold output: timer E sub-block 3 channel 1 = TOE31 on PL6 ----------------
 * (spec 02 section 3.1, manual 13.22 / 22.1.22). 2 MHz count clock (clock-bus line 1,
 * PSCR1 = 4 -> 4 MHz, PSCRE3 = 1 -> divide by 2), cycle 2000 ticks = 1 kHz, counter
 * offset 1452 as stock (staggered start). Duty: the output is high from cycle start and
 * falls at the DTR match, so with TONE = 0 (no inversion) the pin-high fraction is
 * DTR/CYLR and DTR = 0 keeps the pin low — the same level as the port-mode safe state,
 * which is why the pin function is switched last.
 * Stock drives this output with inversion (polarity 1) and a battery-compensated duty;
 * the project starts with the non-inverted mapping (energisation permille -> pin-high
 * permille) and the FE11 commissioning step measures the actual magnet current in both
 * states before any change (docs/firmware/01-m4-range-park.md section 8). */
#define TOE31_CYCLE_TICKS 2000u
#define TOE31_COUNTER_OFFSET 1452u

void plat_toe_out_init(void)
{
    /* Everything is configured while PL6 is still a port pin driven low (port_init). */
    ATU_PSCR(1) = 4u;                   /* clock-bus line 1: P/(4+1) = 4 MHz          */
    TCRE3   = 0x01u;                    /* CKSELE3 = 001: counter from line 1         */
    PSCRE3  = 0x01u;                    /* PSCE3 = 1: divide by 2 -> 2 MHz            */
    TOCRE3  = 0x00u;                    /* TONEE3 = 0: not inverted (see above)       */
    RLDCRE3 = RLDCRE3_RLD1;             /* reload DTR/CYLR from buffers at cycle match */
    CYLRE31 = TOE31_CYCLE_TICKS;
    CRLDE31 = TOE31_CYCLE_TICKS;
    DTRE31  = 0u;                       /* duty 0: pin low = output off               */
    DRLDE31 = 0u;
    TCNTE31 = TOE31_COUNTER_OFFSET;
    SSTRE3 |= SSTRE3_SSTR1;             /* start channel 1 counting                   */
    TSTRE  |= TSTRE_STRE3;              /* start sub-block 3                          */
    ATUENR |= ATUENR_PSCE | ATUENR_TEE; /* prescalers and timer E running             */
    /* Switch the pin to its timer function only now: the PWM is already outputting
     * "low", so the level never changes. */
    PLCR1 = (uint16_t)((PLCR1 & ~PLCR1_PL6MD_TOE31) | PLCR1_PL6MD_TOE31);
}

void plat_toe_out_set(uint16_t permil)
{
    uint32_t d;

    if (permil > 1000u) {
        permil = 1000u;
    }
    d = ((uint32_t)permil * TOE31_CYCLE_TICKS) / 1000u;
    if (d >= TOE31_CYCLE_TICKS) {
        d = TOE31_CYCLE_TICKS - 1u;     /* DTR must stay below CYLR */
    }
    DRLDE31 = (uint16_t)d;              /* takes effect at the next cycle match */
}

/* ---- Memory read for the measurement protocol ---------------------------------- */
static bool readable(uint32_t a, uint16_t n)
{
    const uint32_t end = a + n - 1u;

    if (n == 0u || end < a) {
        return false;
    }
    return end <= 0x001FFFFFu ||
           (a >= 0xFFF80000u && end <= 0xFFF9FFFFu) ||
           (a >= 0xFFFC0000u && end <= 0xFFFFFFFFu);   /* m4v: I/O registers —
            * the stock's boot-handshake mailbox (0xFFFC0D00/D04, spec 03 §6.2),
            * module-stop state (0xFFFE080x) and the chip ID (0xFFFF1000) become
             * readable from the bench (read-only; polled app, no r-t-c risks) */
}

bool plat_mem_read(uint32_t addr, uint8_t *out, uint16_t n)
{
    if (!readable(addr, n)) {
        return false;
    }
    for (uint16_t i = 0u; i < n; i++) {
        out[i] = *(const volatile uint8_t *)(uintptr_t)(addr + i);
    }
    return true;
}

/* ---- Mechatronic counterpart boot mailbox (spec 03 section 6.2) --------------------------
 * The stock's own sequence, disassembled 2026-11-04: trigger 0x67200 reads 0xFFFC0D00 and
 * only 9/10 (letter 85) or 13/14 (letter 86) proceed — every other value stores letter 0
 * and never writes the magic. The letter jobs write 0x1234 to 0xFFFC0D04; sibling 0x6728E
 * polls for the counterpart's clear and answers with 0x5555 (letter 85) / 0x3737 (86). */
#define MB_IN_REG    REG16(0xFFFC0D00u)
#define MB_BOX_REG   REG16(0xFFFC0D04u)
#define MB_WAIT_TICKS 300u          /* 300 x 100 ms = 30 s clear-wait bound (stock: 667) */

void plat_stock_init_mirror(void)
{
    /* stock 0x9533C, in its order, the mailbox-relevant subset (2026-11-04 disassembly):
     * the module-stop protocol is a KEY write at 0xFFFF0802 followed by the VALUE at
     * 0xFFFE0802 - two registers, which the section 6.2 prose had conflated. */
    REG8(0xFFFE0420u) = (uint8_t)(REG8(0xFFFE0420u) | 0x40u);
    REG8(0xFFFE0430u) = 0xE0u;
    REG16(0xFFFF0812u) = 0x7821u;            /* chip ID 0x0021 & 3 == 0 -> 0x7821 */
    REG16(0xFFFF0802u) = 0x69FFu;            /* module-stop key */
    REG16(0xFFFF0842u) = 0x68FFu;
    REG16(0xFFFE0802u) = 0x5555u;            /* module-stop value (the suspect) */
    REG32(0xFFFC1400u) = 1u;                 /* RCCR: ROM cache enable */
    REG32(0xFFFC1408u) = 0xFFFFFFF5u;        /* cache flush */
    REG16(0xFFFE0400u) = (uint16_t)(REG16(0xFFFE0400u) | 0x3C10u);
}

static plat_mb_status_t s_mb;
static uint16_t s_mb_wait;
static uint16_t s_mb_hist[512];          /* input word, one sample per ms from start-up */
static uint16_t s_mb_hist_n;
static bool s_mb_started;                /* capture() ran; later valid values may still fire */

static bool mb_try_letter(uint16_t input)
{
    if (s_mb.letter != 0u || (s_mb.phase != PLAT_MB_IDLE && s_mb.phase != PLAT_MB_NO_HANDSHAKE)) {
        return false;                    /* already fired or already running */
    }
    switch (input) {
    case 9u:
    case 10u:
        s_mb.letter = 0x55u;
        break;
    case 13u:
    case 14u:
        s_mb.letter = 0x56u;
        break;
    default:
        return false;
    }
    /* valid letter: the stock writes the request magic immediately in the trigger */
    MB_BOX_REG = 0x1234u;
    s_mb.mailbox = 0x1234u;
    s_mb.phase = PLAT_MB_WAIT_CLEAR;
    s_mb_wait = MB_WAIT_TICKS;
    return true;
}

void plat_mb_capture(void)
{
    s_mb_started = true;
    s_mb.input_capture = MB_IN_REG;
    s_mb.mailbox_capture = MB_BOX_REG;
    s_mb.mailbox = s_mb.mailbox_capture;
    if (!mb_try_letter(s_mb.input_capture)) {
        s_mb.phase = PLAT_MB_NO_HANDSHAKE;   /* bench: 0x000F at t=0, three boots 2026-11-04 */
    }
}

bool plat_mb_poll_init(void)
{
    return s_mb_started ? mb_try_letter(MB_IN_REG) : false;
}

void plat_mb_tick_1ms(void)
{
    if (s_mb_hist_n < (uint16_t)(sizeof s_mb_hist / sizeof s_mb_hist[0])) {
        s_mb_hist[s_mb_hist_n++] = MB_IN_REG;
        if (s_mb_hist_n <= 500u) {
            (void)plat_mb_poll_init();   /* the valid window may open after start-up */
        }
    }
}

void plat_mb_tick(void)
{
    if (s_mb.phase != PLAT_MB_WAIT_CLEAR) {
        return;
    }
    s_mb.mailbox = MB_BOX_REG;
    if (s_mb.mailbox == 0u) {
        MB_BOX_REG = (s_mb.letter == 0x55u) ? 0x5555u : 0x3737u;
        s_mb.mailbox = MB_BOX_REG;
        s_mb.phase = PLAT_MB_OK;
        return;
    }
    if (s_mb_wait != 0u) {
        s_mb_wait--;
    }
    if (s_mb_wait == 0u) {
        s_mb.phase = PLAT_MB_TIMEOUT;
    }
}

const plat_mb_status_t *plat_mb_status(void)
{
    return &s_mb;
}

const uint16_t *plat_mb_hist(uint16_t *n)
{
    if (n != NULL) {
        *n = s_mb_hist_n;
    }
    return s_mb_hist;
}

/* ---- User boot MAT read-out (manual 25.9.1) ----------------------------------------------
 * Runs from RAM (.ramfunc, copied with .data at start-up): no flash access and no calls while the
 * user boot MAT is selected. The user boot MAT appears at the start of the ROM area. */
__attribute__((section(".ramfunc"), noinline))
static void userboot_copy_ram(uint32_t *dst, uint32_t words)
{
    uint32_t sr;
    uintptr_t a = 0u;
    const volatile uint32_t *src;

    __asm__ volatile ("stc sr, %0" : "=r"(sr));
    __asm__ volatile ("ldc %0, sr" : : "r"(sr | 0xF0u));    /* mask interrupts */
    __asm__ volatile ("" : "+r"(a));                         /* address 0 without UB */
    src = (const volatile uint32_t *)a;

    RCCR = RCCR_FLUSH_ALL;
    ROMMAT = 0x3B01u;                                        /* select the user boot MAT */
    (void)ROMMAT;
    __asm__ volatile ("nop\n nop\n nop\n nop\n nop\n nop");
    RCCR = RCCR_FLUSH_ALL;
    for (uint32_t i = 0u; i < words; i++) {
        dst[i] = src[i];
    }
    ROMMAT = 0x3B00u;                                        /* back to the user MAT */
    (void)ROMMAT;
    __asm__ volatile ("nop\n nop\n nop\n nop\n nop\n nop");
    RCCR = RCCR_FLUSH_ALL;
    __asm__ volatile ("ldc %0, sr" : : "r"(sr));
}

bool plat_userboot_copy(uint8_t *dst, uint32_t len)
{
    if (((uintptr_t)dst & 3u) != 0u || len > USERBOOT_SIZE) {
        return false;
    }
    plat_wdt_service();
    userboot_copy_ram((uint32_t *)(void *)dst, len / 4u);
    plat_wdt_service();
    return true;
}

/* ---- Reset and boot hand-over (spec 06 section 5a) ------------------------------ */
#define BOOT_RECORD ((volatile uint8_t *)0xFFF80080u)   /* outside the linked RAM region */

void plat_reset(void)
{
    __asm__ volatile ("ldc %0, sr" : : "r"(0xF0u));     /* mask all interrupts */
    WDT_WRCR  = 0x5A80u;    /* overflow causes a reset                         */
    WDT_WTCNT = 0x5AB0u;    /* stock 0x966B8 values: short time to overflow    */
    WDT_WTCR  = 0xA563u;
    for (;;) {
    }
}

void plat_boot_handover(const uint8_t *rec, uint8_t len)
{
    __asm__ volatile ("ldc %0, sr" : : "r"(0xF0u));
    for (uint8_t i = 0u; i < len; i++) {
        BOOT_RECORD[i] = rec[i];
    }
    plat_reset();
}

/* ---- Project flasher (spec 16) -------------------------------------------------- */
#define FL_LOAD_ADDR 0xFFF90000u        /* must match firmware/flasher/fl_link.ld */

#if !defined(TCU_RAM_APP) && !defined(TCU_LOADER)
extern const uint32_t fl_blob[];
extern const uint32_t fl_blob_end[];
#endif
void plat_jump_ram(const uint32_t *src, uint32_t dst, uint32_t len, uint32_t entry);

bool plat_flash_fwe(void)
{
    return (REG8(0xFFFFA800u) & 0x80u) != 0u;     /* FPMON.FWE */
}

bool plat_flash_enable(bool on)
{
    if (on) {
        PCIORH = (uint8_t)(PCIORH | 0xB0u);             /* PC12, PC13, PC15 outputs */
        PCDRH = (uint8_t)((PCDRH & 0x4Fu) | 0x30u);     /* PC12 = PC13 = 1, PC15 = 0 */
    } else {
        PCIORH = (uint8_t)(PCIORH & (uint8_t)~0xB0u);
    }
    return (PCPRH & 0x40u) != 0u;                       /* PC14 */
}

#if defined(TCU_RAM_APP)
/* RAM application (spec 18): hand the CPU back to the data-flash loader in code flash, which then
 * stays in loader mode (stay word in boot-preserved RAM, read and cleared by the loader). */
#define LD_STAY_WORD  (*(volatile uint32_t *)0xFFF800D0u)
#define LD_STAY_MAGIC 0x4C4F4144u
#define LOADER_ENTRY  (*(const uint32_t *)0x0004007Cu)    /* code-flash header +0x7C */

void plat_flasher_start(void)
{
    __asm__ volatile ("ldc %0, sr" : : "r"(0xF0u));
    plat_wdt_service();
    LD_STAY_WORD = LD_STAY_MAGIC;
    ((void (*)(void))(uintptr_t)LOADER_ENTRY)();
    for (;;) {
    }
}
#elif !defined(TCU_LOADER)
void plat_flasher_start(void)
{
    const uint32_t len = (uint32_t)((uintptr_t)fl_blob_end - (uintptr_t)fl_blob);

    __asm__ volatile ("ldc %0, sr" : : "r"(0xF0u));
    plat_wdt_service();
    plat_jump_ram(fl_blob, FL_LOAD_ADDR, len, FL_LOAD_ADDR);
}
#endif

/* ---- Idle counter ------------------------------------------------------------ */
static volatile uint32_t s_idle;

void plat_idle_count(void) { s_idle++; }

uint32_t plat_idle_take(void)
{
    uint32_t v = s_idle;
    s_idle = 0u;
    return v;
}

/* ---- Entry snapshot (bench: how the boot software leaves FWE and the ports) ------- */
static const uint8_t s_snap_pfc[] = {
    0x00, 0x04, 0x08, 0x0C, 0x0E, 0x10, 0x14, 0x16, 0x18, 0x1C, 0x1E, 0x20, 0x24, 0x26, 0x28,
    0x2A, 0x2C, 0x2E, 0x30, 0x34, 0x36, 0x3C, 0x40, 0x42, 0x44, 0x50, 0x54, 0x58, 0x60, 0x64,
    0x66, 0x68, 0x6C, 0x70, 0x72, 0x74, 0x80, 0x84, 0x86, 0x88, 0x8C, 0x90, 0x92, 0xA0, 0xA4,
    0xA8, 0xAC, 0xAE,
};
static uint8_t s_snap[1u + 2u * (7u + sizeof s_snap_pfc)];

static void put_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void entry_snapshot(void)
{
    const uint16_t abc[7] = { PAIOR, PADR, PBIOR, PBDR, PCIOR, PCDR, PCPR };
    uint8_t *p = &s_snap[1];

    s_snap[0] = REG8(0xFFFFA800u);                          /* FPMON */
    for (uint32_t i = 0u; i < 7u; i++, p += 2) {
        put_be16(p, abc[i]);
    }
    for (uint32_t i = 0u; i < sizeof s_snap_pfc; i++, p += 2) {
        put_be16(p, PFC16(s_snap_pfc[i]));
    }
}

uint16_t plat_entry_snapshot(uint8_t *out, uint16_t max)
{
    const uint16_t n = (uint16_t)(sizeof s_snap < max ? sizeof s_snap : max);

    for (uint16_t i = 0u; i < n; i++) {
        out[i] = s_snap[i];
    }
    return n;
}

/* ---- CPU clock probe (spec 05 section 1.2) ------------------------------------------ */
/* 16 x 64 dependent adds: the dual-issue core can complete only one per Iphi clock. */
__attribute__((noinline)) static void add_chain(uint32_t n)
{
    __asm__ volatile (
        "1:\n"
        ".rept 64\n"
        "    add   r0, r0\n"
        ".endr\n"
        "    dt    %0\n"
        "    bf    1b\n"
        : "+r"(n) : : "r0", "t");
}

uint16_t plat_clock_probe(uint8_t *out, uint16_t max)
{
    uint32_t adds = 0u, counts = 0u;
    uint16_t prev, now;

    if (max < 16u) {
        return 0u;
    }
    put_be16(&out[0], REG16(0xFFFF1000u));
    put_be16(&out[2], REG16(0xFFFF0812u));
    put_be16(&out[4], REG16(0xFFFF0802u));
    put_be16(&out[6], REG16(0xFFFF0842u));
    plat_wdt_service();
    prev = CMT_CMCNT0;
    for (uint32_t k = 0u; k < 1500u; k++) {
        add_chain(16u);                 /* ~1100 clocks, far below one CMT0 period (1 ms) */
        adds += 16u * 64u;
        now = CMT_CMCNT0;
        counts += (uint32_t)(now >= prev ? (uint32_t)(now - prev) : (uint32_t)now + 2500u - prev);
        prev = now;
        plat_wdt_service();
    }
    put_be16(&out[8], (uint16_t)(adds >> 16));
    put_be16(&out[10], (uint16_t)adds);
    put_be16(&out[12], (uint16_t)(counts >> 16));
    put_be16(&out[14], (uint16_t)counts);
    return 16u;
}

/* ---- The stock's init-time GPIO drives (S10, baseline review) ---------------
 * port_init equals the stock's safe-state block 0x961F0; these four init-
 * table entries run after it in the stock and were missing here, leaving the
 * pins floating inputs. Ported INSTRUCTION FOR INSTRUCTION from the flash
 * disassembly (sh-elf-objdump -m sh2a -EB on the September image) — NOT the
 * Ghidra text, which folds the set/clear pairs and moves the delays (r02g
 * review X1). bset/bclr.b are byte read-modify-writes; big-endian, so +0 is
 * bits 15..8 of a 16-bit port register (r5/r14 = 0xFFFE3900 port C,
 * r13 = 0xFFFE3800 port A):
 *   0x8FB96 (slot 14 via 0x8EEDC): PC11 = 0, out; PC12 = 1, out; PC13 = 1,
 *     out (PC13/PC12 set again); PC15 = 1, out; 150 x bld.b #6,@(30,r5)
 *     (PCPR reads — the stock's delay body); PC15 = 0, out. A PC15 HIGH
 *     PULSE: the one edge in the set.
 *   0x90D12 (slot 16 via 0x8EF1A): PA2 = 0, out; PC11 out; 0x90CBA(1):
 *     PC11 = 0; PJ8 = 1 (0xFFFFC860 bit 8), out (0xFFFFC86C bit 8); PC10
 *     out (still 0 — port_init); PC9 = 0, out; 0x90CC8(1): PC10 = 0, 75 x
 *     bclr PC10 (its delay body), PC10 = 1; PA7 = 0, out; 0x90CF6(0):
 *     0xFFF90920 = 0, PA7 = 0
 *   0x91852 (slot 16): PACR3[3:0] = 0 (PA8 GPIO); PA8 = 0, out
 *   0x67350 (slot 17): PACR4 low = 0; PA15..PA12 out; PACR4 high = 0
 * Not driven in the stock before certification beyond these: PA7 runtime
 * (0x8CE48, after 0x9095B > 14) and the PC12-14 handshake (cal 0x1918B2 =
 * 0 disables it). PC12/PC13/PC15 are also plat_flash_enable's lines — the
 * stock drives them like this on every boot; flashing enters through the
 * boot hand-over before this runs. [I] which of these the supervisor reads. */
#define B8(a) (*(volatile uint8_t *)(a))
#define BSET(a, b) (B8(a) = (uint8_t)(B8(a) | (uint8_t)(1u << (b))))
#define BCLR(a, b) (B8(a) = (uint8_t)(B8(a) & (uint8_t)~(1u << (b))))
static void stock_gpio_init(void)
{
    const uint32_t PC = 0xFFFE3900u, PA = 0xFFFE3800u;

    /* 0x8FB96 */
    BCLR(PC + 2u, 3); BSET(PC + 6u, 3);          /* PC11 = 0, out */
    BSET(PC + 2u, 4); BSET(PC + 6u, 4);          /* PC12 = 1, out */
    BSET(PC + 2u, 5); BSET(PC + 6u, 5);          /* PC13 = 1, out */
    BSET(PC + 2u, 5); BSET(PC + 2u, 4);          /* (stock repeats) */
    BSET(PC + 2u, 7); BSET(PC + 6u, 7);          /* PC15 = 1, out */
    for (uint32_t i = 0x96u; i != 0u; i--) {
        (void)B8(PC + 30u);                      /* bld.b #6,@(30,r5): PCPR */
    }
    BCLR(PC + 2u, 7); BSET(PC + 6u, 7);          /* PC15 = 0 */
    /* 0x90D12 */
    BCLR(PA + 3u, 2); BSET(PA + 7u, 2);          /* PA2 = 0, out */
    BSET(PC + 6u, 3);                            /* PC11 out */
    BCLR(PC + 2u, 3);                            /* 0x90CBA(1): PC11 = 0 */
    BSET(0xFFFFC860u, 0); BSET(0xFFFFC86Cu, 0);  /* PJ8 = 1, out */
    BSET(PC + 6u, 2);                            /* PC10 out */
    BCLR(PC + 2u, 1); BSET(PC + 6u, 1);          /* PC9 = 0, out */
    BCLR(PC + 2u, 2);                            /* 0x90CC8(1): PC10 = 0 */
    for (uint32_t i = 0x4Bu; i != 0u; i--) {
        BCLR(PC + 2u, 2);                        /* its delay body */
    }
    BSET(PC + 2u, 2);                            /* PC10 = 1 */
    BCLR(PA + 3u, 7); BSET(PA + 7u, 7);          /* PA7 = 0, out */
    B8(0xFFF90920u) = 0u;                        /* 0x90CF6(0) */
    BCLR(PA + 3u, 7);
    /* 0x91852 */
    B8(0xFFFE3813u) = (uint8_t)(B8(0xFFFE3813u) & 0xF0u);        /* PACR3: PA8 GPIO */
    BCLR(PA + 2u, 0); BSET(PA + 6u, 0);          /* PA8 = 0, out */
    /* 0x67350 */
    B8(0xFFFE3811u) = 0u;                        /* PACR4 low */
    B8(PA + 6u) = (uint8_t)(B8(PA + 6u) | 0xF0u);                 /* PA15..12 out */
    B8(0xFFFE3810u) = 0u;                        /* PACR4 high */
}
#undef BSET
#undef BCLR
#undef B8

/* ---- Start-up ----------------------------------------------------------------- */
void plat_init(void)
{
    entry_snapshot();       /* before anything changes the boot software's settings */
    wdt_init();             /* first, so the boot side's watchdog setting is replaced early */
    port_init();
    tick_init();
    adc_init();
    speed_init();
    plat_sup_init();
    plat_drv_init();
    plat_toe_out_init();    /* park hold PWM, duty 0 (pin stays low) */
    (void)can_init(RCAN_A_BASE);
    (void)can_init(RCAN_C_BASE);
    plat_wdt_service();
}

/* the APPLICATION's init additions (2026-11-12 baseline fixes) — called by
 * app/main.c after plat_init, never by the loader (which shares plat_init;
 * the loader is the flashing path). NOTE: the loader is NOT byte-identical
 * to r02f's — S9's sup_pfc_mux removal is in the shared plat_sup_init. The
 * bench install merges only the app block, so the on-bench loader is
 * unchanged until a loader is deliberately rebuilt and flashed. */
void plat_app_init(void)
{
    stock_gpio_init();          /* S10: the stock's init-table GPIO drives */
    plat_wdt_service();
    plat_walktmr_calibrate();   /* B2: <= 10 ms, interrupts still closed */
    plat_wdt_service();
}

/* ---- Rebuild additions (design §5) --------------------------------------------- */

/* FRT timestamp — the stock's own read (design §3, corrected 2026-11-10).
 *
 * ONE 32-bit load of TCNTA (0xFFFFF220), the stock's mov.l form at every
 * access site: era stepper 0x6C542, walk task 0x6C64E, timer programmers
 * 0x6C0CA/0x6C166, boot stamp sampler 0x67124/0x67152 (manual §13: TCNTA is
 * a 32-bit register). A single aligned 32-bit load is atomic against the
 * counter's own update, so the old s_last/s_hi wrap tracker is gone — it
 * was shared state read-modify-written from several call sites (5 ms batch,
 * walk monitor) and silently lost a whole 16-bit wrap on any call gap over
 * one low-half wrap period. Callers compare two's-complement deltas of raw
 * counts, exactly like the stock:
 *   era stepper 0x6C542: (u32)(TCNTA - 0xFFF9240C) >= *(u16)0xFFF93848 + 2400
 *   walk task   0x6C64E: (u32)(TCNTA - 0xFFF9240C) >= *(u32)0xFFF92410
 *   boot gates  0x66DF0: (u32)(TCNTA - prev)       >  0xFFFF
 * so the comparison unit is RAW COUNTS (sup_walk.c's constants are counts).
 * RATE: spec 02 §2 / artifact2 hold TCNTA is the stock's 1 MHz-class µs
 * timestamp — 1 count = 1 µs (the boot's own gates fit only that scale:
 * the −3000-count WDT-feed backdate is a ms-scale backdate and the
 * 0x65536-count loop gate is one 65.5 ms µs-counter wrap). Bench r01q
 * calibration: live reads 00 00 E0 73 → 00 00 A9 C4 with a low-half wrap
 * in ~0.4 s spans k=5-6 low-half wraps (0.379-0.445 s) at 1 MHz; the
 * single-wrap reading (~129 kHz) is not excluded by one pair — the era
 * math above is rate-independent in raw counts, and the rate measurement
 * (two reads over a timed interval) stays on the next-boot checklist. */
uint32_t plat_frt_us(void)
{
    return FRT_TCNTA;
}

/* Bus-A bit-rate accessor for the RSPI engine (plat_rspi.c pump). */
uint8_t plat_drv_spbr_get(void)
{
    return s_drv_spbr;
}
