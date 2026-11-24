/* nissan_lp.h - THE NISSAN LINE-PRESSURE LIVE SEAM (ours, Nissan build only, 2026-11-23).
 *
 * WHAT IT IS. With NISSAN_LP_LIVE (CMake: defined on every target that builds app/nissan_lp.c,
 * i.e. the Nissan code-flash application and the Nissan host tests; never in the FCA build)
 * the LIVE Sys request (channel 6) is the read-only healthy line-pressure shadow's would-be
 * Sys current (app/lp_shadow.h lp_t.sys_i, the stock's 0xE54A4 -> 0x107FE0 curve law) instead
 * of the labelled stand-in 500 (the stock's value under fault reaction 50, D-G6-10).
 *
 * THE PATH (unchanged except for the value): the monitor's seam drv_gsbr_take (app/gs_bridge.c)
 * hands channel 6 to the request law's guard (bsw/drv/drv_reqlaw.c rq_prod_guarded) - while the
 * seam carries the candidate (gsb_t.src == GSM_S_CAND) the value is nissan_lp_sys_request()
 * instead of the candidate's own 0x10809C current (500); while the monitor has latched or
 * fenced the P content it is the P content's 500, as before. The guard passes channel 6 in the
 * stock's Sys range [500, 8500] (DRV_RQ_SYS_LIVE_MIN / _MAX, drv_gsbr.h), anything else -> 500.
 *
 * READ-ONLY ARCHITECTURE: this header names no shadow type (the output path's TUs may include
 * it); app/nissan_lp.c reads the shadows (lp, fr) and the vehicle inputs and returns the value
 * BY VALUE. STATELESS: no RAM (the Nissan image has 4 bytes of RAM left under the stack guard).
 *
 * THE GATES (all must hold, else the stock safe value 500; nissan_lp_sys_gate() returns the
 * failed ones):
 *   NLP_G_SHADOW  the shadow computed its law on its last pass: lp_t.w.pass_n != 0 and the last
 *                 pass's inputs had OS mode 1..3 (lp_shadow_pass stores in.os_mode before its
 *                 mode skip; app/main.c runs lp_hook_10ms every 10 ms task, before the F stage
 *                 that calls the seam) and its own Sys mode lp_t.cf5 == 0 (the curve branch)
 *   NLP_G_TORQUE  vehicle_in()->q_torque_actual == Q_OK (Nissan: 0x180 with its counter check)
 *   NLP_G_FR      the fault-reaction shadow has run its monitor (fr_t.w.mon_passes != 0)
 *   NLP_G_R50     fault reaction 50 is NOT active (fr_fm_react(50) == 0, the generic fault
 *                 manager's masks m1 | p1 bit 18) and the shadow's 0x89CF5 (the four-term OR
 *                 0xB986C) == 0 - the stock's own condition for the binary (500) Sys law
 * The value is then clamped to [NLP_SYS_MIN, NLP_SYS_MAX] (the stock's 0x107FE0 clamp /
 * cal 0x1FEF6C .. 0x1FEF6E). */
#ifndef TCU_APP_NISSAN_LP_H
#define TCU_APP_NISSAN_LP_H

#include <stdint.h>

#if !defined(NISSAN_LP_LIVE) || NISSAN_LP_LIVE != 1
#error "nissan_lp.h: the Nissan line-pressure live seam - built only with NISSAN_LP_LIVE=1 (TCU_VEHICLE=NISSAN)"
#endif
#if !defined(TCU_VEHICLE_NISSAN) || TCU_VEHICLE_NISSAN != 1
#error "nissan_lp.h: NISSAN_LP_LIVE belongs to the Nissan build (TCU_VEHICLE_NISSAN=1) only"
#endif

#define NLP_SYS_SAFE   500u     /* the stand-in it replaces: the stock's Sys under reaction 50 */
#define NLP_SYS_MIN    500u     /* cal 0x1FEF6C i_min (the 0x107FE0 clamp) */
#define NLP_SYS_MAX    8500u    /* cal 0x1FEF6E i_max */
#define NLP_R50        50u      /* fault reaction 50 (0xA031C id) */

#define NLP_G_SHADOW   0x01u
#define NLP_G_TORQUE   0x02u
#define NLP_G_FR       0x04u
#define NLP_G_R50      0x08u

/* the failed gates this instant (0 = the shadow's value goes live) */
uint8_t  nissan_lp_sys_gate(void);
/* the live Sys request candidate, 0.1 mA: the shadow's law value (clamped) when every gate
 * holds, else NLP_SYS_SAFE. Called by drv_gsbr_take (app/gs_bridge.c) for channel 6. */
uint16_t nissan_lp_sys_request(void);

#if !defined(__sh__)
/* host only (the chain test): replace the value the seam returns (0 = off, the gates decide) -
 * proves the plumbing (seam -> monitor -> guard -> law) independently of the gates */
void nissan_lp_host_force(uint16_t v);
#endif

#endif /* TCU_APP_NISSAN_LP_H */
