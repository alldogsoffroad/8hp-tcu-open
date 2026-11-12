/* Unit test: the rebuild's core machinery (design §6/§7).
 *   - the scheduler (tick → ring → drain, phases, overrun counter)
 *   - the bus-B job engine (queue → launch → ISR simulation → handler → release,
 *     one-in-flight, use counters)
 *   - the ctrl+C9 composers (normal + engaged dialects, the request cell law)
 *   - the walk init (cells at the stock's initial values) and the W1/W2 builders
 *   - the batch init (serial_rx_init law: request 0, outputs disabled, J5 queued)
 *   - the reply condense (grant cell, WDT A-law)
 */
#include "bsw/sched/sched.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_walk.h"
#include "bsw/sup/sup_batch.h"
#include "bsw/sup/sup_ctrl.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/sup/sup_token.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static unsigned s_t1, s_t10;
static void tick_task(void) { s_t1++; }
static void ten_task(void) { s_t10++; }

static const sched_ent_t s_sched_tbl[] = {
    {  1u, 0u, tick_task, RT_1MS  },
    { 10u, 3u, ten_task,  RT_10MS },
};

/* run every queued job to completion (the test plays the RX ISR) */
static void drain_rspi(void)
{
    for (uint8_t b = 0u; b < 2u; b++) {
        unsigned guard = 256u;
        while (stub_rspi_busy(b) && guard-- != 0u) {
            stub_rspi_isr(b);
        }
    }
}

/* scripted supervisor: the 0x22 read mirrors the staged ctrl byte */
static uint16_t sup_mirror(uint16_t tx)
{
    return (tx & 0xFF00u) == 0x2200u ? CELL_CTRL_STAGED : 0x0000u;
}

static void test_scheduler(void)
{
    sched_init(s_sched_tbl, (uint8_t)(sizeof s_sched_tbl / sizeof s_sched_tbl[0]));
    s_t1 = s_t10 = 0u;
    for (unsigned i = 0u; i < 20u; i++) {
        sched_tick_isr();
        sched_drain();
    }
    CHECK(s_t1 == 20u);
    CHECK(s_t10 == 2u);                     /* phase 3: fires at ticks 3 and 13 */
    CHECK(sched_now_ms() == 20u);
}

static void test_job_engine(void)
{
    sup_jobs_init();
    CHECK(sup_job_check_free(JOB_STAT));

    /* queue → in flight → the slot is taken; a second queue is refused (NR-6) */
    CHECK(sup_job_queue(JOB_STAT));
    CHECK(!sup_job_queue(JOB_STAT));
    sup_job_engine_pump();
    CHECK(plat_rspi_busy(1u));
    CHECK(!sup_job_check_free(JOB_STAT));
    CHECK(CELL_USE_COUNTER(JOB_STAT) == 1u);        /* counted at launch */

    /* the ISR collects 5 frames; the handler releases the slot */
    for (uint8_t i = 0u; i < 5u; i++) {
        stub_rspi_isr(1u);
    }
    CHECK(!plat_rspi_busy(1u));
    CHECK(sup_job_check_free(JOB_STAT));

    /* the walk queue has priority over the cyclic queue (design §7.2) */
    CHECK(sup_job_queue(JOB_STAT));
    CHECK(sup_job_queue(JOB_DUTY));
    sup_job_engine_pump();
    drain_rspi();
    CHECK(CELL_USE_COUNTER(JOB_DUTY) == 1u);
    CHECK(CELL_USE_COUNTER(JOB_STAT) == 2u);

    /* B1 (baseline review): completions CHAIN. One pump, then only the RX
     * interrupt: every queued job must reach the wire through the reply
     * handlers' own engine pumps (stock 0x93A08 → 0x93902). r01s..r02f held
     * plat_rspi_busy true across the handler, so only the first job ran. */
    {
        uint16_t u_stat = CELL_USE_COUNTER(JOB_STAT);
        uint16_t u_id = CELL_USE_COUNTER(JOB_ID);
        uint16_t u_ctrl = CELL_USE_COUNTER(JOB_CTRL);

        CHECK(sup_job_queue(JOB_STAT));
        CHECK(sup_job_queue(JOB_ID));
        CHECK(sup_job_queue(JOB_CTRL));
        sup_job_engine_pump();
        drain_rspi();                       /* ISR only — no further pumps */
        CHECK(CELL_USE_COUNTER(JOB_STAT) == (uint16_t)(u_stat + 1u));
        CHECK(CELL_USE_COUNTER(JOB_ID) == (uint16_t)(u_id + 1u));
        CHECK(CELL_USE_COUNTER(JOB_CTRL) == (uint16_t)(u_ctrl + 1u));
        CHECK(sup_job_check_free(JOB_STAT));
        CHECK(sup_job_check_free(JOB_ID));
        CHECK(sup_job_check_free(JOB_CTRL));
    }

    /* S2: an ERROR completion (frames = 0) harvests nothing, frees the
     * slot, and still chains the next queued job */
    {
        uint16_t u_id = CELL_USE_COUNTER(JOB_ID);

        CELL_ECHO_22 = 0x5Au;
        CHECK(sup_job_queue(JOB_CTRL));
        CHECK(sup_job_queue(JOB_ID));
        sup_job_engine_pump();
        stub.rspi_err_next = true;
        drain_rspi();
        CHECK(CELL_ECHO_22 == 0x5Au);       /* J1's error burst: no harvest */
        CHECK((CELL_J1_ERR & 2u) != 0u);    /* SF1: the error branch's bits */
        CHECK((CELL_B0_ERR & 2u) != 0u);
        CHECK(CELL_USE_COUNTER(JOB_ID) == (uint16_t)(u_id + 1u));
        CHECK(sup_job_check_free(JOB_CTRL));
        CHECK(sup_job_check_free(JOB_ID));
    }

    /* the lab job path (design §7.2): blocking, one at a time */
    {
        uint16_t tx[2] = { 0x2200u, 0x0900u };
        uint16_t rx[2] = { 0u, 0u };

        CHECK(sup_lab_job_queue(tx, rx, 2u, SEL_B1_B0, 2u));
        CHECK(rx[0] == 0xA800u);            /* frame 0 → SSLB1 (A8 class) */
        CHECK(rx[1] == 0xAA00u);            /* frame 1 → SSLB0 (AA class) */
    }
}

static void test_ctrl_composer(void)
{
    sup_jobs_init();

    /* ── normal dialect (mode & 0x11 != 0x11): the echo-complement life sign ── */
    CELL_MODE_BYTE = 0u;
    CELL_OUTPUTS_DIS = 1u;
    CELL_REQUEST = 0u;
    CELL_ECHO_22 = 0u;                      /* echo bit 2 clear → we send bit 2 */
    CELL_CTRL_BIT1_SRC = 0u;
    sup_ctrl_compose();
    CHECK(CELL_CTRL_STAGED == 0x04u);
    /* S7: the composer never touches the TX staging (J1 may be in flight);
     * the J1 arm in the batch stages 0x8E00|CELL_CTRL_STAGED check-free */
    CHECK(sup_job_table[JOB_CTRL].tx_stage[0] == 0x8E00u);
    /* the request cell is NOT forced while outputs are disabled (boot law) */
    CHECK(CELL_REQUEST == 0u);

    /* echo bit 2 set → complement drops the bit */
    CELL_ECHO_22 = 0x04u;
    sup_ctrl_compose();
    CHECK(CELL_CTRL_STAGED == 0x00u);

    /* ── the request cell: forced every pass once outputs are enabled ──────── */
    CELL_OUTPUTS_DIS = 0u;
    sup_ctrl_compose();
    CHECK(CELL_REQUEST == 1u);
    CHECK(CELL_CTRL_STAGED == 0x01u);
    /* bit 1 rides the 0x90-composer's cell */
    CELL_CTRL_BIT1_SRC = 0x02u;
    sup_ctrl_compose();
    CHECK((CELL_CTRL_STAGED & 0x02u) != 0u);
    CELL_CTRL_BIT1_SRC = 0u;
    CELL_OUTPUTS_DIS = 1u;

    /* ── engaged dialect (mode & 0x11 == 0x11): bit 4 toggles, bit 7 held ──── */
    CELL_MODE_BYTE = 0x11u;
    CELL_OUTPUTS_DIS = 0u;
    CELL_REQUEST = 0u;
    sup_ctrl_compose();
    CHECK((CELL_CTRL_STAGED & 0x10u) != 0u);        /* first pass: bit 4 set */
    CHECK((CELL_CTRL_STAGED & 0x80u) != 0u);        /* mode & 0x20 == 0 → bit 7 */
    sup_ctrl_compose();
    CHECK((CELL_CTRL_STAGED & 0x10u) == 0u);        /* second pass: bit 4 clear */
    /* bit 0 rides from the request cell (the engaged 0x8E91/0x8E81 pair) */
    CHECK(CELL_REQUEST == 1u);
    sup_ctrl_compose();
    /* 0x91 = the engaged live pair's SET half: bit 7 (mode) + bit 4 (toggle)
     * + bit 0 (the request cell) */
    CHECK(CELL_CTRL_STAGED == 0x91u);
    CELL_MODE_BYTE = 0u;
    CELL_OUTPUTS_DIS = 1u;

    /* ── the C9 composer: bit 4 while the shutdown selftest is clear ───────── */
    CELL_C9_BASE = 0u;
    CELL_C9_HIGH = 0u;
    CELL_C9_VARIANT = 0u;
    CELL_SHUTDOWN_ST = 0u;
    sup_c9_compose();
    CHECK(CELL_C9_STAGED == 0x10u);
    CELL_SHUTDOWN_ST = 1u;
    sup_c9_compose();
    CHECK(CELL_C9_STAGED == 0x00u);
    CELL_SHUTDOWN_ST = 0u;
}

/* SF2 (r02g review): the handlers against the stock bit map — each check
 * here fails if the old r02f handler behaviour comes back */
extern void sup_resp_ctrl(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void sup_resp_stat(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void sup_resp_en(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);

static void clear_triples(void)
{
    CELL_J1_OK = 0u; CELL_J1_ERR = 0u; CELL_J1_ECHOFAIL = 0u;
    CELL_B0_OK = 0u; CELL_B0_ERR = 0u; CELL_B0_ECHOFAIL = 0u;
}

static void test_handlers(void)
{
    sup_jobs_init();

    /* J6 (0x92BBE): rx[4] — the 0x14 gate register — is NEVER class-
     * checked; an 0xA9-class 0x14 reply still harvests the gate bit */
    {
        uint16_t rx[5] = { 0xA818u, 0xAA0Cu, 0xA81Eu, 0xAA06u, 0xA902u };
        clear_triples();
        CELL_SSLB0_BIT2 = 0u;
        sup_resp_stat(1u, JOB_STAT, rx, 5u);
        CHECK(CELL_SSLB0_BIT2 == 1u);
        CHECK(CELL_REG_14 == 0x02u);
        CHECK((CELL_J1_OK & 4u) != 0u);
        CHECK((CELL_B0_OK & 4u) != 0u);
    }
    /* J1 (0x92D94): an SSLB1 class failure is ERR|=2 and ECHOFAIL|=2, never
     * OK — even when the echo byte matches; the echo is still harvested */
    {
        uint16_t *tx = sup_job_table[JOB_CTRL].tx_stage;
        uint16_t rx[4] = { 0xA800u, 0xAA00u, 0xAB05u, 0xAA10u };
        tx[0] = 0x8E05u; tx[1] = 0xC910u;
        clear_triples();
        sup_resp_ctrl(1u, JOB_CTRL, rx, 4u);
        CHECK((CELL_J1_ERR & 2u) != 0u);
        CHECK((CELL_J1_ECHOFAIL & 2u) != 0u);
        CHECK((CELL_J1_OK & 2u) == 0u);
        CHECK(CELL_ECHO_22 == 0x05u);
        CHECK((CELL_B0_OK & 2u) != 0u);           /* C9 echo 10 == staged 10 */
        CHECK((CELL_J1_OK & 0x20u) == 0u);        /* 0x20 is F's bit, not C9 */
        /* class ok, echo matches the SENT word (bit 5 ignored) */
        rx[2] = 0xA825u;
        clear_triples();
        sup_resp_ctrl(1u, JOB_CTRL, rx, 4u);
        CHECK((CELL_J1_OK & 2u) != 0u);
    }
    /* J2 (0x92ED4): the 0x24 echo harvested unconditionally */
    {
        uint16_t rx[2] = { 0xA800u, 0xAB77u };
        clear_triples();
        CELL_ECHO_24 = 0u;
        sup_resp_en(1u, JOB_EN, rx, 2u);
        CHECK(CELL_ECHO_24 == 0x77u);
        CHECK((CELL_J1_ERR & 0x10u) != 0u);
    }
    /* SF1: the error completion (n = 0) takes each handler's not-'@'
     * branch: error bits, no harvest, no F re-fire */
    {
        uint16_t rx[5] = { 0xA8FFu, 0xAAFFu, 0xA8FFu, 0xAAFFu, 0xA8E2u };
        clear_triples();
        CELL_REG_14 = 0x00u;
        sup_resp_stat(1u, JOB_STAT, rx, 0u);
        CHECK((CELL_J1_ERR & 4u) != 0u);
        CHECK((CELL_B0_ERR & 4u) != 0u);
        CHECK(CELL_REG_14 == 0x00u);
        CHECK(sup_job_check_free(JOB_FLT));       /* not re-fired */
    }
    /* the 0x93126 summarizer: a complete round folds and clears */
    {
        clear_triples();
        CELL_J1_OK = 0x77u;
        CELL_B0_OK = 0x07u;
        CELL_ID_B0_00 = 0u; CELL_ID_B1_00 = 0u;
        sup_reply_summarize();
        CHECK(CELL_SUM_B1 == 0x01u && CELL_J1_OK == 0u);
        CHECK(CELL_SUM_B0 == 0x01u && CELL_B0_OK == 0u);
        CELL_J1_OK = 0x02u;                       /* partial round: kept */
        sup_reply_summarize();
        CHECK(CELL_J1_OK == 0x02u);
    }
}

/* S12: the token table against the stock driving dump (walkdata review):
 * 0x92414 staged = 63936C9C (selector 6), 0x90548 last sent = 2CDC23D3
 * (selector 7); the fresh-chip opening token (selector 0) = FF0FF000 */
static void test_token_table(void)
{
    CHECK(sup_token_compute(6u) == 0x63936C9Cu);
    CHECK(sup_token_compute(7u) == 0x2CDC23D3u);
    CHECK(sup_token_compute(0u) == 0xFF0FF000u);
}

static void test_walk_init(void)
{
    sup_jobs_init();
    sup_walk_init();

    /* the stock's init values (spec 22 §6.1): value pair 0x3F/0xC0, period
     * 0x3138, expected = reported = 7, era counter 1, boot flag 1, state 0 */
    CHECK(CELL_VALUE == 0x3Fu);
    CHECK(CELL_VALUE_COMPL == 0xC0u);
    CHECK(CELL_PERIOD == 0x3138u);
    CHECK(CELL_LVL_EXPECTED == 7u);
    CHECK(CELL_LVL_REPORTED == 7u);
    CHECK(CELL_ERA_COUNT == 1u);
    CHECK(CELL_BOOT_FLAG == 1u);
    CHECK(CELL_WALK_STATE == 0u);
    CHECK(CELL_JOB_MODE == 0u);
    /* NO 84 word is sent and the walk timer is NOT started (design D4/NR-2) */
    CHECK(!plat_walktmr_running());
    CHECK(stub.walktmr_starts == 0u);

    /* the W2 level builder: stages 0x8400|value and queues JOB_LEVEL — but
     * 0x3F never goes on the wire (D4: the init value is not a wire word) */
    sup_level_job();
    sup_job_engine_pump();
    drain_rspi();
    CHECK(CELL_USE_COUNTER(JOB_LEVEL) == 0u);
    CELL_VALUE = 0x06u;                     /* the 0x06 era: a wire word */
    sup_level_job();
    sup_job_engine_pump();
    drain_rspi();
    CHECK(sup_job_table[JOB_LEVEL].tx_stage[0] == 0x8406u);
    CHECK(CELL_STAGED_84 == 0x06u);
    CHECK(CELL_USE_COUNTER(JOB_LEVEL) == 1u);

    /* the W1 duty builder: the stock's 7-word shape {88,88,88,88,12,0C,0A} */
    CELL_USE_COUNTER(JOB_DUTY) = 0u;        /* the counters carry across sections */
    sup_duty_job();
    sup_job_engine_pump();
    drain_rspi();
    CHECK(sup_job_table[JOB_DUTY].tx_stage[0] == 0x8800u);
    CHECK(sup_job_table[JOB_DUTY].tx_stage[4] == 0x1200u);
    CHECK(sup_job_table[JOB_DUTY].tx_stage[6] == 0x0A00u);
    CHECK(CELL_USE_COUNTER(JOB_DUTY) == 1u);
}

static void test_batch_and_condense(void)
{
    sup_jobs_init();
    sup_walk_init();
    sup_batch_init();
    stub.sup_reply_fn = sup_mirror;

    /* the serial_rx_init law (stock 0x91DD2) */
    CHECK(CELL_REQUEST == 0u);
    CHECK(CELL_OUTPUTS_DIS == 1u);
    CHECK(CELL_SSLB2_RAW == 0x8000u);       /* the sentinel */

    /* NO BATCH SILENCE (r02c): the stock's batch 0x8EE82 runs from the
     * first OS tick (5 ms mode-0 task list 0x42CD8, entry 26). This section
     * drives the BATCH ALONE — the monitor (its own 2.5 ms task since r02d)
     * is exercised in test_opening_law. Two 5 ms passes: the batch queues
     * the cyclic jobs and the engine drains; the SSLB2 reply lands in the
     * copy cell (the condense of the next pass copies the handler's cell) */
    stub.frt_us += 5000u;
    sup_batch_5ms();
    drain_rspi();
    CHECK(CELL_REQUEST == 0u);              /* outputs still disabled */
    CHECK(CELL_USE_COUNTER(JOB_ID) >= 1u);  /* the init J5 launched (pass 1's J5
                                             * was refused: already queued) */
    stub.frt_us += 5000u;
    sup_batch_5ms();
    drain_rspi();
    CHECK(CELL_SSLB2_COPY == 0xB400u);      /* the scripted SSLB2 reply */

    /* the WDT A-law: bit 0 sent AND echoed → interval mode, once */
    CHECK(!plat_wdt_is_interval());
    sup_enable_request(true);               /* the request cell arms */
    stub.frt_us += 5000u;
    sup_batch_5ms();                        /* compose: bit 0 in the word */
    drain_rspi();                           /* the echo returns bit 0 (staged ctrl) */
    sup_batch_5ms();                        /* the condense sees the confirmed edge */
    drain_rspi();
    CHECK(plat_wdt_is_interval());
    CHECK(CELL_GRANT == 1u);                /* the 0x22 echo's bit 0 */
}

/* THE OPENING LAW (r02d, decoded from the stock's task table): the monitor
 * 0x873F0 is entry 2 of the 2.5 ms task list in every mode (stub 0x7EE2E)
 * and has no lead-in and no bit-1 gate — its FIRST pass stages the 0x06 era
 * and arms the walk timer; the ask follows the first reply with level < 7
 * (NR-1 holds structurally). The r01w pre-gate return and the r01x..r02c
 * 20-pass lead-in were both [I] and are gone. */
static void test_opening_law(void)
{
    unsigned starts0;

    sup_jobs_init();
    sup_walk_init();
    sup_batch_init();
    plat_walktmr_stop();       /* clear the previous section's flag — the
                                * assertions below are about DELTAS */
    starts0 = stub.walktmr_starts;
    CHECK(CELL_OUTER_FSM == SUP_FSM_BOOT);
    CHECK(CELL_VALUE == 0x3Fu);             /* the init pair unstaged */

    /* THE ARM — the monitor's FIRST pass, unconditionally */
    stub.frt_us += 2500u;
    sup_monitor_poll();
    CHECK(CELL_OUTER_FSM == SUP_FSM_SUPERVISE);  /* the 0x06 era staged → 8 */
    /* r02f: the first stage's delay is the stock's 5 µs floor, so the
     * 84|06 goes out at once (no down-counter shot): it is staged and
     * launched, not merely timer-armed */
    (void)starts0;
    CHECK(CELL_STAGED_84 == 0x06u);
    CHECK(CELL_USE_COUNTER(JOB_LEVEL) != 0u);
    CHECK(CELL_VALUE == 0x06u);                  /* the 0x06 era, not 0x3F */
    /* the ask follows the walk's first replies (the stock's one-shot law:
     * level < 7 — the reported cell still holds the boot init 7 here) */
    CHECK(CELL_REQUEST == 0u);
    CHECK(CELL_EN_ONESHOT == 0u);
    CELL_LVL_REPORTED = 6u;                      /* the first 0x06-era replies */
    sup_monitor_poll();
    CHECK(CELL_REQUEST == 1u);                   /* the ask arms (NR-1: the
                                                  * conversation is rolling) */
    CHECK(CELL_EN_ONESHOT == 1u);
    CHECK(CELL_OUTPUTS_DIS == 0u);
    CELL_SSLB0_BIT2 = 0u;                        /* a gate close does NOT
                                                  * un-arm (the stock never
                                                  * re-flips; the ladder owns
                                                  * revocation) */
    sup_monitor_poll();
    CHECK(CELL_OUTER_FSM != SUP_FSM_BOOT);   /* the ladder ADVANCES past
                                              * supervise (the r01w freeze was
                                              * the old gate's artifact; the
                                              * stock's FSM never stops on a
                                              * gate close — only letters
                                              * are revoked) */
}

int main(void)
{
    test_handlers();
    test_token_table();
    test_scheduler();
    test_job_engine();
    test_ctrl_composer();
    test_walk_init();
    test_batch_and_condense();
    test_opening_law();
    if (fails == 0) {
        printf("test_rebuild: ok\n");
    }
    return fails != 0;
}
