/* Application block header, vector table and tail record (spec 06 sections 2-3).
 *
 * Layout reproduced from the stock application header; fields whose meaning is not known are
 * copied unchanged. The CRC at +0x44 is filled in by tools/build_image.py after linking.
 * The Bosch boot manager (0x24FD0) checks: magic 0x876543xx, end pointer (+0x0C) in range and
 * 4-byte aligned, and the word at the end pointer == 0x12345678. */
#include <stdint.h>

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

/* Unexpected exception or interrupt: stop and let the watchdog reset the controller. */
static void default_handler(void) __attribute__((interrupt_handler));
static void default_handler(void)
{
    for (;;) {
    }
}

/* E58 (spec 03 section 6.2ap): the FULL RSPI-A ISR protocol, transplanted from the
 * stock's vector 445 handler (0x96590, completely disassembled):
 *   1. SPCR  = 0x08                    (arm the 0x08 mode)
 *   2. SPSR  = 0x20                    (clear the overrun flag)
 *   3. gate  = read @(0xFFFE6030)      (the CPG clock-gate status)
 *      @(0xFFFE6030) = 0xF3            (ENABLE the RSPI clock gates — bits 0-1 clear;
 *                                        WITHOUT this the shared clock domain drops and
 *                                        BOTH buses die: E57's verdict)
 *   The stock's steps 1-2 are already done by our feed's packed store; the gate write
 *   is the essential addition. The worker dispatch (0x93A08/0x93A74) is the stock's
 *   mainline transaction machinery — our feed runs from the scheduler instead. The
 *   timing-instrumentation lock/unlock (0x68F34/0x68F3E) is skippable. */
__attribute__((interrupt_handler))
void rspi_a_isr(void)
{
    volatile uint8_t *const spsr = (volatile uint8_t *)0xFFFFB003u;
    volatile uint8_t *const gate = (volatile uint8_t *)0xFFFE6030u;

    (void)*spsr;                         /* read: the flags that fired this interrupt */
    *spsr = 0x20u;                       /* clear the overrun flag (the stock's value) */
    (void)*gate;                         /* read: the gate status (the stock saves it) */
    *gate = 0xF3u;                       /* the clock-gate ENABLE: bits 0-1 clear      */
}

/* Vectors 4..511 (508 entries), at header +0x80.
 * Layout: 7 x V64 (vectors 4-259) | V16 x3 (260-307) | V4 x3 (308-319)
 * then the tail: V4, V4 = vectors 320-327... vector 445 = index 441 (0-based). */
#define V4(x) x, x, x, x
#define V16(x) V4(x), V4(x), V4(x), V4(x)
#define V64(x) V16(x), V16(x), V16(x), V16(x)
__attribute__((section(".app_vectors"), used))
void (*const app_vectors[508])(void) = {
    /* vectors 4..259 (indices 0..255): 4 x V64 */
    V64(default_handler), V64(default_handler), V64(default_handler), V64(default_handler),
    /* vectors 260..307 (indices 256..303): 3 x V16 */
    V16(default_handler), V16(default_handler), V16(default_handler),
    /* vectors 308..319 (indices 304..315): 3 x V4 */
    V4(default_handler), V4(default_handler), V4(default_handler),
    /* indices 316..440 (125 default entries) — 64 + 48 + 12 + 1 */
    V64(default_handler),
    V16(default_handler), V16(default_handler), V16(default_handler),
    V4(default_handler), V4(default_handler), V4(default_handler),
    default_handler,
    /* index 441 = VECTOR 445: the RSPI-A interrupt (stock handler 0x96590) */
    rspi_a_isr,
    /* indices 442..507 (66 default entries) — 64 + 2 */
    V64(default_handler),
    default_handler, default_handler,
};

/* Tail record at 0x17FB70 (outside the CRC range): project identification and end marker. */
__attribute__((section(".app_tail"), used))
const char app_ident[16] = "8HPTCU-OPEN M0";

__attribute__((section(".app_end"), used))
const uint32_t app_end_marker = 0x12345678u;
