/* pa7_shadow.h — a READ-ONLY shadow of the stock's PA7 / "TRS Park signal"
 * law and its AN7 read-back checks (app/pa7_shadow.c; ~/tcu_sweep/
 * pa7_decode.md, the port ~/tcu_sweep/r04d_recorder_pa7_shadow.md).
 *
 * What the stock does [O bytes]:
 *   range task 0x9B34A (10 ms, OS modes 1..3, after the D6D6 chain) and the
 *   init one-shot 0x9B228 → 0xA6CF8:
 *     0xA6D12 / 0xA6D6E → E567 (8 → 2, 6 → 1, 127 → 3, else 0);
 *     0xC6116 → D713 (the override flag), 0xC60CE → D712 (the state 0..3);
 *   the pin-cut entry 0x7CC32 (5 ms mode 0 / 10 ms modes 1..3), its 6th
 *   call 0x86B0C, gated on 0x9095B >= 15:
 *     (0x7AB9C → 0x7ACF2: 0x94122 = D712, earlier in the same list)
 *     0x8CCD4 (from its 2nd pass, 0x9411F) the AN7 line check → 0x93B2C /
 *       0x93B2E (faults 4 / 8 / 0x100 / 0x200), its valid 0x8CDDC;
 *     0x8CE48 the law: state → PA7 (0x94120, 0xFFF90920, PADR bit 7 via
 *       0x90CF6) and 0x9095A (0x94121) = the C9 bit-7 request;
 *     0x8CB7C AN7 → 0x93A94 / 0x93B2A (±1000 mV step limit), the supervisor
 *       nibble → 0x9417B, the expected level → 0x9417C bits 0 / 1, the
 *       blanking timers 0x94784 / 0x94834.
 *
 * The SHADOW runs all of that into ITS OWN cells (pa7s_t, linker section
 * .pa7_shadow at 0xFFF87880, ours): it writes NOT 0xFFF90920, NOT PADR, NOT
 * 0x9095A (the live C9 cell), NOT the live range cells and NOT the live
 * countdown words (it keeps its own copies of 0x94766 / 0x94768 / 0x94784 /
 * 0x94834 and steps them itself, as fb_shadow keeps 0x9476E). Its inputs
 * arrive BY VALUE (pa7_rng_in_t / pa7_pin_in_t, filled by app/pa7_hook.c).
 *
 * NOT PORTED / STOOD IN (stated):
 *   - 0x9095B: its producer is not ported in our firmware (sup_ctrl.c) —
 *     the shadow models it: 0x91DAA (the batch, after the pin-cut entry)
 *     sets 1 on the first batch pass, 0x91ACA (the reply proc, before the
 *     pin-cut entry) turns 1 into 0x0F on the next one [O bytes 0x91DB8,
 *     0x91B94..0x91BD8] — so 0x86B0C runs from the SECOND pin-cut pass;
 *   - 0xA6D12's override (cal 0x1960B8 = 00: off) and its 0xC627A / 0xC62F6
 *     maintenance of 0xFFF8E568 / 0xFFF83078 / 0xFFF83046 (feed only the
 *     override);
 *   - 0xFFF81C6C (0xC6116, read through 0xA7B08): the dump's 00 (its writer
 *     is not ported; it matters only with D60A = 1);
 *   - 0x887C0's report to the supplier ring 0xFFF948B8 (counted);
 *   - mode 5 (power-down, 0x93248 forces PA7 0): our firmware has none.
 *
 * READ-ONLY, ENFORCED: every output TU defines TCU_OUTPUT_PATH — this header
 * #errors under it; app/pa7_shadow.c defines TCU_PA7_SHADOW_TU and the live
 * headers (sup_cells.h, sup_ctrl.h, rng_chain.h, park_cells.h, platform.h)
 * #error under it; pa7_shadow_scan.py (outputs / shadow / hook, with
 * negative fixtures); sh-elf-gcc pass/fail checks; the live-identity test
 * (fb_identity.py over test_odiag with the PA7 hooks at their slots).
 */
#ifndef TCU_APP_PA7_SHADOW_H
#define TCU_APP_PA7_SHADOW_H

#if defined(TCU_OUTPUT_PATH)
#error "the PA7 shadow is read-only: an output path may not include pa7_shadow.h"
#endif

#include <stdint.h>
#include <stddef.h>

/* ── the calibration (variant offset 0xFFF90AB8 = 0; pointer pools 0x668B8 /
 *    0x66C70.. [O bytes]); byte-checked against the stock image by the host
 *    test (test_pa7_shadow --cal) ───────────────────────────────────────── */
#define PA7_CAL_ENABLE      0x01u       /* 0x1919DF (pool 0x668B8) */
#define PA7_CAL_THI         0x0578u     /* 0x1915BE = 1400 (pool 0x668BC): 7.0 s */
#define PA7_CAL_TLO         0x0006u     /* 0x1915C0 = 6 (pool 0x668C0): 30 ms */
#define PA7_CAL_SUPSEL      0xFEu       /* 0x191A18 (pool 0x66C70): != 0xFF → 0x94F86 / 0x94FEC */
#define PA7_CAL_BLANK_LO    0x0096u     /* 0x191898 = 150 (pool 0x66C74): 750 ms after going LOW */
#define PA7_CAL_BLANK_HI    0x0Au       /* 0x191A19 = 10 (pool 0x66C78): 50 ms after going HIGH */
#define PA7_CAL_STEP        0x03E8u     /* 0x19189A = 1000 mV (pool 0x66C7C) */
#define PA7_CAL_SENSE       0x01u       /* 0x191A1A (pool 0x66C80): 1 = low → >9000 check */
#define PA7_CAL_MASK        0xFFFFu     /* 0x19189C (pool 0x66C84) */
#define PA7_CAL_SUPMIN      0x1770u     /* 0x19189E = 6000 mV (pool 0x66C88) */
#define PA7_CAL_HIMV        0x2328u     /* 0x1918A0 = 9000 mV (pool 0x66C8C) */
#define PA7_CAL_LOMV        0x07D0u     /* 0x1918A2 = 2000 mV (pool 0x66C90) */
#define PA7_CAL_AN          0x07u       /* 0x1918D3: the AN-table index (AN7) */
#define PA7_CAL_D65A_MIN    0x1388u     /* 0x196072 = 5000 mV (via *0xFFF84AE8 = 0x4F024) */
#define PA7_CAL_E567_OVR    0x00u       /* 0x1960B8 (via *0xFFF84AEC + 0x10): override off */
_Static_assert(PA7_CAL_SUPSEL != 0xFFu, "pa7: the supply selector picks 0x94F86 / 0x94FEC on this unit");
_Static_assert(PA7_CAL_E567_OVR == 0u, "pa7: the E567 override (0xC627A / 0xC62F6 path) is not ported");

/* the range part's live inputs, BY VALUE (app/pa7_hook.c: the range chain's
 * cells at the stock addresses, after its pass) */
typedef struct {
    uint8_t  d6d6;      /* 0xFFF8D6D6 (0xA7B78) */
    uint8_t  d6da;      /* 0xFFF8D6DA (0xA7B8C) the qualifier */
    uint8_t  d672;      /* 0xFFF8D672 (0xA7820) the lever */
    uint8_t  d673;      /* 0xFFF8D673 (0xA7848) the lever source (2 = none) */
    uint8_t  d65e;      /* 0xFFF8D65E */
    uint8_t  d5f8;      /* 0xFFF8D5F8 (0xA7716) the park sensor says P */
    uint8_t  d5f9;      /* 0xFFF8D5F9 (0xA7720) safety reaction */
    uint8_t  d60a;      /* 0xFFF8D60A (0xA77AA) */
    int16_t  d65a;      /* 0xFFF8D65A supply (mV) */
    uint8_t  os_mode;   /* 0x93B33 (witness) */
    uint8_t  pad;
} pa7_rng_in_t;

/* the pin-cut part's live inputs, BY VALUE */
typedef struct {
    uint32_t t_us;      /* TCNTA (witness) */
    uint8_t  n5;        /* 5 ms countdown steps since the last pass (mode 0: 1, else 2) */
    uint8_t  nib;       /* 0xFFF90958 the supervisor nibble (J6 rx[1] & 0x0F) ^ 0x0F */
    uint8_t  sup_st;    /* 0x94FEC the supply status (8 = no fault) */
    uint8_t  os_mode;   /* 0x93B33 (witness) */
    int16_t  an7;       /* 0x9018C[7] AN7 (mV) */
    int16_t  sup;       /* 0x94F86 the supply (mV) */
} pa7_pin_in_t;

/* one PA7 / C9-bit-7 change (the transition ring) */
typedef struct {
    uint32_t t_us;      /* +0 */
    uint16_t pass;      /* +4 pin-cut pass number (low 16) */
    uint8_t  d6d6;      /* +6 the range the state was built from */
    uint8_t  d672;      /* +7 */
    uint8_t  d673;      /* +8 */
    uint8_t  e567;      /* +9 */
    uint8_t  d712;      /* +10 (the copy 0x94122 the law used) */
    uint8_t  out;       /* +11 bit 0 PA7, bit 1 C9 bit 7 (0x9095A) */
} pa7_tr_t;
_Static_assert(sizeof(pa7_tr_t) == 12u, "pa7_tr_t is 12 bytes (witness_pa7.py)");

#define PA7_TR_N 16u

typedef struct {
    /* +0x00 the range part (0xA6CF8), at the stock's cell names */
    uint8_t  e567;          /* 0xFFF8E567 */
    uint8_t  d712;          /* 0xFFF8D712 the state */
    uint8_t  d713;          /* 0xFFF8D713 the override */
    uint8_t  a6d6e;         /* 0xA6D6E's range code (8 / 6 / 7 / 5 / 0 / 127) */
    /* +0x04 the pin-cut part */
    uint8_t  s94122;        /* 0x94122 (0x7ACF2's copy of D712) */
    uint8_t  pa7;           /* 0x94120 = 0xFFF90920 = PADR bit 7 (SHADOW) */
    uint8_t  c9b7;          /* 0x94121 = 0x9095A, the C9 bit-7 request (SHADOW) */
    uint8_t  s9095b;        /* our model of 0x9095B (0 → 1 → 0x0F) */
    uint16_t t_hi;          /* +0x08 0x94766 (1400, counts while PA7 is low) */
    uint16_t t_lo;          /* +0x0A 0x94768 (6, counts while PA7 is high) */
    uint16_t an7_raw;       /* +0x0C 0x93A94 */
    uint16_t an7_f;         /* +0x0E 0x93B2A (step-limited) */
    uint16_t blank_w;       /* +0x10 0x94784 (word) */
    uint8_t  blank_b;       /* +0x12 0x94834 (byte) */
    uint8_t  s9417b;        /* +0x13 0x9417B the latched nibble */
    uint8_t  s9417c;        /* +0x14 0x9417C b0 expected high, b1 its copy, b2 jump hold,
                             *        b3 initialised, b4 input-1 check skipped, b5 */
    uint8_t  s9411f;        /* +0x15 0x9411F (0x8CCD4 runs from the 2nd pass) */
    uint16_t fail;          /* +0x16 0x93B2C */
    uint16_t tested;        /* +0x18 0x93B2E (tested & mask) */
    uint8_t  valid;         /* +0x1A the last 0x8CDDC result (ours: witness) */
    uint8_t  tr_head;       /* +0x1B next transition slot */
    /* +0x1C our witnesses */
    uint32_t rng_n;         /* range-part passes (incl. the init one) */
    uint32_t pin_n;         /* pin-cut passes */
    uint32_t law_n;         /* 0x86B0C passes past the 0x9095B gate */
    uint32_t hi_n;          /* law passes with PA7 = 1 */
    uint32_t c9b7_n;        /* law passes with 0x9095A = 1 */
    uint32_t tr_n;          /* PA7 / C9b7 changes */
    uint32_t report_n;      /* 0x887C0 calls (the supplier ring: not ported) */
    uint32_t f4_n;          /* passes with fault 4 / 8 / 0x100 / 0x200 */
    uint32_t f8_n;
    uint32_t f100_n;
    uint32_t f200_n;
    uint32_t first_law_us;  /* TCNTA of the first law pass */
    /* +0x4C the last inputs (by value, witness) */
    pa7_rng_in_t rin;       /* +0x4C (12 bytes) */
    pa7_pin_in_t pin;       /* +0x58 (12 bytes) */
    pa7_tr_t tr[PA7_TR_N];  /* +0x64 the last PA7_TR_N changes (a ring) */
} pa7s_t;
_Static_assert(offsetof(pa7s_t, rng_n) == 0x1Cu, "pa7s_t layout (witness_pa7.py)");
_Static_assert(offsetof(pa7s_t, rin) == 0x4Cu, "pa7s_t layout (witness_pa7.py)");
_Static_assert(offsetof(pa7s_t, pin) == 0x58u, "pa7s_t layout (witness_pa7.py)");
_Static_assert(offsetof(pa7s_t, tr) == 0x64u, "pa7s_t layout (witness_pa7.py)");
_Static_assert(sizeof(pa7s_t) == 0x124u, "pa7s_t is 0x124 bytes (.pa7_shadow, witness_pa7.py)");

void pa7_shadow_init(void);                         /* all shadow cells to the stock's init */
void pa7_shadow_range(const pa7_rng_in_t *in);      /* 0xA6CF8 (range task / init one-shot) */
void pa7_shadow_pin(const pa7_pin_in_t *in);        /* 0x7ACF2's copy + 0x86B0C */

const volatile pa7s_t *pa7_shadow_state(void);

/* app/pa7_hook.c — the gathers (they read the live cells by value) */
void pa7_hook_range(uint8_t os_mode);   /* main.c: after rng_chain_tick_10ms (modes 1..3) */
void pa7_hook_range_init(void);         /* main.c: after rng_chain_init (the 0x9B228 call) */

#endif /* TCU_APP_PA7_SHADOW_H */
