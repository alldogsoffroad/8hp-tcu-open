/* Bench output tests (milestone M3 and M4b), under the safety design agreed 2026-10-15
 * and extended for M4b (docs/firmware/01-m4-range-park.md section 8):
 *   - outputs off by default and whenever no test is active;
 *   - a test is started only by UDS RoutineControl 0x31 01 FE1x in the extended session,
 *     with the engine not running and the driver chips and supervisor healthy;
 *   - FE10 (M3): one solenoid channel at a time, channels 0-6 only, at most 1000 mA,
 *     1-5 s, DUMMY LOADS ONLY;
 *   - FE11 (M4b, TCU_M4B_OUTPUTS builds only): commissioning of the two park outputs,
 *     one at a time — out 0 = park release (driver channel 7, full on), out 1 = park
 *     hold (TOE31 / PL6, duty in percent, one byte so the request fits one CAN frame,
 *     default 100). While any bench test is
 *     active the park module suspends its own outputs (app/park.c);
 *   - idle forcing covers channels 0-6 only: channels 7 and TOE31 belong to the park
 *     module (M4b builds) and are never touched by the idle path;
 *   - the test ends on timeout, 0x31 02, leaving the extended session (incl. S3 timeout
 *     without tester present), engine running, or any driver / supervisor fault;
 *   - no supervisor output-enable request is made: bit 0 of the 0x8E word sent without the
 *     stock start-up register init resets the TCU (bench 2026-10-30, spec 03 section 4.4);
 *     the request API stays dormant until that init is replayed (spec 12 section 2d).
 *
 * RoutineControl FE10 (solenoid, dummy loads):
 *   start   31 01 FE 10 ch current seconds      (current in 10 mA steps, 1..100; one CAN frame)
 *   stop    31 02 FE 10
 *   results 31 03 FE 10 -> 71 03 FE 10 state ch mA_hi mA_lo remaining_100ms reason sol_faults
 *           state 0 idle, 1 active; reason = why the last test ended (BENCH_END_*)
 *
 * RoutineControl FE11 (park outputs, commissioning; one CAN frame):
 *   start   31 01 FE 11 out seconds [duty_%]      (out 0 = ch 7, 1 = TOE31;
 *                                                   duty 1..100 %, default 100)
 *   stop    31 02 FE 11
 *   results 31 03 FE 11 -> 71 03 FE 11 state out duty_hi duty_lo remaining_100ms reason faults
 */
#ifndef TCU_BENCH_OUT_H
#define TCU_BENCH_OUT_H

#include "tcu/units.h"

#define BENCH_RID          0xFE10u
#define BENCH_PARK_RID     0xFE11u
#define BENCH_RSPI_DIAG_RID 0xFE12u     /* RSPI-A diagnostic dump (M4b debug; see bench_out.c) */
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
