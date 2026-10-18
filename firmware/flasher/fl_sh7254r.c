/* Flasher hardware services on the SH7254R (spec 16 section 3). Polled, RAM-resident.
 * The application leaves the clocks, pins, CAN bit timing, CMT0 (1 ms), the watchdog and RSPI-B
 * configured; the flasher only reprograms the RCAN-A mailboxes it uses. */
#include "flasher/fl_hal.h"
#include "flasher/fl_regs.h"
#include "platform/sh7254r/regs.h"

#define CANB       RCAN_A_BASE      /* bus 1 */
#define TX_FIRST   1u
#define TX_LAST    3u
#define RX_MB      8u
#define REQ_ID     0x7E1u
#define RSP_ID     0x7E9u
#define TX_MASK    (((1u << (TX_LAST + 1u)) - 1u) & ~((1u << TX_FIRST) - 1u))

#define SUP_CONTROL 0x8E00u         /* spec 03 4.4: bit 2 toggles, bit 0 output enable (kept 0) */
#define SUP_TOGGLE  0x0004u

static uint32_t s_ms;
static uint8_t s_sup_div;
static bool s_toggle;

void fl_hal_init(void)
{
    /* a mailbox may be reconfigured only while its TXPR bit is clear (manual 18.x): cancel what
     * the application left pending (normally nothing, it waited 20 ms) */
    RCAN_TXCR(CANB) = 0xFFFFFFFEu;
    for (uint32_t guard = 200000u; guard != 0u && RCAN_TXPR(CANB) != 0u; guard--) {
        WDT_WTCNT = 0x5ADBu;
    }
    for (uint32_t mb = 1u; mb < RCAN_MAILBOXES; mb++) {
        RCAN_MB_CTRL1(CANB, mb) = RCAN_MBC_OFF;
    }
    RCAN_RXPR(CANB) = 0xFFFFFFFFu;
    RCAN_MB_CTRL0H(CANB, RX_MB) = (uint16_t)(REQ_ID << 2);
    RCAN_MB_CTRL0L(CANB, RX_MB) = 0u;
    RCAN_MB_LAFMH(CANB, RX_MB) = 0u;
    RCAN_MB_LAFML(CANB, RX_MB) = 0u;
    RCAN_MB_CTRL1(CANB, RX_MB) = (uint16_t)(RCAN_MBC_RX | 8u);
    WDT_WTCNT = 0x5ADBu;
}

static void sup_service(void)
{
    const uint8_t st = SPSRB;

    if ((st & SPSR_SPRF) != 0u) {
        (void)SPDRB;                    /* reply not evaluated in the flasher */
    }
    if ((st & (uint8_t)~SPSR_SPTEF) != 0u) {
        SPSRB = (uint8_t)(st & SPSR_SPTEF);
    }
    if ((SPSRB & SPSR_SPTEF) != 0u) {
        s_toggle = !s_toggle;
        SPDRB = (uint16_t)(SUP_CONTROL | (s_toggle ? SUP_TOGGLE : 0u));
    }
}

void fl_hal_poll(void)
{
    if ((CMT_CMSR0 & 0x01u) != 0u) {
        CMT_CMSR0 = 0x00u;
        s_ms++;
        WDT_WTCNT = 0x5ADBu;            /* ~30 ms to overflow; serviced every ms */
        if (++s_sup_div >= 5u) {
            s_sup_div = 0u;
            sup_service();
        }
    }
}

uint32_t fl_hal_ms(void)
{
    return s_ms;
}

bool fl_hal_can_recv(uint8_t d[8], uint8_t *dlc)
{
    if ((RCAN_RXPR(CANB) & (1u << RX_MB)) == 0u) {
        return false;
    }
    RCAN_RXPR(CANB) = 1u << RX_MB;
    *dlc = (uint8_t)(RCAN_MB_CTRL1(CANB, RX_MB) & 0x0Fu);
    if (*dlc > 8u) {
        *dlc = 8u;
    }
    for (uint8_t i = 0u; i < 8u; i++) {
        d[i] = RCAN_MB_DATA(CANB, RX_MB, i);
    }
    return true;
}

bool fl_hal_can_send(const uint8_t d[8])
{
    for (uint32_t mb = TX_FIRST; mb <= TX_LAST; mb++) {
        if ((RCAN_TXPR(CANB) & (1u << mb)) != 0u) {
            continue;
        }
        RCAN_MB_CTRL1(CANB, mb) = RCAN_MBC_OFF;
        RCAN_MB_CTRL0H(CANB, mb) = (uint16_t)(RSP_ID << 2);
        RCAN_MB_CTRL0L(CANB, mb) = 0u;
        for (uint8_t i = 0u; i < 8u; i++) {
            RCAN_MB_DATA(CANB, mb, i) = d[i];
        }
        RCAN_MB_CTRL1(CANB, mb) = (uint16_t)(RCAN_MBC_TX | 8u);
        RCAN_TXPR(CANB) = 1u << mb;
        return true;
    }
    return false;
}

bool fl_hal_can_idle(void)
{
    return (RCAN_TXPR(CANB) & TX_MASK) == 0u;
}

void fl_hal_rom_cache_flush(void)
{
    fl_wr32(FL_RCCR, RCCR_FLUSH_ALL);
}

void fl_hal_reset(void)
{
    /* only called between operations (FRDY = 1, ROM read mode): a watchdog reset is allowed */
    WDT_WRCR  = 0x5A80u;
    WDT_WTCNT = 0x5AB0u;
    WDT_WTCR  = 0xA563u;
    for (;;) {
    }
}
