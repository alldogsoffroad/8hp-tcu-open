/* rng_chain.h — the stock's CYCLIC 10 ms range chain (task 0x9B306 /
 * 0x9B322 / 0x9B34A in the 10 ms lists of OS modes 1..3), ported from the
 * bytes (~/tcu_sweep/range_shadow_port.md, range_shadow_live_inputs.md,
 * u1267_debounce_decode.md) and, since 2026-11-17
 * (~/tcu_sweep/range_chain_live_port.md), THE LIVE OWNER OF THE RANGE:
 *
 *   0x9B228 one-shot (rng_chain_init): the init arbiter 0xA548C / 0xA557C
 *           (app/range.c's pure range_arbitrate) ONCE → E549 + D6D6;
 *   every 10 ms pass in OS modes 1..3 (rng_chain_tick_10ms): the chain
 *           writes D6D6 / D6D7 (+ D6D8 previous, D6D9 reason) and publishes
 *           0xFFF8304C (0xC79B8: long + complement, mirror 0xFFF8E64B).
 *
 * The park path (app/park.c → app/park_law.c: 0xB912C / 0xB97F8 → 0x89CDC,
 * 0xCFFDE → AE29, 0xE88FA → the release / hold demands) reads D6D6 after
 * it in the same 10 ms task, as the stock list runs 0xA2EBC after
 * 0x9B34A. safety/park_mon.c checks the chain's output independently.
 *
 * WRITER GUARD (ours): only this module writes D6D6 / D6D7 / D6D8 / D6D9,
 * E549..E54B and 0xFFF8304C. Every other reader goes through
 * app/park_cells.h, whose macros are read-only (const) outside
 * TCU_RNG_CHAIN_TU; the ctest range_chain_writer_scan refuses the stock
 * addresses in any other firmware source.
 *
 * CELLS — at the stock addresses (pinned NOLOAD sections .rng_* in
 * linker.ld; on the host the same addresses in the mapped stock window):
 *   0xFFF8D5E4..D613  rc_d5e4   D5E4 one-shot flag, D5E6..D60B derived
 *                               inputs (0xA8EFE / 0xA8F5C), D60C object
 *                               (0xA6A3E: u16 D60C, D60E, u16 D610, D612)
 *   0xFFF8D61A..D699  rc_d61a   the gathered input struct (0xA7C9C)
 *   0xFFF8D6CA..D6DF  rc_d6ca   D6CA object (u16 timer, D6CE ..), D6D4
 *                               object (D6D6 / D6D7 / D6D8 / D6D9 ..),
 *                               D6DE / D6DF
 *   0xFFF8E530..E54B  rc_e530   E530, E531, E532 object, E543..E545,
 *                               E546..E548, E549..E54B (the init arbiter)
 *   0xFFF8E572        rc_e572   previous D5F2 (0xA8F5C)
 *   0xFFF8E64B..E68F  rc_e64b   304C+0 mirror E64B; the four filter tables
 *                               E64C / E65D / E66E / E67F (0x15BB28)
 *   0xFFF8E912        rc_e912   engine speed word as 0x13BF66 leaves it
 *   0xFFF893E0..893F7 rc_893e0  the 893E0 FSM (u16 +0/+4/+0xC/+0xE/+0x10)
 *   0xFFF89418..894BB rc_89418  0x89418 object, 0x89420 object (u32 +0,
 *                               +8), the 0x89434 filter object (sub-objects
 *                               +0 table ptr / +4 enable), 894B6, 894B8
 *   0xFFF8304C..3063  rc_304c   the published range object (6 longs)
 *   0xFFF85160..85183 rc_85160  latch 85160, 8517E prev D6D6, 85182 / 83
 *   0xFFF8975C        rc_8975c  0xA8F5C first-pass flag
 * OURS (not stock cells), .rng_wit 0xFFF87000: the change recorder
 * s_rc_rec, the witnesses s_rc_wit, the fault-manager model s_rc_m.
 */
#ifndef TCU_RNG_CHAIN_H
#define TCU_RNG_CHAIN_H

#include <stdint.h>

/* 0x9B228 one-shot (the init list): the init arbiter, the filter tables
 * and table pointers; clears the chain's cells */
void rng_chain_init(void);
/* one stock 10 ms pass: 0x9B306, 0x9B322, 0x9B34A in list order, plus the
 * U1267 verdict 0xA6AB0 of the stock 20 ms list (every second pass).
 *   os_mode    the OS mode 0x93B33 (main.c passes it): the stock runs the
 *              chain only in modes 1..3 (10 ms lists 0x4D834 / 0x4DA2C /
 *              0x4DC24 carry 0x9B306..0x9B34A; the mode-0 10 ms list
 *              0x4D7A0 does not) — outside them the pass is skipped
 *              (counted) and D6D6 keeps the init arbiter's value;
 *   g98_failed / g98_tested   the park sensor safety slice's last group-98 report
 *              (the 0xA0E70(98, src, failed, tested) arguments of 0x141E06)
 *              — values; the fault model debounces them (fail 2500 ms,
 *              heal 100 ms, the reaction sticky for the power cycle) */
void rng_chain_tick_10ms(uint8_t os_mode, uint8_t g98_failed, uint8_t g98_tested);

/* ── the region types (stock layout; the target is big-endian like the
 *    stock, so a word field reads as the stock's word) ─────────────────── */
typedef union { uint8_t b[0x30]; uint16_t w16[0x18]; } rc_d5e4_t;   /* 0xFFF8D5E4 */
typedef union { uint8_t b[0x16]; uint16_t w16[0x0B]; } rc_d6ca_t;   /* 0xFFF8D6CA */
typedef union { uint8_t b[0x18]; uint16_t w16[0x0C]; } rc_893e0_t;  /* 0xFFF893E0 */
typedef union { uint8_t b[0xA4]; uint32_t w32[0x29]; } rc_89418_t;  /* 0xFFF89418 */
typedef struct { uint8_t b[0x45]; } rc_e64b_t;                      /* 0xFFF8E64B */
typedef struct { uint8_t b[0x80]; } rc_d61a_t;                      /* 0xFFF8D61A (2-aligned) */
typedef struct { uint8_t b[0x24]; } rc_85160_t;                     /* 0xFFF85160 */

#define RC_ADDR_D5E4   0xFFF8D5E4u
#define RC_ADDR_D61A   0xFFF8D61Au
#define RC_ADDR_D6CA   0xFFF8D6CAu
#define RC_ADDR_E530   0xFFF8E530u
#define RC_ADDR_E572   0xFFF8E572u
#define RC_ADDR_E64B   0xFFF8E64Bu
#define RC_ADDR_E912   0xFFF8E912u
#define RC_ADDR_893E0  0xFFF893E0u
#define RC_ADDR_89418  0xFFF89418u
#define RC_ADDR_304C   0xFFF8304Cu
#define RC_ADDR_85160  0xFFF85160u
#define RC_ADDR_8975C  0xFFF8975Cu

/* one fault-manager group as 0x9BE50 keeps it (the per-group arrays are
 * indexed g - 1) — the part the reaction bits need [O bytes 0x9BE50..
 * 0x9D01A, 0xAAC0E, 0xABDFA]:
 *   st     0xFFF8DBA6 status: b0 tested (last report), b1 failed (last
 *          report), b2 the up/down timer runs, b3 confirmed, b4 / b7 / b13
 *          set at confirmation (0x2098), b10 (0x400) set by a failed report;
 *          b9 / b11 / b12 (0x9C1C6 test-time bits, the 0x800 toggle) are not
 *          modelled
 *   d714   0xFFF8D714 the up/down timer (ms): a failed report adds the time
 *          since d89a, clamped to the fail time cfg+2; a passed report
 *          subtracts it (after clamping to the heal time cfg+4)
 *   d89a   0xFFF8D89A the time base at the group's last timer step (low 16)
 *   dd2c   0xFFF8DD2C the timer's maximum (dump: 500 / 2500 = the fail time)
 *   rec    the stored record's +4 word (0xFFF81D62 + 0x38 i): b3 active,
 *          b4 its reaction words are in the masks; ours: 0x8000 = it exists */
typedef struct {
    uint16_t st, d714, d89a, dd2c, rec;
    uint8_t  react;                 /* the group's words are in 0xFFF897D0 / D8 */
    uint8_t  pad;
    uint32_t rep_n;                 /* reports processed (tested or not) */
    uint32_t start_n;               /* timer starts from 0: c0b2(g, 1) fail, c0b2(g, 0) heal */
    uint32_t confirm_n;             /* confirmations (the reaction words OR-ed into the masks) */
    uint32_t heal_n;                /* a passed report took the timer to 0 (0x9C608: a heal
                                     * when confirmed, else the debounce just ends) */
    uint32_t untest_reset_n;        /* 0x9C342: an untested report stopped a running timer */
    uint32_t confirm_ms, heal_ms;   /* fm_ms at the last confirmation / heal */
} rng_chain_fm_t;

/* witnesses: reaches of what is NOT ported (each one a reason to distrust
 * the shadow's answer if nonzero), and the change recorder */
typedef struct {
    uint32_t passes;            /* 10 ms passes run */
    uint32_t c5f14_n;           /* 0xC829E: cal > 0 → 0xC5F14(0x89440) not ported (stand-in 0x89444 = 0) */
    uint32_t mask_n[7];         /* 0xC8650 enables: coding-mask virtual call reached (dump enable used) */
    uint32_t guard_c290e_n;     /* 0xC290E refused a D6DE write (event 0x1F reported) */
    uint32_t guard_c792e_n;     /* 0xC792E refused a D6D6 write (event 7 reported) */
    uint32_t report_c78be_n;    /* 0xC78BE's reporter 0x4B888 call (not ported) */
    uint32_t ovr_n;             /* a D61B == 1 cal override reached (D61B is 0: never) */
    uint32_t s8942c_n;          /* 0xC3AE6 state 1 / 2 reached (cal 0x191FDC = 0: never) */
    uint32_t flt_ptr_bad_n;     /* ours: a 0x89434 sub-object table pointer outside the four
                                 * 0x15BB28 tables (never written so; 0xFFF8E64C used) */
    uint8_t  rec_head;          /* next recorder slot */
    uint8_t  rec_n;             /* entries written (saturates 255) */
} rng_chain_wit_t;

/* recorder entry: one per pass in which any watched cell changed */
typedef struct {
    uint32_t now_ms;
    uint8_t  d672, d673, d5fb, e531;
    uint8_t  e53c, x8941a, x894b6, d6ce;
    uint8_t  e545, d6de, d6df, d6d6;
    uint8_t  d6d9, x893e8;
    uint8_t  d607;              /* the brake interlock (0xA90A0) */
    uint8_t  flags;             /* bit 0 D660, 1 D661, 2 D664, 3 D665, 4 D666,
                                 * 5 D631 == 0 (brake valid), 6 D64B (brake on),
                                 * 7 D63C == 0 (engine speed valid) */
} rng_chain_rec_t;
#define RNG_CHAIN_REC_N 16u

/* the fault-manager model and the U1267 report state (ours: the stock's
 * per-group arrays 0xFFF8D714 / D89A / DD2C / DBA6 and records are NOT at
 * their stock addresses — only the two groups the gather reads are
 * modelled) */
typedef struct {
    uint8_t  u_failed, u_tested;    /* the last 0xA6AB0 report (r6 / r7) */
    uint8_t  g109, g98;             /* in the masks 0xFFF897D0 / D8: U1267, P0850 */
    uint8_t  ph20;                  /* 20 ms phase: the verdict runs when 0 */
    uint8_t  pad[3];
    uint32_t g109_ms, g98_ms;       /* sched_now_ms() at the last confirmation (0 = none) */
    uint32_t u_rep_n, u_fail_n, u_pass_n;   /* 0xA6AB0 reports: all / failed+tested / tested only */
    uint32_t skip_mode_n;           /* passes skipped: OS mode outside 1..3 */
    uint32_t fm_ms;                 /* our 0xFFF89898 (ms): +10 per call */
    rng_chain_fm_t fm109, fm98;     /* group 109 (U1267), group 98 (P0850 / P07B2 / P07B8) */
    /* the DSM operating cycle (2026-11-17, ~/tcu_sweep/keyoff_autop_port.md):
     * 0xAE890's state (1 power-up, 2 terminal on, 3 terminal off) and the
     * terminal re-cycles that ran the mode-1 rebuild 0xAACE4 */
    uint8_t  dsm_cyc;
    uint8_t  pad2[3];
    uint32_t recycle_n, recycle_ms;
} rng_chain_model_t;

const rng_chain_model_t *rng_chain_model(void);
const rng_chain_wit_t   *rng_chain_wit(void);
const rng_chain_rec_t   *rng_chain_rec(void);   /* [RNG_CHAIN_REC_N] ring */

#if defined(RNG_CHAIN_TEST_HOOKS)
/* host tests only (#error on SH): override gathered byte D61A+off */
void rng_chain_test_override(uint8_t off, uint8_t val);
void rng_chain_test_clear(void);
#endif

#endif /* TCU_RNG_CHAIN_H */
