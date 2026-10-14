/* Vehicle profile: stock FCA (Ram) bus, as received by TCU 1034420267.
 *
 * Message and signal layout from spec 07 (sections 2, 3, 7.1) and spec 14 section 5a.
 * Assumptions still to be confirmed on a vehicle or bench:
 *   - timeouts: stock raw timeout x 10 ms (spec 07 section 3);
 *   - protection form: variant 1 (spec 07 section 2, coding selection open);
 *   - largest accepted counter step: FCA_COUNTER_MAX_STEP (a calibration value in stock);
 *   - 0x210 (shifter bus) carries no CRC/counter (not seen in the stock receive table).
 * Substitute values are not applied here; invalid or timed-out signals are reported with
 * Q_INVALID and the consumer decides.
 */
#include "io/vehicle/vehicle.h"
#include "bsw/com/com.h"

#include <string.h>

#define FCA_E2E              COM_E2E_V1
#define FCA_COUNTER_MAX_STEP 2u

enum {
    M_108, M_112, M_120, M_140, M_14B, M_180, M_2EA, M_2F8, M_334, M_210, M_COUNT
};

static const com_rx_cfg_t s_cfg[M_COUNT] = {
    [M_108] = { 0x108u, CAN_BUS1, 8u,   50u, FCA_E2E,       FCA_COUNTER_MAX_STEP },
    [M_112] = { 0x112u, CAN_BUS1, 2u,   50u, COM_E2E_NONE,  0u },
    [M_120] = { 0x120u, CAN_BUS1, 7u,   50u, FCA_E2E,       FCA_COUNTER_MAX_STEP },
    [M_140] = { 0x140u, CAN_BUS1, 8u,   50u, FCA_E2E,       FCA_COUNTER_MAX_STEP },
    [M_14B] = { 0x14Bu, CAN_BUS1, 8u,   50u, FCA_E2E,       FCA_COUNTER_MAX_STEP },
    [M_180] = { 0x180u, CAN_BUS1, 8u,  200u, COM_E2E_NONE,  0u },
    [M_2EA] = { 0x2EAu, CAN_BUS1, 5u,   50u, FCA_E2E,       FCA_COUNTER_MAX_STEP },
    [M_2F8] = { 0x2F8u, CAN_BUS1, 8u,   50u, FCA_E2E,       FCA_COUNTER_MAX_STEP },
    [M_334] = { 0x334u, CAN_BUS1, 8u, 2000u, COM_E2E_NONE,  0u },
    [M_210] = { 0x210u, CAN_BUS2, 5u,   50u, COM_E2E_NONE,  0u },
};

static com_rx_state_t s_st[M_COUNT];
static vehicle_in_t s_in;

static uint16_t be16(const uint8_t *d, uint8_t i, uint16_t mask)
{
    return (uint16_t)(((uint16_t)d[i] << 8 | d[i + 1u]) & mask);
}

/* 13-bit torque, Nm = raw x 0.25 - 500, 0x1FFF invalid. Result in 0.1 Nm. */
static void torque13(const uint8_t *d, uint8_t i, nm10_t *v, quality_t *q)
{
    uint16_t raw = be16(d, i, 0x1FFFu);

    if (raw == 0x1FFFu) {
        *v = 0;
        *q = Q_INVALID;
    } else {
        *v = (nm10_t)((int32_t)raw * 5 / 2 - 5000);
        *q = Q_OK;
    }
}

/* Lever code (types 1 and 2, decode table 0x646D8): 1..4 = P/R/N/D, 5 = D with flag,
 * 0 and 6 = no change, 7..15 invalid. Returns false for invalid codes. */
static bool lever_decode(uint8_t code, range_req_t *r, bool *flag)
{
    static const range_req_t map[5] = { RANGE_NONE, RANGE_P, RANGE_R, RANGE_N, RANGE_D };

    if (code >= 1u && code <= 4u) {
        *r = map[code];
        *flag = false;
    } else if (code == 5u) {
        *r = RANGE_D;
        *flag = true;
    } else if (code != 0u && code != 6u) {
        return false;
    }
    return true;
}

static void decode_lever(void)
{
    static const uint8_t src_msg[2] = { M_2EA, M_210 };   /* stock mode 1: prefer CAN-C */
    uint8_t d[8];

    for (uint8_t s = 0u; s < 2u; s++) {
        range_req_t r = s_in.lever;
        bool flag = s_in.lever_flag;

        if (com_rx_read(src_msg[s], d) != COM_RX_OK) {
            continue;
        }
        if (lever_decode((uint8_t)(d[0] & 0x07u), &r, &flag)) {
            s_in.lever = r;
            s_in.lever_flag = flag;
            s_in.lever_src = (lever_src_t)s;
            s_in.q_lever = r == RANGE_NONE ? Q_INVALID : Q_OK;
            return;
        }
    }
    s_in.lever_src = LEVER_SRC_NONE;
    s_in.q_lever = Q_INVALID;
}

void vehicle_init(void)
{
    com_init(s_cfg, s_st, M_COUNT);
    memset(&s_in, 0, sizeof s_in);
    s_in.lever_src = LEVER_SRC_NONE;
    s_in.q_lever = Q_INVALID;
}

void vehicle_rx(can_bus_t bus, const can_frame_t *f)
{
    (void)com_rx_indication(bus, f);
}

void vehicle_update_10ms(void)
{
    uint8_t d[8];
    bool ok;

    com_tick(10u);

    /* 0x108: engine speed (bytes 0-1, [I] 1 rpm/bit), actual torque T1 (bytes 2-3). */
    ok = com_rx_read(M_108, d) == COM_RX_OK;
    if (ok && be16(d, 0u, 0xFFFFu) <= 0x7FFFu) {
        s_in.engine_speed = (rpm_t)be16(d, 0u, 0xFFFFu);
        s_in.q_engine_speed = Q_OK;
    } else {
        s_in.engine_speed = 0;
        s_in.q_engine_speed = Q_INVALID;
    }
    if (ok) {
        torque13(d, 2u, &s_in.torque_actual, &s_in.q_torque_actual);
    } else {
        s_in.q_torque_actual = Q_INVALID;
    }

    /* 0x2F8: torques T2 (bytes 0-1) and T3 (bytes 2-3); engine mode byte 6 bits 3..0, valid
     * when byte 0 bit 7 = 1. */
    ok = com_rx_read(M_2F8, d) == COM_RX_OK;
    if (ok) {
        torque13(d, 0u, &s_in.torque_t2, &s_in.q_torque_t2);
        torque13(d, 2u, &s_in.torque_t3, &s_in.q_torque_t3);
        s_in.engine_mode = (uint8_t)(d[6] & 0x0Fu);
        s_in.q_engine_mode = (d[0] & 0x80u) != 0u && s_in.engine_mode != 0x0Fu ? Q_OK : Q_INVALID;
    } else {
        s_in.q_torque_t2 = s_in.q_torque_t3 = s_in.q_engine_mode = Q_INVALID;
    }

    /* 0x120 byte 2: pedal 0..250 = 0..100 %. */
    ok = com_rx_read(M_120, d) == COM_RX_OK && d[2] <= 250u;
    s_in.pedal = ok ? (permil_t)(d[2] * 4u) : 0u;
    s_in.q_pedal = ok ? Q_OK : Q_INVALID;

    /* 0x180 byte 1: engine temperature, degC = raw - 40, 0xFF invalid. */
    ok = com_rx_read(M_180, d) == COM_RX_OK && d[1] != 0xFFu;
    s_in.engine_temp = ok ? (degc10_t)(((int16_t)d[1] - 40) * 10) : 0;
    s_in.q_engine_temp = ok ? Q_OK : Q_INVALID;

    /* 0x140 byte 0 bits 3..2: brake switch. */
    ok = com_rx_read(M_140, d) == COM_RX_OK;
    s_in.brake = ok ? (brake_t)((d[0] >> 2) & 0x03u) : BRAKE_NA;
    s_in.q_brake = ok && s_in.brake <= BRAKE_ON ? Q_OK : Q_INVALID;

    /* 0x14B bytes 0-1, 11 bit: park-interlock speed, 0x7FF invalid. */
    ok = com_rx_read(M_14B, d) == COM_RX_OK && be16(d, 0u, 0x07FFu) != 0x07FFu;
    s_in.park_speed_raw = ok ? be16(d, 0u, 0x07FFu) : 0u;
    s_in.q_park_speed = ok ? Q_OK : Q_INVALID;

    /* 0x334 bytes 3-4, 12 bit: engine-off time in minutes, 0xFFF invalid. */
    ok = com_rx_read(M_334, d) == COM_RX_OK && be16(d, 3u, 0x0FFFu) != 0x0FFFu;
    s_in.soak_s = ok ? (uint32_t)be16(d, 3u, 0x0FFFu) * 60u : 0u;
    s_in.q_soak = ok ? Q_OK : Q_INVALID;

    /* 0x112 byte 0 bits 2..0: park request / hold status, 0 and >= 5 invalid. */
    ok = com_rx_read(M_112, d) == COM_RX_OK;
    s_in.park_request = ok ? (uint8_t)(d[0] & 0x07u) : 0u;
    s_in.q_park_request = ok && s_in.park_request >= 1u && s_in.park_request <= 4u ? Q_OK : Q_INVALID;

    decode_lever();
}

const vehicle_in_t *vehicle_in(void)
{
    return &s_in;
}
