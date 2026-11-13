/* drv_reqlaw.h — the stock's solenoid REQUEST LAW, ported 1:1 (drv_reqlaw.c):
 *   0x83DB0  the per-channel gates (chip enable, channel mask, the 'U' gate,
 *            the 0x2FFA clamp, the full-on tests, the rail-undervoltage law,
 *            the minimum-current law) → mode 0x94FD8, current 0x94ED4/0x94F58
 *   0x89540  the letter law → letters 0x90812, currents 0x90230
 *   0x901A0  the per-chip driver records 0xFFF91108 + 0x1C·c (mode byte,
 *            setpoints (I + 0x9090A)/6 via 0x90118, dither, config bytes)
 * whose records 0x924B0 (cyclic) / 0x92344 (config) put on bus A
 * (drv_jobs.c). ~/tcu_sweep/request_law_port.md.
 *
 * ── THE SAFETY FRAME (non-negotiable; the bench has a real mechatronic) ──
 * On SH builds every application request is 0 BY CONSTRUCTION:
 *   - the law reads its request through rq_request(), a compile-time 0 on
 *     __sh__ — the RAM cell 0xFFF94EE8 is never read on the target;
 *   - CELL_RQ_REQ is a pointer-to-const on __sh__ (a write does not compile);
 *   - drv_rq_request_set() does not exist on __sh__: calling it is a
 *     _Static_assert failure, and -DDRV_RQ_REQUEST_SOURCE=1 is an #error.
 * So the only bus-A content the target can stage is what the stock itself
 * stages at zero requests: '2' (0x15) before 'U', letter 0 (0x10) after
 * 'U', and the rail-off full-on tests ('C', 0x1F / 0x7FF) of ladder phase
 * 0x10 and the '3' diagnosis — the latter still under our interlock
 * (full-on only while the staged J2 word is 0x9000 and both request cells
 * are 0). Nonzero requests are exercised ONLY by host tests.
 */
#ifndef TCU_DRV_REQLAW_H
#define TCU_DRV_REQLAW_H

#include <stdint.h>
#include <stdbool.h>

#if defined(__sh__)
#  if defined(DRV_RQ_REQUEST_SOURCE) && (DRV_RQ_REQUEST_SOURCE != 0)
#    error "drv_reqlaw: a solenoid request source is refused on SH builds (safety frame: every request is 0 by construction)"
#  endif
#  undef  DRV_RQ_REQUEST_SOURCE
#  define DRV_RQ_REQUEST_SOURCE 0
   /* no setter on the target: any use is a compile error */
#  define drv_rq_request_set(ch, req01ma) \
     _Static_assert(0, "drv_reqlaw: no solenoid request source on SH builds (safety frame)")
#  define drv_rq_corr_inject(ch, corr) \
     _Static_assert(0, "drv_reqlaw: no solenoid request source on SH builds (safety frame)")
/* r03d: THE D1 PILOT (user decision 2026-11-13: "as long as we stay within
 * operating ranges"): exactly ONE channel gets ONE constant request on the
 * target; every other channel stays a compile-time 0 and there is still
 * no runtime source, setter or RAM read. Channel 3 = solenoid E (chip 1
 * SSLA2, ch 1; dump request 0x2134 = 850 mA); 500 (0.1 mA) = 50 mA, the
 * stock's own lowest request (A/WK/Sys in P) and the min-current law's
 * threshold (< 500 is zeroed). Bounds: only C/D/E (ch 4/1/3, no spring-off
 * pressure risk) and never above the stock's own P value 8500. */
#  ifndef DRV_RQ_PILOT_CH
#    define DRV_RQ_PILOT_CH   3u
#  endif
#  ifndef DRV_RQ_PILOT_REQ
#    define DRV_RQ_PILOT_REQ  8500u   /* r03e: E's stock P value, 850 mA (r03d: 500) */
#  endif
_Static_assert(DRV_RQ_PILOT_CH == 1u || DRV_RQ_PILOT_CH == 3u || DRV_RQ_PILOT_CH == 4u,
               "drv_reqlaw pilot: only solenoid D (1), E (3) or C (4)");
_Static_assert(DRV_RQ_PILOT_REQ <= 8500u,
               "drv_reqlaw pilot: request above the stock's P value 8500 (850 mA)");
#else
#  ifndef DRV_RQ_REQUEST_SOURCE
#    define DRV_RQ_REQUEST_SOURCE 1
#  endif
/* host only: the application's request 0xFFF94EE8[ch] (0.1 mA, stock
 * writer 0x7B6D8) and the 0x837E0 adaptive-correction stand-in value */
void drv_rq_request_set(uint8_t ch, uint16_t req01ma);
void drv_rq_corr_inject(uint8_t ch, int16_t corr);
#endif

/* 0x90010's part: letters '2', rounding offset cal 0x1918BB, the config
 * bytes; clears the law's own NOLOAD ranges (call after sup_cells_init) */
void drv_rq_init(void);

/* 0x83DB0 (with 0x89540 per channel): the 5 ms entry 0x7CC1E */
void drv_rq_law(void);

/* 0x901A0's record part (letters applied, 0x9090A step, per-chip records);
 * the staging (0x924B0 / 0x92344) is drv_jobs.c's */
void drv_rq_compose(void);

/* our interlock (not stock): full-on only while the staged J2 word is
 * 0x9000 and both supervisor request cells are 0 */
bool drv_rq_rail_quiet(void);

/* witnesses (ours, .drv_wit): full-on refused by the interlock in the law,
 * 'C' letters demoted at compose time (should stay 0: the law already
 * refused), and the 0x901A0 head check (staged setpoint x6 vs 0x90244 more
 * than 15 apart — stock fault 0x238, reported nowhere here) */
uint32_t drv_rq_ilock_n(void);
uint32_t drv_rq_cguard_n(void);
uint32_t drv_rq_f238_n(void);

#endif /* TCU_DRV_REQLAW_H */
