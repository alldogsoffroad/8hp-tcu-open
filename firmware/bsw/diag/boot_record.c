/* Field layout and values from the stock writers 0x73FC4, 0x734FE and 0x75E9C (spec 06 5a).
 * The timing words (+0x08..+0x18) are written by the stock application from its session
 * tables; their exact roles are [I] and must be checked on the bench by reading the stock
 * record with the measurement protocol (spec 10) before the hand-over is relied on. */
#include "bsw/diag/boot_record.h"

#include <string.h>

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, (uint16_t)(v >> 16));
    put16(p + 2, (uint16_t)v);
}

void boot_record_build_prog(uint8_t rec[BOOT_REC_LEN], uint8_t sid, uint8_t sub)
{
    memset(rec, 0, BOOT_REC_LEN);
    put16(&rec[0x00], 0xA55Au);
    put16(&rec[0x02], BOOT_REC_LEN);
    rec[0x04] = 3u;                     /* source: application */
    rec[0x05] = 2u;                     /* target: boot manager (session 0x02 path) */
    put32(&rec[0x08], 5000000u);        /* [I] session timeout, us */
    put32(&rec[0x0C], 50000u);          /* [I] P2, us */
    put32(&rec[0x10], 5000000u);        /* [I] P2*, us */
    put32(&rec[0x18], 500000u);         /* [I] connection timing, us */
    rec[0x24] = 'w';                    /* request active */
    rec[0x35] = 0u;                     /* only the request is stored */
    rec[0x36] = 2u;                     /* stored message length incl. SID */
    rec[0x37] = sid;
    rec[0x38] = sub;
    put16(&rec[0x44], 0x5AA5u);
}
