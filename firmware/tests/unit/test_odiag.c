/* THE OUTPUT DIAGNOSIS (2026-11-17, bsw/drv/drv_odiag.c;
 * ~/tcu_sweep/output_diag_port.md).
 *
 * Part 1 — "at the bench's healthy replies the CONTENT of the wire and the
 * law is unchanged by the output-cadence port" (2026-11-18,
 * ~/tcu_sweep/output_cadence_port.md): the request producer (compile-time
 * inputs) → the request law → the bus-A cyclic / config / ID words are run
 * through a bench timeline (boot, the phase-0x10 and '3' full-on tests, 'U',
 * the park release on and off, a letter drop, a rail dip), with bus A
 * drained every ms, harvesting from a model of the bench's four healthy
 * driver chips (their r03v replies: config readbacks A29A / A221 / A29F /
 * A255, setpoint readbacks, ID BD 12, diag bytes 00, the empty fifth select
 * 0000). Per 5 ms tick the program prints "t hash" of THE CONTENT:
 *   - the last complete cyclic, config and ID burst on the wire (the diag
 *     job is not compared; it is counted), with the setpoint fields of the
 *     cyclic's E8 / F0 rows masked to their command byte;
 *   - every law cell, 0x90244 (the composed current, before the 0x9090A
 *     offset) and the records' non-setpoint fields; the producer's currents.
 * NOT compared, by design: WHEN a cyclic goes out (every F pass now, on
 * change + every 10 ms before) and the rounding offset 0x9090A (it steps
 * once per F pass now, once per send before) — so the setpoint low bits
 * differ by the phase; the new build checks instead that every staged
 * setpoint is within the 0x901BC head-check band of 0x90244 (s_rq_f238_n
 * stays 0). The golden file tests/unit/golden/odiag_content_head_ebf475a.txt
 * was produced by THIS program built against HEAD ebf475a (-DOD_OLD: the
 * refresh-on-change + 10 ms periodic). The diagnosis must end with no fault
 * word, no failed DSM report and no would-be reset.
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
#include "bsw/drv/drv_odiag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* THE FEEDBACK SHADOW'S LIVE-PATH IDENTITY (2026-11-18, app/fb_shadow.c;
 * ~/tcu_sweep/feedback_shadow_port.md): built twice — test_odiag_full
 * (no shadow) and test_odiag_fbs (-DTEST_ODIAG_FBS: the shadow's hook at the
 * 0x7CBA8 slot, its CMID00 ISR on the Timer F / D model, the model driven
 * from the law's own outputs) — and run with --full, which prints per output
 * tick an EXACT hash of the whole stock cell window 0xFFF80000..0xFFF96000
 * (the law, the diagnosis, the records, the countdowns), every bus-A word on
 * the wire with its µs, and the cadence witnesses, through parts 1-4. The
 * ctest fb_shadow_identity requires the two stdouts byte-identical. */
static bool s_full;
static unsigned s_full_n;
#if defined(TEST_ODIAG_FBS)
#include "app/fb_shadow.h"
#include "app/fb_cal.h"
#include "tests/unit/fb_model.h"
void app_out_head_hook(void);
static uint32_t s_fbs_low[10];
static uint8_t fbs_tf(uint8_t ch) { return FB_PINS[3u * (ch >> 1) + (ch & 1u)]; }
static void fbs_boot(void)
{
    fbm_reset();
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        fbm_config(fbs_tf(ch), (uint8_t)(((ch & 1u) == 0u) ? 1u : 2u));
        fbm_set_static(fbs_tf(ch), true);
        s_fbs_low[ch] = 0u;
    }
    fb_shadow_init();
    fb_shadow_start();
}
/* the chips' feedback as a function of the law's output: a driven channel
 * (mode 3 / 4 with a current) PWMs at ≈ I / 10 ‰ low (capped 990 ‰) */
static void fbs_ms(void)
{
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        const uint8_t m = CELL_RQ_MODE[ch];
        const uint16_t i = CELL_RQ_I[ch];
        uint32_t low = 0u;
        if ((m == 3u || m == 4u) && i != 0u) {
            uint32_t d = i / 10u;
            low = 4u * (d > 990u ? 990u : (d == 0u ? 1u : d));
        }
        if (low != s_fbs_low[ch]) {
            s_fbs_low[ch] = low;
            if (low == 0u) {
                fbm_set_static(fbs_tf(ch), true);
            } else {
                fbm_set_pwm(fbs_tf(ch), 4000u, low, 37u * ch);
            }
        }
    }
    fbm_advance_to(stub.frt_us);
}
#endif
#if defined(TEST_ODIAG_PA7)
/* THE PA7 SHADOW'S LIVE-PATH IDENTITY (2026-11-18, r04d): its hooks at
 * their firmware slots — the range part in the 10 ms task (mode 1), the
 * pin-cut part right after the law (sup_batch.c: 0x7CC1E, then 0x7CC32) —
 * reading this test's live cells by value; ctest pa7_shadow_identity
 * requires the stdout byte-identical with test_odiag's */
#include "app/pa7_shadow.h"
void app_pin_cut_hook(void);
#endif

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

/* part 4: ch 7's request is set by the 10 ms task only (main.c order) */
static bool s_p4;
static uint16_t s_p4_req7;

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
    drv_rq_request_set(7u, (uint16_t)(s_p4 ? s_p4_req7 :
                                     ((t >= 1000u && t < 1300u) ? 10000u : 0u)));
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
    /* NOT CELL_RQ_ROUND (0x9090A: its phase is the cadence) */
    for (uint8_t c = 0u; c < 5u; c++) {        /* the composed record fields, setpoints excluded */
        static const uint8_t OFS[] = { 0x07, 0x09, 0x0A, 0x0B, 0x10, 0x18 };
        for (unsigned k = 0u; k < sizeof OFS; k++) {
            h = fnv(h, CELL_REC_U8(c, OFS[k]));
        }
    }
    for (unsigned k = 0u; k < 7u; k++) {        /* the producer's currents */
        h = fnv(h, *(volatile uint16_t *)(uintptr_t)(0xFFF84B78u + 2u * k));
    }
    return h;
}

/* the wire CONTENT: the last complete cyclic / config / ID burst (the diag
 * job is not part of the comparison; it is counted). The E8 / F0 setpoint
 * fields carry the 0x9090A rounding phase: masked to the command byte. */
static unsigned s_diag_words;
static uint16_t s_last[4][45], s_cur[4][45];
static unsigned s_cur_n[4];
static int s_cur_job = -1;
static const uint8_t k_frames[4] = { 10u, 15u, 40u, 45u };
static uint64_t wire_hash(uint64_t h)
{
    for (unsigned k = 0u; k < stub_log_n; k++) {
        int j;
        if (stub_log[k].bus != 0u) {
            continue;
        }
        if (stub_log[k].job == DRV_DIAG) {
            s_diag_words++;
        }
        j = (int)stub_log[k].job - DRV_ID;
        if (j < 0 || j > 3) {
            continue;
        }
        if (j != s_cur_job) {
            s_cur_job = j;
            s_cur_n[j] = 0u;
        }
        s_cur[j][s_cur_n[j]++] = stub_log[k].word;
        if (s_cur_n[j] == k_frames[j]) {
            memcpy(s_last[j], s_cur[j], sizeof s_cur[j]);
            s_cur_n[j] = 0u;
            s_cur_job = -1;
        }
    }
    stub_log_n = 0u;
    for (unsigned j = 0u; j < 4u; j++) {
        if (j == 1u) {
            continue;                           /* the diag job */
        }
        for (unsigned i = 0u; i < k_frames[j]; i++) {
            uint16_t w = s_last[j][i];
            if (j == 3u && ((i >= 10u && i < 15u) || (i >= 20u && i < 25u))) {
                w = (uint16_t)(w & 0xF800u);    /* E8 / F0: the setpoint masked */
            }
            h = fnv(h, ((uint32_t)j << 16) | w);
        }
    }
    return h;
}

static void boot(void)
{
    stub.drv_reply_fn = chip_model;
    drv_od_host_seed_select(false);     /* harvest from zero, as the target */
    memset(s_last, 0, sizeof s_last);
    memset(s_cur_n, 0, sizeof s_cur_n);
    s_cur_job = -1;
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
#if defined(TEST_ODIAG_FBS)
    fbs_boot();
#endif
#if defined(TEST_ODIAG_PA7)
    pa7_shadow_init();
    pa7_hook_range_init();
#endif
}

/* one output tick: the stock's slots (sup_batch.c) — or, -DOD_OLD (the
 * golden's HEAD ebf475a build), r02w's refresh-on-change */
static void out_tick(void)
{
#if defined(OD_OLD)
    drv_letter_refresh();               /* after seq2 (sup_batch.c) */
    drain();
    drv_id_kick();                      /* the batch 0x8EE82 → 0x921A0 */
    drain();
#else
    drv_out_tick_head();                /* 0x8EE12 + 0x7CBA8 */
#if defined(TEST_ODIAG_FBS)
    app_out_head_hook();                /* the feedback shadow (sup_batch.c's slot) */
#if defined(TEST_ODIAG_FBS_NEG)
    /* NEGATIVE CONTROL (ctest fb_shadow_identity_negative): the shadow's
     * measured duty leaks into the live cell 0x9340C — the identity
     * comparison must catch it */
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        *(volatile uint16_t *)(uintptr_t)(0xFFF9340Cu + 2u * ch) = fb_shadow_state()->duty[ch];
    }
#endif
#endif
    drv_out_law();                      /* 0x7CC1E */
#if defined(TEST_ODIAG_PA7)
    app_pin_cut_hook();                 /* 0x7CC32 → 0x86B0C (the PA7 shadow, sup_batch.c's slot) */
#if defined(TEST_ODIAG_PA7_NEG)
    /* NEGATIVE CONTROL (ctest pa7_shadow_identity_negative): the shadow's
     * cells leak into the live ones — PA7 → 0xFFF90920, the C9 bit-7
     * request → 0xFFF9095A, its 0x9095B model → 0xFFF9095B (this test has no
     * range set up: the shadow sits in state 3 with PA7 = C9b7 = 0, so the
     * 0x0F of 0x9095B is the leak the hash must see) */
    *(volatile uint8_t *)(uintptr_t)0xFFF90920u = pa7_shadow_state()->pa7;
    *(volatile uint8_t *)(uintptr_t)0xFFF9095Au = pa7_shadow_state()->c9b7;
    *(volatile uint8_t *)(uintptr_t)0xFFF9095Bu = pa7_shadow_state()->s9095b;
#endif
#endif
    drv_f_stage();                      /* 0x8EE30: compose, cyclic + config queued */
    drv_id_kick();                      /* 0x8EE82 → 0x921A0 */
    drain();
    drv_out_report();                   /* 0x7ABC8 */
#endif
    if (s_full) {
        /* the exact live state after the tick: the whole cell window, every
         * bus-A word logged so far (with its µs), the cadence witnesses */
        uint64_t h = 0xCBF29CE484222325ull;
        const volatile uint8_t *p = (const volatile uint8_t *)(uintptr_t)0xFFF80000u;
        for (uint32_t a = 0u; a < 0x16000u; a++) {
            h = (h ^ p[a]) * 0x100000001B3ull;
        }
        for (unsigned k = 0u; k < stub_log_n; k++) {
            h = fnv(h, ((uint32_t)stub_log[k].bus << 24) | ((uint32_t)stub_log[k].job << 16) | stub_log[k].word);
            h = fnv(h, stub_log[k].t_us);
        }
#if !defined(OD_OLD)
        {
            const uint8_t *w = (const uint8_t *)drv_out_wit();
            for (unsigned k = 0u; k < sizeof(drv_out_wit_t); k++) {
                h = (h ^ w[k]) * 0x100000001B3ull;
            }
        }
#endif
        printf("F %u %u %016llx\n", s_full_n++, (unsigned)stub.frt_us, (unsigned long long)h);
    }
}

/* one millisecond of main.c (part 1: the mode-0 cadence, an output tick
 * every 5 ms; the old build adds its 10 ms periodic) */
static void step_ms(uint32_t t)
{
    stub.frt_us += 1000u;
    stub.ms++;
#if defined(TEST_ODIAG_FBS)
    fbs_ms();
#endif
    sup_state(t);
    drv_od_diag_poll(stub.frt_us);      /* task_1ms */
    drain();
    if (t % 5u == 0u) {                 /* the 5 ms supervisor tick */
        countdown();
        out_tick();
    }
    if (t % 10u == 5u) {                /* the 10 ms task */
        rq_prod_task_10ms(1u);
#if defined(TEST_ODIAG_PA7)
        pa7_hook_range(1u);
#endif
#if defined(OD_OLD)
        drv_cyclic_periodic();
        drain();
#endif
    }
}

static uint32_t s_fail_at_u[21];

static int part1(FILE *out, FILE *golden)
{
    char line[128];
    unsigned mismatches = 0u;
    boot();
    for (uint32_t t = 1u; t <= 2600u; t++) {
        step_ms(t);
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
        printf("part 1: %u / %u ticks of wire and law CONTENT equal to HEAD ebf475a's; diag words on the wire %u\n",
               520u - mismatches, 520u, s_diag_words);
    }
#if !defined(OD_OLD)
    if (golden != NULL) {
        const drv_out_wit_t *ow = drv_out_wit();
        /* the cadence: one law pass and one compose per F pass, every job
         * queued was sent, nothing dropped or missed; every staged setpoint
         * within the head-check band of the composed current */
        printf("part 1: F passes %u, law passes %u, cyclics %u, configs %u, IDs %u, diags %u; "
               "dropped %u missed %u, ring high-water %u, f238 %u\n",
               (unsigned)ow->f_n, (unsigned)ow->law_n, (unsigned)ow->launched[3],
               (unsigned)ow->launched[2], (unsigned)ow->launched[0], (unsigned)ow->launched[1],
               (unsigned)(ow->dropped[0] + ow->dropped[1] + ow->dropped[2] + ow->dropped[3]),
               (unsigned)(ow->missed[0] + ow->missed[1] + ow->missed[2] + ow->missed[3]),
               ow->ring_hw, (unsigned)drv_rq_f238_n());
        CHECK(ow->f_n == 520u && ow->law_n == 520u && ow->law_f_bad == 0u && ow->compose_f_bad == 0u);
        CHECK(ow->launched[3] == 520u && ow->launched[2] == 520u && ow->launched[0] == 520u);
        CHECK(ow->done[3] == 520u && ow->done[2] == 520u && ow->done[0] == 520u);
        for (unsigned j = 0u; j < 4u; j++) {
            CHECK(ow->dropped[j] == 0u && ow->missed[j] == 0u && ow->err[j] == 0u);
        }
        CHECK(drv_rq_f238_n() == 0u);
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
        CHECK(drv_od_diag_drop_n() == 0u);
    }
#endif
    return 0;
}

#if !defined(OD_OLD)
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
        drv_out_law();                  /* 0x84644 reads the cell this pass */
        drv_f_stage();
        drain();
        CELL_SEQ2_STATE[0] = (uint8_t)'D';
    }
    CHECK(drv_od_fatal()->site_n[6] > n0);
    printf("part 3b: output on in 'D' -> 0x84644 would-be resets %u (no reset: the program runs on)\n",
           (unsigned)(drv_od_fatal()->site_n[6] - n0));
}

/* ── part 4: the park release cycled in MAIN.C's ORDER (2026-11-18,
 * ~/tcu_sweep/ch7_fault_bit4_decode.md). On the target the park demand
 * changes in task_10ms (phase 1: park_tick) one ms BEFORE the batch
 * (phase 2). r03x's 10 ms rebuild (drv_rq_law_between, inside sol_tick)
 * saw the change first and ran the law without 0x897AC — the ch 7 bit-2
 * artefact. Since the output-cadence port there is no rebuild: in OS mode 1
 * the ONE law pass of the tick is the batch's 10 ms part (phase 2 of every
 * second 5 ms pass); the 5 ms part between carries no law and no bus A. The
 * r03x bench sequence (D 2 s, P, R 2 s, P) on a healthy chip must leave no
 * fault on ch 7 — with the r03y snapshot stand-in AND (part 4b) with the
 * pre-r03y live-cell stand-in — while a real chip flag 0x10 on the driven
 * ch 7 must still set bit 2 (monitor 2). ── */
static void step_ms_main(uint32_t t, uint32_t t_sup, bool trace)
{
    stub.frt_us += 1000u;
    stub.ms++;
#if defined(TEST_ODIAG_FBS)
    fbs_ms();
#endif
    sup_state(t_sup);
    drv_od_diag_poll(stub.frt_us);      /* task_1ms */
    drain();
    if (t % 10u == 1u) {                /* task_10ms, phase 1 */
        drv_rq_request_set(7u, s_p4_req7);      /* park_tick: the release demand */
        rq_prod_task_10ms(1u);
#if defined(TEST_ODIAG_PA7)
        pa7_hook_range(1u);
#endif
        /* (sol_tick: no bus A since the output-cadence port) */
    }
    if (t % 10u == 2u) {                /* task_sup_batch, phase 2, the 10 ms part (mode 1) */
        countdown();
        countdown();                    /* the 5 ms countdown ran on both 5 ms passes */
        out_tick();
        if (trace) {
            printf("    t=%4u req7 %5u | law mode %u I %5u | 93B0C %02X 93A2E %02X | 94930 %04X 93004 %04X 94944 %04X\n",
                   t, (unsigned)s_p4_req7, CELL_RQ_MODE[7], CELL_RQ_I[7],
                   *(volatile uint8_t *)(uintptr_t)(0xFFF93B0Cu + 7u),
                   *(volatile uint8_t *)(uintptr_t)(0xFFF93A2Eu + 7u),
                   *(volatile uint16_t *)(uintptr_t)(0xFFF94930u + 14u), fw(7),
                   *(volatile uint16_t *)(uintptr_t)(0xFFF94944u + 14u));
        }
    }
}

static void part4_run(uint32_t t_end, const uint32_t *edges, const uint16_t *vals, unsigned n_edges,
                      bool trace)
{
    unsigned e = 0u;
    for (uint32_t t = 1u; t <= t_end; t++) {
        bool near = false;
        while (e < n_edges && t >= edges[e]) {
            s_p4_req7 = vals[e];
            e++;
        }
        for (unsigned k = 0u; k < n_edges; k++) {
            if (t + 5u >= edges[k] && t <= edges[k] + 25u) {
                near = true;
            }
        }
        step_ms_main(t, t < 900u ? t : 900u, trace && near);
    }
    stub_log_n = 0u;
}

static void part4(void)
{
    /* the r03x bench sequence: D (release) 2 s, P, R 2 s, P — first with the
     * r03y snapshot stand-in, then (4b) with the pre-r03y LIVE-cell stand-in */
    static const uint32_t EDGES[] = { 1001u, 3001u, 3501u, 5501u };
    static const uint16_t VALS[]  = { 10000u, 0u, 10000u, 0u };
    for (unsigned live = 0u; live < 2u; live++) {
        reset_model();
        s_p4 = true;
        s_p4_req7 = 0u;
        drv_od_host_fb_live(live != 0u);
        boot();
        printf("part 4%s: ch 7 per diagnostic pass around each request edge (main.c order, mode-1 cadence)\n",
               live ? "b (live-cell stand-in)" : "");
        part4_run(6000u, EDGES, VALS, 4u, live == 0u);
        printf("part 4%s: healthy chip after two release cycles: 0x93004[7] %04X 0x94930[7] %04X mode %u, events %u, g15 fail %u g16 fail %u\n",
               live ? "b" : "", fw(7), *(volatile uint16_t *)(uintptr_t)(0xFFF94930u + 14u), CELL_RQ_MODE[7],
               (unsigned)drv_od_event_n(7u), (unsigned)drv_od_group(15)->fail_n,
               (unsigned)drv_od_group(16)->fail_n);
        CHECK(fw(7) == 0u);
        CHECK(*(volatile uint16_t *)(uintptr_t)(0xFFF94930u + 14u) == 0u);
        CHECK(CELL_RQ_MODE[7] == 1u);
        CHECK(drv_od_group(15)->fail_n == 0u && drv_od_group(16)->fail_n == 0u);
        for (uint8_t ch = 0u; ch < 10u; ch++) {
            CHECK(fw(ch) == 0u);
        }
        CHECK(drv_out_wit()->law_f_bad == 0u);
    }
    drv_od_host_fb_live(false);

    /* the same with a REAL fault: the chip flags 0x10 on ch 7 (chip 3, odd
     * → the 0x1E row) while the release is driven → bit 2, groups 15 / 16 */
    {
        static const uint32_t E2[] = { 1001u };
        static const uint16_t V2[] = { 10000u };
        reset_model();
        s_p4_req7 = 0u;
        boot();
        part4_run(1300u, E2, V2, 1u, false);
        CHECK(fw(7) == 0u && CELL_RQ_MODE[7] == 3u);
        m_diag[3][1] = 0x10u;
        for (uint32_t t = 1301u; t <= 1400u; t++) {
            step_ms_main(t, 900u, false);
        }
        printf("part 4: injected flag 0x10 on driven ch 7 -> 0x93004[7] %04X, mode %u, g16 fail %u mon %u\n",
               fw(7), CELL_RQ_MODE[7], (unsigned)drv_od_group(16)->fail_n, drv_od_group(16)->monitor);
        CHECK((fw(7) & 0x0004u) != 0u);
        CHECK(CELL_RQ_MODE[7] == 3u);               /* requested: still driven */
        CHECK(drv_od_group(15)->fail_n > 0u || drv_od_group(16)->fail_n > 0u);
        for (uint8_t ch = 0u; ch < 7u; ch++) {
            CHECK(fw(ch) == 0u);
        }
    }
    s_p4 = false;
}
#endif

int main(int argc, char **argv)
{
    FILE *golden = NULL;
    if (argc > 1 && strcmp(argv[1], "--full") == 0) {
        s_full = true;
        part1(stdout, NULL);
#if !defined(OD_OLD)
        part2();
        part3();
        part4();
#endif
#if defined(TEST_ODIAG_FBS)
        {
            const volatile fbs_t *s = fb_shadow_state();
            unsigned cls_ok = 0u;
            for (uint8_t ch = 0u; ch < 10u; ch++) {
                cls_ok += (s->cmp[ch].agree_n != 0u) ? 1u : 0u;
            }
            fprintf(stderr, "fbs isr %u windows %u/%u ticks %u r_upd %u pair_bad %u channels-compared %u"
                    " recorder %u/%u\n",
                    (unsigned)s->isr_n, (unsigned)s->win_n[0], (unsigned)s->win_n[1],
                    (unsigned)s->tick_n, (unsigned)s->r_upd_n, (unsigned)s->pair_bad_n, cls_ok,
                    (unsigned)fb_shadow_rec()->n_rec, (unsigned)fb_shadow_rec()->n_dis);
        }
#endif
#if defined(TEST_ODIAG_PA7)
        fprintf(stderr, "pa7 rng %u pin %u law %u hi %u c9b7 %u\n",
                (unsigned)pa7_shadow_state()->rng_n, (unsigned)pa7_shadow_state()->pin_n,
                (unsigned)pa7_shadow_state()->law_n, (unsigned)pa7_shadow_state()->hi_n,
                (unsigned)pa7_shadow_state()->c9b7_n);
#endif
        printf("%s (%d failures)\n", fails == 0 ? "PASS" : "FAIL", fails);
        return fails == 0 ? 0 : 1;
    }
    if (argc > 1) {
        golden = fopen(argv[1], "r");
        if (golden == NULL) {
            printf("cannot open %s\n", argv[1]);
            return 2;
        }
    }
    part1(stdout, golden);
#if !defined(OD_OLD)
    if (golden != NULL) {
        part2();
        part3();
        part4();
    }
#endif
    if (golden != NULL) {
        fclose(golden);
        printf("%s (%d failures)\n", fails == 0 ? "PASS" : "FAIL", fails);
        return fails == 0 ? 0 : 1;
    }
    return 0;
}
