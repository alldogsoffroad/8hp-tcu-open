/* SAE J1850 CRC-8 as used by the protected receive messages (spec 07 section 2). */
#ifndef TCU_CRC8_H
#define TCU_CRC8_H

#include <stdint.h>

/* poly 0x1D, init 0xFF, no reflection, final XOR 0xFF. Check value of "123456789" is 0x4B. */
uint8_t crc8_j1850(const uint8_t *data, uint8_t len);

#endif
