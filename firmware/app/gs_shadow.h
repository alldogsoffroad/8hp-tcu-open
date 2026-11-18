/* gs_shadow.h — a READ-ONLY shadow of the stock's GARAGE ENGAGEMENT (N/P → D,
 * N/P → R at standstill), driving nothing (app/gs_shadow.c, app/gs_fill.c;
 * ~/tcu_sweep/garage_shadow_port.md; decodes garage_shift_decode.md,
 * shift_engine_decode.md).
 *
 * The shadow computes, once per 10 ms pass in OS modes 1..3, what the stock
 * would do — direction request AE28, the power-flow request 0x103F56 and
 * commit 0xCFD1A (gate 0xDC37E), the exception machine E28E, the engine-
 * state FSM 0xD4DC2, the gear code, the three-element engagement engine
 * 0xFFF8E4AC (start 0xDDCA2 → 0xEB900 → 0x1066D6 → 0x10D130, step 0x10CE88,
 * output 0x10CECA, completion 0x103DE2 → 0xBAB6C → 0x106918 → 0x10CDFA),
 * the producer's garage sources ids 5..10 and the holder into SHADOW
 * request objects (its own fill instance, app/gs_fill.c), and the element
 * law into SHADOW per-channel currents. Every value lands in the shadow's
 * own RAM (linker.ld .gs_req / .gs_obj / .gs_rec); it writes no stock cell
 * and no output.
 *
 * READ-ONLY, ENFORCED:
 *   - every output translation unit defines TCU_OUTPUT_PATH: this header
 *     #errors under it (an output path cannot even name the shadow);
 *   - app/gs_shadow.c defines TCU_GS_SHADOW_TU: the live output headers
 *     (rq_prod.h, rq_fill.h, park_cells.h, park.h, park_law.h, the bsw/drv headers,
 *     sup_cells.h) #error under it; its inputs arrive by VALUE (gs_in_t,
 *     filled by app/main.c from the live getters);
 *   - ctest source scans with negative fixtures, sh-elf-gcc pass/fail
 *     checks, and a host test proving the live path bit-identical with and
 *     without the shadow (CMakeLists "garage shadow").
 *
 * THE FORCED COMMIT (shadow-only, compile-time GS_FORCED, default 1): the
 * bench sits in OS mode 1 (no engine speed), where the stock NEVER commits a
 * nonzero power flow (0xDC37E needs 0x89C3D == 0), and its gear code is the
 * U11E4 fault reaction (code 0x20 → gear 3). GS_FORCED = 1 runs the shadow AS
 * IF (a) the shadow's copy of 0x89C3D were 0 (OS mode ≠ 1, settle counter
 * running from boot) and (b) the gear target were the fault-free strategy's
 * 1st (0xFFF8A697 = 1 → 0x89CE8 → gear code 1) — so the shadow shows the
 * stock's healthy engagement. GS_FORCED = 0 is the stock-faithful variant:
 * 0x89C3D follows the OS mode (mode 1 → 1 → no nonzero commit, ever) and the
 * gear code is the bench's 3 (code 0x20). The SH image builds GS_FORCED = 1;
 * the host test builds both.
 */
#ifndef TCU_APP_GS_SHADOW_H
#define TCU_APP_GS_SHADOW_H

#if defined(TCU_OUTPUT_PATH)
#error "the garage shadow is read-only: an output path may not include gs_shadow.h"
#endif

#include <stdint.h>
#include <stdbool.h>

#ifndef GS_FORCED
#  define GS_FORCED 1
#endif
#if (GS_FORCED != 0) && (GS_FORCED != 1)
#  error "gs_shadow: GS_FORCED is 0 (stock-faithful) or 1 (forced commit), nothing else"
#endif

/* the live values the shadow reads, BY VALUE (app/main.c fills them) */
typedef struct {
    uint8_t  os_mode;       /* 0x93B33 */
    uint8_t  rngd;          /* 0xFFF89CDC as 0xB912C / 0xB97F8 publish it (park_law) */
    uint8_t  park_model;    /* 0xFFF8BC70[0] (1 released) */
    uint8_t  park_req;      /* 0xFFF8AE29 (0xB6A90, the holder's park branch) */
    uint8_t  temp;          /* 0xFFF89C13 (live, app/tmp_chain.c) */
    uint8_t  cd2;           /* 0xFFF89CD2 (live) */
    uint16_t eng_speed;     /* CAN 0x108 engine speed (E908) */
    uint8_t  eng_valid;     /* CAN 0x108 valid (E90C == 0, E908 != 0xFFFF) */
} gs_in_t;

/* one recorder entry = one 10 ms pass from a nonzero commit (24 bytes) */
typedef struct {
    uint16_t pass;          /* passes since the commit (0 = the commit pass) */
    uint8_t  pf;            /* 0x89C30 (shadow) */
    uint8_t  flags;         /* bit0-1 E6EF, 2 pending, 3 engine active, 4 B timeout
                             * (D0DC+0x14), 5 completion this pass, 6 commit this
                             * pass, 7 shadow 0x89C3D */
    uint8_t  ph[3];         /* phase +0x11 of D0DC (first), D148 (second), D1B4 (third) */
    uint8_t  modes;         /* bit 0..4 request mode +7 of A..E; bit 5..7 the
                             * sub-objects' output mode +0x10 (D0DC, D148, D1B4) */
    uint8_t  reqset;        /* bit 0..4: request +0 != 0 for A..E */
    int8_t   gear;          /* the shadow gear code (0x89C2B) */
    uint16_t cur[7];        /* shadow currents 0.1 mA, ch 0..6 = A D B E C WK Sys */
} gs_rec_t;
_Static_assert(sizeof(gs_rec_t) == 24u, "gs_rec_t is 24 bytes");

#define GS_REC_SLOTS    2u
#define GS_REC_N        95u     /* per slot: 0.95 s of passes */

/* our witnesses (not stock cells) */
typedef struct {
    uint32_t passes;        /* shadow passes run */
    uint32_t commits;       /* commits (any direction) */
    uint32_t commit_pass;   /* pass of the last commit */
    uint8_t  commit_old, commit_new;
    uint8_t  done_cause;    /* last completion: 1 normal (B +0x54), 2 timeout,
                             * 3 release stand-in, 4 another 0x103DE2 term */
    uint8_t  rec_slot;      /* next recorder slot */
    uint32_t done_pass;     /* pass of the last completion */
    uint32_t loop_cap_n;    /* 0x10D070 iteration cap (ours) reached */
    uint32_t e896_n;        /* 0x10E896 term stand-in (0) taken */
    uint32_t lim_n;         /* 0x1147C8 limit applied */
    uint32_t unp_n;         /* unported branches reached (this TU) */
    uint32_t rel_n;         /* release (types 3/4) stand-in taken */
    uint32_t seq_n;         /* 0x104A28 sequencing stand-in taken */
    uint32_t lockup_n;      /* 0xDCD38.. WK lock-up branch head true (not ported) */
    uint8_t  rec_n[GS_REC_SLOTS];
    uint8_t  e28e;          /* E28E state */
    uint8_t  e6a8;          /* engine state E6A8[4] */
    /* this pass's state (for the bench read) */
    uint8_t  now_pf;        /* shadow 0x89C30 */
    uint8_t  now_e6ef;      /* power-flow request */
    uint8_t  now_ae28;      /* direction request */
    uint8_t  now_flags;     /* bit0 0x89D0F, 1 pending, 2 shadow 0x89C3D, 3 engine active */
    int8_t   now_gear;      /* shadow 0x89C2B */
    uint8_t  now_ph[3];     /* phases D0DC, D148, D1B4 */
    uint16_t now_cur[7];    /* shadow currents ch 0..6 (A D B E C WK Sys) */
} gs_wit_t;
_Static_assert(sizeof(gs_wit_t) == 76u, "gs_wit_t is 76 bytes (tools/bench/tcu_bench.py gs-check)");

void gs_shadow_init(void);
void gs_shadow_tick_10ms(const gs_in_t *in);

const volatile gs_wit_t *gs_shadow_wit(void);
const volatile gs_rec_t *gs_shadow_rec(uint8_t slot, uint8_t i);
uint16_t gs_shadow_cur(uint8_t ch);         /* last shadow current, ch 0..6 */
uint8_t  gs_shadow_pf(void);                /* shadow 0x89C30 */
int8_t   gs_shadow_gear(void);              /* shadow 0x89C2B */
uint8_t  gs_shadow_phase(uint8_t k);        /* k 0 D0DC, 1 D148, 2 D1B4 */
uint8_t  gs_shadow_pending(void);
uint8_t  gs_shadow_e6ef(void);
uint8_t  gs_shadow_ae28(void);
uint8_t  gs_shadow_d0f(void);               /* 0x89D0F engine running */

#if !defined(__sh__)
/* host only: stand-in overrides for the dry-bench completion study (e):
 * the progress cells BD76 / BD30 (0xFFF8BD76 s16, 0xFFF8BD30 u8; dump
 * 0x7FFF / 0xFF) */
void gs_shadow_host_progress(int16_t bd76, uint8_t bd30);
/* host only: sub-object k's +0xC, +0x24, +0x54, +0x30, +0x32 and its
 * element's request result +2 */
void gs_shadow_host_sub(uint8_t k, int16_t v[6]);
#else
#  if defined(GS_SHADOW_HOST_HOOKS)
#    error "gs_shadow: the host stand-in overrides do not exist on SH builds"
#  endif
#endif

#endif /* TCU_APP_GS_SHADOW_H */
