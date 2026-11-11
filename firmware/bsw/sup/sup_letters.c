/* sup_letters.c — the letters and 0x90 word chain (design §7.8, spec 23).
 *
 * Stock machinery mirrored (addresses in comments):
 *   - selftest ladder   0x8B7A8 selftest_seq_dispatch, phases 1/2/4/8 here;
 *                       the command codes 0x11/0x16 are the seq2 command
 *                       STATES (cells 0xFFF94FD0[ch], set by 0x85E00 via
 *                       seq_group_code_send 0x85600) — DECODE (r01u, 2026-11-11):
 *                       they are internal RAM bookkeeping and NEVER reach a tx
 *                       word; no stock builder consumes them on the wire. The
 *                       wire-visible half of the earn path is only the already
 *                       cyclic J6 STAT request {18,0C,1E,06,14} whose 0x14-class
 *                       reply frame (rx[4], template 0x40D78 frame 5) carries
 *                       the gate bit.
 *   - request cells     0xFFF939B6[ch] via 0x85E64 (set by the phase handlers
 *                       0x8B974/0x8BC54-class; CLEARED to 0 for every channel
 *                       at every ladder init — phase 1's PTR_0008BA44(ch,0)
 *                       loop = 0x85E64(ch,0)); phase 2 fail restarts the ladder
 *                       so the requests stay 0 while the gate is closed.
 *   - the seq2 FSM      0x85828 (periodic task): enable = (state 0x11/0x16)
 *                       AND (request != 0), re-evaluated EVERY pass through
 *                       mon2_chan_enable 0x8C7A8 — the letters follow the
 *                       request cell, so a ladder restart revokes them (the
 *                       earned letter persists only while the request does).
 *   - the phase-2 gate  0x8BB34: 0x14 bit 1 (0xFFF90959) must be SET, else
 *                       error latch (0xFFF93A8C) + ladder restart. Spec 23
 *                       candidate #1: the chip reports bit 1 when the engaged
 *                       context is lawful (stock: from power-on engaged-ready,
 *                       opening_200ms §1 0x14=0xC7; writer of the mode byte
 *                       0xFFF90951 = the vehicle mode-3 latch 0x7DC76/0x6BC76,
 *                       spec 03 §6 — out of bench scope). If it never does,
 *                       letters stay 0 — correct and diagnosable via the cells.
 *   - channel enable    0x8C7A8 mon2_chan_enable: 0xFFF93A90[ch] = enable,
 *                       letters[ch] 0xFFF90944[ch] = 'c' (99) or 0
 *   - enables byte      0x91758 switch_status_build (the 0x1918EF/E8/EB
 *                       flash-const law: base 0, letter slots {0,1,2} →
 *                       bits 5/6/7, calib 0)
 *   - J2/F staging      0x92E80/0x92F7C: staged word =
 *                       0x9000 | (byte & ~(0x14-reply & 0xE0)) — outputs
 *                       whose fault bits are set lose their letter bits
 *                       before the word is sent (NR-8)
 *
 * TAMPER LAW (r01u bench, 2026-11-11): forcing letters[0]='c' with the gate
 * closed puts the 0x9020 certification pair on the wire in a state the stock
 * can never produce (0x91758 folds only EARNED letter bits), and the chip
 * answers with a latched anti-tamper reset loop (~1.15 s period, outlives
 * RAM, holds until power-off). The bench arm is therefore interlocked on the
 * gate bit — the stock's own guard is structural (an unearned letter bit
 * cannot be staged); the interlock restores that invariant at the arm.
 *
 * Cells that sit outside the pinned map (0xFFF93A90/8C, 0xFFF939B6) are
 * module-internal statics — they are not bench-read protocol cells and
 * pinning them could collide with auto-placed RAM (design §9 rule). The
 * bench reads them by ELF symbol (nm), the s_job_err_cnt pattern.
 */
#include "bsw/sup/sup_letters.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_cells.h"

#define LETTERS_CH 3u
#define PHASE_DONE 0x10u       /* ladder complete: no restart (the stock stops
                                * at the terminal phases; only a mode re-entry
                                * re-ladders, which the reduction omits) */

/* the self-test ladder */
static uint8_t s_phase[LETTERS_CH];      /* 0 boot, 1/2/4/8 ladder, 0x10 done */
static uint8_t s_req[LETTERS_CH];        /* the request cells (stock 0xFFF939B6[ch]:
                                          * the seq2 FSM's per-pass enable input) */
static uint8_t s_err_latch;              /* the 0x8BB34 error latch (internal) */
static bool    s_mon_en[LETTERS_CH];     /* the 0xFFF93A90[ch] enables */

/* letter slots → enable bits (flash table 0x1918E8 = {0,1,2} → bits 5/6/7) */
static const uint8_t S_LETTER_BIT[LETTERS_CH] = { 0x20u, 0x40u, 0x80u };

void mon2_chan_enable(uint8_t ch, bool enable)
{
    if (ch >= LETTERS_CH) {
        return;
    }
    s_mon_en[ch] = enable;
    CELL_LETTERS[ch] = enable ? (uint8_t)'c' : 0u;
}

void sup_letters_bench_arm(bool on)
{
    /* TAMPER INTERLOCK (r01u): the arm is refused while the phase-2 gate is
     * closed (CELL_SSLB0_BIT2 == 0). Forcing the letter then stages 0x9020 —
     * a certification write the chip reads as unlawful and answers with the
     * latched anti-tamper reset loop (bench 2026-11-11 ~13:00, ≥31 s of
     * ~1.15 s resets per arm). Accepted arms set the slot-0 REQUEST cell (the
     * stock's only letters lever, 0x85E64/0x85828 law) — the per-pass seq2
     * law below performs the enable, and a later gate close revokes it
     * exactly like a ladder restart would. Disarm withdraws the request. */
    if (on && CELL_SSLB0_BIT2 == 0u) {
        return;
    }
    s_req[0] = on ? 0x01u : 0u;
    mon2_chan_enable(0u, on);
}

void sup_letters_reladder(void)
{
    uint8_t ch;

    /* the stock re-runs the ladder from the mode sequencer's mode re-entry
     * (0x85160 → PTR_000853F4 = 0x8B7A8); the reduction has no mode
     * sequencer, so this is the documented seam: the next pass starts a
     * fresh ladder whose phase-1 init clears the request cells (the
     * 0x85E64(ch,0) law) — earned letters are withdrawn and only re-earned
     * through the gate. Bench FE13 stop uses it to return the chain to the
     * honest state. */
    for (ch = 0u; ch < LETTERS_CH; ch++) {
        s_phase[ch] = 0u;
    }
}

void sup_selftest_step(void)
{
    uint8_t ch;

    for (ch = 0u; ch < LETTERS_CH; ch++) {
        switch (s_phase[ch]) {
        case 0u:
            /* boot → first ladder run (seq_init_11 0x85678 law) */
            s_phase[ch] = 1u;
            break;
        case 1u:
            /* ladder init: clear the request cell for EVERY channel (stock
             * phase 1, 0x8B974's PTR_0008BA44(ch,0) loop = 0x85E64(ch,0)) —
             * a restart revokes the letters through the per-pass law */
            s_req[ch] = 0u;
            /* phase 1: issue the 0x11 command state (0x85600 law) */
            s_phase[ch] = 2u;
            break;
        case 2u:
            /* THE GATE (0x8BB34): 0x14 bit 1 must be SET. */
            if (CELL_SSLB0_BIT2 == 0u) {
                s_err_latch |= (uint8_t)(1u << ch);   /* error latch 0xFFF93A8C */
                s_phase[ch] = 1u;                     /* restart the ladder */
                break;
            }
            /* gate open: set the request cell (0x85E64 law — only lands in
             * the 0x11/0x16 states, which the restart loop guarantees) and
             * issue the 0x16 command state */
            s_req[ch] = 0x01u;
            s_phase[ch] = 4u;
            break;
        case 4u:
            /* phase 4: issue the 0x11 code again (request holds) */
            s_phase[ch] = 8u;
            break;
        case 8u:
            /* phase 8: issue the 0x16 code, ladder complete — the enable is
             * NOT taken here: the per-pass seq2 law (0x85828 → 0x8C7A8)
             * owns every enable from the request cell, exactly like the
             * stock's periodic FSM */
            s_phase[ch] = PHASE_DONE;
            break;
        case PHASE_DONE:
            /* complete: hold (the stock's terminal phases; letters persist
             * while the request does) */
            break;
        default:
            s_phase[ch] = 0u;
            break;
        }
    }

    /* the seq2 FSM law (0x85828 tail, PTR_00085C94 → 0x8C7A8): the enable
     * follows the request cell every pass — earned through the gate,
     * revoked by any ladder restart */
    for (ch = 0u; ch < LETTERS_CH; ch++) {
        mon2_chan_enable(ch, s_req[ch] != 0u);
    }
}

uint8_t sup_switch_status_build(void)
{
    uint8_t en;
    uint8_t ch;

    /* base cell masked by the flash-const law (0x1918EF = 0x00 → base 0) */
    en = (uint8_t)(CELL_90_BASE & 0x1Du);
    /* letter bits: slots 0/1/2 → bits 5/6/7 */
    for (ch = 0u; ch < LETTERS_CH; ch++) {
        if (CELL_LETTERS[ch] == (uint8_t)'c') {
            en |= S_LETTER_BIT[ch];
        }
    }
    /* calibration channel-enable term (flash 0x1918EB = 0 → no <<5 bit) */
    /* bit 3 source: 0xFFF90952 == 1 (the cell is never written — stock law) */
    if (CELL_BIT3_SRC == 1u) {
        en |= 0x08u;
    }
    /* one-way clear latch strips bits 5 AND 6 (never cleared until reboot) */
    if (CELL_CLEAR_LATCH != 0u) {
        en &= (uint8_t)~0x60u;
    }
    CELL_EN_90 = en;
    return en;
}

void sup_enables_stage(void)
{
    uint8_t en = sup_switch_status_build();
    /* NR-8: fault-mask the letter bits from the 0x14 reply BEFORE sending */
    uint8_t masked = (uint8_t)(en & (uint8_t)~(CELL_REG_14 & 0xE0u));
    uint16_t word = (uint16_t)(0x9000u | masked);

    /* stage both enables twins (the stagers own the words) */
    sup_job_table[JOB_EN].tx_stage[0] = word;
    sup_job_table[JOB_EN].tx_stage[1] = 0x2400u;
    sup_job_table[JOB_FLT].tx_stage[0] = word;
    sup_job_table[JOB_FLT].tx_stage[1] = 0x2400u;
    /* the J2 staging mirror the bench reads at 0xFFF88434 */
    CELL_J2_STAGED = word;
    /* queue check-free — the gate is job-idle only (stock 0x93B8E law) */
    if (sup_job_check_free(JOB_EN)) {
        (void)sup_job_queue(JOB_EN);
    }
    if (sup_job_check_free(JOB_FLT)) {
        (void)sup_job_queue(JOB_FLT);
    }
}
