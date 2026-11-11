/* sup_handlers.c — the ten reply-grammar completion handlers (design §7.4,
 * spec 22 §4 validated). Each runs in ISR context: cell writes only, short.
 * On state-byte ≠ '@' or class failure: error bit + per-job error counter.
 */
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_cells.h"
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

/* ── J1: {8E|ctrl, C9, 22, 09} — handler sup_resp_ctrl ─────────────────── */

void sup_resp_ctrl(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    /* frames 0,2 → SSLB1 (A8 class); frames 1,3 → SSLB0 (AA class) */
    if (!SSLB1_OK(rx[0])) CELL_J1_ERR |= 1u;
    if (!SSLB0_OK(rx[1])) CELL_J1_ERR |= 2u;
    if (!SSLB1_OK(rx[2])) CELL_J1_ERR |= 4u;
    if (!SSLB0_OK(rx[3])) CELL_J1_ERR |= 8u;
    /* THE ECHO CELL: rx[2] low byte = the 0x22 echo. Harvested
     * UNCONDITIONALLY — error bits and cell harvests are separate acts in
     * the stock's cell law (spec 22 §4.4 lists the 0xFFF907D5 row
     * unconditionally; F4/D6: the old goto-fin on the class verdict skipped
     * it and permanently killed CELL_ECHO_22/CELL_GRANT on the bench's real
     * 0xAB-class reply, locking sup_ctrl's bit-2 life sign at 0x04). */
    CELL_ECHO_22 = (uint8_t)(rx[2] & 0xFFu);
    /* echo test: (rx[2] ^ staged_ctrl) & 0xDF — bit 5 excluded (NR-8:
     * the rail is the chip's own telemetry, never asserted by us) */
    if (((rx[2] & 0xFFu) ^ CELL_CTRL_STAGED) & 0xDFu) {
        CELL_J1_ECHOFAIL |= 2u;
    } else {
        CELL_J1_OK |= 2u;
    }
    /* C9 echo: rx[3] low byte == staged C9 (SSLB0 mirrors on the next read) */
    if ((uint8_t)(rx[3] & 0xFFu) == CELL_C9_STAGED) {
        CELL_J1_OK |= 0x20u;
    } else {
        CELL_J1_ECHOFAIL |= 0x20u;
    }
    /* grant: echo bit 0 → the confirmation cell */
    CELL_GRANT = (CELL_ECHO_22 & 0x01u) != 0u ? 1u : 0u;
    /* handover readiness latch */
    CELL_HANDOVER_RDY = (uint8_t)(CELL_REG_14 & 0x01u);
    sup_job_release(JOB_CTRL);
    sup_job_engine_pump();
}

/* ── J2: {90|en, 24} — handler sup_resp_en ─────────────────────────────── */

void sup_resp_en(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    if (!SSLB1_OK(rx[0]) || !SSLB1_OK(rx[1])) {
        CELL_J1_ERR |= 0x10u;
    } else {
        /* the 0x24 certification echo */
        CELL_ECHO_24 = (uint8_t)(rx[1] & 0xFFu);
        /* cert mask: bits 0,2,4 of the reply must match the staged 0x90 */
        if (((rx[1] & 0xFFu) ^ CELL_EN_90) & 0x15u) {
            CELL_J1_ECHOFAIL |= 0x10u;
        } else {
            CELL_J1_OK |= 0x10u;
        }
    }
    sup_job_release(JOB_EN);
    sup_job_engine_pump();
}

/* ── J3: {28, 2E} — handler sup_resp_pos ──────────────────────────────── */

void sup_resp_pos(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    if (!SSLB1_OK(rx[0]) || !SSLB1_OK(rx[1])) {
        CELL_J1_ERR |= 0x40u;
    } else {
        CELL_TELEM_VALID = (uint8_t)((rx[0] & 0x40u) != 0u ? 1u : 2u);
        CELL_TELEMETRY = (uint16_t)(((rx[0] & 0x0Fu) << 6) | (rx[1] & 0x3Fu));
        CELL_J1_OK |= 0x40u;
    }
    sup_job_release(JOB_POS);
    sup_job_engine_pump();
}

/* ── J4: {00FF} — handler sup_resp_sslb2 (no header check) ─────────────── */

void sup_resp_sslb2(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    CELL_SSLB2_RAW = rx[0];
    CELL_SSLB2_COPY = rx[0];
    sup_job_release(JOB_SSLB2);
    sup_job_engine_pump();
}

/* ── J5: {06, 03, 00, 00} — handler sup_resp_id ────────────────────────── */

void sup_resp_id(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    /* frames 0,2 → SSLB1 (A8); frames 1,3 → SSLB0 (AA) */
    if (SSLB1_OK(rx[0])) CELL_ID_B1_06 = (uint8_t)(rx[0] & 0xFFu);
    if (SSLB0_OK(rx[1])) CELL_ID_B0_06 = (uint8_t)(rx[1] & 0xFFu);
    if (SSLB1_OK(rx[2])) CELL_ID_B1_00 = (uint8_t)(rx[2] & 0xFFu);
    if (SSLB0_OK(rx[3])) CELL_ID_B0_00 = (uint8_t)(rx[3] & 0xFFu);
    sup_job_release(JOB_ID);
    sup_job_engine_pump();
}

/* ── J6: {18, 0C, 1E, 06, 14} — handler sup_resp_stat ──────────────────── */

void sup_resp_stat(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    /* frames 0,2,4 → SSLB1; frames 1,3 → SSLB0 */
    if (SSLB1_OK(rx[0])) CELL_STAT_18 = (uint8_t)(rx[0] & 0xFFu);
    if (SSLB0_OK(rx[1])) {
        CELL_STAT_B0_0C = (uint8_t)(rx[1] & 0xFFu);
        CELL_SSLB0_NIB = (uint8_t)((rx[1] & 0x0Fu) ^ 0x0Fu);
    }
    if (SSLB1_OK(rx[2])) {
        CELL_STAT_1E = (uint8_t)(rx[2] & 0xFFu);
        /* direction bits */
        if (rx[2] & 0x10u) CELL_DIR_BITS |= 0x01u; else CELL_DIR_BITS &= ~0x01u;
        if (rx[2] & 0x20u) CELL_DIR_BITS |= 0x02u; else CELL_DIR_BITS &= ~0x02u;
    }
    if (SSLB0_OK(rx[3])) CELL_STAT_B0_06 = (uint8_t)(rx[3] & 0xFFu);
    if (SSLB1_OK(rx[4])) {
        /* THE 0x14 REGISTER — bits 5-7 = per-output fault flags */
        CELL_REG_14 = (uint8_t)(rx[4] & 0xFFu);
        /* the bit mirrors are BOOLEANS in the stock (0x92BBE:48/49:
         * DAT_fff90959 = (rx & 2) != 0, DAT_fff90956 = (rx & 0x10) != 0) —
         * 1/0, not the mask, so the bench witness reads the stock's values */
        CELL_SSLB0_BIT2 = ((rx[4] & 0x02u) != 0u) ? 1u : 0u;   /* bit 1 */
        CELL_SSLB0_BIT4 = ((rx[4] & 0x10u) != 0u) ? 1u : 0u;   /* bit 4 */
        /* NR-8: any fault bit → queue the fault twin */
        if (CELL_REG_14 & 0xE0u) {
            sup_job_queue(JOB_FLT);
        }
    }
    sup_job_release(JOB_STAT);
    sup_job_engine_pump();
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
    /* all 7 words must be A8 class */
    for (uint8_t i = 0u; i < 7u; i++) {
        if (!SSLB1_OK(rx[i])) goto bad;
    }
    /* harvest: fb [5], commit [4], echo [6] */
    CELL_FB_0C = (uint8_t)(rx[5] & 0xFFu);
    CELL_COMMIT_12 = (uint8_t)(rx[4] & 0xFFu);
    CELL_ECHO_0A = (uint8_t)(rx[6] & 0x3Fu);
    CELL_DUTY_COUNT++;
    CELL_REPLY_FRESH |= 1u;
    goto fin;
bad:
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
    for (uint8_t i = 0u; i < 4u; i++) {
        if (!SSLB1_OK(rx[i])) goto bad;
    }
    /* harvest: echo [1], commit [2], fb [3] */
    CELL_ECHO_0A = (uint8_t)(rx[1] & 0x3Fu);
    CELL_COMMIT_12 = (uint8_t)(rx[2] & 0xFFu);
    CELL_FB_0C = (uint8_t)(rx[3] & 0xFFu);
    CELL_LEVEL_COUNT++;
    CELL_REPLY_FRESH |= 2u;
    goto fin;
bad:
    /* F4 (protocol_audit): same bookkeeping law as W1 above — reject-all
     * stays, but the discard is counted per job (spec 22 §4 preamble). */
    s_job_err_cnt[JOB_LEVEL]++;
fin:
    sup_job_release(JOB_LEVEL);
    sup_job_engine_pump();
}

/* ── F: {90|en, 24} — handler sup_resp_flt (same shape as J2) ──────────── */

void sup_resp_flt(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n)
{
    (void)bus; (void)job_id; (void)n;
    if (SSLB1_OK(rx[0]) && SSLB1_OK(rx[1])) {
        if (((rx[1] & 0xFFu) ^ CELL_EN_90) & 0x15u) {
            /* echo fail — count but don't fault (the stock: error bits only) */
        }
    }
    sup_job_release(JOB_FLT);
    sup_job_engine_pump();
}
