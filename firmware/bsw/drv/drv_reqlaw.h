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
/* r03f: THE STOCK P SET on the target (user 2026-11-13: "Build stock P
 * set"; D1 "as long as we are staying within operating ranges"). The
 * requests are the stock's own P-state values from the driving dump
 * 0xFFF94EE8 (0.1 mA): A 500, D/B/E/C 8500, WK 500, Sys 500, park release
 * 0, ch 8/9 0 — compile-time constants, still no runtime source, setter or
 * RAM read. Bounds: every entry equals the stock's P value (the table is
 * asserted against it entry by entry). r03d/r03e were the single-channel
 * pilots (E at 500, then 8500). 2026-11-14: ch 7 (park release) is the
 * ported park logic's demand, bounded to {0, 10000} (below). */
/* per channel: ch0 A, ch1 D, ch2 B, ch3 E, ch4 C, ch5 WK, ch6 Sys,
 * ch7 park release, ch8/9 masked */
#  define DRV_RQ_P0 500u
#  define DRV_RQ_P1 8500u
#  define DRV_RQ_P2 8500u
#  define DRV_RQ_P3 8500u
#  define DRV_RQ_P4 8500u
#  define DRV_RQ_P5 500u
#  define DRV_RQ_P6 500u
/* ch 7, the park release: NOT a constant since 2026-11-14 — the ported
 * range/park logic's demand (app/park.c park_slot7_release(), stock getter
 * 0xA469A → *0x89EBA ← 0xE8888(id 2)). DRV_RQ_P7 is its compile-time BOUND:
 * the demand passes only when it equals DRV_RQ_P7 exactly, so the release
 * is 0 or the stock's out-of-P 10000 (1.0 A), and 0 in every stage but
 * DRV_GA_STAGE 1 (bsw/drv/drv_park.h) */
#  include "bsw/drv/drv_park.h"
#  ifndef DRV_RQ_P7
#    define DRV_RQ_P7 ((DRV_PARK_LIVE) ? DRV_PARK_ON_REQ : 0u)
#  endif
_Static_assert(DRV_RQ_P0 == 0x01F4u && DRV_RQ_P1 == 0x2134u && DRV_RQ_P2 == 0x2134u &&
               DRV_RQ_P3 == 0x2134u && DRV_RQ_P4 == 0x2134u && DRV_RQ_P5 == 0x01F4u &&
               DRV_RQ_P6 == 0x01F4u,
               "drv_reqlaw P set: every request must equal the stock's P value (dump 0xFFF94EE8)");
_Static_assert(DRV_RQ_P7 == 0u || DRV_RQ_P7 == 10000u,
               "drv_reqlaw: park release is 0 (P) or the stock's out-of-P 10000 (1.0 A), nothing else");
_Static_assert(DRV_RQ_P7 == ((DRV_PARK_LIVE) ? DRV_PARK_ON_REQ : 0u),
               "drv_reqlaw: the park-release bound follows DRV_GA_STAGE alone");
#  define DRV_RQ_PSET_TABLE { DRV_RQ_P0, DRV_RQ_P1, DRV_RQ_P2, DRV_RQ_P3, DRV_RQ_P4, \
                              DRV_RQ_P5, DRV_RQ_P6, DRV_RQ_P7, 0u, 0u }
#  define DRV_RQ_PSET 1
#else
#  ifndef DRV_RQ_REQUEST_SOURCE
#    define DRV_RQ_REQUEST_SOURCE 1
#  endif
/* host only: the application's request 0xFFF94EE8[ch] (0.1 mA, stock
 * writer 0x7B6D8) and the 0x837E0 adaptive-correction stand-in value */
void drv_rq_request_set(uint8_t ch, uint16_t req01ma);
void drv_rq_corr_inject(uint8_t ch, int16_t corr);
#endif

/* THE RELEASE ON-TIME LIMIT (ours, not stock; user D-M4-7 2026-11-14:
 * "30 seconds"). On a DRY bench the park-release coil heats (~11 W, no
 * oil; r03i's supply crept 2.63 -> 2.80 A in 60 s). 2026-11-14 (range
 * logic port): the window OPENS EACH TIME the release turns on (a law pass
 * at seq2 'U' with a nonzero release request) and counts the release-on
 * time — the time between consecutive law passes while it is on, including
 * the pass on which it turns off — ACCUMULATED OVER THE POWER CYCLE. Once
 * DRV_RQ_REL_WINDOW_MS of release-on time has accumulated, the release
 * request is 0 until the next power cycle (latched; .bss is cleared only
 * at boot). Before 'U' the request is 0 and nothing counts. A pure step
 * function so the host can test it.
 * state: 0 off (not on now), 1 on (counting), 2 latched off. */
#define DRV_RQ_REL_WINDOW_MS 30000u
typedef struct { uint8_t state; uint32_t t_last_ms; uint32_t acc_ms; } drv_rq_win_t;
uint16_t drv_rq_window_step(drv_rq_win_t *w, uint32_t now_ms, bool at_u, uint16_t req);

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
