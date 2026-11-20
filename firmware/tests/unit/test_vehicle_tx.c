/* The neutral transmit seam (io/vehicle/vehicle_tx.h) over the FCA transmit set: the
 * application calls only vehicle_tx_*; the FCA torque-field guard (0x037 / 0x2F9 refuse to
 * enable until every 13-bit torque field is set) survives, and the frames are the ones
 * fca_tx produces. */
#include "io/vehicle/vehicle_tx.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static unsigned count_id(uint16_t id)
{
    unsigned n = 0u;
    for (unsigned i = 0u; i < stub.n_tx; i++) {
        n += stub.tx[i].id == id;
    }
    return n;
}

int main(void)
{
    vehicle_out_t o = { { 2000u, 2000u, 2000u }, false };

    vehicle_tx_init();
    /* nothing set: the torque messages refuse (an all-zero field = -500 Nm), nothing is sent */
    CHECK(!vehicle_tx_enable_torque(true));
    stub.n_tx = 0u;
    for (int i = 0; i < 40; i++) {
        vehicle_tx_tick_1ms();
    }
    CHECK(count_id(0x037u) == 0u && count_id(0x2F9u) == 0u);
    /* a set that is not marked valid changes nothing */
    vehicle_tx_set(&o);
    CHECK(!vehicle_tx_enable_torque(true));
    /* valid fields: both messages enable and go out at their 16 ms period */
    o.torque_request_valid = true;
    vehicle_tx_set(&o);
    CHECK(vehicle_tx_enable_torque(true));
    stub.n_tx = 0u;
    for (int i = 0; i < 40; i++) {
        vehicle_tx_tick_1ms();
    }
    CHECK(count_id(0x037u) >= 2u && count_id(0x2F9u) >= 2u);
    for (unsigned i = 0u; i < stub.n_tx; i++) {
        if (stub.tx[i].id == 0x037u) {
            /* 13-bit field at byte 0 bits 4..0 + byte 1: 2000 = 0x07D0 */
            CHECK((stub.tx[i].data[0] & 0x1Fu) == 0x07u && stub.tx[i].data[1] == 0xD0u);
            break;
        }
    }
    CHECK(vehicle_tx_enable_torque(false));
    stub.n_tx = 0u;
    for (int i = 0; i < 40; i++) {
        vehicle_tx_tick_1ms();
    }
    CHECK(count_id(0x037u) == 0u && count_id(0x2F9u) == 0u);
    /* re-init forgets the "written" marks: the guard is back */
    vehicle_tx_init();
    CHECK(!vehicle_tx_enable_torque(true));

    printf(fails == 0 ? "vehicle_tx OK\n" : "vehicle_tx FAILED (%d)\n", fails);
    return fails == 0 ? 0 : 1;
}
