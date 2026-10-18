/* Park actuator sequencing (milestone M4, docs/firmware/01-m4-range-park.md section 4).
 *
 * Computes the requests for park release (channel 7) and park hold (channel 11 -> TOE31,
 * spec 12 section 1.1). It does not drive anything: app/park.c gates the requests.
 * Called every 10 ms.
 */
#ifndef TCU_PARK_CTL_H
#define TCU_PARK_CTL_H

#include "app/range.h"
#include "io/sensors/sensors.h"

typedef enum { PC_INIT = 0, PC_IN_P, PC_RELEASING, PC_HOLDING, PC_ENGAGING, PC_SAFE } park_state_t;

enum {
    PC_F_RELEASE_TIMEOUT = 0x01u,   /* contacts did not leave P while releasing */
    PC_F_ENGAGE_TIMEOUT  = 0x02u,   /* contacts did not reach P while engaging  */
    PC_F_MONITOR         = 0x04u,   /* forced to SAFE by the monitor            */
    PC_F_INTEGRITY       = 0x08u    /* own state copy corrupt                   */
};

typedef struct {
    uint16_t release_timeout_ms;
    uint16_t engage_timeout_ms;
    bool     keep_release;          /* park release stays on while holding [I] */
} park_ctl_cfg_t;

typedef struct {
    park_target_t target;
    bool       stationary;
    bool       engine_running;
    bool       supply_ok;
    park_pos_t contacts;
    bool       contacts_ok;         /* q_park == Q_OK */
} park_ctl_in_t;

typedef struct {
    bool release;                   /* channel 7  */
    bool hold;                      /* channel 11 */
} park_out_t;

void park_ctl_init(const park_ctl_cfg_t *cfg);
void park_ctl_step(const park_ctl_in_t *in, park_out_t *out);
void park_ctl_force_safe(void);     /* monitor fault: outputs off until power cycle */
park_state_t park_ctl_state(void);  /* PC_SAFE when the stored copy is corrupt      */
uint8_t park_ctl_faults(void);
bool park_ctl_integrity_ok(void);

#ifdef PARK_TEST_HOOKS
void park_ctl_test_corrupt(void);
#endif

#endif
