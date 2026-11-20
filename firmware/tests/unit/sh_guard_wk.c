/* sh_guard_wk.c - the lock-up shadow's SH compile checks (CMakeLists "THE TORQUE-CONVERTER LOCK-UP (WK)
 * SHADOW"): the control compiles; each -D below must FAIL with the read-only frame's message.
 *   SH_GUARD_WK_OUTPUT       an output translation unit includes wk_shadow.h
 *   SH_GUARD_WK_OUTCAL       an output translation unit includes wk_cal.h
 *   SH_GUARD_WK_OUTTU        a real output TU (bsw/drv/drv_pa7.c) includes wk_shadow.h
 *   SH_GUARD_WK_LIVE_<hdr>   a shadow translation unit includes a live header
 *                            (sup_cells.h, drv_cells.h, drv_odiag.h, drv_temp.h, platform.h) */
#if defined(SH_GUARD_WK_OUTPUT)
#define TCU_OUTPUT_PATH 1
#include "app/wk_shadow.h"
#elif defined(SH_GUARD_WK_OUTCAL)
#define TCU_OUTPUT_PATH 1
#include "app/wk_cal.h"
#elif defined(SH_GUARD_WK_OUTTU)
#include "bsw/drv/drv_pa7.c"
#include "app/wk_shadow.h"
#elif defined(SH_GUARD_WK_LIVE_sup_cells)
#define TCU_WK_SHADOW_TU 1
#include "bsw/sup/sup_cells.h"
#elif defined(SH_GUARD_WK_LIVE_drv_cells)
#define TCU_WK_SHADOW_TU 1
#include "bsw/drv/drv_cells.h"
#elif defined(SH_GUARD_WK_LIVE_drv_odiag)
#define TCU_WK_SHADOW_TU 1
#include "bsw/drv/drv_odiag.h"
#elif defined(SH_GUARD_WK_LIVE_drv_temp)
#define TCU_WK_SHADOW_TU 1
#include "bsw/drv/drv_temp.h"
#elif defined(SH_GUARD_WK_LIVE_platform)
#define TCU_WK_SHADOW_TU 1
#include "platform/platform.h"
#else
/* the control: the shadow's headers compile on SH */
#include "app/wk_shadow.h"
#include "app/wk_cal.h"
#endif
int sh_guard_wk_dummy;
