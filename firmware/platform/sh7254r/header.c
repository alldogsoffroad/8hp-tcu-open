/* Application block header, vector table and tail record (spec 06 sections 2-3).
 *
 * Layout reproduced from the stock application header; fields whose meaning is not known are
 * copied unchanged. The CRC at +0x44 is filled in by tools/build_image.py after linking.
 * The Bosch boot manager (0x24FD0) checks: magic 0x876543xx, end pointer (+0x0C) in range and
 * 4-byte aligned, and the word at the end pointer == 0x12345678. */
#include <stdint.h>
#include "platform/platform.h"   /* E60b: default_handler's CAN announce */

extern void start(void);            /* _start in start.S */
extern const uint32_t app_end_marker;

#define HDR_CRC_START 0x00040080u
#define HDR_CRC_END   0x0017FB6Fu   /* inclusive; same range as the stock application */

struct app_header {
    uint32_t magic;                 /* +0x00 */
    uint32_t r04;                   /* +0x04 */
    uint32_t next_header;           /* +0x08 calibration header */
    const uint32_t *end_marker;     /* +0x0C */
    uint32_t marker_list;           /* +0x10 */
    uint32_t r14;                   /* +0x14 */
    uint8_t  ident[20];             /* +0x18 0x0B00 "TYPTEILENR" "bhjc2222" */
    uint8_t  r2c[12];               /* +0x2C */
    uint32_t crc_start;             /* +0x38 */
    uint32_t crc_end;               /* +0x3C */
    uint32_t r40;                   /* +0x40 */
    uint32_t crc;                   /* +0x44 */
    uint32_t r48, r4c, r50;         /* +0x48 */
    uint32_t markers[4];            /* +0x54 address, size, address, size */
    uint32_t r64[4];                /* +0x64 */
    uint32_t app_ptr;               /* +0x74 not used by the boot side; 0 */
    uint32_t r78;                   /* +0x78 */
    void   (*entry)(void);          /* +0x7C */
};

__attribute__((section(".app_header"), used))
const struct app_header app_header = {
    .magic       = 0x876543C1u,
    .r04         = 0u,
    .next_header = 0x00180200u,
    .end_marker  = &app_end_marker,
    .marker_list = 0x00040054u,
    .r14         = 0u,
    .ident       = { 0x0B, 0x00, 'T','Y','P','T','E','I','L','E','N','R',
                     'b','h','j','c','2','2','2','2' },
    .r2c         = { 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x04, 0xFF, 0xFF },
    .crc_start   = HDR_CRC_START,
    .crc_end     = HDR_CRC_END,
    .r40         = 0xFFFFFFFFu,
    .crc         = 0u,
    .r48         = 0u, .r4c = 0u, .r50 = 0xFFFFFFFFu,
    .markers     = { 0x0017FE00u, 0x100u, 0x0017FF00u, 0x100u },
    .r64         = { 0u, 0u, 0u, 0u },
    .app_ptr     = 0u,
    .r78         = 0u,
    .entry       = start,
};

/* E60d: the exception frame reader. Manual 7.5.3: on exception entry the
 * hardware pushes SR then PC, so @r15 = the faulting (or interrupted) PC and
 * @(r15,4) = SR. The naked entry captures both and the body announces them —
 * one frame resolves against the map to the exact instruction. */
void default_body(uint32_t pc, uint32_t sr);   /* global: resolved from naked asm */
__attribute__((naked))
static void default_handler(void)
{
    /* basic asm only inside naked; this toolchain prefixes C symbols with _ */
    __asm__ volatile (
        "mov.l  @r15, r4        \n"     /* the faulting/interrupted PC  */
        "mov.l  @(4, r15), r5   \n"     /* SR at the moment of fault    */
        "mov.l  3f, r0          \n"
        "jsr    @r0             \n"
        " nop                   \n"
        "9: bra   9b            \n"     /* default_body never returns   */
        " nop                   \n"
        ".align 2               \n"
        "3: .long _default_body \n"
    );
}

__attribute__((used))              /* referenced only from the naked asm; keep under --gc-sections */
void default_body(uint32_t pc, uint32_t sr)
{
    can_frame_t f = { .id = 0x7F5u, .dlc = 8u, .data = { 0xDEu, 0xADu, 0, 0, 0, 0, 0, 0 } };
    f.data[2] = (uint8_t)(pc >> 24);
    f.data[3] = (uint8_t)(pc >> 16);
    f.data[4] = (uint8_t)(pc >> 8);
    f.data[5] = (uint8_t)pc;
    f.data[6] = (uint8_t)(sr >> 16);
    f.data[7] = (uint8_t)(sr >> 8);
    (void)plat_can_send(CAN_BUS1, &f);
    for (;;) {
    }
}

/* E60 (spec 03 section 6.2ar): the interrupt-driven feed ISR, corrected.
 * E59's ISR never fired: SR.I was 15 from boot (start.S), IPR27 was 0 and
 * SPCR=0x09 never had SPE set (manual 17.3.1: bit7 SPRIE, bit6 SPE, bit3
 * MSTR — the old "0x09 = SPE+MSTR" decode had SPE wrong). E60 arms the burst
 * with SPCR=0xC8 (SPRIE+SPE+MSTR), IPR27 bits 11-8 = 0xE and SR.I opened only
 * for the burst window (plat_drv_xfer_irq); this handler then feeds frames.
 * The feed order is the proven polled loop's (manual 17.4.9): SPTEF read,
 * SPDR write, SPSR read, SPSR=0 (the CPU start trigger). */
__attribute__((interrupt_handler))
void rspi_a_isr(void)
{
    /* volatile: shared with the mainline fill/start code */
    extern volatile uint16_t rspi_a_tx_buf[64];
    extern volatile uint16_t rspi_a_rx_buf[64];
    extern volatile uint8_t  rspi_a_idx;     /* frames already started */
    extern volatile uint8_t  rspi_a_count;   /* frames in this burst */

    volatile uint8_t *const spsr  = (volatile uint8_t *)0xFFFFB003u;
    volatile uint8_t *const spcr  = (volatile uint8_t *)0xFFFFB000u;
    volatile uint8_t *const gate  = (volatile uint8_t *)0xFFFE6030u;
    volatile uint16_t *const spdr = (volatile uint16_t *)0xFFFFB004u;

    /* the stock's entry protocol (0x96590 decode) */
    *spsr = 0u;                          /* SPSR acknowledge */
    (void)*gate;
    *gate = 0xF3u;                       /* clock-gate enable */

    if (rspi_a_idx == 0u) {
        /* spurious entry (no frame started): disarm and leave */
        *spcr = 0x08u;
        rspi_a_count = 0u;
        return;
    }

    /* collect the reply of the frame that just completed — reading SPDR
     * clears SPRF, the interrupt request. The reply of frame k belongs at
     * index k: the mainline started frame 0 with idx=1, so this is idx-1. */
    rspi_a_rx_buf[rspi_a_idx - 1u] = *spdr;

    if (rspi_a_idx < rspi_a_count) {
        /* feed the next frame (the polled loop's proven order) */
        (void)*spsr;                     /* read SPTEF = 1 */
        *spdr = rspi_a_tx_buf[rspi_a_idx];
        (void)*spsr;
        *spsr = 0u;                      /* start trigger (manual 17.4.9) */
        rspi_a_idx++;
    } else {
        /* burst complete: the final reply is already collected above;
         * disarm the receive interrupt, keep MSTR (the stock's live idle) */
        *spcr = 0x08u;
        rspi_a_count = 0u;               /* releases the mainline wait */
    }
}

/* E60c: the announce net. E60b's DEADBABE fired at the exact arm moment — an
 * interrupt delivers but lands somewhere unexpected. Each RSPI-area vector now
 * names itself on the bus (0x7F5 [0xE6, vecnum]) before hanging: 444 = SPEIA
 * (error), 445 = the ISR's own entry announce, 446 = SPTIA (transmit), 447/448
 * = alignment sentinels (a 447/448 hit means the vector numbering itself is
 * shifted). One flash names the culprit exactly. */
static void announce_vec(uint8_t which)
{
    can_frame_t f = { .id = 0x7F5u, .dlc = 8u, .data = { 0xE6u, 0, 0, 0, 0, 0, 0, 0 } };
    f.data[1] = which;
    (void)plat_can_send(CAN_BUS1, &f);
}

#define ANN_HANDLER(n)                                              \
    static void ann_##n(void) __attribute__((interrupt_handler));   \
    static void ann_##n(void)                                       \
    {                                                               \
        announce_vec(n);                                            \
        for (;;) { }                                                \
    }

ANN_HANDLER(444)
ANN_HANDLER(446)
ANN_HANDLER(447)
ANN_HANDLER(448)

/* ── the rebuild's ISR entries (design §5.2-5.5; spec 24 vector inventory) ─
 * vector 148 WDT ITI (interval mode), 216 ATU-D0 CMID00 (walk), 228 ATU-D1
 * CMID10 (the 1 ms tick), 445/449 RSPI-A/B SPRIB (the job engine). Each
 * wrapper only forwards to the engine's ISR body; the bodies own their
 * acknowledge/re-arm. The data-flash loader (TCU_LOADER) never arms any of
 * these sources — its vectors stay on the default announce net. */
#if !defined(TCU_LOADER)

extern void plat_tick_isr(void);
extern void plat_walktmr_isr(void);
extern void plat_rspi_isr(uint8_t bus);
extern void plat_wdt_kick(void);

__attribute__((interrupt_handler))
static void v_wdt_iti(void)
{
    plat_wdt_kick();                    /* interval mode: reload on overflow */
}

__attribute__((interrupt_handler))
static void v_walktmr(void)
{
    plat_walktmr_isr();
}

__attribute__((interrupt_handler))
static void v_tick(void)
{
    plat_tick_isr();
}

__attribute__((interrupt_handler))
static void v_rspi_a(void)
{
    plat_rspi_isr(0u);
}

__attribute__((interrupt_handler))
static void v_rspi_b(void)
{
    plat_rspi_isr(1u);
}

#else

#define v_wdt_iti default_handler
#define v_walktmr default_handler
#define v_tick default_handler
#define v_rspi_a default_handler
#define v_rspi_b default_handler

#endif

/* Vectors 4..511 (508 entries), at header +0x80.
 * Layout: 7 x V64 (vectors 4-259) | V16 x3 (260-307) | V4 x3 (308-319)
 * then the tail: V4, V4 = vectors 320-327... vector 445 = index 441 (0-based). */
#define V4(x) x, x, x, x
#define V16(x) V4(x), V4(x), V4(x), V4(x)
#define V64(x) V16(x), V16(x), V16(x), V16(x)
__attribute__((section(".app_vectors"), used))
void (*const app_vectors[508])(void) = {
    /* vectors 4..511: 508 entries. Wired: index 144 = vector 148 (WDT ITI),
     * 212 = vector 216 (ATU-D0 CMID00, walk), 224 = vector 228 (ATU-D1
     * CMID10, the 1 ms tick), 440-444 (RSPI-A error/announce sentinels),
     * 441 = vector 445 (RSPI-A SPRIA) and 449 = vector 449 (RSPI-B SPRIB) —
     * the job-engine RX ISRs. Everything else defaults (announce + halt). */
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    v_wdt_iti,  /* index 144 = VECTOR 148 */ default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    v_walktmr,  /* index 212 = VECTOR 216 */ default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    v_tick,  /* index 224 = VECTOR 228 */ default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    v_rspi_b, /* index 440 = VECTOR 444 — the r01i vector EXPERIMENT: the stock builds its table at runtime (its flash table is all park-stubs at 0x408E4), so spec 24's numbers came from RAM-dump inference. A delivers at v445 (proven); B does NOT at v449 with everything else verified (nibbles live, slots verified, SPSR clean) — B's true vector is unknown, so the neighborhood is blanket-wired to v_rspi_b (the body self-guards on state≠FEEDING: stray hits are no-ops). s_vec_count[1] after one boot names the live slot. */
    v_rspi_a, /* index 441 = VECTOR 445 (bench-proven: RSPI-A SPRIA rx) */
    v_rspi_b, v_rspi_b,   /* 442/443 = v446/447 */
    v_rspi_b, v_rspi_b,   /* 444/445 = v448/449 */
    v_rspi_b, v_rspi_b,   /* 446/447 = v450/451 */
    v_rspi_b, v_rspi_b,   /* 448/449 = v452/453 */
    v_rspi_b, v_rspi_b,   /* 450/451 = v454/455 */
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
    default_handler, default_handler, default_handler, default_handler,
};

/* Tail record at 0x17FB70 (outside the CRC range): project identification and end marker. */
__attribute__((section(".app_tail"), used))
const char app_ident[16] = "8HPTCU-OPEN M0";

__attribute__((section(".app_end"), used))
const uint32_t app_end_marker = 0x12345678u;
