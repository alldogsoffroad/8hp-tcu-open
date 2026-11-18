/* sh_guard_gsbr.c — the garage bridge's SH compile checks (CMakeLists
 * "garage bridge"): the control compiles; each -D below must FAIL with the
 * frame's message.
 *   SH_GUARD_GSBR_OUTPUT  an output translation unit includes gs_bridge.h
 *   GS_LIVE_DIR=1         stage G1: never applied on SH
 *   GS_LIVE_DIR=2 / 4     not in {0, 1, 3}
 *   GSB_HOST_MUT_TABLE    the host-only table mutation on SH
 *   GSB_ENG_PASSES=50     the host-only engagement-limit override on SH
 * (gs_bridge.c itself: -DGS_LIVE_DIR=1 -DGS_FORCED=0 and -DGSB_SH_MUT_CH=5) */
#if defined(SH_GUARD_GSBR_OUTPUT)
#define TCU_OUTPUT_PATH 1
#include "app/gs_bridge.h"
#else
#include "bsw/drv/drv_gsbr.h"
#include "app/gs_bridge.h"
#endif
int sh_guard_gsbr(void) { return GS_LIVE_DIR; }
