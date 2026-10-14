/* Platform stubs for unit tests that link modules calling the platform layer. */
#include "platform/platform.h"

bool plat_can_rx_register(can_bus_t bus, uint16_t id)
{
    (void)bus;
    (void)id;
    return true;
}
