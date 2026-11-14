/* Park target arbiter — the stock's 0xA548C → 0xA557C, ported 1:1 from the
 * bytes (app/range.c; ~/tcu_sweep/range_logic_port.md §2, spec 14 §4.2).
 *
 *   0xA557C  the special cases (0xA54A4 rows 1–3, forced releases rows 5/6,
 *            the D-at-speed row 8), then the mode dispatch by cal +0x2C:
 *            0 0xA56A4, 1 0xA56D8, 2 0xA56FA, 3 0xA5778, other 0xA57DA
 *            (STOCK: 4 → 0xA57DA)
 *   0xA548C  stores the result's target in 0xFFF8D6D6 with its complement
 * Result object 0xFFF8E549 (flag, target, reason) — the stock's own, read
 * from the call at 0x9B254. Codes: target 8 engage P, 6 released, 5
 * released (drive special case); reasons 0x01..0x17 as the stock.
 *
 * Called every 10 ms (the stock's park module class [I]).
 */
#ifndef TCU_RANGE_H
#define TCU_RANGE_H

#include <stdint.h>
#include <stdbool.h>

/* the stock calibration group 0xFFF84AB0 (ROM struct 0x4ED14, values at
 * 0x195FD4..) [O bytes] */
#define RANGE_CAL_L1        80u     /* +0x00 u16: output speed D646 limit, rpm */
#define RANGE_CAL_V8        320u    /* +0x08 u16: D650 limit (0xA54A4) */
#define RANGE_CAL_VC        320u    /* +0x0C u16: D650 limit (0xA557C rows 5/6/8) */
#define RANGE_CAL_OVR       0xFFu   /* +0x18: lever override value (0xFF = off; 0xA768C) */
#define RANGE_CAL_EN1C      0u      /* +0x1C: 0xA54A4 row 1 enable */
#define RANGE_CAL_EN20      1u      /* +0x20: row 8 enable */
#define RANGE_CAL_EN24      1u      /* +0x24: 0xA54A4 row 2 enable */
#define RANGE_CAL_EN28      1u      /* +0x28: 0xA54A4 row 3 enable */
#define RANGE_CAL_MODE      4u      /* +0x2C: arbitration mode → 0xA57DA */
#define RANGE_CAL_EN30      1u      /* +0x30: row 5 enable */
#define RANGE_CAL_EN34      1u      /* +0x34: row 6 enable */

/* target codes (0xFFF8D6D6) */
typedef enum { PARK_DRIVE5 = 5, PARK_RELEASE = 6, PARK_ENGAGE = 8 } park_target_t;

/* the arbiter's inputs, each named by its stock source */
typedef struct {
    /* the lever object 0xFFF8304C (inverted-copy getters 0xA51E4 ..) */
    uint8_t  lever;     /* +0x00 (0xA51E4): 5 D, 6 N, 7 R, 8 P, else none */
    uint8_t  lever2;    /* +0x08 (0xA51F4): second request value */
    uint8_t  dir;       /* +0x10 (0xA5204): 0 / 1 D-side / 2 R-side plausible */
    uint8_t  fast;      /* +0x20 (0xA5224): 1 when D5F9 = 1 and D650 > 320 */
    uint8_t  status;    /* +0x28 (0xA5240, byte + complement) */
    /* the input struct 0xFFF8D61A / derived block 0xFFF8D5E6 */
    uint16_t d646;      /* +0x2C: OUTPUT SPEED, rpm, unsigned — no validity term */
    uint8_t  d648;      /* +0x2E: safety-layer park request (mode 2 only) */
    uint16_t d650;      /* +0x36: [I] speed, compared with 320 */
    uint8_t  d652;      /* +0x38: qualifier of +0x2E (mode 2 only) */
    uint8_t  d5f8;      /* 0xFFF8D5F8 (0xA7716): 1 = valid safety park request */
} range_in_t;

/* one arbitration pass, pure: the 0xA557C result object (flag, target,
 * reason) is updated in *res exactly as the stock updates 0xFFF8E549
 * (byte 2 cleared first; byte 0 only ever SET, by rows 5/6) */
typedef struct { uint8_t flag, target, reason; } range_res_t;
void range_arbitrate(const range_in_t *in, range_res_t *res);

/* 0xA548C: arbitrate into 0xFFF8E549 and publish 0xFFF8D6D6 / D6D7 */
void range_init(void);
void range_update(const range_in_t *in);
park_target_t range_target(void);   /* PARK_ENGAGE when the stored copy is corrupt  */
uint8_t range_reason(void);         /* the stock reason code (0xFFF8E54B)           */
bool range_integrity_ok(void);

#ifdef PARK_TEST_HOOKS
void range_test_corrupt(void);      /* break the inverted copy */
#endif

#endif
