#include "app/bench_dump.h"
#include "platform/platform.h"

#define NRC_IMLOIF 0x13
#define NRC_ROOR   0x31

static uint8_t s_buf[USERBOOT_SIZE] __attribute__((aligned(4)));
static bool s_ok;
static uint32_t s_crc;

uint32_t dump_crc32(const uint8_t *d, uint32_t n)
{
    static const uint32_t t[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u,
        0x4DB26158u, 0x5005713Cu, 0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
    };
    uint32_t c = 0xFFFFFFFFu;

    for (uint32_t i = 0u; i < n; i++) {
        c ^= d[i];
        c = (c >> 4) ^ t[c & 0x0Fu];
        c = (c >> 4) ^ t[c & 0x0Fu];
    }
    return ~c;
}

static int record(uint8_t *out, uint16_t max)
{
    const uint32_t a = (uint32_t)(uintptr_t)s_buf;

    if (max < 11u) {
        return -NRC_IMLOIF;
    }
    out[0] = s_ok ? 1u : 0u;
    out[1] = (uint8_t)(a >> 24);
    out[2] = (uint8_t)(a >> 16);
    out[3] = (uint8_t)(a >> 8);
    out[4] = (uint8_t)a;
    out[5] = (uint8_t)(USERBOOT_SIZE >> 8);
    out[6] = (uint8_t)USERBOOT_SIZE;
    out[7] = (uint8_t)(s_crc >> 24);
    out[8] = (uint8_t)(s_crc >> 16);
    out[9] = (uint8_t)(s_crc >> 8);
    out[10] = (uint8_t)s_crc;
    return 11;
}

int dump_routine(uint8_t type, uint16_t rid, const uint8_t *in, uint16_t n, uint8_t *out,
                 uint16_t max)
{
    (void)in;
    if (rid != DUMP_RID) {
        return -NRC_ROOR;
    }
    if (n != 0u) {
        return -NRC_IMLOIF;
    }
    if (type == 1u) {
        s_ok = plat_userboot_copy(s_buf, USERBOOT_SIZE);
        s_crc = s_ok ? dump_crc32(s_buf, USERBOOT_SIZE) : 0u;
    }
    return record(out, max);
}
