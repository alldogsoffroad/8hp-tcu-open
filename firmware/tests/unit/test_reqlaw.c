/* Unit test: the stock's solenoid request law (drv_reqlaw.c — 0x83DB0 →
 * 0x89540 → 0x901A0 → 0x924B0 / 0x92344). HOST ONLY: nonzero requests
 * exist only here (drv_reqlaw.h, the safety frame).
 *
 * The main validation is the stock driving RAM dump
 * stockram_driving_20261104.bin (1.773 A, P, engine off, engaged):
 *   requests 0xFFF94EE8 = 01F4 2134 2134 2134 2134 01F4 01F4 0000 0000 0000
 *   rail node 0x94ED0 = 0x326C (12 908 mV), seq2[0] 'U', 0x94114/5 = 0
 *   corrections 0x9337C = 0000 0004 0004 0002 0006 0002 0002 0 0 0
 *   0x9090A = 01 (after this pass's step), 0x9091B = 00
 * and must reproduce
 *   modes 0x94FD8 = 03 ×7 01 00 00, 0x94FA0 = 01 ×7 00 00 00
 *   letters 0x90812 / 0x9081C = '!' ×7 00 '2' '2'
 *   currents 0x90230 = 0x94F58 = 0x90244 = 01F4 2138 2138 2136 213A 01F6 01F6 0 0 0
 *   records 0x91108 + 0x1C·c: +7 1A 1A 1A 12 15, +9..B 9F 21 55,
 *     +0xC/+0x14 0053/0589, 0589/0589, 0589/0053, 0053/0000, 0000/0000
 *   cyclic 0xFFF88518 = 881A 881A 881A 8812 8815 | A000 ×5 | E853 ED89 ED8A
 *     E853 E800 | A600 ×5 | F589 F589 F053 F000 F000 | 6000/C000/6600/D800 ×5
 *   config 0xFFF88194 = 8E9F ×5 | 9021 ×5 | 9655 ×5 | 881A 881A 881A 8812 8815
 *     | 2800/3000/2E00/3600 ×5
 * The dump's cyclic word 12 (chip 2's E8 row, channel 4) reads ED8A where
 * its own record +0xC and 0x90230[4] = 0x213A give 0x589: the staging was
 * composed on an earlier pass than the record (a channel-4 correction of 7
 * reproduces it) — asserted below in both forms.
 */
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_reqlaw.h"
#include "bsw/drv/drv_cells.h"
#include "bsw/sup/sup_cells.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

extern volatile uint32_t s_drv_refresh_n;   /* drv_jobs.c witness */

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static const uint16_t DUMP_REQ[10]  = { 0x01F4, 0x2134, 0x2134, 0x2134, 0x2134, 0x01F4, 0x01F4, 0, 0, 0 };
static const int16_t  DUMP_CORR[10] = { 0, 4, 4, 2, 6, 2, 2, 0, 0, 0 };
static const uint16_t DUMP_CUR[10]  = { 0x01F4, 0x2138, 0x2138, 0x2136, 0x213A, 0x01F6, 0x01F6, 0, 0, 0 };
static const uint16_t DUMP_CYC[45] = {
    0x881A, 0x881A, 0x881A, 0x8812, 0x8815,
    0xA000, 0xA000, 0xA000, 0xA000, 0xA000,
    0xE853, 0xED89, 0xED8A, 0xE853, 0xE800,
    0xA600, 0xA600, 0xA600, 0xA600, 0xA600,
    0xF589, 0xF589, 0xF053, 0xF000, 0xF000,
    0x6000, 0x6000, 0x6000, 0x6000, 0x6000,
    0xC000, 0xC000, 0xC000, 0xC000, 0xC000,
    0x6600, 0x6600, 0x6600, 0x6600, 0x6600,
    0xD800, 0xD800, 0xD800, 0xD800, 0xD800,
};
static const uint16_t DUMP_CFG[40] = {
    0x8E9F, 0x8E9F, 0x8E9F, 0x8E9F, 0x8E9F, 0x9021, 0x9021, 0x9021, 0x9021, 0x9021,
    0x9655, 0x9655, 0x9655, 0x9655, 0x9655, 0x881A, 0x881A, 0x881A, 0x8812, 0x8815,
    0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000,
    0x2E00, 0x2E00, 0x2E00, 0x2E00, 0x2E00, 0x3600, 0x3600, 0x3600, 0x3600, 0x3600,
};

static void reset(void)
{
    drv_jobs_init();                    /* → drv_rq_init */
    CELL_SEQ2_STATE[0] = 0x11u;         /* sup_ladder_init's value */
    CELL_LAD_94114 = 0u;
    CELL_DIAG_94115 = 0u;
    CELL_J2_STAGED = 0x9000u;
    CELL_LAD_REQ[0] = 0u;
    CELL_LAD_REQ[1] = 0u;
    CELL_OUT_MV[0] = 13360u;            /* r03a: rail node at supply */
}

static void set_req(const uint16_t *r)
{
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        drv_rq_request_set(ch, r[ch]);
    }
}

static unsigned cyc_matches(const uint16_t *want)
{
    const uint16_t *cy = drv_job_tx(DRV_CYC);
    unsigned n = 0u;
    for (unsigned i = 0u; i < 45u; i++) {
        n += cy[i] == want[i];
    }
    return n;
}

static void drain(void)
{
    unsigned guard = 200u;
    while (stub_rspi_busy(0u) && guard-- != 0u) {
        stub_rspi_isr(0u);
    }
}

/* the zero-request content: 9 commands, per chip mode byte m[c], E/F rows */
static bool zero_content(const uint16_t m[5], uint16_t sp)
{
    const uint16_t *cy = drv_job_tx(DRV_CYC);
    const uint16_t *cf = drv_job_tx(DRV_CFG);
    bool ok = true;
    for (unsigned c = 0u; c < 5u; c++) {
        const bool full = m[c] == 0x881Fu;
        ok = ok && cy[c] == m[c] && cy[5u + c] == 0xA000u && cy[15u + c] == 0xA600u;
        ok = ok && cy[10u + c] == (uint16_t)(0xE800u | (full ? sp : 0u));
        ok = ok && cy[20u + c] == (uint16_t)(0xF000u | (full ? sp : 0u));
        ok = ok && cy[25u + c] == 0x6000u && cy[30u + c] == 0xC000u &&
             cy[35u + c] == 0x6600u && cy[40u + c] == 0xD800u;
        ok = ok && cf[c] == 0x8E9Fu && cf[5u + c] == 0x9021u && cf[10u + c] == 0x9655u &&
             cf[15u + c] == m[c];
    }
    return ok;
}

/* r03a's zero-request letter law (drv_jobs.c r02w drv_letter_code +
 * drv_cyclic_build), verbatim in substance — the reference for "no wire
 * change at zero requests" */
static uint8_t r03a_code(uint8_t ch)
{
    const uint8_t s = CELL_SEQ2_STATE[0];
    if (ch >= 8u) {
        return 1u;
    }
    if (s == 0x11u || s == 0x16u || s == (uint8_t)'3') {
        const bool full = ((CELL_DIAG_94115 & 1u) != 0u) ||
                          ((CELL_LAD_94114 & 1u) != 0u && (s == 0x11u || s == 0x16u));
        const bool quiet = CELL_J2_STAGED == 0x9000u &&
                           CELL_LAD_REQ[0] == 0u && CELL_LAD_REQ[1] == 0u;
        return (full && quiet) ? 3u : 1u;
    }
    return 0u;
}

static bool r03a_equal(void)
{
    static const uint16_t RD[4] = { 0x6000u, 0xC000u, 0x6600u, 0xD800u };
    const uint16_t *cy = drv_job_tx(DRV_CYC);
    const uint16_t *cf = drv_job_tx(DRV_CFG);
    bool ok = true;
    for (uint8_t chip = 0u; chip < 5u; chip++) {
        const uint8_t c0 = r03a_code((uint8_t)(chip * 2u));
        const uint8_t c1 = r03a_code((uint8_t)(chip * 2u + 1u));
        const uint16_t m = (uint16_t)(0x8810u | c0 | (uint16_t)(c1 << 2));
        ok = ok && cy[chip] == m && cf[15u + chip] == m;
        ok = ok && cy[5u + chip] == 0xA000u && cy[15u + chip] == 0xA600u;
        ok = ok && cy[10u + chip] == (uint16_t)(0xE800u | (c0 == 3u ? 0x7FFu : 0u));
        ok = ok && cy[20u + chip] == (uint16_t)(0xF000u | (c1 == 3u ? 0x7FFu : 0u));
        for (unsigned k = 0u; k < 4u; k++) {
            ok = ok && cy[25u + 5u * k + chip] == RD[k];
        }
        ok = ok && cf[chip] == 0x8E9Fu && cf[5u + chip] == 0x9021u && cf[10u + chip] == 0x9655u &&
             cf[20u + chip] == 0x2800u && cf[25u + chip] == 0x3000u &&
             cf[30u + chip] == 0x2E00u && cf[35u + chip] == 0x3600u;
    }
    return ok;
}

int main(void)
{
    static const uint16_t PRE[5]  = { 0x8815, 0x8815, 0x8815, 0x8815, 0x8815 };
    static const uint16_t POST[5] = { 0x8810, 0x8810, 0x8810, 0x8810, 0x8815 };
    static const uint16_t FULL[5] = { 0x881F, 0x881F, 0x881F, 0x881F, 0x8815 };

    /* ── 1. the driving dump ─────────────────────────────────────────── */
    reset();
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    CELL_OUT_MV[0] = 0x326Cu;           /* the dump's own rail node */
    set_req(DUMP_REQ);
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        drv_rq_corr_inject(ch, DUMP_CORR[ch]);
    }
    CHECK(CELL_RQ_ROUND == 0u);         /* cal 0x1918BB */
    drv_cyclic_build(0, 8u);            /* 0x9090A 0 → 4 */
    CHECK(CELL_RQ_ROUND == 4u);
    drv_cyclic_build(0, 8u);            /* 4 → 1: the dump's value */
    CHECK(CELL_RQ_ROUND == 1u);
    for (uint8_t ch = 0u; ch < 10u; ch++) {
        CHECK(CELL_RQ_I[ch] == DUMP_REQ[ch]);
        CHECK(CELL_RQ_MODE[ch] == (ch < 7u ? 3u : (ch == 7u ? 1u : 0u)));
        CHECK(CELL_RQ_ACTIVE[ch] == (ch < 7u ? 1u : 0u));
        CHECK(CELL_RQ_LETTER[ch] == (ch < 7u ? (uint8_t)'!' : (ch == 7u ? 0u : (uint8_t)'2')));
        CHECK(CELL_RQ_LET_APPLIED[ch] == CELL_RQ_LETTER[ch]);
        CHECK(CELL_RQ_CUR[ch] == DUMP_CUR[ch]);
        CHECK(CELL_RQ_I2[ch] == DUMP_CUR[ch]);
        CHECK(CELL_RQ_CUR_SENT[ch] == DUMP_CUR[ch]);
        CHECK(CELL_RQ_CORR[ch] == DUMP_CORR[ch]);
    }
    CHECK(CELL_RQ_UVFLAGS == 0u && CELL_RQ_FAULTMASK == 0u && CELL_RQ_FULLMASK == 0u);
    {
        static const uint8_t M[5] = { 0x1A, 0x1A, 0x1A, 0x12, 0x15 };
        static const uint16_t SP0[5] = { 0x053, 0x589, 0x589, 0x053, 0 };
        static const uint16_t SP1[5] = { 0x589, 0x589, 0x053, 0, 0 };
        for (uint8_t c = 0u; c < 5u; c++) {
            CHECK(CELL_REC_U8(c, REC_MODE) == M[c]);
            CHECK(CELL_REC_U8(c, REC_CFG8E) == 0x9Fu && CELL_REC_U8(c, REC_CFG90) == 0x21u &&
                  CELL_REC_U8(c, REC_CFG96) == 0x55u);
            CHECK(CELL_REC_U16(c, REC_SP0) == SP0[c] && CELL_REC_U16(c, REC_SP1) == SP1[c]);
            CHECK(CELL_REC_U8(c, REC_DITH0) == 0u && CELL_REC_U8(c, REC_DITH1) == 0u);
        }
    }
    /* the cyclic: 44 of 45 words; word 12 is the dump's own skew */
    CHECK(cyc_matches(DUMP_CYC) == 44u);
    CHECK(drv_job_tx(DRV_CYC)[12] == 0xED89u);
    {
        const uint16_t *cf = drv_job_tx(DRV_CFG);
        for (unsigned i = 0u; i < 40u; i++) {
            CHECK(cf[i] == DUMP_CFG[i]);
        }
    }
    CHECK(drv_rq_f238_n() == 0u);       /* staged x6 within 15 of 0x90244 */
    /* the same pass with a channel-4 correction of 7: the whole cyclic */
    drv_rq_corr_inject(4u, 7);
    CELL_RQ_ROUND = 4u;                 /* so this pass steps to 1 again */
    drv_cyclic_build(0, 8u);
    CHECK(CELL_RQ_ROUND == 1u && CELL_RQ_CUR[4] == 0x213Bu);
    CHECK(cyc_matches(DUMP_CYC) == 45u);
    CHECK(drv_rq_f238_n() == 0u);
    /* the rounding offset alternates 1 ↔ 4 (stock 0x902A8); 0x213B + 4 →
     * 0x58A, 0x01F4 + 4 → 0x54 */
    drv_cyclic_build(0, 8u);
    CHECK(CELL_RQ_ROUND == 4u && drv_job_tx(DRV_CYC)[10] == 0xE854u);
    drv_cyclic_build(0, 8u);
    CHECK(CELL_RQ_ROUND == 1u && drv_job_tx(DRV_CYC)[10] == 0xE853u);

    /* ── 2. zero requests in every seq2 state ─────────────────────────── */
    {
        static const uint8_t S[] = { 0x00, 0x11, 0x16, 0x22, 0x33, '3', 'U', '4', '8', '?', 'D' };
        reset();
        for (unsigned k = 0u; k < sizeof S; k++) {
            const bool pre = S[k] == 0x11u || S[k] == 0x16u || S[k] == (uint8_t)'3';
            CELL_SEQ2_STATE[0] = S[k];
            drv_cyclic_build(0, 8u);
            CHECK(zero_content(pre ? PRE : POST, 0u));
            for (uint8_t ch = 0u; ch < 10u; ch++) {
                CHECK(CELL_RQ_CUR[ch] == 0u && CELL_RQ_ACTIVE[ch] == 0u);
            }
        }
    }
    /* the full-on tests (ladder phase 0x10 / the '3' diagnosis) */
    reset();
    CELL_SEQ2_STATE[0] = 0x11u; CELL_LAD_94114 = 1u;
    drv_cyclic_build(0, 8u);
    CHECK(zero_content(FULL, 0x7FFu));
    CHECK(CELL_RQ_FULLMASK == 0x00FFu);   /* ch 0..7 (cal 0x191550 = 0x3FF, but 8/9 are not driven) */
    CELL_SEQ2_STATE[0] = 0x16u;
    drv_cyclic_build(0, 8u);
    CHECK(zero_content(FULL, 0x7FFu));
    CELL_SEQ2_STATE[0] = (uint8_t)'3';    /* 0x94114 needs 0x11 / 0x16 */
    drv_cyclic_build(0, 8u);
    CHECK(zero_content(PRE, 0u));
    CELL_DIAG_94115 = 1u;                 /* the '3' diagnosis */
    drv_cyclic_build(0, 8u);
    CHECK(zero_content(FULL, 0x7FFu));
    CELL_SEQ2_STATE[0] = (uint8_t)'U';    /* 0x94115 outside a test state: ignored */
    drv_cyclic_build(0, 8u);
    CHECK(zero_content(POST, 0u));
    /* the interlock (ours): no full-on with the rail word or a request up */
    CELL_SEQ2_STATE[0] = 0x11u; CELL_LAD_94114 = 1u; CELL_DIAG_94115 = 0u;
    {
        const uint32_t n0 = drv_rq_ilock_n();
        CELL_J2_STAGED = 0x9020u;
        drv_cyclic_build(0, 8u);
        CHECK(zero_content(PRE, 0u));
        CHECK(drv_rq_ilock_n() == n0 + 8u);   /* ch 0..7 refused */
        CELL_J2_STAGED = 0x9000u; CELL_LAD_REQ[1] = 1u;
        drv_cyclic_build(0, 8u);
        CHECK(zero_content(PRE, 0u));
        CELL_LAD_REQ[1] = 0u;
        drv_cyclic_build(0, 8u);
        CHECK(zero_content(FULL, 0x7FFu));
        CHECK(drv_rq_cguard_n() == 0u);       /* the compose guard never had to act */
    }
    /* the compose-time guard: a 'C' letter left from a quiet tick is not
     * staged once the rail word is up (law not re-run) */
    CELL_J2_STAGED = 0x9020u;
    drv_rq_compose();
    CHECK(drv_rq_cguard_n() == 8u);
    CELL_J2_STAGED = 0x9000u;
    CELL_LAD_94114 = 0u;

    /* ── 2b. no wire change vs r03a at zero requests: every seq2 byte, every
     *        0x94114 / 0x94115 low pair, J2 0x9000 / 0x9020, both request
     *        cells, three rail values — 45 cyclic + 40 config words each ── */
    {
        static const uint16_t RAIL[3] = { 0u, 7399u, 13360u };
        unsigned n = 0u, bad = 0u;
        reset();
        for (unsigned sq = 0u; sq < 256u; sq++) {
            for (unsigned t = 0u; t < 16u; t++) {
                for (unsigned q = 0u; q < 8u; q++) {
                    CELL_SEQ2_STATE[0] = (uint8_t)sq;
                    CELL_LAD_94114 = (uint8_t)(t & 3u);
                    CELL_DIAG_94115 = (uint8_t)(t >> 2);
                    CELL_J2_STAGED = (q & 1u) != 0u ? 0x9020u : 0x9000u;
                    CELL_LAD_REQ[0] = (uint8_t)((q >> 1) & 1u);
                    CELL_LAD_REQ[1] = (uint8_t)((q >> 2) & 1u);
                    CELL_OUT_MV[0] = RAIL[(sq + t + q) % 3u];
                    drv_cyclic_build(0, 8u);
                    n++;
                    bad += r03a_equal() ? 0u : 1u;
                }
            }
        }
        CHECK(n == 32768u && bad == 0u);
        CHECK(drv_rq_f238_n() == 0u);
        reset();
    }

    /* ── 3. the 'U' gate: requests ignored before 'U' ─────────────────── */
    reset();
    set_req(DUMP_REQ);
    {
        static const uint8_t S[] = { 0x00, 0x11, 0x16, 0x22, 0x33, '3', '4' };
        for (unsigned k = 0u; k < sizeof S; k++) {
            const bool pre = S[k] == 0x11u || S[k] == 0x16u || S[k] == (uint8_t)'3';
            CELL_SEQ2_STATE[0] = S[k];
            drv_cyclic_build(0, 8u);
            CHECK(zero_content(pre ? PRE : POST, 0u));
            CHECK(CELL_RQ_ACTIVE[1] == 1u);   /* 0x94FA0 follows the request regardless */
            CHECK(CELL_RQ_I[1] == 0u && CELL_RQ_CUR[1] == 0u);
        }
    }
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    drv_cyclic_build(0, 8u);
    CHECK(drv_job_tx(DRV_CYC)[0] == 0x881Au && CELL_RQ_CUR[1] == 0x2134u);

    /* ── 4. the rail-undervoltage law (7400 mV, release 7900, 4000 split) */
    reset();
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    {
        uint16_t r[10] = { 0 };
        r[1] = 2000u;                       /* <= 4000: zeroed under 7400 mV */
        r[2] = 8500u;                       /* >  4000: passes, clears its bit */
        set_req(r);
    }
    CELL_OUT_MV[0] = 7399u;
    drv_cyclic_build(0, 8u);
    CHECK(CELL_RQ_UVFLAGS == 0x0002u);
    CHECK(CELL_RQ_MODE[1] == 0u && CELL_RQ_I[1] == 0u && CELL_RQ_LETTER[1] == (uint8_t)'2');
    CHECK(CELL_RQ_MODE[2] == 3u && CELL_RQ_I[2] == 8500u);
    CELL_OUT_MV[0] = 7600u;                 /* above 7400, below the release: held */
    drv_cyclic_build(0, 8u);
    CHECK(CELL_RQ_UVFLAGS == 0x0002u && CELL_RQ_I[1] == 0u);
    CELL_OUT_MV[0] = 7899u;
    drv_cyclic_build(0, 8u);
    CHECK(CELL_RQ_UVFLAGS == 0x0002u && CELL_RQ_I[1] == 0u);
    CELL_OUT_MV[0] = 7900u;                 /* 7400 + 500: released */
    drv_cyclic_build(0, 8u);
    CHECK(CELL_RQ_UVFLAGS == 0u && CELL_RQ_I[1] == 2000u && CELL_RQ_MODE[1] == 3u);
    CELL_OUT_MV[0] = 7400u;                 /* at the threshold, no flag: passes */
    drv_cyclic_build(0, 8u);
    CHECK(CELL_RQ_UVFLAGS == 0u && CELL_RQ_I[1] == 2000u);
    CELL_OUT_MV[0] = 0u;                    /* rail open: zeroed again */
    drv_cyclic_build(0, 8u);
    CHECK(CELL_RQ_UVFLAGS == 0x0002u && CELL_RQ_I[1] == 0u && CELL_RQ_I[2] == 8500u);

    /* ── 5. the minimum current (500 = 50 mA) and the clamp ──────────── */
    reset();
    CELL_SEQ2_STATE[0] = (uint8_t)'U';
    {
        uint16_t r[10] = { 0 };
        r[0] = 499u; r[1] = 500u; r[2] = 1u; r[3] = 0x3000u;
        r[8] = 8500u; r[9] = 8500u;         /* chip 4 channels: not driven (0x19154C) */
        set_req(r);
    }
    drv_cyclic_build(0, 8u);
    CHECK(CELL_RQ_I[0] == 0u && CELL_RQ_MODE[0] == 0u && CELL_RQ_LETTER[0] == (uint8_t)'2');
    CHECK(CELL_RQ_I[1] == 500u && CELL_RQ_MODE[1] == 3u && CELL_RQ_LETTER[1] == (uint8_t)'!');
    CHECK(CELL_RQ_I[2] == 0u && CELL_RQ_MODE[2] == 0u);
    CHECK(CELL_RQ_I[3] == 0x2FFAu && CELL_RQ_CUR[3] == 0x2FFAu);
    CHECK(CELL_RQ_I[8] == 0u && CELL_RQ_LETTER[8] == (uint8_t)'2' && CELL_RQ_ACTIVE[8] == 0u);
    {
        const uint16_t *cy = drv_job_tx(DRV_CYC);
        CHECK(cy[0] == 0x8819u);            /* ch0 '2' (1) | ch1 '!' (2 << 2) */
        CHECK(cy[1] == 0x8819u);            /* ch2 '2' (mode 0 → 1) | ch3 '!' (2 << 2) */
        CHECK(cy[20] == (uint16_t)(0xF000u | (uint16_t)((500u + 4u) / 6u)));
        CHECK(cy[21] == 0xF7FFu);           /* (0x2FFA + 4) / 6 = 0x7FF */
        CHECK(cy[4] == 0x8815u && cy[14] == 0xE800u && cy[24] == 0xF000u);
    }

    /* ── 6. the same-tick refresh: a change is sent at once ──────────── */
    reset();
    drain();
    CELL_SEQ2_STATE[0] = 0x11u;
    drv_letter_refresh();                   /* boot: snapshot empty → send */
    drain();
    {
        const uint32_t n0 = s_drv_refresh_n;
        drv_letter_refresh();               /* nothing changed */
        drain();
        CHECK(s_drv_refresh_n == n0);
        CELL_SEQ2_STATE[0] = (uint8_t)'U';  /* letters 0 → sent this tick */
        drv_letter_refresh();
        drain();
        CHECK(s_drv_refresh_n == n0 + 1u);
        CHECK(zero_content(POST, 0u));
        drv_rq_request_set(4u, 8500u);      /* a current change → sent this tick */
        drv_letter_refresh();
        drain();
        CHECK(s_drv_refresh_n == n0 + 2u);
        CHECK(drv_job_tx(DRV_CYC)[2] == 0x8812u &&
              drv_job_tx(DRV_CYC)[12] == (uint16_t)(0xE800u | (uint16_t)((8500u + CELL_RQ_ROUND) / 6u)));
    }

    /* ── r03d: THE D1 PILOT as the target builds it — only ch 3 (E) at 500
     * (50 mA): chip 1 mode 0x18 (B letter 0 -> 0, E '!' -> 2 << 2), E's
     * setpoint (500 + round) / 6 = 0x53 in the F0 row; every other chip
     * as the zero-request 'U' content; before 'U' nothing changes */
    {
        static const uint16_t PIL[10] = { 0, 0, 0, 500, 0, 0, 0, 0, 0, 0 };
        reset();
        CELL_OUT_MV[0] = 0x3420u;               /* the bench's rail node, 13 344 mV */
        set_req(PIL);
        CELL_SEQ2_STATE[0] = 0x11u;
        drv_cyclic_build(0, 8u);
        CHECK(zero_content(PRE, 0u));           /* before 'U': the gate holds it at 0 */
        CELL_SEQ2_STATE[0] = (uint8_t)'U';
        for (int k = 0; k < 3; k++) {           /* every rounding phase 0/4/1 */
            const uint16_t *cy = drv_job_tx(DRV_CYC);
            drv_cyclic_build(0, 8u);
            CHECK(cy[0] == 0x8810u && cy[1] == 0x8818u && cy[2] == 0x8810u &&
                  cy[3] == 0x8810u && cy[4] == 0x8815u);
            for (unsigned c = 0u; c < 5u; c++) {
                CHECK(cy[10u + c] == 0xE800u);
                CHECK(cy[20u + c] == (c == 1u ? (uint16_t)(0xF000u | (uint16_t)((500u + CELL_RQ_ROUND) / 6u))
                                              : 0xF000u));
            }
            CHECK(cy[21] == 0xF053u || cy[21] == 0xF054u);
            CHECK(CELL_RQ_LETTER[3] == (uint8_t)'!' && CELL_RQ_CUR[3] == 500u);
        }
        drv_rq_request_set(3u, 499u);            /* below the min-current law: zeroed */
        drv_cyclic_build(0, 8u);
        CHECK(drv_job_tx(DRV_CYC)[1] == 0x8814u && drv_job_tx(DRV_CYC)[21] == 0xF000u);   /* mode 0 -> E letter 2 */
    }
    /* r03e: the pilot at E's stock P value 8500 (850 mA): chip 1 mode 0x18,
     * E's setpoint (8500 + round) / 6 = 0x588 / 0x589 (the dump's F589) */
    {
        static const uint16_t PIL[10] = { 0, 0, 0, 8500, 0, 0, 0, 0, 0, 0 };
        reset();
        CELL_OUT_MV[0] = 0x3420u;
        set_req(PIL);
        CELL_SEQ2_STATE[0] = (uint8_t)'U';
        for (int k = 0; k < 3; k++) {
            const uint16_t *cy = drv_job_tx(DRV_CYC);
            drv_cyclic_build(0, 8u);
            CHECK(cy[0] == 0x8810u && cy[1] == 0x8818u && cy[2] == 0x8810u &&
                  cy[3] == 0x8810u && cy[4] == 0x8815u);
            CHECK(cy[21] == (uint16_t)(0xF000u | (uint16_t)((8500u + CELL_RQ_ROUND) / 6u)));
            CHECK(cy[21] == 0xF588u || cy[21] == 0xF589u);
            CHECK(cy[11] == 0xE800u && cy[20] == 0xF000u && cy[22] == 0xF000u);
            CHECK(CELL_RQ_CUR[3] == 8500u && CELL_RQ_LETTER[3] == (uint8_t)'!');
        }
    }
    /* r03f: the stock P set as the target builds it (no adaptive correction):
     * modes 881A 881A 881A 8812 8815 (the dump's), setpoints (I + round)/6 */
    {
        static const uint16_t PS[10] = { 500, 8500, 8500, 8500, 8500, 500, 500, 0, 0, 0 };
        static const uint16_t M[5] = { 0x881A, 0x881A, 0x881A, 0x8812, 0x8815 };
        reset();
        CELL_OUT_MV[0] = 0x3420u;
        set_req(PS);
        CELL_SEQ2_STATE[0] = 0x11u;
        drv_cyclic_build(0, 8u);
        CHECK(zero_content(PRE, 0u));               /* before 'U': nothing */
        CELL_SEQ2_STATE[0] = (uint8_t)'U';
        for (int k = 0; k < 3; k++) {
            const uint16_t *cy = drv_job_tx(DRV_CYC);
            drv_cyclic_build(0, 8u);
            const uint16_t r = CELL_RQ_ROUND;
            for (unsigned c = 0u; c < 5u; c++) {
                CHECK(cy[c] == M[c]);
                const uint16_t i0 = PS[2u * c], i1 = PS[2u * c + 1u];
                CHECK(cy[10u + c] == (uint16_t)(0xE800u | (i0 >= 500u ? (uint16_t)((i0 + r) / 6u) : 0u)));
                CHECK(cy[20u + c] == (uint16_t)(0xF000u | (i1 >= 500u ? (uint16_t)((i1 + r) / 6u) : 0u)));
            }
            CHECK(CELL_RQ_LETTER[7] == 0u && CELL_RQ_CUR[7] == 0u);   /* park release off */
        }
    }
    printf("test_reqlaw: %s (%d failures)\n", fails ? "FAIL" : "ok", fails);
    return fails ? 1 : 0;
}
