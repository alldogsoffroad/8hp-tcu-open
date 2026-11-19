/* gs_cand_bind.h — the BINDING of INSTANCE 2 of the garage engine template
 * (app/gs_engine.c + app/gs_fill.c): the future G6a CANDIDATE.
 * G6-0, 2026-11-19, ~/tcu_sweep/g6_0_port.md.
 *
 * Included FIRST by app/gs_cand.c (the engine) and app/gs_cand_fill.c (its
 * fill instance), never by anything else. It
 *   - binds the request objects to the candidate's own block (GSE_REQ_BASE
 *     → GSC_REQ_BASE; gs_fill.h derives GSE_OBJ_* from it), and
 *   - renames every exported name of the engine (gs_shadow_* → gs_cand_*)
 *     and of the fill instance (gsf_* → gcf_*, gs_fill_cells →
 *     gcf_fill_cells), so that instance 1 and instance 2 link side by side
 *     and share nothing but the const calibration (app/gs_cal.c) and the
 *     shared types.
 *
 * HOST-ONLY IN G6-0 (the plan's G6-0 is "desk + host, no flash"); G6a
 * gives the candidate SH RAM (its sections and GSC_REQ_BASE, below). On the host the
 * candidate's request objects sit in their own page GSC_REQ_BASE, mapped by
 * app/gs_cand_fill.c (outside the stock cell window 0xFFF80000..0xFFF96000
 * the tests hash, so instance 2 can never touch a cell instance 1 or the
 * live path owns).
 *
 * Host negatives (the identity test must FAIL with each):
 *   GSC_NEG_FILL_SHIFT  the fill instance bound one object off the engine's
 *                       (a binding error between the two halves);
 *   GSC_NEG_ALIAS       the candidate's request objects ARE instance 1's (a
 *                       second writer of the same objects). */
#ifndef TCU_APP_GS_CAND_BIND_H
#define TCU_APP_GS_CAND_BIND_H

#if defined(TCU_OUTPUT_PATH)
#error "the garage shadow is read-only: an output path may not include gs_cand_bind.h"
#endif
#if defined(TCU_RAM_APP) || defined(TCU_LOADER)
#error "gs_cand: the G6a candidate is for the code-flash application only (not the RAM application or the loader)"
#endif

#define GSE_BINDING     2

/* ── G6a (2026-11-19, ~/tcu_sweep/g6a_port.md): THE SH CONFIGURATION ──────
 * On SH (and on the host with GSC_G6A — the G6a host test, which models the
 * image) the candidate is the DUAL-RUN instance of the merged producer:
 *   - its request objects at GSC_REQ_BASE = 0xFFF925C0 (.gsc_req, 7 × 0x60),
 *     its statics and its fill instance's statics in .gsc_obj — stock RAM
 *     the driving dump holds all zero (0xFFF925BE..0xFFF92C68) and no code
 *     of ours names (literal scan at the port); the whole block cleared by
 *     gs_cand_init (GSE_CLEAR_BEGIN / END: NOLOAD is not cleared by start.S);
 *   - no recorders (GSE_REC 0: RAM; the comparator app/g6a.c records);
 *   - AE28 = the live 0xFFF8AE28 by value (GSE_AE28_IN, the G6b form);
 *   - its own boot-state-1 pass (GSE_BOOT_ENTRY, the G6b slot);
 *   - D-G6-2 0x89C3D forced 0 and D-G6-3 gear target 1st: GS_FORCED 1
 *     (asserted below). */
#if defined(__sh__) || defined(GSC_G6A)
#  define GSC_SH_FORM   1
#else
#  define GSC_SH_FORM   0
#endif

#if GSC_SH_FORM
#  if defined(GSC_NEG_ALIAS) || defined(GSC_NEG_FILL_SHIFT) || defined(GSC_NEG_LIVE_ALIAS) || \
      defined(GSC_NEG_C3D)
#    if defined(__sh__)
#      error "gs_cand: the binding negatives are host-only tests"
#    endif
#  endif
#  if defined(GSC_NEG_C3D)
/* G6a host negative: the candidate perturbed INSIDE its own RAM — 0x89C3D
 * held at the live producer's 1 (the old value): no commit ever (the
 * comparator must report the bridged engagements as UNEXPECTED, the
 * identity must still hold) */
#    define GSE_NEG_C3D_HOOK() (G.c3d = 1u)
#  endif
#  define GSC_REQ_BASE    0xFFF925C0u               /* .gsc_req (linker.ld ASSERTs it) */
#  define GSE_REC         0
#  define GSE_AE28_IN     1
#  define GSE_BOOT_ENTRY  1
#  define GSE_SEC_OBJ     ".gsc_obj"
#  define GSE_SEC_REC     ".gsc_obj"
#  define GSE_SEC_PRG     ".gsc_obj"
#  define GSE_SEC_RC1     ".gsc_obj"
#  define GSE_SEC_REL     ".gsc_obj"
#  define GSE_CLEAR_BEGIN _gsc_obj_start            /* linker.ld __gsc_obj_start (SH symbols: + '_') */
#  define GSE_CLEAR_END   _gsc_obj_end
/* G6b: the tick's slot groups timed (app/g6a.c, the CPU witnesses) */
void g6a_stage(unsigned k);
#  define GSE_STAGE(k)    g6a_stage(k)
#else
/* G6-0 host binding: the candidate's request objects in their own host
 * page (7 × 0x60), mapped by app/gs_cand_fill.c */
#  define GSC_REQ_BASE    0xFFFC0000u
#  define GSC_REQ_PAGE    0x1000u
#endif

#if defined(GSC_NEG_ALIAS)
#  define GSE_REQ_BASE  0xFFF911A0u                     /* instance 1's GS_REQ_BASE */
#elif defined(GSC_NEG_LIVE_ALIAS)
#  define GSE_REQ_BASE  (GSC_NEG_LIVE_ALIAS)            /* G6a negative: the LIVE request objects
                                                         * (CMakeLists.txt passes their address) */
#elif defined(GSC_NEG_FILL_SHIFT) && defined(GSC_FILL_TU)
#  define GSE_REQ_BASE  (GSC_REQ_BASE + 0x060u)         /* the fill one object off */
#else
#  define GSE_REQ_BASE  GSC_REQ_BASE
#endif

/* the engine's exports (app/gs_shadow.h) */
#define gs_shadow_init          gs_cand_init
#define gs_shadow_tick_10ms     gs_cand_tick_10ms
#define gs_shadow_rel           gs_cand_rel
#define gs_shadow_releasing     gs_cand_releasing
#define gs_shadow_wit           gs_cand_wit
#define gs_shadow_rec           gs_cand_rec
#define gs_shadow_cur           gs_cand_cur
#define gs_shadow_pf            gs_cand_pf
#define gs_shadow_gear          gs_cand_gear
#define gs_shadow_phase         gs_cand_phase
#define gs_shadow_pending       gs_cand_pending
#define gs_shadow_e6ef          gs_cand_e6ef
#define gs_shadow_ae28          gs_cand_ae28
#define gs_shadow_d0f           gs_cand_d0f
#define gs_shadow_reqmode       gs_cand_reqmode
#define gs_shadow_bd76          gs_cand_bd76
#define gs_shadow_bd30          gs_cand_bd30
#define gs_shadow_sync          gs_cand_sync
#define gs_shadow_slip          gs_cand_slip
#define gs_shadow_host_ceca     gs_cand_host_ceca
#define gs_shadow_host_rel      gs_cand_host_rel
#define gs_shadow_host_standins gs_cand_host_standins
#define gs_shadow_host_sub      gs_cand_host_sub
#define gs_shadow_host_state    gs_cand_host_state
#define gs_shadow_fillst        gs_cand_fillst
#define gs_shadow_boot_pass     gs_cand_boot_pass
#define gs_shadow_ae28_x        gs_cand_ae28_x

/* the fill instance's exports (app/gs_fill.h, app/gs_fill.c) */
#define gs_fill_cells           gcf_fill_cells
#define gsf_bind                gcf_bind
#define gsf_bind_in             gcf_bind_in
#define gsf_fl8                 gcf_fl8
#define gsf_fl16                gcf_fl16
#define gsf_fl32                gcf_fl32
#define gsf_cell_ptr            gcf_cell_ptr
#define gsf_construct           gcf_construct
#define gsf_e4e5c               gcf_e4e5c
#define gsf_e5602               gcf_e5602
#define gsf_e5624               gcf_e5624
#define gsf_e566c               gcf_e566c
#define gsf_107c3e              gcf_107c3e
#define gsf_b6eae               gcf_b6eae
#define gsf_b6e10               gcf_b6e10
#define gsf_b6d6c               gcf_b6d6c
#define gsf_b7164               gcf_b7164
#define gsf_rec_frac            gcf_rec_frac
#define gsf_e4c18               gcf_e4c18
#define gsf_e4bec               gcf_e4bec
#define gsf_e4dba               gcf_e4dba
#define gsf_set_bd16            gcf_set_bd16
#define gsf_set_pf              gcf_set_pf
#define gsf_host_e469c_off      gcf_host_e469c_off
#define gsf_unp_n               gcf_unp_n
#define gsf_f19f_n              gcf_f19f_n
#define gsf_shift_n             gcf_shift_n
#define gsf_e469c_n             gcf_e469c_n

#endif /* TCU_APP_GS_CAND_BIND_H */
