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
    uint16_t pad;           /* G5b (GS_M2_XCHECK): passes whose request != the live 0x95483 (os_sel.c) */
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
    uint32_t agree_n;       /* unarmed passes, candidate A..E == lv (r04r: the P content) */
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
    uint16_t lv[5];         /* the reference A..E: r04o..r04q the live producer's 0xFFF84B78;
                             * r04r (the holder retired) the P content after the boot pass,
                             * 0 before — the agree / disagree witnesses only */
    uint16_t cur[5];        /* the would-be A..E currents */
    uint16_t app[5];        /* the applied A..E (the seam; 0 when not applied) */
    gsb_slot_t slot[2];
    gsb_m2_t m2;
    /* ── G6b: THE MONITOR (GS_G6B 1; all zero in the r04n bridge form) ── */
    uint8_t  src;           /* this pass's seam: GSM_S_LIVE (the E28E fence: the live
                             * producer), GSM_S_CAND (the candidate), GSM_S_PCONT (latched) */
    uint8_t  fence;         /* the E28E fence on after this pass (D-G6-4 / D-G6-12) */
    uint8_t  owns;          /* the candidate's engine owns the content (pf, releasing,
                             * a change pending, its engine active) */
    uint8_t  nonp;          /* the candidate's A..E content != the P content (a current,
                             * a request mode != 1, or an element filling) */
    uint8_t  shd_run;       /* consecutive owned non-P passes with candidate != shadow */
    uint8_t  trip_ch;       /* the first latching trip's channel (0xFF: not a channel) */
    uint16_t trip_why;      /* the first latching trip's GSB_R_* / GSB_G_* (G6b meanings) */
    uint32_t trip_pass;     /* bridge pass of the first latching trip (0xFFFFFFFF none) */
    uint32_t cand_n;        /* passes the seam carried the candidate */
    uint32_t live_n;        /* passes the E28E fence handed the live producer */
    uint32_t pc_n;          /* passes the monitor forced the P content */
    uint32_t shd_n;         /* owned non-P passes with candidate != shadow (any) */
    uint16_t fence_ev;      /* E28E fence entries (the boot's included) */
    uint16_t unfence_n;     /* E28E fence exits */
    uint32_t unfence_pass;  /* bridge pass of the first fence exit (the boot window's end) */
    uint16_t trip_v;        /* the first latching trip's value */
    uint16_t trip_n;        /* latching trips (each latches; the first is recorded) */
    /* ── r04q (D-G6-13 / D-G6-14, ~/tcu_sweep/r04q_port.md): THE BOOT
     * ALLOWANCE and the stall fence's P content (12 bytes: .gsb_state ends
     * at 0xFFF87300 = .gsb_rel's start) ── */
    uint8_t  boot_st;       /* GSM_B_*: 0 not yet, 1 all-release row, 2 the target
                             * element's switch-on fill, 3 used (ended on the P content);
                             * 0x80 | GSM_BV_* = violated (tripped, GSB_L_IDLE) */
    uint8_t  boot_calls;    /* boot entries (gs_bridge_boot_pass): 1 per MCU start */
    uint16_t boot_len;      /* monitor evaluations under the allowance (the entry incl.) */
    uint32_t boot_p0;       /* bridge pass at the boot entry */
    uint32_t fence_n;       /* passes the E28E stall fence held the seam (the P content) */
} gsb_t;
_Static_assert(sizeof(gsb_t) == 224u, "gsb_t is 224 bytes (tools/bench/witness_g1.py)");

/* G6b: gsb_t.src. r04q: GSM_S_LIVE is now only the pass BEFORE the
 * candidate's boot pass (the seam empty: drv_reqlaw.c returns the r03f table
 * then, D-G6-15); the E28E stall fence (D-G6-14) hands the seam THE P
 * CONTENT (GSM_S_FENCE, not latched); the boot transient is no longer
 * fenced (D-G6-13: the boot allowance) */
#define GSM_S_LIVE    0u
#define GSM_S_CAND    1u
#define GSM_S_PCONT   2u
#define GSM_S_FENCE   3u
/* r04q: gsb_t.boot_st (the once-per-boot allowance of the E28E boot state) */
#define GSM_B_NONE    0u        /* the candidate's boot pass not seen yet */
#define GSM_B_REL     1u        /* the all-release row (E28E 1..3: 0xD0C94 code 2) */
#define GSM_B_FILL    2u        /* the gear-target element's switch-on fill (E28E 0) */
#define GSM_B_USED    3u        /* ended on the P content (never again this MCU start) */
#define GSM_B_VIOL    0x80u     /* | GSM_BV_*: violated, tripped (GSB_L_IDLE), latched */
#define GSM_BV_TWICE  1u        /* a second boot entry in one MCU start */
#define GSM_BV_ROW    2u        /* content outside the allowed rows (or E28E off its states) */
#define GSM_BV_ORDER  3u        /* the all-release row after the fill began */
#define GSM_BV_LEN    4u        /* still non-P beyond GSB_BOOT_PASSES evaluations */
#define GSM_BV_ENTRY  5u        /* the boot entry's content is not the all-release row */
/* the allowance's length bound: evaluations from the boot entry (incl.) */
#ifndef GSB_BOOT_PASSES
#  define GSB_BOOT_PASSES  15u
#endif
/* G6b: the monitor's own latch bits (gsb_win_t.latch, .noinit, until power-off) */
#define GSB_L_PERMIT  0x10u     /* a permit failed while the candidate's engine owned non-P content */
#define GSB_L_SHADOW  0x20u     /* r04o only: candidate != shadow (A..E) on 2 owned non-P passes
                                 * running — G6c deleted the check with the shadow (never set) */
#define GSB_L_IDLE    0x40u     /* non-P content while the candidate's engine owned nothing */
/* G6b meanings of two r04n reason bits in a trip record (gsb_t.trip_why):
 * GSB_R_AGREE = candidate != shadow (the comparator's monitor role, plan
 * G6b item 4); GSB_R_IDLE | GSB_G_FIXED = non-P content while idle */

/* ── STAGE G4b (2026-11-19, ~/tcu_sweep/g4b_release_bridge_port.md): THE
 * BRIDGED RELEASE (ours, not stock). After a bridged engagement, the stock's
 * garage release (the shadow's BD16 = 4, G4a: phases 0x12 → 0x13 → 0x14,
 * 120 passes on the dry bench, cause 6) stays bridged instead of dropping to
 * the P content in the lever-P pass:
 *   - CONTINUITY: a release is bridged only from its first pass (the
 *     shadow's power-flow-0 commit) and only when the bridge was APPLIED in
 *     the pass before; once a release pass is not armed, that release is
 *     never re-armed (no fresh arm into a release);
 *   - direction = the last nonzero commit's slot (its pf; the shadow's
 *     commit_old must agree), applied only if GS_LIVE_DIR carries it;
 *   - during the release: range D6D6 5..8 accepted, the park-model
 *     condition waived, no agreement entry; every other arm condition and
 *     latch as an engagement (rail, OS mode, park_mon, diagnosis, the
 *     release window, the 30 s budget, the latches);
 *   - THE RELEASE ENVELOPE (compile-time; a trip latches the guard until
 *     power-off, as the element bounds): only the engagement's moving
 *     channels move (D: B, C; R: B, D), every other at its pin 8500; every
 *     moving value in [500, 8500] (mode-1 passes exactly 500 / 8500); B never
 *     above its value in the release's first pass, and that never above B's
 *     applied value in the pass before; the third element never below its
 *     first-pass value, and that never below its applied value before; at
 *     most GSB_REL_PASSES release passes (the shadow's 120 + 5);
 *   - no ramp tail: after the release's completion the shadow's currents
 *     ARE the P content (B released at 500 in mode 1 while its request
 *     object's +0x46 ramps down internally — no current transient, the
 *     m5_transient decode's switch-off), so the bridge disarms in the
 *     completion pass;
 *   - windows: release passes count against the 30 s armed budget (they
 *     are armed and applied); NOT against the 3.5 s engagement limit (the
 *     release commit ends the engagement timer; the release has its own
 *     125-pass limit) nor the 16-commit cap (a power-flow-0 commit). */
#if defined(GSB_REL_PASSES) && defined(__sh__)
#  error "gs bridge: the release limit is 125 passes on SH builds (GSB_REL_PASSES is a host-only test override)"
#endif
#ifndef GSB_REL_PASSES
#  define GSB_REL_PASSES  125u      /* the shadow's 120-pass timer + 5 */
#endif
#if defined(__sh__) && (defined(GSB_HOST_NEG_REL_FRESH) || defined(GSB_HOST_NO_REL_BRIDGE))
#  error "gs bridge: the release negatives are host-only tests"
#endif

/* the release trip codes (gsbr_t.trip_code, gsbr_slot_t.end_code 0x10 + code) */
#define GSBR_T_BUP     1u   /* B above its first-pass value */
#define GSBR_T_XDN     2u   /* the third element below its first-pass value */
#define GSBR_T_B0      3u   /* B's first-pass value above its applied value before */
#define GSBR_T_X0      4u   /* the third's first-pass value below its applied value before */
#define GSBR_T_RANGE   5u   /* outside [500, 8500], or a mode-1 value not 500 / 8500 */
#define GSBR_T_FIXED   6u   /* a pinned channel off 8500 */
#define GSBR_T_LEN     7u   /* more than GSB_REL_PASSES release passes */
/* end codes: 1 the shadow's completion, 2 disarmed by a reason (end_why),
 * 0x10 + trip code: the envelope tripped */
#define GSBR_E_DONE    1u
#define GSBR_E_WHY     2u

#define GSBR_REC_N     125u     /* per slot: release passes 0..120 + the P-content passes after */

/* one bridged-release slot (slot 0: the last release from D, 1: from R) */
typedef struct {
    uint32_t t0;            /* bridge pass of the release commit */
    uint16_t len;           /* bridged release passes */
    uint16_t end_t;         /* release pass the bridge left the release (0xFFFF: not yet) */
    uint16_t end_why;       /* GSB_R_* / GSB_G_* at the end */
    uint16_t b0, x0;        /* B / the third element in the release's first pass (0.1 mA) */
    uint16_t bprev, xprev;  /* B / the third applied in the pass before the release */
    uint8_t  pf;            /* 1 D, 2 R; 0 = never used */
    uint8_t  xch;           /* the third element's channel (4 C, 1 D) */
    uint8_t  end_code;      /* GSBR_E_* / 0x10 + GSBR_T_* */
    uint8_t  n;             /* entries recorded */
    uint8_t  bm[16];        /* bit i: entry i applied */
    uint16_t live[GSBR_REC_N][2];   /* the live staged words of B and the third (E8 / F0
                                     * rows) at the start of entry i — the previous pass's
                                     * compose (the F stage follows the 10 ms task) */
} gsbr_slot_t;
_Static_assert(sizeof(gsbr_slot_t) == 540u, "gsbr_slot_t is 540 bytes (tcu_bench.py gs-check)");

typedef struct {
    uint32_t n;             /* bridged release passes (all releases) */
    uint16_t ev;            /* bridged releases started */
    uint16_t refused;       /* shadow releases not bridged (the bridge not applied before) */
    uint16_t trip_n;        /* envelope trips (each latches) */
    uint16_t trip_v;        /* the last trip's value */
    uint8_t  trip_code;     /* GSBR_T_* */
    uint8_t  trip_ch;
    uint8_t  on;            /* a bridged release is in progress after this pass */
    uint8_t  pf;            /* its direction */
    uint16_t t;             /* its release pass (0 = the release commit) */
    uint8_t  rec;           /* the slot being recorded (0xFF none) */
    uint8_t  rec_tail;      /* passes still recorded after the end */
    uint8_t  rel_prev;      /* the shadow was releasing in the previous pass */
    uint8_t  pad;
    gsbr_slot_t slot[2];
} gsbr_t;
_Static_assert(sizeof(gsbr_t) == 24u + 2u * 540u, "gsbr_t is 1104 bytes (tcu_bench.py gs-check)");

/* the real-feedback estimate 0x93394 of B and the third, per entry, in
 * 4 mA steps (est / 40) — a separate object (.gsb_rel_est) */
typedef struct {
    uint8_t est[2][GSBR_REC_N][2];
} gsbr_est_t;
_Static_assert(sizeof(gsbr_est_t) == 500u, "gsbr_est_t is 500 bytes (tcu_bench.py gs-check)");

const volatile gsbr_t     *gs_bridge_rel(void);
const volatile gsbr_est_t *gs_bridge_rel_est(void);

void gs_bridge_init(bool cold);
void gs_bridge_tick_10ms(const gsb_in_t *in);
/* r04q (D-G6-13): the monitor's evaluation of the candidate's boot pass —
 * app/main.c g6a_boot_pass, right after the candidate's boot pass (bsw/sup/
 * sup_mode.c, boot state 1): opens the once-per-boot allowance and hands
 * the seam the candidate's boot content (the all-release row) for the F
 * stages until the next 10 ms pass; a second call trips */
void gs_bridge_boot_pass(void);
#if !defined(__sh__)
/* host only (GS_G6B builds): the monitor's windows at run time (the
 * negatives): engagement passes, release passes, armed ms, commits */
void gs_bridge_host_limits(uint16_t eng, uint16_t rel, uint32_t win_ms, uint16_t commits);
/* r04q host only: the boot allowance's length bound at run time; the
 * candidate's E28E and A's request mode (0xFF: unchanged) as the monitor
 * reads them on bridge passes [from, to) */
void gs_bridge_host_boot_limit(uint16_t passes);
void gs_bridge_host_inject_boot(uint8_t e28e, uint8_t amode, uint32_t from, uint32_t to);
#endif
#if defined(__sh__) && (GSB_BOOT_PASSES != 15u)
#  error "gs bridge: GSB_BOOT_PASSES is a host-only test override (15 on SH)"
#endif
/* true: this build is the G6b monitor (the seam carries the candidate);
 * G6c: a build constant (the r04n bridge form is deleted) */
bool gs_bridge_g6b(void);

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
