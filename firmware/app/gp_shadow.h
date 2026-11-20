/* gp_shadow.h — a READ-ONLY shadow of the stock's GEAR-PAIR SHIFT ENGINE
 * skeleton: the machinery that changes gear while engaged (power flow != 0),
 * separate from the garage engine we run live (app/gs_engine.c).
 * (2026-11-20; decode ~/tcu_sweep/shift_engine_gearpair_decode.md, the port
 * ~/tcu_sweep/gp_shadow_port.md — every address below re-read in the bytes.)
 *
 * TWO PARTS, both on their OWN RAM (never a live cell):
 *   app/gp_gear.c   the gear feedback chain (app/sm_gear.c, the stock slots
 *                   30..33) compiled a SECOND time with SMG_COUPLED = 1: its
 *                   shift-object stand-ins are bound to this shadow (BD15 /
 *                   BD16 / BD14+0x2C, 0xEB7BC, 0xDDA54's BD15 term → 0xEB1E8,
 *                   0xEB1F0 → 0xEDA48, 0xBAAC8 → 0xDDDE8, 0xBAAE4 → 0xDDE78,
 *                   the full 0xDD954 / 0xDE190 with the early next gear).
 *                   Own state .gp_gear (gf_t); the r04w chain (app/sm_gear.c,
 *                   .sm_gear) is unchanged and keeps its idle stand-ins.
 *   app/gp_shadow.c the shift object 0xFFF8BD14 as a shadow (gp_t, .gp_shadow):
 *       pre-select  0xDDBB4(…,1) → 0xEB9EC (0xEEB04, 0x1176C4 / 0x1176BA on
 *                   block B, 0xEC134 → BD14+0x1BE, 0x11755E or BD15 = 1 +
 *                   0xEDC5A(BD14, 0)), with 0xDD954's +0x14 pending block
 *       strobe      0xDDA54's BD15 term (block A sync BD6C or A+4) → 0xEB1E8
 *                   (+0x2D = 1); 0xEB7BC
 *       commit      0xDDDE8 → 0xDDBB4(…,0) (E33C+0/+1), the snapshots, 0xEBA6E
 *                   (block A, or the check + 0xECB14 SWAP of B into A), BD16 =
 *                   E69D, BD15 = 1, BD3C / BD1A / +0x2C / +4, 0xEDC5A(BD14, 1)
 *                   (0xED9B2 BD1E / BD22 / +0x53, 0x117970, 0xED988 BD17 =
 *                   0x6315C[BD16 + 3·BD1E], 0x11793E progress start, the
 *                   phase-object starts 0x11299A / 0x10FC5C)
 *       completion  0xEB1F0 → 0xEDA48 (the gear-shift branch: on-coming
 *                   applied ∧ block-A sync ∧ E344 == 0 → 0xEB49C +0x2D; the
 *                   0x36B consistency report; the BD1A == 0x32 exit; the timer
 *                   branch)
 *       end         0xDDE78 → 0xDDBB4(g, g, 0) → 0xEBC60 — CORRECTION to the
 *                   decode: 0xEBC60 is the gear-pair shift's NORMAL END (via
 *                   0x103B36 → 0xBAAE4 → 0xDDE78), not only an abort; 0xEBB6C
 *                   (0xDDC38 / 0xDDED6) ends power-flow (garage) shifts and is
 *                   mirrored here on the live garage end
 *       per pass    the sync part of 0x1175D6 (0x117F48 / 0x117A74) on both
 *                   blocks; 0xEB348's four phase-object steps (0x112888 /
 *                   0x10FB72: step, 0x113D0A / 0x110ADE next from 0x5EC24 /
 *                   0x63555, skip equal entries, the > 0x13 / > 0x11 report)
 *                   and outputs (0x112916: +8 = phase == 1)
 *       producer    ids 1..4 (0xDD7B8 / 0xDD862: E33C+0 / +1 with block A's
 *                   on / off outputs, E33C+5 / +6 with block B's) COMPUTED and
 *                   RECORDED, never applied
 *
 * 2026-11-20 STAND-IN RETIREMENTS (~/tcu_sweep/shift_class_counters_decode.md,
 * every address re-read in the bytes; ~/tcu_sweep/gp_standins_port.md):
 *   the kind / class machine 0x1180BE (via 0x117B4A: the start call 0xED906
 *     p = 1 / 0, the slot-34 cyclic 0xEBDBC's calls) with 0xEC362 (f8 from the
 *     0x89CF4 arrays), 0xEC20A and the four condition windows 0xEC40E /
 *     0xEC4D8 / 0xEC578 / 0xEC614 (10 x 10 tables over output speed and the
 *     input torque), the class 0xEC182 / 0xEC1F0 and the 0x117B4A override
 *     (AE31 stood in 0); the 2->1 / 3->2 class-1 rows replace the stand-in
 *     durations of the phases that have decoded numbers (on 1 / 0x0A / 0x0D,
 *     off 1 / 0x0B / 0x0F), the other phases keep GP_SI_T_*;
 *   BD1B 0xEC6A6 / 0xEC7CC (the stand-in GP_SI_BD1B is gone);
 *   the slot-28 counters 0xEC0E0 / 0x117538 / 0x11295C / 0x10FCA2 and their
 *     resets (the phase stand-ins now read the stock counters on +0x1A / +0x12);
 *   the full block swap 0xECB14 (both blocks, both phase objects, the two
 *     16-byte ramp objects, the pair-pointer ids) with the 0x117CE4 re-pin and
 *     the +3 flags; the ramp class 0x49FC0 (init 0xD1D8E / step 0xD1D96);
 *   0x89C0C = the turbine speed 0x89CBA (by value), 0x89C1C / 0x89C0E.
 *
 * NOT PORTED / STOOD IN (each counted in gp_w_t / gp_xw_t):
 *   the phase BODIES (done 0x1138FC / 0x1106DC, entry 0x1132F4 except its
 *     progress-hold and +4 stores, outputs 0x113F16 / 0x1144B0 / 0x110CDC /
 *     0x110C8E / 0x110600, 0x112142, 0x114E26 / 0x113788): a SEQUENCE-ONLY
 *     black box — each phase ends on the documented stand-in GP_SI_* below
 *     (pass counts, the block sync for the inertia phase 9); a non-active
 *     block (B) holds at phase >= 2 (pre-fill) [I]; the outputs read 0;
 *   0xB8246 "on-coming applied" (the request object +0x54): stand-in =
 *     block A's on-coming phase object in its final phase 0x12 [I];
 *   0xEC788 BD1E (0x11632E / 0x116356 / 0xB7C26 → 0xD4FBA): GP_SI_BD1E = 1;
 *   BD14+0x26 (0xEB7BC / 0xEB73E): 0 (dump);
 *     BD14+0x34 (abort pending): 0; 0xEB73E: 1 (no hold, = the r04w chain);
 *   0xEB5A6 + 0xB6BBC (the early next gear's permission): 0 on SH (no
 *     chained pre-select); a host seam sets them;
 *   the EDA48 timer branch (on-coming +0xA vs A+0x24): not modelled;
 *   the slot-34 cyclic 0xEBDBC beyond the sync (BD76 / BD2E / BD30 / DT1 /
 *     0xEC6A6 / 0xEC788 re-evaluation / 0xEDE1C / 0xEE9A4), 0xDDB04 (+0x14),
 *     0xEB288, 0xDDFD8; the torque / line-pressure / notification / E3EC /
 *     E3DE / C190 / garage-engine side calls (0xE5FF6, 0xE5E48, 0xE9404,
 *     0xE9446, 0xE9464, 0xE945E, 0xE941E, 0xE940C, 0xE5A20, 0xE9ED2, 0xE9E5A,
 *     0xEE2B6, 0xEDBA4, 0xAECD8, 0xAED02, 0xAED22, 0xAED2E, 0xAED3A, 0xAEE06,
 *     0xB8574, 0xB6AC4, 0xEAC30, 0xEAC64, 0xEACE0, 0xE9508, 0xEECEC, 0xEEF42,
 *     0xE958A, 0xEEB9C, 0x117B4A, 0x1176E0, 0x117C48, 0x1179D8): counted;
 *   THE NV CALLS ARE NEVER PORTED: 0xEBB6C's 0xBA538 → 0xE8774 → 0xE82AC →
 *     0xE8512 and 0xEBC60's 0xBA52C → 0xE874A → 0xE82A4 → 0xE8512 are
 *     COUNTED (w.nv_end_n / w.nv_abort_n) — g6a_scan.py refuses them in the
 *     image.
 * INPUTS BY VALUE (gp_in_t, app/gp_hook.c): the gear chain's gf_in_t (as
 *   sm_hook_10ms gathers it), the live shift object's BD15 / BD16 / BD17 /
 *   +0x2D (the garage engine owns block A during a garage engagement or
 *   release: the shadow mirrors BD15 / BD16 / BD17 then and runs its own
 *   0xEBB6C end on the live end edge), 0xFFF8D151 (0xEEB04) and 0xFFF8D0E0
 *   (the hold while BD17 == 4), and the r04w chain's E4A7 / E69F (witness).
 *
 * READ-ONLY, ENFORCED: every output TU defines TCU_OUTPUT_PATH — this header
 * #errors under it; app/gp_shadow.c / gp_gear.c define TCU_GP_SHADOW_TU and
 * the live headers (sup_cells.h, drv_cells.h, drv_odiag.h, drv_temp.h,
 * platform.h) #error under it; gp_shadow_scan.py (outputs / shadow / hook,
 * with negative fixtures); sh-elf-gcc pass/fail checks; the live identity
 * (test_g6b with the shadow ticking against the goldens); g6a_scan.py nvm
 * on the shadow's objects and image on tcu_app.elf.
 */
#ifndef TCU_APP_GP_SHADOW_H
#define TCU_APP_GP_SHADOW_H

#if defined(TCU_OUTPUT_PATH)
#error "the gear-pair shadow is read-only: an output path may not include gp_shadow.h"
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "app/sm_shadow.h"

/* ── the stand-ins of the undecoded phase bodies [I], in 10 ms passes ──── */
#define GP_SI_T_SHORT   3u      /* on 1, 0x0A..0x0D; off 1..3, 0x0A..0x0F */
#define GP_SI_T_FILL    8u      /* on 2 (the fill) */
#define GP_SI_T_TORQUE  5u      /* on 4 / 5, off 4 (the torque phase) */
#define GP_SI_BD1E      1u      /* 0xEC788 (D4FBA): 1 (traction) */
#define GP_SI_CF4       1u      /* 0x89CF4 (dump 01; writer 0xFFF8A0A2 not found) */
#define GP_SI_VAR       0u      /* 0x89C06: the f8 array variant (dump 0) */
#define GP_SI_AE31      0u      /* 0xFFF8AE31 == 3: the 0x117B4A override gate (dump 00) */
#define GP_SI_CFA       0u      /* 0x89CFA: BD1E while BD15 == 0 (dump 00) */
#define GP_SI_BD26      0u      /* BD14+0x26 (dump 0xFFF8BD3A = 00) */
#define GP_SI_EB73E     1u      /* 0xEB73E: no hold (the r04w chain's choice) */

/* a phase object (on-coming at ctx+0xB0, off-going at ctx+0x34): the fields
 * the skeleton steps (the stock offsets in the comments) */
typedef struct {
    uint8_t  elem;      /* +0 on +0x13 (ctx+0xC3) / off +8 (ctx+0x3C): 1..5 = A..E, 0 = inactive */
    uint8_t  phase;     /* +1 on +0x12 / off +9 */
    uint8_t  step;      /* +2 on +0x28 / off +0x18 */
    uint8_t  type;      /* +3 on +0x29 (0x1132E2) / off +0x19 (0x10FF9E) */
    uint8_t  f7;        /* +4 on +7 (→ 0x0E; stand-in 0) */
    uint8_t  f4;        /* +5 on +4 (1 at the 0x0D entry, 0x1135C6) */
    uint8_t  mode;      /* +6 on +8 (0x112916: phase == 1) / off +6 (0x110600: stand-in 0) */
    uint8_t  tin;       /* +7 ours: passes in this phase (the done stand-in) */
    uint16_t out;       /* +8 on +0x16 / off +0xE (the phase bodies: stand-in 0) */
    uint16_t phases_n;  /* +10 ours: phase entries since its start (witness) */
} gp_po_t;
_Static_assert(sizeof(gp_po_t) == 12u, "gp_po_t is 12 bytes (witness_gp.py)");

/* a shift context: block A (0xFFF8BD6C, active) / block B (0xFFF8BEB4) */
typedef struct {
    uint8_t  sync;      /* +0  +0x00 the sync flag (0x117F48) */
    int8_t   to;        /* +1  +0x01 (0x1176C4) */
    int8_t   from;      /* +2  +0x02 */
    uint8_t  act;       /* +3  +0x03 the active context (0xECB14) */
    uint8_t  f4;        /* +4  +0x04 (0xED9B2: 0; 0xDDA54 reads it) */
    uint8_t  kind;      /* +5  +0x05 0xB7C14(from, to) */
    uint8_t  dir;       /* +6  +0x2C the sync direction (0xFF = unset) */
    uint8_t  hold;      /* +7  +0x31 the progress hold (0x11793E: 1) */
    uint8_t  p27;       /* +8  +0x27 force sync */
    uint8_t  x1e;       /* +9  +0x1E (B: 0xEC134 the chained sub-type; A: BD8A, 0xEBA6E: 0) */
    int16_t  slip;      /* +10 +0x10 (0x117A74) */
    gp_po_t  on;        /* +12 +0xB0 */
    gp_po_t  off;       /* +24 +0x34 */
} gp_blk_t;
_Static_assert(sizeof(gp_blk_t) == 36u, "gp_blk_t is 36 bytes (witness_gp.py)");

/* the would-be producer request, ids 1..4 (0xDD7B8 / 0xDD862) */
typedef struct {
    uint8_t  elem;      /* E33C+0 / +1 / +5 / +6 */
    uint8_t  phase;     /* the phase object's phase */
    uint8_t  mode;      /* on +8 / off +6 */
    uint8_t  pad;
    uint16_t value;     /* on +0x16 / off +0xE (stand-in 0) */
    uint16_t n;         /* passes it would have requested (saturating) */
} gp_prod_t;
_Static_assert(sizeof(gp_prod_t) == 8u, "gp_prod_t is 8 bytes (witness_gp.py)");

/* the live inputs, BY VALUE (app/gp_hook.c) */
typedef struct {
    gf_in_t  gf;        /* +0  the gear chain's inputs (as sm_hook_10ms gathers them) */
    uint8_t  lbd15;     /* +16 live 0xFFF8BD15 */
    uint8_t  lbd16;     /* +17 live 0xFFF8BD16 */
    uint8_t  lbd17;     /* +18 live 0xFFF8BD17 */
    uint8_t  lbd41;     /* +19 live 0xFFF8BD41 (+0x2D) */
    uint8_t  ld151;     /* +20 live 0xFFF8D151 (0xEEB04) */
    uint8_t  ld0e0;     /* +21 live 0xFFF8D0E0 (the hold while BD17 == 4) */
    int8_t   r_e4a7;    /* +22 the r04w chain's E4A7 (witness) */
    int8_t   r_e69f;    /* +23 the r04w chain's E69F (witness) */
} gp_in_t;
_Static_assert(sizeof(gp_in_t) == 24u, "gp_in_t is 24 bytes (witness_gp.py)");

/* one event of the shadow (a ring) */
typedef struct {
    uint16_t pass;      /* +0 10 ms pass (low 16) */
    uint8_t  kind;      /* +2 GP_EV_* */
    int8_t   a, b;      /* +3 +4 from / to (or phase) */
    uint8_t  c, d, e;   /* +5 +6 +7 */
} gp_ev_t;
_Static_assert(sizeof(gp_ev_t) == 8u, "gp_ev_t is 8 bytes (witness_gp.py)");
#define GP_EV_N         16u
#define GP_EV_PRESEL    1u      /* 0xEB9EC: a=from b=to c=on d=off e=type */
#define GP_EV_COMMIT    2u      /* 0xEBA6E: a=old b=new c=on d=off e=presel */
#define GP_EV_SWAP      3u      /* 0xECB14 */
#define GP_EV_DONE      4u      /* 0xEDA48 → 1: a=from b=to c=bd41 */
#define GP_EV_END       5u      /* 0xEBC60: a=from b=to c=bd41 d=nv */
#define GP_EV_PHON      6u      /* block A on-coming phase entry: a=phase b=step c=elem */
#define GP_EV_PHOFF     7u      /* block A off-going phase entry */
#define GP_EV_GEND      8u      /* the live garage end → 0xEBB6C */

/* the witnesses (all counters u32, saturating not needed) */
typedef struct {
    uint32_t init_n;        /* +0x00 */
    uint32_t pass_n;        /* +0x04 10 ms passes (modes 1..3) */
    uint32_t skip_mode_n;   /* +0x08 */
    uint32_t presel_n;      /* +0x0C 0xEB9EC calls */
    uint32_t presel_t0_n;   /* +0x10   type 0: 0x11755E (nothing prepared) */
    uint32_t presel_busy_n; /* +0x14   0xEEB04 busy: nothing */
    uint32_t commit_n;      /* +0x18 0xEBA6E calls */
    uint32_t commit_ps_n;   /* +0x1C   pre-selected (0xECB14 swap) */
    uint32_t mismatch_n;    /* +0x20   the 0x36B / 0x13 / 0x53 block-B check */
    uint32_t done_n;        /* +0x24 0xEDA48 → 1 (gear branch) */
    uint32_t end_n;         /* +0x28 0xEBC60 (the gear-pair end) */
    uint32_t gend_n;        /* +0x2C 0xEBB6C on the live garage end */
    uint32_t nv_end_n;      /* +0x30 0xBA538 (0xEBB6C's NV shift counter) — NOT CALLED */
    uint32_t nv_abort_n;    /* +0x34 0xBA52C (0xEBC60's NV counter) — NOT CALLED */
    uint32_t si_phase_n;    /* +0x38 phase-done stand-in decisions */
    uint32_t si_appl_n;     /* +0x3C 0xB8246 stand-in evaluations */
    uint32_t si_misc_n;     /* +0x40 BD1E / BD1B / BD26 / EB73E / EB5A6 / B6BBC / timer stand-ins taken */
    uint32_t unp_n;         /* +0x44 unported side calls (torque, notifications, line pressure, …) */
    uint32_t report_n;      /* +0x48 the stock's event reports (0x36B / 0x365 / 0x367 / 0x373), counted */
    uint32_t err_n;         /* +0x4C a table index out of range */
    uint32_t own_n;         /* +0x50 passes the live garage owned the shift object */
    uint32_t block_n;       /* +0x54 passes the BD15 interlock (0xBABD8) held a change */
    uint32_t dis_n;         /* +0x58 passes the coupled E4A7 / E69F differ from the r04w chain's */
    uint32_t us_last;       /* +0x5C µs of the last pass (the coupled chain + the shadow) */
    uint32_t us_max;        /* +0x60 */
    uint32_t eb1e8_n;       /* +0x64 0xDDA54 → 0xEB1E8 (+0x2D = 1) */
} gp_w_t;
_Static_assert(sizeof(gp_w_t) == 0x68u, "gp_w_t is 0x68 bytes (witness_gp.py)");

typedef struct {
    /* the shift object's header BD14 (SHADOW copies; the stock BD14 offsets in
     * the comments — our struct packs them from offset 0) */
    uint8_t  bd15;      /* +0x01 */
    uint8_t  bd16;      /* +0x02 */
    uint8_t  bd17;      /* +0x03 */
    uint8_t  bd18;      /* +0x04 the commit used the pre-selected context */
    uint8_t  bd1a;      /* +0x06 */
    uint8_t  bd1b;      /* +0x07 */
    uint8_t  bd1e;      /* +0x0A */
    uint8_t  bd22;      /* +0x0E */
    uint8_t  bd3c;      /* +0x28 skip */
    uint8_t  bd40;      /* +0x2C */
    uint8_t  bd41;      /* +0x2D completion flag */
    uint8_t  bd45;      /* +0x31 */
    uint8_t  bd48;      /* +0x34 abort pending (stand-in 0) */
    uint8_t  bd49;      /* +0x35 */
    uint8_t  bd5d;      /* +0x49 */
    uint8_t  bd67;      /* +0x53 */
    uint8_t  bd68;      /* +0x54 */
    uint16_t bd38;      /* +0x24 the ended off-going output (0xEBC60) */
    /* E33C (SHADOW copy; the stock offsets in the names) */
    uint8_t  e0, e1;    /* E33C+0 / +1 the active shift's on / off (ids 1 / 2) */
    uint8_t  e5, e6;    /* E33C+5 / +6 the pre-selected shift's (ids 3 / 4) */
    uint8_t  e14;       /* E33C+0x14 pre-select pending */
    uint8_t  own;       /* ours: the live garage owns the shift object */
    int16_t  ea;        /* E33C+0xA turbine snapshot */
    int16_t  e10;       /* E33C+0x10 engine snapshot */
    uint8_t  ev_head;   /* our offset +0x1E */
    uint8_t  pad1f;
    gp_blk_t a;         /* +0x20 block A */
    gp_blk_t b;         /* +0x44 block B */
    gp_prod_t prod[4];  /* +0x68 ids 1..4 */
    gp_in_t  in;        /* +0x88 the last inputs */
    gp_w_t   w;         /* +0xA0 */
    gp_ev_t  ev[GP_EV_N]; /* +0x108 */
} gp_t;
_Static_assert(offsetof(gp_t, a) == 0x20u, "gp_t layout (witness_gp.py)");
_Static_assert(offsetof(gp_t, b) == 0x44u, "gp_t layout (witness_gp.py)");
_Static_assert(offsetof(gp_t, prod) == 0x68u, "gp_t layout (witness_gp.py)");
_Static_assert(offsetof(gp_t, in) == 0x88u, "gp_t layout (witness_gp.py)");
_Static_assert(offsetof(gp_t, w) == 0xA0u, "gp_t layout (witness_gp.py)");
_Static_assert(offsetof(gp_t, ev) == 0x108u, "gp_t layout (witness_gp.py)");
_Static_assert(sizeof(gp_t) == 0x188u, "gp_t is 0x188 bytes (.gp_shadow, witness_gp.py)");

/* ── 2026-11-20: the retired stand-ins' state (own RAM .gp_ext 0xFFF86794,
 * .gp_ramp 0xFFF869BC; gp_t / gf_t untouched) ───────────────────────────── */
/* the pointer-carrying part of a phase object (0x117CE4's re-pin targets: on
 * +0x48 / off +0x38 = its context; the pair-cell pointers on +0x4C.. / off
 * +0x3C.. travel with the object in the swap): ctxp = the block index the object
 * points to, pair = the three pair-cell ids its pointers hold [I: the boot
 * assignment is the constructor's, undecoded — one id per cell] */
typedef struct {
    uint8_t  ctxp;
    uint8_t  pair[3];
} gp_xpo_t;
_Static_assert(sizeof(gp_xpo_t) == 4u, "gp_xpo_t is 4 bytes (witness_gp.py)");

/* a shift context's extension (swapped with it by 0xECB14) */
typedef struct {
    uint8_t  k7;        /* +0  ctx+7  the kind (0x1180BE) */
    uint8_t  f8;        /* +1  ctx+8  the shift-character index */
    uint8_t  cls[2];    /* +2  the class 0xEC182 for the call-site flag 0 / 1 */
    uint8_t  ov;        /* +4  ctx+0x30 the override's sticky flag */
    uint8_t  on_c10;    /* +5  on +0x10 */
    uint8_t  on_c1c;    /* +6  on +0x1C */
    uint8_t  off_c2c;   /* +7  off +0x2C */
    uint16_t c22;       /* +8  ctx+0x22 (BD8E for block A) */
    uint16_t on_c34;    /* +10 on +0x34: the last phase's duration (+0x1A at its end) */
    uint16_t on_ca;     /* +12 on +0xA */
    uint16_t on_cc;     /* +14 on +0xC */
    uint16_t on_c3e;    /* +16 on +0x3E */
    uint16_t off_c22;   /* +18 off +0x22: the last phase's duration (+0x12 at its end) */
    gp_xpo_t xon;       /* +20 */
    gp_xpo_t xoff;      /* +24 */
} gp_xblk_t;
_Static_assert(sizeof(gp_xblk_t) == 28u, "gp_xblk_t is 28 bytes (witness_gp.py)");

/* the inputs by value the stand-in retirements need (gp_shadow_set_in2) */
typedef struct {
    int16_t  tq;        /* 0x89CA0 the input torque [I: our vehicle torque / 10; 0 without frames] */
    uint8_t  pedal;     /* 0x89CD8 */
    uint8_t  pad;
    uint16_t now_ms;    /* 0x89CF8 the ms clock (the ramp objects) */
} gp_in2_t;
_Static_assert(sizeof(gp_in2_t) == 6u, "gp_in2_t is 6 bytes (witness_gp.py)");

/* the witnesses of the retirements (u16, saturating) */
typedef struct {
    uint16_t k_start;   /* +0  0x117B4A calls from the start (0xED906) */
    uint16_t k_cyc;     /* +2  0x117B4A calls from the slot-34 cyclic */
    uint16_t k_chg;     /* +4  kind changes */
    uint16_t k_ovr;     /* +6  the override taken */
    uint16_t k_reload;  /* +8  0xED92E (0x1176E0 + the torque modules): counted, not ported */
    uint16_t k_a1032;   /* +10 0xA1032 evaluations (stand-in 0, exact: cal +0x7C = 1) */
    uint16_t k_adapt;   /* +12 0xAEDC8 / 0xAED0E pass-throughs (exact: cal-disabled) */
    uint16_t c_pass;    /* +14 slot-28 counter passes (0xEC0E0) */
    uint16_t repin_n;   /* +16 phase-object pointers re-pinned by 0x117CE4 after a swap */
    uint16_t ramp_n;    /* +18 ramp object init / step calls */
    uint16_t bd1b_n;    /* +20 BD1B set by 0xEC6A6 */
    uint16_t c1_n;      /* +22 phase durations taken from the class-1 rows */
} gp_xw_t;
_Static_assert(sizeof(gp_xw_t) == 24u, "gp_xw_t is 24 bytes (witness_gp.py)");

typedef struct {
    gp_xblk_t blk[2];   /* +0   block A / B */
    gp_in2_t  in2;      /* +56 */
    uint16_t  bd2e;     /* +62 BD14+0x1A */
    uint8_t   bd30;     /* +64 BD14+0x1C */
    uint8_t   bd5f;     /* +65 BD14+0x4B */
    uint8_t   bd51;     /* +66 BD14+0x3D */
    uint8_t   cls1c;    /* +67 0x89C1C = clamp_u8(torque / 6 + 34) */
    uint8_t   c0e;      /* +68 0x89C0E = clamp_u8(turbine / 32) */
    uint8_t   pad;      /* +69 */
    gp_xw_t   w;        /* +70 */
} gp_x_t;
_Static_assert(offsetof(gp_x_t, in2) == 56u, "gp_x_t layout (witness_gp.py)");
_Static_assert(offsetof(gp_x_t, w) == 70u, "gp_x_t layout (witness_gp.py)");
_Static_assert(sizeof(gp_x_t) == 94u, "gp_x_t is 94 bytes (.gp_ext, witness_gp.py)");
_Static_assert(sizeof(gp_x_t) <= 0x6Cu, "gp_x_t must fit the dump-zero gap after .gp_gear (108 B)");

/* the 16-byte float ramp object of the vtable 0x49FC0 (on +0x68 / off +0x58;
 * the vtable word at object+0x10 is the constant 0x49FC0 and not stored) */
typedef struct {
    float    v;         /* +0  value */
    uint16_t ts;        /* +4  the ms stamp (0x89CF8) */
    uint16_t pad;
    float    mn;        /* +8  min */
    float    mx;        /* +12 max */
} gp_ramp_t;
_Static_assert(sizeof(gp_ramp_t) == 16u, "gp_ramp_t is 16 bytes (0xD1C4C)");

void gp_ramp_init(gp_ramp_t *r, int16_t value, int16_t mn, int16_t mx, uint16_t now);   /* 0xD1D8E */
int16_t gp_ramp_step(gp_ramp_t *r, int16_t rate, uint16_t now);                        /* 0xD1D96 */
uint8_t gp_class_ec182(uint8_t kind, uint8_t flag, uint8_t type);                      /* 0xEC182 */
uint8_t gp_class_ec1f0(uint8_t kind, uint8_t flag, uint8_t type);                      /* 0xEC1F0 */

void gp_shadow_set_in2(const gp_in2_t *in2);                 /* before gp_shadow_pass */
const volatile gp_x_t *gp_shadow_x(void);
const volatile gp_ramp_t (*gp_shadow_ramps(void))[2];        /* [block][0 = on, 1 = off] */

void gp_shadow_init(void);
void gp_shadow_pass(const gp_in_t *in);          /* the 10 ms pass (after sm_hook_10ms) */
const volatile gp_t *gp_shadow_state(void);

/* the coupled gear chain (app/gp_gear.c = app/sm_gear.c with SMG_COUPLED) */
void gp_gear_init(void);
void gp_gear_pass(const gf_in_t *in);
const volatile gf_t *gp_gear_state(void);

/* the hooks the coupled chain calls (app/sm_gear.c's GF_* bindings) */
uint8_t  gp_cpl_bd15(void);
uint8_t  gp_cpl_bd40(void);
bool     gp_cpl_eb7bc(int8_t cmd7, int8_t e6a1);                    /* 0xEB7BC */
uint8_t  gp_cpl_dda54(int base);                                    /* 0xDDA54's BD15 term */
bool     gp_cpl_eb1f0(uint8_t pf, uint8_t e344);                    /* 0xEB1F0 → 0xEDA48 */
void     gp_cpl_dde78(int8_t g, uint8_t skip, uint8_t presel);      /* 0xDDE78 → 0xEBC60 */
void     gp_cpl_ddde8(int8_t old, int8_t nw, uint8_t skip, uint8_t presel, uint8_t e69d); /* 0xDDDE8 */
void     gp_cpl_dd954_change(int8_t c3, int8_t c2b, int8_t c25, int8_t prev); /* 0xDD954's change block */
bool     gp_cpl_early_ok(int8_t c25, int8_t c2b, int8_t nx, uint16_t ne, uint16_t nt); /* 0xEB5A6 ∧ 0xB6BBC */
uint8_t  gp_cpl_eb73e(void);                                        /* 0xEB73E */
uint16_t gp_cpl_ratio(int8_t gear);                                 /* 0xD7C8C */

/* app/gp_hook.c — the gather */
void gp_hook_init(void);
void gp_hook_10ms(void);                         /* main.c's task_10ms, after sm_hook_10ms */
void gp_hook_feed(const gp_in_t *in);            /* the part after the gather (host harness) */
void gp_hook_time(uint32_t us);                  /* the CPU witness of one pass */

#if !defined(__sh__)
/* host seams: the chained pre-select (0xEB5A6 ∧ 0xB6BBC accept, BD14+0x26 = 1
 * with 0xEC90A == 0); the shift object's abort (BD14+0x34) */
void gp_shadow_host_si(uint8_t early_ok, uint8_t bd26);
void gp_shadow_host_ae31(uint8_t ae31);                      /* the 0x117B4A override gate */
/* the swap probe (host): 0xECB14 on the shadow's own blocks, and the mutable views the test fills */
void gp_shadow_host_ecb14(void);
gp_t *gp_shadow_host_mut(void);
gp_x_t *gp_shadow_host_xmut(void);
gp_ramp_t (*gp_shadow_host_rmut(void))[2];
void gp_gear_host_si(uint8_t ae2d, int8_t c43);
#endif

#endif /* TCU_APP_GP_SHADOW_H */
