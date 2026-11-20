/* ti_cal.h - the stock constants the READ-ONLY torque-intervention shadow (app/ti_shadow.c)
 * reads from BELOW 0xA0000, where our application may overwrite the stock image (generated
 * by tools/gen_ti_cal.py; the host test ti_cal_bytes re-derives every table from the stock
 * image). Every pointer WORD inside a block points at the stock calibration at 0x19F000 and
 * above, which is read IN PLACE through app/cal_inplace.h. Variant 0 only (*0xFFF89C04 = 0).
 * (2026-11-20, ~/tcu_sweep/ti_amounts_wk_decode.md, port ~/tcu_sweep/ti_amounts_port.md) */
#ifndef TCU_APP_TI_CAL_H
#define TCU_APP_TI_CAL_H

#if defined(TCU_OUTPUT_PATH)
#error "the torque-intervention shadow is read-only: an output path may not include ti_cal.h"
#endif

#include <stdint.h>

/* per-type index tables: value 0xFF = out of range (the stock's 0xB75B4 report) */
extern const uint8_t TI_IDX_GEAR[10];       /* 0x50804 [0x89C26] -> gear block (9) */
extern const uint8_t TI_IDX_UP[81];         /* 0x5082C [from * 9 + to] -> up block (7) */
extern const uint8_t TI_IDX_DN4[81];        /* 0x50970 -> down block of BD17 == 4 (7) */
extern const uint8_t TI_IDX_DN[81];         /* 0x50AB4 -> down block (18) */
extern const uint8_t TI_IDX_584[5][81];     /* 0xE584E: class 0 (else) 0x50BF8, 1..4: 0x50D3C 0x50E80 0x50FC4 0x51108 -> (35) */
extern const uint8_t TI_IDX_5814[4][81];    /* 0xE5814: class 0 (else) 0x5124C, 1..3: 0x51390 0x514D4 0x51618 -> (28) */
extern const uint8_t TI_IDX_5892[4][81];    /* 0xE5892: class 0 (else) 0x5175C, 1: 0x518A0, 2: 0x519E4, 3 (= class 4): 0x51B28 -> (61) */

/* variant-0 blocks: pointer words (0 = the stock's self-pointer "no map") */
extern const uint32_t TI_B_GEAR[9][13];     /* 0x55868: +0 .. +0x30 */
extern const uint32_t TI_B_UP[7][13];       /* 0x55D6C */
extern const uint32_t TI_B_584[35][4];      /* 0x55AE4 */
extern const uint32_t TI_B_5814[28][10];    /* 0x55440 */
extern const uint32_t TI_B_DN[18][4];       /* 0x56E0C */
extern const uint32_t TI_B_DN4[7][9];       /* 0x55658 */
extern const uint32_t TI_B_5892[61][14];    /* 0x56B00 */

#endif
