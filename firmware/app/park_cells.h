/* park_cells.h — the range/park module's cells at the stock's RAM addresses
 * (app/range.c, app/park_law.c; ~/tcu_sweep/range_logic_port.md). Every
 * address is the stock's, from the bytes of 0x9B228 / 0xA548C / 0xB912C /
 * 0xCFFDE / 0xE88FA / 0xE89D0 / 0xE89DE and the driving RAM dump
 * stockram_driving_20261104.bin (P, no lever, this bench), so a witness read
 * compares 1:1 with the dump:
 *   0xFFF8E549  arbiter result object (A548C's param, 0x9B254): flag,
 *               target, reason               dump 00 08 17 (Auto-P)
 *   0xFFF8D6D6  park target + complement     dump 08 F7
 *   0xFFF89C6C  0xB912C: the target as read  dump 08
 *   0xFFF89CDC  0xB97F8: with diag overrides dump 08
 *   0xFFF8AE28  0xCFFDE: direction, PARK REQUEST (AE29)   dump 00 01
 *   0xFFF8BC70  the park object: model state, LOCK, timer, interlock window
 *                                            dump 04 01 FF 00
 *   0xFFF851A8  output flags [k] (k = 1..4; 2 release, 3 hold), complements
 *               at 0xFFF851AD                dump 00 ×5 / FF ×5
 * Linker reservations: .park_* in linker.ld (NOLOAD, cleared by park_init).
 * The RAM application (legacy, linker_ram.ld) keeps them in .bss instead:
 * its image runs over the stock cell window. */
#ifndef TCU_PARK_CELLS_H
#define TCU_PARK_CELLS_H

#include <stdint.h>

#if defined(TCU_RAM_APP)
extern volatile uint8_t g_park_cells[32];
#define PARK_CELL_PTR(off)  (&g_park_cells[(off)])
#define PC_OFF_ARB   0u
#define PC_OFF_TGT   4u
#define PC_OFF_RNG   6u
#define PC_OFF_RNGD  7u
#define PC_OFF_AE28  8u
#define PC_OFF_OBJ   12u
#define PC_OFF_OUT   16u
#define PC_OFF_OUTC  21u
#define PCELL_ARB    PARK_CELL_PTR(PC_OFF_ARB)
#define PCELL_TGT    (*PARK_CELL_PTR(PC_OFF_TGT))
#define PCELL_TGT_C  (*PARK_CELL_PTR(PC_OFF_TGT + 1u))
#define PCELL_RNG    (*PARK_CELL_PTR(PC_OFF_RNG))
#define PCELL_RNGD   (*PARK_CELL_PTR(PC_OFF_RNGD))
#define PCELL_AE28   (*PARK_CELL_PTR(PC_OFF_AE28))
#define PCELL_AE29   (*PARK_CELL_PTR(PC_OFF_AE28 + 1u))
#define PCELL_OBJ    PARK_CELL_PTR(PC_OFF_OBJ)
#define PCELL_OUT    PARK_CELL_PTR(PC_OFF_OUT)
#define PCELL_OUT_C  PARK_CELL_PTR(PC_OFF_OUTC)
#else
#define PCELL_ARB    ((volatile uint8_t *)0xFFF8E549u)          /* [3] */
#define PCELL_TGT    (*(volatile uint8_t *)0xFFF8D6D6u)
#define PCELL_TGT_C  (*(volatile uint8_t *)0xFFF8D6D7u)
#define PCELL_RNG    (*(volatile uint8_t *)0xFFF89C6Cu)
#define PCELL_RNGD   (*(volatile uint8_t *)0xFFF89CDCu)
#define PCELL_AE28   (*(volatile uint8_t *)0xFFF8AE28u)
#define PCELL_AE29   (*(volatile uint8_t *)0xFFF8AE29u)
#define PCELL_OBJ    ((volatile uint8_t *)0xFFF8BC70u)          /* [4] */
#define PCELL_OUT    ((volatile uint8_t *)0xFFF851A8u)          /* [5] */
#define PCELL_OUT_C  ((volatile uint8_t *)0xFFF851ADu)          /* [5] */
#endif

/* the park object 0xFFF8BC70 */
#define PO_STATE 0u     /* model: 1 released, 2 releasing, 3 engaging, 4 engaged (0 at boot) */
#define PO_LOCK  1u     /* the lock decision: 1 = park hold OFF */
#define PO_TIMER 2u     /* 10 ms ticks since the lock last changed (saturates 255) */
#define PO_WIN   3u     /* interlock window countdown (10 ms ticks) */

/* output flag index k (0xE89D0) */
#define PK_RELEASE 2u   /* 0xFFF851AA → 0xE8888 id 2 → actuator ch 7 */
#define PK_HOLD    3u   /* 0xFFF851AB → 0xE8888 id 3 → actuator ch 11 (TOE31) */

#endif /* TCU_PARK_CELLS_H */
