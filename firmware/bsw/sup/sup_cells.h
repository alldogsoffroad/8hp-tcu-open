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
#define CELL_RX_ERR         (*(volatile uint8_t  *)0xFFF90541u) /* |=1 duty, |=2 level reply class fail (0x910EE/0x911D8) */
#define CELL_REPLY_STAMP    (*(volatile uint32_t *)0xFFF90544u) /* TCNTA - 0x29 at each W1/W2 completion (0x910EE:13 / 0x911D8:13) */
/* the stock's two reply counters are BYTES (0x910EE: 0x9054D++, 0x911D8:
 * 0x9054C++). The old u16 LEVEL_COUNT at 4C spanned 4C-4D (the alias F3
 * worked around by moving the duty counter to 0x90552, kept below). */
#define CELL_LEVEL_COUNT    (*(volatile uint8_t  *)0xFFF9054Cu) /* level reply counter (0x911D8) */
#define CELL_DUTY_RXCNT     (*(volatile uint8_t  *)0xFFF9054Du) /* duty reply counter (0x910EE) */
#define CELL_ECHO_0A        (*(volatile uint8_t  *)0xFFF9054Fu) /* 0x0A echo */
#define CELL_FB_0C          (*(volatile uint8_t  *)0xFFF90550u) /* 0x0C feedback */
#define CELL_COMMIT_12      (*(volatile uint8_t  *)0xFFF90551u) /* 0x12 commit status */
#define CELL_STAGED_84      (*(volatile uint8_t  *)0xFFF9054Eu) /* staged level value */
/* F3/D1 completed 2026-11-10 (protocol_audit F3): the duty counter moved OFF
 * byte 0x9054D — even as a byte it aliased LEVEL_COUNT's high byte (the u16
 * at 4C spans bytes 4C-4D), so every duty reply corrupted the level counter
 * and every level increment reset the duty counter (live now that real W2
 * replies land — r01o). The free window 0x90552-53 sits inside the reserved
 * .sup_walk range (linker.ld). Bench decode note: DUTY_COUNT is no longer
 * at 0x9054D; read USE[JOB_DUTY] as the truth as before. */
#define CELL_DUTY_COUNT     (*(volatile uint8_t  *)0xFFF90552u) /* duty reply counter */

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

/* S6 (baseline review): the stock's SSLB0 result triple (0x92D94/0x92AC0/
 * 0x92BBE) — bit 1 = J5, 2 = J1 (incl. the C9 echo), 4 = J6 — and the
 * 0x93126 summaries; the A8-class triple is 0x907C8/C9/CA above with bits
 * J5 0x01, J1 0x02, J6 0x04, J2 0x10, F 0x20, J3 0x40 (mask 0x77). */
#define CELL_B0_OK          (*(volatile uint8_t  *)0xFFF90973u)
#define CELL_B0_ERR         (*(volatile uint8_t  *)0xFFF90974u)
#define CELL_B0_ECHOFAIL    (*(volatile uint8_t  *)0xFFF90975u)
#define CELL_SUM_B0         (*(volatile uint8_t  *)0xFFF90971u) /* 0x931F8(&0x90973, ., 7) */
#define CELL_SUM_B1         (*(volatile uint8_t  *)0xFFF90972u) /* 0x931F8(&0x907C8, ., 0x77) */
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
/* the guard cells (stock 0x6C594, recomputed on every period change) and
 * the burst guard 0x93846 = max(*(u16)0x40CA2, *(u16)0x40CA4) + 0xBB = 228
 * (0x6C12C; flash 0x40CA2/4 = 0029 0029). Linker .sup_period starts 0x9383C. */
#define CELL_WALK_ERRCNT    (*(volatile uint16_t *)0xFFF9383Cu) /* 0x6C1EA saturating error count */
#define CELL_GUARD_OPEN     (*(volatile uint16_t *)0xFFF9383Eu) /* token window open   (period + 0x47A) */
#define CELL_GUARD_CLOSE    (*(volatile uint16_t *)0xFFF93840u) /* token window close / 84 keep-out start */
#define CELL_GUARD_END      (*(volatile uint16_t *)0xFFF93842u) /* 84 keep-out end */
#define CELL_GUARD_SAMPLE   (*(volatile uint16_t *)0xFFF93844u) /* sample/late bound */
#define CELL_BURST_GUARD    (*(volatile uint16_t *)0xFFF93846u) /* 228 */
#define CELL_PERIOD         (*(volatile uint16_t *)0xFFF93848u) /* era period (ms) */
#define CELL_WALK_SUBST     (*(volatile uint8_t  *)0xFFF93B43u)
#define CELL_LAST_DUTY_CNT  (*(volatile uint8_t  *)0xFFF93B48u) /* last-seen 0x9054D (bytes, 0x6C1EA) */
#define CELL_LAST_LEVEL_CNT (*(volatile uint8_t  *)0xFFF93B49u) /* last-seen 0x9054C */
#define CELL_STAGE_SUB      (*(volatile uint8_t  *)0xFFF93B9Cu) /* 0x6C48C/0x6C742 staging sub-state 0/7/0x19 */
#define CELL_RXERR_CNT      (*(volatile uint8_t  *)0xFFF93B9Fu) /* 0x6C1EA reply-error count */
#define CELL_S8_FLAG        (*(volatile uint8_t  *)0xFFF93B96u) /* 0x8D136 clears / 0x8D020 consumes */
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
#define CELL_PENDING_COMPL  (*(volatile uint8_t  *)0xFFF94E61u)
#define CELL_TARGET         (*(volatile uint8_t  *)0xFFF95012u) /* 0x8D136 level target 0/5/3 */
#define CELL_TARGET_COMPL   (*(volatile uint8_t  *)0xFFF94E8Au)
/* linker .sup_s8 at 0xFFF9417C */
#define CELL_S8_BUDGET      (*(volatile uint8_t  *)0xFFF9417Du) /* 0x8D136/0x8D020 budget */
#define CELL_S8_KIND        (*(volatile uint8_t  *)0xFFF9417Eu) /* 0x8D020: 7 token / 0x19 resend / 0 attention */
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

/* ── L1: the self-test ladder, mode sequencer, seq2, pin cut, publishers ──
 * (sup_ladder.c; ladder_blockers_decode.md / letters_law_decode.md; every
 * address from the stock bytes). u8 unless marked. Pairs: value / diverse
 * complement (the stock's 0x6BD68 byte / 0x6BD6E word pair writers). The
 * linker reserves .sup_l1a..g and .sup_cnt (linker.ld); sup_cells_init
 * clears them. */
#define CELL_L1_U8(a)       (*(volatile uint8_t  *)(uintptr_t)(a))
#define CELL_L1_U16(a)      (*(volatile uint16_t *)(uintptr_t)(a))
/* the sequencer 0x85160 and the ladder 0x8B7A8 */
#define CELL_SEQ_STATE      CELL_SYS_MODE                       /* 0x9500C: 0 / '.' / 'L' / '=' / 0x1F */
#define CELL_SEQ_STATE_C    CELL_L1_U8(0xFFF94E84u)
#define CELL_LAD_SUB        CELL_L1_U8(0xFFF9500Du)             /* sub-state: 0x55 'U' pass, 0x30/31/33/34 wait, >= 0xF0 fail, 0xAA done, 0x40 supply abort */
#define CELL_LAD_SUB_C      CELL_L1_U8(0xFFF94E85u)
#define CELL_LAD_PHASE      CELL_L1_U16(0xFFF94FE2u)            /* phase word 1/2/4/../0x100/0x200 */
#define CELL_LAD_PHASE_C    CELL_L1_U16(0xFFF94E5Au)
#define CELL_LAD_PASS       CELL_L1_U16(0xFFF93A8Cu)            /* pass mask (stock done = 0x00FF) */
#define CELL_LAD_FAULT      CELL_L1_U16(0xFFF93A84u)            /* fault bits (phase / mask+1 / 0x200) */
#define CELL_LAD_OKFLAGS    CELL_L1_U16(0xFFF93A86u)            /* 0xFF finisher | 0x200 'L' */
#define CELL_LAD_SAVED_PH   CELL_L1_U16(0xFFF93A88u)            /* phase saved by the supply window */
#define CELL_LAD_MISMATCH   ((volatile uint8_t *)0xFFF93A8Au)   /* [2] class seen on a mismatch */
#define CELL_EN_PREV        ((volatile uint8_t *)0xFFF93A8Eu)   /* [2] 0x8C7A8 previous enable */
#define CELL_EN_MIRROR      ((volatile uint8_t *)0xFFF93A90u)   /* [2] 0x8C7A8 enable */
#define CELL_OUT_CLASS      ((volatile uint8_t *)0xFFF93A92u)   /* [2] 0x8C6D4 voltage class */
#define CELL_LAD_FAILCLS    ((volatile uint8_t *)0xFFF939B4u)   /* [2] 0x8C2A4 failed class */
#define CELL_LAD_REQ        ((volatile uint8_t *)0xFFF939B6u)   /* [2] THE REQUEST (0x85E64 only) */
#define CELL_SEQ2_STATUS    ((volatile uint8_t *)0xFFF939B8u)   /* [2] 0 / 1 class 0x22 / 2 on / 3-4 'U' */
#define CELL_SEQ2_DIAG      ((volatile uint8_t *)0xFFF939BEu)   /* [2] '3' diagnosis state */
#define CELL_SEQ2_939C0     ((volatile uint8_t *)0xFFF939C0u)   /* [2] */
#define CELL_LAD_94114      CELL_L1_U8(0xFFF94114u)
#define CELL_SEQ2_SUPLOW    CELL_L1_U8(0xFFF94116u)             /* per-ch supply-low bits */
#define CELL_SEQ2_RESOLVED  CELL_L1_U8(0xFFF94118u)             /* per-ch bits → module 7 */
#define CELL_LAD_RETRIES    CELL_L1_U8(0xFFF9414Du)             /* 3 (cal 0x1919F5) */
#define CELL_LAD_SAVED_SUB  CELL_L1_U8(0xFFF9414Eu)
#define CELL_LAD_SAVED_820  CELL_L1_U8(0xFFF9414Fu)
#define CELL_LAD_SAVED_821  CELL_L1_U8(0xFFF94150u)
#define CELL_CUT_FLAGS      CELL_L1_U8(0xFFF94151u)             /* 0x85788 / 0x8C34C flags */
#define CELL_CUT_PERIODIC   CELL_L1_U8(0xFFF94152u)             /* post-ladder cut request */
#define CELL_CUT_MODE       CELL_L1_U8(0xFFF94153u)             /* 0x11 ladder / 0x33 post / 0x22 fail */
#define CELL_CUT_REQ        CELL_L1_U8(0xFFF94154u)             /* 0xAA arm → cut → 0x55 → restore → 0 */
#define CELL_LAD_95005      CELL_L1_U8(0xFFF95005u)
#define CELL_LAD_95005_C    CELL_L1_U8(0xFFF94E7Du)
#define CELL_LAD_9500B      CELL_L1_U8(0xFFF9500Bu)
#define CELL_MOD11          CELL_L1_U8(0xFFF93B85u)             /* startup module 11: 0 → 1 (start) → 0x0F */
#define CELL_MOD7           CELL_L1_U8(0xFFF93B86u)             /* startup module 7 (seq2) */
#define CELL_TIOC_ON        CELL_L1_U8(0xFFF90967u)             /* 0x91E4E 1 / 0x91E6C 0 */
/* channel groups (0x8556E) and the surviving mask */
#define CELL_GRP(g)         CELL_L1_U16(0xFFF94F6Cu + 2u * (g)) /* g 0..2; g 3 = 0x94F72 alive */
#define CELL_GRP_C(g)       CELL_L1_U16(0xFFF94DE4u + 2u * (g))
/* seq2 0x85828 */
#define CELL_SEQ2_STATE     ((volatile uint8_t *)0xFFF94FD0u)   /* [2] 0x11/0x16/0x22/0x33/'U'/'4'/'8'/'?'/'D' */
#define CELL_SEQ2_STATE_C   ((volatile uint8_t *)0xFFF94E48u)
#define CELL_OUT_DEMAND     ((volatile uint8_t *)0xFFF94FCAu)   /* [2] application output demand */
#define CELL_OUT_DEMAND_C   ((volatile uint8_t *)0xFFF94E42u)
#define CELL_SEQ2_94FCC     ((volatile uint8_t *)0xFFF94FCCu)   /* [2] demand && no 0x7BBB4 fault */
#define CELL_SEQ2_94FCE     ((volatile uint8_t *)0xFFF94FCEu)   /* [2] the enable byte */
/* the publishers 0x89114 / 0x8D8DE */
#define CELL_OUT_MV         ((volatile uint16_t *)0xFFF94ED0u)  /* [2] output node mV (mux 8 / mux 10) */
#define CELL_OUT_MV_C       ((volatile uint16_t *)0xFFF94D48u)
#define CELL_OUT_ST         ((volatile uint8_t *)0xFFF94F94u)   /* [2] 1 / 7 / 8 */
#define CELL_OUT_ST_C       ((volatile uint8_t *)0xFFF94E0Cu)
#define CELL_OUT_BYTE       ((volatile uint8_t *)0xFFF93940u)   /* [2] mV x 0.01, cap 255 */
#define CELL_SUPPLY         CELL_L1_U16(0xFFF94F96u)            /* the ladder's supply (mV) */
#define CELL_SUPPLY_C       CELL_L1_U16(0xFFF94E0Eu)
#define CELL_SUPPLY_RAW     CELL_L1_U16(0xFFF94F86u)            /* 0x8D8DE output */
#define CELL_SUPPLY_RAW_C   CELL_L1_U16(0xFFF94DFEu)
#define CELL_SUPPLY_ST      CELL_L1_U8(0xFFF94FECu)             /* 8 ok / 7 / 6 */
#define CELL_SUPPLY_ST_C    CELL_L1_U8(0xFFF94E64u)
#define CELL_SUPPLY_ST2     CELL_L1_U8(0xFFF94FEDu)
#define CELL_SUPPLY_ST2_C   CELL_L1_U8(0xFFF94E65u)
#define CELL_SUP_FLAGS      CELL_L1_U16(0xFFF93AACu)            /* 0x8D8DE fault mask */
#define CELL_SUP_DIFF       CELL_L1_U16(0xFFF93AAEu)
#define CELL_SUP_M13        CELL_L1_U16(0xFFF93AB0u)
#define CELL_SUP_MIN13      CELL_L1_U16(0xFFF93936u)
#define CELL_SUP_ENV        CELL_L1_U16(0xFFF93938u)
#define CELL_SUP_940FA      CELL_L1_U8(0xFFF940FAu)
#define CELL_SUP_9415B      CELL_L1_U8(0xFFF9415Bu)
#define CELL_DIAG_93120     ((volatile uint16_t *)0xFFF93120u)  /* [2] cleared by 0x863D8 */
/* the 5 ms countdown service 0x7C7E4 (table 0x45D30): words [0x94734,
 * 0x94786), bytes [0x94788, 0x94835) */
#define CNT_W_LO            0xFFF94734u
#define CNT_W_HI            0xFFF94786u
#define CNT_B_LO            0xFFF94788u
#define CNT_B_HI            0xFFF94835u
#define CELL_CNT_SUP_DIFF   CELL_L1_U16(0xFFF94772u)            /* 0x8D8DE (cal 0x1916F4 = 5) */
#define CELL_CNT_SUP_LOW    CELL_L1_U16(0xFFF94774u)            /* 0x8D8DE (cal 0x1916F6 = 4) */
#define CELL_CNT_SUP_RIP    CELL_L1_U16(0xFFF94776u)            /* 0x8D8DE (cal 0x1916FA = 10) */
#define CELL_CNT_SUP_RIP2   CELL_L1_U16(0xFFF94778u)            /* 0x8D8DE (cal 0x1916F8 = 100) */
#define CELL_CNT_SUPLOW     ((volatile uint8_t *)0xFFF947ACu)   /* [2] seq2 supply-low hold (20) */
#define CELL_CNT_SETTLE     ((volatile uint8_t *)0xFFF947B0u)   /* [2] seq2 'U' settle (6) */
#define CELL_CNT_HOLDOFF    ((volatile uint8_t *)0xFFF947B2u)   /* [2] seq2 'U' hold-off */
#define CELL_CNT_BIT1       CELL_L1_U8(0xFFF9481Fu)             /* 3 = 15 ms (phases 2 / 0x20) */
#define CELL_CNT_LADDER     CELL_L1_U8(0xFFF94820u)             /* 0x28 = 200 ms whole ladder */
#define CELL_CNT_OUTTOL     CELL_L1_U8(0xFFF94821u)             /* 6 = 30 ms output on/off */
/* the AN0 descriptor status bytes 0x8F50C clears (mux 8 / mux 10 windows) */
#define CELL_ADC_ST_MUX8    CELL_L1_U8(0xFFF90017u)
#define CELL_ADC_ST_MUX10   CELL_L1_U8(0xFFF9001Fu)
/* the AN0 publications the ladder consumes (plat_adcmux.c finisher) */
#define CELL_MUX_MV         ((volatile uint16_t *)0xFFF90188u)  /* [2] mux 8 / mux 10 */
#define CELL_MUX11_MV       CELL_L1_U16(0xFFF907E2u)
#define CELL_MUX13_MV       CELL_L1_U16(0xFFF907E4u)
#define CELL_MUX13_ENV      CELL_L1_U16(0xFFF907F6u)
#define CELL_MUX13_MIN      CELL_L1_U16(0xFFF907FCu)

/* ── L3: the monitor FSM's post-8 states and the '3' switch-on diagnosis ──
 * (sup_walk.c 0x873F0 / 0x87766 / 0x8799C / 0x8CEDC; sup_ladder.c 0x85FC4 /
 * 0x86838 / 0x865E0 / 0x86586). Linker: .sup_value, .sup_fsm, .sup_l1a,
 * .sup_l1c (extended to 0x939C7), .sup_l1f, .sup_cnt, and the new .sup_l3a
 * (0x94958..0x9495F) / .sup_l3b (0x84A24..0x84A2B). */
#define CELL_OUTER_FSM_C    CELL_L1_U8(0xFFF94E88u)             /* ~0x95010 (0x87766 / 0x873F0 pair) */
#define CELL_BOOT_FLAG_C    CELL_L1_U8(0xFFF94E86u)             /* ~0x9500E (entry flag) */
#define CELL_E_STATE        CELL_L1_U8(0xFFF95011u)             /* 0x8CEDC state 0x1E / 0 / 7 / 0x19 / 0x2A */
#define CELL_E_STATE_C      CELL_L1_U8(0xFFF94E89u)
#define CELL_E_LAST         CELL_L1_U8(0xFFF93B97u)             /* 0x8CEDC: last context armed */
#define CELL_FSM_93B99      CELL_L1_U8(0xFFF93B99u)             /* 1 on entry to 'E' from 0x23 */
/* the '3' diagnosis (0x85FC4): per channel a u16 pair at 0x93118 + 4ch —
 * the tested fault bits (0x6C | 0x80 | 0x100) and the bits SEEN failing */
#define CELL_DIAG_TEST(ch)  CELL_L1_U16(0xFFF93118u + 4u * (ch))
#define CELL_DIAG_SEEN(ch)  CELL_L1_U16(0xFFF9311Au + 4u * (ch))
#define CELL_DIAG_9495C     ((volatile uint16_t *)0xFFF9495Cu)  /* [2] tested mask (state 8) */
#define CELL_DIAG_94958     ((volatile uint16_t *)0xFFF94958u)  /* [2] failed bits (state 8) */
#define CELL_DIAG_SUB       ((volatile uint8_t *)0xFFF939C2u)   /* [2] 0x86838 sub-FSM 0..4 */
#define CELL_DIAG_CLS       ((volatile uint8_t *)0xFFF939C4u)   /* [2] 0x86586 status 0/2/0x82 */
#define CELL_CNT_DIAG       ((volatile uint8_t *)0xFFF947B4u)   /* [2] 0x86838 wait/timeout (5 ms) */
#define CELL_DIAG_94115     CELL_L1_U8(0xFFF94115u)             /* per-ch bits (class-0x11 step) */
#define CELL_DIAG_9411B     CELL_L1_U8(0xFFF9411Bu)             /* step 0 / 1 / 2 */
#define CELL_DIAG_9411C     CELL_L1_U8(0xFFF9411Cu)             /* channel under test */
#define CELL_DIAG_84A25     CELL_L1_U8(0xFFF84A25u)             /* per-ch bits (cal 0x1915A4 != 0) */
#define CELL_DIAG_84A26     CELL_L1_U8(0xFFF84A26u)
#define CELL_DIAG_84A27     CELL_L1_U8(0xFFF84A27u)
#define CELL_DIAG_84A28     CELL_L1_U8(0xFFF84A28u)

/* Clear every reserved cell range (the ranges the linker pins; NOT the
 * boot-record gates at 0xFFF800E8/0xFFF800EC, which survive resets by
 * design). Called first in the init bank, before any module init writes
 * its initial cell values. */
void sup_cells_init(void);

#endif /* TCU_SUP_CELLS_H */
