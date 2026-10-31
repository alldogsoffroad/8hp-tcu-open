/* Unit test: solenoid driver chip jobs (frames, setpoints, reply checks, fault handling). */
#include "io/actuators/sol_drv.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void)
{
    const sol_status_t *s = sol_status();

    /* scaling: 0.1 mA / 6, 11 bits */
    CHECK(sol_setpoint(0u) == 0u);
    CHECK(sol_setpoint(6000u) == 1000u);         /* 600 mA */
    CHECK(sol_setpoint(12282u) == 2047u);
    CHECK(sol_setpoint(20000u) == 2047u);

    /* start-up: ID + configuration with the stock bytes */
    sol_init();
    CHECK(s->ready && s->faults == 0u);
    CHECK(s->chip[0].id[0] == 0x12u);
    CHECK(stub.drv_reg[0][0x90] == 0x21u && stub.drv_reg[4][0x96] == 0x55u && stub.drv_reg[2][0x8E] == 0x00u);
    CHECK(stub.drv_reg[3][0x88] == (0x10u | 0x01u | 0x04u));      /* both channels off */

    /* idle cycle: all channels off, setpoints 0 */
    sol_tick_10ms();
    CHECK(s->faults == 0u);
    for (unsigned c = 0u; c < 5u; c++) {
        CHECK(stub.drv_last_tx[c] == (0x8800u | 0x15u));
        CHECK(stub.drv_last_tx[10u + c] == 0xE800u && stub.drv_last_tx[20u + c] == 0xF000u);
    }

    /* channel 3 (chip 1, channel 1 = EDS E) at 500 mA: mode bits 3..2 = 2, setpoint 833 */
    sol_set(3u, SOL_MODE_CURRENT, 5000u);
    sol_tick_10ms();
    CHECK(s->faults == 0u && s->mode[3] == SOL_MODE_CURRENT && s->current[3] == 5000u);
    CHECK(stub.drv_last_tx[1] == (0x8800u | 0x10u | 0x01u | 0x08u));
    CHECK(stub.drv_last_tx[21] == (0xF000u | 833u));
    CHECK(stub.drv_last_tx[10] == 0xE800u && stub.drv_last_tx[0] == (0x8800u | 0x15u));
    /* the cyclic frames carry the right commands */
    CHECK((stub.drv_last_tx[5] >> 8) == 0xA0u && (stub.drv_last_tx[15] >> 8) == 0xA6u);
    CHECK((stub.drv_last_tx[25] >> 8) == 0x60u && (stub.drv_last_tx[44] >> 8) == 0xD8u);

    /* status job every 10th cycle */
    stub.drv_jobs = 0u;
    for (int i = 0; i < 10; i++) {
        sol_tick_10ms();
    }
    CHECK(stub.drv_jobs == 11u);

    sol_all_off();
    sol_tick_10ms();
    CHECK(s->mode[3] == SOL_MODE_OFF && stub.drv_last_tx[21] == 0xF000u);

    /* read-back mismatch: counted in status, not a latched fault (the reply format is open
     * on hardware, bench 2026-10-30); the channel keeps running */
    sol_set(3u, SOL_MODE_CURRENT, 5000u);
    sol_tick_10ms();
    {
        const uint16_t before = s->cfg_mismatch;
        stub.drv_bad_readback = true;
        sol_tick_10ms();
        stub.drv_bad_readback = false;
        CHECK(s->cfg_mismatch > before && s->faults == 0u);
        CHECK(s->mode[3] == SOL_MODE_CURRENT);
    }
    sol_all_off();
    sol_tick_10ms();
    CHECK(s->mode[3] == SOL_MODE_OFF && stub.drv_last_tx[21] == 0xF000u);

    /* header fault, bus fault, and start-up failure */
    sol_init();
    stub.drv_bad_header = true;
    sol_tick_10ms();
    CHECK((s->faults & SOL_F_HEADER) != 0u);
    stub.drv_bad_header = false;
    stub.drv_fail = true;
    sol_init();
    CHECK(!s->ready && (s->faults & (SOL_F_BUS | SOL_F_NOT_READY)) == (SOL_F_BUS | SOL_F_NOT_READY));
    stub.drv_fail = false;
    sol_set(0u, SOL_MODE_CURRENT, 5000u);
    sol_tick_10ms();
    CHECK(s->mode[0] == SOL_MODE_OFF);

    if (fails == 0) {
        printf("test_sol: ok\n");
    }
    return fails != 0;
}
