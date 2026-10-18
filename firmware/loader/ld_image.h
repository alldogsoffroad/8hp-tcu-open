/* Application image in data flash (spec 18 section 4).
 *
 *   +0x00  magic "8HPAPP01"
 *   +0x08  payload length in bytes (multiple of 128, 1..LD_MAX_PAYLOAD)
 *   +0x0C  RAM load address (LD_LOAD_ADDR)
 *   +0x10  entry address (inside the payload)
 *   +0x14  CRC-32 (zlib) of the payload
 *   +0x18  version text, 16 bytes
 *   +0x80  payload: the application linked for RAM
 * All values big-endian. The header occupies the first 128-byte programming unit of DB00 and is
 * programmed last, so an interrupted update never leaves a header over half-written data. */
#ifndef TCU_LD_IMAGE_H
#define TCU_LD_IMAGE_H

#include <stdbool.h>
#include <stdint.h>

#define LD_HDR_LEN      0x80u
#define LD_LOAD_ADDR    0xFFF84100u     /* application RAM start (boot area below is untouched) */
#define LD_RAM_LIMIT    0xFFF9C000u     /* loader's own data and stack live above this */
#define LD_DF_LEN       0x10000u        /* DB00-DB07 */
#define LD_MAX_PAYLOAD  (LD_DF_LEN - LD_HDR_LEN)

typedef struct {
    uint32_t len, load, entry, crc;
    char version[16];
} ld_image_t;

/* Field checks only (magic, length, load address, entry); the CRC is checked by the caller over
 * the payload in data flash. Returns false for anything that must not be run. */
bool ld_image_parse(const uint8_t hdr[LD_HDR_LEN], ld_image_t *img);

/* zlib CRC-32: c = ld_crc32(0xFFFFFFFF, ...) chained over chunks, result ~c. */
uint32_t ld_crc32(uint32_t c, const uint8_t *d, uint32_t n);

#endif
