/* Platform stubs for unit tests: capture transmitted frames, resets and hand-overs. */
#include "tests/unit/stub_plat.h"

#include <string.h>

stub_plat_t stub;

bool plat_can_rx_register(can_bus_t bus, uint16_t id)
{
    (void)bus;
    (void)id;
    return true;
}

bool plat_can_send(can_bus_t bus, const can_frame_t *f)
{
    if (stub.n_tx >= STUB_TX_MAX) {
        return false;
    }
    stub.tx_bus[stub.n_tx] = bus;
    stub.tx[stub.n_tx++] = *f;
    return true;
}

void plat_reset(void)
{
    stub.resets++;
}

void plat_boot_handover(const uint8_t *rec, uint8_t len)
{
    memcpy(stub.rec, rec, len);
    stub.handovers++;
}

bool plat_mem_read(uint32_t addr, uint8_t *out, uint16_t n)
{
    if (addr < 0xFFF80000u || addr - 0xFFF80000u + n > sizeof stub.ram) {
        return false;
    }
    memcpy(out, &stub.ram[addr - 0xFFF80000u], n);
    return true;
}
