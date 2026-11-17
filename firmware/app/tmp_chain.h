/* tmp_chain.h — THE TEMPERATURE BYTE 0xFFF89C13, live (2026-11-17;
 * ~/tcu_sweep/temp_byte_live_port.md): the application end of the stock's
 * chain, ported from the bytes, and the scheduling of the whole chain at
 * the stock rates and order:
 *
 *   0x887D8  (5 ms mode 0 / 10 ms modes 1..3)  0x94102 = round(0x94F98/16)+40,
 *            the fault code → 0x7BF8C → 0x940E8         (bsw/drv/drv_temp.c)
 *   0x7C100  (same task, the 0x7AB48 group)  0x940E6 = 0x940E8 ? 0xFF : 0x94102
 *   safety layer (safety/tmp_safe.c): mode 0 → ED20 re-initialised every
 *            10 ms; boot state 1 → E998 conditioned directly; modes 1..3 →
 *            every 100 ms: gather, slew limit, invalid flags → 0xFFF8E9C8
 *   0xB9646  (30 ms modes 1..3, list 0x4D940 entry 19; and the boot pass,
 *            row-1 entry 6 0xA2D88): 0x89C57 = 0x89CD2 = 0x140050() = E9C8
 *   0xB986C  (10 ms modes 1..3, list 0x4D834 entry 29 0xA2EFC — before the
 *            producer at entry 42; and the boot pass): 0x89C13 = 0x89CD2
 *
 * Boot state 1 (row-1 list 0x4D7B0): entry 0 0x14F32C (E998), entry 6
 * 0xA2D88 (0xB9646 then 0xB986C), entry 17 0xA2E74 (the producer): the
 * producer's boot pass sees the directly conditioned byte.
 */
#ifndef TCU_APP_TMP_CHAIN_H
#define TCU_APP_TMP_CHAIN_H

#include <stdint.h>

#define TMP_CELL_89C13  (*(volatile uint8_t *)0xFFF89C13u)   /* the producer's temperature byte */
#define TMP_CELL_89C57  (*(volatile uint8_t *)0xFFF89C57u)   /* 0xB9646 +0x1B */
#define TMP_CELL_89CD2  (*(volatile uint8_t *)0xFFF89CD2u)   /* 0xB9646 +0x96 (the fill's 0xE4424 reads it) */

/* init (after drv_temp_init): the safety objects (startup + mode-0 init list) */
void tmp_chain_init(void);
/* boot state 1 (sup_mode.c, before the producer's boot pass) */
void tmp_chain_boot_pass(void);
/* the 10 ms task, BEFORE the producer: mode 0 → 0x132DCE (ED20); modes
 * 1..3 → 0xB986C */
void tmp_chain_task_10ms(uint8_t os_mode);
/* the 30 / 100 ms tasks of modes 1..3, on the same tick as the 10 ms task
 * and after it (lower priority); their countdowns are copied from the 10 ms
 * one at the mode switch (0x67A08), so both fire on the first mode-1 tick */
void tmp_chain_task_slow(uint8_t os_mode);

/* 0xB9646 / 0xB986C (exported for the host test) */
void tmp_chain_b9646(void);
void tmp_chain_b986c(void);

#endif /* TCU_APP_TMP_CHAIN_H */
