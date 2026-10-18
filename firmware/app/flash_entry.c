#include "app/flash_entry.h"
#include "platform/platform.h"

#define NRC_SFNS   0x12
#define NRC_IMLOIF 0x13
#define NRC_CNC    0x22
#define NRC_ROOR   0x31

static uint8_t s_countdown;     /* ms until the flasher starts; 0 = not requested */

int flash_entry_routine(uint8_t type, uint16_t rid, const uint8_t *in, uint16_t n, uint8_t *out,
                        uint16_t max, bool allowed)
{
    (void)in;
    (void)out;
    (void)max;
    if (rid != FLASH_RID) {
        return -NRC_ROOR;
    }
    if (type != 1u) {
        return -NRC_SFNS;
    }
    if (n != 0u) {
        return -NRC_IMLOIF;
    }
    if (!allowed || !plat_flash_fwe()) {
        return -NRC_CNC;
    }
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
        plat_flasher_start();
    }
}
