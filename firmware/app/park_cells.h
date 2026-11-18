/* park_cells.h — the range/park module's cells at the stock's RAM addresses
 * (app/range.c, app/park_law.c; ~/tcu_sweep/range_logic_port.md). Every
 * address is the stock's, from the bytes of 0x9B228 / 0xA548C / 0xB912C /
 * 0xCFFDE / 0xE88FA / 0xE89D0 / 0xE89DE and the driving RAM dump
 * stockram_driving_20261104.bin (P, no lever, this bench), so a witness read
 * compares 1:1 with the dump:
 *   0xFFF8E549  init arbiter result (A548C's param, 0x9B254, run ONCE
 *               by the 0x9B228 one-shot): flag, target, reason
 *                                            dump 00 08 17 (Auto-P)
 *   0xFFF8D6D6  the range (the cyclic chain's, app/rng_chain.c) + its
 *               complement; 0xFFF8D6D9 its reason      dump 08 F7 / 00
 *   0xFFF89C6C  0xB912C: the target as read  dump 08
 *   0xFFF89CDC  0xB97F8: with diag overrides dump 08
 *   0xFFF8AE28  0xCFFDE: direction, PARK REQUEST (AE29)   dump 00 01
 *   0xFFF8BC70  the park object: model state, LOCK, timer, interlock window
 *                                            dump 04 01 FF 00
 *   0xFFF851A8  output flags [k] (k = 1..4; 2 release, 3 hold), complements
 *               at 0xFFF851AD                dump 00 ×5 / FF ×5
 * Linker reservations: .park_* in linker.ld (NOLOAD, cleared by park_init);
 * D6D6..D6D9 and E549..E54B lie in the chain's .rng_d6ca / .rng_e530.
 * The RAM application (legacy, linker_ram.ld) keeps them in .bss instead:
 * its image runs over the stock cell window. */
#ifndef TCU_PARK_CELLS_H
#define TCU_PARK_CELLS_H

/* THE GARAGE SHADOW IS READ-ONLY (app/gs_shadow.c defines TCU_GS_SHADOW_TU):
 * it may not include a live output header */
#if defined(TCU_GS_SHADOW_TU)
#error "the garage shadow is read-only: it may not include a live output header (park_cells.h)"
#endif

/* THE RANGE WRITER GUARD (ours, 2026-11-17, ~/tcu_sweep/
 * range_chain_live_port.md): the range D6D6 / D6D7 (+ the reason D6D9) and
 * the init arbiter's E549..E54B belong to the stock's cyclic chain
 * (app/rng_chain.c). Here they are READ-ONLY (const) — a write is a
 * compile error — and the writer macros (*_W) exist only in the chain's
 * translation unit (TCU_RNG_CHAIN_TU; used by its RAM-app stub, the target
 * build writes its own pinned objects). The ctest range_chain_writer_scan
 * refuses the addresses in every other firmware source. */
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
#define PC_OFF_RSN   3u                 /* the RAM app has no chain: reason stays 0 */
#define PC_OFF_AUX   26u                /* 0x89CF0..0x89CF3 */
#define PCELL_ARB    ((const volatile uint8_t *)PARK_CELL_PTR(PC_OFF_ARB))
#define PCELL_TGT    (*(const volatile uint8_t *)PARK_CELL_PTR(PC_OFF_TGT))
#define PCELL_TGT_C  (*(const volatile uint8_t *)PARK_CELL_PTR(PC_OFF_TGT + 1u))
#define PCELL_RSN    (*(const volatile uint8_t *)PARK_CELL_PTR(PC_OFF_RSN))
#if defined(TCU_RNG_CHAIN_TU)
#define PCELL_ARB_W   PARK_CELL_PTR(PC_OFF_ARB)
#define PCELL_TGT_W   (*PARK_CELL_PTR(PC_OFF_TGT))
#define PCELL_TGT_C_W (*PARK_CELL_PTR(PC_OFF_TGT + 1u))
#endif
#define PCELL_RNG    (*PARK_CELL_PTR(PC_OFF_RNG))
#define PCELL_RNGD   (*PARK_CELL_PTR(PC_OFF_RNGD))
#define PCELL_AE28   (*PARK_CELL_PTR(PC_OFF_AE28))
#define PCELL_AE29   (*PARK_CELL_PTR(PC_OFF_AE28 + 1u))
#define PCELL_OBJ    PARK_CELL_PTR(PC_OFF_OBJ)
#define PCELL_OUT    PARK_CELL_PTR(PC_OFF_OUT)
#define PCELL_OUT_C  PARK_CELL_PTR(PC_OFF_OUTC)
#define PCELL_AUX    PARK_CELL_PTR(PC_OFF_AUX)
#else
#define PCELL_ARB    ((const volatile uint8_t *)0xFFF8E549u)    /* [3] read-only */
#define PCELL_TGT    (*(const volatile uint8_t *)0xFFF8D6D6u)   /* read-only */
#define PCELL_TGT_C  (*(const volatile uint8_t *)0xFFF8D6D7u)   /* read-only */
#define PCELL_RSN    (*(const volatile uint8_t *)0xFFF8D6D9u)   /* read-only: reason */
#if defined(TCU_RNG_CHAIN_TU)
#define PCELL_ARB_W   ((volatile uint8_t *)0xFFF8E549u)
#define PCELL_TGT_W   (*(volatile uint8_t *)0xFFF8D6D6u)
#define PCELL_TGT_C_W (*(volatile uint8_t *)0xFFF8D6D7u)
#endif
#define PCELL_RNG    (*(volatile uint8_t *)0xFFF89C6Cu)
#define PCELL_RNGD   (*(volatile uint8_t *)0xFFF89CDCu)
#define PCELL_AE28   (*(volatile uint8_t *)0xFFF8AE28u)
#define PCELL_AE29   (*(volatile uint8_t *)0xFFF8AE29u)
#define PCELL_OBJ    ((volatile uint8_t *)0xFFF8BC70u)          /* [4] */
#define PCELL_OUT    ((volatile uint8_t *)0xFFF851A8u)          /* [5] */
#define PCELL_OUT_C  ((volatile uint8_t *)0xFFF851ADu)          /* [5] */
#define PCELL_AUX    ((volatile uint8_t *)0xFFF89CF0u)          /* [4]: +0xB4, (+0xB5), +0xB6 = 0x89CF2, +0xB7 */
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
