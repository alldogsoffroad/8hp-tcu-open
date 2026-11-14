/* drv_park.h — the two park-output demands as the solenoid drivers pull
 * them, and the ONE compile-time switch that lets them through.
 *
 * Stock path [O, ~/tcu_sweep/park_outputs_decode.md §1.1]: the park module
 * 0xE88FA writes the output flags 0xFFF851A8[k]; the on/off actuator objects
 * call 0xE8888 (10000 when the flag is set, else 0); the actuator array is
 * read by the 5 ms request split 0x7B6D8 through its slot getters
 *   slot 7  0xA469A → *0xFFF89EBA   park release, bus-A ch 7 → 0xFFF94EE8[7]
 *   slot 11 0xA46B2 → *0xFFF89EC2   park hold, TOE31 → 0xFFF94FAA[1]
 * Here the getters are functions of app/park.c (the ported range/park
 * logic, app/range.c + app/park_law.c); the drivers (drv_reqlaw.c slot 7,
 * drv_groupa.c slot 11) call them and BOUND what they return to the stock
 * values: 0 or DRV_PARK_ON_REQ (10000), never anything else. There is no
 * setter, no RAM cell the drivers read, and no UDS path.
 *
 * ── THE SWITCH: DRV_GA_STAGE (CMake cache of tcu_app.elf; SH default 0) ──
 *   0  SAFE BUILD: both park outputs held at 0 (release and hold), group-A
 *      full-on tests held. The logic still runs and its witnesses show what
 *      it WOULD demand.
 *   1  the park outputs FOLLOW THE PORTED LOGIC (release ch 7 ∈ {0, 10000}
 *      inside the D-M4-7 on-time limit, hold slot 11 ∈ {0, 10000}); the
 *      group-A full-on tests held. (Before 2026-11-14 stage 1 meant "TOE31
 *      constant 10000"; r03h/r03j.)
 *   2  group-A full-on tests released, both park outputs held at 0.
 * Every other request-law slot stays at the stock P set (drv_reqlaw.h).
 */
#ifndef TCU_DRV_PARK_H
#define TCU_DRV_PARK_H

#include <stdint.h>

/* the stock's on/off actuator value (0xE8888 returns 10000 when on) */
#define DRV_PARK_ON_REQ 10000u

#if defined(__sh__) || defined(DRV_GA_COMPILE_TIME)
#  ifndef DRV_GA_STAGE
#    define DRV_GA_STAGE 0           /* THE SH DEFAULT WITHOUT CMAKE: no park current */
#  endif
#  if (DRV_GA_STAGE != 0) && (DRV_GA_STAGE != 1) && (DRV_GA_STAGE != 2)
#    error "drv_groupa: DRV_GA_STAGE must be 0, 1 or 2"
#  endif
#  define DRV_PARK_LIVE ((DRV_GA_STAGE) == 1)
#endif

/* the slot getters (app/park.c). Each returns 0 or DRV_PARK_ON_REQ. */
uint16_t park_slot7_release(void);      /* stock 0xA469A → *0x89EBA */
uint16_t park_slot11_hold(void);        /* stock 0xA46B2 → *0x89EC2 */

#endif /* TCU_DRV_PARK_H */
