/* sm_shadow.h — a READ-ONLY shadow of the stock's SHIFT TARGET: the fault-free
 * target gear 0xFFF8A697 and its copy 0xFFF8A0AA (→ 0x89CE8) (app/sm_shadow.c;
 * decode ~/tcu_sweep/shift_map_decode.md, the port ~/tcu_sweep/sm_shadow_port.md).
 *
 * What the stock does [O bytes], the 30 ms list 0x4D940 (modes 1..3):
 *   entry 5   0xF9570: AB30 = 0x121BCC(D646, AB30, w 0x1FEC8C, w 0x1FEC8D) =
 *             (D646·1 + AB30·1) / 2 (unsigned); 0xFC112: the minimum gear by
 *             speed A67C+0x17 (first g ≥ 1 with D646 ≤ curve 0x1E5E30[g], ≤ 8),
 *             A694 = the engaged gear 0x89C28; 0xF869C: A786 = min(pedal, 254)
 *   entry 14  0xF83A0 → 0xF846E → 0x123990 (the program priority loop, table
 *             0x1F3BE4, column A706) → program 1's handler 0x12D420 → THE WALK
 *             0x1277D8: from gear_in = A694, at most 8 iterations; the lines
 *             0x128D00, the thresholds 0x128E94 (two blends 0x128D9C of four
 *             maps 0x1290A8 / 0x121A56 at y = A786 and y = S+0xB0), the full-
 *             load blend (y = 0xFF), the step limits 0xFC358 (table 0x1E5E44)
 *   entry 15  0xF83E4 → 0xF8490: the confirm 0x123AA2 → the candidate, then
 *             0xFC5DE: step clamp against A694, the window 0xFCDFE [+0x2D,
 *             +0x2B], the minimum gear by speed, the 2nd-gear hold, 1..8 → A697
 *   entry 16  0xF7864: A0AA = (AB34 == 2) ? A004+0x3F : A697 (0xB912C copies
 *             it to 0x89CE8 every 10 ms)
 *
 * PORTED from the bytes: 0x121A56 (lookup), 0x121432 / 0x1214B4 / 0x121482
 * (table getters), 0x121CFA (saturation), 0x121BCC (the AB30 filter),
 * 0x128D00 (lines), 0x128D9C (four-map / two-weight blend), 0x1290A8 (the
 * id → map case for the remapped ids 0/5/10/15/20), 0x128E94 (thresholds:
 * both blends, D0/D2/D4/D6, the up-case B4 = B2 copy), 0x1281A6's pass-through
 * (S+0xAE == 0 or pedal < S+0xAF), 0x1277D8 / 0x1279C2 (the walk, every
 * branch), 0x128076 (S+0xDC), 0xFC358 (step limits incl. the column-47 cap),
 * 0xFC5DE (step clamp, the min-gear rule, the 2nd-gear hold, the 1..8 range),
 * 0xFCDFE (the window), the min-gear part of 0xFC112, 0xF8490 (the A694
 * fallback), 0xF7864.
 *
 * STOOD IN (compile-time, _Static_assert-ed to the stock driving dump
 * stockram_driving_20261104.bin; each use counted in sm_t's witnesses):
 *   level (from AB58 = 0) b = c = 1, weight 0; base A 0 / B 1 / weight 0
 *   (A188+0x6D/6C/6F); situation column A706 = 1; S+0xAF/B0 = 102/249
 *   (0xF9418 / 0xF9402 / 0xF93EC); S+0xAE = 1; S+0xB2 = 40 (0x128C04 not
 *   ported); the special limits inactive (0x128894's 0x12EC34 / 0x12EC5A
 *   flags: E6/EA = 10000, E8/EC = 0); the 0x128076 offset flag A1F8+0x13E = 0;
 *   0x12ECD0 / 0x12ECDE / 0xF8C5A flags 0; the 0xFC358 column-11 condition
 *   false; A67C+7 = 0, +4 = 0, +0x24 (0xFC07E) = 0; the window [1, 8]
 *   (A67C+0x2D / +0x2B, 0xFCAAA..0xFCC44 not ported); 0xFCA3A not ported;
 *   AB34 = 0; A004+0x3F = 6 (0xFFF85157); the strategy gate 0xFFF8AB9C+9
 *   open; the 10 ms event path not ported.
 *   THE PROGRAM ARBITRATION 0x123990: only program 1's handler is ported; a
 *   higher-priority id in the column (0x1B, 0x0B, 0x12, 0x08 in column 1) is
 *   skipped as if it did not set "valid" (counted).
 *   THE CONFIRM STAGE 0x127D3E / 0x1284CC: (2026-11-19) PORTED as code with
 *   its condition masks S+0x8C / S+0x94 HELD 0 — an exact pass-through
 *   (decode m6_shiftmap_gaps_decode.md §1); its cells S+0xE3 / E4 / E0 / C0 /
 *   DF / AD / 84 reproduce the dump. conf_n counts its runs.
 *   Pedal ≥ S+0xAF with S+0xAE != 0: 0x1281A6's corrections are NOT ported —
 *   the raw map value is kept (counted).
 * INPUTS BY VALUE (sm_in_t, app/sm_hook.c): D646 (our TIF12 output speed when
 * valid, else 0 — the stock's D646 substitute/decay is not ported), the pedal
 * 0xFFF9545E (0x120 byte 2 × 127 / 125 ≤ 254 when valid [I], else the dump's
 * no-frame value 23; raw 0xFF → 23, raw >= 0xFB → 0xFE: 0x13CA22), the
 * "current gear" 0x89C28 = the COMMANDED gear E4A7 of the shadow's own GEAR
 * FEEDBACK CHAIN (app/sm_gear.c, 2026-11-19; below) — the 30 ms pass's
 * gear_in; 0xFC112's "negative → 9" path (R: 0x89C28 0xFF → A694 9 →
 * 0xFC5DE(A697)) is ported.
 *
 * READ-ONLY, ENFORCED: every output TU defines TCU_OUTPUT_PATH — this header
 * #errors under it; app/sm_shadow.c defines TCU_SM_SHADOW_TU and the live
 * headers (sup_cells.h, drv_cells.h, drv_odiag.h, drv_temp.h, platform.h)
 * #error under it; sm_shadow_scan.py (outputs / shadow / hook, with negative
 * fixtures); sh-elf-gcc pass/fail checks; the live identity (test_g6b with
 * the shadow at its 30 ms slot against the r04r goldens).
 */
#ifndef TCU_APP_SM_SHADOW_H
#define TCU_APP_SM_SHADOW_H

#if defined(TCU_OUTPUT_PATH)
#error "the shift-map shadow is read-only: an output path may not include sm_shadow.h"
#endif

#include <stdint.h>
#include <stddef.h>

/* ── the stand-ins (the driving dump's values) ─────────────────────────── */
#define SM_SI_LVL_LO     1u      /* 0xFFF8A2C0+4: lower level b (AB58 = 0 → 1.00; the decode reads A2C0 = 0064 01 00 | 01 01 00 as curve 100, …, b, c, d [I]) */
#define SM_SI_LVL_HI     1u      /* +5: upper level c */
#define SM_SI_LVL_W      0u      /* +6: level weight d (%) */
#define SM_SI_BASE_A     0u      /* A188+0x6D (0xFFF8A1F5) */
#define SM_SI_BASE_B     1u      /* A188+0x6C (0xFFF8A1F4) */
#define SM_SI_BASE_W     0u      /* A188+0x6F (0xFFF8A1F7) */
#define SM_SI_A706       1u      /* the situation column 0xFFF8A706 */
#define SM_SI_AE         1u      /* S+0xAE 0x1281A6 enable */
#define SM_SI_AF         102u    /* S+0xAF */
#define SM_SI_B0         249u    /* S+0xB0 */
#define SM_SI_B2         40      /* S+0xB2 (0x128C04) */
#define SM_SI_C3         1u      /* S+0xC3 (0x12D420's last argument) */
#define SM_SI_LIM_HI     10000u  /* S+0xE6 / S+0xEA inactive */
#define SM_SI_LIM_LO     0u      /* S+0xE8 / S+0xEC inactive */
#define SM_SI_WIN_HI     8u      /* A67C+0x2B */
#define SM_SI_WIN_LO     1u      /* A67C+0x2D */
#define SM_SI_AB34       0u      /* 0xFFF8AB34 */
#define SM_SI_S3F        6       /* A004+0x3F ← 0xFFF85157 */
#define SM_SI_PEDAL      23u     /* 0xFFF8A786 with no 0x120 (the conditioned substitute [I]) */
#define SM_SI_GEAR       3       /* 0x89C28 in the dump; (2026-11-19) only the pass's initial A694 — the input is the gear chain's 0x89C28 */

/* the live inputs, BY VALUE (app/sm_hook.c, at the 30 ms slot) */
typedef struct {
    uint32_t t_us;      /* +0  TCNTA (witness) */
    uint16_t d646;      /* +4  output speed rpm (A004+0x0A) */
    uint8_t  pedal;     /* +6  conditioned pedal 0..254 (A004+0x89) */
    int8_t   gear_in;   /* +7  0x89C28 */
    uint8_t  os_mode;   /* +8  0x93B33 (witness) */
    uint8_t  q_speed;   /* +9  1 = our output speed was valid (witness) */
    uint8_t  q_pedal;   /* +10 1 = 0x120 valid (witness) */
    uint8_t  pad;       /* +11 */
} sm_in_t;
_Static_assert(sizeof(sm_in_t) == 12u, "sm_in_t is 12 bytes (witness_sm.py)");

/* one change of the target (A697) or the candidate */
typedef struct {
    uint32_t t_us;      /* +0 */
    uint16_t pass;      /* +4 30 ms pass (low 16) */
    uint16_t ab30;      /* +6 */
    uint8_t  gear_in;   /* +8 A694 */
    uint8_t  cand;      /* +9 the walk's S+0xE2 */
    uint8_t  a697;      /* +10 */
    uint8_t  pedal;     /* +11 A786 */
    uint16_t d8;        /* +12 the last up threshold */
    uint16_t da;        /* +14 the last down threshold */
} sm_tr_t;
_Static_assert(sizeof(sm_tr_t) == 16u, "sm_tr_t is 16 bytes (witness_sm.py)");

#define SM_TR_N 8u

typedef struct {
    /* +0x00 S = 0xFFF8A4E4 (the stock offsets in the names) */
    uint8_t  s_ac;          /* program id */
    uint8_t  s_ae;
    uint8_t  s_af;
    uint8_t  s_b0;
    int16_t  s_b2;
    int16_t  s_b4;
    uint8_t  s_b6;          /* "special upshift" flag (0x12133A reads it) */
    uint8_t  s_ba[6];       /* +0x09 S+0xBA..BF = p6, p9, p7, p10, p11, p8 */
    uint8_t  s_c2;          /* +0x0F gear_in */
    uint8_t  s_c3;          /* +0x10 */
    uint8_t  s_de;          /* +0x11 forced flag */
    uint8_t  s_df;          /* +0x12 */
    uint8_t  s_e2;          /* +0x13 THE COMPUTED CANDIDATE */
    uint8_t  s_e3;          /* +0x14 last lookup y */
    uint8_t  s_e4;          /* +0x15 last lookup line */
    uint16_t s_ce;          /* +0x16 full-load up line */
    uint16_t s_d0;          /* +0x18 up at the pedal (raw) */
    uint16_t s_d2;          /* +0x1A down at the pedal (raw) */
    uint16_t s_d4;          /* +0x1C up at y = S+0xB0 */
    uint16_t s_d6;          /* +0x1E down at y = S+0xB0 */
    uint16_t s_d8;          /* +0x20 THE UP THRESHOLD */
    uint16_t s_da;          /* +0x22 THE DOWN THRESHOLD */
    uint16_t s_dc;          /* +0x24 the speed compared */
    uint16_t s_e6, s_e8, s_ea, s_ec;   /* +0x26 the special limits */
    /* +0x2E A67C (0xFFF8A67C + off) and the outputs */
    uint8_t  m17;           /* min gear by speed */
    uint8_t  m18;           /* A694 = the current gear for the strategy */
    uint8_t  m1a;           /* A694 invalid */
    uint8_t  m1d;           /* the clamped step */
    uint8_t  m24;           /* 2nd-gear hold (stood in) */
    uint8_t  m38;           /* the window clamped */
    uint8_t  a697;          /* +0x34 THE TARGET 0xFFF8A697 (SHADOW) */
    uint8_t  a699;          /* +0x35 the last step limit handed to 0x128E94 */
    uint8_t  a0aa;          /* +0x36 0xFFF8A0AA (SHADOW) */
    uint8_t  ce8;           /* +0x37 0x89CE8 (SHADOW, = A0AA) */
    uint16_t ab30;          /* +0x38 filtered output speed 0xFFF8AB30 */
    uint8_t  a786;          /* +0x3A pedal 0xFFF8A786 */
    uint8_t  blk[10];       /* +0x3B the program block 0xFFF8A5D4 [0..9] */
    uint8_t  tr_head;       /* +0x45 */
    uint16_t s_e0;          /* +0x46 S+0xE0 the confirm threshold (0x12E76E) */
    /* +0x48 witnesses */
    uint32_t init_n;
    uint32_t pass_n;        /* 30 ms passes */
    uint32_t skip_mode_n;   /* calls outside modes 1..3 (never in the slot) */
    uint32_t walk_n;        /* walks run */
    uint32_t lookup_n;      /* map lookups */
    uint32_t iter_max;      /* most walk iterations in one pass */
    uint32_t err_n;         /* walk / lookup error returns (111 / 57 / 56 / unported id) */
    uint32_t err_last;      /* the last error code */
    uint32_t arb_skip_n;    /* higher-priority program ids skipped (unported handlers) */
    uint32_t arb_none_n;    /* no program valid → candidate A694 */
    uint32_t corr_n;        /* 0x1281A6 corrections NOT applied (pedal ≥ S+0xAF) */
    uint32_t conf_n;        /* confirm stage stood in (accepted) */
    uint32_t clamp_n;       /* 0xFC5DE step clamps */
    uint32_t win_n;         /* window clamps */
    uint32_t mingear_n;     /* fault 0x4F (the min-gear rule) */
    uint32_t range_n;       /* fault 0x96 (out of 1..8) */
    uint32_t gear_inv_n;    /* A694 invalid input */
    uint32_t chg_n;         /* A697 changes */
    uint8_t  s_c0;          /* +0x90 S+0xC0 (low byte; 0x129064's blend of S+0x85..0x88) */
    uint8_t  s_ad;          /* +0x91 S+0xAD (0x1284CC's first-call flag; 0 after 0x127D3E) */
    uint8_t  s_84;          /* +0x92 S+0x84 (a condition bit was active; masks 0 → 0) */
    uint8_t  neg9_n;        /* +0x93 0xFC112's "negative → 9" path taken (A694 9 → 0xFC5DE(A697)), saturating */
    sm_in_t  in;            /* +0x94 the last inputs */
    sm_tr_t  tr[SM_TR_N];   /* +0xA0 the last SM_TR_N changes (a ring) */
} sm_t;
_Static_assert(offsetof(sm_t, s_e2) == 0x13u, "sm_t layout (witness_sm.py)");
_Static_assert(offsetof(sm_t, s_ce) == 0x16u, "sm_t layout (witness_sm.py)");
_Static_assert(offsetof(sm_t, m17) == 0x2Eu, "sm_t layout (witness_sm.py)");
_Static_assert(offsetof(sm_t, a697) == 0x34u, "sm_t layout (witness_sm.py)");
_Static_assert(offsetof(sm_t, blk) == 0x3Bu, "sm_t layout (witness_sm.py)");
_Static_assert(offsetof(sm_t, init_n) == 0x48u, "sm_t layout (witness_sm.py)");
_Static_assert(offsetof(sm_t, in) == 0x94u, "sm_t layout (witness_sm.py)");
_Static_assert(offsetof(sm_t, tr) == 0xA0u, "sm_t layout (witness_sm.py)");
_Static_assert(sizeof(sm_t) == 0x120u, "sm_t is 0x120 bytes (.sm_shadow, witness_sm.py)");

void sm_shadow_init(void);
void sm_shadow_pass(const sm_in_t *in);     /* the 30 ms entries 5 (part), 14, 15, 16 */
const volatile sm_t *sm_shadow_state(void);

/* host-test seams (the program-1 walk alone, with explicit inputs) */
int  sm_shadow_walk(uint8_t gear_in, uint16_t speed, uint8_t pedal, uint8_t *cand);
uint8_t sm_shadow_step_limit(uint8_t program, uint8_t dir, uint8_t a67c7);
uint16_t sm_shadow_lut(uint8_t map_idx, uint16_t x, uint16_t y);

/* app/sm_hook.c — the gather (reads the live inputs by value) */
void sm_hook_init(void);
void sm_hook_30ms(void);                    /* main.c via tmp_chain's 30 ms slot */
void sm_hook_feed(uint16_t rpm, uint8_t q_speed, uint16_t pedal_permil, uint8_t q_pedal,
                  uint8_t os_mode);         /* the part after the gather (host harness) */

/* ══ THE GEAR FEEDBACK CHAIN (app/sm_gear.c; 2026-11-19, ~/tcu_sweep/
 *    gear_feedback_shadow_port.md; decode m6_shiftmap_gaps_decode.md §3,
 *    b2_slots_subobj_decode.md) — READ-ONLY, its OWN copies of the stock
 *    cells (the live engine owns 0xFFF8E4A7 / E69F / E6A1 and they are only
 *    read, by value, as a witness) ══════════════════════════════════════
 * The stock's 10 ms list (modes 1..3) slots 30..33, from the bytes:
 *   30  0xCFFDE: 0x10485E (E4A4 ← 0x89CE8 / 0x89C43 / CCE4[0] / E29D / AE70),
 *       0xD15A6 (E6A1: the mode-1 return, the R / power-flow-2 −1, 0xD1B10 →
 *       0xD1922 in power flow 0, 0xD1990 in D: the next-gear table 0x4F6A4,
 *       the 0xB7C14 / 0xBA848 coordinator overrides, 0xD16BA / 0xD1854 speed
 *       limits on the tables of *0xFFF84C80, 0xAEA3A)
 *   31  0xD01C4: 0x104A28 (E4A7 → 0x89C28 / 29 / 2A)
 *   32  0xD01E6: 0x103B36(CCE4, 1) (0x103C20, the strobe 0x103CEC, the
 *       completion → CCE4[0], 0x89C25 / 26 / 27, AE2E / AE2F), 0xD140C (on the
 *       strobe: E69F = E4A7, 0x89C2B..2E, E69C[0..2] with 0xB7C02 / 0xB7C14),
 *       then 0x103B36(CCE4, 0)
 *   33  0xDD954 the gear-pair coordinator's gear E33C+7 (0xDE190), E33C+8,
 *       0x8519F / 0x851A0
 * NOT PORTED (stand-ins in sm_gear.c, each counted where reachable): the
 * gear-pair shift engine itself (0xDDBB4 / 0xEB9EC / 0xDDDE8 → 0xEBA6E
 * block B, 0xDDB04): the shift object 0xFFF8BD14 is seen IDLE (BD14+1 = 0,
 * +2 = 0, BD15 = 0, +0x2C = 0 — the stock's values at rest and at the end of
 * an engagement), so a strobed gear completes in the same pass (0x103C2A →
 * 0xBAB5E = 1); 0xD0496 / 0xD0304 (the AE30 sequencer object); 0x1048C8
 * (E4A4[1] = 0); 0xDE190's early-next-gear branch; the event reports
 * (0xB79E4 id 0x9A, 0xD05A0) and the coordinator notifications. */
typedef struct {
    uint8_t  e69b;      /* +0  live committed power flow (0x89C30 = E69B) */
    uint8_t  e6ee;      /* +1  live completed power flow (0x89C2F) — pending = E6EE != E69B */
    uint8_t  e6ef;      /* +2  live power-flow request */
    uint8_t  e28e;      /* +3  live E28E state (0xD0D1C) */
    uint8_t  c3d;       /* +4  live 0x89C3D (the mode-1 flag) */
    uint8_t  os_mode;   /* +5  0x93B33 */
    uint8_t  cd2;       /* +6  live 0x89CD2 (the conditioned temperature byte) */
    int8_t   live_e4a7; /* +7  the live engine's E4A7 (witness only) */
    int8_t   live_e69f; /* +8  the live engine's E69F (witness only) */
    uint8_t  pad9;      /* +9  */
    uint16_t cb8;       /* +10 0x89CB8 output speed (stand-in: our output rpm, 0 if invalid) */
    uint16_t cba;       /* +12 0x89CBA turbine speed (stand-in: our turbine rpm, 0 if invalid) */
    uint16_t cd0;       /* +14 0x89CD0 engine speed (0x108 when valid, else 0) */
} gf_in_t;
_Static_assert(sizeof(gf_in_t) == 16u, "gf_in_t is 16 bytes (witness_sm.py)");

typedef struct {
    uint16_t pass;      /* +0 10 ms pass (low 16) */
    int8_t   e4a4;      /* +2 */
    int8_t   e6a1;      /* +3 */
    int8_t   e4a7;      /* +4 the commanded gear */
    int8_t   e69f;      /* +5 the gear code */
    int8_t   live_e4a7; /* +6 */
    uint8_t  e69b;      /* +7 */
} gf_tr_t;
_Static_assert(sizeof(gf_tr_t) == 8u, "gf_tr_t is 8 bytes (witness_sm.py)");
#define GF_TR_N 6u

typedef struct {
    /* +0x00 the stock objects (SHADOW copies, stock names) */
    int8_t   e4a4;      /* E4A4[0] the target (0x10485E) */
    uint8_t  e4a5;      /* E4A4[1] (0x1048C8: stand-in 0) */
    uint8_t  e4a6;      /* E4A6[0] the 0x104A28 sequencer flag */
    int8_t   e4a7;      /* E4A6[1] THE COMMANDED GEAR */
    int8_t   e6a1;      /* +0x04 the gear target (0xD15A6) */
    uint8_t  e69c[3];   /* +0x05 E69C[0] in progress, [1] 0xB7C02 type, [2] 0xB7C14 kind */
    int8_t   e69f;      /* +0x08 E69C[3] the gear code */
    uint8_t  e6a0;      /* +0x09 E69C[4] = ~E69F */
    int8_t   cce4[4];   /* +0x0A CCE4[0] the sequencer's gear, [1] skip, [2] 0xDDA54, [3] THE STROBE */
    int8_t   c25[3];    /* +0x0E 0x89C25..27 (after the completion) */
    int8_t   c28[3];    /* +0x11 0x89C28..2A (= E4A7: the strategy's "current gear") */
    int8_t   c2b[4];    /* +0x14 0x89C2B..2E (= E69F, ~) */
    int8_t   e343;      /* +0x18 E33C+7 the coordinator's gear */
    uint8_t  e344;      /* +0x19 E33C+8 */
    int8_t   x519f;     /* +0x1A 0x8519F (the last 0x89C2B) */
    uint8_t  x51a0;     /* +0x1B 0x851A0 */
    uint8_t  ae2e;      /* +0x1C AE28+6 */
    int8_t   ae2f;      /* +0x1D AE28+7 */
    uint8_t  tr_head;   /* +0x1E */
    uint8_t  pad1f;     /* +0x1F */
    /* +0x20 witnesses */
    uint32_t init_n;
    uint32_t pass_n;    /* 10 ms passes run (modes 1..3) */
    uint32_t skip_mode_n;
    uint32_t strobe_n;  /* CCE4[3] set (0x103CEC) */
    uint32_t done_n;    /* completions (0x103B36 → 0x89C25) */
    uint32_t e4a7_chg_n;
    uint32_t d1990_n;   /* 0xD1990 calls (D) */
    uint32_t d1b10_n;   /* 0xD1B10 calls (power flow 0) */
    uint32_t lim_n;     /* 0xD16BA / 0xD1854 / 0xD1922 changed the target */
    uint32_t ret1_n;    /* 0xD15A6's mode-1 return */
    uint32_t unp_n;     /* an unported branch reached (its stand-in taken) */
    uint32_t err_n;     /* a table index out of range */
    uint32_t agree_n;   /* passes with shadow E4A7 == live E4A7 and E69F == live E69F */
    uint32_t dis_n;     /* passes where they differ */
    uint32_t gp_n;      /* gear-pair starts the coordinator would make (0xEB9EC, not ported) */
    gf_in_t  in;        /* +0x5C the last inputs */
    gf_tr_t  tr[GF_TR_N]; /* +0x6C the last GF_TR_N changes of E4A7 / E69F */
} gf_t;
_Static_assert(offsetof(gf_t, init_n) == 0x20u, "gf_t layout (witness_sm.py)");
_Static_assert(offsetof(gf_t, in) == 0x5Cu, "gf_t layout (witness_sm.py)");
_Static_assert(offsetof(gf_t, tr) == 0x6Cu, "gf_t layout (witness_sm.py)");
_Static_assert(sizeof(gf_t) == 0x9Cu, "gf_t is 0x9C bytes (.sm_gear, witness_sm.py)");

void sm_gear_init(void);
void sm_gear_pass(const gf_in_t *in);       /* the 10 ms slots 30..33 */
const volatile gf_t *sm_gear_state(void);
#if !defined(__sh__)
void sm_gear_host_si(uint8_t ae2d, int8_t c43);   /* host only: the fault-reaction inputs (dump: 1, 3) */
#endif
void sm_hook_10ms(void);                    /* main.c's task_10ms, after the engine */
void sm_hook_gear_feed(const gf_in_t *in);  /* the part after the gather (host harness) */

#endif /* TCU_APP_SM_SHADOW_H */
