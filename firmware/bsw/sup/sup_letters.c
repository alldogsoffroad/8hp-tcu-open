/* sup_letters.c — the letters and 0x90 word chain (design §7.8, spec 23).
 *
 * Stock machinery mirrored (addresses in comments):
 *   - selftest ladder   0x8B7A8 selftest_seq_dispatch, phases 1/2/4/8 here;
 *                       command-state codes 0x11/0x16 written per channel
 *                       (stock command cells 0xFFF939B6[ch], written by the
 *                       phase handlers 0x85E64-class)
 *   - the phase-2 gate  0x8BB34: 0x14 bit 1 (0xFFF90959) must be SET, else
 *                       error latch (0xFFF93A8C) + ladder restart. Spec 23
 *                       candidate #1: a correctly-run walk makes the chip
 *                       report bit 1; if it never does, letters stay 0 —
 *                       correct and diagnosable via the cells.
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
 * Cells that sit outside the pinned map (0xFFF93A90/8C, 0xFFF939B6) are
 * module-internal statics — they are not bench-read protocol cells and
 * pinning them could collide with auto-placed RAM (design §9 rule).
 */
#include "bsw/sup/sup_letters.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_cells.h"

#define LETTERS_CH 3u

/* the self-test ladder */
static uint8_t s_phase[LETTERS_CH];      /* 0 idle, 1/2/4/8 ladder phases */
static uint8_t s_cmd[LETTERS_CH];        /* command-state codes (0x11/0x16) */
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

void sup_selftest_step(void)
{
    uint8_t ch;

    for (ch = 0u; ch < LETTERS_CH; ch++) {
        switch (s_phase[ch]) {
        case 0u:
            /* idle → start the ladder (phase 1) */
            s_phase[ch] = 1u;
            break;
        case 1u:
            /* phase 1: issue the 0x11 command code */
            s_cmd[ch] = 0x11u;
            s_phase[ch] = 2u;
            break;
        case 2u:
            /* THE GATE (0x8BB34): 0x14 bit 1 must be SET. */
            if (CELL_SSLB0_BIT2 == 0u) {
                s_err_latch |= (uint8_t)(1u << ch);   /* error latch */
                s_phase[ch] = 1u;                     /* restart the ladder */
                break;
            }
            s_cmd[ch] = 0x16u;
            s_phase[ch] = 4u;
            break;
        case 4u:
            /* phase 4: issue the 0x11 code again */
            s_cmd[ch] = 0x11u;
            s_phase[ch] = 8u;
            break;
        case 8u:
            /* phase 8: issue the 0x16 code, then classify the channel */
            s_cmd[ch] = 0x16u;
            s_phase[ch] = 0u;
            mon2_chan_enable(ch, true);               /* letters[ch] = 'c' */
            break;
        default:
            s_phase[ch] = 0u;
            break;
        }
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
