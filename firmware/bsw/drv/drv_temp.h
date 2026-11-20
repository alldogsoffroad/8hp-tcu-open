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
 *   (2026-11-17, ~/tcu_sweep/temp_byte_live_port.md) 0x887D8's fault tail
 *     @0x8896A → 0x7BF8C (fault word 0x93914, flags 0x940E7 / 0x940E8), and
 *   0x7C100 (the 0x7AB48 poll group): the published bytes 0x940E0 / 0x940E3
 *     / 0x940E6 the safety layer gathers (safety/tmp_safe.c).
 */
#ifndef TCU_DRV_TEMP_H
#define TCU_DRV_TEMP_H

/* THE TEMPERATURE-SELECTION SHADOW IS READ-ONLY (app/tsel_shadow.c defines
 * TCU_TSEL_SHADOW_TU): it may not include a live header */
#if defined(TCU_TSEL_SHADOW_TU)
#error "the temperature-selection shadow is read-only: it may not include a live header (drv_temp.h)"
#endif
#if defined(TCU_SM_SHADOW_TU)
#error "the shift-map shadow is read-only: it may not include a live header (drv_temp.h)"
#endif
#if defined(TCU_FR_SHADOW_TU)
#error "the fault-reaction shadow is read-only: it may not include a live header (drv_temp.h)"
#endif
#if defined(TCU_GP_SHADOW_TU)
#error "the gear-pair shadow is read-only: it may not include a live header (drv_temp.h)"
#endif
#if defined(TCU_LP_SHADOW_TU)
#error "the line-pressure shadow is read-only: it may not include a live header (drv_temp.h)"
#endif
#if defined(TCU_TI_SHADOW_TU)
#error "the torque-intervention shadow is read-only: it may not include a live header (drv_temp.h)"
#endif
#if defined(TCU_WK_SHADOW_TU)
#error "the lock-up shadow is read-only: it may not include a live header (drv_temp.h)"
#endif

#include <stdint.h>

/* 0x88A50 (call after sup_cells_init) */
void drv_temp_init(void);

/* the reply proc's head 0x8F70C (AN table) — 0x8EE52's first call */
void drv_temp_an_tick(void);

/* 0x887D8 (5 ms entry 0x7CB6E) */
void drv_temp_tick(void);

/* 0x7C100 (the 0x7AB48 poll group, right after 0x7B150: 5 ms in OS mode 0,
 * 10 ms in modes 1..3) */
void drv_temp_pub_tick(void);

/* the stand-in of the die / board fault flags 0x940E2 / 0x940E5 (held
 * nonzero = "faulted" by drv_temp_init) in every build WITHOUT
 * TSEL_LIVE_FLAGS (bsw/drv/drv_tsel.h; stage 2b makes them live) */
#define TMP_STANDIN_SRC_FAULTED 0x80u

/* the unsigned curve 0x8ED20 → 0x8ED60 ({ n, x[n], y[n] }, round half up) */
uint16_t drv_temp_curve_u16(const uint16_t *tbl, uint16_t x);

#endif /* TCU_DRV_TEMP_H */
