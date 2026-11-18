/* Unit test: FCA vehicle profile decode (spec 07 section 7.1, spec 14 section 5a). */
#include "io/vehicle/vehicle.h"
#include "bsw/com/crc8.h"
#include "bsw/com/com.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static uint8_t s_ctr[0x800];

/* Send a frame; protected messages get counter and variant-1 CRC added. */
static void send(can_bus_t bus, uint16_t id, uint8_t dlc, bool prot, const uint8_t *data)
{
    can_frame_t f = { .id = id, .dlc = dlc };

    memcpy(f.data, data, dlc);
    if (prot) {
        s_ctr[id] = (uint8_t)((s_ctr[id] + 1u) & 0x0Fu);
        f.data[dlc - 2u] = (uint8_t)((f.data[dlc - 2u] & 0x0Fu) | (uint8_t)(s_ctr[id] << 4));
        f.data[dlc - 1u] = crc8_j1850(f.data, (uint8_t)(dlc - 1u));
    }
    vehicle_rx(bus, &f);
}

int main(void)
{
    const vehicle_in_t *v = vehicle_in();

    vehicle_init();
    vehicle_update_10ms();
    CHECK(v->q_engine_speed == Q_INVALID && v->q_lever == Q_INVALID && v->lever_src == LEVER_SRC_NONE);

    /* 0x108: 2000 rpm; torque raw 2400 -> 600 - 500 = 100.0 Nm */
    send(CAN_BUS1, 0x108u, 8u, true, (const uint8_t[8]){ 0x07, 0xD0, 0x09, 0x60, 0, 0, 0, 0 });
    /* 0x2F8: T2 invalid (0x1FFF), T3 raw 0 -> -500 Nm; mode 3 valid (byte 0 bit 7) */
    send(CAN_BUS1, 0x2F8u, 8u, true, (const uint8_t[8]){ 0x9F, 0xFF, 0x00, 0x00, 0, 0, 0x03, 0 });
    /* 0x120 byte 2 = 125 -> 50.0 % */
    send(CAN_BUS1, 0x120u, 7u, true, (const uint8_t[7]){ 0, 0, 125, 0, 0, 0, 0 });
    /* 0x180 byte 1 = 130 -> 90 degC */
    send(CAN_BUS1, 0x180u, 8u, false, (const uint8_t[8]){ 0, 130, 0, 0, 0, 0, 0, 0 });
    /* 0x140 byte 0 bits 3..2 = 1 -> brake on */
    send(CAN_BUS1, 0x140u, 8u, true, (const uint8_t[8]){ 0x04, 0, 0, 0, 0, 0, 0, 0 });
    /* 0x14B: 11-bit 0x123 */
    send(CAN_BUS1, 0x14Bu, 8u, true, (const uint8_t[8]){ 0xF9, 0x23, 0, 0, 0, 0, 0, 0 });
    /* 0x334: 90 min -> 5400 s */
    send(CAN_BUS1, 0x334u, 8u, false, (const uint8_t[8]){ 0, 0, 0, 0xF0, 90, 0, 0, 0 });
    /* 0x112: park request code 2 */
    send(CAN_BUS1, 0x112u, 2u, false, (const uint8_t[2]){ 0x02, 0 });
    /* lever: 0x2EA = D (4), 0x210 = R (2); primary wins */
    send(CAN_BUS1, 0x2EAu, 5u, true, (const uint8_t[5]){ 0x04, 0, 0, 0, 0 });
    send(CAN_BUS2, 0x210u, 5u, true, (const uint8_t[5]){ 0x02, 0, 0, 0, 0 });
    vehicle_update_10ms();

    CHECK(v->engine_speed == 2000 && v->q_engine_speed == Q_OK);
    CHECK(v->torque_actual == 1000 && v->q_torque_actual == Q_OK);
    CHECK(v->q_torque_t2 == Q_INVALID);
    CHECK(v->torque_t3 == -5000 && v->q_torque_t3 == Q_OK);
    CHECK(v->engine_mode == 3u && v->q_engine_mode == Q_OK);
    CHECK(v->pedal == 500u && v->q_pedal == Q_OK);
    CHECK(v->engine_temp == 900 && v->q_engine_temp == Q_OK);
    CHECK(v->brake == BRAKE_ON && v->q_brake == Q_OK);
    CHECK(v->park_speed_raw == 0x123u && v->q_park_speed == Q_OK);
    CHECK(v->soak_s == 5400u && v->q_soak == Q_OK);
    CHECK(v->ign_code == 2u && v->q_ign_code == Q_OK);
    CHECK(v->lever == RANGE_D && v->lever_src == LEVER_SRC_PRIMARY && v->q_lever == Q_OK);

    /* primary sends "no change" (0): last value kept; invalid code 7 -> fallback R */
    send(CAN_BUS1, 0x2EAu, 5u, true, (const uint8_t[5]){ 0x00, 0, 0, 0, 0 });
    vehicle_update_10ms();
    CHECK(v->lever == RANGE_D && v->lever_src == LEVER_SRC_PRIMARY);
    send(CAN_BUS1, 0x2EAu, 5u, true, (const uint8_t[5]){ 0x07, 0, 0, 0, 0 });
    send(CAN_BUS2, 0x210u, 5u, true, (const uint8_t[5]){ 0x02, 0, 0, 0, 0 });
    vehicle_update_10ms();
    CHECK(v->lever == RANGE_R && v->lever_src == LEVER_SRC_FALLBACK && v->q_lever == Q_OK);

    /* code 5: D with flag */
    send(CAN_BUS1, 0x2EAu, 5u, true, (const uint8_t[5]){ 0x05, 0, 0, 0, 0 });
    vehicle_update_10ms();
    CHECK(v->lever == RANGE_D && v->lever_flag && v->lever_src == LEVER_SRC_PRIMARY);

    /* invalid raw values */
    send(CAN_BUS1, 0x120u, 7u, true, (const uint8_t[7]){ 0, 0, 0xFF, 0, 0, 0, 0 });
    send(CAN_BUS1, 0x180u, 8u, false, (const uint8_t[8]){ 0, 0xFF, 0, 0, 0, 0, 0, 0 });
    vehicle_update_10ms();
    CHECK(v->q_pedal == Q_INVALID && v->q_engine_temp == Q_INVALID);

    /* all bus-1 messages time out after 60 ms; 0x334 (2 s timeout) still valid */
    for (int i = 0; i < 6; i++) {
        vehicle_update_10ms();
    }
    CHECK(v->q_engine_speed == Q_INVALID && v->q_torque_actual == Q_INVALID);
    CHECK(v->q_soak == Q_OK);
    CHECK(v->q_lever == Q_INVALID && v->lever_src == LEVER_SRC_NONE);

    /* 2026-11-17 (can_inputs_decode): 0x210 needs CRC + counter — an unprotected
     * frame is rejected, so 0x210 alone gives no lever */
    for (int i = 0; i < 6; i++) {
        send(CAN_BUS2, 0x210u, 5u, false, (const uint8_t[5]){ 0x02, 0, 0, 0, 0x00 });
        vehicle_update_10ms();
    }
    CHECK(v->q_lever == Q_INVALID && v->lever_src == LEVER_SRC_NONE);
    {   /* counter step 3 accepted (cal 0x1918FE), 4 rejected */
        const com_rx_state_t *st;
        uint8_t f[5] = { 0x01, 0, 0, 0, 0 };
        uint16_t e0;
        f[3] = 0x10; f[4] = crc8_j1850(f, 4); vehicle_rx(CAN_BUS2, &(can_frame_t){ .id = 0x210u, .dlc = 5u, .data = { f[0], f[1], f[2], f[3], f[4] } });
        st = com_rx_state(9u);                 /* M_210 */
        e0 = st->err_counter;
        f[3] = 0x40; f[4] = crc8_j1850(f, 4); vehicle_rx(CAN_BUS2, &(can_frame_t){ .id = 0x210u, .dlc = 5u, .data = { f[0], f[1], f[2], f[3], f[4] } });
        CHECK(st->err_counter == e0);          /* 1 -> 4: step 3 */
        f[3] = 0x80; f[4] = crc8_j1850(f, 4); vehicle_rx(CAN_BUS2, &(can_frame_t){ .id = 0x210u, .dlc = 5u, .data = { f[0], f[1], f[2], f[3], f[4] } });
        CHECK(st->err_counter == e0 + 1u);     /* 4 -> 8: step 4 */
    }
    /* 0x112 is the ignition status: 4 = on (valid), 7 = SNA (invalid) */
    send(CAN_BUS1, 0x112u, 2u, false, (const uint8_t[2]){ 0x04, 0 });
    vehicle_update_10ms();
    CHECK(v->ign_code == 4u && v->q_ign_code == Q_OK);
    send(CAN_BUS1, 0x112u, 2u, false, (const uint8_t[2]){ 0x07, 0 });
    vehicle_update_10ms();
    CHECK(v->ign_code == 7u && v->q_ign_code == Q_INVALID);

    if (fails == 0) {
        printf("test_profile_fca: ok\n");
    }
    return fails != 0;
}
