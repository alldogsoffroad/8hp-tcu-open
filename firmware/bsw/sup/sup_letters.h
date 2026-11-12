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
 * (phases 1/2/4/8 then done; phase 2 gates on the 0x14 bit-1 mirror — fail
 * latches the error bit and restarts the channel's ladder, stock 0x8BB34
 * law; ladder init clears the request cell, stock phase-1 0x85E64(ch,0)
 * law; the enable follows the request cell EVERY pass — the seq2 FSM law
 * 0x85828 → 0x8C7A8 — so a restart revokes the letters). DECODE (r01u):
 * the 0x11/0x16 command states never reach the wire — the earn path's only
 * wire content is the cyclic J6 STAT exchange whose 0x14 reply frame
 * carries the gate bit. Call once per 5 ms pass. */
void sup_selftest_step(void);

/* enable/disable a monitor channel: letters[ch] = 'c' or 0
 * (stock 0x8C7A8 mon2_chan_enable) */
void mon2_chan_enable(uint8_t ch, bool enable);

/* FE13 sub-16 bench arm, TAMPER-INTERLOCKED (r01u bench, 2026-11-11):
 * REFUSED while the phase-2 gate is closed (CELL_SSLB0_BIT2 == 0) — with
 * the gate closed the staged 0x9020 is a certification write in a state
 * the stock can never produce (0x91758 folds only earned letter bits),
 * and the chip answers with the latched anti-tamper reset loop (~1.15 s
 * period, outlives RAM, until power-off). Accepted arms set the slot-0
 * request cell (0x85E64 law); the per-pass seq2 law performs the enable
 * and a later gate close revokes it like any ladder restart. Disarm
 * withdraws the request (letters[0] = 0). */
void sup_letters_bench_arm(bool on);

/* restart the ladder from phase 0 (the stock's mode-sequencer re-entry law,
 * 0x85160 → 0x8B7A8; the reduction's seam for it): the next pass re-runs
 * phase 1, whose init clears the request cells (0x85E64(ch,0)) — earned
 * letters are withdrawn and only re-earned through the gate. Bench stop. */
void sup_letters_reladder(void);

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
