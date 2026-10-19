/* Plant-session RAM dumper (spec 17 follow-up: code-flash independence).
 *
 * The boot MAT's user-boot session accepts a code download into its RAM windows and runs it:
 * the session main loop (FUN_0000105a) tail-calls *(*(*(0xFFF88410)) + 8)(0xFFF84A6C, x)
 * whenever the flag at 0xFFF836A3 is set. This image is laid out for that contract:
 *
 *   0xFFF88400  16 bytes  pad (zero)
 *   0xFFF88410  slot      pointer to the descriptor below
 *   0xFFF88420  descriptor, +8 = entry (pd_entry)
 *   0xFFF88430  code / tables
 *
 * pd_entry runs in the session's context on the session's stack and returns to the main
 * loop, so it must keep the watchdog and the external supervisor alive itself. The plant
 * session sets supervisor state 0x19 (word 0x8E80, bit 4 toggling), the same default the
 * project flasher uses (fl_sh7254r.c), so the service routines are copied from there.
 *
 * Output goes out on mailbox 2 (idle after plant entry) with ID 0x7EA so the host can
 * tell dumper traffic from the UDS replies on 0x7E9. Regions are dumped in 6-byte chunks:
 *   region header  'P','D','R', idx, base MSB..LSB
 *   data           (idx<<4 | seq>>8), seq&0xFF, six data bytes
 *   end            'P','D','E', nregions, 0,0,0,0
 * An inbound 0x7E1 frame starting 'P','D','A' aborts the dump early.
 *
 * PD_CANARY builds send a single frame per invocation instead, to probe the download
 * path and the endianness of the transfer without any risk.
 */

#include <stdint.h>

#define REG8(a)   (*(volatile uint8_t  *)(a))
#define REG16(a)  (*(volatile uint16_t *)(a))
#define REG32(a)  (*(volatile uint32_t *)(a))

#define WDT_WTCNT    REG16(0xFFFE0002u)   /* write 0x5Axx */
#define CMT_CMSR0    REG8 (0xFFFEC011u)   /* bit 0: 1 ms compare flag, write 0 to clear */

#define SPSRB        REG8 (0xFFFFB803u)
#define SPDRB        REG16(0xFFFFB804u)
#define SPSR_SPRF    0x80u
#define SPSR_SPTEF   0x20u

#define CANB         0xFFFFD000u          /* RCAN-A, bus 1, TCU pins 5/6 */
#define RCAN_TXPR    REG32(CANB + 0x020u)
#define RCAN_RXPR    REG32(CANB + 0x040u)
#define MB_CTRL0H(n) REG16(CANB + 0x100u + 32u * (n))
#define MB_CTRL0L(n) REG16(CANB + 0x102u + 32u * (n))
#define MB_DATA(n,i) REG8 (CANB + 0x108u + 32u * (n) + (i))
#define MB_CTRL1(n)  REG16(CANB + 0x110u + 32u * (n))
#define MBC_TX       (0u << 8)
#define MBC_OFF      (7u << 8)

#define DUMP_MB      2u                   /* plant-entry RX mailbox, idle afterwards */
#define DUMP_ID      0x7EAu
#define RX_MB        0u                   /* session UDS RX (0x7E1), polled for abort */

#define RUN_FLAG     REG8 (0xFFF836A3u)   /* set by the session to request the run */
#define PD_STATE     REG32(0xFFF88B00u)   /* scratch inside the download window */
#define PD_FLAG_DONE 0x50443031u          /* "PD01": full dump already sent */

struct pd_region {
    uint32_t base;
    uint32_t len;
};

static const struct pd_region pd_regions[] = {
    { 0xFFF83600u, 0x0E00u },   /* session block: conn table, nodes, handlers, image start */
    { 0xFFF82000u, 0x2000u },   /* second session window (target of the install descriptor) */
    { 0xFFF84900u, 0x0200u },   /* session descriptor globals (incl. the 0xFFF84A6C buffer) */
    { 0xFFF88400u, 0x0040u },   /* this image: endianness / echo check */
};

extern const uint32_t pd_desc[4];
void pd_entry(uint32_t arg1, uint32_t arg2);

__attribute__((used))
__attribute__((section(".img_pad")))
static const uint32_t pd_pad[4] = { 0u, 0u, 0u, 0u };

__attribute__((used))
__attribute__((section(".img_slot")))
static const uint32_t pd_slot[1] = { (uint32_t)&pd_desc[0] };

__attribute__((used))
__attribute__((section(".img_desc")))
const uint32_t pd_desc[4] = { 0u, 0u, (uint32_t)&pd_entry, 0u };

static void pd_supervisor(void)
{
    const uint8_t st = SPSRB;

    if ((st & SPSR_SPRF) != 0u) {
        (void)SPDRB;
    }
    if ((st & (uint8_t)~SPSR_SPTEF) != 0u) {
        SPSRB = (uint8_t)(st & SPSR_SPTEF);
    }
    if ((SPSRB & SPSR_SPTEF) != 0u) {
        static const uint16_t word[2] = { 0x8E80u, 0x8E90u };  /* state 0x19: bit 4 toggles */
        REG8(0xFFF88B04u) ^= 1u;             /* phase, in the window's scratch (no .bss) */
        SPDRB = word[REG8(0xFFF88B04u) & 1u];
    }
}

/* Wait one 1 ms CMT period while servicing the watchdog (~30 ms budget) and, every
 * fifth tick, the supervisor. */
static void pd_frame_gap(void)
{
    uint32_t div = 0u;

    for (;;) {
        WDT_WTCNT = 0x5ADBu;
        if ((CMT_CMSR0 & 0x01u) != 0u) {
            CMT_CMSR0 = 0x00u;
            if (++div >= 5u) {
                pd_supervisor();
            }
            return;
        }
    }
}

static void pd_send(const uint8_t d[8])
{
    while ((RCAN_TXPR & (1u << DUMP_MB)) != 0u) {
        WDT_WTCNT = 0x5ADBu;
    }
    MB_CTRL1(DUMP_MB) = MBC_OFF;
    MB_CTRL0H(DUMP_MB) = (uint16_t)(DUMP_ID << 2);
    MB_CTRL0L(DUMP_MB) = 0u;
    for (uint8_t i = 0u; i < 8u; i++) {
        MB_DATA(DUMP_MB, i) = d[i];
    }
    MB_CTRL1(DUMP_MB) = (uint16_t)(MBC_TX | 8u);
    RCAN_TXPR = RCAN_TXPR | (1u << DUMP_MB);   /* 32-bit RMW: keep the session's MB1 pending */
}

static uint8_t pd_abort_pending(void)
{
    if ((RCAN_RXPR & (1u << RX_MB)) == 0u) {
        return 0u;
    }
    RCAN_RXPR = 1u << RX_MB;
    return (MB_DATA(RX_MB, 0) == 'P' && MB_DATA(RX_MB, 1) == 'D' && MB_DATA(RX_MB, 2) == 'A') ? 1u : 0u;
}

void pd_entry(uint32_t arg1, uint32_t arg2)
{
    (void)arg2;

    RUN_FLAG = 0;                       /* one shot per set */
#ifdef PD_CANARY
    uint8_t f[8] = { 'P', 'D', 'C', 0u, 0u, 0u, 0u, 0u };
    PD_STATE++;
    f[3] = (uint8_t)PD_STATE;           /* invocation count: reveals level vs edge flagging */
    f[4] = (uint8_t)(arg1 >> 24);       /* echo arg1 byte-exact: endianness oracle */
    f[5] = (uint8_t)(arg1 >> 16);
    f[6] = (uint8_t)(arg1 >> 8);
    f[7] = (uint8_t)arg1;
    pd_send(f);
    pd_frame_gap();
#else
    (void)arg1;
    if (PD_STATE == PD_FLAG_DONE) {
        return;                         /* later invocations do nothing */
    }
    for (uint32_t r = 0u; r < sizeof(pd_regions) / sizeof(pd_regions[0]); r++) {
        uint8_t f[8] = { 'P', 'D', 'R', 0u, 0u, 0u, 0u, 0u };
        uint32_t base = pd_regions[r].base;
        uint32_t len = pd_regions[r].len;
        f[3] = (uint8_t)r;
        f[4] = (uint8_t)(base >> 24);
        f[5] = (uint8_t)(base >> 16);
        f[6] = (uint8_t)(base >> 8);
        f[7] = (uint8_t)base;
        pd_send(f);
        pd_frame_gap();
        for (uint32_t off = 0u; off < len; off += 6u) {
            uint32_t seq = off / 6u;
            f[0] = (uint8_t)((r << 4) | (seq >> 8));
            f[1] = (uint8_t)seq;
            f[2] = REG8(base + off);
            f[3] = (off + 1u < len) ? REG8(base + off + 1u) : 0u;
            f[4] = (off + 2u < len) ? REG8(base + off + 2u) : 0u;
            f[5] = (off + 3u < len) ? REG8(base + off + 3u) : 0u;
            f[6] = (off + 4u < len) ? REG8(base + off + 4u) : 0u;
            f[7] = (off + 5u < len) ? REG8(base + off + 5u) : 0u;
            pd_send(f);
            pd_frame_gap();
            if (pd_abort_pending() != 0u) {
                PD_STATE = PD_FLAG_DONE;
                uint8_t e[8] = { 'P', 'D', 'A', 0u, 0u, 0u, 0u, 0u };
                pd_send(e);
                return;
            }
        }
    }
    PD_STATE = PD_FLAG_DONE;
    {
        uint8_t e[8] = { 'P', 'D', 'E', 0u, 0u, 0u, 0u, 0u };
        e[3] = (uint8_t)(sizeof(pd_regions) / sizeof(pd_regions[0]));
        pd_send(e);
        pd_frame_gap();
    }
#endif
}
