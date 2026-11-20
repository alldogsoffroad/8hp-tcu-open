/* shadow_sim.c — see io/vehicle/shadow_sim.h. Bench-only; empty without TCU_SHADOW_SIM. */
#include "io/vehicle/shadow_sim.h"

#if defined(TCU_SHADOW_SIM)

#include "platform/platform.h"
#include "io/vehicle/diag_ids.h"

#define SIM_TIMEOUT_TICKS 10u   /* 10 ms ticks = 100 ms */

/* the witness (also the state): 0x20 bytes in a dump-zero run, .shadow_sim 0xFFF87F10 */
typedef struct {
    uint32_t frames;        /* +0x00 good frames */
    uint32_t bad;           /* +0x04 frames refused (dlc / check byte) */
    uint32_t timeouts;      /* +0x08 valid -> invalid transitions */
    uint16_t output_rpm;    /* +0x0C last values */
    uint16_t turbine_rpm;   /* +0x0E */
    int16_t  torque_nm;     /* +0x10 */
    uint8_t  pedal_raw;     /* +0x12 */
    uint8_t  valid;         /* +0x13 */
    uint8_t  age;           /* +0x14 10 ms ticks since the last good frame */
    uint8_t  pad[3];        /* +0x15 */
    uint32_t magic;         /* +0x18 'SSIM' once initialised */
    uint32_t spare;         /* +0x1C */
} shadow_sim_wit_t;

#if defined(__sh__)
#define SS_WIT __attribute__((section(".shadow_sim")))
#else
#define SS_WIT
#endif
SS_WIT static shadow_sim_wit_t s_ss;
_Static_assert(sizeof(shadow_sim_wit_t) == 0x20u, "the shadow-sim witness is 0x20 bytes (linker ASSERT)");

void shadow_sim_init(void)
{
    s_ss.frames = 0u;
    s_ss.bad = 0u;
    s_ss.timeouts = 0u;
    s_ss.output_rpm = 0u;
    s_ss.turbine_rpm = 0u;
    s_ss.torque_nm = 0;
    s_ss.pedal_raw = 0u;
    s_ss.valid = 0u;
    s_ss.age = 0u;
    s_ss.pad[0] = s_ss.pad[1] = s_ss.pad[2] = 0u;
    s_ss.spare = 0u;
    s_ss.magic = 0x5353494Du;
    (void)plat_can_rx_register((can_bus_t)DIAG_BUS_SHADOW_SIM, (uint16_t)DIAG_ID_SHADOW_SIM);
}

void shadow_sim_rx(uint8_t bus, uint16_t id, uint8_t dlc, const uint8_t *data)
{
    if (bus != (uint8_t)DIAG_BUS_SHADOW_SIM || id != (uint16_t)DIAG_ID_SHADOW_SIM) {
        return;
    }
    uint8_t x = 0xA5u;
    if (dlc != 8u) {
        s_ss.bad++;
        return;
    }
    for (uint8_t i = 0u; i < 7u; i++) {
        x = (uint8_t)(x ^ data[i]);
    }
    if (x != data[7]) {
        s_ss.bad++;
        return;
    }
    s_ss.output_rpm = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    s_ss.turbine_rpm = (uint16_t)(((uint16_t)data[2] << 8) | data[3]);
    s_ss.pedal_raw = data[4];
    s_ss.torque_nm = (int16_t)(((uint16_t)data[5] << 8) | data[6]);
    s_ss.age = 0u;
    s_ss.valid = 1u;
    s_ss.frames++;
}

void shadow_sim_tick_10ms(void)
{
    if (s_ss.valid != 0u) {
        if (s_ss.age < 255u) {
            s_ss.age++;
        }
        if (s_ss.age >= SIM_TIMEOUT_TICKS) {
            s_ss.valid = 0u;
            s_ss.timeouts++;
        }
    }
}

void shadow_sim_get(shadow_sim_in_t *out)
{
    out->valid = s_ss.valid;
    out->pedal_raw = s_ss.pedal_raw;
    out->output_rpm = s_ss.output_rpm;
    out->turbine_rpm = s_ss.turbine_rpm;
    out->torque_nm = s_ss.torque_nm;
}

#endif /* TCU_SHADOW_SIM */
