/* drv_cells.h — the solenoid request-law cells at the stock's RAM addresses
 * (drv_reqlaw.c; ~/tcu_sweep/request_law_port.md, solenoid_chain_decode.md
 * §B.3). Every address is the stock's, from the bytes of 0x83DB0 / 0x89540
 * / 0x901A0 / 0x90118 / 0x924B0 / 0x92344 and the driving RAM dump
 * stockram_driving_20261104.bin.
 *
 * Reservations: cells inside .sup_value (0x94E5C..0x95013) and .sup_cells
 * (0x90740..0x9097F) are already pinned and cleared by sup_cells_init; the
 * rest have their own NOLOAD sections in linker.ld (.drv_rq1 .. .drv_rq6)
 * and are cleared by drv_rq_init. None of them is .bss — the stack guard
 * (__noinit_end <= 0xFFF9F000) does not pay for them.
 */
#ifndef TCU_DRV_CELLS_H
#define TCU_DRV_CELLS_H

/* THE TEMPERATURE-SELECTION SHADOW IS READ-ONLY (app/tsel_shadow.c defines
 * TCU_TSEL_SHADOW_TU): it may not include a live header */
#if defined(TCU_TSEL_SHADOW_TU)
#error "the temperature-selection shadow is read-only: it may not include a live header (drv_cells.h)"
#endif

/* THE FEEDBACK SHADOW IS READ-ONLY (app/fb_shadow.c defines TCU_FB_SHADOW_TU):
 * it may not include a live output header */
#if defined(TCU_FB_SHADOW_TU)
#error "the feedback shadow is read-only: it may not include a live output header (drv_cells.h)"
#endif

/* THE RANGE CHAIN IS ISOLATED (app/rng_chain.c, ours): its translation
 * unit defines TCU_RNG_CHAIN_TU and may not include an output-side header —
 * its only way to the outputs is the range D6D6, read by app/park.c */
#if defined(TCU_RNG_CHAIN_TU)
#error "the range chain is isolated: it may not include an output-side header (drv_cells.h)"
#endif

#include <stdint.h>

#define DRV_RQ_NCH          10u                 /* 0x83DB0 loops ch 0..9 */
#define DRV_RQ_NCHIP        5u                  /* 0x901A0 loops chips 0..4 */

/* ── 0x83DB0 inputs / outputs (.sup_value) ─────────────────────────────── */
/* THE REQUEST INPUT 0xFFF94EE8[10] (0.1 mA; stock writer 0x7B6D8). On SH
 * the cell is READ-ONLY by type and drv_reqlaw.c never reads it: every
 * request is a compile-time 0 (drv_reqlaw.h, the safety frame). */
#if defined(__sh__)
#define CELL_RQ_REQ         ((const volatile uint16_t *)0xFFF94EE8u)
#else
#define CELL_RQ_REQ         ((volatile uint16_t *)0xFFF94EE8u)
#endif
#define CELL_RQ_I           ((volatile uint16_t *)0xFFF94ED4u)  /* [10] gated current (0x840C4) */
#define CELL_RQ_I2          ((volatile uint16_t *)0xFFF94F58u)  /* [10] current, then 0x89540's corrected copy */
#define CELL_RQ_ACTIVE      ((volatile uint8_t  *)0xFFF94FA0u)  /* [10] request != 0 && no fault */
#define CELL_RQ_MODE        ((volatile uint8_t  *)0xFFF94FD8u)  /* [10] 0 / 1 off / 2 / 3 current / 4 full on */
#define CELL_RQ_UVFLAGS     (*(volatile uint16_t *)0xFFF93978u) /* rail-undervoltage per-channel bits */
#define CELL_RQ_FAULTMASK   (*(volatile uint16_t *)0xFFF9397Au) /* channels the 0x7BA76 class blocked */
#define CELL_RQ_FULLMASK    (*(volatile uint16_t *)0xFFF9397Cu) /* channels in a full-on test */

/* ── 0x89540 / 0x837E0 ─────────────────────────────────────────────────── */
#define CELL_RQ_LETTER      ((volatile uint8_t  *)0xFFF90812u)  /* [10] 0 / '!' / 'C' / '2' */
#define CELL_RQ_CUR         ((volatile uint16_t *)0xFFF90230u)  /* [10] corrected current, <= 0x2FFA */
#define CELL_RQ_CORR        ((volatile int16_t  *)0xFFF9337Cu)  /* [10] 0x837E0 adaptive correction */
#define CELL_RQ_CORR_LAST   (*(volatile int16_t  *)0xFFF93A12u) /* 0x89540: the last correction */

/* ── 0x901A0 / 0x90118 (.sup_cells unless marked) ──────────────────────── */
#define CELL_RQ_LET_APPLIED ((volatile uint8_t  *)0xFFF9081Cu)  /* [10] letters as applied */
#define CELL_RQ_CUR_SENT    ((volatile uint16_t *)0xFFF90244u)  /* [10] 0x90118's current (.drv_rq1) */
#define CELL_RQ_DITHER      ((volatile uint16_t *)0xFFF90320u)  /* [10] dither demand (0x89648; .drv_rq2) */
#define CELL_RQ_DITH_ST     ((volatile uint8_t  *)0xFFF90830u)  /* [10] dither state 0 / 2 */
#define CELL_RQ_ROUND       (*(volatile uint8_t  *)0xFFF9090Au) /* 0x90118 rounding offset 0 → 4 ↔ 1 */
#define CELL_RQ_REDFREQ     (*(volatile uint8_t  *)0xFFF9091Bu) /* reduced-frequency state 0 / 1 / 2 */
#define CELL_RQ_CFG96       ((volatile uint8_t  *)0xFFF908FBu)  /* [5] 0x96 register byte (0x55) */
#define CELL_RQ_CFG8E       ((volatile uint8_t  *)0xFFF90900u)  /* [5] 0x8E register byte (0x9F) */
#define CELL_RQ_CFG90       ((volatile uint8_t  *)0xFFF90905u)  /* [5] 0x90 register byte (0x21) */

/* the per-chip driver records 0xFFF91108 + 0x1C·chip (.drv_rec) */
#define DRV_REC(c)          (0xFFF91108u + 0x1Cu * (uint32_t)(c))
#define CELL_REC_U8(c, o)   (*(volatile uint8_t  *)(uintptr_t)(DRV_REC(c) + (o)))
#define CELL_REC_U16(c, o)  (*(volatile uint16_t *)(uintptr_t)(DRV_REC(c) + (o)))
#define REC_ID_LO           0x04u   /* chip ID (0x921E0 rx[5]; not harvested here) */
#define REC_ID_HI           0x05u   /* chip ID (0x921E0 rx[0]) */
#define REC_MODE            0x07u   /* 0x88 register byte (both channels) */
#define REC_CFG8E           0x09u
#define REC_CFG90           0x0Au
#define REC_CFG96           0x0Bu
#define REC_SP0             0x0Cu   /* u16, channel 2c setpoint (0xE8 row) */
#define REC_DITH0           0x10u   /* u8, 0xA0 row */
#define REC_SP1             0x14u   /* u16, channel 2c+1 setpoint (0xF0 row) */
#define REC_DITH1           0x18u   /* u8, 0xA6 row */

/* ── M4 group A: the switch-output law 0x849D4 → 0x8ADF4 → 0x91500 ────────
 * (drv_groupa.c; ~/tcu_sweep/groupA_park_port.md, park_outputs_decode.md
 * §2). Outputs k = 0..4 = TOE30 (slot 10), TOE31 park hold (slot 11),
 * TOE33 (12), TOE32 (13), TOE22 (14). Reservations: .sup_value (0x94F9C..
 * 0x95004, complements 0x94E22.. / 0x94E62..), .sup_l1f (0x94110..0x94115),
 * .sup_cnt (0x9480B), .sup_cells (0x9086A..0x9089B) and the new .drv_ga1
 * (0x93A66). Every address from the stock bytes. */
#define DRV_GA_N            5u
#define CELL_GA_FIRST       (*(volatile uint8_t  *)0xFFF94110u) /* 1 from the end of pass 1 (0x8B284 gate) */
#define CELL_GA_ALLOWED     (*(volatile uint8_t  *)0xFFF94111u) /* supply-allowed bits ('U' / battery) */
#define CELL_GA_FAULT       (*(volatile uint8_t  *)0xFFF94112u) /* fault-class bits (0x7BB00 ∉ {0,1}) */
#define CELL_GA_FULL        (*(volatile uint8_t  *)0xFFF94113u) /* full-on test bits (mode 2) */
#define CELL_GA_DEMAND      ((volatile uint16_t *)0xFFF94FAAu)  /* [5] demand (0x7B6D8 slots 10..14) */
#define CELL_GA_DEMAND_C    ((volatile uint16_t *)0xFFF94E22u)  /* [5] its complement */
#define CELL_GA_GATED       ((volatile uint16_t *)0xFFF94FC0u)  /* [5] demand as gated (0x8ADF4 type 2) */
#define CELL_GA_DEMPRES     ((volatile uint8_t  *)0xFFF94FF6u)  /* [5] demand present, no fault */
#define CELL_GA_MODE_C      ((volatile uint8_t  *)0xFFF94FFBu)  /* [5] mode copy */
#define CELL_GA_MODE        ((volatile uint8_t  *)0xFFF95000u)  /* [5] 0 off / 1 demand / 2 test full on */
#define CELL_GA_PULLIN      ((volatile uint8_t  *)0xFFF9480Bu)  /* [5] pull-in countdown (0x7C7E4 bytes) */
#define CELL_GA_PREV        ((volatile uint16_t *)0xFFF93A66u)  /* [5] last duty (pull-in re-arm) */
#define CELL_GA_DUTY        ((volatile uint16_t *)0xFFF9086Au)  /* [5] duty ‰ → 0x91500 */
#define CELL_GA_DUTY2       ((volatile uint16_t *)0xFFF94FB4u)  /* [5] duty ‰ copy */
#define CELL_GA_TEMP_COPY   (*(volatile uint16_t *)0xFFF94FBEu) /* 0x94F9C as used by 0x8ADF4 */
#define CELL_GA_FS_PREV     ((volatile uint16_t *)0xFFF90874u)  /* [5] 0x91500: duty last written (k >= 2) */
#define CELL_GA_FS_DUTY     ((volatile uint16_t *)0xFFF9087Eu)  /* [5] 0x91500: duty this pass */
#define CELL_GA_FS_CYC      ((volatile uint16_t *)0xFFF90888u)  /* [5] cycle wanted (cal 0x1914C2) */
#define CELL_GA_FS_CYCNOW   ((volatile uint16_t *)0xFFF90892u)  /* [5] cycle running (0x9136E) */
#define CELL_GA_OFF10       (*(volatile uint8_t  *)0xFFF94FEAu) /* 1 = slot 10 (TOE30) forced 0 (safety layer) */
#define CELL_GA_OFF10_C     (*(volatile uint8_t  *)0xFFF94E62u)
#define CELL_GA_OFF13       (*(volatile uint8_t  *)0xFFF94FEBu) /* 1 = slot 13 (TOE32) forced 0 */
#define CELL_GA_OFF13_C     (*(volatile uint8_t  *)0xFFF94E63u)
#define CELL_GA_BATT_FF     (*(volatile uint16_t *)0xFFF94F9Eu) /* source 0xFF supply (no output uses it) */

/* ── M4 the hold-law temperature 0xFFF94F9C (drv_temp.c): 0x887D8 publishes
 * it from the fluid-temperature acquisition 0x8D5BA (AN8 sensor, AN1
 * reference, curve 0x1912F0). 1/16 °C. Reservations: .sup_value (0x94F98..
 * 0x94F9C, 0x94FEE..0x94FF0; complements 0x94E10.. / 0x94E66..), .sup_l1f
 * (0x940FC, 0x94102/3, 0x94155..0x94159), .sup_l1b (0x9393A), .sup_cnt
 * (0x94770, 0x94822), .drv_tmp1 (0x93A9C..), .drv_tmp2 (0x9416E/F). */
#define CELL_TMP_RAW        (*(volatile int16_t  *)0xFFF94F9Au) /* 0x93AA0 as published */
#define CELL_TMP_RAW_C      (*(volatile uint16_t *)0xFFF94E12u)
#define CELL_TMP_SEL        (*(volatile int16_t  *)0xFFF94F98u) /* raw or substitute */
#define CELL_TMP_SEL_C      (*(volatile uint16_t *)0xFFF94E10u)
#define CELL_TMP            (*(volatile int16_t  *)0xFFF94F9Cu) /* THE hold-law temperature */
#define CELL_TMP_C          (*(volatile uint16_t *)0xFFF94E14u)
#define CELL_TMP_SEL_ST     (*(volatile uint8_t  *)0xFFF94FEEu) /* status of 0x94F98 (8 ok / 7 hold / 3 default / 6 subst.) */
#define CELL_TMP_SEL_ST_C   (*(volatile uint8_t  *)0xFFF94E66u)
#define CELL_TMP_RAW_ST     (*(volatile uint8_t  *)0xFFF94FEFu)
#define CELL_TMP_RAW_ST_C   (*(volatile uint8_t  *)0xFFF94E67u)
#define CELL_TMP_ST         (*(volatile uint8_t  *)0xFFF94FF0u) /* status of 0x94F9C */
#define CELL_TMP_ST_C       (*(volatile uint8_t  *)0xFFF94E68u)
#define CELL_TMP_RATIO      (*(volatile uint16_t *)0xFFF93A9Cu) /* (5000 - diff) x 1000 / ref, cap 40000 */
#define CELL_TMP_FLAGS      (*(volatile uint16_t *)0xFFF93A9Eu) /* plausibility bits 0x01..0x800 */
#define CELL_TMP_ACQ        (*(volatile int16_t  *)0xFFF93AA0u) /* curve(sensor) - 0x8000, 1/16 °C */
#define CELL_TMP_DDIFF      (*(volatile uint16_t *)0xFFF93AA2u) /* diff - previous diff */
#define CELL_TMP_SENS       (*(volatile uint16_t *)0xFFF93AA4u) /* AN8 mV */
#define CELL_TMP_REF        (*(volatile uint16_t *)0xFFF93AA6u) /* AN1 mV */
#define CELL_TMP_DIFF       (*(volatile uint16_t *)0xFFF93AA8u) /* sensor - reference (>= 0) */
#define CELL_TMP_DIFF_PREV  (*(volatile uint16_t *)0xFFF93AAAu)
#define CELL_TMP_9393A      (*(volatile int16_t  *)0xFFF9393Au) /* substitute source (not produced here) */
#define CELL_TMP_940FC      (*(volatile uint8_t  *)0xFFF940FCu) /* its status (not produced here) */
#define CELL_TMP_BYTE_SEL   (*(volatile uint8_t  *)0xFFF94102u) /* (0x94F98 + 8) >> 4 + 40, 0..255 */
#define CELL_TMP_BYTE       (*(volatile uint8_t  *)0xFFF94103u) /* (0x94F9C + 8) >> 4 + 40, 0..255 */
#define CELL_TMP_94155      (*(volatile uint8_t  *)0xFFF94155u)
#define CELL_TMP_VARIANT    (*(volatile uint8_t  *)0xFFF94156u) /* sensor variant index */
#define CELL_TMP_CODE       (*(volatile uint8_t  *)0xFFF94157u) /* coding byte (stock 0xFFF84796) */
#define CELL_TMP_VAR_RAW    (*(volatile uint8_t  *)0xFFF94158u)
#define CELL_TMP_ACQ_ST     (*(volatile uint8_t  *)0xFFF94159u) /* 8 ok / 7 hold / 3 default */
#define CELL_TMP_9416E      (*(volatile uint8_t  *)0xFFF9416Eu)
#define CELL_TMP_9416F      (*(volatile uint8_t  *)0xFFF9416Fu)
#define CELL_CNT_TMP_HOLD   (*(volatile uint16_t *)0xFFF94770u) /* 0x7C7E4 word: fault hold (20 = 100 ms) */
#define CELL_CNT_TMP_START  (*(volatile uint8_t  *)0xFFF94822u) /* 0x7C7E4 byte: start-up grace (2) */
/* the AN table (0x8F70C; plat_adcmux.c): AN0..AN27 in mV */
#define CELL_AN_MV(n)       (*(volatile uint16_t *)(uintptr_t)(0xFFF9018Cu + 2u * (uint32_t)(n)))

/* ── M5 the temperature byte, live (2026-11-17; drv_temp.c, app/tmp_chain.c,
 * safety/tmp_safe.c; ~/tcu_sweep/temp_byte_live_port.md): 0x887D8's fault
 * tail → 0x7BF8C (the fault word and the published fault flags), 0x7C100
 * the published bytes the safety layer gathers. Reservations: .tmp_93914,
 * .tmp_940e0 (the fault code / mask cells are drv_temp.c's own). */
#define CELL_TMP_FWORD      (*(volatile uint16_t *)0xFFF93914u) /* 0x7BF8C: the fault word */
#define CELL_TMP_PUB_E0     (*(volatile uint8_t  *)0xFFF940E0u) /* 0x7C100: 0x940E2 ? 0xFF : 0x940FE */
#define CELL_TMP_PUB_E2     (*(volatile uint8_t  *)0xFFF940E2u) /* that source's fault flags (not produced: stand-in) */
#define CELL_TMP_PUB_E3     (*(volatile uint8_t  *)0xFFF940E3u) /* 0x7C100: 0x940E5 ? 0xFF : 0x94101 */
#define CELL_TMP_PUB_E5     (*(volatile uint8_t  *)0xFFF940E5u) /* that source's fault flags (not produced: stand-in) */
#define CELL_TMP_PUB_E6     (*(volatile uint8_t  *)0xFFF940E6u) /* 0x7C100: 0x940E8 ? 0xFF : 0x94102 — THE fluid byte */
#define CELL_TMP_PUB_E7     (*(volatile uint8_t  *)0xFFF940E7u) /* 0x7BF8C: the tested mask (0x87) */
#define CELL_TMP_PUB_E8     (*(volatile uint8_t  *)0xFFF940E8u) /* 0x7BF8C: the fluid sensor's fault flags */
#define CELL_TMP_940FE      (*(volatile uint8_t  *)0xFFF940FEu) /* a third temperature byte (not produced) */
#define CELL_TMP_94101      (*(volatile uint8_t  *)0xFFF94101u) /* the second temperature byte 0x88DA8 (not produced) */

#endif /* TCU_DRV_CELLS_H */
