/* READ-ONLY park position sensor (2026-11-16): this translation unit is
 * an output path — the sensor headers refuse to compile here (and the
 * ctest pksens_readonly_scan fails if this file names a sensor cell) */
#define TCU_OUTPUT_PATH 1
#include "app/park.h"
#include "app/park_cells.h"
#include "bsw/drv/drv_park.h"
#include "bsw/drv/drv_cells.h"
#include "io/vehicle/vehicle.h"
#include "io/sensors/sensors.h"

#if defined(TCU_RAM_APP)
volatile uint8_t g_park_cells[32];      /* the RAM app keeps the park cells in .bss */
#endif

/* ── stand-ins for the inputs whose stock producers are not ported ──────
 * (range_logic_port.md §4). Values = the stock driving dump (P, no lever,
 * engine off, this bench) unless stated. */
#define SI_D648     3u      /* D61A+0x2E safety-layer park request (dump 03) */
#define SI_D652     2u      /* D61A+0x38 its qualifier (dump 02) → D5F8 = 0 */
#define SI_D650     0u      /* D61A+0x36 [I] speed (dump 0): special rows inactive */
#define SI_LEVER2   0u      /* lever object +0x08 (writer 0xA7C1A default 0) */
#define SI_DIR      0u      /* lever object +0x10 (0xA52A2 not ported) */
#define SI_FAST     0u      /* lever object +0x20 (0xA5448 not ported) */
#define SI_STATUS   0u      /* lever object +0x28 (0x9B6C8 not ported) */

static park_status_t s_st;
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
    range_init();
    park_law_init();
    park_mon_init();
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

/* The lever request the arbiter sees (lever object +0x00, D-M4-8): the
 * Chrysler type-1 shifter evaluation of 0x2EA / 0x210 (io/vehicle,
 * profile_fca.c: the stock's 0x13CD80 table 0x646D8 + source selection
 * 0x13DB34 → E964), 0 when no source is valid (E95D = 2 → E964 = 0, as the
 * dump). [open] The stock's writer of the lever object from E964 was not
 * found (range_logic_port.md §1.3). */
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

/* D646, the output speed as the arbiter compares it: a raw unsigned rpm
 * value with NO validity term (stock 0xA57DA `cmp/hi`; the stock's producer
 * 0x14BB90 → 0xFFF8EE6E reads 0 on this sensorless bench, dump). Our speed
 * reports 0 when no edge arrives (io/sensors/speed.c); its quality is NOT
 * used, as in the stock. */
static uint16_t output_speed_d646(const sensors_in_t *z)
{
    const int32_t s = z->output_speed;
    const uint32_t a = (uint32_t)(s < 0 ? -s : s);

    return (uint16_t)(a > 0xFFFFu ? 0xFFFFu : a);
}

void park_tick_10ms(void)
{
    const vehicle_in_t *v = vehicle_in();
    const sensors_in_t *z = sensors_in();
    range_in_t ri;
    park_law_in_t li;
    park_mon_in_t mi;

    ri.lever = lever_request(v);
    ri.lever2 = SI_LEVER2;
    ri.dir = SI_DIR;
    ri.fast = SI_FAST;
    ri.status = SI_STATUS;
    ri.d646 = output_speed_d646(z);
    ri.d648 = SI_D648;
    ri.d650 = SI_D650;
    ri.d652 = SI_D652;
    ri.d5f8 = (uint8_t)((ri.d648 == 1u && ri.d652 == 0u) ? 1u : 0u);   /* 0xA9126 */
    range_update(&ri);

    li.inhibit = 0u;                /* 0x89C30 (dump 0) */
    li.aux = 0u;                    /* 0x89CF2 (dump 0) */
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
    park_law_step((uint8_t)PCELL_TGT, &li);

    mi.lever = ri.lever;
    mi.lever2 = ri.lever2;
    mi.dir = ri.dir;
    mi.fast = ri.fast;
    mi.status = ri.status;
    mi.out_speed = ri.d646;
    mi.speed2 = ri.d650;
    mi.park_req = ri.d5f8;
    mi.target = PCELL_TGT;
    mi.app_integrity_ok = range_integrity_ok() && park_law_integrity_ok();
    park_mon_step(&mi);

    if (park_mon_enable()) {
        s_dem_release = park_law_demand(2u);
        s_dem_hold = park_law_demand(3u);
    } else {
        s_dem_release = 0u;
        s_dem_hold = 0u;
    }

    s_st.lever = ri.lever;
    s_st.target = range_target();
    s_st.reason = range_reason();
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
