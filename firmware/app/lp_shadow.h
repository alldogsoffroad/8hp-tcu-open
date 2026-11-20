/* lp_shadow.h - a READ-ONLY shadow of the stock's HEALTHY LINE-PRESSURE /
 * Sys request law (2026-11-20; decodes ~/tcu_sweep/line_pressure_decode.md and
 * ~/tcu_sweep/line_pressure_torque_decode.md, port ~/tcu_sweep/lp_ti_shadow_port.md
 * - every address below re-read in the stock image).
 *
 * WHAT IT IS. The stock's producer 0xDC4C4 -> 0xBABEE -> 0xE9CE0(0xFFF8BCB0) builds
 * the line-pressure target (a max of demands, one reason each), then offers it as
 * the Sys request E53DA(Sys, p, mode = 0x89CF5, 4, 0x15); the Sys law (0xE54A4 ->
 * 0x107FB8: binary 0x10809C in mode != 0, else the curve 0x107FE0 = per-unit curve
 * 0xE108 + the NO temperature map 0xE170, clamp [500, 8500]) gives the current.
 * Today our LIVE engine compiles Sys to the constant 500 (D-G6-10: the stock's
 * bench fault reaction 50 forces 0x89CF5 = 1 and the maximum line pressure). This
 * shadow computes what the stock would command WITHOUT that fault - "the healthy
 * case" - and compares it with the live Sys every pass in a witness. It NEVER
 * writes a live cell, never offers anything to the producer, never reaches a bus.
 *
 * HEALTHY ASSUMPTION (labelled, HELD): fault reaction 50 = 0, and the other
 * terms of the 0x89CF5 OR (0xB8D8 engine-start module 0xFFF8B10C, the t3 latch
 * 0xFFF85195, 0xDCFDA(E6C6, 6)) = 0. LP_R50_HELD / lp_shadow_host_r50() is the
 * only way to force it (a host test seam; the SH build asserts it is 0).
 *
 * PORTED (the bytes read): 0xE9CE0's body, 0xEA230, 0xEA0EE (the start maximum,
 * reason 1) with the slot-36 counters 0xEA01E, 0xEA03A (the dispatch), 0xEA37E
 * (the steady demand: reason 0x19 / 0x18 maps), 0xE9E6A (reason 0x10), 0xEA28C /
 * 0xE9FF0 / 0xE9ED2 (the gear-change demand, reason 0x11), 0xEA07E, 0xEA4C8 (the
 * conditional demands: 0x05 0x0C 0x13 0x1A 0x2D 0x48 and the priority-8 forced
 * zero), 0xEAB90 (leaving P), 0xEA9EE (the hot window), 0xEA806 / 0xEA84E /
 * 0xEA7BA, 0xEA774, 0xEA2F8, 0xEA33E with the decrease filter 0xD1F18 / 0xD1F06,
 * the per-gear maximum clamp, 0xD1D24 / 0xD1D38; the Sys law (the binary and the
 * curve branch with 0xD9EA4's temperature map) and the input torque 0xDDFD8 (the
 * T3 conditioning 0xD4580, the T2/T3 blend 0xD464C, the converter torque ratio
 * 0xD522A). Interpolators 0xB6EAE / 0xB6E10 / 0xB6D6C / 0xB7164 / 0xB72D8 as the
 * live engine's (app/rq_fill.c) - re-written here, the shadow includes no live code.
 *
 * STOOD IN (each counted in lp_w_t): the conditions behind 0xEA4C8's undecoded
 * reasons (0x02 0x31 0x3B 0x03 0x21 0x04 0x06 0x0F 0x1C: the adaptation readers
 * 0xAEA24 / 0xAEA5C / 0xAEAC0 / 0xAEB8C, 0xBA19E, 0xB8064, 0xBA73C, 0xEAB0A, the
 * converter state 0xFFF8B5A8) = not offered; reason 0x0A above cal+0x74 = 8000 rpm
 * (0xEA89C) not ported; 0xEB118 (the cal+0x40 = 0 add); the class-2 filter
 * (every reason is class 0: the table 0x1DE91C is all zero, re-read); variant 1
 * of the gear maps (0x89C04 != 0). */
#ifndef TCU_APP_LP_SHADOW_H
#define TCU_APP_LP_SHADOW_H

#if defined(TCU_OUTPUT_PATH)
#error "the line-pressure shadow is read-only: an output path may not include lp_shadow.h"
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define LP_R50_HELD 0u     /* fault reaction 50 (0x89CF5's t1): HELD 0 = the healthy case */

/* the inputs, BY VALUE (app/lp_hook.c): the stock cell each one stands for */
typedef struct {
    int8_t   c25;       /* +0  0x89C25 engaged gear */
    uint8_t  c26;       /* +1  0x89C26 = engaged + 1 (the map index) */
    int8_t   c27;       /* +2  0x89C27 */
    int8_t   c2b;       /* +3  0x89C2B gear code */
    uint8_t  c2c;       /* +4  0x89C2C = gear code + 1 */
    int8_t   c2d;       /* +5  0x89C2D */
    uint8_t  c2f;       /* +6  0x89C2F completed power flow (E6EE) */
    uint8_t  c30;       /* +7  0x89C30 committed power flow (E69B) */
    uint8_t  bd15;      /* +8  0xFFF8BD15 */
    uint8_t  bd16;      /* +9  0xFFF8BD16 */
    uint8_t  bd8b;      /* +10 0xFFF8BD8B (block A +0x1F, meaning undecoded) */
    uint8_t  bd72;      /* +11 0xFFF8BD72 (undecoded) */
    uint8_t  bd74;      /* +12 0xFFF8BD74 (undecoded) */
    uint8_t  fill_any;  /* +13 0x1069BC: +0x17 of a garage sub-object (D0F3 / D15F / D1CB) */
    uint8_t  bphase4;   /* +14 0x106988: B in phase 4 (the start maximum's end) */
    uint8_t  cf4;       /* +15 0x89CF4 (0xFFF8A0A2, dump 01) */
    uint8_t  d10;       /* +16 0x89D10 engine stopped */
    uint8_t  temp;      /* +17 0x89C13 */
    uint8_t  cd2;       /* +18 0x89CD2 */
    uint8_t  range;     /* +19 0x89CDC the application range (P = 8) */
    uint8_t  pos;       /* +20 0x89CDE the park position (0xFFF8EA40) */
    uint8_t  d1d;       /* +21 0x89D1D */
    uint8_t  c18;       /* +22 0x89C18 (the T3 conditioning input; dump 0x4B) */
    uint8_t  os_mode;   /* +23 0x93B33 */
    uint16_t eng;       /* +24 0x89CD0 / 0x89C10 engine speed */
    uint16_t out;       /* +26 0x89CB8 / 0x89C08 conditioned output speed */
    uint16_t now_ms;    /* +28 0x89CF8 the ms clock */
    uint16_t spd_ratio; /* +30 the converter speed ratio x 1000 (turbine / engine) */
    int16_t  t1;        /* +32 0x89CAA T1 actual engine torque, Nm */
    int16_t  t2;        /* +34 0x89CB4 T2, Nm */
    int16_t  t3;        /* +36 0x89CA8 T3, Nm */
    int16_t  ca0;       /* +38 0x89CA0 */
    uint16_t live_sys;  /* +40 the LIVE law's Sys current (0xFFF94ED4 + 12), witness only */
    uint16_t pad;       /* +42 */
} lp_in_t;
_Static_assert(sizeof(lp_in_t) == 44u, "lp_in_t is 44 bytes");

/* the witnesses (all u32 counters unless noted) */
typedef struct {
    uint32_t pass_n;        /* +0x00 10 ms passes run (modes 1..3) */
    uint32_t skip_mode_n;   /* +0x04 */
    uint32_t cmp_n;         /* +0x08 passes compared with the live Sys */
    uint32_t diff_n;        /* +0x0C passes where the would-be Sys differs from the live one */
    uint32_t err_n;         /* +0x10 a table index out of range (0xB75B4's report, counted) */
    uint32_t si_cond_n;     /* +0x14 undecoded 0xEA4C8 conditions taken as 0 (per pass) */
    uint32_t si_r0a_n;      /* +0x18 reason 0x0A would be offered (engine above 8000, not ported) */
    uint32_t si_class2_n;   /* +0x1C an offer with a nonzero class (class-2 path, not ported) */
    uint32_t si_var_n;      /* +0x20 gear-map variant != 0 (0x89C04), variant 0 used */
    uint32_t sys_bin_n;     /* +0x24 passes the Sys law ran its binary branch */
    uint32_t fault_n;       /* +0x28 the Sys law's fault reports (0x19A / 0x1C2), counted */
    uint32_t fill_n;        /* +0x2C start-maximum arms (obj+0x12 set) */
    uint32_t pt1_n;         /* +0x30 decrease-filter passes */
} lp_w_t;
_Static_assert(sizeof(lp_w_t) == 0x34u, "lp_w_t is 0x34 bytes");

/* the shadow's state: the object 0xFFF8BCB0 (stock offsets in the comments) and the Sys result */
typedef struct {
    uint16_t res;       /* +0x00 the target (BAA58) */
    uint16_t p10;       /* +0x02 res / 10 */
    uint16_t w04;       /* +0x04 the winner / forced value */
    uint16_t w06;       /* +0x06 the last +4 */
    uint16_t w08;       /* +0x08 the class-2 filtered demand */
    uint16_t w0a;       /* +0x0A the class-2 demand */
    uint16_t w0c;       /* +0x0C the normal demand */
    uint16_t w0e;       /* +0x0E max(res - 5000, 0) */
    uint16_t w10;       /* +0x10 the PT1 model (0xBAA5E) */
    uint8_t  b12;       /* +0x12 the start maximum on */
    uint8_t  b13;       /* +0x13 offset taken */
    uint16_t w14;       /* +0x14 offset */
    uint8_t  b16;       /* +0x16 winning reason */
    uint8_t  b17;       /* +0x17 class-2 reason */
    uint8_t  b18;       /* +0x18 normal reason */
    uint8_t  b19;       /* +0x19 the last winning reason */
    uint8_t  b1a;       /* +0x1A the steady reason (0x18 / 0x19) */
    uint8_t  b1b;       /* +0x1B forced flag */
    uint8_t  b1c;       /* +0x1C class-2 hold */
    uint8_t  b1d;       /* +0x1D start maximum done */
    uint8_t  b1e;       /* +0x1E the last +0x12 */
    uint8_t  b1f;       /* +0x1F start maximum counter */
    uint8_t  b20;       /* +0x20 the last range (0xEAB90) */
    uint8_t  b24;       /* +0x24 the hot window (0xEA9EE) */
    uint8_t  b25;       /* +0x25 */
    uint8_t  b26;       /* +0x26 the b10c falling edge */
    uint8_t  b27;       /* ours: the b10c state for the falling edge */
    uint8_t  b28;       /* ours: the last map index (the table-error fallback) */
    uint16_t w22;       /* +0x22 the 0xEAB90 counter */
    uint32_t d28;       /* +0x28 the forced priority */
    int32_t  pt1_v;     /* +0x2C the decrease filter's value (x 1000) */
    int16_t  pt1_t;     /* +0x30 its time stamp */
    int16_t  pt2_t;     /* +0x32 the class-2 filter's time stamp */
    int32_t  pt2_v;     /* +0x34 its value (x 128) */
    int32_t  pt2_r;     /* +0x38 its rate (x 128) */
    int32_t  pt3_v;     /* +0x3C the 0xBAA5E model's value (x 1000) */
    int16_t  pt3_t;     /* +0x40 */
    uint16_t last_res;  /* +0x42 0xFFF8513E: the last result */
    int16_t  e34a;      /* +0x44 the input torque 0xFFF8E34A, Nm */
    uint8_t  cf5;       /* +0x46 0x89CF5 (the Sys request mode) */
    uint8_t  e3ed;      /* +0x47 0xFFF8E3ED the shift demand's variant (stand-in 0) */
    int16_t  sys_p;     /* +0x48 the Sys request pressure */
    uint16_t sys_i;     /* +0x4A the would-be Sys current, 0.1 mA */
    uint16_t cur_up;    /* +0x4C the cached up-shift demand (0xE9ED2 at shift start) */
    uint16_t cur_dn;    /* +0x4E */
    uint8_t  bd16_last; /* +0x50 */
    uint8_t  cache_ok;  /* +0x51 */
    uint16_t pad52;
    lp_in_t  in;        /* +0x54 the last inputs */
    lp_w_t   w;         /* +0x80 */
} lp_t;
_Static_assert(sizeof(lp_t) == 0xB4u, "lp_t is 0xB4 bytes (.lp_shadow)");

void lp_shadow_init(void);
void lp_shadow_pass(const lp_in_t *in);       /* the 10 ms pass: slot 36's counters + slot 42's producer (modes 1..3) */
const volatile lp_t *lp_shadow_state(void);
void lp_hook_init(void);                      /* app/lp_hook.c: the init of both shadows (lp and ti) */
void lp_hook_10ms(void);                      /* main.c's task_10ms, after gp_hook_10ms */
void lp_hook_feed(const lp_in_t *in);         /* the part after the gather (also the host harness's entry) */

/* the host's pure entry points (tests): the Sys law and the curve inputs */
uint16_t lp_sys_law(uint16_t p, uint8_t mode, uint8_t temp, uint16_t line);
int16_t  lp_input_torque(const lp_in_t *in);  /* 0xDDFD8's value for these inputs */

#if !defined(__sh__)
void lp_shadow_host_r50(uint8_t v);           /* force fault reaction 50 (tests: 0 -> healthy) */
#endif

#endif /* TCU_APP_LP_SHADOW_H */
