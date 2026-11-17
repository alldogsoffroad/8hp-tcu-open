/* rng_shadow.h — the stock's CYCLIC 10 ms range chain (task 0x9B306 /
 * 0x9B322 / 0x9B34A in the 10 ms list 0x4D834), ported as a READ-ONLY
 * SHADOW (2026-11-17; ~/tcu_sweep/m5_reverse_range_decode.md,
 * ~/tcu_sweep/range_shadow_port.md).
 *
 * It computes the stock's D6D6 (the cyclic range decision) into its OWN
 * RAM block (.rng_shadow, 0xFFF87000, linker.ld) and drives NOTHING: the
 * live path (app/range.c → app/park_law.c → the park outputs, the request
 * producer, bus A) never reads it. Its disagreement with the live D6D6
 * (0xFFF8D6D6, written by range.c) is the bench observable.
 *
 * READ-ONLY, both directions:
 *   - no output path may include this header (it #errors under
 *     TCU_OUTPUT_PATH, which every output translation unit defines);
 *   - the shadow's own translation unit defines TCU_RNG_SHADOW_TU, and the
 *     live cell headers (app/park_cells.h, bsw/drv/drv_cells.h,
 *     bsw/drv/drv_park.h, bsw/drv/drv_rqprod.h, app/rq_prod.h,
 *     app/park_law.h, app/park.h) #error under it — the shadow cannot
 *     name a live cell;
 *   - the ctest rng_shadow_readonly_scan greps both sides.
 * Inputs: the CAN lever evaluation (io/vehicle, the value app/park.c feeds
 * the live arbiter) for D672 / D673; every other input is a compile-time
 * constant equal to the stock driving dump (rng_shadow.c).
 */
#ifndef TCU_RNG_SHADOW_H
#define TCU_RNG_SHADOW_H

#if defined(TCU_OUTPUT_PATH)
#error "the range shadow is read-only: no output path (range, park law, park demands, bus A, request law, group A) may include rng_shadow.h"
#endif

#include <stdint.h>

/* 0x9B228 one-shot (the init list) + the shadow's own clear */
void rng_shadow_init(void);
/* one stock 10 ms pass: 0x9B306, 0x9B322, 0x9B34A in list order */
void rng_shadow_tick_10ms(void);

/* the shadow cells, at the stock offsets of each object (names = stock
 * addresses; nm: s_rs) */
typedef struct {
    /* 0xFFF8D5E6.. derived inputs (0xA8EFE / 0xA8F5C), index = addr - D5E6 */
    uint8_t  d5e6[0x26];
    /* 0xFFF8D61A.. the gathered input struct (0xA7C9C), index = addr - D61A */
    uint8_t  d61a[0x80];
    /* 0xFFF8E530.. E530 (0xA5884), E531 (0xC4F90), E532 object (0xC7D64,
     * +8..+0xF), E543 object (0xC7A14: E543..E545), E546 object (0xC360A:
     * E546..E548), E549 arbiter result (0xA548C, init only),
     * index = addr - E530 */
    uint8_t  e530[0x1C];
    /* 0xFFF8D6CA object (0xC64D4): +0 u16 timer, +2 .. +9 */
    uint16_t d6ca_t;
    uint8_t  d6ca[0x0A];        /* index = addr - D6CA (d6ca[0], [1] unused) */
    /* 0xFFF8D6D4 object (0xC70CE): D6D4 timer .. D6DD, index = addr - D6D4 */
    uint8_t  d6d4[0x0A];
    /* 0xFFF8D6DE object (0xC28DC): D6DE, D6DF */
    uint8_t  d6de[0x02];
    /* 0xFFF893E0 object (0xC36FC), index = addr - 893E0 (u16 at 0/4/0xC/0xE/0x10) */
    uint16_t x893e0_t0, x893e0_t4, x893e0_c, x893e0_e, x893e0_10;
    uint8_t  x893e0[0x18];
    /* 0xFFF89418 object (0xC29A0), index = addr - 89418 */
    uint8_t  x89418[0x08];
    /* 0xFFF89420 object (0xC3A7E): +0 u32, +4, +8 u32, +0xC (8942C) .. +0x10 */
    uint32_t x89420_t, x89420_lim;
    uint8_t  x89420[0x14];
    /* 0xFFF89434 filter object (0xC8556): the seven sub-objects' enable (+4)
     * and table (+0) in 0xC85D6's order +0x58, +0x40, +0x4C, +0x34, +0,
     * +0x70, +0x64; 0x894B6 (output), 0x894B8 (0xC8578) */
    uint8_t  flt_en[7];
    uint8_t  flt_tab[7];
    uint8_t  x894b6, x894b8;
    /* 0xFFF8304C published range object (0xC79B8 / 0xA525A): longs
     * +0/+4 range + ~, +8/+0xC, +0x10/+0x14 direction + ~ */
    uint32_t o304c[6];
    uint8_t  e64b;              /* 0xFFF8E64B mirror of 304C+0 (0xC79B8) */
    uint8_t  e572;              /* 0xFFF8E572 previous D5F2 (0xA8F5C) */
    uint8_t  p85182, p85183;    /* 0xFFF85182 / 83 previous D68E / D68F (0xC7110 / 0xC71BE) */
    uint8_t  p8517e;            /* 0xFFF8517E previous D6D6 (0xC360A) */
    uint8_t  d5e4;              /* 0xFFF8D5E4 the init one-shot flag */
    uint32_t f8975c;            /* 0xFFF8975C 0xA8F5C first-pass flag */
} rng_shadow_cells_t;

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
    uint8_t  rec_head;          /* next recorder slot */
    uint8_t  rec_n;             /* entries written (saturates 255) */
} rng_shadow_wit_t;

/* recorder entry: one per pass in which any watched cell changed */
typedef struct {
    uint32_t now_ms;
    uint8_t  d672, d673, d5fb, e531;
    uint8_t  e53c, x8941a, x894b6, d6ce;
    uint8_t  e545, d6de, d6df, d6d6;
    uint8_t  d6d9, x893e8, pad0, pad1;
} rng_shadow_rec_t;
#define RNG_SHADOW_REC_N 16u

const rng_shadow_cells_t *rng_shadow_cells(void);
const rng_shadow_wit_t   *rng_shadow_wit(void);
const rng_shadow_rec_t   *rng_shadow_rec(void);   /* [RNG_SHADOW_REC_N] ring */

#if defined(RNG_SHADOW_TEST_HOOKS)
/* host tests only (#error on SH): override gathered byte D61A+off */
void rng_shadow_test_override(uint8_t off, uint8_t val);
void rng_shadow_test_clear(void);
#endif

#endif /* TCU_RNG_SHADOW_H */
