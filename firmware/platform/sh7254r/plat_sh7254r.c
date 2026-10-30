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

/* ---- External supervisor (RSPI-B, SSLB1), CPU-driven as in stock 0x9210C / 0x1D430 ------
 * Bench 2026-10-27: keeping the RSPI enabled across frames wedges the CPU path after the
 * FIRST frame (tick counters sent=345 / ok=1: SPRF never sets again). The stock 5 ms
 * keep-alive task instead re-runs the FULL setup before every frame (boot ROM 0x1D430 each
 * tick from 0x1D0B4) and writes SPSR=0 after the SPDR write; the app's live-reply path is
 * A-DMAC only. So every frame here is stateless: setup -> SPDR -> poll SPRF -> read reply
 * (full duplex: the reply to THIS frame clocks in during its own transfer) -> SPSR=0. */

static void sup_pfc_mux(void)
{
    /* pin mux the engine proved necessary (pf.c rspi_pfc); idempotent OR-masking */
    (*(volatile uint16_t *)0xFFFFC840u) =
        (uint16_t)((*(volatile uint16_t *)0xFFFFC840u & 0x0FFFu) | 0xA000u);
    (*(volatile uint16_t *)0xFFFE3890u) =
        (uint16_t)((*(volatile uint16_t *)0xFFFE3890u & 0x3FFFu) | 0xC000u);
    (*(volatile uint16_t *)0xFFFE3896u) =
        (uint16_t)((*(volatile uint16_t *)0xFFFE3896u & 0x3FFFu) | 0xC000u);
}

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
    sup_pfc_mux();
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

/* ---- Solenoid driver chips: RSPI-A, CPU-driven jobs (spec 03 sections 2-4) ----------------
 * Same bus settings as the stock job start (0x96468 with descriptor 0x40CD8): P/12, no extra
 * delays, a five-command sequence SSLA3, SSLA2, SSLA1, SSLA0, SSLA5 (16 bit, MSB first, mode 1),
 * so frame n goes to chip n mod 5. Each job restarts the sequence by toggling SPE. */
static const uint16_t s_drv_cmd[5] = { 0xAF31u, 0xAF21u, 0xAF11u, 0xAF01u, 0xAF51u };

void plat_drv_init(void)
{
    SPCRA = 0x00u;
    RSPIA_DMA_REQ0 = 0u;
    RSPIA_DMA_REQ1 = 0u;
    SPSRA = 0u;
    SPPCRA = 0u;
    SPBRA = 5u;
    SPCKDA = 0u;
    SSLNDA = 0u;
    SPNDA = 0u;
    SPSCRA = 4u;                        /* sequence of five commands */
    for (uint32_t i = 0u; i < 5u; i++) {
        SPCMDA(i) = s_drv_cmd[i];
    }
}

#define DRV_WAIT_LOOPS 2000u            /* far above one 16-bit frame at P/12 */

bool plat_drv_xfer(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    SPCRA = 0x08u;                      /* disable: restarts the command sequence */
    SPSRA = 0u;
    SPCRA = 0x48u;                      /* enable, master */
    for (uint8_t i = 0u; i < n; i++) {
        uint32_t guard;
        for (guard = DRV_WAIT_LOOPS; guard != 0u && (SPSRA & SPSR_SPTEF) == 0u; guard--) {
        }
        if (guard == 0u) {
            return false;
        }
        SPDRA = tx[i];
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
