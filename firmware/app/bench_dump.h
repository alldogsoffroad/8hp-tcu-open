/* Bench read-out of the user boot MAT (the 32 KB start-up area the ACDP-2 does not read).
 *
 * UDS RoutineControl FE20, extended session only, never run automatically:
 *   start   31 01 FE 20 -> 71 01 FE 20 status addr(4) len(2) crc32(4)
 *   results 31 03 FE 20 -> same record
 *   status 1 = copied; 0 = not available. The copy lands in a RAM buffer at addr; read it with the
 *   measurement protocol (0x2A) and check it against crc32 (zlib CRC-32).
 */
#ifndef TCU_BENCH_DUMP_H
#define TCU_BENCH_DUMP_H

#include "tcu/units.h"

#define DUMP_RID 0xFE20u

int dump_routine(uint8_t type, uint16_t rid, const uint8_t *in, uint16_t n, uint8_t *out,
                 uint16_t max);
uint32_t dump_crc32(const uint8_t *d, uint32_t n);

#endif
