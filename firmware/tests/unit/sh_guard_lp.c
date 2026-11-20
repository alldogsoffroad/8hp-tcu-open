/* sh_guard_lp.c - the line-pressure / torque-intervention shadows' SH compile checks (CMakeLists
 * "THE LINE-PRESSURE AND TORQUE-INTERVENTION SHADOWS"): the control compiles; each -D below must
 * FAIL with the read-only frame's message.
 *   SH_GUARD_LP_OUTPUT       an output translation unit includes lp_shadow.h
 *   SH_GUARD_LP_OUTTI        an output translation unit includes ti_shadow.h
 *   SH_GUARD_LP_OUTCAL       an output translation unit includes lp_cal.h
 *   SH_GUARD_LP_OUTTU        a real output TU (bsw/drv/drv_pa7.c) includes lp_shadow.h
 *   SH_GUARD_LP_LIVE_<hdr>   a shadow translation unit includes a live header
 *                            (sup_cells.h, drv_cells.h, drv_odiag.h, drv_temp.h, platform.h) */
#if defined(SH_GUARD_LP_OUTPUT)
#define TCU_OUTPUT_PATH 1
#include "app/lp_shadow.h"
#elif defined(SH_GUARD_LP_OUTTI)
#define TCU_OUTPUT_PATH 1
#include "app/ti_shadow.h"
#elif defined(SH_GUARD_LP_OUTCAL)
#define TCU_OUTPUT_PATH 1
#include "app/lp_cal.h"
#elif defined(SH_GUARD_LP_OUTTU)
#include "bsw/drv/drv_pa7.c"
#include "app/lp_shadow.h"
#elif defined(SH_GUARD_LP_LIVE_sup_cells)
#define TCU_LP_SHADOW_TU 1
#include "bsw/sup/sup_cells.h"
#elif defined(SH_GUARD_LP_LIVE_drv_cells)
#define TCU_LP_SHADOW_TU 1
#include "bsw/drv/drv_cells.h"
#elif defined(SH_GUARD_LP_LIVE_drv_odiag)
#define TCU_LP_SHADOW_TU 1
#include "bsw/drv/drv_odiag.h"
#elif defined(SH_GUARD_LP_LIVE_drv_temp)
#define TCU_LP_SHADOW_TU 1
#include "bsw/drv/drv_temp.h"
#elif defined(SH_GUARD_LP_LIVE_platform)
#define TCU_LP_SHADOW_TU 1
#include "platform/platform.h"
#else
/* the control: the shadows' headers compile on SH */
#include "app/lp_shadow.h"
#include "app/ti_shadow.h"
#include "app/lp_cal.h"
#endif
int sh_guard_lp_dummy;
