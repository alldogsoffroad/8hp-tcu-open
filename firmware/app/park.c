/* READ-ONLY park position sensor (2026-11-16): this translation unit is
 * an output path — the sensor headers refuse to compile here (and the
 * ctest pksens_readonly_scan fails if this file names a sensor cell) */
#define TCU_OUTPUT_PATH 1
#include "app/park.h"
#include "app/park_cells.h"
#include "bsw/drv/drv_park.h"
#include "bsw/drv/drv_cells.h"
#include "io/vehicle/vehicle.h"

#if defined(TCU_RAM_APP)
volatile uint8_t g_park_cells[32];      /* the RAM app keeps the park cells in .bss */
#endif

static park_status_t s_st;
/* the OS mode (sup_mode), for 0x89C3C; 1 from park_init. In the
 * terminal-15 witness block (free low RAM, .kl15_wit) on the flashed
 * application, so no .data / .bss symbol moves */
#if defined(__sh__) && !defined(TCU_RAM_APP)
__attribute__((section(".kl15_wit"))) static uint8_t s_os_mode;
#else
static uint8_t s_os_mode;
#endif

/* the producers of 0x89CF2's inputs that our firmware does not have
 * (park_law.h park_law_aux; ~/tcu_sweep/keyoff_autop_port.md §3):
 *   0x89C42  the safety substitute-gear code — the bench's U11E4 →
 *            reaction 13 → code 0x20 (gear_code_decode; dump 0x89C41 / 42 =
 *            20 20); held, as the request producer's gear code is
 *   0x89CED  0xDD118(0x89C44): 1 / 2 only while fault reaction 56 / 57 is
 *            active — no group our fault model carries (109: 6 9 12 16 36
 *            53 59 61 63; 98: 36 53) sets either → 0 (dump 00)
 *   0xFFF8B7BE  0xE32FE's start-up data-set mismatch — not ported (dump 00) */
#define SI_89C42    0x20u
#define SI_89CED    0x00u
#define SI_8B7BE    0x00u
_Static_assert(SI_89C42 != 6u && SI_89C42 != 8u && SI_89CED != 1u && SI_89CED != 7u &&
               SI_8B7BE == 0u, "0x89CF2 stand-ins: the dump's (no fault-reaction park request)");

void park_set_os_mode(uint8_t mode)
{
    s_os_mode = mode;
}
static uint16_t s_dem_release, s_dem_hold;  /* 0 or DRV_PARK_ON_REQ, nothing else */

/* ── the slot getters (bsw/drv/drv_park.h): the ONLY way out ─────────── */
uint16_t park_slot7_release(void)
{
    return s_dem_release == DRV_PARK_ON_REQ ? DRV_PARK_ON_REQ : 0u;
}

uint16_t park_slot11_hold(void)
{
    return s_dem_hold == DRV_PARK_ON_REQ ? DRV_PARK_ON_REQ : 0u;
}

void park_init(void)
{
    /* the range itself (D6D6, E549) is the chain's: rng_chain_init */
    park_law_init();
    park_mon_init();
    s_os_mode = 1u;
    s_dem_release = 0u;
    s_dem_hold = 0u;
    s_st.lever = 0u;
    s_st.target = PARK_ENGAGE;
    s_st.reason = 0u;
    s_st.state = 0u;
    s_st.request = 0u;
    s_st.lock = 0u;
    s_st.release = false;
    s_st.hold = false;
    s_st.mon_faults = 0u;
}

/* The lever as the monitor sees it: the Chrysler type-1 shifter evaluation
 * of 0x2EA / 0x210 (io/vehicle, profile_fca.c), 5..8, 0 with no valid
 * source — the same request the chain gathers as D672 (rng_chain.c
 * rs_lever), evaluated here on its own. */
static uint8_t lever_request(const vehicle_in_t *v)
{
    if (v->q_lever != Q_OK) {
        return 0u;
    }
    switch (v->lever) {
    case RANGE_D: return 5u;
    case RANGE_N: return 6u;
    case RANGE_R: return 7u;
    case RANGE_P: return 8u;
    default:      return 0u;
    }
}

/* Call every 10 ms after rng_chain_tick_10ms(): the stock list 0x4D834
 * runs 0x9B306 / 0x9B322 / 0x9B34A (the chain) before 0xA2EBC (0xB912C, the
 * start of this path). */
void park_tick_10ms(void)
{
    const vehicle_in_t *v = vehicle_in();
    const uint8_t tgt = PCELL_TGT;          /* 0x9B588 → 0xA7B78: D6D6, raw */
    const uint8_t tgt_c = PCELL_TGT_C;
    park_law_in_t li;
    park_mon_in_t mi;

    /* stand-ins for the law's inputs whose stock producers are not ported
     * (range_logic_port.md §4): the driving dump's values */
    li.inhibit = 0u;                /* 0x89C30 (dump 0) */
    {
        /* 0x89CF2 (0xB986C +0xB4 → +0xB6): the fault-reaction park request */
        park_aux_in_t ai;

        ai.c3c = (uint8_t)(s_os_mode == 3u);    /* 0x89C3C: OS mode 3 (slot 26) */
        ai.c42 = SI_89C42;
        ai.ced = SI_89CED;
        ai.b7be = SI_8B7BE;
        li.aux = park_law_aux(&ai);
    }
    li.carwash = 0u;                /* 0x8E69B (dump 0) */
    li.temp40 = CELL_TMP_BYTE;      /* 0x89C13 [I]: our 0x94103 = (0x94F9C + 8) >> 4 + 40 */
    li.vspeed = 0;                  /* 0x89D00 (dump 0) */
    li.v89cb8 = 0u;                 /* (dump 0) */
    li.v89d49 = 2u;                 /* (dump 2: not valid) */
    li.b272 = 0u;                   /* (dump 0) */
    li.diag_rng6 = 0u;
    li.diag_rng8 = 0u;
    li.diag_out_en = 0u;
    li.diag_out_val = 0u;
    park_law_step(tgt, &li);

    mi.lever = lever_request(v);
    mi.lever_ok = mi.lever != 0u;
    mi.brake_ok = v->q_brake == Q_OK;
    mi.brake_on = v->brake == BRAKE_ON;
    mi.speed14b_ok = v->q_park_speed == Q_OK;
    mi.speed14b_raw = v->park_speed_raw;
    mi.engine_ok = v->q_engine_speed == Q_OK;
    mi.engine_rpm = (uint16_t)(v->engine_speed < 0 ? 0 :
                               v->engine_speed);
    mi.target = tgt;
    mi.target_c = tgt_c;
    mi.law_integrity_ok = park_law_integrity_ok();
    park_mon_step(&mi);

    if (park_mon_enable()) {
        s_dem_release = park_law_demand(2u);
        s_dem_hold = park_law_demand(3u);
    } else {
        s_dem_release = 0u;
        s_dem_hold = 0u;
    }

    s_st.lever = mi.lever;
    s_st.target = (tgt == (uint8_t)~tgt_c && tgt >= 5u && tgt <= 8u) ? (park_target_t)tgt : PARK_ENGAGE;
    s_st.reason = PCELL_RSN;        /* 0xFFF8D6D9, the chain's reason */
    s_st.state = park_law_state();
    s_st.request = park_law_request();
    s_st.lock = park_law_lock();
    s_st.release = s_dem_release != 0u;
    s_st.hold = s_dem_hold != 0u;
    s_st.mon_faults = park_mon_faults();
}

const park_status_t *park_status(void)
{
    return &s_st;
}
