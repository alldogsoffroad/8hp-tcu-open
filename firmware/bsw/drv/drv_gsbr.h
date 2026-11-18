/* drv_gsbr.h — THE GARAGE BRIDGE SEAM (ours, not stock; 2026-11-18,
 * ~/tcu_sweep/garage_live_plan.md §2.1, ~/tcu_sweep/garage_g1_port.md).
 *
 * The garage-engagement shadow (app/gs_shadow.c) computes the stock's
 * healthy-vehicle engagement on its own request objects; the bridge
 * (app/gs_bridge.c) decides, every 10 ms pass, whether those A–E currents
 * WOULD replace the live producer's (arm conditions, compile-time element
 * bounds, the .noinit windows) and records it. THIS header is the ONE way
 * from the bridge into an output path: drv_reqlaw.c's rq_prod_guarded()
 * may call drv_gsbr_take() — and only inside `#if GS_LIVE_DIR != 0`
 * (ctest gs_bridge_scan_outputs: no other output path may name the bridge,
 * and the one call must sit inside that block).
 *
 * STAGE G1 (this build): GS_LIVE_DIR = 0. The seam does not exist in the
 * output path at all (the call is preprocessed away); the bridge is a
 * witness. On SH a nonzero GS_LIVE_DIR is a compile error (G1). The host may
 * build GS_LIVE_DIR = 1 / 3 for the G2 gate dry-run only.
 *
 *   GS_LIVE_DIR 0  off (G1: computed, never applied)
 *               1  D only (G2)        3  D and R (G3)        anything else: error
 */
#ifndef TCU_BSW_DRV_GSBR_H
#define TCU_BSW_DRV_GSBR_H

#include <stdint.h>
#include <stdbool.h>

#ifndef GS_LIVE_DIR
#  define GS_LIVE_DIR 0
#endif
#if (GS_LIVE_DIR != 0) && (GS_LIVE_DIR != 1) && (GS_LIVE_DIR != 3)
#  error "gs bridge: GS_LIVE_DIR is 0 (off), 1 (D only) or 3 (D and R), nothing else"
#endif
#if (GS_LIVE_DIR != 0) && (defined(TCU_RAM_APP) || defined(TCU_LOADER))
#  error "gs bridge: the RAM application and the loader carry no garage bridge (GS_LIVE_DIR must be 0)"
#endif
#if defined(__sh__)
_Static_assert(GS_LIVE_DIR == 0,
               "gs bridge: stage G1 - the bridge is computed and recorded, never applied, on SH builds (GS_LIVE_DIR 0)");
#endif

#if GS_LIVE_DIR != 0
/* the seam (host G2/G3 dry-run builds only in G1): true and *v = the
 * bridged current (0.1 mA, already inside the bridge's guard) for channel
 * ch 0..4 while the bridge is armed for the shadow's direction; false = the
 * live producer's value stands */
bool drv_gsbr_take(uint8_t ch, uint16_t *v);
#endif

#endif /* TCU_BSW_DRV_GSBR_H */
