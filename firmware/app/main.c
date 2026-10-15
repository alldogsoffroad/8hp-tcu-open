/* M2 application: boots, serves the watchdog, receives and decodes the vehicle bus, reads the
 * transmission sensors (speeds, fluid temperature, supply, park contacts) and sends bench status
 * frames on CAN bus 1. Drives no outputs.
 *
 * Heartbeat (BENCH USE ONLY, identifier not cleared for any vehicle bus):
 *   ID 0x7F0, DLC 8, every 100 ms
 *   byte 0     rolling counter (starts at 0 after every reset)
 *   byte 1     flags: bit0 = at least one missed 1 ms tick since start
 *   bytes 2-3  uptime in seconds (big-endian)
 *   bytes 4-7  idle-loop count during the last 100 ms (big-endian); measured against the
 *              20 MHz peripheral clock this gives the CPU clock (spec 05 section 1.2)
 *
 * Input status (BENCH USE ONLY): ID 0x7F1, DLC 8, every 100 ms
 *   bytes 0-1  engine speed, rpm (big-endian)
 *   byte 2     lever range request (5 D, 6 N, 7 R, 8 P, 0 none)
 *   byte 3     lever source (0 CAN-C 0x2EA, 1 shifter bus 0x210, 2 none)
 *   bytes 4-5  input quality bits, 1 = valid: bit0 engine speed, 1 torque T1, 2 T2, 3 T3,
 *              4 pedal, 5 engine temp, 6 brake, 7 engine mode, 8 park speed, 9 soak,
 *              10 park request, 11 lever
 *   byte 6     pedal, 0.5 % per bit
 *   byte 7     brake state
 * The same 8 bytes can be read over UDS as identifier 0xFD00.
 *
 * Sensor status (BENCH USE ONLY): ID 0x7F2, DLC 8, every 100 ms (UDS identifier 0xFD01)
 *   bytes 0-1  speed channel 0 (TIF12), rpm      bytes 2-3  speed channel 1 (TIF13), rpm
 *   byte 4     fluid temperature, degC + 40 (0xFF = not valid)
 *   byte 5     battery, 0.1 V
 *   byte 6     bits 1..0 park (0 unknown, 1 in P, 2 not in P); bit 2 wake 1; bit 3 wake 2;
 *              bits 7..4 quality bits: speed 0, speed 1, fluid temp, battery (1 = valid)
 *   byte 7     fluid temperature fault bits
 *
 * Measurement protocol (spec 10, read-only): 0x361 -> 0x398 on either bus; project index list
 * in app/meas_table.c.
 */
#include "platform/platform.h"
#include "bsw/sched/sched.h"
#include "bsw/com/com.h"
#include "bsw/diag/uds.h"
#include "bsw/meas/meas.h"
#include "io/vehicle/vehicle.h"
#include "io/sensors/sensors.h"
#include "io/vehicle/fca_tx.h"
#include "app/meas_table.h"

#define HEARTBEAT_ID 0x7F0u
#define INPUTS_ID    0x7F1u
#define SENSORS_ID   0x7F2u

static uint8_t s_counter;

/* [I] shaft assignment of the speed channels: confirm on the bench (spec 02 section 4.1) */
static const sensors_cfg_t s_sensors_cfg = { 0u, 1u, { 60u, 60u, 60u } };

static void task_1ms(void)
{
    can_frame_t f;

    plat_wdt_service();
    sensors_tick_1ms();
    for (uint8_t bus = 0u; bus < (uint8_t)CAN_BUS_COUNT; bus++) {
        for (uint8_t n = 0u; n < 8u && plat_can_recv((can_bus_t)bus, &f); n++) {
            if (!uds_rx((can_bus_t)bus, &f) && !meas_rx((can_bus_t)bus, &f)) {
                vehicle_rx((can_bus_t)bus, &f);
            }
        }
    }
    uds_tick_1ms();
}

static void task_10ms(void)
{
    vehicle_update_10ms();
    sensors_update_10ms();
    meas_table_update();
    meas_tick_10ms();
    fca_tx_tick_10ms();     /* all stock transmit messages disabled in M1 */
}

static void inputs_status(uint8_t d[8])
{
    const vehicle_in_t *v = vehicle_in();
    const uint16_t bits = vehicle_quality_bits(v);

    d[0] = (uint8_t)((uint16_t)v->engine_speed >> 8);
    d[1] = (uint8_t)v->engine_speed;
    d[2] = (uint8_t)v->lever;
    d[3] = (uint8_t)v->lever_src;
    d[4] = (uint8_t)(bits >> 8);
    d[5] = (uint8_t)bits;
    d[6] = (uint8_t)(v->pedal / 5u);
    d[7] = (uint8_t)v->brake;
}

static void sensors_status(uint8_t d[8])
{
    const sensors_in_t *v = sensors_in();
    const int16_t t = (int16_t)(v->fluid_temp / 10 + 40);

    d[0] = (uint8_t)((uint16_t)v->ch_speed[0] >> 8);
    d[1] = (uint8_t)v->ch_speed[0];
    d[2] = (uint8_t)((uint16_t)v->ch_speed[1] >> 8);
    d[3] = (uint8_t)v->ch_speed[1];
    d[4] = (uint8_t)(v->q_fluid_temp != Q_INVALID && t >= 0 && t < 255 ? t : 0xFF);
    d[5] = (uint8_t)(v->battery / 100u > 255u ? 255u : v->battery / 100u);
    d[6] = (uint8_t)((uint8_t)v->park | (v->wake1 ? 0x04u : 0u) | (v->wake2 ? 0x08u : 0u) |
                     (v->q_ch_speed[0] == Q_OK ? 0x10u : 0u) | (v->q_ch_speed[1] == Q_OK ? 0x20u : 0u) |
                     (v->q_fluid_temp == Q_OK ? 0x40u : 0u) | (v->q_battery == Q_OK ? 0x80u : 0u));
    d[7] = v->fluid_temp_faults;
}

static void send_inputs(void)
{
    can_frame_t f = { .id = INPUTS_ID, .dlc = 8u };

    inputs_status(f.data);
    (void)plat_can_send(CAN_BUS1, &f);
    f.id = SENSORS_ID;
    sensors_status(f.data);
    (void)plat_can_send(CAN_BUS1, &f);
}

/* Leaving the application (programming session, ECU reset) only while the engine is not
 * running. M1 drives no outputs, so no output state has to be prepared first. */
static bool leave_allowed(void)
{
    const vehicle_in_t *v = vehicle_in();
    return v->q_engine_speed != Q_OK || v->engine_speed < 50;
}

static int read_did(uint16_t did, uint8_t *out, uint16_t max)
{
    if (did == 0xFD00u && max >= 8u) {
        inputs_status(out);
        return 8;
    }
    if (did == 0xFD01u && max >= 8u) {
        sensors_status(out);
        return 8;
    }
    return -1;
}

static const uds_app_t s_uds_app = { leave_allowed, read_did };

static void task_100ms(void)
{
    const sched_status_t *st = sched_status();
    uint32_t idle = plat_idle_take();
    uint32_t up_s = st->now_ms / 1000u;
    can_frame_t f = { .id = HEARTBEAT_ID, .dlc = 8u };

    f.data[0] = s_counter++;
    f.data[1] = (uint8_t)(st->missed_ticks != 0u ? 0x01u : 0x00u);
    f.data[2] = (uint8_t)(up_s >> 8);
    f.data[3] = (uint8_t)up_s;
    f.data[4] = (uint8_t)(idle >> 24);
    f.data[5] = (uint8_t)(idle >> 16);
    f.data[6] = (uint8_t)(idle >> 8);
    f.data[7] = (uint8_t)idle;
    (void)plat_can_send(CAN_BUS1, &f);
    send_inputs();
}

static const task_t s_tasks[] = {
    { task_1ms,     1u, 0u },
    { task_10ms,   10u, 1u },
    { task_100ms, 100u, 5u },
};

int main(void)
{
    plat_init();
    vehicle_init();
    sensors_init(&s_sensors_cfg);
    fca_tx_init();
    uds_init(&s_uds_app);
    meas_table_init();
    (void)com_register_hw();
    sched_init(s_tasks, (uint8_t)(sizeof s_tasks / sizeof s_tasks[0]));

    for (;;) {
        if (plat_tick_poll()) {
            sched_tick();
        } else {
            plat_idle_count();
        }
    }
}
