/* THE OUTPUT DIAGNOSIS (2026-11-17, bsw/drv/drv_odiag.c;
 * ~/tcu_sweep/output_diag_port.md).
 *
 * Part 1 — "at the bench's healthy replies the wire and the law are
 * unchanged": the request producer (compile-time inputs) → the request law
 * → the bus-A cyclic / config / ID words are run through a bench timeline
 * (boot, the phase-0x10 and '3' full-on tests, 'U', the park release on and
 * off, a letter drop, a rail dip) at main.c's cadence (1 ms diag poll, 5 ms
 * refresh, 10 ms producer + periodic send), with bus A drained every ms.
 * Per 5 ms tick the program hashes every cyclic / config / ID word put on
 * the wire since the last tick and every law cell, and prints "t hash".
 * The golden file tests/unit/golden/odiag_head_a6ec870.txt was produced by
 * THIS program built against HEAD a6ec870 (-DOD_HEAD: no diagnosis, the
 * r03v firmware). The working tree runs the same timeline WITH the
 * diagnosis, unseeded, harvesting from a model of the bench's four healthy
 * driver chips (their r03v replies: config readbacks A29A / A221 / A29F /
 * A255, setpoint readbacks, ID BD 12, diag bytes 00, the empty fifth
 * select 0000) — every hash must equal the golden one, and the diagnosis
 * must end with no fault word, no failed DSM report and no would-be reset.
 *
 *   test_odiag                  print the trace
 *   test_odiag GOLDEN           part 1: compare with GOLDEN, then parts 2–3
 *
 * Part 2 (working tree only) — injected chip faults, per channel: the
 * stock's reaction on that channel alone, reported to its DSM groups.
 * Part 3 — the witness-only fatal checks count and never reset.
 */
#include "bsw/drv/drv_cells.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_rqprod.h"
#include "bsw/sup/sup_cells.h"
#include "tests/unit/stub_plat.h"
#if !defined(OD_HEAD)
#include "bsw/drv/drv_odiag.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct { uint8_t bus; uint8_t job; uint16_t word; uint32_t t_us; } stub_log[32768];
extern unsigned stub_log_n;
extern unsigned stub_log_dropped;

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static uint64_t fnv(uint64_t h, uint32_t v)
{
    for (unsigned k = 0u; k < 4u; k++) {
        h ^= (uint8_t)(v >> (8u * k));
        h *= 0x100000001B3ull;
    }
    return h;
}

/* ── the model of the bench's driver chips (r03v witness, solenoid_chain
 * §B.4): four chips on SSLA3..0, the fifth select empty. A read replies in
 * the same frame (the r03v rx rows). 8-bit commands reply A2|data, the
 * 11-bit setpoint commands (E8/F0 write, C0/D8 read) A000|data. ──────── */
#if !defined(OD_HEAD)
static uint8_t  m_reg8[5][256];
static uint16_t m_e8[5], m_f0[5];
uint8_t  m_diag[5][2];          /* the 0x18 / 0x1E diagnosis bytes (faults) */
uint8_t  m_id_lo[5] = { 0xBDu, 0xBDu, 0xBDu, 0xBDu, 0u };
bool     m_dead[5] = { false, false, false, false, true };

static uint16_t chip_model(uint16_t tx, uint8_t c)
{
    const uint8_t top5 = (uint8_t)(tx >> 11);
    const uint8_t cmd = (uint8_t)(tx >> 8);
    const uint8_t d = (uint8_t)tx;
    if (c >= 5u || m_dead[c]) {
        return 0x0000u;
    }
    switch (top5) {
    case 0x1Du: m_e8[c] = (uint16_t)(tx & 0x7FFu); return 0xA000u;
    case 0x1Eu: m_f0[c] = (uint16_t)(tx & 0x7FFu); return 0xA000u;
    case 0x18u: return (uint16_t)(0xA000u | m_e8[c]);
    case 0x1Bu: return (uint16_t)(0xA000u | m_f0[c]);
    default: break;
    }
    switch (cmd) {
    case 0x88u: case 0x8Eu: case 0x90u: case 0x96u: case 0xA0u: case 0xA6u:
        m_reg8[c][cmd] = d;
        return 0xA200u;
    case 0x28u: return (uint16_t)(0xA200u | 0x80u | m_reg8[c][0x88]);
    case 0x30u: return (uint16_t)(0xA200u | m_reg8[c][0x90]);
    case 0x2Eu: return (uint16_t)(0xA200u | m_reg8[c][0x8E]);
    case 0x36u: return (uint16_t)(0xA200u | m_reg8[c][0x96]);
    case 0x60u: return (uint16_t)(0xA200u | m_reg8[c][0xA0]);
    case 0x66u: return (uint16_t)(0xA200u | m_reg8[c][0xA6]);
    case 0x18u: return (uint16_t)(0xA200u | m_diag[c][0]);
    case 0x1Eu: return (uint16_t)(0xA200u | m_diag[c][1]);
    case 0x06u: return 0xA212u;                         /* ID row 0 */
    case 0x00u: return (uint16_t)(0xA200u | m_id_lo[c]); /* ID row 1 */
    default: return 0xA200u;
    }
}
#endif

/* the 5 ms countdown service 0x7C7E4 (sup_ladder's; local here) */
static void countdown(void)
{
    for (uint32_t a = 0xFFF94734u; a < 0xFFF94786u; a += 2u) {
        volatile uint16_t *w = (volatile uint16_t *)(uintptr_t)a;
        if (*w != 0u) {
            *w = (uint16_t)(*w - 1u);
        }
    }
    for (uint32_t a = 0xFFF94788u; a < 0xFFF94835u; a++) {
        volatile uint8_t *b = (volatile uint8_t *)(uintptr_t)a;
        if (*b != 0u) {
            *b = (uint8_t)(*b - 1u);
        }
    }
}

static void drain(void)
{
    unsigned guard = 400u;
    while (stub_rspi_busy(0u) && guard-- != 0u) {
        stub_rspi_isr(0u);
    }
}

/* the supervisor side of the bench timeline (ms) */
static void sup_state(uint32_t t)
{
    uint8_t s = 0x11u;
    CELL_LAD_94114 = (uint8_t)((t >= 200u && t < 230u) ? 1u : 0u);   /* phase 0x10 */
    CELL_DIAG_94115 = (uint8_t)((t >= 310u && t < 340u) ? 1u : 0u);  /* '3' full-on */
    if (t >= 300u) {
        s = (uint8_t)'3';
    }
    if (t >= 400u) {
        s = (uint8_t)'U';
    }
    if (t >= 1500u && t < 1860u) {
        s = (uint8_t)'4';                                    /* the 0x85E82 letter drop */
    }
    CELL_SEQ2_STATE[0] = s;
    CELL_SEQ2_STATUS[0] = (uint8_t)(s == (uint8_t)'U' ? 4u : 1u);
    CELL_J2_STAGED = (uint16_t)(s == (uint8_t)'U' ? 0x9020u : 0x9000u);
    CELL_LAD_REQ[0] = 0u;
    CELL_LAD_REQ[1] = 0u;
    CELL_OUT_MV[0] = (uint16_t)(s == (uint8_t)'U' ? 13336u : 2939u);
    CELL_OUT_MV[1] = 24916u;                                /* mux 10 */
    if (t >= 2000u && t < 2100u) {
        CELL_OUT_MV[0] = 7000u;                             /* a rail dip (the UV law) */
    }
    drv_rq_request_set(7u, (uint16_t)((t >= 1000u && t < 1300u) ? 10000u : 0u));
}

static uint64_t law_hash(uint64_t h)
{
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        h = fnv(h, CELL_RQ_I[ch]);
        h = fnv(h, CELL_RQ_I2[ch]);
        h = fnv(h, CELL_RQ_MODE[ch]);
        h = fnv(h, CELL_RQ_ACTIVE[ch]);
        h = fnv(h, CELL_RQ_LETTER[ch]);
        h = fnv(h, CELL_RQ_CUR[ch]);
        h = fnv(h, CELL_RQ_LET_APPLIED[ch]);
        h = fnv(h, CELL_RQ_CUR_SENT[ch]);
    }
    h = fnv(h, CELL_RQ_FAULTMASK);
    h = fnv(h, CELL_RQ_FULLMASK);
    h = fnv(h, CELL_RQ_UVFLAGS);
    h = fnv(h, CELL_RQ_ROUND);
    for (uint8_t c = 0u; c < 5u; c++) {        /* the composed record fields only */
        static const uint8_t OFS[] = { 0x07, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x10, 0x14, 0x15, 0x18 };
        for (unsigned k = 0u; k < sizeof OFS; k++) {
            h = fnv(h, CELL_REC_U8(c, OFS[k]));
        }
    }
    for (unsigned k = 0u; k < 7u; k++) {        /* the producer's currents */
        h = fnv(h, *(volatile uint16_t *)(uintptr_t)(0xFFF84B78u + 2u * k));
    }
    return h;
}

/* the wire: every cyclic / config / ID word fed since the last call (the
 * diag job — the diagnosis' own addition — is not part of the comparison;
 * it is counted) */
static unsigned s_diag_words;
static uint64_t wire_hash(uint64_t h)
{
    for (unsigned k = 0u; k < stub_log_n; k++) {
        if (stub_log[k].bus != 0u) {
            continue;
        }
        if (stub_log[k].job == DRV_DIAG) {
            s_diag_words++;
            continue;
        }
        h = fnv(h, ((uint32_t)stub_log[k].job << 16) | stub_log[k].word);
    }
    stub_log_n = 0u;
    return h;
}

static void boot(void)
{
#if !defined(OD_HEAD)
    stub.drv_reply_fn = chip_model;
    drv_od_host_seed_select(false);     /* harvest from zero, as the target */
#endif
    stub.frt_us = 0u;
    stub.ms = 0u;
    drv_jobs_init();                    /* → drv_rq_init → drv_od_init / rq_prod_init */
    *(volatile uint8_t *)0xFFF89C13u = 0x43u;   /* the temperature byte (dump) */
    *(volatile uint8_t *)0xFFF89CD2u = 0x43u;
#if defined(DRV_RQ_USE_PRODUCER) && DRV_RQ_USE_PRODUCER
    drv_rq_prod_host_select(true);
#endif
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        drv_rq_request_set(ch, 0u);
    }
    sup_state(0u);
    rq_prod_boot_pass();
    stub_log_n = 0u;
}

/* one millisecond of main.c */
static void step_ms(uint32_t t)
{
    stub.frt_us += 1000u;
    stub.ms++;
    sup_state(t);
#if !defined(OD_HEAD)
    drv_od_diag_poll(stub.frt_us);      /* task_1ms */
#endif
    drain();
    if (t % 5u == 0u) {                 /* the 5 ms supervisor tick */
        countdown();
        drv_letter_refresh();           /* after seq2 (sup_batch.c) */
        drain();
        drv_id_kick();                  /* the batch 0x8EE82 → 0x921A0 */
        drain();
    }
    if (t % 10u == 5u) {                /* the 10 ms task */
        rq_prod_task_10ms(1u);
        drv_cyclic_periodic();
        drain();
    }
}

#if !defined(OD_HEAD)
static uint32_t s_fail_at_u[21];
#endif

static int part1(FILE *out, FILE *golden)
{
    char line[128];
    unsigned mismatches = 0u;
    boot();
    for (uint32_t t = 1u; t <= 2600u; t++) {
        step_ms(t);
#if !defined(OD_HEAD)
        if (t == 1855u) {
            for (uint8_t g = 1u; g <= 20u; g++) {
                CHECK(drv_od_group(g)->fail_n == 0u);   /* nothing before the return */
            }
        }
        if (t == 1870u) {
            for (uint8_t g = 1u; g <= 20u; g++) {
                s_fail_at_u[g] = drv_od_group(g)->fail_n;
            }
        }
#endif
        if (t % 5u == 0u) {
            const uint64_t h = law_hash(wire_hash(0xCBF29CE484222325ull));
            if (golden != NULL) {
                unsigned long long g = 0ull;
                unsigned gt = 0u;
                if (fgets(line, sizeof line, golden) == NULL ||
                    sscanf(line, "%u %llx", &gt, &g) != 2 || gt != t || g != h) {
                    if (mismatches < 10u) {
                        printf("MISMATCH t=%u got %016llx golden %s", t, (unsigned long long)h, line);
                    }
                    mismatches++;
                }
            } else {
                fprintf(out, "%u %016llx\n", t, (unsigned long long)h);
            }
        }
    }
    CHECK(stub_log_dropped == 0u);
    if (golden != NULL) {
        CHECK(mismatches == 0u);
        printf("part 1: %u / %u ticks equal to the r03v trace; diag words on the wire %u\n",
               520u - mismatches, 520u, s_diag_words);
    }
#if !defined(OD_HEAD)
    if (golden != NULL) {
        /* the diagnosis ran, harvested and found nothing */
        CHECK(s_diag_words > 0u && drv_od_diag_sent_n() > 200u);
        for (uint8_t ch = 0u; ch < 10u; ch++) {
            CHECK(*(volatile uint16_t *)(uintptr_t)(0xFFF93004u + 2u * ch) == 0u);   /* 0x93004 */
            CHECK(*(volatile uint8_t *)(uintptr_t)(0xFFF93A42u + ch) == 0u);         /* 0x93A42 */
        }
        for (uint8_t c = 0u; c < 4u; c++) {
            const uint16_t w = *(volatile uint16_t *)(uintptr_t)(0xFFF9030Cu + 4u * c);
            CHECK((w & 0xE100u) == 0xC100u);                    /* ID, readback, OK */
            CHECK(CELL_REC_U8(c, 0x04u) == 0xBDu && CELL_REC_U8(c, 0x05u) == 0x12u);
        }
        /* the only failed reports: ONE pass at the '4' → 'U' return (t =
         * 1860) on the seven driven channels — 0x93B0C was 0x30 through the
         * letter drop (a request with the output disabled) and the feedback
         * class is still the disabled output's '!' on the first 'U' pass,
         * so 0x89F5C sets bit 2 (monitor 2) for one pass and heals on the
         * next; the stock's feedback (Timer F, 10 ms) lags the same way [I]
         * and the DSM's 60–100 ms fail debounce of groups 1–14 absorbs it */
        for (uint8_t g = 1u; g <= 20u; g++) {
            const drv_od_grp_t *G = drv_od_group(g);
            if (G->fail_n != 0u || G->pend_n != 0u) {
                printf("  group %u: fail_n %u pend_n %u (last: mon %u failed %u tested %u)\n", g,
                       (unsigned)G->fail_n, (unsigned)G->pend_n, G->monitor, G->failed, G->tested);
            }
            CHECK(G->fail_n == s_fail_at_u[g] && G->pend_n == 0u);
            CHECK(G->fail_n <= 1u && (g <= 14u || G->fail_n == 0u));
        }
        CHECK(drv_od_group(3)->tested == 1u);           /* D tested and passed */
        CHECK(drv_od_fatal()->n == 0u);
        printf("part 1: fault words 0, one-pass reports only at the 4->U return, no would-be reset; diag jobs %u (dropped %u)\n",
               (unsigned)drv_od_diag_sent_n(), (unsigned)drv_od_diag_drop_n());
    }
#endif
    return 0;
}

#if !defined(OD_HEAD)
/* ── part 2: injected chip faults ──────────────────────────────────────── */
static uint16_t fw(uint8_t ch) { return *(volatile uint16_t *)(uintptr_t)(0xFFF93004u + 2u * ch); }

/* run the bench timeline to t_end with a fault applied from t_on */
static void run_to(uint32_t t_from, uint32_t t_end)
{
    for (uint32_t t = t_from; t <= t_end; t++) {
        step_ms(t);
    }
    stub_log_n = 0u;
}

static void reset_model(void)
{
    memset(m_diag, 0, sizeof m_diag);
    for (uint8_t c = 0u; c < 4u; c++) {
        m_id_lo[c] = 0xBDu;
    }
}

static void part2(void)
{
    /* A. ch 7 (park release, chip 3 odd, diag config 1) UNREQUESTED in 'U':
     *    off-state code 1 → bit 3 → fault class 2 → mode 0, letter '2' →
     *    chip 3 mode byte 0x12 → 0x16; groups 15 (monitor 3: P1D78) / 16 */
    reset_model();
    boot();
    run_to(1u, 900u);
    CHECK(drv_job_tx(DRV_CYC)[3] == 0x8812u);
    m_diag[3][1] = 0x01u;
    run_to(901u, 960u);
    CHECK(fw(7) == 0x0008u);
    CHECK(CELL_RQ_MODE[7] == 0u && CELL_RQ_LETTER[7] == (uint8_t)'2');
    CHECK((CELL_RQ_FAULTMASK & 0x80u) != 0u);
    CHECK(drv_job_tx(DRV_CYC)[3] == 0x8816u);
    for (uint8_t ch = 0u; ch < 7u; ch++) {
        CHECK(fw(ch) == 0u && CELL_RQ_MODE[ch] == 3u);
    }
    CHECK(drv_od_group(15)->fail_n > 0u && drv_od_group(15)->monitor == 3u);
    CHECK(drv_od_group(16)->fail_n > 0u);
    for (uint8_t g = 1u; g <= 14u; g++) {
        CHECK(drv_od_group(g)->fail_n == 0u);
    }
    printf("part 2A: ch7 off-state code 1 -> word %04X, mode %u letter %02X, cyc[3] %04X, g15 mon %u\n",
           fw(7), CELL_RQ_MODE[7], CELL_RQ_LETTER[7], drv_job_tx(DRV_CYC)[3], drv_od_group(15)->monitor);
    /* ... the fault heals when the chip reports 0 again */
    m_diag[3][1] = 0u;
    run_to(961u, 990u);
    CHECK(fw(7) == 0u && CELL_RQ_MODE[7] == 1u && drv_job_tx(DRV_CYC)[3] == 0x8812u);

    /* B. ch 7 off-state code 2 (cfg 1) → bit 4 (0x10) → class 2, monitor 6 */
    reset_model();
    boot();
    run_to(1u, 900u);
    m_diag[3][1] = 0x02u;
    run_to(901u, 960u);
    CHECK(fw(7) == 0x0010u && CELL_RQ_MODE[7] == 0u);
    CHECK(drv_od_group(15)->monitor == 6u);
    printf("part 2B: ch7 off-state code 2 -> word %04X, g15 mon %u\n", fw(7), drv_od_group(15)->monitor);

    /* C. ch 2 (B, chip 1 even, diag config 2) DRIVEN at 850 mA: the
     *    energised-format flag 0x10 in the diag byte → bit 2 in the fault
     *    word; the channel stays driven (a requested channel: class 1),
     *    reported to groups 5 / 6 (monitor 2) */
    reset_model();
    boot();
    run_to(1u, 900u);
    m_diag[1][0] = 0x10u;
    run_to(901u, 960u);
    CHECK(fw(2) == 0x0004u);
    CHECK(CELL_RQ_MODE[2] == 3u && CELL_RQ_I[2] != 0u);
    CHECK(drv_od_group(5)->fail_n > 0u && drv_od_group(5)->monitor == 2u);
    CHECK(drv_od_group(6)->fail_n > 0u);
    for (uint8_t ch = 0u; ch < 8u; ch++) {
        if (ch != 2u) {
            CHECK(fw(ch) == 0u);
        }
    }
    printf("part 2C: ch2 driven, diag flag 0x10 -> word %04X, mode %u I %u, g5 mon %u\n",
           fw(2), CELL_RQ_MODE[2], CELL_RQ_I[2], drv_od_group(5)->monitor);

    /* D. chip 0 (ch 0 A / ch 1 D) answers the ID with a wrong ID byte: the
     *    status word loses bit 15 → bit 0 on both channels only */
    reset_model();
    boot();
    run_to(1u, 900u);
    m_id_lo[0] = 0x00u;
    run_to(901u, 960u);
    CHECK((fw(0) & 1u) != 0u && (fw(1) & 1u) != 0u);
    for (uint8_t ch = 2u; ch < 8u; ch++) {
        CHECK(fw(ch) == 0u);
    }
    CHECK(CELL_RQ_MODE[0] == 3u && CELL_RQ_MODE[1] == 3u);  /* requested: no law reaction */
    printf("part 2D: chip 0 ID lost -> words %04X %04X (modes %u %u)\n",
           fw(0), fw(1), CELL_RQ_MODE[0], CELL_RQ_MODE[1]);

    /* E. ch 5 (WK, chip 2 odd, diag config 2, driven 50 mA) with the
     *    energised-format flag 0x02 in its diag byte from inside the letter
     *    drop: on return to 'U' 0x89F5C starts the off-state test machine
     *    0x8E8E4 (0x8ECF0(ch, 0x10)); its test current (class 2: mode 2,
     *    '!' at 0 mA) ends with the chip still reporting code 2 → result
     *    '!' → class 4: the law switches THAT channel off (mode 0, letter
     *    '2'), fault word bit 6 (0x40, code 0x43: group 11 via 0x9B918) */
    reset_model();
    boot();
    run_to(1u, 1510u);                          /* inside the '4' window */
    m_diag[2][1] = 0x02u;
    run_to(1511u, 1990u);
    printf("part 2E: ch5 code 2 across the letter drop -> word %04X, 0x93A4C %02X 0x93A42 %u mode %u\n",
           fw(5), *(volatile uint8_t *)(uintptr_t)(0xFFF93A4Cu + 5u),
           *(volatile uint8_t *)(uintptr_t)(0xFFF93A42u + 5u), CELL_RQ_MODE[5]);
    CHECK(*(volatile uint8_t *)(uintptr_t)(0xFFF93A42u + 5u) == 4u);   /* class 4: off */
    CHECK(CELL_RQ_MODE[5] == 0u);
    for (uint8_t ch = 0u; ch < 8u; ch++) {
        if (ch != 5u) {
            CHECK(*(volatile uint8_t *)(uintptr_t)(0xFFF93A42u + ch) == 0u);
        }
    }
}

/* ── part 3: the witness-only fatal checks ─────────────────────────────── */
static void part3(void)
{
    uint32_t n0;
    reset_model();
    boot();
    run_to(1u, 900u);
    n0 = drv_od_fatal()->n;
    CHECK(n0 == 0u);
    /* 0x8493A: the law's 0x94ED4[1] corrupted between its write and the
     * channel tail (0x84392 recomputes the complement from the request) →
     * 0x221 counted at site 8 */
    CELL_RQ_I[1] = (uint16_t)(CELL_RQ_I[1] ^ 0x0001u);
    drv_od_ch_tail(1u, false);
    CHECK(drv_od_fatal()->n > n0);
    CHECK(drv_od_fatal()->code == 0x221u && drv_od_fatal()->site_n[8] > 0u && drv_od_fatal()->ch == 1u);
    printf("part 3a: corrupted complement -> would-be resets %u (last code %03X site %u ch %u)\n",
           (unsigned)drv_od_fatal()->n, drv_od_fatal()->code, drv_od_fatal()->site, drv_od_fatal()->ch);
    /* 0x84644: a setpoint as sent (0x90244) on an output in seq2 'D' two
     *    passes running, after the 30 ms grace of the seq2 change → 0x226 */
    n0 = drv_od_fatal()->site_n[6];
    CELL_SEQ2_STATE[0] = (uint8_t)'D';
    for (unsigned k = 0u; k < 10u; k++) {
        countdown();
        CELL_RQ_CUR_SENT[3] = 8500u;
        drv_letter_refresh();
        CELL_SEQ2_STATE[0] = (uint8_t)'D';
    }
    CHECK(drv_od_fatal()->site_n[6] > n0);
    printf("part 3b: output on in 'D' -> 0x84644 would-be resets %u (no reset: the program runs on)\n",
           (unsigned)(drv_od_fatal()->site_n[6] - n0));
}
#endif

int main(int argc, char **argv)
{
    FILE *golden = NULL;
    if (argc > 1) {
        golden = fopen(argv[1], "r");
        if (golden == NULL) {
            printf("cannot open %s\n", argv[1]);
            return 2;
        }
    }
    part1(stdout, golden);
#if !defined(OD_HEAD)
    if (golden != NULL) {
        part2();
        part3();
    }
#endif
    if (golden != NULL) {
        fclose(golden);
        printf("%s (%d failures)\n", fails == 0 ? "PASS" : "FAIL", fails);
        return fails == 0 ? 0 : 1;
    }
    return 0;
}
