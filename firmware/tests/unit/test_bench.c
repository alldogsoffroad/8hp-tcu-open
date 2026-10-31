/* Unit test: bench solenoid test interlocks (safety design 2026-10-15). */
#include "app/bench_out.h"
#include "bsw/diag/uds.h"
#include "bsw/sup/sup.h"
#include "bsw/com/crc8.h"
#include "io/actuators/sol_drv.h"
#include "io/vehicle/vehicle.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static const uds_app_t app = { NULL, NULL, bench_routine };

static void uds_req(const uint8_t *d, uint8_t n)
{
    can_frame_t f = { .id = 0x7E1u, .dlc = 8u };
    memset(f.data, 0, 8u);
    f.data[0] = n;
    memcpy(&f.data[1], d, n);
    (void)uds_rx(CAN_BUS1, &f);
    uds_tick_1ms();
}

static const uint8_t *last_rsp(void)
{
    return stub.n_tx != 0u ? stub.tx[stub.n_tx - 1u].data : (const uint8_t *)"";
}

static void run_10ms(unsigned n)
{
    while (n-- != 0u) {
        for (int i = 0; i < 10; i++) {
            uds_tick_1ms();
        }
        /* same order as the scheduler: 10 ms task at offset 1, 5 ms task at offsets 2 and 7 */
        vehicle_update_10ms();
        bench_tick_10ms();
        sol_tick_10ms();
        sup_tick_5ms();
        sup_tick_5ms();
        stub.n_tx = 0u;
    }
}

static void engine_rpm(uint16_t rpm)
{
    static uint8_t ctr;
    can_frame_t f = { .id = 0x108u, .dlc = 8u };
    memset(f.data, 0, 8u);
    f.data[0] = (uint8_t)(rpm >> 8);
    f.data[1] = (uint8_t)rpm;
    ctr = (uint8_t)((ctr + 1u) & 0x0Fu);
    f.data[6] = (uint8_t)(ctr << 4);
    f.data[7] = crc8_j1850(f.data, 7u);
    vehicle_rx(CAN_BUS1, &f);
}

static bool start(uint8_t ch, uint16_t ma, uint8_t s)
{
    const uint8_t r[] = { 0x31, 0x01, 0xFE, 0x10, ch, (uint8_t)(ma / 10u), s };
    stub.n_tx = 0u;
    uds_req(r, sizeof r);
    return last_rsp()[1] == 0x71u;
}

static bool start_park(uint8_t out, uint8_t s, uint8_t duty_pct)
{
    const uint8_t r[7] = { 0x31, 0x01, 0xFE, 0x11, out, s, duty_pct };
    const uint8_t n = duty_pct != 0u ? 7u : 6u;
    stub.n_tx = 0u;
    uds_req(r, n);
    return last_rsp()[1] == 0x71u;
}

static uint8_t last_nrc(void) { return last_rsp()[1] == 0x7Fu ? last_rsp()[3] : 0u; }

int main(void)
{
    const sol_status_t *sol = sol_status();
    const sup_status_t *sup = sup_status();

    vehicle_init();
    uds_init(&app);
    sup_init();
    sol_init();
    bench_init();
    stub.sup_reply = 0xA800u;
    run_10ms(2u);
    CHECK(!sup->enable && sol->mode[0] == SOL_MODE_OFF);

    /* not in the extended session: refused */
    CHECK(!start(0u, 500u, 2u));
    CHECK(last_nrc() == 0x7Fu);

    uds_req((const uint8_t[]){ 0x10, 0x03 }, 2u);

    /* limits */
    CHECK(!start(7u, 500u, 2u) && last_nrc() == 0x31u);     /* park release excluded */
    CHECK(!start(0u, 1010u, 2u) && last_nrc() == 0x31u);    /* current cap */
    CHECK(!start(0u, 500u, 6u) && last_nrc() == 0x31u);     /* duration cap */
    CHECK(!start(0u, 0u, 2u) && last_nrc() == 0x31u);

    /* a valid test: channel 2 at 400 mA for 1 s */
    CHECK(start(2u, 400u, 1u));
    run_10ms(1u);
    CHECK(bench_active());
    CHECK(sol->mode[2] == SOL_MODE_CURRENT && sol->current[2] == 4000u);
    for (unsigned c = 0u; c < SOL_CHANNELS; c++) {
        CHECK(c == 2u || sol->mode[c] == SOL_MODE_OFF);
    }
    /* second test while active: refused (one channel at a time) */
    CHECK(!start(3u, 400u, 1u) && last_nrc() == 0x22u);

    /* timeout after 1 s */
    run_10ms(100u);
    CHECK(!bench_active() && sol->mode[2] == SOL_MODE_OFF);
    {
        uint8_t res[7];
        CHECK(bench_routine(3u, BENCH_RID, NULL, 0u, res, 7u) == 7 && res[5] == BENCH_END_TIMEOUT);
    }

    /* stop command */
    CHECK(start(0u, 500u, 5u));
    run_10ms(5u);
    uds_req((const uint8_t[]){ 0x31, 0x02, 0xFE, 0x10 }, 4u);
    run_10ms(1u);
    CHECK(!bench_active() && sol->mode[0] == SOL_MODE_OFF && !sup->enable);

    /* engine starts during a test */
    CHECK(start(0u, 500u, 5u));
    run_10ms(2u);
    engine_rpm(800u);
    run_10ms(2u);
    CHECK(!bench_active() && sol->mode[0] == SOL_MODE_OFF);
    CHECK(!start(0u, 500u, 5u) && last_nrc() == 0x22u);     /* engine running: refused */
    engine_rpm(0u);
    run_10ms(1u);

    /* supervisor stops replying */
    CHECK(start(1u, 300u, 5u));
    run_10ms(2u);
    stub.sup_reply = 0x0000u;
    run_10ms(2u);
    CHECK(!bench_active() && sol->mode[1] == SOL_MODE_OFF && !sup->enable);
    stub.sup_reply = 0xA800u;
    run_10ms(2u);

    /* driver header fault (a read-back mismatch alone is no longer a fault: the reply
     * format is open on hardware, bench 2026-10-30 — counted in sol status instead) */
    CHECK(start(1u, 300u, 5u));
    run_10ms(2u);
    stub.drv_bad_header = true;
    run_10ms(2u);
    CHECK(!bench_active() && sol->mode[1] == SOL_MODE_OFF);
    stub.drv_bad_header = false;
    CHECK(!start(1u, 300u, 5u) && last_nrc() == 0x22u);     /* latched until re-init */
    sol_init();
    run_10ms(1u);

    /* ---- FE11: park output commissioning (M4b) ---- */
    uds_req((const uint8_t[]){ 0x10, 0x03 }, 2u);
    run_10ms(2u);

    /* limits: output selector, duration, duty */
    CHECK(!start_park(2u, 2u, 0u) && last_nrc() == 0x31u);
    CHECK(!start_park(0u, 6u, 0u) && last_nrc() == 0x31u);
    CHECK(!start_park(0u, 2u, 101u) && last_nrc() == 0x31u);

    /* park release (out 0): channel 7 full on, TOE31 off, enable requested */
    CHECK(start_park(0u, 2u, 0u));
    run_10ms(1u);
    CHECK(bench_active() && !sup->enable);   /* no enable request: the bit resets the TCU */
    CHECK(sol->mode[7] == SOL_MODE_FULL_ON && stub.toe_duty == 0u);
    for (unsigned c = 0u; c < SOL_CH_PARK_RELEASE; c++) {
        CHECK(sol->mode[c] == SOL_MODE_OFF);
    }
    run_10ms(210u);                     /* 2 s timeout */
    CHECK(!bench_active() && !sup->enable && sol->mode[7] == SOL_MODE_OFF && stub.toe_duty == 0u);
    {
        uint8_t res[7];
        CHECK(bench_routine(3u, BENCH_PARK_RID, NULL, 0u, res, 7u) == 7 && res[5] == BENCH_END_TIMEOUT);
    }

    /* park hold (out 1): TOE31 duty 1000 default, channel 7 off */
    CHECK(start_park(1u, 1u, 0u));
    run_10ms(1u);
    CHECK(sol->mode[7] == SOL_MODE_OFF && stub.toe_duty == 1000u);
    /* explicit duty honoured */
    uds_req((const uint8_t[]){ 0x31, 0x02, 0xFE, 0x11 }, 4u);
    run_10ms(1u);
    CHECK(!bench_active() && stub.toe_duty == 0u);
    CHECK(start_park(1u, 1u, 50u));     /* 50 % duty */
    run_10ms(1u);
    CHECK(stub.toe_duty == 500u);

    /* one test at a time across the two routines */
    CHECK(!start(0u, 300u, 1u) && last_nrc() == 0x22u);
    /* engine running stops a park test and clears both outputs */
    engine_rpm(800u);
    run_10ms(2u);
    CHECK(!bench_active() && sol->mode[7] == SOL_MODE_OFF && stub.toe_duty == 0u);
    engine_rpm(0u);
    run_10ms(2u);

    /* session ends (S3 timeout without tester present) */
    CHECK(start(4u, 300u, 5u));
    run_10ms(501u);
    CHECK(!bench_active() && sol->mode[4] == SOL_MODE_OFF && uds_session() == 0x01u);

    /* ---- FE13 supervisor lab: one-shot RSPI-B exchange and the stock app pattern ---- */
    uds_req((const uint8_t[]){ 0x10, 0x03 }, 2u);
    {
        uint8_t out[64];
        int r;

        uds_req((const uint8_t[]){ 0x31, 0x03, 0xFE, 0x13 }, 4u);
        CHECK(memcmp(&last_rsp()[1], "\x71\x03\xFE\x13\x00", 5) == 0);   /* idle */
        /* malformed one-shots: odd byte count, unknown sub */
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x01, 0x06 }, 6u);
        CHECK(last_nrc() == 0x13u);
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x09 }, 5u);
        CHECK(last_nrc() == 0x12u);
        /* one-shot 0x0600 (even index -> SSLB1), scripted reply 0xA8BD */
        stub.sup_xfer_rx = 0xA8BDu;
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x01, 0x06, 0x00 }, 7u);
        CHECK(memcmp(&last_rsp()[1], "\x71\x01\xFE\x13", 4) == 0);
        r = bench_routine(3u, BENCH_SUP_RID, NULL, 0u, out, 64u);
        CHECK(r == 4 && out[0] == 1u && out[1] == 1u && out[2] == 0xA8u && out[3] == 0xBDu);
        /* bus timeout -> conditions not correct */
        stub.sup_xfer_fail = true;
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x01, 0x28, 0x00 }, 7u);
        CHECK(last_nrc() == 0x22u);
        stub.sup_xfer_fail = false;
        /* stock app pattern on: the 5 ms tick becomes the three cyclic jobs.
         * Entering, last_rx = 0xA800 (bit 2 clear) so pass 1 sends 0x8E04 and the
         * scripted reply 0xA804 matches the echo (mask 0xDF); pass 2 sends 0x8E00
         * and the echo check then counts bad - the stub script is not word-aware. */
        stub.sup_xfer_rx = 0xA804u;
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x02, 0x00, 0x21 }, 7u);
        CHECK(memcmp(&last_rsp()[1], "\x71\x01\xFE\x13", 4) == 0);
        CHECK(sup->app_pattern);
        run_10ms(1u);
        CHECK(sup->rx_words[2] == 0xA804u && sup->rx_words[0] == 0xA804u);
        CHECK(sup->echo_ok + sup->echo_bad >= 2u && sup->aa_bad >= 8u);
        r = bench_routine(3u, BENCH_SUP_RID, NULL, 0u, out, 64u);
        CHECK(r == 27 && out[0] == 2u && out[1] == 0xA8u && out[2] == 0x04u);
        /* stop restores the single 5 ms word */
        uds_req((const uint8_t[]){ 0x31, 0x02, 0xFE, 0x13 }, 4u);
        CHECK(!sup->app_pattern);
        r = bench_routine(3u, BENCH_SUP_RID, NULL, 0u, out, 64u);
        CHECK(r == 1 && out[0] == 0u);
        /* sub 3 (pattern off via start) also accepted */
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x03 }, 5u);
        CHECK(memcmp(&last_rsp()[1], "\x71\x01\xFE\x13", 4) == 0);
    }

    if (fails == 0) {
        printf("test_bench: ok\n");
    }
    return fails != 0;
}
