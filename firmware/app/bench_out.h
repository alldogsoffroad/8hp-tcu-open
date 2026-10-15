/* Bench solenoid test (milestone M3), under the safety design agreed 2026-10-15:
 *   - outputs off by default and whenever no test is active;
 *   - a test is started only by UDS RoutineControl 0x31 01 FE10 in the extended session,
 *     with the engine not running and the driver chips and supervisor healthy;
 *   - one channel at a time, channels 0-6 only (park release 7 and park hold excluded),
 *     at most 1000 mA, 1-5 s;
 *   - the test ends on timeout, 0x31 02 FE10, leaving the extended session (incl. S3 timeout
 *     without tester present), engine running, or any driver / supervisor fault;
 *   - the supervisor output enable is set only while a test is active.
 * DUMMY LOADS ONLY. Never on a transmission.
 *
 * RoutineControl FE10:
 *   start   31 01 FE 10 ch current seconds      (current in 10 mA steps, 1..100; one CAN frame)
 *   stop    31 02 FE 10
 *   results 31 03 FE 10 -> 71 03 FE 10 state ch mA_hi mA_lo remaining_100ms reason sol_faults
 *           state 0 idle, 1 active; reason = why the last test ended (BENCH_END_*)
 */
#ifndef TCU_BENCH_OUT_H
#define TCU_BENCH_OUT_H

#include "tcu/units.h"

#define BENCH_RID          0xFE10u
#define BENCH_MAX_CHANNEL  6u
#define BENCH_MAX_MA       1000u
#define BENCH_MAX_S        5u

enum {
    BENCH_END_NONE = 0, BENCH_END_TIMEOUT, BENCH_END_STOPPED, BENCH_END_SESSION,
    BENCH_END_ENGINE, BENCH_END_DRIVER, BENCH_END_SUPERVISOR
};

void bench_init(void);
/* Interlock checks and output requests; call every 10 ms BEFORE sol_tick_10ms(). */
void bench_tick_10ms(void);
/* UDS routine handler (uds_app_t.routine). */
int bench_routine(uint8_t type, uint16_t rid, const uint8_t *in, uint16_t n, uint8_t *out,
                  uint16_t max);
bool bench_active(void);

#endif
