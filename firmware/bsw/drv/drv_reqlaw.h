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

/* READ-ONLY range shadow (app/rng_shadow.c): the shadow translation unit
 * defines TCU_RNG_SHADOW_TU and may not name a live cell */
#if defined(TCU_RNG_SHADOW_TU)
#error "the range shadow is read-only: it may not include a live cell header (drv_reqlaw.h)"
#endif

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
/* M5-0 (2026-11-16, ~/tcu_sweep/m5_0_port.md): on the flashed application
 * channels 0–6 come from the ported stock request PRODUCER (app/rq_prod.c,
 * 0xDC4C4 → 0xE53DA → 0x107CFC → 0xD9F70; via bsw/drv/drv_rqprod.h) fed
 * with the driving dump's inputs as compile-time constants (app/rq_prod.h).
 * DRV_RQ_P0..P6 above stay the r03f values: the request law's GUARD forces
 * any producer value other than 500 / 8500 to them (counted, .m5_wit), and
 * they remain the RAM application's requests (its 64 KB limit). */
#  if !defined(TCU_RAM_APP)
#    define DRV_RQ_USE_PRODUCER 1
#  endif
_Static_assert((DRV_RQ_P0 == 500u || DRV_RQ_P0 == 8500u) && (DRV_RQ_P1 == 500u || DRV_RQ_P1 == 8500u) &&
               (DRV_RQ_P2 == 500u || DRV_RQ_P2 == 8500u) && (DRV_RQ_P3 == 500u || DRV_RQ_P3 == 8500u) &&
               (DRV_RQ_P4 == 500u || DRV_RQ_P4 == 8500u) && (DRV_RQ_P5 == 500u || DRV_RQ_P5 == 8500u) &&
               (DRV_RQ_P6 == 500u || DRV_RQ_P6 == 8500u),
               "drv_reqlaw: the guard's fallback values are mode-1 currents (500 / 8500)");
#else
#  ifndef DRV_RQ_REQUEST_SOURCE
#    define DRV_RQ_REQUEST_SOURCE 1
#  endif
/* host only: the application's request 0xFFF94EE8[ch] (0.1 mA, stock
 * writer 0x7B6D8) and the 0x837E0 adaptive-correction stand-in value */
void drv_rq_request_set(uint8_t ch, uint16_t req01ma);
void drv_rq_corr_inject(uint8_t ch, int16_t corr);
#  if defined(DRV_RQ_USE_PRODUCER) && DRV_RQ_USE_PRODUCER
/* host only (test executables built with -DDRV_RQ_USE_PRODUCER=1): take
 * channels 0–6 from the producer (true, the target's configuration) or
 * from CELL_RQ_REQ (false, the r03f-style constant requests), so one
 * binary can compare the two on the wire */
void drv_rq_prod_host_select(bool on);
#  endif
#endif
#ifndef DRV_RQ_USE_PRODUCER
#  define DRV_RQ_USE_PRODUCER 0
#endif

/* the r03f value of channel ch (0..6), the guard's fallback: A 500, D/B/E/C
 * 8500, WK 500, Sys 500 (SH: asserted equal to DRV_RQ_P0..P6) */
#define DRV_RQ_R03F_TABLE { 500u, 8500u, 8500u, 8500u, 8500u, 500u, 500u }

/* M5-1: the guard's fill range = the clamp of the stock mode-0 law
 * (0x107D6A → 0xB7786: [i_min, i_max] = cal 0x1FEF6C / 0x1FEF6E) */
#ifndef DRV_RQ_FILL_MIN
#  define DRV_RQ_FILL_MIN   500u
#endif
#ifndef DRV_RQ_FILL_MAX
#  define DRV_RQ_FILL_MAX   8500u
#endif
#if defined(__sh__)
_Static_assert(DRV_RQ_FILL_MIN == 500u && DRV_RQ_FILL_MAX == 8500u,
               "drv_reqlaw: the M5-1 fill range is the stock clamp [500, 8500], nothing else");
#endif
_Static_assert(DRV_RQ_FILL_MAX <= 8500u, "drv_reqlaw: no fill value above i_max 8500");
/* true when v is a fill value the guard passes on channel ch (0..4) in FSM state st */
bool drv_rq_fill_ok(uint8_t ch, uint8_t st, uint16_t v);

/* M5-0 witness (ours, .m5_wit): producer values the guard replaced */
uint32_t drv_rq_m5_guard_n(void);

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
