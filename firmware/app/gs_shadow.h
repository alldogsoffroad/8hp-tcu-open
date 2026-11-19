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
    /* the shift context's speed inputs (2026-11-18, the synchronisation-
     * progress producer 0x117A74 / 0x117F48): our live sensor speeds stand
     * in for the stock's conditioned cells — 0x89CB8 output speed (from D646
     * through 0xB986C's conditioning, not ported) and 0x89CBA turbine speed
     * (from the TIF13 chain, not ported); an invalid sensor gives 0.  Both
     * read 0 on the dry bench (shafts at rest). */
    uint16_t out_rpm;       /* → shadow 0x89CB8 (sensors output speed) */
    uint16_t tur_rpm;       /* → shadow 0x89CBA (sensors turbine speed) */
} gs_in_t;

/* one recorder entry = one 10 ms pass from a nonzero commit (28 bytes) */
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
    /* the synchronisation progress after this pass's slot 34 (2026-11-18) */
    uint8_t  bd30;          /* 0xFFF8BD30 locked passes (|slip| <= 20 rpm) */
    uint8_t  sync;          /* bit 0 BD6C sync flag (block A +0), bit 1 the
                             * hold (+0x28 = D0DC+4), bit 2 BD2E saturated */
    int16_t  bd76;          /* 0xFFF8BD76 progress (block A +0x0A) */
} gs_rec_t;
_Static_assert(sizeof(gs_rec_t) == 28u, "gs_rec_t is 28 bytes (tools/bench/tcu_bench.py gs-check)");

#define GS_REC_SLOTS    2u
#define GS_REC_N        112u    /* per slot: 1.12 s of passes (the dry-bench
                                 * completion at ≈ pass 96..100, + ≥ 10) */

/* our witnesses (not stock cells) */
typedef struct {
    uint32_t passes;        /* shadow passes run */
    uint32_t commits;       /* commits (any direction) */
    uint32_t commit_pass;   /* pass of the last commit */
    uint8_t  commit_old, commit_new;
    uint8_t  done_cause;    /* last completion: 1 normal (B +0x54), 2 timeout,
                             * 3 BD16 == 0 stand-in, 4 another 0x103DE2 term,
                             * 5 release 0x106918, 6 release BD8E timer (G4a) */
    uint8_t  rec_slot;      /* next recorder slot */
    uint32_t done_pass;     /* pass of the last completion */
    uint32_t loop_cap_n;    /* 0x10D070 iteration cap (ours) reached */
    uint32_t e896_n;        /* 0x10E896 term stand-in (0) taken */
    uint32_t lim_n;         /* 0x1147C8 limit applied */
    uint32_t unp_n;         /* unported branches reached (this TU) */
    uint32_t rel_n;         /* release passes (BD16 == 4; G4a — was: the
                             * one-pass release stand-in taken) */
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

/* ── THE RELEASE (G4a, 2026-11-19, ~/tcu_sweep/g4a_release_port.md) ──────
 * the stock's garage RELEASE (D/R → P/N: phases 0x12 → 0x13 → 0x14 of the
 * three releasing sub-objects, completion by 0x106918 or by the pass timer
 * BD8E ≥ sub0 +0x12) replaces the one-pass stand-in. Its witnesses and two
 * recorder slots live in OUR object .gs_rel (0xFFF8872C, stock RAM the
 * driving dump holds zero and no code of ours names), so that no earlier
 * shadow symbol moves. Slot 0 = the last release from D (old power flow 1),
 * slot 1 = the last release from R (old power flow 2). */
typedef struct {
    uint16_t cur[5];        /* shadow currents 0.1 mA, ch 0..4 = A D B E C */
    uint8_t  ph;            /* 2 bits per sub-object (bits 0-1 D0DC, 2-3 D148, 4-5
                             * D1B4): phase − 0x12 (0..2 = 0x12 / 0x13 / 0x14), 3 = other */
    uint8_t  fl;            /* bit0 BD16 == 4 (releasing), 1 completion this pass,
                             * 2 commit this pass, 3 D0DC +0x1F (vented), 4 D1B4
                             * +0x1F, 5 |BD7C| >= 0x10F26C (the speed exit), 6 D0DC
                             * output mode +0x10, 7 D1B4 output mode +0x10 */
} gs_rrec_t;
_Static_assert(sizeof(gs_rrec_t) == 12u, "gs_rrec_t is 12 bytes (tools/bench/tcu_bench.py gs-check)");

typedef struct {
    uint32_t start_pass;    /* shadow pass of the release commit */
    uint16_t len;           /* passes commit → completion (0xFFFF: not yet) */
    uint16_t bd8e_end;      /* BD8E at the completion */
    uint16_t lim12;         /* D0DC +0x12 (the timer limit 0x10F06A) at the commit */
    uint8_t  n;             /* entries recorded */
    uint8_t  cause;         /* 5 0x106918, 6 the BD8E timer, 4 a new request / E28E */
    int8_t   gear;          /* the gear code (0x89C2B) at the commit */
    uint8_t  el[3];         /* the released elements (D0DC, D148, D1B4; 1 A .. 5 E) */
    int16_t  p26[3];        /* +0x26 of each sub-object after the commit pass (mbar) */
    uint16_t pad;
} gs_rslot_t;
_Static_assert(sizeof(gs_rslot_t) == 24u, "gs_rslot_t is 24 bytes (tools/bench/tcu_bench.py gs-check)");

#define GS_RREC_N       125u    /* per slot: the 120-pass timer + 5 */

typedef struct {
    uint16_t bd8e;          /* 0xFFF8BD8E (block A +0x22): passes since the start */
    uint8_t  on;            /* the slot being recorded, 0xFF idle */
    uint8_t  last;          /* the slot of the last release */
    uint32_t rel_passes;    /* passes with BD16 == 4 */
    uint32_t starts;        /* releases started (BD16 = 4) */
    uint32_t unp_n;         /* release branches not ported reached (kind 2 / 7) */
    uint32_t c3_n;          /* the class-3 / element-C 0x12 special case taken */
    uint32_t e896_n;        /* release passes through 0x10E896 (D252 stand-in 0) */
    uint32_t idle_n;        /* completion with BD16 == 0 (the cause-3 stand-in, kept) */
    gs_rslot_t slot[2];
    gs_rrec_t rec[2][GS_RREC_N];
} gs_rel_t;
_Static_assert(sizeof(gs_rel_t) == 28u + 48u + 2u * 125u * 12u, "gs_rel_t layout (tools/bench/tcu_bench.py gs-check)");

void gs_shadow_init(void);
void gs_shadow_tick_10ms(const gs_in_t *in);
const volatile gs_rel_t *gs_shadow_rel(void);
/* the shadow is releasing (BD16 == 4): G4a does NOT bridge the release —
 * app/gs_bridge.c compares the live producer against the P content in
 * these passes, as it did against HEAD's instant-release shadow */
uint8_t  gs_shadow_releasing(void);

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
uint8_t  gs_shadow_reqmode(uint8_t ch);     /* request mode +7 behind ch 0..4 (A D B E C) */

/* the synchronisation progress of the shift context (block A 0xFFF8BD6C) */
int16_t  gs_shadow_bd76(void);              /* 0xFFF8BD76 */
uint8_t  gs_shadow_bd30(void);              /* 0xFFF8BD30 */
uint8_t  gs_shadow_sync(void);              /* 0xFFF8BD6C */
int16_t  gs_shadow_slip(void);              /* 0xFFF8BD7C */

/* the shadow's reaction-37 input (0xA031C(0x25) → 0xD9CBC: i_min 0 for the
 * actuators other than WK / Sys): a compile-time stand-in, inactive (the
 * dump and a healthy vehicle; actuator_reactions_decode.md) */
#ifndef GS_R37
#  define GS_R37 0
#endif
#if defined(__sh__) && (GS_R37 != 0)
#  error "gs_shadow: reaction 37 is a compile-time stand-in 0 on SH builds"
#endif

#if !defined(__sh__)
/* host only: restore the OLD stand-ins (negative tests) — bit 0: the
 * progress cells at the dump's idle values (BD76 0x7FFF, BD30 0xFF, BD6C 1)
 * instead of the ported producer; bit 1: the garage fill-time term 0xE469C
 * at 0; bit 2: force the progress cells to bd76 / bd30 / BD6C 0 (never
 * satisfied); bit 3: reaction 37 active (i_min 0) */
#define GS_HOST_OLD_PROGRESS   0x01u
#define GS_HOST_OLD_E469C      0x02u
#define GS_HOST_FORCE_PROGRESS 0x04u
#define GS_HOST_R37            0x08u
/* G4a negatives: bit 4 the OLD one-pass release stand-in (completed in the
 * commit pass, cause 3); bit 5 the release timer limit D0DC +0x12 forced 0 */
#define GS_HOST_OLD_RELEASE    0x10u
#define GS_HOST_LIM12_0        0x20u
#define GS_HOST_CECA_CLASS     0x40u    /* 0x10CECA's release test on the class (old) */
/* G4a: the output 0x10CECA on a synthetic sub-object (cls +0x1C, eng +0x20,
 * rel +0x21, f7 +7, calc_p +0xC) → its output mode +0x10; ceca_cls = 1
 * evaluates the OLD class-based release test (the negative) */
uint8_t gs_shadow_host_ceca(uint8_t cls, uint8_t eng, uint8_t rel, uint8_t f7, int16_t calc_p,
                            uint8_t ceca_cls);
/* G4a: sub-object k's release fields: +0x26, +0xA, +0x1F, +0x12 */
void gs_shadow_host_rel(uint8_t k, int16_t v[4]);
void gs_shadow_host_standins(uint8_t mask, int16_t bd76, uint8_t bd30);
/* host only: sub-object k's +0xC, +0x24, +0x54, +0x30, +0x32 and its
 * element's request result +2 */
void gs_shadow_host_sub(uint8_t k, int16_t v[6]);
#else
#  if defined(GS_SHADOW_HOST_HOOKS)
#    error "gs_shadow: the host stand-in overrides do not exist on SH builds"
#  endif
#endif

#endif /* TCU_APP_GS_SHADOW_H */
