#include "bsw/com/sigpack.h"

void sig_put(uint8_t *data, sig_t s, uint16_t value)
{
    /* bit index counted from the start of the frame, MSB of byte 0 = 0 */
    uint16_t pos = (uint16_t)(s.byte * 8u + (7u - s.msb));

    for (uint8_t i = 0u; i < s.len; i++, pos++) {
        const uint8_t mask = (uint8_t)(0x80u >> (pos & 7u));
        if (((value >> (s.len - 1u - i)) & 1u) != 0u) {
            data[pos >> 3] = (uint8_t)(data[pos >> 3] | mask);
        } else {
            data[pos >> 3] = (uint8_t)(data[pos >> 3] & (uint8_t)~mask);
        }
    }
}

uint16_t sig_get(const uint8_t *data, sig_t s)
{
    uint16_t pos = (uint16_t)(s.byte * 8u + (7u - s.msb));
    uint16_t v = 0u;

    for (uint8_t i = 0u; i < s.len; i++, pos++) {
        v = (uint16_t)(v << 1 | ((data[pos >> 3] >> (7u - (pos & 7u))) & 1u));
    }
    return v;
}
