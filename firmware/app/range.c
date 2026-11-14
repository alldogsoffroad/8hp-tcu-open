/* Park target arbiter — 0xA548C / 0xA557C / 0xA54A4 / 0xA56A4 / 0xA56D8 /
 * 0xA56FA / 0xA5778 / 0xA57DA, ported 1:1 from the bytes (sh-elf-objdump of
 * the pristine image; the r08_app decompiles checked against them; the
 * decompile of 0xA54A4 inverts the row-2/3 speed test — the bytes
 * `cmp/hs r2,r4` at 0xA551A continue only when D650 >= 320).
 * ~/tcu_sweep/range_logic_port.md §2.
 *
 * Every speed comparison is the stock's unsigned 16-bit `cmp/hi` / `cmp/hs`
 * on the raw value: THE ARBITER HAS NO SPEED-VALIDITY TERM. A sensorless
 * bench reads output speed 0 in the stock (dump D646 = 0, 0xFFF8EE6E = 0)
 * and so is "stationary"; the caller (app/park.c) passes our speed value the
 * same way, whatever its quality.
 */
#include "app/range.h"
#include "app/park_cells.h"

/* the lever-object getters return 0 on a broken complement; the inputs here
 * are plain values (the object is not ported — app/park.c fills them) */

/* ── 0xA54A4: special cases (rows 1–3), returns 1 when one applies ────── */
static uint8_t arb_special(const range_in_t *in, range_res_t *res)
{
    /* row 1: cal +0x1C, fast == 1, D650 > cal+8, lever2 == 5, dir == 1,
     * status == 1 → 5 / reason 1 */
    if (RANGE_CAL_EN1C == 1u && in->fast == 1u && in->d650 > RANGE_CAL_V8 &&
        in->lever2 == 5u && in->dir == 1u && in->status == 1u) {
        res->target = PARK_DRIVE5;
        res->reason = 1u;
        return 1u;
    }
    /* rows 2/3: cal +0x24 or +0x28, D650 >= cal+8, D5F8 == 0, lever2 == 5,
     * status == 1 */
    if (RANGE_CAL_EN24 != 1u && RANGE_CAL_EN28 != 1u) {
        return 0u;
    }
    if (!(in->d650 >= RANGE_CAL_V8) || in->d5f8 != 0u || in->lever2 != 5u || in->status != 1u) {
        return 0u;
    }
    if (RANGE_CAL_EN24 == 1u && in->dir != 2u) {
        res->target = PARK_DRIVE5;
        res->reason = 2u;
        return 1u;
    }
    if (RANGE_CAL_EN28 == 1u) {
        res->target = PARK_RELEASE;
        res->reason = 3u;
        return 1u;
    }
    return 0u;
}

static bool lever_rnd(uint8_t l)
{
    return l == 7u || l == 6u || l == 5u;
}

/* ── the mode variants ─────────────────────────────────────────────────── */
static void arb_mode0(const range_in_t *in, range_res_t *res)     /* 0xA56A4 */
{
    if (in->d646 > RANGE_CAL_L1 && in->lever != 8u) {
        res->target = PARK_RELEASE;
        res->reason = 0x0Au;
    } else {
        res->target = PARK_ENGAGE;
        res->reason = 0x09u;
    }
}

static void arb_mode1(const range_in_t *in, range_res_t *res)     /* 0xA56D8 */
{
    if (in->d5f8 == 1u) {
        res->target = PARK_ENGAGE;
        res->reason = 0x0Bu;
    } else {
        res->target = PARK_RELEASE;
        res->reason = 0x0Cu;
    }
}

static void arb_mode2(const range_in_t *in, range_res_t *res)     /* 0xA56FA */
{
    if (in->d5f8 == 1u) {
        res->target = PARK_ENGAGE;
        res->reason = 0x0Du;
    } else if (lever_rnd(in->lever)) {
        res->target = PARK_RELEASE;
        res->reason = 0x0Eu;
    } else if (in->d646 > RANGE_CAL_L1) {
        res->target = PARK_RELEASE;
        res->reason = 0x10u;
    } else if (in->d648 != 0u || in->d652 == 2u || in->lever == 8u) {
        res->target = PARK_ENGAGE;
        res->reason = 0x0Fu;
    } else {
        res->target = PARK_RELEASE;
        res->reason = 0x10u;
    }
}

static void arb_mode3(const range_in_t *in, range_res_t *res)     /* 0xA5778 */
{
    if (in->d646 > RANGE_CAL_L1) {
        res->target = PARK_RELEASE;
        res->reason = 0x14u;
    } else if (in->d5f8 == 1u) {
        res->target = PARK_ENGAGE;
        res->reason = 0x11u;
    } else if (lever_rnd(in->lever)) {
        res->target = PARK_RELEASE;
        res->reason = 0x12u;
    } else {
        res->target = PARK_ENGAGE;
        res->reason = 0x13u;
    }
}

/* 0xA57DA — THE STOCK MODE (cal +0x2C = 4):
 *   lever P                                   → 8 / 0x15
 *   stationary (D646 <= 80) and D5F8 == 1     → 8 / 0x15
 *   lever R/N/D, or moving (D646 > 80)        → 6 / 0x16
 *   otherwise (stationary, no valid lever)    → 8 / 0x17  AUTO-P */
static void arb_mode4(const range_in_t *in, range_res_t *res)
{
    if (in->lever == 8u || (!(in->d646 > RANGE_CAL_L1) && in->d5f8 == 1u)) {
        res->target = PARK_ENGAGE;
        res->reason = 0x15u;
    } else if (lever_rnd(in->lever) || in->d646 > RANGE_CAL_L1) {
        res->target = PARK_RELEASE;
        res->reason = 0x16u;
    } else {
        res->target = PARK_ENGAGE;
        res->reason = 0x17u;
    }
}

/* ── 0xA557C ───────────────────────────────────────────────────────────── */
void range_arbitrate(const range_in_t *in, range_res_t *res)
{
    res->reason = 0u;
    if (arb_special(in, res) == 1u) {
        return;
    }
    /* row 5: cal +0x30, fast == 1, D650 > cal+0xC → flag 1, 6 / 5 */
    if (RANGE_CAL_EN30 == 1u && in->fast == 1u && in->d650 > RANGE_CAL_VC) {
        res->flag = 1u;
        res->target = PARK_RELEASE;
        res->reason = 5u;
        return;
    }
    /* row 6: cal +0x34, lever == 0, dir == 0, D650 > cal+0xC → flag 1, 6 / 6 */
    if (RANGE_CAL_EN34 == 1u && in->lever == 0u && in->dir == 0u && in->d650 > RANGE_CAL_VC) {
        res->flag = 1u;
        res->target = PARK_RELEASE;
        res->reason = 6u;
        return;
    }
    /* row 8: cal +0x20, lever == 5, dir == 1, D650 > cal+0xC → 5 / 8 */
    if (RANGE_CAL_EN20 == 1u && in->lever == 5u && in->dir == 1u && in->d650 > RANGE_CAL_VC) {
        res->target = PARK_DRIVE5;
        res->reason = 8u;
        return;
    }
    switch (RANGE_CAL_MODE) {
    case 0u: arb_mode0(in, res); break;
    case 1u: arb_mode1(in, res); break;
    case 2u: arb_mode2(in, res); break;
    case 3u: arb_mode3(in, res); break;
    default: arb_mode4(in, res); break;
    }
}

/* ── 0xA548C ───────────────────────────────────────────────────────────── */
void range_init(void)
{
    PCELL_ARB[0] = 0u;
    PCELL_ARB[1] = 0u;
    PCELL_ARB[2] = 0u;
    /* RAM is cleared at boot in the stock; D6D6 = 0 until the first pass.
     * Our readers treat a non-8/6/5 value as "engage" (range_target) */
    PCELL_TGT = 0u;
    PCELL_TGT_C = 0xFFu;
}

void range_update(const range_in_t *in)
{
    range_res_t r = { PCELL_ARB[0], PCELL_ARB[1], PCELL_ARB[2] };

    range_arbitrate(in, &r);
    PCELL_ARB[0] = r.flag;
    PCELL_ARB[1] = r.target;
    PCELL_ARB[2] = r.reason;
    PCELL_TGT = r.target;
    PCELL_TGT_C = (uint8_t)~r.target;
    /* 0xC709C (the module output, after the store) is not ported */
}

bool range_integrity_ok(void)
{
    const uint8_t t = PCELL_TGT;

    return t == (uint8_t)~PCELL_TGT_C &&
           (t == (uint8_t)PARK_ENGAGE || t == (uint8_t)PARK_RELEASE || t == (uint8_t)PARK_DRIVE5);
}

park_target_t range_target(void)
{
    return range_integrity_ok() ? (park_target_t)PCELL_TGT : PARK_ENGAGE;
}

uint8_t range_reason(void)
{
    return PCELL_ARB[2];
}

#ifdef PARK_TEST_HOOKS
void range_test_corrupt(void)
{
    PCELL_TGT_C ^= 0x01u;
}
#endif
