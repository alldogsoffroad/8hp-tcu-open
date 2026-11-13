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

#endif /* TCU_DRV_CELLS_H */
