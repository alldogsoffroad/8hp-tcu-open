/* drv_pksens.h — the park position sensor's switch evaluator, READ-ONLY
 * (2026-11-16; ~/tcu_sweep/park_sensor_decode.md §1, park_sensor_port.md).
 * Ported 1:1 from the stock bytes (sh-elf-objdump of the pristine image):
 *
 *   0x88EB0  init (the LAST entry of the row-9 init list 0x832AC, after
 *            0x88A50 = drv_temp_init): the status pairs 0x94F8E/0x94E06 =
 *            0/FF, then 0x8DEF0 (start countdown 0x94160 = 2, self-test
 *            reload 0x9477E = 100, state pairs 0x94F88/0x94E00 = 0/FF), then
 *            0x7B12C(0x3E, 5) (a no-op for these arguments)
 *   0x89320  the reference latch 0x93942 = 0x907E8 (mux 15, the shared
 *            sensor supply) — hub c 0x83300 entry 2, BEFORE 0x887D8
 *   0x88EF0  the per-input monitor (hub c entry 9, AFTER 0x887D8), calling
 *     0x8DF2E  the evaluator: AN5 / AN6 (inputs 2 / 3) ratiometric against
 *              0x93942 (0x8ED40), state = ratio < 5000 (active low), band
 *              0..5 (edges 100/1900/5000/8000/9500), the supervisor chip's
 *              digital view 0x90958 cross-checked
 *     0x7B07A  the fault-byte writer 0x847F8[k] / 0x847FC[k] (+0x7B05C)
 *   0x7B150  the copy 0x94F88[0..3] → 0x847F4.., 0x94E00[0..3] → 0x84800..
 *            (poll group 0x7AB48: 5 ms list entry 9 in mode 0, 10 ms in
 *            mode 1)
 * Hub c (0x7CB6E) and 0x7AB48 run at 5 ms in OS mode 0 and 10 ms in mode 1
 * (sup_batch.c places them).
 *
 * READ-ONLY: nothing in an output path may read these cells. Every output
 * translation unit (app/range.c, app/park_law.c, app/park.c,
 * safety/park_mon.c, bsw/drv/drv_reqlaw.c, drv_jobs.c, drv_groupa.c)
 * defines TCU_OUTPUT_PATH before its includes; this header then refuses to
 * compile. The ctest pksens_readonly_scan greps those files for the cells
 * as well.
 */
#ifndef TCU_DRV_PKSENS_H
#define TCU_DRV_PKSENS_H

#if defined(TCU_OUTPUT_PATH)
#error "park position sensor is read-only: no output path (range, park law, park demands, bus A, request law, group A) may include drv_pksens.h"
#endif

#include <stdint.h>

/* ── the stock cells (addresses from the bytes; linker .pks_* / .sup_*) ── */
#define CELL_PKS_MUX15      (*(volatile uint16_t *)0xFFF907E8u) /* 0x9360C(0x40904) mux 15 mV (.sup_cells) */
#define CELL_PKS_REF        (*(volatile uint16_t *)0xFFF93942u) /* 0x89320 latched reference (.pks_ref) */
#define CELL_PKS_RATIO      ((volatile uint16_t *)0xFFF934C4u)  /* [6] AN x 10000 / ref (.pks_rat) */
#define CELL_PKS_ERR        ((volatile uint16_t *)0xFFF934D0u)  /* [6] monitor error bits */
#define CELL_PKS_TESTED     ((volatile uint16_t *)0xFFF934DCu)  /* [6] 0x3E when stable */
#define CELL_PKS_STATE      ((volatile uint8_t  *)0xFFF93AC4u)  /* [6] 1 = active (ratio < 5000) */
#define CELL_PKS_STATE_C    ((volatile uint8_t  *)0xFFF93ACAu)  /* [6] its complement */
#define CELL_PKS_DIG        ((volatile uint8_t  *)0xFFF93AD0u)  /* [6] chip view bit (0x90958) */
#define CELL_PKS_BAND       ((volatile uint8_t  *)0xFFF93AD6u)  /* [6] 0..5; valid 1 / 3 */
#define CELL_PKS_LATCH      ((volatile uint8_t  *)0xFFF93AF8u)  /* [6] latched faulty (0x88EF0) */
#define CELL_PKS_BAND_PREV  ((volatile uint8_t  *)0xFFF93AFEu)  /* [6] previous pass's bands */
#define CELL_PKS_K          (*(volatile uint8_t  *)0xFFF9415Fu) /* last analog input evaluated */
#define CELL_PKS_START      (*(volatile uint8_t  *)0xFFF94160u) /* start countdown (2) */
#define CELL_PKS_BANDJ      (*(volatile uint8_t  *)0xFFF94161u) /* band loop index */
#define CELL_PKS_MODE       (*(volatile uint8_t  *)0xFFF94162u) /* 0 start / 8 normal / 2 ref low / 7 self-test */
#define CELL_PKS_ACT        (*(volatile uint8_t  *)0xFFF94163u) /* active mask */
#define CELL_PKS_INVAL      (*(volatile uint8_t  *)0xFFF94164u) /* band-invalid mask */
#define CELL_PKS_DIGM       (*(volatile uint8_t  *)0xFFF94165u) /* digital mask */
#define CELL_PKS_CHG        (*(volatile uint8_t  *)0xFFF94166u) /* digital change mask */
#define CELL_PKS_DIGM_PREV  (*(volatile uint8_t  *)0xFFF94167u) /* previous digital mask */
#define CELL_PKS_MISM       (*(volatile uint8_t  *)0xFFF94168u) /* analog/digital mismatch mask */
#define CELL_PKS_FLAGS      (*(volatile uint8_t  *)0xFFF94169u) /* bit0 started, 1 ref low, 2/3 self-test */
#define CELL_PKS_STABLE     (*(volatile uint8_t  *)0xFFF94171u) /* 0x88EF0: stable pass */
#define CELL_PKS_ACT_PREV   (*(volatile uint8_t  *)0xFFF94172u) /* 0x88EF0: active mask at entry */
#define CELL_PKS_CHGSEEN    (*(volatile uint8_t  *)0xFFF94173u) /* 0x88EF0: change latch (mismatch test) */
#define CELL_PKS_SELFTEST   (*(volatile uint8_t  *)0xFFF9411Du) /* self-test request (dead 0x86916; .sup_l1f) */
#define CELL_PKS_ST_TMR     (*(volatile uint16_t *)0xFFF9477Eu) /* self-test timer (.sup_cnt: 0x7C7E4 word) */
#define CELL_PKS_PAIR       ((volatile uint8_t  *)0xFFF94F88u)  /* [6] published state (.sup_value) */
#define CELL_PKS_PAIR_C     ((volatile uint8_t  *)0xFFF94E00u)  /* [6] its complement (.sup_l1g) */
#define CELL_PKS_ST         ((volatile uint8_t  *)0xFFF94F8Eu)  /* [6] status 8 / 2 (.sup_value) */
#define CELL_PKS_ST_C       ((volatile uint8_t  *)0xFFF94E06u)  /* [6] its complement (.sup_l1g) */
#define CELL_PKS_MTESTED    ((volatile uint8_t  *)0xFFF92FE4u)  /* [4] 0x7B05C(tested) (.pks_mon) */
#define CELL_PKS_MERR       ((volatile uint16_t *)0xFFF92FE8u)  /* [4] error words (.pks_mon) */
#define CELL_PKS_CP_ST      ((volatile uint8_t  *)0xFFF847F4u)  /* [4] 0x7B150 state copy (.pks_cp) */
#define CELL_PKS_CP_FLT     ((volatile uint8_t  *)0xFFF847F8u)  /* [4] 0x7B07A fault byte */
#define CELL_PKS_CP_FLT_C   ((volatile uint8_t  *)0xFFF847FCu)  /* [4] its inverse */
#define CELL_PKS_CP_ST_C    ((volatile uint8_t  *)0xFFF84800u)  /* [4] 0x7B150 complement copy */

/* 0x88EB0 (call after sup_cells_init and drv_temp_init) */
void drv_pksens_init(void);

/* 0x89320 — hub c entry 2 (before drv_temp_tick) */
void drv_pksens_ref_tick(void);

/* 0x88EF0 → 0x8DF2E — hub c entry 9 (after drv_temp_tick) */
void drv_pksens_tick(void);

/* 0x7B150 — the 0x7AB48 poll group */
void drv_pksens_copy_tick(void);

/* witnesses (ours, .pk_wit): the 0x6BDCA pair checks (stock: fatal 0x221
 * via 0x6865C on a torn state / status pair) counted, not fatal */
uint32_t drv_pksens_f221_n(void);
uint32_t drv_pksens_f221_site(void);   /* the torn cell's address */

#endif /* TCU_DRV_PKSENS_H */
