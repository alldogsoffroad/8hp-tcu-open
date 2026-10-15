/* Project measurement index list (spec 10 protocol; indices are project-defined and do not
 * match the stock list). Values are copied into fixed-size fields every 10 ms so each index
 * has a defined size and byte order on every build.
 *
 * Index  Size  Content
 *   2     2    engine speed, rpm
 *   3     2    torque T1 (actual), 0.1 Nm, signed
 *   4     2    torque T2, 0.1 Nm, signed
 *   5     2    torque T3, 0.1 Nm, signed
 *   6     2    pedal, 0.1 %
 *   7     2    engine temperature, 0.1 degC, signed
 *   8     1    brake state (0 off, 1 on, 2 error, 3 n/a)
 *   9     1    engine mode code
 *  10     2    park-interlock speed, raw 11 bit
 *  11     2    engine-off time, minutes
 *  12     1    park request code
 *  13     1    lever range request (5 D, 6 N, 7 R, 8 P, 0 none)
 *  14     1    lever source (0 CAN-C, 1 shifter bus, 2 none)
 *  15     2    input quality bits (vehicle_quality_bits)
 *  16     2    uptime, s
 *  17     2    missed 1 ms ticks (saturating)
 *  18     1    UDS session
 */
#include "app/meas_table.h"
#include "bsw/meas/meas.h"
#include "bsw/sched/sched.h"
#include "bsw/diag/uds.h"
#include "io/vehicle/vehicle.h"

static struct {
    uint16_t engine_speed;
    int16_t  torque_t1, torque_t2, torque_t3;
    uint16_t pedal;
    int16_t  engine_temp;
    uint8_t  brake, engine_mode;
    uint16_t park_speed, soak_min;
    uint8_t  park_request, lever, lever_src;
    uint16_t quality, uptime_s, missed;
    uint8_t  session;
} s_m;

#define IDX(field) { &s_m.field, (uint8_t)sizeof s_m.field }

static const meas_index_t s_index[] = {
    IDX(engine_speed), IDX(torque_t1), IDX(torque_t2), IDX(torque_t3), IDX(pedal),
    IDX(engine_temp), IDX(brake), IDX(engine_mode), IDX(park_speed), IDX(soak_min),
    IDX(park_request), IDX(lever), IDX(lever_src), IDX(quality), IDX(uptime_s), IDX(missed),
    IDX(session),
};

static const meas_cfg_t s_cfg = {
    s_index, (uint16_t)(sizeof s_index / sizeof s_index[0]), "8HPTCU-OPEN M1",
};

void meas_table_init(void)
{
    meas_init(&s_cfg);
}

void meas_table_update(void)
{
    const vehicle_in_t *v = vehicle_in();
    const sched_status_t *st = sched_status();

    s_m.engine_speed = (uint16_t)v->engine_speed;
    s_m.torque_t1 = v->torque_actual;
    s_m.torque_t2 = v->torque_t2;
    s_m.torque_t3 = v->torque_t3;
    s_m.pedal = v->pedal;
    s_m.engine_temp = v->engine_temp;
    s_m.brake = (uint8_t)v->brake;
    s_m.engine_mode = v->engine_mode;
    s_m.park_speed = v->park_speed_raw;
    s_m.soak_min = (uint16_t)(v->soak_s / 60u);
    s_m.park_request = v->park_request;
    s_m.lever = (uint8_t)v->lever;
    s_m.lever_src = (uint8_t)v->lever_src;
    s_m.quality = vehicle_quality_bits(v);
    s_m.uptime_s = (uint16_t)(st->now_ms / 1000u);
    s_m.missed = (uint16_t)(st->missed_ticks > 0xFFFFu ? 0xFFFFu : st->missed_ticks);
    s_m.session = uds_session();
}
