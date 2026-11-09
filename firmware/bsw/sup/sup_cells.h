/* sup_cells.h — the shadow cell map at the stock's RAM addresses (design §10).
 *
 * Every protocol cell the bench reads by address lives here, pinned to the
 * stock address via the linker's .sup_* sections (design §9). Initial values
 * are the stock's init values (spec 22 §6.1 and the bench live reads).
 *
 * Cells are declared as volatile — written from ISR-context handlers and
 * read from task context. Single byte/word stores are atomic on SH-2A.
 */
#ifndef TCU_SUP_CELLS_H
#define TCU_SUP_CELLS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── staging (linker .sup_stage at 0xFFF88400) ────────────────────────── */
#define CELL_J1_STAGED      (*(volatile uint16_t *)0xFFF88424u) /* 0x8E word */
#define CELL_J1_C9_STAGED   (*(volatile uint8_t  *)0xFFF88426u) /* C9 byte   */
#define CELL_J2_STAGED      (*(volatile uint16_t *)0xFFF88434u) /* 90|en     */
#define CELL_J3_STAGED      (*(volatile uint16_t *)0xFFF88444u) /* 28,2E     */
#define CELL_J7_STAGED      (*(volatile uint16_t *)0xFFF885E0u) /* 0000      */

/* ── walk reply cells (linker .sup_walk at 0xFFF90540) ────────────────── */
#define CELL_REPLY_FRESH    (*(volatile uint8_t  *)0xFFF90540u) /* |=1 duty, |=2 level */
#define CELL_TOKEN_LAST     (*(volatile uint32_t *)0xFFF90548u)
#define CELL_LEVEL_COUNT    (*(volatile uint16_t *)0xFFF9054Cu) /* level reply counter */
#define CELL_DUTY_COUNT     (*(volatile uint16_t *)0xFFF9054Du) /* duty reply counter */
#define CELL_ECHO_0A        (*(volatile uint8_t  *)0xFFF9054Fu) /* 0x0A echo */
#define CELL_FB_0C          (*(volatile uint8_t  *)0xFFF90550u) /* 0x0C feedback */
#define CELL_COMMIT_12      (*(volatile uint8_t  *)0xFFF90551u) /* 0x12 commit status */
#define CELL_STAGED_84      (*(volatile uint8_t  *)0xFFF9054Eu) /* staged level value */

/* ── protocol cells (linker .sup_cells at 0xFFF90740) ─────────────────── */
#define CELL_J1_OK          (*(volatile uint8_t  *)0xFFF907C8u)
#define CELL_J1_ERR         (*(volatile uint8_t  *)0xFFF907C9u)
#define CELL_J1_ECHOFAIL    (*(volatile uint8_t  *)0xFFF907CAu)
#define CELL_TELEMETRY      (*(volatile uint16_t *)0xFFF907CCu) /* 10-bit */
#define CELL_TELEM_VALID    (*(volatile uint8_t  *)0xFFF907CEu)
#define CELL_ID_B1_00       (*(volatile uint8_t  *)0xFFF907CFu)
#define CELL_ID_B1_06       (*(volatile uint8_t  *)0xFFF907D0u)
#define CELL_REG_14         (*(volatile uint8_t  *)0xFFF907D1u) /* 0x14 switch status */
#define CELL_STAT_18        (*(volatile uint8_t  *)0xFFF907D2u)
#define CELL_STAT_1E        (*(volatile uint8_t  *)0xFFF907D3u)
#define CELL_CTRL_STAGED    (*(volatile uint8_t  *)0xFFF907D4u) /* staged ctrl byte */
#define CELL_ECHO_22        (*(volatile uint8_t  *)0xFFF907D5u) /* THE 0x22 ECHO CELL */
#define CELL_EN_90          (*(volatile uint8_t  *)0xFFF907D6u) /* staged 0x90 enables */
#define CELL_ECHO_24        (*(volatile uint8_t  *)0xFFF907D7u) /* 0x24 cert echo */

#define CELL_ID_B0_06       (*(volatile uint8_t  *)0xFFF90978u)
#define CELL_ID_B0_00       (*(volatile uint8_t  *)0xFFF90977u)
#define CELL_STAT_B0_0C     (*(volatile uint8_t  *)0xFFF90979u)
#define CELL_STAT_B0_06     (*(volatile uint8_t  *)0xFFF9097Au)
#define CELL_SSLB0_NIB      (*(volatile uint8_t  *)0xFFF90958u) /* (rx&0xF)^0xF */

#define CELL_LETTERS        ((volatile uint8_t  *)0xFFF90944u) /* [3] */
#define CELL_OUT_FAULTS     ((volatile uint8_t  *)0xFFF90947u) /* [3] per-output */
#define CELL_OUTPUTS_DIS    (*(volatile uint8_t  *)0xFFF9094Au) /* 1 = disabled */
#define CELL_SHUTDOWN_ST    (*(volatile uint8_t  *)0xFFF9094Du)
#define CELL_MODE_BYTE      (*(volatile uint8_t  *)0xFFF90951u)
#define CELL_BIT3_SRC       (*(volatile uint8_t  *)0xFFF90952u) /* never written */
#define CELL_REQUEST        (*(volatile uint8_t  *)0xFFF90953u) /* enable request cell */
#define CELL_GRANT          (*(volatile uint8_t  *)0xFFF90954u) /* echo bit 0 = grant */
#define CELL_SSLB0_BIT2     (*(volatile uint8_t  *)0xFFF90959u) /* 0x14 bit 1 mirror */
#define CELL_SSLB0_BIT4     (*(volatile uint8_t  *)0xFFF90956u) /* 0x14 bit 4 */
#define CELL_C9_VARIANT     (*(volatile uint8_t  *)0xFFF9095Au)
#define CELL_DIR_BITS       (*(volatile uint8_t  *)0xFFF9095Eu)
#define CELL_90_BASE        (*(volatile uint8_t  *)0xFFF90963u)
#define CELL_CTRL_BIT1_SRC  (*(volatile uint8_t  *)0xFFF90964u)
#define CELL_CLEAR_LATCH    (*(volatile uint8_t  *)0xFFF90969u)
#define CELL_C9_STAGED      (*(volatile uint8_t  *)0xFFF9097Bu)

/* ── walk FSM cells (linker .sup_fsm at 0xFFF93B40) ───────────────────── */
#define CELL_PERIOD_BASE    (*(volatile uint16_t *)0xFFF93846u)
#define CELL_PERIOD         (*(volatile uint16_t *)0xFFF93848u) /* era period (ms) */
#define CELL_WALK_SUBST     (*(volatile uint8_t  *)0xFFF93B43u)
#define CELL_LAST_DUTY_CNT  (*(volatile uint16_t *)0xFFF93B48u)
#define CELL_LAST_LEVEL_CNT (*(volatile uint16_t *)0xFFF93B49u)
#define CELL_TWO_PASS       (*(volatile uint8_t  *)0xFFF93B9Au) /* level-7-twice counter */
#define CELL_JOB_MODE       (*(volatile uint8_t  *)0xFFF93B9Bu) /* 0=duty, 7=level */
#define CELL_STALL_CNT      (*(volatile uint8_t  *)0xFFF93B9Eu)
#define CELL_RETRIES        (*(volatile uint8_t  *)0xFFF93B9Du)
#define CELL_ATTENTION      (*(volatile uint8_t  *)0xFFF93BA4u)
#define CELL_WALK_FLAGS     (*(volatile uint8_t  *)0xFFF93BA7u)
#define CELL_SELECTOR       (*(volatile uint8_t  *)0xFFF93BA6u)
#define CELL_COMMIT_MASK    (*(volatile uint8_t  *)0xFFF93BA5u)
#define CELL_MAX_LATCH      (*(volatile uint8_t  *)0xFFF94997u)
#define CELL_LVL_REPORTED   (*(volatile uint8_t  *)0xFFF93BA1u) /* reported level */
#define CELL_LVL_EXPECTED   (*(volatile uint8_t  *)0xFFF93BA2u) /* expected level */
#define CELL_WALK_STATE     (*(volatile uint8_t  *)0xFFF93BA3u) /* 0/7/0x19 */
#define CELL_FSM_RETURN     (*(volatile uint8_t  *)0xFFF93B93u)

/* ── value pairs (linker .sup_value at 0xFFF94E60) ────────────────────── */
#define CELL_ERA_COMPL      (*(volatile uint8_t  *)0xFFF94E5Eu)
#define CELL_DUTY_CTX_COMPL (*(volatile uint8_t  *)0xFFF94E5Fu)
#define CELL_VALUE_COMPL    (*(volatile uint8_t  *)0xFFF94E60u) /* ~value */
#define CELL_ERA_COUNT      (*(volatile uint8_t  *)0xFFF94FE6u)
#define CELL_DUTY_CTX       (*(volatile uint8_t  *)0xFFF94FE7u)
#define CELL_VALUE          (*(volatile uint8_t  *)0xFFF94FE8u) /* commanded 84 value */
#define CELL_PENDING        (*(volatile uint8_t  *)0xFFF94FE9u) /* pending staging slot */
#define CELL_ERA_BASE_LO    (*(volatile uint8_t  *)0xFFF94FEAu)
#define CELL_SYS_MODE       (*(volatile uint8_t  *)0xFFF9500Cu)
#define CELL_BOOT_FLAG      (*(volatile uint8_t  *)0xFFF9500Eu) /* 1 = walk armed */
#define CELL_OUTER_FSM      (*(volatile uint8_t  *)0xFFF95010u) /* 0/8/0x12/0x23/0x45/0x34 */

/* ── era timestamps (in .sup_value; absolute addresses) ───────────────── */
#define CELL_ERA_BASE       (*(volatile uint32_t *)0xFFF9240Cu) /* FRT µs stamp */
#define CELL_ERA_DEADLINE   (*(volatile uint32_t *)0xFFF92410u)
#define CELL_TOKEN_STAGED   (*(volatile uint32_t *)0xFFF92414u)

/* ── output gates (in .sup_value) ──────────────────────────────────────── */
#define CELL_CERTIFIED      (*(volatile uint8_t  *)0xFFF800E8u) /* master output gate */
#define CELL_HANDOVER_RDY   (*(volatile uint8_t  *)0xFFF84A46u)

/* ── job use counters (in .bss; absolute addresses) ────────────────────── */
#define CELL_USE_COUNTER(id) (*(volatile uint16_t *)(uintptr_t)(0xFFF94218u + 2u * (id)))

/* ── SSLB2 (in .sup_cells) ──────────────────────────────────────────────── */
#define CELL_SSLB2_RAW      (*(volatile uint16_t *)0xFFF908EEu)
#define CELL_SSLB2_COPY     (*(volatile uint16_t *)0xFFF908CCu)

/* ── P3 batch/condense/cert cells (design §7.6/§7.7 laws, §10 map) ─────── */
#define CELL_TELEM_AVG      (*(volatile uint16_t *)0xFFF908CAu) /* 4-sample telemetry average (.sup_cells) */
#define CELL_C9_BASE        (*(volatile uint8_t  *)0xFFF90957u) /* C9 base cell (.sup_cells)       */
#define CELL_C9_HIGH        (*(volatile uint8_t  *)0xFFF90962u) /* C9 upper-nibble cell (.sup_cells) */
#define CELL_PD_LATCH       (*(volatile uint8_t  *)0xFFF800ECu) /* power-down latch, mode 6 (below
                                                                 * the RAM image — the stock's own
                                                                 * boot-record latch, never placed)  */
#define CELL_EN_ONESHOT     (*(volatile uint8_t  *)0xFFF93B94u) /* enable one-shot latch (.sup_fsm) */
#define CELL_COMMIT_RETRIES (*(volatile uint8_t  *)0xFFF93BA0u) /* commit-mismatch retries (.sup_fsm) */
#define CELL_CERT_EVENTS    (*(volatile uint16_t *)0xFFF94F84u) /* certification event bits (.sup_value) */

/* Clear every reserved cell range (the ranges the linker pins; NOT the
 * boot-record gates at 0xFFF800E8/0xFFF800EC, which survive resets by
 * design). Called first in the init bank, before any module init writes
 * its initial cell values. */
void sup_cells_init(void);

#endif /* TCU_SUP_CELLS_H */
