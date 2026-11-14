/* drv_temp.h — the producer of the group-A hold-law temperature 0xFFF94F9C
 * (1/16 °C), ported 1:1 from the stock bytes (drv_temp.c;
 * ~/tcu_sweep/groupA_park_port.md §3):
 *   0x88A50 / 0x8D57C  init (row-9 list entry 0x7CB5C → 0x832AC)
 *   0x887D8            the publisher (entry 0x7CB6E → 0x83300: 5 ms in OS
 *                      mode 0, 10 ms in mode 1, right after the reply proc)
 *     0x8D5BA          the fluid-temperature acquisition: AN8 (sensor) and
 *                      AN1 (reference) from the AN table 0x9018C, the
 *                      plausibility bits, curve 0x1912F0 (0x8ED20)
 *     0x8D4B4          0x94F9C = 0x94F98 unless fault path 0x65 is active
 *   0x8F70C (ADC_A part, platform: plat_adcmux_an_convert) at the head of
 *   the reply proc 0x8EE52 — the AN table from the 1 ms ADC_A sums.
 */
#ifndef TCU_DRV_TEMP_H
#define TCU_DRV_TEMP_H

#include <stdint.h>

/* 0x88A50 (call after sup_cells_init) */
void drv_temp_init(void);

/* the reply proc's head 0x8F70C (AN table) — 0x8EE52's first call */
void drv_temp_an_tick(void);

/* 0x887D8 (5 ms entry 0x7CB6E) */
void drv_temp_tick(void);

/* the unsigned curve 0x8ED20 → 0x8ED60 ({ n, x[n], y[n] }, round half up) */
uint16_t drv_temp_curve_u16(const uint16_t *tbl, uint16_t x);

#endif /* TCU_DRV_TEMP_H */
