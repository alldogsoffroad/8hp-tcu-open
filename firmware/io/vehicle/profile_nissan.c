/* Vehicle profile: Nissan Frontier / Xterra (2nd gen) engine and body CAN - A SKELETON.
 *
 * Source: the CONFIRMED signals of nissan_frontier_2015_candidate.dbc (alldogsoffroad/
 * NissanDBCFilez; the DBC lists IDs in decimal). ~/tcu_sweep/nissan_profile_skeleton.md lists
 * every vehicle_in_t input with its source grade and the capture TODOs.
 *
 *   id     DBC   signal                                         grade
 *   0x1F9  505   ENGINE_1 RPM  bytes 2-3 BE, 0.125 rpm/bit        CONFIRMED
 *   0x180  384   ENGINE_RPM_ALT RPM bytes 0-1 BE (same value)     CONFIRMED (mirror)
 *   0x280  640   SPEED bytes 4-5 BE, 0.01 mph                     CONFIRMED
 *   0x284  644   WHEEL_1 b0-1, WHEEL_2 b2-3 BE, 0.01 km/h         CONFIRMED
 *   0x285  645   WHEEL_3 b0-1, WHEEL_4 b2-3 BE, 0.01 km/h         CONFIRMED
 *   0x354  852   BRAKE_LIGHT bit 52 (byte 6 bit 4)                CONFIRMED
 *   0x35D  861   BCM BRAKE_SWITCH bit 36 (byte 4 bit 4)           CONFIRMED
 *   0x160  352   THROTTLE byte 3, 0.392 %/bit                     CONFIRMED (scale assumed)
 *   0x551 1361   ENGINE_5 COOLANT_TEMP byte 0, raw - 45 degC      HIGH CONFIDENCE
 *
 * NOT used, not available: the accelerator-pedal candidate (0x160 byte 4), the engine-torque
 * candidate (0x182 byte 4), the torque-reduction request (0x177, a TCU->ECM TRANSMIT frame) and
 * the gear selector (0x421, also transmitted by the Nissan TCM, which the swap removes).
 * 0x160 and 0x180 are ALSO FCA IDs (transfer case / engine temperature): an FCA build on a
 * Nissan bus would misdecode them - that is why the profiles are selected at build time.
 *
 * Everything the DBC cannot give is reported with its "not available" quality: value 0,
 * Q_INVALID (brake BRAKE_NA, lever RANGE_NONE with LEVER_SRC_NONE). The lever is NONE by design
 * (Nissan cabins have no CAN lever, decision D-N1 open): the stock range chain then confirms
 * U1267 and holds P. The ignition code is 0 / Q_INVALID, i.e. the voltage path (mux 14) as on
 * the bench without 0x112.
 *
 * CAPTURE TODOs (each marked CAPTURE-TODO below): no message period is known, so every timeout
 * is the placeholder NISSAN_TIMEOUT_MS; no counter or checksum is known for any message, so
 * every message is COM_E2E_NONE (validity = timeout only); the bus the frames arrive on
 * (CAN_BUS1 assumed); DLCs are the DBC's.
 */
#include "io/vehicle/vehicle.h"
#include "io/vehicle/profile_nissan.h"
#include "bsw/com/com.h"

#include <string.h>

/* TCU_VEHICLE switch (CMakeLists.txt) */
#if !defined(TCU_VEHICLE_NISSAN) || TCU_VEHICLE_NISSAN != 1
#error "profile_nissan.c: the Nissan vehicle profile - built only with TCU_VEHICLE=NISSAN (defines TCU_VEHICLE_NISSAN=1)"
#endif

/* CAPTURE-TODO: the real periods are unknown. 200 ms is a conservative placeholder (the FCA
 * profile's slowest critical message is 200 ms); replace each with ~3-5 x the captured period. */
#define NISSAN_TIMEOUT_MS 200u
/* CAPTURE-TODO: no integrity protection is documented for any consumed message. */
#define NISSAN_E2E        COM_E2E_NONE

/* CAPTURE-TODO: the engine-speed mirror may differ from the primary during transients; 100 rpm
 * (800 raw) is a placeholder plausibility window. */
#define RPM_MIRROR_WINDOW_RAW 800u

enum {
    M_1F9, M_180, M_280, M_284, M_285, M_354, M_35D, M_160, M_551, M_COUNT
};

static const com_rx_cfg_t s_cfg[M_COUNT] = {
    [M_1F9] = { 0x1F9u, CAN_BUS1, 8u, NISSAN_TIMEOUT_MS, NISSAN_E2E, 0u },   /* CAPTURE-TODO period */
    [M_180] = { 0x180u, CAN_BUS1, 8u, NISSAN_TIMEOUT_MS, NISSAN_E2E, 0u },   /* CAPTURE-TODO period */
    [M_280] = { 0x280u, CAN_BUS1, 8u, NISSAN_TIMEOUT_MS, NISSAN_E2E, 0u },   /* CAPTURE-TODO period */
    [M_284] = { 0x284u, CAN_BUS1, 8u, NISSAN_TIMEOUT_MS, NISSAN_E2E, 0u },   /* CAPTURE-TODO period */
    [M_285] = { 0x285u, CAN_BUS1, 8u, NISSAN_TIMEOUT_MS, NISSAN_E2E, 0u },   /* CAPTURE-TODO period */
    [M_354] = { 0x354u, CAN_BUS1, 8u, NISSAN_TIMEOUT_MS, NISSAN_E2E, 0u },   /* CAPTURE-TODO period */
    [M_35D] = { 0x35Du, CAN_BUS1, 8u, NISSAN_TIMEOUT_MS, NISSAN_E2E, 0u },   /* CAPTURE-TODO period */
    [M_160] = { 0x160u, CAN_BUS1, 7u, NISSAN_TIMEOUT_MS, NISSAN_E2E, 0u },   /* CAPTURE-TODO period */
    [M_551] = { 0x551u, CAN_BUS1, 8u, NISSAN_TIMEOUT_MS, NISSAN_E2E, 0u },   /* CAPTURE-TODO period */
};

static com_rx_state_t s_st[M_COUNT];
static vehicle_in_t s_in;
static nissan_in_t s_nx;

static uint16_t be16(const uint8_t *d, uint8_t i)
{
    return (uint16_t)((uint16_t)d[i] << 8 | d[i + 1u]);
}

/* the "not available" state of every input; the fields the DBC cannot supply stay like this */
static void set_unavailable(void)
{
    memset(&s_in, 0, sizeof s_in);
    s_in.q_engine_speed = s_in.q_torque_actual = s_in.q_torque_t2 = s_in.q_torque_t3 = Q_INVALID;
    s_in.q_pedal = s_in.q_engine_temp = s_in.q_brake = s_in.q_engine_mode = Q_INVALID;
    s_in.q_park_speed = s_in.q_soak = s_in.q_ign_code = s_in.q_lever = Q_INVALID;
    s_in.brake = BRAKE_NA;
    s_in.lever = RANGE_NONE;
    s_in.lever_src = LEVER_SRC_NONE;
    memset(&s_nx, 0, sizeof s_nx);
    s_nx.q_speed = s_nx.q_wheels = s_nx.q_throttle = Q_INVALID;
}

void vehicle_init(void)
{
    com_init(s_cfg, s_st, M_COUNT);
    set_unavailable();
}

void vehicle_rx(can_bus_t bus, const can_frame_t *f)
{
    (void)com_rx_indication(bus, f);
}

/* engine speed: primary 0x1F9 bytes 2-3, mirror 0x180 bytes 0-1, 0.125 rpm/bit. Both fresh and
 * more than RPM_MIRROR_WINDOW_RAW apart -> invalid; one fresh -> that one; none -> invalid. */
static void decode_engine(void)
{
    uint8_t d1[8], d2[8];
    const bool ok1 = com_rx_read(M_1F9, d1) == COM_RX_OK;
    const bool ok2 = com_rx_read(M_180, d2) == COM_RX_OK;
    uint16_t raw = 0u;
    bool ok = ok1 || ok2;

    if (ok1) {
        raw = be16(d1, 2u);
    } else if (ok2) {
        raw = be16(d2, 0u);
    }
    if (ok1 && ok2) {
        const uint16_t m = be16(d2, 0u);
        const uint16_t diff = raw > m ? (uint16_t)(raw - m) : (uint16_t)(m - raw);

        ok = diff <= RPM_MIRROR_WINDOW_RAW;
    }
    s_in.engine_speed = ok ? (rpm_t)((raw + 4u) / 8u) : 0;     /* raw x 0.125, rounded */
    s_in.q_engine_speed = ok ? Q_OK : Q_INVALID;
}

/* brake: 0x35D bit 36 (BCM switch) and 0x354 bit 52 (brake light) are two independent
 * unprotected bits. Both fresh and equal -> that state; both fresh and different -> BRAKE_ERROR
 * (a release interlock never sees ON from a disagreement); only one fresh -> NA (a single
 * unprotected bit is not trusted). CAPTURE-TODO: debounce the transient disagreement. */
static void decode_brake(void)
{
    uint8_t sw[8], lt[8];

    if (com_rx_read(M_35D, sw) == COM_RX_OK && com_rx_read(M_354, lt) == COM_RX_OK) {
        const bool a = (sw[4] & 0x10u) != 0u;
        const bool b = (lt[6] & 0x10u) != 0u;

        s_in.brake = a == b ? (a ? BRAKE_ON : BRAKE_OFF) : BRAKE_ERROR;
    } else {
        s_in.brake = BRAKE_NA;
    }
    s_in.q_brake = s_in.brake <= BRAKE_ON ? Q_OK : Q_INVALID;
}

void vehicle_update_10ms(void)
{
    uint8_t d[8], e[8];
    bool ok;

    com_tick(10u);

    decode_engine();
    decode_brake();

    /* 0x551 byte 0: coolant, degC = raw - 45 (high confidence, no invalid marker known) */
    ok = com_rx_read(M_551, d) == COM_RX_OK;
    s_in.engine_temp = ok ? (degc10_t)(((int16_t)d[0] - 45) * 10) : 0;
    s_in.q_engine_temp = ok ? Q_OK : Q_INVALID;

    /* Nissan-only signals (profile_nissan.h) */
    ok = com_rx_read(M_280, d) == COM_RX_OK;
    s_nx.speed_mph100 = ok ? be16(d, 4u) : 0u;
    s_nx.q_speed = ok ? Q_OK : Q_INVALID;

    ok = com_rx_read(M_284, d) == COM_RX_OK && com_rx_read(M_285, e) == COM_RX_OK;
    s_nx.wheel_kmh100[0] = ok ? be16(d, 0u) : 0u;
    s_nx.wheel_kmh100[1] = ok ? be16(d, 2u) : 0u;
    s_nx.wheel_kmh100[2] = ok ? be16(e, 0u) : 0u;
    s_nx.wheel_kmh100[3] = ok ? be16(e, 2u) : 0u;
    s_nx.q_wheels = ok ? Q_OK : Q_INVALID;

    ok = com_rx_read(M_160, d) == COM_RX_OK;
    s_nx.throttle = ok ? (permil_t)(((uint16_t)d[3] * 392u + 50u) / 100u) : 0u;
    s_nx.q_throttle = ok ? Q_OK : Q_INVALID;
    s_nx.throttle_track_ok = ok && (uint8_t)(d[3] + d[5]) == 0xFFu;
    /* every other vehicle_in_t input stays "not available" (set_unavailable) */
}

const vehicle_in_t *vehicle_in(void)
{
    return &s_in;
}

const nissan_in_t *nissan_in(void)
{
    return &s_nx;
}
