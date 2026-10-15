/* SH7254R platform layer: safe pin state, watchdog, 1 ms tick, CAN buses 1 and 2, ADC, speed inputs. */
#include "platform/platform.h"
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
static void wdt_init(void)
{
    WDT_WTCR  = 0xA547u;    /* stop (TME = 0) before changing the clock select */
    WDT_WRCR  = 0x5A80u;    /* overflow resets the chip                        */
    WDT_WTCNT = 0x5AB7u;    /* first overflow after ~60 ms                     */
    WDT_WTCR  = 0xA567u;    /* watchdog mode, running, P/16384                 */
}

void plat_wdt_service(void)
{
    WDT_WTCNT = 0x5ADBu;    /* ~30 ms to overflow */
}

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

    RCAN_MCR(b) = RCAN_MCR_RESET;
    for (guard = 100000u; guard != 0u && (RCAN_GSR(b) & RCAN_GSR_RESET) == 0u; guard--) {
    }
    RCAN_IMR(b)  = 0xFFFFu;             /* polled operation */
    RCAN_BCR1(b) = 0xE330u;             /* TSEG1 15, TSEG2 4, SJW 4 */
    RCAN_BCR0(b) = 0x0000u;             /* BRP 0 -> 500 kbit/s at 20 MHz */
    for (uint32_t n = 0u; n < RCAN_MAILBOXES; n++) {
        RCAN_MB_CTRL1(b, n) = RCAN_MBC_OFF;
    }
    RCAN_MCR(b) = 0x0000u;              /* leave reset: normal operation */
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

bool plat_can_rx_register(can_bus_t bus, uint16_t id)
{
    uint32_t b, mb;

    if (bus >= CAN_BUS_COUNT || s_rx_next[bus] >= RCAN_MAILBOXES) {
        return false;
    }
    b = s_can_base[bus];
    mb = s_rx_next[bus]++;
    /* Mailbox is inactive (MBC 7) since can_init, so it may be configured now. */
    RCAN_MB_CTRL0H(b, mb) = (uint16_t)((id & 0x7FFu) << 2);   /* standard data frame */
    RCAN_MB_CTRL0L(b, mb) = 0u;
    RCAN_MB_LAFMH(b, mb) = 0u;                                  /* all identifier bits compared */
    RCAN_MB_LAFML(b, mb) = 0u;
    RCAN_MB_CTRL1(b, mb) = (uint16_t)(RCAN_MBC_RX | 8u);        /* NMC 0: newest frame kept */
    return true;
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

/* ---- External supervisor (RSPI-B, SSLB1), CPU-driven as in stock 0x9210C / 0x1D430 ------ */
void plat_sup_init(void)
{
    RSPIB_DMA_REQ0 = 0u;
    RSPIB_DMA_REQ1 = 0u;
    SPSRB = 0u;
    SPPCRB = 0u;
    SPBRB = 5u;                         /* P/12 */
    SPDCRB = 0u;
    SPCKDB = 0u;
    SSLNDB = 0u;
    SPNDB = 1u;
    SPCMDB0 = 0xAF11u;                  /* SSLB1, 16 bit, MSB first, mode 1 */
    SPCRB = 0x48u;                      /* enable, master */
}

uint8_t plat_sup_exchange(uint16_t tx, uint16_t *rx)
{
    uint8_t r = 0u;
    const uint8_t st = SPSRB;

    if ((st & SPSR_SPRF) != 0u) {
        *rx = SPDRB;
        r |= 2u;
    }
    if ((st & (uint8_t)~SPSR_SPTEF) != 0u) {
        /* clear receive / error flags (write 0 after reading 1); writing 1 to SPTEF is ignored,
         * so it keeps its state */
        SPSRB = (uint8_t)(st & SPSR_SPTEF);
    }
    if ((SPSRB & SPSR_SPTEF) != 0u) {
        SPDRB = tx;
        r |= 1u;
    }
    return r;
}

/* ---- Memory read for the measurement protocol ---------------------------------- */
static bool readable(uint32_t a, uint16_t n)
{
    const uint32_t end = a + n - 1u;

    if (n == 0u || end < a) {
        return false;
    }
    return (a >= 0x00040000u && end <= 0x001FFFFFu) ||
           (a >= 0xFFF80000u && end <= 0xFFF9FFFFu);
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

/* ---- Idle counter ------------------------------------------------------------ */
static volatile uint32_t s_idle;

void plat_idle_count(void) { s_idle++; }

uint32_t plat_idle_take(void)
{
    uint32_t v = s_idle;
    s_idle = 0u;
    return v;
}

/* ---- Start-up ----------------------------------------------------------------- */
void plat_init(void)
{
    wdt_init();             /* first, so the boot side's watchdog setting is replaced early */
    port_init();
    tick_init();
    adc_init();
    speed_init();
    plat_sup_init();
    (void)can_init(RCAN_A_BASE);
    (void)can_init(RCAN_C_BASE);
    plat_wdt_service();
}
