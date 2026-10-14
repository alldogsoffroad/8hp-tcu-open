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

/* Vectors 4..511 (508 entries), at header +0x80. */
#define V4(x) x, x, x, x
#define V16(x) V4(x), V4(x), V4(x), V4(x)
#define V64(x) V16(x), V16(x), V16(x), V16(x)
__attribute__((section(".app_vectors"), used))
void (*const app_vectors[508])(void) = {
    V64(default_handler), V64(default_handler), V64(default_handler), V64(default_handler),
    V64(default_handler), V64(default_handler), V64(default_handler),
    V16(default_handler), V16(default_handler), V16(default_handler), V4(default_handler),
    V4(default_handler), V4(default_handler),
};

/* Tail record at 0x17FB70 (outside the CRC range): project identification and end marker. */
__attribute__((section(".app_tail"), used))
const char app_ident[16] = "8HPTCU-OPEN M0";

__attribute__((section(".app_end"), used))
const uint32_t app_end_marker = 0x12345678u;
