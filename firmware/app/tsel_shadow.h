/* tsel_shadow.h — a READ-ONLY shadow of the stock's ECU-side temperatures:
 * the supervisor die temperature (J3), the board sensor (J4) and the
 * SELECTED ECU temperature 0x9393A / status 0x940FC that the output
 * diagnosis's current bounds, the feedback estimate's a-curve / R_drv model,
 * drv_temp's fluid substitute and the safety layer's fallback read
 * (app/tsel_shadow.c; ~/tcu_sweep/diag_temp_inputs_decode.md, the port
 * ~/tcu_sweep/diag_temp_shadow_port.md).
 *
 * What the stock does [O bytes]:
 *   init hub 0x832AC (row-9 list 0x44CA0, stub 0x7CB5C), once:
 *     0x88BAC → 0x8DB28 (0x93ABA = 0x0780, start byte 0x94823 = 2, hold
 *       0x9477A = 8), 0x7C086(0x62) (a no-op: 0x62 & 0xFF9D == 0),
 *       0x9393A = 0x0780;
 *     0x88D98 → 0x8DD40 (0x9393E = 0x0780, 0x94730 = 5, hold 0x9477C = 120,
 *       flags 0x93AC0 = 0x80), 0x7C0E8(0x160) (a no-op: 0x160 & 0xFE9F == 0);
 *   hub 0x83300 (stub 0x7CB6E: 5 ms in mode 0, 10 ms in modes 1..3), after
 *   0x89320 and in this order:
 *     0x88DA8 → 0x8DD7C the board sensor 0x9393E / 0x94100 from the J4 word
 *       0x908CC (1/128 °C; 0xFF03 = not ready, the low 3 bits must read 1,
 *       a 120-step (600 ms) hold), byte 0x94101, report 0x7C0B6 (tested
 *       0x940E4 / failed 0x940E5);
 *     0x887D8 the fluid (drv_temp, live — not part of this shadow);
 *     0x88BD0 → 0x8DB5A the die temperature 0x93ABA / 0x9415E from the J3
 *       average 0x908CA (T = −738 + 3.347·N, 1/16 °C; the comparator flags
 *       0x9095E; an 8-step (40 ms) hold), then THE SELECTION (cal 0x191A02
 *       = 7): fluid (status ≥ 3, capped 6) < die (capped 6) < board (not
 *       capped) → 0x9393A / 0x940FC / source 0x94170, bytes 0x940FF
 *       (selected) and 0x940FE (die), report 0x7C01E (fault word 0x93916,
 *       tested 0x940E1 / failed 0x940E2).
 *   The 0x9477A / 0x9477C words and the 0x94823 byte lie in the countdown
 *   service 0x7C7E4's ranges (5 ms, every mode); 0x94730 does not.
 *
 * STAGE 2a (2026-11-18): under TSEL_LIVE (bsw/drv/drv_tsel.h — the SH
 * application) the hook hands the results by value to bsw/drv/drv_tsel.c,
 * which writes 0x9393A / 0x940FC / 0x94170 / 0x940FF / 0x940FE / 0x94101 and
 * the die / board cells; the fault flags stay stand-ins (stage 2b).
 *
 * The SHADOW runs all of that into ITS OWN struct (tsel_t, linker section
 * .tsel_shadow at 0xFFF87C40, ours): it writes NOT 0x9393A, NOT 0x940FC,
 * NOT 0x940FF / 0x940FE / 0x94101, NOT the fault flags 0x940E1/E2/E4/E5,
 * NOT the live countdown words (it keeps and steps its own copies of
 * 0x9477A / 0x9477C / 0x94823, as pa7_shadow does). Its inputs arrive BY
 * VALUE (tsel_in_t, filled by app/tsel_hook.c).
 *
 * NOT PORTED / STOOD IN (stated):
 *   - 0x7CA24 (the DTC reports 2442 / 2443 into the supplier ring
 *     0xFFF948B8): counted only (rep_die_n / rep_brd_n);
 *   - 0x6F594 "fault memory ready": an input (the hook passes 1, our
 *     stand-in S1 everywhere else — sup_ladder.c, drv_temp.c);
 *   - 0x72AE0 (profiling hook): nothing;
 *   - the variant 0x90966 indexes two-entry tables; the stock's writer
 *     (0x91ACA) produces 0 / 1 only, the shadow clamps any other value to 1;
 *   - (the stock's reply-proc gate 0x9095B on 0x908CA's average is ported
 *     since stage 2a — sup_ctrl.c; the shadow takes the live average.)
 *
 * READ-ONLY, ENFORCED: every output TU defines TCU_OUTPUT_PATH — this header
 * #errors under it; app/tsel_shadow.c defines TCU_TSEL_SHADOW_TU and the
 * live headers (sup_cells.h, drv_cells.h, drv_temp.h, drv_odiag.h,
 * platform.h) #error under it; tsel_shadow_scan.py (outputs / shadow / hook,
 * with negative fixtures); sh-elf-gcc pass/fail checks; the live-identity
 * test (fb_identity.py over test_odiag with the hook at its slot).
 */
#ifndef TCU_APP_TSEL_SHADOW_H
#define TCU_APP_TSEL_SHADOW_H

#if defined(TCU_OUTPUT_PATH)
#error "the temperature-selection shadow is read-only: an output path may not include tsel_shadow.h"
#endif

#include <stdint.h>
#include <stddef.h>

/* ── the calibration (variant offset *0xFFF90AB8 = 0; pointer pools 0x66AD8..
 *    0x66B6C [O bytes]); byte-checked against the stock image by the host
 *    test (test_tsel_shadow --cal) ─────────────────────────────────────── */
/* die 0x8DB28 / 0x8DB5A */
#define TSEL_CAL_TINIT      0x0780      /* 0x191706 (pool 0x66AEC): 120 °C, also the selection default */
#define TSEL_CAL_DSTART     0x02u       /* 0x1919FF (pool 0x66AF4): start byte 0x94823 */
#define TSEL_CAL_DHOLD      0x0008u     /* 0x191708 (pool 0x66AF0): hold 0x9477A (40 ms) */
#define TSEL_CAL_NHI        0x03FEu     /* 0x19170A (pool 0x66AF8): N > 1022 → flag 0x04 */
#define TSEL_CAL_NLO        0x0005u     /* 0x19170C (pool 0x66AFC): N < 5 → flag 0x02 */
#define TSEL_CAL_DIV        10000.0f    /* 0x8DD24 (literal 0x461C4000) */
/* per variant (0x90966 = 0 / 1): 0x191394 offsets (pool 0x66AD8), 0x191390
 * scales (direct literal 0x8DD20), comparator bands 0x1913A0 / 0x1913A4
 * (pools 0x66AE4 / 0x66AE8), 0x191398 / 0x19139C (pools 0x66ADC / 0x66AE0) */
#define TSEL_CAL_OFF0       (-738)
#define TSEL_CAL_OFF1       (-734)
#define TSEL_CAL_SCALE0     33470u
#define TSEL_CAL_SCALE1     35785u
#define TSEL_CAL_C1LO0      2048        /* 128 °C */
#define TSEL_CAL_C1LO1      2128
#define TSEL_CAL_C1HI0      2176        /* 136 °C */
#define TSEL_CAL_C1HI1      2256
#define TSEL_CAL_C2LO0      2304        /* 144 °C */
#define TSEL_CAL_C2LO1      2288
#define TSEL_CAL_C2HI0      2432        /* 152 °C */
#define TSEL_CAL_C2HI1      2416
/* board 0x8DD40 / 0x8DD7C */
#define TSEL_CAL_BINIT      0x0780      /* 0x191710 (pool 0x66B04) */
#define TSEL_CAL_BNEVER     0x0005u     /* 0x19171C (pool 0x66B1C): 0x94730 init */
#define TSEL_CAL_BHOLD      0x0078u     /* 0x19171A (pool 0x66B18): hold 0x9477C = 120 (600 ms) */
#define TSEL_CAL_BNEAR_LO   (-640)      /* 0x191712 (pool 0x66B08): flag 0x80 window */
#define TSEL_CAL_BNEAR_HI   640         /* 0x191714 (pool 0x66B0C) */
#define TSEL_CAL_BNOTRDY    0xFF03u     /* 0x19171E (pool 0x66B20): the device's not-ready word */
#define TSEL_CAL_BONES      0x0007u     /* 0x19170E (pool 0x66B00): the low "1" bits */
#define TSEL_CAL_BMAX       21504       /* 0x191716 (pool 0x66B10): 168 °C → 0x40 */
#define TSEL_CAL_BMIN       (-5760)     /* 0x191718 (pool 0x66B14): −45 °C → 0x20 */
/* selection 0x88BD0 */
#define TSEL_CAL_SEL        0x07u       /* 0x191A02 (pool 0x66B64): fluid | die | board */
#define TSEL_CAL_FOFF       0           /* 0x19173A (pool 0x66B68): fluid offset */
#define TSEL_CAL_BOFF       0           /* 0x19173C (pool 0x66B6C): board offset */

/* the live inputs, BY VALUE (app/tsel_hook.c, at the hub slot after drv_temp_tick) */
typedef struct {
    uint32_t t_us;      /* +0  TCNTA (witness) */
    uint16_t n908ca;    /* +4  0x908CA the J3 die average (10 bit; 0xFFFF invalid) */
    int16_t  r908cc;    /* +6  0x908CC the J4 board word (1/128 °C) */
    int16_t  t94f98;    /* +8  0x94F98 the fluid, raw or 0x887D8's substitute (1/16 °C) */
    uint8_t  st94fee;   /* +10 0x94FEE its status */
    uint8_t  f9095e;    /* +11 0x9095E the supervisor's over-temperature comparator
                         *     flags (our CELL_DIR_BITS — a misnomer) */
    uint8_t  v90966;    /* +12 0x90966 the variant (bench 0) */
    uint8_t  n5;        /* +13 5 ms countdown steps since the last pass (mode 0: 1, else 2) */
    uint8_t  fm_ready;  /* +14 0x6F594 (stand-in 1) */
    uint8_t  os_mode;   /* +15 0x93B33 (witness) */
} tsel_in_t;
_Static_assert(sizeof(tsel_in_t) == 16u, "tsel_in_t is 16 bytes (witness_temps.py)");

/* one change of (0x940FC, 0x94170, 0x9415E, 0x94100) */
typedef struct {
    uint32_t t_us;      /* +0 */
    uint16_t pass;      /* +4 hub pass (low 16) */
    int16_t  t_sel;     /* +6 0x9393A */
    int16_t  r908cc;    /* +8 the board word of that pass */
    uint8_t  st_sel;    /* +10 0x940FC */
    uint8_t  src;       /* +11 0x94170 */
    uint8_t  st_die;    /* +12 0x9415E */
    uint8_t  st_brd;    /* +13 0x94100 */
    uint16_t n908ca;    /* +14 the die average of that pass */
} tsel_tr_t;
_Static_assert(sizeof(tsel_tr_t) == 16u, "tsel_tr_t is 16 bytes (witness_temps.py)");

#define TSEL_TR_N 12u

typedef struct {
    /* +0x00 the die part (0x8DB5A), at the stock's cell names */
    uint16_t ab2;           /* 0x93AB2 N of this pass */
    uint16_t ab4;           /* 0x93AB4 N of the last pass */
    uint16_t ab6;           /* 0x93AB6 flags */
    uint16_t ab8;           /* 0x93AB8 last pass's flags */
    int16_t  aba;           /* 0x93ABA THE DIE TEMPERATURE (1/16 °C) */
    int16_t  t_conv;        /* +0x0A this pass's conversion (ours: witness, also when invalid) */
    uint16_t w477a;         /* +0x0C 0x9477A hold (the shadow's copy) */
    uint8_t  b823;          /* +0x0E 0x94823 start byte (the shadow's copy) */
    uint8_t  s15c;          /* +0x0F 0x9415C the 0x9095E copy */
    uint8_t  s15d;          /* +0x10 0x9415D the 0x90966 copy */
    uint8_t  s15e;          /* +0x11 0x9415E die status */
    /* +0x12 the board part (0x8DD7C) */
    int16_t  abc;           /* 0x93ABC the word of this pass */
    int16_t  abe;           /* 0x93ABE the word of the last pass */
    uint16_t ac0;           /* +0x16 0x93AC0 flags */
    uint16_t ac2;           /* +0x18 0x93AC2 last pass's flags */
    int16_t  s93e;          /* +0x1A 0x9393E THE BOARD TEMPERATURE (1/16 °C) */
    uint16_t w730;          /* +0x1C 0x94730 "never valid" (not a countdown) */
    uint16_t w77c;          /* +0x1E 0x9477C hold (the shadow's copy) */
    uint8_t  s100;          /* +0x20 0x94100 board status */
    uint8_t  s101;          /* +0x21 0x94101 board byte */
    /* +0x22 the selection (0x88BD0) */
    int16_t  s93a;          /* 0x9393A THE SELECTED ECU TEMPERATURE (SHADOW) */
    int16_t  s93c;          /* +0x24 0x9393C the die copy */
    uint8_t  s0fc;          /* +0x26 0x940FC ITS STATUS (SHADOW) */
    uint8_t  s0fd;          /* +0x27 0x940FD the die status copy */
    uint8_t  s0fe;          /* +0x28 0x940FE the die byte */
    uint8_t  s0ff;          /* +0x29 0x940FF the selected byte */
    uint8_t  s170;          /* +0x2A 0x94170 the source (1 fluid, 2 die, 4 board, 0 default) */
    uint8_t  tr_head;       /* +0x2B next transition slot */
    /* +0x2C the reports */
    uint16_t af0;           /* 0x93AF0 die code */
    uint16_t af2;           /* 0x93AF2 die mask (0x60 / 0x62) */
    uint16_t af4;           /* 0x93AF4 board code */
    uint16_t af6;           /* 0x93AF6 board mask (0x160) */
    uint16_t w916;          /* +0x34 0x93916 the die fault word (0x7C01E) */
    uint8_t  e1;            /* +0x36 0x940E1 die tested */
    uint8_t  e2;            /* +0x37 0x940E2 die failed */
    uint8_t  e4;            /* +0x38 0x940E4 board tested */
    uint8_t  e5;            /* +0x39 0x940E5 board failed */
    uint16_t pad;           /* +0x3A */
    /* +0x3C our witnesses */
    uint32_t init_n;        /* init passes (0x832AC's two calls) */
    uint32_t pass_n;        /* hub passes */
    uint32_t die_ok_n;      /* passes with 0x9415E == 8 */
    uint32_t brd_ok_n;      /* passes with 0x94100 == 8 */
    uint32_t sel8_n;        /* passes with 0x940FC == 8 */
    uint32_t ff03_n;        /* passes with the board word == 0xFF03 (not ready) */
    uint32_t r8001_n;       /* passes with the board word == 0x8001 (our re-arm read twice) */
    uint32_t lowbit_n;      /* passes with board flag 0x10 (low "1" bits missing) */
    uint32_t tr_n;          /* status / source changes */
    uint32_t rep_die_n;     /* 0x7C01E calls (0x7CA24 report not ported) */
    uint32_t rep_brd_n;     /* 0x7C0B6 calls */
    uint32_t initrep_n;     /* 0x7C086 / 0x7C0E8 report paths taken (must stay 0) */
    uint32_t first_brd8_us; /* +0x6C TCNTA at the first board status 8 */
    uint32_t first_sel8_us; /* +0x70 TCNTA at the first 0x940FC == 8 */
    int16_t  sel_min;       /* +0x74 min / max of 0x9393A over passes with 0x940FC >= 6 */
    int16_t  sel_max;       /* +0x76 */
    tsel_in_t in;           /* +0x78 the last inputs */
    tsel_tr_t tr[TSEL_TR_N];/* +0x88 the last TSEL_TR_N changes (a ring) */
} tsel_t;
_Static_assert(offsetof(tsel_t, abc) == 0x12u, "tsel_t layout (witness_temps.py)");
_Static_assert(offsetof(tsel_t, s93a) == 0x22u, "tsel_t layout (witness_temps.py)");
_Static_assert(offsetof(tsel_t, af0) == 0x2Cu, "tsel_t layout (witness_temps.py)");
_Static_assert(offsetof(tsel_t, init_n) == 0x3Cu, "tsel_t layout (witness_temps.py)");
_Static_assert(offsetof(tsel_t, first_brd8_us) == 0x6Cu, "tsel_t layout (witness_temps.py)");
_Static_assert(offsetof(tsel_t, in) == 0x78u, "tsel_t layout (witness_temps.py)");
_Static_assert(offsetof(tsel_t, tr) == 0x88u, "tsel_t layout (witness_temps.py)");
_Static_assert(sizeof(tsel_t) == 0x148u, "tsel_t is 0x148 bytes (.tsel_shadow, witness_temps.py)");

void tsel_shadow_init(void);                    /* 0x832AC's 0x88BAC + 0x88D98, into the shadow */
void tsel_shadow_pass(const tsel_in_t *in);     /* 0x83300's 0x88DA8 + 0x88BD0, into the shadow */
const volatile tsel_t *tsel_shadow_state(void);

/* app/tsel_hook.c — the gather (it reads the live cells by value) */
void tsel_hook_init(void);              /* main.c, in the shadow init block */

/* app/tsel_live.c — stage 2a: the shadow's results of this pass handed by
 * value to bsw/drv/drv_tsel.c (the hook calls it under TSEL_LIVE) */
void tsel_live_apply(void);

#endif /* TCU_APP_TSEL_SHADOW_H */
