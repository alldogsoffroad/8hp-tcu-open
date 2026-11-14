/* drv_groupa.h — the stock's GROUP-A switch-output chain (Timer E TOE30 /
 * TOE31 park hold / TOE32, + the disabled TOE33 / TOE22), ported 1:1 from
 * the bytes (drv_groupa.c; ~/tcu_sweep/groupA_park_port.md):
 *   0x7B6D8  slots 10..14 → demand 0xFFF94FAA[5] (+ the 0x94FEA / 0x94FEB
 *            force-offs)                          5 ms entry 0x7AB9C
 *   0x849D4  per output: enable, fault class, supply gate ('U' / battery),
 *            mode 0 / 1 / 2 (the rail-off full-on tests)
 *                                                 5 ms entry 0x7CC32
 *   0x8ADF4  mode → duty ‰: 20 ms pull-in at 1000 (countdown 0x9480B,
 *            reload cal 0x1919C3 = 4), then type 1 = curve(0x94F9C) x 1000
 *            / V_src (0x8AFC0), clamps 50 / 950 → 1000
 *   0x91500  duty → DRLD (0x9203C: cycle x ‰ / 1000), F stage 0x8EE30
 * Cadence: the stock's lists (sup_batch.c) — 5 ms in OS mode 0, 10 ms in
 * mode 1 (0x7CC32 and 0x8EE30 sit in the mode-1 10 ms list).
 *
 * ── THE SAFETY FRAME (non-negotiable; a real mechatronic on the bench) ──
 * On SH every group-A demand is BOUNDED AT COMPILE TIME by ONE macro,
 * DRV_GA_STAGE (default 0; bsw/drv/drv_park.h), and the rail-off full-on
 * tests are a compile-time hold:
 *   stage 0  every demand 0, tests HELD     → no group-A current anywhere;
 *            the law, the pull-in machinery and the temperature producer
 *            run at zero demand; the witnesses show what TOE31 WOULD get
 *   stage 1  TOE31 (slot 11) = the PORTED PARK LOGIC's demand (app/park.c
 *            park_slot11_hold(): 0 or the stock's 10000, passed through
 *            only when it equals the bound exactly), tests held.
 *            (r03h/r03j: stage 1 was the constant 10000.)
 *   stage 2  every demand 0, tests RELEASED (TOE30/31 at 1000 ‰ inside the
 *            stock's test windows, rail-supplied; TOE32 is excluded from
 *            the tests by the law itself: its source is 0xFE)
 * There is no other runtime demand source, setter or RAM read on SH:
 *   - drv_ga_demand_set / drv_ga_tests_set do not exist on __sh__ (a call
 *     is a _Static_assert failure); -DDRV_GA_DEMAND_SOURCE=1 is an #error;
 *   - CELL_GA_DEMAND is written only by the slot split from the table;
 *   - the Timer E writer (plat_te_ga_*) is refused to every translation
 *     unit but drv_groupa.c (platform.h), and drv_groupa.c's own guard
 *     forces 0 (counted) on any output the stage does not allow, and
 *     clamps to the cycle (the stock law's maximum, 1000 ‰).
 */
#ifndef TCU_DRV_GROUPA_H
#define TCU_DRV_GROUPA_H

#include <stdint.h>
#include <stdbool.h>

/* the compile-time path runs on SH, and on the host when a test asks for it
 * (test_groupa_stage0 / _stage1 build the exact target configuration) */
#if defined(__sh__) || defined(DRV_GA_COMPILE_TIME)
#  define DRV_GA_CT 1
#else
#  define DRV_GA_CT 0
#endif

#if DRV_GA_CT
#  if defined(DRV_GA_DEMAND_SOURCE) && (DRV_GA_DEMAND_SOURCE != 0)
#    error "drv_groupa: a group-A demand source is refused on SH builds (safety frame: demands are compile-time constants)"
#  endif
#  include "bsw/drv/drv_park.h"      /* DRV_GA_STAGE (default 0), the slot-11 getter */
#  if DRV_GA_STAGE == 0
#    define DRV_GA_TOE31_ON   0
#    define DRV_GA_TESTS      0
#  elif DRV_GA_STAGE == 1
#    define DRV_GA_TOE31_ON   1      /* 2026-11-14: the park hold FOLLOWS THE PORTED LOGIC */
#    define DRV_GA_TESTS      0
#  else
#    define DRV_GA_TOE31_ON   0
#    define DRV_GA_TESTS      1
#  endif
   /* the stock's on/off actuator demand (0xE8888: 10000 when on) */
#  define DRV_GA_ON_REQ   DRV_PARK_ON_REQ
   /* per slot: 10 TOE30, 11 TOE31 park hold, 12 TOE33 (disabled), 13 TOE32,
    * 14 TOE22 (disabled). Slots 10/12/13/14 are compile-time 0. Slot 11 is
    * the BOUND of the park-hold demand: the slot getter park_slot11_hold()
    * (app/park.c, the ported 0xE88FA → 0xE8888 chain) is passed through
    * only when it equals DRV_GA_D11 exactly — 0 or the stock 10000. */
   /* (overridable only so the guard tests can show the asserts bite: any
    * value but the one DRV_GA_STAGE implies fails to compile) */
#  ifndef DRV_GA_D10
#    define DRV_GA_D10 0u
#  endif
#  ifndef DRV_GA_D11
#    define DRV_GA_D11 ((DRV_GA_TOE31_ON) ? DRV_GA_ON_REQ : 0u)
#  endif
#  ifndef DRV_GA_D12
#    define DRV_GA_D12 0u
#  endif
#  ifndef DRV_GA_D13
#    define DRV_GA_D13 0u
#  endif
#  ifndef DRV_GA_D14
#    define DRV_GA_D14 0u
#  endif
_Static_assert(DRV_GA_D10 == 0u && DRV_GA_D12 == 0u && DRV_GA_D13 == 0u && DRV_GA_D14 == 0u,
               "drv_groupa: only the park hold (slot 11) may carry a demand");
_Static_assert(DRV_GA_D11 == 0u || DRV_GA_D11 == 10000u,
               "drv_groupa: the park-hold demand is the stock's on/off value (0 or 10000)");
_Static_assert(DRV_GA_D11 == ((DRV_GA_TOE31_ON) ? DRV_GA_ON_REQ : 0u),
               "drv_groupa: the park-hold demand follows DRV_GA_STAGE alone");
   /* the bound per slot (slot 11: the getter is gated by it in ga_demand) */
#  define DRV_GA_DEMAND_TABLE { DRV_GA_D10, DRV_GA_D11, DRV_GA_D12, DRV_GA_D13, DRV_GA_D14 }
   /* the outputs the stage may put a nonzero duty on (the guard):
    * stage 1 the park hold; stage 2 the two rail-supplied test outputs */
#  define DRV_GA_ALLOWED  ((uint8_t)((DRV_GA_D11 != 0u ? 0x02u : 0u) | \
                                     ((DRV_GA_TESTS) != 0 ? 0x03u : 0u)))
#  if defined(__sh__)
#    define drv_ga_demand_set(k, v) \
       _Static_assert(0, "drv_groupa: no group-A demand source on SH builds (safety frame)")
#    define drv_ga_tests_set(on) \
       _Static_assert(0, "drv_groupa: no group-A demand source on SH builds (safety frame)")
#  endif
#else
/* host only: the application's demand 0xFFF94FAA[k] source (slot 10 + k)
 * and the full-on test release */
void drv_ga_demand_set(uint8_t k, uint16_t demand);
void drv_ga_tests_set(bool released);
#endif

/* init: the 0x9136E RAM shadows (cycles 0x90888 / 0x90892), the 0x94FEA /
 * 0x94FEB stand-ins, .drv_ga1 cleared (after sup_cells_init) */
void drv_ga_init(void);

/* 0x7B6D8's group-A part: slots 10..14 → 0xFFF94FAA (5 ms entry 0x7AB9C) */
void drv_ga_slots_tick(void);

/* 0x849D4 (+ 0x8ADF4 per enabled output): 5 ms entry 0x7CC32 */
void drv_ga_law_tick(void);

/* 0x91500: the F stage's group-A DRLD writer (5 ms entry 0x8EE30) */
void drv_ga_fstage(void);

/* the curve lookup 0x886AC → 0x8D42C (signed x / y, round half away) */
int16_t drv_ga_curve_s16(const int16_t *tbl, int16_t x);
/* 0x9203C: ‰ → timer counts */
uint16_t drv_ga_9203c(uint16_t permil, uint16_t cycle);

/* witnesses (ours, .drv_wit): what the park hold WOULD get at demand on,
 * after the pull-in (curve mV at 0x94F9C, ‰ after the clamps, DRLD) — the
 * stage-0 read-out; the guard and the test-hold / interlock counters; the
 * k >= 2 0x91500 head check (stock fault 0x238, counted here) */
uint16_t drv_ga_wit_curve(void);
uint16_t drv_ga_wit_permil(void);
uint16_t drv_ga_wit_drld(void);
uint32_t drv_ga_guard_n(void);
uint32_t drv_ga_thold_n(void);
uint32_t drv_ga_ilock_n(void);
uint32_t drv_ga_f238_n(void);

#endif /* TCU_DRV_GROUPA_H */
