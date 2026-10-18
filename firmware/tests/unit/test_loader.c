/* Unit test: data-flash image header checks and CRC (spec 18 section 4). */
#include "loader/ld_image.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void make(uint8_t h[LD_HDR_LEN], uint32_t len, uint32_t load, uint32_t entry)
{
    memset(h, 0, LD_HDR_LEN);
    memcpy(h, "8HPAPP01", 8);
    put32(&h[0x08], len);
    put32(&h[0x0C], load);
    put32(&h[0x10], entry);
    put32(&h[0x14], 0x12345678u);
    memcpy(&h[0x18], "RAM 261018", 10);
}

int main(void)
{
    uint8_t h[LD_HDR_LEN];
    ld_image_t img;

    CHECK(~ld_crc32(0xFFFFFFFFu, (const uint8_t *)"123456789", 9u) == 0xCBF43926u);
    /* chained chunks give the same result */
    CHECK(~ld_crc32(ld_crc32(0xFFFFFFFFu, (const uint8_t *)"1234", 4u), (const uint8_t *)"56789", 5u)
          == 0xCBF43926u);

    make(h, 0x5880u, LD_LOAD_ADDR, 0xFFF84900u);
    CHECK(ld_image_parse(h, &img));
    CHECK(img.len == 0x5880u && img.entry == 0xFFF84900u && img.crc == 0x12345678u);
    CHECK(memcmp(img.version, "RAM 261018", 10) == 0);

    make(h, 0x5880u, LD_LOAD_ADDR, 0xFFF84900u); h[0] = '9';            CHECK(!ld_image_parse(h, &img));
    make(h, 0u, LD_LOAD_ADDR, LD_LOAD_ADDR);                              CHECK(!ld_image_parse(h, &img));
    make(h, 0x5881u, LD_LOAD_ADDR, 0xFFF84900u);                          CHECK(!ld_image_parse(h, &img));
    make(h, LD_MAX_PAYLOAD + 0x80u, LD_LOAD_ADDR, 0xFFF84900u);          CHECK(!ld_image_parse(h, &img));
    make(h, LD_MAX_PAYLOAD, LD_LOAD_ADDR, 0xFFF84900u);                   CHECK(ld_image_parse(h, &img));
    make(h, 0x5880u, 0xFFF80000u, 0xFFF80800u);                          CHECK(!ld_image_parse(h, &img));
    make(h, 0x5880u, LD_LOAD_ADDR, LD_LOAD_ADDR + 0x5880u);              CHECK(!ld_image_parse(h, &img));
    make(h, 0x5880u, LD_LOAD_ADDR, LD_LOAD_ADDR - 2u);                   CHECK(!ld_image_parse(h, &img));
    make(h, 0x5880u, LD_LOAD_ADDR, 0xFFF84901u);                          CHECK(!ld_image_parse(h, &img));
    /* erased data flash (undefined content) must not pass */
    memset(h, 0xFF, sizeof h);                                            CHECK(!ld_image_parse(h, &img));
    memset(h, 0x00, sizeof h);                                            CHECK(!ld_image_parse(h, &img));

    printf("%s (%d failures)\n", fails ? "FAILED" : "passed", fails);
    return fails ? 1 : 0;
}
