/* sup_letters.h — the letters and 0x90 word chain (design §7.8, spec 23).
 *
 * The rail's real producer chain (spec 23 / artifact 3): the power-on
 * self-test ladder classifies each monitor channel → letters[·] = 'c' →
 * switch_status_build folds the letters into the 0x90 enables byte → the
 * J2/F staging ORs it into the cyclic enables job (fault-masked pre-send,
 * NR-8). Reduced-faithful: the vehicle-domain speed classifier of 0x85828
 * is out of scope (no vehicle domain on the bench; design §12 V6).
 */
#ifndef TCU_SUP_LETTERS_H
#define TCU_SUP_LETTERS_H

#include <stdint.h>
#include <stdbool.h>

/* one step of the reduced self-test ladder, all 3 channels in parallel
 * (phases 1/2/4/8; command codes 0x11/0x16 per channel; phase 2 gates on
 * the 0x14 bit-1 mirror — fail latches the error bit and restarts the
 * channel's ladder, stock 0x8BB34 law). Call once per 5 ms pass. */
void sup_selftest_step(void);

/* enable/disable a monitor channel: letters[ch] = 'c' or 0
 * (stock 0x8C7A8 mon2_chan_enable) */
void mon2_chan_enable(uint8_t ch, bool enable);

/* FE13 sub-16 bench arm (spec 23 §2 candidate #1): force the slot-0 letter
 * ('c' — the rail's letter, spec 23 §A1) through the stock's own enable cell
 * 0x8C7A8, past the phase-2 gate (0x8BB34) whose input — the chip's 0x14
 * bit 1 — is chip-side and clear on the bench. Deliberate bench ARM only:
 * the arm and the real ladder share the cell, so if the chip ever reports
 * bit 1 the ladder's phase 8 re-enables through the same path and this hook
 * becomes redundant. Disarm restores letters[0] = 0. */
void sup_letters_bench_arm(bool on);

/* compose the 0x90 enables byte (stock 0x91758 switch_status_build):
 * base & 0x1D | letter bits (slots 0/1/2 → bits 5/6/7) | calib<<5 (0) |
 * bit3 source; one-way clear latch strips 0x60. Writes the enables cell
 * 0xFFF907D6 and returns the byte. The fault mask ~(0x14 & 0xE0) is applied
 * at staging (sup_enables_stage), not here. */
uint8_t sup_switch_status_build(void);

/* stage the 0x90 word for JOB_EN and JOB_FLT (fault-masked, NR-8) and queue
 * both check-free (NR-6). Every 5 ms pass; gate is job-idle only, NOT
 * letter-change (stock 0x91804/0x91810/0x92E80 law). */
void sup_enables_stage(void);

#endif /* TCU_SUP_LETTERS_H */
