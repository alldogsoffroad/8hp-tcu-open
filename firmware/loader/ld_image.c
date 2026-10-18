#include "loader/ld_image.h"

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

bool ld_image_parse(const uint8_t hdr[LD_HDR_LEN], ld_image_t *img)
{
    static const char magic[8] = { '8', 'H', 'P', 'A', 'P', 'P', '0', '1' };

    for (uint32_t i = 0u; i < 8u; i++) {
        if (hdr[i] != (uint8_t)magic[i]) {
            return false;
        }
    }
    img->len = get32(&hdr[0x08]);
    img->load = get32(&hdr[0x0C]);
    img->entry = get32(&hdr[0x10]);
    img->crc = get32(&hdr[0x14]);
    for (uint32_t i = 0u; i < 16u; i++) {
        img->version[i] = (char)hdr[0x18u + i];
    }
    if (img->len == 0u || img->len > LD_MAX_PAYLOAD || (img->len & 0x7Fu) != 0u) {
        return false;
    }
    if (img->load != LD_LOAD_ADDR || img->len > LD_RAM_LIMIT - img->load) {
        return false;
    }
    if (img->entry < img->load || img->entry >= img->load + img->len || (img->entry & 1u) != 0u) {
        return false;
    }
    return true;
}

uint32_t ld_crc32(uint32_t c, const uint8_t *d, uint32_t n)
{
    static const uint32_t t[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u,
        0x4DB26158u, 0x5005713Cu, 0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
    };

    for (uint32_t i = 0u; i < n; i++) {
        c ^= d[i];
        c = (c >> 4) ^ t[c & 0x0Fu];
        c = (c >> 4) ^ t[c & 0x0Fu];
    }
    return c;
}
