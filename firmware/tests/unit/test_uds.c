/* Unit test: ISO-TP transport and UDS server, including the programming-session hand-over. */
#include "bsw/diag/uds.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static bool s_allowed = true;
static bool allowed(void) { return s_allowed; }
static int app_did(uint16_t did, uint8_t *out, uint16_t max)
{
    if (did == 0xFD00u && max >= 2u) {
        out[0] = 0x12u;
        out[1] = 0x34u;
        return 2;
    }
    return -1;
}
static const uds_app_t app = { allowed, app_did, NULL };

static void rx(can_bus_t bus, uint16_t id, const uint8_t *d, uint8_t n)
{
    can_frame_t f = { .id = id, .dlc = 8u };
    memset(f.data, 0xCCu, 8u);
    memcpy(f.data, d, n);
    CHECK(uds_rx(bus, &f));
}

static void ticks(unsigned n)
{
    while (n-- != 0u) {
        uds_tick_1ms();
    }
}

/* last transmitted frame equals the given bytes (rest is padding) */
static bool last_is(uint16_t id, const uint8_t *d, uint8_t n)
{
    const can_frame_t *f;
    if (stub.n_tx == 0u) {
        return false;
    }
    f = &stub.tx[stub.n_tx - 1u];
    return f->id == id && f->dlc == 8u && memcmp(f->data, d, n) == 0;
}

int main(void)
{
    uds_init(&app);

    /* session control: extended */
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x02, 0x10, 0x03 }, 3u);
    ticks(1u);
    CHECK(last_is(0x7E9u, (const uint8_t[]){ 0x06, 0x50, 0x03, 0x00, 0x32, 0x01, 0xF4 }, 7u));
    CHECK(uds_session() == 0x03u);

    /* tester present with suppress bit: no response */
    stub.n_tx = 0u;
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x02, 0x3E, 0x80 }, 3u);
    ticks(1u);
    CHECK(stub.n_tx == 0u);

    /* unsupported service: NRC 0x11 physically, silence functionally */
    rx(CAN_BUS1, 0x7DFu, (const uint8_t[]){ 0x02, 0x85, 0x01 }, 3u);
    ticks(1u);
    CHECK(stub.n_tx == 0u);
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x02, 0x85, 0x01 }, 3u);
    ticks(1u);
    CHECK(last_is(0x7E9u, (const uint8_t[]){ 0x03, 0x7F, 0x85, 0x11 }, 4u));

    /* read software version (17-byte response: first frame, flow control, two CFs) */
    stub.n_tx = 0u;
    rx(CAN_BUS2, 0x7E1u, (const uint8_t[]){ 0x03, 0x22, 0xF1, 0x95 }, 4u);
    ticks(1u);
    CHECK(stub.n_tx == 1u && stub.tx_bus[0] == CAN_BUS2);
    CHECK(last_is(0x7E9u, (const uint8_t[]){ 0x10, 0x11, 0x62, 0xF1, 0x95, '8', 'H', 'P' }, 8u));
    rx(CAN_BUS2, 0x7E1u, (const uint8_t[]){ 0x30, 0x00, 0x00 }, 3u);
    ticks(5u);
    CHECK(stub.n_tx == 3u);
    CHECK(memcmp(stub.tx[1].data, (const uint8_t[]){ 0x21, 'T', 'C', 'U', '-', 'O', 'P', 'E' }, 8u) == 0);
    CHECK(memcmp(stub.tx[2].data, (const uint8_t[]){ 0x22, 'N', ' ', 'M', '1', 0xAA, 0xAA, 0xAA }, 8u) == 0);

    /* first frame announcing fewer than 8 bytes is invalid and ignored */
    stub.n_tx = 0u;
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x10, 0x07, 0x22, 0xFD, 0x00, 0xF1, 0x95, 0xFD }, 8u);
    CHECK(stub.n_tx == 0u);

    /* multi-frame request: 22 FD00 F195 FD00 F195 (9 bytes) -> FC from us, then 41-byte response */
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x10, 0x09, 0x22, 0xFD, 0x00, 0xF1, 0x95, 0xFD }, 8u);
    CHECK(last_is(0x7E9u, (const uint8_t[]){ 0x30, 0x00, 0x00 }, 3u));
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x21, 0x00, 0xF1, 0x95 }, 4u);
    ticks(1u);
    CHECK(last_is(0x7E9u, (const uint8_t[]){ 0x10, 0x29, 0x62, 0xFD, 0x00, 0x12, 0x34, 0xF1 }, 8u));

    /* flow control with block size 2 and STmin 3 ms: 5 CFs, pausing after every 2 */
    stub.n_tx = 0u;
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x30, 0x02, 0x03 }, 3u);
    ticks(10u);
    CHECK(stub.n_tx == 2u && stub.tx[1].data[0] == 0x22u);
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x30, 0x00, 0x00 }, 3u);
    ticks(10u);
    CHECK(stub.n_tx == 5u && stub.tx[4].data[0] == 0x25u);

    /* unknown DID */
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x03, 0x22, 0x12, 0x34 }, 4u);
    ticks(1u);
    CHECK(last_is(0x7E9u, (const uint8_t[]){ 0x03, 0x7F, 0x22, 0x31 }, 4u));

    /* programming session refused while leaving is not allowed */
    s_allowed = false;
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x02, 0x10, 0x02 }, 3u);
    ticks(1u);
    CHECK(last_is(0x7E9u, (const uint8_t[]){ 0x03, 0x7F, 0x10, 0x22 }, 4u));
    CHECK(stub.handovers == 0u);

    /* programming session: response pending, then hand-over with the stock record */
    s_allowed = true;
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x02, 0x10, 0x02 }, 3u);
    ticks(1u);
    CHECK(last_is(0x7E9u, (const uint8_t[]){ 0x03, 0x7F, 0x10, 0x78 }, 4u));
    ticks(10u);
    CHECK(stub.handovers == 0u);
    ticks(20u);
    CHECK(stub.handovers == 1u);
    CHECK(stub.rec[0] == 0xA5u && stub.rec[1] == 0x5Au && stub.rec[2] == 0x00u && stub.rec[3] == 0x48u);
    CHECK(stub.rec[4] == 3u && stub.rec[5] == 2u && stub.rec[0x24] == 'w');
    CHECK(stub.rec[0x35] == 0u && stub.rec[0x36] == 2u && stub.rec[0x37] == 0x10u && stub.rec[0x38] == 0x02u);
    CHECK(stub.rec[0x44] == 0x5Au && stub.rec[0x45] == 0xA5u);

    /* ECU reset: positive response, then reset */
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x02, 0x11, 0x01 }, 3u);
    ticks(1u);
    CHECK(last_is(0x7E9u, (const uint8_t[]){ 0x02, 0x51, 0x01 }, 3u));
    ticks(25u);
    CHECK(stub.resets == 1u);

    /* S3: extended session falls back to default after 5 s without requests */
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x02, 0x10, 0x03 }, 3u);
    ticks(4999u);
    CHECK(uds_session() == 0x03u);
    ticks(2u);
    CHECK(uds_session() == 0x01u);

    /* wrong length */
    rx(CAN_BUS1, 0x7E1u, (const uint8_t[]){ 0x03, 0x10, 0x03, 0x00 }, 4u);
    ticks(1u);
    CHECK(last_is(0x7E9u, (const uint8_t[]){ 0x03, 0x7F, 0x10, 0x13 }, 4u));

    if (fails == 0) {
        printf("test_uds: ok\n");
    }
    return fails != 0;
}
