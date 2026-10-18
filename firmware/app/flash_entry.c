#include "app/flash_entry.h"
#include "platform/platform.h"
#include "bsw/sup/sup.h"

#define NRC_SFNS   0x12
#define NRC_IMLOIF 0x13
#define NRC_CNC    0x22
#define NRC_ROOR   0x31

static uint8_t s_countdown;     /* ms until the flasher starts; 0 = not requested */

static void enable_lines(bool on)
{
    (void)plat_flash_enable(on);
    sup_set_flash_mode(on);
}

static uint8_t fwe_byte(void)
{
    return plat_flash_fwe() ? 0x80u : 0x00u;
}

static int enable_test(uint8_t type, uint16_t n, uint8_t *out, uint16_t max, bool allowed)
{
    if (n != 0u) {
        return -NRC_IMLOIF;
    }
    if (max < 3u) {
        return -NRC_IMLOIF;
    }
    if (type == 1u) {
        if (!allowed) {
            return -NRC_CNC;
        }
        out[0] = fwe_byte();
        out[2] = plat_flash_enable(true) ? 1u : 0u;
        sup_set_flash_mode(true);       /* takes effect with the next 5 ms supervisor frame:
                                           read FD03 afterwards */
        out[1] = fwe_byte();
        return 3;
    }
    if (type == 2u) {
        sup_set_flash_mode(false);
        out[1] = plat_flash_enable(false) ? 1u : 0u;
        out[0] = fwe_byte();
        return 2;
    }
    return -NRC_SFNS;
}

static int sup_pattern(uint8_t type, const uint8_t *in, uint16_t n, uint8_t *out, uint16_t max,
                       bool allowed)
{
    const sup_status_t *st = sup_status();

    if (type == 1u) {
        if (n != 3u) {
            return -NRC_IMLOIF;
        }
        if (!allowed) {
            return -NRC_CNC;
        }
        sup_set_pattern(true, (uint16_t)(in[0] << 8 | in[1]), in[2]);
        return 0;
    }
    if (n != 0u) {
        return -NRC_IMLOIF;
    }
    if (type == 2u) {
        sup_set_pattern(false, 0u, 0u);
        return 0;
    }
    if (max < 5u) {
        return -NRC_IMLOIF;
    }
    out[0] = (uint8_t)(st->last_tx >> 8);
    out[1] = (uint8_t)st->last_tx;
    out[2] = (uint8_t)(st->last_rx >> 8);
    out[3] = (uint8_t)st->last_rx;
    out[4] = fwe_byte();
    return 5;
}

int flash_entry_routine(uint8_t type, uint16_t rid, const uint8_t *in, uint16_t n, uint8_t *out,
                        uint16_t max, bool allowed)
{
    if (rid == SUP_PAT_RID) {
        return sup_pattern(type, in, n, out, max, allowed);
    }
    if (rid == FLASH_EN_RID) {
        return enable_test(type, n, out, max, allowed);
    }
    if (rid != FLASH_RID) {
        return -NRC_ROOR;
    }
    if (type != 1u) {
        return -NRC_SFNS;
    }
    if (n != 0u) {
        return -NRC_IMLOIF;
    }
    if (!allowed) {
        return -NRC_CNC;
    }
    enable_lines(true);                     /* as the user boot MAT does; FWE checked at start */
    s_countdown = FLASH_START_DELAY_MS;     /* time for the positive response to go out */
    return 0;
}

int flash_entry_did(uint8_t *out, uint16_t max)
{
    if (max < 1u) {
        return -1;
    }
    out[0] = plat_flash_fwe() ? 1u : 0u;
    return 1;
}

void flash_entry_tick_1ms(void)
{
    if (s_countdown != 0u && --s_countdown == 0u) {
        if (plat_flash_fwe()) {
            plat_flasher_start();
        } else {
            enable_lines(false);            /* FWE did not follow: stay in the application */
        }
    }
}
