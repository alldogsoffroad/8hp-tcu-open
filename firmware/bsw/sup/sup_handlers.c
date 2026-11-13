/* sup_handlers.c — the ten reply-grammar completion handlers (design §7.4,
 * spec 22 §4 validated). Each runs in ISR context: cell writes only, short.
 * On state-byte ≠ '@' or class failure: error bit + per-job error counter.
 */
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/sup/sup_letters.h"
#include "platform.h"

/* class checks (spec 22 §4 preamble) */
#define SSLB1_OK(rx)  (((rx) & 0xF900u) == 0xA800u)
#define SSLB0_OK(rx)  (((rx) & 0xFE00u) == 0xAA00u)

/* F4/F8 (protocol_audit): per-job reply-error counters. The spec 22 §4
 * preamble's per-job error address (0xFFF94218[2*id]) is owned by the use
 * counters (sup_jobs launch), so the error counts live here — bench reads
 * them by ELF symbol until the stock decode resolves the F8 conflict. */
static volatile uint8_t s_job_err_cnt[JOB_COUNT];

/* the per-job discard counter bump for the job engine's stale-burst recovery
 * (sup_jobs.c check_free — the stock's 0x93B8E busy-find law counts the
 * discard in the per-job error cell via the release fn's bad path; the
 * counter lives here beside the reply handlers that own it) */
void sup_job_err_bump(uint8_t id)
{
    if (id < JOB_COUNT) {
        s_job_err_cnt[id]++;
    }
}

/* r02u: read access for the descent recorder (sup_walk.c) */
uint8_t sup_job_err_get(uint8_t id)
{
    return (id < JOB_COUNT) ? s_job_err_cnt[id] : 0u;
}

/* ── THE STOCK BIT MAP (S5/S6/S8, baseline review 2026-11-11) ────────────
 * Ported 1:1 from the stock handlers' decompiles (~/tcu_sweep/decomp/
 * r08_app/0x92D94, 0x92ED4, 0x92FD0, 0x93096, 0x931D4, 0x92AC0, 0x92BBE):
 *   A8 (SSLB1) class → 0x907C8 OK / 0x907C9 ERR / 0x907CA ECHOFAIL,
 *     bits J5 0x01, J1 0x02, J6 0x04, J2 0x10, F 0x20, J3 0x40;
 *   AA (SSLB0) class → 0x90973 OK / 0x90974 ERR / 0x90975 ECHOFAIL,
 *     bits J5 1, J1 2 (the C9 echo), J6 4.
 * Each class test covers the frames of ONE select as a group (stride 2 on
 * the two-select jobs); echo tests compare against the word the burst
 * actually SENT (the job's TX staging, stock 0xFFF88424/34/3C), never the
 * composer's newer cell. Echo harvests (0x907D5, 0x907D7) are
 * unconditional. Per-round clearing is the 0x93126 summarizer below. */

static bool grp_ok(const uint16_t *rx, uint8_t first, uint8_t count, bool b0)
{
    for (uint8_t i = 0u; i < count; i++) {
        uint16_t w = rx[first + 2u * i];
        if (b0 ? !SSLB0_OK(w) : !SSLB1_OK(w)) {
            return false;
        }
    }
    return true;
}

/* ── J1: {8E|ctrl, C9, 22, 09} — stock 0x92D94 ─────────────────────────── */

void sup_resp_ctrl(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    const uint16_t *tx = sup_job_table[JOB_CTRL].tx_stage;
    bool ok;

    (void)bus; (void)job_id; (void)n;
    if (n == 0u) {                       /* error completion: the stock's not-'@' branch */
        CELL_B0_ERR |= 2u;
        CELL_J1_ERR |= 2u;
        goto fin;
    }
    /* SSLB0 frames 1,3: class, then the C9 echo (rx[3] low byte vs the
     * staged C9 DATA byte — 0x92DDE movu.b @(3,r13) with r13 = 0xFFF88424
     * reads 0xFFF88427; the decompile's "0xFFF88426" is the word base) */
    ok = grp_ok(rx, 1u, 2u, true);
    if (!ok) {
        CELL_B0_ERR |= 2u;
    }
    if (!ok || (uint8_t)(rx[3] & 0xFFu) != (uint8_t)(tx[1] & 0xFFu)) {
        CELL_B0_ECHOFAIL |= 2u;
    } else {
        CELL_B0_OK |= 2u;
    }
    /* SSLB1 frames 0,2: class; the 0x22 echo harvested UNCONDITIONALLY;
     * OK only when the class passed AND the echo matches the SENT word
     * (bit 5 excluded — NR-8: the chip's own rail telemetry) */
    ok = grp_ok(rx, 0u, 2u, false);
    if (!ok) {
        CELL_J1_ERR |= 2u;
    }
    CELL_ECHO_22 = (uint8_t)(rx[2] & 0xFFu);
    if (ok && (((rx[2] ^ tx[0]) & 0xDFu) == 0u)) {
        CELL_J1_OK |= 2u;
    } else {
        CELL_J1_ECHOFAIL |= 2u;
    }
    /* grant: echo bit 0 → the confirmation cell */
    CELL_GRANT = (CELL_ECHO_22 & 0x01u) != 0u ? 1u : 0u;
    /* handover readiness latch */
    CELL_HANDOVER_RDY = (uint8_t)(CELL_REG_14 & 0x01u);
fin:
    sup_job_release(JOB_CTRL);
    sup_job_engine_pump();
}

/* the J2 / F shape (stock 0x92ED4 / 0x92FD0): both frames SSLB1, the 0x24
 * echo harvested unconditionally, cert mask 0x15 against the SENT 0x90 */
static void resp_enables(const uint16_t *rx, const uint16_t *tx, uint8_t bit)
{
    bool ok = SSLB1_OK(rx[0]) && SSLB1_OK(rx[1]);

    if (!ok) {
        CELL_J1_ERR |= bit;
    }
    CELL_ECHO_24 = (uint8_t)(rx[1] & 0xFFu);
    if (ok && (((rx[1] ^ tx[0]) & 0x15u) == 0u)) {
        CELL_J1_OK |= bit;
    } else {
        CELL_J1_ECHOFAIL |= bit;
    }
}

/* ── J2: {90|en, 24} — stock 0x92ED4 ───────────────────────────────────── */

void sup_resp_en(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    if (n == 0u) {                       /* error completion: the stock's not-'@' branch */
        CELL_J1_ERR |= 0x10u;
        goto fin;
    }
    resp_enables(rx, sup_job_table[JOB_EN].tx_stage, 0x10u);
fin:
    sup_job_release(JOB_EN);
    sup_job_engine_pump();
}

/* ── J3: {28, 2E} — stock 0x93096 ──────────────────────────────────────── */

void sup_resp_pos(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    if (n == 0u) {                       /* error completion: the stock's not-'@' branch */
        CELL_J1_ERR |= 0x40u;
        CELL_TELEMETRY = 0xFFFFu;          /* 0x93096: uVar6 = 0xFFFF stored on every path */
        goto fin;
    }
    if (SSLB1_OK(rx[0]) && SSLB1_OK(rx[1])) {
        CELL_TELEM_VALID = (uint8_t)((rx[0] & 0x40u) != 0u ? 1u : 0u);
        CELL_TELEMETRY = (uint16_t)(((rx[0] & 0x0Fu) << 6) | (rx[1] & 0x3Fu));
        CELL_J1_OK |= 0x40u;
    } else {
        CELL_TELEM_VALID = 2u;
        CELL_TELEMETRY = 0xFFFFu;
        CELL_J1_ERR |= 0x40u;
    }
fin:
    sup_job_release(JOB_POS);
    sup_job_engine_pump();
}

/* ── J4: {00FF} — stock 0x931D4 (no header check; 0x908EE only — the
 *    0x908CC copy is the reply processor's, sup_reply_condense) ───────── */

void sup_resp_sslb2(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    CELL_SSLB2_RAW = rx[0];
    sup_job_release(JOB_SSLB2);
    sup_job_engine_pump();
}

/* ── J5: {06, 03, 00, 00} — stock 0x92AC0 (all-or-nothing per select) ── */

void sup_resp_id(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    if (n == 0u) {                       /* error completion: the stock's not-'@' branch */
        CELL_J1_ERR |= 1u;
        CELL_B0_ERR |= 1u;
        goto fin;
    }
    if (grp_ok(rx, 1u, 2u, true)) {
        CELL_ID_B0_06 = (uint8_t)(rx[1] & 0xFFu);
        CELL_ID_B0_00 = (uint8_t)(rx[3] & 0xFFu);
        CELL_B0_OK |= 1u;
    } else {
        CELL_B0_ERR |= 1u;
    }
    if (grp_ok(rx, 0u, 2u, false)) {
        CELL_ID_B1_06 = (uint8_t)(rx[0] & 0xFFu);
        CELL_ID_B1_00 = (uint8_t)(rx[2] & 0xFFu);
        CELL_J1_OK |= 1u;
    } else {
        CELL_J1_ERR |= 1u;
    }
fin:
    sup_job_release(JOB_ID);
    sup_job_engine_pump();
}

/* ── J6: {18, 0C, 1E, 06, 14} — stock 0x92BBE ──────────────────────────
 * The SSLB1 group test covers rx[0] and rx[2] ONLY (2 iterations, stride
 * 2): rx[4] — the 0x14 gate register — is never class-checked (S5: ours
 * required it, so a 0xA9-class 0x14 reply dropped the gate bit the stock
 * keeps). The F re-fire tests the 0x907D1 CELL, harvested or not. The
 * direction bits (0x9095E) are the reply processor's (0x91ACA), not here. */

void sup_resp_stat(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    if (n == 0u) {                       /* error completion: the stock's not-'@' branch */
        CELL_J1_ERR |= 4u;
        CELL_B0_ERR |= 4u;              /* no F re-fire on this branch */
        goto fin;
    }
    if (grp_ok(rx, 1u, 2u, true)) {
        CELL_STAT_B0_0C = (uint8_t)(rx[1] & 0xFFu);
        CELL_STAT_B0_06 = (uint8_t)(rx[3] & 0xFFu);
        CELL_SSLB0_NIB = (uint8_t)((CELL_STAT_B0_0C & 0x0Fu) ^ 0x0Fu);
        CELL_B0_OK |= 4u;
    } else {
        CELL_B0_ERR |= 4u;
    }
    if (grp_ok(rx, 0u, 2u, false)) {
        CELL_STAT_18 = (uint8_t)(rx[0] & 0xFFu);
        CELL_STAT_1E = (uint8_t)(rx[2] & 0xFFu);
        /* the bit mirrors are BOOLEANS (0x92BBE: 0x90959 = (rx&2)!=0,
         * 0x90956 = (rx&0x10)!=0) */
        CELL_SSLB0_BIT2 = ((rx[4] & 0x02u) != 0u) ? 1u : 0u;   /* bit 1 = THE GATE */
        CELL_REG_14 = (uint8_t)(rx[4] & 0xFFu);
        CELL_SSLB0_BIT4 = ((rx[4] & 0x10u) != 0u) ? 1u : 0u;
        CELL_J1_OK |= 4u;
    } else {
        CELL_J1_ERR |= 4u;
    }
    /* NR-8: any fault bit → the F queuer 0x92F7C (check-free first) */
    if ((CELL_REG_14 & 0xE0u) != 0u) {
        sup_flt_requeue();
    }
fin:
    sup_job_release(JOB_STAT);
    sup_job_engine_pump();
}

/* ── the 0x93126 summarizer (stock 0x931F8 twice; runs in 0x91DAA, the
 *    batch's J6/J1 step): fold each triple into its summary byte — bit 1
 *    any ERR, bit 2 ECHOFAIL without ERR, bit 0 OK-only — and CLEAR the
 *    triple once every expected handler has reported (mask 7 / 0x77);
 *    otherwise keep accumulating and carry the previous summary's low 3
 *    bits. Bit 7: the 0xBB / 0xBC ID markers (0x90977, 0x907CF). The
 *    0x9094E = 0x2D/0x2E consumer of the summaries is not decoded [D]. */
static uint8_t sum_fold(volatile uint8_t *t, uint8_t prev, uint8_t mask)
{
    uint8_t ok = t[0], err = t[1], ef = t[2];
    uint8_t r = 0u;

    if (err != 0u) r = 2u;
    if ((ef & (uint8_t)~err) != 0u) r |= 4u;
    if ((ok & (uint8_t)~err & (uint8_t)~ef) != 0u) r |= 1u;
    if (mask == (uint8_t)((ok | err | ef) & mask)) {
        t[0] = 0u; t[1] = 0u; t[2] = 0u;
    } else {
        r |= (uint8_t)(prev & 7u);
    }
    return r;
}

void sup_reply_summarize(void)
{
    uint8_t r;

    r = sum_fold(&CELL_B0_OK, CELL_SUM_B0, 7u);
    if (CELL_ID_B0_00 == 0xBBu) r |= 0x80u;
    CELL_SUM_B0 = r;
    r = sum_fold(&CELL_J1_OK, CELL_SUM_B1, 0x77u);
    if (CELL_ID_B1_00 == 0xBCu) r |= 0x80u;
    CELL_SUM_B1 = r;
}

/* ── J7: {00} — handler sup_resp_nib (class check tolerated) ───────────── */

void sup_resp_nib(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    /* the stock tolerates this job's header failure (0x92CC8) */
    CELL_STAT_B0_0C = (uint8_t)(rx[0] & 0xFFu);
    CELL_SSLB0_NIB = (uint8_t)((rx[0] & 0x0Fu) ^ 0x0Fu);
    sup_job_release(JOB_NIB);
    sup_job_engine_pump();
}

/* ── W1: {88|t×4, 12, 0C, 0A} — handler sup_resp_duty ──────────────────── */

void sup_resp_duty(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    /* stock 0x910EE: stamp the exchange (TCNTA - 0x29, the burst time) and
     * count the completion FIRST, ok or not — the walk re-bases its windows
     * on this stamp and the reply processor's freshness is this counter */
    CELL_REPLY_STAMP = plat_frt_us() - 0x29u;
    CELL_DUTY_RXCNT++;
    /* all 7 words must be A8 class */
    for (uint8_t i = 0u; i < 7u; i++) {
        if (!SSLB1_OK(rx[i])) goto bad;
    }
    /* harvest: fb [5], commit [4], echo [6] */
    CELL_FB_0C = (uint8_t)(rx[5] & 0xFFu);
    CELL_COMMIT_12 = (uint8_t)(rx[4] & 0xFFu);
    CELL_ECHO_0A = (uint8_t)(rx[6] & 0xFFu);   /* 0x910EE: full byte */
    CELL_DUTY_COUNT++;
    CELL_REPLY_FRESH |= 1u;
    goto fin;
bad:
    CELL_RX_ERR |= 1u;                          /* 0x910EE: 0x90541 |= 1 */
    /* F4 (protocol_audit): keep the reject-all gate (spec-faithful, §5 —
     * never widen the masks to accept 0xAB as valid) but COUNT the discard:
     * the empty bad: label left the spec 22 §4 per-job error bookkeeping
     * unimplemented, so the bench could not see why replies die. The walk
     * engine retries on the next timer fire. */
    s_job_err_cnt[JOB_DUTY]++;
fin:
    sup_job_release(JOB_DUTY);
    sup_job_engine_pump();
}

/* ── W2: {84|lvl, 0A, 12, 0C} — handler sup_resp_level ─────────────────── */

void sup_resp_level(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    /* stock 0x911D8: stamp + count first (see sup_resp_duty) */
    CELL_REPLY_STAMP = plat_frt_us() - 0x29u;
    CELL_LEVEL_COUNT++;
    for (uint8_t i = 0u; i < 4u; i++) {
        if (!SSLB1_OK(rx[i])) goto bad;
    }
    /* harvest: echo [1], commit [2], fb [3] */
    CELL_ECHO_0A = (uint8_t)(rx[1] & 0xFFu);   /* 0x911D8: full byte */
    CELL_COMMIT_12 = (uint8_t)(rx[2] & 0xFFu);
    CELL_FB_0C = (uint8_t)(rx[3] & 0xFFu);
    CELL_REPLY_FRESH |= 2u;
    goto fin;
bad:
    CELL_RX_ERR |= 2u;                          /* 0x911D8: 0x90541 |= 2 */
    /* F4 (protocol_audit): same bookkeeping law as W1 above — reject-all
     * stays, but the discard is counted per job (spec 22 §4 preamble). */
    s_job_err_cnt[JOB_LEVEL]++;
fin:
    sup_job_release(JOB_LEVEL);
    sup_job_engine_pump();
}

/* ── F: {90|en, 24} — stock 0x92FD0 (the J2 shape, bit 0x20) ──────────── */

void sup_resp_flt(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    if (n == 0u) {                       /* error completion: the stock's not-'@' branch */
        CELL_J1_ERR |= 0x20u;
        goto fin;
    }
    resp_enables(rx, sup_job_table[JOB_FLT].tx_stage, 0x20u);
fin:
    sup_job_release(JOB_FLT);
    sup_job_engine_pump();
}
