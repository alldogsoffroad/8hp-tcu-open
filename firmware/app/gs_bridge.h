/* gs_bridge.h — THE GARAGE BRIDGE, STAGE G1 (ours, not stock; 2026-11-18,
 * ~/tcu_sweep/garage_live_plan.md §2 / §5 G1, ~/tcu_sweep/garage_g1_port.md).
 *
 * Every 10 ms pass (app/main.c, right after the garage shadow) the bridge
 * decides whether the shadow's A–E currents WOULD replace the live
 * producer's (plan §2.4 arm conditions, §2.2 compile-time element bounds,
 * §2.3 .noinit windows), and records the would-be bus-A words next to the
 * live staged words and the real-feedback estimate 0x93394. In G1 it is a
 * WITNESS: GS_LIVE_DIR = 0 (bsw/drv/drv_gsbr.h), the seam does not exist in
 * the output path, and nothing here writes anything but this module's own
 * RAM (linker.ld .gsb_win / .gsb_state / .gsb_rec0 / .gsb_rec1).
 *
 * Also here (read-only): a witness of the stock's OS-mode selector 0x1418E2
 * (object 0xFFF9547C, request +7 = 0xFFF95483) on our inputs — what the
 * stock WOULD request; never written to 0x93B5E, never applied.
 *
 * READ-ONLY, ENFORCED (CMakeLists "garage bridge"): an output path may not
 * include this header (#error); the garage shadow may not either; ctest
 * gs_bridge_scan: no output path names the bridge but drv_reqlaw.c's one
 * seam call inside `#if GS_LIVE_DIR != 0`; the bridge TU reads live cells
 * only through const volatile and calls no live writer.
 */
#ifndef TCU_APP_GS_BRIDGE_H
#define TCU_APP_GS_BRIDGE_H

#if defined(TCU_OUTPUT_PATH)
#error "the garage bridge is read-only in G1: an output path may not include gs_bridge.h (the one seam is bsw/drv/drv_gsbr.h)"
#endif
#if defined(TCU_GS_SHADOW_TU)
#error "the garage shadow may not include the garage bridge (gs_bridge.h)"
#endif

#include <stdint.h>
#include <stdbool.h>

/* the live values the bridge gets BY VALUE (app/main.c fills them); the
 * stock cells it reads itself through const volatile (gs_bridge.c) */
typedef struct {
    uint8_t  mon_faults;    /* park_mon_faults() (0 = not latched) */
    uint8_t  relwin_st;     /* drv_rq_rel_win_state() (2 = latched off) */
    uint8_t  eng_valid;     /* CAN 0x108 valid */
    uint8_t  tur_ok;        /* turbine sensor valid (→ E7EC = 0) */
    uint16_t eng_speed;     /* CAN 0x108 engine speed (rpm) */
    uint16_t tur_rpm;       /* turbine speed (rpm, 0 when invalid / negative) */
    const uint16_t *cyc;    /* drv_job_tx(DRV_CYC): the staged cyclic (45 words), or NULL */
} gsb_in_t;

/* ── the reasons (one u16 per pass; arm conditions first, then the guard) */
#define GSB_R_IDLE    0x0001u   /* the shadow neither engaging nor engaged */
#define GSB_R_DIR     0x0002u   /* the shadow's direction is not in the evaluated set */
#define GSB_R_RAIL    0x0004u   /* seq2[0] != 'U' or letter != 'c' */
#define GSB_R_OSMODE  0x0008u   /* OS mode 0 */
#define GSB_R_RANGE   0x0010u   /* live D6D6 != the shadow's direction (5 D / 7 R) */
#define GSB_R_PARK    0x0020u   /* live park model 0xFFF8BC70 != 1 (released) */
#define GSB_R_MON     0x0040u   /* park_mon latched */
#define GSB_R_DIAG    0x0080u   /* 0x93004 / 0x93A42 nonzero or 0x93A38 >= 2 on A..E */
#define GSB_R_RELWIN  0x0100u   /* the park-release window latched off */
#define GSB_R_WIN     0x0200u   /* the 30 s armed-time budget spent (latched) */
#define GSB_R_ENGT    0x0400u   /* an engagement exceeded 3.5 s (latched) */
#define GSB_R_COMMITS 0x0800u   /* more than 16 nonzero commits (latched) */
#define GSB_R_AGREE   0x1000u   /* entry: fewer than 10 agreeing unarmed passes */
#define GSB_G_RANGE   0x2000u   /* guard: a moving value outside [500, 8500], or a
                                 * mode-1 value other than 500 / 8500 */
#define GSB_G_FIXED   0x4000u   /* guard: a fixed channel off its pinned value */
#define GSB_G_LATCH   0x8000u   /* the guard is latched (until power-off) */

/* the window latch bits (.noinit) */
#define GSB_L_WIN     0x01u
#define GSB_L_ENGT    0x02u
#define GSB_L_COMMITS 0x04u
#define GSB_L_GUARD   0x08u

#define GSB_WIN_MS        30000u    /* armed time per power cycle (D-G4) */
#if defined(GSB_ENG_PASSES) && defined(__sh__)
#  error "gs bridge: the engagement limit is the plan's 3.5 s on SH builds (GSB_ENG_PASSES is a host-only test override)"
#endif
#ifndef GSB_ENG_PASSES
#  define GSB_ENG_PASSES  350u      /* 3.5 s commit → completion */
#endif
#define GSB_COMMITS_MAX   16u       /* nonzero commits per power cycle */
#define GSB_AGREE_N       10u       /* agreeing unarmed passes before an arm */
#define GSB_WIN_MAGIC     0x47534231u   /* "GSB1" */

/* the .noinit windows (survive an MCU reset; fresh at power-on) — 20 bytes */
typedef struct {
    uint32_t magic;
    uint32_t acc_ms;        /* would-armed time this power cycle */
    uint32_t acc_inv;
    uint16_t commits;       /* nonzero shadow commits this power cycle */
    uint16_t commits_inv;
    uint8_t  latch;         /* GSB_L_* */
    uint8_t  latch_inv;
    uint8_t  warm;          /* warm boots seen this power cycle (witness) */
    uint8_t  broken;        /* 1: a warm boot found the guard broken (budget spent) */
} gsb_win_t;
_Static_assert(sizeof(gsb_win_t) == 20u, "gsb_win_t is 20 bytes (tools/bench/witness_g1.py)");

/* one recorder entry = one bridge pass from a nonzero commit (36 bytes);
 * the entry index is the pass since the commit (0 = the commit pass) */
typedef struct {
    uint8_t  ph[3];         /* the shadow's phases D0DC (first), D148 (second), D1B4 (third) */
    uint8_t  st;            /* bit0 would-arm, 1 applied (seam), 2 guard latched,
                             * 3 shadow A..E == live A..E, 4-5 shadow pf,
                             * 6 commit this pass, 7 completion this pass */
    uint16_t why;           /* GSB_R_* / GSB_G_* this pass */
    uint16_t wb[5];         /* would-be staged words A D B E C (E8 / F0 rows) */
    uint16_t live[5];       /* the live staged words (drv_job_tx(DRV_CYC)) */
    uint16_t est[5];        /* the real-feedback estimate 0x93394[0..4] (0.1 mA) */
} gsb_rec_t;
_Static_assert(sizeof(gsb_rec_t) == 36u, "gsb_rec_t is 36 bytes (tools/bench/witness_g1.py)");

#define GSB_REC_N     112u      /* per slot (slot 0: the last D, slot 1: the last R) */
#define GSB_NONE      0xFFFFu

/* one slot header (20 bytes): passes are counted from the commit */
typedef struct {
    uint32_t t0;            /* bridge pass of the commit */
    uint16_t arm_on;        /* first would-armed pass (GSB_NONE) */
    uint16_t arm_off;       /* first disarm after an arm (GSB_NONE) */
    uint16_t off_why;       /* the reasons at that disarm */
    uint16_t done_at;       /* the shadow's completion (GSB_NONE) */
    uint16_t back_at;       /* shadow A..E == live A..E again after the disarm (GSB_NONE) */
    uint16_t armed_n;       /* would-armed passes of this engagement */
    uint8_t  n;             /* entries written */
    uint8_t  pf;            /* 1 D, 2 R; 0 = never used */
    uint8_t  done_cause;    /* the shadow's completion cause at done_at */
    uint8_t  guard_n;       /* guard trips in this engagement */
} gsb_slot_t;
_Static_assert(sizeof(gsb_slot_t) == 20u, "gsb_slot_t is 20 bytes (tools/bench/witness_g1.py)");

/* the read-only witness of 0x1418E2 (object 0xFFF9547C) — 32 bytes */
typedef struct {
    uint8_t  req;           /* +7 (0xFFF95483): what the stock would request */
    uint8_t  app;           /* +8 (0xFFF95484) modelled: the last request, as if applied [I] */
    uint8_t  live_os;       /* our applied OS mode 0x93B33 (never changed by this) */
    uint8_t  e9ff;          /* the terminal-15 state 0xFFF8E9FF (kl15) */
    uint8_t  f584a;         /* 0xFFF9584A stand-in 1 (no reaction 0x38, no overvoltage start) */
    uint8_t  eng_st;        /* +6: 0 valid / 2 invalid */
    uint8_t  tur_st;        /* E7EC: 0 sensor ok */
    uint8_t  spd_ok;        /* this pass's speed term */
    uint16_t eng;           /* +4: E90E (held while invalid) */
    uint16_t tur;           /* E7E8 */
    uint16_t sup;           /* 0x96066 ← 0x93920 ← 0x94F86 (mV) */
    uint16_t pad;
    uint32_t n;             /* passes (OS modes 1..3) */
    uint32_t n2;            /* passes requesting 2 */
    uint32_t first2;        /* bridge pass of the first request 2 (0 = never) */
    uint32_t chg;           /* request changes */
} gsb_m2_t;
_Static_assert(sizeof(gsb_m2_t) == 32u, "gsb_m2_t is 32 bytes (tools/bench/witness_g1.py)");

/* the bridge's state and witnesses (one struct, .gsb_state) */
typedef struct {
    uint32_t passes;        /* bridge passes */
    uint32_t armed_n;       /* would-armed passes */
    uint32_t applied_n;     /* applied passes (0 in G1) */
    uint32_t take_n;        /* seam reads that returned a value (0 in G1) */
    uint32_t agree_n;       /* unarmed passes, shadow A..E == live A..E */
    uint32_t disagree_n;    /* unarmed passes that disagree */
    uint32_t last_dis;      /* the last disagreeing unarmed pass */
    uint32_t sh_commits;    /* the shadow's commit count last seen */
    uint32_t sh_done;       /* the shadow's done_pass last seen */
    uint16_t arm_ev, disarm_ev;
    uint16_t guard_n;       /* guard trips (each latches) */
    uint16_t guard_why;     /* the last trip's GSB_G_* */
    uint16_t guard_v;       /* the last trip's value */
    uint16_t why;           /* this pass's reasons */
    uint16_t eng_t;         /* passes since the nonzero commit (engagement timer) */
    uint16_t why_armed_or;  /* OR of the reasons at every disarm */
    uint8_t  guard_ch;      /* the last trip's channel */
    uint8_t  armed;         /* this pass would-armed */
    uint8_t  applied;       /* this pass applied (seam) */
    uint8_t  agree_run;     /* consecutive agreeing unarmed passes (saturating) */
    uint8_t  pf;            /* the shadow's pf this pass */
    uint8_t  eng_on;        /* an engagement is being timed */
    uint8_t  slot_cur;      /* the slot of the last nonzero commit (0xFF none) */
    uint8_t  back_pend;     /* waiting for agreement after a disarm */
    uint16_t sh[5];         /* the shadow's A..E currents this pass */
    uint16_t lv[5];         /* the live producer's A..E (0xFFF84B78, complement-checked) */
    uint16_t cur[5];        /* the would-be A..E currents */
    uint16_t app[5];        /* the applied A..E (the seam; 0 when not applied) */
    gsb_slot_t slot[2];
    gsb_m2_t m2;
} gsb_t;
_Static_assert(sizeof(gsb_t) == 172u, "gsb_t is 172 bytes (tools/bench/witness_g1.py)");

void gs_bridge_init(bool cold);
void gs_bridge_tick_10ms(const gsb_in_t *in);

const volatile gsb_t     *gs_bridge_state(void);
const volatile gsb_win_t *gs_bridge_win(void);
const volatile gsb_rec_t *gs_bridge_rec(uint8_t slot, uint8_t i);

/* the compile-time element bounds of direction pf (1 D, 2 R): the moving
 * channels' mask (bit ch, ch 0..4 = A D B E C) and every channel's pinned
 * value (0 for a moving channel); false for any other pf */
bool gs_bridge_table(uint8_t pf, uint8_t *move_mask, uint16_t fixed[5]);

#if !defined(__sh__)
/* host only: replace the shadow's current of channel ch, as the bridge reads
 * it, on bridge passes [from, to) (the guard negatives) */
void gs_bridge_host_inject(uint8_t ch, uint16_t v, uint32_t from, uint32_t to);
#else
#  if defined(GSB_HOST_MUT_TABLE)
#    error "gs bridge: the table mutation is a host-only negative test"
#  endif
#endif

#endif /* TCU_APP_GS_BRIDGE_H */
