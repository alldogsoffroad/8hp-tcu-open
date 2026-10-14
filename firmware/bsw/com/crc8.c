#include "bsw/com/crc8.h"

uint8_t crc8_j1850(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFFu;

    for (uint8_t i = 0u; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0u; bit < 8u; bit++) {
            crc = (uint8_t)((crc & 0x80u) != 0u ? (uint8_t)(crc << 1) ^ 0x1Du : (uint8_t)(crc << 1));
        }
    }
    return (uint8_t)(crc ^ 0xFFu);
}
