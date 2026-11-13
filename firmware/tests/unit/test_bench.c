/* Unit test: bench solenoid test interlocks (safety design 2026-10-15),
 * carried to the rebuild's job-engine interface (design §7/§8/§11). */
#include "app/bench_out.h"
#include "bsw/diag/uds.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_batch.h"
#include "bsw/sup/sup_walk.h"
#include "bsw/sup/sup_letters.h"
#include "bsw/sup/sup_ladder.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/com/crc8.h"
#include "io/actuators/sol_ctl.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_cells.h"
#include "io/vehicle/vehicle.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static const uds_app_t app = { NULL, NULL, bench_routine };

/* scripted supervisor: every word class-checked; the 0x22 read mirrors the
 * staged ctrl byte (the healthy-supervisor law), everything else reads 0 */
static uint16_t sup_mirror(uint16_t tx)
{
    return (tx & 0xFF00u) == 0x2200u ? CELL_CTRL_STAGED : 0x0000u;
}

/* scripted supervisor gone wrong: the echo no longer matches */
static uint16_t sup_garbage(uint16_t tx)
{
    (void)tx;
    return 0x7Fu;
}

static void drain_rspi(void)
{
    for (uint8_t b = 0u; b < 2u; b++) {
        unsigned guard = 100u;
        while (stub_rspi_busy(b) && guard-- != 0u) {
            stub_rspi_isr(b);
        }
    }
}

static void sup_pass(void)
{
    sup_batch_5ms();
    sup_monitor_poll();     /* the real scheduler runs BOTH 5ms tasks — the
                             * silence window's counter lives in the monitor */
    drain_rspi();
}

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
            stub.ms++;
            stub.frt_us += 1000u;
        }
        /* same order as the scheduler: 10 ms task at offset 1, 5 ms tasks at offsets 2 and 7 */
        vehicle_update_10ms();
        bench_tick_10ms();
        sol_tick_10ms();
        drain_rspi();
        sup_pass();
        sup_pass();
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
    vehicle_init();
    uds_init(&app);
    sup_jobs_init();
    sup_batch_init();           /* serial_rx_init: request 0, outputs disabled, J5 queued */
    drv_jobs_init();
    sol_init();
    bench_init();
    stub.sup_reply_fn = sup_mirror;
    drain_rspi();               /* launch J5, harvest the ID replies */
    run_10ms(12u);              /* through the ~95ms silence window (20 passes)
                                 * — the supervisor conversation, and thus the
                                 * bench's health gate, starts at the release */
    CHECK(sol_channel(0u)->mode == SOL_MODE_OFF);
    /* (the bench routines never arm the request cell; the monitor's one-shot
     * legitimately can once the conversation matures — not asserted here) */

    /* not in the extended session: refused */
    CHECK(!start(0u, 500u, 2u));
    CHECK(last_nrc() == 0x7Fu);

    uds_req((const uint8_t[]){ 0x10, 0x03 }, 2u);

    /* limits */
    CHECK(!start(7u, 500u, 2u) && last_nrc() == 0x31u);     /* park release excluded */
    CHECK(!start(0u, 1010u, 2u) && last_nrc() == 0x31u);    /* current cap */
    CHECK(!start(0u, 500u, 6u) && last_nrc() == 0x31u);     /* duration cap */
    CHECK(!start(0u, 0u, 2u) && last_nrc() == 0x31u);

    /* a valid test: channel 2 at 400 mA for 1 s (setpoint = 4000 * 0.1 mA / 6) */
    CHECK(start(2u, 400u, 1u));
    run_10ms(1u);
    CHECK(bench_active());
    CHECK(sol_channel(2u)->mode == SOL_MODE_CURRENT);
    CHECK(sol_channel(2u)->setpoint == 4000u / 6u);
    for (uint8_t c = 0u; c <= BENCH_MAX_CHANNEL; c++) {
        CHECK(c == 2u || sol_channel(c)->mode == SOL_MODE_OFF);
    }
    /* r02v stage 0a: whatever sol_set asked, the bus-A words stay the stock's
     * pre-engagement content — command-major, mode 0x15, setpoints 0 */
    {
        static const uint16_t CMD[9] = { 0x8815u, 0xA000u, 0xE800u, 0xA600u, 0xF000u,
                                         0x6000u, 0xC000u, 0x6600u, 0xD800u };
        const uint16_t *cy = drv_job_tx(DRV_CYC);
        const uint16_t *cf = drv_job_tx(DRV_CFG);
        for (unsigned i = 0u; i < 45u; i++) {
            CHECK(cy[i] == CMD[i / 5u]);
        }
        /* r03b: FE10 is not a request source of the request law — the
         * law's input 0xFFF94EE8 and its outputs stay 0 */
        for (unsigned ch = 0u; ch < DRV_RQ_NCH; ch++) {
            CHECK(CELL_RQ_REQ[ch] == 0u && CELL_RQ_CUR[ch] == 0u && CELL_RQ_I[ch] == 0u);
        }
        for (unsigned i = 0u; i < 5u; i++) {
            CHECK(cf[i] == 0x8E9Fu && cf[5u + i] == 0x9021u && cf[10u + i] == 0x9655u &&
                  cf[15u + i] == 0x8815u && cf[20u + i] == 0x2800u && cf[35u + i] == 0x3600u);
        }
    }
    /* r02w: the zero-request letter law (0x83DB0 / 0x89540 / 0x901A0) */
    {
        const uint16_t *cy = drv_job_tx(DRV_CYC);
        const uint8_t s0 = CELL_SEQ2_STATE[0], t14 = CELL_LAD_94114, t15 = CELL_DIAG_94115;
        CELL_SEQ2_STATE[0] = 0x11u; CELL_LAD_94114 = 1u; CELL_DIAG_94115 = 0u;   /* phase 0x10 */
        drv_cyclic_build(0, 8u);
        for (unsigned c = 0u; c < 4u; c++) {
            CHECK(cy[c] == 0x881Fu && cy[10u + c] == 0xEFFFu && cy[20u + c] == 0xF7FFu);
        }
        CHECK(cy[4] == 0x8815u && cy[14] == 0xE800u && cy[24] == 0xF000u);
        {   /* the interlock: no full-on while the rail word or a request is up */
            const uint16_t j2 = CELL_J2_STAGED;
            CELL_J2_STAGED = 0x9020u; drv_cyclic_build(0, 8u);
            CHECK(cy[0] == 0x8815u && cy[10] == 0xE800u);
            CELL_J2_STAGED = 0x9000u; CELL_LAD_REQ[0] = 1u; drv_cyclic_build(0, 8u);
            CHECK(cy[0] == 0x8815u);
            CELL_LAD_REQ[0] = 0u; CELL_J2_STAGED = j2;
        }
        CELL_SEQ2_STATE[0] = 0x22u; drv_cyclic_build(0, 8u);                    /* not a test state */
        CHECK(cy[0] == 0x8810u && cy[4] == 0x8815u);
        CELL_SEQ2_STATE[0] = (uint8_t)'3'; CELL_LAD_94114 = 1u; drv_cyclic_build(0, 8u);
        CHECK(cy[0] == 0x8815u);                                                /* 94114 needs 0x11/0x16 */
        CELL_DIAG_94115 = 1u; drv_cyclic_build(0, 8u);
        CHECK(cy[0] == 0x881Fu && cy[13] == 0xEFFFu);                           /* the '3' diagnosis */
        CELL_SEQ2_STATE[0] = (uint8_t)'U'; CELL_LAD_94114 = 0u; CELL_DIAG_94115 = 0u;
        drv_cyclic_build(0, 8u);
        for (unsigned c = 0u; c < 4u; c++) {
            CHECK(cy[c] == 0x8810u && cy[10u + c] == 0xE800u && cy[20u + c] == 0xF000u);
        }
        CHECK(cy[4] == 0x8815u && drv_job_tx(DRV_CFG)[15] == 0x8810u);
        CELL_SEQ2_STATE[0] = s0; CELL_LAD_94114 = t14; CELL_DIAG_94115 = t15;
        drv_cyclic_build(0, 8u);
    }
    /* second test while active: refused (one channel at a time) */
    CHECK(!start(3u, 400u, 1u) && last_nrc() == 0x22u);

    /* timeout after 1 s */
    run_10ms(100u);
    CHECK(!bench_active() && sol_channel(2u)->mode == SOL_MODE_OFF);
    {
        uint8_t res[7];
        CHECK(bench_routine(3u, BENCH_RID, NULL, 0u, res, 7u) == 7 && res[5] == BENCH_END_TIMEOUT);
    }

    /* stop command */
    CHECK(start(0u, 500u, 5u));
    run_10ms(5u);
    uds_req((const uint8_t[]){ 0x31, 0x02, 0xFE, 0x10 }, 4u);
    run_10ms(1u);
    CHECK(!bench_active() && sol_channel(0u)->mode == SOL_MODE_OFF);

    /* engine starts during a test */
    CHECK(start(0u, 500u, 5u));
    run_10ms(2u);
    engine_rpm(800u);
    run_10ms(2u);
    CHECK(!bench_active() && sol_channel(0u)->mode == SOL_MODE_OFF);
    CHECK(!start(0u, 500u, 5u) && last_nrc() == 0x22u);     /* engine running: refused */
    engine_rpm(0u);
    run_10ms(1u);

    /* supervisor echo stops matching: the test ends (and cannot restart) */
    CHECK(start(1u, 300u, 5u));
    run_10ms(2u);
    stub.sup_reply_fn = sup_garbage;
    run_10ms(2u);
    CHECK(!bench_active() && sol_channel(1u)->mode == SOL_MODE_OFF);
    {
        uint8_t res[7];
        CHECK(bench_routine(3u, BENCH_RID, NULL, 0u, res, 7u) == 7 && res[5] == BENCH_END_SUPERVISOR);
    }
    CHECK(!start(1u, 300u, 5u) && last_nrc() == 0x22u);
    stub.sup_reply_fn = sup_mirror;
    run_10ms(2u);

    /* the driver engine stuck busy (a job armed, never drained) refuses a start */
    sol_tick_10ms();                        /* queue the cyclic and leave it armed */
    CHECK(!start(1u, 300u, 5u) && last_nrc() == 0x22u);     /* DRV_CYC in flight */
    drain_rspi();
    run_10ms(1u);
    CHECK(start(1u, 300u, 5u));                             /* engine alive again */
    run_10ms(1u);
    uds_req((const uint8_t[]){ 0x31, 0x02, 0xFE, 0x10 }, 4u);
    run_10ms(1u);

    /* ---- FE11: park output commissioning (M4b) ---- */
    uds_req((const uint8_t[]){ 0x10, 0x03 }, 2u);
    run_10ms(2u);

    /* limits: output selector, duration, duty */
    CHECK(!start_park(2u, 2u, 0u) && last_nrc() == 0x31u);
    CHECK(!start_park(0u, 6u, 0u) && last_nrc() == 0x31u);
    CHECK(!start_park(0u, 2u, 101u) && last_nrc() == 0x31u);

    /* park release (out 0): channel 7 at the full setpoint, TOE31 off,
     * no enable request (the bit resets the TCU) */
    CHECK(start_park(0u, 2u, 0u));
    run_10ms(1u);
    CHECK(bench_active());                 /* (the monitor's one-shot may
                                             * legitimately own REQUEST now) */
    CHECK(sol_channel(7u)->mode == SOL_MODE_CURRENT && stub.toe_duty == 0u);
    CHECK(sol_channel(7u)->setpoint == 2047u);
    for (uint8_t c = 0u; c < 7u; c++) {
        CHECK(sol_channel(c)->mode == SOL_MODE_OFF);
    }
    run_10ms(210u);                     /* 2 s timeout */
    CHECK(!bench_active() && sol_channel(7u)->mode == SOL_MODE_OFF && stub.toe_duty == 0u);
    {
        uint8_t res[7];
        CHECK(bench_routine(3u, BENCH_PARK_RID, NULL, 0u, res, 7u) == 7 && res[5] == BENCH_END_TIMEOUT);
    }

    /* park hold (out 1): TOE31 duty 1000 default, channel 7 off */
    CHECK(start_park(1u, 1u, 0u));
    run_10ms(1u);
    CHECK(sol_channel(7u)->mode == SOL_MODE_OFF && stub.toe_duty == 1000u);
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
    CHECK(!bench_active() && sol_channel(7u)->mode == SOL_MODE_OFF && stub.toe_duty == 0u);
    engine_rpm(0u);
    run_10ms(2u);

    /* session ends (S3 timeout without tester present) */
    CHECK(start(4u, 300u, 5u));
    run_10ms(501u);
    CHECK(!bench_active() && sol_channel(4u)->mode == SOL_MODE_OFF && uds_session() == 0x01u);

    /* ---- FE13 supervisor lab: one-shot RSPI-B exchange and the cyclic view ---- */
    uds_req((const uint8_t[]){ 0x10, 0x03 }, 2u);
    {
        uint8_t out[256];
        int r;

        uds_req((const uint8_t[]){ 0x31, 0x03, 0xFE, 0x13 }, 4u);
        CHECK(memcmp(&last_rsp()[1], "\x71\x03\xFE\x13\x00", 5) == 0);   /* idle */
        /* r02f: the stock-law walk holds state 7 (a sample in flight)
         * between passes, and raw one-shots are refused while it runs —
         * park it for the raw one-shot checks (this harness never fires
         * the walk shot, so nothing re-arms it until the next pass) */
        plat_walktmr_stop();
        CELL_WALK_STATE = SUP_WALK_IDLE;
        /* malformed one-shots: odd byte count, unknown sub, bare sub-09 */
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x01, 0x06 }, 6u);
        CHECK(last_nrc() == 0x13u);
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x0B }, 5u);
        CHECK(last_nrc() == 0x12u);
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x09 }, 5u);
        CHECK(last_nrc() == 0x13u);
        /* one-shot 0x0600 (even index -> SSLB1), scripted reply 0xA8BD */
        stub.sup_reply_fn = NULL;
        stub.sup_xfer_rx = 0xA8BDu;
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x01, 0x06, 0x00 }, 7u);
        CHECK(memcmp(&last_rsp()[1], "\x71\x01\xFE\x13", 4) == 0);
        r = bench_routine(3u, BENCH_SUP_RID, NULL, 0u, out, sizeof out);
        CHECK(r == 12 && out[0] == 1u && out[1] == 1u && out[2] == 0xA8u && out[3] == 0xBDu);
        /* bus timeout -> conditions not correct */
        stub.lab_fail = true;
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x01, 0x28, 0x00 }, 7u);
        CHECK(last_nrc() == 0x22u);
        stub.lab_fail = false;
        /* the cyclic IS the stock pattern now: mode 2 reports the live replies */
        run_10ms(1u);
        stub.sup_reply_fn = sup_mirror;
        run_10ms(1u);
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x02, 0x00, 0x21 }, 7u);
        CHECK(memcmp(&last_rsp()[1], "\x71\x01\xFE\x13", 4) == 0);
        r = bench_routine(3u, BENCH_SUP_RID, NULL, 0u, out, sizeof out);
        CHECK(r == 35 && out[0] == 2u);
        /* stop disarms the request cell */
        uds_req((const uint8_t[]){ 0x31, 0x02, 0xFE, 0x13 }, 4u);
        r = bench_routine(3u, BENCH_SUP_RID, NULL, 0u, out, sizeof out);
        CHECK(r == 1 && out[0] == 0u);
        /* sub 3 (pattern off via start) also accepted */
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x03 }, 5u);
        CHECK(memcmp(&last_rsp()[1], "\x71\x01\xFE\x13", 4) == 0);
        run_10ms(1u);

        /* ---- sub 10 (0x0A): bus-A five-select one-shot — word i -> chip i%5 ---- */
        CELL_WALK_STATE = 0u;           /* precondition: the walk is parked */
        stub.sup_reply_fn = NULL;       /* script the raw low byte */
        stub.sup_xfer_rx = 0xA2A2u;
        {   /* direct handler call (the 15-byte UDS form is multi-frame; the
             * bench proven MF path covers it - here we test the handler) */
            const uint8_t sub10[] = { 0x0A, 0x24, 0x00, 0x24, 0x00, 0x24,
                                      0x00, 0x24, 0x00, 0x24, 0x00 };
            CHECK(bench_routine(1u, BENCH_SUP_RID, sub10, sizeof sub10, out, 64u) == 0);
        }
        r = bench_routine(3u, BENCH_SUP_RID, NULL, 0u, out, sizeof out);
        CHECK(r == 20 && out[0] == 1u && out[1] == 5u);   /* 5 replies, bus-A lab used */
        CHECK(out[2] == 0xA2u && out[3] == 0xA2u);
        /* ---- sub 9: the walk view (arm, results mode 3, raw one-shot guard) ---- */
        stub.sup_xfer_rx = 0u;
        stub.sup_reply_fn = sup_mirror;
        run_10ms(1u);
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x09, 0x01 }, 6u);
        CHECK(memcmp(&last_rsp()[1], "\x71\x01\xFE\x13", 4) == 0);
        r = bench_routine(3u, BENCH_SUP_RID, NULL, 0u, out, sizeof out);
        CHECK(r == 198 && out[0] == 3u);
        /* raw one-shots refused while the walk owns the stream */
        CELL_WALK_STATE = 0x07u;
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x01, 0x06, 0x00 }, 7u);
        CHECK(last_nrc() == 0x22u);
        CELL_WALK_STATE = 0u;
        /* disarm stops the request */
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x09, 0x00 }, 6u);
        CHECK(CELL_REQUEST == 0u);

        /* ---- sub 16 (0x10): the letters bench arm is REMOVED (L1) ----
         * It wrote the slot-0 request cell directly — the only request
         * writer outside the stock ladder. The ported ladder (sup_ladder.c)
         * owns the request cells through 0x85E64 alone; sub 16 is refused
         * whatever the gate, and nothing it was ever able to stage reaches
         * the wire. The stop path no longer re-ladders. */
        CELL_WALK_STATE = 0x07u;
        for (uint8_t i = 0u; i < 3u; i++) {
            CELL_LETTERS[i] = 0u;
        }
        CELL_SSLB0_BIT2 = 0u;
        run_10ms(1u);
        CHECK(CELL_J2_STAGED == 0x9000u);
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x10, 0x01 }, 6u);
        CHECK(last_nrc() == 0x22u);             /* refused, gate closed */
        CHECK(CELL_LETTERS[0] == 0u);
        CELL_SSLB0_BIT2 = 1u;
        CELL_REG_14 = 0xC7u;
        uds_req((const uint8_t[]){ 0x31, 0x01, 0xFE, 0x13, 0x10, 0x01 }, 6u);
        CHECK(last_nrc() == 0x22u);             /* refused, gate OPEN too */
        run_10ms(3u);
        CHECK(CELL_LETTERS[0] == 0u);           /* nothing earned, nothing armed */
        CHECK(CELL_LAD_REQ[0] == 0u && CELL_LAD_REQ[1] == 0u);
        CHECK(CELL_EN_90 == 0x00u);
        CHECK(CELL_J2_STAGED == 0x9000u);       /* no rail command on the wire */
        uds_req((const uint8_t[]){ 0x31, 0x02, 0xFE, 0x13 }, 4u);
        run_10ms(2u);
        CHECK(CELL_LETTERS[0] == 0u && CELL_LETTERS[1] == 0u && CELL_LETTERS[2] == 0u);
        CHECK(CELL_J2_STAGED == 0x9000u);
        CELL_SSLB0_BIT2 = 0u;
        CELL_REG_14 = 0u;
        CELL_WALK_STATE = 0u;
    }

    if (fails == 0) {
        printf("test_bench: ok\n");
    }
    return fails != 0;
}
