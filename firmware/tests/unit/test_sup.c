/* Unit test: external supervisor service (toggle, output enable, reply check). */
#include "bsw/sup/sup.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void)
{
    const sup_status_t *s = sup_status();

    sup_init();
    stub.sup_reply = 0xA804u;
    for (int i = 0; i < 6; i++) {
        sup_tick_5ms();
    }
    /* command 0x8E, bit 2 alternating, output enable off */
    CHECK(stub.sup_n == 6u);
    for (unsigned i = 0u; i < stub.sup_n; i++) {
        CHECK((stub.sup_tx[i] & 0xFF00u) == 0x8E00u);
        CHECK((stub.sup_tx[i] & 0x01u) == 0u);
        if (i > 0u) {
            CHECK(((stub.sup_tx[i] ^ stub.sup_tx[i - 1u]) & 0x04u) != 0u);
        }
    }
    CHECK(s->replies_ok == 5u && s->replies_bad == 0u && !s->enable);

    /* busy transmitter: no frame, toggle phase kept */
    stub.sup_busy = true;
    sup_tick_5ms();
    stub.sup_busy = false;
    sup_tick_5ms();
    CHECK(stub.sup_n == 7u && s->busy == 1u);
    CHECK(((stub.sup_tx[6] ^ stub.sup_tx[5]) & 0x04u) != 0u);

    /* output enable request */
    sup_set_output_enable(true);
    sup_tick_5ms();
    CHECK((stub.sup_tx[7] & 0x01u) == 1u && s->enable);
    sup_set_output_enable(false);
    sup_tick_5ms();
    CHECK((stub.sup_tx[8] & 0x01u) == 0u && !s->enable);

    /* wrong reply header counted */
    stub.sup_reply = 0x0000u;
    sup_tick_5ms();
    CHECK(s->replies_bad == 1u);

    /* flash programming mode: 0x8E80 with bit 4 toggling, no enable, no bit 2 */
    sup_set_output_enable(true);
    sup_set_flash_mode(true);
    stub.sup_reply = 0xA804u;
    sup_tick_5ms();
    sup_tick_5ms();
    CHECK((stub.sup_tx[10] & 0xFF85u) == 0x8E80u && (stub.sup_tx[11] & 0xFF85u) == 0x8E80u);
    CHECK(((stub.sup_tx[10] ^ stub.sup_tx[11]) & 0x10u) != 0u && !s->enable);
    sup_set_flash_mode(false);
    sup_set_output_enable(false);
    sup_tick_5ms();
    CHECK((stub.sup_tx[12] & 0xFFF1u) == 0x8E00u);

    /* bench pattern: word ^ toggle on alternate frames, enable bit always cleared */
    sup_set_pattern(true, 0x8401u, 0x0001u);
    sup_tick_5ms();
    sup_tick_5ms();
    CHECK(stub.sup_tx[13] == 0x8401u && stub.sup_tx[14] == 0x8400u);
    sup_set_pattern(true, 0x8E81u, 0x0011u);
    sup_tick_5ms();
    sup_tick_5ms();
    CHECK((stub.sup_tx[15] | stub.sup_tx[16]) == 0x8E90u && (stub.sup_tx[15] & stub.sup_tx[16]) == 0x8E80u);
    sup_set_pattern(false, 0u, 0u);
    sup_tick_5ms();
    CHECK((stub.sup_tx[17] & 0xFFF1u) == 0x8E00u);

    if (fails == 0) {
        printf("test_sup: ok\n");
    }
    return fails != 0;
}
