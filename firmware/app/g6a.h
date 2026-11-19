/* g6a.h — G6a, THE DUAL-RUN COMPARATOR (ours, not stock; 2026-11-19,
 * ~/tcu_sweep/g6a_port.md; plan ~/tcu_sweep/g6_migration_plan.md §3 G6a).
 *
 * The CANDIDATE (app/gs_cand.c + app/gs_cand_fill.c: instance 2 of the
 * garage engine template, the merged producer — engine + sources 5..10 +
 * holder + setter + actuators — in its G6b form: boot pass at boot state 1,
 * the 10 ms pass after park_tick, AE28 from the live 0xFFF8AE28, D-G6-2
 * 0x89C3D forced 0, D-G6-3 gear target 1st) runs on its OWN RAM (.gsc_req,
 * .gsc_obj) and writes nothing live. Every pass this comparator compares
 * the candidate's A..Sys currents (ch 0..6) and, for A..E, the request mode
 * +7 and the fill-FSM state +0x34, with WHAT GOES ON THE WIRE TODAY: the
 * value drv_reqlaw.c's rq_prod_guarded() returns for the F stage that
 * follows this 10 ms task — the bridge's applied value (the shadow's
 * current) while the bridge is applied, else the live producer's
 * 0xFFF84B78 through the guard (500 / 8500, a fill-state value in
 * [500, 8500], else the r03f table). It writes ONLY its own RAM (.g6a).
 *
 * CLASSES (one per comparator pass; disagreement = any current, mode or
 * state differs between the candidate and the wire source):
 *   G6A_C_AGREE    no difference;
 *   G6A_C_BOOT     expected — inside the BOOT WINDOW: from the candidate's
 *                  boot pass until the first pass with candidate E28E == 0,
 *                  no candidate element filling (state 1 / 2) and full
 *                  agreement, at most G6A_BOOT_MAX passes (the stock's E28E
 *                  starts at 1, 0xD0C5A: the holder releases every element
 *                  for 2 passes, then A refills; the live producer has A
 *                  applied from its boot pass);
 *   G6A_C_FENCED   expected — the bridge is NOT applied, the shadow's engine
 *                  OWNS the content (power flow != 0, releasing, a change
 *                  pending, its engine active, or its exception machine E28E
 *                  != 0 — e.g. the all-release of 0xD0C94 on an engine stop,
 *                  0x89D12) and the candidate equals the shadow exactly: the
 *                  live producer (no engine) keeps the P content where the
 *                  stock's engine drives — what G6b's monitor fences or lets
 *                  through by decision;
 *   G6A_C_FSMTAIL  expected — the bridge is not applied, the candidate
 *                  equals the shadow exactly, every current and mode equals
 *                  the wire and each differing fill state is a non-filling
 *                  state (0 off, 3 applied, 4 / 5 release) on both sides —
 *                  e.g. the one-pass 3 → 4 edge at a release's completion,
 *                  where the current is already released: the live request
 *                  objects never ran the engagement, so after a release the
 *                  candidate's released elements ramp down internally (+0x46)
 *                  while the live ones sit at 0 — the wire agrees;
 *   G6A_C_UNEXP    everything else (in particular any pass, after the boot
 *                  window, where the candidate differs from the shadow while
 *                  the bridge applies the shadow, or differs from the live
 *                  producer while the shadow is idle).
 * The comparator is COMMON-MODE with the shadow (the same engine source):
 * it proves the binding, the slot order and the boot pass, not the logic.
 */
/* G6c (2026-11-19, ~/tcu_sweep/g6c_port.md): THE SHADOW IS DELETED (D-G6-7)
 * and with it every candidate-vs-shadow comparison. g6a_t keeps its r04o
 * layout (the bench tools) with these fields retired, always 0: cvs_n,
 * cvs_all, sh_us_max / sh_us_last, g6a_rec_t.sph; class FSMTAIL is never
 * taken (r04o's FSMTAIL passes count as AGREE: candidate == wire). The
 * classes now: AGREE candidate == wire; BOOT inside the boot window (the
 * E28E boot fence); FENCED the monitor chose the live producer (fence) or
 * the latched P content; UNEXP the seam carried the candidate and the wire
 * differs (a guard fallback). The law cross-check, the guard replica and
 * the CPU witnesses are unchanged. The text below is the r04o note. */
#ifndef TCU_APP_G6A_H
#define TCU_APP_G6A_H

#if defined(TCU_OUTPUT_PATH)
#error "the G6a candidate is read-only: an output path may not include g6a.h"
#endif
#if defined(TCU_RAM_APP) || defined(TCU_LOADER)
#error "g6a: the G6a dual-run is for the code-flash application only"
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app/gs_shadow.h"

#define G6A_MAGIC       0x47364131u     /* "G6A1" */
#define G6A_BOOT_MAX    16u             /* the boot window closes by this pass */
#define G6A_REC_N       16u             /* the first disagreeing passes (any class) */
#define G6A_UREC_N      8u              /* the first unexpected passes */

/* G6b: the candidate tick's slot groups (gs_engine.c GSE_STAGE marks) */
#define G6A_ST_HEAD     0u      /* the pass head: inputs, fill_bind */
#define G6A_ST_S28      1u      /* slot 28: timers, element tick / update (0xE5624 / 0xE566C) */
#define G6A_ST_S29      2u      /* slot 29: 0xD4DC2 engine state */
#define G6A_ST_S30      3u      /* slot 30: AE28, E28E 0xD093A, 0x103F56, 0xD15A6 */
#define G6A_ST_S3132    4u      /* slots 31 / 32: commit 0xCFD1A, 0x103CEC, 0xD0BAA */
#define G6A_ST_S34      5u      /* slot 34: sync progress 0x1175D6 */
#define G6A_ST_S35      6u      /* slot 35: element steps / outputs 0x10CE88 / 0x10CECA ×3 */
#define G6A_ST_S42      7u      /* slot 42: producer 0xDC4C4 + actuators */
#define G6A_ST_TAIL     8u      /* the tail: recorders, witnesses */
#define G6A_NST         9u

#define G6A_C_AGREE     0u
#define G6A_C_BOOT      1u
#define G6A_C_FENCED    2u
#define G6A_C_FSMTAIL   3u
#define G6A_C_UNEXP     4u
#define G6A_NCLS        5u

/* G6b CLASSES (GS_G6B, gs_bridge_g6b()): the wire IS the candidate unless
 * the monitor fences (the E28E fence: the live producer) or latched (the P
 * content). AGREE = wire == candidate == shadow; BOOT = inside the boot
 * window (the boot fence); FENCED = wire != candidate because the monitor
 * fenced or latched, candidate == shadow; FSMTAIL = wire == candidate,
 * candidate != shadow only in non-filling fill states; UNEXP = anything else
 * (the seam carried the candidate and the wire differs — a guard fallback —,
 * or candidate != shadow in a current or mode after the boot window). */

/* g6a_rec_t.src */
#define G6A_S_APPLIED   0x01u   /* the bridge applied this pass (wire = shadow) */
#define G6A_S_SHREL     0x02u   /* the shadow releasing (BD16 == 4) */
#define G6A_S_OWNED     0x04u   /* the shadow owns the content (pf / releasing / pending / active) */
#define G6A_S_CEQS      0x08u   /* candidate == shadow (currents, modes, states) */
#define G6A_S_BOOTWIN   0x10u   /* inside the boot window */
#define G6A_S_CREL      0x20u   /* the candidate releasing */
#define G6A_S_STARTED   0x40u   /* the live producer has run (else the wire is the r03f table) */

/* one disagreeing pass (32 bytes) */
typedef struct {
    uint16_t pass;          /* comparator pass (0 = the boot pass at boot state 1) */
    uint8_t  cls;           /* G6A_C_* */
    uint8_t  dcur;          /* bit ch 0..6 (A D B E C WK Sys): current differs */
    uint16_t dms;           /* bit ch 0..4: mode differs; bit 8 + ch: fill state differs */
    uint8_t  src;           /* G6A_S_* */
    uint8_t  e28e_pf;       /* candidate E28E (low nibble) | candidate pf << 4 */
    uint8_t  cph[2];        /* candidate phases: first (D0DC), third (D1B4) */
    uint8_t  sph[2];        /* shadow phases: first, third */
    uint16_t cand[5];       /* candidate currents A D B E C (0.1 mA) */
    uint16_t wire[5];       /* the wire source's currents A D B E C */
} g6a_rec_t;
_Static_assert(sizeof(g6a_rec_t) == 32u, "g6a_rec_t is 32 bytes (tools/bench/witness_g6a.py)");

typedef struct {
    uint32_t magic;         /* G6A_MAGIC after g6a_init */
    uint32_t passes;        /* comparator passes (incl. the boot pass) */
    uint32_t cls_n[G6A_NCLS];       /* passes per class */
    uint32_t dis_ch[7];     /* passes with ch's current differing (any class) */
    uint32_t unexp_ch[7];   /* ... in an UNEXP pass */
    uint32_t ms_dis_n;      /* passes with a mode / state difference (any class) */
    uint32_t cvs_n;         /* bridge-applied passes where candidate != shadow (any field) */
    uint32_t applied_n;     /* passes with the bridge applied */
    uint32_t first_unexp;   /* comparator pass of the first UNEXP (0xFFFFFFFF none) */
    uint32_t last_unexp;    /* ... of the last */
    uint32_t law_n;         /* law cross-checks made (seq2 'U', a law pass since) */
    uint32_t law_x;         /* ... where the law's 0x94ED4 != the predicted wire value */
    uint32_t guard_x;       /* passes where the replicated guard fell back (r03f table) */
    uint32_t task_n;        /* task_10ms passes timed (OS modes 1..3) */
    uint32_t task_over;     /* ... longer than 1000 µs */
    uint16_t boot_close;    /* comparator pass the boot window closed (0xFFFF open) */
    uint16_t boot_dis;      /* disagreeing passes inside it */
    uint16_t cand_us_max;   /* candidate tick, µs (TCNTA) */
    uint16_t cand_us_last;
    uint16_t cmp_us_max;    /* comparison, µs */
    uint16_t boot_us;       /* the candidate's boot pass, µs */
    uint16_t task_us_max;   /* the whole task_10ms (OS modes 1..3), µs */
    uint16_t task_us_last;
    uint8_t  booted;        /* the candidate's boot pass ran at boot state 1 */
    uint8_t  boot_open;     /* the boot window is open */
    uint8_t  rec_n;         /* entries in rec[] */
    uint8_t  urec_n;        /* entries in urec[] */
    uint16_t prev_app[7];   /* the predicted wire value of the previous pass (law cross-check) */
    uint8_t  prev_ok;       /* prev_app valid */
    uint8_t  pad;
    uint32_t prev_law;      /* drv_out_wit()->law_n at the previous pass */
    g6a_rec_t rec[G6A_REC_N];
    g6a_rec_t urec[G6A_UREC_N];
    /* ── G6b: THE CPU WITNESSES PER STAGE (µs, TCNTA) — the candidate's tick
     * by slot group (G6A_ST_*: app/gs_engine.c GSE_STAGE), the shadow, the
     * monitor and the comparison; st_atmax = the groups of the pass that set
     * cand_us_max (names the peak's source) */
    uint16_t st_max[G6A_NST];
    uint16_t st_last[G6A_NST];
    uint16_t st_atmax[G6A_NST];
    uint16_t sh_us_max;     /* the shadow's tick (instance 1, the comparator's reference) */
    uint16_t sh_us_last;
    uint16_t mon_us_max;    /* the monitor (app/gs_bridge.c gs_bridge_tick_10ms) */
    uint16_t mon_us_last;
    uint16_t cmp_us_last;   /* the comparison */
    uint32_t cand_max_pass; /* comparator pass of cand_us_max */
    uint32_t cvs_all;       /* G6b: passes after the boot window with candidate != shadow (any field) */
    uint32_t st_t0;         /* the stage clock (TCNTA of the last stage mark) */
} g6a_t;
_Static_assert(sizeof(g6a_t) == 164u + 32u * (G6A_REC_N + G6A_UREC_N) + 76u, "g6a_t layout (tools/bench/witness_g6a.py)");

/* main init, after gs_bridge_init: the candidate's init (its RAM cleared)
 * and the comparator's */
void g6a_init(void);
/* boot state 1 (bsw/sup/sup_mode.c, after rq_prod_boot_pass): the
 * candidate's boot pass, then comparator pass 0 */
void g6a_boot(const gs_in_t *gi);
/* app/main.c: builds the inputs and calls g6a_boot (the weak hook of
 * bsw/sup/sup_mode.c) */
void g6a_boot_pass(void);
/* G6b order (app/main.c): the candidate's pass BEFORE the monitor (the seam
 * hands the F stage this pass's candidate), the comparison after it */
void g6a_cand_tick(const gs_in_t *gi);
void g6a_compare_10ms(uint8_t os_mode);
/* G6b: a stage's duration, µs (G6A_T_MON; G6A_T_SHADOW retired in G6c) */
#define G6A_T_SHADOW    0u
#define G6A_T_MON       1u
void g6a_time(unsigned what, uint32_t us);
/* the end of task_10ms: its duration (µs, TCNTA) in OS modes 1..3 */
void g6a_task_time(uint8_t os_mode, uint32_t us);
const volatile g6a_t *g6a_state(void);

#endif /* TCU_APP_G6A_H */
