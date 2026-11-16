/* Park output law — 0xB912C (part) / 0xB97F8 / 0xCFFDE (part) / 0xE89BA /
 * 0xBAD48 / 0xBAD64 / 0xE88FA / 0xE8B56 / 0xE8B7E / 0xE8BAA / 0xE89D0 /
 * 0xE89DE / 0xE8888 / 0xB6D6C, ported 1:1 from the bytes (sh-elf-objdump of
 * the pristine image; the r0c_app / r08_app decompiles checked against
 * them). ~/tcu_sweep/range_logic_port.md §3.
 *
 * NOT PORTED (stand-ins, app/park.c): the producers of 0x89C30, 0x89CF2,
 * 0x8E69B, 0x89C13, 0x89D00, 0x89CB8, 0x89D49, 0xFFF8B272 and the diag
 * actuator / range tests 0xBB060 / 0xBA734 / 0xBB110 / 0xBB120; 0xCFFDE's
 * calls other than the AE28 / AE29 computation (0xD04FA, 0xD0262 → AE2A..E,
 * and the tail of status calls); the fault reports 0x195 of 0xE89DE and
 * 0xE8888 (b79e4); the on/off actuator objects 0x107F36 / 0xD9F70 and the
 * array path 0xA3646 → 0xBC0F4 (pass-through per the park decode: the slot
 * getters of app/park.c return 0xE8888's value directly).
 */
/* READ-ONLY park position sensor (2026-11-16): this translation unit is
 * an output path — the sensor headers refuse to compile here (and the
 * ctest pksens_readonly_scan fails if this file names a sensor cell) */
#define TCU_OUTPUT_PATH 1
#include "app/park_law.h"
#include "app/park_cells.h"

/* ── calibration [O bytes] ─────────────────────────────────────────────── */
/* maps in the stock's own layout { u32 n, u8 x[n], u8 y[n] }; x = °C + 40 */
static const uint8_t MAP_ENGAGE[12] = {             /* 0x1B5044 (P[0]) */
    0x00u, 0x00u, 0x00u, 0x04u,
    0x0Au, 0x19u, 0x28u, 0x50u,                     /* −30, −15, 0, 40 °C */
    0x64u, 0x32u, 0x1Eu, 0x0Fu,                     /* 100, 50, 30, 15 ticks */
};
static const uint8_t MAP_WINDOW[14] = {             /* 0x1B5050 (P[2]) */
    0x00u, 0x00u, 0x00u, 0x05u,
    0x14u, 0x3Bu, 0x3Cu, 0x7Du, 0xA0u,              /* −20, 19, 20, 85, 120 °C */
    0x00u, 0x00u, 0x14u, 0x14u, 0x14u,              /* 0, 0, 20, 20, 20 ticks */
};
/* 0xCFFDE's direction table 0x63445[0..8] (0xFF = keep) */
static const uint8_t DIR_TBL[9] = { 1u, 1u, 1u, 1u, 1u, 1u, 0u, 2u, 0u };

/* ── 0xB6D6C ───────────────────────────────────────────────────────────── */
uint8_t park_law_map_u8(const uint8_t *tbl, uint8_t x)
{
    const uint32_t n = ((uint32_t)tbl[0] << 24) | ((uint32_t)tbl[1] << 16) |
                       ((uint32_t)tbl[2] << 8) | tbl[3];
    const uint8_t *xs = &tbl[4];
    const uint8_t *ys = &tbl[4u + n];
    uint32_t i = 0u, frac = 0u, dx = 0u;

    if (n == 0u) {
        return 0u;
    }
    if (x > xs[0]) {
        if (x >= xs[n - 1u]) {
            i = n - 2u;                     /* y[n-2] + (y[n-1] - y[n-2]) * 1 / 1 */
            frac = 1u;
            dx = 1u;
        } else {
            uint32_t j = 0u;
            while (x > xs[j]) {
                j++;
            }
            i = j - 1u;
            dx = (uint32_t)xs[j] - xs[i];
            frac = (uint32_t)x - xs[i];
        }
    }
    if (frac == 0u) {
        return ys[i];
    }
    /* (dy * frac) as an integer, divided as float and truncated (ftrc):
     * exact for these operand sizes, so integer division is the same */
    if (ys[i + 1u] >= ys[i]) {
        return (uint8_t)(ys[i] + ((uint32_t)(ys[i + 1u] - ys[i]) * frac) / dx);
    }
    return (uint8_t)(ys[i] - (uint8_t)(((uint32_t)(ys[i] - ys[i + 1u]) * frac) / dx));
}

uint8_t park_law_engage_ticks(uint8_t temp40) { return park_law_map_u8(MAP_ENGAGE, temp40); }
uint8_t park_law_window_ticks(uint8_t temp40) { return park_law_map_u8(MAP_WINDOW, temp40); }

/* ── 0xE89D0: output flag k and its complement ────────────────────────── */
static void out_set(uint8_t k, uint8_t v)
{
    PCELL_OUT_C[k] = (uint8_t)~v;
    PCELL_OUT[k] = v;
}

/* ── 0xE8B7E: 1 = the interlock does not apply ─────────────────────────── */
static uint8_t il_speed_ok(const park_law_in_t *in)
{
    /* `cmp/gt` on the sign-extended words, then `cmp/hs` on the
     * sign-extended words (an unsigned compare of the extended values) */
    if (!((int32_t)in->vspeed > (int32_t)PL_CAL_SPEED_T)) {
        return 1u;
    }
    if ((uint32_t)(int32_t)(int16_t)in->v89cb8 >= (uint32_t)(int32_t)PL_CAL_89CB8_T) {
        return 1u;
    }
    if (in->v89d49 != 0u) {
        return 1u;
    }
    return 0u;
}

/* ── 0xE8BAA ───────────────────────────────────────────────────────────── */
static uint8_t il_bypass(const park_law_in_t *in)
{
    return (PL_CAL_E8BAA == 0u && in->b272 != 0u) ? 1u : 0u;
}

/* ── 0xE8B56: 1 = a new lock is allowed ───────────────────────────────── */
static uint8_t il_allow(const park_law_in_t *in)
{
    if (PCELL_OBJ[PO_WIN] == 0u) {
        return 1u;
    }
    if (il_speed_ok(in) == 1u) {
        return 1u;
    }
    if (il_bypass(in) != 0u) {
        return 1u;
    }
    return 0u;
}

/* ── 0xE89DE: the timer model (iterates until stable, at most 11 rounds) ─ */
static void model(const park_law_in_t *in)
{
    const uint8_t hold_off = (PCELL_OUT[PK_HOLD] == 0u) ? 1u : 0u;      /* r11, read once */
    uint8_t rounds = 0u;

    for (;;) {
        const uint8_t before = PCELL_OBJ[PO_STATE];

        switch (before) {
        case 0u:
            PCELL_OBJ[PO_STATE] = 3u;
            break;
        case 3u:
            if (PCELL_OBJ[PO_TIMER] >= park_law_engage_ticks(in->temp40) && hold_off != 0u) {
                PCELL_OBJ[PO_STATE] = 4u;
            }
            if (PCELL_OBJ[PO_LOCK] == 0u) {
                PCELL_OBJ[PO_STATE] = 2u;
            }
            break;
        case 4u:
            if (PCELL_AE29 == 0u) {
                PCELL_OBJ[PO_STATE] = 2u;
            }
            break;
        case 2u:
            if (PCELL_OBJ[PO_TIMER] >= PL_CAL_RELEASE_T) {
                PCELL_OBJ[PO_STATE] = 1u;
            }
            if (PCELL_OBJ[PO_LOCK] == 1u) {
                PCELL_OBJ[PO_STATE] = 3u;
            }
            break;
        case 1u:
            if (PCELL_OBJ[PO_LOCK] == 1u && hold_off == 1u) {
                PCELL_OBJ[PO_STATE] = 3u;
            }
            break;
        default:
            PCELL_OBJ[PO_STATE] = 4u;       /* + fault report 0x195 (not ported) */
            break;
        }
        if (rounds > 10u) {
            PCELL_OBJ[PO_STATE] = 4u;       /* + fault report 0x195 (not ported) */
            return;
        }
        rounds++;
        if (before == PCELL_OBJ[PO_STATE]) {
            return;
        }
    }
}

void park_law_init(void)
{
    for (uint8_t i = 0u; i < 4u; i++) {
        PCELL_OBJ[i] = 0u;
    }
    for (uint8_t k = 0u; k < 5u; k++) {
        PCELL_OUT[k] = 0u;
        PCELL_OUT_C[k] = 0xFFu;
    }
    PCELL_AE28 = 0u;
    PCELL_AE29 = 0u;
    PCELL_RNG = 0u;
    PCELL_RNGD = 0u;
}

void park_law_step(uint8_t target, const park_law_in_t *in)
{
    /* 0xB912C (part) + 0xB97F8 */
    uint8_t rng = target;

    PCELL_RNG = target;
    if (in->diag_rng6 != 0u && rng != 8u) {
        rng = 6u;
    }
    if (in->diag_rng8 != 0u) {
        rng = 8u;
    }
    PCELL_RNGD = rng;

    /* 0xCFFDE (part): `cmp/hi #8` on the sign-extended byte */
    if (rng <= 8u) {
        if (DIR_TBL[rng] != 0xFFu) {
            PCELL_AE28 = DIR_TBL[rng];
        }
        PCELL_AE29 = (uint8_t)(((rng == 8u && in->aux == 0u) ||
                                (in->aux != 0u && in->carwash == 0u)) ? 1u : 0u);
    }
    /* else: fault report 0x7E (not ported), AE28 / AE29 unchanged */

    /* 0xE89BA: 0xBAD48 (timer + step, saturating u8) and 0xBAD64 (window
     * − step, 0 on underflow or a zero step) [I: before 0xE88FA in the tick;
     * the stock runs it from the 10 ms fan-out at 0xBAF10] */
    {
        const uint32_t t = (uint32_t)PCELL_OBJ[PO_TIMER] + PL_STEP;
        const uint32_t w = (uint32_t)PCELL_OBJ[PO_WIN];
        const uint32_t d = w - PL_STEP;
        PCELL_OBJ[PO_TIMER] = (uint8_t)(t > 255u ? 255u : t);
        PCELL_OBJ[PO_WIN] = (uint8_t)((d >= w) ? 0u : (d > 255u ? 255u : d));
    }

    /* 0xE88FA */
    if (in->inhibit != 0u) {
        PCELL_OBJ[PO_WIN] = park_law_window_ticks(in->temp40);
    }
    {
        uint8_t lock = 0u;
        if (in->inhibit == 0u &&
            (in->aux != 0u ||
             (PCELL_AE29 != 0u && (PCELL_OBJ[PO_LOCK] != 0u || il_allow(in) != 0u)))) {
            lock = 1u;
        }
        if (lock != PCELL_OBJ[PO_LOCK]) {
            PCELL_OBJ[PO_LOCK] = lock;
            PCELL_OBJ[PO_TIMER] = 0u;
        }
    }
    out_set(1u, 0u);
    out_set(PK_RELEASE, (uint8_t)(PCELL_AE29 == 0u ? 1u : 0u));
    out_set(PK_HOLD, (uint8_t)(PCELL_OBJ[PO_LOCK] == 0u ? 1u : 0u));
    for (uint8_t k = 1u; k < 5u; k++) {
        if ((in->diag_out_en & (1u << k)) != 0u) {
            out_set(k, (uint8_t)((in->diag_out_val >> k) & 1u));
        }
    }
    model(in);
}

/* ── 0xE8888 ───────────────────────────────────────────────────────────── */
uint16_t park_law_demand(uint8_t id)
{
    uint8_t on;

    if (id == 3u) {
        on = PCELL_OUT[PK_HOLD];
    } else if (id == 2u) {
        on = PCELL_OUT[PK_RELEASE];
    } else {
        /* id 1 → 0; id 4 (TOE30, *0xFFF8B100) and others → 0 here: slot 10
         * is a compile-time 0 and forced off by 0x94FEA (drv_groupa.c) */
        return 0u;
    }
    return on != 0u ? 10000u : 0u;
}

uint8_t park_law_state(void)   { return PCELL_OBJ[PO_STATE]; }
uint8_t park_law_lock(void)    { return PCELL_OBJ[PO_LOCK]; }
uint8_t park_law_request(void) { return PCELL_AE29; }

bool park_law_integrity_ok(void)
{
    for (uint8_t k = 1u; k < 5u; k++) {
        if (PCELL_OUT[k] != (uint8_t)~PCELL_OUT_C[k]) {
            return false;
        }
    }
    return true;
}
