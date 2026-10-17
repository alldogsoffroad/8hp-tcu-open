/* Unit test: bit packing, transmit layer (counter, CRC, periods) and the FCA transmit table. */
#include "bsw/com/crc8.h"
#include "bsw/com/sigpack.h"
#include "bsw/com/com_tx.h"
#include "io/vehicle/fca_tx.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

#define SET_ALL_ONES(name, msg, byte, msb, len) fca_tx_set(FCA_##name, 0xFFFFu);
#define MSG_INFO(name, id, dlc, period, crc, ctr) { id, dlc, period, crc, ctr },
static const struct { uint16_t id; uint8_t dlc; uint16_t period; bool crc, ctr; } msgs[] = {
    FCA_TX_MESSAGES(MSG_INFO)
};

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
    uint8_t d[8] = { 0 };
    can_frame_t f;

    /* sigpack: 13-bit field at byte 0 bits 4..0 + byte 1 (the torque layout of spec 07) */
    sig_put(d, (sig_t){ 0u, 4u, 13u }, 0x1ABCu);
    CHECK(d[0] == 0x1Au && d[1] == 0xBCu);
    sig_put(d, (sig_t){ 0u, 7u, 3u }, 0x5u);               /* bits 7..5 of byte 0 */
    CHECK(d[0] == 0xBAu && d[1] == 0xBCu);
    CHECK(sig_get(d, (sig_t){ 0u, 4u, 13u }) == 0x1ABCu);
    CHECK(sig_get(d, (sig_t){ 0u, 7u, 3u }) == 0x5u);
    sig_put(d, (sig_t){ 2u, 2u, 11u }, 0x7FFu);            /* 11-bit: byte 2 bits 2..0 + byte 3 */
    CHECK(d[2] == 0x07u && d[3] == 0xFFu);
    sig_put(d, (sig_t){ 2u, 2u, 11u }, 0x123u);
    CHECK(d[2] == 0x01u && d[3] == 0x23u && sig_get(d, (sig_t){ 2u, 2u, 11u }) == 0x123u);

    fca_tx_init();

    /* every field at its maximum must leave the counter nibble and the CRC byte untouched */
    FCA_TX_FIELDS(SET_ALL_ONES)
    for (uint8_t m = 0u; m < FCA_TX_MSG_COUNT; m++) {
        com_tx_build(m, &f);
        CHECK(f.id == msgs[m].id && f.dlc == msgs[m].dlc);
        if (msgs[m].ctr) {
            CHECK((f.data[f.dlc - 2u] >> 4) == 0u);              /* counter 0, not field bits */
        }
        if (msgs[m].crc) {
            CHECK(f.data[f.dlc - 1u] == crc8_j1850(f.data, (uint8_t)(f.dlc - 1u)));
        }
    }
    fca_tx_init();

    /* field values are masked to their length */
    fca_tx_set(FCA_F_3D1_B0_7, 0x1234u);
    CHECK(fca_tx_get(FCA_F_3D1_B0_7) == 0x34u);

    /* nothing is sent while disabled */
    for (int i = 0; i < 1000; i++) {
        fca_tx_tick_1ms();
    }
    CHECK(stub.n_tx == 0u);

    /* periods (bench measurement): 0x037 every 16 ms, 0x144 every 31 ms, 0x3D1 every 781 ms;
     * over 800 ms: 0x037 at 0, 16, ... 784 = 50, 0x144 at 0, 31, ... 775 = 26, 0x3D1 at 0, 781 = 2 */
    fca_tx_enable(FCA_M_037, true);
    fca_tx_enable(FCA_M_144, true);
    fca_tx_enable(FCA_M_3D1, true);
    {
        unsigned n037 = 0u, n144 = 0u, n3d1 = 0u;
        for (int i = 0; i < 800; i++) {
            stub.n_tx = 0u;
            fca_tx_tick_1ms();
            n037 += count_id(0x037u);
            n144 += count_id(0x144u);
            n3d1 += count_id(0x3D1u);
            for (unsigned k = 0u; k < stub.n_tx; k++) {
                if (stub.tx[k].id == 0x3D1u) {
                    CHECK(stub.tx[k].dlc == 4u && stub.tx[k].data[0] == 0x34u);
                }
            }
        }
        CHECK(n037 == 50u);
        CHECK(n144 == 26u);
        CHECK(n3d1 == 2u);
    }

    /* rolling counter: +1 per transmission, wraps after 15; CRC valid each time */
    stub.n_tx = 0u;
    fca_tx_enable(FCA_M_144, false);
    fca_tx_enable(FCA_M_3D1, false);
    for (int i = 0; i < 320; i++) {          /* 20 frames of 0x037 at 16 ms */
        fca_tx_tick_1ms();
    }
    for (unsigned i = 1u; i < stub.n_tx; i++) {
        const uint8_t a = (uint8_t)(stub.tx[i - 1u].data[6] >> 4), b = (uint8_t)(stub.tx[i].data[6] >> 4);
        CHECK(b == ((a + 1u) & 0x0Fu));
        CHECK(stub.tx[i].data[7] == crc8_j1850(stub.tx[i].data, 7u));
    }

    /* controller busy: counted, retried next tick without advancing the counter */
    for (int i = 0; i < 16 && com_tx_state(FCA_M_037)->busy == 0u; i++) {
        stub.n_tx = STUB_TX_MAX;
        fca_tx_tick_1ms();
    }
    CHECK(com_tx_state(FCA_M_037)->busy == 1u);
    stub.n_tx = 0u;
    fca_tx_tick_1ms();
    CHECK(stub.n_tx == 1u);

    if (fails == 0) {
        printf("test_com_tx: ok\n");
    }
    return fails != 0;
}
