/* Unit test: CRC-8, integrity checks, counter rule and timeouts of the receive layer. */
#include "bsw/com/com.h"
#include "bsw/com/crc8.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static const com_rx_cfg_t cfg[] = {
    { 0x108u, CAN_BUS1, 8u, 50u, COM_E2E_V1, 2u },
    { 0x180u, CAN_BUS1, 8u, 200u, COM_E2E_NONE, 0u },
    { 0x2EAu, CAN_BUS1, 5u, 50u, COM_E2E_V2, 1u },
};
static com_rx_state_t st[3];

/* Build a protected frame: counter in the upper nibble of byte dlc-2, CRC in byte dlc-1. */
static can_frame_t frame(uint16_t id, uint8_t dlc, com_e2e_t e2e, uint8_t ctr, uint8_t fill)
{
    can_frame_t f = { .id = id, .dlc = dlc };
    uint8_t inv[8];

    memset(f.data, fill, dlc);
    f.data[dlc - 2u] = (uint8_t)((f.data[dlc - 2u] & 0x0Fu) | (uint8_t)(ctr << 4));
    if (e2e == COM_E2E_V1) {
        f.data[dlc - 1u] = crc8_j1850(f.data, (uint8_t)(dlc - 1u));
    } else if (e2e == COM_E2E_V2) {
        for (uint8_t i = 0u; i < dlc - 1u; i++) {
            inv[i] = (uint8_t)~f.data[i];
        }
        f.data[dlc - 1u] = (uint8_t)~crc8_j1850(inv, (uint8_t)(dlc - 1u));
    }
    return f;
}

int main(void)
{
    const uint8_t check[] = "123456789";
    uint8_t d[8];
    can_frame_t f;

    CHECK(crc8_j1850(check, 9u) == 0x4Bu);          /* published check value */

    com_init(cfg, st, 3u);
    CHECK(com_rx_read(0u, d) == COM_RX_NEVER);

    /* valid frame accepted, wrong bus and short DLC rejected */
    f = frame(0x108u, 8u, COM_E2E_V1, 3u, 0x11u);
    CHECK(com_rx_indication(CAN_BUS2, &f) == -1);
    CHECK(com_rx_indication(CAN_BUS1, &f) == 0);
    CHECK(com_rx_read(0u, d) == COM_RX_OK && d[0] == 0x11u);
    f.dlc = 7u;
    CHECK(com_rx_indication(CAN_BUS1, &f) == -1 && st[0].err_dlc == 1u);

    /* corrupted CRC rejected, data kept */
    f = frame(0x108u, 8u, COM_E2E_V1, 4u, 0x22u);
    f.data[0] ^= 0x01u;
    CHECK(com_rx_indication(CAN_BUS1, &f) == -1 && st[0].err_crc == 1u);
    CHECK(com_rx_read(0u, d) == COM_RX_OK && d[0] == 0x11u);

    /* counter: +1 and +2 accepted (max step 2), repeat and +3 rejected, wraps mod 16 */
    f = frame(0x108u, 8u, COM_E2E_V1, 4u, 0x22u);
    CHECK(com_rx_indication(CAN_BUS1, &f) == 0);
    f = frame(0x108u, 8u, COM_E2E_V1, 6u, 0x23u);
    CHECK(com_rx_indication(CAN_BUS1, &f) == 0);
    CHECK(com_rx_indication(CAN_BUS1, &f) == -1 && st[0].err_counter == 1u);
    f = frame(0x108u, 8u, COM_E2E_V1, 9u, 0x24u);
    CHECK(com_rx_indication(CAN_BUS1, &f) == -1 && st[0].err_counter == 2u);
    f = frame(0x108u, 8u, COM_E2E_V1, 10u, 0x25u);   /* resynchronised on 9 */
    CHECK(com_rx_indication(CAN_BUS1, &f) == 0);
    for (uint8_t c = 11u; c < 20u; c++) {
        f = frame(0x108u, 8u, COM_E2E_V1, (uint8_t)(c & 0x0Fu), 0x26u);
        CHECK(com_rx_indication(CAN_BUS1, &f) == 0);
    }

    /* variant 2: inverted CRC, counter counting down */
    f = frame(0x2EAu, 5u, COM_E2E_V2, 5u, 0x01u);
    CHECK(com_rx_indication(CAN_BUS1, &f) == 2);
    f = frame(0x2EAu, 5u, COM_E2E_V2, 4u, 0x01u);
    CHECK(com_rx_indication(CAN_BUS1, &f) == 2);
    f = frame(0x2EAu, 5u, COM_E2E_V2, 5u, 0x01u);
    CHECK(com_rx_indication(CAN_BUS1, &f) == -1);
    f = frame(0x2EAu, 5u, COM_E2E_V1, 3u, 0x01u);   /* variant-1 checksum on a variant-2 message */
    CHECK(!com_e2e_check(COM_E2E_V2, f.data, 5u));
    CHECK(com_rx_indication(CAN_BUS1, &f) == -1);

    /* unprotected message and timeouts */
    f = frame(0x180u, 8u, COM_E2E_NONE, 0u, 0x55u);
    CHECK(com_rx_indication(CAN_BUS1, &f) == 1);
    com_tick(50u);
    CHECK(com_rx_read(0u, d) == COM_RX_OK);
    com_tick(10u);
    CHECK(com_rx_read(0u, d) == COM_RX_TIMEOUT && d[0] == 0x26u);
    CHECK(com_rx_read(1u, d) == COM_RX_OK);
    com_tick(150u);
    CHECK(com_rx_read(1u, d) == COM_RX_TIMEOUT);

    /* after a timeout the counter resynchronises on any value */
    f = frame(0x108u, 8u, COM_E2E_V1, 0u, 0x30u);
    CHECK(com_rx_indication(CAN_BUS1, &f) == 0);
    CHECK(com_rx_read(0u, d) == COM_RX_OK && d[0] == 0x30u);

    if (fails == 0) {
        printf("test_com: ok\n");
    }
    return fails != 0;
}
