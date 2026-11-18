/* Park output law — the stock's park module from the target to the two
 * on/off demands, ported 1:1 from the bytes (app/park_law.c;
 * ~/tcu_sweep/range_logic_port.md §3; park_outputs_decode.md §1.1):
 *
 *   0xB912C  (part) 0x89C6C = the target 0xFFF8D6D6 (getter 0x9B588 →
 *            0xA7B78); 0x89CDC = 0xB97F8(0x89C6C): diag test 0xBB060
 *            active and target != 8 → 6; diag 0xBA734 active → 8
 *   0xCFFDE  0xAE28 = table 0x63445[0x89CDC] (direction, 0xFF = keep);
 *            0xAE29 PARK REQUEST = (0x89CDC == 8 && aux 0x89CF2 == 0) ||
 *            (aux != 0 && car-wash inhibit 0x8E69B == 0)
 *   0xE89BA  the park object's tick: timer +1 (sat 255), window −1 (floor 0)
 *            (step 0xFFF8E220 = 1)
 *   0xE88FA  window reload (while the inhibit 0x89C30 is set: map 0x1B5050
 *            at temperature 0x89C13); THE LOCK DECISION
 *              lock = 0x89C30 == 0 && (aux || (0xAE29 && (lock || 0xE8B56)))
 *            (a change clears the timer); outputs: release = (0xAE29 == 0),
 *            hold = (lock == 0); the diag overrides 0xBB110/0xBB120
 *   0xE8B56  the interlock: refuses a new lock only inside the window, with
 *            speed 0x89D00 > 10 and 0x89CB8 < 5 and valid (0xE8B7E), unless
 *            0xE8BAA (cal 0x1FE8C7 == 0 && 0xFFF8B272 != 0)
 *   0xE89DE  the timer model 0xFFF8BC70[0]: 1 released, 2 releasing (150 ms,
 *            cal 0x1FE8C6 = 15 ticks), 3 engaging (map 0x1B5044 at 0x89C13,
 *            ≈ 200 ms at 27 °C, only once the hold is off), 4 engaged
 *   0xE8888  the on/off demand: id 2 (release) / id 3 (hold) → 10000 or 0
 * Called every 10 ms after the range chain (app/rng_chain.c) has written D6D6.
 */
#ifndef TCU_PARK_LAW_H
#define TCU_PARK_LAW_H

/* THE RANGE CHAIN IS ISOLATED (app/rng_chain.c, ours): its translation
 * unit defines TCU_RNG_CHAIN_TU and may not include an output-side header —
 * its only way to the outputs is the range D6D6, read by app/park.c */
#if defined(TCU_RNG_CHAIN_TU)
#error "the range chain is isolated: it may not include an output-side header (park_law.h)"
#endif

#include <stdint.h>
#include <stdbool.h>

/* the inputs this law reads besides the target, each named by its stock
 * cell. The producers of every one of them are NOT ported (stand-ins in
 * app/park.c: the driving dump's values; the temperature from our 0x94103) */
typedef struct {
    uint8_t  inhibit;       /* 0x89C30 global inhibit (~20 readers; dump 0) */
    uint8_t  aux;           /* 0x89CF2 auxiliary park request (dump 0) */
    uint8_t  carwash;       /* 0x8E69B car-wash inhibit (dump 0) */
    uint8_t  temp40;        /* 0x89C13 temperature + 40 °C (dump 0x43 = 27 °C) */
    int16_t  vspeed;        /* 0x89D00 speed, signed (dump 0) */
    uint16_t v89cb8;        /* 0x89CB8 (dump 0) */
    uint8_t  v89d49;        /* 0x89D49 validity, 0 = valid (dump 2) */
    uint8_t  b272;          /* 0xFFF8B272 (dump 0) */
    uint8_t  diag_rng6;     /* 0xBB060(0xFFF8E61C): range test active → 6 */
    uint8_t  diag_rng8;     /* 0xBA734(0xFFF8E618): range test → 8 */
    uint8_t  diag_out_en;   /* bit k: 0xBB110(0xFFF8E61C, k) output k overridden */
    uint8_t  diag_out_val;  /* bit k: 0xBB120 value for output k */
} park_law_in_t;

/* the stock calibration [O bytes; pointer table 0x56EC4 via 0xFFF84D3C] */
#define PL_CAL_RELEASE_T    15u     /* 0x1FE8C6: releasing → released, 10 ms ticks */
#define PL_CAL_SPEED_T      10      /* 0x1FF1B4 (s16): interlock speed limit */
#define PL_CAL_89CB8_T      5       /* 0x1FF1B6 (s16): interlock 0x89CB8 limit */
#define PL_CAL_E8BAA        0u      /* 0x1FE8C7 */
#define PL_STEP             1u      /* 0xFFF8E220 (dump 0001) */

void park_law_init(void);
/* one 10 ms pass: the target is 0xFFF8D6D6 as read by 0xB912C */
void park_law_step(uint8_t target, const park_law_in_t *in);

/* 0xE8888(id): 10000 or 0 (id 2 release, 3 hold; 1 → 0; others 0) */
uint16_t park_law_demand(uint8_t id);

/* the u8 map lookup 0xB6D6C ({ u32 n, u8 x[n], u8 y[n] }, x u8, linear,
 * truncated toward the lower y, clamped) */
uint8_t park_law_map_u8(const uint8_t *tbl, uint8_t x);
uint8_t park_law_engage_ticks(uint8_t temp40);    /* map 0x1B5044 */
uint8_t park_law_window_ticks(uint8_t temp40);    /* map 0x1B5050 */

/* read-outs (the pinned cells, app/park_cells.h) */
uint8_t park_law_state(void);       /* 0xFFF8BC70[0] */
uint8_t park_law_lock(void);        /* 0xFFF8BC70[1] */
uint8_t park_law_request(void);     /* 0xFFF8AE29 */
bool park_law_integrity_ok(void);   /* the 0xFFF851A8 / 0xFFF851AD pairs */

/* THE FAULT-REACTION PARK REQUEST 0x89CF2 (2026-11-17, ~/tcu_sweep/
 * keyoff_autop_port.md §3) — 0xB986C 0xB9B52..0xB9BCE [O bytes]: with
 * r13 = 0xFFF89C3C (GBR block),
 *   +0xB4 (0x89CF0) = 1 unless 0x89C3C == 0 && 0x89C42 ∉ {6, 8} &&
 *                     0x89CED ∉ {1, 7} && 0xB81CE(0xFFF8E5C4) == 0;
 *   +0xB6 (0x89CF2) = +0xB7 (0x89CF3) = +0xB4 (0xB9BC6..0xB9BCE).
 * 0xB81CE ignores its argument: 0xE358C(0xFFF8B7BC) = byte 0xFFF8B7BE (set by
 * 0xE32FE, a data-set identity check at start-up, fault 0x1B1 [I name]).
 * 0x89CED = 0xDD118(E6C6, 0x89C44): 0/1/2/7 pass, 6 → 0, else 1 + event
 * 0x49C; 0x89C44 = 0x14E0B6(0xFFF95838) +0x11 = 1 while fault reaction 56
 * (0x632DE = 0x38, the OS-mode-3 reaction) is active, 2 while reaction 57
 * (0x632DF) is, else 0 [O 0x14E0FC]. */
typedef struct {
    uint8_t c3c;            /* 0x89C3C the one-hot OS mode-3 flag (slot 26) */
    uint8_t c42;            /* 0x89C42 the safety substitute-gear code (← 0x89C41) */
    uint8_t ced;            /* 0x89CED the second safety code (reactions 56 / 57) */
    uint8_t b7be;           /* 0xFFF8B7BE the start-up data-set mismatch flag */
} park_aux_in_t;
/* computes 0x89CF0 / 0x89CF2 / 0x89CF3 and returns 0x89CF2 */
uint8_t park_law_aux(const park_aux_in_t *in);

#endif
