/* sh_guard_gp.c — the gear-pair shadow's SH compile checks (CMakeLists
 * "THE GEAR-PAIR SHIFT-ENGINE SHADOW"): the control compiles; each -D below
 * must FAIL with the read-only frame's message.
 *   SH_GUARD_GP_OUTPUT       an output translation unit includes gp_shadow.h
 *   SH_GUARD_GP_OUTCAL       an output translation unit includes gp_cal.h
 *   SH_GUARD_GP_OUTTU        a real output TU (bsw/drv/drv_pa7.c) includes gp_shadow.h
 *   SH_GUARD_GP_LIVE_<hdr>   the shadow translation unit includes a live header
 *                            (sup_cells.h, drv_cells.h, drv_odiag.h, drv_temp.h,
 *                            platform.h) */
#if defined(SH_GUARD_GP_OUTPUT)
#define TCU_OUTPUT_PATH 1
#include "app/gp_shadow.h"
#elif defined(SH_GUARD_GP_OUTCAL)
#define TCU_OUTPUT_PATH 1
#include "app/gp_cal.h"
#elif defined(SH_GUARD_GP_OUTTU)
#include "bsw/drv/drv_pa7.c"
#include "app/gp_shadow.h"
#elif defined(SH_GUARD_GP_LIVE_sup_cells)
#define TCU_GP_SHADOW_TU 1
#include "bsw/sup/sup_cells.h"
#elif defined(SH_GUARD_GP_LIVE_drv_cells)
#define TCU_GP_SHADOW_TU 1
#include "bsw/drv/drv_cells.h"
#elif defined(SH_GUARD_GP_LIVE_drv_odiag)
#define TCU_GP_SHADOW_TU 1
#include "bsw/drv/drv_odiag.h"
#elif defined(SH_GUARD_GP_LIVE_drv_temp)
#define TCU_GP_SHADOW_TU 1
#include "bsw/drv/drv_temp.h"
#elif defined(SH_GUARD_GP_LIVE_platform)
#define TCU_GP_SHADOW_TU 1
#include "platform/platform.h"
#else
/* the control: the shadow's headers compile on SH */
#include "app/gp_shadow.h"
#include "app/gp_cal.h"
#endif
int sh_guard_gp_dummy;
