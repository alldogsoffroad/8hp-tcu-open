/* Unit test: user boot MAT read-out routine and its CRC-32. */
#include "app/bench_dump.h"
#include "platform/platform.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void)
{
    uint8_t out[16];
    uint8_t ref[USERBOOT_SIZE];
    uint32_t crc, addr;

    /* zlib CRC-32 check value */
    CHECK(dump_crc32((const uint8_t *)"123456789", 9u) == 0xCBF43926u);

    /* not available (host / failure): status 0 */
    stub.userboot_ok = false;
    CHECK(dump_routine(1u, DUMP_RID, NULL, 0u, out, sizeof out) == 11);
    CHECK(out[0] == 0u);

    /* copy: status 1, length 0x8000, CRC of the copied data, buffer readable at addr */
    stub.userboot_ok = true;
    CHECK(dump_routine(1u, DUMP_RID, NULL, 0u, out, sizeof out) == 11);
    for (uint32_t i = 0u; i < USERBOOT_SIZE; i++) {
        ref[i] = (uint8_t)(i * 7u + 3u);
    }
    crc = (uint32_t)out[7] << 24 | (uint32_t)out[8] << 16 | (uint32_t)out[9] << 8 | out[10];
    addr = (uint32_t)out[1] << 24 | (uint32_t)out[2] << 16 | (uint32_t)out[3] << 8 | out[4];
    CHECK(out[0] == 1u && out[5] == 0x80u && out[6] == 0x00u);
    CHECK(crc == dump_crc32(ref, USERBOOT_SIZE));
    CHECK(addr != 0u);

    /* results repeat the record; wrong RID / extra bytes refused */
    memset(out, 0, sizeof out);
    CHECK(dump_routine(3u, DUMP_RID, NULL, 0u, out, sizeof out) == 11 && out[0] == 1u);
    CHECK(dump_routine(1u, 0xFE21u, NULL, 0u, out, sizeof out) == -0x31);
    CHECK(dump_routine(1u, DUMP_RID, out, 1u, out, sizeof out) == -0x13);

    if (fails == 0) {
        printf("test_dump: ok\n");
    }
    return fails != 0;
}
