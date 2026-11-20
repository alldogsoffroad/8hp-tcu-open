/* TEST-ONLY vehicle input injector: see vehicle_inject.h. Host unit tests only. */
#include "io/vehicle/vehicle_inject.h"

#include <string.h>

static vehicle_in_t s_in;
static uint32_t s_rx_n;

/* message-level injection state (see vehicle_inject.h) */
enum { MS_LEV0, MS_LEV1, MS_BRAKE, MS_ENG, MS_IGN, MS_PSP, MS_N };
typedef struct {
    uint8_t  st;        /* 0 never, 1 ok, 2 timed out (com.c's states) */
    uint16_t age_ms;
    uint16_t a, b;      /* the message's decoded raw content */
} vinj_msg_t;
static vinj_msg_t s_m[MS_N];
static uint16_t s_loss_ms = 50u;
static bool s_direct;

void vehicle_inject_reset(void)
{
    memset(s_m, 0, sizeof s_m);
    s_loss_ms = 50u;
    s_direct = false;
    memset(&s_in, 0, sizeof s_in);
    s_in.q_engine_speed = Q_INVALID;
    s_in.q_torque_actual = Q_INVALID;
    s_in.q_torque_t2 = Q_INVALID;
    s_in.q_torque_t3 = Q_INVALID;
    s_in.q_pedal = Q_INVALID;
    s_in.q_engine_temp = Q_INVALID;
    s_in.brake = BRAKE_NA;
    s_in.q_brake = Q_INVALID;
    s_in.q_engine_mode = Q_INVALID;
    s_in.q_park_speed = Q_INVALID;
    s_in.q_soak = Q_INVALID;
    s_in.q_ign_code = Q_INVALID;
    s_in.lever = RANGE_NONE;
    s_in.q_lever = Q_INVALID;
    s_in.lever_src = LEVER_SRC_NONE;
}

void vehicle_inject(const vehicle_in_t *v)
{
    s_in = *v;
    s_direct = true;
}

static void put(uint8_t i, uint16_t a, uint16_t b)
{
    s_direct = false;
    s_m[i].st = 1u;
    s_m[i].age_ms = 0u;
    s_m[i].a = a;
    s_m[i].b = b;
}

void vehicle_inject_set_loss_delay_ms(uint16_t ms) { s_loss_ms = ms; }
void vehicle_inject_msg_lever(lever_src_t src, uint8_t code)
{
    put(src == LEVER_SRC_FALLBACK ? MS_LEV1 : MS_LEV0, (uint16_t)(code & 0x07u), 0u);
}
void vehicle_inject_msg_brake(brake_t raw) { put(MS_BRAKE, (uint16_t)raw & 0x03u, 0u); }
void vehicle_inject_msg_engine(uint16_t speed_raw, uint16_t t1_raw13)
{
    put(MS_ENG, speed_raw, (uint16_t)(t1_raw13 & 0x1FFFu));
}
void vehicle_inject_msg_ign(uint8_t code) { put(MS_IGN, (uint16_t)(code & 0x07u), 0u); }
void vehicle_inject_msg_park_speed(uint16_t raw11) { put(MS_PSP, (uint16_t)(raw11 & 0x07FFu), 0u); }

uint32_t vehicle_inject_rx_count(void)
{
    return s_rx_n;
}

/* ── the profile interface (io/vehicle/vehicle.h) ─────────────────────── */
void vehicle_init(void)
{
    /* profile_fca.c's power-on state, so a golden that reads the inputs before the first
     * vehicle_update_10ms() sees what it saw with the profile: everything 0 (the zero
     * quality is Q_OK), the lever source NONE with an invalid lever. */
    s_rx_n = 0u;
    vehicle_inject_reset();
    memset(&s_in, 0, sizeof s_in);
    s_in.lever_src = LEVER_SRC_NONE;
    s_in.q_lever = Q_INVALID;
}

void vehicle_rx(can_bus_t bus, const can_frame_t *f)
{
    (void)bus;
    (void)f;
    s_rx_n++;
}

/* profile_fca.c's lever decode (types 1 and 2, table 0x646D8) */
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

void vehicle_update_10ms(void)
{
    if (s_direct) {
        return;             /* vehicle_inject(): the test owns vehicle_in_t */
    }
    /* com_tick(10): age every message that was ever received, time it out past the delay */
    for (uint8_t i = 0u; i < MS_N; i++) {
        if (s_m[i].st == 0u) {
            continue;
        }
        s_m[i].age_ms = (uint16_t)(s_m[i].age_ms > 0xFFFFu - 10u ? 0xFFFFu : s_m[i].age_ms + 10u);
        if (s_m[i].age_ms > s_loss_ms) {
            s_m[i].st = 2u;
        }
    }
    /* the messages this injector does not model are never received: as the profile reports
     * them after an update (value 0, quality invalid) */
    s_in.q_torque_t2 = s_in.q_torque_t3 = s_in.q_engine_mode = Q_INVALID;
    s_in.pedal = 0u;
    s_in.q_pedal = Q_INVALID;
    s_in.engine_temp = 0;
    s_in.q_engine_temp = Q_INVALID;
    s_in.soak_s = 0u;
    s_in.q_soak = Q_INVALID;
    {   /* engine message: speed (bytes 0-1) and T1 (13 bit) */
        const bool ok = s_m[MS_ENG].st == 1u;

        if (ok && s_m[MS_ENG].a <= 0x7FFFu) {
            s_in.engine_speed = (rpm_t)s_m[MS_ENG].a;
            s_in.q_engine_speed = Q_OK;
        } else {
            s_in.engine_speed = 0;
            s_in.q_engine_speed = Q_INVALID;
        }
        if (ok) {
            if (s_m[MS_ENG].b == 0x1FFFu) {
                s_in.torque_actual = 0;
                s_in.q_torque_actual = Q_INVALID;
            } else {
                s_in.torque_actual = (nm10_t)((int32_t)s_m[MS_ENG].b * 5 / 2 - 5000);
                s_in.q_torque_actual = Q_OK;
            }
        } else {
            s_in.q_torque_actual = Q_INVALID;
        }
    }
    {   /* brake switch */
        const bool ok = s_m[MS_BRAKE].st == 1u;

        s_in.brake = ok ? (brake_t)s_m[MS_BRAKE].a : BRAKE_NA;
        s_in.q_brake = ok && s_in.brake <= BRAKE_ON ? Q_OK : Q_INVALID;
    }
    {   /* park-interlock speed */
        const bool ok = s_m[MS_PSP].st == 1u && s_m[MS_PSP].a != 0x07FFu;

        s_in.park_speed_raw = ok ? s_m[MS_PSP].a : 0u;
        s_in.q_park_speed = ok ? Q_OK : Q_INVALID;
    }
    {   /* ignition status */
        const bool ok = s_m[MS_IGN].st == 1u;

        s_in.ign_code = ok ? (uint8_t)s_m[MS_IGN].a : 0u;
        s_in.q_ign_code = ok && s_in.ign_code != 7u ? Q_OK : Q_INVALID;
    }
    /* lever: the fallback source (0x210-like) is tried after the primary */
    {
        static const uint8_t src_msg[2] = { MS_LEV0, MS_LEV1 };

        for (uint8_t s = 0u; s < 2u; s++) {
            range_req_t r = s_in.lever;
            bool flag = s_in.lever_flag;

            if (s_m[src_msg[s]].st != 1u) {
                continue;
            }
            if (lever_decode((uint8_t)s_m[src_msg[s]].a, &r, &flag)) {
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
}

const vehicle_in_t *vehicle_in(void)
{
    return &s_in;
}
