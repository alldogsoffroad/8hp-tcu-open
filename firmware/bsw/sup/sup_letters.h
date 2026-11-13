/* sup_letters.h — the letters and 0x90 word chain (design §7.8, spec 23).
 *
 * The rail's producer chain: the self-test ladder and seq2 (sup_ladder.c,
 * stock 0x85160 / 0x8B7A8 / 0x85828) → letters[ch] = 'c' (0x8C7A8) →
 * switch_status_build folds the letters into the 0x90 enables byte → the
 * J2/F staging ORs it into the cyclic enables job (fault-masked pre-send,
 * NR-8).
 */
#ifndef TCU_SUP_LETTERS_H
#define TCU_SUP_LETTERS_H

#include <stdint.h>
#include <stdbool.h>

/* enable/disable a monitor channel (stock 0x8C7A8 mon2_chan_enable, ch
 * 0..1): 0x93A90[ch] = enable, the 0x8F50C window reset on a change,
 * letters[ch] = 'c' or 0. Called ONLY by seq2 (sup_ladder.c, 0x85828). */
void mon2_chan_enable(uint8_t ch, bool enable);

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

/* the F queuer alone (stock 0x92F7C): the J6 handler's fault re-fire */
void sup_flt_requeue(void);

#endif /* TCU_SUP_LETTERS_H */
