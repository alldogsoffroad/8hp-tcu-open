/* Big-endian (Motorola) bit fields in CAN data, as used on the FCA bus (spec 07/08):
 * a field starts at bit `msb` of byte `byte` and continues into the following bytes. */
#ifndef TCU_SIGPACK_H
#define TCU_SIGPACK_H

#include <stdint.h>

typedef struct {
    uint8_t byte;   /* byte holding the most significant bit */
    uint8_t msb;    /* bit number 7..0 within that byte      */
    uint8_t len;    /* 1..16                                 */
} sig_t;

/* Write the low `len` bits of value; other bits of the frame are kept. */
void sig_put(uint8_t *data, sig_t s, uint16_t value);
uint16_t sig_get(const uint8_t *data, sig_t s);

#endif
