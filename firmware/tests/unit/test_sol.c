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
    sol_stream_gate(true);      /* E29: tests exercise the legacy always-on stream */
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

    /* ---- E7 (spec 03 section 6.2e): engaged re-configuration with the level law ---- */
    sol_engage_config(0x1Fu);               /* HOLD level -> the stock's live 0x9F row */
    sol_tick_10ms();                        /* pending config runs before the cyclic job */
    CHECK(s->faults == 0u);
    CHECK(stub.drv_reg[0][0x8E] == 0x9Fu && stub.drv_reg[2][0x8E] == 0x9Fu &&
          stub.drv_reg[3][0x8E] == 0x9Fu);  /* 0x80 | 0x1F on every chip */
    CHECK(stub.drv_reg[1][0x90] == 0x21u && stub.drv_reg[4][0x96] == 0x55u);
    sol_engage_config(3u);                  /* mid-ramp level mirrors too */
    sol_tick_10ms();
    CHECK(stub.drv_reg[2][0x8E] == 0x83u);

    /* ---- E7: the 40-word duty job (slot-2 = SSLA1 gets 0xD0 | duty[r]) ---- */
    {
        static const uint16_t D[8] = { 0x90u, 0x10u, 0x20u, 0x40u, 0x40u, 0x60u, 0x00u, 0x00u };

        CHECK(sol_duty_job(D));
        CHECK(s->duty_runs == 1u && s->faults == 0u);
        CHECK(stub.drv_last_tx[2] == 0xD090u && stub.drv_last_tx[7] == 0xD010u);
        CHECK(stub.drv_last_tx[12] == 0xD020u && stub.drv_last_tx[17] == 0xD040u);
        CHECK(stub.drv_last_tx[22] == 0xD040u && stub.drv_last_tx[27] == 0xD060u);
        CHECK(stub.drv_last_tx[32] == 0xD000u && stub.drv_last_tx[37] == 0xD000u);
        CHECK(stub.drv_last_tx[1] == 0x80B0u && stub.drv_last_tx[4] == 0xE0D0u);  /* fixed rows */
        CHECK(stub.drv_last_tx[3] == 0x30A0u && stub.drv_last_tx[8] == 0xF0A0u);  /* alternating */
        CHECK(stub.drv_last_tx[0] == 0xC070u && stub.drv_last_tx[5] == 0x7050u);  /* slot-0 row */
        CHECK((s->duty_rx[0] & 0xFF00u) == 0xA200u);      /* slot-2 reply class */
    }

    /* header fault, bus fault, and start-up failure */
    sol_stream_gate(true);      /* E29: tests exercise the legacy always-on stream */
    sol_init();
    stub.drv_bad_header = true;
    sol_tick_10ms();
    CHECK((s->faults & SOL_F_HEADER) != 0u);
    stub.drv_bad_header = false;
    stub.drv_fail = true;
    sol_stream_gate(true);      /* E29: tests exercise the legacy always-on stream */
    sol_init();
    CHECK(!s->ready && (s->faults & (SOL_F_BUS | SOL_F_NOT_READY)) == (SOL_F_BUS | SOL_F_NOT_READY));
    stub.drv_fail = false;
    sol_set(0u, SOL_MODE_CURRENT, 5000u);
    sol_tick_10ms();
    CHECK(s->mode[0] == SOL_MODE_OFF);
    {   /* not ready: the duty job refuses (E7 safety gate); sol_init() above reset
         * the status block, so duty_runs is back to zero */
        static const uint16_t D[8] = { 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };

        CHECK(!sol_duty_job(D) && s->duty_runs == 0u);
    }

    if (fails == 0) {
        printf("test_sol: ok\n");
    }
    return fails != 0;
}
