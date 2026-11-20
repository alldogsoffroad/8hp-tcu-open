/* shadow_sim.h — BENCH-ONLY simulated speed / pedal / torque input for the READ-ONLY
 * shadows (2026-11-20, ~/tcu_sweep/shadow_sim_port.md).
 *
 * On the dry bench no shaft turns, so the shift-map, gear-chain, gear-pair and fault
 * shadows never see a speed and never shift. The bench tool sends one frame (DIAG_ID_SHADOW_SIM
 * 0x7F6, bus 2, layout in io/vehicle/diag_ids.h) every 10 ms; while it is fresh the shadows'
 * HOOKS (app/sm_hook.c, app/gp_hook.c, app/fr_hook.c) use these values instead of the real
 * sensors. NOTHING on the live path may read this interface: the engine, the monitor, the
 * range chain, park, drv_*, sup_* keep reading the REAL sensors (tests/unit/
 * shadow_sim_scan.py enforces it; this header refuses an output-path TU).
 *
 * TCU_SHADOW_SIM is a CMake option (default ON): defined for the SH code-flash application
 * and the host builds, never for the RAM application or the loader (#error below). With it
 * off the API collapses to inline stubs (never valid).
 */
#ifndef TCU_SHADOW_SIM_H
#define TCU_SHADOW_SIM_H

#include <stdint.h>

#if defined(TCU_OUTPUT_PATH)
#  error "shadow_sim.h: the simulated-speed input is for the read-only shadows' hooks only; an output path must never include it"
#endif
#if defined(TCU_SHADOW_SIM) && (defined(TCU_RAM_APP) || defined(TCU_LOADER))
#  error "TCU_SHADOW_SIM is not allowed in the RAM application or the loader"
#endif

typedef struct {
    uint8_t  valid;         /* a good frame within the last 100 ms */
    uint8_t  pedal_raw;     /* the 0x120 byte-2 convention (sm_hook: raw * 4 = permil) */
    uint16_t output_rpm;
    uint16_t turbine_rpm;
    int16_t  torque_nm;     /* signed engine torque, Nm */
} shadow_sim_in_t;

#if defined(TCU_SHADOW_SIM)
void shadow_sim_init(void);                 /* clears the witness, registers 0x7F6 on bus 2 */
/* the raw frame (app/main.c's task_1ms drain); other ids / buses ignored */
void shadow_sim_rx(uint8_t bus, uint16_t id, uint8_t dlc, const uint8_t *data);
void shadow_sim_tick_10ms(void);            /* ages the frame: 100 ms -> invalid */
void shadow_sim_get(shadow_sim_in_t *out);  /* by value; valid = 0 when stale */
#else
static inline void shadow_sim_init(void) {}
static inline void shadow_sim_rx(uint8_t bus, uint16_t id, uint8_t dlc, const uint8_t *data)
{ (void)bus; (void)id; (void)dlc; (void)data; }
static inline void shadow_sim_tick_10ms(void) {}
static inline void shadow_sim_get(shadow_sim_in_t *out)
{ out->valid = 0u; out->pedal_raw = 0u; out->output_rpm = 0u; out->turbine_rpm = 0u; out->torque_nm = 0; }
#endif

#endif
