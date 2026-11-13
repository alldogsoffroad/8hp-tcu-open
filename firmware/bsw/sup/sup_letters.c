/* sup_letters.c — the 0x90 word chain: the letter cells, the composer and
 * the J2/F staging (design §7.8, spec 23).
 *
 * L1 (2026-11-13): the old "reduced ladder" that lived here — phases
 * 1/2/4/8 per channel, an INVERTED phase-2 gate test, the request cell
 * driving the enable every pass, and the FE13 sub-16 bench arm that wrote
 * the request directly — is GONE. The stock's ladder, mode sequencer and
 * seq2 FSM are ported 1:1 in sup_ladder.c (0x85160 / 0x8B7A8 / 0x85828),
 * where the request cells 0xFFF939B6[ch] have exactly one writer (0x85E64,
 * guarded by the seq2 state) and the enable follows the request ONLY in
 * seq2 states 0x11/0x16. What stays here is the stock's letter cell writer
 * and the composer:
 *   - channel enable    0x8C7A8 mon2_chan_enable: 0xFFF93A90[ch] = enable;
 *                       on a change vs 0xFFF93A8E[ch] the 0x8F50C status
 *                       reset; letters[ch] 0xFFF90944[ch] = 'c' (99) or 0
 *   - enables byte      0x91758 switch_status_build (the 0x1918EF/E8/EB
 *                       flash-const law: base 0, letter slots {0,1,2} →
 *                       bits 5/6/7, calib 0)
 *   - J2/F staging      0x92E80/0x92F7C: staged word =
 *                       0x9000 | (byte & ~(0x14-reply & 0xE0)) — outputs
 *                       whose fault bits are set lose their letter bits
 *                       before the word is sent (NR-8)
 * Only slot 0 can ever carry a letter on this calibration (0x1919C8 = 1:
 * seq2 runs channel 0 only; channel 1 is held at 0x22 / enable 0).
 */
#include "bsw/sup/sup_letters.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_cells.h"

#define LETTERS_SLOTS 3u        /* the composer's slot table 0x1918E8..EA */
#define MON2_CH       2u        /* 0x8C7A8's channel range (2 slots) */

/* letter slots → enable bits (flash table 0x1918E8 = {0,1,2} → bits 5/6/7) */
static const uint8_t S_LETTER_BIT[LETTERS_SLOTS] = { 0x20u, 0x40u, 0x80u };

/* stock 0x8C7A8 (bytes/decompile 0x8C7A8..0x8C7E6): 0x93A90[ch] = en; if
 * it differs from 0x93A8E[ch], 0x8F50C(ch) and 0x93A8E[ch] = en; then
 * 0x90944[ch] = en ? 99 : 0. 0x8F50C(ch) zeroes the AN0 descriptor status
 * byte (+7) of 0x90010 (ch 0, mux 8) or 0x90018 (ch 1, mux 10) — the
 * output node's smoothing window restarts on every enable change. */
void mon2_chan_enable(uint8_t ch, bool enable)
{
    const uint8_t en = enable ? 1u : 0u;

    if (ch >= MON2_CH) {
        return;
    }
    CELL_EN_MIRROR[ch] = en;
    if (CELL_EN_PREV[ch] != en) {
        if (ch == 1u) {                                   /* 0x8F50C */
            CELL_ADC_ST_MUX10 = 0u;
        } else {
            CELL_ADC_ST_MUX8 = 0u;
        }
        CELL_EN_PREV[ch] = en;
    }
    CELL_LETTERS[ch] = en != 0u ? (uint8_t)'c' : 0u;
}

uint8_t sup_switch_status_build(void)
{
    uint8_t en;
    uint8_t ch;

    /* base cell masked by the flash-const law (0x1918EF = 0x00 → base 0) */
    en = (uint8_t)(CELL_90_BASE & 0x1Du);
    /* letter bits: slots 0/1/2 → bits 5/6/7 */
    for (ch = 0u; ch < LETTERS_SLOTS; ch++) {
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

/* the J2 / F queuers (stock 0x92E80 / 0x92F7C) — S7 (baseline review):
 * check-free FIRST and return if busy; only then stage the word from the
 * enables cell, fault-masked (NR-8: 0x907D6 & ~(0x907D1 & 0xE0)), clear
 * the last reply word (0x8843A / 0x88442) and queue. The old order staged
 * unconditionally — a re-stage while the job was in flight could tear the
 * burst and the handler compared its echo against a word never sent. */
static void enables_queue(uint8_t id)
{
    if (!sup_job_check_free(id)) {
        return;
    }
    uint16_t word = (uint16_t)(0x9000u |
        (uint8_t)(CELL_EN_90 & (uint8_t)~(CELL_REG_14 & 0xE0u)));
    sup_job_table[id].tx_stage[0] = word;
    sup_job_table[id].tx_stage[1] = 0x2400u;
    sup_job_table[id].rx[1] = 0u;
    if (id == JOB_EN) {
        CELL_J2_STAGED = word;          /* bench mirror (= the stage on target) */
    }
    (void)sup_job_queue(id);
}

void sup_flt_requeue(void)
{
    enables_queue(JOB_FLT);
}

/* the J2 stage 0x91804 (in 0x8EDF6: 5 ms in every mode) and the F stage
 * 0x91810 (in 0x8EE30: 5 ms in mode 0, 10 ms in mode 1) each run the
 * status build 0x91758 first (bytes 0x91806 / 0x91812: bsr 0x91758) */
void sup_j2_stage(void)
{
    (void)sup_switch_status_build();    /* 0x91758 → 0x907D6 */
    enables_queue(JOB_EN);              /* 0x92E80 */
}

void sup_f_stage(void)
{
    (void)sup_switch_status_build();    /* 0x91758 → 0x907D6 */
    enables_queue(JOB_FLT);             /* 0x92F7C */
}

void sup_enables_stage(void)
{
    sup_j2_stage();                     /* 0x91804 */
    sup_f_stage();                      /* 0x91810 (same tick: the second
                                         * build rewrites the same byte) */
}
