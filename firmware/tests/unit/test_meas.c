/* Unit test: measurement protocol (spec 10 read-only subset). */
#include "bsw/meas/meas.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static uint16_t v16 = 0x1234u;
static uint8_t v8 = 0x56u;
static const meas_index_t idx[] = { { &v16, 2u }, { &v8, 1u } };     /* indices 2, 3 */
static const meas_cfg_t cfg = { idx, 2u, "8HPTCU-OPEN M1" };

static bool req(can_bus_t bus, const uint8_t d[8])
{
    can_frame_t f = { .id = 0x361u, .dlc = 8u };
    memcpy(f.data, d, 8u);
    return meas_rx(bus, &f);
}

static bool last_is(uint16_t id, can_bus_t bus, const uint8_t d[8])
{
    return stub.n_tx != 0u && stub.tx[stub.n_tx - 1u].id == id &&
           stub.tx_bus[stub.n_tx - 1u] == bus && memcmp(stub.tx[stub.n_tx - 1u].data, d, 8u) == 0;
}

int main(void)
{
    for (unsigned i = 0u; i < sizeof stub.ram; i++) {
        stub.ram[i] = (uint8_t)i;
    }
    meas_init(&cfg);

    /* word read, A16: 0x80, 0x82 readable, 0x1000 outside the readable area -> FFFF */
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0x20, 0, 0x00, 0x80, 0x00, 0x82, 0x10, 0x00 }));
    CHECK(last_is(0x398u, CAN_BUS2, (const uint8_t[8]){ 0x20, 0x81, 0x80, 0x81, 0x82, 0x83, 0xFF, 0xFF }));

    /* the first bus is latched: a request on bus 1 is consumed but not answered */
    stub.n_tx = 0u;
    CHECK(req(CAN_BUS1, (const uint8_t[8]){ 0x20, 0, 0x00, 0x80, 0, 0, 0, 0 }));
    CHECK(stub.n_tx == 0u);

    /* block reads: 0x2A at 32-bit address, 0x28 at a 16-bit absolute address (denied: 0x0E) */
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0x2A, 0, 0xFF, 0xF8, 0x00, 0x80, 0, 0 }));
    CHECK(last_is(0x398u, CAN_BUS2, (const uint8_t[8]){ 0x2A, 0x81, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85 }));
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0x28, 0, 0x00, 0x80, 0, 0, 0, 0 }));
    CHECK(last_is(0x398u, CAN_BUS2, (const uint8_t[8]){ 0x28, 0x81, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E }));

    /* A24 reads: 0xF80010 readable, 0x000000 not */
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0x30, 0, 0xF8, 0x00, 0x10, 0x00, 0x00, 0x00 }));
    CHECK(last_is(0x398u, CAN_BUS2, (const uint8_t[8]){ 0x30, 0x81, 0x10, 0x11, 0x12, 0x0E, 0x0E, 0x0E }));

    /* index address: host variables are outside 0xFFF8xxxx -> 0x22 gives FFFF; unknown index */
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0x22, 0, 0x00, 0x02, 0, 0, 0, 0 }));
    CHECK(last_is(0x398u, CAN_BUS2, (const uint8_t[8]){ 0x22, 0x81, 0xFF, 0xFF, 0, 0, 0, 0 }));
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0x11, 0, 0x00, 0x63, 0, 0, 0, 0 }));
    CHECK(last_is(0x398u, CAN_BUS2, (const uint8_t[8]){ 0x11, 0x81, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0 }));

    /* writes and unverified commands are rejected */
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0x0A, 0, 0x00, 0x80, 0x12, 0x34, 0, 0 }));
    CHECK(last_is(0x398u, CAN_BUS2, (const uint8_t[8]){ 0x03, 0, 0, 0, 0, 0, 0, 0 }));
    CHECK(stub.ram[0x80] == 0x80u);
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0x08, 0, 0, 0, 0, 0, 0, 0 }));
    CHECK(last_is(0x398u, CAN_BUS2, (const uint8_t[8]){ 0x03, 0, 0, 0, 0, 0, 0, 0 }));

    /* identification in parts */
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0x01, 0, 0, 0, 0, 0, 0, 0 }));
    CHECK(last_is(0x398u, CAN_BUS2, (const uint8_t[8]){ 0x21, 0x01, '8', 'H', 'P', 'T', 'C', 'U' }));
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0x21, 0, 0, 0, 0, 0, 0, 0 }));
    CHECK(last_is(0x398u, CAN_BUS2, (const uint8_t[8]){ 0x21, 0x02, '-', 'O', 'P', 'E', 'N', ' ' }));
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0x21, 0, 0, 0, 0, 0, 0, 0 }));
    CHECK(last_is(0x398u, CAN_BUS2, (const uint8_t[8]){ 0x21, 0x03, 'M', '1', 0, 0, 0, 0 }));

    /* periodic list: frame 1 = indices 2 (2 bytes), 3 (1 byte), 99 (unknown);
     * last frame 0x82 = index 3. Channels: 12 34 56 00 56 00 ... */
    stub.n_tx = 0u;
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0xE0, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x63 }));
    CHECK(stub.n_tx == 0u);
    meas_tick_10ms();
    CHECK(stub.n_tx == 0u);                 /* list not complete yet */
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0xE0, 0x82, 0x00, 0x03, 0, 0, 0, 0 }));
    meas_tick_10ms();
    CHECK(stub.n_tx == 4u);
    CHECK(stub.tx[0].id == 0x368u && stub.tx[1].id == 0x378u && stub.tx[2].id == 0x388u && stub.tx[3].id == 0x398u);
    CHECK(memcmp(stub.tx[0].data, (const uint8_t[8]){ 0x12, 0x34, 0x56, 0x00, 0x56, 0, 0, 0 }, 8u) == 0);
    v16 = 0xABCDu;
    meas_tick_10ms();
    CHECK(stub.n_tx == 8u && stub.tx[4].data[0] == 0xABu && stub.tx[4].data[1] == 0xCDu);

    /* 0x02 stops periodic data */
    CHECK(req(CAN_BUS2, (const uint8_t[8]){ 0x02, 0, 0, 0, 0, 0, 0, 0 }));
    CHECK(last_is(0x398u, CAN_BUS2, (const uint8_t[8]){ 0x01, 0, 0, 0, 0, 0, 0, 0 }));
    stub.n_tx = 0u;
    meas_tick_10ms();
    CHECK(stub.n_tx == 0u);

    /* other identifiers are not consumed */
    {
        can_frame_t f = { .id = 0x362u, .dlc = 8u };
        CHECK(!meas_rx(CAN_BUS2, &f));
    }

    if (fails == 0) {
        printf("test_meas: ok\n");
    }
    return fails != 0;
}
