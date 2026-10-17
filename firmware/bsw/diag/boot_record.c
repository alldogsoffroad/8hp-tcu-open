/* Field layout and values from the stock writers 0x73FC4, 0x734FE and 0x75E9C (spec 06 5a).
 * The timing words (+0x08..+0x18) and bytes +0x29 / +0x2E are the values the stock application
 * holds after a diagnostic connection is set up, read on the bench spare with the measurement
 * protocol (2026-10-17, spec 06 5a); their roles are still [I]. */
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
    put32(&rec[0x0C], 40000u);          /* stock bench value; [I] P2 = 40 ms, us */
    put32(&rec[0x10], 5000000u);        /* stock bench value; [I] 5 s */
    put32(&rec[0x14], 5000u);           /* stock bench value */
    put32(&rec[0x18], 500000u);         /* stock bench value; [I] 500 ms */
    rec[0x29] = 0x18u;                  /* stock bench value */
    rec[0x2E] = 0x18u;                  /* stock bench value */
    rec[0x24] = 'w';                    /* request active */
    rec[0x35] = 0u;                     /* only the request is stored */
    rec[0x36] = 2u;                     /* stored message length incl. SID */
    rec[0x37] = sid;
    rec[0x38] = sub;
    put16(&rec[0x44], 0x5AA5u);
}
